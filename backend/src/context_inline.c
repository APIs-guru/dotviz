// clang-format off
#include "alloc.h"
#include "const.h"
#include "gvc.h"
#include "gvplugin.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcproc.h"
#include <stdio.h>
// clang-format on

extern gvplugin_library_t gvplugin_core_LTX_library;

static char *LibInfo[] = {
    "graphviz", /* Program */
    "a",        /* Version */
    "a"         /* Build Date */
};

extern void gvconfig_plugin_install_from_library(GVC_t *gvc, char *package_path,
                                                 gvplugin_library_t *library);

extern void textfont_dict_open(GVC_t *gvc);

GVC_t *gw_create_context(void) {
  agattr_text(NULL, AGNODE, "label", NODENAME_ESC);
  GVC_t *gvc = gv_alloc(sizeof(GVC_t));

  gvc->common.info = LibInfo;
  gvc->common.errorfn = agerrorf;
  gvc->common.demand_loading = 0;

  /* builtins don't require LTDL */
  gvconfig_plugin_install_from_library(gvc, NULL, &gvplugin_core_LTX_library);
  gvc->config_found = false;
  gvtextlayout_select(
      gvc); /* choose best available textlayout plugin immediately */
  textfont_dict_open(gvc); /* initialize font dict */
  return gvc;
}
