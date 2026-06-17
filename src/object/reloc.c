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
#include <byteorder.h>
#include <inttypes.h>
#include <merrno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <object/object.h>
#include <object/reloc.h>
#include <object/section.h>
#include <object/symbol.h>
#include <types/object/file.h>

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

/** Load binary object relocation from object file.
 *
 * @param object Object
 * @param inf Input file
 * @return EOK on success or an error code
 */
int obj_reloc_load_obj(obj_object_t *object, FILE *inf)
{
	obj_file_reloc_t rel;
	uint32_t nsize;
	obj_section_t *section;
	uint32_t section_idx;
	obj_reloc_type_t rtype;
	uint32_t offset;
	char *sym_name = NULL;
	uint64_t addend;
	size_t nr;
	int rc;

	/* Read relocation header */

	nr = fread(&rel, 1, sizeof(rel), inf);
	if (nr != sizeof(rel)) {
		(void)fprintf(stderr, "Error reading relocation header.\n");
		return EIO;
	}

	section_idx = uint32_t_le2host(rel.section_idx);
	rtype = (obj_reloc_type_t)uint32_t_le2host((uint32_t)rel.rtype);
	offset = uint32_t_le2host(rel.offset);
	nsize = uint32_t_le2host(rel.sym_name_len);
	addend = uint64_t_le2host(rel.addend);

	section = obj_section_by_idx(object, section_idx);
	if (section == NULL) {
		(void)fprintf(stderr, "Invalid section index.\n");
		rc = EIO;
		goto error;
	}

	sym_name = calloc((size_t)nsize + 1, 1);
	if (sym_name == NULL) {
		(void)fprintf(stderr, "Out of memory.\n");
		rc = ENOMEM;
		goto error;
	}

	/* Symbol name including padding */
	nr = fread(sym_name, 1, (size_t)nsize, inf);
	if (nr != nsize) {
		(void)fprintf(stderr, "Error reading referenced symbol "
		    "name.\n");
		rc = EIO;
		goto error;
	}

	rc = obj_reloc_create(object, section, rtype, offset, sym_name,
	    addend);
	if (rc != EOK)
		goto error;

	free(sym_name);
	return EOK;
error:
	if (sym_name != NULL)
		free(sym_name);
	return rc;
}

/** Save binary object relocation into object file.
 *
 * @param reloc Relocation
 * @param outf Output file
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_reloc_save_obj(obj_reloc_t *reloc, FILE *outf)
{
	obj_file_entry_hdr_t hdr;
	obj_file_reloc_t rel;
	uint32_t nsize;
	uint32_t size;
	uint8_t pad[obj_file_align];
	size_t nw;

	nsize = obj_align_up(strlen(reloc->sym_name));
	size = sizeof(obj_file_reloc_t) + nsize;

	/* Entry header */

	hdr.etype = host2uint32_t_le(obj_file_ereloc);
	hdr.esize = host2uint32_t_le(size);

	nw = fwrite(&hdr, 1, sizeof(hdr), outf);
	if (nw != sizeof(hdr)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Relocation header */

	rel.section_idx = host2uint32_t_le(obj_section_get_idx(reloc->section));
	rel.rtype = host2uint32_t_le((uint32_t)reloc->rtype);
	rel.offset = host2uint32_t_le(reloc->offset);
	rel.sym_name_len = host2uint32_t_le(nsize);
	rel.addend = host2uint64_t_le(reloc->addend);

	nw = fwrite(&rel, 1, sizeof(rel), outf);
	if (nw != sizeof(rel)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Symbol name */
	nw = fwrite(reloc->sym_name, 1, strlen(reloc->sym_name), outf);
	if (nw != strlen(reloc->sym_name)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	/* Padding */
	memset(pad, 0, sizeof(pad));
	nw = fwrite(pad, 1, (size_t)nsize - strlen(reloc->sym_name), outf);
	if (nw != nsize - strlen(reloc->sym_name)) {
		(void)fprintf(stderr, "Write error.\n");
		return EIO;
	}

	return EOK;
}

/** Copy binary object relocation to another object.
 *
 * @param reloc Relocation
 * @param modidx Source module index
 * @param dest Destination object
 * @return EOK on success, ENOMEM if out of memory
 */
int obj_reloc_copy(obj_reloc_t *reloc, unsigned modidx, obj_object_t *dest)
{
	obj_section_t *dsection;
	char *sname = NULL;
	int rc;

	rc = obj_section_tagged_name(reloc->section, modidx, &sname);
	if (rc != EOK)
		return rc;

	dsection = obj_section_by_name(dest, sname);
	if (dsection == NULL) {
		free(sname);
		return EINVAL;
	}

	free(sname);
	return obj_reloc_create(dest, dsection, reloc->rtype, reloc->offset,
	    reloc->sym_name, reloc->addend);
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
