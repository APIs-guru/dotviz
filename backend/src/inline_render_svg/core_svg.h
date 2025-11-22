#ifndef CORE_SVG_H
#define CORE_SVG_H

#include "geom.h"
#include "gvcext.h"
#include "safe_job.h"
#include "textspan.h"
#include <stdbool.h>
#include <stddef.h>
#include "../output_string.h"
#include "types.h"

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
void svg_begin_node(output_string *output, SafeLayer *safe_layer,
                    obj_state_t *obj);
void svg_end_node(output_string *output);
void svg_comment(output_string *output, char *str);
void svg_begin_edge(output_string *output, obj_state_t *obj);
void svg_begin_anchor(output_string *output, char *href, char *tooltip,
                      char *target, char *id);
void svg_end_anchor(output_string *output);
void svg_end_edge(output_string *output);
void svg_begin_graph(output_string *output, SafeJob *safe_job,
                     obj_state_t *obj);
void svg_end_graph(output_string *output);
void svg_begin_page(output_string *output, SafeLayer *safe_layer,
                    obj_state_t *obj);
void svg_end_page(output_string *output);
void svg_begin_cluster(output_string *output, obj_state_t *obj);
void svg_end_cluster(output_string *output);
void svg_begin_layer(output_string *output, obj_state_t *obj, char *layername);
void svg_end_layer(output_string *output);
void svg_begin_job(output_string *output, const char *stylesheet);
void svg_usershape(output_string *output, int rotation_deg, pointf dpi,
                   char *name, pointf *a, size_t n, char *imagescale,
                   char *imagepos);
void svg_set_pencolor(obj_state_t *obj, char *name);
void svg_set_fillcolor(obj_state_t *obj, char *name);
void svg_set_gradient_vals(obj_state_t *obj, char *stopcolor, int angle,
                           double frac);
void svg_set_style(obj_state_t *obj, char **s);
void svg_set_penwidth(obj_state_t *obj, double penwidth);

extern char *svg_defaultlinestyle[3];

#endif
