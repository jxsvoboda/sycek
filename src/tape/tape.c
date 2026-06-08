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
 * Tape image
 */

#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tape/tape.h>

/** Create tape image.
 *
 * @param rtape Place to store pointer to new tape image
 * @return EOK on success, ENOMEM if out of memory
 */
int tape_create(tape_t **rtape)
{
	tape_t *tape;

	tape = calloc(1, sizeof(tape_t));
	if (tape == NULL)
		return ENOMEM;

	tape->major = 1;
	tape->minor = 1;
	list_initialize(&tape->blocks);
	*rtape = tape;
	return EOK;
}

/** Destroy tape image.
 *
 * @param tape Tape image or @c NULL
 */
void tape_destroy(tape_t *tape)
{
	tape_block_t *block;

	if (tape == NULL)
		return;

	block = tape_block_first(tape);
	while (block != NULL) {
		tape_block_destroy(block);
		block = tape_block_first(tape);
	}

	free(tape);
}

/** Append new block to tape image.
 *
 * @param tape Tape image
 * @param size Block size
 * @param rblock Place to store pointer to new tape block
 * @return EOK on success, ENOMEM if out of memory
 */
int tape_block_append(tape_t *tape, uint16_t size, tape_block_t **rblock)
{
	tape_block_t *block;

	block = calloc(1, sizeof(tape_block_t));
	if (block == NULL)
		return ENOMEM;

	block->data = malloc(size);
	if (block->data == NULL) {
		free(block);
		return ENOMEM;
	}

	block->size = size;
	block->tape = tape;
	block->pause_after = 1000;
	list_append(&block->lblocks, &tape->blocks);
	*rblock = block;
	return EOK;
}

/** Destroy tape block.
 *
 * @param block Tape block
 */
void tape_block_destroy(tape_block_t *block)
{
	list_remove(&block->lblocks);
	free(block->data);
	free(block);
}

/** Get first tape block.
 *
 * @param tape Tape image
 * @return First tape block or @c NULL if there are none
 */
tape_block_t *tape_block_first(tape_t *tape)
{
	link_t *link;

	link = list_first(&tape->blocks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, tape_block_t, lblocks);
}

/** Get next tape block.
 *
 * @param cur Current block
 * @return Next tape block or @c NULL if there are none
 */
tape_block_t *tape_block_next(tape_block_t *cur)
{
	link_t *link;

	link = list_next(&cur->lblocks, &cur->tape->blocks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, tape_block_t, lblocks);
}
