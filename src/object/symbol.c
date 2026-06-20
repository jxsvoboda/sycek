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

#include <byteorder.h>
#include <inttypes.h>
#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <object/object.h>
#include <object/section.h>
#include <object/symbol.h>
#include <types/object/file.h>

/** Create binary object symbol structure.
 *
 * @param object Containing object
 * @param name Symbol name
 * @param section Section containing symbol
 * @param binding Symbol binding
 * @param offset Symbol start offset within section
 * @param size Symbol size
 * @param rsymbol Place to store pointer to new binary object symbol
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_symbol_create(obj_object_t *object, const char *name,
    obj_section_t *section, obj_symbol_binding_t binding, uint32_t offset,
    uint32_t size, obj_symbol_t **rsymbol)
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
	symbol->binding = binding;
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

/** Return object binding as string.
 *
 * @param binding Symbol biding
 * @return Object binding as string
 */
static const char *obj_symbol_binding_str(obj_symbol_binding_t binding)
{
	const char *sbinding;

	switch (binding) {
	case objb_global:
		sbinding = "global";
		break;
	case objb_local:
		sbinding = "local";
		break;
	default:
		sbinding = "unknown";
		break;
	}

	return sbinding;
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
	const char *sbinding;

	sbinding = obj_symbol_binding_str(symbol->binding);

	rc = fprintf(outf, "  Symbol: %s section:%s offset:0x%x "
	    "length:%u binding:%s\n",
	    symbol->name, symbol->section->name, symbol->offset,
	    symbol->size, sbinding);
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
	const char *sbinding;
	uint32_t sym_addr;

	sbinding = obj_symbol_binding_str(symbol->binding);
	sym_addr = symbol->section->base_addr + symbol->offset;

	rc = fprintf(outf, "%s = $%04" PRIx32 " ;  Symbol: %s section:%s "
	    "offset:0x%x length:%u binding:%s\n", symbol->name, sym_addr,
	    symbol->name, symbol->section->name, symbol->offset,
	    symbol->size, sbinding);
	if (rc < 0)
		return EIO;

	return EOK;
}

/** Load binary object symbol from object file.
 *
 * @param object Object
 * @param inf Output file
 * @param rsymbol Place to store pointer to loaded symbol
 * @return EOK on success or an error code
 */
int obj_symbol_load_obj(obj_object_t *object, FILE *inf, obj_symbol_t **rsymbol)
{
	obj_file_symbol_t sym;
	size_t nr;
	uint32_t nsize;
	uint32_t section_idx;
	obj_symbol_binding_t binding;
	uint32_t offset;
	uint32_t size;
	obj_section_t *section;
	char *name = NULL;
	int rc;

	/* Read symbol header. */

	nr = fread(&sym, 1, sizeof(sym), inf);
	if (nr != sizeof(sym)) {
		(void)fprintf(stderr, "Error reading symbol header.\n");
		return EIO;
	}

	nsize = uint32_t_le2host(sym.name_len);
	section_idx = uint32_t_le2host(sym.section_idx);
	binding = (obj_symbol_binding_t)sym.binding;
	offset = uint32_t_le2host(sym.offset);
	size = uint32_t_le2host(sym.size);

	section = obj_section_by_idx(object, section_idx);
	if (section == NULL) {
		(void)fprintf(stderr, "Invalid section index.\n");
		rc = EIO;
		goto error;
	}

	name = calloc((size_t)nsize + 1, 1);
	if (name == NULL) {
		(void)fprintf(stderr, "Out of memory.\n");
		rc = ENOMEM;
		goto error;
	}

	/* Read symbol name including padding. */
	nr = fread(name, 1, (size_t)nsize, inf);
	if (nr != nsize) {
		(void)fprintf(stderr, "Error reading symbol name.\n");
		return EIO;
	}

	rc = obj_symbol_create(object, name, section, binding, offset, size,
	    rsymbol);
	if (rc != EOK)
		goto error;

	free(name);
	return EOK;
error:
	if (name != NULL)
		free(name);
	return rc;
}

/** Save binary object symbol into object file.
 *
 * @param symbol Symbol
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_symbol_save_obj(obj_symbol_t *symbol, FILE *outf)
{
	obj_file_entry_hdr_t hdr;
	obj_file_symbol_t sym;
	size_t nw;
	uint32_t nsize;
	uint8_t pad[obj_file_align];

	nsize = obj_align_up(strlen(symbol->name));

	/* Entry header */

	hdr.etype = host2uint32_t_le(obj_file_esymbol);
	hdr.esize = host2uint32_t_le(sizeof(obj_file_symbol_t) + nsize);

	nw = fwrite(&hdr, 1, sizeof(hdr), outf);
	if (nw != sizeof(hdr)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Symbol header */

	sym.name_len = host2uint32_t_le(nsize);
	sym.section_idx =
	    host2uint32_t_le(obj_section_get_idx(symbol->section));
	sym.binding = (uint8_t)symbol->binding;
	sym.offset = host2uint32_t_le(symbol->offset);
	sym.size = host2uint32_t_le(symbol->size);

	nw = fwrite(&sym, 1, sizeof(sym), outf);
	if (nw != sizeof(sym)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Symbol name */
	nw = fwrite(symbol->name, 1, strlen(symbol->name), outf);
	if (nw != strlen(symbol->name)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Padding */
	memset(pad, 0, sizeof(pad));
	nw = fwrite(pad, 1, (size_t)nsize - strlen(symbol->name), outf);
	if (nw != nsize - strlen(symbol->name)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	return EOK;
}

/** Copy binary object symbol to another object.
 *
 * @param symbol Symbol
 * @param modidx Source module index
 * @param dest Destination object
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_symbol_copy(obj_symbol_t *symbol, unsigned modidx, obj_object_t *dest)
{
	obj_section_t *dsection;
	obj_symbol_t *dsymbol = NULL;
	char *sname = NULL;
	int rc;

	rc = obj_section_tagged_name(symbol->section, modidx, &sname);
	if (rc != EOK)
		return rc;

	dsection = obj_section_by_name(dest, sname);
	if (dsection == NULL) {
		free(sname);
		return EINVAL;
	}

	free(sname);

	return obj_symbol_create(dest, symbol->name, dsection, symbol->binding,
	    symbol->offset, symbol->size, &dsymbol);
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
 * @param modname Module name (only find local symbols in this module)
 * @return Symbol matching @a name or @c NULL if there are none.
 */
obj_symbol_t *obj_symbol_find(obj_object_t *object, const char *name,
    const char *modname)
{
	obj_symbol_t *symbol;

	symbol = obj_symbol_first(object);
	while (symbol != NULL) {
		/* Local or global? */
		if (symbol->binding == objb_local) {
			/* Local symbol: Same name, same module? */
			if (strcmp(symbol->name, name) == 0 &&
			    strcmp(symbol->section->modname, modname) == 0)
				return symbol;
		} else {
			/* Global symbol: Same name? */
			if (strcmp(symbol->name, name) == 0)
				return symbol;

		}

		symbol = obj_symbol_next(symbol);
	}

	/* Not found. */
	return NULL;
}
