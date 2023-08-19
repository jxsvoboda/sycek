/*
 * Copyright 2023 Jiri Svoboda
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
 * Z80 Instruction selector
 *
 * Convert (machine-independent) IR to Z80 IC with virtual registers.
 */

#include <assert.h>
#include <ir.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <z80/argloc.h>
#include <z80/isel.h>
#include <z80/varmap.h>
#include <z80/z80ic.h>

static int z80_isel_texpr_sizeof(z80_isel_t *, ir_texpr_t *, size_t *);

/** Mangle global identifier.
 *
 * @param irident IR global identifier
 * @param rident Place to store pointer to IC procedure identifier
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_mangle_global_ident(const char *irident, char **rident)
{
	int rv;
	char *ident;

	/* The indentifier must have global scope */
	assert(irident[0] == '@');

	rv = asprintf(&ident, "_%s", &irident[1]);
	if (rv < 0)
		return ENOMEM;

	*rident = ident;
	return EOK;
}

/** Mangle label identifier.
 *
 * @param proc IR procedure identifier
 * @param irident IR label identifier
 * @param rident Place to store pointer to IC label identifier
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_mangle_label_ident(const char *proc,
    const char *irident, char **rident)
{
	int rv;
	char *ident;

	/* The procdure identifier must have global scope */
	assert(proc[0] == '@');
	/* The label identifier must have local scope */
	assert(irident[0] == '%');

	rv = asprintf(&ident, "l_%s_%s", &proc[1], &irident[1]);
	if (rv < 0)
		return ENOMEM;

	*rident = ident;
	return EOK;
}

/** Mangle local variable identifier.
 *
 * @param proc IR procedure identifier
 * @param irident IR local variable identifier
 * @param rident Place to store pointer to IC local variable identifier
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_mangle_lvar_ident(const char *proc,
    const char *irident, char **rident)
{
	int rv;
	char *ident;
	char *cp;

	/* The procedure identifier must have global scope */
	assert(proc[0] == '@');
	/* The variable identifier must have local scope */
	assert(irident[0] == '%');

	rv = asprintf(&ident, "%c_%s_%s", irident[1] == '@' ?
	    'e' : 'v', &proc[1], &irident[1]);
	if (rv < 0)
		return ENOMEM;

	/* Replace middling '@' signs with '_', which is allowed in Z80 asm */
	cp = ident;
	while (*cp != '\0') {
		if (*cp == '@')
			*cp = '_';
		++cp;
	}

	*rident = ident;
	return EOK;
}

/** Determine if variable is a virtual register.
 *
 * @param varname Variable name
 * @return @c true iff variable is a virtual register
 */
static bool z80_isel_is_vreg(const char *varname)
{
	char *endptr;

	if (varname[0] != '%')
		return false;

	(void) strtoul(&varname[1], &endptr, 10);
	if (*endptr != '\0')
		return false;

	return true;
}

/** Get virtual register number from variable name.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @param oper Variable operand referring to a local numbered variable
 *
 * @return Virtual register number
 */
static unsigned z80_isel_get_vregno(z80_isel_proc_t *isproc, ir_oper_t *oper)
{
	ir_oper_var_t *opvar;
	z80_varmap_entry_t *entry;
	int rc;

	assert(oper->optype == iro_var);
	opvar = (ir_oper_var_t *) oper->ext;

	rc = z80_varmap_find(isproc->varmap, opvar->varname, &entry);
	assert(rc == EOK);

	return entry->vr0;
}

/** Determine virtual register part and offset in which a particular
 * byte of an integer resides.
 *
 * An 8-bit integer is stored in a single virtual register (%N).
 * A 16-bit or larger integer is stored in one or more virtual
 * register pairs. E. g. a 64-bit integer is stored in four virtual
 * register pairs (%%N, %%N+1, %%N+2. %%N+3) starting from the less
 * significant word and ending with the most significant word.
 *
 * @param byte Byte being addressed (0..lowest, n-1 .. highest)
 * @param nbytes Size of integer in bytes
 * @param part Place to store virtual register part
 * @param vroff Place to store virtual register offset (0, 1, ..)
 */
static void z80_isel_reg_part_off(unsigned byte, unsigned nbytes,
    z80ic_vr_part_t *part, unsigned *vroff)
{
	assert(byte < nbytes);

	if (nbytes == 1) {
		*part = z80ic_vrp_r8;
		*vroff = 0;
	} else {
		*part = (byte & 1) != 0 ? z80ic_vrp_r16h :
		    z80ic_vrp_r16l;
		*vroff = byte / 2;
	}
}

/** Get size of type described by IR integer type expression in bytes.
 *
 * @param isel Instruction selector
 * @param texpr IR integer type expression
 * @param rsize Place to store size in bytes
 */
static void z80_isel_texpr_int_sizeof(z80_isel_t *isel, ir_texpr_t *texpr,
    size_t *rsize)
{
	assert(texpr->tetype == irt_int);
	(void)isel;

	/* Convert bits to bytes */
	*rsize = (texpr->t.tint.width + 7) / 8;
}

/** Get size of type described by IR pointer type expression in bytes.
 *
 * @param isel Instruction selector
 * @param texpr IR pointer type expression
 * @param rsize Place to store size in bytes
 */
static void z80_isel_texpr_ptr_sizeof(z80_isel_t *isel, ir_texpr_t *texpr,
    size_t *rsize)
{
	assert(texpr->tetype == irt_ptr);
	(void)isel;

	/* Convert bits to bytes */
	*rsize = (texpr->t.tptr.width + 7) / 8;
}

/** Get size of type described by IR array type expression in bytes.
 *
 * @param isel Instruction selector
 * @param texpr IR array type expression
 * @param rsize Place to store size in bytes
 * @return EOK on success or an error code
 */
static int z80_isel_texpr_array_sizeof(z80_isel_t *isel, ir_texpr_t *texpr,
    size_t *rsize)
{
	size_t esize;
	int rc;

	assert(texpr->tetype == irt_array);

	/* Element size */
	rc = z80_isel_texpr_sizeof(isel, texpr->t.tarray.etexpr, &esize);
	if (rc != EOK)
		return rc;

	/* Array size */
	*rsize = texpr->t.tarray.asize * esize;
	return EOK;
}

/** Get size of type described by IR identifier type expression in bytes.
 *
 * @param isel Instruction selector
 * @param texpr IR identifier type expression
 * @param rsize Place to store size in bytes
 * @return EOK on success or an error code
 */
static int z80_isel_texpr_ident_sizeof(z80_isel_t *isel, ir_texpr_t *texpr,
    size_t *rsize)
{
	ir_decln_t *decln;
	ir_record_t *record;
	ir_record_elem_t *elem;
	size_t esize;
	size_t recsize;
	int rc;

	assert(texpr->tetype == irt_ident);

	rc = ir_module_find(isel->irmodule, texpr->t.tident.ident, &decln);
	if (rc != EOK)
		return ENOENT;

	if (decln->dtype != ird_record)
		return EINVAL;

	record = (ir_record_t *)decln->ext;
	elem = ir_record_first(record);
	recsize = 0;
	while (elem != NULL) {
		rc = z80_isel_texpr_sizeof(isel, elem->etype, &esize);
		if (rc != EOK)
			return rc;

		recsize += esize;
		elem = ir_record_next(elem);
	}

	*rsize = recsize;
	return EOK;
}

/** Get size of type described by IR type expression in bytes.
 *
 * @param isel Instruction selector
 * @param texpr IR type expression
 * @param rsize Place to store size in bytes
 * @return EOK on success or an error code
 */
static int z80_isel_texpr_sizeof(z80_isel_t *isel, ir_texpr_t *texpr,
    size_t *rsize)
{
	switch (texpr->tetype) {
	case irt_int:
		z80_isel_texpr_int_sizeof(isel, texpr, rsize);
		return EOK;
	case irt_ptr:
		z80_isel_texpr_ptr_sizeof(isel, texpr, rsize);
		return EOK;
	case irt_array:
		return z80_isel_texpr_array_sizeof(isel, texpr, rsize);
	case irt_ident:
		return z80_isel_texpr_ident_sizeof(isel, texpr, rsize);
	}

	assert(false);
	return 0;
}

/** Get record member offset.
 *
 * @param isel Instruction selector
 * @param texpr IR identifier type expression (record)
 * @param member Member identifier
 * @param rsize Place to store offset in bytes
 * @return EOK on success or an error code
 */
static int z80_isel_recmbr_off(z80_isel_t *isel, ir_texpr_t *texpr,
    const char *member, uint16_t *roff)
{
	ir_decln_t *decln;
	ir_record_t *record;
	ir_record_elem_t *elem;
	size_t esize;
	size_t off;
	int rc;

	assert(texpr->tetype == irt_ident);

	rc = ir_module_find(isel->irmodule, texpr->t.tident.ident, &decln);
	if (rc != EOK)
		return ENOENT;

	if (decln->dtype != ird_record)
		return EINVAL;

	record = (ir_record_t *)decln->ext;

	if (record->rtype == irrt_union) {
		/* For unions, offset of any member is zero */
		*roff = 0;
		return EOK;
	}

	/* Structure */

	elem = ir_record_first(record);
	off = 0;
	while (elem != NULL) {
		if (strcmp(member, elem->ident) == 0) {
			*roff = (uint16_t)off;
			return EOK;
		}

		rc = z80_isel_texpr_sizeof(isel, elem->etype, &esize);
		if (rc != EOK)
			return rc;

		off += esize;
		elem = ir_record_next(elem);
	}

	/* Member not found */
	return ENOENT;
}

/** Determine size of return value from call instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param instr IR instruction
 * @param rsize Place to store return value size in bytes
 * @return EOK on success or error code
 */
static int z80_isel_call_rsize(z80_isel_proc_t *isproc, ir_instr_t *instr,
    unsigned *rsize)
{
	ir_module_t *irmod;
	ir_decln_t *decln;
	ir_oper_var_t *op1;
	ir_proc_t *cproc;
	int rc;

	assert(instr->itype == iri_call);
	assert(instr->op1->optype == iro_var);

	op1 = (ir_oper_var_t *) instr->op1->ext;

	irmod = isproc->irproc->decln.module;
	rc = ir_module_find(irmod, op1->varname, &decln);
	if (rc != EOK)
		return ENOENT;

	if (decln->dtype != ird_proc)
		return EINVAL;

	cproc = (ir_proc_t *)decln->ext;
	if (cproc->rtype != NULL) {
		switch (cproc->rtype->tetype) {
		case irt_int:
			*rsize = cproc->rtype->t.tint.width / 8;
			break;
		case irt_ptr:
			*rsize = cproc->rtype->t.tptr.width / 8;
			break;
		default:
			assert(false);
		}
	} else {
		*rsize = 2;
	}
	return EOK;
}

/** Scan IR instruction for defined variables.
 *
 * @a isproc variable map will be updated to reflect the defined variables
 *
 * @param isproc Instruction selector for procedure to update
 * @param instr IR instruction to scan
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_scan_instr_def_vars(z80_isel_proc_t *isproc,
    ir_instr_t *instr)
{
	ir_oper_var_t *opvar;
	z80_varmap_entry_t *entry;
	unsigned bytes;
	unsigned vrs;
	int rc;

	if (instr->dest != NULL && instr->dest->optype == iro_var) {
		opvar = (ir_oper_var_t *) instr->dest->ext;
		if (z80_isel_is_vreg(opvar->varname)) {
			/* Determine destination variable size */
			switch (instr->itype) {
			case iri_eq:
			case iri_gt:
			case iri_gtu:
			case iri_gteq:
			case iri_gteu:
			case iri_lt:
			case iri_ltu:
			case iri_lteq:
			case iri_lteu:
			case iri_neq:
				/* These return truth value / int / 2 bytes */
				bytes = 2;
				break;
			case iri_call:
				/* Depends on function return type */
				rc = z80_isel_call_rsize(isproc, instr, &bytes);
				if (rc != EOK)
					return rc;
				break;
			default:
				/*
				 * Otherwise size of result == width of
				 * instruction
				 */
				bytes = instr->width / 8;
				break;
			}

			vrs = bytes >= 2 ? bytes / 2 : 1;
			rc = z80_varmap_find(isproc->varmap, opvar->varname,
			    &entry);
			if (rc == ENOENT) {
				rc = z80_varmap_insert(isproc->varmap,
				    opvar->varname, vrs);
				if (rc != EOK)
					return rc;
			}
		}
	}

	return EOK;
}

/** Create variable map for procedure.
 *
 * @param isproc Instruction selector for procedure to update
 * @param irproc IR procedure to scan
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_proc_create_varmap(z80_isel_proc_t *isproc,
    ir_proc_t *irproc)
{
	ir_proc_arg_t *arg;
	ir_lblock_entry_t *entry;
	size_t bytes;
	unsigned vregs;
	int rc;

	if (irproc->rtype != NULL) {
		assert(irproc->rtype->tetype == irt_int ||
		    irproc->rtype->tetype == irt_ptr);

		/* Add hidden first argument for returning 64-bit value */
		if (irproc->rtype->tetype == irt_int &&
		    irproc->rtype->t.tint.width == 64) {
			rc = z80_varmap_insert(isproc->varmap, "%.retval", 1);
			if (rc != EOK)
				return rc;
		}
	}

	arg = ir_proc_first_arg(irproc);
	while (arg != NULL) {
		/* Number of bytes / virtual registers occupied */
		rc = z80_isel_texpr_sizeof(isproc->isel, arg->atype, &bytes);
		if (rc != EOK)
			return rc;

		vregs = bytes >= 2 ? bytes / 2 : 1;

		rc = z80_varmap_insert(isproc->varmap, arg->ident, vregs);
		if (rc != EOK)
			return rc;

		arg = ir_proc_next_arg(arg);
	}

	entry = ir_lblock_first(irproc->lblock);
	while (entry != NULL) {
		if (entry->instr != NULL) {
			rc = z80_isel_scan_instr_def_vars(isproc, entry->instr);
			if (rc != EOK)
				return rc;
		}

		entry = ir_lblock_next(entry);
	}

	return EOK;
}

/** Allocate new virtual register number.
 *
 * @param isproc Instruction selector for procedure
 * @return New virtual register number
 */
static unsigned z80_isel_get_new_vregno(z80_isel_proc_t *isproc)
{
	return isproc->varmap->next_vr++;
}

/** Allocate new virtual register numbers.
 *
 * This can be used to allocate a range of virtual registers to hold
 * value of a particular size.
 *
 * @param isproc Instruction selector for procedure
 * @param bytes Size of value in bytes
 * @return Number of first new virtual register
 */
static unsigned z80_isel_get_new_vregnos(z80_isel_proc_t *isproc,
    unsigned bytes)
{
	unsigned vr;
	unsigned i;

	assert(bytes > 0);
	assert(bytes == 1 || bytes % 2 == 0);

	vr = z80_isel_get_new_vregno(isproc);
	if (bytes > 1) {
		/*
		 * One byte is held in one 8-bit virtual register.
		 * Larger values are held in one or more 16-bit virtual
		 * register pairs.
		 */
		for (i = 1; i < bytes / 2; i++)
			(void) z80_isel_get_new_vregno(isproc);
	}

	return vr;
}

/** Create new local label.
 *
 * Allocate a new number for local label(s).
 *
 * @param isproc Instruction selector for procedure
 * @return New label number
 */
static unsigned z80_isel_new_label_num(z80_isel_proc_t *isproc)
{
	return isproc->next_label++;
}

/** Create new local label.
 *
 * Create a new label (not corresponding to a label in IR). The label
 * should use the IR label naming pattern.
 *
 * @param isproc Instruction selector for procedure
 * @param pattern Label pattern (IR label format)
 * @param lblno Label number
 * @param rlabel Place to store pointer to new label
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_create_label(z80_isel_proc_t *isproc, const char *pattern,
    unsigned lblno, char **rlabel)
{
	char *irlabel;
	char *label;
	int rv;
	int rc;

	rv = asprintf(&irlabel, "%%%s%u", pattern, lblno);
	if (rv < 0)
		return ENOMEM;

	rc = z80_isel_mangle_label_ident(isproc->ident, irlabel, &label);
	if (rc != EOK) {
		free(irlabel);
		return rc;
	}

	free(irlabel);
	*rlabel = label;
	return EOK;
}

/** Create instruction selector.
 *
 * @param rz80_isel Place to store pointer to new instruction selector
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_isel_create(z80_isel_t **rz80_isel)
{
	z80_isel_t *isel;

	isel = calloc(1, sizeof(z80_isel_t));
	if (isel == NULL)
		return ENOMEM;

	*rz80_isel = isel;
	return EOK;
}

/** Create procedure instruction selector.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param risproc Place to store pointer to new procedure instruction selector
 * @return EOK on success, ENOMEM if out of memory
 */
static int z80_isel_proc_create(z80_isel_t *isel, ir_proc_t *irproc,
    z80_isel_proc_t **risproc)
{
	z80_isel_proc_t *isproc;
	char *dident;
	int rc;

	isproc = calloc(1, sizeof(z80_isel_proc_t));
	if (isproc == NULL)
		return ENOMEM;

	dident = strdup(irproc->ident);
	if (dident == NULL) {
		free(isproc);
		return ENOMEM;
	}

	rc = z80_varmap_create(&isproc->varmap);
	if (rc != EOK) {
		free(dident);
		free(isproc);
		return ENOMEM;
	}

	isproc->isel = isel;
	isproc->ident = dident;
	isproc->irproc = irproc;
	*risproc = isproc;
	return EOK;
}

/** Destroy instruction selector for procedure.
 *
 * @param isel Instruction selector or @c NULL
 */
static void z80_isel_proc_destroy(z80_isel_proc_t *isproc)
{
	if (isproc == NULL)
		return;

	z80_varmap_destroy(isproc->varmap);
	free(isproc->ident);
	free(isproc);
}

/** Select Z80 IC instructions code for writing reading value stored in
 * memory to virtual registers.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr Virtual register base where to store the value
 * @param bytes Size of the value in bytes
 * @param addrvr Virtual register containing address
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_read_vrr(z80_isel_proc_t *isproc, unsigned destvr,
    unsigned bytes, unsigned addrvr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r16_vrr_t *ldaddr = NULL;
	z80ic_ld_vr_ihl_t *lddata = NULL;
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_r16_t *adest = NULL;
	z80ic_oper_vrr_t *asrc = NULL;
	z80ic_oper_ss_t *ainc = NULL;
	z80ic_oper_vr_t *ddest = NULL;
	unsigned byte;
	z80ic_vr_part_t part;
	unsigned vroff;
	int rc;

	(void)isproc;

	/*
	 * If we could allocate a new virtual register, we might use
	 * that instead of specifying HL directly, which would, in theory,
	 * allow using IX or IY (if available)
	 */

	/* ld HL, vrrA */

	rc = z80ic_ld_r16_vrr_create(&ldaddr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_hl, &adest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(addrvr, &asrc);
	if (rc != EOK)
		goto error;

	ldaddr->dest = adest;
	ldaddr->src = asrc;
	adest = NULL;
	asrc = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldaddr->instr);
	if (rc != EOK)
		goto error;

	ldaddr = NULL;

	/* Do the same thing for every byte */
	for (byte = 0; byte < bytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, bytes, &part, &vroff);

		/* ld vrrB.X, (HL) */

		rc = z80ic_ld_vr_ihl_create(&lddata);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &ddest);
		if (rc != EOK)
			goto error;

		lddata->dest = ddest;
		ddest = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &lddata->instr);
		if (rc != EOK)
			goto error;

		lddata = NULL;

		/* No need to increment HL in last iteration */
		if (byte >= bytes - 1)
			break;

		/* inc HL */

		rc = z80ic_inc_ss_create(&inc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_ss_create(z80ic_ss_hl, &ainc);
		if (rc != EOK)
			goto error;

		inc->dest = ainc;
		ainc = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
		if (rc != EOK)
			goto error;

		inc = NULL;
	}

	return EOK;
error:
	if (ldaddr != NULL)
		z80ic_instr_destroy(&ldaddr->instr);
	if (lddata != NULL)
		z80ic_instr_destroy(&lddata->instr);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);

	z80ic_oper_r16_destroy(adest);
	z80ic_oper_vrr_destroy(asrc);
	z80ic_oper_ss_destroy(ainc);
	z80ic_oper_vr_destroy(ddest);

	return rc;
}

/** Select Z80 IC instructions code for writing value stored in virtual
 * registers to memory.
 *
 * @param isproc Instruction selector for procedure
 * @param addrvr Virtual register containing address
 * @param srcvr Virtual register base of the value
 * @param bytes Size of the value in bytes
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_write_vrr(z80_isel_proc_t *isproc, unsigned addrvr,
    unsigned srcvr, unsigned bytes, z80ic_lblock_t *lblock)
{
	z80ic_ld_r16_vrr_t *ldaddr = NULL;
	z80ic_ld_ihl_vr_t *lddata = NULL;
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_r16_t *adest = NULL;
	z80ic_oper_vrr_t *asrc = NULL;
	z80ic_oper_ss_t *ainc = NULL;
	z80ic_oper_vr_t *dsrc = NULL;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	int rc;

	/*
	 * If we could allocate a new virtual register, we might use
	 * that instead of specifying HL directly, which would, in theory,
	 * allow using IX or IY (if available)
	 */

	(void)isproc;

	/* ld HL, vrrA */

	rc = z80ic_ld_r16_vrr_create(&ldaddr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_hl, &adest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(addrvr, &asrc);
	if (rc != EOK)
		goto error;

	ldaddr->dest = adest;
	ldaddr->src = asrc;
	adest = NULL;
	asrc = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldaddr->instr);
	if (rc != EOK)
		goto error;

	ldaddr = NULL;

	/* Do the same thing afor every byte */
	for (byte = 0; byte < bytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, bytes, &part, &vroff);

		/* ld (HL), vrrB.X */

		rc = z80ic_ld_ihl_vr_create(&lddata);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(srcvr + vroff, part, &dsrc);
		if (rc != EOK)
			goto error;

		lddata->src = dsrc;
		dsrc = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &lddata->instr);
		if (rc != EOK)
			goto error;

		lddata = NULL;

		/* No need to increment HL in last iteration */
		if (byte >= bytes - 1)
			break;

		/* inc HL */

		rc = z80ic_inc_ss_create(&inc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_ss_create(z80ic_ss_hl, &ainc);
		if (rc != EOK)
			goto error;

		inc->dest = ainc;
		ainc = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
		if (rc != EOK)
			goto error;

		inc = NULL;
	}

	return EOK;
error:
	if (ldaddr != NULL)
		z80ic_instr_destroy(&ldaddr->instr);
	if (lddata != NULL)
		z80ic_instr_destroy(&lddata->instr);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);

	z80ic_oper_r16_destroy(adest);
	z80ic_oper_vrr_destroy(asrc);
	z80ic_oper_ss_destroy(ainc);
	z80ic_oper_vr_destroy(dsrc);

	return rc;
}

/** Select Z80 IC instructions code to load constant into virtual registers.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr Destination virtual register base
 * @param value Value to load
 * @param bytes Number of bytes to copy
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_const(z80_isel_proc_t *isproc, unsigned destvr,
    uint64_t value, unsigned bytes, z80ic_lblock_t *lblock)
{
	z80ic_ld_vrr_nn_t *ldimm = NULL;
	z80ic_oper_vrr_t *vrr = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_ld_vr_n_t *ldimm8 = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	unsigned word;
	int rc;

	(void) isproc;

	assert(bytes == 1 || bytes % 2 == 0);

	if (bytes == 1) {

		rc = z80ic_ld_vr_n_create(&ldimm8);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm8_create(value, &imm8);
		if (rc != EOK)
			goto error;

		ldimm8->dest = vr;
		ldimm8->imm8 = imm8;
		vr = NULL;
		imm8 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldimm8->instr);
		if (rc != EOK)
			goto error;

		ldimm8 = NULL;
	} else {
		for (word = 0; word < bytes / 2; word++) {
			/* ld vrr, NN */

			rc = z80ic_ld_vrr_nn_create(&ldimm);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vrr_create(destvr + word, &vrr);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_imm16_create_val(
			    (value >> 16 * word) & 0xffff, &imm);
			if (rc != EOK)
				goto error;

			ldimm->dest = vrr;
			ldimm->imm16 = imm;
			vrr = NULL;
			imm = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &ldimm->instr);
			if (rc != EOK)
				goto error;

			ldimm = NULL;
		}
	}

	return EOK;
error:
	if (ldimm != NULL)
		z80ic_instr_destroy(&ldimm->instr);
	if (ldimm8 != NULL)
		z80ic_instr_destroy(&ldimm8->instr);

	z80ic_oper_vrr_destroy(vrr);
	z80ic_oper_imm16_destroy(imm);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);

	return rc;
}

/** Select Z80 IC instructions code for copying an initial segment of
 * bytes between virtual registers.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr Destination virtual register base
 * @param dbytes Destination size in bytes
 * @param srcvr Source virtual register base
 * @param sbytes Source size in bytes (can be larger than dbytes)
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_copy_iseg(z80_isel_proc_t *isproc, unsigned destvr,
    unsigned dbytes, unsigned srcvr, unsigned sbytes, z80ic_lblock_t *lblock)
{
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	unsigned byte;
	z80ic_vr_part_t part;
	unsigned vroff;
	unsigned cbytes;
	int rc;

	(void) isproc;

	/* Copy cbytes = min(sbytes, dbytes) */
	if (dbytes < sbytes)
		cbytes = dbytes;
	else
		cbytes = sbytes;

	/* Do the same thing for every byte */
	for (byte = 0; byte < cbytes; byte++) {
		/* Determine source register part and offset */
		z80_isel_reg_part_off(byte, sbytes, &part, &vroff);

		/* ld A, vrr.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(srcvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* Determine destination register part and offset */
		z80_isel_reg_part_off(byte, dbytes, &part, &vroff);

		/* ld vrr.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;

	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for copying a value between virtual
 * registers.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr Destination virtual register base
 * @param srcvr Source virtual register base
 * @param bytes Number of bytes to copy
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_copy(z80_isel_proc_t *isproc, unsigned destvr,
    unsigned srcvr, unsigned bytes, z80ic_lblock_t *lblock)
{
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	unsigned byte;
	z80ic_vr_part_t part;
	unsigned vroff;
	int rc;

	(void) isproc;

	/* Do the same thing for every byte */
	for (byte = 0; byte < bytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, bytes, &part, &vroff);

		/* ld A, vrr.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(srcvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* ld vrr.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;

	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for negating a value stored in
 * virtual registers.
 *
 * Destination virtual registers can be the same as, or diferent to
 * the source registers.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr First destination virtual register (pair)
 * @param srcvr First source virtual register (pair)
 * @param bytes Size of value in bytes
 *
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_neg_vrr(z80_isel_proc_t *isproc, unsigned destvr,
    unsigned srcvr, unsigned bytes, z80ic_lblock_t *lblock)
{
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_vrr_t *vrr = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	z80ic_cpl_t *cpl = NULL;
	z80ic_inc_vrr_t *inc = NULL;
	z80ic_inc_vr_t *incvr = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned byte;
	z80ic_vr_part_t part;
	unsigned vroff;
	char *enlabel = NULL;
	unsigned lblno;
	int rc;

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "end_neg", lblno, &enlabel);
	if (rc != EOK)
		goto error;

	/* Do the same thing for every byte */
	for (byte = 0; byte < bytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, bytes, &part, &vroff);

		/* ld A, vrr.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(srcvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* cpl */

		rc = z80ic_cpl_create(&cpl);
		if (rc != EOK)
			goto error;

		rc = z80ic_lblock_append(lblock, NULL, &cpl->instr);
		if (rc != EOK)
			goto error;

		cpl = NULL;

		/* ld vrr.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;

	}

	/* 16-bit case can use 16-bit instruction */
	if (bytes == 2) {
		// XXX Should we be able to turn this optimization off?

		/* inc vrr */

		rc = z80ic_inc_vrr_create(&inc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(destvr, &vrr);
		if (rc != EOK)
			goto error;

		inc->vrr = vrr;
		vrr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
		if (rc != EOK)
			goto error;

		inc = NULL;
	} else {
		/* General case */

		for (byte = 0; byte < bytes; byte++) {
			/* Determine register part and offset */
			z80_isel_reg_part_off(byte, bytes, &part, &vroff);

			/* inc vr */

			rc = z80ic_inc_vr_create(&incvr);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
			if (rc != EOK)
				goto error;

			incvr->vr = vr;
			vr = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &incvr->instr);
			if (rc != EOK)
				goto error;

			incvr = NULL;

			/* No need for conditional jump at the very end */
			if (byte == bytes - 1)
				break;

			/* jp NZ, end_neg */

			rc = z80ic_jp_cc_nn_create(&jpcc);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_imm16_create_symbol(enlabel, &imm16);
			if (rc != EOK)
				goto error;

			jpcc->cc = z80ic_cc_nz;
			jpcc->imm16 = imm16;
			imm16 = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
			if (rc != EOK)
				goto error;

			jpcc = NULL;
		}

		if (bytes > 1) {
			/* label end_neg */

			rc = z80ic_lblock_append(lblock, enlabel, NULL);
			if (rc != EOK)
				goto error;
		}
	}

	free(enlabel);
	return EOK;
error:
	if (enlabel != NULL)
		free(enlabel);
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	if (cpl != NULL)
		z80ic_instr_destroy(&cpl->instr);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_vrr_destroy(vrr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for adding two values in virtual
 * registers to another value in virtual registers.
 *
 * The destination may be the same as one of the source register ranges.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr First destination virtual register number
 * @param vr1 First left operand virtual register number
 * @param vr2 First right operand virtual register number
 * @param bytes Size of the value in bytes
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_add(z80_isel_proc_t *isproc, unsigned destvr,
    unsigned vr1, unsigned vr2, unsigned bytes, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *dest = NULL;
	z80ic_oper_vr_t *src = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	z80ic_add_a_vr_t *add = NULL;
	z80ic_adc_a_vr_t *adc = NULL;
	z80ic_vr_part_t part;
	unsigned vroff;
	unsigned byte;
	int rc;

	(void) isproc;

	assert(bytes > 0);

	/* For each byte */
	for (byte = 0; byte < bytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, bytes, &part, &vroff);

		/* ld A, vr1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &src);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = src;
		reg = NULL;
		src = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		if (byte == 0) {
			/* add A, vr2 */

			rc = z80ic_add_a_vr_create(&add);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vr_create(vr2 + vroff, part, &src);
			if (rc != EOK)
				goto error;

			add->src = src;
			dest = NULL;
			src = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &add->instr);
			if (rc != EOK)
				goto error;
		} else {
			/* adc A, vr2 */
			rc = z80ic_adc_a_vr_create(&adc);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vr_create(vr2 + vroff, part, &src);
			if (rc != EOK)
				goto error;

			adc->src = src;
			dest = NULL;
			src = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &adc->instr);
			if (rc != EOK)
				goto error;
		}

		/* ld destvr.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &dest);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = dest;
		ldvrr->src = reg;
		dest = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;
	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	if (add != NULL)
		z80ic_instr_destroy(&add->instr);
	if (adc != NULL)
		z80ic_instr_destroy(&adc->instr);
	z80ic_oper_vr_destroy(dest);
	z80ic_oper_vr_destroy(src);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for shifting left value in virtual
 * registers by 1.
 *
 * @param isproc Instruction selector for procedure
 * @param vregno First virtual register number
 * @param bytes Size of value in bytes
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_shl(z80_isel_proc_t *isproc, unsigned vregno,
    unsigned bytes, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *dvr = NULL;
	z80ic_sla_vr_t *sla = NULL;
	z80ic_rl_vr_t *rl = NULL;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	int rc;

	(void) isproc;
	assert(bytes > 0);

	z80_isel_reg_part_off(0, bytes, &part, &vroff);

	/* sla dest.<LSB> */

	rc = z80ic_sla_vr_create(&sla);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vregno + vroff, part, &dvr);
	if (rc != EOK)
		goto error;

	sla->vr = dvr;
	dvr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sla->instr);
	if (rc != EOK)
		goto error;

	sla = NULL;

	for (byte = 1; byte < bytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, bytes, &part, &vroff);

		/* rl dest.X */

		rc = z80ic_rl_vr_create(&rl);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vregno + vroff, part, &dvr);
		if (rc != EOK)
			goto error;

		rl->vr = dvr;
		dvr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &rl->instr);
		if (rc != EOK)
			goto error;

		rl = NULL;
	}

	return EOK;
error:
	if (sla != NULL)
		z80ic_instr_destroy(&sla->instr);
	if (rl != NULL)
		z80ic_instr_destroy(&rl->instr);

	z80ic_oper_vr_destroy(dvr);
	return rc;
}

/** Select Z80 IC instructions code for shifting right value in virtual
 * registers by 1.
 *
 * @param isproc Instruction selector for procedure
 * @param vregno First virtual register number
 * @param bytes Size of value in bytes
 * @param arithm Arithmetic (signed) shift
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_shr(z80_isel_proc_t *isproc, unsigned vregno,
    unsigned bytes, bool arithm, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *dvr = NULL;
	z80ic_sra_vr_t *sra = NULL;
	z80ic_srl_vr_t *srl = NULL;
	z80ic_rr_vr_t *rr = NULL;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	int rc;

	(void) isproc;
	assert(bytes > 0);

	z80_isel_reg_part_off(bytes - 1, bytes, &part, &vroff);

	if (arithm) {
		/* sra dest.<MSB> */

		rc = z80ic_sra_vr_create(&sra);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vregno + vroff, part, &dvr);
		if (rc != EOK)
			goto error;

		sra->vr = dvr;
		dvr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sra->instr);
		if (rc != EOK)
			goto error;

		sra = NULL;
	} else {
		/* srl dest.<MSB> */

		rc = z80ic_srl_vr_create(&srl);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vregno + vroff, part, &dvr);
		if (rc != EOK)
			goto error;

		srl->vr = dvr;
		dvr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &srl->instr);
		if (rc != EOK)
			goto error;

		srl = NULL;
	}

	/* From second most significant byte to the least significant */
	for (byte = 1; byte < bytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(bytes - 1 - byte, bytes, &part, &vroff);

		/* rr dest.X */

		rc = z80ic_rr_vr_create(&rr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vregno + vroff, part, &dvr);
		if (rc != EOK)
			goto error;

		rr->vr = dvr;
		dvr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &rr->instr);
		if (rc != EOK)
			goto error;

		rr = NULL;
	}

	return EOK;
error:
	if (sra != NULL)
		z80ic_instr_destroy(&sra->instr);
	if (srl != NULL)
		z80ic_instr_destroy(&srl->instr);
	if (rr != NULL)
		z80ic_instr_destroy(&rr->instr);

	z80ic_oper_vr_destroy(dvr);
	return rc;
}

/** Select Z80 IC instructions code for multiplying value in virtual
 * registers by a constant.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr First destination virtual register number
 * @param cfac Constant factor
 * @param vr First operand virtual register number
 * @param bytes Size of the value in bytes
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_cmul(z80_isel_proc_t *isproc, unsigned destvr,
    uint64_t cfac, unsigned vr2, unsigned bytes, z80ic_lblock_t *lblock)
{
	unsigned tvr;
	unsigned i;
	unsigned used_bits;
	int rc;

	(void)cfac;

	/* Allocate virtual registers for temporary storage */
	tvr = z80_isel_get_new_vregnos(isproc, bytes);

	/* t := vr2 */

	rc = z80_isel_vrr_copy(isproc, tvr, vr2, bytes, lblock);
	if (rc != EOK)
		goto error;

	/* dest := 0 */

	rc = z80_isel_vrr_const(isproc, destvr, 0, bytes, lblock);
	if (rc != EOK)
		goto error;

	/* Find number of used bits in constant factor */
	used_bits = 0;
	for (i = 0; i < bytes * 8; i++) {
		if ((cfac & ((uint64_t)1 << i)) != 0)
			used_bits = i + 1;
	}

	for (i = 0; i < used_bits; i++) {
		if ((cfac & ((uint64_t)1 << i)) != 0) {
			/* dest += t */

			rc = z80_isel_vrr_add(isproc, destvr, destvr, tvr,
			    bytes, lblock);
			if (rc != EOK)
				goto error;
		}

		/* No need to shift after the last addition */
		if (i + 1 < used_bits) {
			/* t <<= 1 */

			rc = z80_isel_vrr_shl(isproc, tvr, bytes, lblock);
			if (rc != EOK)
				goto error;
		}
	}

	return EOK;
error:
	return rc;
}

/** Select Z80 IC instructions code for extending integer from one set of
 * virtual registers to another.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr Destination virtual register base
 * @param dbytes Destination number of bytes
 * @param srcvr Source virtual register base
 * @param sbytes Source number of bytes
 * @param sgnext @c true iff sign extension, @c false if zero extension
 * @param lblock Labeled block where to append the new instructions
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_extend(z80_isel_proc_t *isproc, unsigned destvr,
    unsigned dbytes, unsigned srcvr, unsigned sbytes, bool sgnext,
    z80ic_lblock_t *lblock)
{
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_xor_r_t *xor = NULL;
	z80ic_bit_b_vr_t *bit = NULL;
	z80ic_dec_r_t *dec = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	char *nnlabel = NULL;
	unsigned byte;
	z80ic_vr_part_t part;
	unsigned vroff;
	unsigned lblno;
	int rc;

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "ext_nonneg", lblno, &nnlabel);
	if (rc != EOK)
		goto error;

	/* Copy the overlapping part */
	rc = z80_isel_vrr_copy_iseg(isproc, destvr, dbytes, srcvr, sbytes,
	    lblock);
	if (rc != EOK)
		return rc;

	/* xor A */

	rc = z80ic_xor_r_create(&xor);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	xor->src = reg;
	reg = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &xor->instr);
	if (rc != EOK)
		goto error;

	xor = NULL;

	if (sgnext) {
		/* bit 7, vrr.X */

		rc = z80ic_bit_b_vr_create(&bit);
		if (rc != EOK)
			goto error;

		/*
		 * Determine register part and offset for the highest
		 * byte of the source operand.
		 */
		z80_isel_reg_part_off(sbytes - 1, sbytes, &part, &vroff);

		rc = z80ic_oper_vr_create(srcvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		bit->bit = 7;
		bit->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &bit->instr);
		if (rc != EOK)
			goto error;

		bit = NULL;

		/* jp P, nonneg */

		rc = z80ic_jp_cc_nn_create(&jpcc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm16_create_symbol(nnlabel, &imm16);
		if (rc != EOK)
			goto error;

		jpcc->cc = z80ic_cc_p;
		jpcc->imm16 = imm16;
		imm16 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
		if (rc != EOK)
			goto error;

		jpcc = NULL;

		/* dec A */

		rc = z80ic_dec_r_create(&dec);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		dec->dest = reg;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &dec->instr);
		if (rc != EOK)
			goto error;

		dec = NULL;

		/* label ext_nonneg */

		rc = z80ic_lblock_append(lblock, nnlabel, NULL);
		if (rc != EOK)
			goto error;
	}

	/* Set all remaining bytes */
	for (byte = sbytes; byte < dbytes; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, dbytes, &part, &vroff);

		/* ld vrr.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;

	}

	free(nnlabel);
	return EOK;
error:
	if (nnlabel != NULL)
		free(nnlabel);
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);
	if (bit != NULL)
		z80ic_instr_destroy(&bit->instr);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);
	z80ic_oper_imm16_destroy(imm16);

	return rc;
}

/** Select Z80 IC instructions code to load local variable address into
 * a virtual register.
 *
 * @param isproc Instruction selector for procedure
 * @param destvr Destination virtual register base
 * @param varname Variable name
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_vrr_lvarptr(z80_isel_proc_t *isproc, unsigned destvr,
    const char *varname, z80ic_lblock_t *lblock)
{
	z80ic_oper_vrr_t *vrr = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_ld_vrr_spnn_t *ld = NULL;
	char *varident = NULL;
	int rc;

	rc = z80_isel_mangle_lvar_ident(isproc->ident, varname, &varident);
	if (rc != EOK)
		goto error;

	/* ld vrr, SP+$varident@SP */

	rc = z80ic_ld_vrr_spnn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &vrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(varident, &imm);
	if (rc != EOK)
		goto error;

	ld->dest = vrr;
	ld->imm16 = imm;
	vrr = NULL;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	free(varident);
	return EOK;
error:
	if (varident != NULL)
		free(varident);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);

	z80ic_oper_vrr_destroy(vrr);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Select Z80 IC instructions code for IR add instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_add(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	unsigned vr1, vr2;
	int rc;

	assert(irinstr->itype == iri_add);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		return rc;

	return z80_isel_vrr_add(isproc, destvr, vr1, vr2,
	    irinstr->width / 8, lblock);
}

/** Select Z80 IC instructions code for IR and instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR and instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_and(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_and_vr_t *and = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	z80ic_vr_part_t part;
	unsigned byte;
	unsigned vroff;
	unsigned destvr;
	unsigned vr1, vr2;
	int rc;

	assert(irinstr->itype == iri_and);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	/* Do the same thing for every byte */
	for (byte = 0; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, vr1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* and vr2.X */

		rc = z80ic_and_vr_create(&and);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		and->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &and->instr);
		if (rc != EOK)
			goto error;

		and = NULL;

		/* ld dest.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;
	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (and != NULL)
		z80ic_instr_destroy(&and->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for IR bnot instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR or instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_bnot(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_cpl_t *cpl = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	z80ic_vr_part_t part;
	unsigned byte;
	unsigned vroff;
	unsigned destvr;
	unsigned vr1;
	int rc;

	assert(irinstr->itype == iri_bnot);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);

	/* Do the same thing for every byte */
	for (byte = 0; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, vr1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* cpl */

		rc = z80ic_cpl_create(&cpl);
		if (rc != EOK)
			goto error;

		rc = z80ic_lblock_append(lblock, NULL, &cpl->instr);
		if (rc != EOK)
			goto error;

		cpl = NULL;

		/* ld dest.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;
	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (cpl != NULL)
		z80ic_instr_destroy(&cpl->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions to get 8-bit IR call return value.
 *
 * @param isproc Instruction selector for procedure
 * @param proc IR procedure being called
 * @param destvr Destination virtual register
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_call_get_retv_8(z80_isel_proc_t *isproc, ir_proc_t *proc,
    const char *label, unsigned destvr, z80ic_lblock_t *lblock)
{
	z80ic_ld_vr_r_t *ld = NULL;
	z80ic_oper_reg_t *ldsrc = NULL;
	z80ic_oper_vr_t *lddest = NULL;
	int rc;

	(void)proc;
	(void)isproc;

	/* ld dest, A */

	rc = z80ic_ld_vr_r_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r8, &lddest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &ldsrc);
	if (rc != EOK)
		goto error;

	ld->dest = lddest;
	ld->src = ldsrc;
	lddest = NULL;
	ldsrc = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_vr_destroy(lddest);
	z80ic_oper_reg_destroy(ldsrc);

	return rc;
}

/** Select Z80 IC instructions to get 16-bit IR call return value.
 *
 * @param isproc Instruction selector for procedure
 * @param proc IR procedure being called
 * @param destvr Destination virtual register
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_call_get_retv_16(z80_isel_proc_t *isproc, ir_proc_t *proc,
    const char *label, unsigned destvr, z80ic_lblock_t *lblock)
{
	z80ic_ld_vrr_r16_t *ld = NULL;
	z80ic_oper_r16_t *ldsrc = NULL;
	z80ic_oper_vrr_t *lddest = NULL;
	bool is_usr;
	int rc;

	(void)isproc;
	is_usr = ir_proc_has_attr(proc, "@usr");

	/* ld dest, r16 */

	rc = z80ic_ld_vrr_r16_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &lddest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(is_usr ? z80ic_r16_bc : z80ic_r16_hl, &ldsrc);
	if (rc != EOK)
		goto error;

	ld->dest = lddest;
	ld->src = ldsrc;
	lddest = NULL;
	ldsrc = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_vrr_destroy(lddest);
	z80ic_oper_r16_destroy(ldsrc);

	return rc;
}

/** Select Z80 IC instructions to get 32-bit IR call return value.
 *
 * @param isproc Instruction selector for procedure
 * @param proc IR procedure being called
 * @param destvr Destination virtual register
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_call_get_retv_32(z80_isel_proc_t *isproc, ir_proc_t *proc,
    const char *label, unsigned destvr, z80ic_lblock_t *lblock)
{
	z80ic_ld_vrr_r16_t *ld = NULL;
	z80ic_oper_r16_t *ldsrc = NULL;
	z80ic_oper_vrr_t *lddest = NULL;
	unsigned i;
	int rc;

	(void)proc;
	(void)isproc;

	for (i = 0; i < 2; i++) {
		/* ld dest, r16 */

		rc = z80ic_ld_vrr_r16_create(&ld);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(destvr + i, &lddest);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_r16_create(i > 0 ? z80ic_r16_de : z80ic_r16_hl,
		    &ldsrc);
		if (rc != EOK)
			goto error;

		ld->dest = lddest;
		ld->src = ldsrc;
		lddest = NULL;
		ldsrc = NULL;

		rc = z80ic_lblock_append(lblock, label, &ld->instr);
		if (rc != EOK)
			goto error;

		ld = NULL;
	}

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	z80ic_oper_vrr_destroy(lddest);
	z80ic_oper_r16_destroy(ldsrc);

	return rc;
}

/** Select Z80 IC instructions to get 64-bit IR call return value.
 *
 * 64-bit value does not fit in registers. We must retrieve it from
 * a local variable where it was written by the function (we passed
 * the address of this local variable as the first argument to the function).
 *
 * @param isproc Instruction selector for procedure
 * @param proc IR procedure being called
 * @param destvr Destination virtual register
 * @param addrvr Virtual register containing address of return value
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_call_get_retv_64(z80_isel_proc_t *isproc, ir_proc_t *proc,
    const char *label, unsigned destvr, unsigned addrvr, z80ic_lblock_t *lblock)
{
	(void)proc;
	(void)label;

	/*
	 * Read return value from local variable that was allocated
	 * by z80_isel_call()
	 */

	return z80_isel_read_vrr(isproc, destvr, 8, addrvr, lblock);
}

static int z80_isel_alloc_retvar(z80_isel_proc_t *isproc, uint16_t size,
    uint16_t *roff)
{
	char *icident = NULL;
	z80ic_lvar_t *icvar = NULL;
	int rc;

	rc = z80_isel_mangle_lvar_ident(isproc->ident, "%_retvar",
	    &icident);
	if (rc != EOK)
		goto error;

	rc = z80ic_lvar_create(icident, isproc->icproc->lvar_sz, &icvar);
	if (rc != EOK)
		goto error;

	free(icident);
	icident = NULL;

	z80ic_proc_append_lvar(isproc->icproc, icvar);

	*roff = isproc->icproc->lvar_sz;
	isproc->icproc->lvar_sz += size;
	return EOK;
error:
	if (icident != NULL)
		free(icident);
	return rc;
}

/** Compute the address of return variable to a new virtual register.
 *
 * @param isproc Instruction selector for procedure
 * @param varname Return variable name
 * @param raddrvr Place to store virtual register that holds the address
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_retvar_ptr(z80_isel_proc_t *isproc, const char *varname,
    unsigned *raddrvr, z80ic_lblock_t *lblock)
{
	unsigned addrvr;
	int rc;

	addrvr = z80_isel_get_new_vregno(isproc);
	rc = z80_isel_vrr_lvarptr(isproc, addrvr, varname, lblock);
	if (rc != EOK)
		return rc;

	*raddrvr = addrvr;
	return EOK;
}

/** Select Z80 IC instructions code for setting argument to procedure call.
 *
 * @param isproc Instruction selector for procedure
 * @param argloc Argument locations
 * @param argvr Virtual register base
 * @param varname Variable name
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_call_set_arg(z80_isel_proc_t *isproc, z80_argloc_t *argloc,
    unsigned argvr, const char *varname, z80ic_lblock_t *lblock)
{
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_vrr_t *vrr = NULL;
	z80ic_oper_r16_t *ldadest = NULL;
	z80ic_ld_r16_vrr_t *ldarg = NULL;
	z80ic_ld_r_vr_t *ldarg8 = NULL;
	z80ic_push_vrr_t *push = NULL;
	z80ic_push_vr_t *push8 = NULL;
	z80_argloc_entry_t *entry;
	z80ic_reg_t r;
	unsigned vroff;
	z80ic_r16_t argreg;
	unsigned i;
	int rc;

	(void) isproc;

	/** Find argument location */
	rc = z80_argloc_find(argloc, varname, &entry);
	assert(rc == EOK);
	if (rc != EOK)
		return rc;

	/*
	 * First push to the stack. This requires us to clobber
	 * registers. We go backwards (from the most significant
	 * word to the least), so moving toward lower-number
	 * virtual registers.
	 */

	vroff = entry->reg_entries + (entry->stack_sz + 1) / 2 - 1;

	/* Push words */
	for (i = 0; i + 1 < entry->stack_sz; i += 2) {
		/* push vrr */

		rc = z80ic_push_vrr_create(&push);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(argvr + vroff, &vrr);
		if (rc != EOK)
			goto error;

		push->src = vrr;
		vrr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &push->instr);
		if (rc != EOK)
			goto error;

		push = NULL;
		--vroff;
	}

	/*
	 * Push bytes. We pad bytes to 16-bit stack entries.
	 * The lower part contains the data, the contents of
	 * the upper part is undefined.
	 */
	for (; i < entry->stack_sz; i++) {
		/* push vr */

		rc = z80ic_push_vr_create(&push8);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(argvr + vroff, z80ic_vrp_r8,
		    &vr);
		if (rc != EOK)
			goto error;

		push8->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &push8->instr);
		if (rc != EOK)
			goto error;

		push8 = NULL;
		--vroff;
	}

	/*
	 * Now fill registers (last to first). Again moving
	 * from higher- to lower-numbered virtual registers.
	 */

	for (i = 0; i < entry->reg_entries; i++) {
		if (entry->reg[entry->reg_entries - 1 - i].part ==
		    z80_argloc_hl) {
			/* 16-bit register */

			/* ld r16, vrr */

			rc = z80ic_ld_r16_vrr_create(&ldarg);
			if (rc != EOK)
				goto error;

			argreg = entry->reg[entry->reg_entries - 1 - i].reg;

			rc = z80ic_oper_r16_create(argreg, &ldadest);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vrr_create(argvr + vroff, &vrr);
			if (rc != EOK)
				goto error;

			ldarg->dest = ldadest;
			ldarg->src = vrr;
			ldadest = NULL;
			vrr = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &ldarg->instr);
			if (rc != EOK)
				goto error;

			ldarg = NULL;
		} else {
			/* 8-bit register */

			/* ld r, vr */

			rc = z80ic_ld_r_vr_create(&ldarg8);
			if (rc != EOK)
				goto error;

			argreg = entry->reg[entry->reg_entries - 1 - i].reg;
			z80_argloc_r16_part_to_r(argreg,
			    entry->reg[entry->reg_entries - 1 - i].part, &r);

			rc = z80ic_oper_reg_create(r, &reg);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vr_create(argvr + vroff,
			    z80ic_vrp_r8, &vr);
			if (rc != EOK)
				goto error;

			ldarg8->dest = reg;
			ldarg8->src = vr;
			reg = NULL;
			vr = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &ldarg8->instr);
			if (rc != EOK)
				goto error;

			ldarg8 = NULL;
		}

		--vroff;
	}

	return EOK;
error:
	if (ldarg != NULL)
		z80ic_instr_destroy(&ldarg->instr);
	if (push != NULL)
		z80ic_instr_destroy(&push->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vrr_destroy(vrr);

	return rc;
}

/** Select Z80 IC instructions code for IR call instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR call instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_call(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_call_nn_t *call = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_inc_ss_t *inc = NULL;
	z80ic_oper_ss_t *ainc = NULL;
	z80_argloc_t *argloc = NULL;
	z80_argloc_entry_t *entry;
	ir_oper_var_t *op1;
	ir_oper_list_t *op2;
	ir_oper_t *arg;
	ir_oper_var_t *argvar;
	ir_decln_t *pdecln;
	ir_proc_t *proc;
	ir_proc_arg_t *parg;
	unsigned argvr;
	char *varident = NULL;
	unsigned destvr;
	unsigned bits;
	uint16_t rvoff;
	unsigned rvavr = 0;
	unsigned rvbits;
	unsigned i;
	int rc;

	assert(irinstr->itype == iri_call);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_list);

	if (irinstr->dest != NULL)
		destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	else
		destvr = 0;

	op1 = (ir_oper_var_t *) irinstr->op1->ext;
	op2 = (ir_oper_list_t *) irinstr->op2->ext;

	rc = z80_isel_mangle_global_ident(op1->varname, &varident);
	if (rc != EOK)
		goto error;

	rc = ir_module_find(isproc->isel->irmodule, op1->varname, &pdecln);
	if (rc != EOK) {
		fprintf(stderr, "Call to undefined procedure '%s'.\n",
		    op1->varname);
		goto error;
	}

	if (pdecln->dtype != ird_proc) {
		fprintf(stderr, "Calling object '%s' which is not a procedure.\n",
		    op1->varname);
		rc = EINVAL;
		goto error;
	}

	proc = (ir_proc_t *)pdecln->ext;

	rc = z80_argloc_create(&argloc);
	if (rc != EOK)
		goto error;

	/* 64-bit return value? */
	if (proc->rtype != NULL && proc->rtype->tetype == irt_int) {
		if (proc->rtype->t.tint.width == 64) {
			/* Allocate local variable to hold the return value */
			rc = z80_isel_alloc_retvar(isproc, 8, &rvoff);
			if (rc != EOK)
				goto error;

			rc = z80_isel_retvar_ptr(isproc, "%_retvar", &rvavr,
			    lblock);
			if (rc != EOK)
				goto error;

			/** Allocate hidden argument location */
			rc = z80_argloc_alloc(argloc, "%_retvar", 2, &entry);
			if (rc != EOK)
				goto error;
		}
	}

	/* For arguments first to last */
	arg = ir_oper_list_first(op2);
	parg = ir_proc_first_arg(proc);
	while (arg != NULL) {
		if (parg == NULL) {
			/* Too many arguments */
			fprintf(stderr, "Too many arguments to procedure "
			    "'%s'.\n", op1->varname);
			rc = EINVAL;
			goto error;
		}

		if (parg->atype->tetype == irt_int) {
			bits = parg->atype->t.tint.width;
		} else if (parg->atype->tetype == irt_ptr) {
			bits = parg->atype->t.tptr.width;
		} else {
			fprintf(stderr, "Unsupported argument type (%d)\n",
			    parg->atype->tetype);
			goto error;
		}

		assert(arg->optype == iro_var);
		argvar = (ir_oper_var_t *) arg->ext;

		/** Allocate argument location */
		rc = z80_argloc_alloc(argloc, argvar->varname,
		    (bits + 7) / 8, &entry);
		if (rc != EOK)
			goto error;

		arg = ir_oper_list_next(arg);
		parg = ir_proc_next_arg(parg);
	}

	if (parg != NULL) {
		/* Too few arguments */
		fprintf(stderr, "Too few arguments to procedure "
		    "'%s'.\n", op1->varname);
		rc = EINVAL;
		goto error;
	}

	/*
	 * Process arguments from last to the first. This ensures that
	 * (1) the argument at the top of the stack has the lowest number,
	 * (2) arguments passed by registers are loaded into those registers
	 * just prior to the call instruction (thus not occupying the
	 * registers longer than necessary).
	 *
	 * XXX We should explicitly process stack arguments first and
	 * register arguments second, we might have a late register
	 * entry in the form of an 8-bit argument.
	 */
	arg = ir_oper_list_last(op2);
	while (arg != NULL) {
		assert(arg->optype == iro_var);
		argvar = (ir_oper_var_t *) arg->ext;

		argvr = z80_isel_get_vregno(isproc, arg);

		rc = z80_isel_call_set_arg(isproc, argloc, argvr,
		    argvar->varname, lblock);
		if (rc != EOK)
			goto error;

		arg = ir_oper_list_prev(arg);
	}

	/* 64-bit return value? Pass hidden argument. */
	if (proc->rtype != NULL && proc->rtype->tetype == irt_int) {
		if (proc->rtype->t.tint.width == 64) {
			rc = z80_isel_call_set_arg(isproc, argloc, rvavr,
			    "%_retvar", lblock);
			if (rc != EOK)
				goto error;
		}
	}

	/* call NN */

	rc = z80ic_call_nn_create(&call);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(varident, &imm);
	if (rc != EOK)
		goto error;

	call->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &call->instr);
	if (rc != EOK)
		goto error;

	call = NULL;

	/* Get return value */

	if (proc->rtype != NULL) {
		switch (proc->rtype->tetype) {
		case irt_int:
			rvbits = proc->rtype->t.tint.width;
			break;
		case irt_ptr:
			rvbits = proc->rtype->t.tptr.width;
			break;
		default:
			assert(false);
		}

		switch (rvbits) {
		case 8:
			rc = z80_isel_call_get_retv_8(isproc, proc, label,
			    destvr, lblock);
			break;
		case 16:
			rc = z80_isel_call_get_retv_16(isproc, proc, label,
			    destvr, lblock);
			break;
		case 32:
			rc = z80_isel_call_get_retv_32(isproc, proc, label,
			    destvr, lblock);
			break;
		case 64:
			rc = z80_isel_call_get_retv_64(isproc, proc, label,
			    destvr, rvavr, lblock);
			break;
		}
	}

	if (rc != EOK)
		goto error;

	/*
	 * Remove arguments from the stack.
	 */
	arg = ir_oper_list_last(op2);
	while (arg != NULL) {
		assert(arg->optype == iro_var);
		argvar = (ir_oper_var_t *) arg->ext;

		/** Find argument location */
		rc = z80_argloc_find(argloc, argvar->varname, &entry);
		assert(rc == EOK);
		if (rc != EOK)
			return rc;

		for (i = 0; i < entry->stack_sz; i++) {

			/* inc SP */

			rc = z80ic_inc_ss_create(&inc);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_ss_create(z80ic_ss_sp, &ainc);
			if (rc != EOK)
				goto error;

			inc->dest = ainc;
			ainc = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
			if (rc != EOK)
				goto error;

			inc = NULL;
		}

		arg = ir_oper_list_prev(arg);
	}

	free(varident);
	z80_argloc_destroy(argloc);
	return EOK;
error:
	if (varident != NULL)
		free(varident);
	if (call != NULL)
		z80ic_instr_destroy(&call->instr);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	z80ic_oper_imm16_destroy(imm);
	z80ic_oper_ss_destroy(ainc);
	if (argloc != NULL)
		z80_argloc_destroy(argloc);

	return rc;
}

/** Select Z80 IC instructions code for IR eq instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR eq instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_eq(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	char *false_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_eq);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "eq_false", lblno, &false_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "eq_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	/* Do the same thing for every byte */
	for (byte = 0; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sub op2.L */

		rc = z80ic_sub_vr_create(&sub);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sub->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
		if (rc != EOK)
			goto error;

		sub = NULL;

		/* jp NZ, eq_false */

		rc = z80ic_jp_cc_nn_create(&jpcc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm16_create_symbol(false_lbl, &imm16);
		if (rc != EOK)
			goto error;

		jpcc->cc = z80ic_cc_nz;
		jpcc->imm16 = imm16;
		imm16 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
		if (rc != EOK)
			goto error;

		jpcc = NULL;
	}

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp eq_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label eq_false */

	rc = z80ic_lblock_append(lblock, false_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label eq_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(false_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (false_lbl != NULL)
		free(false_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR gt instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR gt instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_gt(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	char *true_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_gt);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "gt_true", lblno, &true_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "gt_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width, &part, &vroff);

	/* ld A, op2.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op1.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op2.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op1.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;
	}

	/* jp M, gt_true */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(true_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_m;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp gt_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label gt_true */

	rc = z80ic_lblock_append(lblock, true_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label gt_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(true_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (true_lbl != NULL)
		free(true_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR gtu instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR gtu instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_gtu(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	char *true_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_gtu);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "gtu_true", lblno, &true_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "gtu_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width, &part, &vroff);

	/* ld A, op2.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op1.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op2.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op1.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;
	}

	/* jp C, gtu_true */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(true_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_c;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp gtu_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label gtu_true */

	rc = z80ic_lblock_append(lblock, true_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label gtu_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(true_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (true_lbl != NULL)
		free(true_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR gteq instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR gteq instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_gteq(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	char *false_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_gteq);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "gteq_false", lblno, &false_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "gteq_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* ld A, op1.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op2.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op2.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;

	}

	/* jp M, gteq_false */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(false_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_m;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp gteq_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label gteq_false */

	rc = z80ic_lblock_append(lblock, false_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label gteq_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(false_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (false_lbl != NULL)
		free(false_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR gteu instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR gteu instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_gteu(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	char *false_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_gteu);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "gteu_false", lblno, &false_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "gteu_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* ld A, op1.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op2.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op2.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;

	}

	/* jp C, gteu_false */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(false_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_c;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp gteu_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label gteu_false */

	rc = z80ic_lblock_append(lblock, false_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label gteu_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(false_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (false_lbl != NULL)
		free(false_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR sgnext instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR trunc instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_sgnext(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	unsigned vr1;
	unsigned srcw;
	ir_oper_imm_t *op2i;
	int rc;

	assert(irinstr->itype == iri_sgnext);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_imm);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	op2i = (ir_oper_imm_t *)irinstr->op2->ext;
	srcw = op2i->value;

	assert(srcw % 8 == 0);
	assert(irinstr->width > srcw);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		return rc;

	return z80_isel_vrr_extend(isproc, destvr, irinstr->width / 8,
	    vr1, srcw / 8, true, lblock);
}

/** Select Z80 IC instructions code for IR shl instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR shl instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_shl(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *dvr = NULL;
	z80ic_oper_vr_t *svr = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_ld_vr_vr_t *ldvr = NULL;
	z80ic_dec_vr_t *dec = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned cntvr;
	unsigned lblno;
	char *rep_lbl = NULL;
	char *end_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_shl);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	cntvr = z80_isel_get_new_vregno(isproc);
	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "shl_rep", lblno, &rep_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "shl_end", lblno, &end_lbl);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		goto error;

	/* destvr := vr1 */

	rc = z80_isel_vrr_copy(isproc, destvr, vr1, irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* ld cnt, vr2.L */

	rc = z80ic_ld_vr_vr_create(&ldvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(cntvr, z80ic_vrp_r8, &dvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2, z80ic_vrp_r16l, &svr);
	if (rc != EOK)
		goto error;

	ldvr->dest = dvr;
	ldvr->src = svr;
	dvr = NULL;
	svr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvr->instr);
	if (rc != EOK)
		goto error;

	ldvr = NULL;

	/* label shl_rep */

	rc = z80ic_lblock_append(lblock, rep_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* dec cnt */

	rc = z80ic_dec_vr_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(cntvr, z80ic_vrp_r8, &dvr);
	if (rc != EOK)
		goto error;

	dec->vr = dvr;
	dvr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &dec->instr);
	if (rc != EOK)
		goto error;

	dec = NULL;

	/* jp M, shl_end */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(end_lbl, &imm);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_m;
	jpcc->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* dest <<= 1 */

	rc = z80_isel_vrr_shl(isproc, destvr, irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* jp shl_rep */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rep_lbl, &imm);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label shl_end */

	rc = z80ic_lblock_append(lblock, end_lbl, NULL);
	if (rc != EOK)
		goto error;

	free(rep_lbl);
	free(end_lbl);
	return EOK;
error:
	if (ldvr != NULL)
		z80ic_instr_destroy(&ldvr->instr);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_vr_destroy(dvr);
	z80ic_oper_vr_destroy(svr);
	z80ic_oper_imm16_destroy(imm);

	if (rep_lbl != NULL)
		free(rep_lbl);
	if (end_lbl != NULL)
		free(end_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR shra/shrl instructions.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR shr instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_shr(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *dvr = NULL;
	z80ic_oper_vr_t *svr = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_ld_vr_vr_t *ldvr = NULL;
	z80ic_dec_vr_t *dec = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	unsigned destvr;
	unsigned vroff;
	z80ic_vr_part_t part;
	unsigned vr1, vr2;
	unsigned cntvr;
	unsigned lblno;
	char *rep_lbl = NULL;
	char *end_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_shra || irinstr->itype == iri_shrl);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	cntvr = z80_isel_get_new_vregno(isproc);
	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "shl_rep", lblno, &rep_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "shl_end", lblno, &end_lbl);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		goto error;

	/* destvr := vr1 */

	rc = z80_isel_vrr_copy(isproc, destvr, vr1, irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* ld cnt, vr2.L */

	rc = z80ic_ld_vr_vr_create(&ldvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(cntvr, z80ic_vrp_r8, &dvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2, z80ic_vrp_r16l, &svr);
	if (rc != EOK)
		goto error;

	ldvr->dest = dvr;
	ldvr->src = svr;
	dvr = NULL;
	svr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvr->instr);
	if (rc != EOK)
		goto error;

	ldvr = NULL;

	/* label shl_rep */

	rc = z80ic_lblock_append(lblock, rep_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* dec cnt */

	rc = z80ic_dec_vr_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(cntvr, z80ic_vrp_r8, &dvr);
	if (rc != EOK)
		goto error;

	dec->vr = dvr;
	dvr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &dec->instr);
	if (rc != EOK)
		goto error;

	dec = NULL;

	/* jp M, shl_end */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(end_lbl, &imm);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_m;
	jpcc->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* dest >>= 1 */

	rc = z80_isel_vrr_shr(isproc, destvr, irinstr->width / 8,
	    irinstr->itype == iri_shra, lblock);
	if (rc != EOK)
		goto error;

	/* jp shl_rep */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rep_lbl, &imm);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label shl_end */

	rc = z80ic_lblock_append(lblock, end_lbl, NULL);
	if (rc != EOK)
		goto error;

	free(rep_lbl);
	free(end_lbl);
	return EOK;
error:
	if (ldvr != NULL)
		z80ic_instr_destroy(&ldvr->instr);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_vr_destroy(dvr);
	z80ic_oper_vr_destroy(svr);
	z80ic_oper_imm16_destroy(imm);

	if (rep_lbl != NULL)
		free(rep_lbl);
	if (end_lbl != NULL)
		free(end_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR sub instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR sub instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_sub(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *dest = NULL;
	z80ic_oper_vr_t *src = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_vr_part_t part;
	unsigned vroff;
	unsigned byte;
	unsigned destvr;
	unsigned vr1, vr2;
	int rc;

	assert(irinstr->itype == iri_sub);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	/* For each byte */
	for (byte = 0; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, vr1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &src);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = src;
		reg = NULL;
		src = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		if (byte == 0) {
			/* sub vr2 */

			rc = z80ic_sub_vr_create(&sub);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vr_create(vr2 + vroff, part, &src);
			if (rc != EOK)
				goto error;

			sub->src = src;
			dest = NULL;
			src = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
			if (rc != EOK)
				goto error;
		} else {
			/* sbc vr2 */
			rc = z80ic_sbc_a_vr_create(&sbc);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vr_create(vr2 + vroff, part, &src);
			if (rc != EOK)
				goto error;

			sbc->src = src;
			dest = NULL;
			src = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
			if (rc != EOK)
				goto error;
		}

		/* ld destvr.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &dest);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = dest;
		ldvrr->src = reg;
		dest = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;
	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	z80ic_oper_vr_destroy(dest);
	z80ic_oper_vr_destroy(src);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for IR trunc instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR trunc instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_trunc(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	unsigned vr1;
	unsigned srcw;
	ir_oper_imm_t *op2i;
	int rc;

	assert(irinstr->itype == iri_trunc);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_imm);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	op2i = (ir_oper_imm_t *)irinstr->op2->ext;
	srcw = op2i->value;

	assert(srcw % 8 == 0);
	assert(irinstr->width < srcw);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		return rc;

	return z80_isel_vrr_copy_iseg(isproc, destvr, irinstr->width / 8,
	    vr1, srcw / 8, lblock);
}

/** Select Z80 IC instructions code for IR load immediate instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR immediate instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_imm(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	ir_oper_imm_t *irimm;
	unsigned vregno;
	int rc;

	assert(irinstr->itype == iri_imm);
	assert(irinstr->width > 0);
	assert(irinstr->width == 8 || irinstr->width % 16 == 0);

	assert(irinstr->op1->optype == iro_imm);
	irimm = (ir_oper_imm_t *) irinstr->op1->ext;

	assert(irinstr->op2 == NULL);

	vregno = z80_isel_get_vregno(isproc, irinstr->dest);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		return rc;

	return z80_isel_vrr_const(isproc, vregno, irimm->value,
	    irinstr->width / 8, lblock);
}

/** Select Z80 IC instructions code for IR jump instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR jump instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_jmp(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	ir_oper_var_t *op1;
	char *ident = NULL;
	int rc;

	(void) isproc;

	assert(irinstr->itype == iri_jmp);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	op1 = (ir_oper_var_t *) irinstr->op1->ext;

	rc = z80_isel_mangle_label_ident(isproc->ident, op1->varname, &ident);
	if (rc != EOK)
		goto error;

	/* jp NN */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(ident, &imm);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	free(ident);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Select Z80 IC instructions code for IR jump if not zero instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR jump if not zero instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_jnz(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ld = NULL;
	z80ic_or_vr_t *or = NULL;
	z80ic_jp_cc_nn_t *jp = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_oper_vr_t *src = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	unsigned vr1;
	ir_oper_var_t *op2;
	char *ident = NULL;
	int rc;

	(void) isproc;

	assert(irinstr->itype == iri_jnz);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	op2 = (ir_oper_var_t *) irinstr->op2->ext;

	rc = z80_isel_mangle_label_ident(isproc->ident, op2->varname, &ident);
	if (rc != EOK)
		goto error;

	/* ld A, vr.H */

	rc = z80ic_ld_r_vr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1, z80ic_vrp_r16h, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* or vr.L */

	rc = z80ic_or_vr_create(&or);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1, z80ic_vrp_r16l, &src);
	if (rc != EOK)
		goto error;

	or->src = src;
	src = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &or->instr);
	if (rc != EOK)
		goto error;

	or = NULL;

	/* jp NZ, label */

	rc = z80ic_jp_cc_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(ident, &imm);
	if (rc != EOK)
		goto error;

	jp->cc = z80ic_cc_nz;
	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	free(ident);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (or != NULL)
		z80ic_instr_destroy(&or->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);
	z80ic_oper_reg_destroy(dest);
	z80ic_oper_vr_destroy(src);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Select Z80 IC instructions code for IR jump if zero instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR jump if zero instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_jz(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ld = NULL;
	z80ic_or_vr_t *or = NULL;
	z80ic_jp_cc_nn_t *jp = NULL;
	z80ic_oper_reg_t *dest = NULL;
	z80ic_oper_vr_t *src = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	unsigned vr1;
	ir_oper_var_t *op2;
	char *ident = NULL;
	int rc;

	(void) isproc;

	assert(irinstr->itype == iri_jz);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	op2 = (ir_oper_var_t *) irinstr->op2->ext;

	rc = z80_isel_mangle_label_ident(isproc->ident, op2->varname, &ident);
	if (rc != EOK)
		goto error;

	/* ld A, vr.H */

	rc = z80ic_ld_r_vr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1, z80ic_vrp_r16h, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* or vr.L */

	rc = z80ic_or_vr_create(&or);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1, z80ic_vrp_r16l, &src);
	if (rc != EOK)
		goto error;

	or->src = src;
	src = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &or->instr);
	if (rc != EOK)
		goto error;

	or = NULL;

	/* jp Z, label */

	rc = z80ic_jp_cc_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(ident, &imm);
	if (rc != EOK)
		goto error;

	jp->cc = z80ic_cc_z;
	jp->imm16 = imm;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	free(ident);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (or != NULL)
		z80ic_instr_destroy(&or->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);
	z80ic_oper_reg_destroy(dest);
	z80ic_oper_vr_destroy(src);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Select Z80 IC instructions code for IR lt instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR lt instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_lt(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	char *true_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_lt);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "lt_true", lblno, &true_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "lt_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* ld A, op1.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op2.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op2.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;
	}

	/* jp M, lt_true */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(true_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_m;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp lt_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label lt_true */

	rc = z80ic_lblock_append(lblock, true_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label lt_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(true_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (true_lbl != NULL)
		free(true_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR ltu instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR ltu instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_ltu(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	char *true_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_ltu);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "ltu_true", lblno, &true_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "ltu_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* ld A, op1.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op2.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op2.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;
	}

	/* jp C, ltu_true */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(true_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_c;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp ltu_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label ltu_true */

	rc = z80ic_lblock_append(lblock, true_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label ltu_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(true_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (true_lbl != NULL)
		free(true_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR lteq instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR lteq instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_lteq(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	char *false_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_lteq);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "lteq_false", lblno, &false_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "lteq_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* ld A, op2.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op1.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op2.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op1.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;
	}

	/* jp M, lteq_false */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(false_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_m;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp lteq_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label lteq_false */

	rc = z80ic_lblock_append(lblock, false_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label lteq_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(false_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (false_lbl != NULL)
		free(false_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR lteu instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR lteu instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_lteu(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_sbc_a_vr_t *sbc = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	char *false_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_lteu);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "lteu_false", lblno, &false_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "lteu_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* ld A, op2.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op1.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op2.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sbc A, op1.X */

		rc = z80ic_sbc_a_vr_create(&sbc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sbc->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sbc->instr);
		if (rc != EOK)
			goto error;

		sbc = NULL;
	}

	/* jp C, lteu_false */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(false_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_c;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp lteu_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label lteu_false */

	rc = z80ic_lblock_append(lblock, false_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label lteu_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(false_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (sbc != NULL)
		z80ic_instr_destroy(&sbc->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (false_lbl != NULL)
		free(false_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR local variable pointer instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR lvarptr instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_lvarptr(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	ir_oper_var_t *op1;

	assert(irinstr->itype == iri_lvarptr);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	op1 = (ir_oper_var_t *) irinstr->op1->ext;

	(void)label;
	return z80_isel_vrr_lvarptr(isproc, destvr, op1->varname, lblock);
}

/** Select Z80 IC instructions code for IR mul instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR mul instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_mul(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_ld_vr_n_t *ldn = NULL;
	z80ic_dec_vr_t *dec = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned uvr, tvr;
	unsigned cntvr;
	unsigned lblno;
	char *rep_lbl = NULL;
	char *no_add_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_mul);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->width < 256);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	/* Allocate virtual registers for temporary storage */
	tvr = z80_isel_get_new_vregnos(isproc, irinstr->width / 8);
	uvr = z80_isel_get_new_vregnos(isproc, irinstr->width / 8);
	cntvr = z80_isel_get_new_vregno(isproc);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "mul_rep", lblno, &rep_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "mul_no_add", lblno, &no_add_lbl);
	if (rc != EOK)
		goto error;

	/* t := vr1 */

	rc = z80_isel_vrr_copy(isproc, tvr, vr1, irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* dest := 0 */

	rc = z80_isel_vrr_const(isproc, destvr, 0, irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* u := vr2 */

	rc = z80_isel_vrr_copy(isproc, uvr, vr2, irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* ld cnt, <width> */

	rc = z80ic_ld_vr_n_create(&ldn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(cntvr, z80ic_vrp_r8, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(irinstr->width, &imm8);
	if (rc != EOK)
		goto error;

	ldn->dest = vr;
	ldn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldn->instr);
	if (rc != EOK)
		goto error;

	ldn = NULL;

	/*
	 * Main multiplication loop
	 */

	/* label mul_rep */

	rc = z80ic_lblock_append(lblock, rep_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* u >>= 1 */

	rc = z80_isel_vrr_shr(isproc, uvr, irinstr->width / 8, false, lblock);
	if (rc != EOK)
		goto error;

	/* jp NC, mul_no_add */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(no_add_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_nc;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	/* dest += t */

	rc = z80_isel_vrr_add(isproc, destvr, destvr, tvr,
	    irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* label mul_no_add */

	rc = z80ic_lblock_append(lblock, no_add_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* t <<= 1 */

	rc = z80_isel_vrr_shl(isproc, tvr, irinstr->width / 8, lblock);
	if (rc != EOK)
		goto error;

	/* dec cnt */

	rc = z80ic_dec_vr_create(&dec);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(cntvr, z80ic_vrp_r8, &vr);
	if (rc != EOK)
		goto error;

	dec->vr = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &dec->instr);
	if (rc != EOK)
		goto error;

	dec = NULL;

	/* jp NZ, mul_rep */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rep_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_nz;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	free(rep_lbl);
	free(no_add_lbl);
	return EOK;
error:
	if (ldn != NULL)
		z80ic_instr_destroy(&ldn->instr);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm16_destroy(imm16);
	z80ic_oper_imm8_destroy(imm8);

	if (rep_lbl != NULL)
		free(rep_lbl);
	if (no_add_lbl != NULL)
		free(no_add_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR neg instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR or instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_neg(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	unsigned vr1;
	int rc;

	assert(irinstr->itype == iri_neg);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		return rc;

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);

	return z80_isel_neg_vrr(isproc, destvr, vr1, irinstr->width / 8, lblock);
}

/** Select Z80 IC instructions code for IR neq instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR neq instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_neq(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_n_t *ldvrn = NULL;
	z80ic_sub_vr_t *sub = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_jp_nn_t *jp = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned lblno;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	char *true_lbl = NULL;
	char *rejoin_lbl = NULL;
	int rc;

	assert(irinstr->itype == iri_neq);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "neq_true", lblno, &true_lbl);
	if (rc != EOK)
		goto error;

	rc = z80_isel_create_label(isproc, "neq_rejoin", lblno, &rejoin_lbl);
	if (rc != EOK)
		goto error;

	z80_isel_reg_part_off(0, irinstr->width / 8, &part, &vroff);

	/* ld A, op1.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* sub op2.L */

	rc = z80ic_sub_vr_create(&sub);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
	if (rc != EOK)
		goto error;

	sub->src = vr;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
	if (rc != EOK)
		goto error;

	sub = NULL;

	/* jp NZ, neq_true */

	rc = z80ic_jp_cc_nn_create(&jpcc);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(true_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jpcc->cc = z80ic_cc_nz;
	jpcc->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
	if (rc != EOK)
		goto error;

	jpcc = NULL;

	for (byte = 1; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, op1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* sub op2.X */

		rc = z80ic_sub_vr_create(&sub);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		sub->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &sub->instr);
		if (rc != EOK)
			goto error;

		sub = NULL;

		/* jp NZ, neq_true */

		rc = z80ic_jp_cc_nn_create(&jpcc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm16_create_symbol(true_lbl, &imm16);
		if (rc != EOK)
			goto error;

		jpcc->cc = z80ic_cc_nz;
		jpcc->imm16 = imm16;
		imm16 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
		if (rc != EOK)
			goto error;

		jpcc = NULL;
	}

	/* ld dest.L, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* jp neq_rejoin */

	rc = z80ic_jp_nn_create(&jp);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(rejoin_lbl, &imm16);
	if (rc != EOK)
		goto error;

	jp->imm16 = imm16;
	imm16 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &jp->instr);
	if (rc != EOK)
		goto error;

	jp = NULL;

	/* label neq_true */

	rc = z80ic_lblock_append(lblock, true_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.L, 1 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	/* label neq_rejoin */

	rc = z80ic_lblock_append(lblock, rejoin_lbl, NULL);
	if (rc != EOK)
		goto error;

	/* ld dest.H, 0 */

	rc = z80ic_ld_vr_n_create(&ldvrn);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(0, &imm8);
	if (rc != EOK)
		goto error;

	ldvrn->dest = vr;
	ldvrn->imm8 = imm8;
	vr = NULL;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrn->instr);
	if (rc != EOK)
		goto error;

	ldvrn = NULL;

	free(true_lbl);
	free(rejoin_lbl);
	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrn != NULL)
		z80ic_instr_destroy(&ldvrn->instr);
	if (sub != NULL)
		z80ic_instr_destroy(&sub->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	if (jp != NULL)
		z80ic_instr_destroy(&jp->instr);

	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);

	if (true_lbl != NULL)
		free(true_lbl);
	if (rejoin_lbl != NULL)
		free(rejoin_lbl);

	return rc;
}

/** Select Z80 IC instructions code for IR nop instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR nop instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_nop(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_nop_t *nop = NULL;
	int rc;

	(void) isproc;

	assert(irinstr->itype == iri_nop);

	/* nop */

	rc = z80ic_nop_create(&nop);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &nop->instr);
	if (rc != EOK)
		goto error;

	nop = NULL;
	return EOK;
error:
	if (nop != NULL)
		z80ic_instr_destroy(&nop->instr);

	return rc;
}

/** Select Z80 IC instructions code for IR or instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR or instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_or(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_or_vr_t *or = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	unsigned byte;
	z80ic_vr_part_t part;
	unsigned vroff;
	unsigned destvr;
	unsigned vr1, vr2;
	int rc;

	assert(irinstr->itype == iri_or);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	/* Do the same thing for every byte */
	for (byte = 0; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, vr1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* or vr2.X */

		rc = z80ic_or_vr_create(&or);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		or->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &or->instr);
		if (rc != EOK)
			goto error;

		or = NULL;

		/* ld dest.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;
	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (or != NULL)
		z80ic_instr_destroy(&or->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);

	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for IR read instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR read instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_read(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	unsigned vr;

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr = z80_isel_get_vregno(isproc, irinstr->op1);

	(void)label;

	return z80_isel_read_vrr(isproc, destvr, irinstr->width / 8, vr,
	    lblock);
}

/** Select Z80 IC instructions code for IR reccopy instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR reccopy instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_reccopy(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ld_vr_n_t *ldimm8 = NULL;
	z80ic_ld_vrr_vrr_t *ldvrr = NULL;
	z80ic_ld_vr_ivrr_t *ldvrivrr = NULL;
	z80ic_ld_ivrr_vr_t *ldivrrvr = NULL;
	z80ic_jp_cc_nn_t *jpcc = NULL;
	z80ic_dec_vr_t *dec = NULL;
	z80ic_inc_vrr_t *inc = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16 = NULL;
	z80ic_oper_vrr_t *vrrd = NULL;
	z80ic_oper_vrr_t *vrrs = NULL;
	unsigned nlvr;
	unsigned nhvr;
	unsigned svr;
	unsigned dvr;
	unsigned tvr;
	unsigned vr1;
	unsigned vr2;
	uint8_t high;
	uint8_t low;
	size_t elemsz;
	size_t tocopy;
	unsigned lblno;
	char *clabel = NULL;
	int rc;

	(void)lblock;

	assert(irinstr->itype == iri_reccopy);
	assert(irinstr->width == 0);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);
	assert(irinstr->opt != NULL);

	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	rc = z80_isel_texpr_sizeof(isproc->isel, irinstr->opt, &elemsz);
	if (rc != EOK)
		return rc;

	nlvr = z80_isel_get_new_vregno(isproc);
	nhvr = z80_isel_get_new_vregno(isproc);
	svr = z80_isel_get_new_vregno(isproc);
	dvr = z80_isel_get_new_vregno(isproc);
	tvr = z80_isel_get_new_vregno(isproc);

	lblno = z80_isel_new_label_num(isproc);

	rc = z80_isel_create_label(isproc, "reccopy", lblno, &clabel);
	if (rc != EOK)
		goto error;

	(void)label;

	/*
	 * Here's a generic version using virtual registers.
	 * For large structures LDI(R) is probably much faster,
	 * but not so good for smaller structures, especially with
	 * constant address.
	 *
	 * For these cases this general code can be further optimized
	 * and generate much less register pressure. For larger copies,
	 * we should probably opt for LDI(R).
	 *
	 * Our code can only loop up to 0x7fff times. If the structure
	 * is larger, we need to generate multiple instances of the loop.
	 */

	while (elemsz > 0) {
		if (elemsz < 0x8000)
			tocopy = elemsz;
		else
			tocopy = 0x7fff;

		/*
		 * Because 16-bit decrement does not affect flags and
		 * 8-bit decrement does not affect carry, we are left
		 * with a little strange way of counting down
		 * dec low; jp NZ; dec high; jp P. Thus 00 as
		 * the low byte of the counter means 256. Also we cannot
		 * have the high byte > 0x7f.
		 */
		if ((tocopy & 0xff) == 0) {
			high = (tocopy >> 8) - 1;
			low = 0x00;
		} else {
			high = tocopy >> 8;
			low = tocopy & 0xff;
		}

		/* ld %nl, high */

		rc = z80ic_ld_vr_n_create(&ldimm8);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(nlvr, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm8_create(high, &imm8);
		if (rc != EOK)
			goto error;

		ldimm8->dest = vr;
		ldimm8->imm8 = imm8;
		vr = NULL;
		imm8 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldimm8->instr);
		if (rc != EOK)
			goto error;

		/* ld %nh, low */

		rc = z80ic_ld_vr_n_create(&ldimm8);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(nhvr, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm8_create(low, &imm8);
		if (rc != EOK)
			goto error;

		ldimm8->dest = vr;
		ldimm8->imm8 = imm8;
		vr = NULL;
		imm8 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldimm8->instr);
		if (rc != EOK)
			goto error;

		/* ld %%d, %%op1 */

		rc = z80ic_ld_vrr_vrr_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(dvr, &vrrd);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(vr1, &vrrs);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vrrd;
		ldvrr->src = vrrs;
		vrrd = NULL;
		vrrs = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		/* ld %%s, %%op2 */

		rc = z80ic_ld_vrr_vrr_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(svr, &vrrd);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(vr2, &vrrs);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vrrd;
		ldvrr->src = vrrs;
		vrrd = NULL;
		vrrs = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		/* label %reccopy */

		rc = z80ic_lblock_append(lblock, clabel, NULL);
		if (rc != EOK)
			goto error;

		/* ld %t, (%%s) */

		rc = z80ic_ld_vr_ivrr_create(&ldvrivrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(tvr, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(svr, &vrrs);
		if (rc != EOK)
			goto error;

		ldvrivrr->dest = vr;
		ldvrivrr->isrc = vrrs;
		vr = NULL;
		vrrs = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldvrivrr->instr);
		if (rc != EOK)
			goto error;

		/* inc %%s */

		rc = z80ic_inc_vrr_create(&inc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(svr, &vrrs);
		if (rc != EOK)
			goto error;

		inc->vrr = vrrs;
		vrrs = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
		if (rc != EOK)
			goto error;

		inc = NULL;

		/* ld (%%d), %t */

		rc = z80ic_ld_ivrr_vr_create(&ldivrrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(dvr, &vrrd);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(tvr, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		ldivrrvr->idest = vrrd;
		ldivrrvr->src = vr;
		vrrd = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldivrrvr->instr);
		if (rc != EOK)
			goto error;

		/* inc %%d */

		rc = z80ic_inc_vrr_create(&inc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(dvr, &vrrd);
		if (rc != EOK)
			goto error;

		inc->vrr = vrrd;
		vrrd = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &inc->instr);
		if (rc != EOK)
			goto error;

		inc = NULL;

		/* dec %nl */

		rc = z80ic_dec_vr_create(&dec);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(nlvr, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		dec->vr = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &dec->instr);
		if (rc != EOK)
			goto error;

		dec = NULL;

		/* jp NZ, %reccopy */

		rc = z80ic_jp_cc_nn_create(&jpcc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm16_create_symbol(clabel, &imm16);
		if (rc != EOK)
			goto error;

		jpcc->cc = z80ic_cc_nz;
		jpcc->imm16 = imm16;
		imm16 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
		if (rc != EOK)
			goto error;

		jpcc = NULL;

		/* dec %nh */

		rc = z80ic_dec_vr_create(&dec);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(nhvr, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		dec->vr = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &dec->instr);
		if (rc != EOK)
			goto error;

		dec = NULL;

		/* jp P, %reccopy */

		rc = z80ic_jp_cc_nn_create(&jpcc);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_imm16_create_symbol(clabel, &imm16);
		if (rc != EOK)
			goto error;

		jpcc->cc = z80ic_cc_p;
		jpcc->imm16 = imm16;
		imm16 = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &jpcc->instr);
		if (rc != EOK)
			goto error;

		jpcc = NULL;

		elemsz -= tocopy;
	}

	free(clabel);
	return EOK;
error:
	if (clabel != NULL)
		free(clabel);
	if (ldimm8 != NULL)
		z80ic_instr_destroy(&ldimm8->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	if (ldvrivrr != NULL)
		z80ic_instr_destroy(&ldvrivrr->instr);
	if (ldivrrvr != NULL)
		z80ic_instr_destroy(&ldivrrvr->instr);
	if (inc != NULL)
		z80ic_instr_destroy(&inc->instr);
	if (dec != NULL)
		z80ic_instr_destroy(&dec->instr);
	if (jpcc != NULL)
		z80ic_instr_destroy(&jpcc->instr);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16);
	z80ic_oper_vrr_destroy(vrrd);
	z80ic_oper_vrr_destroy(vrrs);
	return rc;
}

/** Select Z80 IC instructions code for IR recmbr instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR recmbr instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_recmbr(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	ir_oper_var_t *opvar;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	z80ic_add_a_n_t *addaimm8 = NULL;
	z80ic_adc_a_n_t *adcaimm8 = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_imm8_t *imm8 = NULL;
	unsigned srcvr;
	unsigned destvr;
	size_t elemsz;
	uint16_t off;
	int rc;

	(void)label;

	assert(irinstr->itype == iri_recmbr);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);
	assert(irinstr->opt != NULL);

	opvar = (ir_oper_var_t *) irinstr->op2->ext;

	off = 0x0;
	rc = z80_isel_recmbr_off(isproc->isel, irinstr->opt, opvar->varname,
	    &off);
	if (rc != EOK) {
		fprintf(stderr, "Error determning offset of member '%s' in "
		    "record '", opvar->varname);
		(void) ir_texpr_print(irinstr->opt, stderr);
		fprintf(stderr, "'.\n");
		return rc;
	}

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	srcvr = z80_isel_get_vregno(isproc, irinstr->op1);

	rc = z80_isel_texpr_sizeof(isproc->isel, irinstr->opt, &elemsz);
	if (rc != EOK)
		return rc;

	/* ld A, srcvr.L */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(srcvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* add A, LO(off) */

	rc = z80ic_add_a_n_create(&addaimm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(off & 0xff, &imm8);
	if (rc != EOK)
		goto error;

	addaimm8->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &addaimm8->instr);
	if (rc != EOK)
		goto error;

	addaimm8 = NULL;

	/* ld destvr.L, A */

	rc = z80ic_ld_vr_r_create(&ldvrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16l, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	ldvrr->dest = vr;
	ldvrr->src = reg;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
	if (rc != EOK)
		goto error;

	ldvrr = NULL;

	/* ld A, srcvr.H */

	rc = z80ic_ld_r_vr_create(&ldrvr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(srcvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	ldrvr->dest = reg;
	ldrvr->src = vr;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldrvr->instr);
	if (rc != EOK)
		goto error;

	ldrvr = NULL;

	/* adc A, LO(off) */

	rc = z80ic_adc_a_n_create(&adcaimm8);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm8_create(off >> 8, &imm8);
	if (rc != EOK)
		goto error;

	adcaimm8->imm8 = imm8;
	imm8 = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &adcaimm8->instr);
	if (rc != EOK)
		goto error;

	adcaimm8 = NULL;

	/* ld destvr.H, A */

	rc = z80ic_ld_vr_r_create(&ldvrr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(destvr, z80ic_vrp_r16h, &vr);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		goto error;

	ldvrr->dest = vr;
	ldvrr->src = reg;
	reg = NULL;
	vr = NULL;

	rc = z80ic_lblock_append(lblock, NULL, &ldvrr->instr);
	if (rc != EOK)
		goto error;

	ldvrr = NULL;

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);
	if (addaimm8 != NULL)
		z80ic_instr_destroy(&addaimm8->instr);
	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_imm8_destroy(imm8);
	return rc;
}

/** Select Z80 IC instructions code for IR ptridx instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_ptridx(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	unsigned vr1, vr2;
	unsigned offvr;
	size_t elemsz;
	int rc;

	assert(irinstr->itype == iri_ptridx);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);
	assert(irinstr->opt != NULL);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);
	rc = z80_isel_texpr_sizeof(isproc->isel, irinstr->opt, &elemsz);
	if (rc != EOK)
		return rc;

	offvr = z80_isel_get_new_vregnos(isproc, 2);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		return rc;

	/* offvr = index * elemsz */
	rc = z80_isel_vrr_cmul(isproc, offvr, elemsz, vr2, 2, lblock);
	if (rc != EOK)
		return rc;

	/* dest = base + offvr */
	return z80_isel_vrr_add(isproc, destvr, vr1, offvr, 2, lblock);
}

/** Select Z80 IC instructions code for IR return instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_ret(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ret_t *ret = NULL;
	int rc;

	(void) isproc;

	assert(irinstr->itype == iri_ret);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1 == NULL);
	assert(irinstr->op2 == NULL);

	/* ret */

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, label, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;
	return EOK;
error:
	if (ret != NULL)
		z80ic_instr_destroy(&ret->instr);

	return rc;
}

/** Select Z80 IC instructions code for IR 8-bit return value instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_retv_8(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_reg_t *dest = NULL;
	z80ic_oper_vr_t *src = NULL;
	z80ic_ld_r_vr_t *ld = NULL;
	z80ic_ret_t *ret = NULL;
	unsigned vr;
	int rc;

	assert(irinstr->itype == iri_retv);
	assert(irinstr->width == 8);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	vr = z80_isel_get_vregno(isproc, irinstr->op1);

	/* ld A, vr */

	rc = z80ic_ld_r_vr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_reg_create(z80ic_reg_a, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vr_create(vr, z80ic_vrp_r8, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* ret */

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;
	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ret != NULL)
		z80ic_instr_destroy(&ret->instr);

	z80ic_oper_reg_destroy(dest);
	z80ic_oper_vr_destroy(src);

	return rc;
}

/** Select Z80 IC instructions code for IR 16-bit return value instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_retv_16(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_r16_t *dest = NULL;
	z80ic_oper_vrr_t *src = NULL;
	z80ic_ld_r16_vrr_t *ld = NULL;
	z80ic_ret_t *ret = NULL;
	unsigned vr;
	int rc;

	assert(irinstr->itype == iri_retv);
	assert(irinstr->width == 16);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	vr = z80_isel_get_vregno(isproc, irinstr->op1);

	/* ld r16, vr */

	rc = z80ic_ld_r16_vrr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(isproc->usr ? z80ic_r16_bc :
	    z80ic_r16_hl, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* ret */

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;
	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ret != NULL)
		z80ic_instr_destroy(&ret->instr);

	z80ic_oper_r16_destroy(dest);
	z80ic_oper_vrr_destroy(src);

	return rc;
}

/** Select Z80 IC instructions code for IR 32-bit return value instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_retv_32(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_r16_t *dest = NULL;
	z80ic_oper_vrr_t *src = NULL;
	z80ic_ld_r16_vrr_t *ld = NULL;
	z80ic_ret_t *ret = NULL;
	unsigned vr;
	int rc;

	assert(irinstr->itype == iri_retv);
	assert(irinstr->width == 32);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	vr = z80_isel_get_vregno(isproc, irinstr->op1);

	/* ld DE, vr+1 */

	rc = z80ic_ld_r16_vrr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_de, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr + 1, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* ld HL, vr */

	rc = z80ic_ld_r16_vrr_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_r16_create(z80ic_r16_hl, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(vr, &src);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->src = src;
	dest = NULL;
	src = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;

	/* ret */

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;
	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ret != NULL)
		z80ic_instr_destroy(&ret->instr);

	z80ic_oper_r16_destroy(dest);
	z80ic_oper_vrr_destroy(src);

	return rc;
}

/** Select Z80 IC instructions code for IR 64-bit return value instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_retv_64(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_ret_t *ret = NULL;
	z80_varmap_entry_t *entry;
	unsigned vr;
	unsigned addr_vr;
	int rc;

	(void)label;

	assert(irinstr->itype == iri_retv);
	assert(irinstr->width == 64);
	assert(irinstr->dest == NULL);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	rc = z80_varmap_find(isproc->varmap, "%.retval", &entry);
	assert(rc == EOK);

	/* addr_vr is the virtual register holding %.retval */
	addr_vr = entry->vr0;

	/* vr is the first virtual register hodling the return value */
	vr = z80_isel_get_vregno(isproc, irinstr->op1);

	rc = z80_isel_write_vrr(isproc, addr_vr, vr, irinstr->width / 8,
	    lblock);
	if (rc != EOK)
		goto error;

	/* ret */

	rc = z80ic_ret_create(&ret);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_append(lblock, NULL, &ret->instr);
	if (rc != EOK)
		goto error;

	ret = NULL;
	return EOK;
error:
	if (ret != NULL)
		z80ic_instr_destroy(&ret->instr);

	return rc;
}

/** Select Z80 IC instructions code for IR return value instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR add instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_retv(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	switch (irinstr->width) {
	case 8:
		return z80_isel_retv_8(isproc, label, irinstr, lblock);
	case 16:
		return z80_isel_retv_16(isproc, label, irinstr, lblock);
	case 32:
		return z80_isel_retv_32(isproc, label, irinstr, lblock);
	case 64:
		return z80_isel_retv_64(isproc, label, irinstr, lblock);
	default:
		assert(false);
	}
}

/** Select Z80 IC instructions code for IR variable pointer instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR varptr instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_varptr(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vrr_t *dest = NULL;
	z80ic_oper_imm16_t *imm = NULL;
	z80ic_ld_vrr_nn_t *ld = NULL;
	unsigned destvr;
	ir_oper_var_t *op1;
	char *varident = NULL;
	int rc;

	assert(irinstr->itype == iri_varptr);
	assert(irinstr->width == 16);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2 == NULL);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	op1 = (ir_oper_var_t *) irinstr->op1->ext;

	rc = z80_isel_mangle_global_ident(op1->varname, &varident);
	if (rc != EOK)
		goto error;

	/* ld dest, NN */

	rc = z80ic_ld_vrr_nn_create(&ld);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_vrr_create(destvr, &dest);
	if (rc != EOK)
		goto error;

	rc = z80ic_oper_imm16_create_symbol(varident, &imm);
	if (rc != EOK)
		goto error;

	ld->dest = dest;
	ld->imm16 = imm;
	dest = NULL;
	imm = NULL;

	rc = z80ic_lblock_append(lblock, label, &ld->instr);
	if (rc != EOK)
		goto error;

	ld = NULL;
	free(varident);
	return EOK;
error:
	if (varident != NULL)
		free(varident);
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);

	z80ic_oper_vrr_destroy(dest);
	z80ic_oper_imm16_destroy(imm);

	return rc;
}

/** Select Z80 IC instructions code for IR write instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR write instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_write(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned srcvr;
	unsigned vr;

	vr = z80_isel_get_vregno(isproc, irinstr->op1);
	srcvr = z80_isel_get_vregno(isproc, irinstr->op2);

	(void)label;

	return z80_isel_write_vrr(isproc, vr, srcvr, irinstr->width / 8,
	    lblock);
}

/** Select Z80 IC instructions code for IR xor instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR xor instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_xor(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_ld_r_vr_t *ldrvr = NULL;
	z80ic_xor_vr_t *xor = NULL;
	z80ic_ld_vr_r_t *ldvrr = NULL;
	unsigned byte;
	unsigned vroff;
	z80ic_vr_part_t part;
	unsigned destvr;
	unsigned vr1, vr2;
	int rc;

	assert(irinstr->itype == iri_xor);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_var);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	vr2 = z80_isel_get_vregno(isproc, irinstr->op2);

	/* Do the same thing for every byte */
	for (byte = 0; byte < irinstr->width / 8; byte++) {
		/* Determine register part and offset */
		z80_isel_reg_part_off(byte, irinstr->width / 8, &part, &vroff);

		/* ld A, vr1.X */

		rc = z80ic_ld_r_vr_create(&ldrvr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr1 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		ldrvr->dest = reg;
		ldrvr->src = vr;
		reg = NULL;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldrvr->instr);
		if (rc != EOK)
			goto error;

		ldrvr = NULL;

		/* xor vr2.X */

		rc = z80ic_xor_vr_create(&xor);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(vr2 + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		xor->src = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &xor->instr);
		if (rc != EOK)
			goto error;

		xor = NULL;

		/* ld dest.X, A */

		rc = z80ic_ld_vr_r_create(&ldvrr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(destvr + vroff, part, &vr);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
		if (rc != EOK)
			goto error;

		ldvrr->dest = vr;
		ldvrr->src = reg;
		vr = NULL;
		reg = NULL;

		rc = z80ic_lblock_append(lblock, label, &ldvrr->instr);
		if (rc != EOK)
			goto error;

		ldvrr = NULL;
	}

	return EOK;
error:
	if (ldrvr != NULL)
		z80ic_instr_destroy(&ldrvr->instr);
	if (xor != NULL)
		z80ic_instr_destroy(&xor->instr);
	if (ldvrr != NULL)
		z80ic_instr_destroy(&ldvrr->instr);

	z80ic_oper_vr_destroy(vr);
	z80ic_oper_reg_destroy(reg);

	return rc;
}

/** Select Z80 IC instructions code for IR zrext instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR trunc instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_zrext(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	unsigned destvr;
	unsigned vr1;
	unsigned srcw;
	ir_oper_imm_t *op2i;
	int rc;

	assert(irinstr->itype == iri_zrext);
	assert(irinstr->width > 0);
	assert(irinstr->width % 8 == 0);
	assert(irinstr->op1->optype == iro_var);
	assert(irinstr->op2->optype == iro_imm);

	destvr = z80_isel_get_vregno(isproc, irinstr->dest);
	vr1 = z80_isel_get_vregno(isproc, irinstr->op1);
	op2i = (ir_oper_imm_t *)irinstr->op2->ext;
	srcw = op2i->value;

	assert(srcw % 8 == 0);
	assert(irinstr->width > srcw);

	rc = z80ic_lblock_append(lblock, label, NULL);
	if (rc != EOK)
		return rc;

	return z80_isel_vrr_extend(isproc, destvr, irinstr->width / 8,
	    vr1, srcw / 8, false, lblock);
}

/** Select Z80 IC instructions for IR instruction.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_instr(z80_isel_proc_t *isproc, const char *label,
    ir_instr_t *irinstr, z80ic_lblock_t *lblock)
{
	switch (irinstr->itype) {
	case iri_add:
		return z80_isel_add(isproc, label, irinstr, lblock);
	case iri_and:
		return z80_isel_and(isproc, label, irinstr, lblock);
	case iri_bnot:
		return z80_isel_bnot(isproc, label, irinstr, lblock);
	case iri_call:
		return z80_isel_call(isproc, label, irinstr, lblock);
	case iri_eq:
		return z80_isel_eq(isproc, label, irinstr, lblock);
	case iri_gt:
		return z80_isel_gt(isproc, label, irinstr, lblock);
	case iri_gtu:
		return z80_isel_gtu(isproc, label, irinstr, lblock);
	case iri_gteq:
		return z80_isel_gteq(isproc, label, irinstr, lblock);
	case iri_gteu:
		return z80_isel_gteu(isproc, label, irinstr, lblock);
	case iri_imm:
		return z80_isel_imm(isproc, label, irinstr, lblock);
	case iri_jmp:
		return z80_isel_jmp(isproc, label, irinstr, lblock);
	case iri_jnz:
		return z80_isel_jnz(isproc, label, irinstr, lblock);
	case iri_jz:
		return z80_isel_jz(isproc, label, irinstr, lblock);
	case iri_lt:
		return z80_isel_lt(isproc, label, irinstr, lblock);
	case iri_ltu:
		return z80_isel_ltu(isproc, label, irinstr, lblock);
	case iri_lteq:
		return z80_isel_lteq(isproc, label, irinstr, lblock);
	case iri_lteu:
		return z80_isel_lteu(isproc, label, irinstr, lblock);
	case iri_lvarptr:
		return z80_isel_lvarptr(isproc, label, irinstr, lblock);
	case iri_mul:
		return z80_isel_mul(isproc, label, irinstr, lblock);
	case iri_neg:
		return z80_isel_neg(isproc, label, irinstr, lblock);
	case iri_neq:
		return z80_isel_neq(isproc, label, irinstr, lblock);
	case iri_nop:
		return z80_isel_nop(isproc, label, irinstr, lblock);
	case iri_or:
		return z80_isel_or(isproc, label, irinstr, lblock);
	case iri_ptridx:
		return z80_isel_ptridx(isproc, label, irinstr, lblock);
	case iri_read:
		return z80_isel_read(isproc, label, irinstr, lblock);
	case iri_reccopy:
		return z80_isel_reccopy(isproc, label, irinstr, lblock);
	case iri_recmbr:
		return z80_isel_recmbr(isproc, label, irinstr, lblock);
	case iri_ret:
		return z80_isel_ret(isproc, label, irinstr, lblock);
	case iri_retv:
		return z80_isel_retv(isproc, label, irinstr, lblock);
	case iri_sgnext:
		return z80_isel_sgnext(isproc, label, irinstr, lblock);
	case iri_shl:
		return z80_isel_shl(isproc, label, irinstr, lblock);
	case iri_shra:
	case iri_shrl:
		return z80_isel_shr(isproc, label, irinstr, lblock);
	case iri_sub:
		return z80_isel_sub(isproc, label, irinstr, lblock);
	case iri_trunc:
		return z80_isel_trunc(isproc, label, irinstr, lblock);
	case iri_varptr:
		return z80_isel_varptr(isproc, label, irinstr, lblock);
	case iri_write:
		return z80_isel_write(isproc, label, irinstr, lblock);
	case iri_xor:
		return z80_isel_xor(isproc, label, irinstr, lblock);
	case iri_zrext:
		return z80_isel_zrext(isproc, label, irinstr, lblock);
	}

	assert(false);
	return EINVAL;
}

/** Select Z80 IC for IR label.
 *
 * @param isproc Instruction selector for procedure
 * @param irinstr IR instruction
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_label(z80_isel_proc_t *isproc, const char *label,
    z80ic_lblock_t *lblock)
{
	int rc;
	int rv;
	char *iclabel = NULL;

	(void) isproc;

	assert(isproc->ident[0] == '@');
	assert(label[0] == '%');

	rv = asprintf(&iclabel, "l_%s_%s", &isproc->ident[1], &label[1]);
	if (rv < 0) {
		rc = ENOMEM;
		goto error;
	}

	rc = z80ic_lblock_append(lblock, iclabel, NULL);
	if (rc != EOK)
		goto error;

	free(iclabel);
	return EOK;
error:
	if (iclabel != NULL)
		free(iclabel);
	return rc;
}

/** Select Z80 IC instructions for IR integer data entry.
 *
 * @param isel Instruction selector
 * @param irdentry IR integer data entry
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_isel_int(z80_isel_t *isel, ir_dentry_t *irdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	int rc;

	(void) isel;
	assert(irdentry->dtype == ird_int);

	switch (irdentry->width) {
	case 8:
		rc = z80ic_dentry_create_defb(irdentry->value, &dentry);
		if (rc != EOK)
			goto error;
		break;
	case 16:
		rc = z80ic_dentry_create_defw(irdentry->value, &dentry);
		if (rc != EOK)
			goto error;
		break;
	case 32:
		rc = z80ic_dentry_create_defdw(irdentry->value, &dentry);
		if (rc != EOK)
			goto error;
		break;
	case 64:
		rc = z80ic_dentry_create_defqw(irdentry->value, &dentry);
		if (rc != EOK)
			goto error;
		break;
	default:
		assert(false);
	}

	rc = z80ic_dblock_append(dblock, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	z80ic_dentry_destroy(dentry);
	return rc;
}

/** Select Z80 IC instructions for IR pointer data entry.
 *
 * @param isel Instruction selector
 * @param irdentry IR pointer data entry
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_isel_ptr(z80_isel_t *isel, ir_dentry_t *irdentry,
    z80ic_dblock_t *dblock)
{
	z80ic_dentry_t *dentry = NULL;
	char *ident = NULL;
	int rc;

	(void) isel;
	assert(irdentry->dtype == ird_ptr);

	switch (irdentry->width) {
	case 16:
		rc = z80_isel_mangle_global_ident(irdentry->symbol,
		    &ident);
		if (rc != EOK)
			goto error;

		rc = z80ic_dentry_create_defw_sym(ident, irdentry->value,
		    &dentry);
		if (rc != EOK)
			goto error;

		free(ident);
		ident = NULL;
		break;
	default:
		assert(false);
	}

	rc = z80ic_dblock_append(dblock, dentry);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (ident != NULL)
		free(ident);
	z80ic_dentry_destroy(dentry);
	return rc;
}

/** Select Z80 IC instructions for IR data entry.
 *
 * @param isel Instruction selector
 * @param irdentry IR data entry
 * @param dblock Data block where to append the new data entry
 * @return EOK on success or an error code
 */
static int z80_isel_dentry(z80_isel_t *isel, ir_dentry_t *irdentry,
    z80ic_dblock_t *dblock)
{
	switch (irdentry->dtype) {
	case ird_int:
		return z80_isel_int(isel, irdentry, dblock);
	case ird_ptr:
		return z80_isel_ptr(isel, irdentry, dblock);
	}

	assert(false);
	return EINVAL;
}

/** Select instructions code for variable.
 *
 * @param isel Instruction selector
 * @param irvar IR variable
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_var(z80_isel_t *isel, ir_var_t *irvar,
    z80ic_module_t *icmod)
{
	ir_dblock_entry_t *entry;
	z80ic_var_t *icvar = NULL;
	z80ic_dblock_t *dblock = NULL;
	char *ident = NULL;
	int rc;

	rc = z80ic_dblock_create(&dblock);
	if (rc != EOK)
		goto error;

	rc = z80_isel_mangle_global_ident(irvar->ident, &ident);
	if (rc != EOK)
		goto error;

	rc = z80ic_var_create(ident, dblock, &icvar);
	if (rc != EOK)
		goto error;

	entry = ir_dblock_first(irvar->dblock);
	while (entry != NULL) {
		rc = z80_isel_dentry(isel, entry->dentry, dblock);
		if (rc != EOK)
			goto error;

		entry = ir_dblock_next(entry);
	}

	free(ident);
	z80ic_module_append(icmod, &icvar->decln);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	z80ic_var_destroy(icvar);
	z80ic_dblock_destroy(dblock);
	return rc;
}

/** Select instructions to load procedure argument to virtual register(s).
 *
 * @param isel Instruction selector
 * @param ident Argument identifier
 * @param bits Number of bits in argument
 * @param vrno Pointer to variable holding next free virtual register
 * @param fpoff Pointer to vairable holding next free address on the stack
 * @param argloc Pointer to Argument location allocator
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_proc_arg(z80_isel_t *isel, const char *ident, unsigned bits, unsigned *vrno,
    unsigned *fpoff, z80_argloc_t *argloc, z80ic_lblock_t *lblock)
{
	z80ic_oper_r16_t *ldsrc = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vrr_t *vrr = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_ld_vrr_r16_t *ld = NULL;
	z80ic_ld_vr_r_t *ld8 = NULL;
	z80ic_ld_vrr_iixd_t *ldix = NULL;
	z80ic_ld_vr_iixd_t *ldix8 = NULL;
	z80_argloc_entry_t *entry;
	z80ic_reg_t r;
	unsigned i;
	z80ic_r16_t argreg;
	int rc;

	(void) isel;

	/* Allocate location for the argument */
	rc = z80_argloc_alloc(argloc, ident, (bits + 7) / 8,
	    &entry);
	if (rc != EOK)
		goto error;

	/* Parts stored in registers */
	for (i = 0; i < entry->reg_entries; i++) {
		argreg = entry->reg[i].reg;

		if (entry->reg[i].part == z80_argloc_hl) {
			/* 16-bit register */

			rc = z80ic_ld_vrr_r16_create(&ld);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vrr_create(*vrno, &vrr);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_r16_create(argreg, &ldsrc);
			if (rc != EOK)
				goto error;

			ld->dest = vrr;
			ld->src = ldsrc;
			vrr = NULL;
			ldsrc = NULL;

			rc = z80ic_lblock_append(lblock, NULL, &ld->instr);
			if (rc != EOK)
				goto error;

			ld = NULL;
			++*vrno;
		} else {
			/* 8-bit register */

			z80_argloc_r16_part_to_r(argreg,
			    entry->reg[i].part, &r);

			rc = z80ic_ld_vr_r_create(&ld8);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_vr_create(*vrno, z80ic_vrp_r8,
			    &vr);
			if (rc != EOK)
				goto error;

			rc = z80ic_oper_reg_create(r, &reg);
			if (rc != EOK)
				goto error;

			ld8->dest = vr;
			ld8->src = reg;
			vr = NULL;
			reg = NULL;

			rc = z80ic_lblock_append(lblock, NULL,
			    &ld8->instr);
			if (rc != EOK)
				goto error;

			ld = NULL;
			++*vrno;
		}
	}

	/* Words stored on the stack */
	for (i = 0; i + 1 < entry->stack_sz; i += 2) {
		/* ld vrr, (IX+d) */

		rc = z80ic_ld_vrr_iixd_create(&ldix);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vrr_create(*vrno, &vrr);
		if (rc != EOK)
			goto error;

		ldix->disp = *fpoff;
		ldix->dest = vrr;
		vrr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldix->instr);
		if (rc != EOK)
			goto error;

		ldix = NULL;
		++*vrno;
		*fpoff += 2;
	}

	/*
	 * Byte stored on the stack. A byte is padded to a full
	 * 16-bit stack entry (to make things simpler and faster).
	 * The value of the padding is undefined. Here we just
	 * load the lower byte to an 8-bit virtual register.
	 */
	for (; i < entry->stack_sz; i++) {
		/* ld vr, (IX+d) */

		rc = z80ic_ld_vr_iixd_create(&ldix8);
		if (rc != EOK)
			goto error;

		rc = z80ic_oper_vr_create(*vrno, z80ic_vrp_r8, &vr);
		if (rc != EOK)
			goto error;

		ldix8->disp = *fpoff;
		ldix8->dest = vr;
		vr = NULL;

		rc = z80ic_lblock_append(lblock, NULL, &ldix8->instr);
		if (rc != EOK)
			goto error;

		ldix8 = NULL;
		++*vrno;
		*fpoff += 2;
	}

	return EOK;
error:
	if (ld != NULL)
		z80ic_instr_destroy(&ld->instr);
	if (ldix != NULL)
		z80ic_instr_destroy(&ldix->instr);
	z80ic_oper_vrr_destroy(vrr);
	z80ic_oper_r16_destroy(ldsrc);
	return rc;
}

/** Select instructions to load procedure arguments to virtual registers.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param lblock Labeled block where to append the new instruction
 * @return EOK on success or an error code
 */
static int z80_isel_proc_args(z80_isel_t *isel, ir_proc_t *irproc,
    z80ic_lblock_t *lblock)
{
	z80_argloc_t *argloc = NULL;
	ir_proc_arg_t *arg;
	unsigned vrno;
	unsigned fpoff;
	unsigned bits;
	int rc;

	(void) isel;
	rc = z80_argloc_create(&argloc);
	if (rc != EOK)
		goto error;

	arg = ir_proc_first_arg(irproc);
	vrno = 0;

	/*
	 * IX points to old frame pointer, IX+2 to the return address,
	 * IX+4 to the first argument on the stack
	 */
	fpoff = 4;

	if (irproc->rtype != NULL) {
		assert(irproc->rtype->tetype == irt_int ||
		    irproc->rtype->tetype == irt_ptr);

		/* Add hidden first argument for returning 64-bit value */
		if (irproc->rtype->tetype == irt_int &&
		    irproc->rtype->t.tint.width == 64) {
			rc = z80_isel_proc_arg(isel, "%.retval", 16, &vrno,
			    &fpoff, argloc, lblock);
			if (rc != EOK)
				goto error;
		}
	}

	while (arg != NULL) {
		if (arg->atype->tetype == irt_int) {
			bits = arg->atype->t.tint.width;
		} else if (arg->atype->tetype == irt_ptr) {
			bits = arg->atype->t.tptr.width;
		} else {
			fprintf(stderr, "Unsupported argument type (%d)\n",
			    arg->atype->tetype);
			goto error;
		}

		rc = z80_isel_proc_arg(isel, arg->ident, bits, &vrno, &fpoff, argloc, lblock);
		if (rc != EOK)
			goto error;

		arg = ir_proc_next_arg(arg);
	}

	z80_argloc_destroy(argloc);
	return EOK;
error:
	if (argloc != NULL)
		z80_argloc_destroy(argloc);
	return rc;
}

/** Generate Z80 IC local variables from IR procedure local variables.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param icproc Z80 IC procedure where to append the new local variables
 * @return EOK on success or an error code
 */
static int z80_isel_proc_lvars(z80_isel_t *isel, ir_proc_t *irproc,
    z80ic_proc_t *icproc)
{
	ir_lvar_t *lvar;
	z80ic_lvar_t *icvar;
	char *icident = NULL;
	size_t size;
	int rc;

	(void) isel;

	lvar = ir_proc_first_lvar(irproc);
	while (lvar != NULL) {
		rc = z80_isel_mangle_lvar_ident(irproc->ident, lvar->ident,
		    &icident);
		if (rc != EOK)
			goto error;

		rc = z80_isel_texpr_sizeof(isel, lvar->vtype, &size);
		if (rc != EOK)
			goto error;

		rc = z80ic_lvar_create(icident, icproc->lvar_sz, &icvar);
		if (rc != EOK)
			goto error;

		free(icident);
		icident = NULL;

		z80ic_proc_append_lvar(icproc, icvar);

		icproc->lvar_sz += size;
		lvar = ir_proc_next_lvar(lvar);
	}

	return EOK;
error:
	if (icident != NULL)
		free(icident);
	return rc;
}

/** Add termnating entry to Z80 IC local variables.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param icproc Z80 IC procedure where to append the new local variables
 * @return EOK on success or an error code
 */
static int z80_isel_proc_lvars_finish(z80_isel_t *isel, ir_proc_t *irproc,
    z80ic_proc_t *icproc)
{
	z80ic_lvar_t *icvar;
	char *icident = NULL;
	int rc;

	(void) isel;

	/* If there are any local variables */
	if (icproc->lvar_sz > 0) {
		/* Add a special %@end entry to denote total size of variables */
		rc = z80_isel_mangle_lvar_ident(irproc->ident, "%@end",
		    &icident);
		if (rc != EOK)
			goto error;

		rc = z80ic_lvar_create(icident, icproc->lvar_sz, &icvar);
		if (rc != EOK)
			goto error;

		z80ic_proc_append_lvar(icproc, icvar);

		free(icident);
		icident = NULL;
	}

	return EOK;
error:
	if (icident != NULL)
		free(icident);
	return rc;
}

/** Select instructions code for procedure definition.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_proc_def(z80_isel_t *isel, ir_proc_t *irproc,
    z80ic_module_t *icmod)
{
	z80_isel_proc_t *isproc = NULL;
	ir_lblock_entry_t *entry;
	z80ic_proc_t *icproc = NULL;
	z80ic_lblock_t *lblock = NULL;
	char *ident = NULL;
	int rc;

	rc = z80_isel_proc_create(isel, irproc, &isproc);
	if (rc != EOK)
		goto error;

	if (ir_proc_has_attr(irproc, "@usr"))
		isproc->usr = true;

	/* Build variable - VR map */
	rc = z80_isel_proc_create_varmap(isproc, irproc);
	if (rc != EOK)
		goto error;

	rc = z80ic_lblock_create(&lblock);
	if (rc != EOK)
		goto error;

	rc = z80_isel_mangle_global_ident(irproc->ident, &ident);
	if (rc != EOK)
		goto error;

	rc = z80ic_proc_create(ident, lblock, &icproc);
	if (rc != EOK)
		goto error;

	lblock = NULL;
	isproc->icproc = icproc;

	rc = z80_isel_proc_args(isel, irproc, icproc->lblock);
	if (rc != EOK)
		goto error;

	rc = z80_isel_proc_lvars(isel, irproc, icproc);
	if (rc != EOK)
		goto error;

	entry = ir_lblock_first(irproc->lblock);
	while (entry != NULL) {
		if (entry->instr != NULL) {
			/* Instruction */
			assert(entry->label == NULL);
			rc = z80_isel_instr(isproc, NULL, entry->instr,
			    icproc->lblock);
			if (rc != EOK)
				goto error;
		} else {
			/* Label */
			rc = z80_isel_label(isproc, entry->label,
			    icproc->lblock);
			if (rc != EOK)
				goto error;
		}

		entry = ir_lblock_next(entry);
	}

	icproc->used_vrs = isproc->varmap->next_vr;

	/* Add the terminating entry to local variables */
	rc = z80_isel_proc_lvars_finish(isel, irproc, icproc);
	if (rc != EOK)
		goto error;

	free(ident);
	z80_isel_proc_destroy(isproc);
	z80ic_module_append(icmod, &icproc->decln);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	z80ic_proc_destroy(icproc);
	z80ic_lblock_destroy(lblock);
	z80_isel_proc_destroy(isproc);
	return rc;
}

/** Select instructions code for external procedure declaration.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_proc_extern(z80_isel_t *isel, ir_proc_t *irproc,
    z80ic_module_t *icmod)
{
	z80ic_extern_t *icextern = NULL;
	char *ident = NULL;
	int rc;

	(void) isel;

	rc = z80_isel_mangle_global_ident(irproc->ident, &ident);
	if (rc != EOK)
		goto error;

	rc = z80ic_extern_create(ident, &icextern);
	if (rc != EOK)
		goto error;

	free(ident);
	z80ic_module_append(icmod, &icextern->decln);
	return EOK;
error:
	if (ident != NULL)
		free(ident);
	return rc;
}

/** Select instructions code for procedure.
 *
 * @param isel Instruction selector
 * @param irproc IR procedure
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_proc(z80_isel_t *isel, ir_proc_t *irproc,
    z80ic_module_t *icmod)
{
	int rc;

	if ((irproc->flags & irp_extern) != 0)
		rc = z80_isel_proc_extern(isel, irproc, icmod);
	else
		rc = z80_isel_proc_def(isel, irproc, icmod);

	return rc;
}

/** Select instructions code for record type.
 *
 * @param isel Instruction selector
 * @param irrec IR record
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_record(z80_isel_t *isel, ir_record_t *irrec,
    z80ic_module_t *icmod)
{
	(void)isel;
	(void)irrec;
	(void)icmod;
	return EOK;
}

/** Select instructions code for declaration.
 *
 * @param isel Instruction selector
 * @param decln IR declaration
 * @param icmod Z80 IC module to which the code should be appended
 * @return EOK on success or an error code
 */
static int z80_isel_decln(z80_isel_t *isel, ir_decln_t *decln,
    z80ic_module_t *icmod)
{
	int rc;

	switch (decln->dtype) {
	case ird_var:
		rc = z80_isel_var(isel, (ir_var_t *) decln->ext, icmod);
		break;
	case ird_proc:
		rc = z80_isel_proc(isel, (ir_proc_t *) decln->ext, icmod);
		break;
	case ird_record:
		rc = z80_isel_record(isel, (ir_record_t *) decln->ext, icmod);
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Select instructions for module.
 *
 * @param isel Instruction selector
 * @param irmod IR module
 * @param ricmod Place to store pointer to new Z80 IC module
 * @return EOK on success or an error code
 */
int z80_isel_module(z80_isel_t *isel, ir_module_t *irmod,
    z80ic_module_t **ricmod)
{
	z80ic_module_t *icmod;
	int rc;
	ir_decln_t *decln;

	rc = z80ic_module_create(&icmod);
	if (rc != EOK)
		return rc;

	isel->irmodule = irmod;

	decln = ir_module_first(irmod);
	while (decln != NULL) {
		rc = z80_isel_decln(isel, decln, icmod);
		if (rc != EOK)
			goto error;

		decln = ir_module_next(decln);
	}

	isel->irmodule = NULL;
	*ricmod = icmod;
	return EOK;
error:
	z80ic_module_destroy(icmod);
	return rc;
}

/** Destroy instruction selector.
 *
 * @param isel Instruction selector or @c NULL
 */
void z80_isel_destroy(z80_isel_t *isel)
{
	if (isel == NULL)
		return;

	free(isel);
}
