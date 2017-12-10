/*
 * Checker
 */

#include <assert.h>
#include <checker.h>
#include <lexer.h>
#include <merrno.h>
#include <stdlib.h>

/** Create checker.
 *
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param rchecker Place to store new checker.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int checker_create(lexer_input_ops_t *input_ops, void *input_arg,
    checker_t **rchecker)
{
	checker_t *checker = NULL;
	lexer_t *lexer = NULL;
	int rc;

	checker = calloc(1, sizeof(checker_t));
	if (checker == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = lexer_create(input_ops, input_arg, &lexer);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		goto error;
	}

	checker->lexer = lexer;

	*rchecker = checker;
	return EOK;
error:
	if (lexer != NULL)
		lexer_destroy(lexer);
	if (checker != NULL)
		free(checker);
	return rc;
}

/** Destroy checker.
 *
 * @param checker Checker
 */
void checker_destroy(checker_t *checker)
{
	lexer_destroy(checker->lexer);
	free(checker);
}

/** Run checker.
 *
 * @param checker Checker
 */
int checker_run(checker_t *checker)
{
	(void)checker;
	return 0;
}
