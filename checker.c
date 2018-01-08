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
 * Checker
 */

#include <adt/list.h>
#include <assert.h>
#include <ast.h>
#include <checker.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
#include <stdbool.h>
#include <stdlib.h>

static void checker_parser_get_tok(void *, lexer_tok_t *);
static void *checker_parser_tok_data(void *, lexer_tok_t *);
static int checker_module_check_decl(checker_module_t *, ast_node_t *);
static int checker_module_check_tspec(checker_module_t *, ast_node_t *);

static parser_input_ops_t checker_parser_input = {
	.get_tok = checker_parser_get_tok,
	.tok_data = checker_parser_tok_data
};

/** Create checker module.
 *
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param rmodule Place to store new checker module.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int checker_module_create(checker_module_t **rmodule)
{
	checker_module_t *module = NULL;

	module = calloc(1, sizeof(checker_module_t));
	if (module == NULL)
		return ENOMEM;

	list_initialize(&module->toks);

	*rmodule = module;
	return EOK;
}

/** Destroy checker module.
 *
 * @param checker Checker
 */
static void checker_module_destroy(checker_module_t *module)
{
	free(module);
}

/** Append a token to checker module.
 *
 * @param module Checker module
 * @param tok Lexer token
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int checker_module_append(checker_module_t *module, lexer_tok_t *tok)
{
	checker_tok_t *ctok;

	ctok = calloc(1, sizeof(checker_tok_t));
	if (ctok == NULL)
		return ENOMEM;

	ctok->mod = module;
	ctok->tok = *tok;
	list_append(&ctok->ltoks, &module->toks);
	return EOK;
}

/** Create checker.
 *
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param rchecker Place to store new checker.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int checker_create(lexer_input_ops_t *input_ops, void *input_arg,
    checker_t **rchecker)
{
	checker_t *checker = NULL;
	lexer_t *lexer = NULL;
	int rc;

	checker = calloc(1, sizeof(checker_t));
	if (checker == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = lexer_create(input_ops, input_arg, &lexer);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		goto error;
	}

	checker->lexer = lexer;
	*rchecker = checker;
	return EOK;
error:
	if (lexer != NULL)
		lexer_destroy(lexer);
	if (checker != NULL)
		free(checker);
	return rc;
}

/** Destroy checker.
 *
 * @param checker Checker
 */
void checker_destroy(checker_t *checker)
{
	checker_module_destroy(checker->mod);
	lexer_destroy(checker->lexer);
	free(checker);
}

/** Lex a module.
 *
 * @param checker Checker
 * @param rmodule Place to store pointer to new module
 * @return EOK on success, or error code
 */
static int checker_module_lex(checker_t *checker, checker_module_t **rmodule)
{
	checker_module_t *module = NULL;
	bool done;
	lexer_tok_t tok;
	int rc;

	rc = checker_module_create(&module);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		goto error;
	}

	done = false;
	while (!done) {
		rc = lexer_get_tok(checker->lexer, &tok);
		if (rc != EOK)
			return rc;

		rc = checker_module_append(module, &tok);
		if (rc != EOK) {
			lexer_free_tok(&tok);
			return rc;
		}

		if (tok.ttype == ltt_eof)
			done = true;
	}

	*rmodule = module;
	return EOK;
error:
	return rc;
}

/** Get first token in a checker module.
 *
 * @param mod Checker module
 * @return First token or @c NULL if the list is empty
 */
static checker_tok_t *checker_module_first_tok(checker_module_t *mod)
{
	link_t *link;

	link = list_first(&mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, checker_tok_t, ltoks);
}

/** Get next token in a checker module
 *
 * @param tok Current token or @c NULL
 * @return Next token or @c NULL
 */
static checker_tok_t *checker_module_next_tok(checker_tok_t *tok)
{
	link_t *link;

	if (tok == NULL)
		return NULL;

	link = list_next(&tok->ltoks, &tok->mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, checker_tok_t, ltoks);
}

/** Get previous token in a checker module
 *
 * @param tok Current token or @c NULL
 * @return Previous token or @c NULL
 */
static checker_tok_t *checker_module_prev_tok(checker_tok_t *tok)
{
	link_t *link;

	if (tok == NULL)
		return NULL;

	link = list_prev(&tok->ltoks, &tok->mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, checker_tok_t, ltoks);
}

/** Check that a token is at the beginning of the line.
 *
 * The token must be at the beginning of the line and indented appropriately
 */
static void checker_module_check_lbegin(checker_tok_t *tok, const char *msg)
{
	checker_tok_t *p;

	p = checker_module_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype != ltt_wspace) {
		lexer_dprint_tok(&tok->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Non-whitespace before.
 *
 * There should be non-whitespace before the token
 */
static void checker_module_check_nows_before(checker_tok_t *tok,
    const char *msg)
{
	checker_tok_t *p;

	assert(tok != NULL);
	p = checker_module_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype == ltt_wspace) {
		lexer_dprint_tok(&p->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Non-whitespace after.
 *
 * There should be non-whitespace after the token
 */
static void checker_module_check_nows_after(checker_tok_t *tok,
    const char *msg)
{
	checker_tok_t *p;

	assert(tok != NULL);
	p = checker_module_next_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype == ltt_wspace) {
		lexer_dprint_tok(&p->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Non-spacing break.
 *
 * There should be either non-whitespace or a line break before the token. */
#if 0
static void checker_module_check_nsbrk_before(checker_tok_t *tok,
    const char *msg)
{
	(void)tok; (void)msg;
}
#endif

/** Breakable space.
 *
 * There should be either a single space or a line break before the token.
 * If there is a line break, the token must be indented appropriately.
 */
#if 0
static void checker_module_check_brkspace_before(checker_tok_t *tok,
    const char *msg)
{
	(void)tok; (void)msg;
}
#endif

/** Non-breakable space before.
 *
 * There should be a single space before the token
 */
static void checker_module_check_nbspace_before(checker_tok_t *tok,
    const char *msg)
{
	checker_tok_t *p;

	assert(tok != NULL);
	p = checker_module_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype != ltt_wspace) {
		lexer_dprint_tok(&p->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Parse a module.
 *
 * @param mod Checker module
 * @return EOK on success or error code
 */
static int checker_module_parse(checker_module_t *mod)
{
	ast_module_t *amod;
	parser_t *parser;
	checker_parser_input_t pinput;
	int rc;

	pinput.tok = checker_module_first_tok(mod);
	rc = parser_create(&checker_parser_input, &pinput, &parser);
	if (rc != EOK)
		return rc;

	rc = parser_process_module(parser, &amod);
	if (rc != EOK)
		return rc;


	if (0) {
		putchar('\n');
		rc = ast_tree_print(&amod->node, stdout);
		if (rc != EOK)
			return rc;
		putchar('\n');
	}


	mod->ast = amod;
	parser_destroy(parser);

	return EOK;
}

/** Run checks on a statement.
 *
 * @param mod Checker module
 * @param stmt AST statement
 * @return EOK on success or error code
 */
static int checker_module_check_stmt(checker_module_t *mod, ast_node_t *stmt)
{
	checker_tok_t *treturn;
	checker_tok_t *tscolon;
	ast_return_t *areturn;

	(void)mod;

	assert(stmt->ntype == ant_return);
	areturn = (ast_return_t *)stmt->ext;
	treturn = (checker_tok_t *)areturn->treturn.data;
	tscolon = (checker_tok_t *)areturn->tscolon.data;

	checker_module_check_lbegin(treturn,
	    "Statement must start on a new line.");
	checker_module_check_nows_before(tscolon,
	    "Unexpected whitespace before ';'.");

	return EOK;
}

/** Run checks on a parenthesized declarator.
 *
 * @param mod Checker module
 * @param dparen AST parenthesized declarator
 * @return EOK on success or error code
 */
static int checker_module_check_dparen(checker_module_t *mod,
    ast_dparen_t *dparen)
{
	checker_tok_t *tlparen;
	checker_tok_t *trparen;

	tlparen = (checker_tok_t *)dparen->tlparen.data;
	checker_module_check_nows_after(tlparen,
	    "Unexpected whitespace after '('.");

	trparen = (checker_tok_t *)dparen->trparen.data;
	checker_module_check_nows_before(trparen,
	    "Unexpected whitespace before ')'.");

	return checker_module_check_decl(mod, dparen->bdecl);
}

/** Run checks on a pointer declarator.
 *
 * @param mod Checker module
 * @param dptr AST pointer declarator
 * @return EOK on success or error code
 */
static int checker_module_check_dptr(checker_module_t *mod, ast_dptr_t *dptr)
{
	checker_tok_t *tasterisk;

	tasterisk = (checker_tok_t *)dptr->tasterisk.data;
	checker_module_check_nows_after(tasterisk,
	    "Unexpected whitespace after '*'.");

	return checker_module_check_decl(mod, dptr->bdecl);
}

/** Run checks on a declarator.
 *
 * @param mod Checker module
 * @param decl AST declarator
 * @return EOK on success or error code
 */
static int checker_module_check_decl(checker_module_t *mod, ast_node_t *decl)
{
	int rc;

	switch (decl->ntype) {
	case ant_dnoident:
	case ant_dident:
		rc = EOK;
		break;
	case ant_dparen:
		rc = checker_module_check_dparen(mod, (ast_dparen_t *)decl->ext);
		break;
	case ant_dptr:
		rc = checker_module_check_dptr(mod, (ast_dptr_t *)decl->ext);
		break;
	default:
		assert(false);
		rc = EOK;
		break;
	}

	return rc;
}

/** Run checks on a record type specifier.
 *
 * @param mod Checker module
 * @param tsrecord AST record type specifier
 * @return EOK on success or error code
 */
static int checker_module_check_tsrecord(checker_module_t *mod,
    ast_tsrecord_t *tsrecord)
{
	ast_tsrecord_elem_t *elem;
	checker_tok_t *tscolon;
	int rc;

	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		rc = checker_module_check_tspec(mod, elem->tspec);
		if (rc != EOK)
			return rc;

		rc = checker_module_check_decl(mod, elem->decl);
		if (rc != EOK)
			return rc;

		tscolon = (checker_tok_t *)elem->tscolon.data;
		checker_module_check_nows_before(tscolon,
		    "Unexpected whitespace before ';'.");

		elem = ast_tsrecord_next(elem);
	}

	return EOK;
}

/** Run checks on an enum type specifier.
 *
 * @param mod Checker module
 * @param tsenum AST enum type specifier
 * @return EOK on success or error code
 */
static int checker_module_check_tsenum(checker_module_t *mod,
    ast_tsenum_t *tsenum)
{
	ast_tsenum_elem_t *elem;
	checker_tok_t *tlbrace;
	checker_tok_t *telem;
	checker_tok_t *tequals;
	checker_tok_t *tinit;
	checker_tok_t *tcomma;
	checker_tok_t *trbrace;

	(void) mod;

	tlbrace = (checker_tok_t *)tsenum->tlbrace.data;
	if (tlbrace != NULL) {
		checker_module_check_nbspace_before(tlbrace,
		    "Expected single space before '{'.");
	}

	elem = ast_tsenum_first(tsenum);
	while (elem != NULL) {
		telem = (checker_tok_t *)elem->tident.data;
		checker_module_check_lbegin(telem,
		    "Enum field must begin on a new line.");

		tequals = (checker_tok_t *)elem->tequals.data;
		if (tequals != NULL) {
			checker_module_check_nbspace_before(tequals,
			    "Expected space before '='.");

			tinit = (checker_tok_t *)elem->tinit.data;
			checker_module_check_nbspace_before(tinit,
			    "Expected whitespace before initializer.");
		}

		tcomma = (checker_tok_t *)elem->tcomma.data;
		if (tcomma != NULL) {
			checker_module_check_nows_before(tcomma,
			    "Unexpected whitespace before ','.");
		}

		elem = ast_tsenum_next(elem);
	}

	trbrace = (checker_tok_t *)tsenum->trbrace.data;
	if (trbrace != NULL) {
		checker_module_check_lbegin(tlbrace,
		    "'{' must begin on a new line.");
	}

	return EOK;
}

/** Run checks on a type specifier.
 *
 * @param mod Checker module
 * @param tspec AST type specifier
 * @return EOK on success or error code
 */
static int checker_module_check_tspec(checker_module_t *mod, ast_node_t *tspec)
{
	int rc;

	switch (tspec->ntype) {
	case ant_tsbuiltin:
	case ant_tsident:
		rc = EOK;
		break;
	case ant_tsrecord:
		rc = checker_module_check_tsrecord(mod, (ast_tsrecord_t *)tspec->ext);
		break;
	case ant_tsenum:
		rc = checker_module_check_tsenum(mod, (ast_tsenum_t *)tspec->ext);
		break;
	default:
		assert(false);
		rc = EOK;
		break;
	}

	return rc;
}


/** Run checks on a global declaration.
 *
 * @param mod Checker module
 * @param decl AST declaration
 * @return EOK on success or error code
 */
static int checker_module_check_gdecln(checker_module_t *mod, ast_node_t *decl)
{
	int rc;
	ast_node_t *stmt;
	ast_fundef_t *fundef;
	checker_tok_t *tscolon;

	(void)mod;

	if (0) printf("Check function declaration\n");
	assert(decl->ntype == ant_fundef);
	fundef = (ast_fundef_t *)decl->ext;

	if (fundef->body == NULL) {
		tscolon = (checker_tok_t *)fundef->tscolon.data;
		checker_module_check_nows_before(tscolon,
		    "Unexpected whitespace before ';'.");
		return EOK;
	}

	stmt = ast_block_first(fundef->body);
	while (stmt != NULL) {
		rc = checker_module_check_stmt(mod, stmt);
		if (rc != EOK)
			return rc;

		stmt = ast_block_next(stmt);
	}

	return EOK;
}

/** Run checks on a type definition.
 *
 * @param mod Checker module
 * @param decl AST declaration
 * @return EOK on success or error code
 */
static int checker_module_check_typedef(checker_module_t *mod, ast_node_t *decln)
{
	checker_tok_t *tcomma;
	checker_tok_t *tscolon;
	ast_typedef_t *atypedef;
	ast_typedef_decl_t *decl;
	int rc;

	assert(decln->ntype == ant_typedef);
	atypedef = (ast_typedef_t *)decln->ext;

	rc = checker_module_check_tspec(mod, atypedef->tspec);
	if (rc != EOK)
		return rc;

	decl = ast_typedef_first(atypedef);
	while (decl != NULL) {
		tcomma = (checker_tok_t *)decl->tcomma.data;
		/* Note: declarators have a preceding comma except for the first */
		if (tcomma != NULL) {
			checker_module_check_nows_before(tcomma,
			    "Unexpected whitespace before ','.");
		}

		rc = checker_module_check_decl(mod, decl->decl);
		if (rc != EOK)
			return rc;

		decl = ast_typedef_next(decl);
	}

	tscolon = (checker_tok_t *)atypedef->tscolon.data;
	checker_module_check_nows_before(tscolon,
	    "Unexpected whitespace before ';'.");

	return EOK;
}

/** Run checks on a module.
 *
 * @param mod Checker module
 * @return EOK on success or error code
 */
static int checker_module_check(checker_module_t *mod)
{
	int rc;
	ast_node_t *decl;

	if (0) printf("Check module\n");
	decl = ast_module_first(mod->ast);
	while (decl != NULL) {
		switch (decl->ntype) {
		case ant_fundef:
			rc = checker_module_check_gdecln(mod, decl);
			break;
		case ant_typedef:
			rc = checker_module_check_typedef(mod, decl);
			break;
		default:
			assert(false);
			break;
		}

		if (rc != EOK)
			return rc;

		decl = ast_module_next(decl);
	}

	return EOK;
}

/** Run checker.
 *
 * @param checker Checker
 */
int checker_run(checker_t *checker)
{
	int rc;

	if (checker->mod == NULL) {
		rc = checker_module_lex(checker, &checker->mod);
		if (rc != EOK)
			return rc;

		rc = checker_module_parse(checker->mod);
		if (rc != EOK)
			return rc;

		rc = checker_module_check(checker->mod);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parser input from checker token list.
 *
 * @param arg Checker parser input (checker_parser_input_t)
 * @param tok Place to store token
 */
static void checker_parser_get_tok(void *arg, lexer_tok_t *tok)
{
	checker_parser_input_t *pinput = (checker_parser_input_t *)arg;

	*tok = pinput->tok->tok;
	/* Pass pointer to checker token down to checker_parser_tok_data */
	tok->udata = pinput->tok;

	if (tok->ttype != ltt_eof)
		pinput->tok = checker_module_next_tok(pinput->tok);
}

/** Get user data for a token.
 *
 * Return a pointer to the token. We can do this since we keep the
 * tokens in memory all the time.
 *
 * @param arg Checker parser input (checker_parser_input_t)
 * @param tok Place to store token
 */
static void *checker_parser_tok_data(void *arg, lexer_tok_t *tok)
{
	checker_tok_t *ctok;

	(void)arg;

	/* Pointer to checker_tok_t sent by checker_parser_get_tok. */
	ctok = (checker_tok_t *)tok->udata;

	/* Set this as user data for the AST token */
	return ctok;
}
