#include "cgraph.h"
#include "types.h"
#include <stdlib.h>

Agraph_t *wrapped_agopen(const char *name, bool directed) {
  Agdesc_t desc = {.directed = directed, .strict = false};
  return agopen((char *)name, desc, NULL);
}

Agraphinfo_t* graphInfo(Agraph_t *g) {
  return (Agraphinfo_t *)g->base.data;
}

Agnodeinfo_t* nodeInfoPtr(Agnode_t *n) {
  return (Agnodeinfo_t *)n->base.data;
}
