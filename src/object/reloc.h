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
 * Binary object relocation
 */

#ifndef OBJECT_RELOC_H
#define OBJECT_RELOC_H

#include <stdio.h>
#include <types/object/linker.h>
#include <types/object/object.h>
#include <types/object/reloc.h>
#include <types/object/section.h>

extern int obj_reloc_create(obj_object_t *, obj_section_t *,
    obj_reloc_type_t, uint32_t, const char *, uint64_t);
extern void obj_reloc_destroy(obj_reloc_t *);
extern int obj_reloc_dump(obj_reloc_t *, FILE *);
extern int obj_reloc_load_obj(obj_object_t *, FILE *);
extern int obj_reloc_save_obj(obj_reloc_t *, FILE *);
extern int obj_reloc_copy(obj_reloc_t *, unsigned, obj_object_t *);
extern obj_reloc_t *obj_reloc_first(obj_object_t *);
extern obj_reloc_t *obj_reloc_next(obj_reloc_t *);
extern obj_reloc_t *obj_reloc_find(obj_object_t *, obj_section_t *,
    uint32_t);
extern int obj_reloc_process(obj_reloc_t *, obj_linker_flags_t);

#endif
