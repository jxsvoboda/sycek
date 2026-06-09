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

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <tape/basic_linebuf.h>

/** Initialize BASIC line buffer.
 *
 * @param linebuf Line buffer
 */
void basic_linebuf_init(basic_linebuf_t *linebuf)
{
	memset(linebuf->buf, 0, sizeof(linebuf->buf));
	linebuf->len = 4;
}

/** Finish BASIC line buffer.
 *
 * Appends a CR at the end of the line and fills in line length
 *
 * @param linebuf Line buffer
 */
void basic_linebuf_finish(basic_linebuf_t *linebuf)
{
	uint16_t llen;

	/* carriage return */
	basic_linebuf_append_u8(linebuf, 0x0D);

	llen = linebuf->len - 4;
	linebuf->buf[2] = llen & 0xff;
	linebuf->buf[3] = llen >> 16;
}

/** Set line number.
 *
 * @param linebuf Line buffer
 * @param val Line number (1-9999)
 */
void basic_linebuf_set_lineno(basic_linebuf_t *linebuf, uint16_t val)
{
	linebuf->buf[0] = val >> 8;
	linebuf->buf[1] = val & 0xff;
}

/** Append byte at the end of line.
 *
 * @param linebuf Line buffer
 * @param val Byte value
 */
void basic_linebuf_append_u8(basic_linebuf_t *linebuf, uint8_t val)
{
	assert(linebuf->len < sizeof(linebuf->buf));
	linebuf->buf[linebuf->len++] = val;
}

/** Append integer literal at the end of line.
 *
 * @param linebuf Line buffer
 * @param val Integer value
 */
void basic_linebuf_append_intlit(basic_linebuf_t *linebuf, uint16_t val)
{
	uint16_t div;
	uint16_t v;
	uint8_t q;

	div = 1;
	while (div * 10 > div && div * 10 <= val) {
		div = div * 10;
	}

	v = val;
	while (div > 1) {
		q = (uint8_t)(v / div);
		assert(q < 10);
		basic_linebuf_append_u8(linebuf, '0' + q);
		v = v % div;
		div = div / 10;
	}

	assert(v < 10);
	basic_linebuf_append_u8(linebuf, '0' + v);

	/* Floating point number */
	basic_linebuf_append_u8(linebuf, 0x0e);
	basic_linebuf_append_u8(linebuf, 0x00);
	basic_linebuf_append_u8(linebuf, 0x00);
	basic_linebuf_append_u8(linebuf, val & 0xff);
	basic_linebuf_append_u8(linebuf, val >> 8);
	basic_linebuf_append_u8(linebuf, 0x00);
}
