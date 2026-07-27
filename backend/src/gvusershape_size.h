#ifndef GVUSERSHAPE_SIZE_H_
#define GVUSERSHAPE_SIZE_H_

#include "geom.h"

typedef struct pointf_s pointf;
typedef struct Agraph_s Agraph_t;

point convert_image_dimensions(pointf dpi, const char *raw_height,
                               const char *raw_width);
point my_gvusershape_size(Agraph_t *g, const char *raw_height,
                          const char *raw_width);
#endif
