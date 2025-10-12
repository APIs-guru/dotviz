
#include "gv_char_classes.h"
#include "output_string.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void my_attach_attrs_and_arrows(graph_t *g);

extern output_string my_agwrite(Agraph_t *g,
                                unsigned long max_output_linelength);
void render_dot(Agraph_t *g, char **result, size_t *length) {
  char *linelength = agget(g, "linelength");
  unsigned long max_len = 0;
  if (linelength != NULL && gv_isdigit(*linelength)) {
    max_len = strtoul(linelength, NULL, 10);
  }

  // agwarningf("pagedir=%s ignored\n", gvc->pagedir);

  // GVC_t* gvc_ = GD_gvc(g);
  // GD_gvc(g) = NULL;
  my_attach_attrs_and_arrows(g);

  /* reset node state */
  for (node_t *n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;

  output_string output = my_agwrite(g, max_len);
  // GD_gvc(g) = gvc_;

  *result = output.data;
  *length = output.data_position;
}