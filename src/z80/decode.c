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

/** Get qq register from opcode.
 *
 * @param opc 8-bit opcode
 * @return 16-bit qq register
 */
static z80ic_qq_t z80_decode_get_qq(uint8_t opc)
{
	return (z80ic_qq_t)((opc >> 4) & 0x3);
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

	(void)decode;

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

	(void)decode;

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

	(void)decode;

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

	(void)decode;

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
	z80ic_push_ix_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_push_ix_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
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
	z80ic_push_iy_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_push_iy_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
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
	z80ic_pop_ix_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_pop_ix_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
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
	z80ic_pop_iy_t *ld = NULL;
	int rc;

	(void)decode;

	rc = z80ic_pop_iy_create(&ld);
	if (rc != EOK)
		return rc;

	z80ic_lblock_append(lblock, NULL, &ld->instr);
	return EOK;
}

/** Decode one instrucction with CB prefix.
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
	default:
		printf("Unknown opcode 0xcb%" PRIx8 "\n", b);
	}

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
	uint8_t b;

	b = z80_decode_get_u8(decode);
	switch (b) {
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
	default:
		printf("Unknown opcode 0xdd%" PRIx8 "\n", b);
	}

	if ((b & 0xc7) == (z80opc_ld_r_iixd & 0xff))
		return z80_decode_ld_r_iixd(decode, b, lblock);
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
	default:
		printf("Unknown opcode 0xed%" PRIx8 "\n", b);
	}

	if ((b & 0xcf) == (z80opc_ld_dd_inn & 0xff))
		return z80_decode_ld_dd_inn(decode, b, lblock);
	if ((b & 0xcf) == (z80opc_ld_inn_dd & 0xff))
		return z80_decode_ld_inn_dd(decode, b, lblock);

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
	uint8_t b;

	b = z80_decode_get_u8(decode);
	switch (b) {
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
	default:
		printf("Unknown opcode 0xfd%" PRIx8 "\n", b);
	}

	if ((b & 0xc7) == (z80opc_ld_r_iiyd & 0xff))
		return z80_decode_ld_r_iiyd(decode, b, lblock);
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
	default:
		break;
	}

	if ((b & 0xcf) == z80opc_ld_dd_nn)
		return z80_decode_ld_dd_nn(decode, b, lblock);
	if ((b & 0xcf) == z80opc_push_qq)
		return z80_decode_push_qq(decode, b, lblock);
	if ((b & 0xcf) == z80opc_pop_qq)
		return z80_decode_pop_qq(decode, b, lblock);
	if ((b & 0xc7) == z80opc_ld_r_ihl)
		return z80_decode_ld_r_ihl(decode, b, lblock);
	if ((b & 0xc7) == z80opc_ld_r_n)
		return z80_decode_ld_r_n(decode, b, lblock);
	if ((b & 0xf8) == z80opc_ld_ihl_r)
		return z80_decode_ld_ihl_r(decode, b, lblock);
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
