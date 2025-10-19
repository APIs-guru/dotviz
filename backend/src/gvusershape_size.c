#include "geom.h"
#include "types.h"
#include <stdio.h>

#define DEFAULT_DPI 96

static int svg_units_convert(double n, char *u) {
  if (strcmp(u, "in") == 0)
    return round(n * POINTS_PER_INCH);
  if (strcmp(u, "px") == 0)
    return round(n * POINTS_PER_INCH / 96);
  if (strcmp(u, "pc") == 0)
    return round(n * POINTS_PER_INCH / 6);
  if (strcmp(u, "pt") == 0 ||
      strcmp(u, "\"") == 0) /* ugly!!  - if there are no inits then the %2s get
                               the trailing '"' */
    return round(n);
  if (strcmp(u, "cm") == 0)
    return round(n * POINTS_PER_CM);
  if (strcmp(u, "mm") == 0)
    return round(n * POINTS_PER_MM);
  return 0;
}

// point my_gvusershape_size(graph_t *g, char *name)
point my_gvusershape_size(graph_t *g, const char *my_height,
                          const char *my_width) {
  point rv;
  pointf dpi;

  if ((dpi.y = GD_drawing(g)->dpi) >= 1.0)
    dpi.x = dpi.y;
  else
    dpi.x = dpi.y = DEFAULT_DPI;

  double n;
  char u[3];

  int w = 0;
  int h = 0;
  if (sscanf(my_width, "%lf%2s", &n, u) == 2) {
    w = svg_units_convert(n, u);
  } else if (sscanf(my_width, "%lf", &n) == 1) {
    w = svg_units_convert(n, "pt");
  }

  if (sscanf(my_height, "%lf%2s", &n, u) == 2) {
    h = svg_units_convert(n, u);

  } else if (sscanf(my_height, "%lf", &n) == 1) {
    h = svg_units_convert(n, "pt");
  }

  rv.x = (int)(w * POINTS_PER_INCH / dpi.x);
  rv.y = (int)(h * POINTS_PER_INCH / dpi.y);

  return rv;
}