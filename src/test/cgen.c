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
 * Test code generator
 */

#include <ast.h>
#include <cgen.h>
#include <ir.h>
#include <merrno.h>
#include <symbols.h>
#include <test/cgen.h>

/** Test code generation for module.
 *
 * @return EOK on success or non-zero error code
 */
static int test_cgen_module(void)
{
	int rc;
	cgen_t *cgen = NULL;
	ast_module_t *amodule = NULL;
	symbols_t *symbols = NULL;
	ir_module_t *module = NULL;

	rc = cgen_create(&cgen);
	if (rc != EOK)
		goto error;

	rc = ast_module_create(&amodule);
	if (rc != EOK)
		goto error;

	rc = symbols_create(&symbols);
	if (rc != EOK)
		goto error;

	rc = cgen_module(cgen, amodule, symbols, &module);
	if (rc != EOK)
		goto error;

	ast_tree_destroy(&amodule->node);
	symbols_destroy(symbols);
	ir_module_destroy(module);
	cgen_destroy(cgen);

	return EOK;
error:
	if (symbols != NULL)
		symbols_destroy(symbols);
	if (amodule != NULL)
		ast_tree_destroy(&amodule->node);
	ir_module_destroy(module);
	cgen_destroy(cgen);
	return rc;
}

/** Run code generator tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_cgen(void)
{
	int rc;

	rc = test_cgen_module();
	if (rc != EOK)
		return rc;

	return EOK;
}
