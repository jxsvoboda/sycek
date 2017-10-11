#include <errno.h>
#include <lexer.h>
#include <merrno.h>
#include <stdlib.h>

int lexer_create(lexer_input_ops_t *ops, void *arg, lexer_t **rlexer)
{
	lexer_t *lexer;

	(void) ops;
	(void) arg;
	(void) rlexer;

	lexer = calloc(1, sizeof(lexer_t));
	if (lexer == NULL)
		return ENOMEM;

	*rlexer = lexer;
	return EOK;
}

void lexer_destroy(lexer_t *lexer)
{
	if (lexer == NULL)
		return;

	free(lexer);
}

int lexer_get_tok(lexer_t *lexer, lexer_tok_t *tok)
{
	(void)lexer;
	(void)tok;
	return EOK;
}

void lexer_free_tok(lexer_tok_t *tok)
{
	(void)tok;
}

int lexer_dump_tok(lexer_tok_t *tok, FILE *f)
{
	(void)tok;
	return fprintf(f, "<>") >= 0;
}
