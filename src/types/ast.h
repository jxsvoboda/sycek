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

#include <adt/list.h>

typedef struct ast_block ast_block_t;

/** AST node type */
typedef enum {
	/** Type qualifier */
	ant_tqual,
	/** Basic type specifier */
	ant_tsbasic,
	/** Identifier type specifier */
	ant_tsident,
	/** Atomic type specifier */
	ant_tsatomic,
	/** Record type specifier */
	ant_tsrecord,
	/** Enum type specifier */
	ant_tsenum,
	/** Function specifier */
	ant_fspec,
	/** Register assignment */
	ant_regassign,
	/** Attribute specifier */
	ant_aspec,
	/** Attribute specifier list */
	ant_aslist,
	/** Macro attribute */
	ant_mattr,
	/** Macro attribute list */
	ant_malist,
	/** Specifier-qualifier list */
	ant_sqlist,
	/** Type qualifier list */
	ant_tqlist,
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
	/** Init-declarator list */
	ant_idlist,
	/** Type name */
	ant_typename,
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
	/** String concatenation expression */
	ant_econcat,
	/** Binary operator expression */
	ant_ebinop,
	/** Ternary conditional expression */
	ant_etcond,
	/** Comma expression */
	ant_ecomma,
	/** Call expression */
	ant_ecall,
	/** Index expression */
	ant_eindex,
	/** Dereference expression */
	ant_ederef,
	/** Address expression */
	ant_eaddr,
	/** Sizeof expression */
	ant_esizeof,
	/** Cast expression */
	ant_ecast,
	/** Compound literal expression */
	ant_ecliteral,
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
	/** Compound initializer */
	ant_cinit,
	/** Assembler */
	ant_asm,
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
	/** Declaration statement */
	ant_stdecln,
	/** Null statement */
	ant_stnull,
	/** Loop macro invocation */
	ant_lmacro,
	/** Statement block */
	ant_block,
	/** Global declaration */
	ant_gdecln,
	/** Macro-based declaration */
	ant_mdecln,
	/** Global macro-based declaration */
	ant_gmdecln,
	/** C++ extern "C" construct */
	ant_externc,
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
	aqt_volatile,
	/** Atomic qualifier */
	aqt_atomic
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
	/** Attribute specifier list after struct/union keyword */
	struct ast_aslist *aslist1;
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
	/** Trailing attribute specifier list after '}' */
	struct ast_aslist *aslist2;
} ast_tsrecord_t;

/** Record element */
typedef struct {
	/** Containing record type specifier */
	ast_tsrecord_t *tsrecord;
	/** Link to tsrecord->elems */
	link_t ltsrecord;
	/** Specifier-qualifier list or @c NULL if using @c mdecln */
	struct ast_sqlist *sqlist;
	/** Declarator list or @c NULL if using @c mdecln */
	struct ast_dlist *dlist;
	/** Macro declaration or @c NULL if using @c sqlist and @c dlist */
	struct ast_mdecln *mdecln;
	/** Semicolon token */
	ast_tok_t tscolon;
} ast_tsrecord_elem_t;

/** Atomic type specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Atomic token */
	ast_tok_t tatomic;
	/** Left parenthesis token */
	ast_tok_t tlparen;
	/** Type name (if argument is type name) */
	struct ast_typename *atypename;
	/** Right parenthesis token */
	ast_tok_t trparen;
} ast_tsatomic_t;

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
	/** Initializer expression */
	ast_node_t *init;
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

/** Register assignment */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'asm' token */
	ast_tok_t tasm;
	/** '(' token */
	ast_tok_t tlparen;
	/** Register token */
	ast_tok_t treg;
	/** ')' token */
	ast_tok_t trparen;
} ast_regassign_t;

/** Attribute specifier list */
typedef struct ast_aslist {
	/** Base object */
	ast_node_t node;
	/** Attribute specifiers */
	list_t aspecs; /* of ast_aspec_t */
} ast_aslist_t;

/** Attribute specifier */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Containing attribute specifier list or @c NULL */
	ast_aslist_t *aslist;
	/** Link to aslist->aspecs */
	link_t laslist;
	/** '__attribute__' token */
	ast_tok_t tattr;
	/** First '(' token */
	ast_tok_t tlparen1;
	/** Second '(' token */
	ast_tok_t tlparen2;
	/** Attributes */
	list_t attrs; /* of ast_attr_t */
	/** First ')' token */
	ast_tok_t trparen1;
	/** Second ')' token */
	ast_tok_t trparen2;
} ast_aspec_t;

/** Attribute */
typedef struct {
	/** Containing attribute specifier */
	ast_aspec_t *aspec;
	/** Link to @c aspec->attrs */
	link_t lspec;
	/** Attribute name token */
	ast_tok_t tname;
	/** @c true if we have perenthesized parameter list */
	bool have_params;
	/** Left parenthesis token */
	ast_tok_t tlparen;
	/** Parameters */
	list_t params; /* of ast_aspec_param_t */
	/** Right parenthesis token */
	ast_tok_t trparen;
	/** Separating ',' token (except for the last element) */
	ast_tok_t tcomma;
} ast_aspec_attr_t;

/** Attribute parameter */
typedef struct {
	/** Containing attribute */
	ast_aspec_attr_t *attr;
	/** Link to @c attr->params */
	link_t lattr;
	/** Parameter expression */
	ast_node_t *expr;
	/** Separating ',' token (except for the last parameter) */
	ast_tok_t tcomma;
} ast_aspec_param_t;

/** Macro attribute list */
typedef struct ast_malist {
	/** Base object */
	ast_node_t node;
	/** Macro attributes */
	list_t mattrs; /* of ast_mattr_t */
} ast_malist_t;

/** Macro attribute */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Containing macro attribute list or @c NULL */
	ast_malist_t *malist;
	/** Link to malist->mattrs */
	link_t lmattrs;
	/** Macro name token */
	ast_tok_t tname;
	/** @c true if we have parentheses and parameters */
	bool have_params;
	/** '(' token (if @c have_params is true) */
	ast_tok_t tlparen;
	/** Parameters */
	list_t params; /* of ast_mattr_param_t */
	/** ')' token (if @c have_params is true) */
	ast_tok_t trparen;
} ast_mattr_t;

/** Macro attribute parameter */
typedef struct {
	/** Containing macro attribute */
	ast_mattr_t *mattr;
	/** Link to @c mattr->params */
	link_t lparams;
	/** Parameter expression */
	ast_node_t *expr;
	/** Separating ',' token (except for the last parameter) */
	ast_tok_t tcomma;
} ast_mattr_param_t;

/** Specifier-qualifier list */
typedef struct ast_sqlist {
	/** Base object */
	ast_node_t node;
	/** Specifiers and qualifiers */
	list_t elems; /* of ast_node_t */
} ast_sqlist_t;

/** Type qualifier list */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Specifiers and qualifiers */
	list_t elems; /* of ast_node_t */
} ast_tqlist_t;

/** Declaration specifiers */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Declaration specifiers */
	list_t dspecs; /* of ast_node_t */
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
	/** Type qualifier list */
	ast_tqlist_t *tqlist;
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
	/** @c true if we have an ellipsis as the last argument */
	bool have_ellipsis;
	/** Ellipsis token */
	ast_tok_t tellipsis;
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
	/** Attribute specifier list or @c NULL */
	ast_aslist_t *aslist;
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
	/** Array size expression or @c NULL if not present */
	ast_node_t *asize;
	/** Right bracket token */
	ast_tok_t trbracket;
} ast_darray_t;

/** Declarator list */
typedef struct ast_dlist {
	/** Base object */
	ast_node_t node;
	/** Declarators */
	list_t decls; /* of ast_dlist_entry_t */
} ast_dlist_t;

typedef enum {
	/** Allow abstract declarators */
	ast_abs_allow,
	/** Disallow abstract declarators */
	ast_abs_disallow
} ast_abs_allow_t;

/** Declarator list entry */
typedef struct {
	/** Containing declarator list */
	ast_dlist_t *dlist;
	/** Link to dlist->decls */
	link_t ldlist;
	/** Preceding comma token (if not the first entry */
	ast_tok_t tcomma;
	/** Declarator */
	ast_node_t *decl;
	/** @c true if we have a colon and a bit width field */
	bool have_bitwidth;
	/** ':' token if @c have_bidwidth is true */
	ast_tok_t tcolon;
	/** Bit width expression if @c have_bitwidth is true */
	ast_node_t *bitwidth;
} ast_dlist_entry_t;

/** Init-declarator list */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Init-declarators */
	list_t idecls; /* of ast_idlist_entry_t */
} ast_idlist_t;

/** Init-declarator list entry */
typedef struct {
	/** Containing init-declarator list */
	ast_idlist_t *idlist;
	/** Link to idlist->idecls */
	link_t lidlist;
	/** Preceding comma token (if not the first entry */
	ast_tok_t tcomma;
	/** Declarator */
	ast_node_t *decl;
	/** Register assignment or @c NULL */
	ast_regassign_t *regassign;
	/** Attribute specifier list or @c NULL */
	ast_aslist_t *aslist;
	/** @c true if we have an initializer */
	bool have_init;
	/** '=' token */
	ast_tok_t tassign;
	/** Initializer */
	ast_node_t *init;
} ast_idlist_entry_t;

/** Type name. */
typedef struct ast_typename {
	/** Base object */
	ast_node_t node;
	/** Declaration specifiers */
	ast_dspecs_t *dspecs;
	/** Declarator (abstract) */
	ast_node_t *decl;
} ast_typename_t;

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

/** Concatenation expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** List of elements */
	list_t elems; /* of ast_econcat_elem_t */
} ast_econcat_t;

/** Concatenation expression element */
typedef struct {
	/** Containing concatenation expression */
	ast_econcat_t *econcat;
	/** Link to @c econcat->elems */
	link_t lelems;
	/** Base expression */
	ast_node_t *bexpr;
} ast_econcat_elem_t;

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

/** Call expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Function expression */
	ast_node_t *fexpr;
	/** Left parenthesis token */
	ast_tok_t tlparen;
	/** Arguments */
	list_t args; /* of ast_ecall_arg_t */
	/** Right parenthesis token */
	ast_tok_t trparen;
} ast_ecall_t;

/** Function call argument */
typedef struct {
	/** Containing function call expression */
	ast_ecall_t *ecall;
	/** Link to @c ecall->args */
	link_t lcall;
	/** Preceding comma (if not first argument) */
	ast_tok_t tcomma;
	/** Argument (expression or type name) */
	ast_node_t *arg;
} ast_ecall_arg_t;

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

/** Sizeof expression.
 *
 * The argument can either be an expression or a type name
 */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'sizeof' token */
	ast_tok_t tsizeof;
	/** Base expression (if argument is expression) */
	ast_node_t *bexpr;
	/** '(' token (if argument is type name) */
	ast_tok_t tlparen;
	/** Type name (if argument is type name) */
	ast_typename_t *atypename;
	/** ')' token (if argument is type name) */
	ast_tok_t trparen;
} ast_esizeof_t;

/** Cast expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** '(' token */
	ast_tok_t tlparen;
	/** Declaration specifiers */
	ast_dspecs_t *dspecs;
	/** Declarator */
	ast_node_t *decl;
	/** ')' token */
	ast_tok_t trparen;
	/** Base expression */
	ast_node_t *bexpr;
} ast_ecast_t;

/** Compound literal expression */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** '(' token */
	ast_tok_t tlparen;
	/** Declaration specifiers */
	ast_dspecs_t *dspecs;
	/** Declarator */
	ast_node_t *decl;
	/** ')' token */
	ast_tok_t trparen;
	/** Compound initializer */
	struct ast_cinit *cinit;
} ast_ecliteral_t;

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

/** Compound initializer */
typedef struct ast_cinit {
	/** Base object */
	ast_node_t node;
	/** '{' token */
	ast_tok_t tlbrace;
	/** Elements */
	list_t elems; /* of ast_cinit_elem_t */
	/** '}' token */
	ast_tok_t trbrace;
} ast_cinit_t;

/** Compound initializer accesor type */
typedef enum {
	/** Index accessor ([expr]) */
	aca_index,
	/** Member accessor (.member) */
	aca_member
} ast_cinit_acc_type_t;

/** Compound initializer element */
typedef struct {
	/** Containing compound initializer */
	ast_cinit_t *cinit;
	/** Link to cinit->elems */
	link_t lcinit;
	/** Accessors */
	list_t accs; /* of ast_cinit_acc_t */
	/** '=' token (if accs is non-empty) */
	ast_tok_t tassign;
	/** Initializer value expression (or nested compound initializer) */
	ast_node_t *init;
	/** @c true if we have a comma */
	bool have_comma;
	/** Comma (optional for the last element) */
	ast_tok_t tcomma;
} ast_cinit_elem_t;

/** Compound initializer accessor */
typedef struct {
	/** Containing compound initializer element */
	ast_cinit_elem_t *elem;
	/** Link to elem->accs */
	link_t laccs;
	/** Accessor type */
	ast_cinit_acc_type_t atype;
	/** '.' token (member accessor only) */
	ast_tok_t tperiod;
	/** Member name (member accessor only) */
	ast_tok_t tmember;
	/** '[' token (index accessor only) */
	ast_tok_t tlbracket;
	/** Index expression (index accesor only) */
	ast_node_t *index;
	/** ']' token (index accessor only) */
	ast_tok_t trbracket;
} ast_cinit_acc_t;

/** Asm statement (GCC style) */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'asm' token */
	ast_tok_t tasm;
	/** @c true if we have a volatile keyword */
	bool have_volatile;
	/** 'volatile' token (if @c have_volatile is true) */
	ast_tok_t tvolatile;
	/** @c true if we have a volatile keyword */
	bool have_goto;
	/** 'volatile' token (if @c have_volatile is true) */
	ast_tok_t tgoto;
	/** '(' token */
	ast_tok_t tlparen;
	/** Assembler template */
	ast_node_t *atemplate;
	/** @c true if we have @c tcolon1 and @c out_ops */
	bool have_out_ops;
	/** First ':' token */
	ast_tok_t tcolon1;
	/** Output operands */
	list_t out_ops; /* of ast_asm_op_t */
	/** @c true if we have @c tcolon2 and @c in_ops */
	bool have_in_ops;
	/** Second ':' token */
	ast_tok_t tcolon2;
	/** Input operands */
	list_t in_ops; /* of ast_asm_op_t */
	/** @c true if we have @c tcolon3 and @c clobbers */
	bool have_clobbers;
	/** Third ':' token */
	ast_tok_t tcolon3;
	/** Input operands */
	list_t clobbers; /* of ast_asm_clobber_t */
	/** @c true if we have @c tcolon4 and @c labels */
	bool have_labels;
	/** Fourth ':' token */
	ast_tok_t tcolon4;
	/** Input operands */
	list_t labels; /* of ast_asm_label_t */
	/** ')' token */
	ast_tok_t trparen;
	/** ';' token */
	ast_tok_t tscolon;
} ast_asm_t;

/** Asm statement operand (input or output) */
typedef struct {
	/** Containing assembler statement */
	ast_asm_t *aasm;
	/** Link to aasm->in_ops or aasm->out_ops */
	link_t lasm;
	/** @c true if we have a symbolic name in brackets */
	bool have_symname;
	/** '[' token (if @c have_symname is true) */
	ast_tok_t tlbracket;
	/** Symbolic name token (if @c have_symname is true) */
	ast_tok_t tsymname;
	/** ']' token (if @c have_symname is true) */
	ast_tok_t trbracket;
	/** Constraint token */
	ast_tok_t tconstraint;
	/** '(' token */
	ast_tok_t tlparen;
	/** Expression */
	ast_node_t *expr;
	/** ')' token */
	ast_tok_t trparen;
	/** ',' token (except for the last operand) */
	ast_tok_t tcomma;
} ast_asm_op_t;

/** Asm clobber list element */
typedef struct {
	/** Containing assembler statement */
	ast_asm_t *aasm;
	/** Link to aasm->clobbers */
	link_t lasm;
	/** Clobber token */
	ast_tok_t tclobber;
	/** ',' token (except for the last element) */
	ast_tok_t tcomma;
} ast_asm_clobber_t;

/** Asm label list element */
typedef struct {
	/** Containing assembler statement */
	ast_asm_t *aasm;
	/** Link to aasm->labels */
	link_t lasm;
	/** Label token */
	ast_tok_t tlabel;
	/** ',' token (except for the last element) */
	ast_tok_t tcomma;
} ast_asm_label_t;

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
	/** Else-if parts */
	list_t elseifs; /* of ast_elseif_t */
	/** 'else' token */
	ast_tok_t telse;
	/** False branch */
	ast_block_t *fbranch;
} ast_if_t;

typedef struct {
	/** Containing if statement */
	ast_if_t *aif;
	/** Link to aif->elseifs */
	link_t lif;
	/** 'else' token */
	ast_tok_t telse;
	/** 'if' token */
	ast_tok_t tif;
	/** '(' token */
	ast_tok_t tlparen;
	/** Condition */
	ast_node_t *cond;
	/** ')' token */
	ast_tok_t trparen;
	/** Else-if branch */
	ast_block_t *ebranch;
} ast_elseif_t;

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

/** For loop statement.
 *
 * For loop initialization we can use either linit or dspecs + idlist
 * or neither if it is empty.
 */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'for' token */
	ast_tok_t tfor;
	/** '(' token */
	ast_tok_t tlparen;
	/** Loop initialization or @c NULL */
	ast_node_t *linit;
	/** Declaration specifiers or @c NULL*/
	ast_dspecs_t *dspecs;
	/** Init-declarator list or @c NULL */
	ast_idlist_t *idlist;
	/** ';' token */
	ast_tok_t tscolon1;
	/** Loop condition or @c NULL */
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

/** Declaration statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Declaration specifiers */
	ast_dspecs_t *dspecs;
	/** Init-declarator list */
	ast_idlist_t *idlist;
	/** Trailing ';' token (if @c have_scolon is @c true) */
	ast_tok_t tscolon;
} ast_stdecln_t;

/** Null statement. */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** ';' token */
	ast_tok_t tscolon;
} ast_stnull_t;

/** Loop macro invocation */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Macro invocation expression */
	ast_node_t *expr;
	/** Function body (if function definition) */
	ast_block_t *body;
} ast_lmacro_t;

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
	/** Init-declarator list */
	ast_idlist_t *idlist;
	/** Macro attribute list */
	ast_malist_t *malist;
	/** Function body (if function definition) */
	ast_block_t *body;
	/** @c true if we have a trailing semicolon */
	bool have_scolon;
	/** Trailing ';' token (if @c have_scolon is @c true) */
	ast_tok_t tscolon;
} ast_gdecln_t;

/** Macro-based declaration
 *
 * Declaration using a macro call that expands to both
 * the declaration specifier(s) and the declarator.
 * e.g. GIMMICK_INITIALIZE(foo)
 */
typedef struct ast_mdecln {
	/** Base object */
	ast_node_t node;
	/** Declaration specifiers or @c NULL if none */
	ast_dspecs_t *dspecs;
	/** Macro name token */
	ast_tok_t tname;
	/** '(' token */
	ast_tok_t tlparen;
	/** Arguments */
	list_t args; /* of ast_gmdecln_arg_t */
	/** ')' token */
	ast_tok_t trparen;
} ast_mdecln_t;

/** Macro-based declaration argument */
typedef struct {
	/** Containing macro-based declaration */
	ast_mdecln_t *mdecln;
	/** Link to @c mdecln->args */
	link_t lmdecln;
	/** Argument expression */
	ast_node_t *expr;
	/** Separation comma (except for the last argument) */
	ast_tok_t tcomma;
} ast_mdecln_arg_t;

/** Global macro-based declaration
 *
 * Global variable declaration using a macro call that expands to both
 * the declaration specifier(s) and the declarator.
 * e.g. GIMMICK_INITIALIZE(foo);
 */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** Macro-based declaration */
	ast_mdecln_t *mdecln;
	/** Function body (if function definition) */
	ast_block_t *body;
	/** @c true if we have a trailing semicolon */
	bool have_scolon;
	/** Trailing ';' token (if @c have_scolon is @c true) */
	ast_tok_t tscolon;
} ast_gmdecln_t;

/** C++ extern "C" declaration
 *
 * Even though this technically is C++, we need to parse it to support
 * dual C/C++ headers.
 */
typedef struct {
	/** Base object */
	ast_node_t node;
	/** 'extern' keyword */
	ast_tok_t textern;
	/** "C" string literal */
	ast_tok_t tlang;
	/** '{' token */
	ast_tok_t tlbrace;
	/** Declarations */
	list_t decls; /* of ast_node_t */
	/** '}' token */
	ast_tok_t trbrace;
} ast_externc_t;

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
