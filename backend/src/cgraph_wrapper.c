#include "cgraph.h"
#include "const.h"
#include "types.h"
#include <stdlib.h>

Agraph_t *wrapped_agopen(const char *name, bool directed, bool strict) {
  Agdesc_t desc = {.directed = directed, .strict = strict};
  return agopen((char *)name, desc, NULL);
}

void set_gvc_to_null(Agraph_t *g) { GD_gvc(g) = NULL; }

void wrapped_init_graph(Agraph_t *g) {
  agbindrec(g, "Agraphinfo_t", sizeof(Agraphinfo_t), true);
  GD_drawing(g) = calloc(1, sizeof(layout_t));
  GD_charset(g) = CHAR_UTF8;
}
