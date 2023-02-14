/*
 * GZX - George's ZX Spectrum Emulator
 * Instruction decode tables
 *
 * Copyright (c) 1999-2017 Jiri Svoboda
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
  this file is included into z80.c
*/

void (*const ei_op[256])(void) PROGMEM = {
  ei_nop,	ei_ld_BC_NN,	ei_ld_iBC_A,	ei_inc_BC, 	/* 0x00 */
  ei_inc_B,	ei_dec_B,	ei_ld_B_N,	ei_rlca, 	/* 0x04 */
  ei_ex_AF_xAF,	ei_add_HL_BC,	ei_ld_A_iBC,	ei_dec_BC, 	/* 0x08 */
  ei_inc_C,	ei_dec_C,	ei_ld_C_N,	ei_rrca, 	/* 0x0C */
  ei_djnz,	ei_ld_DE_NN,	ei_ld_iDE_A,	ei_inc_DE, 	/* 0x10 */
  ei_inc_D,	ei_dec_D,	ei_ld_D_N,	ei_rla, 	/* 0x14 */
  ei_jr_N,	ei_add_HL_DE,	ei_ld_A_iDE,	ei_dec_DE, 	/* 0x18 */
  ei_inc_E,	ei_dec_E,	ei_ld_E_N,	ei_rra, 	/* 0x1C */
  ei_jr_NZ_N,	ei_ld_HL_NN,	ei_ld_iNN_HL,	ei_inc_HL, 	/* 0x20 */
  ei_inc_H,	ei_dec_H,	ei_ld_H_N,	ei_daa, 	/* 0x24 */
  ei_jr_Z_N,	ei_add_HL_HL,	ei_ld_HL_iNN,	ei_dec_HL, 	/* 0x28 */
  ei_inc_L,	ei_dec_L,	ei_ld_L_N,	ei_cpl, 	/* 0x2C */
  ei_jr_NC_N,	ei_ld_SP_NN,	ei_ld_iNN_A,	ei_inc_SP, 	/* 0x30 */
  ei_inc_iHL,	ei_dec_iHL,	ei_ld_iHL_N,	ei_scf, 	/* 0x34 */
  ei_jr_C_N,	ei_add_HL_SP,	ei_ld_A_iNN,	ei_dec_SP, 	/* 0x38 */
  ei_inc_A,	ei_dec_A,	ei_ld_A_N,	ei_ccf, 	/* 0x3C */
  ei_ld_B_r,	ei_ld_B_r,	ei_ld_B_r,	ei_ld_B_r, 	/* 0x40 */
  ei_ld_B_r,	ei_ld_B_r,	ei_ld_B_iHL,	ei_ld_B_r, 	/* 0x44 */
  ei_ld_C_r,	ei_ld_C_r,	ei_ld_C_r,	ei_ld_C_r, 	/* 0x48 */
  ei_ld_C_r,	ei_ld_C_r,	ei_ld_C_iHL,	ei_ld_C_r, 	/* 0x4C */
  ei_ld_D_r,	ei_ld_D_r,	ei_ld_D_r,	ei_ld_D_r, 	/* 0x50 */
  ei_ld_D_r,	ei_ld_D_r,	ei_ld_D_iHL,	ei_ld_D_r, 	/* 0x54 */
  ei_ld_E_r,	ei_ld_E_r,	ei_ld_E_r,	ei_ld_E_r, 	/* 0x58 */
  ei_ld_E_r,	ei_ld_E_r,	ei_ld_E_iHL,	ei_ld_E_r, 	/* 0x5C */
  ei_ld_H_r,	ei_ld_H_r,	ei_ld_H_r,	ei_ld_H_r, 	/* 0x60 */
  ei_ld_H_r,	ei_ld_H_r,	ei_ld_H_iHL,	ei_ld_H_r, 	/* 0x64 */
  ei_ld_L_r,	ei_ld_L_r,	ei_ld_L_r,	ei_ld_L_r, 	/* 0x68 */
  ei_ld_L_r,	ei_ld_L_r,	ei_ld_L_iHL,	ei_ld_L_r, 	/* 0x6C */
  ei_ld_iHL_r,	ei_ld_iHL_r,	ei_ld_iHL_r,	ei_ld_iHL_r, 	/* 0x70 */
  ei_ld_iHL_r,	ei_ld_iHL_r,	ei_halt,	ei_ld_iHL_r, 	/* 0x74 */
  ei_ld_A_r,	ei_ld_A_r,	ei_ld_A_r,	ei_ld_A_r, 	/* 0x78 */
  ei_ld_A_r,	ei_ld_A_r,	ei_ld_A_iHL,	ei_ld_A_r, 	/* 0x7C */
  ei_add_A_r,	ei_add_A_r,	ei_add_A_r,	ei_add_A_r, 	/* 0x80 */
  ei_add_A_r,	ei_add_A_r,	ei_add_A_iHL,	ei_add_A_r, 	/* 0x84 */
  ei_adc_A_r,	ei_adc_A_r,	ei_adc_A_r,	ei_adc_A_r, 	/* 0x88 */
  ei_adc_A_r,	ei_adc_A_r,	ei_adc_A_iHL,	ei_adc_A_r, 	/* 0x8C */
  ei_sub_r,	ei_sub_r,	ei_sub_r,	ei_sub_r, 	/* 0x90 */
  ei_sub_r,	ei_sub_r,	ei_sub_iHL,	ei_sub_r, 	/* 0x94 */
  ei_sbc_A_r,	ei_sbc_A_r,	ei_sbc_A_r,	ei_sbc_A_r, 	/* 0x98 */
  ei_sbc_A_r,	ei_sbc_A_r,	ei_sbc_A_iHL,	ei_sbc_A_r, 	/* 0x9C */
  ei_and_r,	ei_and_r,	ei_and_r,	ei_and_r, 	/* 0xA0 */
  ei_and_r,	ei_and_r,	ei_and_iHL,	ei_and_r, 	/* 0xA4 */
  ei_xor_r,	ei_xor_r,	ei_xor_r,	ei_xor_r, 	/* 0xA8 */
  ei_xor_r,	ei_xor_r,	ei_xor_iHL,	ei_xor_r, 	/* 0xAC */
  ei_or_r,	ei_or_r,	ei_or_r,	ei_or_r, 	/* 0xB0 */
  ei_or_r,	ei_or_r,	ei_or_iHL,	ei_or_r, 	/* 0xB4 */
  ei_cp_r,	ei_cp_r,	ei_cp_r,	ei_cp_r, 	/* 0xB8 */
  ei_cp_r,	ei_cp_r,	ei_cp_iHL,	ei_cp_r, 	/* 0xBC */
  ei_ret_NZ,	ei_pop_BC,	ei_jp_NZ_NN,	ei_jp_NN, 	/* 0xC0 */
  ei_call_NZ_NN,ei_push_BC,	ei_add_A_N,	ei_rst_0, 	/* 0xC4 */
  ei_ret_Z,	ei_ret, 	ei_jp_Z_NN,	0,	 	/* 0xC8 */
  ei_call_Z_NN,	ei_call_NN,	ei_adc_A_N,	ei_rst_8, 	/* 0xCC */
  ei_ret_NC,	ei_pop_DE,	ei_jp_NC_NN,	ei_out_iN_A, 	/* 0xD0 */
  ei_call_NC_NN,ei_push_DE,	ei_sub_N,	ei_rst_10, 	/* 0xD4 */
  ei_ret_C,	ei_exx, 	ei_jp_C_NN,	ei_in_A_iN, 	/* 0xD8 */
  ei_call_C_NN,	Mi_dd,		ei_sbc_A_N,	ei_rst_18, 	/* 0xDC */
  ei_ret_PO,	ei_pop_HL,	ei_jp_PO_NN,	ei_ex_iSP_HL, 	/* 0xE0 */
  ei_call_PO_NN,ei_push_HL,	ei_and_N,	ei_rst_20, 	/* 0xE4 */
  ei_ret_PE,	ei_jp_HL,	ei_jp_PE_NN,	ei_ex_DE_HL, 	/* 0xE8 */
  ei_call_PE_NN,0,		ei_xor_N,	ei_rst_28, 	/* 0xEC */
  ei_ret_P,	ei_pop_AF,	ei_jp_P_NN,	ei_di,   	/* 0xF0 */
  ei_call_P_NN,	ei_push_AF,	ei_or_N,	ei_rst_30, 	/* 0xF4 */
  ei_ret_M,	ei_ld_SP_HL,	ei_jp_M_NN,	ei_ei,		/* 0xF8 */
  ei_call_M_NN,	Mi_fd,		ei_cp_N,	ei_rst_38	/* 0xFC */

};

void (*const ei_ddop[256])(void) PROGMEM = {
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x00 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x04 */
  Si_stray,	ei_add_IX_BC,	Si_stray,	Si_stray, 	/* 0x08 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x0C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x10 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x14 */
  Si_stray,	ei_add_IX_DE,	Si_stray,	Si_stray, 	/* 0x18 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x1C */
  Si_stray,	ei_ld_IX_NN,	ei_ld_iNN_IX,	ei_inc_IX, 	/* 0x20 */
  Ui_inc_IXh,	Ui_dec_IXh,	Ui_ld_IXh_N,	Si_stray, 	/* 0x24 */
  Si_stray,	ei_add_IX_IX,	ei_ld_IX_iNN,	ei_dec_IX, 	/* 0x28 */
  Ui_inc_IXl,	Ui_dec_IXl,	Ui_ld_IXl_N,	Si_stray, 	/* 0x2C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x30 */
  ei_inc_iIXN,	ei_dec_iIXN,	ei_ld_iIXN_N,	Si_stray, 	/* 0x34 */
  Si_stray,	ei_add_IX_SP,	Si_stray,	Si_stray, 	/* 0x38 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x3C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x40 */
  Ui_ld_B_IXh,	Ui_ld_B_IXl,	ei_ld_B_iIXN,	Si_stray, 	/* 0x44 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x48 */
  Ui_ld_C_IXh,	Ui_ld_C_IXl,	ei_ld_C_iIXN,	Si_stray, 	/* 0x4C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x50 */
  Ui_ld_D_IXh,	Ui_ld_D_IXl,	ei_ld_D_iIXN,	Si_stray, 	/* 0x54 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x58 */
  Ui_ld_E_IXh,	Ui_ld_E_IXl,	ei_ld_E_iIXN,	Si_stray, 	/* 0x5C */
  Ui_ld_IXh_B,	Ui_ld_IXh_C,	Ui_ld_IXh_D,	Ui_ld_IXh_E, 	/* 0x60 */
  Ui_ld_IXh_IXh,Ui_ld_IXh_IXl,	ei_ld_H_iIXN,	Ui_ld_IXh_A, 	/* 0x64 */
  Ui_ld_IXl_B,	Ui_ld_IXl_C,	Ui_ld_IXl_D,	Ui_ld_IXl_E, 	/* 0x68 */
  Ui_ld_IXl_IXh,Ui_ld_IXl_IXl,	ei_ld_L_iIXN,	Ui_ld_IXl_A, 	/* 0x6C */
  ei_ld_iIXN_r,	ei_ld_iIXN_r,	ei_ld_iIXN_r,	ei_ld_iIXN_r, 	/* 0x70 */
  ei_ld_iIXN_r,	ei_ld_iIXN_r,	Si_stray,	ei_ld_iIXN_r, 	/* 0x74 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x78 */
  Ui_ld_A_IXh,	Ui_ld_A_IXl,	ei_ld_A_iIXN,	Si_stray, 	/* 0x7C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x80 */
  Ui_add_A_IXh,	Ui_add_A_IXl,	ei_add_A_iIXN,	Si_stray, 	/* 0x84 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x88 */
  Ui_adc_A_IXh,	Ui_adc_A_IXl,	ei_adc_A_iIXN,	Si_stray, 	/* 0x8C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x90 */
  Ui_sub_IXh,	Ui_sub_IXl,	ei_sub_iIXN,	Si_stray, 	/* 0x94 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x98 */
  Ui_sbc_IXh,	Ui_sbc_IXl,	ei_sbc_A_iIXN,	Si_stray, 	/* 0x9C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xA0 */
  Ui_and_IXh,	Ui_and_IXl,	ei_and_iIXN,	Si_stray, 	/* 0xA4 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xA8 */
  Ui_xor_IXh,	Ui_xor_IXl,	ei_xor_iIXN,	Si_stray, 	/* 0xAC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xB0 */
  Ui_or_IXh,	Ui_or_IXl,	ei_or_iIXN,	Si_stray, 	/* 0xB4 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xB8 */
  Ui_cp_IXh,	Ui_cp_IXl,	ei_cp_iIXN,	Si_stray, 	/* 0xBC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xC0 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xC4 */
  Si_stray,	Si_stray,	Si_stray,	0,	 	/* 0xC8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xCC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xD0 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xD4 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xD8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xDC */
  Si_stray,	ei_pop_IX,	Si_stray,	ei_ex_iSP_IX, 	/* 0xE0 */
  Si_stray,	ei_push_IX,	Si_stray,	Si_stray, 	/* 0xE4 */
  Si_stray,	ei_jp_IX,	Si_stray,	Si_stray, 	/* 0xE8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xEC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xF0 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xF4 */
  Si_stray,	ei_ld_SP_IX,	Si_stray,	Si_stray, 	/* 0xF8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray  	/* 0xFC */

};

void (*const ei_edop[256])(void) PROGMEM = {
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x00 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x04 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x08 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x0C */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x10 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x14 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x18 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x1C */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x20 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x24 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x28 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x2C */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x30 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x34 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x38 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x3C */
  ei_in_B_iC,	ei_out_iC_B,	ei_sbc_HL_BC,	ei_ld_iNN_BC, 	/* 0x40 */
  ei_neg,	ei_retn,	ei_im_0,	ei_ld_I_A, 	/* 0x44 */
  ei_in_C_iC,	ei_out_iC_C,	ei_adc_HL_BC,	ei_ld_BC_iNN, 	/* 0x48 */
  Ui_neg,	ei_reti,	Ui_im_0,	ei_ld_R_A, 	/* 0x4C */
  ei_in_D_iC,	ei_out_iC_D,	ei_sbc_HL_DE,	ei_ld_iNN_DE, 	/* 0x50 */
  Ui_neg,	Ui_retn,	ei_im_1,	ei_ld_A_I, 	/* 0x54 */
  ei_in_E_iC,	ei_out_iC_E,	ei_adc_HL_DE,	ei_ld_DE_iNN, 	/* 0x58 */
  Ui_neg,	Ui_reti,	ei_im_2,	ei_ld_A_R, 	/* 0x5C */
  ei_in_H_iC,	ei_out_iC_H,	ei_sbc_HL_HL,	ei_ld_iNN_HL, 	/* 0x60 */
  Ui_neg,	Ui_retn,	Ui_im_0,	ei_rrd, 	/* 0x64 */
  ei_in_L_iC,	ei_out_iC_L,	ei_adc_HL_HL,	ei_ld_HL_iNN_x,	/* 0x68 */
  Ui_neg,	Ui_reti,	Ui_im_0,	ei_rld, 	/* 0x6C */
  Ui_in_iC,	Ui_out_iC_0,	ei_sbc_HL_SP,	ei_ld_iNN_SP, 	/* 0x70 */
  Ui_neg,	Ui_retn,	Ui_im_1,	Ui_ednop, 	/* 0x74 */
  ei_in_A_iC,	ei_out_iC_A,	ei_adc_HL_SP,	ei_ld_SP_iNN, 	/* 0x78 */
  Ui_neg,	Ui_reti,	Ui_im_2,	Ui_ednop, 	/* 0x7C */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x80 */
  Ui_ednop,	Ui_ednop,	Ui_ednop,	Ui_ednop, 	/* 0x84 */
  Ui_ednop,	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0x88 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0x8C */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0x90 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0x94 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0x98 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0x9C */
  ei_ldi,	ei_cpi,		ei_ini, 	ei_outi, 	/* 0xA0 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xA4 */
  ei_ldd,	ei_cpd,		ei_ind, 	ei_outd, 	/* 0xA8 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xAC */
  ei_ldir,	ei_cpir,	ei_inir,	ei_otir, 	/* 0xB0 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xB4 */
  ei_lddr,	ei_cpdr,	ei_indr,	ei_otdr, 	/* 0xB8 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xBC */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xC0 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xC4 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xC8 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xCC */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xD0 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xD4 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xD8 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xDC */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xE0 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xE4 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xE8 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xEC */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xF0 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xF4 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop,  	/* 0xF8 */
  Ui_ednop, 	Ui_ednop, 	Ui_ednop, 	Ui_ednop  	/* 0xFC */

};

void (*const ei_fdop[256])(void) PROGMEM = {
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x00 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x04 */
  Si_stray,	ei_add_IY_BC,	Si_stray,	Si_stray, 	/* 0x08 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x0C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x10 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x14 */
  Si_stray,	ei_add_IY_DE,	Si_stray,	Si_stray, 	/* 0x18 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x1C */
  Si_stray,	ei_ld_IY_NN,	ei_ld_iNN_IY,	ei_inc_IY, 	/* 0x20 */
  Ui_inc_IYh,	Ui_dec_IYh,	Ui_ld_IYh_N,	Si_stray, 	/* 0x24 */
  Si_stray,	ei_add_IY_IY,	ei_ld_IY_iNN,	ei_dec_IY, 	/* 0x28 */
  Ui_inc_IYl,	Ui_dec_IYl,	Ui_ld_IYl_N,	Si_stray, 	/* 0x2C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x30 */
  ei_inc_iIYN,	ei_dec_iIYN,	ei_ld_iIYN_N,	Si_stray, 	/* 0x34 */
  Si_stray,	ei_add_IY_SP,	Si_stray,	Si_stray, 	/* 0x38 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x3C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x40 */
  Ui_ld_B_IYh,	Ui_ld_B_IYl,	ei_ld_B_iIYN,	Si_stray, 	/* 0x44 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x48 */
  Ui_ld_C_IYh,	Ui_ld_C_IYl,	ei_ld_C_iIYN,	Si_stray, 	/* 0x4C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x50 */
  Ui_ld_D_IYh,	Ui_ld_D_IYl,	ei_ld_D_iIYN,	Si_stray, 	/* 0x54 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x58 */
  Ui_ld_E_IYh,	Ui_ld_E_IYl,	ei_ld_E_iIYN,	Si_stray, 	/* 0x5C */
  Ui_ld_IYh_B,	Ui_ld_IYh_C,	Ui_ld_IYh_D,	Ui_ld_IYh_E, 	/* 0x60 */
  Ui_ld_IYh_IYh,Ui_ld_IYh_IYl,	ei_ld_H_iIYN,	Ui_ld_IYh_A, 	/* 0x64 */
  Ui_ld_IYl_B,	Ui_ld_IYl_C,	Ui_ld_IYl_D,	Ui_ld_IYl_E, 	/* 0x68 */
  Ui_ld_IYl_IYh,Ui_ld_IYl_IYl,	ei_ld_L_iIYN,	Ui_ld_IYl_A, 	/* 0x6C */
  ei_ld_iIYN_r,	ei_ld_iIYN_r,	ei_ld_iIYN_r,	ei_ld_iIYN_r, 	/* 0x70 */
  ei_ld_iIYN_r,	ei_ld_iIYN_r,	Si_stray,	ei_ld_iIYN_r, 	/* 0x74 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x78 */
  Ui_ld_A_IYh,	Ui_ld_A_IYl,	ei_ld_A_iIYN,	Si_stray, 	/* 0x7C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x80 */
  Ui_add_A_IYh,	Ui_add_A_IYl,	ei_add_A_iIYN,	Si_stray, 	/* 0x84 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x88 */
  Ui_adc_A_IYh,	Ui_adc_A_IYl,	ei_adc_A_iIYN,	Si_stray, 	/* 0x8C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x90 */
  Ui_sub_IYh,	Ui_sub_IYl,	ei_sub_iIYN,	Si_stray, 	/* 0x94 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0x98 */
  Ui_sbc_IYh,	Ui_sbc_IYl,	ei_sbc_A_iIYN,	Si_stray, 	/* 0x9C */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xA0 */
  Ui_and_IYh,	Ui_and_IYl,	ei_and_iIYN,	Si_stray, 	/* 0xA4 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xA8 */
  Ui_xor_IYh,	Ui_xor_IYl,	ei_xor_iIYN,	Si_stray, 	/* 0xAC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xB0 */
  Ui_or_IYh,	Ui_or_IYl,	ei_or_iIYN,	Si_stray, 	/* 0xB4 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xB8 */
  Ui_cp_IYh,	Ui_cp_IYl,	ei_cp_iIYN,	Si_stray, 	/* 0xBC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xC0 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xC4 */
  Si_stray,	Si_stray,	Si_stray,	0,	  	/* 0xC8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xCC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xD0 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xD4 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xD8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xDC */
  Si_stray,	ei_pop_IY,	Si_stray,	ei_ex_iSP_IY, 	/* 0xE0 */
  Si_stray,	ei_push_IY,	Si_stray,	Si_stray, 	/* 0xE4 */
  Si_stray,	ei_jp_IY,	Si_stray,	Si_stray, 	/* 0xE8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xEC */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xF0 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray, 	/* 0xF4 */
  Si_stray,	ei_ld_SP_IY,	Si_stray,	Si_stray, 	/* 0xF8 */
  Si_stray,	Si_stray,	Si_stray,	Si_stray  	/* 0xFC */

};


void (*const ei_cbop[256])(void) PROGMEM = {
  ei_rlc_r,	ei_rlc_r,	ei_rlc_r,	ei_rlc_r, 	/* 0x00 */
  ei_rlc_r,	ei_rlc_r,	ei_rlc_iHL,	ei_rlc_r, 	/* 0x04 */
  ei_rrc_r,	ei_rrc_r,	ei_rrc_r,	ei_rrc_r, 	/* 0x08 */
  ei_rrc_r,	ei_rrc_r,	ei_rrc_iHL,	ei_rrc_r, 	/* 0x0C */
  ei_rl_r,	ei_rl_r,	ei_rl_r,	ei_rl_r, 	/* 0x10 */
  ei_rl_r,	ei_rl_r,	ei_rl_iHL,	ei_rl_r, 	/* 0x14 */
  ei_rr_r,	ei_rr_r,	ei_rr_r,	ei_rr_r, 	/* 0x18 */
  ei_rr_r,	ei_rr_r,	ei_rr_iHL,	ei_rr_r, 	/* 0x1C */
  ei_sla_r,	ei_sla_r,	ei_sla_r,	ei_sla_r, 	/* 0x20 */
  ei_sla_r,	ei_sla_r,	ei_sla_iHL,	ei_sla_r, 	/* 0x24 */
  ei_sra_r,	ei_sra_r,	ei_sra_r,	ei_sra_r, 	/* 0x28 */
  ei_sra_r,	ei_sra_r,	ei_sra_iHL,	ei_sra_r, 	/* 0x2C */
  Ui_sll_r,	Ui_sll_r,	Ui_sll_r,	Ui_sll_r, 	/* 0x30 */
  Ui_sll_r,	Ui_sll_r,	Ui_sll_iHL,	Ui_sll_r, 	/* 0x34 */
  ei_srl_r,	ei_srl_r,	ei_srl_r,	ei_srl_r, 	/* 0x38 */
  ei_srl_r,	ei_srl_r,	ei_srl_iHL,	ei_srl_r, 	/* 0x3C */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x40 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x44 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x48 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x4C */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x50 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x54 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x58 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x5C */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x60 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x64 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x68 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x6C */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x70 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x74 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_r, 	/* 0x78 */
  ei_bit_b_r,	ei_bit_b_r,	ei_bit_b_iHL,	ei_bit_b_r, 	/* 0x7C */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0x80 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0x84 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0x88 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0x8C */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0x90 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0x94 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0x98 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0x9C */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0xA0 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0xA4 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0xA8 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0xAC */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0xB0 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0xB4 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_r,	ei_res_b_r, 	/* 0xB8 */
  ei_res_b_r,	ei_res_b_r,	ei_res_b_iHL,	ei_res_b_r, 	/* 0xBC */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xC0 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r, 	/* 0xC4 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xC8 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r, 	/* 0xCC */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xD0 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r, 	/* 0xD4 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xD8 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r, 	/* 0xDC */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xE0 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r, 	/* 0xE4 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xE8 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r, 	/* 0xEC */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xF0 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r, 	/* 0xF4 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_r,	ei_set_b_r, 	/* 0xF8 */
  ei_set_b_r,	ei_set_b_r,	ei_set_b_iHL,	ei_set_b_r  	/* 0xFC */

};

void (*const ei_ddcbop[256])(void) PROGMEM = {
  Ui_ld_r_rlc_iIXN,	Ui_ld_r_rlc_iIXN,	Ui_ld_r_rlc_iIXN,	Ui_ld_r_rlc_iIXN, 	/* 0x00 */
  Ui_ld_r_rlc_iIXN,	Ui_ld_r_rlc_iIXN,	ei_rlc_iIXN,		Ui_ld_r_rlc_iIXN, 	/* 0x04 */
  Ui_ld_r_rrc_iIXN,	Ui_ld_r_rrc_iIXN,	Ui_ld_r_rrc_iIXN,	Ui_ld_r_rrc_iIXN, 	/* 0x08 */
  Ui_ld_r_rrc_iIXN,	Ui_ld_r_rrc_iIXN,	ei_rrc_iIXN,		Ui_ld_r_rrc_iIXN, 	/* 0x0C */
  Ui_ld_r_rl_iIXN,	Ui_ld_r_rl_iIXN,	Ui_ld_r_rl_iIXN,	Ui_ld_r_rl_iIXN, 	/* 0x10 */
  Ui_ld_r_rl_iIXN,	Ui_ld_r_rl_iIXN,	ei_rl_iIXN,		Ui_ld_r_rl_iIXN, 	/* 0x14 */
  Ui_ld_r_rr_iIXN,	Ui_ld_r_rr_iIXN,	Ui_ld_r_rr_iIXN,	Ui_ld_r_rr_iIXN, 	/* 0x18 */
  Ui_ld_r_rr_iIXN,	Ui_ld_r_rr_iIXN,	ei_rr_iIXN,		Ui_ld_r_rr_iIXN, 	/* 0x1C */
  Ui_ld_r_sla_iIXN,	Ui_ld_r_sla_iIXN,	Ui_ld_r_sla_iIXN,	Ui_ld_r_sla_iIXN, 	/* 0x20 */
  Ui_ld_r_sla_iIXN,	Ui_ld_r_sla_iIXN,	ei_sla_iIXN,		Ui_ld_r_sla_iIXN, 	/* 0x24 */
  Ui_ld_r_sra_iIXN,	Ui_ld_r_sra_iIXN,	Ui_ld_r_sra_iIXN,	Ui_ld_r_sra_iIXN, 	/* 0x28 */
  Ui_ld_r_sra_iIXN,	Ui_ld_r_sra_iIXN,	ei_sra_iIXN,		Ui_ld_r_sra_iIXN, 	/* 0x2C */
  Ui_ld_r_sll_iIXN,	Ui_ld_r_sll_iIXN,	Ui_ld_r_sll_iIXN,	Ui_ld_r_sll_iIXN, 	/* 0x30 */
  Ui_ld_r_sll_iIXN,	Ui_ld_r_sll_iIXN,	Ui_sll_iIXN,		Ui_ld_r_sll_iIXN, 	/* 0x34 */
  Ui_ld_r_srl_iIXN,	Ui_ld_r_srl_iIXN,	Ui_ld_r_srl_iIXN,	Ui_ld_r_srl_iIXN, 	/* 0x38 */
  Ui_ld_r_srl_iIXN,	Ui_ld_r_srl_iIXN,	ei_srl_iIXN,		Ui_ld_r_srl_iIXN, 	/* 0x3C */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x40 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x44 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x48 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x4C */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x50 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x54 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x58 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x5C */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x60 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x64 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x68 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x6C */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x70 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x74 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x78 */
  Ui_bit_b_iIXN,	Ui_bit_b_iIXN,	ei_bit_b_iIXN,	Ui_bit_b_iIXN, 	/* 0x7C */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0x80 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0x84 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0x88 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0x8C */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0x90 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0x94 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0x98 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0x9C */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0xA0 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0xA4 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0xA8 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0xAC */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0xB0 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0xB4 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN, 	/* 0xB8 */
  Ui_ld_r_res_b_iIXN,	Ui_ld_r_res_b_iIXN,	ei_res_b_iIXN,		Ui_ld_r_res_b_iIXN, 	/* 0xBC */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xC0 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN, 	/* 0xC4 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xC8 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN, 	/* 0xCC */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xD0 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN, 	/* 0xD4 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xD8 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN, 	/* 0xDC */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xE0 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN, 	/* 0xE4 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xE8 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN, 	/* 0xEC */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xF0 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN, 	/* 0xF4 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN, 	/* 0xF8 */
  Ui_ld_r_set_b_iIXN,	Ui_ld_r_set_b_iIXN,	ei_set_b_iIXN,		Ui_ld_r_set_b_iIXN  	/* 0xFC */

};

void (*const ei_fdcbop[256])(void) PROGMEM = {
  Ui_ld_r_rlc_iIYN,	Ui_ld_r_rlc_iIYN,	Ui_ld_r_rlc_iIYN,	Ui_ld_r_rlc_iIYN, 	/* 0x00 */
  Ui_ld_r_rlc_iIYN,	Ui_ld_r_rlc_iIYN,	ei_rlc_iIYN,		Ui_ld_r_rlc_iIYN, 	/* 0x04 */
  Ui_ld_r_rrc_iIYN,	Ui_ld_r_rrc_iIYN,	Ui_ld_r_rrc_iIYN,	Ui_ld_r_rrc_iIYN, 	/* 0x08 */
  Ui_ld_r_rrc_iIYN,	Ui_ld_r_rrc_iIYN,	ei_rrc_iIYN,		Ui_ld_r_rrc_iIYN, 	/* 0x0C */
  Ui_ld_r_rl_iIYN,	Ui_ld_r_rl_iIYN,	Ui_ld_r_rl_iIYN,	Ui_ld_r_rl_iIYN, 	/* 0x10 */
  Ui_ld_r_rl_iIYN,	Ui_ld_r_rl_iIYN,	ei_rl_iIYN,		Ui_ld_r_rl_iIYN, 	/* 0x14 */
  Ui_ld_r_rr_iIYN,	Ui_ld_r_rr_iIYN,	Ui_ld_r_rr_iIYN,	Ui_ld_r_rr_iIYN, 	/* 0x18 */
  Ui_ld_r_rr_iIYN,	Ui_ld_r_rr_iIYN,	ei_rr_iIYN,		Ui_ld_r_rr_iIYN, 	/* 0x1C */
  Ui_ld_r_sla_iIYN,	Ui_ld_r_sla_iIYN,	Ui_ld_r_sla_iIYN,	Ui_ld_r_sla_iIYN, 	/* 0x20 */
  Ui_ld_r_sla_iIYN,	Ui_ld_r_sla_iIYN,	ei_sla_iIYN,		Ui_ld_r_sla_iIYN, 	/* 0x24 */
  Ui_ld_r_sra_iIYN,	Ui_ld_r_sra_iIYN,	Ui_ld_r_sra_iIYN,	Ui_ld_r_sra_iIYN, 	/* 0x28 */
  Ui_ld_r_sra_iIYN,	Ui_ld_r_sra_iIYN,	ei_sra_iIYN,		Ui_ld_r_sra_iIYN, 	/* 0x2C */
  Ui_ld_r_sll_iIYN,	Ui_ld_r_sll_iIYN,	Ui_ld_r_sll_iIYN,	Ui_ld_r_sll_iIYN, 	/* 0x30 */
  Ui_ld_r_sll_iIYN,	Ui_ld_r_sll_iIYN,	Ui_sll_iIYN,		Ui_ld_r_sll_iIYN, 	/* 0x34 */
  Ui_ld_r_srl_iIYN,	Ui_ld_r_srl_iIYN,	Ui_ld_r_srl_iIYN,	Ui_ld_r_srl_iIYN, 	/* 0x38 */
  Ui_ld_r_srl_iIYN,	Ui_ld_r_srl_iIYN,	ei_srl_iIYN,		Ui_ld_r_srl_iIYN, 	/* 0x3C */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x40 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x44 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x48 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x4C */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x50 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x54 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x58 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x5C */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x60 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x64 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x68 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x6C */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x70 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x74 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x78 */
  Ui_bit_b_iIYN,	Ui_bit_b_iIYN,	ei_bit_b_iIYN,	Ui_bit_b_iIYN, 	/* 0x7C */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0x80 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0x84 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0x88 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0x8C */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0x90 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0x94 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0x98 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0x9C */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0xA0 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0xA4 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0xA8 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0xAC */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0xB0 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0xB4 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN, 	/* 0xB8 */
  Ui_ld_r_res_b_iIYN,	Ui_ld_r_res_b_iIYN,	ei_res_b_iIYN,		Ui_ld_r_res_b_iIYN, 	/* 0xBC */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xC0 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN, 	/* 0xC4 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xC8 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN, 	/* 0xCC */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xD0 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN, 	/* 0xD4 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xD8 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN, 	/* 0xDC */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xE0 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN, 	/* 0xE4 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xE8 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN, 	/* 0xEC */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xF0 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN, 	/* 0xF4 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN, 	/* 0xF8 */
  Ui_ld_r_set_b_iIYN,	Ui_ld_r_set_b_iIYN,	ei_set_b_iIYN,		Ui_ld_r_set_b_iIYN  	/* 0xFC */

};
