/*
 * Copyright 2021 Jiri Svoboda
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
 * IR Lexer (lexical analyzer)
 */

#ifndef IRLEXER_H
#define IRLEXER_H

#include <stdbool.h>
#include <stdio.h>
#include <types/irlexer.h>
#include <types/linput.h>

extern int ir_lexer_create(lexer_input_ops_t *, void *, ir_lexer_t **);
extern void ir_lexer_destroy(ir_lexer_t *);
extern int ir_lexer_get_tok(ir_lexer_t *, ir_lexer_tok_t *);
extern void ir_lexer_free_tok(ir_lexer_tok_t *);
extern int ir_lexer_dprint_char(char, FILE *);
extern int ir_lexer_dprint_tok(ir_lexer_tok_t *, FILE *);
extern int ir_lexer_dprint_tok_chr(ir_lexer_tok_t *, size_t, FILE *);
extern int ir_lexer_print_tok(ir_lexer_tok_t *, FILE *);
extern bool ir_lexer_tok_valid_chars(ir_lexer_tok_t *, size_t, size_t *);
extern const char *ir_lexer_str_ttype(ir_lexer_toktype_t);
extern int ir_lexer_print_ttype(ir_lexer_toktype_t, FILE *);
extern bool ir_lexer_is_comment(ir_lexer_toktype_t);
extern bool ir_lexer_is_wspace(ir_lexer_toktype_t);
extern bool ir_lexer_is_resword(ir_lexer_toktype_t);

#endif
