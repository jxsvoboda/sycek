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
 * Identifier scope
 */

#ifndef SCOPE_H
#define SCOPE_H

#include <types/cgenum.h>
#include <types/cgrec.h>
#include <types/cgtype.h>
#include <types/scope.h>
#include <types/symbols.h>

extern int scope_create(scope_t *, scope_t **);
extern void scope_destroy(scope_t *);
extern int scope_insert_gsym(scope_t *, lexer_tok_t *, cgtype_t *, symbol_t *);
extern int scope_insert_arg(scope_t *, lexer_tok_t *, cgtype_t *,
    const char *);
extern int scope_insert_lvar(scope_t *, lexer_tok_t *, cgtype_t *,
    const char *);
extern int scope_insert_tdef(scope_t *, lexer_tok_t *, cgtype_t *);
extern int scope_insert_record(scope_t *, lexer_tok_t *, scope_rec_type_t,
    cgen_record_t *, scope_member_t **);
extern int scope_insert_enum(scope_t *, lexer_tok_t *, cgen_enum_t *,
    scope_member_t **);
extern int scope_insert_eelem(scope_t *, lexer_tok_t *, cgen_enum_elem_t *,
    scope_member_t **);
extern scope_member_t *scope_first(scope_t *);
extern scope_member_t *scope_next(scope_member_t *);
extern scope_member_t *scope_lookup_local(scope_t *, const char *);
extern scope_member_t *scope_lookup_tag_local(scope_t *, const char *);
extern scope_member_t *scope_lookup(scope_t *, const char *);
extern scope_member_t *scope_lookup_tag(scope_t *, const char *);

#endif
