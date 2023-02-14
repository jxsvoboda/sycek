/*
 * Copyright 2023 Jiri Svoboda
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
 * Symbols
 */

#ifndef Z80TEST_SYMBOLS_H
#define Z80TEST_SYMBOLS_H

#include <stdint.h>
#include "types/z80/z80test/symbols.h"

extern int symbols_create(symbols_t **);
extern void symbols_destroy(symbols_t *);
extern int symbols_insert(symbols_t *, const char *, uint16_t);
extern symbol_t *symbols_first(symbols_t *);
extern symbol_t *symbols_next(symbol_t *);
extern symbol_t *symbols_lookup(symbols_t *, const char *);
extern int symbols_mapfile_load(symbols_t *, const char *);

#endif
