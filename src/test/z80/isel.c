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
 * Test instruction selection
 */

#include <ir.h>
#include <merrno.h>
#include <test/z80/isel.h>
#include <z80/isel.h>
#include <z80/z80ic.h>

/** Test code generation for module.
 *
 * @return EOK on success or non-zero error code
 */
static int test_isel_module(void)
{
	int rc;
	z80_isel_t *isel = NULL;
	ir_module_t *irmodule = NULL;
	z80ic_module_t *icmodule = NULL;

	rc = z80_isel_create(&isel);
	if (rc != EOK)
		goto error;

	rc = ir_module_create(&irmodule);
	if (rc != EOK)
		goto error;

	rc = z80_isel_module(isel, irmodule, &icmodule);
	if (rc != EOK)
		goto error;

	ir_module_destroy(irmodule);
	z80ic_module_destroy(icmodule);
	z80_isel_destroy(isel);

	return EOK;
error:
	ir_module_destroy(irmodule);
	z80ic_module_destroy(icmodule);
	z80_isel_destroy(isel);
	return rc;
}

/** Run instruction selection tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_z80_isel(void)
{
	int rc;

	rc = test_isel_module();
	if (rc != EOK)
		return rc;

	return EOK;
}
