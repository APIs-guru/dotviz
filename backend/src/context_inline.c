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

GVC_t *my_gvNEWcontext(const lt_symlist_t *builtins, int demand_loading) {
  GVC_t *gvc = gv_alloc(sizeof(GVC_t));

  gvc->common.info = LibInfo;
  gvc->common.errorfn = agerrorf;
  gvc->common.builtins = builtins;
  gvc->common.demand_loading = demand_loading;

  return gvc;
}

GVC_t *gw_create_context(void) {
  GVC_t *gvc;

  agattr_text(NULL, AGNODE, "label", NODENAME_ESC);
  gvc = my_gvNEWcontext(lt_preloaded_symbols, 0);
  gvconfig(gvc, false); /* configure for available plugins */
  return gvc;
}
