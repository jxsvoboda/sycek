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

#include <adt/bitset.h>
#include <assert.h>
#include <merrno.h>
#include <stdlib.h>

/** Create bit set.
 *
 * @param nbits Number of bits in bit set
 * @param rbitset Place to store pointer to new bit set
 * @return EOK on success, ENOMEM if out of memory
 */
int bitset_create(uint32_t nbits, bitset_t **rbitset)
{
	bitset_t *bitset;

	bitset = calloc(1, sizeof(bitset_t));
	if (bitset == NULL)
		return ENOMEM;

	bitset->bufsz = (size_t)(nbits / bitset_bpw);
	bitset->buf = calloc(bitset->bufsz, sizeof(uint64_t));
	if (bitset->buf == NULL) {
		free(bitset);
		return ENOMEM;
	}

	*rbitset = bitset;
	return EOK;
}

/** Destroy bit set.
 *
 * @param bitset Bit set
 */
void bitset_destroy(bitset_t *bitset)
{
	free(bitset->buf);
	free(bitset);
}

/** Set bit value in bit set.
 *
 * @param bitset Bit set
 * @param bit Bit number
 * @param val Bit value
 */
void bitset_set(bitset_t *bitset, uint32_t bit, uint8_t val)
{
	size_t bufoff;
	uint8_t bitoff;
	uint64_t mask;

	bufoff = (size_t)(bit / bitset_bpw);
	bitoff = (uint8_t)(bit % bitset_bpw);
	mask = 1 << bitoff;

	assert(bufoff < bitset->bufsz);
	assert((val & ~1) == 0);
	bitset->buf[bufoff] = (bitset->buf[bufoff] & ~mask) |
	    (val << bitoff);
}

/** Get bit value in bit set.
 *
 * @param bitset Bit set
 * @param bit Bit number
 * @return Bit value
 */
uint8_t bitset_get(bitset_t *bitset, uint32_t bit)
{
	size_t bufoff;
	uint8_t bitoff;

	bufoff = (size_t)(bit / bitset_bpw);
	bitoff = (uint8_t)(bit % bitset_bpw);

	assert(bufoff < bitset->bufsz);
	return (bitset->buf[bufoff] >> bitoff) & 1;
}

/** Make bitset empty.
 *
 * @param bitset Bitset
 */
void bitset_empty(bitset_t *bitset)
{
	size_t i;

	for (i = 0; i < bitset->bufsz; i++)
		bitset->buf[i] = 0;
}

/** Compare two bitsets.
 *
 * The two bitsets must have the same number of bits.
 *
 * @param a First bitset
 * @param b Second bitset
 * @return Zero if the bitsets are equal, non-zero otherwise.
 */
int bitset_cmp(bitset_t *a, bitset_t *b)
{
	size_t i;

	assert(a->bufsz == b->bufsz);
	for (i = 0; i < a->bufsz; i++)
		if (a->buf[i] != b->buf[i])
			return 1;

	return 0;
}

/** Copy bitset to another bitset.
 *
 * @param src Source bitset
 * @parma dest Destination bitset
 */
void bitset_copy(bitset_t *src, bitset_t *dest)
{
	size_t i;

	assert(dest->bufsz == src->bufsz);

	for (i = 0; i < dest->bufsz; i++)
		dest->buf[i] = src->buf[i];
}

/** Compute difference of two bitsets.
 *
 * All bitsets must have the same number of bits.
 *
 * @param a First bitset
 * @param b Second bitset
 * @param dest Result bitset
 */
void bitset_union_3(bitset_t *a, bitset_t *b, bitset_t *dest)
{
	size_t i;

	assert(a->bufsz == b->bufsz);
	assert(dest->bufsz == a->bufsz);
	for (i = 0; i < dest->bufsz; i++)
		dest->buf[i] = a->buf[i] | b->buf[i];
}

/** Intersect bitset with another bitset.
 *
 * Both bit sets must have the same number of bits.
 *
 * @param src Source bitset
 * @param dest Bitset being modified
 */
void bitset_isect_2(bitset_t *src, bitset_t *dest)
{
	size_t i;

	assert(dest->bufsz == src->bufsz);
	for (i = 0; i < dest->bufsz; i++)
		dest->buf[i] &= src->buf[i];
}

/** Union bitset with another bitset.
 *
 * Both bit sets must have the same number of bits.
 *
 * @param src Source bitset
 * @param dest Bitset being modified
 */
void bitset_union_2(bitset_t *src, bitset_t *dest)
{
	size_t i;

	assert(dest->bufsz == src->bufsz);
	for (i = 0; i < dest->bufsz; i++)
		dest->buf[i] |= src->buf[i];
}

/** Subtract bitset from another bitset.
 *
 * Both bitsets must have the same number of bits.
 *
 * @param src Source bitset
 * @param dest Destination bitset
 */
void bitset_diff_2(bitset_t *src, bitset_t *dest)
{
	size_t i;

	assert(src->bufsz == dest->bufsz);
	for (i = 0; i < dest->bufsz; i++)
		dest->buf[i] &= ~src->buf[i];
}

/** Compute difference of two bitsets.
 *
 * All bitsets must have the same number of bits.
 *
 * @param a First bitset
 * @param b Second bitset
 * @param dest Result bitset
 */
void bitset_diff_3(bitset_t *a, bitset_t *b, bitset_t *dest)
{
	size_t i;

	assert(a->bufsz == b->bufsz);
	assert(dest->bufsz == a->bufsz);
	for (i = 0; i < dest->bufsz; i++)
		dest->buf[i] = a->buf[i] & ~b->buf[i];
}
