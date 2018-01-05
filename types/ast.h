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

#ifndef TYPES_AST_H
#define TYPES_AST_H

#include <types/adt/list.h>

/** AST node type */
typedef enum {
	ant_tsbuiltin,
	ant_tsident,
	ant_dident,
	ant_dnoident,
	ant_dparen,
	ant_dptr,
	ant_ident,
	ant_expr,
	ant_return,
	ant_block,
	ant_fundef,
	ant_typedef,
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

/** Built-in type specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Signedness token */
	ast_tok_t tsign;
	/** Length modifier token */
	ast_tok_t tlm;
	/** Second Length modifier token */
	ast_tok_t tlm2;
	/** Base type token */
	ast_tok_t tbase;
} ast_tsbuiltin_t;

/** Identifier type specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Ideintifier token */
	ast_tok_t tident;
} ast_tsident_t;

/** Declarator - identifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Identifier token */
	ast_tok_t tident;
} ast_dident_t;

/** Declarator - no identifier */
typedef struct {
	/** Base object */
	ast_node_t node;
} ast_dnoident_t;

/** Parenthesized declarator */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Left parenthesis token */
	ast_tok_t tlparen;
	/** Base declarator */
	ast_node_t *bdecl;
	/** Right parenthesis token */
	ast_tok_t trparen;
} ast_dparen_t;

/** Pointer declarator */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Asterisk token */
	ast_tok_t tasterisk;
	/** Base declarator */
	ast_node_t *bdecl;
} ast_dptr_t;

/** Pointer type */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Asterisk token */
	ast_tok_t tasterisk;
	/** Base type */
	ast_node_t *btype;
} ast_tptr_t;

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
	/** Function type specifier */
	ast_node_t *ftspec;
	/** Function declarator */
	ast_node_t *fdecl;
	/** Function body */
	ast_block_t *body;
	/** Trailing ';' token (if function declaration) */
	ast_tok_t tscolon;
} ast_fundef_t;

/** Type definition */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Type specifier */
	ast_node_t *tspec;
	/** Declarators */
	list_t decls; /* of ast_typedef_decl_t */
	/** Trailing ';' token */
	ast_tok_t tscolon;
} ast_typedef_t;

/** Typedef declarator entry */
typedef struct {
	/** Containing type definition */
	ast_typedef_t *atypedef;
	/** Link to atypedef->decls */
	link_t ltypedef;
	/** Preceding comma token (if not the first entry */
	ast_tok_t tcomma;
	/** Declarator */
	ast_node_t *decl;
} ast_typedef_decl_t;

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
