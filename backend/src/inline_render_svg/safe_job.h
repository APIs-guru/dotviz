#ifndef SAFE_JOB_H
#define SAFE_JOB_H

#include "const.h"
#include "types.h"

/// 1. ONLY CONST FIELDS!!!!!!!!!!!!!!
/// 2. Function can receive only GVJ_t or SafeJob, not both!!!!
typedef struct SafeJob_s {
  const int layerNum;         /* current layer - 1 based*/
  const point pagesArrayElem; /* 2D coord of current page - 0,0 based */
  const pointf dpi;           /* device resolution device-units-per-inch */
  const int rotation;         /* viewport rotation (degrees)  0=portrait,
90=landscape */
  const box pageBoundingBox;  /* rotated boundingBox - device units */
  const unsigned int width;   /* device width - device units */
  const unsigned int height;  /* device height - device units */
  const pointf scale; /* composite device to graph units (zoom and dpi) */
  boxf canvasBox;
  double zoom;
  const boxf clip; /* clip region in graph units */

  // from gvc:
  const graph_t *const graph;
  char **defaultlinestyle;     /* default line style */
  char **const layerIDs;       /* array of layer names */
  char *const layerDelims;     /* delimiters in layer names */
  char *const layerListDelims; /* delimiters between layer ranges */
  const int numLayers;         /* number of layers */
} SafeJob;

typedef struct SafeLayer_s {
  const int layerNum; /* current layer - 1 based*/
  SafeJob *safe_job;
} SafeLayer;

// typedef struct SafeLayer_s {

// } SafeLayer;

SafeJob to_safe_job(GVJ_t *job);
SafeLayer to_safe_layer(SafeJob *safe_job, int layerNum);

#endif
