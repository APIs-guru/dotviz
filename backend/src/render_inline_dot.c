#include "agrw.h"
#include "cgraph.h"
#include "const.h"
#include "geomprocs.h"
#include "gv_ctype.h"
#include "gv_math.h"
#include "gvc.h" // IWYU pragma: keep
#include "gvcext.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvcproc.h"
#include "gvplugin.h"
#include "gvplugin_device.h" // IWYU pragma: keep
#include "gvplugin_render.h" // IWYU pragma: keep
#include "render_inline.h"
#include "streq.h"
#include "strview.h" // IWYU pragma: keep
#include "util/list.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  FORMAT_DOT,
  FORMAT_CANON,
  FORMAT_PLAIN,
  FORMAT_PLAIN_EXT,
  FORMAT_XDOT,
  FORMAT_XDOT12,
  FORMAT_XDOT14,
} format_type;
extern gvrender_engine_t dot_engine;
extern bool Y_invert;

gvdevice_features_t my_device_features_dot = {
    0,          /* flags */
    {0., 0.},   /* default margin - points */
    {0., 0.},   /* default page width, height - points */
    {72., 72.}, /* default dpi */
};

gvrender_features_t my_render_features_dot = {
    GVRENDER_DOES_TRANSFORM,
    /* not really - uses raw graph coords */ /* flags */
    0.,                                      /* default pad - graph units */
    NULL,                                    /* knowncolors */
    0,                                       /* sizeof knowncolors */
    COLOR_STRING,                            /* color_type */
};

gvplugin_installed_t dot_device_installed = {FORMAT_DOT, "dot:dot", 1, NULL,
                                             &my_device_features_dot};
gvplugin_available_t dot_device_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &dot_device_installed,
    .typestr = "dot:dot",
};

gvplugin_installed_t dot_render_installed = {FORMAT_DOT, "dot", 1, &dot_engine,
                                             &my_render_features_dot};
gvplugin_available_t dot_render_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &dot_render_installed,
    .typestr = "dot",
};

extern gvdevice_callbacks_t gvdevice_callbacks;

#define DEFAULT_DPI 96

#define EPSILON .0001

static point pagecode(GVJ_t *job, char c) {
  point rv = {0};
  switch (c) {
  case 'T':
    job->pagesArrayFirst.y = job->pagesArraySize.y - 1;
    rv.y = -1;
    break;
  case 'B':
    rv.y = 1;
    break;
  case 'L':
    rv.x = 1;
    break;
  case 'R':
    job->pagesArrayFirst.x = job->pagesArraySize.x - 1;
    rv.x = -1;
    break;
  default:
    // ignore; will trigger a warning later in our caller
    break;
  }
  return rv;
}

static void init_job_pagination(GVJ_t *job, graph_t *g) {
  GVC_t *gvc = job->gvc;
  pointf pageSize;        /* page size for the graph - points*/
  pointf centering = {0}; // centering offset - points

  /* unpaginated image size - in points - in graph orientation */
  pointf imageSize = job->view; // image size on one page of the graph - points

  /* rotate imageSize to page orientation */
  if (job->rotation)
    imageSize = exch_xyf(imageSize);

  /* margin - in points - in page orientation */
  pointf margin = job->margin; // margin for a page of the graph - points

  /* determine pagination */
  if (gvc->graph_sets_pageSize && (job->flags & GVDEVICE_DOES_PAGES)) {
    /* page was set by user */

    /* determine size of page for image */
    pageSize.x = gvc->pageSize.x - 2 * margin.x;
    pageSize.y = gvc->pageSize.y - 2 * margin.y;

    if (pageSize.x < EPSILON)
      job->pagesArraySize.x = 1;
    else {
      job->pagesArraySize.x = (int)(imageSize.x / pageSize.x);
      if (imageSize.x - job->pagesArraySize.x * pageSize.x > EPSILON)
        job->pagesArraySize.x++;
    }
    if (pageSize.y < EPSILON)
      job->pagesArraySize.y = 1;
    else {
      job->pagesArraySize.y = (int)(imageSize.y / pageSize.y);
      if (imageSize.y - job->pagesArraySize.y * pageSize.y > EPSILON)
        job->pagesArraySize.y++;
    }
    job->numPages = job->pagesArraySize.x * job->pagesArraySize.y;

    /* find the drawable size in points */
    imageSize.x = fmin(imageSize.x, pageSize.x);
    imageSize.y = fmin(imageSize.y, pageSize.y);
  } else {
    /* page not set by user, use default from renderer */
    if (job->render.features) {
      pageSize.x = job->device.features->default_pagesize.x - 2 * margin.x;
      pageSize.x = fmax(pageSize.x, 0);
      pageSize.y = job->device.features->default_pagesize.y - 2 * margin.y;
      pageSize.y = fmax(pageSize.y, 0);
    } else
      pageSize.x = pageSize.y = 0.;
    job->pagesArraySize.x = job->pagesArraySize.y = job->numPages = 1;

    pageSize.x = fmax(pageSize.x, imageSize.x);
    pageSize.y = fmax(pageSize.y, imageSize.y);
  }

  /* initial window size */
  job->width =
      ROUND((pageSize.x + 2 * margin.x) * job->dpi.x / POINTS_PER_INCH);
  job->height =
      ROUND((pageSize.y + 2 * margin.y) * job->dpi.y / POINTS_PER_INCH);

  /* set up pagedir */
  job->pagesArrayMajor = (point){0};
  job->pagesArrayMinor = (point){0};
  job->pagesArrayFirst = (point){0};
  job->pagesArrayMajor = pagecode(job, gvc->pagedir[0]);
  job->pagesArrayMinor = pagecode(job, gvc->pagedir[1]);
  if (abs(job->pagesArrayMajor.x + job->pagesArrayMinor.x) != 1 ||
      abs(job->pagesArrayMajor.y + job->pagesArrayMinor.y) != 1) {
    job->pagesArrayMajor = pagecode(job, 'B');
    job->pagesArrayMinor = pagecode(job, 'L');
    agwarningf("pagedir=%s ignored\n", gvc->pagedir);
  }

  /* determine page box including centering */
  if (GD_drawing(g)->centered) {
    if (pageSize.x > imageSize.x)
      centering.x = (pageSize.x - imageSize.x) / 2;
    if (pageSize.y > imageSize.y)
      centering.y = (pageSize.y - imageSize.y) / 2;
  }

  /* rotate back into graph orientation */
  if (job->rotation) {
    imageSize = exch_xyf(imageSize);
    pageSize = exch_xyf(pageSize);
    margin = exch_xyf(margin);
    centering = exch_xyf(centering);
  }

  /* canvas area, centered if necessary */
  job->canvasBox.LL.x = margin.x + centering.x;
  job->canvasBox.LL.y = margin.y + centering.y;
  job->canvasBox.UR.x = margin.x + centering.x + imageSize.x;
  job->canvasBox.UR.y = margin.y + centering.y + imageSize.y;

  /* size of one page in graph units */
  job->pageSize.x = imageSize.x / job->zoom;
  job->pageSize.y = imageSize.y / job->zoom;

  /* pageBoundingBox in device units and page orientation */
  job->pageBoundingBox.LL.x =
      ROUND(job->canvasBox.LL.x * job->dpi.x / POINTS_PER_INCH);
  job->pageBoundingBox.LL.y =
      ROUND(job->canvasBox.LL.y * job->dpi.y / POINTS_PER_INCH);
  job->pageBoundingBox.UR.x =
      ROUND(job->canvasBox.UR.x * job->dpi.x / POINTS_PER_INCH);
  job->pageBoundingBox.UR.y =
      ROUND(job->canvasBox.UR.y * job->dpi.y / POINTS_PER_INCH);
  if (job->rotation) {
    job->pageBoundingBox.LL = exch_xy(job->pageBoundingBox.LL);
    job->pageBoundingBox.UR = exch_xy(job->pageBoundingBox.UR);
    job->canvasBox.LL = exch_xyf(job->canvasBox.LL);
    job->canvasBox.UR = exch_xyf(job->canvasBox.UR);
  }
}

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
extern obj_state_t *push_obj_state(GVJ_t *job);
extern void initObjMapData(GVJ_t *job, textlabel_t *lab, void *gobj);

void my_emit_begin_graph(GVJ_t *job, graph_t *g) {
  obj_state_t *obj;

  obj = push_obj_state(job);
  obj->type = ROOTGRAPH_OBJTYPE;
  obj->u.g = g;
  obj->emit_state = EMIT_GDRAW;

  initObjMapData(job, GD_label(g), g);

  dot_engine.begin_graph(job);
}

extern void emit_colors(GVJ_t *job, graph_t *g);
extern void firstlayer(GVJ_t *job, int **listp);
extern bool validlayer(GVJ_t *job);
extern void nextlayer(GVJ_t *job, int **listp);
extern int numPhysicalLayers(GVJ_t *job);
extern void firstpage(GVJ_t *job);
extern bool validpage(GVJ_t *job);
extern void nextpage(GVJ_t *job);
extern void emit_page(GVJ_t *job, graph_t *g);
extern void emit_end_graph(GVJ_t *job);

static void my_emit_graph(GVJ_t *job, graph_t *g) {
  node_t *n;
  char *s;
  int flags = job->flags;
  int *lp;

  /* device dpi is now known */
  job->scale.x = job->zoom * job->dpi.x / POINTS_PER_INCH;
  job->scale.y = job->zoom * job->dpi.y / POINTS_PER_INCH;

  job->devscale.x = job->dpi.x / POINTS_PER_INCH;
  job->devscale.y = job->dpi.y / POINTS_PER_INCH;
  if ((job->flags & GVRENDER_Y_GOES_DOWN) || (Y_invert))
    job->devscale.y *= -1;

  /* compute current view in graph units */
  if (job->rotation) {
    job->view.y = job->width / job->scale.y;
    job->view.x = job->height / job->scale.x;
  } else {
    job->view.x = job->width / job->scale.x;
    job->view.y = job->height / job->scale.y;
  }

  job->layerNum = 0;
  my_emit_begin_graph(job, g);

  if (flags & EMIT_COLORS)
    emit_colors(job, g);

  /* reset node state */
  for (n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;
  /* iterate layers */
  for (firstlayer(job, &lp); validlayer(job); nextlayer(job, &lp)) {
    if (numPhysicalLayers(job) > 1)
      gvrender_begin_layer(job);

    /* iterate pages */
    for (firstpage(job); validpage(job); nextpage(job))
      emit_page(job, g);

    if (numPhysicalLayers(job) > 1)
      gvrender_end_layer(job);
  }
  emit_end_graph(job);
}

int render_dot(GVC_t *gvc, GVJ_t *job, Agraph_t *g, char **result,
               size_t *length) {
  int rc;
  gvc->api[API_device] = &dot_device_available;
  gvc->api[API_render] = &dot_render_available;

  job->device.engine = NULL;
  job->device.features = &my_device_features_dot;
  job->device.id = FORMAT_DOT;
  job->device.type = "dot:dot";

  job->flags |= my_device_features_dot.flags;

  job->render.engine = &dot_engine;
  job->render.features = &my_render_features_dot;
  job->render.type = "dot";

  job->flags |= my_render_features_dot.flags;

  job->render.id = FORMAT_DOT;

  gvc->active_jobs = job;  /* first job of new list */
  job->next_active = NULL; /* terminate active list */
  job->callbacks = &gvdevice_callbacks;

  init_job_pad(job);
  init_job_margin(job);
  init_job_dpi(job, g);
  init_job_viewport(job, g);
  init_job_pagination(job, g);

  my_emit_graph(job, g);

  job->gvc->common.lib = NULL; /* FIXME - minimally this doesn't belong here */

  if (rc == 0) {
    *result = job->output_data;
    *length = job->output_data_position;
  }

  free(job->active_tooltip);
  free(job->selected_href);
  free(job);
  gvc->jobs = gvc->job = gvc->active_jobs = NULL;
  gvc->common.viewNum = 0;

  return 0;
}