/*
 * Copyright 2023 Jiri Svoboda
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
#include <cgtype.h>
#include <ir.h>
#include <labels.h>
#include <lexer.h>
#include <merrno.h>
#include <scope.h>
#include <stdlib.h>
#include <string.h>
#include <symbols.h>

static void cgen_proc_destroy(cgen_proc_t *);
static int cgen_decl(cgen_t *, scope_t *, cgtype_t *, ast_node_t *,
    ast_aslist_t *, cgtype_t **);
static int cgen_sqlist(cgen_t *, scope_t *, ast_sqlist_t *, cgtype_t **);
static int cgen_const_int(cgen_proc_t *, cgtype_elmtype_t, int64_t,
    ir_lblock_t *, cgen_eres_t *);
static void cgen_expr_check_unused(cgen_proc_t *, ast_node_t *,
    cgen_eres_t *);
static int cgen_expr_lvalue(cgen_proc_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_expr_rvalue(cgen_proc_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_expr_promoted_rvalue(cgen_proc_t *, ast_node_t *,
    ir_lblock_t *, cgen_eres_t *);
static int cgen_eres_promoted_rvalue(cgen_proc_t *, cgen_eres_t *,
    ir_lblock_t *, cgen_eres_t *);
static int cgen_uac(cgen_proc_t *, cgen_eres_t *, cgen_eres_t *, ir_lblock_t *,
    cgen_eres_t *, cgen_eres_t *, cgen_uac_flags_t *);
static int cgen_expr2_uac(cgen_proc_t *, ast_node_t *, ast_node_t *,
    ir_lblock_t *, cgen_eres_t *, cgen_eres_t *, cgen_uac_flags_t *);
static int cgen_expr2lr_uac(cgen_proc_t *, ast_node_t *, ast_node_t *,
    ir_lblock_t *lblock, cgen_eres_t *, cgen_eres_t *, cgen_eres_t *,
    cgen_uac_flags_t *);
static int cgen_expr(cgen_proc_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_eres_rvalue(cgen_proc_t *, cgen_eres_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_type_convert(cgen_proc_t *, comp_tok_t *, cgen_eres_t *,
    cgtype_t *, cgen_expl_t, ir_lblock_t *, cgen_eres_t *);
static int cgen_truth_cjmp(cgen_proc_t *, ast_node_t *, bool, const char *,
    ir_lblock_t *);
static int cgen_block(cgen_proc_t *, ast_block_t *, ir_lblock_t *);
static int cgen_gn_block(cgen_proc_t *, ast_block_t *, ir_lblock_t *);
static int cgen_loop_create(cgen_loop_t *, cgen_loop_t **);
static void cgen_loop_destroy(cgen_loop_t *);
static int cgen_switch_create(cgen_switch_t *, cgen_switch_t **);
static void cgen_switch_destroy(cgen_switch_t *);
static int cgen_loop_switch_create(cgen_loop_switch_t *, cgen_loop_switch_t **);
static void cgen_loop_switch_destroy(cgen_loop_switch_t *);
static int cgen_ret(cgen_proc_t *, ir_lblock_t *);
static int cgen_cgtype(cgen_t *, cgtype_t *, ir_texpr_t **);
static int cgen_typedef(cgen_t *, scope_t *, ast_idlist_t *, cgtype_t *);

enum {
	cgen_pointer_bits = 16
};

/** Return the bit width of an arithmetic type.
 *
 * Note that this might depend on the memory model.
 *
 * @param cgen Code generator
 * @param tbasic Basic type
 */
static int cgen_basic_type_bits(cgen_t *cgen, cgtype_basic_t *tbasic)
{
	(void) cgen;

	switch (tbasic->elmtype) {
	case cgelm_char:
	case cgelm_uchar:
		return 8;
	case cgelm_short:
	case cgelm_ushort:
	case cgelm_int:
	case cgelm_uint:
	case cgelm_logic:
		return 16;
	case cgelm_long:
	case cgelm_ulong:
		return 32;
	case cgelm_longlong:
	case cgelm_ulonglong:
		return 64;
	default:
		return 0;
	}
}

/** Return if basic type is signed.
 *
 * @param cgen Code generator
 * @param tbasic Basic type
 * @return @c true iff type is signed
 */
static bool cgen_basic_type_signed(cgen_t *cgen, cgtype_basic_t *tbasic)
{
	(void) cgen;

	switch (tbasic->elmtype) {
	case cgelm_char:
	case cgelm_short:
	case cgelm_int:
	case cgelm_logic:
	case cgelm_long:
	case cgelm_longlong:
		return true;
	case cgelm_uchar:
	case cgelm_ushort:
	case cgelm_uint:
	case cgelm_ulong:
	case cgelm_ulonglong:
		return false;
	default:
		assert(false);
		return false;
	}
}

/** Determine if type is signed.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is an integer type
 */
static bool cgen_type_is_signed(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_basic_t *tbasic;

	(void) cgen;

	assert(cgtype->ntype == cgn_basic);

	tbasic = (cgtype_basic_t *)cgtype->ext;
	return cgen_basic_type_signed(cgen, tbasic);
}

/** Determine if type is an integer type.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is an integer type
 */
static bool cgen_type_is_integer(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_basic_t *tbasic;

	(void) cgen;

	if (cgtype->ntype != cgn_basic) {
		return false;
	}

	tbasic = (cgtype_basic_t *)cgtype->ext;

	switch (tbasic->elmtype) {
	case cgelm_char:
	case cgelm_short:
	case cgelm_int:
	case cgelm_logic:
	case cgelm_long:
	case cgelm_longlong:
	case cgelm_uchar:
	case cgelm_ushort:
	case cgelm_uint:
	case cgelm_ulong:
	case cgelm_ulonglong:
		return true;
	default:
		return false;
	}
}

/** Return the size of a type in bytes.
 *
 * @param cgen Code generator
 * @param tbasic Basic type
 */
static unsigned cgen_type_sizeof(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_basic_t *tbasic;

	switch (cgtype->ntype) {
	case cgn_basic:
		tbasic = (cgtype_basic_t *)cgtype->ext;
		return cgen_basic_type_bits(cgen, tbasic) / 8;
	case cgn_func:
		assert(false);
		return 0;
	case cgn_pointer:
		return cgen_pointer_bits / 8;
	case cgn_record:
		/*
		 * XXX We might want to delegate calculation of record
		 * size to the backend.
		 */
		assert(false);
		return 0;
	}

	assert(false);
	return 0;
}

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
 * @param cgen Code generator
 * @param tlit Literal token
 * @param rval Place to store value
 * @param rtype Place to store elementary type
 * @return EOK on success, EINVAL if token format is invalid
 */
static int cgen_intlit_val(cgen_t *cgen, comp_tok_t *tlit, int64_t *rval,
    cgtype_elmtype_t *rtype)
{
	const char *text = tlit->tok.text;
	cgtype_elmtype_t elmtype;
	bool lunsigned;
	int64_t val;

	val = 0;
	lunsigned = false;

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

	/* Unsigned */
	if (*text == 'u' || *text == 'U') {
		++text;
		lunsigned = true;
	}

	/* Long */
	if (*text == 'l' || *text == 'L') {
		++text;

		/* Long long */
		if (*text == 'l' || *text == 'L') {
			++text;
			elmtype = lunsigned ? cgelm_ulonglong : cgelm_longlong;
		} else {
			elmtype = lunsigned ? cgelm_ulong : cgelm_long;
		}
	} else {
		elmtype = lunsigned ? cgelm_uint : cgelm_int;
	}

	if ((uint64_t)val > 0xffffffffu && elmtype != cgelm_longlong &&
	    elmtype != cgelm_ulonglong) {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Constant should be long long.\n");
		++cgen->warnings;
	} else if ((uint64_t)val > 0xffff && elmtype != cgelm_long &&
	    elmtype != cgelm_ulong && elmtype != cgelm_longlong &&
	    elmtype != cgelm_ulonglong) {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Constant should be long.\n");
		++cgen->warnings;
	}

	if (*text != '\0')
		return EINVAL;

	*rval = val;
	*rtype = elmtype;
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

/** Create new goto label.
 *
 * @param cgproc Code generator for procedure
 * @param ident C goto label identifier
 * @param rlabel Place to store pointer to new label
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_create_goto_label(cgen_proc_t *cgproc, const char *ident,
    char **rlabel)
{
	char *label;
	int rv;

	(void) cgproc;

	/*
	 * XXX Once we are free of the shackles of z80asm, we can change
	 * this to be just %ident (instead of %_ident), because
	 * compiler-generated labels will have the form %name@number,
	 * and C labels cannot contain a '@', so they will be distinct.
	 */
	rv = asprintf(&label, "%%_%s", ident);
	if (rv < 0)
		return ENOMEM;

	*rlabel = label;
	return EOK;
}

/** Initialize expression result.
 *
 * Every variable of type cgen_eres_t must be initialized using this
 * function first.
 *
 * @param eres Expression result to initialize
 */
static void cgen_eres_init(cgen_eres_t *eres)
{
	memset(eres, 0, sizeof(*eres));
}

/** Finalize expression result.
 *
 * Every variable of type cgen_eres_t must be finalized using this function.
 *
 * @param eres Expression result to finalize
 */
static void cgen_eres_fini(cgen_eres_t *eres)
{
	if (eres->cgtype == NULL)
		return;

	cgtype_destroy(eres->cgtype);
	eres->cgtype = NULL;
}

/** Clone expression result.
 *
 * @param res Expression result to copy
 * @param dres Destination expression result
 */
static int cgen_eres_clone(cgen_eres_t *res, cgen_eres_t *dres)
{
	cgtype_t *cgtype;
	int rc;

	rc = cgtype_clone(res->cgtype, &cgtype);
	if (rc != EOK)
		return rc;

	dres->varname = res->varname;
	dres->valtype = res->valtype;
	dres->cgtype = cgtype;
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

	/* Track the procedure's labels */
	rc = labels_create(&cgproc->labels);
	if (rc != EOK)
		goto error;

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
	cgen_proc_destroy(cgproc);
	return rc;
}

/** Destroy code generator for procedure.
 *
 * @param cgproc Code generator for procedure or @c NULL
 */
static void cgen_proc_destroy(cgen_proc_t *cgproc)
{
	if (cgproc == NULL)
		return;

	labels_destroy(cgproc->labels);
	scope_destroy(cgproc->proc_scope);
	scope_destroy(cgproc->arg_scope);
	cgtype_destroy(cgproc->rtype);
	free(cgproc);
}

/** Check scope for defined, but unused, identifiers.
 *
 * @param cgproc Code generator for procedure
 * @param scope Scope to check
 */
static void cgen_check_scope_unused(cgen_proc_t *cgproc, scope_t *scope)
{
	scope_member_t *member;

	member = scope_first(scope);
	while (member != NULL) {
		if (!member->used) {
			lexer_dprint_tok(member->tident, stderr);
			fprintf(stderr, ": Warning: '%s' is defined, but not "
			    "used.\n", member->tident->text);
			++cgproc->cgen->warnings;
		}
		member = scope_next(member);
	}
}

/** Check for used, but undefined and defined, but unused, labels.
 *
 * @param cgproc Code generator for procedure
 * @param labels Labels to check
 * @return EOK on success
 */
static int cgen_check_labels(cgen_proc_t *cgproc, labels_t *labels)
{
	label_t *label;

	label = labels_first(labels);
	while (label != NULL) {
		if (!label->used) {
			lexer_dprint_tok(label->tident, stderr);
			fprintf(stderr, ": Warning: Label '%s' is defined, "
			    "but not used.\n", label->tident->text);
			++cgproc->cgen->warnings;
		}

		if (!label->defined) {
			lexer_dprint_tok(label->tident, stderr);
			fprintf(stderr, ": Undefined label '%s'.\n",
			    label->tident->text);
			cgproc->cgen->error = true; // TODO
			return EINVAL;
		}

		label = labels_next(label);
	}

	return EOK;
}

/** Get the position at which declaration specifier should appear.
 *
 * @param dspec Declaration specifier
 * @return Order (0 - 4) relative to other declaration specifiers
 */
static int cgen_dspec_get_order(ast_node_t *dspec)
{
	switch (dspec->ntype) {
	case ant_sclass:
		/* Storace class specifier */
		return 0;
	case ant_tqual:
		/* Type qualifier */
		return 1;
	case ant_fspec:
		/* Function specifier */
		return 2;
	case ant_aspec:
		/* Attribute specifier */
		return 3;
	case ant_tsident:
	case ant_tsatomic:
	case ant_tsrecord:
	case ant_tsenum:
	case ant_tsbasic:
		/* Type specifier */
		return 4;
	default:
		assert(false);
		return -1;
	}
}

/** Get the position at which type qualifier should appear.
 *
 * @param dspec Type qualifier
 * @return Order (0 - 4) relative to other type qualifiers
 */
static int cgen_tqual_get_order(ast_tqual_t *a)
{
	switch (a->qtype) {
	case aqt_const:
		return 0;
	case aqt_restrict:
		return 1;
	case aqt_volatile:
		return 2;
	case aqt_atomic:
		return 3;
	}

	assert(false);
	return -1;
}

/** Warn if type qualifiers are not in the preferred order.
 *
 * @param cgproc Code generator
 * @param a First type qualifier
 * @param b Second type qualifier
 */
static void cgen_tqual_check_order(cgen_t *cgen, ast_tqual_t *a, ast_tqual_t *b)
{
	comp_tok_t *catok;
	comp_tok_t *cbtok;
	int oa, ob;

	oa = cgen_tqual_get_order(a);
	ob = cgen_tqual_get_order(b);
	if (oa > ob) {
		catok = (comp_tok_t *) a->tqual.data;
		cbtok = (comp_tok_t *) b->tqual.data;
		lexer_dprint_tok(&cbtok->tok, stderr);
		fprintf(stderr, ": Warning: '%s' should come before '%s'.\n",
		    cbtok->tok.text, catok->tok.text);
		++cgen->warnings;
	}
}

/** Get the position at which type specifier should appear.
 *
 * @param dspec Type specifier
 * @return Order (0 - 4) relative to other type specifiers
 */
static int cgen_tspec_get_order(ast_node_t *tspec)
{
	ast_tsbasic_t *tsbasic;

	switch (tspec->ntype) {
	case ant_tsbasic:
		tsbasic = (ast_tsbasic_t *) tspec->ext;

		switch (tsbasic->btstype) {
		case abts_signed:
		case abts_unsigned:
			return 0;
		case abts_long:
		case abts_short:
			return 1;
		case abts_void:
		case abts_char:
		case abts_int:
		case abts_int128:
		case abts_float:
		case abts_double:
			return 2;
		}
		break;

	case ant_tsident:
	case ant_tsatomic:
	case ant_tsrecord:
	case ant_tsenum:
	default:
		return 2;
	}

	assert(false);
	return -1;
}

/** Warn if type specifiers are not in the preferred order.
 *
 * @param cgproc Code generator
 * @param a First type specifier
 * @param b Second type specifier
 */
static void cgen_tspec_check_order(cgen_t *cgen, ast_node_t *a, ast_node_t *b)
{
	ast_tok_t *atok;
	ast_tok_t *btok;
	comp_tok_t *catok;
	comp_tok_t *cbtok;
	int oa, ob;

	oa = cgen_tspec_get_order(a);
	ob = cgen_tspec_get_order(b);

	if (oa > ob) {
		atok = ast_tree_first_tok(a);
		catok = (comp_tok_t *) atok->data;
		btok = ast_tree_first_tok(b);
		cbtok = (comp_tok_t *) btok->data;
		lexer_dprint_tok(&cbtok->tok, stderr);
		fprintf(stderr, ": Warning: '%s' should come before '%s'.\n",
		    cbtok->tok.text, catok->tok.text);
		++cgen->warnings;
	}
}

/** Warn if declaration specifiers are not in the preferred order.
 *
 * For some obscure reason the C standard allows specifiers to come
 * in any order. On the other hand, most of C programmers tend to
 * always follow the same order. Thus the parser has made effort to parse
 * any order of declaration specifiers and now we meticulously verify
 * that they are in one, specific, order. If they are not, we produce
 * warnings.
 *
 * @param cgproc Code generator
 * @param a First declaration specifier
 * @param b Second declaration specifier
 */
static void cgen_dspec_check_order(cgen_t *cgen, ast_node_t *a, ast_node_t *b)
{
	ast_tok_t *atok;
	ast_tok_t *btok;
	comp_tok_t *catok;
	comp_tok_t *cbtok;
	int oa, ob;

	oa = cgen_dspec_get_order(a);
	ob = cgen_dspec_get_order(b);

	if (oa != ob) {
		if (oa > ob) {
			atok = ast_tree_first_tok(a);
			catok = (comp_tok_t *) atok->data;
			btok = ast_tree_first_tok(b);
			cbtok = (comp_tok_t *) btok->data;
			lexer_dprint_tok(&cbtok->tok, stderr);
			fprintf(stderr, ": Warning: '%s' should come before '%s'.\n",
			    cbtok->tok.text, catok->tok.text);
			++cgen->warnings;
		}

		return;
	}

	/* Declaration specifiers are of the same class */

	/* Two type qualifiers */
	if (a->ntype == ant_tqual) {
		assert(b->ntype == ant_tqual);
		cgen_tqual_check_order(cgen, (ast_tqual_t *)a->ext,
		    (ast_tqual_t *)b->ext);
	}

	if (oa == 4 && ob == 4) {
		/* Two type specifiers */
		cgen_tspec_check_order(cgen, a, b);
	}
}

/** Generate error: multiple type specifiers.
 *
 * @param cgen Code generator
 * @param prev Previous type specifier
 * @param cur Current type specifier
 */
static void cgen_error_multiple_tspecs(cgen_t *cgen, ast_node_t *prev,
    ast_node_t *cur)
{
	ast_tok_t *atok1;
	ast_tok_t *atok2;
	comp_tok_t *tok1;
	comp_tok_t *tok2;

	atok1 = ast_tree_first_tok(prev);
	atok2 = ast_tree_first_tok(cur);
	tok1 = (comp_tok_t *) atok1->data;
	tok2 = (comp_tok_t *) atok2->data;

	lexer_dprint_tok(&tok2->tok, stderr);
	fprintf(stderr, ": Multiple type specifiers ('%s', '%s').\n",
	    tok1->tok.text, tok2->tok.text);

	cgen->error = true; // TODO
}

/** Generate error: multiple short specifiers.
 *
 * @param cgen Code generator
 * @param tsshort Current short specifier
 */
static void cgen_error_multiple_short(cgen_t *cgen, ast_tsbasic_t *tsshort)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tsshort->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": More than one short specifier.\n");

	cgen->error = true; // TODO
}

/** Generate error: more than two long specifiers.
 *
 * @param cgen Code generator
 * @param tslong Current long specifier
 */
static void cgen_error_many_long(cgen_t *cgen, ast_tsbasic_t *tslong)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tslong->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": More than two long specifiers.\n");

	cgen->error = true; // TODO
}

/** Generate error: both short and long specifier.
 *
 * @param cgen Code generator
 * @param tspec Current short/long specifier
 */
static void cgen_error_short_long(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both short and long specifier.\n");

	cgen->error = true; // TODO
}

/** Generate error: both short and char specifier.
 *
 * @param cgen Code generator
 * @param tspec Char specifier
 */
static void cgen_error_short_char(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both short and char specifier.\n");

	cgen->error = true; // TODO
}

/** Generate error: both long and char specifier.
 *
 * @param cgen Code generator
 * @param tspec Char specifier
 */
static void cgen_error_long_char(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both long and char specifier.\n");

	cgen->error = true; // TODO
}

/** Generate error: multiple signed specifiers.
 *
 * @param cgen Code generator
 * @param tssigned Current signed specifier
 */
static void cgen_error_multiple_signed(cgen_t *cgen, ast_tsbasic_t *tssigned)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tssigned->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": More than one signed specifier.\n");

	cgen->error = true; // TODO
}

/** Generate error: multiple unsigned specifiers.
 *
 * @param cgen Code generator
 * @param tsunsigned Current unsigned specifier
 */
static void cgen_error_multiple_unsigned(cgen_t *cgen,
    ast_tsbasic_t *tsunsigned)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tsunsigned->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": More than one unsigned specifier.\n");

	cgen->error = true; // TODO
}

/** Generate error: both signed and unsigned specifier.
 *
 * @param cgen Code generator
 * @param tspec Current signed/unsigned specifier
 */
static void cgen_error_signed_unsigned(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both signed and unsigned specifier.\n");

	cgen->error = true; // TODO
}

static void cgen_warn_tspec_not_impl(cgen_t *cgen, ast_node_t *tspec)
{
	ast_tok_t *atok;
	comp_tok_t *tok;

	atok = ast_tree_first_tok(tspec);
	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Unimplemented type specifier.\n");
	++cgen->warnings;
}

/** Generate warning: int is superfluous.
 *
 * @param cgen Code generator
 * @param tspec Char specifier
 */
static void cgen_warn_int_superfluous(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": superfluous 'int' used with short/long/signed/unsigned.\n");

	++cgen->warnings;
}

/** Generate code for identifier type specifier.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param tsident Identifier type specifier
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_tsident(cgen_t *cgen, scope_t *scope, ast_tsident_t *tsident,
    cgtype_t **rstype)
{
	comp_tok_t *ident;
	scope_member_t *member;
	int rc;

	ident = (comp_tok_t *)tsident->tident.data;

	/* Check if the type is defined */
	member = scope_lookup(scope, ident->tok.text);
	if (member == NULL) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Undefined type name '%s'.\n",
		    ident->tok.text);
		cgen->error = true; // TODO
		return EINVAL;
	}

	/* Resulting type is the same as type of the member */
	rc = cgtype_clone(member->cgtype, rstype);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Generate code for record type specifier element.
 *
 * In outher words, one field of a struct or union definition.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param elem Record type specifier element
 * @param member Scope member (that is a record)
 * @return EOK on success or an error code
 */
static int cgen_tsrecord_elem(cgen_t *cgen, scope_t *scope,
    ast_tsrecord_elem_t *elem, scope_member_t *member)
{
	ast_dlist_entry_t *dlentry;
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	comp_tok_t *ctok;
	int rc;

	(void)member;

	/* When compiling we should not get a macro declaration */
	assert(elem->mdecln == NULL);

	rc = cgen_sqlist(cgen, scope, elem->sqlist, &stype);
	if (rc != EOK)
		goto error;

	dlentry = ast_dlist_first(elem->dlist);
	while (dlentry != NULL) {
		rc = cgen_decl(cgen, scope, stype, dlentry->decl,
		    dlentry->aslist, &dtype);
		if (rc != EOK)
			goto error;

		aident = ast_decl_get_ident(dlentry->decl);
		ident = (comp_tok_t *) aident->data;

		if (dlentry->have_bitwidth) {
			ctok = (comp_tok_t *)dlentry->tcolon.data;
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Unimplemented bit field.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		assert(member->mtype == sm_record);

		rc = scope_member_record_append(&member->m.record,
		    ident->tok.text, dtype);
		if (rc == EEXIST) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Duplicate record member '%s'.\n",
			    ident->tok.text);
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}
		if (rc != EOK)
			goto error;

		cgtype_destroy(dtype);
		dtype = NULL;

		dlentry = ast_dlist_next(dlentry);
	}

	cgtype_destroy(stype);
	return EOK;
error:
	cgtype_destroy(stype);
	cgtype_destroy(dtype);
	return rc;
}

/** Generate code for record type specifier.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param tsrecord Record type specifier
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_tsrecord(cgen_t *cgen, scope_t *scope, ast_tsrecord_t *tsrecord,
    cgtype_t **rstype)
{
	comp_tok_t *ident;
	scope_member_t *member;
	ast_tok_t *tok;
	comp_tok_t *ctok;
	cgtype_basic_t *btype;
	const char *rtype;
	scope_rec_type_t srtype;
	ast_tsrecord_elem_t *elem;
	int rc;

	(void)scope;
	(void)rstype;

	if (tsrecord->rtype == ar_struct) {
		rtype = "struct";
		srtype = sr_struct;
	} else {
		rtype = "union";
		srtype = sr_union;
	}

	if (tsrecord->aslist1 != NULL) {
		tok = ast_tree_first_tok(&tsrecord->aslist1->node);
		ctok = (comp_tok_t *)tok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented attribute specifier "
		    "in this context.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	if (tsrecord->have_ident)
		ident = (comp_tok_t *)tsrecord->tident.data;

	if (!tsrecord->have_ident) {
		ctok = (comp_tok_t *)tsrecord->tsu.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented anonymous struct/union.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	if (!tsrecord->have_def) {
		ctok = (comp_tok_t *)tsrecord->tsu.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented struct/union usage.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	if (tsrecord->have_def && scope->parent != NULL) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Definition of '%s %s' in a non-global "
		    "scope.\n", rtype, ident->tok.text);
		++cgen->warnings;

		member = scope_lookup_tag(scope->parent, ident->tok.text);
		if (member != NULL) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Definition of '%s %s' shadows "
			    "a wider-scope struct/union definition.\n", rtype,
			    ident->tok.text);
			++cgen->warnings;
		}
	}

	if (tsrecord->have_def && cgen->tsrec_cnt > 0) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Definition of '%s %s' inside another "
		    "struct/union definition.\n", rtype, ident->tok.text);
		++cgen->warnings;
	}

	member = scope_lookup_tag_local(scope, ident->tok.text);
	if (member != NULL) {
		lexer_dprint_tok(member->tident, stderr);
		fprintf(stderr, ": Redefinition of '%s %s'.\n",
		    rtype, member->tident->text);
		cgen->error = true; // TODO
		return EINVAL;
	}

	if (tsrecord->aslist2 != NULL) {
		tok = ast_tree_first_tok(&tsrecord->aslist2->node);
		ctok = (comp_tok_t *)tok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented attribute specifier "
		    "in this context.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	/* Insert new record definition */
	rc = scope_insert_record(scope, &ident->tok, srtype, &member);
	if (rc != EOK)
		return EINVAL;

	++cgen->tsrec_cnt;
	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		rc = cgen_tsrecord_elem(cgen, scope, elem, member);
		if (rc != EOK)  {
			assert(cgen->tsrec_cnt > 0);
			--cgen->tsrec_cnt;
			return rc;
		}

		elem = ast_tsrecord_next(elem);
	}
	assert(cgen->tsrec_cnt > 0);
	--cgen->tsrec_cnt;

	/* Resulting type is the same as type of the member */
	rc = cgtype_basic_create(cgelm_int, &btype);
	if (rc != EOK)
		return rc;

	*rstype = &btype->cgtype;
	return EOK;
}

/** Initialize code generator for declaration speciers.
 *
 * This structure holds state for processing declaration specifiers
 * or specifier-qualifier list.
 *
 * @param cgen Code generator
 * @param cgds Code generator for declaration specifiers
 */
static void cgen_dspec_init(cgen_t *cgen, cgen_dspec_t *cgds)
{
	cgds->cgen = cgen;

	/* There should be exactly one type specifier. Track it in tspec. */
	cgds->tspec = NULL;
	cgds->short_cnt = 0;
	cgds->long_cnt = 0;
	cgds->signed_cnt = 0;
	cgds->unsigned_cnt = 0;
	cgds->sctype = asc_none;
}

/** Generate code for declaratio specifier / specifier-qualifier.
 *
 * Declaration specifiers declare the base type which is then further modified
 * by the declarator(s).
 *
 * @param cgds Code generator for declaration specifier
 * @param dspec Declaration specifier
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_dspec(cgen_dspec_t *cgds, ast_node_t *dspec)
{
	ast_tok_t *atok;
	ast_tsbasic_t *tsbasic;
	ast_sclass_t *sclass;
	comp_tok_t *tok;

	switch (dspec->ntype) {
	case ant_tsbasic:
		tsbasic = (ast_tsbasic_t *) dspec->ext;
		switch (tsbasic->btstype) {
		case abts_short:
			if (cgds->short_cnt > 0) {
				cgen_error_multiple_short(cgds->cgen, tsbasic);
				return EINVAL;
			}
			if (cgds->long_cnt > 0) {
				cgen_error_short_long(cgds->cgen, tsbasic);
				return EINVAL;
			}
			++cgds->short_cnt;
			break;
		case abts_long:
			if (cgds->long_cnt > 1) {
				cgen_error_many_long(cgds->cgen, tsbasic);
				return EINVAL;
			}
			if (cgds->short_cnt > 0) {
				cgen_error_short_long(cgds->cgen, tsbasic);
				return EINVAL;
			}
			++cgds->long_cnt;
			break;
		case abts_signed:
			if (cgds->signed_cnt > 0) {
				cgen_error_multiple_signed(cgds->cgen, tsbasic);
				return EINVAL;
			}
			if (cgds->unsigned_cnt > 0) {
				cgen_error_signed_unsigned(cgds->cgen, tsbasic);
				return EINVAL;
			}
			++cgds->signed_cnt;
			break;
		case abts_unsigned:
			if (cgds->unsigned_cnt > 0) {
				cgen_error_multiple_unsigned(cgds->cgen, tsbasic);
				return EINVAL;
			}
			if (cgds->signed_cnt > 0) {
				cgen_error_signed_unsigned(cgds->cgen, tsbasic);
				return EINVAL;
			}
			++cgds->unsigned_cnt;
			break;
		default:
			if (cgds->tspec != NULL) {
				/* More than one type specifier */
				cgen_error_multiple_tspecs(cgds->cgen, cgds->tspec,
				    dspec);
				return EINVAL;
			}

			cgds->tspec = dspec;
			break;
		}
		break;
	case ant_tsident:
	case ant_tsatomic:
	case ant_tsrecord:
	case ant_tsenum:
		if (cgds->tspec != NULL) {
			/* More than one type specifier */
			cgen_error_multiple_tspecs(cgds->cgen, cgds->tspec, dspec);
			return EINVAL;
		}

		cgds->tspec = dspec;
		break;
	case ant_sclass:
		/* Storage class specifier */
		sclass = (ast_sclass_t *)dspec->ext;
		/*
		 * Multiple storage classes should have been caught by
		 * the parser.
		 */
		assert(cgds->sctype == asc_none);
		cgds->sctype = sclass->sctype;
		break;
	default:
		atok = ast_tree_first_tok(dspec);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Unimplemented declaration specifier.\n");
		++cgds->cgen->warnings;
		break;
	}

	return EOK;
}

/** Finish up generating code for declaration specifiers / specifier-qualifier
 * list.
 *
 * @param cgds Code generator for declaration specifiers
 * @param scope Current scope
 * @param rsctype Place to store storage class type
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_dspec_finish(cgen_dspec_t *cgds, scope_t *scope,
    ast_sclass_type_t *rsctype, cgtype_t **rstype)
{
	cgen_t *cgen = cgds->cgen;
	ast_tok_t *atok;
	ast_tsbasic_t *tsbasic;
	comp_tok_t *tok;
	cgtype_elmtype_t elmtype;
	cgtype_basic_t *btype = NULL;
	cgtype_t *stype;
	int rc;

	if (cgds->tspec != NULL) {
		/* Process type specifier */
		switch (cgds->tspec->ntype) {
		case ant_tsbasic:
			tsbasic = (ast_tsbasic_t *) cgds->tspec->ext;
			switch (tsbasic->btstype) {
			case abts_char:
				if (cgds->unsigned_cnt > 0)
					elmtype = cgelm_uchar;
				else
					elmtype = cgelm_char;

				if (cgds->short_cnt > 0) {
					cgen_error_short_char(cgen, tsbasic);
					return EINVAL;
				}
				if (cgds->long_cnt > 0) {
					cgen_error_long_char(cgen, tsbasic);
					return EINVAL;
				}
				break;
			case abts_int:
				elmtype = cgelm_int;
				break;
			case abts_void:
				elmtype = cgelm_void;
				break;
			default:
				cgen_warn_tspec_not_impl(cgen, cgds->tspec);
				elmtype = cgelm_int;
				break;
			}

			if (elmtype == cgelm_int) {
				if (cgds->unsigned_cnt > 0) {
					if (cgds->long_cnt > 1)
						elmtype = cgelm_ulonglong;
					else if (cgds->long_cnt > 0)
						elmtype = cgelm_ulong;
					else if (cgds->short_cnt > 0)
						elmtype = cgelm_ushort;
					else
						elmtype = cgelm_uint;
				} else {
					if (cgds->long_cnt > 1)
						elmtype = cgelm_longlong;
					else if (cgds->long_cnt > 0)
						elmtype = cgelm_long;
					else if (cgds->short_cnt > 0)
						elmtype = cgelm_short;
					else
						elmtype = cgelm_int;
				}

				/*
				 * Style: If there is any other specifier
				 * than int (signed, unsigned, short, long),
				 * then int is superfluous.
				 */
				if (cgds->long_cnt > 0 || cgds->short_cnt > 0 ||
				    cgds->signed_cnt > 0 || cgds->unsigned_cnt > 0) {
					cgen_warn_int_superfluous(cgen,
					    tsbasic);
				}
			}

			rc = cgtype_basic_create(elmtype, &btype);
			if (rc != EOK)
				goto error;

			stype = &btype->cgtype;
			break;
		case ant_tsident:
			rc = cgen_tsident(cgen, scope,
			    (ast_tsident_t *)cgds->tspec->ext, &stype);
			if (rc != EOK)
				goto error;
			break;
		case ant_tsrecord:
			rc = cgen_tsrecord(cgen, scope,
			    (ast_tsrecord_t *)cgds->tspec->ext, &stype);
			if (rc != EOK)
				goto error;
			break;
		default:
			stype = NULL;
			atok = ast_tree_first_tok(cgds->tspec);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Unimplemented type specifier.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}
	} else {
		/* Default to int */

		if (cgds->unsigned_cnt > 0) {
			if (cgds->long_cnt > 1)
				elmtype = cgelm_ulonglong;
			else if (cgds->long_cnt > 0)
				elmtype = cgelm_ulong;
			else if (cgds->short_cnt > 0)
				elmtype = cgelm_ushort;
			else
				elmtype = cgelm_uint;
		} else {
			if (cgds->long_cnt > 1)
				elmtype = cgelm_longlong;
			else if (cgds->long_cnt > 0)
				elmtype = cgelm_long;
			else if (cgds->short_cnt > 0)
				elmtype = cgelm_short;
			else
				elmtype = cgelm_int;
		}

		rc = cgtype_basic_create(elmtype, &btype);
		if (rc != EOK)
			goto error;

		stype = &btype->cgtype;
	}

	*rsctype = cgds->sctype;
	*rstype = stype;
	return EOK;
error:
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for declaration specifiers.
 *
 * Declaration specifiers declare the base type which is then further modified
 * by the declarator(s).
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param dspecs Declaration specifiers
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_dspecs(cgen_t *cgen, scope_t *scope, ast_dspecs_t *dspecs,
    ast_sclass_type_t *rsctype, cgtype_t **rstype)
{
	ast_node_t *dspec;
	ast_node_t *prev;
	cgen_dspec_t cgds;
	int rc;

	/* Initialize dspec tracking structure. */
	cgen_dspec_init(cgen, &cgds);

	dspec = ast_dspecs_first(dspecs);
	prev = NULL;
	while (dspec != NULL) {
		/* Coding style requires specifiers to be in a certain order. */
		if (prev != NULL)
			cgen_dspec_check_order(cgen, prev, dspec);

		/* Process declaration specifier */
		rc = cgen_dspec(&cgds, dspec);
		if (rc != EOK)
			return rc;

		prev = dspec;
		dspec = ast_dspecs_next(dspec);
	}

	return cgen_dspec_finish(&cgds, scope, rsctype, rstype);
}

/** Generate code for specifier-qualifier list.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param sqlist Specifier-qualifier list
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_sqlist(cgen_t *cgen, scope_t *scope, ast_sqlist_t *sqlist,
    cgtype_t **rstype)
{
	ast_node_t *dspec;
	ast_node_t *prev;
	ast_sclass_type_t sctype;
	cgen_dspec_t cgds;
	int rc;

	/* Initialize dspec tracking structure. */
	cgen_dspec_init(cgen, &cgds);

	dspec = ast_sqlist_first(sqlist);
	prev = NULL;
	while (dspec != NULL) {
		/* Coding style requires specifiers to be in a certain order. */
		if (prev != NULL)
			cgen_dspec_check_order(cgen, prev, dspec);

		/* Process specifier-qualifier list entry */
		rc = cgen_dspec(&cgds, dspec);
		if (rc != EOK)
			return rc;

		prev = dspec;
		dspec = ast_sqlist_next(dspec);
	}

	rc = cgen_dspec_finish(&cgds, scope, &sctype, rstype);
	if (rc != EOK)
		return rc;

	assert(sctype == asc_none);
	return EOK;
}

/** Generate code for identifier declarator.
 *
 * Base type (@a stype) determined by the declaration specifiers is
 * further modified by the declarator and returned as @a *rdtype.
 *
 * @param cgen Code generator
 * @param stype Type derived from declaration specifiers
 * @param dident Identifier declarator
 * @param aslist Attribute specifier list or @c NULL
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl_ident(cgen_t *cgen, cgtype_t *stype, ast_dident_t *dident,
    ast_aslist_t *aslist, cgtype_t **rdtype)
{
	cgtype_t *dtype;
	ast_tok_t *tok;
	comp_tok_t *ctok;
	int rc;

	(void)cgen;
	(void)dident;

	if (aslist != NULL) {
		tok = ast_tree_first_tok(&aslist->node);
		ctok = (comp_tok_t *)tok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented attribute specifier "
		    "in this context.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	rc = cgtype_clone(stype, &dtype);
	if (rc != EOK)
		return rc;

	*rdtype = dtype;
	return EOK;
}

/** Generate code for function declarator.
 *
 * Base type (@a stype) determined by the declaration specifiers is
 * further modified by the declarator and returned as @a *rdtype.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param btype Type derived from declaration specifiers
 * @param dfun Function declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl_fun(cgen_t *cgen, scope_t *scope, cgtype_t *btype,
    ast_dfun_t *dfun, ast_aslist_t *aslist, cgtype_t **rdtype)
{
	cgtype_func_t *func = NULL;
	ast_dfun_arg_t *arg;
	ast_tok_t *atok;
	comp_tok_t *tok;
	cgtype_t *btype_copy = NULL;
	cgtype_t *stype = NULL;
	cgtype_t *atype = NULL;
	cgtype_basic_t *abasic;
	ast_aspec_t *aspec;
	ast_aspec_attr_t *attr;
	ast_sclass_type_t sctype;
	bool have_args = false;
	int rc;

	rc = cgtype_clone(btype, &btype_copy);
	if (rc != EOK)
		goto error;

	rc = cgtype_func_create(btype_copy, &func);
	if (rc != EOK)
		goto error;

	btype_copy = NULL; /* ownership transferred */

	arg = ast_dfun_first(dfun);
	while (arg != NULL) {
		rc = cgen_dspecs(cgen, scope, arg->dspecs, &sctype, &stype);
		if (rc != EOK)
			goto error;

		if (sctype != asc_none) {
			atok = ast_tree_first_tok(&arg->dspecs->node);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Unimplemented storage class specifier.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		rc = cgen_decl(cgen, scope, stype, arg->decl, arg->aslist,
		    &atype);
		if (rc != EOK)
			goto error;

		/* Check for 'void' being the only parameter */
		if (stype->ntype == cgn_basic) {
			abasic = (cgtype_basic_t *)atype->ext;

			if (abasic->elmtype == cgelm_void &&
			    arg->decl->ntype == ant_dnoident) {
				if ((ast_dfun_next(arg) != NULL || arg != ast_dfun_first(dfun))) {
					atok = ast_tree_first_tok(&arg->dspecs->node);
					tok = (comp_tok_t *) atok->data;
					lexer_dprint_tok(&tok->tok, stderr);
					fprintf(stderr, ": 'void' must be the only parameter.\n");
					cgen->error = true; // XXX
					rc = EINVAL;
					goto error;
				}

				cgtype_destroy(stype);
				stype = NULL;
				cgtype_destroy(atype);
				atype = NULL;
				break;
			}
		}

		if (arg->aslist != NULL) {
			atok = ast_tree_first_tok(&arg->aslist->node);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Attribute specifier (unimplemented).\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = cgtype_func_append_arg(func, atype);
		if (rc != EOK)
			goto error;

		have_args = true;

		atype = NULL; /* ownership transferred */

		cgtype_destroy(stype);
		stype = NULL;

		arg = ast_dfun_next(arg);
	}

	/* Function attributes */
	if (aslist != NULL) {
		aspec = ast_aslist_first(aslist);
		while (aspec != NULL) {
			attr = ast_aspec_first(aspec);
			while (attr != NULL) {
				tok = (comp_tok_t *) attr->tname.data;
				if (strcmp(tok->tok.text, "usr") == 0) {
					if (attr->have_params) {
						tok = (comp_tok_t *) attr->tlparen.data;

						lexer_dprint_tok(&tok->tok, stderr);
						fprintf(stderr, ": Attribute 'usr' should not have any "
						    "arguments.\n");
						cgen->error = true; // XXX
						return EINVAL;
					}

					if (have_args) {
						tok = (comp_tok_t *) attr->tname.data;

						lexer_dprint_tok(&tok->tok, stderr);
						fprintf(stderr, ": User service routine cannot "
						    "have any arguments.\n");
						cgen->error = true; // XXX
						return EINVAL;
					}

					/* User service routine */
					func->cconv = cgcc_usr;
				} else {
					lexer_dprint_tok(&tok->tok, stderr);
					fprintf(stderr, ": Unknown attribute '%s'.\n",
					    tok->tok.text);
					cgen->error = true; // XXX
					return EINVAL;
				}

				attr = ast_aspec_next(attr);
			}

			aspec = ast_aslist_next(aspec);
		}
	}

	*rdtype = &func->cgtype;
	return EOK;
error:
	if (stype != NULL)
		cgtype_destroy(stype);
	if (atype != NULL)
		cgtype_destroy(atype);
	if (func != NULL)
		cgtype_destroy(&func->cgtype);
	if (btype_copy != NULL)
		cgtype_destroy(btype_copy);
	return rc;
}

/** Generate code for pointer declarator.
 *
 * Base type (@a stype) determined by the declaration specifiers is
 * further modified by the declarator and returned as @a *rdtype.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param btype Type derived from declaration specifiers
 * @param dptr Pointer declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl_ptr(cgen_t *cgen, scope_t *scope, cgtype_t *btype,
    ast_dptr_t *dptr, ast_aslist_t *aslist, cgtype_t **rdtype)
{
	cgtype_pointer_t *ptrtype;
	cgtype_t *btype_copy = NULL;
	int rc;

	(void)cgen;
	(void)dptr;

	rc = cgtype_clone(btype, &btype_copy);
	if (rc != EOK)
		goto error;

	rc = cgtype_pointer_create(btype_copy, &ptrtype);
	if (rc != EOK)
		goto error;

	rc = cgen_decl(cgen, scope, &ptrtype->cgtype, dptr->bdecl, aslist,
	    rdtype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(&ptrtype->cgtype);
	return EOK;
error:
	cgtype_destroy(btype_copy);
	return rc;
}

/** Generate code for declarator.
 *
 * Base type (@a stype) determined by the declaration specifiers is
 * further modified by the declarator and returned as @a *rdtype.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param stype Type derived from declaration specifiers
 * @param decl Declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl(cgen_t *cgen, scope_t *scope, cgtype_t *stype,
    ast_node_t *decl, ast_aslist_t *aslist, cgtype_t **rdtype)
{
	cgtype_t *dtype;
	int rc;

	switch (decl->ntype) {
	case ant_dident:
	case ant_dnoident:
		rc = cgen_decl_ident(cgen, stype, (ast_dident_t *) decl->ext,
		    aslist, &dtype);
		break;
	case ant_dfun:
		rc = cgen_decl_fun(cgen, scope, stype, (ast_dfun_t *) decl->ext,
		    aslist, &dtype);
		break;
	case ant_dptr:
		rc = cgen_decl_ptr(cgen, scope, stype, (ast_dptr_t *) decl->ext,
		    aslist, &dtype);
		break;
	default:
		printf("[cgen_decl] Unimplemented declarator type.\n");
		return ENOTSUP;
	}

	if (rc != EOK)
		return rc;

	*rdtype = dtype;
	return EOK;
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
	int64_t val;
	cgtype_elmtype_t elmtype;
	int rc;

	lit = (comp_tok_t *) eint->tlit.data;
	rc = cgen_intlit_val(cgproc->cgen, lit, &val, &elmtype);
	if (rc != EOK) {
		lexer_dprint_tok(&lit->tok, stderr);
		fprintf(stderr, ": Invalid integer literal.\n");
		cgproc->cgen->error = true; // TODO
		return rc;
	}

	return cgen_const_int(cgproc, elmtype, val, lblock, eres);
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
	eres->cgtype = NULL;

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
	eres->cgtype = NULL;
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
	eres->cgtype = NULL;

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
	cgtype_t *cgtype = NULL;
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

	/* Resulting type is the same as type of the member */
	rc = cgtype_clone(member->cgtype, &cgtype);
	if (rc != EOK)
		return rc;

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
	case sm_record:
		/*
		 * Should not happen - call to scope lookup above cannot
		 * match a record tag.
		 */
		assert(false);
		return EINVAL;
		break;
	case sm_tdef:
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Expected variable name. '%s' is a type.\n",
		    ident->tok.text);
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	if (rc != EOK)
		return rc;

	/* Mark identifier as used */
	member->used = true;

	eres->cgtype = cgtype;
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

/** Generate code to return integer constant.
 *
 * @param cgproc Code generator for procedure
 * @param elmtype Elementary type
 * @param val Value
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_const_int(cgen_proc_t *cgproc, cgtype_elmtype_t elmtype,
    int64_t val, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_imm_t *imm = NULL;
	cgtype_basic_t *btype = NULL;
	int rc;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(val, &imm);
	if (rc != EOK)
		goto error;

	rc = cgtype_basic_create(elmtype, &btype);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgen_basic_type_bits(cgproc->cgen, btype);
	instr->dest = &dest->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for addition of integers.
 *
 * @param cgproc Code generator for procedure
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_add_int(cgen_proc_t *cgproc, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgtype_t *cgtype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	/* Perform usual arithmetic conversion */
	rc = cgen_uac(cgproc, lres, rres, lblock, &res1, &res2, &flags);
	if (rc != EOK)
		goto error;

	/* Unsigned addition of mixed-signed numbers is OK */
	(void)flags;

	/* Check the type */
	if (res1.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)res1.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(res1.cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(res1.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(res2.varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_add;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);

	return EOK;
error:
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for addition of pointer and integer.
 *
 * @param cgproc Code generator for procedure
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_add_ptr_int(cgen_proc_t *cgproc, comp_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t cres;
	cgtype_t *idxtype = NULL;
	cgtype_t *cgtype = NULL;
	ir_texpr_t *elemte = NULL;
	cgtype_pointer_t *ptrt;
	int rc;

	cgen_eres_init(&cres);

	rc = cgtype_int_construct(false, cgir_int, &idxtype);
	if (rc != EOK)
		goto error;

	/* Convert index to be same size as pointer */
	rc = cgen_type_convert(cgproc, optok, rres, idxtype,
	    cgen_implicit, lblock, &cres);
	if (rc != EOK)
		goto error;

	cgtype_destroy(idxtype);
	idxtype = NULL;

	/* Check the type */
	assert(lres->cgtype->ntype == cgn_pointer);
	ptrt = (cgtype_pointer_t *)lres->cgtype->ext;

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	/* Generate IR type expression for the element type */
	rc = cgen_cgtype(cgproc->cgen, ptrt->tgtype, &elemte);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_ptridx;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;
	instr->opt = elemte;

	elemte = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	cgen_eres_fini(&cres);

	return EOK;
error:
	cgen_eres_fini(&cres);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	cgtype_destroy(idxtype);
	ir_texpr_destroy(elemte);
	return rc;
}

/** Generate code for addition.
 *
 * @param cgproc Code generator for procedure
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_add(cgen_proc_t *cgproc, comp_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	bool l_int;
	bool r_int;
	bool l_ptr;
	bool r_ptr;

	l_int = cgen_type_is_integer(cgproc->cgen, lres->cgtype);
	r_int = cgen_type_is_integer(cgproc->cgen, rres->cgtype);

	/* Integer + integer */
	if (l_int && r_int)
		return cgen_add_int(cgproc, lres, rres, lblock, eres);

	l_ptr = lres->cgtype->ntype == cgn_pointer;
	r_ptr = rres->cgtype->ntype == cgn_pointer;

	/* Pointer + pointer */
	if (l_ptr && r_ptr) {
		lexer_dprint_tok(&optok->tok, stderr);
		fprintf(stderr, ": Cannot add ");
		(void) cgtype_print(lres->cgtype, stderr);
		fprintf(stderr, " and ");
		(void) cgtype_print(rres->cgtype, stderr);
		fprintf(stderr, ".\n");

		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	/* Pointer + integer */
	if (l_ptr && r_int)
		return cgen_add_ptr_int(cgproc, optok, lres, rres, lblock, eres);

	/* Integer + pointer */
	if (l_int && r_ptr) {
		/* Produce a style warning and switch the operands */
		lexer_dprint_tok(&optok->tok, stderr);
		fprintf(stderr, ": Warning: Pointer should be the left "
		    "operand while indexing.\n");
		++cgproc->cgen->warnings;
		return cgen_add_ptr_int(cgproc, optok, rres, lres, lblock, eres);
		return EINVAL;
	}

	fprintf(stderr, "Unimplemented addition of ");
	(void) cgtype_print(lres->cgtype, stderr);
	fprintf(stderr, " and ");
	(void) cgtype_print(rres->cgtype, stderr);
	fprintf(stderr, ".\n");
	cgproc->cgen->error = true; // TODO
	return EINVAL;
}

/** Generate code for subtraction of integers.
 *
 * @param cgproc Code generator for procedure
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of subtraction
 * @return EOK on success or an error code
 */
static int cgen_sub_int(cgen_proc_t *cgproc, cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgtype_t *cgtype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	/* Perform usual arithmetic conversion */
	rc = cgen_uac(cgproc, lres, rres, lblock, &res1, &res2, &flags);
	if (rc != EOK)
		goto error;

	/* Unsigned subtraction of mixed-signed numbers is OK */
	(void)flags;

	/* Check the type */
	if (res1.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)res1.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(res1.cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(res1.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(res2.varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_sub;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);

	return EOK;
error:
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for subtraction of pointer and integer.
 *
 * @param cgproc Code generator for procedure
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_sub_ptr_int(cgen_proc_t *cgproc, comp_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *tmp = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	char *tmpname;
	cgen_eres_t cres;
	cgtype_t *idxtype = NULL;
	cgtype_t *cgtype = NULL;
	ir_texpr_t *elemte = NULL;
	cgtype_pointer_t *ptrt;
	int rc;

	cgen_eres_init(&cres);

	rc = cgtype_int_construct(false, cgir_int, &idxtype);
	if (rc != EOK)
		goto error;

	/* Convert index to be same size as pointer */
	rc = cgen_type_convert(cgproc, optok, rres, idxtype,
	    cgen_implicit, lblock, &cres);
	if (rc != EOK)
		goto error;

	cgtype_destroy(idxtype);
	idxtype = NULL;

	/* Check the type */
	assert(lres->cgtype->ntype == cgn_pointer);
	ptrt = (cgtype_pointer_t *)lres->cgtype->ext;

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	/* Generate IR type expression for the element type */
	rc = cgen_cgtype(cgproc->cgen, ptrt->tgtype, &elemte);
	if (rc != EOK)
		goto error;

	/* neg %<tmp>, %<cres> */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &tmp);
	if (rc != EOK)
		goto error;

	tmpname = tmp->varname;

	rc = ir_oper_var_create(cres.varname, &carg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_neg;
	instr->width = cgen_pointer_bits;
	instr->dest = &tmp->oper;
	instr->op1 = &carg->oper;
	instr->op2 = NULL;

	carg = NULL;
	tmp = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* ptridx %<dest>, %<tmp> */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(tmpname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_ptridx;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;
	instr->opt = elemte;

	elemte = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	cgen_eres_fini(&cres);

	return EOK;
error:
	cgen_eres_fini(&cres);
	ir_instr_destroy(instr);
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (tmp != NULL)
		ir_oper_destroy(&tmp->oper);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	cgtype_destroy(idxtype);
	ir_texpr_destroy(elemte);
	return rc;
}

/** Generate code for subtraction.
 *
 * @param cgproc Code generator for procedure
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of subtraction
 * @return EOK on success or an error code
 */
static int cgen_sub(cgen_proc_t *cgproc, comp_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	bool l_int;
	bool r_int;
	bool l_ptr;
	bool r_ptr;

	l_int = cgen_type_is_integer(cgproc->cgen, lres->cgtype);
	r_int = cgen_type_is_integer(cgproc->cgen, rres->cgtype);

	/* Integer - integer */
	if (l_int && r_int)
		return cgen_sub_int(cgproc, lres, rres, lblock, eres);

	l_ptr = lres->cgtype->ntype == cgn_pointer;
	r_ptr = rres->cgtype->ntype == cgn_pointer;

	/* Pointer - pointer */
	if (l_ptr && r_ptr) {
		lexer_dprint_tok(&optok->tok, stderr);
		fprintf(stderr, ": Unimplemented pointer subtraction.\n");

		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	/* Pointer - integer */
	if (l_ptr && r_int)
		return cgen_sub_ptr_int(cgproc, optok, lres, rres, lblock, eres);

	/* Integer - pointer */
	if (l_int && r_ptr) {
		lexer_dprint_tok(&optok->tok, stderr);
		fprintf(stderr, ": Invalid subtraction of ");
		(void) cgtype_print(lres->cgtype, stderr);
		fprintf(stderr, " and ");
		(void) cgtype_print(rres->cgtype, stderr);
		fprintf(stderr, ".\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	fprintf(stderr, "Unimplemented subtraction of ");
	(void) cgtype_print(lres->cgtype, stderr);
	fprintf(stderr, " and ");
	(void) cgtype_print(rres->cgtype, stderr);
	fprintf(stderr, ".\n");
	cgproc->cgen->error = true; // TODO
	return EINVAL;
}

/** Generate code for multiplication.
 *
 * @param cgproc Code generator for procedure
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of multiplication
 * @return EOK on success or an error code
 */
static int cgen_mul(cgen_proc_t *cgproc, cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_mul;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for shift left.
 *
 * @param cgproc Code generator for procedure
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_shl(cgen_proc_t *cgproc, cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_shl;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for shift right.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop Binary operator expression (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_shr(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	unsigned bits;
	bool is_signed;
	int rc;

	/* Check the type */
	if (!cgen_type_is_integer(cgproc->cgen, lres->cgtype)) {
		atok = ast_tree_first_tok(ebinop->larg);
		ctok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Left argument of '>>' operator should be "
		    "of integer type. Found ");
		(void) cgtype_print(lres->cgtype, stderr);
		fprintf(stderr, ".\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	assert(lres->cgtype->ntype == cgn_basic);

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	assert(bits != 0);

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = is_signed ? iri_shra : iri_shrl;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for bitwise AND.
 *
 * @param cgproc Code generator for procedure
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_band(cgen_proc_t *cgproc, cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_and;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for bitwise XOR.
 *
 * @param cgproc Code generator for procedure
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_bxor(cgen_proc_t *cgproc, cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_xor;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for bitwise OR.
 *
 * @param cgproc Code generator for procedure
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_bor(cgen_proc_t *cgproc, cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_or;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(cgtype);
	return rc;
}

/** Generate code for binary '+' operator.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_plus(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	comp_tok_t *ctok;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) ebinop->top.data;

	/* Add the two operands */
	rc = cgen_add(cgproc, ctok, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for binary '-' operator.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_minus(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	comp_tok_t *ctok;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) ebinop->top.data;

	/* Subtract the two operands */
	rc = cgen_sub(cgproc, ctok, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for binary '*' operator.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (multiplication)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_times(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Unsigned multiplication of mixed-sign numbers is OK. */
	(void)flags;

	/* Multiply the two operands */
	rc = cgen_mul(cgproc, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for shift left binary operator.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (shift left)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_shl(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Promoted value of left operand */
	rc = cgen_expr_promoted_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Shift left */
	rc = cgen_shl(cgproc, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for shift right operator.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (shift right)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_shr(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Promoted value of left operand */
	rc = cgen_expr_promoted_rvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Shift right */
	rc = cgen_shr(cgproc, ebinop, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
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
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	// XXX Only If both operands have arithmetic type
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (lres.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Unsigned comparison of mixed-sign "
		    "integers.\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
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

	instr->itype = is_signed ? iri_lt : iri_ltu;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
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
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	// XXX Only If both operands have arithmetic type
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (lres.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Unsigned comparison of mixed-sign "
		    "integers.\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
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

	instr->itype = is_signed ? iri_lteq : iri_lteu;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
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
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	// XXX Only If both operands have arithmetic type
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (!cgen_type_is_integer(cgproc->cgen, lres.cgtype) ||
	    !cgen_type_is_integer(cgproc->cgen, rres.cgtype)) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Unsigned comparison of mixed-sign "
		    "integers.\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
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

	instr->itype = is_signed ? iri_gt : iri_gtu;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
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
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	// XXX Only If both operands have arithmetic type
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (lres.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Unsigned comparison of mixed-sign "
		    "integers.\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
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

	instr->itype = is_signed ? iri_gteq : iri_gteu;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
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
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	unsigned bits;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	// XXX Only If both operands have arithmetic type
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (lres.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if ((flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Unsigned comparison of mixed-sign "
		    "integers.\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
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
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
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
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	unsigned bits;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	// XXX Only If both operands have arithmetic type
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (lres.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if ((flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Unsigned comparison of mixed-sign "
		    "integers.\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
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
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for bitwise AND expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (bitwise AND)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_band(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	/* If any of the operands was signed */
	if (is_signed || (flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Bitwise operation on signed "
		    "integer(s).\n");
		++cgproc->cgen->warnings;
	}

	/* Bitwise AND */
	rc = cgen_band(cgproc, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for bitwise XOR expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (bitwise XOR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_bxor(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	/* If any of the operands was signed */
	if (is_signed || (flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Bitwise operation on signed "
		    "integer(s).\n");
		++cgproc->cgen->warnings;
	}

	/* Bitwise XOR */
	rc = cgen_bxor(cgproc, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for bitwise OR operator.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (bitwise OR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_bor(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	/* If any of the operands was signed */
	if (is_signed || (flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Bitwise operation on signed "
		    "integer(s).\n");
		++cgproc->cgen->warnings;
	}

	/* Bitwise OR */
	rc = cgen_bor(cgproc, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
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
	ir_oper_var_t *larg = NULL;
	ir_oper_imm_t *imm = NULL;
	unsigned lblno;
	char *flabel = NULL;
	char *elabel = NULL;
	const char *dvarname;
	cgtype_basic_t *btype = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "false_and", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_and", lblno, &elabel);
	if (rc != EOK)
		goto error;

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	/* Jump to %false_and if left argument is zero */

	rc = cgen_truth_cjmp(cgproc, ebinop->larg, false, flabel, lblock);
	if (rc != EOK)
		goto error;

	/* Jump to %false_and if right argument is zero */

	rc = cgen_truth_cjmp(cgproc, ebinop->rarg, false, flabel, lblock);
	if (rc != EOK)
		goto error;

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
	eres->cgtype = &btype->cgtype;

	dvarname = dest->varname;
	dest = NULL;
	imm = NULL;

	/* jmp %end_and */

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
	eres->cgtype = &btype->cgtype;

	dest2 = NULL;
	imm = NULL;

	/* %end_and label */
	ir_lblock_append(lblock, elabel, NULL);

	free(flabel);
	free(elabel);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
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
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
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
	ir_oper_var_t *larg = NULL;
	ir_oper_imm_t *imm = NULL;
	unsigned lblno;
	char *tlabel = NULL;
	char *elabel = NULL;
	const char *dvarname;
	cgtype_basic_t *btype = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "true_or", lblno, &tlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_or", lblno, &elabel);
	if (rc != EOK)
		goto error;

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	/* Jump to %true_or if left argument is not zero */

	rc = cgen_truth_cjmp(cgproc, ebinop->larg, true, tlabel, lblock);
	if (rc != EOK)
		goto error;

	/* Jump to %true_or if right argument is not zero */

	rc = cgen_truth_cjmp(cgproc, ebinop->rarg, true, tlabel, lblock);
	if (rc != EOK)
		goto error;

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

	/* jmp %end_or */

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
	eres->cgtype = &btype->cgtype;

	dest2 = NULL;
	imm = NULL;

	/* %end_and label */
	ir_lblock_append(lblock, elabel, NULL);

	free(tlabel);
	free(elabel);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
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
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for storing a value (in an assignment expression).
 *
 * @param cgproc Code generator for procedure
 * @param ares Address expression result
 * @param vres Value expression result
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_store(cgen_proc_t *cgproc, cgen_eres_t *ares,
    cgen_eres_t *vres, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (vres->cgtype->ntype == cgn_basic) {
		bits = cgen_basic_type_bits(cgproc->cgen,
		    (cgtype_basic_t *)vres->cgtype->ext);
		if (bits == 0) {
			fprintf(stderr, "Unimplemented variable type.\n");
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}
	} else if (vres->cgtype->ntype == cgn_pointer) {
		bits = cgen_pointer_bits;
	} else {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(vres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_write;
	instr->width = bits;
	instr->dest = NULL;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	return EOK;
error:
	ir_instr_destroy(instr);
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
	comp_tok_t *ctok;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_eres_t cres;
	cgtype_t *cgtype;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);
	cgen_eres_init(&cres);

	/* Address of left hand expresson */
	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Value of right hand expresson */
	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) ebinop->top.data;

	/* Convert expression result to the destination type */
	rc = cgen_type_convert(cgproc, ctok, &rres, lres.cgtype,
	    cgen_implicit, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* Store the converted value */
	rc = cgen_store(cgproc, &lres, &cres, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from lres */
	cgtype = lres.cgtype;
	lres.cgtype = NULL;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&cres);

	eres->varname = rres.varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&cres);
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
	comp_tok_t *ctok;
	cgen_eres_t laddr;
	cgen_eres_t lval;
	cgen_eres_t rres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&laddr);
	cgen_eres_init(&lval);
	cgen_eres_init(&rres);
	cgen_eres_init(&ores);

	/* Address of left hand expresson */
	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &laddr);
	if (rc != EOK)
		goto error;

	/* Value of left hand expresson */
	rc = cgen_eres_rvalue(cgproc, &laddr, lblock, &lval);
	if (rc != EOK)
		goto error;

	/* Value of right hand expresson */
	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) ebinop->top.data;

	/* Add the two operands */
	rc = cgen_add(cgproc, ctok, &lval, &rres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &laddr, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&laddr);
	cgen_eres_fini(&lval);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&laddr);
	cgen_eres_fini(&lval);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&ores);
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
	comp_tok_t *ctok;
	cgen_eres_t laddr;
	cgen_eres_t lval;
	cgen_eres_t rres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&laddr);
	cgen_eres_init(&lval);
	cgen_eres_init(&rres);
	cgen_eres_init(&ores);

	/* Address of left hand expresson */
	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &laddr);
	if (rc != EOK)
		goto error;

	/* Value of left hand expresson */
	rc = cgen_eres_rvalue(cgproc, &laddr, lblock, &lval);
	if (rc != EOK)
		goto error;

	/* Value of right hand expresson */
	rc = cgen_expr_rvalue(cgproc, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) ebinop->top.data;

	/* Subtract the two operands */
	rc = cgen_sub(cgproc, ctok, &lval, &rres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &laddr, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&laddr);
	cgen_eres_fini(&lval);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&laddr);
	cgen_eres_fini(&lval);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&ores);
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
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	cgen_uac_flags_t flags;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2lr_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	/* Unsigned multiplication of mixed-sign integers is OK */
	(void)flags;

	/* Multiply the two operands */
	rc = cgen_mul(cgproc, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
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
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	/* Address of left hand expresson */
	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Promoted value of left operand */
	rc = cgen_eres_promoted_rvalue(cgproc, &lres, lblock, &ares);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Shift left */
	rc = cgen_shl(cgproc, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
	return rc;
}

/** Generate code for shift right assign expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST binary operator expression (shift right assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression resucess or an error code
 */
static int cgen_shr_assign(cgen_proc_t *cgproc, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	/* Address of left hand expresson */
	rc = cgen_expr_lvalue(cgproc, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Promoted value of left operand */
	rc = cgen_eres_promoted_rvalue(cgproc, &lres, lblock, &ares);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgproc, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Shift right */
	rc = cgen_shr(cgproc, ebinop, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
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
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	bool is_signed;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2lr_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	/* If any of the operands was signed */
	if (is_signed || (flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Bitwise operation on signed "
		    "integer(s).\n");
		++cgproc->cgen->warnings;
	}

	/* Bitwise AND the two operands */
	rc = cgen_band(cgproc, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
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
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	bool is_signed;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2lr_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	/* If any of the operands was signed */
	if (is_signed || (flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Bitwise operation on signed "
		    "integer(s).\n");
		++cgproc->cgen->warnings;
	}

	/* Bitwise XOR the two operands */
	rc = cgen_bxor(cgproc, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
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
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgtype_t *cgtype;
	bool is_signed;
	cgen_uac_flags_t flags;
	comp_tok_t *ctok;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2lr_uac(cgproc, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	is_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	/* If any of the operands was signed */
	if (is_signed || (flags & cguac_mix2u) != 0) {
		ctok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Bitwise operation on signed "
		    "integer(s).\n");
		++cgproc->cgen->warnings;
	}

	/* Bitwise OR the two operands */
	rc = cgen_bor(cgproc, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
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
		return cgen_bo_plus(cgproc, ebinop, lblock, eres);
	case abo_minus:
		return cgen_bo_minus(cgproc, ebinop, lblock, eres);
	case abo_times:
		return cgen_bo_times(cgproc, ebinop, lblock, eres);
	case abo_divide:
	case abo_modulo:
		tok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unimplemented binary operator.\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	case abo_shl:
		return cgen_bo_shl(cgproc, ebinop, lblock, eres);
	case abo_shr:
		return cgen_bo_shr(cgproc, ebinop, lblock, eres);
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
		return cgen_bo_band(cgproc, ebinop, lblock, eres);
	case abo_bxor:
		return cgen_bo_bxor(cgproc, ebinop, lblock, eres);
	case abo_bor:
		return cgen_bo_bor(cgproc, ebinop, lblock, eres);
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

/** Generate code for comma expression.
 *
 * @param cgproc Code generator for procedure
 * @param ecomma AST comma expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ecomma(cgen_proc_t *cgproc, ast_ecomma_t *ecomma,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	int rc;

	/* Evaluate and ignore left argument */
	rc = cgen_expr(cgproc, ecomma->larg, lblock, &lres);
	if (rc != EOK)
		return rc;

	cgen_expr_check_unused(cgproc, ecomma->larg, &lres);

	cgen_eres_fini(&lres);

	/* Evaluate and return right argument */
	return cgen_expr(cgproc, ecomma->rarg, lblock, eres);
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
	cgen_eres_t cres;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *fun = NULL;
	ir_oper_list_t *args = NULL;
	ir_oper_var_t *arg = NULL;
	cgtype_t *rtype = NULL;
	cgtype_func_t *ftype;
	cgtype_func_arg_t *farg;
	int rc;

	cgen_eres_init(&ares);
	cgen_eres_init(&cres);

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
		rc = EINVAL;
		goto error;
	}

	if (member->cgtype->ntype != cgn_func) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Called object '%s' is not a function.\n",
		    ident->tok.text);
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	ftype = (cgtype_func_t *)member->cgtype->ext;

	/* Mark identifier as used */

	member->used = true;

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = cgtype_clone(ftype->rtype, &rtype);
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
	farg = cgtype_func_first(ftype);
	while (earg != NULL) {
		/*
		 * We have an argument, but function does not have
		 * another parameter.
		 */
		if (farg == NULL) {
			atok = ast_tree_first_tok(earg->arg);
			tok = (comp_tok_t *) atok->data;

			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Too many arguments to function '%s'.\n",
			    ident->tok.text);
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = cgen_expr_rvalue(cgproc, earg->arg, lblock, &ares);
		if (rc != EOK)
			goto error;

		atok = ast_tree_first_tok(earg->arg);
		tok = (comp_tok_t *)atok->data;

		/*
		 * If the function has a prototype and the argument is not
		 * variadic, convert it to its declared type.
		 * XXX Otherwise it should be simply promoted.
		 */
		rc = cgen_type_convert(cgproc, tok, &ares, farg->atype,
		    cgen_implicit, lblock, &cres);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(cres.varname, &arg);
		if (rc != EOK)
			goto error;

		ir_oper_list_append(args, &arg->oper);

		/* Prepare result for reuse */
		cgen_eres_fini(&ares);
		cgen_eres_fini(&cres);

		cgen_eres_init(&ares);
		cgen_eres_init(&cres);

		earg = ast_ecall_next(earg);
		farg = cgtype_func_next(farg);
	}

	/*
	 * Check if we provided all the declared parameters.
	 */
	if (farg != NULL) {
		/* Still some left */
		tok = (comp_tok_t *) ecall->trparen.data;

		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Too few arguments to function '%s'.\n",
		    ident->tok.text);
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if (!cgtype_is_void(ftype->rtype)) {
		rc = cgen_create_new_lvar_oper(cgproc, &dest);
		if (rc != EOK)
			goto error;
	}

	free(pident);

	instr->itype = iri_call;
	instr->dest = dest != NULL ? &dest->oper : NULL;
	instr->op1 = &fun->oper;
	instr->op2 = &args->oper;

	ir_lblock_append(lblock, NULL, instr);

	cgen_eres_fini(&ares);
	cgen_eres_fini(&cres);

	eres->varname = dest ? dest->varname : NULL;
	eres->valtype = cgen_rvalue;
	eres->cgtype = rtype;
	eres->valused = cgtype_is_void(ftype->rtype);
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
	cgen_eres_fini(&ares);
	cgen_eres_fini(&cres);
	cgtype_destroy(rtype);
	return rc;
}

/** Generate code for index expression.
 *
 * @param cgproc Code generator for procedure
 * @param eindex AST index expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eindex(cgen_proc_t *cgproc, ast_eindex_t *eindex,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	cgen_eres_t ires;
	cgen_eres_t sres;
	comp_tok_t *ctok;
	cgtype_pointer_t *ptrtype;
	cgtype_t *cgtype;
	bool b_ptr;
	bool i_ptr;
	bool b_int;
	bool i_int;
	int rc;

	cgen_eres_init(&bres);
	cgen_eres_init(&ires);
	cgen_eres_init(&sres);

	/* Evaluate base operand */
	rc = cgen_expr_rvalue(cgproc, eindex->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Evaluate index operand */
	rc = cgen_expr_rvalue(cgproc, eindex->iexpr, lblock, &ires);
	if (rc != EOK)
		goto error;

	b_int = cgen_type_is_integer(cgproc->cgen, bres.cgtype);
	i_int = cgen_type_is_integer(cgproc->cgen, ires.cgtype);

	b_ptr = bres.cgtype->ntype == cgn_pointer;
	i_ptr = ires.cgtype->ntype == cgn_pointer;

	ctok = (comp_tok_t *) eindex->tlbracket.data;

	if (!b_ptr && !i_ptr) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Subscripted object is neither pointer nor array.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if ((b_ptr && !i_int) || (i_ptr && !b_int)) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Subscript index is not an integer.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Add the two operands */
	rc = cgen_add(cgproc, ctok, &bres, &ires, lblock, &sres);
	if (rc != EOK)
		goto error;

	assert(sres.cgtype->ntype == cgn_pointer);

	/* Resulting type is the pointer target type */
	ptrtype = (cgtype_pointer_t *)sres.cgtype->ext;
	rc = cgtype_clone(ptrtype->tgtype, &cgtype);
	if (rc != EOK)
		goto error;

	/* Return address as lvalue */
	eres->varname = sres.varname;
	eres->valtype = cgen_lvalue;
	eres->cgtype = cgtype;

	cgen_eres_fini(&bres);
	cgen_eres_fini(&ires);
	cgen_eres_fini(&sres);
	return EOK;
error:
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ires);
	cgen_eres_fini(&sres);
	return rc;
}

/** Generate code for dereference expression.
 *
 * @param cgproc Code generator for procedure
 * @param ederef AST dereference expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ederef(cgen_proc_t *cgproc, ast_ederef_t *ederef,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	cgtype_t *cgtype;
	comp_tok_t *tok;
	cgtype_pointer_t *ptrtype;
	int rc;

	cgen_eres_init(&bres);

	/* Evaluate expression as rvalue */
	rc = cgen_expr_rvalue(cgproc, ederef->bexpr, lblock, &bres);
	if (rc != EOK) {
		printf("error\n");
		goto error;
	}

	/* Check that we are dereferencing a pointer */
	if (bres.cgtype->ntype != cgn_pointer) {
		/* Still some left */
		tok = (comp_tok_t *) ederef->tasterisk.data;

		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Dereference operator needs a pointer, got '");
		(void) cgtype_print(bres.cgtype, stderr);
		fprintf(stderr, "'.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Resulting type is the pointer target type */
	ptrtype = (cgtype_pointer_t *)bres.cgtype->ext;
	rc = cgtype_clone(ptrtype->tgtype, &cgtype);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&bres);

	/* Return address as lvalue */
	eres->varname = bres.varname;
	eres->valtype = cgen_lvalue;
	eres->cgtype = cgtype;
	return EOK;
error:
	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for address expression.
 *
 * @param cgproc Code generator for procedure
 * @param eaddr AST address expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eaddr(cgen_proc_t *cgproc, ast_eaddr_t *eaddr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	cgtype_t *cgtype;
	cgtype_pointer_t *ptrtype;
	int rc;

	cgen_eres_init(&bres);

	/* Evaluate expression as lvalue */
	rc = cgen_expr_lvalue(cgproc, eaddr->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Construct the type */
	rc = cgtype_pointer_create(bres.cgtype, &ptrtype);
	if (rc != EOK)
		goto error;

	/* Ownership transferred to ptrtype */
	bres.cgtype = NULL;

	cgtype = &ptrtype->cgtype;
	cgen_eres_fini(&bres);

	/* Return address as rvalue */
	eres->varname = bres.varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	return EOK;
error:
	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for sizeof expression.
 *
 * @param cgproc Code generator for procedure
 * @param esizeof AST sizeof expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_esizeof(cgen_proc_t *cgproc, ast_esizeof_t *esizeof,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_tok_t *atok;
	comp_tok_t *tok;

	(void)lblock;
	(void)eres;

	(void)cgen_type_sizeof;

	atok = ast_tree_first_tok(&esizeof->node);
	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": This expression type is not implemented.\n");
	cgproc->cgen->error = true; // TODO
	return EINVAL;
}

/** Generate code for cast expression.
 *
 * @param cgproc Code generator for procedure
 * @param ecast AST cast expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ecast(cgen_proc_t *cgproc, ast_ecast_t *ecast,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	ast_sclass_type_t sctype;
	int rc;

	cgen_eres_init(&bres);

	/* Declaration specifiers */
	rc = cgen_dspecs(cgproc->cgen, cgproc->cur_scope, ecast->dspecs,
	    &sctype, &stype);
	if (rc != EOK)
		goto error;

	if (sctype != asc_none) {
		atok = ast_tree_first_tok(&ecast->dspecs->node);
		ctok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented storage class specifier.\n");
		cgproc->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	/* Declarator */
	rc = cgen_decl(cgproc->cgen, cgproc->cur_scope, stype, ecast->decl,
	    NULL, &dtype);
	if (rc != EOK)
		goto error;

	/* Evaluate expression */
	rc = cgen_expr(cgproc, ecast->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *)ecast->tlparen.data;

	rc = cgen_type_convert(cgproc, ctok, &bres, dtype,
	    cgen_explicit, lblock, eres);
	if (rc != EOK)
		goto error;

	cgtype_destroy(dtype);
	cgtype_destroy(stype);
	cgen_eres_fini(&bres);
	return EOK;
error:
	cgen_eres_fini(&bres);
	cgtype_destroy(dtype);
	cgtype_destroy(stype);
	return rc;
}

/** Generate code for unary sign expression.
 *
 * @param cgproc Code generator for procedure
 * @param eusign AST unary sign expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eusign(cgen_proc_t *cgproc, ast_eusign_t *eusign,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *barg = NULL;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	cgen_eres_t bres;
	unsigned bits;
	int rc;

	cgen_eres_init(&bres);

	/* Evaluate and promote base expression */
	rc = cgen_expr_promoted_rvalue(cgproc, eusign->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	atok = ast_tree_first_tok(&eusign->node);
	ctok = (comp_tok_t *) atok->data;

	if (bres.cgtype->ntype != cgn_basic) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)bres.cgtype->ext);
	if (bits == 0) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if (eusign->usign == aus_minus) {
		/* neg %<dest>, %<bres> */

		rc = ir_instr_create(&instr);
		if (rc != EOK)
			goto error;

		rc = cgen_create_new_lvar_oper(cgproc, &dest);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(bres.varname, &barg);
		if (rc != EOK)
			goto error;

		instr->itype = iri_neg;
		instr->width = bits;
		instr->dest = &dest->oper;
		instr->op1 = &barg->oper;
		instr->op2 = NULL;

		ir_lblock_append(lblock, NULL, instr);
		instr = NULL;

		eres->varname = dest->varname;
		eres->valtype = cgen_rvalue;
		/* Salvage the type from bres */
		eres->cgtype = bres.cgtype;
		bres.cgtype = NULL;
	} else {
		eres->varname = bres.varname;
		eres->valtype = cgen_rvalue;
		/* Salvage the type from bres */
		eres->cgtype = bres.cgtype;
		bres.cgtype = NULL;
	}

	cgen_eres_fini(&bres);

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (barg != NULL)
		ir_oper_destroy(&barg->oper);
	cgen_eres_fini(&bres);
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
	ir_oper_var_t *larg = NULL;
	ir_oper_imm_t *imm = NULL;
	unsigned lblno;
	char *flabel = NULL;
	char *elabel = NULL;
	const char *dvarname;
	cgtype_basic_t *btype = NULL;
	int rc;

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "false_lnot", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_lnot", lblno, &elabel);
	if (rc != EOK)
		goto error;

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	/* Jump to false_lnot if base expression is not zero */

	rc = cgen_truth_cjmp(cgproc, elnot->bexpr, true, flabel, lblock);
	if (rc != EOK)
		goto error;

	/* imm.16 %<dest>, 1 */

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
	instr = NULL;

	dvarname = dest->varname;
	dest = NULL;
	imm = NULL;

	/* jmp %end_lnot */

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

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	larg = NULL;

	/* %false_lnot: */
	ir_lblock_append(lblock, flabel, NULL);

	/* imm.16 %<dest>, 0 */

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
	rc = ir_oper_var_create(dvarname, &dest);
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
	instr = NULL;

	dest = NULL;
	imm = NULL;

	/* %end_lnot: */
	ir_lblock_append(lblock, elabel, NULL);

	eres->varname = dvarname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	free(flabel);
	free(elabel);
	return EOK;
error:
	if (flabel != NULL)
		free(flabel);
	if (elabel != NULL)
		free(elabel);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for bitwise NOT expression.
 *
 * @param cgproc Code generator for procedure
 * @param ebinop AST bitwise NOT expression
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
	unsigned bits;
	cgtype_t *cgtype;
	int rc;

	cgen_eres_init(&bres);

	rc = cgen_expr_promoted_rvalue(cgproc, ebnot->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (bres.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)bres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

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
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &barg->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	/* Salvage type from bres */
	cgtype = bres.cgtype;
	bres.cgtype = NULL;
	cgen_eres_fini(&bres);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (barg != NULL)
		ir_oper_destroy(&barg->oper);
	cgen_eres_fini(&bres);
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
	comp_tok_t *ctok;
	cgen_eres_t baddr;
	cgen_eres_t bval;
	cgen_eres_t adj;
	cgen_eres_t ares;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&baddr);
	cgen_eres_init(&bval);
	cgen_eres_init(&adj);
	cgen_eres_init(&ares);

	/* Evaluate base expression as lvalue */
	rc = cgen_expr_lvalue(cgproc, epreadj->bexpr, lblock, &baddr);
	if (rc != EOK)
		goto error;

	/* Get the value */
	rc = cgen_eres_rvalue(cgproc, &baddr, lblock, &bval);
	if (rc != EOK)
		goto error;

	/* Adjustment value */
	rc = cgen_const_int(cgproc, cgelm_int, 1, lblock, &adj);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) epreadj->tadj.data;

	if (epreadj->adj == aat_inc) {
		/* Add the two operands */
		rc = cgen_add(cgproc, ctok, &bval, &adj, lblock, &ares);
		if (rc != EOK)
			goto error;
	} else {
		/* Subtract the two operands */
		rc = cgen_sub(cgproc, ctok, &bval, &adj, lblock, &ares);
		if (rc != EOK)
			goto error;
	}

	/* Store the updated value */
	rc = cgen_store(cgproc, &baddr, &ares, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ares */
	cgtype = ares.cgtype;
	ares.cgtype = NULL;

	resvn = ares.varname;
	ares.varname = NULL;

	cgen_eres_fini(&baddr);
	cgen_eres_fini(&bval);
	cgen_eres_fini(&adj);
	cgen_eres_fini(&ares);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&baddr);
	cgen_eres_fini(&bval);
	cgen_eres_fini(&adj);
	cgen_eres_fini(&ares);
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
	comp_tok_t *ctok;
	cgen_eres_t baddr;
	cgen_eres_t bval;
	cgen_eres_t adj;
	cgen_eres_t ares;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&baddr);
	cgen_eres_init(&bval);
	cgen_eres_init(&adj);
	cgen_eres_init(&ares);

	/* Evaluate base expression as lvalue */
	rc = cgen_expr_lvalue(cgproc, epostadj->bexpr, lblock, &baddr);
	if (rc != EOK)
		goto error;

	/* Get the value */
	rc = cgen_eres_rvalue(cgproc, &baddr, lblock, &bval);
	if (rc != EOK)
		goto error;

	/* Adjustment value */
	rc = cgen_const_int(cgproc, cgelm_int, 1, lblock, &adj);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) epostadj->tadj.data;

	if (epostadj->adj == aat_inc) {
		/* Add the two operands */
		rc = cgen_add(cgproc, ctok, &bval, &adj, lblock, &ares);
		if (rc != EOK)
			goto error;
	} else {
		/* Subtract the two operands */
		rc = cgen_sub(cgproc, ctok, &bval, &adj, lblock, &ares);
		if (rc != EOK)
			goto error;
	}

	/* Store the updated value */
	rc = cgen_store(cgproc, &baddr, &ares, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from bval */
	cgtype = bval.cgtype;
	bval.cgtype = NULL;

	resvn = bval.varname;
	bval.varname = NULL;

	cgen_eres_fini(&baddr);
	cgen_eres_fini(&bval);
	cgen_eres_fini(&adj);
	cgen_eres_fini(&ares);

	eres->varname = resvn;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = true;
	return EOK;
error:
	cgen_eres_fini(&baddr);
	cgen_eres_fini(&bval);
	cgen_eres_fini(&adj);
	cgen_eres_fini(&ares);
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
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_ecomma:
		rc = cgen_ecomma(cgproc, (ast_ecomma_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ecall:
		rc = cgen_ecall(cgproc, (ast_ecall_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eindex:
		rc = cgen_eindex(cgproc, (ast_eindex_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ederef:
		rc = cgen_ederef(cgproc, (ast_ederef_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eaddr:
		rc = cgen_eaddr(cgproc, (ast_eaddr_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_esizeof:
		rc = cgen_esizeof(cgproc, (ast_esizeof_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ecast:
		rc = cgen_ecast(cgproc, (ast_ecast_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ecliteral:
	case ant_emember:
	case ant_eindmember:
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_eusign:
		rc = cgen_eusign(cgproc, (ast_eusign_t *) expr->ext, lblock,
		    eres);
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
	int rc;

	cgen_eres_init(&res);

	rc = cgen_expr(cgproc, expr, lblock, &res);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvalue(cgproc, &res, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&res);
	return EOK;
error:
	cgen_eres_fini(&res);
	return rc;
}

/** Generate code for converting expression result to rvalue.
 *
 * If the result is an lvalue, read it to produce an rvalue.
 *
 * @param cgproc Code generator for procedure
 * @param res Original expression result
 * @param lblock IR labeled block to which the code should be appended
 * @param dres Place to store rvalue expression result
 * @return EOK on success or an error code
 */
static int cgen_eres_rvalue(cgen_proc_t *cgproc, cgen_eres_t *res,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	cgtype_t *cgtype;
	unsigned bits;
	int rc;

	/* Check if we already have an rvalue */
	if (res->valtype == cgen_rvalue) {
		rc = cgtype_clone(res->cgtype, &cgtype);
		if (rc != EOK)
			goto error;

		eres->varname = res->varname;
		eres->valtype = res->valtype;
		eres->cgtype = cgtype;
		eres->valused = res->valused;
		return EOK;
	}

	/* Check the type */
	if (res->cgtype->ntype == cgn_basic) {
		bits = cgen_basic_type_bits(cgproc->cgen,
		    (cgtype_basic_t *)res->cgtype->ext);
		if (bits == 0) {
			fprintf(stderr, "Unimplemented variable type.\n");
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}
	} else if (res->cgtype->ntype == cgn_pointer) {
		bits = cgen_pointer_bits;
	} else {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Need to read the value in */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(res->varname, &var);
	if (rc != EOK)
		goto error;

	instr->itype = iri_read;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &var->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	rc = cgtype_clone(res->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->valused = res->valused;

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (var != NULL)
		ir_oper_destroy(&var->oper);
	return rc;
}

/** Read and promote value.
 *
 * If @a bres is an lvalue, read it to produce an rvalue.
 * If it is smaller than int (float), promote it.
 *
 * @param cgproc Code generator for procedure
 * @param bres Base expression result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eres_promoted_rvalue(cgen_proc_t *cgproc, cgen_eres_t *bres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	// TODO
	return cgen_eres_rvalue(cgproc, bres, lblock, eres);
}

/** Generate code for expression, producing a promoted rvalue.
 *
 * If the result of expression is an lvalue, read it to produce an rvalue.
 * If it is smaller than int (float), promote it.
 *
 * @param cgproc Code generator for procedure
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr_promoted_rvalue(cgen_proc_t *cgproc, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	int rc;

	cgen_eres_init(&bres);

	rc = cgen_expr_rvalue(cgproc, expr, lblock, &bres);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_promoted_rvalue(cgproc, &bres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&bres);
	return EOK;
error:
	cgen_eres_fini(&bres);
	return rc;
}

/** Perform usual arithmetic conversions on a pair of expression results.
 *
 * Usual arithmetic conversions are defined in 6.3.1.8 of the C99 standard.
 * The expressions are evaluated into rvalues, checked for being scalar type,
 * the 'larger' type is determined and the value of the smaller type
 * is converted, if needed. Values of type smaller than int (or float)
 * should be promoted.
 *
 * @param cgproc Code generator for procedure
 * @param res1 First expression result
 * @param res2 Second expression result
 * @param lblock Labeled block to which to append code
 * @param eres1 Place to store result of first converted result
 * @param eres2 Place to store result of second converted result
 * @param flags Place to store flags
 *
 * @return EOK on success or an error code
 */
static int cgen_uac(cgen_proc_t *cgproc, cgen_eres_t *res1,
    cgen_eres_t *res2, ir_lblock_t *lblock, cgen_eres_t *eres1,
    cgen_eres_t *eres2, cgen_uac_flags_t *flags)
{
	cgtype_t *rtype = NULL;
	cgen_eres_t pr1;
	cgen_eres_t pr2;
	cgtype_int_rank_t rank1;
	cgtype_int_rank_t rank2;
	cgtype_int_rank_t rrank;
	bool sign1;
	bool sign2;
	bool rsign;
	unsigned bits1;
	unsigned bits2;
	cgtype_basic_t *bt1;
	cgtype_basic_t *bt2;
	int rc;

	cgen_eres_init(&pr1);
	cgen_eres_init(&pr2);

	if (!cgen_type_is_integer(cgproc->cgen, res1->cgtype) ||
	    !cgen_type_is_integer(cgproc->cgen, res2->cgtype)) {
		fprintf(stderr, "Performing UAC on non-integral type(s) ");
		(void) cgtype_print(res1->cgtype, stderr);
		fprintf(stderr, ", ");
		(void) cgtype_print(res2->cgtype, stderr);
		fprintf(stderr, " (not implemented).\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bt1 = (cgtype_basic_t *)res1->cgtype->ext;
	bt2 = (cgtype_basic_t *)res2->cgtype->ext;

	/* Get rank, bits and signedness of both operands */

	rank1 = cgtype_int_rank(res1->cgtype);
	sign1 = cgen_type_is_signed(cgproc->cgen, res1->cgtype);
	bits1 = cgen_basic_type_bits(cgproc->cgen, bt1);

	rank2 = cgtype_int_rank(res2->cgtype);
	sign2 = cgen_type_is_signed(cgproc->cgen, res2->cgtype);
	bits2 = cgen_basic_type_bits(cgproc->cgen, bt2);

	/* Determine resulting rank */

	rrank = rank1 > rank2 ? rank1 : rank2;

	/* Determine resulting signedness */

	*flags = cguac_none;

	if (sign1 == sign2) {
		rsign = sign1;
	} else if ((sign1 && bits1 > bits2) || (sign2 && bits1 < bits2)) {
		/*
		 * The unsigned type can be wholly represented in
		 * the signed type.
		 */
		rsign = true;
	} else {
		/*
		 * Resulting type is unsigned.
		 */
		rsign = false;
		/* XXX Signed smaller than unsigned */
		*flags |= cguac_mix2u;
	}

	/* Promote both operands */

	rc = cgen_eres_promoted_rvalue(cgproc, res1, lblock, &pr1);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_promoted_rvalue(cgproc, res2, lblock, &pr2);
	if (rc != EOK)
		goto error;

	/* Construct result type */

	rc = cgtype_int_construct(rsign, rrank, &rtype);
	if (rc != EOK)
		goto error;

	/* Convert the promoted arguments to the result type */

	rc = cgen_type_convert(cgproc, NULL, &pr1, rtype, cgen_explicit, lblock,
	    eres1);
	if (rc != EOK)
		goto error;

	rc = cgen_type_convert(cgproc, NULL, &pr2, rtype, cgen_explicit, lblock,
	    eres2);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&pr1);
	cgen_eres_fini(&pr2);
	cgtype_destroy(rtype);
	return EOK;
error:
	cgen_eres_fini(&pr1);
	cgen_eres_fini(&pr2);
	cgtype_destroy(rtype);
	return rc;
}

/** Evaluate left and right operand experssions and perform usual arithmetic
 * conversions.
 *
 * The results can be used when generating code for a binary operator,
 * knowing that the two results already have the same type.
 *
 * @param cgproc Code generator for procedure
 * @param expr1 First expression
 * @param expr2 Second expression
 * @param lblock Labeled block to which to append code
 * @param eres1 Place to store result of first expression
 * @param eres2 Place to store result of second expression
 * @param flags Place to store flags
 *
 * @return EOK on success or an error code
 */
static int cgen_expr2_uac(cgen_proc_t *cgproc, ast_node_t *expr1,
    ast_node_t *expr2, ir_lblock_t *lblock, cgen_eres_t *eres1,
    cgen_eres_t *eres2, cgen_uac_flags_t *flags)
{
	cgen_eres_t res1;
	cgen_eres_t res2;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	rc = cgen_expr_rvalue(cgproc, expr1, lblock, &res1);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, expr2, lblock, &res2);
	if (rc != EOK)
		goto error;

	rc = cgen_uac(cgproc, &res1, &res2, lblock, eres1, eres2, flags);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	return EOK;
error:
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	return rc;
}

/** Evaluate left and right operand experssions and perform usual arithmetic
 * conversions, while returning lvalue of left operand.
 *
 * This is used in a compound assignment to get the assignment destination
 * as well as the value of both opeands.
 *
 * @param cgproc Code generator for procedure
 * @param expr1 First expression
 * @param expr2 Second expression
 * @param lblock Labeled block to which to append code
 * @param lres1 Place to store lvalue result of first expression
 * @param eres1 Place to store converted result of first expression
 * @param eres2 Place to store converted result of second expression
 * @param flags Place to store flags
 *
 * @return EOK on success or an error code
 */
static int cgen_expr2lr_uac(cgen_proc_t *cgproc, ast_node_t *expr1,
    ast_node_t *expr2, ir_lblock_t *lblock, cgen_eres_t *lres1,
    cgen_eres_t *eres1, cgen_eres_t *eres2, cgen_uac_flags_t *flags)
{
	cgen_eres_t res1;
	cgen_eres_t res2;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	rc = cgen_expr_lvalue(cgproc, expr1, lblock, lres1);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvalue(cgproc, lres1, lblock, &res1);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgproc, expr2, lblock, &res2);
	if (rc != EOK)
		goto error;

	rc = cgen_uac(cgproc, &res1, &res2, lblock, eres1, eres2, flags);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	return EOK;
error:
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	return rc;
}

/** Convert expression result to void.
 *
 * @param cgproc Code generator for procedure
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expresson result)
 * @param dtype Destination type
 * @param cres Place to store conversion result
 *
 * @return EOk or an error code
 */
static int cgen_type_convert_to_void(cgen_proc_t *cgproc, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_eres_t *cres)
{
	cgtype_t *cgtype;
	int rc;

	(void)cgproc;
	(void)ctok;
	(void)ares;

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		return rc;

	cres->varname = NULL;
	cres->valtype = cgen_rvalue;
	cres->cgtype = cgtype;
	cres->valused = true;

	return EOK;
}

/** Convert expression result between two integer types.
 *
 * @param cgproc Code generator for procedure
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expresson result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_integer(cgen_proc_t *cgproc, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	ir_instr_t *instr = NULL;
	ir_instr_type_t itype;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *sarg = NULL;
	ir_oper_imm_t *imm = NULL;
	cgen_eres_t rres;
	cgtype_t *cgtype;
	unsigned srcw, destw;
	bool src_signed;
	int rc;

	assert(ares->cgtype->ntype == cgn_basic);
	srcw = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)ares->cgtype->ext);
	src_signed = cgen_basic_type_signed(cgproc->cgen,
	    (cgtype_basic_t *)ares->cgtype->ext);

	assert(dtype->ntype == cgn_basic);
	destw = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)dtype->ext);

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		return rc;

	/* Source and destination are of the same size ? */
	if (destw == srcw) {
		/* No conversion needed */
		cres->varname = ares->varname;
		cres->valtype = ares->valtype;
		cres->cgtype = cgtype;
		cres->valused = true;

		return EOK;
	}

	cgen_eres_init(&rres);

	rc = cgen_eres_rvalue(cgproc, ares, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Generate trunc/sgnext/zrext instruction */

	if (destw < srcw) {
		/* Integer truncation */
		itype = iri_trunc;

		if (expl != cgen_explicit) {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Conversion may loose "
			    "significant digits.\n");
			++cgproc->cgen->warnings;
		}
	} else {
		/* Extension */
		assert(srcw < destw);

		if (src_signed)
			itype = iri_sgnext;
		else
			itype = iri_zrext;
	}

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres.varname, &sarg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(srcw, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = itype;
	instr->width = destw;
	instr->dest = &dest->oper;
	instr->op1 = &sarg->oper;
	instr->op2 = &imm->oper;

	ir_lblock_append(lblock, NULL, instr);

	cres->varname = dest->varname;
	cres->valtype = cgen_rvalue;
	cres->cgtype = cgtype;
	cres->valused = true;

	cgen_eres_fini(&rres);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (sarg != NULL)
		ir_oper_destroy(&sarg->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	cgtype_destroy(cgtype);

	cgen_eres_fini(&rres);
	return rc;
}

/** Convert expression result between two pointer types.
 *
 * @param cgproc Code generator for procedure
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expresson result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_pointer(cgen_proc_t *cgproc, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgtype_pointer_t *ptrtype1;
	cgtype_pointer_t *ptrtype2;
	cgtype_t *cgtype;
	int rc;

	(void)cgproc;
	(void)lblock;

	assert(ares->cgtype->ntype == cgn_pointer);
	assert(dtype->ntype == cgn_pointer);

	ptrtype1 = (cgtype_pointer_t *)ares->cgtype->ext;
	ptrtype2 = (cgtype_pointer_t *)dtype->ext;

	if (!cgtype_ptr_compatible(ptrtype1, ptrtype2) &&
	    expl != cgen_explicit) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Converting from ");
		(void) cgtype_print(ares->cgtype, stderr);
		fprintf(stderr, " to incompatible pointer type ");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, ".\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		goto error;

	cres->varname = ares->varname;
	cres->valtype = ares->valtype;
	cres->cgtype = cgtype;
	cres->valused = ares->valused;
error:
	return rc;
}

/** Convert expression result from integer to pointer.
 *
 * @param cgproc Code generator for procedure
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expresson result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_int_ptr(cgen_proc_t *cgproc, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	unsigned bits;
	cgtype_t *cgtype;
	int rc;

	(void)lblock;

	assert(ares->cgtype->ntype == cgn_basic);
	assert(dtype->ntype == cgn_pointer);

	bits = cgen_basic_type_bits(cgproc->cgen,
	    (cgtype_basic_t *)ares->cgtype->ext);

	if (expl != cgen_explicit) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Implicit conversion from integer "
		    "to pointer.\n");
		++cgproc->cgen->warnings;
	}

	if (bits != cgen_pointer_bits) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Converting to pointer from integer "
		    "of different size.\n");
		++cgproc->cgen->warnings;
	}

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		goto error;

	cres->varname = ares->varname;
	cres->valtype = ares->valtype;
	cres->cgtype = cgtype;
	cres->valused = ares->valused;
error:
	return rc;
}

/** Convert value expression result to the specified type.
 *
 * @param cgen Code generator for procedure
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expresson result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_rval(cgen_proc_t *cgproc, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	assert(ares->valtype == cgen_rvalue);
	assert(ares->cgtype != NULL);

	/* Source and destination types are the same basic type */
	if (ares->cgtype->ntype == cgn_basic &&
	    dtype->ntype == cgn_basic &&
	    ((cgtype_basic_t *)ares->cgtype->ext)->elmtype ==
	    ((cgtype_basic_t *)dtype->ext)->elmtype) {
		/* Return unchanged */
		return cgen_eres_clone(ares, cres);
	}

	/* Destination type is void */
	if (cgtype_is_void(dtype)) {
		return cgen_type_convert_to_void(cgproc, ctok, ares,
		    dtype, cres);
	}

	/* Source and destination types are pointers */
	if (ares->cgtype->ntype == cgn_pointer &&
	    dtype->ntype == cgn_pointer) {
		return cgen_type_convert_pointer(cgproc, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	/* Source and destination types are pointers */
	if (cgen_type_is_integer(cgproc->cgen, ares->cgtype) &&
	    dtype->ntype == cgn_pointer) {
		return cgen_type_convert_int_ptr(cgproc, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	if (ares->cgtype->ntype == cgn_basic &&
	    ((cgtype_basic_t *)(ares->cgtype->ext))->elmtype == cgelm_logic &&
	    expl != cgen_explicit) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Truth value used as an integer.\n");
		++cgproc->cgen->warnings;
	}

	/* Converting between two integer types */
	if (cgen_type_is_integer(cgproc->cgen, ares->cgtype) &&
	    cgen_type_is_integer(cgproc->cgen, dtype)) {
		return cgen_type_convert_integer(cgproc, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	if (dtype->ntype != cgn_basic ||
	    ((cgtype_basic_t *)(dtype->ext))->elmtype != cgelm_int) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Converting to type ");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, " which is different from int "
		    "(not implemented).\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	if (ares->cgtype->ntype != cgn_basic ||
	    (((cgtype_basic_t *)(ares->cgtype->ext))->elmtype != cgelm_int &&
	    ((cgtype_basic_t *)(ares->cgtype->ext))->elmtype != cgelm_logic)) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Converting from type ");
		(void) cgtype_print(ares->cgtype, stderr);
		fprintf(stderr, " which is different from int "
		    "(not implemented).\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	return cgen_eres_clone(ares, cres);
}

/** Convert expression result to the specified type.
 *
 * @param cgen Code generator for procedure
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expresson result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert(cgen_proc_t *cgproc, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&rres);

	rc = cgen_eres_rvalue(cgproc, ares, lblock, &rres);
	if (rc != EOK)
		goto error;

	rc = cgen_type_convert_rval(cgproc, ctok, &rres, dtype, expl, lblock,
	    cres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for a truth test / conditional jump.
 *
 * Evaluate truth expression, then jump if it is true/non-zero (@a cval == true),
 * or false/non-zero (@a cval == false), respectively.
 *
 * @param cgproc Code generator for procedure
 * @param aexpr Truth expression
 * @param cval Condition value when jump is taken
 * @param dlabel Jump destination label
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_truth_cjmp(cgen_proc_t *cgproc, ast_node_t *aexpr,
    bool cval, const char *dlabel, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	ast_tok_t *atok;
	comp_tok_t *tok;
	cgen_eres_t cres;
	int rc;

	cgen_eres_init(&cres);

	/* Condition */

	rc = cgen_expr_rvalue(cgproc, aexpr, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* Check the type */

	if (cres.cgtype->ntype != cgn_basic ||
	    ((cgtype_basic_t *)(cres.cgtype->ext))->elmtype != cgelm_logic) {
		atok = ast_tree_first_tok(aexpr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: '");
		cgtype_print(cres.cgtype, stderr);
		fprintf(stderr, "' used as a truth value.\n");
		++cgproc->cgen->warnings;
	}

	/* j[n]z %<cres>, %dlabel */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cres.varname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(dlabel, &larg);
	if (rc != EOK)
		goto error;

	instr->itype = cval ? iri_jnz : iri_jz;
	instr->width = 0;
	instr->dest = NULL;
	instr->op1 = &carg->oper;
	instr->op2 = &larg->oper;

	carg = NULL;
	larg = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	cgen_eres_fini(&cres);
	return EOK;
error:
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (instr != NULL)
		ir_instr_destroy(instr);
	cgen_eres_fini(&cres);
	return rc;
}

/** Generate code for break statement.
 *
 * @param cgproc Code generator for procedure
 * @param abreak AST break statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_break(cgen_proc_t *cgproc, ast_break_t *abreak,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *label = NULL;
	comp_tok_t *tok;
	int rc;

	if (cgproc->cur_loop_switch == NULL) {
		tok = (comp_tok_t *) abreak->tbreak.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Break without enclosing switch "
		    "or loop statement.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cgproc->cur_loop_switch->blabel, &label);
	if (rc != EOK)
		goto error;

	instr->itype = iri_jmp;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &label->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (label != NULL)
		ir_oper_destroy(&label->oper);
	return rc;
}

/** Generate code for continue statement.
 *
 * @param cgproc Code generator for procedure
 * @param acontinue AST continue statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_continue(cgen_proc_t *cgproc, ast_continue_t *acontinue,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *label = NULL;
	comp_tok_t *tok;
	int rc;

	if (cgproc->cur_loop == NULL) {
		tok = (comp_tok_t *) acontinue->tcontinue.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Continue without enclosing loop "
		    "statement.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cgproc->cur_loop->clabel, &label);
	if (rc != EOK)
		goto error;

	instr->itype = iri_jmp;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &label->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (label != NULL)
		ir_oper_destroy(&label->oper);
	return rc;
}

/** Generate code for goto statement.
 *
 * @param cgproc Code generator for procedure
 * @param agoto AST goto statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_goto(cgen_proc_t *cgproc, ast_goto_t *agoto,
    ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *label = NULL;
	comp_tok_t *tok;
	char *glabel = NULL;
	int rc;

	tok = (comp_tok_t *) agoto->ttarget.data;

	rc = cgen_create_goto_label(cgproc, tok->tok.text, &glabel);
	if (rc != EOK)
		goto error;

	rc = labels_use_label(cgproc->labels, &tok->tok);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(glabel, &label);
	if (rc != EOK)
		goto error;

	free(glabel);
	glabel = NULL;

	instr->itype = iri_jmp;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = NULL;
	instr->op1 = &label->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (label != NULL)
		ir_oper_destroy(&label->oper);
	if (glabel != NULL)
		free(glabel);
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
	cgen_eres_t cres;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	unsigned bits;
	int rc;

	cgen_eres_init(&ares);
	cgen_eres_init(&cres);

	/* Verify that function return type is not void if we have argument */
	if (areturn->arg != NULL && cgtype_is_void(cgproc->rtype)) {
		atok = ast_tree_first_tok(areturn->arg);
		ctok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Return with a value in "
		    "function returning void.\n");
		++cgproc->cgen->warnings;
	}

	/* Verify that function return type is void if do not have argument */
	if (areturn->arg == NULL && !cgtype_is_void(cgproc->rtype)) {
		ctok = (comp_tok_t *) areturn->treturn.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Return without a value in "
		    "function returning non-void.\n");
		++cgproc->cgen->warnings;
	}

	/* Only if we have an argument */
	if (areturn->arg != NULL) {
		/* Evaluate the return value */

		rc = cgen_expr_rvalue(cgproc, areturn->arg, lblock, &ares);
		if (rc != EOK)
			goto error;
	}

	/* Only if we have an argument and return type is not void */
	if (areturn->arg != NULL && !cgtype_is_void(cgproc->rtype)) {
		atok = ast_tree_first_tok(areturn->arg);
		ctok = (comp_tok_t *) atok->data;

		/* Convert to the return type */
		rc = cgen_type_convert(cgproc, ctok, &ares,
		    cgproc->rtype, cgen_implicit, lblock, &cres);
		if (rc != EOK)
			goto error;

		/* Check the type */
		if (cgproc->rtype->ntype == cgn_basic) {
			bits = cgen_basic_type_bits(cgproc->cgen,
			    (cgtype_basic_t *)cgproc->rtype->ext);
			if (bits == 0) {
				fprintf(stderr, "Unimplemented return type.\n");
				cgproc->cgen->error = true; // TODO
				rc = EINVAL;
				goto error;
			}
		} else if (cgproc->rtype->ntype == cgn_pointer) {
			bits = cgen_pointer_bits;
		} else {
			fprintf(stderr, "Unimplemented return type.\n");
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = ir_instr_create(&instr);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(cres.varname, &arg);
		if (rc != EOK)
			goto error;

		instr->itype = iri_retv;
		instr->width = bits;
		instr->dest = NULL;
		instr->op1 = &arg->oper;
		instr->op2 = NULL;

		ir_lblock_append(lblock, NULL, instr);

		cgen_eres_fini(&ares);
		cgen_eres_fini(&cres);
	} else {
		if (areturn->arg != NULL)
			cgen_eres_fini(&ares);

		/* Return without value */
		// XXX Verify that function is void
		rc = cgen_ret(cgproc, lblock);
		if (rc != EOK)
			goto error;
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (arg != NULL)
		ir_oper_destroy(&arg->oper);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&cres);
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
	ir_oper_var_t *larg = NULL;
	ast_elseif_t *elsif;
	cgen_eres_t cres;
	unsigned lblno;
	char *fiflabel = NULL;
	char *eiflabel = NULL;
	int rc;

	cgen_eres_init(&cres);

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "false_if", lblno, &fiflabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_if", lblno, &eiflabel);
	if (rc != EOK)
		goto error;

	/* Jump to false_if if condition is false */

	rc = cgen_truth_cjmp(cgproc, aif->cond, false, fiflabel, lblock);
	if (rc != EOK)
		goto error;

	/* True branch */

	rc = cgen_block(cgproc, aif->tbranch, lblock);
	if (rc != EOK)
		goto error;

	/* jmp %end_if */

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

	ir_lblock_append(lblock, fiflabel, NULL);
	free(fiflabel);
	fiflabel = NULL;

	/* Else-if branches */

	elsif = ast_if_first(aif);
	while (elsif != NULL) {
		/* Prepare cres for reuse */
		cgen_eres_fini(&cres);
		cgen_eres_init(&cres);
		/*
		 * Create false else-if label. Allocate a number every time
		 * since there might be multiple else-if branches.
		 */
		lblno = cgen_new_label_num(cgproc);

		rc = cgen_create_label(cgproc, "false_elseif", lblno, &fiflabel);
		if (rc != EOK)
			goto error;

		/* Jump to false_if if else-if condition is false */

		rc = cgen_truth_cjmp(cgproc, elsif->cond, false, fiflabel, lblock);
		if (rc != EOK)
			goto error;

		/* Else-if branch */
		rc = cgen_block(cgproc, elsif->ebranch, lblock);
		if (rc != EOK)
			goto error;

		/* jmp %end_if */

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
	cgen_eres_fini(&cres);
	return EOK;
error:
	if (instr != NULL)
		ir_instr_destroy(instr);
	if (fiflabel != NULL)
		free(fiflabel);
	if (eiflabel != NULL)
		free(eiflabel);
	cgen_eres_fini(&cres);
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
	cgen_loop_switch_t *lswitch = NULL;
	cgen_loop_t *loop = NULL;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	cgen_eres_t cres;
	unsigned lblno;
	char *wlabel = NULL;
	char *ewlabel = NULL;
	int rc;

	cgen_eres_init(&cres);

	lblno = cgen_new_label_num(cgproc);

	/* Create a new loop or switch tracking record */

	rc = cgen_loop_switch_create(cgproc->cur_loop_switch, &lswitch);
	if (rc != EOK)
		goto error;

	/* Create a new loop tracking record */

	rc = cgen_loop_create(cgproc->cur_loop, &loop);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "while", lblno, &wlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_while", lblno, &ewlabel);
	if (rc != EOK)
		goto error;

	lswitch->blabel = ewlabel;
	loop->clabel = wlabel;

	/* Set this as the innermost loop or switch */
	cgproc->cur_loop_switch = lswitch;

	/* Set this as the innermost loop */
	cgproc->cur_loop = loop;

	ir_lblock_append(lblock, wlabel, NULL);

	/* Jump to %end_while if condition is false */

	rc = cgen_truth_cjmp(cgproc, awhile->cond, false, ewlabel, lblock);
	if (rc != EOK)
		goto error;

	/* Body */

	rc = cgen_block(cgproc, awhile->body, lblock);
	if (rc != EOK)
		goto error;

	/* jmp %while */

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

	cgen_loop_switch_destroy(lswitch);
	cgen_loop_destroy(loop);

	cgen_eres_fini(&cres);
	free(wlabel);
	free(ewlabel);
	return EOK;
error:
	if (instr != NULL)
		ir_instr_destroy(instr);
	if (lswitch != NULL)
		cgen_loop_switch_destroy(lswitch);
	if (loop != NULL)
		cgen_loop_destroy(loop);
	if (wlabel != NULL)
		free(wlabel);
	if (ewlabel != NULL)
		free(ewlabel);
	cgen_eres_fini(&cres);
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
	cgen_loop_switch_t *lswitch = NULL;
	cgen_loop_t *loop = NULL;
	cgen_eres_t cres;
	unsigned lblno;
	char *dlabel = NULL;
	char *ndlabel = NULL;
	char *edlabel = NULL;
	int rc;

	cgen_eres_init(&cres);

	lblno = cgen_new_label_num(cgproc);

	/* Create a new loop or switch tracking record */

	rc = cgen_loop_switch_create(cgproc->cur_loop_switch, &lswitch);
	if (rc != EOK)
		goto error;

	/* Create a new loop tracking record */

	rc = cgen_loop_create(cgproc->cur_loop, &loop);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "do", lblno, &dlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "next_do", lblno, &ndlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_do", lblno, &edlabel);
	if (rc != EOK)
		goto error;

	lswitch->blabel = edlabel;
	loop->clabel = ndlabel;

	/* Set this as the innermost loop or switch */
	cgproc->cur_loop_switch = lswitch;

	/* Set this as the innermost loop */
	cgproc->cur_loop = loop;

	ir_lblock_append(lblock, dlabel, NULL);

	/* Body */

	rc = cgen_block(cgproc, ado->body, lblock);
	if (rc != EOK)
		goto error;

	/* label next_do */

	ir_lblock_append(lblock, ndlabel, NULL);

	/* Jump to %do if condition is true */

	rc = cgen_truth_cjmp(cgproc, ado->cond, true, dlabel, lblock);
	if (rc != EOK)
		goto error;

	ir_lblock_append(lblock, edlabel, NULL);

	cgen_loop_switch_destroy(lswitch);
	cgen_loop_destroy(loop);

	free(dlabel);
	free(ndlabel);
	free(edlabel);
	cgen_eres_fini(&cres);
	return EOK;
error:
	if (lswitch != NULL)
		cgen_loop_switch_destroy(lswitch);
	if (loop != NULL)
		cgen_loop_destroy(loop);
	if (dlabel != NULL)
		free(dlabel);
	if (ndlabel != NULL)
		free(ndlabel);
	if (edlabel != NULL)
		free(edlabel);
	cgen_eres_fini(&cres);
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
	cgen_loop_switch_t *lswitch = NULL;
	cgen_loop_t *loop = NULL;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	cgen_eres_t ires;
	cgen_eres_t cres;
	cgen_eres_t nres;
	unsigned lblno;
	char *flabel = NULL;
	char *eflabel = NULL;
	char *nflabel = NULL;
	int rc;

	cgen_eres_init(&ires);
	cgen_eres_init(&cres);
	cgen_eres_init(&nres);

	lblno = cgen_new_label_num(cgproc);

	/* Create a new loop or switch tracking record */

	rc = cgen_loop_switch_create(cgproc->cur_loop_switch, &lswitch);
	if (rc != EOK)
		goto error;

	/* Create a new loop tracking record */

	rc = cgen_loop_create(cgproc->cur_loop, &loop);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "for", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "next_for", lblno, &nflabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_for", lblno, &eflabel);
	if (rc != EOK)
		goto error;

	lswitch->blabel = eflabel;
	loop->clabel = nflabel;

	/* Set this as the innermost loop or switch */
	cgproc->cur_loop_switch = lswitch;

	/* Set this as the innermost loop */
	cgproc->cur_loop = loop;

	/* Loop initialization */

	if (afor->linit != NULL) {
		/* Evaluate and ignore initialization expression */
		rc = cgen_expr_rvalue(cgproc, afor->linit, lblock, &ires);
		if (rc != EOK)
			goto error;

		cgen_expr_check_unused(cgproc, afor->linit, &ires);
	}

	ir_lblock_append(lblock, flabel, NULL);

	/* Condition */

	if (afor->lcond != NULL) {
		/* Jump to %end_for if condition is false */

		rc = cgen_truth_cjmp(cgproc, afor->lcond, false, eflabel, lblock);
		if (rc != EOK)
			goto error;
	}

	/* Body */

	rc = cgen_block(cgproc, afor->body, lblock);
	if (rc != EOK)
		goto error;

	/* Loop iteration */

	ir_lblock_append(lblock, nflabel, NULL);

	if (afor->lnext != NULL) {
		/* Evaluate and ignore next iteration expression */
		rc = cgen_expr_rvalue(cgproc, afor->lnext, lblock, &nres);
		if (rc != EOK)
			goto error;

		cgen_expr_check_unused(cgproc, afor->lnext, &nres);
	}

	/* jmp %for */

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

	cgen_loop_switch_destroy(lswitch);
	cgen_loop_destroy(loop);
	free(flabel);
	free(nflabel);
	free(eflabel);
	cgen_eres_fini(&ires);
	cgen_eres_fini(&cres);
	cgen_eres_fini(&nres);
	return EOK;
error:
	if (instr != NULL)
		ir_instr_destroy(instr);
	if (lswitch != NULL)
		cgen_loop_switch_destroy(lswitch);
	if (loop != NULL)
		cgen_loop_destroy(loop);
	if (flabel != NULL)
		free(flabel);
	if (nflabel != NULL)
		free(nflabel);
	if (eflabel != NULL)
		free(eflabel);

	cgen_eres_fini(&ires);
	cgen_eres_fini(&cres);
	cgen_eres_fini(&nres);
	return rc;
}

/** Generate code for 'switch' statement.
 *
 * @param cgproc Code generator for procedure
 * @param aswitch AST switch statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_switch(cgen_proc_t *cgproc, ast_switch_t *aswitch,
    ir_lblock_t *lblock)
{
	cgen_eres_t eres;
	unsigned lblno;
	char *eslabel = NULL;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	cgen_switch_t *cgswitch = NULL;
	cgen_loop_switch_t *lswitch = NULL;
	int rc;

	cgen_eres_init(&eres);

	lblno = cgen_new_label_num(cgproc);

	/* Create a new switch tracking record */

	rc = cgen_switch_create(cgproc->cur_switch, &cgswitch);
	if (rc != EOK)
		goto error;

	/* Create a new loop or switch tracking record */

	rc = cgen_loop_switch_create(cgproc->cur_loop_switch, &lswitch);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "end_switch", lblno, &eslabel);
	if (rc != EOK)
		goto error;

	lswitch->blabel = eslabel;

	rc = cgen_create_label(cgproc, "case_cnd", lblno, &cgswitch->nclabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgproc, "case_body", lblno, &cgswitch->nblabel);
	if (rc != EOK)
		goto error;

	/* Switch expression */

	rc = cgen_expr_rvalue(cgproc, aswitch->sexpr, lblock, &eres);
	if (rc != EOK)
		goto error;

	/* Skip over any code before the first case label */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cgswitch->nclabel, &larg);
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

	/* Set this as the innermost switch */
	cgproc->cur_switch = cgswitch;

	/* Set this as the innermost loop/switch */
	cgproc->cur_loop_switch = lswitch;

	/* Switch expression result variable name */
	cgswitch->svarname = eres.varname;

	/* Body */

	rc = cgen_block(cgproc, aswitch->body, lblock);
	if (rc != EOK)
		goto error;

	/* jmp %end_switch */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(eslabel, &larg);
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

	/* Final case label */

	ir_lblock_append(lblock, cgswitch->nclabel, NULL);
	ir_lblock_append(lblock, cgswitch->nblabel, NULL);

	if (cgswitch->dlabel != NULL) {
		/* jmp %default */

		rc = ir_instr_create(&instr);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(cgswitch->dlabel, &larg);
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
	}

	/* label end_switch */

	ir_lblock_append(lblock, eslabel, NULL);

	cgproc->cur_switch = cgswitch->parent;
	cgen_switch_destroy(cgswitch);
	cgproc->cur_loop_switch = lswitch->parent;
	cgen_loop_switch_destroy(lswitch);
	free(eslabel);
	cgen_eres_fini(&eres);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (eslabel != NULL)
		free(eslabel);
	if (cgswitch != NULL) {
		cgproc->cur_switch = cgswitch->parent;
		cgen_switch_destroy(cgswitch);
	}
	if (lswitch != NULL) {
		cgproc->cur_loop_switch = lswitch->parent;
		cgen_loop_switch_destroy(lswitch);
	}
	cgen_eres_fini(&eres);
	return rc;
}

/** Generate code for 'case' label.
 *
 * @param cgproc Code generator for procedure
 * @param aclabel AST case label
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_clabel(cgen_proc_t *cgproc, ast_clabel_t *aclabel,
    ir_lblock_t *lblock)
{
	cgen_eres_t cres;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	ir_oper_var_t *carg = NULL;
	comp_tok_t *tok;
	unsigned lblno;
	char *dvarname;
	int rc;

	cgen_eres_init(&cres);

	/* If there is no enclosing switch statement */
	if (cgproc->cur_switch == NULL) {
		tok = (comp_tok_t *) aclabel->tcase.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Case label without enclosing switch "
		    "statement.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* jmp %case_bodyN */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cgproc->cur_switch->nblabel, &larg);
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

	/* Insert previously generated label for this case statement */

	ir_lblock_append(lblock, cgproc->cur_switch->nclabel, NULL);

	free(cgproc->cur_switch->nclabel);
	cgproc->cur_switch->nclabel = NULL;

	/* Create label for next case condition */

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "case_cnd", lblno,
	    &cgproc->cur_switch->nclabel);
	if (rc != EOK)
		goto error;

	/* Evaluate case expression */

	// TODO Verify that the expression is constant

	rc = cgen_expr_rvalue(cgproc, aclabel->cexpr, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* Compare values of switch and case */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cgproc->cur_switch->svarname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cres.varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_eq;
	instr->width = cgproc->cgen->arith_width;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	dvarname = dest->varname;
	dest = NULL;
	larg = NULL;
	rarg = NULL;

	ir_lblock_append(lblock, NULL, instr);
	instr = NULL;

	/* jz %<dest>, %caseN+1 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(dvarname, &carg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cgproc->cur_switch->nclabel, &larg);
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

	/* %case_bodyN */

	ir_lblock_append(lblock, cgproc->cur_switch->nblabel, NULL);

	free(cgproc->cur_switch->nblabel);
	cgproc->cur_switch->nblabel = NULL;

	/* Create label for next case body */

	rc = cgen_create_label(cgproc, "case_body", lblno,
	    &cgproc->cur_switch->nblabel);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&cres);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	cgen_eres_fini(&cres);
	return rc;
}

/** Generate code for 'default' label.
 *
 * @param cgproc Code generator for procedure
 * @param adlabel AST default label
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_dlabel(cgen_proc_t *cgproc, ast_dlabel_t *adlabel,
    ir_lblock_t *lblock)
{
	comp_tok_t *tok;
	unsigned lblno;
	int rc;

	/* If there is no enclosing switch statement */
	if (cgproc->cur_switch == NULL) {
		tok = (comp_tok_t *) adlabel->tdefault.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Default label without enclosing switch "
		    "statement.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Create and insert label for default case */

	lblno = cgen_new_label_num(cgproc);

	rc = cgen_create_label(cgproc, "default", lblno,
	    &cgproc->cur_switch->dlabel);
	if (rc != EOK)
		goto error;

	ir_lblock_append(lblock, cgproc->cur_switch->dlabel, NULL);

	return EOK;
error:
	return rc;
}

/** Generate code for goto label.
 *
 * @param cgproc Code generator for procedure
 * @param aglabel AST goto label
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_glabel(cgen_proc_t *cgproc, ast_glabel_t *aglabel,
    ir_lblock_t *lblock)
{
	comp_tok_t *tok;
	char *glabel = NULL;
	int rc;

	tok = (comp_tok_t *) aglabel->tlabel.data;

	rc = cgen_create_goto_label(cgproc, tok->tok.text,
	    &glabel);
	if (rc != EOK)
		goto error;

	rc = labels_define_label(cgproc->labels, &tok->tok);
	if (rc == EEXIST) {
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Duplicate label '%s'.\n", tok->tok.text);
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}
	if (rc != EOK)
		goto error;

	ir_lblock_append(lblock, glabel, NULL);

	free(glabel);
	return EOK;
error:
	if (glabel != NULL)
		free(glabel);
	return rc;
}

/** Check if expression value is used.
 *
 * This function is called whenever the result of an expression is to be
 * ignored. It checks whether it is okay to ignore the expression result
 * (e.g. because it is intrinsically used (++i) or void (calling function
 * returing void, explicitly cast to void).
 *
 * If it is not OK to ignore the return value, it will produce a warning.
 *
 * @param cgproc Code generator for procedure
 * @param expr Expression
 * @param ares Expression result
 */
static void cgen_expr_check_unused(cgen_proc_t *cgproc, ast_node_t *expr,
    cgen_eres_t *ares)
{
	ast_tok_t *atok;
	comp_tok_t *catok;
	ast_tok_t *btok;
	comp_tok_t *cbtok;

	if (!ares->valused) {
		atok = ast_tree_first_tok(expr);
		btok = ast_tree_last_tok(expr);
		catok = (comp_tok_t *) atok->data;
		cbtok = (comp_tok_t *) btok->data;
		lexer_dprint_tok_range(&catok->tok, &catok->tok.bpos,
		    &cbtok->tok.epos, stderr);
		fprintf(stderr, ": Warning: Computed expression value is not "
		    "used.\n");
		++cgproc->cgen->warnings;
	}
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

	cgen_eres_init(&ares);

	/* Compute the value of the expression (e.g. read volatile variable) */
	rc = cgen_expr_rvalue(cgproc, stexpr->expr, lblock, &ares);
	if (rc != EOK)
		goto error;

	/*
	 * If the expression computes a value that is not used within
	 * the expression itself (e.g. i + 1), generate a warning.
	 */
	cgen_expr_check_unused(cgproc, stexpr->expr, &ares);

	/* Ignore the value of the expression */
	cgen_eres_fini(&ares);
	return EOK;
error:
	return rc;
}

/** Generate code for declaring a local variable.
 *
 * @param cgproc Code generator for procedure
 * @param stdecln AST declaration statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_lvar(cgen_proc_t *cgproc, ast_sclass_type_t sctype,
    cgtype_t *dtype, comp_tok_t *ident, ir_lblock_t *lblock)
{
	char *vident = NULL;
	ir_lvar_t *lvar;
	ir_texpr_t *vtype = NULL;
	int rc;

	(void) lblock;

	if (sctype != asc_none) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Warning: Unimplemented storage class specifier.\n");
		++cgproc->cgen->warnings;
		rc = EINVAL;
		goto error;
	}

	/* Generate an IR variable name */
	rc = cgen_create_loc_var_name(cgproc, ident->tok.text, &vident);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	/* Insert identifier into current scope */
	rc = scope_insert_lvar(cgproc->cur_scope, &ident->tok,
	    dtype, vident);
	if (rc != EOK) {
		if (rc == EEXIST) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Duplicate identifier '%s'.\n",
			    ident->tok.text);
			cgproc->cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}
		goto error;
	}

	rc = cgen_cgtype(cgproc->cgen, dtype, &vtype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(dtype);
	dtype = NULL;

	rc = ir_lvar_create(vident, vtype, &lvar);
	if (rc != EOK)
		goto error;

	free(vident);
	vident = NULL;
	vtype = NULL; /* ownership transferred */
	ir_proc_append_lvar(cgproc->irproc, lvar);

	return EOK;
error:
	cgtype_destroy(dtype);
	if (vident != NULL)
		free(vident);
	if (vtype != NULL)
		ir_texpr_destroy(vtype);
	return rc;
}

/** Generate code for local variable declaration statement.
 *
 * @param cgproc Code generator for procedure
 * @param stdecln Declaration statement for local variables
 * @param sctype Storage class type
 * @param stype Type based on declaration specifiers
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_stdecln_lvars(cgen_proc_t *cgproc, ast_stdecln_t *stdecln,
    ast_sclass_type_t sctype, cgtype_t *stype, ir_lblock_t *lblock)
{
	ast_tok_t *atok;
	ast_idlist_entry_t *identry;
	comp_tok_t *tok;
	ast_tok_t *aident;
	comp_tok_t *ident;
	scope_member_t *member;
	cgtype_t *dtype = NULL;
	int rc;

	(void) lblock;

	if (sctype != asc_none) {
		atok = ast_tree_first_tok(&stdecln->dspecs->node);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unimplemented storage class specifier.\n");
		cgproc->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	identry = ast_idlist_first(stdecln->idlist);
	while (identry != NULL) {
		/* Process declarator */
		rc = cgen_decl(cgproc->cgen, cgproc->cur_scope, stype,
		    identry->decl, identry->aslist, &dtype);
		if (rc != EOK)
			goto error;

		/* Register assignment */
		if (identry->regassign != NULL) {
			tok = (comp_tok_t *) identry->regassign->tasm.data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Variable register assignment (unimplemented).\n");
			cgproc->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		/* Attribute specifier list */
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
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Declaration of '%s' "
			    "shadows a wider-scope declaration.\n",
			    ident->tok.text);
			++cgproc->cgen->warnings;
		}

		/* Local variable */
		rc = cgen_lvar(cgproc, sctype, dtype, ident, lblock);
		if (rc != EOK)
			goto error;

		identry = ast_idlist_next(identry);
	}

	return EOK;
error:
	cgtype_destroy(dtype);
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
	cgtype_t *stype = NULL;
	ast_sclass_type_t sctype;
	int rc;

	/* Process declaration specifiers */

	rc = cgen_dspecs(cgproc->cgen, cgproc->cur_scope, stdecln->dspecs,
	    &sctype, &stype);
	if (rc != EOK)
		goto error;

	if (sctype == asc_typedef) {
		rc = cgen_typedef(cgproc->cgen, cgproc->cur_scope,
		    stdecln->idlist, stype);
		if (rc != EOK)
			goto error;
	} else {
		rc = cgen_stdecln_lvars(cgproc, stdecln, sctype, stype, lblock);
		if (rc != EOK)
			goto error;
	}

	cgtype_destroy(stype);
	return EOK;
error:
	cgtype_destroy(stype);
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
		atok = ast_tree_first_tok(stmt);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This statement type is not implemented.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_break:
		rc = cgen_break(cgproc, (ast_break_t *) stmt->ext, lblock);
		break;
	case ant_continue:
		rc = cgen_continue(cgproc, (ast_continue_t *) stmt->ext, lblock);
		break;
	case ant_goto:
		rc = cgen_goto(cgproc, (ast_goto_t *) stmt->ext, lblock);
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
		rc = cgen_switch(cgproc, (ast_switch_t *) stmt->ext, lblock);
		break;
	case ant_clabel:
		rc = cgen_clabel(cgproc, (ast_clabel_t *) stmt->ext, lblock);
		break;
	case ant_dlabel:
		rc = cgen_dlabel(cgproc, (ast_dlabel_t *) stmt->ext, lblock);
		break;
	case ant_glabel:
		rc = cgen_glabel(cgproc, (ast_glabel_t *) stmt->ext, lblock);
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
			goto error;

		stmt = ast_block_next(stmt);
	}

	/* Check for defined, but unused, identiiers */
	cgen_check_scope_unused(cgproc, block_scope);

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

/** Generate return instruction.
 *
 * @param cgproc Code generator for procedure
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_ret(cgen_proc_t *cgproc, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	int rc;

	(void) cgproc;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	instr->itype = iri_ret;
	instr->width = 0;
	instr->dest = NULL;
	instr->op1 = NULL;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	return EOK;
error:
	ir_instr_destroy(instr);
	return rc;
}

/** Generate code for function arguments definition.
 *
 * Add arguments to IR procedure based on CG function type.
 *
 * @param cgen Code generator
 * @param ftype Function type
 * @param irproc IR procedure to which the arguments should be appended
 * @return EOK on success or an error code
 */
static int cgen_fun_args(cgen_t *cgen, cgtype_t *ftype, ir_proc_t *proc)
{
	ir_proc_arg_t *iarg;
	ir_texpr_t *atype = NULL;
	char *arg_ident = NULL;
	cgtype_func_t *dtfunc;
	cgtype_func_arg_t *dtarg;
	cgtype_t *stype;
	unsigned next_var;
	int rc;
	int rv;

	assert(ftype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)ftype->ext;

	next_var = 0;

	/* Arguments */
	dtarg = cgtype_func_first(dtfunc);
	while (dtarg != NULL) {
		assert(dtarg != NULL);
		stype = dtarg->atype;

		rv = asprintf(&arg_ident, "%%%d", next_var++);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		rc = cgen_cgtype(cgen, stype, &atype);
		if (rc != EOK)
			goto error;

		rc = ir_proc_arg_create(arg_ident, atype, &iarg);
		if (rc != EOK)
			goto error;

		free(arg_ident);
		arg_ident = NULL;
		atype = NULL; /* ownership transferred */

		ir_proc_append_arg(proc, iarg);
		dtarg = cgtype_func_next(dtarg);
	}

	return EOK;
error:
	if (arg_ident != NULL)
		free(arg_ident);
	if (atype != NULL)
		ir_texpr_destroy(atype);
	return rc;
}

/** Generate IR type expression for CG type.
 *
 * @param cgen Code generator
 * @param cgtype CG type
 * @param rirtexpr Place to store pointer to IR type expression
 * @return EOK on success or an error code
 */
static int cgen_cgtype(cgen_t *cgen, cgtype_t *cgtype, ir_texpr_t **rirtexpr)
{
	cgtype_basic_t *tbasic;
	unsigned bits;
	int rc;

	/* Check the type */
	if (cgtype->ntype == cgn_basic) {
		/* Void? */
		tbasic = (cgtype_basic_t *)cgtype->ext;
		if (tbasic->elmtype == cgelm_void)
			return EOK;

		bits = cgen_basic_type_bits(cgen,
		    (cgtype_basic_t *)cgtype->ext);
		if (bits == 0) {
			fprintf(stderr, "cgen_cgtype: Unimplemented type.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = ir_texpr_int_create(bits, rirtexpr);
		if (rc != EOK)
			goto error;

	} else if (cgtype->ntype == cgn_pointer) {
		/* Pointer */

		rc = ir_texpr_ptr_create(cgen_pointer_bits, rirtexpr);
		if (rc != EOK)
			goto error;
	} else {
		fprintf(stderr, "cgen_cgtype: Unimplemented type.\n");
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	return EOK;
error:
	return rc;
}

/** Generate code for function return type.
 *
 * Add return type to IR procedure based on CG function type.
 *
 * @param cgen Code generator
 * @param ftype Function type
 * @param irproc IR procedure to which the return type should be added
 * @return EOK on success or an error code
 */
static int cgen_fun_rtype(cgen_t *cgen, cgtype_t *ftype, ir_proc_t *proc)
{
	cgtype_func_t *dtfunc;
	cgtype_t *stype;
	int rc;

	assert(ftype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)ftype->ext;
	stype = dtfunc->rtype;

	rc = cgen_cgtype(cgen, stype, &proc->rtype);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Process function definition attribute 'usr'.
 *
 * @param cgproc Code generator for procedure
 */
static int cgen_fundef_attr_usr(cgen_proc_t *cgproc, ast_aspec_attr_t *attr)
{
	comp_tok_t *tok;
	ir_proc_attr_t *irattr = NULL;
	int rc;

	if (attr->have_params) {
		tok = (comp_tok_t *) attr->tlparen.data;

		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Attribute 'usr' should not have any "
		    "arguments.\n");
		cgproc->cgen->error = true; // XXX
		return EINVAL;
	}

	rc = ir_proc_attr_create("@usr", &irattr);
	if (rc != EOK)
		return rc;

	ir_proc_append_attr(cgproc->irproc, irattr);
	return EOK;
}

/** Process one attribute of function definition.
 *
 * @param cgproc Code generator for procedure
 */
static int cgen_fundef_attr(cgen_proc_t *cgproc, ast_aspec_attr_t *attr)
{
	comp_tok_t *tok;
	int rc;

	tok = (comp_tok_t *) attr->tname.data;

	if (strcmp(tok->tok.text, "usr") == 0) {
		rc = cgen_fundef_attr_usr(cgproc, attr);
		if (rc != EOK)
			return rc;
	} else {
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unknown attribute '%s'.\n",
		    tok->tok.text);
		cgproc->cgen->error = true; // XXX
		return EINVAL;
	}

	return EOK;
}

/** Generate code for function definition.
 *
 * @param cgen Code generator
 * @param gdecln Global declaration that is a function definition
 * @param btype Type derived from declaration specifiers
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_fundef(cgen_t *cgen, ast_gdecln_t *gdecln, cgtype_t *btype,
    ir_module_t *irmod)
{
	ir_proc_t *proc = NULL;
	ir_lblock_t *lblock = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	cgen_proc_t *cgproc = NULL;
	ast_idlist_entry_t *idle;
	ast_dfun_t *dfun;
	ast_dfun_arg_t *arg;
	ir_texpr_t *atype = NULL;
	char *pident = NULL;
	char *arg_ident = NULL;
	ast_tok_t *atok;
	ast_tok_t *dident;
	comp_tok_t *tok;
	scope_member_t *member;
	symbol_t *symbol;
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	cgtype_func_t *dtfunc;
	cgtype_func_arg_t *dtarg;
	ast_aspec_t *aspec;
	ast_aspec_attr_t *attr;
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

	/* Identifier-declarator list entry */
	idle = ast_idlist_first(gdecln->idlist);
	assert(idle != NULL);
	assert(ast_idlist_next(idle) == NULL);

	/* Process declarator */
	rc = cgen_decl(cgen, cgen->scope, btype, idle->decl, idle->aslist,
	    &dtype);
	if (rc != EOK)
		goto error;

	/* Copy type to symbol */
	rc = cgtype_clone(dtype, &symbol->cgtype);
	if (rc != EOK)
		goto error;

	assert(dtype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)dtype->ext;

	/* Insert identifier into module scope */
	rc = scope_insert_gsym(cgen->scope, &ident->tok, dtype);
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

	lblock = NULL;

	rc = cgen_proc_create(cgen, proc, &cgproc);
	if (rc != EOK)
		goto error;

	/* Remember return type for use in return statement */
	rc = cgtype_clone(dtfunc->rtype, &cgproc->rtype);
	if (rc != EOK)
		goto error;

	/* Attributes */
	if (idle->aslist != NULL) {
		aspec = ast_aslist_first(idle->aslist);
		while (aspec != NULL) {
			attr = ast_aspec_first(aspec);
			while (attr != NULL) {
				rc = cgen_fundef_attr(cgproc, attr);
				if (rc != EOK)
					goto error;

				attr = ast_aspec_next(attr);
			}

			aspec = ast_aslist_next(aspec);
		}
	}

	/* lblock is now owned by proc */
	lblock = NULL;

	/* Get the function declarator */
	dfun = ast_decl_get_dfun(idle->decl);
	if (dfun == NULL) {
		atok = ast_tree_first_tok(idle->decl);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Function declarator required.\n");
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Arguments */
	arg = ast_dfun_first(dfun);
	dtarg = cgtype_func_first(dtfunc);
	while (dtarg != NULL) {
		stype = dtarg->atype;

		dident = ast_decl_get_ident(arg->decl);
		tok = (comp_tok_t *) dident->data;

		if (arg->aslist != NULL) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Atribute specifier not implemented.\n");
			++cgen->warnings;
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

		/* Insert identifier into argument scope */
		rc = scope_insert_arg(cgproc->arg_scope, &tok->tok,
		    stype, arg_ident);
		if (rc != EOK) {
			if (rc == EEXIST) {
				lexer_dprint_tok(&tok->tok, stderr);
				fprintf(stderr, ": Duplicate argument identifier '%s'.\n",
				    tok->tok.text);
				cgen->error = true; // XXX
				rc = EINVAL;
				goto error;
			}
			goto error;
		}

		free(arg_ident);
		arg_ident = NULL;

		arg = ast_dfun_next(arg);
		dtarg = cgtype_func_next(dtarg);
	}

	/* Generate IR procedure arguments */
	rc = cgen_fun_args(cgproc->cgen, dtype, proc);
	if (rc != EOK)
		goto error;

	/* Generate IR return type */
	rc = cgen_fun_rtype(cgen, dtype, proc);
	if (rc != EOK)
		goto error;

	if (gdecln->body != NULL) {
		rc = cgen_block(cgproc, gdecln->body, proc->lblock);
		if (rc != EOK)
			goto error;

		/* Make sure we return if control reaches '}' */
		rc = cgen_ret(cgproc, proc->lblock);
		if (rc != EOK)
			goto error;
	}

	free(pident);
	pident = NULL;

	cgtype_destroy(dtype);
	dtype = NULL;

	ir_module_append(irmod, &proc->decln);
	proc = NULL;

	/* Check for defined, but unused, identifiers */
	cgen_check_scope_unused(cgproc, cgproc->arg_scope);
	cgen_check_scope_unused(cgproc, cgproc->proc_scope);

	/* Check for used, but not defined and defined, but not used labels */
	rc = cgen_check_labels(cgproc, cgproc->labels);
	if (rc != EOK)
		goto error;

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
	if (atype != NULL)
		ir_texpr_destroy(atype);
	if (dtype != NULL)
		cgtype_destroy(dtype);
	return rc;
}

/** Generate code for type definition.
 *
 * @param cgen Code generator
 * @param scope Current scope
 * @param idlist Init-declarator list
 * @param btype Type derived from declaration specifiers
 * @return EOK on success or an error code
 */
static int cgen_typedef(cgen_t *cgen, scope_t *scope, ast_idlist_t *idlist,
    cgtype_t *btype)
{
	ast_idlist_entry_t *idle;
	scope_member_t *member;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	cgtype_t *dtype = NULL;
	int rc;

	/* For all init-declarator list entries */
	idle = ast_idlist_first(idlist);
	while (idle != NULL) {
		/* Process declarator */
		rc = cgen_decl(cgen, scope, btype, idle->decl, idle->aslist,
		    &dtype);
		if (rc != EOK)
			goto error;

		atok = ast_decl_get_ident(idle->decl);
		ctok = (comp_tok_t *)atok->data;

		/* Non-global scope? */
		if (scope != cgen->scope) {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Type definition in a "
			    " non-global scope.\n");
			++cgen->warnings;

			/* Check for shadowing a wider-scope identifier */
			member = scope_lookup(scope->parent,
			    ctok->tok.text);
			if (member != NULL) {
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Warning: Declaration of '%s' "
				    "shadows a wider-scope declaration.\n",
				    ctok->tok.text);
				++cgen->warnings;
			}
		}

		/* Insert typedef into current scope */
		rc = scope_insert_tdef(scope, &ctok->tok, dtype);
		if (rc != EOK) {
			if (rc == EEXIST) {
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Duplicate identifier '%s'.\n",
				    ctok->tok.text);
				cgen->error = true; // XXX
				rc = EINVAL;
				goto error;
			}

			goto error;
		}

		cgtype_destroy(dtype);
		dtype = NULL;

		idle = ast_idlist_next(idle);
	}

	return EOK;
error:
	if (dtype != NULL)
		cgtype_destroy(dtype);
	return rc;
}

/** Generate code for function declaration.
 *
 * @param cgen Code generator
 * @param ftype Function type
 * @param gdecln Global declaration that is a function definition
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_fundecl(cgen_t *cgen, cgtype_t *ftype, ast_gdecln_t *gdecln,
    ir_module_t *irmod)
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

		symbol = symbols_lookup(cgen->symbols, ident->tok.text);
		assert(symbol != NULL);

		rc = cgtype_clone(ftype, &symbol->cgtype);
		if (rc != EOK)
			return rc;

		/* Insert identifier into module scope */
		rc = scope_insert_gsym(cgen->scope, &ident->tok, ftype);
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
 * @param dtype Variable type
 * @param entry Init-declarator list entry that declares a variable
 * @param irmod IR module to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_vardef(cgen_t *cgen, cgtype_t *stype,
    ast_idlist_entry_t *entry, ir_module_t *irmod)
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
	int64_t initval;
	cgtype_elmtype_t elmtype;
	ir_texpr_t *vtype = NULL;
	unsigned bits;
	int rc;

	aident = ast_decl_get_ident(entry->decl);
	ident = (comp_tok_t *) aident->data;

	/* Insert identifier into module scope */
	rc = scope_insert_gsym(cgen->scope, &ident->tok, stype);
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
		rc = cgen_intlit_val(cgen, lit, &initval, &elmtype);
		if (rc != EOK) {
			lexer_dprint_tok(&lit->tok, stderr);
			fprintf(stderr, ": Invalid integer literal.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		// XXX Convert from elmtype to stype
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

	rc = cgen_cgtype(cgen, stype, &vtype);
	if (rc != EOK)
		goto error;

	rc = ir_var_create(pident, vtype, dblock, &var);
	if (rc != EOK)
		goto error;

	free(pident);
	pident = NULL;
	vtype = NULL;
	dblock = NULL;

	if (stype->ntype == cgn_basic) {
		bits = cgen_basic_type_bits(cgen, (cgtype_basic_t *)stype->ext);
		if (bits == 0) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Unimplemented variable type.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = ir_dentry_create_int(bits, initval, &dentry);
		if (rc != EOK)
			goto error;

	} else if (stype->ntype == cgn_pointer) {
		rc = ir_dentry_create_int(16, initval, &dentry);
		if (rc != EOK)
			goto error;
	} else {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Unimplemented variable type.\n");
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

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
	ir_texpr_destroy(vtype);
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
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	ast_sclass_type_t sctype;
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	/* Process declaration specifiers */
	rc = cgen_dspecs(cgen, cgen->scope, gdecln->dspecs, &sctype, &stype);
	if (rc != EOK)
		goto error;

	if (sctype == asc_typedef) {
		rc = cgen_typedef(cgen, cgen->scope, gdecln->idlist, stype);
		if (rc != EOK)
			goto error;
	} else if (gdecln->body != NULL) {
		rc = cgen_fundef(cgen, gdecln, stype, irmod);
		if (rc != EOK)
			goto error;
	} else if (gdecln->idlist != NULL) {
		if (sctype != asc_none) {
			atok = ast_tree_first_tok(&gdecln->dspecs->node);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Unimplemented storage class specifier.\n");
			++cgen->warnings;
		}

		/* Possibly variable declarations */
		entry = ast_idlist_first(gdecln->idlist);
		while (entry != NULL) {
			/* Process declarator */
			rc = cgen_decl(cgen, cgen->scope, stype, entry->decl,
			    entry->aslist, &dtype);
			if (rc != EOK)
				goto error;

			if (ast_decl_is_vardecln(entry->decl)) {
				/* Variable declaration */
				rc = cgen_vardef(cgen, dtype, entry,
				    irmod);
				if (rc != EOK)
					goto error;
			} else {
				/* Assuming it's a function declaration */
				rc = cgen_fundecl(cgen, dtype, gdecln, irmod);
				if (rc != EOK)
					goto error;
			}

			cgtype_destroy(dtype);
			dtype = NULL;

			entry = ast_idlist_next(entry);
		}
	}

	cgtype_destroy(stype);
	stype = NULL;

	return EOK;
error:
	if (stype != NULL)
		cgtype_destroy(stype);
	if (dtype != NULL)
		cgtype_destroy(dtype);
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
	ir_proc_t *proc = NULL;
	symbol_t *symbol;
	char *pident = NULL;
	ir_proc_attr_t *irattr;
	cgtype_func_t *cgfunc;

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

			rc = cgen_fun_args(cgen, symbol->cgtype, proc);
			if (rc != EOK)
				goto error;

			/* Generate IR return type */
			rc = cgen_fun_rtype(cgen, symbol->cgtype, proc);
			if (rc != EOK)
				goto error;

			assert(symbol->cgtype->ntype == cgn_func);
			cgfunc = (cgtype_func_t *)symbol->cgtype->ext;

			if (cgfunc->cconv == cgcc_usr) {
				rc = ir_proc_attr_create("@usr", &irattr);
				if (rc != EOK)
					return rc;

				ir_proc_append_attr(proc, irattr);
			}

			ir_module_append(irmod, &proc->decln);
			proc = NULL;
		}

		symbol = symbols_next(symbol);
	}

	return EOK;
error:
	if (pident != NULL)
		free(pident);
	if (proc != NULL)
		ir_proc_destroy(proc);
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

/** Create new code generator loop tracking record.
 *
 * @param parent Parent loop
 * @param rloop Place to store pointer to new loop tracking record
 * @return EOK on success, ENOMEM if out of memory
 */
int cgen_loop_create(cgen_loop_t *parent, cgen_loop_t **rloop)
{
	cgen_loop_t *loop;

	loop = calloc(1, sizeof(cgen_loop_t));
	if (loop == NULL)
		return ENOMEM;

	loop->parent = parent;
	*rloop = loop;
	return EOK;
}

/** Destroy code generator loop tracking record.
 *
 * @param loop Code generator loop tracking record or @c NULL
 */
void cgen_loop_destroy(cgen_loop_t *loop)
{
	if (loop == NULL)
		return;

	free(loop);
}

/** Create new code generator switch tracking record.
 *
 * @param parent Parent switch
 * @param rswitch Place to store pointer to new switch tracking record
 * @return EOK on success, ENOMEM if out of memory
 */
int cgen_switch_create(cgen_switch_t *parent, cgen_switch_t **rswitch)
{
	cgen_switch_t *cgswitch;

	cgswitch = calloc(1, sizeof(cgen_switch_t));
	if (cgswitch == NULL)
		return ENOMEM;

	cgswitch->parent = parent;
	*rswitch = cgswitch;
	return EOK;
}

/** Destroy code generator switch tracking record.
 *
 * @param cgswitch Code generator switch tracking record or @c NULL
 */
void cgen_switch_destroy(cgen_switch_t *cgswitch)
{
	if (cgswitch == NULL)
		return;

	if (cgswitch->nclabel != NULL)
		free(cgswitch->nclabel);
	if (cgswitch->nblabel != NULL)
		free(cgswitch->nblabel);
	if (cgswitch->dlabel != NULL)
		free(cgswitch->dlabel);
	free(cgswitch);
}

/** Create new loop or switch tracking record.
 *
 * @param parent Parent loop or switch
 * @param rlswitch Place to store pointer to new loop or switch tracking record
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_loop_switch_create(cgen_loop_switch_t *parent,
    cgen_loop_switch_t **rlswitch)
{
	cgen_loop_switch_t *lswitch;

	lswitch = calloc(1, sizeof(cgen_loop_switch_t));
	if (lswitch == NULL)
		return ENOMEM;

	lswitch->parent = parent;
	*rlswitch = lswitch;
	return EOK;
}

/** Destroy code generator break label tracking record.
 *
 * @param lswitch Code generator loop or switch tracking record or @c NULL
 */
static void cgen_loop_switch_destroy(cgen_loop_switch_t *lswitch)
{
	if (lswitch == NULL)
		return;

	free(lswitch);
}
