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
 * Compiler
 */

#ifndef COMP_H
#define COMP_H

#include <stdio.h>
#include <types/comp.h>
#include <types/lexer.h>

extern int comp_create(lexer_input_ops_t *, void *, comp_mtype_t, comp_t **);
extern int comp_make_ast(comp_t *);
extern int comp_make_ir(comp_t *);
extern int comp_make_vric(comp_t *);
extern int comp_dump_ast(comp_t *, FILE *);
extern int comp_dump_toks(comp_t *, FILE *);
extern int comp_dump_ir(comp_t *, FILE *);
extern int comp_dump_vric(comp_t *, FILE *);
extern int comp_dump_ic(comp_t *, FILE *);
extern void comp_destroy(comp_t *);
extern int comp_run(comp_t *, FILE *);

#endif
