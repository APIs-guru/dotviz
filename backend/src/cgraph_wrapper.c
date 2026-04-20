#include "cgraph.h"
#include "types.h"

Agraph_t *wrapped_agopen(const char *name, bool directed, bool strict) {
  Agdesc_t desc = {.directed = directed, .strict = strict};
  return agopen((char *)name, desc, NULL);
}

void set_gvc_to_null(Agraph_t *g) { GD_gvc(g) = NULL; }

void wrapped_sym_set_print(Agsym_t* sym) {
    sym->print = 1;
}
