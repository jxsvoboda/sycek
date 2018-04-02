/*
 * Copyright 2018 Jiri Svoboda
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
 * C-style checker tool
 */

#include <checker.h>
#include <file_input.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <test/ast.h>
#include <test/checker.h>
#include <test/lexer.h>
#include <test/parser.h>

static void print_syntax(void)
{
	printf("C-style checker\n");
	printf("syntax:\n"
	    "\tccheck [options] <file> Check C-style in the specified file\n"
	    "\tccheck --test Run internal unit tests\n"
	    "options:\n"
	    "\t--fix Attempt to fix issues instead of just reporting them\n"
	    "\t--dump-ast Dump internal abstract syntax tree\n");
}

static int check_file(const char *fname, bool fix, bool dump_ast)
{
	int rc;
	checker_t *checker = NULL;
	char *bkname;
	file_input_t finput;
	FILE *f;

	f = fopen(fname, "rt");
	if (f == NULL) {
		printf("Cannot open '%s'.\n", fname);
		rc = ENOENT;
		goto error;
	}

	file_input_init(&finput, f, fname);

	rc = checker_create(&lexer_file_input, &finput, &checker);
	if (rc != EOK)
		goto error;

	if (dump_ast) {
		rc = checker_dump_ast(checker, stdout);
		if (rc != EOK)
			goto error;

		printf("\n");
	}

	rc = checker_run(checker, fix);
	if (rc != EOK)
		goto error;

	fclose(f);

	if (fix) {
		if (asprintf(&bkname, "%s.orig", fname) < 0) {
			rc = ENOMEM;
			goto error;
		}

		if (rename(fname, bkname) < 0) {
			printf("Error renaming '%s' to '%s'.\n", fname,
			    bkname);
			rc = EIO;
			goto error;
		}

		f = fopen(fname, "wt");
		if (f == NULL) {
			printf("Cannot open '%s' for writing.\n", fname);
			rc = EIO;
			goto error;
		}

		rc = checker_print(checker, f);
		if (rc != EOK)
			goto error;

		if (fclose(f) < 0) {
			printf("Error writing '%s'.\n", fname);
			rc = EIO;
			goto error;
		}
	}

	checker_destroy(checker);

	return EOK;
error:
	if (checker != NULL)
		checker_destroy(checker);
	if (f != NULL)
		fclose(f);
	return rc;
}

int main(int argc, char *argv[])
{
	int rc;
	int i;
	bool fix = false;
	bool dump_ast = false;

	(void)argc;
	(void)argv;

	if (argc < 2) {
		print_syntax();
		return 1;
	}

	if (argc == 2 && strcmp(argv[1], "--test") == 0) {
		/* Run tests */
		rc = test_lexer();
		printf("test_lexer -> %d\n", rc);

		rc = test_ast();
		printf("test_ast -> %d\n", rc);

		rc = test_parser();
		printf("test_parser -> %d\n", rc);

		rc = test_checker();
		printf("test_checker -> %d\n", rc);
	} else {
		i = 1;
		while (argc > i && argv[i][0] == '-') {
			if (strcmp(argv[i], "--fix") == 0) {
				++i;
				fix = true;
			} else if (strcmp(argv[i], "--dump-ast") == 0) {
				++i;
				dump_ast = true;
			} else if (strcmp(argv[i], "-") == 0) {
				++i;
				break;
			} else {
				printf("Invalid option.\n");
				return 1;
			}
		}

		if (argc <= i) {
			printf("Argument missing.\n");
			return 1;
		}

		rc = check_file(argv[i], fix, dump_ast);
	}

	if (rc != EOK)
		return 1;

	return 0;
}