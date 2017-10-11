#include <lexer.h>
#include <test/lexer.h>

int main(int argc, char *argv[])
{
	int rc;

	(void)argc;
	(void)argv;
	rc = test_lexer();
	printf("test_lexer -> %d\n", rc);
	return 0;
}
