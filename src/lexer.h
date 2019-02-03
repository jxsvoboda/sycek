/*
 * Copyright 2019 Jiri Svoboda
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
 * Lexer (lexical analyzer)
 */

#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stdio.h>
#include <types/lexer.h>

extern int lexer_create(lexer_input_ops_t *, void *, lexer_t **);
extern void lexer_destroy(lexer_t *);
extern int lexer_get_tok(lexer_t *, lexer_tok_t *);
extern void lexer_free_tok(lexer_tok_t *);
extern int lexer_dprint_char(char , FILE *);
extern int lexer_dprint_tok(lexer_tok_t *, FILE *);
extern int lexer_dprint_tok_chr(lexer_tok_t *, size_t, FILE *);
extern int lexer_print_tok(lexer_tok_t *, FILE *);
extern bool lexer_tok_valid_chars(lexer_tok_t *, size_t, size_t *);
extern const char *lexer_str_ttype(lexer_toktype_t);
extern int lexer_print_ttype(lexer_toktype_t, FILE *);
extern bool lexer_is_comment(lexer_toktype_t);
extern bool lexer_is_wspace(lexer_toktype_t);
extern bool lexer_is_resword(lexer_toktype_t);

#endif
