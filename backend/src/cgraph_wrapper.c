#include "cgraph.h"
#include "types.h"
#include <stdlib.h>

Agraph_t *wrapped_agopen(const char *name, bool directed, bool strict) {
  Agdesc_t desc = {.directed = directed, .strict = strict};
  return agopen((char *)name, desc, NULL);
}

Agraphinfo_t* graphInfo(Agraph_t *g) {
  return (Agraphinfo_t *)g->base.data;
}
