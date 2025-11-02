/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

/* Comments on the SVG coordinate system (SN 8 Dec 2006):
   The initial <svg> element defines the SVG coordinate system so
   that the graphviz canvas (in units of points) fits the intended
   absolute size in inches.  After this, the situation should be
   that "px" = "pt" in SVG, so we can dispense with stating units.
   Also, the input units (such as fontsize) should be preserved
   without scaling in the output SVG (as long as the graph size
   was not constrained.)
 */

#include "cgraph.h"
#include "config.h"
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <macros.h>
#include <const.h>

#include <gvplugin_render.h>
#include <utils.h>
#include <gvplugin_device.h>
#include <gvcint.h>
#include <util/agxbuf.h>
#include <util/strcasecmp.h>
#include <util/unreachable.h>

#include "geomprocs.h"
#include "gvcjob.h"
#include "gvcproc.h"
#include "gvio_svg.h"

#define LOCALNAMEPREFIX '%'

/* SVG dash array */
static const char sdasharray[] = "5,2";
/* SVG dot array */
static const char sdotarray[] = "1,5";

static const char transparent[] = "transparent";
static const char none[] = "none";
static const char black[] = "black";

static void svg_bzptarray(output_string *output, pointf *A, size_t n) {
  char c;

  c = 'M'; /* first point */
  for (size_t i = 0; i < n; i++) {
    gvputc(output, c);
    gvprintdouble(output, A[i].x);
    gvputc(output, ',');
    gvprintdouble(output, -A[i].y);
    if (i == 0)
      c = 'C'; /* second point */
    else
      c = ' '; /* remaining points */
  }
}

static void svg_print_id_class(output_string *output, char *id, char *idx,
                               char *kind, void *obj) {
  char *str;

  gvputs(output, "<g id=\"");
  gvputs_xml(output, id);
  if (idx) {
    gvputc(output, '_');
    gvputs_xml(output, idx);
  }
  gvprintf(output, "\" class=\"%s", kind);
  if ((str = agget(obj, "class")) && *str) {
    gvputc(output, ' ');
    gvputs_xml(output, str);
  }
  gvputc(output, '"');
}

/* svg_print_paint assumes the caller will set the opacity if the alpha channel
 * is greater than 0 and less than 255
 */
static void svg_print_paint(output_string *output, gvcolor_t color) {
  switch (color.type) {
  case COLOR_STRING:
    if (!strcmp(color.u.string, transparent))
      gvputs(output, none);
    else
      gvputs(output, color.u.string);
    break;
  case RGBA_BYTE:
    if (color.u.rgba[3] == 0) /* transparent */
      gvputs(output, none);
    else
      gvprintf(output, "#%02x%02x%02x", color.u.rgba[0], color.u.rgba[1],
               color.u.rgba[2]);
    break;
  default:
    UNREACHABLE(); // internal error
  }
}

/* svg_print_gradient_color assumes the caller will set the opacity if the
 * alpha channel is less than 255.
 *
 * "transparent" in SVG 2 gradients is considered to be black with 0 opacity,
 * so for compatibility with SVG 1.1 output we use black when the color string
 * is transparent and assume the caller will also check and set opacity 0.
 */
static void svg_print_gradient_color(output_string *output, gvcolor_t color) {
  switch (color.type) {
  case COLOR_STRING:
    if (!strcmp(color.u.string, transparent))
      gvputs(output, black);
    else
      gvputs(output, color.u.string);
    break;
  case RGBA_BYTE:
    gvprintf(output, "#%02x%02x%02x", color.u.rgba[0], color.u.rgba[1],
             color.u.rgba[2]);
    break;
  default:
    UNREACHABLE(); // internal error
  }
}

static void svg_grstyle(output_string *output, obj_state_t *obj, int filled,
                        int gid) {
  gvputs(output, " fill=\"");
  if (filled == GRADIENT) {
    gvputs(output, "url(#");
    if (obj->id != NULL) {
      gvputs_xml(output, obj->id);
      gvputc(output, '_');
    }
    gvprintf(output, "l_%d)", gid);
  } else if (filled == RGRADIENT) {
    gvputs(output, "url(#");
    if (obj->id != NULL) {
      gvputs_xml(output, obj->id);
      gvputc(output, '_');
    }
    gvprintf(output, "r_%d)", gid);
  } else if (filled) {
    svg_print_paint(output, obj->fillcolor);
    if (obj->fillcolor.type == RGBA_BYTE && obj->fillcolor.u.rgba[3] > 0 &&
        obj->fillcolor.u.rgba[3] < 255)
      gvprintf(output, "\" fill-opacity=\"%f",
               (float)obj->fillcolor.u.rgba[3] / 255.0);
  } else {
    gvputs(output, "none");
  }
  gvputs(output, "\" stroke=\"");
  svg_print_paint(output, obj->pencolor);
  // will `gvprintdouble` output something different from `PENWIDTH_NORMAL`?
  const double GVPRINT_DOUBLE_THRESHOLD = 0.005;
  if (!(fabs(obj->penwidth - PENWIDTH_NORMAL) < GVPRINT_DOUBLE_THRESHOLD)) {
    gvputs(output, "\" stroke-width=\"");
    gvprintdouble(output, obj->penwidth);
  }
  if (obj->pen == PEN_DASHED) {
    gvprintf(output, "\" stroke-dasharray=\"%s", sdasharray);
  } else if (obj->pen == PEN_DOTTED) {
    gvprintf(output, "\" stroke-dasharray=\"%s", sdotarray);
  }
  if (obj->pencolor.type == RGBA_BYTE && obj->pencolor.u.rgba[3] > 0 &&
      obj->pencolor.u.rgba[3] < 255)
    gvprintf(output, "\" stroke-opacity=\"%f",
             (float)obj->pencolor.u.rgba[3] / 255.0);

  gvputc(output, '"');
}

void svg_comment(GVJ_t *job, char *str) {
  if (!str || !str[0])
    return;

  output_string output = job2output_string(job);
  gvputs(&output, "<!-- ");
  gvputs_xml(&output, str);
  gvputs(&output, " -->\n");
  output_string2job(job, &output);
}

void svg_begin_job(GVJ_t *job) {
  output_string output = job2output_string(job);

  char *s;
  gvputs(&output,
         "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
  if ((s = agget(job->gvc->g, "stylesheet")) && s[0]) {
    gvputs(&output, "<?xml-stylesheet href=\"");
    gvputs(&output, s);
    gvputs(&output, "\" type=\"text/css\"?>\n");
  }
  gvputs(&output, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
                  " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
  gvputs(&output, "<!-- Generated by ");
  gvputs_xml(&output, job->common->info[0]);
  gvputs(&output, " version ");
  gvputs_xml(&output, job->common->info[1]);
  gvputs(&output, " (");
  gvputs_xml(&output, job->common->info[2]);
  gvputs(&output, ")\n"
                  " -->\n");

  output_string2job(job, &output);
}

void svg_begin_graph(GVJ_t *job) {
  output_string output = job2output_string(job);

  obj_state_t *obj = job->obj;

  gvputs(&output, "<!--");
  if (agnameof(obj->u.g)[0] && agnameof(obj->u.g)[0] != LOCALNAMEPREFIX) {
    gvputs(&output, " Title: ");
    gvputs_xml(&output, agnameof(obj->u.g));
  }
  gvprintf(&output, " Pages: %d -->\n",
           job->pagesArraySize.x * job->pagesArraySize.y);

  gvprintf(&output, "<svg width=\"%dpt\" height=\"%dpt\"\n", job->width,
           job->height);
  gvprintf(&output, " viewBox=\"%d.00 %d.00 %d.00 %d.00\"",
           job->pageBoundingBox.LL.x, job->pageBoundingBox.LL.y,
           job->pageBoundingBox.UR.x, job->pageBoundingBox.UR.y);
  // https://svgwg.org/svg2-draft/struct.html#Namespace says:
  // > There's no need to have an ‘xmlns’ attribute declaring that the
  // > element is in the SVG namespace when using the HTML parser. The HTML
  // > parser will automatically create the SVG elements in the proper
  // > namespace.
  /* namespace of svg */
  gvputs(&output, " xmlns=\"http://www.w3.org/2000/svg\""
                  /* namespace of xlink */
                  " xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
  gvputs(&output, ">\n");

  output_string2job(job, &output);
}

void svg_end_graph(GVJ_t *job) {
  output_string output = job2output_string(job);
  gvputs(&output, "</svg>\n");
  output_string2job(job, &output);
}

void svg_begin_layer(GVJ_t *job, char *layername, int layerNum, int numLayers) {
  (void)layerNum;
  (void)numLayers;
  output_string output = job2output_string(job);

  obj_state_t *obj = job->obj;

  svg_print_id_class(&output, layername, NULL, "layer", obj->u.g);
  gvputs(&output, ">\n");
  output_string2job(job, &output);
}

void svg_end_layer(GVJ_t *job) {
  output_string output = job2output_string(job);
  gvputs(&output, "</g>\n");
  output_string2job(job, &output);
}

/* svg_begin_page:
 * Currently, svg output does not support pages.
 * FIX: If implemented, we must guarantee the id is unique.
 */
void svg_begin_page(GVJ_t *job) {
  output_string output = job2output_string(job);

  obj_state_t *obj = job->obj;

  /* its really just a page of the graph, but its still a graph,
   * and it is the entire graph if we're not currently paging */
  svg_print_id_class(&output, obj->id, NULL, "graph", obj->u.g);
  gvputs(&output, " transform=\"scale(");
  // cannot be gvprintdouble because 2 digits precision insufficient
  gvprintf(&output, "%g %g", job->scale.x, job->scale.y);
  gvprintf(&output, ") rotate(%d) translate(", -job->rotation);
  gvprintdouble(&output, job->translation.x);
  gvputc(&output, ' ');
  gvprintdouble(&output, -job->translation.y);
  gvputs(&output, ")\">\n");
  /* default style */
  if (agnameof(obj->u.g)[0] && agnameof(obj->u.g)[0] != LOCALNAMEPREFIX) {
    gvputs(&output, "<title>");
    gvputs_xml(&output, agnameof(obj->u.g));
    gvputs(&output, "</title>\n");
  }

  output_string2job(job, &output);
}

void svg_end_page(GVJ_t *job) {
  output_string output = job2output_string(job);
  gvputs(&output, "</g>\n");
  output_string2job(job, &output);
}

void svg_begin_cluster(GVJ_t *job) {
  output_string output = job2output_string(job);

  obj_state_t *obj = job->obj;

  svg_print_id_class(&output, obj->id, NULL, "cluster", obj->u.sg);
  gvputs(&output, ">\n"
                  "<title>");
  gvputs_xml(&output, agnameof(obj->u.g));
  gvputs(&output, "</title>\n");
  output_string2job(job, &output);
}

void svg_end_cluster(GVJ_t *job) {
  output_string output = job2output_string(job);
  gvputs(&output, "</g>\n");
  output_string2job(job, &output);
}

void svg_begin_node(GVJ_t *job) {
  output_string output = job2output_string(job);

  obj_state_t *obj = job->obj;
  char *idx;

  if (job->layerNum > 1)
    idx = job->gvc->layerIDs[job->layerNum];
  else
    idx = NULL;
  svg_print_id_class(&output, obj->id, idx, "node", obj->u.n);
  gvputs(&output, ">\n"
                  "<title>");
  gvputs_xml(&output, agnameof(obj->u.n));
  gvputs(&output, "</title>\n");
  output_string2job(job, &output);
}

void svg_end_node(GVJ_t *job) {
  output_string output = job2output_string(job);
  gvputs(&output, "</g>\n");
  output_string2job(job, &output);
}

void svg_begin_edge(GVJ_t *job) {
  output_string output = job2output_string(job);

  obj_state_t *obj = job->obj;
  char *ename;

  svg_print_id_class(&output, obj->id, NULL, "edge", obj->u.e);
  gvputs(&output, ">\n"

                  "<title>");
  ename = strdup_and_subst_obj("\\E", obj->u.e);
  gvputs_xml(&output, ename);
  free(ename);
  gvputs(&output, "</title>\n");

  output_string2job(job, &output);
}

void svg_end_edge(GVJ_t *job) {
  output_string output = job2output_string(job);
  gvputs(&output, "</g>\n");
  output_string2job(job, &output);
}

void svg_begin_anchor(GVJ_t *job, char *href, char *tooltip, char *target,
                      char *id) {
  output_string output = job2output_string(job);
  gvputs(&output, "<g");
  if (id) {
    gvputs(&output, " id=\"a_");
    gvputs_xml(&output, id);
    gvputc(&output, '"');
  }
  gvputs(&output, ">"

                  "<a");
  if (href && href[0]) {
    gvputs(&output, " xlink:href=\"");
    const xml_flags_t flags = {0};
    gvputs_xml_with_flags(&output, href, flags);
    gvputc(&output, '"');
  }
  if (tooltip && tooltip[0]) {
    gvputs(&output, " xlink:title=\"");
    const xml_flags_t flags = {.raw = 1, .dash = 1, .nbsp = 1};
    gvputs_xml_with_flags(&output, tooltip, flags);
    gvputc(&output, '"');
  }
  if (target && target[0]) {
    gvputs(&output, " target=\"");
    gvputs_xml(&output, target);
    gvputc(&output, '"');
  }
  gvputs(&output, ">\n");
  output_string2job(job, &output);
}

void svg_end_anchor(GVJ_t *job) {
  output_string output = job2output_string(job);
  gvputs(&output, "</a>\n"
                  "</g>\n");
  output_string2job(job, &output);
}

void svg_textspan(GVJ_t *job, pointf raw_p, textspan_t *span) {
  output_string output = job2output_string(job);

  if (!(span->str && span->str[0] &&
        (!job->obj /* because of xdgen non-conformity */
         || job->obj->pen != PEN_NONE))) {
    return;
  }

  pointf p;
  if (job->flags & GVRENDER_DOES_TRANSFORM)
    p = raw_p;
  else
    p = gvrender_ptf(job, raw_p);

  obj_state_t *obj = job->obj;
  PostscriptAlias *pA;
  char *family = NULL, *weight = NULL, *stretch = NULL, *style = NULL;
  unsigned int flags;

  gvputs(&output, "<text xml:space=\"preserve\"");
  switch (span->just) {
  case 'l':
    gvputs(&output, " text-anchor=\"start\"");
    break;
  case 'r':
    gvputs(&output, " text-anchor=\"end\"");
    break;
  default:
  case 'n':
    gvputs(&output, " text-anchor=\"middle\"");
    break;
  }
  p.y += span->yoffset_centerline;
  if (!obj->labeledgealigned) {
    gvputs(&output, " x=\"");
    gvprintdouble(&output, p.x);
    gvputs(&output, "\" y=\"");
    gvprintdouble(&output, -p.y);
    gvputs(&output, "\"");
  }
  pA = span->font->postscript_alias;
  if (pA) {
    switch (GD_fontnames(job->gvc->g)) {
    case PSFONTS:
      family = pA->name;
      weight = pA->weight;
      style = pA->style;
      break;
    case SVGFONTS:
      family = pA->svg_font_family;
      weight = pA->svg_font_weight;
      style = pA->svg_font_style;
      break;
    default:
    case NATIVEFONTS:
      family = pA->family;
      weight = pA->weight;
      style = pA->style;
      break;
    }
    stretch = pA->stretch;

    gvprintf(&output, " font-family=\"%s", family);
    if (pA->svg_font_family && pA->svg_font_family != family)
      gvprintf(&output, ",%s", pA->svg_font_family);
    gvputc(&output, '"');
    if (weight)
      gvprintf(&output, " font-weight=\"%s\"", weight);
    if (stretch)
      gvprintf(&output, " font-stretch=\"%s\"", stretch);
    if (style)
      gvprintf(&output, " font-style=\"%s\"", style);
  } else
    gvprintf(&output, " font-family=\"%s\"", span->font->name);
  if ((flags = span->font->flags)) {
    if ((flags & HTML_BF) && !weight)
      gvputs(&output, " font-weight=\"bold\"");
    if ((flags & HTML_IF) && !style)
      gvputs(&output, " font-style=\"italic\"");
    if (flags & (HTML_UL | HTML_S | HTML_OL)) {
      int comma = 0;
      gvputs(&output, " text-decoration=\"");
      if ((flags & HTML_UL)) {
        gvputs(&output, "underline");
        comma = 1;
      }
      if (flags & HTML_OL) {
        gvprintf(&output, "%soverline", (comma ? "," : ""));
        comma = 1;
      }
      if (flags & HTML_S)
        gvprintf(&output, "%sline-through", (comma ? "," : ""));
      gvputc(&output, '"');
    }
    if (flags & HTML_SUP)
      gvputs(&output, " baseline-shift=\"super\"");
    if (flags & HTML_SUB)
      gvputs(&output, " baseline-shift=\"sub\"");
  }

  gvprintf(&output, " font-size=\"%.2f\"", span->font->size);
  switch (obj->pencolor.type) {
  case COLOR_STRING:
    if (strcasecmp(obj->pencolor.u.string, "black"))
      gvprintf(&output, " fill=\"%s\"", obj->pencolor.u.string);
    break;
  case RGBA_BYTE:
    gvprintf(&output, " fill=\"#%02x%02x%02x\"", obj->pencolor.u.rgba[0],
             obj->pencolor.u.rgba[1], obj->pencolor.u.rgba[2]);
    if (obj->pencolor.u.rgba[3] < 255)
      gvprintf(&output, " fill-opacity=\"%f\"",
               (float)obj->pencolor.u.rgba[3] / 255.0);
    break;
  default:
    UNREACHABLE(); // internal error
  }
  gvputc(&output, '>');
  if (obj->labeledgealigned) {
    gvputs(&output, "<textPath xlink:href=\"#");
    gvputs_xml(&output, obj->id);
    gvputs(&output, "_p\" startOffset=\"50%\"><tspan x=\"0\" dy=\"");
    gvprintdouble(&output, -p.y);
    gvputs(&output, "\">");
  }
  const xml_flags_t xml_flags = {.raw = 1, .dash = 1, .nbsp = 1};
  gvputs_xml_with_flags(&output, span->str, xml_flags);
  if (obj->labeledgealigned)
    gvputs(&output, "</tspan></textPath>");
  gvputs(&output, "</text>\n");

  output_string2job(job, &output);
}

static void svg_print_stop(output_string *output, double offset,
                           gvcolor_t color) {
  if (fabs(offset - 0.0) < 0.0005)
    gvputs(output, "<stop offset=\"0\" style=\"stop-color:");
  else if (fabs(offset - 1.0) < 0.0005)
    gvputs(output, "<stop offset=\"1\" style=\"stop-color:");
  else
    gvprintf(output, "<stop offset=\"%.03f\" style=\"stop-color:", offset);
  svg_print_gradient_color(output, color);
  gvputs(output, ";stop-opacity:");
  if (color.type == RGBA_BYTE && color.u.rgba[3] < 255)
    gvprintf(output, "%f", (float)color.u.rgba[3] / 255.0);
  else if (color.type == COLOR_STRING && !strcmp(color.u.string, transparent))
    gvputs(output, "0");
  else
    gvputs(output, "1.");
  gvputs(output, ";\"/>\n");
}

/* svg_gradstyle
 * Outputs the SVG statements that define the gradient pattern
 */
static int svg_gradstyle(output_string *output, obj_state_t *obj, pointf *A,
                         size_t n) {
  pointf G[2];
  static int gradId;
  int id = gradId++;

  double angle = obj->gradient_angle * M_PI / 180; // angle of gradient line
  G[0].x = G[0].y = G[1].x = G[1].y = 0.;
  get_gradient_points(A, G, n, angle, 0); // get points on gradient line

  gvputs(output, "<defs>\n<linearGradient id=\"");
  if (obj->id != NULL) {
    gvputs_xml(output, obj->id);
    gvputc(output, '_');
  }
  gvprintf(output, "l_%d\" gradientUnits=\"userSpaceOnUse\" ", id);
  gvputs(output, "x1=\"");
  gvprintdouble(output, G[0].x);
  gvputs(output, "\" y1=\"");
  gvprintdouble(output, G[0].y);
  gvputs(output, "\" x2=\"");
  gvprintdouble(output, G[1].x);
  gvputs(output, "\" y2=\"");
  gvprintdouble(output, G[1].y);
  gvputs(output, "\" >\n");

  svg_print_stop(output,
                 obj->gradient_frac > 0 ? obj->gradient_frac - 0.001 : 0.0,
                 obj->fillcolor);
  svg_print_stop(output, obj->gradient_frac > 0 ? obj->gradient_frac : 1.0,
                 obj->stopcolor);

  gvputs(output, "</linearGradient>\n</defs>\n");
  return id;
}

/* svg_rgradstyle
 * Outputs the SVG statements that define the radial gradient pattern
 */
static int svg_rgradstyle(output_string *output, obj_state_t *obj) {
  double ifx, ify;
  static int rgradId;
  int id = rgradId++;

  if (obj->gradient_angle == 0) {
    ifx = ify = 50;
  } else {
    double angle = obj->gradient_angle * M_PI / 180; // angle of gradient line
    ifx = round(50 * (1 + cos(angle)));
    ify = round(50 * (1 - sin(angle)));
  }
  gvputs(output, "<defs>\n<radialGradient id=\"");
  if (obj->id != NULL) {
    gvputs_xml(output, obj->id);
    gvputc(output, '_');
  }
  gvprintf(output,
           "r_%d\" cx=\"50%%\" cy=\"50%%\" r=\"75%%\" "
           "fx=\"%.0f%%\" fy=\"%.0f%%\">\n",
           id, ifx, ify);

  svg_print_stop(output, 0.0, obj->fillcolor);
  svg_print_stop(output, 1.0, obj->stopcolor);

  gvputs(output, "</radialGradient>\n</defs>\n");
  return id;
}

void svg_ellipse(GVJ_t *job, pointf *pf, int filled) {
  output_string output = job2output_string(job);
  obj_state_t *obj = job->obj;

  if (obj->pen == PEN_NONE) {
    return;
  }

  pointf A[] = {
      mid_pointf(pf[0], pf[1]), // center
      pf[1]                     // corner
  };

  if (!(job->flags & GVRENDER_DOES_TRANSFORM))
    gvrender_ptf_A(job, A, A, 2);

  int gid = 0;

  /* A[] contains 2 points: the center and corner. */
  if (filled == GRADIENT) {
    gid = svg_gradstyle(&output, obj, A, 2);
  } else if (filled == RGRADIENT) {
    gid = svg_rgradstyle(&output, obj);
  }
  gvputs(&output, "<ellipse");
  svg_grstyle(&output, obj, filled, gid);
  gvputs(&output, " cx=\"");
  gvprintdouble(&output, A[0].x);
  gvputs(&output, "\" cy=\"");
  gvprintdouble(&output, -A[0].y);
  gvputs(&output, "\" rx=\"");
  gvprintdouble(&output, A[1].x - A[0].x);
  gvputs(&output, "\" ry=\"");
  gvprintdouble(&output, A[1].y - A[0].y);
  gvputs(&output, "\"/>\n");

  output_string2job(job, &output);
}

static void svg_bezier_impl(output_string *output, obj_state_t *obj, pointf *A,
                            size_t n, int filled) {
  int gid = 0;

  if (filled == GRADIENT) {
    gid = svg_gradstyle(output, obj, A, n);
  } else if (filled == RGRADIENT) {
    gid = svg_rgradstyle(output, obj);
  }
  gvputs(output, "<path");
  if (obj->labeledgealigned) {
    gvputs(output, " id=\"");
    gvputs_xml(output, obj->id);
    gvputs(output, "_p\" ");
  }
  svg_grstyle(output, obj, filled, gid);
  gvputs(output, " d=\"");
  svg_bzptarray(output, A, n);
  gvputs(output, "\"/>\n");
}

void svg_bezier(GVJ_t *job, pointf *af, size_t n, int filled) {
  output_string output = job2output_string(job);
  obj_state_t *obj = job->obj;

  if (job->obj->pen != PEN_NONE) {
    if (job->flags & GVRENDER_DOES_TRANSFORM)
      svg_bezier_impl(&output, obj, af, n, filled);
    else {
      pointf *AF = gv_calloc(n, sizeof(pointf));
      gvrender_ptf_A(job, af, AF, n);
      svg_bezier_impl(&output, obj, AF, n, filled);
      free(AF);
    }
  }
  output_string2job(job, &output);
}

static void svg_polygon_impl(output_string *output, obj_state_t *obj, pointf *A,
                             size_t n, int filled) {
  int gid = 0;
  if (filled == GRADIENT) {
    gid = svg_gradstyle(output, obj, A, n);
  } else if (filled == RGRADIENT) {
    gid = svg_rgradstyle(output, obj);
  }
  gvputs(output, "<polygon");
  svg_grstyle(output, obj, filled, gid);
  gvputs(output, " points=\"");
  for (size_t i = 0; i < n; i++) {
    gvprintdouble(output, A[i].x);
    gvputc(output, ',');
    gvprintdouble(output, -A[i].y);
    gvputc(output, ' ');
  }
  /* repeat the first point because Adobe SVG is broken */
  gvprintdouble(output, A[0].x);
  gvputc(output, ',');
  gvprintdouble(output, -A[0].y);
  gvputs(output, "\"/>\n");
}

void svg_polygon(GVJ_t *job, pointf *af, size_t n, int filled) {
  output_string output = job2output_string(job);
  obj_state_t *obj = job->obj;

  int noPoly = 0;
  gvcolor_t save_pencolor;

  if (obj->pen != PEN_NONE) {
    if (filled & NO_POLY) {
      noPoly = 1;
      filled &= ~NO_POLY;
      save_pencolor = obj->pencolor;
      obj->pencolor = obj->fillcolor;
    }
    if (job->flags & GVRENDER_DOES_TRANSFORM)
      svg_polygon_impl(&output, obj, af, n, filled);
    else {
      pointf *AF = gv_calloc(n, sizeof(pointf));
      gvrender_ptf_A(job, af, AF, n);
      svg_polygon_impl(&output, obj, AF, n, filled);
      free(AF);
    }
    if (noPoly)
      obj->pencolor = save_pencolor;
  }

  output_string2job(job, &output);
}

void svg_box(GVJ_t *job, boxf B, int filled) {
  pointf A[4];

  A[0] = B.LL;
  A[2] = B.UR;
  A[1].x = A[0].x;
  A[1].y = A[2].y;
  A[3].x = A[2].x;
  A[3].y = A[0].y;

  svg_polygon(job, A, 4, filled);
}

static void svg_polyline_impl(output_string *output, obj_state_t *obj,
                              pointf *A, size_t n) {
  gvputs(output, "<polyline");
  svg_grstyle(output, obj, 0, 0);
  gvputs(output, " points=\"");
  for (size_t i = 0; i < n; i++) {
    gvprintdouble(output, A[i].x);
    gvputc(output, ',');
    gvprintdouble(output, -A[i].y);
    if (i + 1 != n) {
      gvputc(output, ' ');
    }
  }
  gvputs(output, "\"/>\n");
}

void svg_polyline(GVJ_t *job, pointf *af, size_t n) {
  output_string output = job2output_string(job);
  obj_state_t *obj = job->obj;

  if (obj->pen != PEN_NONE) {
    if (job->flags & GVRENDER_DOES_TRANSFORM)
      svg_polyline_impl(&output, obj, af, n);
    else {
      pointf *AF = gv_calloc(n, sizeof(pointf));
      gvrender_ptf_A(job, af, AF, n);
      svg_polyline_impl(&output, obj, AF, n);
      free(AF);
    }
  }

  output_string2job(job, &output);
}

/* color names from http://www.w3.org/TR/SVG/types.html */
/* NB.  List must be LANG_C sorted */
char *svg_knowncolors[] = {"aliceblue",
                           "antiquewhite",
                           "aqua",
                           "aquamarine",
                           "azure",
                           "beige",
                           "bisque",
                           "black",
                           "blanchedalmond",
                           "blue",
                           "blueviolet",
                           "brown",
                           "burlywood",
                           "cadetblue",
                           "chartreuse",
                           "chocolate",
                           "coral",
                           "cornflowerblue",
                           "cornsilk",
                           "crimson",
                           "cyan",
                           "darkblue",
                           "darkcyan",
                           "darkgoldenrod",
                           "darkgray",
                           "darkgreen",
                           "darkgrey",
                           "darkkhaki",
                           "darkmagenta",
                           "darkolivegreen",
                           "darkorange",
                           "darkorchid",
                           "darkred",
                           "darksalmon",
                           "darkseagreen",
                           "darkslateblue",
                           "darkslategray",
                           "darkslategrey",
                           "darkturquoise",
                           "darkviolet",
                           "deeppink",
                           "deepskyblue",
                           "dimgray",
                           "dimgrey",
                           "dodgerblue",
                           "firebrick",
                           "floralwhite",
                           "forestgreen",
                           "fuchsia",
                           "gainsboro",
                           "ghostwhite",
                           "gold",
                           "goldenrod",
                           "gray",
                           "green",
                           "greenyellow",
                           "grey",
                           "honeydew",
                           "hotpink",
                           "indianred",
                           "indigo",
                           "ivory",
                           "khaki",
                           "lavender",
                           "lavenderblush",
                           "lawngreen",
                           "lemonchiffon",
                           "lightblue",
                           "lightcoral",
                           "lightcyan",
                           "lightgoldenrodyellow",
                           "lightgray",
                           "lightgreen",
                           "lightgrey",
                           "lightpink",
                           "lightsalmon",
                           "lightseagreen",
                           "lightskyblue",
                           "lightslategray",
                           "lightslategrey",
                           "lightsteelblue",
                           "lightyellow",
                           "lime",
                           "limegreen",
                           "linen",
                           "magenta",
                           "maroon",
                           "mediumaquamarine",
                           "mediumblue",
                           "mediumorchid",
                           "mediumpurple",
                           "mediumseagreen",
                           "mediumslateblue",
                           "mediumspringgreen",
                           "mediumturquoise",
                           "mediumvioletred",
                           "midnightblue",
                           "mintcream",
                           "mistyrose",
                           "moccasin",
                           "navajowhite",
                           "navy",
                           "oldlace",
                           "olive",
                           "olivedrab",
                           "orange",
                           "orangered",
                           "orchid",
                           "palegoldenrod",
                           "palegreen",
                           "paleturquoise",
                           "palevioletred",
                           "papayawhip",
                           "peachpuff",
                           "peru",
                           "pink",
                           "plum",
                           "powderblue",
                           "purple",
                           "red",
                           "rosybrown",
                           "royalblue",
                           "saddlebrown",
                           "salmon",
                           "sandybrown",
                           "seagreen",
                           "seashell",
                           "sienna",
                           "silver",
                           "skyblue",
                           "slateblue",
                           "slategray",
                           "slategrey",
                           "snow",
                           "springgreen",
                           "steelblue",
                           "tan",
                           "teal",
                           "thistle",
                           "tomato",
                           "transparent",
                           "turquoise",
                           "violet",
                           "wheat",
                           "white",
                           "whitesmoke",
                           "yellow",
                           "yellowgreen"};

gvrender_engine_t svg_engine = {
    svg_begin_job,
    0, /* svg_end_job */
    svg_begin_graph,
    svg_end_graph,
    svg_begin_layer,
    svg_end_layer,
    svg_begin_page,
    svg_end_page,
    svg_begin_cluster,
    svg_end_cluster,
    0, /* svg_begin_nodes */
    0, /* svg_end_nodes */
    0, /* svg_begin_edges */
    0, /* svg_end_edges */
    svg_begin_node,
    svg_end_node,
    svg_begin_edge,
    svg_end_edge,
    svg_begin_anchor,
    svg_end_anchor,
    0, /* svg_begin_anchor */
    0, /* svg_end_anchor */
    svg_textspan,
    0, /* svg_resolve_color */
    svg_ellipse,
    svg_polygon,
    svg_bezier,
    svg_polyline,
    svg_comment,
    0, /* svg_library_shape */
};

gvrender_features_t my_render_features_svg = {
    GVRENDER_Y_GOES_DOWN | GVRENDER_DOES_TRANSFORM | GVRENDER_DOES_LABELS |
        GVRENDER_DOES_MAPS | GVRENDER_DOES_TARGETS |
        GVRENDER_DOES_TOOLTIPS,               /* flags */
    4.,                                       /* default pad - graph units */
    svg_knowncolors,                          /* knowncolors */
    sizeof(svg_knowncolors) / sizeof(char *), /* sizeof knowncolors */
    RGBA_BYTE,                                /* color_type */
};

gvdevice_features_t my_device_features_svg = {
    GVDEVICE_DOES_TRUECOLOR | GVDEVICE_DOES_LAYERS, /* flags */
    {0., 0.},   /* default margin - points */
    {0., 0.},   /* default page width, height - points */
    {72., 72.}, /* default dpi */
};

gvplugin_installed_t svg_device_installed = {0, "svg:svg", 1, NULL,
                                             &my_device_features_svg};
gvplugin_available_t svg_device_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &svg_device_installed,
    .typestr = "svg:svg",
};

gvplugin_installed_t svg_render_installed = {0, "svg", 1, &svg_engine,
                                             &my_render_features_svg};
gvplugin_available_t svg_render_available = {
    .next = NULL,
    .package = NULL,
    .quality = 1,
    .typeptr = &svg_render_installed,
    .typestr = "svg",
};
