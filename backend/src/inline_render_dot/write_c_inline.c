/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

// clang-format off
// non-graphviz headers
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h> /* need sprintf() */
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../output_string.h"
#include "../gv_char_classes.h"

// graphviz headers
#include "cghdr.h"
#include "cgraph.h"
// clang-format on

#define EMPTY(s) (((s) == 0) || (s)[0] == '\0')
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static unsigned int Max_outputline = 0;
static Agsym_t *Tailport, *Headport;

typedef struct {
  size_t level;
  output_string output;
  uint64_t *preorder_number; // of a graph or subgraph
  uint64_t
      *node_last_written; // postorder number of subg when node was last written
  uint64_t
      *edge_last_written; // postorder number of subg when edge was last written
} write_info_t;

static void write_body(Agraph_t *g, write_info_t *wr_info);

static write_info_t before_write(Agraph_t *);
static void after_write(write_info_t);

static void indent(write_info_t *wr_info) {
  for (int i = wr_info->level; i > 0; i--) {
    out_puts(&wr_info->output, "\t");
  }
}

// alphanumeric, '.', '-', or non-ascii; basically, chars used in unquoted ids
static bool is_id_char(char c) {
  return gv_isalnum(c) || c == '.' || c == '-' || !isascii(c);
}

// is the prefix of this string a recognized Graphviz escape sequence?
// https://graphviz.org/docs/attr-types/escString/
static bool is_escape(const char *str) {
  assert(str != NULL);
  if (str[0] == '\\') {
    switch (str[1]) {
    case 'E':
    case 'G':
    case 'H':
    case 'L':
    case 'N':
    case 'T':
    case 'l':
    case 'n':
    case 'r':
    case '\\':
    case '"':
      return true;
    }
  }
  return false;
}

/* Canonicalize ordinary strings.
 * Assumes buf is large enough to hold output.
 */
static char *return_canonstr(char *arg, char *buf) {
  if (EMPTY(arg))
    return "\"\"";

  static const char *tokenlist[] /* must agree with scan.l */
      = {"node", "edge", "strict", "graph", "digraph", "subgraph", NULL};
  unsigned int cnt = 0, dotcnt = 0;
  bool needs_quotes = false;
  bool part_of_escape = false;
  bool backslash_pending = false;
  char *src = arg;
  char *dst = buf;
  *dst++ = '\"';
  char uc = *src++;
  bool maybe_num = gv_isdigit(uc) || uc == '.' || uc == '-';
  while (uc) {
    if (uc == '\"' && !part_of_escape) {
      *dst++ = '\\';
      needs_quotes = true;
    } else if (!part_of_escape && is_escape(&src[-1])) {
      needs_quotes = true;
      part_of_escape = true;
    } else if (maybe_num) {
      if (uc == '-') {
        if (cnt) {
          maybe_num = false;
          needs_quotes = true;
        }
      } else if (uc == '.') {
        if (dotcnt++) {
          maybe_num = false;
          needs_quotes = true;
        }
      } else if (!gv_isdigit(uc)) {
        maybe_num = false;
        needs_quotes = true;
      }
      part_of_escape = false;
    } else if (!(gv_isalnum(uc) || uc == '_' || !isascii(uc))) {
      needs_quotes = true;
      part_of_escape = false;
    } else {
      part_of_escape = false;
    }
    *dst++ = uc;
    uc = *src++;
    cnt++;

    /* If breaking long strings into multiple lines, only allow breaks after a
     * non-id char, not a backslash, where the next char is an id char.
     */
    if (Max_outputline) {
      if (uc && backslash_pending && !(is_id_char(dst[-1]) || dst[-1] == '\\') &&
          is_id_char(uc)) {
        *dst++ = '\\';
        *dst++ = '\n';
        needs_quotes = true;
        backslash_pending = false;
        cnt = 0;
      } else if (uc && (cnt >= Max_outputline)) {
        if (!(is_id_char(dst[-1]) || dst[-1] == '\\') && is_id_char(uc)) {
          *dst++ = '\\';
          *dst++ = '\n';
          needs_quotes = true;
          cnt = 0;
        } else {
          backslash_pending = true;
        }
      }
    }
  }
  *dst++ = '\"';
  *dst = '\0';
  if (needs_quotes || (cnt == 1 && (*arg == '.' || *arg == '-')))
    return buf;

  /* Use quotes to protect tokens (example, a node named "node") */
  /* It would be great if it were easier to use flex here. */
  for (const char **tok = tokenlist; *tok; tok++)
    if (!strcasecmp(*tok, arg))
      return buf;
  return arg;
}

static void write_canonstr_str(write_info_t *wr_info, char *str) {

  // maximum bytes required for canonicalized string
  const size_t required = 2 * strlen(str) + 2;

  // allocate space to stage the canonicalized string
  char *const scratch = malloc(required);
  if (scratch == NULL) {
    agerrorf("memory allocation failure\n");
    exit(1);
  }

  char *canonicalized = return_canonstr(str, scratch);
  out_puts(&wr_info->output, canonicalized);

  free(scratch);
}

static void write_canonstr_refstr(write_info_t *wr_info, char *str) {
  if (aghtmlstr(str)) {
    out_puts(&wr_info->output, "<");
    out_puts(&wr_info->output, str);
    out_puts(&wr_info->output, ">");
  } else {
    write_canonstr_str(wr_info, str);
  }
}

static void write_dict(write_info_t *wr_info, char *name, Dict_t *dict,
                       bool top) {
  int cnt = 0;
  Dict_t *view;

  if (!top)
    view = dtview(dict, NULL);
  else
    view = 0;
  for (Agsym_t *sym = dtfirst(dict); sym; sym = dtnext(dict, sym)) {
    if (EMPTY(sym->defval) &&
        !sym->print) { /* try to skip empty str (default) */
      if (view == NULL)
        continue; /* no parent */
      Agsym_t *psym = dtsearch(view, sym);
      assert(psym);
      if (EMPTY(psym->defval) && psym->print)
        continue; /* also empty in parent */
    }
    if (cnt++ == 0) {
      indent(wr_info);
      out_puts(&wr_info->output, name);
      out_puts(&wr_info->output, " [");
      wr_info->level++;
    } else {
      out_puts(&wr_info->output, ",\n");
      indent(wr_info);
    }
    write_canonstr_refstr(wr_info, sym->name);
    out_puts(&wr_info->output, "=");
    write_canonstr_refstr(wr_info, sym->defval);
  }
  if (cnt > 0) {
    wr_info->level--;
    if (cnt > 1) {
      out_puts(&wr_info->output, "\n");
      indent(wr_info);
    }
    out_puts(&wr_info->output, "];\n");
  }
  if (!top)
    dtview(dict, view); /* restore previous view */
}

static void write_dicts(Agraph_t *g, write_info_t *wr_info, bool top) {
  Agdatadict_t *def = agdatadict(g, false);
  if (def) {
    write_dict(wr_info, "graph", def->dict.g, top);
    write_dict(wr_info, "node", def->dict.n, top);
    write_dict(wr_info, "edge", def->dict.e, top);
  }
}

static void write_hdr(Agraph_t *g, write_info_t *wr_info, bool top) {
  bool root = false;
  char *strict = "";
  char *kind;
  if (!top && agparent(g))
    kind = "sub";
  else {
    root = true;
    if (g->desc.directed)
      kind = "di";
    else
      kind = "";
    if (agisstrict(g))
      strict = "strict ";
    Tailport = agattr_text(g, AGEDGE, TAILPORT_ID, NULL);
    Headport = agattr_text(g, AGEDGE, HEADPORT_ID, NULL);
  }

  char *name = agnameof(g);
  char *sep = " ";
  bool hasName = true;
  if (!name || name[0] == LOCALNAMEPREFIX) {
    sep = name = "";
    hasName = false;
  }
  indent(wr_info);
  out_puts(&wr_info->output, strict);

  /* output "<kind>graph" only for root graphs or graphs with names */
  if (root || hasName) {
    out_puts(&wr_info->output, kind);
    out_puts(&wr_info->output, "graph ");
  }
  if (hasName)
    write_canonstr_str(wr_info, name);
  out_puts(&wr_info->output, sep);
  out_puts(&wr_info->output, "{\n");
  wr_info->level++;
  write_dicts(g, wr_info, top);
  AGATTRWF(g) = true;
}

static void write_trl(write_info_t *wr_info) {
  wr_info->level--;
  indent(wr_info);
  out_puts(&wr_info->output, "}\n");
}

/// is this graph unnamed?
///
/// @param g Graph to inspect
/// @return True if this graph was given no explicit name
static bool is_anonymous(Agraph_t *g) {
  assert(g != NULL);

  // handle the common case inline for performance
  if (AGDISC(g, id) == &AgIdDisc) {
    // replicate `idprint`
    const IDTYPE id = AGID(g);
    if (id % 2 != 0) {
      return true;
    }
    return *(char *)(uintptr_t)id == LOCALNAMEPREFIX;
  }

  const char *const name = agnameof(g);
  return name == NULL || name[0] == LOCALNAMEPREFIX;
}

static bool irrelevant_subgraph(Agraph_t *g) {
  if (!is_anonymous(g))
    return false;

  Agattr_t *sdata = agattrrec(g);
  Agattr_t *pdata = agattrrec(agparent(g));
  if (sdata && pdata) {
    Agattr_t *rdata = agattrrec(agroot(g));
    int n = dtsize(rdata->dict);
    for (int i = 0; i < n; i++)
      if (sdata->str[i] && pdata->str[i] &&
          strcmp(sdata->str[i], pdata->str[i]))
        return false;
  }

  Agdatadict_t *dd = agdatadict(g, false);
  if (!dd)
    return true;
  if (dtsize(dd->dict.n) > 0 || dtsize(dd->dict.e) > 0)
    return false;
  return true;
}

static bool has_no_edges(Agraph_t *g, Agnode_t *n) {
  return agfstin(g, n) == NULL && agfstout(g, n) == NULL;
}

static bool not_default_attrs(Agnode_t *n) {
  Agattr_t *data = agattrrec(n);
  if (data) {
    for (Agsym_t *sym = dtfirst(data->dict); sym;
         sym = dtnext(data->dict, sym)) {
      if (data->str[sym->id] != sym->defval)
        return true;
    }
  }
  return false;
}

static void write_subgs(Agraph_t *g, write_info_t *wr_info) {
  for (Agraph_t *subg = agfstsubg(g); subg; subg = agnxtsubg(subg)) {
    if (irrelevant_subgraph(subg)) {
      write_subgs(subg, wr_info);
    } else {
      write_hdr(subg, wr_info, false);
      write_body(subg, wr_info);
      write_trl(wr_info);
    }
  }
}

static int write_edge_name(Agedge_t *e, write_info_t *wr_info, bool terminate) {
  char *p = agnameof(e);
  if (!EMPTY(p)) {
    if (!terminate) {
      wr_info->level++;
    }
    out_puts(&wr_info->output, "\t[key=");
    write_canonstr_str(wr_info, p);
    if (terminate)
      out_puts(&wr_info->output, "]");
    return 1;
  }
  return 0;
}

static void write_nondefault_attrs(void *obj, write_info_t *wr_info,
                                   Dict_t *defdict) {
  int cnt = 0;
  if (AGTYPE(obj) == AGINEDGE || AGTYPE(obj) == AGOUTEDGE) {
    int rv = write_edge_name(obj, wr_info, false);
    if (rv)
      cnt++;
  }

  Agattr_t *data = agattrrec(obj);
  if (data)
    for (Agsym_t *sym = dtfirst(defdict); sym; sym = dtnext(defdict, sym)) {
      if (AGTYPE(obj) == AGINEDGE || AGTYPE(obj) == AGOUTEDGE) {
        if (Tailport && sym->id == Tailport->id)
          continue;
        if (Headport && sym->id == Headport->id)
          continue;
      }
      if (data->str[sym->id] != sym->defval) {
        if (cnt++ == 0) {
          out_puts(&wr_info->output, "\t[");
          wr_info->level++;
        } else {
          out_puts(&wr_info->output, ",\n");
          indent(wr_info);
        }
        write_canonstr_refstr(wr_info, sym->name);
        out_puts(&wr_info->output, "=");
        write_canonstr_refstr(wr_info, data->str[sym->id]);
      }
    }
  if (cnt > 0) {
    out_puts(&wr_info->output, "]");
    wr_info->level--;
  }
  AGATTRWF(obj) = true;
}

static void write_nodename(Agnode_t *n, write_info_t *wr_info) {
  char *name = agnameof(n);
  if (name) {
    write_canonstr_str(wr_info, name);
  } else {
    char buf[sizeof("__SUSPECT") + 20];
    snprintf(buf, sizeof(buf), "_%" PRIu64 "_SUSPECT",
             AGID(n)); /* could be deadly wrong */
    out_puts(&wr_info->output, buf);
  }
}

static int attrs_written(void *obj) { return AGATTRWF(obj); }

static void write_node(Agraph_t *subg, Agnode_t *n, write_info_t *wr_info,
                       Dict_t *d) {
  indent(wr_info);
  write_nodename(n, wr_info);
  if (!attrs_written(n))
    write_nondefault_attrs(n, wr_info, d);
  wr_info->node_last_written[AGSEQ(n)] = wr_info->preorder_number[AGSEQ(subg)];
  out_puts(&wr_info->output, ";\n");
}

/* node must be written if it wasn't already emitted because of
 * a subgraph or one of its predecessors, and if it is a singleton
 * or has non-default attributes.
 */
static bool write_node_test(Agraph_t *g, Agnode_t *n, write_info_t *wr_info) {
  /* test if node was already written in g or a subgraph of g */
  if (wr_info->node_last_written[AGSEQ(n)] >=
      wr_info->preorder_number[AGSEQ(g)])
    return false;

  if (has_no_edges(g, n) || not_default_attrs(n))
    return true;
  return false;
}

static void write_port(Agedge_t *e, write_info_t *wr_info, Agsym_t *port) {
  if (!port)
    return;

  char *val = agxget(e, port);
  if (val[0] == '\0')
    return;

  out_puts(&wr_info->output, ":");
  if (aghtmlstr(val)) {
    write_canonstr_refstr(wr_info, val);
  } else {
    char *s = strchr(val, ':');
    if (s) {
      *s = '\0';
      write_canonstr_str(wr_info, val);
      out_puts(&wr_info->output, ":");
      write_canonstr_str(wr_info, s + 1);
      *s = ':';
    } else {
      write_canonstr_str(wr_info, val);
    }
  }
}

static bool write_edge_test(Agraph_t *g, Agedge_t *e, write_info_t *wr_info) {
  if (wr_info->edge_last_written[AGSEQ(e)] >=
      wr_info->preorder_number[AGSEQ(g)])
    return false;
  return true;
}

static void write_edge(Agraph_t *subg, Agedge_t *e, write_info_t *wr_info,
                       Dict_t *d) {
  Agnode_t *t = AGTAIL(e);
  Agnode_t *h = AGHEAD(e);
  indent(wr_info);
  write_nodename(t, wr_info);
  write_port(e, wr_info, Tailport);
  out_puts(&wr_info->output, (agisdirected(agraphof(t)) ? " -> " : " -- "));
  write_nodename(h, wr_info);
  write_port(e, wr_info, Headport);
  if (!attrs_written(e)) {
    write_nondefault_attrs(e, wr_info, d);
  } else {
    write_edge_name(e, wr_info, true);
  }
  wr_info->edge_last_written[AGSEQ(e)] = wr_info->preorder_number[AGSEQ(subg)];
  out_puts(&wr_info->output, ";\n");
}

static void write_body(Agraph_t *g, write_info_t *wr_info) {
  write_subgs(g, wr_info);
  Agdatadict_t *dd = agdatadict(g, false);
  for (Agnode_t *n = agfstnode(g); n; n = agnxtnode(g, n)) {
    if (write_node_test(g, n, wr_info))
      write_node(g, n, wr_info, dd ? dd->dict.n : 0);

    Agnode_t *prev = n;
    for (Agedge_t *e = agfstout(g, n); e; e = agnxtout(g, e)) {
      if (prev != aghead(e) && write_node_test(g, aghead(e), wr_info)) {
        write_node(g, aghead(e), wr_info, dd ? dd->dict.n : 0);
        prev = aghead(e);
      }
      if (write_edge_test(g, e, wr_info))
        write_edge(g, e, wr_info, dd ? dd->dict.e : 0);
    }
  }
}

static void set_attrwf(Agraph_t *g, bool toplevel, bool value) {
  AGATTRWF(g) = value;
  for (Agraph_t *subg = agfstsubg(g); subg; subg = agnxtsubg(subg)) {
    set_attrwf(subg, false, value);
  }
  if (toplevel) {
    for (Agnode_t *n = agfstnode(g); n; n = agnxtnode(g, n)) {
      AGATTRWF(n) = value;
      for (Agedge_t *e = agfstout(g, n); e; e = agnxtout(g, e))
        AGATTRWF(e) = value;
    }
  }
}

/// Return 0 on success, EOF on failure
output_string my_agwrite(Agraph_t *g, unsigned int max_output_linelength) {
  Max_outputline = max_output_linelength;
  write_info_t wr_info = before_write(g);
  write_hdr(g, &wr_info, true);
  write_body(g, &wr_info);
  write_trl(&wr_info);
  after_write(wr_info);

  return wr_info.output;
}

static uint64_t subgdfs(Agraph_t *g, uint64_t ix, write_info_t *wr_info) {
  uint64_t ix0 = ix;
  wr_info->preorder_number[AGSEQ(g)] = ix0;
  for (Agraph_t *subg = agfstsubg(g); subg; subg = agnxtsubg(subg)) {
    ix0 = subgdfs(subg, ix0, wr_info);
  }
  return ix0 + 1;
}

static write_info_t before_write(Agraph_t *g) {
  write_info_t wr_info = {0};
  set_attrwf(g, true, false);

  wr_info.level = 0;
  wr_info.preorder_number =
      gv_calloc(g->clos->seq[AGRAPH] + 1, sizeof(uint64_t));
  wr_info.node_last_written =
      gv_calloc(g->clos->seq[AGNODE] + 1, sizeof(uint64_t));
  wr_info.edge_last_written =
      gv_calloc(g->clos->seq[AGEDGE] + 1, sizeof(uint64_t));
  subgdfs(g, 1, &wr_info);

  /* page size on Linux, Mac OS X and Windows */
  const int OUTPUT_DATA_INITIAL_ALLOCATION = 4096;
  output_string output;
  if (!(output.data = malloc(OUTPUT_DATA_INITIAL_ALLOCATION))) {
    agerrorf("failure malloc'ing for result string");
    exit(-1);
  }
  output.data_allocated = OUTPUT_DATA_INITIAL_ALLOCATION;
  output.data_position = 0;
  wr_info.output = output;

  return wr_info;
}

static void after_write(write_info_t wr_info) {
  free(wr_info.preorder_number);
  free(wr_info.node_last_written);
  free(wr_info.edge_last_written);
}
