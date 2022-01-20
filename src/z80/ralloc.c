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
 * Z80 Register allocation
 *
 * Convert Z80 IC with virtual registers to pure Z80 IC (not using virtual
 * registers).
 */

#include <assert.h>
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

/** Check and return displacement.
 *
 * XXX Make this a mandatory part of setting the displacement in z80ic
 */
static int8_t z80_ralloc_disp(long disp)
{
	assert(disp >= -128);
	assert(disp <= 127);

	return (int8_t) disp;
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
	unsigned vroff;
	int rc;

	(void) raproc;

	/* ld r, (IX+d) */

	rc = z80ic_ld_r_iixd_create(&ld);
	if (rc != EOK)
		goto error;

	vroff = z80_ralloc_vroff(part);
	ld->disp = z80_ralloc_disp(-2 * (1 + (long) vregno) + vroff);

	rc = z80ic_oper_reg_create(reg, &oreg);
	if (rc != EOK)
		goto error;

	ld->dest = oreg;
	oreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;
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
	unsigned vroff;
	int rc;

	(void) raproc;

	/* ld (IX+d), r */

	rc = z80ic_ld_iixd_r_create(&ld);
	if (rc != EOK)
		goto error;

	vroff = z80_ralloc_vroff(part);
	ld->disp = z80_ralloc_disp(-2 * (1 + (long) vregno) + vroff);

	rc = z80ic_oper_reg_create(reg, &oreg);
	if (rc != EOK)
		goto error;

	ld->src = oreg;
	oreg = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;
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
	if (reg != NULL)
		z80ic_oper_reg_destroy(reg);
	if (imm != NULL)
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
	if (imm != NULL)
		z80ic_oper_imm8_destroy(imm);

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
	z80ic_oper_ss_t *ss;
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

	rc = z80ic_lblock_append(lblock, label, &inc->instr);
	if (rc != EOK)
		goto error;

	inc = NULL;
	return EOK;
error:
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
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
	z80ic_oper_imm16_t *imm;
	int rc;

	(void) raproc;

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_copy(vrjp->imm16, &imm);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm;

	rc = z80ic_lblock_append(lblock, label, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;
	return EOK;
error:
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);
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
	z80ic_oper_imm16_t *imm;
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

	rc = z80ic_lblock_append(lblock, label, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;
	return EOK;
error:
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);
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
	z80ic_oper_imm16_t *imm;
	int rc;

	(void) raproc;

	rc = z80ic_call_nn_create(&call);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_copy(vrcall->imm16, &imm);
	if (rc != EOK)
		goto error;

	call->imm16 = imm;

	rc = z80ic_lblock_append(lblock, label, &call->instr);
	if (rc != EOK)
		goto error;

	call = NULL;
	return EOK;
error:
	if (call != NULL)
		z80ic_instr_destroy(&call->instr);
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
	if (ret != NULL)
		z80ic_instr_destroy(&ret->instr);
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
	unsigned vregno;
	unsigned vroff;
	int rc;

	(void) raproc;

	/* ld (IX+d), n */

	rc = z80ic_ld_iixd_n_create(&ld);
	if (rc != EOK)
		goto error;

	vregno = vrld->dest->vregno;
	vroff = z80_ralloc_vroff(vrld->dest->part);
	ld->disp = z80_ralloc_disp(-2 * (1 + (long) vregno) + vroff);

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
	if (imm != NULL)
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

	rc = z80ic_oper_imm16_copy(vrld->imm16, &imm);
	if (rc != EOK)
		goto error;

	ldnn->imm16 = imm;

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
	unsigned vroff;
	int rc;

	(void) raproc;

	/* and (IX+d) */

	rc = z80ic_and_iixd_create(&and);
	if (rc != EOK)
		goto error;

	vroff = z80_ralloc_vroff(vrand->src->part);
	and->disp = z80_ralloc_disp(-2 * (1 + (long) vrand->src->vregno) + vroff);

	rc = z80ic_lblock_append(lblock, label, &and->instr);
	if (rc != EOK)
		goto error;

	and = NULL;
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
	unsigned vroff;
	int rc;

	(void) raproc;

	/* or (IX+d) */

	rc = z80ic_or_iixd_create(&or);
	if (rc != EOK)
		goto error;

	vroff = z80_ralloc_vroff(vror->src->part);
	or->disp = z80_ralloc_disp(-2 * (1 + (long) vror->src->vregno) + vroff);

	rc = z80ic_lblock_append(lblock, label, &or->instr);
	if (rc != EOK)
		goto error;

	or = NULL;
	return EOK;
error:
	if (or != NULL)
		z80ic_instr_destroy(&or->instr);

	return rc;
}

/** Allocate registers for Z80 bitwise XOR with virtual register instruction.
 *
 * @param raproc Register allocator for procedure
 * @param vrxor OR instruction with VRs
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_ralloc_xor_vr(z80_ralloc_proc_t *raproc, const char *label,
    z80ic_xor_vr_t *vrxor, z80ic_lblock_t *lblock)
{
	z80ic_xor_iixd_t *xor = NULL;
	unsigned vroff;
	int rc;

	(void) raproc;

	/* xor (IX+d) */

	rc = z80ic_xor_iixd_create(&xor);
	if (rc != EOK)
		goto error;

	vroff = z80_ralloc_vroff(vrxor->src->part);
	xor->disp = z80_ralloc_disp(-2 * (1 + (long) vrxor->src->vregno) + vroff);

	rc = z80ic_lblock_append(lblock, label, &xor->instr);
	if (rc != EOK)
		goto error;

	xor = NULL;
	return EOK;
error:
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);

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
	case z80i_sub_n:
		return z80_ralloc_sub_n(raproc, label,
		    (z80ic_sub_n_t *) vrinstr->ext, lblock);
	case z80i_cpl:
		return z80_ralloc_cpl(raproc, label,
		    (z80ic_cpl_t *) vrinstr->ext, lblock);
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
	case z80i_ld_vr_n:
		return z80_ralloc_ld_vr_n(raproc, label,
		    (z80ic_ld_vr_n_t *) vrinstr->ext, lblock);
	case z80i_ld_vr_ihl:
		return z80_ralloc_ld_vr_ihl(raproc, label,
		    (z80ic_ld_vr_ihl_t *) vrinstr->ext, lblock);
	case z80i_ld_ihl_vr:
		return z80_ralloc_ld_ihl_vr(raproc, label,
		    (z80ic_ld_ihl_vr_t *) vrinstr->ext, lblock);
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
	case z80i_ld_vrr_nn:
		return z80_ralloc_ld_vrr_nn(raproc, label,
		    (z80ic_ld_vrr_nn_t *) vrinstr->ext, lblock);
	case z80i_and_vr:
		return z80_ralloc_and_vr(raproc, label,
		    (z80ic_and_vr_t *) vrinstr->ext, lblock);
	case z80i_or_vr:
		return z80_ralloc_or_vr(raproc, label,
		    (z80ic_or_vr_t *) vrinstr->ext, lblock);
	case z80i_xor_vr:
		return z80_ralloc_xor_vr(raproc, label,
		    (z80ic_xor_vr_t *) vrinstr->ext, lblock);
	case z80i_add_vrr_vrr:
		return z80_ralloc_add_vrr_vrr(raproc, label,
		    (z80ic_add_vrr_vrr_t *) vrinstr->ext, lblock);
	case z80i_sub_vrr_vrr:
		return z80_ralloc_sub_vrr_vrr(raproc, label,
		    (z80ic_sub_vrr_vrr_t *) vrinstr->ext, lblock);
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

	rc = z80ic_dentry_create_defw(vrdentry->value, &dentry);
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
	}

	assert(false);
	return EINVAL;
}

/** Copy over extern declaration through the regster allocation stage
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

/** Copy over variable declaration through the regster allocation stage
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
	z80ic_instr_t *instr = NULL;
	size_t sfsize;
	int rc;

	/* XXX Assumes all virtual registers are 16-bit */
	sfsize = vrproc->used_vrs * 2;

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
	rc = z80_ralloc_sfalloc(sfsize, lblock);
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
	case z80icd_extern:
		rc = z80_ralloc_extern(ralloc, (z80ic_extern_t *) decln->ext, icmod);
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
