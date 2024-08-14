/*
 * Copyright 2024 Jiri Svoboda
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
 * Abstract syntax tree
 */

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stdio.h>
#include <types/ast.h>

extern int ast_module_create(ast_module_t **);
extern void ast_module_append(ast_module_t *, ast_node_t *);
extern ast_node_t *ast_module_first(ast_module_t *);
extern ast_node_t *ast_module_next(ast_node_t *);
extern ast_node_t *ast_module_last(ast_module_t *);
extern ast_node_t *ast_module_prev(ast_node_t *);
extern int ast_externc_create(ast_externc_t **);
extern void ast_externc_append(ast_externc_t *, ast_node_t *);
extern ast_node_t *ast_externc_first(ast_externc_t *);
extern ast_node_t *ast_externc_next(ast_node_t *);
extern ast_node_t *ast_externc_last(ast_externc_t *);
extern ast_node_t *ast_externc_prev(ast_node_t *);
extern int ast_sclass_create(ast_sclass_type_t, ast_sclass_t **);
extern int ast_gdecln_create(ast_dspecs_t *, ast_idlist_t *, ast_malist_t *,
    ast_block_t *, ast_gdecln_t **);
extern int ast_mdecln_create(ast_mdecln_t **);
extern int ast_mdecln_append(ast_mdecln_t *, ast_node_t *, void *);
extern ast_mdecln_arg_t *ast_mdecln_first(ast_mdecln_t *);
extern ast_mdecln_arg_t *ast_mdecln_next(ast_mdecln_arg_t *);
extern int ast_gmdecln_create(ast_gmdecln_t **);
extern int ast_nulldecln_create(ast_nulldecln_t **);
extern int ast_block_create(ast_braces_t, ast_block_t **);
extern void ast_block_append(ast_block_t *, ast_node_t *);
extern ast_node_t *ast_block_first(ast_block_t *);
extern ast_node_t *ast_block_next(ast_node_t *);
extern ast_node_t *ast_block_last(ast_block_t *);
extern ast_node_t *ast_block_prev(ast_node_t *);
extern int ast_tqual_create(ast_qtype_t, ast_tqual_t **);
extern int ast_tsbasic_create(ast_btstype_t, ast_tsbasic_t **);
extern int ast_tsident_create(ast_tsident_t **);
extern int ast_tsatomic_create(ast_tsatomic_t **);
extern int ast_tsrecord_create(ast_rtype_t, ast_tsrecord_t **);
extern int ast_tsrecord_append(ast_tsrecord_t *, ast_sqlist_t *, ast_dlist_t *,
    void *);
extern int ast_tsrecord_append_mdecln(ast_tsrecord_t *, ast_mdecln_t *,
    void *);
extern int ast_tsrecord_append_null(ast_tsrecord_t *, void *);
extern ast_tsrecord_elem_t *ast_tsrecord_first(ast_tsrecord_t *);
extern ast_tsrecord_elem_t *ast_tsrecord_next(ast_tsrecord_elem_t *);
extern int ast_tsenum_create(ast_tsenum_t **);
extern int ast_tsenum_append(ast_tsenum_t *, void *, void *, ast_node_t *,
    void *);
extern ast_tsenum_elem_t *ast_tsenum_first(ast_tsenum_t *);
extern ast_tsenum_elem_t *ast_tsenum_next(ast_tsenum_elem_t *);
extern int ast_fspec_create(ast_fspec_t **);
extern int ast_alignspec_create(ast_alignspec_t **);
extern int ast_aspec_create(ast_aspec_t **);
extern void ast_aspec_append(ast_aspec_t *, ast_aspec_attr_t *);
extern ast_aspec_attr_t *ast_aspec_first(ast_aspec_t *);
extern ast_aspec_attr_t *ast_aspec_next(ast_aspec_attr_t *);
extern ast_aspec_attr_t *ast_aspec_last(ast_aspec_t *);
extern ast_aspec_attr_t *ast_aspec_prev(ast_aspec_attr_t *);
extern int ast_regassign_create(ast_regassign_t **);
extern int ast_aspec_attr_create(ast_aspec_attr_t **);
extern void ast_aspec_attr_destroy(ast_aspec_attr_t *);
extern int ast_aspec_attr_append(ast_aspec_attr_t *, ast_node_t *, void *);
extern ast_aspec_param_t *ast_aspec_attr_first(ast_aspec_attr_t *);
extern ast_aspec_param_t *ast_aspec_attr_next(ast_aspec_param_t *);
extern ast_aspec_param_t *ast_aspec_attr_last(ast_aspec_attr_t *);
extern ast_aspec_param_t *ast_aspec_attr_prev(ast_aspec_param_t *);
extern int ast_aslist_create(ast_aslist_t **);
extern void ast_aslist_append(ast_aslist_t *, ast_aspec_t *);
extern ast_aspec_t *ast_aslist_first(ast_aslist_t *);
extern ast_aspec_t *ast_aslist_next(ast_aspec_t *);
extern ast_aspec_t *ast_aslist_last(ast_aslist_t *);
extern ast_aspec_t *ast_aslist_prev(ast_aspec_t *);

extern int ast_mattr_create(ast_mattr_t **);
extern int ast_mattr_append(ast_mattr_t *, ast_node_t *, void *);
extern ast_mattr_param_t *ast_mattr_first(ast_mattr_t *);
extern ast_mattr_param_t *ast_mattr_next(ast_mattr_param_t *);
extern ast_mattr_param_t *ast_mattr_last(ast_mattr_t *);
extern ast_mattr_param_t *ast_mattr_prev(ast_mattr_param_t *);
extern int ast_malist_create(ast_malist_t **);
extern void ast_malist_append(ast_malist_t *, ast_mattr_t *);
extern ast_mattr_t *ast_malist_first(ast_malist_t *);
extern ast_mattr_t *ast_malist_next(ast_mattr_t *);
extern ast_mattr_t *ast_malist_last(ast_malist_t *);
extern ast_mattr_t *ast_malist_prev(ast_mattr_t *);

extern int ast_sqlist_create(ast_sqlist_t **);
extern void ast_sqlist_append(ast_sqlist_t *, ast_node_t *);
extern ast_node_t *ast_sqlist_first(ast_sqlist_t *);
extern ast_node_t *ast_sqlist_next(ast_node_t *);
extern ast_node_t *ast_sqlist_last(ast_sqlist_t *);
extern ast_node_t *ast_sqlist_prev(ast_node_t *);
extern bool ast_sqlist_has_tsrecord(ast_sqlist_t *);
extern int ast_tqlist_create(ast_tqlist_t **);
extern void ast_tqlist_append(ast_tqlist_t *, ast_node_t *);
extern ast_node_t *ast_tqlist_first(ast_tqlist_t *);
extern ast_node_t *ast_tqlist_next(ast_node_t *);
extern ast_node_t *ast_tqlist_last(ast_tqlist_t *);
extern ast_node_t *ast_tqlist_prev(ast_node_t *);
extern int ast_dspecs_create(ast_dspecs_t **);
extern void ast_dspecs_append(ast_dspecs_t *, ast_node_t *);
extern ast_node_t *ast_dspecs_first(ast_dspecs_t *);
extern ast_node_t *ast_dspecs_next(ast_node_t *);
extern ast_node_t *ast_dspecs_last(ast_dspecs_t *);
extern ast_node_t *ast_dspecs_prev(ast_node_t *);
extern ast_sclass_t *ast_dspecs_get_sclass(ast_dspecs_t *);
extern int ast_dident_create(ast_dident_t **);
extern int ast_dnoident_create(ast_dnoident_t **);
extern int ast_dparen_create(ast_dparen_t **);
extern int ast_dptr_create(ast_dptr_t **);
extern int ast_dfun_create(ast_dfun_t **);
extern int ast_dfun_append(ast_dfun_t *, ast_dspecs_t *, ast_node_t *,
    ast_aslist_t *, void *);
extern ast_dfun_arg_t *ast_dfun_first(ast_dfun_t *);
extern ast_dfun_arg_t *ast_dfun_next(ast_dfun_arg_t *);
extern int ast_darray_create(ast_darray_t **);
extern int ast_dlist_create(ast_dlist_t **);
extern int ast_dlist_append(ast_dlist_t *, void *, ast_node_t *, bool, void *,
    ast_node_t *, ast_aslist_t *);
extern ast_dlist_entry_t *ast_dlist_first(ast_dlist_t *);
extern ast_dlist_entry_t *ast_dlist_next(ast_dlist_entry_t *);
extern ast_dlist_entry_t *ast_dlist_last(ast_dlist_t *);
extern ast_dlist_entry_t *ast_dlist_prev(ast_dlist_entry_t *);
extern int ast_idlist_create(ast_idlist_t **);
extern int ast_idlist_append(ast_idlist_t *, void *, ast_node_t *,
    ast_regassign_t *, ast_aslist_t *, bool, void *, ast_node_t *);
extern int ast_typename_create(ast_typename_t **);
extern ast_idlist_entry_t *ast_idlist_first(ast_idlist_t *);
extern ast_idlist_entry_t *ast_idlist_next(ast_idlist_entry_t *);
extern ast_idlist_entry_t *ast_idlist_last(ast_idlist_t *);
extern ast_idlist_entry_t *ast_idlist_prev(ast_idlist_entry_t *);
extern bool ast_decl_is_abstract(ast_node_t *);
extern ast_tok_t *ast_decl_get_ident(ast_node_t *);
extern bool ast_decl_is_ptrdecln(ast_node_t *);
extern bool ast_decl_is_fundecln(ast_node_t *);
extern bool ast_decl_is_vardecln(ast_node_t *);
extern ast_dfun_t *ast_decl_get_dfun(ast_node_t *);
extern ast_tok_t *ast_gdecln_get_ident(ast_gdecln_t *);
extern int ast_eint_create(ast_eint_t **);
extern int ast_echar_create(ast_echar_t **);
extern int ast_estring_create(ast_estring_t **);
extern int ast_estring_append(ast_estring_t *, void *);
extern ast_estring_lit_t *ast_estring_first(ast_estring_t *);
extern ast_estring_lit_t *ast_estring_next(ast_estring_lit_t *);
extern ast_estring_lit_t *ast_estring_last(ast_estring_t *);
extern int ast_eident_create(ast_eident_t **);
extern int ast_eparen_create(ast_eparen_t **);
extern int ast_econcat_create(ast_econcat_t **);
extern int ast_econcat_append(ast_econcat_t *, ast_node_t *);
extern ast_econcat_elem_t *ast_econcat_first(ast_econcat_t *);
extern ast_econcat_elem_t *ast_econcat_next(ast_econcat_elem_t *);
extern ast_econcat_elem_t *ast_econcat_last(ast_econcat_t *);
extern int ast_ebinop_create(ast_ebinop_t **);
extern int ast_etcond_create(ast_etcond_t **);
extern int ast_ecomma_create(ast_ecomma_t **);
extern int ast_ecall_create(ast_ecall_t **);
extern int ast_ecall_append(ast_ecall_t *, void *, ast_node_t *);
extern ast_ecall_arg_t *ast_ecall_first(ast_ecall_t *);
extern ast_ecall_arg_t *ast_ecall_next(ast_ecall_arg_t *);
extern int ast_eindex_create(ast_eindex_t **);
extern int ast_ederef_create(ast_ederef_t **);
extern int ast_eaddr_create(ast_eaddr_t **);
extern int ast_esizeof_create(ast_esizeof_t **);
extern int ast_ecast_create(ast_ecast_t **);
extern int ast_ecliteral_create(ast_ecliteral_t **);
extern int ast_emember_create(ast_emember_t **);
extern int ast_eindmember_create(ast_eindmember_t **);
extern int ast_eusign_create(ast_eusign_t **);
extern int ast_elnot_create(ast_elnot_t **);
extern int ast_ebnot_create(ast_ebnot_t **);
extern int ast_epreadj_create(ast_epreadj_t **);
extern int ast_epostadj_create(ast_epostadj_t **);
extern int ast_eva_arg_create(ast_eva_arg_t **);
extern int ast_cinit_create(ast_cinit_t **);
extern void ast_cinit_append(ast_cinit_t *, ast_cinit_elem_t *);
extern ast_cinit_elem_t *ast_cinit_first(ast_cinit_t *);
extern ast_cinit_elem_t *ast_cinit_next(ast_cinit_elem_t *);
extern int ast_cinit_elem_create(ast_cinit_elem_t **);
extern void ast_cinit_elem_destroy(ast_cinit_elem_t *);
extern int ast_cinit_elem_append_index(ast_cinit_elem_t *, void *,
    ast_node_t *, void *);
extern int ast_cinit_elem_append_member(ast_cinit_elem_t *, void *, void *);
extern ast_cinit_acc_t *ast_cinit_elem_first(ast_cinit_elem_t *);
extern ast_cinit_acc_t *ast_cinit_elem_next(ast_cinit_acc_t *);
extern int ast_asm_create(ast_asm_t **);
extern int ast_asm_append_out_op(ast_asm_t *, bool, void *, void *, void *,
    void *, void *, ast_node_t *, void *, void *);
extern int ast_asm_append_in_op(ast_asm_t *, bool, void *, void *, void *,
    void *, void *, ast_node_t *, void *, void *);
extern int ast_asm_append_clobber(ast_asm_t *, void *, void *);
extern int ast_asm_append_label(ast_asm_t *, void *, void *);
extern ast_asm_op_t *ast_asm_first_out_op(ast_asm_t *);
extern ast_asm_op_t *ast_asm_next_out_op(ast_asm_op_t *);
extern ast_asm_op_t *ast_asm_first_in_op(ast_asm_t *);
extern ast_asm_op_t *ast_asm_next_in_op(ast_asm_op_t *);
extern ast_asm_clobber_t *ast_asm_first_clobber(ast_asm_t *);
extern ast_asm_clobber_t *ast_asm_next_clobber(ast_asm_clobber_t *);
extern ast_asm_label_t *ast_asm_first_label(ast_asm_t *);
extern ast_asm_label_t *ast_asm_next_label(ast_asm_label_t *);
extern int ast_break_create(ast_break_t **);
extern int ast_continue_create(ast_continue_t **);
extern int ast_goto_create(ast_goto_t **);
extern int ast_return_create(ast_return_t **);
extern int ast_if_create(ast_if_t **);
extern int ast_if_append(ast_if_t *, void *, void *, void *, ast_node_t *,
    void *, ast_block_t *);
extern ast_elseif_t *ast_if_first(ast_if_t *);
extern ast_elseif_t *ast_if_next(ast_elseif_t *);
extern int ast_while_create(ast_while_t **);
extern int ast_do_create(ast_do_t **);
extern int ast_for_create(ast_for_t **);
extern int ast_switch_create(ast_switch_t **);
extern int ast_clabel_create(ast_clabel_t **);
extern int ast_dlabel_create(ast_dlabel_t **);
extern int ast_glabel_create(ast_glabel_t **);
extern int ast_stexpr_create(ast_stexpr_t **);
extern int ast_stdecln_create(ast_stdecln_t **);
extern int ast_stnull_create(ast_stnull_t **);
extern int ast_va_end_create(ast_va_end_t **);
extern int ast_va_copy_create(ast_va_copy_t **);
extern int ast_va_start_create(ast_va_start_t **);
extern int ast_lmacro_create(ast_lmacro_t **);
extern int ast_tree_print(ast_node_t *, FILE *);
extern void ast_tree_destroy(ast_node_t *);
extern ast_tok_t *ast_tree_first_tok(ast_node_t *);
extern ast_tok_t *ast_tree_last_tok(ast_node_t *);

#endif
