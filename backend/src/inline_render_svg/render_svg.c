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

  return inner_render_svg(gvc, graph);
}
