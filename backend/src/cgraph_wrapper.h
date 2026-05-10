#ifndef CGRAPH_WRAPPER_H_
#define CGRAPH_WRAPPER_H_

#include <stdbool.h>
#include "cgraph.h"
#include "types.h"

Agraph_t *wrapped_agopen(const char *name, bool directed, bool strict);

void set_gvc_to_null(Agraph_t *g);

void dot_layout(graph_t *g);
void dot_cleanup(graph_t *g);

void circo_layout(Agraph_t *g);
void circo_cleanup(graph_t *g);

void neato_layout(graph_t *g);
void neato_cleanup(graph_t *g);

void fdp_layout(graph_t *g);
void fdp_cleanup(graph_t *g);

void twopi_layout(graph_t *g);
void twopi_cleanup(graph_t *g);

void patchwork_layout(graph_t *g);
void patchwork_cleanup(graph_t *g);

void osage_layout(graph_t *g);
void osage_cleanup(graph_t *g);

void sfdp_layout(graph_t *g);
void sfdp_cleanup(graph_t *g);

void graph_cleanup(graph_t *g);

#endif /* CGRAPH_WRAPPER_H_ */
