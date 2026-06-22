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

#if 0
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

	rc = z80ic_oper_imm8_create(value, &imm);
	if (rc != EOK)
		return rc;

	z80ic_parser_skip(parser);

	*rimm = imm;
	return EOK;
}
#endif

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

			z80ic_parser_skip(parser);

			rc = z80ic_lvar_create(ident, offset, &lvar);
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

	z80ic_parser_skip(parser);

	/* ';' */

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		goto error;

	rc = z80ic_dentry_create_defb(value, &dentry);
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

	z80ic_parser_skip(parser);

	/* ';' */

	rc = z80ic_parser_match(parser, ztt_scolon);
	if (rc != EOK)
		goto error;

	rc = z80ic_dentry_create_defw(value, &dentry);
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

	rc = z80ic_dentry_create_defdw(value, &dentry);
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
