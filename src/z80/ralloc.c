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
 * Z80 Register allocation
 *
 * Convert Z80 IC with virtual registers to pure Z80 IC (not using virtual
 * registers).
 */

#include <assert.h>
#include <ir.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <z80/ralloc.h>
#include <z80/z80ic.h>

/** Create register allocator.
 *
 * @param rz80_ralloc Place to store pointer to new register allocator
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_ralloc_create(z80_ralloc_t **rz80_ralloc)
{
	z80_ralloc_t *ralloc;

	ralloc = calloc(1, sizeof(z80_ralloc_t));
	if (ralloc == NULL)
		return ENOMEM;

	*rz80_ralloc = ralloc;
	return EOK;
}

/** Create register allocator.
 *
 * @param rz80_ralloc Place to store pointer to new register allocator
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_proc_create(z80_ralloc_t *ralloc,
    z80_ralloc_proc_t **rraproc)
{
	z80_ralloc_proc_t *raproc;

	raproc = calloc(1, sizeof(z80_ralloc_proc_t));
	if (raproc == NULL)
		return ENOMEM;

	raproc->ralloc = ralloc;
	*rraproc = raproc;
	return EOK;
}

/** Destroy register allocator for procedure.
 *
 * @param ralloc Register allocator or @c NULL
 */
static void z80_ralloc_proc_destroy(z80_ralloc_proc_t *raproc)
{
	if (raproc == NULL)
		return;

	free(raproc);
}

/** Add instructions to allocate a stack frame.
 *
 * @param nbytes Stack frame size in bytes
 * @param lblock Logical block (must be empty)
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_sfalloc(size_t nbytes, z80ic_lblock_t *lblock)
{
	z80ic_push_ix_t *push = NULL;
	z80ic_ld_ix_nn_t *ldix = NULL;
	z80ic_add_ix_pp_t *addix = NULL;
	z80ic_ld_sp_ix_t *ldspix = NULL;
	z80ic_oper_pp_t *pp;
	z80ic_oper_imm16_t *imm;
	int rc;

	(void) nbytes;

	/*
	 * With all the glory of the Z80 instruction set where we cannot
	 * read the SP or add to SP, the only really feasible way to set up
	 * a stack frame is:
	 *
	 *	push IX			; store previous frame pointer
	 *	ld IX, -nbytes		; compute new stack top
	 *	add IX, SP
	 *	ld SP, IX		; save to SP
	 *
	 *	ld IX, +nbytes		; make IX point to the bottom of
	 *	add IX, SP		; the stack frame again
	 *
	 * The last two instructions could be skipped as an optimization,
	 * if we are sure the stack frame fits into 127 bytes anyway or
	 * modified to cover more area if we have little arguments and
	 * many locals or vice versa.
	 */

	/* push IX */

	rc = z80ic_push_ix_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	/* ld IX, -nbytes */

	rc = z80ic_ld_ix_nn_create(&ldix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_val(-(uint16_t) nbytes, &imm);
	if (rc != EOK)
		goto error;

	ldix->imm16 = imm;

	rc = z80ic_lblock_append(lblock, NULL, &ldix->instr);
	if (rc != EOK)
		goto error;

	ldix = NULL;

	/* add IX, SP */

	rc = z80ic_add_ix_pp_create(&addix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_pp_create(z80ic_pp_sp, &pp);
	if (rc != EOK)
		goto error;

	addix->src = pp;

	rc = z80ic_lblock_append(lblock, NULL, &addix->instr);
	if (rc != EOK)
		goto error;

	addix = NULL;

	/* ld SP, IX */

	rc = z80ic_ld_sp_ix_create(&ldspix);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &ldspix->instr);
	if (rc != EOK)
		goto error;

	ldspix = NULL;

	/* ld IX, +nbytes */

	rc = z80ic_ld_ix_nn_create(&ldix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_val((uint16_t) nbytes, &imm);
	if (rc != EOK)
		goto error;

	ldix->imm16 = imm;

	rc = z80ic_lblock_append(lblock, NULL, &ldix->instr);
	if (rc != EOK)
		goto error;

	ldix = NULL;

	/* add IX, SP */

	rc = z80ic_add_ix_pp_create(&addix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_pp_create(z80ic_pp_sp, &pp);
	if (rc != EOK)
		goto error;

	addix->src = pp;

	rc = z80ic_lblock_append(lblock, NULL, &addix->instr);
	if (rc != EOK)
		goto error;

	addix = NULL;

	return EOK;
error:
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	if (ldix != NULL)
		z80ic_instr_destroy(&ldix->instr);
	if (addix != NULL)
		z80ic_instr_destroy(&addix->instr);
	if (ldspix != NULL)
		z80ic_instr_destroy(&ldspix->instr);

	return rc;
}

/** Append instructions to de allocate the stack frame.
 *
 * @param lblock Logical block
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_sffree(z80ic_lblock_t *lblock)
{
	z80ic_pop_ix_t *pop = NULL;
	int rc;

	/* pop IX */

	rc = z80ic_pop_ix_create(&pop);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &pop->instr);
	if (rc != EOK)
		goto error;

	pop = NULL;
	return EOK;
error:
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);

	return rc;
}

/** Allocate registers for Z80 return instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrret Return instruction with VR
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_ralloc_ret(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ret_t *vrret, z80ic_lblock_t *lblock)
{
	z80ic_ret_t *ret = NULL;
	int rc;

	(void) raproc;
	(void) vrret;

	/* Insert epilogue to free the stack frame */
	rc = z80_ralloc_sffree(lblock);
	if (rc != EOK)
		goto error;

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;
	return EOK;
error:
	z80ic_instr_destroy(&ret->instr);
	return rc;
}

/** Allocate registers for Z80 instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrinstr IR instruction
 * @param ricinstr Place to store pointer to new Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_ralloc_instr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_instr_t *vrinstr, z80ic_lblock_t *lblock)
{
	switch (vrinstr->itype) {
	case z80i_ret:
		return z80_ralloc_ret(raproc, label,
		    (z80ic_ret_t *) vrinstr->ext, lblock);
	default:
		return EOK;
		assert(false);
		return EINVAL;
	}

	assert(false);
	return EINVAL;
}

/** Allocate registers for Z80 procedure.
 *
 * @param ralloc Register allocator
 * @param proc IR procedure
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_ralloc_proc(z80_ralloc_t *ralloc, z80ic_proc_t *vrproc,
    z80ic_module_t *icmod)
{
	z80_ralloc_proc_t *raproc = NULL;
	z80ic_lblock_entry_t *entry;
	z80ic_proc_t *icproc = NULL;
	z80ic_lblock_t *lblock = NULL;
	z80ic_instr_t *instr = NULL;
	int rc;

	rc = z80_ralloc_proc_create(ralloc, &raproc);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = z80ic_proc_create(vrproc->ident, lblock, &icproc);
	if (rc != EOK)
		goto error;

	/* Insert prologue to allocate a stack frame */
	rc = z80_ralloc_sfalloc(42, lblock);
	if (rc != EOK)
		goto error;

	/* Convert each instruction */
	entry = z80ic_lblock_first(vrproc->lblock);
	while (entry != NULL) {
		rc = z80_ralloc_instr(raproc, entry->label, entry->instr, lblock);
		if (rc != EOK)
			goto error;

		entry = z80ic_lblock_next(entry);
	}

	z80_ralloc_proc_destroy(raproc);
	z80ic_module_append(icmod, &icproc->decln);
	return EOK;
error:
	z80ic_proc_destroy(icproc);
	z80ic_lblock_destroy(lblock);
	z80ic_instr_destroy(instr);
	z80_ralloc_proc_destroy(raproc);
	return rc;
}

/** Allocate registers for Z80 IC declaration.
 *
 * @param ralloc Register allocator
 * @param decln IR declaration
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_ralloc_decln(z80_ralloc_t *ralloc, z80ic_decln_t *decln,
    z80ic_module_t *icmod)
{
	int rc;

	switch (decln->dtype) {
	case ird_proc:
		rc = z80_ralloc_proc(ralloc, (z80ic_proc_t *) decln->ext, icmod);
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Allocate registers for Z80 IC module.
 *
 * @param ralloc Register allocator
 * @param vrmod Z80 IC module with virtual registers
 * @param ricmod Place to store pointer to new Z80 IC module
 * @return EOK on success or an error code
 */
int z80_ralloc_module(z80_ralloc_t *ralloc, z80ic_module_t *vrmod,
    z80ic_module_t **ricmod)
{
	z80ic_module_t *icmod;
	int rc;
	z80ic_decln_t *decln;

	rc = z80ic_module_create(&icmod);
	if (rc != EOK)
		return rc;

	decln = z80ic_module_first(vrmod);
	while (decln != NULL) {
		rc = z80_ralloc_decln(ralloc, decln, icmod);
		if (rc != EOK)
			goto error;

		decln = z80ic_module_next(decln);
	}

	*ricmod = icmod;
	return EOK;
error:
	z80ic_module_destroy(icmod);
	return rc;
}

/** Destroy register allocator.
 *
 * @param ralloc Register allocator or @c NULL
 */
void z80_ralloc_destroy(z80_ralloc_t *ralloc)
{
	if (ralloc == NULL)
		return;

	free(ralloc);
}
