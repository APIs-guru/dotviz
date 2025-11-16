#include "safe_job.h"
#include "gvcjob.h" // IWYU pragma: keep
#include "gvcint.h" // IWYU pragma: keep

SafeJob to_safe_job(GVJ_t *job) {
  SafeJob safe_job = {
      .layerNum = job->layerNum,
      .pagesArrayElem = job->pagesArrayElem,
      .dpi = job->dpi,
      .rotation = job->rotation,
      .pageBoundingBox = job->pageBoundingBox,
      .height = job->height,
      .width = job->width,
      .scale = job->scale,
      .canvasBox = job->canvasBox,
      .zoom = job->zoom,
      .clip = job->clip,

      // from gvc
      .graph = job->gvc->g,
      .defaultlinestyle = job->gvc->defaultlinestyle,
      .layerIDs = job->gvc->layerIDs,
      .layerDelims = job->gvc->layerDelims,
      .layerListDelims = job->gvc->layerListDelims,
      .numLayers = job->gvc->numLayers,
  };

  return safe_job;
}

SafeLayer to_safe_layer(SafeJob *safe_job, int layerNum) {
  SafeLayer safe_layer = {.layerNum = layerNum, .safe_job = safe_job};
  return safe_layer;
}
