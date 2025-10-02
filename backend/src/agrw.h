#ifndef AGRW_H_
#define AGRW_H_

#include "gvcext.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *Agrw_t;
typedef uintptr_t Agrw_node_t;
typedef uintptr_t Agrw_edge_t;

enum Agrw_graph_type {
  Agrw_directed,
  Agrw_strictdirected,
  Agrw_undirected,
  Agrw_strictundirected
};

GVC_t *gw_create_context(void);
Agrw_t gw_agopen(const char *name, bool directed, bool stricted);
int gw_agclose(Agrw_t graph);
Agrw_t gw_agmemread(const char *cp);
int gw_gvLayoutDot(GVC_t *gvc, Agrw_t graph);
bool gw_gvLayoutDone(GVC_t *gvc, Agrw_t graph);

void gw_agattr_text(Agrw_t graph, int kind, char *name, const char *value);
void gw_agattr_html(Agrw_t graph, int kind, char *name, const char *value);
void gw_agsafeset_text(void *object, char *name, const char *value);
void gw_agsafeset_html(void *object, char *name, const char *value);
void *gw_agnode(Agrw_t graph, const char *name);
Agrw_node_t gw_agfstnode(Agrw_t graph);
Agrw_node_t gw_agnxtnode(Agrw_t graph, Agrw_node_t node);

void *gw_agedge(Agrw_t graph, void *tail, void *head);
void *gw_agsubg(Agrw_t graph, const char *name);

const char *gw_agnameof_graph(Agrw_t graph);
const char *gw_agnameof_node(Agrw_node_t node);
const char *gw_agnameof_edge(Agrw_edge_t edge);

#endif
