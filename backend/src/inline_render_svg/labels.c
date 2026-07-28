/// @file
/// @ingroup common_render
/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#include <types.h>
#include "agxbuf.h"
#include "const.h"
#include <utils.h>
#include <gvcint.h>
#include <htmltable.h>

#include "safe_job.h"
#include "core_svg.h"

extern pointf textspan_size(GVC_t *gvc, textspan_t *span);
static char *strdup_and_subst_obj0(char *str, void *obj, int escBackslash);

static void storeline(GVC_t *gvc, textlabel_t *lp, char *line,
                      char terminator) {
  pointf size;
  textspan_t *span;
  size_t oldsz = lp->u.txt.nspans + 1;

  lp->u.txt.span =
      gv_recalloc(lp->u.txt.span, oldsz, oldsz + 1, sizeof(textspan_t));
  span = &lp->u.txt.span[lp->u.txt.nspans];
  span->str = line;
  span->just = terminator;
  if (line && line[0]) {
    textfont_t tf = {0};
    tf.name = lp->fontname;
    tf.size = lp->fontsize;
    span->font = dtinsert(gvc->textfont_dt, &tf);
    size = textspan_size(gvc, span);
  } else {
    size.x = 0.0;
    span->size.y = size.y = (int)(lp->fontsize * LINESPACING);
  }

  lp->u.txt.nspans++;
  /* width = max line width */
  lp->dimen.x = MAX(lp->dimen.x, size.x);
  /* accumulate height */
  lp->dimen.y += size.y;
}

/* compiles <str> into a label <lp> */
void make_simple_label(GVC_t *gvc, textlabel_t *lp) {
  lp->dimen.x = lp->dimen.y = 0.0;
  if (*lp->text == '\0')
    return;

  agxbuf line = {0};
  for (char c, *p = lp->text; (c = *p++);) {
    if (c == '\\') {
      switch (*p) {
      case 'n':
      case 'l':
      case 'r':
        storeline(gvc, lp, agxbdisown(&line), *p);
        break;
      default:
        agxbputc(&line, *p);
      }
      if (*p)
        p++;
      /* tcldot can enter real linend characters */
    } else if (c == '\n') {
      storeline(gvc, lp, agxbdisown(&line), 'n');
    } else {
      agxbputc(&line, c);
    }
  }

  if (agxblen(&line) > 0) {
    storeline(gvc, lp, agxbdisown(&line), 'n');
  }

  agxbfree(&line);
  lp->space = lp->dimen;
}

/* make_label:
 * Assume str is freshly allocated for this instance, so it
 * can be freed in free_label.
 */
textlabel_t *make_label(void *obj, char *str, int kind, double fontsize,
                        char *fontname, char *fontcolor) {
  textlabel_t *rv = gv_alloc(sizeof(textlabel_t));
  graph_t *g = NULL, *sg = NULL;
  node_t *n = NULL;
  edge_t *e = NULL;

  switch (agobjkind(obj)) {
  case AGRAPH:
    sg = obj;
    g = sg->root;
    break;
  case AGNODE:
    n = obj;
    g = agroot(agraphof(n));
    break;
  case AGEDGE:
    e = obj;
    g = agroot(agraphof(aghead(e)));
    break;
  }
  rv->fontname = fontname;
  rv->fontcolor = fontcolor;
  rv->fontsize = fontsize;
  rv->charset = CHAR_UTF8;
  if (kind & LT_RECD) {
    rv->text = gv_strdup(str);
    if (kind & LT_HTML) {
      rv->html = true;
    }
  } else if (kind == LT_HTML) {
    rv->text = gv_strdup(str);
    rv->html = true;
    if (make_html_label(obj, rv)) {
      switch (agobjkind(obj)) {
      case AGRAPH:
        agerr(AGPREV, "in label of graph %s\n", agnameof(sg));
        break;
      case AGNODE:
        agerr(AGPREV, "in label of node %s\n", agnameof(n));
        break;
      case AGEDGE:
        agerr(AGPREV, "in label of edge %s %s %s\n", agnameof(agtail(e)),
              agisdirected(g) ? "->" : "--", agnameof(aghead(e)));
        break;
      }
    }
  } else {
    assert(kind == LT_NONE);
    /* This call just processes the graph object based escape sequences. The
     * formatting escape sequences (\n, \l, \r) are processed in
     * make_simple_label. That call also replaces \\ with \.
     */
    rv->text = strdup_and_subst_obj0(str, obj, 0);
    char *s = htmlEntityUTF8(rv->text, g);
    free(rv->text);
    rv->text = s;
    make_simple_label(GD_gvc(g), rv);
  }
  return rv;
}

/* free_textspan:
 * Free resources related to textspan_t.
 * tl is an array of cnt textspan_t's.
 * It is also assumed that the text stored in the str field
 * is all stored in one large buffer shared by all of the textspan_t,
 * so only the first one needs to free its tlp->str.
 */
void free_textspan(textspan_t *tl, size_t cnt) {
  textspan_t *tlp = tl;

  if (!tl)
    return;
  for (size_t i = 0; i < cnt; i++) {
    free(tlp->str);
    if (tlp->layout && tlp->free_layout)
      tlp->free_layout(tlp->layout);
    tlp++;
  }
  free(tl);
}

void free_label(textlabel_t *p) {
  if (p) {
    free(p->text);
    if (p->html) {
      if (p->u.html)
        free_html_label(p->u.html, 1);
    } else {
      free_textspan(p->u.txt.span, p->u.txt.nspans);
    }
    free(p);
  }
}

extern void svg_html_label(output_string *output, SafeLayer *safe_layer,
                           obj_state_t *parent, htmllabel_t *lp,
                           textlabel_t *tp);
void emit_label(output_string *output, SafeLayer *safe_layer, obj_state_t *obj,
                emit_state_t emit_state, textlabel_t *lp) {
  pointf p;
  emit_state_t old_emit_state;

  old_emit_state = obj->emit_state;
  obj->emit_state = emit_state;

  if (lp->html) {
    svg_html_label(output, safe_layer, obj, lp->u.html, lp);

    obj->emit_state = old_emit_state;
    return;
  }

  /* make sure that there is something to do */
  if (lp->u.txt.nspans < 1)
    return;

  obj->pencolor = svg_resolve_color(lp->fontcolor);

  /* position for first span */
  switch (lp->valign) {
  case 't':
    p.y = lp->pos.y + lp->space.y / 2.0 - lp->fontsize;
    break;
  case 'b':
    p.y = lp->pos.y - lp->space.y / 2.0 + lp->dimen.y - lp->fontsize;
    break;
  case 'c':
  default:
    p.y = lp->pos.y + lp->dimen.y / 2.0 - lp->fontsize;
    break;
  }
  if (obj->labeledgealigned)
    p.y -= lp->pos.y;
  for (size_t i = 0; i < lp->u.txt.nspans; i++) {
    switch (lp->u.txt.span[i].just) {
    case 'l':
      p.x = lp->pos.x - lp->space.x / 2.0;
      break;
    case 'r':
      p.x = lp->pos.x + lp->space.x / 2.0;
      break;
    default:
    case 'n':
      p.x = lp->pos.x;
      break;
    }
    svg_textspan(output, GD_fontnames(safe_layer->safe_job->graph), obj, p,
                 &lp->u.txt.span[i]);

    /* UL position for next span */
    p.y -= lp->u.txt.span[i].size.y;
  }

  obj->emit_state = old_emit_state;
}

/* strdup_and_subst_obj0:
 * Replace various escape sequences with the name of the associated
 * graph object. A double backslash \\ can be used to avoid a replacement.
 * If escBackslash is true, convert \\ to \; else leave alone. All other dyads
 * of the form \. are passed through unchanged.
 */
static char *strdup_and_subst_obj0(char *str, void *obj, int escBackslash) {
  textlabel_t *tl = NULL;
  graph_t *graph = NULL;
  node_t *node = NULL;
  edge_t *edge = NULL;
  /* prepare substitution strings */
  switch (agobjkind(obj)) {
  case AGRAPH:
    tl = GD_label(obj);
    graph = (graph_t *)obj;
    break;
  case AGNODE:
    tl = ND_label(obj);
    graph = agraphof(obj);
    node = (node_t *)obj;
    break;
  case AGEDGE:
    graph = agraphof(obj);
    edge = (edge_t *)obj;
    tl = ED_label(obj);
    break;
  }

  /* allocate a dynamic buffer that we will use to construct the result */
  agxbuf buf = {0};

  /* assemble new string */
  bool seenSlash = false;
  for (char *s = str; *s != '\0'; ++s) {
    char c = *s;
    if (!seenSlash) {
      if (c == '\\') {
        seenSlash = true;
      } else {
        agxbputc(&buf, c);
      }
      continue;
    }

    seenSlash = false;
    switch (c) {
    case 'G':
      agxbput(&buf, graph != NULL ? agnameof(graph) : "\\G");
      break;
    case 'N':
      agxbput(&buf, node != NULL ? agnameof(node) : "\\N");
      break;
    case 'E':
      if (edge != NULL) {
        agxbput(&buf, agnameof(agtail(edge)));
        port pt = ED_tail_port(obj);
        if (*pt.name != '\0') {
          agxbprint(&buf, ":%s", pt.name);
        }
        if (agisdirected(graph))
          agxbput(&buf, "->");
        else
          agxbput(&buf, "--");
        agxbprint(&buf, "%s", agnameof(aghead(edge)));
        pt = ED_head_port(obj);
        if (*pt.name != '\0') {
          agxbprint(&buf, ":%s", pt.name);
        }
        continue;
      } else {
        agxbput(&buf, "\\E");
      }
      break;
    case 'T':
      agxbput(&buf, edge != NULL ? agnameof(aghead(edge)) : "\\T");
      break;
    case 'H':
      agxbput(&buf, edge != NULL ? agnameof(agtail(edge)) : "\\H");
      break;
    case 'L':
      agxbput(&buf, tl != NULL ? tl->text : "\\L");
      break;
    case '\\':
      agxbput(&buf, escBackslash ? "\\" : "\\\\");
      break;
    default:
      /* leave other escape sequences unmodified, e.g. \n \l \r */
      agxbprint(&buf, "\\%c", c);
    }
  }
  if (seenSlash) {
    agxbputc(&buf, '\\'); // handle trailing slash
  }

  /* extract the final string with replacements applied */
  return agxbdisown(&buf);
}

/* strdup_and_subst_obj:
 * Processes graph object escape sequences; also collapses \\ to \.
 */
char *strdup_and_subst_obj(char *str, void *obj) {
  return strdup_and_subst_obj0(str, obj, 1);
}
