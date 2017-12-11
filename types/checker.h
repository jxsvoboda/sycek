/*
 * Checker
 */

#ifndef TYPES_CHECKER_H
#define TYPES_CHECKER_H

#include <adt/list.h>
#include <types/ast.h>
#include <types/lexer.h>

/** Checker token */
typedef struct {
	/** Containing checker module */
	struct checker_module *mod;
	/** Link in list of tokens */
	link_t ltoks;
	/** Lexer token */
	lexer_tok_t tok;
} checker_tok_t;

/** Checker module */
typedef struct checker_module {
	/** Tokens */
	list_t toks; /* of checker_tok_t */
	ast_module_t *ast;
} checker_module_t;

/** Checker */
typedef struct {
	/** Lexer */
	lexer_t *lexer;
	/** Module */
	checker_module_t *mod;
} checker_t;

/** Checker parser input */
typedef struct {
	checker_tok_t *tok;
} checker_parser_input_t;

#endif
