#include "types.h"

extern point get_image_dimensions_by_name_in_points(const char *name);

point get_dimensions_by_name(const char *name, double dpi) {
  point size_in_points = get_image_dimensions_by_name_in_points(name);

  point rv;
  rv.x = (int)(size_in_points.x * POINTS_PER_INCH / dpi);
  rv.y = (int)(size_in_points.y * POINTS_PER_INCH / dpi);
  return rv;
}

#define DEFAULT_DPI 96
point gvusershape_size(graph_t *g, char *name) {
  double dpi = GD_drawing(g)->dpi;
  if (dpi < 1.0)
    dpi = DEFAULT_DPI;
  return get_dimensions_by_name(name, dpi);
}
