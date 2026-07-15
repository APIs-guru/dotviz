/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#include <errno.h>

#include "cgraph.h"
#include "gvio_svg.h"
#include "util/agxbuf.h"

#include "../output_string.h"

/* return true if *s points to &[A-Za-z]+;      (e.g. &Ccedil; )
 *                          or &#[0-9]*;        (e.g. &#38; )
 *                          or &#x[0-9a-fA-F]*; (e.g. &#x6C34; )
 */
static bool xml_isentity(const char *s) {
  s++; /* already known to be '&' */
  if (*s == '#') {
    s++;
    if (*s == 'x' || *s == 'X') {
      s++;
      while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') ||
             (*s >= 'A' && *s <= 'F'))
        s++;
    } else {
      while (*s >= '0' && *s <= '9')
        s++;
    }
    return *s == ';';
  }

  if (*s == ';')
    return false; // '&;' is not a valid entity
  while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))
    s++;
  return *s == ';';
}

void gvputs_xml(output_string *output, const char *s) {
  const xml_flags_t flags = {.dash = 1, .nbsp = 1};
  gvputs_xml_with_flags(output, s, flags);
}

void gvputs_xml_with_flags(output_string *output, const char *s,
                           xml_flags_t flags) {
  char previous = '\0';
  while (*s != '\0') {
    char c = *s;
    if (c == '&' && (flags.raw || !xml_isentity(s))) {
      // escape '&' only if not part of a legal entity sequence
      out_puts(output, "&amp;");
    } else if (c == '<') {
      // '<' '>' are safe to substitute even if string is already XML encoded
      // since XML strings won’t contain '<' or '>'
      out_puts(output, "&lt;");
    } else if (c == '>') {
      out_puts(output, "&gt;");
    } else if (c == '-' && flags.dash) {
      // '-' cannot be used in XML comment strings
      out_puts(output, "&#45;");
    } else if (c == ' ' && previous == ' ' && flags.nbsp) {
      // substitute 2nd and subsequent spaces with required_spaces
      out_puts(output, "&#160;"); // Inkscape does not recognize &nbsp;
    } else if (c == '"') {
      out_puts(output, "&quot;");
    } else if (c == '\'') {
      out_puts(output, "&#39;");
    } else if (c == '\n' && flags.raw) {
      out_puts(output, "&#10;");
    } else if (c == '\r' && flags.raw) {
      out_puts(output, "&#13;");
    } else {
      out_putc(output, c); // otherwise, output the character as-is
    }

    previous = c;
    ++s;
  }
}

void gvprintf(output_string *output, const char *format, ...) {
  agxbuf buf = {0};
  va_list argp;

  va_start(argp, format);
  int len = vagxbprint(&buf, format, argp);
  if (len < 0) {
    va_end(argp);
    agerrorf("gvprintf: %s\n", strerror(errno));
    return;
  }
  va_end(argp);

  out_put(output, agxbuse(&buf), (size_t)len);

  agxbfree(&buf);
}

void gvprintdouble(output_string *output, double num) {
  // Prevents values like -0
  if (num > -0.005 && num < 0.005) {
    out_putc(output, '0');
    return;
  }

  char buf[50];
  size_t len = snprintf(buf, 50, "%.02f", num);
  if (buf[len - 1] == '0') {
    len -= 1; // skip '0'
  }
  if (buf[len - 1] == '0') {
    len -= 2; // skip both '.' and '0'
  }
  out_put(output, buf, len);
}
