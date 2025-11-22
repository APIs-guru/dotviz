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

void init_bb(graph_t *g) {
  node_t *n;

  for (n = agfstnode(g); n; n = agnxtnode(g, n))
    init_bb_node(g, n);
}
