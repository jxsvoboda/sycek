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
 * Z80 binary instruction emitter
 *
 * Convert Z80 IC to binary object
 */

#include <merrno.h>
#include <stdlib.h>
#include <object/object.h>
#include <object/section.h>
#include <z80/emit.h>
#include <z80/z80ic.h>

/** Create binary instruction emitter.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_emit_create(z80_emit_t **remit)
{
	z80_emit_t *emit;

	emit = calloc(1, sizeof(z80_emit_t));
	if (emit == NULL)
		return ENOMEM;

	*remit = emit;
	return EOK;

}
/** Emit binary instructions for data block entry.
 *
 * @param emit Binary instruction emitter
 * @param entry Data block entry
 * @return EOK on success or an error code
 */
static int z80_emit_dblock_entry(z80_emit_t *emit, z80ic_dblock_entry_t *entry)
{
	int rc = EOK;

	(void)emit;
	(void)entry;

	return rc;
}

/** Emit binary instructions for variable.
 *
 * @param emit Binary instruction emitter
 * @param var Z80 IC variable
 * @return EOK on success or an error code
 */
static int z80_emit_var(z80_emit_t *emit, z80ic_var_t *var)
{
	int rc = EOK;
	z80ic_dblock_entry_t *entry;

	(void)emit;
	(void)var;

	entry = z80ic_dblock_first(var->dblock);
	while (entry != NULL) {
		rc = z80_emit_dblock_entry(emit, entry);
		if (rc != EOK)
			return rc;

		entry = z80ic_dblock_next(entry);
	}

	return rc;
}

/** Emit binary instructions for procedure.
 *
 * @param emit Binary instruction emitter
 * @param var Z80 IC variable
 * @return EOK on success or an error code
 */
static int z80_emit_proc(z80_emit_t *emit, z80ic_proc_t *proc)
{
	int rc = EOK;

	(void)emit;
	(void)proc;

	return rc;
}

/** Emit binary instructions for declaration.
 *
 * @param emit Binary instruction emitter
 * @param decln Z80 IC declaration
 * @return EOK on success or an error code
 */
static int z80_emit_decln(z80_emit_t *emit, z80ic_decln_t *decln)
{
	int rc = EINVAL;

	(void)emit;
	(void)decln;

	switch (decln->dtype) {
	case z80icd_extern:
	case z80icd_global:
		rc = EOK;
		break;
	case z80icd_var:
		rc = z80_emit_var(emit, (z80ic_var_t *)decln->ext);
		break;
	case z80icd_proc:
		rc = z80_emit_proc(emit, (z80ic_proc_t *)decln->ext);
		break;
	}

	return rc;
}

/** Emit binary instructions for module.
 *
 * @param emit Binary instruction emitter
 * @param icmod Z80 IC module
 * @param robject Place to store pointer to new binary object
 * @return EOK on success or an error code
 */
int z80_emit_module(z80_emit_t *emit, z80ic_module_t *icmod,
    obj_object_t **robject)
{
	obj_object_t *object = NULL;
	obj_section_t *section = NULL;
	z80ic_decln_t *decln;
	int rc;

	rc = obj_object_create(&object);
	if (rc != EOK)
		goto error;

	emit->object = object;
	emit->ic_module = icmod;

	rc = obj_section_create(object, &section);
	if (rc != EOK)
		goto error;

	decln = z80ic_module_first(icmod);
	while (decln != NULL) {
		rc = z80_emit_decln(emit, decln);
		if (rc != EOK)
			goto error;

		decln = z80ic_module_next(decln);
	}

	emit->ic_module = NULL;
	*robject = object;
	return EOK;
error:
	obj_object_destroy(object);
	return rc;
}

/** Destroy binary instruction emitter.
 *
 * @param emit Instruction emitter or @c NULL
 */
void z80_emit_destroy(z80_emit_t *emit)
{
	if (emit == NULL)
		return;

	free(emit);
}
