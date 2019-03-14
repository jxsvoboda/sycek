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
 * Test Z80 IC
 */

#include <assert.h>
#include <merrno.h>
#include <string.h>
#include <test/z80/z80ic.h>
#include <z80/z80ic.h>

/** Test Z80 IC module.
 *
 * @return EOK on success or non-zero error code
 */
static int test_z80ic_module(void)
{
	z80ic_module_t *module = NULL;
	z80ic_proc_t *proc1 = NULL;
	z80ic_proc_t *proc2 = NULL;
	z80ic_lblock_t *lblock1 = NULL;
	z80ic_lblock_t *lblock2 = NULL;
	z80ic_decln_t *decln;
	int rc;

	rc = z80ic_module_create(&module);
	if (rc != EOK)
		return rc;

	assert(module != NULL);

	rc = z80ic_lblock_create(&lblock1);
	if (rc != EOK)
		return rc;

	assert(lblock1 != NULL);

	rc = z80ic_lblock_create(&lblock2);
	if (rc != EOK)
		return rc;

	assert(lblock2 != NULL);

	rc = z80ic_proc_create("@foo1", lblock1, &proc1);
	if (rc != EOK)
		return rc;

	assert(proc1 != NULL);

	rc = z80ic_proc_create("@foo2", lblock2, &proc2);
	if (rc != EOK)
		return rc;

	assert(proc1 != NULL);

	z80ic_module_append(module, &proc1->decln);
	z80ic_module_append(module, &proc2->decln);

	rc = z80ic_module_print(module, stdout);
	if (rc != EOK)
		return rc;

	/* Forward iteration */
	decln = z80ic_module_first(module);
	assert(decln != NULL);
	assert(&proc1->decln == decln);

	decln = z80ic_module_next(decln);
	assert(decln != NULL);
	assert(&proc2->decln == decln);

	decln = z80ic_module_next(decln);
	assert(decln == NULL);

	/* Backward iteration */
	decln = z80ic_module_last(module);
	assert(decln != NULL);
	assert(&proc2->decln == decln);

	decln = z80ic_module_prev(decln);
	assert(decln != NULL);
	assert(&proc1->decln == decln);

	decln = z80ic_module_prev(decln);
	assert(decln == NULL);

	z80ic_module_destroy(module);
	return EOK;
}

/** Test Z80 IC variable.
 *
 * @return EOK on success or non-zero error code
 */
static int test_z80ic_var(void)
{
	z80ic_var_t *var = NULL;
	z80ic_dblock_t *dblock = NULL;
	int rc;

	rc = z80ic_dblock_create(&dblock);
	if (rc != EOK)
		return rc;

	assert(dblock != NULL);

	rc = z80ic_var_create("@myvar", dblock, &var);
	if (rc != EOK)
		return rc;

	assert(var != NULL);

	rc = z80ic_var_print(var, stdout);
	if (rc != EOK)
		return rc;

	z80ic_var_destroy(var);

	return EOK;
}

/** Test Z80 IC data block.
 *
 * @return EOK on success or non-zero error code
 */
static int test_z80ic_dblock(void)
{
	z80ic_dblock_t *dblock = NULL;
	z80ic_dentry_t *dentry1 = NULL;
	z80ic_dentry_t *dentry2 = NULL;
	z80ic_dblock_entry_t *entry;
	int rc;

	rc = z80ic_dblock_create(&dblock);
	if (rc != EOK)
		return rc;

	assert(dblock != NULL);

	rc = z80ic_dentry_create_defb(0xff, &dentry1);
	if (rc != EOK)
		return rc;

	assert(dentry1 != NULL);

	rc = z80ic_dentry_create_defw(0xffff, &dentry2);
	if (rc != EOK)
		return rc;

	assert(dentry2 != NULL);

	rc = z80ic_dblock_append(dblock, dentry1);
	if (rc != EOK)
		return rc;

	rc = z80ic_dblock_append(dblock, dentry2);
	if (rc != EOK)
		return rc;

	rc = z80ic_dblock_print(dblock, stdout);
	if (rc != EOK)
		return rc;

	/* Forward iteration */
	entry = z80ic_dblock_first(dblock);
	assert(entry != NULL);
	assert(entry->dentry == dentry1);

	entry = z80ic_dblock_next(entry);
	assert(entry != NULL);
	assert(entry->dentry == dentry2);

	entry = z80ic_dblock_next(entry);
	assert(entry == NULL);

	/* Backward iteration */
	entry = z80ic_dblock_last(dblock);
	assert(entry != NULL);
	assert(entry->dentry == dentry2);

	entry = z80ic_dblock_prev(entry);
	assert(entry != NULL);
	assert(entry->dentry == dentry1);

	entry = z80ic_dblock_prev(entry);
	assert(entry == NULL);

	z80ic_dblock_destroy(dblock);
	return EOK;
}



/** Test Z80 IC procedure.
 *
 * @return EOK on success or non-zero error code
 */
static int test_z80ic_proc(void)
{
	z80ic_proc_t *proc = NULL;
	z80ic_lblock_t *lblock = NULL;
	int rc;

	rc = z80ic_lblock_create(&lblock);
	if (rc != EOK)
		return rc;

	assert(lblock != NULL);

	rc = z80ic_proc_create("@foo", lblock, &proc);
	if (rc != EOK)
		return rc;

	assert(proc != NULL);

	rc = z80ic_proc_print(proc, stdout);
	if (rc != EOK)
		return rc;

	z80ic_proc_destroy(proc);

	return EOK;
}

/** Test Z80 IC labeled block.
 *
 * @return EOK on success or non-zero error code
 */
static int test_z80ic_lblock(void)
{
	z80ic_lblock_t *lblock = NULL;
	z80ic_ld_vrr_nn_t *instr1 = NULL;
	z80ic_oper_vrr_t *dest1 = NULL;
	z80ic_oper_imm16_t *op1 = NULL;
	z80ic_ld_vrr_nn_t *instr2 = NULL;
	z80ic_oper_vrr_t *dest2 = NULL;
	z80ic_oper_imm16_t *op2 = NULL;
	z80ic_lblock_entry_t *entry;
	int rc;

	rc = z80ic_lblock_create(&lblock);
	if (rc != EOK)
		return rc;

	assert(lblock != NULL);

	rc = z80ic_ld_vrr_nn_create(&instr1);
	if (rc != EOK)
		return rc;

	assert(instr1 != NULL);

	rc = z80ic_oper_vrr_create(0, &dest1);
	if (rc != EOK)
		return rc;

	rc = z80ic_oper_imm16_create_val(42, &op1);
	if (rc != EOK)
		return rc;

	instr1->dest = dest1;
	instr1->imm16 = op1;

	rc = z80ic_ld_vrr_nn_create(&instr2);
	if (rc != EOK)
		return rc;

	assert(instr2 != NULL);

	rc = z80ic_oper_vrr_create(0, &dest2);
	if (rc != EOK)
		return rc;

	rc = z80ic_oper_imm16_create_val(42, &op2);
	if (rc != EOK)
		return rc;

	instr2->dest = dest2;
	instr2->imm16 = op2;

	rc = z80ic_lblock_append(lblock, NULL, &instr1->instr);
	if (rc != EOK)
		return rc;

	rc = z80ic_lblock_append(lblock, "%l.1", &instr2->instr);
	if (rc != EOK)
		return rc;

	rc = z80ic_lblock_print(lblock, stdout);
	if (rc != EOK)
		return rc;

	/* Forward iteration */
	entry = z80ic_lblock_first(lblock);
	assert(entry != NULL);
	assert(entry->label == NULL);
	assert(entry->instr == &instr1->instr);

	entry = z80ic_lblock_next(entry);
	assert(entry != NULL);
	assert(entry->label != NULL);
	assert(strcmp(entry->label, "%l.1") == 0);
	assert(entry->instr == &instr2->instr);

	entry = z80ic_lblock_next(entry);
	assert(entry == NULL);

	/* Backward iteration */
	entry = z80ic_lblock_last(lblock);
	assert(entry != NULL);
	assert(entry->label != NULL);
	assert(strcmp(entry->label, "%l.1") == 0);
	assert(entry->instr == &instr2->instr);

	entry = z80ic_lblock_prev(entry);
	assert(entry != NULL);
	assert(entry->label == NULL);
	assert(entry->instr == &instr1->instr);

	entry = z80ic_lblock_prev(entry);
	assert(entry == NULL);

	z80ic_lblock_destroy(lblock);
	return EOK;
}

/** Test Z80 IC instruction load virtual register pair from 16-bit immediate.
 *
 * @return EOK on success or non-zero error code
 */
static int test_z80ic_ld_vrr_nn(void)
{
	z80ic_ld_vrr_nn_t *instr = NULL;
	z80ic_oper_vrr_t *dest = NULL;
	z80ic_oper_imm16_t *op = NULL;
	int rc;

	rc = z80ic_ld_vrr_nn_create(&instr);
	if (rc != EOK)
		return rc;

	assert(instr != NULL);

	rc = z80ic_oper_vrr_create(0, &dest);
	if (rc != EOK)
		return rc;

	rc = z80ic_oper_imm16_create_val(42, &op);
	if (rc != EOK)
		return rc;

	instr->dest = dest;
	instr->imm16 = op;

	rc = z80ic_instr_print(&instr->instr, stdout);
	if (rc != EOK)
		return rc;

	z80ic_instr_destroy(&instr->instr);
	return EOK;
}

/** Test Z80 IC operand.
 *
 * @return EOK on success or non-zero error code
 */
static int test_z80ic_oper(void)
{
	z80ic_oper_imm8_t *imm8 = NULL;
	z80ic_oper_imm16_t *imm16v = NULL;
	z80ic_oper_imm16_t *imm16s = NULL;
	z80ic_oper_reg_t *reg = NULL;
	z80ic_oper_vr_t *vr = NULL;
	z80ic_oper_vrr_t *vrr = NULL;
	int rc;
	int rv;

	rc = z80ic_oper_imm8_create(1, &imm8);
	if (rc != EOK)
		return rc;

	assert(imm8 != NULL);

	rc = z80ic_oper_imm16_create_val(42, &imm16v);
	if (rc != EOK)
		return rc;

	assert(imm16v != NULL);

	rc = z80ic_oper_imm16_create_symbol("foo", &imm16s);
	if (rc != EOK)
		return rc;

	assert(imm16s != NULL);

	rc = z80ic_oper_reg_create(z80ic_reg_a, &reg);
	if (rc != EOK)
		return rc;

	assert(reg != NULL);

	rc = z80ic_oper_vr_create(1, &vr);
	if (rc != EOK)
		return rc;

	assert(vr != NULL);

	rc = z80ic_oper_vrr_create(2, &vrr);
	if (rc != EOK)
		return rc;

	assert(vrr != NULL);

	rc = z80ic_oper_imm8_print(imm8, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(imm16v, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_imm16_print(imm16s, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_reg_print(reg, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vr_print(vr, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	rc = z80ic_oper_vrr_print(vrr, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	z80ic_oper_imm8_destroy(imm8);
	z80ic_oper_imm16_destroy(imm16v);
	z80ic_oper_imm16_destroy(imm16s);
	z80ic_oper_reg_destroy(reg);
	z80ic_oper_vr_destroy(vr);
	z80ic_oper_vrr_destroy(vrr);

	return EOK;
}

/** Run Z80 IC tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_z80ic(void)
{
	int rc;

	rc = test_z80ic_module();
	if (rc != EOK)
		return rc;

	rc = test_z80ic_var();
	if (rc != EOK)
		return rc;

	rc = test_z80ic_dblock();
	if (rc != EOK)
		return rc;

	rc = test_z80ic_proc();
	if (rc != EOK)
		return rc;

	rc = test_z80ic_lblock();
	if (rc != EOK)
		return rc;

	rc = test_z80ic_ld_vrr_nn();
	if (rc != EOK)
		return rc;

	rc = test_z80ic_oper();
	if (rc != EOK)
		return rc;

	return EOK;
}
