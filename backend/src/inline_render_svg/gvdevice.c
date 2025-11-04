/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

/*
 *  This library forms the socket for run-time loadable device plugins.
 */

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <assert.h>
#include <const.h>
#include "cgraph.h"
#include "gvio_svg.h"
#include <util/agxbuf.h>
#include <util/exit.h>
#include <util/startswith.h>

#include "../output_string.h"
#include "gv_ctype.h"
#include "unreachable.h"

/* return true if *s points to &[A-Za-z]+;      (e.g. &Ccedil; )
 *                          or &#[0-9]*;        (e.g. &#38; )
 *                          or &#x[0-9a-fA-F]*; (e.g. &#x6C34; )
 */
static bool xml_isentity(const char *s) {
  s++;             /* already known to be '&' */
  if (*s == ';') { // '&;' is not a valid entity
    return false;
  }
  if (*s == '#') {
    s++;
    if (*s == 'x' || *s == 'X') {
      s++;
      while (gv_isxdigit(*s))
        s++;
    } else {
      while (gv_isdigit(*s))
        s++;
    }
  } else {
    while (gv_isalpha(*s))
      s++;
  }
  if (*s == ';')
    return true;
  return false;
}

/** XML-escape a character
 *
 * \param previous The source character preceding the current one or '\0' if
 *   there was no prior character.
 * \param[in, out] current Pointer to the current position in a source string
 *   being escaped. The pointer is updated based on how many characters are
 *   consumed.
 * \param flags Options for configuring behavior.
 * \param cb User function for emitting escaped data. This is expected to take a
 *   caller-defined state type as the first parameter and the string to emit as
 *   the second, and then return an opaque value that is passed back to the
 *   caller.
 * \param state Data to pass as the first parameter when calling `cb`.
 * \return The return value of a call to `cb`.
 */
static void xml_core(char previous, const char **current, xml_flags_t flags,
                     output_string *output) {

  const char *s = *current;
  char c = *s;

  // we will consume at least one character, so note that now
  ++*current;

  // escape '&' only if not part of a legal entity sequence
  if (c == '&' && (flags.raw || !xml_isentity(s))) {
    out_puts(output, "&amp;");
    return;
  }

  // '<' '>' are safe to substitute even if string is already XML encoded since
  // XML strings won’t contain '<' or '>'
  if (c == '<') {
    out_puts(output, "&lt;");
    return;
  }

  if (c == '>') {
    out_puts(output, "&gt;");
    return;
  }

  // '-' cannot be used in XML comment strings
  if (c == '-' && flags.dash) {
    out_puts(output, "&#45;");
    return;
  }

  if (c == ' ' && previous == ' ' && flags.nbsp) {
    // substitute 2nd and subsequent spaces with required_spaces
    out_puts(output,
             "&#160;"); // Inkscape does not recognize &nbsp;
    return;
  }

  if (c == '"') {
    out_puts(output, "&quot;");
    return;
  }

  if (c == '\'') {
    out_puts(output, "&#39;");
    return;
  }

  if (c == '\n' && flags.raw) {
    out_puts(output, "&#10;");
    return;
  }

  if (c == '\r' && flags.raw) {
    out_puts(output, "&#13;");
    return;
  }

  unsigned char uc = (unsigned char)c;
  if (uc > 0x7f && flags.utf8) {

    // replicating a table from https://en.wikipedia.org/wiki/UTF-8:
    //
    //   ┌────────────────┬───────────────┬────────┬────────┬────────┬────────┐
    //   │First code point│Last code point│Byte 1  │Byte 2  │Byte 3
    //   │Byte 4  │
    //   ├────────────────┼───────────────┼────────┼────────┼────────┼────────┤
    //   │          U+0000│         U+007F│0xxxxxxx│        │        │ │
    //   │          U+0080│         U+07FF│110xxxxx│10xxxxxx│        │ │
    //   │          U+0800│         U+FFFF│1110xxxx│10xxxxxx│10xxxxxx│ │
    //   │         U+10000│
    //   U+10FFFF│11110xxx│10xxxxxx│10xxxxxx│10xxxxxx│
    //   └────────────────┴───────────────┴────────┴────────┴────────┴────────┘
    //
    // from which we can calculate the byte length of the current
    // character
    size_t length = (uc >> 5) == 6    ? 2
                    : (uc >> 4) == 14 ? 3
                    : (uc >> 3) == 30 ? 4
                                      : 0;

    // was the length malformed or is the follow on sequence truncated?
    bool is_invalid = length == 0;
    for (size_t l = 1; !is_invalid && length > l; ++l)
      is_invalid |= s[l] == '\0';

    // TODO: a better strategy than aborting on malformed data
    if (is_invalid) {
      fprintf(stderr, "Error during conversion to \"UTF-8\". Quiting.\n");
      graphviz_exit(EXIT_FAILURE);
    }

    // Decode the character. Refer again to the above table to
    // understand this algorithm.
    uint32_t utf8_char = 0;
    switch (length) {
    case 2: {
      uint32_t low = ((uint32_t)s[1]) & ((1 << 6) - 1);
      uint32_t high = ((uint32_t)s[0]) & ((1 << 5) - 1);
      utf8_char = low | (high << 6);
      break;
    }
    case 3: {
      uint32_t low = ((uint32_t)s[2]) & ((1 << 6) - 1);
      uint32_t mid = ((uint32_t)s[1]) & ((1 << 6) - 1);
      uint32_t high = ((uint32_t)s[0]) & ((1 << 4) - 1);
      utf8_char = low | (mid << 6) | (high << 12);
      break;
    }
    case 4: {
      uint32_t low = ((uint32_t)s[3]) & ((1 << 6) - 1);
      uint32_t mid1 = ((uint32_t)s[2]) & ((1 << 6) - 1);
      uint32_t mid2 = ((uint32_t)s[1]) & ((1 << 6) - 1);
      uint32_t high = ((uint32_t)s[0]) & ((1 << 3) - 1);
      utf8_char = low | (mid1 << 6) | (mid2 << 12) | (high << 18);
      break;
    }
    default:
      UNREACHABLE();
    }

    // setup a buffer that will fit the largest escape we need to print
    char buffer[sizeof("&#xFFFFFFFF;")];

    // emit the escape sequence itself
    snprintf(buffer, sizeof(buffer), "&#x%" PRIx32 ";", utf8_char);

    // note how many extra characters we consumed
    *current += length - 1;

    out_puts(output, buffer);
    return;
  }

  // otherwise, output the character as-is
  char buffer[2] = {c, '\0'};
  out_puts(output, buffer);
}

static void my_xml_escape(const char *s, xml_flags_t flags,
                          output_string *output) {
  char previous = '\0';
  while (*s != '\0') {
    char p = *s;
    xml_core(previous, &s, flags, output);
    previous = p;
  }
}

void gvputs_xml(output_string *output, const char *s) {
  const xml_flags_t flags = {.dash = 1, .nbsp = 1};
  my_xml_escape(s, flags, output);
}

void gvputs_xml_with_flags(output_string *output, const char *s,
                           xml_flags_t flags) {
  my_xml_escape(s, flags, output);
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

/* Test with:
 *	cc -DGVPRINTNUM_TEST gvprintnum.c -o gvprintnum
 */

/* gv_trim_zeros
 * Identify Trailing zeros and decimal point, if possible.
 * Assumes the input is the result of %.02f printing.
 */
static size_t gv_trim_zeros(const char *buf) {
  char *dotp = strchr(buf, '.');
  if (dotp == NULL) {
    return strlen(buf);
  }

  // check this really is the result of %.02f printing
  assert(isdigit((int)dotp[1]) && isdigit((int)dotp[2]) && dotp[3] == '\0');

  if (dotp[2] == '0') {
    if (dotp[1] == '0') {
      return (size_t)(dotp - buf);
    } else {
      return (size_t)(dotp - buf) + 2;
    }
  }

  return strlen(buf);
}

void gvprintdouble(output_string *output, double num) {
  // Prevents values like -0
  if (num > -0.005 && num < 0.005) {
    out_putc(output, '0');
    return;
  }
  char buf[50];

  snprintf(buf, 50, "%.02f", num);
  size_t len = gv_trim_zeros(buf);

  out_put(output, buf, len);
}
