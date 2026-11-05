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
 * Screen drawing.
 */

#include <stdint.h>
#include "scr.h"

enum {
	font_base = 0x3d00u,
	scr_base = 0x4000u
};

static uint8_t scr_col = 0;
static uint8_t scr_row = 0;

/** Compute pointer to screen memory of given characte cell.
 *
 * @param col Character cell column
 * @param row Character cell row
 * @return Pointer to first byte of character cell
 */
static uint8_t *scr_ptr(uint8_t col, uint8_t row)
{
	uint16_t faddr;
	uint16_t addr;

	faddr = scr_base + ((uint16_t)row << 8) + col;

	/* flip address bits */
	addr = (faddr & 0xf81fu) |
	    ((faddr & 0x0700u) << 3) |
	    ((faddr & 0x00e0u) >> 3);

	return (uint8_t *)addr;
}

/** Print character at current screen position.
 *
 * @param c Character
 */
void __scr_putchar(char c)
{
	uint8_t *p = scr_ptr(scr_col, scr_row);
	uint8_t *glyph;
	uint8_t i;

	if (c < 32 || c > 127)
		return;

	glyph = (uint8_t *)(font_base + ((uint16_t)(c - 32) << 3));

	for (i = 0; i < 8; i++) {
		*p = *glyph++;
		p += 32 << 3;
	}

	++scr_col;
}
