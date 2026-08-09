#include "geom.h"
#include "types.h"
#include <stdio.h>

#define DEFAULT_DPI 96

point convert_image_dimensions(double dpi, uint64_t heightPt,
                               uint64_t widthPt) {
  point rv;
  rv.x = (int)(widthPt * POINTS_PER_INCH / dpi);
  rv.y = (int)(heightPt * POINTS_PER_INCH / dpi);

  return rv;
}

point my_gvusershape_size(Agraph_t *g, uint64_t heightPt, uint64_t widthPt) {
  double dpi = GD_drawing(g)->dpi;
  if (dpi < 1.0)
    dpi = DEFAULT_DPI;

  return convert_image_dimensions(dpi, heightPt, widthPt);
}
