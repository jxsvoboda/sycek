/*
 * Copyright 2018 Jiri Svoboda
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
extern int ast_sclass_create(ast_sclass_type_t, ast_sclass_t **);
extern int ast_gdecln_create(ast_dspecs_t *, ast_dlist_t *, ast_block_t *,
    ast_gdecln_t **);
extern int ast_block_create(ast_braces_t, ast_block_t **);
extern void ast_block_append(ast_block_t *, ast_node_t *);
extern ast_node_t *ast_block_first(ast_block_t *);
extern ast_node_t *ast_block_next(ast_node_t *);
extern ast_node_t *ast_block_last(ast_block_t *);
extern ast_node_t *ast_block_prev(ast_node_t *);
extern int ast_tqual_create(ast_qtype_t, ast_tqual_t **);
extern int ast_tsbasic_create(ast_tsbasic_t **);
extern int ast_tsident_create(ast_tsident_t **);
extern int ast_tsrecord_create(ast_rtype_t, ast_tsrecord_t **);
extern int ast_tsrecord_append(ast_tsrecord_t *, ast_sqlist_t *, ast_dlist_t *,
    void *);
extern ast_tsrecord_elem_t *ast_tsrecord_first(ast_tsrecord_t *);
extern ast_tsrecord_elem_t *ast_tsrecord_next(ast_tsrecord_elem_t *);
extern int ast_tsenum_create(ast_tsenum_t **);
extern int ast_tsenum_append(ast_tsenum_t *, void *, void *, void *, void *);
extern ast_tsenum_elem_t *ast_tsenum_first(ast_tsenum_t *);
extern ast_tsenum_elem_t *ast_tsenum_next(ast_tsenum_elem_t *);
extern int ast_fspec_create(ast_fspec_t **);
extern int ast_sqlist_create(ast_sqlist_t **);
extern void ast_sqlist_append(ast_sqlist_t *, ast_node_t *);
extern ast_node_t *ast_sqlist_first(ast_sqlist_t *);
extern ast_node_t *ast_sqlist_next(ast_node_t *);
extern ast_node_t *ast_sqlist_last(ast_sqlist_t *);
extern ast_node_t *ast_sqlist_prev(ast_node_t *);
extern int ast_dspecs_create(ast_dspecs_t **);
extern void ast_dspecs_append(ast_dspecs_t *, ast_node_t *);
extern ast_node_t *ast_dspecs_first(ast_dspecs_t *);
extern ast_node_t *ast_dspecs_next(ast_node_t *);
extern ast_node_t *ast_dspecs_last(ast_dspecs_t *);
extern ast_node_t *ast_dspecs_prev(ast_node_t *);
extern int ast_dident_create(ast_dident_t **);
extern int ast_dnoident_create(ast_dnoident_t **);
extern int ast_dparen_create(ast_dparen_t **);
extern int ast_dptr_create(ast_dptr_t **);
extern int ast_dfun_create(ast_dfun_t **);
extern int ast_dfun_append(ast_dfun_t *, ast_dspecs_t *, ast_node_t *, void *);
extern ast_dfun_arg_t *ast_dfun_first(ast_dfun_t *);
extern ast_dfun_arg_t *ast_dfun_next(ast_dfun_arg_t *);
extern int ast_darray_create(ast_darray_t **);
extern int ast_dlist_create(ast_dlist_t **);
extern int ast_dlist_append(ast_dlist_t *, void *, ast_node_t *);
extern ast_dlist_entry_t *ast_dlist_first(ast_dlist_t *);
extern ast_dlist_entry_t *ast_dlist_next(ast_dlist_entry_t *);
extern ast_dlist_entry_t *ast_dlist_last(ast_dlist_t *);
extern ast_dlist_entry_t *ast_dlist_prev(ast_dlist_entry_t *);
extern bool ast_decl_is_abstract(ast_node_t *);
extern int ast_eint_create(ast_eint_t **);
extern int ast_echar_create(ast_echar_t **);
extern int ast_estring_create(ast_estring_t **);
extern int ast_eident_create(ast_eident_t **);
extern int ast_eparen_create(ast_eparen_t **);
extern int ast_ebinop_create(ast_ebinop_t **);
extern int ast_etcond_create(ast_etcond_t **);
extern int ast_ecomma_create(ast_ecomma_t **);
extern int ast_efuncall_create(ast_efuncall_t **);
extern int ast_eindex_create(ast_eindex_t **);
extern int ast_ederef_create(ast_ederef_t **);
extern int ast_eaddr_create(ast_eaddr_t **);
extern int ast_esizeof_create(ast_esizeof_t **);
extern int ast_emember_create(ast_emember_t **);
extern int ast_eindmember_create(ast_eindmember_t **);
extern int ast_eusign_create(ast_eusign_t **);
extern int ast_elnot_create(ast_elnot_t **);
extern int ast_ebnot_create(ast_ebnot_t **);
extern int ast_epreadj_create(ast_epreadj_t **);
extern int ast_epostadj_create(ast_epostadj_t **);
extern int ast_break_create(ast_break_t **);
extern int ast_continue_create(ast_continue_t **);
extern int ast_goto_create(ast_goto_t **);
extern int ast_return_create(ast_return_t **);
extern int ast_if_create(ast_if_t **);
extern int ast_while_create(ast_while_t **);
extern int ast_do_create(ast_do_t **);
extern int ast_for_create(ast_for_t **);
extern int ast_switch_create(ast_switch_t **);
extern int ast_clabel_create(ast_clabel_t **);
extern int ast_glabel_create(ast_glabel_t **);
extern int ast_tree_print(ast_node_t *, FILE *);
extern void ast_tree_destroy(ast_node_t *);
extern ast_tok_t *ast_tree_first_tok(ast_node_t *);
extern ast_tok_t *ast_tree_last_tok(ast_node_t *);

#endif
