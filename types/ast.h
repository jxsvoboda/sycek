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
	ant_module,
	ant_sclass
} ast_node_type_t;

/** Presence or absence of braces around a block */
typedef enum {
	ast_nobraces,
	ast_braces
} ast_braces_t;

/** Storage class type */
typedef enum {
	/** Extern storage class */
	asc_extern,
	/** Static storage class */
	asc_static,
	/** Automatic storage class */
	asc_auto,
	/** Register storage class */
	asc_register,
	/** No storage class specified */
	asc_none
} ast_sclass_type_t;

/** AST token data.
 *
 * Used to allow the user to store information related to each token
 * the AST corresponds to.
 */
typedef struct {
	/** User data related to token */
	void *data;
} ast_tok_t;

/** AST node */
typedef struct ast_node {
	/** Pointer to entire/specific node structure */
	void *ext;
	/** Node in which we are enlisted */
	struct ast_node *lnode;
	/** Link to list we are in */
	link_t llist;
	/** Node type */
	ast_node_type_t ntype;
} ast_node_t;

/** Type expression */
typedef struct {
	/** Base object */
	ast_node_t node;
} ast_type_t;

/** Identifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Ideintifier token */
	ast_tok_t tident;
} ast_ident_t;

/** Arithmetic expression */
typedef struct {
} ast_expr_t;

/** Return statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'return' token */
	ast_tok_t treturn;
	/** Argument */
	ast_expr_t arg;
	/** ';' token */
	ast_tok_t tscolon;
} ast_return_t;

/** Statement block. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Block having braces or not */
	ast_braces_t braces;
	/** Opening brace token */
	ast_tok_t topen;
	list_t stmts; /* of ast_node_t */
	/** Closing brace token */
	ast_tok_t tclose;
} ast_block_t;

/** Storage-class specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Storage class type */
	ast_sclass_type_t sctype;
	/** Storage class token */
	ast_tok_t tsclass;
} ast_sclass_t;

/** Function definition */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Storage class specifier */
	ast_sclass_t *sclass;
	/** Function type expression, including the function identifier */
	ast_type_t *ftype;
	/** Function body */
	ast_block_t *body;
	/** Trailing ';' token (if function declaration) */
	ast_tok_t tscolon;
} ast_fundef_t;

/** Module.
 *
 * decls must be one of ast_var_t, ast_typedef_t, ast_fundecl_t, ast_fundef_t
 */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Declarations */
	list_t decls; /* of ast_node_t */
} ast_module_t;

#endif
