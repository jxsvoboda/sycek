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
 * Binary object relocation
 */

#include <assert.h>
#include <inttypes.h>
#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <object/object.h>
#include <object/reloc.h>
#include <object/section.h>
#include <object/symbol.h>

/** Create binary object relocation structure.
 *
 * @param object Containing object
 * @param section Section containing relocation
 * @param rtype Relocation type
 * @param offset Relocation offset within section
 * @param name Referenced symbol name
 * @param addend Addend
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_reloc_create(obj_object_t *object, obj_section_t *section,
    obj_reloc_type_t rtype, uint32_t offset, const char *sym_name,
    uint64_t addend)
{
	obj_reloc_t *reloc;

	reloc = calloc(1, sizeof(obj_reloc_t));
	if (reloc == NULL)
		return ENOMEM;

	reloc->object = object;
	reloc->section = section;
	reloc->rtype = rtype;
	reloc->offset = offset;

	reloc->sym_name = strdup(sym_name);
	if (reloc->sym_name == NULL) {
		free(reloc);
		return ENOMEM;
	}

	reloc->addend = addend;

	list_append(&reloc->lrelocs, &object->relocs);
	return EOK;
}

/** Destroy binary object relocation.
 *
 * @param reloc Relocation or @c NULL
 */
void obj_reloc_destroy(obj_reloc_t *reloc)
{
	if (reloc == NULL)
		return;

	list_remove(&reloc->lrelocs);
	free(reloc->sym_name);
	free(reloc);
}

static const char *obj_reloc_type_name(obj_reloc_type_t rtype)
{
	switch (rtype) {
	case objr_sa16:
		return "SA16";
	case objr_rela16:
		return "RELA16";
	default:
		return "unknown";
	}
}

/** Dump binary object relocation.
 *
 * @param reloc Relocation
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_reloc_dump(obj_reloc_t *reloc, FILE *outf)
{
	int rc;

	rc = fprintf(outf, "  Relocation: section:%s type:%s offset:0x%x "
	    "sym_name:%s addend:%llu\n", reloc->section->name,
	    obj_reloc_type_name(reloc->rtype), reloc->offset, reloc->sym_name,
	    (unsigned long long)reloc->addend);
	if (rc < 0)
		return EIO;

	return EOK;
}

/** Copy binary object relocation to another object.
 *
 * @param reloc Relocation
 * @param dest Destination object
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_reloc_copy(obj_reloc_t *reloc, obj_object_t *dest)
{
	obj_section_t *dsection;
	int rc;

	dsection = obj_section_by_name(dest, reloc->section->name);
	if (dsection == NULL)
		return EINVAL;

	rc = obj_reloc_create(dest, dsection, reloc->rtype, reloc->offset,
	    reloc->sym_name, reloc->addend);
	return rc;
}

/** Get first relocation in object.
 *
 * @param object Object
 * @return First relocation or @c NULL if there are none.
 */
obj_reloc_t *obj_reloc_first(obj_object_t *object)
{
	link_t *link;

	link = list_first(&object->relocs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_reloc_t, lrelocs);
}

/** Get next relocation in object.
 *
 * @param cur Current relocation
 * @return Next relocation or @c NULL if @a cur is the last relocation.
 */
obj_reloc_t *obj_reloc_next(obj_reloc_t *cur)
{
	link_t *link;

	link = list_next(&cur->lrelocs, &cur->object->relocs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, obj_reloc_t, lrelocs);
}

/** Process SA16 relocation.
 *
 * @param reloc Relocation
 * @return EOK on success or an error code
 */
static int obj_reloc_process_sa16(obj_reloc_t *reloc)
{
	obj_symbol_t *symbol;
	uint64_t addr;
	int rc;

	symbol = obj_symbol_find(reloc->object, reloc->sym_name);
	if (symbol == NULL) {
		(void)fprintf(stderr, "Link error: Symbol '%s' not found.\n",
		    reloc->sym_name);
		return ENOENT;
	}

	addr = symbol->section->base_addr + symbol->offset + reloc->addend;
	if (addr > 0xffffu) {
		(void)fprintf(stderr, "Link error: Address 0x%" PRIx64
		    " is out of range.\n", addr);
		return EINVAL;
	}

	rc = obj_section_write_u16le(reloc->section, reloc->offset,
	    (uint16_t)addr);
	if (rc != EOK)
		return rc;

	obj_reloc_destroy(reloc);
	return EOK;
}

/** Process relocation.
 *
 * @param reloc Relocation
 * @return EOK on success or an error code
 */
int obj_reloc_process(obj_reloc_t *reloc)
{
	switch (reloc->rtype) {
	case objr_sa16:
		return obj_reloc_process_sa16(reloc);
		break;
	default:
		assert(false);
		return EINVAL;
	}
}
