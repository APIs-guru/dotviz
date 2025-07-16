#include <cgraph.h>
#include <gvc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hello.h"

extern gvplugin_library_t gvplugin_core_LTX_library;
extern gvplugin_library_t gvplugin_dot_layout_LTX_library;

lt_symlist_t lt_preloaded_symbols[] = {
    {"gvplugin_core_LTX_library", &gvplugin_core_LTX_library},
    {"gvplugin_dot_layout_LTX_library", &gvplugin_dot_layout_LTX_library},
    {0, 0}};

GVC_t *create_context(void) {
  return gvContextPlugins(lt_preloaded_symbols, 0);
}

uintptr_t read_graph_from_string(const char *dot_string) {
  Agraph_t *graph = agmemread(dot_string);
  if (!graph) {
    return 0;
  }
  return (uintptr_t)graph;
}

int render_graph_to_svg(GVC_t *gvc, uintptr_t graphptr, char *outputBuf,
                        size_t bufSize, size_t *writtenLen) {
  *writtenLen = 0;
  Agraph_t *graph = (Agraph_t *)graphptr;

  char *svgData = NULL;
  size_t svgLen = 0;

  if (gvLayout(gvc, graph, "dot") != 0) {
    return -2;
  }

  if (gvRenderData(gvc, graph, "svg", &svgData, &svgLen) != 0) {
    gvFreeLayout(gvc, graph);
    return -3;
  }

  if (svgLen + 1 > bufSize) {
    gvFreeRenderData(svgData);
    gvFreeLayout(gvc, graph);
    return -4;
  }

  memcpy(outputBuf, svgData, svgLen);
  outputBuf[svgLen] = '\0';
  *writtenLen = svgLen;

  gvFreeRenderData(svgData);
  gvFreeLayout(gvc, graph);
  return 0;
}
