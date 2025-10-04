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

extern gvplugin_installed_t gvdevice_dot_types[];
extern gvplugin_installed_t gvdevice_fig_types[];
extern gvplugin_installed_t gvdevice_map_types[];
extern gvplugin_installed_t gvdevice_ps_types[];
extern gvplugin_installed_t gvdevice_svg_types[];
extern gvplugin_installed_t gvdevice_json_types[];
extern gvplugin_installed_t gvdevice_tk_types[];
extern gvplugin_installed_t gvdevice_pic_types[];
extern gvplugin_installed_t gvdevice_pov_types[];

extern gvplugin_installed_t gvrender_dot_types[];
extern gvplugin_installed_t gvrender_fig_types[];
extern gvplugin_installed_t gvrender_map_types[];
extern gvplugin_installed_t gvrender_ps_types[];
extern gvplugin_installed_t gvrender_svg_types[];
extern gvplugin_installed_t gvrender_json_types[];
extern gvplugin_installed_t gvrender_tk_types[];
extern gvplugin_installed_t gvrender_pic_types[];
extern gvplugin_installed_t gvrender_pov_types[];

extern gvplugin_installed_t gvloadimage_core_types[];

static gvplugin_api_t apis[] = {
    {API_device, gvdevice_dot_types},
    {API_device, gvdevice_svg_types},

    {API_render, gvrender_dot_types},
    {API_render, gvrender_svg_types},

    {(api_t)0, 0},
};

/* install a plugin description into the list of available plugins
 * list is alpha sorted by type (not including :dependency), then
 * quality sorted within the type, then, if qualities are the same,
 * last install wins.
 */
bool my_gvplugin_install(GVC_t *gvc, api_t api, const char *typestr,
                         int quality, gvplugin_package_t *package,
                         gvplugin_installed_t *typeptr) {
  gvplugin_available_t *plugin, **pnext;
  char *t;

  /* duplicate typestr to later save in the plugin list */
  t = strdup(typestr);
  if (t == NULL)
    return false;

  // find the current plugin
  const strview_t type = strview(typestr, ':');

  /* point to the beginning of the linked list of plugins for this api */
  pnext = &gvc->apis[api];

  /* keep alpha-sorted and insert new duplicates ahead of old */
  while (*pnext) {

    // find the next plugin
    const strview_t next_type = strview((*pnext)->typestr, ':');

    if (strview_cmp(type, next_type) <= 0)
      break;
    pnext = &(*pnext)->next;
  }

  /* keep quality sorted within type and insert new duplicates ahead of old */
  while (*pnext) {

    // find the next plugin
    const strview_t next_type = strview((*pnext)->typestr, ':');

    if (!strview_eq(type, next_type))
      break;
    if (quality >= (*pnext)->quality)
      break;
    pnext = &(*pnext)->next;
  }

  plugin = gv_alloc(sizeof(gvplugin_available_t));
  plugin->next = *pnext;
  *pnext = plugin;
  plugin->typestr = t;
  plugin->quality = quality;
  plugin->package = package;
  plugin->typeptr = typeptr; /* null if not loaded */

  return true;
}

void my_gvconfig_plugin_install_from_library(GVC_t *gvc) {

  gvplugin_installed_t *types;

  gvplugin_package_t *package = gv_alloc(sizeof(gvplugin_package_t));
  package->path = NULL;
  package->name = gv_strdup(gvplugin_core_LTX_library.packagename);
  package->next = gvc->packages;
  gvc->packages = package;
  for (gvplugin_api_t *apis_ = apis; (types = apis_->types); apis_++) {
    for (int i = 0; types[i].type; i++) {
      my_gvplugin_install(gvc, apis_->api, types[i].type, types[i].quality,
                          package, &types[i]);
    }
  }
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
