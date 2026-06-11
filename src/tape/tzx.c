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
 * TZX tape format
 */

#include <byteorder.h>
#include <merrno.h>
#include <stdio.h>
#include <string.h>
#include <tape/tape.h>
#include <tape/tzx.h>
#include <types/tape/tzx.h>

/** Save tape block to TZX file.
 *
 * @param block Tape block
 * @param outf Output file
 */
static int tape_block_save_tzx(tape_block_t *block, FILE *outf)
{
	tzx_block_data_t bdata;
	uint8_t btype;
	size_t nw;

	btype = (uint8_t)tzxb_data;
	nw = fwrite(&btype, 1, sizeof(btype), outf);
	if (nw < sizeof(btype))
		return EIO;

	bdata.pause_after = host2uint16_t_le(block->pause_after);
	bdata.data_len = host2uint16_t_le(block->size);

	nw = fwrite(&bdata, 1, sizeof(bdata), outf);
	if (nw < sizeof(bdata))
		return EIO;

	nw = fwrite(block->data, 1, block->size, outf);
	if (nw < block->size)
		return EIO;

	return EOK;
}

/** Save TZX header.
 *
 * @param tape Tape
 * @param outf Output file
 * @return EOK on success or an error code.
 */
static int tape_save_tzx_header(tape_t *tape, FILE *outf)
{
	tzx_header_t header;
	size_t nw;

	memcpy(header.signature, "ZXTape!", sizeof(header.signature));
	header.eof_mark = 0x1a;
	header.major = tape->major;
	header.minor = tape->minor;

	nw = fwrite(&header, 1, sizeof(header), outf);
	if (nw < sizeof(header))
		return EIO;

	return EOK;
}

/** Save tape image to TZX file.
 *
 * @param tape Tape image
 * @param fname Output file name
 * @return EOK on success or an error code
 */
int tape_save_tzx(tape_t *tape, const char *fname)
{
	tape_block_t *block;
	FILE *outf;
	int rv;
	int rc;

	outf = fopen(fname, "wb");
	if (outf == NULL)
		return EIO;

	rc = tape_save_tzx_header(tape, outf);
	if (rc != EOK)
		goto error;

	block = tape_block_first(tape);
	while (block != NULL) {
		rc = tape_block_save_tzx(block, outf);
		block = tape_block_next(block);
	}

	rv = fflush(outf);
	if (rv < 0) {
		rc = EIO;
		goto error;
	}

	(void)fclose(outf);
	return EOK;
error:
	(void)fclose(outf);
	return rc;
}
