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

#include <stdio.h>
#include <types/ast.h>

extern int ast_module_create(ast_module_t **);
extern void ast_module_append(ast_module_t *, ast_node_t *);
extern ast_node_t *ast_module_first(ast_module_t *);
extern ast_node_t *ast_module_next(ast_node_t *);
extern int ast_sclass_create(ast_sclass_type_t, ast_sclass_t **);
extern int ast_fundef_create(ast_node_t *, ast_node_t *, ast_block_t *,
    ast_fundef_t **);
extern int ast_typedef_create(ast_node_t *, ast_typedef_t **);
extern int ast_typedef_append(ast_typedef_t *, void *, ast_node_t *);
extern ast_typedef_decl_t *ast_typedef_first(ast_typedef_t *);
extern ast_typedef_decl_t *ast_typedef_next(ast_typedef_decl_t *);
extern int ast_block_create(ast_braces_t, ast_block_t **);
extern void ast_block_append(ast_block_t *, ast_node_t *);
extern ast_node_t *ast_block_first(ast_block_t *);
extern ast_node_t *ast_block_next(ast_node_t *);
extern int ast_tsbuiltin_create(ast_tsbuiltin_t **);
extern int ast_tsident_create(ast_tsident_t **);
extern int ast_tsrecord_create(ast_rtype_t, ast_tsrecord_t **);
extern int ast_tsrecord_append(ast_tsrecord_t *, ast_node_t *, ast_node_t *,
    void *);
extern ast_tsrecord_elem_t *ast_tsrecord_first(ast_tsrecord_t *);
extern ast_tsrecord_elem_t *ast_tsrecord_next(ast_tsrecord_elem_t *);
extern int ast_dident_create(ast_dident_t **);
extern int ast_dnoident_create(ast_dnoident_t **);
extern int ast_dparen_create(ast_dparen_t **);
extern int ast_dptr_create(ast_dptr_t **);
extern int ast_return_create(ast_return_t **);
extern int ast_tree_print(ast_node_t *, FILE *);
extern void ast_tree_destroy(ast_node_t *);

#endif
