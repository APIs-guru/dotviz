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

#include "gvcext.h"
#include <stddef.h>
int gvputc(GVJ_t *job, int c);
int gvputs(GVJ_t *job, const char *s);

// `gvputs`, but XML-escape the input string
int gvputs_xml(GVJ_t *job, const char *s);

__attribute__((format(printf, 2, 3))) void gvprintf(GVJ_t *job,
                                                    const char *format, ...);

void gvprintdouble(GVJ_t *job, double num);
#endif
