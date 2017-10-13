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

int test_parser(void)
{
	parser_t *parser;
	int rc;
	size_t idx;
	ast_node_t *mod;

	idx = 0;
	rc = parser_create(&parser_test_input, &idx, &parser);
	if (rc != EOK)
		return rc;

	rc = parser_process_module(parser, &mod);
	if (rc != EOK)
		return rc;

	parser_destroy(parser);

	return EOK;
}

static int parser_test_get_tok(void *arg, lexer_tok_t *tok)
{
	size_t *idx = (size_t *)arg;

	tok->ttype = toks[*idx];
	if (tok->ttype != ltt_eof)
		++(*idx);

	return EOK;
}
