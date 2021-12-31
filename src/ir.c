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

#include <adt/list.h>
#include <assert.h>
#include <inttypes.h>
#include <ir.h>
#include <merrno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ir_decln_destroy(ir_decln_t *);

/** Instruction names */
static const char *instr_name[] = {
	[iri_add] = "add",
	[iri_call] = "call",
	[iri_sub] = "sub",
	[iri_imm] = "imm",
	[iri_jmp] = "jmp",
	[iri_read] = "read",
	[iri_retv] = "retv",
	[iri_varptr] = "varptr",
	[iri_write] = "write"
};

/** @c true iff instruction has bit width specifier */
static bool instr_has_width[] = {
	[iri_add] = true,
	[iri_sub] = true,
	[iri_imm] = true,
	[iri_read] = true,
	[iri_retv] = true,
	[iri_varptr] = true,
	[iri_write] = true
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

	rv = fprintf(f, "int.%u %" PRId32, dentry->width, dentry->value);
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

	rv = fprintf(f, "uint.%u %" PRId32, dentry->width, dentry->value);
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
int ir_dentry_create_int(unsigned width, int32_t value, ir_dentry_t **rdentry)
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
int ir_dentry_create_uint(unsigned width, int32_t value, ir_dentry_t **rdentry)
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

	if ((proc->flags & irp_extern) != 0) {
		rv = fputs(" extern", f);
		if (rv < 0)
			return EIO;
	}

	if (proc->lblock != NULL) {
		rv = fprintf(f, "\nbegin\n");
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

/** Destroy IR procedure.
 *
 * @param proc IR procedure or @c NULL
 */
void ir_proc_destroy(ir_proc_t *proc)
{
	ir_proc_arg_t *arg;

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
 * @param rarg Place to store pointer to new argument
 * @return EOK on success, ENOMEM if out of memory
 */
int ir_proc_arg_create(const char *ident, ir_proc_arg_t **rarg)
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

	rv = fputs(arg->ident, f);
	if (rv < 0)
		return EIO;

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
int ir_oper_imm_create(int32_t value, ir_oper_imm_t **rimm)
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

	rv = fprintf(f, "%" PRId32, imm->value);
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
		return ir_oper_imm_destroy((ir_oper_imm_t *) oper->ext);
	case iro_list:
		return ir_oper_list_destroy((ir_oper_list_t *) oper->ext);
	case iro_var:
		return ir_oper_var_destroy((ir_oper_var_t *) oper->ext);
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
