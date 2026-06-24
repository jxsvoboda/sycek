/*
 * Copyright 2026 Jiri Svoboda
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
 * Z80 IC lexer (lexical analyzer)
 */

#ifndef TYPES_Z80_ICLEXER_H
#define TYPES_Z80_ICLEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <types/linput.h>
#include <types/src_pos.h>

enum {
	z80ic_lexer_buf_size = 32,
	z80ic_lexer_buf_low_watermark = 16
};

/** Z80 IC token type */
typedef enum {
	ztt_space,
	ztt_tab,
	ztt_newline,
	ztt_comment,
	ztt_lparen,
	ztt_rparen,
	ztt_comma,
	ztt_colon,
	ztt_scolon,
	ztt_period,
	ztt_plus,
	ztt_minus,

	ztt_A,
	ztt_AF,
	ztt_AF_,
	ztt_B,
	ztt_BC,
	ztt_C,
	ztt_D,
	ztt_DE,
	ztt_E,
	ztt_F,
	ztt_H,
	ztt_HL,
	ztt_I,
	ztt_IX,
	ztt_IY,
	ztt_L,
	ztt_M,
	ztt_NC,
	ztt_NZ,
	ztt_P,
	ztt_PE,
	ztt_PO,
	ztt_R,
	ztt_SP,
	ztt_Z,

	ztt_add,
	ztt_adc,
	ztt_and,
	ztt_begin,
	ztt_bit,
	ztt_call,
	ztt_ccf,
	ztt_cp,
	ztt_cpd,
	ztt_cpdr,
	ztt_cpi,
	ztt_cpir,
	ztt_cpl,
	ztt_daa,
	ztt_dec,
	ztt_defb,
	ztt_defw,
	ztt_defdw,
	ztt_defqw,
	ztt_di,
	ztt_djnz,
	ztt_ei,
	ztt_end,
	ztt_ex,
	ztt_exx,
	ztt_extern,
	ztt_global,
	ztt_halt,
	ztt_im,
	ztt_in,
	ztt_ind,
	ztt_indr,
	ztt_ini,
	ztt_inir,
	ztt_inc,
	ztt_jp,
	ztt_jr,
	ztt_ld,
	ztt_ldd,
	ztt_lddr,
	ztt_ldi,
	ztt_ldir,
	ztt_lvar,
	ztt_neg,
	ztt_nop,
	ztt_or,
	ztt_otdr,
	ztt_otir,
	ztt_out,
	ztt_outd,
	ztt_outi,
	ztt_pop,
	ztt_proc,
	ztt_push,
	ztt_res,
	ztt_ret,
	ztt_reti,
	ztt_retn,
	ztt_rl,
	ztt_rla,
	ztt_rlc,
	ztt_rlca,
	ztt_rrc,
	ztt_rld,
	ztt_rr,
	ztt_rra,
	ztt_rrca,
	ztt_rrd,
	ztt_rst,
	ztt_scf,
	ztt_sbc,
	ztt_set,
	ztt_sla,
	ztt_sra,
	ztt_srl,
	ztt_sub,
	ztt_var,
	ztt_xor,

	ztt_ident,
	ztt_number,

	ztt_invalid,
	ztt_invchar,
	ztt_eof,
	ztt_error
} z80ic_lexer_toktype_t;

#define ztt_resword_first ztt_add
#define ztt_resword_last ztt_xor

/** Z80 IC lexer token */
typedef struct {
	/** Position of beginning of token */
	src_pos_t bpos;
	/** Position of end of token */
	src_pos_t epos;
	/** Token type */
	z80ic_lexer_toktype_t ttype;
	/** Token full text */
	char *text;
	/** Text size not including null terminator */
	size_t text_size;
	/** User data that can be piggybacked on the token */
	void *udata;
} z80ic_lexer_tok_t;

/** Z80 IC Lexer */
typedef struct z80ic_lexer {
	/** Input buffer */
	char buf[z80ic_lexer_buf_size];
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
	/** I/O error hit in input */
	bool in_error;
	/** Input ops */
	lexer_input_ops_t *input_ops;
	/** Input argument */
	void *input_arg;
} z80ic_lexer_t;

#endif
