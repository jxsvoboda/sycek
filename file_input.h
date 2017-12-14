/*
 * Lexer input from file
 */

#ifndef FILE_INPUT_H
#define FILE_INPUT_H

#include <stdio.h>
#include <types/lexer.h>
#include <types/file_input.h>

extern lexer_input_ops_t lexer_file_input;

extern void file_input_init(file_input_t *, FILE *, const char *);

#endif
