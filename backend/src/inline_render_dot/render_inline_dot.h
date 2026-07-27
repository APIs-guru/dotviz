#include "../output_string.h"

typedef struct Agraph_s Agraph_t;
void my_attach_attrs_and_arrows(Agraph_t *g);
output_string my_agwrite(Agraph_t *g, unsigned int max_output_linelength);
