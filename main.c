#include <lexer.h>
#include <parser.h>
#include <test/lexer.h>
#include <test/parser.h>

int main(int argc, char *argv[])
{
	int rc;

	(void)argc;
	(void)argv;

	rc = test_lexer();
	printf("test_lexer -> %d\n", rc);

	rc = test_parser();
	printf("test_parser -> %d\n", rc);
	return 0;
}
