#ifndef SAFE_JOB_H
#define SAFE_JOB_H

#include "types.h"
/// 1. ONLY CONST FIELDS!!!!!!!!!!!!!!
/// 2. Function can receive only GVJ_t or SafeJob, not both!!!!
typedef struct SafeJob_s {
  const graph_t *const graph;
  const int layerNum;         /* current layer - 1 based*/
  char **const layerIDs;      /* array of layer names */
  const point pagesArrayElem; /* 2D coord of current page - 0,0 based */
  char **defaultlinestyle;    /* default line style */
  const pointf dpi;           /* device resolution device-units-per-inch */
  const int rotation;         /* viewport rotation (degrees)  0=portrait,
90=landscape */
  const point pagesArraySize; /* 2D size of page array */
  const box pageBoundingBox;  /* rotated boundingBox - device units */
  const unsigned int width;   /* device width - device units */
  const unsigned int height;  /* device height - device units */
  const pointf scale;       /* composite device to graph units (zoom and dpi) */
  const pointf translation; /* composite translation */
} SafeJob;

SafeJob to_safe_job(GVJ_t *job);

#endif
