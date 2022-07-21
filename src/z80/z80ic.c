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
 * Z80 Instruction code
 */

#include <adt/list.h>
#include <assert.h>
#include <inttypes.h>
#include <merrno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <z80/z80ic.h>

static void z80ic_decln_destroy(z80ic_decln_t *);

/** Z80 register names */
static const char *z80ic_reg_name[] = {
	[z80ic_reg_a] = "A",
	[z80ic_reg_b] = "B",
	[z80ic_reg_c] = "C",
	[z80ic_reg_d] = "D",
	[z80ic_reg_e] = "E",
	[z80ic_reg_h] = "H",
	[z80ic_reg_l] = "L"
};

/** Z80 16-bit dd register names */
static const char *z80ic_dd_name[] = {
	[z80ic_pp_bc] = "BC",
	[z80ic_pp_de] = "DE",
	[z80ic_pp_ix] = "HL",
	[z80ic_pp_sp] = "SP"
};

/** Z80 16-bit pp register names */
static const char *z80ic_pp_name[] = {
	[z80ic_pp_bc] = "BC",
	[z80ic_pp_de] = "DE",
	[z80ic_pp_ix] = "IX",
	[z80ic_pp_sp] = "SP"
};

/** Z80 16-bit ss register names */
static const char *z80ic_ss_name[] = {
	[z80ic_ss_bc] = "BC",
	[z80ic_ss_de] = "DE",
	[z80ic_ss_hl] = "HL",
	[z80ic_ss_sp] = "SP"
};

/** Z80 16-bit register names */
static const char *z80ic_r16_name[] = {
	[z80ic_r16_af] = "AF",
	[z80ic_r16_bc] = "BC",
	[z80ic_r16_de] = "DE",
	[z80ic_r16_hl] = "HL",
	[z80ic_r16_ix] = "IX",
	[z80ic_r16_iy] = "IY",
	[z80ic_r16_sp] = "SP"
};

/** Z80 condition names */
static const char *z80ic_cc_name[] = {
	[z80ic_cc_nz] = "NZ",
	[z80ic_cc_z] = "Z",
	[z80ic_cc_nc] = "NC",
	[z80ic_cc_c] = "C",
	[z80ic_cc_po] = "PO",
	[z80ic_cc_pe] = "PE",
	[z80ic_cc_p] = "P",
	[z80ic_cc_m] = "M"
};

/** Create Z80 IC module.
 *
 * @param rmodule Place to store pointer to new module.
 * @return EOK on success, ENOMEM if out of memory.
 */
int z80ic_module_create(z80ic_module_t **rmodule)
{
	z80ic_module_t *module;

	module = calloc(1, sizeof(z80ic_module_t));
	if (module == NULL)
		return ENOMEM;

	list_initialize(&module->declns);
	*rmodule = module;
	return EOK;
}

/** Append declaration to Z80 IC module.
 *
 * @param module Z80 IC module
 * @param decln Declaration
 */
void z80ic_module_append(z80ic_module_t *module, z80ic_decln_t *decln)
{
	assert(decln->module == NULL);
	decln->module = module;
	list_append(&decln->ldeclns, &module->declns);
}

/** Get first declaration in Z80 IC module.
 *
 * @param module Z80 IC module
 * @return First declaration or @c NULL if there is none
 */
z80ic_decln_t *z80ic_module_first(z80ic_module_t *module)
{
	link_t *link;

	link = list_first(&module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_decln_t, ldeclns);
}

/** Get next declaration in Z80 IC module.
 *
 * @param cur Current declaration
 * @return Next declaration or @c NULL if there is none
 */
z80ic_decln_t *z80ic_module_next(z80ic_decln_t *cur)
{
	link_t *link;

	link = list_next(&cur->ldeclns, &cur->module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_decln_t, ldeclns);
}

/** Get last declaration in Z80 IC module.
 *
 * @param module Z80 IC module
 * @return Last declaration or @c NULL if there is none
 */
z80ic_decln_t *z80ic_module_last(z80ic_module_t *module)
{
	link_t *link;

	link = list_last(&module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_decln_t, ldeclns);
}

/** Get previous declaration in Z80 IC module.
 *
 * @param cur Current declaration
 * @return Previous declaration or @c NULL if there is none
 */
z80ic_decln_t *z80ic_module_prev(z80ic_decln_t *cur)
{
	link_t *link;

	link = list_prev(&cur->ldeclns, &cur->module->declns);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_decln_t, ldeclns);
}

/** Print Z80 IC module.
 *
 * @param module Z80 IC module
 * @param f Output file
 */
int z80ic_module_print(z80ic_module_t *module, FILE *f)
{
	z80ic_decln_t *decln;
	int rc;

	decln = z80ic_module_first(module);
	while (decln != NULL) {
		rc = z80ic_decln_print(decln, f);
		if (rc != EOK)
			return rc;

		decln = z80ic_module_next(decln);
	}

	return EOK;
}

/** Destroy Z80 IC module.
 *
 * @param module Z80 IC module or @c NULL
 */
void z80ic_module_destroy(z80ic_module_t *module)
{
	z80ic_decln_t *decln;

	if (module == NULL)
		return;

	decln = z80ic_module_first(module);
	while (decln != NULL) {
		list_remove(&decln->ldeclns);
		z80ic_decln_destroy(decln);

		decln = z80ic_module_first(module);
	}

	free(module);
}

/** Destroy Z80 IC declaration.
 *
 * @param decln Z80 IC declaration or @c NULL
 */
static void z80ic_decln_destroy(z80ic_decln_t *decln)
{
	if (decln == NULL)
		return;

	switch (decln->dtype) {
	case z80icd_extern:
		z80ic_extern_destroy((z80ic_extern_t *) decln->ext);
		break;
	case z80icd_var:
		z80ic_var_destroy((z80ic_var_t *) decln->ext);
		break;
	case z80icd_proc:
		z80ic_proc_destroy((z80ic_proc_t *) decln->ext);
		break;
	}
}

/** Print Z80 IC declaration.
 *
 * @param decln Z80 IC declaration
 * @param f Output file
 *
 * @return EOK on success or an error code
 */
int z80ic_decln_print(z80ic_decln_t *decln, FILE *f)
{
	switch (decln->dtype) {
	case z80icd_extern:
		return z80ic_extern_print((z80ic_extern_t *) decln->ext, f);
	case z80icd_var:
		return z80ic_var_print((z80ic_var_t *) decln->ext, f);
	case z80icd_proc:
		return z80ic_proc_print((z80ic_proc_t *) decln->ext, f);
	}

	/* Should not be reached */
	assert(false);
	return EINVAL;
}

/** Create Z80 IC extern declaration.
 *
 * @param ident Identifier (will be copied)
 * @param rextern Place to store pointer to new extern declaration
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_extern_create(const char *ident, z80ic_extern_t **rextern)
{
	z80ic_extern_t *dextern;

	dextern = calloc(1, sizeof(z80ic_extern_t));
	if (dextern == NULL)
		return ENOMEM;

	dextern->ident = strdup(ident);
	if (dextern->ident == NULL) {
		free(dextern);
		return ENOMEM;
	}

	dextern->decln.dtype = z80icd_extern;
	dextern->decln.ext = (void *) dextern;
	*rextern = dextern;
	return EOK;
}

/** Print Z80 IC extern declaration.
 *
 * @param dextern Z80 IC extern declaration
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_extern_print(z80ic_extern_t *dextern, FILE *f)
{
	int rv;

	rv = fprintf(f, "\nEXTERN %s\n", dextern->ident);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC extern declaration.
 *
 * @param dextern Z80 IC extern declaration or @c NULL
 */
void z80ic_extern_destroy(z80ic_extern_t *dextern)
{
	if (dextern == NULL)
		return;

	if (dextern->ident != NULL)
		free(dextern->ident);

	free(dextern);
}

/** Create Z80 IC variable.
 *
 * @param ident Identifier (will be copied)
 * @param dblock Data block
 * @param rvar Place to store pointer to new variable
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_var_create(const char *ident, z80ic_dblock_t *dblock, z80ic_var_t **rvar)
{
	z80ic_var_t *var;

	var = calloc(1, sizeof(z80ic_var_t));
	if (var == NULL)
		return ENOMEM;

	var->ident = strdup(ident);
	if (var->ident == NULL) {
		free(var);
		return ENOMEM;
	}

	assert(dblock != NULL);
	var->dblock = dblock;
	var->decln.dtype = z80icd_var;
	var->decln.ext = (void *) var;
	*rvar = var;
	return EOK;
}

/** Print Z80 IC variable.
 *
 * @param var Z80 IC variable
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_var_print(z80ic_var_t *var, FILE *f)
{
	int rv;
	int rc;

	rv = fprintf(f, "\n; var %s\n", var->ident);
	if (rv < 0)
		return EIO;

	rv = fprintf(f, ".%s\n", var->ident);
	if (rv < 0)
		return EIO;

	rc = z80ic_dblock_print(var->dblock, f);
	if (rc != EOK)
		return EIO;

	rv = fprintf(f, "; end var %s\n", var->ident);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC variable.
 *
 * @param var Z80 IC variable or @c NULL
 */
void z80ic_var_destroy(z80ic_var_t *var)
{
	if (var == NULL)
		return;

	if (var->ident != NULL)
		free(var->ident);

	z80ic_dblock_destroy(var->dblock);
	free(var);
}

/** Create Z80 IC data block.
 *
 * @param rdblock Place to store pointer to new data block.
 * @return EOK on success, ENOMEM if out of memory.
 */
int z80ic_dblock_create(z80ic_dblock_t **rdblock)
{
	z80ic_dblock_t *dblock;

	dblock = calloc(1, sizeof(z80ic_dblock_t));
	if (dblock == NULL)
		return ENOMEM;

	list_initialize(&dblock->entries);
	*rdblock = dblock;
	return EOK;
}

/** Append data entry to Z80 IC data block.
 *
 * @param dblock Z80 IC data block
 * @param dentry Data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_dblock_append(z80ic_dblock_t *dblock, z80ic_dentry_t *dentry)
{
	z80ic_dblock_entry_t *entry;

	entry = calloc(1, sizeof(z80ic_dblock_entry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->dblock = dblock;
	list_append(&entry->lentries, &dblock->entries);

	entry->dentry = dentry;
	return EOK;
}

/** Print Z80 IC DEFB data entry.
 *
 * @param dentry Z80 IC DEFB data entry
 * @param f Output file
 * @return EOK on success or an error code
 */
static int z80ic_dentry_defb_print(z80ic_dentry_t *dentry, FILE *f)
{
	int rv;

	assert(dentry->dtype == z80icd_defb);

	rv = fprintf(f, "defb $%" PRIx16, (unsigned)dentry->value);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print Z80 IC DEFW data entry.
 *
 * @param dentry Z80 IC DEFW data entry
 * @param f Output file
 * @return EOK on success or an error code
 */
static int z80ic_dentry_defw_print(z80ic_dentry_t *dentry, FILE *f)
{
	int rv;

	assert(dentry->dtype == z80icd_defw);

	rv = fprintf(f, "defw $%" PRIx16, (unsigned)dentry->value);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print Z80 IC DEFDW data entry.
 *
 * @param dentry Z80 IC DEFDW data entry
 * @param f Output file
 * @return EOK on success or an error code
 */
static int z80ic_dentry_defdw_print(z80ic_dentry_t *dentry, FILE *f)
{
	int rv;

	assert(dentry->dtype == z80icd_defdw);

	/* Need to print as two words for Z80asm */

	rv = fprintf(f, "; defdw $%" PRIx32 "\n", (uint32_t)dentry->value);
	if (rv < 0)
		return EIO;

	rv = fprintf(f, "\tdefw $%" PRIx16 "\n", (unsigned)(dentry->value & 0xffff));
	if (rv < 0)
		return EIO;

	rv = fprintf(f, "\tdefw $%" PRIx16, (unsigned)(dentry->value >> 16));
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print Z80 IC DEFQW data entry.
 *
 * @param dentry Z80 IC DEFQW data entry
 * @param f Output file
 * @return EOK on success or an error code
 */
static int z80ic_dentry_defqw_print(z80ic_dentry_t *dentry, FILE *f)
{
	int rv;

	assert(dentry->dtype == z80icd_defqw);

	/* Need to print as four words for Z80asm */

	rv = fprintf(f, "; defqw $%" PRIx64 "\n", dentry->value);
	if (rv < 0)
		return EIO;

	rv = fprintf(f, "\tdefw $%" PRIx16 "\n",
	    (unsigned)(dentry->value & 0xffff));
	if (rv < 0)
		return EIO;

	rv = fprintf(f, "\tdefw $%" PRIx16 "\n",
	    (unsigned)((dentry->value >> 16) & 0xffff));
	if (rv < 0)
		return EIO;

	rv = fprintf(f, "\tdefw $%" PRIx16 "\n",
	    (unsigned)((dentry->value >> 32) & 0xffff));
	if (rv < 0)
		return EIO;

	rv = fprintf(f, "\tdefw $%" PRIx16,
	    (unsigned)((dentry->value >> 48) & 0xffff));
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Print Z80 IC data block.
 *
 * @param dblock Labeled block
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_dblock_print(z80ic_dblock_t *dblock, FILE *f)
{
	z80ic_dblock_entry_t *entry;
	int rc;

	entry = z80ic_dblock_first(dblock);
	while (entry != NULL) {
		rc = z80ic_dentry_print(entry->dentry, f);
		if (rc != EOK)
			return rc;

		entry = z80ic_dblock_next(entry);
	}

	return EOK;
}

/** Destroy Z80 IC data block.
 *
 * @param dblock Data block or @c NULL
 */
void z80ic_dblock_destroy(z80ic_dblock_t *dblock)
{
	z80ic_dblock_entry_t *entry;

	if (dblock == NULL)
		return;

	entry = z80ic_dblock_first(dblock);
	while (entry != NULL) {
		list_remove(&entry->lentries);
		z80ic_dentry_destroy(entry->dentry);
		free(entry);

		entry = z80ic_dblock_first(dblock);
	}

	free(dblock);
}

/** Create Z80 IC DEFB data entry.
 *
 * @param value Value
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_dentry_create_defb(uint8_t value, z80ic_dentry_t **rdentry)
{
	z80ic_dentry_t *dentry;

	dentry = calloc(1, sizeof(z80ic_dentry_t));
	if (dentry == NULL)
		return ENOMEM;

	dentry->dtype = z80icd_defb;
	dentry->value = value;

	*rdentry = dentry;
	return EOK;
}

/** Create Z80 IC DEFW data entry.
 *
 * @param value Value
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_dentry_create_defw(uint16_t value, z80ic_dentry_t **rdentry)
{
	z80ic_dentry_t *dentry;

	dentry = calloc(1, sizeof(z80ic_dentry_t));
	if (dentry == NULL)
		return ENOMEM;

	dentry->dtype = z80icd_defw;
	dentry->value = value;

	*rdentry = dentry;
	return EOK;
}

/** Create Z80 IC DEFDW data entry.
 *
 * @param value Value
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_dentry_create_defdw(uint32_t value, z80ic_dentry_t **rdentry)
{
	z80ic_dentry_t *dentry;

	dentry = calloc(1, sizeof(z80ic_dentry_t));
	if (dentry == NULL)
		return ENOMEM;

	dentry->dtype = z80icd_defdw;
	dentry->value = value;

	*rdentry = dentry;
	return EOK;
}

/** Create Z80 IC DEFQW data entry.
 *
 * @param value Value
 * @param rdentry Place to store pointer to new data entry
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_dentry_create_defqw(uint64_t value, z80ic_dentry_t **rdentry)
{
	z80ic_dentry_t *dentry;

	dentry = calloc(1, sizeof(z80ic_dentry_t));
	if (dentry == NULL)
		return ENOMEM;

	dentry->dtype = z80icd_defqw;
	dentry->value = value;

	*rdentry = dentry;
	return EOK;
}

/** Print Z80 IC data entry.
 *
 * @param dblock Labeled block
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_dentry_print(z80ic_dentry_t *dentry, FILE *f)
{
	int rc;
	int rv;

	rv = fputc('\t', f);
	if (rv < 0)
		return EIO;

	switch (dentry->dtype) {
	case z80icd_defb:
		rc = z80ic_dentry_defb_print(dentry, f);
		break;
	case z80icd_defw:
		rc = z80ic_dentry_defw_print(dentry, f);
		break;
	case z80icd_defdw:
		rc = z80ic_dentry_defdw_print(dentry, f);
		break;
	case z80icd_defqw:
		rc = z80ic_dentry_defqw_print(dentry, f);
		break;
	default:
		assert(false);
		rc = ENOTSUP;
		break;
	}

	if (rc != EOK)
		return rc;

	rv = fputc('\n', f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC data entry.
 *
 * @param dentry Data entry or @c NULL
 */
void z80ic_dentry_destroy(z80ic_dentry_t *dentry)
{
	if (dentry == NULL)
		return;

	free(dentry);
}

/** Get first entry in Z80 IC data block.
 *
 * @param dblock Z80 IC data block
 * @return First entry or @c NULL if there is none
 */
z80ic_dblock_entry_t *z80ic_dblock_first(z80ic_dblock_t *dblock)
{
	link_t *link;

	link = list_first(&dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_dblock_entry_t, lentries);
}

/** Get next entry in Z80 IC data block.
 *
 * @param cur Current entry
 * @return Next entry or @c NULL if there is none
 */
z80ic_dblock_entry_t *z80ic_dblock_next(z80ic_dblock_entry_t *cur)
{
	link_t *link;

	link = list_next(&cur->lentries, &cur->dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_dblock_entry_t, lentries);
}

/** Get last entry in Z80 IC data block.
 *
 * @param dblock Z80 IC data block
 * @return Last entry or @c NULL if there is none
 */
z80ic_dblock_entry_t *z80ic_dblock_last(z80ic_dblock_t *dblock)
{
	link_t *link;

	link = list_last(&dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_dblock_entry_t, lentries);
}

/** Get previous entry in Z80 IC data block.
 *
 * @param cur Current entry
 * @return Previous entry or @c NULL if there is none
 */
z80ic_dblock_entry_t *z80ic_dblock_prev(z80ic_dblock_entry_t *cur)
{
	link_t *link;

	link = list_prev(&cur->lentries, &cur->dblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_dblock_entry_t, lentries);
}

/** Create Z80 IC procedure.
 *
 * @param ident Identifier (will be copied)
 * @param lblock Labeled block
 * @param rproc Place to store pointer to new procedure
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_proc_create(const char *ident, z80ic_lblock_t *lblock, z80ic_proc_t **rproc)
{
	z80ic_proc_t *proc;

	proc = calloc(1, sizeof(z80ic_proc_t));
	if (proc == NULL)
		return ENOMEM;

	proc->ident = strdup(ident);
	if (proc->ident == NULL) {
		free(proc);
		return ENOMEM;
	}

	assert(lblock != NULL);
	list_initialize(&proc->lvars);
	proc->lblock = lblock;
	proc->decln.dtype = z80icd_proc;
	proc->decln.ext = (void *) proc;
	*rproc = proc;
	return EOK;
}

/** Print Z80 IC procedure.
 *
 * @param proc Z80 IC procedure
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_proc_print(z80ic_proc_t *proc, FILE *f)
{
	z80ic_lvar_t *lvar;
	int rv;
	int rc;

	rv = fprintf(f, "\n; proc %s\n.%s\n", proc->ident, proc->ident);
	if (rv < 0)
		return EIO;

	/* Print local variables */
	lvar = z80ic_proc_first_lvar(proc);
	if (lvar != NULL) {
		rv = fputs("; lvar\n", f);
		if (rv < 0)
			return EIO;

		while (lvar != NULL) {
			rc = z80ic_lvar_print(lvar, f);
			if (rc != EOK)
				return rc;

			rv = fputs("\n", f);
			if (rv < 0)
				return EIO;

			lvar = z80ic_proc_next_lvar(lvar);
		}
	}

	rv = fputs("; begin\n", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_lblock_print(proc->lblock, f);
	if (rc != EOK)
		return EIO;

	rv = fprintf(f, "\n; end proc %s\n\n", proc->ident);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Append local variable to Z80 IC procedure.
 *
 * @param proc Z80 IC procedure
 * @param lvar Local variable
 */
void z80ic_proc_append_lvar(z80ic_proc_t *proc, z80ic_lvar_t *lvar)
{
	assert(lvar->proc == NULL);
	lvar->proc = proc;
	list_append(&lvar->llvars, &proc->lvars);
}

/** Destroy Z80 IC procedure.
 *
 * @param proc Z80 IC procedure or @c NULL
 */
void z80ic_proc_destroy(z80ic_proc_t *proc)
{
	z80ic_lvar_t *lvar;

	if (proc == NULL)
		return;

	if (proc->ident != NULL)
		free(proc->ident);

	lvar = z80ic_proc_first_lvar(proc);
	while (lvar != NULL) {
		list_remove(&lvar->llvars);
		z80ic_lvar_destroy(lvar);
		lvar = z80ic_proc_first_lvar(proc);
	}

	z80ic_lblock_destroy(proc->lblock);
	free(proc);
}

/** Get first local variable of Z80 IC procedure.
 *
 * @param proc Z80 IC procedure
 * @return First local variable or @c NULL if there is none
 */
z80ic_lvar_t *z80ic_proc_first_lvar(z80ic_proc_t *proc)
{
	link_t *link;

	link = list_first(&proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lvar_t, llvars);
}

/** Get next local variable of Z80 IC procedure.
 *
 * @param cur Current local variable
 * @return Next local variable or @c NULL if @a cur is the last local variable
 */
z80ic_lvar_t *z80ic_proc_next_lvar(z80ic_lvar_t *cur)
{
	link_t *link;

	link = list_next(&cur->llvars, &cur->proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lvar_t, llvars);
}

/** Get last local variable of Z80 IC procedure.
 *
 * @param proc Z80 IC procedure
 * @return Last local variable or @c NULL if there is none
 */
z80ic_lvar_t *z80ic_proc_last_lvar(z80ic_proc_t *proc)
{
	link_t *link;

	link = list_last(&proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lvar_t, llvars);
}

/** Get previous local variable of Z80 IC procedure.
 *
 * @param cur Current local variable
 * @return Previous local variable or @c NULL if @a cur is the first local
 *         variable
 */
z80ic_lvar_t *z80ic_proc_prev_lvar(z80ic_lvar_t *cur)
{
	link_t *link;

	link = list_prev(&cur->llvars, &cur->proc->lvars);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lvar_t, llvars);
}

/** Create Z80 IC procedure local variable.
 *
 * @param ident Local variable identifier
 * @param off Local variable offset within variable storage area
 * @param rlvar Place to store pointer to new local variable
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_lvar_create(const char *ident, uint16_t off, z80ic_lvar_t **rlvar)
{
	z80ic_lvar_t *lvar;

	lvar = calloc(1, sizeof(z80ic_lvar_t));
	if (lvar == NULL)
		return ENOMEM;

	lvar->ident = strdup(ident);
	if (lvar->ident == NULL) {
		free(lvar);
		return ENOMEM;
	}

	lvar->off = off;
	*rlvar = lvar;
	return EOK;
}

/** Destroy Z80 IC procedure local variable.
 *
 * @param lvar Local variable
 */
void z80ic_lvar_destroy(z80ic_lvar_t *lvar)
{
	free(lvar->ident);
	free(lvar);
}

/** Print Z80 IC procedure local variable.
 *
 * @param lvar Local variable
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_lvar_print(z80ic_lvar_t *lvar, FILE *f)
{
	int rv;

	rv = fprintf(f, ".%s equ 0x%x", lvar->ident, lvar->off);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Create Z80 IC labeled block.
 *
 * @param rlblock Place to store pointer to new labeled block.
 * @return EOK on success, ENOMEM if out of memory.
 */
int z80ic_lblock_create(z80ic_lblock_t **rlblock)
{
	z80ic_lblock_t *lblock;

	lblock = calloc(1, sizeof(z80ic_lblock_t));
	if (lblock == NULL)
		return ENOMEM;

	list_initialize(&lblock->entries);
	*rlblock = lblock;
	return EOK;
}

/** Append entry to Z80 IC labeled block.
 *
 * @param lblock Z80 IC labeled block
 * @param label Label or @c NULL if none
 * @param instr Instruction
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_lblock_append(z80ic_lblock_t *lblock, const char *label,
    z80ic_instr_t *instr)
{
	char *dlabel;
	z80ic_lblock_entry_t *entry;

	if (label != NULL) {
		dlabel = strdup(label);
		if (dlabel == NULL)
			return ENOMEM;
	} else {
		dlabel = NULL;
	}

	entry = calloc(1, sizeof(z80ic_lblock_entry_t));
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

/** Print Z80 IC block.
 *
 * @param lblock Labeled block
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_lblock_print(z80ic_lblock_t *lblock, FILE *f)
{
	z80ic_lblock_entry_t *entry;
	int rc;
	int rv;

	entry = z80ic_lblock_first(lblock);
	while (entry != NULL) {
		if (entry->label != NULL) {
			rv = fprintf(f, "%s:\n", entry->label);
			if (rv < 0)
				return EIO;
		}

		if (entry->instr != NULL) {
			rc = z80ic_instr_print(entry->instr, f);
			if (rc != EOK)
				return rc;
		}

		entry = z80ic_lblock_next(entry);
	}

	return EOK;
}

/** Destroy Z80 IC labeled block.
 *
 * @param lblock Labeled block or @c NULL
 */
void z80ic_lblock_destroy(z80ic_lblock_t *lblock)
{
	z80ic_lblock_entry_t *entry;

	if (lblock == NULL)
		return;

	entry = z80ic_lblock_first(lblock);
	while (entry != NULL) {
		list_remove(&entry->lentries);
		if (entry->label != NULL)
			free(entry->label);
		z80ic_instr_destroy(entry->instr);
		free(entry);

		entry = z80ic_lblock_first(lblock);
	}

	free(lblock);
}

/** Get first entry in Z80 IC labeled block.
 *
 * @param lblock Z80 IC labeled block
 * @return First entry or @c NULL if there is none
 */
z80ic_lblock_entry_t *z80ic_lblock_first(z80ic_lblock_t *lblock)
{
	link_t *link;

	link = list_first(&lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lblock_entry_t, lentries);
}

/** Get next entry in Z80 IC labeled block.
 *
 * @param cur Current entry
 * @return Next entry or @c NULL if there is none
 */
z80ic_lblock_entry_t *z80ic_lblock_next(z80ic_lblock_entry_t *cur)
{
	link_t *link;

	link = list_next(&cur->lentries, &cur->lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lblock_entry_t, lentries);
}

/** Get last entry in Z80 IC labeled block.
 *
 * @param lblock Z80 IC labeled block
 * @return Last entry or @c NULL if there is none
 */
z80ic_lblock_entry_t *z80ic_lblock_last(z80ic_lblock_t *lblock)
{
	link_t *link;

	link = list_last(&lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lblock_entry_t, lentries);
}

/** Get previous entry in Z80 IC labeled block.
 *
 * @param cur Current entry
 * @return Previous entry or @c NULL if there is none
 */
z80ic_lblock_entry_t *z80ic_lblock_prev(z80ic_lblock_entry_t *cur)
{
	link_t *link;

	link = list_prev(&cur->lentries, &cur->lblock->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80ic_lblock_entry_t, lentries);
}

/** Create Z80 IC load 8-bit register from 8-bit immediate instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_r_n_create(z80ic_ld_r_n_t **rinstr)
{
	z80ic_ld_r_n_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_r_n_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_r_n;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load 16-bit dd register from 16-bit immediate instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_r_n_print(z80ic_ld_r_n_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm8_print(instr->imm8, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load 16-bit dd register from 16-bit immediate instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_r_n_destroy(z80ic_ld_r_n_t *instr)
{
	z80ic_oper_reg_destroy(instr->dest);
	z80ic_oper_imm8_destroy(instr->imm8);
}

/** Create Z80 IC load register from (HL) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_r_ihl_create(z80ic_ld_r_ihl_t **rinstr)
{
	z80ic_ld_r_ihl_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_r_ihl_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_r_ihl;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load register from (HL) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_r_ihl_print(z80ic_ld_r_ihl_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", (HL)", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC load register from (HL) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_r_ihl_destroy(z80ic_ld_r_ihl_t *instr)
{
	z80ic_oper_reg_destroy(instr->dest);
}

/** Create Z80 IC load register from (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_r_iixd_create(z80ic_ld_r_iixd_t **rinstr)
{
	z80ic_ld_r_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_r_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_r_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load register from (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_r_iixd_print(z80ic_ld_r_iixd_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fprintf(f, ", (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC load register from (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_r_iixd_destroy(z80ic_ld_r_iixd_t *instr)
{
	z80ic_oper_reg_destroy(instr->dest);
}

/** Create Z80 IC load (HL) from register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_ihl_r_create(z80ic_ld_ihl_r_t **rinstr)
{
	z80ic_ld_ihl_r_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_ihl_r_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_ihl_r;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load (HL) from register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_ihl_r_print(z80ic_ld_ihl_r_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rv = fputs("(HL), ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load (HL) from register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_ihl_r_destroy(z80ic_ld_ihl_r_t *instr)
{
	z80ic_oper_reg_destroy(instr->src);
}

/** Create Z80 IC load (IX+d) from register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_iixd_r_create(z80ic_ld_iixd_r_t **rinstr)
{
	z80ic_ld_iixd_r_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_iixd_r_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_iixd_r;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load (IX+d) from register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_iixd_r_print(z80ic_ld_iixd_r_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fprintf(f, "ld (IX%+" PRId8 "), ", instr->disp);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load (IX+d) from register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_iixd_r_destroy(z80ic_ld_iixd_r_t *instr)
{
	z80ic_oper_reg_destroy(instr->src);
}

/** Create Z80 IC load (IX+d) from 8-bit immediate instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_iixd_n_create(z80ic_ld_iixd_n_t **rinstr)
{
	z80ic_ld_iixd_n_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_iixd_n_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_iixd_n;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load (IX+d) from 8-bit immediate instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_iixd_n_print(z80ic_ld_iixd_n_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fprintf(f, "ld (IX%+" PRId8 "), ", instr->disp);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm8_print(instr->imm8, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load (IX+d) from 8-bit immediate instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_iixd_n_destroy(z80ic_ld_iixd_n_t *instr)
{
	z80ic_oper_imm8_destroy(instr->imm8);
}

/** Create Z80 IC load 16-bit dd register from 16-bit immediate instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_dd_nn_create(z80ic_ld_dd_nn_t **rinstr)
{
	z80ic_ld_dd_nn_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_dd_nn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_dd_nn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load 16-bit dd register from 16-bit immediate instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_dd_nn_print(z80ic_ld_dd_nn_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_dd_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(instr->imm16, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load 16-bit dd register from 16-bit immediate instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_dd_nn_destroy(z80ic_ld_dd_nn_t *instr)
{
	z80ic_oper_dd_destroy(instr->dest);
	z80ic_oper_imm16_destroy(instr->imm16);
}

/** Create Z80 IC load IX from 16-bit immediate instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_ix_nn_create(z80ic_ld_ix_nn_t **rinstr)
{
	z80ic_ld_ix_nn_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_ix_nn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_ix_nn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load IX from 16-bit immediate instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_ix_nn_print(z80ic_ld_ix_nn_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld IX, ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(instr->imm16, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load IX from 16-bit immediate instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_ix_nn_destroy(z80ic_ld_ix_nn_t *instr)
{
	z80ic_oper_imm16_destroy(instr->imm16);
}

/** Create Z80 IC load SP from IX instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_sp_ix_create(z80ic_ld_sp_ix_t **rinstr)
{
	z80ic_ld_sp_ix_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_sp_ix_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_sp_ix;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register pair IC from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_sp_ix_print(z80ic_ld_sp_ix_t *instr, FILE *f)
{
	int rv;

	(void) instr;

	rv = fputs("ld SP, IX", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC load virtual register pair IC from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_sp_ix_destroy(z80ic_ld_sp_ix_t *instr)
{
	/* Intentionally empty */
	(void) instr;
}

/** Create Z80 IC push IX instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_push_ix_create(z80ic_push_ix_t **rinstr)
{
	z80ic_push_ix_t *instr;

	instr = calloc(1, sizeof(z80ic_push_ix_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_push_ix;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC push IX instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_push_ix_print(z80ic_push_ix_t *instr, FILE *f)
{
	int rv;

	(void) instr;

	rv = fputs("push IX", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC push IX instruction.
 *
 * @param instr Instruction
 */
static void z80ic_push_ix_destroy(z80ic_push_ix_t *instr)
{
	/* Intentionally empty */
	(void) instr;
}

/** Create Z80 IC pop IX instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_pop_ix_create(z80ic_pop_ix_t **rinstr)
{
	z80ic_pop_ix_t *instr;

	instr = calloc(1, sizeof(z80ic_pop_ix_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_pop_ix;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC pop IX instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_pop_ix_print(z80ic_pop_ix_t *instr, FILE *f)
{
	int rv;

	(void) instr;

	rv = fputs("pop IX", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC pop IX instruction.
 *
 * @param instr Instruction
 */
static void z80ic_pop_ix_destroy(z80ic_pop_ix_t *instr)
{
	/* Intentionally empty */
	(void) instr;
}

/** Create Z80 IC add (IX+d) to A instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_add_a_iixd_create(z80ic_add_a_iixd_t **rinstr)
{
	z80ic_add_a_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_add_a_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_add_a_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC add (IX+d) to A instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_add_a_iixd_print(z80ic_add_a_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "add A, (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC add (IX+d) to A instruction.
 *
 * @param instr Instruction
 */
static void z80ic_add_a_iixd_destroy(z80ic_add_a_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC add (IX+d) to A instruction with carry.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_adc_a_iixd_create(z80ic_adc_a_iixd_t **rinstr)
{
	z80ic_adc_a_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_adc_a_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_adc_a_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC add (IX+d) to A instruction with carry.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_adc_a_iixd_print(z80ic_adc_a_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "adc A, (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC add (IX+d) to A instruction with carry.
 *
 * @param instr Instruction
 */
static void z80ic_adc_a_iixd_destroy(z80ic_adc_a_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC subtract 8-bit immediate instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sub_n_create(z80ic_sub_n_t **rinstr)
{
	z80ic_sub_n_t *instr;

	instr = calloc(1, sizeof(z80ic_sub_n_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sub_n;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC subtract 8-bit immediate instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sub_n_print(z80ic_sub_n_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("sub ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm8_print(instr->imm8, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC subtract 8-bit immediate instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sub_n_destroy(z80ic_sub_n_t *instr)
{
	z80ic_oper_imm8_destroy(instr->imm8);
}

/** Create Z80 IC subtract (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sub_iixd_create(z80ic_sub_iixd_t **rinstr)
{
	z80ic_sub_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_sub_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sub_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC subtract (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sub_iixd_print(z80ic_sub_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "sub (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC subtract (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sub_iixd_destroy(z80ic_sub_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC subtract (IX+d) from A with carry instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sbc_a_iixd_create(z80ic_sbc_a_iixd_t **rinstr)
{
	z80ic_sbc_a_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_sbc_a_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sbc_a_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC subtract (IX+d) from A with carry instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sbc_a_iixd_print(z80ic_sbc_a_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "sbc A, (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC subtrcat (IX+d) from A with carry instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sbc_a_iixd_destroy(z80ic_sbc_a_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC bitwise AND with register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_and_r_create(z80ic_and_r_t **rinstr)
{
	z80ic_and_r_t *instr;

	instr = calloc(1, sizeof(z80ic_and_r_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_and_r;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC bitwise AND with register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_and_r_print(z80ic_and_r_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("and ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC bitwise AND with register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_and_r_destroy(z80ic_and_r_t *instr)
{
	z80ic_oper_reg_destroy(instr->src);
}

/** Create Z80 IC bitwise AND with (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_and_iixd_create(z80ic_and_iixd_t **rinstr)
{
	z80ic_and_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_and_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_and_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC bitwise AND with (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_and_iixd_print(z80ic_and_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "and (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC bitwise AND with (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_and_iixd_destroy(z80ic_and_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC bitwise OR with (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_or_iixd_create(z80ic_or_iixd_t **rinstr)
{
	z80ic_or_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_or_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_or_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC bitwise OR with (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_or_iixd_print(z80ic_or_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "or (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC bitwise OR with (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_or_iixd_destroy(z80ic_or_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC bitwise XOR with (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_xor_iixd_create(z80ic_xor_iixd_t **rinstr)
{
	z80ic_xor_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_xor_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_xor_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC bitwise XOR with (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_xor_iixd_print(z80ic_xor_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "xor (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC bitwise XOR with (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_xor_iixd_destroy(z80ic_xor_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC increment (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_inc_iixd_create(z80ic_inc_iixd_t **rinstr)
{
	z80ic_inc_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_inc_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_inc_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC increment (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_inc_iixd_print(z80ic_inc_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "inc (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC increment (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_inc_iixd_destroy(z80ic_inc_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC decrement (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_dec_iixd_create(z80ic_dec_iixd_t **rinstr)
{
	z80ic_dec_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_dec_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_dec_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC decrement (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_dec_iixd_print(z80ic_dec_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "dec (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC decrement (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_dec_iixd_destroy(z80ic_dec_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC complement instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_cpl_create(z80ic_cpl_t **rinstr)
{
	z80ic_cpl_t *instr;

	instr = calloc(1, sizeof(z80ic_cpl_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_cpl;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC complement instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_cpl_print(z80ic_cpl_t *instr, FILE *f)
{
	int rv;

	(void) instr;

	rv = fputs("cpl", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC complement instruction.
 *
 * @param instr Instruction
 */
static void z80ic_cpl_destroy(z80ic_cpl_t *instr)
{
	(void) instr;
}

/** Create Z80 IC no operation instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_nop_create(z80ic_nop_t **rinstr)
{
	z80ic_nop_t *instr;

	instr = calloc(1, sizeof(z80ic_nop_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_nop;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC no operation instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_nop_print(z80ic_nop_t *instr, FILE *f)
{
	int rv;

	(void) instr;

	rv = fputs("nop", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC no operation instruction.
 *
 * @param instr Instruction
 */
static void z80ic_nop_destroy(z80ic_nop_t *instr)
{
	(void) instr;
}

/** Create Z80 IC add 16-bit ss register to HL instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_add_hl_ss_create(z80ic_add_hl_ss_t **rinstr)
{
	z80ic_add_hl_ss_t *instr;

	instr = calloc(1, sizeof(z80ic_add_hl_ss_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_add_hl_ss;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC add 16-bit ss register to HL instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_add_hl_ss_print(z80ic_add_hl_ss_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("add HL, ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_ss_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC add 16-bit ss register to HL instruction.
 *
 * @param instr Instruction
 */
static void z80ic_add_hl_ss_destroy(z80ic_add_hl_ss_t *instr)
{
	z80ic_oper_ss_destroy(instr->src);
}

/** Create Z80 IC subtract 16-bit ss register from HL with carry instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sbc_hl_ss_create(z80ic_sbc_hl_ss_t **rinstr)
{
	z80ic_sbc_hl_ss_t *instr;

	instr = calloc(1, sizeof(z80ic_sbc_hl_ss_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sbc_hl_ss;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC subtract 16-bit ss register from HL with carry instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sbc_hl_ss_print(z80ic_sbc_hl_ss_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("sbc HL, ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_ss_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC subtract 16-bit ss register from HL with carry instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sbc_hl_ss_destroy(z80ic_sbc_hl_ss_t *instr)
{
	z80ic_oper_ss_destroy(instr->src);
}

/** Create Z80 IC add 16-bit pp register to IX instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_add_ix_pp_create(z80ic_add_ix_pp_t **rinstr)
{
	z80ic_add_ix_pp_t *instr;

	instr = calloc(1, sizeof(z80ic_add_ix_pp_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_add_ix_pp;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register pair from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_add_ix_pp_print(z80ic_add_ix_pp_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("add IX, ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_pp_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load virtual register pair from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 */
static void z80ic_add_ix_pp_destroy(z80ic_add_ix_pp_t *instr)
{
	z80ic_oper_pp_destroy(instr->src);
}

/** Create Z80 IC increment 16-bit ss register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_inc_ss_create(z80ic_inc_ss_t **rinstr)
{
	z80ic_inc_ss_t *instr;

	instr = calloc(1, sizeof(z80ic_inc_ss_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_inc_ss;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC increment 16-bit ss register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_inc_ss_print(z80ic_inc_ss_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("inc ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_ss_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC increment virtual register pair.
 *
 * @param instr Instruction
 */
static void z80ic_inc_ss_destroy(z80ic_inc_ss_t *instr)
{
	z80ic_oper_ss_destroy(instr->dest);
}

/** Create Z80 IC rotate left accumulator instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_rla_create(z80ic_rla_t **rinstr)
{
	z80ic_rla_t *instr;

	instr = calloc(1, sizeof(z80ic_rla_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_rla;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC rotate left accumulator instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_rla_print(z80ic_rla_t *instr, FILE *f)
{
	int rv;

	(void) instr;

	rv = fputs("rla", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC rotate left accumulator instruction.
 *
 * @param instr Instruction
 */
static void z80ic_rla_destroy(z80ic_rla_t *instr)
{
	/* Intentionally empty */
	(void) instr;
}

/** Create Z80 IC rotate left (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_rl_iixd_create(z80ic_rl_iixd_t **rinstr)
{
	z80ic_rl_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_rl_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_rl_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC rotate left (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_rl_iixd_print(z80ic_rl_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "rl (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC rotate left (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_rl_iixd_destroy(z80ic_rl_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC rotate right (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_rr_iixd_create(z80ic_rr_iixd_t **rinstr)
{
	z80ic_rr_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_rr_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_rr_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC rotate right (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_rr_iixd_print(z80ic_rr_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "rr (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC rotate right (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_rr_iixd_destroy(z80ic_rr_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC shift left arithmetic (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sla_iixd_create(z80ic_sla_iixd_t **rinstr)
{
	z80ic_sla_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_sla_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sla_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC shift left arithmetic (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sla_iixd_print(z80ic_sla_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "sla (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC shift left arithmetic (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sla_iixd_destroy(z80ic_sla_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC shift right arithmetic (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sra_iixd_create(z80ic_sra_iixd_t **rinstr)
{
	z80ic_sra_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_sra_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sra_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC shift right arithmetic (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sra_iixd_print(z80ic_sra_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "sra (IX%+" PRId8 ")", instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC shift right arithmetic (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sra_iixd_destroy(z80ic_sra_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC test bit of (IX+d) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_bit_b_iixd_create(z80ic_bit_b_iixd_t **rinstr)
{
	z80ic_bit_b_iixd_t *instr;

	instr = calloc(1, sizeof(z80ic_bit_b_iixd_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_bit_b_iixd;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC test bit of (IX+d) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_bit_b_iixd_print(z80ic_bit_b_iixd_t *instr, FILE *f)
{
	int rv;

	rv = fprintf(f, "bit %u, (IX%+" PRId8 ")", instr->bit, instr->disp);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC shift right arithmetic (IX+d) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_bit_b_iixd_destroy(z80ic_bit_b_iixd_t *instr)
{
	(void) instr;
}

/** Create Z80 IC jump instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_jp_nn_create(z80ic_jp_nn_t **rinstr)
{
	z80ic_jp_nn_t *instr;

	instr = calloc(1, sizeof(z80ic_jp_nn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_jp_nn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC jump instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_jp_nn_print(z80ic_jp_nn_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("jp ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(instr->imm16, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC jump instruction.
 *
 * @param instr Instruction
 */
static void z80ic_jp_nn_destroy(z80ic_jp_nn_t *instr)
{
	z80ic_oper_imm16_destroy(instr->imm16);
}

/** Create Z80 IC conditional jump instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_jp_cc_nn_create(z80ic_jp_cc_nn_t **rinstr)
{
	z80ic_jp_cc_nn_t *instr;

	instr = calloc(1, sizeof(z80ic_jp_cc_nn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_jp_cc_nn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC conditional jump instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_jp_cc_nn_print(z80ic_jp_cc_nn_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fprintf(f, "jp %s, ", z80ic_cc_name[instr->cc]);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(instr->imm16, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC conditional jump instruction.
 *
 * @param instr Instruction
 */
static void z80ic_jp_cc_nn_destroy(z80ic_jp_cc_nn_t *instr)
{
	z80ic_oper_imm16_destroy(instr->imm16);
}

/** Create Z80 IC call instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_call_nn_create(z80ic_call_nn_t **rinstr)
{
	z80ic_call_nn_t *instr;

	instr = calloc(1, sizeof(z80ic_call_nn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_call_nn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC call instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_call_nn_print(z80ic_call_nn_t *instr, FILE *f)
{
	int rc;
	int rv;

	(void) instr;

	rv = fputs("call ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(instr->imm16, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC call instruction.
 *
 * @param instr Instruction
 */
static void z80ic_call_nn_destroy(z80ic_call_nn_t *instr)
{
	z80ic_oper_imm16_destroy(instr->imm16);
}

/** Create Z80 IC return instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ret_create(z80ic_ret_t **rinstr)
{
	z80ic_ret_t *instr;

	instr = calloc(1, sizeof(z80ic_ret_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ret;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC return instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ret_print(z80ic_ret_t *instr, FILE *f)
{
	int rv;

	(void) instr;

	rv = fputs("ret", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC return instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ret_destroy(z80ic_ret_t *instr)
{
	/* Intentionally empty */
	(void) instr;
}

/** Create Z80 IC load virtual register from virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vr_vr_create(z80ic_ld_vr_vr_t **rinstr)
{
	z80ic_ld_vr_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vr_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vr_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register from virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vr_vr_print(z80ic_ld_vr_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load virtual register from virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vr_vr_destroy(z80ic_ld_vr_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->dest);
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC load virtual register from 8-bit immediate instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vr_n_create(z80ic_ld_vr_n_t **rinstr)
{
	z80ic_ld_vr_n_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vr_n_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vr_n;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register pair from 16-bit immediate instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vr_n_print(z80ic_ld_vr_n_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm8_print(instr->imm8, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load virtual register pair from 16-bit immediate instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vr_n_destroy(z80ic_ld_vr_n_t *instr)
{
	z80ic_oper_vr_destroy(instr->dest);
	z80ic_oper_imm8_destroy(instr->imm8);
}

/** Create Z80 IC load virtual register from (HL) instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vr_ihl_create(z80ic_ld_vr_ihl_t **rinstr)
{
	z80ic_ld_vr_ihl_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vr_ihl_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vr_ihl;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register from (HL) instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vr_ihl_print(z80ic_ld_vr_ihl_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", (HL)", f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC load virtual register from (HL) instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vr_ihl_destroy(z80ic_ld_vr_ihl_t *instr)
{
	z80ic_oper_vr_destroy(instr->dest);
}

/** Create Z80 IC load (HL) from virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_ihl_vr_create(z80ic_ld_ihl_vr_t **rinstr)
{
	z80ic_ld_ihl_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_ihl_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_ihl_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load (HL) from virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_ihl_vr_print(z80ic_ld_ihl_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rv = fputs("(HL), ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load (HL) from virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_ihl_vr_destroy(z80ic_ld_ihl_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC load virtual register pair from virtual register pair
 * instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vrr_vrr_create(z80ic_ld_vrr_vrr_t **rinstr)
{
	z80ic_ld_vrr_vrr_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vrr_vrr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vrr_vrr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register pair from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vrr_vrr_print(z80ic_ld_vrr_vrr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load virtual register pair from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vrr_vrr_destroy(z80ic_ld_vrr_vrr_t *instr)
{
	z80ic_oper_vrr_destroy(instr->dest);
	z80ic_oper_vrr_destroy(instr->src);
}

/** Create Z80 IC load 8-bit register from virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_r_vr_create(z80ic_ld_r_vr_t **rinstr)
{
	z80ic_ld_r_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_r_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_r_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load 8-bit register from virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_r_vr_print(z80ic_ld_r_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load 8-bit register from virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_r_vr_destroy(z80ic_ld_r_vr_t *instr)
{
	z80ic_oper_reg_destroy(instr->dest);
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC load virtual register from 8-bit register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vr_r_create(z80ic_ld_vr_r_t **rinstr)
{
	z80ic_ld_vr_r_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vr_r_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vr_r;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register from 8-bit register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vr_r_print(z80ic_ld_vr_r_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load 8-bit register from virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vr_r_destroy(z80ic_ld_vr_r_t *instr)
{
	z80ic_oper_vr_destroy(instr->dest);
	z80ic_oper_reg_destroy(instr->src);
}

/** Create Z80 IC load 16-bit register from virtual register pair instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_r16_vrr_create(z80ic_ld_r16_vrr_t **rinstr)
{
	z80ic_ld_r16_vrr_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_r16_vrr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_r16_vrr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load 16-bit register from virtual register pair instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_r16_vrr_print(z80ic_ld_r16_vrr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_r16_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load 16-bit register from virtual register pair instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_r16_vrr_destroy(z80ic_ld_r16_vrr_t *instr)
{
	z80ic_oper_r16_destroy(instr->dest);
	z80ic_oper_vrr_destroy(instr->src);
}

/** Create Z80 IC load virtual register pair from 16-bit register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vrr_r16_create(z80ic_ld_vrr_r16_t **rinstr)
{
	z80ic_ld_vrr_r16_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vrr_r16_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vrr_r16;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register pair from 16-bit register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vrr_r16_print(z80ic_ld_vrr_r16_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_r16_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load virtual register pair from 16-bit register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vrr_r16_destroy(z80ic_ld_vrr_r16_t *instr)
{
	z80ic_oper_vrr_destroy(instr->dest);
	z80ic_oper_r16_destroy(instr->src);
}

/** Create Z80 IC load virtual register pair from 16-bit immediate instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vrr_nn_create(z80ic_ld_vrr_nn_t **rinstr)
{
	z80ic_ld_vrr_nn_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vrr_nn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vrr_nn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register pair from 16-bit immediate instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vrr_nn_print(z80ic_ld_vrr_nn_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(instr->imm16, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load virtual register pair from 16-bit immediate instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vrr_nn_destroy(z80ic_ld_vrr_nn_t *instr)
{
	z80ic_oper_vrr_destroy(instr->dest);
	z80ic_oper_imm16_destroy(instr->imm16);
}

/** Create Z80 IC load virtual register pair from SP with 16-bit displacement
 * instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_ld_vrr_spnn_create(z80ic_ld_vrr_spnn_t **rinstr)
{
	z80ic_ld_vrr_spnn_t *instr;

	instr = calloc(1, sizeof(z80ic_ld_vrr_spnn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vrr_spnn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load virtual register pair from SP with 16-bit displacement
 * instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_ld_vrr_spnn_print(z80ic_ld_vrr_spnn_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("ld ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", SP+", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(instr->imm16, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC load virtual register pair from SP with 16-bit displacement
 * instruction.
 *
 * @param instr Instruction
 */
static void z80ic_ld_vrr_spnn_destroy(z80ic_ld_vrr_spnn_t *instr)
{
	z80ic_oper_vrr_destroy(instr->dest);
	z80ic_oper_imm16_destroy(instr->imm16);
}

/** Create Z80 IC add virtual register to A instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_add_a_vr_create(z80ic_add_a_vr_t **rinstr)
{
	z80ic_add_a_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_add_a_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_add_a_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC add virtual register to A instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_add_a_vr_print(z80ic_add_a_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("add A, ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC add virtual register to A instruction.
 *
 * @param instr Instruction
 */
static void z80ic_add_a_vr_destroy(z80ic_add_a_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC add virtual register to A instruction with carry.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_adc_a_vr_create(z80ic_adc_a_vr_t **rinstr)
{
	z80ic_adc_a_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_adc_a_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_adc_a_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC add virtual register to A instruction with carry.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_adc_a_vr_print(z80ic_adc_a_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("adc A, ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC add virtual register to A instruction with carry.
 *
 * @param instr Instruction
 */
static void z80ic_adc_a_vr_destroy(z80ic_adc_a_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC subtract virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sub_vr_create(z80ic_sub_vr_t **rinstr)
{
	z80ic_sub_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_sub_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sub_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC subtract virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sub_vr_print(z80ic_sub_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("sub ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC subtract virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sub_vr_destroy(z80ic_sub_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC subtract virtual register from A with carry instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sbc_a_vr_create(z80ic_sbc_a_vr_t **rinstr)
{
	z80ic_sbc_a_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_sbc_a_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sbc_a_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC subtract virtual register from A with carry instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sbc_a_vr_print(z80ic_sbc_a_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("sbc A, ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC subtract virtual register from A with carry instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sbc_a_vr_destroy(z80ic_sbc_a_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC bitwise AND with virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_and_vr_create(z80ic_and_vr_t **rinstr)
{
	z80ic_and_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_and_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_and_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC bitwise AND with virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_and_vr_print(z80ic_and_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("and ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC bitwise AND with virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_and_vr_destroy(z80ic_and_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC bitwise OR with virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_or_vr_create(z80ic_or_vr_t **rinstr)
{
	z80ic_or_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_or_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_or_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC bitwise OR with virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_or_vr_print(z80ic_or_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("or ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC bitwise OR with virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_or_vr_destroy(z80ic_or_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC bitwise XOR with virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_xor_vr_create(z80ic_xor_vr_t **rinstr)
{
	z80ic_xor_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_xor_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_xor_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC bitwise XOR with virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_xor_vr_print(z80ic_xor_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("xor ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC bitwise XOR with virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_xor_vr_destroy(z80ic_xor_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Create Z80 IC increment virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_inc_vr_create(z80ic_inc_vr_t **rinstr)
{
	z80ic_inc_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_inc_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_inc_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC increment virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_inc_vr_print(z80ic_inc_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("inc ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->vr, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC increment virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_inc_vr_destroy(z80ic_inc_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->vr);
}

/** Create Z80 IC decrement virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_dec_vr_create(z80ic_dec_vr_t **rinstr)
{
	z80ic_dec_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_dec_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_dec_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC decrement virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_dec_vr_print(z80ic_dec_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("dec ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->vr, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC decrement virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_dec_vr_destroy(z80ic_dec_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->vr);
}

/** Create Z80 IC add virtual register pair to virtual register
 * pair instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_add_vrr_vrr_create(z80ic_add_vrr_vrr_t **rinstr)
{
	z80ic_add_vrr_vrr_t *instr;

	instr = calloc(1, sizeof(z80ic_add_vrr_vrr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_add_vrr_vrr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC add virtual register pair to virtual register pair
 * instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_add_vrr_vrr_print(z80ic_add_vrr_vrr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("add ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC add virtual register pair to virtual register pair
 * instruction.
 *
 * @param instr Instruction
 */
static void z80ic_add_vrr_vrr_destroy(z80ic_add_vrr_vrr_t *instr)
{
	z80ic_oper_vrr_destroy(instr->dest);
	z80ic_oper_vrr_destroy(instr->src);
}

/** Create Z80 IC subtract virtual register pair from virtual register
 * pair instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sub_vrr_vrr_create(z80ic_sub_vrr_vrr_t **rinstr)
{
	z80ic_sub_vrr_vrr_t *instr;

	instr = calloc(1, sizeof(z80ic_sub_vrr_vrr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sub_vrr_vrr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC subtract virtual register pair from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sub_vrr_vrr_print(z80ic_sub_vrr_vrr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("sub ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->dest, f);
	if (rc != EOK)
		return rc;

	rv = fputs(", ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC subtract virtual register pair from virtual register pair
 * instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sub_vrr_vrr_destroy(z80ic_sub_vrr_vrr_t *instr)
{
	z80ic_oper_vrr_destroy(instr->dest);
	z80ic_oper_vrr_destroy(instr->src);
}

/** Create Z80 IC increment virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_inc_vrr_create(z80ic_inc_vrr_t **rinstr)
{
	z80ic_inc_vrr_t *instr;

	instr = calloc(1, sizeof(z80ic_inc_vrr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_inc_vrr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC increment virtual register pair instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_inc_vrr_print(z80ic_inc_vrr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("inc ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(instr->vrr, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC increment virtual register pair instruction.
 *
 * @param instr Instruction
 */
static void z80ic_inc_vrr_destroy(z80ic_inc_vrr_t *instr)
{
	z80ic_oper_vrr_destroy(instr->vrr);
}

/** Create Z80 IC rotate left virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_rl_vr_create(z80ic_rl_vr_t **rinstr)
{
	z80ic_rl_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_rl_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_rl_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC rotate left virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_rl_vr_print(z80ic_rl_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("rl ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->vr, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC rotate left virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_rl_vr_destroy(z80ic_rl_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->vr);
}

/** Create Z80 IC rotate right virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_rr_vr_create(z80ic_rr_vr_t **rinstr)
{
	z80ic_rr_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_rr_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_rr_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC rotate right virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_rr_vr_print(z80ic_rr_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("rr ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->vr, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC rotate right virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_rr_vr_destroy(z80ic_rr_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->vr);
}

/** Create Z80 IC shift left arithmetic virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sla_vr_create(z80ic_sla_vr_t **rinstr)
{
	z80ic_sla_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_sla_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sla_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC shift left arithmetic virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sla_vr_print(z80ic_sla_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("sla ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->vr, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC shift left arithmetic virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sla_vr_destroy(z80ic_sla_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->vr);
}

/** Create Z80 IC shift right arithmetic virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_sra_vr_create(z80ic_sra_vr_t **rinstr)
{
	z80ic_sra_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_sra_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_sra_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC shift right arithmetic virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_sra_vr_print(z80ic_sra_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("sra ", f);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->vr, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC shift right arithmetic virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_sra_vr_destroy(z80ic_sra_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->vr);
}

/** Create Z80 test bit of virtual register instruction.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_bit_b_vr_create(z80ic_bit_b_vr_t **rinstr)
{
	z80ic_bit_b_vr_t *instr;

	instr = calloc(1, sizeof(z80ic_bit_b_vr_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_bit_b_vr;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC test bit of virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_bit_b_vr_print(z80ic_bit_b_vr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fprintf(f, "bit %u, ", instr->bit);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(instr->src, f);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Destroy Z80 IC bitwise XOR with virtual register instruction.
 *
 * @param instr Instruction
 */
static void z80ic_bit_b_vr_destroy(z80ic_bit_b_vr_t *instr)
{
	z80ic_oper_vr_destroy(instr->src);
}

/** Print Z80 IC instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
int z80ic_instr_print(z80ic_instr_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputc('\t', f);
	if (rv < 0)
		return EIO;

	switch (instr->itype) {
	case z80i_ld_r_n:
		rc = z80ic_ld_r_n_print((z80ic_ld_r_n_t *) instr->ext, f);
		break;
	case z80i_ld_r_ihl:
		rc = z80ic_ld_r_ihl_print((z80ic_ld_r_ihl_t *) instr->ext, f);
		break;
	case z80i_ld_r_iixd:
		rc = z80ic_ld_r_iixd_print((z80ic_ld_r_iixd_t *) instr->ext, f);
		break;
	case z80i_ld_ihl_r:
		rc = z80ic_ld_ihl_r_print((z80ic_ld_ihl_r_t *) instr->ext, f);
		break;
	case z80i_ld_iixd_r:
		rc = z80ic_ld_iixd_r_print((z80ic_ld_iixd_r_t *) instr->ext, f);
		break;
	case z80i_ld_iixd_n:
		rc = z80ic_ld_iixd_n_print((z80ic_ld_iixd_n_t *) instr->ext, f);
		break;
	case z80i_ld_dd_nn:
		rc = z80ic_ld_dd_nn_print((z80ic_ld_dd_nn_t *) instr->ext, f);
		break;
	case z80i_ld_ix_nn:
		rc = z80ic_ld_ix_nn_print((z80ic_ld_ix_nn_t *) instr->ext, f);
		break;
	case z80i_ld_sp_ix:
		rc = z80ic_ld_sp_ix_print((z80ic_ld_sp_ix_t *) instr->ext, f);
		break;
	case z80i_push_ix:
		rc = z80ic_push_ix_print((z80ic_push_ix_t *) instr->ext, f);
		break;
	case z80i_pop_ix:
		rc = z80ic_pop_ix_print((z80ic_pop_ix_t *) instr->ext, f);
		break;
	case z80i_add_a_iixd:
		rc = z80ic_add_a_iixd_print((z80ic_add_a_iixd_t *) instr->ext, f);
		break;
	case z80i_adc_a_iixd:
		rc = z80ic_adc_a_iixd_print((z80ic_adc_a_iixd_t *) instr->ext, f);
		break;
	case z80i_sub_n:
		rc = z80ic_sub_n_print((z80ic_sub_n_t *) instr->ext, f);
		break;
	case z80i_sub_iixd:
		rc = z80ic_sub_iixd_print((z80ic_sub_iixd_t *) instr->ext, f);
		break;
	case z80i_sbc_a_iixd:
		rc = z80ic_sbc_a_iixd_print((z80ic_sbc_a_iixd_t *) instr->ext, f);
		break;
	case z80i_and_r:
		rc = z80ic_and_r_print((z80ic_and_r_t *) instr->ext, f);
		break;
	case z80i_and_iixd:
		rc = z80ic_and_iixd_print((z80ic_and_iixd_t *) instr->ext, f);
		break;
	case z80i_or_iixd:
		rc = z80ic_or_iixd_print((z80ic_or_iixd_t *) instr->ext, f);
		break;
	case z80i_xor_iixd:
		rc = z80ic_xor_iixd_print((z80ic_xor_iixd_t *) instr->ext, f);
		break;
	case z80i_inc_iixd:
		rc = z80ic_inc_iixd_print((z80ic_inc_iixd_t *) instr->ext, f);
		break;
	case z80i_dec_iixd:
		rc = z80ic_dec_iixd_print((z80ic_dec_iixd_t *) instr->ext, f);
		break;
	case z80i_cpl:
		rc = z80ic_cpl_print((z80ic_cpl_t *) instr->ext, f);
		break;
	case z80i_nop:
		rc = z80ic_nop_print((z80ic_nop_t *) instr->ext, f);
		break;
	case z80i_add_hl_ss:
		rc = z80ic_add_hl_ss_print((z80ic_add_hl_ss_t *) instr->ext, f);
		break;
	case z80i_sbc_hl_ss:
		rc = z80ic_sbc_hl_ss_print((z80ic_sbc_hl_ss_t *) instr->ext, f);
		break;
	case z80i_add_ix_pp:
		rc = z80ic_add_ix_pp_print((z80ic_add_ix_pp_t *) instr->ext, f);
		break;
	case z80i_inc_ss:
		rc = z80ic_inc_ss_print((z80ic_inc_ss_t *) instr->ext, f);
		break;
	case z80i_rla:
		rc = z80ic_rla_print((z80ic_rla_t *) instr->ext, f);
		break;
	case z80i_rl_iixd:
		rc = z80ic_rl_iixd_print((z80ic_rl_iixd_t *) instr->ext, f);
		break;
	case z80i_rr_iixd:
		rc = z80ic_rr_iixd_print((z80ic_rr_iixd_t *) instr->ext, f);
		break;
	case z80i_sla_iixd:
		rc = z80ic_sla_iixd_print((z80ic_sla_iixd_t *) instr->ext, f);
		break;
	case z80i_sra_iixd:
		rc = z80ic_sra_iixd_print((z80ic_sra_iixd_t *) instr->ext, f);
		break;
	case z80i_bit_b_iixd:
		rc = z80ic_bit_b_iixd_print((z80ic_bit_b_iixd_t *) instr->ext, f);
		break;
	case z80i_jp_nn:
		rc = z80ic_jp_nn_print((z80ic_jp_nn_t *) instr->ext, f);
		break;
	case z80i_jp_cc_nn:
		rc = z80ic_jp_cc_nn_print((z80ic_jp_cc_nn_t *) instr->ext, f);
		break;
	case z80i_call_nn:
		rc = z80ic_call_nn_print((z80ic_call_nn_t *) instr->ext, f);
		break;
	case z80i_ret:
		rc = z80ic_ret_print((z80ic_ret_t *) instr->ext, f);
		break;
	case z80i_ld_vr_vr:
		rc = z80ic_ld_vr_vr_print((z80ic_ld_vr_vr_t *) instr->ext,
		    f);
		break;
	case z80i_ld_vr_n:
		rc = z80ic_ld_vr_n_print((z80ic_ld_vr_n_t *) instr->ext,
		    f);
		break;
	case z80i_ld_vr_ihl:
		rc = z80ic_ld_vr_ihl_print((z80ic_ld_vr_ihl_t *) instr->ext,
		    f);
		break;
	case z80i_ld_ihl_vr:
		rc = z80ic_ld_ihl_vr_print((z80ic_ld_ihl_vr_t *) instr->ext,
		    f);
		break;
	case z80i_ld_vrr_vrr:
		rc = z80ic_ld_vrr_vrr_print((z80ic_ld_vrr_vrr_t *) instr->ext,
		    f);
		break;
	case z80i_ld_r_vr:
		rc = z80ic_ld_r_vr_print((z80ic_ld_r_vr_t *) instr->ext,
		    f);
		break;
	case z80i_ld_vr_r:
		rc = z80ic_ld_vr_r_print((z80ic_ld_vr_r_t *) instr->ext,
		    f);
		break;
	case z80i_ld_r16_vrr:
		rc = z80ic_ld_r16_vrr_print((z80ic_ld_r16_vrr_t *) instr->ext,
		    f);
		break;
	case z80i_ld_vrr_r16:
		rc = z80ic_ld_vrr_r16_print((z80ic_ld_vrr_r16_t *) instr->ext,
		    f);
		break;
	case z80i_ld_vrr_nn:
		rc = z80ic_ld_vrr_nn_print((z80ic_ld_vrr_nn_t *) instr->ext, f);
		break;
	case z80i_ld_vrr_spnn:
		rc = z80ic_ld_vrr_spnn_print((z80ic_ld_vrr_spnn_t *) instr->ext, f);
		break;
	case z80i_add_a_vr:
		rc = z80ic_add_a_vr_print((z80ic_add_a_vr_t *) instr->ext, f);
		break;
	case z80i_adc_a_vr:
		rc = z80ic_adc_a_vr_print((z80ic_adc_a_vr_t *) instr->ext, f);
		break;
	case z80i_sub_vr:
		rc = z80ic_sub_vr_print((z80ic_sub_vr_t *) instr->ext, f);
		break;
	case z80i_sbc_a_vr:
		rc = z80ic_sbc_a_vr_print((z80ic_sbc_a_vr_t *) instr->ext, f);
		break;
	case z80i_and_vr:
		rc = z80ic_and_vr_print((z80ic_and_vr_t *) instr->ext, f);
		break;
	case z80i_or_vr:
		rc = z80ic_or_vr_print((z80ic_or_vr_t *) instr->ext, f);
		break;
	case z80i_xor_vr:
		rc = z80ic_xor_vr_print((z80ic_xor_vr_t *) instr->ext, f);
		break;
	case z80i_inc_vr:
		rc = z80ic_inc_vr_print((z80ic_inc_vr_t *) instr->ext, f);
		break;
	case z80i_dec_vr:
		rc = z80ic_dec_vr_print((z80ic_dec_vr_t *) instr->ext, f);
		break;
	case z80i_add_vrr_vrr:
		rc = z80ic_add_vrr_vrr_print((z80ic_add_vrr_vrr_t *) instr->ext,
		    f);
		break;
	case z80i_sub_vrr_vrr:
		rc = z80ic_sub_vrr_vrr_print((z80ic_sub_vrr_vrr_t *) instr->ext,
		    f);
		break;
	case z80i_inc_vrr:
		rc = z80ic_inc_vrr_print((z80ic_inc_vrr_t *) instr->ext,
		    f);
		break;
	case z80i_rl_vr:
		rc = z80ic_rl_vr_print((z80ic_rl_vr_t *) instr->ext, f);
		break;
	case z80i_rr_vr:
		rc = z80ic_rr_vr_print((z80ic_rr_vr_t *) instr->ext, f);
		break;
	case z80i_sla_vr:
		rc = z80ic_sla_vr_print((z80ic_sla_vr_t *) instr->ext, f);
		break;
	case z80i_sra_vr:
		rc = z80ic_sra_vr_print((z80ic_sra_vr_t *) instr->ext, f);
		break;
	case z80i_bit_b_vr:
		rc = z80ic_bit_b_vr_print((z80ic_bit_b_vr_t *) instr->ext, f);
		break;
	default:
		assert(false);
		rc = ENOTSUP;
		break;
	}

	rv = fputc('\n', f);
	if (rv < 0)
		return EIO;

	return rc;
}

/** Destroy Z80 IC instruction.
 *
 * @param instr Z80 IC instruction or @c NULL
 */
void z80ic_instr_destroy(z80ic_instr_t *instr)
{
	if (instr == NULL)
		return;

	switch (instr->itype) {
	case z80i_ld_r_n:
		z80ic_ld_r_n_destroy((z80ic_ld_r_n_t *) instr->ext);
		break;
	case z80i_ld_r_ihl:
		z80ic_ld_r_ihl_destroy((z80ic_ld_r_ihl_t *) instr->ext);
		break;
	case z80i_ld_r_iixd:
		z80ic_ld_r_iixd_destroy((z80ic_ld_r_iixd_t *) instr->ext);
		break;
	case z80i_ld_ihl_r:
		z80ic_ld_ihl_r_destroy((z80ic_ld_ihl_r_t *) instr->ext);
		break;
	case z80i_ld_iixd_r:
		z80ic_ld_iixd_r_destroy((z80ic_ld_iixd_r_t *) instr->ext);
		break;
	case z80i_ld_iixd_n:
		z80ic_ld_iixd_n_destroy((z80ic_ld_iixd_n_t *) instr->ext);
		break;
	case z80i_ld_dd_nn:
		z80ic_ld_dd_nn_destroy((z80ic_ld_dd_nn_t *) instr->ext);
		break;
	case z80i_ld_ix_nn:
		z80ic_ld_ix_nn_destroy((z80ic_ld_ix_nn_t *) instr->ext);
		break;
	case z80i_ld_sp_ix:
		z80ic_ld_sp_ix_destroy((z80ic_ld_sp_ix_t *) instr->ext);
		break;
	case z80i_push_ix:
		z80ic_push_ix_destroy((z80ic_push_ix_t *) instr->ext);
		break;
	case z80i_pop_ix:
		z80ic_pop_ix_destroy((z80ic_pop_ix_t *) instr->ext);
		break;
	case z80i_add_hl_ss:
		z80ic_add_hl_ss_destroy((z80ic_add_hl_ss_t *) instr->ext);
		break;
	case z80i_add_a_iixd:
		z80ic_add_a_iixd_destroy((z80ic_add_a_iixd_t *) instr->ext);
		break;
	case z80i_adc_a_iixd:
		z80ic_adc_a_iixd_destroy((z80ic_adc_a_iixd_t *) instr->ext);
		break;
	case z80i_sub_n:
		z80ic_sub_n_destroy((z80ic_sub_n_t *) instr->ext);
		break;
	case z80i_sub_iixd:
		z80ic_sub_iixd_destroy((z80ic_sub_iixd_t *) instr->ext);
		break;
	case z80i_sbc_a_iixd:
		z80ic_sbc_a_iixd_destroy((z80ic_sbc_a_iixd_t *) instr->ext);
		break;
	case z80i_and_r:
		z80ic_and_r_destroy((z80ic_and_r_t *) instr->ext);
		break;
	case z80i_and_iixd:
		z80ic_and_iixd_destroy((z80ic_and_iixd_t *) instr->ext);
		break;
	case z80i_or_iixd:
		z80ic_or_iixd_destroy((z80ic_or_iixd_t *) instr->ext);
		break;
	case z80i_xor_iixd:
		z80ic_xor_iixd_destroy((z80ic_xor_iixd_t *) instr->ext);
		break;
	case z80i_inc_iixd:
		z80ic_inc_iixd_destroy((z80ic_inc_iixd_t *) instr->ext);
		break;
	case z80i_dec_iixd:
		z80ic_dec_iixd_destroy((z80ic_dec_iixd_t *) instr->ext);
		break;
	case z80i_cpl:
		z80ic_cpl_destroy((z80ic_cpl_t *) instr->ext);
		break;
	case z80i_nop:
		z80ic_nop_destroy((z80ic_nop_t *) instr->ext);
		break;
	case z80i_sbc_hl_ss:
		z80ic_sbc_hl_ss_destroy((z80ic_sbc_hl_ss_t *) instr->ext);
		break;
	case z80i_add_ix_pp:
		z80ic_add_ix_pp_destroy((z80ic_add_ix_pp_t *) instr->ext);
		break;
	case z80i_inc_ss:
		z80ic_inc_ss_destroy((z80ic_inc_ss_t *) instr->ext);
		break;
	case z80i_rla:
		z80ic_rla_destroy((z80ic_rla_t *) instr->ext);
		break;
	case z80i_rl_iixd:
		z80ic_rl_iixd_destroy((z80ic_rl_iixd_t *) instr->ext);
		break;
	case z80i_rr_iixd:
		z80ic_rr_iixd_destroy((z80ic_rr_iixd_t *) instr->ext);
		break;
	case z80i_sla_iixd:
		z80ic_sla_iixd_destroy((z80ic_sla_iixd_t *) instr->ext);
		break;
	case z80i_sra_iixd:
		z80ic_sra_iixd_destroy((z80ic_sra_iixd_t *) instr->ext);
		break;
	case z80i_bit_b_iixd:
		z80ic_bit_b_iixd_destroy((z80ic_bit_b_iixd_t *) instr->ext);
		break;
	case z80i_jp_nn:
		z80ic_jp_nn_destroy((z80ic_jp_nn_t *) instr->ext);
		break;
	case z80i_jp_cc_nn:
		z80ic_jp_cc_nn_destroy((z80ic_jp_cc_nn_t *) instr->ext);
		break;
	case z80i_call_nn:
		z80ic_call_nn_destroy((z80ic_call_nn_t *) instr->ext);
		break;
	case z80i_ret:
		z80ic_ret_destroy((z80ic_ret_t *) instr->ext);
		break;
	case z80i_ld_vr_vr:
		z80ic_ld_vr_vr_destroy((z80ic_ld_vr_vr_t *) instr->ext);
		break;
	case z80i_ld_vr_n:
		z80ic_ld_vr_n_destroy((z80ic_ld_vr_n_t *) instr->ext);
		break;
	case z80i_ld_vr_ihl:
		z80ic_ld_vr_ihl_destroy((z80ic_ld_vr_ihl_t *) instr->ext);
		break;
	case z80i_ld_ihl_vr:
		z80ic_ld_ihl_vr_destroy((z80ic_ld_ihl_vr_t *) instr->ext);
		break;
	case z80i_ld_vrr_vrr:
		z80ic_ld_vrr_vrr_destroy((z80ic_ld_vrr_vrr_t *) instr->ext);
		break;
	case z80i_ld_r_vr:
		z80ic_ld_r_vr_destroy((z80ic_ld_r_vr_t *) instr->ext);
		break;
	case z80i_ld_vr_r:
		z80ic_ld_vr_r_destroy((z80ic_ld_vr_r_t *) instr->ext);
		break;
	case z80i_ld_r16_vrr:
		z80ic_ld_r16_vrr_destroy((z80ic_ld_r16_vrr_t *) instr->ext);
		break;
	case z80i_ld_vrr_r16:
		z80ic_ld_vrr_r16_destroy((z80ic_ld_vrr_r16_t *) instr->ext);
		break;
	case z80i_ld_vrr_nn:
		z80ic_ld_vrr_nn_destroy((z80ic_ld_vrr_nn_t *) instr->ext);
		break;
	case z80i_ld_vrr_spnn:
		z80ic_ld_vrr_spnn_destroy((z80ic_ld_vrr_spnn_t *) instr->ext);
		break;
	case z80i_add_a_vr:
		z80ic_add_a_vr_destroy((z80ic_add_a_vr_t *) instr->ext);
		break;
	case z80i_adc_a_vr:
		z80ic_adc_a_vr_destroy((z80ic_adc_a_vr_t *) instr->ext);
		break;
	case z80i_sub_vr:
		z80ic_sub_vr_destroy((z80ic_sub_vr_t *) instr->ext);
		break;
	case z80i_sbc_a_vr:
		z80ic_sbc_a_vr_destroy((z80ic_sbc_a_vr_t *) instr->ext);
		break;
	case z80i_and_vr:
		z80ic_and_vr_destroy((z80ic_and_vr_t *) instr->ext);
		break;
	case z80i_or_vr:
		z80ic_or_vr_destroy((z80ic_or_vr_t *) instr->ext);
		break;
	case z80i_xor_vr:
		z80ic_xor_vr_destroy((z80ic_xor_vr_t *) instr->ext);
		break;
	case z80i_inc_vr:
		z80ic_inc_vr_destroy((z80ic_inc_vr_t *) instr->ext);
		break;
	case z80i_dec_vr:
		z80ic_dec_vr_destroy((z80ic_dec_vr_t *) instr->ext);
		break;
	case z80i_add_vrr_vrr:
		z80ic_add_vrr_vrr_destroy((z80ic_add_vrr_vrr_t *) instr->ext);
		break;
	case z80i_sub_vrr_vrr:
		z80ic_sub_vrr_vrr_destroy((z80ic_sub_vrr_vrr_t *) instr->ext);
		break;
	case z80i_inc_vrr:
		z80ic_inc_vrr_destroy((z80ic_inc_vrr_t *) instr->ext);
		break;
	case z80i_rl_vr:
		z80ic_rl_vr_destroy((z80ic_rl_vr_t *) instr->ext);
		break;
	case z80i_rr_vr:
		z80ic_rr_vr_destroy((z80ic_rr_vr_t *) instr->ext);
		break;
	case z80i_sla_vr:
		z80ic_sla_vr_destroy((z80ic_sla_vr_t *) instr->ext);
		break;
	case z80i_sra_vr:
		z80ic_sra_vr_destroy((z80ic_sra_vr_t *) instr->ext);
		break;
	case z80i_bit_b_vr:
		z80ic_bit_b_vr_destroy((z80ic_bit_b_vr_t *) instr->ext);
		break;
	default:
		assert(false);
		break;
	}

	free(instr);
}

/** Create Z80 IC 8-bit immediate operand.
 *
 * @param value Value
 * @param rimm Place to store pointer to new Z80 IC immediate operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_imm8_create(uint8_t value, z80ic_oper_imm8_t **rimm)
{
	z80ic_oper_imm8_t *imm;

	imm = calloc(1, sizeof(z80ic_oper_imm8_t));
	if (imm == NULL)
		return ENOMEM;

	imm->imm8 = value;

	*rimm = imm;
	return EOK;
}

/** Print Z80 IC 8-bit immediate operand.
 *
 * @param imm Z80 IC 8-bit immediate operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_imm8_print(z80ic_oper_imm8_t *imm, FILE *f)
{
	int rv;

	rv = fprintf(f, "%" PRIu8, imm->imm8);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC 8-bit immediate operand.
 *
 * @param imm Z80 IC 8-bit immediate operand
 */
void z80ic_oper_imm8_destroy(z80ic_oper_imm8_t *imm)
{
	free(imm);
}

/** Create Z80 IC 16-bit immediate operand with value.
 *
 * @param value Value
 * @param rimm Place to store pointer to new Z80 IC immediate operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_imm16_create_val(uint16_t value, z80ic_oper_imm16_t **rimm)
{
	z80ic_oper_imm16_t *imm;

	imm = calloc(1, sizeof(z80ic_oper_imm16_t));
	if (imm == NULL)
		return ENOMEM;

	imm->symbol = NULL;
	imm->imm16 = value;

	*rimm = imm;
	return EOK;
}

/** Create Z80 IC 16-bit immediate operand with symbol reference.
 *
 * @param symbol Symbol
 * @param rimm Place to store pointer to new Z80 IC immediate operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_imm16_create_symbol(const char *symbol,
    z80ic_oper_imm16_t **rimm)
{
	z80ic_oper_imm16_t *imm;
	char *dsymbol;

	imm = calloc(1, sizeof(z80ic_oper_imm16_t));
	if (imm == NULL)
		return ENOMEM;

	dsymbol = strdup(symbol);
	if (dsymbol == NULL) {
		free(imm);
		return ENOMEM;
	}

	imm->symbol = dsymbol;
	imm->imm16 = 0;

	*rimm = imm;
	return EOK;
}

/** Create copy of Z80 IC 16-bit immediate operand.
 *
 * @param orig Source 16-bit immediate operand
 * @param rimm Place to store pointer to new Z80 IC immediate operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_imm16_copy(z80ic_oper_imm16_t *orig, z80ic_oper_imm16_t **rimm)
{
	z80ic_oper_imm16_t *imm;
	int rc;

	if (orig->symbol != NULL) {
		rc = z80ic_oper_imm16_create_symbol(orig->symbol, &imm);
		if (rc != EOK)
			return rc;
	} else {
		rc = z80ic_oper_imm16_create_val(orig->imm16, &imm);
		if (rc != EOK)
			return rc;
	}

	*rimm = imm;
	return EOK;
}

/** Print Z80 IC 16-bit immediate operand.
 *
 * @param imm Z80 IC 16-bit immediate operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_imm16_print(z80ic_oper_imm16_t *imm, FILE *f)
{
	int rv;

	if (imm->symbol != NULL) {
		rv = fputs(imm->symbol, f);
		if (rv < 0)
			return EIO;
	} else {
		rv = fprintf(f, "%" PRIu16, imm->imm16);
		if (rv < 0)
			return EIO;
	}

	return EOK;
}

/** Destroy Z80 IC 16-bit immediate operand.
 *
 * @param imm Z80 IC 16-bit immediate operand
 */
void z80ic_oper_imm16_destroy(z80ic_oper_imm16_t *imm)
{
	if (imm->symbol != NULL)
		free(imm->symbol);
	free(imm);
}

/** Create Z80 IC register operand.
 *
 * @param reg Register
 * @param rreg Place to store pointer to new Z80 IC register operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_reg_create(z80ic_reg_t reg, z80ic_oper_reg_t **rreg)
{
	z80ic_oper_reg_t *oreg;

	oreg = calloc(1, sizeof(z80ic_oper_reg_t));
	if (oreg == NULL)
		return ENOMEM;

	oreg->reg = reg;

	*rreg = oreg;
	return EOK;
}

/** Print Z80 IC register operand.
 *
 * @param reg Z80 IC register operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_reg_print(z80ic_oper_reg_t *reg, FILE *f)
{
	int rv;

	rv = fputs(z80ic_reg_name[reg->reg], f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC register operand.
 *
 * @param reg Z80 IC register operand
 */
void z80ic_oper_reg_destroy(z80ic_oper_reg_t *reg)
{
	free(reg);
}

/** Create Z80 IC 16-bit dd register operand.
 *
 * @param rdd 16-bit dd register
 * @param rreg Place to store pointer to new Z80 IC register operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_dd_create(z80ic_dd_t rdd, z80ic_oper_dd_t **rodd)
{
	z80ic_oper_dd_t *odd;

	odd = calloc(1, sizeof(z80ic_oper_dd_t));
	if (odd == NULL)
		return ENOMEM;

	odd->rdd = rdd;

	*rodd = odd;
	return EOK;
}

/** Print Z80 IC 16-bit dd register operand.
 *
 * @param dd Z80 IC 16-bit dd register operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_dd_print(z80ic_oper_dd_t *dd, FILE *f)
{
	int rv;

	rv = fputs(z80ic_dd_name[dd->rdd], f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC 16-bit dd register operand.
 *
 * @param dd Z80 IC 16-bit dd register operand
 */
void z80ic_oper_dd_destroy(z80ic_oper_dd_t *dd)
{
	free(dd);
}

/** Create Z80 IC 16-bit pp register operand.
 *
 * @param rpp 16-bit pp register
 * @param rreg Place to store pointer to new Z80 IC register operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_pp_create(z80ic_pp_t rpp, z80ic_oper_pp_t **ropp)
{
	z80ic_oper_pp_t *opp;

	opp = calloc(1, sizeof(z80ic_oper_pp_t));
	if (opp == NULL)
		return ENOMEM;

	opp->rpp = rpp;

	*ropp = opp;
	return EOK;
}

/** Print Z80 IC 16-bit pp register operand.
 *
 * @param pp Z80 IC 16-bit pp register operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_pp_print(z80ic_oper_pp_t *pp, FILE *f)
{
	int rv;

	rv = fputs(z80ic_pp_name[pp->rpp], f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC 16-bit pp register operand.
 *
 * @param pp Z80 IC 16-bit pp register operand
 */
void z80ic_oper_pp_destroy(z80ic_oper_pp_t *pp)
{
	free(pp);
}

/** Create Z80 IC 16-bit ss register operand.
 *
 * @param rss 16-bit ss register
 * @param rreg Place to store pointer to new Z80 IC register operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_ss_create(z80ic_ss_t rss, z80ic_oper_ss_t **ross)
{
	z80ic_oper_ss_t *oss;

	oss = calloc(1, sizeof(z80ic_oper_ss_t));
	if (oss == NULL)
		return ENOMEM;

	oss->rss = rss;

	*ross = oss;
	return EOK;
}

/** Print Z80 IC 16-bit ss register operand.
 *
 * @param ss Z80 IC 16-bit ss register operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_ss_print(z80ic_oper_ss_t *ss, FILE *f)
{
	int rv;

	rv = fputs(z80ic_ss_name[ss->rss], f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC 16-bit ss register operand.
 *
 * @param ss Z80 IC 16-bit ss register operand
 */
void z80ic_oper_ss_destroy(z80ic_oper_ss_t *ss)
{
	free(ss);
}

/** Create Z80 IC 16-bit register operand.
 *
 * @param reg Register
 * @param rreg Place to store pointer to new Z80 IC 16-bit register operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_r16_create(z80ic_r16_t reg, z80ic_oper_r16_t **rreg)
{
	z80ic_oper_r16_t *oreg;

	oreg = calloc(1, sizeof(z80ic_oper_r16_t));
	if (oreg == NULL)
		return ENOMEM;

	oreg->r16 = reg;

	*rreg = oreg;
	return EOK;
}

/** Print Z80 IC 16-bit register operand.
 *
 * @param reg Z80 IC 16-bit register operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_r16_print(z80ic_oper_r16_t *reg, FILE *f)
{
	int rv;

	rv = fputs(z80ic_r16_name[reg->r16], f);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC 16-bit register operand.
 *
 * @param reg Z80 IC 16-bit register operand
 */
void z80ic_oper_r16_destroy(z80ic_oper_r16_t *reg)
{
	free(reg);
}

/** Create Z80 IC virtual register operand.
 *
 * @param vregno Virtual register number
 * @param part Virtual register part
 * @param rvr Place to store pointer to new Z80 IC virtual register operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_vr_create(unsigned vregno, z80ic_vr_part_t part,
    z80ic_oper_vr_t **rvr)
{
	z80ic_oper_vr_t *vr;

	vr = calloc(1, sizeof(z80ic_oper_vr_t));
	if (vr == NULL)
		return ENOMEM;

	vr->vregno = vregno;
	vr->part = part;

	*rvr = vr;
	return EOK;
}

/** Print Z80 IC virtual register operand.
 *
 * @param vr Z80 IC virtual register operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_vr_print(z80ic_oper_vr_t *vr, FILE *f)
{
	int rv;

	switch (vr->part) {
	case z80ic_vrp_r8:
		rv = fprintf(f, "%%%u", vr->vregno);
		if (rv < 0)
			return EIO;
		break;
	case z80ic_vrp_r16l:
		rv = fprintf(f, "%%%%%u.L", vr->vregno);
		if (rv < 0)
			return EIO;
		break;
	case z80ic_vrp_r16h:
		rv = fprintf(f, "%%%%%u.H", vr->vregno);
		if (rv < 0)
			return EIO;
		break;
	}

	return EOK;
}

/** Destroy Z80 IC virtual register operand.
 *
 * @param reg Z80 IC virtual register operand
 */
void z80ic_oper_vr_destroy(z80ic_oper_vr_t *vr)
{
	free(vr);
}

/** Create Z80 IC virtual register pair operand.
 *
 * @param vregno Virtual register pair number
 * @param rvr Place to store pointer to new Z80 IC virtual register pair operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_vrr_create(unsigned vregno, z80ic_oper_vrr_t **rvrr)
{
	z80ic_oper_vrr_t *vrr;

	vrr = calloc(1, sizeof(z80ic_oper_vrr_t));
	if (vrr == NULL)
		return ENOMEM;

	vrr->vregno = vregno;

	*rvrr = vrr;
	return EOK;
}

/** Print Z80 IC virtual register pair operand.
 *
 * @param vrr Z80 IC virtual register pair operand
 * @param f Output file
 * @return EOK on success or an error code
 */
int z80ic_oper_vrr_print(z80ic_oper_vrr_t *vrr, FILE *f)
{
	int rv;

	rv = fprintf(f, "%%%%%u", vrr->vregno);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Destroy Z80 IC virtual register pair operand.
 *
 * @param reg Z80 IC virtual register pair operand
 */
void z80ic_oper_vrr_destroy(z80ic_oper_vrr_t *vrr)
{
	free(vrr);
}

/** Get lower half of 16-bit register.
 *
 * The lower half of the register must be addressable, otherwise the
 * result is undefined (cannot address lower half of AF, IX, IY, SP)
 *
 * @param r16 16-bit register (must be a register pair, not AF)
 * @return Which 8-bit register is the lower half of the pair
 */
z80ic_reg_t z80ic_r16_lo(z80ic_r16_t r16)
{
	switch (r16) {
	case z80ic_r16_af:
		assert(false);
		return 0;
	case z80ic_r16_bc:
		return z80ic_reg_c;
	case z80ic_r16_de:
		return z80ic_reg_e;
	case z80ic_r16_hl:
		return z80ic_reg_l;
	case z80ic_r16_ix:
	case z80ic_r16_iy:
	case z80ic_r16_sp:
		assert(false);
		return 0;
	}

	assert(false);
	return 0;
}

/** Get upper half of 16-bit register.
 *
 * The upper half of the register must be addressable, otherwise the
 * result is undefined (cannot address upper half of IX, IY, SP)
 *
 * @param r16 16-bit register (must be a register pair)
 * @return Which 8-bit register is the upper half of the pair
 */
z80ic_reg_t z80ic_r16_hi(z80ic_r16_t r16)
{
	switch (r16) {
	case z80ic_r16_af:
		return z80ic_reg_a;
	case z80ic_r16_bc:
		return z80ic_reg_b;
	case z80ic_r16_de:
		return z80ic_reg_d;
	case z80ic_r16_hl:
		return z80ic_reg_h;
	case z80ic_r16_ix:
	case z80ic_r16_iy:
	case z80ic_r16_sp:
		assert(false);
		return 0;
	}

	assert(false);
	return 0;
}
