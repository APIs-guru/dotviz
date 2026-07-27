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

#include <stdatomic.h>

#include "agxbuf.h"
#include "types.h"
#include "const.h"
#include "utils.h"
#include "config.h"
#include "geomprocs.h"
#include "util/gv_ctype.h"
#include "util/gv_math.h"
#include "util/list.h"
#include "util/streq.h"
#include "util/tokenize.h"
#include "util/unreachable.h"
#include "colorprocs.h"

#include "core_svg.h"
#include "safe_job.h"
#include "gvio_svg.h"
#include "internal_render_svg.h"
#include "../output_string.h"

static void emit_clusters(output_string *output, SafeLayer *safe_layer,
                          obj_state_t *parent, Agraph_t *g);

extern Agsym_t *G_gradientangle, *G_peripheries, *G_penwidth;
extern Agsym_t *N_style, *N_layer, *N_comment;
extern Agsym_t *E_layer, *E_dir, *E_arrowsz, *E_color, *E_fillcolor,
    *E_penwidth, *E_decorate, *E_comment, *E_style;

#define EPSILON .0001

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
}

/// Use id of root graph if any, plus kind and internal id of object
char *getObjId(const SafeLayer *safe_layer, void *obj, agxbuf *xb) {
  const graph_t *const root = safe_layer->safe_job->graph;
  if (safe_layer->layerNum > 1) {
    agxbprint(xb, "%s_", safe_layer->safe_job->layerIDs[safe_layer->layerNum]);
  }

  char *id = agget(obj, "id");
  if (id && *id != '\0') {
    agxbput(xb, id);
    return agxbuse(xb);
  }

  char *gid = GD_drawing(root)->id;
  if (obj != root && gid) {
    agxbprint(xb, "%s_", gid);
  }

  switch (agobjkind(obj)) {
  case AGRAPH:
    if (root == obj)
      agxbprint(xb, "graph%u", AGSEQ(obj));
    else
      agxbprint(xb, "clust%u", AGSEQ(obj));
    break;
  case AGNODE:
    agxbprint(xb, "node%u", AGSEQ(obj));
    break;
  case AGEDGE:
    agxbprint(xb, "edge%u", AGSEQ(obj));
    break;
  }

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
  char *news = htmlEntityUTF8(s, g);
  return interpretCRNL(news);
}

static void initObjMapData(obj_state_t *obj, textlabel_t *lab, void *gobj) {
  if (lab)
    obj->label = lab->text;

  char *url = agget(gobj, "href");
  if (!url || !*url) /* try URL as an alias for href */
    url = agget(gobj, "URL");
  if (url && url[0]) {
    obj->url = strdup_and_subst_obj(url, gobj);
  }

  char *tooltip = agget(gobj, "tooltip");
  if (tooltip && tooltip[0]) {
    tooltip = preprocessTooltip(tooltip, gobj);
    obj->tooltip = strdup_and_subst_obj(tooltip, gobj);
    obj->explicit_tooltip = true;
    free(tooltip);
  } else if (obj->label) {
    obj->tooltip = gv_strdup(obj->label);
  }

  char *target = agget(gobj, "target");
  if (target && target[0]) {
    obj->target = strdup_and_subst_obj(target, gobj);
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
    obj->penwidth = THIN_LINE;

  angle0 = 0;
  for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
    const colorseg_t s = colorsegs_get(&segs, i);
    if (s.color == NULL)
      break;
    if (s.t <= 0)
      continue;
    obj->fillcolor = svg_resolve_color(s.color);

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
    obj->penwidth = save_penwidth;
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
    obj->penwidth = THIN_LINE;
  for (size_t i = 0; i < colorsegs_size(&segs); ++i) {
    const colorseg_t s = colorsegs_get(&segs, i);
    if (s.color == NULL)
      break;
    if (s.t <= 0)
      continue;
    obj->fillcolor = svg_resolve_color(s.color);
    if (i + 1 == colorsegs_size(&segs))
      pts[1].x = pts[2].x = lastx;
    else
      pts[1].x = pts[2].x = pts[0].x + xdelta * (s.t);
    svg_polygon(output, obj, pts, 4, FILL);
    pts[0].x = pts[3].x = pts[1].x;
  }
  if (save_penwidth > THIN_LINE)
    obj->penwidth = save_penwidth;
  colorsegs_free(&segs);
  return rv;
}

static bool is_natural_number(const char *sstr) {
  const char *str = sstr;

  while (*str)
    if (!gv_isdigit(*str++))
      return false;
  return true;
}

static size_t layer_index(SafeJob *safe_job, char *str, int all) {
  if (streq(str, "all"))
    return all;
  if (is_natural_number(str))
    return atoi(str);
  if (safe_job->layerIDs)
    for (int i = 1; i <= safe_job->numLayers; i++)
      if (streq(str, safe_job->layerIDs[i]))
        return i;
  return -1;
}

static bool selectedLayer(int layerNum, SafeJob *safe_job, char *spec) {
  int numLayers = safe_job->numLayers;
  char *const layerDelims = safe_job->layerDelims;
  char *const layerListDelims = safe_job->layerListDelims;

  // copy `spec` so we can `strtok_r` it
  char *spec_copy = gv_strdup(spec);
  char *part_in_p = spec_copy;

  bool rval = false;
  char *buf_part_p = NULL;
  while (!rval) {
    char *cur = strtok_r(part_in_p, layerListDelims, &buf_part_p);
    if (cur == NULL)
      break;

    char *buf_p = NULL;
    char *w0 = strtok_r(cur, layerDelims, &buf_p);
    if (w0 != NULL) {
      char *w1 = strtok_r(NULL, layerDelims, &buf_p);
      if (w1 != NULL) {
        int n0 = layer_index(safe_job, w0, 0);
        int n1 = layer_index(safe_job, w1, numLayers);
        if (n0 >= 0 || n1 >= 0) {
          if (n0 > n1) {
            SWAP(&n0, &n1);
          }
          rval = BETWEEN(n0, layerNum, n1);
        }
      } else {
        int n0 = layer_index(safe_job, w0, layerNum);
        rval = (n0 == layerNum);
      }
    } else {
      rval = false;
    }
    part_in_p = NULL;
  }
  free(spec_copy);
  return rval;
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
static int *parse_layerselect(SafeJob *safe_job, char *p) {
  int numLayers = safe_job->numLayers;
  int *laylist = gv_calloc(numLayers + 2, sizeof(int));
  int cnt = 0;
  for (int i = 1; i <= numLayers; i++) {
    if (selectedLayer(i, safe_job, p)) {
      laylist[++cnt] = i;
    }
  }
  if (cnt == 0) {
    agwarningf("The layerselect attribute \"%s\" does not match any layer "
               "specifed by the layers attribute - ignored.\n",
               p);
    free(laylist);
    return NULL;
  }
  laylist[0] = cnt;
  laylist[cnt + 1] = numLayers + 1;
  return laylist;
}

static void emit_background(output_string *output, SafeLayer *safe_layer,
                            obj_state_t *obj, graph_t *g) {
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
      obj->fillcolor = svg_resolve_color(clrs[0]);
      obj->pencolor = svg_resolve_color("transparent");
      checkClusterStyle(g, &istyle);
      if (clrs[1])
        obj->stopcolor = svg_resolve_color(clrs[1]);
      else
        obj->stopcolor = svg_resolve_color(DEFAULT_COLOR);
      obj->gradient_angle = late_int(g, G_gradientangle, 0, 0);
      obj->gradient_frac = frac;
      if (istyle.radial)
        filled = RGRADIENT;
      else
        filled = GRADIENT;
      svg_box(output, obj, safe_layer->safe_job->clip, filled);
      free(clrs[0]);
      free(clrs[1]);
    } else {
      obj->fillcolor = svg_resolve_color(str);
      obj->pencolor = svg_resolve_color("transparent");
      svg_box(output, obj, safe_layer->safe_job->clip, FILL); /* filled */
    }
  }
}

static bool node_in_layer(int layerNum, SafeJob *safe_job, graph_t *g, node_t *n) {
  char *pn, *pe;
  edge_t *e;

  if (safe_job->numLayers <= 1)
    return true;
  pn = late_string(n, N_layer, "");
  if (selectedLayer(layerNum, safe_job, pn))
    return true;
  if (pn[0])
    return false; /* Only check edges if pn = "" */
  if ((e = agfstedge(g, n)) == NULL)
    return true;
  for (e = agfstedge(g, n); e; e = agnxtedge(g, e, n)) {
    pe = late_string(e, E_layer, "");
    if (pe[0] == '\0' || selectedLayer(layerNum, safe_job, pe))
      return true;
  }
  return false;
}

static bool edge_in_layer(int layerNum, SafeJob *safe_job, edge_t *e) {
  char *pe, *pn;
  int cnt;

  if (safe_job->numLayers <= 1)
    return true;
  pe = late_string(e, E_layer, "");
  if (selectedLayer(layerNum, safe_job, pe))
    return true;
  if (pe[0])
    return false;
  for (cnt = 0; cnt < 2; cnt++) {
    pn = late_string(cnt < 1 ? agtail(e) : aghead(e), N_layer, "");
    if (pn[0] == '\0' || selectedLayer(layerNum, safe_job, pn))
      return true;
  }
  return false;
}

static bool clust_in_layer(int layerNum, SafeJob *safe_job, graph_t *sg) {
  char *pg;
  node_t *n;

  if (safe_job->numLayers <= 1)
    return true;
  pg = late_string(sg, agattr_text(sg, AGRAPH, "layer", 0), "");
  if (selectedLayer(layerNum, safe_job, pg))
    return true;
  if (pg[0])
    return false;
  for (n = agfstnode(sg); n; n = agnxtnode(sg, n))
    if (node_in_layer(layerNum, safe_job, sg, n))
      return true;
  return false;
}

static bool node_in_box(node_t *n, boxf b) { return boxf_overlap(ND_bb(n), b); }

static char *saved_color_scheme;

static void emit_begin_node(output_string *output, SafeLayer *safe_layer,
                            obj_state_t *obj, node_t *n) {
  obj->type = NODE_OBJTYPE;
  obj->u.n = n;
  obj->emit_state = EMIT_NDRAW;
  agxbuf xb = {0};
  char *id = getObjId(safe_layer, n, &xb);
  obj->id = strdup_and_subst_obj(id, n);
  agxbfree(&xb);
  initObjMapData(obj, ND_label(n), n);
  saved_color_scheme = setColorScheme(agget(n, "colorscheme"));

  out_puts(output, "<g");
  if (safe_layer->layerNum > 1) {
    char *idx = safe_layer->safe_job->layerIDs[safe_layer->layerNum];
    svg_print_id(output, obj->id, idx);
  } else
    svg_print_id(output, obj->id, NULL);
  svg_print_class(output, "node", n);
  out_puts(output, ">\n<title>");
  gvputs_xml(output, agnameof(n));
  out_puts(output, "</title>\n");
}

static void emit_end_node(output_string *output) {
  out_puts(output, "</g>\n");

  char *color_scheme = setColorScheme(saved_color_scheme);
  free(color_scheme);
  free(saved_color_scheme);
  saved_color_scheme = NULL;
}

static void emit_node(output_string *output, SafeLayer *safe_layer,
                      int *viewNum, obj_state_t *parent, node_t *n) {
  int layerNum = safe_layer->layerNum;
  SafeJob *safe_job = safe_layer->safe_job;
  if (ND_shape(n)                                   /* node has a shape */
      && node_in_layer(layerNum, safe_job, agraphof(n), n)  /* and is in layer */
      && node_in_box(n, safe_job->clip) /* and is in page/view */
      && ND_state(n) != *viewNum)                   /* and not already drawn */
  {
    ND_state(n) = *viewNum; /* mark node as drawn */

    svg_comment(output, agnameof(n));
    char *s = late_string(n, N_comment, "");
    svg_comment(output, s);

    char *style = late_string(n, N_style, "");
    if (style[0]) {
      char **styles = parse_style(style);
      char **sp = styles;
      char *p;
      while ((p = *sp++)) {
        if (streq(p, "invis")) {
          return;
        }
      }
    }

    obj_state_t obj = child_obj_state(parent);
    emit_begin_node(output, safe_layer, &obj, n);
    ND_shape(n)->fns->codefn(output, safe_layer, &obj, n);

    if (ND_xlabel(n) && ND_xlabel(n)->set) {
      emit_label(output, safe_layer, &obj, EMIT_NLABEL, ND_xlabel(n));
    }

    emit_end_node(output);
    free_child_obj(&obj);
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

static void emit_attachment(output_string *output, obj_state_t *obj,
                            textlabel_t *lp, splines *spl) {

  for (const char *s = lp->text; true; ++s) {
    if (*s == '\0')
      return;
    if (!gv_isspace(*s))
      break;
  }

  pointf sz = lp->dimen;
  pointf AF[3] = {
    {lp->pos.x + sz.x / 2., lp->pos.y - sz.y / 2.},
    {AF[0].x - sz.x, AF[0].y},
    dotneato_closest(spl, lp->pos),
  };
  /* Don't use edge style to draw attachment */
  obj->pen = PEN_SOLID; // default line style
  obj->penwidth = 1.0;  // default line style
  /* Use font color to draw attachment
     - need something unambiguous in case of multicolored parallel edges
     - defaults to black for html-like labels
   */
  obj->pencolor = svg_resolve_color(lp->fontcolor);
  svg_polyline(output, obj, AF, 3);
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
static int multicolor(output_string *output, obj_state_t *obj, edge_t *e,
                      char **styles, const char *colors, double arrowsize,
                      double penwidth) {
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
      obj->pencolor = svg_resolve_color(s.color);
      left -= s.t;
      endcolor = s.color;
      if (first) {
        first = 0;
        splitBSpline(&bz, s.t, &bz_l, &bz_r);
        svg_bezier(output, obj, bz_l.list, bz_l.size, 0);
        free(bz_l.list);
        if (AEQ0(left)) {
          free(bz_r.list);
          break;
        }
      } else if (AEQ0(left)) {
        svg_bezier(output, obj, bz_r.list, bz_r.size, 0);
        free(bz_r.list);
        break;
      } else {
        bz0 = bz_r;
        splitBSpline(&bz0, s.t / (left + s.t), &bz_l, &bz_r);
        free(bz0.list);
        svg_bezier(output, obj, bz_l.list, bz_l.size, 0);
        free(bz_l.list);
      }
    }
    /* arrow_gen resets the job style  (How?  FIXME)
     * If we have more splines to do, restore the old one.
     * Use local copy of penwidth to work around reset.
     */
    if (bz.sflag) {
      obj->pencolor = svg_resolve_color(colorsegs_front(&segs)->color);
      obj->fillcolor = svg_resolve_color(colorsegs_front(&segs)->color);
      arrow_gen(output, obj, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize, penwidth,
                bz.sflag);
    }
    if (bz.eflag) {
      obj->pencolor = svg_resolve_color(endcolor);
      obj->fillcolor = svg_resolve_color(endcolor);
      arrow_gen(output, obj, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1], arrowsize,
                penwidth, bz.eflag);
    }
    if (ED_spl(e)->size > 1 && (bz.sflag || bz.eflag) && styles)
      svg_set_style(obj, styles);
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

static void emit_edge_graphics(output_string *output, obj_state_t *obj,
                               edge_t *e, char **styles) {
  bezier bz;
  double penwidth = obj->penwidth;
  agxbuf buf = {0};

#define SEP 2.0

  char *previous_color_scheme = setColorScheme(agget(e, "colorscheme"));
  if (ED_spl(e)) {
    double arrowsize = late_double(e, E_arrowsz, 1.0, 0.0);
    char* color = late_string(e, E_color, "");
    bool tapered = false;

    if (styles) {
      char **sp = styles;
      char* p;
      while ((p = *sp++)) {
        if (streq(p, "tapered")) {
          tapered = true;
          break;
        }
      }
    }

    /* need to know how many colors separated by ':' */
    int numsemi = 0;
    size_t numc = 0;
    for (char* p = color; *p; p++) {
      if (*p == ':')
        numc++;
      else if (*p == ';')
        numsemi++;
    }

    if (numsemi && numc) {
      if (multicolor(output, obj, e, styles, color, arrowsize, penwidth)) {
        color = DEFAULT_COLOR;
      } else
        goto done;
    }

    char* fillcolor = late_nnstring(e, E_fillcolor, color);
    if (fillcolor != color)
      obj->fillcolor = svg_resolve_color(fillcolor);

    if (tapered) {
      if (*color == '\0')
        color = DEFAULT_COLOR;
      if (*fillcolor == '\0')
        fillcolor = DEFAULT_COLOR;
      obj->pencolor = svg_resolve_color("transparent");
      obj->fillcolor = svg_resolve_color(color);
      bz = ED_spl(e)->list[0];
      stroke_t stp = taper(&bz, taperfun(e), penwidth);
      assert(stp.nvertices <= INT_MAX);
      svg_polygon(output, obj, stp.vertices, stp.nvertices, 1);
      free_stroke(stp);
      obj->pencolor = svg_resolve_color(color);
      if (fillcolor != color)
        obj->fillcolor = svg_resolve_color(fillcolor);
      if (bz.sflag) {
        arrow_gen(output, obj, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize,
                  penwidth, bz.sflag);
      }
      if (bz.eflag) {
        arrow_gen(output, obj, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1],
                  arrowsize, penwidth, bz.eflag);
      }
    }
    /* if more than one color - then generate parallel beziers, one per color */
    else if (numc) {
      /* calculate and save offset vector spline and initialize first offset
       * spline */
      splines tmpspl;
      splines offspl;
      tmpspl.size = offspl.size = ED_spl(e)->size;
      offspl.list = gv_calloc(offspl.size, sizeof(bezier));
      tmpspl.list = gv_calloc(tmpspl.size, sizeof(bezier));
      double numc2 = (2 + (double)numc) / 2.0;
      for (size_t i = 0; i < offspl.size; i++) {
        bz = ED_spl(e)->list[i];
        tmpspl.list[i].size = offspl.list[i].size = bz.size;
        pointf *offlist = offspl.list[i].list = gv_calloc(bz.size, sizeof(pointf));
        pointf *tmplist = tmpspl.list[i].list = gv_calloc(bz.size, sizeof(pointf));
        pointf pf2 = {0, 0};
        pointf pf3 = bz.list[0];
        size_t j;
        for (j = 0; j < bz.size - 1; j += 3) {
          pointf pf0 = pf3;
          pointf pf1 = bz.list[j + 1];
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

      char* lastcolor = color;
      char* headcolor = color;
      char* tailcolor = color;
      char *colors = gv_strdup(color);
      int cnum = 0;
      for (color = strtok(colors, ":"); color;
           cnum++, color = strtok(0, ":")) {
        if (!color[0])
          color = DEFAULT_COLOR;
        if (color != lastcolor) {
          obj->pencolor = svg_resolve_color(color);
          obj->fillcolor = svg_resolve_color(color);
          lastcolor = color;
        }
        if (cnum == 0)
          headcolor = tailcolor = color;
        if (cnum == 1)
          tailcolor = color;
        for (size_t i = 0; i < tmpspl.size; i++) {
          pointf *tmplist = tmpspl.list[i].list;
          pointf *offlist = offspl.list[i].list;
          for (size_t j = 0; j < tmpspl.list[i].size; j++) {
            tmplist[j].x += offlist[j].x;
            tmplist[j].y += offlist[j].y;
          }
          svg_bezier(output, obj, tmplist, tmpspl.list[i].size, 0);
        }
      }
      if (bz.sflag) {
        if (color != tailcolor) {
          color = tailcolor;
          obj->pencolor = svg_resolve_color(color);
          obj->fillcolor = svg_resolve_color(color);
        }
        arrow_gen(output, obj, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize,
                  penwidth, bz.sflag);
      }
      if (bz.eflag) {
        if (color != headcolor) {
          color = headcolor;
          obj->pencolor = svg_resolve_color(color);
          obj->fillcolor = svg_resolve_color(color);
        }
        arrow_gen(output, obj, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1],
                  arrowsize, penwidth, bz.eflag);
      }
      free(colors);
      for (size_t i = 0; i < offspl.size; i++) {
        free(offspl.list[i].list);
        free(tmpspl.list[i].list);
      }
      free(offspl.list);
      free(tmpspl.list);
    } else {
      if (color[0]) {
        obj->pencolor = svg_resolve_color(color);
        obj->fillcolor = svg_resolve_color(fillcolor);
      } else {
        obj->pencolor = svg_resolve_color(DEFAULT_COLOR);
        if (fillcolor[0])
          obj->fillcolor = svg_resolve_color(fillcolor);
        else
          obj->fillcolor = svg_resolve_color(DEFAULT_COLOR);
      }
      for (size_t i = 0; i < ED_spl(e)->size; i++) {
        bz = ED_spl(e)->list[i];
        svg_bezier(output, obj, bz.list, bz.size, 0);
        if (bz.sflag) {
          arrow_gen(output, obj, EMIT_TDRAW, bz.sp, bz.list[0], arrowsize,
                    penwidth, bz.sflag);
        }
        if (bz.eflag) {
          arrow_gen(output, obj, EMIT_HDRAW, bz.ep, bz.list[bz.size - 1],
                    arrowsize, penwidth, bz.eflag);
        }
        if (ED_spl(e)->size > 1 && (bz.sflag || bz.eflag) && styles)
          svg_set_style(obj, styles);
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

static void emit_begin_edge(output_string *output, SafeLayer *safe_layer,
                            obj_state_t *obj, edge_t *e, char **styles) {
  char *s;
  textlabel_t *lab = NULL, *tlab = NULL, *hlab = NULL;
  char *dflt_url = NULL;
  char *dflt_target = NULL;
  double penwidth;

  obj->type = EDGE_OBJTYPE;
  obj->u.e = e;
  obj->emit_state = EMIT_EDRAW;
  if (ED_label(e) && !ED_label(e)->html && mapbool(agget(e, "labelaligned")))
    obj->labeledgealigned = true;

  /* We handle the edge style and penwidth here because the width
   * is needed below for calculating polygonal image maps
   */
  if (styles && ED_spl(e))
    svg_set_style(obj, styles);

  if (E_penwidth && (s = agxget(e, E_penwidth)) && s[0]) {
    penwidth = late_double(e, E_penwidth, 1.0, 0.0);
    obj->penwidth = penwidth;
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

  s = getObjId(safe_layer, e, &xb);
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

  out_puts(output, "<g");
  svg_print_id(output, obj->id, NULL);
  svg_print_class(output, "edge", e);
  out_puts(output, ">\n<title>");
  char *ename = strdup_and_subst_obj("\\E", e);
  gvputs_xml(output, ename);
  free(ename);
  out_puts(output, "</title>\n");
  if (obj->url || obj->explicit_tooltip)
    svg_begin_anchor(output, obj->url, obj->tooltip, obj->target, obj->id);
}

static void emit_edge_label(output_string *output, SafeLayer *safe_layer,
                            obj_state_t *obj, textlabel_t *lbl,
                            emit_state_t lkind, int explicit, char *url,
                            char *tooltip, char *target, char *id,
                            splines *spl) {
  if (lbl == NULL || !lbl->set)
    return;

  emit_state_t old_emit_state = obj->emit_state;
  obj->emit_state = lkind;
  if (url || explicit) {
    agxbuf xb = {0};
    char *newid = NULL;
    if (id) { /* non-NULL if needed */
      switch (lkind) {
      case EMIT_ELABEL:
        agxbprint(&xb, "%s-label", id);
        break;
      case EMIT_HLABEL:
        agxbprint(&xb, "%s-headlabel", id);
        break;
      case EMIT_TLABEL:
        agxbprint(&xb, "%s-taillabel", id);
        break;
      default:
        UNREACHABLE();
      }
      newid = agxbuse(&xb);
    }

    svg_begin_anchor(output, url, tooltip, target, newid);
    agxbfree(&xb);
  }
  emit_label(output, safe_layer, obj, lkind, lbl);
  if (spl)
    emit_attachment(output, obj, lbl, spl);
  if (url || explicit) {
    out_puts(output, "</a>\n</g>\n"); // end anchor
  }
  obj->emit_state = old_emit_state;
}

static void emit_end_edge(output_string *output, SafeLayer *safe_layer,
                          obj_state_t *obj) {
  edge_t *e = obj->u.e;

  if (obj->url || obj->explicit_tooltip) {
    svg_end_anchor(output);
  }
  emit_edge_label(output, safe_layer, obj, ED_label(e), EMIT_ELABEL,
                  obj->explicit_labeltooltip, obj->labelurl, obj->labeltooltip,
                  obj->labeltarget, obj->id,
                  ((mapbool(late_string(e, E_decorate, "false")) && ED_spl(e))
                       ? ED_spl(e)
                       : 0));
  emit_edge_label(output, safe_layer, obj, ED_xlabel(e), EMIT_ELABEL,
                  obj->explicit_labeltooltip, obj->labelurl, obj->labeltooltip,
                  obj->labeltarget, obj->id,
                  ((mapbool(late_string(e, E_decorate, "false")) && ED_spl(e))
                       ? ED_spl(e)
                       : 0));
  emit_edge_label(output, safe_layer, obj, ED_head_label(e), EMIT_HLABEL,
                  obj->explicit_headtooltip, obj->headurl, obj->headtooltip,
                  obj->headtarget, obj->id, 0);
  emit_edge_label(output, safe_layer, obj, ED_tail_label(e), EMIT_TLABEL,
                  obj->explicit_tailtooltip, obj->tailurl, obj->tailtooltip,
                  obj->tailtarget, obj->id, 0);

  out_puts(output, "</g>\n"); // end edge
}

static void emit_edge(output_string *output, SafeLayer *safe_layer,
                      obj_state_t *parent, edge_t *e) {
  char *s;
  char *style;
  char **styles = NULL;
  char **sp;
  char *p;
  int layerNum = safe_layer->layerNum;
  SafeJob *safe_job = safe_layer->safe_job;

  if (edge_in_box(e, safe_job->clip) &&
      edge_in_layer(layerNum, safe_job, e)) {

    agxbuf edge = {0};
    agxbput(&edge, agnameof(agtail(e)));
    if (agisdirected(agraphof(aghead(e))))
      agxbput(&edge, "->");
    else
      agxbput(&edge, "--");
    agxbput(&edge, agnameof(aghead(e)));
    svg_comment(output, agxbuse(&edge));
    agxbfree(&edge);

    s = late_string(e, E_comment, "");
    svg_comment(output, s);

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

    obj_state_t obj = child_obj_state(parent);
    emit_begin_edge(output, safe_layer, &obj, e, styles);
    emit_edge_graphics(output, &obj, e, styles);
    emit_end_edge(output, safe_layer, &obj);
    free_child_obj(&obj);
  }
}

static void emit_view(output_string *output, SafeLayer *safe_layer,
                      obj_state_t *obj, graph_t *g, int *viewNum,
                      int graph_outputorder) {
  node_t *n;
  edge_t *e;

  *viewNum += 1;
  /* when drawing, lay clusters down before nodes and edges */
  emit_clusters(output, safe_layer, obj, g);
  if (graph_outputorder & EMIT_SORTED) {
    /* output all nodes, then all edges */
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
      emit_node(output, safe_layer, viewNum, obj, n);
    }
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
      for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
        emit_edge(output, safe_layer, obj, e);
      }
    }
  } else if (graph_outputorder & EMIT_EDGE_SORTED) {
    /* output all edges, then all nodes */
    for (n = agfstnode(g); n; n = agnxtnode(g, n))
      for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
        emit_edge(output, safe_layer, obj, e);
      }
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
      emit_node(output, safe_layer, viewNum, obj, n);
    }
  } else {
    /* output in breadth first graph walk order */
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
      emit_node(output, safe_layer, viewNum, obj, n);
      for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
        emit_node(output, safe_layer, viewNum, obj, aghead(e));
        emit_edge(output, safe_layer, obj, e);
      }
    }
  }
}

static void emit_layer(output_string *output, SafeLayer *safe_layer,
                       obj_state_t *obj, graph_t *g, int *viewNum,
                       int graph_outputorder) {
  agxbuf xb = {0};

  /* For the first page, we can use the values generated in emit_begin_graph.
   * For multiple pages, we need to generate a new id.
   */
  bool obj_id_needs_restore = false;
  char *saveid;
  if (safe_layer->layerNum > 1) {
    saveid = obj->id;
    agxbprint(&xb, "%s_", safe_layer->safe_job->layerIDs[safe_layer->layerNum]);
    agxbput(&xb, saveid == NULL ? "layer" : saveid);
    obj->id = agxbuse(&xb);
    obj_id_needs_restore = true;
  } else
    saveid = NULL;

  char *previous_color_scheme = setColorScheme(agget(g, "colorscheme"));

  pointf scale; /* composite device to graph units (zoom and dpi) */
  scale.x = safe_layer->safe_job->zoom * safe_layer->safe_job->dpi.x /
            POINTS_PER_INCH;
  scale.y = safe_layer->safe_job->zoom * safe_layer->safe_job->dpi.y /
            POINTS_PER_INCH;

  /* its really just a page of the graph, but its still a graph,
   * and it is the entire graph if we're not currently paging */
  out_puts(output, "<g");
  svg_print_id(output, obj->id, NULL);
  svg_print_class(output, "graph", g);
  out_puts(output, " transform=\"scale(");
  // cannot be gvprintdouble because 2 digits precision insufficient
  gvprintf(output, "%g %g", scale.x, scale.y);
  gvprintf(output, ") rotate(%d) translate(", -safe_layer->safe_job->rotation);

  /* CAUTION - job->translation was difficult to get right. */
  // Test with and without asymmetric margins, e.g: -Gmargin="1,0"
  double translation_y = 0;
  double translation_x = 0;
  if (safe_layer->safe_job->rotation) {
    translation_y =
        -safe_layer->safe_job->clip.UR.y -
        safe_layer->safe_job->canvasBox.LL.y / safe_layer->safe_job->zoom;
    translation_x =
        -safe_layer->safe_job->clip.UR.x -
        safe_layer->safe_job->canvasBox.LL.x / safe_layer->safe_job->zoom;
  } else {
    /* pre unscale margins to keep them constant under scaling */
    translation_x =
        -safe_layer->safe_job->clip.LL.x +
        safe_layer->safe_job->canvasBox.LL.x / safe_layer->safe_job->zoom;
    translation_y =
        -safe_layer->safe_job->clip.UR.y -
        safe_layer->safe_job->canvasBox.LL.y / safe_layer->safe_job->zoom;
  }

  gvprintdouble(output, translation_x);
  out_putc(output, ' ');
  gvprintdouble(output, -translation_y);
  out_puts(output, ")\">\n");
  /* default style */
  if (agnameof(obj->u.g)[0] && agnameof(g)[0] != LOCALNAMEPREFIX) {
    out_puts(output, "<title>");
    gvputs_xml(output, agnameof(g));
    out_puts(output, "</title>\n");
  }
  obj->pencolor = svg_resolve_color(DEFAULT_COLOR);
  obj->fillcolor = svg_resolve_color(DEFAULT_FILL);

  textlabel_t *lab = GD_label(g);
  if (lab != NULL) {
    /* do graph label on every page and rely on clipping to show it on the right
     * one(s) */
    obj->label = lab->text;
  }
  /* If EMIT_CLUSTERS_LAST is set, we assume any URL or tooltip
   * attached to the root graph is emitted either in begin_page
   * or end_page of renderer.
   */
  if (obj->url || obj->explicit_tooltip) {
    svg_begin_anchor(output, obj->url, obj->tooltip, obj->target, obj->id);
  }
  emit_background(output, safe_layer, obj, g);
  if (GD_label(g))
    emit_label(output, safe_layer, obj, EMIT_GLABEL, GD_label(g));
  if (obj->url || obj->explicit_tooltip)
    svg_end_anchor(output);
  emit_view(output, safe_layer, obj, g, viewNum, graph_outputorder);
  out_puts(output, "</g>\n"); // end page
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

static void emit_begin_cluster(output_string *output, SafeLayer *safe_layer,
                               obj_state_t *obj, Agraph_t *sg) {
  obj->type = CLUSTER_OBJTYPE;
  obj->u.sg = sg;
  obj->emit_state = EMIT_CDRAW;
  agxbuf xb = {0};
  char *id = getObjId(safe_layer, sg, &xb);
  obj->id = strdup_and_subst_obj(id, sg);
  agxbfree(&xb);
  initObjMapData(obj, GD_label(sg), sg);

  out_puts(output, "<g");
  svg_print_id(output, obj->id, NULL);
  svg_print_class(output, "cluster", sg);
  out_puts(output, ">\n<title>");
  gvputs_xml(output, agnameof(sg));
  out_puts(output, "</title>\n");
}

static void emit_clusters(output_string *output, SafeLayer *safe_layer,
                          obj_state_t *parent, Agraph_t *g) {
  char *color, *fillcolor, *pencolor;
  SafeJob *safe_job = safe_layer->safe_job;

  for (int c = 1; c <= GD_n_cluster(g); c++) {
    graph_t *sg = GD_clust(g)[c];
    int layerNum = safe_layer->layerNum;
    if (!clust_in_layer(layerNum, safe_job, sg))
      continue;
    obj_state_t obj = child_obj_state(parent);
    emit_begin_cluster(output, safe_layer, &obj, sg);
    int doAnchor = obj.url || obj.explicit_tooltip;
    char *previous_color_scheme = setColorScheme(agget(sg, "colorscheme"));
    if (doAnchor) {
      svg_begin_anchor(output, obj.url, obj.tooltip, obj.target, obj.id);
    }
    int filled = 0;
    graphviz_polygon_style_t istyle = {0};
    char** style = checkClusterStyle(sg, &istyle);
    if (style != NULL) {
      svg_set_style(&obj, style);
      if (istyle.filled)
        filled = FILL;
    }
    fillcolor = pencolor = 0;
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
    if (!pencolor)
      pencolor = DEFAULT_COLOR;
    if (!fillcolor)
      fillcolor = DEFAULT_FILL;
    char *clrs[2] = {0};
    if (filled != 0) {
      double frac;
      if (findStopColor(fillcolor, clrs, &frac)) {
        obj.fillcolor = svg_resolve_color(clrs[0]);
        if (clrs[1])
          obj.stopcolor = svg_resolve_color(clrs[1]);
        else
          obj.stopcolor = svg_resolve_color(DEFAULT_COLOR);
        obj.gradient_angle = late_int(sg, G_gradientangle, 0, 0);
        obj.gradient_frac = frac;
        if (istyle.radial)
          filled = RGRADIENT;
        else
          filled = GRADIENT;
      } else
        obj.fillcolor = svg_resolve_color(fillcolor);
    }

    char *s;
    if (G_penwidth && ((s = ag_xget(sg, G_penwidth)) && s[0])) {
      obj.penwidth = late_double(sg, G_penwidth, 1.0, 0.0);
    }

    if (istyle.rounded) {
      int doPerim = late_int(sg, G_peripheries, 1, 0);
      if (doPerim != 0 || filled != 0) {
        pointf AF[4];
        AF[0] = GD_bb(sg).LL;
        AF[2] = GD_bb(sg).UR;
        AF[1].x = AF[2].x;
        AF[1].y = AF[0].y;
        AF[3].x = AF[0].x;
        AF[3].y = AF[2].y;
        if (doPerim)
          obj.pencolor = svg_resolve_color(pencolor);
        else
          obj.pencolor = svg_resolve_color("transparent");
        round_corners(output, &obj, AF, 4, istyle, filled);
      }
    } else if (istyle.striped) {
      pointf AF[4];
      AF[0] = GD_bb(sg).LL;
      AF[2] = GD_bb(sg).UR;
      AF[1].x = AF[2].x;
      AF[1].y = AF[0].y;
      AF[3].x = AF[0].x;
      AF[3].y = AF[2].y;
      if (late_int(sg, G_peripheries, 1, 0) == 0)
        obj.pencolor = svg_resolve_color("transparent");
      else
        obj.pencolor = svg_resolve_color(pencolor);
      if (stripedBox(output, &obj, AF, fillcolor, 0) > 1)
        agerr(AGPREV, "in cluster %s\n", agnameof(sg));
      svg_box(output, &obj, GD_bb(sg), 0);
    } else {
      if (late_int(sg, G_peripheries, 1, 0)) {
        obj.pencolor = svg_resolve_color(pencolor);
        svg_box(output, &obj, GD_bb(sg), filled);
      } else if (filled != 0) {
        obj.pencolor = svg_resolve_color("transparent");
        svg_box(output, &obj, GD_bb(sg), filled);
      }
    }

    free(clrs[0]);
    free(clrs[1]);
    textlabel_t *lab = GD_label(sg);
    if (lab != NULL)
      emit_label(output, safe_layer, &obj, EMIT_CLABEL, lab);

    if (doAnchor) {
      svg_end_anchor(output);
    }

    out_puts(output, "</g>\n"); // end cluster
    free_child_obj(&obj);
    /* when drawing, lay down clusters before sub_clusters */
    emit_clusters(output, safe_layer, &obj, sg);

    char *color_scheme = setColorScheme(previous_color_scheme);
    free(color_scheme);
    free(previous_color_scheme);
  }
}

typedef struct {
  bool in_parens;
  bool has_error;
  char const *input;
  char const *p;
} parser_state_t;

static char const *next_token(parser_state_t *state) {
  for (const char *start = NULL;; ++state->p) {
    switch (*state->p) {
    case '\0':
      if (state->in_parens) {
        agerrorf("unmatched '(' in style: %s\n", state->input);
        state->has_error = true;
        return NULL;
      }
      return start;
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
    case ' ':
    case ',':
      if (start != NULL)
        return start;
      break;
    case '(':
      if (state->in_parens) {
        agerrorf("nesting not allowed in style: %s\n", state->input);
        state->has_error = true;
        return NULL;
      }
      if (start != NULL)
        return start;
      state->in_parens = true;
      break;
    case ')':
      if (!state->in_parens) {
        agerrorf("unmatched ')' in style: %s\n", state->input);
        state->has_error = true;
        return NULL;
      }
      if (start != NULL)
        return start;
      state->in_parens = false;
      break;
    default:
      if (start == NULL)
        start = state->p;
    }
  }
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
  size_t parse_offsets[FUNLIMIT];
  size_t fun = 0;
  static agxbuf ps_xb;

  parser_state_t state = {.input = s, .p = s, .in_parens = false, .has_error = false};
  const char *start;
  while ((start = next_token(&state)) != NULL) {
    if (!state.in_parens) {
      if (fun == FUNLIMIT - 1) {
        agwarningf("truncating style '%s'\n", s);
        parse[fun] = NULL;
        return parse;
      }
      parse_offsets[fun++] = agxblen(&ps_xb);
    }
    agxbput_n(&ps_xb, start, state.p - start);
    agxbputc(&ps_xb, '\0');
  }
  if (state.has_error) {
    agxbstart(&ps_xb);
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
  clrs[0] = NULL;
  clrs[1] = NULL;

  int rv = parseSegs(colorlist, &segs);
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

output_string emit_graph(SafeJob *safe_job, graph_t *g,
                         int graph_outputorder) {
  /* page size on Linux, Mac OS X and Windows */
  output_string output = {.data_position = 0, .data_allocated = 4096};
  if (!(output.data = malloc(output.data_allocated))) {
    agerrorf("failure malloc'ing for result string");
    exit(-1);
  }

  out_puts(&output,
           "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
  const char *stylesheet = agget(g, "stylesheet");
  if (stylesheet && stylesheet[0]) {
    out_puts(&output, "<?xml-stylesheet href=\"");
    out_puts(&output, stylesheet);
    out_puts(&output, "\" type=\"text/css\"?>\n");
  }
  out_puts(&output, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n "
                    "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
  // FIXME: remove hardcode
  svg_comment(&output, "Generated by graphviz version a (a)\n");

  char *s = late_string(g, agattr_text(g, AGRAPH, "comment", 0), "");
  svg_comment(&output, s);

  out_puts(&output, "<!--");
  if (agnameof(g)[0] && agnameof(g)[0] != LOCALNAMEPREFIX) {
    out_puts(&output, " Title: ");
    gvputs_xml(&output, agnameof(g));
  }
  out_puts(&output, " Pages: 1 -->\n");

  gvprintf(&output, "<svg width=\"%dpt\" height=\"%dpt\"\n", safe_job->width,
           safe_job->height);
  gvprintf(&output, " viewBox=\"%d.00 %d.00 %d.00 %d.00\"",
           safe_job->pageBoundingBox.LL.x, safe_job->pageBoundingBox.LL.y,
           safe_job->pageBoundingBox.UR.x, safe_job->pageBoundingBox.UR.y);
  // https://svgwg.org/svg2-draft/struct.html#Namespace says:
  // > There's no need to have an ‘xmlns’ attribute declaring that the
  // > element is in the SVG namespace when using the HTML parser. The HTML
  // > parser will automatically create the SVG elements in the proper
  // > namespace.
  /* namespace of svg */
  out_puts(&output, " xmlns=\"http://www.w3.org/2000/svg\""
                    /* namespace of xlink */
                    " xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
  out_puts(&output, ">\n");

  /* reset node state */
  for (node_t *n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;

  obj_state_t obj = {0};
  obj.parent = NULL;
  obj.pen = PEN_SOLID;
  obj.fill = FILL_NONE;
  obj.penwidth = PENWIDTH_NORMAL;
  obj.type = ROOTGRAPH_OBJTYPE;
  obj.u.g = g;
  obj.emit_state = EMIT_GDRAW;

  SafeLayer dummy_layer = {.layerNum = 0, .safe_job = safe_job};
  agxbuf xb = {0};
  char *id = getObjId(&dummy_layer, g, &xb);
  obj.id = strdup_and_subst_obj(id, g);
  agxbfree(&xb);
  initObjMapData(&obj, GD_label(g), g);

  int *lp = NULL;
  int layerNum = 1;
  int num_physical_layers = safe_job->numLayers;

  char *layerselect_str = agget(g, "layerselect");
  if (layerselect_str != NULL && *layerselect_str) {
    int *layerlist = parse_layerselect(safe_job, layerselect_str);
    num_physical_layers = layerlist[0];
    layerNum = layerlist[1]; // first layer
    lp = layerlist + 2;      // tail layers
  }

  int viewNum = 0; ///< current view - 1 based count of views, all pages
                   ///< in all layers
  if (num_physical_layers > 1) {
    /* iterate layers */
    while (layerNum <= safe_job->numLayers) {
        out_puts(&output, "<g");
        svg_print_id(&output, safe_job->layerIDs[layerNum], NULL);
        svg_print_class(&output, "layer", g);
        out_puts(&output, ">\n");
        SafeLayer safe_layer = {.layerNum = layerNum, .safe_job = safe_job};
        emit_layer(&output, &safe_layer, &obj, g, &viewNum, graph_outputorder);
        out_puts(&output, "</g>\n");

      if (lp) {
        layerNum = *lp;
        lp += 1;
      } else {
        layerNum += 1;
      }
    }
  } else {
    SafeLayer safe_layer = {.layerNum = layerNum, .safe_job = safe_job};
    emit_layer(&output, &safe_layer, &obj, g, &viewNum, graph_outputorder);
  }
  out_puts(&output, "</svg>\n"); // end graph
  free_child_obj(&obj);
  return output;
}
