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
#include <tape/basic_linebuf.h>
#include <tape/maker.h>
#include <tape/tape.h>
#include <types/tape/basic.h>
#include <types/tape/romblock.h>

/** Make ROM tape header.
 *
 * @param ftype File type
 * @param name File name
 * @param dblen Size of data in bytes
 * @param param1 Parameter 1
 * @param param2 Parameter 2
 * @param tape Tape where the header block should be appended
 *
 * @return EOK on success or an error code
 */
static int tape_make_header(rom_ftype_t ftype, const char *name,
    uint16_t dblen, uint16_t param1, uint16_t param2, tape_t *tape)
{
	tape_block_t *block;
	rom_tape_header_t *header;
	size_t name_len;
	uint8_t parity;
	size_t i;
	int rc;

	rc = tape_block_append(tape, sizeof(rom_tape_header_t), &block);
	if (rc != EOK)
		goto error;

	name_len = strlen(name);
	if (name_len > sizeof(header->fname))
		name_len = sizeof(header->fname);

	header = (rom_tape_header_t *)block->data;

	header->flag = bflag_header;
	header->ftype = ftype;
	memset(header->fname, ' ', sizeof(header->fname));
	for (i = 0; i < name_len; i++) {
		if (name[i] >= 32 && name[i] < 127)
			header->fname[i] = name[i];
		else
			header->fname[i] = '_';
	}
	header->dblen = host2uint16_t_le(dblen);
	header->param1 = host2uint16_t_le(param1);
	header->param2 = host2uint16_t_le(param2);

	/* Compute parity byte. */
	parity = 0;
	for (i = 0; i < sizeof(rom_tape_header_t) - 1; i++) {
		parity ^= block->data[i];
	}

	header->parity = parity;
	return EOK;
error:
	return rc;
}

/** Make a program loader.
 *
 * @param name Program file name
 * @param org Address where the code is loaded to and executed from
 * @param tape Tape where the loader should be appended
 *
 * @return EOK on success or an error code
 */
static int tape_make_loader(const char *name, uint16_t org, tape_t *tape)
{
	basic_linebuf_t linebuf;
	tape_block_t *block;
	uint8_t parity;
	uint16_t i;
	uint8_t b;
	int rc;

	basic_linebuf_init(&linebuf);

	/* Line number */
	basic_linebuf_set_lineno(&linebuf, 10);

	/* CLEAR <org> */
	basic_linebuf_append_u8(&linebuf, btt_clear);
	basic_linebuf_append_intlit(&linebuf, org);

	/* : */
	basic_linebuf_append_u8(&linebuf, ':');

	/* LOAD ""CODE */
	basic_linebuf_append_u8(&linebuf, btt_load);
	basic_linebuf_append_u8(&linebuf, '"');
	basic_linebuf_append_u8(&linebuf, '"');
	basic_linebuf_append_u8(&linebuf, btt_code);

	/* : */
	basic_linebuf_append_u8(&linebuf, ':');

	/* RANDOMIZE USR <org> */
	basic_linebuf_append_u8(&linebuf, btt_randomize);
	basic_linebuf_append_u8(&linebuf, btt_usr);
	basic_linebuf_append_intlit(&linebuf, org);

	/* Append CR and fill in length. */
	basic_linebuf_finish(&linebuf);

	/* header block */
	rc = tape_make_header(ftype_program, name, linebuf.len,
	    10, linebuf.len, tape);
	if (rc != EOK)
		return rc;

	/* data block */
	rc = tape_block_append(tape, linebuf.len + 2, &block);
	if (rc != EOK)
		goto error;

	block->data[0] = bflag_data;
	parity = block->data[0];
	for (i = 0; i < linebuf.len; i++) {
		b = linebuf.buf[i];
		parity ^= b;
		block->data[1 + i] = b;
	}

	block->data[1 + linebuf.len] = parity;
	return EOK;
error:
	return rc;
}

/** Make tape image from linked executable.
 *
 * @param object Linked executable object
 * @param name Program name
 * @param rtape Place to store pointer to new tape image
 * @return EOK on success or an error code
 */
int tape_make_from_object(obj_object_t *object, const char *name,
    tape_t **rtape)
{
	obj_section_t *section;
	tape_t *tape = NULL;
	tape_block_t *block;
	uint16_t i;
	uint8_t b;
	uint8_t parity;
	int rc;

	rc = tape_create(&tape);
	if (rc != EOK)
		goto error;

	rc = tape_make_loader(name, 0x8000 /* org */, tape);
	if (rc != EOK)
		goto error;

	section = obj_section_first(object);
	while (section != NULL) {
		/* header block */
		rc = tape_make_header(ftype_bytes, section->name,
		    (uint16_t)section->len, 0x8000 /* org */,
		    32768, tape);
		if (rc != EOK)
			goto error;

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
