#include <ast.h>
#include <merrno.h>
#include <stdio.h>

static int test_ast_module(void)
{
	ast_module_t *module;
	int rc;

	rc = ast_module_create(&module);
	if (rc != EOK)
		return rc;

	ast_tree_print(&module->node, stdout);
	putchar('\n');

	return EOK;
}

static int test_ast_fundef(void)
{
	ast_fundef_t *fundef;
	int rc;

	rc = ast_fundef_create(NULL, NULL, &fundef);
	if (rc != EOK)
		return rc;

	ast_tree_print(&fundef->node, stdout);
	putchar('\n');

	return EOK;
}

int test_ast(void)
{
	int rc;

	rc = test_ast_module();
	if (rc != EOK)
		return rc;

	rc = test_ast_fundef();
	if (rc != EOK)
		return rc;

	return EOK;
}
