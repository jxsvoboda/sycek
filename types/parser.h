/*
 * Parser
 */

#ifndef TYPES_PARSER_H
#define TYPES_PARSER_H

#include <types/lexer.h>

enum {
	parser_lookahead = 2
};

/** Parser input ops */
typedef struct {
	/** Get next input token */
	void (*get_tok)(void *, lexer_tok_t *);
	/** Get data that should be stored into AST for a token */
	void *(*tok_data)(void *, lexer_tok_t *);
} parser_input_ops_t;

/** Parser */
typedef struct {
	/** Input ops */
	parser_input_ops_t *input_ops;
	/** Input argument */
	void *input_arg;
	/** Number of tokens in lookahead buffer @c tok */
	size_t tokcnt;
	/** Lookahead buffer */
	lexer_tok_t tok[parser_lookahead];
} parser_t;

#endif
