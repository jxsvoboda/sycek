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
#include <scope.h>
#include <stdlib.h>
#include <string.h>
#include <symbols.h>

static int cgen_expr_lvalue(cgen_proc_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_expr_rvalue(cgen_proc_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_expr(cgen_proc_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_block(cgen_proc_t *, ast_block_t *, ir_lblock_t *);

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

/** Create local variable operand with specific number.
 *
 * @param var Variable number
 * @param roper Place to store pointer to new variable operand
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_create_lvar_num_oper(unsigned var, ir_oper_var_t **roper)
{
	int rc;
	int rv;
	ir_oper_var_t *oper = NULL;
	char *svar = NULL;

	rv = asprintf(&svar, "%%%u", var);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	rc = ir_oper_var_create(svar, &oper);
	if (rc != EOK)
		goto error;

	free(svar);

	*roper = oper;
	return EOK;
error:
	if (svar != NULL)
		free(svar);
	return rc;
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
	unsigned var;

	var = cgproc->next_var++;
	return cgen_create_lvar_num_oper(var, roper);
}

/** Create new local label.
 *
 * @param cgproc Code generator for procedure
 * @return New label number
 */
static unsigned cgen_new_label_num(cgen_proc_t *cgproc)
{
	return cgproc->next_label++;
}

/** Create new local label.
 *
 * @param cgproc Code generator for procedure
 * @param pattern Label pattern
 * @param lblno Label number
 * @param rlabel Place to store pointer to new label
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_create_label(cgen_proc_t *cgproc, const char *pattern,
    unsigned lblno, char **rlabel)
{
	char *label;
	int rv;

	(void) cgproc;

	rv = asprintf(&label, "%%%s%u", pattern, lblno);
	if (rv < 0)
		return ENOMEM;

	*rlabel = label;
	return EOK;
}

/** Create code generator.
 *
 * @param rcgen Place to store pointer to new code generator
 * @return EOK on success, ENOMEM if out of memory
 */
int cgen_create(cgen_t **rcgen)
{
	cgen_t *cgen;
	int rc;

	cgen = calloc(1, sizeof(cgen_t));
	if (cgen == NULL)
		return ENOMEM;

	rc = scope_create(NULL, &cgen->scope);
	if (rc != EOK) {
		free(cgen);
		return ENOMEM;
	}

	cgen->error = false;
	cgen->warnings = 0;
	*rcgen = cgen;
	return EOK;
}

/** Create code generator.
 *
 * @param cgen Code generator
 * @param rcgen Place to store pointer to new code generator
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_proc_create(cgen_t *cgen, cgen_proc_t **rcgproc)
{
	cgen_proc_t *cgproc;
	int rc;

	cgproc = calloc(1, sizeof(cgen_proc_t));
	if (cgproc == NULL)
		return ENOMEM;

	rc = scope_create(cgen->scope, &cgproc->arg_scope);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	cgproc->cgen = cgen;
	cgproc->next_var = 0;
	cgproc->next_label = 0;
	*rcgproc = cgproc;
	return EOK;
error:
	free(cgproc);
	return rc;
}

/** Destroy code generator for procedure.
 *
 * @param cgen Code generator or @c NULL
 */
static void cgen_proc_destroy(cgen_proc_t *cgproc)
{
	if (cgproc == NULL)
		return;

	scope_destroy(cgproc->arg_scope);
	free(cgproc);
}

/** Generate code for integer literal expression.
 *
 * @param cgproc Code generator for procedure
 * @param eint AST integer literal expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eint(cgen_proc_t *cgproc, ast_eint_t *eint,
    ir_lblock_t *lblock, cgen_eres_t *eres)
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

	instr->itype = iri_imm;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	return rc;
}

/** Generate code for identifier expression referencing global symbol.
 *
 * @param cgproc Code generator for procedure
 * @param eident AST identifier expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eident_gsym(cgen_proc_t *cgproc, ast_eident_t *eident,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *ident;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	char *pident = NULL;
	int rc;

	ident = (comp_tok_t *) eident->tident.data;

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(pident, &var);
	if (rc != EOK)
		goto error;

	instr->itype = iri_varptr;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &var->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_lvalue;

	dest = NULL;
	var = NULL;
	free(pident);
	pident = NULL;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (var != NULL)
		ir_oper_destroy(&var->oper);
	if (pident != NULL)
		free(pident);
	return rc;
}

/** Generate code for identifier expression referencing function argument.
 *
 * @param cgproc Code generator for procedure
 * @param eident AST identifier expression
 * @param vident Identifier of IR variable holding the argument
 * @param idx Argument index
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 *
 * @return EOK on success or an error code
 */
static int cgen_eident_arg(cgen_proc_t *cgproc, ast_eident_t *eident,
    const char *vident, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	(void) cgproc;
	(void) eident;
	(void) lblock;

	eres->varname = vident;
	eres->valtype = cgen_rvalue;
	return EOK;
}

/** Generate code for identifier expression.
 *
 * @param cgproc Code generator for procedure
 * @param eident AST identifier expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eident(cgen_proc_t *cgproc, ast_eident_t *eident,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *ident;
	scope_member_t *member;
	int rc = EINVAL;

	ident = (comp_tok_t *) eident->tident.data;

	/* Check if the identifier is declared */
	member = scope_lookup(cgproc->arg_scope, ident->tok.text);
	if (member == NULL) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Undeclared identifier '%s'.\n",
		    ident->tok.text);
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	switch (member->mtype) {
	case sm_gsym:
		rc = cgen_eident_gsym(cgproc, eident, lblock, eres);
		break;
	case sm_arg:
		rc = cgen_eident_arg(cgproc, eident, member->m.arg.vident,
		    lblock, eres);
		break;
	case sm_lvar:
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Referencing local variable '%s' "
		    "is not implemented.\n", ident->tok.text);
		cgproc->cgen->error = true; // TODO
		rc = ENOTSUP;
		break;
	}

	return rc;
}

/** Generate code for addition expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_add(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	rc = cgen_expr_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres.varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_add;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
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

/** Generate code for subtraction expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_subtract(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	rc = cgen_expr_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres.varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_sub;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
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

/** Generate code for assignment expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres.varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = rres.varname;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	return rc;
}

/** Generate code for binary operator expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ebinop(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *tok;

	switch (ebinop->optype) {
	case abo_plus:
		return cgen_add(cgproc, ebinop, lblock, eres);
	case abo_minus:
		return cgen_subtract(cgproc, ebinop, lblock, eres);
	case abo_assign:
		return cgen_assign(cgproc, ebinop, lblock, eres);
	default:
		tok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unimplemented binary operator.\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}
}

/** Generate code for call expression.
 *
 * @param cgproc Code generator for procedure
 * @param ecall AST call expression)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ecall(cgen_proc_t *cgproc, ast_ecall_t *ecall,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	comp_tok_t *ident;
	ast_eident_t *eident;
	scope_member_t *member;
	char *pident = NULL;
	ast_ecall_arg_t *earg;
	cgen_eres_t ares;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *fun = NULL;
	ir_oper_list_t *args = NULL;
	ir_oper_var_t *arg = NULL;
	int rc;

	if (ecall->fexpr->ntype != ant_eident) {
		atok = ast_tree_first_tok(ecall->fexpr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Function call needs an identifier (not implemented).\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	eident = (ast_eident_t *) ecall->fexpr->ext;
	ident = (comp_tok_t *) eident->tident.data;

	/* Check if the identifier is declared */
	member = scope_lookup(cgproc->cgen->scope, ident->tok.text);
	if (member == NULL) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Undeclared identifier '%s'.\n",
		    ident->tok.text);
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(pident, &fun);
	if (rc != EOK)
		goto error;

	rc = ir_oper_list_create(&args);
	if (rc != EOK)
		goto error;

	/*
	 * Each argument needs to be evaluated. The code for evaluating
	 * arguments will precede the call instruction. The resulting
	 * value of each argument needs to be appended to the argument
	 * list.
	 */
	earg = ast_ecall_first(ecall);
	while (earg != NULL) {
		rc = cgen_expr_rvalue(cgproc, earg->arg, lblock, &ares);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(ares.varname, &arg);
		if (rc != EOK)
			goto error;

		ir_oper_list_append(args, &arg->oper);
		earg = ast_ecall_next(earg);
	}

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	free(pident);

	instr->itype = iri_call;
	instr->dest = &dest->oper;
	instr->op1 = &fun->oper;
	instr->op2 = &args->oper;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (fun != NULL)
		ir_oper_destroy(&fun->oper);
	if (args != NULL)
		ir_oper_destroy(&args->oper);
	if (pident != NULL)
		free(pident);
	return rc;
}

/** Generate code for expression.
 *
 * @param cgproc Code generator for procedure
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr(cgen_proc_t *cgproc, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	(void) lblock;

	switch (expr->ntype) {
	case ant_eint:
		rc = cgen_eint(cgproc, (ast_eint_t *) expr->ext, lblock, eres);
		break;
	case ant_echar:
	case ant_estring:
	case ant_eident:
		rc = cgen_eident(cgproc, (ast_eident_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eparen:
	case ant_econcat:
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_ebinop:
		rc = cgen_ebinop(cgproc, (ast_ebinop_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_etcond:
	case ant_ecomma:
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_ecall:
		rc = cgen_ecall(cgproc, (ast_ecall_t *) expr->ext, lblock,
		    eres);
		break;
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
		rc = EINVAL;
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Generate code for expression, producing an lvalue.
 *
 * Verify that it is actually an lvalue, otherwise produce an error.
 *
 * @param cgproc Code generator for procedure
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr_lvalue(cgen_proc_t *cgproc, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	rc = cgen_expr(cgproc, expr, lblock, eres);
	if (rc != EOK)
		return rc;

	if (eres->valtype != cgen_lvalue) {
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr); // XXX Print range
		fprintf(stderr, ": Lvalue required.\n");
		cgproc->cgen->error = true;
		return EINVAL;
	}

	return EOK;
}

/** Generate code for expression, producing an rvalue.
 *
 * If the result of expression is an lvalue, read it to produce an rvalue.
 *
 * @param cgproc Code generator for procedure
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr_rvalue(cgen_proc_t *cgproc, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t res;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	int rc;

	rc = cgen_expr(cgproc, expr, lblock, &res);
	if (rc != EOK)
		return rc;

	/* Check if we already have an rvalue */
	if (res.valtype == cgen_rvalue) {
		*eres = res;
		return EOK;
	}

	/* Need to read the value in */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(res.varname, &var);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &var->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (var != NULL)
		ir_oper_destroy(&var->oper);
	return rc;
}

/** Generate code for return statement.
 *
 * @param cgproc Code generator for procedure
 * @param areturn AST return statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_return(cgen_proc_t *cgproc, ast_return_t *areturn,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *arg = NULL;
	cgen_eres_t ares;
	int rc;

	rc = cgen_expr_rvalue(cgproc, areturn->arg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &arg);
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

/** Generate code for while statement.
 *
 * @param cgproc Code generator for procedure
 * @param awhile AST while statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_while(cgen_proc_t *cgproc, ast_while_t *awhile,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	cgen_eres_t cres;
	unsigned lblno;
	char *wlabel = NULL;
	char *ewlabel = NULL;
	int rc;

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "while", lblno, &wlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_while", lblno, &ewlabel);
	if (rc != EOK)
		goto error;

	ir_lblock_append(lblock, wlabel, NULL);

	/* Condition */

	rc = cgen_expr_rvalue(cgproc, awhile->cond, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* jz %<cres>, %end_while */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ewlabel, &larg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_jz;
	instr->width = 0;
	instr->dest = NULL;
	instr->op1 = &carg->oper;
	instr->op2 = &larg->oper;

	carg = NULL;
	larg = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* Body */

	rc = cgen_block(cgproc, awhile->body, lblock);
	if (rc != EOK)
		goto error;

	/* jp %while */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(wlabel, &larg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_jmp;
	instr->width = 0;
	instr->dest = NULL;
	instr->op1 = &larg->oper;
	instr->op2 = NULL;

	larg = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	ir_lblock_append(lblock, ewlabel, NULL);

	free(wlabel);
	free(ewlabel);
	return EOK;
error:
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (instr != NULL)
		ir_instr_destroy(instr);
	if (wlabel != NULL)
		free(wlabel);
	if (ewlabel != NULL)
		free(ewlabel);
	return rc;
}

/** Generate code for do loop statement.
 *
 * @param cgproc Code generator for procedure
 * @param awhile AST do loop statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_do(cgen_proc_t *cgproc, ast_do_t *ado, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	cgen_eres_t cres;
	unsigned lblno;
	char *dlabel = NULL;
	char *edlabel = NULL;
	int rc;

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "do", lblno, &dlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_do", lblno, &edlabel);
	if (rc != EOK)
		goto error;

	ir_lblock_append(lblock, dlabel, NULL);

	/* Body */

	rc = cgen_block(cgproc, ado->body, lblock);
	if (rc != EOK)
		goto error;

	/* Condition */

	rc = cgen_expr_rvalue(cgproc, ado->cond, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* jnz %<cres>, %do */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(dlabel, &larg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_jnz;
	instr->width = 0;
	instr->dest = NULL;
	instr->op1 = &carg->oper;
	instr->op2 = &larg->oper;

	carg = NULL;
	larg = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	ir_lblock_append(lblock, edlabel, NULL);

	free(dlabel);
	free(edlabel);
	return EOK;
error:
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (instr != NULL)
		ir_instr_destroy(instr);
	if (dlabel != NULL)
		free(dlabel);
	if (edlabel != NULL)
		free(edlabel);
	return rc;
}

/** Generate code for expression statement.
 *
 * @param cgproc Code generator for procedure
 * @param stexpr AST expression statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_stexpr(cgen_proc_t *cgproc, ast_stexpr_t *stexpr,
    ir_lblock_t *lblock)
{
	cgen_eres_t ares;
	int rc;

	/* Compute the value of the expression (e.g. read volatile variable) */
	rc = cgen_expr_rvalue(cgproc, stexpr->expr, lblock, &ares);
	if (rc != EOK)
		goto error;

	/* Ignore the value of the expression */
	(void) ares;

	return EOK;
error:
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
		rc = EINVAL;
		break;
	case ant_return:
		rc = cgen_return(cgproc, (ast_return_t *) stmt->ext, lblock);
		break;
	case ant_if:
		atok = ast_tree_first_tok(stmt);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This statement type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_while:
		rc = cgen_while(cgproc, (ast_while_t *) stmt->ext, lblock);
		break;
	case ant_do:
		rc = cgen_do(cgproc, (ast_do_t *) stmt->ext, lblock);
		break;
	case ant_for:
	case ant_switch:
	case ant_clabel:
	case ant_glabel:
		atok = ast_tree_first_tok(stmt);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This statement type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_stexpr:
		rc = cgen_stexpr(cgproc, (ast_stexpr_t *) stmt->ext, lblock);
		break;
	case ant_stdecln:
	case ant_stnull:
	case ant_lmacro:
	case ant_block:
		atok = ast_tree_first_tok(stmt);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This statement type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
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

/** Generate code for function definition.
 *
 * @param cgen Code generator
 * @param gdecln Global declaration that is a function definition
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_fundef(cgen_t *cgen, ast_gdecln_t *gdecln, ir_module_t *irmod)
{
	ir_proc_t *proc = NULL;
	ir_lblock_t *lblock = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	cgen_proc_t *cgproc = NULL;
	ast_idlist_entry_t *idle;
	ast_dfun_t *dfun;
	ast_dfun_arg_t *arg;
	ir_proc_arg_t *iarg;
	char *pident = NULL;
	char *arg_ident = NULL;
	ast_tok_t *atok;
	ast_dident_t *dident;
	comp_tok_t *tok;
	scope_member_t *member;
	symbol_t *symbol;
	int rc;
	int rv;

	aident = ast_gdecln_get_ident(gdecln);
	ident = (comp_tok_t *) aident->data;

	/* If the function is not declared yet, create a symbol */
	symbol = symbols_lookup(cgen->symbols, ident->tok.text);
	if (symbol == NULL) {
		rc = symbols_insert(cgen->symbols, st_fun, ident->tok.text);
		if (rc != EOK)
			goto error;

		symbol = symbols_lookup(cgen->symbols, ident->tok.text);
		assert(symbol != NULL);
	} else {
		if (symbol->stype != st_fun) {
			/* Already declared as a different type of symbol */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": '%s' already declared as a "
			    "different type of symbol.\n", ident->tok.text);
			cgen->error = true; // XXX
			return EINVAL;
		}
	}

	/* Mark the symbol as defined */
	symbol->flags |= sf_defined;

	/* Insert identifier into module scope */
	rc = scope_insert_gsym(cgen->scope, ident->tok.text);
	if (rc == ENOMEM)
		goto error;

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = cgen_proc_create(cgen, &cgproc);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create(pident, 0, lblock, &proc);
	if (rc != EOK)
		goto error;

	/* lblock is now owned by proc */
	lblock = NULL;

	/* Identifier-declarator list entry */
	idle = ast_idlist_first(gdecln->idlist);
	assert(idle != NULL);
	assert(ast_idlist_next(idle) == NULL);

	/* Get the function declarator */
	if (idle->decl->ntype != ant_dfun) {
		atok = ast_tree_first_tok(idle->decl);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Function declarator required.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	dfun = (ast_dfun_t *)idle->decl->ext;

	/* Arguments */
	arg = ast_dfun_first(dfun);
	while (arg != NULL) {
		// XXX Process arg->dspecs

		if (arg->decl->ntype == ant_dnoident) {
			/* Should be void */
			arg = ast_dfun_next(arg);
			if (arg != NULL) {
				fprintf(stderr, ": 'void' must be the only parameter.\n");
				cgproc->cgen->error = true; // XXX
				rc = EINVAL;
				goto error;
			}

			break;
		}

		if (arg->decl->ntype != ant_dident) {
			atok = ast_tree_first_tok(arg->decl);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Declarator not implemented.\n");
			cgproc->cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		dident = (ast_dident_t *) arg->decl->ext;
		tok = (comp_tok_t *) dident->tident.data;

		if (arg->aslist != NULL) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Atribute specifier not implemented.\n");
			++cgproc->cgen->warnings;
		}

		/* Check for shadowing a global-scope identifier */
		member = scope_lookup(cgen->scope, tok->tok.text);
		if (member != NULL) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Declaration of '%s' "
			    "shadows a wider-scope declaration.\n",
			    tok->tok.text);
			++cgen->warnings;
		}

		rv = asprintf(&arg_ident, "%%%d", cgproc->next_var++);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		rc = ir_proc_arg_create(arg_ident, &iarg);
		if (rc != EOK)
			goto error;

		free(arg_ident);
		arg_ident = NULL;

		/* Insert identifier into argument scope */
		rc = scope_insert_arg(cgproc->arg_scope, tok->tok.text, iarg->ident);
		if (rc != EOK) {
			if (rc == EEXIST) {
				lexer_dprint_tok(&tok->tok, stderr);
				fprintf(stderr, ": Duplicate argument identifier '%s'.\n",
				    tok->tok.text);
				cgen->error = true; // XXX
				rc = EINVAL;
				goto error;
			}
		}

		ir_proc_append_arg(proc, iarg);
		arg = ast_dfun_next(arg);
	}

	if (gdecln->body != NULL) {
		rc = cgen_block(cgproc, gdecln->body, proc->lblock);
		if (rc != EOK)
			goto error;
	}

	free(pident);
	pident = NULL;

	ir_module_append(irmod, &proc->decln);
	proc = NULL;

	cgen_proc_destroy(cgproc);
	cgproc = NULL;

	return EOK;
error:
	ir_proc_destroy(proc);
	cgen_proc_destroy(cgproc);
	if (lblock != NULL)
		ir_lblock_destroy(lblock);
	if (pident != NULL)
		free(pident);
	if (arg_ident != NULL)
		free(arg_ident);
	return rc;
}

/** Generate code for function declaration.
 *
 * @param cgen Code generator
 * @param gdecln Global declaration that is a function definition
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_fundecl(cgen_t *cgen, ast_gdecln_t *gdecln, ir_module_t *irmod)
{
	ast_tok_t *aident;
	comp_tok_t *ident;
	symbol_t *symbol;
	int rc;

	(void) irmod;

	aident = ast_gdecln_get_ident(gdecln);
	ident = (comp_tok_t *) aident->data;

	/*
	 * All we do here is create a symbol. At the end of processing the
	 * module we will go back and find all declared, but not defined,
	 * functions and create extern declarations for them. We will insert
	 * those at the beginning of the IR module.
	 */

	/* The function can already be declared or even defined */
	symbol = symbols_lookup(cgen->symbols, ident->tok.text);
	if (symbol == NULL) {
		rc = symbols_insert(cgen->symbols, st_fun, ident->tok.text);
		if (rc != EOK)
			return rc;

		/* Insert identifier into module scope */
		rc = scope_insert_gsym(cgen->scope, ident->tok.text);
		if (rc == ENOMEM)
			return rc;
	} else {
		if (symbol->stype != st_fun) {
			/* Already declared as a different type of symbol */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": '%s' already declared as a "
			    "different type of symbol.\n", ident->tok.text);
			cgen->error = true; // XXX
			return EINVAL;
		}
	}

	return EOK;
}

/** Generate code for global variable definition.
 *
 * @param cgen Code generator
 * @param entry Init-declarator list entry that declares a variable
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_vardef(cgen_t *cgen, ast_idlist_entry_t *entry,
    ir_module_t *irmod)
{
	ir_var_t *var = NULL;
	ir_dblock_t *dblock = NULL;
	ir_dentry_t *dentry = NULL;
	ast_tok_t *aident;
	ast_eint_t *eint;
	comp_tok_t *ident;
	comp_tok_t *lit;
	ast_tok_t *atok;
	comp_tok_t *tok;
	char *pident = NULL;
	int32_t initval;
	int rc;

	(void) cgen;

	aident = ast_decl_get_ident(entry->decl);
	ident = (comp_tok_t *) aident->data;

	/* Insert identifier into module scope */
	rc = scope_insert_gsym(cgen->scope, ident->tok.text);
	if (rc == ENOMEM)
		goto error;

	if (entry->init != NULL) {
		if (entry->init->ntype != ant_eint) {
			atok = ast_tree_first_tok(entry->init);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Unsupported initializer.\n");
			cgen->error = true;
			rc = EINVAL;
			goto error;
		}

		eint = (ast_eint_t *) entry->init->ext;

		lit = (comp_tok_t *) eint->tlit.data;
		rc = cgen_intlit_val(lit, &initval);
		if (rc != EOK)
			goto error;
	} else {
		/* Initialize with zero (TODO: uninitialized?) */
		initval = 0;
	}

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = ir_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	rc = ir_var_create(pident, dblock, &var);
	if (rc != EOK)
		goto error;

	free(pident);
	pident = NULL;
	dblock = NULL;

	rc = ir_dentry_create_int(cgen->arith_width, initval, &dentry);
	if (rc != EOK)
		goto error;

	rc = ir_dblock_append(var->dblock, dentry);
	if (rc != EOK)
		goto error;

	dentry = NULL;

	ir_module_append(irmod, &var->decln);
	var = NULL;

	return EOK;
error:
	ir_var_destroy(var);
	ir_dentry_destroy(dentry);
	if (pident != NULL)
		free(pident);
	return rc;
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
	ast_idlist_entry_t *entry;
	int rc;

	if (gdecln->body != NULL) {
		rc = cgen_fundef(cgen, gdecln, irmod);
		if (rc != EOK)
			goto error;
	} else if (gdecln->idlist != NULL) {
		/* Possibly variable declarations */
		entry = ast_idlist_first(gdecln->idlist);
		while (entry != NULL) {
			if (ast_decl_is_vardecln(entry->decl)) {
				/* Variable declaration */
				rc = cgen_vardef(cgen, entry, irmod);
				if (rc != EOK)
					goto error;
			} else {
				/* Assuming it's a function declaration */
				rc = cgen_fundecl(cgen, gdecln, irmod);
				if (rc != EOK)
					goto error;
			}

			entry = ast_idlist_next(entry);
		}
	}

	return EOK;
error:
	return rc;
}

/** Generate code for global declaration.
 *
 * @param cgen Code generator
 * @param decln Global (macro, extern) declaration
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_global_decln(cgen_t *cgen, ast_node_t *decln,
    ir_module_t *irmod)
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
		rc = EINVAL;
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Generate symbol declarations for a module.
 *
 * @param cgen Code generator
 * @param symbols Symbol directory
 * @param irmod IR module where to add the declarations
 * @return EOK on success or an error code
 */
static int cgen_module_symdecls(cgen_t *cgen, symbols_t *symbols,
    ir_module_t *irmod)
{
	int rc;
	ir_proc_t *proc;
	symbol_t *symbol;
	char *pident = NULL;

	(void) cgen;

	symbol = symbols_first(symbols);
	while (symbol != NULL) {
		if ((symbol->flags & sf_defined) == 0) {
			rc = cgen_gprefix(symbol->ident, &pident);
			if (rc != EOK)
				goto error;

			rc = ir_proc_create(pident, irp_extern, NULL, &proc);
			if (rc != EOK)
				goto error;

			free(pident);
			pident = NULL;

			ir_module_append(irmod, &proc->decln);
		}

		symbol = symbols_next(symbol);
	}

	return EOK;
error:
	if (pident != NULL)
		free(pident);
	return rc;
}

/** Generate code for module.
 *
 * @param cgen Code generator
 * @param astmod AST module
 * @param symbols Symbol directory to fill in
 * @param rirmod Place to store pointer to new IR module
 * @return EOK on success or an error code
 */
int cgen_module(cgen_t *cgen, ast_module_t *astmod, symbols_t *symbols,
    ir_module_t **rirmod)
{
	ir_module_t *irmod;
	int rc;
	ast_node_t *decln;

	cgen->symbols = symbols;

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

	rc = cgen_module_symdecls(cgen, symbols, irmod);
	if (rc != EOK)
		goto error;

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

	scope_destroy(cgen->scope);
	free(cgen);
}
