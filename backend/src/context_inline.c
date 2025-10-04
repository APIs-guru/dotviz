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

static void *textfont_makef(void *obj, Dtdisc_t *disc) {
  (void)disc;

  textfont_t *f1 = obj;
  textfont_t *f2 = gv_alloc(sizeof(textfont_t));

  /* key */
  if (f1->name)
    f2->name = gv_strdup(f1->name);
  if (f1->color)
    f2->color = gv_strdup(f1->color);
  f2->flags = f1->flags;
  f2->size = f1->size;

  /* non key */
  f2->postscript_alias = f1->postscript_alias;

  return f2;
}

static void textfont_freef(void *obj) {
  textfont_t *f = obj;

  free(f->name);
  free(f->color);
  free(f);
}

static int textfont_comparf(void *key1, void *key2) {
  int rc;
  textfont_t *f1 = key1, *f2 = key2;

  if (f1->name || f2->name) {
    if (!f1->name)
      return -1;
    if (!f2->name)
      return 1;
    rc = strcmp(f1->name, f2->name);
    if (rc)
      return rc;
  }
  if (f1->color || f2->color) {
    if (!f1->color)
      return -1;
    if (!f2->color)
      return 1;
    rc = strcmp(f1->color, f2->color);
    if (rc)
      return rc;
  }
  if (f1->flags < f2->flags)
    return -1;
  if (f1->flags > f2->flags)
    return 1;
  if (f1->size < f2->size)
    return -1;
  if (f1->size > f2->size)
    return 1;
  return 0;
}

GVC_t *gw_create_context(void) {
  agattr_text(NULL, AGNODE, "label", NODENAME_ESC);
  GVC_t *gvc = gv_alloc(sizeof(GVC_t));

  gvc->common.info = LibInfo;
  gvc->common.errorfn = agerrorf;
  gvc->common.demand_loading = 0;

  gvc->packages = NULL;
  gvc->config_found = false;

  (gvc->textfont_disc).key = 0;
  (gvc->textfont_disc).size = sizeof(textfont_t);
  (gvc->textfont_disc).link = -1;
  (gvc->textfont_disc).makef = textfont_makef;
  (gvc->textfont_disc).freef = textfont_freef;
  (gvc->textfont_disc).comparf = textfont_comparf;
  gvc->textfont_dt = dtopen(&(gvc->textfont_disc), Dtoset);
  return gvc;
}
