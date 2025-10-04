#include "render_inline.h"
#include "const.h"
#include "geomprocs.h"
#include "gv_ctype.h"
#include "gv_math.h"
#include "gvc.h"
#include "gvcext.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvcproc.h"
#include "gvplugin.h"
#include "streq.h"
#include "strview.h" // IWYU pragma: keep
#include "util/list.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int my_gvrender_select(GVJ_t *job, const char *str);

extern Agsym_t *G_ordering, *G_peripheries, *G_penwidth, *G_gradientangle,
    *G_margin;
extern Agsym_t *N_height, *N_width, *N_shape, *N_color, *N_fillcolor,
    *N_fontsize, *N_fontname, *N_fontcolor, *N_label, *N_xlabel, *N_nojustify,
    *N_style, *N_showboxes, *N_sides, *N_peripheries, *N_ordering,
    *N_orientation, *N_skew, *N_distortion, *N_fixed, *N_imagescale,
    *N_imagepos, *N_layer, *N_group, *N_comment, *N_vertices, *N_z, *N_penwidth,
    *N_gradientangle;
extern Agsym_t *E_weight, *E_minlen, *E_color, *E_fillcolor, *E_fontsize,
    *E_fontname, *E_fontcolor, *E_label, *E_xlabel, *E_dir, *E_style,
    *E_decorate, *E_showboxes, *E_arrowsz, *E_constr, *E_layer, *E_comment,
    *E_label_float, *E_samehead, *E_sametail, *E_headlabel, *E_taillabel,
    *E_labelfontsize, *E_labelfontname, *E_labelfontcolor, *E_labeldistance,
    *E_labelangle, *E_tailclip, *E_headclip, *E_penwidth;

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

#define EPSILON .0001

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

static bool is_natural_number(const char *sstr) {
  const char *str = sstr;

  while (*str)
    if (!gv_isdigit(*str++))
      return false;
  return true;
}

static int layer_index(GVC_t *gvc, char *str, int all) {
  int i;

  if (streq(str, "all"))
    return all;
  if (is_natural_number(str))
    return atoi(str);
  if (gvc->layerIDs)
    for (i = 1; i <= gvc->numLayers; i++)
      if (streq(str, gvc->layerIDs[i]))
        return i;
  return -1;
}

static bool selectedLayer(GVC_t *gvc, int layerNum, int numLayers, char *spec) {
  int n0, n1;
  char *w0, *w1;
  char *buf_part_p = NULL, *buf_p = NULL, *cur, *part_in_p;
  bool rval = false;

  // copy `spec` so we can `strtok_r` it
  char *spec_copy = gv_strdup(spec);
  part_in_p = spec_copy;

  while (!rval &&
         (cur = strtok_r(part_in_p, gvc->layerListDelims, &buf_part_p))) {
    w1 = w0 = strtok_r(cur, gvc->layerDelims, &buf_p);
    if (w0)
      w1 = strtok_r(NULL, gvc->layerDelims, &buf_p);
    if (w1 != NULL) {
      assert(w0 != NULL);
      n0 = layer_index(gvc, w0, 0);
      n1 = layer_index(gvc, w1, numLayers);
      if (n0 >= 0 || n1 >= 0) {
        if (n0 > n1) {
          SWAP(&n0, &n1);
        }
        rval = BETWEEN(n0, layerNum, n1);
      }
    } else if (w0 != NULL) {
      n0 = layer_index(gvc, w0, layerNum);
      rval = (n0 == layerNum);
    } else {
      rval = false;
    }
    part_in_p = NULL;
  }
  free(spec_copy);
  return rval;
}

static bool selectedlayer(GVJ_t *job, char *spec) {
  return selectedLayer(job->gvc, job->layerNum, job->numLayers, spec);
}

/* Parse the graph's layerselect attribute, which determines
 * which layers are emitted. The specification is the same used
 * by the layer attribute.
 *
 * If we find n layers, we return an array arr of n+2 ints. arr[0]=n.
 * arr[n+1]=numLayers+1, acting as a sentinel. The other entries give
 * the desired layer indices.
 *
 * If no layers are detected, NULL is returned.
 *
 * This implementation does a linear walk through each layer index and
 * uses selectedLayer to match it against p. There is probably a more
 * efficient way to do this, but this is simple and until we find people
 * using huge numbers of layers, it should be adequate.
 */
static int *parse_layerselect(GVC_t *gvc, char *p) {
  int *laylist = gv_calloc(gvc->numLayers + 2, sizeof(int));
  int i, cnt = 0;
  for (i = 1; i <= gvc->numLayers; i++) {
    if (selectedLayer(gvc, i, gvc->numLayers, p)) {
      laylist[++cnt] = i;
    }
  }
  if (cnt) {
    laylist[0] = cnt;
    laylist[cnt + 1] = gvc->numLayers + 1;
  } else {
    agwarningf("The layerselect attribute \"%s\" does not match any layer "
               "specifed by the layers attribute - ignored.\n",
               p);
    free(laylist);
    laylist = NULL;
  }
  return laylist;
}

DEFINE_LIST(layer_names, char *)

/* Split input string into tokens, with separators specified by
 * the layersep attribute. Store the values in the gvc->layerIDs array,
 * starting at index 1, and return the count.
 * Note that there is no mechanism
 * to free the memory before exit.
 */
static int parse_layers(GVC_t *gvc, graph_t *g, char *p) {
  char *tok;

  gvc->layerDelims = agget(g, "layersep");
  if (!gvc->layerDelims)
    gvc->layerDelims = DEFAULT_LAYERSEP;
  gvc->layerListDelims = agget(g, "layerlistsep");
  if (!gvc->layerListDelims)
    gvc->layerListDelims = DEFAULT_LAYERLISTSEP;
  if ((tok =
           strpbrk(gvc->layerDelims,
                   gvc->layerListDelims))) { /* conflict in delimiter strings */
    agwarningf("The character \'%c\' appears in both the layersep and "
               "layerlistsep attributes - layerlistsep ignored.\n",
               *tok);
    gvc->layerListDelims = "";
  }

  gvc->layers = gv_strdup(p);
  layer_names_t layerIDs = {0};

  // inferred entry for the first (unnamed) layer
  layer_names_append(&layerIDs, NULL);

  for (tok = strtok(gvc->layers, gvc->layerDelims); tok;
       tok = strtok(NULL, gvc->layerDelims)) {
    layer_names_append(&layerIDs, tok);
  }

  assert(layer_names_size(&layerIDs) - 1 <= INT_MAX);
  int ntok = (int)(layer_names_size(&layerIDs) - 1);

  // if we found layers, save them for later reference
  if (layer_names_size(&layerIDs) > 1) {
    layer_names_append(&layerIDs, NULL); // add a terminating entry
    gvc->layerIDs = layer_names_detach(&layerIDs);
  }
  layer_names_free(&layerIDs);

  return ntok;
}

static void init_layering(GVC_t *gvc, graph_t *g) {
  char *str;

  /* free layer strings and pointers from previous graph */
  free(gvc->layers);
  gvc->layers = NULL;
  free(gvc->layerIDs);
  gvc->layerIDs = NULL;
  free(gvc->layerlist);
  gvc->layerlist = NULL;
  if ((str = agget(g, "layers")) != 0) {
    gvc->numLayers = parse_layers(gvc, g, str);
    if ((str = agget(g, "layerselect")) != 0 && *str) {
      gvc->layerlist = parse_layerselect(gvc, str);
    }
  } else {
    gvc->numLayers = 1;
  }
}

static char *defaultlinestyle[3] = {"solid\0", "setlinewidth\0001\0", 0};

static void init_gvc(GVC_t *gvc, graph_t *g) {
  double xf, yf;
  char *p;
  int i;

  gvc->g = g;

  /* margins */
  gvc->graph_sets_margin = false;
  if ((p = agget(g, "margin"))) {
    i = sscanf(p, "%lf,%lf", &xf, &yf);
    if (i > 0) {
      gvc->margin.x = gvc->margin.y = xf * POINTS_PER_INCH;
      if (i > 1)
        gvc->margin.y = yf * POINTS_PER_INCH;
      gvc->graph_sets_margin = true;
    }
  }

  /* pad */
  gvc->graph_sets_pad = false;
  if ((p = agget(g, "pad"))) {
    i = sscanf(p, "%lf,%lf", &xf, &yf);
    if (i > 0) {
      gvc->pad.x = gvc->pad.y = xf * POINTS_PER_INCH;
      if (i > 1)
        gvc->pad.y = yf * POINTS_PER_INCH;
      gvc->graph_sets_pad = true;
    }
  }

  /* pagesize */
  gvc->graph_sets_pageSize = false;
  gvc->pageSize = GD_drawing(g)->page;
  if (GD_drawing(g)->page.x > 0.001 && GD_drawing(g)->page.y > 0.001)
    gvc->graph_sets_pageSize = true;

  /* rotation */
  if (GD_drawing(g)->landscape)
    gvc->rotation = 90;
  else
    gvc->rotation = 0;

  /* pagedir */
  gvc->pagedir = "BL";
  if ((p = agget(g, "pagedir")) && p[0])
    gvc->pagedir = p;

  /* bounding box */
  gvc->bb = GD_bb(g);

  /* clusters have peripheries */
  G_peripheries = agfindgraphattr(g, "peripheries");
  G_penwidth = agfindgraphattr(g, "penwidth");

  /* default font */
  gvc->defaultfontname = late_nnstring(NULL, N_fontname, DEFAULT_FONTNAME);
  gvc->defaultfontsize =
      late_double(NULL, N_fontsize, DEFAULT_FONTSIZE, MIN_FONTSIZE);

  /* default line style */
  gvc->defaultlinestyle = defaultlinestyle;

  gvc->graphname = agnameof(g);
}

static boxf bezier_bb(bezier bz) {
  pointf p, p1, p2;
  boxf bb;

  assert(bz.size > 0);
  assert(bz.size % 3 == 1);
  bb.LL = bb.UR = bz.list[0];
  for (size_t i = 1; i < bz.size;) {
    /* take mid-point between two control points for bb calculation */
    p1 = bz.list[i];
    i++;
    p2 = bz.list[i];
    i++;
    p.x = (p1.x + p2.x) / 2;
    p.y = (p1.y + p2.y) / 2;
    expandbp(&bb, p);

    p = bz.list[i];
    expandbp(&bb, p);
    i++;
  }
  return bb;
}

extern boxf arrow_bb(pointf p, pointf u, double arrowsize);
static void init_splines_bb(splines *spl) {
  bezier bz;
  boxf bb, b;

  assert(spl->size > 0);
  bz = spl->list[0];
  bb = bezier_bb(bz);
  for (size_t i = 0; i < spl->size; i++) {
    if (i > 0) {
      bz = spl->list[i];
      b = bezier_bb(bz);
      EXPANDBB(&bb, b);
    }
    if (bz.sflag) {
      b = arrow_bb(bz.sp, bz.list[0], 1);
      EXPANDBB(&bb, b);
    }
    if (bz.eflag) {
      b = arrow_bb(bz.ep, bz.list[bz.size - 1], 1);
      EXPANDBB(&bb, b);
    }
  }
  spl->bb = bb;
}

static void init_bb_edge(edge_t *e) {
  splines *spl;

  spl = ED_spl(e);
  if (spl)
    init_splines_bb(spl);
}

static void init_bb_node(graph_t *g, node_t *n) {
  edge_t *e;

  ND_bb(n).LL.x = ND_coord(n).x - ND_lw(n);
  ND_bb(n).LL.y = ND_coord(n).y - ND_ht(n) / 2.;
  ND_bb(n).UR.x = ND_coord(n).x + ND_rw(n);
  ND_bb(n).UR.y = ND_coord(n).y + ND_ht(n) / 2.;

  for (e = agfstout(g, n); e; e = agnxtout(g, e))
    init_bb_edge(e);

  /* IDEA - could also save in the node the bb of the node and
  all of its outedges, then the scan time would be proportional
  to just the number of nodes for many graphs.
  Wouldn't work so well if the edges are sprawling all over the place
  because then the boxes would overlap a lot and require more tests,
  but perhaps that wouldn't add much to the cost before trying individual
  nodes and edges. */
}

static void init_bb(graph_t *g) {
  node_t *n;

  for (n = agfstnode(g); n; n = agnxtnode(g, n))
    init_bb_node(g, n);
}

/* Determine order of output.
 * Output usually in breadth first graph walk order
 */
static int chkOrder(graph_t *g) {
  char *p = agget(g, "outputorder");
  if (p) {
    if (!strcmp(p, "nodesfirst"))
      return EMIT_SORTED;
    if (!strcmp(p, "edgesfirst"))
      return EMIT_EDGE_SORTED;
  }
  return 0;
}

extern gvevent_key_binding_t gvevent_key_binding[];
extern const size_t gvevent_key_binding_size;
extern gvdevice_callbacks_t gvdevice_callbacks;
extern void emit_graph(GVJ_t *job, graph_t *g);

/* load a plugin of type=str
        the str can optionally contain one or more ":dependencies"

        examples:
                png
                png:cairo
        fully qualified:
                png:cairo:cairo
                png:cairo:gd
                png:gd:gd

*/

gvplugin_available_t *available_from_install(gvplugin_installed_t *lib) {
  gvplugin_available_t *plugin = gv_alloc(sizeof(gvplugin_available_t));
  plugin->next = NULL;
  plugin->typestr = (char *)lib->type;
  plugin->quality = 1;
  plugin->package = NULL;
  plugin->typeptr = lib; /* null if not loaded */
}

typedef enum {
  FORMAT_DOT,
  FORMAT_CANON,
  FORMAT_PLAIN,
  FORMAT_PLAIN_EXT,
  FORMAT_XDOT,
  FORMAT_XDOT12,
  FORMAT_XDOT14,
} format_type;

extern gvdevice_features_t device_features_dot;

gvplugin_installed_t dot_installed = {FORMAT_DOT, "dot:dot", 1, NULL,
                                      &device_features_dot};
gvplugin_installed_t gv_installed = {FORMAT_DOT, "gv:dot", 1, NULL,
                                     &device_features_dot};
gvplugin_available_t dot_device_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &dot_installed,
    .typestr = "dot:dot",
};
gvplugin_available_t gv_device_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &gv_installed,
    .typestr = "gv:dot",
};

extern gvrender_engine_t dot_engine;
extern gvrender_features_t render_features_dot;
gvplugin_installed_t gvrender_dot_installed = {
    FORMAT_DOT, "dot", 1, &dot_engine, &render_features_dot};
gvplugin_available_t gvrender_dot_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &gvrender_dot_installed,
    .typestr = "dot",
};

enum { FORMAT_SVG, FORMAT_SVGZ, FORMAT_SVG_INLINE };
extern gvrender_engine_t svg_engine;
extern gvrender_features_t render_features_svg;

gvplugin_installed_t gvrender_svg_installed = {
    FORMAT_SVG, "svg", 1, &svg_engine, &render_features_svg};
gvplugin_available_t gvrender_svg_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &gvrender_svg_installed,
    .typestr = "svg",
};

extern gvdevice_features_t device_features_svg;
gvplugin_installed_t svg_installed = {FORMAT_SVG, "svg:svg", 1, NULL,
                                      &device_features_svg};
gvplugin_available_t svg_device_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &svg_installed,
    .typestr = "svg:svg",
};

void my_gvplugin_load(GVC_t *gvc, const char *str) {
  if (!strcmp(str, "dot")) {
    gvc->api[API_device] = &dot_device_available;
    gvc->api[API_render] = &gvrender_dot_available;
  } else if (!strcmp(str, "gv")) {
    gvc->api[API_device] = &gv_device_available;
    gvc->api[API_render] = &gvrender_dot_available;
  } else if (!strcmp(str, "svg")) {
    gvc->api[API_device] = &svg_device_available;
    gvc->api[API_render] = &gvrender_svg_available;
  }
}

int my_gvrender_select(GVJ_t *job, const char *str) {
  GVC_t *gvc = job->gvc;
  gvplugin_available_t *plugin;
  gvplugin_installed_t *typeptr;

  my_gvplugin_load(gvc, str);

  /* When job is created, it is zeroed out.
   * Some flags, such as OUTPUT_NOT_REQUIRED, may already be set,
   * so don't reset.
   */
  /* job->flags = 0; */

  plugin = gvc->api[API_device];
  if (plugin) {
    typeptr = plugin->typeptr;
    job->device.engine = typeptr->engine;
    job->device.features = typeptr->features;
    job->device.id = typeptr->id;
    job->device.type = plugin->typestr;

    job->flags |= job->device.features->flags;
  } else {
    return NO_SUPPORT; /* FIXME - should differentiate problem */
  }
  /* The device plugin has a dependency on a render plugin,
   * so the render plugin should be available as well now */
  plugin = gvc->api[API_render];
  if (plugin) {
    typeptr = plugin->typeptr;
    job->render.engine = typeptr->engine;
    job->render.features = typeptr->features;
    job->render.type = plugin->typestr;

    job->flags |= job->render.features->flags;

    if (job->device.engine)
      job->render.id = typeptr->id;
    else
      /* A null device engine indicates that the device id is also the renderer
       * id and that the renderer doesn't need "device" functions. Device
       * "features" settings are still available */
      job->render.id = job->device.id;
    return GVRENDER_PLUGIN;
  }
  job->render.engine = NULL;
  return NO_SUPPORT; /* FIXME - should differentiate problem */
}

int my_gvRenderJobs(GVC_t *gvc, graph_t *g) {
  static GVJ_t *prevjob;
  GVJ_t *job, *firstjob;

  init_bb(g);
  init_gvc(gvc, g);
  init_layering(gvc, g);

  int i = 0;
  for (job = gvjobs_first(gvc); job; job = gvjobs_next(gvc)) {

    if (gvc->gvg) {
      job->input_filename = gvc->gvg->input_filename;
      job->graph_index = gvc->gvg->graph_index;
    } else {
      job->input_filename = NULL;
      job->graph_index = 0;
    }
    job->common = &gvc->common;
    job->layout_type = gvc->layout.type;
    job->keybindings = gvevent_key_binding;
    job->numkeys = gvevent_key_binding_size;
    if (!GD_drawing(g)) {
      agerrorf("layout was not done\n");
      return -1;
    }

    job->output_lang = my_gvrender_select(job, job->output_langname);
    if (job->output_lang == NO_SUPPORT) {
      agerrorf("renderer for %s is unavailable\n", job->output_langname);
      return -1;
    }

    job->flags |= chkOrder(g);
    // if we already have an active job list and the device doesn't support
    // multiple output files, or we are about to write to a different output
    // device
    firstjob = gvc->active_jobs;
    if (firstjob) {
      if (!(firstjob->flags & GVDEVICE_DOES_PAGES) ||
          strcmp(job->output_langname, firstjob->output_langname)) {

        gvrender_end_job(firstjob);

        gvc->active_jobs = NULL; /* clear active list */
        gvc->common.viewNum = 0;
        prevjob = NULL;
      }
    } else {
      prevjob = NULL;
    }

    if (gvrender_begin_job(job))
      continue;
    gvc->active_jobs = job;  /* first job of new list */
    job->next_active = NULL; /* terminate active list */
    job->callbacks = &gvdevice_callbacks;

    init_job_pad(job);
    init_job_margin(job);
    init_job_dpi(job, g);
    init_job_viewport(job, g);
    init_job_pagination(job, g);

    if (!(job->flags & GVDEVICE_EVENTS)) {
      emit_graph(job, g);
    }
  }
  return 0;
}

/* Render layout in a specified format to a malloc'ed string */
int gw_gvRenderData(GVC_t *gvc, Agrw_t graph, const char *format, char **result,
                    size_t *length) {
  Agraph_t *g = graph;
  int rc;

  if (strncmp(format, "dot", 3) && strncmp(format, "gv", 2) &&
      strncmp(format, "svg", 3)) {
    agerrorf("Format: \"%s\" not recognized. Use one of: dot gv svg\n", format);
    return -1;
  }

  /* create a job for the required format */
  GVJ_t *job = gvc->job = gvc->jobs = gv_alloc(sizeof(GVJ_t));
  job->output_langname = format;
  job->gvc = gvc;
  job = gvc->job;

  /* page size on Linux, Mac OS X and Windows */
  const int OUTPUT_DATA_INITIAL_ALLOCATION = 4096;

  if (!(*result = malloc(OUTPUT_DATA_INITIAL_ALLOCATION))) {
    agerrorf("failure malloc'ing for result string");
    return -1;
  }

  job->output_data = *result;
  job->output_data_allocated = OUTPUT_DATA_INITIAL_ALLOCATION;
  job->output_data_position = 0;

  rc = my_gvRenderJobs(gvc, g);
  gvrender_end_job(job);

  if (rc == 0) {
    *result = job->output_data;
    *length = job->output_data_position;
  }
  gvjobs_delete(gvc);

  return rc;
}

void gw_gvFreeRenderData(char *data) { free(data); }
