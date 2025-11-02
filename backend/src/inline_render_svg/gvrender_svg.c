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

void gvrender_set_penwidth(GVJ_t *job, double penwidth) {
  gvrender_engine_t *gvre = job->render.engine;

  if (gvre) {
    job->obj->penwidth = penwidth;
  }
}
