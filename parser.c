#include <ast.h>
#include <parser.h>
#include <merrno.h>
#include <stdlib.h>

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

void parser_destroy(parser_t *parser)
{
	free(parser);
}

int parser_process_module(parser_t *parser, ast_module_t **rmodule)
{
	ast_module_t *module;
	int rc;

	(void)parser;

	rc = ast_module_create(&module);
	if (rc != EOK)
		return rc;

	*rmodule = module;
	return EOK;
}
