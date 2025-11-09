#include "safe_job.h"
#include "gvcjob.h" // IWYU pragma: keep
#include "gvcint.h" // IWYU pragma: keep

SafeJob to_safe_job(GVJ_t *job) {
  SafeJob safe_job = {
      .graph = job->gvc->g,
      .layerNum = job->layerNum,
      .layerIDs = job->gvc->layerIDs,
      .pagesArrayElem = job->pagesArrayElem,
      .defaultlinestyle = job->gvc->defaultlinestyle,
      .dpi = job->dpi,
      .rotation = job->rotation,
      .pagesArraySize = job->pagesArraySize,
      .pageBoundingBox = job->pageBoundingBox,
      .height = job->height,
      .width = job->width,
      .scale = job->scale,
      .translation = job->translation,
  };
  return safe_job;
}
