/*
 * Lexer input from string
 */

#ifndef TYPES_STR_INPUT_H
#define TYPES_STR_INPUT_H

#include <stddef.h>
#include <types/lexer.h>

/** Lexer input from string */
typedef struct {
	/** String */
	const char *str;
	/** Current position in buffer */
	size_t pos;
	/** Current source position */
	src_pos_t cpos;
} str_input_t;

#endif
