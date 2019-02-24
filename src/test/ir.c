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
 * Test IR
 */

#include <assert.h>
#include <ir.h>
#include <merrno.h>
#include <stdio.h>
#include <string.h>
#include <test/ir.h>

/** Test IR module.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ir_module(void)
{
	ir_module_t *module = NULL;
	ir_proc_t *proc1 = NULL;
	ir_proc_t *proc2 = NULL;
	ir_lblock_t *lblock1 = NULL;
	ir_lblock_t *lblock2 = NULL;
	ir_decln_t *decln;
	int rc;

	rc = ir_module_create(&module);
	if (rc != EOK)
		return rc;

	assert(module != NULL);

	rc = ir_lblock_create(&lblock1);
	if (rc != EOK)
		return rc;

	assert(lblock1 != NULL);

	rc = ir_lblock_create(&lblock2);
	if (rc != EOK)
		return rc;

	assert(lblock2 != NULL);

	rc = ir_proc_create("@foo1", lblock1, &proc1);
	if (rc != EOK)
		return rc;

	assert(proc1 != NULL);

	rc = ir_proc_create("@foo2", lblock2, &proc2);
	if (rc != EOK)
		return rc;

	assert(proc1 != NULL);

	ir_module_append(module, &proc1->decln);
	ir_module_append(module, &proc2->decln);

	rc = ir_module_print(module, stdout);
	if (rc != EOK)
		return rc;

	/* Forward iteration */
	decln = ir_module_first(module);
	assert(decln != NULL);
	assert(&proc1->decln == decln);

	decln = ir_module_next(decln);
	assert(decln != NULL);
	assert(&proc2->decln == decln);

	decln = ir_module_next(decln);
	assert(decln == NULL);

	/* Backward iteration */
	decln = ir_module_last(module);
	assert(decln != NULL);
	assert(&proc2->decln == decln);

	decln = ir_module_prev(decln);
	assert(decln != NULL);
	assert(&proc1->decln == decln);

	decln = ir_module_prev(decln);
	assert(decln == NULL);

	ir_module_destroy(module);
	return EOK;
}

/** Test IR procedure.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ir_proc(void)
{
	ir_proc_t *proc = NULL;
	ir_lblock_t *lblock = NULL;
	int rc;

	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		return rc;

	assert(lblock != NULL);

	rc = ir_proc_create("@foo", lblock, &proc);
	if (rc != EOK)
		return rc;

	assert(proc != NULL);

	rc = ir_proc_print(proc, stdout);
	if (rc != EOK)
		return rc;

	ir_proc_destroy(proc);

	return EOK;
}

/** Test IR labeled block.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ir_lblock(void)
{
	ir_lblock_t *lblock = NULL;
	ir_instr_t *instr1 = NULL;
	ir_instr_t *instr2 = NULL;
	ir_lblock_entry_t *entry;
	int rc;

	rc = ir_lblock_create(&lblock);
	if (rc != EOK)
		return rc;

	assert(lblock != NULL);

	rc = ir_instr_create(&instr1);
	if (rc != EOK)
		return rc;

	assert(instr1 != NULL);
	instr1->itype = iri_add;
	instr1->width = 8;

	rc = ir_instr_create(&instr2);
	if (rc != EOK)
		return rc;

	assert(instr2 != NULL);
	instr2->itype = iri_ldimm;
	instr2->width = 16;

	rc = ir_lblock_append(lblock, NULL, instr1);
	if (rc != EOK)
		return rc;

	rc = ir_lblock_append(lblock, "%l.1", instr2);
	if (rc != EOK)
		return rc;

	rc = ir_lblock_print(lblock, stdout);
	if (rc != EOK)
		return rc;

	/* Forward iteration */
	entry = ir_lblock_first(lblock);
	assert(entry != NULL);
	assert(entry->label == NULL);
	assert(entry->instr == instr1);

	entry = ir_lblock_next(entry);
	assert(entry != NULL);
	assert(entry->label != NULL);
	assert(strcmp(entry->label, "%l.1") == 0);
	assert(entry->instr == instr2);

	entry = ir_lblock_next(entry);
	assert(entry == NULL);

	/* Backward iteration */
	entry = ir_lblock_last(lblock);
	assert(entry != NULL);
	assert(entry->label != NULL);
	assert(strcmp(entry->label, "%l.1") == 0);
	assert(entry->instr == instr2);

	entry = ir_lblock_prev(entry);
	assert(entry != NULL);
	assert(entry->label == NULL);
	assert(entry->instr == instr1);

	entry = ir_lblock_prev(entry);
	assert(entry == NULL);

	ir_lblock_destroy(lblock);
	return EOK;
}

/** Test IR instruction.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ir_instr(void)
{
	ir_instr_t *instr = NULL;
	ir_oper_var_t *dest = NULL;
	ir_oper_var_t *op1 = NULL;
	ir_oper_var_t *op2 = NULL;
	int rc;

	rc = ir_instr_create(&instr);
	if (rc != EOK)
		return rc;

	assert(instr != NULL);
	instr->itype = iri_add;
	instr->width = 8;

	rc = ir_oper_var_create("%2", &dest);
	if (rc != EOK)
		return rc;

	rc = ir_oper_var_create("%1", &op1);
	if (rc != EOK)
		return rc;

	rc = ir_oper_var_create("%foo", &op2);
	if (rc != EOK)
		return rc;

	instr->dest = &dest->oper;
	instr->op1 = &op1->oper;
	instr->op2 = &op2->oper;

	rc = ir_instr_print(instr, stdout);
	if (rc != EOK)
		return rc;

	ir_instr_destroy(instr);
	return EOK;
}

/** Test IR operand.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ir_oper(void)
{
	ir_oper_var_t *var = NULL;
	ir_oper_imm_t *imm = NULL;
	int rc;
	int rv;

	rc = ir_oper_var_create("%1", &var);
	if (rc != EOK)
		return rc;

	assert(var != NULL);

	rc = ir_oper_imm_create(-1, &imm);
	if (rc != EOK)
		return rc;

	assert(imm != NULL);

	rc = ir_oper_print(&var->oper, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	rc = ir_oper_print(&imm->oper, stdout);
	if (rc != EOK)
		return rc;

	rv = fputc('\n', stdout);
	if (rv < 0)
		return EIO;

	ir_oper_destroy(&var->oper);
	ir_oper_destroy(&imm->oper);
	return EOK;
}

/** Run IR tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_ir(void)
{
	int rc;

	rc = test_ir_module();
	if (rc != EOK)
		return rc;

	rc = test_ir_proc();
	if (rc != EOK)
		return rc;

	rc = test_ir_lblock();
	if (rc != EOK)
		return rc;

	rc = test_ir_instr();
	if (rc != EOK)
		return rc;

	rc = test_ir_oper();
	if (rc != EOK)
		return rc;

	return EOK;
}
