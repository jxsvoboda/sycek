/*
 * Test checker
 */

#include <checker.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <str_input.h>
#include <string.h>
#include <test/checker.h>

static const char *str_hello =
	"int main(void) {\nreturn 0;return 0;return 0;return 0;\n"
	"return 0;\nreturn 0;return 0;return 0;\n"
	"return 0;return 0;}\n";

/** Run lexer tests on a code fragment.
 *
 * @param str Code fragment
 * @return EOK on success or non-zero error code
 */
static int test_check_string(const char *str)
{
	int rc;
	checker_t *checker;
	str_input_t sinput;

	str_input_init(&sinput, str);

	rc = checker_create(&lexer_str_input, &sinput, &checker);
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
