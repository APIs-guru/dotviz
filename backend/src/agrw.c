#include "agrw.h"
#include "gvc.h"

Agrw_t gw_agopen(const char *name, bool directed, bool strict) {
  Agdesc_t desc = {.directed = directed, .strict = strict};
  return (Agrw_t)agopen((char *)name, desc, NULL);
}

int gw_agclose(Agrw_t graph) { return agclose((Agraph_t *)graph); }

Agrw_t gw_agmemread(const char *cp) { return (Agrw_t)agmemread(cp); }

void gw_set_default_attr_text(Agrw_t graph, int kind, char *name, const char *value) {
  if (agattr_text(graph, kind, name, NULL) == NULL) {
    agattr_text(graph, kind, name, "");
  }
  agattr_text((Agraph_t *)graph, kind, name, value);
}

void gw_set_default_attr_html(Agrw_t graph, int kind, char *name, const char *value) {
  if (agattr_text(graph, kind, name, NULL) == NULL) {
    agattr_text(graph, kind, name, "");
  }
  agattr_html((Agraph_t *)graph, kind, name, value);
}

void gw_agsafeset_text(void *object, char *name, const char *value) {
  agsafeset_text(object, name, value, "");
}

void gw_agsafeset_html(void *object, char *name, const char *value) {
  agsafeset_html(object, name, value, "");
}

void *gw_agnode(Agrw_t graph, const char *name) {
  return (void *)agnode((Agraph_t *)graph, (char *)name, true);
}

void *gw_agedge(Agrw_t graph, void *tail, void *head) {
  return (void *)agedge((Agraph_t *)graph, (Agnode_t *)tail, (Agnode_t *)head,
                        NULL, 1);
}

void *gw_agsubg(Agrw_t graph, const char *name) {
  return agsubg((Agraph_t *)graph, (char *)name, true);
}
