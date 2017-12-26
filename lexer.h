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
extern int lexer_dprint_tok(lexer_tok_t *, FILE *);
extern int lexer_print_tok(lexer_tok_t *, FILE *);
extern const char *lexer_str_ttype(lexer_toktype_t);
extern int lexer_print_ttype(lexer_toktype_t, FILE *);

#endif
