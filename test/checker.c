/*
 * Test checker
 */

#include <checker.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <string.h>
#include <test/checker.h>

static int lexer_str_read(void *, char *, size_t, size_t *, src_pos_t *);

static lexer_input_ops_t lexer_str_input = {
	.read = lexer_str_read
};

static const char *str_hello =
	"int main(void) {\nreturn 0;\n}\n";

/** Run lexer tests on a code fragment.
 *
 * @param str Code fragment
 * @return EOK on success or non-zero error code
 */
static int test_check_string(const char *str)
{
	int rc;
	checker_t *checker;
	const char *sp;

	sp = str;

	rc = checker_create(&lexer_str_input, &sp, &checker);
	if (rc != EOK)
		return rc;

	rc = checker_run(checker);
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

/** Lexer input form a string constant. */
static int lexer_str_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	char **sp = (char **)arg;
	size_t len;

//	printf("lexer_str_read\n");
	len = strlen(*sp);
//	printf("lexer_str_read: bsize=%zu len=%zu\n", bsize, len);
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
