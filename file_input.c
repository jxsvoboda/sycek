/*
 * Lexer input from file
 */

#include <file_input.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdio.h>

static int file_lexer_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t lexer_file_input = {
	.read = file_lexer_read
};

/** Lexer input form a string constant. */
static int file_lexer_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	file_input_t *finput = (file_input_t *)arg;
	size_t i;
	size_t nr;

	nr = fread(buf, 1, bsize, finput->f);
	if (ferror(finput->f))
		return EIO;

	*nread = nr;
	*bpos = finput->cpos;

	/* Advance source position */
	for (i = 0; i < nr; i++)
		src_pos_fwd_char(&finput->cpos, buf[i]);

	return EOK;
}

void file_input_init(file_input_t *finput, FILE *f, const char *fname)
{
	finput->f = f;

	src_pos_set(&finput->cpos, fname, 1, 1);
}
