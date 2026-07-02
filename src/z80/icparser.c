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
 * Z80 IC parser
 */

#include <assert.h>
#include <inttypes.h>
#include <merrno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <z80/z80ic.h>
#include <z80/iclexer.h>
#include <z80/icparser.h>

/** Create Z80 IC parser.
 *
 * @param ops Parser input ops
 * @param arg Argument to input ops
 * @param rparser Place to store pointer to new parser
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_parser_create(z80ic_parser_input_ops_t *ops, void *arg,
    z80ic_parser_t **rparser)
{
	z80ic_parser_t *parser;

	parser = calloc(1, sizeof(z80ic_parser_t));
	if (parser == NULL)
		return ENOMEM;

	parser->input_ops = ops;
	parser->input_arg = arg;

	*rparser = parser;
	return EOK;
}

/** Destroy Z80 IC parser.
 *
 * @param parser Z80 IC parser
 */
void z80ic_parser_destroy(z80ic_parser_t *parser)
{
	if (parser != NULL)
		free(parser);
}

/** Return @c true if token type is to be ignored when parsing.
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is ignored during parsing
 */
bool z80ic_parser_ttype_ignore(z80ic_lexer_toktype_t ttype)
{
	return ttype == ztt_space || ttype == ztt_tab ||
	    ttype == ztt_newline || ttype == ztt_comment ||
	    ttype == ztt_invchar;
}

/** Return valid input token skipping tokens that should be ignored.
 *
 * At the same time we read the token contents into the provided buffer @a rtok
 *
 * @param parser Z80 IC parser
 * @param rtok Place to store next lexer token
 */
static void z80ic_parser_next_input_tok(z80ic_parser_t *parser,
    z80ic_lexer_tok_t *rtok)
{
	parser->input_ops->read_tok(parser->input_arg, rtok);
	while (z80ic_parser_ttype_ignore(rtok->ttype)) {
		z80ic_lexer_free_tok(rtok);
		parser->input_ops->next_tok(parser->input_arg);
		parser->input_ops->read_tok(parser->input_arg, rtok);
	}
}

/** Return type of next token.
 *
 * @param parser Z80 IC parser
 * @return Type of next token being parsed
 */
static z80ic_lexer_toktype_t z80ic_parser_next_ttype(z80ic_parser_t *parser)
{
	z80ic_lexer_tok_t ltok;

	z80ic_parser_next_input_tok(parser, &ltok);
	return ltok.ttype;
}

/** Read next token.
 *
 * @param parser Z80 IC parser
 * @param tok Place to store token
 */
static void z80ic_parser_read_next_tok(z80ic_parser_t *parser,
    z80ic_lexer_tok_t *tok)
{
	z80ic_parser_next_input_tok(parser, tok);
}

static int z80ic_parser_dprint_next_tok(z80ic_parser_t *parser, FILE *f)
{
	z80ic_lexer_tok_t tok;

	z80ic_parser_read_next_tok(parser, &tok);
	return z80ic_lexer_dprint_tok(&tok, f);
}

/** Skip over current token.
 *
 * @param parser Z80 IC parser
 */
static void z80ic_parser_skip(z80ic_parser_t *parser)
{
	z80ic_lexer_tok_t tok;

	/* Find non-ignored token */
	z80ic_parser_next_input_tok(parser, &tok);
	z80ic_lexer_free_tok(&tok);

	/* Skip over */
	parser->input_ops->next_tok(parser->input_arg);
}

/** Match a particular token type.
 *
 * If the type of the next token is @a mtype, skip over it. Otherwise
 * generate an error.
 *
 * @param parser Z80 IC parser
 * @param mtype Expected token type
 *
 * @return EOK on sucecss, EINVAL if token does not have expected type
 */
static int z80ic_parser_match(z80ic_parser_t *parser,
    z80ic_lexer_toktype_t mtype)
{
	z80ic_lexer_toktype_t ztt;

	ztt = z80ic_parser_next_ttype(parser);
	if (ztt != mtype) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected %s.\n",
		    z80ic_lexer_str_ttype(mtype));
		return EINVAL;
	}

	z80ic_parser_skip(parser);
	return EOK;
}

/** Determine if token is one of the general purpose 8-bit registers (r).
 *
 * @param ztt Token type
 * @return @c true iff ztt is one of A, B, C, D, E, H, L.
 */
static bool z80ic_parser_ttype_reg(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_A:
	case ztt_B:
	case ztt_C:
	case ztt_D:
	case ztt_E:
	case ztt_H:
	case ztt_L:
		return true;
	default:
		return false;
	}
}

/** Get register from token,
 *
 * @param ztt Token type
 * @return Register number / z80ic_reg_t.
 */
static z80ic_reg_t z80ic_parser_ttype_get_reg(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_A:
		return z80ic_reg_a;
	case ztt_B:
		return z80ic_reg_b;
	case ztt_C:
		return z80ic_reg_c;
	case ztt_D:
		return z80ic_reg_d;
	case ztt_E:
		return z80ic_reg_e;
	case ztt_H:
		return z80ic_reg_h;
	case ztt_L:
		return z80ic_reg_l;
	default:
		assert(false);
		return z80ic_reg_a;
	}
}

/** Determine if token is one of the four 16-bit dd registers.
 *
 * @param ztt Token type
 * @return @c true iff ztt is one of BC, DE, HL, SP.
 */
static bool z80ic_parser_ttype_dd(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
	case ztt_DE:
	case ztt_HL:
	case ztt_SP:
		return true;
	default:
		return false;
	}
}

/** Get 16-bit dd register from token,
 *
 * @param ztt Token type
 * @return 16-bit dd register
 */
static z80ic_dd_t z80ic_parser_ttype_get_dd(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
		return z80ic_dd_bc;
	case ztt_DE:
		return z80ic_dd_de;
	case ztt_HL:
		return z80ic_dd_hl;
	case ztt_SP:
		return z80ic_dd_sp;
	default:
		assert(false);
		return z80ic_dd_bc;
	}
}

/** Determine if token is one of the four 16-bit qq registers.
 *
 * @param ztt Token type
 * @return @c true iff ztt is one of BC, DE, HL, AF.
 */
static bool z80ic_parser_ttype_qq(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
	case ztt_DE:
	case ztt_HL:
	case ztt_AF:
		return true;
	default:
		return false;
	}
}

/** Get 16-bit qq register from token,
 *
 * @param ztt Token type
 * @return 16-bit qq register
 */
static z80ic_qq_t z80ic_parser_ttype_get_qq(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
		return z80ic_qq_bc;
	case ztt_DE:
		return z80ic_qq_de;
	case ztt_HL:
		return z80ic_qq_hl;
	case ztt_AF:
		return z80ic_qq_af;
	default:
		assert(false);
		return z80ic_qq_bc;
	}
}

/** Determine if token is one of the four 16-bit pp registers.
 *
 * @param ztt Token type
 * @return @c true iff ztt is one of BC, DE, IX, SP.
 */
static bool z80ic_parser_ttype_pp(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
	case ztt_DE:
	case ztt_IX:
	case ztt_SP:
		return true;
	default:
		return false;
	}
}

/** Get 16-bit pp register from token,
 *
 * @param ztt Token type
 * @return 16-bit pp register
 */
static z80ic_pp_t z80ic_parser_ttype_get_pp(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
		return z80ic_pp_bc;
	case ztt_DE:
		return z80ic_pp_de;
	case ztt_IX:
		return z80ic_pp_ix;
	case ztt_SP:
		return z80ic_pp_sp;
	default:
		assert(false);
		return z80ic_pp_bc;
	}
}

/** Determine if token is one of the four 16-bit rr registers.
 *
 * @param ztt Token type
 * @return @c true iff ztt is one of BC, DE, IY, SP.
 */
static bool z80ic_parser_ttype_rr(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
	case ztt_DE:
	case ztt_IY:
	case ztt_SP:
		return true;
	default:
		return false;
	}
}

/** Get 16-bit rr register from token,
 *
 * @param ztt Token type
 * @return 16-bit rr register
 */
static z80ic_rr_t z80ic_parser_ttype_get_rr(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
		return z80ic_rr_bc;
	case ztt_DE:
		return z80ic_rr_de;
	case ztt_IY:
		return z80ic_rr_iy;
	case ztt_SP:
		return z80ic_rr_sp;
	default:
		assert(false);
		return z80ic_rr_bc;
	}
}

/** Determine if token is one of the four 16-bit ss registers.
 *
 * @param ztt Token type
 * @return @c true iff ztt is one of BC, DE, HL, SP.
 */
static bool z80ic_parser_ttype_ss(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
	case ztt_DE:
	case ztt_HL:
	case ztt_SP:
		return true;
	default:
		return false;
	}
}

/** Get 16-bit ss register from token,
 *
 * @param ztt Token type
 * @return 16-bit ss register
 */
static z80ic_ss_t z80ic_parser_ttype_get_ss(z80ic_lexer_toktype_t ztt)
{
	switch (ztt) {
	case ztt_BC:
		return z80ic_ss_bc;
	case ztt_DE:
		return z80ic_ss_de;
	case ztt_HL:
		return z80ic_ss_hl;
	case ztt_SP:
		return z80ic_ss_sp;
	default:
		assert(false);
		return z80ic_ss_bc;
	}
}

/** Parse Z80 IC 8-bit immediate operand.
 *
 * @param parser Z80 IC parser
 * @param rimm Place to store pointer to new immediate operand
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_oper_imm8(z80ic_parser_t *parser,
    z80ic_oper_imm8_t **rimm)
{
	z80ic_lexer_tok_t itok;
	z80ic_oper_imm8_t *imm = NULL;
	int32_t value;
	int rc;

	z80ic_parser_read_next_tok(parser, &itok);
	assert(itok.ttype == ztt_number);

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	if (value < 0 || (uint32_t)value > 0xff) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is out of range of [0, 255].\n");
		return rc;
	}

	rc = z80ic_oper_imm8_create((uint8_t)value, &imm);
	if (rc != EOK)
		return rc;

	z80ic_parser_skip(parser);

	*rimm = imm;
	return EOK;
}

/** Parse Z80 IC 16-bit immediate operand.
 *
 * @param parser Z80 IC parser
 * @param rimm Place to store pointer to new immediate operand
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_oper_imm16(z80ic_parser_t *parser,
    z80ic_oper_imm16_t **rimm)
{
	z80ic_lexer_tok_t itok;
	z80ic_oper_imm16_t *imm = NULL;
	int32_t value;
	int rc;

	z80ic_parser_read_next_tok(parser, &itok);
	assert(itok.ttype == ztt_number);

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	if (value < 0 || (uint32_t)value > 0xffffu) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is out of range of [0, 65535].\n");
		return rc;
	}

	rc = z80ic_oper_imm16_create_val((uint16_t)value, &imm);
	if (rc != EOK)
		return rc;

	z80ic_parser_skip(parser);

	*rimm = imm;
	return EOK;
}

/** Parse Z80 IC 8-bit displacement.
 *
 * @param parser Z80 IC parser
 * @param rdisp Place to store displacement
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_disp(z80ic_parser_t *parser, int8_t *rdisp)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_lexer_tok_t itok;
	int32_t value;
	bool negative;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);

	switch (ztt) {
	case ztt_plus:
		negative = false;
		break;
	case ztt_minus:
		negative = true;
		break;
	default:
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " sign (+/-) expected.\n");
		return EINVAL;
	}

	z80ic_parser_skip(parser);

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_number) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " number expected.\n");
		return EINVAL;
	}

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	if ((negative && value > 128) || (!negative && value > 127)) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " Displacement %" PRId32 " is out "
		    "of range of [-128, 127].\n", value);
		return EINVAL;
	}

	z80ic_parser_skip(parser);

	if (negative)
		*rdisp = (int8_t)-value;
	else
		*rdisp = (int8_t)value;
	return EOK;
}

/** Parse Z80 IC load register from register instruction.
 *
 * @param parser Z80 IC parser
 * @param dreg Destination register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_r(z80ic_parser_t *parser, z80ic_reg_t dreg,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_r_r_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_ld_r_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load register from 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param dreg Destination register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_n(z80ic_parser_t *parser, z80ic_reg_t dreg,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_r_n_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_r_n_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm8 = imm8;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load register from (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param dreg Destination register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_ihl(z80ic_parser_t *parser,
    z80ic_reg_t dreg, z80ic_instr_t **rinstr)
{
	z80ic_ld_r_ihl_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_r_ihl_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load register from (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param dreg Destination register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_iixd(z80ic_parser_t *parser,
    z80ic_reg_t dreg, z80ic_instr_t **rinstr)
{
	z80ic_ld_r_iixd_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_r_iixd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->disp = disp;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load register from (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param dreg Destination register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_iiyd(z80ic_parser_t *parser,
    z80ic_reg_t dreg, z80ic_instr_t **rinstr)
{
	z80ic_ld_r_iiyd_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_r_iiyd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->disp = disp;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load A from (BC) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_a_ibc(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_a_ibc_t *ld;
	int rc;

	/* Skip 'BC'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_a_ibc_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load A from (DE) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_a_ide(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_a_ide_t *ld;
	int rc;

	/* Skip 'DE'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_a_ide_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load A from fixed memory location instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_a_inn(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_a_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80ic_ld_a_inn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load (BC) from A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ibc_a(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_ibc_a_t *ld;
	int rc;

	/* Skip 'BC'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_A);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_ibc_a_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load (DE) from A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ide_a(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_ide_a_t *ld;
	int rc;

	/* Skip 'DE'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_A);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_ide_a_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load fixed memory location from A instruction.
 *
 * @param parser Z80 IC parser
 * @param imm16 16-bit immediate operand
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_inn_a(z80ic_parser_t *parser,
    z80ic_oper_imm16_t *imm16, z80ic_instr_t **rinstr)
{
	z80ic_ld_inn_a_t *ld = NULL;
	int rc;

	rc = z80ic_parser_match(parser, ztt_A);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_inn_a_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load fixed memory location from HL instruction.
 *
 * @param parser Z80 IC parser
 * @param imm16 16-bit immediate operand
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_inn_hl(z80ic_parser_t *parser,
    z80ic_oper_imm16_t *imm16, z80ic_instr_t **rinstr)
{
	z80ic_ld_inn_hl_t *ld = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_inn_hl_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load fixed memory location from 16-bit dd register instruction.
 *
 * @param parser Z80 IC parser
 * @param imm16 16-bit immediate operand
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_inn_dd(z80ic_parser_t *parser,
    z80ic_oper_imm16_t *imm16, z80ic_instr_t **rinstr)
{
	z80ic_ld_inn_dd_t *ld = NULL;
	z80ic_oper_dd_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_dd_t dd;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	dd = z80ic_parser_ttype_get_dd(ztt);

	/* Skip 'dd'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_inn_dd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(dd, &src);
	if (rc != EOK)
		goto error;

	ld->src = src;
	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_dd_destroy(src);
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load fixed memory location from IX instruction.
 *
 * @param parser Z80 IC parser
 * @param imm16 16-bit immediate operand
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_inn_ix(z80ic_parser_t *parser,
    z80ic_oper_imm16_t *imm16, z80ic_instr_t **rinstr)
{
	z80ic_ld_inn_ix_t *ld = NULL;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_inn_ix_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load fixed memory location from IY instruction.
 *
 * @param parser Z80 IC parser
 * @param imm16 16-bit immediate operand
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_inn_iy(z80ic_parser_t *parser,
    z80ic_oper_imm16_t *imm16, z80ic_instr_t **rinstr)
{
	z80ic_ld_inn_iy_t *ld = NULL;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_inn_iy_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load fixed memory location from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_inn_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_inn_a_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	z80ic_lexer_toktype_t ztt;
	int rc;

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		goto error;

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_ld_inn_hl(parser, imm16, rinstr);
		if (rc != EOK)
			goto error;
	} else if (z80ic_parser_ttype_dd(ztt)) {
		rc = z80ic_parser_process_ld_inn_dd(parser, imm16, rinstr);
		if (rc != EOK)
			goto error;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_ld_inn_ix(parser, imm16, rinstr);
		if (rc != EOK)
			goto error;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_ld_inn_iy(parser, imm16, rinstr);
		if (rc != EOK)
			goto error;
	} else {
		rc = z80ic_parser_process_ld_inn_a(parser, imm16, rinstr);
		if (rc != EOK)
			goto error;
	}

	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load A from interrupt vector register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_a_i(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_a_i_t *ld;
	int rc;

	/* Skip 'I'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_a_i_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load A from refresh register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_a_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_a_r_t *ld;
	int rc;

	/* Skip 'R'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_a_r_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load interrupt vector register from A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_i_a(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_i_a_t *ld;
	int rc;

	/* Skip 'I'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_A);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_i_a_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load refresh register from A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_a(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_r_a_t *ld;
	int rc;

	/* Skip 'R'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_A);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_r_a_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load register from (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param dreg Destination register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_ixx(z80ic_parser_t *parser,
    z80ic_reg_t dreg, z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_ld_r_ihl(parser, dreg, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_ld_r_iixd(parser, dreg, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_ld_r_iiyd(parser, dreg, rinstr);
		if (rc != EOK)
			return rc;
	} else if (dreg == z80ic_reg_a && ztt == ztt_BC) {
		rc = z80ic_parser_process_ld_a_ibc(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (dreg == z80ic_reg_a && ztt == ztt_DE) {
		rc = z80ic_parser_process_ld_a_ide(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_a_inn(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC load register from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_r_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t reg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	reg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_ld_r_r(parser, reg, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_ld_r_ixx(parser, reg, rinstr);
		if (rc != EOK)
			return rc;
	} else if (reg == z80ic_reg_a && ztt == ztt_I) {
		rc = z80ic_parser_process_ld_a_i(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (reg == z80ic_reg_a && ztt == ztt_R) {
		rc = z80ic_parser_process_ld_a_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_r_n(parser, reg, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC load (HL) from register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ihl_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_ihl_r_t *ld = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_ld_ihl_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->src = src;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load (IX+d) from register instruction.
 *
 * @param parser Z80 IC parser
 * @param disp Displacement
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iixd_r(z80ic_parser_t *parser,
    int8_t disp, z80ic_instr_t **rinstr)
{
	z80ic_ld_iixd_r_t *ld = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_ld_iixd_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->src = src;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load (IY+d) from register instruction.
 *
 * @param parser Z80 IC parser
 * @param disp Displacement
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iiyd_r(z80ic_parser_t *parser,
    int8_t disp, z80ic_instr_t **rinstr)
{
	z80ic_ld_iiyd_r_t *ld = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_ld_iiyd_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->src = src;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load (HL) from 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ihl_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_ihl_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_ihl_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load (IX+d) from 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param disp Displacement
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iixd_n(z80ic_parser_t *parser,
    int8_t disp, z80ic_instr_t **rinstr)
{
	z80ic_ld_iixd_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_iixd_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->imm8 = imm8;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load (IY+d) from 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param disp Displacement
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iiyd_n(z80ic_parser_t *parser,
    int8_t disp, z80ic_instr_t **rinstr)
{
	z80ic_ld_iiyd_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_iiyd_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->imm8 = imm8;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load (HL) from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ihl_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'HL' */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_ld_ihl_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_ihl_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC load (IX+d) from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iixd_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_ld_iixd_r(parser, disp, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_iixd_n(parser, disp, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC load (IY+d) from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iiyd_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_ld_iiyd_r(parser, disp, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_iiyd_n(parser, disp, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC load (XX) from YY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ixx_yy(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	switch (ztt) {
	case ztt_HL:
		rc = z80ic_parser_process_ld_ihl_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_IX:
		rc = z80ic_parser_process_ld_iixd_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_IY:
		rc = z80ic_parser_process_ld_iiyd_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_BC:
		rc = z80ic_parser_process_ld_ibc_a(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_DE:
		rc = z80ic_parser_process_ld_ide_a(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	default:
		rc = z80ic_parser_process_ld_inn_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	}

	return EOK;
}

/** Parse Z80 IC load 16-bit dd register from 16-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param dd Destination 16-bit register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_dd_nn(z80ic_parser_t *parser,
    z80ic_dd_t dd, z80ic_instr_t **rinstr)
{
	z80ic_ld_dd_nn_t *ld = NULL;
	z80ic_oper_dd_t *dest = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_dd_nn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(dd, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_dd_destroy(dest);
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load HL from fixed memory location instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_hl_inn(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_hl_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_lparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_hl_inn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load 16-bit dd register from fixed memory location instruction.
 *
 * @param parser Z80 IC parser
 * @param dd Destination 16-bit register
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_dd_inn(z80ic_parser_t *parser,
    z80ic_dd_t dd, z80ic_instr_t **rinstr)
{
	z80ic_ld_dd_inn_t *ld = NULL;
	z80ic_oper_dd_t *dest = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	/* Skip '('. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_dd_inn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(dd, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_dd_destroy(dest);
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load SP from HL instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_sp_hl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_sp_hl_t *ld = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_sp_hl_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load SP from IX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_sp_ix(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_sp_ix_t *ld = NULL;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_sp_ix_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load SP from IY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_sp_iy(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_sp_iy_t *ld = NULL;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ld_sp_iy_create(&ld);
	if (rc != EOK)
		return rc;

	*rinstr = &ld->instr;
	return EOK;
}

/** Parse Z80 IC load 16-bit dd register from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_dd_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_dd_t dd;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	dd = z80ic_parser_ttype_get_dd(ztt);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (dd == z80ic_dd_sp && ztt == ztt_HL) {
		rc = z80ic_parser_process_ld_sp_hl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (dd == z80ic_dd_sp && ztt == ztt_IX) {
		rc = z80ic_parser_process_ld_sp_ix(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (dd == z80ic_dd_sp && ztt == ztt_IY) {
		rc = z80ic_parser_process_ld_sp_iy(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_ld_dd_inn(parser, dd, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_dd_nn(parser, dd, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC load IX from 16-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ix_nn(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_ix_nn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_ix_nn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load IX from 16-bit fixed memory location instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ix_inn(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_ix_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	/* Skip '('. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_ix_inn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load IX from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_ix_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_ld_ix_inn(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_ix_nn(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC load IY from 16-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iy_nn(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_iy_nn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_iy_nn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load IY from 16-bit fixed memory location instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iy_inn(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ld_iy_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	/* Skip '('. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_oper_imm16(parser, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_ld_iy_inn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	*rinstr = &ld->instr;
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Parse Z80 IC load IY from XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld_iy_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_ld_iy_inn(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_ld_iy_nn(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC load instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ld(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'ld' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_ld_r_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_ld_ix_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_ld_iy_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_HL) {
		rc = z80ic_parser_process_ld_hl_inn(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (z80ic_parser_ttype_dd(ztt)) {
		rc = z80ic_parser_process_ld_dd_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_ld_ixx_yy(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_I) {
		rc = z80ic_parser_process_ld_i_a(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_R) {
		rc = z80ic_parser_process_ld_r_a(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC push qq 16-bit register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_push_qq(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_push_qq_t *push = NULL;
	z80ic_oper_qq_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_qq_t qq;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	qq = z80ic_parser_ttype_get_qq(ztt);

	/* Skip 'qq'. */
	z80ic_parser_skip(parser);

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(qq, &src);
	if (rc != EOK)
		goto error;

	push->src = src;

	*rinstr = &push->instr;
	return EOK;
error:
	z80ic_oper_qq_destroy(src);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	return rc;
}

/** Parse Z80 IC push IX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_push_ix(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_push_ix_t *push = NULL;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_push_ix_create(&push);
	if (rc != EOK)
		return rc;

	*rinstr = &push->instr;
	return EOK;
}

/** Parse Z80 IC push IY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_push_iy(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_push_iy_t *push = NULL;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_push_iy_create(&push);
	if (rc != EOK)
		return rc;

	*rinstr = &push->instr;
	return EOK;
}

/** Parse Z80 IC push instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_push(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'push' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	if (z80ic_parser_ttype_qq(ztt)) {
		rc = z80ic_parser_process_push_qq(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_push_ix(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_push_iy(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC pop qq 16-bit register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_pop_qq(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_pop_qq_t *pop = NULL;
	z80ic_oper_qq_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_qq_t qq;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	qq = z80ic_parser_ttype_get_qq(ztt);

	/* Skip 'qq'. */
	z80ic_parser_skip(parser);

	rc = z80ic_pop_qq_create(&pop);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(qq, &src);
	if (rc != EOK)
		goto error;

	pop->src = src;

	*rinstr = &pop->instr;
	return EOK;
error:
	z80ic_oper_qq_destroy(src);
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);
	return rc;
}

/** Parse Z80 IC pop IX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_pop_ix(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_pop_ix_t *pop = NULL;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_pop_ix_create(&pop);
	if (rc != EOK)
		return rc;

	*rinstr = &pop->instr;
	return EOK;
}

/** Parse Z80 IC pop IY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_pop_iy(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_pop_iy_t *pop = NULL;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_pop_iy_create(&pop);
	if (rc != EOK)
		return rc;

	*rinstr = &pop->instr;
	return EOK;
}

/** Parse Z80 IC pop instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_pop(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'pop' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	if (z80ic_parser_ttype_qq(ztt)) {
		rc = z80ic_parser_process_pop_qq(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_pop_ix(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_pop_iy(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC exchange DE and HL instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ex_de_hl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ex_de_hl_t *ex = NULL;
	int rc;

	/* Skip 'DE'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_HL);
	if (rc != EOK)
		return rc;

	rc = z80ic_ex_de_hl_create(&ex);
	if (rc != EOK)
		return rc;

	*rinstr = &ex->instr;
	return EOK;
}

/** Parse Z80 IC exchange AF and AF' instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ex_af_afp(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ex_af_afp_t *ex = NULL;
	int rc;

	/* Skip 'AF'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_AF_);
	if (rc != EOK)
		return rc;

	rc = z80ic_ex_af_afp_create(&ex);
	if (rc != EOK)
		return rc;

	*rinstr = &ex->instr;
	return EOK;
}

/** Parse Z80 IC exchange (SP) and HL instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ex_isp_hl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ex_isp_hl_t *ex = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ex_isp_hl_create(&ex);
	if (rc != EOK)
		return rc;

	*rinstr = &ex->instr;
	return EOK;
}

/** Parse Z80 IC exchange (SP) and IX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ex_isp_ix(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ex_isp_ix_t *ex = NULL;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ex_isp_ix_create(&ex);
	if (rc != EOK)
		return rc;

	*rinstr = &ex->instr;
	return EOK;
}

/** Parse Z80 IC exchange (SP) and IY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ex_isp_iy(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ex_isp_iy_t *ex = NULL;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_ex_isp_iy_create(&ex);
	if (rc != EOK)
		return rc;

	*rinstr = &ex->instr;
	return EOK;
}

/** Parse Z80 IC exchange (SP) and XX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ex_isp_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_SP);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);
	switch (ztt) {
	case ztt_HL:
		rc = z80ic_parser_process_ex_isp_hl(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_IX:
		rc = z80ic_parser_process_ex_isp_ix(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_IY:
		rc = z80ic_parser_process_ex_isp_iy(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	default:
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC exchange instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ex(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'ex' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	switch (ztt) {
	case ztt_DE:
		rc = z80ic_parser_process_ex_de_hl(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_AF:
		rc = z80ic_parser_process_ex_af_afp(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	case ztt_lparen:
		rc = z80ic_parser_process_ex_isp_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
		break;
	default:
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC exchange BC, DE, HL with BC', DE', HL' instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_exx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_exx_t *exx;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_exx_create(&exx);
	if (rc != EOK)
		return rc;

	*rinstr = &exx->instr;
	return EOK;
}

/** Parse Z80 IC load, increment instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ldi(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ldi_t *ldi;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_ldi_create(&ldi);
	if (rc != EOK)
		return rc;

	*rinstr = &ldi->instr;
	return EOK;
}

/** Parse Z80 IC load, increment, repeat instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ldir(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ldir_t *ldir;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_ldir_create(&ldir);
	if (rc != EOK)
		return rc;

	*rinstr = &ldir->instr;
	return EOK;
}

/** Parse Z80 IC load, decrement instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ldd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ldd_t *ldd;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_ldd_create(&ldd);
	if (rc != EOK)
		return rc;

	*rinstr = &ldd->instr;
	return EOK;
}

/** Parse Z80 IC load, decrement, repeat instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_lddr(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lddr_t *lddr;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_lddr_create(&lddr);
	if (rc != EOK)
		return rc;

	*rinstr = &lddr->instr;
	return EOK;
}

/** Parse Z80 IC compare, increment instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cpi(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cpi_t *cpi;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_cpi_create(&cpi);
	if (rc != EOK)
		return rc;

	*rinstr = &cpi->instr;
	return EOK;
}

/** Parse Z80 IC compare, increment, repeat instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cpir(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cpir_t *cpir;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_cpir_create(&cpir);
	if (rc != EOK)
		return rc;

	*rinstr = &cpir->instr;
	return EOK;
}

/** Parse Z80 IC compare, decrement instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cpd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cpd_t *cpd;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_cpd_create(&cpd);
	if (rc != EOK)
		return rc;

	*rinstr = &cpd->instr;
	return EOK;
}

/** Parse Z80 IC compare, decrement, repeat instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cpdr(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cpdr_t *cpdr;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_cpdr_create(&cpdr);
	if (rc != EOK)
		return rc;

	*rinstr = &cpdr->instr;
	return EOK;
}

/** Parse Z80 IC add register to A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_a_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_a_r_t *add = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_add_a_r_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	*rinstr = &add->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Parse Z80 IC add 8-bit immediate to A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_a_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_a_n_t *add = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;

	*rinstr = &add->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Parse Z80 IC add (HL) to A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_a_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_a_ihl_t *add = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_add_a_ihl_create(&add);
	if (rc != EOK)
		return rc;

	*rinstr = &add->instr;
	return EOK;
}

/** Parse Z80 IC add (IX+d) to A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_a_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_a_iixd_t *add = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_add_a_iixd_create(&add);
	if (rc != EOK)
		return rc;

	add->disp = disp;

	*rinstr = &add->instr;
	return EOK;
}

/** Parse Z80 IC add (IY+d) to A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_a_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_a_iiyd_t *add = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_add_a_iiyd_create(&add);
	if (rc != EOK)
		return rc;

	add->disp = disp;

	*rinstr = &add->instr;
	return EOK;
}

/** Parse Z80 IC add (XX) to A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_a_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_add_a_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_add_a_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_add_a_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC add XX to A instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_a_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'A'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_add_a_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_add_a_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_add_a_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC add ss 16-bit register to HL instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_hl_ss(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_hl_ss_t *add = NULL;
	z80ic_oper_ss_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_ss_t ss;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);
	if (!z80ic_parser_ttype_ss(ztt)) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " expected BC, DE, HL or SP.\n");
		return EINVAL;
	}

	ss = z80ic_parser_ttype_get_ss(ztt);
	z80ic_parser_skip(parser);

	rc = z80ic_add_hl_ss_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	*rinstr = &add->instr;
	return EOK;
error:
	z80ic_oper_ss_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Parse Z80 IC add pp 16-bit register to IX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_ix_pp(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_ix_pp_t *add = NULL;
	z80ic_oper_pp_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_pp_t pp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);
	if (!z80ic_parser_ttype_pp(ztt)) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " expected BC, DE, IX or SP.\n");
		return EINVAL;
	}

	pp = z80ic_parser_ttype_get_pp(ztt);
	z80ic_parser_skip(parser);

	rc = z80ic_add_ix_pp_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_pp_create(pp, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	*rinstr = &add->instr;
	return EOK;
error:
	z80ic_oper_pp_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Parse Z80 IC add rr 16-bit register to IY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add_iy_rr(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_add_iy_rr_t *add = NULL;
	z80ic_oper_rr_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_rr_t rr;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);
	if (!z80ic_parser_ttype_rr(ztt)) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " expected BC, DE, IY or SP.\n");
		return EINVAL;
	}

	rr = z80ic_parser_ttype_get_rr(ztt);
	z80ic_parser_skip(parser);

	rc = z80ic_add_iy_rr_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_rr_create(rr, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	*rinstr = &add->instr;
	return EOK;
error:
	z80ic_oper_rr_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Parse Z80 IC add instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_add(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'add' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	if (ztt == ztt_A) {
		rc = z80ic_parser_process_add_a_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_HL) {
		rc = z80ic_parser_process_add_hl_ss(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_add_ix_pp(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_add_iy_rr(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC add register to A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_a_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_adc_a_r_t *add = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_adc_a_r_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	*rinstr = &add->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Parse Z80 IC add 8-bit immediate to A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_a_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_adc_a_n_t *add = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_adc_a_n_create(&add);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;

	*rinstr = &add->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Parse Z80 IC add (HL) to A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_a_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_adc_a_ihl_t *add = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_adc_a_ihl_create(&add);
	if (rc != EOK)
		return rc;

	*rinstr = &add->instr;
	return EOK;
}

/** Parse Z80 IC add (IX+d) to A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_a_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_adc_a_iixd_t *add = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_adc_a_iixd_create(&add);
	if (rc != EOK)
		return rc;

	add->disp = disp;

	*rinstr = &add->instr;
	return EOK;
}

/** Parse Z80 IC add (IY+d) to A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_a_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_adc_a_iiyd_t *add = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_adc_a_iiyd_create(&add);
	if (rc != EOK)
		return rc;

	add->disp = disp;

	*rinstr = &add->instr;
	return EOK;
}

/** Parse Z80 IC add (XX) to A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_a_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_adc_a_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_adc_a_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_adc_a_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC add XX to A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_a_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'A'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_adc_a_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_adc_a_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_adc_a_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC add ss 16-bit register to HL with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc_hl_ss(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_adc_hl_ss_t *adc = NULL;
	z80ic_oper_ss_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_ss_t ss;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);
	if (!z80ic_parser_ttype_ss(ztt)) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " expected BC, DE, HL or SP.\n");
		return EINVAL;
	}

	ss = z80ic_parser_ttype_get_ss(ztt);
	z80ic_parser_skip(parser);

	rc = z80ic_adc_hl_ss_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &src);
	if (rc != EOK)
		goto error;

	adc->src = src;

	*rinstr = &adc->instr;
	return EOK;
error:
	z80ic_oper_ss_destroy(src);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	return rc;
}

/** Parse Z80 IC add with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_adc(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'adc' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	if (ztt == ztt_A) {
		rc = z80ic_parser_process_adc_a_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_HL) {
		rc = z80ic_parser_process_adc_hl_ss(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC subtract register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sub_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sub_r_t *sub = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_sub_r_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	sub->src = src;

	*rinstr = &sub->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	return rc;
}

/** Parse Z80 IC subtract 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sub_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sub_n_t *sub = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_sub_n_create(&sub);
	if (rc != EOK)
		goto error;

	sub->imm8 = imm8;

	*rinstr = &sub->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	return rc;
}

/** Parse Z80 IC subtract (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sub_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sub_ihl_t *sub = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_sub_ihl_create(&sub);
	if (rc != EOK)
		return rc;

	*rinstr = &sub->instr;
	return EOK;
}

/** Parse Z80 IC subtract (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sub_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sub_iixd_t *sub = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_sub_iixd_create(&sub);
	if (rc != EOK)
		return rc;

	sub->disp = disp;

	*rinstr = &sub->instr;
	return EOK;
}

/** Parse Z80 IC subtract (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sub_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sub_iiyd_t *sub = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_sub_iiyd_create(&sub);
	if (rc != EOK)
		return rc;

	sub->disp = disp;

	*rinstr = &sub->instr;
	return EOK;
}

/** Parse Z80 IC subtract (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sub_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_sub_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_sub_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_sub_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC subtract instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sub(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'sub'. */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_sub_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_sub_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_sub_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC subtract register from A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_a_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sbc_a_r_t *sbc = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_sbc_a_r_create(&sbc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	sbc->src = src;

	*rinstr = &sbc->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	return rc;
}

/** Parse Z80 IC subtract 8-bit immediate from A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_a_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sbc_a_n_t *sbc = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_sbc_a_n_create(&sbc);
	if (rc != EOK)
		goto error;

	sbc->imm8 = imm8;

	*rinstr = &sbc->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	return rc;
}

/** Parse Z80 IC subtract (HL) from A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_a_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sbc_a_ihl_t *sbc = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_sbc_a_ihl_create(&sbc);
	if (rc != EOK)
		return rc;

	*rinstr = &sbc->instr;
	return EOK;
}

/** Parse Z80 IC subtract (IX+d) from A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_a_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sbc_a_iixd_t *sbc = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_sbc_a_iixd_create(&sbc);
	if (rc != EOK)
		return rc;

	sbc->disp = disp;

	*rinstr = &sbc->instr;
	return EOK;
}

/** Parse Z80 IC subtract (IY+d) from A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_a_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sbc_a_iiyd_t *sbc = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_sbc_a_iiyd_create(&sbc);
	if (rc != EOK)
		return rc;

	sbc->disp = disp;

	*rinstr = &sbc->instr;
	return EOK;
}

/** Parse Z80 IC subtract (XX) from A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_a_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_sbc_a_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_sbc_a_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_sbc_a_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC subtract XX from A with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_a_xx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'A'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_sbc_a_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_sbc_a_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_sbc_a_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC subtract ss 16-bit register from HL with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc_hl_ss(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_sbc_hl_ss_t *sbc = NULL;
	z80ic_oper_ss_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_ss_t ss;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_match(parser, ztt_comma);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);
	if (!z80ic_parser_ttype_ss(ztt)) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " expected BC, DE, HL or SP.\n");
		return EINVAL;
	}

	ss = z80ic_parser_ttype_get_ss(ztt);
	z80ic_parser_skip(parser);

	rc = z80ic_sbc_hl_ss_create(&sbc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &src);
	if (rc != EOK)
		goto error;

	sbc->src = src;

	*rinstr = &sbc->instr;
	return EOK;
error:
	z80ic_oper_ss_destroy(src);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	return rc;
}



/** Parse Z80 IC subtract with carry instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_sbc(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'sbc' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);
	if (ztt == ztt_A) {
		rc = z80ic_parser_process_sbc_a_xx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_HL) {
		rc = z80ic_parser_process_sbc_hl_ss(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC bitwise AND with register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_and_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_and_r_t *band = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_and_r_create(&band);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	band->src = src;

	*rinstr = &band->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (band != NULL)
		z80ic_instr_destroy(&band->instr);
	return rc;
}

/** Parse Z80 IC bitwise AND with 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_and_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_and_n_t *band = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_and_n_create(&band);
	if (rc != EOK)
		goto error;

	band->imm8 = imm8;

	*rinstr = &band->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (band != NULL)
		z80ic_instr_destroy(&band->instr);
	return rc;
}

/** Parse Z80 IC bitwise AND with (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_and_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_and_ihl_t *band = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_and_ihl_create(&band);
	if (rc != EOK)
		return rc;

	*rinstr = &band->instr;
	return EOK;
}

/** Parse Z80 IC bitwise AND with (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_and_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_and_iixd_t *band = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_and_iixd_create(&band);
	if (rc != EOK)
		return rc;

	band->disp = disp;

	*rinstr = &band->instr;
	return EOK;
}

/** Parse Z80 IC bitwise AND with (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_and_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_and_iiyd_t *band = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_and_iiyd_create(&band);
	if (rc != EOK)
		return rc;

	band->disp = disp;

	*rinstr = &band->instr;
	return EOK;
}

/** Parse Z80 IC bitwise AND with (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_and_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_and_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_and_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_and_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC bitwise AND instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_and(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'and'. */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_and_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_and_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_and_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}



/** Parse Z80 IC bitwise OR with register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_or_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_or_r_t *bor = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_or_r_create(&bor);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	bor->src = src;

	*rinstr = &bor->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (bor != NULL)
		z80ic_instr_destroy(&bor->instr);
	return rc;
}

/** Parse Z80 IC bitwise OR with 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_or_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_or_n_t *bor = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_or_n_create(&bor);
	if (rc != EOK)
		goto error;

	bor->imm8 = imm8;

	*rinstr = &bor->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (bor != NULL)
		z80ic_instr_destroy(&bor->instr);
	return rc;
}

/** Parse Z80 IC bitwise OR with (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_or_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_or_ihl_t *bor = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_or_ihl_create(&bor);
	if (rc != EOK)
		return rc;

	*rinstr = &bor->instr;
	return EOK;
}

/** Parse Z80 IC bitwise OR with (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_or_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_or_iixd_t *bor = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_or_iixd_create(&bor);
	if (rc != EOK)
		return rc;

	bor->disp = disp;

	*rinstr = &bor->instr;
	return EOK;
}

/** Parse Z80 IC bitwise OR with (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_or_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_or_iiyd_t *bor = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_or_iiyd_create(&bor);
	if (rc != EOK)
		return rc;

	bor->disp = disp;

	*rinstr = &bor->instr;
	return EOK;
}

/** Parse Z80 IC bitwise OR with (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_or_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_or_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_or_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_or_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC bitwise OR instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_or(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'or'. */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_or_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_or_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_or_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse Z80 IC bitwise XOR with register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_xor_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_xor_r_t *xor = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_xor_r_create(&xor);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	xor->src = src;

	*rinstr = &xor->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);
	return rc;
}

/** Parse Z80 IC bitwise XOR with 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_xor_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_xor_n_t *xor = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_xor_n_create(&xor);
	if (rc != EOK)
		goto error;

	xor->imm8 = imm8;

	*rinstr = &xor->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);
	return rc;
}

/** Parse Z80 IC bitwise XOR with (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_xor_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_xor_ihl_t *xor = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_xor_ihl_create(&xor);
	if (rc != EOK)
		return rc;

	*rinstr = &xor->instr;
	return EOK;
}

/** Parse Z80 IC bitwise XOR with (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_xor_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_xor_iixd_t *xor = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_xor_iixd_create(&xor);
	if (rc != EOK)
		return rc;

	xor->disp = disp;

	*rinstr = &xor->instr;
	return EOK;
}

/** Parse Z80 IC bitwise XOR with (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_xor_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_xor_iiyd_t *xor = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_xor_iiyd_create(&xor);
	if (rc != EOK)
		return rc;

	xor->disp = disp;

	*rinstr = &xor->instr;
	return EOK;
}

/** Parse Z80 IC bitwise XOR with (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_xor_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_xor_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_xor_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_xor_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC bitwise XOR instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_xor(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'xor'. */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_xor_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_xor_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_xor_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}



/** Parse Z80 IC compare with register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cp_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cp_r_t *cp = NULL;
	z80ic_oper_reg_t *src = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_cp_r_create(&cp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	cp->src = src;

	*rinstr = &cp->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (cp != NULL)
		z80ic_instr_destroy(&cp->instr);
	return rc;
}

/** Parse Z80 IC compare with 8-bit immediate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cp_n(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cp_n_t *cp = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80ic_parser_process_oper_imm8(parser, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_cp_n_create(&cp);
	if (rc != EOK)
		goto error;

	cp->imm8 = imm8;

	*rinstr = &cp->instr;
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (cp != NULL)
		z80ic_instr_destroy(&cp->instr);
	return rc;
}

/** Parse Z80 IC compare with (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cp_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cp_ihl_t *cp = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_cp_ihl_create(&cp);
	if (rc != EOK)
		return rc;

	*rinstr = &cp->instr;
	return EOK;
}

/** Parse Z80 IC compare with (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cp_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cp_iixd_t *cp = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_cp_iixd_create(&cp);
	if (rc != EOK)
		return rc;

	cp->disp = disp;

	*rinstr = &cp->instr;
	return EOK;
}

/** Parse Z80 IC compare with (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cp_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cp_iiyd_t *cp = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_cp_iiyd_create(&cp);
	if (rc != EOK)
		return rc;

	cp->disp = disp;

	*rinstr = &cp->instr;
	return EOK;
}

/** Parse Z80 IC compare with (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cp_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_cp_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_cp_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_cp_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC compare with instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cp(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'cp'. */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_cp_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_cp_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_parser_process_cp_n(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}



/** Parse Z80 IC increment register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_inc_r_t *inc = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_inc_r_create(&inc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	inc->dest = dest;

	*rinstr = &inc->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	return rc;
}

/** Parse Z80 IC increment (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_inc_ihl_t *inc = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_inc_ihl_create(&inc);
	if (rc != EOK)
		return rc;

	*rinstr = &inc->instr;
	return EOK;
}

/** Parse Z80 IC increment (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_inc_iixd_t *inc = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_inc_iixd_create(&inc);
	if (rc != EOK)
		return rc;

	inc->disp = disp;

	*rinstr = &inc->instr;
	return EOK;
}

/** Parse Z80 IC increment (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_inc_iiyd_t *inc = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_inc_iiyd_create(&inc);
	if (rc != EOK)
		return rc;

	inc->disp = disp;

	*rinstr = &inc->instr;
	return EOK;
}

/** Parse Z80 IC increment (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_inc_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_inc_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_inc_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC increment ss 16-bit register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_ss(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_ss_t *dest = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_ss_t ss;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);

	ss = z80ic_parser_ttype_get_ss(ztt);
	z80ic_parser_skip(parser);

	rc = z80ic_inc_ss_create(&inc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &dest);
	if (rc != EOK)
		goto error;

	inc->dest = dest;

	*rinstr = &inc->instr;
	return EOK;
error:
	z80ic_oper_ss_destroy(dest);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	return rc;
}

/** Parse Z80 IC increment IX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_ix(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_inc_ix_t *inc = NULL;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_inc_ix_create(&inc);
	if (rc != EOK)
		return rc;

	*rinstr = &inc->instr;
	return EOK;
}

/** Parse Z80 IC increment IY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc_iy(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_inc_iy_t *inc = NULL;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_inc_iy_create(&inc);
	if (rc != EOK)
		return rc;

	*rinstr = &inc->instr;
	return EOK;
}

/** Parse Z80 IC increment instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_inc(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'inc'. */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_inc_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_inc_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (z80ic_parser_ttype_ss(ztt)) {
		rc = z80ic_parser_process_inc_ss(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_inc_ix(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_inc_iy(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}



/** Parse Z80 IC decrement register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_r(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_dec_r_t *dec = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_reg_t sreg;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	z80ic_parser_skip(parser);

	sreg = z80ic_parser_ttype_get_reg(ztt);

	rc = z80ic_dec_r_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	dec->dest = dest;

	*rinstr = &dec->instr;
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	return rc;
}

/** Parse Z80 IC decrement (HL) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_ihl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_dec_ihl_t *dec = NULL;
	int rc;

	/* Skip 'HL'. */
	z80ic_parser_skip(parser);

	rc = z80ic_dec_ihl_create(&dec);
	if (rc != EOK)
		return rc;

	*rinstr = &dec->instr;
	return EOK;
}

/** Parse Z80 IC decrement (IX+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_iixd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_dec_iixd_t *dec = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_dec_iixd_create(&dec);
	if (rc != EOK)
		return rc;

	dec->disp = disp;

	*rinstr = &dec->instr;
	return EOK;
}

/** Parse Z80 IC decrement (IY+d) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_iiyd(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_dec_iiyd_t *dec = NULL;
	int8_t disp;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_parser_process_disp(parser, &disp);
	if (rc != EOK)
		return rc;

	rc = z80ic_dec_iiyd_create(&dec);
	if (rc != EOK)
		return rc;

	dec->disp = disp;

	*rinstr = &dec->instr;
	return EOK;
}

/** Parse Z80 IC decrement (XX) instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_ixx(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip '(' */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (ztt == ztt_HL) {
		rc = z80ic_parser_process_dec_ihl(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_dec_iixd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_dec_iiyd(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	rc = z80ic_parser_match(parser, ztt_rparen);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC decrement ss 16-bit register instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_ss(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_dec_ss_t *dec = NULL;
	z80ic_oper_ss_t *dest = NULL;
	z80ic_lexer_toktype_t ztt;
	z80ic_ss_t ss;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);

	ss = z80ic_parser_ttype_get_ss(ztt);
	z80ic_parser_skip(parser);

	rc = z80ic_dec_ss_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &dest);
	if (rc != EOK)
		goto error;

	dec->dest = dest;

	*rinstr = &dec->instr;
	return EOK;
error:
	z80ic_oper_ss_destroy(dest);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	return rc;
}

/** Parse Z80 IC decrement IX instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_ix(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_dec_ix_t *dec = NULL;
	int rc;

	/* Skip 'IX'. */
	z80ic_parser_skip(parser);

	rc = z80ic_dec_ix_create(&dec);
	if (rc != EOK)
		return rc;

	*rinstr = &dec->instr;
	return EOK;
}

/** Parse Z80 IC decrement IY instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec_iy(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_dec_iy_t *dec = NULL;
	int rc;

	/* Skip 'IY'. */
	z80ic_parser_skip(parser);

	rc = z80ic_dec_iy_create(&dec);
	if (rc != EOK)
		return rc;

	*rinstr = &dec->instr;
	return EOK;
}



/** Parse Z80 IC decrement instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dec(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Skip 'dec'. */
	z80ic_parser_skip(parser);

	ztt = z80ic_parser_next_ttype(parser);

	if (z80ic_parser_ttype_reg(ztt)) {
		rc = z80ic_parser_process_dec_r(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_lparen) {
		rc = z80ic_parser_process_dec_ixx(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (z80ic_parser_ttype_ss(ztt)) {
		rc = z80ic_parser_process_dec_ss(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IX) {
		rc = z80ic_parser_process_dec_ix(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (ztt == ztt_IY) {
		rc = z80ic_parser_process_dec_iy(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected.\n");
		return EINVAL;
	}

	return EOK;
}

/** Parse Z80 IC decimal adjust accumulator instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_daa(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_daa_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_daa_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC complement instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_cpl(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_cpl_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_cpl_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC negate instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_neg(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_neg_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_neg_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC complement carry flag instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ccf(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ccf_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_ccf_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC set carry flag instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_scf(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_scf_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_scf_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC no operation instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_nop(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_nop_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_nop_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC halt instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_halt(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_halt_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_halt_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC disable interrupt instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_di(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_di_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_di_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC enable interupt instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ei(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ei_t *daa;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_ei_create(&daa);
	if (rc != EOK)
		return rc;

	*rinstr = &daa->instr;
	return EOK;
}

/** Parse Z80 IC set interrupt mode 0 instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_im_0(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_im_0_t *im0;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_im_0_create(&im0);
	if (rc != EOK)
		return rc;

	*rinstr = &im0->instr;
	return EOK;
}

/** Parse Z80 IC set interrupt mode 1 instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_im_1(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_im_1_t *im1;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_im_1_create(&im1);
	if (rc != EOK)
		return rc;

	*rinstr = &im1->instr;
	return EOK;
}

/** Parse Z80 IC set interrupt mode 2 instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_im_2(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_im_2_t *im2;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_im_2_create(&im2);
	if (rc != EOK)
		return rc;

	*rinstr = &im2->instr;
	return EOK;
}

/** Parse Z80 IC set interrupt mode instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_im(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_tok_t itok;
	int32_t value;
	int rc;

	/* Skip 'im'. */
	z80ic_parser_skip(parser);

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_number) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " number expected.\n");
		return EINVAL;
	}

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	if (value < 0 || (uint32_t)value > 2) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is out of range of [0, 2].\n");
		return rc;
	}

	if (value == 0) {
		rc = z80ic_parser_process_im_0(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else if (value == 1) {
		rc = z80ic_parser_process_im_1(parser, rinstr);
		if (rc != EOK)
			return rc;
	} else {
		assert(value == 2);

		rc = z80ic_parser_process_im_2(parser, rinstr);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}


/** Parse Z80 IC return instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_ret(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_ret_t *ret;
	int rc;

	z80ic_parser_skip(parser);

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		return rc;

	*rinstr = &ret->instr;
	return EOK;
}

/** Parse Z80 IC instruction.
 *
 * @param parser Z80 IC parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_instr(z80ic_parser_t *parser,
    z80ic_instr_t **rinstr)
{
	z80ic_lexer_toktype_t ztt;
	int rc;

	/* Instruction keyword */

	ztt = z80ic_parser_next_ttype(parser);
	switch (ztt) {
	case ztt_ld:
		rc = z80ic_parser_process_ld(parser, rinstr);
		break;
	case ztt_push:
		rc = z80ic_parser_process_push(parser, rinstr);
		break;
	case ztt_pop:
		rc = z80ic_parser_process_pop(parser, rinstr);
		break;
	case ztt_ex:
		rc = z80ic_parser_process_ex(parser, rinstr);
		break;
	case ztt_exx:
		rc = z80ic_parser_process_exx(parser, rinstr);
		break;
	case ztt_ldi:
		rc = z80ic_parser_process_ldi(parser, rinstr);
		break;
	case ztt_ldir:
		rc = z80ic_parser_process_ldir(parser, rinstr);
		break;
	case ztt_ldd:
		rc = z80ic_parser_process_ldd(parser, rinstr);
		break;
	case ztt_lddr:
		rc = z80ic_parser_process_lddr(parser, rinstr);
		break;
	case ztt_cpi:
		rc = z80ic_parser_process_cpi(parser, rinstr);
		break;
	case ztt_cpir:
		rc = z80ic_parser_process_cpir(parser, rinstr);
		break;
	case ztt_cpd:
		rc = z80ic_parser_process_cpd(parser, rinstr);
		break;
	case ztt_cpdr:
		rc = z80ic_parser_process_cpdr(parser, rinstr);
		break;
	case ztt_add:
		rc = z80ic_parser_process_add(parser, rinstr);
		break;
	case ztt_adc:
		rc = z80ic_parser_process_adc(parser, rinstr);
		break;
	case ztt_sub:
		rc = z80ic_parser_process_sub(parser, rinstr);
		break;
	case ztt_sbc:
		rc = z80ic_parser_process_sbc(parser, rinstr);
		break;
	case ztt_and:
		rc = z80ic_parser_process_and(parser, rinstr);
		break;
	case ztt_or:
		rc = z80ic_parser_process_or(parser, rinstr);
		break;
	case ztt_xor:
		rc = z80ic_parser_process_xor(parser, rinstr);
		break;
	case ztt_cp:
		rc = z80ic_parser_process_cp(parser, rinstr);
		break;
	case ztt_inc:
		rc = z80ic_parser_process_inc(parser, rinstr);
		break;
	case ztt_dec:
		rc = z80ic_parser_process_dec(parser, rinstr);
		break;
	case ztt_daa:
		rc = z80ic_parser_process_daa(parser, rinstr);
		break;
	case ztt_cpl:
		rc = z80ic_parser_process_cpl(parser, rinstr);
		break;
	case ztt_neg:
		rc = z80ic_parser_process_neg(parser, rinstr);
		break;
	case ztt_ccf:
		rc = z80ic_parser_process_ccf(parser, rinstr);
		break;
	case ztt_scf:
		rc = z80ic_parser_process_scf(parser, rinstr);
		break;
	case ztt_nop:
		rc = z80ic_parser_process_nop(parser, rinstr);
		break;
	case ztt_halt:
		rc = z80ic_parser_process_halt(parser, rinstr);
		break;
	case ztt_di:
		rc = z80ic_parser_process_di(parser, rinstr);
		break;
	case ztt_ei:
		rc = z80ic_parser_process_ei(parser, rinstr);
		break;
	case ztt_im:
		rc = z80ic_parser_process_im(parser, rinstr);
		break;
	case ztt_ret:
		rc = z80ic_parser_process_ret(parser, rinstr);
		break;
	default:
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected instruction "
		    "keyword.\n");
		rc = EINVAL;
		break;
	}

	if (rc != EOK)
		return rc;

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Parse Z80 IC labeled block.
 *
 * @param parser Z80 IC parser
 * @param lblock Labeled block to which instructions should be appended
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_lblock(z80ic_parser_t *parser,
    z80ic_lblock_t *lblock)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_lexer_tok_t itok;
	z80ic_instr_t *instr;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	while (ztt != ztt_end) {
		if (ztt == ztt_ident) {
			/* Label */
			z80ic_parser_read_next_tok(parser, &itok);
			assert(itok.ttype == ztt_ident);

			rc = z80ic_lblock_append(lblock, itok.text, NULL);
			if (rc != EOK)
				goto error;

			z80ic_parser_skip(parser);

			rc = z80ic_parser_match(parser, ztt_colon);
			if (rc != EOK)
				goto error;
		} else {
			/* Instruction */
			rc = z80ic_parser_process_instr(parser, &instr);
			if (rc != EOK)
				goto error;

			rc = z80ic_lblock_append(lblock, NULL, instr);
			if (rc != EOK)
				goto error;
		}

		ztt = z80ic_parser_next_ttype(parser);
	}

	return EOK;
error:
	return rc;
}

/** Parse Z80 IC procedure declaration.
 *
 * @param parser Z80 IC parser
 * @param rproc Place to store pointer to new procedure
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_proc(z80ic_parser_t *parser,
    z80ic_proc_t **rproc)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_lexer_tok_t itok;
	z80ic_proc_t *proc = NULL;
	z80ic_lvar_t *lvar;
	char *ident = NULL;
	z80ic_lblock_t *lblock = NULL;
	int32_t offset;
	int rc;

	/* proc keyword */

	rc = z80ic_parser_match(parser, ztt_proc);
	if (rc != EOK)
		goto error;

	/* Identifier */

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_ident) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected identifier.\n");
		rc = EINVAL;
		goto error;
	}

	rc = z80ic_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = z80ic_proc_create(itok.text, lblock, &proc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	z80ic_parser_skip(parser);

	/* Lvar */

	ztt = z80ic_parser_next_ttype(parser);
	if (ztt == ztt_lvar) {
		z80ic_parser_skip(parser);
		ztt = z80ic_parser_next_ttype(parser);
		while (ztt == ztt_ident) {
			/* Identifier */

			z80ic_parser_read_next_tok(parser, &itok);
			/* itok.text is only valid until we skip the token */
			ident = strdup(itok.text);
			if (ident == NULL) {
				rc = ENOMEM;
				goto error;
			}

			z80ic_parser_skip(parser);

			/* ':' */

			rc = z80ic_parser_match(parser, ztt_colon);
			if (rc != EOK)
				goto error;

			ztt = z80ic_parser_next_ttype(parser);
			if (ztt != ztt_ident) {
				(void)fprintf(stderr, "Error: ");
				(void)z80ic_parser_dprint_next_tok(parser,
				    stderr);
				(void)fprintf(stderr, " numeric offset "
				    "expected.\n");
				rc = EINVAL;
				goto error;
			}

			/* Offset */
			rc = z80ic_lexer_number_val(&itok, &offset);
			if (rc != EOK) {
				(void)fprintf(stderr, "Error: ");
				(void)z80ic_parser_dprint_next_tok(parser,
				    stderr);
				(void)fprintf(stderr, " is not a valid "
				    "number.\n");
				goto error;
			}

			if (offset < 0 || (uint32_t)offset > 0xffffu) {
				(void)fprintf(stderr, "Error: ");
				(void)z80ic_parser_dprint_next_tok(parser,
				    stderr);
				(void)fprintf(stderr, " is out of range of "
				    "[0, 65535].\n");
				return rc;
			}

			z80ic_parser_skip(parser);

			rc = z80ic_lvar_create(ident, (uint16_t)offset, &lvar);
			if (rc != EOK)
				goto error;

			free(ident);
			ident = NULL;
			z80ic_proc_append_lvar(proc, lvar);

			rc = z80ic_parser_match(parser, ztt_scolon);
			if (rc != EOK)
				goto error;

			ztt = z80ic_parser_next_ttype(parser);
		}
	}

	/* Begin, end */

	rc = z80ic_parser_match(parser, ztt_begin);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_process_lblock(parser, proc->lblock);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_end);
	if (rc != EOK)
		goto error;

	*rproc = proc;
	return EOK;
error:
	if (proc != NULL)
		z80ic_proc_destroy(proc);
	if (lblock != NULL)
		z80ic_lblock_destroy(lblock);
	if (ident != NULL)
		free(ident);
	return rc;
}

/** Parse Z80 IC DEFB data entry.
 *
 * @param parser Z80 IC parser
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dentry_defb(z80ic_parser_t *parser,
    z80ic_dentry_t **rdentry)
{
	z80ic_lexer_tok_t itok;
	z80ic_dentry_t *dentry = NULL;
	int32_t value;
	int rc;

	/* defb keyword */

	rc = z80ic_parser_match(parser, ztt_defb);
	if (rc != EOK)
		goto error;

	/* Value */

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_number) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected number.\n");
		rc = EINVAL;
		goto error;
	}

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		rc = EINVAL;
		goto error;
	}

	if (value < 0 || (uint32_t)value > 0xff) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is out of range of [0, 255].\n");
		return rc;
	}

	z80ic_parser_skip(parser);

	/* ';' */

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		goto error;

	rc = z80ic_dentry_create_defb((uint8_t)value, &dentry);
	if (rc != EOK)
		goto error;

	*rdentry = dentry;
	return EOK;
error:
	if (dentry != NULL)
		z80ic_dentry_destroy(dentry);
	return rc;
}

/** Parse Z80 IC DEFW data entry.
 *
 * @param parser Z80 IC parser
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dentry_defw(z80ic_parser_t *parser,
    z80ic_dentry_t **rdentry)
{
	z80ic_lexer_tok_t itok;
	z80ic_dentry_t *dentry = NULL;
	int32_t value;
	int rc;

	/* defw keyword */

	rc = z80ic_parser_match(parser, ztt_defw);
	if (rc != EOK)
		goto error;

	/* Value */

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_number) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected number.\n");
		rc = EINVAL;
		goto error;
	}

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		rc = EINVAL;
		goto error;
	}

	if (value < 0 || (uint32_t)value > 0xffffu) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is out of range of [0, 65535].\n");
		return rc;
	}

	z80ic_parser_skip(parser);

	/* ';' */

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		goto error;

	rc = z80ic_dentry_create_defw((uint16_t)value, &dentry);
	if (rc != EOK)
		goto error;

	*rdentry = dentry;
	return EOK;
error:
	if (dentry != NULL)
		z80ic_dentry_destroy(dentry);
	return rc;
}

/** Parse Z80 IC DEFDW data entry.
 *
 * @param parser Z80 IC parser
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dentry_defdw(z80ic_parser_t *parser,
    z80ic_dentry_t **rdentry)
{
	z80ic_lexer_tok_t itok;
	z80ic_dentry_t *dentry = NULL;
	int32_t value;
	int rc;

	/* defdw keyword */

	rc = z80ic_parser_match(parser, ztt_defdw);
	if (rc != EOK)
		goto error;

	/* Value */

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_number) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected number.\n");
		rc = EINVAL;
		goto error;
	}

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		rc = EINVAL;
		goto error;
	}

	z80ic_parser_skip(parser);

	/* ';' */

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		goto error;

	rc = z80ic_dentry_create_defdw((uint32_t)value, &dentry);
	if (rc != EOK)
		goto error;

	*rdentry = dentry;
	return EOK;
error:
	if (dentry != NULL)
		z80ic_dentry_destroy(dentry);
	return rc;
}

/** Parse Z80 IC DEFQW data entry.
 *
 * @param parser Z80 IC parser
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dentry_defqw(z80ic_parser_t *parser,
    z80ic_dentry_t **rdentry)
{
	z80ic_lexer_tok_t itok;
	z80ic_dentry_t *dentry = NULL;
	int32_t value;
	int rc;

	/* defqw keyword */

	rc = z80ic_parser_match(parser, ztt_defqw);
	if (rc != EOK)
		goto error;

	/* Value */

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_number) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected number.\n");
		rc = EINVAL;
		goto error;
	}

	rc = z80ic_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " is not a valid number.\n");
		rc = EINVAL;
		goto error;
	}

	z80ic_parser_skip(parser);

	/* ';' */

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		goto error;

	rc = z80ic_dentry_create_defqw(value, &dentry);
	if (rc != EOK)
		goto error;

	*rdentry = dentry;
	return EOK;
error:
	if (dentry != NULL)
		z80ic_dentry_destroy(dentry);
	return rc;
}

/** Parse Z80 IC data entry.
 *
 * @param parser Z80 IC parser
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dentry(z80ic_parser_t *parser,
    z80ic_dentry_t **rdentry)
{
	z80ic_lexer_tok_t itok;
	int rc;

	/* Which keyword / data entry type */

	z80ic_parser_read_next_tok(parser, &itok);
	switch (itok.ttype) {
	case ztt_defb:
		rc = z80ic_parser_process_dentry_defb(parser, rdentry);
		break;
	case ztt_defw:
		rc = z80ic_parser_process_dentry_defw(parser, rdentry);
		break;
	case ztt_defdw:
		rc = z80ic_parser_process_dentry_defdw(parser, rdentry);
		break;
	case ztt_defqw:
		rc = z80ic_parser_process_dentry_defqw(parser, rdentry);
		break;
	default:
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpeced, expected 'int' or 'ptr'.\n");
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Parse Z80 IC data block.
 *
 * @param parser Z80 IC parser
 * @param rdblock Place to store pointer to new data block
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_dblock(z80ic_parser_t *parser,
    z80ic_dblock_t **rdblock)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_dblock_t *dblock = NULL;
	z80ic_dentry_t *dentry;
	int rc;

	rc = z80ic_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	ztt = z80ic_parser_next_ttype(parser);
	while (ztt != ztt_end) {
		rc = z80ic_parser_process_dentry(parser, &dentry);
		if (rc != EOK)
			goto error;

		rc = z80ic_dblock_append(dblock, dentry);
		if (rc != EOK)
			goto error;

		ztt = z80ic_parser_next_ttype(parser);
	}

	*rdblock = dblock;
	return EOK;
error:
	z80ic_dblock_destroy(dblock);
	return rc;
}

/** Parse Z80 IC variable declaration.
 *
 * @param parser Z80 IC parser
 * @param rvar Place to store pointer to new variable
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_var(z80ic_parser_t *parser, z80ic_var_t **rvar)
{
	z80ic_lexer_tok_t itok;
	z80ic_var_t *var = NULL;
	z80ic_dblock_t *dblock = NULL;
	char *ident = NULL;
	int rc;

	/* var keyword */

	rc = z80ic_parser_match(parser, ztt_var);
	if (rc != EOK)
		goto error;

	/* Identifier */

	z80ic_parser_read_next_tok(parser, &itok);
	if (itok.ttype != ztt_ident) {
		(void)fprintf(stderr, "Error: ");
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, " unexpected, expected identifier.\n");
		rc = EINVAL;
		goto error;
	}

	ident = strdup(itok.text);
	z80ic_parser_skip(parser);

	/* Begin, end */

	rc = z80ic_parser_match(parser, ztt_begin);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_process_dblock(parser, &dblock);
	if (rc != EOK)
		goto error;

	rc = z80ic_parser_match(parser, ztt_end);
	if (rc != EOK)
		goto error;

	rc = z80ic_var_create(ident, dblock, &var);
	if (rc != EOK)
		goto error;

	*rvar = var;
	free(ident);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	if (var != NULL)
		z80ic_var_destroy(var);
	if (dblock != NULL)
		z80ic_dblock_destroy(dblock);
	return rc;
}

/** Parse Z80 IC declaration.
 *
 * @param parser Z80 IC parser
 * @param rdecln Place to store pointer to new declaration
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_parser_process_decln(z80ic_parser_t *parser,
    z80ic_decln_t **rdecln)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_proc_t *proc;
	z80ic_var_t *var;
	z80ic_decln_t *decln;
	int rc;

	ztt = z80ic_parser_next_ttype(parser);
	switch (ztt) {
	case ztt_proc:
		rc = z80ic_parser_process_proc(parser, &proc);
		if (rc != EOK)
			goto error;
		decln = &proc->decln;
		break;
	case ztt_var:
		rc = z80ic_parser_process_var(parser, &var);
		if (rc != EOK)
			goto error;
		decln = &var->decln;
		break;
	default:
		(void)z80ic_parser_dprint_next_tok(parser, stderr);
		(void)fprintf(stderr, ": Declaration expected.\n");
		rc = EINVAL;
		goto error;
	}

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		goto error;

	*rdecln = decln;
	return EOK;
error:
	return rc;
}

/** Parse Z80 IC module.
 *
 * @param parser Z80 IC parser
 * @param rmodule Place to store pointer to new module
 *
 * @return EOK on success or non-zero error code
 */
int z80ic_parser_process_module(z80ic_parser_t *parser,
    z80ic_module_t **rmodule)
{
	z80ic_lexer_toktype_t ztt;
	z80ic_module_t *module;
	z80ic_decln_t *decln;
	int rc;

	rc = z80ic_module_create(&module);
	if (rc != EOK)
		return rc;

	ztt = z80ic_parser_next_ttype(parser);
	while (ztt != ztt_eof) {
		rc = z80ic_parser_process_decln(parser, &decln);
		if (rc != EOK)
			goto error;

		z80ic_module_append(module, decln);
		ztt = z80ic_parser_next_ttype(parser);
	}

	*rmodule = module;
	return EOK;
error:
	z80ic_module_destroy(module);
	return rc;
}
