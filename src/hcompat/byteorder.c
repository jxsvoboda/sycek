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
 * Byte order
 */

#include <stdint.h>
#include <string.h>
#include "byteorder.h"

/** Convert 16-bit integer from host to little endian.
 *
 * @param w 16-bit integer in host endian
 * @return Value converted to little endian.
 */
uint16_t host2uint16_t_le(uint16_t w)
{
	uint8_t b[2];
	uint16_t le_w;

	b[0] = w & 0xff;
	b[1] = w >> 8;
	memcpy(&le_w, b, sizeof(le_w));
	return le_w;
}

/** Convert 16-bit integer from little endian to host endian.
 *
 * @param w 16-bit integer in little endian
 * @return Value converted to host endian.
 */
uint16_t uint16_t_le2host(uint16_t le_w)
{
	uint8_t b[2];
	uint16_t w;

	memcpy(b, &le_w, sizeof(b));
	w = b[0] | ((uint16_t)b[1] << 8);

	return w;
}

/** Convert 32-bit integer from host to little endian.
 *
 * @param w 32-bit integer in host endian
 * @return Value converted to little endian.
 */
uint32_t host2uint32_t_le(uint32_t dw)
{
	uint8_t b[4];
	uint32_t le_dw;

	b[0] = dw & 0xff;
	b[1] = (dw >> 8) & 0xff;
	b[2] = (dw >> 16) & 0xff;
	b[3] = dw >> 24;

	memcpy(&le_dw, b, sizeof(le_dw));
	return le_dw;
}

/** Convert 32-bit integer from little endian to host endian.
 *
 * @param w 32-bit integer in little endian
 * @return Value converted to host endian.
 */
uint32_t uint32_t_le2host(uint32_t le_dw)
{
	uint8_t b[4];
	uint32_t dw;

	memcpy(b, &le_dw, sizeof(b));
	dw = b[0] | ((uint32_t)b[1] << 8) |
	    ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);

	return dw;
}

/** Convert 64-bit integer from host to little endian.
 *
 * @param w 64-bit integer in host endian
 * @return Value converted to little endian.
 */
uint64_t host2uint64_t_le(uint64_t qw)
{
	uint8_t b[8];
	uint64_t le_qw;

	b[0] = qw & 0xff;
	b[1] = (qw >> 8) & 0xff;
	b[2] = (qw >> 16) & 0xff;
	b[3] = (qw >> 24) & 0xff;
	b[4] = (qw >> 32) & 0xff;
	b[5] = (qw >> 40) & 0xff;
	b[6] = (qw >> 48) & 0xff;
	b[7] = qw >> 56;

	memcpy(&le_qw, b, sizeof(le_qw));
	return le_qw;
}

/** Convert 64-bit integer from little endian to host endian.
 *
 * @param w 64-bit integer in little endian
 * @return Value converted to host endian.
 */
uint64_t uint64_t_le2host(uint64_t le_qw)
{
	uint8_t b[8];
	uint64_t qw;

	memcpy(b, &le_qw, sizeof(b));
	qw = b[0] | ((uint64_t)b[1] << 8) | ((uint64_t)b[2] << 16) |
	    ((uint64_t)b[3] << 24) | ((uint64_t)b[4] << 32) |
	    ((uint64_t)b[5] << 40) | ((uint64_t)b[6] << 48) |
	    ((uint64_t)b[7] << 56);

	return qw;
}
