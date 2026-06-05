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

#include <inttypes.h>
#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <object/object.h>
#include <object/section.h>
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

/** Save binary object symbol into a z80asm compatible map file.
 *
 * @param symbol Symbol
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_symbol_save_map(obj_symbol_t *symbol, FILE *outf)
{
	int rc;
	uint32_t sym_addr;

	sym_addr = symbol->section->base_addr + symbol->offset;

	rc = fprintf(outf, "%s = $%04" PRIx32 " ;  Symbol: %s section:%s "
	    "offset:0x%x length:%u\n", symbol->name, sym_addr,
	    symbol->name, symbol->section->name, symbol->offset,
	    symbol->size);
	if (rc < 0)
		return EIO;

	return EOK;
}

/** Copy binary object symbol to another object.
 *
 * @param symbol Symbol
 * @param dest Destination object
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_symbol_copy(obj_symbol_t *symbol, obj_object_t *dest)
{
	obj_section_t *dsection;
	obj_symbol_t *dsymbol = NULL;
	int rc;

	dsection = obj_section_by_name(dest, symbol->section->name);
	if (dsection == NULL)
		return EINVAL;

	rc = obj_symbol_create(dest, symbol->name, dsection, symbol->offset,
	    symbol->size, &dsymbol);
	return rc;
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

/** Find symbol in object by name.
 *
 * @param object Object
 * @param name Symbol name
 * @return Symbol matching @a name or @c NULL if there are none.
 */
obj_symbol_t *obj_symbol_find(obj_object_t *object, const char *name)
{
	obj_symbol_t *symbol;

	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		if (strcmp(symbol->name, name) == 0)
			return symbol;

		symbol = obj_symbol_next(symbol);
	}

	/* Not found. */
	return NULL;
}
