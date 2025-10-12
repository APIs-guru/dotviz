
#include "cgraph.h"
#include "geom.h"
#include "geomprocs.h"
#include "gv_ctype.h"
#include "gvc.h" // IWYU pragma: keep
#include "gvcext.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvplugin.h"
#include "gvplugin_device.h" // IWYU pragma: keep
#include "gvplugin_render.h" // IWYU pragma: keep
#include "output_string.h"
#include "strview.h" // IWYU pragma: keep
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

extern void my_attach_attrs_and_arrows(graph_t *g);

typedef int (*putstrfn)(void *chan, const char *str);
typedef int (*flushfn)(void *chan);

static gvrender_engine_t dot_engine = {
    0,       /* dot_begin_job */
    0,       /* dot_end_job */
    0, 0, 0, /* dot_begin_layer */
    0,       /* dot_end_layer */
    0,       /* dot_begin_page */
    0,       /* dot_end_page */
    0,       /* dot_begin_cluster */
    0,       /* dot_end_cluster */
    0,       /* dot_begin_nodes */
    0,       /* dot_end_nodes */
    0,       /* dot_begin_edges */
    0,       /* dot_end_edges */
    0,       /* dot_begin_node */
    0,       /* dot_end_node */
    0,       /* dot_begin_edge */
    0,       /* dot_end_edge */
    0,       /* dot_begin_anchor */
    0,       /* dot_end_anchor */
    0,       /* dot_begin_label */
    0,       /* dot_end_label */
    0,       /* dot_textspan */
    0,       /* dot_resolve_color */
    0,       /* dot_ellipse */
    0,       /* dot_polygon */
    0,       /* dot_bezier */
    0,       /* dot_polyline */
    0,       /* dot_comment */
    0,       /* dot_library_shape */
};
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

static void init_job_pad(GVJ_t *job) {
  GVC_t *gvc = job->gvc;

  if (gvc->graph_sets_pad) {
    job->pad = gvc->pad;
  } else {
    job->pad.x = job->pad.y = 0;
  }
}

static void init_job_margin(GVJ_t *job) {
  GVC_t *gvc = job->gvc;

  if (gvc->graph_sets_margin) {
    job->margin = gvc->margin;
  } else {
    job->margin.x = job->margin.y = 0;
  }
}

static void init_job_dpi(GVJ_t *job, graph_t *g) {
  if (GD_drawing(g)->dpi != 0) {
    job->dpi.x = job->dpi.y = GD_drawing(g)->dpi;
  } else {
    job->dpi.x = job->dpi.y = 72;
  }
}

extern output_string my_agwrite(Agraph_t *g,
                                unsigned long max_output_linelength);
int render_dot(GVC_t *gvc, GVJ_t *job, Agraph_t *g, char **result,
               size_t *length) {
  char *linelength = agget(g, "linelength");
  unsigned long max_len = 0;
  if (linelength != NULL && gv_isdigit(*linelength)) {
    max_len = strtoul(linelength, NULL, 10);
  }

  gvc->api[API_device] = &dot_device_available;
  gvc->api[API_render] = &dot_render_available;

  job->device.engine = NULL;
  job->device.features = &my_device_features_dot;
  job->device.id = FORMAT_DOT;
  job->device.type = "dot:dot";

  job->render.engine = &dot_engine;
  job->render.features = &my_render_features_dot;
  job->render.type = "dot";

  job->flags |= GVRENDER_DOES_TRANSFORM;

  job->render.id = FORMAT_DOT;

  gvc->active_jobs = job;  /* first job of new list */
  job->next_active = NULL; /* terminate active list */
  job->callbacks = &gvdevice_callbacks;

  init_job_pad(job);
  init_job_margin(job);
  init_job_dpi(job, g);
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

  // agwarningf("pagedir=%s ignored\n", gvc->pagedir);

  // GVC_t* gvc_ = GD_gvc(g);
  // GD_gvc(g) = NULL;
  my_attach_attrs_and_arrows(g);

  /* reset node state */
  for (node_t *n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;

  output_string output = my_agwrite(g, max_len);
  // GD_gvc(g) = gvc_;

  job->gvc->common.lib = NULL; /* FIXME - minimally this doesn't belong here */

  *result = output.data;
  *length = output.data_position;

  free(job->active_tooltip);
  free(job->selected_href);
  free(job);
  gvc->jobs = gvc->job = gvc->active_jobs = NULL;
  gvc->common.viewNum = 0;

  return 0;
}