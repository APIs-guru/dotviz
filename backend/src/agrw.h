#ifndef AGRW_H_
#define AGRW_H_

#include "gvcext.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *Agrw_t;

GVC_t *gw_create_context(void);
int gw_gvLayoutDot(GVC_t *gvc, Agrw_t graph);

Agrw_t gw_agopen(const char *name, bool directed, bool stricted);
int gw_agclose(Agrw_t graph);
Agrw_t gw_agmemread(const char *cp);

void gw_agattr_text(Agrw_t graph, int kind, char *name, const char *value);
void gw_agattr_html(Agrw_t graph, int kind, char *name, const char *value);
void gw_agsafeset_text(void *object, char *name, const char *value);
void gw_agsafeset_html(void *object, char *name, const char *value);

void *gw_agnode(Agrw_t graph, const char *name);
void *gw_agedge(Agrw_t graph, void *tail, void *head);
void *gw_agsubg(Agrw_t graph, const char *name);

#endif
