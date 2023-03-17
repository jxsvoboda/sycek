/*
 * Copyright 2023 Jiri Svoboda
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
 * Code generator record definitions
 */

#ifndef CGREC_H
#define CGREC_H

#include <types/cgrec.h>
#include <types/cgtype.h>
#include <types/ir.h>

extern int cgen_records_create(cgen_records_t **);
extern void cgen_records_destroy(cgen_records_t *);
extern int cgen_record_create(cgen_records_t *, cgen_rec_type_t, const char *,
    ir_record_t *, cgen_record_t **);
extern cgen_record_t *cgen_records_find(cgen_records_t *, const char *);
extern cgen_record_t *cgen_records_first(cgen_records_t *);
extern cgen_record_t *cgen_records_next(cgen_record_t *);
extern void cgen_record_destroy(cgen_record_t *);
extern int cgen_record_append(cgen_record_t *, const char *,
    cgtype_t *);
extern cgen_rec_elem_t *cgen_record_elem_find(cgen_record_t *,
    const char *);

#endif
