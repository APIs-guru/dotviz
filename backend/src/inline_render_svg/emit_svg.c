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
#include "types.h"
#include "config.h"
#include <assert.h>
#include <float.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <geomprocs.h>
#include "gvcext.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvcproc.h"
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
#include <util/debug.h>
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

static char *defaultlinestyle[3] = {"solid\0", "setlinewidth\0001\0", 0};

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

/* Store image map data into job, substituting for node, edge, etc.
 * names.
 * @return True if an assignment was made for ID, URL, tooltip, or target
 */
bool initMapData(GVJ_t *job, char *lbl, char *url, char *tooltip, char *target,
                 char *id, void *gobj) {
  obj_state_t *obj = job->obj;
  int flags = job->flags;
  bool assigned = false;

  if ((flags & GVRENDER_DOES_LABELS) && lbl)
    obj->label = lbl;
  if (flags & GVRENDER_DOES_MAPS) {
    obj->id = strdup_and_subst_obj(id, gobj);
    if (url && url[0]) {
      obj->url = strdup_and_subst_obj(url, gobj);
    }
    assigned = true;
  }
  if (flags & GVRENDER_DOES_TOOLTIPS) {
    if (tooltip && tooltip[0]) {
      obj->tooltip = strdup_and_subst_obj(tooltip, gobj);
      obj->explicit_tooltip = true;
      assigned = true;
    } else if (obj->label) {
      obj->tooltip = gv_strdup(obj->label);
      assigned = true;
    }
  }
  if ((flags & GVRENDER_DOES_TARGETS) && target && target[0]) {
    obj->target = strdup_and_subst_obj(target, gobj);
    assigned = true;
  }
  return assigned;
}

static void layerPagePrefix(GVJ_t *job, agxbuf *xb) {
  if (job->layerNum > 1) {
    agxbprint(xb, "%s_", job->gvc->layerIDs[job->layerNum]);
  }
  if (job->pagesArrayElem.x > 0 || job->pagesArrayElem.y > 0) {
    agxbprint(xb, "page%d,%d_", job->pagesArrayElem.x, job->pagesArrayElem.y);
  }
}

/// Use id of root graph if any, plus kind and internal id of object
char *getObjId(GVJ_t *job, void *obj, agxbuf *xb) {
  char *id;
  graph_t *root = job->gvc->g;
  char *gid = GD_drawing(root)->id;
  long idnum = 0;
  char *pfx = NULL;

  layerPagePrefix(job, xb);

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

void initObjMapData(GVJ_t *job, textlabel_t *lab, void *gobj) {
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
  id = getObjId(job, gobj, &xb);
  if (tooltip)
    tooltip = preprocessTooltip(tooltip, gobj);
  initMapData(job, lbl, url, tooltip, target, id, gobj);

  free(tooltip);
  agxbfree(&xb);
}

static void map_point(GVJ_t *job, pointf pf) {
  obj_state_t *obj = job->obj;
  int flags = job->flags;
  pointf *p;

  if (flags & (GVRENDER_DOES_MAPS | GVRENDER_DOES_TOOLTIPS)) {
    if (flags & GVRENDER_DOES_MAP_RECTANGLE) {
      obj->url_map_shape = MAP_RECTANGLE;
      obj->url_map_n = 2;
    } else {
      obj->url_map_shape = MAP_POLYGON;
      obj->url_map_n = 4;
    }
    free(obj->url_map_p);
    obj->url_map_p = p = gv_calloc(obj->url_map_n, sizeof(pointf));
    P2RECT(pf, p, FUZZ, FUZZ);
    if (!(flags & GVRENDER_DOES_TRANSFORM))
      gvrender_ptf_A(job, p, p, 2);
    if (!(flags & GVRENDER_DOES_MAP_RECTANGLE))
      rect2poly(p);
  }
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
extern void svg_bezier(GVJ_t *job, pointf *A, size_t n, int filled);
int wedgedEllipse(GVJ_t *job, pointf *pf, const char *clrs) {
  colorsegs_t segs;
  int rv;
  double save_penwidth = job->obj->penwidth;
  Ppolyline_t *pp;
  double angle0, angle1;

  rv = parseSegs(clrs, &segs);
  if (rv == 1 || rv == 2)
    return rv;
  const pointf ctr = mid_pointf(pf[0], pf[1]);
  const pointf semi = sub_pointf(pf[1], ctr);
  if (save_penwidth > THIN_LINE)
    gvrender_set_penwidth(job, THIN_LINE);

  angle0 = 0;
  for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
    const colorseg_t s = colorsegs_get(&segs, i);
    if (s.color == NULL)
      break;
    if (s.t <= 0)
      continue;
    gvrender_set_fillcolor(job, s.color);

    if (i + 1 == colorsegs_size(&segs))
      angle1 = 2 * M_PI;
    else
      angle1 = angle0 + 2 * M_PI * s.t;
    pp = ellipticWedge(ctr, semi.x, semi.y, angle0, angle1);
    svg_bezier(job, pp->ps, pp->pn, 1);
    angle0 = angle1;
    freePath(pp);
  }

  if (save_penwidth > THIN_LINE)
    gvrender_set_penwidth(job, save_penwidth);
  colorsegs_free(&segs);
  return rv;
}

/* Fill a rectangular box with vertical stripes of colors.
 * AF gives 4 corner points, with AF[0] the LL corner and the points ordered
 * CCW. clrs is a list of colon separated colors, with possible quantities. Thin
 * boundaries are drawn. 0 => okay 1 => error without message 2 => error with
 * message 3 => warning message
 */
extern void svg_polygon(GVJ_t *job, pointf *A, size_t n, int filled);
int stripedBox(GVJ_t *job, pointf *AF, const char *clrs, int rotate) {
  colorsegs_t segs;
  int rv;
  double xdelta;
  pointf pts[4];
  double lastx;
  double save_penwidth = job->obj->penwidth;

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
    gvrender_set_penwidth(job, THIN_LINE);
  for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
    const colorseg_t s = colorsegs_get(&segs, i);
    if (s.color == NULL)
      break;
    if (s.t <= 0)
      continue;
    gvrender_set_fillcolor(job, s.color);
    if (i + 1 == colorsegs_size(&segs))
      pts[1].x = pts[2].x = lastx;
    else
      pts[1].x = pts[2].x = pts[0].x + xdelta * (s.t);
    svg_polygon(job, pts, 4, FILL);
    pts[0].x = pts[3].x = pts[1].x;
  }
  if (save_penwidth > THIN_LINE)
    gvrender_set_penwidth(job, save_penwidth);
  colorsegs_free(&segs);
  return rv;
}

void emit_map_rect(GVJ_t *job, boxf b) {
  obj_state_t *obj = job->obj;
  int flags = job->flags;
  pointf *p;

  if (flags & (GVRENDER_DOES_MAPS | GVRENDER_DOES_TOOLTIPS)) {
    if (flags & GVRENDER_DOES_MAP_RECTANGLE) {
      obj->url_map_shape = MAP_RECTANGLE;
      obj->url_map_n = 2;
    } else {
      obj->url_map_shape = MAP_POLYGON;
      obj->url_map_n = 4;
    }
    free(obj->url_map_p);
    obj->url_map_p = p = gv_calloc(obj->url_map_n, sizeof(pointf));
    p[0] = b.LL;
    p[1] = b.UR;
    if (!(flags & GVRENDER_DOES_TRANSFORM))
      gvrender_ptf_A(job, p, p, 2);
    if (!(flags & GVRENDER_DOES_MAP_RECTANGLE))
      rect2poly(p);
  }
}

static void map_label(GVJ_t *job, textlabel_t *lab) {
  obj_state_t *obj = job->obj;
  int flags = job->flags;
  pointf *p;

  if (flags & (GVRENDER_DOES_MAPS | GVRENDER_DOES_TOOLTIPS)) {
    if (flags & GVRENDER_DOES_MAP_RECTANGLE) {
      obj->url_map_shape = MAP_RECTANGLE;
      obj->url_map_n = 2;
    } else {
      obj->url_map_shape = MAP_POLYGON;
      obj->url_map_n = 4;
    }
    free(obj->url_map_p);
    obj->url_map_p = p = gv_calloc(obj->url_map_n, sizeof(pointf));
    P2RECT(lab->pos, p, lab->dimen.x / 2., lab->dimen.y / 2.);
    if (!(flags & GVRENDER_DOES_TRANSFORM))
      gvrender_ptf_A(job, p, p, 2);
    if (!(flags & GVRENDER_DOES_MAP_RECTANGLE))
      rect2poly(p);
  }
}

/* isRect function returns true when polygon has
 * regular rectangular shape. Rectangle is regular when
 * it is not skewed and distorted and orientation is almost zero
 */
static bool isRect(polygon_t *p) {
  return p->sides == 4 && fabs(fmod(p->orientation, 90)) < 0.5 &&
         is_exactly_zero(p->distortion) && is_exactly_zero(p->skew);
}

/*
 * isFilled function returns true if filled style has been set for node 'n'
 * otherwise returns false. it accepts pointer to node_t as an argument
 */
static bool isFilled(node_t *n) {
  char *style, *p, **pp;
  bool r = false;
  style = late_nnstring(n, N_style, "");
  if (style[0]) {
    pp = parse_style(style);
    while ((p = *pp)) {
      if (strcmp(p, "filled") == 0)
        r = true;
      pp++;
    }
  }
  return r;
}

/* pEllipse function returns 'np' points from the circumference
 * of ellipse described by radii 'a' and 'b'.
 * Assumes 'np' is greater than zero.
 * 'np' should be at least 4 to sample polygon from ellipse
 */
static pointf *pEllipse(double a, double b, size_t np) {
  double theta = 0.0;
  double deltheta = 2 * M_PI / (double)np;

  pointf *ps = gv_calloc(np, sizeof(pointf));
  for (size_t i = 0; i < np; i++) {
    ps[i].x = a * cos(theta);
    ps[i].y = b * sin(theta);
    theta += deltheta;
  }
  return ps;
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

static segitem_t *appendSeg(pointf p, segitem_t *lp) {
  segitem_t *s = gv_alloc(sizeof(segitem_t));
  INIT_SEG(p, s);
  lp->next = s;
  return s;
}

DEFINE_LIST(pbs_size, size_t)

/* Output the polygon determined by the n points in p1, followed
 * by the n points in p2 in reverse order. Assumes n <= 50.
 */
static void map_bspline_poly(points_t *pbs_p, pbs_size_t *pbs_n, size_t n,
                             pointf *p1, pointf *p2) {
  pbs_size_append(pbs_n, 2 * n);

  const UNUSED size_t nump = points_size(pbs_p);
  for (size_t i = 0; i < n; i++) {
    points_append(pbs_p, p1[i]);
  }
  for (size_t i = 0; i < n; i++) {
    points_append(pbs_p, p2[n - i - 1]);
  }
#if defined(DEBUG) && DEBUG == 2
  psmapOutput(pbs_p, nump, 2 * n);
#endif
}

/* Approximate Bezier by line segments. If the four points are
 * almost colinear, as determined by check_control_points, we store
 * the segment cp[0]-cp[3]. Otherwise we split the Bezier into 2 and recurse.
 * Since 2 contiguous segments share an endpoint, we actually store
 * the segments as a list of points.
 * New points are appended to the list given by lp. The tail of the
 * list is returned.
 */
static segitem_t *approx_bezier(pointf *cp, segitem_t *lp) {
  pointf left[4], right[4];

  if (check_control_points(cp)) {
    if (FIRST_SEG(lp))
      INIT_SEG(cp[0], lp);
    lp = appendSeg(cp[3], lp);
  } else {
    Bezier(cp, 0.5, left, right);
    lp = approx_bezier(left, lp);
    lp = approx_bezier(right, lp);
  }
  return lp;
}

/* Return the angle of the bisector between the two rays
 * pp-cp and cp-np. The bisector returned is always to the
 * left of pp-cp-np.
 */
static double bisect(pointf pp, pointf cp, pointf np) {
  double ang, theta, phi;
  theta = atan2(np.y - cp.y, np.x - cp.x);
  phi = atan2(pp.y - cp.y, pp.x - cp.x);
  ang = theta - phi;
  if (ang > 0)
    ang -= 2 * M_PI;

  return phi + ang / 2.0;
}

/* Determine polygon points related to 2 segments prv-cur and cur-nxt.
 * The points lie on the bisector of the 2 segments, passing through cur,
 * and distance w2 from cur. The points are stored in p1 and p2.
 * If p1 is NULL, we use the normal to cur-nxt.
 * If p2 is NULL, we use the normal to prv-cur.
 * Assume at least one of prv or nxt is non-NULL.
 */
static void mkSegPts(segitem_t *prv, segitem_t *cur, segitem_t *nxt, pointf *p1,
                     pointf *p2, double w2) {
  pointf cp, pp, np;
  double theta, delx, dely;
  pointf p;

  cp = cur->p;
  /* if prv or nxt are NULL, use the one given to create a collinear
   * prv or nxt. This could be more efficiently done with special case code,
   * but this way is more uniform.
   */
  if (prv) {
    pp = prv->p;
    if (nxt)
      np = nxt->p;
    else {
      np.x = 2 * cp.x - pp.x;
      np.y = 2 * cp.y - pp.y;
    }
  } else {
    np = nxt->p;
    pp.x = 2 * cp.x - np.x;
    pp.y = 2 * cp.y - np.y;
  }
  theta = bisect(pp, cp, np);
  delx = w2 * cos(theta);
  dely = w2 * sin(theta);
  p.x = cp.x + delx;
  p.y = cp.y + dely;
  *p1 = p;
  p.x = cp.x - delx;
  p.y = cp.y - dely;
  *p2 = p;
}

/* Construct and output a closed polygon approximating the input
 * B-spline bp. We do this by first approximating bp by a sequence
 * of line segments. We then use the sequence of segments to determine
 * the polygon.
 * In cmapx, polygons are limited to 100 points, so we output polygons
 * in chunks of 100.
 */
static void map_output_bspline(points_t *pbs, pbs_size_t *pbs_n, bezier *bp,
                               double w2) {
  segitem_t *segl = gv_alloc(sizeof(segitem_t));
  segitem_t *segp = segl;
  segitem_t *segprev;
  segitem_t *segnext;
  pointf pts[4], pt1[50], pt2[50];

  MARK_FIRST_SEG(segl);
  const size_t nc = (bp->size - 1) / 3; // nc is number of bezier curves
  for (size_t j = 0; j < nc; j++) {
    for (size_t k = 0; k < 4; k++) {
      pts[k] = bp->list[3 * j + k];
    }
    segp = approx_bezier(pts, segp);
  }

  segp = segl;
  segprev = 0;
  size_t cnt = 0;
  while (segp) {
    segnext = segp->next;
    mkSegPts(segprev, segp, segnext, pt1 + cnt, pt2 + cnt, w2);
    cnt++;
    if (segnext == NULL || cnt == 50) {
      map_bspline_poly(pbs, pbs_n, cnt, pt1, pt2);
      pt1[0] = pt1[cnt - 1];
      pt2[0] = pt2[cnt - 1];
      cnt = 1;
    }
    segprev = segp;
    segp = segnext;
  }

  /* free segl */
  while (segl) {
    segp = segl->next;
    free(segl);
    segl = segp;
  }
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
    int cnt = *list++;
    if (cnt > 1 && !(job->flags & GVDEVICE_DOES_LAYERS)) {
      agwarningf("layers not supported in %s output\n", job->output_langname);
      list[1] = job->numLayers + 1; /* only one layer printed */
    }
    job->layerNum = *list++;
    *listp = list;
  } else {
    if (job->numLayers > 1 && !(job->flags & GVDEVICE_DOES_LAYERS)) {
      agwarningf("layers not supported in %s output\n", job->output_langname);
      job->numLayers = 1;
    }
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

static bool write_edge_test(Agraph_t *g, Agedge_t *e) {
  Agraph_t *sg;
  int c;

  for (c = 1; c <= GD_n_cluster(g); c++) {
    sg = GD_clust(g)[c];
    if (agcontains(sg, e))
      return false;
  }
  return true;
}

static bool write_node_test(Agraph_t *g, Agnode_t *n) {
  Agraph_t *sg;
  int c;

  for (c = 1; c <= GD_n_cluster(g); c++) {
    sg = GD_clust(g)[c];
    if (agcontains(sg, n))
      return false;
  }
  return true;
}

static pointf *copyPts(xdot_point *inpts, size_t numpts) {
  pointf *pts = gv_calloc(numpts, sizeof(pointf));
  for (size_t i = 0; i < numpts; i++) {
    pts[i].x = inpts[i].x;
    pts[i].y = inpts[i].y;
  }
  return pts;
}

extern void svg_ellipse(GVJ_t *job, pointf *A, int filled);
extern void svg_polyline(GVJ_t *job, pointf *A, size_t n);
extern void svg_textspan(GVJ_t *job, pointf p, textspan_t *span);
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
        svg_ellipse(job, pts, op->op.kind == xd_filled_ellipse ? filled : 0);
      }
      break;
    case xd_filled_polygon:
    case xd_unfilled_polygon:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf *pts = copyPts(op->op.u.polygon.pts, op->op.u.polygon.cnt);
        assert(op->op.u.polygon.cnt <= INT_MAX &&
               "polygon count exceeds svg_polygon support");
        svg_polygon(job, pts, op->op.u.polygon.cnt,
                    op->op.kind == xd_filled_polygon ? filled : 0);
        free(pts);
      }
      break;
    case xd_filled_bezier:
    case xd_unfilled_bezier:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf *pts = copyPts(op->op.u.bezier.pts, op->op.u.bezier.cnt);
        svg_bezier(job, pts, op->op.u.bezier.cnt,
                   op->op.kind == xd_filled_bezier ? filled : 0);
        free(pts);
      }
      break;
    case xd_polyline:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf *pts = copyPts(op->op.u.polyline.pts, op->op.u.polyline.cnt);
        svg_polyline(job, pts, op->op.u.polyline.cnt);
        free(pts);
      }
      break;
    case xd_text:
      if (boxf_overlap(op->bb, job->clip)) {
        pointf pt = {.x = op->op.u.text.x, .y = op->op.u.text.y};
        svg_textspan(job, pt, op->span);
      }
      break;
    case xd_fill_color:
      gvrender_set_fillcolor(job, op->op.u.color);
      filled = FILL;
      break;
    case xd_pen_color:
      gvrender_set_pencolor(job, op->op.u.color);
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
        gvrender_set_fillcolor(job, clr0);
        gvrender_set_gradient_vals(job, clr1, angle, frac);
        filled = RGRADIENT;
      } else {
        xdot_linear_grad *p = &op->op.u.grad_color.u.ling;
        char *const clr0 = p->stops[0].color;
        char *const clr1 = p->stops[1].color;
        const double frac = p->stops[1].frac;
        angle = (int)(180 * atan2(p->y1 - p->y0, p->x1 - p->x0) / M_PI);
        gvrender_set_fillcolor(job, clr0);
        gvrender_set_gradient_vals(job, clr1, angle, frac);
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
      gvrender_set_style(job, styles);
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
    gvrender_set_style(job, job->gvc->defaultlinestyle);
}

extern void svg_box(GVJ_t *job, boxf B, int filled);
static void emit_background(GVJ_t *job, graph_t *g) {
  xdot *xd;
  char *str;
  int dfltColor;

  /* if no bgcolor specified - first assume default of "white" */
  if (!((str = agget(g, "bgcolor")) && str[0])) {
    str = "white";
    dfltColor = 1;
  } else
    dfltColor = 0;

  /* if device has no truecolor support, change "transparent" to "white" */
  if (!(job->flags & GVDEVICE_DOES_TRUECOLOR) && (streq(str, "transparent"))) {
    str = "white";
    dfltColor = 1;
  }

  /* except for "transparent" on truecolor, or default "white" on (assumed)
   * white paper, paint background */
  if (!(((job->flags & GVDEVICE_DOES_TRUECOLOR) && streq(str, "transparent")) ||
        ((job->flags & GVRENDER_NO_WHITE_BG) && dfltColor))) {
    char *clrs[2] = {0};
    double frac;

    if ((findStopColor(str, clrs, &frac))) {
      int filled;
      graphviz_polygon_style_t istyle = {0};
      gvrender_set_fillcolor(job, clrs[0]);
      gvrender_set_pencolor(job, "transparent");
      checkClusterStyle(g, &istyle);
      if (clrs[1])
        gvrender_set_gradient_vals(job, clrs[1],
                                   late_int(g, G_gradientangle, 0, 0), frac);
      else
        gvrender_set_gradient_vals(job, DEFAULT_COLOR,
                                   late_int(g, G_gradientangle, 0, 0), frac);
      if (istyle.radial)
        filled = RGRADIENT;
      else
        filled = GRADIENT;
      svg_box(job, job->clip, filled);
      free(clrs[0]);
      free(clrs[1]);
    } else {
      gvrender_set_fillcolor(job, str);
      gvrender_set_pencolor(job, "transparent");
      svg_box(job, job->clip, FILL); /* filled */
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

  if (job->flags & GVDEVICE_EVENTS) {
    job->clip.LL.x = job->focus.x - job->view.x / 2.;
    job->clip.LL.y = job->focus.y - job->view.y / 2.;
    job->clip.UR.x = job->focus.x + job->view.x / 2.;
    job->clip.UR.y = job->focus.y + job->view.y / 2.;
  } else {
    job->clip.LL.x = job->focus.x + job->pageSize.x * (pagesArrayElem.x -
                                                       pagesArraySize.x / 2.);
    job->clip.LL.y = job->focus.y + job->pageSize.y * (pagesArrayElem.y -
                                                       pagesArraySize.y / 2.);
    job->clip.UR.x = job->clip.LL.x + job->pageSize.x;
    job->clip.UR.y = job->clip.LL.y + job->pageSize.y;
  }

  /* CAUTION - job->translation was difficult to get right. */
  // Test with and without asymmetric margins, e.g: -Gmargin="1,0"
  if (job->rotation) {
    job->translation.y = -job->clip.UR.y - job->canvasBox.LL.y / job->zoom;
    if ((job->flags & GVRENDER_Y_GOES_DOWN) || Y_invert)
      job->translation.x = -job->clip.UR.x - job->canvasBox.LL.x / job->zoom;
    else
      job->translation.x = -job->clip.LL.x + job->canvasBox.LL.x / job->zoom;
  } else {
    /* pre unscale margins to keep them constant under scaling */
    job->translation.x = -job->clip.LL.x + job->canvasBox.LL.x / job->zoom;
    if ((job->flags & GVRENDER_Y_GOES_DOWN) || Y_invert)
      job->translation.y = -job->clip.UR.y - job->canvasBox.LL.y / job->zoom;
    else
      job->translation.y = -job->clip.LL.y + job->canvasBox.LL.y / job->zoom;
  }
}

static bool node_in_layer(GVJ_t *job, graph_t *g, node_t *n) {
  char *pn, *pe;
  edge_t *e;

  if (job->numLayers <= 1)
    return true;
  pn = late_string(n, N_layer, "");
  if (selectedlayer(job, pn))
    return true;
  if (pn[0])
    return false; /* Only check edges if pn = "" */
  if ((e = agfstedge(g, n)) == NULL)
    return true;
  for (e = agfstedge(g, n); e; e = agnxtedge(g, e, n)) {
    pe = late_string(e, E_layer, "");
    if (pe[0] == '\0' || selectedlayer(job, pe))
      return true;
  }
  return false;
}

static bool edge_in_layer(GVJ_t *job, edge_t *e) {
  char *pe, *pn;
  int cnt;

  if (job->numLayers <= 1)
    return true;
  pe = late_string(e, E_layer, "");
  if (selectedlayer(job, pe))
    return true;
  if (pe[0])
    return false;
  for (cnt = 0; cnt < 2; cnt++) {
    pn = late_string(cnt < 1 ? agtail(e) : aghead(e), N_layer, "");
    if (pn[0] == '\0' || selectedlayer(job, pn))
      return true;
  }
  return false;
}

static bool clust_in_layer(GVJ_t *job, graph_t *sg) {
  char *pg;
  node_t *n;

  if (job->numLayers <= 1)
    return true;
  pg = late_string(sg, agattr_text(sg, AGRAPH, "layer", 0), "");
  if (selectedlayer(job, pg))
    return true;
  if (pg[0])
    return false;
  for (n = agfstnode(sg); n; n = agnxtnode(sg, n))
    if (node_in_layer(job, sg, n))
      return true;
  return false;
}

static bool node_in_box(node_t *n, boxf b) { return boxf_overlap(ND_bb(n), b); }

static char *saved_color_scheme;

extern void svg_begin_node(GVJ_t *job);
static void emit_begin_node(GVJ_t *job, node_t *n) {
  obj_state_t *obj;
  int flags = job->flags;
  int shape;
  size_t nump = 0;
  polygon_t *poly = NULL;
  pointf *vertices, *p = NULL;
  pointf coord;
  char *s;

  obj = push_obj_state(job);
  obj->type = NODE_OBJTYPE;
  obj->u.n = n;
  obj->emit_state = EMIT_NDRAW;

  if (flags & GVRENDER_DOES_Z) {
    if (GD_odim(agraphof(n)) >= 3)
      obj->z = POINTS(ND_pos(n)[2]);
    else
      obj->z = 0.0;
  }
  initObjMapData(job, ND_label(n), n);
  if ((flags & (GVRENDER_DOES_MAPS | GVRENDER_DOES_TOOLTIPS)) &&
      (obj->url || obj->explicit_tooltip)) {

    /* checking shape of node */
    shape = shapeOf(n);
    /* node coordinate */
    coord = ND_coord(n);
    /* checking if filled style has been set for node */
    bool filled = isFilled(n);

    bool is_rect = false;
    if (shape == SH_POLY || shape == SH_POINT) {
      poly = ND_shape_info(n);

      /* checking if polygon is regular rectangle */
      if (isRect(poly) && (poly->peripheries || filled))
        is_rect = true;
    }

    /* When node has polygon shape and requested output supports polygons
     * we use a polygon to map the clickable region that is a:
     * circle, ellipse, polygon with n side, or point.
     * For regular rectangular shape we have use node's bounding box to map
     * clickable region
     */
    if (poly && !is_rect && (flags & GVRENDER_DOES_MAP_POLYGON)) {

      const size_t sides = poly->sides < 3 ? 1 : poly->sides;
      const size_t peripheries = poly->peripheries < 2 ? 1 : poly->peripheries;

      vertices = poly->vertices;

      int nump_int = 0;
      if ((s = agget(n, "samplepoints")))
        nump_int = atoi(s);
      /* We want at least 4 points. For server-side maps, at most 100
       * points are allowed. To simplify things to fit with the 120 points
       * used for skewed ellipses, we set the bound at 60.
       */
      nump = (nump_int < 4 || nump_int > 60) ? DFLT_SAMPLE : (size_t)nump_int;
      /* use bounding box of text label or node image for mapping
       * when polygon has no peripheries and node is not filled
       */
      if (poly->peripheries == 0 && !filled) {
        obj->url_map_shape = MAP_RECTANGLE;
        nump = 2;
        p = gv_calloc(nump, sizeof(pointf));
        P2RECT(coord, p, ND_lw(n), ND_ht(n) / 2.0);
      }
      /* circle or ellipse */
      else if (poly->sides < 3 && is_exactly_zero(poly->skew) &&
               is_exactly_zero(poly->distortion)) {
        if (poly->regular) {
          obj->url_map_shape = MAP_CIRCLE;
          nump = 2; /* center of circle and top right corner of bb */
          p = gv_calloc(nump, sizeof(pointf));
          p[0].x = coord.x;
          p[0].y = coord.y;
          /* even vertices contain LL corner of bb */
          /* odd vertices contain UR corner of bb */
          p[1].x = coord.x + vertices[2 * peripheries - 1].x;
          p[1].y = coord.y + vertices[2 * peripheries - 1].y;
        } else { /* ellipse is treated as polygon */
          obj->url_map_shape = MAP_POLYGON;
          p = pEllipse(vertices[2 * peripheries - 1].x,
                       vertices[2 * peripheries - 1].y, nump);
          for (size_t i = 0; i < nump; i++) {
            p[i].x += coord.x;
            p[i].y += coord.y;
          }
        }
      }
      /* all other polygonal shape */
      else {
        assert(peripheries >= 1);
        size_t offset = (peripheries - 1) * poly->sides;
        obj->url_map_shape = MAP_POLYGON;
        /* distorted or skewed ellipses and circles are polygons with 120
         * sides. For mapping we convert them into polygon with sample sides
         */
        if (poly->sides >= nump) {
          size_t delta = poly->sides / nump;
          p = gv_calloc(nump, sizeof(pointf));
          for (size_t i = 0, j = 0; j < nump; i += delta, j++) {
            p[j].x = coord.x + vertices[i + offset].x;
            p[j].y = coord.y + vertices[i + offset].y;
          }
        } else {
          nump = sides;
          p = gv_calloc(nump, sizeof(pointf));
          for (size_t i = 0; i < nump; i++) {
            p[i].x = coord.x + vertices[i + offset].x;
            p[i].y = coord.y + vertices[i + offset].y;
          }
        }
      }
    } else {
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
    }
    if (!(flags & GVRENDER_DOES_TRANSFORM))
      gvrender_ptf_A(job, p, p, nump);
    obj->url_map_p = p;
    obj->url_map_n = nump;
  }

  saved_color_scheme = setColorScheme(agget(n, "colorscheme"));
  svg_begin_node(job);
}

extern void svg_end_node(GVJ_t *job);
static void emit_end_node(GVJ_t *job) {
  svg_end_node(job);

  char *color_scheme = setColorScheme(saved_color_scheme);
  free(color_scheme);
  free(saved_color_scheme);
  saved_color_scheme = NULL;

  pop_obj_state(job);
}

extern void svg_comment(GVJ_t *job, char *str);
static void emit_node(GVJ_t *job, node_t *n) {
  GVC_t *gvc = job->gvc;
  char *s;
  char *style;
  char **styles = NULL;
  char **sp;
  char *p;

  if (ND_shape(n)                            /* node has a shape */
      && node_in_layer(job, agraphof(n), n)  /* and is in layer */
      && node_in_box(n, job->clip)           /* and is in page/view */
      && ND_state(n) != gvc->common.viewNum) /* and not already drawn */
  {
    ND_state(n) = gvc->common.viewNum; /* mark node as drawn */

    svg_comment(job, agnameof(n));
    s = late_string(n, N_comment, "");
    svg_comment(job, s);

    style = late_string(n, N_style, "");
    if (style[0]) {
      styles = parse_style(style);
      sp = styles;
      while ((p = *sp++)) {
        if (streq(p, "invis"))
          return;
      }
    }

    emit_begin_node(job, n);
    ND_shape(n)->fns->codefn(job, n);
    if (ND_xlabel(n) && ND_xlabel(n)->set)
      emit_label(job, EMIT_NLABEL, ND_xlabel(n));
    emit_end_node(job);
  }
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
  gvrender_set_style(job, job->gvc->defaultlinestyle);
  /* Use font color to draw attachment
     - need something unambiguous in case of multicolored parallel edges
     - defaults to black for html-like labels
   */
  gvrender_set_pencolor(job, lp->fontcolor);
  svg_polyline(job, AF, 3);
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
      gvrender_set_pencolor(job, s.color);
      left -= s.t;
      endcolor = s.color;
      if (first) {
        first = 0;
        splitBSpline(&bz, s.t, &bz_l, &bz_r);
        svg_bezier(job, bz_l.list, bz_l.size, 0);
        free(bz_l.list);
        if (AEQ0(left)) {
          free(bz_r.list);
          break;
        }
      } else if (AEQ0(left)) {
        svg_bezier(job, bz_r.list, bz_r.size, 0);
        free(bz_r.list);
        break;
      } else {
        bz0 = bz_r;
        splitBSpline(&bz0, s.t / (left + s.t), &bz_l, &bz_r);
        free(bz0.list);
        svg_bezier(job, bz_l.list, bz_l.size, 0);
        free(bz_l.list);
      }
    }
    /* arrow_gen resets the job style  (How?  FIXME)
     * If we have more splines to do, restore the old one.
     * Use local copy of penwidth to work around reset.
     */
    if (bz.sflag) {
      gvrender_set_pencolor(job, colorsegs_front(&segs)->color);
      gvrender_set_fillcolor(job, colorsegs_front(&segs)->color);
      arrow_gen(job, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                bz.sflag);
    }
    if (bz.eflag) {
      gvrender_set_pencolor(job, endcolor);
      gvrender_set_fillcolor(job, endcolor);
      arrow_gen(job, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1], arrowsize,
                penwidth, bz.eflag);
    }
    if (ED_spl(e)->size > 1 && (bz.sflag || bz.eflag) && styles)
      gvrender_set_style(job, styles);
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
      gvrender_set_pencolor(job, pencolor);
    if (fillcolor != color)
      gvrender_set_fillcolor(job, fillcolor);
    color = pencolor;

    if (tapered) {
      if (*color == '\0')
        color = DEFAULT_COLOR;
      if (*fillcolor == '\0')
        fillcolor = DEFAULT_COLOR;
      gvrender_set_pencolor(job, "transparent");
      gvrender_set_fillcolor(job, color);
      bz = ED_spl(e)->list[0];
      stroke_t stp = taper(&bz, taperfun(e), penwidth);
      assert(stp.nvertices <= INT_MAX);
      svg_polygon(job, stp.vertices, stp.nvertices, 1);
      free_stroke(stp);
      gvrender_set_pencolor(job, color);
      if (fillcolor != color)
        gvrender_set_fillcolor(job, fillcolor);
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
            gvrender_set_pencolor(job, color);
            gvrender_set_fillcolor(job, color);
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
          svg_bezier(job, tmplist, tmpspl.list[i].size, 0);
        }
      }
      if (bz.sflag) {
        if (color != tailcolor) {
          color = tailcolor;
          if (!(ED_gui_state(e) & (GUI_STATE_ACTIVE | GUI_STATE_SELECTED))) {
            gvrender_set_pencolor(job, color);
            gvrender_set_fillcolor(job, color);
          }
        }
        arrow_gen(job, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                  bz.sflag);
      }
      if (bz.eflag) {
        if (color != headcolor) {
          color = headcolor;
          if (!(ED_gui_state(e) & (GUI_STATE_ACTIVE | GUI_STATE_SELECTED))) {
            gvrender_set_pencolor(job, color);
            gvrender_set_fillcolor(job, color);
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
          gvrender_set_pencolor(job, color);
          gvrender_set_fillcolor(job, fillcolor);
        } else {
          gvrender_set_pencolor(job, DEFAULT_COLOR);
          if (fillcolor[0])
            gvrender_set_fillcolor(job, fillcolor);
          else
            gvrender_set_fillcolor(job, DEFAULT_COLOR);
        }
      }
      for (size_t i = 0; i < ED_spl(e)->size; i++) {
        bz = ED_spl(e)->list[i];
        svg_bezier(job, bz.list, bz.size, 0);
        if (bz.sflag) {
          arrow_gen(job, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                    bz.sflag);
        }
        if (bz.eflag) {
          arrow_gen(job, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1], arrowsize,
                    penwidth, bz.eflag);
        }
        if (ED_spl(e)->size > 1 && (bz.sflag || bz.eflag) && styles)
          gvrender_set_style(job, styles);
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

extern void svg_begin_edge(GVJ_t *job);
extern void svg_begin_anchor(GVJ_t *job, char *href, char *tooltip,
                             char *target, char *id);
static void emit_begin_edge(GVJ_t *job, edge_t *e, char **styles) {
  obj_state_t *obj;
  int flags = job->flags;
  char *s;
  textlabel_t *lab = NULL, *tlab = NULL, *hlab = NULL;
  char *dflt_url = NULL;
  char *dflt_target = NULL;
  double penwidth;

  obj = push_obj_state(job);
  obj->type = EDGE_OBJTYPE;
  obj->u.e = e;
  obj->emit_state = EMIT_EDRAW;
  if (ED_label(e) && !ED_label(e)->html && mapbool(agget(e, "labelaligned")))
    obj->labeledgealigned = true;

  /* We handle the edge style and penwidth here because the width
   * is needed below for calculating polygonal image maps
   */
  if (styles && ED_spl(e))
    gvrender_set_style(job, styles);

  if (E_penwidth && (s = agxget(e, E_penwidth)) && s[0]) {
    penwidth = late_double(e, E_penwidth, 1.0, 0.0);
    gvrender_set_penwidth(job, penwidth);
  }

  if (flags & GVRENDER_DOES_Z) {
    if (GD_odim(agraphof(agtail(e))) >= 3) {
      obj->tail_z = POINTS(ND_pos(agtail(e))[2]);
      obj->head_z = POINTS(ND_pos(aghead(e))[2]);
    } else {
      obj->tail_z = obj->head_z = 0.0;
    }
  }

  if (flags & GVRENDER_DOES_LABELS) {
    if ((lab = ED_label(e)))
      obj->label = lab->text;
    obj->taillabel = obj->headlabel = obj->xlabel = obj->label;
    if ((tlab = ED_xlabel(e)))
      obj->xlabel = tlab->text;
    if ((tlab = ED_tail_label(e)))
      obj->taillabel = tlab->text;
    if ((hlab = ED_head_label(e)))
      obj->headlabel = hlab->text;
  }

  if (flags & GVRENDER_DOES_MAPS) {
    agxbuf xb = {0};

    s = getObjId(job, e, &xb);
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
  }

  if (flags & GVRENDER_DOES_TARGETS) {
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
  }

  if (flags & GVRENDER_DOES_TOOLTIPS) {
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
  }

  free(dflt_url);
  free(dflt_target);

  if (flags & (GVRENDER_DOES_MAPS | GVRENDER_DOES_TOOLTIPS)) {
    if (ED_spl(e) && (obj->url || obj->tooltip) &&
        (flags & GVRENDER_DOES_MAP_POLYGON)) {
      splines *spl;
      double w2 = fmax(job->obj->penwidth / 2.0, 2.0);

      spl = ED_spl(e);
      const size_t ns = spl->size; /* number of splines */
      points_t pbs = {0};
      pbs_size_t pbs_n = {0};
      for (size_t i = 0; i < ns; i++)
        map_output_bspline(&pbs, &pbs_n, spl->list + i, w2);
      if (!(flags & GVRENDER_DOES_TRANSFORM)) {
        size_t nump = 0;
        for (size_t i = 0; i < pbs_size_size(&pbs_n); ++i) {
          nump += pbs_size_get(&pbs_n, i);
        }
        gvrender_ptf_A(job, points_front(&pbs), points_front(&pbs), nump);
      }
      obj->url_bsplinemap_p = points_front(&pbs);
      obj->url_map_shape = MAP_POLYGON;
      obj->url_map_p = points_detach(&pbs);
      obj->url_map_n = *pbs_size_front(&pbs_n);
      obj->url_bsplinemap_poly_n = pbs_size_size(&pbs_n);
      obj->url_bsplinemap_n = pbs_size_detach(&pbs_n);
    }
  }

  svg_begin_edge(job);
  if (obj->url || obj->explicit_tooltip)
    svg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
}

extern void svg_end_anchor(GVJ_t *job);
static void emit_edge_label(GVJ_t *job, textlabel_t *lbl, emit_state_t lkind,
                            int explicit, char *url, char *tooltip,
                            char *target, char *id, splines *spl) {
  int flags = job->flags;
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
  if ((url || explicit) && !(flags & EMIT_CLUSTERS_LAST)) {
    map_label(job, lbl);
    svg_begin_anchor(job, url, tooltip, target, newid);
  }
  emit_label(job, lkind, lbl);
  if (spl)
    emit_attachment(job, lbl, spl);
  if (url || explicit) {
    if (flags & EMIT_CLUSTERS_LAST) {
      map_label(job, lbl);
      svg_begin_anchor(job, url, tooltip, target, newid);
    }
    svg_end_anchor(job);
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

extern void svg_end_edge(GVJ_t *job);
static void emit_end_edge(GVJ_t *job) {
  obj_state_t *obj = job->obj;
  edge_t *e = obj->u.e;

  if (obj->url || obj->explicit_tooltip) {
    svg_end_anchor(job);
    if (obj->url_bsplinemap_poly_n) {
      for (size_t nump = obj->url_bsplinemap_n[0], i = 1;
           i < obj->url_bsplinemap_poly_n; i++) {
        /* additional polygon maps around remaining bezier pieces */
        obj->url_map_n = obj->url_bsplinemap_n[i];
        obj->url_map_p = &(obj->url_bsplinemap_p[nump]);
        svg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
        svg_end_anchor(job);
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

  svg_end_edge(job);
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
    svg_comment(job, agxbuse(&edge));
    agxbfree(&edge);

    s = late_string(e, E_comment, "");
    svg_comment(job, s);

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

static void emit_cluster_colors(GVJ_t *job, graph_t *g) {
  graph_t *sg;
  int c;
  char *str;

  for (c = 1; c <= GD_n_cluster(g); c++) {
    sg = GD_clust(g)[c];
    emit_cluster_colors(job, sg);
    if (((str = agget(sg, "color")) != 0) && str[0])
      gvrender_set_pencolor(job, str);
    if (((str = agget(sg, "pencolor")) != 0) && str[0])
      gvrender_set_pencolor(job, str);
    if (((str = agget(sg, "bgcolor")) != 0) && str[0])
      gvrender_set_pencolor(job, str);
    if (((str = agget(sg, "fillcolor")) != 0) && str[0])
      gvrender_set_fillcolor(job, str);
    if (((str = agget(sg, "fontcolor")) != 0) && str[0])
      gvrender_set_pencolor(job, str);
  }
}

void emit_colors(GVJ_t *job, graph_t *g) {
  node_t *n;
  edge_t *e;
  char *str, *colors;

  gvrender_set_fillcolor(job, DEFAULT_FILL);
  if (((str = agget(g, "bgcolor")) != 0) && str[0])
    gvrender_set_fillcolor(job, str);
  if (((str = agget(g, "fontcolor")) != 0) && str[0])
    gvrender_set_pencolor(job, str);

  emit_cluster_colors(job, g);
  for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
    if (((str = agget(n, "color")) != 0) && str[0])
      gvrender_set_pencolor(job, str);
    if (((str = agget(n, "pencolor")) != 0) && str[0])
      gvrender_set_fillcolor(job, str);
    if (((str = agget(n, "fillcolor")) != 0) && str[0]) {
      if (strchr(str, ':')) {
        colors = gv_strdup(str);
        for (str = strtok(colors, ":"); str; str = strtok(0, ":")) {
          if (str[0])
            gvrender_set_pencolor(job, str);
        }
        free(colors);
      } else {
        gvrender_set_pencolor(job, str);
      }
    }
    if (((str = agget(n, "fontcolor")) != 0) && str[0])
      gvrender_set_pencolor(job, str);
    for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
      if (((str = agget(e, "color")) != 0) && str[0]) {
        if (strchr(str, ':')) {
          colors = gv_strdup(str);
          for (str = strtok(colors, ":"); str; str = strtok(0, ":")) {
            if (str[0])
              gvrender_set_pencolor(job, str);
          }
          free(colors);
        } else {
          gvrender_set_pencolor(job, str);
        }
      }
      if (((str = agget(e, "fontcolor")) != 0) && str[0])
        gvrender_set_pencolor(job, str);
    }
  }
}

static void emit_view(GVJ_t *job, graph_t *g, int flags) {
  GVC_t *gvc = job->gvc;
  node_t *n;
  edge_t *e;

  gvc->common.viewNum++;
  /* when drawing, lay clusters down before nodes and edges */
  if (!(flags & EMIT_CLUSTERS_LAST))
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
  } else if (flags & EMIT_PREORDER) {
    for (n = agfstnode(g); n; n = agnxtnode(g, n))
      if (write_node_test(g, n))
        emit_node(job, n);

    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
      for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
        if (write_edge_test(g, e))
          emit_edge(job, e);
      }
    }
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
  /* when mapping, detect events on clusters after nodes and edges */
  if (flags & EMIT_CLUSTERS_LAST)
    emit_clusters(job, g, flags);
}

extern void svg_begin_graph(GVJ_t *job);
void emit_begin_graph(GVJ_t *job, graph_t *g) {
  obj_state_t *obj;

  obj = push_obj_state(job);
  obj->type = ROOTGRAPH_OBJTYPE;
  obj->u.g = g;
  obj->emit_state = EMIT_GDRAW;

  initObjMapData(job, GD_label(g), g);

  svg_begin_graph(job);
}

extern void svg_end_graph(GVJ_t *job);
void emit_end_graph(GVJ_t *job) {
  svg_end_graph(job);
  gvdevice_format(job);
  pop_obj_state(job);
}

#define NotFirstPage(j)                                                        \
  (((j)->layerNum > 1) || ((j)->pagesArrayElem.x > 0) ||                       \
   ((j)->pagesArrayElem.x > 0))

extern void svg_begin_page(GVJ_t *job);
extern void svg_end_page(GVJ_t *job);
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
    layerPagePrefix(job, &xb);
    agxbput(&xb, saveid == NULL ? "layer" : saveid);
    obj->id = agxbuse(&xb);
    obj_id_needs_restore = true;
  } else
    saveid = NULL;

  char *previous_color_scheme = setColorScheme(agget(g, "colorscheme"));
  setup_page(job);
  svg_begin_page(job);
  gvrender_set_pencolor(job, DEFAULT_COLOR);
  gvrender_set_fillcolor(job, DEFAULT_FILL);
  if ((flags & (GVRENDER_DOES_MAPS | GVRENDER_DOES_TOOLTIPS)) &&
      (obj->url || obj->explicit_tooltip)) {
    if (flags & (GVRENDER_DOES_MAP_RECTANGLE | GVRENDER_DOES_MAP_POLYGON)) {
      if (flags & GVRENDER_DOES_MAP_RECTANGLE) {
        obj->url_map_shape = MAP_RECTANGLE;
        nump = 2;
      } else {
        obj->url_map_shape = MAP_POLYGON;
        nump = 4;
      }
      p = gv_calloc(nump, sizeof(pointf));
      p[0] = job->pageBox.LL;
      p[1] = job->pageBox.UR;
      if (!(flags & (GVRENDER_DOES_MAP_RECTANGLE)))
        rect2poly(p);
    }
    if (!(flags & GVRENDER_DOES_TRANSFORM))
      gvrender_ptf_A(job, p, p, nump);
    obj->url_map_p = p;
    obj->url_map_n = nump;
  }
  if ((flags & GVRENDER_DOES_LABELS) && ((lab = GD_label(g))))
    /* do graph label on every page and rely on clipping to show it on the right
     * one(s) */
    obj->label = lab->text;
  /* If EMIT_CLUSTERS_LAST is set, we assume any URL or tooltip
   * attached to the root graph is emitted either in begin_page
   * or end_page of renderer.
   */
  if (!(flags & EMIT_CLUSTERS_LAST) && (obj->url || obj->explicit_tooltip)) {
    emit_map_rect(job, job->clip);
    svg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
  }
  emit_background(job, g);
  if (GD_label(g))
    emit_label(job, EMIT_GLABEL, GD_label(g));
  if (!(flags & EMIT_CLUSTERS_LAST) && (obj->url || obj->explicit_tooltip))
    svg_end_anchor(job);
  emit_view(job, g, flags);
  svg_end_page(job);
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

extern void svg_begin_cluster(GVJ_t *job);
static void emit_begin_cluster(GVJ_t *job, Agraph_t *sg) {
  obj_state_t *obj;

  obj = push_obj_state(job);
  obj->type = CLUSTER_OBJTYPE;
  obj->u.sg = sg;
  obj->emit_state = EMIT_CDRAW;

  initObjMapData(job, GD_label(sg), sg);

  svg_begin_cluster(job);
}

extern void svg_end_cluster(GVJ_t *job);
static void emit_end_cluster(GVJ_t *job) {
  svg_end_cluster(job);
  pop_obj_state(job);
}

void emit_clusters(GVJ_t *job, Agraph_t *g, int flags) {
  int doPerim, c, filled;
  pointf AF[4];
  char *color, *fillcolor, *pencolor, **style, *s;
  graph_t *sg;
  node_t *n;
  edge_t *e;
  obj_state_t *obj;
  textlabel_t *lab;
  int doAnchor;
  double penwidth;

  for (c = 1; c <= GD_n_cluster(g); c++) {
    sg = GD_clust(g)[c];
    if (!clust_in_layer(job, sg))
      continue;
    /* when mapping, detect events on clusters after sub_clusters */
    if (flags & EMIT_CLUSTERS_LAST)
      emit_clusters(job, sg, flags);
    emit_begin_cluster(job, sg);
    obj = job->obj;
    doAnchor = obj->url || obj->explicit_tooltip;
    char *previous_color_scheme = setColorScheme(agget(sg, "colorscheme"));
    if (doAnchor && !(flags & EMIT_CLUSTERS_LAST)) {
      emit_map_rect(job, GD_bb(sg));
      svg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
    }
    filled = 0;
    graphviz_polygon_style_t istyle = {0};
    if ((style = checkClusterStyle(sg, &istyle))) {
      gvrender_set_style(job, style);
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
        gvrender_set_fillcolor(job, clrs[0]);
        if (clrs[1])
          gvrender_set_gradient_vals(job, clrs[1],
                                     late_int(sg, G_gradientangle, 0, 0), frac);
        else
          gvrender_set_gradient_vals(job, DEFAULT_COLOR,
                                     late_int(sg, G_gradientangle, 0, 0), frac);
        if (istyle.radial)
          filled = RGRADIENT;
        else
          filled = GRADIENT;
      } else
        gvrender_set_fillcolor(job, fillcolor);
    }

    if (G_penwidth && ((s = ag_xget(sg, G_penwidth)) && s[0])) {
      penwidth = late_double(sg, G_penwidth, 1.0, 0.0);
      gvrender_set_penwidth(job, penwidth);
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
          gvrender_set_pencolor(job, pencolor);
        else
          gvrender_set_pencolor(job, "transparent");
        round_corners(job, AF, 4, istyle, filled);
      }
    } else if (istyle.striped) {
      AF[0] = GD_bb(sg).LL;
      AF[2] = GD_bb(sg).UR;
      AF[1].x = AF[2].x;
      AF[1].y = AF[0].y;
      AF[3].x = AF[0].x;
      AF[3].y = AF[2].y;
      if (late_int(sg, G_peripheries, 1, 0) == 0)
        gvrender_set_pencolor(job, "transparent");
      else
        gvrender_set_pencolor(job, pencolor);
      if (stripedBox(job, AF, fillcolor, 0) > 1)
        agerr(AGPREV, "in cluster %s\n", agnameof(sg));
      svg_box(job, GD_bb(sg), 0);
    } else {
      if (late_int(sg, G_peripheries, 1, 0)) {
        gvrender_set_pencolor(job, pencolor);
        svg_box(job, GD_bb(sg), filled);
      } else if (filled != 0) {
        gvrender_set_pencolor(job, "transparent");
        svg_box(job, GD_bb(sg), filled);
      }
    }

    free(clrs[0]);
    free(clrs[1]);
    if ((lab = GD_label(sg)))
      emit_label(job, EMIT_CLABEL, lab);

    if (doAnchor) {
      if (flags & EMIT_CLUSTERS_LAST) {
        emit_map_rect(job, GD_bb(sg));
        svg_begin_anchor(job, obj->url, obj->tooltip, obj->target, obj->id);
      }
      svg_end_anchor(job);
    }

    if (flags & EMIT_PREORDER) {
      for (n = agfstnode(sg); n; n = agnxtnode(sg, n)) {
        emit_node(job, n);
        for (e = agfstout(sg, n); e; e = agnxtout(sg, e))
          emit_edge(job, e);
      }
    }
    emit_end_cluster(job);
    /* when drawing, lay down clusters before sub_clusters */
    if (!(flags & EMIT_CLUSTERS_LAST))
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

extern void svg_begin_layer(GVJ_t *job, char *layername, int layerNum,
                            int numLayers);
extern void svg_end_layer(GVJ_t *job);
void emit_graph(GVJ_t *job, graph_t *g) {
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

  s = late_string(g, agattr_text(g, AGRAPH, "comment", 0), "");
  svg_comment(job, s);

  job->layerNum = 0;
  emit_begin_graph(job, g);

  if (flags & EMIT_COLORS)
    emit_colors(job, g);

  /* reset node state */
  for (n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;
  /* iterate layers */
  for (firstlayer(job, &lp); validlayer(job); nextlayer(job, &lp)) {
    if (numPhysicalLayers(job) > 1) {
      svg_begin_layer(job, job->gvc->layerIDs[job->layerNum], job->layerNum,
                      job->numLayers);
    }

    /* iterate pages */
    for (firstpage(job); validpage(job); nextpage(job))
      emit_page(job, g);

    if (numPhysicalLayers(job) > 1)
      svg_end_layer(job);
  }
  emit_end_graph(job);
}
