/*
 * Test lexer
 */

#include <lexer.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <str_input.h>
#include <string.h>
#include <test/lexer.h>

static const char *str_hello =
	"int main(void) {\nreturn 0;\n}\n";

static const char *str_keywords =
	"char do double enum extern float for goto if inline int long "
	"register return short signed sizeof static struct typedef union "
	"unsigned void volatile while";

/** Run lexer tests on a code fragment.
 *
 * @param str Code fragment
 * @return EOK on success or non-zero error code
 */
static int test_lex_string(const char *str)
{
	int rc;
	lexer_t *lexer;
	str_input_t sinput;
	bool done;
	lexer_tok_t tok;

	str_input_init(&sinput, str);

	rc = lexer_create(&lexer_str_input, &sinput, &lexer);
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

	str_input_init(&sinput, str);

	rc = lexer_create(&lexer_str_input, &sinput, &lexer);
	if (rc != EOK)
		return rc;

	done = false;
	while (!done) {
		rc = lexer_get_tok(lexer, &tok);
		if (rc != EOK)
			return rc;

		if (tok.ttype == ltt_eof)
			done = true;
		else
			lexer_print_tok(&tok, stdout);
		lexer_free_tok(&tok);
	}

	lexer_destroy(lexer);
	printf("\n");

	return EOK;
}

/** Run lexer tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_lexer(void)
{
	int rc;

	rc = test_lex_string(str_hello);
	if (rc != EOK)
		return rc;

	rc = test_lex_string(str_keywords);
	if (rc != EOK)
		return rc;

	return EOK;
}
