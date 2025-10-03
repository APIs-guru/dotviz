#include "render_inline.h"
#include "const.h"
#include "gvc.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcjob.h"
#include "gvcproc.h"
#include "strview.h" // IWYU pragma: keep
#include <stdlib.h>

int my_gvrender_select(GVJ_t *job) {
  GVC_t *gvc = job->gvc;
  gvplugin_installed_t *typeptr;

  /* When job is created, it is zeroed out.
   * Some flags, such as OUTPUT_NOT_REQUIRED, may already be set,
   * so don't reset.
   */
  /* job->flags = 0; */
  typeptr = (gvc->api[API_device])->typeptr;
  job->device.engine = typeptr->engine;
  job->device.features = typeptr->features;
  job->device.id = typeptr->id;
  job->device.type = (gvc->api[API_device])->typestr;

  job->flags |= job->device.features->flags;

  /* The device plugin has a dependency on a render plugin,
   * so the render plugin should be available as well now */
  gvplugin_available_t *plugin = gvc->api[API_render];
  if (plugin) {
    typeptr = plugin->typeptr;
    job->render.engine = typeptr->engine;
    job->render.features = typeptr->features;
    job->render.type = plugin->typestr;

    job->flags |= job->render.features->flags;

    if (job->device.engine)
      job->render.id = typeptr->id;
    else
      /* A null device engine indicates that the device id is also the renderer
       * id and that the renderer doesn't need "device" functions. Device
       * "features" settings are still available */
      job->render.id = job->device.id;
    return GVRENDER_PLUGIN;
  }
  job->render.engine = NULL;
  return NO_SUPPORT; /* FIXME - should differentiate problem */
}

/* Render layout in a specified format to a malloc'ed string */
int gw_gvRenderData(GVC_t *gvc, Agrw_t graph, const char *format, char **result,
                    size_t *length) {
  Agraph_t *g = graph;
  int rc;

  if (strncmp(format, "dot", 3) && strncmp(format, "gv", 2) &&
      strncmp(format, "svg", 3)) {
    agerrorf("Format: \"%s\" not recognized. Use one of: dot gv svg\n", format);
    return -1;
  }

  /* create a job for the required format */
  GVJ_t *job = gvc->job = gvc->jobs = gv_alloc(sizeof(GVJ_t));
  job->output_langname = format;
  job->gvc = gvc;
  job = gvc->job;

  job->output_lang = my_gvrender_select(job);
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
