#include "../output_string.h"
#include "cgraph.h"
#include "const.h"
#include "geom.h"
#include "geomprocs.h"
#include "gv_ctype.h"
#include "gv_math.h"
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
#include "streq.h"
#include "types.h"
#include "util/list.h"

static bool is_natural_number(const char *sstr) {
  const char *str = sstr;

  while (*str)
    if (!gv_isdigit(*str++))
      return false;
  return true;
}

static int layer_index(int numLayers, char **layerIDs, char *str, int all) {
  int i;

  if (streq(str, "all"))
    return all;
  if (is_natural_number(str))
    return atoi(str);
  if (layerIDs)
    for (i = 1; i <= numLayers; i++)
      if (streq(str, layerIDs[i]))
        return i;
  return -1;
}

static bool selectedLayer(int layerNum, int numLayers, char *layerDelims,
                          char *layerListDelims, char **layerIDs, char *spec) {
  int n0, n1;
  char *w0, *w1;
  char *buf_part_p = NULL, *buf_p = NULL, *cur, *part_in_p;
  bool rval = false;

  // copy `spec` so we can `strtok_r` it
  char *spec_copy = gv_strdup(spec);
  part_in_p = spec_copy;

  while (!rval && (cur = strtok_r(part_in_p, layerListDelims, &buf_part_p))) {
    w1 = w0 = strtok_r(cur, layerDelims, &buf_p);
    if (w0)
      w1 = strtok_r(NULL, layerDelims, &buf_p);
    if (w1 != NULL) {
      assert(w0 != NULL);
      n0 = layer_index(numLayers, layerIDs, w0, 0);
      n1 = layer_index(numLayers, layerIDs, w1, numLayers);
      if (n0 >= 0 || n1 >= 0) {
        if (n0 > n1) {
          SWAP(&n0, &n1);
        }
        rval = BETWEEN(n0, layerNum, n1);
      }
    } else if (w0 != NULL) {
      n0 = layer_index(numLayers, layerIDs, w0, layerNum);
      rval = (n0 == layerNum);
    } else {
      rval = false;
    }
    part_in_p = NULL;
  }
  free(spec_copy);
  return rval;
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

DEFINE_LIST(layer_names, char *)

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
static int *parse_layerselect(int numLayers, char *layerDelims,
                              char *layerListDelims, char **layerIDs, char *p) {
  int *laylist = gv_calloc(numLayers + 2, sizeof(int));
  int i, cnt = 0;
  for (i = 1; i <= numLayers; i++) {
    if (selectedLayer(i, numLayers, layerDelims, layerListDelims, layerIDs,
                      p)) {
      laylist[++cnt] = i;
    }
  }
  if (cnt) {
    laylist[0] = cnt;
    laylist[cnt + 1] = numLayers + 1;
  } else {
    agwarningf("The layerselect attribute \"%s\" does not match any layer "
               "specifed by the layers attribute - ignored.\n",
               p);
    free(laylist);
    laylist = NULL;
  }
  return laylist;
}

/* Split input string into tokens, with separators specified by
 * the layersep attribute. Store the values in the gvc->layerIDs array,
 * starting at index 1, and return the count.
 * Note that there is no mechanism
 * to free the memory before exit.
 */
static int parse_layers(char ***out_layerIDs, char *layerDelims, char *p) {
  char *tok;

  char *layers = gv_strdup(p);
  layer_names_t layerIDs = {0};

  // inferred entry for the first (unnamed) layer
  layer_names_append(&layerIDs, NULL);

  for (tok = strtok(layers, layerDelims); tok;
       tok = strtok(NULL, layerDelims)) {
    layer_names_append(&layerIDs, tok);
  }

  assert(layer_names_size(&layerIDs) - 1 <= INT_MAX);
  int ntok = (int)(layer_names_size(&layerIDs) - 1);

  // if we found layers, save them for later reference
  if (layer_names_size(&layerIDs) > 1) {
    layer_names_append(&layerIDs, NULL); // add a terminating entry
    *out_layerIDs = layer_names_detach(&layerIDs);
  }
  layer_names_free(&layerIDs);

  return ntok;
}

extern Agsym_t *G_gradientangle, *G_peripheries, *G_penwidth;
extern Agsym_t *N_style, *N_layer, *N_comment, *N_fontname, *N_fontsize;
extern Agsym_t *E_layer, *E_dir, *E_arrowsz, *E_color, *E_fillcolor,
    *E_penwidth, *E_decorate, *E_comment, *E_style;
output_string inner_render_svg(GVC_t *gvc, Agraph_t *g) {
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

  int rotation = gvc->rotation;
  pointf UR = gvc->bb.UR;
  pointf LL = gvc->bb.LL;

  pointf pad = {.x = 4., .y = 4.};
  if (gvc->graph_sets_pad) {
    pad = gvc->pad;
  }

  /* margin - in points - in page orientation */
  pointf margin = (pointf){0, 0}; // margin for a page of the graph - points
  if (gvc->graph_sets_margin) {
    margin = gvc->margin;
  }

  char *str;
  /* free layer strings and pointers from previous graph */
  char **layerIDs = NULL;
  int *layerlist = NULL;
  char *layerListDelims = NULL;
  char *layerDelims = NULL;
  int numLayers = 1;
  char *layer_str;
  if ((layer_str = agget(g, "layers")) != 0) {
    layerDelims = agget(g, "layersep");
    if (!layerDelims)
      layerDelims = DEFAULT_LAYERSEP;

    layerListDelims = agget(g, "layerlistsep");
    if (!layerListDelims)
      layerListDelims = DEFAULT_LAYERLISTSEP;
    char *tok;
    if ((tok = strpbrk(layerDelims,
                       layerListDelims))) { /* conflict in delimiter strings */
      agwarningf("The character \'%c\' appears in both the layersep and "
                 "layerlistsep attributes - layerlistsep ignored.\n",
                 *tok);
      layerListDelims = "";
    }

    numLayers = parse_layers(&layerIDs, layerDelims, layer_str);
    char *layerselect_str = NULL;
    if ((layerselect_str = agget(g, "layerselect")) != 0 && *layerselect_str) {
      layerlist = parse_layerselect(numLayers, layerDelims, layerListDelims,
                                    layerIDs, layerselect_str);
    }
  }

  /* page size on Linux, Mac OS X and Windows */
  output_string output = {.data_position = 0, .data_allocated = 4096};
  if (!(output.data = malloc(output.data_allocated))) {
    agerrorf("failure malloc'ing for result string");
    exit(-1);
  }

  const char *stylesheet = agget(g, "stylesheet");
  svg_begin_job(&output, stylesheet);
  // FIXME: remove hardcode
  svg_comment(&output, "Generated by graphviz version a (a)\n");

  pointf dpi = (pointf){72, 72}; // FIXME: make dpi single value
  if (GD_drawing(g)->dpi != 0) {
    dpi.x = dpi.y = GD_drawing(g)->dpi;
  }

  int rv;
  Agnode_t *n;
  char *nodename = NULL;

  boxf bb = {
      .LL = sub_pointf(LL, pad),
      .UR = add_pointf(UR, pad)}; // bb is bb of graph and padding - graph units

  pointf sz = sub_pointf(bb.UR,
                         bb.LL); // size, including padding - graph units

  /* view gives port size in graph units, unscaled or rotated
   * zoom gives scaling factor.
   * focus gives the position in the graph of the center of the port
   */
  double zoom = 1.0; /* scaling factor */

  /* determine final drawing size and scale to apply. */
  /* N.B. size given by user is not rotated by landscape mode */
  /* start with "natural" size of layout */

  if (GD_drawing(g)->size.x > 0.001 &&
      GD_drawing(g)->size.y > 0.001) { /* graph size was given by user... */
    pointf size = GD_drawing(g)->size;
    if (sz.x <= 0.001)
      sz.x = size.x;
    if (sz.y <= 0.001)
      sz.y = size.y;
    if (size.x < sz.x ||
        size.y < sz.y             /* drawing is too big (in either axis) ... */
        || (GD_drawing(g)->filled /* or ratio=filled requested and ... */
            && size.x > sz.x &&
            size.y > sz.y)) /* drawing is too small (in both axes) ... */
      zoom = fmin(size.x / sz.x, size.y / sz.y);
  }

  /* default focus, in graph units = center of bb */
  pointf focus = scale(0.5, add_pointf(LL, UR));

  /* rotate and scale bb to give default absolute size in points*/
  pointf view = scale(zoom, sz);

  /* user can override */
  if ((str = agget(g, "viewport"))) {
    nodename = gv_alloc(strlen(str) + 1);
    rv = sscanf(str, "%lf,%lf,%lf,\'%[^\']\'", &view.x, &view.y, &zoom,
                nodename);
    if (rv == 4) {
      n = agfindnode(g->root, nodename);
      if (n) {
        focus = ND_coord(n);
      }
    } else {
      rv = sscanf(str, "%lf,%lf,%lf,%[^,]%c", &view.x, &view.y, &zoom, nodename,
                  &(char){0});
      if (rv == 4) {
        n = agfindnode(g->root, nodename);
        if (n) {
          focus = ND_coord(n);
        }
      } else {
        sscanf(str, "%lf,%lf,%lf,%lf,%lf", &view.x, &view.y, &zoom, &focus.x,
               &focus.y);
      }
    }
    free(nodename);
  }

  /* unpaginated image size - in points - in graph orientation */
  pointf imageSize = view; // image size on one page of the graph - points

  /* rotate imageSize to page orientation */
  if (rotation)
    imageSize = exch_xyf(imageSize);

  /* initial window size */
  unsigned int width =
      ROUND((imageSize.x + 2 * margin.x) * dpi.x / POINTS_PER_INCH);
  unsigned int height =
      ROUND((imageSize.y + 2 * margin.y) * dpi.y / POINTS_PER_INCH);

  // FIXME: add warning about ignoring centering attribute
  // https://graphviz.org/docs/attrs/center/

  /* rotate back into graph orientation */
  if (rotation) {
    margin = exch_xyf(margin);
  }

  /* canvas area, centered if necessary */
  boxf canvasBox = {0};
  canvasBox.LL.x = margin.x;
  canvasBox.LL.y = margin.y;
  canvasBox.UR.x = margin.x + view.x;
  canvasBox.UR.y = margin.y + view.y;

  /* pageBoundingBox in device units and page orientation */
  box pageBoundingBox = {0};
  pageBoundingBox.LL.x = ROUND(canvasBox.LL.x * dpi.x / POINTS_PER_INCH);
  pageBoundingBox.LL.y = ROUND(canvasBox.LL.y * dpi.y / POINTS_PER_INCH);
  pageBoundingBox.UR.x = ROUND(canvasBox.UR.x * dpi.x / POINTS_PER_INCH);
  pageBoundingBox.UR.y = ROUND(canvasBox.UR.y * dpi.y / POINTS_PER_INCH);
  if (rotation) {
    pageBoundingBox.LL = exch_xy(pageBoundingBox.LL);
    pageBoundingBox.UR = exch_xy(pageBoundingBox.UR);
    canvasBox.LL = exch_xyf(canvasBox.LL);
    canvasBox.UR = exch_xyf(canvasBox.UR);
  }

  /* size of one page in graph units */
  double pageSize_x = view.x / zoom;
  double pageSize_y = view.y / zoom;
  boxf clip = {0};
  clip.LL.x = focus.x - pageSize_x / 2.0;
  clip.LL.y = focus.y - pageSize_y / 2.0;
  clip.UR.x = clip.LL.x + pageSize_x;
  clip.UR.y = clip.LL.y + pageSize_y;

  SafeJob safe_job = {
      .layerNum = 0,
      .dpi = dpi,
      .rotation = rotation,
      .pageBoundingBox = pageBoundingBox,
      .height = height,
      .width = width,
      .canvasBox = canvasBox,
      .zoom = zoom,
      .clip = clip,

      // from gvc
      .graph = g,
      .layerIDs = layerIDs,
      .layerDelims = layerDelims,
      .layerListDelims = layerListDelims,
      .numLayers = numLayers,
  };

  emit_graph(&output, &safe_job, g, layerlist, chkOrder(g));

  return output;
}
