#ifndef CORE_SVG_H
#define CORE_SVG_H

#include "geom.h"
#include "gvcext.h"
#include "gvcjob.h"
#include "textspan.h"
#include <stdbool.h>
#include <stddef.h>
#include "../output_string.h"
#include "types.h"

void jobsvg_bezier(GVJ_t *job, pointf *A, size_t n, int filled);
void jobsvg_polygon(GVJ_t *job, pointf *A, size_t n, int filled);
void jobsvg_ellipse(GVJ_t *job, pointf *A, int filled);
void jobsvg_polyline(GVJ_t *job, pointf *A, size_t n);
void jobsvg_textspan(GVJ_t *job, pointf p, textspan_t *span);
void jobsvg_box(GVJ_t *job, boxf B, int filled);
void jobsvg_begin_node(GVJ_t *job);
void jobsvg_end_node(GVJ_t *job);
void jobsvg_comment(GVJ_t *job, char *str);
void jobsvg_begin_edge(GVJ_t *job);
void jobsvg_begin_anchor(GVJ_t *job, char *href, char *tooltip, char *target,
                         char *id);
void jobsvg_end_anchor(GVJ_t *job);
void jobsvg_end_edge(GVJ_t *job);
void jobsvg_begin_graph(GVJ_t *job);
void jobsvg_end_graph(GVJ_t *job);
void jobsvg_begin_page(GVJ_t *job);
void jobsvg_end_page(GVJ_t *job);
void jobsvg_begin_cluster(GVJ_t *job);
void jobsvg_end_cluster(GVJ_t *job);
void jobsvg_begin_layer(GVJ_t *job, char *layername, int layerNum,
                        int numLayers);
void jobsvg_end_layer(GVJ_t *job);

void jobsvg_begin_job(GVJ_t *job);

void jobsvg_usershape(GVJ_t *job, char *name, pointf *a, size_t n,
                      char *imagescale, char *imagepos);
void jobsvg_set_pencolor(GVJ_t *job, char *name);
void jobsvg_set_fillcolor(GVJ_t *job, char *name);
void jobsvg_set_gradient_vals(GVJ_t *job, char *stopcolor, int angle,
                              double frac);
void jobsvg_set_style(GVJ_t *job, char **s);

void svg_bezier(output_string *output, obj_state_t *obj, pointf *A, size_t n,
                int filled);
void svg_polygon(output_string *output, obj_state_t *obj, pointf *A, size_t n,
                 int filled);
void svg_ellipse(output_string *output, obj_state_t *obj, pointf *pf,
                 int filled);
void svg_polyline(output_string *output, obj_state_t *obj, pointf *A, size_t n);
void svg_textspan(output_string *output, fontname_kind fontnames,
                  obj_state_t *obj, pointf p, textspan_t *span);
void svg_box(output_string *output, obj_state_t *obj, boxf B, int filled);
// void svg_begin_node(GVJ_t *job);
void svg_end_node(output_string *output);
// void svg_comment(GVJ_t *job, char *str);
void svg_begin_edge(output_string *output, obj_state_t *obj);
void svg_begin_anchor(output_string *output, char *href, char *tooltip,
                      char *target, char *id);
void svg_end_anchor(output_string *output);
void svg_end_edge(output_string *output);
// void svg_begin_graph(GVJ_t *job);
// void svg_end_graph(GVJ_t *job);
// void svg_begin_page(GVJ_t *job);
// void svg_end_page(GVJ_t *job);
// void svg_begin_cluster(GVJ_t *job);
// void svg_end_cluster(GVJ_t *job);
// void svg_begin_layer(GVJ_t *job, char *layername, int layerNum, int
// numLayers); void svg_end_layer(GVJ_t *job);

// void svg_begin_job(GVJ_t *job);

void svg_usershape(output_string *output, int rotation_deg, pointf dpi,
                   char *name, pointf *a, size_t n, char *imagescale,
                   char *imagepos);
void svg_set_pencolor(obj_state_t *obj, char *name);
void svg_set_fillcolor(obj_state_t *obj, char *name);
void svg_set_gradient_vals(obj_state_t *obj, char *stopcolor, int angle,
                           double frac);
void svg_set_style(obj_state_t *obj, char **s);
void svg_set_penwidth(obj_state_t *obj, double penwidth);
#endif
