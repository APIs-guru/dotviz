#ifndef CORE_SVG_H
#define CORE_SVG_H

#include "geom.h"
#include "gvcext.h"
#include "textspan.h"
#include <stddef.h>
pointf *svg_ptf_A(GVJ_t *job, pointf *af, pointf *AF, size_t n);
void svg_bezier(GVJ_t *job, pointf *A, size_t n, int filled);
void svg_polygon(GVJ_t *job, pointf *A, size_t n, int filled);
void svg_ellipse(GVJ_t *job, pointf *A, int filled);
void svg_polyline(GVJ_t *job, pointf *A, size_t n);
void svg_textspan(GVJ_t *job, pointf p, textspan_t *span);
void svg_box(GVJ_t *job, boxf B, int filled);
void svg_begin_node(GVJ_t *job);
void svg_end_node(GVJ_t *job);
void svg_comment(GVJ_t *job, char *str);
void svg_begin_edge(GVJ_t *job);
void svg_begin_anchor(GVJ_t *job, char *href, char *tooltip, char *target,
                      char *id);
void svg_end_anchor(GVJ_t *job);
void svg_end_edge(GVJ_t *job);
void svg_begin_graph(GVJ_t *job);
void svg_end_graph(GVJ_t *job);
void svg_begin_page(GVJ_t *job);
void svg_end_page(GVJ_t *job);
void svg_begin_cluster(GVJ_t *job);
void svg_end_cluster(GVJ_t *job);
void svg_begin_layer(GVJ_t *job, char *layername, int layerNum, int numLayers);
void svg_end_layer(GVJ_t *job);

void svg_begin_job(GVJ_t *job);

#endif
