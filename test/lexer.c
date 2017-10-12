#include <lexer.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <string.h>
#include <test/lexer.h>

static int lexer_str_read(void *, char *, size_t, size_t *, src_pos_t *);

static lexer_input_ops_t lexer_str_input = {
	.read = lexer_str_read
};

int test_lexer(void)
{
	int rc;
	lexer_t *lexer;
	char *sp;
	bool done;
	lexer_tok_t tok;

	sp = "int main(void) {\nreturn 0;\n}\n";

	rc = lexer_create(&lexer_str_input, &sp, &lexer);
	if (rc != EOK)
		return rc;

	done = false;
	while (!done) {
		rc = lexer_get_tok(lexer, &tok);
		if (rc != EOK)
			return rc;

		lexer_dprint_tok(&tok, stdout);
		if (tok.ttype == ltt_eof)
			done = true;
		lexer_free_tok(&tok);
	}

	lexer_destroy(lexer);
	printf("\n");

	return EOK;
}

static int lexer_str_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	char **sp = (char **)arg;
	size_t len;

	printf("lexer_str_read\n");
	len = strlen(*sp);
	printf("lexer_str_read: bsize=%zu len=%zu\n", bsize, len);
	if (bsize < len)
		len = bsize;

	memcpy(buf, *sp, len);
	*nread = len;
	*sp += len;
	snprintf(bpos->file, src_pos_fname_max, "none");
	bpos->line = 1;
	bpos->col = 1;

	return EOK;
}
