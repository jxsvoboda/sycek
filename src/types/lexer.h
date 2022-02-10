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
 * Lexer (lexical analyzer)
 */

#ifndef TYPES_LEXER_H
#define TYPES_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <types/linput.h>
#include <types/src_pos.h>

enum {
	lexer_buf_size = 32,
	lexer_buf_low_watermark = 16
};

/** Token type */
typedef enum {
	ltt_space,
	ltt_tab,
	ltt_newline,
	ltt_elbspace,
	ltt_copen,
	ltt_ctext,
	ltt_ccont,
	ltt_cclose,
	ltt_dcopen,
	ltt_dctopen,
	ltt_dscomment,
	ltt_preproc,
	ltt_lparen,
	ltt_rparen,
	ltt_lbrace,
	ltt_rbrace,
	ltt_lbracket,
	ltt_rbracket,
	ltt_comma,
	ltt_colon,
	ltt_scolon,
	ltt_qmark,
	ltt_period,
	ltt_ellipsis,
	ltt_arrow,
	ltt_plus,
	ltt_minus,
	ltt_asterisk,
	ltt_slash,
	ltt_modulo,
	ltt_shl,
	ltt_shr,
	ltt_inc,
	ltt_dec,
	ltt_amper,
	ltt_bor,
	ltt_bxor,
	ltt_bnot,
	ltt_land,
	ltt_lor,
	ltt_lnot,
	ltt_less,
	ltt_greater,
	ltt_equal,
	ltt_lteq,
	ltt_gteq,
	ltt_notequal,
	ltt_assign,
	ltt_plus_assign,
	ltt_minus_assign,
	ltt_times_assign,
	ltt_divide_assign,
	ltt_modulo_assign,
	ltt_shl_assign,
	ltt_shr_assign,
	ltt_band_assign,
	ltt_bor_assign,
	ltt_bxor_assign,

	ltt_atomic,
	ltt_attribute,
	ltt_asm,
	ltt_auto,
	ltt_break,
	ltt_case,
	ltt_char,
	ltt_const,
	ltt_continue,
	ltt_default,
	ltt_do,
	ltt_double,
	ltt_else,
	ltt_enum,
	ltt_extern,
	ltt_float,
	ltt_for,
	ltt_goto,
	ltt_if,
	ltt_inline,
	ltt_int,
	ltt_int128,
	ltt_long,
	ltt_register,
	ltt_restrict,
	ltt_restrict_alt,
	ltt_return,
	ltt_short,
	ltt_signed,
	ltt_sizeof,
	ltt_static,
	ltt_struct,
	ltt_switch,
	ltt_typedef,
	ltt_union,
	ltt_unsigned,
	ltt_void,
	ltt_volatile,
	ltt_while,

	ltt_ident,
	ltt_number,
	ltt_charlit,
	ltt_strlit,

	ltt_invalid,
	ltt_invchar,
	ltt_eof,
	ltt_error
} lexer_toktype_t;

#define ltt_resword_first ltt_attribute
#define ltt_resword_last ltt_while

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

typedef enum {
	/** Normal state */
	ls_normal,
	/** Comment state */
	ls_comment
} lexer_state_t;

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
	/** State */
	lexer_state_t state;
} lexer_t;

#endif
