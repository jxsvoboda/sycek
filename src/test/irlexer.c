/*
 * Copyright 2021 Jiri Svoboda
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
 * Test IR lexer
 */

#include <irlexer.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <str_input.h>
#include <string.h>
#include <test/irlexer.h>

static const char *str_var =
    "var @a begin int.16 0; end;\n";

static const char *str_proc = "proc @add(%0, %1) begin "
    "add.16 %2, %0, %1; retv.16 %2; end;\n";

static const char *str_extern = "proc @foo() extern;\n";

/** Run IR lexer tests on a code fragment.
 *
 * @param str Code fragment (IR)
 * @return EOK on success or non-zero error code
 */
static int test_ir_lex_string(const char *str)
{
	int rc;
	ir_lexer_t *lexer;
	str_input_t sinput;
	bool done;
	ir_lexer_tok_t tok;

	str_input_init(&sinput, str);

	rc = ir_lexer_create(&lexer_str_input, &sinput, &lexer);
	if (rc != EOK)
		return rc;

	done = false;
	while (!done) {
		rc = ir_lexer_get_tok(lexer, &tok);
		if (rc != EOK)
			return rc;

		ir_lexer_dprint_tok(&tok, stdout);
		if (tok.ttype == itt_eof)
			done = true;
		ir_lexer_free_tok(&tok);
	}

	ir_lexer_destroy(lexer);
	printf("\n");

	str_input_init(&sinput, str);

	rc = ir_lexer_create(&lexer_str_input, &sinput, &lexer);
	if (rc != EOK)
		return rc;

	done = false;
	while (!done) {
		rc = ir_lexer_get_tok(lexer, &tok);
		if (rc != EOK)
			return rc;

		if (tok.ttype == itt_eof)
			done = true;
		else
			ir_lexer_print_tok(&tok, stdout);
		ir_lexer_free_tok(&tok);
	}

	ir_lexer_destroy(lexer);
	printf("\n");

	return EOK;
}

/** Run IR lexer tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_ir_lexer(void)
{
	int rc;

	rc = test_ir_lex_string(str_var);
	if (rc != EOK)
		return rc;

	rc = test_ir_lex_string(str_proc);
	if (rc != EOK)
		return rc;

	rc = test_ir_lex_string(str_extern);
	if (rc != EOK)
		return rc;

	return EOK;
}
