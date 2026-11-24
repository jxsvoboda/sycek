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
 * Bit set
 */

#ifndef ADT_BITSET_H
#define ADT_BITSET_H

#include <stdint.h>
#include <types/adt/bitset.h>

extern int bitset_create(uint32_t, bitset_t **);
extern void bitset_destroy(bitset_t *);
extern void bitset_set(bitset_t *, uint32_t, uint8_t);
extern uint8_t bitset_get(bitset_t *, uint32_t);
extern void bitset_empty(bitset_t *);
extern int bitset_cmp(bitset_t *, bitset_t *);
extern void bitset_copy(bitset_t *, bitset_t *);
extern void bitset_diff_2(bitset_t *, bitset_t *);
extern void bitset_diff_3(bitset_t *, bitset_t *, bitset_t *);
extern void bitset_isect_2(bitset_t *, bitset_t *);
extern void bitset_union_2(bitset_t *, bitset_t *);
extern void bitset_union_3(bitset_t *, bitset_t *, bitset_t *);

#endif
