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
 * Binary object symbol
 */

#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <object/object.h>
#include <object/symbol.h>

/** Create binary object symbol structure.
 *
 * @param object Containing object
 * @param name Symbol name
 * @param section Section containing symbol
 * @param offset Symbol start offset within section
 * @param size Symbol size
 * @param rsymbol Place to store pointer to new binary object symbol
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_symbol_create(obj_object_t *object, const char *name,
    obj_section_t *section, uint32_t offset, uint32_t size,
    obj_symbol_t **rsymbol)
{
	obj_symbol_t *symbol;

	symbol = calloc(1, sizeof(obj_symbol_t));
	if (symbol == NULL)
		return ENOMEM;

	symbol->object = object;
	symbol->name = strdup(name);
	if (symbol->name == NULL) {
		free(symbol);
		return ENOMEM;
	}

	symbol->section = section;
	symbol->offset = offset;
	symbol->size = size;

	list_append(&symbol->lsymbols, &object->symbols);
	*rsymbol = symbol;
	return EOK;
}

/** Destroy binary object symbol.
 *
 * @param symbol Symbol or @c NULL
 */
void obj_symbol_destroy(obj_symbol_t *symbol)
{
	if (symbol == NULL)
		return;

	list_remove(&symbol->lsymbols);
	free(symbol->name);
	free(symbol);
}

/** Dump binary object symbol.
 *
 * @param symbol Symbol
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_symbol_dump(obj_symbol_t *symbol, FILE *outf)
{
	int rc;

	rc = fprintf(outf, "  Symbol: %s section:%s offset:0x%x "
	    "length:%u\n",
	    symbol->name, symbol->section->name, symbol->offset,
	    symbol->size);
	if (rc < 0)
		return EIO;

	return EOK;
}

/** Get first symbol in object.
 *
 * @param object Object
 * @return First symbol or @c NULL if there are none.
 */
obj_symbol_t *obj_symbol_first(obj_object_t *object)
{
	link_t *link;

	link = list_first(&object->symbols);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_symbol_t, lsymbols);
}

/** Get next symbol in object.
 *
 * @param cur Current symbol
 * @return Next symbol or @c NULL if @a cur is the last symbol.
 */
obj_symbol_t *obj_symbol_next(obj_symbol_t *cur)
{
	link_t *link;

	link = list_next(&cur->lsymbols, &cur->object->symbols);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_symbol_t, lsymbols);
}
