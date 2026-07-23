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
#include <inttypes.h>
#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

/** Comparison function for sorting symbols.
 *
 * @param a Pointer to pointer to symbol
 * @param b Pointer to pointer to symbol
 * @return -1, 0, +1 if a < b, a == b, a > b, respectively.
 */
static int obj_object_symbol_cmp(const void *a, const void *b)
{
	const obj_symbol_t *sa = *(const obj_symbol_t **)a;
	const obj_symbol_t *sb = *(const obj_symbol_t **)b;
	int rv;

	rv = strcmp(sa->section->name, sb->section->name);
	if (rv != 0)
		return rv;

	if (sa->offset < sb->offset)
		return -1;
	if (sa->offset > sb->offset)
		return +1;

	return 0;
}

/** Sort list of symbols by section name, offset.
 *
 * @param object Object
 * @return EOK on success or an error code
 */
int obj_object_sort_symbols(obj_object_t *object)
{
	obj_symbol_t *symbol;
	obj_symbol_t **syms;
	size_t sym_cnt;
	size_t i;

	/* Count symbols. */
	sym_cnt = 0;
	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		++sym_cnt;
		symbol = obj_symbol_next(symbol);
	}

	/* Nothing to do? */
	if (sym_cnt == 0)
		return EOK;

	syms = calloc(sym_cnt, sizeof(obj_symbol_t *));
	if (syms == NULL)
		return ENOMEM;

	/* Write symbol pointers to array. */
	i = 0;
	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		syms[i++] = symbol;
		symbol = obj_symbol_next(symbol);
	}

	/* Sort array. */
	qsort(syms, sym_cnt, sizeof(obj_symbol_t *), obj_object_symbol_cmp);

	/* Unlink symbols from list. */
	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		list_remove(&symbol->lsymbols);
		symbol = obj_symbol_first(object);
	}

	/* Relink symbols in new order. */
	for (i = 0; i < sym_cnt; i++) {
		list_append(&syms[i]->lsymbols, &object->symbols);
	}

	free(syms);
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

/** Load binary object from an object file.
 *
 * @param inf Input file
 * @param modname Module name
 * @param robject Place to store pointer to loaded object
 * @return EOK on success or an error code
 */
int obj_object_load_obj(FILE *inf, const char *modname, obj_object_t **robject)
{
	obj_file_hdr_t hdr;
	obj_object_t *object = NULL;
	obj_file_entry_hdr_t ehdr;
	obj_section_t *section;
	obj_symbol_t *symbol;
	size_t nr;
	int rc;
	int rv;

	rc = obj_object_create(&object);
	if (rc != EOK)
		goto error;

	/* Read object file header. */

	nr = fread(&hdr, 1, sizeof(hdr), inf);
	if (nr != sizeof(hdr)) {
		(void)fprintf(stderr, "Error reading object file header\n");
		rc = EIO;
		goto error;
	}

	if (uint32_t_le2host(hdr.signature) != obj_file_sign) {
		(void)fprintf(stderr, "Invalid object file signature.\n");
		rc = EIO;
		goto error;
	}

	if (uint16_t_le2host(hdr.major) != obj_file_major ||
	    uint16_t_le2host(hdr.minor) != obj_file_minor) {
		(void)fprintf(stderr, "Invalid object file version %" PRIu16
		    ".%" PRIu16 ".\n", uint16_t_le2host(hdr.major),
		    uint16_t_le2host(hdr.minor));
		rc = EIO;
		goto error;
	}

	while (true) {
		nr = fread(&ehdr, 1, sizeof(ehdr), inf);
		if (nr == 0)
			break;

		if (nr != sizeof(ehdr)) {
			(void)fprintf(stderr, "Error reading object file "
			    "entry header.\n");
			rc = EIO;
			goto error;
		}

		switch (uint32_t_le2host(ehdr.etype)) {
		case obj_file_ereloc:
			rc = obj_reloc_load_obj(object, inf);
			break;
		case obj_file_esection:
			rc = obj_section_load_obj(object, inf, modname,
			    &section);
			(void)section;
			break;
		case obj_file_esymbol:
			rc = obj_symbol_load_obj(object, inf, &symbol);
			(void)symbol;
			break;
		default:
			/* Skip over unknown entry. */
			rv = fseek(inf, (long)uint32_t_le2host(ehdr.esize),
			    SEEK_CUR);
			if (rv < 0) {
				(void)fprintf(stderr, "Error reading unknown "
				    "object file entry.\n");
				rc = EIO;
				goto error;
			}
			rc = EOK;
			break;
		}

		if (rc != EOK)
			goto error;
	}

	*robject = object;
	return EOK;
error:
	obj_object_destroy(object);
	return rc;
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
