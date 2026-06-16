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
 * Binary object
 */

#include <byteorder.h>
#include <merrno.h>
#include <stdlib.h>
#include <object/object.h>
#include <object/reloc.h>
#include <object/section.h>
#include <object/symbol.h>
#include <types/object/file.h>

/** Create binary object structure.
 *
 * @param robject Place to store pointer to new binary object
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_object_create(obj_object_t **robject)
{
	obj_object_t *object;

	object = calloc(1, sizeof(obj_object_t));
	if (object == NULL)
		return ENOMEM;

	list_initialize(&object->sections);
	list_initialize(&object->symbols);
	list_initialize(&object->relocs);
	*robject = object;
	return EOK;
}

/** Destroy binary object.
 *
 * @param object Object or @c NULL
 */
void obj_object_destroy(obj_object_t *object)
{
	obj_section_t *section;
	obj_symbol_t *symbol;
	obj_reloc_t *reloc;

	if (object == NULL)
		return;

	/* Destroy sections. */
	section = obj_section_first(object);
	while (section != NULL) {
		obj_section_destroy(section);
		section = obj_section_first(object);
	}

	/* Destroy symbols. */
	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		obj_symbol_destroy(symbol);
		symbol = obj_symbol_first(object);
	}

	/* Destroy relocations. */
	reloc = obj_reloc_first(object);
	while (reloc != NULL) {
		obj_reloc_destroy(reloc);
		reloc = obj_reloc_first(object);
	}

	free(object);
}

/** Dump binary object.
 *
 * @param object Object
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_object_dump(obj_object_t *object, FILE *outf)
{
	obj_section_t *section;
	obj_symbol_t *symbol;
	obj_reloc_t *reloc;
	int rc;

	rc = fprintf(outf, "Binary object:\n");
	if (rc < 0)
		return EIO;

	section = obj_section_first(object);
	while (section != NULL) {
		rc = obj_section_dump(section, outf);
		if (rc != EOK)
			return rc;

		section = obj_section_next(section);
	}

	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		rc = obj_symbol_dump(symbol, outf);
		if (rc != EOK)
			return rc;

		symbol = obj_symbol_next(symbol);
	}

	reloc = obj_reloc_first(object);
	while (reloc != NULL) {
		rc = obj_reloc_dump(reloc, outf);
		if (rc != EOK)
			return rc;

		reloc = obj_reloc_next(reloc);
	}

	return EOK;
}

/** Copy contents of one object to another object.
 *
 * The destination can be non-empty and the contents are appended
 * at the end. The source object is not modified.
 *
 * @param src Source object
 * @param modidx Source module index
 * @param dest Destination object
 * @return EOK on success or an error code
 */
int obj_object_copy(obj_object_t *src, unsigned modidx, obj_object_t *dest)
{
	obj_section_t *section;
	obj_symbol_t *symbol;
	obj_reloc_t *reloc;
	int rc;

	section = obj_section_first(src);
	while (section != NULL) {
		rc = obj_section_copy(section, modidx, dest);
		if (rc != EOK)
			return rc;

		section = obj_section_next(section);
	}

	symbol = obj_symbol_first(src);
	while (symbol != NULL) {
		rc = obj_symbol_copy(symbol, modidx, dest);
		if (rc != EOK)
			return rc;

		symbol = obj_symbol_next(symbol);
	}

	reloc = obj_reloc_first(src);
	while (reloc != NULL) {
		rc = obj_reloc_copy(reloc, modidx, dest);
		if (rc != EOK)
			return rc;

		reloc = obj_reloc_next(reloc);
	}

	return EOK;
}

/** Save object contents into a raw binary file.
 *
 * @param object Object
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_object_save_bin(obj_object_t *object, FILE *outf)
{
	obj_section_t *section;
	int rc;

	section = obj_section_first(object);
	while (section != NULL) {
		rc = obj_section_save_bin(section, outf);
		if (rc != EOK)
			return rc;

		section = obj_section_next(section);
	}

	return EOK;
}

/** Save object contents into a z80asm compatible map file file.
 *
 * @param object Object
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_object_save_map(obj_object_t *object, FILE *outf)
{
	obj_symbol_t *symbol;
	int rc;

	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		rc = obj_symbol_save_map(symbol, outf);
		if (rc != EOK)
			return rc;

		symbol = obj_symbol_next(symbol);
	}

	return EOK;
}

/** Save binary object into an object file.
 *
 * @param object Object
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_object_save_obj(obj_object_t *object, FILE *outf)
{
	obj_file_hdr_t hdr;
	obj_section_t *section;
	obj_symbol_t *symbol;
	obj_reloc_t *reloc;
	size_t nw;
	int rc;

	hdr.signature = host2uint32_t_le(obj_file_sign);
	hdr.major = host2uint16_t_le(obj_file_major);
	hdr.minor = host2uint16_t_le(obj_file_minor);

	nw = fwrite(&hdr, 1, sizeof(hdr), outf);
	if (nw != sizeof(hdr)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	section = obj_section_first(object);
	while (section != NULL) {
		rc = obj_section_save_obj(section, outf);
		if (rc != EOK)
			return rc;

		section = obj_section_next(section);
	}

	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		rc = obj_symbol_save_obj(symbol, outf);
		if (rc != EOK)
			return rc;

		symbol = obj_symbol_next(symbol);
	}

	reloc = obj_reloc_first(object);
	while (reloc != NULL) {
		rc = obj_reloc_save_obj(reloc, outf);
		if (rc != EOK)
			return rc;

		reloc = obj_reloc_next(reloc);
	}

	return EOK;
}

/** Align size to multiple of obj_file_align.
 *
 * @param size
 * @return Size aligned up
 */
uint32_t obj_align_up(uint32_t size)
{
	return (size + (obj_file_align - 1)) & ~((uint32_t)obj_file_align - 1);
}

