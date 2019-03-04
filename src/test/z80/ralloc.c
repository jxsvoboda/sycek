/*
 * Copyright 2019 Jiri Svoboda
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
 * Test register allocation
 */

#include <merrno.h>
#include <test/z80/ralloc.h>
#include <z80/ralloc.h>
#include <z80/z80ic.h>

/** Test register allocation for module.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ralloc_module(void)
{
	int rc;
	z80_ralloc_t *ralloc = NULL;
	z80ic_module_t *vricmodule = NULL;
	z80ic_module_t *icmodule = NULL;

	rc = z80_ralloc_create(&ralloc);
	if (rc != EOK)
		goto error;

	rc = z80ic_module_create(&vricmodule);
	if (rc != EOK)
		goto error;

	rc = z80_ralloc_module(ralloc, vricmodule, &icmodule);
	if (rc != EOK)
		goto error;

	z80ic_module_destroy(vricmodule);
	z80ic_module_destroy(icmodule);
	z80_ralloc_destroy(ralloc);

	return EOK;
error:
	z80ic_module_destroy(vricmodule);
	z80ic_module_destroy(icmodule);
	z80_ralloc_destroy(ralloc);
	return rc;
}

/** Run register allocation tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_z80_ralloc(void)
{
	int rc;

	rc = test_ralloc_module();
	if (rc != EOK)
		return rc;

	return EOK;
}
