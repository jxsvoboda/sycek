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
 * Code generator C types
 */

#ifndef CGTYPE_H
#define CGTYPE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <types/cgenum.h>
#include <types/cgrec.h>
#include <types/cgtype.h>

extern int cgtype_basic_create(cgtype_elmtype_t, cgtype_basic_t **);
extern int cgtype_func_create(cgtype_t *, cgtype_func_t **);
extern int cgtype_func_append_arg(cgtype_func_t *, cgtype_t *);
extern cgtype_func_arg_t *cgtype_func_first(cgtype_func_t *);
extern cgtype_func_arg_t *cgtype_func_next(cgtype_func_arg_t *);
extern cgtype_func_arg_t *cgtype_func_last(cgtype_func_t *);
extern cgtype_func_arg_t *cgtype_func_prev(cgtype_func_arg_t *);
extern int cgtype_pointer_create(cgtype_t *, cgtype_pointer_t **);
extern int cgtype_record_create(cgen_record_t *, cgtype_record_t **);
extern int cgtype_enum_create(cgen_enum_t *, cgtype_enum_t **);
extern int cgtype_array_create(cgtype_t *, cgtype_t *, bool, uint64_t,
    cgtype_array_t **);
extern int cgtype_clone(cgtype_t *, cgtype_t **);
extern int cgtype_compose(cgtype_t *, cgtype_t *, cgtype_t **);
extern void cgtype_destroy(cgtype_t *);
extern int cgtype_print(cgtype_t *, FILE *f);
extern bool cgtype_is_void(cgtype_t *);
extern cgtype_int_rank_t cgtype_int_rank(cgtype_t *);
extern int cgtype_int_construct(bool, cgtype_int_rank_t, cgtype_t **);
extern bool cgtype_ptr_compatible(cgtype_pointer_t *, cgtype_pointer_t *);
extern int cgtype_ptr_combine_qual(cgtype_pointer_t *, cgtype_pointer_t *,
    cgtype_t **);
extern bool cgtype_is_strict_enum(cgtype_t *);

#endif
