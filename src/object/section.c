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

#include <assert.h>
#include <byteorder.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <object/object.h>
#include <object/section.h>
#include <object/symbol.h>
#include <types/object/file.h>

/** Create binary object section structure.
 *
 * @param object Containing object
 * @param name Section name
 * @param rsection Place to store pointer to new binary object section
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_section_create(obj_object_t *object, const char *name,
    obj_section_t **rsection)
{
	obj_section_t *section;

	section = calloc(1, sizeof(obj_section_t));
	if (section == NULL)
		return ENOMEM;

	section->object = object;
	section->len = 0;
	section->alloc_len = 16;

	section->name = strdup(name);
	if (section->name == NULL) {
		(void)fprintf(stderr, "Out of memory.\n");
		free(section);
		return ENOMEM;
	}

	section->data = malloc((size_t)section->alloc_len);
	if (section->data == NULL) {
		(void)fprintf(stderr, "Out of memory.\n");
		free(section->name);
		free(section);
		return ENOMEM;
	}

	list_append(&section->lsections, &object->sections);
	*rsection = section;
	return EOK;
}

/** Destroy binary object section.
 *
 * @param section Section or @c NULL
 */
void obj_section_destroy(obj_section_t *section)
{
	if (section == NULL)
		return;

	list_remove(&section->lsections);
	free(section->name);
	free(section->data);
	free(section);
}

/** Dump binary object section.
 *
 * @param section Section
 * @param outf Output file
 * @return EOK on success, EIO on I/O error
 */
int obj_section_dump(obj_section_t *section, FILE *outf)
{
	int rc;
	size_t i;

	rc = fprintf(outf, "  Section: (length: %u bytes base:0x%x)\n", section->len, section->base_addr);
	if (rc < 0)
		return EIO;

	i = 0;
	while (i < section->len) {
		rc = fprintf(outf, "    %04zx:", i);
		if (rc < 0)
			return EIO;

		do {
			rc = fprintf(outf, " %02x", section->data[i]);
			if (rc < 0)
				return EIO;
			++i;
		} while (i % 16 != 0 && i < section->len);

		rc = fputc('\n', outf);
		if (rc < 0)
			return EIO;
	}

	return EOK;
}

/** Load binary object section from object file.
 *
 * #param object Object
 * @param inf Output file
 * @param rsection Place to store pointer to loaded section
 * @return EOK on success or an error code
 */
int obj_section_load_obj(obj_object_t *object, FILE *inf,
    obj_section_t **rsection)
{
	obj_section_t *section = NULL;
	obj_file_section_t sect;
	uint32_t nsize;
	uint32_t data_len;
	char *name = NULL;
	void *data;
	size_t nr;
	int rc;

	/* Read section header */

	nr = fread(&sect, 1, sizeof(sect), inf);
	if (nr != sizeof(sect)) {
		(void)fprintf(stderr, "Error reading section header.\n");
		rc = EIO;
		goto error;
	}

	/* Read section name */
	nsize = uint32_t_le2host(sect.name_len);
	name = calloc((size_t)nsize + 1, 1);
	if (name == NULL) {
		(void)fprintf(stderr, "Out of memory.\n");
		rc = ENOMEM;
		goto error;
	}

	nr = fread(name, 1, (size_t)nsize, inf);
	if (nr != nsize) {
		(void)fprintf(stderr, "Error reading section name.\n");
		rc = EIO;
		goto error;
	}

	rc = obj_section_create(object, name, &section);
	if (rc != EOK)
		goto error;

	free(name);
	name = NULL;

	data_len = uint32_t_le2host(sect.data_len);
	data = malloc((size_t)obj_align_up(data_len));
	if (data == NULL) {
		(void)fprintf(stderr, "Out of memory.\n");
		goto error;
	}

	free(section->data);
	section->data = data;
	section->len = data_len;
	section->alloc_len = obj_align_up(data_len);

	section->base_addr = uint32_t_le2host(sect.base_addr);

	/* Read section data including padding */
	nr = fread(section->data, 1, (size_t)section->alloc_len, inf);
	if (nr != section->alloc_len) {
		(void)fprintf(stderr, "Error reading section data.\n");
		return EIO;
	}

	*rsection = section;
	return EOK;
error:
	obj_section_destroy(section);
	if (name != NULL)
		free(name);
	return rc;
}

/** Save binary object section raw data into a file.
 *
 * @param section Section
 * @param outf Output file
 * @return EOK on success, EIO on I/O error.
 */
int obj_section_save_bin(obj_section_t *section, FILE *outf)
{
	size_t nw;

	nw = fwrite(section->data, 1, (size_t)section->len, outf);
	if (nw < section->len)
		return EIO;

	return EOK;
}

/** Save binary object section into object file.
 *
 * @param section Section
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_section_save_obj(obj_section_t *section, FILE *outf)
{
	obj_file_entry_hdr_t hdr;
	obj_file_section_t sect;
	uint32_t dsize;
	uint32_t size;
	uint32_t nsize;
	uint8_t pad[obj_file_align];
	size_t nw;

	/* Entry header */
	nsize = obj_align_up(strlen(section->name));
	dsize = sizeof(obj_file_section_t) + nsize + section->len;
	size = obj_align_up(dsize);

	hdr.etype = host2uint32_t_le(obj_file_esection);
	hdr.esize = host2uint32_t_le(size);

	nw = fwrite(&hdr, 1, sizeof(hdr), outf);
	if (nw != sizeof(hdr)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Section header */

	sect.name_len = host2uint32_t_le(nsize);
	sect.data_len = host2uint32_t_le(section->len);
	sect.base_addr = host2uint32_t_le(section->base_addr);
	sect.pad = 0;

	nw = fwrite(&sect, 1, sizeof(sect), outf);
	if (nw != sizeof(sect)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Section name */
	nw = fwrite(section->name, 1, strlen(section->name), outf);
	if (nw != strlen(section->name)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Padding */
	memset(pad, 0, sizeof(pad));
	nw = fwrite(pad, 1, (size_t)nsize - strlen(section->name), outf);
	if (nw != nsize - strlen(section->name)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Section data */
	nw = fwrite(section->data, 1, (size_t)section->len, outf);
	if (nw != section->len) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Padding */
	nw = fwrite(pad, 1, (size_t)(size - dsize), outf);
	if (nw != size - dsize) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	return EOK;
}

/** Get section name with module index appended.
 *
 * This is used when copying modules into a single object. The sections
 * are then renamed by appending 'at-sign' + modidx.
 *
 * @param section Original section
 * @param modidx Module index
 * @param rname Place to store pointer to tagged name.
 * @return EOK on success or an error code
 */
int obj_section_tagged_name(obj_section_t *section, unsigned modidx,
    char **rname)
{
	char *dname = NULL;
	int rv;

	rv = asprintf(&dname, "%s@%u", section->name, modidx);
	if (rv < 0)
		return ENOMEM;

	*rname = dname;
	return EOK;
}

uint32_t obj_section_get_idx(obj_section_t *section)
{
	uint32_t idx;
	obj_section_t *sp;

	idx = 0;
	sp = obj_section_first(section->object);
	while (sp != NULL && sp != section) {
		++idx;
		sp = obj_section_next(sp);
	}

	assert(sp == section);
	return idx;
}

/** Copy binary object section to another object.
 *
 * @param section Section
 * @param modidx Source module index
 * @param dest Destination object
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_section_copy(obj_section_t *section, unsigned modidx,
    obj_object_t *dest)
{
	int rc;
	obj_section_t *dsection = NULL;
	char *dname = NULL;
	void *data;

	rc = obj_section_tagged_name(section, modidx, &dname);
	if (rc != EOK)
		return rc;

	rc = obj_section_create(dest, dname, &dsection);
	if (rc != EOK) {
		free(dname);
		return rc;
	}

	free(dname);

	data = malloc((size_t)section->alloc_len);
	if (data == NULL) {
		obj_section_destroy(dsection);
		return rc;
	}

	free(dsection->data);
	dsection->data = data;
	dsection->alloc_len = section->alloc_len;
	dsection->len = section->len;

	memcpy(dsection->data, section->data, (size_t)section->len);
	return EOK;
}

/** Merge section into another section.
 *
 * The @a src section is merged into (appended to) @a dest, section
 * @a src is removed.
 *
 * @param dest Destination section (extended)
 * @param src Source section (removed)
 * @return EOK on success or an error code
 */
int obj_section_merge(obj_section_t *dest, obj_section_t *src)
{
	size_t new_len;
	obj_symbol_t *symbol;
	void *data;

	new_len = (size_t)(dest->len + src->len);

	/* Enlarge destination section buffer. */
	data = realloc(dest->data, new_len);
	if (data == NULL)
		return ENOMEM;

	/* Copy data. */
	dest->data = data;
	memcpy(dest->data + (size_t)dest->len, src->data, (size_t)src->len);

	/* Move symbols. */
	symbol = obj_symbol_first(dest->object);
	while (symbol != NULL) {
		if (symbol->section == src) {
			symbol->section = dest;
			symbol->offset += dest->len;
		}
		symbol = obj_symbol_next(symbol);
	}

	dest->len = new_len;
	dest->alloc_len = new_len;

	obj_section_destroy(src);
	return EOK;
}

/** Compare base names of two sections.
 *
 * Base name is the name without the traling 'at'-module-index tag.
 *
 * @param sa First section
 * @param sb Second section
 *
 * @return Zero if the two base names are the same, non-zero otherwise.
 */
int obj_section_basename_cmp(obj_section_t *sa, obj_section_t *sb)
{
	char *a;
	char *b;

	a = sa->name;
	b = sb->name;

	while (*a != '\0' && *a != '@' && *b != '\0' && *b != '@' &&
	    *a == *b) {
		++a;
		++b;
	}

	return (int)(*a - *b);
}

/** Remove module index tag from section name.
 *
 * E.g. 'common@1' -> 'common'.
 *
 * @param section Section to be modified.
 * @return EOK on success or an error code
 */
int obj_section_remove_tag(obj_section_t *section)
{
	char *p;

	p = strchr(section->name, '@');
	if (p != NULL)
		*p = '\0';
	return EOK;
}

/** Get first section in object.
 *
 * @param object Object
 * @return First section or @c NULL if there are none.
 */
obj_section_t *obj_section_first(obj_object_t *object)
{
	link_t *link;

	link = list_first(&object->sections);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_section_t, lsections);
}

/** Get next section in object.
 *
 * @param cur Current section
 * @return Next section or @c NULL if @a cur is the last section.
 */
obj_section_t *obj_section_next(obj_section_t *cur)
{
	link_t *link;

	link = list_next(&cur->lsections, &cur->object->sections);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_section_t, lsections);
}

/** Find section in object by index.
 *
 * @param object Object
 * @param idx Section index (0 = first, 1 = second, ...)
 * @return Section with the index @a idx or @c NULL if not found.
 */
obj_section_t *obj_section_by_idx(obj_object_t *object, uint32_t idx)
{
	obj_section_t *section;

	section = obj_section_first(object);
	while (section != NULL && idx > 0) {
		--idx;
		section = obj_section_next(section);
	}

	return section;
}

/** Find section in object by name.
 *
 * @param object Object
 * @param name Section name
 * @return Section matching @a name or @c NULL if there are none.
 */
obj_section_t *obj_section_by_name(obj_object_t *object, const char *name)
{
	obj_section_t *section;

	section = obj_section_first(object);
	while (section != NULL) {
		if (strcmp(section->name, name) == 0)
			return section;

		section = obj_section_next(section);
	}

	/* Not found. */
	return NULL;
}

/** Append 8-bit value at the end of section.
 *
 * @param section Section
 * @param value Value
 * @return EOK on success or an error code
 */
int obj_section_append_u8(obj_section_t *section, uint8_t value)
{
	void *ptr;

	/* Need to allocate more memory? */
	if (section->len >= section->alloc_len) {
		section->alloc_len *= 2;
		ptr = realloc(section->data, (size_t)(section->alloc_len * 2));
		if (ptr == NULL)
			return ENOMEM;

		section->data = ptr;
		section->alloc_len = section->alloc_len * 2;
	}

	section->data[(size_t)(section->len++)] = value;
	return EOK;
}

/** Append 16-bit value at the end of section.
 *
 * @param section Section
 * @param value Value
 * @return EOK on success or an error code
 */
int obj_section_append_u16le(obj_section_t *section, uint16_t value)
{
	int rc;

	rc = obj_section_append_u8(section, value & 0xff);
	if (rc != EOK)
		return rc;

	rc = obj_section_append_u8(section, value >> 8);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Append 32-bit value at the end of section.
 *
 * @param section Section
 * @param value Value
 * @return EOK on success or an error code
 */
int obj_section_append_u32le(obj_section_t *section, uint32_t value)
{
	int rc;

	rc = obj_section_append_u16le(section, value & 0xffffl);
	if (rc != EOK)
		return rc;

	rc = obj_section_append_u16le(section, value >> 16);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Append 64-bit value at the end of section.
 *
 * @param section Section
 * @param value Value
 * @return EOK on success or an error code
 */
int obj_section_append_u64le(obj_section_t *section, uint64_t value)
{
	int rc;

	rc = obj_section_append_u32le(section, value & 0xffffffffll);
	if (rc != EOK)
		return rc;

	rc = obj_section_append_u32le(section, value >> 32);
	if (rc != EOK)
		return rc;

	return EOK;
}

/** Write 8-bit value at the specified offset in a section.
 *
 * @param section Section
 * @param offset Offset
 * @param value Value
 * @return EOK on success or an error code
 */
int obj_section_write_u8(obj_section_t *section, uint32_t offset,
    uint8_t value)
{
	/* Range check. */
	if (offset >= section->len)
		return EINVAL;

	section->data[(size_t)offset] = value;
	return EOK;
}

/** Write 16-bit value at the specified offset in a section.
 *
 * @param section Section
 * @param offset Offset
 * @param value Value
 * @return EOK on success or an error code
 */
int obj_section_write_u16le(obj_section_t *section, uint32_t offset,
    uint16_t value)
{
	int rc;

	rc = obj_section_write_u8(section, offset, value & 0xff);
	if (rc != EOK)
		return rc;

	rc = obj_section_write_u8(section, offset + 1, value >> 8);
	if (rc != EOK)
		return rc;

	return EOK;
}
