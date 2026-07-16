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

#include "types.h"
#include "const.h"
#include "utils.h"
#include "util/unreachable.h"
#include "geomprocs.h"
#include "streq.h"

#include "safe_job.h"
#include "core_svg.h"
#include "gvio_svg.h"
#include "internal_render_svg.h"
#include "colortbl.h"
#include "../output_string.h"

static void hsv2rgb(double h, double s, double v, double *r, double *g,
                    double *b) {
  int i;
  double f, p, q, t;

  if (s <= 0.0) { /* achromatic */
    *r = v;
    *g = v;
    *b = v;
  } else {
    if (h >= 1.0)
      h = 0.0;
    h = 6.0 * h;
    i = (int)h;
    f = h - i;
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));
    switch (i) {
    case 0:
      *r = v;
      *g = t;
      *b = p;
      break;
    case 1:
      *r = q;
      *g = v;
      *b = p;
      break;
    case 2:
      *r = p;
      *g = v;
      *b = t;
      break;
    case 3:
      *r = p;
      *g = q;
      *b = v;
      break;
    case 4:
      *r = t;
      *g = p;
      *b = v;
      break;
    case 5:
      *r = v;
      *g = p;
      *b = q;
      break;
    default:
      UNREACHABLE();
    }
  }
}

/* fullColor:
 * Return "/prefix/str"
 */
static char *fullColor(agxbuf *xb, const char *prefix, const char *str) {
  agxbprint(xb, "/%s/%s", prefix, str);
  return agxbuse(xb);
}

static int colorcmpf(const void *p0, const void *p1) {
  return strcasecmp(p0, ((const hsvrgbacolor_t *)p1)->name);
}

/* resolveColor:
 * Resolve input color str allowing color scheme namespaces.
 *  0) "black" => "black"
 *     "white" => "white"
 *     "lightgrey" => "lightgrey"
 *    NB: This is something of a hack due to the remaining codegen.
 *        Once these are gone, this case could be removed and all references
 *        to "black" could be replaced by "/X11/black".
 *  1) No initial / =>
 *          if colorscheme is defined and no "X11", return /colorscheme/str
 *          else return str
 *  2) One initial / => return str+1
 *  3) Two initial /'s =>
 *       a) If colorscheme is defined and not "X11", return /colorscheme/(str+2)
 *       b) else return (str+2)
 *  4) Two /'s, not both initial => return str.
 *
 * Note that 1), 2), and 3b) allow the default X11 color scheme.
 *
 * In other words,
 *   xxx => /colorscheme/xxx     if colorscheme is defined and not "X11"
 *   xxx => xxx                  otherwise
 *   /xxx => xxx
 *   /X11/yyy => yyy
 *   /xxx/yyy => /xxx/yyy
 *   //yyy => /colorscheme/yyy   if colorscheme is defined and not "X11"
 *   //yyy => yyy                otherwise
 *
 * At present, no other error checking is done. For example,
 * yyy could be "". This will be caught later.
 */

#define DFLT_SCHEME "X11/" /* Must have final '/' */
#define DFLT_SCHEME_LEN ((sizeof(DFLT_SCHEME) - 1) / sizeof(char))
#define ISNONDFLT(s)                                                           \
  ((s) && *(s) && strncasecmp(DFLT_SCHEME, s, DFLT_SCHEME_LEN - 1))
static char *colorscheme;

char *setColorScheme(const char *s) {
  char *previous = colorscheme;
  colorscheme = s == NULL ? NULL : gv_strdup(s);
  return previous;
}

static char *resolveColor(const char *str) {
  const char *s;

  if (!strcmp(str, "black"))
    return strdup(str);
  if (!strcmp(str, "white"))
    return strdup(str);
  if (!strcmp(str, "lightgrey"))
    return strdup(str);
  agxbuf xb = {0};
  if (*str == '/') {                        /* if begins with '/' */
    const char *const c2 = str + 1;         // second char
    const char *const ss = strchr(c2, '/'); // second slash
    if (ss != NULL) {                       // if has second '/'
      if (*c2 == '/') { /* if second '/' is second character */
                        /* Do not compare against final '/' */
        if (ISNONDFLT(colorscheme))
          s = fullColor(&xb, colorscheme, c2 + 1);
        else
          s = c2 + 1;
      } else if (strncasecmp(DFLT_SCHEME, c2, DFLT_SCHEME_LEN))
        s = str;
      else
        s = ss + 1;
    } else
      s = c2;
  } else if (ISNONDFLT(colorscheme))
    s = fullColor(&xb, colorscheme, str);
  else
    s = str;
  char *on_heap = strdup(s);
  agxbfree(&xb);
  return on_heap;
}

static void my_colorxlate(const char *str, gvcolor_t *color) {
  for (; *str == ' '; str++)
    ; /* skip over any leading whitespace */

  /* test for rgb value such as: "#ff0000"
     or rgba value such as "#ff000080" */
  unsigned a = 255; // default alpha channel value=opaque in case not supplied
  unsigned int r, g, b;
  bool is_rgb = sscanf(str, "#%2x%2x%2x%2x", &r, &g, &b, &a) >= 3;
  if (!is_rgb) { // try 3 letter form
    is_rgb = strlen(str) == 4 && sscanf(str, "#%1x%1x%1x", &r, &g, &b) == 3;
    if (is_rgb) {
      r |= r << 4;
      g |= g << 4;
      b |= b << 4;
    }
  }
  if (is_rgb) {
    color->type = RGBA_BYTE;
    color->u.rgba[0] = (unsigned char)r;
    color->u.rgba[1] = (unsigned char)g;
    color->u.rgba[2] = (unsigned char)b;
    color->u.rgba[3] = (unsigned char)a;
    return;
  }

  /* test for hsv value such as: ".6,.5,.3" */
  const char *p = str;
  char c = *p;
  if (c == '.' || (c >= '0' && c <= '9')) {
    agxbuf canon = {0};
    while ((c = *p++)) {
      agxbputc(&canon, c == ',' ? ' ' : c);
    }

    double H, S, V, A, R, G, B;
    A = 1.0; // default
    if (sscanf(agxbuse(&canon), "%lf%lf%lf%lf", &H, &S, &V, &A) >= 3) {
      /* clip to reasonable values */
      H = fmax(fmin(H, 1.0), 0.0);
      S = fmax(fmin(S, 1.0), 0.0);
      V = fmax(fmin(V, 1.0), 0.0);
      A = fmax(fmin(A, 1.0), 0.0);
      hsv2rgb(H, S, V, &R, &G, &B);
      color->type = RGBA_BYTE;
      color->u.rgba[0] = (unsigned char)(R * 255);
      color->u.rgba[1] = (unsigned char)(G * 255);
      color->u.rgba[2] = (unsigned char)(B * 255);
      color->u.rgba[3] = (unsigned char)(A * 255);
      agxbfree(&canon);
      return;
    }
    agxbfree(&canon);
  }

  /* test for known color name (generic, not renderer specific known names) */
  char *name = resolveColor(str);
  if (!name)
    return;
  const hsvrgbacolor_t *known =
      bsearch(name, color_lib, sizeof(color_lib) / sizeof(hsvrgbacolor_t),
              sizeof(color_lib[0]), colorcmpf);
  free(name);
  if (known != NULL) {
    color->type = RGBA_BYTE;
    color->u.rgba[0] = known->r;
    color->u.rgba[1] = known->g;
    color->u.rgba[2] = known->b;
    color->u.rgba[3] = known->a;
    return;
  }

  /* if we're still here then we failed to find a valid color spec */
  agxbuf missedcolor = {0};
  agxbprint(&missedcolor, "color %s", name);
  if (emit_once(agxbuse(&missedcolor)))
    agwarningf("%s is not a known color.\n", name);
  agxbfree(&missedcolor);

  color->type = RGBA_BYTE;
  color->u.rgba[0] = color->u.rgba[1] = color->u.rgba[2] = 0;
  color->u.rgba[3] = 255; /* opaque */
}

char *svg_defaultlinestyle[3] = {"solid\0", "setlinewidth\0001\0", 0};

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

/* svg_print_paint assumes the caller will set the opacity if the alpha channel
 * is greater than 0 and less than 255
 */
static void svg_print_paint(output_string *output, gvcolor_t color) {
  switch (color.type) {
  case COLOR_STRING:
    if (!strcmp(color.u.string, "transparent"))
      out_puts(output, "none");
    else
      out_puts(output, color.u.string);
    break;
  case RGBA_BYTE:
    if (color.u.rgba[3] == 0) /* transparent */
      out_puts(output, "none");
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
    if (!strcmp(color.u.string, "transparent"))
      out_puts(output, "black");
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

/* svg_begin_page:
 * Currently, svg output does not support pages.
 * FIX: If implemented, we must guarantee the id is unique.
 */
void svg_begin_page(output_string *output, SafeLayer *safe_layer,
                    obj_state_t *obj) {
  pointf scale; /* composite device to graph units (zoom and dpi) */
  scale.x = safe_layer->safe_job->zoom * safe_layer->safe_job->dpi.x /
            POINTS_PER_INCH;
  scale.y = safe_layer->safe_job->zoom * safe_layer->safe_job->dpi.y /
            POINTS_PER_INCH;

  /* its really just a page of the graph, but its still a graph,
   * and it is the entire graph if we're not currently paging */
  out_puts(output, "<g");
  svg_print_id(output, obj->id, NULL);
  svg_print_class(output, "graph", obj->u.g);
  out_puts(output, " transform=\"scale(");
  // cannot be gvprintdouble because 2 digits precision insufficient
  gvprintf(output, "%g %g", scale.x, scale.y);
  gvprintf(output, ") rotate(%d) translate(", -safe_layer->safe_job->rotation);

  /* CAUTION - job->translation was difficult to get right. */
  // Test with and without asymmetric margins, e.g: -Gmargin="1,0"
  double translation_y = 0;
  double translation_x = 0;
  if (safe_layer->safe_job->rotation) {
    translation_y =
        -safe_layer->safe_job->clip.UR.y -
        safe_layer->safe_job->canvasBox.LL.y / safe_layer->safe_job->zoom;
    translation_x =
        -safe_layer->safe_job->clip.UR.x -
        safe_layer->safe_job->canvasBox.LL.x / safe_layer->safe_job->zoom;
  } else {
    /* pre unscale margins to keep them constant under scaling */
    translation_x =
        -safe_layer->safe_job->clip.LL.x +
        safe_layer->safe_job->canvasBox.LL.x / safe_layer->safe_job->zoom;
    translation_y =
        -safe_layer->safe_job->clip.UR.y -
        safe_layer->safe_job->canvasBox.LL.y / safe_layer->safe_job->zoom;
  }

  gvprintdouble(output, translation_x);
  out_putc(output, ' ');
  gvprintdouble(output, -translation_y);
  out_puts(output, ")\">\n");
  /* default style */
  if (agnameof(obj->u.g)[0] && agnameof(obj->u.g)[0] != LOCALNAMEPREFIX) {
    out_puts(output, "<title>");
    gvputs_xml(output, agnameof(obj->u.g));
    out_puts(output, "</title>\n");
  }
}

void svg_end_page(output_string *output) { out_puts(output, "</g>\n"); }

void svg_end_cluster(output_string *output) { out_puts(output, "</g>\n"); }

void svg_begin_edge(output_string *output, obj_state_t *obj) {
  out_puts(output, "<g");
  svg_print_id(output, obj->id, NULL);
  svg_print_class(output, "edge", obj->u.e);
  out_puts(output, ">\n<title>");
  char *ename = strdup_and_subst_obj("\\E", obj->u.e);
  gvputs_xml(output, ename);
  free(ename);
  out_puts(output, "</title>\n");
}

void svg_end_edge(output_string *output) { out_puts(output, "</g>\n"); }

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

void svg_end_anchor(output_string *output) {
  out_puts(output, "</a>\n"
                   "</g>\n");
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
  else if (color.type == COLOR_STRING && !strcmp(color.u.string, "transparent"))
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

void svg_bezier(output_string *output, obj_state_t *obj, pointf *A, size_t n,
                int filled) {
  if (obj->pen == PEN_NONE) {
    return;
  }
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

extern bool mapbool(const char *s);
static int svg_comparestr(const void *s1, const void *s2) {
  return strcasecmp(s1, *(char *const *)s2);
}

/* gvrender_resolve_color:
 * N.B. strcasecmp cannot be used in bsearch, as it will pass a pointer
 * to an element in the array features->knowncolors (i.e., a char**)
 * as an argument of the compare function, while the arguments to
 * strcasecmp are both char*.
 */
gvcolor_t svg_resolve_color(char *name) {
  gvcolor_t color = {0};
  char *cp = NULL;

  if ((cp = strchr(name, ':'))) // if it’s a color list, then use only first
    *cp = '\0';

  color.u.string = name;
  color.type = COLOR_STRING;
  const size_t sz_knowncolors = sizeof(svg_knowncolors) / sizeof(char *);
  if (bsearch(name, svg_knowncolors, sz_knowncolors, sizeof(char *),
              svg_comparestr) == NULL) {
    /* if name was not found in known_colors */
    my_colorxlate(name, &color);
  }

  if (cp) /* restore color list */
    *cp = ':';

  return color;
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
