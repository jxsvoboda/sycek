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
 * BASIC line buffer
 */

#ifndef TAPE_BASIC_LINEBUF_H
#define TAPE_BASIC_LINEBUF_H

#include <stdint.h>
#include <types/tape/basic_linebuf.h>

extern void basic_linebuf_init(basic_linebuf_t *);
extern void basic_linebuf_finish(basic_linebuf_t *);
extern void basic_linebuf_set_lineno(basic_linebuf_t *, uint16_t);
extern void basic_linebuf_append_u8(basic_linebuf_t *, uint8_t);
extern void basic_linebuf_append_intlit(basic_linebuf_t *, uint16_t);

#endif
