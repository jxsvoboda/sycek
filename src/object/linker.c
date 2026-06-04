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

#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <object/linker.h>
#include <object/object.h>
#include <object/reloc.h>
#include <object/section.h>

/** Create binary linker.
 *
 * @param rlinker Place to store pointer to new binary object linker
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_linker_create(obj_linker_t **rlinker)
{
	obj_linker_t *linker;

	linker = calloc(1, sizeof(obj_linker_t));
	if (linker == NULL)
		return ENOMEM;

	*rlinker = linker;
	return EOK;
}

/** Destroy binary object linker.
 *
 * @param linker Linker or @c NULL
 */
void obj_linker_destroy(obj_linker_t *linker)
{
	if (linker == NULL)
		return;

	obj_object_destroy(linker->dest);
	free(linker);
}

/** Add source object to linker.
 *
 * @param linker Linker
 * @return EOK on success or an error code
 */
int obj_linker_add_src(obj_linker_t *linker, obj_object_t *src)
{
	if (linker->src != NULL)
		return EINVAL;

	linker->src = src;
	return EOK;
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
	obj_section_t *section;
	obj_reloc_t *reloc;
	obj_reloc_t *next;
	uint32_t address;
	int rc;

	rc = obj_object_create(&dest);
	if (rc != EOK)
		return rc;

	/* Copy everything from sources. */
	if (linker->src != NULL) {
		rc = obj_object_copy(linker->src, dest);
		if (rc != EOK)
			goto error;
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

		rc = obj_reloc_process(reloc);
		if (rc != EOK)
			goto error;

		reloc = next;
	}

	*rdest = dest;
	return EOK;
error:
	obj_object_destroy(dest);
	return rc;
}
