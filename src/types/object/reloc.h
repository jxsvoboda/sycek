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

#ifndef TYPES_OBJECT_RELOC_H
#define TYPES_OBJECT_RELOC_H

#include <adt/list.h>
#include <stdint.h>

/** Object relocation type */
typedef enum {
	/** 16-bit symbol value + addend */
	objr_sa16 = 1,
	/** 16-bit base address + addend */
	objr_rela16 = 2,
	/** 8-bit relative jump */
	objr_rj8 = 3
} obj_reloc_type_t;

/** Object relocation */
typedef struct obj_reloc {
	/** Containing object */
	struct obj_object *object;
	/** Link to @c object->relocs */
	link_t lrelocs;
	/** Section where the relocation is located */
	struct obj_section *section;
	/** Relocation type */
	obj_reloc_type_t rtype;
	/** Relocation offset within section */
	uint32_t offset;
	/** Referenced symbol name */
	char *sym_name;
	/** Addend */
	uint64_t addend;
} obj_reloc_t;

#endif
