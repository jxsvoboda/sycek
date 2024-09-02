/*
 * Copyright 2024 Jiri Svoboda
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
 * Script lexer (lexical analyzer)
 */

#ifndef TYPES_Z80TEST_SLEXER_H
#define TYPES_Z80TEST_SLEXER_H

#include <stdbool.h>
#include <stddef.h>
#include "types/linput.h"
#include "src_pos.h"

enum {
	scr_lexer_buf_size = 32,
	scr_lexer_buf_low_watermark = 16
};

/** Script token type */
typedef enum {
	stt_space,
	stt_tab,
	stt_newline,
	stt_comment,
	stt_lparen,
	stt_rparen,
	stt_lbrace,
	stt_rbrace,
	stt_comma,
	stt_colon,
	stt_scolon,
	stt_period,
	stt_plus,

	stt_A,
	stt_AF,
	stt_B,
	stt_BC,
	stt_C,
	stt_D,
	stt_DE,
	stt_E,
	stt_H,
	stt_HL,
	stt_L,
	stt_byte,
	stt_call,
	stt_dword,
	stt_ld,
	stt_ldbin,
	stt_mapfile,
	stt_pop,
	stt_print,
	stt_ptr,
	stt_push,
	stt_qword,
	stt_verify,
	stt_word,

	stt_ident,
	stt_number,
	stt_strlit,

	stt_invalid,
	stt_invchar,
	stt_eof,
	stt_error
} scr_lexer_toktype_t;

#define stt_resword_first stt_call
#define stt_resword_last stt_print

/** IR lexer token */
typedef struct {
	/** Position of beginning of token */
	src_pos_t bpos;
	/** Position of end of token */
	src_pos_t epos;
	/** Token type */
	scr_lexer_toktype_t ttype;
	/** Token full text */
	char *text;
	/** Text size not including null terminator */
	size_t text_size;
	/** User data that can be piggybacked on the token */
	void *udata;
} scr_lexer_tok_t;

/** Script Lexer */
typedef struct scr_lexer {
	/** Input buffer */
	char buf[scr_lexer_buf_size];
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
} scr_lexer_t;

#endif
