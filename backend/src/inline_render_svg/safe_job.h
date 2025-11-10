#ifndef SAFE_JOB_H
#define SAFE_JOB_H

#include "types.h"
/// 1. ONLY CONST FIELDS!!!!!!!!!!!!!!
/// 2. Function can receive only GVJ_t or SafeJob, not both!!!!
typedef struct SafeJob_s {
  const int layerNum;         /* current layer - 1 based*/
  const point pagesArrayElem; /* 2D coord of current page - 0,0 based */
  const pointf dpi;           /* device resolution device-units-per-inch */
  const int rotation;         /* viewport rotation (degrees)  0=portrait,
90=landscape */
  const point pagesArraySize; /* 2D size of page array */
  const box pageBoundingBox;  /* rotated boundingBox - device units */
  const unsigned int width;   /* device width - device units */
  const unsigned int height;  /* device height - device units */
  const pointf scale;       /* composite device to graph units (zoom and dpi) */
  const pointf translation; /* composite translation */
  const boxf clip;          /* clip region in graph units */

  // from gvc:
  const graph_t *const graph;
  char **defaultlinestyle; /* default line style */
  const int viewNum; ///< current view - 1 based count of views, all pages in
                     ///< all layers
  char **const layerIDs;       /* array of layer names */
  char *const layerDelims;     /* delimiters in layer names */
  char *const layerListDelims; /* delimiters between layer ranges */
  const int numLayers;         /* number of layers */
} SafeJob;

SafeJob to_safe_job(GVJ_t *job);

#endif
