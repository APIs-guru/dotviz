#include "render_inline.h"
#include "alloc.h"
#include "gvc.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvcproc.h"
#include <stdlib.h>

static GVJ_t *output_langname_job;

/* -T switches */
bool my_gvjobs_output_langname(GVC_t *gvc, const char *name) {
  fprintf(stderr, "\npointer=[%p]\n", gvc->jobs);
  output_langname_job = gvc->job = gvc->jobs = gv_alloc(sizeof(GVJ_t));

  output_langname_job->output_langname = name;
  output_langname_job->gvc = gvc;

  /* load it now to check that it exists */
  if (gvplugin_load(gvc, API_device, name, NULL))
    return true;
  return false;
}

/* Render layout in a specified format to a malloc'ed string */
int gw_gvRenderData(GVC_t *gvc, Agrw_t graph, const char *format, char **result,
                    size_t *length) {
  Agraph_t *g = graph;
  int rc;
  GVJ_t *job;

  if (strncmp(format, "dot", 3) && strncmp(format, "gv", 2) &&
      strncmp(format, "svg", 3)) {
    agerrorf("Format: \"%s\" not recognized. Use one of: dot gv svg\n", format);
    return -1;
  }

  /* create a job for the required format */
  output_langname_job = gvc->job = gvc->jobs = gv_alloc(sizeof(GVJ_t));
  output_langname_job->output_langname = format;
  output_langname_job->gvc = gvc;

  gvplugin_load(gvc, API_device, format, NULL);

  job = gvc->job;

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

void gw_gvFreeRenderData(char *data) { free(data); }
