/*
 * Abstract syntax tree
 */

#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <types/ast.h>

extern int ast_module_create(ast_module_t **);
extern void ast_module_append(ast_module_t *, ast_node_t *);
extern int ast_fundef_create(ast_type_t *, ast_ident_t *, ast_fundef_t **);
extern int ast_tree_print(ast_node_t *, FILE *);
extern void ast_tree_destroy(ast_node_t *);

#endif
