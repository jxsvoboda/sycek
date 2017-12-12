/*
 * Lexer (lexical analyzer)
 */

#ifndef TYPES_LEXER_H
#define TYPES_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <types/src_pos.h>

enum {
	lexer_buf_size = 32,
	lexer_buf_low_watermark = 16
};

/** Token type */
typedef enum {
	ltt_wspace,
	ltt_lparen,
	ltt_rparen,
	ltt_lbrace,
	ltt_rbrace,
	ltt_scolon,

	ltt_char,
	ltt_do,
	ltt_double,
	ltt_enum,
	ltt_extern,
	ltt_float,
	ltt_for,
	ltt_goto,
	ltt_if,
	ltt_inline,
	ltt_int,
	ltt_long,
	ltt_register,
	ltt_return,
	ltt_short,
	ltt_signed,
	ltt_sizeof,
	ltt_static,
	ltt_struct,
	ltt_typedef,
	ltt_union,
	ltt_unsigned,
	ltt_void,
	ltt_volatile,
	ltt_while,

	ltt_ident,
	ltt_number,

	ltt_invalid,
	ltt_eof,
	ltt_error
} lexer_toktype_t;

/** Lexer token */
typedef struct {
	/** Position of beginning of token */
	src_pos_t bpos;
	/** Position of end of token */
	src_pos_t epos;
	/** Token type */
	lexer_toktype_t ttype;
	/** Token full text */
	char *text;
	/** Text size not including null terminator */
	size_t text_size;
	/** User data that can be piggybacked on the token */
	void *udata;
} lexer_tok_t;

/** Lexer input ops */
typedef struct {
	int (*read)(void *, char *, size_t, size_t *, src_pos_t *);
} lexer_input_ops_t;

/** Lexer */
typedef struct {
	/** Input buffer */
	char buf[lexer_buf_size];
	/** Buffer position */
	size_t buf_pos;
	/** Number of used bytes in buf */
	size_t buf_used;
	/** Position of start of input buffer */
	src_pos_t buf_bpos;
	/** Current position */
	src_pos_t pos;
	/** EOF hit in input */
	bool in_eof;
	/** Input ops */
	lexer_input_ops_t *input_ops;
	/** Input argument */
	void *input_arg;
} lexer_t;

#endif
