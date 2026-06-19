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
 * Binary object linker
 */

#include <adt/list.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <object/linker.h>
#include <object/object.h>
#include <object/reloc.h>
#include <object/section.h>

static obj_linker_src_t *obj_linker_src_first(obj_linker_t *);
static obj_linker_src_t *obj_linker_src_next(obj_linker_src_t *);
static void obj_linker_src_destroy(obj_linker_src_t *);

/** Create binary linker.
 *
 * @param lflags Linker flags
 * @param rlinker Place to store pointer to new binary object linker
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_linker_create(obj_linker_flags_t lflags, obj_linker_t **rlinker)
{
	obj_linker_t *linker;

	linker = calloc(1, sizeof(obj_linker_t));
	if (linker == NULL)
		return ENOMEM;

	linker->flags = lflags;
	list_initialize(&linker->sources);
	*rlinker = linker;
	return EOK;
}

/** Destroy binary object linker.
 *
 * @param linker Linker or @c NULL
 */
void obj_linker_destroy(obj_linker_t *linker)
{
	obj_linker_src_t *src;

	if (linker == NULL)
		return;

	src = obj_linker_src_first(linker);
	while (src != NULL) {
		obj_linker_src_destroy(src);
		src = obj_linker_src_first(linker);
	}

	free(linker);
}

/** Add source object to linker.
 *
 * @param linker Linker
 * @return EOK on success or an error code
 */
int obj_linker_add_src(obj_linker_t *linker, obj_object_t *src)
{
	obj_linker_src_t *source;

	source = calloc(1, sizeof(obj_linker_src_t));
	if (source == NULL)
		return ENOMEM;

	source->linker = linker;
	source->object = src;
	list_append(&source->lsources, &linker->sources);
	return EOK;
}

/** Destroy linker source.
 *
 * @param src Linker source
 */
static void obj_linker_src_destroy(obj_linker_src_t *src)
{
	list_remove(&src->lsources);
	free(src);
}

/** Set address where code should start.
 *
 * @param linker Linker
 * @param origin Origin address
 */
int obj_linker_set_origin(obj_linker_t *linker, uint32_t origin)
{
	linker->org = origin;
	return EOK;
}

/** Perform linking.
 *
 * @param linker Linker
 * @param rdest Place to store pointer to resulting object
 * @return EOK on success or an error code
 */
int obj_linker_link(obj_linker_t *linker, obj_object_t **rdest)
{
	obj_object_t *dest = NULL;
	obj_linker_src_t *src;
	obj_section_t *section;
	obj_section_t *src_sec;
	obj_section_t *src_nsec;
	obj_reloc_t *reloc;
	obj_reloc_t *next;
	uint32_t address;
	unsigned modidx;
	int rc;

	rc = obj_object_create(&dest);
	if (rc != EOK)
		return rc;

	/* Copy everything from sources. */
	src = obj_linker_src_first(linker);
	modidx = 1;
	while (src != NULL) {
		rc = obj_object_copy(src->object, modidx++, dest);
		if (rc != EOK)
			goto error;

		src = obj_linker_src_next(src);
	}

	/* Assign addresses to sections. */
	address = linker->org;
	section = obj_section_first(dest);
	while (section != NULL) {
		section->base_addr = address;
		address += section->len;

		section = obj_section_next(section);
	}

	/* Process relocations. */
	reloc = obj_reloc_first(dest);
	while (reloc != NULL) {
		/* Relocation can disappear when processed. */
		next = obj_reloc_next(reloc);

		rc = obj_reloc_process(reloc, linker->flags);
		if (rc != EOK)
			goto error;

		reloc = next;
	}

	/* Merge sections. */
	section = obj_section_first(dest);
	while (section != NULL) {
		src_sec = obj_section_next(section);
		while (src_sec != NULL) {
			src_nsec = obj_section_next(src_sec);

			if (obj_section_basename_cmp(section, src_sec) == 0) {
				rc = obj_section_merge(section, src_sec);
				if (rc != EOK)
					goto error;

				rc = obj_section_remove_tag(section);
				if (rc != EOK)
					goto error;
			}

			src_sec = src_nsec;
		}

		section = obj_section_next(section);
	}

	*rdest = dest;
	return EOK;
error:
	obj_object_destroy(dest);
	return rc;
}

/** Get first linker source.
 *
 * @param linker Linker
 * @return First source or @c NULL if there are none.
 */
static obj_linker_src_t *obj_linker_src_first(obj_linker_t *linker)
{
	link_t *link;

	link = list_first(&linker->sources);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_linker_src_t, lsources);
}

/** Get next linker source.
 *
 * @param cur Current source
 * @return Next section or @c NULL if @a cur is the last source.
 */
static obj_linker_src_t *obj_linker_src_next(obj_linker_src_t *cur)
{
	link_t *link;

	link = list_next(&cur->lsources, &cur->linker->sources);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_linker_src_t, lsources);
}
