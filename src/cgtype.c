/*
 * Copyright 2024 Jiri Svoboda
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
#include <cgrec.h>
#include <cgtype.h>
#include <charcls.h>
#include <inttypes.h>
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
	case cgelm_char:
		rv = fputs("char", f);
		break;
	case cgelm_uchar:
		rv = fputs("unsigned char", f);
		break;
	case cgelm_short:
		rv = fputs("short", f);
		break;
	case cgelm_ushort:
		rv = fputs("unsigned short", f);
		break;
	case cgelm_int:
		rv = fputs("int", f);
		break;
	case cgelm_uint:
		rv = fputs("unsigned int", f);
		break;
	case cgelm_long:
		rv = fputs("long", f);
		break;
	case cgelm_ulong:
		rv = fputs("unsigned long", f);
		break;
	case cgelm_longlong:
		rv = fputs("long long", f);
		break;
	case cgelm_ulonglong:
		rv = fputs("unsigned long long", f);
		break;
	case cgelm_logic:
		rv = fputs("logic", f);
		break;
	case cgelm_va_list:
		rv = fputs("__va_list", f);
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

/** Compose basic types.
 *
 * @param a First basic type
 * @param b Second basic type
 * @param rcomp Place to store pointer to composite type
 * @return EOK on success, EINVAL if the two types are not compatible,
 *         ENOMEM if out of memory
 */
static int cgtype_basic_compose(cgtype_basic_t *a, cgtype_basic_t *b,
    cgtype_t **rcomp)
{
	cgtype_basic_t *comp = NULL;
	int rc;

	if (a->elmtype != b->elmtype)
		return EINVAL;

	rc = cgtype_basic_create(a->elmtype, &comp);
	if (rc != EOK)
		return rc;

	*rcomp = &comp->cgtype;
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

/** Create function type.
 *
 * @param rtype Return type
 * @param rfunc Place to store pointer to new function type
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_func_create(cgtype_t *rtype, cgtype_func_t **rfunc)
{
	cgtype_func_t *func;

	func = calloc(1, sizeof(cgtype_func_t));
	if (func == NULL)
		return ENOMEM;

	func->cgtype.ntype = cgn_func;
	func->cgtype.ext = func;
	func->rtype = rtype;
	func->cconv = cgcc_default;
	list_initialize(&func->args);

	*rfunc = func;
	return EOK;
}

/** Print function type.
 *
 * @param func Function type
 * @param f Output stream
 *
 * @return EOK on success, EIO on I/O error
 */
static int cgtype_func_print(cgtype_func_t *func, FILE *f)
{
	cgtype_func_arg_t *arg;
	bool first;
	int rv;
	int rc;

	rc = cgtype_print(func->rtype, f);
	if (rc != EOK)
		return rc;

	rv = fputc('(', f);
	if (rv < 0)
		return EIO;

	/* Print arguments */

	first = true;
	arg = cgtype_func_first(func);

	while (arg != NULL) {
		if (!first) {
			rv = fputs(", ", f);
			if (rv < 0)
				return EIO;
		}

		rc = cgtype_print(arg->atype, f);
		if (rc != EOK)
			return rc;

		first = false;
		arg = cgtype_func_next(arg);
	}

	if (func->variadic) {
		rv = fputs(", ...", f);
		if (rv < 0)
			return EIO;
	}

	rv = fputc(')', f);
	if (rv < 0)
		return EIO;

	if (func->cconv == cgcc_usr) {
		rv = fputs(" __attribute__((usr))", f);
		if (rv < 0)
			return EIO;
	}

	return EOK;
}

/** Clone function type.
 *
 * @param orig Original function type
 * @param rcopy Place to store pointer to copy
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgtype_func_clone(cgtype_func_t *orig, cgtype_t **rcopy)
{
	cgtype_func_t *copy = NULL;
	cgtype_t *rtcopy = NULL;
	cgtype_t *catype = NULL;
	cgtype_func_arg_t *arg;
	int rc;

	rc = cgtype_clone(orig->rtype, &rtcopy);
	if (rc != EOK)
		goto error;

	rc = cgtype_func_create(rtcopy, &copy);
	if (rc != EOK)
		goto error;

	rtcopy = NULL; /* ownership transferred */

	/* Copy arguments */

	arg = cgtype_func_first(orig);
	while (arg != NULL) {
		rc = cgtype_clone(arg->atype, &catype);
		if (rc != EOK)
			goto error;

		rc = cgtype_func_append_arg(copy, catype);
		if (rc != EOK)
			goto error;

		catype = NULL; /* ownership transferred */

		arg = cgtype_func_next(arg);
	}

	copy->variadic = orig->variadic;
	copy->cconv = orig->cconv;
	*rcopy = &copy->cgtype;
	return EOK;

error:
	if (rtcopy != NULL)
		cgtype_destroy(rtcopy);
	if (copy != NULL)
		cgtype_destroy(&copy->cgtype);
	if (catype != NULL)
		cgtype_destroy(catype);
	return rc;
}

/** Compose function types.
 *
 * @param a First function type
 * @param b Second function type
 * @param rcomp Place to store pointer to composite type
 * @return EOK on success, EINVAL if the two types are not compatible,
 *         ENOMEM if out of memory
 */
static int cgtype_func_compose(cgtype_func_t *a, cgtype_func_t *b,
    cgtype_t **rcomp)
{
	cgtype_func_t *comp = NULL;
	cgtype_t *rtcomp = NULL;
	cgtype_t *catype = NULL;
	cgtype_func_arg_t *aarg, *barg;
	int rc;

	rc = cgtype_compose(a->rtype, b->rtype, &rtcomp);
	if (rc != EOK)
		goto error;

	rc = cgtype_func_create(rtcomp, &comp);
	if (rc != EOK)
		goto error;

	rtcomp = NULL; /* ownership transferred */

	/* Compose argument types */

	aarg = cgtype_func_first(a);
	barg = cgtype_func_first(b);
	while (aarg != NULL && barg != NULL) {
		rc = cgtype_compose(aarg->atype, barg->atype, &catype);
		if (rc != EOK)
			goto error;

		rc = cgtype_func_append_arg(comp, catype);
		if (rc != EOK)
			goto error;

		catype = NULL; /* ownership transferred */

		aarg = cgtype_func_next(aarg);
		barg = cgtype_func_next(barg);
	}

	/* One type has more arguments? */
	if (aarg != NULL || barg != NULL) {
		rc = EINVAL;
		goto error;
	}

	/* One is variadic, the other is not? */
	if (a->variadic != b->variadic) {
		rc = EINVAL;
		goto error;
	}

	/* Mismatched calling conventions? */
	if (a->cconv != b->cconv) {
		rc = EINVAL;
		goto error;
	}

	comp->cconv = a->cconv;
	comp->variadic = a->variadic;
	*rcomp = &comp->cgtype;
	return EOK;

error:
	if (rtcomp != NULL)
		cgtype_destroy(rtcomp);
	if (comp != NULL)
		cgtype_destroy(&comp->cgtype);
	if (catype != NULL)
		cgtype_destroy(catype);
	return rc;
}

/** Destroy function type.
 *
 * @param func Function type
 */
static void cgtype_func_destroy(cgtype_func_t *func)
{
	cgtype_func_arg_t *arg;

	cgtype_destroy(func->rtype);

	/* Destroy arguments */
	arg = cgtype_func_first(func);
	while (arg != NULL) {
		cgtype_destroy(arg->atype);
		list_remove(&arg->largs);
		free(arg);

		arg = cgtype_func_first(func);
	}

	free(func);
}

/** Append argument to function type.
 *
 * @param func Function type
 * @param atype Argument type (ownership transferred)
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_func_append_arg(cgtype_func_t *func, cgtype_t *atype)
{
	cgtype_func_arg_t *arg;

	arg = calloc(1, sizeof(cgtype_func_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->func = func;
	list_append(&arg->largs, &func->args);
	arg->atype = atype;
	return EOK;
}

/** Get first argument of function type.
 *
 * @param func Function type
 * @return First argument or @c NULL if none
 */
cgtype_func_arg_t *cgtype_func_first(cgtype_func_t *func)
{
	link_t *link;

	link = list_first(&func->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgtype_func_arg_t, largs);
}

/** Get next argument of function type.
 *
 * @param cur Current argument
 * @return Next argument or @c NULL if none
 */
cgtype_func_arg_t *cgtype_func_next(cgtype_func_arg_t *cur)
{
	link_t *link;

	link = list_next(&cur->largs, &cur->func->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgtype_func_arg_t, largs);
}

/** Get last argument of function type.
 *
 * @param func Function type
 * @return last argument or @c NULL if none
 */
cgtype_func_arg_t *cgtype_func_last(cgtype_func_t *func)
{
	link_t *link;

	link = list_last(&func->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgtype_func_arg_t, largs);
}

/** Get previous argument of function type.
 *
 * @param cur Current argument
 * @return Previous argument or @c NULL if none
 */
cgtype_func_arg_t *cgtype_func_prev(cgtype_func_arg_t *cur)
{
	link_t *link;

	link = list_prev(&cur->largs, &cur->func->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgtype_func_arg_t, largs);
}

/** Create pointer type.
 *
 * @param tgtype Pointer target type (ownership transferred)
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

/** Compose pointer types.
 *
 * @param a First pointer type
 * @param b Second pointer type
 * @param rcomp Place to store pointer to composite type
 * @return EOK on success, EINVAL if the two types are not compatible,
 *         ENOMEM if out of memory
 */
static int cgtype_pointer_compose(cgtype_pointer_t *a, cgtype_pointer_t *b,
    cgtype_t **rcomp)
{
	cgtype_pointer_t *comp = NULL;
	cgtype_t *tgcomp = NULL;
	int rc;

	rc = cgtype_compose(a->tgtype, b->tgtype, &tgcomp);
	if (rc != EOK)
		return rc;

	rc = cgtype_pointer_create(tgcomp, &comp);
	if (rc != EOK) {
		cgtype_destroy(tgcomp);
		return rc;
	}

	*rcomp = &comp->cgtype;
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

/** Create record type.
 *
 * @param cgrec Code generator record definition
 * @param rrecord Place to store pointer to new record type
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_record_create(cgen_record_t *cgrec, cgtype_record_t **rrecord)
{
	cgtype_record_t *record;

	record = calloc(1, sizeof(cgtype_record_t));
	if (record == NULL)
		return ENOMEM;

	record->cgtype.ntype = cgn_record;
	record->cgtype.ext = record;
	record->record = cgrec;
	*rrecord = record;
	return EOK;
}

/** Print record type.
 *
 * @param record Record type
 * @param f Output stream
 *
 * @return EOK on success, EIO on I/O error
 */
static int cgtype_record_print(cgtype_record_t *record, FILE *f)
{
	int rv;
	const char *rtype = NULL;
	const char *cident;

	switch (record->record->rtype) {
	case cgr_struct:
		rtype = "struct";
		break;
	case cgr_union:
		rtype = "union";
		break;
	}

	if (record->record->cident != NULL)
		cident = record->record->cident;
	else
		cident = "<anonymous>";

	rv = fprintf(f, "%s %s", rtype, cident);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Clone record type.
 *
 * @param orig Original record type
 * @param rcopy Place to store pointer to copy
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgtype_record_clone(cgtype_record_t *orig, cgtype_t **rcopy)
{
	cgtype_record_t *copy = NULL;
	int rc;

	rc = cgtype_record_create(orig->record, &copy);
	if (rc != EOK)
		return rc;

	*rcopy = &copy->cgtype;
	return EOK;
}

/** Compose record types.
 *
 * @param a First record type
 * @param b Second record type
 * @param rcomp Place to store pointer to composite type
 * @return EOK on success, EINVAL if the two types are not compatible,
 *         ENOMEM if out of memory
 */
static int cgtype_record_compose(cgtype_record_t *a, cgtype_record_t *b,
    cgtype_t **rcomp)
{
	cgtype_record_t *comp = NULL;
	int rc;

	if (a->record != b->record)
		return EINVAL;

	rc = cgtype_record_create(a->record, &comp);
	if (rc != EOK)
		return rc;

	*rcomp = &comp->cgtype;
	return EOK;
}

/** Destroy record type.
 *
 * @param record Record type
 */
static void cgtype_record_destroy(cgtype_record_t *record)
{
	free(record);
}

/** Create enum type.
 *
 * @param cgenum Code generator enum definition
 * @param renum Place to store pointer to new enum type
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_enum_create(cgen_enum_t *cgenum, cgtype_enum_t **renum)
{
	cgtype_enum_t *tenum;

	tenum = calloc(1, sizeof(cgtype_enum_t));
	if (tenum == NULL)
		return ENOMEM;

	tenum->cgtype.ntype = cgn_enum;
	tenum->cgtype.ext = tenum;
	tenum->cgenum = cgenum;
	*renum = tenum;
	return EOK;
}

/** Print enum type.
 *
 * @param tenum Enum type
 * @param f Output stream
 *
 * @return EOK on success, EIO on I/O error
 */
static int cgtype_enum_print(cgtype_enum_t *tenum, FILE *f)
{
	int rv;
	const char *cident;

	if (tenum->cgenum->cident != NULL)
		cident = tenum->cgenum->cident;
	else
		cident = "<anonymous>";

	rv = fprintf(f, "enum %s", cident);
	if (rv < 0)
		return EIO;

	return EOK;
}

/** Clone enum type.
 *
 * @param orig Original enum type
 * @param rcopy Place to store pointer to copy
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgtype_enum_clone(cgtype_enum_t *orig, cgtype_t **rcopy)
{
	cgtype_enum_t *copy = NULL;
	int rc;

	rc = cgtype_enum_create(orig->cgenum, &copy);
	if (rc != EOK)
		return rc;

	*rcopy = &copy->cgtype;
	return EOK;
}

/** Compose enum types.
 *
 * @param a First enum type
 * @param b Second enum type
 * @param rcomp Place to store pointer to composite type
 * @return EOK on success, EINVAL if the two types are not compatible,
 *         ENOMEM if out of memory
 */
static int cgtype_enum_compose(cgtype_enum_t *a, cgtype_enum_t *b,
    cgtype_t **rcomp)
{
	cgtype_enum_t *comp = NULL;
	int rc;

	if (a->cgenum != b->cgenum)
		return EINVAL;

	rc = cgtype_enum_create(a->cgenum, &comp);
	if (rc != EOK)
		return rc;

	*rcomp = &comp->cgtype;
	return EOK;
}

/** Destroy enum type.
 *
 * @param tenum Enum type
 */
static void cgtype_enum_destroy(cgtype_enum_t *tenum)
{
	free(tenum);
}

/** Create array type.
 *
 * @param etype Array element type (ownership transferred)
 * @param itype Index type or @c NULL (ownership transferred)
 * @param have_size @c true iff array has a specified size
 * @param asize Array size
 * @param rarray Place to store pointer to new array type
 * @return EOK on success, ENOMEM if out of memory
 */
int cgtype_array_create(cgtype_t *etype, cgtype_t *itype, bool have_size,
    uint64_t asize, cgtype_array_t **rarray)
{
	cgtype_array_t *array;

	array = calloc(1, sizeof(cgtype_array_t));
	if (array == NULL)
		return ENOMEM;

	array->cgtype.ntype = cgn_array;
	array->cgtype.ext = array;
	array->etype = etype;
	array->itype = itype;
	array->have_size = have_size;
	array->asize = asize;
	*rarray = array;
	return EOK;
}

/** Print array type.
 *
 * @param array Array type
 * @param f Output stream
 *
 * @return EOK on success, EIO on I/O error
 */
static int cgtype_array_print(cgtype_array_t *array, FILE *f)
{
	int rc;
	int rv;

	rv = fputc('[', f);
	if (rv < 0)
		return EIO;

	if (array->have_size) {
		rv = fprintf(f, PRIu64, array->asize);
		if (rv < 0)
			return EIO;
	}

	if (array->itype != NULL) {
		rv = fputc(':', f);
		if (rv < 0)
			return EIO;

		rc = cgtype_print(array->itype, f);
		if (rc != EOK)
			return rc;
	}

	rv = fputc(']', f);
	if (rv < 0)
		return EIO;

	return cgtype_print(array->etype, f);
}

/** Clone array type.
 *
 * @param orig Original array type
 * @param rcopy Place to store pointer to copy
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgtype_array_clone(cgtype_array_t *orig, cgtype_t **rcopy)
{
	cgtype_array_t *copy = NULL;
	cgtype_t *ecopy = NULL;
	cgtype_t *icopy = NULL;
	int rc;

	rc = cgtype_clone(orig->etype, &ecopy);
	if (rc != EOK)
		return rc;

	if (orig->itype != NULL) {
		rc = cgtype_clone(orig->itype, &icopy);
		if (rc != EOK)
			return rc;
	}

	rc = cgtype_array_create(ecopy, icopy, orig->have_size, orig->asize,
	    &copy);
	if (rc != EOK) {
		cgtype_destroy(ecopy);
		cgtype_destroy(icopy);
		return rc;
	}

	*rcopy = &copy->cgtype;
	return EOK;
}

/** Compose array types.
 *
 * @param a First array type
 * @param b Second array type
 * @param rcomp Place to store pointer to composite type
 * @return EOK on success, EINVAL if the two types are not compatible,
 *         ENOMEM if out of memory
 */
static int cgtype_array_compose(cgtype_array_t *a, cgtype_array_t *b,
    cgtype_t **rcomp)
{
	cgtype_array_t *comp = NULL;
	cgtype_t *ecomp = NULL;
	cgtype_t *icomp = NULL;
	bool have_size;
	uint64_t asize;
	int rc;

	rc = cgtype_compose(a->etype, b->etype, &ecomp);
	if (rc != EOK)
		goto error;

	have_size = false;
	asize = 0;

	if (a->have_size) {
		have_size = true;
		asize = a->asize;
	}

	if (b->have_size) {
		have_size = true;
		asize = b->asize;
	}

	if (a->itype != NULL) {
		rc = cgtype_clone(a->itype, &icomp);
		if (rc != EOK)
			goto error;
	} else if (b->itype != NULL) {
		rc = cgtype_clone(b->itype, &icomp);
		if (rc != EOK)
			goto error;
	}

	rc = cgtype_array_create(ecomp, icomp, have_size, asize, &comp);
	if (rc != EOK) {
		cgtype_destroy(ecomp);
		return rc;
	}

	*rcomp = &comp->cgtype;
	return EOK;
error:
	cgtype_destroy(ecomp);
	cgtype_destroy(icomp);
	return rc;
}

/** Destroy array type.
 *
 * @param array Array type
 */
static void cgtype_array_destroy(cgtype_array_t *array)
{
	cgtype_destroy(array->etype);
	free(array);
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
	case cgn_func:
		return cgtype_func_clone((cgtype_func_t *) orig->ext, rcopy);
	case cgn_pointer:
		return cgtype_pointer_clone((cgtype_pointer_t *) orig->ext,
		    rcopy);
	case cgn_record:
		return cgtype_record_clone((cgtype_record_t *) orig->ext,
		    rcopy);
	case cgn_enum:
		return cgtype_enum_clone((cgtype_enum_t *) orig->ext,
		    rcopy);
	case cgn_array:
		return cgtype_array_clone((cgtype_array_t *) orig->ext,
		    rcopy);
	}

	assert(false);
	return EINVAL;
}

/** Construct composite type.
 *
 * Composite type is created by combining the elements of two compatible
 * types to produce the most specified type.
 *
 * @param a First type
 * @param b Second type
 * @param rcomp Place to store pointer to new, composite type
 * @return EOK on success, EINVAL if the two types are not compatible,
 *         ENOMEM if out of memory
 */
int cgtype_compose(cgtype_t *a, cgtype_t *b, cgtype_t **rcomp)
{
	if (a->ntype != b->ntype)
		return EINVAL;

	switch (a->ntype) {
	case cgn_basic:
		return cgtype_basic_compose((cgtype_basic_t *) a->ext,
		    (cgtype_basic_t *) b->ext, rcomp);
	case cgn_func:
		return cgtype_func_compose((cgtype_func_t *) a->ext,
		    (cgtype_func_t *) b->ext, rcomp);
	case cgn_pointer:
		return cgtype_pointer_compose((cgtype_pointer_t *) a->ext,
		    (cgtype_pointer_t *) b->ext, rcomp);
	case cgn_record:
		return cgtype_record_compose((cgtype_record_t *) a->ext,
		    (cgtype_record_t *) b->ext, rcomp);
	case cgn_enum:
		return cgtype_enum_compose((cgtype_enum_t *) a->ext,
		    (cgtype_enum_t *) b->ext, rcomp);
	case cgn_array:
		return cgtype_array_compose((cgtype_array_t *) a->ext,
		    (cgtype_array_t *) b->ext, rcomp);
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
	case cgn_func:
		cgtype_func_destroy((cgtype_func_t *) cgtype->ext);
		break;
	case cgn_pointer:
		cgtype_pointer_destroy((cgtype_pointer_t *) cgtype->ext);
		break;
	case cgn_record:
		cgtype_record_destroy((cgtype_record_t *) cgtype->ext);
		break;
	case cgn_enum:
		cgtype_enum_destroy((cgtype_enum_t *) cgtype->ext);
		break;
	case cgn_array:
		cgtype_array_destroy((cgtype_array_t *) cgtype->ext);
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
	case cgn_func:
		return cgtype_func_print((cgtype_func_t *) cgtype->ext, f);
	case cgn_pointer:
		return cgtype_pointer_print((cgtype_pointer_t *) cgtype->ext,
		    f);
	case cgn_record:
		return cgtype_record_print((cgtype_record_t *) cgtype->ext, f);
	case cgn_enum:
		return cgtype_enum_print((cgtype_enum_t *) cgtype->ext, f);
	case cgn_array:
		return cgtype_array_print((cgtype_array_t *) cgtype->ext, f);
	}

	assert(false);
	return EINVAL;
}

/** Determine if type is void.
 *
 * @param cgtype Code generator type
 * @return @c true iff type is void
 */
bool cgtype_is_void(cgtype_t *cgtype)
{
	cgtype_basic_t *basic;

	if (cgtype->ntype != cgn_basic)
		return false;

	basic = (cgtype_basic_t *)cgtype->ext;
	return basic->elmtype == cgelm_void;
}

/** Determine integer rank of type.
 *
 * @param cgtype Code generator type (integer type)
 * @return @c true iff type is void
 */
cgtype_int_rank_t cgtype_int_rank(cgtype_t *cgtype)
{
	cgtype_basic_t *basic;
	cgtype_int_rank_t rank;

	assert(cgtype->ntype == cgn_basic);

	basic = (cgtype_basic_t *)cgtype->ext;

	switch (basic->elmtype) {
	case cgelm_void:
	case cgelm_va_list:
		assert(false);
		break;
	case cgelm_char:
	case cgelm_uchar:
		rank = cgir_char;
		break;
	case cgelm_short:
	case cgelm_ushort:
		rank = cgir_short;
		break;
	case cgelm_int:
	case cgelm_uint:
		rank = cgir_int;
		break;
	case cgelm_long:
	case cgelm_ulong:
		rank = cgir_long;
		break;
	case cgelm_longlong:
	case cgelm_ulonglong:
		rank = cgir_longlong;
		break;
	case cgelm_logic:
		rank = cgir_int;
		break;
	}

	return rank;
}

/** Construct integer type from signedness and rank.
 *
 * @param sign True iff type should be signed
 * @param rank Integer rank
 * @param rtype Place to store new type
 * @return EOK on success or an error code
 */
int cgtype_int_construct(bool sign, cgtype_int_rank_t rank, cgtype_t **rtype)
{
	cgtype_elmtype_t etsigned;
	cgtype_elmtype_t etunsigned;
	cgtype_basic_t *tbasic;
	int rc;

	switch (rank) {
	case cgir_char:
		etsigned = cgelm_char;
		etunsigned = cgelm_uchar;
		break;
	case cgir_short:
		etsigned = cgelm_short;
		etunsigned = cgelm_ushort;
		break;
	case cgir_int:
		etsigned = cgelm_int;
		etunsigned = cgelm_uint;
		break;
	case cgir_long:
		etsigned = cgelm_long;
		etunsigned = cgelm_ulong;
		break;
	case cgir_longlong:
		etsigned = cgelm_longlong;
		etunsigned = cgelm_ulonglong;
		break;
	}

	rc = cgtype_basic_create(sign ? etsigned : etunsigned, &tbasic);
	if (rc != EOK)
		return rc;

	*rtype = &tbasic->cgtype;
	return EOK;
}

/** Determine if two pointer types point to qualified or unqualified
 * versions of compatible types.
 *
 * @param sptr Source pointer type
 * @param dptr Destination pointer type
 */
bool cgtype_ptr_compatible(cgtype_pointer_t *sptr, cgtype_pointer_t *dptr)
{
	cgtype_t *ctgtype;
	int rc;

	rc = cgtype_compose(sptr->tgtype, dptr->tgtype, &ctgtype);
	if (rc != EOK)
		return false;

	cgtype_destroy(ctgtype);
	return true;
}

/** Combine qualifiers from two compatible pointer types.
 *
 * Resulting type has all the qualifiers from both types.
 *
 * @param aptr First pointer type
 * @param bptr Second pointer type
 * @param rrtype Place to store resulting type
 * @return EOK on success or an error code
 */
int cgtype_ptr_combine_qual(cgtype_pointer_t *aptr, cgtype_pointer_t *bptr,
    cgtype_t **rrtype)
{
	assert(cgtype_ptr_compatible(aptr, bptr));
	// XXX TODO qualifiers
	return cgtype_clone(&aptr->cgtype, rrtype);
}

/** Return @c true iff @a cgtype is a strict enum.
 *
 * An enum is strict iff it is named, i.e. it has a tag, typedef or
 * an instance.
 *
 * @param @a cgtype Type
 * @return @c true if @a cgtype is a strict enum, @c false otherwise
 */
bool cgtype_is_strict_enum(cgtype_t *cgtype)
{
	cgtype_enum_t *tenum;

	if (cgtype->ntype != cgn_enum)
		return false;

	tenum = (cgtype_enum_t *)cgtype->ext;
	return tenum->cgenum->named;
}
