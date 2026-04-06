#ifndef LAYOUT_INLINE_H
#define LAYOUT_INLINE_H

typedef struct GVC_s GVC_t;
typedef struct Agraph_s Agraph_t;

int gw_gvLayoutDot(GVC_t *gvc, Agraph_t* graph);
void gw_gvFreeLayout(Agraph_t* graph);
int gw_gvLayout(GVC_t *gvc, Agraph_t *g, const char *engine);

#endif
