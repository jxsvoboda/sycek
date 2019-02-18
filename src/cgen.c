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
 * Code generator
 *
 * Generate IR (machine-independent assembly) from abstract syntax tree (AST).
 */

#include <assert.h>
#include <ast.h>
#include <comp.h>
#include <cgen.h>
#include <ir.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>

/** Prefix identifier with '@' global variable prefix.
 *
 * @param ident Identifier
 * @param rpident Place to store pointer to new prefixed identifier
 * @return EOK on success, ENOMEM if out of memory
 */
static int cgen_gprefix(const char *ident, char **rpident)
{
	size_t len;
	char *pident;

	len = strlen(ident);
	pident = malloc(len + 2);
	if (pident == NULL)
		return ENOMEM;

	pident[0] = '@';
	strncpy(pident + 1, ident, len + 1);

	*rpident = pident;
	return EOK;
}

/** Create code generator.
 *
 * @param rcgen Place to store pointer to new code generator
 * @return EOK on success, ENOMEM if out of memory
 */
int cgen_create(cgen_t **rcgen)
{
	cgen_t *cgen;

	cgen = calloc(1, sizeof(cgen_t));
	if (cgen == NULL)
		return ENOMEM;

	*rcgen = cgen;
	return EOK;
}

/** Generate code for global declaration.
 *
 * @param cgen Code generator
 * @param gdecln Global declaration
 * @return EOK on success or an error code
 */
static int cgen_gdecln(cgen_t *cgen, ast_gdecln_t *gdecln, ir_module_t *irmod)
{
	ir_proc_t *proc = NULL;
	ir_lblock_t *lblock = NULL;
	ast_tok_t *aident;
	comp_tok_t *ident;
	char *pident = NULL;
	int rc;

	(void) cgen;

	if (gdecln->body != NULL) {
		aident = ast_gdecln_get_ident(gdecln);
		ident = (comp_tok_t *) aident->data;
		rc = cgen_gprefix(ident->tok.text, &pident);
		if (rc != EOK)
			goto error;

		rc = ir_lblock_create(&lblock);
		if (rc != EOK)
			goto error;

		rc = ir_proc_create(pident, lblock, &proc);
		if (rc != EOK)
			goto error;

		free(pident);
		pident = NULL;
		lblock = NULL;

		ir_module_append(irmod, &proc->decln);
		proc = NULL;
	}

	return EOK;
error:
	ir_proc_destroy(proc);
	if (pident != NULL)
		free(pident);
	return rc;
}

/** Generate code for global declaration.
 *
 * @param cgen Code generator
 * @param decln Global (macro, extern) declaration
 * @return EOK on success or an error code
 */
static int cgen_global_decln(cgen_t *cgen, ast_node_t *decln, ir_module_t *irmod)
{
	int rc;

	switch (decln->ntype) {
	case ant_gdecln:
		rc = cgen_gdecln(cgen, (ast_gdecln_t *) decln->ext, irmod);
		break;
	case ant_gmdecln:
		assert(false); // XXX
		rc = EINVAL;
		break;
	case ant_nulldecln:
	case ant_externc:
		// TODO
		rc = EOK;
		break;
	default:
		assert(false);
		rc = EINVAL;
		break;
	}

	return rc;
}

/** Generate code for module.
 *
 * @param cgen Code generator
 * @param astmod AST module
 * @param rirmod Place to store pointer to new IR module
 * @return EOK on success or an error code
 */
int cgen_module(cgen_t *cgen, ast_module_t *astmod, ir_module_t **rirmod)
{
	ir_module_t *irmod;
	int rc;
	ast_node_t *decln;

	(void) cgen;
	(void) astmod;

	rc = ir_module_create(&irmod);
	if (rc != EOK)
		return rc;

	decln = ast_module_first(astmod);
	while (decln != NULL) {
		rc = cgen_global_decln(cgen, decln, irmod);
		if (rc != EOK)
			goto error;

		decln = ast_module_next(decln);
	}

	*rirmod = irmod;
	return EOK;
error:
	ir_module_destroy(irmod);
	return rc;
}

/** Destroy code generator.
 *
 * @param cgen Code generator or @c NULL
 */
void cgen_destroy(cgen_t *cgen)
{
	if (cgen == NULL)
		return;

	free(cgen);
}
