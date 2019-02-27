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
	case z80icd_proc:
		return z80ic_proc_print((z80ic_proc_t *) decln->ext, f);
	}

	/* Should not be reached */
	assert(false);
	return EINVAL;
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
	int rv;
	int rc;

	rv = fprintf(f, "\n; proc %s\n.%s\n", proc->ident, proc->ident);
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

/** Destroy Z80 IC procedure.
 *
 * @param proc Z80 IC procedure or @c NULL
 */
void z80ic_proc_destroy(z80ic_proc_t *proc)
{
	if (proc == NULL)
		return;

	if (proc->ident != NULL)
		free(proc->ident);

	z80ic_lblock_destroy(proc->lblock);
	free(proc);
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

		rc = z80ic_instr_print(entry->instr, f);
		if (rc != EOK)
			return rc;

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

/** Create Z80 IC instruction load virtual register pair from 16-bit immediate.
 *
 * @param rinstr Place to store pointer to new instruction
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_instr_ld_vrr_nn_create(z80ic_instr_ld_vrr_nn_t **rinstr)
{
	z80ic_instr_ld_vrr_nn_t *instr;

	instr = calloc(1, sizeof(z80ic_instr_ld_vrr_nn_t));
	if (instr == NULL)
		return ENOMEM;

	instr->instr.itype = z80i_ld_vrr_nn;
	instr->instr.ext = instr;
	*rinstr = instr;
	return EOK;
}

/** Print Z80 IC load 16-bit immediate to virtual register instruction.
 *
 * @param instr Instruction
 * @param f Output file
 */
static int z80ic_instr_ld_vrr_nn_print(z80ic_instr_ld_vrr_nn_t *instr, FILE *f)
{
	int rc;
	int rv;

	rv = fputs("LD ", f);
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
	case z80i_ld_vrr_nn:
		rc = z80ic_instr_ld_vrr_nn_print(
		    (z80ic_instr_ld_vrr_nn_t *) instr->ext, f);
		break;
	default:
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

	// TODO

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

/** Create Z80 IC virtual register operand.
 *
 * @param vregno Virtual register number
 * @param rvr Place to store pointer to new Z80 IC virtual register operand
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_oper_vr_create(unsigned vregno, z80ic_oper_vr_t **rvr)
{
	z80ic_oper_vr_t *vr;

	vr = calloc(1, sizeof(z80ic_oper_vr_t));
	if (vr == NULL)
		return ENOMEM;

	vr->vregno = vregno;

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

	rv = fprintf(f, "%%%u", vr->vregno);
	if (rv < 0)
		return EIO;

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
