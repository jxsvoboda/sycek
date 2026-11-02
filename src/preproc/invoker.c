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
 * C preprocessor macro invoker
 */

#include <adt/list.h>
#include <assert.h>
#include <charcls.h>
#include <input_buf.h>
#include <lexer.h>
#include <preproc/invoker.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdlib.h>
#include <string.h>

static int preproc_invoker_read(void *, char *, src_pos_t *, size_t, size_t *);

lexer_input_ops_t preproc_invoker_input = {
	.read = preproc_invoker_read
};

static int preproc_invoker_process(preproc_invoker_t *);
static preproc_inv_arg_t *preproc_invoker_arg_first(preproc_invoker_t *);
static preproc_inv_arg_t *preproc_invoker_arg_find(preproc_invoker_t *,
    const char *);
static void preproc_invoker_arg_destroy(preproc_inv_arg_t *);

/** Create preprocessor macro invoker.
 *
 * @param inbuf Input buffer
 * @param rinvoker Place to store new preprocessor macro invoker.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int preproc_invoker_create(preproc_macro_t *macro, preproc_invoker_t **rinvoker)
{
	preproc_invoker_t *invoker = NULL;
	int rc;

	invoker = calloc(1, sizeof(preproc_invoker_t));
	if (invoker == NULL) {
		rc = ENOMEM;
		goto error;
	}

	invoker->macro = macro;
	invoker->pos = macro->rpos;
	list_initialize(&invoker->args);

	rc = preproc_invoker_arg_insert(invoker, "A", "a");
	if (rc != EOK)
		goto error;

	*rinvoker = invoker;
	return EOK;
error:
	preproc_invoker_destroy(invoker);
	return rc;
}

/** Destroy preprocessor macro invoker.
 *
 * @param invoker Preprocessor macro invoker or @c NULL
 */
void preproc_invoker_destroy(preproc_invoker_t *invoker)
{
	preproc_inv_arg_t *arg;

	if (invoker == NULL)
		return;

	arg = preproc_invoker_arg_first(invoker);
	while (arg != NULL) {
		preproc_invoker_arg_destroy(arg);
		arg = preproc_invoker_arg_first(invoker);
	}

	free(invoker);
}

/** Determine if invoker output buffer has available space.
 *
 * @param invoker Preprocessor macro invoker
 * @return @c true iff output buffer has available space.
 */
static bool preproc_invoker_buf_avail(preproc_invoker_t *invoker)
{
	return invoker->out_buf_used < preproc_invoker_buf_size;
}

/** Insert character to preprocessor output buffer.
 *
 * @param invoker Preprocessor invoker
 * @param c Character
 * @param pos Character source code position
 * @return EOK on success or an error code.
 */
static void preproc_invoker_buf_insert(preproc_invoker_t *invoker, char c,
    src_pos_t *pos)
{
	if (invoker->out_buf_used == 0)
		invoker->out_buf_pos = invoker->pos;

	assert(preproc_invoker_buf_avail(invoker));

	invoker->out_buf[invoker->out_buf_used] = c;
	invoker->out_posbuf[invoker->out_buf_used] = *pos;
	++invoker->out_buf_used;
}

/** Do some macro invoking in scanning state.
 *
 * This may or may not write data to the invoker output buffer.
 *
 * @param invoker Preprocessor invoker
 * @return EOK on success or an error code
 */
static int preproc_invoker_process_scan(preproc_invoker_t *invoker)
{
	char c;

	while (invoker->macro->replacement[invoker->macro_off] != '\0') {
		c = invoker->macro->replacement[invoker->macro_off];

		if (is_idbegin(c)) {
			invoker->state = ppis_ident;
			invoker->ident_buf_used = 0;
			break;
		}

		++invoker->macro_off;

		preproc_invoker_buf_insert(invoker, c, &invoker->pos);
		src_pos_fwd_char(&invoker->pos, c);

		if (!preproc_invoker_buf_avail(invoker))
			break;
	}

	return EOK;
}

/** Handle identifier found in the input stream.
 *
 * @param invoker Macro invoker
 * @return EOK on success or an error code
 */
static int preproc_invoker_handle_ident(preproc_invoker_t *invoker)
{
	preproc_inv_arg_t *arg;

	arg = preproc_invoker_arg_find(invoker, invoker->ident_buf);

	if (arg != NULL) {
		invoker->state = ppis_argument;
		invoker->arg_ptr = arg;
		invoker->arg_off = 0;
	} else {
		/* Unknown idntifier. */
		invoker->state = ppis_unkident;
		invoker->ident_buf_off = 0;
	}

	return EOK;
}

/** Do some macro invoking in identifier state.
 *
 * This may or may not write data to the invoker output buffer.
 *
 * @param invoker Preprocessor invoker
 * @return EOK on success or an error code
 */
static int preproc_invoker_process_ident(preproc_invoker_t *invoker)
{
	char c;
	int rc;

	while (true) {
		c = invoker->macro->replacement[invoker->macro_off];

		if (!is_idcnt(c)) {
			invoker->ident_buf[invoker->ident_buf_used] = '\0';
			rc = preproc_invoker_handle_ident(invoker);
			if (rc != EOK)
				return rc;
			break;
		}

		if (invoker->ident_buf_used ==
		    preproc_invoker_ident_buf_size - 1) {
			/* Identifier too long. */
			invoker->state = ppis_unkident;
			invoker->ident_buf_off = 0;
			break;
		}

		invoker->ident_buf[invoker->ident_buf_used++] = c;
		++invoker->macro_off;
		src_pos_fwd_char(&invoker->pos, c);

		if (!preproc_invoker_buf_avail(invoker))
			break;
	}

	return EOK;
}

/** Do some macro invoking in unknown identifier dumping state.
 *
 * This may or may not write data to the expander output buffer.
 *
 * @param invoker Preprocessor macro invoker
 * @return EOK on success or an error code
 */
static int preproc_invoker_process_unkident(preproc_invoker_t *invoker)
{
	char c;

	while (invoker->ident_buf_off < invoker->ident_buf_used) {
		c = invoker->ident_buf[invoker->ident_buf_off++];

		preproc_invoker_buf_insert(invoker, c, &invoker->pos);

		if (!preproc_invoker_buf_avail(invoker))
			break;
	}

	if (invoker->ident_buf_off >= invoker->ident_buf_used)
		invoker->state = ppis_scan;

	return EOK;
}

/** Do some macro invoking in unknown argument state.
 *
 * This may or may not write data to the expander output buffer.
 *
 * @param invoker Preprocessor macro invoker
 * @return EOK on success or an error code
 */
static int preproc_invoker_process_argument(preproc_invoker_t *invoker)
{
	char c;

	while (true) {
		c = invoker->arg_ptr->text[invoker->arg_off];
		if (c == '\0')
			break;

		++invoker->arg_off;
		preproc_invoker_buf_insert(invoker, c, &invoker->pos);

		if (!preproc_invoker_buf_avail(invoker))
			break;
	}

	if (c == '\0')
		invoker->state = ppis_scan;

	return EOK;
}

/** Do some macro invoking.
 *
 * This may or may not write data to the invoker output buffer.
 *
 * @param invoker Preprocessor macro invoker
 * @return EOK on success or an error code
 */
static int preproc_invoker_process(preproc_invoker_t *invoker)
{
	switch (invoker->state) {
	case ppis_scan:
		return preproc_invoker_process_scan(invoker);
	case ppis_ident:
		return preproc_invoker_process_ident(invoker);
	case ppis_unkident:
		return preproc_invoker_process_unkident(invoker);
	case ppis_argument:
		return preproc_invoker_process_argument(invoker);
	}

	return EINVAL;
}

/** Read input from preprocessor invoker.
 *
 * @param arg Argument (preproc_t *)
 * @param buf Character buffer
 * @param posbuf Position buffer
 * @param bsize Size of character buffer
 * @param nread Place to store number of characters read.
 * @return EOK on success or an error code
 */
static int preproc_invoker_read(void *arg, char *buf, src_pos_t *posbuf,
    size_t bsize, size_t *nread)
{
	preproc_invoker_t *invoker = (preproc_invoker_t *)arg;
	size_t nbytes;
	int rc;

	while ((invoker->macro->replacement[invoker->macro_off] != '\0' ||
	    invoker->state != ppis_scan) && preproc_invoker_buf_avail(invoker)) {
		rc = preproc_invoker_process(invoker);
		if (rc != EOK)
			return rc;
	}

	if (bsize < invoker->out_buf_used)
		nbytes = bsize;
	else
		nbytes = invoker->out_buf_used;

	memcpy(buf, invoker->out_buf, nbytes);
	memcpy(posbuf, invoker->out_posbuf, nbytes * sizeof(src_pos_t));
	invoker->out_buf_used -= nbytes;

	*nread = nbytes;

	memcpy(invoker->out_buf, invoker->out_buf + nbytes,
	    invoker->out_buf_used);
	memcpy(invoker->out_posbuf, invoker->out_posbuf + nbytes,
	    invoker->out_buf_used * sizeof(src_pos_t));

	return EOK;
}

/** Insert macro argument into macro invoker.
 *
 * @param invoker Preprocessor macro invoker
 * @param name Argument name
 * @param text Argument text
 *
 * @return EOK on success or an error code
 */
int preproc_invoker_arg_insert(preproc_invoker_t *invoker,
    const char *name, const char *text)
{
	preproc_inv_arg_t *arg;

	arg = calloc(1, sizeof(preproc_inv_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->name = strdup(name);
	if (arg->name == NULL)
		goto error;

	arg->text = strdup(text);
	if (arg->text == NULL)
		goto error;

	arg->invoker = invoker;
	list_append(&arg->largs, &invoker->args);
	return EOK;
error:
	if (arg->name != NULL)
		free(arg->name);
	free(arg);
	return ENOMEM;
}

/** Return fist preprocessor macro argument.
 *
 * @param invoker Preprocessor macro invoker
 * @return First argument or @c NULL if there are none
 */
static preproc_inv_arg_t *preproc_invoker_arg_first(preproc_invoker_t *invoker)
{
	link_t *link;

	link = list_first(&invoker->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, preproc_inv_arg_t, largs);
}

/** Return next preprocessor macro argument.
 *
 * @param cur Current argument
 * @return Next argument or @c NULL if @a cur was the last.
 */
static preproc_inv_arg_t *preproc_invoker_arg_next(preproc_inv_arg_t *cur)
{
	link_t *link;

	link = list_next(&cur->largs, &cur->invoker->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, preproc_inv_arg_t, largs);
}

/** Find preprocessor macro argument by name.
 *
 * @param invoker Preprocessor macro invoker
 * @param name Argument name
 * @return Argument or @c NULL if not found
 */
static preproc_inv_arg_t *preproc_invoker_arg_find(preproc_invoker_t *invoker,
    const char *name)
{
	preproc_inv_arg_t *arg;

	arg = preproc_invoker_arg_first(invoker);
	while (arg != NULL) {
		if (strcmp(arg->name, name) == 0)
			return arg;

		arg = preproc_invoker_arg_next(arg);
	}

	return NULL;
}

/** Destroy preprocessor macro argument.
 *
 * @param invoker Preprocessor macro invoker
 */
static void preproc_invoker_arg_destroy(preproc_inv_arg_t *arg)
{
	list_remove(&arg->largs);
	free(arg->name);
	free(arg->text);
	free(arg);
}
