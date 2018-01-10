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

static int parser_process_tspec(parser_t *, ast_node_t **);
static int parser_process_decl(parser_t *, ast_node_t **);
static int parser_process_dlist(parser_t *, ast_dlist_t **);

/** Create parser.
 *
 * @param ops Parser input ops
 * @param arg Argument to input ops
 * @param rparser Place to store pointer to new parser
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int parser_create(parser_input_ops_t *ops, void *arg, parser_t **rparser)
{
	parser_t *parser;

	parser = calloc(1, sizeof(parser_t));
	if (parser == NULL)
		return ENOMEM;

	parser->input_ops = ops;
	parser->input_arg = arg;
	*rparser = parser;
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
static bool parser_ttype_ignore(lexer_toktype_t ttype)
{
	return ttype == ltt_wspace || ttype == ltt_comment ||
	    ttype == ltt_dscomment || ttype == ltt_preproc;
}

/** Fill lookahead buffer up to the specified number of tokens.
 *
 * @param parser Parser
 * @param i Number of tokens required in lookahead buffer
 *          (not greater than @c parser_lookahead)
 */
static void parser_look_ahead(parser_t *parser, size_t i)
{
	assert(i <= parser_lookahead);

	while (parser->tokcnt < i) {
		/* Need to skip whitespace */
		do {
			parser->input_ops->get_tok(parser->input_arg,
			    &parser->tok[parser->tokcnt]);
		} while (parser_ttype_ignore(parser->tok[parser->tokcnt].ttype));

		++parser->tokcnt;
	}
}

/** Return type of next token.
 *
 * @param parser Parser
 * @return Type of next token being parsed
 */
static lexer_toktype_t parser_next_ttype(parser_t *parser)
{
	parser_look_ahead(parser, 1);
	return parser->tok[0].ttype;
}

static void *parser_get_tok_data(parser_t *parser, lexer_tok_t *tok)
{
	return parser->input_ops->tok_data(parser->input_arg, tok);
}

/** Skip over current token.
 *
 * @param parser Parser
 * @param rdata Place to store user data for token or @c NULL if not interested
 */
static void parser_skip(parser_t *parser, void **rdata)
{
	size_t i;

	/* We should never skip a token without looking at it first */
	assert(parser->tokcnt > 0);

	if (rdata != NULL)
		*rdata = parser_get_tok_data(parser, &parser->tok[0]);

	for (i = 1; i < parser->tokcnt; i++)
		parser->tok[i - 1] = parser->tok[i];

	--parser->tokcnt;
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
		fprintf(stderr, "Error: ");
		lexer_dprint_tok(&parser->tok[0], stderr);
		fprintf(stderr, " unexpected, expected '%s'.\n",
		    lexer_str_ttype(mtype));
		return EINVAL;
	}

	parser_skip(parser, rdata);
	return EOK;
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
	ast_return_t *areturn;
	void *dreturn;
	void *dscolon;
	int rc;

	rc = parser_match(parser, ltt_return, &dreturn);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_number, NULL);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		return rc;

	rc = ast_return_create(&areturn);
	if (rc != EOK)
		return rc;

	areturn->treturn.data = dreturn;
	areturn->tscolon.data = dscolon;
	*rstmt = &areturn->node;
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
				return rc;

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
			return rc;

		ast_block_append(block, stmt);
	}

	*rblock = block;
	return EOK;
}

/** Parse builtin type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST type specifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsbuiltin(parser_t *parser, ast_node_t **rtype)
{
	ast_tsbuiltin_t *pbuiltin;
	lexer_toktype_t ltt;
	bool done;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_const)
		parser_skip(parser, NULL);

	done = false;
	while (!done) {
		ltt = parser_next_ttype(parser);
		switch (ltt) {
		case ltt_char:
		case ltt_double:
		case ltt_float:
		case ltt_int:
		case ltt_long:
		case ltt_short:
		case ltt_void:
		case ltt_signed:
		case ltt_unsigned:
			break;
		default:
			done = true;
			break;
		}

		if (done)
			break;

		parser_skip(parser, NULL);
	}

	rc = ast_tsbuiltin_create(&pbuiltin);
	if (rc != EOK)
		return rc;

	*rtype = &pbuiltin->node;
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
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_const)
		parser_skip(parser, NULL);

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_ident:
		break;
	default:
		fprintf(stderr, "Error: ");
		lexer_dprint_tok(&parser->tok[0], stderr);
		fprintf(stderr, " unexpected, expected type identifer.\n");
		return EINVAL;
	}

	parser_skip(parser, NULL);

	rc = ast_tsident_create(&pident);
	if (rc != EOK)
		return rc;

	*rtype = &pident->node;
	return EOK;
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
	ast_node_t *tspec;
	ast_dlist_t *dlist;
	void *dscolon;
	void *drbrace;
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
	if (ltt == ltt_ident) {
		parser_skip(parser, &dident);
		precord->tident.data = dident;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lbrace) {
		parser_skip(parser, &dlbrace);

		precord->tlbrace.data = dlbrace;

		ltt = parser_next_ttype(parser);
		while (ltt != ltt_rbrace) {
			rc = parser_process_tspec(parser, &tspec);
			if (rc != EOK)
				goto error;

			rc = parser_process_dlist(parser, &dlist);
			if (rc != EOK)
				goto error;

			rc = parser_match(parser, ltt_scolon, &dscolon);
			if (rc != EOK)
				goto error;

			rc = ast_tsrecord_append(precord, tspec, dlist, dscolon);
			if (rc != EOK)
				goto error;

			ltt = parser_next_ttype(parser);
		}

		rc = parser_match(parser, ltt_rbrace, &drbrace);
		if (rc != EOK)
			goto error;

		precord->trbrace.data = drbrace;
	}

	*rtype = &precord->node;
	return EOK;
error:
	if (precord != NULL)
		ast_tree_destroy(&precord->node);
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
	void *dinit;
	void *dcomma;
	void *drbrace;
	int rc;

	rc = ast_tsenum_create(&penum);
	if (rc != EOK)
		return rc;

	rc = parser_match(parser, ltt_enum, &denum);
	if (rc != EOK)
		return rc;

	penum->tenum.data = denum;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_ident) {
		parser_skip(parser, &dident);
		penum->tident.data = dident;
	}

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_lbrace) {
		parser_skip(parser, &dlbrace);

		penum->tlbrace.data = dlbrace;

		ltt = parser_next_ttype(parser);
		while (ltt != ltt_rbrace) {
			rc = parser_match(parser, ltt_ident, &delem);
			if (rc != EOK)
				goto error;

			ltt = parser_next_ttype(parser);
			if (ltt == ltt_equals) {
				rc = parser_match(parser, ltt_equals, &dequals);
				if (rc != EOK)
					goto error;

				ltt = parser_next_ttype(parser);
				if (ltt == ltt_ident || ltt == ltt_number) {
					parser_skip(parser, &dinit);
				} else {
					fprintf(stderr, "Error: ");
					lexer_dprint_tok(&parser->tok[0],
					    stderr);
					fprintf(stderr, " unexpected, expected"
					    " number or identifier.\n");
					rc = EINVAL;
					goto error;
				}
			} else {
				dequals = NULL;
				dinit = NULL;
			}

			ltt = parser_next_ttype(parser);
			if (ltt == ltt_comma)
				parser_skip(parser, &dcomma);
			else
				dcomma = NULL;

			rc = ast_tsenum_append(penum, delem, dequals, dinit,
			    dcomma);
			if (rc != EOK)
				goto error;

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
	return rc;
}

/** Parse primitive type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST type
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tsprim(parser_t *parser, ast_node_t **rtype)
{
	lexer_toktype_t ltt;
	int rc;

	ltt = parser_next_ttype(parser);
	if (ltt == ltt_const)
		parser_skip(parser, NULL);

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
		rc = parser_process_tsbuiltin(parser, rtype);
		break;
	}

	return rc;
}

/** Parse type specifier.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST type specifier
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_tspec(parser_t *parser, ast_node_t **rtype)
{
	return parser_process_tsprim(parser, rtype);
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
	void *dsize;
	void *drbracket;
	int rc;

	rc = ast_darray_create(&darray);
	if (rc != EOK)
		goto error;

	rc = parser_process_dparen(parser, &bdecl);
	if (rc != EOK)
		goto error;

	darray->bdecl = bdecl;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_lbracket) {
		*rdecl = bdecl;
		return EOK;
	}

	parser_skip(parser, &dlbracket);
	darray->tlbracket.data = dlbracket;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_ident && ltt != ltt_number) {
		fprintf(stderr, "Error: ");
		lexer_dprint_tok(&parser->tok[0], stderr);
		fprintf(stderr, " unexpected, expected number or identifier.\n");
		return EINVAL;
	}

	parser_skip(parser, &dsize);
	darray->tsize.data = dsize;

	rc = parser_match(parser, ltt_rbracket, &drbracket);
	if (rc != EOK)
		goto error;

	darray->trbracket.data = drbracket;

	*rdecl = &darray->node;
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
	ast_node_t *tspec;
	ast_node_t *decl;
	void *dcomma;
	void *drparen;
	int rc;

	rc = ast_dfun_create(&dfun);
	if (rc != EOK)
		goto error;

	rc = parser_process_darray(parser, &bdecl);
	if (rc != EOK)
		goto error;

	dfun->bdecl = bdecl;

	ltt = parser_next_ttype(parser);
	if (ltt != ltt_lparen) {
		*rdecl = bdecl;
		return EOK;
	}

	parser_skip(parser, &dlparen);
	dfun->tlparen.data = dlparen;

	/* Parse arguments */
	ltt = parser_next_ttype(parser);
	if (ltt != ltt_rparen) {
		do {
			rc = parser_process_tspec(parser, &tspec);
			if (rc != EOK)
				goto error;

			rc = parser_process_decl(parser, &decl);
			if (rc != EOK) {
				ast_tree_destroy(tspec);
				goto error;
			}

			ltt = parser_next_ttype(parser);
			if (ltt != ltt_rparen) {
				rc = parser_match(parser, ltt_comma, &dcomma);
				if (rc != EOK)
					goto error;
			} else {
				dcomma = NULL;
			}

			rc = ast_dfun_append(dfun, tspec, decl, dcomma);
			if (rc != EOK)
				goto error;
		} while (ltt != ltt_rparen);
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
	ast_dptr_t *dptr;
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

	rc = parser_process_decl(parser, &bdecl);
	if (rc != EOK) {
		ast_tree_destroy(&dptr->node);
		return rc;
	}

	dptr->bdecl = bdecl;
	*rdecl = &dptr->node;
	return EOK;
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
 * @param rdlist Place to store pointer to new declarator list
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_dlist(parser_t *parser, ast_dlist_t **rdlist)
{
	lexer_toktype_t ltt;
	ast_dlist_t *dlist;
	ast_node_t *decl = NULL;
	void *dcomma;
	int rc;

	rc = ast_dlist_create(&dlist);
	if (rc != EOK)
		goto error;

	rc = parser_process_decl(parser, &decl);
	if (rc != EOK)
		goto error;

	rc = ast_dlist_append(dlist, NULL, decl);
	if (rc != EOK)
		goto error;

	ltt = parser_next_ttype(parser);
	while (ltt == ltt_comma) {
		rc = parser_match(parser, ltt_comma, &dcomma);
		if (rc != EOK)
			goto error;

		rc = parser_process_decl(parser, &decl);
		if (rc != EOK)
			goto error;

		rc = ast_dlist_append(dlist, dcomma, decl);
		if (rc != EOK)
			goto error;

		decl = NULL;

		ltt = parser_next_ttype(parser);
	}

	*rdlist = dlist;
	return EOK;
error:
	if (dlist != NULL)
		ast_tree_destroy(&dlist->node);
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

	rc = ast_sclass_create(sctype, &sclass);
	if (rc != EOK)
		return rc;

	sclass->tsclass.data = dsclass;

	*rsclass = sclass;
	return EOK;
}

/** Parse function definition.
 *
 * @param parser Parser
 * @param ftspec Function type specifier
 * @param fdecl  Function declarator
 * @param rnode Place to store pointer to new function definition
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_fundef(parser_t *parser, ast_node_t *ftspec,
    ast_node_t *fdecl, ast_fundef_t **rfundef)
{
	lexer_toktype_t ltt;
	ast_fundef_t *fundef;
	ast_block_t *body;
	void *dscolon;
	int rc;

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_scolon:
		body = NULL;
		parser_skip(parser, &dscolon);
		break;
	case ltt_lbrace:
		rc = parser_process_block(parser, &body);
		if (rc != EOK)
			return rc;
		break;
	default:
		fprintf(stderr, "Error: ");
		lexer_dprint_tok(&parser->tok[0], stderr);
		fprintf(stderr, " unexpected, expected '{' or ';'.\n");
		return EINVAL;
	}

	rc = ast_fundef_create(ftspec, fdecl, body, &fundef);
	if (rc != EOK) {
		ast_tree_destroy(&body->node);
		return rc;
	}

	if (body == NULL)
		fundef->tscolon.data = dscolon;

	*rfundef = fundef;
	return EOK;
}

/** Parse typedef.
 *
 * @param parser Parser
 * @param rnode Place to store pointer to new typedef node
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_typedef(parser_t *parser, ast_node_t **rnode)
{
	ast_typedef_t *atypedef = NULL;
	void *dtypedef;
	ast_node_t *tspec = NULL;
	ast_dlist_t *dlist = NULL;
	void *dscolon;
	int rc;

	parser_skip(parser, &dtypedef);

	rc = parser_process_tspec(parser, &tspec);
	if (rc != EOK)
		goto error;

	rc = parser_process_dlist(parser, &dlist);
	if (rc != EOK)
		goto error;

	rc = ast_typedef_create(tspec, dlist, &atypedef);
	if (rc != EOK)
		goto error;

	tspec = NULL;
	dlist = NULL;

	rc = parser_match(parser, ltt_scolon, &dscolon);
	if (rc != EOK)
		goto error;

	atypedef->ttypedef.data = dtypedef;
	atypedef->tscolon.data = dscolon;

	*rnode = &atypedef->node;
	return EOK;
error:
	if (tspec != NULL)
		ast_tree_destroy(tspec);
	if (dlist != NULL)
		ast_tree_destroy(&dlist->node);
	if (atypedef != NULL)
		ast_tree_destroy(&atypedef->node);
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
	ast_fundef_t *fundef;
	ast_node_t *rtspec = NULL;
	ast_node_t *fdecl = NULL;
	ast_sclass_t *sclass;
	int rc;

	rc = parser_process_sclass(parser, &sclass);
	if (rc != EOK)
		goto error;

	rc = parser_process_tspec(parser, &rtspec);
	if (rc != EOK)
		goto error;

	rc = parser_process_decl(parser, &fdecl);
	if (rc != EOK)
		goto error;

	rc = parser_process_fundef(parser, rtspec, fdecl, &fundef);
	if (rc != EOK)
		goto error;

	*rnode = &fundef->node;
	return EOK;
error:
	if (rtspec != NULL)
		ast_tree_destroy(rtspec);
	if (fdecl != NULL)
		ast_tree_destroy(fdecl);
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
	ast_node_t *decln;
	int rc;

	(void)parser;

	rc = ast_module_create(&module);
	if (rc != EOK)
		return rc;

	ltt = parser_next_ttype(parser);
	while (ltt != ltt_eof) {
		switch (ltt) {
		case ltt_typedef:
			rc = parser_process_typedef(parser, &decln);
			break;
		default:
			rc = parser_process_gdecln(parser, &decln);
			break;
		}

		if (rc != EOK)
			return rc;

		ast_module_append(module, decln);
		ltt = parser_next_ttype(parser);
	}

	*rmodule = module;
	return EOK;
}
