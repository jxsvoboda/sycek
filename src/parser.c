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
 * Parser
 */

#include <assert.h>
#include <ast.h>
#include <parser.h>
#include <lexer.h>
#include <merrno.h>
#include <stdio.h>
#include <stdlib.h>

static int parser_process_sclass(parser_t *, ast_sclass_t **);
static int parser_process_fspec(parser_t *, ast_fspec_t **);
static int parser_process_tspec(parser_t *, ast_node_t **);
static int parser_process_regassign(parser_t *, ast_regassign_t **);
static int parser_process_aspec(parser_t *, ast_aspec_t **);
static int parser_process_aslist(parser_t *, ast_aslist_t **);
static int parser_process_dspecs(parser_t *, unsigned, bool *, ast_dspecs_t **);
static int parser_process_decl(parser_t *, ast_node_t **);
static int parser_process_dlist(parser_t *, ast_abs_allow_t, ast_dlist_t **);
static int parser_process_mdecln(parser_t *, ast_mdecln_t **);
static int parser_process_idlist(parser_t *, ast_abs_allow_t, ast_idlist_t **);
static int parser_process_typename(parser_t *, ast_typename_t **);
static int parser_process_sqlist(parser_t *, ast_sqlist_t **);
static int parser_process_tqlist(parser_t *, ast_tqlist_t **);
static int parser_process_eprefix(parser_t *, ast_node_t **);
static int parser_process_epostfix(parser_t *, ast_node_t **);
static int parser_process_eassign(parser_t *, ast_node_t **);
static int parser_process_econcat(parser_t *, ast_node_t **);
static int parser_process_expr(parser_t *, ast_node_t **);
static int parser_process_cinit(parser_t *, ast_cinit_t **);
static int parser_process_init(parser_t *, ast_node_t **);
static int parser_process_block(parser_t *, ast_block_t **);

/** Create parser.
 *
 * @param ops Parser input ops
 * @param arg Argument to input ops
 * @param tok Starting token
 * @param rparser Place to store pointer to new parser
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int parser_create(parser_input_ops_t *ops, void *arg, void *tok,
    parser_t **rparser)
{
	parser_t *parser;
	void *ntok;
	lexer_tok_t ltok;

	parser = calloc(1, sizeof(parser_t));
	if (parser == NULL)
		return ENOMEM;

	parser->input_ops = ops;
	parser->input_arg = arg;

	ntok = tok;
	parser->input_ops->read_tok(parser->input_arg, ntok, &ltok);
	while (parser_ttype_ignore(ltok.ttype)) {
		ntok = parser->input_ops->next_tok(parser->input_arg, ntok);
		parser->input_ops->read_tok(parser->input_arg, ntok, &ltok);
	}

	parser->tok = ntok;

	*rparser = parser;
	return EOK;
}

/** Create a silent clone parser.
 *
 * Create a parser starting at the same point as @ a parent, but
 * with error messages disabled. This is used in cases where we must
 * try multiple parsing options.
 *
 * @param parent Parser to clone
 * @param rparser Place to store pointer to new parser
 * @return EOK on success, or error code
 */
static int parser_create_silent_sub(parser_t *parent, parser_t **rparser)
{
	int rc;

	rc = parser_create(parent->input_ops, parent->input_arg,
	    parent->tok, rparser);
	if (rc != EOK)
		return rc;

	(*rparser)->silent = true;
	return EOK;
}

/** Destroy parser.
 *
 * @param parser Parser
 */
void parser_destroy(parser_t *parser)
{
	free(parser);
}

/** Return @c true if token type is to be ignored when parsing.
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is ignored during parsing
 */
bool parser_ttype_ignore(lexer_toktype_t ttype)
{
	return ttype == ltt_space || ttype == ltt_tab ||
	    ttype == ltt_newline || ttype == ltt_comment ||
	    ttype == ltt_dscomment || ttype == ltt_preproc;
}

/** Return @c true if token type is an assignment operator.
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is an assignment operator
 */
static bool parser_ttype_assignop(lexer_toktype_t ttype)
{
	return ttype == ltt_assign || ttype == ltt_plus_assign ||
	    ttype == ltt_minus_assign || ttype == ltt_times_assign ||
	    ttype == ltt_divide_assign || ttype == ltt_modulo_assign ||
	    ttype == ltt_shl_assign || ttype == ltt_shr_assign ||
	    ttype == ltt_band_assign || ttype == ltt_bor_assign ||
	    ttype == ltt_bxor_assign;
}

/** Return next input token skipping tokens that should be ignored.
 *
 * At the same time we read the token contents into the provided buffer @a rtok
 *
 * @param parser Parser
 * @param itok Current input token pointer
 * @param ritok Place to store next input token pointer
 * @param rtok Place to store next lexer token
 */
static void parser_next_input_tok(parser_t *parser, void *itok,
    void **ritok, lexer_tok_t *rtok)
{
	void *ntok = itok;

	do {
		ntok = parser->input_ops->next_tok(parser->input_arg, ntok);
		parser->input_ops->read_tok(parser->input_arg, ntok, rtok);
	} while (parser_ttype_ignore(rtok->ttype));

	*ritok = ntok;
}

/** Return type of next token.
 *
 * @param parser Parser
 * @return Type of next token being parsed
 */
static lexer_toktype_t parser_next_ttype(parser_t *parser)
{
	lexer_tok_t tok;

	parser->input_ops->read_tok(parser->input_arg, parser->tok, &tok);
	return tok.ttype;
}

/** Return type of next next token.
 *
 * @param parser Parser
 * @return Type of next next token being parsed
 */
static lexer_toktype_t parser_next_next_ttype(parser_t *parser)
{
	lexer_tok_t tok;
	void *ntok;

	parser_next_input_tok(parser, parser->tok, &ntok, &tok);
	return tok.ttype;
}

/** Read next token.
 *
 * This may be only used for printing out tokens for debugging purposes.
 * All decisions are based only on token type (see parser_next_ttype).
 *
 * @param parser Parser
 * @param 
 * @return Pointer to the next token at the read head
 */
static void parser_read_next_tok(parser_t *parser, lexer_tok_t *tok)
{
	parser->input_ops->read_tok(parser->input_arg, parser->tok, tok);
}

static int parser_dprint_next_tok(parser_t *parser, FILE *f)
{
	lexer_tok_t tok;

	parser_read_next_tok(parser, &tok);
	return lexer_dprint_tok(&tok, f);
}

/** Get user data that should be stored into the AST for a token.
 *
 * @param parser Parser
 * @param tok Token
 * @return User data for token @a tok
 */
static void *parser_get_tok_data(parser_t *parser, void *tok)
{
	(void) parser;
	return parser->input_ops->tok_data(parser->input_arg, tok);
}

/** Skip over current token.
 *
 * @param parser Parser
 * @param rdata Place to store user data for token or @c NULL if not interested
 */
static void parser_skip(parser_t *parser, void **rdata)
{
	void *ntok;

	if (rdata != NULL)
		*rdata = parser_get_tok_data(parser, parser->tok);

	do {
		ntok = parser->input_ops->next_tok(parser->input_arg,
		    parser->tok);
		parser->tok = ntok;
	} while (parser_ttype_ignore(parser_next_ttype(parser)));
}

/** Match a particular token type.
 *
 * If the type of the next token is @a mtype, skip over it. Otherwise
 * generate an error.
 *
 * @param parser Parser
 * @param mtype Expected token type
 * @param rdata Place to store user data for token or @c NULL if not interested
 *
 * @return EOK on sucecss, EINVAL if token does not have expected type
 */
static int parser_match(parser_t *parser, lexer_toktype_t mtype, void **rdata)
{
	lexer_toktype_t ltype;

	ltype = parser_next_ttype(parser);
	if (ltype != mtype) {
		if (!parser->silent) {
			fprintf(stderr, "Error: ");
			parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " unexpected, expected '%s'.\n",
			    lexer_str_ttype(mtype));
		}
		return EINVAL;
	}

	parser_skip(parser, rdata);
	return EOK;
}

/** Return @c true if token type a type qualifier.
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is a type qualifier
 */
static bool parser_ttype_tqual(lexer_toktype_t ttype)
{
	return ttype == ltt_const || ttype == ltt_restrict ||
	    ttype == ltt_restrict_alt || ttype == ltt_volatile;
}

/** Return @c true if token type is a basic type specifier
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is a basic type specifier
 */
static bool parser_ttype_tsbasic(lexer_toktype_t ttype)
{
	return ttype == ltt_void || ttype == ltt_char || ttype == ltt_short ||
	    ttype == ltt_int || ttype == ltt_long || ttype == ltt_int128 ||
	    ttype == ltt_float || ttype == ltt_double || ttype == ltt_signed ||
	    ttype == ltt_unsigned;
}

/** Return @c true if token type is a type specifier
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is a type specifier
 */
static bool parser_ttype_tspec(lexer_toktype_t ttype)
{
	return parser_ttype_tsbasic(ttype) || ttype == ltt_struct ||
	    ttype == ltt_union || ttype == ltt_enum || ttype == ltt_ident;
}

/** Return @c true if token type is a storage class specifier
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is a type specifier
 */
static bool parser_ttype_sclass(lexer_toktype_t ttype)
{
	return ttype == ltt_typedef || ttype == ltt_extern ||
	    ttype == ltt_static || ttype == ltt_auto || ttype == ltt_register;
}

/** Return @c true if token type is a function specifier
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is a type specifier
 */
static bool parser_ttype_fspec(lexer_toktype_t ttype)
{
	return ttype == ltt_inline;
}

/** Return @c true if token type is an attribute specifier
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is an attribute specifier
 */
static bool parser_ttype_aspec(lexer_toktype_t ttype)
{
	return ttype == ltt_attribute;
}

/** Parse integer literal.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eint(parser_t *parser, ast_node_t **rexpr)
{
	ast_eint_t *eint = NULL;
	void *dlit;
	int rc;

	rc = ast_eint_create(&eint);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_number, &dlit);
	if (rc != EOK)
		goto error;

	eint->tlit.data = dlit;
	*rexpr = &eint->node;
	return EOK;
error:
	if (eint != NULL)
		ast_tree_destroy(&eint->node);
	return rc;
}

/** Parse string literal.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_estring(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_estring_t *estring = NULL;
	void *dlit;
	int rc;

	rc = ast_estring_create(&estring);
	if (rc != EOK)
		return rc;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_strlit && ltt != ltt_ident) {
		/* Should not happen */
		rc = EINVAL;
		goto error;
	}

	parser_skip(parser, &dlit);

	rc = ast_estring_append(estring, dlit);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_strlit || ltt == ltt_ident) {
		parser_skip(parser, &dlit);

		rc = ast_estring_append(estring, dlit);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = &estring->node;
	return EOK;
error:
	if (estring != NULL)
		ast_tree_destroy(&estring->node);
	return rc;
}

/** Parse character literal.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_echar(parser_t *parser, ast_node_t **rexpr)
{
	ast_echar_t *echar = NULL;
	void *dlit;
	int rc;

	rc = ast_echar_create(&echar);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_charlit, &dlit);
	if (rc != EOK)
		goto error;

	echar->tlit.data = dlit;
	*rexpr = &echar->node;
	return EOK;
error:
	if (echar != NULL)
		ast_tree_destroy(&echar->node);
	return rc;
}

/** Parse identifier expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eident(parser_t *parser, ast_node_t **rexpr)
{
	ast_eident_t *eident = NULL;
	void *dident;
	int rc;

	rc = ast_eident_create(&eident);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_ident, &dident);
	if (rc != EOK)
		goto error;

	eident->tident.data = dident;
	*rexpr = &eident->node;
	return EOK;
error:
	if (eident != NULL)
		ast_tree_destroy(&eident->node);
	return rc;
}

/** Parse cast expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new cast expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_ecast(parser_t *parser, ast_node_t **rexpr)
{
	ast_eparen_t *eparen = NULL;
	ast_node_t *bexpr = NULL;
	ast_ecast_t *ecast = NULL;
	ast_dspecs_t *dspecs = NULL;
	ast_node_t *decl = NULL;
	void *dlparen;
	void *drparen;
	int rc;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	/* Try parsing as a type cast */
	rc = parser_process_dspecs(parser, 0, NULL, &dspecs);
	if (rc != EOK)
		goto error;

	rc = parser_process_decl(parser, &decl);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_eprefix(parser, &bexpr);
	if (rc != EOK)
		goto error;

	rc = ast_ecast_create(&ecast);
	if (rc != EOK)
		goto error;

	ecast->tlparen.data = dlparen;
	ecast->dspecs = dspecs;
	ecast->decl = decl;
	ecast->trparen.data = drparen;
	ecast->bexpr = bexpr;
	*rexpr = &ecast->node;

	return EOK;
error:
	if (eparen != NULL)
		ast_tree_destroy(&eparen->node);
	if (bexpr != NULL)
		ast_tree_destroy(bexpr);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	if (decl != NULL)
		ast_tree_destroy(decl);
	return rc;
}

/** Parse compound literal expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new compound literal expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_ecliteral(parser_t *parser, ast_node_t **rexpr)
{
	ast_eparen_t *eparen = NULL;
	ast_cinit_t *cinit = NULL;
	ast_ecliteral_t *ecliteral = NULL;
	ast_dspecs_t *dspecs = NULL;
	ast_node_t *decl = NULL;
	void *dlparen;
	void *drparen;
	int rc;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	/* Try parsing as a type cast */
	rc = parser_process_dspecs(parser, 0, NULL, &dspecs);
	if (rc != EOK)
		goto error;

	rc = parser_process_decl(parser, &decl);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_cinit(parser, &cinit);
	if (rc != EOK)
		goto error;

	rc = ast_ecliteral_create(&ecliteral);
	if (rc != EOK)
		goto error;

	ecliteral->tlparen.data = dlparen;
	ecliteral->dspecs = dspecs;
	ecliteral->decl = decl;
	ecliteral->trparen.data = drparen;
	ecliteral->cinit = cinit;
	*rexpr = &ecliteral->node;

	return EOK;
error:
	if (eparen != NULL)
		ast_tree_destroy(&eparen->node);
	if (cinit != NULL)
		ast_tree_destroy(&cinit->node);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	if (decl != NULL)
		ast_tree_destroy(decl);
	return rc;
}

/** Parse parenthesized expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eparexpr(parser_t *parser, ast_node_t **rexpr)
{
	ast_eparen_t *eparen = NULL;
	ast_node_t *bexpr = NULL;
	void *dlparen;
	void *drparen;
	int rc;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &bexpr);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	/* Parenthesized expression */
	rc = ast_eparen_create(&eparen);
	if (rc != EOK)
		goto error;

	eparen->tlparen.data = dlparen;
	eparen->bexpr = bexpr;
	eparen->trparen.data = drparen;
	*rexpr = &eparen->node;

	return EOK;
error:
	if (eparen != NULL)
		ast_tree_destroy(&eparen->node);
	if (bexpr != NULL)
		ast_tree_destroy(bexpr);
	return rc;
}

/** Parse expression beginning with left parenthesis.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eparen(parser_t *parser, ast_node_t **rexpr)
{
	parser_t *sparser = NULL;
	int rc;

	rc = parser_create_silent_sub(parser, &sparser);
	if (rc != EOK)
		goto error;

	/* Try parsing as a type cast */
	rc = parser_process_ecast(sparser, rexpr);
	if (rc == EOK) {
		/* It worked */
		parser->tok = sparser->tok;
		parser_destroy(sparser);
		return EOK;
	}

	parser_destroy(sparser);

	rc = parser_create_silent_sub(parser, &sparser);
	if (rc != EOK)
		goto error;

	/* Try parsing as a compound literal */
	rc = parser_process_ecliteral(sparser, rexpr);
	if (rc == EOK) {
		/* It worked */
		parser->tok = sparser->tok;
		parser_destroy(sparser);
		return EOK;
	}

	parser_destroy(sparser);

	/* Try parsing the as an expression in parentheses */

	rc = parser_process_eparexpr(parser, rexpr);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Parse arithmetic term.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eterm(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt, ltt2;

	ltt = parser_next_ttype(parser);

	switch (ltt) {
	case ltt_number:
		return parser_process_eint(parser, rexpr);
	case ltt_strlit:
		return parser_process_estring(parser, rexpr);
	case ltt_charlit:
		return parser_process_echar(parser, rexpr);
	case ltt_ident:
		ltt2 = parser_next_next_ttype(parser);
		if (ltt2 == ltt_strlit || ltt2 == ltt_ident)
			return parser_process_estring(parser, rexpr);
		else
			return parser_process_eident(parser, rexpr);
	case ltt_lparen:
		return parser_process_eparen(parser, rexpr);
	default:
		if (!parser->silent) {
			fprintf(stderr, "Error: ");
			parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " unexpected, expected expression.\n");
		}
		return EINVAL;
	}
}

/** Parse LTR associative binary operator expression.
 *
 * Consists of one or more expressions of higher precedence separated
 * by the operator.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_ltr_binop(parser_t *parser, lexer_toktype_t optt,
    int (*process_arg)(parser_t *, ast_node_t **), ast_binop_t optype,
    ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ebinop_t *ebinop = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dop;
	int rc;

	rc = (*process_arg)(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == optt) {
		parser_skip(parser, &dop);

		rc = (*process_arg)(parser, &eb);
		if (rc != EOK)
			goto error;

		rc = ast_ebinop_create(&ebinop);
		if (rc != EOK)
			goto error;

		ebinop->larg = ea;
		ebinop->optype = optype;
		ebinop->top.data = dop;
		ebinop->rarg = eb;

		ea = &ebinop->node;
		ebinop = NULL;
		eb = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = ea;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ebinop != NULL)
		ast_tree_destroy(&ebinop->node);
	return rc;
}

/** Parse call expression.
 *
 * @param parser Parser
 * @param ea Parsed base expression
 * @param recall Place to store pointer to new call expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_ecall(parser_t *parser, ast_node_t *ea,
    ast_ecall_t **recall)
{
	lexer_toktype_t ltt;
	ast_ecall_t *ecall = NULL;
	ast_node_t *arg = NULL;
	ast_typename_t *atypename;
	parser_t *sparser;
	void *dop;
	void *drparen;
	void *dcomma;
	int rc;

	rc = parser_match(parser, ltt_lparen, &dop);
	if (rc != EOK)
		goto error;

	rc = ast_ecall_create(&ecall);
	if (rc != EOK)
		goto error;

	ecall->tlparen.data = dop;

	ltt = parser_next_ttype(parser);
	dcomma = NULL;

	/* We can only fail this test upon entry */
	while (ltt != ltt_rparen) {
		rc = parser_create_silent_sub(parser, &sparser);
		if (rc != EOK)
			goto error;

		rc = parser_process_econcat(sparser, &arg);
		if (rc == EOK) {
			parser->tok = sparser->tok;
			parser_destroy(sparser);
		} else {
			parser_destroy(sparser);
			rc = parser_process_typename(parser, &atypename);
			if (rc != EOK)
				goto error;

			arg = &atypename->node;
		}
		rc = ast_ecall_append(ecall, dcomma,
		    arg);
		if (rc != EOK)
			goto error;

		arg = NULL;

		ltt = parser_next_ttype(parser);
		if (ltt == ltt_rparen)
			break;

		rc = parser_match(parser, ltt_comma, &dcomma);
		if (rc != EOK)
			goto error;
	}

	parser_skip(parser, &drparen);
	ecall->trparen.data = drparen;

	ecall->fexpr = ea;
	*recall = ecall;
	return EOK;
error:
	if (ecall != NULL)
		ast_tree_destroy(&ecall->node);
	if (arg != NULL)
		ast_tree_destroy(arg);
	return rc;
}

/** Parse postfix operator expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_epostfix(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_node_t *ea = NULL;
	ast_node_t *iexpr = NULL;
	ast_epostadj_t *epostadj = NULL;
	ast_emember_t *emember = NULL;
	ast_eindmember_t *eindmember = NULL;
	ast_eindex_t *eindex = NULL;
	ast_ecall_t *ecall = NULL;
	void *dop;
	void *dmember;
	void *drbracket;
	int rc;

	rc = parser_process_eterm(parser, &ea);
	if (rc != EOK)
		goto error;

	while (true) {
		ltt = parser_next_ttype(parser);
		switch (ltt) {
		case ltt_inc:
		case ltt_dec:
			parser_skip(parser, &dop);

			rc = ast_epostadj_create(&epostadj);
			if (rc != EOK)
				goto error;

			epostadj->bexpr = ea;
			epostadj->adj = ltt == ltt_inc ? aat_inc : aat_dec;
			epostadj->tadj.data = dop;

			ea = &epostadj->node;
			epostadj = NULL;
			break;
		case ltt_period:
			parser_skip(parser, &dop);

			rc = parser_match(parser, ltt_ident, &dmember);
			if (rc != EOK)
				goto error;

			rc = ast_emember_create(&emember);
			if (rc != EOK)
				goto error;

			emember->bexpr = ea;
			emember->tperiod.data = dop;
			emember->tmember.data = dmember;

			ea = &emember->node;
			emember = NULL;
			break;
		case ltt_arrow:
			parser_skip(parser, &dop);

			rc = parser_match(parser, ltt_ident, &dmember);
			if (rc != EOK)
				goto error;

			rc = ast_eindmember_create(&eindmember);
			if (rc != EOK)
				goto error;

			eindmember->bexpr = ea;
			eindmember->tarrow.data = dop;
			eindmember->tmember.data = dmember;

			ea = &eindmember->node;
			eindmember = NULL;
			break;
		case ltt_lbracket:
			parser_skip(parser, &dop);

			rc = parser_process_expr(parser, &iexpr);
			if (rc != EOK)
				goto error;

			rc = parser_match(parser, ltt_rbracket, &drbracket);
			if (rc != EOK)
				goto error;

			rc = ast_eindex_create(&eindex);
			if (rc != EOK)
				goto error;

			eindex->bexpr = ea;
			eindex->tlbracket.data = dop;
			eindex->iexpr = iexpr;
			eindex->trbracket.data = drbracket;
			iexpr = NULL;

			ea = &eindex->node;
			eindex = NULL;
			break;
		case ltt_lparen:
			rc = parser_process_ecall(parser, ea, &ecall);
			if (rc != EOK)
				goto error;

			ea = &ecall->node;
			ecall = NULL;
			break;
		default:
			*rexpr = ea;
			return EOK;
		}
	}

error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (iexpr != NULL)
		ast_tree_destroy(iexpr);
	return rc;
}

/** Parse sizeof operator expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_esizeof(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_esizeof_t *esizeof = NULL;
	ast_node_t *bexpr = NULL;
	ast_typename_t *atypename = NULL;
	parser_t *sparser;
	void *dsizeof;
	void *dlparen;
	void *drparen;
	int rc;

	rc = ast_esizeof_create(&esizeof);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_sizeof, &dsizeof);
	if (rc != EOK)
		goto error;

	esizeof->tsizeof.data = dsizeof;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lparen) {
		rc = parser_create_silent_sub(parser, &sparser);
		if (rc != EOK)
			goto error;

		rc = parser_process_eparexpr(sparser, &bexpr);
		if (rc == EOK) {
			parser->tok = sparser->tok;
			parser_destroy(sparser);
			esizeof->bexpr = bexpr;
		} else {
			parser_skip(parser, &dlparen);
			parser_destroy(sparser);

			rc = parser_process_typename(parser, &atypename);
			if (rc != EOK)
				goto error;

			rc = parser_match(parser, ltt_rparen, &drparen);
			if (rc != EOK)
				goto error;

			esizeof->tlparen.data = dlparen;
			esizeof->atypename = atypename;
			esizeof->trparen.data = drparen;
		}
	} else {
		rc = parser_process_eprefix(parser, &bexpr);
		if (rc != EOK)
			goto error;

		esizeof->tsizeof.data = dsizeof;
		esizeof->bexpr = bexpr;
	}

	*rexpr = &esizeof->node;
	return EOK;
error:
	if (bexpr != NULL)
		ast_tree_destroy(bexpr);
	if (atypename != NULL)
		ast_tree_destroy(&atypename->node);
	if (esizeof != NULL)
		ast_tree_destroy(&esizeof->node);
	return rc;
}

/** Parse prefix operator expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eprefix(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_epreadj_t *epreadj;
	ast_eusign_t *eusign;
	ast_elnot_t *elnot;
	ast_ebnot_t *ebnot;
	ast_ederef_t *ederef;
	ast_eaddr_t *eaddr;
	ast_node_t *bexpr = NULL;
	void *dop;
	int rc;

	ltt = parser_next_ttype(parser);

	switch (ltt) {
	case ltt_inc:
	case ltt_dec:
		parser_skip(parser, &dop);

		rc = parser_process_eprefix(parser, &bexpr);
		if (rc != EOK)
			goto error;

		rc = ast_epreadj_create(&epreadj);
		if (rc != EOK)
			goto error;

		epreadj->adj = ltt == ltt_inc ? aat_inc : aat_dec;
		epreadj->tadj.data = dop;
		epreadj->bexpr = bexpr;
		*rexpr = &epreadj->node;
		break;
	case ltt_plus:
	case ltt_minus:
		parser_skip(parser, &dop);

		rc = parser_process_eprefix(parser, &bexpr);
		if (rc != EOK)
			goto error;

		rc = ast_eusign_create(&eusign);
		if (rc != EOK)
			goto error;

		eusign->usign = ltt == ltt_plus ? aus_plus : aus_minus;
		eusign->tsign.data = dop;
		eusign->bexpr = bexpr;
		*rexpr = &eusign->node;
		break;
	case ltt_lnot:
		parser_skip(parser, &dop);

		rc = parser_process_eprefix(parser, &bexpr);
		if (rc != EOK)
			goto error;

		rc = ast_elnot_create(&elnot);
		if (rc != EOK)
			goto error;

		elnot->tlnot.data = dop;
		elnot->bexpr = bexpr;
		*rexpr = &elnot->node;
		break;
	case ltt_bnot:
		parser_skip(parser, &dop);

		rc = parser_process_eprefix(parser, &bexpr);
		if (rc != EOK)
			goto error;

		rc = ast_ebnot_create(&ebnot);
		if (rc != EOK)
			goto error;

		ebnot->tbnot.data = dop;
		ebnot->bexpr = bexpr;
		*rexpr = &ebnot->node;
		break;
	case ltt_asterisk:
		parser_skip(parser, &dop);

		rc = parser_process_eprefix(parser, &bexpr);
		if (rc != EOK)
			goto error;

		rc = ast_ederef_create(&ederef);
		if (rc != EOK)
			goto error;

		ederef->tasterisk.data = dop;
		ederef->bexpr = bexpr;
		*rexpr = &ederef->node;
		break;
	case ltt_amper:
		parser_skip(parser, &dop);

		rc = parser_process_eprefix(parser, &bexpr);
		if (rc != EOK)
			goto error;

		rc = ast_eaddr_create(&eaddr);
		if (rc != EOK)
			goto error;

		eaddr->tamper.data = dop;
		eaddr->bexpr = bexpr;
		*rexpr = &eaddr->node;
		break;
	case ltt_sizeof:
		rc = parser_process_esizeof(parser, rexpr);
		if (rc != EOK)
			goto error;
		break;
	default:
		return parser_process_epostfix(parser, rexpr);
	}

	return EOK;
error:
	if (bexpr != NULL)
		ast_tree_destroy(bexpr);
	return rc;
}

/** Parse multiplicative expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_emul(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ebinop_t *ebinop = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dop;
	int rc;

	rc = parser_process_eprefix(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_asterisk || ltt == ltt_slash || ltt == ltt_modulo) {
		parser_skip(parser, &dop);

		rc = parser_process_eprefix(parser, &eb);
		if (rc != EOK)
			goto error;

		rc = ast_ebinop_create(&ebinop);
		if (rc != EOK)
			goto error;

		ebinop->larg = ea;

		switch (ltt) {
		case ltt_asterisk:
			ebinop->optype = abo_times;
			break;
		case ltt_slash:
			ebinop->optype = abo_divide;
			break;
		case ltt_modulo:
			ebinop->optype = abo_modulo;
			break;
		default:
			assert(false);
		}

		ebinop->top.data = dop;
		ebinop->rarg = eb;

		ea = &ebinop->node;
		ebinop = NULL;
		eb = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = ea;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ebinop != NULL)
		ast_tree_destroy(&ebinop->node);
	return rc;
}

/** Parse additive expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eadd(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ebinop_t *ebinop = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dop;
	int rc;

	rc = parser_process_emul(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_plus || ltt == ltt_minus) {
		parser_skip(parser, &dop);

		rc = parser_process_emul(parser, &eb);
		if (rc != EOK)
			goto error;

		rc = ast_ebinop_create(&ebinop);
		if (rc != EOK)
			goto error;

		ebinop->larg = ea;
		ebinop->optype = (ltt == ltt_plus) ? abo_plus : abo_minus;
		ebinop->top.data = dop;
		ebinop->rarg = eb;

		ea = &ebinop->node;
		ebinop = NULL;
		eb = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = ea;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ebinop != NULL)
		ast_tree_destroy(&ebinop->node);
	return rc;
}

/** Parse shift expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eshift(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ebinop_t *ebinop = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dop;
	int rc;

	rc = parser_process_eadd(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_shl || ltt == ltt_shr) {
		parser_skip(parser, &dop);

		rc = parser_process_eadd(parser, &eb);
		if (rc != EOK)
			goto error;

		rc = ast_ebinop_create(&ebinop);
		if (rc != EOK)
			goto error;

		ebinop->larg = ea;
		ebinop->optype = (ltt == ltt_shl) ? abo_shl : abo_shr;
		ebinop->top.data = dop;
		ebinop->rarg = eb;

		ea = &ebinop->node;
		ebinop = NULL;
		eb = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = ea;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ebinop != NULL)
		ast_tree_destroy(&ebinop->node);
	return rc;
}

/** Parse non-equality expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eltgt(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ebinop_t *ebinop = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dop;
	int rc;

	rc = parser_process_eshift(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_less || ltt == ltt_lteq || ltt == ltt_greater ||
	    ltt == ltt_gteq) {
		parser_skip(parser, &dop);

		rc = parser_process_eshift(parser, &eb);
		if (rc != EOK)
			goto error;

		rc = ast_ebinop_create(&ebinop);
		if (rc != EOK)
			goto error;

		ebinop->larg = ea;
		switch (ltt) {
		case ltt_less:
			ebinop->optype = abo_lt;
			break;
		case ltt_lteq:
			ebinop->optype = abo_lteq;
			break;
		case ltt_greater:
			ebinop->optype = abo_gt;
			break;
		case ltt_gteq:
			ebinop->optype = abo_gteq;
			break;
		default:
			assert(false);
		}

		ebinop->top.data = dop;
		ebinop->rarg = eb;

		ea = &ebinop->node;
		ebinop = NULL;
		eb = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = ea;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ebinop != NULL)
		ast_tree_destroy(&ebinop->node);
	return rc;
}

/** Parse equality expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eequal(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ebinop_t *ebinop = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dop;
	int rc;

	rc = parser_process_eltgt(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_equal || ltt == ltt_notequal) {
		parser_skip(parser, &dop);

		rc = parser_process_eltgt(parser, &eb);
		if (rc != EOK)
			goto error;

		rc = ast_ebinop_create(&ebinop);
		if (rc != EOK)
			goto error;

		ebinop->larg = ea;
		ebinop->optype = (ltt == ltt_equal) ? abo_eq : abo_neq;
		ebinop->top.data = dop;
		ebinop->rarg = eb;

		ea = &ebinop->node;
		ebinop = NULL;
		eb = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = ea;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ebinop != NULL)
		ast_tree_destroy(&ebinop->node);
	return rc;
}

/** Parse bitwise and expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eband(parser_t *parser, ast_node_t **rexpr)
{
	return parser_process_ltr_binop(parser, ltt_amper,
	    parser_process_eequal, abo_band, rexpr);
}

/** Parse bitwise xor expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_ebxor(parser_t *parser, ast_node_t **rexpr)
{
	return parser_process_ltr_binop(parser, ltt_bxor, parser_process_eband,
	    abo_bxor, rexpr);
}

/** Parse bitwise or expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_ebor(parser_t *parser, ast_node_t **rexpr)
{
	return parser_process_ltr_binop(parser, ltt_bor, parser_process_ebxor,
	    abo_bor, rexpr);
}

/** Parse logical and expression.
 *
 * A logical or expression consists of one or more bitwise or expressions
 * separated by &&.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eland(parser_t *parser, ast_node_t **rexpr)
{
	return parser_process_ltr_binop(parser, ltt_land, parser_process_ebor,
	    abo_land, rexpr);
}

/** Parse logical or expression.
 *
 * A logical or expression consists of one or more logical and expressions
 * separated by ||.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_elor(parser_t *parser, ast_node_t **rexpr)
{
	return parser_process_ltr_binop(parser, ltt_lor, parser_process_eland,
	    abo_lor, rexpr);
}

/** Parse ternary conditional expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_etcond(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_etcond_t *etcond = NULL;
	ast_node_t *cond = NULL;
	void *dqmark;
	ast_node_t *targ = NULL;
	void *dcolon;
	ast_node_t *farg = NULL;
	int rc;

	rc = parser_process_elor(parser, &cond);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_qmark) {
		*rexpr = cond;
		return EOK;
	}

	parser_skip(parser, &dqmark);

	rc = parser_process_eassign(parser, &targ);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_colon, &dcolon);
	if (rc != EOK)
		goto error;

	rc = parser_process_etcond(parser, &farg);
	if (rc != EOK)
		goto error;

	rc = ast_etcond_create(&etcond);
	if (rc != EOK)
		goto error;

	etcond->cond = cond;
	etcond->tqmark.data = dqmark;
	etcond->targ = targ;
	etcond->tcolon.data = dcolon;
	etcond->farg = farg;

	*rexpr = &etcond->node;
	return EOK;
error:
	ast_tree_destroy(cond);
	ast_tree_destroy(targ);
	ast_tree_destroy(farg);
	return rc;
}

/** Parse assignment expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_eassign(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ebinop_t *ebinop = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dassign;
	int rc;

	rc = parser_process_etcond(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (!parser_ttype_assignop(ltt)) {
		*rexpr = ea;
		return EOK;
	}

	parser_skip(parser, &dassign);

	rc = parser_process_eassign(parser, &eb);
	if (rc != EOK)
		goto error;

	rc = ast_ebinop_create(&ebinop);
	if (rc != EOK)
		goto error;

	ebinop->larg = ea;
	ebinop->optype = abo_assign; /* XXX */
	ebinop->top.data = dassign;
	ebinop->rarg = eb;

	*rexpr = &ebinop->node;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ebinop != NULL)
		ast_tree_destroy(&ebinop->node);
	return rc;
}

/** Parse concatenation expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_econcat(parser_t *parser, ast_node_t **rexpr)
{
	ast_econcat_t *econcat = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	parser_t *sparser = NULL;
	int rc;

	rc = parser_process_eassign(parser, &ea);
	if (rc != EOK)
		goto error;

	while (true) {
		rc = parser_create_silent_sub(parser, &sparser);
		if (rc != EOK)
			goto error;

		rc = parser_process_eassign(sparser, &eb);
		if (rc != EOK)
			break;

		/* Success */
		parser->tok = sparser->tok;
		free(sparser);
		sparser = NULL;

		if (econcat == NULL) {
			rc = ast_econcat_create(&econcat);
			if (rc != EOK)
				goto error;

			rc = ast_econcat_append(econcat, ea);
			if (rc != EOK)
				goto error;

			ea = NULL;
		}

		rc = ast_econcat_append(econcat, eb);
		if (rc != EOK)
			goto error;

		eb = NULL;
	}

	parser_destroy(sparser);

	*rexpr = econcat != NULL ? &econcat->node : ea;
	return EOK;
error:
	if (sparser != NULL)
		parser_destroy(sparser);
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (econcat != NULL)
		ast_tree_destroy(&econcat->node);
	return rc;
}

/** Parse comma expression.
 *
 * A comma expression consists of one or more assignment expressions
 * separated by commas.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_ecomma(parser_t *parser, ast_node_t **rexpr)
{
	lexer_toktype_t ltt;
	ast_ecomma_t *ecomma = NULL;
	ast_node_t *ea = NULL;
	ast_node_t *eb = NULL;
	void *dcomma;
	int rc;

	rc = parser_process_econcat(parser, &ea);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_comma) {
		parser_skip(parser, &dcomma);

		rc = parser_process_econcat(parser, &eb);
		if (rc != EOK)
			goto error;

		rc = ast_ecomma_create(&ecomma);
		if (rc != EOK)
			goto error;

		ecomma->larg = ea;
		ecomma->tcomma.data = dcomma;
		ecomma->rarg = eb;

		ea = &ecomma->node;
		ecomma = NULL;
		eb = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rexpr = ea;
	return EOK;
error:
	if (ea != NULL)
		ast_tree_destroy(ea);
	if (eb != NULL)
		ast_tree_destroy(eb);
	if (ecomma != NULL)
		ast_tree_destroy(&ecomma->node);
	return rc;
}

/** Parse arithmetic expression.
 *
 * @param parser Parser
 * @param rexpr Place to store pointer to new arithmetic expression
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_expr(parser_t *parser, ast_node_t **rexpr)
{
	return parser_process_ecomma(parser, rexpr);
}

/** Parse compound initializer element.
 *
 * @param parser Parser
 * @param cinit AST compound initializer to append element to
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_cinit_elem(parser_t *parser, ast_cinit_t *cinit,
    ast_cinit_elem_t **relem)
{
	ast_cinit_elem_t *elem = NULL;
	lexer_toktype_t ltt;
	ast_node_t *init = NULL;
	ast_node_t *index = NULL;
	void *dlbracket;
	void *drbracket;
	void *dperiod;
	void *dassign;
	void *dmember;
	size_t naccs;
	int rc;

	rc = ast_cinit_elem_create(&elem);
	if (rc != EOK)
		goto error;

	naccs = 0;
	ltt = parser_next_ttype(parser);
	while (ltt == ltt_lbracket || ltt == ltt_period) {
		if (ltt == ltt_lbracket) {
			parser_skip(parser, &dlbracket);
			rc = parser_process_expr(parser, &index);
			if (rc != EOK)
				goto error;
			rc = parser_match(parser, ltt_rbracket, &drbracket);
			if (rc != EOK)
				goto error;

			rc = ast_cinit_elem_append_index(elem, dlbracket,
			    index, drbracket);
			if (rc != EOK)
				goto error;

			index = NULL;
		} else {
			/* ltt == ltt_period */
			parser_skip(parser, &dperiod);
			rc = parser_match(parser, ltt_ident, &dmember);
			if (rc != EOK)
				goto error;

			rc = ast_cinit_elem_append_member(elem, dperiod,
			    dmember);
			if (rc != EOK)
				goto error;
		}

		++naccs;
		ltt = parser_next_ttype(parser);
	}

	/* Designated initializer */
	if (naccs != 0) {
		rc = parser_match(parser, ltt_assign, &dassign);
		if (rc != EOK)
			goto error;

		elem->tassign.data = dassign;
	}

	/* Initializer expression */
	rc = parser_process_init(parser, &init);
	if (rc != EOK)
		goto error;

	elem->init = init;
	init = NULL;

	ast_cinit_append(cinit, elem);

	*relem = elem;
	return EOK;
error:
	if (index != NULL)
		ast_tree_destroy(index);
	if (elem != NULL)
		ast_cinit_elem_destroy(elem);
	if (init != NULL)
		ast_tree_destroy(init);
	return rc;
}

/** Parse compound initializer.
 *
 * @param parser Parser
 * @param rcinit Place to store pointer to new AST compound initializer
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_cinit(parser_t *parser, ast_cinit_t **rcinit)
{
	ast_cinit_t *cinit = NULL;
	ast_cinit_elem_t *elem = NULL;
	lexer_toktype_t ltt;
	void *dlbrace;
	void *dcomma;
	void *drbrace;
	int rc;

	rc = parser_match(parser, ltt_lbrace, &dlbrace);
	if (rc != EOK)
		goto error;

	rc = ast_cinit_create(&cinit);
	if (rc != EOK)
		goto error;

	cinit->tlbrace.data = dlbrace;

	ltt = parser_next_ttype(parser);
	while (ltt != ltt_rbrace) {
		rc = parser_process_cinit_elem(parser, cinit, &elem);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);
		if (ltt == ltt_comma) {
			parser_skip(parser, &dcomma);
			elem->have_comma = true;
			elem->tcomma.data = dcomma;
		} else {
			elem->have_comma = false;
		}

		if (ltt != ltt_comma)
			break;

		ltt = parser_next_ttype(parser);
	}

	rc = parser_match(parser, ltt_rbrace, &drbrace);
	if (rc != EOK)
		goto error;

	cinit->trbrace.data = drbrace;

	*rcinit = cinit;
	return EOK;
error:
	if (cinit != NULL)
		ast_tree_destroy(&cinit->node);
	return rc;
}

/** Parse initializer.
 *
 * @param parser Parser
 * @param rinit Place to store pointer to new initializer
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_init(parser_t *parser, ast_node_t **rinit)
{
	lexer_toktype_t ltt;
	ast_cinit_t *cinit;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lbrace) {
		/* Compound initializer */
		rc = parser_process_cinit(parser, &cinit);
		if (rc != EOK)
			return rc;

		*rinit = &cinit->node;
		return EOK;
	} else {
		/* Initializer expression (cannot contain comma) */
		return parser_process_econcat(parser, rinit);
	}
}

/** Parse asm statement output operands.
 *
 * @param parser Parser
 * @param aasm Asm statement to add the output operands to
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_asm_out_ops(parser_t *parser, ast_asm_t *aasm)
{
	lexer_toktype_t ltt;
	bool have_symname;
	void *dlbracket;
	void *dsymname;
	void *drbracket;
	void *dconstraint;
	void *dlparen;
	ast_node_t *expr = NULL;
	void *drparen;
	void *dcomma;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon || ltt == ltt_rparen)
		return EOK;

	while (true) {
		ltt = parser_next_ttype(parser);
		if (ltt == ltt_lbracket) {
			have_symname = true;
			parser_skip(parser, &dlbracket);

			rc = parser_match(parser, ltt_ident, &dsymname);
			if (rc != EOK)
				goto error;

			rc = parser_match(parser, ltt_rbracket, &drbracket);
			if (rc != EOK)
				goto error;
		} else {
			have_symname = false;
			dlbracket = NULL;
			dsymname = NULL;
			drbracket = NULL;
		}

		rc = parser_match(parser, ltt_strlit, &dconstraint);
		if (rc != EOK)
			goto error;

		rc = parser_match(parser, ltt_lparen, &dlparen);
		if (rc != EOK)
			goto error;

		rc = parser_process_expr(parser, &expr);
		if (rc != EOK)
			goto error;

		rc = parser_match(parser, ltt_rparen, &drparen);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);

		if (ltt == ltt_comma)
			parser_skip(parser, &dcomma);
		else
			dcomma = NULL;

		rc = ast_asm_append_out_op(aasm, have_symname, dlbracket,
		    dsymname, drbracket, dconstraint, dlparen, expr, drparen,
		    dcomma);
		if (rc != EOK)
			goto error;

		expr = NULL;

		if (ltt != ltt_comma)
			break;
	}

	return EOK;
error:
	if (expr != NULL)
		ast_tree_destroy(expr);
	return rc;
}

/** Parse asm statement input operands.
 *
 * @param parser Parser
 * @param aasm Asm statement to add the input operands to
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_asm_in_ops(parser_t *parser, ast_asm_t *aasm)
{
	lexer_toktype_t ltt;
	bool have_symname;
	void *dlbracket;
	void *dsymname;
	void *drbracket;
	void *dconstraint;
	void *dlparen;
	ast_node_t *expr = NULL;
	void *drparen;
	void *dcomma;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon || ltt == ltt_rparen)
		return EOK;

	while (true) {
		ltt = parser_next_ttype(parser);
		if (ltt == ltt_lbracket) {
			have_symname = true;
			parser_skip(parser, &dlbracket);

			rc = parser_match(parser, ltt_ident, &dsymname);
			if (rc != EOK)
				goto error;

			rc = parser_match(parser, ltt_rbracket, &drbracket);
			if (rc != EOK)
				goto error;
		} else {
			have_symname = false;
			dlbracket = NULL;
			dsymname = NULL;
			drbracket = NULL;
		}

		rc = parser_match(parser, ltt_strlit, &dconstraint);
		if (rc != EOK)
			goto error;

		rc = parser_match(parser, ltt_lparen, &dlparen);
		if (rc != EOK)
			goto error;

		rc = parser_process_expr(parser, &expr);
		if (rc != EOK)
			goto error;

		rc = parser_match(parser, ltt_rparen, &drparen);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);

		if (ltt == ltt_comma)
			parser_skip(parser, &dcomma);
		else
			dcomma = NULL;

		rc = ast_asm_append_in_op(aasm, have_symname, dlbracket,
		    dsymname, drbracket, dconstraint, dlparen, expr, drparen,
		    dcomma);
		if (rc != EOK)
			goto error;

		expr = NULL;

		if (ltt != ltt_comma)
			break;
	}

	return EOK;
error:
	if (expr != NULL)
		ast_tree_destroy(expr);
	return rc;
}

/** Parse asm statement clobber list.
 *
 * @param parser Parser
 * @param aasm Asm statement to add the clobber list elements to
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_asm_clobbers(parser_t *parser, ast_asm_t *aasm)
{
	lexer_toktype_t ltt;
	void *dclobber;
	void *dcomma;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon || ltt == ltt_rparen)
		return EOK;

	while (true) {
		rc = parser_match(parser, ltt_strlit, &dclobber);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);

		if (ltt == ltt_comma)
			parser_skip(parser, &dcomma);
		else
			dcomma = NULL;

		rc = ast_asm_append_clobber(aasm, dclobber, dcomma);
		if (rc != EOK)
			goto error;

		if (ltt != ltt_comma)
			break;
	}

	return EOK;
error:
	return rc;
}

/** Parse asm statement label list.
 *
 * @param parser Parser
 * @param aasm Asm statement to add the label list elements to
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_asm_labels(parser_t *parser, ast_asm_t *aasm)
{
	lexer_toktype_t ltt;
	void *dlabel;
	void *dcomma;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon || ltt == ltt_rparen)
		return EOK;

	while (true) {
		rc = parser_match(parser, ltt_ident, &dlabel);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);

		if (ltt == ltt_comma)
			parser_skip(parser, &dcomma);
		else
			dcomma = NULL;

		rc = ast_asm_append_label(aasm, dlabel, dcomma);
		if (rc != EOK)
			goto error;

		if (ltt != ltt_comma)
			break;
	}

	return EOK;
error:
	return rc;
}

/** Parse asm statement.
 *
 * @param parser Parser
 * @param rasm Place to store pointer to new asm statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_asm(parser_t *parser, ast_node_t **rasm)
{
	ast_asm_t *aasm = NULL;
	lexer_toktype_t ltt;
	void *dasm;
	bool have_volatile;
	void *dvolatile;
	bool have_goto;
	void *dgoto;
	void *dlparen;
	ast_node_t *atemplate = NULL;
	bool have_out_ops;
	void *dcolon1;
	bool have_in_ops;
	void *dcolon2;
	bool have_clobbers;
	void *dcolon3;
	bool have_labels;
	void *dcolon4;
	void *drparen;
	void *dscolon;
	int rc;

	rc = ast_asm_create(&aasm);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_asm, &dasm);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_volatile) {
		have_volatile = true;
		parser_skip(parser, &dvolatile);
	} else {
		have_volatile = false;
		dvolatile = NULL;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_goto) {
		have_goto = true;
		parser_skip(parser, &dgoto);
	} else {
		have_goto = false;
		dgoto = NULL;
	}

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_econcat(parser, &atemplate);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon) {
		parser_skip(parser, &dcolon1);
		if (rc != EOK)
			goto error;

		rc = parser_process_asm_out_ops(parser, aasm);
		if (rc != EOK)
			goto error;

		have_out_ops = true;
	} else {
		have_out_ops = false;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon) {
		parser_skip(parser, &dcolon2);
		if (rc != EOK)
			goto error;

		rc = parser_process_asm_in_ops(parser, aasm);
		if (rc != EOK)
			goto error;

		have_in_ops = true;
	} else {
		have_in_ops = false;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon) {
		parser_skip(parser, &dcolon3);
		if (rc != EOK)
			goto error;

		rc = parser_process_asm_clobbers(parser, aasm);
		if (rc != EOK)
			goto error;

		have_clobbers = true;
	} else {
		have_clobbers = false;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_colon) {
		parser_skip(parser, &dcolon4);
		if (rc != EOK)
			goto error;

		rc = parser_process_asm_labels(parser, aasm);
		if (rc != EOK)
			goto error;

		have_labels = true;
	} else {
		have_labels = false;
	}

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	aasm->tasm.data = dasm;
	aasm->have_volatile = have_volatile;
	if (have_volatile)
		aasm->tvolatile.data = dvolatile;
	aasm->have_goto = have_goto;
	if (have_goto)
		aasm->tgoto.data = dgoto;

	aasm->tlparen.data = dlparen;
	aasm->atemplate = atemplate;
	aasm->have_out_ops = have_out_ops;
	if (have_out_ops)
		aasm->tcolon1.data = dcolon1;
	aasm->have_in_ops = have_in_ops;
	if (have_in_ops)
		aasm->tcolon2.data = dcolon2;
	aasm->have_clobbers = have_clobbers;
	if (have_clobbers)
		aasm->tcolon3.data = dcolon3;
	aasm->have_labels = have_labels;
	if (have_labels)
		aasm->tcolon4.data = dcolon4;
	aasm->trparen.data = drparen;
	aasm->tscolon.data = dscolon;

	*rasm = &aasm->node;
	return EOK;
error:
	if (aasm != NULL)
		ast_tree_destroy(&aasm->node);
	if (atemplate != NULL)
		ast_tree_destroy(atemplate);
	return rc;
}


/** Parse break statement.
 *
 * @param parser Parser
 * @param rbreak Place to store pointer to new break statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_break(parser_t *parser, ast_node_t **rbreak)
{
	ast_break_t *abreak;
	void *dbreak;
	void *dscolon;
	int rc;

	rc = parser_match(parser, ltt_break, &dbreak);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_break_create(&abreak);
	if (rc != EOK)
		goto error;

	abreak->tbreak.data = dbreak;
	abreak->tscolon.data = dscolon;
	*rbreak = &abreak->node;
	return EOK;
error:
	return rc;
}

/** Parse continue statement.
 *
 * @param parser Parser
 * @param rcontinue Place to store pointer to new continue statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_continue(parser_t *parser, ast_node_t **rcontinue)
{
	ast_continue_t *acontinue;
	void *dcontinue;
	void *dscolon;
	int rc;

	rc = parser_match(parser, ltt_continue, &dcontinue);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_continue_create(&acontinue);
	if (rc != EOK)
		goto error;

	acontinue->tcontinue.data = dcontinue;
	acontinue->tscolon.data = dscolon;
	*rcontinue = &acontinue->node;
	return EOK;
error:
	return rc;
}


/** Parse goto statement.
 *
 * @param parser Parser
 * @param rgoto Place to store pointer to new goto statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_goto(parser_t *parser, ast_node_t **rgoto)
{
	ast_goto_t *agoto;
	ast_node_t *arg = NULL;
	void *dgoto;
	void *dtarget;
	void *dscolon;
	int rc;

	rc = parser_match(parser, ltt_goto, &dgoto);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_ident, &dtarget);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_goto_create(&agoto);
	if (rc != EOK)
		goto error;

	agoto->tgoto.data = dgoto;
	agoto->ttarget.data = dtarget;
	agoto->tscolon.data = dscolon;
	*rgoto = &agoto->node;
	return EOK;
error:
	if (arg != NULL)
		ast_tree_destroy(arg);
	return rc;
}

/** Parse return statement.
 *
 * @param parser Parser
 * @param rreturn Place to store pointer to new return statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_return(parser_t *parser, ast_node_t **rreturn)
{
	lexer_toktype_t ltt;
	ast_return_t *areturn;
	ast_node_t *arg = NULL;
	void *dreturn;
	void *dscolon;
	int rc;

	rc = parser_match(parser, ltt_return, &dreturn);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_scolon) {
		rc = parser_process_expr(parser, &arg);
		if (rc != EOK)
			goto error;
	}

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_return_create(&areturn);
	if (rc != EOK)
		goto error;

	areturn->treturn.data = dreturn;
	areturn->arg = arg;
	areturn->tscolon.data = dscolon;
	*rreturn = &areturn->node;
	return EOK;
error:
	if (arg != NULL)
		ast_tree_destroy(arg);
	return rc;
}

/** Parse if statement.
 *
 * @param parser Parser
 * @param rif Place to store pointer to new if statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_if(parser_t *parser, ast_node_t **rif)
{
	lexer_toktype_t ltt, ltt2;
	ast_if_t *aif = NULL;
	void *dif;
	void *dlparen;
	ast_node_t *cond = NULL;
	void *drparen;
	ast_block_t *tbranch = NULL;
	void *delse;
	ast_block_t *fbranch = NULL;
	ast_block_t *ebranch = NULL;
	int rc;

	rc = ast_if_create(&aif);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_if, &dif);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &cond);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_block(parser, &tbranch);
	if (rc != EOK)
		goto error;

	aif->tif.data = dif;
	aif->tlparen.data = dlparen;
	aif->cond = cond;
	aif->trparen.data = drparen;
	aif->tbranch = tbranch;
	cond = NULL;
	tbranch = NULL;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_else) {
		parser_skip(parser, &delse);

		ltt2 = parser_next_ttype(parser);
		if (ltt2 != ltt_if)
			break;

		/* Else-if part */
		rc = parser_match(parser, ltt_if, &dif);
		if (rc != EOK)
			goto error;

		rc = parser_match(parser, ltt_lparen, &dlparen);
		if (rc != EOK)
			goto error;

		rc = parser_process_expr(parser, &cond);
		if (rc != EOK)
			goto error;

		rc = parser_match(parser, ltt_rparen, &drparen);
		if (rc != EOK)
			goto error;

		rc = parser_process_block(parser, &ebranch);
		if (rc != EOK)
			goto error;

		rc = ast_if_append(aif, delse, dif, dlparen, cond, drparen,
		    ebranch);
		if (rc != EOK)
			goto error;

		cond = NULL;
		ebranch = NULL;

		ltt = parser_next_ttype(parser);
	}

	if (ltt == ltt_else) {
		rc = parser_process_block(parser, &fbranch);
		if (rc != EOK)
			goto error;
	} else {
		delse = NULL;
		fbranch = NULL;
	}

	aif->telse.data = delse;
	aif->fbranch = fbranch;

	*rif = &aif->node;
	return EOK;
error:
	if (aif != NULL)
		ast_tree_destroy(&aif->node);
	if (cond != NULL)
		ast_tree_destroy(cond);
	if (tbranch != NULL)
		ast_tree_destroy(&tbranch->node);
	if (fbranch != NULL)
		ast_tree_destroy(&fbranch->node);
	if (ebranch != NULL)
		ast_tree_destroy(&ebranch->node);
	return rc;
}

/** Parse while loop statement.
 *
 * @param parser Parser
 * @param rwhile Place to store pointer to new while loop statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_while(parser_t *parser, ast_node_t **rwhile)
{
	lexer_toktype_t ltt;
	ast_while_t *awhile = NULL;
	void *dwhile;
	void *dlparen;
	ast_node_t *cond = NULL;
	void *drparen;
	void *dscolon;
	ast_block_t *body = NULL;
	ast_stnull_t *stnull = NULL;
	int rc;

	rc = parser_match(parser, ltt_while, &dwhile);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &cond);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_scolon) {
		/* Empty body */
		parser_skip(parser, &dscolon);

		rc = ast_block_create(ast_nobraces, &body);
		if (rc != EOK)
			goto error;

		rc = ast_stnull_create(&stnull);
		if (rc != EOK)
			goto error;

		stnull->tscolon.data = dscolon;

		ast_block_append(body, &stnull->node);
		stnull = NULL;
	} else {
		rc = parser_process_block(parser, &body);
		if (rc != EOK)
			goto error;
	}

	rc = ast_while_create(&awhile);
	if (rc != EOK)
		goto error;

	awhile->twhile.data = dwhile;
	awhile->tlparen.data = dlparen;
	awhile->cond = cond;
	awhile->trparen.data = drparen;
	awhile->body = body;

	*rwhile = &awhile->node;
	return EOK;
error:
	if (cond != NULL)
		ast_tree_destroy(cond);
	if (body != NULL)
		ast_tree_destroy(&body->node);
	if (stnull != NULL)
		ast_tree_destroy(&stnull->node);
	return rc;
}

/** Parse do loop statement.
 *
 * @param parser Parser
 * @param rdo Place to store pointer to new do loop statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_do(parser_t *parser, ast_node_t **rdo)
{
	ast_do_t *ado = NULL;
	void *ddo;
	ast_block_t *body = NULL;
	void *dwhile;
	void *dlparen;
	ast_node_t *cond = NULL;
	void *drparen;
	void *dscolon;
	int rc;

	rc = parser_match(parser, ltt_do, &ddo);
	if (rc != EOK)
		goto error;

	rc = parser_process_block(parser, &body);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_while, &dwhile);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &cond);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_do_create(&ado);
	if (rc != EOK)
		goto error;

	ado->tdo.data = ddo;
	ado->body = body;
	ado->twhile.data = dwhile;
	ado->tlparen.data = dlparen;
	ado->cond = cond;
	ado->trparen.data = drparen;
	ado->tscolon.data = dscolon;

	*rdo = &ado->node;
	return EOK;
error:
	if (cond != NULL)
		ast_tree_destroy(cond);
	if (body != NULL)
		ast_tree_destroy(&body->node);
	return rc;
}

/** Parse for loop statement.
 *
 * @param parser Parser
 * @param rfor Place to store pointer to new for loop statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_for(parser_t *parser, ast_node_t **rfor)
{
	ast_for_t *afor = NULL;
	parser_t *sparser;
	lexer_toktype_t ltt;
	void *dfor;
	void *dlparen;
	ast_node_t *linit = NULL;
	ast_dspecs_t *dspecs = NULL;
	ast_idlist_t *idlist = NULL;
	void *dscolon1;
	ast_node_t *lcond = NULL;
	void *dscolon2;
	ast_node_t *lnext = NULL;
	void *drparen;
	ast_block_t *body = NULL;
	int rc;

	rc = parser_match(parser, ltt_for, &dfor);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_create_silent_sub(parser, &sparser);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);

	/* Try parsing as a declaration */
	rc = parser_process_dspecs(sparser, 0, NULL, &dspecs);
	if (rc == EOK)
		rc = parser_process_idlist(sparser, ast_abs_disallow, &idlist);

	if (rc == EOK) {
		/* Success */
		parser->tok = sparser->tok;
		parser_destroy(sparser);
		sparser = NULL;
	} else if (ltt != ltt_scolon) {
		/* Not successful. Try parsing as an expression */
		parser_destroy(sparser);
		sparser = NULL;

		rc = parser_process_expr(parser, &linit);
		if (rc != EOK)
			goto error;
	}

	rc = parser_match(parser, ltt_scolon, &dscolon1);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);

	if (ltt != ltt_scolon) {
		rc = parser_process_expr(parser, &lcond);
		if (rc != EOK)
			goto error;
	}

	rc = parser_match(parser, ltt_scolon, &dscolon2);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &lnext);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_block(parser, &body);
	if (rc != EOK)
		goto error;

	rc = ast_for_create(&afor);
	if (rc != EOK)
		goto error;

	afor->tfor.data = dfor;
	afor->tlparen.data = dlparen;
	afor->linit = linit;
	afor->dspecs = dspecs;
	afor->idlist = idlist;
	afor->tscolon1.data = dscolon1;
	afor->lcond = lcond;
	afor->tscolon2.data = dscolon2;
	afor->lnext = lnext;
	afor->trparen.data = drparen;
	afor->body = body;

	*rfor = &afor->node;
	return EOK;
error:
	if (linit != NULL)
		ast_tree_destroy(linit);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	if (idlist != NULL)
		ast_tree_destroy(&idlist->node);
	if (lcond != NULL)
		ast_tree_destroy(lcond);
	if (lnext != NULL)
		ast_tree_destroy(lnext);
	if (body != NULL)
		ast_tree_destroy(&body->node);
	return rc;
}

/** Parse switch statement.
 *
 * @param parser Parser
 * @param rswitch Place to store pointer to new switch statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_switch(parser_t *parser, ast_node_t **rswitch)
{
	ast_switch_t *aswitch = NULL;
	void *dswitch;
	void *dlparen;
	ast_node_t *sexpr = NULL;
	void *drparen;
	ast_block_t *body = NULL;
	int rc;

	rc = parser_match(parser, ltt_switch, &dswitch);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &sexpr);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = parser_process_block(parser, &body);
	if (rc != EOK)
		goto error;

	rc = ast_switch_create(&aswitch);
	if (rc != EOK)
		goto error;

	aswitch->tswitch.data = dswitch;
	aswitch->tlparen.data = dlparen;
	aswitch->sexpr = sexpr;
	aswitch->trparen.data = drparen;
	aswitch->body = body;

	*rswitch = &aswitch->node;
	return EOK;
error:
	if (sexpr != NULL)
		ast_tree_destroy(sexpr);
	if (body != NULL)
		ast_tree_destroy(&body->node);
	return rc;
}

/** Parse case label.
 *
 * @param parser Parser
 * @param rclabel Place to store pointer to new case label
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_clabel(parser_t *parser, ast_node_t **rclabel)
{
	ast_clabel_t *clabel = NULL;
	void *dcase;
	ast_node_t *cexpr = NULL;
	void *dcolon;
	int rc;

	rc = parser_match(parser, ltt_case, &dcase);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &cexpr);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_colon, &dcolon);
	if (rc != EOK)
		goto error;

	rc = ast_clabel_create(&clabel);
	if (rc != EOK)
		goto error;

	clabel->tcase.data = dcase;
	clabel->cexpr = cexpr;
	clabel->tcolon.data = dcolon;

	*rclabel = &clabel->node;
	return EOK;
error:
	if (cexpr != NULL)
		ast_tree_destroy(cexpr);
	return rc;
}

/** Parse goto label.
 *
 * @param parser Parser
 * @param rglabel Place to store pointer to new goto label
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_glabel(parser_t *parser, ast_node_t **rglabel)
{
	ast_glabel_t *glabel = NULL;
	void *dlabel;
	void *dcolon;
	int rc;

	rc = parser_match(parser, ltt_ident, &dlabel);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_colon, &dcolon);
	if (rc != EOK)
		goto error;

	rc = ast_glabel_create(&glabel);
	if (rc != EOK)
		goto error;

	glabel->tlabel.data = dlabel;
	glabel->tcolon.data = dcolon;

	*rglabel = &glabel->node;
	return EOK;
error:
	return rc;
}

/** Parse loop macro invocation.
 *
 * @param parser Parser
 * @param expr Loop macro invocation expression
 * @param rstmt Place to store pointer to new statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_lmacro(parser_t *parser, ast_node_t *expr,
    ast_node_t **rstmt)
{
	ast_lmacro_t *lmacro = NULL;
	ast_block_t *block = NULL;
	int rc;

	rc = ast_lmacro_create(&lmacro);
	if (rc != EOK)
		goto error;

	rc = parser_process_block(parser, &block);
	if (rc != EOK)
		goto error;

	lmacro->expr = expr;
	lmacro->body = block;

	*rstmt = &lmacro->node;
	return EOK;
error:
	if (lmacro != NULL)
		ast_tree_destroy(&lmacro->node);
	return rc;
}

/** Parse expression statement.
 *
 * @param parser Parser
 * @param rstmt Place to store pointer to new statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_stexpr(parser_t *parser, ast_node_t **rstmt)
{
	lexer_toktype_t ltt;
	ast_stexpr_t *stexpr = NULL;
	ast_node_t *expr = NULL;
	void *dscolon;
	int rc;

	rc = ast_stexpr_create(&stexpr);
	if (rc != EOK)
		goto error;

	rc = parser_process_expr(parser, &expr);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_scolon && expr->ntype == ant_ecall) {
		/* Possibly a loop macro */
		rc = parser_process_lmacro(parser, expr, rstmt);
		if (rc != EOK)
			goto error;

		return EOK;
	}

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	stexpr->expr = expr;
	stexpr->tscolon.data = dscolon;

	*rstmt = &stexpr->node;
	return EOK;
error:
	if (stexpr != NULL)
		ast_tree_destroy(&stexpr->node);
	if (expr != NULL)
		ast_tree_destroy(expr);
	return rc;
}

/** Parse declaration statement.
 *
 * @param parser Parser
 * @param rnode Place to store pointer to new declaration statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_stdecln(parser_t *parser, ast_node_t **rstmt)
{
	ast_stdecln_t *stdecln = NULL;
	ast_dspecs_t *dspecs = NULL;
	ast_idlist_t *idlist = NULL;
	ast_node_t *init = NULL;
	void *dscolon;
	int rc;

	rc = parser_process_dspecs(parser, 0, NULL, &dspecs);
	if (rc != EOK)
		goto error;

	rc = parser_process_idlist(parser, ast_abs_disallow, &idlist);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_stdecln_create(&stdecln);
	if (rc != EOK)
		goto error;

	stdecln->dspecs = dspecs;
	stdecln->idlist = idlist;

	stdecln->tscolon.data = dscolon;

	*rstmt = &stdecln->node;
	return EOK;
error:
	if (stdecln != NULL)
		ast_tree_destroy(&stdecln->node);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	if (idlist != NULL)
		ast_tree_destroy(&idlist->node);
	if (init != NULL)
		ast_tree_destroy(init);
	return rc;
}

/** Parse statement.
 *
 * @param parser Parser
 * @param rstmt Place to store pointer to new statement
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_stmt(parser_t *parser, ast_node_t **rstmt)
{
	lexer_toktype_t ltt, ltt2;
	parser_t *sparser;
	int rc;

	ltt = parser_next_ttype(parser);

	switch (ltt) {
	case ltt_asm:
		return parser_process_asm(parser, rstmt);
	case ltt_break:
		return parser_process_break(parser, rstmt);
	case ltt_continue:
		return parser_process_continue(parser, rstmt);
	case ltt_goto:
		return parser_process_goto(parser, rstmt);
	case ltt_return:
		return parser_process_return(parser, rstmt);
	case ltt_if:
		return parser_process_if(parser, rstmt);
	case ltt_while:
		return parser_process_while(parser, rstmt);
	case ltt_do:
		return parser_process_do(parser, rstmt);
	case ltt_for:
		return parser_process_for(parser, rstmt);
	case ltt_switch:
		return parser_process_switch(parser, rstmt);
	case ltt_case:
		return parser_process_clabel(parser, rstmt);
	case ltt_ident:
		ltt2 = parser_next_next_ttype(parser);
		if (ltt2 == ltt_colon)
			return parser_process_glabel(parser, rstmt);
		/* fall through */
	default:
		break;
	}

	rc = parser_create_silent_sub(parser, &sparser);
	if (rc != EOK)
		return rc;

	/* Try parsing the statement as a declaration */
	rc = parser_process_stdecln(sparser, rstmt);
	if (rc == EOK) {
		/* It worked */
		parser->tok = sparser->tok;
		parser_destroy(sparser);
	} else {
		/* Didn't work. Try parsing as an expression instead */
		parser_destroy(sparser);
		rc = parser_process_stexpr(parser, rstmt);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parse block.
 *
 * @param parser Parser
 * @param rblock Place to store pointer to new AST block
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_block(parser_t *parser, ast_block_t **rblock)
{
	ast_block_t *block;
	ast_braces_t braces;
	ast_node_t *stmt;
	void *dopen;
	void *dclose;
	int rc;

	if (parser_next_ttype(parser) == ltt_lbrace) {
		braces = ast_braces;
		parser_skip(parser, &dopen);
	} else {
		braces = ast_nobraces;
	}

	rc = ast_block_create(braces, &block);
	if (rc != EOK)
		return rc;

	if (braces == ast_braces) {
		/* Brace-enclosed block */
		while (parser_next_ttype(parser) != ltt_rbrace) {
			rc = parser_process_stmt(parser, &stmt);
			if (rc != EOK)
				goto error;

			ast_block_append(block, stmt);
		}

		/* Skip closing brace */
		parser_skip(parser, &dclose);
		block->topen.data = dopen;
		block->tclose.data = dclose;
	} else {
		/* Single statement */
		rc = parser_process_stmt(parser, &stmt);
		if (rc != EOK)
			goto error;

		ast_block_append(block, stmt);
	}

	*rblock = block;
	return EOK;
error:
	ast_tree_destroy(&block->node);
	return rc;
}

/** Parse type qualifier.
 *
 * @param parser Parser
 * @param rtqual Place to store pointer to new AST type qualifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tqual(parser_t *parser, ast_tqual_t **rtqual)
{
	ast_tqual_t *tqual;
	lexer_toktype_t ltt;
	ast_qtype_t qtype;
	void *dqual;
	int rc;

	ltt = parser_next_ttype(parser);

	switch (ltt) {
	case ltt_const:
		qtype = aqt_const;
		break;
	case ltt_restrict:
		qtype = aqt_restrict;
		break;
	case ltt_restrict_alt:
		qtype = aqt_restrict;
		break;
	case ltt_volatile:
		qtype = aqt_volatile;
		break;
	default:
		assert(false);
		return EINVAL;
	}

	parser_skip(parser, &dqual);

	rc = ast_tqual_create(qtype, &tqual);
	if (rc != EOK)
		return rc;

	tqual->tqual.data = dqual;

	*rtqual = tqual;
	return EOK;
}


/** Parse basic type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST type specifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsbasic(parser_t *parser, ast_node_t **rtype)
{
	ast_tsbasic_t *pbasic;
	lexer_toktype_t ltt;
	void *dbasic;
	int rc;

	ltt = parser_next_ttype(parser);
	assert(parser_ttype_tsbasic(ltt));

	parser_skip(parser, &dbasic);

	rc = ast_tsbasic_create(&pbasic);
	if (rc != EOK)
		return rc;

	pbasic->tbasic.data = dbasic;

	*rtype = &pbasic->node;
	return EOK;
}

/** Parse identifier type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST identifier type specifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsident(parser_t *parser, ast_node_t **rtype)
{
	ast_tsident_t *pident;
	lexer_toktype_t ltt;
	void *dident;
	int rc;

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_ident:
		break;
	default:
		if (!parser->silent) {
			fprintf(stderr, "Error: ");
			parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " unexpected, expected type "
			    "identifer.\n");
		}
		return EINVAL;
	}

	parser_skip(parser, &dident);

	rc = ast_tsident_create(&pident);
	if (rc != EOK)
		return rc;

	pident->tident.data = dident;

	*rtype = &pident->node;
	return EOK;
}

/** Parse record type specifier element.
 *
 * @param parser Parser
 * @param tsrecord Record type specifier to append element to
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsrecord_elem(parser_t *parser,
    ast_tsrecord_t *tsrecord)
{
	lexer_toktype_t ltt;
	ast_sqlist_t *sqlist = NULL;
	ast_dlist_t *dlist = NULL;
	void *dscolon;
	ast_abs_allow_t allow;
	int rc;

	rc = parser_process_sqlist(parser, &sqlist);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ast_sqlist_has_tsrecord(sqlist) &&
	    ltt == ltt_scolon) {
		/* Allow anonymous sub-struct or sub-union */
		allow = ast_abs_allow;
	} else {
		allow = ast_abs_disallow;
	}

	rc = parser_process_dlist(parser, allow, &dlist);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_tsrecord_append(tsrecord, sqlist, dlist, dscolon);
	if (rc != EOK)
		goto error;

	sqlist = NULL;
	dlist = NULL;

	return EOK;
error:
	if (sqlist != NULL)
		ast_tree_destroy(&sqlist->node);
	if (dlist != NULL)
		ast_tree_destroy(&dlist->node);
	return rc;
}

/** Parse record type specifier element using a macro declaration.
 *
 * @param parser Parser
 * @param tsrecord Record type specifier to append element to
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsrecord_elem_mdecln(parser_t *parser,
    ast_tsrecord_t *tsrecord)
{
	ast_mdecln_t *mdecln = NULL;
	void *dscolon;
	int rc;

	rc = parser_process_mdecln(parser, &mdecln);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	rc = ast_tsrecord_append_mdecln(tsrecord, mdecln, dscolon);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (mdecln != NULL)
		ast_tree_destroy(&mdecln->node);
	return rc;
}


/** Parse record type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST record type specifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsrecord(parser_t *parser, ast_node_t **rtype)
{
	ast_tsrecord_t *precord = NULL;
	ast_rtype_t rt;
	lexer_toktype_t ltt;
	void *dsu;
	void *dident;
	void *dlbrace;
	ast_sqlist_t *sqlist = NULL;
	ast_dlist_t *dlist = NULL;
	ast_aslist_t *aslist;
	void *drbrace;
	parser_t *sparser = NULL;
	int rc;

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_struct:
		rt = ar_struct;
		break;
	case ltt_union:
		rt = ar_union;
		break;
	default:
		assert(false);
		return EINVAL;
	}

	rc = ast_tsrecord_create(rt, &precord);
	if (rc != EOK)
		return rc;

	parser_skip(parser, &dsu);
	precord->tsu.data = dsu;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_attribute) {
		rc = parser_process_aslist(parser, &aslist);
		if (rc != EOK)
			goto error;

		precord->aslist1 = aslist;
		aslist = NULL;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_ident) {
		parser_skip(parser, &dident);
		precord->have_ident = true;
		precord->tident.data = dident;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lbrace) {
		precord->have_def = true;
		parser_skip(parser, &dlbrace);

		precord->tlbrace.data = dlbrace;

		ltt = parser_next_ttype(parser);
		while (ltt != ltt_rbrace) {
			rc = parser_create_silent_sub(parser, &sparser);
			if (rc != EOK)
				goto error;

			rc = parser_process_tsrecord_elem(sparser, precord);
			if (rc == EOK) {
				parser->tok = sparser->tok;
			} else {
				sparser->tok = parser->tok;

				rc = parser_process_tsrecord_elem_mdecln(
				    sparser, precord);
				if (rc == EOK) {
					parser->tok = sparser->tok;
				} else {
					/* To get a good error message */
					rc = parser_process_tsrecord_elem(
					    parser, precord);
					if (rc != EOK)
						goto error;
				}
			}

			parser_destroy(sparser);
			sparser = NULL;

			ltt = parser_next_ttype(parser);
		}

		rc = parser_match(parser, ltt_rbrace, &drbrace);
		if (rc != EOK)
			goto error;

		precord->trbrace.data = drbrace;

		ltt = parser_next_ttype(parser);
		if (ltt == ltt_attribute) {
			rc = parser_process_aslist(parser, &aslist);
			if (rc != EOK)
				goto error;

			precord->aslist2 = aslist;
			aslist = NULL;
		}
	}

	parser_destroy(sparser);

	*rtype = &precord->node;
	return EOK;
error:
	if (sparser != NULL)
		parser_destroy(sparser);
	if (precord != NULL)
		ast_tree_destroy(&precord->node);
	if (sqlist != NULL)
		ast_tree_destroy(&sqlist->node);
	if (dlist != NULL)
		ast_tree_destroy(&dlist->node);
	return rc;
}

/** Parse enum type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST enum type specifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsenum(parser_t *parser, ast_node_t **rtype)
{
	ast_tsenum_t *penum = NULL;
	lexer_toktype_t ltt;
	void *denum;
	void *dident;
	void *dlbrace;
	void *delem;
	void *dequals;
	ast_node_t *init = NULL;
	void *dcomma;
	void *drbrace;
	int rc;

	rc = ast_tsenum_create(&penum);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_enum, &denum);
	if (rc != EOK)
		goto error;

	penum->tenum.data = denum;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_ident) {
		parser_skip(parser, &dident);
		penum->have_ident = true;
		penum->tident.data = dident;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lbrace) {
		penum->have_def = true;

		parser_skip(parser, &dlbrace);
		penum->tlbrace.data = dlbrace;

		ltt = parser_next_ttype(parser);
		while (ltt != ltt_rbrace) {
			rc = parser_match(parser, ltt_ident, &delem);
			if (rc != EOK)
				goto error;

			ltt = parser_next_ttype(parser);
			if (ltt == ltt_assign) {
				parser_skip(parser, &dequals);

				/*
				 * Initializer expression must not contain
				 * a comma
				 */
				rc = parser_process_econcat(parser, &init);
				if (rc != EOK)
					goto error;
			} else {
				dequals = NULL;
				init = NULL;
			}

			ltt = parser_next_ttype(parser);
			if (ltt == ltt_comma)
				parser_skip(parser, &dcomma);
			else
				dcomma = NULL;

			rc = ast_tsenum_append(penum, delem, dequals, init,
			    dcomma);
			if (rc != EOK)
				goto error;

			init = NULL;

			if (ltt != ltt_comma)
				break;

			ltt = parser_next_ttype(parser);
		}

		rc = parser_match(parser, ltt_rbrace, &drbrace);
		if (rc != EOK)
			goto error;

		penum->trbrace.data = drbrace;
	}

	*rtype = &penum->node;
	return EOK;
error:
	if (penum != NULL)
		ast_tree_destroy(&penum->node);
	if (init != NULL)
		ast_tree_destroy(init);
	return rc;
}

/** Parse type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST type
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tspec(parser_t *parser, ast_node_t **rtype)
{
	lexer_toktype_t ltt;
	int rc;

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_ident:
		rc = parser_process_tsident(parser, rtype);
		break;
	case ltt_struct:
	case ltt_union:
		rc = parser_process_tsrecord(parser, rtype);
		break;
	case ltt_enum:
		rc = parser_process_tsenum(parser, rtype);
		break;
	default:
		if (parser_ttype_tsbasic(ltt)) {
			rc = parser_process_tsbasic(parser, rtype);
		} else {
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				parser_dprint_next_tok(parser, stderr);
				fprintf(stderr, " unexpected, expected type "
				    "specifier.\n");
			}
			return EINVAL;
		}
		break;
	}

	return rc;
}

/** Parse specifier-qualifier list.
 *
 * @param parser Parser
 * @param rsqlist Place to store pointer to new AST specifier-qualifier list
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_sqlist(parser_t *parser, ast_sqlist_t **rsqlist)
{
	lexer_toktype_t ltt;
	ast_sqlist_t *sqlist;
	ast_tqual_t *tqual;
	ast_node_t *elem;
	bool have_tspec;
	int rc;

	rc = ast_sqlist_create(&sqlist);
	if (rc != EOK)
		return rc;

	have_tspec = false;

	ltt = parser_next_ttype(parser);
	do {
		if (parser_ttype_tspec(ltt)) {
			/*
			 * Stop before identifier if we already have
			 * a specifier
			 */
			if (ltt == ltt_ident && have_tspec)
				break;

			rc = parser_process_tspec(parser, &elem);
			if (rc != EOK)
				goto error;

			have_tspec = true;
		} else if (parser_ttype_tqual(ltt)) {
			rc = parser_process_tqual(parser, &tqual);
			if (rc != EOK)
				goto error;
			elem = &tqual->node;
		} else {
			/* Unexpected */
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				parser_dprint_next_tok(parser, stderr);
				fprintf(stderr, " unexpected, expected "
				    "type specifier or qualifier.\n");
			}

			return EINVAL;
		}

		ast_sqlist_append(sqlist, elem);
		ltt = parser_next_ttype(parser);
	} while (parser_ttype_tspec(ltt) || parser_ttype_tqual(ltt));

	*rsqlist = sqlist;
	return EOK;
error:
	ast_tree_destroy(&sqlist->node);
	return rc;
}

/** Parse (possibly empty) type qualifier list.
 *
 * @param parser Parser
 * @param rtqlist Place to store pointer to new AST type qualifier list
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tqlist(parser_t *parser, ast_tqlist_t **rtqlist)
{
	lexer_toktype_t ltt;
	ast_tqlist_t *tqlist;
	ast_tqual_t *tqual;
	ast_node_t *elem;
	int rc;

	rc = ast_tqlist_create(&tqlist);
	if (rc != EOK)
		return rc;

	ltt = parser_next_ttype(parser);
	while (parser_ttype_tqual(ltt)) {
		rc = parser_process_tqual(parser, &tqual);
		if (rc != EOK)
			goto error;

		elem = &tqual->node;

		ast_tqlist_append(tqlist, elem);
		ltt = parser_next_ttype(parser);
	}

	*rtqlist = tqlist;
	return EOK;
error:
	ast_tree_destroy(&tqlist->node);
	return rc;
}

/** Parse declaration specifiers.
 *
 * @param parser Parser
 * @param add_idents Number of additional identifiers allowes (these are
 *                   assumed to be macro specifiers).
 * @param more_idents Place to store @c true if we stopped because of finding
 *                    another identifier or @c NULL if not interested
 * @param rdspecs Place to store pointer to new AST declaration specifiers
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_dspecs(parser_t *parser, unsigned add_idents,
    bool *more_idents, ast_dspecs_t **rdspecs)
{
	lexer_toktype_t ltt;
	ast_sclass_t *sclass;
	ast_dspecs_t *dspecs;
	ast_tqual_t *tqual;
	ast_fspec_t *fspec;
	ast_aspec_t *aspec;
	ast_node_t *elem;
	unsigned idcnt;
	bool have_tspec;
	int rc;

	if (more_idents != NULL)
		*more_idents = false;

	rc = ast_dspecs_create(&dspecs);
	if (rc != EOK)
		return rc;

	have_tspec = false;
	idcnt = 0;

	ltt = parser_next_ttype(parser);
	do {
		if (parser_ttype_sclass(ltt)) {
			rc = parser_process_sclass(parser, &sclass);
			if (rc != EOK)
				goto error;
			elem = &sclass->node;
		} else if (parser_ttype_tspec(ltt)) {
			/*
			 * Stop before identifier if we already have
			 * a specifier
			 */
			if (ltt == ltt_ident && have_tspec &&
			    idcnt >= add_idents) {
				if (more_idents != NULL)
					*more_idents = true;
				break;
			}

			rc = parser_process_tspec(parser, &elem);
			if (rc != EOK)
				goto error;

			if (ltt == ltt_ident && have_tspec)
				++idcnt;
			have_tspec = true;
		} else if (parser_ttype_tqual(ltt)) {
			rc = parser_process_tqual(parser, &tqual);
			if (rc != EOK)
				goto error;
			elem = &tqual->node;
		} else if (parser_ttype_fspec(ltt)) {
			/* Function specifier */
			rc = parser_process_fspec(parser, &fspec);
			if (rc != EOK)
				goto error;
			elem = &fspec->node;
		} else if (parser_ttype_aspec(ltt)) {
			/* Attribute specifier */
			rc = parser_process_aspec(parser, &aspec);
			if (rc != EOK)
				goto error;
			elem = &aspec->node;
		} else {
			/* Unexpected */
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				parser_dprint_next_tok(parser, stderr);
				fprintf(stderr, " unexpected, expected "
				    "declaration specifier.\n");
			}
			return EINVAL;
		}

		ast_dspecs_append(dspecs, elem);
		ltt = parser_next_ttype(parser);
	} while (parser_ttype_sclass(ltt) || parser_ttype_tspec(ltt) ||
	    parser_ttype_tqual(ltt) || parser_ttype_fspec(ltt) ||
	    parser_ttype_aspec(ltt));

	*rdspecs = dspecs;
	return EOK;
error:
	ast_tree_destroy(&dspecs->node);
	return rc;
}

/** Parse identifier declarator.
 *
 * @param parser Parser
 * @param rdecl Place to store pointer to new AST declarator
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_dident(parser_t *parser, ast_node_t **rdecl)
{
	ast_dident_t *decl;
	ast_dnoident_t *ndecl;
	lexer_toktype_t ltt;
	void *dident;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_ident) {
		rc = ast_dnoident_create(&ndecl);
		if (rc != EOK)
			return rc;

		*rdecl = &ndecl->node;
		return EOK;
	}

	parser_skip(parser, &dident);

	rc = ast_dident_create(&decl);
	if (rc != EOK)
		return rc;

	decl->tident.data = dident;

	*rdecl = &decl->node;
	return EOK;
}

/** Parse possible parenthesized declarator.
 *
 * @param parser Parser
 * @param rdecl Place to store pointer to new AST declarator
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_dparen(parser_t *parser, ast_node_t **rdecl)
{
	ast_dparen_t *dparen = NULL;
	ast_node_t *bdecl = NULL;
	lexer_toktype_t ltt;
	void *dlparen;
	void *drparen;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_lparen)
		return parser_process_dident(parser, rdecl);

	parser_skip(parser, &dlparen);

	rc = ast_dparen_create(&dparen);
	if (rc != EOK)
		return rc;

	dparen->tlparen.data = dlparen;

	rc = parser_process_decl(parser, &bdecl);
	if (rc != EOK)
		goto error;

	dparen->bdecl = bdecl;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	dparen->trparen.data = drparen;

	*rdecl = &dparen->node;
	return EOK;
error:
	if (dparen != NULL)
		ast_tree_destroy(&dparen->node);
	return rc;
}

/** Parse possible array declarator.
 *
 * @param parser Parser
 * @param rdecl Place to store pointer to new AST declarator
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_darray(parser_t *parser, ast_node_t **rdecl)
{
	ast_darray_t *darray = NULL;
	ast_node_t *bdecl = NULL;
	lexer_toktype_t ltt;
	void *dlbracket;
	ast_node_t *asize;
	void *drbracket;
	int rc;

	rc = parser_process_dparen(parser, &bdecl);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_lbracket) {
		rc = ast_darray_create(&darray);
		if (rc != EOK)
			goto error;

		darray->bdecl = bdecl;

		parser_skip(parser, &dlbracket);
		darray->tlbracket.data = dlbracket;

		ltt = parser_next_ttype(parser);
		if (ltt != ltt_rbracket) {
			rc = parser_process_expr(parser, &asize);
			if (rc != EOK)
				goto error;
		} else {
			/* No size specified */
			asize = NULL;
		}

		darray->asize = asize;

		rc = parser_match(parser, ltt_rbracket, &drbracket);
		if (rc != EOK)
			goto error;

		darray->trbracket.data = drbracket;
		bdecl = &darray->node;

		ltt = parser_next_ttype(parser);
	}

	*rdecl = bdecl;
	return EOK;
error:
	if (darray != NULL)
		ast_tree_destroy(&darray->node);
	return rc;
}

/** Parse possible function declarator.
 *
 * @param parser Parser
 * @param rdecl Place to store pointer to new AST declarator
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_dfun(parser_t *parser, ast_node_t **rdecl)
{
	ast_dfun_t *dfun = NULL;
	ast_node_t *bdecl = NULL;
	lexer_toktype_t ltt;
	void *dlparen;
	ast_dspecs_t *dspecs;
	ast_node_t *decl;
	ast_aslist_t *aslist;
	void *dcomma;
	void *dellipsis;
	void *drparen;
	int rc;

	rc = parser_process_darray(parser, &bdecl);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_lparen) {
		*rdecl = bdecl;
		return EOK;
	}

	rc = ast_dfun_create(&dfun);
	if (rc != EOK)
		goto error;

	dfun->bdecl = bdecl;

	parser_skip(parser, &dlparen);
	dfun->tlparen.data = dlparen;

	/* Parse arguments */
	ltt = parser_next_ttype(parser);
	if (ltt != ltt_rparen) {
		do {
			ltt = parser_next_ttype(parser);
			if (ltt == ltt_ellipsis)
				break;

			rc = parser_process_dspecs(parser, 0, NULL, &dspecs);
			if (rc != EOK)
				goto error;

			rc = parser_process_decl(parser, &decl);
			if (rc != EOK) {
				ast_tree_destroy(&dspecs->node);
				goto error;
			}

			ltt = parser_next_ttype(parser);
			if (ltt == ltt_attribute) {
				rc = parser_process_aslist(parser, &aslist);
				if (rc != EOK)
					goto error;
			} else {
				aslist = NULL;
			}

			ltt = parser_next_ttype(parser);
			if (ltt != ltt_rparen) {
				rc = parser_match(parser, ltt_comma, &dcomma);
				if (rc != EOK) {
					ast_tree_destroy(&dspecs->node);
					goto error;
				}
			} else {
				dcomma = NULL;
			}

			rc = ast_dfun_append(dfun, dspecs, decl, aslist,
			    dcomma);
			if (rc != EOK)
				goto error;
		} while (ltt != ltt_rparen);

		if (ltt == ltt_ellipsis) {
			parser_skip(parser, &dellipsis);

			dfun->have_ellipsis = true;
			dfun->tellipsis.data = dellipsis;
		}
	}

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	dfun->trparen.data = drparen;

	*rdecl = &dfun->node;
	return EOK;
error:
	if (dfun != NULL)
		ast_tree_destroy(&dfun->node);
	return rc;
}

/** Parse possible pointer declarator.
 *
 * @param parser Parser
 * @param rdecl Place to store pointer to new AST declarator
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_dptr(parser_t *parser, ast_node_t **rdecl)
{
	ast_dptr_t *dptr = NULL;
	ast_tqlist_t *tqlist = NULL;
	ast_node_t *bdecl;
	lexer_toktype_t ltt;
	void *dasterisk;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_asterisk)
		return parser_process_dfun(parser, rdecl);

	parser_skip(parser, &dasterisk);

	rc = ast_dptr_create(&dptr);
	if (rc != EOK)
		return rc;

	dptr->tasterisk.data = dasterisk;

	rc = parser_process_tqlist(parser, &tqlist);
	if (rc != EOK)
		goto error;

	rc = parser_process_decl(parser, &bdecl);
	if (rc != EOK)
		goto error;

	dptr->tqlist = tqlist;
	dptr->bdecl = bdecl;
	*rdecl = &dptr->node;
	return EOK;
error:
	if (dptr != NULL)
		ast_tree_destroy(&dptr->node);
	if (tqlist != NULL)
		ast_tree_destroy(&tqlist->node);
	return rc;
}

/** Parse declarator.
 *
 * @param parser Parser
 * @param rdecl Place to store pointer to new AST declarator
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_decl(parser_t *parser, ast_node_t **rdecl)
{
	return parser_process_dptr(parser, rdecl);
}

/** Parse declarator list.
 *
 * @param parser Parser
 * @param aallow @c ast_abs_allow to allow abstract declarators,
 *        ast_abs_disallow to disallow them
 * @param rdlist Place to store pointer to new declarator list
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_dlist(parser_t *parser, ast_abs_allow_t aallow,
    ast_dlist_t **rdlist)
{
	lexer_toktype_t ltt;
	lexer_tok_t dtok;
	ast_dlist_t *dlist;
	ast_node_t *decl = NULL;
	void *dcomma;
	bool have_bitwidth;
	void *dcolon;
	ast_node_t *bitwidth = NULL;
	bool first;
	int rc;

	rc = ast_dlist_create(&dlist);
	if (rc != EOK)
		goto error;

	first = true;
	do {
		if (first) {
			dcomma = NULL;
		} else {
			rc = parser_match(parser, ltt_comma, &dcomma);
			if (rc != EOK)
				goto error;
		}

		parser_read_next_tok(parser, &dtok);

		rc = parser_process_decl(parser, &decl);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);

		if (first && ast_decl_is_abstract(decl) &&
		    aallow != ast_abs_allow && ltt != ltt_colon) {
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				lexer_dprint_tok(&dtok, stderr);
				fprintf(stderr, " unexpected abstract "
				    "declarator.\n");
			}
			rc = EINVAL;
			goto error;
		}

		/*
		 * XXX Hack so as not to produce false warnings for macro
		 * declarators at the cost of treating declarators that are
		 * totally enclosed in parentheses as not valid C code even
		 * if they are.
		 */
		if (first && decl->ntype == ant_dparen) {
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				lexer_dprint_tok(&dtok, stderr);
				fprintf(stderr, " parenthesized declarator "
				    "(cough).\n");
			}
			rc = EINVAL;
			goto error;
		}

		if (ltt == ltt_colon) {
			/* Bit width */
			have_bitwidth = true;
			parser_skip(parser, &dcolon);

			rc = parser_process_econcat(parser, &bitwidth);
			if (rc != EOK)
				goto error;
		} else {
			have_bitwidth = false;
			dcolon = NULL;
			bitwidth = NULL;
		}

		rc = ast_dlist_append(dlist, dcomma, decl, have_bitwidth,
		    dcolon, bitwidth);
		if (rc != EOK)
			goto error;

		decl = NULL;
		bitwidth = NULL;

		first = false;
		ltt = parser_next_ttype(parser);
	} while (ltt == ltt_comma);

	*rdlist = dlist;
	return EOK;
error:
	if (dlist != NULL)
		ast_tree_destroy(&dlist->node);
	if (decl != NULL)
		ast_tree_destroy(decl);
	if (bitwidth != NULL)
		ast_tree_destroy(bitwidth);
	return rc;
}

/** Parse init-declarator list.
 *
 * @param parser Parser
 * @param aallow @c ast_abs_allow to allow abstract declarators,
 *        ast_abs_disallow to disallow them
 * @param ridlist Place to store pointer to new declarator list
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_idlist(parser_t *parser, ast_abs_allow_t aallow,
    ast_idlist_t **ridlist)
{
	lexer_toktype_t ltt;
	lexer_tok_t dtok;
	ast_idlist_t *idlist;
	ast_node_t *decl = NULL;
	void *dcomma;
	ast_regassign_t *regassign;
	ast_aslist_t *aslist;
	bool have_init;
	void *dassign;
	ast_node_t *init = NULL;
	int rc;

	rc = ast_idlist_create(&idlist);
	if (rc != EOK)
		goto error;

	dcomma = NULL;
	while (true) {
		parser_read_next_tok(parser, &dtok);

		rc = parser_process_decl(parser, &decl);
		if (rc != EOK)
			goto error;

		if (ast_decl_is_abstract(decl) && aallow != ast_abs_allow) {
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				lexer_dprint_tok(&dtok, stderr);
				fprintf(stderr, " unexpected abstract "
				    "declarator.\n");
			}
			rc = EINVAL;
			goto error;
		}

		/*
		 * XXX Hack so as not to produce false warnings for macro declarators
		 * at the cost of treating declarators that are totally enclosed
		 * in parentheses as not valid C code even if they are.
		 */
		if (decl->ntype == ant_dparen) {
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				lexer_dprint_tok(&dtok, stderr);
				fprintf(stderr, " parenthesized declarator "
				    "(cough).\n");
			}
			rc = EINVAL;
			goto error;
		}

		/* Is there register assignment? */
		ltt = parser_next_ttype(parser);
		if (ltt == ltt_asm) {
			rc = parser_process_regassign(parser, &regassign);
			if (rc != EOK)
				goto error;
		} else {
			regassign = NULL;
		}

		/* Are there any attribute specifiers? */
		ltt = parser_next_ttype(parser);
		if (ltt == ltt_attribute) {
			rc = parser_process_aslist(parser, &aslist);
			if (rc != EOK)
				goto error;
		} else {
			aslist = NULL;
		}

		/* Is there an initialization? */
		ltt = parser_next_ttype(parser);
		if (ltt == ltt_assign) {
			have_init = true;
			parser_skip(parser, &dassign);

			rc = parser_process_init(parser, &init);
			if (rc != EOK)
				goto error;
		} else {
			have_init = false;
			dassign = NULL;
			init = NULL;
		}

		rc = ast_idlist_append(idlist, dcomma, decl, regassign, aslist,
		    have_init, dassign, init);
		if (rc != EOK)
			goto error;

		decl = NULL;
		init = NULL;

		ltt = parser_next_ttype(parser);
		if (ltt != ltt_comma)
			break;

		rc = parser_match(parser, ltt_comma, &dcomma);
		if (rc != EOK)
			goto error;
	}

	*ridlist = idlist;
	return EOK;
error:
	if (idlist != NULL)
		ast_tree_destroy(&idlist->node);
	if (decl != NULL)
		ast_tree_destroy(decl);
	if (init != NULL)
		ast_tree_destroy(init);
	return rc;
}

/** Parse type name.
 *
 * @param parser Parser
 * @param rtypename Place to store pointer to new type name
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_typename(parser_t *parser, ast_typename_t **rtypename)
{
	ast_typename_t *atypename = NULL;
	ast_dspecs_t *dspecs = NULL;
	ast_node_t *decl = NULL;
	int rc;

	rc = ast_typename_create(&atypename);
	if (rc != EOK)
		goto error;

	rc = parser_process_dspecs(parser, 0, NULL, &dspecs);
	if (rc != EOK)
		goto error;

	rc = parser_process_decl(parser, &decl);
	if (rc != EOK)
		goto error;

	atypename->dspecs = dspecs;
	atypename->decl = decl;
	*rtypename = atypename;

	return EOK;
error:
	if (atypename != NULL)
		ast_tree_destroy(&atypename->node);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	if (decl != NULL)
		ast_tree_destroy(decl);
	return rc;
}


/** Parse storage-class specifier.
 *
 * @param parser Parser
 * @param rsclass Place to store storage class
 *
 * @return EOK on success or error code
 */
static int parser_process_sclass(parser_t *parser, ast_sclass_t **rsclass)
{
	lexer_toktype_t ltt;
	ast_sclass_type_t sctype;
	ast_sclass_t *sclass;
	void *dsclass;
	int rc;

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_typedef:
		sctype = asc_typedef;
		break;
	case ltt_extern:
		sctype = asc_extern;
		break;
	case ltt_static:
		sctype = asc_static;
		break;
	case ltt_auto:
		sctype = asc_auto;
		break;
	case ltt_register:
		sctype = asc_register;
		break;
	default:
		sctype = asc_none;
		break;
	}

	if (sctype != asc_none)
		parser_skip(parser, &dsclass);
	else
		dsclass = NULL;

	rc = ast_sclass_create(sctype, &sclass);
	if (rc != EOK)
		return rc;

	sclass->tsclass.data = dsclass;

	*rsclass = sclass;
	return EOK;
}

/** Parse function specifier.
 *
 * @param parser Parser
 * @param rfspec Place to store function specifier
 *
 * @return EOK on success or error code
 */
static int parser_process_fspec(parser_t *parser, ast_fspec_t **rfspec)
{
	lexer_toktype_t ltt;
	ast_fspec_t *fspec;
	void *dfspec;
	int rc;

	ltt = parser_next_ttype(parser);
	assert(ltt == ltt_inline);

	parser_skip(parser, &dfspec);

	rc = ast_fspec_create(&fspec);
	if (rc != EOK)
		return rc;

	fspec->tfspec.data = dfspec;

	*rfspec = fspec;
	return EOK;
}

/** Parse register assignment.
 *
 * @param parser Parser
 * @param rregassign Place to store pointer to new register assignment
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_regassign(parser_t *parser,
    ast_regassign_t **rregassign)
{
	ast_regassign_t *regassign = NULL;
	void *dasm;
	void *dlparen;
	void *dreg;
	void *drparen;
	int rc;

	rc = parser_match(parser, ltt_asm, &dasm);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_strlit, &dreg);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	rc = ast_regassign_create(&regassign);
	if (rc != EOK)
		goto error;

	regassign->tasm.data = dasm;
	regassign->tlparen.data = dlparen;
	regassign->treg.data = dreg;
	regassign->trparen.data = drparen;

	*rregassign = regassign;
	return EOK;
error:
	return rc;
}

/** Parse attribute.
 *
 * @param parser Parser
 * @param rattr Place to store pointer to new attribute
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_aspec_attr(parser_t *parser, ast_aspec_attr_t **rattr)
{
	lexer_toktype_t ltt;
	ast_aspec_attr_t *attr = NULL;
	ast_node_t *expr = NULL;
	void *dname;
	void *dlparen;
	void *dcomma;
	void *drparen;
	int rc;

	rc = ast_aspec_attr_create(&attr);
	if (rc != EOK)
		goto error;

	/* XXX Attribute name can also be a reserved word */
	ltt = parser_next_ttype(parser);
	if (ltt != ltt_ident && !lexer_is_resword(ltt)) {
		fprintf(stderr, "Error: ");
		parser_dprint_next_tok(parser, stderr);
		fprintf(stderr, " unexpected, expected identifier or "
		    "reserved word.\n");

		rc = EINVAL;
		goto error;
	}

	parser_skip(parser, &dname);
	attr->tname.data = dname;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lparen) {
		parser_skip(parser, &dlparen);

		ltt = parser_next_ttype(parser);
		dcomma = NULL;

		/* We can only fail this test upon entry */
		while (ltt != ltt_rparen) {
			rc = parser_process_econcat(parser, &expr);
			if (rc != EOK)
				goto error;

			rc = ast_aspec_attr_append(attr, expr, dcomma);
			if (rc != EOK)
				goto error;

			expr = NULL;

			ltt = parser_next_ttype(parser);
			if (ltt == ltt_rparen)
				break;

			rc = parser_match(parser, ltt_comma, &dcomma);
			if (rc != EOK)
				goto error;
		}

		rc = parser_match(parser, ltt_rparen, &drparen);
		if (rc != EOK)
			goto error;

		attr->have_params = true;
		attr->tlparen.data = dlparen;
		attr->trparen.data = drparen;
	}

	*rattr = attr;
	return EOK;
error:
	if (attr != NULL)
		ast_aspec_attr_destroy(attr);
	if (expr != NULL)
		ast_tree_destroy(expr);
	return rc;
}

/** Parse attribute specifier.
 *
 * @param parser Parser
 * @param raspec Place to store pointer to new attribute specifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_aspec(parser_t *parser, ast_aspec_t **raspec)
{
	lexer_toktype_t ltt;
	ast_aspec_t *aspec = NULL;
	ast_aspec_attr_t *attr = NULL;
	void *dattr;
	void *dlparen1;
	void *dlparen2;
	void *dcomma;
	void *drparen1;
	void *drparen2;
	int rc;

	rc = parser_match(parser, ltt_attribute, &dattr);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen1);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen2);
	if (rc != EOK)
		goto error;

	rc = ast_aspec_create(&aspec);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	dcomma = NULL;

	/* We can only fail this test upon entry */
	while (ltt != ltt_rparen) {
		rc = parser_process_aspec_attr(parser, &attr);
		if (rc != EOK)
			goto error;

		attr->tcomma.data = dcomma;
		ast_aspec_append(aspec, attr);
		attr = NULL;

		ltt = parser_next_ttype(parser);
		if (ltt == ltt_rparen)
			break;

		rc = parser_match(parser, ltt_comma, &dcomma);
		if (rc != EOK)
			goto error;
	}

	rc = parser_match(parser, ltt_rparen, &drparen1);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, &drparen2);
	if (rc != EOK)
		goto error;

	aspec->tattr.data = dattr;
	aspec->tlparen1.data = dlparen1;
	aspec->tlparen2.data = dlparen2;
	aspec->trparen1.data = drparen1;
	aspec->trparen2.data = drparen2;

	*raspec = aspec;
	return EOK;
error:
	if (aspec != NULL)
		ast_tree_destroy(&aspec->node);
	return rc;
}

/** Parse attribute specifier list.
 *
 * @param parser Parser
 * @param raslist Place to store pointer to new AST attribute specifier list
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_aslist(parser_t *parser, ast_aslist_t **raslist)
{
	lexer_toktype_t ltt;
	ast_aslist_t *aslist;
	ast_aspec_t *aspec;
	int rc;

	rc = ast_aslist_create(&aslist);
	if (rc != EOK)
		return rc;

	ltt = parser_next_ttype(parser);
	do {
		if (parser_ttype_aspec(ltt)) {
			/* Attribute specifier */
			rc = parser_process_aspec(parser, &aspec);
			if (rc != EOK)
				goto error;
		} else {
			/* Unexpected */
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				parser_dprint_next_tok(parser, stderr);
				fprintf(stderr, " unexpected, expected "
				    "attribute specifier.\n");
			}
			return EINVAL;
		}

		ast_aslist_append(aslist, aspec);
		ltt = parser_next_ttype(parser);
	} while (parser_ttype_aspec(ltt));

	*raslist = aslist;
	return EOK;
error:
	ast_tree_destroy(&aslist->node);
	return rc;
}

/** Parse macro attribute.
 *
 * @param parser Parser
 * @param rmattr Place to store pointer to new macro attribute
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_mattr(parser_t *parser, ast_mattr_t **rmattr)
{
	lexer_toktype_t ltt;
	ast_mattr_t *mattr = NULL;
	void *dname;
	void *dlparen;
	ast_node_t *expr = NULL;
	void *dcomma;
	void *drparen;
	int rc;

	rc = ast_mattr_create(&mattr);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_ident, &dname);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lparen) {

		rc = parser_match(parser, ltt_lparen, &dlparen);
		if (rc != EOK)
			goto error;

		ltt = parser_next_ttype(parser);
		dcomma = NULL;

		/* We can only fail this test upon entry */
		while (ltt != ltt_rparen) {
			rc = parser_process_econcat(parser, &expr);
			if (rc != EOK)
				goto error;

			ltt = parser_next_ttype(parser);
			if (ltt == ltt_comma) {
				parser_skip(parser, &dcomma);
			} else {
				dcomma = NULL;
			}

			rc = ast_mattr_append(mattr, expr, dcomma);
			if (rc != EOK)
				goto error;

			expr = NULL;

			if (ltt != ltt_comma)
				break;
		}

		rc = parser_match(parser, ltt_rparen, &drparen);
		if (rc != EOK)
			goto error;

		mattr->have_params = true;
		mattr->tlparen.data = dlparen;
		mattr->trparen.data = drparen;
	} else {
		mattr->have_params = false;
	}

	mattr->tname.data = dname;

	*rmattr = mattr;
	return EOK;
error:
	if (mattr != NULL)
		ast_tree_destroy(&mattr->node);
	if (expr != NULL)
		ast_tree_destroy(expr);
	return rc;
}

/** Parse macro attribute list.
 *
 * @param parser Parser
 * @param rmalist Place to store pointer to new AST attribute specifier list
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_malist(parser_t *parser, ast_malist_t **rmalist)
{
	lexer_toktype_t ltt;
	ast_malist_t *malist;
	ast_mattr_t *mattr;
	int rc;

	rc = ast_malist_create(&malist);
	if (rc != EOK)
		return rc;

	do {
		rc = parser_process_mattr(parser, &mattr);
		if (rc != EOK)
			goto error;

		ast_malist_append(malist, mattr);
		ltt = parser_next_ttype(parser);
	} while (ltt == ltt_ident);

	*rmalist = malist;
	return EOK;
error:
	ast_tree_destroy(&malist->node);
	return rc;
}

/** Parse global declaration.
 *
 * @param parser Parser
 * @param rnode Place to store pointer to new declaration node
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_gdecln(parser_t *parser, ast_node_t **rnode)
{
	lexer_toktype_t ltt;
	ast_gdecln_t *gdecln = NULL;
	ast_dspecs_t *dspecs = NULL;
	ast_idlist_t *idlist = NULL;
	ast_idlist_entry_t *entry;
	bool more_decls;
	ast_malist_t *malist = NULL;
	ast_block_t *body = NULL;
	bool have_scolon;
	bool have_realdecl;
	unsigned add_idents;
	bool more_idents;
	void *dscolon;
	void *old_tok;
	int rc;

	add_idents = 0;
	old_tok = parser->tok;
again:
	parser->tok = old_tok;

	rc = parser_process_dspecs(parser, add_idents, &more_idents, &dspecs);
	if (rc != EOK)
		goto error;

	rc = parser_process_idlist(parser, ast_abs_allow, &idlist);
	if (rc != EOK)
		goto error;

	/* See if we have more than one declarator */
	entry = ast_idlist_first(idlist);
	assert(entry != NULL);
	more_decls = ast_idlist_next(entry) != NULL;

	if (!more_decls)
		have_realdecl = entry->decl->ntype != ant_dident;
	else
		have_realdecl = false;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_ident && have_realdecl) {
		rc = parser_process_malist(parser, &malist);
		if (rc != EOK)
			goto error;
	}

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_scolon:
		body = NULL;
		parser_skip(parser, &dscolon);
		have_scolon = true;
		break;
	case ltt_lbrace:
		if (more_decls) {
			if (!parser->silent) {
				fprintf(stderr, "Error: ");
				parser_dprint_next_tok(parser, stderr);
				fprintf(stderr, " '{' unexpected, "
				    "expected ';'.\n");
			}
			rc = EINVAL;
			goto error;
		}

		rc = parser_process_block(parser, &body);
		if (rc != EOK)
			goto error;

		dscolon = NULL;
		have_scolon = false;
		break;
	default:
		if (more_idents) {
			++add_idents;
			goto again;
		}

		if (!parser->silent) {
			fprintf(stderr, "Error: ");
			parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " unexpected, expected '{' or ';'.\n");
		}

		rc = EINVAL;
		goto error;
	}

	rc = ast_gdecln_create(dspecs, idlist, malist, body, &gdecln);
	if (rc != EOK)
		goto error;

	if (have_scolon) {
		gdecln->have_scolon = true;
		gdecln->tscolon.data = dscolon;
	}

	*rnode = &gdecln->node;
	return EOK;
error:
	if (gdecln != NULL)
		ast_tree_destroy(&gdecln->node);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	if (idlist != NULL)
		ast_tree_destroy(&idlist->node);
	if (malist != NULL)
		ast_tree_destroy(&malist->node);
	if (body != NULL)
		ast_tree_destroy(&body->node);
	return rc;
}

/** Parse macro-based declaration.
 *
 * @param parser Parser
 * @param rmdecln Place to store pointer to new macro-based declaration
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_mdecln(parser_t *parser, ast_mdecln_t **rmdecln)
{
	lexer_toktype_t ltt;
	ast_mdecln_t *mdecln = NULL;
	ast_dspecs_t *dspecs = NULL;
	void *dlparen;
	ast_node_t *expr = NULL;
	void *dcomma;
	void *drparen;
	int rc;

	rc = ast_mdecln_create(&mdecln);
	if (rc != EOK)
		goto error;

	rc = parser_process_dspecs(parser, 0, NULL, &dspecs);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, &dlparen);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_rparen) {
		do {
			rc = parser_process_econcat(parser, &expr);
			if (rc != EOK)
				goto error;

			ltt = parser_next_ttype(parser);

			if (ltt == ltt_comma) {
				rc = parser_match(parser, ltt_comma, &dcomma);
				if (rc != EOK)
					goto error;
			} else {
				dcomma = NULL;
			}

			rc = ast_mdecln_append(mdecln, expr, dcomma);
			if (rc != EOK)
				goto error;

			expr = NULL;

		} while (ltt == ltt_comma);
	}

	rc = parser_match(parser, ltt_rparen, &drparen);
	if (rc != EOK)
		goto error;

	mdecln->dspecs = dspecs;
	dspecs = NULL;

	/* XXX Dig out macro name from dspecs */
	mdecln->tname.data = NULL;
	mdecln->tlparen.data = dlparen;
	mdecln->trparen.data = drparen;

	*rmdecln = mdecln;
	return EOK;
error:
	if (expr != NULL)
		ast_tree_destroy(expr);
	if (mdecln != NULL)
		ast_tree_destroy(&mdecln->node);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	return rc;
}

/** Parse global macro-based declaration.
 *
 * @param parser Parser
 * @param rgmdecln Place to store pointer to new macro-based declaration
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_gmdecln(parser_t *parser, ast_gmdecln_t **rgmdecln)
{
	lexer_toktype_t ltt;
	ast_gmdecln_t *gmdecln = NULL;
	ast_mdecln_t *mdecln = NULL;
	ast_dspecs_t *dspecs = NULL;
	ast_block_t *body;
	bool have_scolon;
	void *dscolon;
	int rc;

	rc = ast_gmdecln_create(&gmdecln);
	if (rc != EOK)
		goto error;

	rc = parser_process_mdecln(parser, &mdecln);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_scolon:
		body = NULL;
		parser_skip(parser, &dscolon);
		have_scolon = true;
		break;
	case ltt_lbrace:
		rc = parser_process_block(parser, &body);
		if (rc != EOK)
			goto error;
		dscolon = NULL;
		have_scolon = false;
		break;
	default:
		if (!parser->silent) {
			fprintf(stderr, "Error: ");
			parser_dprint_next_tok(parser, stderr);
			fprintf(stderr, " unexpected, expected '{' or ';'.\n");
		}
		rc = EINVAL;
		goto error;
	}

	gmdecln->mdecln = mdecln;

	gmdecln->body = body;
	gmdecln->have_scolon = have_scolon;
	if (have_scolon)
		gmdecln->tscolon.data = dscolon;

	*rgmdecln = gmdecln;
	return EOK;
error:
	if (mdecln != NULL)
		ast_tree_destroy(&mdecln->node);
	if (gmdecln != NULL)
		ast_tree_destroy(&gmdecln->node);
	if (dspecs != NULL)
		ast_tree_destroy(&dspecs->node);
	return rc;
}

/** Parse module.
 *
 * @param parser Parser
 * @param rmodule Place to store pointer to new module
 *
 * @return EOK on success or non-zero error code
 */
int parser_process_module(parser_t *parser, ast_module_t **rmodule)
{
	lexer_toktype_t ltt;
	ast_module_t *module;
	ast_node_t *decln = NULL;
	ast_node_t *node;
	ast_gmdecln_t *gmdecln = NULL;
	parser_t *sparser;
	int rc;

	(void)parser;

	rc = ast_module_create(&module);
	if (rc != EOK)
		return rc;

	ltt = parser_next_ttype(parser);
	while (ltt != ltt_eof) {
		rc = parser_create_silent_sub(parser, &sparser);
		if (rc != EOK)
			goto error;

		rc = parser_process_gmdecln(sparser, &gmdecln);
		if (rc == EOK) {
			parser->tok = sparser->tok;
			parser_destroy(sparser);
			node = &gmdecln->node;
		} else {
			parser_destroy(sparser);

			rc = parser_process_gdecln(parser, &decln);
			if (rc != EOK)
				goto error;

			node = decln;
		}

		ast_module_append(module, node);
		ltt = parser_next_ttype(parser);
	}

	*rmodule = module;
	return EOK;
error:
	ast_tree_destroy(&module->node);
	return rc;
}
