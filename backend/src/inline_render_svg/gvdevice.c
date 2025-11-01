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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <util/gv_fopen.h>
#include <util/prisize_t.h>
#include <util/xml.h>

#include <assert.h>
#include <const.h>
#include <gvplugin_device.h>
#include <gvcjob.h>
#include <gvcint.h>
#include <gvcproc.h>
#include <utils.h>
#include <gvio.h>
#include <util/agxbuf.h>
#include <util/exit.h>
#include <util/startswith.h>

#include "../output_string.h"

size_t gvwrite(GVJ_t *job, const char *s, size_t len) {
  output_string output;
  output.data_allocated = job->output_data_allocated;
  output.data_position = job->output_data_position;
  output.data = job->output_data;
  out_put(&output, s, len);
  job->output_data_allocated = output.data_allocated;
  job->output_data_position = output.data_position;
  job->output_data = output.data;
  return len;
}

int gvferror(FILE *stream) {
  GVJ_t *job = (GVJ_t *)stream;

  if (!job->gvc->write_fn && !job->output_data)
    return ferror(job->output_file);

  return 0;
}

int gvputs(GVJ_t *job, const char *s) {
  size_t len = strlen(s);

  if (gvwrite(job, s, len) != len) {
    return EOF;
  }
  return 1;
}

/// wrap `gvputs` to offer a `void *` first parameter
static int gvputs_wrapper(void *state, const char *s) {
  return gvputs(state, s);
}

int gvputs_xml(GVJ_t *job, const char *s) {
  const xml_flags_t flags = {.dash = 1, .nbsp = 1};
  return gv_xml_escape(s, flags, gvputs_wrapper, job);
}

void gvputs_nonascii(GVJ_t *job, const char *s) {
  for (; *s != '\0'; ++s) {
    if (*s == '\\') {
      gvputs(job, "\\\\");
    } else if (isascii((int)*s)) {
      gvputc(job, *s);
    } else {
      gvprintf(job, "%03o", (unsigned)*s);
    }
  }
}

int gvputc(GVJ_t *job, int c) {
  const char cc = (char)c;

  if (gvwrite(job, &cc, 1) != 1) {
    return EOF;
  }
  return c;
}

void gvprintf(GVJ_t *job, const char *format, ...) {
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

  gvwrite(job, agxbuse(&buf), (size_t)len);
  agxbfree(&buf);
}

/* Test with:
 *	cc -DGVPRINTNUM_TEST gvprintnum.c -o gvprintnum
 */

/* use macro so maxnegnum is stated just once for both double and string
 * versions */
#define val_str(n, x)                                                          \
  static double n = x;                                                         \
  static char n##str[] = #x;
val_str(maxnegnum, -999999999999999.99)

    static void gvprintnum(agxbuf *xb, double number) {
  /*
      number limited to a working range: maxnegnum >= n >= -maxnegnum
      suppressing trailing "0" and "."
   */

  if (number < maxnegnum) { /* -ve limit */
    agxbput(xb, maxnegnumstr);
    return;
  }
  if (number > -maxnegnum) {       /* +ve limit */
    agxbput(xb, maxnegnumstr + 1); // +1 to skip the '-' sign
    return;
  }

  agxbprint(xb, "%.03f", number);
  agxbuf_trim_zeros(xb);

  // strip off unnecessary leading '0'
  {
    char *staging = agxbdisown(xb);
    if (startswith(staging, "0.")) {
      memmove(staging, &staging[1], strlen(staging));
    } else if (startswith(staging, "-0.")) {
      memmove(&staging[1], &staging[2], strlen(&staging[1]));
    }
    agxbput(xb, staging);
    free(staging);
  }
}

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

void gvprintdouble(GVJ_t *job, double num) {
  // Prevents values like -0
  if (num > -0.005 && num < 0.005) {
    gvwrite(job, "0", 1);
    return;
  }

  char buf[50];

  snprintf(buf, 50, "%.02f", num);
  size_t len = gv_trim_zeros(buf);

  gvwrite(job, buf, len);
}

void gvprintpointf(GVJ_t *job, pointf p) {
  agxbuf xb = {0};

  gvprintnum(&xb, p.x);
  const char *buf = agxbuse(&xb);
  gvwrite(job, buf, strlen(buf));
  gvwrite(job, " ", 1);
  gvprintnum(&xb, p.y);
  buf = agxbuse(&xb);
  gvwrite(job, buf, strlen(buf));
  agxbfree(&xb);
}

void gvprintpointflist(GVJ_t *job, pointf *p, size_t n) {
  const char *separator = "";
  for (size_t i = 0; i < n; ++i) {
    gvputs(job, separator);
    gvprintpointf(job, p[i]);
    separator = " ";
  }
}
