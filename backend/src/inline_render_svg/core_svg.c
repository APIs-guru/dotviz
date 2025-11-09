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
#include "colorprocs.h"
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
#include "gvio_svg.h"
#include "../output_string.h"
#include "render_svg.h"
#include "safe_job.h"
#include "streq.h"
#include "types.h"
#include "core_svg.h"

static imagescale_t get_imagescale(char *s) {
  if (*s == '\0')
    return IMAGESCALE_FALSE;
  if (!strcasecmp(s, "width"))
    return IMAGESCALE_WIDTH;
  if (!strcasecmp(s, "height"))
    return IMAGESCALE_HEIGHT;
  if (!strcasecmp(s, "both"))
    return IMAGESCALE_BOTH;
  if (mapbool(s))
    return IMAGESCALE_TRUE;
  return IMAGESCALE_FALSE;
}

static imagepos_t get_imagepos(char *s) {
  if (*s == '\0')
    return IMAGEPOS_MIDDLE_CENTER;
  if (!strcasecmp(s, "tl"))
    return IMAGEPOS_TOP_LEFT;
  if (!strcasecmp(s, "tc"))
    return IMAGEPOS_TOP_CENTER;
  if (!strcasecmp(s, "tr"))
    return IMAGEPOS_TOP_RIGHT;
  if (!strcasecmp(s, "ml"))
    return IMAGEPOS_MIDDLE_LEFT;
  if (!strcasecmp(s, "mc"))
    return IMAGEPOS_MIDDLE_CENTER;
  if (!strcasecmp(s, "mr"))
    return IMAGEPOS_MIDDLE_RIGHT;
  if (!strcasecmp(s, "bl"))
    return IMAGEPOS_BOTTOM_LEFT;
  if (!strcasecmp(s, "bc"))
    return IMAGEPOS_BOTTOM_CENTER;
  if (!strcasecmp(s, "br"))
    return IMAGEPOS_BOTTOM_RIGHT;
  return IMAGEPOS_MIDDLE_CENTER;
}

static void core_loadimage_svg(output_string *output, int rotation_deg,
                               const char *name, boxf b) {
  double width = (b.UR.x - b.LL.x);
  double height = (b.UR.y - b.LL.y);
  double originx = (b.UR.x + b.LL.x - width) / 2;
  double originy = (b.UR.y + b.LL.y + height) / 2;

  out_puts(output, "<image xlink:href=\"");
  out_puts(output, name);
  if (rotation_deg != 0) {

    // FIXME - this is messed up >>>
    gvprintf(output,
             "\" width=\"%gpx\" height=\"%gpx\" preserveAspectRatio=\"xMidYMid "
             "meet\" x=\"%g\" y=\"%g\"",
             height, width, originx, -originy);
    gvprintf(output, " transform=\"rotate(%d %g %g)\"", rotation_deg, originx,
             -originy);
    // <<<
  } else {
    gvprintf(output,
             "\" width=\"%gpx\" height=\"%gpx\" preserveAspectRatio=\"xMinYMin "
             "meet\" x=\"%g\" y=\"%g\"",
             width, height, originx, -originy);
  }
  out_puts(output, "/>\n");
}

extern point get_dimensions_by_name(const char *name, pointf dpi);
/* gvrender_usershape:
 * Scale image to fill polygon bounding box accordingus to "imagescale",
 * positioned at "imagepos"
 */
void svg_usershape(output_string *output, int rotation_deg, pointf dpi,
                   char *name, pointf *a, size_t n, char *imagescale,
                   char *imagepos) {
  assert(name);
  assert(name[0]);

  point isz = get_dimensions_by_name(name, dpi);

  if ((isz.x <= 0) && (isz.y <= 0))
    return;

  /* compute bb of polygon */
  boxf b; /* target box */
  b.LL = b.UR = a[0];
  for (size_t i = 1; i < n; i++) {
    expandbp(&b, a[i]);
  }

  double pw = b.UR.x - b.LL.x;
  double ph = b.UR.y - b.LL.y;
  double ih = (double)isz.y;
  double iw = (double)isz.x;

  /* scale factors */
  double scalex = pw / iw;
  double scaley = ph / ih;

  switch (get_imagescale(imagescale)) {
  case IMAGESCALE_TRUE:
    /* keep aspect ratio fixed by just using the smaller scale */
    if (scalex < scaley) {
      iw *= scalex;
      ih *= scalex;
    } else {
      iw *= scaley;
      ih *= scaley;
    }
    break;
  case IMAGESCALE_WIDTH:
    iw *= scalex;
    break;
  case IMAGESCALE_HEIGHT:
    ih *= scaley;
    break;
  case IMAGESCALE_BOTH:
    iw *= scalex;
    ih *= scaley;
    break;
  case IMAGESCALE_FALSE:
  default:
    break;
  }

  /* if image is smaller in any dimension, apply the specified positioning */
  imagepos_t position = get_imagepos(imagepos);
  if (iw < pw) {
    switch (position) {
    case IMAGEPOS_TOP_LEFT:
    case IMAGEPOS_MIDDLE_LEFT:
    case IMAGEPOS_BOTTOM_LEFT:
      b.UR.x = b.LL.x + iw;
      break;
    case IMAGEPOS_TOP_RIGHT:
    case IMAGEPOS_MIDDLE_RIGHT:
    case IMAGEPOS_BOTTOM_RIGHT:
      b.LL.x += (pw - iw);
      b.UR.x = b.LL.x + iw;
      break;
    default:
      b.LL.x += (pw - iw) / 2.0;
      b.UR.x -= (pw - iw) / 2.0;
      break;
    }
  }
  if (ih < ph) {
    switch (position) {
    case IMAGEPOS_TOP_LEFT:
    case IMAGEPOS_TOP_CENTER:
    case IMAGEPOS_TOP_RIGHT:
      b.LL.y = b.UR.y - ih;
      break;
    case IMAGEPOS_BOTTOM_LEFT:
    case IMAGEPOS_BOTTOM_CENTER:
    case IMAGEPOS_BOTTOM_RIGHT:
      b.LL.y += ih;
      b.UR.y = b.LL.y - ih;
      break;
    default:
      b.LL.y += (ph - ih) / 2.0;
      b.UR.y -= (ph - ih) / 2.0;
      break;
    }
  }

  if (b.LL.x > b.UR.x) {
    double d = b.LL.x;
    b.LL.x = b.UR.x;
    b.UR.x = d;
  }
  if (b.LL.y > b.UR.y) {
    double d = b.LL.y;
    b.LL.y = b.UR.y;
    b.UR.y = d;
  }
  core_loadimage_svg(output, rotation_deg, name, b);
}

void jobsvg_usershape(GVJ_t *job, char *name, pointf *a, size_t n,
                      char *imagescale, char *imagepos) {
  output_string output = job2output_string(job);
  svg_usershape(&output, job->rotation, job->dpi, name, a, n, imagescale,
                imagepos);
  output_string2job(job, &output);
}

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
    out_putc(output, c);
    gvprintdouble(output, A[i].x);
    out_putc(output, ',');
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

  out_puts(output, "<g id=\"");
  gvputs_xml(output, id);
  if (idx) {
    out_putc(output, '_');
    gvputs_xml(output, idx);
  }
  gvprintf(output, "\" class=\"%s", kind);
  if ((str = agget(obj, "class")) && *str) {
    out_putc(output, ' ');
    gvputs_xml(output, str);
  }
  out_putc(output, '"');
}

/* svg_print_paint assumes the caller will set the opacity if the alpha channel
 * is greater than 0 and less than 255
 */
static void svg_print_paint(output_string *output, gvcolor_t color) {
  switch (color.type) {
  case COLOR_STRING:
    if (!strcmp(color.u.string, transparent))
      out_puts(output, none);
    else
      out_puts(output, color.u.string);
    break;
  case RGBA_BYTE:
    if (color.u.rgba[3] == 0) /* transparent */
      out_puts(output, none);
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
      out_puts(output, black);
    else
      out_puts(output, color.u.string);
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
  out_puts(output, " fill=\"");
  if (filled == GRADIENT) {
    out_puts(output, "url(#");
    if (obj->id != NULL) {
      gvputs_xml(output, obj->id);
      out_putc(output, '_');
    }
    gvprintf(output, "l_%d)", gid);
  } else if (filled == RGRADIENT) {
    out_puts(output, "url(#");
    if (obj->id != NULL) {
      gvputs_xml(output, obj->id);
      out_putc(output, '_');
    }
    gvprintf(output, "r_%d)", gid);
  } else if (filled) {
    svg_print_paint(output, obj->fillcolor);
    if (obj->fillcolor.type == RGBA_BYTE && obj->fillcolor.u.rgba[3] > 0 &&
        obj->fillcolor.u.rgba[3] < 255)
      gvprintf(output, "\" fill-opacity=\"%f",
               (float)obj->fillcolor.u.rgba[3] / 255.0);
  } else {
    out_puts(output, "none");
  }
  out_puts(output, "\" stroke=\"");
  svg_print_paint(output, obj->pencolor);
  // will `gvprintdouble` output something different from `PENWIDTH_NORMAL`?
  const double GVPRINT_DOUBLE_THRESHOLD = 0.005;
  if (!(fabs(obj->penwidth - PENWIDTH_NORMAL) < GVPRINT_DOUBLE_THRESHOLD)) {
    out_puts(output, "\" stroke-width=\"");
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

  out_putc(output, '"');
}

void svg_comment(output_string *output, char *str) {
  if (!str || !str[0])
    return;

  out_puts(output, "<!-- ");
  gvputs_xml(output, str);
  out_puts(output, " -->\n");
}

void jobsvg_comment(GVJ_t *job, char *str) {
  output_string output = job2output_string(job);
  svg_comment(&output, str);
  output_string2job(job, &output);
}

void svg_begin_job(output_string *output, SafeJob *safe_job) {
  char *s;
  out_puts(output,
           "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
  if ((s = agget((graph_t *)safe_job->graph, "stylesheet")) && s[0]) {
    out_puts(output, "<?xml-stylesheet href=\"");
    out_puts(output, s);
    out_puts(output, "\" type=\"text/css\"?>\n");
  }
  out_puts(output, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
                   " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
}

void jobsvg_begin_job(GVJ_t *job) {
  output_string output = job2output_string(job);
  SafeJob safe_job = to_safe_job(job);
  svg_begin_job(&output, &safe_job);
  output_string2job(job, &output);
}

void svg_begin_graph(output_string *output, SafeJob *safe_job,
                     obj_state_t *obj) {

  out_puts(output, "<!--");
  if (agnameof(obj->u.g)[0] && agnameof(obj->u.g)[0] != LOCALNAMEPREFIX) {
    out_puts(output, " Title: ");
    gvputs_xml(output, agnameof(obj->u.g));
  }
  gvprintf(output, " Pages: %d -->\n",
           safe_job->pagesArraySize.x * safe_job->pagesArraySize.y);

  gvprintf(output, "<svg width=\"%dpt\" height=\"%dpt\"\n", safe_job->width,
           safe_job->height);
  gvprintf(output, " viewBox=\"%d.00 %d.00 %d.00 %d.00\"",
           safe_job->pageBoundingBox.LL.x, safe_job->pageBoundingBox.LL.y,
           safe_job->pageBoundingBox.UR.x, safe_job->pageBoundingBox.UR.y);
  // https://svgwg.org/svg2-draft/struct.html#Namespace says:
  // > There's no need to have an ‘xmlns’ attribute declaring that the
  // > element is in the SVG namespace when using the HTML parser. The HTML
  // > parser will automatically create the SVG elements in the proper
  // > namespace.
  /* namespace of svg */
  out_puts(output, " xmlns=\"http://www.w3.org/2000/svg\""
                   /* namespace of xlink */
                   " xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
  out_puts(output, ">\n");
}

void jobsvg_begin_graph(GVJ_t *job) {
  output_string output = job2output_string(job);
  SafeJob safe_job = to_safe_job(job);
  svg_begin_graph(&output, &safe_job, job->obj);
  output_string2job(job, &output);
}

void svg_end_graph(output_string *output) { out_puts(output, "</svg>\n"); }

void jobsvg_end_graph(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_end_graph(&output);
  output_string2job(job, &output);
}

void svg_begin_layer(output_string *output, obj_state_t *obj, char *layername) {
  svg_print_id_class(output, layername, NULL, "layer", obj->u.g);
  out_puts(output, ">\n");
}

void jobsvg_begin_layer(GVJ_t *job, char *layername) {
  output_string output = job2output_string(job);
  svg_begin_layer(&output, job->obj, layername);
  output_string2job(job, &output);
}

void svg_end_layer(output_string *output) { out_puts(output, "</g>\n"); }

void jobsvg_end_layer(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_end_layer(&output);
  output_string2job(job, &output);
}

/* svg_begin_page:
 * Currently, svg output does not support pages.
 * FIX: If implemented, we must guarantee the id is unique.
 */
void svg_begin_page(output_string *output, SafeJob *safe_job,
                    obj_state_t *obj) {
  /* its really just a page of the graph, but its still a graph,
   * and it is the entire graph if we're not currently paging */
  svg_print_id_class(output, obj->id, NULL, "graph", obj->u.g);
  out_puts(output, " transform=\"scale(");
  // cannot be gvprintdouble because 2 digits precision insufficient
  gvprintf(output, "%g %g", safe_job->scale.x, safe_job->scale.y);
  gvprintf(output, ") rotate(%d) translate(", -safe_job->rotation);
  gvprintdouble(output, safe_job->translation.x);
  out_putc(output, ' ');
  gvprintdouble(output, -safe_job->translation.y);
  out_puts(output, ")\">\n");
  /* default style */
  if (agnameof(obj->u.g)[0] && agnameof(obj->u.g)[0] != LOCALNAMEPREFIX) {
    out_puts(output, "<title>");
    gvputs_xml(output, agnameof(obj->u.g));
    out_puts(output, "</title>\n");
  }
}

void jobsvg_begin_page(GVJ_t *job) {
  output_string output = job2output_string(job);
  SafeJob safe_job = to_safe_job(job);
  svg_begin_page(&output, &safe_job, job->obj);
  output_string2job(job, &output);
}

void svg_end_page(output_string *output) { out_puts(output, "</g>\n"); }

void jobsvg_end_page(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_end_page(&output);
  output_string2job(job, &output);
}

void svg_begin_cluster(output_string *output, obj_state_t *obj) {
  svg_print_id_class(output, obj->id, NULL, "cluster", obj->u.sg);
  out_puts(output, ">\n"
                   "<title>");
  gvputs_xml(output, agnameof(obj->u.g));
  out_puts(output, "</title>\n");
}

void jobsvg_begin_cluster(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_begin_cluster(&output, job->obj);
  output_string2job(job, &output);
}

void svg_end_cluster(output_string *output) { out_puts(output, "</g>\n"); }

void jobsvg_end_cluster(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_end_cluster(&output);
  output_string2job(job, &output);
}

void svg_begin_node(output_string *output, SafeJob *safe_job,
                    obj_state_t *obj) {
  char *idx;

  if (safe_job->layerNum > 1)
    idx = safe_job->layerIDs[safe_job->layerNum];
  else
    idx = NULL;
  svg_print_id_class(output, obj->id, idx, "node", obj->u.n);
  out_puts(output, ">\n"
                   "<title>");
  gvputs_xml(output, agnameof(obj->u.n));
  out_puts(output, "</title>\n");
}

void jobsvg_begin_node(GVJ_t *job) {
  output_string output = job2output_string(job);
  SafeJob safe_job = to_safe_job(job);
  svg_begin_node(&output, &safe_job, job->obj);
  output_string2job(job, &output);
}

void svg_end_node(output_string *output) { out_puts(output, "</g>\n"); }

void jobsvg_end_node(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_end_node(&output);
  output_string2job(job, &output);
}

void svg_begin_edge(output_string *output, obj_state_t *obj) {
  char *ename;

  svg_print_id_class(output, obj->id, NULL, "edge", obj->u.e);
  out_puts(output, ">\n"

                   "<title>");
  ename = strdup_and_subst_obj("\\E", obj->u.e);
  gvputs_xml(output, ename);
  free(ename);
  out_puts(output, "</title>\n");
}

void jobsvg_begin_edge(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_begin_edge(&output, job->obj);
  output_string2job(job, &output);
}

void svg_end_edge(output_string *output) { out_puts(output, "</g>\n"); }

void jobsvg_end_edge(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_end_edge(&output);
  output_string2job(job, &output);
}

void svg_begin_anchor(output_string *output, char *href, char *tooltip,
                      char *target, char *id) {
  out_puts(output, "<g");
  if (id) {
    out_puts(output, " id=\"a_");
    gvputs_xml(output, id);
    out_putc(output, '"');
  }
  out_puts(output, ">"

                   "<a");
  if (href && href[0]) {
    out_puts(output, " xlink:href=\"");
    const xml_flags_t flags = {0};
    gvputs_xml_with_flags(output, href, flags);
    out_putc(output, '"');
  }
  if (tooltip && tooltip[0]) {
    out_puts(output, " xlink:title=\"");
    const xml_flags_t flags = {.raw = 1, .dash = 1, .nbsp = 1};
    gvputs_xml_with_flags(output, tooltip, flags);
    out_putc(output, '"');
  }
  if (target && target[0]) {
    out_puts(output, " target=\"");
    gvputs_xml(output, target);
    out_putc(output, '"');
  }
  out_puts(output, ">\n");
}

void jobsvg_begin_anchor(GVJ_t *job, char *href, char *tooltip, char *target,
                         char *id) {
  output_string output = job2output_string(job);
  svg_begin_anchor(&output, href, tooltip, target, id);
  output_string2job(job, &output);
}

void svg_end_anchor(output_string *output) {
  out_puts(output, "</a>\n"
                   "</g>\n");
}

void jobsvg_end_anchor(GVJ_t *job) {
  output_string output = job2output_string(job);
  svg_end_anchor(&output);
  output_string2job(job, &output);
}

// GD_fontnames(job->gvc->g)
void svg_textspan(output_string *output, fontname_kind fontnames,
                  obj_state_t *obj, pointf p, textspan_t *span) {
  if (!(span->str && span->str[0] &&
        (!obj /* because of xdgen non-conformity */
         || obj->pen != PEN_NONE))) {
    return;
  }
  PostscriptAlias *pA;
  char *family = NULL, *weight = NULL, *stretch = NULL, *style = NULL;
  unsigned int flags;

  out_puts(output, "<text xml:space=\"preserve\"");
  switch (span->just) {
  case 'l':
    out_puts(output, " text-anchor=\"start\"");
    break;
  case 'r':
    out_puts(output, " text-anchor=\"end\"");
    break;
  default:
  case 'n':
    out_puts(output, " text-anchor=\"middle\"");
    break;
  }
  p.y += span->yoffset_centerline;
  if (!obj->labeledgealigned) {
    out_puts(output, " x=\"");
    gvprintdouble(output, p.x);
    out_puts(output, "\" y=\"");
    gvprintdouble(output, -p.y);
    out_puts(output, "\"");
  }
  pA = span->font->postscript_alias;
  if (pA) {
    switch (fontnames) {
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

    gvprintf(output, " font-family=\"%s", family);
    if (pA->svg_font_family && pA->svg_font_family != family)
      gvprintf(output, ",%s", pA->svg_font_family);
    out_putc(output, '"');
    if (weight)
      gvprintf(output, " font-weight=\"%s\"", weight);
    if (stretch)
      gvprintf(output, " font-stretch=\"%s\"", stretch);
    if (style)
      gvprintf(output, " font-style=\"%s\"", style);
  } else
    gvprintf(output, " font-family=\"%s\"", span->font->name);
  if ((flags = span->font->flags)) {
    if ((flags & HTML_BF) && !weight)
      out_puts(output, " font-weight=\"bold\"");
    if ((flags & HTML_IF) && !style)
      out_puts(output, " font-style=\"italic\"");
    if (flags & (HTML_UL | HTML_S | HTML_OL)) {
      int comma = 0;
      out_puts(output, " text-decoration=\"");
      if ((flags & HTML_UL)) {
        out_puts(output, "underline");
        comma = 1;
      }
      if (flags & HTML_OL) {
        gvprintf(output, "%soverline", (comma ? "," : ""));
        comma = 1;
      }
      if (flags & HTML_S)
        gvprintf(output, "%sline-through", (comma ? "," : ""));
      out_putc(output, '"');
    }
    if (flags & HTML_SUP)
      out_puts(output, " baseline-shift=\"super\"");
    if (flags & HTML_SUB)
      out_puts(output, " baseline-shift=\"sub\"");
  }

  gvprintf(output, " font-size=\"%.2f\"", span->font->size);
  switch (obj->pencolor.type) {
  case COLOR_STRING:
    if (strcasecmp(obj->pencolor.u.string, "black"))
      gvprintf(output, " fill=\"%s\"", obj->pencolor.u.string);
    break;
  case RGBA_BYTE:
    gvprintf(output, " fill=\"#%02x%02x%02x\"", obj->pencolor.u.rgba[0],
             obj->pencolor.u.rgba[1], obj->pencolor.u.rgba[2]);
    if (obj->pencolor.u.rgba[3] < 255)
      gvprintf(output, " fill-opacity=\"%f\"",
               (float)obj->pencolor.u.rgba[3] / 255.0);
    break;
  default:
    UNREACHABLE(); // internal error
  }
  out_putc(output, '>');
  if (obj->labeledgealigned) {
    out_puts(output, "<textPath xlink:href=\"#");
    gvputs_xml(output, obj->id);
    out_puts(output, "_p\" startOffset=\"50%\"><tspan x=\"0\" dy=\"");
    gvprintdouble(output, -p.y);
    out_puts(output, "\">");
  }
  const xml_flags_t xml_flags = {.raw = 1, .dash = 1, .nbsp = 1};
  gvputs_xml_with_flags(output, span->str, xml_flags);
  if (obj->labeledgealigned)
    out_puts(output, "</tspan></textPath>");
  out_puts(output, "</text>\n");
}

void jobsvg_textspan(GVJ_t *job, pointf p, textspan_t *span) {
  output_string output = job2output_string(job);
  svg_textspan(&output, GD_fontnames(job->gvc->g), job->obj, p, span);
  output_string2job(job, &output);
}

static void svg_print_stop(output_string *output, double offset,
                           gvcolor_t color) {
  if (fabs(offset - 0.0) < 0.0005)
    out_puts(output, "<stop offset=\"0\" style=\"stop-color:");
  else if (fabs(offset - 1.0) < 0.0005)
    out_puts(output, "<stop offset=\"1\" style=\"stop-color:");
  else
    gvprintf(output, "<stop offset=\"%.03f\" style=\"stop-color:", offset);
  svg_print_gradient_color(output, color);
  out_puts(output, ";stop-opacity:");
  if (color.type == RGBA_BYTE && color.u.rgba[3] < 255)
    gvprintf(output, "%f", (float)color.u.rgba[3] / 255.0);
  else if (color.type == COLOR_STRING && !strcmp(color.u.string, transparent))
    out_puts(output, "0");
  else
    out_puts(output, "1.");
  out_puts(output, ";\"/>\n");
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

  out_puts(output, "<defs>\n<linearGradient id=\"");
  if (obj->id != NULL) {
    gvputs_xml(output, obj->id);
    out_putc(output, '_');
  }
  gvprintf(output, "l_%d\" gradientUnits=\"userSpaceOnUse\" ", id);
  out_puts(output, "x1=\"");
  gvprintdouble(output, G[0].x);
  out_puts(output, "\" y1=\"");
  gvprintdouble(output, G[0].y);
  out_puts(output, "\" x2=\"");
  gvprintdouble(output, G[1].x);
  out_puts(output, "\" y2=\"");
  gvprintdouble(output, G[1].y);
  out_puts(output, "\" >\n");

  svg_print_stop(output,
                 obj->gradient_frac > 0 ? obj->gradient_frac - 0.001 : 0.0,
                 obj->fillcolor);
  svg_print_stop(output, obj->gradient_frac > 0 ? obj->gradient_frac : 1.0,
                 obj->stopcolor);

  out_puts(output, "</linearGradient>\n</defs>\n");
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
  out_puts(output, "<defs>\n<radialGradient id=\"");
  if (obj->id != NULL) {
    gvputs_xml(output, obj->id);
    out_putc(output, '_');
  }
  gvprintf(output,
           "r_%d\" cx=\"50%%\" cy=\"50%%\" r=\"75%%\" "
           "fx=\"%.0f%%\" fy=\"%.0f%%\">\n",
           id, ifx, ify);

  svg_print_stop(output, 0.0, obj->fillcolor);
  svg_print_stop(output, 1.0, obj->stopcolor);

  out_puts(output, "</radialGradient>\n</defs>\n");
  return id;
}

void svg_ellipse(output_string *output, obj_state_t *obj, pointf *pf,
                 int filled) {
  if (obj->pen == PEN_NONE) {
    return;
  }

  pointf A[] = {
      mid_pointf(pf[0], pf[1]), // center
      pf[1]                     // corner
  };

  int gid = 0;

  /* A[] contains 2 points: the center and corner. */
  if (filled == GRADIENT) {
    gid = svg_gradstyle(output, obj, A, 2);
  } else if (filled == RGRADIENT) {
    gid = svg_rgradstyle(output, obj);
  }
  out_puts(output, "<ellipse");
  svg_grstyle(output, obj, filled, gid);
  out_puts(output, " cx=\"");
  gvprintdouble(output, A[0].x);
  out_puts(output, "\" cy=\"");
  gvprintdouble(output, -A[0].y);
  out_puts(output, "\" rx=\"");
  gvprintdouble(output, A[1].x - A[0].x);
  out_puts(output, "\" ry=\"");
  gvprintdouble(output, A[1].y - A[0].y);
  out_puts(output, "\"/>\n");
}

void jobsvg_ellipse(GVJ_t *job, pointf *pf, int filled) {
  output_string output = job2output_string(job);
  svg_ellipse(&output, job->obj, pf, filled);
  output_string2job(job, &output);
}

void svg_bezier(output_string *output, obj_state_t *obj, pointf *A, size_t n,
                int filled) {
  if (obj->pen != PEN_NONE) {
    int gid = 0;

    if (filled == GRADIENT) {
      gid = svg_gradstyle(output, obj, A, n);
    } else if (filled == RGRADIENT) {
      gid = svg_rgradstyle(output, obj);
    }
    out_puts(output, "<path");
    if (obj->labeledgealigned) {
      out_puts(output, " id=\"");
      gvputs_xml(output, obj->id);
      out_puts(output, "_p\" ");
    }
    svg_grstyle(output, obj, filled, gid);
    out_puts(output, " d=\"");
    svg_bzptarray(output, A, n);
    out_puts(output, "\"/>\n");
  }
}

void jobsvg_bezier(GVJ_t *job, pointf *A, size_t n, int filled) {
  output_string output = job2output_string(job);
  svg_bezier(&output, job->obj, A, n, filled);
  output_string2job(job, &output);
}

void svg_polygon(output_string *output, obj_state_t *obj, pointf *A, size_t n,
                 int filled) {
  int noPoly = 0;
  gvcolor_t save_pencolor;

  if (obj->pen != PEN_NONE) {
    if (filled & NO_POLY) {
      noPoly = 1;
      filled &= ~NO_POLY;
      save_pencolor = obj->pencolor;
      obj->pencolor = obj->fillcolor;
    }
    int gid = 0;
    if (filled == GRADIENT) {
      gid = svg_gradstyle(output, obj, A, n);
    } else if (filled == RGRADIENT) {
      gid = svg_rgradstyle(output, obj);
    }
    out_puts(output, "<polygon");
    svg_grstyle(output, obj, filled, gid);
    out_puts(output, " points=\"");
    for (size_t i = 0; i < n; i++) {
      gvprintdouble(output, A[i].x);
      out_putc(output, ',');
      gvprintdouble(output, -A[i].y);
      out_putc(output, ' ');
    }
    /* repeat the first point because Adobe SVG is broken */
    gvprintdouble(output, A[0].x);
    out_putc(output, ',');
    gvprintdouble(output, -A[0].y);
    out_puts(output, "\"/>\n");

    if (noPoly)
      obj->pencolor = save_pencolor;
  }
}

void jobsvg_polygon(GVJ_t *job, pointf *A, size_t n, int filled) {
  output_string output = job2output_string(job);
  svg_polygon(&output, job->obj, A, n, filled);
  output_string2job(job, &output);
}

void svg_box(output_string *output, obj_state_t *obj, boxf B, int filled) {
  pointf A[4];

  A[0] = B.LL;
  A[2] = B.UR;
  A[1].x = A[0].x;
  A[1].y = A[2].y;
  A[3].x = A[2].x;
  A[3].y = A[0].y;

  svg_polygon(output, obj, A, 4, filled);
}

void jobsvg_box(GVJ_t *job, boxf B, int filled) {
  output_string output = job2output_string(job);
  svg_box(&output, job->obj, B, filled);
  output_string2job(job, &output);
}

void svg_polyline(output_string *output, obj_state_t *obj, pointf *A,
                  size_t n) {
  if (obj->pen != PEN_NONE) {
    out_puts(output, "<polyline");
    svg_grstyle(output, obj, 0, 0);
    out_puts(output, " points=\"");
    for (size_t i = 0; i < n; i++) {
      gvprintdouble(output, A[i].x);
      out_putc(output, ',');
      gvprintdouble(output, -A[i].y);
      if (i + 1 != n) {
        out_putc(output, ' ');
      }
    }
    out_puts(output, "\"/>\n");
  }
}

void jobsvg_polyline(GVJ_t *job, pointf *af, size_t n) {
  output_string output = job2output_string(job);
  svg_polyline(&output, job->obj, af, n);
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
    jobsvg_begin_job,
    0, /* svg_end_job */
    jobsvg_begin_graph,
    jobsvg_end_graph,
    NULL,
    jobsvg_end_layer,
    jobsvg_begin_page,
    jobsvg_end_page,
    jobsvg_begin_cluster,
    jobsvg_end_cluster,
    0, /* svg_begin_nodes */
    0, /* svg_end_nodes */
    0, /* svg_begin_edges */
    0, /* svg_end_edges */
    jobsvg_begin_node,
    jobsvg_end_node,
    jobsvg_begin_edge,
    jobsvg_end_edge,
    jobsvg_begin_anchor,
    jobsvg_end_anchor,
    0, /* svg_begin_anchor */
    0, /* svg_end_anchor */
    jobsvg_textspan,
    0, /* svg_resolve_color */
    jobsvg_ellipse,
    jobsvg_polygon,
    jobsvg_bezier,
    jobsvg_polyline,
    jobsvg_comment,
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

extern bool mapbool(const char *s);

/* font modifiers */
#define REGULAR 0
#define BOLD 1
#define ITALIC 2

static int svg_comparestr(const void *s1, const void *s2) {
  return strcasecmp(s1, *(char *const *)s2);
}

/* gvrender_resolve_color:
 * N.B. strcasecmp cannot be used in bsearch, as it will pass a pointer
 * to an element in the array features->knowncolors (i.e., a char**)
 * as an argument of the compare function, while the arguments to
 * strcasecmp are both char*.
 */
static void svg_resolve_color(char *name, gvcolor_t *color) {
  int rc;
  color->u.string = name;
  color->type = COLOR_STRING;
  const size_t sz_knowncolors = sizeof(svg_knowncolors) / sizeof(char *);
  if (bsearch(name, svg_knowncolors, sz_knowncolors, sizeof(char *),
              svg_comparestr) == NULL) {
    /* if name was not found in known_colors */
    rc = colorxlate(name, color, RGBA_BYTE);
    if (rc != COLOR_OK) {
      if (rc == COLOR_UNKNOWN) {
        agxbuf missedcolor = {0};
        agxbprint(&missedcolor, "color %s", name);
        if (emit_once(agxbuse(&missedcolor)))
          agwarningf("%s is not a known color.\n", name);
        agxbfree(&missedcolor);
      } else {
        agerrorf("error in colorxlate()\n");
      }
    }
  }
}

void svg_set_pencolor(obj_state_t *obj, char *name) {
  gvcolor_t *color = &(obj->pencolor);
  char *cp = NULL;

  if ((cp = strchr(name, ':'))) // if it’s a color list, then use only first
    *cp = '\0';

  svg_resolve_color(name, color);

  if (cp) /* restore color list */
    *cp = ':';
}

void jobsvg_set_pencolor(GVJ_t *job, char *name) {
  svg_set_pencolor(job->obj, name);
}

void svg_set_fillcolor(obj_state_t *obj, char *name) {
  gvcolor_t *color = &(obj->fillcolor);
  char *cp = NULL;

  if ((cp = strchr(name, ':'))) // if it’s a color list, then use only first
    *cp = '\0';

  svg_resolve_color(name, color);

  if (cp)
    *cp = ':';
}

void jobsvg_set_fillcolor(GVJ_t *job, char *name) {
  svg_set_fillcolor(job->obj, name);
}

void svg_set_gradient_vals(obj_state_t *obj, char *stopcolor, int angle,
                           double frac) {
  gvcolor_t *color = &(obj->stopcolor);

  svg_resolve_color(stopcolor, color);

  obj->gradient_angle = angle;
  obj->gradient_frac = frac;
}

void jobsvg_set_gradient_vals(GVJ_t *job, char *stopcolor, int angle,
                              double frac) {
  svg_set_gradient_vals(job->obj, stopcolor, angle, frac);
}

void svg_set_style(obj_state_t *obj, char **s) {
  char *line, *p;

  obj->rawstyle = s;
  if (s)
    while ((p = line = *s++)) {
      if (streq(line, "solid"))
        obj->pen = PEN_SOLID;
      else if (streq(line, "dashed"))
        obj->pen = PEN_DASHED;
      else if (streq(line, "dotted"))
        obj->pen = PEN_DOTTED;
      else if (streq(line, "invis") || streq(line, "invisible"))
        obj->pen = PEN_NONE;
      else if (streq(line, "bold"))
        obj->penwidth = PENWIDTH_BOLD;
      else if (streq(line, "setlinewidth")) {
        while (*p)
          p++;
        p++;
        obj->penwidth = atof(p);
      } else if (streq(line, "filled"))
        obj->fill = FILL_SOLID;
      else if (streq(line, "unfilled"))
        obj->fill = FILL_NONE;
      else if (streq(line, "tapered"))
        ;
      else {
        agwarningf("svg_set_style: unsupported style %s - ignoring\n", line);
      }
    }
}

void jobsvg_set_style(GVJ_t *job, char **s) { svg_set_style(job->obj, s); }

void svg_set_penwidth(obj_state_t *obj, double penwidth) {
  obj->penwidth = penwidth;
}
