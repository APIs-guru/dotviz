/**
 * @file
 * @ingroup common_render
 */
/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#pragma once

#include <stdbool.h>

#include "types.h"
#include "gvcjob.h"
#include "agxbuf.h"

#include "core_svg.h"

typedef struct SafeJob_s SafeJob;

void arrow_flags(Agedge_t *e, uint32_t *sflag, uint32_t *eflag);
boxf arrow_bb(pointf p, pointf u, double arrowsize);
void arrow_gen(output_string *output, obj_state_t *obj, emit_state_t emit_state,
               pointf p, pointf u, double arrowsize, double penwidth,
               uint32_t flag);
char *getObjId(const SafeLayer *safe_layer, void *obj, agxbuf *xb);
void bezier_clip(inside_t *inside_context,
                 bool (*insidefn)(inside_t *inside_context, pointf p),
                 pointf *sp, bool left_inside);
Ppolyline_t *ellipticWedge(pointf ctr, double major, double minor,
                           double angle0, double angle1);
output_string emit_graph(SafeJob *safe_job, graph_t *g, int graph_outputorder);
void emit_label(output_string *output, SafeLayer *safe_layer, obj_state_t *obj,
                emit_state_t emit_state, textlabel_t *lp);
bool emit_once(char *message);
void epsf_init(node_t *n);
void epsf_free(node_t *n);
void free_label(textlabel_t *);
void free_textspan(textspan_t *tl, size_t);
bool isPolygon(node_t *);
textlabel_t *make_label(void *obj, char *str, int kind, double fontsize,
                        char *fontname, char *fontcolor);
char **parse_style(char *s);
obj_state_t child_obj_state(obj_state_t *parent);
void free_child_obj(obj_state_t *child);
port resolvePort(node_t *n, node_t *other, port *oldport);
void make_simple_label(GVC_t *gvc, textlabel_t *rv);
int stripedBox(output_string *output, obj_state_t *obj, pointf *AF,
               const char *clrs, int rotate);
stroke_t taper(bezier *, double (*radfunc_t)(double, double, double),
               double initwid);
pointf textspan_size(GVC_t *gvc, textspan_t *span);
int wedgedEllipse(output_string *output, obj_state_t *obj, pointf *pf,
                  const char *clrs);
void init_bb(graph_t *g);
void rounded_svg_box(output_string *output, obj_state_t *obj, boxf B,
                     svg_fill_type_t fill_type);
