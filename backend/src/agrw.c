#include "agrw.h"
#include "gvc.h"


Agrw_t gw_agopen(const char *name, bool directed, bool strict) {
  Agdesc_t desc = {.directed = directed, .strict = strict};
  return (Agrw_t)agopen((char *)name, desc, NULL);
}

int gw_agclose(Agrw_t graph) { return agclose((Agraph_t *)graph); }

Agrw_t gw_agmemread(const char *cp) { return (Agrw_t)agmemread(cp); }

bool gw_gvLayoutDone(GVC_t *gvc, Agrw_t graph) {
  return gvLayoutDone((Agraph_t *)graph);
}

void gw_agattr_text(Agrw_t graph, int kind, char *name, const char *value) {
  agattr_text((Agraph_t *)graph, kind, name, value);
}

void gw_agattr_html(Agrw_t graph, int kind, char *name, const char *value) {
  agattr_html((Agraph_t *)graph, kind, name, value);
}

void *gw_agnode(Agrw_t graph, const char *name) {
  return (void *)agnode((Agraph_t *)graph, (char *)name, true);
}

Agrw_node_t gw_agfstnode(Agrw_t graph) {
  return (Agrw_node_t)agfstnode((Agraph_t *)graph);
}

Agrw_node_t gw_agnxtnode(Agrw_t graph, Agrw_node_t node) {
  return (Agrw_node_t)agnxtnode((Agraph_t *)graph, (Agnode_t *)node);
}

void *gw_agedge(Agrw_t graph, void *tail, void *head) {
  return (void *)agedge((Agraph_t *)graph, (Agnode_t *)tail, (Agnode_t *)head,
                        NULL, 1);
}

void *gw_agsubg(Agrw_t graph, const char *name) {
  return agsubg((Agraph_t *)graph, (char *)name, true);
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

void gw_agsafeset_text(void *object, char *name, const char *value) {
  agsafeset_text(object, name, value, "");
}

void gw_agsafeset_html(void *object, char *name, const char *value) {
  agsafeset_html(object, name, value, "");
}