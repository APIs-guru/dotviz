#include "../agrw.h"
#include "../gv_char_classes.h"
#include "const.h"
#include "geomprocs.h"
#include "gvc.h" // IWYU pragma: keep
#include "gvcint.h"
#include "util/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool Y_invert;
extern Agsym_t *G_peripheries, *G_penwidth;
extern Agsym_t *N_fontsize, *N_fontname;

/// swap data referenced by two pointers
///
/// You can think of this macro as having the following C type:
///
///   void SWAP(<t1> *a, <t1> *b);
///
/// Both `a` and `b` are expected to be pure expressions.
#define SWAP(a, b)                                                             \
  do {                                                                         \
    /* trigger a -Wcompare-distinct-pointer-types compiler warning if `a` */   \
    /* and `b` have differing types                                       */   \
    (void)((a) == (b));                                                        \
                                                                               \
    /* Swap their targets. Contemporary compilers will optimize the `memcpy`s  \
     * into direct writes for primitive types.                                 \
     */                                                                        \
    char tmp_[sizeof(*(a))];                                                   \
    memcpy(tmp_, (a), sizeof(*(a)));                                           \
    *(a) = *(b);                                                               \
    memcpy((b), tmp_, sizeof(*(b)));                                           \
  } while (0)

/// are `a` and `b` equal?
static inline bool streq(const char *a, const char *b) {
  assert(a != NULL);
  assert(b != NULL);
  return strcmp(a, b) == 0;
}

static double late_double(void *obj, attrsym_t *attr, double defaultValue,
                          double minimum) {
  if (!attr || !obj)
    return defaultValue;
  char *p = ag_xget(obj, attr);
  if (!p || p[0] == '\0')
    return defaultValue;
  char *endp;
  double rv = strtod(p, &endp);
  if (p == endp)
    return defaultValue; /* invalid double format */
  if (rv < minimum)
    return minimum;
  return rv;
}

static char *late_string(void *obj, attrsym_t *attr, char *defaultValue) {
  if (!attr || !obj)
    return defaultValue;
  return agxget(obj, attr);
}

static char *late_nnstring(void *obj, attrsym_t *attr, char *defaultValue) {
  char *rv = late_string(obj, attr, defaultValue);
  if (!rv || (rv[0] == '\0'))
    return defaultValue;
  return rv;
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

extern gvevent_key_binding_t gvevent_key_binding[];
extern const size_t gvevent_key_binding_size;
extern gvdevice_callbacks_t gvdevice_callbacks;

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

extern output_string inner_render_svg(GVC_t *gvc, Agrw_t graph);
/* Render layout in a specified format to a malloc'ed string */
output_string render_svg(GVC_t *gvc, Agrw_t graph) {
  init_bb(graph);
  init_gvc(gvc, graph);
  init_layering(gvc, graph);
  return inner_render_svg(gvc, graph);
}
