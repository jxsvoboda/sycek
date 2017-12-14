/*
 * C-style checker tool
 */

#include <checker.h>
#include <file_input.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
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
	    "\tccheck <file> Check C-style in the specified file\n"
	    "\tccheck --test Run internal unit tests\n");
}

static int check_file(const char *fname)
{
	int rc;
	checker_t *checker = NULL;
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

	rc = checker_run(checker);
	if (rc != EOK)
		goto error;

	checker_destroy(checker);
	fclose(f);

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
	} else if (argc == 2) {
		rc = check_file(argv[1]);
	}

	if (rc != EOK)
		return 1;

	return 0;
}
