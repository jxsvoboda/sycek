#include <lexer.h>
#include <merrno.h>
#include <string.h>
#include <test/lexer.h>

static int lexer_str_read(void *, char *, size_t, size_t *);

static lexer_input_ops_t lexer_str_input = {
	.read = lexer_str_read
};

int test_lexer(void)
{
	int rc;
	lexer_t *lexer;
	char *sp;
	lexer_tok_t tok;

	sp = "int main(void) {\nreturn 0;\n}\n";

	rc = lexer_create(&lexer_str_input, &sp, &lexer);
	if (rc != EOK)
		return rc;

	rc = lexer_get_tok(lexer, &tok);
	if (rc != EOK)
		return rc;

	lexer_dump_tok(&tok, stdout);
	lexer_free_tok(&tok);

	lexer_destroy(lexer);
	printf("\n");

	return EOK;
}

static int lexer_str_read(void *arg, char *buf, size_t bsize, size_t *nread)
{
	char **sp = (char **)arg;
	size_t len;

	len = strlen(*sp);
	if (bsize < len)
		len = bsize;

	memcpy(buf, *sp, len);
	*nread = len;
	*sp += len;

	return EOK;
}
