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
 * Z80 Instruction Code
 */

#ifndef TYPES_Z80IC_H
#define TYPES_Z80IC_H

#include <adt/list.h>
#include <stdint.h>

/** Z80 IC instruction type.
 *
 * We define an instruction type for each actual opcode (as documented by
 * Zilog Z80 CPU User manual) with real registers. We also define instructions
 * with virtual registers. These are more generic and when allocating
 * registers the allocated needs to deal with constraints stemming from
 * which instructions a virtual register is used with. Depending on the
 * actual register allocated, different opcodes / instruction types may
 * result.
 */
typedef enum {
	/** Load register from register */
	z80i_ld_r_r,
	/** Load register from 8-bit immediate */
	z80i_ld_r_n,
	/** Load register from (HL) */
	z80i_ld_r_ihl,
	/** Load register from (IX+d) */
	z80i_ld_r_iixd,
	/** Load register from (IY+d) */
	z80i_ld_r_iiyd,
	/** Load (HL) from register */
	z80i_ld_ihl_r,
	/** Load (IX+d) from register */
	z80i_ld_iixd_r,
	/** Load (IY+d) from register */
	z80i_ld_iiyd_r,
	/** Load (HL) from 8-bit immediate */
	z80i_ld_ihl_n,
	/** Load (IX+d) from 8-bit immediate */
	z80i_ld_iixd_n,
	/** Load (IY+d) from 8-bit immediate */
	z80i_ld_iiyd_n,
	/** Load A from (BC) */
	z80i_ld_a_ibc,
	/** Load A from (DE) */
	z80i_ld_a_ide,
	/** Load A from fixed memory location */
	z80i_ld_a_inn,
	/** Load (BC) from A */
	z80i_ld_ibc_a,
	/** Load (DE) from A */
	z80i_ld_ide_a,
	/** Load fixed memory location from A */
	z80i_ld_inn_a,
	/** Load A from interrupt vector register */
	z80i_ld_a_i,
	/** Load A from memory refresh register */
	z80i_ld_a_r,
	/** Load interrupt vector register from A */
	z80i_ld_i_a,
	/** Load memory refresh register from A */
	z80i_ld_r_a,

	/** Load register pair from 16-bit immediate */
	z80i_ld_dd_nn,
	/** Load IX from 16-bit immediate */
	z80i_ld_ix_nn,
	/** Load IY from 16-bit immediate */
	z80i_ld_iy_nn,
	/** Load HL from fixed memory location */
	z80i_ld_hl_inn,
	/** Load register pair from fixed memory location */
	z80i_ldd_dd_inn,
	/** Load IX from fixed memory location */
	z80i_ld_ix_inn,
	/** Load IY from fixed memory location */
	z80i_ld_iy_inn,
	/** Load fixed memory location from HL */
	z80i_ld_inn_hl,
	/** Load fixed memory location from register pair */
	z80i_ld_inn_dd,
	/** Load fixed memory location from IX */
	z80i_ld_inn_ix,
	/** Load fixed memory location from IY */
	z80i_ld_inn_iy,
	/** Load SP from HL */
	z80i_ld_sp_hl,
	/** Load SP from IX */
	z80i_ld_sp_ix,
	/** Load SP from IY */
	z80i_ld_sp_iy,
	/** Push register pair */
	z80i_push_qq,
	/** Push IX */
	z80i_push_ix,
	/** Push IY */
	z80i_push_iy,
	/** Pop register pair */
	z80i_pop_qq,
	/** Pop IX */
	z80i_pop_ix,
	/** Pop IY */
	z80i_pop_iy,

	/** Exchange DE and HL */
	z80i_ex_de_hl,
	/** Exchange AF and AF' */
	z80i_ex_af_afp,
	/** Exchange BC, DE, HL with BC', DE', HL' */
	z80i_exx,
	/** Exchange (SP) with HL */
	z80i_ex_isp_hl,
	/** Exchange (SP) with IX */
	z80i_ex_isp_ix,
	/** Exchange (SP) with IY */
	z80i_ex_isp_iy,
	/** Load, increment */
	z80i_ldi,
	/** Load, increment, repeat */
	z80i_ldir,
	/** Load, decrement */
	z80i_ldd,
	/** Load, decrement, repeat */
	z80i_lddr,
	/** Compare, increment */
	z80i_cpi,
	/** Compare, increment, repeat */
	z80i_cpir,
	/** Compare, decrement */
	z80i_cpd,
	/** Compare, decrement, repeat */
	z80i_cpdr,

	/** Add register to A */
	z80i_add_a_r,
	/** Add 8-bit immediate to A */
	z80i_add_a_n,
	/** Add (HL) to A */
	z80i_add_a_ihl,
	/** Add (IX+d) to A */
	z80i_add_a_iixd,
	/** Add (IY+d) to A */
	z80i_add_a_iiyd,
	/** Add register to A with carry */
	z80i_adc_a_r,
	/** Add 8-bit immediate to A with carry */
	z80i_adc_a_n,
	/** Add (HL) to A with carry */
	z80i_adc_a_ihl,
	/** Add (IX+d) to A with carry */
	z80i_adc_a_iixd,
	/** Add (IY+d) to A with carry */
	z80i_adc_a_iiyd,
	/** Subtract register */
	z80i_sub_r,
	/** Subtract 8-bit immediate */
	z80i_sub_n,
	/** Subtract (HL) */
	z80i_sub_ihl,
	/** Subtract (IX+d) */
	z80i_sub_iixd,
	/** Subtract (IY+d) */
	z80i_sub_iiyd,
	/** Subtract register to A with carry */
	z80i_sbc_a_r,
	/** Subtract 8-bit immediate to A with carry */
	z80i_sbc_a_n,
	/** Subtract (HL) to A with carry */
	z80i_sbc_a_ihl,
	/** Subtract (IX+d) to A with carry */
	z80i_sbc_a_iixd,
	/** Subtract (IY+d) to A with carry */
	z80i_sbc_a_iiyd,
	/** Bitwise AND with register */
	z80i_and_r,
	/** Bitwise AND with 8-bit immediate */
	z80i_and_n,
	/** Bitwise AND with (HL) */
	z80i_and_ihl,
	/** Bitwise AND with (IX+d) */
	z80i_and_iixd,
	/** Bitwise AND with (IY+d) */
	z80i_and_iiyd,
	/** Bitwise OR with register */
	z80i_or_r,
	/** Bitwise OR with 8-bit immediate */
	z80i_or_n,
	/** Bitwise OR with (HL) */
	z80i_or_ihl,
	/** Bitwise OR with (IX+d) */
	z80i_or_iixd,
	/** Bitwise OR with (IY+d) */
	z80i_or_iiyd,
	/** Bitwise XOR with register */
	z80i_xor_r,
	/** Bitwise XOR with 8-bit immediate */
	z80i_xor_n,
	/** Bitwise XOR with (HL) */
	z80i_xor_ihl,
	/** Bitwise XOR with (IX+d) */
	z80i_xor_iixd,
	/** Bitwise XOR with (IY+d) */
	z80i_xor_iiyd,
	/** Compare with register */
	z80i_cp_r,
	/** Compare with 8-bit immediate */
	z80i_cp_n,
	/** Compare with (HL) */
	z80i_cp_ihl,
	/** Compare with (IX+d) */
	z80i_cp_iixd,
	/** Compare with (IY+d) */
	z80i_cp_iiyd,
	/** Increment register */
	z80i_inc_r,
	/** Increment (HL) */
	z80i_inc_ihl,
	/** Increment (IX+d) */
	z80i_inc_iixd,
	/** Increment (IY+d) */
	z80i_inc_iiyd,
	/** Decrement register */
	z80i_dec_r,
	/** Decrement (HL) */
	z80i_dec_ihl,
	/** Decrement (IX+d) */
	z80i_dec_iixd,
	/** Decrement (IY+d) */
	z80i_dec_iiyd,

	/** Decimal adjust accumulator */
	z80i_daa,
	/** Complement */
	z80i_cpl,
	/** Negate */
	z80i_neg,
	/** Complement carry flag */
	z80i_ccf,
	/** Set carry flag */
	z80i_scf,
	/** No operation */
	z80i_nop,
	/** Halt */
	z80i_halt,
	/** Disable interrupt */
	z80i_di,
	/** Enable interrupt */
	z80i_ei,
	/** Set interrupt mode 0 */
	z80i_im_0,
	/** Set interrupt mode 1 */
	z80i_im_1,
	/** Set interrupt mode 2 */
	z80i_im_2,

	/** Add register pair to HL */
	z80i_add_hl_ss,
	/** Add register pair to HL with carry */
	z80i_adc_hl_ss,
	/** Subtract register pair from HL with carry */
	z80i_sbc_hl_ss,
	/** Add register pair to IX */
	z80i_add_ix_pp,
	/** Add register pair to IY */
	z80i_add_iy_rr,
	/** Increment register pair */
	z80i_inc_ss,
	/** Increment IX */
	z80i_inc_ix,
	/** Increment IY */
	z80i_inc_iy,
	/** Decrement register pair */
	z80i_dec_ss,
	/** Decrement IX */
	z80i_dec_ix,
	/** Decrement IY */
	z80i_dec_iy,

	/** Rotate left circular accumulator */
	z80i_rlca,
	/** Rotate left accumulator */
	z80i_rla,
	/** Rotate right circular accumulator */
	z80i_rrca,
	/** Rotate right accumulator */
	z80i_rra,
	/** Rotate left circular register */
	z80i_rlc_r,
	/** Rotate left circular (HL) */
	z80i_rlc_ihl,
	/** Rotate left circular (IX+d) */
	z80i_rlc_iixd,
	/** Rotate left circular (IY+d) */
	z80i_rlc_iiyd,
	/** Rotate left register */
	z80i_rl_r,
	/** Rotate left (HL) */
	z80i_rl_ihl,
	/** Rotate left (IX+d) */
	z80i_rl_iixd,
	/** Rotate left (IY+d) */
	z80i_rl_iiyd,
	/** Rotate right circular register */
	z80i_rrc_r,
	/** Rotate right circular (HL) */
	z80i_rrc_ihl,
	/** Rotate right circular (IX+d) */
	z80i_rrc_iixd,
	/** Rotate right circular (IY+d) */
	z80i_rrc_iiyd,
	/** Rotate right register */
	z80i_rr_r,
	/** Rotate right (HL) */
	z80i_rr_ihl,
	/** Rotate right (IX+d) */
	z80i_rr_iixd,
	/** Rotate right (IY+d) */
	z80i_rr_iiyd,
	/** Shift left arithmetic register */
	z80i_sla_r,
	/** Shift left arithmetic (HL) */
	z80i_sla_ihl,
	/** Shift left arithmetic (IX+d) */
	z80i_sla_iixd,
	/** Shift left arithmetic (IY+d) */
	z80i_sla_iiyd,
	/** Shift right arithmetic register */
	z80i_sra_r,
	/** Shift right arithmetic (HL) */
	z80i_sra_ihl,
	/** Shift right arithmetic (IX+d) */
	z80i_sra_iixd,
	/** Shift right arithmetic (IY+d) */
	z80i_sra_iiyd,
	/** Shift right logical register */
	z80i_srl_r,
	/** Shift right logical (HL) */
	z80i_srl_ihl,
	/** Shift right logical (IX+d) */
	z80i_srl_iixd,
	/** Shift right logical (IY+d) */
	z80i_srl_iiyd,
	/** RLD */
	z80i_rld,
	/** RRD */
	z80i_rrd,

	/** Test bit b in register */
	z80i_bit_b_r,
	/** Test bit b in (HL) */
	z80i_bit_b_ihl,
	/** Test bit b in (IX+d) */
	z80i_bit_b_iixd,
	/** Test bit b in (IY+d) */
	z80i_bit_b_iiyd,
	/** Set bit b in register */
	z80i_set_b_r,
	/** Set bit b in (HL) */
	z80i_set_b_ihl,
	/** Set bit b in (IX+d) */
	z80i_set_b_iixd,
	/** Set bit b in (IY+d) */
	z80i_set_b_iiyd,
	/** Reset bit b in register */
	z80i_res_b_r,
	/** Reset bit b in (HL) */
	z80i_res_b_ihl,
	/** Reset bit b in (IX+d) */
	z80i_res_b_iixd,
	/** Reset bit b in (IY+d) */
	z80i_res_b_iiyd,

	/** Jump to address */
	z80i_jp_nn,
	/** Conditional jump to address */
	z80i_jp_cc_nn,
	/** Relative jump */
	z80i_jr_e,
	/** Relative jump if carry */
	z80i_jr_c_e,
	/** Relative jump if not carry */
	z80i_jr_nc_e,
	/** Relative jump if zero */
	z80i_jr_z_e,
	/** Relative jump if not zero */
	z80i_jr_nz_e,
	/** Jump to HL (mnemonic is JP (HL)) */
	z80i_jp_hl,
	/** Jump to IX (mnemonic is JP (IX)) */
	z80i_jp_ix,
	/** Jump to IY (mnemonic is JP (IY)) */
	z80i_jp_iy,
	/** Decrement, jump if not zero */
	z80i_djnz_e,

	/** Call address */
	z80i_call_nn,
	/** Conditional call */
	z80i_call_cc_nn,
	/** Return */
	z80i_ret,
	/** Contitional return */
	z80i_ret_cc,
	/** Return from interrupt */
	z80i_reti,
	/** Return from NMI */
	z80i_retn,
	/** Restart */
	z80i_rst_p,

	/** Input from fixed port to A */
	z80i_in_a_in,
	/** Input from port (C) to register */
	z80i_in_r_ic,
	/** Input, increment */
	z80i_ini,
	/** Input, increment, repeat */
	z80i_inir,
	/** Input, decrement */
	z80i_ind,
	/** Input, decrement, repeat */
	z80i_indr,
	/** Output A to fixed port */
	z80i_out_in_a,
	/** Output register to port (C) */
	z80i_in_ic_r,
	/** Output, increment */
	z80i_outi,
	/** Output, increment, repeat */
	z80i_otir,
	/** Output, decrement */
	z80i_outd,
	/** Output, decrement, repeat */
	z80i_otdr,

	/** Load virtual register from virtual register */
	z80i_ld_vr_vr,
	/** Load virtual register from 8-bit immediate */
	z80i_ld_vr_n,
	/** Load virtual register from (HL) */
	z80i_ld_vr_ihl,
	/** Load virtual register from (IX+d) */
	z80i_ld_vr_iixd,
	/** Load virtual register from address stored in virt. reg. pair */
	z80i_ld_vr_ivrr,
	/** Load virtual register from address stored in virt. reg. pair + d. */
	z80i_ld_vr_ivrrd,
	/** Load (HL) from virtual register */
	z80i_ld_ihl_vr,
	/** Load virtual register to address stored in virt. reg. pair */
	z80i_ld_ivrr_vr,
	/** Load virtual register to address stored in virt. reg. pair + d. */
	z80i_ld_ivrrd_vr,
	/** Load 8-bit immediate to address stored in virt. reg. pair */
	z80i_ld_ivrr_n,
	/** Load 8-bit immediate to address stored in virt. reg. pair + d. */
	z80i_ld_ivrrd_n,
	/** Load virtual register from fixed memory location */
	z80i_ld_vr_inn,
	/** Load fixed memory location from virtual register */
	z80i_ld_inn_vr,

	/** Load virtual register pair from virtual register pair.
	 *
	 * This supplants a pair of 8-bit loads operating on the two
	 * halves of the virtual register pair.
	 */
	z80i_ld_vrr_vrr,

	/** Loads 8-bit register from virtual register.
	 *
	 * This allows us to place a value in a particular 8-bit register,
	 * e.g. the acummulator.
	 */
	z80i_ld_r_vr,

	/** Loads virtual register from 8-bit register.
	 *
	 * This allows us to read a value from a particular 8-bit register,
	 * e.g. the acummulator.
	 */
	z80i_ld_vr_r,

	/** Load 16-bit register from virtual register pair.
	 *
	 * This allows us to place a value in a particular 16-bit register,
	 * such as when returning value from a function
	 */
	z80i_ld_r16_vrr,

	/** Load virtual register pair from 16-bit register.
	 *
	 * This allows us to read a value from a particular 16-bit register,
	 * such as when reading return value from a function
	 */
	z80i_ld_vrr_r16,

	/** Load virtual register pair from (IX+d) */
	z80i_ld_vrr_iixd,

	/** Load virt. register pair from 16-bit immediate */
	z80i_ld_vrr_nn,
	/** Load virt. register pair from fixed memory address */
	z80i_ld_vrr_inn,
	/** Load virt. register pair from SP + 16-bit immediate */
	z80i_ld_vrr_spnn,
	/** Load fixed memory address from virt. register pair */
	z80i_ld_inn_vrr,
	/** Load SP from virt. register pair */
	z80i_ld_sp_vrr,
	/** Push virt. register pair */
	z80i_push_vrr,
	/** Pop virt. register pair */
	z80i_pop_vrr,

	/** Exchange virt. register pairs */
	z80i_ex_vrr_vrr,
	/** Exchange (SP) with virt. register pair */
	z80i_ex_isp_vrr,

	/** Add virtual register to A */
	z80i_add_a_vr,
	/** Add indirect memory location to A */
	z80i_add_a_ivrr,
	/** Add displaced indirect memory location to A */
	z80i_add_a_ivrrd,
	/** Add virtual register to A with carry */
	z80i_adc_a_vr,
	/** Add indirect memory location to A with carry */
	z80i_adc_a_ivrr,
	/** Add displaced indirect memory location to A with carry */
	z80i_adc_a_ivrrd,
	/** Subtract virtual register */
	z80i_sub_vr,
	/** Subtract indirect memory location */
	z80i_sub_ivrr,
	/** Subtract displaced indirect memory location */
	z80i_sub_ivrrd,
	/** Subtract virtual register from A with carry */
	z80i_sbc_a_vr,
	/** Subtract indirect memory location from A with carry */
	z80i_sbc_a_ivrr,
	/** Subtract displaced indirect memory location from A with carry */
	z80i_sbc_a_ivrrd,
	/** Bitwise AND with virtual register */
	z80i_and_vr,
	/** Bitwise AND with indirect memory location */
	z80i_and_ivrr,
	/** Bitwise AND with displaced indirect memory location */
	z80i_and_ivrrd,
	/** Bitwise OR with virtual register */
	z80i_or_vr,
	/** Bitwise OR with indirect memory location */
	z80i_or_ivrr,
	/** Bitwise OR with displaced indirect memory location */
	z80i_or_ivrrd,
	/** Bitwise XOR with virtual register */
	z80i_xor_vr,
	/** Bitwise XOR with indirect memory location */
	z80i_xor_ivrr,
	/** Bitwise XOR with displaced indirect memory location */
	z80i_xor_ivrrd,
	/** Compare with virtual register */
	z80i_cp_vr,
	/** Compare with indirect memory location */
	z80i_cp_ivrr,
	/** Compare with displaced indirect memory location */
	z80i_cp_ivrrd,
	/** Increment virtual register */
	z80i_inc_vr,
	/** Increment indirect memory location */
	z80i_inc_ivrr,
	/** Increment displaced indirect memory location */
	z80i_inc_ivrrd,
	/** Decrement virtual register */
	z80i_dec_vr,
	/** Decrement indirect memory location */
	z80i_dec_ivrr,
	/** Decrement displaced indirect memory location */
	z80i_dec_ivrrd,

	/** Negate virtual register */
	z80i_neg_vr,

	/** Add virtual register pair to virtual register pair */
	z80i_add_vrr_vrr,
	/** Add virtual register pair to virtual register pair with carry */
	z80i_adc_vrr_vrr,
	/** Subtract virt. register pair from virt. register pair with carry */
	z80i_sbc_vrr_vrr,
	/** Subtract virt. register pair from virt. register pair */
	z80i_sub_vrr_vrr,
	/** Increment virtual register pair */
	z80i_inc_vrr,
	/** Decrement virtual register pair */
	z80i_dec_vrr,

	/* Add SP to virtual register pair */
	z80i_add_vrr_sp,

	/** Rotate left circular virtual register */
	z80i_rlc_vr,
	/** Rotate left circular indirect memory location */
	z80i_rlc_ivrr,
	/** Rotate left circular displaced indirect memory location */
	z80i_rlc_ivrrd,
	/** Rotate left virtual register */
	z80i_rl_vr,
	/** Rotate left indirect memory location */
	z80i_rl_ivrr,
	/** Rotate left displaced indirect memory location */
	z80i_rl_ivrrd,
	/** Rotate right circular virtual register */
	z80i_rrc_vr,
	/** Rotate right circular indirect memory location */
	z80i_rrc_ivrr,
	/** Rotate right circular displaced indirect memory location */
	z80i_rrc_ivrrd,
	/** Rotate right virtual register */
	z80i_rr_vr,
	/** Rotate right indirect memory location */
	z80i_rr_ivrr,
	/** Rotate right displaced indirect memory location */
	z80i_rr_ivrrd,
	/** Shift left arithmetic virtual register */
	z80i_sla_vr,
	/** Shift left arithmetic indirect memory location */
	z80i_sla_ivrr,
	/** Shift left arithmetic displaced indirect memory location */
	z80i_sla_ivvrd,
	/** Shift right arithmetic virtual register */
	z80i_sra_vr,
	/** Shift right arithmetic indirect memory location */
	z80i_sra_ivrr,
	/** Shift right arithmetic displaced indirect memory location */
	z80i_sra_ivrrd,
	/** Shift right logical virtual register */
	z80i_srl_vr,
	/** Shift right logical indirect memory location */
	z80i_srl_ivrr,
	/** Shift right logical displaced indirect memory location */
	z80i_srl_ivrrd,

	/** Test virtual register bit */
	z80i_bit_b_vr,
	/** Test indirect memory location bit */
	z80i_bit_b_ivrr,
	/** Test displaced indirect memory location bit */
	z80i_bit_b_ivvrd,
	/** Set virtual register bit */
	z80i_set_b_vr,
	/** Set indirect memory location bit */
	z80i_set_b_ivrr,
	/** Set displaced indirect memory location bit */
	z80i_set_b_ivvrd,
	/** Reset virtual register bit */
	z80i_res_b_vr,
	/** Reset indirect memory location bit */
	z80i_res_b_ivrr,
	/** Reset displaced indirect memory location bit */
	z80i_res_b_ivvrd,

	/** Jump to address in virtual register pair */
	z80i_jp_vrr
} z80ic_instr_type_t;

/** Z80 IC register.
 *
 * The enum values correspond to actual encoding in instruction.
 */
typedef enum {
	/** A register */
	z80ic_reg_a = 0x7,
	/** B register */
	z80ic_reg_b = 0x0,
	/** C register */
	z80ic_reg_c = 0x1,
	/** D register */
	z80ic_reg_d = 0x2,
	/** E register */
	z80ic_reg_e = 0x3,
	/** H register */
	z80ic_reg_h = 0x4,
	/** L register */
	z80ic_reg_l = 0x5
} z80ic_reg_t;

/** Z80 IC dd register pair.
 *
 * One of the four 16-bit registers BC, DE, HL, SP.
 */
typedef enum {
	/** BC register pair */
	z80ic_dd_bc,
	/** DE register pair */
	z80ic_dd_de,
	/** HL register pair */
	z80ic_dd_hl,
	/** SP 16-bit register */
	z80ic_dd_sp
} z80ic_dd_t;

/** Z80 IC pp register pair.
 *
 * One of the four 16-bit registers BC, DE, IX, SP.
 */
typedef enum {
	/** BC register pair */
	z80ic_pp_bc,
	/** DE register pair */
	z80ic_pp_de,
	/** HL register pair */
	z80ic_pp_ix,
	/** SP 16-bit register */
	z80ic_pp_sp
} z80ic_pp_t;

/** Z80 IC qq register pair.
 *
 * One of the four 16-bit registers BC, DE, HL, AF.
 */
typedef enum {
	/** BC register pair */
	z80ic_qq_bc,
	/** DE register pair */
	z80ic_qq_de,
	/** HL register pair */
	z80ic_qq_hl,
	/** AF register pair */
	z80ic_qq_af
} z80ic_qq_t;

/** Z80 IC ss register pair.
 *
 * One of the four 16-bit registers BC, DE, HL, SP.
 */
typedef enum {
	/** BC register pair */
	z80ic_ss_bc,
	/** DE register pair */
	z80ic_ss_de,
	/** HL register pair */
	z80ic_ss_hl,
	/** SP 16-bit register */
	z80ic_ss_sp
} z80ic_ss_t;

/** Z80 IC condition.
 *
 * Used in conditional jumps.
 */
typedef enum {
	/** Non-Zero */
	z80ic_cc_nz,
	/** Zero */
	z80ic_cc_z,
	/** No Carry */
	z80ic_cc_nc,
	/** Carry */
	z80ic_cc_c,
	/** Parity Odd */
	z80ic_cc_po,
	/** Parity Even */
	z80ic_cc_pe,
	/** Sign Positive */
	z80ic_cc_p,
	/** Sign Negative */
	z80ic_cc_m
} z80ic_cc_t;

/** Z80 IC 16-bit register.
 *
 * This specifies any register pair / 16-bit register. This is not used
 * in any Z80 instuction, but we can used it for pseudo-instructions such as
 * ld_r16_vrr (although we need not actually implement all the variants).
 */
typedef enum {
	/** AF register pair */
	z80ic_r16_af,
	/** BC register pair */
	z80ic_r16_bc,
	/** DE register pair */
	z80ic_r16_de,
	/** HL register pair */
	z80ic_r16_hl,
	/** IX 16-bit register */
	z80ic_r16_ix,
	/** IY 16-bit register */
	z80ic_r16_iy,
	/** SP 16-bit register */
	z80ic_r16_sp
} z80ic_r16_t;

enum {
	z80ic_r16_limit = z80ic_r16_sp + 1
};

/** Virtual register part */
typedef enum {
	/** Entire 8-bit virtual register */
	z80ic_vrp_r8,
	/** Lower half of virtual register pair */
	z80ic_vrp_r16l,
	/** Upper half of virtual register pair */
	z80ic_vrp_r16h
} z80ic_vr_part_t;

/** Z80 IC register operand.
 *
 * This is simply one of the real general-purpose 8-bit registers
 * (A, B, C, D, E, H, L) as used by most opcodes.
 */
typedef struct {
	/** Register */
	z80ic_reg_t reg;
} z80ic_oper_reg_t;

/** Z80 IC dd register pair operand.
 *
 * One of the four 16-bit registers BC, DE, HL, SP.
 */
typedef struct {
	/** Register */
	z80ic_dd_t rdd;
} z80ic_oper_dd_t;

/** Z80 IC pp register pair operand.
 *
 * One of the four 16-bit registers BC, DE, IX, SP.
 */
typedef struct {
	/** Register */
	z80ic_pp_t rpp;
} z80ic_oper_pp_t;

/** Z80 IC qq register pair operand.
 *
 * One of the four 16-bit registers BC, DE, HL, AF.
 */
typedef struct {
	/** Register */
	z80ic_qq_t rqq;
} z80ic_oper_qq_t;

/** Z80 IC ss register pair operand.
 *
 * One of the four 16-bit registers BC, DE, IX, SP.
 */
typedef struct {
	/** Register */
	z80ic_ss_t rss;
} z80ic_oper_ss_t;

/** Z80 IC 16-bit register operand.
 *
 * This is only used with pseudo-instructions such as ld_r16_vrr, real
 * instructions use pp, qq, rr, ss, each of which are sets of four 16-bit
 * registers (giving just 4 choices for each particular opcode).
 */
typedef struct {
	/** 16-bit register */
	z80ic_r16_t r16;
} z80ic_oper_r16_t;

/** Z80 IC immediate 8-bit operand.
 *
 * This is a constant value.
 */
typedef struct {
	/** Immediate value */
	uint8_t imm8;
} z80ic_oper_imm8_t;

/** Z80 IC immediate 16-bit operand.
 *
 * This can be either a number or a symbol reference (in which case
 * it evaluates to the value of that symbol. This is needed to refer
 * to symbols in assembly.
 */
typedef struct {
	/** Symbol reference or @c NULL to use immediate value */
	char *symbol;
	/** Immediate value */
	uint16_t imm16;
} z80ic_oper_imm16_t;

/** Z80 IC virtual register operand */
typedef struct {
	/** Virtual register number */
	unsigned vregno;
	/** Virtual register part */
	z80ic_vr_part_t part;
} z80ic_oper_vr_t;

/** Z80 IC virtual register pair operand */
typedef struct {
	/** Virtual register pair number */
	unsigned vregno;
} z80ic_oper_vrr_t;

/** Z80 IC instruction */
typedef struct {
	/** Instruction Type */
	z80ic_instr_type_t itype;
	/** Entire/type-specific data */
	void *ext;
} z80ic_instr_t;

/** Z80 IC load register from register instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_reg_t *dest;
	/** Source register */
	z80ic_oper_reg_t *src;
} z80ic_ld_r_r_t;

/** Z80 IC load register from 8-bit immediate */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_reg_t *dest;
	/** Immediate operand */
	z80ic_oper_imm8_t *imm8;
} z80ic_ld_r_n_t;

/** Z80 IC load register from (HL) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_reg_t *dest;
} z80ic_ld_r_ihl_t;

/** Z80 IC load register from (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_reg_t *dest;
	/** Displacement */
	int8_t disp;
} z80ic_ld_r_iixd_t;

/** Z80 IC load register from (IY+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_reg_t *dest;
	/** Displacement */
	int8_t disp;
} z80ic_ld_r_iiyd_t;

/** Z80 IC load (HL) from register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source register */
	z80ic_oper_reg_t *src;
} z80ic_ld_ihl_r_t;

/** Z80 IC load (IX+d) from register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
	/** Source register */
	z80ic_oper_reg_t *src;
} z80ic_ld_iixd_r_t;

/** Z80 IC load (IX+d) from 8-bit immediate */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
	/** Immediate operand */
	z80ic_oper_imm8_t *imm8;
} z80ic_ld_iixd_n_t;

/** Z80 IC load 16-bit dd register from 16-bit immediate */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_dd_t *dest;
	/** Immediate */
	z80ic_oper_imm16_t *imm16;
} z80ic_ld_dd_nn_t;

/** Z80 IC load IY register from 16-bit immediate */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Immediate */
	z80ic_oper_imm16_t *imm16;
} z80ic_ld_ix_nn_t;

/** Z80 IC load SP register from IX register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
} z80ic_ld_sp_ix_t;

/** Z80 IC push 16-bit qq register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source register */
	z80ic_oper_qq_t *src;
} z80ic_push_qq_t;

/** Z80 IC push IX instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
} z80ic_push_ix_t;

/** Z80 IC pop 16-bit qq register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_qq_t *src;
} z80ic_pop_qq_t;

/** Z80 IC pop IX instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
} z80ic_pop_ix_t;

/** Z80 IC add (IX+d) to A */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_add_a_iixd_t;

/** Z80 IC add (IX+d) to A with carry */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_adc_a_iixd_t;

/** Z80 IC subtract 8-bit immediate */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Immediate */
	z80ic_oper_imm8_t *imm8;
} z80ic_sub_n_t;

/** Z80 IC subtract (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_sub_iixd_t;

/** Z80 IC subtract (IX+d) from A with carry */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_sbc_a_iixd_t;

/** Z80 IC bitwise AND with register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source register */
	z80ic_oper_reg_t *src;
} z80ic_and_r_t;

/** Z80 IC bitwise AND with (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_and_iixd_t;

/** Z80 IC bitwise OR with (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_or_iixd_t;

/** Z80 IC bitwise XOR with (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_xor_iixd_t;

/** Z80 IC increment (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_inc_iixd_t;

/** Z80 IC decrement (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_dec_iixd_t;

/** Z80 IC complement */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
} z80ic_cpl_t;

/** Z80 IC no operation */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
} z80ic_nop_t;

/** Z80 IC add 16-bit register to HL */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source register pair */
	z80ic_oper_ss_t *src;
} z80ic_add_hl_ss_t;

/** Z80 IC subtract 16-bit register from HL with carry */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source register pair */
	z80ic_oper_ss_t *src;
} z80ic_sbc_hl_ss_t;

/** Z80 IC add 16-bit register to IX */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source register pair */
	z80ic_oper_pp_t *src;
} z80ic_add_ix_pp_t;

/** Z80 IC increment 16-bit ss register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination / source register pair */
	z80ic_oper_ss_t *dest;
} z80ic_inc_ss_t;

/** Z80 IC rotate left accumulator instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
} z80ic_rla_t;

/** Z80 IC rotate left (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_rl_iixd_t;

/** Z80 IC rotate right (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_rr_iixd_t;

/** Z80 IC shift left arithmetic (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_sla_iixd_t;

/** Z80 IC shift right arithmetic (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_sra_iixd_t;

/** Z80 IC shift right logical (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Displacement */
	int8_t disp;
} z80ic_srl_iixd_t;

/** Z80 IC test bit of (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Bit (0 - 7) */
	uint8_t bit;
	/** Displacement */
	int8_t disp;
} z80ic_bit_b_iixd_t;

/** Z80 IC jump direct instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Immediate */
	z80ic_oper_imm16_t *imm16;
} z80ic_jp_nn_t;

/** Z80 IC conditional jump direct instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Condition */
	z80ic_cc_t cc;
	/** Immediate */
	z80ic_oper_imm16_t *imm16;
} z80ic_jp_cc_nn_t;

/** Z80 IC call direct instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Immediate */
	z80ic_oper_imm16_t *imm16;
} z80ic_call_nn_t;

/** Z80 IC return instruction */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
} z80ic_ret_t;

/** Z80 IC load virtual register from virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register */
	z80ic_oper_vr_t *dest;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_ld_vr_vr_t;

/** Z80 IC load virtual register from 8-bit immediate */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register */
	z80ic_oper_vr_t *dest;
	/** Immediate */
	z80ic_oper_imm8_t *imm8;
} z80ic_ld_vr_n_t;

/** Z80 IC load virtual register from (HL) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register */
	z80ic_oper_vr_t *dest;
} z80ic_ld_vr_ihl_t;

/** Z80 IC load virtual register from (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register */
	z80ic_oper_vr_t *dest;
	/** Displacement */
	int8_t disp;
} z80ic_ld_vr_iixd_t;

/** Z80 IC load (HL) from virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_ld_ihl_vr_t;

/** Z80 IC load virtual register pair from virtual register pair */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register pair */
	z80ic_oper_vrr_t *dest;
	/** Source virtual register pair */
	z80ic_oper_vrr_t *src;
} z80ic_ld_vrr_vrr_t;

/** Z80 IC load 8-bit register from virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination register */
	z80ic_oper_reg_t *dest;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_ld_r_vr_t;

/** Z80 IC load virtual register from 8-bit register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register */
	z80ic_oper_vr_t *dest;
	/** Source register */
	z80ic_oper_reg_t *src;
} z80ic_ld_vr_r_t;

/** Z80 IC load 16-bit register from virtual register pair */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination 16-bit register */
	z80ic_oper_r16_t *dest;
	/** Source virtual register pair */
	z80ic_oper_vrr_t *src;
} z80ic_ld_r16_vrr_t;

/** Z80 IC load virtual register pair from 16-bit register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register pair */
	z80ic_oper_vrr_t *dest;
	/** Source 16-bit register */
	z80ic_oper_r16_t *src;
} z80ic_ld_vrr_r16_t;

/** Z80 IC load virtual register pair from (IX+d) */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register pair */
	z80ic_oper_vrr_t *dest;
	/** Displacement */
	int8_t disp;
} z80ic_ld_vrr_iixd_t;

/** Z80 IC load virtual register pair from 16-bit immediate */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register pair */
	z80ic_oper_vrr_t *dest;
	/** Immediate */
	z80ic_oper_imm16_t *imm16;
} z80ic_ld_vrr_nn_t;

/** Z80 IC load SP+imm16 to virtual register pair */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register pair */
	z80ic_oper_vrr_t *dest;
	/** Immediate */
	z80ic_oper_imm16_t *imm16;
} z80ic_ld_vrr_spnn_t;

/** Z80 IC push virtual register pair */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register pair */
	z80ic_oper_vrr_t *src;
} z80ic_push_vrr_t;

/** Z80 IC add virtual register to A */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_add_a_vr_t;

/** Z80 IC add virtual register to A with carry */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_adc_a_vr_t;

/** Z80 IC subtract virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_sub_vr_t;

/** Z80 IC subtract virtual register from A with carry */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_sbc_a_vr_t;

/** Z80 IC bitwise AND with virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_and_vr_t;

/** Z80 IC bitwise OR with virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_or_vr_t;

/** Z80 IC bitwise XOR with virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_xor_vr_t;

/** Z80 IC increment virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register */
	z80ic_oper_vr_t *vr;
} z80ic_inc_vr_t;

/** Z80 IC decrement virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register */
	z80ic_oper_vr_t *vr;
} z80ic_dec_vr_t;

/** Z80 IC add virtual register pair to virtual register pair */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register pair */
	z80ic_oper_vrr_t *dest;
	/** Source virtual register pair */
	z80ic_oper_vrr_t *src;
} z80ic_add_vrr_vrr_t;

/** Z80 IC subtract virtual register pair from virtual register pair */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Destination virtual register pair */
	z80ic_oper_vrr_t *dest;
	/** Source virtual register pair */
	z80ic_oper_vrr_t *src;
} z80ic_sub_vrr_vrr_t;

/** Z80 IC increment virtual register pair */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register pair */
	z80ic_oper_vrr_t *vrr;
} z80ic_inc_vrr_t;

/** Z80 IC rotate left virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register */
	z80ic_oper_vr_t *vr;
} z80ic_rl_vr_t;

/** Z80 IC rotate right virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register */
	z80ic_oper_vr_t *vr;
} z80ic_rr_vr_t;

/** Z80 IC shift left arithmetic virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register */
	z80ic_oper_vr_t *vr;
} z80ic_sla_vr_t;

/** Z80 IC shift right arithmetic virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register */
	z80ic_oper_vr_t *vr;
} z80ic_sra_vr_t;

/** Z80 IC shift right logical virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Virtual register */
	z80ic_oper_vr_t *vr;
} z80ic_srl_vr_t;

/** Z80 IC shift right arithmetic virtual register */
typedef struct {
	/** Base object */
	z80ic_instr_t instr;
	/** Bit (0 - 7) */
	uint8_t bit;
	/** Source virtual register */
	z80ic_oper_vr_t *src;
} z80ic_bit_b_vr_t;

/** Z80 IC labeled block entry */
typedef struct {
	/** Containing labeled block */
	struct z80ic_lblock *lblock;
	/** Link to @c lblock->entries */
	link_t lentries;
	/** Label or @c NULL if none */
	char *label;
	/** Instruction */
	z80ic_instr_t *instr;
} z80ic_lblock_entry_t;

/** Z80 IC labeled block */
typedef struct z80ic_lblock {
	/** Entries */
	list_t entries; /* of z80ic_lblock_entry_t */
} z80ic_lblock_t;

/** Z80 IC data entry type */
typedef enum {
	/** Define byte */
	z80icd_defb,
	/** Define 16-bit value (word) */
	z80icd_defw,
	/** Define 32-bit value (doubleword) */
	z80icd_defdw,
	/** Define 64-bit value (quadword) */
	z80icd_defqw
} z80ic_dentry_type_t;

/** Z80 IC data entry */
typedef struct {
	/** Data entry type */
	z80ic_dentry_type_t dtype;
	/** Value */
	uint64_t value;
} z80ic_dentry_t;

/** Z80 IC data block entry */
typedef struct {
	/** Containing data block */
	struct z80ic_dblock *dblock;
	/** Link to @c dblock->entries */
	link_t lentries;
	/** Data entry */
	z80ic_dentry_t *dentry;
} z80ic_dblock_entry_t;

/** Z80 IC data block */
typedef struct z80ic_dblock {
	/** Entries */
	list_t entries; /* of z80ic_dblock_entry_t */
} z80ic_dblock_t;

/** Z80 IC declaration Type */
typedef enum {
	/** Extern symbol declaration */
	z80icd_extern,
	/** Variable declaration */
	z80icd_var,
	/** Procedure declaration */
	z80icd_proc
} z80ic_decln_type_t;

/** Z80 IC declaration */
typedef struct {
	/** Containing module */
	struct z80ic_module *module;
	/** Link to @c module->declns */
	link_t ldeclns;
	/** Declaration type */
	z80ic_decln_type_t dtype;
	/** Pointer to entire/specific structure */
	void *ext;
} z80ic_decln_t;

/** Z80 IC variable definition */
typedef struct {
	/** Base object */
	z80ic_decln_t decln;
	/** Indentifier */
	char *ident;
	/** Data block containing variable data */
	z80ic_dblock_t *dblock;
} z80ic_var_t;

/** Z80 IC local variable
 *
 * This ties a local variable name to a numeric offset in the local variable
 * area of the stack frame. This allows the IC to refer to local variables
 * by name.
 */
typedef struct {
	/** Containing procedure definition */
	struct z80ic_proc *proc;
	/** Link to @c proc->lvars */
	link_t llvars;
	/** Identifier */
	char *ident;
	/** Offset */
	uint16_t off;
} z80ic_lvar_t;

/** Z80 IC procedure definition */
typedef struct z80ic_proc {
	/** Base object */
	z80ic_decln_t decln;
	/** Indentifier */
	char *ident;
	/** Local variables (of z80ic_lvar_t) */
	list_t lvars;
	/** Labeled block containing the implementation */
	z80ic_lblock_t *lblock;
	/** Number of used virtual registers */
	size_t used_vrs;
} z80ic_proc_t;

/** Z80 IC extern symbol declaration */
typedef struct {
	/** Base object */
	z80ic_decln_t decln;
	/** Indentifier */
	char *ident;
} z80ic_extern_t;

/** Z80 IC module */
typedef struct z80ic_module {
	/** Declarations */
	list_t declns; /* of z80ic_decln_t */
} z80ic_module_t;

#endif
