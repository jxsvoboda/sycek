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
 * Code generator C types
 *
 * The code generator's model of the C type system.
 *
 */

#include <assert.h>
#include <cgtype.h>
#include <charcls.h>
#include <merrno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/** Create basic type.
 *
 * @param elmtype Elementary type
 * @param rbasic Place to store pointer to new basic type
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_basic_create(cgtype_elmtype_t elmtype, cgtype_basic_t **rbasic)
{
	cgtype_basic_t *basic;

	basic = calloc(1, sizeof(cgtype_basic_t));
	if (basic == NULL)
		return ENOMEM;

	basic->cgtype.ntype = cgn_basic;
	basic->cgtype.ext = basic;
	basic->elmtype = elmtype;
	*rbasic = basic;
	return EOK;
}

/** Print basic type.
 *
 * @param basic Basic type
 * @param f Output stream
 *
 * @return EOK on success, EIO on I/O error
 */
static int cgtype_basic_print(cgtype_basic_t *basic, FILE *f)
{
	int rv = -1;

	switch (basic->elmtype) {
	case cgelm_void:
		rv = fputs("void", f);
		break;
	case cgelm_int:
		rv = fputs("int", f);
		break;
	case cgelm_logic:
		rv = fputs("logic", f);
		break;
	}

	if (rv < 0)
		return EIO;

	return EOK;
}

/** Clone basic type.
 *
 * @param orig Original basic type
 * @param rcopy Place to store pointer to copy
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgtype_basic_clone(cgtype_basic_t *orig, cgtype_t **rcopy)
{
	cgtype_basic_t *copy = NULL;
	int rc;

	rc = cgtype_basic_create(orig->elmtype, &copy);
	if (rc != EOK)
		return rc;

	*rcopy = &copy->cgtype;
	return EOK;
}

/** Destroy basic type.
 *
 * @param pointer Pointer type
 */
static void cgtype_basic_destroy(cgtype_basic_t *basic)
{
	free(basic);
}

/** Create pointer type.
 *
 * @param tgtype Pointer target type
 * @param rpointer Place to store pointer to new pointer type
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_pointer_create(cgtype_t *tgtype, cgtype_pointer_t **rpointer)
{
	cgtype_pointer_t *pointer;

	pointer = calloc(1, sizeof(cgtype_pointer_t));
	if (pointer == NULL)
		return ENOMEM;

	pointer->cgtype.ntype = cgn_pointer;
	pointer->cgtype.ext = pointer;
	pointer->tgtype = tgtype;
	*rpointer = pointer;
	return EOK;
}

/** Print pointer type.
 *
 * @param pointer Pointer type
 * @param f Output stream
 *
 * @return EOK on success, EIO on I/O error
 */
static int cgtype_pointer_print(cgtype_pointer_t *pointer, FILE *f)
{
	int rv;

	rv = fputc('^', f);
	if (rv < 0)
		return EIO;

	return cgtype_print(pointer->tgtype, f);
}

/** Clone pointer type.
 *
 * @param orig Original pointer type
 * @param rcopy Place to store pointer to copy
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgtype_pointer_clone(cgtype_pointer_t *orig, cgtype_t **rcopy)
{
	cgtype_pointer_t *copy = NULL;
	cgtype_t *tgcopy = NULL;
	int rc;

	rc = cgtype_clone(orig->tgtype, &tgcopy);
	if (rc != EOK)
		return rc;

	rc = cgtype_pointer_create(tgcopy, &copy);
	if (rc != EOK) {
		cgtype_destroy(tgcopy);
		return rc;
	}

	*rcopy = &copy->cgtype;
	return EOK;
}

/** Destroy pointer type.
 *
 * @param pointer Pointer type
 */
static void cgtype_pointer_destroy(cgtype_pointer_t *pointer)
{
	cgtype_destroy(pointer->tgtype);
	free(pointer);
}

/** Deep clone of code generator type.
 *
 * It's easier to deep clone types than to manage sharing nodes. Let's
 * keep it simple and stupid.
 *
 * @param orig Original type
 * @param rcopy Place to store pointer to new, copied type
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_clone(cgtype_t *orig, cgtype_t **rcopy)
{
	if (orig == NULL) {
		*rcopy = NULL;
		return EOK; // XXX
	}
	switch (orig->ntype) {
	case cgn_basic:
		return cgtype_basic_clone((cgtype_basic_t *) orig->ext, rcopy);
	case cgn_pointer:
		return cgtype_pointer_clone((cgtype_pointer_t *) orig->ext,
		    rcopy);
	}

	assert(false);
	return EINVAL;
}

/** Destroy code generator type.
 *
 * @param cgtype Code generator type or @c NULL
 */
void cgtype_destroy(cgtype_t *cgtype)
{
	if (cgtype == NULL)
		return;

	switch (cgtype->ntype) {
	case cgn_basic:
		cgtype_basic_destroy((cgtype_basic_t *) cgtype->ext);
		break;
	case cgn_pointer:
		cgtype_pointer_destroy((cgtype_pointer_t *) cgtype->ext);
		break;
	}
}

/** Print code generator type to stream.
 *
 * @param cgtype Code generator type
 * @param f Output stream
 *
 * @return EOK on success, EIO on I/O error
 */
int cgtype_print(cgtype_t *cgtype, FILE *f)
{
	switch (cgtype->ntype) {
	case cgn_basic:
		return cgtype_basic_print((cgtype_basic_t *) cgtype->ext, f);
	case cgn_pointer:
		return cgtype_pointer_print((cgtype_pointer_t *) cgtype->ext,
		    f);
	}

	assert(false);
	return EINVAL;
}
