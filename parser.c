/*
 * Parser
 */

#include <ast.h>
#include <parser.h>
#include <merrno.h>
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
	int rc;

	(void)parser;

	rc = ast_return_create(&areturn);
	if (rc != EOK)
		return rc;

	*rstmt = &areturn->node;
	return EOK;
}


/** Parse block.
 *
 * @param parser Parser
 * @param braces Whether there should be braces around the block
 * @param rblock Place to store pointer to new AST block
 *
 * @return EOK on success or non-zero error code
 */
static int parser_process_block(parser_t *parser, ast_braces_t braces,
    ast_block_t **rblock)
{
	ast_block_t *block;
	ast_node_t *stmt;
	int rc;

	rc = ast_block_create(braces, &block);
	if (rc != EOK)
		return rc;

	while (1 /*not end of block*/) {
		rc = parser_process_stmt(parser, &stmt);
		if (rc != EOK)
			return rc;

		ast_block_append(block, stmt);
		break;
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
	int rc;

	(void)parser;

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

	rc = parser_process_block(parser, ast_braces, &body);
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
	ast_type_t *ftype = NULL;
	int rc;

	(void)parser;

	rc = parser_process_type(parser, &ftype);
	if (rc != EOK)
		goto error;

	rc = parser_process_fundef(parser, ftype, &fundef);
	if (rc != EOK)
		goto error;

	*rnode = &fundef->node;
	return EOK;
error:
	if (ftype != NULL)
		ast_tree_destroy(&ftype->node);
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

	while (1 /*not end of module*/) {
		rc = parser_process_decl(parser, &decl);
		if (rc != EOK)
			return rc;

		ast_module_append(module, decl);
		break;
	}

	*rmodule = module;
	return EOK;
}
