#ifndef RENDER_INLINE_H
#define RENDER_INLINE_H

#include "agrw.h"
#include "output_string.h"

void gw_gvRenderData(GVC_t *gvc, Agrw_t graph, char **result, size_t *length);
void gw_gvFreeRenderData(char *data);
output_string render_dot(Agrw_t g, char **result, size_t *length);

#endif