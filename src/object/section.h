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
 * Binary object section
 */

#ifndef OBJECT_SECTION_H
#define OBJECT_SECTION_H

#include <stdint.h>
#include <stdio.h>
#include <types/object/object.h>
#include <types/object/section.h>

extern int obj_section_create(obj_object_t *, const char *, obj_section_t **);
extern void obj_section_destroy(obj_section_t *);
extern int obj_section_dump(obj_section_t *, FILE *);
extern int obj_section_save_bin(obj_section_t *, FILE *);
extern int obj_section_copy(obj_section_t *, unsigned, obj_object_t *);
extern int obj_section_merge(obj_section_t *, obj_section_t *);
extern int obj_section_remove_tag(obj_section_t *);
extern int obj_section_basename_cmp(obj_section_t *, obj_section_t *);
extern int obj_section_tagged_name(obj_section_t *, unsigned, char **);
extern obj_section_t *obj_section_first(obj_object_t *);
extern obj_section_t *obj_section_next(obj_section_t *);
extern obj_section_t *obj_section_by_name(obj_object_t *, const char *);
extern int obj_section_append_u8(obj_section_t *, uint8_t);
extern int obj_section_append_u16le(obj_section_t *, uint16_t);
extern int obj_section_append_u32le(obj_section_t *, uint32_t);
extern int obj_section_append_u64le(obj_section_t *, uint64_t);

extern int obj_section_write_u8(obj_section_t *, uint32_t, uint8_t);
extern int obj_section_write_u16le(obj_section_t *, uint32_t, uint16_t);

#endif
