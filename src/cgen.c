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
#include <scope.h>
#include <stdlib.h>
#include <string.h>
#include <symbols.h>

static unsigned cgen_type_sizeof(cgen_t *, cgtype_t *);
static int cgen_proc_create(cgen_t *, ir_proc_t *, cgen_proc_t **);
static void cgen_proc_destroy(cgen_proc_t *);
static int cgen_decl(cgen_t *, cgtype_t *, ast_node_t *,
    ast_aslist_t *, cgtype_t **);
static int cgen_sqlist(cgen_t *, ast_sqlist_t *, cgtype_t **);
static int cgen_const_int(cgen_proc_t *, cgtype_elmtype_t, int64_t,
    ir_lblock_t *, cgen_eres_t *);
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
static int cgen_cgtype(cgen_t *, cgtype_t *, ir_texpr_t **);
static int cgen_typedef(cgen_t *, ast_tok_t *, ast_idlist_t *, cgtype_t *);
static int cgen_fun_arg_passed_type(cgen_t *, cgtype_t *, cgtype_t **);
static int cgen_init_dentries_cinit(cgen_t *, cgtype_t *, comp_tok_t *,
    ast_cinit_elem_t **, ir_dblock_t *);

enum {
	cgen_pointer_bits = 16,
	cgen_enum_bits = 16,
	cgen_char_bits = 8,
	cgen_char_max = 255,
	cgen_lchar_bits = 16,
	cgen_lchar_max = 65535
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

/** Determine if type is lotif type.
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
		return tarray->have_size == false;
	}

	assert(false);
	return false;
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
	int rc;

	/*
	 * Create a dummy labeled block where the code will be emitted
	 * and then it will be dropped.
	 */
	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create("foo", 0, lblock, &irproc);
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

	rc = cgen_expr_rvalue(&cgexpr, expr, irproc->lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);

	assert(eres->cvknown);
	return EOK;
error:
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
	cgen_eres_t bres;
	int rc;

	cgen_eres_init(&bres);

	/*
	 * Create a dummy labeled block where the code will be emitted
	 * and then it will be dropped.
	 */
	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create("foo", 0, lblock, &irproc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	rc = cgen_proc_create(cgen, irproc, &cgproc);
	if (rc != EOK)
		goto error;

	/* Code generator for a constant expression */
	cgen_expr_init(&cgexpr);
	cgexpr.cgen = cgen;
	cgexpr.cgproc = cgproc;
	cgexpr.cexpr = true;

	rc = cgen_expr_rvalue(&cgexpr, expr, irproc->lblock, &bres);
	if (rc != EOK)
		goto error;

	rc = cgen_type_convert(&cgexpr, itok, &bres, dtype, cgen_implicit,
	    irproc->lblock, eres);
	if (rc != EOK)
		goto error;

	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);

	assert(eres->cvknown);
	cgen_eres_fini(&bres);
	return EOK;
error:
	cgen_eres_fini(&bres);
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
	int rc;

	cgen_eres_init(&eres);

	/*
	 * Create a dummy labeled block where the code will be emitted
	 * and then it will be dropped.
	 */
	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create("foo", 0, lblock, &irproc);
	if (rc != EOK)
		goto error;

	lblock = NULL;

	rc = cgen_proc_create(cgen, irproc, &cgproc);
	if (rc != EOK)
		goto error;

	/* Code generator for an integer constant expression */
	cgen_expr_init(&cgexpr);
	cgexpr.cgen = cgproc->cgen;
	cgexpr.cgproc = cgproc;

	rc = cgen_expr(&cgexpr, expr, irproc->lblock, &eres);
	if (rc != EOK)
		goto error;

	cgen_proc_destroy(cgproc);
	ir_proc_destroy(irproc);
	ir_lblock_destroy(lblock);

	*etype = eres.cgtype;
	eres.cgtype = NULL;

	cgen_eres_fini(&eres);
	return EOK;
error:
	cgen_eres_fini(&eres);
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

	*rdtype = &func->cgtype;
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
	} else {
		/* Array size not specified */
		have_size = false;
		asize = 0;
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

	rc = cgtype_array_create(btype_copy, have_size, asize, &arrtype);
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
 * @param is_signed @c true iff addition is signed
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
	comp_tok_t *ident;
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *var = NULL;
	char *pident = NULL;
	int rc;

	ident = (comp_tok_t *) eident->tident.data;

	if (cgexpr->icexpr) {
		cgen_error_expr_not_constant(cgexpr->cgen, &eident->tident);
		return EINVAL;
	}

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		goto error;

	rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
	if (rc != EOK)
		goto error;

	rc = ir_oper_var_create(pident, &var);
	if (rc != EOK)
		goto error;

	instr->itype = iri_varptr;
	instr->width = cgexpr->cgen->arith_width;
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

	/*
	 * If the left operand is a pointer, we need to convert it
	 * to rvalue. We leave an array as an lvalue. In either case
	 * we end up with the base address stored in the result variable.
	 */
	if (lres->cgtype->ntype == cgn_pointer) {
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
		assert(lres->cgtype->ntype == cgn_array);
		arrt = (cgtype_array_t *)lres->cgtype->ext;

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

	/* Pointer/array + integer */
	if (l_ptra && r_int)
		return cgen_add_ptra_int(cgexpr, ctok, lres, rres, lblock, eres);

	/* Integer + pointer/array */
	if (l_int && r_ptra) {
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
	bool l_ptr;
	bool r_ptr;

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

	l_ptr = lres->cgtype->ntype == cgn_pointer;
	r_ptr = rres->cgtype->ntype == cgn_pointer;

	/* Pointer - pointer */
	if (l_ptr && r_ptr) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Unimplemented pointer subtraction.\n");

		cgexpr->cgen->error = true; // TODO
		return EINVAL;
	}

	/* Pointer - integer */
	if (l_ptr && r_int)
		return cgen_sub_ptr_int(cgexpr, ctok, lres, rres, lblock, eres);

	/* Integer - pointer */
	if (l_int && r_ptr) {
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
	rc = cgen_expr_rvalue(cgexpr, ebinop->larg, lblock, &lres);
	if (rc != EOK)
		goto error;

	/* Evaluate right operand */
	rc = cgen_expr_rvalue(cgexpr, ebinop->rarg, lblock, &rres);
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
 * @param cgexpr Code generator for expression
 * @param ares Address expression result
 * @param vres Value expression result
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_store_record(cgen_expr_t *cgexpr, cgen_eres_t *ares,
    cgen_eres_t *vres, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	ir_texpr_t *recte = NULL;
	int rc;

	assert(vres->cgtype->ntype == cgn_record);

	/* Generate IR type expression for the record type */
	rc = cgen_cgtype(cgexpr->cgen, vres->cgtype, &recte);
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

/** Generate code for storing a value (in an assignment expression).
 *
 * @param cgexpr Code generator for expression
 * @param ares Address expression result
 * @param vres Value expression result
 * @param lblock IR labeled block to which the code should be appended
 * @return EOK on success or an error code
 */
static int cgen_store(cgen_expr_t *cgexpr, cgen_eres_t *ares,
    cgen_eres_t *vres, ir_lblock_t *lblock)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *larg = NULL;
	ir_oper_var_t *rarg = NULL;
	unsigned bits;
	int rc;

	/* Check the type */
	if (vres->cgtype->ntype == cgn_basic) {
		bits = cgen_basic_type_bits(cgexpr->cgen,
		    (cgtype_basic_t *)vres->cgtype->ext);
		if (bits == 0) {
			fprintf(stderr, "Unimplemented variable type.\n");
			cgexpr->cgen->error = true; // TODO
			rc = EINVAL;
			goto error;
		}
	} else if (vres->cgtype->ntype == cgn_pointer) {
		bits = cgen_pointer_bits;
	} else if (vres->cgtype->ntype == cgn_record) {
		return cgen_store_record(cgexpr, ares, vres, lblock);
	} else if (vres->cgtype->ntype == cgn_enum) {
		bits = cgen_enum_bits;
	} else {
		fprintf(stderr, "Unimplemented variable type.\n");
		cgexpr->cgen->error = true; // TODO
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
	rc = cgen_store(cgexpr, &lres, &cres, lblock);
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
	rc = cgen_store(cgexpr, &laddr, &ores, lblock);
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
	rc = cgen_store(cgexpr, &laddr, &ores, lblock);
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
	rc = cgen_store(cgexpr, &lres, &ores, lblock);
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
	rc = cgen_store(cgexpr, &lres, &ores, lblock);
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
	rc = cgen_store(cgexpr, &lres, &ores, lblock);
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
	rc = cgen_store(cgexpr, &lres, &ores, lblock);
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
	rc = cgen_store(cgexpr, &lres, &ores, lblock);
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
	rc = cgen_store(cgexpr, &lres, &ores, lblock);
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
	comp_tok_t *tok;

	switch (ebinop->optype) {
	case abo_plus:
		return cgen_bo_plus(cgexpr, ebinop, lblock, eres);
	case abo_minus:
		return cgen_bo_minus(cgexpr, ebinop, lblock, eres);
	case abo_times:
		return cgen_bo_times(cgexpr, ebinop, lblock, eres);
	case abo_divide:
	case abo_modulo:
		tok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unimplemented binary operator.\n");
		cgexpr->cgen->error = true; // TODO
		return EINVAL;
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
	case abo_modulo_assign:
		tok = (comp_tok_t *) ebinop->top.data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Unimplemented binary operator.\n");
		cgexpr->cgen->error = true; // TODO
		return EINVAL;
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
	cgtype_t *argtype = NULL;
	int rc;

	cgen_eres_init(&ares);
	cgen_eres_init(&cres);

	if (ecall->fexpr->ntype != ant_eident) {
		atok = ast_tree_first_tok(ecall->fexpr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": Function call needs an identifier (not implemented).\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	eident = (ast_eident_t *) ecall->fexpr->ext;
	ident = (comp_tok_t *) eident->tident.data;

	/* Check if the identifier is declared */
	member = scope_lookup(cgexpr->cgen->cur_scope, ident->tok.text);
	if (member == NULL) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Undeclared identifier '%s'.\n",
		    ident->tok.text);
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if (member->cgtype->ntype != cgn_func) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Called object '%s' is not a function.\n",
		    ident->tok.text);
		cgexpr->cgen->error = true; // TODO
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
		 * If the function has a prototype and the argument is not
		 * variadic, convert it to its declared type.
		 * XXX Otherwise it should be simply promoted.
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
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	if (!cgtype_is_void(ftype->rtype)) {
		rc = cgen_create_new_lvar_oper(cgexpr->cgproc, &dest);
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
	bool b_int;
	bool i_int;
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

	b_int = cgen_type_is_integer(cgexpr->cgen, bres.cgtype);
	i_int = cgen_type_is_integer(cgexpr->cgen, ires.cgtype);

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

	if ((b_ptra && !i_int) || (i_ptra && !b_int)) {
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
		cgexpr->cgen->error = true; // TODO
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
	int rc;

	cgen_eres_init(&bres);

	/* Evaluate expression as lvalue */
	rc = cgen_expr_lvalue(cgexpr, eaddr->bexpr, lblock, &bres);
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
	eres->cvknown = bres.cvknown;
	eres->cvint = bres.cvint;
	eres->cvsymbol = bres.cvsymbol;
	eres->cgtype = cgtype;

	return EOK;
error:
	cgen_eres_fini(&bres);
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
	unsigned sz;
	int rc;

	/* Declaration specifiers */
	rc = cgen_dspecs(cgexpr->cgen, esizeof->atypename->dspecs,
	    &sctype, &flags, &stype);
	if (rc != EOK)
		goto error;

	if ((flags & cgrd_def) != 0) {
		atok = ast_tree_first_tok(&esizeof->atypename->node);
		ctok = (comp_tok_t *) atok->data;
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

	sz = cgen_type_sizeof(cgexpr->cgen, etype);

	rc = cgen_const_int(cgexpr->cgproc, cgelm_int, sz, lblock, eres);
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
	unsigned sz;
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

	sz = cgen_type_sizeof(cgexpr->cgen, etype);

	rc = cgen_const_int(cgexpr->cgproc, cgelm_int, sz, lblock, eres);
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
	cgtype_t *stype = NULL;
	cgtype_t *dtype = NULL;
	ast_sclass_type_t sctype;
	cgen_rd_flags_t flags;
	int rc;

	cgen_eres_init(&bres);

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

	elem = cgen_record_elem_find(record, mtok->tok.text);
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

	elem = cgen_record_elem_find(record, mtok->tok.text);
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
	rc = cgen_const_int(cgexpr->cgproc, cgelm_int, 1, lblock, &adj);
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
	rc = cgen_store(cgexpr, &baddr, &ares, lblock);
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
	rc = cgen_const_int(cgexpr->cgproc, cgelm_int, 1, lblock, &adj);
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
	rc = cgen_store(cgexpr, &baddr, &ares, lblock);
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
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
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
		atok = ast_tree_first_tok(expr);
		tok = (comp_tok_t *) atok->data;
		lexer_dprint_tok(&tok->tok, stderr);
		fprintf(stderr, ": This expression type is not implemented.\n");
		cgexpr->cgen->error = true; // TODO
		rc = EINVAL;
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
 * @param dres Place to store rvalue expression result
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
		fprintf(stderr, "Unimplemented variable type.\n");
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
	int rc;
	*converted = false;

	(void)cgen;

	if (res->cgtype->ntype == cgn_enum) {
		/* Return corresponding integer type */
		if (cgtype_is_strict_enum(res->cgtype))
			*converted = true;

		rres->varname = res->varname;
		rres->valtype = res->valtype;
		rres->cvknown = res->cvknown;
		rres->cvint = res->cvint;
		rres->cvsymbol = res->cvsymbol;
		rres->tfirst = res->tfirst;
		rres->tlast = res->tlast;

		rc = cgtype_int_construct(true, cgir_int, &rres->cgtype);
		if (rc != EOK)
			goto error;
	} else {
		/* Return unchanged */
		rc = cgen_eres_clone(res, rres);
		if (rc != EOK)
			goto error;
	}

	return EOK;
error:
	return rc;
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
	cgtype_int_rank_t rank1;
	cgtype_int_rank_t rank2;
	cgtype_int_rank_t rrank;
	bool sign1;
	bool sign2;
	bool const1;
	bool const2;
	bool rsign;
	unsigned bits1;
	unsigned bits2;
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

	/* Get rank, bits and signedness of both operands */

	rank1 = cgtype_int_rank(ir1.cgtype);
	sign1 = cgen_type_is_signed(cgexpr->cgen, ir1.cgtype);
	bits1 = cgen_basic_type_bits(cgexpr->cgen, bt1);
	const1 = ir1.cvknown;
	neg1 = const1 && cgen_cvint_is_negative(cgexpr->cgen, sign1, ir1.cvint);

	rank2 = cgtype_int_rank(ir2.cgtype);
	sign2 = cgen_type_is_signed(cgexpr->cgen, ir2.cgtype);
	bits2 = cgen_basic_type_bits(cgexpr->cgen, bt2);
	const2 = ir2.cvknown;
	neg2 = const2 && cgen_cvint_is_negative(cgexpr->cgen, sign2, ir2.cvint);

	/* Determine resulting rank */

	rrank = rank1 > rank2 ? rank1 : rank2;

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

	/* Promote both operands */

	rc = cgen_eres_promoted_rvalue(cgexpr, &ir1, lblock, &pr1);
	if (rc != EOK)
		goto error;

	rc = cgen_eres_promoted_rvalue(cgexpr, &ir2, lblock, &pr2);
	if (rc != EOK)
		goto error;

	/* Construct result type */

	rc = cgtype_int_construct(rsign, rrank, &rtype);
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
	    expl != cgen_explicit) {
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
	int rc;

	(void)lblock;

	assert(ares->cgtype->ntype == cgn_basic);
	assert(dtype->ntype == cgn_pointer);

	bits = cgen_basic_type_bits(cgexpr->cgen,
	    (cgtype_basic_t *)ares->cgtype->ext);

	if (expl != cgen_explicit) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Implicit conversion from integer "
		    "to pointer.\n");
		++cgexpr->cgen->warnings;
	}

	if (bits != cgen_pointer_bits) {
		lexer_dprint_tok(&ctok->tok, stderr);
		fprintf(stderr, ": Warning: Converting to pointer from integer "
		    "of different size.\n");
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

	/* Source and destination types are record types */
	if (ares->cgtype->ntype == cgn_record &&
	    dtype->ntype == cgn_record) {
		return cgen_type_convert_record(cgexpr, ctok, ares, dtype,
		    lblock, cres);
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

	/* Source and destination types are pointers */
	if (cgen_type_is_integer(cgexpr->cgen, ares->cgtype) &&
	    dtype->ntype == cgn_pointer) {
		return cgen_type_convert_int_ptr(cgexpr, ctok, ares, dtype,
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
	cgtype_t *etype = NULL;
	cgtype_pointer_t *ptrt = NULL;
	cgtype_array_t *arrt;
	int rc;

	assert(ares->cgtype->ntype == cgn_array);
	arrt = (cgtype_array_t *)ares->cgtype->ext;

	cgen_eres_init(&pres);

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
	if (etype != NULL)
		cgtype_destroy(etype);
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

	/* Destination type is an array */
	if (dtype->ntype == cgn_array) {
		assert(expl == cgen_explicit);
		cgen_error_cast_array(cgexpr->cgen, ctok);
		return EINVAL;
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

		rc = cgen_expr_rvalue(&cgproc->cgexpr, areturn->arg, lblock, &ares);
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

		rc = cgen_truth_expr_cjmp(&cgproc->cgexpr, elsif->cond, false,
		    fiflabel, lblock);
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

	rc = cgen_truth_expr_cjmp(&cgproc->cgexpr, awhile->cond, false, ewlabel, lblock);
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

	rc = cgen_block(cgproc, afor->body, lblock);
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

	/* Compute the value of the expression (e.g. read volatile variable) */
	rc = cgen_expr_rvalue(&cgproc->cgexpr, stexpr->expr, lblock, &ares);
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
		rc = cgen_expr_rvalue(&cgproc->cgexpr, iexpr, lblock, &ires);
		if (rc != EOK)
			goto error;

		/* Convert expression result to variable type */
		rc = cgen_type_convert(&cgproc->cgexpr, itok, &ires, dtype,
		    cgen_implicit, lblock, &cres);
		if (rc != EOK)
			goto error;

		/* Store the converted value */
		rc = cgen_store(&cgproc->cgexpr, &lres, &cres, lblock);
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

	rc = scope_create(cgproc->cgen->cur_scope, &block_scope);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	/* Enter block scope */
	cgproc->cgen->cur_scope = block_scope;

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
	cgtype_array_t *arrt;
	cgtype_pointer_t *ptrt;
	int rc;

	(void)cgen;

	if (stype->ntype == cgn_array) {
		/* An array is really passed as a pointer */

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

		/* Check type for completeness */
		if (cgen_type_is_incomplete(cgen, stype)) {
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
 * @param btype Type derived from declaration specifiers
 * @return EOK on success or an error code
 */
static int cgen_fundef(cgen_t *cgen, ast_gdecln_t *gdecln, cgtype_t *btype)
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
	cgtype_t *ctype = NULL;
	cgtype_t *ptype = NULL;
	cgtype_func_t *dtfunc;
	cgtype_func_arg_t *dtarg;
	ast_aspec_t *aspec;
	ast_aspec_attr_t *attr;
	scope_t *prev_scope = NULL;
	int rc;
	int rv;

	aident = ast_gdecln_get_ident(gdecln);
	ident = (comp_tok_t *) aident->data;

	/* If the function is not declared yet, create a symbol */
	symbol = symbols_lookup(cgen->symbols, ident->tok.text);
	if (symbol == NULL) {
		rc = symbols_insert(cgen->symbols, st_fun, ident);
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

		if ((symbol->flags & sf_defined) != 0) {
			/* Already defined */
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Redefinition of '%s'.\n", ident->tok.text);
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

		rv = asprintf(&arg_ident, "%%%d", cgproc->next_var++);
		if (rv < 0) {
			rc = ENOMEM;
			goto error;
		}

		/* Convert argument type */
		rc = cgen_fun_arg_passed_type(cgproc->cgen, stype, &ptype);
		if (rc != EOK)
			goto error;

		/* Insert identifier into argument scope */
		rc = scope_insert_arg(cgproc->arg_scope, &tok->tok,
		    ptype, arg_ident);
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

	cgen_proc_destroy(cgproc);
	cgproc = NULL;

	return EOK;
error:
	if (prev_scope != NULL)
		cgen->cur_scope = prev_scope;

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
			    " non-global scope.\n");
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
 * @param gdecln Global declaration that is a function definition
 * @return EOK on success or an error code
 */
static int cgen_fundecl(cgen_t *cgen, cgtype_t *ftype, ast_gdecln_t *gdecln)
{
	ast_tok_t *aident;
	comp_tok_t *ident;
	symbol_t *symbol;
	cgtype_t *ctype = NULL;
	cgtype_func_t *dtfunc;
	int rc;

	aident = ast_gdecln_get_ident(gdecln);
	ident = (comp_tok_t *) aident->data;

	assert(ftype->ntype == cgn_func);
	dtfunc = (cgtype_func_t *)ftype->ext;

	if (dtfunc->rtype->ntype == cgn_array) {
		/* Function returning array */
		cgen_error_fun_ret_array(cgen, aident);
		return EINVAL;
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
		rc = symbols_insert(cgen->symbols, st_fun, ident);
		if (rc != EOK)
			return rc;

		symbol = symbols_lookup(cgen->symbols, ident->tok.text);
		assert(symbol != NULL);

		rc = cgtype_clone(ftype, &symbol->cgtype);
		if (rc != EOK)
			return rc;

		/* Insert identifier into module scope */
		rc = scope_insert_gsym(cgen->scope, &ident->tok, ftype, symbol);
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
			return EINVAL;
		}
		if (rc != EOK)
			return rc;

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
	}

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
	char *sident = NULL;
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
			rc = cgen_gprefix(initsym->ident->tok.text,
			    &sident);
			if (rc != EOK)
				goto error;

			rc = ir_dentry_create_ptr(16, sident, initval, &dentry);
			if (rc != EOK)
				goto error;

			free(sident);
			sident = NULL;
		} else {
			rc = ir_dentry_create_int(16, initval, &dentry);
			if (rc != EOK)
				goto error;
		}

		rc = ir_dblock_append(dblock, dentry);
		if (rc != EOK)
			goto error;

		dentry = NULL;
	} else if (stype->ntype == cgn_enum) {
		rc = ir_dentry_create_int(16, initval, &dentry);
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
	if (sident != NULL)
		free(sident);
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
 * @param dblock Data block to which data should be appended
 * @return EOK on success or an error code
 */
static int cgen_init_dentries_array(cgen_t *cgen, cgtype_array_t *tarray,
    comp_tok_t *itok, ast_cinit_elem_t **elem, ir_dblock_t *dblock)
{
	uint64_t i;
	size_t entries;
	int rc;

	if (tarray->have_size) {
		/* Size of array is known. Process that number of entries. */
		for (i = 0; i < tarray->asize; i++) {
			rc = cgen_init_dentries_cinit(cgen, tarray->etype, itok,
			    elem, dblock);
			if (rc != EOK)
				goto error;
		}
	} else {
		/* Size of array is not known. Process and count all entries. */
		entries = 0;
		while (*elem != NULL) {
			rc = cgen_init_dentries_cinit(cgen, tarray->etype, itok,
			    elem, dblock);
			if (rc != EOK)
				goto error;
			++entries;
		}

		/* Fix up array type */
		tarray->have_size = true;
		tarray->asize = entries;
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
 * @param dblock Data block to which data should be appended
 * @return EOK on success or an error code
 */
static int cgen_init_dentries_record(cgen_t *cgen, cgtype_record_t *trecord,
    comp_tok_t *itok, ast_cinit_elem_t **elem, ir_dblock_t *dblock)
{
	cgen_rec_elem_t *relem;
	int rc;

	relem = cgen_record_first(trecord->record);
	while (relem != NULL) {
		rc = cgen_init_dentries_cinit(cgen, relem->cgtype, itok,
		    elem, dblock);
		if (rc != EOK)
			goto error;

		/* In case of union, only initialize the first element. */
		if (trecord->record->rtype == cgr_union)
			break;

		relem = cgen_record_next(relem);
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
    ast_cinit_elem_t **elem, ir_dblock_t *dblock)
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
				    &melem, dblock);
				if (rc != EOK)
					goto error;
			} else {
				cgrec = (cgtype_record_t *)stype->ext;

				rc = cgen_init_dentries_record(cgen, cgrec,
				    itok, &melem, dblock);
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
				    elem, dblock);
				if (rc != EOK)
					goto error;
			} else {
				cgrec = (cgtype_record_t *)stype->ext;

				rc = cgen_init_dentries_record(cgen, cgrec,
				    itok, elem, dblock);
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

		rc = cgen_init_dentries_scalar(cgen, stype, itok, init, dblock);
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
	bool wide;
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
	int rc;

	if (stype->ntype == cgn_array || stype->ntype == cgn_record) {
		if (init == NULL || init->ntype == ant_cinit) {
			celem = init != NULL ?
			    ast_cinit_first((ast_cinit_t *)init->ext) : NULL;

			if (stype->ntype == cgn_array) {
				cgarr = (cgtype_array_t *)stype->ext;

				rc = cgen_init_dentries_array(cgen, cgarr, itok,
				    &celem, dblock);
				if (rc != EOK)
					goto error;
			} else {
				cgrec = (cgtype_record_t *)stype->ext;

				rc = cgen_init_dentries_record(cgen, cgrec,
				    itok, &celem, dblock);
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
			    (ast_estring_t *)init->ext, dblock);
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
		rc = cgen_init_dentries_scalar(cgen, stype, itok, init, dblock);
		if (rc != EOK)
			goto error;
	}

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
 * @param entry Init-declarator list entry that declares a variable
 * @return EOK on success or an error code
 */
static int cgen_vardef(cgen_t *cgen, cgtype_t *stype, ast_idlist_entry_t *entry)
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
	cgtype_t *ctype;
	cgtype_enum_t *tenum;
	int rc;

	aident = ast_decl_get_ident(entry->decl);
	ident = (comp_tok_t *) aident->data;

	/* Mark enum as named, because it has an instance. */
	if (stype->ntype == cgn_enum) {
		tenum = (cgtype_enum_t *)stype;
		tenum->cgenum->named = true;
	}

	symbol = symbols_lookup(cgen->symbols, ident->tok.text);
	if (symbol == NULL) {
		rc = symbols_insert(cgen->symbols, st_var, ident);
		if (rc != EOK)
			goto error;

		symbol = symbols_lookup(cgen->symbols, ident->tok.text);
		assert(symbol != NULL);

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
	}

	if (entry->init != NULL) {
		/* Mark the symbol as defined */
		symbol->flags |= sf_defined;

		/* Create initialized IR variable */

		rc = cgen_gprefix(ident->tok.text, &pident);
		if (rc != EOK)
			goto error;

		rc = ir_dblock_create(&dblock);
		if (rc != EOK)
			goto error;

		rc = ir_var_create(pident, NULL, dblock, &var);
		if (rc != EOK)
			goto error;

		free(pident);
		pident = NULL;
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
	} else if (gdecln->body != NULL) {
		rc = cgen_fundef(cgen, gdecln, stype);
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
			rc = cgen_decl(cgen, stype, entry->decl, entry->aslist,
			    &dtype);
			if (rc != EOK)
				goto error;

			if (ast_decl_is_vardecln(entry->decl)) {
				/* Variable declaration */
				rc = cgen_vardef(cgen, dtype, entry);
				if (rc != EOK)
					goto error;
			} else if (entry->decl->ntype == ant_dnoident) {
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
				rc = cgen_fundecl(cgen, dtype, gdecln);
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
 * @param ident Identifier torken
 * @param cgtype Function type
 * @return EOK on success or an error code
 */
static int cgen_module_symdecl_fun(cgen_t *cgen, comp_tok_t *ident,
    cgtype_t *cgtype)
{
	int rc;
	ir_proc_t *proc = NULL;
	char *pident = NULL;
	ir_proc_attr_t *irattr;
	cgtype_func_t *cgfunc;

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = ir_proc_create(pident, irp_extern, NULL, &proc);
	if (rc != EOK)
		goto error;

	free(pident);
	pident = NULL;

	rc = cgen_fun_args(cgen, ident, cgtype, proc);
	if (rc != EOK)
		goto error;

	/* Generate IR return type */
	rc = cgen_fun_rtype(cgen, cgtype, proc);
	if (rc != EOK)
		goto error;

	assert(cgtype->ntype == cgn_func);
	cgfunc = (cgtype_func_t *)cgtype->ext;

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
	if (pident != NULL)
		free(pident);
	if (proc != NULL)
		ir_proc_destroy(proc);
	return rc;
}

/** Generate definition from variable symbol declaration.
 *
 * @param cgen Code generator
 * @param ident Identifier torken
 * @param cgtype Variable
 * @return EOK on success or an error code
 */
static int cgen_module_symdecl_var(cgen_t *cgen, comp_tok_t *ident,
    cgtype_t *cgtype)
{
	int rc;
	ir_proc_t *proc = NULL;
	char *pident = NULL;
	ir_dblock_t *dblock = NULL;
	ir_dentry_t *dentry = NULL;
	ir_texpr_t *vtype = NULL;
	ir_var_t *var = NULL;
	unsigned bits;

	if (cgen_type_is_incomplete(cgen, cgtype)) {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": Variable has incomplete type.\n");
		cgen->error = true; // TODO
		return EINVAL;
	}

	rc = cgen_gprefix(ident->tok.text, &pident);
	if (rc != EOK)
		goto error;

	rc = ir_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	rc = cgen_cgtype(cgen, cgtype, &vtype);
	if (rc != EOK)
		goto error;

	rc = ir_var_create(pident, vtype, dblock, &var);
	if (rc != EOK)
		goto error;

	free(pident);
	pident = NULL;
	vtype = NULL;
	dblock = NULL;

	if (cgtype->ntype == cgn_basic) {
		bits = cgen_basic_type_bits(cgen, (cgtype_basic_t *)cgtype->ext);

		if (bits == 0) {
			lexer_dprint_tok(&ident->tok, stderr);
			fprintf(stderr, ": Unimplemented variable type.\n");
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
		rc = ir_dentry_create_int(16, 0, &dentry);
		if (rc != EOK)
			goto error;

		rc = ir_dblock_append(var->dblock, dentry);
		if (rc != EOK)
			goto error;

		dentry = NULL;

	} else if (cgtype->ntype == cgn_record || cgtype->ntype == cgn_enum ||
	    cgtype->ntype == cgn_array) {
		rc = cgen_init_dentries(cgen, cgtype, NULL, NULL, var->dblock);
		if (rc != EOK)
			goto error;
	} else {
		lexer_dprint_tok(&ident->tok, stderr);
		fprintf(stderr, ": UUUUnimplemented variable type.\n");
		cgen->error = true; // TODO
		rc = EINVAL;
		goto error;
	}

	ir_module_append(cgen->irmod, &var->decln);
	var = NULL;

	return EOK;
error:
	if (pident != NULL)
		free(pident);
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
				rc = cgen_module_symdecl_fun(cgen, symbol->ident,
				    symbol->cgtype);
				if (rc != EOK)
					goto error;
				break;
			case st_var:
				rc = cgen_module_symdecl_var(cgen, symbol->ident,
				    symbol->cgtype);
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
 * @param astmod AST module
 * @param symbols Symbol directory to fill in
 * @param rirmod Place to store pointer to new IR module
 * @return EOK on success or an error code
 */
int cgen_module(cgen_t *cgen, ast_module_t *astmod, symbols_t *symbols,
    ir_module_t **rirmod)
{
	int rc;
	ast_node_t *decln;
	ir_module_t *irmod = NULL;

	cgen->symbols = symbols;

	rc = ir_module_create(&irmod);
	if (rc != EOK)
		return rc;

	cgen->irmod = irmod;

	decln = ast_module_first(astmod);
	while (decln != NULL) {
		rc = cgen_global_decln(cgen, decln);
		if (rc != EOK)
			goto error;

		decln = ast_module_next(decln);
	}

	rc = cgen_module_symdecls(cgen, symbols);
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
