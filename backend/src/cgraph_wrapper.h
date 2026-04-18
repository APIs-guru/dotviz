#ifndef CGRAPH_WRAPPER_H_
#define CGRAPH_WRAPPER_H_

#include <stdbool.h>

typedef int (*agusererrf)(char *);
agusererrf agseterrf(agusererrf);

typedef enum { AGWARN, AGERR, AGMAX, AGPREV } agerrlevel_t;
agerrlevel_t agseterr(agerrlevel_t);

int agreseterrors(void);

enum { AGRAPH, AGNODE, AGEDGE, AGOUTEDGE = AGEDGE, AGINEDGE };

typedef struct Agnode_s Agnode_t;
typedef struct Agedge_s Agedge_t;
typedef struct Agraph_s Agraph_t;
typedef struct Agraph_s graph_t;
typedef struct Agsym_s Agsym_t;

Agraph_t *agmemread(const char *cp);
int agclose(Agraph_t *g);
Agraph_t *wrapped_agopen(const char *name, bool directed, bool strict);

void set_gvc_to_null(Agraph_t *g);

const char *agget(void *obj, const char *name);
Agraph_t *agsubg(Agraph_t *g, const char *name, int cflag);
Agnode_t *agnode(Agraph_t *g, const char *name, int createflag);
Agedge_t *agedge(Agraph_t *g, Agnode_t *t, Agnode_t *h, char *name,
                 int createflag);
Agsym_t *agattr_text(Agraph_t *g, int kind, char *name, const char *value);
Agsym_t *agattr_html(Agraph_t *g, int kind, char *name, const char *value);
Agnode_t *agsubnode(Agraph_t *g, Agnode_t *n, int createflag);

int agsafeset_text(void *obj, char *name, const char *value, const char *def);
int agsafeset_html(void *obj, char *name, const char *value, const char *def);

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

void graph_cleanup(graph_t *g);

#endif /* CGRAPH_WRAPPER_H_ */
