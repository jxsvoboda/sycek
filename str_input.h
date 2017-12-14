/*
 * Lexer input from string
 */

#ifndef STR_INPUT_H
#define STR_INPUT_H

#include <types/lexer.h>
#include <types/str_input.h>

extern lexer_input_ops_t lexer_str_input;

extern void str_input_init(str_input_t *, const char *);

#endif
