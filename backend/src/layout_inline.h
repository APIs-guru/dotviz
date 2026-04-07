#ifndef LAYOUT_INLINE_H
#define LAYOUT_INLINE_H

#include <stdbool.h>

typedef struct GVC_s GVC_t;
typedef struct Agraph_s Agraph_t;

void my_graph_init(GVC_t *gvc, Agraph_t *g, bool use_rankdir);

#endif
