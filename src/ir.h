/*
 * Copyright 2021 Jiri Svoboda
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
 * Intermediate Representation
 */

#ifndef IR_H
#define IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <types/ir.h>

extern int ir_module_create(ir_module_t **);
extern void ir_module_append(ir_module_t *, ir_decln_t *);
extern ir_decln_t *ir_module_first(ir_module_t *);
extern ir_decln_t *ir_module_next(ir_decln_t *);
extern ir_decln_t *ir_module_last(ir_module_t *);
extern ir_decln_t *ir_module_prev(ir_decln_t *);
extern int ir_module_find(ir_module_t *, const char *, ir_decln_t **);
extern int ir_module_print(ir_module_t *, FILE *);
extern void ir_module_destroy(ir_module_t *);
extern int ir_decln_print(ir_decln_t *, FILE *);
extern const char *ir_decln_ident(ir_decln_t *);
extern int ir_var_create(const char *, ir_dblock_t *, ir_var_t **);
extern void ir_var_destroy(ir_var_t *);
extern int ir_var_print(ir_var_t *, FILE *);
extern int ir_dblock_create(ir_dblock_t **);
extern int ir_dblock_append(ir_dblock_t *, ir_dentry_t *);
extern int ir_dblock_print(ir_dblock_t *, FILE *);
extern void ir_dblock_destroy(ir_dblock_t *);
extern ir_dblock_entry_t *ir_dblock_first(ir_dblock_t *);
extern ir_dblock_entry_t *ir_dblock_next(ir_dblock_entry_t *);
extern ir_dblock_entry_t *ir_dblock_last(ir_dblock_t *);
extern ir_dblock_entry_t *ir_dblock_prev(ir_dblock_entry_t *);
extern int ir_dentry_create_int(unsigned, int64_t, ir_dentry_t **);
extern int ir_dentry_create_uint(unsigned, uint64_t, ir_dentry_t **);
extern int ir_dentry_print(ir_dentry_t *, FILE *);
extern void ir_dentry_destroy(ir_dentry_t *);
extern int ir_proc_create(const char *, ir_proc_flags_t, ir_lblock_t *,
    ir_proc_t **);
extern void ir_proc_append_arg(ir_proc_t *, ir_proc_arg_t *);
extern void ir_proc_append_lvar(ir_proc_t *, ir_lvar_t *);
extern void ir_proc_destroy(ir_proc_t *);
extern int ir_proc_print(ir_proc_t *, FILE *);
extern ir_proc_arg_t *ir_proc_first_arg(ir_proc_t *);
extern ir_proc_arg_t *ir_proc_next_arg(ir_proc_arg_t *);
extern ir_proc_arg_t *ir_proc_last_arg(ir_proc_t *);
extern ir_proc_arg_t *ir_proc_prev_arg(ir_proc_arg_t *);
extern int ir_proc_arg_create(const char *, ir_texpr_t *, ir_proc_arg_t **);
extern void ir_proc_arg_destroy(ir_proc_arg_t *);
extern int ir_proc_arg_print(ir_proc_arg_t *, FILE *);
extern ir_lvar_t *ir_proc_first_lvar(ir_proc_t *);
extern ir_lvar_t *ir_proc_next_lvar(ir_lvar_t *);
extern ir_lvar_t *ir_proc_last_lvar(ir_proc_t *);
extern ir_lvar_t *ir_proc_prev_lvar(ir_lvar_t *);
extern int ir_lvar_create(const char *, ir_texpr_t *, ir_lvar_t **);
extern void ir_lvar_destroy(ir_lvar_t *);
extern int ir_lvar_print(ir_lvar_t *, FILE *);
extern int ir_lblock_create(ir_lblock_t **);
extern int ir_lblock_append(ir_lblock_t *, const char *, ir_instr_t *);
extern int ir_lblock_print(ir_lblock_t *, FILE *);
extern void ir_lblock_move_entries(ir_lblock_t *, ir_lblock_t *);
extern void ir_lblock_destroy(ir_lblock_t *);
extern ir_lblock_entry_t *ir_lblock_first(ir_lblock_t *);
extern ir_lblock_entry_t *ir_lblock_next(ir_lblock_entry_t *);
extern ir_lblock_entry_t *ir_lblock_last(ir_lblock_t *);
extern ir_lblock_entry_t *ir_lblock_prev(ir_lblock_entry_t *);
extern int ir_instr_create(ir_instr_t **);
extern int ir_instr_print(ir_instr_t *, FILE *);
extern void ir_instr_destroy(ir_instr_t *);
extern int ir_oper_imm_create(int64_t, ir_oper_imm_t **);
extern int ir_oper_list_create(ir_oper_list_t **);
extern int ir_oper_var_create(const char *, ir_oper_var_t **);
extern int ir_oper_print(ir_oper_t *, FILE *);
extern void ir_oper_destroy(ir_oper_t *);
extern void ir_oper_list_append(ir_oper_list_t *, ir_oper_t *);
extern ir_oper_t *ir_oper_list_first(ir_oper_list_t *);
extern ir_oper_t *ir_oper_list_next(ir_oper_t *);
extern ir_oper_t *ir_oper_list_last(ir_oper_list_t *);
extern ir_oper_t *ir_oper_list_prev(ir_oper_t *);
extern int ir_texpr_int_create(unsigned, ir_texpr_t **);
extern int ir_texpr_print(ir_texpr_t *, FILE *);
extern size_t ir_texpr_sizeof(ir_texpr_t *);
extern void ir_texpr_destroy(ir_texpr_t *);

#endif
