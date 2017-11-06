/*
 * Test parser
 */

#include <ast.h>
#include <merrno.h>
#include <parser.h>
#include <test/parser.h>

static int parser_test_get_tok(void *, lexer_tok_t *);

static parser_input_ops_t parser_test_input = {
	.get_tok = parser_test_get_tok
};

lexer_toktype_t toks[] = {
	ltt_int,
	ltt_wspace,
	ltt_ident,
	ltt_lparen,
	ltt_void,
	ltt_rparen,
	ltt_wspace,
	ltt_lbrace,
	ltt_return,
	ltt_number,
	ltt_scolon,
	ltt_wspace,
	ltt_rbrace,
	ltt_wspace,
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
	size_t idx;
	ast_module_t *module;

	idx = 0;
	rc = parser_create(&parser_test_input, &idx, &parser);
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
static int parser_test_get_tok(void *arg, lexer_tok_t *tok)
{
	size_t *idx = (size_t *)arg;

	tok->ttype = toks[*idx];
	if (tok->ttype != ltt_eof)
		++(*idx);

	return EOK;
}
