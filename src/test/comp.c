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
 * Test compiler
 */

#include <comp.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <str_input.h>
#include <string.h>
#include <test/comp.h>

static const char *str_hello =
    "int main(void) {\nreturn 1 + 1;\n}\n";

/** Run compiler test on a code fragment.
 *
 * @param str Code fragment
 * @return EOK on success or non-zero error code
 */
static int test_comp_string(const char *str)
{
	int rc;
	comp_t *comp;
	str_input_t sinput;

	str_input_init(&sinput, str);

	rc = comp_create(&lexer_str_input, &sinput, &comp);
	if (rc != EOK)
		return rc;

	rc = comp_run(comp);
	if (rc != EOK)
		return rc;

	rc = comp_dump_toks(comp, stdout);
	if (rc != EOK)
		return rc;

	rc = comp_dump_ast(comp, stdout);
	if (rc != EOK)
		return rc;

	rc = comp_dump_ir(comp, stdout);
	if (rc != EOK)
		return rc;

	comp_destroy(comp);

	return EOK;
}

/** Run compiler tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_comp(void)
{
	int rc;

	rc = test_comp_string(str_hello);
	if (rc != EOK)
		return rc;

	return EOK;
}
