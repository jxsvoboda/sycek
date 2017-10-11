#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <types/lexer.h>

extern int lexer_create(lexer_input_ops_t *, void *, lexer_t **);
extern void lexer_destroy(lexer_t *);
extern int lexer_get_tok(lexer_t *, lexer_tok_t *);
extern void lexer_free_tok(lexer_tok_t *);
extern int lexer_dump_tok(lexer_tok_t *, FILE *);

#endif
