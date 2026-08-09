#ifndef GVUSERSHAPE_SIZE_H_
#define GVUSERSHAPE_SIZE_H_

#include <stdint.h>
#include "geom.h"

typedef struct pointf_s pointf;
typedef struct Agraph_s Agraph_t;

point convert_image_dimensions(pointf dpi, uint64_t heightPt, uint64_t widthPt);
point my_gvusershape_size(Agraph_t *g, uint64_t heightPt, uint64_t widthPt);
#endif
