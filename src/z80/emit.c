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
#include <string.h>
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

/** Emit 16-bit immediate.
 *
 * @param emit Binary instruction emitter.
 * @param opc Opcode including prefixes
 * @param imm16 16-bit immediate operand
 * @return EOK on success or an error code
 */
static int z80_emit_imm16(z80_emit_t *emit, z80ic_oper_imm16_t *imm16)
{
	z80ic_lvar_t *lvar;

	/* Is it just a constant? */
	if (imm16->symbol == NULL)
		return z80_emit_u16(emit, imm16->imm16);

	/* Is it a local variable reference? */
	lvar = z80ic_proc_first_lvar(emit->ic_proc);
	while (lvar != NULL) {
		if (strcmp(lvar->ident, imm16->symbol) == 0) {
			return z80_emit_u16(emit, lvar->off + imm16->imm16);
		}

		lvar = z80ic_proc_next_lvar(lvar);
	}

	/* It is a symbol reference. */
	return z80_emit_sa16(emit, imm16->symbol, imm16->imm16);
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
 * @param imm16 16-bit immediate operand
 * @return EOK on success or an error code
 */
static int z80_emit_opc_nn(z80_emit_t *emit, uint32_t opc,
    z80ic_oper_imm16_t *imm16)
{
	int rc;

	rc = z80_emit_opc(emit, opc);
	if (rc != EOK)
		return rc;

	return z80_emit_imm16(emit, imm16);
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
	    objb_local, offset, size, &symbol);
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

/** Emit binary load (IY+d) from register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_iiyd_r(z80_emit_t *emit, z80ic_ld_iiyd_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_iiyd_r | (uint8_t)instr->src->reg;
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

/** Emit binary load (IY+d) from 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_iiyd_n(z80_emit_t *emit, z80ic_ld_iiyd_n_t *instr)
{
	return z80_emit_opc_d_n(emit, z80opc_ld_iiyd_n, instr->disp,
	    instr->imm8);
}

/** Emit binary load A from (BC) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_a_ibc(z80_emit_t *emit, z80ic_ld_a_ibc_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_a_ibc);
}

/** Emit binary load A from (DE) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_a_ide(z80_emit_t *emit, z80ic_ld_a_ide_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_a_ide);
}

/** Emit binary load A from fixed memory location instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_a_inn(z80_emit_t *emit, z80ic_ld_a_inn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_a_inn, instr->imm16);
}

/** Emit binary load (BC) from A instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_ibc_a(z80_emit_t *emit, z80ic_ld_ibc_a_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_ibc_a);
}

/** Emit binary load (DE) from A instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_ide_a(z80_emit_t *emit, z80ic_ld_ide_a_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_ide_a);
}

/** Emit binary load fixed memory from A location instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_inn_a(z80_emit_t *emit, z80ic_ld_inn_a_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_inn_a, instr->imm16);
}

/** Emit binary load A from interrupt vector register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_a_i(z80_emit_t *emit, z80ic_ld_a_i_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_a_i);
}

/** Emit binary load A from refresh register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_a_r(z80_emit_t *emit, z80ic_ld_a_r_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_a_r);
}

/** Emit binary load A from interrupt vector register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_i_a(z80_emit_t *emit, z80ic_ld_i_a_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_i_a);
}

/** Emit binary load A from refresh register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_r_a(z80_emit_t *emit, z80ic_ld_r_a_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_r_a);
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

/** Emit binary load IY from 16-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_iy_nn(z80_emit_t *emit, z80ic_ld_iy_nn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_iy_nn, instr->imm16);
}

/** Emit binary load HL from fixed memory location instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_hl_inn(z80_emit_t *emit, z80ic_ld_hl_inn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_hl_inn, instr->imm16);
}

/** Emit binary load register pair from fixed memory location instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_dd_inn(z80_emit_t *emit, z80ic_ld_dd_inn_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_dd_inn | ((uint8_t)instr->dest->rdd << 4);
	return z80_emit_opc_nn(emit, opc, instr->imm16);
}

/** Emit binary load IX from fixed memory location instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_ix_inn(z80_emit_t *emit, z80ic_ld_ix_inn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_ix_inn, instr->imm16);
}

/** Emit binary load IY from fixed memory location instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_iy_inn(z80_emit_t *emit, z80ic_ld_iy_inn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_iy_inn, instr->imm16);
}

/** Emit binary load fixed memory location from HL instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_inn_hl(z80_emit_t *emit, z80ic_ld_inn_hl_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_inn_hl, instr->imm16);
}

/** Emit binary load fixed memory location from 16-bit dd register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_inn_dd(z80_emit_t *emit, z80ic_ld_inn_dd_t *instr)
{
	uint32_t opc;

	opc = z80opc_ld_inn_dd | ((uint8_t)instr->src->rdd << 4);
	return z80_emit_opc_nn(emit, opc, instr->imm16);
}

/** Emit binary load fixed memory location from IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_inn_ix(z80_emit_t *emit, z80ic_ld_inn_ix_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_inn_ix, instr->imm16);
}

/** Emit binary load fixed memory location from IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_inn_iy(z80_emit_t *emit, z80ic_ld_inn_iy_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_ld_inn_iy, instr->imm16);
}

/** Emit binary load SP from HL instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_sp_hl(z80_emit_t *emit, z80ic_ld_sp_hl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_sp_hl);
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

/** Emit binary load SP from IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ld_sp_iy(z80_emit_t *emit, z80ic_ld_sp_iy_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ld_sp_iy);
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

/** Emit binary push IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_push_iy(z80_emit_t *emit, z80ic_push_iy_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_push_iy);
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

/** Emit binary pop IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_pop_iy(z80_emit_t *emit, z80ic_pop_iy_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_pop_iy);
}

/** Emit binary exchange DE and HL instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ex_de_hl(z80_emit_t *emit, z80ic_ex_de_hl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ex_de_hl);
}

/** Emit binary exchange AF and AF' instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ex_af_afp(z80_emit_t *emit, z80ic_ex_af_afp_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ex_af_afp);
}

/** Emit binary exchange BC, DE, HL with BC', DE', HL' instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_exx(z80_emit_t *emit, z80ic_exx_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_exx);
}

/** Emit binary exchange (SP) with HL instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ex_isp_hl(z80_emit_t *emit, z80ic_ex_isp_hl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ex_isp_hl);
}

/** Emit binary exchange (SP) with IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ex_isp_ix(z80_emit_t *emit, z80ic_ex_isp_ix_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ex_isp_ix);
}

/** Emit binary exchange (SP) with IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ex_isp_iy(z80_emit_t *emit, z80ic_ex_isp_iy_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ex_isp_iy);
}

/** Emit binary load, increment instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ldi(z80_emit_t *emit, z80ic_ldi_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ldi);
}

/** Emit binary load, increment, repeat instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ldir(z80_emit_t *emit, z80ic_ldir_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ldir);
}

/** Emit binary load, decrement instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ldd(z80_emit_t *emit, z80ic_ldd_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ldd);
}

/** Emit binary load, decrement, repeat instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_lddr(z80_emit_t *emit, z80ic_lddr_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_lddr);
}

/** Emit binary compare, increment instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cpi(z80_emit_t *emit, z80ic_cpi_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_cpi);
}

/** Emit binary compare, increment, repeat instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cpir(z80_emit_t *emit, z80ic_cpir_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_cpir);
}

/** Emit binary compare, decrement instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cpd(z80_emit_t *emit, z80ic_cpd_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_cpd);
}

/** Emit binary compare, decrement, repeat instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cpdr(z80_emit_t *emit, z80ic_cpdr_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_cpdr);
}

/** Emit binary add register to A instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_a_r(z80_emit_t *emit, z80ic_add_a_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_add_a_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary add 8-bit immediate to A instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_a_n(z80_emit_t *emit, z80ic_add_a_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_add_a_n, instr->imm8);
}

/** Emit binary add (HL) to A instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_a_ihl(z80_emit_t *emit, z80ic_add_a_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_add_a_ihl);
}

/** Emit binary add (IX+d) to A instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_a_iixd(z80_emit_t *emit, z80ic_add_a_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_add_a_iixd, instr->disp);
}

/** Emit binary add (IY+d) to A instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_a_iiyd(z80_emit_t *emit, z80ic_add_a_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_add_a_iiyd, instr->disp);
}

/** Emit binary add register to A with carry instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_adc_a_r(z80_emit_t *emit, z80ic_adc_a_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_adc_a_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary add 8-bit immediate to A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_adc_a_n(z80_emit_t *emit, z80ic_adc_a_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_adc_a_n, instr->imm8);
}

/** Emit binary add (HL) to A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_adc_a_ihl(z80_emit_t *emit, z80ic_adc_a_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_adc_a_ihl);
}

/** Emit binary add (IX+d) to A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_adc_a_iixd(z80_emit_t *emit, z80ic_adc_a_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_adc_a_iixd, instr->disp);
}

/** Emit binary add (IY+d) to A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_adc_a_iiyd(z80_emit_t *emit, z80ic_adc_a_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_adc_a_iiyd, instr->disp);
}

/** Emit binary subtract register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sub_r(z80_emit_t *emit, z80ic_sub_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_sub_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary subtract 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sub_n(z80_emit_t *emit, z80ic_sub_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_sub_n, instr->imm8);
}

/** Emit binary subtract (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sub_ihl(z80_emit_t *emit, z80ic_sub_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_sub_ihl);
}

/** Emit binary subtract (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sub_iixd(z80_emit_t *emit, z80ic_sub_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sub_iixd, instr->disp);
}

/** Emit binary subtract (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sub_iiyd(z80_emit_t *emit, z80ic_sub_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sub_iiyd, instr->disp);
}

/** Emit binary subtract register from A with carry instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sbc_a_r(z80_emit_t *emit, z80ic_sbc_a_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_sbc_a_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary subtract 8-bit immediate from A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sbc_a_n(z80_emit_t *emit, z80ic_sbc_a_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_sbc_a_n, instr->imm8);
}

/** Emit binary subtract (HL) from A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sbc_a_ihl(z80_emit_t *emit, z80ic_sbc_a_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_sbc_a_ihl);
}

/** Emit binary subtract (IX+d) from A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sbc_a_iixd(z80_emit_t *emit, z80ic_sbc_a_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sbc_a_iixd, instr->disp);
}

/** Emit binary subtract (IY+d) from A instruction with carry.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sbc_a_iiyd(z80_emit_t *emit, z80ic_sbc_a_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sbc_a_iiyd, instr->disp);
}

/** Emit binary bitwise AND with register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_and_r(z80_emit_t *emit, z80ic_and_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_and_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary bitwise AND with 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_and_n(z80_emit_t *emit, z80ic_and_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_and_n, instr->imm8);
}

/** Emit binary bitwise AND with (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_and_ihl(z80_emit_t *emit, z80ic_and_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_and_ihl);
}

/** Emit binary bitwise AND with (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_and_iixd(z80_emit_t *emit, z80ic_and_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_and_iixd, instr->disp);
}

/** Emit binary bitwise AND with (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_and_iiyd(z80_emit_t *emit, z80ic_and_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_and_iiyd, instr->disp);
}

/** Emit binary bitwise OR with register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_or_r(z80_emit_t *emit, z80ic_or_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_or_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary bitwise OR with 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_or_n(z80_emit_t *emit, z80ic_or_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_or_n, instr->imm8);
}

/** Emit binary bitwise OR with (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_or_ihl(z80_emit_t *emit, z80ic_or_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_or_ihl);
}

/** Emit binary bitwise OR with (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_or_iixd(z80_emit_t *emit, z80ic_or_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_or_iixd, instr->disp);
}

/** Emit binary bitwise OR with (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_or_iiyd(z80_emit_t *emit, z80ic_or_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_or_iiyd, instr->disp);
}

/** Emit binary bitwise XOR with register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_xor_r(z80_emit_t *emit, z80ic_xor_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_xor_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary bitwise XOR with 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_xor_n(z80_emit_t *emit, z80ic_xor_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_xor_n, instr->imm8);
}

/** Emit binary bitwise XOR with (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_xor_ihl(z80_emit_t *emit, z80ic_xor_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_xor_ihl);
}

/** Emit binary bitwise XOR with (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_xor_iixd(z80_emit_t *emit, z80ic_xor_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_xor_iixd, instr->disp);
}

/** Emit binary bitwise XOR with (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_xor_iiyd(z80_emit_t *emit, z80ic_xor_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_xor_iiyd, instr->disp);
}

/** Emit binary compare with register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cp_r(z80_emit_t *emit, z80ic_cp_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_cp_r | (uint8_t)instr->src->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary compare with 8-bit immediate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cp_n(z80_emit_t *emit, z80ic_cp_n_t *instr)
{
	return z80_emit_opc_n(emit, z80opc_cp_n, instr->imm8);
}

/** Emit binary compare with (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cp_ihl(z80_emit_t *emit, z80ic_cp_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_cp_ihl);
}

/** Emit binary compare with (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cp_iixd(z80_emit_t *emit, z80ic_cp_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_cp_iixd, instr->disp);
}

/** Emit binary compare with (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cp_iiyd(z80_emit_t *emit, z80ic_cp_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_cp_iiyd, instr->disp);
}

/** Emit binary increment register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_inc_r(z80_emit_t *emit, z80ic_inc_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_inc_r | ((uint8_t)instr->dest->reg << 3);
	return z80_emit_opc(emit, opc);
}

/** Emit binary increment (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_inc_ihl(z80_emit_t *emit, z80ic_inc_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_inc_ihl);
}

/** Emit binary increment (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_inc_iixd(z80_emit_t *emit, z80ic_inc_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_inc_iixd, instr->disp);
}

/** Emit binary increment (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_inc_iiyd(z80_emit_t *emit, z80ic_inc_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_inc_iiyd, instr->disp);
}

/** Emit binary decrement register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_dec_r(z80_emit_t *emit, z80ic_dec_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_dec_r | ((uint8_t)instr->dest->reg << 3);
	return z80_emit_opc(emit, opc);
}

/** Emit binary decrement (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_dec_ihl(z80_emit_t *emit, z80ic_dec_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_dec_ihl);
}

/** Emit binary decrement (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_dec_iixd(z80_emit_t *emit, z80ic_dec_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_dec_iixd, instr->disp);
}

/** Emit binary decrement (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_dec_iiyd(z80_emit_t *emit, z80ic_dec_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_dec_iiyd, instr->disp);
}

/** Emit binary decimal adjust accumulator instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_daa(z80_emit_t *emit, z80ic_daa_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_daa);
}

/** Emit binary complement instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_cpl(z80_emit_t *emit, z80ic_cpl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_cpl);
}

/** Emit binary negate instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_neg(z80_emit_t *emit, z80ic_neg_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_neg);
}

/** Emit binary complement carry flag instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ccf(z80_emit_t *emit, z80ic_ccf_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ccf);
}

/** Emit binary set carry flag instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_scf(z80_emit_t *emit, z80ic_scf_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_scf);
}

/** Emit binary no operation instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_nop(z80_emit_t *emit, z80ic_nop_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_nop);
}

/** Emit binary halt instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_halt(z80_emit_t *emit, z80ic_halt_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_halt);
}

/** Emit binary disable interrupt instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_di(z80_emit_t *emit, z80ic_di_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_di);
}

/** Emit binary enable interrupt instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_ei(z80_emit_t *emit, z80ic_ei_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_ei);
}

/** Emit binary set interrupt mode 0 instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_im_0(z80_emit_t *emit, z80ic_im_0_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_im_0);
}

/** Emit binary set interrupt mode 1 instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_im_1(z80_emit_t *emit, z80ic_im_1_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_im_1);
}

/** Emit binary set interrupt mode 2 instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_im_2(z80_emit_t *emit, z80ic_im_2_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_im_2);
}

/** Emit binary add register pair to HL instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_hl_ss(z80_emit_t *emit, z80ic_add_hl_ss_t *instr)
{
	uint32_t opc;

	opc = z80opc_add_hl_ss | ((uint8_t)instr->src->rss << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary add register pair to HL with carry instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_adc_hl_ss(z80_emit_t *emit, z80ic_adc_hl_ss_t *instr)
{
	uint32_t opc;

	opc = z80opc_adc_hl_ss | ((uint8_t)instr->src->rss << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary decrement register pair from HL with carry instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sbc_hl_ss(z80_emit_t *emit, z80ic_sbc_hl_ss_t *instr)
{
	uint32_t opc;

	opc = z80opc_sbc_hl_ss | ((uint8_t)instr->src->rss << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary add register pair to IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_ix_pp(z80_emit_t *emit, z80ic_add_ix_pp_t *instr)
{
	uint32_t opc;

	opc = z80opc_add_ix_pp | ((uint8_t)instr->src->rpp << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary add register pair to IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_add_iy_rr(z80_emit_t *emit, z80ic_add_iy_rr_t *instr)
{
	uint32_t opc;

	opc = z80opc_add_iy_rr | ((uint8_t)instr->src->rrr << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary increment register pair instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_inc_ss(z80_emit_t *emit, z80ic_inc_ss_t *instr)
{
	uint32_t opc;

	opc = z80opc_inc_ss | ((uint8_t)instr->dest->rss << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary increment IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_inc_ix(z80_emit_t *emit, z80ic_inc_ix_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_inc_ix);
}

/** Emit binary increment IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_inc_iy(z80_emit_t *emit, z80ic_inc_iy_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_inc_iy);
}

/** Emit binary decrement register pair instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_dec_ss(z80_emit_t *emit, z80ic_dec_ss_t *instr)
{
	uint32_t opc;

	opc = z80opc_dec_ss | ((uint8_t)instr->dest->rss << 4);
	return z80_emit_opc(emit, opc);
}

/** Emit binary decrement IX instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_dec_ix(z80_emit_t *emit, z80ic_dec_ix_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_dec_ix);
}

/** Emit binary decrement IY instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_dec_iy(z80_emit_t *emit, z80ic_dec_iy_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_dec_iy);
}

/** Emit binary rotate left circular accumulator instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rlca(z80_emit_t *emit, z80ic_rlca_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rlca);
}

/** Emit binary rotate left accumulator instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rla(z80_emit_t *emit, z80ic_rla_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rla);
}

/** Emit binary rotate right circular accumulator instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rrca(z80_emit_t *emit, z80ic_rrca_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rrca);
}

/** Emit binary rotate right accumulator instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rra(z80_emit_t *emit, z80ic_rra_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rra);
}

/** Emit binary rotate left circular register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rlc_r(z80_emit_t *emit, z80ic_rlc_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_rlc_r | (uint8_t)instr->dest->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary rotate left circular (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rlc_ihl(z80_emit_t *emit, z80ic_rlc_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rlc_ihl);
}

/** Emit binary rotate left circular (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rlc_iixd(z80_emit_t *emit, z80ic_rlc_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rlc_iixd, instr->disp);
}

/** Emit binary rotate left circular (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rlc_iiyd(z80_emit_t *emit, z80ic_rlc_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rlc_iiyd, instr->disp);
}

/** Emit binary rotate left register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rl_r(z80_emit_t *emit, z80ic_rl_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_rl_r | (uint8_t)instr->dest->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary rotate left (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rl_ihl(z80_emit_t *emit, z80ic_rl_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rl_ihl);
}

/** Emit binary rotate left (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rl_iixd(z80_emit_t *emit, z80ic_rl_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rl_iixd, instr->disp);
}

/** Emit binary rotate left (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rl_iiyd(z80_emit_t *emit, z80ic_rl_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rl_iiyd, instr->disp);
}

/** Emit binary rotate right circular register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rrc_r(z80_emit_t *emit, z80ic_rrc_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_rrc_r | (uint8_t)instr->dest->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary rotate right circular (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rrc_ihl(z80_emit_t *emit, z80ic_rrc_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rrc_ihl);
}

/** Emit binary rotate right circular (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rrc_iixd(z80_emit_t *emit, z80ic_rrc_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rrc_iixd, instr->disp);
}

/** Emit binary rotate right circular (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rrc_iiyd(z80_emit_t *emit, z80ic_rrc_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rrc_iiyd, instr->disp);
}

/** Emit binary rotate right register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rr_r(z80_emit_t *emit, z80ic_rr_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_rr_r | (uint8_t)instr->dest->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary rotate right (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rr_ihl(z80_emit_t *emit, z80ic_rr_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rr_ihl);
}

/** Emit binary rotate right (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rr_iixd(z80_emit_t *emit, z80ic_rr_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rr_iixd, instr->disp);
}

/** Emit binary rotate right (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rr_iiyd(z80_emit_t *emit, z80ic_rr_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_rr_iiyd, instr->disp);
}

/** Emit binary shift left arithmetic register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sla_r(z80_emit_t *emit, z80ic_sla_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_sla_r | (uint8_t)instr->dest->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary shift left arithmetic (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sla_ihl(z80_emit_t *emit, z80ic_sla_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_sla_ihl);
}

/** Emit binary shift left arithmetic (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sla_iixd(z80_emit_t *emit, z80ic_sla_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sla_iixd, instr->disp);
}

/** Emit binary shift left arithmetic (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sla_iiyd(z80_emit_t *emit, z80ic_sla_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sla_iiyd, instr->disp);
}

/** Emit binary shift right arithmetic register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sra_r(z80_emit_t *emit, z80ic_sra_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_sra_r | (uint8_t)instr->dest->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary shift right arithmetic (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sra_ihl(z80_emit_t *emit, z80ic_sra_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_sra_ihl);
}

/** Emit binary shift right arithmetic (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sra_iixd(z80_emit_t *emit, z80ic_sra_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sra_iixd, instr->disp);
}

/** Emit binary shift right arithmetic (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_sra_iiyd(z80_emit_t *emit, z80ic_sra_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_sra_iiyd, instr->disp);
}

/** Emit binary shift right logical register instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_srl_r(z80_emit_t *emit, z80ic_srl_r_t *instr)
{
	uint32_t opc;

	opc = z80opc_srl_r | (uint8_t)instr->dest->reg;
	return z80_emit_opc(emit, opc);
}

/** Emit binary shift right logical (HL) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_srl_ihl(z80_emit_t *emit, z80ic_srl_ihl_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_srl_ihl);
}

/** Emit binary shift right logical (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_srl_iixd(z80_emit_t *emit, z80ic_srl_iixd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_srl_iixd, instr->disp);
}

/** Emit binary shift right logical (IY+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_srl_iiyd(z80_emit_t *emit, z80ic_srl_iiyd_t *instr)
{
	return z80_emit_opc_d(emit, z80opc_srl_iiyd, instr->disp);
}

/** Emit binary rotate left digit instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rld(z80_emit_t *emit, z80ic_rld_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rld);
}

/** Emit binary rotate right digit instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_rrd(z80_emit_t *emit, z80ic_rrd_t *instr)
{
	(void)instr;
	return z80_emit_opc(emit, z80opc_rrd);
}

/** Emit binary test bit b in (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_bit_b_iixd(z80_emit_t *emit, z80ic_bit_b_iixd_t *instr)
{
	uint32_t opc;

	opc = z80opc_bit_b_iixd | (instr->bit << 3);
	return z80_emit_opc_d(emit, opc, instr->disp);
}

/** Emit binary set bit b in (IX+d) instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_set_b_iixd(z80_emit_t *emit, z80ic_set_b_iixd_t *instr)
{
	uint32_t opc;

	opc = z80opc_set_b_iixd | (instr->bit << 3);
	return z80_emit_opc_d(emit, opc, instr->disp);
}

/** Emit binary jump to address instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_jp_nn(z80_emit_t *emit, z80ic_jp_nn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_jp_nn, instr->imm16);
}

/** Emit binary conditional jump to address instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_jp_cc_nn(z80_emit_t *emit, z80ic_jp_cc_nn_t *instr)
{
	uint32_t opc;

	opc = z80opc_jp_cc_nn | ((uint8_t)instr->cc << 3);
	return z80_emit_opc_nn(emit, opc, instr->imm16);
}

/** Emit binary call address instruction.
 *
 * @param emit Binary instruction emitter
 * @param instr Z80 IC instruction
 * @return EOK on success or an error code
 */
static int z80_emit_call_nn(z80_emit_t *emit, z80ic_call_nn_t *instr)
{
	return z80_emit_opc_nn(emit, z80opc_call_nn, instr->imm16);
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
	case z80i_ld_iiyd_r:
		return z80_emit_ld_iiyd_r(emit,
		    (z80ic_ld_iiyd_r_t *)instr->ext);
	case z80i_ld_ihl_n:
		return z80_emit_ld_ihl_n(emit, (z80ic_ld_ihl_n_t *)instr->ext);
	case z80i_ld_iixd_n:
		return z80_emit_ld_iixd_n(emit,
		    (z80ic_ld_iixd_n_t *)instr->ext);
	case z80i_ld_iiyd_n:
		return z80_emit_ld_iiyd_n(emit,
		    (z80ic_ld_iiyd_n_t *)instr->ext);
	case z80i_ld_a_ibc:
		return z80_emit_ld_a_ibc(emit, (z80ic_ld_a_ibc_t *)instr->ext);
	case z80i_ld_a_ide:
		return z80_emit_ld_a_ide(emit, (z80ic_ld_a_ide_t *)instr->ext);
	case z80i_ld_a_inn:
		return z80_emit_ld_a_inn(emit, (z80ic_ld_a_inn_t *)instr->ext);
	case z80i_ld_ibc_a:
		return z80_emit_ld_ibc_a(emit, (z80ic_ld_ibc_a_t *)instr->ext);
	case z80i_ld_ide_a:
		return z80_emit_ld_ide_a(emit, (z80ic_ld_ide_a_t *)instr->ext);
	case z80i_ld_inn_a:
		return z80_emit_ld_inn_a(emit, (z80ic_ld_inn_a_t *)instr->ext);
	case z80i_ld_a_i:
		return z80_emit_ld_a_i(emit, (z80ic_ld_a_i_t *)instr->ext);
	case z80i_ld_a_r:
		return z80_emit_ld_a_r(emit, (z80ic_ld_a_r_t *)instr->ext);
	case z80i_ld_i_a:
		return z80_emit_ld_i_a(emit, (z80ic_ld_i_a_t *)instr->ext);
	case z80i_ld_r_a:
		return z80_emit_ld_r_a(emit, (z80ic_ld_r_a_t *)instr->ext);
	case z80i_ld_dd_nn:
		return z80_emit_ld_dd_nn(emit, (z80ic_ld_dd_nn_t *)instr->ext);
	case z80i_ld_ix_nn:
		return z80_emit_ld_ix_nn(emit, (z80ic_ld_ix_nn_t *)instr->ext);
	case z80i_ld_iy_nn:
		return z80_emit_ld_iy_nn(emit, (z80ic_ld_iy_nn_t *)instr->ext);
	case z80i_ld_hl_inn:
		return z80_emit_ld_hl_inn(emit,
		    (z80ic_ld_hl_inn_t *)instr->ext);
	case z80i_ld_dd_inn:
		return z80_emit_ld_dd_inn(emit,
		    (z80ic_ld_dd_inn_t *)instr->ext);
	case z80i_ld_ix_inn:
		return z80_emit_ld_ix_inn(emit,
		    (z80ic_ld_ix_inn_t *)instr->ext);
	case z80i_ld_iy_inn:
		return z80_emit_ld_iy_inn(emit,
		    (z80ic_ld_iy_inn_t *)instr->ext);
	case z80i_ld_inn_hl:
		return z80_emit_ld_inn_hl(emit,
		    (z80ic_ld_inn_hl_t *)instr->ext);
	case z80i_ld_inn_dd:
		return z80_emit_ld_inn_dd(emit,
		    (z80ic_ld_inn_dd_t *)instr->ext);
	case z80i_ld_inn_ix:
		return z80_emit_ld_inn_ix(emit,
		    (z80ic_ld_inn_ix_t *)instr->ext);
	case z80i_ld_inn_iy:
		return z80_emit_ld_inn_iy(emit,
		    (z80ic_ld_inn_iy_t *)instr->ext);
	case z80i_ld_sp_hl:
		return z80_emit_ld_sp_hl(emit, (z80ic_ld_sp_hl_t *)instr->ext);
	case z80i_ld_sp_ix:
		return z80_emit_ld_sp_ix(emit, (z80ic_ld_sp_ix_t *)instr->ext);
	case z80i_ld_sp_iy:
		return z80_emit_ld_sp_iy(emit, (z80ic_ld_sp_iy_t *)instr->ext);
	case z80i_push_qq:
		return z80_emit_push_qq(emit, (z80ic_push_qq_t *)instr->ext);
	case z80i_push_ix:
		return z80_emit_push_ix(emit, (z80ic_push_ix_t *)instr->ext);
	case z80i_push_iy:
		return z80_emit_push_iy(emit, (z80ic_push_iy_t *)instr->ext);
	case z80i_pop_qq:
		return z80_emit_pop_qq(emit, (z80ic_pop_qq_t *)instr->ext);
	case z80i_pop_ix:
		return z80_emit_pop_ix(emit, (z80ic_pop_ix_t *)instr->ext);
	case z80i_pop_iy:
		return z80_emit_pop_iy(emit, (z80ic_pop_iy_t *)instr->ext);
	case z80i_ex_de_hl:
		return z80_emit_ex_de_hl(emit, (z80ic_ex_de_hl_t *)instr->ext);
	case z80i_ex_af_afp:
		return z80_emit_ex_af_afp(emit, (z80ic_ex_af_afp_t *)instr->ext);
	case z80i_exx:
		return z80_emit_exx(emit, (z80ic_exx_t *)instr->ext);
	case z80i_ex_isp_hl:
		return z80_emit_ex_isp_hl(emit, (z80ic_ex_isp_hl_t *)instr->ext);
	case z80i_ex_isp_ix:
		return z80_emit_ex_isp_ix(emit, (z80ic_ex_isp_ix_t *)instr->ext);
	case z80i_ex_isp_iy:
		return z80_emit_ex_isp_iy(emit, (z80ic_ex_isp_iy_t *)instr->ext);
	case z80i_ldi:
		return z80_emit_ldi(emit, (z80ic_ldi_t *)instr->ext);
	case z80i_ldir:
		return z80_emit_ldir(emit, (z80ic_ldir_t *)instr->ext);
	case z80i_ldd:
		return z80_emit_ldd(emit, (z80ic_ldd_t *)instr->ext);
	case z80i_lddr:
		return z80_emit_lddr(emit, (z80ic_lddr_t *)instr->ext);
	case z80i_cpi:
		return z80_emit_cpi(emit, (z80ic_cpi_t *)instr->ext);
	case z80i_cpir:
		return z80_emit_cpir(emit, (z80ic_cpir_t *)instr->ext);
	case z80i_cpd:
		return z80_emit_cpd(emit, (z80ic_cpd_t *)instr->ext);
	case z80i_cpdr:
		return z80_emit_cpdr(emit, (z80ic_cpdr_t *)instr->ext);
	case z80i_add_a_r:
		return z80_emit_add_a_r(emit, (z80ic_add_a_r_t *)instr->ext);
	case z80i_add_a_n:
		return z80_emit_add_a_n(emit, (z80ic_add_a_n_t *)instr->ext);
	case z80i_add_a_ihl:
		return z80_emit_add_a_ihl(emit,
		    (z80ic_add_a_ihl_t *)instr->ext);
	case z80i_add_a_iixd:
		return z80_emit_add_a_iixd(emit,
		    (z80ic_add_a_iixd_t *)instr->ext);
	case z80i_add_a_iiyd:
		return z80_emit_add_a_iiyd(emit,
		    (z80ic_add_a_iiyd_t *)instr->ext);
	case z80i_adc_a_r:
		return z80_emit_adc_a_r(emit, (z80ic_adc_a_r_t *)instr->ext);
	case z80i_adc_a_n:
		return z80_emit_adc_a_n(emit, (z80ic_adc_a_n_t *)instr->ext);
	case z80i_adc_a_ihl:
		return z80_emit_adc_a_ihl(emit,
		    (z80ic_adc_a_ihl_t *)instr->ext);
	case z80i_adc_a_iixd:
		return z80_emit_adc_a_iixd(emit,
		    (z80ic_adc_a_iixd_t *)instr->ext);
	case z80i_adc_a_iiyd:
		return z80_emit_adc_a_iiyd(emit,
		    (z80ic_adc_a_iiyd_t *)instr->ext);
	case z80i_sub_r:
		return z80_emit_sub_r(emit, (z80ic_sub_r_t *)instr->ext);
	case z80i_sub_n:
		return z80_emit_sub_n(emit, (z80ic_sub_n_t *)instr->ext);
	case z80i_sub_ihl:
		return z80_emit_sub_ihl(emit,
		    (z80ic_sub_ihl_t *)instr->ext);
	case z80i_sub_iixd:
		return z80_emit_sub_iixd(emit,
		    (z80ic_sub_iixd_t *)instr->ext);
	case z80i_sub_iiyd:
		return z80_emit_sub_iiyd(emit,
		    (z80ic_sub_iiyd_t *)instr->ext);
	case z80i_sbc_a_r:
		return z80_emit_sbc_a_r(emit, (z80ic_sbc_a_r_t *)instr->ext);
	case z80i_sbc_a_n:
		return z80_emit_sbc_a_n(emit, (z80ic_sbc_a_n_t *)instr->ext);
	case z80i_sbc_a_ihl:
		return z80_emit_sbc_a_ihl(emit,
		    (z80ic_sbc_a_ihl_t *)instr->ext);
	case z80i_sbc_a_iixd:
		return z80_emit_sbc_a_iixd(emit,
		    (z80ic_sbc_a_iixd_t *)instr->ext);
	case z80i_sbc_a_iiyd:
		return z80_emit_sbc_a_iiyd(emit,
		    (z80ic_sbc_a_iiyd_t *)instr->ext);
	case z80i_and_r:
		return z80_emit_and_r(emit, (z80ic_and_r_t *)instr->ext);
	case z80i_and_n:
		return z80_emit_and_n(emit, (z80ic_and_n_t *)instr->ext);
	case z80i_and_ihl:
		return z80_emit_and_ihl(emit,
		    (z80ic_and_ihl_t *)instr->ext);
	case z80i_and_iixd:
		return z80_emit_and_iixd(emit,
		    (z80ic_and_iixd_t *)instr->ext);
	case z80i_and_iiyd:
		return z80_emit_and_iiyd(emit,
		    (z80ic_and_iiyd_t *)instr->ext);
	case z80i_or_r:
		return z80_emit_or_r(emit, (z80ic_or_r_t *)instr->ext);
	case z80i_or_n:
		return z80_emit_or_n(emit, (z80ic_or_n_t *)instr->ext);
	case z80i_or_ihl:
		return z80_emit_or_ihl(emit,
		    (z80ic_or_ihl_t *)instr->ext);
	case z80i_or_iixd:
		return z80_emit_or_iixd(emit,
		    (z80ic_or_iixd_t *)instr->ext);
	case z80i_or_iiyd:
		return z80_emit_or_iiyd(emit,
		    (z80ic_or_iiyd_t *)instr->ext);
		return z80_emit_cpl(emit, (z80ic_cpl_t *)instr->ext);
	case z80i_xor_r:
		return z80_emit_xor_r(emit, (z80ic_xor_r_t *)instr->ext);
	case z80i_xor_n:
		return z80_emit_xor_n(emit, (z80ic_xor_n_t *)instr->ext);
	case z80i_xor_ihl:
		return z80_emit_xor_ihl(emit,
		    (z80ic_xor_ihl_t *)instr->ext);
	case z80i_xor_iixd:
		return z80_emit_xor_iixd(emit,
		    (z80ic_xor_iixd_t *)instr->ext);
	case z80i_xor_iiyd:
		return z80_emit_xor_iiyd(emit,
		    (z80ic_xor_iiyd_t *)instr->ext);
		return z80_emit_cpl(emit, (z80ic_cpl_t *)instr->ext);
	case z80i_cp_r:
		return z80_emit_cp_r(emit, (z80ic_cp_r_t *)instr->ext);
	case z80i_cp_n:
		return z80_emit_cp_n(emit, (z80ic_cp_n_t *)instr->ext);
	case z80i_cp_ihl:
		return z80_emit_cp_ihl(emit,
		    (z80ic_cp_ihl_t *)instr->ext);
	case z80i_cp_iixd:
		return z80_emit_cp_iixd(emit,
		    (z80ic_cp_iixd_t *)instr->ext);
	case z80i_cp_iiyd:
		return z80_emit_cp_iiyd(emit,
		    (z80ic_cp_iiyd_t *)instr->ext);
		return z80_emit_cpl(emit, (z80ic_cpl_t *)instr->ext);
	case z80i_inc_r:
		return z80_emit_inc_r(emit, (z80ic_inc_r_t *)instr->ext);
	case z80i_inc_ihl:
		return z80_emit_inc_ihl(emit,
		    (z80ic_inc_ihl_t *)instr->ext);
	case z80i_inc_iixd:
		return z80_emit_inc_iixd(emit,
		    (z80ic_inc_iixd_t *)instr->ext);
	case z80i_inc_iiyd:
		return z80_emit_inc_iiyd(emit,
		    (z80ic_inc_iiyd_t *)instr->ext);
		return z80_emit_cpl(emit, (z80ic_cpl_t *)instr->ext);
	case z80i_dec_r:
		return z80_emit_dec_r(emit, (z80ic_dec_r_t *)instr->ext);
	case z80i_dec_ihl:
		return z80_emit_dec_ihl(emit,
		    (z80ic_dec_ihl_t *)instr->ext);
	case z80i_dec_iixd:
		return z80_emit_dec_iixd(emit,
		    (z80ic_dec_iixd_t *)instr->ext);
	case z80i_dec_iiyd:
		return z80_emit_dec_iiyd(emit,
		    (z80ic_dec_iiyd_t *)instr->ext);
		return z80_emit_cpl(emit, (z80ic_cpl_t *)instr->ext);
	case z80i_daa:
		return z80_emit_daa(emit, (z80ic_daa_t *)instr->ext);
	case z80i_cpl:
		return z80_emit_cpl(emit, (z80ic_cpl_t *)instr->ext);
	case z80i_neg:
		return z80_emit_neg(emit, (z80ic_neg_t *)instr->ext);
	case z80i_ccf:
		return z80_emit_ccf(emit, (z80ic_ccf_t *)instr->ext);
	case z80i_scf:
		return z80_emit_scf(emit, (z80ic_scf_t *)instr->ext);
	case z80i_nop:
		return z80_emit_nop(emit, (z80ic_nop_t *)instr->ext);
	case z80i_halt:
		return z80_emit_halt(emit, (z80ic_halt_t *)instr->ext);
	case z80i_di:
		return z80_emit_di(emit, (z80ic_di_t *)instr->ext);
	case z80i_ei:
		return z80_emit_ei(emit, (z80ic_ei_t *)instr->ext);
	case z80i_im_0:
		return z80_emit_im_0(emit, (z80ic_im_0_t *)instr->ext);
	case z80i_im_1:
		return z80_emit_im_1(emit, (z80ic_im_1_t *)instr->ext);
	case z80i_im_2:
		return z80_emit_im_2(emit, (z80ic_im_2_t *)instr->ext);
	case z80i_add_hl_ss:
		return z80_emit_add_hl_ss(emit,
		    (z80ic_add_hl_ss_t *)instr->ext);
	case z80i_adc_hl_ss:
		return z80_emit_adc_hl_ss(emit,
		    (z80ic_adc_hl_ss_t *)instr->ext);
	case z80i_sbc_hl_ss:
		return z80_emit_sbc_hl_ss(emit,
		    (z80ic_sbc_hl_ss_t *)instr->ext);
	case z80i_add_ix_pp:
		return z80_emit_add_ix_pp(emit,
		    (z80ic_add_ix_pp_t *)instr->ext);
	case z80i_add_iy_rr:
		return z80_emit_add_iy_rr(emit,
		    (z80ic_add_iy_rr_t *)instr->ext);
	case z80i_inc_ss:
		return z80_emit_inc_ss(emit,
		    (z80ic_inc_ss_t *)instr->ext);
	case z80i_inc_ix:
		return z80_emit_inc_ix(emit,
		    (z80ic_inc_ix_t *)instr->ext);
	case z80i_inc_iy:
		return z80_emit_inc_iy(emit,
		    (z80ic_inc_iy_t *)instr->ext);
	case z80i_dec_ss:
		return z80_emit_dec_ss(emit,
		    (z80ic_dec_ss_t *)instr->ext);
	case z80i_dec_ix:
		return z80_emit_dec_ix(emit,
		    (z80ic_dec_ix_t *)instr->ext);
	case z80i_dec_iy:
		return z80_emit_dec_iy(emit,
		    (z80ic_dec_iy_t *)instr->ext);
	case z80i_rlca:
		return z80_emit_rlca(emit, (z80ic_rlca_t *)instr->ext);
	case z80i_rla:
		return z80_emit_rla(emit, (z80ic_rla_t *)instr->ext);
	case z80i_rrca:
		return z80_emit_rrca(emit, (z80ic_rrca_t *)instr->ext);
	case z80i_rra:
		return z80_emit_rra(emit, (z80ic_rra_t *)instr->ext);
	case z80i_rlc_r:
		return z80_emit_rlc_r(emit, (z80ic_rlc_r_t *)instr->ext);
	case z80i_rlc_ihl:
		return z80_emit_rlc_ihl(emit, (z80ic_rlc_ihl_t *)instr->ext);
	case z80i_rlc_iixd:
		return z80_emit_rlc_iixd(emit, (z80ic_rlc_iixd_t *)instr->ext);
	case z80i_rlc_iiyd:
		return z80_emit_rlc_iiyd(emit, (z80ic_rlc_iiyd_t *)instr->ext);
	case z80i_rl_r:
		return z80_emit_rl_r(emit, (z80ic_rl_r_t *)instr->ext);
	case z80i_rl_ihl:
		return z80_emit_rl_ihl(emit, (z80ic_rl_ihl_t *)instr->ext);
	case z80i_rl_iixd:
		return z80_emit_rl_iixd(emit, (z80ic_rl_iixd_t *)instr->ext);
	case z80i_rl_iiyd:
		return z80_emit_rl_iiyd(emit, (z80ic_rl_iiyd_t *)instr->ext);
	case z80i_rrc_r:
		return z80_emit_rrc_r(emit, (z80ic_rrc_r_t *)instr->ext);
	case z80i_rrc_ihl:
		return z80_emit_rrc_ihl(emit, (z80ic_rrc_ihl_t *)instr->ext);
	case z80i_rrc_iixd:
		return z80_emit_rrc_iixd(emit, (z80ic_rrc_iixd_t *)instr->ext);
	case z80i_rrc_iiyd:
		return z80_emit_rrc_iiyd(emit, (z80ic_rrc_iiyd_t *)instr->ext);
	case z80i_rr_r:
		return z80_emit_rr_r(emit, (z80ic_rr_r_t *)instr->ext);
	case z80i_rr_ihl:
		return z80_emit_rr_ihl(emit, (z80ic_rr_ihl_t *)instr->ext);
	case z80i_rr_iixd:
		return z80_emit_rr_iixd(emit, (z80ic_rr_iixd_t *)instr->ext);
	case z80i_rr_iiyd:
		return z80_emit_rr_iiyd(emit, (z80ic_rr_iiyd_t *)instr->ext);
	case z80i_sla_r:
		return z80_emit_sla_r(emit, (z80ic_sla_r_t *)instr->ext);
	case z80i_sla_ihl:
		return z80_emit_sla_ihl(emit, (z80ic_sla_ihl_t *)instr->ext);
	case z80i_sla_iixd:
		return z80_emit_sla_iixd(emit, (z80ic_sla_iixd_t *)instr->ext);
	case z80i_sla_iiyd:
		return z80_emit_sla_iiyd(emit, (z80ic_sla_iiyd_t *)instr->ext);
	case z80i_sra_r:
		return z80_emit_sra_r(emit, (z80ic_sra_r_t *)instr->ext);
	case z80i_sra_ihl:
		return z80_emit_sra_ihl(emit, (z80ic_sra_ihl_t *)instr->ext);
	case z80i_sra_iixd:
		return z80_emit_sra_iixd(emit, (z80ic_sra_iixd_t *)instr->ext);
	case z80i_sra_iiyd:
		return z80_emit_sra_iiyd(emit, (z80ic_sra_iiyd_t *)instr->ext);
	case z80i_srl_r:
		return z80_emit_srl_r(emit, (z80ic_srl_r_t *)instr->ext);
	case z80i_srl_ihl:
		return z80_emit_srl_ihl(emit, (z80ic_srl_ihl_t *)instr->ext);
	case z80i_srl_iixd:
		return z80_emit_srl_iixd(emit, (z80ic_srl_iixd_t *)instr->ext);
	case z80i_srl_iiyd:
		return z80_emit_srl_iiyd(emit, (z80ic_srl_iiyd_t *)instr->ext);
	case z80i_rld:
		return z80_emit_rld(emit, (z80ic_rld_t *)instr->ext);
	case z80i_rrd:
		return z80_emit_rrd(emit, (z80ic_rrd_t *)instr->ext);
	case z80i_bit_b_iixd:
		return z80_emit_bit_b_iixd(emit,
		    (z80ic_bit_b_iixd_t *)instr->ext);
	case z80i_set_b_iixd:
		return z80_emit_set_b_iixd(emit,
		    (z80ic_set_b_iixd_t *)instr->ext);
	case z80i_jp_nn:
		return z80_emit_jp_nn(emit, (z80ic_jp_nn_t *)instr->ext);
	case z80i_jp_cc_nn:
		return z80_emit_jp_cc_nn(emit, (z80ic_jp_cc_nn_t *)instr->ext);
	case z80i_call_nn:
		return z80_emit_call_nn(emit, (z80ic_call_nn_t *)instr->ext);
	case z80i_ret:
		return z80_emit_ret(emit, (z80ic_ret_t *)instr->ext);
	default:
		/* XXX */
		(void)fprintf(stderr, "Unsupported instruction.\n");
		abort();
		return EINVAL;
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

	emit->ic_proc = proc;
	offset = emit->section->len;

	entry = z80ic_lblock_first(proc->lblock);
	while (entry != NULL) {
		if (entry->label != NULL) {
			rc = obj_symbol_create(emit->object, entry->label,
			    emit->section, objb_local, emit->section->len, 0,
			    &symbol);
			if (rc != EOK)
				goto error;
		}

		if (entry->instr != NULL) {
			rc = z80_emit_instr(emit, entry->instr);
			if (rc != EOK)
				goto error;
		}

		entry = z80ic_lblock_next(entry);
	}

	size = emit->section->len - offset;
	rc = obj_symbol_create(emit->object, proc->ident, emit->section,
	    objb_local, offset, size, &symbol);
	if (rc != EOK)
		goto error;

	emit->ic_proc = NULL;
	return EOK;
error:
	emit->ic_proc = NULL;
	return rc;
}

/** Emit binary instructions global symbol export.
 *
 * @param emit Binary instruction emitter
 * @param global Global symbol export
 * @param modname Module name
 * @return EOK on success or an error code
 */
static int z80_emit_global(z80_emit_t *emit, z80ic_global_t *global,
    const char *modname)
{
	obj_symbol_t *symbol;

	symbol = obj_symbol_find(emit->object, global->ident,
	    modname);
	if (symbol == NULL)
		return EINVAL;

	symbol->binding = objb_global;
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
	case z80icd_global:
	case z80icd_extern:
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
 * @param modname Module name
 * @param robject Place to store pointer to new binary object
 * @return EOK on success or an error code
 */
int z80_emit_module(z80_emit_t *emit, z80ic_module_t *icmod,
    const char *modname, obj_object_t **robject)
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

	rc = obj_section_create(object, "common", modname, &section);
	if (rc != EOK)
		goto error;

	emit->section = section;

	/* Process variable and procedure declarations */
	decln = z80ic_module_first(icmod);
	while (decln != NULL) {
		rc = z80_emit_decln(emit, decln);
		if (rc != EOK)
			goto error;

		decln = z80ic_module_next(decln);
	}

	/* Process symbol exports once all symbols are defined. */
	decln = z80ic_module_first(icmod);
	while (decln != NULL) {
		if (decln->dtype == z80icd_global) {
			rc = z80_emit_global(emit,
			    (z80ic_global_t *)decln->ext, modname);
			if (rc != EOK)
				goto error;
		}

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
