#include "safe_job.h"
#include "gvcjob.h" // IWYU pragma: keep
#include "gvcint.h" // IWYU pragma: keep

SafeLayer to_safe_layer(SafeJob *safe_job, int layerNum) {
  SafeLayer safe_layer = {.layerNum = layerNum, .safe_job = safe_job};
  return safe_layer;
}
