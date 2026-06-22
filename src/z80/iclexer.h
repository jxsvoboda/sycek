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
 * Z80 IC Lexer (lexical analyzer)
 */

#ifndef Z80_ICLEXER_H
#define Z80_ICLEXER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <types/linput.h>
#include <types/z80/iclexer.h>

extern int z80ic_lexer_create(lexer_input_ops_t *, void *, z80ic_lexer_t **);
extern void z80ic_lexer_destroy(z80ic_lexer_t *);
extern int z80ic_lexer_get_tok(z80ic_lexer_t *, z80ic_lexer_tok_t *);
extern void z80ic_lexer_free_tok(z80ic_lexer_tok_t *);
extern int z80ic_lexer_dprint_char(char, FILE *);
extern int z80ic_lexer_dprint_tok(z80ic_lexer_tok_t *, FILE *);
extern int z80ic_lexer_dprint_tok_chr(z80ic_lexer_tok_t *, size_t, FILE *);
extern int z80ic_lexer_print_tok(z80ic_lexer_tok_t *, FILE *);
extern bool z80ic_lexer_tok_valid_chars(z80ic_lexer_tok_t *, size_t, size_t *);
extern const char *z80ic_lexer_str_ttype(z80ic_lexer_toktype_t);
extern int z80ic_lexer_print_ttype(z80ic_lexer_toktype_t, FILE *);
extern bool z80ic_lexer_is_comment(z80ic_lexer_toktype_t);
extern bool z80ic_lexer_is_wspace(z80ic_lexer_toktype_t);
extern bool z80ic_lexer_is_resword(z80ic_lexer_toktype_t);
extern int z80ic_lexer_number_val(z80ic_lexer_tok_t *, int32_t *);

#endif
