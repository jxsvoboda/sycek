/*
 * Copyright 2026 Jiri Svoboda
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Lexer input from file
 */

#include <assert.h>
#include <file_input.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdio.h>
#include <stdlib.h>

static void file_input_pop(file_input_t *);
static int file_lexer_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t lexer_file_input = {
	.read = file_lexer_read
};

/** Lexer input from file(s).
 *
 * @param arg Argument (file_input_t *)
 * @param buf Character buffer
 * @param posbuf Position buffer
 * @param bsize Size of character buffer / position buffer in entries
 * @param nread Place to store number of characters read.
 * @param bpos Place to store position of the beginning of the buffer
 * @return EOK on success or an error code
 */
static int file_lexer_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	file_input_t *finput = (file_input_t *)arg;
	size_t i;
	size_t nr;

	*nread = 0;

	while (*nread == 0 && finput->top != NULL) {
		*bpos = finput->top->cpos;

		nr = fread(buf + *nread, 1, bsize - *nread, finput->top->f);
		if (ferror(finput->top->f) != 0)
			return EIO;

		/* Advance source position */
		for (i = 0; i < nr; i++)
			src_pos_fwd_char(&finput->top->cpos, buf[*nread + i]);

		*nread += nr;

		if (*nread == bsize)
			break;

		/* End of file - pop it off the stack. */
		file_input_pop(finput);
	}

	return EOK;
}

/** Create file input.
 *
 * @param f File stream
 * @param fname File name
 * @param rfinput Place to store pointer to new file input
 * @return EOK on success, ENOMEM if out of memory
 */
int file_input_create(FILE *f, const char *fname, file_input_t **rfinput)
{
	file_input_t *finput = NULL;
	int rc;

	finput = calloc(1, sizeof(file_input_t));
	if (finput == NULL)
		return ENOMEM;

	list_initialize(&finput->stack);

	rc = file_input_push(finput, f, fname);
	if (rc != EOK) {
		free(finput);
		return rc;
	}

	*rfinput = finput;
	return EOK;
}

/** Push new file to file input stack.
 *
 * @param finput File input
 * @param f File stream
 * @param fname File name
 * @return EOK on success, ENOMEM if out of memory
 */
int file_input_push(file_input_t *finput, FILE *f, const char *fname)
{
	file_input_entry_t *entry;

	entry = calloc(1, sizeof(file_input_entry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->f = f;
	src_pos_set(&entry->cpos, fname, 1, 1);

	list_append(&entry->lstack, &finput->stack);
	finput->top = entry;
	return EOK;
}

/** Get topmost file input entry.
 *
 * @param finput File input
 * @return Topmost etry or @c NULL if the stack is empty.
 */
static file_input_entry_t *file_input_get_top(file_input_t *finput)
{
	link_t *link;

	link = list_last(&finput->stack);
	if (link != NULL) {
		return list_get_instance(link, file_input_entry_t, lstack);
	} else {
		return NULL;
	}
}

/** Remove topmost file input stack entry.
 *
 * @param finput File input
 */
static void file_input_pop(file_input_t *finput)
{
	list_remove(&finput->top->lstack);
	free(finput->top);

	finput->top = file_input_get_top(finput);
}

/** Destroy file input.
 *
 * @param finput File input
 */
void file_input_destroy(file_input_t *finput)
{
	if (finput == NULL)
		return;

	while (finput->top != NULL)
		file_input_pop(finput);

	free(finput);
}
