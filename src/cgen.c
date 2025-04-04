/*
 * Copyright 2025 Jiri Svoboda
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

#include <adt/list.h>
#include <assert.h>
#include <ast.h>
#include <cgenum.h>
#include <cgrec.h>
#include <charcls.h>
#include <comp.h>
#include <cgen.h>
#include <cgtype.h>
#include <inttypes.h>
#include <ir.h>
#include <labels.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
#include <scope.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <symbols.h>

static unsigned cgen_type_sizeof(cgen_t *, cgtype_t *);
static bool cgen_type_is_integral(cgen_t *, cgtype_t *);
static int cgen_proc_create(cgen_t *, ir_proc_t *, cgen_proc_t **);
static void cgen_proc_destroy(cgen_proc_t *);
static int cgen_decl(cgen_t *, cgtype_t *, ast_node_t *,
    ast_aslist_t *, cgtype_t **);
static void cgen_error_expr_not_constant(cgen_t *, ast_tok_t *);
static int cgen_sqlist(cgen_t *, ast_sqlist_t *, cgtype_t **);
static int cgen_dspecs(cgen_t *, ast_dspecs_t *, ast_sclass_type_t *,
    cgen_rd_flags_t *, cgtype_t **);
static int cgen_const_int(cgen_proc_t *, cgtype_elmtype_t, int64_t,
    ir_lblock_t *, cgen_eres_t *);
static int cgen_gsym_ptr(cgen_proc_t *, symbol_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_lvaraddr(cgen_proc_t *, const char *, ir_lblock_t *,
    cgen_eres_t *);
static void cgen_expr_check_unused(cgen_expr_t *, ast_node_t *,
    cgen_eres_t *);
static int cgen_expr_lvalue(cgen_expr_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_expr_rvalue(cgen_expr_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_expr_promoted_rvalue(cgen_expr_t *, ast_node_t *,
    ir_lblock_t *, cgen_eres_t *);
static int cgen_eres_promoted_rvalue(cgen_expr_t *, cgen_eres_t *,
    ir_lblock_t *, cgen_eres_t *);
static int cgen_enum2int(cgen_t *, cgen_eres_t *, cgen_eres_t *, bool *);
static int cgen_int2enum(cgen_expr_t *, cgen_eres_t *, cgtype_t *,
    cgen_eres_t *);
static int cgen_uac_rtype(cgen_expr_t *, cgtype_t *, cgtype_t *, cgtype_t **);
static int cgen_uac(cgen_expr_t *, cgen_eres_t *, cgen_eres_t *, ir_lblock_t *,
    cgen_eres_t *, cgen_eres_t *, cgen_uac_flags_t *);
static int cgen_expr2_uac(cgen_expr_t *, ast_node_t *, ast_node_t *,
    ir_lblock_t *, cgen_eres_t *, cgen_eres_t *, cgen_uac_flags_t *);
static int cgen_expr2lr_uac(cgen_expr_t *, ast_node_t *, ast_node_t *,
    ir_lblock_t *lblock, cgen_eres_t *, cgen_eres_t *, cgen_eres_t *,
    cgen_uac_flags_t *);
static int cgen_expr(cgen_expr_t *, ast_node_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_eres_rvalue(cgen_expr_t *, cgen_eres_t *, ir_lblock_t *,
    cgen_eres_t *);
static int cgen_array_to_ptr(cgen_expr_t *, cgen_eres_t *, cgen_eres_t *);
static int cgen_type_convert(cgen_expr_t *, comp_tok_t *, cgen_eres_t *,
    cgtype_t *, cgen_expl_t, ir_lblock_t *, cgen_eres_t *);
static int cgen_truth_eres_cjmp(cgen_expr_t *, ast_tok_t *, cgen_eres_t *, bool,
    const char *, ir_lblock_t *);
static int cgen_truth_expr_cjmp(cgen_expr_t *, ast_node_t *, bool, const char *,
    ir_lblock_t *);
static int cgen_block(cgen_proc_t *, ast_block_t *, ir_lblock_t *);
static int cgen_gn_block(cgen_proc_t *, ast_block_t *, ir_lblock_t *);
static int cgen_loop_create(cgen_loop_t *, cgen_loop_t **);
static void cgen_loop_destroy(cgen_loop_t *);
static int cgen_switch_create(cgen_switch_t *, cgen_switch_t **);
static void cgen_switch_destroy(cgen_switch_t *);
static int cgen_switch_insert_value(cgen_switch_t *, int64_t);
static int cgen_switch_find_value(cgen_switch_t *, int64_t, cgen_switch_value_t **);
static int cgen_loop_switch_create(cgen_loop_switch_t *, cgen_loop_switch_t **);
static void cgen_loop_switch_destroy(cgen_loop_switch_t *);
static int cgen_ret(cgen_proc_t *, ir_lblock_t *);
static int cgen_stmt(cgen_proc_t *, ast_node_t *, ir_lblock_t *);

static int cgen_cgtype(cgen_t *, cgtype_t *, ir_texpr_t **);
static int cgen_typedef(cgen_t *, ast_tok_t *, ast_idlist_t *, cgtype_t *);
static void cgen_init_destroy(cgen_init_t *);
static int cgen_init_digest(cgen_t *, cgen_init_t *, cgtype_t *, int,
    ir_dblock_t *);
static int cgen_uninit_digest(cgen_t *, cgtype_t *, ir_dblock_t *);
static int cgen_fun_arg_passed_type(cgen_t *, cgtype_t *, cgtype_t **);
static int cgen_init_dentries_cinit(cgen_t *, cgtype_t *, comp_tok_t *,
    ast_cinit_elem_t **, cgen_init_t *);
static int cgen_init_dentries_string(cgen_t *, cgtype_t *, comp_tok_t *,
    ast_estring_t *, ir_dblock_t *);
static int cgen_global_decln(cgen_t *, ast_node_t *);
static int cgen_fundef(cgen_t *, ast_gdecln_t *, ast_sclass_type_t, cgtype_t *);
static int cgen_if(cgen_proc_t *, ast_if_t *, ir_lblock_t *);
static int cgen_while(cgen_proc_t *, ast_while_t *, ir_lblock_t *);
static int cgen_do(cgen_proc_t *, ast_do_t *, ir_lblock_t *);
static int cgen_for(cgen_proc_t *, ast_for_t *, ir_lblock_t *);
static int cgen_switch(cgen_proc_t *, ast_switch_t *, ir_lblock_t *);

enum {
	cgen_pointer_bits = 16,
	cgen_enum_bits = 16,
	cgen_char_bits = 8,
	cgen_char_max = 255,
	cgen_lchar_bits = 16,
	cgen_lchar_max = 65535
};

static int cgen_process_global_decln(void *, parser_t *, ast_node_t **);
static int cgen_process_fundef(void *, parser_t *, ast_gdecln_t *);
static int cgen_process_stmt(void *, parser_t *, ast_node_t **);
static int cgen_process_block(void *, parser_t *, ast_block_t *);
static int cgen_process_if(void *, parser_t *, ast_if_t *);
static int cgen_process_while(void *, parser_t *, ast_while_t *);
static int cgen_process_do(void *, parser_t *, ast_do_t *);
static int cgen_process_for(void *, parser_t *, ast_for_t *);
static int cgen_process_switch(void *, parser_t *, ast_switch_t *);
static bool cgen_ident_is_type(void *, const char *);

static parser_cb_t cgen_parser_cb = {
	.process_global_decln = cgen_process_global_decln,
	.process_fundef = cgen_process_fundef,
	.process_stmt = cgen_process_stmt,
	.process_block = cgen_process_block,
	.process_if = cgen_process_if,
	.process_while = cgen_process_while,
	.process_do = cgen_process_do,
	.process_for = cgen_process_for,
	.process_switch = cgen_process_switch,
	.ident_is_type = cgen_ident_is_type
};

/** Parser callback to process global declaration.
 *
 * @param arg Argument (cgen_t *)
 * @param parser Parser (can be different every time)
 * @param rnode Place to store pointer to AST
 * @return EOK on success or an error code
 */
static int cgen_process_global_decln(void *arg, parser_t *parser,
    ast_node_t **rnode)
{
	cgen_t *cgen = (cgen_t *)arg;
	ast_node_t *decln;
	int rc;

	cgen->parser = parser;

	rc = parser_process_global_decln(cgen->parser, &decln);
	if (rc != EOK)
		return rc;

	rc = cgen_global_decln(cgen, decln);
	if (rc != EOK)
		return rc;

	*rnode = decln;
	return EOK;
}

/** Parser callback to process function definition.
 *
 * @param arg Argument (cgen_t *)
 * @param parser Parser (can be different every time)
 * @param gdecln Global declaration
 * @return EOK on success or an error code
 */
static int cgen_process_fundef(void *arg, parser_t *parser, ast_gdecln_t *gdecln)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgtype_t *stype = NULL;
	ast_sclass_type_t sctype;
	cgen_rd_flags_t flags;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	/* Process declaration specifiers */
	rc = cgen_dspecs(cgen, gdecln->dspecs, &sctype, &flags, &stype);
	if (rc != EOK)
		goto error;

	rc = cgen_fundef(cgen, gdecln, sctype, stype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(stype);
	cgen->parser = old_parser;
	return EOK;
error:
	cgtype_destroy(stype);
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to process statement.
 *
 * @param arg Argument (cgen_proc_t *)
 * @param parser Parser (can be different every time)
 * @param rnode Place to store pointer to AST
 * @return EOK on success or an error code
 */
static int cgen_process_stmt(void *arg, parser_t *parser, ast_node_t **rnode)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgen_proc_t *cgproc = cgen->cur_cgproc;
	ast_node_t *stmt;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	rc = parser_process_stmt(cgproc->cgen->parser, &stmt);
	if (rc != EOK)
		goto error;

	rc = cgen_stmt(cgproc, stmt, cgen->cur_lblock);
	if (rc != EOK)
		goto error;

	cgen->parser = old_parser;
	*rnode = stmt;
	return EOK;
error:
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to process block.
 *
 * @param arg Argument (cgen_proc_t *)
 * @param parser Parser (can be different every time)
 * @param block Block
 * @return EOK on success or an error code
 */
static int cgen_process_block(void *arg, parser_t *parser, ast_block_t *block)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgen_proc_t *cgproc = cgen->cur_cgproc;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	rc = cgen_block(cgproc, block, cgen->cur_lblock);
	if (rc != EOK)
		goto error;

	cgen->parser = old_parser;
	return EOK;
error:
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to process if statement.
 *
 * @param arg Argument (cgen_t *)
 * @param parser Parser (can be different every time)
 * @param aif If statement
 * @return EOK on success or an error code
 */
static int cgen_process_if(void *arg, parser_t *parser, ast_if_t *aif)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgen_proc_t *cgproc = cgen->cur_cgproc;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	rc = cgen_if(cgproc, aif, cgen->cur_lblock);
	if (rc != EOK)
		goto error;

	cgen->parser = old_parser;
	return EOK;
error:
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to process while statement.
 *
 * @param arg Argument (cgen_t *)
 * @param parser Parser (can be different every time)
 * @param awhile While statement
 * @return EOK on success or an error code
 */
static int cgen_process_while(void *arg, parser_t *parser, ast_while_t *awhile)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgen_proc_t *cgproc = cgen->cur_cgproc;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	rc = cgen_while(cgproc, awhile, cgen->cur_lblock);
	if (rc != EOK)
		goto error;

	cgen->parser = old_parser;
	return EOK;
error:
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to process do statement.
 *
 * @param arg Argument (cgen_t *)
 * @param parser Parser (can be different every time)
 * @param ado Do statement
 * @return EOK on success or an error code
 */
static int cgen_process_do(void *arg, parser_t *parser, ast_do_t *ado)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgen_proc_t *cgproc = cgen->cur_cgproc;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	rc = cgen_do(cgproc, ado, cgen->cur_lblock);
	if (rc != EOK)
		goto error;

	cgen->parser = old_parser;
	return EOK;
error:
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to process for statement.
 *
 * @param arg Argument (cgen_t *)
 * @param parser Parser (can be different every time)
 * @param afor For statement
 * @return EOK on success or an error code
 */
static int cgen_process_for(void *arg, parser_t *parser, ast_for_t *afor)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgen_proc_t *cgproc = cgen->cur_cgproc;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	rc = cgen_for(cgproc, afor, cgen->cur_lblock);
	if (rc != EOK)
		goto error;

	cgen->parser = old_parser;
	return EOK;
error:
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to process switch statement.
 *
 * @param arg Argument (cgen_t *)
 * @param parser Parser (can be different every time)
 * @param afor For statement
 * @return EOK on success or an error code
 */
static int cgen_process_switch(void *arg, parser_t *parser,
    ast_switch_t *aswitch)
{
	cgen_t *cgen = (cgen_t *)arg;
	cgen_proc_t *cgproc = cgen->cur_cgproc;
	parser_t *old_parser;
	int rc;

	old_parser = cgen->parser;
	cgen->parser = parser;

	rc = cgen_switch(cgproc, aswitch, cgen->cur_lblock);
	if (rc != EOK)
		goto error;

	cgen->parser = old_parser;
	return EOK;
error:
	cgen->parser = old_parser;
	return rc;
}

/** Parser callback to determine if identifier is a type name.
 *
 * @param arg Argument (cgen_t *)
 * @param ident Identifier
 * @return @c true iff the identifier is a defined type name
 */
static bool cgen_ident_is_type(void *arg, const char *ident)
{
	cgen_t *cgen = (cgen_t *)arg;
	scope_member_t *member;

	/* Check if the identifier is defined. */
	member = scope_lookup(cgen->cur_scope, ident);
	if (member == NULL)
		return false;

	/* Is it actually a type definition? */
	if (member->mtype != sm_tdef)
		return false;

	return true;
}

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

/** Return minimum value of int type
 *
 * @param cgen Code generator
 * @return Minimum int value
 */
static int64_t cgen_int_min(cgen_t *cgen)
{
	(void)cgen;
	return -32768;
}

/** Return maximum value of int type
 *
 * @param cgen Code generator
 * @return Maximum int value
 */
static int64_t cgen_int_max(cgen_t *cgen)
{
	(void)cgen;
	return (int64_t)32767;
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

	assert(cgtype->ntype == cgn_basic || cgtype->ntype == cgn_enum);

	switch (cgtype->ntype) {
	case cgn_basic:
		tbasic = (cgtype_basic_t *)cgtype->ext;
		return cgen_basic_type_signed(cgen, tbasic);
	case cgn_enum:
		return true;
	default:
		assert(false);
	}
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

/** Determine if type is a floating type.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is a floating type
 */
static bool cgen_type_is_floating(cgen_t *cgen, cgtype_t *cgtype)
{
	(void)cgen;
	(void)cgtype;
	return false; // XXX TODO
}

/** Determine if type is an arithmetic type.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is an arithmetic type
 */
static bool cgen_type_is_arithmetic(cgen_t *cgen, cgtype_t *cgtype)
{
	return cgen_type_is_integral(cgen, cgtype) ||
	    cgen_type_is_floating(cgen, cgtype);
}

/** Determine if type is of an integral type (int or enum).
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is an integer type
 */
static bool cgen_type_is_integral(cgen_t *cgen, cgtype_t *cgtype)
{
	if (cgen_type_is_integer(cgen, cgtype))
		return true;

	if (cgtype->ntype == cgn_enum)
		return true;

	return false;
}

/** Determine if type is logic type.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is an integer type
 */
static bool cgen_type_is_logic(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_basic_t *tbasic;

	(void) cgen;

	if (cgtype->ntype != cgn_basic)
		return false;

	tbasic = (cgtype_basic_t *)cgtype->ext;
	return tbasic->elmtype == cgelm_logic;
}

/** Determine if type is a function pointer type.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is a function pointer type
 */
static bool cgen_type_is_fptr(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_pointer_t *tptr;

	(void) cgen;

	if (cgtype->ntype != cgn_pointer)
		return false;

	tptr = (cgtype_pointer_t *)cgtype->ext;
	return tptr->tgtype->ntype == cgn_func;
}

/** Determine if record is defined (or just declared).
 *
 * @param record Record definition
 * @return @c true iff record is defined
 */
static bool cgen_record_is_defined(cgen_record_t *record)
{
	return record->irrecord != NULL;
}

/** Determine if enum is defined (or just declared).
 *
 * @param cgenum Enum definition
 * @return @c true iff enum is defined
 */
static bool cgen_enum_is_defined(cgen_enum_t *cgenum)
{
	return cgenum->defined;
}

/** Determine if type is complete.
 *
 * Structs, unions and arrays may be incomplete. In that case their size,
 * members, are not known.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is incomplete
 */
static bool cgen_type_is_incomplete(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_record_t *trecord;
	cgtype_enum_t *tenum;
	cgtype_array_t *tarray;

	(void)cgen;

	switch (cgtype->ntype) {
	case cgn_basic:
		return false;
	case cgn_pointer:
		return false;
	case cgn_record:
		trecord = (cgtype_record_t *)cgtype->ext;
		return cgen_record_is_defined(trecord->record) == false;
	case cgn_enum:
		tenum = (cgtype_enum_t *)cgtype->ext;
		return cgen_enum_is_defined(tenum->cgenum) == false;
	case cgn_func:
		assert(false);
		return false;
	case cgn_array:
		tarray = (cgtype_array_t *)cgtype->ext;
		if (cgen_type_is_incomplete(cgen, tarray->etype))
			return true;
		return tarray->have_size == false;
	}

	assert(false);
	return false;
}

/** Determine if type is complete or an array of type that is complete.
 *
 * For function arguments the type may be an array of unknown size,
 * as long as the array element type is complete.
 *
 * @param cgen Code generator
 * @param cgtype Code generator type
 * @return @c true iff @a cgtype is incomplete
 */
static bool cgen_type_is_complete_or_array(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_array_t *tarray;

	if (cgtype->ntype == cgn_array) {
		tarray = (cgtype_array_t *)cgtype->ext;
		if (cgen_type_is_incomplete(cgen, tarray->etype))
			return false;
		return true;
	} else {
		return !cgen_type_is_incomplete(cgen, cgtype);
	}
}

/** Determine if two enum types are compatible.
 *
 * @param cgen Code generator
 * @param atype First type, which is an enum type
 * @param btype Second type, which is an enum type
 *
 * @return @c true iff the two enums are compatible
 */
static bool cgen_enum_types_are_compatible(cgen_t *cgen, cgtype_t *atype,
    cgtype_t *btype)
{
	cgtype_enum_t *et1;
	cgtype_enum_t *et2;

	(void)cgen;

	assert(atype->ntype == cgn_enum);
	assert(btype->ntype == cgn_enum);
	et1 = (cgtype_enum_t *)atype->ext;
	et2 = (cgtype_enum_t *)btype->ext;
	return et1->cgenum == et2->cgenum;
}

/** Get record size.
 *
 * XXX We might want to remember this information in the record itself.
 * Computing it again each time is not effictient.
 *
 * @param cgen Code generator
 * @param record Record
 * @return Record size in bytes
 */
static unsigned cgen_record_size(cgen_t *cgen, cgen_record_t *record)
{
	unsigned sz;
	unsigned esz;
	cgen_rec_elem_t *e;

	sz = 0;

	if (record->rtype == cgr_struct) {
		/* Sum up sizes of all elements */
		e = cgen_record_first(record);
		while (e != NULL) {
			sz += cgen_type_sizeof(cgen, e->cgtype);
			e = cgen_record_next(e);
		}
	} else {
		assert(record->rtype == cgr_union);

		/* Size is the maximum of element sizes */
		e = cgen_record_first(record);
		while (e != NULL) {
			esz = cgen_type_sizeof(cgen, e->cgtype);
			if (esz > sz)
				sz = esz;

			e = cgen_record_next(e);
		}
	}

	return sz;
}

/** Return the size of a type in bytes.
 *
 * @param cgen Code generator
 * @param tbasic Basic type
 */
static unsigned cgen_type_sizeof(cgen_t *cgen, cgtype_t *cgtype)
{
	cgtype_basic_t *tbasic;
	cgtype_record_t *trecord;
	cgtype_array_t *tarray;

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
		trecord = (cgtype_record_t *)cgtype->ext;
		return cgen_record_size(cgen, trecord->record);
	case cgn_enum:
		return cgen_enum_bits / 8;
	case cgn_array:
		tarray = (cgtype_array_t *)cgtype->ext;
		assert(tarray->have_size);
		return cgen_type_sizeof(cgen, tarray->etype) * tarray->asize;
	}

	assert(false);
	return 0;
}

/** Get offset of record element.
 *
 * XXX We might want to remember this information in the element itself.
 * Computing it again each time is not effictient.
 *
 * @param cgen Code generator
 * @param elem Record element
 * @return Element offset within its record
 */
static unsigned cgen_rec_elem_offset(cgen_t *cgen, cgen_rec_elem_t *elem)
{
	unsigned off;
	cgen_rec_elem_t *e;

	/* In a union all elements start at zero offset */
	if (elem->record->rtype == cgr_union)
		return 0;

	/* Sum up sizes of all preceding elements */
	off = 0;
	e = cgen_record_first(elem->record);
	while (e != elem) {
		off += cgen_type_sizeof(cgen, e->cgtype);
		e = cgen_record_next(e);
	}

	return off;
}

/** Determine if declaration is just a simple identifier.
 *
 * @param dspecs Declaration specifiers
 * @param decl Declarator
 * @param rtident Place to store pointer to identifier token on success
 * @return EOK on success, EINVAL if declaration is not just a simple
 *         identifier.
 */
static int cgen_decl_is_just_ident(ast_dspecs_t *dspecs, ast_node_t *decl,
    ast_tok_t **rtident)
{
	ast_node_t *dspec;
	ast_tsident_t *tsident;

	dspec = ast_dspecs_first(dspecs);
	if (dspec == NULL)
		return EINVAL;

	if (ast_dspecs_next(dspec) != NULL)
		return EINVAL;

	if (dspec->ntype != ant_tsident)
		return EINVAL;

	if (decl->ntype != ant_dnoident)
		return EINVAL;

	tsident = (ast_tsident_t *)dspec->ext;
	*rtident = &tsident->tident;
	return EOK;
}

/** Determine if string literal is wide.
 *
 * @param lit String literal
 * @return @c true iff string literal is wide
 */
static bool cgen_estring_lit_is_wide(ast_estring_lit_t *lit)
{
	comp_tok_t *tlit;

	tlit = (comp_tok_t *)lit->tlit.data;
	return tlit->tok.text[0] == 'L';
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
	bool toolarge;
	uint64_t val;
	uint64_t nval;
	uint64_t verif;

	val = 0;
	lunsigned = false;
	toolarge = false;

	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		text += 2;

		/* Hexadecimal */
		while (is_hexdigit(*text)) {
			nval = val * 16 + cc_hexdigit_val(*text);

			/* Verify to check for overflow */
			verif = nval / 16;
			if (verif != val)
				toolarge = true;

			val = nval;
			++text;
		}
	} else if (text[0] == '0' && is_num(text[1])) {
		++text;
		/* Octal */
		while (is_octdigit(*text)) {
			nval = val * 8 + cc_octdigit_val(*text);

			/* Verify to check for overflow */
			verif = nval / 8;
			if (verif != val)
				toolarge = true;

			val = nval;
			++text;
		}
	} else {
		/* Decimal */
		while (is_num(*text)) {
			nval = val * 10 + cc_decdigit_val(*text);

			/* Verify to check for overflow */
			verif = nval / 10;
			if (verif != val)
				toolarge = true;

			val = nval;
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

	if (!lunsigned && (uint64_t)val > 0x7fffffffu && elmtype != cgelm_longlong &&
	    elmtype != cgelm_ulonglong) {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Constant should be long long.\n");
		++cgen->warnings;
	} else if ((uint64_t)val > 0xffffffffu && elmtype != cgelm_longlong &&
	    elmtype != cgelm_ulonglong) {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Constant should be long long.\n");
		++cgen->warnings;
	} else if (!lunsigned && (uint64_t)val > 0x7fff && elmtype != cgelm_long &&
	    elmtype != cgelm_ulong && elmtype != cgelm_longlong &&
	    elmtype != cgelm_ulonglong) {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Constant should be long.\n");
		++cgen->warnings;
	} else if ((uint64_t)val > 0xffff && elmtype != cgelm_long &&
	    elmtype != cgelm_ulong && elmtype != cgelm_longlong &&
	    elmtype != cgelm_ulonglong) {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Constant should be long.\n");
		++cgen->warnings;
	}

	if (toolarge) {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Constant is too large.\n");
		++cgen->warnings;
	}

	if (*text != '\0')
		return EINVAL;

	*rval = val;
	*rtype = elmtype;
	return EOK;
}

/** Process escape sequence.
 *
 * @param cgen Code generator
 * @param tlit Literal token
 * @param cp Pointer to character pointer
 * @param max Maximum allowed character value
 * @param rval Place to store value
 * @return EOK on success, EINVAL if token format is invalid
 */
static int cgen_escseq(cgen_t *cgen, comp_tok_t *tlit, const char **cp,
    uint32_t max, uint32_t *rval)
{
	const char *text = *cp;
	unsigned i;
	unsigned val;
	uint32_t c;

	assert(*text == '\\');
	++text;

	switch (*text) {
	case '\'':
	case '"':
	case '?':
	case '\\':
		c = *text++;
		break;
	case 'a':
		c = '\a';
		++text;
		break;
	case 'b':
		c = '\b';
		++text;
		break;
	case 'f':
		c = '\f';
		++text;
		break;
	case 'n':
		c = '\n';
		++text;
		break;
	case 'r':
		c = '\r';
		++text;
		break;
	case 't':
		c = '\t';
		++text;
		break;
	case 'v':
		c = '\v';
		++text;
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		/* Octal escape sequence */
		val = 0;
		i = 0;
		while (i < 3 && is_octdigit(*text)) {
			val = val * 8 + (*text - '0');
			++text;
			++i;
		}
		if (val > max) {
			lexer_dprint_tok(&tlit->tok, stderr);
			fprintf(stderr, ": Warning: Octal escape sequence "
			    "out of range.\n");
			++cgen->warnings;
		}
		c = (uint32_t)val;
		break;
	case 'x':
		++text;

		/* Hexadecimal escape sequence */
		if (!is_hexdigit(*text)) {
			lexer_dprint_tok(&tlit->tok, stderr);
			fprintf(stderr, ": Invalid hexadecimal "
			    "sequence.\n");
			cgen->error = true; // TODO
			return EINVAL;
		}
		val = 0;
		while (is_hexdigit(*text)) {
			val = val * 16 + cc_hexdigit_val(*text);
			++text;
		}
		c = (uint32_t)val;
		if (val > max) {
			lexer_dprint_tok(&tlit->tok, stderr);
			fprintf(stderr, ": Warning: Hexadecimal escape sequence "
			    "out of range.\n");
			++cgen->warnings;
		}
		break;
	default:
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Warning: Unknown escape sequence '\\%c'.\n",
		    *text);
		++cgen->warnings;
		c = *text++;
		break;
	}

	*cp = text;
	*rval = c;
	return EOK;
}

/** Get value of character literal token.
 *
 * @param cgen Code generator
 * @param tlit Literal token
 * @param rval Place to store value
 * @param rtype Place to store elementary type
 * @return EOK on success, EINVAL if token format is invalid
 */
static int cgen_charlit_val(cgen_t *cgen, comp_tok_t *tlit, int64_t *rval,
    cgtype_elmtype_t *rtype)
{
	const char *text = tlit->tok.text;
	cgtype_elmtype_t elmtype;
	bool llong;
	uint32_t max;
	uint32_t c;
	int rc;

	/* Long character literal? */
	llong = false;
	max = cgen_char_max;

	if (text[0] == 'L' && text[1] == '\'') {
		++text;
		llong = true;
		max = cgen_lchar_max;
	}

	if (*text != '\'')
		return EINVAL;
	++text;

	if (*text == '\0')
		return EINVAL;

	/* Character */
	if (*text == '\\') {
		/* Escape sequence */
		rc = cgen_escseq(cgen, tlit, &text, max, &c);
		if (rc != EOK)
			return rc;
	} else {
		c = *text++;
	}

	if (*text != '\'') {
		lexer_dprint_tok(&tlit->tok, stderr);
		fprintf(stderr, ": Multiple characters in character "
		    "constant.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}
	++text;

	if (*text != '\0')
		return EINVAL;

	/* Long? */
	if (llong) {
		elmtype = cgelm_int;
	} else {
		elmtype = cgelm_char;
	}

	*rval = (int64_t)c;
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
	dres->cvint = res->cvint;
	dres->cvsymbol = res->cvsymbol;
	dres->cvknown = res->cvknown;
	dres->tfirst = res->tfirst;
	dres->tlast = res->tlast;
	return EOK;
}

/** Initialize code generator for expession.
 *
 * @param cgexpr Code generator for expression
 */
static void cgen_expr_init(cgen_expr_t *cgexpr)
{
	memset(cgexpr, 0, sizeof(*cgexpr));
}

/** Get value of constant integer expression.
 *
 * @param cgen Code generator
 * @param expr Constant integer expression
 * @param eres Place to store expression result
 * @return EOK on success, EINVAL if expression is not valid
 */
static int cgen_intexpr_val(cgen_t *cgen, ast_node_t *expr, cgen_eres_t *eres)
{
	cgen_expr_t cgexpr;
	ir_lblock_t *lblock = NULL;
	ir_proc_t *irproc = NULL;
	cgen_proc_t *cgproc = NULL;
	cgen_proc_t *old_cgproc;
	int rc;

	old_cgproc = cgen->cur_cgproc;

	/*
	 * Create a dummy labeled block where the code will be emitted
	 * and then it will be dropped.
	 */
	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create("foo", irl_default, lblock, &irproc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	rc = cgen_proc_create(cgen, irproc, &cgproc);
	if (rc != EOK)
		goto error;

	/* Code generator for an integer constant expression */
	cgen_expr_init(&cgexpr);
	cgexpr.cgen = cgen;
	cgexpr.cgproc = cgproc;
	cgexpr.cexpr = true;
	cgexpr.icexpr = true;

	cgen->cur_cgproc = cgproc;

	rc = cgen_expr_rvalue(&cgexpr, expr, irproc->lblock, eres);
	if (rc != EOK)
		goto error;

	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);

	assert(eres->cvknown);
	return EOK;
error:
	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);
	return rc;
}

/** Get value of constant (initializer) expression.
 *
 * The expression is implicitly converted to the type of the variable
 * being initialized (@a dtype).
 *
 * @param cgen Code generator
 * @param expr Constant expression
 * @param itok Initialization token (for printing diagnostics)
 * @param dtype Destination type
 * @param eres Place to store expression result
 * @return EOK on success, EINVAL if expression is not valid
 */
static int cgen_constexpr_val(cgen_t *cgen, ast_node_t *expr, comp_tok_t *itok,
    cgtype_t *dtype, cgen_eres_t *eres)
{
	cgen_expr_t cgexpr;
	ir_lblock_t *lblock = NULL;
	ir_proc_t *irproc = NULL;
	cgen_proc_t *cgproc = NULL;
	cgen_proc_t *old_cgproc;
	cgen_eres_t bres;
	int rc;

	old_cgproc = cgen->cur_cgproc;

	cgen_eres_init(&bres);

	/*
	 * Create a dummy labeled block where the code will be emitted
	 * and then it will be dropped.
	 */
	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create("foo", irl_default, lblock, &irproc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	rc = cgen_proc_create(cgen, irproc, &cgproc);
	if (rc != EOK)
		goto error;

	cgen->cur_cgproc = cgproc;

	/* Code generator for a constant expression */
	cgen_expr_init(&cgexpr);
	cgexpr.cgen = cgen;
	cgexpr.cgproc = cgproc;
	cgexpr.cexpr = true;

	rc = cgen_expr(&cgexpr, expr, irproc->lblock, &bres);
	if (rc != EOK)
		goto error;

	rc = cgen_type_convert(&cgexpr, itok, &bres, dtype, cgen_implicit,
	    irproc->lblock, eres);
	if (rc != EOK)
		goto error;

	if (eres->cvknown != true) {
		cgen_error_expr_not_constant(cgen, ast_tree_first_tok(expr));
		goto error;
	}

	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);
	cgen_eres_fini(&bres);
	return EOK;
error:
	cgen_eres_fini(&bres);
	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);
	return rc;
}

/** Get type of expression (argument to sizeof operator)
 *
 * The expression will not actually be evaluated (i.e. will not have
 * any visible side effects).
 *
 * @param cgen Code generator
 * @param expr Expression
 * @param etype Place to store expression type
 * @return EOK on success, EINVAL if expression is not valid
 */
static int cgen_szexpr_type(cgen_t *cgen, ast_node_t *expr,
    cgtype_t **etype)
{
	cgen_eres_t eres;
	cgen_expr_t cgexpr;
	ir_lblock_t *lblock = NULL;
	ir_proc_t *irproc = NULL;
	cgen_proc_t *cgproc = NULL;
	cgen_proc_t *old_cgproc;
	int rc;

	old_cgproc = cgen->cur_cgproc;
	cgen_eres_init(&eres);

	/*
	 * Create a dummy labeled block where the code will be emitted
	 * and then it will be dropped.
	 */
	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create("foo", irl_default, lblock, &irproc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	rc = cgen_proc_create(cgen, irproc, &cgproc);
	if (rc != EOK)
		goto error;

	cgen->cur_cgproc = cgproc;

	/* Code generator for an integer constant expression */
	cgen_expr_init(&cgexpr);
	cgexpr.cgen = cgproc->cgen;
	cgexpr.cgproc = cgproc;

	rc = cgen_expr(&cgexpr, expr, irproc->lblock, &eres);
	if (rc != EOK)
		goto error;

	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);

	*etype = eres.cgtype;
	eres.cgtype = NULL;

	cgen_eres_fini(&eres);
	return EOK;
error:
	cgen_eres_fini(&eres);
	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);
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
	int rc;

	cgen = calloc(1, sizeof(cgen_t));
	if (cgen == NULL)
		return ENOMEM;

	rc = scope_create(NULL, &cgen->scope);
	if (rc != EOK) {
		free(cgen);
		return ENOMEM;
	}

	rc = cgen_records_create(&cgen->records);
	if (rc != EOK) {
		scope_destroy(cgen->scope);
		free(cgen);
		return ENOMEM;
	}

	rc = cgen_enums_create(&cgen->enums);
	if (rc != EOK) {
		cgen_records_destroy(cgen->records);
		scope_destroy(cgen->scope);
		free(cgen);
		return ENOMEM;
	}

	cgen->cur_scope = cgen->scope;
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

	/* Track the procedure's labels */
	rc = labels_create(&cgproc->labels);
	if (rc != EOK)
		goto error;

	cgen_expr_init(&cgproc->cgexpr);
	cgproc->cgexpr.cgproc = cgproc;
	cgproc->cgexpr.cgen = cgen;

	cgproc->cgen = cgen;
	cgproc->irproc = irproc;
	cgproc->next_var = 0;
	cgproc->next_label = 0;
	*rcgproc = cgproc;
	return EOK;
error:
	scope_destroy(cgproc->arg_scope);
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
	scope_destroy(cgproc->arg_scope);
	cgtype_destroy(cgproc->rtype);
	if (cgproc->last_arg != NULL)
		free(cgproc->last_arg);
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
		case abts_va_list:
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

/** Generate error: both short and 'xxx' specifier.
 *
 * @param cgen Code generator
 * @param tspec Char specifier
 */
static void cgen_error_short_xxx(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both short and %s specifier.\n", tok->tok.text);

	cgen->error = true; // TODO
}

/** Generate error: both long and 'xxx' specifier.
 *
 * @param cgen Code generator
 * @param tspec Char specifier
 */
static void cgen_error_long_xxx(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both long and %s specifier.\n", tok->tok.text);

	cgen->error = true; // TODO
}

/** Generate error: both signed and 'xxx' specifier.
 *
 * @param cgen Code generator
 * @param tspec Char specifier
 */
static void cgen_error_signed_xxx(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both signed and %s specifier.\n", tok->tok.text);

	cgen->error = true; // TODO
}

/** Generate error: both unsigned and 'xxx' specifier.
 *
 * @param cgen Code generator
 * @param tspec Char specifier
 */
static void cgen_error_unsigned_xxx(cgen_t *cgen, ast_tsbasic_t *tspec)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) tspec->tbasic.data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Both unsigned and %s specifier.\n", tok->tok.text);

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

/** Generate error: invalid use of void value.
 *
 * @param cgen Code generator
 * @param atok Token
 */
static void cgen_error_use_void_value(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *)atok->data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Invalid use of void value.\n");

	cgen->error = true; // TODO
}

/** Generate error: comparison of invalid types.
 *
 * @param cgen Code generator
 * @param atok Operator token
 * @param ltype Left operand type
 * @param rtype Right operand type
 */
static void cgen_error_cmp_invalid(cgen_t *cgen, ast_tok_t *atok,
    cgtype_t *ltype, cgtype_t *rtype)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Comparison of invalid types ");
	(void) cgtype_print(ltype, stderr);
	fprintf(stderr, " and ");
	(void) cgtype_print(rtype, stderr);
	fprintf(stderr, ".\n");

	cgen->error = true; // TODO
}

/** Generate error: Pointers being compared are not constant.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_error_cmp_ptr_nc(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Pointers being compared are not constant.\n");

	cgen->error = true; // TODO
}

/** Generate error: scalar type required.
 *
 * @param cgen Code generator
 * @param atok Token
 */
static void cgen_error_need_scalar(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *)atok->data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Need scalar type.\n");

	cgen->error = true; // TODO
}

/** Generate error: assignment to an array.
 *
 * @param cgen Code generator
 * @param atok Token
 */
static void cgen_error_assign_array(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *)atok->data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Assignment to an array.\n");

	cgen->error = true; // TODO
}

/** Generate error: casting to an array type.
 *
 * @param cgen Code generator
 * @param ctok Token
 */
static void cgen_error_cast_array(cgen_t *cgen, comp_tok_t *ctok)
{
	lexer_dprint_tok(&ctok->tok, stderr);
	fprintf(stderr, ": Casting to an array type.\n");

	cgen->error = true; // TODO
}

/** Generate error: function returning an array.
 *
 * @param cgen Code generator
 * @param atok Token
 */
static void cgen_error_fun_ret_array(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *)atok->data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Function returning an array.\n");

	cgen->error = true; // TODO
}

/** Generate error: expression is not constant.
 *
 * @param cgen Code generator
 * @param atok Token
 */
static void cgen_error_expr_not_constant(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *)atok->data;

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Expression is not constant.\n");

	cgen->error = true; // TODO
}

/** Generate warning: unimplemented type specifier.
 *
 * @param cgen Code generator
 * @param tspec Type specifier
 */
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

/** Generate warning: useless type in empty declaration.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_useless_type(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Useless type in empty declaration.\n");
	++cgen->warnings;
}

/** Generate warning: suspicious arithmetic operation involving enums.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_arith_enum(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Suspicious arithmetic operation "
	    "involving enums.\n");
	++cgen->warnings;
}

/** Generate warning: subtracting different enum types.
 *
 * @param cgen Code generator
 * @param top Operator token
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 */
static void cgen_warn_sub_enum_inc(cgen_t *cgen, ast_tok_t *atok,
    cgen_eres_t *lres, cgen_eres_t *rres)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Subtracting incompatible enum types ");
	(void) cgtype_print(lres->cgtype, stderr);
	fprintf(stderr, " and ");
	(void) cgtype_print(rres->cgtype, stderr);
	fprintf(stderr, ".\n");
	++cgen->warnings;
}

/** Generate warning: initializing enum member from incompatible type.
 *
 * @param cgen Code generator
 * @param top Operator token
 * @param eres Initializer expression result
 */
static void cgen_warn_init_enum_inc(cgen_t *cgen, ast_tok_t *atok,
    cgen_eres_t *eres)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Initializing enum member from incompatible "
	    "type ");
	(void) cgtype_print(eres->cgtype, stderr);
	fprintf(stderr, ".\n");
	++cgen->warnings;
}

/** Generate warning: enum initializer is out of range of type int.
 *
 * @param cgen Code generator
 * @param atok Token for diagnostics
 * @param eres Initializer expression result
 */
static void cgen_warn_init_enum_range(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Enum initializer is out of range "
	    "of int.\n");
	++cgen->warnings;
}

/** Generate warning: suspicious logic operation involving enums.
 *
 * @param cgen Code generator
 * @param atok Token
 */
static void cgen_warn_logic_enum(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Suspicious logic operation "
	    "involving enums.\n");
	++cgen->warnings;
}

/** Generate warning: comparison of different enum types.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_cmp_enum_inc(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Comparison of different enum types.\n'");
	++cgen->warnings;
}

/** Generate warning: comparison enum and non-enum type.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_cmp_enum_mix(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Comparison of enum and non-enum type.\n'");
	++cgen->warnings;
}

/** Generate warning: bitwise operation on different enum types.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_bitop_enum_inc(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Bitwise operation on different enum types.\n'");
	++cgen->warnings;
}

/** Generate warning: bitwise operation on enum and non-enum type.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_bitop_enum_mix(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Bitwise operation on enum and non-enum type.\n'");
	++cgen->warnings;
}

/** Generate warning: suspicious arithmetic operation involving truth values.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_arith_truth(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Suspicious arithmetic operation "
	    "involving truth values.\n");
	++cgen->warnings;
}

/** Generate warning: comparison of truth value and non-truth type.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_cmp_truth_mix(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Comparison of truth value and "
	    "non-truth type.\n'");
	++cgen->warnings;
}

/** Generate warning: bitwise operation on signed integers.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_bitop_signed(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Bitwise operation on signed integer(s).\n");
	++cgen->warnings;
}

/** Generate warning: bitwise operation on negative number(s).
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_bitop_negative(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Bitwise operation on negative number(s).\n");
	++cgen->warnings;
}

/** Generate warning: unsigned comparison of mixed-sign integers.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_cmp_sign_mix(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Unsigned comparison of mixed-sign "
	    "integers.\n");
	++cgen->warnings;
}

/** Generate warning: unsigned division of mixed-sign integers.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_div_sign_mix(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Unsigned division of mixed-sign "
	    "integers.\n");
	++cgen->warnings;
}

/** Generate warning: negative number converted to unsigned before comparison.
 *
 * @param cgen Code generator
 * @param atok Operator token
 */
static void cgen_warn_cmp_neg_unsigned(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Negative number converted to unsigned "
	    "before comparison.\n");
	++cgen->warnings;
}

/** Generate warning: integer arithmetic overflow.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_integer_overflow(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Integer arithmetic overflow.\n");
	++cgen->warnings;
}

/** Generate warning: shift amount exceeds operand width.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_shift_exceed_bits(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Shift amount exceeds operand width.\n");
	++cgen->warnings;
}

/** Generate warning: division by zero.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_div_by_zero(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Division by zero.\n");
	++cgen->warnings;
}

/** Generate warning: shift is negative.
 *
 * @param cgen Code generator
 * @param top Operator token
 */
static void cgen_warn_shift_negative(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Shift is negative.\n");
	++cgen->warnings;
}

/** Generate warning: number sign changed in conversion.
 *
 * @param cgen Code generator
 * @param ctok Operator token
 */
static void cgen_warn_sign_changed(cgen_t *cgen, comp_tok_t *ctok)
{
	lexer_dprint_tok(&ctok->tok, stderr);
	fprintf(stderr, ": Warning: Number sign changed in conversion.\n");
	++cgen->warnings;
}

/** Generate warning: conversion from 'x' to 'y' changes signedness.
 *
 * @param cgen Code generator
 * @param ctok Operator token
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 */
static void cgen_warn_sign_convert(cgen_t *cgen, comp_tok_t *ctok,
    cgen_eres_t *lres, cgen_eres_t *rres)
{
	lexer_dprint_tok(&ctok->tok, stderr);
	fprintf(stderr, ": Warning: Conversion from ");
	(void) cgtype_print(lres->cgtype, stderr);
	fprintf(stderr, " to ");
	(void) cgtype_print(rres->cgtype, stderr);
	fprintf(stderr, " changes signedness.\n");
	++cgen->warnings;
}

/** Generate warning: number changed in conversion.
 *
 * @param cgen Code generator
 * @param ctok Operator token
 */
static void cgen_warn_number_changed(cgen_t *cgen, comp_tok_t *ctok)
{
	lexer_dprint_tok(&ctok->tok, stderr);
	fprintf(stderr, ": Warning: Number changed in conversion.\n");
	++cgen->warnings;
}

/** Generate warning: case value is out of range of type.
 *
 * @param cgen Code generator
 * @param atok Case expression token
 * @param cgtype Type of switch expression
 */
static void cgen_warn_case_value_range(cgen_t *cgen, ast_tok_t *atok,
    cgtype_t *cgtype)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Case value is out of range of ");
	(void) cgtype_print(cgtype, stderr);
	fprintf(stderr, ".\n");
	++cgen->warnings;
}

/** Generate warning: case value is not boolean.
 *
 * @param cgen Code generator
 * @param atok Case expression token
 */
static void cgen_warn_case_value_not_bool(cgen_t *cgen, ast_tok_t *atok)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Case value is not boolean.\n");
	++cgen->warnings;
}

/** Generate warning: case value is not in enum.
 *
 * @param cgen Code generator
 * @param atok Case expression token
 * @param cgtype Enum type
 */
static void cgen_warn_case_value_not_in_enum(cgen_t *cgen, ast_tok_t *atok,
    cgtype_t *cgtype)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Case value is not in ");
	(void) cgtype_print(cgtype, stderr);
	fprintf(stderr, ".\n");
	++cgen->warnings;
}

/** Generate warning: comparison of incompatible pointer types.
 *
 * @param cgen Code generator
 * @param atok Operator token
 * @param ltype Left operand type
 * @param rtype Right operand type
 */
static void cgen_warn_cmp_incom_ptr(cgen_t *cgen, ast_tok_t *atok,
    cgtype_t *ltype, cgtype_t *rtype)
{
	comp_tok_t *tok;

	tok = (comp_tok_t *) atok->data;
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Comparison of incompatible pointer types ");
	(void) cgtype_print(ltype, stderr);
	fprintf(stderr, " and ");
	(void) cgtype_print(rtype, stderr);
	fprintf(stderr, ".\n");
	++cgen->warnings;
}

/** Generate warning: truth value used as an integer.
 *
 * @param cgen Code generator
 * @param ctok Operator token
 */
static void cgen_warn_truth_as_int(cgen_t *cgen, comp_tok_t *ctok)
{
	lexer_dprint_tok(&ctok->tok, stderr);
	fprintf(stderr, ": Warning: Truth value used as an integer.\n");
	++cgen->warnings;
}

/** Generate warning: array index is negative.
 *
 * @param cgen Code generator
 * @param tok Operator token
 */
static void cgen_warn_array_index_negative(cgen_t *cgen, comp_tok_t *tok)
{
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Array index is negative.\n");
	++cgen->warnings;
}

/** Generate warning: array index is out of bounds.
 *
 * @param cgen Code generator
 * @param tok Operator token
 */
static void cgen_warn_array_index_oob(cgen_t *cgen, comp_tok_t *tok)
{
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Array index is out of bounds.\n");
	++cgen->warnings;
}

/** Generate warning: array index is out of bounds.
 *
 * @param cgen Code generator
 * @param tok Operator token
 */
static void cgen_warn_init_field_overwritten(cgen_t *cgen, comp_tok_t *tok)
{
	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Warning: Initializer field overwritten.\n");
	++cgen->warnings;
}

/** Generate code for record definition.
 *
 * @param cgen Code generator
 * @param record Record definition
 * @return EOK on success or an error code
 */
static int cgen_record(cgen_t *cgen, cgen_record_t *record)
{
	ir_texpr_t *irtype = NULL;
	cgen_rec_elem_t *elem;
	char *irident = NULL;
	int rc;
	int rv;

	elem = cgen_record_first(record);
	while (elem != NULL) {
		rc = cgen_cgtype(cgen, elem->cgtype, &irtype);
		if (rc != EOK)
			goto error;

		rv = asprintf(&irident, "@%s", elem->ident);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		rc = ir_record_append(record->irrecord, irident,
		    irtype, NULL);
		if (rc != EOK)
			goto error;

		free(irident);
		irident = NULL;
		ir_texpr_destroy(irtype);
		irtype = NULL;

		elem = cgen_record_next(elem);
	}

	return EOK;
error:
	ir_texpr_destroy(irtype);
	if (irident != NULL)
		free(irident);
	return rc;
}

/** Generate code for type identifer.
 *
 * @param cgen Code generator
 * @param itok Identifier token
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_tident(cgen_t *cgen, ast_tok_t *itok, cgtype_t **rstype)
{
	comp_tok_t *ident;
	scope_member_t *member;
	int rc;

	ident = (comp_tok_t *)itok->data;

	/* Check if the type is defined */
	member = scope_lookup(cgen->cur_scope, ident->tok.text);
	if (member == NULL) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Undefined type name '%s'.\n",
		    ident->tok.text);
		cgen->error = true; // TODO
		return EINVAL;
	}

	/* Is it actually a type definition? */
	if (member->mtype != sm_tdef) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Identifer '%s' is not a type.\n",
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

/** Generate code for identifier type specifier.
 *
 * @param cgen Code generator
 * @param tsident Identifier type specifier
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_tsident(cgen_t *cgen, ast_tsident_t *tsident,
    cgtype_t **rstype)
{
	return cgen_tident(cgen, &tsident->tident, rstype);
}

/** Generate code for record type specifier element.
 *
 * In outher words, one field of a struct or union definition.
 *
 * @param cgen Code generator
 * @param elem Record type specifier element
 * @param record Record definition
 * @return EOK on success or an error code
 */
static int cgen_tsrecord_elem(cgen_t *cgen, ast_tsrecord_elem_t *elem,
    cgen_record_t *record)
{
	ast_dlist_entry_t *dlentry;
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	comp_tok_t *ctok;
	ir_texpr_t *irtype = NULL;
	char *irident;
	int rc;
	int rv;

	/* When compiling we should not get a macro declaration */
	assert(elem->mdecln == NULL);

	rc = cgen_sqlist(cgen, elem->sqlist, &stype);
	if (rc != EOK)
		goto error;

	dlentry = ast_dlist_first(elem->dlist);
	while (dlentry != NULL) {
		rc = cgen_decl(cgen, stype, dlentry->decl,
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

		rc = cgen_record_append(record, ident->tok.text, dtype);
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

		/* Check for function type */
		if (dtype->ntype == cgn_func) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Record member is a function.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		/* Check type for completeness */
		if (cgen_type_is_incomplete(cgen, dtype)) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Record member has incomplete type.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = cgen_cgtype(cgen, dtype, &irtype);
		if (rc != EOK)
			goto error;

		rv = asprintf(&irident, "@%s", ident->tok.text);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		free(irident);
		irident = NULL;
		ir_texpr_destroy(irtype);
		irtype = NULL;

		cgtype_destroy(dtype);
		dtype = NULL;

		dlentry = ast_dlist_next(dlentry);
	}

	cgtype_destroy(stype);
	return EOK;
error:
	ir_texpr_destroy(irtype);
	cgtype_destroy(stype);
	cgtype_destroy(dtype);
	return rc;
}

/** Generate code for record type specifier.
 *
 * @param cgen Code generator
 * @param tsrecord Record type specifier
 * @param rflags Place to store flags
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_tsrecord(cgen_t *cgen, ast_tsrecord_t *tsrecord,
    cgen_rd_flags_t *rflags, cgtype_t **rstype)
{
	comp_tok_t *ident_tok;
	const char *ident;
	scope_member_t *member;
	scope_member_t *dmember;
	ast_tok_t *tok;
	comp_tok_t *ctok;
	cgtype_record_t *rectype;
	const char *rtype;
	ir_record_type_t irrtype;
	scope_rec_type_t srtype;
	cgen_rec_type_t cgrtype;
	ast_tsrecord_elem_t *elem;
	cgen_record_t *record = NULL;
	ir_record_t *irrecord = NULL;
	ir_decln_t *decln;
	cgen_rd_flags_t flags;
	char *irident = NULL;
	unsigned seqno;
	int rc;
	int rv;

	flags = cgrd_none;

	if (tsrecord->rtype == ar_struct) {
		rtype = "struct";
		irrtype = irrt_struct;
		cgrtype = cgr_struct;
		srtype = sr_struct;
	} else {
		rtype = "union";
		irrtype = irrt_union;
		cgrtype = cgr_union;
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

	if (tsrecord->have_ident) {
		ident_tok = (comp_tok_t *)tsrecord->tident.data;
		ident = ident_tok->tok.text;
	} else {
		/* Point to 'struct/union' keyword if no identifier */
		ident_tok = (comp_tok_t *)tsrecord->tsu.data;
		ident = "<anonymous>";
	}

	if (tsrecord->have_def && cgen->cur_scope->parent != NULL) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Warning: Definition of '%s %s' in a "
		    "non-global scope.\n", rtype, ident);
		++cgen->warnings;

		member = scope_lookup_tag(cgen->cur_scope->parent,
		    ident);
		if (member != NULL) {
			lexer_dprint_tok(&ident_tok->tok, stderr);
			fprintf(stderr, ": Warning: Definition of '%s %s' "
			    "shadows a wider-scope struct, union or enum "
			    "definition.\n", rtype, ident);
			++cgen->warnings;
		}
	}

	if (tsrecord->have_def && cgen->tsrec_cnt > 0) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Warning: Definition of '%s %s' inside "
		    "another struct/union definition.\n", rtype, ident);
		++cgen->warnings;
	}

	if (tsrecord->have_def && cgen->arglist_cnt > 0) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Warning: Definition of '%s %s' inside "
		    "parameter list will not be visible outside of function "
		    "declaration/definition.\n", rtype, ident);
		++cgen->warnings;
	}

	/* Look for previous definition in current scope */
	member = scope_lookup_tag_local(cgen->cur_scope, ident);

	/* If already exists, but as a different kind of tag */
	if (member != NULL && (member->mtype != sm_record ||
	    member->m.record.srtype != srtype)) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Redefinition of '%s' as a different kind of tag.\n",
		    member->tident->text);
		cgen->error = true; // TODO
		return EINVAL;
	}

	/* Look for a usable previous declaration */
	dmember = scope_lookup_tag(cgen->cur_scope, ident);
	if (dmember != NULL && (dmember->mtype != sm_record ||
	    dmember->m.record.srtype != srtype))
		dmember = NULL;

	/* If already exists, is defined */
	if (member != NULL && cgen_record_is_defined(member->m.record.record)) {
		/* Record was previously defined */
		flags |= cgrd_prevdef;

		if (tsrecord->have_def) {
			lexer_dprint_tok(&ident_tok->tok, stderr);
			fprintf(stderr, ": Redefinition of '%s'.\n",
			    member->tident->text);
			cgen->error = true; // TODO
			return EINVAL;
		}
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

	if (dmember != NULL) {
		/* Already declared */
		record = dmember->m.record.record;
		irrecord = record->irrecord;
		flags |= cgrd_prevdecl;
	}

	/*
	 * We only create a new scope member and cgen_record either if there
	 * is no previous declaration or if we're shadowing a wider-scope
	 * declaration by a new defininiton.
	 */
	if (dmember == NULL || (member == NULL && tsrecord->have_def)) {
		if (tsrecord->have_ident) {
			/* Construct IR identifier */
			rv = asprintf(&irident, "@@%s", ident);
			if (rv < 0)
				return ENOMEM;

			/*
			 * Since multiple structs/unions with the same name could
			 * be defined in different scopes, check for conflict.
			 * If found, try numbered version with increasing numbers.
			 *
			 * Don't bother with efficiency, this is not something
			 * that should happen a lot - we warn against doing it,
			 * afterall.
			 */

			seqno = 0;
			rc = ir_module_find(cgen->irmod, irident, &decln);
			while (rc == EOK) {
				free(irident);

				/* Construct IR identifier with number */
				++seqno;
				rv = asprintf(&irident, "@@%s.%d", ident, seqno);
				if (rv < 0)
					return ENOMEM;

				rc = ir_module_find(cgen->irmod, irident, &decln);
			}
		} else {
			++cgen->anon_tag_cnt;
			rv = asprintf(&irident, "@@%d", cgen->anon_tag_cnt);
			if (rv < 0)
				return ENOMEM;
		}

		/* Create new record definition */
		rc = cgen_record_create(cgen->records, cgrtype,
		    tsrecord->have_ident ? ident : NULL, irident, irrecord,
		    &record);
		if (rc != EOK) {
			free(irident);
			return rc;
		}

		free(irident);
		irident = NULL;

		if (dmember != NULL)
			dmember->m.record.record = record;

		if (tsrecord->have_ident) {
			/* Insert new record scope member */
			rc = scope_insert_record(cgen->cur_scope, &ident_tok->tok,
			    srtype, record, &dmember);
			if (rc != EOK)
				return EINVAL;
		}
	}

	/* Catch nested redefinitions */
	if (tsrecord->have_def) {
		if (tsrecord->have_def && record->defining) {
			lexer_dprint_tok(&ident_tok->tok, stderr);
			fprintf(stderr, ": Nested redefinition of '%s'.\n",
			    record->cident);
			cgen->error = true; // TODO
			return EINVAL;
		}
		record->defining = true;
	}

	++cgen->tsrec_cnt;
	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		rc = cgen_tsrecord_elem(cgen, elem, record);
		if (rc != EOK)  {
			assert(cgen->tsrec_cnt > 0);
			--cgen->tsrec_cnt;
			if (tsrecord->have_def)
				record->defining = false;
			return EINVAL;
		}

		elem = ast_tsrecord_next(elem);
	}
	assert(cgen->tsrec_cnt > 0);
	--cgen->tsrec_cnt;

	if (tsrecord->have_def)
		record->defining = false;

	if (tsrecord->have_def) {
		/* Create IR definition */
		rc = ir_record_create(record->irident, irrtype, &irrecord);
		if (rc != EOK)
			return rc;

		record->irrecord = irrecord;

		rc = cgen_record(cgen, record);
		if (rc != EOK)
			return rc;

		ir_module_append(cgen->irmod, &irrecord->decln);
	}

	/* Record type */
	rc = cgtype_record_create(record, &rectype);
	if (rc != EOK)
		return rc;

	if (tsrecord->have_ident)
		flags |= cgrd_ident;
	if (tsrecord->have_def)
		flags |= cgrd_def;

	*rflags = flags;
	*rstype = &rectype->cgtype;
	return EOK;
}

/** Generate code for enum type specifier element.
 *
 * In outher words, one field of a struct or union definition.
 *
 * @param cgen Code generator
 * @param elem Enum type specifier element
 * @param cgenum Enum definition
 * @return EOK on success or an error code
 */
static int cgen_tsenum_elem(cgen_t *cgen, ast_tsenum_elem_t *elem,
    cgen_enum_t *cgenum)
{
	cgtype_t *stype = NULL;
	cgen_enum_elem_t *eelem;
	comp_tok_t *ident;
	scope_member_t *member;
	int64_t value;
	cgen_eres_t eres;
	cgen_eres_t cres;
	cgtype_enum_t *tenum;
	int rc;

	ident = (comp_tok_t *) elem->tident.data;

	cgen_eres_init(&eres);
	cgen_eres_init(&cres);

	if (elem->init != NULL) {
		rc = cgen_intexpr_val(cgen, elem->init, &eres);
		if (rc != EOK)
			goto error;

		assert(eres.cvknown);
		if (eres.cgtype->ntype == cgn_enum) {
			tenum = (cgtype_enum_t *)eres.cgtype->ext;
			if (tenum->cgenum != cgenum &&
			    cgtype_is_strict_enum(eres.cgtype))
				cgen_warn_init_enum_inc(cgen, &elem->tequals,
				    &eres);
		}

		if (eres.cvint < cgen_int_min(cgen) ||
		    eres.cvint > cgen_int_max(cgen)) {
			cgen_warn_init_enum_range(cgen, &elem->tequals);
		}

		value = eres.cvint;
	} else {
		value = cgenum->next_value;
	}

	rc = cgen_enum_append(cgenum, ident->tok.text, value, &eelem);
	if (rc == EEXIST) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Duplicate enum member '%s'.\n",
		    ident->tok.text);
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}
	if (rc != EOK)
		goto error;

	/* Check for shadowing a wider-scope identifier */
	if (cgen->cur_scope->parent != NULL) {
		member = scope_lookup(cgen->cur_scope->parent,
		    ident->tok.text);
		if (member != NULL) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Declaration of '%s' "
			    "shadows a wider-scope declaration.\n",
			    ident->tok.text);
			++cgen->warnings;
		}
	}

	/* Insert identifier into current scope */
	rc = scope_insert_eelem(cgen->cur_scope, &ident->tok, eelem,
	    &member);
	if (rc != EOK) {
		if (rc == EEXIST) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Duplicate identifier '%s'.\n",
			    ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}
		goto error;
	}

	cgen_eres_fini(&eres);
	cgen_eres_fini(&cres);
	cgtype_destroy(stype);
	cgenum->next_value = value + 1;
	return EOK;
error:
	cgen_eres_fini(&eres);
	cgen_eres_fini(&cres);
	cgtype_destroy(stype);
	return rc;
}

/** Generate code for enum type specifier.
 *
 * @param cgen Code generator
 * @param tsenum Enum type specifier
 * @param rflags Place to store flags
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_tsenum(cgen_t *cgen, ast_tsenum_t *tsenum,
    cgen_rd_flags_t *rflags, cgtype_t **rstype)
{
	comp_tok_t *ident_tok;
	const char *ident;
	scope_member_t *member;
	scope_member_t *dmember;
	cgtype_enum_t *etype;
	ast_tsenum_elem_t *elem;
	cgen_enum_t *cgenum = NULL;
	cgen_rd_flags_t flags;
	int rc;

	flags = cgrd_none;

	if (tsenum->have_ident) {
		ident_tok = (comp_tok_t *)tsenum->tident.data;
		ident = ident_tok->tok.text;
	} else {
		/* Point to 'enum' keyword if no identifier */
		ident_tok = (comp_tok_t *)tsenum->tenum.data;
		ident = "<anonymous>";
	}

	if (tsenum->have_def && cgen->cur_scope->parent != NULL) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Warning: Definition of 'enum %s' in a "
		    "non-global scope.\n", ident);
		++cgen->warnings;

		member = scope_lookup_tag(cgen->cur_scope->parent,
		    ident);
		if (member != NULL) {
			lexer_dprint_tok(&ident_tok->tok, stderr);
			fprintf(stderr, ": Warning: Definition of 'enum %s' "
			    "shadows a wider-scope struct, union or enum "
			    "definition.\n", ident);
			++cgen->warnings;
		}
	}

	if (tsenum->have_def && cgen->tsrec_cnt > 0) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Warning: Definition of 'enum %s' inside "
		    "struct or union definition.\n", ident);
		++cgen->warnings;
	}

	if (tsenum->have_def && cgen->arglist_cnt > 0) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Warning: Definition of 'enum %s' inside "
		    "parameter list will not be visible outside of function "
		    "declaration/definition.\n", ident);
		++cgen->warnings;
	}

	/* Look for previous definition in current scope */
	member = scope_lookup_tag_local(cgen->cur_scope, ident);

	/* If already exists, but as a different kind of tag */
	if (member != NULL && member->mtype != sm_enum) {
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Redefinition of '%s' as a different kind of tag.\n",
		    member->tident->text);
		cgen->error = true; // TODO
		return EINVAL;
	}

	/* Look for a usable previous declaration */
	dmember = scope_lookup_tag(cgen->cur_scope, ident);
	if (dmember != NULL && dmember->mtype != sm_enum)
		dmember = NULL;

	/* If already exists, is defined */
	if (member != NULL && cgen_enum_is_defined(member->m.menum.cgenum)) {
		/* Enum was previously defined */
		flags |= cgrd_prevdef;

		if (tsenum->have_def) {
			lexer_dprint_tok(&ident_tok->tok, stderr);
			fprintf(stderr, ": Redefinition of '%s'.\n",
			    member->tident->text);
			cgen->error = true; // TODO
			return EINVAL;
		}
	}

	if (dmember != NULL) {
		/* Already declared */
		cgenum = dmember->m.menum.cgenum;
		flags |= cgrd_prevdecl;
	}

	/*
	 * We only create a new scope member and cgen_enum either if there
	 * is no previous declaration or if we're shadowing a wider-scope
	 * declaration by a new defininiton.
	 */
	if (dmember == NULL || (member == NULL && tsenum->have_def)) {

		/* Create new enum definition */
		rc = cgen_enum_create(cgen->enums, tsenum->have_ident ?
		    ident : NULL, &cgenum);
		if (rc != EOK)
			return rc;

		if (dmember != NULL)
			dmember->m.menum.cgenum = cgenum;

		if (tsenum->have_ident) {
			cgenum->named = true;

			/* Insert new enum scope member */
			rc = scope_insert_enum(cgen->cur_scope, &ident_tok->tok,
			    cgenum, &dmember);
			if (rc != EOK)
				return EINVAL;
		}
	}

	elem = ast_tsenum_first(tsenum);
	if (tsenum->have_def && elem == NULL) {
		/* No elements */
		lexer_dprint_tok(&ident_tok->tok, stderr);
		fprintf(stderr, ": Enum '%s' is empty.\n", ident);
		cgen->error = true; // TODO
		return EINVAL;
	}

	while (elem != NULL) {
		rc = cgen_tsenum_elem(cgen, elem, cgenum);
		if (rc != EOK)
			return EINVAL;

		elem = ast_tsenum_next(elem);
	}

	/* Enum type */
	rc = cgtype_enum_create(cgenum, &etype);
	if (rc != EOK)
		return rc;

	if (tsenum->have_def)
		cgenum->defined = true;

	/*
	 * Enum definition always declares useful identifiers.
	 * We want to allow anonymous enum definitions 'enum { ... };',
	 * but warn for enum forward declarations 'enum x;' which
	 * are forbidden by the C standard
	 */
	if (tsenum->have_def)
		flags |= cgrd_ident;

	if (tsenum->have_def)
		flags |= cgrd_def;

	*rflags = flags;
	*rstype = &etype->cgtype;
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

/** Generate code for declaration specifier / specifier-qualifier.
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
 * @param rsctype Place to store storage class type
 * @param rflags Place to store recordd declaration flags
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_dspec_finish(cgen_dspec_t *cgds, ast_sclass_type_t *rsctype,
    cgen_rd_flags_t *rflags, cgtype_t **rstype)
{
	cgen_t *cgen = cgds->cgen;
	ast_tok_t *atok;
	ast_tsbasic_t *tsbasic;
	comp_tok_t *tok;
	cgtype_elmtype_t elmtype;
	cgtype_basic_t *btype = NULL;
	cgtype_t *stype;
	cgen_rd_flags_t flags = cgrd_none;
	int rc;

	if (cgds->tspec != NULL) {
		/* Process type specifier */
		switch (cgds->tspec->ntype) {
		case ant_tsbasic:
			tsbasic = (ast_tsbasic_t *) cgds->tspec->ext;

			/* Short and long cannot be used with some types */
			switch (tsbasic->btstype) {
			case abts_char:
			case abts_void:
			case abts_va_list:
				if (cgds->short_cnt > 0) {
					cgen_error_short_xxx(cgen, tsbasic);
					return EINVAL;
				}
				if (cgds->long_cnt > 0) {
					cgen_error_long_xxx(cgen, tsbasic);
					return EINVAL;
				}
				break;
			default:
				break;
			}

			/* Signed and unsigned cannot be used with some types */
			switch (tsbasic->btstype) {
			case abts_void:
			case abts_va_list:
				if (cgds->signed_cnt > 0) {
					cgen_error_signed_xxx(cgen, tsbasic);
					return EINVAL;
				}
				if (cgds->unsigned_cnt > 0) {
					cgen_error_unsigned_xxx(cgen, tsbasic);
					return EINVAL;
				}
				break;
			default:
				break;
			}

			switch (tsbasic->btstype) {
			case abts_char:
				if (cgds->unsigned_cnt > 0)
					elmtype = cgelm_uchar;
				else
					elmtype = cgelm_char;

				if (cgds->short_cnt > 0) {
					cgen_error_short_xxx(cgen, tsbasic);
					return EINVAL;
				}
				if (cgds->long_cnt > 0) {
					cgen_error_long_xxx(cgen, tsbasic);
					return EINVAL;
				}
				break;
			case abts_int:
				elmtype = cgelm_int;
				break;
			case abts_void:
				elmtype = cgelm_void;
				break;
			case abts_va_list:
				elmtype = cgelm_va_list;
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
			rc = cgen_tsident(cgen,
			    (ast_tsident_t *)cgds->tspec->ext, &stype);
			if (rc != EOK)
				goto error;
			break;
		case ant_tsrecord:
			rc = cgen_tsrecord(cgen,
			    (ast_tsrecord_t *)cgds->tspec->ext, &flags, &stype);
			if (rc != EOK)
				goto error;
			break;
		case ant_tsenum:
			rc = cgen_tsenum(cgen,
			    (ast_tsenum_t *)cgds->tspec->ext, &flags, &stype);
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
	*rflags = flags;
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
 * @param dspecs Declaration specifiers
 * @param rsctype Place to store storage class
 * @param rflags Place to store record declaration flags
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_dspecs(cgen_t *cgen, ast_dspecs_t *dspecs,
    ast_sclass_type_t *rsctype, cgen_rd_flags_t *rflags, cgtype_t **rstype)
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

	return cgen_dspec_finish(&cgds, rsctype, rflags, rstype);
}

/** Generate code for specifier-qualifier list.
 *
 * @param cgen Code generator
 * @param sqlist Specifier-qualifier list
 * @param rstype Place to store pointer to the specified type
 * @return EOK on success or an error code
 */
static int cgen_sqlist(cgen_t *cgen, ast_sqlist_t *sqlist, cgtype_t **rstype)
{
	ast_node_t *dspec;
	ast_node_t *prev;
	ast_sclass_type_t sctype;
	cgen_dspec_t cgds;
	cgen_rd_flags_t flags;
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

	rc = cgen_dspec_finish(&cgds, &sctype, &flags, rstype);
	if (rc != EOK)
		return rc;

	(void)flags;

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
 * @param btype Type derived from declaration specifiers
 * @param dfun Function declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl_fun(cgen_t *cgen, cgtype_t *btype, ast_dfun_t *dfun,
    ast_aslist_t *aslist, cgtype_t **rdtype)
{
	cgtype_func_t *func = NULL;
	scope_t *arg_scope = NULL;
	scope_t *prev_scope = NULL;
	ast_dfun_arg_t *arg;
	ast_tok_t *atok;
	comp_tok_t *tok;
	cgtype_t *btype_copy = NULL;
	cgtype_t *bdtype = NULL;
	cgtype_t *stype = NULL;
	cgtype_t *atype = NULL;
	cgtype_basic_t *abasic;
	ast_aspec_t *aspec;
	ast_aspec_attr_t *attr;
	ast_sclass_type_t sctype;
	bool have_args = false;
	ast_tok_t *aident;
	comp_tok_t *ident;
	bool arg_with_ident;
	bool arg_without_ident;
	cgen_rd_flags_t flags;
	int rc;

	rc = cgtype_clone(btype, &btype_copy);
	if (rc != EOK)
		goto error;

	rc = cgtype_func_create(btype_copy, &func);
	if (rc != EOK)
		goto error;

	btype_copy = NULL; /* ownership transferred */

	rc = scope_create(cgen->cur_scope, &arg_scope);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	prev_scope = cgen->cur_scope;
	cgen->cur_scope = arg_scope;

	++cgen->arglist_cnt;
	arg_with_ident = false;
	arg_without_ident = false;

	arg = ast_dfun_first(dfun);
	while (arg != NULL) {
		rc = cgen_dspecs(cgen, arg->dspecs, &sctype, &flags, &stype);
		if (rc != EOK) {
			--cgen->arglist_cnt;
			goto error;
		}

		(void)flags;

		if (sctype != asc_none) {
			atok = ast_tree_first_tok(&arg->dspecs->node);
			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Unimplemented storage class specifier.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			--cgen->arglist_cnt;
			goto error;
		}

		rc = cgen_decl(cgen, stype, arg->decl, arg->aslist,
		    &atype);
		if (rc != EOK) {
			--cgen->arglist_cnt;
			goto error;
		}

		aident = ast_decl_get_ident(arg->decl);
		if (aident != NULL) {
			ident = (comp_tok_t *) aident->data;
			arg_with_ident = true;

			/* Insert identifier into argument scope */
			rc = scope_insert_arg(arg_scope, &ident->tok,
			    stype, "dummy");
			if (rc != EOK) {
				if (rc == EEXIST) {
					lexer_dprint_tok(&ident->tok, stderr);
					fprintf(stderr, ": Duplicate argument identifier '%s'.\n",
					    ident->tok.text);
					cgen->error = true; // XXX
					rc = EINVAL;
					goto error;
				}
			}
		} else {
			arg_without_ident = true;
		}

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
					--cgen->arglist_cnt;
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
			--cgen->arglist_cnt;
			goto error;
		}

		rc = cgtype_func_append_arg(func, atype);
		if (rc != EOK) {
			--cgen->arglist_cnt;
			goto error;
		}

		have_args = true;

		atype = NULL; /* ownership transferred */

		cgtype_destroy(stype);
		stype = NULL;

		arg = ast_dfun_next(arg);
	}

	--cgen->arglist_cnt;

	/* Either all arguments should have identifiers, or none */
	if (arg_with_ident && arg_without_ident) {
		tok = (comp_tok_t *) dfun->tlparen.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Mixing arguments with and without an "
		    "identifier.\n");
		++cgen->warnings;
	}

	/* Variadic function? */
	func->variadic = dfun->have_ellipsis;

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

	cgen->cur_scope = prev_scope;
	scope_destroy(arg_scope);

	/* Modify type via base declarator */

	rc = cgen_decl(cgen, &func->cgtype, dfun->bdecl, NULL, &bdtype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(&func->cgtype);
	*rdtype = bdtype;
	return EOK;
error:
	if (prev_scope != NULL) {
		cgen->cur_scope = prev_scope;
		scope_destroy(arg_scope);
	}

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
 * @param btype Type derived from declaration specifiers
 * @param dptr Pointer declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl_ptr(cgen_t *cgen, cgtype_t *btype, ast_dptr_t *dptr,
    ast_aslist_t *aslist, cgtype_t **rdtype)
{
	cgtype_pointer_t *ptrtype;
	cgtype_t *btype_copy = NULL;
	int rc;

	rc = cgtype_clone(btype, &btype_copy);
	if (rc != EOK)
		goto error;

	rc = cgtype_pointer_create(btype_copy, &ptrtype);
	if (rc != EOK)
		goto error;

	rc = cgen_decl(cgen, &ptrtype->cgtype, dptr->bdecl, aslist,
	    rdtype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(&ptrtype->cgtype);
	return EOK;
error:
	cgtype_destroy(btype_copy);
	return rc;
}

/** Generate code for array declarator.
 *
 * Base type (@a stype) determined by the declaration specifiers is
 * further modified by the declarator and returned as @a *rdtype.
 *
 * @param cgen Code generator
 * @param btype Type derived from declaration specifiers
 * @param darray Array declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl_array(cgen_t *cgen, cgtype_t *btype, ast_darray_t *darray,
    ast_aslist_t *aslist, cgtype_t **rdtype)
{
	cgtype_array_t *arrtype;
	cgtype_t *btype_copy = NULL;
	ast_tok_t *tok;
	comp_tok_t *ctok;
	cgtype_t *size_type;
	cgen_eres_t szres;
	bool have_size;
	uint64_t asize;
	int rc;

	cgen_eres_init(&szres);

	if (darray->asize != NULL) {
		/* Array has a size specification */
		rc = cgen_intexpr_val(cgen, darray->asize, &szres);
		if (rc != EOK)
			goto error;

		/* Truth value should not be used */
		if (cgen_type_is_logic(cgen, szres.cgtype)) {
			tok = ast_tree_first_tok(darray->asize);
			ctok = (comp_tok_t *)tok->data;
			cgen_warn_truth_as_int(cgen, ctok);
		}

		have_size = true;
		asize = szres.cvint;
		size_type = szres.cgtype;
		szres.cgtype = NULL;
	} else {
		/* Array size not specified */
		have_size = false;
		asize = 0;
		size_type = NULL;
	}

	if (cgen_type_is_incomplete(cgen, btype)) {
		ctok = (comp_tok_t *)darray->tlbracket.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Array element has incomplete type.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	rc = cgtype_clone(btype, &btype_copy);
	if (rc != EOK)
		goto error;

	rc = cgtype_array_create(btype_copy, size_type, have_size, asize,
	    &arrtype);
	if (rc != EOK)
		goto error;

	rc = cgen_decl(cgen, &arrtype->cgtype, darray->bdecl, aslist,
	    rdtype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(&arrtype->cgtype);
	cgen_eres_fini(&szres);
	return EOK;
error:
	cgen_eres_fini(&szres);
	cgtype_destroy(btype_copy);
	return rc;
}

/** Generate code for parenthesized declarator.
 *
 * Base type (@a stype) determined by the declaration specifiers is
 * further modified by the declarator and returned as @a *rdtype.
 *
 * @param cgen Code generator
 * @param btype Type derived from declaration specifiers
 * @param dparen Parenthesized declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl_paren(cgen_t *cgen, cgtype_t *btype, ast_dparen_t *dparen,
    ast_aslist_t *aslist, cgtype_t **rdtype)
{
	int rc;

	rc = cgen_decl(cgen, btype, dparen->bdecl, aslist, rdtype);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Generate code for declarator.
 *
 * Base type (@a stype) determined by the declaration specifiers is
 * further modified by the declarator and returned as @a *rdtype.
 *
 * @param cgen Code generator
 * @param stype Type derived from declaration specifiers
 * @param decl Declarator
 * @param aslist Attribute specifier list
 * @param rdtype Place to store pointer to the declared type
 * @return EOK on success or an error code
 */
static int cgen_decl(cgen_t *cgen, cgtype_t *stype, ast_node_t *decl,
    ast_aslist_t *aslist, cgtype_t **rdtype)
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
		rc = cgen_decl_fun(cgen, stype, (ast_dfun_t *) decl->ext,
		    aslist, &dtype);
		break;
	case ant_dptr:
		rc = cgen_decl_ptr(cgen, stype, (ast_dptr_t *) decl->ext,
		    aslist, &dtype);
		break;
	case ant_darray:
		rc = cgen_decl_array(cgen, stype, (ast_darray_t *) decl->ext,
		    aslist, &dtype);
		break;
	case ant_dparen:
		rc = cgen_decl_paren(cgen, stype, (ast_dparen_t *) decl->ext,
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

/** Mask constant value to a specified number of bits.
 *
 * We represent constant values of all integer types as int64_t
 * (sign-extended). After performing a computation it may be necessary
 * to mask the result to the number of bits of the actual type
 * to simulate the limited precision. If the type is signed,
 * we also need to sign-extend the result.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff the constant is of signed type
 * @param bits Width of the type of the constant in bits
 * @param a Value
 * @param res Place to store masked and sign-exteneded value
 */
static void cgen_cvint_mask(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a, int64_t *res)
{
	uint64_t r;
	uint64_t mask;

	(void)cgen;

	if (bits < 64) {
		mask = ((uint64_t)1 << bits) - 1;
	} else {
		mask = ~(uint64_t)0;
	}

	r = (uint64_t)a & mask;
	if (bits < 64 && is_signed) {
		/* Need to sign-exend after masking */
		if ((r & ((uint64_t)1 << (bits - 1))) != 0) {
			r |= ~mask;
		}
	}

	*res = (int64_t)r;
}

/** Add two integer constants with overflow checking.
 *
 * Overflow is only detected for signed addition. Unsigned addition
 * is modulo.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff addition is signed
 * @param bits Number of bits
 * @param a1 First argument
 * @param a2 Second argument
 * @param res Place to store result
 * @param overflow Place to store @c true on signed overflow
 */
static void cgen_cvint_add(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t a2, int64_t *res, bool *overflow)
{
	uint64_t r;
	int64_t rm;
	bool neg1, neg2;
	bool rneg;

	*overflow = false;

	r = (uint64_t)a1 + (uint64_t)a2;
	cgen_cvint_mask(cgen, is_signed, bits, r, &rm);

	if (is_signed) {
		neg1 = a1 < 0;
		neg2 = a2 < 0;
		rneg = rm < 0;

		/* Addition can only overflow if both operands have same sign */
		if (neg1 == neg2 && rneg != neg1)
			*overflow = true;
	}

	*res = rm;
}

/** Subtract two integer constants with overflow checking.
 *
 * Overflow is only detected for signed subtraction. Unsigned subtraction
 * is modulo.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff addition is signed
 * @param bits Number of bits
 * @param a1 First argument
 * @param a2 Second argument
 * @param res Place to store result
 * @param overflow Place to store @c true on signed overflow
 */
static void cgen_cvint_sub(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t a2, int64_t *res, bool *overflow)
{
	uint64_t r;
	int64_t rm;
	bool neg1, neg2;
	bool rneg;

	*overflow = false;

	r = (uint64_t)a1 - (uint64_t)a2;
	cgen_cvint_mask(cgen, is_signed, bits, r, &rm);

	if (is_signed) {
		neg1 = a1 < 0;
		neg2 = a2 < 0;
		rneg = rm < 0;

		/* Subtraction can only overflow if operand signs differ */
		if (neg1 != neg2 && rneg != neg1)
			*overflow = true;
	}

	*res = rm;
}

/** Negate an integer constants with overflow checking.
 *
 * Overflow is only detected for signed negation.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff addition is signed
 * @param bits Number of bits
 * @param a1 First argument
 * @param res Place to store result
 * @param overflow Place to store @c true on signed overflow
 */
static void cgen_cvint_neg(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t *res, bool *overflow)
{
	uint64_t r;
	int64_t rm;
	bool neg1;
	bool rneg;

	*overflow = false;

	r = -(uint64_t)a1;
	cgen_cvint_mask(cgen, is_signed, bits, r, &rm);

	if (is_signed) {
		neg1 = a1 < 0;
		rneg = rm < 0;

		/* Most negative value in two's complement has no negation */
		if (rneg == neg1)
			*overflow = true;
	}

	*res = rm;
}

/** Multiply two integer constants with overflow checking.
 *
 * Overflow is only detected for signed multiplication. Unsigned multiplication
 * is modulo.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff multiplication is signed
 * @param bits Number of bits
 * @param a1 First argument
 * @param a2 Second argument
 * @param res Place to store result
 * @param overflow Place to store @c true on signed overflow
 */
static void cgen_cvint_mul(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t a2, int64_t *res, bool *overflow)
{
	uint64_t r;
	int64_t v;
	int64_t rm;

	*overflow = false;

	r = (uint64_t)a1 * (uint64_t)a2;
	cgen_cvint_mask(cgen, is_signed, bits, r, &rm);

	if (is_signed && a2 != 0) {
		/* Verification */
		v = (int64_t)rm / (int64_t) a2;

		/* If verification failed, we have an overflow */
		if (v != a1)
			*overflow = true;
	}

	*res = rm;
}

/** Divide two integer constants with division by zero checking.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff division is signed
 * @param bits Number of bits
 * @param a1 First argument
 * @param a2 Second argument
 * @param res Place to store result
 * @param divbyzero Place to store @c true on division by zero
 */
static void cgen_cvint_div(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t a2, int64_t *res, bool *divbyzero)
{
	uint64_t r;
	int64_t rm;

	if (a2 != 0) {
		*divbyzero = false;

		r = (uint64_t)a1 / (uint64_t)a2;
		cgen_cvint_mask(cgen, is_signed, bits, r, &rm);
	} else {
		*divbyzero = true;
		rm = 0;
	}

	*res = rm;
}

/** Compute modulus of two integer constants with division by zero checking.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff division is signed
 * @param bits Number of bits
 * @param a1 First argument
 * @param a2 Second argument
 * @param res Place to store result
 * @param divbyzero Place to store @c true on division by zero
 */
static void cgen_cvint_mod(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t a2, int64_t *res, bool *divbyzero)
{
	uint64_t r;
	int64_t rm;

	if (a2 != 0) {
		*divbyzero = false;

		r = (uint64_t)a1 % (uint64_t)a2;
		cgen_cvint_mask(cgen, is_signed, bits, r, &rm);
	} else {
		*divbyzero = true;
		rm = 0;
	}

	*res = rm;
}

/** Shift constant left.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff addition is signed
 * @param bits Number of bits
 * @param a1 First argument (value)
 * @param a2 Second argument (shift)
 * @param res Place to store result
 */
static void cgen_cvint_shl(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t a2, int64_t *res)
{
	uint64_t r;

	r = (uint64_t)a1 << a2;
	cgen_cvint_mask(cgen, is_signed, bits, r, res);
}

/** Shift constant right.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff addition is signed
 * @param bits Number of bits
 * @param a1 First argument (value)
 * @param a2 Second argument (shift)
 * @param res Place to store result
 */
static void cgen_cvint_shr(cgen_t *cgen, bool is_signed, unsigned bits,
    int64_t a1, int64_t a2, int64_t *res)
{
	uint64_t r;

	if (is_signed) {
		r = a1 >> a2;
	} else {
		r = (uint64_t)a1 >> a2;
	}

	cgen_cvint_mask(cgen, is_signed, bits, r, res);
}

/** Determine if constant value is negative.
 *
 * @param cgen Code generator
 * @param is_signed @c true iff constant is of signed integer type
 * @param a Constant value
 * @return @c true iff constant value is negative
 */
static bool cgen_cvint_is_negative(cgen_t *cgen, bool is_signed, int64_t a)
{
	(void) cgen;

	if (is_signed)
		return a < 0;
	else
		return false;
}

/** Determine if constant value is in the range of a basic type.
 *
 * @param cgen Code generator
 * @param asigned Is constant signed
 * @param a Constant value
 * @param tbasic Basic type
 * @return @c true iff constant is in range of the basic type
 */
static bool cgen_cvint_in_tbasic_range(cgen_t *cgen, bool asigned,
    int64_t a, cgtype_basic_t *tbasic)
{
	int bits;
	bool is_signed;
	int64_t lo;
	int64_t hi;

	bits = cgen_basic_type_bits(cgen, tbasic);
	is_signed = cgen_basic_type_signed(cgen, tbasic);

	/* Have to treat 64 bits as special due to limitations of int64_t */
	if (bits == 64) {
		/* Switch expression type is signed? */
		if (is_signed) {
			/* Only out of range if >= 2^63 */
			if (asigned) {
				return true;
			} else {
				/* unsigned & int64_t < 0 means bit 63 set */
				return a >= 0;
			}
		} else {
			/* Only out of range if a is negative */
			if (asigned)
				return a >= 0;
			else
				return true;
		}
	}

	/* Compute upper and lower bound */
	if (is_signed) {
		lo = -((int64_t)1 << (bits - 1));
		hi = ((int64_t)1 << (bits - 1)) - 1;
	} else {
		lo = 0;
		hi = ((int64_t)1 << bits) - 1;
	}

	return lo <= a && a <= hi;
}

/** Determine if constant value is one of the values of an enum.
 *
 * @param cgen Code generator
 * @param asigned Is constant signed
 * @param a Constant value
 * @param cgenum Enum
 * @return @c true iff constant is in enum
 */
static bool cgen_cvint_in_enum(cgen_t *cgen, bool asigned,
    int64_t a, cgen_enum_t *cgenum)
{
	cgen_enum_elem_t *elem;

	(void)asigned;

	if (a < cgen_int_min(cgen) ||
	    a > cgen_int_max(cgen))
		return false;

	elem = cgen_enum_val_find(cgenum, (int)a);
	return elem != NULL;
}

/** Determine if constant value of expression result is true.
 *
 * @param cgen Code generator
 * @param eres Expression result
 * @return @c true iff constant value is logically true
 */
static bool cgen_eres_is_true(cgen_t *cgen, cgen_eres_t *eres)
{
	assert(eres->cvknown);

	(void) cgen;
	// XXX Floating point
	return eres->cvint != 0;
}

/** Determine if constant value of expression result is integer zero.
 *
 * @param cgen Code generator
 * @param eres Expression result
 * @return @c true iff constant value is logically true
 */
static bool cgen_eres_is_int_zero(cgen_t *cgen, cgen_eres_t *eres)
{
	if (!eres->cvknown)
		return false;
	if (!cgen_type_is_integral(cgen, eres->cgtype))
		return false;

	return eres->cvint == 0;
}

/** Generate code for integer literal expression.
 *
 * @param cgexpr Code generator for expression
 * @param eint AST integer literal expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eint(cgen_expr_t *cgexpr, ast_eint_t *eint,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *lit;
	int64_t val;
	cgtype_elmtype_t elmtype;
	int rc;

	lit = (comp_tok_t *) eint->tlit.data;
	rc = cgen_intlit_val(cgexpr->cgen, lit, &val, &elmtype);
	if (rc != EOK) {
		lexer_dprint_tok(&lit->tok, stderr);
		fprintf(stderr, ": Invalid integer literal.\n");
		cgexpr->cgen->error = true; // TODO
		return rc;
	}

	return cgen_const_int(cgexpr->cgproc, elmtype, val, lblock, eres);
}

/** Generate code for character literal expression.
 *
 * @param cgexpr Code generator for expression
 * @param echar AST character literal expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_echar(cgen_expr_t *cgexpr, ast_echar_t *echar,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *lit;
	int64_t val;
	cgtype_elmtype_t elmtype;
	int rc;

	lit = (comp_tok_t *) echar->tlit.data;
	rc = cgen_charlit_val(cgexpr->cgen, lit, &val, &elmtype);
	if (rc != EOK)
		return rc;

	return cgen_const_int(cgexpr->cgproc, elmtype, val, lblock, eres);
}

/** Generate code for string literal expression.
 *
 * @param cgexpr Code generator for expression
 * @param estring AST string literal expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_estring(cgen_expr_t *cgexpr, ast_estring_t *estring,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	(void)estring;
	comp_tok_t *itok;
	symbol_t *symbol = NULL;
	ir_var_t *var = NULL;
	ir_dblock_t *dblock = NULL;
	char *pident = NULL;
	cgtype_basic_t *btype = NULL;
	cgtype_basic_t *itype = NULL;
	cgtype_array_t *atype = NULL;
	ast_estring_lit_t *lit;
	bool wide;
	int rc;
	int rv;

	/* Determine if string is wide */
	lit = ast_estring_first(estring);
	assert(lit != NULL);
	wide = cgen_estring_lit_is_wide(lit);

	++cgexpr->cgen->str_cnt;
	rv = asprintf(&pident, "@_Str_%u", cgexpr->cgen->str_cnt);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	/* Create array element type */
	rc = cgtype_basic_create(wide ? cgelm_int : cgelm_char, &btype);
	if (rc != EOK)
		goto error;

	/* Create array index type */
	rc = cgtype_basic_create(cgelm_int, &itype);
	if (rc != EOK)
		goto error;

	/* Create array type with unknown size */
	rc = cgtype_array_create(&btype->cgtype, &itype->cgtype, false, 0,
	    &atype);
	if (rc != EOK)
		goto error;

	btype = NULL;
	itype = NULL;

	/* Create 'anonymous' symbol (no C identifer, only IR identifier) */
	rc = symbols_insert(cgexpr->cgen->symbols, st_var, NULL, pident,
	    &symbol);
	if (rc != EOK)
		goto error;

	symbol->flags |= sf_defined;
	symbol->flags |= sf_static;

	/* Generate a new IR array */
	rc = ir_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	rc = ir_var_create(pident, NULL, irl_default, dblock, &var);
	if (rc != EOK)
		goto error;

	dblock = NULL;

	/* Initialize it with the string literal */

	itok = (comp_tok_t *)ast_tree_first_tok(&estring->node)->data;

	rc = cgen_init_dentries_string(cgexpr->cgen, &atype->cgtype, itok,
	    estring, var->dblock);
	if (rc != EOK)
		goto error;

	/* Now that array size is filled in, clone type to symbol */
	rc = cgtype_clone(&atype->cgtype, &symbol->cgtype);
	if (rc != EOK)
		goto error;

	rc = cgen_cgtype(cgexpr->cgen, &atype->cgtype, &var->vtype);
	if (rc != EOK)
		goto error;

	ir_module_append(cgexpr->cgen->irmod, &var->decln);
	var = NULL;

	/* Generate pointer to the array */
	rc = cgen_gsym_ptr(cgexpr->cgproc, symbol, lblock, eres);
	if (rc != EOK)
		goto error;

	free(pident);
	eres->cgtype = &atype->cgtype;
	return EOK;
error:
	if (pident != NULL)
		free(pident);
	if (itype != NULL)
		cgtype_destroy(&itype->cgtype);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	if (dblock != NULL)
		ir_dblock_destroy(dblock);
	return rc;
}

/** Generate code for identifier expression referencing global symbol.
 *
 * @param cgexpr Code generator for expression
 * @param eident AST identifier expression
 * @param symbol Global symbol
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eident_gsym(cgen_expr_t *cgexpr, ast_eident_t *eident,
    symbol_t *symbol, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	if (cgexpr->icexpr) {
		cgen_error_expr_not_constant(cgexpr->cgen, &eident->tident);
		return EINVAL;
	}

	return cgen_gsym_ptr(cgexpr->cgproc, symbol, lblock, eres);
}

/** Generate code for identifier expression referencing function argument.
 *
 * @param cgexpr Code generator for expression
 * @param eident AST identifier expression
 * @param vident Identifier of IR variable holding the argument
 * @param idx Argument index
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 *
 * @return EOK on success or an error code
 */
static int cgen_eident_arg(cgen_expr_t *cgexpr, ast_eident_t *eident,
    const char *vident, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	(void) lblock;

	if (cgexpr->cexpr) {
		cgen_error_expr_not_constant(cgexpr->cgen, &eident->tident);
		return EINVAL;
	}

	eres->varname = vident;
	eres->valtype = cgen_rvalue;
	eres->cgtype = NULL;
	return EOK;
}

/** Generate code for identifier expression referencing local variable.
 *
 * @param cgexpr Code generator for expression
 * @param eident AST identifier expression
 * @param vident Identifier of IR variable holding the local variable
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 *
 * @return EOK on success or an error code
 */
static int cgen_eident_lvar(cgen_expr_t *cgexpr, ast_eident_t *eident,
    const char *vident, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	if (cgexpr->cexpr) {
		cgen_error_expr_not_constant(cgexpr->cgen, &eident->tident);
		return EINVAL;
	}

	return cgen_lvaraddr(cgexpr->cgproc, vident, lblock, eres);
}

/** Generate code for identifier expression referencing enum element.
 *
 * @param cgexpr Code generator for expression
 * @param eident AST identifier expression
 * @param eelem Enum element
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 *
 * @return EOK on success or an error code
 */
static int cgen_eident_eelem(cgen_expr_t *cgexpr, ast_eident_t *eident,
    cgen_enum_elem_t *eelem, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_imm_t *imm = NULL;
	int rc;

	(void)eident;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(eelem->value, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgen_enum_bits;
	instr->dest = &dest->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = NULL;
	eres->cvknown = true;
	eres->cvint = eelem->value;
	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
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
static int cgen_eident(cgen_expr_t *cgexpr, ast_eident_t *eident,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *ident;
	scope_member_t *member;
	cgtype_t *cgtype = NULL;
	int rc = EINVAL;

	ident = (comp_tok_t *) eident->tident.data;

	/* Check if the identifier is declared */
	member = scope_lookup(cgexpr->cgen->cur_scope, ident->tok.text);
	if (member == NULL) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Undeclared identifier '%s'.\n",
		    ident->tok.text);
		cgexpr->cgen->error = true; // TODO
		return EINVAL;
	}

	/* Resulting type is the same as type of the member */
	rc = cgtype_clone(member->cgtype, &cgtype);
	if (rc != EOK)
		return rc;

	switch (member->mtype) {
	case sm_gsym:
		rc = cgen_eident_gsym(cgexpr, eident, member->m.gsym.symbol,
		    lblock, eres);
		break;
	case sm_arg:
		rc = cgen_eident_arg(cgexpr, eident, member->m.arg.vident,
		    lblock, eres);
		break;
	case sm_lvar:
		rc = cgen_eident_lvar(cgexpr, eident, member->m.lvar.vident,
		    lblock, eres);
		break;
	case sm_record:
	case sm_enum:
		/*
		 * Should not happen - call to scope lookup above cannot
		 * match a record or enum tag.
		 */
		assert(false);
		return EINVAL;
	case sm_eelem:
		rc = cgen_eident_eelem(cgexpr, eident, member->m.eelem.eelem,
		    lblock, eres);
		break;
	case sm_tdef:
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Expected variable name. '%s' is a type.\n",
		    ident->tok.text);
		cgexpr->cgen->error = true; // TODO
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
 * @param cgexpr Code generator for expression
 * @param eparen AST parenthesized expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eparen(cgen_expr_t *cgexpr, ast_eparen_t *eparen,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	int rc;

	rc = cgen_expr(cgexpr, eparen->bexpr, lblock, eres);
	if (rc != EOK)
		return rc;

	eres->tfirst = &eparen->tlparen;
	eres->tlast = &eparen->trparen;

	return EOK;
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
	eres->cvknown = true;
	eres->cvint = val;
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

/** Generate code to return pointer to global symbol.
 *
 * @param cgproc Code generator for procedure
 * @param symbol Symbol
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gsym_ptr(cgen_proc_t *cgproc, symbol_t *symbol,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	int rc;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(symbol->irident, &var);
	if (rc != EOK)
		goto error;

	instr->itype = iri_varptr;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &var->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_lvalue;
	eres->cgtype = NULL;
	eres->cvknown = true;
	eres->cvsymbol = symbol;
	eres->cvint = 0;

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

/** Generate code for addition of integers.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_add_int(cgen_expr_t *cgexpr, ast_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgtype_t *cgtype = NULL;
	cgen_uac_flags_t flags;
	cgtype_basic_t *tbasic;
	bool is_signed;
	unsigned bits;
	bool overflow;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	/* Perform usual arithmetic conversion */
	rc = cgen_uac(cgexpr, lres, rres, lblock, &res1, &res2, &flags);
	if (rc != EOK)
		goto error;

	/* Unsigned addition of mixed-signed numbers is OK */
	(void)flags;

	/* Warn if truth value involved in addition */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, optok);

	/* Check the type */
	if (res1.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic = (cgtype_basic_t *)res1.cgtype->ext;
	bits = cgen_basic_type_bits(cgexpr->cgen, tbasic);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen, tbasic);

	rc = cgtype_clone(res1.cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (res1.cvknown && res2.cvknown) {
		eres->cvknown = true;
		cgen_cvint_add(cgexpr->cgen, is_signed, bits, res1.cvint,
		    res2.cvint, &eres->cvint, &overflow);
		if (overflow)
			cgen_warn_integer_overflow(cgexpr->cgen, optok);
	}

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

/** Generate code for addition of enum and integer.
 *
 * @param cgexpr Code generator for procedure
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_add_enum_int(cgen_expr_t *cgexpr, ast_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	cgen_eres_t ares;
	int rc;

	cgen_eres_init(&ares);

	rc = cgen_add_int(cgexpr, optok, lres, rres, lblock, &ares);
	if (rc != EOK)
		goto error;

	/* Convert result back to original enum type, if possible */
	rc = cgen_int2enum(cgexpr, &ares, lres->cgtype, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&ares);
	return EOK;
error:
	cgen_eres_fini(&ares);
	return rc;
}

/** Generate code for addition of pointer/array and integer.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_add_ptra_int(cgen_expr_t *cgexpr, comp_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lval;
	cgen_eres_t cres;
	cgtype_t *idxtype = NULL;
	cgtype_t *cgtype = NULL;
	ir_texpr_t *elemte = NULL;
	cgtype_pointer_t *ptrt;
	cgtype_array_t *arrt;
	cgtype_t *etype;
	bool idx_signed;
	int rc;

	cgen_eres_init(&lval);
	cgen_eres_init(&cres);

	idx_signed = cgen_type_is_signed(cgexpr->cgen, rres->cgtype);

	/*
	 * If the left operand is a pointer, we need to convert it
	 * to rvalue. We leave an array as an lvalue. In either case
	 * we end up with the base address stored in the result variable.
	 */
	if (lres->cgtype->ntype == cgn_pointer) {
		rc = cgtype_int_construct(idx_signed, cgir_int, &idxtype);
		if (rc != EOK)
			goto error;

		/* Convert index to be same size as pointer */
		rc = cgen_type_convert(cgexpr, optok, rres, idxtype,
		    cgen_implicit, lblock, &cres);
		if (rc != EOK)
			goto error;

		cgtype_destroy(idxtype);
		idxtype = NULL;

		/* Value of left hand expression */
		rc = cgen_eres_rvalue(cgexpr, lres, lblock, &lval);
		if (rc != EOK)
			goto error;

		ptrt = (cgtype_pointer_t *)lval.cgtype->ext;

		/* Check type for completeness */
		if (cgen_type_is_incomplete(cgexpr->cgen, ptrt->tgtype)) {
			lexer_dprint_tok(&optok->tok, stderr);
			fprintf(stderr, ": Indexing pointer to incomplete type.\n");
			cgexpr->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		/* Generate IR type expression for the element type */
		rc = cgen_cgtype(cgexpr->cgen, ptrt->tgtype, &elemte);
		if (rc != EOK)
			goto error;

		/* Result type is the same as the base pointer type */
		rc = cgtype_clone(lval.cgtype, &cgtype);
		if (rc != EOK)
			goto error;

	} else {
		/* Indexing an array. */
		assert(lres->cgtype->ntype == cgn_array);
		arrt = (cgtype_array_t *)lres->cgtype->ext;

		/*
		 * If the type of array dimension (int, enum) is known,
		 * we can convert the subscript to it, thus checking
		 * if its type matches the dimension type. Otherwise,
		 * we will assume that the array dimension is an integer.
		 */
		if (arrt->itype == NULL) {
			rc = cgtype_int_construct(idx_signed, cgir_int,
			    &idxtype);
			if (rc != EOK)
				goto error;
		}

		/* Convert index to be same size as pointer */
		rc = cgen_type_convert(cgexpr, optok, rres,
		    arrt->itype != NULL ? arrt->itype : idxtype,
		    cgen_implicit, lblock, &cres);
		if (rc != EOK)
			goto error;

		cgtype_destroy(idxtype);
		idxtype = NULL;

		if (cres.cvknown) {
			if (cgen_cvint_is_negative(cgexpr->cgen,
			    idx_signed, cres.cvint)) {
				cgen_warn_array_index_negative(cgexpr->cgen,
				    optok);
			} else if ((uint64_t)cres.cvint >=
			    (uint64_t)arrt->asize) {
				cgen_warn_array_index_oob(cgexpr->cgen, optok);
			}
		} else {
			/* TODO Optional run-time check */
		}

		rc = cgen_eres_clone(lres, &lval);
		if (rc != EOK)
			goto error;

		/* Generate IR type expression for the element type */
		rc = cgen_cgtype(cgexpr->cgen, arrt->etype, &elemte);
		if (rc != EOK)
			goto error;

		/* Create a private copy of element type */
		rc = cgtype_clone(arrt->etype, &etype);
		if (rc != EOK)
			goto error;

		/*
		 * Result type is a pointer to the array element type.
		 * Note that ownership of etype is transferred to ptrt.
		 */
		rc = cgtype_pointer_create(etype, &ptrt);
		if (rc != EOK) {
			cgtype_destroy(etype);
			goto error;
		}

		cgtype = &ptrt->cgtype;
	}

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lval.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cres.varname, &rarg);
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

	if (lres->cvknown && rres->cvknown) {
		/* Compute constant pointer */
		eres->cvknown = true;
		eres->cvsymbol = lres->cvsymbol;
		eres->cvint = lres->cvint + rres->cvint *
		    cgen_type_sizeof(cgexpr->cgen, cgtype);
	}

	cgen_eres_fini(&lval);
	cgen_eres_fini(&cres);

	return EOK;
error:
	cgen_eres_fini(&lval);
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
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_add(cgen_expr_t *cgexpr, ast_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *ctok;
	bool l_int;
	bool r_int;
	bool l_enum;
	bool r_enum;
	bool l_ptra;
	bool r_ptra;

	ctok = (comp_tok_t *)optok->data;

	l_int = cgen_type_is_integer(cgexpr->cgen, lres->cgtype);
	r_int = cgen_type_is_integer(cgexpr->cgen, rres->cgtype);
	l_enum = lres->cgtype->ntype == cgn_enum;
	r_enum = rres->cgtype->ntype == cgn_enum;

	/* Integer + integer */
	if (l_int && r_int)
		return cgen_add_int(cgexpr, optok, lres, rres, lblock, eres);

	/* Enum + integer */
	if (l_enum && r_int) {
		return cgen_add_enum_int(cgexpr, optok, lres, rres, lblock,
		    eres);
	}

	l_ptra = lres->cgtype->ntype == cgn_pointer ||
	    lres->cgtype->ntype == cgn_array;
	r_ptra = rres->cgtype->ntype == cgn_pointer ||
	    rres->cgtype->ntype == cgn_array;

	/* Pointer/array + pointer/array */
	if (l_ptra && r_ptra) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Cannot add ");
		(void) cgtype_print(lres->cgtype, stderr);
		fprintf(stderr, " and ");
		(void) cgtype_print(rres->cgtype, stderr);
		fprintf(stderr, ".\n");

		cgexpr->cgen->error = true; // TODO
		return EINVAL;
	}

	/* Pointer/array + integer/enum */
	if (l_ptra && (r_int || r_enum))
		return cgen_add_ptra_int(cgexpr, ctok, lres, rres, lblock, eres);

	/* Integer/enum + pointer/array */
	if ((l_int || l_enum) && r_ptra) {
		/* Produce a style warning and switch the operands */
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Pointer should be the left "
		    "operand while indexing.\n");
		++cgexpr->cgen->warnings;
		return cgen_add_ptra_int(cgexpr, ctok, rres, lres, lblock, eres);
	}

	/* Integer + enum */
	if (l_int && r_enum) {
		/* Produce a style warning if enum is strict */
		if (cgtype_is_strict_enum(rres->cgtype)) {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Enum should be the left "
			    "operand while adjusting.\n");
			++cgexpr->cgen->warnings;
		}
		/* Switch the operands */
		return cgen_add_enum_int(cgexpr, optok, rres, lres, lblock,
		    eres);
	}

	/* Enum + enum */
	if (l_enum && r_enum) {
		/* Produce warning if both enums are strict */
		if (cgtype_is_strict_enum(lres->cgtype) &&
		    cgtype_is_strict_enum(rres->cgtype))
			cgen_warn_arith_enum(cgexpr->cgen, optok);
		return cgen_add_int(cgexpr, optok, lres, rres, lblock, eres);
	}

	/* Integer + pointer/array */
	if (l_int && r_ptra) {
		/* Produce a style warning and switch the operands */
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Pointer should be the left "
		    "operand while indexing.\n");
		++cgexpr->cgen->warnings;
		return cgen_add_ptra_int(cgexpr, ctok, rres, lres, lblock, eres);
	}

	fprintf(stderr, "Unimplemented addition of ");
	(void) cgtype_print(lres->cgtype, stderr);
	fprintf(stderr, " and ");
	(void) cgtype_print(rres->cgtype, stderr);
	fprintf(stderr, ".\n");
	cgexpr->cgen->error = true; // TODO
	return EINVAL;
}

/** Generate code for subtraction of integers.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of subtraction
 * @return EOK on success or an error code
 */
static int cgen_sub_int(cgen_expr_t *cgexpr, ast_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgtype_t *cgtype = NULL;
	cgen_uac_flags_t flags;
	cgtype_basic_t *tbasic;
	bool is_signed;
	unsigned bits;
	bool overflow;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	/* Perform usual arithmetic conversion */
	rc = cgen_uac(cgexpr, lres, rres, lblock, &res1, &res2, &flags);
	if (rc != EOK)
		goto error;

	/* Unsigned subtraction of mixed-signed numbers is OK */
	(void)flags;

	/* Warn if truth value involved in subtraction */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, optok);

	/* Check the type */
	if (res1.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic = (cgtype_basic_t *)res1.cgtype->ext;
	bits = cgen_basic_type_bits(cgexpr->cgen, tbasic);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen, tbasic);

	rc = cgtype_clone(res1.cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (res1.cvknown && res2.cvknown) {
		eres->cvknown = true;
		cgen_cvint_sub(cgexpr->cgen, is_signed, bits, res1.cvint,
		    res2.cvint, &eres->cvint, &overflow);
		if (overflow)
			cgen_warn_integer_overflow(cgexpr->cgen, optok);
	}

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

/** Generate code for subtraction of enum and integer.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_sub_enum_int(cgen_expr_t *cgexpr, ast_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	cgen_eres_t ares;
	int rc;

	cgen_eres_init(&ares);

	rc = cgen_sub_int(cgexpr, optok, lres, rres, lblock, &ares);
	if (rc != EOK)
		goto error;

	/* Convert result back to original enum type, if possible */
	rc = cgen_int2enum(cgexpr, &ares, lres->cgtype, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&ares);
	return EOK;
error:
	cgen_eres_fini(&ares);
	return rc;
}

/** Generate code for subtraction of enums.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operator token
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_sub_enum(cgen_expr_t *cgexpr, ast_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	cgtype_enum_t *lenum;
	cgtype_enum_t *renum;
	int rc;

	assert(lres->cgtype->ntype == cgn_enum);
	lenum = (cgtype_enum_t *)lres->cgtype->ext;
	assert(rres->cgtype->ntype == cgn_enum);
	renum = (cgtype_enum_t *)rres->cgtype->ext;

	rc = cgen_sub_int(cgexpr, optok, lres, rres, lblock, eres);
	if (rc != EOK)
		return rc;

	/* Different enum types? */
	if (lenum->cgenum != renum->cgenum) {
		if (cgtype_is_strict_enum(lres->cgtype) &&
		    (cgtype_is_strict_enum(rres->cgtype))) {
			/* Subtracting incompatible strict enum type */
			cgen_warn_sub_enum_inc(cgexpr->cgen, optok, lres, rres);
		} else if (cgtype_is_strict_enum(rres->cgtype)) {
			/* Subtracting strict enum from non-strict */
			cgen_warn_arith_enum(cgexpr->cgen, optok);
		}
	}

	return EOK;
}

/** Generate code for subtraction of pointer and integer.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of addition
 * @return EOK on success or an error code
 */
static int cgen_sub_ptr_int(cgen_expr_t *cgexpr, comp_tok_t *optok,
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
	cgen_eres_t lval;
	cgen_eres_t cres;
	cgtype_t *idxtype = NULL;
	cgtype_t *cgtype = NULL;
	ir_texpr_t *elemte = NULL;
	cgtype_pointer_t *ptrt;
	int rc;

	cgen_eres_init(&lval);
	cgen_eres_init(&cres);

	rc = cgtype_int_construct(false, cgir_int, &idxtype);
	if (rc != EOK)
		goto error;

	/* Convert index to be same size as pointer */
	rc = cgen_type_convert(cgexpr, optok, rres, idxtype,
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

	/* Value of left hand expression */
	rc = cgen_eres_rvalue(cgexpr, lres, lblock, &lval);
	if (rc != EOK)
		goto error;

	/* Check type for completeness */
	if (cgen_type_is_incomplete(cgexpr->cgen, ptrt->tgtype)) {
		lexer_dprint_tok(&optok->tok, stderr);
		fprintf(stderr, ": Indexing pointer to incomplete type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Generate IR type expression for the element type */
	rc = cgen_cgtype(cgexpr->cgen, ptrt->tgtype, &elemte);
	if (rc != EOK)
		goto error;

	/* neg %<tmp>, %<cres> */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &tmp);
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

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lval.varname, &larg);
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

	cgen_eres_fini(&lval);
	cgen_eres_fini(&cres);

	return EOK;
error:
	cgen_eres_fini(&lval);
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

/** Convert pointer or array to pointer rvalue.
 *
 * @param cgexpr Code generator for expression
 * @param bres Base expression result
 * @param eres Place to store result
 * @return EOK on success or an error code
 */
static int cgen_eres_rvptr(cgen_expr_t *cgexpr, cgen_eres_t *bres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_texpr_t *elemte = NULL;
	cgen_eres_t bval;
	cgen_eres_t cres;
	cgtype_t *ptdtype = NULL;
	cgtype_t *cgtype = NULL;
	cgtype_pointer_t *ptrt;
	cgtype_array_t *arrt;
	cgtype_t *etype;
	int rc;

	cgen_eres_init(&bval);
	cgen_eres_init(&cres);

	/*
	 * If the operand is a pointer, we need to convert it
	 * to rvalue. We leave an array as an lvalue. In either case
	 * we end up with the base address stored in the result variable.
	 */
	if (bres->cgtype->ntype == cgn_pointer) {
		/* Value of base expression */
		rc = cgen_eres_rvalue(cgexpr, bres, lblock, &bval);
		if (rc != EOK)
			goto error;

		ptrt = (cgtype_pointer_t *)bval.cgtype->ext;

		/* Result type is the same as the base pointer type */
		rc = cgtype_clone(bval.cgtype, &cgtype);
		if (rc != EOK)
			goto error;

	} else {
		assert(bres->cgtype->ntype == cgn_array);
		arrt = (cgtype_array_t *)bres->cgtype->ext;

		rc = cgen_eres_clone(bres, &bval);
		if (rc != EOK)
			goto error;

		/* Generate IR type expression for the element type */
		rc = cgen_cgtype(cgexpr->cgen, arrt->etype, &elemte);
		if (rc != EOK)
			goto error;

		/* Create a private copy of element type */
		rc = cgtype_clone(arrt->etype, &etype);
		if (rc != EOK)
			goto error;

		/*
		 * Result type is a pointer to the array element type.
		 * Note that ownership of etype is transferred to ptrt.
		 */
		rc = cgtype_pointer_create(etype, &ptrt);
		if (rc != EOK) {
			cgtype_destroy(etype);
			goto error;
		}

		cgtype = &ptrt->cgtype;
		ir_texpr_destroy(elemte);
		elemte = NULL;
	}

	eres->varname = bval.varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;
	eres->cvknown = bres->cvknown;
	eres->cvsymbol = bres->cvsymbol;
	eres->cvint = bres->cvint;

	cgen_eres_fini(&bval);

	return EOK;
error:
	cgen_eres_fini(&bval);
	cgtype_destroy(cgtype);
	cgtype_destroy(ptdtype);
	ir_texpr_destroy(elemte);
	return rc;
}

/** Generate code for subtraction of pointers or arrays.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of subtraction
 * @return EOK on success or an error code
 */
static int cgen_sub_ptra(cgen_expr_t *cgexpr, comp_tok_t *optok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lval;
	cgen_eres_t rval;
	cgtype_pointer_t *tptr1;
	cgtype_pointer_t *tptr2;
	cgtype_t *ptdtype = NULL;
	ir_texpr_t *elemte = NULL;
	int rc;

	cgen_eres_init(&lval);
	cgen_eres_init(&rval);

	rc = cgtype_int_construct(true, cgir_int, &ptdtype);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvptr(cgexpr, lres, lblock, &lval);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvptr(cgexpr, rres, lblock, &rval);
	if (rc != EOK)
		goto error;

	/* Error for incompatible pointer types */
	assert(lval.cgtype->ntype == cgn_pointer);
	tptr1 = (cgtype_pointer_t *)lval.cgtype->ext;
	assert(rval.cgtype->ntype == cgn_pointer);
	tptr2 = (cgtype_pointer_t *)rval.cgtype->ext;

	if (!cgtype_ptr_compatible(tptr1, tptr2)) {
		lexer_dprint_tok(&optok->tok, stderr);
		fprintf(stderr, ": Subtracting pointers of incompatible "
		    "type (");
		cgtype_print(lval.cgtype, stderr);
		fprintf(stderr, " and ");
		cgtype_print(rval.cgtype, stderr);
		fprintf(stderr, ").\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Check type for completeness */
	if (cgen_type_is_incomplete(cgexpr->cgen, tptr1->tgtype) ||
	    cgen_type_is_incomplete(cgexpr->cgen, tptr2->tgtype)) {
		lexer_dprint_tok(&optok->tok, stderr);
		fprintf(stderr, ": Subtracting pointers of incomplete type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Generate IR type expression for the element type */
	rc = cgen_cgtype(cgexpr->cgen, tptr1->tgtype, &elemte);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lval.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rval.varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_ptrdiff;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;
	instr->opt = elemte;

	elemte = NULL;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = ptdtype;
	if (lval.cvknown && rval.cvknown && lval.cvsymbol == rval.cvsymbol) {
		eres->cvknown = true;
		eres->cvint = lval.cvint - rval.cvint;
	}

	cgen_eres_fini(&lval);
	cgen_eres_fini(&rval);

	return EOK;
error:
	cgen_eres_fini(&lval);
	cgen_eres_fini(&rval);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	cgtype_destroy(ptdtype);
	ir_texpr_destroy(elemte);
	return rc;
}

/** Generate code for subtraction.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of subtraction
 * @return EOK on success or an error code
 */
static int cgen_sub(cgen_expr_t *cgexpr, ast_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	comp_tok_t *ctok;
	bool l_int;
	bool r_int;
	bool l_enum;
	bool r_enum;
	bool l_ptra;
	bool r_ptra;

	ctok = (comp_tok_t *)optok->data;

	l_int = cgen_type_is_integer(cgexpr->cgen, lres->cgtype);
	r_int = cgen_type_is_integer(cgexpr->cgen, rres->cgtype);
	l_enum = lres->cgtype->ntype == cgn_enum;
	r_enum = rres->cgtype->ntype == cgn_enum;

	/* Integer - integer */
	if (l_int && r_int)
		return cgen_sub_int(cgexpr, optok, lres, rres, lblock, eres);

	/* Enum - integer */
	if (l_enum && r_int) {
		return cgen_sub_enum_int(cgexpr, optok, lres, rres, lblock,
		    eres);
	}

	/* Integer - enum */
	if (l_int && r_enum) {
		cgen_warn_arith_enum(cgexpr->cgen, optok);
		return cgen_sub_int(cgexpr, optok, lres, rres, lblock, eres);
	}

	/* Enum - enum */
	if (l_enum && r_enum)
		return cgen_sub_enum(cgexpr, optok, lres, rres, lblock, eres);

	l_ptra = lres->cgtype->ntype == cgn_pointer ||
	    lres->cgtype->ntype == cgn_array;
	r_ptra = rres->cgtype->ntype == cgn_pointer ||
	    rres->cgtype->ntype == cgn_array;

	/* Pointer/array - pointer/array */
	if (l_ptra && r_ptra)
		return cgen_sub_ptra(cgexpr, ctok, lres, rres, lblock, eres);

	/* Pointer/array - integer */
	if (l_ptra && r_int)
		return cgen_sub_ptr_int(cgexpr, ctok, lres, rres, lblock, eres);

	/* Integer - pointer/array */
	if (l_int && r_ptra) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Invalid subtraction of ");
		(void) cgtype_print(lres->cgtype, stderr);
		fprintf(stderr, " and ");
		(void) cgtype_print(rres->cgtype, stderr);
		fprintf(stderr, ".\n");
		cgexpr->cgen->error = true; // TODO
		return EINVAL;
	}

	fprintf(stderr, "Unimplemented subtraction of ");
	(void) cgtype_print(lres->cgtype, stderr);
	fprintf(stderr, " and ");
	(void) cgtype_print(rres->cgtype, stderr);
	fprintf(stderr, ".\n");
	cgexpr->cgen->error = true; // TODO
	return EINVAL;
}

/** Generate code for multiplication.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of multiplication
 * @return EOK on success or an error code
 */
static int cgen_mul(cgen_expr_t *cgexpr, ast_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	cgtype_basic_t *tbasic;
	bool is_signed;
	unsigned bits;
	bool overflow;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic = (cgtype_basic_t *)lres->cgtype->ext;
	bits = cgen_basic_type_bits(cgexpr->cgen, tbasic);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen, tbasic);

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		cgen_cvint_mul(cgexpr->cgen, is_signed, bits, lres->cvint,
		    rres->cvint, &eres->cvint, &overflow);
		if (overflow)
			cgen_warn_integer_overflow(cgexpr->cgen, optok);
	}

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

/** Generate code for division.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of multiplication
 * @return EOK on success or an error code
 */
static int cgen_div(cgen_expr_t *cgexpr, ast_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	cgtype_basic_t *tbasic;
	bool is_signed;
	unsigned bits;
	bool divbyzero;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic = (cgtype_basic_t *)lres->cgtype->ext;
	bits = cgen_basic_type_bits(cgexpr->cgen, tbasic);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen, tbasic);

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = is_signed ? iri_sdiv : iri_udiv;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		cgen_cvint_div(cgexpr->cgen, is_signed, bits, lres->cvint,
		    rres->cvint, &eres->cvint, &divbyzero);
		if (divbyzero)
			cgen_warn_div_by_zero(cgexpr->cgen, optok);
	}

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

/** Generate code for modulus.
 *
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of multiplication
 * @return EOK on success or an error code
 */
static int cgen_mod(cgen_expr_t *cgexpr, ast_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	cgtype_basic_t *tbasic;
	bool is_signed;
	unsigned bits;
	bool divbyzero;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic = (cgtype_basic_t *)lres->cgtype->ext;
	bits = cgen_basic_type_bits(cgexpr->cgen, tbasic);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen, tbasic);

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = is_signed ? iri_smod : iri_umod;
	instr->width = bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		cgen_cvint_mod(cgexpr->cgen, is_signed, bits, lres->cvint,
		    rres->cvint, &eres->cvint, &divbyzero);
		if (divbyzero)
			cgen_warn_div_by_zero(cgexpr->cgen, optok);
	}

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
 * @param cgexpr Code generator for expression
 * @param optok Operand token (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_shl(cgen_expr_t *cgexpr, ast_tok_t *optok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	cgtype_basic_t *tbasic1;
	cgtype_basic_t *tbasic2;
	bool is_signed1;
	bool is_signed2;
	unsigned bits1;
	int rc;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic1 = (cgtype_basic_t *)lres->cgtype->ext;
	bits1 = cgen_basic_type_bits(cgexpr->cgen, tbasic1);
	if (bits1 == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed1 = cgen_basic_type_signed(cgexpr->cgen, tbasic1);

	/* Check the type */
	if (rres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic2 = (cgtype_basic_t *)rres->cgtype->ext;
	is_signed2 = cgen_basic_type_signed(cgexpr->cgen, tbasic2);

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_shl;
	instr->width = bits1;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		cgen_cvint_shl(cgexpr->cgen, is_signed1, bits1, lres->cvint,
		    rres->cvint, &eres->cvint);
		if (rres->cvint >= bits1)
			cgen_warn_shift_exceed_bits(cgexpr->cgen, optok);
		if (cgen_cvint_is_negative(cgexpr->cgen, is_signed2, rres->cvint))
			cgen_warn_shift_negative(cgexpr->cgen, optok);
	}

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
 * @param cgexpr Code generator for expression
 * @param ebinop Binary operator expression (for printing diagnostics)
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_shr(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    cgen_eres_t *lres, cgen_eres_t *rres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_t *cgtype = NULL;
	cgtype_basic_t *tbasic1;
	cgtype_basic_t *tbasic2;
	ast_tok_t *optok;
	unsigned bits1;
	bool is_signed1;
	bool is_signed2;
	int rc;

	optok = &ebinop->top;

	/* Check the type */
	if (lres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic1 = (cgtype_basic_t *)lres->cgtype->ext;
	bits1 = cgen_basic_type_bits(cgexpr->cgen, tbasic1);
	if (bits1 == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed1 = cgen_basic_type_signed(cgexpr->cgen, tbasic1);

	/* Check the type */
	if (rres->cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic2 = (cgtype_basic_t *)rres->cgtype->ext;
	is_signed2 = cgen_basic_type_signed(cgexpr->cgen, tbasic2);

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = is_signed1 ? iri_shra : iri_shrl;
	instr->width = bits1;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		cgen_cvint_shr(cgexpr->cgen, is_signed1, bits1, lres->cvint,
		    rres->cvint, &eres->cvint);
		if (rres->cvint >= bits1)
			cgen_warn_shift_exceed_bits(cgexpr->cgen, optok);
		if (cgen_cvint_is_negative(cgexpr->cgen, is_signed2, rres->cvint))
			cgen_warn_shift_negative(cgexpr->cgen, optok);
	}

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
 * @param cgexpr Code generator for expression
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_band(cgen_expr_t *cgexpr, cgen_eres_t *lres, cgen_eres_t *rres,
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
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		eres->cvint = lres->cvint & rres->cvint;
	}

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
 * @param cgexpr Code generator for expression
 * @param lres Result of evaluating left operand
 * @param rres Result of evaluating right operand
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store result of shifting
 * @return EOK on success or an error code
 */
static int cgen_bxor(cgen_expr_t *cgexpr, cgen_eres_t *lres, cgen_eres_t *rres,
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
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		eres->cvint = lres->cvint ^ rres->cvint;
	}

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
static int cgen_bor(cgen_expr_t *cgexpr, cgen_eres_t *lres, cgen_eres_t *rres,
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
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres->cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = cgtype_clone(lres->cgtype, &cgtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres->cvknown && rres->cvknown) {
		eres->cvknown = true;
		eres->cvint = lres->cvint | rres->cvint;
	}

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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_plus(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Add the two operands */
	rc = cgen_add(cgexpr, &ebinop->top, &lres, &rres, lblock, eres);
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_minus(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Subtract the two operands */
	rc = cgen_sub(cgexpr, &ebinop->top, &lres, &rres, lblock, eres);
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (multiplication)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_times(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgexpr, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/*
	 * Unsigned multiplication of mixed-sign numbers is OK.
	 * Multiplication involving enums is not.
	 */
	if ((flags & cguac_enum) != 0)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Multiply the two operands */
	rc = cgen_mul(cgexpr, &ebinop->top, &lres, &rres, lblock, eres);
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

/** Generate code for binary '/' operator.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (division)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_divide(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgexpr, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_div_sign_mix(cgexpr->cgen, &ebinop->top);
	if ((flags & cguac_enum) != 0)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in division */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Divide the two operands */
	rc = cgen_div(cgexpr, &ebinop->top, &lres, &rres, lblock, eres);
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

/** Generate code for binary '%' operator.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (modulo)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_modulo(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgexpr, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_div_sign_mix(cgexpr->cgen, &ebinop->top);
	if ((flags & cguac_enum) != 0)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in division */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Compute modulus of two operands */
	rc = cgen_mod(cgexpr, &ebinop->top, &lres, &rres, lblock, eres);
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (shift left)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_shl(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_eres_t lires;
	cgen_eres_t rires;
	bool conv1;
	bool conv2;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);
	cgen_eres_init(&lires);
	cgen_eres_init(&rires);

	/* Promoted value of left operand */
	rc = cgen_expr_promoted_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &lres, &lires, &conv1);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &rres, &rires, &conv2);
	if (rc != EOK)
		goto error;

	if (conv1 || conv2)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in left shift */
	if (cgen_type_is_logic(cgexpr->cgen, lires.cgtype) ||
	    cgen_type_is_logic(cgexpr->cgen, rires.cgtype))
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Shift left */
	rc = cgen_shl(cgexpr, &ebinop->top, &lires, &rires, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&lires);
	cgen_eres_fini(&rires);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&lires);
	cgen_eres_fini(&rires);
	return rc;
}

/** Generate code for shift right operator.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (shift right)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_shr(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_eres_t lires;
	cgen_eres_t rires;
	bool conv1;
	bool conv2;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);
	cgen_eres_init(&lires);
	cgen_eres_init(&rires);

	/* Promoted value of left operand */
	rc = cgen_expr_promoted_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &lres, &lires, &conv1);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &rres, &rires, &conv2);
	if (rc != EOK)
		goto error;

	if (conv1 || conv2)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in right shift */
	if (cgen_type_is_logic(cgexpr->cgen, lires.cgtype) ||
	    cgen_type_is_logic(cgexpr->cgen, rires.cgtype))
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Shift right */
	rc = cgen_shr(cgexpr, ebinop, &lires, &rires, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&lires);
	cgen_eres_fini(&rires);

	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&lires);
	cgen_eres_fini(&rires);
	return rc;
}

/** Generate code for integer less than expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param ares Left operand result
 * @param bres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lt_int(cgen_expr_t *cgexpr, ast_tok_t *atok, cgen_eres_t *ares,
    cgen_eres_t *bres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Perform usual arithmetic conversions */
	rc = cgen_uac(cgexpr, ares, bres, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	assert(lres.cgtype->ntype == cgn_basic);
	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_cmp_sign_mix(cgexpr->cgen, atok);
	if ((flags & cguac_neg2u) != 0)
		cgen_warn_cmp_neg_unsigned(cgexpr->cgen, atok);
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_cmp_enum_inc(cgexpr->cgen, atok);
	if ((flags & cguac_enummix) != 0)
		cgen_warn_cmp_enum_mix(cgexpr->cgen, atok);
	if ((flags & cguac_truthmix) != 0)
		cgen_warn_cmp_truth_mix(cgexpr->cgen, atok);

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres.cvknown && rres.cvknown) {
		eres->cvknown = true;
		if (is_signed) {
			eres->cvint = lres.cvint < rres.cvint ? 1 : 0;
		} else {
			eres->cvint = (uint64_t)lres.cvint <
			    (uint64_t)rres.cvint ? 1 : 0;
		}
	}

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

/** Generate code for pointer less than expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param lres Left operand result
 * @param rres Right operand result
 * @param ebinop AST binary operator expression (less than)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lt_ptr(cgen_expr_t *cgexpr, ast_tok_t *atok, cgen_eres_t *lres,
    cgen_eres_t *rres, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_basic_t *btype = NULL;
	cgtype_pointer_t *tptr1;
	cgtype_pointer_t *tptr2;
	int rc;

	/* Warn for incompatible pointer types */
	assert(lres->cgtype->ntype == cgn_pointer);
	tptr1 = (cgtype_pointer_t *)lres->cgtype->ext;
	assert(rres->cgtype->ntype == cgn_pointer);
	tptr2 = (cgtype_pointer_t *)rres->cgtype->ext;

	if (!cgtype_ptr_compatible(tptr1, tptr2)) {
		cgen_warn_cmp_incom_ptr(cgexpr->cgen, atok, lres->cgtype,
		    rres->cgtype);
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_ltu;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	if (lres->cvknown && rres->cvknown && lres->cvsymbol == NULL &&
	    rres->cvsymbol == NULL) {
		eres->cvknown = true;
		eres->cvint = (uint64_t)lres->cvint <
		    (uint64_t)rres->cvint ? 1 : 0;
	}

	/* In a constant expression the result must be known */
	if (cgexpr->cexpr && eres->cvknown == false) {
		cgen_error_cmp_ptr_nc(cgexpr->cgen, atok);
		return EINVAL;
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for less than expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (less than)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lt(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	int rc;

	cgen_eres_t lres;
	cgen_eres_t rres;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	if (lres.cgtype->ntype == cgn_pointer &&
	    rres.cgtype->ntype == cgn_pointer) {
		rc = cgen_lt_ptr(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else if (cgen_type_is_integral(cgexpr->cgen, lres.cgtype) &&
	    cgen_type_is_integral(cgexpr->cgen, rres.cgtype)) {
		rc = cgen_lt_int(cgexpr, &ebinop->top, &lres, &rres,
		    lblock, eres);
		if (rc != EOK)
			goto error;
	} else {
		cgen_error_cmp_invalid(cgexpr->cgen, &ebinop->top,
		    lres.cgtype, rres.cgtype);
		rc = EINVAL;
		goto error;
	}

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for integer less than or equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param ares Left operand result
 * @param bres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lteq_int(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *ares, cgen_eres_t *bres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Perform usual arithmetic conversions */
	rc = cgen_uac(cgexpr, ares, bres, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	assert(lres.cgtype->ntype == cgn_basic);
	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_cmp_sign_mix(cgexpr->cgen, atok);
	if ((flags & cguac_neg2u) != 0)
		cgen_warn_cmp_neg_unsigned(cgexpr->cgen, atok);
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_cmp_enum_inc(cgexpr->cgen, atok);
	if ((flags & cguac_enummix) != 0)
		cgen_warn_cmp_enum_mix(cgexpr->cgen, atok);
	if ((flags & cguac_truthmix) != 0)
		cgen_warn_cmp_truth_mix(cgexpr->cgen, atok);

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres.cvknown && rres.cvknown) {
		eres->cvknown = true;
		if (is_signed) {
			eres->cvint = lres.cvint <= rres.cvint ? 1 : 0;
		} else {
			eres->cvint = (uint64_t)lres.cvint <=
			    (uint64_t)rres.cvint ? 1 : 0;
		}
	}

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

/** Generate code for pointer less than or equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param lres Left operand result
 * @param rres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lteq_ptr(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_basic_t *btype = NULL;
	cgtype_pointer_t *tptr1;
	cgtype_pointer_t *tptr2;
	int rc;

	/* Warn for incompatible pointer types */
	assert(lres->cgtype->ntype == cgn_pointer);
	tptr1 = (cgtype_pointer_t *)lres->cgtype->ext;
	assert(rres->cgtype->ntype == cgn_pointer);
	tptr2 = (cgtype_pointer_t *)rres->cgtype->ext;

	if (!cgtype_ptr_compatible(tptr1, tptr2)) {
		cgen_warn_cmp_incom_ptr(cgexpr->cgen, atok, lres->cgtype,
		    rres->cgtype);
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_lteu;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	if (lres->cvknown && rres->cvknown && lres->cvsymbol == NULL &&
	    rres->cvsymbol == NULL) {
		eres->cvknown = true;
		eres->cvint = (uint64_t)lres->cvint <=
		    (uint64_t)rres->cvint ? 1 : 0;
	}

	/* In a constant expression the result must be known */
	if (cgexpr->cexpr && eres->cvknown == false) {
		cgen_error_cmp_ptr_nc(cgexpr->cgen, atok);
		return EINVAL;
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for less than or equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (less than or equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lteq(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	int rc;

	cgen_eres_t lres;
	cgen_eres_t rres;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	if (lres.cgtype->ntype == cgn_pointer &&
	    rres.cgtype->ntype == cgn_pointer) {
		rc = cgen_lteq_ptr(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else if (cgen_type_is_integral(cgexpr->cgen, lres.cgtype) &&
	    cgen_type_is_integral(cgexpr->cgen, rres.cgtype)) {
		rc = cgen_lteq_int(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else {
		cgen_error_cmp_invalid(cgexpr->cgen, &ebinop->top,
		    lres.cgtype, rres.cgtype);
		rc = EINVAL;
		goto error;
	}

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for integer greater than expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param ares Left operand result
 * @param bres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gt_int(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *ares, cgen_eres_t *bres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Perform usual arithmetic conversions */
	rc = cgen_uac(cgexpr, ares, bres, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	assert(lres.cgtype->ntype == cgn_basic);
	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_cmp_sign_mix(cgexpr->cgen, atok);
	if ((flags & cguac_neg2u) != 0)
		cgen_warn_cmp_neg_unsigned(cgexpr->cgen, atok);
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_cmp_enum_inc(cgexpr->cgen, atok);
	if ((flags & cguac_enummix) != 0)
		cgen_warn_cmp_enum_mix(cgexpr->cgen, atok);
	if ((flags & cguac_truthmix) != 0)
		cgen_warn_cmp_truth_mix(cgexpr->cgen, atok);

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres.cvknown && rres.cvknown) {
		eres->cvknown = true;
		if (is_signed) {
			eres->cvint = lres.cvint > rres.cvint ? 1 : 0;
		} else {
			eres->cvint = (uint64_t)lres.cvint >
			    (uint64_t)rres.cvint ? 1 : 0;
		}
	}

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

/** Generate code for pointer greater than expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param lres Left operand result
 * @param rres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gt_ptr(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_basic_t *btype = NULL;
	cgtype_pointer_t *tptr1;
	cgtype_pointer_t *tptr2;
	int rc;

	/* Warn for incompatible pointer types */
	assert(lres->cgtype->ntype == cgn_pointer);
	tptr1 = (cgtype_pointer_t *)lres->cgtype->ext;
	assert(rres->cgtype->ntype == cgn_pointer);
	tptr2 = (cgtype_pointer_t *)rres->cgtype->ext;

	if (!cgtype_ptr_compatible(tptr1, tptr2)) {
		cgen_warn_cmp_incom_ptr(cgexpr->cgen, atok, lres->cgtype,
		    rres->cgtype);
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_gtu;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	if (lres->cvknown && rres->cvknown && lres->cvsymbol == NULL &&
	    rres->cvsymbol == NULL) {
		eres->cvknown = true;
		eres->cvint = (uint64_t)lres->cvint >
		    (uint64_t)rres->cvint ? 1 : 0;
	}

	/* In a constant expression the result must be known */
	if (cgexpr->cexpr && eres->cvknown == false) {
		cgen_error_cmp_ptr_nc(cgexpr->cgen, atok);
		return EINVAL;
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for greater than expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (greater than)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gt(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	int rc;

	cgen_eres_t lres;
	cgen_eres_t rres;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	if (lres.cgtype->ntype == cgn_pointer &&
	    rres.cgtype->ntype == cgn_pointer) {
		rc = cgen_gt_ptr(cgexpr, &ebinop->top, &lres, &rres,
		    lblock, eres);
		if (rc != EOK)
			goto error;
	} else if (cgen_type_is_integral(cgexpr->cgen, lres.cgtype) &&
	    cgen_type_is_integral(cgexpr->cgen, rres.cgtype)) {
		rc = cgen_gt_int(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else {
		cgen_error_cmp_invalid(cgexpr->cgen, &ebinop->top,
		    lres.cgtype, rres.cgtype);
		rc = EINVAL;
		goto error;
	}

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for integer greater than or equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param ares Left operand result
 * @param bres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gteq_int(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *ares, cgen_eres_t *bres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	bool is_signed;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Perform usual arithmetic conversions */
	rc = cgen_uac(cgexpr, ares, bres, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	assert(lres.cgtype->ntype == cgn_basic);
	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_cmp_sign_mix(cgexpr->cgen, atok);
	if ((flags & cguac_neg2u) != 0)
		cgen_warn_cmp_neg_unsigned(cgexpr->cgen, atok);
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_cmp_enum_inc(cgexpr->cgen, atok);
	if ((flags & cguac_enummix) != 0)
		cgen_warn_cmp_enum_mix(cgexpr->cgen, atok);
	if ((flags & cguac_truthmix) != 0)
		cgen_warn_cmp_truth_mix(cgexpr->cgen, atok);

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres.cvknown && rres.cvknown) {
		eres->cvknown = true;
		if (is_signed) {
			eres->cvint = lres.cvint >= rres.cvint ? 1 : 0;
		} else {
			eres->cvint = (uint64_t)lres.cvint >=
			    (uint64_t)rres.cvint ? 1 : 0;
		}
	}

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

/** Generate code for pointer greater than or equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param lres Left operand result
 * @param rres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gteq_ptr(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_basic_t *btype = NULL;
	cgtype_pointer_t *tptr1;
	cgtype_pointer_t *tptr2;
	int rc;

	/* Warn for incompatible pointer types */
	assert(lres->cgtype->ntype == cgn_pointer);
	tptr1 = (cgtype_pointer_t *)lres->cgtype->ext;
	assert(rres->cgtype->ntype == cgn_pointer);
	tptr2 = (cgtype_pointer_t *)rres->cgtype->ext;

	if (!cgtype_ptr_compatible(tptr1, tptr2)) {
		cgen_warn_cmp_incom_ptr(cgexpr->cgen, atok, lres->cgtype,
		    rres->cgtype);
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_gteu;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	if (lres->cvknown && rres->cvknown && lres->cvsymbol == NULL &&
	    rres->cvsymbol == NULL) {
		eres->cvknown = true;
		eres->cvint = (uint64_t)lres->cvint >=
		    (uint64_t)rres->cvint ? 1 : 0;
	}

	/* In a constant expression the result must be known */
	if (cgexpr->cexpr && eres->cvknown == false) {
		cgen_error_cmp_ptr_nc(cgexpr->cgen, atok);
		return EINVAL;
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for greater than or equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (greater than or equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_gteq(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	int rc;

	cgen_eres_t lres;
	cgen_eres_t rres;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	if (lres.cgtype->ntype == cgn_pointer &&
	    rres.cgtype->ntype == cgn_pointer) {
		rc = cgen_gteq_ptr(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else if (cgen_type_is_integral(cgexpr->cgen, lres.cgtype) &&
	    cgen_type_is_integral(cgexpr->cgen, rres.cgtype)) {
		rc = cgen_gteq_int(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else {
		cgen_error_cmp_invalid(cgexpr->cgen, &ebinop->top,
		    lres.cgtype, rres.cgtype);
		rc = EINVAL;
		goto error;
	}

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for integer equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param ares Left operand result
 * @param bres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eq_int(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *ares, cgen_eres_t *bres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Perform usual arithmetic conversions */
	rc = cgen_uac(cgexpr, ares, bres, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	assert(lres.cgtype->ntype == cgn_basic);
	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_cmp_sign_mix(cgexpr->cgen, atok);
	if ((flags & cguac_neg2u) != 0)
		cgen_warn_cmp_neg_unsigned(cgexpr->cgen, atok);
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_cmp_enum_inc(cgexpr->cgen, atok);
	if ((flags & cguac_enummix) != 0)
		cgen_warn_cmp_enum_mix(cgexpr->cgen, atok);
	if ((flags & cguac_truthmix) != 0)
		cgen_warn_cmp_truth_mix(cgexpr->cgen, atok);

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres.cvknown && rres.cvknown) {
		eres->cvknown = true;
		eres->cvint = lres.cvint == rres.cvint ? 1 : 0;
	}

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

/** Generate code for pointer equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param lres Left operand result
 * @param rres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eq_ptr(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_basic_t *btype = NULL;
	cgtype_pointer_t *tptr1;
	cgtype_pointer_t *tptr2;
	int rc;

	/* Warn for incompatible pointer types */
	assert(lres->cgtype->ntype == cgn_pointer);
	tptr1 = (cgtype_pointer_t *)lres->cgtype->ext;
	assert(rres->cgtype->ntype == cgn_pointer);
	tptr2 = (cgtype_pointer_t *)rres->cgtype->ext;

	if (!cgtype_ptr_compatible(tptr1, tptr2)) {
		cgen_warn_cmp_incom_ptr(cgexpr->cgen, atok, lres->cgtype,
		    rres->cgtype);
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_eq;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	if (lres->cvknown && rres->cvknown && lres->cvsymbol == NULL &&
	    rres->cvsymbol == NULL) {
		eres->cvknown = true;
		eres->cvint = (uint64_t)lres->cvint >=
		    (uint64_t)rres->cvint ? 1 : 0;
	}

	/* In a constant expression the result must be known */
	if (cgexpr->cexpr && eres->cvknown == false) {
		cgen_error_cmp_ptr_nc(cgexpr->cgen, atok);
		return EINVAL;
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eq(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	int rc;

	cgen_eres_t lres;
	cgen_eres_t rres;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	if (lres.cgtype->ntype == cgn_pointer &&
	    rres.cgtype->ntype == cgn_pointer) {
		rc = cgen_eq_ptr(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else if (cgen_type_is_integral(cgexpr->cgen, lres.cgtype) &&
	    cgen_type_is_integral(cgexpr->cgen, rres.cgtype)) {
		rc = cgen_eq_int(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else {
		cgen_error_cmp_invalid(cgexpr->cgen, &ebinop->top,
		    lres.cgtype, rres.cgtype);
		rc = EINVAL;
		goto error;
	}

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for integer not equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param ares Left operand result
 * @param bres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_neq_int(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *ares, cgen_eres_t *bres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgtype_basic_t *btype = NULL;
	cgen_uac_flags_t flags;
	unsigned bits;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Perform usual arithmetic conversions */
	rc = cgen_uac(cgexpr, ares, bres, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	assert(lres.cgtype->ntype == cgn_basic);
	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)lres.cgtype->ext);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_cmp_sign_mix(cgexpr->cgen, atok);
	if ((flags & cguac_neg2u) != 0)
		cgen_warn_cmp_neg_unsigned(cgexpr->cgen, atok);
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_cmp_enum_inc(cgexpr->cgen, atok);
	if ((flags & cguac_enummix) != 0)
		cgen_warn_cmp_enum_mix(cgexpr->cgen, atok);
	if ((flags & cguac_truthmix) != 0)
		cgen_warn_cmp_truth_mix(cgexpr->cgen, atok);

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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

	if (lres.cvknown && rres.cvknown) {
		eres->cvknown = true;
		eres->cvint = lres.cvint != rres.cvint ? 1 : 0;
	}

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

/** Generate code for pointer not equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param atok Operator token
 * @param lres Left operand result
 * @param rres Right operand result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_neq_ptr(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *lres, cgen_eres_t *rres, ir_lblock_t *lblock,
    cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	cgtype_basic_t *btype = NULL;
	cgtype_pointer_t *tptr1;
	cgtype_pointer_t *tptr2;
	int rc;

	/* Warn for incompatible pointer types */
	assert(lres->cgtype->ntype == cgn_pointer);
	tptr1 = (cgtype_pointer_t *)lres->cgtype->ext;
	assert(rres->cgtype->ntype == cgn_pointer);
	tptr2 = (cgtype_pointer_t *)rres->cgtype->ext;

	if (!cgtype_ptr_compatible(tptr1, tptr2)) {
		cgen_warn_cmp_incom_ptr(cgexpr->cgen, atok, lres->cgtype,
		    rres->cgtype);
	}

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(lres->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(rres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_neq;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	if (lres->cvknown && rres->cvknown && lres->cvsymbol == NULL &&
	    rres->cvsymbol == NULL) {
		eres->cvknown = true;
		eres->cvint = (uint64_t)lres->cvint !=
		    (uint64_t)rres->cvint ? 1 : 0;
	}

	/* In a constant expression the result must be known */
	if (cgexpr->cexpr && eres->cvknown == false) {
		cgen_error_cmp_ptr_nc(cgexpr->cgen, atok);
		return EINVAL;
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	if (btype != NULL)
		cgtype_destroy(&btype->cgtype);
	return rc;
}

/** Generate code for not equal expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (not equal)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_neq(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	int rc;

	cgen_eres_t lres;
	cgen_eres_t rres;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	if (lres.cgtype->ntype == cgn_pointer &&
	    rres.cgtype->ntype == cgn_pointer) {
		rc = cgen_neq_ptr(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else if (cgen_type_is_integral(cgexpr->cgen, lres.cgtype) &&
	    cgen_type_is_integral(cgexpr->cgen, rres.cgtype)) {
		rc = cgen_neq_int(cgexpr, &ebinop->top, &lres, &rres, lblock,
		    eres);
		if (rc != EOK)
			goto error;
	} else {
		cgen_error_cmp_invalid(cgexpr->cgen, &ebinop->top,
		    lres.cgtype, rres.cgtype);
		rc = EINVAL;
		goto error;
	}

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for bitwise AND expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (bitwise AND)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_band(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_eres_t bres;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);
	cgen_eres_init(&lres);
	cgen_eres_init(&rres);
	cgen_eres_init(&bres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &res1);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &res2);
	if (rc != EOK)
		goto error;

	/* Usual arithmetic conversions */
	rc = cgen_uac(cgexpr, &res1, &res2, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Integer (not enum) operands, any of them signed */
	if ((flags & cguac_signed) != 0 && (flags & cguac_enum) == 0)
		cgen_warn_bitop_signed(cgexpr->cgen, &ebinop->top);
	/* Any operand is a negative constant */
	if ((flags & cguac_negative) != 0)
		cgen_warn_bitop_negative(cgexpr->cgen, &ebinop->top);
	/* Two incompatible enums */
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_bitop_enum_inc(cgexpr->cgen, &ebinop->top);
	/* One enum, one not */
	if ((flags & cguac_enummix) != 0)
		cgen_warn_bitop_enum_mix(cgexpr->cgen, &ebinop->top);
	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Bitwise AND */
	rc = cgen_band(cgexpr, &lres, &rres, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Operating on enums? */
	if ((flags & cguac_enum) != 0 && (flags & cguac_enuminc) == 0 &&
	    (flags & cguac_enummix) == 0) {
		/* Convert result back to original enum type, if possible */
		rc = cgen_int2enum(cgexpr, &bres, res1.cgtype, eres);
		if (rc != EOK)
			return rc;
	} else {
		rc = cgen_eres_clone(&bres, eres);
		if (rc != EOK)
			return rc;
	}

	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&bres);

	return EOK;
error:
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for bitwise XOR expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (bitwise XOR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_bxor(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_eres_t bres;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);
	cgen_eres_init(&lres);
	cgen_eres_init(&rres);
	cgen_eres_init(&bres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &res1);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &res2);
	if (rc != EOK)
		goto error;

	/* Usual arithmetic conversions */
	rc = cgen_uac(cgexpr, &res1, &res2, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Integer (not enum) operands, any of them signed */
	if ((flags & cguac_signed) != 0 && (flags & cguac_enum) == 0)
		cgen_warn_bitop_signed(cgexpr->cgen, &ebinop->top);
	/* Any operand is a negative constant */
	if ((flags & cguac_negative) != 0)
		cgen_warn_bitop_negative(cgexpr->cgen, &ebinop->top);
	/* Two incompatible enums */
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_bitop_enum_inc(cgexpr->cgen, &ebinop->top);
	/* One enum, one not */
	if ((flags & cguac_enummix) != 0)
		cgen_warn_bitop_enum_mix(cgexpr->cgen, &ebinop->top);
	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Bitwise XOR */
	rc = cgen_bxor(cgexpr, &lres, &rres, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Operating on enums? */
	if ((flags & cguac_enum) != 0 && (flags & cguac_enuminc) == 0 &&
	    (flags & cguac_enummix) == 0) {
		/* Convert result back to original enum type, if possible */
		rc = cgen_int2enum(cgexpr, &bres, res1.cgtype, eres);
		if (rc != EOK)
			return rc;
	} else {
		rc = cgen_eres_clone(&bres, eres);
		if (rc != EOK)
			return rc;
	}

	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&bres);

	return EOK;
error:
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for bitwise OR expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (bitwise OR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bo_bor(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_eres_t bres;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);
	cgen_eres_init(&lres);
	cgen_eres_init(&rres);
	cgen_eres_init(&bres);

	/* Evaluate left operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &res1);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &res2);
	if (rc != EOK)
		goto error;

	/* Usual arithmetic conversions */
	rc = cgen_uac(cgexpr, &res1, &res2, lblock, &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	/* Integer (not enum) operands, any of them signed */
	if ((flags & cguac_signed) != 0 && (flags & cguac_enum) == 0)
		cgen_warn_bitop_signed(cgexpr->cgen, &ebinop->top);
	/* Any operand is a negative constant */
	if ((flags & cguac_negative) != 0)
		cgen_warn_bitop_negative(cgexpr->cgen, &ebinop->top);
	/* Two incompatible enums */
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_bitop_enum_inc(cgexpr->cgen, &ebinop->top);
	/* One enum, one not */
	if ((flags & cguac_enummix) != 0)
		cgen_warn_bitop_enum_mix(cgexpr->cgen, &ebinop->top);
	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Bitwise OR */
	rc = cgen_bor(cgexpr, &lres, &rres, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Operating on enums? */
	if ((flags & cguac_enum) != 0 && (flags & cguac_enuminc) == 0 &&
	    (flags & cguac_enummix) == 0) {
		/* Convert result back to original enum type, if possible */
		rc = cgen_int2enum(cgexpr, &bres, res1.cgtype, eres);
		if (rc != EOK)
			return rc;
	} else {
		rc = cgen_eres_clone(&bres, eres);
		if (rc != EOK)
			return rc;
	}

	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&bres);

	return EOK;
error:
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for logical AND expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (logical AND)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_land(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
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

	lblno = cgen_new_label_num(cgexpr->cgproc);

	rc = cgen_create_label(cgexpr->cgproc, "false_and", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgexpr->cgproc, "end_and", lblno, &elabel);
	if (rc != EOK)
		goto error;

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	/* Evaluate left argument */

	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Jump to %false_and if left argument is zero */

	rc = cgen_truth_eres_cjmp(cgexpr, ast_tree_first_tok(ebinop->larg),
	    &lres, false, flabel, lblock);
	if (rc != EOK)
		goto error;

	/* Evaluate right argument */

	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Jump to %false_and if right argument is zero */

	rc = cgen_truth_eres_cjmp(cgexpr, ast_tree_first_tok(ebinop->rarg),
	    &rres, false, flabel, lblock);
	if (rc != EOK)
		goto error;

	/* Return 1 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(1, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgexpr->cgen->arith_width;
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
	instr->width = cgexpr->cgen->arith_width;
	instr->dest = &dest2->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest2->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	dest2 = NULL;
	imm = NULL;

	if (lres.cvknown) {
		if (!cgen_eres_is_true(cgexpr->cgen, &lres)) {
			eres->cvknown = true;
			eres->cvint = 0;
		} else {
			if (rres.cvknown) {
				eres->cvknown = true;
				if (!cgen_eres_is_true(cgexpr->cgen, &rres))
					eres->cvint = 0;
				else
					eres->cvint = 1;
			}
		}
	}

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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (logical OR)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_lor(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
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

	lblno = cgen_new_label_num(cgexpr->cgproc);

	rc = cgen_create_label(cgexpr->cgproc, "true_or", lblno, &tlabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgexpr->cgproc, "end_or", lblno, &elabel);
	if (rc != EOK)
		goto error;

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	/* Evaluate left argument */

	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Jump to %true_or if left argument is not zero */

	rc = cgen_truth_eres_cjmp(cgexpr, ast_tree_first_tok(ebinop->larg),
	    &lres, true, tlabel, lblock);
	if (rc != EOK)
		goto error;

	/* Evaluate right argument */

	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Jump to %true_or if right argument is not zero */

	rc = cgen_truth_eres_cjmp(cgexpr, ast_tree_first_tok(ebinop->rarg),
	    &rres, true, tlabel, lblock);
	if (rc != EOK)
		goto error;

	/* Return 0 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(0, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgexpr->cgen->arith_width;
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
	instr->width = cgexpr->cgen->arith_width;
	instr->dest = &dest2->oper;
	instr->op1 = &imm->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);
	eres->varname = dest2->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = &btype->cgtype;

	dest2 = NULL;
	imm = NULL;

	if (lres.cvknown) {
		if (cgen_eres_is_true(cgexpr->cgen, &lres)) {
			eres->cvknown = true;
			eres->cvint = 1;
		} else {
			if (rres.cvknown) {
				eres->cvknown = true;
				if (cgen_eres_is_true(cgexpr->cgen, &rres))
					eres->cvint = 1;
				else
					eres->cvint = 0;
			}
		}
	}

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

/** Generate code to get address of local variable.
 *
 * @param cgproc Code generator for procedure
 * @param eident AST identifier expression
 * @param vident Identifier of IR variable holding the local variable
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 *
 * @return EOK on success or an error code
 */
static int cgen_lvaraddr(cgen_proc_t *cgproc, const char *vident,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	int rc;

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

/** Generate code for storing a record (in an assignment expression).
 *
 * @param cgproc Code generator for procedure
 * @param ares Address expression result
 * @param vres Value expression result
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_store_record(cgen_proc_t *cgproc, cgen_eres_t *ares,
    cgen_eres_t *vres, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	ir_texpr_t *recte = NULL;
	int rc;

	assert(vres->cgtype->ntype == cgn_record);

	/* Generate IR type expression for the record type */
	rc = cgen_cgtype(cgproc->cgen, vres->cgtype, &recte);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(ares->varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(vres->varname, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_reccopy;
	instr->width = 0;
	instr->dest = NULL;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;
	instr->opt = recte;

	ir_lblock_append(lblock, NULL, instr);

	return EOK;
error:
	ir_texpr_destroy(recte);
	ir_instr_destroy(instr);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);
	return rc;
}

/** Generate code for storing a value.
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
	} else if (vres->cgtype->ntype == cgn_record) {
		return cgen_store_record(cgproc, ares, vres, lblock);
	} else if (vres->cgtype->ntype == cgn_enum) {
		bits = cgen_enum_bits;
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (addition)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
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

	/* Address of left hand expression */
	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right hand expression */
	rc = cgen_expr(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) ebinop->top.data;

	if (lres.cgtype->ntype == cgn_array) {
		cgen_error_assign_array(cgexpr->cgen, &ebinop->top);
		rc = EINVAL;
		goto error;
	}

	/* Convert expression result to the destination type */
	rc = cgen_type_convert(cgexpr, ctok, &rres, lres.cgtype,
	    cgen_implicit, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* Store the converted value */
	rc = cgen_store(cgexpr->cgproc, &lres, &cres, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from lres */
	cgtype = lres.cgtype;
	lres.cgtype = NULL;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	cgen_eres_fini(&cres);

	eres->varname = cres.varname;
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (add assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_plus_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
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

	/* Address of left hand expression */
	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &laddr);
	if (rc != EOK)
		goto error;

	/* Value of left hand expression */
	rc = cgen_eres_rvalue(cgexpr, &laddr, lblock, &lval);
	if (rc != EOK)
		goto error;

	/* Value of right hand expression */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Add the two operands */
	rc = cgen_add(cgexpr, &ebinop->top, &lval, &rres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &laddr, &ores, lblock);
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (subtract assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_minus_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
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

	/* Address of left hand expression */
	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &laddr);
	if (rc != EOK)
		goto error;

	/* Value of left hand expression */
	rc = cgen_eres_rvalue(cgexpr, &laddr, lblock, &lval);
	if (rc != EOK)
		goto error;

	/* Value of right hand expression */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Subtract the two operands */
	rc = cgen_sub(cgexpr, &ebinop->top, &lval, &rres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &laddr, &ores, lblock);
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (mutiply assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_times_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
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
	rc = cgen_expr2lr_uac(cgexpr, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	/*
	 * Unsigned multiplication of mixed-sign numbers is OK.
	 * Multiplication involving enums is not.
	 */
	if ((flags & cguac_enum) != 0)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Multiply the two operands */
	rc = cgen_mul(cgexpr, &ebinop->top, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
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

/** Generate code for divide assign expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (divide assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_divide_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
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
	rc = cgen_expr2lr_uac(cgexpr, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_div_sign_mix(cgexpr->cgen, &ebinop->top);
	if ((flags & cguac_enum) != 0)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in division */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Divide the two operands */
	rc = cgen_div(cgexpr, &ebinop->top, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
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

/** Generate code for modulo assign expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (modulo assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_modulo_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
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
	rc = cgen_expr2lr_uac(cgexpr, ebinop->larg, ebinop->rarg, lblock,
	    &lres, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	if ((flags & cguac_mix2u) != 0)
		cgen_warn_div_sign_mix(cgexpr->cgen, &ebinop->top);
	if ((flags & cguac_enum) != 0)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in division */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Compute modulus of the two operands */
	rc = cgen_mod(cgexpr, &ebinop->top, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
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
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (shift left assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_shl_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t aires;
	cgen_eres_t bires;
	cgen_eres_t ores;
	bool conv1;
	bool conv2;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&aires);
	cgen_eres_init(&bires);
	cgen_eres_init(&ores);

	/* Address of left hand expression */
	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Promoted value of left operand */
	rc = cgen_eres_promoted_rvalue(cgexpr, &lres, lblock, &ares);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &ares, &aires, &conv1);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgexpr, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &bres, &bires, &conv2);
	if (rc != EOK)
		goto error;

	if (conv1 || conv2)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in left shift */
	if (cgen_type_is_logic(cgexpr->cgen, aires.cgtype) ||
	    cgen_type_is_logic(cgexpr->cgen, bires.cgtype))
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Shift left */
	rc = cgen_shl(cgexpr, &ebinop->top, &aires, &bires, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&aires);
	cgen_eres_fini(&bires);
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
	cgen_eres_fini(&aires);
	cgen_eres_fini(&bires);
	cgen_eres_fini(&ores);
	return rc;
}

/** Generate code for shift right assign expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (shift right assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression resucess or an error code
 */
static int cgen_shr_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t aires;
	cgen_eres_t bires;
	cgen_eres_t ores;
	bool conv1;
	bool conv2;
	cgtype_t *cgtype;
	const char *resvn;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&aires);
	cgen_eres_init(&bires);
	cgen_eres_init(&ores);

	/* Address of left hand expression */
	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Promoted value of left operand */
	rc = cgen_eres_promoted_rvalue(cgexpr, &lres, lblock, &ares);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &ares, &aires, &conv1);
	if (rc != EOK)
		goto error;

	/* Promoted value of right operand */
	rc = cgen_expr_promoted_rvalue(cgexpr, ebinop->rarg, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &bres, &bires, &conv2);
	if (rc != EOK)
		goto error;

	if (conv1 || conv2)
		cgen_warn_arith_enum(cgexpr->cgen, &ebinop->top);

	/* Warn if truth value involved in right shift */
	if (cgen_type_is_logic(cgexpr->cgen, aires.cgtype) ||
	    cgen_type_is_logic(cgexpr->cgen, bires.cgtype))
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Shift right */
	rc = cgen_shr(cgexpr, ebinop, &aires, &bires, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Salvage type from ores */
	cgtype = ores.cgtype;
	ores.cgtype = NULL;

	resvn = ores.varname;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&aires);
	cgen_eres_fini(&bires);
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
	cgen_eres_fini(&aires);
	cgen_eres_fini(&bires);
	cgen_eres_fini(&ores);
	return rc;
}

/** Generate code for bitwise AND assign expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (bitwise AND assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_band_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&res1);
	cgen_eres_init(&res2);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvalue(cgexpr, &lres, lblock, &res1);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &res2);
	if (rc != EOK)
		goto error;

	rc = cgen_uac(cgexpr, &res1, &res2, lblock, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	/* Integer (not enum) operands, any of them signed */
	if ((flags & cguac_signed) != 0 && (flags & cguac_enum) == 0)
		cgen_warn_bitop_signed(cgexpr->cgen, &ebinop->top);
	/* Any operand is a negative constant */
	if ((flags & cguac_negative) != 0)
		cgen_warn_bitop_negative(cgexpr->cgen, &ebinop->top);
	/* Two incompatible enums */
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_bitop_enum_inc(cgexpr->cgen, &ebinop->top);
	/* One enum, one not */
	if ((flags & cguac_enummix) != 0)
		cgen_warn_bitop_enum_mix(cgexpr->cgen, &ebinop->top);
	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Bitwise AND the two operands */
	rc = cgen_band(cgexpr, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Operating on enums? */
	if ((flags & cguac_enum) != 0 && (flags & cguac_enuminc) == 0 &&
	    (flags & cguac_enummix) == 0) {
		/* Convert result back to original enum type, if possible */
		rc = cgen_int2enum(cgexpr, &ores, res1.cgtype, eres);
		if (rc != EOK)
			goto error;
	} else {
		rc = cgen_eres_clone(&ores, eres);
		if (rc != EOK)
			goto error;
	}

	eres->valused = true;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
	return rc;
}

/** Generate code for bitwise XOR assign expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (bitwise XOR assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bxor_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&res1);
	cgen_eres_init(&res2);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvalue(cgexpr, &lres, lblock, &res1);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &res2);
	if (rc != EOK)
		goto error;

	rc = cgen_uac(cgexpr, &res1, &res2, lblock, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	/* Integer (not enum) operands, any of them signed */
	if ((flags & cguac_signed) != 0 && (flags & cguac_enum) == 0)
		cgen_warn_bitop_signed(cgexpr->cgen, &ebinop->top);
	/* Any operand is a negative constant */
	if ((flags & cguac_negative) != 0)
		cgen_warn_bitop_negative(cgexpr->cgen, &ebinop->top);
	/* Two incompatible enums */
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_bitop_enum_inc(cgexpr->cgen, &ebinop->top);
	/* One enum, one not */
	if ((flags & cguac_enummix) != 0)
		cgen_warn_bitop_enum_mix(cgexpr->cgen, &ebinop->top);
	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Bitwise XOR the two operands */
	rc = cgen_bxor(cgexpr, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Operating on enums? */
	if ((flags & cguac_enum) != 0 && (flags & cguac_enuminc) == 0 &&
	    (flags & cguac_enummix) == 0) {
		/* Convert result back to original enum type, if possible */
		rc = cgen_int2enum(cgexpr, &ores, res1.cgtype, eres);
		if (rc != EOK)
			goto error;
	} else {
		rc = cgen_eres_clone(&ores, eres);
		if (rc != EOK)
			goto error;
	}

	eres->valused = true;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
	return rc;
}

/** Generate code for bitwise OR assign expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression (bitwise OR assign)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_bor_assign(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t res1;
	cgen_eres_t res2;
	cgen_eres_t ares;
	cgen_eres_t bres;
	cgen_eres_t ores;
	cgen_uac_flags_t flags;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&res1);
	cgen_eres_init(&res2);
	cgen_eres_init(&ares);
	cgen_eres_init(&bres);
	cgen_eres_init(&ores);

	rc = cgen_expr_lvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvalue(cgexpr, &lres, lblock, &res1);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &res2);
	if (rc != EOK)
		goto error;

	rc = cgen_uac(cgexpr, &res1, &res2, lblock, &ares, &bres, &flags);
	if (rc != EOK)
		goto error;

	/* Integer (not enum) operands, any of them signed */
	if ((flags & cguac_signed) != 0 && (flags & cguac_enum) == 0)
		cgen_warn_bitop_signed(cgexpr->cgen, &ebinop->top);
	/* Any operand is a negative constant */
	if ((flags & cguac_negative) != 0)
		cgen_warn_bitop_negative(cgexpr->cgen, &ebinop->top);
	/* Two incompatible enums */
	if ((flags & cguac_enuminc) != 0)
		cgen_warn_bitop_enum_inc(cgexpr->cgen, &ebinop->top);
	/* One enum, one not */
	if ((flags & cguac_enummix) != 0)
		cgen_warn_bitop_enum_mix(cgexpr->cgen, &ebinop->top);
	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ebinop->top);

	/* Bitwise OR the two operands */
	rc = cgen_bor(cgexpr, &ares, &bres, lblock, &ores);
	if (rc != EOK)
		goto error;

	/* Store the resulting value */
	rc = cgen_store(cgexpr->cgproc, &lres, &ores, lblock);
	if (rc != EOK)
		goto error;

	/* Operating on enums? */
	if ((flags & cguac_enum) != 0 && (flags & cguac_enuminc) == 0 &&
	    (flags & cguac_enummix) == 0) {
		/* Convert result back to original enum type, if possible */
		rc = cgen_int2enum(cgexpr, &ores, res1.cgtype, eres);
		if (rc != EOK)
			goto error;
	} else {
		rc = cgen_eres_clone(&ores, eres);
		if (rc != EOK)
			goto error;
	}

	eres->valused = true;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
	return EOK;
error:
	cgen_eres_fini(&lres);
	cgen_eres_fini(&res1);
	cgen_eres_fini(&res2);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&ores);
	return rc;
}

/** Generate code for binary operator expression.
 *
 * @param cgexpr Code generator for expression
 * @param ebinop AST binary operator expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ebinop(cgen_expr_t *cgexpr, ast_ebinop_t *ebinop,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	switch (ebinop->optype) {
	case abo_plus:
		return cgen_bo_plus(cgexpr, ebinop, lblock, eres);
	case abo_minus:
		return cgen_bo_minus(cgexpr, ebinop, lblock, eres);
	case abo_times:
		return cgen_bo_times(cgexpr, ebinop, lblock, eres);
	case abo_divide:
		return cgen_bo_divide(cgexpr, ebinop, lblock, eres);
	case abo_modulo:
		return cgen_bo_modulo(cgexpr, ebinop, lblock, eres);
	case abo_shl:
		return cgen_bo_shl(cgexpr, ebinop, lblock, eres);
	case abo_shr:
		return cgen_bo_shr(cgexpr, ebinop, lblock, eres);
	case abo_lt:
		return cgen_lt(cgexpr, ebinop, lblock, eres);
	case abo_lteq:
		return cgen_lteq(cgexpr, ebinop, lblock, eres);
	case abo_gt:
		return cgen_gt(cgexpr, ebinop, lblock, eres);
	case abo_gteq:
		return cgen_gteq(cgexpr, ebinop, lblock, eres);
	case abo_eq:
		return cgen_eq(cgexpr, ebinop, lblock, eres);
	case abo_neq:
		return cgen_neq(cgexpr, ebinop, lblock, eres);
	case abo_band:
		return cgen_bo_band(cgexpr, ebinop, lblock, eres);
	case abo_bxor:
		return cgen_bo_bxor(cgexpr, ebinop, lblock, eres);
	case abo_bor:
		return cgen_bo_bor(cgexpr, ebinop, lblock, eres);
	case abo_land:
		return cgen_land(cgexpr, ebinop, lblock, eres);
	case abo_lor:
		return cgen_lor(cgexpr, ebinop, lblock, eres);
	case abo_assign:
		return cgen_assign(cgexpr, ebinop, lblock, eres);
	case abo_plus_assign:
		return cgen_plus_assign(cgexpr, ebinop, lblock, eres);
	case abo_minus_assign:
		return cgen_minus_assign(cgexpr, ebinop, lblock, eres);
	case abo_times_assign:
		return cgen_times_assign(cgexpr, ebinop, lblock, eres);
	case abo_divide_assign:
		return cgen_divide_assign(cgexpr, ebinop, lblock, eres);
	case abo_modulo_assign:
		return cgen_modulo_assign(cgexpr, ebinop, lblock, eres);
	case abo_shl_assign:
		return cgen_shl_assign(cgexpr, ebinop, lblock, eres);
	case abo_shr_assign:
		return cgen_shr_assign(cgexpr, ebinop, lblock, eres);
	case abo_band_assign:
		return cgen_band_assign(cgexpr, ebinop, lblock, eres);
	case abo_bxor_assign:
		return cgen_bxor_assign(cgexpr, ebinop, lblock, eres);
	case abo_bor_assign:
		return cgen_bor_assign(cgexpr, ebinop, lblock, eres);
	}

	/* Should not be reached */
	assert(false);
	return EINVAL;
}

/** Determine return type from conitional operator.
 *
 * The return type is computed from the type of the arguments.
 *
 * @param cgexpr Code generator for expression
 * @param tok Operator token
 * @param ares Result of true expression
 * @param bres Result of false expression
 * @param rrtype Place to store result type
 */
static int cgen_etcond_rtype(cgen_expr_t *cgexpr, comp_tok_t *tok,
    cgen_eres_t *ares, cgen_eres_t *bres, cgtype_t **rrtype)
{
	cgtype_t *atype;
	cgtype_t *btype;
	cgtype_record_t *arec;
	cgtype_record_t *brec;
	cgtype_pointer_t *aptr;
	cgtype_pointer_t *bptr;

	atype = ares->cgtype;
	btype = bres->cgtype;

	if (cgen_type_is_arithmetic(cgexpr->cgen, atype) &&
	    cgen_type_is_arithmetic(cgexpr->cgen, btype)) {
		/* Both operands have arithmetic type */
		if (cgen_type_is_logic(cgexpr->cgen, atype) &&
		    cgen_type_is_logic(cgexpr->cgen, btype)) {
			/* Both operands have logic type */
			return cgtype_clone(atype, rrtype);
		} else if (atype->ntype == cgn_enum && btype->ntype == cgn_enum &&
		    cgen_enum_types_are_compatible(cgexpr->cgen, atype, btype)) {
			/* Same enum types */
			return cgtype_clone(atype, rrtype);
		} else {
			/* Integer/floating */
			return cgen_uac_rtype(cgexpr, atype, btype, rrtype);
		}
	}

	/* Both operands have the same structure or union type */
	if (atype->ntype == cgn_record && btype->ntype == cgn_record) {
		arec = (cgtype_record_t *)atype->ext;
		brec = (cgtype_record_t *)btype->ext;

		if (arec->record == brec->record)
			return cgtype_clone(atype, rrtype);
	}

	/* Both operands have void type */
	if (cgtype_is_void(atype) && cgtype_is_void(btype)) {
		return cgtype_clone(atype, rrtype);
	}

	/*
	 * Both operands are pointers to qualified or unqualified version of
	 * compatible types
	 */
	if (atype->ntype == cgn_pointer && btype->ntype == cgn_pointer) {
		aptr = (cgtype_pointer_t *)atype->ext;
		bptr = (cgtype_pointer_t *)btype->ext;

		/* Result type has all the qualifiers from both types */
		if (cgtype_ptr_compatible(aptr, bptr))
			return cgtype_ptr_combine_qual(aptr, bptr, rrtype);
	}

	/* One operand is a pointer and the other is a null pointer constant */
	if (atype->ntype == cgn_pointer && cgen_eres_is_int_zero(cgexpr->cgen,
	    bres)) {
		return cgtype_clone(atype, rrtype);
	} else if (cgen_eres_is_int_zero(cgexpr->cgen, ares) &&
	    btype->ntype == cgn_pointer) {
		return cgtype_clone(btype, rrtype);
	}

	/*
	 * One pointer is a pointer to an object or incomplete type and
	 * the other is a pointer to a qualified or unqualified version of
	 * void
	 */
	if (atype->ntype == cgn_pointer && btype->ntype == cgn_pointer) {
		aptr = (cgtype_pointer_t *)atype->ext;
		bptr = (cgtype_pointer_t *)btype->ext;

		if (cgtype_is_void(aptr->tgtype)) {
			/* Return appropriately qualified pointer to void */
			// XXX Qualifiers
			return cgtype_clone(atype, rrtype);
		} else if (cgtype_is_void(bptr->tgtype)) {
			/* Return appropriately qualified pointer to void */
			// XXX Qualifiers
			return cgtype_clone(btype, rrtype);
		}
	}

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": Invalid argument types to conditional operator (");
	cgtype_print(atype, stderr);
	fprintf(stderr, ", ");
	cgtype_print(btype, stderr);
	fprintf(stderr, ").\n");
	cgexpr->cgen->error = true; // TODO
	return EINVAL;
	(void)rrtype;
}

/** Generate code for conditional operator expression.
 *
 * @param cgexpr Code generator for expression
 * @param etcond AST conditional operator expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_etcond(cgen_expr_t *cgexpr, ast_etcond_t *etcond,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *dest = NULL;
	cgtype_t *rtype = NULL;
	cgen_eres_t cres;
	cgen_eres_t tres;
	cgen_eres_t fres;
	cgen_eres_t tcres;
	cgen_eres_t fcres;
	char *flabel = NULL;
	char *elabel = NULL;
	unsigned lblno;
	ir_lblock_t *flblock = NULL;
	bool isvoid;
	comp_tok_t *ctok;
	int rc;

	cgen_eres_init(&cres);
	cgen_eres_init(&tres);
	cgen_eres_init(&fres);
	cgen_eres_init(&tcres);
	cgen_eres_init(&fcres);

	lblno = cgen_new_label_num(cgexpr->cgproc);

	rc = cgen_create_label(cgexpr->cgproc, "false_cond", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgexpr->cgproc, "end_cond", lblno, &elabel);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgexpr, etcond->cond, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* Jump to %false_cond if condition argument is zero */

	rc = cgen_truth_eres_cjmp(cgexpr, ast_tree_first_tok(etcond->cond),
	    &cres, false, flabel, lblock);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgexpr, etcond->targ, lblock, &tres);
	if (rc != EOK)
		goto error;

	/*
	 * At this point we need to know the type of result of etcond->farg
	 * so that we can determine rtype. But it's too early.
	 * So we need to store the generated code on the side in flblock.
	 */
	rc = ir_lblock_create(&flblock);
	if (rc != EOK)
		goto error;

	/* Generate farg, but put it on the side into flblock. */
	rc = cgen_expr_rvalue(cgexpr, etcond->farg, flblock, &fres);
	if (rc != EOK)
		goto error;

	/* Compute result type */
	rc = cgen_etcond_rtype(cgexpr, (comp_tok_t *)etcond->tqmark.data,
	    &tres, &fres, &rtype);
	if (rc != EOK)
		goto error;

	/* If the type is void, we don't have a value, just side effects. */
	isvoid = cgtype_is_void(rtype);
	if (isvoid) {
		ctok = (comp_tok_t *)etcond->tqmark.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Conditional with void operands "
		    "can be rewritten as an if-else statement.\n");
		++cgexpr->cgen->warnings;
	}

	/* Convert tres to result type */
	rc = cgen_type_convert(cgexpr, (comp_tok_t *)etcond->tqmark.data,
	    &tres, rtype, cgen_implicit, lblock, &tcres);
	if (rc != EOK)
		goto error;

	/* jmp %end_cond */

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

	/* %false_cond label */
	ir_lblock_append(lblock, flabel, NULL);

	/* Append code for computing etcond->farg */
	ir_lblock_move_entries(flblock, lblock);
	ir_lblock_destroy(flblock);
	flblock = NULL;

	/* Convert fres to result type */
	rc = cgen_type_convert(cgexpr, (comp_tok_t *)etcond->tcolon.data,
	    &fres, rtype, cgen_implicit, lblock, &fcres);
	if (rc != EOK)
		goto error;

	if (!isvoid) {
		/* copy %t, %f XXX Violates SSA */

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
		rc = ir_oper_var_create(tcres.varname, &dest);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(fcres.varname, &larg);
		if (rc != EOK)
			goto error;

		instr->itype = iri_copy;
		instr->width = cgexpr->cgen->arith_width;
		instr->dest = &dest->oper;
		instr->op1 = &larg->oper;
		instr->op2 = NULL;

		ir_lblock_append(lblock, NULL, instr);

		eres->varname = tcres.varname;
		eres->valtype = cgen_rvalue;
		eres->cgtype = rtype;

		dest = NULL;
		larg = NULL;
	} else {
		eres->varname = NULL;
		eres->valtype = cgen_rvalue;
		eres->cgtype = rtype;
		eres->valused = true;
	}

	/* %end_cond label */
	ir_lblock_append(lblock, elabel, NULL);

	free(flabel);
	free(elabel);
	cgen_eres_fini(&cres);
	cgen_eres_fini(&tres);
	cgen_eres_fini(&fres);
	cgen_eres_fini(&tcres);
	cgen_eres_fini(&fcres);
	return EOK;
error:
	if (flabel != NULL)
		free(flabel);
	if (elabel != NULL)
		free(elabel);
	cgen_eres_fini(&cres);
	cgen_eres_fini(&tres);
	cgen_eres_fini(&fres);
	cgen_eres_fini(&tcres);
	cgen_eres_fini(&fcres);
	ir_lblock_destroy(flblock);
	cgtype_destroy(rtype);
	return rc;
}

/** Generate code for comma expression.
 *
 * @param cgexpr Code generator for expression
 * @param ecomma AST comma expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ecomma(cgen_expr_t *cgexpr, ast_ecomma_t *ecomma,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	int rc;

	/* Evaluate and ignore left argument */
	rc = cgen_expr(cgexpr, ecomma->larg, lblock, &lres);
	if (rc != EOK)
		return rc;

	cgen_expr_check_unused(cgexpr, ecomma->larg, &lres);

	cgen_eres_fini(&lres);

	/* Evaluate and return right argument */
	return cgen_expr(cgexpr, ecomma->rarg, lblock, eres);
}

/** Check dimension of array passed to function.
 *
 * @param cgen Code generator
 * @param ftype Formal argument type
 * @param atype Actual argument type
 */
static void cgen_check_passed_array_dim(cgen_t *cgen, comp_tok_t *tok,
    cgtype_t *ftype, cgtype_t *atype)
{
	cgtype_array_t *farray;
	cgtype_array_t *aarray;

	(void)cgen;

	assert(ftype->ntype == cgn_array);
	farray = (cgtype_array_t *)ftype->ext;

	assert(atype->ntype == cgn_array);
	aarray = (cgtype_array_t *)atype->ext;

	if (aarray->asize < farray->asize) {
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Array passed to function is too "
		    "small (expected dimension %" PRId64 ", actual dimension %"
		    PRId64 ").\n", farray->asize, aarray->asize);
		++cgen->warnings;
	}
}

/** Generate call signature.
 *
 * An indirect call instruction needs to know the signature of the function
 * being called. As pointers are opaque, we need to provide it separately.
 *
 * cgen_callsign() declares a 'procedure' with the callsign linkage.
 * This is not a real procedure, it just serves as a function type
 * declaration.
 *
 * E.g.:
 *     proc @@callsign1() : int.16 callsign;
 *     proc @@callsign2() : int.16 attr(usr);
 *
 * Then you need to pass the name of the callsignature as the type operand
 * to icall:
 *     icall %d, { }, @@callsign1
 *
 * This allows the compiler to know the type of the procedure's arguments,
 * its return type and attributes (e.g. calling convention).
 *
 * cgen_callsign() creates an IR type expression containing the identifier
 * of the call signature. This can be used directly as the icall's
 * type operard.
 *
 * @param cgen Code generator
 * @param ftype Function type
 * @param rcstype Place to store pointer to type expression
 * @return EOK on success or an erro
 */
static int cgen_callsign(cgen_t *cgen, cgtype_func_t *ftype,
    ir_texpr_t **rcstype)
{
	ir_proc_t *proc = NULL;
	ir_proc_arg_t *parg = NULL;
	ir_texpr_t *atype = NULL;
	cgtype_func_arg_t *arg;
	char *pident = NULL;
	char *aident = NULL;
	unsigned aidx;
	int rc;
	int rv;

	++cgen->callsign_cnt;
	rv = asprintf(&pident, "@@callsign_%u", cgen->callsign_cnt);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	rc = ir_proc_create(pident, irl_callsign, NULL, &proc);
	if (rc != EOK)
		goto error;

	aidx = 0;
	arg = cgtype_func_first(ftype);
	while (arg != NULL) {
		rc = cgen_cgtype(cgen, arg->atype, &atype);
		if (rc != EOK)
			goto error;

		rv = asprintf(&aident, "%%%u", aidx++);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		rc = ir_proc_arg_create(aident, atype, &parg);
		if (rc != EOK)
			goto error;

		free(aident);
		aident = NULL;
		atype = NULL;

		ir_proc_append_arg(proc, parg);
		parg = NULL;
		arg = cgtype_func_next(arg);
	}

	/* Return type */
	rc = cgen_cgtype(cgen, ftype->rtype, &proc->rtype);
	if (rc != EOK)
		goto error;

	ir_module_append(cgen->irmod, &proc->decln);
	proc = NULL;

	rc = ir_texpr_ident_create(pident, rcstype);
	if (rc != EOK)
		goto error;

	free(pident);
	return EOK;
error:
	if (aident != NULL)
		free(aident);
	if (pident != NULL)
		free(pident);
	if (proc != NULL)
		ir_proc_destroy(proc);
	if (parg != NULL)
		ir_proc_arg_destroy(parg);
	if (atype != NULL)
		ir_texpr_destroy(atype);
	return rc;
}

/** Generate code for call expression.
 *
 * @param cgexpr Code generator for expression
 * @param ecall AST call expression)
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ecall(cgen_expr_t *cgexpr, ast_ecall_t *ecall,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	char *cident = "<anonymous>";
	char *pident = NULL;
	ast_ecall_arg_t *earg;
	cgen_eres_t ares;
	cgen_eres_t cres;
	cgen_eres_t fres;
	cgen_eres_t frres;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *fun = NULL;
	ir_oper_list_t *args = NULL;
	ir_oper_var_t *arg = NULL;
	cgtype_t *rtype = NULL;
	cgtype_pointer_t *fptype;
	cgtype_func_t *ftype;
	cgtype_func_arg_t *farg;
	cgtype_t *argtype = NULL;
	ir_texpr_t *cstype = NULL;
	int rc;

	cgen_eres_init(&ares);
	cgen_eres_init(&cres);
	cgen_eres_init(&fres);
	cgen_eres_init(&frres);

	/*
	 * Evaluate the function expression
	 */
	rc = cgen_expr(cgexpr, ecall->fexpr, lblock, &fres);
	if (rc != EOK)
		goto error;

	/* Is the result a function pointer? */
	if (cgen_type_is_fptr(cgexpr->cgen, fres.cgtype)) {
		assert(fres.cgtype->ntype == cgn_pointer);
		fptype = (cgtype_pointer_t *)fres.cgtype->ext;
		assert(fptype->tgtype->ntype == cgn_func);
		ftype = (cgtype_func_t *)fptype->tgtype->ext;

		/* Need to convert it to rvalue */
		rc = cgen_eres_rvalue(cgexpr, &fres, lblock, &frres);
		if (rc != EOK)
			goto error;
	} else if (fres.cgtype->ntype == cgn_func) {
		/* It's a function */
		ftype = (cgtype_func_t *)fres.cgtype->ext;

		rc = cgen_eres_clone(&fres, &frres);
		if (rc != EOK)
			goto error;
	} else {
		if (fres.cvknown)
			cident = fres.cvsymbol->ident->tok.text;

		tok = (comp_tok_t *)ecall->tlparen.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Called object '%s' is not a function.\n",
		    cident);
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Is the symbol name known at compile time? */
	if (frres.cvknown) {
		cident = frres.cvsymbol->ident->tok.text;
		rc = cgen_gprefix(cident, &pident);
		if (rc != EOK)
			goto error;
	}

	rc = cgtype_clone(ftype->rtype, &rtype);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
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
		if (farg == NULL && ftype->variadic == false) {
			atok = ast_tree_first_tok(earg->arg);
			tok = (comp_tok_t *) atok->data;

			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Too many arguments to function "
			    "'%s'.\n", cident);
			cgexpr->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = cgen_expr(cgexpr, earg->arg, lblock, &ares);
		if (rc != EOK)
			goto error;

		atok = ast_tree_first_tok(earg->arg);
		tok = (comp_tok_t *)atok->data;

		/*
		 * Is the type of the argument known? (i.e. declared and
		 * not variadic?)
		 */
		if (farg != NULL) {
			/*
			 * If the function has a prototype and the argument is not
			 * variadic, convert it to its declared type.
			 */

			/* Check dimension of passed array */
			if (farg->atype->ntype == cgn_array &&
			    ares.cgtype->ntype == cgn_array) {
				cgen_check_passed_array_dim(cgexpr->cgen, tok,
				    farg->atype, ares.cgtype);
			}

			rc = cgen_fun_arg_passed_type(cgexpr->cgen, farg->atype,
			    &argtype);
			if (rc != EOK)
				goto error;

			rc = cgen_type_convert(cgexpr, tok, &ares, argtype,
			    cgen_implicit, lblock, &cres);
			if (rc != EOK)
				goto error;

			cgtype_destroy(argtype);
			argtype = NULL;
		} else {
			/*
			 * Just promote it
			 */
			rc = cgen_eres_promoted_rvalue(cgexpr, &ares, lblock,
			    &cres);
			if (rc != EOK)
				goto error;
		}

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
		if (farg != NULL)
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
		    cident);
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if (!cgtype_is_void(ftype->rtype)) {
		rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
		if (rc != EOK)
			goto error;
	}

	/* Identifier of called function is known? */
	if (pident != NULL) {
		/* Direct call */
		rc = ir_oper_var_create(pident, &fun);
		if (rc != EOK)
			goto error;

		instr->itype = iri_call;
		instr->dest = dest != NULL ? &dest->oper : NULL;
		instr->op1 = &fun->oper;
		instr->op2 = &args->oper;
	} else {
		/* Indirect call */

		/* Declare call signature */
		rc = cgen_callsign(cgexpr->cgen, ftype, &cstype);
		if (rc != EOK)
			goto error;

		/* Function address operand */
		rc = ir_oper_var_create(frres.varname, &fun);
		if (rc != EOK)
			goto error;

		instr->itype = iri_calli;
		instr->width = cgen_pointer_bits;
		instr->dest = dest != NULL ? &dest->oper : NULL;
		instr->op1 = &fun->oper;
		instr->op2 = &args->oper;
		instr->opt = cstype;

		cstype = NULL;
	}

	ir_lblock_append(lblock, NULL, instr);

	free(pident);

	eres->varname = dest ? dest->varname : NULL;
	eres->valtype = cgen_rvalue;
	eres->cgtype = rtype;
	eres->valused = cgtype_is_void(ftype->rtype);

	cgen_eres_fini(&ares);
	cgen_eres_fini(&cres);
	cgen_eres_fini(&fres);
	cgen_eres_fini(&frres);
	return EOK;
error:
	ir_instr_destroy(instr);
	if (cstype != NULL)
		ir_texpr_destroy(cstype);
	if (argtype != NULL)
		cgtype_destroy(argtype);
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
	cgen_eres_fini(&fres);
	cgen_eres_fini(&frres);
	cgtype_destroy(rtype);
	return rc;
}

/** Generate code for index expression.
 *
 * @param cgexpr Code generator for expression
 * @param eindex AST index expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eindex(cgen_expr_t *cgexpr, ast_eindex_t *eindex,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	cgen_eres_t ires;
	cgen_eres_t sres;
	comp_tok_t *ctok;
	cgtype_pointer_t *ptrtype;
	cgtype_t *cgtype;
	bool b_ptra;
	bool i_ptra;
	bool b_inte;
	bool i_inte;
	int rc;

	cgen_eres_init(&bres);
	cgen_eres_init(&ires);
	cgen_eres_init(&sres);

	/*
	 * In this case we must allow for the expression results
	 * to be either lvalue (e.g. an array) or rvalue (e.g. array index).
	 */

	/* Evaluate base operand */
	rc = cgen_expr(cgexpr, eindex->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Evaluate index operand */
	rc = cgen_expr(cgexpr, eindex->iexpr, lblock, &ires);
	if (rc != EOK)
		goto error;

	b_inte = cgen_type_is_integer(cgexpr->cgen, bres.cgtype) ||
	    bres.cgtype->ntype == cgn_enum;

	i_inte = cgen_type_is_integer(cgexpr->cgen, ires.cgtype) ||
	    ires.cgtype->ntype == cgn_enum;

	b_ptra = bres.cgtype->ntype == cgn_pointer ||
	    bres.cgtype->ntype == cgn_array;
	i_ptra = ires.cgtype->ntype == cgn_pointer ||
	    ires.cgtype->ntype == cgn_array;

	ctok = (comp_tok_t *) eindex->tlbracket.data;

	if (!b_ptra && !i_ptra) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Subscripted object is neither pointer nor array.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if ((b_ptra && !i_inte) || (i_ptra && !b_inte)) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Subscript index is not an integer.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Add the two operands */
	rc = cgen_add(cgexpr, &eindex->tlbracket, &bres, &ires, lblock, &sres);
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
 * @param cgexpr Code generator for expression
 * @param ederef AST dereference expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ederef(cgen_expr_t *cgexpr, ast_ederef_t *ederef,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	cgtype_t *cgtype;
	comp_tok_t *tok;
	cgtype_pointer_t *ptrtype;
	int rc;

	cgen_eres_init(&bres);

	/* Evaluate expression as rvalue */
	rc = cgen_expr_rvalue(cgexpr, ederef->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Check that we are dereferencing a pointer */
	if (bres.cgtype->ntype != cgn_pointer) {
		/* Still some left */
		tok = (comp_tok_t *) ederef->tasterisk.data;

		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Dereference operator needs a pointer, got '");
		(void) cgtype_print(bres.cgtype, stderr);
		fprintf(stderr, "'.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if (cgen_type_is_fptr(cgexpr->cgen, bres.cgtype)) {
		tok = (comp_tok_t *)ederef->tasterisk.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Explicitly dereferencing function "
		    "pointer is not necessary.\n");
		++cgexpr->cgen->warnings;
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
 * @param cgexpr Code generator for expression
 * @param eaddr AST address expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eaddr(cgen_expr_t *cgexpr, ast_eaddr_t *eaddr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	cgtype_t *cgtype;
	cgtype_pointer_t *ptrtype;
	comp_tok_t *ctok;
	int rc;

	cgen_eres_init(&bres);

	/* Evaluate expression as lvalue */
	rc = cgen_expr_lvalue(cgexpr, eaddr->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	if (bres.cgtype->ntype == cgn_func) {
		ctok = (comp_tok_t *)eaddr->tamper.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Explicitly taking the address of "
		    "a function is not necessary.\n");
		++cgexpr->cgen->warnings;
	}

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
	eres->cvknown = bres.cvknown;
	eres->cvint = bres.cvint;
	eres->cvsymbol = bres.cvsymbol;
	eres->cgtype = cgtype;

	return EOK;
error:
	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for sizeof() with processed type.
 *
 * Once we processed the typename or type of the expression, i.e.,
 * the argument to sizeof(), this function handles the rest.
 *
 * @param cgexpr Code generator for expression
 * @param etype Type for which we want to determine size
 * @param ctok Token for printing diagnostics
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_esizeof_cgtype(cgen_expr_t *cgexpr, cgtype_t *etype,
    comp_tok_t *ctok, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	unsigned sz;
	int rc;

	if (etype->ntype == cgn_func) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Sizeof operator applied to a function.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	sz = cgen_type_sizeof(cgexpr->cgen, etype);

	rc = cgen_const_int(cgexpr->cgproc, cgelm_int, sz, lblock, eres);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Generate code for sizeof(typename) expression.
 *
 * @param cgexpr Code generator for expression
 * @param esizeof AST sizeof expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_esizeof_typename(cgen_expr_t *cgexpr, ast_esizeof_t *esizeof,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgtype_t *stype = NULL;
	cgtype_t *etype = NULL;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	ast_sclass_type_t sctype;
	cgen_rd_flags_t flags;
	int rc;

	/* Declaration specifiers */
	rc = cgen_dspecs(cgexpr->cgen, esizeof->atypename->dspecs,
	    &sctype, &flags, &stype);
	if (rc != EOK)
		goto error;

	atok = ast_tree_first_tok(&esizeof->atypename->node);
	ctok = (comp_tok_t *) atok->data;

	if ((flags & cgrd_def) != 0) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Struct/union/enum definition inside sizeof().\n");
		++cgexpr->cgen->warnings;
	}

	if (sctype != asc_none) {
		atok = ast_tree_first_tok(&esizeof->atypename->node);
		ctok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented storage class specifier.\n");
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	/* Declarator */
	rc = cgen_decl(cgexpr->cgen, stype, esizeof->atypename->decl, NULL,
	    &etype);
	if (rc != EOK)
		goto error;

	rc  = cgen_esizeof_cgtype(cgexpr, etype, ctok, lblock, eres);
	if (rc != EOK)
		goto error;

	cgtype_destroy(stype);
	cgtype_destroy(etype);
	return EOK;
error:
	cgtype_destroy(stype);
	cgtype_destroy(etype);
	return rc;
}

/** Generate code for 'sizeof <expression>' expression.
 *
 * @param cgexpr Code generator for expression
 * @param esizeof AST sizeof expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_esizeof_expr(cgen_expr_t *cgexpr, ast_esizeof_t *esizeof,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgtype_t *etype = NULL;
	ast_eparen_t *eparen;
	ast_eident_t *eident;
	comp_tok_t *ident;
	scope_member_t *member;
	ast_tok_t *atok;
	comp_tok_t *ctok;

	int rc;

	/*
	 * Because the parser does not have semantic information,
	 * it misparses sizeof(type-ident) as sizeof applied to
	 * a parenthesized expression. In this particular case
	 * we need to re-interpret it as a type identifier.
	 */
	if (esizeof->bexpr->ntype == ant_eparen) {
		eparen = (ast_eparen_t *)esizeof->bexpr->ext;

		if (eparen->bexpr->ntype == ant_eident) {
			/* Ambiguous case of sizeof(ident) */
			eident = (ast_eident_t *)eparen->bexpr->ext;

			/* Check if it is a type identifier */
			ident = (comp_tok_t *)eident->tident.data;
			member = scope_lookup(cgexpr->cgen->cur_scope,
			    ident->tok.text);
			if (member != NULL && member->mtype == sm_tdef) {
				/* It is a type identifier */

				rc = cgen_tident(cgexpr->cgen, &eident->tident,
				    &etype);
				if (rc != EOK)
					goto error;
			}
		}
	}

	/* In the normal case */
	if (etype == NULL) {
		/*
		 * 'Evaluate' the expression getting its type and ignoring both
		 * the result and any side effects.
		 */
		rc = cgen_szexpr_type(cgexpr->cgen, esizeof->bexpr, &etype);
		if (rc != EOK)
			goto error;
	}

	atok = ast_tree_first_tok(esizeof->bexpr);
	ctok = (comp_tok_t *) atok->data;

	rc  = cgen_esizeof_cgtype(cgexpr, etype, ctok, lblock, eres);
	if (rc != EOK)
		goto error;

	cgtype_destroy(etype);
	return EOK;
error:
	cgtype_destroy(etype);
	return rc;
}

/** Generate code for sizeof expression.
 *
 * @param cgexpr Code generator for expression
 * @param esizeof AST sizeof expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_esizeof(cgen_expr_t *cgexpr, ast_esizeof_t *esizeof,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	if (esizeof->atypename != NULL)
		return cgen_esizeof_typename(cgexpr, esizeof, lblock, eres);
	else
		return cgen_esizeof_expr(cgexpr, esizeof, lblock, eres);
}

/** Generate code for overparenthesized multiplication misparsed as a
 * cast expression.
 *
 * @param cgexpr Code generator for expression
 * @param ecast AST cast expression (misparsed multiplication)
 * @param ident Identifier token which was misparsed as type
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_overpar_bo_times(cgen_expr_t *cgexpr, ast_ecast_t *ecast,
    comp_tok_t *ident, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	cgen_uac_flags_t flags;
	ast_ederef_t *ederef;
	ast_eident_t *eident = NULL;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	assert(ecast->bexpr->ntype == ant_ederef);
	ederef = (ast_ederef_t *)ecast->bexpr->ext;

	rc = ast_eident_create(&eident);
	if (rc != EOK)
		goto error;

	eident->tident.data = (void *)ident;

	/* Evaluate and perform usual arithmetic conversions on operands */
	rc = cgen_expr2_uac(cgexpr, &eident->node, ederef->bexpr, lblock,
	    &lres, &rres, &flags);
	if (rc != EOK)
		goto error;

	ast_tree_destroy(&eident->node);
	eident = NULL;

	/*
	 * Unsigned multiplication of mixed-sign numbers is OK.
	 * Multiplication involving enums is not.
	 */
	if ((flags & cguac_enum) != 0)
		cgen_warn_arith_enum(cgexpr->cgen, &ederef->tasterisk);

	/* Warn if truth value involved in multiplication */
	if ((flags & cguac_truth) != 0)
		cgen_warn_arith_truth(cgexpr->cgen, &ederef->tasterisk);

	/* Multiply the two operands */
	rc = cgen_mul(cgexpr, &ederef->tasterisk, &lres, &rres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	if (eident != NULL)
		ast_tree_destroy(&eident->node);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for overparenthesized addition or subtraction misparsed as a
 * cast expression.
 *
 * @param cgexpr Code generator for expression
 * @param ecast AST cast expression (misparsed addition/subtraction)
 * @param ident Identifier token which was misparsed as type
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_overpar_addsub(cgen_expr_t *cgexpr, ast_ecast_t *ecast,
    comp_tok_t *ident, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t lres;
	cgen_eres_t rres;
	ast_eusign_t *eusign = NULL;
	ast_eident_t *eident = NULL;
	int rc;

	cgen_eres_init(&lres);
	cgen_eres_init(&rres);

	assert(ecast->bexpr->ntype == ant_eusign);
	eusign = (ast_eusign_t *)ecast->bexpr->ext;

	rc = ast_eident_create(&eident);
	if (rc != EOK)
		goto error;

	eident->tident.data = (void *)ident;

	/* Evaluate left operand */
	rc = cgen_expr(cgexpr, &eident->node, lblock, &lres);
	if (rc != EOK)
		goto error;

	ast_tree_destroy(&eident->node);
	eident = NULL;

	/* Evaluate right operand */
	rc = cgen_expr(cgexpr, eusign->bexpr, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Add the two operands */
	if (eusign->usign == aus_plus) {
		rc = cgen_add(cgexpr, &eusign->tsign, &lres, &rres, lblock,
		    eres);
	} else {
		rc = cgen_sub(cgexpr, &eusign->tsign, &lres, &rres, lblock,
		    eres);
	}

	if (rc != EOK)
		goto error;

	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);

	return EOK;
error:
	if (eident != NULL)
		ast_tree_destroy(&eident->node);
	cgen_eres_fini(&lres);
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for overparenthesized call expression misparsed as a
 * cast expression.
 *
 * @param cgexpr Code generator for expression
 * @param ecast AST cast expression (misparsed call)
 * @param ident Identifier token which was misparsed as type
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_overpar_call(cgen_expr_t *cgexpr, ast_ecast_t *ecast,
    comp_tok_t *ident, ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_eparen_t *eparen = NULL;
	ast_eident_t *eident = NULL;
	ast_ecall_t *ecall = NULL;
	ast_ecomma_t *ecomma;
	ast_node_t *node;
	ast_ecall_arg_t *arg;
	int rc;

	assert(ecast->bexpr->ntype == ant_eparen);
	eparen = (ast_eparen_t *)ecast->bexpr->ext;

	rc = ast_eident_create(&eident);
	if (rc != EOK)
		goto error;

	eident->tident.data = (void *)ident;

	rc = ast_ecall_create(&ecall);
	if (rc != EOK)
		goto error;

	ecall->fexpr = &eident->node;
	ecall->tlparen.data = eparen->tlparen.data;
	ecall->trparen.data = eparen->trparen.data;
	eident = NULL;

	node = eparen->bexpr;
	while (node->ntype == ant_ecomma) {
		ecomma = (ast_ecomma_t *)node->ext;

		rc = ast_ecall_prepend(ecall, NULL, ecomma->rarg);
		if (rc != EOK)
			goto error;

		node = ecomma->larg;
	}

	rc = ast_ecall_prepend(ecall, NULL, node);
	if (rc != EOK)
		goto error;

	rc = cgen_ecall(cgexpr, ecall, lblock, eres);
	if (rc != EOK)
		goto error;

	arg = ast_ecall_first(ecall);
	while (arg != NULL) {
		arg->arg = NULL;
		arg = ast_ecall_next(arg);
	}
	ast_tree_destroy(&ecall->node);
	return EOK;
error:
	if (eident != NULL)
		ast_tree_destroy(&eident->node);
	if (ecall != NULL) {
		arg = ast_ecall_first(ecall);
		while (arg != NULL) {
			arg->arg = NULL;
			arg = ast_ecall_next(arg);
		}
		ast_tree_destroy(&ecall->node);
	}
	return rc;
}

/** Generate code for cast expression.
 *
 * @param cgexpr Code generator for expression
 * @param ecast AST cast expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ecast(cgen_expr_t *cgexpr, ast_ecast_t *ecast,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	comp_tok_t *ident;
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	ast_sclass_type_t sctype;
	cgen_rd_flags_t flags;
	scope_member_t *member;
	int rc;

	cgen_eres_init(&bres);

	/*
	 * Check for overparenthesized mutiplication, addition or subtraction
	 * misparsed as a cast expression.
	 */
	rc = cgen_decl_is_just_ident(ecast->dspecs, ecast->decl, &atok);
	if (rc == EOK) {
		/* Check if it is a type identifier */
		ident = (comp_tok_t *)atok->data;
		member = scope_lookup(cgexpr->cgen->cur_scope,
		    ident->tok.text);
		if (member == NULL || member->mtype != sm_tdef) {
			/* It is NOT a type identifier, therefore not a cast. */
			switch (ecast->bexpr->ntype) {
			case ant_ederef:
				rc = cgen_overpar_bo_times(cgexpr, ecast,
				    ident, lblock, eres);
				break;
			case ant_eusign:
				rc = cgen_overpar_addsub(cgexpr, ecast, ident,
				    lblock, eres);
				break;
			case ant_eparen:
				rc = cgen_overpar_call(cgexpr, ecast, ident,
				    lblock, eres);
				break;
			default:
				lexer_dprint_tok(&ident->tok, stderr);
				fprintf(stderr, ": Type identifier "
				    "expected.\n");

				cgexpr->cgen->error = true; // TODO
				rc = EINVAL;
				break;
			}

			if (rc != EOK)
				goto error;

			cgen_eres_fini(&bres);
			return EOK;
		}
	}

	/* Declaration specifiers */
	rc = cgen_dspecs(cgexpr->cgen, ecast->dspecs, &sctype, &flags, &stype);
	if (rc != EOK)
		goto error;

	if ((flags & cgrd_def) != 0) {
		atok = ast_tree_first_tok(&ecast->dspecs->node);
		ctok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Struct/union/enum definition inside a cast.\n");
		++cgexpr->cgen->warnings;
	}

	if (sctype != asc_none) {
		atok = ast_tree_first_tok(&ecast->dspecs->node);
		ctok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented storage class specifier.\n");
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	/* Declarator */
	rc = cgen_decl(cgexpr->cgen, stype, ecast->decl, NULL, &dtype);
	if (rc != EOK)
		goto error;

	/* Evaluate expression */
	rc = cgen_expr(cgexpr, ecast->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *)ecast->tlparen.data;

	rc = cgen_type_convert(cgexpr, ctok, &bres, dtype,
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

/** Generate code for member expression.
 *
 * @param cgexpr Code generator for expression
 * @param emember AST member expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_emember(cgen_expr_t *cgexpr, ast_emember_t *emember,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	comp_tok_t *ctok;
	comp_tok_t *mtok;
	cgtype_t *btype;
	cgtype_record_t *rtype;
	cgtype_t *mtype;
	cgen_record_t *record;
	cgen_rec_elem_t *elem;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	unsigned mbroff;
	char *irident = NULL;
	ir_texpr_t *recte = NULL;
	int rc;
	int rv;

	cgen_eres_init(&bres);

	/* Evaluate expression */
	rc = cgen_expr(cgexpr, emember->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	btype = bres.cgtype;
	if (btype->ntype != cgn_record) {
		ctok = (comp_tok_t *)emember->tperiod.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": '.' requires a struct or union.\n");
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	rtype = (cgtype_record_t *)btype->ext;
	record = rtype->record;

	mtok = (comp_tok_t *)emember->tmember.data;

	elem = cgen_record_elem_find(record, mtok->tok.text, NULL);
	if (elem == NULL) {
		ctok = (comp_tok_t *)emember->tperiod.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Record type ");
		(void) cgtype_print(btype, stderr);
		fprintf(stderr, " has no member named '%s'.\n", mtok->tok.text);
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	rv = asprintf(&irident, "@%s", mtok->tok.text);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	/* Generate IR type expression for the record type */
	rc = cgen_cgtype(cgexpr->cgen, btype, &recte);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(irident, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_recmbr;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;
	instr->opt = recte;
	recte = NULL;

	ir_lblock_append(lblock, NULL, instr);

	rc = cgtype_clone(elem->cgtype, &mtype);
	if (rc != EOK)
		return rc;

	eres->varname = dest->varname;
	eres->valtype = cgen_lvalue;
	eres->cgtype = mtype;
	eres->valused = true;

	/* If record address is known */
	if (bres.cvknown) {
		/* Compute member address */
		mbroff = cgen_rec_elem_offset(cgexpr->cgen, elem);

		eres->cvknown = true;
		eres->cvint = bres.cvint + mbroff;
		eres->cvsymbol = bres.cvsymbol;
	}

	cgen_eres_fini(&bres);
	free(irident);
	return EOK;
error:
	if (irident != NULL)
		free(irident);
	ir_texpr_destroy(recte);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);

	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for indirect member expression.
 *
 * @param cgexpr Code generator for expression
 * @param eindmember AST indirect member expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eindmember(cgen_expr_t *cgexpr, ast_eindmember_t *eindmember,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	comp_tok_t *ctok;
	comp_tok_t *mtok;
	cgtype_t *btype;
	cgtype_pointer_t *ptype;
	cgtype_record_t *rtype;
	cgtype_t *mtype;
	cgen_record_t *record;
	cgen_rec_elem_t *elem;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	unsigned mbroff;
	char *irident = NULL;
	ir_texpr_t *recte = NULL;
	int rc;
	int rv;

	cgen_eres_init(&bres);

	/* Evaluate expression as rvalue */
	rc = cgen_expr_rvalue(cgexpr, eindmember->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	btype = bres.cgtype;
	if (btype->ntype != cgn_pointer) {
		ctok = (comp_tok_t *)eindmember->tarrow.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": '->' requires a pointer to a struct or union.\n");
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	ptype = (cgtype_pointer_t *)btype->ext;

	if (ptype->tgtype->ntype != cgn_record) {
		ctok = (comp_tok_t *)eindmember->tarrow.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": '->' requires a pointer to a struct or union.\n");
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	rtype = (cgtype_record_t *)ptype->tgtype->ext;
	record = rtype->record;

	mtok = (comp_tok_t *)eindmember->tmember.data;

	elem = cgen_record_elem_find(record, mtok->tok.text, NULL);
	if (elem == NULL) {
		ctok = (comp_tok_t *)eindmember->tarrow.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Record type ");
		(void) cgtype_print(ptype->tgtype, stderr);
		fprintf(stderr, " has no member named '%s'.\n", mtok->tok.text);
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	rv = asprintf(&irident, "@%s", mtok->tok.text);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	/* Generate IR type expression for the record type */
	rc = cgen_cgtype(cgexpr->cgen, ptype->tgtype, &recte);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bres.varname, &larg);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(irident, &rarg);
	if (rc != EOK)
		goto error;

	instr->itype = iri_recmbr;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &larg->oper;
	instr->op2 = &rarg->oper;
	instr->opt = recte;
	recte = NULL;

	ir_lblock_append(lblock, NULL, instr);

	rc = cgtype_clone(elem->cgtype, &mtype);
	if (rc != EOK)
		return rc;

	eres->varname = dest->varname;
	eres->valtype = cgen_lvalue;
	eres->cgtype = mtype;
	eres->valused = true;

	/* If record address is known */
	if (bres.cvknown) {
		/* Compute member address */
		mbroff = cgen_rec_elem_offset(cgexpr->cgen, elem);

		eres->cvknown = true;
		eres->cvint = bres.cvint + mbroff;
		eres->cvsymbol = bres.cvsymbol;
	}

	cgen_eres_fini(&bres);
	free(irident);
	return EOK;
error:
	if (irident != NULL)
		free(irident);
	ir_texpr_destroy(recte);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (larg != NULL)
		ir_oper_destroy(&larg->oper);
	if (rarg != NULL)
		ir_oper_destroy(&rarg->oper);

	cgen_eres_fini(&bres);
	return rc;
}

/** Generate code for unary sign expression.
 *
 * @param cgexpr Code generator for expression
 * @param eusign AST unary sign expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eusign(cgen_expr_t *cgexpr, ast_eusign_t *eusign,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *barg = NULL;
	comp_tok_t *ctok;
	cgen_eres_t bres;
	cgen_eres_t bires;
	cgen_eres_t sres;
	cgtype_basic_t *tbasic;
	bool conv;
	bool is_signed;
	unsigned bits;
	bool overflow;
	int rc;

	cgen_eres_init(&bres);
	cgen_eres_init(&bires);
	cgen_eres_init(&sres);

	/* Evaluate and promote base expression */
	rc = cgen_expr_promoted_rvalue(cgexpr, eusign->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &bres, &bires, &conv);
	if (rc != EOK)
		goto error;

	ctok = (comp_tok_t *) eusign->tsign.data;

	if (bires.cgtype->ntype != cgn_basic) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic = (cgtype_basic_t *)bires.cgtype->ext;
	bits = cgen_basic_type_bits(cgexpr->cgen, tbasic);
	if (bits == 0) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen, tbasic);

	if (eusign->usign == aus_minus) {
		if (conv)
			cgen_warn_arith_enum(cgexpr->cgen, &eusign->tsign);

		/* Warn if truth value involved in unary minus */
		if (cgen_type_is_logic(cgexpr->cgen, bires.cgtype))
			cgen_warn_arith_truth(cgexpr->cgen, &eusign->tsign);

		/* neg %<dest>, %<bires> */

		rc = ir_instr_create(&instr);
		if (rc != EOK)
			goto error;

		rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
		if (rc != EOK)
			goto error;

		rc = ir_oper_var_create(bires.varname, &barg);
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
		/* Salvage the type from bires */
		eres->cgtype = bires.cgtype;
		bires.cgtype = NULL;

		if (bires.cvknown) {
			eres->cvknown = true;
			cgen_cvint_neg(cgexpr->cgen, is_signed, bits, bires.cvint,
			    &eres->cvint, &overflow);
			if (overflow)
				cgen_warn_integer_overflow(cgexpr->cgen,
				    &eusign->tsign);
		}
	} else {
		/* Unary plus */
		sres.varname = bires.varname;
		sres.valtype = cgen_rvalue;
		/* Salvage the type from bires */
		sres.cgtype = bires.cgtype;
		bires.cgtype = NULL;
		sres.cvknown = bires.cvknown;
		sres.cvint = bires.cvint;

		/* Convert result back to original enum type, if possible */
		rc = cgen_int2enum(cgexpr, &sres, bres.cgtype, eres);
		if (rc != EOK)
			return rc;
	}

	cgen_eres_fini(&bres);
	cgen_eres_fini(&bires);
	cgen_eres_fini(&sres);

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (barg != NULL)
		ir_oper_destroy(&barg->oper);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&bires);
	cgen_eres_fini(&sres);
	return rc;
}

/** Generate code for logical not expression.
 *
 * @param cgexpr Code generator for expression
 * @param elnot AST logical not expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_elnot(cgen_expr_t *cgexpr, ast_elnot_t *elnot,
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
	cgen_eres_t bres;
	int rc;

	cgen_eres_init(&bres);

	lblno = cgen_new_label_num(cgexpr->cgproc);

	rc = cgen_create_label(cgexpr->cgproc, "false_lnot", lblno, &flabel);
	if (rc != EOK)
		goto error;

	rc = cgen_create_label(cgexpr->cgproc, "end_lnot", lblno, &elabel);
	if (rc != EOK)
		goto error;

	rc = cgtype_basic_create(cgelm_logic, &btype);
	if (rc != EOK)
		goto error;

	/* Evaluate base expression */

	rc = cgen_expr_rvalue(cgexpr, elnot->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Jump to false_lnot if base expression is not zero */

	rc = cgen_truth_eres_cjmp(cgexpr, ast_tree_first_tok(elnot->bexpr),
	    &bres, true, flabel, lblock);
	if (rc != EOK)
		goto error;

	/* imm.16 %<dest>, 1 */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(1, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_imm;
	instr->width = cgexpr->cgen->arith_width;
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
	instr->width = cgexpr->cgen->arith_width;
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

	if (bres.cvknown) {
		eres->cvknown = true;
		if (cgen_eres_is_true(cgexpr->cgen, &bres))
			eres->cvint = 0;
		else
			eres->cvint = 1;
	}

	cgen_eres_fini(&bres);

	free(flabel);
	free(elabel);
	return EOK;
error:
	cgen_eres_fini(&bres);

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
 * @param cgexpr Code generator for expression
 * @param ebinop AST bitwise NOT expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_ebnot(cgen_expr_t *cgexpr, ast_ebnot_t *ebnot,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *barg = NULL;
	bool conv;
	cgen_eres_t bres;
	cgen_eres_t bires;
	bool is_signed;
	unsigned bits;
	cgtype_t *cgtype;
	cgtype_basic_t *tbasic;
	int rc;

	cgen_eres_init(&bres);
	cgen_eres_init(&bires);

	rc = cgen_expr_promoted_rvalue(cgexpr, ebnot->bexpr, lblock, &bres);
	if (rc != EOK)
		goto error;

	/* Convert enum to int if needed */
	rc = cgen_enum2int(cgexpr->cgen, &bres, &bires, &conv);
	if (rc != EOK)
		goto error;

	/* Check the type */
	if (bires.cgtype->ntype != cgn_basic) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	tbasic = (cgtype_basic_t *)bires.cgtype->ext;
	bits = cgen_basic_type_bits(cgexpr->cgen, tbasic);
	if (bits == 0) {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	is_signed = cgen_basic_type_signed(cgexpr->cgen, tbasic);

	/* Signed integer (not enum) operand */
	if (is_signed && !conv && !bires.cvknown)
		cgen_warn_bitop_signed(cgexpr->cgen, &ebnot->tbnot);
	/* Warn if truth value involved in bitwise negation */
	if (cgen_type_is_logic(cgexpr->cgen, bires.cgtype))
		cgen_warn_arith_truth(cgexpr->cgen, &ebnot->tbnot);
	/* Negative constant operand */
	if (bires.cvknown && cgen_cvint_is_negative(cgexpr->cgen, is_signed,
	    bires.cvint)) {
		cgen_warn_bitop_negative(cgexpr->cgen, &ebnot->tbnot);
	}

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(bires.varname, &barg);
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
	cgen_eres_fini(&bires);

	eres->varname = dest->varname;
	eres->valtype = cgen_rvalue;
	eres->cgtype = cgtype;

	if (bires.cvknown) {
		eres->cvknown = true;
		cgen_cvint_mask(cgexpr->cgen, is_signed, bits,
		    ~bires.cvint, &eres->cvint);
	}

	return EOK;
error:
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (barg != NULL)
		ir_oper_destroy(&barg->oper);
	cgen_eres_fini(&bres);
	cgen_eres_fini(&bires);
	return rc;
}

/** Generate code for preadjustment expression.
 *
 * @param cgexpr Code generator for expression
 * @param epreadj AST preadjustment expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_epreadj(cgen_expr_t *cgexpr, ast_epreadj_t *epreadj,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
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
	rc = cgen_expr_lvalue(cgexpr, epreadj->bexpr, lblock, &baddr);
	if (rc != EOK)
		goto error;

	/* Get the value */
	rc = cgen_eres_rvalue(cgexpr, &baddr, lblock, &bval);
	if (rc != EOK)
		goto error;

	/* Adjustment value */
	rc = cgen_const_int(cgexpr->cgproc, cgelm_char, 1, lblock, &adj);
	if (rc != EOK)
		goto error;

	if (epreadj->adj == aat_inc) {
		/* Add the two operands */
		rc = cgen_add(cgexpr, &epreadj->tadj, &bval, &adj, lblock,
		    &ares);
		if (rc != EOK)
			goto error;
	} else {
		/* Subtract the two operands */
		rc = cgen_sub(cgexpr, &epreadj->tadj, &bval, &adj, lblock,
		    &ares);
		if (rc != EOK)
			goto error;
	}

	/* Store the updated value */
	rc = cgen_store(cgexpr->cgproc, &baddr, &ares, lblock);
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
 * @param cgexpr Code generator for expression
 * @param epostadj AST postadjustment expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_epostadj(cgen_expr_t *cgexpr, ast_epostadj_t *epostadj,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
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
	rc = cgen_expr_lvalue(cgexpr, epostadj->bexpr, lblock, &baddr);
	if (rc != EOK)
		goto error;

	/* Get the value */
	rc = cgen_eres_rvalue(cgexpr, &baddr, lblock, &bval);
	if (rc != EOK)
		goto error;

	/* Adjustment value */
	rc = cgen_const_int(cgexpr->cgproc, cgelm_char, 1, lblock, &adj);
	if (rc != EOK)
		goto error;

	if (epostadj->adj == aat_inc) {
		/* Add the two operands */
		rc = cgen_add(cgexpr, &epostadj->tadj, &bval, &adj, lblock,
		    &ares);
		if (rc != EOK)
			goto error;
	} else {
		/* Subtract the two operands */
		rc = cgen_sub(cgexpr, &epostadj->tadj, &bval, &adj, lblock,
		    &ares);
		if (rc != EOK)
			goto error;
	}

	/* Store the updated value */
	rc = cgen_store(cgexpr->cgproc, &baddr, &ares, lblock);
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

/** Check type is __va_list or pointer to va_list.
 *
 * @param cgproc Code generator for procedure
 * @param cgtype Expression type
 * @param atok Token for diagnostics
 * @return EOK on success or an error code
 */
static int cgen_check_va_list(cgen_proc_t *cgproc, cgtype_t *cgtype,
    ast_tok_t *atok)
{
	cgtype_basic_t *tbasic;
	cgtype_pointer_t *tpointer;
	comp_tok_t *tok;

	(void)cgproc;
	tok = (comp_tok_t *)atok->data;

	/* If it is a pointer, look at its target. */
	if (cgtype->ntype == cgn_pointer) {
		tpointer = (cgtype_pointer_t *)cgtype->ext;
		cgtype = tpointer->tgtype;
	}

	if (cgtype->ntype == cgn_basic) {
		tbasic = (cgtype_basic_t *)cgtype->ext;
		if (tbasic->elmtype == cgelm_va_list)
			return EOK;
	}

	lexer_dprint_tok(&tok->tok, stderr);
	fprintf(stderr, ": expected expression of type __va_list, got ");
	cgtype_print(cgtype, stderr);
	fprintf(stderr, ".\n");
	cgproc->cgen->error = true; // TODO
	return EINVAL;
}

/** Generate code for __va_arg expression.
 *
 * @param cgexpr Code generator for expression
 * @param eva_arg AST __va_arg expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eva_arg(cgen_expr_t *cgexpr, ast_eva_arg_t *eva_arg,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t apres;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	ir_oper_imm_t *imm = NULL;
	cgtype_t *stype = NULL;
	cgtype_t *etype = NULL;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	ast_sclass_type_t sctype;
	cgen_rd_flags_t flags;
	size_t sz;
	int rc;

	cgen_eres_init(&apres);

	/* Evaluate ap expression */
	rc = cgen_expr(cgexpr, eva_arg->apexpr, lblock, &apres);
	if (rc != EOK)
		goto error;

	/* Check that ap is of type __va_list */
	rc = cgen_check_va_list(cgexpr->cgproc, apres.cgtype,
	    ast_tree_first_tok(eva_arg->apexpr));
	if (rc != EOK)
		goto error;

	/* Declaration specifiers */
	rc = cgen_dspecs(cgexpr->cgen, eva_arg->atypename->dspecs,
	    &sctype, &flags, &stype);
	if (rc != EOK)
		goto error;

	atok = ast_tree_first_tok(&eva_arg->atypename->node);
	ctok = (comp_tok_t *) atok->data;

	if ((flags & cgrd_def) != 0) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Struct/union/enum definition "
		    "inside __va_arg().\n");
		++cgexpr->cgen->warnings;
	}

	if (sctype != asc_none) {
		atok = ast_tree_first_tok(&eva_arg->atypename->node);
		ctok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented storage class specifier.\n");
		cgexpr->cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	/* Declarator */
	rc = cgen_decl(cgexpr->cgen, stype, eva_arg->atypename->decl, NULL,
	    &etype);
	if (rc != EOK)
		goto error;

	sz = cgen_type_sizeof(cgexpr->cgen, etype);

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(apres.varname, &var);
	if (rc != EOK)
		goto error;

	rc = ir_oper_imm_create(sz, &imm);
	if (rc != EOK)
		goto error;

	instr->itype = iri_vaarg;
	instr->width = cgen_pointer_bits;
	instr->dest = &dest->oper;
	instr->op1 = &var->oper;
	instr->op2 = &imm->oper;

	ir_lblock_append(lblock, NULL, instr);

	eres->varname = dest->varname;
	eres->valtype = cgen_lvalue;
	eres->cgtype = NULL;
	eres->cvknown = false;
	eres->cvsymbol = NULL;
	eres->cvint = 0;
	eres->cgtype = etype;

	dest = NULL;
	var = NULL;
	imm = NULL;

	cgtype_destroy(stype);
	cgen_eres_fini(&apres);

	return EOK;
error:
	cgtype_destroy(stype);
	cgtype_destroy(etype);
	cgen_eres_fini(&apres);
	ir_instr_destroy(instr);
	if (dest != NULL)
		ir_oper_destroy(&dest->oper);
	if (var != NULL)
		ir_oper_destroy(&var->oper);
	if (imm != NULL)
		ir_oper_destroy(&imm->oper);
	return rc;
}

/** Generate code for expression.
 *
 * @param cgexpr Code generator for expression
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr(cgen_expr_t *cgexpr, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	(void) lblock;
	eres->tfirst = ast_tree_first_tok(expr);
	eres->tlast = ast_tree_last_tok(expr);

	switch (expr->ntype) {
	case ant_eint:
		rc = cgen_eint(cgexpr, (ast_eint_t *) expr->ext, lblock, eres);
		break;
	case ant_echar:
		rc = cgen_echar(cgexpr, (ast_echar_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_estring:
		rc = cgen_estring(cgexpr, (ast_estring_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eident:
		rc = cgen_eident(cgexpr, (ast_eident_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eparen:
		rc = cgen_eparen(cgexpr, (ast_eparen_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_econcat:
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_ebinop:
		rc = cgen_ebinop(cgexpr, (ast_ebinop_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_etcond:
		rc = cgen_etcond(cgexpr, (ast_etcond_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ecomma:
		rc = cgen_ecomma(cgexpr, (ast_ecomma_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ecall:
		rc = cgen_ecall(cgexpr, (ast_ecall_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eindex:
		rc = cgen_eindex(cgexpr, (ast_eindex_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ederef:
		rc = cgen_ederef(cgexpr, (ast_ederef_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eaddr:
		rc = cgen_eaddr(cgexpr, (ast_eaddr_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_esizeof:
		rc = cgen_esizeof(cgexpr, (ast_esizeof_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ecast:
		rc = cgen_ecast(cgexpr, (ast_ecast_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ecliteral:
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		break;
	case ant_emember:
		rc = cgen_emember(cgexpr, (ast_emember_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eindmember:
		rc = cgen_eindmember(cgexpr, (ast_eindmember_t *) expr->ext,
		    lblock, eres);
		break;
	case ant_eusign:
		rc = cgen_eusign(cgexpr, (ast_eusign_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_elnot:
		rc = cgen_elnot(cgexpr, (ast_elnot_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_ebnot:
		rc = cgen_ebnot(cgexpr, (ast_ebnot_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_epreadj:
		rc = cgen_epreadj(cgexpr, (ast_epreadj_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_epostadj:
		rc = cgen_epostadj(cgexpr, (ast_epostadj_t *) expr->ext, lblock,
		    eres);
		break;
	case ant_eva_arg:
		rc = cgen_eva_arg(cgexpr, (ast_eva_arg_t *) expr->ext, lblock,
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
 * @param cgexpr Code generator for expression
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr_lvalue(cgen_expr_t *cgexpr, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	rc = cgen_expr(cgexpr, expr, lblock, eres);
	if (rc != EOK)
		return rc;

	if (eres->valtype != cgen_lvalue) {
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr); // XXX Print range
		fprintf(stderr, ": Lvalue required.\n");
		cgexpr->cgen->error = true;
		return EINVAL;
	}

	return EOK;
}

/** Generate code for expression, producing an rvalue.
 *
 * If the result of expression is an lvalue, read it to produce an rvalue.
 *
 * @param cgexpr Code generator for expression
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr_rvalue(cgen_expr_t *cgexpr, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t res;
	int rc;

	cgen_eres_init(&res);

	rc = cgen_expr(cgexpr, expr, lblock, &res);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvalue(cgexpr, &res, lblock, eres);
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
 * @param cgexpr Code generator for expression
 * @param res Original expression result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store rvalue expression result
 * @return EOK on success or an error code
 */
static int cgen_eres_rvalue(cgen_expr_t *cgexpr, cgen_eres_t *res,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	cgtype_t *cgtype;
	unsigned bits;
	int rc;

	/*
	 * If we already have an rvalue or we have a record or array type,
	 * which are always handled via a pointer, then we don't need
	 * to do anything.
	 */
	if (res->valtype == cgen_rvalue || res->cgtype->ntype == cgn_record) {
		rc = cgtype_clone(res->cgtype, &cgtype);
		if (rc != EOK)
			goto error;

		eres->varname = res->varname;
		eres->valtype = cgen_rvalue;
		eres->cgtype = cgtype;
		eres->valused = res->valused;
		eres->cvknown = res->cvknown;
		eres->cvint = res->cvint;
		eres->cvsymbol = res->cvsymbol;
		eres->tfirst = res->tfirst;
		eres->tlast = res->tlast;
		return EOK;
	}

	/*
	 * If it is an array, we need to convert it to a pointer.
	 */
	if (res->cgtype->ntype == cgn_array) {
		rc = cgen_array_to_ptr(cgexpr, res, eres);
		if (rc != EOK)
			goto error;

		return EOK;
	}

	/* Check the type */
	if (res->cgtype->ntype == cgn_basic) {
		bits = cgen_basic_type_bits(cgexpr->cgen,
		    (cgtype_basic_t *)res->cgtype->ext);
		if (bits == 0) {
			fprintf(stderr, "Unimplemented variable type.\n");
			cgexpr->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}
	} else if (res->cgtype->ntype == cgn_pointer) {
		bits = cgen_pointer_bits;
	} else if (res->cgtype->ntype == cgn_enum) {
		bits = cgen_enum_bits;
	} else {
		fprintf(stderr, "Unimplemented variable type (%d).\n",
		    (int)res->cgtype->ntype);
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Reading variables is not allowed in constant expressions */
	if (cgexpr->cexpr) {
		cgen_error_expr_not_constant(cgexpr->cgen, res->tfirst);
		return EINVAL;
	}

	/* Need to read the value in */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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
	eres->cvknown = false;
	eres->tfirst = res->tfirst;
	eres->tlast = res->tlast;

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
 * @param cgexpr Code generator for expression
 * @param bres Base expression result
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_eres_promoted_rvalue(cgen_expr_t *cgexpr, cgen_eres_t *bres,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	// TODO
	return cgen_eres_rvalue(cgexpr, bres, lblock, eres);
}

/** Generate code for expression, producing a promoted rvalue.
 *
 * If the result of expression is an lvalue, read it to produce an rvalue.
 * If it is smaller than int (float), promote it.
 *
 * @param cgexpr Code generator for expression
 * @param expr AST expression
 * @param lblock IR labeled block to which the code should be appended
 * @param eres Place to store expression result
 * @return EOK on success or an error code
 */
static int cgen_expr_promoted_rvalue(cgen_expr_t *cgexpr, ast_node_t *expr,
    ir_lblock_t *lblock, cgen_eres_t *eres)
{
	cgen_eres_t bres;
	int rc;

	cgen_eres_init(&bres);

	rc = cgen_expr_rvalue(cgexpr, expr, lblock, &bres);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_promoted_rvalue(cgexpr, &bres, lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&bres);
	return EOK;
error:
	cgen_eres_fini(&bres);
	return rc;
}

/** Get result type of optional enum to integer conversion.
 *
 * If @a etype is an enum, return correspondin integer type.
 * If the enum is strict, set @a *converted to true.
 * Set it to false, if there was no
 * conversion (already an integer) or the enum was not strict.
 *
 * @param cgen Code generator
 * @param etype Expression type
 * @param rrtype Place to store pointer to result type
 * @param converted Place to store @c true iff a strict enum was
 *                  converted to int, @c false otherwise.
 */
static int cgen_enum2int_rtype(cgen_t *cgen, cgtype_t *etype,
    cgtype_t **rrtype, bool *converted)
{
	int rc;

	*converted = false;
	(void)cgen;

	if (etype->ntype == cgn_enum) {
		/* Return corresponding integer type */
		if (cgtype_is_strict_enum(etype))
			*converted = true;

		rc = cgtype_int_construct(true, cgir_int, rrtype);
		if (rc != EOK)
			return rc;
	} else {
		/* Return unchanged */
		rc = cgtype_clone(etype, rrtype);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Convert enum to integer if applicable.
 *
 * If @a res is an enum, convert it to integer. If the enum is strict,
 * set @a *converted to true. Set it to false, if there was no
 * conversion (already an integer) or the enum was not strict.
 *
 * @param cgen Code generator
 * @param res Expression result that is an int or enum
 * @param rres Place to store converted result
 * @param converted Place to store @c true iff a strict enum was
 *                  converted to int, @c false otherwise.
 */
static int cgen_enum2int(cgen_t *cgen, cgen_eres_t *res,
    cgen_eres_t *rres, bool *converted)
{
	cgtype_t *rtype = NULL;
	int rc;

	rc = cgen_enum2int_rtype(cgen, res->cgtype, &rtype, converted);
	if (rc != EOK)
		return rc;

	rres->varname = res->varname;
	rres->valtype = res->valtype;
	rres->cvknown = res->cvknown;
	rres->cvint = res->cvint;
	rres->cvsymbol = res->cvsymbol;
	rres->tfirst = res->tfirst;
	rres->tlast = res->tlast;
	rres->cgtype = rtype;

	return EOK;
}

/** Generate code for converting integer back to enum after arithmetic
 * operation.
 *
 * Arithmetic with enums works, in general, such that for allowed operations
 * we first convert enum inputs to integer types, perform the operation
 * with integers and then, if the result hasn't been extended beyond
 * the range of int, we can pronounce the result to be still the original
 * enum type (or, possibly, a promoted version thereof).
 *
 * @param cgexpr Code generator for expression
 * @param bres Result of arithmetic operation
 * @param etype Original enum type
 * @param eres Place to store result, possibly converted to enum
 * @return EOK on success or an error code
 */
static int cgen_int2enum(cgen_expr_t *cgexpr, cgen_eres_t *ares,
    cgtype_t *etype, cgen_eres_t *eres)
{
	cgtype_int_rank_t rank;
	bool is_signed;
	int rc;

	rank = cgtype_int_rank(ares->cgtype);
	is_signed = cgen_type_is_signed(cgexpr->cgen, ares->cgtype);

	/*
	 * If the number was extended beyond the range of 'int',
	 * we cannot pretend it is an enum anymore. Just return it
	 * as an integer type.
	 */
	if (rank > cgir_int || (rank == cgir_int && !is_signed)) {
		cgen_eres_clone(ares, eres);
		return EOK;
	}

	eres->varname = ares->varname;
	eres->valtype = ares->valtype;
	eres->cvknown = ares->cvknown;
	eres->cvint = ares->cvint;
	eres->cvsymbol = ares->cvsymbol;
	eres->tfirst = ares->tfirst;
	eres->tlast = ares->tlast;

	rc = cgtype_clone(etype, &eres->cgtype);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Get the type resulting from usual arithmetic conversion of operands
 * of the specified types.
 *
 * Usual arithmetic conversions are defined in 6.3.1.8 of the C99 standard.
 * The expressions are evaluated into rvalues, checked for being scalar type,
 * the 'larger' type is determined and the value of the smaller type
 * is converted, if needed. Values of type smaller than int (or float)
 * should be promoted.
 *
 * @param cgexpr Code generator for expression
 * @param type1 First argument type
 * @param type2 Second argument type
 * @param rrtype Place to store pointer to resulting type
 *
 * @return EOK on success or an error code
 */
static int cgen_uac_rtype(cgen_expr_t *cgexpr, cgtype_t *type1,
    cgtype_t *type2, cgtype_t **rrtype)
{
	cgtype_t *rtype = NULL;
	cgtype_t *itype1 = NULL;
	cgtype_t *itype2 = NULL;
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
	bool conv1;
	bool conv2;
	int rc;

	/*
	 * For each argument, if it is an enum, get corresponding integer
	 * type and set conv1/conv2.
	 */

	rc = cgen_enum2int_rtype(cgexpr->cgen, type1, &itype1, &conv1);
	if (rc != EOK)
		goto error;

	rc = cgen_enum2int_rtype(cgexpr->cgen, type2, &itype2, &conv2);
	if (rc != EOK)
		goto error;

	if (!cgen_type_is_integer(cgexpr->cgen, itype1) ||
	    !cgen_type_is_integer(cgexpr->cgen, itype2)) {
		fprintf(stderr, "Performing UAC on non-integral type(s) ");
		(void) cgtype_print(itype1, stderr);
		fprintf(stderr, ", ");
		(void) cgtype_print(itype2, stderr);
		fprintf(stderr, " (not implemented).\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bt1 = (cgtype_basic_t *)itype1->ext;
	bt2 = (cgtype_basic_t *)itype2->ext;

	/* Get rank, bits and signedness of both operands */

	rank1 = cgtype_int_rank(itype1);
	sign1 = cgen_type_is_signed(cgexpr->cgen, itype1);
	bits1 = cgen_basic_type_bits(cgexpr->cgen, bt1);

	rank2 = cgtype_int_rank(itype2);
	sign2 = cgen_type_is_signed(cgexpr->cgen, itype2);
	bits2 = cgen_basic_type_bits(cgexpr->cgen, bt2);

	/* Determine resulting rank */

	rrank = rank1 > rank2 ? rank1 : rank2;

	/* XXX Must be at least int (due to integer promotion) */
#if 0
	if (rrank < cgir_int)
		rrank = cgir_int;
#endif

	/* Determine resulting signedness */

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
	}

	/* Construct result type */

	rc = cgtype_int_construct(rsign, rrank, &rtype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(itype1);
	cgtype_destroy(itype2);

	*rrtype = rtype;
	return EOK;
error:
	cgtype_destroy(itype1);
	cgtype_destroy(itype2);
	cgtype_destroy(rtype);
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
 * @param cgexpr Code generator for expression
 * @param res1 First expression result
 * @param res2 Second expression result
 * @param lblock Labeled block to which to append code
 * @param eres1 Place to store result of first converted result
 * @param eres2 Place to store result of second converted result
 * @param flags Place to store flags
 *
 * @return EOK on success or an error code
 */
static int cgen_uac(cgen_expr_t *cgexpr, cgen_eres_t *res1,
    cgen_eres_t *res2, ir_lblock_t *lblock, cgen_eres_t *eres1,
    cgen_eres_t *eres2, cgen_uac_flags_t *flags)
{
	cgtype_t *rtype = NULL;
	cgen_eres_t ir1;
	cgen_eres_t ir2;
	cgen_eres_t pr1;
	cgen_eres_t pr2;
	bool sign1;
	bool sign2;
	bool const1;
	bool const2;
	bool rsign;
	cgtype_basic_t *bt1;
	cgtype_basic_t *bt2;
	cgtype_enum_t *et1;
	cgtype_enum_t *et2;
	bool conv1;
	bool conv2;
	bool neg1;
	bool neg2;
	int rc;

	*flags = cguac_none;

	cgen_eres_init(&ir1);
	cgen_eres_init(&ir2);
	cgen_eres_init(&pr1);
	cgen_eres_init(&pr2);

	rc = cgen_uac_rtype(cgexpr, res1->cgtype, res2->cgtype, &rtype);
	if (rc != EOK)
		goto error;

	rc = cgen_enum2int(cgexpr->cgen, res1, &ir1, &conv1);
	if (rc != EOK)
		goto error;

	rc = cgen_enum2int(cgexpr->cgen, res2, &ir2, &conv2);
	if (rc != EOK)
		goto error;

	if (!cgen_type_is_integer(cgexpr->cgen, ir1.cgtype) ||
	    !cgen_type_is_integer(cgexpr->cgen, ir2.cgtype)) {
		fprintf(stderr, "Performing UAC on non-integral type(s) ");
		(void) cgtype_print(ir1.cgtype, stderr);
		fprintf(stderr, ", ");
		(void) cgtype_print(ir2.cgtype, stderr);
		fprintf(stderr, " (not implemented).\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	bt1 = (cgtype_basic_t *)ir1.cgtype->ext;
	bt2 = (cgtype_basic_t *)ir2.cgtype->ext;

	/* Always warn for logic type in UAC */

	if (bt1->elmtype == cgelm_logic || bt2->elmtype == cgelm_logic)
		*flags |= cguac_truth;
	if (bt1->elmtype == cgelm_logic && bt2->elmtype != cgelm_logic)
		*flags |= cguac_truthmix;
	if (bt1->elmtype != cgelm_logic && bt2->elmtype == cgelm_logic)
		*flags |= cguac_truthmix;

	/* Get signedness, constantness and negative flag for both operands */

	sign1 = cgen_type_is_signed(cgexpr->cgen, ir1.cgtype);
	const1 = ir1.cvknown;
	neg1 = const1 && cgen_cvint_is_negative(cgexpr->cgen, sign1, ir1.cvint);

	sign2 = cgen_type_is_signed(cgexpr->cgen, ir2.cgtype);
	const2 = ir2.cvknown;
	neg2 = const2 && cgen_cvint_is_negative(cgexpr->cgen, sign2, ir2.cvint);

	/* Result type signedness */
	rsign = cgen_type_is_signed(cgexpr->cgen, rtype);

	/* One of the operands is signed (but not a constant) */
	if ((sign1 && !const1) || (sign2 && !const2))
		*flags |= cguac_signed;
	/* One of the operands is a negative constant */
	if (neg1 || neg2)
		*flags |= cguac_negative;
	/* First operand was negative and converted to unsigned */
	if (neg1 && rsign == false)
		*flags |= cguac_neg2u;
	/* Second operand was negative and converted to unsigned */
	if (neg2 && rsign == false)
		*flags |= cguac_neg2u;
	/* First operand non-constant, signed and converted to unsigned */
	if (!const1 && sign1 && rsign == false)
		*flags |= cguac_mix2u;
	/* Second operand non-constant, signed and converted to unsigned */
	if (!const2 && sign2 && rsign == false)
		*flags |= cguac_mix2u;

	/* Promote both operands (XXX not needed?) */

	rc = cgen_eres_promoted_rvalue(cgexpr, &ir1, lblock, &pr1);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_promoted_rvalue(cgexpr, &ir2, lblock, &pr2);
	if (rc != EOK)
		goto error;

	/* Convert the promoted arguments to the result type */

	rc = cgen_type_convert(cgexpr, NULL, &pr1, rtype, cgen_explicit, lblock,
	    eres1);
	if (rc != EOK)
		goto error;

	rc = cgen_type_convert(cgexpr, NULL, &pr2, rtype, cgen_explicit, lblock,
	    eres2);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&ir1);
	cgen_eres_fini(&ir2);
	cgen_eres_fini(&pr1);
	cgen_eres_fini(&pr2);
	cgtype_destroy(rtype);

	if (conv1 || conv2)
		*flags |= cguac_enum;
	if ((conv1 && !conv2) || (!conv1 && conv2))
		*flags |= cguac_enummix;
	if (conv1 && conv2) {
		assert(res1->cgtype->ntype == cgn_enum);
		assert(res2->cgtype->ntype == cgn_enum);
		et1 = (cgtype_enum_t *)res1->cgtype->ext;
		et2 = (cgtype_enum_t *)res2->cgtype->ext;
		if (et1->cgenum != et2->cgenum)
			*flags |= cguac_enuminc;
	}
	return EOK;
error:
	cgen_eres_fini(&ir1);
	cgen_eres_fini(&ir2);
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
 * @param cgexpr Code generator for pexression
 * @param expr1 First expression
 * @param expr2 Second expression
 * @param lblock Labeled block to which to append code
 * @param eres1 Place to store result of first expression
 * @param eres2 Place to store result of second expression
 * @param flags Place to store flags
 *
 * @return EOK on success or an error code
 */
static int cgen_expr2_uac(cgen_expr_t *cgexpr, ast_node_t *expr1,
    ast_node_t *expr2, ir_lblock_t *lblock, cgen_eres_t *eres1,
    cgen_eres_t *eres2, cgen_uac_flags_t *flags)
{
	cgen_eres_t res1;
	cgen_eres_t res2;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	rc = cgen_expr_rvalue(cgexpr, expr1, lblock, &res1);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgexpr, expr2, lblock, &res2);
	if (rc != EOK)
		goto error;

	rc = cgen_uac(cgexpr, &res1, &res2, lblock, eres1, eres2, flags);
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
 * @param cgexpr Code generator for expression
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
static int cgen_expr2lr_uac(cgen_expr_t *cgexpr, ast_node_t *expr1,
    ast_node_t *expr2, ir_lblock_t *lblock, cgen_eres_t *lres1,
    cgen_eres_t *eres1, cgen_eres_t *eres2, cgen_uac_flags_t *flags)
{
	cgen_eres_t res1;
	cgen_eres_t res2;
	int rc;

	cgen_eres_init(&res1);
	cgen_eres_init(&res2);

	rc = cgen_expr_lvalue(cgexpr, expr1, lblock, lres1);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_rvalue(cgexpr, lres1, lblock, &res1);
	if (rc != EOK)
		goto error;

	rc = cgen_expr_rvalue(cgexpr, expr2, lblock, &res2);
	if (rc != EOK)
		goto error;

	rc = cgen_uac(cgexpr, &res1, &res2, lblock, eres1, eres2, flags);
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
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param cres Place to store conversion result
 *
 * @return EOk or an error code
 */
static int cgen_type_convert_to_void(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_eres_t *cres)
{
	cgtype_t *cgtype;
	int rc;

	(void)cgexpr;
	(void)ctok;

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		return rc;

	cres->varname = NULL;
	cres->valtype = cgen_rvalue;
	cres->cgtype = cgtype;
	cres->valused = true;
	cres->tfirst = ares->tfirst;
	cres->tlast = ares->tlast;

	return EOK;
}

/** Convert expression result between two integer types.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_integer(cgen_expr_t *cgexpr, comp_tok_t *ctok,
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
	bool dest_signed;
	bool src_neg;
	bool dest_neg;
	int rc;

	assert(ares->cgtype->ntype == cgn_basic);
	srcw = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)ares->cgtype->ext);
	src_signed = cgen_basic_type_signed(cgexpr->cgen,
	    (cgtype_basic_t *)ares->cgtype->ext);

	assert(dtype->ntype == cgn_basic);
	destw = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)dtype->ext);
	dest_signed = cgen_basic_type_signed(cgexpr->cgen,
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
		cres->tfirst = ares->tfirst;
		cres->tlast = ares->tlast;

		/*
		 * For constant expression masking/sign extension may be
		 * needed when signedness is changed.
		 */
		if (ares->cvknown) {
			cres->cvknown = true;
			cgen_cvint_mask(cgexpr->cgen, dest_signed, destw,
			    ares->cvint, &cres->cvint);

			/* Test for sign change */
			src_neg = cgen_cvint_is_negative(cgexpr->cgen,
			    src_signed, ares->cvint);
			dest_neg = cgen_cvint_is_negative(cgexpr->cgen,
			    dest_signed, cres->cvint);
			if (expl != cgen_explicit && src_neg != dest_neg)
				cgen_warn_sign_changed(cgexpr->cgen, ctok);
		} else if (expl != cgen_explicit && src_signed != dest_signed) {
			cgen_warn_sign_convert(cgexpr->cgen,
			    ctok, ares, cres);
		}

		return EOK;
	}

	cgen_eres_init(&rres);

	rc = cgen_eres_rvalue(cgexpr, ares, lblock, &rres);
	if (rc != EOK)
		goto error;

	/* Generate trunc/sgnext/zrext instruction */

	if (destw < srcw) {
		/* Integer truncation */
		itype = iri_trunc;

		if (expl != cgen_explicit && !ares->cvknown) {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Conversion may loose "
			    "significant digits.\n");
			++cgexpr->cgen->warnings;
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

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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
	cres->tfirst = ares->tfirst;
	cres->tlast = ares->tlast;

	if (ares->cvknown) {
		cres->cvknown = true;
		cgen_cvint_mask(cgexpr->cgen, dest_signed, destw,
		    ares->cvint, &cres->cvint);
		if (expl != cgen_explicit && cres->cvint != ares->cvint)
			cgen_warn_number_changed(cgexpr->cgen, ctok);
	}

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
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_pointer(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgtype_pointer_t *ptrtype1;
	cgtype_pointer_t *ptrtype2;
	cgtype_t *cgtype;
	int rc;

	(void)lblock;

	assert(ares->cgtype->ntype == cgn_pointer);
	assert(dtype->ntype == cgn_pointer);

	ptrtype1 = (cgtype_pointer_t *)ares->cgtype->ext;
	ptrtype2 = (cgtype_pointer_t *)dtype->ext;

	if (!cgtype_ptr_compatible(ptrtype1, ptrtype2) &&
	    expl != cgen_explicit &&
	    !cgtype_is_void(ptrtype2->tgtype)) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Converting from ");
		(void) cgtype_print(ares->cgtype, stderr);
		fprintf(stderr, " to incompatible pointer type ");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, ".\n");
		++cgexpr->cgen->warnings;
	}

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		goto error;

	cres->varname = ares->varname;
	cres->valtype = ares->valtype;
	cres->cgtype = cgtype;
	cres->valused = ares->valused;
	cres->cvknown = ares->cvknown;
	cres->cvint = ares->cvint;
	cres->cvsymbol = ares->cvsymbol;
	cres->tfirst = ares->tfirst;
	cres->tlast = ares->tlast;
error:
	return rc;
}

/** Convert expression result between two record types.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_record(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgtype_record_t *rtype1;
	cgtype_record_t *rtype2;
	cgtype_t *cgtype;
	int rc;

	(void)lblock;

	assert(ares->cgtype->ntype == cgn_record);
	assert(dtype->ntype == cgn_record);

	rtype1 = (cgtype_record_t *)ares->cgtype->ext;
	rtype2 = (cgtype_record_t *)dtype->ext;

	if (rtype1->record != rtype2->record) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Converting from '");
		(void) cgtype_print(ares->cgtype, stderr);
		fprintf(stderr, "' to incompatible struct/union type '");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, "'.\n");
		cgexpr->cgen->error = true; // TODO
	}

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		goto error;

	cres->varname = ares->varname;
	cres->valtype = ares->valtype;
	cres->cgtype = cgtype;
	cres->valused = ares->valused;
	cres->cvknown = ares->cvknown;
	cres->cvint = ares->cvint;
	cres->cvsymbol = ares->cvsymbol;
	cres->tfirst = ares->tfirst;
	cres->tlast = ares->tlast;
error:
	return rc;
}

/** Convert expression result between two enum types.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_enum(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype,  cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgtype_enum_t *etype1;
	cgtype_enum_t *etype2;
	cgtype_t *cgtype;
	int rc;

	(void)lblock;

	assert(ares->cgtype->ntype == cgn_enum);
	assert(dtype->ntype == cgn_enum);

	etype1 = (cgtype_enum_t *)ares->cgtype->ext;
	etype2 = (cgtype_enum_t *)dtype->ext;

	if (etype1->cgenum != etype2->cgenum && expl != cgen_explicit) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Implicit conversion from '");
		(void) cgtype_print(ares->cgtype, stderr);
		fprintf(stderr, "' to different enum type '");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, "'.\n");
		++cgexpr->cgen->warnings;
	}

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		goto error;

	cres->varname = ares->varname;
	cres->valtype = ares->valtype;
	cres->cgtype = cgtype;
	cres->valused = ares->valused;
	cres->cvknown = ares->cvknown;
	cres->cvint = ares->cvint;
	cres->cvsymbol = ares->cvsymbol;
	cres->tfirst = ares->tfirst;
	cres->tlast = ares->tlast;
error:
	return rc;
}

/** Convert expression result from enum to a different type.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_from_enum(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype,  cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgen_eres_t ires;
	bool converted;
	int rc;

	cgen_eres_init(&ires);

	/* First drop to corresponding integer type */
	rc = cgen_enum2int(cgexpr->cgen, ares, &ires, &converted);
	if (rc != EOK)
		goto error;

	/* Conversion is implicit and enum is strict */
	if (expl != cgen_explicit && converted) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Implicit conversion from '");
		(void) cgtype_print(ares->cgtype, stderr);
		fprintf(stderr, "' to '");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, "'.\n");
		++cgexpr->cgen->warnings;
	}

	/* Now do the rest of the work */
	rc = cgen_type_convert(cgexpr, ctok, &ires, dtype, expl, lblock, cres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&ires);
	return EOK;
error:
	cgen_eres_fini(&ires);
	return rc;
}

/** Convert expression result to enum from a different type.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_to_enum(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype,  cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgen_eres_t ires;
	cgtype_t *cgtype = NULL;
	int rc;

	cgen_eres_init(&ires);

	/* Construct corresponding integer type */
	rc = cgtype_int_construct(true, cgir_int, &cgtype);
	if (rc != EOK)
		goto error;

	/* Convert to corresponding integer type */
	rc = cgen_type_convert(cgexpr, ctok, ares, cgtype, expl, lblock, &ires);
	if (rc != EOK)
		goto error;

	if (expl != cgen_explicit) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Implicit conversion from '");
		(void) cgtype_print(ares->cgtype, stderr);
		fprintf(stderr, "' to '");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, "'.\n");
		++cgexpr->cgen->warnings;
	}

	cres->varname = ires.varname;
	cres->valtype = ires.valtype;
	cres->cvknown = ares->cvknown;
	cres->cvint = ares->cvint;
	cres->cvsymbol = ares->cvsymbol;
	cres->tfirst = ares->tfirst;
	cres->tlast = ares->tlast;

	rc = cgtype_clone(dtype, &cres->cgtype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(cgtype);
	cgen_eres_fini(&ires);
	return EOK;
error:
	cgtype_destroy(cgtype);
	cgen_eres_fini(&ires);
	return rc;
}

/** Convert expression result from integer to pointer.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_int_ptr(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	unsigned bits;
	cgtype_t *cgtype;
	cgtype_basic_t *tbasic = NULL;
	cgen_eres_t icres;
	int rc;

	cgen_eres_init(&icres);

	assert(ares->cgtype->ntype == cgn_basic);
	assert(dtype->ntype == cgn_pointer);

	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)ares->cgtype->ext);

	rc = cgtype_clone(dtype, &cgtype);
	if (rc != EOK)
		goto error;

	if (expl != cgen_explicit) {
		if (ares->cvknown && ares->cvint == 0) {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Zero used as a null "
			    "pointer constant.\n");
			++cgexpr->cgen->warnings;
		} else {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Implicit conversion from "
			    "integer to pointer.\n");
			++cgexpr->cgen->warnings;
		}
	}

	if (bits != cgen_pointer_bits) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Converting to pointer from integer "
		    "of different size.\n");
		++cgexpr->cgen->warnings;

		rc = cgtype_basic_create(cgelm_uint, &tbasic);
		if (rc != EOK)
			goto error;

		rc = cgen_type_convert_integer(cgexpr, ctok, ares,
		    &tbasic->cgtype, expl, lblock, &icres);
		if (rc != EOK)
			goto error;

		cgtype_destroy(&tbasic->cgtype);
		tbasic = NULL;

		cres->varname = icres.varname;
		cres->valtype = icres.valtype;
		cres->cgtype = cgtype;
		cres->valused = icres.valused;
		cres->cvknown = icres.cvknown;
		cres->cvint = icres.cvint;
		cres->cvsymbol = icres.cvsymbol;
		cres->tfirst = icres.tfirst;
		cres->tlast = icres.tlast;

		cgen_eres_fini(&icres);
	} else {
		cres->varname = ares->varname;
		cres->valtype = ares->valtype;
		cres->cgtype = cgtype;
		cres->valused = ares->valused;
		cres->cvknown = ares->cvknown;
		cres->cvint = ares->cvint;
		cres->cvsymbol = ares->cvsymbol;
		cres->tfirst = ares->tfirst;
		cres->tlast = ares->tlast;
	}

	return EOK;
error:
	if (cgtype != NULL)
		cgtype_destroy(cgtype);
	if (tbasic != NULL)
		cgtype_destroy(&tbasic->cgtype);
	cgen_eres_fini(&icres);
	return rc;
}

/** Convert expression result from pointer to integer.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_ptr_int(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	unsigned bits;
	cgtype_t *cgtype = NULL;
	cgtype_basic_t *tbasic = NULL;
	cgen_eres_t icres;
	int rc;

	cgen_eres_init(&icres);

	assert(ares->cgtype->ntype == cgn_pointer);
	assert(dtype->ntype == cgn_basic);

	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)dtype->ext);

	if (expl != cgen_explicit) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Implicit conversion from "
		    "pointer to integer.\n");
		++cgexpr->cgen->warnings;
	}

	if (bits != cgen_pointer_bits) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Converting from pointer to integer "
		    "of different size.\n");
		++cgexpr->cgen->warnings;

		rc = cgtype_basic_create(cgelm_uint, &tbasic);
		if (rc != EOK)
			goto error;

		/* Pointer converted to unsigned int. */
		icres.varname = ares->varname;
		icres.valtype = ares->valtype;
		icres.cgtype = &tbasic->cgtype;
		icres.valused = ares->valused;
		icres.cvknown = ares->cvknown;
		icres.cvint = ares->cvint;
		icres.cvsymbol = ares->cvsymbol;
		icres.tfirst = ares->tfirst;
		icres.tlast = ares->tlast;

		rc = cgen_type_convert_integer(cgexpr, ctok, &icres,
		    &tbasic->cgtype, expl, lblock, cres);
		if (rc != EOK)
			goto error;

		cgen_eres_fini(&icres);
	} else {
		rc = cgtype_clone(dtype, &cgtype);
		if (rc != EOK)
			goto error;

		cres->varname = ares->varname;
		cres->valtype = ares->valtype;
		cres->cgtype = cgtype;
		cres->valused = ares->valused;
		cres->cvknown = ares->cvknown;
		cres->cvint = ares->cvint;
		cres->cvsymbol = ares->cvsymbol;
		cres->tfirst = ares->tfirst;
		cres->tlast = ares->tlast;
	}

	return EOK;
error:
	if (cgtype != NULL)
		cgtype_destroy(cgtype);
	if (tbasic != NULL)
		cgtype_destroy(&tbasic->cgtype);
	cgen_eres_fini(&icres);
	return rc;
}

/** Convert value expression result to the specified type.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_rval(cgen_expr_t *cgexpr, comp_tok_t *ctok,
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

	/* Source and destination types are pointers */
	if (ares->cgtype->ntype == cgn_pointer &&
	    dtype->ntype == cgn_pointer) {
		return cgen_type_convert_pointer(cgexpr, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	/* Source and destination types are enum types */
	if (ares->cgtype->ntype == cgn_enum &&
	    dtype->ntype == cgn_enum) {
		return cgen_type_convert_enum(cgexpr, ctok, ares, dtype, expl,
		    lblock, cres);
	}

	/* Source type is enum (but destination is not) */
	if (ares->cgtype->ntype == cgn_enum) {
		return cgen_type_convert_from_enum(cgexpr, ctok, ares, dtype, expl,
		    lblock, cres);
	}

	/* Destination type is enum (but source is not) */
	if (dtype->ntype == cgn_enum) {
		return cgen_type_convert_to_enum(cgexpr, ctok, ares, dtype, expl,
		    lblock, cres);
	}

	/* Source is an integer and destination is a pointer. */
	if (cgen_type_is_integer(cgexpr->cgen, ares->cgtype) &&
	    dtype->ntype == cgn_pointer) {
		return cgen_type_convert_int_ptr(cgexpr, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	/* Source is a pointer and destination is an integer. */
	if (ares->cgtype->ntype == cgn_pointer &&
	    cgen_type_is_integer(cgexpr->cgen, dtype)) {
		return cgen_type_convert_ptr_int(cgexpr, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	if (ares->cgtype->ntype == cgn_basic &&
	    ((cgtype_basic_t *)(ares->cgtype->ext))->elmtype == cgelm_logic &&
	    expl != cgen_explicit) {
		cgen_warn_truth_as_int(cgexpr->cgen, ctok);
	}

	/* Converting between two integer types */
	if (cgen_type_is_integer(cgexpr->cgen, ares->cgtype) &&
	    cgen_type_is_integer(cgexpr->cgen, dtype)) {
		return cgen_type_convert_integer(cgexpr, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	if (dtype->ntype != cgn_basic ||
	    ((cgtype_basic_t *)(dtype->ext))->elmtype != cgelm_int) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Converting to type ");
		(void) cgtype_print(dtype, stderr);
		fprintf(stderr, " which is different from int "
		    "(not implemented).\n");
		cgexpr->cgen->error = true; // TODO
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
		cgexpr->cgen->error = true; // TODO
		return EINVAL;
	}

	return cgen_eres_clone(ares, cres);
}

/** Convert array to pointer to the first element.
 *
 * @param cgexpr Code generator for expression
 * @param ares Argument (expression result)
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_array_to_ptr(cgen_expr_t *cgexpr, cgen_eres_t *ares,
    cgen_eres_t *cres)
{
	cgtype_t *etype = NULL;
	cgtype_pointer_t *ptrt = NULL;
	cgtype_array_t *arrt;
	int rc;

	(void)cgexpr;

	assert(ares->cgtype->ntype == cgn_array);
	arrt = (cgtype_array_t *)ares->cgtype->ext;

	rc = cgtype_clone(arrt->etype, &etype);
	if (rc != EOK)
		goto error;

	/*
	 * Create pointer type whose target is the array element type.
	 * Note that ownership of etype is transferred to ptrt.
	 */
	rc = cgtype_pointer_create(etype, &ptrt);
	if (rc != EOK)
		goto error;

	etype = NULL;

	/* Clone the result except... */
	rc = cgen_eres_clone(ares, cres);
	if (rc != EOK)
		goto error;

	/* Replace the type with the pointer type */
	cgtype_destroy(cres->cgtype);
	cres->cgtype = &ptrt->cgtype;
	ptrt = NULL;

	/*
	 * Make it an rvalue .. it's the value of the pointer, not the
	 * address.
	 */
	cres->valtype = cgen_rvalue;
	return EOK;
error:
	if (etype != NULL)
		cgtype_destroy(etype);
	if (ptrt != NULL)
		cgtype_destroy(&ptrt->cgtype);
	return rc;
}

/** Convert array to the specified type.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_array(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgen_eres_t pres;
	int rc;

	assert(ares->cgtype->ntype == cgn_array);

	cgen_eres_init(&pres);

	/* Convert array to pointer */
	rc = cgen_array_to_ptr(cgexpr, ares, &pres);
	if (rc != EOK)
		goto error;

	/* Continue conversion */
	rc = cgen_type_convert(cgexpr, ctok, &pres, dtype, expl, lblock,
	    cres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&pres);
error:
	cgen_eres_fini(&pres);
	return rc;
}

/** Convert va_list to the specified type.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_va_list(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgen_eres_t pres;
	cgtype_t *ltype = NULL;
	cgtype_pointer_t *ptrt = NULL;
	cgtype_basic_t *basict;
	int rc;

	assert(ares->cgtype->ntype == cgn_basic);
	basict = (cgtype_basic_t *)ares->cgtype->ext;
	assert(basict->elmtype == cgelm_va_list);

	cgen_eres_init(&pres);

	rc = cgtype_clone(ares->cgtype, &ltype);
	if (rc != EOK)
		goto error;

	/*
	 * Create pointer type whose target is the array element type.
	 * Note that ownership of etype is transferred to ptrt.
	 */
	rc = cgtype_pointer_create(ltype, &ptrt);
	if (rc != EOK)
		goto error;

	ltype = NULL;

	/* Clone the result except.. */
	rc = cgen_eres_clone(ares, &pres);
	if (rc != EOK)
		goto error;

	/* Replace the type with the pointer type */
	cgtype_destroy(pres.cgtype);
	pres.cgtype = &ptrt->cgtype;
	ptrt = NULL;

	/*
	 * Make it an rvalue .. it's the value of the pointer, not the
	 * address.
	 */
	pres.valtype = cgen_rvalue;

	/* Continue conversion */
	rc = cgen_type_convert(cgexpr, ctok, &pres, dtype, expl, lblock,
	    cres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&pres);
error:
	if (ltype != NULL)
		cgtype_destroy(ltype);
	if (ptrt != NULL)
		cgtype_destroy(&ptrt->cgtype);
	cgen_eres_fini(&pres);
	return rc;
}

/** Convert function to the specified type.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert_func(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgen_eres_t pres;
	cgtype_t *ftype = NULL;
	cgtype_pointer_t *ptrt = NULL;
	int rc;

	assert(ares->cgtype->ntype == cgn_func);

	cgen_eres_init(&pres);

	rc = cgtype_clone(ares->cgtype, &ftype);
	if (rc != EOK)
		goto error;

	/*
	 * Create pointer type whose target is the function type.
	 * Note that ownership of ftype is transferred to ptrt.
	 */
	rc = cgtype_pointer_create(ftype, &ptrt);
	if (rc != EOK)
		goto error;

	ftype = NULL;

	/* Clone the result except.. */
	rc = cgen_eres_clone(ares, &pres);
	if (rc != EOK)
		goto error;

	/* Replace the type with the pointer type */
	cgtype_destroy(pres.cgtype);
	pres.cgtype = &ptrt->cgtype;
	ptrt = NULL;

	/*
	 * Make it an rvalue .. it's the value of the pointer, not the
	 * address.
	 */
	pres.valtype = cgen_rvalue;

	/* Continue conversion */
	rc = cgen_type_convert(cgexpr, ctok, &pres, dtype, expl, lblock,
	    cres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&pres);
error:
	if (ftype != NULL)
		cgtype_destroy(ftype);
	if (ptrt != NULL)
		cgtype_destroy(&ptrt->cgtype);
	cgen_eres_fini(&pres);
	return rc;
}

/** Convert expression result to the specified type.
 *
 * @param cgexpr Code generator for expression
 * @param ctok Conversion token - only used to print diagnostics
 * @param ares Argument (expression result)
 * @param dtype Destination type
 * @param expl Explicit (@c cgen_explicit) or implicit (@c cgen_implicit)
 *             type conversion
 * @param lblock IR labeled block to which the code should be appended
 * @param cres Place to store conversion result
 *
 * @return EOK or an error code
 */
static int cgen_type_convert(cgen_expr_t *cgexpr, comp_tok_t *ctok,
    cgen_eres_t *ares, cgtype_t *dtype, cgen_expl_t expl, ir_lblock_t *lblock,
    cgen_eres_t *cres)
{
	cgen_eres_t rres;
	int rc;

	cgen_eres_init(&rres);

	/* Destination type is void */
	if (cgtype_is_void(dtype)) {
		return cgen_type_convert_to_void(cgexpr, ctok, ares,
		    dtype, cres);
	}

	/* Source type is an array */
	if (ares->cgtype->ntype == cgn_array) {
		return cgen_type_convert_array(cgexpr, ctok, ares, dtype, expl,
		    lblock, cres);
	}

	/* Source type is a va_list */
	if (ares->cgtype->ntype == cgn_basic &&
	    ((cgtype_basic_t *)ares->cgtype->ext)->elmtype == cgelm_va_list) {
		return cgen_type_convert_va_list(cgexpr, ctok, ares, dtype,
		    expl, lblock, cres);
	}

	/* Source type is a function */
	if (ares->cgtype->ntype == cgn_func) {
		return cgen_type_convert_func(cgexpr, ctok, ares, dtype, expl,
		    lblock, cres);
	}

	/* Destination type is an array */
	if (dtype->ntype == cgn_array) {
		assert(expl == cgen_explicit);
		cgen_error_cast_array(cgexpr->cgen, ctok);
		return EINVAL;
	}

	/* Source and destination types are record types */
	if (ares->cgtype->ntype == cgn_record &&
	    dtype->ntype == cgn_record) {
		return cgen_type_convert_record(cgexpr, ctok, ares, dtype,
		    lblock, cres);
	}

	rc = cgen_eres_rvalue(cgexpr, ares, lblock, &rres);
	if (rc != EOK)
		goto error;

	rc = cgen_type_convert_rval(cgexpr, ctok, &rres, dtype, expl, lblock,
	    cres);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&rres);
	return EOK;
error:
	cgen_eres_fini(&rres);
	return rc;
}

/** Generate code for a truth expression result test / conditional jump.
 *
 * Evaluate truth expression, then jump if it is true/non-zero (@a cval == true),
 * or false/non-zero (@a cval == false), respectively.
 *
 * @param cgexpr Code generator for expression
 * @param atok First token of condition expression for printing diagnostics
 * @param cres Result of evaluating condition expression
 * @param cval Condition value when jump is taken
 * @param dlabel Jump destination label
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_truth_eres_cjmp(cgen_expr_t *cgexpr, ast_tok_t *atok,
    cgen_eres_t *cres, bool cval, const char *dlabel, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *carg = NULL;
	ir_oper_var_t *larg = NULL;
	comp_tok_t *tok;
	cgtype_basic_t *btype;
	int rc;

	/* Check the type */

	switch (cres->cgtype->ntype) {
	case cgn_basic:
		btype = (cgtype_basic_t *)cres->cgtype->ext;
		switch (btype->elmtype) {
		case cgelm_void:
			cgen_error_use_void_value(cgexpr->cgen, atok);
			return EINVAL;
		case cgelm_char:
		case cgelm_uchar:
		case cgelm_short:
		case cgelm_ushort:
		case cgelm_int:
		case cgelm_uint:
		case cgelm_long:
		case cgelm_ulong:
		case cgelm_longlong:
		case cgelm_ulonglong:
			break;
		case cgelm_logic:
			break;
		case cgelm_va_list:
			cgen_error_need_scalar(cgexpr->cgen, atok);
			return EINVAL;
		}
		break;
	case cgn_enum:
		cgen_warn_logic_enum(cgexpr->cgen, atok);
		break;
	case cgn_func:
		// XXX TODO
		assert(false);
		break;
	case cgn_pointer:
		break;
	case cgn_record:
	case cgn_array:
		cgen_error_need_scalar(cgexpr->cgen, atok);
		return EINVAL;
	}

	if (cres->cgtype->ntype != cgn_basic ||
	    ((cgtype_basic_t *)cres->cgtype->ext)->elmtype != cgelm_logic) {
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: '");
		cgtype_print(cres->cgtype, stderr);
		fprintf(stderr, "' used as a truth value.\n");
		++cgexpr->cgen->warnings;
	}

	/* j[n]z %<cres>, %dlabel */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cres->varname, &carg);
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

	return EOK;
error:
	if (carg != NULL)
		ir_oper_destroy(&carg->oper);
	if (instr != NULL)
		ir_instr_destroy(instr);
	return rc;
}

/** Generate code for a truth expression test / conditional jump.
 *
 * Evaluate truth expression, then jump if it is true/non-zero (@a cval == true),
 * or false/non-zero (@a cval == false), respectively.
 *
 * @param cgexpr Code generator for expression
 * @param aexpr Truth expression
 * @param cval Condition value when jump is taken
 * @param dlabel Jump destination label
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_truth_expr_cjmp(cgen_expr_t *cgexpr, ast_node_t *aexpr,
    bool cval, const char *dlabel, ir_lblock_t *lblock)
{
	cgen_eres_t cres;
	int rc;

	cgen_eres_init(&cres);

	/* Condition */

	rc = cgen_expr_rvalue(cgexpr, aexpr, lblock, &cres);
	if (rc != EOK)
		goto error;

	rc = cgen_truth_eres_cjmp(cgexpr, ast_tree_first_tok(aexpr),
	    &cres, cval, dlabel, lblock);
	if (rc != EOK)
		goto error;

	cgen_eres_fini(&cres);
	return EOK;
error:
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

		rc = cgen_expr(&cgproc->cgexpr, areturn->arg, lblock, &ares);
		if (rc != EOK)
			goto error;
	}

	/* Only if we have an argument and return type is not void */
	if (areturn->arg != NULL && !cgtype_is_void(cgproc->rtype)) {
		atok = ast_tree_first_tok(areturn->arg);
		ctok = (comp_tok_t *) atok->data;

		/* Convert to the return type */
		rc = cgen_type_convert(&cgproc->cgexpr, ctok, &ares,
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
		} else if (cgproc->rtype->ntype == cgn_enum) {
			bits = cgen_enum_bits;
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

	rc = cgen_truth_expr_cjmp(&cgproc->cgexpr, aif->cond, false, fiflabel, lblock);
	if (rc != EOK)
		goto error;

	/* True branch */

	rc = parser_process_block(cgproc->cgen->parser, &aif->tbranch);
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

	rc = parser_process_if_elseif(cgproc->cgen->parser, aif);
	if (rc != EOK && rc != ENOENT)
		goto error;

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

		rc = cgen_truth_expr_cjmp(&cgproc->cgexpr, elsif->cond, false,
		    fiflabel, lblock);
		if (rc != EOK)
			goto error;

		/* Else-if branch */
		rc = parser_process_block(cgproc->cgen->parser,
		    &elsif->ebranch);
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

		rc = parser_process_if_elseif(cgproc->cgen->parser, aif);
		if (rc != EOK && rc != ENOENT)
			goto error;

		elsif = ast_if_next(elsif);
	}

	/* False branch */

	rc = parser_process_if_else(cgproc->cgen->parser, aif);
	if (rc != EOK && rc != ENOENT) {
		goto error;
	}

	if (rc != ENOENT) {
		rc = parser_process_block(cgproc->cgen->parser, &aif->fbranch);
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
	cgen_loop_switch_t *old_lswitch = cgproc->cur_loop_switch;
	cgen_loop_t *old_loop = cgproc->cur_loop;
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

	rc = cgen_truth_expr_cjmp(&cgproc->cgexpr, awhile->cond, false, ewlabel, lblock);
	if (rc != EOK)
		goto error;

	/* Body */

	rc = parser_process_block(cgproc->cgen->parser, &awhile->body);
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
	cgproc->cur_loop_switch = old_lswitch;
	cgproc->cur_loop = old_loop;
	return EOK;
error:
	cgproc->cur_loop_switch = old_lswitch;
	cgproc->cur_loop = old_loop;

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
	cgen_loop_switch_t *old_lswitch = cgproc->cur_loop_switch;
	cgen_loop_t *old_loop = cgproc->cur_loop;
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

	rc = parser_process_block(cgproc->cgen->parser, &ado->body);
	if (rc != EOK)
		goto error;

	/* label next_do */

	ir_lblock_append(lblock, ndlabel, NULL);

	rc = parser_process_do_while(cgproc->cgen->parser, ado);
	if (rc != EOK)
		goto error;

	/* Jump to %do if condition is true */

	rc = cgen_truth_expr_cjmp(&cgproc->cgexpr, ado->cond, true, dlabel, lblock);
	if (rc != EOK)
		goto error;

	ir_lblock_append(lblock, edlabel, NULL);

	cgen_loop_switch_destroy(lswitch);
	cgen_loop_destroy(loop);

	free(dlabel);
	free(ndlabel);
	free(edlabel);
	cgen_eres_fini(&cres);
	cgproc->cur_loop_switch = old_lswitch;
	cgproc->cur_loop = old_loop;
	return EOK;
error:
	cgproc->cur_loop_switch = old_lswitch;
	cgproc->cur_loop = old_loop;

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
	cgen_loop_switch_t *old_lswitch = cgproc->cur_loop_switch;
	cgen_loop_t *old_loop = cgproc->cur_loop;
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
		rc = cgen_expr_rvalue(&cgproc->cgexpr, afor->linit, lblock, &ires);
		if (rc != EOK)
			goto error;

		cgen_expr_check_unused(&cgproc->cgexpr, afor->linit, &ires);
	}

	ir_lblock_append(lblock, flabel, NULL);

	/* Condition */

	if (afor->lcond != NULL) {
		/* Jump to %end_for if condition is false */

		rc = cgen_truth_expr_cjmp(&cgproc->cgexpr, afor->lcond, false,
		    eflabel, lblock);
		if (rc != EOK)
			goto error;
	}

	/* Body */

	rc = parser_process_block(cgproc->cgen->parser, &afor->body);
	if (rc != EOK)
		goto error;

	/* Loop iteration */

	ir_lblock_append(lblock, nflabel, NULL);

	if (afor->lnext != NULL) {
		/* Evaluate and ignore next iteration expression */
		rc = cgen_expr_rvalue(&cgproc->cgexpr, afor->lnext, lblock,
		    &nres);
		if (rc != EOK)
			goto error;

		cgen_expr_check_unused(&cgproc->cgexpr, afor->lnext, &nres);
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
	cgproc->cur_loop_switch = old_lswitch;
	cgproc->cur_loop = old_loop;
	return EOK;
error:
	cgproc->cur_loop_switch = old_lswitch;
	cgproc->cur_loop = old_loop;

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
	cgen_loop_switch_t *old_lswitch = cgproc->cur_loop_switch;
	cgtype_enum_t *tenum;
	cgen_enum_elem_t *elem;
	cgen_switch_value_t *value;
	ast_tok_t *atok;
	comp_tok_t *tok;
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

	rc = cgen_expr_rvalue(&cgproc->cgexpr, aswitch->sexpr, lblock, &eres);
	if (rc != EOK)
		goto error;

	/* Expression must be integer or enum */
	if (!cgen_type_is_integer(cgproc->cgen, eres.cgtype) &&
	    eres.cgtype->ntype != cgn_enum) {
		atok = ast_tree_first_tok(aswitch->sexpr);
		tok = (comp_tok_t *)atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Switch expression does not have integer type.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

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

	/* Switch expression result */
	cgswitch->sres = &eres;

	/* Body */

	rc = parser_process_block(cgproc->cgen->parser, &aswitch->body);
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

	/* Switch expression is an enum and there is no default label */
	if (eres.cgtype->ntype == cgn_enum && cgswitch->dlabel == NULL) {
		/* Verify that all enum values are handled */
		tenum = (cgtype_enum_t *)eres.cgtype->ext;
		elem = cgen_enum_first(tenum->cgenum);
		while (elem != NULL) {
			rc = cgen_switch_find_value(cgswitch, elem->value,
			    &value);
			if (rc != EOK) {
				tok = (comp_tok_t *)aswitch->tswitch.data;
				lexer_dprint_tok(&tok->tok, stderr);
				fprintf(stderr, ": Warning: Enumeration value "
				    "'%s' not handled in switch.\n",
				    elem->ident);
				++cgproc->cgen->warnings;
			}

			elem = cgen_enum_next(elem);
		}
	}

	cgproc->cur_switch = cgswitch->parent;
	cgen_switch_destroy(cgswitch);
	cgproc->cur_loop_switch = lswitch->parent;
	cgen_loop_switch_destroy(lswitch);
	free(eslabel);
	cgen_eres_fini(&eres);
	cgproc->cur_loop_switch = old_lswitch;
	return EOK;
error:
	cgproc->cur_loop_switch = old_lswitch;
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

/** Check case label type where switch expression is an integer.
 *
 * @param cgproc Code generator for procedure
 * @param stype Switch expression type
 * @param ctype Case expression type
 * @param atok Token of case expresison
 */
static void cgen_clabel_check_integer(cgen_proc_t *cgproc, cgtype_t *stype,
    cgtype_t *ctype, ast_tok_t *atok)
{
	cgtype_basic_t *tbasic;
	comp_tok_t *tok;

	(void)cgproc;
	tok = (comp_tok_t *)atok->data;

	switch (ctype->ntype) {
	case cgn_basic:
		tbasic = (cgtype_basic_t *)ctype->ext;
		if (tbasic->elmtype == cgelm_logic) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Case expression has truth "
			    "value, switch expression type is ");
			(void) cgtype_print(stype, stderr);
			fprintf(stderr, ".\n");
			++cgproc->cgen->warnings;
		}
		break;
	case cgn_enum:
		if (cgtype_is_strict_enum(ctype)) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Case expression is ");
			(void) cgtype_print(ctype, stderr);
			fprintf(stderr, ", switch expression type is ");
			(void) cgtype_print(stype, stderr);
			fprintf(stderr, ".\n");
			++cgproc->cgen->warnings;
		}
		break;
	default:
		assert(false);
	}
}

/** Check case label type where switch expression is a truth value.
 *
 * @param cgproc Code generator for procedure
 * @param stype Switch expression type
 * @param ctype Case expression type
 * @param atok Token of case expresison
 */
static void cgen_clabel_check_logic(cgen_proc_t *cgproc, cgtype_t *ctype,
    ast_tok_t *atok)
{
	cgtype_basic_t *tbasic;
	comp_tok_t *tok;

	(void)cgproc;
	tok = (comp_tok_t *)atok->data;

	switch (ctype->ntype) {
	case cgn_basic:
		tbasic = (cgtype_basic_t *)ctype->ext;
		if (tbasic->elmtype != cgelm_logic) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Case expression is ");
			(void) cgtype_print(ctype, stderr);
			fprintf(stderr, ", switch expression has truth value.\n");
			++cgproc->cgen->warnings;
		}
		break;
	case cgn_enum:
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Case expression is ");
		(void) cgtype_print(ctype, stderr);
		fprintf(stderr, ", switch expression has truth value.\n");
		++cgproc->cgen->warnings;
		break;
	default:
		assert(false);
	}
}

/** Check case label type where switch expression is an enum.
 *
 * @param cgproc Code generator for procedure
 * @param stype Switch expression type
 * @param ctype Case expression type
 * @param atok Token of case expresison
 */
static void cgen_clabel_check_enum(cgen_proc_t *cgproc, cgtype_t *stype,
    cgtype_t *ctype, ast_tok_t *atok)
{
	cgtype_basic_t *tbasic;
	cgtype_enum_t *senum;
	cgtype_enum_t *cenum;
	comp_tok_t *tok;

	(void)cgproc;
	tok = (comp_tok_t *)atok->data;

	switch (ctype->ntype) {
	case cgn_basic:
		tbasic = (cgtype_basic_t *)ctype->ext;
		if (tbasic->elmtype == cgelm_logic) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Case expression has truth "
			    "value, switch expression type is ");
			(void) cgtype_print(stype, stderr);
			fprintf(stderr, ".\n");
			++cgproc->cgen->warnings;
		} else {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Case expression is ");
			(void) cgtype_print(ctype, stderr);
			fprintf(stderr, ", switch expression type is ");
			(void) cgtype_print(stype, stderr);
			fprintf(stderr, ".\n");
			++cgproc->cgen->warnings;
		}
		break;
	case cgn_enum:
		senum = (cgtype_enum_t *)stype->ext;
		cenum = (cgtype_enum_t *)ctype->ext;
		if (senum->cgenum != cenum->cgenum) {
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Warning: Case expression is ");
			(void) cgtype_print(ctype, stderr);
			fprintf(stderr, ", switch expression type is ");
			(void) cgtype_print(stype, stderr);
			fprintf(stderr, ".\n");
			++cgproc->cgen->warnings;
		}
		break;
	default:
		assert(false);
	}
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
	cgen_eres_t *sres;
	cgen_eres_t cres;
	cgen_eres_t eres;
	cgen_eres_t ieres;
	cgtype_basic_t *ctbasic;
	cgtype_basic_t *tbasic;
	cgtype_enum_t *tenum;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	ir_oper_var_t *carg = NULL;
	ast_tok_t *atok;
	comp_tok_t *tok;
	bool csigned;
	unsigned lblno;
	cgtype_elmtype_t elmtype;
	char *dvarname;
	bool converted;
	cgen_switch_value_t *value;
	int rc;

	cgen_eres_init(&cres);
	cgen_eres_init(&eres);
	cgen_eres_init(&ieres);

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

	rc = cgen_intexpr_val(cgproc->cgen, aclabel->cexpr, &eres);
	if (rc != EOK)
		goto error;

	/* If it is an enum, convert it to integer */
	rc = cgen_enum2int(cgproc->cgen, &eres, &ieres, &converted);
	if (rc != EOK)
		goto error;

	/* Switch expression result */
	sres = cgproc->cur_switch->sres;

	switch (sres->cgtype->ntype) {
	case cgn_basic:
		ctbasic = (cgtype_basic_t *)ieres.cgtype->ext;
		csigned = cgen_basic_type_signed(cgproc->cgen, ctbasic);
		tbasic = (cgtype_basic_t *)sres->cgtype->ext;

		atok = ast_tree_first_tok(aclabel->cexpr);

		if (tbasic->elmtype == cgelm_logic) {
			/* Check case expression value is boolean */
			if (eres.cvint != 0 && eres.cvint != 1) {
				cgen_warn_case_value_not_bool(cgproc->cgen,
				    atok);
			}

			/*
			 * Check case expression type is compatible with
			 * logic type
			 */
			cgen_clabel_check_logic(cgproc, eres.cgtype, atok);
		} else {
			/* Check expression value is in integer type range */
			if (!cgen_cvint_in_tbasic_range(cgproc->cgen, csigned,
			    eres.cvint, tbasic)) {
				cgen_warn_case_value_range(cgproc->cgen, atok,
				    sres->cgtype);
			}

			/*
			 * Check case expression type is compatible with
			 * integer type
			 */
			cgen_clabel_check_integer(cgproc, sres->cgtype,
			    eres.cgtype, atok);
		}

		elmtype = tbasic->elmtype;
		break;
	case cgn_enum:
		// XXX Flexible-sized enum
		elmtype = cgelm_int;
		ctbasic = (cgtype_basic_t *)ieres.cgtype->ext;
		csigned = cgen_basic_type_signed(cgproc->cgen, ctbasic);
		tenum = (cgtype_enum_t *)sres->cgtype->ext;

		atok = ast_tree_first_tok(aclabel->cexpr);

		/*
		 * Is the value in the enum? This might be a little redundant
		 * if strict enum checking is enabled. Might be a problem
		 * if one decided to switch on a bitfield-style enum.
		 * We'll see.
		 */
		if (!cgen_cvint_in_enum(cgproc->cgen, csigned, eres.cvint,
		    tenum->cgenum)) {
			cgen_warn_case_value_not_in_enum(cgproc->cgen, atok,
			    sres->cgtype);
		}

		/*
		 * Check case expression type is compatible with
		 * enum type
		 */
		cgen_clabel_check_enum(cgproc, sres->cgtype, eres.cgtype,
		    atok);
		break;
	default:
		assert(false);
		rc = EINVAL;
		goto error;
	}

	/* Check for duplicate case value */
	rc = cgen_switch_find_value(cgproc->cur_switch, eres.cvint, &value);
	if (rc == EOK) {
		/* Found existing value */
		atok = ast_tree_first_tok(aclabel->cexpr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Duplicate case value.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Insert to list of values */
	rc = cgen_switch_insert_value(cgproc->cur_switch, eres.cvint);
	if (rc != EOK)
		goto error;

	/* Introduce constant with case expression value */

	rc = cgen_const_int(cgproc, elmtype, eres.cvint, lblock, &cres);
	if (rc != EOK)
		goto error;

	/* Compare values of switch and case */

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(cgproc->cur_switch->sres->varname, &larg);
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
	cgen_eres_fini(&eres);
	cgen_eres_fini(&ieres);
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
	cgen_eres_fini(&eres);
	cgen_eres_fini(&ieres);
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

	if (cgproc->cur_switch->dlabel != NULL) {
		tok = (comp_tok_t *) adlabel->tdefault.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Multiple default labels in switch statement.\n");
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
 * @param cgexpr Code generator for expression
 * @param expr Expression
 * @param ares Expression result
 */
static void cgen_expr_check_unused(cgen_expr_t *cgexpr, ast_node_t *expr,
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
		++cgexpr->cgen->warnings;
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

	/* Compute the value of the expression (may have side effects) */
	rc = cgen_expr(&cgproc->cgexpr, stexpr->expr, lblock, &ares);
	if (rc != EOK)
		goto error;

	/*
	 * If the expression computes a value that is not used within
	 * the expression itself (e.g. i + 1), generate a warning.
	 */
	cgen_expr_check_unused(&cgproc->cgexpr, stexpr->expr, &ares);

	/* Ignore the value of the expression */
	cgen_eres_fini(&ares);
	return EOK;
error:
	return rc;
}

/** Generate code for declaring a local variable.
 *
 * @param cgproc Code generator for procedure
 * @param sctype Storage class
 * @param dtype Variable type
 * @param ident Variable identifier
 * @param itok Initialization token or @c NULL
 * @param iexpr Initializer expression or @c NULL
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_lvar(cgen_proc_t *cgproc, ast_sclass_type_t sctype,
    cgtype_t *dtype, comp_tok_t *ident, comp_tok_t *itok, ast_node_t *iexpr,
    ir_lblock_t *lblock)
{
	char *vident = NULL;
	ir_lvar_t *lvar;
	ir_texpr_t *vtype = NULL;
	cgen_eres_t cres;
	cgen_eres_t ires;
	cgen_eres_t lres;
	int rc;

	if (cgen_type_is_incomplete(cgproc->cgen, dtype)) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Variable has incomplete type.\n");
		cgproc->cgen->error = true; // TODO
		return EINVAL;
	}

	cgen_eres_init(&cres);
	cgen_eres_init(&ires);
	cgen_eres_init(&lres);

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
	rc = scope_insert_lvar(cgproc->cgen->cur_scope, &ident->tok,
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

	rc = ir_lvar_create(vident, vtype, &lvar);
	if (rc != EOK)
		goto error;

	/* Initializer? */
	if (iexpr != NULL) {
		/* Variable address */
		rc = cgen_lvaraddr(cgproc, vident, lblock, &lres);
		if (rc != EOK)
			goto error;

		/* Value of initializer expression */
		rc = cgen_expr(&cgproc->cgexpr, iexpr, lblock, &ires);
		if (rc != EOK)
			goto error;

		/* Convert expression result to variable type */
		rc = cgen_type_convert(&cgproc->cgexpr, itok, &ires, dtype,
		    cgen_implicit, lblock, &cres);
		if (rc != EOK)
			goto error;

		/* Store the converted value */
		rc = cgen_store(cgproc, &lres, &cres, lblock);
		if (rc != EOK)
			goto error;
	}

	free(vident);
	vident = NULL;
	vtype = NULL; /* ownership transferred */
	ir_proc_append_lvar(cgproc->irproc, lvar);

	cgtype_destroy(dtype);
	dtype = NULL;

	cgen_eres_fini(&cres);
	cgen_eres_fini(&ires);
	cgen_eres_fini(&lres);
	return EOK;
error:
	cgen_eres_fini(&cres);
	cgen_eres_fini(&ires);
	cgen_eres_fini(&lres);
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
	cgtype_enum_t *tenum;
	comp_tok_t *itok;
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
		/* Mark enum as named, because it has an instance. */
		if (stype->ntype == cgn_enum) {
			tenum = (cgtype_enum_t *)stype;
			tenum->cgenum->named = true;
		}

		/* Process declarator */
		rc = cgen_decl(cgproc->cgen, stype, identry->decl,
		    identry->aslist, &dtype);
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

		aident = ast_decl_get_ident(identry->decl);
		ident = (comp_tok_t *) aident->data;

		/* Check for shadowing a wider-scope identifier */
		member = scope_lookup(cgproc->cgen->cur_scope->parent,
		    ident->tok.text);
		if (member != NULL) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Declaration of '%s' "
			    "shadows a wider-scope declaration.\n",
			    ident->tok.text);
			++cgproc->cgen->warnings;
		}

		if (identry->have_init) {
			itok = (comp_tok_t *)identry->tassign.data;
		} else {
			itok = NULL;
		}

		/* Local variable */
		rc = cgen_lvar(cgproc, sctype, dtype, ident, itok,
		    identry->init, lblock);
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
	cgen_rd_flags_t flags;
	int rc;

	/* Process declaration specifiers */

	rc = cgen_dspecs(cgproc->cgen, stdecln->dspecs, &sctype, &flags,
	    &stype);
	if (rc != EOK)
		goto error;

	(void)flags;

	if (sctype == asc_typedef) {
		rc = cgen_typedef(cgproc->cgen,
		    ast_tree_first_tok(&stdecln->dspecs->node), stdecln->idlist,
		    stype);
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

/** Generate code for __va_copy statement.
 *
 * @param cgexpr Code generator for procedure
 * @param stva_copy AST __va_copy statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_va_copy(cgen_proc_t *cgproc, ast_va_copy_t *stva_copy,
    ir_lblock_t *lblock)
{
	cgen_eres_t dres;
	cgen_eres_t sres;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *var1 = NULL;
	ir_oper_var_t *var2 = NULL;
	int rc;

	cgen_eres_init(&dres);
	cgen_eres_init(&sres);

	/* Evaluate dest expression */
	rc = cgen_expr(&cgproc->cgexpr, stva_copy->dexpr, lblock, &dres);
	if (rc != EOK)
		goto error;

	/* Check that dest is of type __va_list */
	rc = cgen_check_va_list(cgproc, dres.cgtype,
	    ast_tree_first_tok(stva_copy->dexpr));
	if (rc != EOK)
		goto error;

	/* Evaluate src expression */
	rc = cgen_expr(&cgproc->cgexpr, stva_copy->sexpr, lblock, &sres);
	if (rc != EOK)
		goto error;

	/* Check that src is of type __va_list */
	rc = cgen_check_va_list(cgproc, sres.cgtype,
	    ast_tree_first_tok(stva_copy->sexpr));
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(dres.varname, &var1);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(sres.varname, &var2);
	if (rc != EOK)
		goto error;

	instr->itype = iri_vacopy;
	instr->dest = NULL;
	instr->op1 = &var1->oper;
	instr->op2 = &var2->oper;

	ir_lblock_append(lblock, NULL, instr);

	var1 = NULL;
	var2 = NULL;

	cgen_eres_fini(&dres);
	cgen_eres_fini(&sres);

	return EOK;
error:
	cgen_eres_fini(&dres);
	cgen_eres_fini(&sres);
	ir_instr_destroy(instr);
	if (var1 != NULL)
		ir_oper_destroy(&var1->oper);
	if (var2 != NULL)
		ir_oper_destroy(&var2->oper);
	return rc;
}

/** Generate code for __va_end statement.
 *
 * @param cgexpr Code generator for procedure
 * @param va_end AST __va_end statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_va_end(cgen_proc_t *cgproc, ast_va_end_t *va_end,
    ir_lblock_t *lblock)
{
	cgen_eres_t apres;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *var = NULL;
	int rc;

	cgen_eres_init(&apres);

	/* Evaluate ap expression */
	rc = cgen_expr(&cgproc->cgexpr, va_end->apexpr, lblock, &apres);
	if (rc != EOK)
		goto error;

	/* Check that ap is of type __va_list */
	rc = cgen_check_va_list(cgproc, apres.cgtype,
	    ast_tree_first_tok(va_end->apexpr));
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(apres.varname, &var);
	if (rc != EOK)
		goto error;

	instr->itype = iri_vaend;
	instr->dest = NULL;
	instr->op1 = &var->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	var = NULL;

	cgen_eres_fini(&apres);

	return EOK;
error:
	cgen_eres_fini(&apres);
	ir_instr_destroy(instr);
	if (var != NULL)
		ir_oper_destroy(&var->oper);
	return rc;
}

/** Generate code for __va_start statement.
 *
 * @param cgexpr Code generator for procedure
 * @param stva_start AST __va_start statement
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_va_start(cgen_proc_t *cgproc, ast_va_start_t *stva_start,
    ir_lblock_t *lblock)
{
	cgen_eres_t apres;
	ast_tok_t *atok;
	comp_tok_t *tok;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *var1 = NULL;
	ast_eident_t *aident;
	int rc;

	cgen_eres_init(&apres);

	/* Make sure the current function is variadic */
	if (cgproc->irproc->variadic != true) {
		atok = &stva_start->tva_start;
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Use of __va_start in a function that "
		    "does not take variable arguments.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Evaluate ap expression */
	rc = cgen_expr(&cgproc->cgexpr, stva_start->apexpr, lblock, &apres);
	if (rc != EOK)
		goto error;

	/* Check that ap is of type __va_list */
	rc = cgen_check_va_list(cgproc, apres.cgtype,
	    ast_tree_first_tok(stva_start->apexpr));
	if (rc != EOK)
		goto error;

	/* lexpr should be the identifier of the last fixed parameter */
	if (stva_start->lexpr->ntype != ant_eident) {
		atok = ast_tree_first_tok(stva_start->lexpr);
		tok = (comp_tok_t *)atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Expected identifier of last fixed "
		    "parameter.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	aident = (ast_eident_t *)stva_start->lexpr->ext;
	tok = (comp_tok_t *) aident->tident.data;

	if (cgproc->last_arg == NULL ||
	    strcmp(cgproc->last_arg, tok->tok.text) != 0) {
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Expected identifier of last fixed "
		    "parameter.\n");
		cgproc->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(apres.varname, &var1);
	if (rc != EOK)
		goto error;

	instr->itype = iri_vastart;
	instr->dest = NULL;
	instr->op1 = &var1->oper;
	instr->op2 = NULL;

	ir_lblock_append(lblock, NULL, instr);

	var1 = NULL;

	cgen_eres_fini(&apres);

	return EOK;
error:
	cgen_eres_fini(&apres);
	ir_instr_destroy(instr);
	if (var1 != NULL)
		ir_oper_destroy(&var1->oper);
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
	case ant_while:
	case ant_do:
	case ant_for:
	case ant_switch:
		rc = EOK; /* already processed */
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
	case ant_va_copy:
		rc = cgen_va_copy(cgproc, (ast_va_copy_t *) stmt->ext, lblock);
		break;
	case ant_va_end:
		rc = cgen_va_end(cgproc, (ast_va_end_t *) stmt->ext, lblock);
		break;
	case ant_va_start:
		rc = cgen_va_start(cgproc, (ast_va_start_t *) stmt->ext,
		    lblock);
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

	rc = scope_create(cgproc->cgen->cur_scope, &block_scope);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	/* Enter block scope */
	cgproc->cgen->cur_scope = block_scope;

	(void)lblock;

	do {
		rc = parser_process_stmt(cgproc->cgen->parser, &stmt);
		if (rc == ENOENT)
			break;
		if (rc != EOK)
			goto error;

		ast_block_append(block, stmt);

		rc = cgen_stmt(cgproc, stmt, lblock);
		if (rc != EOK)
			goto error;
	} while (true);

	/* Check for defined, but unused, identiiers */
	cgen_check_scope_unused(cgproc, block_scope);

	/* Leave block scope */
	cgproc->cgen->cur_scope = block_scope->parent;
	scope_destroy(block_scope);
	return EOK;
error:
	if (block_scope != NULL) {
		/* Leave block scope */
		cgproc->cgen->cur_scope = block_scope->parent;
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

/** Adjust type when passing to a function.
 *
 * Arrays are passed as pointers, other types are unchanged.
 *
 * @param cgen Code generator
 * @param stype Specified type
 * @param ptype Place to store passed type
 */
static int cgen_fun_arg_passed_type(cgen_t *cgen, cgtype_t *stype,
    cgtype_t **ptype)
{
	cgtype_t *etype = NULL;
	cgtype_t *ltype = NULL;
	cgtype_array_t *arrt;
	cgtype_pointer_t *ptrt;
	int rc;

	(void)cgen;

	if (stype->ntype == cgn_array) {
		/*
		 * An array is really passed as a pointer to its elements.
		 */

		arrt = (cgtype_array_t *)stype->ext;

		rc = cgtype_clone(arrt->etype, &etype);
		if (rc != EOK)
			goto error;

		rc = cgtype_pointer_create(etype, &ptrt);
		if (rc != EOK) {
			cgtype_destroy(etype);
			goto error;
		}

		*ptype = &ptrt->cgtype;
		etype = NULL;
	} else if (stype->ntype == cgn_basic &&
	    ((cgtype_basic_t *)stype->ext)->elmtype == cgelm_va_list) {
		/*
		 * A va_list is really passed as a pointer.
		 */

		rc = cgtype_clone(stype, &ltype);
		if (rc != EOK)
			goto error;

		rc = cgtype_pointer_create(ltype, &ptrt);
		if (rc != EOK) {
			cgtype_destroy(etype);
			goto error;
		}

		*ptype = &ptrt->cgtype;
		ltype = NULL;
	} else {
		/* Other types are passed as themselves */
		rc = cgtype_clone(stype, ptype);
		if (rc != EOK)
			goto error;
	}

	return EOK;
error:
	if (etype != NULL)
		cgtype_destroy(etype);
	if (ltype != NULL)
		cgtype_destroy(ltype);
	return rc;
}

/** Code generate function argument type.
 *
 * @param cgen Code generator
 * @param stype Specified type
 * @param atype Place to store IR type expression
 */
static int cgen_fun_arg_type(cgen_t *cgen, cgtype_t *stype,
    ir_texpr_t **atype)
{
	cgtype_t *argtype = NULL;
	int rc;

	/* Determine how argument should be passed */
	rc = cgen_fun_arg_passed_type(cgen, stype, &argtype);
	if (rc != EOK)
		goto error;

	rc = cgen_cgtype(cgen, argtype, atype);
	if (rc != EOK)
		goto error;

	cgtype_destroy(argtype);
	return EOK;
error:
	if (argtype != NULL)
		cgtype_destroy(argtype);
	return rc;
}

/** Generate code for function arguments definition.
 *
 * Add arguments to IR procedure based on CG function type.
 *
 * @param cgen Code generator
 * @param ident Function identifier token
 * @param ftype Function type
 * @param irproc IR procedure to which the arguments should be appended
 * @return EOK on success or an error code
 */
static int cgen_fun_args(cgen_t *cgen, comp_tok_t *ident, cgtype_t *ftype,
    ir_proc_t *proc)
{
	ir_proc_arg_t *iarg;
	ir_texpr_t *atype = NULL;
	char *arg_ident = NULL;
	cgtype_func_t *dtfunc;
	cgtype_func_arg_t *dtarg;
	cgtype_t *stype;
	unsigned next_var;
	unsigned argidx;
	int rc;
	int rv;

	assert(ftype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)ftype->ext;

	next_var = 0;

	/* Arguments */
	dtarg = cgtype_func_first(dtfunc);
	argidx = 1;
	while (dtarg != NULL) {
		assert(dtarg != NULL);
		stype = dtarg->atype;

		rv = asprintf(&arg_ident, "%%%d", next_var++);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		/*
		 * Check type for completeness. Arrays may have unspecified
		 * size, as long as the element type is complete.
		 */
		if (!cgen_type_is_complete_or_array(cgen, stype)) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Argument %u has incomplete type.\n",
			    argidx);
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		/* Generate argument type */
		rc = cgen_fun_arg_type(cgen, stype, &atype);
		if (rc != EOK)
			goto error;

		rc = ir_proc_arg_create(arg_ident, atype, &iarg);
		if (rc != EOK)
			goto error;

		free(arg_ident);
		arg_ident = NULL;
		atype = NULL; /* ownership transferred */

		ir_proc_append_arg(proc, iarg);
		++argidx;
		dtarg = cgtype_func_next(dtarg);
	}

	proc->variadic = dtfunc->variadic;

	return EOK;
error:
	if (arg_ident != NULL)
		free(arg_ident);
	if (atype != NULL)
		ir_texpr_destroy(atype);
	return rc;
}

/** Generate code for function lvalue arguments.
 *
 * Declare a variable for each argument and initialize it with the argument
 * value.
 *
 * @param cgen Code generator for procedure
 * @param ident Function identifier token
 * @param ftype Function type
 * @param dfun  Function declarator
 * @param irproc IR procedure to which the arguments should be appended
 * @return EOK on success or an error code
 */
static int cgen_fun_lvalue_args(cgen_proc_t *cgproc, comp_tok_t *ident,
    cgtype_t *ftype, ast_dfun_t *dfun, ir_proc_t *proc)
{
	ir_texpr_t *atype = NULL;
	char *vident = NULL;
	char *arg_ident = NULL;
	cgtype_func_t *dtfunc;
	cgtype_func_arg_t *dtarg;
	ast_dfun_arg_t *arg;
	cgtype_t *stype;
	cgtype_t *ptype = NULL;
	ir_lvar_t *lvar = NULL;
	ast_tok_t *aident;
	comp_tok_t *caident;
	const char *cident;
	unsigned next_var;
	unsigned argidx;
	cgen_eres_t ares;
	cgen_eres_t vres;
	int rc;
	int rv;

	(void)ident;
	assert(ftype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)ftype->ext;

	next_var = 0;
	cgen_eres_init(&ares);
	cgen_eres_init(&vres);

	/* Arguments */
	dtarg = cgtype_func_first(dtfunc);
	arg = ast_dfun_first(dfun);
	argidx = 1;
	while (dtarg != NULL) {
		aident = ast_decl_get_ident(arg->decl);
		caident = (comp_tok_t *) aident->data;
		cident = caident->tok.text;

		assert(dtarg != NULL);
		stype = dtarg->atype;

		rv = asprintf(&arg_ident, "%%%d", next_var++);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		/* Generate argument type */
		rc = cgen_fun_arg_type(cgproc->cgen, stype, &atype);
		if (rc != EOK)
			goto error;

		rc = cgen_fun_arg_passed_type(cgproc->cgen, stype, &ptype);
		if (rc != EOK)
			goto error;

		/* Generate an IR variable name */
		rc = cgen_create_loc_var_name(cgproc, cident, &vident);
		if (rc != EOK) {
			rc = ENOMEM;
			goto error;
		}

		/* Insert identifier into argument scope */
		rc = scope_insert_lvar(cgproc->arg_scope, &caident->tok,
		    stype, vident);
		if (rc != EOK)
			goto error;

		rc = ir_lvar_create(vident, atype, &lvar);
		if (rc != EOK)
			goto error;

		atype = NULL; /* ownership transferred */

		ir_proc_append_lvar(cgproc->irproc, lvar);
		lvar = NULL;

		rc = cgen_lvaraddr(cgproc, vident, cgproc->irproc->lblock,
		    &ares);
		if (rc != EOK)
			goto error;

		/* Argument value */
		vres.varname = arg_ident;
		vres.valtype = cgen_rvalue;
		vres.cgtype = ptype;
		ptype = NULL;

		rc = cgen_store(cgproc, &ares, &vres, cgproc->irproc->lblock);
		if (rc != EOK)
			goto error;

		free(arg_ident);
		arg_ident = NULL;

		++argidx;
		dtarg = cgtype_func_next(dtarg);
		arg = ast_dfun_next(arg);
	}

	proc->variadic = dtfunc->variadic;

	cgen_eres_fini(&ares);
	cgen_eres_fini(&vres);
	return EOK;
error:
	if (ptype != NULL)
		cgtype_destroy(ptype);
	cgen_eres_fini(&ares);
	cgen_eres_fini(&vres);
	if (lvar != NULL)
		ir_lvar_destroy(lvar);
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
 * @return EOK on success, EINVAL if cgtype is an incomplete type,
 *         ENOMEM if out of memory.
 */
static int cgen_cgtype(cgen_t *cgen, cgtype_t *cgtype, ir_texpr_t **rirtexpr)
{
	cgtype_basic_t *tbasic;
	cgtype_record_t *trecord;
	cgtype_array_t *tarray;
	ir_texpr_t *iretexpr = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (cgtype->ntype == cgn_basic) {
		/* Void? */
		tbasic = (cgtype_basic_t *)cgtype->ext;
		if (tbasic->elmtype == cgelm_void)
			return EOK;

		if (tbasic->elmtype == cgelm_va_list) {
			rc = ir_texpr_va_list_create(rirtexpr);
			if (rc != EOK)
				goto error;
		} else {
			bits = cgen_basic_type_bits(cgen,
			    (cgtype_basic_t *)cgtype->ext);
			if (bits == 0) {
				fprintf(stderr,
				    "cgen_cgtype: Unimplemented type.\n");
				cgen->error = true; // TODO
				rc = EINVAL;
				goto error;
			}

			rc = ir_texpr_int_create(bits, rirtexpr);
			if (rc != EOK)
				goto error;
		}

	} else if (cgtype->ntype == cgn_pointer) {
		/* Pointer */

		rc = ir_texpr_ptr_create(cgen_pointer_bits, rirtexpr);
		if (rc != EOK)
			goto error;
	} else if (cgtype->ntype == cgn_record) {
		/* Record */
		trecord = (cgtype_record_t *)cgtype->ext;
		if (trecord->record == NULL)
			return EINVAL;

		rc = ir_texpr_ident_create(trecord->record->irident,
		    rirtexpr);
		if (rc != EOK)
			goto error;
	} else if (cgtype->ntype == cgn_enum) {
		rc = ir_texpr_int_create(cgen_enum_bits, rirtexpr);
		if (rc != EOK)
			goto error;
	} else if (cgtype->ntype == cgn_array) {
		/* Array */
		tarray = (cgtype_array_t *)cgtype->ext;
		rc = cgen_cgtype(cgen, tarray->etype, &iretexpr);
		if (rc != EOK)
			goto error;

		assert(tarray->have_size);

		rc = ir_texpr_array_create(tarray->asize, iretexpr, rirtexpr);
		if (rc != EOK)
			goto error;

		iretexpr = NULL;
	} else {
		fprintf(stderr, "cgen_cgtype: Unimplemented type.\n");
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	return EOK;
error:
	if (iretexpr != NULL)
		ir_texpr_destroy(iretexpr);
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
static int cgen_fun_rtype(cgen_t *cgen, cgtype_t *ftype,
    ir_proc_t *proc)
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
 * @param sctype Storace class specifier type
 * @param btype Type derived from declaration specifiers
 * @return EOK on success or an error code
 */
static int cgen_fundef(cgen_t *cgen, ast_gdecln_t *gdecln,
    ast_sclass_type_t sctype, cgtype_t *btype)
{
	ir_proc_t *proc = NULL;
	ir_lblock_t *lblock = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	cgen_proc_t *cgproc = NULL;
	cgen_proc_t *old_cgproc;
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
	cgtype_t *ctype = NULL;
	cgtype_t *ptype = NULL;
	cgtype_func_t *dtfunc;
	cgtype_func_arg_t *dtarg;
	ast_aspec_t *aspec;
	ast_aspec_attr_t *attr;
	scope_t *prev_scope = NULL;
	bool vstatic = false;
	bool old_static;
	ir_linkage_t linkage;
	int rc;
	int rv;

	old_cgproc = cgen->cur_cgproc;

	aident = ast_gdecln_get_ident(gdecln);
	ident = (comp_tok_t *) aident->data;

	if (sctype == asc_static) {
		vstatic = true;
	} else if (sctype == asc_extern) {
		atok = ast_tree_first_tok(&gdecln->dspecs->node);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Function definition should not use 'extern'.\n");
		++cgen->warnings;
	} else if (sctype != asc_none) {
		atok = ast_tree_first_tok(&gdecln->dspecs->node);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Unimplemented storage class specifier.\n");
		++cgen->warnings;
	}

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	/* If the function is not declared yet, create a symbol */
	symbol = symbols_lookup(cgen->symbols, ident->tok.text);
	if (symbol == NULL) {
		rc = symbols_insert(cgen->symbols, st_fun, ident, pident,
		    &symbol);
		if (rc != EOK)
			goto error;

		assert(symbol != NULL);
		if (vstatic)
			symbol->flags |= sf_static;
	} else {
		if (symbol->stype != st_fun) {
			/* Already declared as a different type of symbol */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": '%s' already declared as a "
			    "different type of symbol.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		if ((symbol->flags & sf_defined) != 0) {
			/* Already defined */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Redefinition of '%s'.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		/* Check if static did not change */
		old_static = (symbol->flags & sf_static) != 0;
		if (vstatic && !old_static) {
			/* Non-static previously declared as static */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Static '%s' was previously "
			    "declared as non-static.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		} else if (!vstatic && old_static) {
			/* Non-static previously declared as static */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: non-static '%s' was "
			    "previously declared as static.\n",
			    ident->tok.text);
			++cgen->warnings;
		}
	}

	/* Mark the symbol as defined and not extern */
	symbol->flags |= sf_defined;
	symbol->flags &= ~sf_extern;

	/* Identifier-declarator list entry */
	idle = ast_idlist_first(gdecln->idlist);
	assert(idle != NULL);
	assert(ast_idlist_next(idle) == NULL);

	/* Process declarator */
	rc = cgen_decl(cgen, btype, idle->decl, idle->aslist, &dtype);
	if (rc != EOK)
		goto error;

	if (symbol->cgtype == NULL) {
		rc = cgtype_clone(dtype, &ctype);
		if (rc != EOK)
			goto error;
	} else {
		rc = cgtype_compose(symbol->cgtype, dtype, &ctype);
		if (rc == EINVAL) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Conflicting type '");
			cgtype_print(dtype, stderr);
			fprintf(stderr, "' for '%s', previously "
			    "declared as '", ident->tok.text);
			cgtype_print(symbol->cgtype, stderr);
			fprintf(stderr, "'.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}
		if (rc != EOK)
			goto error;
	}

	/* Copy type to symbol */
	if (symbol->cgtype == NULL) {
		rc = cgtype_clone(ctype, &symbol->cgtype);
		if (rc != EOK)
			goto error;
	}

	assert(ctype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)ctype->ext;

	if (dtfunc->rtype->ntype == cgn_array) {
		/* Function returning array */
		cgen_error_fun_ret_array(cgen, aident);
		rc = EINVAL;
		goto error;
	}

	/* Insert identifier into module scope */
	rc = scope_insert_gsym(cgen->scope, &ident->tok, ctype, symbol);
	if (rc == ENOMEM)
		goto error;

	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	if ((symbol->flags & sf_static) != 0)
		linkage = irl_default;
	else
		linkage = irl_global;

	rc = ir_proc_create(pident, linkage, lblock, &proc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	rc = cgen_proc_create(cgen, proc, &cgproc);
	if (rc != EOK)
		goto error;

	cgen->cur_cgproc = cgproc;

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
		if (dident == NULL) {
			atok = ast_tree_first_tok(&arg->dspecs->node);

			tok = (comp_tok_t *) atok->data;
			lexer_dprint_tok(&tok->tok, stderr);
			fprintf(stderr, ": Argument identifier missing.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

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

		/* Remember identifier of last fixed argument */
		if (cgproc->last_arg != NULL)
			free(cgproc->last_arg);

		cgproc->last_arg = strdup(tok->tok.text);
		if (cgproc->last_arg == NULL) {
			rc = ENOMEM;
			goto error;
		}

		rv = asprintf(&arg_ident, "%%%d", cgproc->next_var++);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		/* Convert argument type */
		rc = cgen_fun_arg_passed_type(cgproc->cgen, stype, &ptype);
		if (rc != EOK)
			goto error;

		if ((cgproc->cgen->flags & cgf_lvalue_args) == 0) {
			/* Insert identifier into argument scope */
			rc = scope_insert_arg(cgproc->arg_scope, &tok->tok,
			    ptype, arg_ident);
			if (rc != EOK)
				goto error;
		}

		cgtype_destroy(ptype);
		ptype = NULL;

		free(arg_ident);
		arg_ident = NULL;

		arg = ast_dfun_next(arg);
		dtarg = cgtype_func_next(dtarg);
	}

	/* Generate IR procedure arguments */
	rc = cgen_fun_args(cgproc->cgen, ident, ctype, proc);
	if (rc != EOK)
		goto error;

	/* Lvalue arguments? */
	if ((cgproc->cgen->flags & cgf_lvalue_args) != 0) {
		/* Create an initialized variable for each argument. */
		rc = cgen_fun_lvalue_args(cgproc, ident, ctype, dfun, proc);
		if (rc != EOK)
			goto error;
	}

	/* Check return type for completeness */
	if (cgen_type_is_incomplete(cgen, dtfunc->rtype)) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Function returns incomplete type '");
		cgtype_print(dtfunc->rtype, stderr);
		fprintf(stderr, "'.\n");
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	/* Generate IR return type */
	rc = cgen_fun_rtype(cgen, ctype, proc);
	if (rc != EOK)
		goto error;

	/* Enter argument scope */
	prev_scope = cgen->cur_scope;
	cgen->cur_scope = cgproc->arg_scope;
	cgen->cur_lblock = proc->lblock;

	rc = parser_process_block(cgen->parser, &gdecln->body);
	if (rc != EOK)
		goto error;

	cgen->cur_lblock = NULL;

	/* Make sure we return if control reaches '}' */
	rc = cgen_ret(cgproc, proc->lblock);
	if (rc != EOK)
		goto error;

	free(pident);
	pident = NULL;

	cgtype_destroy(dtype);
	dtype = NULL;
	cgtype_destroy(ctype);
	ctype = NULL;

	ir_module_append(cgen->irmod, &proc->decln);
	proc = NULL;

	/* Check for defined, but unused, identifiers */
	cgen_check_scope_unused(cgproc, cgproc->arg_scope);

	/* Leave argument scope */
	cgen->cur_scope = prev_scope;
	prev_scope = NULL;

	/* Check for used, but not defined and defined, but not used labels */
	rc = cgen_check_labels(cgproc, cgproc->labels);
	if (rc != EOK)
		goto error;

	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	cgproc = NULL;

	return EOK;
error:
	cgen->cur_lblock = NULL;

	if (prev_scope != NULL)
		cgen->cur_scope = prev_scope;

	ir_proc_destroy(proc);
	cgen->cur_cgproc = old_cgproc;
	cgen_proc_destroy(cgproc);
	if (lblock != NULL)
		ir_lblock_destroy(lblock);
	if (pident != NULL)
		free(pident);
	if (arg_ident != NULL)
		free(arg_ident);
	if (atype != NULL)
		ir_texpr_destroy(atype);
	if (ptype != NULL)
		cgtype_destroy(ptype);
	if (dtype != NULL)
		cgtype_destroy(dtype);
	if (ctype != NULL)
		cgtype_destroy(ctype);
	return rc;
}

/** Generate code for type definition.
 *
 * @param cgen Code generator
 * @param atok Type definition token (for diagnostics)
 * @param idlist Init-declarator list
 * @param btype Type derived from declaration specifiers
 * @return EOK on success or an error code
 */
static int cgen_typedef(cgen_t *cgen, ast_tok_t *dtok, ast_idlist_t *idlist,
    cgtype_t *btype)
{
	ast_idlist_entry_t *idle;
	scope_member_t *member;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	cgtype_t *dtype = NULL;
	cgtype_enum_t *tenum;
	int rc;

	/* For all init-declarator list entries */
	idle = ast_idlist_first(idlist);
	while (idle != NULL) {
		/* Mark enum as named, because it has an instance. */
		if (btype->ntype == cgn_enum) {
			tenum = (cgtype_enum_t *)btype;
			tenum->cgenum->named = true;
		}

		/* Process declarator */
		rc = cgen_decl(cgen, btype, idle->decl, idle->aslist,
		    &dtype);
		if (rc != EOK)
			goto error;

		if (idle->decl->ntype == ant_dnoident) {
			cgen_warn_useless_type(cgen, dtok);
			cgtype_destroy(dtype);
			dtype = NULL;

			idle = ast_idlist_next(idle);
			break;
		}

		atok = ast_decl_get_ident(idle->decl);
		ctok = (comp_tok_t *)atok->data;

		/* Non-global scope? */
		if (cgen->cur_scope->parent != NULL) {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Type definition in a "
			    "non-global scope.\n");
			++cgen->warnings;

			/* Check for shadowing a wider-scope identifier */
			member = scope_lookup(cgen->cur_scope->parent,
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
		rc = scope_insert_tdef(cgen->cur_scope, &ctok->tok, dtype);
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

	if (idle != NULL) {
		/* This means we have a dnoident followed by comma */
		ctok = (comp_tok_t *)idle->tcomma.data;
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Declarator expected before ','.\n");
		cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
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
 * @param sctype Storace class specifier type
 * @param gdecln Global declaration that is a function definition
 * @return EOK on success or an error code
 */
static int cgen_fundecl(cgen_t *cgen, cgtype_t *ftype, ast_sclass_type_t sctype,
    ast_gdecln_t *gdecln)
{
	ast_tok_t *aident;
	comp_tok_t *ident;
	ast_tok_t *atok;
	comp_tok_t *tok;
	symbol_t *symbol;
	cgtype_t *ctype = NULL;
	cgtype_func_t *dtfunc;
	char *pident = NULL;
	bool vstatic = false;
	bool old_static;
	bool vextern = false;
	bool old_extern;
	int rc;

	aident = ast_gdecln_get_ident(gdecln);
	ident = (comp_tok_t *) aident->data;

	if (sctype == asc_static) {
		vstatic = true;
	} else if (sctype == asc_extern) {
		/*
		 * XXX If we knew that we are not in a header,
		 * we could print a warning here (since extern functions should
		 * be declared in a header and non-extern function declarations
		 * should not have extern storage class.
		 */
		vextern = true;
	} else if (sctype != asc_none) {
		atok = ast_tree_first_tok(&gdecln->dspecs->node);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Unimplemented storage class specifier.\n");
		++cgen->warnings;
	}

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	assert(ftype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)ftype->ext;

	if (dtfunc->rtype->ntype == cgn_array) {
		/* Function returning array */
		cgen_error_fun_ret_array(cgen, aident);
		rc = EINVAL;
		goto error;
	}

	/*
	 * All we do here is create a symbol. At the end of processing the
	 * module we will go back and find all declared, but not defined,
	 * functions and create extern declarations for them. We will insert
	 * those at the beginning of the IR module.
	 */

	/* The function can already be declared or even defined */
	symbol = symbols_lookup(cgen->symbols, ident->tok.text);
	if (symbol == NULL) {
		rc = symbols_insert(cgen->symbols, st_fun, ident, pident,
		    &symbol);
		if (rc != EOK)
			goto error;

		if (vstatic)
			symbol->flags |= sf_static;
		if (vextern)
			symbol->flags |= sf_extern;

		rc = cgtype_clone(ftype, &symbol->cgtype);
		if (rc != EOK)
			goto error;

		/* Insert identifier into module scope */
		rc = scope_insert_gsym(cgen->scope, &ident->tok, ftype, symbol);
		if (rc == ENOMEM)
			goto error;
	} else {
		if (symbol->stype != st_fun) {
			/* Already declared as a different type of symbol */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": '%s' already declared as a "
			    "different type of symbol.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		/* Create composite type */
		rc = cgtype_compose(symbol->cgtype, ftype, &ctype);
		if (rc == EINVAL) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Conflicting type '");
			cgtype_print(ftype, stderr);
			fprintf(stderr, "' for '%s', previously "
			    "declared as '", ident->tok.text);
			cgtype_print(symbol->cgtype, stderr);
			fprintf(stderr, "'.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}
		if (rc != EOK)
			goto error;

		cgtype_destroy(symbol->cgtype);
		symbol->cgtype = ctype;

		if ((symbol->flags & sf_defined) != 0) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Declaration of '%s' follows definition.\n",
			    ident->tok.text);
			++cgen->warnings;
		} else {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Multiple declarations of '%s'.\n",
			    ident->tok.text);
			++cgen->warnings;
		}

		/* Check if static did not change */
		old_static = (symbol->flags & sf_static) != 0;
		if (vstatic && !old_static) {
			/* Non-static previously declared as static */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Static '%s' was previously "
			    "declared as non-static.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		} else if (!vstatic && old_static) {
			/* Non-static previously declared as static */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: non-static '%s' was "
			    "previously declared as static.\n",
			    ident->tok.text);
			++cgen->warnings;
		}

		/* Check if extern did not change */
		old_extern = (symbol->flags & sf_extern) != 0;
		if (vextern && !old_extern) {
			/* Non-extern previously declared as extern */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Extern '%s' was previously "
			    "declared as non-extern.\n", ident->tok.text);
			++cgen->warnings;
		} else if (!vextern && old_extern) {
			/* Non-extern previously declared as extern */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: non-extern '%s' was "
			    "previously declared as extern.\n",
			    ident->tok.text);
			++cgen->warnings;
		}
	}

	free(pident);
	return EOK;
error:
	if (pident != NULL)
		free(pident);
	return rc;
}

/** Create designated initializer.
 *
 * @param rinit Place to store pointer to new initializer
 * @return EOK on success or an error code
 */
static int cgen_init_create(cgen_init_t **rinit)
{
	cgen_init_t *init;
	int rc;

	init = calloc(1, sizeof(cgen_init_t));
	if (init == NULL)
		return ENOMEM;

	rc = ir_dblock_create(&init->dblock);
	if (rc != EOK) {
		free(init);
		return ENOMEM;
	}

	list_initialize(&init->inits);
	init->next = 0;
	init->next_elem = NULL;
	*rinit = init;
	return EOK;
}

/** Get first child initializer.
 *
 * @param parent Parent initializer
 * @return First child initializer
 */
static cgen_init_t *cgen_init_first(cgen_init_t *parent)
{
	link_t *link;

	link = list_first(&parent->inits);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_init_t, linits);
}

/** Get next child initializer.
 *
 * @param cur Current child initializer
 * @return Next child initializer
 */
static cgen_init_t *cgen_init_next(cgen_init_t *cur)
{
	link_t *link;

	link = list_next(&cur->linits, &cur->parent->inits);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_init_t, linits);
}

/** Get last child initializer.
 *
 * @param parent Parent initializer
 * @return First child initializer
 */
static cgen_init_t *cgen_init_last(cgen_init_t *parent)
{
	link_t *link;

	link = list_last(&parent->inits);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_init_t, linits);
}

/** Get previous child initializer.
 *
 * @param cur Current child initializer
 * @return Previous child initializer
 */
static cgen_init_t *cgen_init_prev(cgen_init_t *cur)
{
	link_t *link;

	link = list_prev(&cur->linits, &cur->parent->inits);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_init_t, linits);
}

/** Destroy initializer.
 *
 * @param init Initializer
 */
static void cgen_init_destroy(cgen_init_t *init)
{
	cgen_init_t *child;

	if (init->parent != NULL)
		list_remove(&init->linits);
	ir_dblock_destroy(init->dblock);

	child = cgen_init_first(init);
	while (child != NULL) {
		cgen_init_destroy(child);
		child = cgen_init_first(init);
	}

	free(init);
}

/** Insert new initializer.
 *
 * @param parent Parent initializer
 * @param etype Element type
 * @param dsg Designator or zero
 * @param relem Which record element this is
 * @param rinit Place to store pointer to new intializer
 * @return EOK on success or an error code
 */
static int cgen_init_insert(cgen_init_t *parent, cgtype_t *etype, uint64_t dsg,
    cgen_rec_elem_t *relem, cgen_init_t **rinit)
{
	cgen_init_t *init;
	cgen_init_t *old;
	cgtype_record_t *trecord;
	int rc;

	old = cgen_init_last(parent);
	while (old != NULL && dsg < old->dsg)
		old = cgen_init_prev(old);

	if (old != NULL && dsg == old->dsg) {
		parent->next = dsg + 1;
		if (relem != NULL)
			parent->next_elem = cgen_record_next(relem);
		*rinit = old;
		return EOK;
	}

	rc = cgen_init_create(&init);
	if (rc != EOK)
		return rc;

	init->dsg = dsg;
	init->parent = parent;
	if (etype->ntype == cgn_record) {
		trecord = (cgtype_record_t *)etype->ext;
		init->next_elem = cgen_record_first(trecord->record);
	} else {
		init->next_elem = NULL;
	}

	if (old != NULL)
		list_insert_after(&init->linits, &old->linits);
	else
		list_prepend(&init->linits, &parent->inits);

	parent->next = dsg + 1;
	if (relem != NULL)
		parent->next_elem = cgen_record_next(relem);
	*rinit = init;
	return EOK;
}

/** Look up (designated) initializer.
 *
 * @param cgen Code generator
 * @param parent Parent initializer
 * @param cgtype Type of variable being initialized
 * @param elem Initializer element
 * @param rcgtype Place to store pointer to type of field
 * @param rinit Place to store pointer to new initializer
 * @return EOK on success, EDOM if this initializer should be skipped hoping
 *         to apply it in the parent scope, or other error code on error.
 */
static int cgen_init_lookup(cgen_t *cgen, cgen_init_t *parent, cgtype_t *cgtype,
    ast_cinit_elem_t *elem, cgtype_t **rcgtype, cgen_init_t **rinit)
{
	ast_cinit_acc_t *acc;
	comp_tok_t *tassign;
	comp_tok_t *ctok;
	cgen_init_t *pinit;
	cgen_init_t *init = NULL;
	cgen_init_t *old_init;
	cgen_eres_t eres;
	cgtype_array_t *tarray;
	cgtype_record_t *trecord;
	cgen_rec_elem_t *relem;
	int64_t dsg;
	uint64_t udsg;
	bool first;
	int rc;

	pinit = parent;
	cgen_eres_init(&eres);

	first = true;
	acc = ast_cinit_elem_first(elem);
	while (acc != NULL) {
		tassign = (comp_tok_t *)elem->tassign.data;
		relem = NULL;

		switch (acc->atype) {
		case aca_index:
			if (cgtype->ntype != cgn_array) {
				if (parent->parent != NULL) {
					/* Try going one level up. */
					rc = EDOM;
					goto error;
				}
				lexer_dprint_tok(&tassign->tok, stderr);
				fprintf(stderr, ": Array index in non-array "
				    "initializer.\n");
				cgen->error = true; // TODO
				rc = EINVAL;
				goto error;
			}
			tarray = (cgtype_array_t *)cgtype->ext;
			rc = cgen_intexpr_val(cgen, acc->index, &eres);
			if (rc != EOK)
				goto error;
			assert(eres.cvknown);
			dsg = eres.cvint;
			if (dsg < 0 || (tarray->have_size &&
			    dsg >= (int64_t)tarray->asize)) {
				lexer_dprint_tok(&tassign->tok, stderr);
				fprintf(stderr, ": Array index exceeds array "
				    "bounds.\n");
				cgen->error = true; // TODO
				rc = EINVAL;
				goto error;
			}
			/*
			 * The topmost designator needs to update the current
			 * designator used for non-designated fields.
			 */
			if (first) {
				parent->next = dsg;
			}
			cgtype = tarray->etype;
			break;
		case aca_member:
			dsg = 0;
			if (cgtype->ntype != cgn_record) {
				if (parent->parent != NULL) {
					/* Try going one level up. */
					rc = EDOM;
					goto error;
				}
				lexer_dprint_tok(&tassign->tok, stderr);
				fprintf(stderr, ": Member access in non-record "
				    "initializer.\n");
				cgen->error = true; // TODO
				rc = EINVAL;
				goto error;
			}
			ctok = (comp_tok_t *)acc->tmember.data;
			trecord = (cgtype_record_t *)cgtype->ext;
			relem = cgen_record_elem_find(trecord->record,
			    ctok->tok.text, &udsg);
			if (relem == NULL) {
				if (parent->parent != NULL) {
					/* Try going one level up. */
					rc = EDOM;
					goto error;
				}
				lexer_dprint_tok(&tassign->tok, stderr);
				fprintf(stderr, ": Record type ");
				(void)cgtype_print(cgtype, stderr);
				fprintf(stderr, " has no member named '%s'.\n",
				    ctok->tok.text);
				cgen->error = true; // TODO
				rc = EINVAL;
				goto error;
			}
			dsg = (int64_t)udsg;

			/*
			 * If it's a union and there is already some
			 * initialized field.
			 */
			old_init = cgen_init_first(parent);
			if (trecord->record->rtype == cgr_union &&
			    old_init != NULL) {
				ctok = (comp_tok_t *)acc->tmember.data;
				cgen_warn_init_field_overwritten(cgen, ctok);

				/* Remove previous initializer */
				cgen_init_destroy(old_init);
			}

			/*
			 * The topmost designator needs to update the current
			 * record element used for non-designated fields.
			 */
			if (first)
				parent->next_elem = relem;

			cgtype = relem->cgtype;
			break;
		}

		/* Find or create initializer. */
		rc = cgen_init_insert(pinit, cgtype, dsg, relem, &init);
		if (rc != EOK)
			goto error;

		first = false;
		pinit = init;
		acc = ast_cinit_elem_next(acc);
	}

	if (init == NULL) {
		/* There were no designators. */

		/*
		 * Determine element type.
		 */
		if (cgtype->ntype == cgn_array) {
			tarray = (cgtype_array_t *)cgtype->ext;
			cgtype = tarray->etype;
		} else {
			assert(cgtype->ntype == cgn_record);

			/*
			 * If parent->next_elem == NULL, it is an excess
			 * initializer, which will be caught later on.
			 */
			if (parent->next_elem != NULL)
				cgtype = parent->next_elem->cgtype;
		}

		rc = cgen_init_insert(parent, cgtype, parent->next,
		    parent->next_elem,
		    &init);
		if (rc != EOK)
			goto error;

		cgtype = NULL;
	}

	cgen_eres_fini(&eres);
	*rcgtype = cgtype;
	*rinit = init;
	return EOK;
error:
	cgen_eres_fini(&eres);
	return rc;
}

/** Generate zero-filled data block of specified size.
 *
 * @param cgen Code generator
 * @param nbytes Number of bytes
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_uninit_zeros(cgen_t *cgen, size_t nbytes, ir_dblock_t *dest)
{
	ir_dentry_t *dentry = NULL;
	size_t i;
	int rc;

	(void)cgen;

	for (i = 0; i < nbytes; i++) {
		rc = ir_dentry_create_int(8, 0, &dentry);
		if (rc != EOK)
			goto error;

		rc = ir_dblock_append(dest, dentry);
		if (rc != EOK)
			goto error;

		dentry = NULL;
	}

	return EOK;
error:
	ir_dentry_destroy(dentry);
	return rc;
}

/** Generate zero-filled data block for uninitialized array.
 *
 * @param cgen Code generator
 * @param tarray Array type
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_uninit_digest_array(cgen_t *cgen, cgtype_array_t *tarray,
    ir_dblock_t *dest)
{
	uint64_t i;
	int rc;

	assert(tarray->have_size);
	for (i = 0; i < tarray->asize; i++) {
		rc = cgen_uninit_digest(cgen, tarray->etype, dest);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Generate zero-filled data block for uninitialized record.
 *
 * @param cgen Code generator
 * @param trecord Record type
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_uninit_digest_record(cgen_t *cgen, cgtype_record_t *trecord,
    ir_dblock_t *dest)
{
	cgen_rec_elem_t *elem;
	int rc;

	elem = cgen_record_first(trecord->record);
	while (elem != NULL) {
		rc = cgen_uninit_digest(cgen, elem->cgtype, dest);
		if (rc != EOK)
			return rc;

		elem = cgen_record_next(elem);
	}

	return EOK;
}

/** Generate zero-filled data block for uninitialized basic type.
 *
 * @param cgen Code generator
 * @param tbasic Basic type
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_uninit_digest_basic(cgen_t *cgen, cgtype_basic_t *tbasic,
    ir_dblock_t *dest)
{
	unsigned bits;
	ir_dentry_t *dentry = NULL;
	int rc;

	bits = cgen_basic_type_bits(cgen, tbasic);
	assert(bits != 0);

	rc = ir_dentry_create_int(bits, 0, &dentry);
	if (rc != EOK)
		goto error;

	rc = ir_dblock_append(dest, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	ir_dentry_destroy(dentry);
	return rc;
}

/** Generate zero-filled data block for uninitialized pointer type.
 *
 * @param cgen Code generator
 * @param tpointer Pointer type
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_uninit_digest_pointer(cgen_t *cgen, cgtype_pointer_t *tpointer,
    ir_dblock_t *dest)
{
	ir_dentry_t *dentry = NULL;
	int rc;

	(void)cgen;
	(void)tpointer;

	rc = ir_dentry_create_int(cgen_pointer_bits, 0, &dentry);
	if (rc != EOK)
		goto error;

	rc = ir_dblock_append(dest, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	ir_dentry_destroy(dentry);
	return rc;
}

/** Generate zero-filled data block for uninitialized enum type.
 *
 * @param cgen Code generator
 * @param tenum Enum type
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_uninit_digest_enum(cgen_t *cgen, cgtype_enum_t *tenum,
    ir_dblock_t *dest)
{
	ir_dentry_t *dentry = NULL;
	int rc;

	(void)cgen;
	(void)tenum;

	rc = ir_dentry_create_int(cgen_enum_bits, 0, &dentry);
	if (rc != EOK)
		goto error;

	rc = ir_dblock_append(dest, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	ir_dentry_destroy(dentry);
	return rc;
}

/** Generate zero-filled data block for uninitialized variable.
 *
 * @param cgen Code generator
 * @param cgtype Variable type
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_uninit_digest(cgen_t *cgen, cgtype_t *cgtype, ir_dblock_t *dest)
{
	cgtype_array_t *tarray;
	cgtype_basic_t *tbasic;
	cgtype_pointer_t *tpointer;
	cgtype_record_t *trecord;
	cgtype_enum_t *tenum;

	switch (cgtype->ntype) {
	case cgn_array:
		tarray = (cgtype_array_t *)cgtype->ext;
		return cgen_uninit_digest_array(cgen, tarray, dest);
	case cgn_basic:
		tbasic = (cgtype_basic_t *)cgtype->ext;
		return cgen_uninit_digest_basic(cgen, tbasic, dest);
	case cgn_pointer:
		tpointer = (cgtype_pointer_t *)cgtype->ext;
		return cgen_uninit_digest_pointer(cgen, tpointer, dest);
	case cgn_record:
		trecord = (cgtype_record_t *)cgtype->ext;
		return cgen_uninit_digest_record(cgen, trecord, dest);
	case cgn_enum:
		tenum = (cgtype_enum_t *)cgtype->ext;
		return cgen_uninit_digest_enum(cgen, tenum, dest);
	default:
		assert(false);
	}
}

/** Generate data block for initialized array variable.
 *
 * @param cgen Code generator
 * @param parent Inititializer
 * @param tarray Array type
 * @param lvl Nesting level
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_init_digest_array(cgen_t *cgen, cgen_init_t *parent,
    cgtype_array_t *tarray, int lvl, ir_dblock_t *dest)
{
	cgen_init_t *init;
	uint64_t i;
	int rc;

	if (!tarray->have_size) {
		/* Determine array size */
		init = cgen_init_last(parent);
		if (init != NULL)
			tarray->asize = init->dsg + 1;

		tarray->have_size = true;
	}

	init = cgen_init_first(parent);
	for (i = 0; i < tarray->asize; i++) {
		while (init != NULL && init->dsg < i) {
			init = cgen_init_next(init);
		}

		if (init != NULL && init->dsg == i) {
			/* Initialized field */
			cgen_init_digest(cgen, init, tarray->etype, lvl + 1,
			    dest);
		} else {
			/* Uninitialized field */
			rc = cgen_uninit_digest(cgen, tarray->etype, dest);
			if (rc != EOK)
				return rc;
		}
	}

	return EOK;
}

/** Generate data block for initialized struct variable.
 *
 * @param cgen Code generator
 * @param parent Inititializer
 * @param trecord Record type
 * @param lvl Nesting level
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_init_digest_struct(cgen_t *cgen, cgen_init_t *parent,
    cgtype_record_t *trecord, int lvl, ir_dblock_t *dest)
{
	cgen_init_t *init;
	cgen_rec_elem_t *elem;
	uint64_t i;
	int rc;

	init = cgen_init_first(parent);
	i = 0;
	elem = cgen_record_first(trecord->record);
	while (elem != NULL) {
		while (init != NULL && init->dsg < i) {
			init = cgen_init_next(init);
		}

		if (init != NULL && init->dsg == i) {
			/* Initialized field */
			cgen_init_digest(cgen, init, elem->cgtype, lvl + 1,
			    dest);
		} else {
			/* Uninitialized field */
			rc = cgen_uninit_digest(cgen, elem->cgtype, dest);
			if (rc != EOK)
				return rc;
		}

		++i;
		elem = cgen_record_next(elem);
	}

	return EOK;
}

/** Generate data block for initialized union variable.
 *
 * @param cgen Code generator
 * @param parent Inititializer
 * @param trecord Record type
 * @param lvl Nesting level
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_init_digest_union(cgen_t *cgen, cgen_init_t *parent,
    cgtype_record_t *trecord, int lvl, ir_dblock_t *dest)
{
	cgen_init_t *init;
	cgen_rec_elem_t *elem;
	size_t usize;
	size_t esize;
	uint64_t i;
	int rc;

	init = cgen_init_first(parent);
	i = 0;
	elem = cgen_record_first(trecord->record);
	while (elem != NULL) {
		while (init != NULL && init->dsg < i) {
			init = cgen_init_next(init);
		}

		if (init != NULL && init->dsg == i) {
			/* Initialized field */
			cgen_init_digest(cgen, init, elem->cgtype, lvl + 1,
			    dest);
			break;
		}

		++i;
		elem = cgen_record_next(elem);
	}

	if (init != NULL) {
		/*
		 * Pad the initialized field zith zeros to the size
		 * of the union.
		 */
		esize = cgen_type_sizeof(cgen, elem->cgtype);
		usize = cgen_record_size(cgen, trecord->record);

		assert(usize >= esize);
		rc = cgen_uninit_zeros(cgen, usize - esize, dest);
		if (rc != EOK)
			return rc;
	} else {
		/* Union is not initialized. Fill it with zeros. */
		usize = cgen_record_size(cgen, trecord->record);
		rc = cgen_uninit_zeros(cgen, usize, dest);
		if (rc != EOK)
			return rc;

	}

	return EOK;
}

/** Generate data block for initialized variable.
 *
 * @param cgen Code generator
 * @param parent Inititializer
 * @param cgtype Variable type
 * @param lvl Nesting level
 * @param dest Destination data block
 * @return EOK on success or an error code
 */
static int cgen_init_digest(cgen_t *cgen, cgen_init_t *parent, cgtype_t *cgtype,
    int lvl, ir_dblock_t *dest)
{
	cgtype_array_t *tarray;
	cgtype_record_t *trecord;
	int rc;

	if (cgtype->ntype == cgn_array) {
		tarray = (cgtype_array_t *)cgtype->ext;
		rc = cgen_init_digest_array(cgen, parent, tarray, lvl, dest);
		if (rc != EOK)
			return rc;
	} else if (cgtype->ntype == cgn_record) {
		trecord = (cgtype_record_t *)cgtype->ext;
		if (trecord->record->rtype == cgr_struct) {
			rc = cgen_init_digest_struct(cgen, parent, trecord,
			    lvl, dest);
			if (rc != EOK)
				return rc;
		} else {
			rc = cgen_init_digest_union(cgen, parent, trecord,
			    lvl, dest);
			if (rc != EOK)
				return rc;
		}
	}

	ir_dblock_transfer_to_end(parent->dblock, dest);
	return EOK;
}

/** Generate data entries for initializing a scalar type.
 *
 * @param cgen Code generator
 * @param tbasic Basic variable type
 * @param itok Initialization token (for printing diagnostics)
 * @param init Initializer or @c NULL
 * @param dblock Data block to which data should be appended
 * @return EOK on success or an error code
 */
static int cgen_init_dentries_scalar(cgen_t *cgen, cgtype_t *stype,
    comp_tok_t *itok, ast_node_t *init, ir_dblock_t *dblock)
{
	ir_dentry_t *dentry = NULL;
	cgen_eres_t eres;
	ast_cinit_t *cinit;
	ast_cinit_elem_t *elem;
	cgtype_basic_t *tbasic;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	unsigned bits;
	symbol_t *initsym;
	int64_t initval;
	int rc;

	cgen_eres_init(&eres);

	if (init != NULL) {
		if (init->ntype == ant_cinit) {
			cinit = (ast_cinit_t *)init->ext;

			ctok = (comp_tok_t *)cinit->tlbrace.data;
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Warning: Excess braces around "
			    "scalar initializer.\n");
			++cgen->warnings;

			elem = ast_cinit_first(cinit);

			rc = cgen_init_dentries_scalar(cgen, stype, itok,
			    elem->init, dblock);
			if (rc != EOK)
				goto error;

			elem = ast_cinit_next(elem);
			if (elem != NULL) {
				atok = ast_tree_first_tok(elem->init);
				ctok = (comp_tok_t *)atok->data;
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Excess initializer.\n'");
				cgen->error = true; // XXX
				return EINVAL;
			}
			return EOK;
		} else {
			rc = cgen_constexpr_val(cgen, init, itok, stype, &eres);
			if (rc != EOK)
				goto error;
		}

		/* Initializer already has valid data? */
		if (ir_dblock_first(dblock) != NULL) {
			atok = ast_tree_first_tok(init);
			ctok = (comp_tok_t *)atok->data;
			cgen_warn_init_field_overwritten(cgen, ctok);

			/*
			 * Empty the block so that it can be filled with
			 * new data.
			 */
			ir_dblock_empty(dblock);
		}

		initval = eres.cvint;
		initsym = eres.cvsymbol;
	} else {
		initval = 0;
		initsym = NULL;
	}

	if (stype->ntype == cgn_basic) {
		tbasic = (cgtype_basic_t *)stype->ext;

		bits = cgen_basic_type_bits(cgen, tbasic);
		if (bits == 0) {
			fprintf(stderr, "Unimplemented variable type.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}

		rc = ir_dentry_create_int(bits, initval, &dentry);
		if (rc != EOK)
			goto error;

		rc = ir_dblock_append(dblock, dentry);
		if (rc != EOK)
			goto error;
	} else if (stype->ntype == cgn_pointer) {

		if (initsym != NULL) {
			rc = ir_dentry_create_ptr(cgen_pointer_bits,
			    initsym->irident, initval, &dentry);
			if (rc != EOK)
				goto error;
		} else {
			rc = ir_dentry_create_int(cgen_pointer_bits, initval,
			    &dentry);
			if (rc != EOK)
				goto error;
		}

		rc = ir_dblock_append(dblock, dentry);
		if (rc != EOK)
			goto error;

		dentry = NULL;
	} else if (stype->ntype == cgn_enum) {
		rc = ir_dentry_create_int(cgen_enum_bits, initval, &dentry);
		if (rc != EOK)
			goto error;

		rc = ir_dblock_append(dblock, dentry);
		if (rc != EOK)
			goto error;

		dentry = NULL;
	} else {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	dentry = NULL;
	cgen_eres_fini(&eres);
	return EOK;
error:
	cgen_eres_fini(&eres);
	ir_dentry_destroy(dentry);
	return rc;
}

/** Generate data entries for initializing an array type.
 *
 * @param cgen Code generator
 * @param tarray Array type
 * @param itok Initialization token (for printing diagnostics)
 * @param elem Compound initializer element pointer
 * @param parent Initializer
 * @return EOK on success or an error code
 */
static int cgen_init_dentries_array(cgen_t *cgen, cgtype_array_t *tarray,
    comp_tok_t *itok, ast_cinit_elem_t **elem, cgen_init_t *parent)
{
	uint64_t i;
	uint64_t dsg;
	cgen_init_t *init = NULL;
	cgtype_t *cgtype;
	size_t entries;
	int rc;

	i = 0;
	entries = 0;
	while (*elem != NULL) {
		dsg = parent->next;
		rc = cgen_init_lookup(cgen, parent, &tarray->cgtype, *elem,
		    &cgtype, &init);
		/* EDOM means break and try this initializer in outer scope */
		if (rc == EDOM)
			return EOK;
		if (rc != EOK)
			goto error;

		/* No designators? */
		if (cgtype == NULL) {
			cgtype = tarray->etype;
			if (tarray->have_size && dsg >= tarray->asize)
				return EOK;
		}

		rc = cgen_init_dentries_cinit(cgen, cgtype, itok,
		    elem, init);
		if (rc != EOK)
			goto error;
		++i;
		++entries;
	}

	return EOK;
error:
	return rc;
}

/** Generate data entries for initializing a record type.
 *
 * @param cgen Code generator
 * @param trecord Record type
 * @param itok Initialization token (for printing diagnostics)
 * @param elem Compound initializer element pointer
 * @param parent Initializer
 * @return EOK on success or an error code
 */
static int cgen_init_dentries_record(cgen_t *cgen, cgtype_record_t *trecord,
    comp_tok_t *itok, ast_cinit_elem_t **elem, cgen_init_t *parent)
{
	uint64_t i;
	cgen_init_t *init = NULL;
	cgtype_t *cgtype;
	cgen_rec_elem_t *relem;
	int rc;

	i = 0;
	while (*elem != NULL) {
		relem = parent->next_elem;

		rc = cgen_init_lookup(cgen, parent, &trecord->cgtype, *elem,
		    &cgtype, &init);
		/* EDOM means break and try this initializer in outer scope */
		if (rc == EDOM)
			return EOK;
		if (rc != EOK)
			goto error;

		/* No designators? */
		if (cgtype == NULL) {
			/* Ran out of record elements? */
			if (relem == NULL) {
				return EOK;
			}
			cgtype = relem->cgtype;
			/*
			 * Only the first union element can be initialized
			 * without a designator.
			 */
			if (trecord->record->rtype == cgr_union &&
			    relem != cgen_record_first(trecord->record)) {
				return EOK;
			}
		}

		rc = cgen_init_dentries_cinit(cgen, cgtype, itok,
		    elem, init);
		if (rc != EOK)
			goto error;
		++i;
	}

	return EOK;
error:
	return rc;
}

/** Generate data entries for initializing a CG type from a compound initializer
 * element.
 *
 * @param cgen Code generator
 * @param stype Variable type
 * @param itok Initialization token (for printing diagnostics)
 * @param elem Compound initializer element pointer
 * @param dblock Data block to which data should be appended
 * @return EOK on success or an error code
 */
static int cgen_init_dentries_cinit(cgen_t *cgen, cgtype_t *stype, comp_tok_t *itok,
    ast_cinit_elem_t **elem, cgen_init_t *parent)
{
	ir_dentry_t *dentry = NULL;
	ir_texpr_t *vtype = NULL;
	ast_node_t *init;
	cgtype_record_t *cgrec;
	cgtype_array_t *cgarr;
	ast_cinit_elem_t *melem;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	int rc;

	if (stype->ntype == cgn_array || stype->ntype == cgn_record) {
		if (*elem != NULL && (*elem)->init->ntype == ant_cinit) {
			melem = ast_cinit_first((ast_cinit_t *)(*elem)->init->ext);

			if (stype->ntype == cgn_array) {
				cgarr = (cgtype_array_t *)stype->ext;

				rc = cgen_init_dentries_array(cgen, cgarr, itok,
				    &melem, parent);
				if (rc != EOK)
					goto error;
			} else {
				cgrec = (cgtype_record_t *)stype->ext;

				rc = cgen_init_dentries_record(cgen, cgrec,
				    itok, &melem, parent);
				if (rc != EOK)
					goto error;

			}

			if (melem != NULL) {
				atok = ast_tree_first_tok(melem->init);
				ctok = (comp_tok_t *)atok->data;
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Excess initializer.\n'");
				cgen->error = true; // XXX
				return EINVAL;
			}
			*elem = ast_cinit_next(*elem);
		} else {
			if (*elem != NULL) {
				atok = ast_tree_first_tok((*elem)->init);
				ctok = (comp_tok_t *)atok->data;
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Warning: Initialization is not "
				    "fully bracketed.\n");
				++cgen->warnings;
			}

			if (stype->ntype == cgn_array) {
				cgarr = (cgtype_array_t *)stype->ext;

				rc = cgen_init_dentries_array(cgen, cgarr, itok,
				    elem, parent);
				if (rc != EOK)
					goto error;
			} else {
				cgrec = (cgtype_record_t *)stype->ext;

				rc = cgen_init_dentries_record(cgen, cgrec,
				    itok, elem, parent);
				if (rc != EOK)
					goto error;
			}
		}
	} else {
		/* Scalar type */
		if (*elem != NULL)
			init = (*elem)->init;
		else
			init = NULL;

		rc = cgen_init_dentries_scalar(cgen, stype, itok, init,
		    parent->dblock);
		if (rc != EOK)
			goto error;

		if (*elem != NULL)
			*elem = ast_cinit_next(*elem);
	}

	return EOK;
error:
	ir_dentry_destroy(dentry);
	ir_texpr_destroy(vtype);
	return rc;
}

/** Generate data entries for initializing a CG type from a string initializer
 * element.
 *
 * @param cgen Code generator
 * @param stype Variable type
 * @param itok Initialization token (for printing diagnostics)
 * @param estring String initializer
 * @param dblock Data block to which data should be appended
 * @return EOK on success or an error code
 */
static int cgen_init_dentries_string(cgen_t *cgen, cgtype_t *stype,
    comp_tok_t *itok, ast_estring_t *estring, ir_dblock_t *dblock)
{
	ast_estring_lit_t *lit;
	ir_dentry_t *dentry = NULL;
	comp_tok_t *ctok;
	const char *text;
	cgtype_array_t *tarray;
	cgtype_int_rank_t rrank;
	uint32_t value;
	uint32_t max;
	uint64_t idx;
	bool wide = false;
	int rc;

	(void)itok;

	if (stype->ntype != cgn_array) {
		fprintf(stderr, ": Cannot initialize variable of type ");
		(void) cgtype_print(stype, stderr);
		fprintf(stderr, " from (wide) string.\n");
		cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	tarray = (cgtype_array_t *)stype->ext;
	if (!cgen_type_is_integer(cgen, tarray->etype)) {
		fprintf(stderr, ": Cannot initialize array of ");
		(void) cgtype_print(tarray->etype, stderr);
		fprintf(stderr, " from (wide) string.\n");
		cgen->error = true; // XXX
		rc = EINVAL;
		goto error;
	}

	idx = 0;
	lit = ast_estring_first(estring);
	while (lit != NULL) {
		wide = false;

		ctok = (comp_tok_t *)lit->tlit.data;
		text = ctok->tok.text;
		if (*text == 'L' && text[1] == '"') {
			++text;
			wide = true;
		}

		max = wide ? cgen_lchar_max : cgen_char_max;
		rrank = wide ? cgir_int : cgir_char;

		if (cgtype_int_rank(tarray->etype) != rrank) {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Cannot initialize array of ");
			(void) cgtype_print(tarray->etype, stderr);
			fprintf(stderr, " from %s.\n", wide ? "wide string" :
			    "string");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		if (*text != '"') {
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": String constant expected.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		++text;

		while (*text != '"') {
			if (*text == '\0') {
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Unexpected end of string literal.\n'");
				cgen->error = true; // XXX
				rc = EINVAL;
				goto error;
			}

			if (*text == '\\') {
				rc = cgen_escseq(cgen, ctok, &text,
				    max, &value);
				if (rc != EOK)
					goto error;
			} else {
				value = *text;
				++text;
			}

			if (tarray->have_size && idx >= tarray->asize) {
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Excess initializer "
				    "characters in string.\n");
				cgen->error = true; // XXX
				rc = EINVAL;
				goto error;
			}

			rc = ir_dentry_create_int(wide ? cgen_lchar_bits :
			    cgen_char_bits, value, &dentry);
			if (rc != EOK)
				goto error;

			rc = ir_dblock_append(dblock, dentry);
			if (rc != EOK)
				goto error;

			dentry = NULL;
			++idx;
		}

		lit = ast_estring_next(lit);
	}

	/* Fill in array size now if not known. */
	if (!tarray->have_size) {
		tarray->have_size = true;
		tarray->asize = idx + 1;
	}

	/* Pad with zeroes up to the size of the array */
	while (idx < tarray->asize) {
		rc = ir_dentry_create_int(wide ? cgen_lchar_bits :
		    cgen_char_bits, 0, &dentry);
		if (rc != EOK)
			goto error;

		rc = ir_dblock_append(dblock, dentry);
		if (rc != EOK)
			goto error;

		++idx;
	}

	return EOK;
error:
	ir_dentry_destroy(dentry);
	return rc;
}

/** Generate data entries for initializing a CG type.
 *
 * @param cgen Code generator
 * @param stype Variable type
 * @param itok Initialization token (for printing diagnostics)
 * @param init Initializer or @c NULL
 * @param dblock Data block to which data should be appended
 * @return EOK on success or an error code
 */
static int cgen_init_dentries(cgen_t *cgen, cgtype_t *stype, comp_tok_t *itok,
    ast_node_t *init, ir_dblock_t *dblock)
{
	ir_dentry_t *dentry = NULL;
	ir_texpr_t *vtype = NULL;
	cgtype_record_t *cgrec;
	ast_cinit_elem_t *celem;
	cgtype_array_t *cgarr;
	ast_tok_t *atok;
	comp_tok_t *ctok;
	cgen_init_t *parent;
	int rc;

	rc = cgen_init_create(&parent);
	if (rc != EOK)
		return rc;

	if (stype->ntype == cgn_array || stype->ntype == cgn_record) {
		if (init == NULL || init->ntype == ant_cinit) {
			celem = init != NULL ?
			    ast_cinit_first((ast_cinit_t *)init->ext) : NULL;

			if (stype->ntype == cgn_array) {
				cgarr = (cgtype_array_t *)stype->ext;

				rc = cgen_init_dentries_array(cgen, cgarr, itok,
				    &celem, parent);
				if (rc != EOK)
					goto error;
			} else {
				cgrec = (cgtype_record_t *)stype->ext;
				parent->next_elem = cgen_record_first(
				    cgrec->record);

				rc = cgen_init_dentries_record(cgen, cgrec,
				    itok, &celem, parent);
				if (rc != EOK)
					goto error;
			}

			if (celem != NULL) {
				atok = ast_tree_first_tok(celem->init);
				ctok = (comp_tok_t *)atok->data;
				lexer_dprint_tok(&ctok->tok, stderr);
				fprintf(stderr, ": Excess initializer.\n'");
				cgen->error = true; // XXX
				return EINVAL;
			}
		} else if (init->ntype == ant_estring) {
			rc = cgen_init_dentries_string(cgen, stype, itok,
			    (ast_estring_t *)init->ext, parent->dblock);
			if (rc != EOK)
				goto error;
		} else {
			atok = ast_tree_first_tok(init);
			ctok = (comp_tok_t *)atok->data;
			lexer_dprint_tok(&ctok->tok, stderr);
			fprintf(stderr, ": Invalid initializer.\n'");
			cgen->error = true; // XXX
			return EINVAL;
		}
	} else {
		rc = cgen_init_dentries_scalar(cgen, stype, itok, init,
		    parent->dblock);
		if (rc != EOK)
			goto error;
	}

	cgen_init_digest(cgen, parent, stype, 0, dblock);
	cgen_init_destroy(parent);
	return EOK;
error:
	ir_dentry_destroy(dentry);
	ir_texpr_destroy(vtype);
	return rc;
}

/** Generate code for global variable declaration or definition.
 *
 * @param cgen Code generator
 * @param stype Variable type
 * @param sctype Storace class specifier type
 * @param entry Init-declarator list entry that declares a variable
 * @param gdecln Global declaration (for diagnostics)
 * @return EOK on success or an error code
 */
static int cgen_vardef(cgen_t *cgen, cgtype_t *stype, ast_sclass_type_t sctype,
    ast_idlist_entry_t *entry, ast_gdecln_t *gdecln)
{
	ir_var_t *var = NULL;
	ir_dblock_t *dblock = NULL;
	ir_dentry_t *dentry = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	char *pident = NULL;
	char *sident = NULL;
	ir_texpr_t *vtype = NULL;
	symbol_t *symbol;
	scope_member_t *member;
	cgtype_t *ctype = NULL;
	cgtype_enum_t *tenum;
	ast_tok_t *atok;
	comp_tok_t *tok;
	bool vstatic = false;
	bool vextern = false;
	bool old_static;
	bool old_extern;
	int rc;

	aident = ast_decl_get_ident(entry->decl);
	ident = (comp_tok_t *) aident->data;

	if (sctype == asc_static) {
		vstatic = true;
	} else if (sctype == asc_extern) {
		vextern = true;
	} else if (sctype != asc_none) {
		atok = ast_tree_first_tok(&gdecln->dspecs->node);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Warning: Unimplemented storage class specifier.\n");
		++cgen->warnings;
	}

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	/* Mark enum as named, because it has an instance. */
	if (stype->ntype == cgn_enum) {
		tenum = (cgtype_enum_t *)stype;
		tenum->cgenum->named = true;
	}

	symbol = symbols_lookup(cgen->symbols, ident->tok.text);
	if (symbol == NULL) {
		rc = symbols_insert(cgen->symbols, st_var, ident, pident,
		    &symbol);
		if (rc != EOK)
			goto error;

		assert(symbol != NULL);
		if (vstatic)
			symbol->flags |= sf_static;
		if (vextern)
			symbol->flags |= sf_extern;

		rc = cgtype_clone(stype, &ctype);
		if (rc != EOK)
			goto error;
	} else {
		if (symbol->stype != st_var) {
			/* Already declared as a different type of symbol */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": '%s' already declared as a "
			    "different type of symbol.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		rc = cgtype_compose(symbol->cgtype, stype, &ctype);
		if (rc == EINVAL) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Conflicting type '");
			cgtype_print(stype, stderr);
			fprintf(stderr, "' for '%s', previously "
			    "declared as '", ident->tok.text);
			cgtype_print(symbol->cgtype, stderr);
			fprintf(stderr, "'.\n");
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}
		if (rc != EOK)
			goto error;

		if ((symbol->flags & sf_defined) != 0 && entry->init != NULL) {
			/* Already defined */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Redefinition of '%s'.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		}

		if ((symbol->flags & sf_defined) != 0) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Declaration of '%s' follows definition.\n",
			    ident->tok.text);
			++cgen->warnings;
		} else if (entry->init == NULL) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Multiple declarations of '%s'.\n",
			    ident->tok.text);
			++cgen->warnings;
		} else {
			/* Symbol should be member of module scope */
			member = scope_lookup(cgen->cur_scope, ident->tok.text);
			assert(member != NULL);

			if (!member->used) {
				lexer_dprint_tok(&ident->tok, stderr);
				fprintf(stderr, ": Warning: Variable '%s' not "
				    "used since forward declaration.\n",
				    ident->tok.text);
				++cgen->warnings;
			}
		}

		/* Check if static did not change */
		old_static = (symbol->flags & sf_static) != 0;
		if (vstatic && !old_static) {
			/* Non-static previously declared as static */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Static '%s' was previously "
			    "declared as non-static.\n", ident->tok.text);
			cgen->error = true; // XXX
			rc = EINVAL;
			goto error;
		} else if (!vstatic && old_static) {
			/* Non-static previously declared as static */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: non-static '%s' was "
			    "previously declared as static.\n",
			    ident->tok.text);
			++cgen->warnings;
		}

		/* Check if extern did not change */
		old_extern = (symbol->flags & sf_extern) != 0;
		if (vextern && !old_extern) {
			/* Non-extern previously declared as extern */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: Extern '%s' was previously "
			    "declared as non-extern.\n", ident->tok.text);
			++cgen->warnings;
		} else if (!vextern && old_extern && entry->init == NULL) {
			/* Non-extern previously declared as extern */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Warning: non-extern '%s' was "
			    "previously declared as extern.\n",
			    ident->tok.text);
			symbol->flags &= ~sf_extern;
			++cgen->warnings;
		}
	}

	if (entry->init != NULL) {
		/* Mark the symbol as defined */
		symbol->flags |= sf_defined;
		symbol->flags &= ~sf_extern;

		/* Create initialized IR variable */

		rc = ir_dblock_create(&dblock);
		if (rc != EOK)
			goto error;

		rc = ir_var_create(pident, NULL, vstatic ? irl_default :
		    irl_global, dblock, &var);
		if (rc != EOK)
			goto error;

		dblock = NULL;

		/*
		 * Generate data entries. This can also fill in unknown
		 * array sizes in ctype.
		 */

		rc = cgen_init_dentries(cgen, ctype,
		    (comp_tok_t *)entry->tassign.data, entry->init, var->dblock);
		if (rc != EOK)
			goto error;

		/* Now that stype is finalized, generate IR variable type */
		rc = cgen_cgtype(cgen, ctype, &vtype);
		if (rc != EOK)
			goto error;

		var->vtype = vtype;
		vtype = NULL;

		ir_module_append(cgen->irmod, &var->decln);
		var = NULL;
	}

	/* Copy type to symbol */
	if (symbol->cgtype == NULL) {
		rc = cgtype_clone(ctype, &symbol->cgtype);
		if (rc != EOK)
			goto error;
	}

	/* Insert identifier into module scope */
	rc = scope_insert_gsym(cgen->scope, &ident->tok, ctype, symbol);
	if (rc == ENOMEM)
		goto error;

	cgtype_destroy(ctype);
	free(pident);
	return EOK;
error:
	if (sident != NULL)
		free(sident);
	cgtype_destroy(ctype);
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
 * @return EOK on success or an error code
 */
static int cgen_gdecln(cgen_t *cgen, ast_gdecln_t *gdecln)
{
	ast_idlist_entry_t *entry;
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	ast_sclass_type_t sctype;
	cgen_rd_flags_t flags;
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	if (gdecln->body != NULL) {
		/* Function definition was already processed. */
		return EOK;
	}

	/* Process declaration specifiers */
	rc = cgen_dspecs(cgen, gdecln->dspecs, &sctype, &flags, &stype);
	if (rc != EOK)
		goto error;

	if (sctype == asc_typedef) {
		rc = cgen_typedef(cgen,
		    ast_tree_first_tok(&gdecln->dspecs->node),
		    gdecln->idlist, stype);
		if (rc != EOK)
			goto error;
	} else if (gdecln->idlist != NULL) {
		/* Possibly variable declarations */
		entry = ast_idlist_first(gdecln->idlist);
		while (entry != NULL) {
			/* Process declarator */
			rc = cgen_decl(cgen, stype, entry->decl, entry->aslist,
			    &dtype);
			if (rc != EOK)
				goto error;

			if (ast_decl_is_vardecln(entry->decl)) {
				/* Variable declaration */
				rc = cgen_vardef(cgen, dtype, sctype, entry,
				    gdecln);
				if (rc != EOK)
					goto error;
			} else if (entry->decl->ntype == ant_dnoident) {
				if (entry->have_init) {
					tok = (comp_tok_t *)
					    entry->tassign.data;
					lexer_dprint_tok(&tok->tok, stderr);
					fprintf(stderr, ": Unexpected "
					    "initializer.\n");
					cgen->error = true; // XXX
					rc = EINVAL;
					goto error;
				}
				if ((flags & cgrd_ident) == 0) {
					atok = ast_tree_first_tok(&gdecln->dspecs->node);
					cgen_warn_useless_type(cgen, atok);
				}
				if ((flags & cgrd_def) == 0) {
					/* This is a pure struct/union declaration */
					if ((flags & cgrd_prevdef) != 0) {
						atok = ast_tree_first_tok(&gdecln->dspecs->node);
						tok = (comp_tok_t *) atok->data;
						lexer_dprint_tok(&tok->tok, stderr);
						fprintf(stderr, ": Warning: Declaration of '");
						cgtype_print(stype, stderr);
						fprintf(stderr, "' follows definition.\n");
						++cgen->warnings;
					} else if ((flags & cgrd_prevdecl) != 0) {
						atok = ast_tree_first_tok(&gdecln->dspecs->node);
						tok = (comp_tok_t *) atok->data;
						lexer_dprint_tok(&tok->tok, stderr);
						fprintf(stderr, ": Warning: Multiple declarations of '");
						cgtype_print(stype, stderr);
						fprintf(stderr, "'.\n");
						++cgen->warnings;
					}
				}
			} else {
				/* Assuming it's a function declaration */
				rc = cgen_fundecl(cgen, dtype, sctype, gdecln);
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
 * @return EOK on success or an error code
 */
static int cgen_global_decln(cgen_t *cgen, ast_node_t *decln)
{
	ast_tok_t *atok;
	comp_tok_t *tok;
	int rc;

	switch (decln->ntype) {
	case ant_gdecln:
		rc = cgen_gdecln(cgen, (ast_gdecln_t *) decln->ext);
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

/** Generate definition from function symbol declaration.
 *
 * @param cgen Code generator
 * @param symbol Symbol
 * @param ident Identifier torken
 * @param irident IR identifier
 * @param cgtype Function type
 * @return EOK on success or an error code
 */
static int cgen_module_symdecl_fun(cgen_t *cgen, symbol_t *symbol)
{
	int rc;
	ir_proc_t *proc = NULL;
	ir_proc_attr_t *irattr;
	cgtype_func_t *cgfunc;

	rc = ir_proc_create(symbol->irident, irl_extern, NULL, &proc);
	if (rc != EOK)
		goto error;

	rc = cgen_fun_args(cgen, symbol->ident, symbol->cgtype, proc);
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

	ir_module_append(cgen->irmod, &proc->decln);
	proc = NULL;

	return EOK;
error:
	if (proc != NULL)
		ir_proc_destroy(proc);
	return rc;
}

/** Generate definition from variable symbol declaration.
 *
 * @param cgen Code generator
 * @param symbol Symbol
 * @return EOK on success or an error code
 */
static int cgen_module_symdecl_var(cgen_t *cgen, symbol_t *symbol)
{
	int rc;
	ir_proc_t *proc = NULL;
	ir_dblock_t *dblock = NULL;
	ir_dentry_t *dentry = NULL;
	ir_linkage_t linkage;
	ir_texpr_t *vtype = NULL;
	ir_var_t *var = NULL;
	cgtype_t *cgtype = symbol->cgtype;
	unsigned bits;
	unsigned i;

	if (cgen_type_is_incomplete(cgen, cgtype)) {
		lexer_dprint_tok(&symbol->ident->tok, stderr);
		fprintf(stderr, ": Variable has incomplete type.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	if ((symbol->flags & sf_extern) != 0)
		linkage = irl_extern;
	else if ((symbol->flags & sf_static) != 0)
		linkage = irl_default;
	else
		linkage = irl_global;

	if (linkage != irl_extern) {
		rc = ir_dblock_create(&dblock);
		if (rc != EOK)
			goto error;
	}

	rc = cgen_cgtype(cgen, cgtype, &vtype);
	if (rc != EOK)
		goto error;

	rc = ir_var_create(symbol->irident, vtype, linkage, dblock, &var);
	if (rc != EOK)
		goto error;

	vtype = NULL;
	dblock = NULL;

	if (linkage != irl_extern) {
		if (cgtype->ntype == cgn_basic &&
		    ((cgtype_basic_t *)cgtype->ext)->elmtype == cgelm_va_list) {
			/* XXX Ideally use a special va_list initializer */
			for (i = 0; i < 3; i++) {
				rc = ir_dentry_create_int(16, 0, &dentry);
				if (rc != EOK)
					goto error;

				rc = ir_dblock_append(var->dblock, dentry);
				if (rc != EOK)
					goto error;

				dentry = NULL;
			}
		} else if (cgtype->ntype == cgn_basic) {
			bits = cgen_basic_type_bits(cgen,
			    (cgtype_basic_t *)cgtype->ext);

			if (bits == 0) {
				lexer_dprint_tok(&symbol->ident->tok, stderr);
				fprintf(stderr,
				    ": Unimplemented variable type.XXX\n");
				cgen->error = true; // TODO
				rc = EINVAL;
				goto error;
			}

			rc = ir_dentry_create_int(bits, 0, &dentry);
			if (rc != EOK)
				goto error;

			rc = ir_dblock_append(var->dblock, dentry);
			if (rc != EOK)
				goto error;

			dentry = NULL;
		} else if (cgtype->ntype == cgn_pointer) {
			rc = ir_dentry_create_int(cgen_pointer_bits, 0, &dentry);
			if (rc != EOK)
				goto error;

			rc = ir_dblock_append(var->dblock, dentry);
			if (rc != EOK)
				goto error;

			dentry = NULL;

		} else if (cgtype->ntype == cgn_record ||
		    cgtype->ntype == cgn_enum ||
		    cgtype->ntype == cgn_array) {
			rc = cgen_init_dentries(cgen, cgtype, NULL, NULL,
			    var->dblock);
			if (rc != EOK)
				goto error;
		} else {
			lexer_dprint_tok(&symbol->ident->tok, stderr);
			fprintf(stderr, ": Unimplemented variable type.\n");
			cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}
	}

	ir_module_append(cgen->irmod, &var->decln);
	var = NULL;

	return EOK;
error:
	if (proc != NULL)
		ir_proc_destroy(proc);
	return rc;
}

/** Generate definitions from symbol declarations for a module.
 *
 * @param cgen Code generator
 * @param symbols Symbol directory
 * @return EOK on success or an error code
 */
static int cgen_module_symdecls(cgen_t *cgen, symbols_t *symbols)
{
	int rc;
	symbol_t *symbol;

	symbol = symbols_first(symbols);
	while (symbol != NULL) {
		if ((symbol->flags & sf_defined) == 0) {
			switch (symbol->stype) {
			case st_fun:
				rc = cgen_module_symdecl_fun(cgen, symbol);
				if (rc != EOK)
					goto error;
				break;
			case st_var:
				rc = cgen_module_symdecl_var(cgen, symbol);
				if (rc != EOK)
					goto error;
				break;
			case st_type:
				break;
			}
		}

		symbol = symbols_next(symbol);
	}

	return EOK;
error:
	return rc;
}

/** Generate code for module.
 *
 * @param cgen Code generator
 * @param inops Parser input ops
 * @param inarg Parser input arg
 * @param stok Staring token
 * @param symbols Symbol directory to fill in
 * @param rirmod Place to store pointer to new IR module
 * @return EOK on success or an error code
 */
int cgen_module(cgen_t *cgen, parser_input_ops_t *inops, void *inarg,
    void *stok, symbols_t *symbols, ir_module_t **rirmod)
{
	int rc;
	ast_module_t *amod;
	ir_module_t *irmod = NULL;
	parser_t *parser = NULL;

	rc = parser_create(inops, inarg, stok, 0, false, &parser);
	if (rc != EOK)
		return rc;

	parser->cb = &cgen_parser_cb;
	parser->cb_arg = (void *)cgen;

	cgen->parser = parser;
	cgen->symbols = symbols;

	rc = ir_module_create(&irmod);
	if (rc != EOK)
		return rc;

	cgen->irmod = irmod;

	rc = parser_process_module(parser, &amod);
	if (rc != EOK)
		goto error;

	cgen->astmod = amod;

	rc = cgen_module_symdecls(cgen, symbols);
	if (rc != EOK)
		goto error;

	parser_destroy(parser);
	*rirmod = irmod;
	return EOK;
error:
	parser_destroy(parser);
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

	cgen_enums_destroy(cgen->enums);
	scope_destroy(cgen->scope);
	cgen_records_destroy(cgen->records);
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
	list_initialize(&cgswitch->values);
	*rswitch = cgswitch;
	return EOK;
}

/** Insert new value to switch tracking record.
 *
 * @param cgswitch Code generator switch tracking record
 * @param val Value
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_switch_insert_value(cgen_switch_t *cgswitch, int64_t val)
{
	cgen_switch_value_t *value;

	value = calloc(1, sizeof(cgen_switch_value_t));
	if (value == NULL)
		return ENOMEM;

	value->cgswitch = cgswitch;
	value->value = val;
	list_append(&value->lvalues, &cgswitch->values);
	return EOK;
}

/** Get first case value in switch tracking record.
 *
 * @param cgswitch Code generator switch tracking record
 * @return First value or @c NULL if there are none
 */
static cgen_switch_value_t *cgen_switch_first_value(cgen_switch_t *cgswitch)
{
	link_t *link;

	link = list_first(&cgswitch->values);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_switch_value_t, lvalues);
}

/** Get next case value in switch tracking record.
 *
 * @param cur Current value
 * @return Next value or @c NULL if @a cur is the last one
 */
static cgen_switch_value_t *cgen_switch_next_value(cgen_switch_value_t *cur)
{
	link_t *link;

	link = list_next(&cur->lvalues, &cur->cgswitch->values);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_switch_value_t, lvalues);
}

/** Find case value in switch tracking record.
 *
 * @param cgswitch Code generator switch tracking record
 * @param val Value
 * @param rvalue Place to store value structure if found
 * @return EOK on success, ENOENT if not found
 */
static int cgen_switch_find_value(cgen_switch_t *cgswitch, int64_t val,
    cgen_switch_value_t **rvalue)
{
	cgen_switch_value_t *value;

	value = cgen_switch_first_value(cgswitch);
	while (value != NULL) {
		if (value->value == val) {
			*rvalue = value;
			return EOK;
		}

		value = cgen_switch_next_value(value);
	}

	return ENOENT;
}

/** Destroy code generator switch value.
 *
 * @param value Code generator switch value or @c NULL
 */
static void cgen_switch_value_destroy(cgen_switch_value_t *value)
{
	if (value == NULL)
		return;

	list_remove(&value->lvalues);
	free(value);
}

/** Destroy code generator switch tracking record.
 *
 * @param cgswitch Code generator switch tracking record or @c NULL
 */
void cgen_switch_destroy(cgen_switch_t *cgswitch)
{
	cgen_switch_value_t *value;

	if (cgswitch == NULL)
		return;

	value = cgen_switch_first_value(cgswitch);
	while (value != NULL) {
		cgen_switch_value_destroy(value);
		value = cgen_switch_first_value(cgswitch);
	}

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
