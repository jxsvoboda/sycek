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
extern int z80ic_proc_create(const char *, z80ic_lblock_t *, z80ic_proc_t **);
extern void z80ic_proc_destroy(z80ic_proc_t *);
extern int z80ic_proc_print(z80ic_proc_t *, FILE *);
extern int z80ic_lblock_create(z80ic_lblock_t **);
extern int z80ic_lblock_append(z80ic_lblock_t *, const char *, z80ic_instr_t *);
extern int z80ic_lblock_print(z80ic_lblock_t *, FILE *);
extern void z80ic_lblock_destroy(z80ic_lblock_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_first(z80ic_lblock_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_next(z80ic_lblock_entry_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_last(z80ic_lblock_t *);
extern z80ic_lblock_entry_t *z80ic_lblock_prev(z80ic_lblock_entry_t *);
extern int z80ic_ld_vrr_vrr_create(z80ic_ld_vrr_vrr_t **);
extern int z80ic_ld_vrr_nn_create(z80ic_ld_vrr_nn_t **);
extern int z80ic_add_vrr_vrr_create(z80ic_add_vrr_vrr_t **);
extern int z80ic_instr_print(z80ic_instr_t *, FILE *);
extern void z80ic_instr_destroy(z80ic_instr_t *);
extern int z80ic_oper_imm8_create(uint8_t, z80ic_oper_imm8_t **);
extern int z80ic_oper_imm8_print(z80ic_oper_imm8_t *, FILE *);
extern void z80ic_oper_imm8_destroy(z80ic_oper_imm8_t *);
extern int z80ic_oper_imm16_create_val(uint16_t, z80ic_oper_imm16_t **);
extern int z80ic_oper_imm16_create_symbol(const char *, z80ic_oper_imm16_t **);
extern int z80ic_oper_imm16_print(z80ic_oper_imm16_t *, FILE *);
extern void z80ic_oper_imm16_destroy(z80ic_oper_imm16_t *);
extern int z80ic_oper_reg_create(z80ic_reg_t, z80ic_oper_reg_t **);
extern int z80ic_oper_reg_print(z80ic_oper_reg_t *, FILE *);
extern void z80ic_oper_reg_destroy(z80ic_oper_reg_t *);
extern int z80ic_oper_vr_create(unsigned, z80ic_oper_vr_t **);
extern int z80ic_oper_vr_print(z80ic_oper_vr_t *, FILE *);
extern void z80ic_oper_vr_destroy(z80ic_oper_vr_t *);
extern int z80ic_oper_vrr_create(unsigned, z80ic_oper_vrr_t **);
extern int z80ic_oper_vrr_print(z80ic_oper_vrr_t *, FILE *);
extern void z80ic_oper_vrr_destroy(z80ic_oper_vrr_t *);

#endif
