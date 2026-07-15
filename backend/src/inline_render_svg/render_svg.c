#include "const.h"
#include "types.h"
#include "util/list.h"
#include "geomprocs.h"
#include "gvcjob.h"

#include "safe_job.h"
#include "internal_render_svg.h"
#include "../output_string.h"

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

/* Split input string into tokens, with separators specified by
 * the layersep attribute. Store the values in the gvc->layerIDs array,
 * starting at index 1, and return the count.
 * Note that there is no mechanism
 * to free the memory before exit.
 */
static size_t parse_layers(char ***out_layerIDs, char *layerDelims, char *p) {
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
  size_t ntok = layer_names_size(&layerIDs) - 1;

  // if we found layers, save them for later reference
  if (ntok > 0) {
    layer_names_append(&layerIDs, NULL); // add a terminating entry
    *out_layerIDs = layer_names_detach(&layerIDs);
  }
  layer_names_free(&layerIDs);

  return ntok;
}

extern Agsym_t *G_peripheries, *G_penwidth;
extern void init_bb(graph_t *g);

output_string render_svg(Agraph_t *g) {
  // FIXME: do we need it? we suspect it is used only for clip!
  init_bb(g);

  char *p;
  /* margin - in points - in page orientation */
  pointf margin = (pointf){0, 0}; // margin for a page of the graph - points
  if ((p = agget(g, "margin"))) {
    double xf, yf;
    int i = sscanf(p, "%lf,%lf", &xf, &yf);
    if (i > 0) {
      margin.x = margin.y = xf * POINTS_PER_INCH;
      if (i > 1)
        margin.y = yf * POINTS_PER_INCH;
    }
  }

  /* pad */
  pointf pad = {.x = 4., .y = 4.};
  if ((p = agget(g, "pad"))) {
    double xf, yf;
    int i = sscanf(p, "%lf,%lf", &xf, &yf);
    if (i > 0) {
      pad.x = pad.y = xf * POINTS_PER_INCH;
      if (i > 1)
        pad.y = yf * POINTS_PER_INCH;
    }
  }

  /* rotation */
  int rotation = 0;
  if (GD_drawing(g)->landscape)
    rotation = 90;

  /* clusters have peripheries */
  G_peripheries = agfindgraphattr(g, "peripheries"); // FIXME: used only once
  G_penwidth = agfindgraphattr(g, "penwidth");       // FIXME: used only once

  /* free layer strings and pointers from previous graph */
  char **layerIDs = NULL;
  char *layerListDelims = NULL;
  char *layerDelims = NULL;
  int numLayers = 1;
  char *layer_str = agget(g, "layers");
  if (layer_str != NULL) {
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
  }

  pointf dpi = (pointf){72, 72}; // FIXME: make dpi single value
  if (GD_drawing(g)->dpi != 0) {
    dpi.x = dpi.y = GD_drawing(g)->dpi;
  }

  /* bounding box */
  boxf graph_bb = GD_bb(g);
  boxf bb = {
      .LL = sub_pointf(graph_bb.LL, pad),
      .UR = add_pointf(graph_bb.UR,
                       pad)}; // bb is bb of graph and padding - graph units

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
  pointf focus = scale(0.5, add_pointf(graph_bb.LL, graph_bb.UR));

  /* rotate and scale bb to give default absolute size in points*/
  pointf view = scale(zoom, sz);

  /* user can override */
  char *str;
  if ((str = agget(g, "viewport"))) {
    char *nodename = gv_alloc(strlen(str) + 1);
    int rv = sscanf(str, "%lf,%lf,%lf,\'%[^\']\'", &view.x, &view.y, &zoom,
                    nodename);
    if (rv == 4) {
      Agnode_t *n = agfindnode(g->root, nodename);
      if (n) {
        focus = ND_coord(n);
      }
    } else {
      rv = sscanf(str, "%lf,%lf,%lf,%[^,]%c", &view.x, &view.y, &zoom, nodename,
                  &(char){0});
      if (rv == 4) {
        Agnode_t *n = agfindnode(g->root, nodename);
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
  return emit_graph(&safe_job, g, chkOrder(g));
}
