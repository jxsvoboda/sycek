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
 * Intermediate Representation
 */

#include <adt/list.h>
#include <assert.h>
#include <inttypes.h>
#include <ir.h>
#include <merrno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ir_decln_destroy(ir_decln_t *);

/** Instruction names */
static const char *instr_name[] = {
	[iri_add] = "add",
	[iri_and] = "and",
	[iri_bnot] = "bnot",
	[iri_call] = "call",
	[iri_eq] = "eq",
	[iri_gt] = "gt",
	[iri_gteq] = "gteq",
	[iri_sub] = "sub",
	[iri_shl] = "shl",
	[iri_shra] = "shra",
	[iri_shrl] = "shrl",
	[iri_imm] = "imm",
	[iri_jmp] = "jmp",
	[iri_jnz] = "jnz",
	[iri_jz] = "jz",
	[iri_lt] = "lt",
	[iri_lteq] = "lteq",
	[iri_lvarptr] = "lvarptr",
	[iri_mul] = "mul",
	[iri_neg] = "neg",
	[iri_neq] = "neq",
	[iri_nop] = "nop",
	[iri_or] = "or",
	[iri_read] = "read",
	[iri_ret] = "ret",
	[iri_retv] = "retv",
	[iri_varptr] = "varptr",
	[iri_write] = "write",
	[iri_xor] = "xor"
};

/** @c true iff instruction has bit width specifier */
static bool instr_has_width[] = {
	[iri_add] = true,
	[iri_and] = true,
	[iri_bnot] = true,
	[iri_eq] = true,
	[iri_gt] = true,
	[iri_gtu] = true,
	[iri_gteq] = true,
	[iri_gteu] = true,
	[iri_shl] = true,
	[iri_shra] = true,
	[iri_shrl] = true,
	[iri_sub] = true,
	[iri_imm] = true,
	[iri_lt] = true,
	[iri_ltu] = true,
	[iri_lteq] = true,
	[iri_lteu] = true,
	[iri_lvarptr] = true,
	[iri_mul] = true,
	[iri_neg] = true,
	[iri_neq] = true,
	[iri_or] = true,
	[iri_read] = true,
	[iri_retv] = true,
	[iri_varptr] = true,
	[iri_write] = true,
	[iri_xor] = true
};

/** Create IR module.
 *
 * @param rmodule Place to store pointer to new module.
 * @return EOK on success, ENOMEM if out of memory.
 */
int ir_module_create(ir_module_t **rmodule)
{
	ir_module_t *module;

	module = calloc(1, sizeof(ir_module_t));
	if (module == NULL)
		return ENOMEM;

	list_initialize(&module->declns);
	*rmodule = module;
	return EOK;
}

/** Append declaration to IR module.
 *
 * @param module IR module
 * @param decln Declaration
 */
void ir_module_append(ir_module_t *module, ir_decln_t *decln)
{
	assert(decln->module == NULL);
	decln->module = module;
	list_append(&decln->ldeclns, &module->declns);
}

/** Get first declaration in IR module.
 *
 * @param module IR module
 * @return First declaration or @c NULL if there is none
 */
ir_decln_t *ir_module_first(ir_module_t *module)
{
	link_t *link;

	link = list_first(&module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_decln_t, ldeclns);
}

/** Get next declaration in IR module.
 *
 * @param cur Current declaration
 * @return Next declaration or @c NULL if there is none
 */
ir_decln_t *ir_module_next(ir_decln_t *cur)
{
	link_t *link;

	link = list_next(&cur->ldeclns, &cur->module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_decln_t, ldeclns);
}

/** Get last declaration in IR module.
 *
 * @param module IR module
 * @return Last declaration or @c NULL if there is none
 */
ir_decln_t *ir_module_last(ir_module_t *module)
{
	link_t *link;

	link = list_last(&module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_decln_t, ldeclns);
}

/** Find declaration by name.
 *
 * @param module IR module
 * @param ident Identifier
 * @param rdecln Place to store pointer to declaration
 * @return EOK on success, ENOENT if not found
 */
int ir_module_find(ir_module_t *module, const char *ident, ir_decln_t **rdecln)
{
	ir_decln_t *decln;
	const char *dident;

	decln = ir_module_first(module);
	while (decln != NULL) {
		dident = ir_decln_ident(decln);
		if (strcmp(dident, ident) == 0) {
			*rdecln = decln;
			return EOK;
		}

		decln = ir_module_next(decln);
	}

	return ENOENT;
}

/** Get previous declaration in IR module.
 *
 * @param cur Current declaration
 * @return Previous declaration or @c NULL if there is none
 */
ir_decln_t *ir_module_prev(ir_decln_t *cur)
{
	link_t *link;

	link = list_prev(&cur->ldeclns, &cur->module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_decln_t, ldeclns);
}

/** Print IR module.
 *
 * @param module IR module
 * @param f Output file
 */
int ir_module_print(ir_module_t *module, FILE *f)
{
	ir_decln_t *decln;
	int rc;

	decln = ir_module_first(module);
	while (decln != NULL) {
		rc = ir_decln_print(decln, f);
		if (rc != EOK)
			return rc;

		decln = ir_module_next(decln);
	}

	return EOK;
}

/** Destroy IR module.
 *
 * @param module IR module or @c NULL
 */
void ir_module_destroy(ir_module_t *module)
{
	ir_decln_t *decln;

	if (module == NULL)
		return;

	decln = ir_module_first(module);
	while (decln != NULL) {
		list_remove(&decln->ldeclns);
		ir_decln_destroy(decln);

		decln = ir_module_first(module);
	}

	free(module);
}

/** Destroy IR declaration.
 *
 * @param decln IR declaration or @c NULL
 */
static void ir_decln_destroy(ir_decln_t *decln)
{
	if (decln == NULL)
		return;

	switch (decln->dtype) {
	case ird_var:
		ir_var_destroy((ir_var_t *) decln->ext);
		break;
	case ird_proc:
		ir_proc_destroy((ir_proc_t *) decln->ext);
		break;
	}
}

/** Print IR declaration.
 *
 * @param decln IR declaration
 * @param f Output file
 *
 * @return EOK on success or an error code
 */
int ir_decln_print(ir_decln_t *decln, FILE *f)
{
	int rc;
	int rv;

	rc = EINVAL;
	(void)rc;
	switch (decln->dtype) {
	case ird_var:
		rc = ir_var_print((ir_var_t *) decln->ext, f);
		break;
	case ird_proc:
		rc = ir_proc_print((ir_proc_t *) decln->ext, f);
		break;
	}

	if (rc != EOK)
		return rc;

	rv = fprintf(f, ";\n");
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Get IR declaration identifier.
 *
 * @param decln IR declaration
 * @return Identifier
 */
const char *ir_decln_ident(ir_decln_t *decln)
{
	switch (decln->dtype) {
	case ird_var:
		return ((ir_var_t *)decln->ext)->ident;
	case ird_proc:
		return ((ir_proc_t *)decln->ext)->ident;
	}

	return NULL;
}

/** Create IR variable.
 *
 * @param ident Identifier (will be copied)
 * @param dblock Data block
 * @param rvar Place to store pointer to new variable
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_var_create(const char *ident, ir_dblock_t *dblock, ir_var_t **rvar)
{
	ir_var_t *var;

	var = calloc(1, sizeof(ir_var_t));
	if (var == NULL)
		return ENOMEM;

	var->ident = strdup(ident);
	if (var->ident == NULL) {
		free(var);
		return ENOMEM;
	}

	assert(dblock != NULL);
	var->dblock = dblock;
	var->decln.dtype = ird_var;
	var->decln.ext = (void *) var;
	*rvar = var;
	return EOK;
}

/** Print IR variable.
 *
 * @param proc IR procedure
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_var_print(ir_var_t *var, FILE *f)
{
	int rv;
	int rc;

	rv = fprintf(f, "\nvar %s\n", var->ident);
	if (rv < 0)
		return EIO;

	rv = fprintf(f, "begin\n");
	if (rv < 0)
		return EIO;

	rc = ir_dblock_print(var->dblock, f);
	if (rc != EOK)
		return EIO;

	rv = fprintf(f, "end");
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy IR variable.
 *
 * @param var IR variable or @c NULL
 */
void ir_var_destroy(ir_var_t *var)
{
	if (var == NULL)
		return;

	if (var->ident != NULL)
		free(var->ident);

	ir_dblock_destroy(var->dblock);
	free(var);
}

/** Create IR data block.
 *
 * @param rdblock Place to store pointer to new data block.
 * @return EOK on success, ENOMEM if out of memory.
 */
int ir_dblock_create(ir_dblock_t **rdblock)
{
	ir_dblock_t *dblock;

	dblock = calloc(1, sizeof(ir_dblock_t));
	if (dblock == NULL)
		return ENOMEM;

	list_initialize(&dblock->entries);
	*rdblock = dblock;
	return EOK;
}

/** Append data entry to IR data block.
 *
 * @param dblock IR data block
 * @param dentry Data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_dblock_append(ir_dblock_t *dblock, ir_dentry_t *dentry)
{
	ir_dblock_entry_t *entry;

	entry = calloc(1, sizeof(ir_dblock_entry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->dblock = dblock;
	list_append(&entry->lentries, &dblock->entries);

	entry->dentry = dentry;
	return EOK;
}

/** Print IR integer data entry.
 *
 * @param dentry IR integer data entry
 * @param f Output file
 * @return EOK on success or an error code
 */
static int ir_dentry_int_print(ir_dentry_t *dentry, FILE *f)
{
	int rv;

	assert(dentry->dtype == ird_int);

	rv = fprintf(f, "int.%u %" PRId64, dentry->width, dentry->value);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print IR unsigned integer data entry.
 *
 * @param dentry IR unsigned integer data entry
 * @param f Output file
 * @return EOK on success or an error code
 */
static int ir_dentry_uint_print(ir_dentry_t *dentry, FILE *f)
{
	int rv;

	assert(dentry->dtype == ird_uint);

	rv = fprintf(f, "uint.%u %" PRIu64, dentry->width, dentry->value);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print IR data block.
 *
 * @param dblock Labeled block
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_dblock_print(ir_dblock_t *dblock, FILE *f)
{
	ir_dblock_entry_t *entry;
	int rc;

	entry = ir_dblock_first(dblock);
	while (entry != NULL) {
		rc = ir_dentry_print(entry->dentry, f);
		if (rc != EOK)
			return rc;

		entry = ir_dblock_next(entry);
	}

	return EOK;
}

/** Destroy IR data block.
 *
 * @param dblock Data block or @c NULL
 */
void ir_dblock_destroy(ir_dblock_t *dblock)
{
	ir_dblock_entry_t *entry;

	if (dblock == NULL)
		return;

	entry = ir_dblock_first(dblock);
	while (entry != NULL) {
		list_remove(&entry->lentries);
		ir_dentry_destroy(entry->dentry);
		free(entry);

		entry = ir_dblock_first(dblock);
	}

	free(dblock);
}

/** Create IR integer data entry.
 *
 * @param width Width of data entry in bits
 * @param value Value
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_dentry_create_int(unsigned width, int64_t value, ir_dentry_t **rdentry)
{
	ir_dentry_t *dentry;

	dentry = calloc(1, sizeof(ir_dentry_t));
	if (dentry == NULL)
		return ENOMEM;

	dentry->dtype = ird_int;
	dentry->width = width;
	dentry->value = value;

	*rdentry = dentry;
	return EOK;
}

/** Create IR unsigned integer data entry.
 *
 * @param width Width of data entry in bits
 * @param value Value
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_dentry_create_uint(unsigned width, uint64_t value, ir_dentry_t **rdentry)
{
	ir_dentry_t *dentry;

	dentry = calloc(1, sizeof(ir_dentry_t));
	if (dentry == NULL)
		return ENOMEM;

	dentry->dtype = ird_uint;
	dentry->width = width;
	dentry->value = value;

	*rdentry = dentry;
	return EOK;
}

/** Print IR data entry.
 *
 * @param dblock Labeled block
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_dentry_print(ir_dentry_t *dentry, FILE *f)
{
	int rc;
	int rv;

	rv = fputc('\t', f);
	if (rv < 0)
		return EIO;

	switch (dentry->dtype) {
	case ird_int:
		rc = ir_dentry_int_print(dentry, f);
		break;
	case ird_uint:
		rc = ir_dentry_uint_print(dentry, f);
		break;
	default:
		assert(false);
		rc = ENOTSUP;
		break;
	}

	if (rc != EOK)
		return rc;

	rv = fputs(";\n", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy IR data entry.
 *
 * @param dentry Data entry or @c NULL
 */
void ir_dentry_destroy(ir_dentry_t *dentry)
{
	if (dentry == NULL)
		return;

	free(dentry);
}

/** Get first entry in IR data block.
 *
 * @param dblock IR data block
 * @return First entry or @c NULL if there is none
 */
ir_dblock_entry_t *ir_dblock_first(ir_dblock_t *dblock)
{
	link_t *link;

	link = list_first(&dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_dblock_entry_t, lentries);
}

/** Get next entry in IR data block.
 *
 * @param cur Current entry
 * @return Next entry or @c NULL if there is none
 */
ir_dblock_entry_t *ir_dblock_next(ir_dblock_entry_t *cur)
{
	link_t *link;

	link = list_next(&cur->lentries, &cur->dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_dblock_entry_t, lentries);
}

/** Get last entry in IR data block.
 *
 * @param dblock IR data block
 * @return Last entry or @c NULL if there is none
 */
ir_dblock_entry_t *ir_dblock_last(ir_dblock_t *dblock)
{
	link_t *link;

	link = list_last(&dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_dblock_entry_t, lentries);
}

/** Get previous entry in IR data block.
 *
 * @param cur Current entry
 * @return Previous entry or @c NULL if there is none
 */
ir_dblock_entry_t *ir_dblock_prev(ir_dblock_entry_t *cur)
{
	link_t *link;

	link = list_prev(&cur->lentries, &cur->dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_dblock_entry_t, lentries);
}

/** Create IR procedure.
 *
 * @param ident Identifier (will be copied)
 * @parma flags Flags
 * @param lblock Labeled block (or @c NULL if not a definition)
 * @param rproc Place to store pointer to new procedure
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_proc_create(const char *ident, ir_proc_flags_t flags,
    ir_lblock_t *lblock, ir_proc_t **rproc)
{
	ir_proc_t *proc;

	proc = calloc(1, sizeof(ir_proc_t));
	if (proc == NULL)
		return ENOMEM;

	proc->ident = strdup(ident);
	if (proc->ident == NULL) {
		free(proc);
		return ENOMEM;
	}

	proc->flags = flags;

	assert(lblock != NULL || (flags & irp_extern) != 0);
	proc->lblock = lblock;
	proc->decln.dtype = ird_proc;
	proc->decln.ext = (void *) proc;
	list_initialize(&proc->args);
	list_initialize(&proc->attrs);
	list_initialize(&proc->lvars);
	*rproc = proc;
	return EOK;
}

/** Print IR procedure.
 *
 * @param proc IR procedure
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_proc_print(ir_proc_t *proc, FILE *f)
{
	ir_proc_arg_t *arg;
	ir_proc_attr_t *attr;
	ir_lvar_t *lvar;
	int rv;
	int rc;

	rv = fprintf(f, "\nproc %s(", proc->ident);
	if (rv < 0)
		return EIO;

	/* Print argument list */
	arg = ir_proc_first_arg(proc);
	if (arg != NULL) {
		rc = ir_proc_arg_print(arg, f);
		if (rc != EOK)
			return rc;

		arg = ir_proc_next_arg(arg);
		while (arg != NULL) {
			rv = fputs(", ", f);
			if (rv < 0)
				return EIO;

			rc = ir_proc_arg_print(arg, f);
			if (rc != EOK)
				return rc;

			arg = ir_proc_next_arg(arg);
		}
	}

	rv = fputs(")", f);
	if (rv < 0)
		return EIO;

	attr = ir_proc_first_attr(proc);
	if (attr != NULL) {
		rv = fputs(" attr(", f);
		if (rv < 0)
			return EIO;

		do {
			rc = ir_proc_attr_print(attr, f);
			if (rc != EOK)
				return rc;

			attr = ir_proc_next_attr(attr);
		} while (attr != NULL);

		rv = fputs(")", f);
		if (rv < 0)
			return EIO;
	}

	if ((proc->flags & irp_extern) != 0) {
		rv = fputs(" extern", f);
		if (rv < 0)
			return EIO;
	} else {
		rv = fputs("\n", f);
		if (rv < 0)
			return EIO;
	}

	/* Print local variables */
	lvar = ir_proc_first_lvar(proc);
	if (lvar != NULL) {
		rv = fputs("lvar\n", f);
		if (rv < 0)
			return EIO;

		while (lvar != NULL) {
			rv = fputs("\t", f);
			if (rv < 0)
				return EIO;

			rc = ir_lvar_print(lvar, f);
			if (rc != EOK)
				return rc;

			rv = fputs(";\n", f);
			if (rv < 0)
				return EIO;

			lvar = ir_proc_next_lvar(lvar);
		}
	}

	if (proc->lblock != NULL) {
		rv = fprintf(f, "begin\n");
		if (rv < 0)
			return EIO;

		rc = ir_lblock_print(proc->lblock, f);
		if (rc != EOK)
			return EIO;

		rv = fprintf(f, "end");
		if (rv < 0)
			return EIO;
	}

	return EOK;
}

/** Append argument to IR procedure.
 *
 * @param proc IR procedure
 * @param arg Argument
 */
void ir_proc_append_arg(ir_proc_t *proc, ir_proc_arg_t *arg)
{
	assert(arg->proc == NULL);
	arg->proc = proc;
	list_append(&arg->largs, &proc->args);
}

/** Append attribute to IR procedure.
 *
 * @param proc IR procedure
 * @param attr Attibute
 */
void ir_proc_append_attr(ir_proc_t *proc, ir_proc_attr_t *attr)
{
	assert(attr->proc == NULL);
	attr->proc = proc;
	list_append(&attr->lattrs, &proc->attrs);
}

/** Append local variable to IR procedure.
 *
 * @param proc IR procedure
 * @param lvar Local variable
 */
void ir_proc_append_lvar(ir_proc_t *proc, ir_lvar_t *lvar)
{
	assert(lvar->proc == NULL);
	lvar->proc = proc;
	list_append(&lvar->llvars, &proc->lvars);
}

/** Destroy IR procedure.
 *
 * @param proc IR procedure or @c NULL
 */
void ir_proc_destroy(ir_proc_t *proc)
{
	ir_proc_arg_t *arg;
	ir_proc_attr_t *attr;
	ir_lvar_t *lvar;

	if (proc == NULL)
		return;

	if (proc->ident != NULL)
		free(proc->ident);

	arg = ir_proc_first_arg(proc);
	while (arg != NULL) {
		list_remove(&arg->largs);
		ir_proc_arg_destroy(arg);
		arg = ir_proc_first_arg(proc);
	}

	attr = ir_proc_first_attr(proc);
	while (attr != NULL) {
		list_remove(&attr->lattrs);
		ir_proc_attr_destroy(attr);
		attr = ir_proc_first_attr(proc);
	}

	lvar = ir_proc_first_lvar(proc);
	while (lvar != NULL) {
		list_remove(&lvar->llvars);
		ir_lvar_destroy(lvar);
		lvar = ir_proc_first_lvar(proc);
	}

	ir_lblock_destroy(proc->lblock);
	free(proc);
}

/** Get first argument of IR procedure.
 *
 * @param proc IR procedure
 * @return First argument or @c NULL if there is none
 */
ir_proc_arg_t *ir_proc_first_arg(ir_proc_t *proc)
{
	link_t *link;

	link = list_first(&proc->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_arg_t, largs);
}

/** Get next argument of IR procedure.
 *
 * @param cur Current argument
 * @return Next argument or @c NULL if @a cur is the last argument
 */
ir_proc_arg_t *ir_proc_next_arg(ir_proc_arg_t *cur)
{
	link_t *link;

	link = list_next(&cur->largs, &cur->proc->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_arg_t, largs);
}

/** Get last argument of IR procedure.
 *
 * @param proc IR procedure
 * @return Last argument or @c NULL if there is none
 */
ir_proc_arg_t *ir_proc_last_arg(ir_proc_t *proc)
{
	link_t *link;

	link = list_last(&proc->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_arg_t, largs);
}

/** Get previous argument of IR procedure.
 *
 * @param cur Current argument
 * @return Previous argument or @c NULL if @a cur is the first argument
 */
ir_proc_arg_t *ir_proc_prev_arg(ir_proc_arg_t *cur)
{
	link_t *link;

	link = list_prev(&cur->largs, &cur->proc->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_arg_t, largs);
}

/** Create IR procedure argument.
 *
 * @param ident Argument identifier
 * @param atype Argument type type (ownership transferred)
 * @param rarg Place to store pointer to new argument
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_proc_arg_create(const char *ident,  ir_texpr_t *atype,
    ir_proc_arg_t **rarg)
{
	ir_proc_arg_t *arg;

	arg = calloc(1, sizeof(ir_proc_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->ident = strdup(ident);
	if (arg->ident == NULL) {
		free(arg);
		return ENOMEM;
	}

	arg->atype = atype;
	*rarg = arg;
	return EOK;
}

/** Destroy IR procedure argument.
 *
 * @param arg Argument
 */
void ir_proc_arg_destroy(ir_proc_arg_t *arg)
{
	free(arg->ident);
	ir_texpr_destroy(arg->atype);
	free(arg);
}

/** Print IR procedure argument.
 *
 * @param arg Argument
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_proc_arg_print(ir_proc_arg_t *arg, FILE *f)
{
	int rv;
	int rc;

	rv = fprintf(f, "%s : ", arg->ident);
	if (rv < 0)
		return EIO;

	rc = ir_texpr_print(arg->atype, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Get first attribute of IR procedure.
 *
 * @param proc IR procedure
 * @return First attribute or @c NULL if there is none
 */
ir_proc_attr_t *ir_proc_first_attr(ir_proc_t *proc)
{
	link_t *link;

	link = list_first(&proc->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_attr_t, lattrs);
}

/** Get next attribute of IR procedure.
 *
 * @param cur Current attribute
 * @return Next attribute or @c NULL if @a cur is the last attribute
 */
ir_proc_attr_t *ir_proc_next_attr(ir_proc_attr_t *cur)
{
	link_t *link;

	link = list_next(&cur->lattrs, &cur->proc->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_attr_t, lattrs);
}

/** Get last attribute of IR procedure.
 *
 * @param proc IR procedure
 * @return Last attribute or @c NULL if there is none
 */
ir_proc_attr_t *ir_proc_last_attr(ir_proc_t *proc)
{
	link_t *link;

	link = list_last(&proc->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_attr_t, lattrs);
}

/** Get previous attribute of IR procedure.
 *
 * @param cur Current attribute
 * @return Previous attribute or @c NULL if @a cur is the first attribute
 */
ir_proc_attr_t *ir_proc_prev_attr(ir_proc_attr_t *cur)
{
	link_t *link;

	link = list_prev(&cur->lattrs, &cur->proc->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_proc_attr_t, lattrs);
}

/** Determine if procedure has attribute.
 *
 * @param proc IR procedure
 * @param ident Attribute identifier
 * @return @c true iff @a proc has attribute @a ident.
 */
bool ir_proc_has_attr(ir_proc_t *proc, const char *ident)
{
	ir_proc_attr_t *attr;

	attr = ir_proc_first_attr(proc);
	while (attr != NULL) {
		if (strcmp(attr->ident, ident) == 0)
			return true;
		attr = ir_proc_next_attr(attr);
	}

	return false;
}

/** Create IR procedure attribute.
 *
 * @param ident Attribute identifier
 * @param rattr Place to store pointer to new attribute
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_proc_attr_create(const char *ident, ir_proc_attr_t **rattr)
{
	ir_proc_attr_t *attr;

	attr = calloc(1, sizeof(ir_proc_attr_t));
	if (attr == NULL)
		return ENOMEM;

	attr->ident = strdup(ident);
	if (attr->ident == NULL) {
		free(attr);
		return ENOMEM;
	}

	*rattr = attr;
	return EOK;
}

/** Destroy IR procedure attribute.
 *
 * @param attr Attribute
 */
void ir_proc_attr_destroy(ir_proc_attr_t *attr)
{
	free(attr->ident);
	free(attr);
}

/** Print IR procedure attribute.
 *
 * @param attr Attribute
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_proc_attr_print(ir_proc_attr_t *attr, FILE *f)
{
	int rv;

	rv = fputs(attr->ident, f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Get first local variable of IR procedure.
 *
 * @param proc IR procedure
 * @return First local variable or @c NULL if there is none
 */
ir_lvar_t *ir_proc_first_lvar(ir_proc_t *proc)
{
	link_t *link;

	link = list_first(&proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lvar_t, llvars);
}

/** Get next local variable of IR procedure.
 *
 * @param cur Current local variable
 * @return Next local variable or @c NULL if @a cur is the last local variable
 */
ir_lvar_t *ir_proc_next_lvar(ir_lvar_t *cur)
{
	link_t *link;

	link = list_next(&cur->llvars, &cur->proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lvar_t, llvars);
}

/** Get last local variable of IR procedure.
 *
 * @param proc IR procedure
 * @return Last local variable or @c NULL if there is none
 */
ir_lvar_t *ir_proc_last_lvar(ir_proc_t *proc)
{
	link_t *link;

	link = list_last(&proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lvar_t, llvars);
}

/** Get previous local variable of IR procedure.
 *
 * @param cur Current local variable
 * @return Previous local variable or @c NULL if @a cur is the first local
 *         variable
 */
ir_lvar_t *ir_proc_prev_lvar(ir_lvar_t *cur)
{
	link_t *link;

	link = list_prev(&cur->llvars, &cur->proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lvar_t, llvars);
}

/** Create IR procedure local variable.
 *
 * @param ident Variable identifier (cloned)
 * @param vtype Variable type (ownership transferred)
 * @param rlvar Place to store pointer to new local variable
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_lvar_create(const char *ident, ir_texpr_t *vtype, ir_lvar_t **rlvar)
{
	ir_lvar_t *lvar;

	lvar = calloc(1, sizeof(ir_lvar_t));
	if (lvar == NULL)
		return ENOMEM;

	lvar->ident = strdup(ident);
	if (lvar->ident == NULL) {
		free(lvar);
		return ENOMEM;
	}

	lvar->vtype = vtype;

	*rlvar = lvar;
	return EOK;
}

/** Destroy IR procedure local variable.
 *
 * @param lvar Local variable
 */
void ir_lvar_destroy(ir_lvar_t *lvar)
{
	free(lvar->ident);
	ir_texpr_destroy(lvar->vtype);
	free(lvar);
}

/** Print IR procedure local variable.
 *
 * @param lvar Local variable
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_lvar_print(ir_lvar_t *lvar, FILE *f)
{
	int rv;
	int rc;

	rv = fprintf(f, "%s : ", lvar->ident);
	if (rv < 0)
		return EIO;

	rc = ir_texpr_print(lvar->vtype, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Create IR labeled block.
 *
 * @param rlblock Place to store pointer to new labeled block.
 * @return EOK on success, ENOMEM if out of memory.
 */
int ir_lblock_create(ir_lblock_t **rlblock)
{
	ir_lblock_t *lblock;

	lblock = calloc(1, sizeof(ir_lblock_t));
	if (lblock == NULL)
		return ENOMEM;

	list_initialize(&lblock->entries);
	*rlblock = lblock;
	return EOK;
}

/** Append entry to IR labeled block.
 *
 * @param lblock IR labeled block
 * @param label Label or @c NULL if none
 * @param instr Instruction
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_lblock_append(ir_lblock_t *lblock, const char *label,
    ir_instr_t *instr)
{
	char *dlabel;
	ir_lblock_entry_t *entry;

	if (label != NULL) {
		dlabel = strdup(label);
		if (dlabel == NULL)
			return ENOMEM;
	} else {
		dlabel = NULL;
	}

	entry = calloc(1, sizeof(ir_lblock_entry_t));
	if (entry == NULL) {
		free(dlabel);
		return ENOMEM;
	}

	entry->lblock = lblock;
	list_append(&entry->lentries, &lblock->entries);
	entry->label = dlabel;
	entry->instr = instr;

	return EOK;
}

/** Print IR labeled block.
 *
 * @param lblock Labeled block
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_lblock_print(ir_lblock_t *lblock, FILE *f)
{
	ir_lblock_entry_t *entry;
	int rc;
	int rv;

	entry = ir_lblock_first(lblock);
	while (entry != NULL) {
		if (entry->label != NULL) {
			rv = fprintf(f, "%s:\n", entry->label);
			if (rv < 0)
				return EIO;
		}

		if (entry->instr != NULL) {
			rc = ir_instr_print(entry->instr, f);
			if (rc != EOK)
				return rc;
		}

		entry = ir_lblock_next(entry);
	}

	return EOK;
}

/** Move/append IR labeled block entries to a different labeled block.
 *
 * @param slblock Source labeled block
 * @param dlblock Destination labeled block
 */
void ir_lblock_move_entries(ir_lblock_t *slblock, ir_lblock_t *dlblock)
{
	ir_lblock_entry_t *entry;

	entry = ir_lblock_first(slblock);
	while (entry != NULL) {
		list_remove(&entry->lentries);

		entry->lblock = dlblock;
		list_append(&entry->lentries, &dlblock->entries);

		entry = ir_lblock_first(slblock);
	}
}

/** Destroy IR labeled block.
 *
 * @param lblock Labeled block or @c NULL
 */
void ir_lblock_destroy(ir_lblock_t *lblock)
{
	ir_lblock_entry_t *entry;

	if (lblock == NULL)
		return;

	entry = ir_lblock_first(lblock);
	while (entry != NULL) {
		list_remove(&entry->lentries);
		if (entry->label != NULL)
			free(entry->label);
		ir_instr_destroy(entry->instr);
		free(entry);

		entry = ir_lblock_first(lblock);
	}

	free(lblock);
}

/** Get first entry in IR labeled block.
 *
 * @param lblock IR labeled block
 * @return First entry or @c NULL if there is none
 */
ir_lblock_entry_t *ir_lblock_first(ir_lblock_t *lblock)
{
	link_t *link;

	link = list_first(&lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lblock_entry_t, lentries);
}

/** Get next entry in IR labeled block.
 *
 * @param cur Current entry
 * @return Next entry or @c NULL if there is none
 */
ir_lblock_entry_t *ir_lblock_next(ir_lblock_entry_t *cur)
{
	link_t *link;

	link = list_next(&cur->lentries, &cur->lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lblock_entry_t, lentries);
}

/** Get last entry in IR labeled block.
 *
 * @param lblock IR labeled block
 * @return Last entry or @c NULL if there is none
 */
ir_lblock_entry_t *ir_lblock_last(ir_lblock_t *lblock)
{
	link_t *link;

	link = list_last(&lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lblock_entry_t, lentries);
}

/** Get previous entry in IR labeled block.
 *
 * @param cur Current entry
 * @return Previous entry or @c NULL if there is none
 */
ir_lblock_entry_t *ir_lblock_prev(ir_lblock_entry_t *cur)
{
	link_t *link;

	link = list_prev(&cur->lentries, &cur->lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_lblock_entry_t, lentries);
}

/** Create IR instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_instr_create(ir_instr_t **rinstr)
{
	ir_instr_t *instr;

	instr = calloc(1, sizeof(ir_instr_t));
	if (instr == NULL)
		return ENOMEM;

	*rinstr = instr;
	return EOK;
}

/** Print IR instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
int ir_instr_print(ir_instr_t *instr, FILE *f)
{
	int rc;
	int rv;

	if (instr_has_width[instr->itype]) {
		rv = fprintf(f, "\t%s.%u ", instr_name[instr->itype],
		    instr->width);
		if (rv < 0)
			return EIO;
	} else {
		rv = fprintf(f, "\t%s ", instr_name[instr->itype]);
		if (rv < 0)
			return EIO;
	}

	if (instr->dest != NULL) {
		rc = ir_oper_print(instr->dest, f);
		if (rc != EOK)
			return rc;

	} else {
		rv = fputs("nil", f);
		if (rv < 0)
			return EIO;
	}

	if (instr->op1 != NULL) {
		rv = fputs(", ", f);
		if (rv < 0)
			return EIO;

		rc = ir_oper_print(instr->op1, f);
		if (rc != EOK)
			return rc;
	}

	if (instr->op2 != NULL) {
		rv = fputs(", ", f);
		if (rv < 0)
			return EIO;

		rc = ir_oper_print(instr->op2, f);
		if (rc != EOK)
			return rc;
	}

	rv = fputs(";\n", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy IR instruction.
 *
 * @param instr IR instruction or @c NULL
 */
void ir_instr_destroy(ir_instr_t *instr)
{
	if (instr == NULL)
		return;

	ir_oper_destroy(instr->dest);
	ir_oper_destroy(instr->op1);
	ir_oper_destroy(instr->op2);
	free(instr);
}

/** Create IR immediate operand.
 *
 * @param value Value
 * @param rimm Place to store pointer to new IR immediate operand
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_oper_imm_create(int64_t value, ir_oper_imm_t **rimm)
{
	ir_oper_imm_t *imm;

	imm = calloc(1, sizeof(ir_oper_imm_t));
	if (imm == NULL)
		return ENOMEM;

	imm->oper.optype = iro_imm;
	imm->oper.ext = (void *) imm;
	imm->value = value;

	*rimm = imm;
	return EOK;
}

/** Create IR list operand.
 *
 * @param rlist Place to store pointer to new IR list operand
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_oper_list_create(ir_oper_list_t **rlist)
{
	ir_oper_list_t *list;

	list = calloc(1, sizeof(ir_oper_list_t));
	if (list == NULL)
		return ENOMEM;

	list->oper.optype = iro_list;
	list->oper.ext = (void *) list;
	list_initialize(&list->list);

	*rlist = list;
	return EOK;
}

/** Create IR variable operand.
 *
 * @param value Value
 * @param rvar Place to store pointer to new IR variable operand
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_oper_var_create(const char *varname, ir_oper_var_t **rvar)
{
	ir_oper_var_t *var;
	char *dvarname;

	dvarname = strdup(varname);
	if (dvarname == NULL)
		return ENOMEM;

	var = calloc(1, sizeof(ir_oper_var_t));
	if (var == NULL) {
		free(dvarname);
		return ENOMEM;
	}

	var->oper.optype = iro_var;
	var->oper.ext = (void *) var;
	var->varname = dvarname;

	*rvar = var;
	return EOK;
}

/** Print IR immediate operand.
 *
 * @param imm IR immediate operand
 * @param f Output file
 * @return EOK on success or an error code
 */
static int ir_oper_imm_print(ir_oper_imm_t *imm, FILE *f)
{
	int rv;

	rv = fprintf(f, "%" PRId64, imm->value);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print IR list operand.
 *
 * @param list IR list operand
 * @param f Output file
 * @return EOK on success or an error code
 */
static int ir_oper_list_print(ir_oper_list_t *list, FILE *f)
{
	ir_oper_t *oper;
	int rc;
	int rv;

	rv = fputs("{", f);
	if (rv < 0)
		return EIO;

	oper = ir_oper_list_first(list);

	while (oper != NULL) {
		rv = fputs(" ", f);
		if (rv < 0)
			return EIO;

		rc = ir_oper_print(oper, f);
		if (rc != EOK)
			return rc;

		oper = ir_oper_list_next(oper);
		if (oper == NULL)
			break;

		rv = fputs(",", f);
		if (rv < 0)
			return EIO;

	}

	rv = fputs(" }", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print IR variable operand.
 *
 * @param var IR variable operand
 * @param f Output file
 * @return EOK on success or an error code
 */
static int ir_oper_var_print(ir_oper_var_t *var, FILE *f)
{
	int rv;

	rv = fputs(var->varname, f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print IR operand.
 *
 * @param oper IR operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_oper_print(ir_oper_t *oper, FILE *f)
{
	switch (oper->optype) {
	case iro_imm:
		return ir_oper_imm_print((ir_oper_imm_t *) oper->ext, f);
	case iro_list:
		return ir_oper_list_print((ir_oper_list_t *) oper->ext, f);
	case iro_var:
		return ir_oper_var_print((ir_oper_var_t *) oper->ext, f);
	}

	assert(false);
	return EIO;
}

/** Destroy IR immediate operand.
 *
 * @param imm IR immediate operand
 */
static void ir_oper_imm_destroy(ir_oper_imm_t *imm)
{
	free(imm);
}

/** Destroy IR list operand.
 *
 * @param list IR list operand
 */
static void ir_oper_list_destroy(ir_oper_list_t *list)
{
	ir_oper_t *oper;

	oper = ir_oper_list_first(list);
	while (oper != NULL) {
		list_remove(&oper->llist);
		ir_oper_destroy(oper);

		oper = ir_oper_list_first(list);
	}

	free(list);
}

/** Destroy IR variable operand.
 *
 * @param var IR variable operand
 */
static void ir_oper_var_destroy(ir_oper_var_t *var)
{
	free(var->varname);
	free(var);
}

/** Destroy IR operand.
 *
 * @param oper IR operand or @c NULL
 */
void ir_oper_destroy(ir_oper_t *oper)
{
	if (oper == NULL)
		return;

	switch (oper->optype) {
	case iro_imm:
		ir_oper_imm_destroy((ir_oper_imm_t *) oper->ext);
		break;
	case iro_list:
		ir_oper_list_destroy((ir_oper_list_t *) oper->ext);
		break;
	case iro_var:
		ir_oper_var_destroy((ir_oper_var_t *) oper->ext);
		break;
	}
}

/** Append entry to IR list operand.
 *
 * @param list IR list operand
 * @param oper IR operand (new list entry)
 */
void ir_oper_list_append(ir_oper_list_t *list, ir_oper_t *oper)
{
	assert(oper->parent == NULL);
	oper->parent = list;
	list_append(&oper->llist, &list->list);
}

/** Get first entry in IR list operand.
 *
 * @param list IR list operand
 * @return First entry or @c NULL if there is none
 */
ir_oper_t *ir_oper_list_first(ir_oper_list_t *list)
{
	link_t *link;

	link = list_first(&list->list);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_oper_t, llist);
}

/** Get next entry in IR list operand.
 *
 * @param cur Current entry
 * @return Next entry or @c NULL if there is none
 */
ir_oper_t *ir_oper_list_next(ir_oper_t *cur)
{
	link_t *link;

	link = list_next(&cur->llist, &cur->parent->list);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_oper_t, llist);
}

/** Get last entry in IR list operand.
 *
 * @param list IR list operand
 * @return Last entry or @c NULL if there is none
 */
ir_oper_t *ir_oper_list_last(ir_oper_list_t *list)
{
	link_t *link;

	link = list_last(&list->list);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_oper_t, llist);
}

/** Get previous entry in IR list operand.
 *
 * @param cur Current entry
 * @return Previous entry or @c NULL if there is none
 */
ir_oper_t *ir_oper_list_prev(ir_oper_t *cur)
{
	link_t *link;

	link = list_prev(&cur->llist, &cur->parent->list);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ir_oper_t, llist);
}

/** Create IR integer type expression.
 *
 * @param width Number of bits
 * @param rtexpr Place to store pointer to new type expression
 * @return EOK on success or an error code
 */
int ir_texpr_int_create(unsigned width, ir_texpr_t **rtexpr)
{
	ir_texpr_t *texpr;

	texpr = calloc(1, sizeof(ir_texpr_t));
	if (texpr == NULL)
		return ENOMEM;

	texpr->tetype = irt_int;
	texpr->t.tint.width = width;
	*rtexpr = texpr;
	return EOK;
}

/** Print IR integer type expression.
 *
 * @param irtype IR integer type expression
 * @param f Output file
 * @return EOK on success or an error code
 */
static int ir_texpr_int_print(ir_texpr_t *texpr, FILE *f)
{
	int rv;

	assert(texpr->tetype == irt_int);

	rv = fprintf(f, "int.%u", texpr->t.tint.width);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print IR type expression.
 *
 * @param irtype IR type expression
 * @param f Output file
 * @return EOK on success or an error code
 */
int ir_texpr_print(ir_texpr_t *texpr, FILE *f)
{
	switch (texpr->tetype) {
	case irt_int:
		return ir_texpr_int_print(texpr, f);
	}

	assert(false);
	return EIO;
}

/** Get size of type described by IR integer type expression in bytes.
 *
 * @param irtype IR integer type expression
 * @return Size in bytes
 */
static size_t ir_texpr_int_sizeof(ir_texpr_t *texpr)
{
	assert(texpr->tetype == irt_int);

	/* Convert bits to bytes */
	return (texpr->t.tint.width + 7) / 8;
}

/** Get size of type described by IR type expression in bytes.
 *
 * @param irtype IR type expression
 * @return Size in bytes
 */
size_t ir_texpr_sizeof(ir_texpr_t *texpr)
{
	switch (texpr->tetype) {
	case irt_int:
		return ir_texpr_int_sizeof(texpr);
	}

	assert(false);
	return 0;
}

/** Destroy type expression.
 *
 * @param texpr Type expression
 */
void ir_texpr_destroy(ir_texpr_t *texpr)
{
	free(texpr);
}
