#include "geom.h"
#include "types.h"
#include <stdio.h>

#define DEFAULT_DPI 96

point convert_image_dimensions(pointf dpi, uint64_t heightPt,
                               uint64_t widthPt) {
  point rv;
  rv.x = (int)(widthPt * POINTS_PER_INCH / dpi.x);
  rv.y = (int)(heightPt * POINTS_PER_INCH / dpi.y);

  return rv;
}

point my_gvusershape_size(Agraph_t *g, uint64_t heightPt, uint64_t widthPt) {
  pointf dpi;

  if ((dpi.y = GD_drawing(g)->dpi) >= 1.0)
    dpi.x = dpi.y;
  else
    dpi.x = dpi.y = DEFAULT_DPI;

  return convert_image_dimensions(dpi, heightPt, widthPt);
}
