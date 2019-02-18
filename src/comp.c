/*
 * Copyright 2019 Jiri Svoboda
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
 * Compiler
 */

#include <adt/list.h>
#include <assert.h>
#include <ast.h>
#include <cgen.h>
#include <comp.h>
#include <ir.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
#include <stdbool.h>
#include <stdlib.h>

static void comp_parser_read_tok(void *, void *, unsigned, bool,
    lexer_tok_t *);
static void *comp_parser_next_tok(void *, void *);
static void *comp_parser_tok_data(void *, void *);
static comp_tok_t *comp_module_first_tok(comp_module_t *);
static comp_tok_t *comp_next_tok(comp_tok_t *);
static void comp_remove_token(comp_tok_t *);

static parser_input_ops_t comp_parser_input = {
	.read_tok = comp_parser_read_tok,
	.next_tok = comp_parser_next_tok,
	.tok_data = comp_parser_tok_data
};

/** Create compiler module.
 *
 * @param comp Compiler
 * @param rmodule Place to store new compiler module.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int comp_module_create(comp_t *comp,
    comp_module_t **rmodule)
{
	comp_module_t *module = NULL;

	module = calloc(1, sizeof(comp_module_t));
	if (module == NULL)
		return ENOMEM;

	list_initialize(&module->toks);
	module->comp = comp;

	*rmodule = module;
	return EOK;
}

/** Destroy compiler module.
 *
 * @param comp Compiler
 */
static void comp_module_destroy(comp_module_t *module)
{
	comp_tok_t *tok;

	if (module == NULL)
		return;

	if (module->ast != NULL)
		ast_tree_destroy(&module->ast->node);

	tok = comp_module_first_tok(module);
	while (tok != NULL) {
		comp_remove_token(tok);
		tok = comp_module_first_tok(module);
	}

	free(module);
}

/** Create a compiler token.
 *
 * @param tok Lexer token
 * @param rctok Place to store pointer to new compiler token
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int comp_tok_new(lexer_tok_t *tok, comp_tok_t **rctok)
{
	comp_tok_t *ctok;

	ctok = calloc(1, sizeof(comp_tok_t));
	if (ctok == NULL)
		return ENOMEM;

	ctok->tok = *tok;
	*rctok = ctok;
	return EOK;
}

/** Append a token to compiler module.
 *
 * @param module Compiler module
 * @param tok Lexer token
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int comp_module_append(comp_module_t *module, lexer_tok_t *tok)
{
	comp_tok_t *ctok;
	int rc;

	rc = comp_tok_new(tok, &ctok);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		return rc;
	}

	ctok->mod = module;
	list_append(&ctok->ltoks, &module->toks);

	return EOK;
}

/** Remove a token from the source code.
 *
 * @param tok Token to remove
 */
static void comp_remove_token(comp_tok_t *tok)
{
	list_remove(&tok->ltoks);
	lexer_free_tok(&tok->tok);
	free(tok);
}

/** Create compiler.
 *
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param mtype Module type
 * @param cfg Configuration
 * @param rcomp Place to store new compiler.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int comp_create(lexer_input_ops_t *input_ops, void *input_arg,
    comp_t **rcomp)
{
	comp_t *comp = NULL;
	lexer_t *lexer = NULL;
	int rc;

	comp = calloc(1, sizeof(comp_t));
	if (comp == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = lexer_create(input_ops, input_arg, &lexer);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		goto error;
	}

	comp->lexer = lexer;
	*rcomp = comp;
	return EOK;
error:
	if (lexer != NULL)
		lexer_destroy(lexer);
	if (comp != NULL)
		free(comp);
	return rc;
}

/** Destroy compiler.
 *
 * @param comp Compiler
 */
void comp_destroy(comp_t *comp)
{
	comp_module_destroy(comp->mod);
	lexer_destroy(comp->lexer);
	free(comp);
}

/** Lex a module.
 *
 * @param comp Compiler
 * @param rmodule Place to store pointer to new module
 * @return EOK on success, or error code
 */
static int comp_module_lex(comp_t *comp, comp_module_t **rmodule)
{
	comp_module_t *module = NULL;
	bool done;
	lexer_tok_t tok;
	int rc;

	rc = comp_module_create(comp, &module);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		goto error;
	}

	done = false;
	while (!done) {
		rc = lexer_get_tok(comp->lexer, &tok);
		if (rc != EOK)
			return rc;

		rc = comp_module_append(module, &tok);
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

/** Get first token in a compiler module.
 *
 * @param mod Compiler module
 * @return First token or @c NULL if the list is empty
 */
static comp_tok_t *comp_module_first_tok(comp_module_t *mod)
{
	link_t *link;

	link = list_first(&mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, comp_tok_t, ltoks);
}

/** Get next token in a compiler module
 *
 * @param tok Current token or @c NULL
 * @return Next token or @c NULL
 */
static comp_tok_t *comp_next_tok(comp_tok_t *tok)
{
	link_t *link;

	if (tok == NULL)
		return NULL;

	link = list_next(&tok->ltoks, &tok->mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, comp_tok_t, ltoks);
}

/** Parse a module.
 *
 * @param mod Compiler module
 * @return EOK on success or error code
 */
static int comp_module_parse(comp_module_t *mod)
{
	ast_module_t *amod;
	parser_t *parser;
	comp_parser_input_t pinput;
	int rc;

	rc = parser_create(&comp_parser_input, &pinput,
	    comp_module_first_tok(mod), 0, false, &parser);
	if (rc != EOK)
		return rc;

	rc = parser_process_module(parser, &amod);
	if (rc != EOK)
		goto error;

	if (0) {
		putchar('\n');
		rc = ast_tree_print(&amod->node, stdout);
		if (rc != EOK)
			goto error;
		putchar('\n');
	}

	mod->ast = amod;
	parser_destroy(parser);

	return EOK;
error:
	parser_destroy(parser);
	return rc;
}

/** Make sure compiler tokenized source is available.
 *
 * If source hasn't been tokenized yet, do it now.
 *
 * @param comp Compiler
 * @return EOK on success or error code
 */
static int comp_build_toks(comp_t *comp)
{
	int rc;

	if (comp->mod == NULL) {
		rc = comp_module_lex(comp, &comp->mod);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Make sure compiler AST is available.
 *
 * If AST hasn't been built yet, build it
 *
 * @param comp Compiler
 * @return EOK on success or error code
 */
static int comp_build_ast(comp_t *comp)
{
	int rc;

	rc = comp_build_toks(comp);
	if (rc != EOK)
		return rc;

	if (comp->mod->ast == NULL) {
		rc = comp_module_parse(comp->mod);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Run compiler.
 *
 * @param comp Compiler
 */
int comp_run(comp_t *comp)
{
	int rc;
	cgen_t *cgen = NULL;

	if (comp->mod == NULL || comp->mod->ast == NULL) {
		rc = comp_build_ast(comp);
		if (rc != EOK)
			return rc;
	}

	if (comp->mod->ir == NULL) {
		rc = cgen_create(&cgen);
		if (rc != EOK)
			goto error;

		rc = cgen_module(cgen, comp->mod->ast, &comp->mod->ir);
		if (rc != EOK)
			goto error;

		cgen_destroy(cgen);
		cgen = NULL;
	}

	return EOK;
error:
	cgen_destroy(cgen);
	return rc;
}

/** Dump AST.
 *
 * @param comp Compiler
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_dump_ast(comp_t *comp, FILE *f)
{
	int rc;

	if (comp->mod == NULL || comp->mod->ast == NULL) {
		rc = comp_build_ast(comp);
		if (rc != EOK)
			return rc;
	}

	return ast_tree_print(&comp->mod->ast->node, f);
}

/** Dump tokenized source.
 *
 * @param comp Compiler
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_dump_toks(comp_t *comp, FILE *f)
{
	comp_tok_t *tok;
	int rc;

	rc = comp_build_toks(comp);
	if (rc != EOK)
		return rc;

	tok = comp_module_first_tok(comp->mod);
	while (tok->tok.ttype != ltt_eof) {
		rc = lexer_dprint_tok(&tok->tok, f);
		if (rc != EOK)
			return rc;

		if (tok->tok.ttype == ltt_newline) {
			if (fputc('\n', f) < 0) {
				rc = EIO;
				return rc;
			}
		}

		tok = comp_next_tok(tok);
	}

	return EOK;
}

/** Dump intermediate representation.
 *
 * @param comp Compiler
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_dump_ir(comp_t *comp, FILE *f)
{
	int rc;

	assert(comp->mod->ir != NULL);
	rc = ir_module_print(comp->mod->ir, f);
	return rc;
}

/** Parser function to read input token from compiler.
 *
 * @param apinput Compiler parser input (comp_parser_input_t *)
 * @param atok Compiler token (comp_tok_t *)
 * @param indlvl Indentation level to annotate the token with
 * @param seccont Secondary continuation flag to annotate the token with
 * @param ltok Place to store token
 */
static void comp_parser_read_tok(void *apinput, void *atok,
    unsigned indlvl, bool seccont, lexer_tok_t *ltok)
{
	comp_parser_input_t *pinput = (comp_parser_input_t *)apinput;
	comp_tok_t *tok = (comp_tok_t *)atok;

	(void) pinput;
	(void) indlvl;
	(void) seccont;
	*ltok = tok->tok;
	/* Pass pointer to compiler token down to comp_parser_tok_data */
	ltok->udata = tok;
}

/** Parser function to get next input token from compiler.
 *
 * @param apinput Compiler parser input (comp_parser_input_t *)
 * @param atok Compiler token (comp_tok_t *)
 * @return Pointer to next token
 */
static void *comp_parser_next_tok(void *apinput, void *atok)
{
	comp_parser_input_t *pinput = (comp_parser_input_t *)apinput;
	comp_tok_t *tok = (comp_tok_t *)atok;
	comp_tok_t *ntok;

	(void) pinput;

	if (tok->tok.ttype != ltt_eof)
		ntok = comp_next_tok(tok);
	else
		ntok = tok;

	return (void *) ntok;
}

/** Get user data for a token.
 *
 * Return a pointer to the token. We can do this since we keep the
 * tokens in memory all the time.
 *
 * @param arg Compiler parser input (comp_parser_input_t)
 * @param tok Token
 * @param tok Place to store token
 */
static void *comp_parser_tok_data(void *apinput, void *tok)
{
	(void)apinput;

	/* Set this as user data for the AST token */
	return tok;
}
