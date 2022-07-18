/*
 * Copyright 2022 Jiri Svoboda
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

#ifndef Z80_VARMAP_H
#define Z80_VARMAP_H

#include <types/z80/varmap.h>

extern int z80_varmap_create(z80_varmap_t **);
extern void z80_varmap_destroy(z80_varmap_t *);
extern int z80_varmap_insert(z80_varmap_t *, const char *, unsigned);
extern int z80_varmap_find(z80_varmap_t *, const char *, z80_varmap_entry_t **);
extern void z80_varmap_entry_destroy(z80_varmap_entry_t *);
extern z80_varmap_entry_t *z80_varmap_first(z80_varmap_t *);
extern z80_varmap_entry_t *z80_varmap_next(z80_varmap_entry_t *);

#endif
