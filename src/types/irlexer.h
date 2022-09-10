/*
 * Copyright 2022 Jiri Svoboda
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * IR Lexer (lexical analyzer)
 */

#ifndef TYPES_IRLEXER_H
#define TYPES_IRLEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <types/linput.h>
#include <types/src_pos.h>

enum {
	ir_lexer_buf_size = 32,
	ir_lexer_buf_low_watermark = 16
};

/** IR token type */
typedef enum {
	itt_space,
	itt_tab,
	itt_newline,
	itt_comment,
	itt_lparen,
	itt_rparen,
	itt_lbrace,
	itt_rbrace,
	itt_comma,
	itt_colon,
	itt_scolon,
	itt_period,

	itt_add,
	itt_and,
	itt_attr,
	itt_begin,
	itt_bnot,
	itt_call,
	itt_end,
	itt_eq,
	itt_extern,
	itt_gt,
	itt_gtu,
	itt_gteq,
	itt_gteu,
	itt_imm,
	itt_int,
	itt_jmp,
	itt_jnz,
	itt_jz,
	itt_lt,
	itt_ltu,
	itt_lteq,
	itt_lteu,
	itt_lvar,
	itt_lvarptr,
	itt_mul,
	itt_neg,
	itt_neq,
	itt_nil,
	itt_nop,
	itt_or,
	itt_proc,
	itt_read,
	itt_ret,
	itt_retv,
	itt_shl,
	itt_shra,
	itt_shrl,
	itt_sub,
	itt_var,
	itt_varptr,
	itt_write,
	itt_xor,

	itt_ident,
	itt_number,

	itt_invalid,
	itt_invchar,
	itt_eof,
	itt_error
} ir_lexer_toktype_t;

#define itt_resword_first itt_add
#define itt_resword_last itt_write

/** IR lexer token */
typedef struct {
	/** Position of beginning of token */
	src_pos_t bpos;
	/** Position of end of token */
	src_pos_t epos;
	/** Token type */
	ir_lexer_toktype_t ttype;
	/** Token full text */
	char *text;
	/** Text size not including null terminator */
	size_t text_size;
	/** User data that can be piggybacked on the token */
	void *udata;
} ir_lexer_tok_t;

/** IR Lexer */
typedef struct ir_lexer {
	/** Input buffer */
	char buf[ir_lexer_buf_size];
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
} ir_lexer_t;

#endif
