/*
 * Copyright 2024 Jiri Svoboda
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
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <types/z80/stackframe.h>
#include <z80/isel.h>
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

/** Create register allocator for procedure.
 *
 * @param ralloc Register allocator
 * @param vrproc Procedure with VRs
 * @param rz80_ralloc Place to store pointer to new register allocator
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_proc_create(z80_ralloc_t *ralloc, z80ic_proc_t *vrproc,
    z80_ralloc_proc_t **rraproc)
{
	z80_ralloc_proc_t *raproc;

	raproc = calloc(1, sizeof(z80_ralloc_proc_t));
	if (raproc == NULL)
		return ENOMEM;

	raproc->ralloc = ralloc;
	raproc->vrproc = vrproc;
	*rraproc = raproc;
	return EOK;
}

/** Destroy register allocator for procedure.
 *
 * @param ralloc Register allocator for procedure or @c NULL
 */
static void z80_ralloc_proc_destroy(z80_ralloc_proc_t *raproc)
{
	if (raproc == NULL)
		return;

	free(raproc);
}

/** Get virtual register offset for register part.
 *
 * Get byte offset into virtual register (stored on stack) based on
 * which register part we are accessing.
 *
 * @param part Virtual register part
 * @return Byte offset
 */
static unsigned z80_ralloc_vroff(z80ic_vr_part_t part)
{
	switch (part) {
	case z80ic_vrp_r8:
	case z80ic_vrp_r16l:
		return 0;
	case z80ic_vrp_r16h:
		return 1;
	default:
		assert(false);
		return 0;
	}
}

/** Set up index register for accessing a range of the stack frame.
 *
 * @param idxacc Index access structure
 * @param raproc Register allocator for procedure
 * @param rel    Offset is relative to beginning or end
 * @param off    Offset
 * @param size   Size of the accessed area
 * @param lblock Labeled block to which to append the instructions
 *
 * @return EOK on success or an error code
 */
static int z80_idxacc_setup(z80_idxacc_t *idxacc, z80_ralloc_proc_t *raproc,
    z80sf_rel_t rel, long off, uint16_t size, z80ic_lblock_t *lblock)
{
	z80ic_ld_ix_nn_t *ldix = NULL;
	z80ic_add_ix_pp_t *addix = NULL;
	z80ic_push_qq_t *push = NULL;
	z80ic_pop_qq_t *pop = NULL;
	z80ic_oper_pp_t *pp = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_oper_qq_t *qq = NULL;
	int rc;
	long offsp;
	long offbp;

	/*
	 * The stack frame is pointed to as follows:
	 *   word0 <- SP (without adjustment)
	 *   ...
	 *   wordN <- IX
	 *
	 * So the difference between SP and frame pointer is
	 * spadj + sfsize - 2.
	 *
	 * Let's compute offset relative to both SP and frame pointer.
	 */
	if (rel == z80sf_begin) {
		offsp = off + raproc->spadj;
		offbp = off - (raproc->sfsize - 2);
	} else {
		offbp = off;
		offsp = off + raproc->spadj + raproc->sfsize - 2;
	}

	/* Are we within the reach of frame pointer? */
	if (offbp >= -128 && offbp + size - 1 <= 127) {
		idxacc->disp = (int16_t)offbp;
		return EOK;
	}

	/*
	 * Adjust IX. This breaks IX == fp, but we do not need to
	 * use IY (which can be used by the interrupt) or HL
	 * (which is not reserved for register allocator).
	 */

	/*
	 * We need to preserve AF because add IX, SP changes flags.
	 * To account for AF being pushed while doing add IX, SP,
	 * we need to adjust offsp.
	 */
	offsp += 2;

	/* ld IX, off@SP */

	rc = z80ic_ld_ix_nn_create(&ldix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_val((uint16_t)offsp, &imm);
	if (rc != EOK)
		goto error;

	ldix->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldix->instr);
	if (rc != EOK)
		goto error;

	ldix = NULL;

	/* push AF */

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	push->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	/* add IX, SP */

	rc = z80ic_add_ix_pp_create(&addix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_pp_create(z80ic_pp_sp, &pp);
	if (rc != EOK)
		goto error;

	addix->src = pp;
	pp = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &addix->instr);
	if (rc != EOK)
		goto error;

	addix = NULL;

	/* pop AF */

	rc = z80ic_pop_qq_create(&pop);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	pop->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &pop->instr);
	if (rc != EOK)
		goto error;

	pop = NULL;

	/* We also use disp == 0 to mark that adjustment has taken place. */
	idxacc->disp = 0;
	return EOK;
error:
	if (ldix != NULL)
		z80ic_instr_destroy(&ldix->instr);
	if (addix != NULL)
		z80ic_instr_destroy(&addix->instr);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);

	z80ic_oper_pp_destroy(pp);
	z80ic_oper_qq_destroy(qq);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Set up index register for accessing virtual register on the stack.
 *
 * Note: If part is z80ic_vrp_r16l, the higher register part must also
 * be within the reach of the index register.
 *
 * @param idxacc Index access structure
 * @param raproc Register allocator for procedure
 * @param vregno Virtual register number
 * @param part   Virtual register part
 * @param lblock Labeled block to which to append the instructions
 *
 * @return EOK on success or an error code
 */
static int z80_idxacc_setup_vr(z80_idxacc_t *idxacc, z80_ralloc_proc_t *raproc,
    unsigned vregno, z80ic_vr_part_t part, z80ic_lblock_t *lblock)
{
	unsigned vroff;
	unsigned size;
	long disp;

	vroff = z80_ralloc_vroff(part);
	size = (part == z80ic_vrp_r8) ? 1 : 2;

	/*
	 * The stack frame structure is as follows:
	 *   lvar0      <- SP
	 *   lvar1
	 *   ...
	 *   IX-3/IX-4: vr1
	 *   IX-2/IX-3: vr0
	 *   IX-0/IX-1: saved IX
	 */
	disp = -2 * (1 + (long) vregno) + vroff;

	return z80_idxacc_setup(idxacc, raproc, z80sf_end, disp, size, lblock);
}

/** Tear down index register access.
 *
 * @param idxacc Index access structure
 * @param raproc Register allocator for procedure
 * @param lblock Labeled block to which to append the instructions
 * @return EOK on success or an error code
 */
static int z80_idxacc_teardown(z80_idxacc_t *idxacc, z80_ralloc_proc_t *raproc,
    z80ic_lblock_t *lblock)
{
	z80ic_ld_ix_nn_t *ldix = NULL;
	z80ic_add_ix_pp_t *addix = NULL;
	z80ic_push_qq_t *push = NULL;
	z80ic_pop_qq_t *pop = NULL;
	z80ic_oper_pp_t *pp = NULL;
	z80ic_oper_qq_t *qq = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	long sfatsp;
	int rc;

	(void)lblock;

	/* If disp != 0, there was no IX adjustment */
	if (idxacc->disp != 0)
		return EOK;

	/* IX was adjusted, need to restore it */

	/*
	 * We need to know the current position of SP relative to
	 * the stack frame to determine stackframe@SP.
	 */
	sfatsp = raproc->spadj + raproc->sfsize - 2;

	/*
	 * We need to preserve AF because add IX, SP changes flags.
	 * To account for AF being pushed while doing add IX, SP,
	 * we need to adjust sfatsp.
	 */
	sfatsp += 2;

	/* ld IX, var@SP */

	rc = z80ic_ld_ix_nn_create(&ldix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_val((uint16_t)sfatsp, &imm);
	if (rc != EOK)
		goto error;

	ldix->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldix->instr);
	if (rc != EOK)
		goto error;

	ldix = NULL;

	/* push AF */

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	push->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	/* add IX, SP */

	rc = z80ic_add_ix_pp_create(&addix);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_pp_create(z80ic_pp_sp, &pp);
	if (rc != EOK)
		goto error;

	addix->src = pp;
	pp = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &addix->instr);
	if (rc != EOK)
		goto error;

	addix = NULL;

	/* pop AF */

	rc = z80ic_pop_qq_create(&pop);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	pop->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &pop->instr);
	if (rc != EOK)
		goto error;

	pop = NULL;

	idxacc->disp = 0;
	return EOK;
error:
	if (ldix != NULL)
		z80ic_instr_destroy(&ldix->instr);
	if (addix != NULL)
		z80ic_instr_destroy(&addix->instr);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);

	z80ic_oper_pp_destroy(pp);
	z80ic_oper_qq_destroy(qq);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Get displacement to use for index register access.
 *
 * @param idxacc Index access structure
 * @return Displacement
 */
static int8_t z80_idxacc_disp(z80_idxacc_t *idxacc)
{
	return idxacc->disp;
}

/** Get displacement to use for index register access to higher register part.
 *
 * Note that z80_idxacc_setup_vr() must have been called with
 * z80ic_vrp_r16l for this to work correctly.
 *
 * @param idxacc Index access structure
 * @return Displacement
 */
static int8_t z80_idxacc_disp_r16h(z80_idxacc_t *idxacc)
{
	return idxacc->disp + 1;
}

/** Create new local label.
 *
 * Allocate a new number for local label(s).
 *
 * @param raproc Register allocator for procedure
 * @return New label number
 */
static unsigned z80_ralloc_new_label_num(z80_ralloc_proc_t *raproc)
{
	return raproc->next_label++;
}

/** Mangle label identifier.
 *
 * @param proc VRIC procedure identifier
 * @param irident IR label identifier
 * @param rident Place to store pointer to IC label identifier
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_mangle_label_ident(const char *proc,
    const char *irident, char **rident)
{
	int rv;
	char *ident;

	assert(proc[0] == '_');
	assert(irident[0] == '%');

	rv = asprintf(&ident, "l_%s_%s", &proc[1], &irident[1]);
	if (rv < 0)
		return ENOMEM;

	*rident = ident;
	return EOK;
}

/** Create new local label.
 *
 * Create a new label (not corresponding to a label in IR or VRIC). The label
 * should use the IR label naming pattern.
 *
 * @param raproc Register allocator for procedure
 * @param pattern Label pattern (IR label format)
 * @param lblno Label number
 * @param rlabel Place to store pointer to new label
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_create_label(z80_ralloc_proc_t *raproc,
    const char *pattern, unsigned lblno, char **rlabel)
{
	char *irlabel;
	char *label;
	int rv;
	int rc;

	rv = asprintf(&irlabel, "%%%s%u", pattern, lblno);
	if (rv < 0)
		return ENOMEM;

	rc = z80_ralloc_mangle_label_ident(raproc->vrproc->ident, irlabel, &label);
	if (rc != EOK) {
		free(irlabel);
		return rc;
	}

	free(irlabel);
	*rlabel = label;
	return EOK;
}

/** Add instructions to allocate a stack frame.
 *
 * @param raproc Register allocator for procedure
 * @param nbytes Stack frame size in bytes
 * @param lblock Logical block (must be empty)
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_sfalloc(z80_ralloc_proc_t *raproc, size_t nbytes,
    z80ic_lblock_t *lblock)
{
	z80ic_push_ix_t *push = NULL;
	z80ic_ld_ix_nn_t *ldix = NULL;
	z80ic_add_ix_pp_t *addix = NULL;
	z80ic_ld_sp_ix_t *ldspix = NULL;
	z80ic_oper_pp_t *pp = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	int rc;

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
	imm = NULL;

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
	pp = NULL;

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
	imm = NULL;

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
	pp = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &addix->instr);
	if (rc != EOK)
		goto error;

	addix = NULL;

	/*
	 * Save stack frame size for later computations of frame pointer.
	 * Total stack frame size includes saved IX
	 */
	raproc->sfsize = nbytes + 2;
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

	z80ic_oper_pp_destroy(pp);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Append instructions to deallocate the stack frame.
 *
 * @param lblock Logical block
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_sffree(z80ic_lblock_t *lblock)
{
	z80ic_ld_sp_ix_t *ldspix = NULL;
	z80ic_pop_ix_t *pop = NULL;
	int rc;

	/* ld SP, IX */

	rc = z80ic_ld_sp_ix_create(&ldspix);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &ldspix->instr);
	if (rc != EOK)
		goto error;

	ldspix = NULL;

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
	if (ldspix != NULL)
		z80ic_instr_destroy(&ldspix->instr);
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);

	return rc;
}

/** Load 8-bit register from stack frame slot of particular VR.
 *
 * @param raproc Register allocator for procedure
 * @param label Label for the first instruction
 * @param vregno Virtual register number
 * @param part Part of virtual register
 * @param reg Physical register to load
 * @param lblock Labeled block to which to append the instructions
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_fill_reg(z80_ralloc_proc_t *raproc, const char *label,
    unsigned vregno, z80ic_vr_part_t part, z80ic_reg_t reg,
    z80ic_lblock_t *lblock)
{
	z80ic_ld_r_iixd_t *ld = NULL;
	z80ic_oper_reg_t *oreg = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vregno, part, lblock);
	if (rc != EOK)
		goto error;

	/* ld r, (IX+d) */

	rc = z80ic_ld_r_iixd_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_oper_reg_create(reg, &oreg);
	if (rc != EOK)
		goto error;

	ld->dest = oreg;
	oreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(oreg);

	return rc;
}

/** Load 16-bit register from stack frame slot of particular VR.
 *
 * @param raproc Register allocator for procedure
 * @param label Label for the first instruction
 * @param vregno Virtual register number
 * @param reg Physical register to load
 * @param lblock Labeled block to which to append the instructions
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_fill_r16(z80_ralloc_proc_t *raproc, const char *label,
    unsigned vregno, z80ic_r16_t reg, z80ic_lblock_t *lblock)
{
	int rc;

	rc = z80_ralloc_fill_reg(raproc, label, vregno, z80ic_vrp_r16l,
	    z80ic_r16_lo(reg), lblock);
	if (rc != EOK)
		return rc;

	rc = z80_ralloc_fill_reg(raproc, NULL, vregno, z80ic_vrp_r16h,
	    z80ic_r16_hi(reg), lblock);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Save 8-bit register to stack frame slot of particular VR.
 *
 * @param raproc Register allocator for procedure
 * @param label Label for the first instruction
 * @param reg Physical register to save
 * @param vregno Virtual register number
 * @param part Part of virtual register
 * @param lblock Labeled block to which to append the instructions
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_spill_reg(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_reg_t reg, unsigned vregno, z80ic_vr_part_t part,
    z80ic_lblock_t *lblock)
{
	z80ic_ld_iixd_r_t *ld = NULL;
	z80ic_oper_reg_t *oreg = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vregno, part, lblock);
	if (rc != EOK)
		goto error;

	/* ld (IX+d), r */

	rc = z80ic_ld_iixd_r_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_oper_reg_create(reg, &oreg);
	if (rc != EOK)
		goto error;

	ld->src = oreg;
	oreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(oreg);

	return rc;
}

/** Save 16-bit register to stack frame slot of particular VR.
 *
 * @param raproc Register allocator for procedure
 * @param label Label for the first instruction
 * @param reg Physical register to save
 * @param vregno Virtual register number
 * @param lblock Labeled block to which to append the instructions
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_ralloc_spill_r16(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_r16_t reg, unsigned vregno, z80ic_lblock_t *lblock)
{
	int rc;

	rc = z80_ralloc_spill_reg(raproc, label, z80ic_r16_lo(reg), vregno,
	    z80ic_vrp_r16l, lblock);
	if (rc != EOK)
		return rc;

	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_r16_hi(reg), vregno,
	    z80ic_vrp_r16h, lblock);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Allocate registers for Z80 load 8-bit register from 8-bit immediate
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_r_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_r_n_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_n_t *ld = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_imm8_t *imm = NULL;
	int rc;

	(void) raproc;

	/* ld r, n */

	rc = z80ic_ld_r_n_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(vrld->dest->reg, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(vrld->imm8->imm8, &imm);
	if (rc != EOK)
		goto error;

	ld->dest = reg;
	ld->imm8 = imm;
	reg = NULL;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;
	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_imm8_destroy(imm);

	return rc;
}

/** Allocate registers for Z80 add 8-bit immediate to A instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vradd Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_add_a_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_add_a_n_t *vradd, z80ic_lblock_t *lblock)
{
	z80ic_add_a_n_t *add = NULL;
	z80ic_oper_imm8_t *imm = NULL;
	int rc;

	(void) raproc;

	/* add A, n */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(vradd->imm8->imm8, &imm);
	if (rc != EOK)
		goto error;

	add->imm8 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;
	return EOK;
error:
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	z80ic_oper_imm8_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 add 8-bit immediate to A with carry instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vradd Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_adc_a_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_adc_a_n_t *vradd, z80ic_lblock_t *lblock)
{
	z80ic_adc_a_n_t *add = NULL;
	z80ic_oper_imm8_t *imm = NULL;
	int rc;

	(void) raproc;

	/* adc A, n */

	rc = z80ic_adc_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(vradd->imm8->imm8, &imm);
	if (rc != EOK)
		goto error;

	add->imm8 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;
	return EOK;
error:
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	z80ic_oper_imm8_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 subtract 8-bit immediate instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsub Subtract instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_sub_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_sub_n_t *vrsub, z80ic_lblock_t *lblock)
{
	z80ic_sub_n_t *sub = NULL;
	z80ic_oper_imm8_t *imm = NULL;
	int rc;

	(void) raproc;

	/* sub n */

	rc = z80ic_sub_n_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(vrsub->imm8->imm8, &imm);
	if (rc != EOK)
		goto error;

	sub->imm8 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;
	return EOK;
error:
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	z80ic_oper_imm8_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 bitwise AND with register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrxor AND instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_and_r(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_and_r_t *vrand, z80ic_lblock_t *lblock)
{
	z80ic_and_r_t *and = NULL;
	z80ic_oper_reg_t *reg = NULL;
	int rc;

	(void) raproc;

	/* and r */

	rc = z80ic_and_r_create(&and);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(vrand->src->reg, &reg);
	if (rc != EOK)
		goto error;

	and->src = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &and->instr);
	if (rc != EOK)
		goto error;

	and = NULL;
	return EOK;
error:
	if (and != NULL)
		z80ic_instr_destroy(&and->instr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Allocate registers for Z80 bitwise XOR with register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrxor XOR instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_xor_r(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_xor_r_t *vrxor, z80ic_lblock_t *lblock)
{
	z80ic_xor_r_t *xor = NULL;
	z80ic_oper_reg_t *reg = NULL;
	int rc;

	(void) raproc;

	/* xor r */

	rc = z80ic_xor_r_create(&xor);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(vrxor->src->reg, &reg);
	if (rc != EOK)
		goto error;

	xor->src = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &xor->instr);
	if (rc != EOK)
		goto error;

	xor = NULL;
	return EOK;
error:
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Allocate registers for Z80 compare with 8-bit immediate instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrcp Compare instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_cp_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_cp_n_t *vrcp, z80ic_lblock_t *lblock)
{
	z80ic_cp_n_t *cp = NULL;
	z80ic_oper_imm8_t *imm = NULL;
	int rc;

	(void) raproc;

	/* cp n */

	rc = z80ic_cp_n_create(&cp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(vrcp->imm8->imm8, &imm);
	if (rc != EOK)
		goto error;

	cp->imm8 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &cp->instr);
	if (rc != EOK)
		goto error;

	cp = NULL;
	return EOK;
error:
	if (cp != NULL)
		z80ic_instr_destroy(&cp->instr);
	z80ic_oper_imm8_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 decrement register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrdec Decrement instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_dec_r(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_dec_r_t *vrdec, z80ic_lblock_t *lblock)
{
	z80ic_dec_r_t *dec = NULL;
	z80ic_oper_reg_t *reg = NULL;
	int rc;

	(void) raproc;

	/* dec r */

	rc = z80ic_dec_r_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(vrdec->dest->reg, &reg);
	if (rc != EOK)
		goto error;

	dec->dest = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &dec->instr);
	if (rc != EOK)
		goto error;

	dec = NULL;
	return EOK;
error:
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Allocate registers for Z80 complement instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrcpl Complement instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_cpl(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_cpl_t *vrcpl, z80ic_lblock_t *lblock)
{
	z80ic_cpl_t *cpl = NULL;
	int rc;

	(void) raproc;
	(void) vrcpl;

	rc = z80ic_cpl_create(&cpl);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &cpl->instr);
	if (rc != EOK)
		goto error;

	cpl = NULL;
	return EOK;
error:
	if (cpl != NULL)
		z80ic_instr_destroy(&cpl->instr);
	return rc;
}

/** Allocate registers for Z80 no operation instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrnop No operation instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_nop(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_nop_t *vrnop, z80ic_lblock_t *lblock)
{
	z80ic_nop_t *nop = NULL;
	int rc;

	(void) raproc;
	(void) vrnop;

	rc = z80ic_nop_create(&nop);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &nop->instr);
	if (rc != EOK)
		goto error;

	nop = NULL;
	return EOK;
error:
	if (nop != NULL)
		z80ic_instr_destroy(&nop->instr);
	return rc;
}

/** Allocate registers for Z80 increment register pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrinc Increment instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_inc_ss(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_inc_ss_t *vrinc, z80ic_lblock_t *lblock)
{
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_ss_t *ss = NULL;
	int rc;

	(void) raproc;
	(void) vrinc;

	rc = z80ic_inc_ss_create(&inc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(vrinc->dest->rss, &ss);
	if (rc != EOK)
		goto error;

	inc->dest = ss;
	ss = NULL;

	rc = z80ic_lblock_append(lblock, label, &inc->instr);
	if (rc != EOK)
		goto error;

	inc = NULL;

	if (vrinc->dest->rss == z80ic_ss_sp)
		raproc->spadj -= 1;
	return EOK;
error:
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	z80ic_oper_ss_destroy(ss);
	return rc;
}

/** Allocate registers for Z80 rotate left accumulator instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrret Rotate left accumulator instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_rla(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_rla_t *vrrla, z80ic_lblock_t *lblock)
{
	z80ic_rla_t *rla = NULL;
	int rc;

	(void) raproc;
	(void) vrrla;

	rc = z80ic_rla_create(&rla);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &rla->instr);
	if (rc != EOK)
		goto error;

	rla = NULL;
	return EOK;
error:
	if (rla != NULL)
		z80ic_instr_destroy(&rla->instr);
	return rc;
}

/** Allocate registers for Z80 jump instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrjp Jump instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_jp_nn(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_jp_nn_t *vrjp, z80ic_lblock_t *lblock)
{
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	int rc;

	(void) raproc;

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_copy(vrjp->imm16, &imm);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;
	return EOK;
error:
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_imm16_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 conditional jump instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrjp Conditional jump instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_jp_cc_nn(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_jp_cc_nn_t *vrjp, z80ic_lblock_t *lblock)
{
	z80ic_jp_cc_nn_t *jp = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	int rc;

	(void) raproc;

	rc = z80ic_jp_cc_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_copy(vrjp->imm16, &imm);
	if (rc != EOK)
		goto error;

	jp->cc = vrjp->cc;
	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;
	return EOK;
error:
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);
	z80ic_oper_imm16_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 call instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrcall Call instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_call_nn(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_call_nn_t *vrcall, z80ic_lblock_t *lblock)
{
	z80ic_call_nn_t *call = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	int rc;

	(void) raproc;

	rc = z80ic_call_nn_create(&call);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_copy(vrcall->imm16, &imm);
	if (rc != EOK)
		goto error;

	call->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &call->instr);
	if (rc != EOK)
		goto error;

	call = NULL;
	return EOK;
error:
	if (call != NULL)
		z80ic_instr_destroy(&call->instr);
	z80ic_oper_imm16_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 return instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrret Return instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ret(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ret_t *vrret, z80ic_lblock_t *lblock)
{
	z80ic_ret_t *ret = NULL;
	bool is_calli;
	int rc;

	(void) vrret;

	/* Is this really an indirect call? */
	is_calli = raproc->spadj > 0;

	if (!is_calli) {
		/* Insert epilogue to free the stack frame */
		rc = z80_ralloc_sffree(lblock);
		if (rc != EOK)
			goto error;
	}

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;

	if (is_calli) {
		/*
		 * This ret is used for an indirect call. It removes
		 * the top two words from the stack before returning.
		 */
		assert(raproc->spadj >= 4);
		raproc->spadj -= 4;
	}

	return EOK;
error:
	if (ret != NULL)
		z80ic_instr_destroy(&ret->instr);
	return rc;
}

/** Allocate registers for Z80 load register from indirect virtual register
 * pair with displacement instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_r_ivrrd(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_r_ivrrd_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_ihl_t *ld = NULL;
	z80ic_ld_r_r_t *ldrr = NULL;
	z80ic_add_a_n_t *add = NULL;
	z80ic_adc_a_n_t *adc = NULL;
	z80ic_oper_reg_t *dreg = NULL;
	z80ic_oper_reg_t *sreg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	uint16_t dispw;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->isrc->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Sign-extend to 16 bits */
	dispw = (int16_t)vrld->disp;

	/* ld A, L */
	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* add A, LO(dispw) */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* ld L, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, H */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* adc A, HI(dispw) */

	rc = z80ic_adc_a_n_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adc->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* ld H, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld r, (HL) */

	rc = z80ic_ld_r_ihl_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(vrld->dest->reg, &dreg);
	if (rc != EOK)
		goto error;

	ld->dest = dreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ldrr != NULL)
		z80ic_instr_destroy(&ldrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	z80ic_oper_reg_destroy(dreg);
	z80ic_oper_reg_destroy(sreg);
	z80ic_oper_imm8_destroy(imm8);
	return rc;
}

/** Allocate registers for Z80 load virtual register from virtual register
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vr_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vr_vr_t *vrld, z80ic_lblock_t *lblock)
{
	int rc;

	/* Fill A */
	rc = z80_ralloc_fill_reg(raproc, label, vrld->src->vregno,
	    vrld->src->part, z80ic_reg_a, lblock);
	if (rc != EOK)
		goto error;

	/* Spill A */
	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_reg_a,
	    vrld->dest->vregno, vrld->dest->part, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Allocate registers for Z80 load virtual register from 8-bit immediate
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vr_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vr_n_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_iixd_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrld->dest->vregno,
	    vrld->dest->part, lblock);
	if (rc != EOK)
		goto error;

	/* ld (IX+d), n */

	rc = z80ic_ld_iixd_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_oper_imm8_create(vrld->imm8->imm8, &imm);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_imm8_destroy(imm);

	return rc;
}

/** Allocate registers for Z80 load virtual register from (HL) instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vr_ihl(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vr_ihl_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_ihl_t *ld = NULL;
	z80ic_oper_reg_t *reg = NULL;
	int rc;

	/* ld A, (HL) */

	rc = z80ic_ld_r_ihl_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	ld->dest = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Spill A */
	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_reg_a,
	    vrld->dest->vregno, vrld->dest->part, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(reg);
	return rc;
}

/** Allocate registers for Z80 load virtual register from (IX+d) instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vr_iixd(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vr_iixd_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_iixd_t *ld = NULL;
	z80ic_oper_reg_t *oreg = NULL;
	int rc;

	/* ld A, (IX+d) */

	rc = z80ic_ld_r_iixd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &oreg);
	if (rc != EOK)
		goto error;

	ld->dest = oreg;
	oreg = NULL;
	ld->disp = vrld->disp;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Spill A to vr */
	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_reg_a,
	    vrld->dest->vregno, z80ic_vrp_r8, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(oreg);
	return rc;
}

/** Allocate registers for Z80 load virtual register from indirect virtual
 * register pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vr_ivrr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vr_ivrr_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_ihl_t *ld = NULL;
	z80ic_oper_reg_t *reg = NULL;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->isrc->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* ld A, (HL) */

	rc = z80ic_ld_r_ihl_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	ld->dest = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Spill A to vr */
	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_reg_a,
	    vrld->dest->vregno, z80ic_vrp_r8, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(reg);
	return rc;
}

/** Allocate registers for Z80 load virtual register from indirect virtual
 * register pair with displacement instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vr_ivrrd(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vr_ivrrd_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_ihl_t *ld = NULL;
	z80ic_ld_r_r_t *ldrr = NULL;
	z80ic_add_a_n_t *add = NULL;
	z80ic_adc_a_n_t *adc = NULL;
	z80ic_oper_reg_t *dreg = NULL;
	z80ic_oper_reg_t *sreg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	uint16_t dispw;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->isrc->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Sign-extend to 16 bits */
	dispw = (int16_t)vrld->disp;

	/* ld A, L */
	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* add A, LO(dispw) */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* ld L, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, H */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* adc A, HI(dispw) */

	rc = z80ic_adc_a_n_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adc->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* ld H, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, (HL) */

	rc = z80ic_ld_r_ihl_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	ld->dest = dreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Spill A to vr */
	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_reg_a,
	    vrld->dest->vregno, vrld->dest->part, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ldrr != NULL)
		z80ic_instr_destroy(&ldrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	z80ic_oper_reg_destroy(dreg);
	z80ic_oper_reg_destroy(sreg);
	z80ic_oper_imm8_destroy(imm8);
	return rc;
}

/** Allocate registers for Z80 load (HL) from virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_ihl_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_ihl_vr_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_ihl_r_t *ld = NULL;
	z80ic_oper_reg_t *reg = NULL;
	int rc;

	/* Fill A */
	rc = z80_ralloc_fill_reg(raproc, label, vrld->src->vregno,
	    vrld->src->part, z80ic_reg_a, lblock);
	if (rc != EOK)
		goto error;

	/* ld (HL), A */

	rc = z80ic_ld_ihl_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	ld->src = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(reg);
	return rc;
}

/** Allocate registers for Z80 load indirect virtual register pair from virtual
 * register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_ivrr_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_ivrr_vr_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_ihl_r_t *ld = NULL;
	z80ic_oper_reg_t *reg = NULL;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->idest->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Fill A from vr */
	rc = z80_ralloc_fill_reg(raproc, NULL, vrld->src->vregno,
	    vrld->src->part, z80ic_reg_a, lblock);
	if (rc != EOK)
		goto error;

	/* ld (HL), A */

	rc = z80ic_ld_ihl_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	ld->src = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(reg);
	return rc;
}

/** Allocate registers for Z80 load indirect virtual register pair with
 * displacement from register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_ivrrd_r(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_ivrrd_r_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_push_qq_t *push = NULL;
	z80ic_pop_qq_t *pop = NULL;
	z80ic_ld_ihl_r_t *ld = NULL;
	z80ic_ld_r_r_t *ldrr = NULL;
	z80ic_add_a_n_t *add = NULL;
	z80ic_adc_a_n_t *adc = NULL;
	z80ic_oper_reg_t *dreg = NULL;
	z80ic_oper_reg_t *sreg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_qq_t *qq = NULL;
	uint16_t dispw;
	int rc;

	/* push AF */

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	push->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->idest->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Sign-extend to 16 bits */
	dispw = (int16_t)vrld->disp;

	/* ld A, L */
	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* add A, LO(dispw) */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* ld L, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, H */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* adc A, HI(dispw) */

	rc = z80ic_adc_a_n_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adc->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* ld H, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* pop AF */

	rc = z80ic_pop_qq_create(&pop);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	pop->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &pop->instr);
	if (rc != EOK)
		goto error;

	pop = NULL;

	/* ld (HL), r */

	rc = z80ic_ld_ihl_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(vrld->src->reg, &sreg);
	if (rc != EOK)
		goto error;

	ld->src = sreg;
	sreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ldrr != NULL)
		z80ic_instr_destroy(&ldrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);
	z80ic_oper_reg_destroy(dreg);
	z80ic_oper_reg_destroy(sreg);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_qq_destroy(qq);
	return rc;
}

/** Allocate registers for Z80 load indirect virtual register pair with
 * displacement from virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_ivrrd_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_ivrrd_vr_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_ihl_r_t *ld = NULL;
	z80ic_ld_r_r_t *ldrr = NULL;
	z80ic_add_a_n_t *add = NULL;
	z80ic_adc_a_n_t *adc = NULL;
	z80ic_oper_reg_t *dreg = NULL;
	z80ic_oper_reg_t *sreg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	uint16_t dispw;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->idest->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Sign-extend to 16 bits */
	dispw = (int16_t)vrld->disp;

	/* ld A, L */
	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* add A, LO(dispw) */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* ld L, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, H */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* adc A, HI(dispw) */

	rc = z80ic_adc_a_n_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adc->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* ld H, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* Fill A from vr */
	rc = z80_ralloc_fill_reg(raproc, NULL, vrld->src->vregno,
	    vrld->src->part, z80ic_reg_a, lblock);
	if (rc != EOK)
		goto error;

	/* ld (HL), A */

	rc = z80ic_ld_ihl_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ld->src = sreg;
	sreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ldrr != NULL)
		z80ic_instr_destroy(&ldrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	z80ic_oper_reg_destroy(dreg);
	z80ic_oper_reg_destroy(sreg);
	z80ic_oper_imm8_destroy(imm8);
	return rc;
}

/** Allocate registers for Z80 load indirect virtual register pair from
 * 8-bit immediate instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_ivrr_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_ivrr_n_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_ihl_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm = NULL;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->idest->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* ld (HL), n */

	rc = z80ic_ld_ihl_n_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(vrld->imm8->imm8, &imm);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_imm8_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 load indirect virtual register pair with
 * displacement from 8-bit immediate instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_ivrrd_n(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_ivrrd_n_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_ihl_n_t *ld = NULL;
	z80ic_ld_r_r_t *ldrr = NULL;
	z80ic_add_a_n_t *add = NULL;
	z80ic_adc_a_n_t *adc = NULL;
	z80ic_oper_reg_t *dreg = NULL;
	z80ic_oper_reg_t *sreg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	uint16_t dispw;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->idest->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Sign-extend to 16 bits */
	dispw = (int16_t)vrld->disp;

	/* ld A, L */
	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* add A, LO(dispw) */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* ld L, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, H */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* adc A, HI(dispw) */

	rc = z80ic_adc_a_n_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adc->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* ld H, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld (HL), n */

	rc = z80ic_ld_ihl_n_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(vrld->imm8->imm8, &imm8);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ldrr != NULL)
		z80ic_instr_destroy(&ldrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	z80ic_oper_reg_destroy(dreg);
	z80ic_oper_reg_destroy(sreg);
	z80ic_oper_imm8_destroy(imm8);
	return rc;
}

/** Allocate registers for Z80 load virtual register pair from virtual register
 * pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vrr_vrr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vrr_vrr_t *vrld, z80ic_lblock_t *lblock)
{
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->src->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Spill HL */
	rc = z80_ralloc_spill_r16(raproc, NULL, z80ic_r16_hl,
	    vrld->dest->vregno, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Allocate registers for Z80 load 8-bit register from virtual register
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_r_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_r_vr_t *vrld, z80ic_lblock_t *lblock)
{
	int rc;

	/* Fill 8-bit register */
	rc = z80_ralloc_fill_reg(raproc, label, vrld->src->vregno,
	    vrld->src->part, vrld->dest->reg, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Allocate registers for Z80 virtual register from 8-bit register
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vr_r(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vr_r_t *vrld, z80ic_lblock_t *lblock)
{
	int rc;

	/* Spill 8-bit register */
	rc = z80_ralloc_spill_reg(raproc, label, vrld->src->reg,
	    vrld->dest->vregno, vrld->dest->part, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Allocate registers for Z80 load 16-bit register from virtual register
 * pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_r16_vrr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_r16_vrr_t *vrld, z80ic_lblock_t *lblock)
{
	int rc;

	/* Fill 16-bit register */
	rc = z80_ralloc_fill_r16(raproc, label, vrld->src->vregno,
	    vrld->dest->r16, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Allocate registers for Z80 load virtual register pair from 16-bit register
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vrr_r16(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vrr_r16_t *vrld, z80ic_lblock_t *lblock)
{
	int rc;

	/* Spill 16-bit register */
	rc = z80_ralloc_spill_r16(raproc, label, vrld->src->r16,
	    vrld->dest->vregno, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Allocate registers for Z80 load virtual register pair from (IX+d)
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vrr_iixd(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vrr_iixd_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_iixd_t *ld = NULL;
	z80ic_oper_reg_t *oreg = NULL;
	int rc;

	/* ld A, (IX+d) */

	rc = z80ic_ld_r_iixd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &oreg);
	if (rc != EOK)
		goto error;

	ld->dest = oreg;
	oreg = NULL;
	ld->disp = vrld->disp;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Spill A to vrr.L */
	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_reg_a,
	    vrld->dest->vregno, z80ic_vrp_r16l, lblock);
	if (rc != EOK)
		goto error;

	/* ld A, (IX+d+1) */

	rc = z80ic_ld_r_iixd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &oreg);
	if (rc != EOK)
		goto error;

	ld->dest = oreg;
	oreg = NULL;
	ld->disp = vrld->disp + 1;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Spill A to vrr.H */
	rc = z80_ralloc_spill_reg(raproc, NULL, z80ic_reg_a,
	    vrld->dest->vregno, z80ic_vrp_r16h, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(oreg);
	return rc;
}

/** Allocate registers for Z80 load virtual register pair from 16-bit
 * immediate instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vrr_nn(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vrr_nn_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_dd_nn_t *ldnn = NULL;
	z80ic_oper_dd_t *dd = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	int rc;

	/* ld HL, nn */

	rc = z80ic_ld_dd_nn_create(&ldnn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(z80ic_dd_hl, &dd);
	if (rc != EOK)
		goto error;

	ldnn->dest = dd;
	dd = NULL;

	rc = z80ic_oper_imm16_copy(vrld->imm16, &imm);
	if (rc != EOK)
		goto error;

	ldnn->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldnn->instr);
	if (rc != EOK)
		goto error;

	/* Spill HL */
	rc = z80_ralloc_spill_r16(raproc, NULL, z80ic_r16_hl,
	    vrld->dest->vregno, lblock);
	if (rc != EOK)
		goto error;

	ldnn = NULL;
	return EOK;
error:
	if (ldnn != NULL)
		z80ic_instr_destroy(&ldnn->instr);
	z80ic_oper_dd_destroy(dd);
	z80ic_oper_imm16_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 load virtual register pair from Stack Frame
 * Begin with 16-bit displacement instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vrr_sfbnn(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vrr_sfbnn_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_dd_nn_t *ldnn = NULL;
	z80ic_add_hl_ss_t *add = NULL;
	z80ic_oper_dd_t *dd = NULL;
	z80ic_oper_ss_t *ss = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	int rc;

	/*
	 * SFB (Stack Frame Begin) == SP + raproc->spadj
	 * SFB+nn = SP + (nn+raproc->spadj)
	 */

	/* ld HL, nn */

	rc = z80ic_ld_dd_nn_create(&ldnn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(z80ic_dd_hl, &dd);
	if (rc != EOK)
		goto error;

	ldnn->dest = dd;
	dd = NULL;

	rc = z80ic_oper_imm16_copy(vrld->imm16, &imm);
	if (rc != EOK)
		goto error;

	/* Add stack adjustment */
	imm->imm16 += raproc->spadj;

	ldnn->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldnn->instr);
	if (rc != EOK)
		goto error;

	/* add HL, SP */

	rc = z80ic_add_hl_ss_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(z80ic_ss_sp, &ss);
	if (rc != EOK)
		goto error;

	add->src = ss;
	ss = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* Spill HL */
	rc = z80_ralloc_spill_r16(raproc, NULL, z80ic_r16_hl,
	    vrld->dest->vregno, lblock);
	if (rc != EOK)
		goto error;

	ldnn = NULL;
	return EOK;
error:
	if (ldnn != NULL)
		z80ic_instr_destroy(&ldnn->instr);
	z80ic_oper_dd_destroy(dd);
	z80ic_oper_imm16_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 load virtual register pair from Stack Frame
 * End with 16-bit displacement instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_vrr_sfenn(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_vrr_sfenn_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_dd_nn_t *ldnn = NULL;
	z80ic_add_hl_ss_t *add = NULL;
	z80ic_oper_dd_t *dd = NULL;
	z80ic_oper_ss_t *ss = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	int rc;

	/*
	 * SFE (Stack Frame End) == SP + raproc->spadj + raproc->sfsize - 2
	 * SFE+nn = SP + (nn+raproc->spadj + raproc->sfsize - 2)
	 */

	/* ld HL, nn */

	rc = z80ic_ld_dd_nn_create(&ldnn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(z80ic_dd_hl, &dd);
	if (rc != EOK)
		goto error;

	ldnn->dest = dd;
	dd = NULL;

	rc = z80ic_oper_imm16_copy(vrld->imm16, &imm);
	if (rc != EOK)
		goto error;

	/* Add stack adjustment and stack frame size */
	imm->imm16 += raproc->spadj + raproc->sfsize - 2;

	ldnn->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldnn->instr);
	if (rc != EOK)
		goto error;

	/* add HL, SP */

	rc = z80ic_add_hl_ss_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(z80ic_ss_sp, &ss);
	if (rc != EOK)
		goto error;

	add->src = ss;
	ss = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* Spill HL */
	rc = z80_ralloc_spill_r16(raproc, NULL, z80ic_r16_hl,
	    vrld->dest->vregno, lblock);
	if (rc != EOK)
		goto error;

	ldnn = NULL;
	return EOK;
error:
	if (ldnn != NULL)
		z80ic_instr_destroy(&ldnn->instr);
	z80ic_oper_dd_destroy(dd);
	z80ic_oper_imm16_destroy(imm);
	return rc;
}

/** Allocate registers for Z80 load (SFT+imm16) from register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrld Load instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_ld_isfbnn_r(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_ld_isfbnn_r_t *vrld, z80ic_lblock_t *lblock)
{
	z80ic_ld_iixd_r_t *ld = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_imm16_t *imm;
	z80_idxacc_t idxacc;
	int rc;

	imm = vrld->imm16;
	assert(imm->symbol == NULL);

	/* Set up index register */
	rc = z80_idxacc_setup(&idxacc, raproc, z80sf_begin,
	    imm->imm16, 1, lblock);
	if (rc != EOK)
		goto error;

	/* ld (IX+d), r */

	rc = z80ic_ld_iixd_r_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_oper_reg_create(vrld->src->reg, &reg);
	if (rc != EOK)
		goto error;

	ld->src = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Allocate registers for Z80 push virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrpush Push instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_push_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_push_vr_t *vrpush, z80ic_lblock_t *lblock)
{
	z80ic_push_qq_t *push = NULL;
	z80ic_oper_qq_t *qq = NULL;
	int rc;

	/* Fill L */
	rc = z80_ralloc_fill_reg(raproc, label, vrpush->src->vregno,
	    z80ic_vrp_r8, z80ic_reg_l, lblock);
	if (rc != EOK)
		goto error;

	/* push HL */

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_hl, &qq);
	if (rc != EOK)
		goto error;

	push->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, label, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	raproc->spadj += 2;
	return EOK;
error:
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	z80ic_oper_qq_destroy(qq);
	return rc;
}

/** Allocate registers for Z80 push virtual register pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrpush Push instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_push_vrr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_push_vrr_t *vrpush, z80ic_lblock_t *lblock)
{
	z80ic_push_qq_t *push = NULL;
	z80ic_oper_qq_t *qq = NULL;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrpush->src->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* push HL */

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_hl, &qq);
	if (rc != EOK)
		goto error;

	push->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, label, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	raproc->spadj += 2;
	return EOK;
error:
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	z80ic_oper_qq_destroy(qq);
	return rc;
}

/** Allocate registers for Z80 add virtual register to A instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsub Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_add_a_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_add_a_vr_t *vradd, z80ic_lblock_t *lblock)
{
	z80ic_add_a_iixd_t *add = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vradd->src->vregno,
	    vradd->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* add A, (IX+d) */

	rc = z80ic_add_a_iixd_create(&add);
	if (rc != EOK)
		goto error;

	add->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);

	return rc;
}

/** Allocate registers for Z80 add indirect virtual register pair with
 * displacement to A instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vradd Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_add_a_ivrrd(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_add_a_ivrrd_t *vradd, z80ic_lblock_t *lblock)
{
	z80ic_push_qq_t *push = NULL;
	z80ic_pop_qq_t *pop = NULL;
	z80ic_add_a_ihl_t *addihl = NULL;
	z80ic_ld_r_r_t *ldrr = NULL;
	z80ic_add_a_n_t *add = NULL;
	z80ic_adc_a_n_t *adc = NULL;
	z80ic_oper_reg_t *dreg = NULL;
	z80ic_oper_reg_t *sreg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_qq_t *qq = NULL;
	uint16_t dispw;
	int rc;

	/* push AF */

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	push->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vradd->isrc->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Sign-extend to 16 bits */
	dispw = (int16_t)vradd->disp;

	/* ld A, L */
	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* add A, LO(dispw) */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* ld L, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, H */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* adc A, HI(dispw) */

	rc = z80ic_adc_a_n_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adc->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* ld H, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* pop AF */

	rc = z80ic_pop_qq_create(&pop);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	pop->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &pop->instr);
	if (rc != EOK)
		goto error;

	pop = NULL;

	/* add A, (HL) */

	rc = z80ic_add_a_ihl_create(&addihl);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &addihl->instr);
	if (rc != EOK)
		goto error;

	addihl = NULL;

	return EOK;
error:
	if (addihl != NULL)
		z80ic_instr_destroy(&addihl->instr);
	if (ldrr != NULL)
		z80ic_instr_destroy(&ldrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);
	z80ic_oper_reg_destroy(dreg);
	z80ic_oper_reg_destroy(sreg);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_qq_destroy(qq);
	return rc;
}

/** Allocate registers for Z80 add virtual register to A with carry instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsub Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_adc_a_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_adc_a_vr_t *vradc, z80ic_lblock_t *lblock)
{
	z80ic_adc_a_iixd_t *adc = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vradc->src->vregno,
	    vradc->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* adc A, (IX+d) */

	rc = z80ic_adc_a_iixd_create(&adc);
	if (rc != EOK)
		goto error;

	adc->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);

	return rc;
}

/** Allocate registers for Z80 add indirect virtual register pair with
 * displacement to a with carry instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vradc Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_adc_a_ivrrd(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_adc_a_ivrrd_t *vradc, z80ic_lblock_t *lblock)
{
	z80ic_push_qq_t *push = NULL;
	z80ic_pop_qq_t *pop = NULL;
	z80ic_adc_a_ihl_t *adcihl = NULL;
	z80ic_ld_r_r_t *ldrr = NULL;
	z80ic_add_a_n_t *add = NULL;
	z80ic_adc_a_n_t *adc = NULL;
	z80ic_oper_reg_t *dreg = NULL;
	z80ic_oper_reg_t *sreg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_qq_t *qq = NULL;
	uint16_t dispw;
	int rc;

	/* push AF */

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	push->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &push->instr);
	if (rc != EOK)
		goto error;

	push = NULL;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vradc->isrc->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Sign-extend to 16 bits */
	dispw = (int16_t)vradc->disp;

	/* ld A, L */
	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* add A, LO(dispw) */

	rc = z80ic_add_a_n_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	add->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	add = NULL;

	/* ld L, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_l, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* ld A, H */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* adc A, HI(dispw) */

	rc = z80ic_adc_a_n_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(dispw >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adc->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, label, &adc->instr);
	if (rc != EOK)
		goto error;

	adc = NULL;

	/* ld H, A */

	rc = z80ic_ld_r_r_create(&ldrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_h, &dreg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &sreg);
	if (rc != EOK)
		goto error;

	ldrr->dest = dreg;
	ldrr->src = sreg;
	dreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrr->instr);
	if (rc != EOK)
		goto error;

	ldrr = NULL;

	/* pop AF */

	rc = z80ic_pop_qq_create(&pop);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(z80ic_qq_af, &qq);
	if (rc != EOK)
		goto error;

	pop->src = qq;
	qq = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &pop->instr);
	if (rc != EOK)
		goto error;

	pop = NULL;

	/* adc A, (HL) */

	rc = z80ic_adc_a_ihl_create(&adcihl);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &adcihl->instr);
	if (rc != EOK)
		goto error;

	adcihl = NULL;

	return EOK;
error:
	if (adcihl != NULL)
		z80ic_instr_destroy(&adcihl->instr);
	if (ldrr != NULL)
		z80ic_instr_destroy(&ldrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	if (pop != NULL)
		z80ic_instr_destroy(&pop->instr);
	z80ic_oper_reg_destroy(dreg);
	z80ic_oper_reg_destroy(sreg);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_qq_destroy(qq);
	return rc;
}

/** Allocate registers for Z80 subtract virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsub Subtract instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_sub_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_sub_vr_t *vrsub, z80ic_lblock_t *lblock)
{
	z80ic_sub_iixd_t *sub = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrsub->src->vregno,
	    vrsub->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* sub (IX+d) */

	rc = z80ic_sub_iixd_create(&sub);
	if (rc != EOK)
		goto error;

	sub->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);

	return rc;
}

/** Allocate registers for Z80 subtract virtual register from A with carry
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsbc Subtract with carry instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_sbc_a_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_sbc_a_vr_t *vrsbc, z80ic_lblock_t *lblock)
{
	z80ic_sbc_a_iixd_t *sbc = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrsbc->src->vregno,
	    vrsbc->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* sbc A, (IX+d) */

	rc = z80ic_sbc_a_iixd_create(&sbc);
	if (rc != EOK)
		goto error;

	sbc->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &sbc->instr);
	if (rc != EOK)
		goto error;

	sbc = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);

	return rc;
}

/** Allocate registers for Z80 bitwise AND with virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrand AND instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_and_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_and_vr_t *vrand, z80ic_lblock_t *lblock)
{
	z80ic_and_iixd_t *and = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrand->src->vregno,
	    vrand->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* and (IX+d) */

	rc = z80ic_and_iixd_create(&and);
	if (rc != EOK)
		goto error;

	and->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &and->instr);
	if (rc != EOK)
		goto error;

	and = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (and != NULL)
		z80ic_instr_destroy(&and->instr);

	return rc;
}

/** Allocate registers for Z80 bitwise OR with virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vror OR instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_or_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_or_vr_t *vror, z80ic_lblock_t *lblock)
{
	z80ic_or_iixd_t *or = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vror->src->vregno,
	    vror->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* or (IX+d) */

	rc = z80ic_or_iixd_create(&or);
	if (rc != EOK)
		goto error;

	or->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &or->instr);
	if (rc != EOK)
		goto error;

	or = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (or != NULL)
		z80ic_instr_destroy(&or->instr);

	return rc;
}

/** Allocate registers for Z80 bitwise XOR with virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrxor XOR instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_xor_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_xor_vr_t *vrxor, z80ic_lblock_t *lblock)
{
	z80ic_xor_iixd_t *xor = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrxor->src->vregno,
	    vrxor->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* xor (IX+d) */

	rc = z80ic_xor_iixd_create(&xor);
	if (rc != EOK)
		goto error;

	xor->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &xor->instr);
	if (rc != EOK)
		goto error;

	xor = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);

	return rc;
}

/** Allocate registers for Z80 increment virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrdec Increment instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_inc_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_inc_vr_t *vrinc, z80ic_lblock_t *lblock)
{
	z80ic_inc_iixd_t *inc = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrinc->vr->vregno,
	    vrinc->vr->part, lblock);
	if (rc != EOK)
		goto error;

	/* inc (IX+d) */

	rc = z80ic_inc_iixd_create(&inc);
	if (rc != EOK)
		goto error;

	inc->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &inc->instr);
	if (rc != EOK)
		goto error;

	inc = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);

	return rc;
}

/** Allocate registers for Z80 decrement virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrdec Decrement instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_dec_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_dec_vr_t *vrdec, z80ic_lblock_t *lblock)
{
	z80ic_dec_iixd_t *dec = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrdec->vr->vregno,
	    vrdec->vr->part, lblock);
	if (rc != EOK)
		goto error;

	/* dec (IX+d) */

	rc = z80ic_dec_iixd_create(&dec);
	if (rc != EOK)
		goto error;

	dec->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &dec->instr);
	if (rc != EOK)
		goto error;

	dec = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);

	return rc;
}

/** Allocate registers for Z80 add virtual register pair to virtual register
 * pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vradd Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_add_vrr_vrr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_add_vrr_vrr_t *vradd, z80ic_lblock_t *lblock)
{
	z80ic_add_hl_ss_t *add = NULL;
	z80ic_oper_ss_t *ss = NULL;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vradd->dest->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Fill BC */
	rc = z80_ralloc_fill_r16(raproc, label, vradd->src->vregno,
	    z80ic_r16_bc, lblock);
	if (rc != EOK)
		goto error;

	/* add HL, BC */

	rc = z80ic_add_hl_ss_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(z80ic_ss_bc, &ss);
	if (rc != EOK)
		goto error;

	add->src = ss;
	ss = NULL;

	rc = z80ic_lblock_append(lblock, label, &add->instr);
	if (rc != EOK)
		goto error;

	/* Spill HL */
	rc = z80_ralloc_spill_r16(raproc, NULL, z80ic_r16_hl,
	    vradd->dest->vregno, lblock);
	if (rc != EOK)
		goto error;

	add = NULL;
	return EOK;
error:
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	z80ic_oper_ss_destroy(ss);
	return rc;
}

/** Allocate registers for Z80 subtract virtual register pair from virtual
 * register pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vradd Add instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_sub_vrr_vrr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_sub_vrr_vrr_t *vrsub, z80ic_lblock_t *lblock)
{
	z80ic_sbc_hl_ss_t *sbc = NULL;
	z80ic_oper_ss_t *ss = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_and_r_t *anda = NULL;
	int rc;

	/* Fill HL */
	rc = z80_ralloc_fill_r16(raproc, label, vrsub->dest->vregno,
	    z80ic_r16_hl, lblock);
	if (rc != EOK)
		goto error;

	/* Fill BC */
	rc = z80_ralloc_fill_r16(raproc, label, vrsub->src->vregno,
	    z80ic_r16_bc, lblock);
	if (rc != EOK)
		goto error;

	/* and A */

	rc = z80ic_and_r_create(&anda);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	anda->src = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, label, &anda->instr);
	if (rc != EOK)
		goto error;

	anda = NULL;

	/*
	 * sbc HL, BC
	 *
	 * This instruction is pretty slow (15 T states, 2 bytes) plus
	 * and instruction (4 T states, 2 bytes). It seems it would be
	 * more efficient to actually implement this as 8-bit sub +
	 * 8-bit sbc.
	 */

	rc = z80ic_sbc_hl_ss_create(&sbc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(z80ic_ss_bc, &ss);
	if (rc != EOK)
		goto error;

	sbc->src = ss;
	ss = NULL;

	rc = z80ic_lblock_append(lblock, label, &sbc->instr);
	if (rc != EOK)
		goto error;

	sbc = NULL;

	/* Spill HL */
	rc = z80_ralloc_spill_r16(raproc, NULL, z80ic_r16_hl,
	    vrsub->dest->vregno, lblock);
	if (rc != EOK)
		goto error;

	sbc = NULL;
	return EOK;
error:
	if (anda != NULL)
		z80ic_instr_destroy(&anda->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	z80ic_oper_ss_destroy(ss);
	z80ic_oper_reg_destroy(reg);
	return rc;
}

/** Allocate registers for Z80 increment virtual register pair instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrinc Increment instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_inc_vrr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_inc_vrr_t *vrinc, z80ic_lblock_t *lblock)
{
	z80ic_inc_iixd_t *inc = NULL;
	z80ic_jp_cc_nn_t *jp = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80_idxacc_t idxacc;
	unsigned lblno;
	char *nocarry_lbl = NULL;
	int rc;

	lblno = z80_ralloc_new_label_num(raproc);

	rc = z80_ralloc_create_label(raproc, "inc16_nocarry", lblno,
	    &nocarry_lbl);
	if (rc != EOK)
		goto error;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrinc->vrr->vregno,
	    z80ic_vrp_r16l, lblock);
	if (rc != EOK)
		goto error;

	/* inc (IX+d) */

	rc = z80ic_inc_iixd_create(&inc);
	if (rc != EOK)
		goto error;

	inc->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &inc->instr);
	if (rc != EOK)
		goto error;

	inc = NULL;

	/* jp NZ, inc16_nocarry */

	rc = z80ic_jp_cc_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(nocarry_lbl, &imm);
	if (rc != EOK)
		goto error;

	jp->cc = z80ic_cc_nz;
	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* inc (IX+d) */

	rc = z80ic_inc_iixd_create(&inc);
	if (rc != EOK)
		goto error;

	inc->disp = z80_idxacc_disp_r16h(&idxacc);

	rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
	if (rc != EOK)
		goto error;

	/* label inc16_nocarry */

	rc = z80ic_lblock_append(lblock, nocarry_lbl, NULL);
	if (rc != EOK)
		goto error;

	inc = NULL;
	free(nocarry_lbl);
	nocarry_lbl = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_imm16_destroy(imm);

	if (nocarry_lbl != NULL)
		free(nocarry_lbl);
	return rc;
}

/** Allocate registers for Z80 rotate left virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vr_rr Rotate left instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_rl_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_rl_vr_t *vr_rl, z80ic_lblock_t *lblock)
{
	z80ic_rl_iixd_t *rl = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vr_rl->vr->vregno,
	    vr_rl->vr->part, lblock);
	if (rc != EOK)
		goto error;

	/* rl (IX+d) */

	rc = z80ic_rl_iixd_create(&rl);
	if (rc != EOK)
		goto error;

	rl->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &rl->instr);
	if (rc != EOK)
		goto error;

	rl = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (rl != NULL)
		z80ic_instr_destroy(&rl->instr);

	return rc;
}

/** Allocate registers for Z80 rotate right virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vr_rr Rotate right instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_rr_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_rr_vr_t *vr_rr, z80ic_lblock_t *lblock)
{
	z80ic_rr_iixd_t *rr = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vr_rr->vr->vregno,
	    vr_rr->vr->part, lblock);
	if (rc != EOK)
		goto error;

	/* rr (IX+d) */

	rc = z80ic_rr_iixd_create(&rr);
	if (rc != EOK)
		goto error;

	rr->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &rr->instr);
	if (rc != EOK)
		goto error;

	rr = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (rr != NULL)
		z80ic_instr_destroy(&rr->instr);

	return rc;
}

/** Allocate registers for Z80 shift left arithmetic virtual register
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsla Shift left arithmetic instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_sla_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_sla_vr_t *vrsla, z80ic_lblock_t *lblock)
{
	z80ic_sla_iixd_t *sla = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrsla->vr->vregno,
	    vrsla->vr->part, lblock);
	if (rc != EOK)
		goto error;

	/* sla (IX+d) */

	rc = z80ic_sla_iixd_create(&sla);
	if (rc != EOK)
		goto error;

	sla->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &sla->instr);
	if (rc != EOK)
		goto error;

	sla = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (sla != NULL)
		z80ic_instr_destroy(&sla->instr);

	return rc;
}

/** Allocate registers for Z80 shift right arithmetic virtual register
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsra Shift right arithmetic instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_sra_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_sra_vr_t *vrsra, z80ic_lblock_t *lblock)
{
	z80ic_sra_iixd_t *sra = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrsra->vr->vregno,
	    vrsra->vr->part, lblock);
	if (rc != EOK)
		goto error;

	/* sra (IX+d) */

	rc = z80ic_sra_iixd_create(&sra);
	if (rc != EOK)
		goto error;

	sra->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &sra->instr);
	if (rc != EOK)
		goto error;

	sra = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (sra != NULL)
		z80ic_instr_destroy(&sra->instr);

	return rc;
}

/** Allocate registers for Z80 shift right logical virtual register
 * instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrsrl Shift right logical instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_srl_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_srl_vr_t *vrsrl, z80ic_lblock_t *lblock)
{
	z80ic_srl_iixd_t *srl = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrsrl->vr->vregno,
	    vrsrl->vr->part, lblock);
	if (rc != EOK)
		goto error;

	/* srl (IX+d) */

	rc = z80ic_srl_iixd_create(&srl);
	if (rc != EOK)
		goto error;

	srl->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &srl->instr);
	if (rc != EOK)
		goto error;

	srl = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (srl != NULL)
		z80ic_instr_destroy(&srl->instr);

	return rc;
}

/** Allocate registers for Z80 test bit of virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrbit Test bit instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_bit_b_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_bit_b_vr_t *vrbit, z80ic_lblock_t *lblock)
{
	z80ic_bit_b_iixd_t *bit = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrbit->src->vregno,
	    vrbit->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* bit b, (IX+d) */

	rc = z80ic_bit_b_iixd_create(&bit);
	if (rc != EOK)
		goto error;

	bit->bit = vrbit->bit;
	bit->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &bit->instr);
	if (rc != EOK)
		goto error;

	bit = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (bit != NULL)
		z80ic_instr_destroy(&bit->instr);

	return rc;
}

/** Allocate registers for Z80 set bit of virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrset Set bit instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_set_b_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_set_b_vr_t *vrbit, z80ic_lblock_t *lblock)
{
	z80ic_set_b_iixd_t *set = NULL;
	z80_idxacc_t idxacc;
	int rc;

	(void) raproc;

	/* Set up index register */
	rc = z80_idxacc_setup_vr(&idxacc, raproc, vrbit->src->vregno,
	    vrbit->src->part, lblock);
	if (rc != EOK)
		goto error;

	/* set b, (IX+d) */

	rc = z80ic_set_b_iixd_create(&set);
	if (rc != EOK)
		goto error;

	set->bit = vrbit->bit;
	set->disp = z80_idxacc_disp(&idxacc);

	rc = z80ic_lblock_append(lblock, label, &set->instr);
	if (rc != EOK)
		goto error;

	set = NULL;

	/* Restore index register */
	rc = z80_idxacc_teardown(&idxacc, raproc, lblock);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (set != NULL)
		z80ic_instr_destroy(&set->instr);

	return rc;
}

/** Allocate registers for Z80 instruction.
 *
 * @param raproc Register allocator for procedure
 * @param label Label
 * @param vrinstr Instruction with virtual registers
 * @param lblock Labeled block where to append
 * @return EOK on success or an error code
 */
static int z80_ralloc_instr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_instr_t *vrinstr, z80ic_lblock_t *lblock)
{
	switch (vrinstr->itype) {
	case z80i_ld_r_n:
		return z80_ralloc_ld_r_n(raproc, label,
		    (z80ic_ld_r_n_t *) vrinstr->ext, lblock);
	case z80i_add_a_n:
		return z80_ralloc_add_a_n(raproc, label,
		    (z80ic_add_a_n_t *) vrinstr->ext, lblock);
	case z80i_adc_a_n:
		return z80_ralloc_adc_a_n(raproc, label,
		    (z80ic_adc_a_n_t *) vrinstr->ext, lblock);
	case z80i_sub_n:
		return z80_ralloc_sub_n(raproc, label,
		    (z80ic_sub_n_t *) vrinstr->ext, lblock);
	case z80i_and_r:
		return z80_ralloc_and_r(raproc, label,
		    (z80ic_and_r_t *) vrinstr->ext, lblock);
	case z80i_xor_r:
		return z80_ralloc_xor_r(raproc, label,
		    (z80ic_xor_r_t *) vrinstr->ext, lblock);
	case z80i_cp_n:
		return z80_ralloc_cp_n(raproc, label,
		    (z80ic_cp_n_t *) vrinstr->ext, lblock);
	case z80i_dec_r:
		return z80_ralloc_dec_r(raproc, label,
		    (z80ic_dec_r_t *) vrinstr->ext, lblock);
	case z80i_cpl:
		return z80_ralloc_cpl(raproc, label,
		    (z80ic_cpl_t *) vrinstr->ext, lblock);
	case z80i_nop:
		return z80_ralloc_nop(raproc, label,
		    (z80ic_nop_t *) vrinstr->ext, lblock);
	case z80i_inc_ss:
		return z80_ralloc_inc_ss(raproc, label,
		    (z80ic_inc_ss_t *) vrinstr->ext, lblock);
	case z80i_rla:
		return z80_ralloc_rla(raproc, label,
		    (z80ic_rla_t *) vrinstr->ext, lblock);
	case z80i_jp_nn:
		return z80_ralloc_jp_nn(raproc, label,
		    (z80ic_jp_nn_t *) vrinstr->ext, lblock);
	case z80i_jp_cc_nn:
		return z80_ralloc_jp_cc_nn(raproc, label,
		    (z80ic_jp_cc_nn_t *) vrinstr->ext, lblock);
	case z80i_call_nn:
		return z80_ralloc_call_nn(raproc, label,
		    (z80ic_call_nn_t *) vrinstr->ext, lblock);
	case z80i_ret:
		return z80_ralloc_ret(raproc, label,
		    (z80ic_ret_t *) vrinstr->ext, lblock);
	case z80i_ld_r_ivrrd:
		return z80_ralloc_ld_r_ivrrd(raproc, label,
		    (z80ic_ld_r_ivrrd_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_vr:
		return z80_ralloc_ld_vr_vr(raproc, label,
		    (z80ic_ld_vr_vr_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_n:
		return z80_ralloc_ld_vr_n(raproc, label,
		    (z80ic_ld_vr_n_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_ihl:
		return z80_ralloc_ld_vr_ihl(raproc, label,
		    (z80ic_ld_vr_ihl_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_iixd:
		return z80_ralloc_ld_vr_iixd(raproc, label,
		    (z80ic_ld_vr_iixd_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_ivrr:
		return z80_ralloc_ld_vr_ivrr(raproc, label,
		    (z80ic_ld_vr_ivrr_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_ivrrd:
		return z80_ralloc_ld_vr_ivrrd(raproc, label,
		    (z80ic_ld_vr_ivrrd_t *) vrinstr->ext, lblock);
	case z80i_ld_ihl_vr:
		return z80_ralloc_ld_ihl_vr(raproc, label,
		    (z80ic_ld_ihl_vr_t *) vrinstr->ext, lblock);
	case z80i_ld_ivrr_vr:
		return z80_ralloc_ld_ivrr_vr(raproc, label,
		    (z80ic_ld_ivrr_vr_t *) vrinstr->ext, lblock);
	case z80i_ld_ivrrd_r:
		return z80_ralloc_ld_ivrrd_r(raproc, label,
		    (z80ic_ld_ivrrd_r_t *) vrinstr->ext, lblock);
	case z80i_ld_ivrrd_vr:
		return z80_ralloc_ld_ivrrd_vr(raproc, label,
		    (z80ic_ld_ivrrd_vr_t *) vrinstr->ext, lblock);
	case z80i_ld_ivrr_n:
		return z80_ralloc_ld_ivrr_n(raproc, label,
		    (z80ic_ld_ivrr_n_t *) vrinstr->ext, lblock);
	case z80i_ld_ivrrd_n:
		return z80_ralloc_ld_ivrrd_n(raproc, label,
		    (z80ic_ld_ivrrd_n_t *) vrinstr->ext, lblock);
	case z80i_ld_vrr_vrr:
		return z80_ralloc_ld_vrr_vrr(raproc, label,
		    (z80ic_ld_vrr_vrr_t *) vrinstr->ext, lblock);
	case z80i_ld_r_vr:
		return z80_ralloc_ld_r_vr(raproc, label,
		    (z80ic_ld_r_vr_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_r:
		return z80_ralloc_ld_vr_r(raproc, label,
		    (z80ic_ld_vr_r_t *) vrinstr->ext, lblock);
	case z80i_ld_r16_vrr:
		return z80_ralloc_ld_r16_vrr(raproc, label,
		    (z80ic_ld_r16_vrr_t *) vrinstr->ext, lblock);
	case z80i_ld_vrr_r16:
		return z80_ralloc_ld_vrr_r16(raproc, label,
		    (z80ic_ld_vrr_r16_t *) vrinstr->ext, lblock);
	case z80i_ld_vrr_iixd:
		return z80_ralloc_ld_vrr_iixd(raproc, label,
		    (z80ic_ld_vrr_iixd_t *) vrinstr->ext, lblock);
	case z80i_ld_vrr_nn:
		return z80_ralloc_ld_vrr_nn(raproc, label,
		    (z80ic_ld_vrr_nn_t *) vrinstr->ext, lblock);
	case z80i_ld_vrr_sfbnn:
		return z80_ralloc_ld_vrr_sfbnn(raproc, label,
		    (z80ic_ld_vrr_sfbnn_t *) vrinstr->ext, lblock);
	case z80i_ld_vrr_sfenn:
		return z80_ralloc_ld_vrr_sfenn(raproc, label,
		    (z80ic_ld_vrr_sfenn_t *) vrinstr->ext, lblock);
	case z80i_ld_isfbnn_r:
		return z80_ralloc_ld_isfbnn_r(raproc, label,
		    (z80ic_ld_isfbnn_r_t *) vrinstr->ext, lblock);
	case z80i_push_vr:
		return z80_ralloc_push_vr(raproc, label,
		    (z80ic_push_vr_t *) vrinstr->ext, lblock);
	case z80i_push_vrr:
		return z80_ralloc_push_vrr(raproc, label,
		    (z80ic_push_vrr_t *) vrinstr->ext, lblock);
	case z80i_add_a_vr:
		return z80_ralloc_add_a_vr(raproc, label,
		    (z80ic_add_a_vr_t *) vrinstr->ext, lblock);
	case z80i_add_a_ivrrd:
		return z80_ralloc_add_a_ivrrd(raproc, label,
		    (z80ic_add_a_ivrrd_t *) vrinstr->ext, lblock);
	case z80i_adc_a_vr:
		return z80_ralloc_adc_a_vr(raproc, label,
		    (z80ic_adc_a_vr_t *) vrinstr->ext, lblock);
	case z80i_adc_a_ivrrd:
		return z80_ralloc_adc_a_ivrrd(raproc, label,
		    (z80ic_adc_a_ivrrd_t *) vrinstr->ext, lblock);
	case z80i_sub_vr:
		return z80_ralloc_sub_vr(raproc, label,
		    (z80ic_sub_vr_t *) vrinstr->ext, lblock);
	case z80i_sbc_a_vr:
		return z80_ralloc_sbc_a_vr(raproc, label,
		    (z80ic_sbc_a_vr_t *) vrinstr->ext, lblock);
	case z80i_and_vr:
		return z80_ralloc_and_vr(raproc, label,
		    (z80ic_and_vr_t *) vrinstr->ext, lblock);
	case z80i_or_vr:
		return z80_ralloc_or_vr(raproc, label,
		    (z80ic_or_vr_t *) vrinstr->ext, lblock);
	case z80i_xor_vr:
		return z80_ralloc_xor_vr(raproc, label,
		    (z80ic_xor_vr_t *) vrinstr->ext, lblock);
	case z80i_inc_vr:
		return z80_ralloc_inc_vr(raproc, label,
		    (z80ic_inc_vr_t *) vrinstr->ext, lblock);
	case z80i_dec_vr:
		return z80_ralloc_dec_vr(raproc, label,
		    (z80ic_dec_vr_t *) vrinstr->ext, lblock);
	case z80i_add_vrr_vrr:
		return z80_ralloc_add_vrr_vrr(raproc, label,
		    (z80ic_add_vrr_vrr_t *) vrinstr->ext, lblock);
	case z80i_sub_vrr_vrr:
		return z80_ralloc_sub_vrr_vrr(raproc, label,
		    (z80ic_sub_vrr_vrr_t *) vrinstr->ext, lblock);
	case z80i_inc_vrr:
		return z80_ralloc_inc_vrr(raproc, label,
		    (z80ic_inc_vrr_t *) vrinstr->ext, lblock);
	case z80i_rl_vr:
		return z80_ralloc_rl_vr(raproc, label,
		    (z80ic_rl_vr_t *) vrinstr->ext, lblock);
	case z80i_rr_vr:
		return z80_ralloc_rr_vr(raproc, label,
		    (z80ic_rr_vr_t *) vrinstr->ext, lblock);
	case z80i_sla_vr:
		return z80_ralloc_sla_vr(raproc, label,
		    (z80ic_sla_vr_t *) vrinstr->ext, lblock);
	case z80i_sra_vr:
		return z80_ralloc_sra_vr(raproc, label,
		    (z80ic_sra_vr_t *) vrinstr->ext, lblock);
	case z80i_srl_vr:
		return z80_ralloc_srl_vr(raproc, label,
		    (z80ic_srl_vr_t *) vrinstr->ext, lblock);
	case z80i_bit_b_vr:
		return z80_ralloc_bit_b_vr(raproc, label,
		    (z80ic_bit_b_vr_t *) vrinstr->ext, lblock);
	case z80i_set_b_vr:
		return z80_ralloc_set_b_vr(raproc, label,
		    (z80ic_set_b_vr_t *) vrinstr->ext, lblock);
	default:
		assert(false);
		return EINVAL;
	}

	assert(false);
	return EINVAL;
}

/** Allocate registers for Z80 label.
 *
 * @param raproc Register allocator for procedure
 * @param label Label
 * @param lblock Labeled block where to append
 * @return EOK on success or an error code
 */
static int z80_ralloc_label(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_lblock_t *lblock)
{
	int rc;

	(void) raproc;

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Copy over Z80 IC DEFB data entry through register allocation stage.
 *
 * @param ralloc Register allocator
 * @param vrdentry Data entry from Z80 IC module with virtual registers
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_ralloc_defb(z80_ralloc_t *ralloc, z80ic_dentry_t *vrdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	int rc;

	(void) ralloc;
	assert(vrdentry->dtype == z80icd_defb);

	rc = z80ic_dentry_create_defb(vrdentry->value, &dentry);
	if (rc != EOK)
		goto error;

	rc = z80ic_dblock_append(dblock, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_dentry_destroy(dentry);
	return rc;
}

/** Copy over Z80 IC DEFW data entry through register allocation stage.
 *
 * @param ralloc Register allocator
 * @param vrdentry Data entry from Z80 IC module with virtual registers
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_ralloc_defw(z80_ralloc_t *ralloc, z80ic_dentry_t *vrdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	int rc;

	(void) ralloc;
	assert(vrdentry->dtype == z80icd_defw);

	if (vrdentry->ident != NULL) {
		rc = z80ic_dentry_create_defw_sym(vrdentry->ident,
		    vrdentry->value, &dentry);
		if (rc != EOK)
			goto error;
	} else {
		rc = z80ic_dentry_create_defw(vrdentry->value, &dentry);
		if (rc != EOK)
			goto error;
	}

	rc = z80ic_dblock_append(dblock, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_dentry_destroy(dentry);
	return rc;
}

/** Copy over Z80 IC DEFDW data entry through register allocation stage.
 *
 * @param ralloc Register allocator
 * @param vrdentry Data entry from Z80 IC module with virtual registers
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_ralloc_defdw(z80_ralloc_t *ralloc, z80ic_dentry_t *vrdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	int rc;

	(void) ralloc;
	assert(vrdentry->dtype == z80icd_defdw);

	rc = z80ic_dentry_create_defdw(vrdentry->value, &dentry);
	if (rc != EOK)
		goto error;

	rc = z80ic_dblock_append(dblock, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_dentry_destroy(dentry);
	return rc;
}

/** Copy over Z80 IC DEFQW data entry through register allocation stage.
 *
 * @param ralloc Register allocator
 * @param vrdentry Data entry from Z80 IC module with virtual registers
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_ralloc_defqw(z80_ralloc_t *ralloc, z80ic_dentry_t *vrdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	int rc;

	(void) ralloc;
	assert(vrdentry->dtype == z80icd_defqw);

	rc = z80ic_dentry_create_defqw(vrdentry->value, &dentry);
	if (rc != EOK)
		goto error;

	rc = z80ic_dblock_append(dblock, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_dentry_destroy(dentry);
	return rc;
}

/** Copy over Z80 IC data entry through register allocation stage.
 *
 * @param ralloc Register allocator
 * @param vrdentry Z80 IC data entry
 * @param dblock Labeled block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_ralloc_dentry(z80_ralloc_t *ralloc, z80ic_dentry_t *vrdentry,
    z80ic_dblock_t *dblock)
{
	switch (vrdentry->dtype) {
	case z80icd_defb:
		return z80_ralloc_defb(ralloc, vrdentry, dblock);
	case z80icd_defw:
		return z80_ralloc_defw(ralloc, vrdentry, dblock);
	case z80icd_defdw:
		return z80_ralloc_defdw(ralloc, vrdentry, dblock);
	case z80icd_defqw:
		return z80_ralloc_defqw(ralloc, vrdentry, dblock);
	}

	assert(false);
	return EINVAL;
}

/** Copy over extern declaration through the register allocation stage
 *
 * @param ralloc Register allocator
 * @param vrextern IR extern declartion
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_ralloc_extern(z80_ralloc_t *ralloc, z80ic_extern_t *vrextern,
    z80ic_module_t *icmod)
{
	z80ic_extern_t *icextern = NULL;
	int rc;

	(void) ralloc;

	rc = z80ic_extern_create(vrextern->ident, &icextern);
	if (rc != EOK)
		goto error;

	z80ic_module_append(icmod, &icextern->decln);
	return EOK;
error:
	return rc;
}

/** Copy over global declaration through the register allocation stage
 *
 * @param ralloc Register allocator
 * @param vrglobal IR global declartion
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_ralloc_global(z80_ralloc_t *ralloc, z80ic_global_t *vrglobal,
    z80ic_module_t *icmod)
{
	z80ic_global_t *icglobal = NULL;
	int rc;

	(void) ralloc;

	rc = z80ic_global_create(vrglobal->ident, &icglobal);
	if (rc != EOK)
		goto error;

	z80ic_module_append(icmod, &icglobal->decln);
	return EOK;
error:
	return rc;
}

/** Copy over variable declaration through the register allocation stage
 *
 * @param ralloc Register allocator
 * @param vrvar IR variable
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_ralloc_var(z80_ralloc_t *ralloc, z80ic_var_t *vrvar,
    z80ic_module_t *icmod)
{
	z80ic_dblock_entry_t *entry;
	z80ic_var_t *icvar = NULL;
	z80ic_dblock_t *dblock = NULL;
	int rc;

	rc = z80ic_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	rc = z80ic_var_create(vrvar->ident, dblock, &icvar);
	if (rc != EOK)
		goto error;

	entry = z80ic_dblock_first(vrvar->dblock);
	while (entry != NULL) {
		rc = z80_ralloc_dentry(ralloc, entry->dentry, dblock);
		if (rc != EOK)
			goto error;

		entry = z80ic_dblock_next(entry);
	}

	z80ic_module_append(icmod, &icvar->decln);
	return EOK;
error:
	z80ic_var_destroy(icvar);
	z80ic_dblock_destroy(dblock);
	return rc;
}

/** Copy over local variables for Z80 procedure.
 *
 * @param ralloc Register allocator
 * @param proc Z80 procedure with virtual registers
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80ic_ralloc_lvars(z80_ralloc_t *ralloc, z80ic_proc_t *vrproc,
    z80ic_proc_t *icproc)
{
	z80ic_lvar_t *vrvar;
	z80ic_lvar_t *icvar = NULL;
	int rc;

	(void) ralloc;

	vrvar = z80ic_proc_first_lvar(vrproc);
	while (vrvar != NULL) {
		rc = z80ic_lvar_create(vrvar->ident, vrvar->off, &icvar);
		if (rc != EOK)
			goto error;

		z80ic_proc_append_lvar(icproc, icvar);
		vrvar = z80ic_proc_next_lvar(vrvar);
	}

	return EOK;
error:
	return rc;
}

/** Allocate registers for Z80 procedure.
 *
 * @param ralloc Register allocator
 * @param proc Z80 procedure with virtual registers
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
	z80ic_lvar_t *lvar;
	size_t sfsize;
	size_t varsize;
	int rc;

	/* Last variable should be __end denoting total size of local variables */
	lvar = z80ic_proc_last_lvar(vrproc);
	if (lvar != NULL) {
		varsize = lvar->off;
	} else {
		varsize = 0;
	}

	/* XXX Assumes all virtual registers are 16-bit */
	sfsize = varsize + vrproc->used_vrs * 2;

	rc = z80_ralloc_proc_create(ralloc, vrproc, &raproc);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = z80ic_proc_create(vrproc->ident, lblock, &icproc);
	if (rc != EOK)
		goto error;

	/* Copy over local variables */
	rc = z80ic_ralloc_lvars(ralloc, vrproc, icproc);
	if (rc != EOK)
		goto error;

	/* Insert prologue to allocate a stack frame */
	rc = z80_ralloc_sfalloc(raproc, sfsize, lblock);
	if (rc != EOK)
		goto error;

	/* Convert each instruction */
	entry = z80ic_lblock_first(vrproc->lblock);
	while (entry != NULL) {
		if (entry->instr != NULL) {
			/* Instruction */
			assert(entry->label == NULL);

			rc = z80_ralloc_instr(raproc, NULL, entry->instr, lblock);
			if (rc != EOK)
				goto error;
		} else {
			/* Label */
			rc = z80_ralloc_label(raproc, entry->label, lblock);
			if (rc != EOK)
				goto error;
		}

		entry = z80ic_lblock_next(entry);
	}

	/*
	 * We should be back to zero at the end of a procedure. If not,
	 * either our computation is incorrect, or the generated code
	 * is incorrect (unbalanced).
	 */
	assert(raproc->spadj == 0);

	z80_ralloc_proc_destroy(raproc);
	z80ic_module_append(icmod, &icproc->decln);
	return EOK;
error:
	z80ic_proc_destroy(icproc);
	z80ic_lblock_destroy(lblock);
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
	case z80icd_extern:
		rc = z80_ralloc_extern(ralloc, (z80ic_extern_t *) decln->ext, icmod);
		break;
	case z80icd_global:
		rc = z80_ralloc_global(ralloc, (z80ic_global_t *) decln->ext, icmod);
		break;
	case z80icd_var:
		rc = z80_ralloc_var(ralloc, (z80ic_var_t *) decln->ext, icmod);
		break;
	case z80icd_proc:
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
