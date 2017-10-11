#ifndef TYPES_LEXER_H
#define TYPES_LEXER_H

#include <stddef.h>
#include <types/src_pos.h>

enum {
	lexer_lbuf_size = 128
};

/** Lexer token */
typedef struct {
	/** Position of beginning of token */
	src_pos_t bpos;
	/** Position of end of token */
	src_pos_t epos;
	int dummy;
} lexer_tok_t;

/** Lexer input ops */
typedef struct {
	int (*read)(void *, char *, size_t, size_t *);
} lexer_input_ops_t;

/** Lexer */
typedef struct {
	/** Line input buffer */
	char line_buf[lexer_lbuf_size];
	/** Number of used bytes in line_buf */
	size_t lb_used;
	/** Position of start of input buffer */
	src_pos_t lb_pos;
	/** Input ops */
	lexer_input_ops_t input_ops;
	/** Input argument */
	void *input_arg;
} lexer_t;

#endif
