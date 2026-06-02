/*
 * Copyright 2026 Jiri Svoboda
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
 * Z80 binary instruction emitter
 *
 * Convert Z80 IC to binary object
 */

#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <types/z80/z80opc.h>
#include <object/object.h>
#include <object/reloc.h>
#include <object/section.h>
#include <object/symbol.h>
#include <z80/emit.h>
#include <z80/z80ic.h>

/** Create binary instruction emitter.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_emit_create(z80_emit_t **remit)
{
	z80_emit_t *emit;

	emit = calloc(1, sizeof(z80_emit_t));
	if (emit == NULL)
		return ENOMEM;

	*remit = emit;
	return EOK;

}

/** Emit constant 8-bit value.
 *
 * @param emit Binary instruction emitter.
 * @param value Value
 * @return EOK on success or an error code
 */
static int z80_emit_u8(z80_emit_t *emit, uint8_t value)
{
	return obj_section_append_u8(emit->section, value);
}

/** Emit constant 16-bit value.
 *
 * @param emit Binary instruction emitter.
 * @param value Value
 * @return EOK on success or an error code
 */
static int z80_emit_u16(z80_emit_t *emit, uint16_t value)
{
	return obj_section_append_u16le(emit->section, value);
}

/** Emit constant 32-bit value.
 *
 * @param emit Binary instruction emitter.
 * @param value Value
 * @return EOK on success or an error code
 */
static int z80_emit_u32(z80_emit_t *emit, uint32_t value)
{
	return obj_section_append_u32le(emit->section, value);
}

/** Emit constant 64-bit value.
 *
 * @param emit Binary instruction emitter.
 * @param value Value
 * @return EOK on success or an error code
 */
static int z80_emit_u64(z80_emit_t *emit, uint64_t value)
{
	return obj_section_append_u64le(emit->section, value);
}

/** Emit 16-bit symbol address with addend.
 *
 * @param emit Binary instruction emitter.
 * @param ident Symbol identifier
 * @param value Addend
 * @return EOK on success or an error code
 */
static int z80_emit_sa16(z80_emit_t *emit, const char *ident, uint16_t value)
{
	int rc;
	uint32_t offset;

	offset = emit->section->len;

	rc = obj_section_append_u16le(emit->section, 0);
	if (rc != EOK)
		return rc;

	rc = obj_reloc_create(emit->object, emit->section, objr_sa16,
	    offset, ident, value);
	return rc;
}

/** Emit instruction opcode.
 *
 * The opcode can contain prefixes encoded in big endian. E.g.,
 * 0xdd35 correspond to dec (IX+d) prefixed opcode of 0xdd 0x3f.
 *
 * @param emit Binary instruction emitter.
 * @param opc Opcode including prefixes
 * @return EOK on success or an error code
 */
static int z80_emit_opc(z80_emit_t *emit, uint32_t opc)
{
	int rc;

	/* Two prefixes? */
	if ((opc >> 16) != 0) {
		rc = obj_section_append_u8(emit->section, (opc >> 16) & 0xff);
		if (rc != EOK)
			return rc;
	}

	/* One or two prefixes? */
	if ((opc >> 8) != 0) {
		rc = obj_section_append_u8(emit->section, (opc >> 8) & 0xff);
		if (rc != EOK)
			return rc;
	}

	rc = obj_section_append_u8(emit->section, opc & 0xff);
	return rc;
}

/** Emit instruction opcode + 8-bit immediate.
 *
 * @param emit Binary instruction emitter.
 * @param opc Opcode including prefixes
 * @param imm8 8-bit immediate operand
 * @return EOK on success or an error code
 */
static int z80_emit_opc_n(z80_emit_t *emit, uint32_t opc,
    z80ic_oper_imm8_t *imm8)
{
	int rc;

	rc = z80_emit_opc(emit, opc);
	if (rc != EOK)
		return rc;

	rc = z80_emit_u8(emit, imm8->imm8);
	return rc;
}

/** Emit instruction opcode + 16-bit immediate.
 *
 * @param emit Binary instruction emitter.
 * @param opc Opcode including prefixes
 * @param imm8 8-bit immediate operand
 * @return EOK on success or an error code
 */
static int z80_emit_opc_nn(z80_emit_t *emit, uint32_t opc,
    z80ic_oper_imm16_t *imm16)
{
	int rc;

	rc = z80_emit_opc(emit, opc);
	if (rc != EOK)
		return rc;

	if (imm16->symbol != NULL)
		rc = z80_emit_sa16(emit, imm16->symbol, imm16->imm16);
	else
		rc = z80_emit_u16(emit, imm16->imm16);

	return rc;
}

/** Emit instruction opcode + 8-bit displacement.
 *
 * @param emit Binary instruction emitter.
 * @param opc Opcode including prefixes
 * @param disp Displacement
 * @return EOK on success or an error code
 */
static int z80_emit_opc_d(z80_emit_t *emit, uint32_t opc, int8_t disp)
{
	int rc;

	if ((opc >> 8) == 0xddcbu) {
		/* Must be encoded as DD CB dd opc. */
		rc = z80_emit_opc(emit, 0xddcbu);
		if (rc != EOK)
			return rc;

		rc = z80_emit_u8(emit, (uint8_t)disp);
		if (rc != EOK)
			return rc;

		rc = z80_emit_u8(emit, opc & 0xff);
		if (rc != EOK)
			return rc;
	} else {
		/* Normal encoding. */
		rc = z80_emit_opc(emit, opc);
		if (rc != EOK)
			return rc;

		rc = z80_emit_u8(emit, (uint8_t)disp);
		if (rc != EOK)
			return rc;
	}
	return EOK;
}

/** Emit instruction opcode + 8-bit displacement + 8-bit immediate.
 *
 * @param emit Binary instruction emitter.
 * @param opc Opcode including prefixes
 * @param disp Displacement
 * @param imm8 8-bit immediate operand
 * @return EOK on success or an error code
 */
static int z80_emit_opc_d_n(z80_emit_t *emit, uint32_t opc, int8_t disp,
    z80ic_oper_imm8_t *imm8)
{
	int rc;

	rc = z80_emit_opc(emit, opc);
	if (rc != EOK)
		return rc;

	rc = z80_emit_u8(emit, (uint8_t)disp);
	if (rc != EOK)
		return rc;

	rc = z80_emit_u8(emit, imm8->imm8);
	return rc;
}

/** Emit binary instructions for data block entry.
 *
 * @param emit Binary instruction emitter
 * @param entry Data block entry
 * @return EOK on success or an error code
 */
static int z80_emit_dblock_entry(z80_emit_t *emit, z80ic_dblock_entry_t *entry)
{
	z80ic_dentry_t *dentry = entry->dentry;

	switch (dentry->dtype) {
	case z80icd_defb:
		return z80_emit_u8(emit, (uint8_t)dentry->value);
	case z80icd_defw:
		if (dentry->ident != NULL)
			return z80_emit_sa16(emit, dentry->ident,
			    (uint16_t)dentry->value);
		else
			return z80_emit_u16(emit, (uint16_t)dentry->value);
	case z80icd_defdw:
		return z80_emit_u32(emit, (uint32_t)dentry->value);
	case z80icd_defqw:
		return z80_emit_u64(emit, (uint64_t)dentry->value);
	}

	return EINVAL;
}

/** Emit binary instructions for variable.
 *
 * @param emit Binary instruction emitter
 * @param var Z80 IC variable
 * @return EOK on success or an error code
 */
static int z80_emit_var(z80_emit_t *emit, z80ic_var_t *var)
{
	int rc;
	z80ic_dblock_entry_t *entry;
	uint32_t offset;
	uint32_t size;
	obj_symbol_t *symbol;

	offset = emit->section->len;

	entry = z80ic_dblock_first(var->dblock);
	while (entry != NULL) {
		rc = z80_emit_dblock_entry(emit, entry);
		if (rc != EOK)
			return rc;

		entry = z80ic_dblock_next(entry);
	}

	size = emit->section->len - offset;
	rc = obj_symbol_create(emit->object, var->ident, emit->section,
	    offset, size, &symbol);
	if (rc != EOK)
		return rc;

	(void)symbol;
	return EOK;
}

/** Emit binary load register from register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_r_r(z80_emit_t *emit, z80ic_ld_r_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_r_r | ((uint8_t)instr->dest->reg << 3) |
	    (uint8_t)instr->src->reg;

	return z80_emit_opc(emit, opc);
}

/** Emit binary load register from 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_r_n(z80_emit_t *emit, z80ic_ld_r_n_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_r_n | ((uint8_t)instr->dest->reg << 3);
	return z80_emit_opc_n(emit, opc, instr->imm8);
}

/** Emit binary load register from (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_r_ihl(z80_emit_t *emit, z80ic_ld_r_ihl_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_r_ihl | ((uint8_t)instr->dest->reg << 3);
	return z80_emit_opc(emit, opc);
}

/** Emit binary load register from (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_r_iixd(z80_emit_t *emit, z80ic_ld_r_iixd_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_r_iixd | ((uint8_t)instr->dest->reg << 3);
	return z80_emit_opc_d(emit, opc, instr->disp);
}

/** Emit binary load register from (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_r_iiyd(z80_emit_t *emit, z80ic_ld_r_iiyd_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_r_iiyd | ((uint8_t)instr->dest->reg << 3);
	return z80_emit_opc_d(emit, opc, instr->disp);
}

/** Emit binary load (HL) from register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_ihl_r(z80_emit_t *emit, z80ic_ld_ihl_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_ihl_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary load (IX+d) from register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_iixd_r(z80_emit_t *emit, z80ic_ld_iixd_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_iixd_r | (uint8_t)instr->src->reg;
	return z80_emit_opc_d(emit, opc, instr->disp);
}

/** Emit binary load (HL) from 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_ihl_n(z80_emit_t *emit, z80ic_ld_ihl_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_ld_ihl_n, instr->imm8);
}

/** Emit binary load (IX+d) from 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_iixd_n(z80_emit_t *emit, z80ic_ld_iixd_n_t *instr)
{
	return z80_emit_opc_d_n(emit, z80opc_ld_iixd_n, instr->disp,
	    instr->imm8);
}

/** Emit binary load register pair from 16-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_dd_nn(z80_emit_t *emit, z80ic_ld_dd_nn_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_dd_nn | ((uint8_t)instr->dest->rdd << 4);
	return z80_emit_opc_nn(emit, opc, instr->imm16);
}

/** Emit binary load IX from 16-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_ix_nn(z80_emit_t *emit, z80ic_ld_ix_nn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_ix_nn, instr->imm16);
}

/** Emit binary load SP from IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_sp_ix(z80_emit_t *emit, z80ic_ld_sp_ix_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_sp_ix);
}

/** Emit binary push register pair instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_push_qq(z80_emit_t *emit, z80ic_push_qq_t *instr)
{
	uint32_t opc;

	opc = z80opc_push_qq | ((uint8_t)instr->src->rqq << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary push IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_push_ix(z80_emit_t *emit, z80ic_push_ix_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_push_ix);
}

/** Emit binary pop register pair instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_pop_qq(z80_emit_t *emit, z80ic_pop_qq_t *instr)
{
	uint32_t opc;

	opc = z80opc_pop_qq | ((uint8_t)instr->src->rqq << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary pop IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_pop_ix(z80_emit_t *emit, z80ic_pop_ix_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_pop_ix);
}

/** Emit binary return instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ret(z80_emit_t *emit, z80ic_ret_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ret);
}

/** Emit binary instruction.
 *
 * @param emit Binary instruction emitter
 * @param var Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_instr(z80_emit_t *emit, z80ic_instr_t *instr)
{
	switch (instr->itype) {
	case z80i_ld_r_r:
		return z80_emit_ld_r_r(emit, (z80ic_ld_r_r_t *)instr->ext);
	case z80i_ld_r_n:
		return z80_emit_ld_r_n(emit, (z80ic_ld_r_n_t *)instr->ext);
	case z80i_ld_r_ihl:
		return z80_emit_ld_r_ihl(emit, (z80ic_ld_r_ihl_t *)instr->ext);
	case z80i_ld_r_iixd:
		return z80_emit_ld_r_iixd(emit,
		    (z80ic_ld_r_iixd_t *)instr->ext);
	case z80i_ld_r_iiyd:
		return z80_emit_ld_r_iiyd(emit,
		    (z80ic_ld_r_iiyd_t *)instr->ext);
	case z80i_ld_ihl_r:
		return z80_emit_ld_ihl_r(emit, (z80ic_ld_ihl_r_t *)instr->ext);
	case z80i_ld_iixd_r:
		return z80_emit_ld_iixd_r(emit,
		    (z80ic_ld_iixd_r_t *)instr->ext);
	case z80i_ld_ihl_n:
		return z80_emit_ld_ihl_n(emit, (z80ic_ld_ihl_n_t *)instr->ext);
	case z80i_ld_iixd_n:
		return z80_emit_ld_iixd_n(emit,
		    (z80ic_ld_iixd_n_t *)instr->ext);
	case z80i_ld_dd_nn:
		return z80_emit_ld_dd_nn(emit, (z80ic_ld_dd_nn_t *)instr->ext);
	case z80i_ld_ix_nn:
		return z80_emit_ld_ix_nn(emit, (z80ic_ld_ix_nn_t *)instr->ext);
	case z80i_ld_sp_ix:
		return z80_emit_ld_sp_ix(emit, (z80ic_ld_sp_ix_t *)instr->ext);
	case z80i_push_qq:
		return z80_emit_push_qq(emit, (z80ic_push_qq_t *)instr->ext);
	case z80i_push_ix:
		return z80_emit_push_ix(emit, (z80ic_push_ix_t *)instr->ext);
	case z80i_pop_qq:
		return z80_emit_pop_qq(emit, (z80ic_pop_qq_t *)instr->ext);
	case z80i_pop_ix:
		return z80_emit_pop_ix(emit, (z80ic_pop_ix_t *)instr->ext);
	case z80i_ret:
		return z80_emit_ret(emit, (z80ic_ret_t *)instr->ext);
	default:
		/* XXX */
		return EOK;
	}

	return EINVAL;
}

/** Emit binary instructions for procedure.
 *
 * @param emit Binary instruction emitter
 * @param var Z80 IC variable
 * @return EOK on success or an error code
 */
static int z80_emit_proc(z80_emit_t *emit, z80ic_proc_t *proc)
{
	z80ic_lblock_entry_t *entry;
	uint32_t offset;
	uint32_t size;
	obj_symbol_t *symbol;
	int rc;

	offset = emit->section->len;

	entry = z80ic_lblock_first(proc->lblock);
	while (entry != NULL) {
		if (entry->label != NULL) {
			rc = obj_symbol_create(emit->object, entry->label,
			    emit->section, emit->section->len, 0, &symbol);
			if (rc != EOK)
				return rc;
		}

		if (entry->instr != NULL) {
			rc = z80_emit_instr(emit, entry->instr);
			if (rc != EOK)
				return rc;
		}

		entry = z80ic_lblock_next(entry);
	}

	size = emit->section->len - offset;
	rc = obj_symbol_create(emit->object, proc->ident, emit->section,
	    offset, size, &symbol);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Emit binary instructions for declaration.
 *
 * @param emit Binary instruction emitter
 * @param decln Z80 IC declaration
 * @return EOK on success or an error code
 */
static int z80_emit_decln(z80_emit_t *emit, z80ic_decln_t *decln)
{
	int rc = EINVAL;

	(void)emit;
	(void)decln;

	switch (decln->dtype) {
	case z80icd_extern:
	case z80icd_global:
		rc = EOK;
		break;
	case z80icd_var:
		rc = z80_emit_var(emit, (z80ic_var_t *)decln->ext);
		break;
	case z80icd_proc:
		rc = z80_emit_proc(emit, (z80ic_proc_t *)decln->ext);
		break;
	}

	return rc;
}

/** Emit binary instructions for module.
 *
 * @param emit Binary instruction emitter
 * @param icmod Z80 IC module
 * @param robject Place to store pointer to new binary object
 * @return EOK on success or an error code
 */
int z80_emit_module(z80_emit_t *emit, z80ic_module_t *icmod,
    obj_object_t **robject)
{
	obj_object_t *object = NULL;
	obj_section_t *section = NULL;
	z80ic_decln_t *decln;
	int rc;

	rc = obj_object_create(&object);
	if (rc != EOK)
		goto error;

	emit->object = object;
	emit->ic_module = icmod;

	rc = obj_section_create(object, "common", &section);
	if (rc != EOK)
		goto error;

	emit->section = section;

	decln = z80ic_module_first(icmod);
	while (decln != NULL) {
		rc = z80_emit_decln(emit, decln);
		if (rc != EOK)
			goto error;

		decln = z80ic_module_next(decln);
	}

	emit->ic_module = NULL;
	*robject = object;
	return EOK;
error:
	obj_object_destroy(object);
	return rc;
}

/** Destroy binary instruction emitter.
 *
 * @param emit Instruction emitter or @c NULL
 */
void z80_emit_destroy(z80_emit_t *emit)
{
	if (emit == NULL)
		return;

	free(emit);
}
