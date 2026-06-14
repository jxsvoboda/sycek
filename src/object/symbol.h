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
 * Binary object symbol
 */

#ifndef OBJECT_SYMBOL_H
#define OBJECT_SYMBOL_H

#include <stdio.h>
#include <types/object/object.h>
#include <types/object/section.h>
#include <types/object/symbol.h>

extern int obj_symbol_create(obj_object_t *, const char *, obj_section_t *,
    uint32_t, uint32_t, obj_symbol_t **);
extern void obj_symbol_destroy(obj_symbol_t *);
extern int obj_symbol_dump(obj_symbol_t *, FILE *);
extern int obj_symbol_save_map(obj_symbol_t *, FILE *);
extern int obj_symbol_copy(obj_symbol_t *, unsigned, obj_object_t *);
extern obj_symbol_t *obj_symbol_first(obj_object_t *);
extern obj_symbol_t *obj_symbol_next(obj_symbol_t *);
extern obj_symbol_t *obj_symbol_find(obj_object_t *, const char *);

#endif
