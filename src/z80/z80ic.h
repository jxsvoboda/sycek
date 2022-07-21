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

#ifndef Z80IC_H
#define Z80IC_H

#include <stdbool.h>
#include <stdio.h>
#include <types/z80/z80ic.h>

extern int z80ic_module_create(z80ic_module_t **);
extern void z80ic_module_append(z80ic_module_t *, z80ic_decln_t *);
extern z80ic_decln_t *z80ic_module_first(z80ic_module_t *);
extern z80ic_decln_t *z80ic_module_next(z80ic_decln_t *);
extern z80ic_decln_t *z80ic_module_last(z80ic_module_t *);
extern z80ic_decln_t *z80ic_module_prev(z80ic_decln_t *);
extern int z80ic_module_print(z80ic_module_t *, FILE *);
extern void z80ic_module_destroy(z80ic_module_t *);
extern int z80ic_decln_print(z80ic_decln_t *, FILE *);
extern int z80ic_extern_create(const char *, z80ic_extern_t **);
extern void z80ic_extern_destroy(z80ic_extern_t *);
extern int z80ic_extern_print(z80ic_extern_t *, FILE *);
extern int z80ic_var_create(const char *, z80ic_dblock_t *, z80ic_var_t **);
extern void z80ic_var_destroy(z80ic_var_t *);
extern int z80ic_var_print(z80ic_var_t *, FILE *);
extern int z80ic_dblock_create(z80ic_dblock_t **);
extern int z80ic_dblock_append(z80ic_dblock_t *, z80ic_dentry_t *);
extern int z80ic_dblock_print(z80ic_dblock_t *, FILE *);
extern void z80ic_dblock_destroy(z80ic_dblock_t *);
extern z80ic_dblock_entry_t *z80ic_dblock_first(z80ic_dblock_t *);
extern z80ic_dblock_entry_t *z80ic_dblock_next(z80ic_dblock_entry_t *);
extern z80ic_dblock_entry_t *z80ic_dblock_last(z80ic_dblock_t *);
extern z80ic_dblock_entry_t *z80ic_dblock_prev(z80ic_dblock_entry_t *);
extern int z80ic_dentry_create_defb(uint8_t, z80ic_dentry_t **);
extern int z80ic_dentry_create_defw(uint16_t, z80ic_dentry_t **);
extern int z80ic_dentry_create_defdw(uint32_t, z80ic_dentry_t **);
extern int z80ic_dentry_create_defqw(uint64_t, z80ic_dentry_t **);
extern int z80ic_dentry_print(z80ic_dentry_t *, FILE *);
extern void z80ic_dentry_destroy(z80ic_dentry_t *);
extern int z80ic_proc_create(const char *, z80ic_lblock_t *, z80ic_proc_t **);
extern void z80ic_proc_append_lvar(z80ic_proc_t *, z80ic_lvar_t *);
extern void z80ic_proc_destroy(z80ic_proc_t *);
extern int z80ic_proc_print(z80ic_proc_t *, FILE *);
extern z80ic_lvar_t *z80ic_proc_first_lvar(z80ic_proc_t *);
extern z80ic_lvar_t *z80ic_proc_next_lvar(z80ic_lvar_t *);
extern z80ic_lvar_t *z80ic_proc_last_lvar(z80ic_proc_t *);
extern z80ic_lvar_t *z80ic_proc_prev_lvar(z80ic_lvar_t *);
extern int z80ic_lvar_create(const char *, uint16_t, z80ic_lvar_t **);
extern void z80ic_lvar_destroy(z80ic_lvar_t *);
extern int z80ic_lvar_print(z80ic_lvar_t *, FILE *);
extern int z80ic_lblock_create(z80ic_lblock_t **);
extern int z80ic_lblock_append(z80ic_lblock_t *, const char *, z80ic_instr_t *);
extern int z80ic_lblock_print(z80ic_lblock_t *, FILE *);
extern void z80ic_lblock_destroy(z80ic_lblock_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_first(z80ic_lblock_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_next(z80ic_lblock_entry_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_last(z80ic_lblock_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_prev(z80ic_lblock_entry_t *);
extern int z80ic_ld_r_n_create(z80ic_ld_r_n_t **);
extern int z80ic_ld_r_ihl_create(z80ic_ld_r_ihl_t **);
extern int z80ic_ld_r_iixd_create(z80ic_ld_r_iixd_t **);
extern int z80ic_ld_ihl_r_create(z80ic_ld_ihl_r_t **);
extern int z80ic_ld_iixd_r_create(z80ic_ld_iixd_r_t **);
extern int z80ic_ld_iixd_n_create(z80ic_ld_iixd_n_t **);
extern int z80ic_ld_dd_nn_create(z80ic_ld_dd_nn_t **);
extern int z80ic_ld_ix_nn_create(z80ic_ld_ix_nn_t **);
extern int z80ic_ld_sp_ix_create(z80ic_ld_sp_ix_t **);
extern int z80ic_push_ix_create(z80ic_push_ix_t **);
extern int z80ic_pop_ix_create(z80ic_pop_ix_t **);
extern int z80ic_add_a_iixd_create(z80ic_add_a_iixd_t **);
extern int z80ic_adc_a_iixd_create(z80ic_adc_a_iixd_t **);
extern int z80ic_sub_n_create(z80ic_sub_n_t **);
extern int z80ic_sub_iixd_create(z80ic_sub_iixd_t **);
extern int z80ic_sbc_a_iixd_create(z80ic_sbc_a_iixd_t **);
extern int z80ic_and_r_create(z80ic_and_r_t **);
extern int z80ic_and_iixd_create(z80ic_and_iixd_t **);
extern int z80ic_or_iixd_create(z80ic_or_iixd_t **);
extern int z80ic_xor_iixd_create(z80ic_xor_iixd_t **);
extern int z80ic_inc_iixd_create(z80ic_inc_iixd_t **);
extern int z80ic_dec_iixd_create(z80ic_dec_iixd_t **);
extern int z80ic_cpl_create(z80ic_cpl_t **);
extern int z80ic_nop_create(z80ic_nop_t **);
extern int z80ic_add_hl_ss_create(z80ic_add_hl_ss_t **);
extern int z80ic_sbc_hl_ss_create(z80ic_sbc_hl_ss_t **);
extern int z80ic_add_ix_pp_create(z80ic_add_ix_pp_t **);
extern int z80ic_inc_ss_create(z80ic_inc_ss_t **);
extern int z80ic_rla_create(z80ic_rla_t **);
extern int z80ic_rl_iixd_create(z80ic_rl_iixd_t **);
extern int z80ic_rr_iixd_create(z80ic_rr_iixd_t **);
extern int z80ic_sla_iixd_create(z80ic_sla_iixd_t **);
extern int z80ic_sra_iixd_create(z80ic_sra_iixd_t **);
extern int z80ic_bit_b_iixd_create(z80ic_bit_b_iixd_t **);
extern int z80ic_jp_nn_create(z80ic_jp_nn_t **);
extern int z80ic_jp_cc_nn_create(z80ic_jp_cc_nn_t **);
extern int z80ic_call_nn_create(z80ic_call_nn_t **);
extern int z80ic_ret_create(z80ic_ret_t **);
extern int z80ic_ld_vr_vr_create(z80ic_ld_vr_vr_t **);
extern int z80ic_ld_vr_n_create(z80ic_ld_vr_n_t **);
extern int z80ic_ld_vr_ihl_create(z80ic_ld_vr_ihl_t **);
extern int z80ic_ld_ihl_vr_create(z80ic_ld_ihl_vr_t **);
extern int z80ic_ld_vrr_vrr_create(z80ic_ld_vrr_vrr_t **);
extern int z80ic_ld_r_vr_create(z80ic_ld_r_vr_t **);
extern int z80ic_ld_vr_r_create(z80ic_ld_vr_r_t **);
extern int z80ic_ld_r16_vrr_create(z80ic_ld_r16_vrr_t **);
extern int z80ic_ld_vrr_r16_create(z80ic_ld_vrr_r16_t **);
extern int z80ic_ld_vrr_nn_create(z80ic_ld_vrr_nn_t **);
extern int z80ic_ld_vrr_spnn_create(z80ic_ld_vrr_spnn_t **);
extern int z80ic_add_a_vr_create(z80ic_add_a_vr_t **);
extern int z80ic_adc_a_vr_create(z80ic_adc_a_vr_t **);
extern int z80ic_sub_vr_create(z80ic_sub_vr_t **);
extern int z80ic_sbc_a_vr_create(z80ic_sbc_a_vr_t **);
extern int z80ic_and_vr_create(z80ic_and_vr_t **);
extern int z80ic_or_vr_create(z80ic_or_vr_t **);
extern int z80ic_xor_vr_create(z80ic_xor_vr_t **);
extern int z80ic_inc_vr_create(z80ic_inc_vr_t **);
extern int z80ic_dec_vr_create(z80ic_dec_vr_t **);
extern int z80ic_add_vrr_vrr_create(z80ic_add_vrr_vrr_t **);
extern int z80ic_sub_vrr_vrr_create(z80ic_sub_vrr_vrr_t **);
extern int z80ic_inc_vrr_create(z80ic_inc_vrr_t **);
extern int z80ic_rl_vr_create(z80ic_rl_vr_t **);
extern int z80ic_rr_vr_create(z80ic_rr_vr_t **);
extern int z80ic_sla_vr_create(z80ic_sla_vr_t **);
extern int z80ic_sra_vr_create(z80ic_sra_vr_t **);
extern int z80ic_bit_b_vr_create(z80ic_bit_b_vr_t **);
extern int z80ic_instr_print(z80ic_instr_t *, FILE *);
extern void z80ic_instr_destroy(z80ic_instr_t *);
extern int z80ic_oper_imm8_create(uint8_t, z80ic_oper_imm8_t **);
extern int z80ic_oper_imm8_print(z80ic_oper_imm8_t *, FILE *);
extern void z80ic_oper_imm8_destroy(z80ic_oper_imm8_t *);
extern int z80ic_oper_imm16_create_val(uint16_t, z80ic_oper_imm16_t **);
extern int z80ic_oper_imm16_create_symbol(const char *, z80ic_oper_imm16_t **);
extern int z80ic_oper_imm16_copy(z80ic_oper_imm16_t *, z80ic_oper_imm16_t **);
extern int z80ic_oper_imm16_print(z80ic_oper_imm16_t *, FILE *);
extern void z80ic_oper_imm16_destroy(z80ic_oper_imm16_t *);
extern int z80ic_oper_reg_create(z80ic_reg_t, z80ic_oper_reg_t **);
extern int z80ic_oper_reg_print(z80ic_oper_reg_t *, FILE *);
extern void z80ic_oper_reg_destroy(z80ic_oper_reg_t *);
extern int z80ic_oper_dd_create(z80ic_dd_t, z80ic_oper_dd_t **);
extern int z80ic_oper_dd_print(z80ic_oper_dd_t *, FILE *);
extern void z80ic_oper_dd_destroy(z80ic_oper_dd_t *);
extern int z80ic_oper_pp_create(z80ic_pp_t, z80ic_oper_pp_t **);
extern int z80ic_oper_pp_print(z80ic_oper_pp_t *, FILE *);
extern void z80ic_oper_pp_destroy(z80ic_oper_pp_t *);
extern int z80ic_oper_ss_create(z80ic_ss_t, z80ic_oper_ss_t **);
extern int z80ic_oper_ss_print(z80ic_oper_ss_t *, FILE *);
extern void z80ic_oper_ss_destroy(z80ic_oper_ss_t *);
extern int z80ic_oper_r16_create(z80ic_r16_t, z80ic_oper_r16_t **);
extern int z80ic_oper_r16_print(z80ic_oper_r16_t *, FILE *);
extern void z80ic_oper_r16_destroy(z80ic_oper_r16_t *);
extern int z80ic_oper_vr_create(unsigned, z80ic_vr_part_t, z80ic_oper_vr_t **);
extern int z80ic_oper_vr_print(z80ic_oper_vr_t *, FILE *);
extern void z80ic_oper_vr_destroy(z80ic_oper_vr_t *);
extern int z80ic_oper_vrr_create(unsigned, z80ic_oper_vrr_t **);
extern int z80ic_oper_vrr_print(z80ic_oper_vrr_t *, FILE *);
extern void z80ic_oper_vrr_destroy(z80ic_oper_vrr_t *);
extern z80ic_reg_t z80ic_r16_lo(z80ic_r16_t);
extern z80ic_reg_t z80ic_r16_hi(z80ic_r16_t);

#endif
