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
 * Test checker
 */

#include <checker.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <str_input.h>
#include <string.h>
#include <test/checker.h>

static const char *str_hello =
    "int main(void) {\nreturn 0; return 0; return 0; return 0;\n"
    "return 0;\nreturn 0;\nreturn 0 ; return 0;\n"
    "return 0; return 0;}\n";

/** Run lexer tests on a code fragment.
 *
 * @param str Code fragment
 * @return EOK on success or non-zero error code
 */
static int test_check_string(const char *str)
{
	int rc;
	checker_t *checker;
	str_input_t sinput;
	checker_cfg_t cfg;

	checker_cfg_init(&cfg);

	str_input_init(&sinput, str);

	rc = checker_create(&lexer_str_input, &sinput, cmod_c, &cfg, &checker);
	if (rc != EOK)
		return rc;

	rc = checker_run(checker, false);
	if (rc != EOK)
		return rc;

	checker_destroy(checker);

	return EOK;
}

/** Run checker tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_checker(void)
{
	int rc;

	rc = test_check_string(str_hello);
	if (rc != EOK)
		return rc;

	return EOK;
}
