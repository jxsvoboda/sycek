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
 * Test code generator C types
 */

#include <cgtype.h>
#include <merrno.h>
#include <test/cgtype.h>

/** Test code generator basic type.
 *
 * @return EOK on success or non-zero error code
 */
static int test_cgtype_basic(void)
{
	int rc;
	cgtype_basic_t *basic = NULL;
	cgtype_t *copy = NULL;

	rc = cgtype_basic_create(cgelm_int, &basic);
	if (rc != EOK)
		goto error;

	rc = cgtype_print(&basic->cgtype, stdout);
	if (rc != EOK)
		goto error;

	rc = cgtype_clone(&basic->cgtype, &copy);
	if (rc != EOK)
		goto error;

	cgtype_destroy(copy);
	cgtype_destroy(&basic->cgtype);
	return EOK;
error:
	if (basic != NULL)
		cgtype_destroy(&basic->cgtype);
	return rc;
}

/** Test code generator pointer type.
 *
 * @return EOK on success or non-zero error code
 */
static int test_cgtype_pointer(void)
{
	int rc;
	cgtype_basic_t *basic = NULL;
	cgtype_pointer_t *pointer = NULL;
	cgtype_t *copy = NULL;

	rc = cgtype_basic_create(cgelm_void, &basic);
	if (rc != EOK)
		goto error;

	rc = cgtype_pointer_create(&basic->cgtype, &pointer);
	if (rc != EOK)
		goto error;

	basic = NULL;

	rc = cgtype_print(&pointer->cgtype, stdout);
	if (rc != EOK)
		goto error;

	rc = cgtype_clone(&pointer->cgtype, &copy);
	if (rc != EOK)
		goto error;

	cgtype_destroy(copy);
	cgtype_destroy(&pointer->cgtype);
	return EOK;
error:
	if (basic != NULL)
		cgtype_destroy(&basic->cgtype);
	if (pointer != NULL)
		cgtype_destroy(&pointer->cgtype);
	return rc;
}

/** Run code generator C type tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_cgtype(void)
{
	int rc;

	rc = test_cgtype_basic();
	if (rc != EOK)
		return rc;

	rc = test_cgtype_pointer();
	if (rc != EOK)
		return rc;

	return EOK;
}
