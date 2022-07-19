/*
 * Copyright 2022 Jiri Svoboda
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
 * Z80 IR local variable to VR map
 *
 * Maps local variable names in an IR procedure to virtual registers
 * in a Z80 VR IC procedure.
 */

#include <assert.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <z80/varmap.h>

/** Create variable map.
 *
 * @param rvarmap Place to store pointer to new variable map
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_varmap_create(z80_varmap_t **rvarmap)
{
	z80_varmap_t *varmap;

	varmap = calloc(1, sizeof(z80_varmap_t));
	if (varmap == NULL)
		return ENOMEM;

	list_initialize(&varmap->entries);

	*rvarmap = varmap;
	return EOK;
}

/** Destroy variable map.
 *
 * @param isel Variable map or @c NULL
 */
void z80_varmap_destroy(z80_varmap_t *varmap)
{
	z80_varmap_entry_t *entry;

	if (varmap == NULL)
		return;

	entry = z80_varmap_first(varmap);
	while (entry != NULL) {
		z80_varmap_entry_destroy(entry);
		entry = z80_varmap_first(varmap);
	}

	free(varmap);
}

/** Insert variable into variable map.
 *
 * @param varmap Variable map
 * @param ident Variable identifier
 * @param vrn Number of used virtual registers
 * @return EOK on success or an error code
 */
int z80_varmap_insert(z80_varmap_t *varmap, const char *ident, unsigned vrn)
{
	z80_varmap_entry_t *entry;

	entry = calloc(1, sizeof(z80_varmap_entry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->ident = strdup(ident);
	if (entry->ident == NULL) {
		free(entry);
		return ENOMEM;
	}

	/* Allocate next vrn virtual registers to the variable */
	entry->vr0 = varmap->next_vr;
	entry->vrn = vrn;
	varmap->next_vr += vrn;

	list_append(&entry->lentries, &varmap->entries);
	entry->varmap = varmap;

	return EOK;
}

/** Find variable map entry.
 *
 * @param varmap Variable map
 * @param ident Variable identifier
 * @param rentry Place to store pointer to variable map entry
 * @return EOK on success, ENOENT if not found
 */
int z80_varmap_find(z80_varmap_t *varmap, const char *ident,
    z80_varmap_entry_t **rentry)
{
	z80_varmap_entry_t *entry;

	entry = z80_varmap_first(varmap);
	while (entry != NULL) {
		if (strcmp(entry->ident, ident) == 0) {
			*rentry = entry;
			return EOK;
		}

		entry = z80_varmap_next(entry);
	}

	return ENOENT;
}

void z80_varmap_entry_destroy(z80_varmap_entry_t *entry)
{
	free(entry->ident);
	list_remove(&entry->lentries);
	free(entry);
}

/** Get first variable map entry.
 *
 * @param varmap Variable map
 * @return First variable map entry or @c NULL if the map is empty
 */
z80_varmap_entry_t *z80_varmap_first(z80_varmap_t *varmap)
{
	link_t *link;

	link = list_first(&varmap->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80_varmap_entry_t, lentries);
}

/** Get next variable map entry.
 *
 * @param cur Current entry
 * @return Next entry or @c NULL if @a cur is the last entry
 */
z80_varmap_entry_t *z80_varmap_next(z80_varmap_entry_t *cur)
{
	link_t *link;

	link = list_next(&cur->lentries, &cur->varmap->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80_varmap_entry_t, lentries);
}
