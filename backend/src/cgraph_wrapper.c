#include "cgraph.h"

Agraph_t *wrapped_agopen(const char *name, bool directed, bool strict) {
  Agdesc_t desc = {.directed = directed, .strict = strict};
  return agopen((char *)name, desc, NULL);
}
