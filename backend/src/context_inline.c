// clang-format off
#include "const.h"
#include "gvc.h" // IWYU pragma: keep
#include "gvplugin.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcproc.h"
#include "strview.h"
#include <stdio.h> // IWYU pragma: keep
// clang-format on

extern gvplugin_library_t gvplugin_core_LTX_library;

static char *LibInfo[] = {
    "graphviz", /* Program */
    "a",        /* Version */
    "a"         /* Build Date */
};

void my_gvconfig_plugin_install_from_library(GVC_t *gvc) {

  gvplugin_installed_t *types;

  gvplugin_package_t *package = gv_alloc(sizeof(gvplugin_package_t));
  package->path = NULL;
  package->name = gv_strdup(gvplugin_core_LTX_library.packagename);
  package->next = gvc->packages;
  gvc->packages = package;
}
extern void textfont_dict_open(GVC_t *gvc);

GVC_t *gw_create_context(void) {
  agattr_text(NULL, AGNODE, "label", NODENAME_ESC);
  GVC_t *gvc = gv_alloc(sizeof(GVC_t));

  gvc->common.info = LibInfo;
  gvc->common.errorfn = agerrorf;
  gvc->common.demand_loading = 0;

  /* builtins don't require LTDL */
  my_gvconfig_plugin_install_from_library(gvc);
  gvc->config_found = false;
  gvtextlayout_select(
      gvc); /* choose best available textlayout plugin immediately */
  textfont_dict_open(gvc); /* initialize font dict */
  return gvc;
}
