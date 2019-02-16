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
 * Intermediate Representation
 */

#ifndef TYPES_IR_H
#define TYPES_IR_H

#include <adt/list.h>
#include <stdint.h>

/** IR Instruction Type */
typedef enum {
	/** Addition */
	iri_add,
	/** Load Immediate Value */
	iri_ldimm,
	/** Return Value */
	iri_retv
} ir_instr_type_t;

/** IR Operand Type */
typedef enum {
	/** Immediate Value */
	iro_imm,
	/** Variable Reference */
	iro_var
} ir_oper_type_t;

/** IR Operand */
typedef struct {
	/** Operand Type */
	ir_oper_type_t optype;
	void *ext;
} ir_oper_t;

/** IR Immediate Value Operand */
typedef struct {
	/** Base Object */
	ir_oper_t oper;
	/** Value */
	int32_t value;
} ir_oper_imm_t;

/** IR Variable Reference Operand */
typedef struct {
	/** Base Object */
	ir_oper_t oper;
	/** Variable Name */
	char *varname;
} ir_oper_var_t;

/** IR instruction */
typedef struct {
	/** Instruction Type */
	ir_instr_type_t itype;
	/** Operation Width in Bits */
	unsigned width;
	/** Destination */
	ir_oper_t *dest;
	/** Left Operand */
	ir_oper_t *op1;
	/** Right Operand */
	ir_oper_t *op2;
} ir_instr_t;

/** IR Labeled Block Entry */
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

/** IR Labeled Block */
typedef struct ir_lblock {
	/** Entries */
	list_t entries; /* of ir_lblock_entry_t */
} ir_lblock_t;

/** IR Declaration Type */
typedef enum {
	/** Procedure declaration */
	ird_proc
} ir_decln_type_t;

/** IR Declaration */
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

/** IR Procedure Definition */
typedef struct {
	/** Base object */
	ir_decln_t decln;
	/** Indentifier */
	char *ident;
	/** Labeled block containing the implementation */
	ir_lblock_t *lblock;
} ir_proc_t;

/** IR Module.
 */
typedef struct ir_module {
	/** Declarations */
	list_t declns; /* of ir_decln_t */
} ir_module_t;

#endif
