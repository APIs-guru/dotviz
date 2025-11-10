// clang-format off
/**
 * @file
 * @brief graphics code generator
 * @ingroup common_render
 */

/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/
// clang-format off
#include "const.h"
#include "gvplugin_render.h" // IWYU pragma: keep
#include "safe_job.h"
#include "types.h"
#include "config.h"
#include <assert.h>
#include <float.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <geomprocs.h>
#include "gvcext.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "textspan.h"
#include "geom.h"
#include "textspan.h"
#include "types.h"
#include <htmltable.h>
#include <gvc.h>
#include <cdt.h>
#include <pathgeom.h>
#include <util/agxbuf.h>
#include <util/alloc.h>
#include <util/gv_ctype.h>
#include <util/gv_math.h>
#include <util/list.h>
#include <util/streq.h>
#include <util/strview.h>
#include <util/tokenize.h>
#include <util/unreachable.h>
#include <util/unused.h>
#include "utils.h"
#include "xdot.h"
#include "render_svg.h"
#include "core_svg.h"
// clang-format on

extern bool Y_invert;

extern Agsym_t *G_gradientangle, *G_peripheries, *G_penwidth;
extern Agsym_t *N_style, *N_layer, *N_comment, *N_fontname, *N_fontsize;
extern Agsym_t *E_layer, *E_dir, *E_arrowsz, *E_color, *E_fillcolor,
    *E_penwidth, *E_decorate, *E_comment, *E_style;

#define P2RECT(p, pr, sx, sy)                                                  \
  (pr[0].x = p.x - sx, pr[0].y = p.y - sy, pr[1].x = p.x + sx,                 \
   pr[1].y = p.y + sy)
#define FUZZ 3
#define EPSILON .0001

typedef struct {
  xdot_op op;
  boxf bb;
  textspan_t *span;
} exdot_op;

void *init_xdot(Agraph_t *g) {
  char *p;
  xdot *xd = NULL;

  if (!((p = agget(g, "_background")) && p[0])) {
    if (!((p = agget(g, "_draw_")) && p[0])) {
      return NULL;
    }
  }

  xd = parseXDotF(p, NULL, sizeof(exdot_op));

  if (!xd) {
    agwarningf("Could not parse \"_background\" attribute in graph %s\n",
               agnameof(g));
    agerr(AGPREV, "  \"%s\"\n", p);
  }

  return xd;
}

/* push empty graphic state for current object */
obj_state_t *push_obj_state(GVJ_t *job) {
  obj_state_t *obj = gv_alloc(sizeof(obj_state_t));

  obj_state_t *parent = obj->parent = job->obj;
  job->obj = obj;
  if (parent) {
    obj->pencolor = parent->pencolor; /* default styles to parent's style */
    obj->fillcolor = parent->fillcolor;
    obj->pen = parent->pen;
    obj->fill = parent->fill;
    obj->penwidth = parent->penwidth;
    obj->gradient_angle = parent->gradient_angle;
    obj->stopcolor = parent->stopcolor;
  } else {
    obj->pen = PEN_SOLID;
    obj->fill = FILL_NONE;
    obj->penwidth = PENWIDTH_NORMAL;
  }
  return obj;
}

/* push empty graphic state for current object */
obj_state_t child_obj_state(obj_state_t *parent) {
  obj_state_t child = {0};
  child.parent = parent;
  if (parent) {
    child.pencolor = parent->pencolor; /* default styles to parent's style */
    child.fillcolor = parent->fillcolor;
    child.pen = parent->pen;
    child.fill = parent->fill;
    child.penwidth = parent->penwidth;
    child.gradient_angle = parent->gradient_angle;
    child.stopcolor = parent->stopcolor;
  } else {
    child.pen = PEN_SOLID;
    child.fill = FILL_NONE;
    child.penwidth = PENWIDTH_NORMAL;
  }
  return child;
}

/* pop graphic state of current object */
void pop_obj_state(GVJ_t *job) {
  obj_state_t *obj = job->obj;

  assert(obj);

  free(obj->id);
  free(obj->url);
  free(obj->labelurl);
  free(obj->tailurl);
  free(obj->headurl);
  free(obj->tooltip);
  free(obj->labeltooltip);
  free(obj->tailtooltip);
  free(obj->headtooltip);
  free(obj->target);
  free(obj->labeltarget);
  free(obj->tailtarget);
  free(obj->headtarget);
  free(obj->url_map_p);
  free(obj->url_bsplinemap_p);
  free(obj->url_bsplinemap_n);

  job->obj = obj->parent;
  free(obj);
}

/* pop graphic state of current object */
void free_child_obj(obj_state_t *child) {
  assert(child);

  free(child->id);
  free(child->url);
  free(child->labelurl);
  free(child->tailurl);
  free(child->headurl);
  free(child->tooltip);
  free(child->labeltooltip);
  free(child->tailtooltip);
  free(child->headtooltip);
  free(child->target);
  free(child->labeltarget);
  free(child->tailtarget);
  free(child->headtarget);
  free(child->url_map_p);
  free(child->url_bsplinemap_p);
  free(child->url_bsplinemap_n);
}

/* Store image map data into job, substituting for node, edge, etc.
 * names.
 * @return True if an assignment was made for ID, URL, tooltip, or target
 */
bool initMapData(obj_state_t *obj, char *lbl, char *url, char *tooltip,
                 char *target, char *id, void *gobj) {
  bool assigned = false;

  if (lbl)
    obj->label = lbl;
  obj->id = strdup_and_subst_obj(id, gobj);
  if (url && url[0]) {
    obj->url = strdup_and_subst_obj(url, gobj);
  }
  assigned = true;

  if (tooltip && tooltip[0]) {
    obj->tooltip = strdup_and_subst_obj(tooltip, gobj);
    obj->explicit_tooltip = true;
    assigned = true;
  } else if (obj->label) {
    obj->tooltip = gv_strdup(obj->label);
    assigned = true;
  }

  if (target && target[0]) {
    obj->target = strdup_and_subst_obj(target, gobj);
    assigned = true;
  }
  return assigned;
}

static void layerPagePrefix(const SafeJob *safe_job, agxbuf *xb) {
  if (safe_job->layerNum > 1) {
    agxbprint(xb, "%s_", safe_job->layerIDs[safe_job->layerNum]);
  }
  if (safe_job->pagesArrayElem.x > 0 || safe_job->pagesArrayElem.y > 0) {
    agxbprint(xb, "page%d,%d_", safe_job->pagesArrayElem.x,
              safe_job->pagesArrayElem.y);
  }
}

/// Use id of root graph if any, plus kind and internal id of object
char *getObjId(const SafeJob *safe_job, void *obj, agxbuf *xb) {
  char *id;
  const graph_t *const root = safe_job->graph;
  char *gid = GD_drawing(root)->id;
  long idnum = 0;
  char *pfx = NULL;

  layerPagePrefix(safe_job, xb);

  id = agget(obj, "id");
  if (id && *id != '\0') {
    agxbput(xb, id);
    return agxbuse(xb);
  }

  if (obj != root && gid) {
    agxbprint(xb, "%s_", gid);
  }

  switch (agobjkind(obj)) {
  case AGRAPH:
    idnum = AGSEQ(obj);
    if (root == obj)
      pfx = "graph";
    else
      pfx = "clust";
    break;
  case AGNODE:
    idnum = AGSEQ((Agnode_t *)obj);
    pfx = "node";
    break;
  case AGEDGE:
    idnum = AGSEQ((Agedge_t *)obj);
    pfx = "edge";
    break;
  }

  agxbprint(xb, "%s%ld", pfx, idnum);

  return agxbuse(xb);
}

char *job_getObjId(GVJ_t *job, void *obj, agxbuf *xb) {
  SafeJob safe_job = to_safe_job(job);
  return getObjId(&safe_job, obj, xb);
}

/* Map "\n" to ^J, "\r" to ^M and "\l" to ^J.
 * Map "\\" to backslash.
 * Map "\x" to x.
 * Mapping is done in place.
 * Return input string.
 */
static char *interpretCRNL(char *ins) {
  char *rets = ins;
  char *outs = ins;
  char c;
  bool backslash_seen = false;

  while ((c = *ins++)) {
    if (backslash_seen) {
      switch (c) {
      case 'n':
      case 'l':
        *outs++ = '\n';
        break;
      case 'r':
        *outs++ = '\r';
        break;
      default:
        *outs++ = c;
        break;
      }
      backslash_seen = false;
    } else {
      if (c == '\\')
        backslash_seen = true;
      else
        *outs++ = c;
    }
  }
  *outs = '\0';
  return rets;
}

/* Tooltips are a weak form of escString, so we expect object substitution
 * and newlines to be handled. The former occurs in initMapData. Here we
 * map "\r", "\l" and "\n" to newlines. (We don't try to handle alignment
 * as in real labels.) To make things uniform when the
 * tooltip is emitted latter as visible text, we also convert HTML escape
 * sequences into UTF8. This is already occurring when tooltips are input
 * via HTML-like tables.
 */
static char *preprocessTooltip(char *s, void *gobj) {
  Agraph_t *g = agroot(gobj);
  int charset = GD_charset(g);
  char *news;
  switch (charset) {
  case CHAR_LATIN1:
    news = latin1ToUTF8(s);
    break;
  default: /* UTF8 */
    news = htmlEntityUTF8(s, g);
    break;
  }

  return interpretCRNL(news);
}

static void initObjMapData(SafeJob *safe_job, obj_state_t *obj,
                           textlabel_t *lab, void *gobj) {
  char *lbl;
  char *url = agget(gobj, "href");
  char *tooltip = agget(gobj, "tooltip");
  char *target = agget(gobj, "target");
  char *id;
  agxbuf xb = {0};

  if (lab)
    lbl = lab->text;
  else
    lbl = NULL;
  if (!url || !*url) /* try URL as an alias for href */
    url = agget(gobj, "URL");
  id = getObjId(safe_job, gobj, &xb);
  if (tooltip)
    tooltip = preprocessTooltip(tooltip, gobj);
  initMapData(obj, lbl, url, tooltip, target, id, gobj);

  free(tooltip);
  agxbfree(&xb);
}

static void job_initObjMapData(GVJ_t *job, textlabel_t *lab, void *gobj) {
  SafeJob safe_job = to_safe_job(job);
  initObjMapData(&safe_job, job->obj, lab, gobj);
}

static void map_point(GVJ_t *job, pointf pf) {
  obj_state_t *obj = job->obj;
  pointf *p;

  obj->url_map_shape = MAP_POLYGON;
  obj->url_map_n = 4;
  free(obj->url_map_p);
  obj->url_map_p = p = gv_calloc(obj->url_map_n, sizeof(pointf));
  P2RECT(pf, p, FUZZ, FUZZ);
  rect2poly(p);
}

static char **checkClusterStyle(graph_t *sg, graphviz_polygon_style_t *flagp) {
  char *style;
  char **pstyle = NULL;
  graphviz_polygon_style_t istyle = {0};

  if ((style = agget(sg, "style")) != 0 && style[0]) {
    char **pp;
    char **qp;
    char *p;
    pp = pstyle = parse_style(style);
    while ((p = *pp)) {
      if (strcmp(p, "filled") == 0) {
        istyle.filled = true;
        pp++;
      } else if (strcmp(p, "radial") == 0) {
        istyle.filled = true;
        istyle.radial = true;
        qp = pp; /* remove rounded from list passed to renderer */
        do {
          qp++;
          *(qp - 1) = *qp;
        } while (*qp);
      } else if (strcmp(p, "striped") == 0) {
        istyle.striped = true;
        qp = pp; /* remove rounded from list passed to renderer */
        do {
          qp++;
          *(qp - 1) = *qp;
        } while (*qp);
      } else if (strcmp(p, "rounded") == 0) {
        istyle.rounded = true;
        qp = pp; /* remove rounded from list passed to renderer */
        do {
          qp++;
          *(qp - 1) = *qp;
        } while (*qp);
      } else
        pp++;
    }
  }

  *flagp = istyle;
  return pstyle;
}

typedef struct {
  char *color;      /* segment color */
  double t;         ///< segment size >= 0
  bool hasFraction; /* true if color explicitly specifies its fraction */
} colorseg_t;

static void freeSeg(colorseg_t seg) { free(seg.color); }

/* Sum of segment sizes should add to 1 */
DEFINE_LIST_WITH_DTOR(colorsegs, colorseg_t, freeSeg)

/* Find semicolon in s, replace with '\0'.
 * Convert remainder to float v.
 * Return 0 if no float given
 * Return -1 on failure
 */
static double getSegLen(strview_t *s) {
  char *p = memchr(s->data, ';', s->size);
  char *endp;
  double v;

  if (!p) {
    return 0;
  }
  s->size = (size_t)(p - s->data);
  ++p;
  // Calling `strtod` on something that originated from a `strview_t` here
  // looks dangerous. But we know `s` points to something obtained from `tok`
  // with ':'. So `strtod` will run into either a ':' or a '\0' to safely stop
  // it.
  v = strtod(p, &endp);
  if (endp != p) { /* scanned something */
    if (v >= 0)
      return v;
  }
  return -1;
}

#define EPS 1E-5
#define AEQ0(x) (((x) < EPS) && ((x) > -EPS))

/* Parse string of form color;float:color;float:...:color;float:color
 * where the semicolon-floats are optional, nonnegative, sum to <= 1.
 * Store the values in an array of colorseg_t's and return the array in psegs.
 * If nseg == 0, count the number of colors.
 * If the sum of the floats does not equal 1, the remainder is equally
 * distributed to all colors without an explicit float. If no such colors exist,
 * the remainder is added to the last color. 0 => okay 1 => error without
 * message 2 => error with message 3 => warning message
 *
 * Note that psegs is only assigned to if the return value is 0 or 3.
 * Otherwise, psegs is left unchanged and the allocated memory is
 * freed before returning.
 */
static int parseSegs(const char *clrs, colorsegs_t *psegs) {
  colorsegs_t segs = {0};
  double v, left = 1;
  static atomic_flag warned;
  int rval = 0;

  for (tok_t t = tok(clrs, ":"); !tok_end(&t); tok_next(&t)) {
    strview_t color = tok_get(&t);
    if ((v = getSegLen(&color)) >= 0) {
      double del = v - left;
      if (del > 0) {
        if (!AEQ0(del) && !atomic_flag_test_and_set(&warned)) {
          agwarningf("Total size > 1 in \"%s\" color spec ", clrs);
          rval = 3;
        }
        v = left;
      }
      left -= v;
      colorseg_t s = {.t = v};
      if (v > 0)
        s.hasFraction = true;
      if (color.size > 0)
        s.color = strview_str(color);
      colorsegs_append(&segs, s);
    } else {
      if (!atomic_flag_test_and_set(&warned)) {
        agerrorf("Illegal value in \"%s\" color attribute; float expected "
                 "after ';'\n",
                 clrs);
        rval = 2;
      } else
        rval = 1;
      colorsegs_free(&segs);
      return rval;
    }
    if (AEQ0(left)) {
      left = 0;
      break;
    }
  }

  /* distribute remaining into slot with t == 0; if none, add to last */
  if (left > 0) {
    /* count zero segments */
    size_t nseg = 0;
    for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
      if (colorsegs_get(&segs, i).t <= 0)
        nseg++;
    }
    if (nseg > 0) {
      double delta = left / (double)nseg;
      for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
        colorseg_t *s = colorsegs_at(&segs, i);
        if (s->t <= 0)
          s->t = delta;
      }
    } else {
      colorsegs_back(&segs)->t += left;
    }
  }

  // terminate at the last positive segment
  while (!colorsegs_is_empty(&segs)) {
    if (colorsegs_back(&segs)->t > 0)
      break;
    colorseg_t discard = colorsegs_pop_back(&segs);
    freeSeg(discard);
  }

  *psegs = segs;
  return rval;
}

#define THIN_LINE 0.5

/* Fill an ellipse whose bounding box is given by 2 points in pf
 * with multiple wedges determined by the color spec in clrs.
 * clrs is a list of colon separated colors, with possible quantities.
 * Thin boundaries are drawn.
 *  0 => okay
 *  1 => error without message
 *  2 => error with message
 *  3 => warning message
 */
int wedgedEllipse(output_string *output, obj_state_t *obj, pointf *pf,
                  const char *clrs) {
  colorsegs_t segs;
  int rv;
  double save_penwidth = obj->penwidth;
  Ppolyline_t *pp;
  double angle0, angle1;

  rv = parseSegs(clrs, &segs);
  if (rv == 1 || rv == 2)
    return rv;
  const pointf ctr = mid_pointf(pf[0], pf[1]);
  const pointf semi = sub_pointf(pf[1], ctr);
  if (save_penwidth > THIN_LINE)
    svg_set_penwidth(obj, THIN_LINE);

  angle0 = 0;
  for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
    const colorseg_t s = colorsegs_get(&segs, i);
    if (s.color == NULL)
      break;
    if (s.t <= 0)
      continue;
    svg_set_fillcolor(obj, s.color);

    if (i + 1 == colorsegs_size(&segs))
      angle1 = 2 * M_PI;
    else
      angle1 = angle0 + 2 * M_PI * s.t;
    pp = ellipticWedge(ctr, semi.x, semi.y, angle0, angle1);
    svg_bezier(output, obj, pp->ps, pp->pn, 1);
    angle0 = angle1;
    freePath(pp);
  }

  if (save_penwidth > THIN_LINE)
    svg_set_penwidth(obj, save_penwidth);
  colorsegs_free(&segs);
  return rv;
}

/* Fill a rectangular box with vertical stripes of colors.
 * AF gives 4 corner points, with AF[0] the LL corner and the points ordered
 * CCW. clrs is a list of colon separated colors, with possible quantities. Thin
 * boundaries are drawn. 0 => okay 1 => error without message 2 => error with
 * message 3 => warning message
 */
int stripedBox(output_string *output, obj_state_t *obj, pointf *AF,
               const char *clrs, int rotate) {
  colorsegs_t segs;
  int rv;
  double xdelta;
  pointf pts[4];
  double lastx;
  double save_penwidth = obj->penwidth;

  rv = parseSegs(clrs, &segs);
  if (rv == 1 || rv == 2)
    return rv;
  if (rotate) {
    pts[0] = AF[2];
    pts[1] = AF[3];
    pts[2] = AF[0];
    pts[3] = AF[1];
  } else {
    pts[0] = AF[0];
    pts[1] = AF[1];
    pts[2] = AF[2];
    pts[3] = AF[3];
  }
  lastx = pts[1].x;
  xdelta = (pts[1].x - pts[0].x);
  pts[1].x = pts[2].x = pts[0].x;

  if (save_penwidth > THIN_LINE)
    svg_set_penwidth(obj, THIN_LINE);
  for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
    const colorseg_t s = colorsegs_get(&segs, i);
    if (s.color == NULL)
      break;
    if (s.t <= 0)
      continue;
    svg_set_fillcolor(obj, s.color);
    if (i + 1 == colorsegs_size(&segs))
      pts[1].x = pts[2].x = lastx;
    else
      pts[1].x = pts[2].x = pts[0].x + xdelta * (s.t);
    svg_polygon(output, obj, pts, 4, FILL);
    pts[0].x = pts[3].x = pts[1].x;
  }
  if (save_penwidth > THIN_LINE)
    svg_set_penwidth(obj, save_penwidth);
  colorsegs_free(&segs);
  return rv;
}

int job_stripedBox(GVJ_t *job, pointf *AF, const char *clrs, int rotate) {
  output_string output = job2output_string(job);
  int rv = stripedBox(&output, job->obj, AF, clrs, rotate);
  output_string2job(job, &output);
  return rv;
}

void emit_map_rect(obj_state_t *obj, boxf b) {
  pointf *p;

  obj->url_map_shape = MAP_POLYGON;
  obj->url_map_n = 4;
  free(obj->url_map_p);
  obj->url_map_p = p = gv_calloc(obj->url_map_n, sizeof(pointf));
  p[0] = b.LL;
  p[1] = b.UR;
  rect2poly(p);
}

static void map_label(GVJ_t *job, textlabel_t *lab) {
  obj_state_t *obj = job->obj;
  pointf *p;

  obj->url_map_shape = MAP_POLYGON;
  obj->url_map_n = 4;

  free(obj->url_map_p);
  obj->url_map_p = p = gv_calloc(obj->url_map_n, sizeof(pointf));
  P2RECT(lab->pos, p, lab->dimen.x / 2., lab->dimen.y / 2.);
  rect2poly(p);
}

#define HW 2.0 /* maximum distance away from line, in points */

/* check_control_points function checks the size of quadrilateral
 * formed by four control points
 * returns true if four points are in line (or close to line)
 * else return false
 */
static bool check_control_points(pointf *cp) {
  double dis1 = ptToLine2(cp[0], cp[3], cp[1]);
  double dis2 = ptToLine2(cp[0], cp[3], cp[2]);
  return dis1 < HW * HW && dis2 < HW * HW;
}

/* update bounding box to contain a bezier segment */
void update_bb_bz(boxf *bb, pointf *cp) {

  /* if any control point of the segment is outside the bounding box */
  if (cp[0].x > bb->UR.x || cp[0].x < bb->LL.x || cp[0].y > bb->UR.y ||
      cp[0].y < bb->LL.y || cp[1].x > bb->UR.x || cp[1].x < bb->LL.x ||
      cp[1].y > bb->UR.y || cp[1].y < bb->LL.y || cp[2].x > bb->UR.x ||
      cp[2].x < bb->LL.x || cp[2].y > bb->UR.y || cp[2].y < bb->LL.y ||
      cp[3].x > bb->UR.x || cp[3].x < bb->LL.x || cp[3].y > bb->UR.y ||
      cp[3].y < bb->LL.y) {

    /* if the segment is sufficiently refined */
    if (check_control_points(cp)) {
      int i;
      /* expand the bounding box */
      for (i = 0; i < 4; i++) {
        if (cp[i].x > bb->UR.x)
          bb->UR.x = cp[i].x;
        else if (cp[i].x < bb->LL.x)
          bb->LL.x = cp[i].x;
        if (cp[i].y > bb->UR.y)
          bb->UR.y = cp[i].y;
        else if (cp[i].y < bb->LL.y)
          bb->LL.y = cp[i].y;
      }
    } else { /* else refine the segment */
      pointf left[4], right[4];
      Bezier(cp, 0.5, left, right);
      update_bb_bz(bb, left);
      update_bb_bz(bb, right);
    }
  }
}

DEFINE_LIST(points, pointf)

static UNUSED void psmapOutput(const points_t *ps, size_t start, size_t n) {
  const pointf first = points_get(ps, start);
  fprintf(stdout, "newpath %f %f moveto\n", first.x, first.y);
  for (size_t i = start + 1; i < start + n; ++i) {
    const pointf pt = points_get(ps, i);
    fprintf(stdout, "%f %f lineto\n", pt.x, pt.y);
  }
  fprintf(stdout, "closepath stroke\n");
}

typedef struct segitem_s {
  pointf p;
  struct segitem_s *next;
} segitem_t;

#define MARK_FIRST_SEG(L) ((L)->next = (segitem_t *)1)
#define FIRST_SEG(L) ((L)->next == (segitem_t *)1)
#define INIT_SEG(P, L)                                                         \
  {                                                                            \
    (L)->next = 0;                                                             \
    (L)->p = P;                                                                \
  }

DEFINE_LIST(pbs_size, size_t)

static bool is_natural_number(const char *sstr) {
  const char *str = sstr;

  while (*str)
    if (!gv_isdigit(*str++))
      return false;
  return true;
}

static int layer_index(SafeJob *safe_job, char *str, int all) {
  int i;

  if (streq(str, "all"))
    return all;
  if (is_natural_number(str))
    return atoi(str);
  if (safe_job->layerIDs)
    for (i = 1; i <= safe_job->numLayers; i++)
      if (streq(str, safe_job->layerIDs[i]))
        return i;
  return -1;
}

static bool selectedLayer(SafeJob *safe_job, int layerNum, int numLayers,
                          char *spec) {
  int n0, n1;
  char *w0, *w1;
  char *buf_part_p = NULL, *buf_p = NULL, *cur, *part_in_p;
  bool rval = false;

  // copy `spec` so we can `strtok_r` it
  char *spec_copy = gv_strdup(spec);
  part_in_p = spec_copy;

  while (!rval &&
         (cur = strtok_r(part_in_p, safe_job->layerListDelims, &buf_part_p))) {
    w1 = w0 = strtok_r(cur, safe_job->layerDelims, &buf_p);
    if (w0)
      w1 = strtok_r(NULL, safe_job->layerDelims, &buf_p);
    if (w1 != NULL) {
      assert(w0 != NULL);
      n0 = layer_index(safe_job, w0, 0);
      n1 = layer_index(safe_job, w1, numLayers);
      if (n0 >= 0 || n1 >= 0) {
        if (n0 > n1) {
          SWAP(&n0, &n1);
        }
        rval = BETWEEN(n0, layerNum, n1);
      }
    } else if (w0 != NULL) {
      n0 = layer_index(safe_job, w0, layerNum);
      rval = (n0 == layerNum);
    } else {
      rval = false;
    }
    part_in_p = NULL;
  }
  free(spec_copy);
  return rval;
}

static bool selectedlayer(SafeJob *safe_job, char *spec) {
  return selectedLayer(safe_job, safe_job->layerNum, safe_job->numLayers, spec);
}

DEFINE_LIST(layer_names, char *)

/// Return number of physical layers to be emitted.
int numPhysicalLayers(GVJ_t *job) {
  if (job->gvc->layerlist) {
    return job->gvc->layerlist[0];
  } else
    return job->numLayers;
}

void firstlayer(GVJ_t *job, int **listp) {
  job->numLayers = job->gvc->numLayers;
  if (job->gvc->layerlist) {
    int *list = job->gvc->layerlist;
    (void)*list++;
    job->layerNum = *list++;
    *listp = list;
  } else {
    job->layerNum = 1;
    *listp = NULL;
  }
}

bool validlayer(GVJ_t *job) { return job->layerNum <= job->numLayers; }

void nextlayer(GVJ_t *job, int **listp) {
  int *list = *listp;
  if (list) {
    job->layerNum = *list++;
    *listp = list;
  } else
    job->layerNum++;
}

void firstpage(GVJ_t *job) { job->pagesArrayElem = job->pagesArrayFirst; }

bool validpage(GVJ_t *job) {
  return job->pagesArrayElem.x >= 0 &&
         job->pagesArrayElem.x < job->pagesArraySize.x &&
         job->pagesArrayElem.y >= 0 &&
         job->pagesArrayElem.y < job->pagesArraySize.y;
}

void nextpage(GVJ_t *job) {
  job->pagesArrayElem = add_point(job->pagesArrayElem, job->pagesArrayMinor);
  if (!validpage(job)) {
    if (job->pagesArrayMajor.y)
      job->pagesArrayElem.x = job->pagesArrayFirst.x;
    else
      job->pagesArrayElem.y = job->pagesArrayFirst.y;
    job->pagesArrayElem = add_point(job->pagesArrayElem, job->pagesArrayMajor);
  }
}

static pointf *copyPts(xdot_point *inpts, size_t numpts) {
  pointf *pts = gv_calloc(numpts, sizeof(pointf));
  for (size_t i = 0; i < numpts; i++) {
    pts[i].x = inpts[i].x;
    pts[i].y = inpts[i].y;
  }
  return pts;
}

static void emit_xdot(GVJ_t *job, xdot *xd) {
  int image_warn = 1;
  int angle;
  char **styles = NULL;
  int filled = FILL;

  exdot_op *op = (exdot_op *)xd->ops;
  for (size_t i = 0; i < xd->cnt; i++) {
    switch (op->op.kind) {
    case xd_filled_ellipse:
    case xd_unfilled_ellipse:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf pts[] = {{.x = op->op.u.ellipse.x - op->op.u.ellipse.w,
                         .y = op->op.u.ellipse.y - op->op.u.ellipse.h},
                        {.x = op->op.u.ellipse.x + op->op.u.ellipse.w,
                         .y = op->op.u.ellipse.y + op->op.u.ellipse.h}};
        jobsvg_ellipse(job, pts, op->op.kind == xd_filled_ellipse ? filled : 0);
      }
      break;
    case xd_filled_polygon:
    case xd_unfilled_polygon:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf *pts = copyPts(op->op.u.polygon.pts, op->op.u.polygon.cnt);
        assert(op->op.u.polygon.cnt <= INT_MAX &&
               "polygon count exceeds svg_polygon support");
        jobsvg_polygon(job, pts, op->op.u.polygon.cnt,
                       op->op.kind == xd_filled_polygon ? filled : 0);
        free(pts);
      }
      break;
    case xd_filled_bezier:
    case xd_unfilled_bezier:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf *pts = copyPts(op->op.u.bezier.pts, op->op.u.bezier.cnt);
        jobsvg_bezier(job, pts, op->op.u.bezier.cnt,
                      op->op.kind == xd_filled_bezier ? filled : 0);
        free(pts);
      }
      break;
    case xd_polyline:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf *pts = copyPts(op->op.u.polyline.pts, op->op.u.polyline.cnt);
        jobsvg_polyline(job, pts, op->op.u.polyline.cnt);
        free(pts);
      }
      break;
    case xd_text:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf pt = {.x = op->op.u.text.x, .y = op->op.u.text.y};
        jobsvg_textspan(job, pt, op->span);
      }
      break;
    case xd_fill_color:
      jobsvg_set_fillcolor(job, op->op.u.color);
      filled = FILL;
      break;
    case xd_pen_color:
      jobsvg_set_pencolor(job, op->op.u.color);
      filled = FILL;
      break;
    case xd_grad_fill_color:
      if (op->op.u.grad_color.type == xd_radial) {
        xdot_radial_grad *p = &op->op.u.grad_color.u.ring;
        char *const clr0 = p->stops[0].color;
        char *const clr1 = p->stops[1].color;
        const double frac = p->stops[1].frac;
        if (p->x1 == p->x0 && p->y1 == p->y0) {
          angle = 0;
        } else {
          angle = (int)(180 * acos((p->x0 - p->x1) / p->r0) / M_PI);
        }
        jobsvg_set_fillcolor(job, clr0);
        jobsvg_set_gradient_vals(job, clr1, angle, frac);
        filled = RGRADIENT;
      } else {
        xdot_linear_grad *p = &op->op.u.grad_color.u.ling;
        char *const clr0 = p->stops[0].color;
        char *const clr1 = p->stops[1].color;
        const double frac = p->stops[1].frac;
        angle = (int)(180 * atan2(p->y1 - p->y0, p->x1 - p->x0) / M_PI);
        jobsvg_set_fillcolor(job, clr0);
        jobsvg_set_gradient_vals(job, clr1, angle, frac);
        filled = GRADIENT;
      }
      break;
    case xd_grad_pen_color:
      agwarningf("gradient pen colors not yet supported.\n");
      break;
    case xd_font:
      /* fontsize and fontname already encoded via xdotBB */
      break;
    case xd_style:
      styles = parse_style(op->op.u.style);
      jobsvg_set_style(job, styles);
      break;
    case xd_fontchar:
      /* font characteristics already encoded via xdotBB */
      break;
    case xd_image:
      if (image_warn) {
        agwarningf("Images unsupported in \"background\" attribute\n");
        image_warn = 0;
      }
      break;
    default:
      UNREACHABLE();
    }
    op++;
  }
  if (styles)
    jobsvg_set_style(job, job->gvc->defaultlinestyle);
}

static void emit_background(GVJ_t *job, graph_t *g) {
  xdot *xd;
  char *str;

  /* if no bgcolor specified - first assume default of "white" */
  if (!((str = agget(g, "bgcolor")) && str[0])) {
    str = "white";
  }

  /* except for "transparent" on truecolor, or default "white" on (assumed)
   * white paper, paint background */
  if (!streq(str, "transparent")) {
    char *clrs[2] = {0};
    double frac;

    if ((findStopColor(str, clrs, &frac))) {
      int filled;
      graphviz_polygon_style_t istyle = {0};
      jobsvg_set_fillcolor(job, clrs[0]);
      jobsvg_set_pencolor(job, "transparent");
      checkClusterStyle(g, &istyle);
      if (clrs[1])
        jobsvg_set_gradient_vals(job, clrs[1],
                                 late_int(g, G_gradientangle, 0, 0), frac);
      else
        jobsvg_set_gradient_vals(job, DEFAULT_COLOR,
                                 late_int(g, G_gradientangle, 0, 0), frac);
      if (istyle.radial)
        filled = RGRADIENT;
      else
        filled = GRADIENT;
      jobsvg_box(job, job->clip, filled);
      free(clrs[0]);
      free(clrs[1]);
    } else {
      jobsvg_set_fillcolor(job, str);
      jobsvg_set_pencolor(job, "transparent");
      jobsvg_box(job, job->clip, FILL); /* filled */
    }
  }

  if ((xd = GD_drawing(g)->xdots))
    emit_xdot(job, xd);
}

static void setup_page(GVJ_t *job) {
  point pagesArrayElem = job->pagesArrayElem,
        pagesArraySize = job->pagesArraySize;

  if (job->rotation) {
    pagesArrayElem = exch_xy(pagesArrayElem);
    pagesArraySize = exch_xy(pagesArraySize);
  }

  /* establish current box in graph units */
  job->pageBox.LL.x = pagesArrayElem.x * job->pageSize.x - job->pad.x;
  job->pageBox.LL.y = pagesArrayElem.y * job->pageSize.y - job->pad.y;
  job->pageBox.UR.x = job->pageBox.LL.x + job->pageSize.x;
  job->pageBox.UR.y = job->pageBox.LL.y + job->pageSize.y;

  /* maximum boundingBox in device units and page orientation */
  if (job->common->viewNum == 0)
    job->boundingBox = job->pageBoundingBox;
  else
    EXPANDBB(&job->boundingBox, job->pageBoundingBox);

  job->clip.LL.x = job->focus.x +
                   job->pageSize.x * (pagesArrayElem.x - pagesArraySize.x / 2.);
  job->clip.LL.y = job->focus.y +
                   job->pageSize.y * (pagesArrayElem.y - pagesArraySize.y / 2.);
  job->clip.UR.x = job->clip.LL.x + job->pageSize.x;
  job->clip.UR.y = job->clip.LL.y + job->pageSize.y;

  /* CAUTION - job->translation was difficult to get right. */
  // Test with and without asymmetric margins, e.g: -Gmargin="1,0"
  if (job->rotation) {
    job->translation.y = -job->clip.UR.y - job->canvasBox.LL.y / job->zoom;
    job->translation.x = -job->clip.UR.x - job->canvasBox.LL.x / job->zoom;
  } else {
    /* pre unscale margins to keep them constant under scaling */
    job->translation.x = -job->clip.LL.x + job->canvasBox.LL.x / job->zoom;
    job->translation.y = -job->clip.UR.y - job->canvasBox.LL.y / job->zoom;
  }
}

static bool node_in_layer(SafeJob *safe_job, graph_t *g, node_t *n) {
  char *pn, *pe;
  edge_t *e;

  if (safe_job->numLayers <= 1)
    return true;
  pn = late_string(n, N_layer, "");
  if (selectedlayer(safe_job, pn))
    return true;
  if (pn[0])
    return false; /* Only check edges if pn = "" */
  if ((e = agfstedge(g, n)) == NULL)
    return true;
  for (e = agfstedge(g, n); e; e = agnxtedge(g, e, n)) {
    pe = late_string(e, E_layer, "");
    if (pe[0] == '\0' || selectedlayer(safe_job, pe))
      return true;
  }
  return false;
}

static bool edge_in_layer(GVJ_t *job, edge_t *e) {
  SafeJob safe_job = to_safe_job(job);
  char *pe, *pn;
  int cnt;

  if (safe_job.numLayers <= 1)
    return true;
  pe = late_string(e, E_layer, "");
  if (selectedlayer(&safe_job, pe))
    return true;
  if (pe[0])
    return false;
  for (cnt = 0; cnt < 2; cnt++) {
    pn = late_string(cnt < 1 ? agtail(e) : aghead(e), N_layer, "");
    if (pn[0] == '\0' || selectedlayer(&safe_job, pn))
      return true;
  }
  return false;
}

static bool clust_in_layer(GVJ_t *job, graph_t *sg) {
  SafeJob safe_job = to_safe_job(job);
  char *pg;
  node_t *n;

  if (safe_job.numLayers <= 1)
    return true;
  pg = late_string(sg, agattr_text(sg, AGRAPH, "layer", 0), "");
  if (selectedlayer(&safe_job, pg))
    return true;
  if (pg[0])
    return false;
  for (n = agfstnode(sg); n; n = agnxtnode(sg, n))
    if (node_in_layer(&safe_job, sg, n))
      return true;
  return false;
}

static bool node_in_box(node_t *n, boxf b) { return boxf_overlap(ND_bb(n), b); }

static char *saved_color_scheme;

static void emit_begin_node(output_string *output, SafeJob *safe_job,
                            obj_state_t *obj, node_t *n) {
  size_t nump = 0;
  pointf *p = NULL;
  pointf coord;

  obj->type = NODE_OBJTYPE;
  obj->u.n = n;
  obj->emit_state = EMIT_NDRAW;

  initObjMapData(safe_job, obj, ND_label(n), n);
  if (obj->url || obj->explicit_tooltip) {

    /* node coordinate */
    coord = ND_coord(n);

    /* When node has polygon shape and requested output supports polygons
     * we use a polygon to map the clickable region that is a:
     * circle, ellipse, polygon with n side, or point.
     * For regular rectangular shape we have use node's bounding box to map
     * clickable region
     */

    /* we have to use the node's bounding box to map clickable region
     * when requested output format is not capable of polygons.
     */
    obj->url_map_shape = MAP_RECTANGLE;
    nump = 2;
    p = gv_calloc(nump, sizeof(pointf));
    p[0].x = coord.x - ND_lw(n);
    p[0].y = coord.y - (ND_ht(n) / 2);
    p[1].x = coord.x + ND_rw(n);
    p[1].y = coord.y + (ND_ht(n) / 2);

    obj->url_map_p = p;
    obj->url_map_n = nump;
  }

  saved_color_scheme = setColorScheme(agget(n, "colorscheme"));
  svg_begin_node(output, safe_job, obj);
}

static void emit_end_node(output_string *output) {
  svg_end_node(output);

  char *color_scheme = setColorScheme(saved_color_scheme);
  free(color_scheme);
  free(saved_color_scheme);
  saved_color_scheme = NULL;
}

static void emit_node(GVJ_t *job, node_t *n) {
  output_string output = job2output_string(job);
  SafeJob safe_job = to_safe_job(job);

  if (ND_shape(n)                                 /* node has a shape */
      && node_in_layer(&safe_job, agraphof(n), n) /* and is in layer */
      && node_in_box(n, safe_job.clip)            /* and is in page/view */
      && ND_state(n) != safe_job.viewNum)         /* and not already drawn */
  {
    ND_state(n) = safe_job.viewNum; /* mark node as drawn */

    svg_comment(&output, agnameof(n));
    char *s = late_string(n, N_comment, "");
    svg_comment(&output, s);

    char *style = late_string(n, N_style, "");
    if (style[0]) {
      char **styles = parse_style(style);
      char **sp = styles;
      char *p;
      while ((p = *sp++)) {
        if (streq(p, "invis")) {
          output_string2job(job, &output);
          return;
        }
      }
    }

    obj_state_t obj = child_obj_state(job->obj);
    emit_begin_node(&output, &safe_job, &obj, n);
    ND_shape(n)->fns->codefn(&output, &safe_job, &obj, n);

    if (ND_xlabel(n) && ND_xlabel(n)->set) {
      emit_label(&output, &safe_job, &obj, EMIT_NLABEL, ND_xlabel(n));
    }

    emit_end_node(&output);
    free_child_obj(&obj);
  }
  output_string2job(job, &output);
}

/* calculate an offset vector, length d, perpendicular to line p,q */
static pointf computeoffset_p(pointf p, pointf q, double d) {
  pointf res;
  double x = p.x - q.x, y = p.y - q.y;

  /* keep d finite as line length approaches 0 */
  d /= sqrt(x * x + y * y + EPSILON);
  res.x = y * d;
  res.y = -x * d;
  return res;
}

/* calculate offset vector, length d, perpendicular to spline p,q,r,s at q&r */
static pointf computeoffset_qr(pointf p, pointf q, pointf r, pointf s,
                               double d) {
  pointf res;
  double len;
  double x = q.x - r.x, y = q.y - r.y;

  len = hypot(x, y);
  if (len < EPSILON) {
    /* control points are on top of each other
       use slope between endpoints instead */
    x = p.x - s.x, y = p.y - s.y;
    /* keep d finite as line length approaches 0 */
    len = sqrt(x * x + y * y + EPSILON);
  }
  d /= len;
  res.x = y * d;
  res.y = -x * d;
  return res;
}

static void emit_attachment(GVJ_t *job, textlabel_t *lp, splines *spl) {
  pointf sz, AF[3];
  const char *s;

  for (s = lp->text; *s; s++) {
    if (!gv_isspace(*s))
      break;
  }
  if (*s == '\0')
    return;

  sz = lp->dimen;
  AF[0] = (pointf){lp->pos.x + sz.x / 2., lp->pos.y - sz.y / 2.};
  AF[1] = (pointf){AF[0].x - sz.x, AF[0].y};
  AF[2] = dotneato_closest(spl, lp->pos);
  /* Don't use edge style to draw attachment */
  jobsvg_set_style(job, job->gvc->defaultlinestyle);
  /* Use font color to draw attachment
     - need something unambiguous in case of multicolored parallel edges
     - defaults to black for html-like labels
   */
  jobsvg_set_pencolor(job, lp->fontcolor);
  jobsvg_polyline(job, AF, 3);
}

/* edges’ colors can be multiple colors separated by ":"
 * so we compute a default pencolor with the same number of colors. */
static char *default_pencolor(agxbuf *buf, const char *pencolor,
                              const char *deflt) {
  agxbput(buf, deflt);
  for (const char *p = pencolor; *p; p++) {
    if (*p == ':')
      agxbprint(buf, ":%s", deflt);
  }
  return agxbuse(buf);
}

static double approxLen(pointf *pts) {
  double d = DIST(pts[0], pts[1]);
  d += DIST(pts[1], pts[2]);
  d += DIST(pts[2], pts[3]);
  return d;
}

/* Given B-spline bz and 0 < t < 1, split bz so that left corresponds to
 * the fraction t of the arc length. The new parts are store in left and right.
 * The caller needs to free the allocated points.
 *
 * In the current implementation, we find the Bezier that should contain t by
 * treating the control points as a polyline.
 * We then split that Bezier.
 */
static void splitBSpline(bezier *bz, double t, bezier *left, bezier *right) {
  const size_t cnt = (bz->size - 1) / 3;
  double last, len, sum;
  pointf *pts;

  if (cnt == 1) {
    left->size = 4;
    left->list = gv_calloc(4, sizeof(pointf));
    right->size = 4;
    right->list = gv_calloc(4, sizeof(pointf));
    Bezier(bz->list, t, left->list, right->list);
    return;
  }

  double *lens = gv_calloc(cnt, sizeof(double));
  sum = 0;
  pts = bz->list;
  for (size_t i = 0; i < cnt; i++) {
    lens[i] = approxLen(pts);
    sum += lens[i];
    pts += 3;
  }
  len = t * sum;
  sum = 0;
  size_t i;
  for (i = 0; i < cnt; i++) {
    sum += lens[i];
    if (sum >= len)
      break;
  }

  left->size = 3 * (i + 1) + 1;
  left->list = gv_calloc(left->size, sizeof(pointf));
  right->size = 3 * (cnt - i) + 1;
  right->list = gv_calloc(right->size, sizeof(pointf));
  size_t j;
  for (j = 0; j < left->size; j++)
    left->list[j] = bz->list[j];
  size_t k = j - 4;
  for (j = 0; j < right->size; j++)
    right->list[j] = bz->list[k++];

  last = lens[i];
  const double r = (len - (sum - last)) / last;
  Bezier(bz->list + 3 * i, r, left->list + 3 * i, right->list);

  free(lens);
}

/* Draw an edge as a sequence of colors.
 * Not sure how to handle multiple B-splines, so do a naive
 * implementation.
 * Return non-zero if color spec is incorrect
 */
static int multicolor(GVJ_t *job, edge_t *e, char **styles, const char *colors,
                      double arrowsize, double penwidth) {
  bezier bz;
  bezier bz0, bz_l, bz_r;
  int rv;
  colorsegs_t segs;
  char *endcolor = NULL;
  double left;
  int first; /* first segment with t > 0 */

  rv = parseSegs(colors, &segs);
  if (rv > 1) {
    Agraph_t *g = agraphof(agtail(e));
    agerr(AGPREV, "in edge %s%s%s\n", agnameof(agtail(e)),
          (agisdirected(g) ? " -> " : " -- "), agnameof(aghead(e)));

    if (rv == 2)
      return 1;
  } else if (rv == 1)
    return 1;

  for (size_t i = 0; i < ED_spl(e)->size; i++) {
    left = 1;
    bz = ED_spl(e)->list[i];
    first = 1;
    for (size_t j = 0; j < colorsegs_size(&segs); ++j) {
      const colorseg_t s = colorsegs_get(&segs, j);
      if (s.color == NULL)
        break;
      if (AEQ0(s.t))
        continue;
      jobsvg_set_pencolor(job, s.color);
      left -= s.t;
      endcolor = s.color;
      if (first) {
        first = 0;
        splitBSpline(&bz, s.t, &bz_l, &bz_r);
        jobsvg_bezier(job, bz_l.list, bz_l.size, 0);
        free(bz_l.list);
        if (AEQ0(left)) {
          free(bz_r.list);
          break;
        }
      } else if (AEQ0(left)) {
        jobsvg_bezier(job, bz_r.list, bz_r.size, 0);
        free(bz_r.list);
        break;
      } else {
        bz0 = bz_r;
        splitBSpline(&bz0, s.t / (left + s.t), &bz_l, &bz_r);
        free(bz0.list);
        jobsvg_bezier(job, bz_l.list, bz_l.size, 0);
        free(bz_l.list);
      }
    }
    /* arrow_gen resets the job style  (How?  FIXME)
     * If we have more splines to do, restore the old one.
     * Use local copy of penwidth to work around reset.
     */
    if (bz.sflag) {
      jobsvg_set_pencolor(job, colorsegs_front(&segs)->color);
      jobsvg_set_fillcolor(job, colorsegs_front(&segs)->color);
      arrow_gen(job, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                bz.sflag);
    }
    if (bz.eflag) {
      jobsvg_set_pencolor(job, endcolor);
      jobsvg_set_fillcolor(job, endcolor);
      arrow_gen(job, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1], arrowsize,
                penwidth, bz.eflag);
    }
    if (ED_spl(e)->size > 1 && (bz.sflag || bz.eflag) && styles)
      jobsvg_set_style(job, styles);
  }
  colorsegs_free(&segs);
  return 0;
}

static void free_stroke(stroke_t sp) { free(sp.vertices); }

typedef double (*radfunc_t)(double, double, double);

static double forfunc(double curlen, double totallen, double initwid) {
  return (1 - curlen / totallen) * initwid / 2.0;
}

static double revfunc(double curlen, double totallen, double initwid) {
  return curlen / totallen * initwid / 2.0;
}

static double nonefunc(double curlen, double totallen, double initwid) {
  (void)curlen;
  (void)totallen;

  return initwid / 2.0;
}

static double bothfunc(double curlen, double totallen, double initwid) {
  double fr = curlen / totallen;
  if (fr <= 0.5)
    return fr * initwid;
  return (1 - fr) * initwid;
}

static radfunc_t taperfun(edge_t *e) {
  char *attr;
  if (E_dir && ((attr = agxget(e, E_dir)))[0]) {
    if (streq(attr, "forward"))
      return forfunc;
    if (streq(attr, "back"))
      return revfunc;
    if (streq(attr, "both"))
      return bothfunc;
    if (streq(attr, "none"))
      return nonefunc;
  }
  return agisdirected(agraphof(aghead(e))) ? forfunc : nonefunc;
}

static void emit_edge_graphics(GVJ_t *job, edge_t *e, char **styles) {
  int cnum, numsemi = 0;
  char *color, *pencolor, *fillcolor;
  char *headcolor, *tailcolor, *lastcolor;
  char *colors = NULL;
  bezier bz;
  splines offspl, tmpspl;
  pointf pf0, pf1, pf2 = {0, 0}, pf3, *offlist, *tmplist;
  double arrowsize, numc2, penwidth = job->obj->penwidth;
  char *p;
  bool tapered = false;
  agxbuf buf = {0};

#define SEP 2.0

  char *previous_color_scheme = setColorScheme(agget(e, "colorscheme"));
  if (ED_spl(e)) {
    arrowsize = late_double(e, E_arrowsz, 1.0, 0.0);
    color = late_string(e, E_color, "");

    if (styles) {
      char **sp = styles;
      while ((p = *sp++)) {
        if (streq(p, "tapered")) {
          tapered = true;
          break;
        }
      }
    }

    /* need to know how many colors separated by ':' */
    size_t numc = 0;
    for (p = color; *p; p++) {
      if (*p == ':')
        numc++;
      else if (*p == ';')
        numsemi++;
    }

    if (numsemi && numc) {
      if (multicolor(job, e, styles, color, arrowsize, penwidth)) {
        color = DEFAULT_COLOR;
      } else
        goto done;
    }

    fillcolor = pencolor = color;
    if (ED_gui_state(e) & GUI_STATE_ACTIVE) {
      pencolor = default_pencolor(&buf, pencolor, DEFAULT_ACTIVEPENCOLOR);
      fillcolor = DEFAULT_ACTIVEFILLCOLOR;
    } else if (ED_gui_state(e) & GUI_STATE_SELECTED) {
      pencolor = default_pencolor(&buf, pencolor, DEFAULT_SELECTEDPENCOLOR);
      fillcolor = DEFAULT_SELECTEDFILLCOLOR;
    } else if (ED_gui_state(e) & GUI_STATE_DELETED) {
      pencolor = default_pencolor(&buf, pencolor, DEFAULT_DELETEDPENCOLOR);
      fillcolor = DEFAULT_DELETEDFILLCOLOR;
    } else if (ED_gui_state(e) & GUI_STATE_VISITED) {
      pencolor = default_pencolor(&buf, pencolor, DEFAULT_VISITEDPENCOLOR);
      fillcolor = DEFAULT_VISITEDFILLCOLOR;
    } else
      fillcolor = late_nnstring(e, E_fillcolor, color);
    if (pencolor != color)
      jobsvg_set_pencolor(job, pencolor);
    if (fillcolor != color)
      jobsvg_set_fillcolor(job, fillcolor);
    color = pencolor;

    if (tapered) {
      if (*color == '\0')
        color = DEFAULT_COLOR;
      if (*fillcolor == '\0')
        fillcolor = DEFAULT_COLOR;
      jobsvg_set_pencolor(job, "transparent");
      jobsvg_set_fillcolor(job, color);
      bz = ED_spl(e)->list[0];
      stroke_t stp = taper(&bz, taperfun(e), penwidth);
      assert(stp.nvertices <= INT_MAX);
      jobsvg_polygon(job, stp.vertices, stp.nvertices, 1);
      free_stroke(stp);
      jobsvg_set_pencolor(job, color);
      if (fillcolor != color)
        jobsvg_set_fillcolor(job, fillcolor);
      if (bz.sflag) {
        arrow_gen(job, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                  bz.sflag);
      }
      if (bz.eflag) {
        arrow_gen(job, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1], arrowsize,
                  penwidth, bz.eflag);
      }
    }
    /* if more than one color - then generate parallel beziers, one per color */
    else if (numc) {
      /* calculate and save offset vector spline and initialize first offset
       * spline */
      tmpspl.size = offspl.size = ED_spl(e)->size;
      offspl.list = gv_calloc(offspl.size, sizeof(bezier));
      tmpspl.list = gv_calloc(tmpspl.size, sizeof(bezier));
      numc2 = (2 + (double)numc) / 2.0;
      for (size_t i = 0; i < offspl.size; i++) {
        bz = ED_spl(e)->list[i];
        tmpspl.list[i].size = offspl.list[i].size = bz.size;
        offlist = offspl.list[i].list = gv_calloc(bz.size, sizeof(pointf));
        tmplist = tmpspl.list[i].list = gv_calloc(bz.size, sizeof(pointf));
        pf3 = bz.list[0];
        size_t j;
        for (j = 0; j < bz.size - 1; j += 3) {
          pf0 = pf3;
          pf1 = bz.list[j + 1];
          /* calculate perpendicular vectors for each bezier point */
          if (j == 0) /* first segment, no previous pf2 */
            offlist[j] = computeoffset_p(pf0, pf1, SEP);
          else /* i.e. pf2 is available from previous segment */
            offlist[j] = computeoffset_p(pf2, pf1, SEP);
          pf2 = bz.list[j + 2];
          pf3 = bz.list[j + 3];
          offlist[j + 1] = offlist[j + 2] =
              computeoffset_qr(pf0, pf1, pf2, pf3, SEP);
          /* initialize tmpspl to outermost position */
          tmplist[j].x = pf0.x - numc2 * offlist[j].x;
          tmplist[j].y = pf0.y - numc2 * offlist[j].y;
          tmplist[j + 1].x = pf1.x - numc2 * offlist[j + 1].x;
          tmplist[j + 1].y = pf1.y - numc2 * offlist[j + 1].y;
          tmplist[j + 2].x = pf2.x - numc2 * offlist[j + 2].x;
          tmplist[j + 2].y = pf2.y - numc2 * offlist[j + 2].y;
        }
        /* last segment, no next pf1 */
        offlist[j] = computeoffset_p(pf2, pf3, SEP);
        tmplist[j].x = pf3.x - numc2 * offlist[j].x;
        tmplist[j].y = pf3.y - numc2 * offlist[j].y;
      }
      lastcolor = headcolor = tailcolor = color;
      colors = gv_strdup(color);
      for (cnum = 0, color = strtok(colors, ":"); color;
           cnum++, color = strtok(0, ":")) {
        if (!color[0])
          color = DEFAULT_COLOR;
        if (color != lastcolor) {
          if (!(ED_gui_state(e) & (GUI_STATE_ACTIVE | GUI_STATE_SELECTED))) {
            jobsvg_set_pencolor(job, color);
            jobsvg_set_fillcolor(job, color);
          }
          lastcolor = color;
        }
        if (cnum == 0)
          headcolor = tailcolor = color;
        if (cnum == 1)
          tailcolor = color;
        for (size_t i = 0; i < tmpspl.size; i++) {
          tmplist = tmpspl.list[i].list;
          offlist = offspl.list[i].list;
          for (size_t j = 0; j < tmpspl.list[i].size; j++) {
            tmplist[j].x += offlist[j].x;
            tmplist[j].y += offlist[j].y;
          }
          jobsvg_bezier(job, tmplist, tmpspl.list[i].size, 0);
        }
      }
      if (bz.sflag) {
        if (color != tailcolor) {
          color = tailcolor;
          if (!(ED_gui_state(e) & (GUI_STATE_ACTIVE | GUI_STATE_SELECTED))) {
            jobsvg_set_pencolor(job, color);
            jobsvg_set_fillcolor(job, color);
          }
        }
        arrow_gen(job, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                  bz.sflag);
      }
      if (bz.eflag) {
        if (color != headcolor) {
          color = headcolor;
          if (!(ED_gui_state(e) & (GUI_STATE_ACTIVE | GUI_STATE_SELECTED))) {
            jobsvg_set_pencolor(job, color);
            jobsvg_set_fillcolor(job, color);
          }
        }
        arrow_gen(job, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1], arrowsize,
                  penwidth, bz.eflag);
      }
      free(colors);
      for (size_t i = 0; i < offspl.size; i++) {
        free(offspl.list[i].list);
        free(tmpspl.list[i].list);
      }
      free(offspl.list);
      free(tmpspl.list);
    } else {
      if (!(ED_gui_state(e) & (GUI_STATE_ACTIVE | GUI_STATE_SELECTED))) {
        if (color[0]) {
          jobsvg_set_pencolor(job, color);
          jobsvg_set_fillcolor(job, fillcolor);
        } else {
          jobsvg_set_pencolor(job, DEFAULT_COLOR);
          if (fillcolor[0])
            jobsvg_set_fillcolor(job, fillcolor);
          else
            jobsvg_set_fillcolor(job, DEFAULT_COLOR);
        }
      }
      for (size_t i = 0; i < ED_spl(e)->size; i++) {
        bz = ED_spl(e)->list[i];
        jobsvg_bezier(job, bz.list, bz.size, 0);
        if (bz.sflag) {
          arrow_gen(job, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                    bz.sflag);
        }
        if (bz.eflag) {
          arrow_gen(job, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1], arrowsize,
                    penwidth, bz.eflag);
        }
        if (ED_spl(e)->size > 1 && (bz.sflag || bz.eflag) && styles)
          jobsvg_set_style(job, styles);
      }
    }
  }

done:;
  char *color_scheme = setColorScheme(previous_color_scheme);
  free(color_scheme);
  free(previous_color_scheme);
  agxbfree(&buf);
}

static bool edge_in_box(edge_t *e, boxf b) {
  splines *spl;
  textlabel_t *lp;

  spl = ED_spl(e);
  if (spl && boxf_overlap(spl->bb, b))
    return true;

  lp = ED_label(e);
  if (lp && overlap_label(lp, b))
    return true;

  lp = ED_xlabel(e);
  if (lp && lp->set && overlap_label(lp, b))
    return true;

  return false;
}

static void emit_begin_edge(GVJ_t *job, edge_t *e, char **styles) {
  char *s;
  textlabel_t *lab = NULL, *tlab = NULL, *hlab = NULL;
  char *dflt_url = NULL;
  char *dflt_target = NULL;
  double penwidth;

  obj_state_t *obj = push_obj_state(job);
  obj->type = EDGE_OBJTYPE;
  obj->u.e = e;
  obj->emit_state = EMIT_EDRAW;
  if (ED_label(e) && !ED_label(e)->html && mapbool(agget(e, "labelaligned")))
    obj->labeledgealigned = true;

  /* We handle the edge style and penwidth here because the width
   * is needed below for calculating polygonal image maps
   */
  if (styles && ED_spl(e))
    jobsvg_set_style(job, styles);

  if (E_penwidth && (s = agxget(e, E_penwidth)) && s[0]) {
    penwidth = late_double(e, E_penwidth, 1.0, 0.0);
    svg_set_penwidth(obj, penwidth);
  }

  if ((lab = ED_label(e)))
    obj->label = lab->text;
  obj->taillabel = obj->headlabel = obj->xlabel = obj->label;
  if ((tlab = ED_xlabel(e)))
    obj->xlabel = tlab->text;
  if ((tlab = ED_tail_label(e)))
    obj->taillabel = tlab->text;
  if ((hlab = ED_head_label(e)))
    obj->headlabel = hlab->text;

  agxbuf xb = {0};

  s = job_getObjId(job, e, &xb);
  obj->id = strdup_and_subst_obj(s, e);
  agxbfree(&xb);

  if (((s = agget(e, "href")) && s[0]) || ((s = agget(e, "URL")) && s[0]))
    dflt_url = strdup_and_subst_obj(s, e);
  if (((s = agget(e, "edgehref")) && s[0]) ||
      ((s = agget(e, "edgeURL")) && s[0]))
    obj->url = strdup_and_subst_obj(s, e);
  else if (dflt_url)
    obj->url = gv_strdup(dflt_url);
  if (((s = agget(e, "labelhref")) && s[0]) ||
      ((s = agget(e, "labelURL")) && s[0]))
    obj->labelurl = strdup_and_subst_obj(s, e);
  else if (dflt_url)
    obj->labelurl = gv_strdup(dflt_url);
  if (((s = agget(e, "tailhref")) && s[0]) ||
      ((s = agget(e, "tailURL")) && s[0])) {
    obj->tailurl = strdup_and_subst_obj(s, e);
    obj->explicit_tailurl = true;
  } else if (dflt_url)
    obj->tailurl = gv_strdup(dflt_url);
  if (((s = agget(e, "headhref")) && s[0]) ||
      ((s = agget(e, "headURL")) && s[0])) {
    obj->headurl = strdup_and_subst_obj(s, e);
    obj->explicit_headurl = true;
  } else if (dflt_url)
    obj->headurl = gv_strdup(dflt_url);

  if ((s = agget(e, "target")) && s[0])
    dflt_target = strdup_and_subst_obj(s, e);
  if ((s = agget(e, "edgetarget")) && s[0]) {
    obj->explicit_edgetarget = true;
    obj->target = strdup_and_subst_obj(s, e);
  } else if (dflt_target)
    obj->target = gv_strdup(dflt_target);
  if ((s = agget(e, "labeltarget")) && s[0])
    obj->labeltarget = strdup_and_subst_obj(s, e);
  else if (dflt_target)
    obj->labeltarget = gv_strdup(dflt_target);
  if ((s = agget(e, "tailtarget")) && s[0]) {
    obj->tailtarget = strdup_and_subst_obj(s, e);
    obj->explicit_tailtarget = true;
  } else if (dflt_target)
    obj->tailtarget = gv_strdup(dflt_target);
  if ((s = agget(e, "headtarget")) && s[0]) {
    obj->explicit_headtarget = true;
    obj->headtarget = strdup_and_subst_obj(s, e);
  } else if (dflt_target)
    obj->headtarget = gv_strdup(dflt_target);

  if (((s = agget(e, "tooltip")) && s[0]) ||
      ((s = agget(e, "edgetooltip")) && s[0])) {
    char *tooltip = preprocessTooltip(s, e);
    obj->tooltip = strdup_and_subst_obj(tooltip, e);
    free(tooltip);
    obj->explicit_tooltip = true;
  } else if (obj->label)
    obj->tooltip = gv_strdup(obj->label);

  if ((s = agget(e, "labeltooltip")) && s[0]) {
    char *tooltip = preprocessTooltip(s, e);
    obj->labeltooltip = strdup_and_subst_obj(tooltip, e);
    free(tooltip);
    obj->explicit_labeltooltip = true;
  } else if (obj->label)
    obj->labeltooltip = gv_strdup(obj->label);

  if ((s = agget(e, "tailtooltip")) && s[0]) {
    char *tooltip = preprocessTooltip(s, e);
    obj->tailtooltip = strdup_and_subst_obj(tooltip, e);
    free(tooltip);
    obj->explicit_tailtooltip = true;
  } else if (obj->taillabel)
    obj->tailtooltip = gv_strdup(obj->taillabel);

  if ((s = agget(e, "headtooltip")) && s[0]) {
    char *tooltip = preprocessTooltip(s, e);
    obj->headtooltip = strdup_and_subst_obj(tooltip, e);
    free(tooltip);
    obj->explicit_headtooltip = true;
  } else if (obj->headlabel)
    obj->headtooltip = gv_strdup(obj->headlabel);

  free(dflt_url);
  free(dflt_target);

  jobsvg_begin_edge(job);
  if (obj->url || obj->explicit_tooltip)
    jobsvg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
}

static void emit_edge_label(GVJ_t *job, textlabel_t *lbl, emit_state_t lkind,
                            int explicit, char *url, char *tooltip,
                            char *target, char *id, splines *spl) {
  emit_state_t old_emit_state;
  char *newid;
  agxbuf xb = {0};
  char *type;

  if (lbl == NULL || !lbl->set)
    return;
  if (id) { /* non-NULL if needed */
    switch (lkind) {
    case EMIT_ELABEL:
      type = "label";
      break;
    case EMIT_HLABEL:
      type = "headlabel";
      break;
    case EMIT_TLABEL:
      type = "taillabel";
      break;
    default:
      UNREACHABLE();
    }
    agxbprint(&xb, "%s-%s", id, type);
    newid = agxbuse(&xb);
  } else
    newid = NULL;
  old_emit_state = job->obj->emit_state;
  job->obj->emit_state = lkind;
  if (url || explicit) {
    map_label(job, lbl);
    jobsvg_begin_anchor(job, url, tooltip, target, newid);
  }
  job_emit_label(job, lkind, lbl);
  if (spl)
    emit_attachment(job, lbl, spl);
  if (url || explicit) {
    jobsvg_end_anchor(job);
  }
  agxbfree(&xb);
  job->obj->emit_state = old_emit_state;
}

/* Common logic for setting hot spots at the beginning and end of
 * an edge.
 * If we are given a value (url, tooltip, target) explicitly set for
 * the head/tail, we use that.
 * Otherwise, if we are given a value explicitly set for the edge,
 * we use that.
 * Otherwise, we use whatever the argument value is.
 * We also note whether or not the tooltip was explicitly set.
 * If the url is non-NULL or the tooltip was explicit, we set
 * a hot spot around point p.
 */
static void nodeIntersect(GVJ_t *job, pointf p, bool explicit_iurl, char *iurl,
                          bool explicit_itooltip) {
  obj_state_t *obj = job->obj;
  char *url;
  bool explicit;

  if (explicit_iurl)
    url = iurl;
  else
    url = obj->url;
  if (explicit_itooltip) {
    explicit = true;
  } else if (obj->explicit_tooltip) {
    explicit = true;
  } else {
    explicit = false;
  }

  if (url || explicit) {
    map_point(job, p);
  }
}

static void emit_end_edge(GVJ_t *job) {
  obj_state_t *obj = job->obj;
  edge_t *e = obj->u.e;

  if (obj->url || obj->explicit_tooltip) {
    jobsvg_end_anchor(job);
    if (obj->url_bsplinemap_poly_n) {
      for (size_t nump = obj->url_bsplinemap_n[0], i = 1;
           i < obj->url_bsplinemap_poly_n; i++) {
        /* additional polygon maps around remaining bezier pieces */
        obj->url_map_n = obj->url_bsplinemap_n[i];
        obj->url_map_p = &(obj->url_bsplinemap_p[nump]);
        jobsvg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
        jobsvg_end_anchor(job);
        nump += obj->url_bsplinemap_n[i];
      }
    }
  }
  obj->url_map_n = 0; /* null out copy so that it doesn't get freed twice */
  obj->url_map_p = NULL;

  if (ED_spl(e)) {
    pointf p;
    bezier bz;

    /* process intersection with tail node */
    bz = ED_spl(e)->list[0];
    if (bz.sflag) /* Arrow at start of splines */
      p = bz.sp;
    else /* No arrow at start of splines */
      p = bz.list[0];
    nodeIntersect(job, p, obj->explicit_tailurl != 0, obj->tailurl,
                  obj->explicit_tailtooltip != 0);

    /* process intersection with head node */
    bz = ED_spl(e)->list[ED_spl(e)->size - 1];
    if (bz.eflag) /* Arrow at end of splines */
      p = bz.ep;
    else /* No arrow at end of splines */
      p = bz.list[bz.size - 1];
    nodeIntersect(job, p, obj->explicit_headurl != 0, obj->headurl,
                  obj->explicit_headtooltip != 0);
  }

  emit_edge_label(job, ED_label(e), EMIT_ELABEL, obj->explicit_labeltooltip,
                  obj->labelurl, obj->labeltooltip, obj->labeltarget, obj->id,
                  ((mapbool(late_string(e, E_decorate, "false")) && ED_spl(e))
                       ? ED_spl(e)
                       : 0));
  emit_edge_label(job, ED_xlabel(e), EMIT_ELABEL, obj->explicit_labeltooltip,
                  obj->labelurl, obj->labeltooltip, obj->labeltarget, obj->id,
                  ((mapbool(late_string(e, E_decorate, "false")) && ED_spl(e))
                       ? ED_spl(e)
                       : 0));
  emit_edge_label(job, ED_head_label(e), EMIT_HLABEL, obj->explicit_headtooltip,
                  obj->headurl, obj->headtooltip, obj->headtarget, obj->id, 0);
  emit_edge_label(job, ED_tail_label(e), EMIT_TLABEL, obj->explicit_tailtooltip,
                  obj->tailurl, obj->tailtooltip, obj->tailtarget, obj->id, 0);

  jobsvg_end_edge(job);
  pop_obj_state(job);
}

static void emit_edge(GVJ_t *job, edge_t *e) {
  char *s;
  char *style;
  char **styles = NULL;
  char **sp;
  char *p;

  if (edge_in_box(e, job->clip) && edge_in_layer(job, e)) {

    agxbuf edge = {0};
    agxbput(&edge, agnameof(agtail(e)));
    if (agisdirected(agraphof(aghead(e))))
      agxbput(&edge, "->");
    else
      agxbput(&edge, "--");
    agxbput(&edge, agnameof(aghead(e)));
    jobsvg_comment(job, agxbuse(&edge));
    agxbfree(&edge);

    s = late_string(e, E_comment, "");
    jobsvg_comment(job, s);

    style = late_string(e, E_style, "");
    /* We shortcircuit drawing an invisible edge because the arrowhead
     * code resets the style to solid, and most of the code generators
     * (except PostScript) won't honor a previous style of invis.
     */
    if (style[0]) {
      styles = parse_style(style);
      sp = styles;
      while ((p = *sp++)) {
        if (streq(p, "invis"))
          return;
      }
    }

    emit_begin_edge(job, e, styles);
    emit_edge_graphics(job, e, styles);
    emit_end_edge(job);
  }
}

static void emit_view(GVJ_t *job, graph_t *g, int flags) {
  GVC_t *gvc = job->gvc;
  node_t *n;
  edge_t *e;

  gvc->common.viewNum++;
  /* when drawing, lay clusters down before nodes and edges */
  emit_clusters(job, g, flags);
  if (flags & EMIT_SORTED) {
    /* output all nodes, then all edges */
    for (n = agfstnode(g); n; n = agnxtnode(g, n))
      emit_node(job, n);
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
      for (e = agfstout(g, n); e; e = agnxtout(g, e))
        emit_edge(job, e);
    }
  } else if (flags & EMIT_EDGE_SORTED) {
    /* output all edges, then all nodes */
    for (n = agfstnode(g); n; n = agnxtnode(g, n))
      for (e = agfstout(g, n); e; e = agnxtout(g, e))
        emit_edge(job, e);
    for (n = agfstnode(g); n; n = agnxtnode(g, n))
      emit_node(job, n);
  } else {
    /* output in breadth first graph walk order */
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
      emit_node(job, n);
      for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
        emit_node(job, aghead(e));
        emit_edge(job, e);
      }
    }
  }
}

void emit_begin_graph(GVJ_t *job, graph_t *g) {
  obj_state_t *obj;

  obj = push_obj_state(job);
  obj->type = ROOTGRAPH_OBJTYPE;
  obj->u.g = g;
  obj->emit_state = EMIT_GDRAW;

  job_initObjMapData(job, GD_label(g), g);

  jobsvg_begin_graph(job);
}

void emit_end_graph(GVJ_t *job) {
  jobsvg_end_graph(job);
  pop_obj_state(job);
}

#define NotFirstPage(j)                                                        \
  (((j)->layerNum > 1) || ((j)->pagesArrayElem.x > 0) ||                       \
   ((j)->pagesArrayElem.x > 0))

void emit_page(GVJ_t *job, graph_t *g) {
  obj_state_t *obj = job->obj;
  int flags = job->flags;
  size_t nump = 0;
  textlabel_t *lab;
  pointf *p = NULL;
  char *saveid;
  agxbuf xb = {0};

  /* For the first page, we can use the values generated in emit_begin_graph.
   * For multiple pages, we need to generate a new id.
   */
  bool obj_id_needs_restore = false;
  if (NotFirstPage(job)) {
    saveid = obj->id;
    {
      SafeJob safe_job = to_safe_job(job);
      layerPagePrefix(&safe_job, &xb);
    }
    agxbput(&xb, saveid == NULL ? "layer" : saveid);
    obj->id = agxbuse(&xb);
    obj_id_needs_restore = true;
  } else
    saveid = NULL;

  char *previous_color_scheme = setColorScheme(agget(g, "colorscheme"));
  setup_page(job);
  jobsvg_begin_page(job);
  jobsvg_set_pencolor(job, DEFAULT_COLOR);
  jobsvg_set_fillcolor(job, DEFAULT_FILL);
  if (obj->url || obj->explicit_tooltip) {
    obj->url_map_p = p;
    obj->url_map_n = nump;
  }
  if ((lab = GD_label(g))) {
    /* do graph label on every page and rely on clipping to show it on the right
     * one(s) */
    obj->label = lab->text;
  }
  /* If EMIT_CLUSTERS_LAST is set, we assume any URL or tooltip
   * attached to the root graph is emitted either in begin_page
   * or end_page of renderer.
   */
  if (obj->url || obj->explicit_tooltip) {
    emit_map_rect(obj, job->clip);
    jobsvg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
  }
  emit_background(job, g);
  if (GD_label(g))
    job_emit_label(job, EMIT_GLABEL, GD_label(g));
  if (obj->url || obj->explicit_tooltip)
    jobsvg_end_anchor(job);
  emit_view(job, g, flags);
  jobsvg_end_page(job);
  if (obj_id_needs_restore) {
    obj->id = saveid;
  }
  agxbfree(&xb);

  char *color_scheme = setColorScheme(previous_color_scheme);
  free(color_scheme);
  free(previous_color_scheme);
}

static Dict_t *strings;
static Dtdisc_t stringdict = {
    .link = -1, // link - allocate separate holder objects
    .freef = free,
};

bool emit_once(char *str) {
  if (strings == 0)
    strings = dtopen(&stringdict, Dtoset);
  if (!dtsearch(strings, str)) {
    dtinsert(strings, gv_strdup(str));
    return true;
  }
  return false;
}

void emit_once_reset(void) {
  if (strings) {
    dtclose(strings);
    strings = 0;
  }
}

static void emit_begin_cluster(GVJ_t *job, Agraph_t *sg) {
  obj_state_t *obj;

  obj = push_obj_state(job);
  obj->type = CLUSTER_OBJTYPE;
  obj->u.sg = sg;
  obj->emit_state = EMIT_CDRAW;

  job_initObjMapData(job, GD_label(sg), sg);

  jobsvg_begin_cluster(job);
}

static void emit_end_cluster(GVJ_t *job) {
  jobsvg_end_cluster(job);
  pop_obj_state(job);
}

void emit_clusters(GVJ_t *job, Agraph_t *g, int flags) {
  int doPerim, c, filled;
  pointf AF[4];
  char *color, *fillcolor, *pencolor, **style, *s;
  graph_t *sg;
  textlabel_t *lab;
  int doAnchor;
  double penwidth;

  for (c = 1; c <= GD_n_cluster(g); c++) {
    sg = GD_clust(g)[c];
    if (!clust_in_layer(job, sg))
      continue;
    emit_begin_cluster(job, sg);
    obj_state_t *obj = job->obj;
    doAnchor = obj->url || obj->explicit_tooltip;
    char *previous_color_scheme = setColorScheme(agget(sg, "colorscheme"));
    if (doAnchor) {
      emit_map_rect(obj, GD_bb(sg));
      jobsvg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
    }
    filled = 0;
    graphviz_polygon_style_t istyle = {0};
    if ((style = checkClusterStyle(sg, &istyle))) {
      jobsvg_set_style(job, style);
      if (istyle.filled)
        filled = FILL;
    }
    fillcolor = pencolor = 0;

    if (GD_gui_state(sg) & GUI_STATE_ACTIVE) {
      pencolor = DEFAULT_ACTIVEPENCOLOR;
      fillcolor = DEFAULT_ACTIVEFILLCOLOR;
      filled = FILL;
    } else if (GD_gui_state(sg) & GUI_STATE_SELECTED) {
      pencolor = DEFAULT_SELECTEDPENCOLOR;
      fillcolor = DEFAULT_SELECTEDFILLCOLOR;
      filled = FILL;
    } else if (GD_gui_state(sg) & GUI_STATE_DELETED) {
      pencolor = DEFAULT_DELETEDPENCOLOR;
      fillcolor = DEFAULT_DELETEDFILLCOLOR;
      filled = FILL;
    } else if (GD_gui_state(sg) & GUI_STATE_VISITED) {
      pencolor = DEFAULT_VISITEDPENCOLOR;
      fillcolor = DEFAULT_VISITEDFILLCOLOR;
      filled = FILL;
    } else {
      if ((color = agget(sg, "color")) != 0 && color[0])
        fillcolor = pencolor = color;
      if ((color = agget(sg, "pencolor")) != 0 && color[0])
        pencolor = color;
      if ((color = agget(sg, "fillcolor")) != 0 && color[0])
        fillcolor = color;
      /* bgcolor is supported for backward compatibility
         if fill is set, fillcolor trumps bgcolor, so
         don't bother checking.
         if gradient is set fillcolor trumps bgcolor
       */
      if ((filled == 0 || !fillcolor) && (color = agget(sg, "bgcolor")) != 0 &&
          color[0]) {
        fillcolor = color;
        filled = FILL;
      }
    }
    if (!pencolor)
      pencolor = DEFAULT_COLOR;
    if (!fillcolor)
      fillcolor = DEFAULT_FILL;
    char *clrs[2] = {0};
    if (filled != 0) {
      double frac;
      if (findStopColor(fillcolor, clrs, &frac)) {
        jobsvg_set_fillcolor(job, clrs[0]);
        if (clrs[1])
          jobsvg_set_gradient_vals(job, clrs[1],
                                   late_int(sg, G_gradientangle, 0, 0), frac);
        else
          jobsvg_set_gradient_vals(job, DEFAULT_COLOR,
                                   late_int(sg, G_gradientangle, 0, 0), frac);
        if (istyle.radial)
          filled = RGRADIENT;
        else
          filled = GRADIENT;
      } else
        jobsvg_set_fillcolor(job, fillcolor);
    }

    if (G_penwidth && ((s = ag_xget(sg, G_penwidth)) && s[0])) {
      penwidth = late_double(sg, G_penwidth, 1.0, 0.0);
      svg_set_penwidth(obj, penwidth);
    }

    if (istyle.rounded) {
      if ((doPerim = late_int(sg, G_peripheries, 1, 0)) || filled != 0) {
        AF[0] = GD_bb(sg).LL;
        AF[2] = GD_bb(sg).UR;
        AF[1].x = AF[2].x;
        AF[1].y = AF[0].y;
        AF[3].x = AF[0].x;
        AF[3].y = AF[2].y;
        if (doPerim)
          jobsvg_set_pencolor(job, pencolor);
        else
          jobsvg_set_pencolor(job, "transparent");
        job_round_corners(job, AF, 4, istyle, filled);
      }
    } else if (istyle.striped) {
      AF[0] = GD_bb(sg).LL;
      AF[2] = GD_bb(sg).UR;
      AF[1].x = AF[2].x;
      AF[1].y = AF[0].y;
      AF[3].x = AF[0].x;
      AF[3].y = AF[2].y;
      if (late_int(sg, G_peripheries, 1, 0) == 0)
        jobsvg_set_pencolor(job, "transparent");
      else
        jobsvg_set_pencolor(job, pencolor);
      if (job_stripedBox(job, AF, fillcolor, 0) > 1)
        agerr(AGPREV, "in cluster %s\n", agnameof(sg));
      jobsvg_box(job, GD_bb(sg), 0);
    } else {
      if (late_int(sg, G_peripheries, 1, 0)) {
        jobsvg_set_pencolor(job, pencolor);
        jobsvg_box(job, GD_bb(sg), filled);
      } else if (filled != 0) {
        jobsvg_set_pencolor(job, "transparent");
        jobsvg_box(job, GD_bb(sg), filled);
      }
    }

    free(clrs[0]);
    free(clrs[1]);
    if ((lab = GD_label(sg)))
      job_emit_label(job, EMIT_CLABEL, lab);

    if (doAnchor) {
      jobsvg_end_anchor(job);
    }

    emit_end_cluster(job);
    /* when drawing, lay down clusters before sub_clusters */
    emit_clusters(job, sg, flags);

    char *color_scheme = setColorScheme(previous_color_scheme);
    free(color_scheme);
    free(previous_color_scheme);
  }
}

static bool is_style_delim(int c) {
  switch (c) {
  case '(':
  case ')':
  case ',':
  case '\0':
    return true;
  default:
    return false;
  }
}

#define SID 1

/// Recognized token, returned from `style_token`
///
/// The token content fields, `.start` and `.size` are only populated with
/// useful values when `.type` is `SID`, an identifier.
typedef struct {
  int type;          ///< Token category
  const char *start; ///< Beginning of the token content
  size_t size;       ///< Number of bytes in the token content
} token_t;

static token_t style_token(char **s) {
  char *p = *s;
  int token;

  while (gv_isspace(*p) || *p == ',')
    p++;
  const char *start = p;
  switch (*p) {
  case '\0':
    token = 0;
    break;
  case '(':
  case ')':
    token = *p++;
    break;
  default:
    token = SID;
    while (!is_style_delim(*p)) {
      p++;
    }
  }
  *s = p;
  assert(start <= p);
  size_t size = (size_t)(p - start);
  return (token_t){.type = token, .start = start, .size = size};
}

#define FUNLIMIT 64

/* This is one of the worst internal designs in graphviz.
 * The use of '\0' characters within strings seems cute but it
 * makes all of the standard functions useless if not dangerous.
 * Plus the function uses static memory for both the array and
 * the character buffer. One hopes all of the values are used
 * before the function is called again.
 */
char **parse_style(char *s) {
  static char *parse[FUNLIMIT];
  size_t parse_offsets[sizeof(parse) / sizeof(parse[0])];
  size_t fun = 0;
  bool in_parens = false;
  char *p;
  static agxbuf ps_xb;

  p = s;
  while (true) {
    token_t c = style_token(&p);
    if (c.type == 0) {
      break;
    }
    switch (c.type) {
    case '(':
      if (in_parens) {
        agerrorf("nesting not allowed in style: %s\n", s);
        parse[0] = NULL;
        return parse;
      }
      in_parens = true;
      break;

    case ')':
      if (!in_parens) {
        agerrorf("unmatched ')' in style: %s\n", s);
        parse[0] = NULL;
        return parse;
      }
      in_parens = false;
      break;

    default:
      if (!in_parens) {
        if (fun == FUNLIMIT - 1) {
          agwarningf("truncating style '%s'\n", s);
          parse[fun] = NULL;
          return parse;
        }
        agxbputc(&ps_xb, '\0'); /* terminate previous */
        parse_offsets[fun++] = agxblen(&ps_xb);
      }
      agxbput_n(&ps_xb, c.start, c.size);
      agxbputc(&ps_xb, '\0');
    }
  }

  if (in_parens) {
    agerrorf("unmatched '(' in style: %s\n", s);
    parse[0] = NULL;
    return parse;
  }

  char *base = agxbuse(&ps_xb); // add final '\0' to buffer

  // construct list of style strings
  for (size_t i = 0; i < fun; ++i) {
    parse[i] = base + parse_offsets[i];
  }
  parse[fun] = NULL;

  return parse;
}

/* Check for colon in colorlist. If one exists, and not the first
 * character, store the characters before the colon in clrs[0] and
 * the characters after the colon (and before the next or end-of-string)
 * in clrs[1]. If there are no characters after the first colon, clrs[1]
 * is NULL. Return TRUE.
 * If there is no non-trivial string before a first colon, set clrs[0] to
 * NULL and return FALSE.
 *
 * Note that memory for clrs must be freed by calling function.
 */
bool findStopColor(const char *colorlist, char *clrs[2], double *frac) {
  colorsegs_t segs = {0};
  int rv;
  clrs[0] = NULL;
  clrs[1] = NULL;

  rv = parseSegs(colorlist, &segs);
  if (rv || colorsegs_size(&segs) < 2 ||
      colorsegs_front(&segs)->color == NULL) {
    colorsegs_free(&segs);
    return false;
  }

  if (colorsegs_size(&segs) > 2)
    agwarningf(
        "More than 2 colors specified for a gradient - ignoring remaining\n");

  clrs[0] = gv_strdup(colorsegs_front(&segs)->color);
  if (colorsegs_get(&segs, 1).color) {
    clrs[1] = gv_strdup(colorsegs_get(&segs, 1).color);
  }

  if (colorsegs_front(&segs)->hasFraction)
    *frac = colorsegs_front(&segs)->t;
  else if (colorsegs_get(&segs, 1).hasFraction)
    *frac = 1 - colorsegs_get(&segs, 1).t;
  else
    *frac = 0;

  colorsegs_free(&segs);
  return true;
}

void emit_graph(GVJ_t *job, graph_t *g) {
  node_t *n;
  char *s;
  int *lp;

  /* device dpi is now known */
  job->scale.x = job->zoom * job->dpi.x / POINTS_PER_INCH;
  job->scale.y = job->zoom * job->dpi.y / POINTS_PER_INCH;

  job->devscale.x = job->dpi.x / POINTS_PER_INCH;
  job->devscale.y = job->dpi.y / POINTS_PER_INCH;
  job->devscale.y *= -1;

  /* compute current view in graph units */
  if (job->rotation) {
    job->view.y = job->width / job->scale.y;
    job->view.x = job->height / job->scale.x;
  } else {
    job->view.x = job->width / job->scale.x;
    job->view.y = job->height / job->scale.y;
  }

  s = late_string(g, agattr_text(g, AGRAPH, "comment", 0), "");
  jobsvg_comment(job, s);

  job->layerNum = 0;
  emit_begin_graph(job, g);

  /* reset node state */
  for (n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;
  /* iterate layers */
  for (firstlayer(job, &lp); validlayer(job); nextlayer(job, &lp)) {
    if (numPhysicalLayers(job) > 1) {
      jobsvg_begin_layer(job, job->gvc->layerIDs[job->layerNum]);
    }

    /* iterate pages */
    for (firstpage(job); validpage(job); nextpage(job))
      emit_page(job, g);

    if (numPhysicalLayers(job) > 1)
      jobsvg_end_layer(job);
  }
  emit_end_graph(job);
}
