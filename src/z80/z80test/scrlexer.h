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
 * Script lexer (lexical analyzer)
 */

#ifndef Z80TEST_SCRLEXER_H
#define Z80TEST_SCRLEXER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <types/z80/z80test/scrlexer.h>
#include <types/linput.h>

extern int scr_lexer_create(lexer_input_ops_t *, void *, scr_lexer_t **);
extern void scr_lexer_destroy(scr_lexer_t *);
extern int scr_lexer_get_tok(scr_lexer_t *, scr_lexer_tok_t *);
extern void scr_lexer_free_tok(scr_lexer_tok_t *);
extern int scr_lexer_dprint_char(char, FILE *);
extern int scr_lexer_dprint_tok(scr_lexer_tok_t *, FILE *);
extern int scr_lexer_dprint_tok_chr(scr_lexer_tok_t *, size_t, FILE *);
extern int scr_lexer_print_tok(scr_lexer_tok_t *, FILE *);
extern bool scr_lexer_tok_valid_chars(scr_lexer_tok_t *, size_t, size_t *);
extern const char *scr_lexer_str_ttype(scr_lexer_toktype_t);
extern int scr_lexer_print_ttype(scr_lexer_toktype_t, FILE *);
extern bool scr_lexer_is_comment(scr_lexer_toktype_t);
extern bool scr_lexer_is_wspace(scr_lexer_toktype_t);
extern bool scr_lexer_is_resword(scr_lexer_toktype_t);
extern int scr_lexer_number_val(scr_lexer_tok_t *, int64_t *);
extern int scr_lexer_string_text(scr_lexer_tok_t *, char **);

#endif
