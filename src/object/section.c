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

#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <object/object.h>
#include <object/section.h>

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
		free(section);
		return ENOMEM;
	}

	section->data = malloc((size_t)section->alloc_len);
	if (section->data == NULL) {
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
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_section_dump(obj_section_t *section, FILE *outf)
{
	int rc;
	size_t i;

	rc = fprintf(outf, "  Section: (length: %u bytes)\n", section->len);
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

/** Append 8-bit value at the end of section.
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

/** Append 8-bit value at the end of section.
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

/** Append 8-bit value at the end of section.
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
