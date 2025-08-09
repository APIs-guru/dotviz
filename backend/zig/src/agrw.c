#include "agrw.h"
#include "cgraph.h"
#include "gvc.h"

extern gvplugin_library_t gvplugin_core_LTX_library;
extern gvplugin_library_t gvplugin_dot_layout_LTX_library;

lt_symlist_t lt_preloaded_symbols[] = {
    {"gvplugin_core_LTX_library", &gvplugin_core_LTX_library},
    {"gvplugin_dot_layout_LTX_library", &gvplugin_dot_layout_LTX_library},
    {0, 0}};

GVC_t *gw_create_context(void) {
  return gvContextPlugins(lt_preloaded_symbols, 0);
}

Agrw_t gw_agopen(const char *name, enum Agrw_graph_type graph_type) {
  Agdesc_t type = Agdirected;
  switch (graph_type) {
  case Agrw_directed:
    type = Agdirected;
    break;
  case Agrw_strictdirected:
    type = Agstrictdirected;
    break;
  case Agrw_undirected:
    type = Agundirected;
    break;
  case Agrw_strictundirected:
    type = Agstrictundirected;
    break;
  }

  return (Agrw_t)agopen((char *)name, type, 0);
}

int gw_agclose(Agrw_t graph) { return agclose((Agraph_t *)graph); }

int gw_gvFreeLayout(GVC_t *gvc, Agrw_t graph) {
  return gvFreeLayout(gvc, (Agraph_t *)graph);
}

Agrw_t gw_agmemread(const char *cp) { return (Agrw_t)agmemread(cp); }

// undirected
#include "const.h"
#include "gvcint.h" // IWYU pragma: keep
// undirected

#include "gvcproc.h"

int gw_gvLayoutDot(GVC_t *gvc, Agrw_t graph) {
  graph_t *g = (graph_t *)graph;
  const char* engine = "dot";
  char buf[256];
  int rc;

  rc = gvlayout_select(gvc, engine);
  if (rc == NO_SUPPORT) {
    agerrorf("Layout type: \"%s\" not recognized. Use one of:%s\n", engine,
             gvplugin_list(gvc, API_layout, engine));
    return -1;
  }

  if (gvLayoutJobs(gvc, g) == -1)
    return -1;

  /* set bb attribute for basic layout.
   * doesn't yet include margins, scaling or page sizes because
   * those depend on the renderer being used. */
  if (GD_drawing(g)->landscape)
    snprintf(buf, sizeof(buf), "%.0f %.0f %.0f %.0f", round(GD_bb(g).LL.y),
             round(GD_bb(g).LL.x), round(GD_bb(g).UR.y), round(GD_bb(g).UR.x));
  else
    snprintf(buf, sizeof(buf), "%.0f %.0f %.0f %.0f", round(GD_bb(g).LL.x),
             round(GD_bb(g).LL.y), round(GD_bb(g).UR.x), round(GD_bb(g).UR.y));
  agsafeset(g, "bb", buf, "");

  return 0;
}

bool gw_gvLayoutDone(GVC_t *gvc, Agrw_t graph) {
  return gvLayoutDone((Agraph_t *)graph);
}

int gw_gvRenderDataSvg(GVC_t *gvc, Agrw_t graph, char **result,
                       size_t *length) {
  return gvRenderData(gvc, (Agraph_t *)graph, "svg", result, length);
}

void gw_gvFreeRenderData(char *data) { gvFreeRenderData(data); }

Agrw_node_t gw_agnode(Agrw_t graph, const char *name) {
  return (Agrw_node_t)agnode((Agraph_t *)graph, (char *)name, 1);
}

Agrw_node_t gw_agfstnode(Agrw_t graph) {
  return (Agrw_node_t)agfstnode((Agraph_t *)graph);
}

Agrw_node_t gw_agnxtnode(Agrw_t graph, Agrw_node_t node) {
  return (Agrw_node_t)agnxtnode((Agraph_t *)graph, (Agnode_t *)node);
}

Agrw_edge_t gw_agedge(Agrw_t graph, Agrw_node_t tail, Agrw_node_t head) {
  return (Agrw_edge_t)agedge((Agraph_t *)graph, (Agnode_t *)tail,
                             (Agnode_t *)head, NULL, 1);
}

const char *gw_agnameof_graph(Agrw_t graph) {
  return agnameof((Agraph_t *)graph);
}

const char *gw_agnameof_node(Agrw_node_t node) {
  return agnameof((Agnode_t *)node);
}

const char *gw_agnameof_edge(Agrw_edge_t edge) {
  return agnameof((Agedge_t *)edge);
}
