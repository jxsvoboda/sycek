/*
 * Copyright 2019 Jiri Svoboda
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
 * Code generator
 *
 * Generate IR (machine-independent assembly) from abstract syntax tree (AST).
 */

#include <assert.h>
#include <ast.h>
#include <comp.h>
#include <cgen.h>
#include <ir.h>
#include <lexer.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>

static int cgen_expr(cgen_proc_t *, ast_node_t *, ir_lblock_t *,
    const char **);

/** Prefix identifier with '@' global variable prefix.
 *
 * @param ident Identifier
 * @param rpident Place to store pointer to new prefixed identifier
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_gprefix(const char *ident, char **rpident)
{
	size_t len;
	char *pident;

	len = strlen(ident);
	pident = malloc(len + 2);
	if (pident == NULL)
		return ENOMEM;

	pident[0] = '@';
	strncpy(pident + 1, ident, len + 1);

	*rpident = pident;
	return EOK;
}

/** Get value of integer literal token.
 *
 * @param tlit Literal token
 * @param rval Place to store value
 * @return EOK on success, EINVAL if token format is invalid
 */
static int cgen_intlit_val(comp_tok_t *tlit, int32_t *rval)
{
	const char *text = tlit->tok.text;
	int32_t val;

	val = 0;
	while (*text != '\0') {
		if (*text < '0' || *text > '9')
			return EINVAL;

		val = val * 10 + (*text - '0');
		++text;
	}

	*rval = val;
	return EOK;
}

/** Create new numbered local variable operand.
 *
 * @param cgproc Code generator for procedure
 * @param roper Place to store pointer to new variable operand
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_create_new_lvar_oper(cgen_proc_t *cgproc,
    ir_oper_var_t **roper)
{
	int rc;
	int rv;
	ir_oper_var_t *oper = NULL;
	unsigned var;
	char *svar = NULL;

	var = cgproc->next_var++;

	rv = asprintf(&svar, "%%%u", var);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	rc = ir_oper_var_create(svar, &oper);
	if (rc != EOK)
		goto error;

	*roper = oper;
	return EOK;
error:
	if (svar != NULL)
		free(svar);
	return rc;
}

/** Create code generator.
 *
 * @param rcgen Place to store pointer to new code generator
 * @return EOK on success, ENOMEM if out of memory
 */
int cgen_create(cgen_t **rcgen)
{
	cgen_t *cgen;

	cgen = calloc(1, sizeof(cgen_t));
	if (cgen == NULL)
		return ENOMEM;

	cgen->error = false;
	*rcgen = cgen;
	return EOK;
}

/** Create code generator.
 *
 * @param rcgen Place to store pointer to new code generator
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_proc_create(cgen_t *cgen, cgen_proc_t **rcgproc)
{
	cgen_proc_t *cgproc;

	cgproc = calloc(1, sizeof(cgen_proc_t));
	if (cgproc == NULL)
		return ENOMEM;

	cgproc->cgen = cgen;
	cgproc->next_var = 0;
	*rcgproc = cgproc;
	return EOK;
}

/** Destroy code generator for procedure.
 *
 * @param cgen Code generator or @c NULL
 */
static void cgen_proc_destroy(cgen_proc_t *cgproc)
{
	if (cgproc == NULL)
		return;

	free(cgproc);
}

/** Generate code for integer literal expression.
 *
 * @param cgproc Code generator for procedure
 * @param eint AST integer literal expression
 * @param lblock IR labeled block to which the code should be appended
 * @param dname Place to store pointer to destination variable name
 * @return EOK on success or an error code
 */
static int cgen_eint(cgen_proc_t *cgproc, ast_eint_t *eint,
    ir_lblock_t *lblock, const char **dname)
{
	comp_tok_t *lit;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_imm_t *imm = NULL;
	int32_t val;
	int rc;

	lit = (comp_tok_t *) eint->tlit.data;
	rc = cgen_intlit_val(lit, &val);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(val, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_ldimm;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	*dname = dest->varname;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	return rc;
}

/** Generate code for binary operator expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression
 * @param lblock IR labeled block to which the code should be appended
 * @param dname Place to store pointer to destination variable name
 * @return EOK on success or an error code
 */
static int cgen_ebinop(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, const char **dname)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	const char *lvar = NULL;
	const char *rvar = NULL;
	int rc;

	rc = cgen_expr(cgproc, ebinop->larg, lblock, &lvar);
	if (rc != EOK)
		goto error;

	rc = cgen_expr(cgproc, ebinop->rarg, lblock, &rvar);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lvar, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rvar, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_add;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);
	*dname = dest->varname;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	return rc;
}


/** Generate code for expression.
 *
 * @param cgproc Code generator for procedure
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param dname Place to store pointer to destination variable name
 * @return EOK on success or an error code
 */
static int cgen_expr(cgen_proc_t *cgproc, ast_node_t *expr,
    ir_lblock_t *lblock, const char **dname)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	(void) lblock;

	switch (expr->ntype) {
	case ant_eint:
		rc = cgen_eint(cgproc, (ast_eint_t *) expr->ext, lblock, dname);
		break;
	case ant_echar:
	case ant_estring:
	case ant_eident:
	case ant_eparen:
	case ant_econcat:
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EOK;
		break;
	case ant_ebinop:
		rc = cgen_ebinop(cgproc, (ast_ebinop_t *) expr->ext, lblock,
		    dname);
		break;
	case ant_etcond:
	case ant_ecomma:
	case ant_ecall:
	case ant_eindex:
	case ant_ederef:
	case ant_eaddr:
	case ant_esizeof:
	case ant_ecast:
	case ant_ecliteral:
	case ant_emember:
	case ant_eindmember:
	case ant_eusign:
	case ant_elnot:
	case ant_ebnot:
	case ant_epreadj:
	case ant_epostadj:
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EOK;
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}


/** Generate code for return statement.
 *
 * @param cgproc Code generator for procedure
 * @param block AST return statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_return(cgen_proc_t *cgproc, ast_return_t *areturn,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *arg = NULL;
	const char *avar = NULL;
	int rc;

	rc = cgen_expr(cgproc, areturn->arg, lblock, &avar);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avar, &arg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_retv;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &arg->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (arg != NULL)
		ir_oper_destroy(&arg->oper);
	return rc;
}

/** Generate code for statement.
 *
 * @param cgproc Code generator for procedure
 * @param stmt AST statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_stmt(cgen_proc_t *cgproc, ast_node_t *stmt,
    ir_lblock_t *lblock)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	switch (stmt->ntype) {
	case ant_asm:
	case ant_break:
	case ant_continue:
	case ant_goto:
		atok = ast_tree_first_tok(stmt);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This statement type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EOK;
		break;
	case ant_return:
		rc = cgen_return(cgproc, (ast_return_t *) stmt->ext, lblock);
		break;
	case ant_if:
	case ant_while:
	case ant_do:
	case ant_for:
	case ant_switch:
	case ant_clabel:
	case ant_glabel:
	case ant_stexpr:
	case ant_stdecln:
	case ant_stnull:
	case ant_lmacro:
	case ant_block:
		atok = ast_tree_first_tok(stmt);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This statement type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EOK;
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Generate code for block.
 *
 * @param cgproc Code generator for procedure
 * @param block AST block
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_block(cgen_proc_t *cgproc, ast_block_t *block,
    ir_lblock_t *lblock)
{
	ast_node_t *stmt;
	int rc;

	stmt = ast_block_first(block);
	while (stmt != NULL) {
		rc = cgen_stmt(cgproc, stmt, lblock);
		if (rc != EOK)
			return rc;

		stmt = ast_block_next(stmt);
	}

	return EOK;
}

/** Generate code for global declaration.
 *
 * @param cgen Code generator
 * @param gdecln Global declaration
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_gdecln(cgen_t *cgen, ast_gdecln_t *gdecln, ir_module_t *irmod)
{
	ir_proc_t *proc = NULL;
	ir_lblock_t *lblock = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	cgen_proc_t *cgproc = NULL;
	char *pident = NULL;
	int rc;

	if (gdecln->body != NULL) {
		aident = ast_gdecln_get_ident(gdecln);
		ident = (comp_tok_t *) aident->data;
		rc = cgen_gprefix(ident->tok.text, &pident);
		if (rc != EOK)
			goto error;

		rc = ir_lblock_create(&lblock);
		if (rc != EOK)
			goto error;

		rc = cgen_proc_create(cgen, &cgproc);
		if (rc != EOK)
			goto error;

		rc = cgen_block(cgproc, gdecln->body, lblock);
		if (rc != EOK)
			goto error;

		rc = ir_proc_create(pident, lblock, &proc);
		if (rc != EOK)
			goto error;

		free(pident);
		pident = NULL;
		lblock = NULL;

		ir_module_append(irmod, &proc->decln);
		proc = NULL;

		cgen_proc_destroy(cgproc);
		cgproc = NULL;
	}

	return EOK;
error:
	ir_proc_destroy(proc);
	cgen_proc_destroy(cgproc);
	if (pident != NULL)
		free(pident);
	return rc;
}

/** Generate code for global declaration.
 *
 * @param cgen Code generator
 * @param decln Global (macro, extern) declaration
 * @return EOK on success or an error code
 */
static int cgen_global_decln(cgen_t *cgen, ast_node_t *decln, ir_module_t *irmod)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	switch (decln->ntype) {
	case ant_gdecln:
		rc = cgen_gdecln(cgen, (ast_gdecln_t *) decln->ext, irmod);
		break;
	case ant_gmdecln:
		assert(false); // XXX
		rc = EINVAL;
		break;
	case ant_nulldecln:
	case ant_externc:
		atok = ast_tree_first_tok(decln);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This declaration type is not implemented.\n");
		cgen->error = true; // TODO
		rc = EOK;
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Generate code for module.
 *
 * @param cgen Code generator
 * @param astmod AST module
 * @param rirmod Place to store pointer to new IR module
 * @return EOK on success or an error code
 */
int cgen_module(cgen_t *cgen, ast_module_t *astmod, ir_module_t **rirmod)
{
	ir_module_t *irmod;
	int rc;
	ast_node_t *decln;

	(void) cgen;
	(void) astmod;

	rc = ir_module_create(&irmod);
	if (rc != EOK)
		return rc;

	decln = ast_module_first(astmod);
	while (decln != NULL) {
		rc = cgen_global_decln(cgen, decln, irmod);
		if (rc != EOK)
			goto error;

		decln = ast_module_next(decln);
	}

	*rirmod = irmod;
	return EOK;
error:
	ir_module_destroy(irmod);
	return rc;
}

/** Destroy code generator.
 *
 * @param cgen Code generator or @c NULL
 */
void cgen_destroy(cgen_t *cgen)
{
	if (cgen == NULL)
		return;

	free(cgen);
}
