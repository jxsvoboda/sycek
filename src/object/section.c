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
#include <object/object.h>
#include <object/section.h>

/** Create binary object section structure.
 *
 * @param object Containing object
 * @param rsection Place to store pointer to new binary object section
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_section_create(obj_object_t *object, obj_section_t **rsection)
{
	obj_section_t *section;

	section = calloc(1, sizeof(obj_section_t));
	if (section == NULL)
		return ENOMEM;

	section->object = object;
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

	(void)section;

	rc = fprintf(outf, "  Section:\n");
	if (rc < 0)
		return EIO;

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
