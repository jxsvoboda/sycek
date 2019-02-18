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
 * C compiler / static checker
 */

#include <comp.h>
#include <file_input.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <test/ir.h>

static void print_syntax(void)
{
	printf("C compiler / static checker\n");
	printf("syntax:\n"
	    "\tsyc [options] <file> Compile / check the specified file\n"
	    "\tsyc --test Run internal unit tests\n"
	    "options:\n"
	    "\t--dump-ast Dump internal abstract syntax tree\n"
	    "\t--dump-toks Dump tokenized source file\n"
	    "\t--dump-ir Dump intermediate representation\n");
}

static int compile_file(const char *fname, comp_flags_t flags)
{
	int rc;
	comp_t *comp = NULL;
	file_input_t finput;
	FILE *f = NULL;

	f = fopen(fname, "rt");
	if (f == NULL) {
		fprintf(stderr, "Cannot open '%s'.\n", fname);
		rc = ENOENT;
		goto error;
	}

	file_input_init(&finput, f, fname);

	rc = comp_create(&lexer_file_input, &finput, &comp);
	if (rc != EOK)
		goto error;

	if ((flags & compf_dump_ast) != 0) {
		rc = comp_dump_ast(comp, stdout);
		if (rc != EOK)
			goto error;

		printf("\n");
	}

	if ((flags & compf_dump_toks) != 0) {
		rc = comp_dump_toks(comp, stdout);
		if (rc != EOK)
			goto error;

		printf("\n");
	}

	rc = comp_run(comp);
	if (rc != EOK)
		goto error;

	if ((flags & compf_dump_ir) != 0) {
		rc = comp_dump_ir(comp, stdout);
		if (rc != EOK)
			goto error;
	}

	fclose(f);

	comp_destroy(comp);

	return EOK;
error:
	if (comp != NULL)
		comp_destroy(comp);
	if (f != NULL)
		fclose(f);
	return rc;
}

int main(int argc, char *argv[])
{
	int rc;
	int i;
	comp_flags_t flags = 0;

	if (argc < 2) {
		print_syntax();
		return 1;
	}

	if (argc == 2 && strcmp(argv[1], "--test") == 0) {
		/* Run tests */
		rc = test_ir();
		printf("test_ir -> %d\n", rc);
		if (rc != EOK)
			return 1;

		printf("Tests passed.\n");
		return 0;
	}

	i = 1;
	while (argc > i && argv[i][0] == '-') {
		if (strcmp(argv[i], "--dump-ast") == 0) {
			++i;
			flags |= compf_dump_ast;
		} else if (strcmp(argv[i], "--dump-toks") == 0) {
			++i;
			flags |= compf_dump_toks;
		} else if (strcmp(argv[i], "--dump-ir") == 0) {
			++i;
			flags |= compf_dump_ir;
		} else if (strcmp(argv[i], "-") == 0) {
			++i;
			break;
		} else {
			fprintf(stderr, "Invalid option.\n");
			return 1;
		}
	}

	if (argc <= i) {
		fprintf(stderr, "Argument missing.\n");
		return 1;
	}

	rc = compile_file(argv[i], flags);
	if (rc != EOK)
		return 1;

	return 0;
}