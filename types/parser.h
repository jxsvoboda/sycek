#ifndef TYPES_PARSER_H
#define TYPES_PARSER_H

#include <types/lexer.h>

/** Parser input ops */
typedef struct {
	int (*get_tok)(void *, lexer_tok_t *);
} parser_input_ops_t;

/** Parser */
typedef struct {
	/** Input ops */
	parser_input_ops_t *input_ops;
	/** Input argument */
	void *input_arg;
} parser_t;

#endif
