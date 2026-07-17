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
 * Z80 binary instruction decoder
 *
 * Convert binary object to Z80 IC
 */

#include <assert.h>
#include <inttypes.h>
#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <types/z80/z80opc.h>
#include <object/object.h>
#include <object/reloc.h>
#include <object/section.h>
#include <object/symbol.h>
#include <z80/decode.h>
#include <z80/z80ic.h>

/** Create binary instruction decoder.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_decode_create(z80_decode_t **rdecode)
{
	z80_decode_t *decode;

	decode = calloc(1, sizeof(z80_decode_t));
	if (decode == NULL)
		return ENOMEM;

	*rdecode = decode;
	return EOK;

}

/** Get one instruction byte for decoding.
 *
 * @param decode Binary instruction decoder
 * @return Next byte in instruction stream
 */
static uint8_t z80_decode_get_u8(z80_decode_t *decode)
{
	uint8_t b;

	assert(decode->rem_bytes > 0);
	b = decode->section->data[decode->offset++];
	--decode->rem_bytes;

	return b;
}

/** Get one 16-bit word for decoding.
 *
 * @param decode Binary instruction decoder
 * @return Next 16-bit word in instruction stream
 */
static uint16_t z80_decode_get_u16le(z80_decode_t *decode)
{
	uint16_t w;

	w = z80_decode_get_u8(decode);
	w = w | ((uint16_t)z80_decode_get_u8(decode) << 8);
	return w;
}

/** Get destination register from opcode.
 *
 * @param opc 8-bit opcode
 * @return Destination register
 */
static z80ic_reg_t z80_decode_get_dreg(uint8_t opc)
{
	return (z80ic_reg_t)((opc >> 3) & 0x7);
}

/** Get source register from opcode.
 *
 * @param opc 8-bit opcode
 * @return Source register
 */
static z80ic_reg_t z80_decode_get_sreg(uint8_t opc)
{
	return (z80ic_reg_t)(opc & 0x7);
}

/** Get dd register from opcode.
 *
 * @param opc 8-bit opcode
 * @return 16-bit dd register
 */
static z80ic_dd_t z80_decode_get_dd(uint8_t opc)
{
	return (z80ic_dd_t)((opc >> 4) & 0x3);
}

/** Get pp register from opcode.
 *
 * @param opc 8-bit opcode
 * @return 16-bit qq register
 */
static z80ic_pp_t z80_decode_get_pp(uint8_t opc)
{
	return (z80ic_pp_t)((opc >> 4) & 0x3);
}

/** Get qq register from opcode.
 *
 * @param opc 8-bit opcode
 * @return 16-bit qq register
 */
static z80ic_qq_t z80_decode_get_qq(uint8_t opc)
{
	return (z80ic_qq_t)((opc >> 4) & 0x3);
}

/** Get rr register from opcode.
 *
 * @param opc 8-bit opcode
 * @return 16-bit qq register
 */
static z80ic_rr_t z80_decode_get_rr(uint8_t opc)
{
	return (z80ic_rr_t)((opc >> 4) & 0x3);
}

/** Get ss register from opcode.
 *
 * @param opc 8-bit opcode
 * @return 16-bit ss register
 */
static z80ic_ss_t z80_decode_get_ss(uint8_t opc)
{
	return (z80ic_ss_t)((opc >> 4) & 0x3);
}

/** Decode 8-bit immediate operand.
 *
 * @param decode Binary instruction decoder
 * @param rimm8 Place to store pointer to new operand
 * @return EOK on success or an error code
 */
static int z80_decode_imm8(z80_decode_t *decode, z80ic_oper_imm8_t **rimm8)
{
	uint8_t value;
	z80ic_oper_imm8_t *imm8;
	int rc;

	value = z80_decode_get_u8(decode);

	rc = z80ic_oper_imm8_create(value, &imm8);
	if (rc != EOK)
		return rc;

	*rimm8 = imm8;
	return EOK;
}

/** Decode 16-bit immediate operand.
 *
 * @param decode Binary instruction decoder
 * @param rimm16 Place to store pointer to new operand
 * @return EOK on success or an error code
 */
static int z80_decode_imm16(z80_decode_t *decode, z80ic_oper_imm16_t **rimm16)
{
	uint16_t value;
	z80ic_oper_imm16_t *imm16;
	int rc;

	value = z80_decode_get_u16le(decode);

	rc = z80ic_oper_imm16_create_val(value, &imm16);
	if (rc != EOK)
		return rc;

	*rimm16 = imm16;
	return EOK;
}

/** Decode load register from register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_r_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t dreg, sreg;
	z80ic_ld_r_r_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	dreg = z80_decode_get_dreg(opc);
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_ld_r_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode load register from 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_r_n(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t dreg;
	z80ic_ld_r_n_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	dreg = z80_decode_get_dreg(opc);

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_r_n_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode load register from (HL).
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_r_ihl(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t dreg;
	z80ic_ld_r_ihl_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	dreg = z80_decode_get_dreg(opc);

	rc = z80ic_ld_r_ihl_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode load register from (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_r_iixd(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t dreg;
	uint8_t disp;
	z80ic_ld_r_iixd_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	dreg = z80_decode_get_dreg(opc);
	disp = z80_decode_get_u8(decode);

	rc = z80ic_ld_r_iixd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->disp = disp;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode load register from (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_r_iiyd(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t dreg;
	uint8_t disp;
	z80ic_ld_r_iiyd_t *ld = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	dreg = z80_decode_get_dreg(opc);
	disp = z80_decode_get_u8(decode);

	rc = z80ic_ld_r_iiyd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->disp = disp;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode load (HL) from register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_ihl_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_ld_ihl_r_t *ld = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_ld_ihl_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->src = src;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode load (IX+d) from register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_iixd_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	uint8_t disp;
	z80ic_ld_iixd_r_t *ld = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_ld_iixd_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->src = src;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode load (IY+d) from register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_iiyd_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	uint8_t disp;
	z80ic_ld_iiyd_r_t *ld = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_ld_iiyd_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->src = src;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode load (HL) from 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_ihl_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_ihl_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_ihl_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode load (IX+d) from 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_iixd_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_ld_iixd_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_iixd_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode load (IY+d) from 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_iiyd_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_ld_iiyd_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_iiyd_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->disp = disp;
	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode load A from (BC).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_a_ibc(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_a_ibc_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_a_ibc_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load A from (DE).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_a_ide(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_a_ide_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_a_ide_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load A from fixed memory location.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_a_inn(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_a_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_a_inn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load (BC) from A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_ibc_a(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_ibc_a_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_ibc_a_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load (DE) from A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_ide_a(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_ide_a_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_ide_a_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load fixed memory location from A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_inn_a(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_inn_a_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_inn_a_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load A from interrupt vector register.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_a_i(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_a_i_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_a_i_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load A from memory refresh register.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_a_r(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_a_r_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_a_r_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load interrupt vector register from A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_i_a(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_i_a_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_i_a_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load memory refresh register from A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_r_a(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_a_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_r_a_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load 16-bit register from immediate.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_dd_nn(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_dd_t dd;
	z80ic_ld_dd_nn_t *ld = NULL;
	z80ic_oper_dd_t *dest = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	dd = z80_decode_get_dd(opc);

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_dd_nn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(dd, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_dd_destroy(dest);
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode load IX from 16-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_ix_nn(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_ix_nn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_ix_nn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load IY from 16-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_iy_nn(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_iy_nn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_iy_nn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load HL from fixed memory location.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_hl_inn(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_hl_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_hl_inn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load 16-bit register from fixed memory location.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_dd_inn(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_dd_t dd;
	z80ic_ld_dd_inn_t *ld = NULL;
	z80ic_oper_dd_t *dest = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	dd = z80_decode_get_dd(opc);

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_dd_inn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(dd, &dest);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_dd_destroy(dest);
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode load IX from fixed memory location.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_ix_inn(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_ix_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_ix_inn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load IY from fixed memory location.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_iy_inn(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_iy_inn_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_iy_inn_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load fixed memory location from HL.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_inn_hl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_inn_hl_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_inn_hl_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load fixed memory location from 16-bit register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_inn_dd(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_dd_t dd;
	z80ic_ld_inn_dd_t *ld = NULL;
	z80ic_oper_dd_t *src = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	dd = z80_decode_get_dd(opc);

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_inn_dd_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_dd_create(dd, &src);
	if (rc != EOK)
		goto error;

	ld->src = src;
	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_dd_destroy(src);
	z80ic_oper_imm16_destroy(imm16);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode load fixed memory location from IX.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_inn_ix(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_inn_ix_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_inn_ix_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load fixed memory location from IY.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_inn_iy(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_inn_iy_t *ld = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	int rc;

	rc = z80_decode_imm16(decode, &imm16);
	if (rc != EOK)
		goto error;

	rc = z80ic_ld_inn_iy_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm16 = imm16;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm16_destroy(imm16);
	return rc;
}

/** Decode load SP from HL.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_sp_hl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_sp_hl_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_sp_hl_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load SP from IX.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_sp_ix(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_sp_ix_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_sp_ix_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode load SP from IY.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ld_sp_iy(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ld_sp_iy_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ld_sp_iy_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode push register pair.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_push_qq(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_qq_t qq;
	z80ic_push_qq_t *push = NULL;
	z80ic_oper_qq_t *src = NULL;
	int rc;

	(void)decode;

	qq = z80_decode_get_qq(opc);

	rc = z80ic_push_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(qq, &src);
	if (rc != EOK)
		goto error;

	push->src = src;

	z80ic_lblock_append(lblock, NULL, &push->instr);
	return EOK;
error:
	z80ic_oper_qq_destroy(src);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	return rc;
}

/** Decode push IX.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_push_ix(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_push_ix_t *push = NULL;
	int rc;

	(void)decode;

	rc = z80ic_push_ix_create(&push);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &push->instr);
	return EOK;
}

/** Decode push IY.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_push_iy(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_push_iy_t *push = NULL;
	int rc;

	(void)decode;

	rc = z80ic_push_iy_create(&push);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &push->instr);
	return EOK;
}

/** Decode pop register pair.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_pop_qq(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_qq_t qq;
	z80ic_pop_qq_t *push = NULL;
	z80ic_oper_qq_t *src = NULL;
	int rc;

	(void)decode;

	qq = z80_decode_get_qq(opc);

	rc = z80ic_pop_qq_create(&push);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_qq_create(qq, &src);
	if (rc != EOK)
		goto error;

	push->src = src;

	z80ic_lblock_append(lblock, NULL, &push->instr);
	return EOK;
error:
	z80ic_oper_qq_destroy(src);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	return rc;
}

/** Decode pop IX.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_pop_ix(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_pop_ix_t *pop = NULL;
	int rc;

	(void)decode;

	rc = z80ic_pop_ix_create(&pop);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &pop->instr);
	return EOK;
}

/** Decode pop IY.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_pop_iy(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_pop_iy_t *pop = NULL;
	int rc;

	(void)decode;

	rc = z80ic_pop_iy_create(&pop);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &pop->instr);
	return EOK;
}

/** Decode exchange DE with HL.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ex_de_hl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ex_de_hl_t *ex = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ex_de_hl_create(&ex);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ex->instr);
	return EOK;
}

/** Decode exchange AF with AF'.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ex_af_afp(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ex_af_afp_t *ex = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ex_af_afp_create(&ex);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ex->instr);
	return EOK;
}

/** Decode exchange BC, DE, HL with BC', DE', HL'.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_exx(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_exx_t *exx = NULL;
	int rc;

	(void)decode;

	rc = z80ic_exx_create(&exx);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &exx->instr);
	return EOK;
}

/** Decode exchange (SP) with HL.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ex_isp_hl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ex_isp_hl_t *ex = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ex_isp_hl_create(&ex);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ex->instr);
	return EOK;
}

/** Decode exchange (SP) with IX.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ex_isp_ix(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ex_isp_ix_t *ex = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ex_isp_ix_create(&ex);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ex->instr);
	return EOK;
}

/** Decode exchange (SP) with IY.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ex_isp_iy(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ex_isp_iy_t *ex = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ex_isp_iy_create(&ex);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ex->instr);
	return EOK;
}

/** Decode load, increment.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ldi(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ldi_t *ldi = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ldi_create(&ldi);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ldi->instr);
	return EOK;
}

/** Decode load, increment, repeat.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ldir(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ldir_t *ldir = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ldir_create(&ldir);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ldir->instr);
	return EOK;
}

/** Decode load, decrement.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ldd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ldd_t *ldd = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ldd_create(&ldd);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ldd->instr);
	return EOK;
}

/** Decode load, decrement, repeat.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_lddr(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_lddr_t *lddr = NULL;
	int rc;

	(void)decode;

	rc = z80ic_lddr_create(&lddr);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &lddr->instr);
	return EOK;
}

/** Decode compare, increment.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cpi(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_cpi_t *cpi = NULL;
	int rc;

	(void)decode;

	rc = z80ic_cpi_create(&cpi);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &cpi->instr);
	return EOK;
}

/** Decode compare, increment, repeat.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cpir(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_cpir_t *cpir = NULL;
	int rc;

	(void)decode;

	rc = z80ic_cpir_create(&cpir);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &cpir->instr);
	return EOK;
}

/** Decode compare, decrement.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cpd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_cpd_t *cpd = NULL;
	int rc;

	(void)decode;

	rc = z80ic_cpd_create(&cpd);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &cpd->instr);
	return EOK;
}

/** Decode compare, decrement, repeat.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cpdr(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_cpdr_t *cpdr = NULL;
	int rc;

	(void)decode;

	rc = z80ic_cpdr_create(&cpdr);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &cpdr->instr);
	return EOK;
}

/** Decode add register to A.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_a_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_add_a_r_t *add = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_add_a_r_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	z80ic_lblock_append(lblock, NULL, &add->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode add 8-bit immediate to A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_a_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_add_a_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_add_a_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode add (HL) to A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_a_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_add_a_ihl_t *add = NULL;
	int rc;

	(void)decode;

	rc = z80ic_add_a_ihl_create(&add);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &add->instr);
	return EOK;
}

/** Decode add (IX+d) to A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_a_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_add_a_iixd_t *add = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_add_a_iixd_create(&add);
	if (rc != EOK)
		return rc;

	add->disp = disp;

	z80ic_lblock_append(lblock, NULL, &add->instr);
	return EOK;
}

/** Decode add (IY+d) to A.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_a_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_add_a_iiyd_t *add = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_add_a_iiyd_create(&add);
	if (rc != EOK)
		return rc;

	add->disp = disp;

	z80ic_lblock_append(lblock, NULL, &add->instr);
	return EOK;
}

/** Decode add register to A with carry.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_adc_a_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_adc_a_r_t *adc = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_adc_a_r_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	adc->src = src;

	z80ic_lblock_append(lblock, NULL, &adc->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode add 8-bit immediate to A with carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_adc_a_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_adc_a_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_adc_a_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode add (HL) to A carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_adc_a_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_adc_a_ihl_t *adc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_adc_a_ihl_create(&adc);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &adc->instr);
	return EOK;
}

/** Decode add (IX+d) to A with carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_adc_a_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_adc_a_iixd_t *adc = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_adc_a_iixd_create(&adc);
	if (rc != EOK)
		return rc;

	adc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &adc->instr);
	return EOK;
}

/** Decode add (IY+d) to A with carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_adc_a_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_adc_a_iiyd_t *adc = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_adc_a_iiyd_create(&adc);
	if (rc != EOK)
		return rc;

	adc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &adc->instr);
	return EOK;
}

/** Decode subtract register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sub_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_sub_r_t *sub = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_sub_r_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	sub->src = src;

	z80ic_lblock_append(lblock, NULL, &sub->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode subtract 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sub_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_sub_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_sub_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode subtract (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sub_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_sub_ihl_t *sub = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sub_ihl_create(&sub);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &sub->instr);
	return EOK;
}

/** Decode subtract (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sub_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_sub_iixd_t *sub = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_sub_iixd_create(&sub);
	if (rc != EOK)
		return rc;

	sub->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sub->instr);
	return EOK;
}

/** Decode subtract (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sub_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_sub_iiyd_t *sub = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_sub_iiyd_create(&sub);
	if (rc != EOK)
		return rc;

	sub->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sub->instr);
	return EOK;
}

/** Decode subtract register from A with carry.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sbc_a_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_sbc_a_r_t *sbc = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_sbc_a_r_create(&sbc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	sbc->src = src;

	z80ic_lblock_append(lblock, NULL, &sbc->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode subtract 8-bit immediate from A with carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sbc_a_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_sbc_a_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_sbc_a_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode subtract (HL) from A carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sbc_a_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_sbc_a_ihl_t *sbc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sbc_a_ihl_create(&sbc);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &sbc->instr);
	return EOK;
}

/** Decode subtract (IX+d) from A with carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sbc_a_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_sbc_a_iixd_t *sbc = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_sbc_a_iixd_create(&sbc);
	if (rc != EOK)
		return rc;

	sbc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sbc->instr);
	return EOK;
}

/** Decode subtract (IY+d) from A with carry.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sbc_a_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_sbc_a_iiyd_t *sbc = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_sbc_a_iiyd_create(&sbc);
	if (rc != EOK)
		return rc;

	sbc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sbc->instr);
	return EOK;
}

/** Decode bitwise AND with register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_and_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_and_r_t *and = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_and_r_create(&and);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	and->src = src;

	z80ic_lblock_append(lblock, NULL, &and->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (and != NULL)
		z80ic_instr_destroy(&and->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode bitwise AND with 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_and_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_and_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_and_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode bitwise AND with (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_and_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_and_ihl_t *and = NULL;
	int rc;

	(void)decode;

	rc = z80ic_and_ihl_create(&and);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &and->instr);
	return EOK;
}

/** Decode bitwise AND with (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_and_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_and_iixd_t *and = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_and_iixd_create(&and);
	if (rc != EOK)
		return rc;

	and->disp = disp;

	z80ic_lblock_append(lblock, NULL, &and->instr);
	return EOK;
}

/** Decode bitwise AND with (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_and_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_and_iiyd_t *and = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_and_iiyd_create(&and);
	if (rc != EOK)
		return rc;

	and->disp = disp;

	z80ic_lblock_append(lblock, NULL, &and->instr);
	return EOK;
}

/** Decode bitwise OR with register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_or_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_or_r_t *or = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_or_r_create(&or);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	or->src = src;

	z80ic_lblock_append(lblock, NULL, &or->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (or != NULL)
		z80ic_instr_destroy(&or->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode bitwise OR with 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_or_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_or_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_or_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode bitwise OR with (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_or_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_or_ihl_t *or = NULL;
	int rc;

	(void)decode;

	rc = z80ic_or_ihl_create(&or);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &or->instr);
	return EOK;
}

/** Decode bitwise OR with (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_or_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_or_iixd_t *or = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_or_iixd_create(&or);
	if (rc != EOK)
		return rc;

	or->disp = disp;

	z80ic_lblock_append(lblock, NULL, &or->instr);
	return EOK;
}

/** Decode bitwise OR with (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_or_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_or_iiyd_t *or = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_or_iiyd_create(&or);
	if (rc != EOK)
		return rc;

	or->disp = disp;

	z80ic_lblock_append(lblock, NULL, &or->instr);
	return EOK;
}

/** Decode bitwise XOR with register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_xor_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_xor_r_t *xor = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_xor_r_create(&xor);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	xor->src = src;

	z80ic_lblock_append(lblock, NULL, &xor->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode bitwise XOR with 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_xor_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_xor_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_xor_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode bitwise XOR with (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_xor_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_xor_ihl_t *xor = NULL;
	int rc;

	(void)decode;

	rc = z80ic_xor_ihl_create(&xor);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &xor->instr);
	return EOK;
}

/** Decode bitwise XOR with (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_xor_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_xor_iixd_t *xor = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_xor_iixd_create(&xor);
	if (rc != EOK)
		return rc;

	xor->disp = disp;

	z80ic_lblock_append(lblock, NULL, &xor->instr);
	return EOK;
}

/** Decode bitwise XOR with (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_xor_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_xor_iiyd_t *xor = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_xor_iiyd_create(&xor);
	if (rc != EOK)
		return rc;

	xor->disp = disp;

	z80ic_lblock_append(lblock, NULL, &xor->instr);
	return EOK;
}

/** Decode compare with register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cp_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_cp_r_t *cp = NULL;
	z80ic_oper_reg_t *src = NULL;
	int rc;

	(void)decode;

	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_cp_r_create(&cp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &src);
	if (rc != EOK)
		goto error;

	cp->src = src;

	z80ic_lblock_append(lblock, NULL, &cp->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(src);
	if (cp != NULL)
		z80ic_instr_destroy(&cp->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode compare with 8-bit immediate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cp_n(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_cp_n_t *ld = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	int rc;

	rc = z80_decode_imm8(decode, &imm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_cp_n_create(&ld);
	if (rc != EOK)
		goto error;

	ld->imm8 = imm8;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
error:
	z80ic_oper_imm8_destroy(imm8);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	return rc;
}

/** Decode compare with (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cp_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_cp_ihl_t *cp = NULL;
	int rc;

	(void)decode;

	rc = z80ic_cp_ihl_create(&cp);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &cp->instr);
	return EOK;
}

/** Decode compare with (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cp_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_cp_iixd_t *cp = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_cp_iixd_create(&cp);
	if (rc != EOK)
		return rc;

	cp->disp = disp;

	z80ic_lblock_append(lblock, NULL, &cp->instr);
	return EOK;
}

/** Decode compare with (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cp_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_cp_iiyd_t *cp = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_cp_iiyd_create(&cp);
	if (rc != EOK)
		return rc;

	cp->disp = disp;

	z80ic_lblock_append(lblock, NULL, &cp->instr);
	return EOK;
}

/** Decode increment register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_inc_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t dreg;
	z80ic_inc_r_t *inc = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	dreg = z80_decode_get_dreg(opc);

	rc = z80ic_inc_r_create(&inc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	inc->dest = dest;

	z80ic_lblock_append(lblock, NULL, &inc->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode increment (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_inc_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_inc_ihl_t *inc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_inc_ihl_create(&inc);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &inc->instr);
	return EOK;
}

/** Decode increment (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_inc_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_inc_iixd_t *inc = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_inc_iixd_create(&inc);
	if (rc != EOK)
		return rc;

	inc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &inc->instr);
	return EOK;
}

/** Decode increment (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_inc_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_inc_iiyd_t *inc = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_inc_iiyd_create(&inc);
	if (rc != EOK)
		return rc;

	inc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &inc->instr);
	return EOK;
}

/** Decode decrement register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dec_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t dreg;
	z80ic_dec_r_t *dec = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	dreg = z80_decode_get_dreg(opc);

	rc = z80ic_dec_r_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(dreg, &dest);
	if (rc != EOK)
		goto error;

	dec->dest = dest;

	z80ic_lblock_append(lblock, NULL, &dec->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode decrement (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dec_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_dec_ihl_t *dec = NULL;
	int rc;

	(void)decode;

	rc = z80ic_dec_ihl_create(&dec);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &dec->instr);
	return EOK;
}

/** Decode decrement (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dec_iixd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_dec_iixd_t *dec = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_dec_iixd_create(&dec);
	if (rc != EOK)
		return rc;

	dec->disp = disp;

	z80ic_lblock_append(lblock, NULL, &dec->instr);
	return EOK;
}

/** Decode decrement (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dec_iiyd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	z80ic_dec_iiyd_t *dec = NULL;
	int rc;

	disp = z80_decode_get_u8(decode);

	rc = z80ic_dec_iiyd_create(&dec);
	if (rc != EOK)
		return rc;

	dec->disp = disp;

	z80ic_lblock_append(lblock, NULL, &dec->instr);
	return EOK;
}

/** Decode decimal adjust accumulator.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_daa(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_daa_t *daa = NULL;
	int rc;

	(void)decode;

	rc = z80ic_daa_create(&daa);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &daa->instr);
	return EOK;
}

/** Decode complement.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cpl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_cpl_t *cpl = NULL;
	int rc;

	(void)decode;

	rc = z80ic_cpl_create(&cpl);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &cpl->instr);
	return EOK;
}

/** Decode negate.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_neg(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_neg_t *neg = NULL;
	int rc;

	(void)decode;

	rc = z80ic_neg_create(&neg);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &neg->instr);
	return EOK;
}

/** Decode complement carry flag.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ccf(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ccf_t *ccf = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ccf_create(&ccf);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ccf->instr);
	return EOK;
}

/** Decode set carry flag.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_scf(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_scf_t *scf = NULL;
	int rc;

	(void)decode;

	rc = z80ic_scf_create(&scf);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &scf->instr);
	return EOK;
}

/** Decode no operation.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_nop(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_nop_t *nop = NULL;
	int rc;

	(void)decode;

	rc = z80ic_nop_create(&nop);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &nop->instr);
	return EOK;
}

/** Decode halt.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_halt(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_halt_t *halt = NULL;
	int rc;

	(void)decode;

	rc = z80ic_halt_create(&halt);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &halt->instr);
	return EOK;
}

/** Decode disable interrupt.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_di(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_di_t *di = NULL;
	int rc;

	(void)decode;

	rc = z80ic_di_create(&di);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &di->instr);
	return EOK;
}

/** Decode enable interrupt.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ei(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_ei_t *ei = NULL;
	int rc;

	(void)decode;

	rc = z80ic_ei_create(&ei);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ei->instr);
	return EOK;
}

/** Decode set interrupt mode 0.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_im_0(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_im_0_t *im = NULL;
	int rc;

	(void)decode;

	rc = z80ic_im_0_create(&im);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &im->instr);
	return EOK;
}

/** Decode set interrupt mode 1.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_im_1(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_im_1_t *im = NULL;
	int rc;

	(void)decode;

	rc = z80ic_im_1_create(&im);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &im->instr);
	return EOK;
}

/** Decode set interrupt mode 2.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_im_2(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_im_2_t *im = NULL;
	int rc;

	(void)decode;

	rc = z80ic_im_2_create(&im);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &im->instr);
	return EOK;
}

/** Decode add 16-bit register to HL.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_hl_ss(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_ss_t ss;
	z80ic_add_hl_ss_t *add = NULL;
	z80ic_oper_ss_t *src = NULL;
	int rc;

	(void)decode;

	ss = z80_decode_get_ss(opc);

	rc = z80ic_add_hl_ss_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	z80ic_lblock_append(lblock, NULL, &add->instr);
	return EOK;
error:
	z80ic_oper_ss_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Decode add 16-bit register to HL with carry.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_adc_hl_ss(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_ss_t ss;
	z80ic_adc_hl_ss_t *adc = NULL;
	z80ic_oper_ss_t *src = NULL;
	int rc;

	(void)decode;

	ss = z80_decode_get_ss(opc);

	rc = z80ic_adc_hl_ss_create(&adc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &src);
	if (rc != EOK)
		goto error;

	adc->src = src;

	z80ic_lblock_append(lblock, NULL, &adc->instr);
	return EOK;
error:
	z80ic_oper_ss_destroy(src);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	return rc;
}

/** Decode subtract 16-bit register from HL with carry.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sbc_hl_ss(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_ss_t ss;
	z80ic_sbc_hl_ss_t *sbc = NULL;
	z80ic_oper_ss_t *src = NULL;
	int rc;

	(void)decode;

	ss = z80_decode_get_ss(opc);

	rc = z80ic_sbc_hl_ss_create(&sbc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &src);
	if (rc != EOK)
		goto error;

	sbc->src = src;

	z80ic_lblock_append(lblock, NULL, &sbc->instr);
	return EOK;
error:
	z80ic_oper_ss_destroy(src);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	return rc;
}

/** Decode add 16-bit register to IX.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_ix_pp(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_pp_t pp;
	z80ic_add_ix_pp_t *add = NULL;
	z80ic_oper_pp_t *src = NULL;
	int rc;

	(void)decode;

	pp = z80_decode_get_pp(opc);

	rc = z80ic_add_ix_pp_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_pp_create(pp, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	z80ic_lblock_append(lblock, NULL, &add->instr);
	return EOK;
error:
	z80ic_oper_pp_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Decode add 16-bit register to IY.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_add_iy_rr(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_rr_t rr;
	z80ic_add_iy_rr_t *add = NULL;
	z80ic_oper_rr_t *src = NULL;
	int rc;

	(void)decode;

	rr = z80_decode_get_rr(opc);

	rc = z80ic_add_iy_rr_create(&add);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_rr_create(rr, &src);
	if (rc != EOK)
		goto error;

	add->src = src;

	z80ic_lblock_append(lblock, NULL, &add->instr);
	return EOK;
error:
	z80ic_oper_rr_destroy(src);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	return rc;
}

/** Decode increment 16-bit register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_inc_ss(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_ss_t ss;
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_ss_t *dest = NULL;
	int rc;

	(void)decode;

	ss = z80_decode_get_ss(opc);

	rc = z80ic_inc_ss_create(&inc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &dest);
	if (rc != EOK)
		goto error;

	inc->dest = dest;

	z80ic_lblock_append(lblock, NULL, &inc->instr);
	return EOK;
error:
	z80ic_oper_ss_destroy(dest);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	return rc;
}

/** Decode increment IX.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_inc_ix(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_inc_ix_t *inc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_inc_ix_create(&inc);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &inc->instr);
	return EOK;
}

/** Decode increment IY.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_inc_iy(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_inc_iy_t *inc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_inc_iy_create(&inc);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &inc->instr);
	return EOK;
}

/** Decode decrement 16-bit register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dec_ss(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_ss_t ss;
	z80ic_dec_ss_t *dec = NULL;
	z80ic_oper_ss_t *dest = NULL;
	int rc;

	(void)decode;

	ss = z80_decode_get_ss(opc);

	rc = z80ic_dec_ss_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_ss_create(ss, &dest);
	if (rc != EOK)
		goto error;

	dec->dest = dest;

	z80ic_lblock_append(lblock, NULL, &dec->instr);
	return EOK;
error:
	z80ic_oper_ss_destroy(dest);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	return rc;
}

/** Decode decrement IX.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dec_ix(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_dec_ix_t *dec = NULL;
	int rc;

	(void)decode;

	rc = z80ic_dec_ix_create(&dec);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &dec->instr);
	return EOK;
}

/** Decode decrement IY.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dec_iy(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_dec_iy_t *dec = NULL;
	int rc;

	(void)decode;

	rc = z80ic_dec_iy_create(&dec);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &dec->instr);
	return EOK;
}

/** Decode rotate left circular accumulator.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rlca(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rlca_t *rlca = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rlca_create(&rlca);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rlca->instr);
	return EOK;
}

/** Decode rotate left accumulator.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rla(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rla_t *rla = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rla_create(&rla);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rla->instr);
	return EOK;
}

/** Decode rotate right circular accumulator.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rrca(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rrca_t *rrca = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rrca_create(&rrca);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rrca->instr);
	return EOK;
}

/** Decode rotate right accumulator.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rra(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rra_t *rra = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rra_create(&rra);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rra->instr);
	return EOK;
}

/** Decode rotate left circular register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rlc_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_rlc_r_t *rlc = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	/*
	 * NOTE In rotate and shift instructions the register number is
	 * encoded as sreg!
	 */
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_rlc_r_create(&rlc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	rlc->dest = dest;

	z80ic_lblock_append(lblock, NULL, &rlc->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (rlc != NULL)
		z80ic_instr_destroy(&rlc->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode rotate left circular (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rlc_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rlc_ihl_t *rlc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rlc_ihl_create(&rlc);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rlc->instr);
	return EOK;
}

/** Decode rotate left circular (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rlc_iixd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rlc_iixd_t *rlc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rlc_iixd_create(&rlc);
	if (rc != EOK)
		return rc;

	rlc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rlc->instr);
	return EOK;
}

/** Decode rotate left circular (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rlc_iiyd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rlc_iiyd_t *rlc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rlc_iiyd_create(&rlc);
	if (rc != EOK)
		return rc;

	rlc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rlc->instr);
	return EOK;
}

/** Decode rotate left register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rl_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_rl_r_t *rl = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	/*
	 * NOTE In rotate and shift instructions the register number is
	 * encoded as sreg!
	 */
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_rl_r_create(&rl);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	rl->dest = dest;

	z80ic_lblock_append(lblock, NULL, &rl->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (rl != NULL)
		z80ic_instr_destroy(&rl->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode rotate left (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rl_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rl_ihl_t *rl = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rl_ihl_create(&rl);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rl->instr);
	return EOK;
}

/** Decode rotate left (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rl_iixd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rl_iixd_t *rl = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rl_iixd_create(&rl);
	if (rc != EOK)
		return rc;

	rl->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rl->instr);
	return EOK;
}

/** Decode rotate left (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rl_iiyd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rl_iiyd_t *rl = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rl_iiyd_create(&rl);
	if (rc != EOK)
		return rc;

	rl->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rl->instr);
	return EOK;
}

/** Decode rotate right circular register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rrc_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_rrc_r_t *rrc = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	/*
	 * NOTE In rotate and shift instructions the register number is
	 * encoded as sreg!
	 */
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_rrc_r_create(&rrc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	rrc->dest = dest;

	z80ic_lblock_append(lblock, NULL, &rrc->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (rrc != NULL)
		z80ic_instr_destroy(&rrc->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode rotate right circular (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rrc_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rrc_ihl_t *rrc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rrc_ihl_create(&rrc);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rrc->instr);
	return EOK;
}

/** Decode rotate right circular (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rrc_iixd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rrc_iixd_t *rrc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rrc_iixd_create(&rrc);
	if (rc != EOK)
		return rc;

	rrc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rrc->instr);
	return EOK;
}

/** Decode rotate right circular (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rrc_iiyd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rrc_iiyd_t *rrc = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rrc_iiyd_create(&rrc);
	if (rc != EOK)
		return rc;

	rrc->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rrc->instr);
	return EOK;
}

/** Decode rotate right register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rr_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_rr_r_t *rr = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	/*
	 * NOTE In rotate and shift instructions the register number is
	 * encoded as sreg!
	 */
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_rr_r_create(&rr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	rr->dest = dest;

	z80ic_lblock_append(lblock, NULL, &rr->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (rr != NULL)
		z80ic_instr_destroy(&rr->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode rotate right (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rr_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rr_ihl_t *rr = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rr_ihl_create(&rr);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rr->instr);
	return EOK;
}

/** Decode rotate right (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rr_iixd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rr_iixd_t *rr = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rr_iixd_create(&rr);
	if (rc != EOK)
		return rc;

	rr->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rr->instr);
	return EOK;
}

/** Decode rotate right (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rr_iiyd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_rr_iiyd_t *rr = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rr_iiyd_create(&rr);
	if (rc != EOK)
		return rc;

	rr->disp = disp;

	z80ic_lblock_append(lblock, NULL, &rr->instr);
	return EOK;
}

/** Decode shift left arithmetic register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sla_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_sla_r_t *sla = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	/*
	 * NOTE In rotate and shift instructions the register number is
	 * encoded as sreg!
	 */
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_sla_r_create(&sla);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	sla->dest = dest;

	z80ic_lblock_append(lblock, NULL, &sla->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (sla != NULL)
		z80ic_instr_destroy(&sla->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode shift left arithmetic (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sla_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_sla_ihl_t *sla = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sla_ihl_create(&sla);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &sla->instr);
	return EOK;
}

/** Decode shift left arithmetic (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sla_iixd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_sla_iixd_t *sla = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sla_iixd_create(&sla);
	if (rc != EOK)
		return rc;

	sla->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sla->instr);
	return EOK;
}

/** Decode shift left arithmetic (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sla_iiyd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_sla_iiyd_t *sla = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sla_iiyd_create(&sla);
	if (rc != EOK)
		return rc;

	sla->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sla->instr);
	return EOK;
}

/** Decode shift right arithmetic register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sra_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_sra_r_t *sra = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	/*
	 * NOTE In rotate and shift instructions the register number is
	 * encoded as sreg!
	 */
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_sra_r_create(&sra);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	sra->dest = dest;

	z80ic_lblock_append(lblock, NULL, &sra->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (sra != NULL)
		z80ic_instr_destroy(&sra->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode shift right arithmetic (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sra_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_sra_ihl_t *sra = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sra_ihl_create(&sra);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &sra->instr);
	return EOK;
}

/** Decode shift right arithmetic (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sra_iixd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_sra_iixd_t *sra = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sra_iixd_create(&sra);
	if (rc != EOK)
		return rc;

	sra->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sra->instr);
	return EOK;
}

/** Decode shift right arithmetic (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_sra_iiyd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_sra_iiyd_t *sra = NULL;
	int rc;

	(void)decode;

	rc = z80ic_sra_iiyd_create(&sra);
	if (rc != EOK)
		return rc;

	sra->disp = disp;

	z80ic_lblock_append(lblock, NULL, &sra->instr);
	return EOK;
}

/** Decode shift right logical register.
 *
 * @param decode Binary instruction decoder
 * @param opc Opcode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_srl_r(z80_decode_t *decode, uint8_t opc,
    z80ic_lblock_t *lblock)
{
	z80ic_reg_t sreg;
	z80ic_srl_r_t *srl = NULL;
	z80ic_oper_reg_t *dest = NULL;
	int rc;

	(void)decode;

	/*
	 * NOTE In rotate and shift instructions the register number is
	 * encoded as sreg!
	 */
	sreg = z80_decode_get_sreg(opc);

	rc = z80ic_srl_r_create(&srl);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(sreg, &dest);
	if (rc != EOK)
		goto error;

	srl->dest = dest;

	z80ic_lblock_append(lblock, NULL, &srl->instr);
	return EOK;
error:
	z80ic_oper_reg_destroy(dest);
	if (srl != NULL)
		z80ic_instr_destroy(&srl->instr);
	return rc;

	(void)lblock;
	return EOK;
}

/** Decode shift right logical (HL).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_srl_ihl(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_srl_ihl_t *srl = NULL;
	int rc;

	(void)decode;

	rc = z80ic_srl_ihl_create(&srl);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &srl->instr);
	return EOK;
}

/** Decode shift right logical (IX+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_srl_iixd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_srl_iixd_t *srl = NULL;
	int rc;

	(void)decode;

	rc = z80ic_srl_iixd_create(&srl);
	if (rc != EOK)
		return rc;

	srl->disp = disp;

	z80ic_lblock_append(lblock, NULL, &srl->instr);
	return EOK;
}

/** Decode shift right logical (IY+d).
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_srl_iiyd(z80_decode_t *decode, uint8_t disp,
    z80ic_lblock_t *lblock)
{
	z80ic_srl_iiyd_t *srl = NULL;
	int rc;

	(void)decode;

	rc = z80ic_srl_iiyd_create(&srl);
	if (rc != EOK)
		return rc;

	srl->disp = disp;

	z80ic_lblock_append(lblock, NULL, &srl->instr);
	return EOK;
}

/** Decode rotate left digit.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rld(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rld_t *rld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rld_create(&rld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rld->instr);
	return EOK;
}

/** Decode rotate right digit.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_rrd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	z80ic_rrd_t *rrd = NULL;
	int rc;

	(void)decode;

	rc = z80ic_rrd_create(&rrd);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &rrd->instr);
	return EOK;
}

/** Decode one instruction with CB prefix.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_cb(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t b;

	b = z80_decode_get_u8(decode);
	switch (b) {
	case z80opc_rlc_ihl & 0xff:
		return z80_decode_rlc_ihl(decode, lblock);
	case z80opc_rl_ihl & 0xff:
		return z80_decode_rl_ihl(decode, lblock);
	case z80opc_rrc_ihl & 0xff:
		return z80_decode_rrc_ihl(decode, lblock);
	case z80opc_rr_ihl & 0xff:
		return z80_decode_rr_ihl(decode, lblock);
	case z80opc_sla_ihl & 0xff:
		return z80_decode_sla_ihl(decode, lblock);
	case z80opc_sra_ihl & 0xff:
		return z80_decode_sra_ihl(decode, lblock);
	case z80opc_srl_ihl & 0xff:
		return z80_decode_srl_ihl(decode, lblock);
	default:
		printf("Unknown opcode 0xcb%" PRIx8 "\n", b);
	}

	if ((b & 0xf8) == (z80opc_rlc_r & 0xff))
		return z80_decode_rlc_r(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_rl_r & 0xff))
		return z80_decode_rl_r(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_rrc_r & 0xff))
		return z80_decode_rrc_r(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_rr_r & 0xff))
		return z80_decode_rr_r(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_sla_r & 0xff))
		return z80_decode_sla_r(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_sra_r & 0xff))
		return z80_decode_sra_r(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_srl_r & 0xff))
		return z80_decode_srl_r(decode, b, lblock);

	(void)lblock;
	return EOK;
}

/** Decode one instrucction with DDCB prefix.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ddcb(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	uint8_t b;

	disp = z80_decode_get_u8(decode);
	b = z80_decode_get_u8(decode);

	switch (b) {
	case z80opc_rlc_iixd & 0xff:
		return z80_decode_rlc_iixd(decode, disp, lblock);
	case z80opc_rl_iixd & 0xff:
		return z80_decode_rl_iixd(decode, disp, lblock);
	case z80opc_rrc_iixd & 0xff:
		return z80_decode_rrc_iixd(decode, disp, lblock);
	case z80opc_rr_iixd & 0xff:
		return z80_decode_rr_iixd(decode, disp, lblock);
	case z80opc_sla_iixd & 0xff:
		return z80_decode_sla_iixd(decode, disp, lblock);
	case z80opc_sra_iixd & 0xff:
		return z80_decode_sra_iixd(decode, disp, lblock);
	case z80opc_srl_iixd & 0xff:
		return z80_decode_srl_iixd(decode, disp, lblock);
	default:
		printf("Unknown opcode 0xddcb%" PRIx8 "\n", b);
	}

	(void)lblock;
	return EOK;
}

/** Decode one instrucction with DD prefix.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_dd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t b;

	b = z80_decode_get_u8(decode);
	switch (b) {
	case 0xcb:
		return z80_decode_ddcb(decode, lblock);
	case z80opc_ld_iixd_n & 0xff:
		return z80_decode_ld_iixd_n(decode, lblock);
	case z80opc_ld_ix_nn & 0xff:
		return z80_decode_ld_ix_nn(decode, lblock);
	case z80opc_ld_ix_inn & 0xff:
		return z80_decode_ld_ix_inn(decode, lblock);
	case z80opc_ld_inn_ix & 0xff:
		return z80_decode_ld_inn_ix(decode, lblock);
	case z80opc_ld_sp_ix & 0xff:
		return z80_decode_ld_sp_ix(decode, lblock);
	case z80opc_push_ix & 0xff:
		return z80_decode_push_ix(decode, lblock);
	case z80opc_pop_ix & 0xff:
		return z80_decode_pop_ix(decode, lblock);
	case z80opc_ex_isp_hl & 0xff:
		return z80_decode_ex_isp_ix(decode, lblock);
	case z80opc_add_a_iixd & 0xff:
		return z80_decode_add_a_iixd(decode, lblock);
	case z80opc_adc_a_iixd & 0xff:
		return z80_decode_adc_a_iixd(decode, lblock);
	case z80opc_sub_iixd & 0xff:
		return z80_decode_sub_iixd(decode, lblock);
	case z80opc_sbc_a_iixd & 0xff:
		return z80_decode_sbc_a_iixd(decode, lblock);
	case z80opc_and_iixd & 0xff:
		return z80_decode_and_iixd(decode, lblock);
	case z80opc_or_iixd & 0xff:
		return z80_decode_or_iixd(decode, lblock);
	case z80opc_xor_iixd & 0xff:
		return z80_decode_xor_iixd(decode, lblock);
	case z80opc_cp_iixd & 0xff:
		return z80_decode_cp_iixd(decode, lblock);
	case z80opc_inc_iixd & 0xff:
		return z80_decode_inc_iixd(decode, lblock);
	case z80opc_dec_iixd & 0xff:
		return z80_decode_dec_iixd(decode, lblock);
	case z80opc_inc_ix & 0xff:
		return z80_decode_inc_ix(decode, lblock);
	case z80opc_dec_ix & 0xff:
		return z80_decode_dec_ix(decode, lblock);
	default:
		printf("Unknown opcode 0xdd%" PRIx8 "\n", b);
	}

	if ((b & 0xc7) == (z80opc_ld_r_iixd & 0xff))
		return z80_decode_ld_r_iixd(decode, b, lblock);
	if ((b & 0xcf) == (z80opc_add_ix_pp & 0xff))
		return z80_decode_add_ix_pp(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_ld_iixd_r & 0xff))
		return z80_decode_ld_iixd_r(decode, b, lblock);

	(void)lblock;
	return EOK;
}

/** Decode one instrucction with ED prefix.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_ed(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t b;

	b = z80_decode_get_u8(decode);
	switch (b) {
	case z80opc_ld_a_i & 0xff:
		return z80_decode_ld_a_i(decode, lblock);
	case z80opc_ld_a_r & 0xff:
		return z80_decode_ld_a_r(decode, lblock);
	case z80opc_ld_i_a & 0xff:
		return z80_decode_ld_i_a(decode, lblock);
	case z80opc_ld_r_a & 0xff:
		return z80_decode_ld_r_a(decode, lblock);
	case z80opc_ldi & 0xff:
		return z80_decode_ldi(decode, lblock);
	case z80opc_ldir & 0xff:
		return z80_decode_ldir(decode, lblock);
	case z80opc_ldd & 0xff:
		return z80_decode_ldd(decode, lblock);
	case z80opc_lddr & 0xff:
		return z80_decode_lddr(decode, lblock);
	case z80opc_cpi & 0xff:
		return z80_decode_cpi(decode, lblock);
	case z80opc_cpir & 0xff:
		return z80_decode_cpir(decode, lblock);
	case z80opc_cpd & 0xff:
		return z80_decode_cpd(decode, lblock);
	case z80opc_cpdr & 0xff:
		return z80_decode_cpdr(decode, lblock);
	case z80opc_neg & 0xff:
		return z80_decode_neg(decode, lblock);
	case z80opc_im_0 & 0xff:
		return z80_decode_im_0(decode, lblock);
	case z80opc_im_1 & 0xff:
		return z80_decode_im_1(decode, lblock);
	case z80opc_im_2 & 0xff:
		return z80_decode_im_2(decode, lblock);
	case z80opc_rld & 0xff:
		return z80_decode_rld(decode, lblock);
	case z80opc_rrd & 0xff:
		return z80_decode_rrd(decode, lblock);
	default:
		printf("Unknown opcode 0xed%" PRIx8 "\n", b);
	}

	if ((b & 0xcf) == (z80opc_ld_dd_inn & 0xff))
		return z80_decode_ld_dd_inn(decode, b, lblock);
	if ((b & 0xcf) == (z80opc_ld_inn_dd & 0xff))
		return z80_decode_ld_inn_dd(decode, b, lblock);
	if ((b & 0xcf) == (z80opc_adc_hl_ss & 0xff))
		return z80_decode_adc_hl_ss(decode, b, lblock);
	if ((b & 0xcf) == (z80opc_sbc_hl_ss & 0xff))
		return z80_decode_sbc_hl_ss(decode, b, lblock);

	(void)lblock;
	return EOK;
}

/** Decode one instrucction with FDCB prefix.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_fdcb(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t disp;
	uint8_t b;

	disp = z80_decode_get_u8(decode);
	b = z80_decode_get_u8(decode);
	printf("disp 0x%02x, opc=0x%02x\n", disp, b);

	switch (b) {
	case z80opc_rlc_iiyd & 0xff:
		return z80_decode_rlc_iiyd(decode, disp, lblock);
	case z80opc_rl_iiyd & 0xff:
		return z80_decode_rl_iiyd(decode, disp, lblock);
	case z80opc_rrc_iiyd & 0xff:
		return z80_decode_rrc_iiyd(decode, disp, lblock);
	case z80opc_rr_iiyd & 0xff:
		return z80_decode_rr_iiyd(decode, disp, lblock);
	case z80opc_sla_iiyd & 0xff:
		return z80_decode_sla_iiyd(decode, disp, lblock);
	case z80opc_sra_iiyd & 0xff:
		return z80_decode_sra_iiyd(decode, disp, lblock);
	case z80opc_srl_iiyd & 0xff:
		return z80_decode_srl_iiyd(decode, disp, lblock);
	default:
		printf("Unknown opcode 0xfdcb%" PRIx8 "\n", b);
	}

	(void)lblock;
	return EOK;
}

/** Decode one instrucction with FD prefix.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_fd(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t b;

	b = z80_decode_get_u8(decode);
	switch (b) {
	case 0xcb:
		return z80_decode_fdcb(decode, lblock);
	case z80opc_ld_iiyd_n & 0xff:
		return z80_decode_ld_iiyd_n(decode, lblock);
	case z80opc_ld_iy_nn & 0xff:
		return z80_decode_ld_iy_nn(decode, lblock);
	case z80opc_ld_iy_inn & 0xff:
		return z80_decode_ld_iy_inn(decode, lblock);
	case z80opc_ld_inn_iy & 0xff:
		return z80_decode_ld_inn_iy(decode, lblock);
	case z80opc_ld_sp_iy & 0xff:
		return z80_decode_ld_sp_iy(decode, lblock);
	case z80opc_push_iy & 0xff:
		return z80_decode_push_iy(decode, lblock);
	case z80opc_pop_iy & 0xff:
		return z80_decode_pop_iy(decode, lblock);
	case z80opc_ex_isp_iy & 0xff:
		return z80_decode_ex_isp_iy(decode, lblock);
	case z80opc_add_a_iiyd & 0xff:
		return z80_decode_add_a_iiyd(decode, lblock);
	case z80opc_adc_a_iiyd & 0xff:
		return z80_decode_adc_a_iiyd(decode, lblock);
	case z80opc_sub_iiyd & 0xff:
		return z80_decode_sub_iiyd(decode, lblock);
	case z80opc_sbc_a_iiyd & 0xff:
		return z80_decode_sbc_a_iiyd(decode, lblock);
	case z80opc_and_iiyd & 0xff:
		return z80_decode_and_iiyd(decode, lblock);
	case z80opc_or_iiyd & 0xff:
		return z80_decode_or_iiyd(decode, lblock);
	case z80opc_xor_iiyd & 0xff:
		return z80_decode_xor_iiyd(decode, lblock);
	case z80opc_cp_iiyd & 0xff:
		return z80_decode_cp_iiyd(decode, lblock);
	case z80opc_inc_iiyd & 0xff:
		return z80_decode_inc_iiyd(decode, lblock);
	case z80opc_dec_iiyd & 0xff:
		return z80_decode_dec_iiyd(decode, lblock);
	case z80opc_inc_iy & 0xff:
		return z80_decode_inc_iy(decode, lblock);
	case z80opc_dec_iy & 0xff:
		return z80_decode_dec_iy(decode, lblock);
	default:
		printf("Unknown opcode 0xfd%" PRIx8 "\n", b);
	}

	if ((b & 0xc7) == (z80opc_ld_r_iiyd & 0xff))
		return z80_decode_ld_r_iiyd(decode, b, lblock);
	if ((b & 0xcf) == (z80opc_add_iy_rr & 0xff))
		return z80_decode_add_iy_rr(decode, b, lblock);
	if ((b & 0xf8) == (z80opc_ld_iiyd_r & 0xff))
		return z80_decode_ld_iiyd_r(decode, b, lblock);

	(void)lblock;
	return EOK;
}

/** Decode one instrucction.
 *
 * @param decode Binary instruction decoder
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_instr(z80_decode_t *decode, z80ic_lblock_t *lblock)
{
	uint8_t b;

	b = z80_decode_get_u8(decode);

	switch (b) {
	case 0xcb:
		return z80_decode_cb(decode, lblock);
	case 0xdd:
		return z80_decode_dd(decode, lblock);
	case 0xed:
		return z80_decode_ed(decode, lblock);
	case 0xfd:
		return z80_decode_fd(decode, lblock);
	case z80opc_ld_ihl_n:
		return z80_decode_ld_ihl_n(decode, lblock);
	case z80opc_ld_a_ibc:
		return z80_decode_ld_a_ibc(decode, lblock);
	case z80opc_ld_a_ide:
		return z80_decode_ld_a_ide(decode, lblock);
	case z80opc_ld_a_inn:
		return z80_decode_ld_a_inn(decode, lblock);
	case z80opc_ld_ibc_a:
		return z80_decode_ld_ibc_a(decode, lblock);
	case z80opc_ld_ide_a:
		return z80_decode_ld_ide_a(decode, lblock);
	case z80opc_ld_inn_a:
		return z80_decode_ld_inn_a(decode, lblock);
	case z80opc_ld_hl_inn:
		return z80_decode_ld_hl_inn(decode, lblock);
	case z80opc_ld_inn_hl:
		return z80_decode_ld_inn_hl(decode, lblock);
	case z80opc_ld_sp_hl:
		return z80_decode_ld_sp_hl(decode, lblock);
	case z80opc_ex_de_hl:
		return z80_decode_ex_de_hl(decode, lblock);
	case z80opc_ex_af_afp:
		return z80_decode_ex_af_afp(decode, lblock);
	case z80opc_exx:
		return z80_decode_exx(decode, lblock);
	case z80opc_ex_isp_hl:
		return z80_decode_ex_isp_hl(decode, lblock);
	case z80opc_add_a_n:
		return z80_decode_add_a_n(decode, lblock);
	case z80opc_add_a_ihl:
		return z80_decode_add_a_ihl(decode, lblock);
	case z80opc_adc_a_n:
		return z80_decode_adc_a_n(decode, lblock);
	case z80opc_adc_a_ihl:
		return z80_decode_adc_a_ihl(decode, lblock);
	case z80opc_sub_n:
		return z80_decode_sub_n(decode, lblock);
	case z80opc_sub_ihl:
		return z80_decode_sub_ihl(decode, lblock);
	case z80opc_sbc_a_n:
		return z80_decode_sbc_a_n(decode, lblock);
	case z80opc_sbc_a_ihl:
		return z80_decode_sbc_a_ihl(decode, lblock);
	case z80opc_and_n:
		return z80_decode_and_n(decode, lblock);
	case z80opc_and_ihl:
		return z80_decode_and_ihl(decode, lblock);
	case z80opc_or_n:
		return z80_decode_or_n(decode, lblock);
	case z80opc_or_ihl:
		return z80_decode_or_ihl(decode, lblock);
	case z80opc_xor_n:
		return z80_decode_xor_n(decode, lblock);
	case z80opc_xor_ihl:
		return z80_decode_xor_ihl(decode, lblock);
	case z80opc_cp_n:
		return z80_decode_cp_n(decode, lblock);
	case z80opc_cp_ihl:
		return z80_decode_cp_ihl(decode, lblock);
	case z80opc_inc_ihl:
		return z80_decode_inc_ihl(decode, lblock);
	case z80opc_dec_ihl:
		return z80_decode_dec_ihl(decode, lblock);
	case z80opc_daa:
		return z80_decode_daa(decode, lblock);
	case z80opc_cpl:
		return z80_decode_cpl(decode, lblock);
	case z80opc_ccf:
		return z80_decode_ccf(decode, lblock);
	case z80opc_scf:
		return z80_decode_scf(decode, lblock);
	case z80opc_nop:
		return z80_decode_nop(decode, lblock);
	case z80opc_halt:
		return z80_decode_halt(decode, lblock);
	case z80opc_di:
		return z80_decode_di(decode, lblock);
	case z80opc_ei:
		return z80_decode_ei(decode, lblock);
	case z80opc_rlca:
		return z80_decode_rlca(decode, lblock);
	case z80opc_rla:
		return z80_decode_rla(decode, lblock);
	case z80opc_rrca:
		return z80_decode_rrca(decode, lblock);
	case z80opc_rra:
		return z80_decode_rra(decode, lblock);
	default:
		break;
	}

	if ((b & 0xcf) == z80opc_ld_dd_nn)
		return z80_decode_ld_dd_nn(decode, b, lblock);
	if ((b & 0xcf) == z80opc_push_qq)
		return z80_decode_push_qq(decode, b, lblock);
	if ((b & 0xcf) == z80opc_pop_qq)
		return z80_decode_pop_qq(decode, b, lblock);
	if ((b & 0xcf) == z80opc_add_hl_ss)
		return z80_decode_add_hl_ss(decode, b, lblock);
	if ((b & 0xcf) == z80opc_inc_ss)
		return z80_decode_inc_ss(decode, b, lblock);
	if ((b & 0xcf) == z80opc_dec_ss)
		return z80_decode_dec_ss(decode, b, lblock);
	if ((b & 0xc7) == z80opc_ld_r_ihl)
		return z80_decode_ld_r_ihl(decode, b, lblock);
	if ((b & 0xc7) == z80opc_ld_r_n)
		return z80_decode_ld_r_n(decode, b, lblock);
	if ((b & 0xc7) == z80opc_inc_r)
		return z80_decode_inc_r(decode, b, lblock);
	if ((b & 0xc7) == z80opc_dec_r)
		return z80_decode_dec_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_ld_ihl_r)
		return z80_decode_ld_ihl_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_add_a_r)
		return z80_decode_add_a_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_adc_a_r)
		return z80_decode_adc_a_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_sub_r)
		return z80_decode_sub_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_sbc_a_r)
		return z80_decode_sbc_a_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_and_r)
		return z80_decode_and_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_or_r)
		return z80_decode_or_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_xor_r)
		return z80_decode_xor_r(decode, b, lblock);
	if ((b & 0xf8) == z80opc_cp_r)
		return z80_decode_cp_r(decode, b, lblock);
	if ((b & 0xc0) == z80opc_ld_r_r)
		return z80_decode_ld_r_r(decode, b, lblock);

	printf("Unknown opcode 0x%" PRIx8 "\n", b);
	return EOK;
}

/** Decode binary instructions from a range of bytes in a section.
 *
 * @param decode Binary instruction decoder
 * @param section Object section
 * @param offset Start offset in sectino
 * @param size Number of bytes to decode
 * @param lblock Labeled block to append instructions to
 * @return EOK on success or an error code
 */
static int z80_decode_range(z80_decode_t *decode, obj_section_t *section,
    uint32_t offset, uint32_t size, z80ic_lblock_t *lblock)
{
	int rc;

	decode->section = section;
	decode->offset = offset;
	decode->rem_bytes = size;

	while (decode->rem_bytes > 0) {
		rc = z80_decode_instr(decode, lblock);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Decode binary instructions from object.
 *
 * @param decode Binary instruction decoder
 * @param object Binary object
 * @param ricmod Place to store pointer to new Z80 IC module
 * @return EOK on success or an error code
 */
int z80_decode_object(z80_decode_t *decode, obj_object_t *object,
    z80ic_module_t **ricmod)
{
	z80ic_module_t *icmod = NULL;
	z80ic_lblock_t *lblock = NULL;
	z80ic_proc_t *proc = NULL;
	obj_symbol_t *symbol;
	int rc;

	rc = z80ic_module_create(&icmod);
	if (rc != EOK)
		goto error;

	decode->object = object;
	decode->ic_module = icmod;

	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		if (symbol->size != 0) {
			rc = z80ic_lblock_create(&lblock);
			if (rc != EOK)
				goto error;

			rc = z80_decode_range(decode, symbol->section,
			    symbol->offset, symbol->size, lblock);
			if (rc != EOK)
				goto error;

			rc = z80ic_proc_create(symbol->name, lblock, &proc);
			if (rc != EOK)
				goto error;

			lblock = NULL;

			z80ic_module_append(icmod, &proc->decln);
		}

		symbol = obj_symbol_next(symbol);
	}

	decode->object = NULL;
	*ricmod = icmod;
	return EOK;
error:
	z80ic_proc_destroy(proc);
	z80ic_lblock_destroy(lblock);
	z80ic_module_destroy(icmod);
	return rc;
}

/** Destroy binary instruction decoder.
 *
 * @param decode Instruction decoder or @c NULL
 */
void z80_decode_destroy(z80_decode_t *decode)
{
	if (decode == NULL)
		return;

	free(decode);
}
