#include "render_inline.h"
#include "gvc.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvcproc.h"
#include <stdlib.h>

/* Render layout in a specified format to a malloc'ed string */
int gw_gvRenderData(GVC_t *gvc, Agrw_t graph, const char *format, char **result,
                    size_t *length) {
  Agraph_t *g = graph;
  int rc;
  GVJ_t *job;

  /* create a job for the required format */
  bool r = gvjobs_output_langname(gvc, format);
  job = gvc->job;
  if (!r) {
    agerrorf("Format: \"%s\" not recognized. Use one of:%s\n", format,
             gvplugin_list(gvc, API_device, format));
    return -1;
  }

  job->output_lang = gvrender_select(job, job->output_langname);
  if (!LAYOUT_DONE(g) && !(job->flags & LAYOUT_NOT_REQUIRED)) {
    agerrorf("Layout was not done\n");
    return -1;
  }

/* page size on Linux, Mac OS X and Windows */
#define OUTPUT_DATA_INITIAL_ALLOCATION 4096

  if (!result || !(*result = malloc(OUTPUT_DATA_INITIAL_ALLOCATION))) {
    agerrorf("failure malloc'ing for result string");
    return -1;
  }

  job->output_data = *result;
  job->output_data_allocated = OUTPUT_DATA_INITIAL_ALLOCATION;
  job->output_data_position = 0;

  rc = gvRenderJobs(gvc, g);
  gvrender_end_job(job);

  if (rc == 0) {
    *result = job->output_data;
    *length = job->output_data_position;
  }
  gvjobs_delete(gvc);

  return rc;
}

void gw_gvFreeRenderData(char *data) { gvFreeRenderData(data); }
