/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * https://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#ifndef GVIO_SVG_
#define GVIO_SVG_ /* nothing */

#include <stddef.h>
#include "../output_string.h"
// `gvputs`, but XML-escape the input string
void gvputs_xml(output_string *output, const char *s);
/// options to tweak the behavior of XML escaping
typedef struct {
  /// assume no embedded escapes, and escape "\n" and "\r"
  unsigned raw : 1;
  /// escape '-'
  unsigned dash : 1;
  /// escape consecutive ' '
  unsigned nbsp : 1;
  /// anticipate non-ASCII characters that need to be encoded
  unsigned utf8 : 1;
} xml_flags_t;
void gvputs_xml_with_flags(output_string *output, const char *s,
                           xml_flags_t flags);

__attribute__((format(printf, 2, 3))) void gvprintf(output_string *output,
                                                    const char *format, ...);

void gvprintdouble(output_string *output, double num);
#endif
