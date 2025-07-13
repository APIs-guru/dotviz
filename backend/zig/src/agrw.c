#include "agrw.h"
#include "cgraph.h"

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

int gw_agclose(Agrw_t graph) {
  return agclose((Agraph_t *)graph);
}

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
  return (Agrw_edge_t)agedge(
      (Agraph_t *)graph,
      (Agnode_t *)tail,
      (Agnode_t *)head,
      NULL, 1);
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
