/*
 * Copyright 2026 Jiri Svoboda
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Code generator enum definitions
 */

#ifndef CGENUM_H
#define CGENUM_H

#include <types/cgenum.h>

extern int cgen_enums_create(cgen_enums_t **);
extern void cgen_enums_destroy(cgen_enums_t *);
extern int cgen_enum_create(cgen_enums_t *, const char *,
    cgen_enum_t **);
extern cgen_enum_t *cgen_enums_find(cgen_enums_t *, const char *);
extern cgen_enum_t *cgen_enums_first(cgen_enums_t *);
extern cgen_enum_t *cgen_enums_next(cgen_enum_t *);
extern void cgen_enum_destroy(cgen_enum_t *);
extern int cgen_enum_append(cgen_enum_t *, const char *, int,
    cgen_enum_elem_t **);
extern cgen_enum_elem_t *cgen_enum_first(cgen_enum_t *);
extern cgen_enum_elem_t *cgen_enum_next(cgen_enum_elem_t *);
extern cgen_enum_elem_t *cgen_enum_elem_find(cgen_enum_t *,
    const char *);
extern cgen_enum_elem_t *cgen_enum_val_find(cgen_enum_t *,
    int);
extern int cgen_enum_max_val(cgen_enum_t *);

#endif
