/*
 * Checker
 */

#ifndef TYPES_CHECKER_H
#define TYPES_CHECKER_H

#include <types/lexer.h>

/** Checker */
typedef struct {
	/** Lexer */
	lexer_t *lexer;
} checker_t;

#endif
