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
 * Tape maker
 */

#include <byteorder.h>
#include <merrno.h>
#include <object/object.h>
#include <object/section.h>
#include <string.h>
#include <tape/maker.h>
#include <tape/tape.h>
#include <types/tape/romblock.h>

/** Make tape image from linked executable.
 *
 * @param object Linked executable object
 * @param rtape Place to store pointer to new tape image
 * @return EOK on success or an error code
 */
int tape_make_from_object(obj_object_t *object, tape_t **rtape)
{
	obj_section_t *section;
	tape_t *tape = NULL;
	rom_tape_header_t *header;
	tape_block_t *block;
	uint16_t i;
	uint8_t b;
	uint8_t parity;
	int rc;

	rc = tape_create(&tape);
	if (rc != EOK)
		goto error;

	section = obj_section_first(object);
	while (section != NULL) {

		/* header block */
		rc = tape_block_append(tape, sizeof(rom_tape_header_t), &block);
		if (rc != EOK)
			goto error;

		header = (rom_tape_header_t *)block->data;

		header->flag = bflag_header;
		header->ftype = ftype_bytes;
		memset(header->fname, ' ', sizeof(header->fname));
		header->fname[0] = 't';
		header->fname[1] = 'e';
		header->fname[2] = 's';
		header->fname[3] = 't';
		header->dblen = host2uint16_t_le((uint16_t)section->len);
		header->param1 = host2uint16_t_le(0x8000); /* address */
		header->param2 = host2uint16_t_le(32768); /* always 32768 */

		parity = 0;
		for (i = 0; i < sizeof(rom_tape_header_t) - 1; i++) {
			parity ^= block->data[i];
		}

		header->parity = parity;

		/* data block */
		rc = tape_block_append(tape, section->len + 2, &block);
		if (rc != EOK)
			goto error;

		block->data[0] = bflag_data;
		parity = block->data[0];
		for (i = 0; i < (uint16_t)section->len; i++) {
			b = section->data[i];
			parity ^= b;
			block->data[1 + i] = b;
		}

		block->data[1 + (size_t)section->len] = parity;

		section = obj_section_next(section);
	}

	*rtape = tape;
	return EOK;
error:
	tape_destroy(tape);
	return rc;
}
