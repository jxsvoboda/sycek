/*
 * Copyright 2021 Jiri Svoboda
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
 * Intermediate Representation
 */

#ifndef TYPES_IR_H
#define TYPES_IR_H

#include <adt/list.h>
#include <stdint.h>

/** IR instruction type */
typedef enum {
	/** Addition */
	iri_add,
	/** Call */
	iri_call,
	/** Subtraction */
	iri_sub,
	/** Introduce immediate value */
	iri_imm,
	/** Read from memory */
	iri_read,
	/** Return value */
	iri_retv,
	/** Get pointer to global variable */
	iri_varptr,
	/** Write to memory */
	iri_write,
} ir_instr_type_t;

/** IR operand type */
typedef enum {
	/** Immediate value */
	iro_imm,
	/** List */
	iro_list,
	/** Variable reference */
	iro_var
} ir_oper_type_t;

/** IR operand */
typedef struct {
	/** Operand Type */
	ir_oper_type_t optype;
	/** Parent list operand */
	struct ir_oper_list *parent;
	/** Link to containing list operand */
	link_t llist;
	/** Type-specific data */
	void *ext;
} ir_oper_t;

/** IR immediate value operand */
typedef struct {
	/** Base object */
	ir_oper_t oper;
	/** Value */
	int32_t value;
} ir_oper_imm_t;

/** IR list operand */
typedef struct ir_oper_list {
	/** Base object */
	ir_oper_t oper;
	/** List of ir_oper_t */
	list_t list;
} ir_oper_list_t;

/** IR variable reference operand */
typedef struct {
	/** Base object */
	ir_oper_t oper;
	/** Variable name */
	char *varname;
} ir_oper_var_t;

/** IR instruction */
typedef struct {
	/** Instruction type */
	ir_instr_type_t itype;
	/** Operation width in bits */
	unsigned width;
	/** Destination */
	ir_oper_t *dest;
	/** Left operand */
	ir_oper_t *op1;
	/** Right operand */
	ir_oper_t *op2;
} ir_instr_t;

/** IR labeled block entry */
typedef struct {
	/** Containing labeled block */
	struct ir_lblock *lblock;
	/** Link to @c lblock->entries */
	link_t lentries;
	/** Label or @c NULL if none */
	char *label;
	/** Instruction */
	ir_instr_t *instr;
} ir_lblock_entry_t;

/** IR labeled block */
typedef struct ir_lblock {
	/** Entries */
	list_t entries; /* of ir_lblock_entry_t */
} ir_lblock_t;

/** IR data entry type */
typedef enum {
	/** Signed integer */
	ird_int,
	/** Unsigned integer */
	ird_uint
} ir_dentry_type_t;

/** IR data entry */
typedef struct {
	/** Data entry type */
	ir_dentry_type_t dtype;
	/** Data entry width in bits */
	unsigned width;
	/** Value */
	int32_t value;
} ir_dentry_t;

/** IR data block entry */
typedef struct {
	/** Containing data block */
	struct ir_dblock *dblock;
	/** Link to @c dblock->entries */
	link_t lentries;
	/** Data entry */
	ir_dentry_t *dentry;
} ir_dblock_entry_t;

/** IR data block */
typedef struct ir_dblock {
	/** Entries */
	list_t entries; /* of ir_dblock_entry_t */
} ir_dblock_t;

/** IR declaration type */
typedef enum {
	/** Variable declaration */
	ird_var,
	/** Procedure declaration */
	ird_proc
} ir_decln_type_t;

/** IR declaration */
typedef struct {
	/** Containing module */
	struct ir_module *module;
	/** Link to @c module->declns */
	link_t ldeclns;
	/** Declaration type */
	ir_decln_type_t dtype;
	/** Pointer to entire/specific structure */
	void *ext;
} ir_decln_t;

/** IR variable definition */
typedef struct {
	/** Base object */
	ir_decln_t decln;
	/** Indentifier */
	char *ident;
	/** Data block containing variable data */
	ir_dblock_t *dblock;
} ir_var_t;

/** IR procedure definition */
typedef struct {
	/** Base object */
	ir_decln_t decln;
	/** Indentifier */
	char *ident;
	/** Labeled block containing the implementation */
	ir_lblock_t *lblock;
} ir_proc_t;

/** IR module */
typedef struct ir_module {
	/** Declarations */
	list_t declns; /* of ir_decln_t */
} ir_module_t;

#endif
