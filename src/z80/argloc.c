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
 * Z80 function argument locations
 *
 * Describes where each function argument is stored and allows allocation
 * of storage for arguments.
 *
 * Arguments can be passed in registers, on stack, or a combination of both.
 * The initial part of an argument (when viewed as stored in memory) can
 * be allocated to one or more 8- or 16-bit registers. Once registers
 * are no longer available, the remainder of the argument is stored on the
 * stack. Since the Z80 cannot process more than 16-bits worth of data
 * at a time, this is perfectly fine.
 *
 * For example, one might pass a 64-bit integer as follows:
 *   - bytes 0-1 (least significant) in HL
 *   - bytes 2-3 in DE
 *   - bytes 4-5 in BC
 *   - bytes 6-7 (most significant) on the stack
 *
 * Allocation: For each argument, in turn, we try to allocate a suitable
 * register. For 8-bit integers we try to allocate a single 8-bit register.
 * For larger integers we try to allocate a corresponding number of 16-bit
 * registers.
 *
 * If an 8-bit register is required, they are allocated, in order:
 *   - A, B, C, D, E, H, L
 * If a 16-bit register is required, they are allocated, in order:
 *   - HL, DE, BC
 *
 * Variadic functions (TODO): Fixed arguments can be treated just like with a
 * normal function (they are never accessed by va_arg().
 *
 * Variable arguments can be passed in registers, but they must be
 * allocated to the same registers regardless of the argument type.
 * This so that the called variadic function can copy the registers
 * holding the variadic arguments to its stack frame,
 * where they are picked up by the stdarg macros.
 *
 * An 8-bit integer variable argument is thus allocated to the first
 * available pair from HL, DE, BC (only the lower half is used).
 */

#include <assert.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <z80/argloc.h>

enum {
	z80_reg_alloc_num = 7,
	z80_r16_alloc_num = 3
};

static z80ic_reg_t z80_reg_alloc_order[z80_reg_alloc_num] = {
	z80ic_reg_a,
	z80ic_reg_b,
	z80ic_reg_c,
	z80ic_reg_d,
	z80ic_reg_e,
	z80ic_reg_h,
	z80ic_reg_l
};

static z80ic_r16_t z80_r16_alloc_order[z80_r16_alloc_num] = {
	z80ic_r16_hl,
	z80ic_r16_de,
	z80ic_r16_bc
};

/** Create argument locations.
 *
 * @param rargloc Place to store pointer to new argument locations
 * @return EOK on success, ENOMEM if out of memory
 */
int z80_argloc_create(z80_argloc_t **rargloc)
{
	z80_argloc_t *argloc;

	argloc = calloc(1, sizeof(z80_argloc_t));
	if (argloc == NULL)
		return ENOMEM;

	list_initialize(&argloc->entries);

	*rargloc = argloc;
	return EOK;
}

/** Destroy argument locations.
 *
 * @param isel Argument locations or @c NULL
 */
void z80_argloc_destroy(z80_argloc_t *argloc)
{
	z80_argloc_entry_t *entry;

	if (argloc == NULL)
		return;

	entry = z80_argloc_first(argloc);
	while (entry != NULL) {
		z80_argloc_entry_destroy(entry);
		entry = z80_argloc_first(argloc);
	}

	free(argloc);
}

static void z80_argloc_r_to_r16_part(z80ic_reg_t r, z80ic_r16_t *r16,
    z80_argloc_rp_t *part)
{
	switch (r) {
	case z80ic_reg_a:
		*r16 = z80ic_r16_af;
		*part = z80_argloc_h;
		break;
	case z80ic_reg_b:
		*r16 = z80ic_r16_bc;
		*part = z80_argloc_h;
		break;
	case z80ic_reg_c:
		*r16 = z80ic_r16_bc;
		*part = z80_argloc_l;
		break;
	case z80ic_reg_d:
		*r16 = z80ic_r16_de;
		*part = z80_argloc_h;
		break;
	case z80ic_reg_e:
		*r16 = z80ic_r16_de;
		*part = z80_argloc_l;
		break;
	case z80ic_reg_h:
		*r16 = z80ic_r16_hl;
		*part = z80_argloc_h;
		break;
	case z80ic_reg_l:
		*r16 = z80ic_r16_hl;
		*part = z80_argloc_l;
		break;
	}
}

/** Allocate 8-bit register for argument.
 *
 * @param argloc Argument locations
 * @parapm r16 Place to store allocated 16-bit register
 * @param rpart Place to store allocated register part
 *
 * @return EOK on success, ENOENT if there are no free 8-bit registers
 */
static int z80_argloc_reg_alloc(z80_argloc_t *argloc, z80ic_r16_t *rr16,
    z80_argloc_rp_t *rpart)
{
	z80ic_r16_t r16;
	z80_argloc_rp_t part;
	unsigned i;
	bool *used_mask;

	/* Allocate 8-bit register from A, B, C, D, E, H, L */
	for (i = 0; i < z80_reg_alloc_num; i++) {
		/* Which part of which register pair is this? */
		z80_argloc_r_to_r16_part(z80_reg_alloc_order[i], &r16, &part);

		used_mask = (part == z80_argloc_l) ?
		    argloc->r16l_used : argloc->r16h_used;

		if (used_mask[r16] == false) {
			/* Register is available */
			used_mask[r16] = true;
			*rr16 = r16;
			*rpart = part;
			return EOK;
		}
	}

	/* No available registers */
	return ENOENT;
}

/** Allocate 16-bit register for argument.
 *
 * @param argloc Argument locations
 * @parapm r16 Place to store allocated 16-bit register
 *
 * @return EOK on success, ENOENT if there are no free 8-bit registers
 */
static int z80_argloc_r16_alloc(z80_argloc_t *argloc, z80ic_r16_t *rr16)
{
	unsigned i;
	z80ic_r16_t r16;

	/* Allocate 16-bit register */

	for (i = 0; i < z80_r16_alloc_num; i++) {
		r16 = z80_r16_alloc_order[i];

		if (!argloc->r16l_used[r16] && !argloc->r16h_used[r16]) {
			argloc->r16l_used[r16] = true;
			argloc->r16h_used[r16] = true;
			*rr16 = r16;
			return EOK;
		}
	}

	return ENOENT;
}

/** Allocate argument location.
 *
 * @param argloc Argument locations
 * @param ident Argument identifier
 * @param bytes Size in bytes
 * @param rentry Place to store pointer to new entry
 * @return EOK on success or an error code
 */
int z80_argloc_alloc(z80_argloc_t *argloc, const char *ident, unsigned bytes,
    z80_argloc_entry_t **rentry)
{
	z80_argloc_entry_t *entry;
	z80ic_r16_t r16;
	z80_argloc_rp_t part;
	unsigned eidx;
	int rc;

	entry = calloc(1, sizeof(z80_argloc_entry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->ident = strdup(ident);
	if (entry->ident == NULL) {
		free(entry);
		return ENOMEM;
	}

	/* Allocate registers */

	if (bytes == 1) {
		/* Try allocating one register */
		rc = z80_argloc_reg_alloc(argloc, &r16, &part);
		if (rc == EOK) {
			/* Success */
			entry->reg[0].reg = r16;
			entry->reg[0].part = part;
			--bytes;
		}
	} else {
		eidx = 0;

		/* Allocate one or more 16-bit registers */
		while (bytes >= 2) {
			/* Try allocating a 16-bit register */
			rc = z80_argloc_r16_alloc(argloc, &r16);
			if (rc != EOK)
				break;

			/* Success */
			assert(eidx < z80_max_reg_entries);
			entry->reg[eidx].reg = r16;
			entry->reg[eidx].part = z80_argloc_hl;
			++eidx;
			bytes -= 2;
		}
	}

	/* Allocate remaining bytes on the stack */
	entry->stack_off = argloc->stack_used;
	entry->stack_sz = bytes;
	argloc->stack_used += bytes;

	list_append(&entry->lentries, &argloc->entries);
	entry->argloc = argloc;

	*rentry = entry;
	return EOK;
}

/** Find argument locations entry.
 *
 * @param argloc Argument locations
 * @param ident Variable identifier
 * @param rentry Place to store pointer to argument locations entry
 * @return EOK on success, ENOENT if not found
 */
int z80_argloc_find(z80_argloc_t *argloc, const char *ident,
    z80_argloc_entry_t **rentry)
{
	z80_argloc_entry_t *entry;

	entry = z80_argloc_first(argloc);
	while (entry != NULL) {
		if (strcmp(entry->ident, ident) == 0) {
			*rentry = entry;
			return EOK;
		}

		entry = z80_argloc_next(entry);
	}

	return ENOENT;
}

/** Destroy argument locations entry.
 *
 * @param entry Argument location
 */
void z80_argloc_entry_destroy(z80_argloc_entry_t *entry)
{
	free(entry->ident);
	list_remove(&entry->lentries);
	free(entry);
}

/** Get first argument locations entry.
 *
 * @param argloc Argument locations
 * @return First argument locations entry or @c NULL if the map is empty
 */
z80_argloc_entry_t *z80_argloc_first(z80_argloc_t *argloc)
{
	link_t *link;

	link = list_first(&argloc->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80_argloc_entry_t, lentries);
}

/** Get next argument locations entry.
 *
 * @param cur Current entry
 * @return Next entry or @c NULL if @a cur is the last entry
 */
z80_argloc_entry_t *z80_argloc_next(z80_argloc_entry_t *cur)
{
	link_t *link;

	link = list_next(&cur->lentries, &cur->argloc->entries);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, z80_argloc_entry_t, lentries);
}
