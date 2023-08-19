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
	/** Binary AND */
	iri_and,
	/** Binary NOT */
	iri_bnot,
	/** Call */
	iri_call,
	/** Equal */
	iri_eq,
	/** Greater than */
	iri_gt,
	/** Greater than unsigned */
	iri_gtu,
	/** Greater than or equal */
	iri_gteq,
	/** Greater than or equal unsigned */
	iri_gteu,
	/** Introduce immediate value */
	iri_imm,
	/** Jump */
	iri_jmp,
	/** Jump if not zero */
	iri_jnz,
	/** Jump if zero */
	iri_jz,
	/** Less than */
	iri_lt,
	/** Less than unsigned */
	iri_ltu,
	/** Less than or equal */
	iri_lteq,
	/** Less than or equal unsigned */
	iri_lteu,
	/** Get ponter to local variable */
	iri_lvarptr,
	/** Multiplication */
	iri_mul,
	/** Negate */
	iri_neg,
	/** Not equal */
	iri_neq,
	/** No operation */
	iri_nop,
	/** Binary OR */
	iri_or,
	/** Index pointer */
	iri_ptridx,
	/** Read from memory */
	iri_read,
	/** Copy record */
	iri_reccopy,
	/** Record member */
	iri_recmbr,
	/** Return */
	iri_ret,
	/** Return value */
	iri_retv,
	/** Sign-extend integer */
	iri_sgnext,
	/** Shift left */
	iri_shl,
	/** Shift right arithmetic */
	iri_shra,
	/** Shift right logical */
	iri_shrl,
	/** Subtraction */
	iri_sub,
	/** Truncate integer */
	iri_trunc,
	/** Get pointer to global variable */
	iri_varptr,
	/** Write to memory */
	iri_write,
	/** Binary XOR */
	iri_xor,
	/** Zero-extend integer */
	iri_zrext
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
	int64_t value;
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
	/** Type operand (third operand) */
	struct ir_texpr *opt;
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
	/** Integer */
	ird_int,
	/** Pointer */
	ird_ptr
} ir_dentry_type_t;

/** IR data entry */
typedef struct {
	/** Data entry type */
	ir_dentry_type_t dtype;
	/** Data entry width in bits */
	unsigned width;
	/** Symbol */
	char *symbol;
	/** Value */
	int64_t value;
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
	ird_proc,
	/** Record type declaration */
	ird_record
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

/** IR type type */
typedef enum {
	/** Integer type */
	irt_int,
	/** Pointer type */
	irt_ptr,
	/** Array tyoe */
	irt_array,
	/** Identified type */
	irt_ident
} ir_tetype_t;

/** Integer type */
typedef struct {
	/** Number of bits */
	unsigned width;
} ir_type_int_t;

/** Pointer type */
typedef struct {
	/** Number of bits */
	unsigned width;
} ir_type_ptr_t;

/** Array type */
typedef struct {
	/** Array size */
	uint64_t asize;
	/** Element type expression */
	struct ir_texpr *etexpr;
} ir_type_array_t;

/** Identified type */
typedef struct {
	/* Type identifier */
	char *ident;
} ir_type_ident_t;

/** IR type expression */
typedef struct ir_texpr {
	/** Type expression type */
	ir_tetype_t tetype;
	union {
		ir_type_int_t tint;
		ir_type_ptr_t tptr;
		ir_type_array_t tarray;
		ir_type_ident_t tident;
	} t;
} ir_texpr_t;

/** IR variable definition */
typedef struct {
	/** Base object */
	ir_decln_t decln;
	/** Indentifier */
	char *ident;
	/** Variable type */
	ir_texpr_t *vtype;
	/** Data block containing variable data */
	ir_dblock_t *dblock;
} ir_var_t;

/** IR record type element */
typedef struct {
	/** Containing IR record */
	struct ir_record *record;
	/** Link to @c record->elems */
	link_t lelems;
	/** Identifier */
	char *ident;
	/** Element type */
	ir_texpr_t *etype;
} ir_record_elem_t;

/** IR record type (struct/union) */
typedef enum {
	/** Struct */
	irrt_struct,
	/** Union */
	irrt_union
} ir_record_type_t;

/** IR record type definition */
typedef struct ir_record {
	/** Base object */
	ir_decln_t decln;
	/** Record type */
	ir_record_type_t rtype;
	/** Indentifier */
	char *ident;
	/** Elements (of ir_record_elem_t) */
	list_t elems;
} ir_record_t;

/** IR argument in procedure definition */
typedef struct {
	/** Containing procedure definition */
	struct ir_proc *proc;
	/** Link to @c proc->args */
	link_t largs;
	/** Identifier */
	char *ident;
	/** Argument type */
	ir_texpr_t *atype;
} ir_proc_arg_t;

/** IR procedure attribute */
typedef struct {
	/** Containing procedure definition */
	struct ir_proc *proc;
	/** Link to @c proc->attrs */
	link_t lattrs;
	/** Attribute identifier */
	char *ident;
} ir_proc_attr_t;

/** IR procedure flags */
typedef enum {
	/** Extern procedure declaration */
	irp_extern = 0x1
} ir_proc_flags_t;

/** IR local variable */
typedef struct {
	/** Containing procedure definition */
	struct ir_proc *proc;
	/** Link to @c proc->lvars */
	link_t llvars;
	/** Identifier */
	char *ident;
	/** Variable type */
	ir_texpr_t *vtype;
} ir_lvar_t;

/** IR procedure definition */
typedef struct ir_proc {
	/** Base object */
	ir_decln_t decln;
	/** Indentifier */
	char *ident;
	/** Arguments */
	list_t args; /* of ir_proc_arg_t */
	/** Return type */
	ir_texpr_t *rtype;
	/** Attributes */
	list_t attrs; /* of ir_proc_attr_t */
	/** Flags */
	ir_proc_flags_t flags;
	/** Local variables */
	list_t lvars;
	/** Labeled block containing the implementation */
	ir_lblock_t *lblock;
} ir_proc_t;

/** IR module */
typedef struct ir_module {
	/** Declarations */
	list_t declns; /* of ir_decln_t */
} ir_module_t;

#endif
