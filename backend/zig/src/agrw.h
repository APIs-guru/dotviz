#ifndef AGRW_H_
#define AGRW_H_

#include <stdint.h>

typedef uintptr_t Agrw_t;
typedef uintptr_t Agrw_node_t;
typedef uintptr_t Agrw_edge_t;

enum Agrw_graph_type {
  Agrw_directed,
  Agrw_strictdirected,
  Agrw_undirected,
  Agrw_strictundirected
};

Agrw_t gw_agopen(const char *name, enum Agrw_graph_type graph_type);
int gw_agclose(Agrw_t graph);

Agrw_node_t gw_agnode(Agrw_t graph, const char *name);
Agrw_node_t gw_agfstnode(Agrw_t graph);
Agrw_node_t gw_agnxtnode(Agrw_t graph, Agrw_node_t node);

Agrw_edge_t gw_agedge(Agrw_t graph, Agrw_node_t tail, Agrw_node_t head);

const char *gw_agnameof_graph(Agrw_t graph);
const char *gw_agnameof_node(Agrw_node_t node);
const char *gw_agnameof_edge(Agrw_edge_t edge);

#endif