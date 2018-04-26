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
 * Test parser
 */

#include <ast.h>
#include <merrno.h>
#include <parser.h>
#include <test/parser.h>

static void parser_test_read_tok(void *, void *, unsigned, lexer_tok_t *);
static void *parser_test_next_tok(void *, void *);
static void *parser_test_tok_data(void *, void *);

static parser_input_ops_t parser_test_input = {
	.read_tok = parser_test_read_tok,
	.next_tok = parser_test_next_tok,
	.tok_data = parser_test_tok_data
};

lexer_toktype_t toks[] = {
	ltt_int,
	ltt_space,
	ltt_ident,
	ltt_lparen,
	ltt_void,
	ltt_rparen,
	ltt_space,
	ltt_lbrace,
	ltt_return,
	ltt_ident,
	ltt_scolon,
	ltt_space,
	ltt_rbrace,
	ltt_newline,
	ltt_eof
};

/** Run parser tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_parser(void)
{
	parser_t *parser;
	int rc;
	ast_module_t *module;

	rc = parser_create(&parser_test_input, NULL, (void *)0, 0, &parser);
	if (rc != EOK)
		return rc;

	rc = parser_process_module(parser, &module);
	if (rc != EOK)
		return rc;

	rc = ast_tree_print(&module->node, stdout);
	if (rc != EOK)
		return rc;

	putchar('\n');

	ast_tree_destroy(&module->node);
	parser_destroy(parser);

	return EOK;
}

/** Parser input from a global array */
static void parser_test_read_tok(void *apinput, void *atok, unsigned indlvl,
    lexer_tok_t *tok)
{
	size_t idx = (size_t)atok;
	(void) apinput;
	(void) indlvl;

	tok->ttype = toks[idx];
	tok->bpos.col = idx;
	tok->epos.col = idx;
}

/** Parser input from a global array */
static void *parser_test_next_tok(void *apinput, void *atok)
{
	size_t idx = (size_t)atok;
	(void) apinput;

	return (void *)(idx + 1);
}

/** Parser input from a global array */
static void *parser_test_tok_data(void *apinput, void *tok)
{
	(void) apinput;
	return tok;
}
