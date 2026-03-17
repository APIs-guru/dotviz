#ifndef AGRW_H_
#define AGRW_H_

#include "geom.h"
#include "gvcext.h"
#include "output_string.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *Agrw_t;

GVC_t *gw_create_context(void);
int gw_gvLayout(GVC_t *gvc, Agrw_t graph, const char *engine);
point my_gvusershape_size(Agrw_t graph, const char *height, const char *width);

output_string render_svg(Agrw_t graph);
output_string render_dot(Agrw_t g);

Agrw_t gw_agopen(const char *name, bool directed, bool stricted);
int gw_agclose(Agrw_t graph);
Agrw_t gw_agmemread(const char *cp);

void gw_set_default_attr_text(Agrw_t graph, int kind, char *name, const char *value);
void gw_set_default_attr_html(Agrw_t graph, int kind, char *name, const char *value);
void gw_agsafeset_text(void *object, char *name, const char *value);
void gw_agsafeset_html(void *object, char *name, const char *value);

void *gw_agnode(Agrw_t graph, const char *name);
void *gw_agedge(Agrw_t graph, void *tail, void *head);
void *gw_agsubg(Agrw_t graph, const char *name);

#endif
