/*
 * Copyright 2024 Jiri Svoboda
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
 * Z80 IR local variable to VR map
 */

#ifndef TYPES_Z80_VARMAP_H
#define TYPES_Z80_VARMAP_H

#include <adt/list.h>

/** Z80 IR local variable to VR map */
typedef struct {
	/** Entries (of z80_varmap_entry_t) */
	list_t entries;
	/** Next free virtual register */
	unsigned next_vr;
} z80_varmap_t;

/** Z80 IR local variable to VR map entry */
typedef struct {
	/** Containing variable map */
	z80_varmap_t *varmap;
	/** Link to @c varmap->entries */
	link_t lentries;
	/** Variable identifier */
	char *ident;
	/** First used virtual register */
	unsigned vr0;
	/** Number of used virtual registers */
	unsigned vrn;
	/** Variable size in bytes */
	unsigned bytes;
} z80_varmap_entry_t;

#endif
