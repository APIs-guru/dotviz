#include "../output_string.h"
#include "cgraph.h"
#include "const.h"
#include "geom.h"
#include "geomprocs.h"
#include "gvc.h" // IWYU pragma: keep
#include "gvcext.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvplugin_render.h" // IWYU pragma: keep
#include "render_svg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core_svg.h"
#include "safe_job.h"

extern bool Y_invert;

extern gvdevice_callbacks_t gvdevice_callbacks;

#define EPSILON .0001

#define DEFAULT_DPI 96

static void init_job_pad(GVJ_t *job) {
  GVC_t *gvc = job->gvc;

  if (gvc->graph_sets_pad) {
    job->pad = gvc->pad;
  } else {
    switch (job->output_lang) {
    case GVRENDER_PLUGIN:
      job->pad.x = job->pad.y = job->render.features->default_pad;
      break;
    default:
      job->pad.x = job->pad.y = DEFAULT_GRAPH_PAD;
      break;
    }
  }
}

static void init_job_margin(GVJ_t *job) {
  GVC_t *gvc = job->gvc;

  if (gvc->graph_sets_margin) {
    job->margin = gvc->margin;
  } else {
    /* set default margins depending on format */
    switch (job->output_lang) {
    case GVRENDER_PLUGIN:
      job->margin = job->device.features->default_margin;
      break;
    case PCL:
    case MIF:
    case METAPOST:
    case VTX:
    case QPDF:
      job->margin.x = job->margin.y = DEFAULT_PRINT_MARGIN;
      break;
    default:
      job->margin.x = job->margin.y = DEFAULT_EMBED_MARGIN;
      break;
    }
  }
}

static void init_job_dpi(GVJ_t *job, graph_t *g) {
  GVJ_t *firstjob = job->gvc->active_jobs;

  if (GD_drawing(g)->dpi != 0) {
    job->dpi.x = job->dpi.y = GD_drawing(g)->dpi;
  } else if (firstjob && firstjob->device_sets_dpi) {
    job->dpi = firstjob->device_dpi; /* some devices set dpi in initialize() */
  } else {
    /* set default margins depending on format */
    switch (job->output_lang) {
    case GVRENDER_PLUGIN:
      job->dpi = job->device.features->default_dpi;
      break;
    default:
      job->dpi.x = job->dpi.y = DEFAULT_DPI;
      break;
    }
  }
}

static void init_job_viewport(GVJ_t *job, graph_t *g) {
  GVC_t *gvc = job->gvc;
  pointf LL, UR, size, sz;
  double Z;
  int rv;
  Agnode_t *n;
  char *str, *nodename = NULL;

  UR = gvc->bb.UR;
  LL = gvc->bb.LL;
  job->bb.LL = sub_pointf(
      LL, job->pad); // job->bb is bb of graph and padding - graph units
  job->bb.UR = add_pointf(UR, job->pad);
  sz = sub_pointf(job->bb.UR,
                  job->bb.LL); // size, including padding - graph units

  /* determine final drawing size and scale to apply. */
  /* N.B. size given by user is not rotated by landscape mode */
  /* start with "natural" size of layout */

  Z = 1.0;
  if (GD_drawing(g)->size.x > 0.001 &&
      GD_drawing(g)->size.y > 0.001) { /* graph size was given by user... */
    size = GD_drawing(g)->size;
    if (sz.x <= 0.001)
      sz.x = size.x;
    if (sz.y <= 0.001)
      sz.y = size.y;
    if (size.x < sz.x ||
        size.y < sz.y             /* drawing is too big (in either axis) ... */
        || (GD_drawing(g)->filled /* or ratio=filled requested and ... */
            && size.x > sz.x &&
            size.y > sz.y)) /* drawing is too small (in both axes) ... */
      Z = fmin(size.x / sz.x, size.y / sz.y);
  }

  /* default focus, in graph units = center of bb */
  pointf xy = scale(0.5, add_pointf(LL, UR));

  /* rotate and scale bb to give default absolute size in points*/
  job->rotation = job->gvc->rotation;
  pointf XY = scale(Z, sz);

  /* user can override */
  if ((str = agget(g, "viewport"))) {
    nodename = gv_alloc(strlen(str) + 1);
    rv = sscanf(str, "%lf,%lf,%lf,\'%[^\']\'", &XY.x, &XY.y, &Z, nodename);
    if (rv == 4) {
      n = agfindnode(g->root, nodename);
      if (n) {
        xy = ND_coord(n);
      }
    } else {
      rv = sscanf(str, "%lf,%lf,%lf,%[^,]%c", &XY.x, &XY.y, &Z, nodename,
                  &(char){0});
      if (rv == 4) {
        n = agfindnode(g->root, nodename);
        if (n) {
          xy = ND_coord(n);
        }
      } else {
        sscanf(str, "%lf,%lf,%lf,%lf,%lf", &XY.x, &XY.y, &Z, &xy.x, &xy.y);
      }
    }
    free(nodename);
  }
  /* rv is ignored since args retain previous values if not scanned */

  /* job->view gives port size in graph units, unscaled or rotated
   * job->zoom gives scaling factor.
   * job->focus gives the position in the graph of the center of the port
   */
  job->view = XY;
  job->zoom = Z; /* scaling factor */
  job->focus = xy;
}

enum { FORMAT_SVG, FORMAT_SVGZ, FORMAT_SVG_INLINE };

extern gvrender_engine_t svg_engine;
extern gvplugin_available_t svg_device_available;
extern gvplugin_available_t svg_render_available;
extern gvdevice_features_t my_device_features_svg;
extern gvrender_features_t my_render_features_svg;

output_string inner_render_svg(GVC_t *gvc, GVJ_t *job, Agraph_t *g) {
  /* page size on Linux, Mac OS X and Windows */
  const int OUTPUT_DATA_INITIAL_ALLOCATION = 4096;
  output_string output;
  if (!(output.data = malloc(OUTPUT_DATA_INITIAL_ALLOCATION))) {
    agerrorf("failure malloc'ing for result string");
    exit(-1);
  }
  job->output_data = output.data;
  job->output_data_allocated = OUTPUT_DATA_INITIAL_ALLOCATION;
  job->output_data_position = 0;

  gvc->api[API_device] = &svg_device_available;
  gvc->api[API_render] = &svg_render_available;

  job->device.engine = NULL;
  job->device.features = &my_device_features_svg;
  job->device.id = FORMAT_SVG;
  job->device.type = "svg:svg";

  job->flags |= my_device_features_svg.flags;

  job->render.engine = &svg_engine;
  job->render.features = &my_render_features_svg;
  job->render.type = "svg";

  job->flags |= my_render_features_svg.flags;

  job->render.id = FORMAT_SVG;

  jobsvg_begin_job(job);
  // FIXME: remove hardcode
  jobsvg_comment(job, "Generated by graphviz version a (a)\n");

  gvc->active_jobs = job;  /* first job of new list */
  job->next_active = NULL; /* terminate active list */
  job->callbacks = NULL;   // FIXME: not used

  init_job_pad(job);
  init_job_margin(job);
  init_job_dpi(job, g);
  init_job_viewport(job, g);
  /* device dpi is now known */
  job->scale.x = job->zoom * job->dpi.x / POINTS_PER_INCH;
  job->scale.y = job->zoom * job->dpi.y / POINTS_PER_INCH;

  /* unpaginated image size - in points - in graph orientation */
  pointf imageSize = job->view; // image size on one page of the graph - points

  /* rotate imageSize to page orientation */
  if (job->rotation)
    imageSize = exch_xyf(imageSize);

  /* margin - in points - in page orientation */
  pointf margin = job->margin; // margin for a page of the graph - points

  /* determine pagination */
  job->pagesArraySize.x = job->pagesArraySize.y = job->numPages = 1;

  /* initial window size */
  job->width =
      ROUND((imageSize.x + 2 * margin.x) * job->dpi.x / POINTS_PER_INCH);
  job->height =
      ROUND((imageSize.y + 2 * margin.y) * job->dpi.y / POINTS_PER_INCH);

  // FIXME: add warning about ignoring centering attribute
  // https://graphviz.org/docs/attrs/center/

  /* rotate back into graph orientation */
  if (job->rotation) {
    margin = exch_xyf(margin);
  }

  /* canvas area, centered if necessary */
  job->canvasBox.LL.x = margin.x;
  job->canvasBox.LL.y = margin.y;
  job->canvasBox.UR.x = margin.x + job->view.x;
  job->canvasBox.UR.y = margin.y + job->view.y;

  /* pageBoundingBox in device units and page orientation */
  box pageBoundingBox = {0};
  pageBoundingBox.LL.x =
      ROUND(job->canvasBox.LL.x * job->dpi.x / POINTS_PER_INCH);
  pageBoundingBox.LL.y =
      ROUND(job->canvasBox.LL.y * job->dpi.y / POINTS_PER_INCH);
  pageBoundingBox.UR.x =
      ROUND(job->canvasBox.UR.x * job->dpi.x / POINTS_PER_INCH);
  pageBoundingBox.UR.y =
      ROUND(job->canvasBox.UR.y * job->dpi.y / POINTS_PER_INCH);
  if (job->rotation) {
    pageBoundingBox.LL = exch_xy(pageBoundingBox.LL);
    pageBoundingBox.UR = exch_xy(pageBoundingBox.UR);
    job->canvasBox.LL = exch_xyf(job->canvasBox.LL);
    job->canvasBox.UR = exch_xyf(job->canvasBox.UR);
  }

  /* size of one page in graph units */
  double pageSize_x = job->view.x / job->zoom;
  double pageSize_y = job->view.y / job->zoom;
  boxf clip = {0};
  clip.LL.x = job->focus.x - pageSize_x / 2.0;
  clip.LL.y = job->focus.y - pageSize_y / 2.0;
  clip.UR.x = clip.LL.x + pageSize_x;
  clip.UR.y = clip.LL.y + pageSize_y;

  {
    output_string output = job2output_string(job);
    SafeJob safe_job = {
        .layerNum = job->layerNum,
        .pagesArrayElem = job->pagesArrayElem,
        .dpi = job->dpi,
        .rotation = job->rotation,
        .pageBoundingBox = pageBoundingBox,
        .height = job->height,
        .width = job->width,
        .scale = job->scale,
        .canvasBox = job->canvasBox,
        .zoom = job->zoom,
        .clip = clip,

        // from gvc
        .graph = job->gvc->g,
        .defaultlinestyle = job->gvc->defaultlinestyle,
        .layerIDs = job->gvc->layerIDs,
        .layerDelims = job->gvc->layerDelims,
        .layerListDelims = job->gvc->layerListDelims,
        .numLayers = job->gvc->numLayers,
    };
    emit_graph(&output, &safe_job, job->obj, g, job->gvc->layerlist,
               job->flags);

    job->gvc->common.lib =
        NULL; /* FIXME - minimally this doesn't belong here */

    free(job->active_tooltip);
    free(job->selected_href);
    free(job);
    gvc->jobs = gvc->job = gvc->active_jobs = NULL;

    return output;
  }
}
