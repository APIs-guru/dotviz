#include "agrw.h"
#include "cgraph.h"
#include "gvc.h" // IWYU pragma: keep
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

// undirected
#include "const.h"           // IWYU pragma: keep
#include "gvcint.h"          // IWYU pragma: keep
#include "gvplugin_layout.h" // IWYU pragma: keep
// undirected

#include "entities.h"
#include "streq.h"

extern char *Gvfilepath;  /* Per-process path of files allowed in image
                             attributes (also ps libs) */
extern char *Gvimagepath; /* Per-graph path of files allowed in image attributes
                             (also ps libs) */
extern int CL_type;       /* NONE, LOCAL, GLOBAL */
extern bool Concentrate;  /// if parallel edges should be merged
extern int State;         /* last finished phase */
extern int EdgeLabelsDone; /* true if edge labels have been positioned */
extern double Initial_dist;
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

extern char *my_strdup_and_subst_obj0(char *str, void *obj, int escBackslash);

extern char *strdup_and_subst_obj(char *str, void *obj);

extern bool mapbool(const char *p);

extern int maptoken(char *p, char **name, int *val);

extern int late_int(void *obj, attrsym_t *attr, int defaultValue, int minimum);

extern double late_double(void *obj, attrsym_t *attr, double defaultValue,
                          double minimum);

extern char *late_string(void *obj, attrsym_t *attr, char *defaultValue);

extern char *late_nnstring(void *obj, attrsym_t *attr, char *defaultValue);

extern void do_graph_label(graph_t *sg);

extern void *init_xdot(Agraph_t *g);

/* converts a graph attribute in inches to a pointf in points.
 * If only one number is given, it is used for both x and y.
 * Returns true if the attribute ends in '!'.
 */
static bool getdoubles2ptf(graph_t *g, char *name, pointf *result) {
  char *p;
  int i;
  double xf, yf;
  char c = '\0';
  bool rv = false;

  if ((p = agget(g, name))) {
    i = sscanf(p, "%lf,%lf%c", &xf, &yf, &c);
    if (i > 1 && xf > 0 && yf > 0) {
      result->x = POINTS(xf);
      result->y = POINTS(yf);
      if (c == '!')
        rv = true;
    } else {
      c = '\0';
      i = sscanf(p, "%lf%c", &xf, &c);
      if (i > 0 && xf > 0) {
        result->y = result->x = POINTS(xf);
        if (c == '!')
          rv = true;
      }
    }
  }
  return rv;
}

/// Checks "ratio" attribute, if any, and sets enum type.
static void setRatio(graph_t *g) {
  char *p;
  double ratio;

  if ((p = agget(g, "ratio"))) {
    if (streq(p, "auto")) {
      GD_drawing(g)->ratio_kind = R_AUTO;
    } else if (streq(p, "compress")) {
      GD_drawing(g)->ratio_kind = R_COMPRESS;
    } else if (streq(p, "expand")) {
      GD_drawing(g)->ratio_kind = R_EXPAND;
    } else if (streq(p, "fill")) {
      GD_drawing(g)->ratio_kind = R_FILL;
    } else {
      ratio = atof(p);
      if (ratio > 0.0) {
        GD_drawing(g)->ratio_kind = R_VALUE;
        GD_drawing(g)->ratio = ratio;
      }
    }
  }
}

static inline void *my_gv_calloc(size_t nmemb, size_t size) {
  void *p = calloc(nmemb, size);
  assert(p != NULL);
  return p;
}

void my_graph_init(graph_t *g, bool use_rankdir) {
  char *p;
  double xf;
  static char *rankname[] = {"local", "global", "none", NULL};
  static int rankcode[] = {LOCAL, GLOBAL, NOCLUST, LOCAL};
  static char *fontnamenames[] = {"gd", "ps", "svg", NULL};
  static int fontnamecodes[] = {NATIVEFONTS, PSFONTS, SVGFONTS, -1};
  int rankdir;
  GD_drawing(g) = my_gv_calloc(1, sizeof(layout_t));

  p = agget(g, "charset");
  if (p != NULL) {
    if (!strncmp(p, "utf-8", 5) && !strncmp(p, "utf8", 4)) {
      agwarningf("Unsupported charset \"%s\" - assuming utf-8\n", p);
    }
  }
  GD_charset(g) = CHAR_UTF8;

  Gvimagepath = agget(g, "imagepath");
  if (!Gvimagepath) {
    Gvimagepath = Gvfilepath;
  }

  GD_drawing(g)->quantum =
      late_double(g, agfindgraphattr(g, "quantum"), 0.0, 0.0);

  /* setting rankdir=LR is only defined in dot,
   * but having it set causes shape code and others to use it.
   * The result is confused output, so we turn it off unless requested.
   * This effective rankdir is stored in the bottom 2 bits of g->u.rankdir.
   * Sometimes, the code really needs the graph's rankdir, e.g., neato -n
   * with record shapes, so we store the real rankdir in the next 2 bits.
   */
  rankdir = RANKDIR_TB;
  if ((p = agget(g, "rankdir"))) {
    if (streq(p, "LR"))
      rankdir = RANKDIR_LR;
    else if (streq(p, "BT"))
      rankdir = RANKDIR_BT;
    else if (streq(p, "RL"))
      rankdir = RANKDIR_RL;
  }
  if (use_rankdir)
    SET_RANKDIR(g, (rankdir << 2) | rankdir);
  else
    SET_RANKDIR(g, rankdir << 2);

  xf = late_double(g, agfindgraphattr(g, "nodesep"), DEFAULT_NODESEP,
                   MIN_NODESEP);
  GD_nodesep(g) = POINTS(xf);

  p = late_string(g, agfindgraphattr(g, "ranksep"), NULL);
  if (p) {
    if (sscanf(p, "%lf", &xf) == 0)
      xf = DEFAULT_RANKSEP;
    else {
      if (xf < MIN_RANKSEP)
        xf = MIN_RANKSEP;
    }
    if (strstr(p, "equally"))
      GD_exact_ranksep(g) = true;
  } else
    xf = DEFAULT_RANKSEP;
  GD_ranksep(g) = POINTS(xf);

  {
    int showboxes = late_int(g, agfindgraphattr(g, "showboxes"), 0, 0);
    if (showboxes > UCHAR_MAX) {
      showboxes = UCHAR_MAX;
    }
    GD_showboxes(g) = (unsigned char)showboxes;
  }
  p = late_string(g, agfindgraphattr(g, "fontnames"), NULL);
  GD_fontnames(g) = maptoken(p, fontnamenames, fontnamecodes);

  setRatio(g);
  GD_drawing(g)->filled = getdoubles2ptf(g, "size", &GD_drawing(g)->size);
  getdoubles2ptf(g, "page", &GD_drawing(g)->page);

  GD_drawing(g)->centered = mapbool(agget(g, "center"));

  if ((p = agget(g, "rotate")))
    GD_drawing(g)->landscape = atoi(p) == 90;
  else if ((p = agget(g, "orientation")))
    GD_drawing(g)->landscape = p[0] == 'l' || p[0] == 'L';
  else if ((p = agget(g, "landscape")))
    GD_drawing(g)->landscape = mapbool(p);

  p = agget(g, "clusterrank");
  CL_type = maptoken(p, rankname, rankcode);
  p = agget(g, "concentrate");
  Concentrate = mapbool(p);
  State = GVBEGIN;
  EdgeLabelsDone = 0;

  GD_drawing(g)->dpi = 0.0;
  if (((p = agget(g, "dpi")) && p[0]) || ((p = agget(g, "resolution")) && p[0]))
    GD_drawing(g)->dpi = atof(p);

  do_graph_label(g);

  Initial_dist = MYHUGE;

  G_ordering = agfindgraphattr(g, "ordering");
  G_gradientangle = agfindgraphattr(g, "gradientangle");
  G_margin = agfindgraphattr(g, "margin");

  /* initialize nodes */
  N_height = agfindnodeattr(g, "height");
  N_width = agfindnodeattr(g, "width");
  N_shape = agfindnodeattr(g, "shape");
  N_color = agfindnodeattr(g, "color");
  N_fillcolor = agfindnodeattr(g, "fillcolor");
  N_style = agfindnodeattr(g, "style");
  N_fontsize = agfindnodeattr(g, "fontsize");
  N_fontname = agfindnodeattr(g, "fontname");
  N_fontcolor = agfindnodeattr(g, "fontcolor");
  N_label = agfindnodeattr(g, "label");
  if (!N_label)
    N_label = agattr_text(g, AGNODE, "label", NODENAME_ESC);
  N_xlabel = agfindnodeattr(g, "xlabel");
  N_showboxes = agfindnodeattr(g, "showboxes");
  N_penwidth = agfindnodeattr(g, "penwidth");
  N_ordering = agfindnodeattr(g, "ordering");
  /* attribs for polygon shapes */
  N_sides = agfindnodeattr(g, "sides");
  N_peripheries = agfindnodeattr(g, "peripheries");
  N_skew = agfindnodeattr(g, "skew");
  N_orientation = agfindnodeattr(g, "orientation");
  N_distortion = agfindnodeattr(g, "distortion");
  N_fixed = agfindnodeattr(g, "fixedsize");
  N_imagescale = agfindnodeattr(g, "imagescale");
  N_imagepos = agfindnodeattr(g, "imagepos");
  N_nojustify = agfindnodeattr(g, "nojustify");
  N_layer = agfindnodeattr(g, "layer");
  N_group = agfindnodeattr(g, "group");
  N_comment = agfindnodeattr(g, "comment");
  N_vertices = agfindnodeattr(g, "vertices");
  N_z = agfindnodeattr(g, "z");
  N_gradientangle = agfindnodeattr(g, "gradientangle");

  /* initialize edges */
  E_weight = agfindedgeattr(g, "weight");
  E_color = agfindedgeattr(g, "color");
  E_fillcolor = agfindedgeattr(g, "fillcolor");
  E_fontsize = agfindedgeattr(g, "fontsize");
  E_fontname = agfindedgeattr(g, "fontname");
  E_fontcolor = agfindedgeattr(g, "fontcolor");
  E_label = agfindedgeattr(g, "label");
  E_xlabel = agfindedgeattr(g, "xlabel");
  E_label_float = agfindedgeattr(g, "labelfloat");
  E_dir = agfindedgeattr(g, "dir");
  E_headlabel = agfindedgeattr(g, "headlabel");
  E_taillabel = agfindedgeattr(g, "taillabel");
  E_labelfontsize = agfindedgeattr(g, "labelfontsize");
  E_labelfontname = agfindedgeattr(g, "labelfontname");
  E_labelfontcolor = agfindedgeattr(g, "labelfontcolor");
  E_labeldistance = agfindedgeattr(g, "labeldistance");
  E_labelangle = agfindedgeattr(g, "labelangle");
  E_minlen = agfindedgeattr(g, "minlen");
  E_showboxes = agfindedgeattr(g, "showboxes");
  E_style = agfindedgeattr(g, "style");
  E_decorate = agfindedgeattr(g, "decorate");
  E_arrowsz = agfindedgeattr(g, "arrowsize");
  E_constr = agfindedgeattr(g, "constraint");
  E_layer = agfindedgeattr(g, "layer");
  E_comment = agfindedgeattr(g, "comment");
  E_tailclip = agfindedgeattr(g, "tailclip");
  E_headclip = agfindedgeattr(g, "headclip");
  E_penwidth = agfindedgeattr(g, "penwidth");

  /* background */
  GD_drawing(g)->xdots = init_xdot(g);

  /* initialize id, if any */
  if ((p = agget(g, "id")) && *p)
    GD_drawing(g)->id = strdup_and_subst_obj(p, g);
}

extern void dot_layout(graph_t *g);
extern void dot_cleanup(graph_t *g);
extern gvlayout_features_t dotgen_features;
/* gvLayoutJobs:
 * Layout input graph g based on layout engine attached to gvc.
 * Check that the root graph has been initialized. If not, initialize it.
 * Return 0 on success.
 */
int my_gvLayoutJobs(GVC_t *gvc, Agraph_t *g) {
  agbindrec(g, "Agraphinfo_t", sizeof(Agraphinfo_t), true);
  GD_gvc(g) = gvc;
  if (g != agroot(g)) {
    agbindrec(agroot(g), "Agraphinfo_t", sizeof(Agraphinfo_t), true);
    GD_gvc(agroot(g)) = gvc;
  }

  my_graph_init(g, !!(dotgen_features.flags & LAYOUT_USES_RANKDIR));
  GD_drawing(agroot(g)) = GD_drawing(g);
  dot_layout(g);

  GD_cleanup(g) = dot_cleanup;

  return 0;
}

int gw_gvLayoutDot(GVC_t *gvc, Agrw_t graph) {
  graph_t *g = (graph_t *)graph;

  // FIXME: handle "layout" attribute on graph
  char *p;
  if ((p = agget(g, "layout"))) {
    if (strncmp(p, "dot", 3)) {
      agerrorf("Layout type: \"%s\" not recognized. Use one of: dot\n", p);
      return -1;
    }
  }

  if (my_gvLayoutJobs(gvc, g) == -1)
    return -1;

  /* set bb attribute for basic layout.
   * doesn't yet include margins, scaling or page sizes because
   * those depend on the renderer being used. */
  char buf[256];
  if (GD_drawing(g)->landscape)
    snprintf(buf, sizeof(buf), "%.0f %.0f %.0f %.0f", round(GD_bb(g).LL.y),
             round(GD_bb(g).LL.x), round(GD_bb(g).UR.y), round(GD_bb(g).UR.x));
  else
    snprintf(buf, sizeof(buf), "%.0f %.0f %.0f %.0f", round(GD_bb(g).LL.x),
             round(GD_bb(g).LL.y), round(GD_bb(g).UR.x), round(GD_bb(g).UR.y));
  agsafeset(g, "bb", buf, "");

  return 0;
}

extern void graph_cleanup(graph_t *g);
int gw_gvFreeLayout(Agrw_t graph) {
  graph_t *g = (graph_t *)graph;

  /* skip if no Agraphinfo_t yet */
  if (!agbindrec(g, "Agraphinfo_t", 0, true))
    return 0;

  dot_cleanup(g);

  graph_cleanup(g);
  return 0;
}
