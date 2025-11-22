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

// clang-format off
#include "config.h"

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <util/agxbuf.h>

#include <inttypes.h>

#include "safe_job.h"
#include "types.h"
#include "macros.h"
#include "const.h"
#include "colorprocs.h"		/* must follow color.h (in types.h) */
#include "geomprocs.h"		/* must follow geom.h (in types.h) */
#include "utils.h"		/* must follow types.h and agxbuf.h */
#include "gvplugin.h"		/* must follow gvcext.h (in types.h) */
#include "gvcjob.h"		/* must follow gvcext.h (in types.h) */
#include "gvcint.h"		/* must follow gvcext.h (in types.h) */
#include "gvcproc.h"		/* must follow gvcext.h (in types.h) */
#include "../output_string.h"
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

typedef struct epsf_s {
  int macro_id;
  pointf offset;
} epsf_t;

#ifdef GVDLL
#ifdef GVC_EXPORTS
#define RENDER_API __declspec(dllexport)
#else
#define RENDER_API __declspec(dllimport)
#endif
#endif

#ifndef RENDER_API
#define RENDER_API /* nothing */
#endif

void arrow_flags(Agedge_t *e, uint32_t *sflag, uint32_t *eflag);
boxf arrow_bb(pointf p, pointf u, double arrowsize);
void arrow_gen(output_string *output, obj_state_t *obj, emit_state_t emit_state,
               pointf p, pointf u, double arrowsize, double penwidth,
               uint32_t flag);
RENDER_API void bezier_clip(inside_t *inside_context,
                            bool (*insidefn)(inside_t *inside_context,
                                             pointf p),
                            pointf *sp, bool left_inside);
RENDER_API Ppolyline_t *ellipticWedge(pointf ctr, double major, double minor,
                                      double angle0, double angle1);
char *getObjId(const SafeLayer *safe_layer, void *obj, agxbuf *xb);
void emit_graph(output_string *output, SafeJob *safe_job, graph_t *g,
                int *layerlist, int graph_outputorder);
void emit_label(output_string *output, SafeLayer *safe_layer, obj_state_t *obj,
                emit_state_t emit_state, textlabel_t *lp);
bool emit_once(char *message);
void emit_map_rect(obj_state_t *obj, boxf b);
RENDER_API void epsf_init(node_t *n);
RENDER_API void epsf_free(node_t *n);
void free_label(textlabel_t *);
void free_textspan(textspan_t *tl, size_t);
void *init_xdot(Agraph_t *g);
bool initMapData(obj_state_t *, char *, char *, char *, char *, char *, void *);
bool isPolygon(node_t *);
textlabel_t *make_label(void *obj, char *str, int kind, double fontsize,
                        char *fontname, char *fontcolor);
char **parse_style(char *s);
obj_state_t child_obj_state(obj_state_t *parent);
void free_child_obj(obj_state_t *child);
port resolvePort(node_t *n, node_t *other, port *oldport);
void round_corners(output_string *output, obj_state_t *obj, pointf *AF,
                   size_t sides, graphviz_polygon_style_t style, int filled);
void make_simple_label(GVC_t *gvc, textlabel_t *rv);
int stripedBox(output_string *output, obj_state_t *obj, pointf *AF,
               const char *clrs, int rotate);
RENDER_API stroke_t taper(bezier *, double (*radfunc_t)(double, double, double),
                          double initwid);
RENDER_API pointf textspan_size(GVC_t *gvc, textspan_t *span);
int wedgedEllipse(output_string *output, obj_state_t *obj, pointf *pf,
                  const char *clrs);
void init_bb(graph_t *g);

#undef RENDER_API

#ifdef __cplusplus
}
#endif
/**
 * @defgroup common_render rendering
 * @brief rendering for layout engines
 * @ingroup engines
 */
