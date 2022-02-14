/*
 * Copyright 2022 Jiri Svoboda
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
 * Labels
 *
 * labels_t tracks the definitions and uses of goto labels when generating
 * code for a procedure.
 */

#include <labels.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>

/** Create new labels structure.
 *
 * @param rlabels Place to store pointer to new labels
 * @return EOK on success, ENOMEM if out of memory
 */
int labels_create(labels_t **rlabels)
{
	labels_t *labels;

	labels = calloc(1, sizeof(labels_t));
	if (labels == NULL)
		return ENOMEM;

	list_initialize(&labels->labels);
	*rlabels = labels;
	return EOK;
}

/** Destroy labels.
 *
 * @param labels Labels or @c NULL
 */
void labels_destroy(labels_t *labels)
{
	label_t *label;

	if (labels == NULL)
		return;

	label = labels_first(labels);
	while (label != NULL) {
		list_remove(&label->llabels);
		free(label);

		label = labels_first(labels);
	}

	free(labels);
}

/** Insert label definition to labels.
 *
 * @param labels Labels
 * @param tident Identifier token
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *        label is already defined
 */
int labels_define_label(labels_t *labels, lexer_tok_t *tident)
{
	label_t *label;

	label = labels_lookup(labels, tident->text);
	if (label != NULL && label->defined) {
		/* Label is already defined */
		return EEXIST;
	}

	if (label != NULL) {
		/* Label is already used, mark it as defined. */
		label->defined = true;
		return EOK;
	}

	label = calloc(1, sizeof(label_t));
	if (label == NULL)
		return ENOMEM;

	label->tident = tident;
	label->labels = labels;
	label->defined = true;
	list_append(&label->llabels, &labels->labels);
	return EOK;
}

/** Insert label use to labels.
 *
 * @param labels Labels
 * @param tident Identifier token
 * @return EOK on success, ENOMEM if out of memory
 */
int labels_use_label(labels_t *labels, lexer_tok_t *tident)
{
	label_t *label;

	label = labels_lookup(labels, tident->text);
	if (label != NULL) {
		/* Label is already defined or used. This is fine. */
		label->used = true;
		return EOK;
	}

	label = calloc(1, sizeof(label_t));
	if (label == NULL)
		return ENOMEM;

	label->tident = tident;
	label->labels = labels;
	label->used = true;
	list_append(&label->llabels, &labels->labels);
	return EOK;
}

/** Get first label.
 *
 * @param labels Labels
 * @return First label or @c NULL if there are no labels
 */
label_t *labels_first(labels_t *labels)
{
	link_t *link;

	link = list_first(&labels->labels);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, label_t, llabels);
}

/** Get next label.
 *
 * @param cur Current label
 * @return Next label or @c NULL if @a cur was the last label
 */
label_t *labels_next(label_t *cur)
{
	link_t *link;

	link = list_next(&cur->llabels, &cur->labels->labels);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, label_t, llabels);
}

/** Look up label by identifier.
 *
 * @param labels Labels
 * @param ident Identifier
 * @return Member or @c NULL if identifier is not found
 */
label_t *labels_lookup(labels_t *labels, const char *ident)
{
	label_t *label;

	label = labels_first(labels);
	while (label != NULL) {
		if (strcmp(label->tident->text, ident) == 0)
			return label;

		label = labels_next(label);
	}

	return NULL;
}
