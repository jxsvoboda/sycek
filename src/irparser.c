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
 * IR parser
 */

#include <assert.h>
#include <ir.h>
#include <irparser.h>
#include <irlexer.h>
#include <merrno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ir_parser_process_oper(ir_parser_t *, ir_oper_t **);

/** Create IR parser.
 *
 * @param ops Parser input ops
 * @param arg Argument to input ops
 * @param rparser Place to store pointer to new parser
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_parser_create(ir_parser_input_ops_t *ops, void *arg,
    ir_parser_t **rparser)
{
	ir_parser_t *parser;

	parser = calloc(1, sizeof(ir_parser_t));
	if (parser == NULL)
		return ENOMEM;

	parser->input_ops = ops;
	parser->input_arg = arg;

	*rparser = parser;
	return EOK;
}

/** Destroy IR parser.
 *
 * @param parser IR parser
 */
void ir_parser_destroy(ir_parser_t *parser)
{
	if (parser != NULL)
		free(parser);
}

/** Return @c true if token type is to be ignored when parsing.
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is ignored during parsing
 */
bool ir_parser_ttype_ignore(ir_lexer_toktype_t ttype)
{
	return ttype == itt_space || ttype == itt_tab ||
	    ttype == itt_newline || ttype == itt_comment ||
	    ttype == itt_invchar;
}

/** Return valid input token skipping tokens that should be ignored.
 *
 * At the same time we read the token contents into the provided buffer @a rtok
 *
 * @param parser IR parser
 * @param rtok Place to store next lexer token
 */
static void ir_parser_next_input_tok(ir_parser_t *parser, ir_lexer_tok_t *rtok)
{
	parser->input_ops->read_tok(parser->input_arg, rtok);
	while (ir_parser_ttype_ignore(rtok->ttype)) {
		parser->input_ops->next_tok(parser->input_arg);
		parser->input_ops->read_tok(parser->input_arg, rtok);
	}
}

/** Return type of next token.
 *
 * @param parser IR parser
 * @return Type of next token being parsed
 */
static ir_lexer_toktype_t ir_parser_next_ttype(ir_parser_t *parser)
{
	ir_lexer_tok_t ltok;

	ir_parser_next_input_tok(parser, &ltok);
	return ltok.ttype;
}

/** Read next token.
 *
 * @param parser IR parser
 * @param tok Place to store token
 */
static void ir_parser_read_next_tok(ir_parser_t *parser, ir_lexer_tok_t *tok)
{
	ir_parser_next_input_tok(parser, tok);
}

static int ir_parser_dprint_next_tok(ir_parser_t *parser, FILE *f)
{
	ir_lexer_tok_t tok;

	ir_parser_read_next_tok(parser, &tok);
	return ir_lexer_dprint_tok(&tok, f);
}

/** Skip over current token.
 *
 * @param parser IR parser
 */
static void ir_parser_skip(ir_parser_t *parser)
{
	ir_lexer_tok_t tok;

	/* Find non-ignored token */
	ir_parser_next_input_tok(parser, &tok);
	ir_lexer_free_tok(&tok);

	/* Skip over */
	parser->input_ops->next_tok(parser->input_arg);
}

/** Match a particular token type.
 *
 * If the type of the next token is @a mtype, skip over it. Otherwise
 * generate an error.
 *
 * @param parser IR parser
 * @param mtype Expected token type
 *
 * @return EOK on sucecss, EINVAL if token does not have expected type
 */
static int ir_parser_match(ir_parser_t *parser, ir_lexer_toktype_t mtype)
{
	ir_lexer_toktype_t itt;

	itt = ir_parser_next_ttype(parser);
	if (itt != mtype) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected %s.\n",
		    ir_lexer_str_ttype(mtype));
		return EINVAL;
	}

	ir_parser_skip(parser);
	return EOK;
}

/** Parse IR variable operand.
 *
 * @param parser IR parser
 * @param rvar Place to store pointer to new variable operand
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_oper_var(ir_parser_t *parser, ir_oper_var_t **rvar)
{
	ir_lexer_tok_t itok;
	ir_oper_var_t *var = NULL;
	int rc;

	ir_parser_read_next_tok(parser, &itok);
	assert(itok.ttype == itt_ident);

	rc = ir_oper_var_create(itok.text, &var);
	if (rc != EOK)
		return rc;

	ir_parser_skip(parser);

	*rvar = var;
	return EOK;
}

/** Parse IR list operand.
 *
 * @param parser IR parser
 * @param rlist Place to store pointer to new list operand
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_oper_list(ir_parser_t *parser,
    ir_oper_list_t **rlist)
{
	ir_lexer_toktype_t itt;
	ir_lexer_tok_t itok;
	ir_oper_list_t *list = NULL;
	ir_oper_t *oper;
	bool first;
	int rc;

	ir_parser_read_next_tok(parser, &itok);
	assert(itok.ttype == itt_lbrace);
	ir_parser_skip(parser);

	rc = ir_oper_list_create(&list);
	if (rc != EOK)
		return rc;

	itt = ir_parser_next_ttype(parser);
	first = true;
	while (itt != itt_rbrace) {
		if (!first) {
			rc = ir_parser_match(parser, itt_comma);
			if (rc != EOK) {
				rc = EINVAL;
				goto error;
			}
		}

		rc = ir_parser_process_oper(parser, &oper);
		if (rc != EOK)
			goto error;

		ir_oper_list_append(list, oper);

		first = false;
		itt = ir_parser_next_ttype(parser);
	}

	ir_parser_skip(parser);

	*rlist = list;
	return EOK;
error:
	ir_oper_destroy(&list->oper);
	return rc;
}

/** Parse IR nil operand.
 *
 * @param parser IR parser
 * @param roper Place to store @c NULL
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_oper_nil(ir_parser_t *parser, ir_oper_t **roper)
{
	int rc;

	rc = ir_parser_match(parser, itt_nil);
	if (rc != EOK)
		return EINVAL;

	*roper = NULL;
	return EOK;
}

/** Parse IR immediate operand.
 *
 * @param parser IR parser
 * @param rimm Place to store pointer to new immediate operand
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_oper_imm(ir_parser_t *parser, ir_oper_imm_t **rimm)
{
	ir_lexer_tok_t itok;
	ir_oper_imm_t *imm = NULL;
	int32_t value;
	int rc;

	ir_parser_read_next_tok(parser, &itok);
	assert(itok.ttype == itt_number);

	rc = ir_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	rc = ir_oper_imm_create(value, &imm);
	if (rc != EOK)
		return rc;

	ir_parser_skip(parser);

	*rimm = imm;
	return EOK;
}

/** Parse IR operand.
 *
 * @param parser IR parser
 * @param roper Place to store pointer to new operand
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_oper(ir_parser_t *parser, ir_oper_t **roper)
{
	ir_lexer_toktype_t itt;
	ir_oper_var_t *var;
	ir_oper_list_t *list;
	ir_oper_imm_t *imm;
	int rc;

	itt = ir_parser_next_ttype(parser);
	switch (itt) {
	case itt_ident:
		rc = ir_parser_process_oper_var(parser, &var);
		if (rc != EOK)
			return rc;
		*roper = &var->oper;
		break;
	case itt_lbrace:
		rc = ir_parser_process_oper_list(parser, &list);
		if (rc != EOK)
			return rc;
		*roper = &list->oper;
		break;
	case itt_nil:
		rc = ir_parser_process_oper_nil(parser, roper);
		if (rc != EOK)
			return rc;
		/* *roper is now set to NULL */
		break;
	case itt_number:
		rc = ir_parser_process_oper_imm(parser, &imm);
		if (rc != EOK)
			return rc;
		*roper = &imm->oper;
		break;
	default:
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected operand.\n");
		return EINVAL;
	}

	return rc;
}

/** Parse IR instruction.
 *
 * @param parser IR parser
 * @param rinstr Place to store pointer to new instruction
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_instr(ir_parser_t *parser, ir_instr_t **rinstr)
{
	ir_lexer_toktype_t itt;
	ir_lexer_tok_t itok;
	ir_instr_t *instr = NULL;
	int32_t width;
	int rc;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	/* Instruction keyword */

	itt = ir_parser_next_ttype(parser);
	switch (itt) {
	case itt_add:
		instr->itype = iri_add;
		break;
	case itt_and:
		instr->itype = iri_and;
		break;
	case itt_bnot:
		instr->itype = iri_bnot;
		break;
	case itt_call:
		instr->itype = iri_call;
		break;
	case itt_eq:
		instr->itype = iri_eq;
		break;
	case itt_gt:
		instr->itype = iri_gt;
		break;
	case itt_gteq:
		instr->itype = iri_gteq;
		break;
	case itt_imm:
		instr->itype = iri_imm;
		break;
	case itt_jmp:
		instr->itype = iri_jmp;
		break;
	case itt_jnz:
		instr->itype = iri_jnz;
		break;
	case itt_jz:
		instr->itype = iri_jz;
		break;
	case itt_lt:
		instr->itype = iri_lt;
		break;
	case itt_lteq:
		instr->itype = iri_lteq;
		break;
	case itt_lvarptr:
		instr->itype = iri_lvarptr;
		break;
	case itt_mul:
		instr->itype = iri_mul;
		break;
	case itt_neg:
		instr->itype = iri_neg;
		break;
	case itt_neq:
		instr->itype = iri_neq;
		break;
	case itt_nop:
		instr->itype = iri_nop;
		break;
	case itt_or:
		instr->itype = iri_or;
		break;
	case itt_read:
		instr->itype = iri_read;
		break;
	case itt_retv:
		instr->itype = iri_retv;
		break;
	case itt_shl:
		instr->itype = iri_shl;
		break;
	case itt_shr:
		instr->itype = iri_shr;
		break;
	case itt_sub:
		instr->itype = iri_sub;
		break;
	case itt_varptr:
		instr->itype = iri_varptr;
		break;
	case itt_write:
		instr->itype = iri_write;
		break;
	case itt_xor:
		instr->itype = iri_xor;
		break;
	default:
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected instruction keyword.\n");
		rc = EINVAL;
		goto error;
	}

	ir_parser_skip(parser);

	/* '.' width */

	itt = ir_parser_next_ttype(parser);
	if (itt == itt_period) {
		ir_parser_skip(parser);

		ir_parser_read_next_tok(parser, &itok);
		if (itok.ttype != itt_number) {
			fprintf(stderr, "Error: ");
			ir_parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " unexpected, expected number.\n");
			rc = EINVAL;
			goto error;
		}

		rc = ir_lexer_number_val(&itok, &width);
		if (rc != EOK) {
			fprintf(stderr, "Error: ");
			ir_parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " is not a valid number.\n");
			rc = EINVAL;
			goto error;
		}

		ir_parser_skip(parser);
	} else {
		/* No width */
		width = 0;
	}

	instr->width = width;

	/* Destination */

	rc = ir_parser_process_oper(parser, &instr->dest);
	if (rc != EOK)
		goto error;

	/* Operand 1 (optional) */

	itt = ir_parser_next_ttype(parser);
	if (itt == itt_comma) {
		ir_parser_skip(parser);
		rc = ir_parser_process_oper(parser, &instr->op1);
		if (rc != EOK)
			goto error;
	}

	/* Operand 2 (optional) */

	itt = ir_parser_next_ttype(parser);
	if (itt == itt_comma) {
		ir_parser_skip(parser);
		rc = ir_parser_process_oper(parser, &instr->op2);
		if (rc != EOK)
			goto error;
	}

	/* ';' */

	rc = ir_parser_match(parser, itt_scolon);
	if (rc != EOK)
		goto error;

	*rinstr = instr;
	return EOK;
error:
	if (instr != NULL)
		ir_instr_destroy(instr);
	return rc;
}

/** Parse IR labeled block.
 *
 * @param parser IR parser
 * @param lblock Labeled block to which instructions should be appended
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_lblock(ir_parser_t *parser, ir_lblock_t *lblock)
{
	ir_lexer_toktype_t itt;
	ir_lexer_tok_t itok;
	ir_instr_t *instr;
	int rc;

	itt = ir_parser_next_ttype(parser);
	while (itt != itt_end) {
		if (itt == itt_ident) {
			/* Label */
			ir_parser_read_next_tok(parser, &itok);
			assert(itok.ttype == itt_ident);

			ir_lblock_append(lblock, itok.text, NULL);
			ir_parser_skip(parser);

			rc = ir_parser_match(parser, itt_colon);
			if (rc != EOK)
				goto error;
		} else {
			/* Instruction */
			rc = ir_parser_process_instr(parser, &instr);
			if (rc != EOK)
				goto error;

			ir_lblock_append(lblock, NULL, instr);
		}

		itt = ir_parser_next_ttype(parser);
	}

	return EOK;
error:
	return rc;
}

/** Parse IR procedure declaration.
 *
 * @param parser IR parser
 * @param rproc Place to store pointer to new procedure
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_proc(ir_parser_t *parser, ir_proc_t **rproc)
{
	ir_lexer_toktype_t itt;
	ir_lexer_tok_t itok;
	ir_proc_t *proc = NULL;
	ir_proc_arg_t *arg;
	ir_lvar_t *lvar;
	ir_lblock_t *lblock = NULL;
	bool first;
	int rc;

	/* proc keyword */

	rc = ir_parser_match(parser, itt_proc);
	if (rc != EOK)
		goto error;

	/* Identifier */

	ir_parser_read_next_tok(parser, &itok);
	if (itok.ttype != itt_ident) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected indentifier.\n");
		rc = EINVAL;
		goto error;
	}

	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create(itok.text, 0, lblock, &proc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	ir_parser_skip(parser);

	/* Parentheses */

	rc = ir_parser_match(parser, itt_lparen);
	if (rc != EOK)
		goto error;

	itt = ir_parser_next_ttype(parser);
	first = true;
	while (itt != itt_rparen) {
		if (!first) {
			rc = ir_parser_match(parser, itt_comma);
			if (rc != EOK)
				goto error;
		}

		ir_parser_read_next_tok(parser, &itok);
		if (itok.ttype != itt_ident) {
			fprintf(stderr, "Error: ");
			ir_parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " unexpected, expected indentifier.\n");
			rc = EINVAL;
			goto error;
		}

		rc = ir_proc_arg_create(itok.text, &arg);
		if (rc != EOK)
			goto error;

		ir_proc_append_arg(proc, arg);

		ir_parser_skip(parser);

		first = false;
		itt = ir_parser_next_ttype(parser);
	}

	rc = ir_parser_match(parser, itt_rparen);
	if (rc != EOK)
		goto error;

	/* Extern */

	itt = ir_parser_next_ttype(parser);
	if (itt == itt_extern) {
		ir_parser_skip(parser);
		proc->flags |= irp_extern;
		ir_lblock_destroy(proc->lblock);
		proc->lblock = NULL;
	}

	/* Lvar */

	itt = ir_parser_next_ttype(parser);
	if (itt == itt_lvar) {
		ir_parser_skip(parser);
		itt = ir_parser_next_ttype(parser);
		while (itt == itt_ident) {
			ir_parser_read_next_tok(parser, &itok);
			rc = ir_lvar_create(itok.text, &lvar);
			if (rc != EOK)
				goto error;

			ir_proc_append_lvar(proc, lvar);

			ir_parser_skip(parser);
			rc = ir_parser_match(parser, itt_scolon);
			if (rc != EOK)
				goto error;

			itt = ir_parser_next_ttype(parser);
		}
	}

	/* Begin, end */

	if ((proc->flags & irp_extern) == 0) {
		rc = ir_parser_match(parser, itt_begin);
		if (rc != EOK)
			goto error;

		rc = ir_parser_process_lblock(parser, proc->lblock);
		if (rc != EOK)
			goto error;

		rc = ir_parser_match(parser, itt_end);
		if (rc != EOK)
			goto error;
	}

	*rproc = proc;
	return EOK;
error:
	if (proc != NULL)
		ir_proc_destroy(proc);
	if (lblock != NULL)
		ir_lblock_destroy(lblock);
	return rc;
}

/** Parse IR data entry.
 *
 * @param parser IR parser
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_dentry(ir_parser_t *parser, ir_dentry_t **rdentry)
{
	ir_lexer_tok_t itok;
	ir_dentry_t *dentry = NULL;
	int32_t width;
	int32_t value;
	int rc;

	/* int keyword */

	rc = ir_parser_match(parser, itt_int);
	if (rc != EOK)
		goto error;

	/* '.' */

	rc = ir_parser_match(parser, itt_period);
	if (rc != EOK)
		goto error;

	/* Width */

	ir_parser_read_next_tok(parser, &itok);
	if (itok.ttype != itt_number) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected number.\n");
		rc = EINVAL;
		goto error;
	}

	rc = ir_lexer_number_val(&itok, &width);
	if (rc != EOK) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " is not a valid number.\n");
		rc = EINVAL;
		goto error;
	}

	ir_parser_skip(parser);

	/* Value */

	ir_parser_read_next_tok(parser, &itok);
	if (itok.ttype != itt_number) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected number.\n");
		rc = EINVAL;
		goto error;
	}

	rc = ir_lexer_number_val(&itok, &value);
	if (rc != EOK) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " is not a valid number.\n");
		rc = EINVAL;
		goto error;
	}

	ir_parser_skip(parser);

	/* ';' */

	rc = ir_parser_match(parser, itt_scolon);
	if (rc != EOK)
		goto error;

	rc = ir_dentry_create_int(width, value, &dentry);
	if (rc != EOK)
		goto error;

	*rdentry = dentry;
	return EOK;
error:
	if (dentry != NULL)
		ir_dentry_destroy(dentry);
	return rc;
}

/** Parse IR data block.
 *
 * @param parser IR parser
 * @param rdblock Place to store pointer to new data block
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_dblock(ir_parser_t *parser, ir_dblock_t **rdblock)
{
	ir_lexer_toktype_t itt;
	ir_dblock_t *dblock = NULL;
	ir_dentry_t *dentry;
	int rc;

	rc = ir_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	itt = ir_parser_next_ttype(parser);
	while (itt != itt_end) {
		rc = ir_parser_process_dentry(parser, &dentry);
		if (rc != EOK)
			goto error;

		ir_dblock_append(dblock, dentry);
		itt = ir_parser_next_ttype(parser);
	}

	*rdblock = dblock;
	return EOK;
error:
	ir_dblock_destroy(dblock);
	return rc;
}

/** Parse IR variable declaration.
 *
 * @param parser IR parser
 * @param rvar Place to store pointer to new variable
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_var(ir_parser_t *parser, ir_var_t **rvar)
{
	ir_lexer_tok_t itok;
	ir_var_t *var = NULL;
	ir_dblock_t *dblock = NULL;
	char *ident = NULL;
	int rc;

	/* var keyword */

	rc = ir_parser_match(parser, itt_var);
	if (rc != EOK)
		goto error;

	/* Identifier */

	ir_parser_read_next_tok(parser, &itok);
	if (itok.ttype != itt_ident) {
		fprintf(stderr, "Error: ");
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected indentifier.\n");
		rc = EINVAL;
		goto error;
	}

	ident = strdup(itok.text);
	ir_parser_skip(parser);

	/* Begin, end */

	rc = ir_parser_match(parser, itt_begin);
	if (rc != EOK)
		goto error;

	rc = ir_parser_process_dblock(parser, &dblock);
	if (rc != EOK)
		goto error;

	rc = ir_parser_match(parser, itt_end);
	if (rc != EOK)
		goto error;

	rc = ir_var_create(ident, dblock, &var);
	if (rc != EOK)
		goto error;

	*rvar = var;
	free(ident);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	if (var != NULL)
		ir_var_destroy(var);
	if (dblock != NULL)
		ir_dblock_destroy(dblock);
	return rc;
}

/** Parse IR declaration.
 *
 * @param parser IR parser
 * @param rdecln Place to store pointer to new declaration
 *
 * @return EOK on success or non-zero error code
 */
static int ir_parser_process_decln(ir_parser_t *parser, ir_decln_t **rdecln)
{
	ir_lexer_toktype_t itt;
	ir_proc_t *proc;
	ir_var_t *var;
	ir_decln_t *decln;
	int rc;

	itt = ir_parser_next_ttype(parser);
	switch (itt) {
	case itt_proc:
		rc = ir_parser_process_proc(parser, &proc);
		if (rc != EOK)
			goto error;
		decln = &proc->decln;
		break;
	case itt_var:
		rc = ir_parser_process_var(parser, &var);
		if (rc != EOK)
			goto error;
		decln = &var->decln;
		break;
	default:
		ir_parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, ": Declaration expected.\n");
		rc = EINVAL;
		goto error;
	}

	rc = ir_parser_match(parser, itt_scolon);
	if (rc != EOK)
		goto error;

	*rdecln = decln;
	return EOK;
error:
	return rc;
}

/** Parse IR module.
 *
 * @param parser IR parser
 * @param rmodule Place to store pointer to new module
 *
 * @return EOK on success or non-zero error code
 */
int ir_parser_process_module(ir_parser_t *parser, ir_module_t **rmodule)
{
	ir_lexer_toktype_t itt;
	ir_module_t *module;
	ir_decln_t *decln;
	int rc;

	rc = ir_module_create(&module);
	if (rc != EOK)
		return rc;

	itt = ir_parser_next_ttype(parser);
	while (itt != itt_eof) {
		rc = ir_parser_process_decln(parser, &decln);
		if (rc != EOK)
			goto error;

		ir_module_append(module, decln);
		itt = ir_parser_next_ttype(parser);
	}

	*rmodule = module;
	return EOK;
error:
	ir_module_destroy(module);
	return rc;
}
