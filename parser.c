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
		} while (parser->tok[parser->tokcnt].ttype == ltt_wspace);

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

/** Parse type expression.
 *
 * @param parser Parser
 * @param rtype Place to store pointer to new AST type
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_type(parser_t *parser, ast_type_t **rtype)
{
	ast_type_t *ptype;
	lexer_toktype_t ltt;
	int rc;

	ltt = parser_next_ttype(parser);
	switch (ltt) {
	case ltt_void:
	case ltt_int:
		break;
	default:
		printf("Unexpected %d, expected type.\n", ltt);
		return EINVAL;
	}

	parser_skip(parser, NULL);

	rc = ast_type_create(&ptype);
	if (rc != EOK)
		return rc;

	*rtype = ptype;
	return EOK;
}

/** Parse function definition.
 *
 * @param parser Parser
 * @param ftype Function type expression, including function identifier
 * @param rnode Place to store pointer to new function definition
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_fundef(parser_t *parser, ast_type_t *ftype,
    ast_fundef_t **rfundef)
{
	ast_fundef_t *fundef;
	ast_block_t *body;
	int rc;

	(void)parser;

	rc = parser_process_block(parser, &body);
	if (rc != EOK)
		return rc;

	rc = ast_fundef_create(ftype, body, &fundef);
	if (rc != EOK) {
		ast_tree_destroy(&body->node);
		return rc;
	}

	*rfundef = fundef;
	return EOK;
}

/** Parse declaration.
 *
 * @param parser Parser
 * @param rnode Place to store pointer to new declaration node
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_decl(parser_t *parser, ast_node_t **rnode)
{
	ast_fundef_t *fundef;
	ast_type_t *rtype = NULL;
	ast_type_t *atype = NULL;
	int rc;

	(void)parser;

	rc = parser_process_type(parser, &rtype);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_ident, NULL);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_lparen, NULL);
	if (rc != EOK)
		goto error;

	rc = parser_process_type(parser, &atype);
	if (rc != EOK)
		goto error;

	rc = parser_match(parser, ltt_rparen, NULL);
	if (rc != EOK)
		goto error;

	rc = parser_process_fundef(parser, rtype/*XXX*/, &fundef);
	if (rc != EOK)
		goto error;

	*rnode = &fundef->node;
	return EOK;
error:
	if (rtype != NULL)
		ast_tree_destroy(&rtype->node);
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
	ast_module_t *module;
	ast_node_t *decl;
	int rc;

	(void)parser;

	rc = ast_module_create(&module);
	if (rc != EOK)
		return rc;

	while (parser_next_ttype(parser) != ltt_eof) {
		rc = parser_process_decl(parser, &decl);
		if (rc != EOK)
			return rc;

		ast_module_append(module, decl);
		break;
	}

	*rmodule = module;
	return EOK;
}
