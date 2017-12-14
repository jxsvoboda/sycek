/*
 * Lexer input from file
 */

#ifndef TYPES_FILE_INPUT_H
#define TYPES_FILE_INPUT_H

#include <stdio.h>
#include <types/src_pos.h>

/** Lexer input from file */
typedef struct {
	/** Input file */
	FILE *f;
	/** Current source position */
	src_pos_t cpos;
} file_input_t;

#endif
