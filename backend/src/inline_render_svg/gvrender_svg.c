/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

/*
 *  graphics code generator wrapper
 *
 *  This library forms the socket for run-time loadable render plugins.
 */

// clang-format off
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <const.h>
#include <macros.h>
#include <colorprocs.h>
#include <gvplugin_render.h>
#include <cgraph.h>
#include <gvcint.h>
#include <geom.h>
#include <geomprocs.h>
#include <gvcproc.h>
#include <limits.h>
#include <stdlib.h>
#include <util/agxbuf.h>
#include <util/alloc.h>
#include <util/strcasecmp.h>
#include <util/streq.h>

#include "render_svg.h"
// clang-format on

extern bool mapbool(const char *s);

/* font modifiers */
#define REGULAR 0
#define BOLD 1
#define ITALIC 2

pointf gvrender_ptf(GVJ_t *job, pointf p) {
  pointf rv, translation, scale;

  translation = job->translation;
  scale.x = job->zoom * job->devscale.x;
  scale.y = job->zoom * job->devscale.y;

  if (job->rotation) {
    rv.x = -(p.y + translation.y) * scale.x;
    rv.y = (p.x + translation.x) * scale.y;
  } else {
    rv.x = (p.x + translation.x) * scale.x;
    rv.y = (p.y + translation.y) * scale.y;
  }
  return rv;
}

static int gvrender_comparestr(const void *s1, const void *s2) {
  return strcasecmp(s1, *(char *const *)s2);
}

/* gvrender_resolve_color:
 * N.B. strcasecmp cannot be used in bsearch, as it will pass a pointer
 * to an element in the array features->knowncolors (i.e., a char**)
 * as an argument of the compare function, while the arguments to
 * strcasecmp are both char*.
 */
extern gvrender_features_t my_render_features_svg;
static void gvrender_resolve_color(gvrender_features_t *features, char *name,
                                   gvcolor_t *color) {
  int rc;
  color->u.string = name;
  color->type = COLOR_STRING;
  if (bsearch(name, my_render_features_svg.knowncolors,
              my_render_features_svg.sz_knowncolors, sizeof(char *),
              gvrender_comparestr) == NULL) {
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

void gvrender_set_pencolor(GVJ_t *job, char *name) {
  gvrender_engine_t *gvre = job->render.engine;
  gvcolor_t *color = &(job->obj->pencolor);
  char *cp = NULL;

  if ((cp = strchr(name, ':'))) // if it’s a color list, then use only first
    *cp = '\0';
  if (gvre) {
    gvrender_resolve_color(job->render.features, name, color);
    // if (gvre->resolve_color)
    //   gvre->resolve_color(job, color);
  }
  if (cp) /* restore color list */
    *cp = ':';
}

void gvrender_set_fillcolor(GVJ_t *job, char *name) {
  gvrender_engine_t *gvre = job->render.engine;
  gvcolor_t *color = &(job->obj->fillcolor);
  char *cp = NULL;

  if ((cp = strchr(name, ':'))) // if it’s a color list, then use only first
    *cp = '\0';
  if (gvre) {
    gvrender_resolve_color(job->render.features, name, color);
    // if (gvre->resolve_color)
    //   gvre->resolve_color(job, color);
  }
  if (cp)
    *cp = ':';
}

void gvrender_set_gradient_vals(GVJ_t *job, char *stopcolor, int angle,
                                double frac) {
  gvrender_engine_t *gvre = job->render.engine;
  gvcolor_t *color = &(job->obj->stopcolor);

  if (gvre) {
    gvrender_resolve_color(job->render.features, stopcolor, color);
    // if (gvre->resolve_color)
    //   gvre->resolve_color(job, color);
  }
  job->obj->gradient_angle = angle;
  job->obj->gradient_frac = frac;
}

void gvrender_set_style(GVJ_t *job, char **s) {
  gvrender_engine_t *gvre = job->render.engine;
  obj_state_t *obj = job->obj;
  char *line, *p;

  obj->rawstyle = s;
  if (gvre) {
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
          agwarningf("gvrender_set_style: unsupported style %s - ignoring\n",
                     line);
        }
      }
  }
}

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

/* gvrender_usershape:
 * Scale image to fill polygon bounding box according to "imagescale",
 * positioned at "imagepos"
 */
void gvrender_usershape(GVJ_t *job, char *name, pointf *a, size_t n,
                        bool filled, char *imagescale, char *imagepos) {
  gvrender_engine_t *gvre = job->render.engine;
  usershape_t *us;
  double iw, ih, pw, ph;
  double scalex, scaley; /* scale factors */
  boxf b;                /* target box */
  point isz;
  imagepos_t position;

  assert(job);
  assert(name);
  assert(name[0]);

  if (!(us = gvusershape_find(name))) {
    return;
  }

  isz = gvusershape_size_dpi(us, job->dpi);
  if ((isz.x <= 0) && (isz.y <= 0))
    return;

  /* compute bb of polygon */
  b.LL = b.UR = a[0];
  for (size_t i = 1; i < n; i++) {
    expandbp(&b, a[i]);
  }

  pw = b.UR.x - b.LL.x;
  ph = b.UR.y - b.LL.y;
  ih = (double)isz.y;
  iw = (double)isz.x;

  scalex = pw / iw;
  scaley = ph / ih;

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
  position = get_imagepos(imagepos);
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

  /* convert from graph to device coordinates */
  if (!(job->flags & GVRENDER_DOES_TRANSFORM)) {
    b.LL = gvrender_ptf(job, b.LL);
    b.UR = gvrender_ptf(job, b.UR);
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
  if (gvre) {
    gvloadimage(job, us, b, filled, job->render.type);
  }
}

void gvrender_set_penwidth(GVJ_t *job, double penwidth) {
  gvrender_engine_t *gvre = job->render.engine;

  if (gvre) {
    job->obj->penwidth = penwidth;
  }
}
