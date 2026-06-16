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
 * Binary object file
 */

#ifndef TYPES_OBJECT_FILE_H
#define TYPES_OBJECT_FILE_H

enum {
	/** Object file signature 'ObjS' */
	obj_file_sign = 0x536a624ful,
	obj_file_major = 1,
	obj_file_minor = 0
};

enum {
	obj_file_align = 8
};

enum {
	/** Relocation entry 'RELA' */
	obj_file_ereloc = 0x414c4552ul,
	/** Section entry 'SECT' */
	obj_file_esection = 0x54434553ul,
	/** Symbol entry 'SYMB' */
	obj_file_esymbol = 0x424d5953ul
};

/** Object file header. */
typedef struct {
	/** Object file signature */
	uint32_t signature;
	uint16_t major;
	uint16_t minor;
} __attribute__((packed)) obj_file_hdr_t;

/** Object file entry header. */
typedef struct {
	/** Entry type */
	uint32_t etype;
	/** Entry size */
	uint32_t esize;
} __attribute__((packed)) obj_file_entry_hdr_t;

/** Object file section header */
typedef struct {
	/** Section name length*/
	uint32_t name_len;
	/** Section data length */
	uint32_t data_len;
	/** Base address */
	uint32_t base_addr;
	/** Padding */
	uint32_t pad;
} obj_file_section_t;

/** Object file symbol header */
typedef struct {
	/** Symbol name length */
	uint32_t name_len;
	/** Index of section the symbol belongs to */
	uint32_t section_idx;
	/** Symbol offset within section */
	uint32_t offset;
	/** Symbol size */
	uint32_t size;
} __attribute__((packed)) obj_file_symbol_t;

/** Object file relocation header */
typedef struct {
	/** Index of section where the relocation is located */
	uint32_t section_idx;
	/** Relocation type */
	uint32_t rtype;
	/** Relocation offset within section */
	uint32_t offset;
	/** Referenced symbol name length */
	uint32_t sym_name_len;
	/** Addend */
	uint64_t addend;
}  __attribute__((packed)) obj_file_reloc_t;

#endif
