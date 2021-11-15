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
 * Z80 Instruction selector
 *
 * Convert (machine-independent) IR to Z80 IC with virtual registers.
 */

#include <assert.h>
#include <ir.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <z80/isel.h>
#include <z80/z80ic.h>

/** Mangle global identifier.
 *
 * @param irident IR global identifier
 * @param rident Place to store pointer to IC procedure identifier
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_mangle_global_ident(const char *irident, char **rident)
{
	int rv;
	char *ident;

	/* The indentifier must have global scope */
	assert(irident[0] == '@');

	rv = asprintf(&ident, "_%s", &irident[1]);
	if (rv < 0)
		return ENOMEM;

	*rident = ident;
	return EOK;
}

/** Get virtual register number from variable name.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @param oper Variable operand referring to a local numbered variable
 *
 * @return Virtual register number (same as variable number)
 */
static unsigned z80_isel_get_vregno(z80_isel_proc_t *isproc, ir_oper_t *oper)
{
	ir_oper_var_t *opvar;
	char *endptr;
	unsigned long rn;

	assert(oper->optype == iro_var);
	opvar = (ir_oper_var_t *) oper->ext;

	assert(opvar->varname[0] == '%');
	rn = strtoul(&opvar->varname[1], &endptr, 10);
	assert(*endptr == '\0');

	/* Remember maximum size of virtual register space used */
	if (rn + 1 > isproc->used_vrs) {
		isproc->used_vrs = rn + 1;
	}

	return (unsigned) rn;
}

/** Create instruction selector.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_isel_create(z80_isel_t **rz80_isel)
{
	z80_isel_t *isel;

	isel = calloc(1, sizeof(z80_isel_t));
	if (isel == NULL)
		return ENOMEM;

	*rz80_isel = isel;
	return EOK;
}

/** Create instruction selector.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_proc_create(z80_isel_t *isel, z80_isel_proc_t **risproc)
{
	z80_isel_proc_t *isproc;

	isproc = calloc(1, sizeof(z80_isel_proc_t));
	if (isproc == NULL)
		return ENOMEM;

	isproc->isel = isel;
	isproc->used_vrs = 0;
	*risproc = isproc;
	return EOK;
}

/** Destroy instruction selector for procedure.
 *
 * @param isel Instruction selector or @c NULL
 */
static void z80_isel_proc_destroy(z80_isel_proc_t *isproc)
{
	if (isproc == NULL)
		return;

	free(isproc);
}

/** Select Z80 IC instructions code for IR add instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_add(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vrr_t *dest = NULL;
	z80ic_oper_vrr_t *src = NULL;
	z80ic_ld_vrr_vrr_t *ld = NULL;
	z80ic_add_vrr_vrr_t *add = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	int rc;

	assert(irinstr->itype == iri_add);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	/* Load instruction */

	rc = z80ic_ld_vrr_vrr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr1, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Add instruction */

	rc = z80ic_add_vrr_vrr_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr2, &src);
	if (rc != EOK)
		goto error;

	add->dest = dest;
	add->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &add->instr);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_instr_destroy(&ld->instr);
	z80ic_instr_destroy(&add->instr);
	z80ic_oper_vrr_destroy(dest);
	z80ic_oper_vrr_destroy(src);

	return rc;
}

/** Select Z80 IC instructions code for IR call instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_call(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_r16_t *ldsrc = NULL;
	z80ic_oper_vrr_t *lddest = NULL;
	z80ic_call_nn_t *call = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_ld_vrr_r16_t *ld = NULL;
	ir_oper_var_t *op1;
	char *varident = NULL;
	unsigned destvr;
	int rc;

	assert(irinstr->itype == iri_call);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_list);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	op1 = (ir_oper_var_t *) irinstr->op1->ext;

	rc = z80_isel_mangle_global_ident(op1->varname, &varident);
	if (rc != EOK)
		goto error;

	/* Call instruction */

	rc = z80ic_call_nn_create(&call);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(varident, &imm);
	if (rc != EOK)
		goto error;

	call->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &call->instr);
	if (rc != EOK)
		goto error;

	call = NULL;

	/* ld vrr, BC */

	rc = z80ic_ld_vrr_r16_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &lddest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_bc, &ldsrc);
	if (rc != EOK)
		goto error;

	ld->dest = lddest;
	ld->src = ldsrc;
	lddest = NULL;
	ldsrc = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (call != NULL)
		z80ic_instr_destroy(&call->instr);
	z80ic_oper_vrr_destroy(lddest);
	z80ic_oper_r16_destroy(ldsrc);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Select Z80 IC instructions code for IR sub instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_sub(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vrr_t *dest = NULL;
	z80ic_oper_vrr_t *src = NULL;
	z80ic_ld_vrr_vrr_t *ld = NULL;
	z80ic_sub_vrr_vrr_t *sub = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	int rc;

	assert(irinstr->itype == iri_sub);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	/* Load instruction */

	rc = z80ic_ld_vrr_vrr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr1, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Subtract instruction */

	rc = z80ic_sub_vrr_vrr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr2, &src);
	if (rc != EOK)
		goto error;

	sub->dest = dest;
	sub->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_instr_destroy(&ld->instr);
	z80ic_instr_destroy(&sub->instr);
	z80ic_oper_vrr_destroy(dest);
	z80ic_oper_vrr_destroy(src);

	return rc;
}

/** Select Z80 IC instructions code for IR load immediate instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_imm(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_vrr_nn_t *ldimm = NULL;
	z80ic_oper_vrr_t *vrr = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	ir_oper_imm_t *irimm;
	unsigned vregno;
	int rc;

	assert(irinstr->itype == iri_imm);
	assert(irinstr->width == 16);

	assert(irinstr->op1->optype == iro_imm);
	irimm = (ir_oper_imm_t *) irinstr->op1->ext;

	assert(irinstr->op2 == NULL);

	vregno = z80_isel_get_vregno(isproc, irinstr->dest);

	rc = z80ic_ld_vrr_nn_create(&ldimm);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vregno, &vrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_val(irimm->value, &imm);
	if (rc != EOK)
		goto error;

	ldimm->dest = vrr;
	ldimm->imm16 = imm;
	vrr = NULL;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldimm->instr);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_instr_destroy(&ldimm->instr);
	z80ic_oper_vrr_destroy(vrr);
	z80ic_oper_imm16_destroy(imm);
	return rc;
}

/** Select Z80 IC instructions code for IR read instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR read instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_read(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r16_vrr_t *ldaddr = NULL;
	z80ic_ld_vr_ihl_t *lddata = NULL;
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_r16_t *adest = NULL;
	z80ic_oper_vrr_t *asrc = NULL;
	z80ic_oper_ss_t *ainc = NULL;
	z80ic_oper_vr_t *ddest = NULL;
	unsigned destvr;
	unsigned vr;
	int rc;

	/*
	 * If we could allocate a new virtual register, we might use
	 * that instead of specifying HL directly, which would, in theory,
	 * allow using IX or IY (if available)
	 */

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr = z80_isel_get_vregno(isproc, irinstr->op1);

	/* ld HL, vrrA */

	rc = z80ic_ld_r16_vrr_create(&ldaddr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_hl, &adest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr, &asrc);
	if (rc != EOK)
		goto error;

	ldaddr->dest = adest;
	ldaddr->src = asrc;
	adest = NULL;
	asrc = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldaddr->instr);
	if (rc != EOK)
		goto error;

	ldaddr = NULL;

	/* ld vrrB.L, (HL) */

	rc = z80ic_ld_vr_ihl_create(&lddata);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &ddest);
	if (rc != EOK)
		goto error;

	lddata->dest = ddest;
	ddest = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &lddata->instr);
	if (rc != EOK)
		goto error;

	lddata = NULL;

	/* inc HL */

	rc = z80ic_inc_ss_create(&inc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(z80ic_ss_hl, &ainc);
	if (rc != EOK)
		goto error;

	inc->dest = ainc;
	ainc = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
	if (rc != EOK)
		goto error;

	lddata = NULL;

	/* ld vrrB.H, (HL) */

	rc = z80ic_ld_vr_ihl_create(&lddata);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &ddest);
	if (rc != EOK)
		goto error;

	lddata->dest = ddest;
	ddest = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &lddata->instr);
	if (rc != EOK)
		goto error;

	lddata = NULL;

	return EOK;
error:
	z80ic_instr_destroy(&ldaddr->instr);
	z80ic_instr_destroy(&lddata->instr);
	z80ic_instr_destroy(&inc->instr);
	z80ic_oper_r16_destroy(adest);
	z80ic_oper_vrr_destroy(asrc);
	z80ic_oper_ss_destroy(ainc);
	z80ic_oper_vr_destroy(ddest);

	return rc;
}

/** Select Z80 IC instructions code for IR return value instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_retv(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_r16_t *dest = NULL;
	z80ic_oper_vrr_t *src = NULL;
	z80ic_ld_r16_vrr_t *ld = NULL;
	z80ic_ret_t *ret = NULL;
	unsigned vr;
	int rc;

	assert(irinstr->itype == iri_retv);
	assert(irinstr->width == 16);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	vr = z80_isel_get_vregno(isproc, irinstr->op1);

	/* Load instruction */

	rc = z80ic_ld_r16_vrr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_bc, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* Ret instruction */

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;
	return EOK;
error:
	z80ic_instr_destroy(&ld->instr);
	z80ic_instr_destroy(&ret->instr);
	z80ic_oper_r16_destroy(dest);
	z80ic_oper_vrr_destroy(src);

	return rc;
}

/** Select Z80 IC instructions code for IR variable pointer instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_varptr(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vrr_t *dest = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_ld_vrr_nn_t *ld = NULL;
	unsigned destvr;
	ir_oper_var_t *op1;
	char *varident = NULL;
	int rc;

	assert(irinstr->itype == iri_varptr);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	op1 = (ir_oper_var_t *) irinstr->op1->ext;

	rc = z80_isel_mangle_global_ident(op1->varname, &varident);
	if (rc != EOK)
		goto error;

	/* Load instruction */

	rc = z80ic_ld_vrr_nn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(varident, &imm);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm16 = imm;
	dest = NULL;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (varident != NULL)
		free(varident);
	z80ic_instr_destroy(&ld->instr);
	z80ic_oper_vrr_destroy(dest);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Select Z80 IC instructions code for IR write instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR write instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_write(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r16_vrr_t *ldaddr = NULL;
	z80ic_ld_ihl_vr_t *lddata = NULL;
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_r16_t *adest = NULL;
	z80ic_oper_vrr_t *asrc = NULL;
	z80ic_oper_ss_t *ainc = NULL;
	z80ic_oper_vr_t *dsrc = NULL;
	unsigned srcvr;
	unsigned vr;
	int rc;

	/*
	 * If we could allocate a new virtual register, we might use
	 * that instead of specifying HL directly, which would, in theory,
	 * allow using IX or IY (if available)
	 */

	vr = z80_isel_get_vregno(isproc, irinstr->op1);
	srcvr = z80_isel_get_vregno(isproc, irinstr->op2);

	/* ld HL, vrrA */

	rc = z80ic_ld_r16_vrr_create(&ldaddr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_hl, &adest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr, &asrc);
	if (rc != EOK)
		goto error;

	ldaddr->dest = adest;
	ldaddr->src = asrc;
	adest = NULL;
	asrc = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldaddr->instr);
	if (rc != EOK)
		goto error;

	ldaddr = NULL;

	/* ld (HL), vrrB.L */

	rc = z80ic_ld_ihl_vr_create(&lddata);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(srcvr, z80ic_vrp_r16l, &dsrc);
	if (rc != EOK)
		goto error;

	lddata->src = dsrc;
	dsrc = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &lddata->instr);
	if (rc != EOK)
		goto error;

	lddata = NULL;

	/* inc HL */

	rc = z80ic_inc_ss_create(&inc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(z80ic_ss_hl, &ainc);
	if (rc != EOK)
		goto error;

	inc->dest = ainc;
	ainc = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
	if (rc != EOK)
		goto error;

	lddata = NULL;

	/* ld (HL), vrrB.H */

	rc = z80ic_ld_ihl_vr_create(&lddata);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(srcvr, z80ic_vrp_r16h, &dsrc);
	if (rc != EOK)
		goto error;

	lddata->src = dsrc;
	dsrc = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &lddata->instr);
	if (rc != EOK)
		goto error;

	lddata = NULL;

	return EOK;
error:
	z80ic_instr_destroy(&ldaddr->instr);
	z80ic_instr_destroy(&lddata->instr);
	z80ic_instr_destroy(&inc->instr);
	z80ic_oper_r16_destroy(adest);
	z80ic_oper_vrr_destroy(asrc);
	z80ic_oper_ss_destroy(ainc);
	z80ic_oper_vr_destroy(dsrc);

	return rc;
}

/** Select Z80 IC instructions for IR instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_instr(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	switch (irinstr->itype) {
	case iri_add:
		return z80_isel_add(isproc, label, irinstr, lblock);
	case iri_call:
		return z80_isel_call(isproc, label, irinstr, lblock);
	case iri_sub:
		return z80_isel_sub(isproc, label, irinstr, lblock);
	case iri_imm:
		return z80_isel_imm(isproc, label, irinstr, lblock);
	case iri_read:
		return z80_isel_read(isproc, label, irinstr, lblock);
	case iri_retv:
		return z80_isel_retv(isproc, label, irinstr, lblock);
	case iri_varptr:
		return z80_isel_varptr(isproc, label, irinstr, lblock);
	case iri_write:
		return z80_isel_write(isproc, label, irinstr, lblock);
	}

	assert(false);
	return EINVAL;
}

/** Select Z80 IC instructions for IR integer data entry.
 *
 * @param isel Instruction selector
 * @param irdentry IR integer data entry
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_isel_int(z80_isel_t *isel, ir_dentry_t *irdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	int rc;

	(void) isel;
	assert(irdentry->dtype == ird_int);

	rc = z80ic_dentry_create_defw(irdentry->value, &dentry);
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

/** Select Z80 IC instructions for IR unsigned integer data entry.
 *
 * @param isel Instruction selector
 * @param irdentry IR integer data entry
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_isel_uint(z80_isel_t *isel, ir_dentry_t *irdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	int rc;

	(void) isel;
	assert(irdentry->dtype == ird_uint);

	rc = z80ic_dentry_create_defw(irdentry->value, &dentry);
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

/** Select Z80 IC instructions for IR data entry.
 *
 * @param isel Instruction selector
 * @param irdentry IR data entry
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_isel_dentry(z80_isel_t *isel, ir_dentry_t *irdentry,
    z80ic_dblock_t *dblock)
{
	switch (irdentry->dtype) {
	case ird_int:
		return z80_isel_int(isel, irdentry, dblock);
	case ird_uint:
		return z80_isel_uint(isel, irdentry, dblock);
	}

	assert(false);
	return EINVAL;
}

/** Select instructions code for variable.
 *
 * @param isel Instruction selector
 * @param irvar IR variable
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_var(z80_isel_t *isel, ir_var_t *irvar,
    z80ic_module_t *icmod)
{
	ir_dblock_entry_t *entry;
	z80ic_var_t *icvar = NULL;
	z80ic_dblock_t *dblock = NULL;
	char *ident = NULL;
	int rc;

	rc = z80ic_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	rc = z80_isel_mangle_global_ident(irvar->ident, &ident);
	if (rc != EOK)
		goto error;

	rc = z80ic_var_create(ident, dblock, &icvar);
	if (rc != EOK)
		goto error;

	entry = ir_dblock_first(irvar->dblock);
	while (entry != NULL) {
		rc = z80_isel_dentry(isel, entry->dentry, dblock);
		if (rc != EOK)
			goto error;

		entry = ir_dblock_next(entry);
	}

	free(ident);
	z80ic_module_append(icmod, &icvar->decln);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	z80ic_var_destroy(icvar);
	z80ic_dblock_destroy(dblock);
	return rc;
}

/** Select instructions code for procedure.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_proc(z80_isel_t *isel, ir_proc_t *irproc,
    z80ic_module_t *icmod)
{
	z80_isel_proc_t *isproc = NULL;
	ir_lblock_entry_t *entry;
	z80ic_proc_t *icproc = NULL;
	z80ic_lblock_t *lblock = NULL;
	char *ident = NULL;
	int rc;

	rc = z80_isel_proc_create(isel, &isproc);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = z80_isel_mangle_global_ident(irproc->ident, &ident);
	if (rc != EOK)
		goto error;

	rc = z80ic_proc_create(ident, lblock, &icproc);
	if (rc != EOK)
		goto error;

	entry = ir_lblock_first(irproc->lblock);
	while (entry != NULL) {
		rc = z80_isel_instr(isproc, entry->label, entry->instr, lblock);
		if (rc != EOK)
			goto error;

		entry = ir_lblock_next(entry);
	}

	icproc->used_vrs = isproc->used_vrs;

	free(ident);
	z80_isel_proc_destroy(isproc);
	z80ic_module_append(icmod, &icproc->decln);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	z80ic_proc_destroy(icproc);
	z80ic_lblock_destroy(lblock);
	z80_isel_proc_destroy(isproc);
	return rc;
}

/** Select instructions code for declaration.
 *
 * @param isel Instruction selector
 * @param decln IR declaration
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_decln(z80_isel_t *isel, ir_decln_t *decln,
    z80ic_module_t *icmod)
{
	int rc;

	switch (decln->dtype) {
	case ird_var:
		rc = z80_isel_var(isel, (ir_var_t *) decln->ext, icmod);
		break;
	case ird_proc:
		rc = z80_isel_proc(isel, (ir_proc_t *) decln->ext, icmod);
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Select instructions for module.
 *
 * @param isel Instruction selector
 * @param irmod IR module
 * @param ricmod Place to store pointer to new Z80 IC module
 * @return EOK on success or an error code
 */
int z80_isel_module(z80_isel_t *isel, ir_module_t *irmod,
    z80ic_module_t **ricmod)
{
	z80ic_module_t *icmod;
	int rc;
	ir_decln_t *decln;

	rc = z80ic_module_create(&icmod);
	if (rc != EOK)
		return rc;

	decln = ir_module_first(irmod);
	while (decln != NULL) {
		rc = z80_isel_decln(isel, decln, icmod);
		if (rc != EOK)
			goto error;

		decln = ir_module_next(decln);
	}

	*ricmod = icmod;
	return EOK;
error:
	z80ic_module_destroy(icmod);
	return rc;
}

/** Destroy instruction selector.
 *
 * @param isel Instruction selector or @c NULL
 */
void z80_isel_destroy(z80_isel_t *isel)
{
	if (isel == NULL)
		return;

	free(isel);
}
