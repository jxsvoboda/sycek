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
 */

#ifndef TYPES_CGEN_H
#define TYPES_CGEN_H

#include <stdbool.h>
#include <stdint.h>
#include <types/ast.h>
#include <types/cgtype.h>

/** Code generator */
typedef struct {
	/** Arithmetic width */
	int arith_width;
	/** Code generator hit an error */
	bool error;
	/** Number of warnings produced by code generator */
	int warnings;
	/** Module scope */
	struct scope *scope;
	/** Current (innermost) scope */
	struct scope *cur_scope;
	/** Module symbols */
	struct symbols *symbols;
	/** Record definitions */
	struct cgen_records *records;
	/** Enum definitions */
	struct cgen_enums *enums;
	/** Count nesting of record type specifiers (struct/union defs) */
	unsigned tsrec_cnt;
	/** Count nesting of argument lists */
	unsigned arglist_cnt;
	/** Anonymous tag counter */
	unsigned anon_tag_cnt;
	/** Output IR module */
	struct ir_module *irmod;
} cgen_t;

/** Code generator for expression */
typedef struct {
	/** Code generator */
	cgen_t *cgen;
	/** Code generator for procedure or @c NULL if constant expression */
	struct cgen_proc *cgproc;
	/** @c true iff constant expression */
	bool cexpr;
	/** @c true iff integer constant expression */
	bool icexpr;
} cgen_expr_t;

/** Code generator for procedure */
typedef struct cgen_proc {
	/** Containing code generator */
	cgen_t *cgen;
	/** IR procedure being constructed */
	struct ir_proc *irproc;
	/** Return type of the current procedure */
	cgtype_t *rtype;
	/** Next local variable number to allocate */
	unsigned next_var;
	/** Argument scope for this procedure */
	struct scope *arg_scope;
	/** Next label number to allocate */
	unsigned next_label;
	/** Current (innermost) loop */
	struct cgen_loop *cur_loop;
	/** Current (innermost) switch */
	struct cgen_switch *cur_switch;
	/** Current (innermo) loop or switch */
	struct cgen_loop_switch *cur_loop_switch;
	/** Goto labels */
	struct labels *labels;
	/** Code generator for non-constant expressions */
	cgen_expr_t cgexpr;
} cgen_proc_t;

/** Code generator for declaration specifies / specifier-qualifier list.
 *
 * A specifier-qualifier list is a subset of declaration-specifiers,
 * namely it cannot contain function-specifier ('inline') and it
 * cannot contain a storage class. Otherwise they are processed
 * in the same way.
 */
typedef struct {
	/** Containing code generator */
	cgen_t *cgen;
	/** Type specifier (there should be exactly one) */
	ast_node_t *tspec;
	/** Number of 'short' specifiers */
	int short_cnt;
	/** Number of 'long' specifiers */
	int long_cnt;
	/** Number of 'signed' specifiers */
	int signed_cnt;
	/** Number of 'unsigned' specifiers */
	int unsigned_cnt;
	/** Storage class type */
	ast_sclass_type_t sctype;
} cgen_dspec_t;

/** Value type.
 *
 * The type of value resulting from an expression.
 */
typedef enum {
	/** Lvalue (address) */
	cgen_rvalue,
	/** Rvalue (value) */
	cgen_lvalue
} cgen_valtype_t;

/** Code generator expression result.
 *
 * Describes where and how the result of an expression was stored during code
 * generation for that expression.
 */
typedef struct {
	/** Name of variable containing the result */
	const char *varname;

	/** Value type.
	 *
	 * For rvalue, the variable @c varname contains the actual value,
	 * for lvalue it contains the address of a memory location.
	 */
	cgen_valtype_t valtype;
	/** C type */
	struct cgtype *cgtype;
	/** Value used.
	 *
	 * Indicates whether the outermost operation of the expression
	 * (but not the subexpressions!) has some kind of side effect
	 * that justifies it even in case the value of the entire expression
	 * is then not used. Used for checking for computing values
	 * that are then not used. Example: ++i -> true, (++i) + 1 -> false.
	 */
	bool valused;
	/** @c true if expression has a known constant value */
	bool cvknown;
	/** Integer value of constant expression (or offset if a pointer) */
	int64_t cvint;
} cgen_eres_t;

/** Code generator loop tracking record.
 *
 * We keep a stack of enclosing loop statements.
 */
typedef struct cgen_loop {
	/** Outside loop statement */
	struct cgen_loop *parent;
	/** Continue label */
	const char *clabel;
} cgen_loop_t;

/** Code generator switch tracking record.
 *
 * We keep a stack of enclosing switch statements.
 */
typedef struct cgen_switch {
	/** Outside switch statement */
	struct cgen_switch *parent;
	/** Name of variable containing case expression result */
	const char *svarname;
	/** Next case condition label */
	char *nclabel;
	/** Next case body label */
	char *nblabel;
	/** Default label */
	char *dlabel;
} cgen_switch_t;

/** Code generator loop or switch tracking record.
 *
 * We keep a stack of enclosing loop or switch statements.
 */
typedef struct cgen_loop_switch {
	/** Outside loop or switch statement */
	struct cgen_loop_switch *parent;
	/** Break label */
	const char *blabel;
} cgen_loop_switch_t;

/** Explicicit or implicit type conversion */
typedef enum {
	/** Explicit type conversion */
	cgen_explicit,
	/** Implicit type conversion */
	cgen_implicit
} cgen_expl_t;

/** Usual arithmetic conversion flags */
typedef enum {
	/** No flags */
	cguac_none = 0,
	/** Mixed signedness integers converted to unsigned */
	cguac_mix2u = 0x1,
	/** Negative number converted to unsigned */
	cguac_neg2u = 0x2,
	/** Enum(s) converted to integer(s) */
	cguac_enum = 0x4,
	/** Two incompatible enum types */
	cguac_enuminc = 0x8,
	/** Enum type and non-enum type */
	cguac_enummix = 0x10,
	/** Any of the operands was signed integer (not constant) */
	cguac_signed = 0x20,
	/** Any of the operands was a negative constant */
	cguac_negative = 0x40
} cgen_uac_flags_t;

/** Record declaration flags */
typedef enum {
	/** No flags */
	cgrd_none = 0,
	/** This is a struct/union definition */
	cgrd_def = 0x1,
	/** The struct/union has a tag identifer */
	cgrd_ident = 0x2,
	/** The struct/union was already declared before */
	cgrd_prevdecl = 0x4,
	/** The struct/union was already defined beore */
	cgrd_prevdef = 0x8
} cgen_rd_flags_t;

#endif
