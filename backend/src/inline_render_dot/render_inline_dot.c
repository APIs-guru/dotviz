
#include "../gv_char_classes.h"
#include "../output_string.h"
#include "types.h"
#include <limits.h>
#include <stdlib.h>

#define MAX_OUTPUTLINE 128
#define MIN_OUTPUTLINE 60

extern void my_attach_attrs_and_arrows(graph_t *g);

extern output_string my_agwrite(Agraph_t *g,
                                unsigned long max_output_linelength);
output_string render_dot(Agraph_t *g) {
  if (agget(g, "layers") != 0) {
    agwarningf("layers not supported in dot output\n");
  }
  char *linelength = agget(g, "linelength");
  unsigned long max_output_linelength = MAX_OUTPUTLINE;
  if (linelength != NULL && gv_isdigit(*linelength)) {
    unsigned long num = strtoul(linelength, NULL, 10);
    if ((num == 0 || num >= MIN_OUTPUTLINE) && num <= INT_MAX) {
      max_output_linelength = (int)max_output_linelength;
    }
  }

  my_attach_attrs_and_arrows(g);

  /* reset node state */
  for (node_t *n = agfstnode(g); n; n = agnxtnode(g, n))
    ND_state(n) = 0;

  output_string output = my_agwrite(g, max_output_linelength);

  return output;
}
