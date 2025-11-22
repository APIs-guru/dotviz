#ifndef SAFE_JOB_H
#define SAFE_JOB_H

#include "const.h"
#include "types.h"

/// 1. ONLY CONST FIELDS!!!!!!!!!!!!!!
/// no job :)
typedef struct SafeJob_s {
  const int layerNum;        /* current layer - 1 based*/
  const pointf dpi;          /* device resolution device-units-per-inch */
  const int rotation;        /* viewport rotation (degrees)  0=portrait,
90=landscape */
  const box pageBoundingBox; /* rotated boundingBox - device units */
  const unsigned int width;  /* device width - device units */
  const unsigned int height; /* device height - device units */
  boxf canvasBox;
  double zoom;
  const boxf clip; /* clip region in graph units */

  // from gvc:
  const graph_t *const graph;
  char **const layerIDs;       /* array of layer names */
  char *const layerDelims;     /* delimiters in layer names */
  char *const layerListDelims; /* delimiters between layer ranges */
  const int numLayers;         /* number of layers */
} SafeJob;

typedef struct SafeLayer_s {
  const int layerNum; /* current layer - 1 based*/
  SafeJob *safe_job;
} SafeLayer;

#endif
