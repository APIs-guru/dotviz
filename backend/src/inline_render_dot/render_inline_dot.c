#include "../output_string.h"
#include "types.h"
#include <limits.h>
#include <stdlib.h>

extern void my_attach_attrs_and_arrows(graph_t *g);

extern output_string my_agwrite(Agraph_t *g,
                                unsigned int max_output_linelength);
output_string render_dot(Agraph_t *g, unsigned int max_output_linelength) {
  my_attach_attrs_and_arrows(g);

  /* reset node state */
  for (node_t *n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;

  output_string output = my_agwrite(g, max_output_linelength);

  return output;
}
