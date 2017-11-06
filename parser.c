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
	int rc;

	(void)parser;

	rc = ast_fundef_create(NULL, NULL, &fundef);
	if (rc != EOK)
		return rc;

	*rnode = &fundef->node;
	return EOK;
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
