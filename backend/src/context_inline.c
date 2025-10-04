// clang-format off
#include "alloc.h"
#include "const.h"
#include "gvc.h"
#include "gvplugin.h"
#include "gvcint.h" // IWYU pragma: keep
#include "gvcproc.h"
// clang-format on

extern gvplugin_library_t gvplugin_core_LTX_library;
extern gvplugin_library_t gvplugin_dot_layout_LTX_library;

lt_symlist_t lt_preloaded_symbols[] = {
    {"gvplugin_core_LTX_library", &gvplugin_core_LTX_library},
    {0, 0},
};

static char *LibInfo[] = {
    "graphviz", /* Program */
    "a",        /* Version */
    "a"         /* Build Date */
};

extern void gvconfig_plugin_install_from_library(GVC_t *gvc, char *package_path,
                                                 gvplugin_library_t *library);

static void gvconfig_plugin_install_builtins(GVC_t *gvc) {
  const lt_symlist_t *s;
  const char *name;

  if (gvc->common.builtins == NULL)
    return;

  for (s = gvc->common.builtins; (name = s->name); s++)
    if (name[0] == 'g' && strstr(name, "_LTX_library"))
      gvconfig_plugin_install_from_library(gvc, NULL, s->address);
}

extern void textfont_dict_open(GVC_t *gvc);

GVC_t *gw_create_context(void) {
  agattr_text(NULL, AGNODE, "label", NODENAME_ESC);
  GVC_t *gvc = gv_alloc(sizeof(GVC_t));

  gvc->common.info = LibInfo;
  gvc->common.errorfn = agerrorf;
  gvc->common.builtins = lt_preloaded_symbols;
  gvc->common.demand_loading = 0;

  /* builtins don't require LTDL */
  gvconfig_plugin_install_builtins(gvc);
  gvc->config_found = false;
  gvtextlayout_select(
      gvc); /* choose best available textlayout plugin immediately */
  textfont_dict_open(gvc); /* initialize font dict */
  return gvc;
}
