#include "gvc.h"
#include "gvplugin.h"

extern gvplugin_library_t gvplugin_core_LTX_library;
extern gvplugin_library_t gvplugin_dot_layout_LTX_library;

lt_symlist_t lt_preloaded_symbols[] = {
    {"gvplugin_core_LTX_library", &gvplugin_core_LTX_library},
    {0, 0},
};

GVC_t *gw_create_context(void) {
  return gvContextPlugins(lt_preloaded_symbols, 0);
}
