/*
 * Abstract syntax tree
 */

#ifndef TYPES_AST_H
#define TYPES_AST_H

#include <types/adt/list.h>

/** AST node type */
typedef enum {
	ant_type,
	ant_ident,
	ant_expr,
	ant_return,
	ant_block,
	ant_fundef,
	ant_module
} ast_node_type_t;

/** Presence or absence of braces around a block */
typedef enum {
	ast_nobraces,
	ast_braces
} ast_braces_t;

/** AST node */
typedef struct ast_node {
	/** Pointer to entire/specific node structure */
	void *ext;
	/** Node in which we are enlisted */
	struct ast_node *lnode;
	/** Link to list we are in */
	link_t llist;
	ast_node_type_t ntype;
} ast_node_t;

/** Type expression */
typedef struct {
	ast_node_t node;
} ast_type_t;

/** Identifier */
typedef struct {
	ast_node_t node;
} ast_ident_t;

/** Arithmetic expression */
typedef struct {
} ast_expr_t;

/** Return statement. */
typedef struct {
	ast_node_t node;
	ast_expr_t arg;
} ast_return_t;

/** Statement block. */
typedef struct {
	ast_node_t node;
	ast_braces_t braces;
	list_t stmts; /* of ast_node_t */
} ast_block_t;

/** Function definition */
typedef struct {
	ast_node_t node;
	/** Function type expression, including the function identifier */
	ast_type_t *ftype;
	ast_block_t *body;
} ast_fundef_t;

/** Module.
 *
 * decls must be one of ast_var_t, ast_typedef_t, ast_fundecl_t, ast_fundef_t
 */
typedef struct {
	ast_node_t node;
	list_t decls; /* of ast_node_t */
} ast_module_t;

#endif
