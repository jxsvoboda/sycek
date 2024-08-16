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

#ifndef Z80_ARGLOC_H
#define Z80_ARGLOC_H

#include <types/z80/argloc.h>
#include <types/z80/vainfo.h>

extern int z80_argloc_create(bool, z80_argloc_t **);
extern void z80_argloc_destroy(z80_argloc_t *);
extern int z80_argloc_alloc(z80_argloc_t *, const char *, unsigned,
    z80_argloc_entry_t **);
extern int z80_argloc_find(z80_argloc_t *, const char *, z80_argloc_entry_t **);
extern void z80_argloc_entry_destroy(z80_argloc_entry_t *);
extern z80_argloc_entry_t *z80_argloc_first(z80_argloc_t *);
extern z80_argloc_entry_t *z80_argloc_next(z80_argloc_entry_t *);
extern void z80_argloc_r16_part_to_r(z80ic_r16_t, z80_argloc_rp_t,
    z80ic_reg_t *);
extern void z80_argloc_entry_vainfo(z80_argloc_entry_t *, z80_vainfo_t *);

#endif
