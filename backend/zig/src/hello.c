#include <gvc.h>
#include <cgraph.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "hello.h"

extern gvplugin_library_t gvplugin_core_LTX_library;
extern gvplugin_library_t gvplugin_dot_layout_LTX_library;

lt_symlist_t lt_preloaded_symbols[] = {
  { "gvplugin_core_LTX_library", &gvplugin_core_LTX_library },
  { "gvplugin_dot_layout_LTX_library", &gvplugin_dot_layout_LTX_library },
  { 0, 0 }
};

GVC_t *viz_create_context(void) {
  return gvContextPlugins(lt_preloaded_symbols, 0);
}

int hello(GVC_t *gvc, const char *dot_string, char *outputBuf, 
  size_t bufSize, size_t *writtenLen) {
  *writtenLen = 0;

  Agraph_t *graph = agmemread(dot_string);
  if (!graph) {
    gvFreeContext(gvc);
    return -1;
  }

  if (gvLayout(gvc, graph, "dot") != 0) {
    agclose(graph);
    gvFreeContext(gvc);
    return -2;
  }

  char *svgData = NULL;
  size_t svgLen = 0;

  if (gvRenderData(gvc, graph, "svg", &svgData, &svgLen) != 0) {
    gvFreeLayout(gvc, graph);
    agclose(graph);
    gvFreeContext(gvc);
    return -3;
  }

  if (svgLen + 1 > bufSize) {
    gvFreeRenderData(svgData);
    gvFreeLayout(gvc, graph);
    agclose(graph);
    gvFreeContext(gvc);
    return -4;
  }

  memcpy(outputBuf, svgData, svgLen);
  outputBuf[svgLen] = '\0';
  *writtenLen = svgLen;

  gvFreeRenderData(svgData);
  gvFreeLayout(gvc, graph);
  agclose(graph);
  return 0;
}
