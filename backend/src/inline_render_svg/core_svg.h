#ifndef CORE_SVG_H
#define CORE_SVG_H

#include <stddef.h>
#include "types.h"

typedef struct color_s gvcolor_t;

void svg_print_id(output_string *output, char *id, char *idx);
void svg_print_class(output_string *output, char *kind, void *obj);
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

void svg_comment(output_string *output, char *str);
void svg_begin_anchor(output_string *output, char *href, char *tooltip,
                      char *target, char *id);
void svg_end_anchor(output_string *output);
void svg_usershape(output_string *output, int rotation_deg, pointf dpi,
                   char *name, pointf *a, size_t n, imagescale_t imagescale,
                   imagepos_t imagepos);
gvcolor_t svg_resolve_color(char *name);
void svg_set_style(obj_state_t *obj, char **s);

imagescale_t get_imagescale(char *s);
imagepos_t get_imagepos(char *s);

#define LOCALNAMEPREFIX '%'

#endif
