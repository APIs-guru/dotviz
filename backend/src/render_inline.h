#ifndef RENDER_INLINE_H
#define RENDER_INLINE_H

#include "agrw.h"

int gw_gvRenderData(GVC_t *gvc, Agrw_t graph, const char *format, char **result,
                    size_t *length);
void gw_gvFreeRenderData(char *data);

#endif