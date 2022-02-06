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
#include <charcls.h>
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
static int cgen_gn_block(cgen_proc_t *, ast_block_t *, ir_lblock_t *);

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

	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		text += 2;

		/* Hexadecimal */
		while (is_hexdigit(*text)) {
			if (is_num(*text))
				val = val * 16 + (*text - '0');
			else if (*text >= 'a' && *text <= 'f')
				val = val * 16 + 10 + (*text - 'a');
			else
				val = val * 16 + 10 + (*text - 'A');
			++text;
		}
	} else if (text[0] == '0' && is_num(text[1])) {
		++text;
		/* Octal */
		while (is_octdigit(*text)) {
			val = val * 8 + (*text - '0');
			++text;
		}
	} else {
		/* Decimal */
		while (is_num(*text)) {
			val = val * 10 + (*text - '0');
			++text;
		}
	}

	// XXX Support suffixes
	if (*text != '\0')
		return EINVAL;

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

/** Find local variable in procedure by name.
 *
 * @param cgproc Code generator for procedure
 * @param ident C variable identifier
 * @return IR local variable or @c NULL if not found
 */
static ir_lvar_t *cgen_proc_find_lvar(cgen_proc_t *cgproc, const char *ident)
{
	ir_lvar_t *lvar;

	lvar = ir_proc_first_lvar(cgproc->irproc);
	while (lvar != NULL) {
		if (strcmp(lvar->ident, ident) == 0)
			return lvar;

		lvar = ir_proc_next_lvar(lvar);
	}

	return NULL;
}

/** Create new local variable name.
 *
 * Since in C any number of variables of the same name can be declared
 * in the same function (in adjacent / nested blocks), we need to append
 * a number in case of conflict.
 *
 * @param cgproc Code generator for procedure
 * @param ident C variable identifier
 * @param rname Place to store pointer to new variable name
 * @return EOK on success or an error code
 */
static int cgen_create_loc_var_name(cgen_proc_t *cgproc, const char *ident,
    char **rname)
{
	char *vident = NULL;
	ir_lvar_t *lvar;
	int version;
	int rv;
	int rc;

	rv = asprintf(&vident, "%%%s", ident);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	lvar = cgen_proc_find_lvar(cgproc, vident);
	if (lvar == NULL) {
		*rname = vident;
		return EOK;
	}

	version = 1;
	while (lvar != NULL) {
		free(vident);
		vident = NULL;

		/*
		 * Due to the limitations of Z80asm identifiers we
		 * cannot render this as %name@version, because
		 * we would not be able to mangle it ouside of
		 * C variable namespace. Therefore %version@name
		 * (C variables cannot start with a a number).
		 *
		 * Once we are free of the shackles of Z80asm,
		 * we can flip this around.
		 */
		rv = asprintf(&vident, "%%%d@%s", version, ident);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		++version;
		lvar = cgen_proc_find_lvar(cgproc, vident);
	}

	*rname = vident;
	return EOK;
error:
	return rc;
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
 * @param irproc IR procedure
 * @param rcgen Place to store pointer to new code generator
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_proc_create(cgen_t *cgen, ir_proc_t *irproc,
    cgen_proc_t **rcgproc)
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

	/* Function's top-level scope is inside the argument scope */
	rc = scope_create(cgproc->arg_scope, &cgproc->proc_scope);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	/*
	 * The member cur_scope tracks the current scope as we descend into
	 * and ascend out of nested blocks.
	 */
	cgproc->cur_scope = cgproc->proc_scope;

	cgproc->cgen = cgen;
	cgproc->irproc = irproc;
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

	scope_destroy(cgproc->proc_scope);
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
	if (rc != EOK) {
		lexer_dprint_tok(&lit->tok, stderr);
		fprintf(stderr, ": Invalid integer literal.\n");
		cgproc->cgen->error = true; // TODO
		goto error;
	}

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

/** Generate code for identifier expression referencing local variable.
 *
 * @param cgproc Code generator for procedure
 * @param eident AST identifier expression
 * @param vident Identifier of IR variable holding the local variable
 * @param idx Argument index
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 *
 * @return EOK on success or an error code
 */
static int cgen_eident_lvar(cgen_proc_t *cgproc, ast_eident_t *eident,
    const char *vident, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	comp_tok_t *ident;
	int rc;

	ident = (comp_tok_t *) eident->tident.data;
	(void) ident;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(vident, &var);
	if (rc != EOK)
		goto error;

	instr->itype = iri_lvarptr;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &var->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_lvalue;

	dest = NULL;
	var = NULL;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (var != NULL)
		ir_oper_destroy(&var->oper);
	return rc;
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
	member = scope_lookup(cgproc->cur_scope, ident->tok.text);
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
		rc = cgen_eident_lvar(cgproc, eident, member->m.lvar.vident,
		    lblock, eres);
		break;
	}

	return rc;
}

/** Generate code for parenthesized expression.
 *
 * @param cgproc Code generator for procedure
 * @param eparen AST parenthesized expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eparen(cgen_proc_t *cgproc, ast_eparen_t *eparen,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	return cgen_expr(cgproc, eparen->bexpr, lblock, eres);
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

/** Generate code for multiplication expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (multiplication)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_mul(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_mul;
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

/** Generate code for shift left expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (shift left)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_shl(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_shl;
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

/** Generate code for shift right expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (shift right)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_shr(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_shr;
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

/** Generate code for less than expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (less than)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lt(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_lt;
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

/** Generate code for less than or equal expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (less than or equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lteq(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_lteq;
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

/** Generate code for greater than expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (greater than)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gt(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_gt;
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

/** Generate code for greater than or equal expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (greater than or equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gteq(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_gteq;
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

/** Generate code for equal expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eq(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_eq;
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

/** Generate code for not equal expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (not equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_neq(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_neq;
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

/** Generate code for binary AND expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (binary AND)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_band(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_and;
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

/** Generate code for binary XOR expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (binary XOR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bxor(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_xor;
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

/** Generate code for binary OR expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (binary OR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bor(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
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

	instr->itype = iri_or;
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

/** Generate code for logical AND expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (logical AND)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_land(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *dest2 = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	ir_oper_imm_t *imm = NULL;
	unsigned lblno;
	char *flabel = NULL;
	char *elabel = NULL;
	const char *dvarname;
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "false_and", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_and", lblno, &elabel);
	if (rc != EOK)
		goto error;

	/* Evaluate left argument */
	rc = cgen_expr_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* jz %<lres>, %false_and */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(flabel, &larg);
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

	/* Evaluate right argument */

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* jz %<rres>, %false_and */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(flabel, &larg);
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

	/* Return 1 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(1, &imm);
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

	dvarname = dest->varname;
	dest = NULL;
	imm = NULL;

	/* jp %end_and */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(elabel, &larg);
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

	/* %false_and label */
	ir_lblock_append(lblock, flabel, NULL);

	/* Return 0 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	/*
	 * XXX Reusing the destination VR in this way does not conform
	 * to SSA. The only way to conform to SSA would be either to
	 * fuse the results of the two branches by using a Phi function or
	 * by writing/reading the result through a local variable
	 * (which is not constrained by SSA).
	 */
	rc = ir_oper_var_create(dvarname, &dest2);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(0, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest2->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest2->varname;
	eres->valtype = cgen_rvalue;

	dest2 = NULL;
	imm = NULL;

	/* %end_and label */
	ir_lblock_append(lblock, elabel, NULL);

	free(flabel);
	free(elabel);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (flabel != NULL)
		free(flabel);
	if (elabel != NULL)
		free(elabel);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (dest2 != NULL)
		ir_oper_destroy(&dest2->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	return rc;
}

/** Generate code for logical OR expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (logical OR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lor(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *dest2 = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	ir_oper_imm_t *imm = NULL;
	unsigned lblno;
	char *tlabel = NULL;
	char *elabel = NULL;
	const char *dvarname;
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "true_or", lblno, &tlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_or", lblno, &elabel);
	if (rc != EOK)
		goto error;

	/* Evaluate left argument */
	rc = cgen_expr_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* jnz %<lres>, %true_or */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(tlabel, &larg);
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

	/* Evaluate right argument */

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* jnz %<rres>, %true_or */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(tlabel, &larg);
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

	/* Return 0 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(0, &imm);
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

	dvarname = dest->varname;
	dest = NULL;
	imm = NULL;

	/* jp %end_or */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(elabel, &larg);
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

	/* %true_or label */
	ir_lblock_append(lblock, tlabel, NULL);

	/* Return 1 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	/*
	 * XXX Reusing the destination VR in this way does not conform
	 * to SSA. The only way to conform to SSA would be either to
	 * fuse the results of the two branches by using a Phi function or
	 * by writing/reading the result through a local variable
	 * (which is not constrained by SSA).
	 */
	rc = ir_oper_var_create(dvarname, &dest2);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(1, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest2->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest2->varname;
	eres->valtype = cgen_rvalue;

	dest2 = NULL;
	imm = NULL;

	/* %end_and label */
	ir_lblock_append(lblock, elabel, NULL);

	free(tlabel);
	free(elabel);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (tlabel != NULL)
		free(tlabel);
	if (elabel != NULL)
		free(elabel);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (dest2 != NULL)
		ir_oper_destroy(&dest2->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
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

/** Generate code for add assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (add assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_plus_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* add %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_add;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for subtract assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (subtract assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_minus_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* sub %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_sub;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for multiply assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (mutiply assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_times_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* mul %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_mul;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for shift left assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (shift left assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_shl_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* shl %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_shl;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for shift right assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (shift right assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_shr_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* shr %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_shr;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for bitwise AND assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (bitwise AND assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_band_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* and %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_and;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for bitwise XOR assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (bitwise XOR assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bxor_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* xor %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_xor;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for bitwise OR assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (bitwise OR assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bor_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *addr = NULL;
	ir_oper_var_t *aval = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t ares;
	cgen_eres_t bres;
	char *avalvn;
	char *resvn;
	int rc;

	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &ares);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %aval, %lres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &aval->oper;
	instr->op1 = &addr->oper;
	instr->op2 = NULL;
	avalvn = aval->varname;
	aval = NULL;
	addr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* or %res, %aval, %bval */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(avalvn, &aval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &bval);
	if (rc != EOK)
		goto error;

	instr->itype = iri_or;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &aval->oper;
	instr->op2 = &bval->oper;
	resvn = res->varname;
	res = NULL;
	aval = NULL;
	bval = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %laddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares.varname, &addr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &addr->oper;
	instr->op2 = &res->oper;
	addr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (addr != NULL)
		ir_oper_destroy(&addr->oper);
	if (aval != NULL)
		ir_oper_destroy(&aval->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
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
	case abo_times:
		return cgen_mul(cgproc, ebinop, lblock, eres);
	case abo_divide:
	case abo_modulo:
		tok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unimplemented binary operator.\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	case abo_shl:
		return cgen_shl(cgproc, ebinop, lblock, eres);
	case abo_shr:
		return cgen_shr(cgproc, ebinop, lblock, eres);
	case abo_lt:
		return cgen_lt(cgproc, ebinop, lblock, eres);
	case abo_lteq:
		return cgen_lteq(cgproc, ebinop, lblock, eres);
	case abo_gt:
		return cgen_gt(cgproc, ebinop, lblock, eres);
	case abo_gteq:
		return cgen_gteq(cgproc, ebinop, lblock, eres);
	case abo_eq:
		return cgen_eq(cgproc, ebinop, lblock, eres);
	case abo_neq:
		return cgen_neq(cgproc, ebinop, lblock, eres);
	case abo_band:
		return cgen_band(cgproc, ebinop, lblock, eres);
	case abo_bxor:
		return cgen_bxor(cgproc, ebinop, lblock, eres);
	case abo_bor:
		return cgen_bor(cgproc, ebinop, lblock, eres);
	case abo_land:
		return cgen_land(cgproc, ebinop, lblock, eres);
	case abo_lor:
		return cgen_lor(cgproc, ebinop, lblock, eres);
	case abo_assign:
		return cgen_assign(cgproc, ebinop, lblock, eres);
	case abo_plus_assign:
		return cgen_plus_assign(cgproc, ebinop, lblock, eres);
	case abo_minus_assign:
		return cgen_minus_assign(cgproc, ebinop, lblock, eres);
	case abo_times_assign:
		return cgen_times_assign(cgproc, ebinop, lblock, eres);
	case abo_divide_assign:
	case abo_modulo_assign:
		tok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unimplemented binary operator.\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	case abo_shl_assign:
		return cgen_shl_assign(cgproc, ebinop, lblock, eres);
	case abo_shr_assign:
		return cgen_shr_assign(cgproc, ebinop, lblock, eres);
	case abo_band_assign:
		return cgen_band_assign(cgproc, ebinop, lblock, eres);
	case abo_bxor_assign:
		return cgen_bxor_assign(cgproc, ebinop, lblock, eres);
	case abo_bor_assign:
		return cgen_bor_assign(cgproc, ebinop, lblock, eres);
	}

	/* Should not be reached */
	assert(false);
	return EINVAL;
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

/** Generate code for logical not expression.
 *
 * @param cgproc Code generator for procedure
 * @param elnot AST logical not expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_elnot(cgen_proc_t *cgproc, ast_elnot_t *elnot,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *barg = NULL;
	cgen_eres_t bres;
	int rc;

	/* Evaluate base expression */
	rc = cgen_expr_rvalue(cgproc, elnot->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* lnot %<dest>, %<bres> */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &barg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_lnot;
	instr->width = 0;
	instr->dest = &dest->oper;
	instr->op1 = &barg->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (barg != NULL)
		ir_oper_destroy(&barg->oper);
	return rc;
}

/** Generate code for binary NOT expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary NOT expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ebnot(cgen_proc_t *cgproc, ast_ebnot_t *ebnot,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *barg = NULL;
	cgen_eres_t bres;
	int rc;

	rc = cgen_expr_rvalue(cgproc, ebnot->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &barg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_bnot;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &barg->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (barg != NULL)
		ir_oper_destroy(&barg->oper);
	return rc;
}

/** Generate code for preadjustment expression.
 *
 * @param cgproc Code generator for procedure
 * @param epreadj AST preadjustment expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_epreadj(cgen_proc_t *cgproc, ast_epreadj_t *epreadj,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *baddr = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_imm_t *imm = NULL;
	ir_oper_var_t *adj = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t bres;
	char *bvalvn;
	char *adjvn;
	char *resvn;
	int rc;

	/* Evaluate base expression as lvalue */

	rc = cgen_expr_lvalue(cgproc, epreadj->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %bval, %bres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &bval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &baddr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &bval->oper;
	instr->op1 = &baddr->oper;
	instr->op2 = NULL;
	bvalvn = bval->varname;
	bval = NULL;
	baddr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* imm.16 %adj, 1 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &adj);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(1, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &adj->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;
	adjvn = adj->varname;
	adj = NULL;
	imm = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* add/sub %res, %bval, %adj */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bvalvn, &bval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(adjvn, &adj);
	if (rc != EOK)
		goto error;

	instr->itype = epreadj->adj == aat_inc ? iri_add : iri_sub;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &bval->oper;
	instr->op2 = &adj->oper;
	resvn = res->varname;
	res = NULL;
	bval = NULL;
	adj = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %baddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &baddr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &baddr->oper;
	instr->op2 = &res->oper;
	baddr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (baddr != NULL)
		ir_oper_destroy(&baddr->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	if (adj != NULL)
		ir_oper_destroy(&adj->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
	return rc;
}

/** Generate code for postadjustment expression.
 *
 * @param cgproc Code generator for procedure
 * @param epostadj AST postadjustment expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_epostadj(cgen_proc_t *cgproc, ast_epostadj_t *epostadj,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *baddr = NULL;
	ir_oper_var_t *bval = NULL;
	ir_oper_imm_t *imm = NULL;
	ir_oper_var_t *adj = NULL;
	ir_oper_var_t *res = NULL;
	cgen_eres_t bres;
	char *bvalvn;
	char *adjvn;
	char *resvn;
	int rc;

	/* Evaluate base expression as lvalue */

	rc = cgen_expr_lvalue(cgproc, epostadj->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* read %bval, %bres */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &bval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &baddr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &bval->oper;
	instr->op1 = &baddr->oper;
	instr->op2 = NULL;
	bvalvn = bval->varname;
	bval = NULL;
	baddr = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* imm.16 %adj, 1 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &adj);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(1, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &adj->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;
	adjvn = adj->varname;
	adj = NULL;
	imm = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* add/sub %res, %bval, %adj */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &res);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bvalvn, &bval);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(adjvn, &adj);
	if (rc != EOK)
		goto error;

	instr->itype = epostadj->adj == aat_inc ? iri_add : iri_sub;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &res->oper;
	instr->op1 = &bval->oper;
	instr->op2 = &adj->oper;
	resvn = res->varname;
	res = NULL;
	bval = NULL;
	adj = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* write nil, %baddr, %res */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &baddr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(resvn, &res);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &baddr->oper;
	instr->op2 = &res->oper;
	baddr = NULL;
	res = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	eres->varname = bvalvn;
	eres->valtype = cgen_rvalue;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (baddr != NULL)
		ir_oper_destroy(&baddr->oper);
	if (bval != NULL)
		ir_oper_destroy(&bval->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	if (adj != NULL)
		ir_oper_destroy(&adj->oper);
	if (res != NULL)
		ir_oper_destroy(&res->oper);
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
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_eident:
		rc = cgen_eident(cgproc, (ast_eident_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eparen:
		rc = cgen_eparen(cgproc, (ast_eparen_t *) expr->ext, lblock,
		    eres);
		break;
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
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_elnot:
		rc = cgen_elnot(cgproc, (ast_elnot_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ebnot:
		rc = cgen_ebnot(cgproc, (ast_ebnot_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_epreadj:
		rc = cgen_epreadj(cgproc, (ast_epreadj_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_epostadj:
		rc = cgen_epostadj(cgproc, (ast_epostadj_t *) expr->ext, lblock,
		    eres);
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

/** Generate code for if statement.
 *
 * @param cgproc Code generator for procedure
 * @param aif AST if statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_if(cgen_proc_t *cgproc, ast_if_t *aif,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	ast_elseif_t *elsif;
	cgen_eres_t cres;
	unsigned lblno;
	char *fiflabel = NULL;
	char *eiflabel = NULL;
	int rc;

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "false_if", lblno, &fiflabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_if", lblno, &eiflabel);
	if (rc != EOK)
		goto error;

	/* Condition */

	rc = cgen_expr_rvalue(cgproc, aif->cond, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* jz %<cres>, %false_if */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(fiflabel, &larg);
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

	/* True branch */

	rc = cgen_block(cgproc, aif->tbranch, lblock);
	if (rc != EOK)
		goto error;

	ir_lblock_append(lblock, fiflabel, NULL);
	free(fiflabel);
	fiflabel = NULL;

	/* Else-if branches */

	elsif = ast_if_first(aif);
	while (elsif != NULL) {
		/*
		 * Create false else-if label. Allocate a number every time
		 * since there might be multiple else-if branches.
		 */
		lblno = cgen_new_label_num(cgproc);

		rc = cgen_create_label(cgproc, "false_elseif", lblno, &fiflabel);
		if (rc != EOK)
			goto error;

		/* Else-if condition */
		rc = cgen_expr_rvalue(cgproc, elsif->cond, lblock, &cres);
		if (rc != EOK)
			goto error;

		/* jz %<cres>, %false_elseif */

		rc = ir_instr_create(&instr);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(cres.varname, &carg);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(fiflabel, &larg);
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

		/* Else-if branch */
		rc = cgen_block(cgproc, elsif->ebranch, lblock);
		if (rc != EOK)
			goto error;

		/* jp %end_if */

		rc = ir_instr_create(&instr);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(eiflabel, &larg);
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

		/* False else-if label */
		ir_lblock_append(lblock, fiflabel, NULL);
		free(fiflabel);
		fiflabel = NULL;

		elsif = ast_if_next(elsif);
	}

	/* False branch */

	if (aif->fbranch != NULL) {
		rc = cgen_block(cgproc, aif->fbranch, lblock);
		if (rc != EOK)
			goto error;
	}

	ir_lblock_append(lblock, eiflabel, NULL);

	free(eiflabel);
	return EOK;
error:
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (instr != NULL)
		ir_instr_destroy(instr);
	if (fiflabel != NULL)
		free(fiflabel);
	if (eiflabel != NULL)
		free(eiflabel);
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

/** Generate code for 'for' loop statement.
 *
 * @param cgproc Code generator for procedure
 * @param afor AST for loop statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_for(cgen_proc_t *cgproc, ast_for_t *afor, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	cgen_eres_t ires;
	cgen_eres_t cres;
	cgen_eres_t nres;
	unsigned lblno;
	char *flabel = NULL;
	char *eflabel = NULL;
	int rc;

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "for", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_for", lblno, &eflabel);
	if (rc != EOK)
		goto error;

	/* Loop initialization */

	if (afor->linit != NULL) {
		rc = cgen_expr_rvalue(cgproc, afor->linit, lblock, &ires);
		if (rc != EOK)
			goto error;
	}

	ir_lblock_append(lblock, flabel, NULL);

	/* Condition */

	if (afor->lcond != NULL) {
		rc = cgen_expr_rvalue(cgproc, afor->lcond, lblock, &cres);
		if (rc != EOK)
			goto error;

		/* jz %<cres>, %end_for */

		rc = ir_instr_create(&instr);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(cres.varname, &carg);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(eflabel, &larg);
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
	}

	/* Body */

	rc = cgen_block(cgproc, afor->body, lblock);
	if (rc != EOK)
		goto error;

	/* Loop iteration */

	if (afor->lnext != NULL) {
		rc = cgen_expr_rvalue(cgproc, afor->lnext, lblock, &nres);
		if (rc != EOK)
			goto error;
	}

	/* jp %for */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(flabel, &larg);
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

	ir_lblock_append(lblock, eflabel, NULL);

	free(flabel);
	free(eflabel);
	return EOK;
error:
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (instr != NULL)
		ir_instr_destroy(instr);
	if (flabel != NULL)
		free(flabel);
	if (eflabel != NULL)
		free(eflabel);
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

/** Generate code for declaration statement.
 *
 * @param cgproc Code generator for procedure
 * @param stdecln AST declaration statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_stdecln(cgen_proc_t *cgproc, ast_stdecln_t *stdecln,
    ir_lblock_t *lblock)
{
	ast_node_t *dspec;
	ast_tok_t *atok;
	ast_idlist_entry_t *identry;
	comp_tok_t *tok;
	ast_tok_t *aident;
	comp_tok_t *ident;
	char *vident = NULL;
	ir_lvar_t *lvar;
	scope_member_t *member;
	int rc;

	(void) lblock;

	dspec = ast_dspecs_first(stdecln->dspecs);

	atok = ast_tree_first_tok(dspec);
	tok = (comp_tok_t *) atok->data;

	if (ast_dspecs_next(dspec) != NULL) {
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Multiple declaration specifiers (unimplemented).\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if (dspec->ntype != ant_tsbasic) {
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Type specifier is not basic (unimplemented).\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	// XXX AST could give us the exact basic specifier and we should verify

	identry = ast_idlist_first(stdecln->idlist);
	while (identry != NULL) {
		if (identry->regassign != NULL) {
			tok = (comp_tok_t *) identry->regassign->tasm.data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Variable register assignment (unimplemented).\n");
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		if (identry->aslist != NULL) {
			atok = ast_tree_first_tok(&identry->aslist->node);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Attribute specifier (unimplemented).\n");
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		if (identry->have_init) {
			tok = (comp_tok_t *) identry->tassign.data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Initializer (unimplemented).\n");
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		aident = ast_decl_get_ident(identry->decl);
		ident = (comp_tok_t *) aident->data;

		/* Check for shadowing a wider-scope identifier */
		member = scope_lookup(cgproc->cur_scope->parent,
		    ident->tok.text);
		if (member != NULL) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Declaration of '%s' "
			    "shadows a wider-scope declaration.\n",
			    ident->tok.text);
			++cgproc->cgen->warnings;
		}

		/* Generate an IR variable name */
		rc = cgen_create_loc_var_name(cgproc, ident->tok.text, &vident);
		if (rc != EOK) {
			rc = ENOMEM;
			goto error;
		}

		/* Insert identifier into current scope */
		rc = scope_insert_lvar(cgproc->cur_scope, ident->tok.text,
		    vident);
		if (rc != EOK) {
			if (rc == EEXIST) {
				lexer_dprint_tok(&tok->tok, stderr);
				fprintf(stderr, ": Duplicate identifier '%s'.\n",
				    ident->tok.text);
				cgproc->cgen->error = true; // XXX
				rc = EINVAL;
				goto error;
			}
		}

		rc = ir_lvar_create(vident, &lvar);
		if (rc != EOK)
			goto error;

		free(vident);
		vident = NULL;

		ir_proc_append_lvar(cgproc->irproc, lvar);

		identry = ast_idlist_next(identry);
	}

	return EOK;
error:
	if (vident != NULL)
		free(vident);
	return rc;
}

/** Generate code for null statement.
 *
 * @param cgproc Code generator for procedure
 * @param stnull AST nullstatement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_stnull(cgen_proc_t *cgproc, ast_stnull_t *stnull,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	int rc;

	(void) cgproc;
	(void) stnull;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_nop;
	instr->dest = NULL;
	instr->op1 = NULL;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	return EOK;
error:
	ir_instr_destroy(instr);
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
		rc = cgen_if(cgproc, (ast_if_t *) stmt->ext, lblock);
		break;
	case ant_while:
		rc = cgen_while(cgproc, (ast_while_t *) stmt->ext, lblock);
		break;
	case ant_do:
		rc = cgen_do(cgproc, (ast_do_t *) stmt->ext, lblock);
		break;
	case ant_for:
		rc = cgen_for(cgproc, (ast_for_t *) stmt->ext, lblock);
		break;
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
		rc = cgen_stdecln(cgproc, (ast_stdecln_t *) stmt->ext, lblock);
		break;
	case ant_stnull:
		rc = cgen_stnull(cgproc, (ast_stnull_t *) stmt->ext, lblock);
		break;
	case ant_lmacro:
		atok = ast_tree_first_tok(stmt);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This statement type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_block:
		rc = cgen_gn_block(cgproc, (ast_block_t *) stmt->ext, lblock);
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
	scope_t *block_scope = NULL;
	int rc;

	rc = scope_create(cgproc->cur_scope, &block_scope);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	/* Enter block scope */
	cgproc->cur_scope = block_scope;

	stmt = ast_block_first(block);
	while (stmt != NULL) {
		rc = cgen_stmt(cgproc, stmt, lblock);
		if (rc != EOK)
			return rc;

		stmt = ast_block_next(stmt);
	}

	/* Leave block scope */
	cgproc->cur_scope = block_scope->parent;
	scope_destroy(block_scope);
	return EOK;
error:
	if (block_scope != NULL) {
		/* Leave block scope */
		cgproc->cur_scope = block_scope->parent;
		scope_destroy(block_scope);
	}

	return rc;
}

/** Generate code for gratuitous nested block.
 *
 * @param cgproc Code generator for procedure
 * @param block AST block
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_gn_block(cgen_proc_t *cgproc, ast_block_t *block,
    ir_lblock_t *lblock)
{
	comp_tok_t *tok;

	/* Gratuitous nested block always has braces */
	assert(block->braces);

	tok = (comp_tok_t *) block->topen.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Gratuitous nested block.\n");
	++cgproc->cgen->warnings;

	return cgen_block(cgproc, block, lblock);
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

	rc = ir_proc_create(pident, 0, lblock, &proc);
	if (rc != EOK)
		goto error;

	rc = cgen_proc_create(cgen, proc, &cgproc);
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
		if (rc != EOK) {
			lexer_dprint_tok(&lit->tok, stderr);
			fprintf(stderr, ": Invalid integer literal.\n");
			cgen->error = true; // TODO
			goto error;
		}
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
