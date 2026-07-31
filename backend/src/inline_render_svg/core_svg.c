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

#include "agxbuf.h"
#include "color.h"
#include "types.h"
#include "const.h"
#include "utils.h"
#include "util/unreachable.h"
#include "geomprocs.h"
#include "streq.h"

#include "core_svg.h"
#include "gvio_svg.h"
#include "internal_render_svg.h"
#include "../output_string.h"

imagescale_t get_imagescale(char *s) {
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

imagepos_t get_imagepos(char *s) {
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

boxf compute_polygon_bb(pointf *A, size_t n) {
  assert(n > 2);
  pointf min = A[0];
  pointf max = A[0];
  for (size_t i = 1; i < n; ++i) {
    pointf p = A[i];
    if (p.x < min.x) {
      min.x = p.x;
    } else if (p.x > max.x) {
      max.x = p.x;
    }

    if (p.y < min.y) {
      min.y = p.y;
    } else if (p.y > max.y) {
      max.y = p.y;
    }
  }
  return (boxf){.LL = min, .UR = max};
}

extern point get_dimensions_by_name(const char *name, pointf dpi);
/* gvrender_usershape:
 * Scale image to fill polygon bounding box accordingus to "imagescale",
 * positioned at "imagepos"
 */
void svg_usershape(output_string *output, int rotation_deg, pointf dpi,
                   char *name, pointf *a, size_t n, imagescale_t imagescale,
                   imagepos_t imagepos) {
  assert(name);
  assert(name[0]);

  point isz = get_dimensions_by_name(name, dpi);

  if ((isz.x <= 0) && (isz.y <= 0))
    return;

  /* compute bb of polygon */
  boxf b = compute_polygon_bb(a, n);

  double pw = b.UR.x - b.LL.x;
  double ph = b.UR.y - b.LL.y;
  double ih = (double)isz.y;
  double iw = (double)isz.x;

  /* scale factors */
  double scalex = pw / iw;
  double scaley = ph / ih;

  switch (imagescale) {
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
  if (iw < pw) {
    switch (imagepos) {
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
    switch (imagepos) {
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

void svg_print_id(output_string *output, char *id, char *idx) {
  out_puts(output, " id=\"");
  gvputs_xml(output, id);
  if (idx) {
    out_putc(output, '_');
    gvputs_xml(output, idx);
  }
  out_putc(output, '"');
}

void svg_print_class(output_string *output, char *kind, void *obj) {
  gvprintf(output, " class=\"%s", kind);
  char *str = agget(obj, "class");
  if (str && *str) {
    out_putc(output, ' ');
    gvputs_xml(output, str);
  }
  out_putc(output, '"');
}

static bool isTransparent(gvcolor_t color) {
  switch (color.type) {
  case RGBA_BYTE:
    return color.u.rgba[3] == 0;
  default:
    return false;
  }
}

/* svg_print_paint assumes the caller will set the opacity if the alpha channel
 * is greater than 0 and less than 255
 */
static void svg_print_color(output_string *output, gvcolor_t color) {
  if (isTransparent(color)) {
    out_puts(output, "none");
    return;
  }

  switch (color.type) {
  case COLOR_STRING:
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
  switch (filled) {
  case 0:
    out_puts(output, "none");
    break;
  case GRADIENT:
    out_puts(output, "url(#");
    if (obj->id != NULL) {
      gvputs_xml(output, obj->id);
      out_putc(output, '_');
    }
    gvprintf(output, "l_%d)", gid);
    break;
  case RGRADIENT:
    out_puts(output, "url(#");
    if (obj->id != NULL) {
      gvputs_xml(output, obj->id);
      out_putc(output, '_');
    }
    gvprintf(output, "r_%d)", gid);
    break;
  default:
    svg_print_color(output, obj->fillcolor);
    if (obj->fillcolor.type == RGBA_BYTE && obj->fillcolor.u.rgba[3] > 0 &&
        obj->fillcolor.u.rgba[3] < 255)
      gvprintf(output, "\" fill-opacity=\"%f",
               (float)obj->fillcolor.u.rgba[3] / 255.0);
  }

  out_puts(output, "\" stroke=\"");
  svg_print_color(output, obj->pencolor);
  // will `gvprintdouble` output something different from `PENWIDTH_NORMAL`?
  const double GVPRINT_DOUBLE_THRESHOLD = 0.005;
  if (!(fabs(obj->penwidth - PENWIDTH_NORMAL) < GVPRINT_DOUBLE_THRESHOLD)) {
    out_puts(output, "\" stroke-width=\"");
    gvprintdouble(output, obj->penwidth);
  }
  /* SVG dash array */
  if (obj->pen == PEN_DASHED) {
    out_puts(output, "\" stroke-dasharray=\"5,2");
  } else if (obj->pen == PEN_DOTTED) {
    out_puts(output, "\" stroke-dasharray=\"1,5");
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

void svg_begin_anchor(output_string *output, char *href, char *tooltip,
                      char *target, char *id) {
  out_puts(output, "<g");
  if (id) {
    out_puts(output, " id=\"a_");
    gvputs_xml(output, id);
    out_putc(output, '"');
  }
  out_puts(output, "><a");
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

void svg_end_anchor(output_string *output) { out_puts(output, "</a>\n</g>\n"); }

void svg_textspan(output_string *output, fontname_kind fontnames,
                  obj_state_t *obj, pointf p, textspan_t *span) {
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
  if (!obj->labeledgealigned) {
    out_puts(output, " x=\"");
    gvprintdouble(output, p.x);
    out_puts(output, "\" y=\"");
    gvprintdouble(output, -p.y);
    out_puts(output, "\"");
  }

  PostscriptAlias *pA = span->font->postscript_alias;
  char *family = NULL, *weight = NULL, *style = NULL;
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

    gvprintf(output, " font-family=\"%s", family);
    if (pA->svg_font_family && pA->svg_font_family != family)
      gvprintf(output, ",%s", pA->svg_font_family);
    out_putc(output, '"');
    if (weight)
      gvprintf(output, " font-weight=\"%s\"", weight);

    char *stretch = pA->stretch;
    if (stretch)
      gvprintf(output, " font-stretch=\"%s\"", stretch);
    if (style)
      gvprintf(output, " font-style=\"%s\"", style);
  } else
    gvprintf(output, " font-family=\"%s\"", span->font->name);

  unsigned int flags = span->font->flags;
  if (flags != 0) {
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

static void svg_print_stop(output_string *output, double offset,
                           gvcolor_t color) {
  if (fabs(offset - 0.0) < 0.0005)
    out_puts(output, "<stop offset=\"0\" style=\"stop-color:");
  else if (fabs(offset - 1.0) < 0.0005)
    out_puts(output, "<stop offset=\"1\" style=\"stop-color:");
  else
    gvprintf(output, "<stop offset=\"%.03f\" style=\"stop-color:", offset);

  // "transparent" in SVG 2 gradients is considered to be black with 0 opacity,
  // so for compatibility with SVG 1.1 output we use black when the color string
  // is transparent and assume the caller will also check and set opacity 0.
  if (isTransparent(color))
    out_puts(output, "black");
  else
    svg_print_color(output, color);

  out_puts(output, ";stop-opacity:");
  if (color.type == RGBA_BYTE && color.u.rgba[3] < 255)
    gvprintf(output, "%f", (float)color.u.rgba[3] / 255.0);
  else
    out_puts(output, "1.");
  out_puts(output, ";\"/>\n");
}

/* svg_gradstyle
 * Outputs the SVG statements that define the gradient pattern
 */
static int svg_define_linearGradient(output_string *output, obj_state_t *obj,
                                     boxf bb) {
  static int gradId;
  int id = gradId++;

  double angle = obj->gradient_angle * M_PI / 180;
  pointf center = mid_pointf(bb.LL, bb.UR);
  pointf half = {(bb.UR.x - bb.LL.x) / 2 * cos(angle),
                 (bb.UR.y - bb.LL.y) / 2 * sin(angle)};
  pointf start = sub_pointf(center, half);
  pointf end = add_pointf(center, half);

  out_puts(output, "<defs>\n<linearGradient id=\"");
  if (obj->id != NULL) {
    gvputs_xml(output, obj->id);
    out_putc(output, '_');
  }
  gvprintf(output, "l_%d\" gradientUnits=\"userSpaceOnUse\" ", id);
  out_puts(output, "x1=\"");
  gvprintdouble(output, start.x);
  out_puts(output, "\" y1=\"");
  gvprintdouble(output, -start.y);
  out_puts(output, "\" x2=\"");
  gvprintdouble(output, end.x);
  out_puts(output, "\" y2=\"");
  gvprintdouble(output, -end.y);
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
static int svg_define_radialGradient(output_string *output, obj_state_t *obj) {
  static int rgradId;
  int id = rgradId++;

  double ifx = 50, ify = 50;
  if (obj->gradient_angle != 0) {
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

void svg_ellipse(output_string *output, obj_state_t *obj, pointf center,
                 pointf radius, int filled) {
  if (obj->pen == PEN_NONE) {
    return;
  }

  int gid = 0;
  /* A[] contains 2 points: the center and corner. */
  if (filled == GRADIENT) {
    boxf bb = {.LL = sub_pointf(center, radius),
               .UR = add_pointf(center, radius)};
    gid = svg_define_linearGradient(output, obj, bb);
  } else if (filled == RGRADIENT) {
    gid = svg_define_radialGradient(output, obj);
  }
  out_puts(output, "<ellipse");
  svg_grstyle(output, obj, filled, gid);
  out_puts(output, " cx=\"");
  gvprintdouble(output, center.x);
  out_puts(output, "\" cy=\"");
  gvprintdouble(output, -center.y);
  out_puts(output, "\" rx=\"");
  gvprintdouble(output, radius.x);
  out_puts(output, "\" ry=\"");
  gvprintdouble(output, radius.y);
  out_puts(output, "\"/>\n");
}

void svg_bezier(output_string *output, obj_state_t *obj, pointf *A, size_t n,
                int filled) {
  if (obj->pen == PEN_NONE) {
    return;
  }

  int gid = 0;
  if (filled == GRADIENT) {
    boxf bb = compute_polygon_bb(A, n);
    gid = svg_define_linearGradient(output, obj, bb);
  } else if (filled == RGRADIENT) {
    gid = svg_define_radialGradient(output, obj);
  }

  out_puts(output, "<path");
  if (obj->labeledgealigned) {
    out_puts(output, " id=\"");
    gvputs_xml(output, obj->id);
    out_puts(output, "_p\" ");
  }
  svg_grstyle(output, obj, filled, gid);

  out_puts(output, " d=\"");
  char c = 'M'; /* first point */
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

  out_puts(output, "\"/>\n");
}

void svg_polygon(output_string *output, obj_state_t *obj, pointf *A, size_t n,
                 int filled) {
  if (obj->pen == PEN_NONE) {
    return;
  }

  int noPoly = 0;
  gvcolor_t save_pencolor;

  if (filled & NO_POLY) {
    noPoly = 1;
    filled &= ~NO_POLY;
    save_pencolor = obj->pencolor;
    obj->pencolor = obj->fillcolor;
  }
  int gid = 0;
  if (filled == GRADIENT) {
    boxf bb = compute_polygon_bb(A, n);
    gid = svg_define_linearGradient(output, obj, bb);
  } else if (filled == RGRADIENT) {
    gid = svg_define_radialGradient(output, obj);
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

bool resolveColor(const char *str, gvcolor_t *result);
gvcolor_t svg_resolve_color(char *name) {
  gvcolor_t color = {0};

  char *cp = strchr(name, ':');
  if (cp != NULL) // if it’s a color list, then use only first
    *cp = '\0';

  if (!resolveColor(name, &color)) {
    // if we here then we failed to find a valid color spec
    agxbuf missedcolor = {0};
    agxbprint(&missedcolor, "color %s", name);
    if (emit_once(agxbuse(&missedcolor)))
      agwarningf("%s is not a known color.\n", name);
    agxbfree(&missedcolor);

    color.type = RGBA_BYTE;
    color.u.rgba[0] = 0;
    color.u.rgba[1] = 0;
    color.u.rgba[2] = 0;
    color.u.rgba[3] = 255;
  }

  if (cp) /* restore color list */
    *cp = ':';

  return color;
}

void svg_set_style(obj_state_t *obj, char **s) {
  char *line, *p;
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
