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
extern int ast_fundef_create(ast_type_t *, ast_block_t *, ast_fundef_t **);
extern int ast_block_create(ast_braces_t, ast_block_t **);
extern void ast_block_append(ast_block_t *, ast_node_t *);
extern ast_node_t *ast_block_first(ast_block_t *);
extern ast_node_t *ast_block_next(ast_node_t *);
extern int ast_type_create(ast_type_t **);
extern int ast_return_create(ast_return_t **);
extern int ast_tree_print(ast_node_t *, FILE *);
extern void ast_tree_destroy(ast_node_t *);

#endif
