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
#include <stddef.h>
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

static long Max_outputline = 0;
static Agsym_t *Tailport, *Headport;

typedef struct {
  size_t level;
  output_string output;
  size_t
      *node_last_written; // postorder number of subg when node was last written
  size_t
      *edge_last_written; // postorder number of subg when edge was last written
} write_info_t;

static size_t write_body(Agraph_t *g, write_info_t *wr_info,
                         size_t g_visit_number);
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
static void write_canonstr_str(write_info_t *wr_info, char *str) {
  if (EMPTY(str)) {
    out_puts(&wr_info->output, "\"\"");
    return;
  }

  static const char *tokenlist[] /* must agree with scan.l */
      = {"node", "edge", "strict", "graph", "digraph", "subgraph", NULL};
  char *src = str;
  char uc = *src;
  bool start_with_dot = uc == '.';
  bool start_with_minus = uc == '-';
  bool start_with_digit = gv_isdigit(uc);
  if (start_with_digit || start_with_dot || start_with_minus) {
    // maybe number?
    bool seen_dot = start_with_dot;
    bool seen_digit = start_with_digit;

    while (true) {
      uc = *(++src);
      if (uc == 0) {
        if (!seen_digit) {
          out_putc(&wr_info->output, '\"');
          out_puts(&wr_info->output, str);
          out_putc(&wr_info->output, '\"');
          return;
        } else {
          // arg is number
          out_puts(&wr_info->output, str);
          return;
        }
      } else if (uc == '.') {
        if (seen_dot) {
          break;
        }
        seen_dot = true;
      } else if (gv_isdigit(uc)) {
        seen_digit = true;
      } else {
        break;
      }
    }
  } else {
    // maybe id?
    while (gv_isalnum(uc) || uc == '_' || !isascii(uc)) {
      uc = *(++src);
      if (uc == 0) {
        /* Use quotes to protect tokens (example, a node named "node") */
        /* It would be great if it were easier to use flex here. */
        for (const char **tok = tokenlist; *tok; tok++) {
          if (!strcasecmp(*tok, str)) {
            out_putc(&wr_info->output, '\"');
            out_puts(&wr_info->output, str);
            out_putc(&wr_info->output, '\"');
            return;
          }
        }
        out_puts(&wr_info->output, str);
        return;
      }
    }
  }

  out_putc(&wr_info->output, '\"');
  out_put(&wr_info->output, str, src - str);
  char *checkpoint = src;
  char *linestart = str;
  while (uc != 0) {
    if (uc == '\"') {
      out_put(&wr_info->output, checkpoint, src - checkpoint);
      out_puts(&wr_info->output, "\\\"");
      uc = *(++src);
      checkpoint = src;
    } else if (is_escape(src)) {
      uc = *(src += 2);
    } else {
      uc = *(++src);
    }

    char prev = src[-1];
    if (Max_outputline != 0 && (src - linestart) >= Max_outputline &&
        !is_id_char(prev) && prev != '\\' && is_id_char(uc)) {
      /* If breaking long strings into multiple lines, only allow breaks after a
       * non-id char, not a backslash, where the next char is an id char.
       */
      out_put(&wr_info->output, checkpoint, src - checkpoint);
      out_puts(&wr_info->output, "\\\n");
      linestart = checkpoint = src;
    }
  }
  out_put(&wr_info->output, checkpoint, src - checkpoint);
  out_putc(&wr_info->output, '\"');
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
                       bool isRoot) {
  int cnt = 0;
  Dict_t *view;

  if (!isRoot)
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
  if (!isRoot)
    dtview(dict, view); /* restore previous view */
}

static void write_dicts(Agraph_t *g, write_info_t *wr_info) {
  bool isRoot = agparent(g) == NULL;

  Agdatadict_t *def = agdatadict(g, false);
  if (def) {
    write_dict(wr_info, "graph", def->dict.g, isRoot);
    write_dict(wr_info, "node", def->dict.n, isRoot);
    write_dict(wr_info, "edge", def->dict.e, isRoot);
  }
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

static size_t write_subgs(Agraph_t *g, write_info_t *wr_info,
                          size_t g_visit_number) {
  size_t subg_visit_number = g_visit_number;
  for (Agraph_t *subg = agfstsubg(g); subg; subg = agnxtsubg(subg)) {
    ++subg_visit_number;
    if (irrelevant_subgraph(subg)) {
      subg_visit_number = write_subgs(subg, wr_info, subg_visit_number);
    } else {
      indent(wr_info);
      char *name = agnameof(subg);
      if (name != NULL && name[0] != LOCALNAMEPREFIX) {
        // output "subgraph" only subgraphs with names
        out_puts(&wr_info->output, "subgraph ");
        write_canonstr_str(wr_info, name);
        out_puts(&wr_info->output, " ");
      }
      subg_visit_number = write_body(subg, wr_info, subg_visit_number);
    }
  }
  return subg_visit_number;
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

static void write_node(Agraph_t *subg, Agnode_t *n, write_info_t *wr_info,
                       Dict_t *d, size_t subg_visit_number) {
  size_t last_written = wr_info->node_last_written[AGSEQ(n)];
  /* test if node was already written in g or a subgraph of g */
  if (last_written >= subg_visit_number) {
    return;
  }

  /* node must be written if it wasn't already emitted because of
   * a subgraph or one of its predecessors, and if it is a singleton
   * or has non-default attributes.
   */
  if (!has_no_edges(subg, n) && !not_default_attrs(n)) {
    return;
  }

  indent(wr_info);
  write_nodename(n, wr_info);
  if (last_written == 0) {
    write_nondefault_attrs(n, wr_info, d);
  }
  out_puts(&wr_info->output, ";\n");
  wr_info->node_last_written[AGSEQ(n)] = subg_visit_number;
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

static void write_edge(Agedge_t *e, write_info_t *wr_info, Dict_t *d,
                       size_t subg_visit_number) {
  size_t last_written = wr_info->edge_last_written[AGSEQ(e)];
  if (last_written >= subg_visit_number) {
    return;
  }

  Agnode_t *t = AGTAIL(e);
  Agnode_t *h = AGHEAD(e);
  indent(wr_info);
  write_nodename(t, wr_info);
  write_port(e, wr_info, Tailport);
  out_puts(&wr_info->output, (agisdirected(agraphof(t)) ? " -> " : " -- "));
  write_nodename(h, wr_info);
  write_port(e, wr_info, Headport);
  if (last_written == 0) {
    write_nondefault_attrs(e, wr_info, d);
  } else {
    write_edge_name(e, wr_info, true);
  }
  out_puts(&wr_info->output, ";\n");
  wr_info->edge_last_written[AGSEQ(e)] = subg_visit_number;
}

static size_t write_body(Agraph_t *g, write_info_t *wr_info,
                         size_t g_visit_number) {
  out_puts(&wr_info->output, "{\n");
  wr_info->level++;
  write_dicts(g, wr_info);

  size_t next_visit_number = write_subgs(g, wr_info, g_visit_number);

  Agdatadict_t *dd = agdatadict(g, false);
  for (Agnode_t *n = agfstnode(g); n; n = agnxtnode(g, n)) {
    write_node(g, n, wr_info, dd ? dd->dict.n : 0, g_visit_number);

    Agnode_t *prev = n;
    for (Agedge_t *e = agfstout(g, n); e; e = agnxtout(g, e)) {
      if (prev != aghead(e)) {
        write_node(g, aghead(e), wr_info, dd ? dd->dict.n : 0, g_visit_number);
        prev = aghead(e);
      }
      write_edge(e, wr_info, dd ? dd->dict.e : 0, g_visit_number);
    }
  }

  wr_info->level--;
  indent(wr_info);
  out_puts(&wr_info->output, "}\n");
  return next_visit_number;
}

output_string my_agwrite(Agraph_t *g, unsigned int max_output_linelength) {
  Max_outputline = max_output_linelength;
  Tailport = agattr_text(g, AGEDGE, TAILPORT_ID, NULL);
  Headport = agattr_text(g, AGEDGE, HEADPORT_ID, NULL);

  write_info_t wr_info = before_write(g);

  indent(&wr_info);
  if (agisstrict(g)) {
    out_puts(&wr_info.output, "strict ");
  }
  if (g->desc.directed)
    out_puts(&wr_info.output, "digraph ");
  else
    out_puts(&wr_info.output, "graph ");

  char *name = agnameof(g);
  if (name != NULL && name[0] != LOCALNAMEPREFIX) {
    write_canonstr_str(&wr_info, name);
    out_puts(&wr_info.output, " ");
  }

  write_body(g, &wr_info, 1);
  after_write(wr_info);
  return wr_info.output;
}

static write_info_t before_write(Agraph_t *g) {
  write_info_t wr_info = {0};

  wr_info.level = 0;
  wr_info.node_last_written =
      gv_calloc(g->clos->seq[AGNODE] + 1, sizeof(size_t));
  wr_info.edge_last_written =
      gv_calloc(g->clos->seq[AGEDGE] + 1, sizeof(size_t));

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
  free(wr_info.node_last_written);
  free(wr_info.edge_last_written);
}
