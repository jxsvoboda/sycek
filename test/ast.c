/*
 * Test AST
 */

#include <ast.h>
#include <merrno.h>
#include <stdio.h>

/** Test AST module.
 *
 * @return EOK on success or non-zero error code
 */
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

/** Test AST function definition.
 *
 * @return EOK on success or non-zero error code
 */
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

/** Run AST tests.
 *
 * @return EOK on success or non-zero error code
 */
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
