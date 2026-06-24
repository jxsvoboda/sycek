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
 * Z80 Opcodes
 */

#ifndef TYPES_Z80OPC_H
#define TYPES_Z80OPC_H

/** Z80 opcodes */
enum {
	z80opc_ld_r_r = 0x40,
	z80opc_ld_r_n = 0x06,
	z80opc_ld_r_ihl = 0x46,
	z80opc_ld_r_iixd = 0xdd46u,
	z80opc_ld_r_iiyd = 0xfd46u,
	z80opc_ld_ihl_r = 0x70,
	z80opc_ld_iixd_r = 0xdd70u,
	z80opc_ld_iiyd_r = 0xfd70u,
	z80opc_ld_ihl_n = 0x36,
	z80opc_ld_iixd_n = 0xdd36u,
	z80opc_ld_iiyd_n = 0xfd36u,
	z80opc_ld_a_ibc = 0x0a,
	z80opc_ld_a_ide = 0x1a,
	z80opc_ld_a_inn = 0x3a,
	z80opc_ld_ibc_a = 0x02,
	z80opc_ld_ide_a = 0x12,
	z80opc_ld_inn_a = 0x32,
	z80opc_ld_a_i = 0xed57u,
	z80opc_ld_a_r = 0xed5fu,
	z80opc_ld_i_a = 0xed47u,
	z80opc_ld_r_a = 0xed4fu,
	z80opc_ld_dd_nn = 0x01,
	z80opc_ld_ix_nn = 0xdd21u,
	z80opc_ld_sp_ix = 0xddf9u,
	z80opc_push_qq = 0xc5,
	z80opc_push_ix = 0xdde5u,
	z80opc_pop_qq = 0xc1,
	z80opc_pop_ix = 0xdde1u,
	z80opc_add_a_n = 0xc6,
	z80opc_add_a_ihl = 0x86,
	z80opc_add_a_iixd = 0xdd86u,
	z80opc_adc_a_n = 0xce,
	z80opc_adc_a_ihl = 0x8e,
	z80opc_adc_a_iixd = 0xdd8eu,
	z80opc_sub_n = 0xd6,
	z80opc_sub_iixd = 0xdd96u,
	z80opc_sbc_a_iixd = 0xdd9eu,
	z80opc_and_r = 0xa0,
	z80opc_and_iixd = 0xdda6u,
	z80opc_or_iixd = 0xddb6u,
	z80opc_xor_r = 0xa8,
	z80opc_xor_iixd = 0xddaeu,
	z80opc_cp_n = 0xfe,
	z80opc_inc_iixd = 0xdd34u,
	z80opc_dec_r = 0x05,
	z80opc_dec_iixd = 0xdd35u,
	z80opc_cpl = 0x2f,
	z80opc_nop = 0x00,
	z80opc_add_hl_ss = 0x09,
	z80opc_sbc_hl_ss = 0xed42u,
	z80opc_add_ix_pp = 0xdd09u,
	z80opc_inc_ss = 0x03,
	z80opc_rla = 0x17,
	z80opc_rl_iixd = 0xddcb16ul,
	z80opc_rr_iixd = 0xddcb1eul,
	z80opc_sla_iixd = 0xddcb26ul,
	z80opc_sra_iixd = 0xddcb2eul,
	z80opc_srl_iixd = 0xddcb3eul,
	z80opc_bit_b_iixd = 0xddcb46ul,
	z80opc_set_b_iixd = 0xddcbc6ul,
	z80opc_jp_nn = 0xc3,
	z80opc_jp_cc_nn = 0xc2,
	z80opc_call_nn = 0xcd,
	z80opc_ret = 0xc9
};

#endif
