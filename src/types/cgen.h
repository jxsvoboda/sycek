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
 */

#ifndef TYPES_CGEN_H
#define TYPES_CGEN_H

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
	/** Module symbols */
	struct symbols *symbols;
} cgen_t;

/** Code generator for procedure */
typedef struct {
	/** Containing code generator */
	cgen_t *cgen;
	/** IR procedure being constructed */
	struct ir_proc *irproc;
	/** Next local variable number to allocate */
	unsigned next_var;
	/** Next label number to allocate */
	unsigned next_label;
	/** Argument scope for this procedure */
	struct scope *arg_scope;
	/** Procedure scope */
	struct scope *proc_scope;
	/** Current (innermost) scope */
	struct scope *cur_scope;
} cgen_proc_t;

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
} cgen_eres_t;

#endif
