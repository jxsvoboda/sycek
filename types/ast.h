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

typedef struct ast_block ast_block_t;

/** AST node type */
typedef enum {
	/** Type qualifier */
	ant_tqual,
	/** Basic type specifier */
	ant_tsbasic,
	/** Identifier type specifier */
	ant_tsident,
	/** Record type specifier */
	ant_tsrecord,
	/** Enum type specifier */
	ant_tsenum,
	/** Function specifier */
	ant_fspec,
	/** Specifier-qualifier list */
	ant_sqlist,
	/** Declaration specifiers */
	ant_dspecs,
	/** Ideintifier declarator */
	ant_dident,
	/** No-identifier declarator */
	ant_dnoident,
	/** Parenthesized declarator */
	ant_dparen,
	/** Pointer declarator */
	ant_dptr,
	/** Function declarator */
	ant_dfun,
	/** Array declarator */
	ant_darray,
	/** Declarator list */
	ant_dlist,
	/** Integer literal */
	ant_eint,
	/** Character literal */
	ant_echar,
	/** String literal */
	ant_estring,
	/** Identifier expression */
	ant_eident,
	/** Parenthesized expression */
	ant_eparen,
	/** Binary operator expression */
	ant_ebinop,
	/** Ternary conditional expression */
	ant_etcond,
	/** Comma expression */
	ant_ecomma,
	/** Function call expression */
	ant_efuncall,
	/** Index expression */
	ant_eindex,
	/** Dereference expression */
	ant_ederef,
	/** Address expression */
	ant_eaddr,
	/** Sizeof expression */
	ant_esizeof,
	/** Member expression */
	ant_emember,
	/** Indirect member expression */
	ant_eindmember,
	/** Unary sign expression */
	ant_eusign,
	/** Logical not expression */
	ant_elnot,
	/** Bitwise not expression */
	ant_ebnot,
	/** Pre-increment/-decrement expression */
	ant_epreadj,
	/** Post-increment/-decrement expression */
	ant_epostadj,
	/** Break statement */
	ant_break,
	/** Continue statement */
	ant_continue,
	/** Goto statement */
	ant_goto,
	/** Return statement */
	ant_return,
	/** If statement */
	ant_if,
	/** While loop statement */
	ant_while,
	/** Do loop statement */
	ant_do,
	/** For loop statement */
	ant_for,
	/** Switch statement */
	ant_switch,
	/** Case label */
	ant_clabel,
	/** Goto label */
	ant_glabel,
	/** Expression statement */
	ant_stexpr,
	/* Null statement */
	ant_stnull,
	/** Statement block */
	ant_block,
	/** Global declaration */
	ant_gdecln,
	/** Module */
	ant_module,
	/** Storage-class specifier */
	ant_sclass,
} ast_node_type_t;

/** Presence or absence of braces around a block */
typedef enum {
	ast_nobraces,
	ast_braces
} ast_braces_t;

/** Storage class type */
typedef enum {
	/** Typedef storage class */
	asc_typedef,
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

/** Binary operator type */
typedef enum {
	/* *'+' addition */
	abo_plus,
	/** '-' subtraction */
	abo_minus,
	/** '*' multiplication */
	abo_times,
	/** '/' division */
	abo_divide,
	/** '%' remainder */
	abo_modulo,
	/** '<<' shift left */
	abo_shl,
	/** '>>' shift right */
	abo_shr,
	/** '<' less than */
	abo_lt,
	/** '<=' less-than or equal */
	abo_lteq,
	/** '>' greater-than */
	abo_gt,
	/** '>=' greater-than or equal */
	abo_gteq,
	/** '==' equal */
	abo_eq,
	/** '!=' not equal */
	abo_neq,
	/** '&' bitwise and */
	abo_band,
	/** '^' bitwise xor */
	abo_bxor,
	/** '|' bitwise or */
	abo_bor,
	/** '&&' logical and */
	abo_land,
	/** '||' logical or */
	abo_lor,
	/** '=' assignment */
	abo_assign,
	/** '+=' assignment by sum */
	abo_plus_assign,
	/** '-=' assignment by difference */
	abo_minus_assign,
	/** '*=' assignment by product */
	abo_times_assign,
	/** '/=' assignment by quotient */
	abo_divide_assign,
	/** '%=' assignment by remainder */
	abo_modulo_assign,
	/** '<<=' assignment by left shift */
	abo_shl_assign,
	/** '>>=' assignment by right shift */
	abo_shr_assign,
	/** '&=' assignment by bitwise and */
	abo_band_assign,
	/** '|=' assignment by bitwise or */
	abo_bxor_assign,
	/** '^=' assignment by bitwise xor */
	abo_bor_assign,
	/** Comma */
	abo_comma
} ast_binop_t;

/** Unary sign operator type */
typedef enum {
	/** Plus sign */
	aus_plus,
	/** Minus sign */
	aus_minus
} ast_usign_t;

/** Increment or decrement */
typedef enum {
	/** Pre/post increment */
	aat_inc,
	/** Pre/post decrement */
	aat_dec
} ast_adj_t;

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

/** Qualifier type */
typedef enum {
	/** Const qualifier */
	aqt_const,
	/** Restrict qualifier */
	aqt_restrict,
	/** Volatile qualifier */
	aqt_volatile
} ast_qtype_t;

/** Type qualifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Qualifier type */
	ast_qtype_t qtype;
	/** Qualifier token */
	ast_tok_t tqual;
} ast_tqual_t;

/** Basic type specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Basic type specifier token */
	ast_tok_t tbasic;
} ast_tsbasic_t;

/** Identifier type specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Ideintifier token */
	ast_tok_t tident;
} ast_tsident_t;

/** Type of record */
typedef enum {
	/** Struct */
	ar_struct,
	/** Union */
	ar_union
} ast_rtype_t;

/** Record (struct or union) type specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Record type */
	ast_rtype_t rtype;
	/** Struct or union token */
	ast_tok_t tsu;
	/** @c true if we have an identifier */
	bool have_ident;
	/** Record type identifier */
	ast_tok_t tident;
	/** @c true if we have the definition (braces and elements) */
	bool have_def;
	/** Left brace token */
	ast_tok_t tlbrace;
	/** Elements */
	list_t elems; /* of ast_tsrecord_elem_t */
	/** Right brace token */
	ast_tok_t trbrace;
} ast_tsrecord_t;

/** Record element */
typedef struct {
	/** Containing record type specifier */
	ast_tsrecord_t *tsrecord;
	/** Link to tsrecord->elems */
	link_t ltsrecord;
	/** Specifier-qualifier list */
	struct ast_sqlist *sqlist;
	/** Declarator list */
	struct ast_dlist *dlist;
	/** Semicolon token */
	ast_tok_t tscolon;
} ast_tsrecord_elem_t;

/** Enum type specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Enum token */
	ast_tok_t tenum;
	/** @c true if we have an identifier */
	bool have_ident;
	/** Enum identifier token */
	ast_tok_t tident;
	/** @c true if we have the definition (braces and elements) */
	bool have_def;
	/** Left brace token */
	ast_tok_t tlbrace;
	/** Elements */
	list_t elems; /* of ast_tsenum_elem_t */
	/** Right brace token */
	ast_tok_t trbrace;
} ast_tsenum_t;

/** Enum element */
typedef struct {
	/** Containing enum type specifier */
	ast_tsenum_t *tsenum;
	/** Link to tsenum->elems */
	link_t ltsenum;
	/** Identifier token */
	ast_tok_t tident;
	/** Equals token */
	ast_tok_t tequals;
	/** Initializer token */
	ast_tok_t tinit;
	/** Comma token */
	ast_tok_t tcomma;
} ast_tsenum_elem_t;

/** Function specifier (i. e. inline) */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Function specifier token */
	ast_tok_t tfspec;
} ast_fspec_t;

/** Specifier-qualifier list */
typedef struct ast_sqlist {
	/** Base object */
	ast_node_t node;
	/** Specifiers and qualifiers */
	list_t elems;
} ast_sqlist_t;

/** Declaration specifiers */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Declaration specifiers */
	list_t dspecs;
} ast_dspecs_t;

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

/** Function declarator */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Base declarator */
	ast_node_t *bdecl;
	/** Left parenthesis token */
	ast_tok_t tlparen;
	/** Arguments */
	list_t args; /* of ast_dfun_arg_t */
	/** Right parenthesis token */
	ast_tok_t trparen;
} ast_dfun_t;

/** Function declarator argument */
typedef struct {
	/** Containing function declarator */
	ast_dfun_t *dfun;
	/** Link to dfun->args */
	link_t ldfun;
	/** Declaration specifiers */
	ast_dspecs_t *dspecs;
	/** Declarator */
	ast_node_t *decl;
	/** Comma token (except for last argument) */
	ast_tok_t tcomma;
} ast_dfun_arg_t;

/** Array declarator */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Base declarator */
	ast_node_t *bdecl;
	/** Left bracket token */
	ast_tok_t tlbracket;
	/** Size token */
	ast_tok_t tsize;
	/** Right bracket token */
	ast_tok_t trbracket;
} ast_darray_t;

/** Declarator list */
typedef struct ast_dlist {
	/** Base object */
	ast_node_t node;
	/** Declarators */
	list_t decls; /* of ast_decllist_entry_t */
} ast_dlist_t;

typedef enum {
	/** Allow abstract declarators */
	ast_abs_allow,
	/** Disallow abstract declarators */
	ast_abs_disallow
} ast_abs_allow_t;

/** Declarator list entry */
typedef struct {
	/** Containing type definition */
	ast_dlist_t *dlist;
	/** Link to decllist->decls */
	link_t ldlist;
	/** Preceding comma token (if not the first entry */
	ast_tok_t tcomma;
	/** Declarator */
	ast_node_t *decl;
} ast_dlist_entry_t;

/** Pointer type */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Asterisk token */
	ast_tok_t tasterisk;
	/** Base type */
	ast_node_t *btype;
} ast_tptr_t;

/** Integer literal expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Literal token */
	ast_tok_t tlit;
} ast_eint_t;

/** Character literal expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Literal token */
	ast_tok_t tlit;
} ast_echar_t;

/** String literal expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** List of literals */
	list_t lits;
} ast_estring_t;

/** String literal expression element */
typedef struct {
	/** Containing string literal expression */
	ast_estring_t *estring;
	/** Link to @c estring->lits */
	link_t lstring;
	/** Literal token */
	ast_tok_t tlit;
} ast_estring_lit_t;

/** Identifier expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Ideintifier token */
	ast_tok_t tident;
} ast_eident_t;

/** Parenthesized expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** '(' token */
	ast_tok_t tlparen;
	/** Base expression */
	ast_node_t *bexpr;
	/** ')' token */
	ast_tok_t trparen;
} ast_eparen_t;

/** Binary operator expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Binary operator type */
	ast_binop_t optype;
	/** Left argument */
	ast_node_t *larg;
	/** Operator token */
	ast_tok_t top;
	/** Right argument */
	ast_node_t *rarg;
} ast_ebinop_t;

/** Ternary conditional expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Condition */
	ast_node_t *cond;
	/** '?' token */
	ast_tok_t tqmark;
	/** True argument */
	ast_node_t *targ;
	/** ':' token */
	ast_tok_t tcolon;
	/** False argument */
	ast_node_t *farg;
} ast_etcond_t;

/** Comma expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Left argument */
	ast_node_t *larg;
	/** Comma token */
	ast_tok_t tcomma;
	/** Right argument */
	ast_node_t *rarg;
} ast_ecomma_t;

/** Function call expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Function expression */
	ast_node_t *fexpr;
	/** Left parenthesis token */
	ast_tok_t tlparen;
	/*( Arguments */
	list_t args; /* of ast_efuncall_arg_t */
	/** Right parenthesis token */
	ast_tok_t trparen;
} ast_efuncall_t;

/** Function call argument */
typedef struct {
	/** Containing function call expression */
	ast_efuncall_t *efuncall;
	/** Link to @c efuncall->args */
	link_t lfuncall;
	/** Preceding comma (if not first argument) */
	ast_tok_t tcomma;
	/** Argument expression */
	ast_node_t *expr;
} ast_efuncall_arg_t;

/** Index expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Base expression */
	ast_node_t *bexpr;
	/** Left bracket token */
	ast_tok_t tlbracket;
	/* Index expression */
	ast_node_t *iexpr;
	/** Right bracket token */
	ast_tok_t trbracket;
} ast_eindex_t;

/** Dereference expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** '*' token */
	ast_tok_t tasterisk;
	/** Base expression */
	ast_node_t *bexpr;
} ast_ederef_t;

/** Address expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** '&' token */
	ast_tok_t tamper;
	/** Base expression */
	ast_node_t *bexpr;
} ast_eaddr_t;

/** Sizeof expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'sizeof' token */
	ast_tok_t tsizeof;
	/** '(' token */
	ast_tok_t tlparen;
	/** Base expression */
	ast_node_t *bexpr;
	/** ')' token */
	ast_tok_t trparen;
} ast_esizeof_t;

/** Member expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Base expression */
	ast_node_t *bexpr;
	/** '.' token */
	ast_tok_t tperiod;
	/** Member name token */
	ast_tok_t tmember;
} ast_emember_t;

/** Indirect member expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Base expression */
	ast_node_t *bexpr;
	/** '->' token */
	ast_tok_t tarrow;
	/** Member name token */
	ast_tok_t tmember;
} ast_eindmember_t;

/** Unary sign expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Unary sign type (+ or -) */
	ast_usign_t usign;
	/** Sign token */
	ast_tok_t tsign;
	/** Base expression */
	ast_node_t *bexpr;
} ast_eusign_t;

/** Logical not expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Logical not token */
	ast_tok_t tlnot;
	/** Base expression */
	ast_node_t *bexpr;
} ast_elnot_t;

/** Bitwise not expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Bitwise not token */
	ast_tok_t tbnot;
	/** Base expression */
	ast_node_t *bexpr;
} ast_ebnot_t;

/** Pre-adjustment (increment/decrement) */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Adjustment type (++ or --) */
	ast_adj_t adj;
	/** Adjustment token */
	ast_tok_t tadj;
	/** Base expression */
	ast_node_t *bexpr;
} ast_epreadj_t;

/** Post-adjustment (increment/decrement) */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Base expression */
	ast_node_t *bexpr;
	/** Adjustment type (++ or --) */
	ast_adj_t adj;
	/** Adjustment token */
	ast_tok_t tadj;
} ast_epostadj_t;

/** Break statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'break' token */
	ast_tok_t tbreak;
	/** ';' token */
	ast_tok_t tscolon;
} ast_break_t;

/** Continue statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'continue' token */
	ast_tok_t tcontinue;
	/** ';' token */
	ast_tok_t tscolon;
} ast_continue_t;

/** Goto statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'goto' token */
	ast_tok_t tgoto;
	/** Target */
	ast_tok_t ttarget;
	/** ';' token */
	ast_tok_t tscolon;
} ast_goto_t;

/** Return statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'return' token */
	ast_tok_t treturn;
	/** Argument */
	ast_node_t *arg;
	/** ';' token */
	ast_tok_t tscolon;
} ast_return_t;

/** If statement */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'if' token */
	ast_tok_t tif;
	/** '(' token */
	ast_tok_t tlparen;
	/** Condition */
	ast_node_t *cond;
	/** ')' token */
	ast_tok_t trparen;
	/** True branch */
	ast_block_t *tbranch;
	/** 'else' token */
	ast_tok_t telse;
	/** False branch */
	ast_block_t *fbranch;
} ast_if_t;

/** While loop statement */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'while' token */
	ast_tok_t twhile;
	/** '(' token */
	ast_tok_t tlparen;
	/** Condition */
	ast_node_t *cond;
	/** ')' token */
	ast_tok_t trparen;
	/** Loop body */
	ast_block_t *body;
} ast_while_t;

/** Do loop statement */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'do' token */
	ast_tok_t tdo;
	/** Loop body */
	ast_block_t *body;
	/** 'while' token */
	ast_tok_t twhile;
	/** '(' token */
	ast_tok_t tlparen;
	/** Condition */
	ast_node_t *cond;
	/** ')' token */
	ast_tok_t trparen;
	/** ';' token */
	ast_tok_t tscolon;
} ast_do_t;

/** For loop statement */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'for' token */
	ast_tok_t tfor;
	/** '(' token */
	ast_tok_t tlparen;
	/** Loop initialization */
	ast_node_t *linit;
	/** ';' token */
	ast_tok_t tscolon1;
	/** Loop condition */
	ast_node_t *lcond;
	/** ';' token */
	ast_tok_t tscolon2;
	/** Next iteration */
	ast_node_t *lnext;
	/** ')' token */
	ast_tok_t trparen;
	/** Loop body */
	ast_block_t *body;
} ast_for_t;

/** Switch statement */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'switch' token */
	ast_tok_t tswitch;
	/** '(' token */
	ast_tok_t tlparen;
	/** Switch expression */
	ast_node_t *sexpr;
	/** ')' token */
	ast_tok_t trparen;
	/** Switch body */
	ast_block_t *body;
} ast_switch_t;

/** Case label */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'case' token */
	ast_tok_t tcase;
	/** Case expression */
	ast_node_t *cexpr;
	/** Colon token */
	ast_tok_t tcolon;
} ast_clabel_t;

/** Goto label */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Label token */
	ast_tok_t tlabel;
	/** Colon token */
	ast_tok_t tcolon;
} ast_glabel_t;

/** Expression statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Expression */
	ast_node_t *expr;
	/** ';' token */
	ast_tok_t tscolon;
} ast_stexpr_t;

/** Null statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** ';' token */
	ast_tok_t tscolon;
} ast_stnull_t;

/** Statement block. */
struct ast_block {
	/** Base object */
	ast_node_t node;
	/** Block having braces or not */
	ast_braces_t braces;
	/** Opening brace token */
	ast_tok_t topen;
	list_t stmts; /* of ast_node_t */
	/** Closing brace token */
	ast_tok_t tclose;
};

/** Storage-class specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Storage class type */
	ast_sclass_type_t sctype;
	/** Storage class token */
	ast_tok_t tsclass;
} ast_sclass_t;

/** Global declaration */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Declaration specifiers */
	ast_dspecs_t *dspecs;
	/** Declarator list */
	ast_dlist_t *dlist;
	/** Function body (if function definition) */
	ast_block_t *body;
	/** @c true if we have a trailing semicolon */
	bool have_scolon;
	/** Trailing ';' token (if @c have_scolon is @c true) */
	ast_tok_t tscolon;
} ast_gdecln_t;

/** Module.
 *
 * decls must be ast_gdecln_t
 */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Declarations */
	list_t decls; /* of ast_node_t */
} ast_module_t;

#endif
