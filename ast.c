/*
 * Copyright 2018 Jiri Svoboda
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
 * Abstract syntax tree
 */

#include <adt/list.h>
#include <assert.h>
#include <ast.h>
#include <merrno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static int ast_block_print(ast_block_t *, FILE *);
static ast_tok_t *ast_block_last_tok(ast_block_t *);
static int ast_dlist_print(ast_dlist_t *, FILE *);
static ast_tok_t *ast_dspecs_first_tok(ast_dspecs_t *);
static void ast_dspecs_destroy(ast_dspecs_t *);
static void ast_block_destroy(ast_block_t *);
static void ast_sqlist_destroy(ast_sqlist_t *);
static void ast_dlist_destroy(ast_dlist_t *);

/** Create AST module.
 *
 * @param rmodule Place to store pointer to new module
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_module_create(ast_module_t **rmodule)
{
	ast_module_t *module;

	module = calloc(1, sizeof(ast_module_t));
	if (module == NULL)
		return ENOMEM;

	module->node.ext = module;
	module->node.ntype = ant_module;
	list_initialize(&module->decls);
	*rmodule = module;
	return EOK;
}

/** Append declaration to module.
 *
 * @param module Module
 * @param decl Declaration
 */
void ast_module_append(ast_module_t *module, ast_node_t *decl)
{
	list_append(&decl->llist, &module->decls);
	decl->lnode = &module->node;
}

/** Return first declaration in module.
 *
 * @param module Module
 * @return First declaration or @c NULL
 */
ast_node_t *ast_module_first(ast_module_t *module)
{
	link_t *link;

	link = list_first(&module->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return next declaration in module.
 *
 * @param module Module
 * @return Next declaration or @c NULL
 */
ast_node_t *ast_module_next(ast_node_t *node)
{
	link_t *link;
	ast_module_t *module;

	assert(node->lnode->ntype == ant_module);
	module = (ast_module_t *) node->lnode->ext;

	link = list_next(&node->llist, &module->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return last declaration in module.
 *
 * @param module Module
 * @return Last declaration or @c NULL
 */
ast_node_t *ast_module_last(ast_module_t *module)
{
	link_t *link;

	link = list_last(&module->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return previous declaration in module.
 *
 * @param module Module
 * @return Previous declaration or @c NULL
 */
ast_node_t *ast_module_prev(ast_node_t *node)
{
	link_t *link;
	ast_module_t *module;

	assert(node->lnode->ntype == ant_module);
	module = (ast_module_t *) node->lnode->ext;

	link = list_prev(&node->llist, &module->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Print AST module.
 *
 * @param module Module
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_module_print(ast_module_t *module, FILE *f)
{
	ast_node_t *decl;

	if (fprintf(f, "module(") < 0)
		return EIO;

	decl = ast_module_first(module);
	while (decl != NULL) {
		ast_tree_print(decl, f);
		decl = ast_module_next(decl);
	}

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST module.
 *
 * @param module Module
 */
static void ast_module_destroy(ast_module_t *module)
{
	ast_node_t *decl;

	decl = ast_module_first(module);
	while (decl != NULL) {
		list_remove(&decl->llist);
		ast_tree_destroy(decl);
		decl = ast_module_first(module);
	}

	free(module);
}

/** Get first token of AST module.
 *
 * @param module Module
 * @return First token or @c NULL
 */
static ast_tok_t *ast_module_first_tok(ast_module_t *module)
{
	ast_node_t *decl;

	decl = ast_module_first(module);
	if (decl == NULL)
		return NULL;

	return ast_tree_first_tok(decl);
}

/** Get last token of AST module.
 *
 * @param module Module
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_module_last_tok(ast_module_t *module)
{
	ast_node_t *decl;

	decl = ast_module_last(module);
	if (decl == NULL)
		return NULL;

	return ast_tree_last_tok(decl);
}

/** Create AST storage-class specifier.
 *
 * @param sctype Storage class type
 * @param rsclass Place to store pointer to new storage class specifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_sclass_create(ast_sclass_type_t sctype,
    ast_sclass_t **rsclass)
{
	ast_sclass_t *sclass;

	sclass = calloc(1, sizeof(ast_sclass_t));
	if (sclass == NULL)
		return ENOMEM;

	sclass->sctype = sctype;

	sclass->node.ext = sclass;
	sclass->node.ntype = ant_sclass;

	*rsclass = sclass;
	return EOK;
}

/** Print AST storage-class specifier.
 *
 * @param sclass Storage-class specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_sclass_print(ast_sclass_t *sclass, FILE *f)
{
	const char *s;

	s = "<invalid>";
	switch (sclass->sctype) {
	case asc_typedef:
		s = "typedef";
		break;
	case asc_extern:
		s = "extern";
		break;
	case asc_static:
		s = "static";
		break;
	case asc_auto:
		s = "auto";
		break;
	case asc_register:
		s = "register";
		break;
	case asc_none:
		s = "none";
		break;
	}

	if (fprintf(f, "sclass(%s)", s) < 0)
		return EIO;

	return EOK;
}

/** Destroy AST storage-class specifier.
 *
 * @param sclass Storage-class specifier
 */
static void ast_sclass_destroy(ast_sclass_t *sclass)
{
	free(sclass);
}

/** Get first token of AST storage-class specifier.
 *
 * @param sclass Storage-class specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_sclass_first_tok(ast_sclass_t *sclass)
{
	return &sclass->tsclass;
}

/** Get last token of AST storage-class specifier.
 *
 * @param sclass Storage-class specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_sclass_last_tok(ast_sclass_t *sclass)
{
	return &sclass->tsclass;
}

/** Create AST global declaration.
 *
 * @param dspecs Declaration specifiers
 * @param dlist Declarator list
 * @param body Body or @c NULL
 * @param rgdecln Place to store pointer to new global declaration
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_gdecln_create(ast_dspecs_t *dspecs, ast_dlist_t *dlist,
    ast_block_t *body, ast_gdecln_t **rgdecln)
{
	ast_gdecln_t *gdecln;

	gdecln = calloc(1, sizeof(ast_gdecln_t));
	if (gdecln == NULL)
		return ENOMEM;

	gdecln->dspecs = dspecs;
	gdecln->dlist = dlist;
	gdecln->body = body;

	gdecln->node.ext = gdecln;
	gdecln->node.ntype = ant_gdecln;

	*rgdecln = gdecln;
	return EOK;
}

/** Print AST global declaration.
 *
 * @param gdecln Global declaration
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_gdecln_print(ast_gdecln_t *gdecln, FILE *f)
{
	int rc;

	if (fprintf(f, "gdecln(") < 0)
		return EIO;

	rc = ast_tree_print(&gdecln->dspecs->node, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ", ") < 0)
		return EIO;

	rc = ast_dlist_print(gdecln->dlist, f);
	if (rc != EOK)
		return rc;

	if (gdecln->body != NULL) {
		if (fprintf(f, ", ") < 0)
			return EIO;
		rc = ast_block_print(gdecln->body, f);
		if (rc != EOK)
			return rc;
	}

	if (gdecln->have_init) {
		rc = ast_tree_print(gdecln->init, f);
		if (rc != EOK)
			return rc;
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST global declaration.
 *
 * @param gdecln Global declaration
 */
static void ast_gdecln_destroy(ast_gdecln_t *gdecln)
{
	ast_dspecs_destroy(gdecln->dspecs);
	ast_dlist_destroy(gdecln->dlist);
	ast_block_destroy(gdecln->body);
	ast_tree_destroy(gdecln->init);

	free(gdecln);
}

/** Get first token of AST global declaration.
 *
 * @param gdecln Global declaration
 * @return First token or @c NULL
 */
static ast_tok_t *ast_gdecln_first_tok(ast_gdecln_t *gdecln)
{
	return ast_dspecs_first_tok(gdecln->dspecs);
}

/** Get last token of AST global declaration.
 *
 * @param gdecln Global declaration
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_gdecln_last_tok(ast_gdecln_t *gdecln)
{
	if (gdecln->have_scolon)
		return &gdecln->tscolon;
	else
		return ast_block_last_tok(gdecln->body);
}

/** Create AST block.
 *
 * @param braces Whether the block has braces or not
 * @param rblock Place to store pointer to new block
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_block_create(ast_braces_t braces, ast_block_t **rblock)
{
	ast_block_t *block;

	block = calloc(1, sizeof(ast_block_t));
	if (block == NULL)
		return ENOMEM;

	block->braces = braces;
	list_initialize(&block->stmts);

	block->node.ext = block;
	block->node.ntype = ant_block;

	*rblock = block;
	return EOK;
}

/** Append declaration to block.
 *
 * @param block Block
 * @param stmt Statement
 */
void ast_block_append(ast_block_t *block, ast_node_t *stmt)
{
	list_append(&stmt->llist, &block->stmts);
	stmt->lnode = &block->node;
}

/** Return first statement in block.
 *
 * @param block Block
 * @return First statement or @c NULL
 */
ast_node_t *ast_block_first(ast_block_t *block)
{
	link_t *link;

	link = list_first(&block->stmts);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return next statement in block.
 *
 * @param node Current statement
 * @return Next statement or @c NULL
 */
ast_node_t *ast_block_next(ast_node_t *node)
{
	link_t *link;
	ast_block_t *block;

	assert(node->lnode->ntype == ant_block);
	block = (ast_block_t *) node->lnode->ext;

	link = list_next(&node->llist, &block->stmts);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return last statement in block.
 *
 * @param block Block
 * @return Last statement or @c NULL
 */
ast_node_t *ast_block_last(ast_block_t *block)
{
	link_t *link;

	link = list_last(&block->stmts);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return previous statement in block.
 *
 * @param node Current statement
 * @return Previous statement or @c NULL
 */
ast_node_t *ast_block_prev(ast_node_t *node)
{
	link_t *link;
	ast_block_t *block;

	assert(node->lnode->ntype == ant_block);
	block = (ast_block_t *) node->lnode->ext;

	link = list_prev(&node->llist, &block->stmts);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Print AST block.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_block_print(ast_block_t *block, FILE *f)
{
	ast_node_t *stmt;
	int rc;

	if (fprintf(f, "block(%s", block->braces == ast_braces ? "{" : "") < 0)
		return EIO;

	stmt = ast_block_first(block);
	while (stmt != NULL) {
		rc = ast_tree_print(stmt, f);
		if (rc != EOK)
			return rc;

		stmt = ast_block_next(stmt);
	}
	if (fprintf(f, "%s)", block->braces == ast_braces ? "}" : "") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST block.
 *
 * @param block Block
 */
static void ast_block_destroy(ast_block_t *block)
{
	ast_node_t *stmt;

	if (block == NULL)
		return;

	stmt = ast_block_first(block);
	while (stmt != NULL) {
		list_remove(&stmt->llist);
		ast_tree_destroy(stmt);
		stmt = ast_block_first(block);
	}

	free(block);
}

/** Get first token of AST block.
 *
 * @param block Block
 * @return First token or @c NULL
 */
static ast_tok_t *ast_block_first_tok(ast_block_t *block)
{
	ast_node_t *stmt;

	if (block->braces) {
		return &block->topen;
	} else {
		stmt = ast_block_first(block);
		if (stmt == NULL)
			return NULL;
		return ast_tree_first_tok(stmt);
	}
}

/** Get last token of AST block.
 *
 * @param block Block
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_block_last_tok(ast_block_t *block)
{
	ast_node_t *stmt;

	if (block->braces) {
		return &block->tclose;
	} else {
		stmt = ast_block_last(block);
		if (stmt == NULL)
			return NULL;
		return ast_tree_last_tok(stmt);
	}
}

/** Create AST type qualifier.
 *
 * @param qtype Qualifier type
 * @param rtqual Place to store pointer to new type qualifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tqual_create(ast_qtype_t qtype, ast_tqual_t **rtqual)
{
	ast_tqual_t *tqual;

	tqual = calloc(1, sizeof(ast_tqual_t));
	if (tqual == NULL)
		return ENOMEM;

	tqual->node.ext = tqual;
	tqual->node.ntype = ant_tqual;
	tqual->qtype = qtype;

	*rtqual = tqual;
	return EOK;
}

/** Print AST type qualifier.
 *
 * @param tqual Type qualifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tqual_print(ast_tqual_t *tqual, FILE *f)
{
	const char *s = NULL;

	switch (tqual->qtype) {
	case aqt_const:
		s = "const";
		break;
	case aqt_restrict:
		s = "restrict";
		break;
	case aqt_volatile:
		s = "volatile";
		break;
	}

	if (fprintf(f, "tqual(%s)", s) < 0)
		return EIO;

	return EOK;
}

/** Destroy AST type qualifier.
 *
 * @param tqual Type qualifier
 */
static void ast_tqual_destroy(ast_tqual_t *tqual)
{
	free(tqual);
}

/** Get first token of AST type qualifier.
 *
 * @param tqual Type qualifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_tqual_first_tok(ast_tqual_t *tqual)
{
	return &tqual->tqual;
}

/** Get last token of AST type qualifier.
 *
 * @param tqual Type qualifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_tqual_last_tok(ast_tqual_t *tqual)
{
	return &tqual->tqual;
}

/** Create AST basic type specifier.
 *
 * @param rtsbasic Place to store pointer to new basic type specifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsbasic_create(ast_tsbasic_t **rtsbasic)
{
	ast_tsbasic_t *tsbasic;

	tsbasic = calloc(1, sizeof(ast_tsbasic_t));
	if (tsbasic == NULL)
		return ENOMEM;

	tsbasic->node.ext = tsbasic;
	tsbasic->node.ntype = ant_tsbasic;

	*rtsbasic = tsbasic;
	return EOK;
}

/** Print AST basic type specifier.
 *
 * @param tsbasic Basic type specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tsbasic_print(ast_tsbasic_t *tsbasic, FILE *f)
{
	(void)tsbasic;
	if (fprintf(f, "tsbasic(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST basic type specifier.
 *
 * @param tsbasic Basic type specifier
 */
static void ast_tsbasic_destroy(ast_tsbasic_t *tsbasic)
{
	free(tsbasic);
}

/** Get first token of AST basic type specifier.
 *
 * @param tsbasic Basic type specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_tsbasic_first_tok(ast_tsbasic_t *tsbasic)
{
	return &tsbasic->tbasic;
}

/** Get last token of AST basic type specifier.
 *
 * @param tsbasic Basic type specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_tsbasic_last_tok(ast_tsbasic_t *tsbasic)
{
	return &tsbasic->tbasic;
}

/** Create AST identifier type specifier.
 *
 * @param rtsident Place to store pointer to new identifier type specifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsident_create(ast_tsident_t **rtsident)
{
	ast_tsident_t *atsident;

	atsident = calloc(1, sizeof(ast_tsident_t));
	if (atsident == NULL)
		return ENOMEM;

	atsident->node.ext = atsident;
	atsident->node.ntype = ant_tsident;

	*rtsident = atsident;
	return EOK;
}

/** Print AST identifier type specifier.
 *
 * @param atsident Identifier specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tsident_print(ast_tsident_t *atsident, FILE *f)
{
	(void)atsident;
	if (fprintf(f, "tsident(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST identifier type specifier.
 *
 * @param atsident Identifier specifier
 */
static void ast_tsident_destroy(ast_tsident_t *atsident)
{
	free(atsident);
}

/** Get first token of AST identifier type specifier.
 *
 * @param tsident Identifier type specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_tsident_first_tok(ast_tsident_t *tsident)
{
	return &tsident->tident;
}

/** Get last token of AST identifier type specifier.
 *
 * @param tsident Identifier type specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_tsident_last_tok(ast_tsident_t *tsident)
{
	return &tsident->tident;
}

/** Create AST record type specifier.
 *
 * @param rtype Record type (struct or union)
 * @param rtsrecord Place to store pointer to new record type specifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsrecord_create(ast_rtype_t rtype, ast_tsrecord_t **rtsrecord)
{
	ast_tsrecord_t *tsrecord;

	tsrecord = calloc(1, sizeof(ast_tsrecord_t));
	if (tsrecord == NULL)
		return ENOMEM;

	tsrecord->rtype = rtype;
	list_initialize(&tsrecord->elems);

	tsrecord->node.ext = tsrecord;
	tsrecord->node.ntype = ant_tsrecord;

	*rtsrecord = tsrecord;
	return EOK;
}

/** Append element to record type specifier.
 *
 * @param tsrecord Record type specifier
 * @param sqlist Specifier-qualifier list
 * @param dlist Declarator list
 * @param stmt Statement
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsrecord_append(ast_tsrecord_t *tsrecord, ast_sqlist_t *sqlist,
    ast_dlist_t *dlist, void *dscolon)
{
	ast_tsrecord_elem_t *elem;

	elem = calloc(1, sizeof(ast_tsrecord_elem_t));
	if (elem == NULL)
		return ENOMEM;

	elem->sqlist = sqlist;
	elem->dlist = dlist;
	elem->tscolon.data = dscolon;

	elem->tsrecord = tsrecord;
	list_append(&elem->ltsrecord, &tsrecord->elems);
	return EOK;
}

/** Return first element in record type specifier.
 *
 * @param tsrecord Record type specifier
 * @return First element or @c NULL
 */
ast_tsrecord_elem_t *ast_tsrecord_first(ast_tsrecord_t *tsrecord)
{
	link_t *link;

	link = list_first(&tsrecord->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_tsrecord_elem_t, ltsrecord);
}

/** Return next element in record type specifier.
 *
 * @param elem Current element
 * @return Next element or @c NULL
 */
ast_tsrecord_elem_t *ast_tsrecord_next(ast_tsrecord_elem_t *elem)
{
	link_t *link;

	link = list_next(&elem->ltsrecord, &elem->tsrecord->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_tsrecord_elem_t, ltsrecord);
}

/** Print AST record type specifier.
 *
 * @param tsrecord Record type specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tsrecord_print(ast_tsrecord_t *tsrecord, FILE *f)
{
	ast_tsrecord_elem_t *elem;
	int rc;

	if (fprintf(f, "tsrecord(%s ", tsrecord->rtype == ar_struct ? "struct" :
	    "union") < 0)
		return EIO;

	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		rc = ast_tree_print(&elem->sqlist->node, f);
		if (rc != EOK)
			return rc;

		elem = ast_tsrecord_next(elem);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST record type specifier.
 *
 * @param tsrecord Record type specifier
 */
static void ast_tsrecord_destroy(ast_tsrecord_t *tsrecord)
{
	ast_tsrecord_elem_t *elem;

	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		list_remove(&elem->ltsrecord);
		ast_sqlist_destroy(elem->sqlist);
		ast_dlist_destroy(elem->dlist);
		free(elem);
		elem = ast_tsrecord_first(tsrecord);
	}

	free(tsrecord);
}

/** Get first token of AST record type specifier.
 *
 * @param tsrecord Record type specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_tsrecord_first_tok(ast_tsrecord_t *tsrecord)
{
	return &tsrecord->tsu;
}

/** Get last token of AST record type specifier.
 *
 * @param tsrecord Record type specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_tsrecord_last_tok(ast_tsrecord_t *tsrecord)
{
	if (tsrecord->have_def)
		return &tsrecord->trbrace;
	else if (tsrecord->have_ident)
		return &tsrecord->tident;
	else
		return &tsrecord->tsu;
}

/** Create AST enum type specifier.
 *
 * @param rtsenum Place to store pointer to new enum type specifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsenum_create(ast_tsenum_t **rtsenum)
{
	ast_tsenum_t *tsenum;

	tsenum = calloc(1, sizeof(ast_tsenum_t));
	if (tsenum == NULL)
		return ENOMEM;

	list_initialize(&tsenum->elems);

	tsenum->node.ext = tsenum;
	tsenum->node.ntype = ant_tsenum;

	*rtsenum = tsenum;
	return EOK;
}

/** Append element to enum type specifier.
 *
 * @param tsenum Enum type specifier
 * @param dident Data for identifier token
 * @param dequals Data for equals token or @c NULL
 * @param dinit Data for initializer token or @c NULL
 * @param dcomma Data for comma token or @c NULL
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsenum_append(ast_tsenum_t *tsenum, void *dident, void *dequals,
    void *dinit, void *dcomma)
{
	ast_tsenum_elem_t *elem;

	elem = calloc(1, sizeof(ast_tsenum_elem_t));
	if (elem == NULL)
		return ENOMEM;

	elem->tident.data = dident;
	elem->tequals.data = dequals;
	elem->tinit.data = dinit;
	elem->tcomma.data = dcomma;

	elem->tsenum = tsenum;
	list_append(&elem->ltsenum, &tsenum->elems);
	return EOK;
}

/** Return first element in record type specifier.
 *
 * @param tsenum Record type specifier
 * @return First element or @c NULL
 */
ast_tsenum_elem_t *ast_tsenum_first(ast_tsenum_t *tsenum)
{
	link_t *link;

	link = list_first(&tsenum->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_tsenum_elem_t, ltsenum);
}

/** Return next element in record type specifier.
 *
 * @param elem Current element
 * @return Next element or @c NULL
 */
ast_tsenum_elem_t *ast_tsenum_next(ast_tsenum_elem_t *elem)
{
	link_t *link;

	link = list_next(&elem->ltsenum, &elem->tsenum->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_tsenum_elem_t, ltsenum);
}

/** Print AST enum type specifier.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tsenum_print(ast_tsenum_t *tsenum, FILE *f)
{
	ast_tsenum_elem_t *elem;

	if (fprintf(f, "tsenum(") < 0)
		return EIO;

	elem = ast_tsenum_first(tsenum);
	while (elem != NULL) {
		if (fprintf(f, "elem") < 0)
			return EIO;

		elem = ast_tsenum_next(elem);

		if (elem != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST enum type specifier.
 *
 * @param block Block
 */
static void ast_tsenum_destroy(ast_tsenum_t *tsenum)
{
	ast_tsenum_elem_t *elem;

	elem = ast_tsenum_first(tsenum);
	while (elem != NULL) {
		list_remove(&elem->ltsenum);
		free(elem);
		elem = ast_tsenum_first(tsenum);
	}

	free(tsenum);
}

/** Get first token of AST enum type specifier.
 *
 * @param tsenum enum type specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_tsenum_first_tok(ast_tsenum_t *tsenum)
{
	return &tsenum->tenum;
}

/** Get last token of AST enum type specifier.
 *
 * @param tsenum enum type specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_tsenum_last_tok(ast_tsenum_t *tsenum)
{
	if (tsenum->have_def)
		return &tsenum->trbrace;
	else if (tsenum->have_ident)
		return &tsenum->tident;
	else
		return &tsenum->tenum;
}

/** Create AST function specifier.
 *
 * @param rtsbasic Place to store pointer to new function specifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_fspec_create(ast_fspec_t **rfspec)
{
	ast_fspec_t *fspec;

	fspec = calloc(1, sizeof(ast_fspec_t));
	if (fspec == NULL)
		return ENOMEM;

	fspec->node.ext = fspec;
	fspec->node.ntype = ant_fspec;

	*rfspec = fspec;
	return EOK;
}

/** Print AST function specifier.
 *
 * @param fspec Function specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_fspec_print(ast_fspec_t *fspec, FILE *f)
{
	(void) fspec;
	if (fprintf(f, "fspec") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST function specifier.
 *
 * @param fspec Function specifier
 */
static void ast_fspec_destroy(ast_fspec_t *fspec)
{
	free(fspec);
}

/** Get first token of AST function specifier.
 *
 * @param fspec Function specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_fspec_first_tok(ast_fspec_t *fspec)
{
	return &fspec->tfspec;
}

/** Get last token of AST function specifier.
 *
 * @param fspec Function specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_fspec_last_tok(ast_fspec_t *fspec)
{
	return &fspec->tfspec;
}

/** Create AST specifier-qualifier list.
 *
 * @param rsqlist Place to store pointer to new specifier-qualifier list
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_sqlist_create(ast_sqlist_t **rsqlist)
{
	ast_sqlist_t *sqlist;

	sqlist = calloc(1, sizeof(ast_sqlist_t));
	if (sqlist == NULL)
		return ENOMEM;

	list_initialize(&sqlist->elems);

	sqlist->node.ext = sqlist;
	sqlist->node.ntype = ant_sqlist;

	*rsqlist = sqlist;
	return EOK;
}

/** Append element to specifier-qualifier list.
 *
 * @param sqlist Specifier-qualifier list
 * @param elem Specifier or qualifier
 */
void ast_sqlist_append(ast_sqlist_t *sqlist, ast_node_t *elem)
{
	list_append(&elem->llist, &sqlist->elems);
	elem->lnode = &sqlist->node;
}

/** Return first element in specifier-qualifier list.
 *
 * @param sqlist Specifier-qualifier list
 * @return First element or @c NULL
 */
ast_node_t *ast_sqlist_first(ast_sqlist_t *sqlist)
{
	link_t *link;

	link = list_first(&sqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return next element in specifier-qualifier list.
 *
 * @param node Current element
 * @return Next element or @c NULL
 */
ast_node_t *ast_sqlist_next(ast_node_t *node)
{
	link_t *link;
	ast_sqlist_t *sqlist;

	assert(node->lnode->ntype == ant_sqlist);
	sqlist = (ast_sqlist_t *) node->lnode->ext;

	link = list_next(&node->llist, &sqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return last element in specifier-qualifier list.
 *
 * @param sqlist Specifier-qualifier list
 * @return Last element or @c NULL
 */
ast_node_t *ast_sqlist_last(ast_sqlist_t *sqlist)
{
	link_t *link;

	link = list_last(&sqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return previous element in specifier-qualifier list.
 *
 * @param node Current element
 * @return Previous element or @c NULL
 */
ast_node_t *ast_sqlist_prev(ast_node_t *node)
{
	link_t *link;
	ast_sqlist_t *sqlist;

	assert(node->lnode->ntype == ant_sqlist);
	sqlist = (ast_sqlist_t *) node->lnode->ext;

	link = list_prev(&node->llist, &sqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Print AST specifier-qualifier list.
 *
 * @param sqlist Specifier-qualifier list
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_sqlist_print(ast_sqlist_t *sqlist, FILE *f)
{
	ast_node_t *elem;
	int rc;

	if (fprintf(f, "sqlist(") < 0)
		return EIO;

	elem = ast_sqlist_first(sqlist);
	while (elem != NULL) {
		rc = ast_tree_print(elem, f);
		if (rc != EOK)
			return rc;

		elem = ast_sqlist_next(elem);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST specifier-qualifier list.
 *
 * @param sqlist Specifier-qualifier list
 */
static void ast_sqlist_destroy(ast_sqlist_t *sqlist)
{
	ast_node_t *elem;

	elem = ast_sqlist_first(sqlist);
	while (elem != NULL) {
		list_remove(&elem->llist);
		ast_tree_destroy(elem);
		elem = ast_sqlist_first(sqlist);
	}

	free(sqlist);
}

/** Get first token of AST specifier-qualifier list.
 *
 * @param sqlist Specifier-qualifier list
 * @return First token or @c NULL
 */
static ast_tok_t *ast_sqlist_first_tok(ast_sqlist_t *sqlist)
{
	return ast_tree_first_tok(ast_sqlist_first(sqlist));
}

/** Get last token of AST specifier-qualifier list.
 *
 * @param sqlist Specifier-qualifier list
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_sqlist_last_tok(ast_sqlist_t *sqlist)
{
	return ast_tree_last_tok(ast_sqlist_last(sqlist));
}

/** Create AST declaration specifiers.
 *
 * @param rdspecs Place to store pointer to new declaration specifiers
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dspecs_create(ast_dspecs_t **rdspecs)
{
	ast_dspecs_t *dspecs;

	dspecs = calloc(1, sizeof(ast_dspecs_t));
	if (dspecs == NULL)
		return ENOMEM;

	list_initialize(&dspecs->dspecs);

	dspecs->node.ext = dspecs;
	dspecs->node.ntype = ant_dspecs;

	*rdspecs = dspecs;
	return EOK;
}

/** Append element to declaration specifiers.
 *
 * @param dspecs Declaration specifiers
 * @param dspec Declaration specifier
 */
void ast_dspecs_append(ast_dspecs_t *dspecs, ast_node_t *dspec)
{
	list_append(&dspec->llist, &dspecs->dspecs);
	dspec->lnode = &dspecs->node;
}

/** Return first element in declaration specifiers.
 *
 * @param dspecs Declaration specifiers
 * @return First element or @c NULL
 */
ast_node_t *ast_dspecs_first(ast_dspecs_t *dspecs)
{
	link_t *link;

	link = list_first(&dspecs->dspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return next element in declaration specifiers.
 *
 * @param node Current element
 * @return Next element or @c NULL
 */
ast_node_t *ast_dspecs_next(ast_node_t *node)
{
	link_t *link;
	ast_dspecs_t *dspecs;

	assert(node->lnode->ntype == ant_dspecs);
	dspecs = (ast_dspecs_t *) node->lnode->ext;

	link = list_next(&node->llist, &dspecs->dspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return last element in declaration specifiers.
 *
 * @param dspecs Declaration specifiers
 * @return Last element or @c NULL
 */
ast_node_t *ast_dspecs_last(ast_dspecs_t *dspecs)
{
	link_t *link;

	link = list_last(&dspecs->dspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return previous element in declaration specifiers.
 *
 * @param node Current element
 * @return Previous element or @c NULL
 */
ast_node_t *ast_dspecs_prev(ast_node_t *node)
{
	link_t *link;
	ast_dspecs_t *dspecs;

	assert(node->lnode->ntype == ant_dspecs);
	dspecs = (ast_dspecs_t *) node->lnode->ext;

	link = list_prev(&node->llist, &dspecs->dspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Print AST declaration specifiers.
 *
 * @param dspecs Declaration specifiers
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dspecs_print(ast_dspecs_t *dspecs, FILE *f)
{
	ast_node_t *elem;
	int rc;

	if (fprintf(f, "dspecs(") < 0)
		return EIO;

	elem = ast_dspecs_first(dspecs);
	while (elem != NULL) {
		rc = ast_tree_print(elem, f);
		if (rc != EOK)
			return rc;

		elem = ast_dspecs_next(elem);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST declaration specifiers.
 *
 * @param dspecs Declaration specifiers
 */
static void ast_dspecs_destroy(ast_dspecs_t *dspecs)
{
	ast_node_t *elem;

	elem = ast_dspecs_first(dspecs);
	while (elem != NULL) {
		list_remove(&elem->llist);
		ast_tree_destroy(elem);
		elem = ast_dspecs_first(dspecs);
	}

	free(dspecs);
}

/** Get first token of AST declaration specifiers.
 *
 * @param dspecs Declaration specifiers
 * @return First token or @c NULL
 */
static ast_tok_t *ast_dspecs_first_tok(ast_dspecs_t *dspecs)
{
	return ast_tree_first_tok(ast_dspecs_first(dspecs));
}

/** Get last token of AST declaration specifiers.
 *
 * @param dspecs Declaration specifiers
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_dspecs_last_tok(ast_dspecs_t *dspecs)
{
	return ast_tree_last_tok(ast_dspecs_last(dspecs));
}


/** Create AST identifier declarator.
 *
 * @param rdident Place to store pointer to new identifier declarator
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dident_create(ast_dident_t **rdident)
{
	ast_dident_t *adident;

	adident = calloc(1, sizeof(ast_dident_t));
	if (adident == NULL)
		return ENOMEM;

	adident->node.ext = adident;
	adident->node.ntype = ant_dident;

	*rdident = adident;
	return EOK;
}

/** Print AST identifier declarator.
 *
 * @param adident Identifier specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dident_print(ast_dident_t *adident, FILE *f)
{
	(void)adident;
	if (fprintf(f, "dident(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST identifier declarator.
 *
 * @param adident Identifier specifier
 */
static void ast_dident_destroy(ast_dident_t *adident)
{
	free(adident);
}

/** Get first token of AST identifier declarator.
 *
 * @param dident Identifier declarator
 * @return First token or @c NULL
 */
static ast_tok_t *ast_dident_first_tok(ast_dident_t *dident)
{
	return &dident->tident;
}

/** Get last token of AST identifier declarator.
 *
 * @param dident Identifier declarator
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_dident_last_tok(ast_dident_t *dident)
{
	return &dident->tident;
}

/** Create AST no-identifier declarator.
 *
 * @param rdnoident Place to store pointer to new no-identifier declarator
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dnoident_create(ast_dnoident_t **rdnoident)
{
	ast_dnoident_t *adnoident;

	adnoident = calloc(1, sizeof(ast_dnoident_t));
	if (adnoident == NULL)
		return ENOMEM;

	adnoident->node.ext = adnoident;
	adnoident->node.ntype = ant_dnoident;

	*rdnoident = adnoident;
	return EOK;
}

/** Print AST no-identifier declarator.
 *
 * @param adnoident Identifier specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dnoident_print(ast_dnoident_t *adnoident, FILE *f)
{
	(void)adnoident;
	if (fprintf(f, "dnoident(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST no-identifier declarator.
 *
 * @param adnoident Identifier specifier
 */
static void ast_dnoident_destroy(ast_dnoident_t *adnoident)
{
	free(adnoident);
}

/** Get first token of AST no-identifier declarator.
 *
 * @param dnoident No-identifier declarator
 * @return First token or @c NULL
 */
static ast_tok_t *ast_dnoident_first_tok(ast_dnoident_t *dnoident)
{
	(void) dnoident;
	return NULL;
}

/** Get last token of AST no-identifier declarator.
 *
 * @param dnoident No-identifier declarator
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_dnoident_last_tok(ast_dnoident_t *dnoident)
{
	(void) dnoident;
	return NULL;
}

/** Create AST parenthesized declarator.
 *
 * @param rdparen Place to store pointer to new parenthesized declarator
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dparen_create(ast_dparen_t **rdparen)
{
	ast_dparen_t *adparen;

	adparen = calloc(1, sizeof(ast_dparen_t));
	if (adparen == NULL)
		return ENOMEM;

	adparen->node.ext = adparen;
	adparen->node.ntype = ant_dparen;

	*rdparen = adparen;
	return EOK;
}

/** Print AST parenthesized declarator.
 *
 * @param adparen Identifier specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dparen_print(ast_dparen_t *adparen, FILE *f)
{
	int rc;

	if (fprintf(f, "dparen(") < 0)
		return EIO;
	rc = ast_tree_print(adparen->bdecl, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST parenthesized declarator.
 *
 * @param adparen Identifier specifier
 */
static void ast_dparen_destroy(ast_dparen_t *adparen)
{
	ast_tree_destroy(adparen->bdecl);
	free(adparen);
}

/** Get first token of AST parenthesized declarator.
 *
 * @param dparen Parenthesized declarator
 * @return First token or @c NULL
 */
static ast_tok_t *ast_dparen_first_tok(ast_dparen_t *dparen)
{
	return &dparen->tlparen;
}

/** Get last token of AST parenthesized declarator.
 *
 * @param dparen Parenthesized declarator
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_dparen_last_tok(ast_dparen_t *dparen)
{
	return &dparen->trparen;
}

/** Create AST pointer declarator.
 *
 * @param rdptr Place to store pointer to new pointer declarator
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dptr_create(ast_dptr_t **rdptr)
{
	ast_dptr_t *adptr;

	adptr = calloc(1, sizeof(ast_dptr_t));
	if (adptr == NULL)
		return ENOMEM;

	adptr->node.ext = adptr;
	adptr->node.ntype = ant_dptr;

	*rdptr = adptr;
	return EOK;
}

/** Print AST pointer declarator.
 *
 * @param adptr pointer declarator
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dptr_print(ast_dptr_t *adptr, FILE *f)
{
	int rc;

	if (fprintf(f, "dptr(") < 0)
		return EIO;
	rc = ast_tree_print(adptr->bdecl, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST pointer declarator.
 *
 * @param adptr pointer declarator
 */
static void ast_dptr_destroy(ast_dptr_t *adptr)
{
	ast_tree_destroy(adptr->bdecl);
	free(adptr);
}

/** Get first token of AST pointer declarator.
 *
 * @param dptr Pointer declarator
 * @return First token or @c NULL
 */
static ast_tok_t *ast_dptr_first_tok(ast_dptr_t *dptr)
{
	return &dptr->tasterisk;
}

/** Get last token of AST pointer declarator.
 *
 * @param dptr Pointer declarator
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_dptr_last_tok(ast_dptr_t *dptr)
{
	ast_tok_t *tok;

	tok = ast_tree_last_tok(dptr->bdecl);
	if (tok != NULL)
		return tok;

	return &dptr->tasterisk;
}

/** Create AST function declarator.
 *
 * @param rdfun Place to store pointer to new function declarator
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dfun_create(ast_dfun_t **rdfun)
{
	ast_dfun_t *dfun;

	dfun = calloc(1, sizeof(ast_dfun_t));
	if (dfun == NULL)
		return ENOMEM;

	list_initialize(&dfun->args);

	dfun->node.ext = dfun;
	dfun->node.ntype = ant_dfun;

	*rdfun = dfun;
	return EOK;
}

/** Append argument to function declarator.
 *
 * @param dfun Function declarator
 * @param dspecs Declaration specifiers
 * @param decl Argument declarator
 * @param dcomma Data for comma token or @c NULL
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dfun_append(ast_dfun_t *dfun, ast_dspecs_t *dspecs, ast_node_t *decl,
    void *dcomma)
{
	ast_dfun_arg_t *arg;

	arg = calloc(1, sizeof(ast_dfun_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->dspecs = dspecs;
	arg->decl = decl;
	arg->tcomma.data = dcomma;

	arg->dfun = dfun;
	list_append(&arg->ldfun, &dfun->args);
	return EOK;
}

/** Return first argument of function declarator.
 *
 * @param dfun Function declarator
 * @return First argument or @c NULL
 */
ast_dfun_arg_t *ast_dfun_first(ast_dfun_t *dfun)
{
	link_t *link;

	link = list_first(&dfun->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_dfun_arg_t, ldfun);
}

/** Return next argument of function declarator.
 *
 * @param arg Current argument
 * @return Next argument or @c NULL
 */
ast_dfun_arg_t *ast_dfun_next(ast_dfun_arg_t *arg)
{
	link_t *link;

	link = list_next(&arg->ldfun, &arg->dfun->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_dfun_arg_t, ldfun);
}

/** Print AST function declarator.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dfun_print(ast_dfun_t *dfun, FILE *f)
{
	ast_dfun_arg_t *arg;
	int rc;

	if (fprintf(f, "dfun(") < 0)
		return EIO;

	arg = ast_dfun_first(dfun);
	while (arg != NULL) {
		rc = ast_dspecs_print(arg->dspecs, f);
		if (rc != EOK)
			return rc;

		if (fprintf(f, " ") < 0)
			return EIO;

		rc = ast_tree_print(arg->decl, f);
		if (rc != EOK)
			return rc;

		arg = ast_dfun_next(arg);

		if (arg != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST function declarator.
 *
 * @param block Block
 */
static void ast_dfun_destroy(ast_dfun_t *dfun)
{
	ast_dfun_arg_t *arg;

	ast_tree_destroy(dfun->bdecl);

	arg = ast_dfun_first(dfun);
	while (arg != NULL) {
		list_remove(&arg->ldfun);
		ast_dspecs_destroy(arg->dspecs);
		ast_tree_destroy(arg->decl);
		free(arg);

		arg = ast_dfun_first(dfun);
	}

	free(dfun);
}

/** Get first token of AST function declarator.
 *
 * @param dfun Function declarator
 * @return First token or @c NULL
 */
static ast_tok_t *ast_dfun_first_tok(ast_dfun_t *dfun)
{
	ast_tok_t *tok;

	tok = ast_tree_first_tok(dfun->bdecl);
	if (tok != NULL)
		return tok;

	return &dfun->tlparen;
}

/** Get last token of AST function declarator.
 *
 * @param dfun Function declarator
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_dfun_last_tok(ast_dfun_t *dfun)
{
	return &dfun->trparen;
}

/** Create AST array declarator.
 *
 * @param rdarray Place to store pointer to new array declarator
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_darray_create(ast_darray_t **rdarray)
{
	ast_darray_t *darray;

	darray = calloc(1, sizeof(ast_darray_t));
	if (darray == NULL)
		return ENOMEM;

	darray->node.ext = darray;
	darray->node.ntype = ant_darray;

	*rdarray = darray;
	return EOK;
}

/** Print AST array declarator.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_darray_print(ast_darray_t *darray, FILE *f)
{
	int rc;

	if (fprintf(f, "darray(") < 0)
		return EIO;

	rc = ast_tree_print(darray->bdecl, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST array declarator.
 *
 * @param block Block
 */
static void ast_darray_destroy(ast_darray_t *darray)
{
	ast_tree_destroy(darray->bdecl);
	free(darray);
}

/** Get first token of AST array declarator.
 *
 * @param darray Array declarator
 * @return First token or @c NULL
 */
static ast_tok_t *ast_darray_first_tok(ast_darray_t *darray)
{
	ast_tok_t *tok;

	tok = ast_tree_first_tok(darray->bdecl);
	if (tok != NULL)
		return tok;

	return &darray->tlbracket;
}

/** Get last token of AST array declarator.
 *
 * @param darray Array declarator
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_darray_last_tok(ast_darray_t *darray)
{
	return &darray->trbracket;
}

/** Create AST declarator list.
 *
 * @param rdfun Place to store pointer to new declarator list
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dlist_create(ast_dlist_t **rdlist)
{
	ast_dlist_t *dlist;

	dlist = calloc(1, sizeof(ast_dlist_t));
	if (dlist == NULL)
		return ENOMEM;

	list_initialize(&dlist->decls);

	dlist->node.ext = dlist;
	dlist->node.ntype = ant_dlist;

	*rdlist = dlist;
	return EOK;
}

/** Append entry to declarator list.
 *
 * @param dlist Declarator list
 * @param dcomma Data for preceding comma token or @c NULL
 * @param decl Declarator
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dlist_append(ast_dlist_t *dlist, void *dcomma, ast_node_t *decl)
{
	ast_dlist_entry_t *arg;

	arg = calloc(1, sizeof(ast_dlist_entry_t));
	if (arg == NULL)
		return ENOMEM;

	arg->tcomma.data = dcomma;
	arg->decl = decl;

	arg->dlist = dlist;
	list_append(&arg->ldlist, &dlist->decls);
	return EOK;
}

/** Return first declarator list entry.
 *
 * @param dlist Declarator list
 * @return First entry or @c NULL
 */
ast_dlist_entry_t *ast_dlist_first(ast_dlist_t *dlist)
{
	link_t *link;

	link = list_first(&dlist->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_dlist_entry_t, ldlist);
}

/** Return next declarator list entry.
 *
 * @param arg Current entry
 * @return Next entry or @c NULL
 */
ast_dlist_entry_t *ast_dlist_next(ast_dlist_entry_t *arg)
{
	link_t *link;

	link = list_next(&arg->ldlist, &arg->dlist->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_dlist_entry_t, ldlist);
}

/** Return last declarator list entry.
 *
 * @param dlist Declarator list
 * @return Last entry or @c NULL
 */
ast_dlist_entry_t *ast_dlist_last(ast_dlist_t *dlist)
{
	link_t *link;

	link = list_last(&dlist->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_dlist_entry_t, ldlist);
}

/** Return previous declarator list entry.
 *
 * @param arg Current entry
 * @return Previous entry or @c NULL
 */
ast_dlist_entry_t *ast_dlist_prev(ast_dlist_entry_t *arg)
{
	link_t *link;

	link = list_prev(&arg->ldlist, &arg->dlist->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_dlist_entry_t, ldlist);
}

/** Print AST declarator list.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dlist_print(ast_dlist_t *dlist, FILE *f)
{
	ast_dlist_entry_t *entry;
	int rc;

	if (fprintf(f, "dlist(") < 0)
		return EIO;

	entry = ast_dlist_first(dlist);
	while (entry != NULL) {
		rc = ast_tree_print(entry->decl, f);
		if (rc != EOK)
			return EIO;

		entry = ast_dlist_next(entry);

		if (entry != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST declarator list.
 *
 * @param block Block
 */
static void ast_dlist_destroy(ast_dlist_t *dlist)
{
	ast_dlist_entry_t *entry;

	entry = ast_dlist_first(dlist);
	while (entry != NULL) {
		list_remove(&entry->ldlist);
		ast_tree_destroy(entry->decl);
		free(entry);

		entry = ast_dlist_first(dlist);
	}

	free(dlist);
}

/** Get first token of AST declarator list.
 *
 * @param dlist Declarator list
 * @return First token or @c NULL
 */
static ast_tok_t *ast_dlist_first_tok(ast_dlist_t *dlist)
{
	ast_dlist_entry_t *entry;
	ast_tok_t *tok;

	/* Try first token of first entry */
	entry = ast_dlist_first(dlist);
	tok = ast_tree_first_tok(entry->decl);
	if (tok != NULL)
		return tok;

	/* See if there are at least two entries */
	entry = ast_dlist_next(entry);
	if (entry == NULL)
		return NULL;

	/* Return separating comma */
	return &entry->tcomma;
}

/** Get last token of AST declarator list.
 *
 * @param dlist Declarator list
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_dlist_last_tok(ast_dlist_t *dlist)
{
	ast_dlist_entry_t *entry;
	ast_tok_t *tok;

	/* Try last token of last entry */
	entry = ast_dlist_last(dlist);
	tok = ast_tree_last_tok(entry->decl);
	if (tok != NULL)
		return tok;

	/* See if there are at least two entries */
	if (ast_dlist_prev(entry) == NULL)
		return NULL;

	/* Return separating comma */
	return &entry->tcomma;
}

bool ast_decl_is_abstract(ast_node_t *node)
{
	switch (node->ntype) {
	case ant_dident:
		return false;
	case ant_dnoident:
		return true;
	case ant_dparen:
		return ast_decl_is_abstract(((ast_dparen_t *)node->ext)->bdecl);
	case ant_dptr:
		return ast_decl_is_abstract(((ast_dptr_t *)node->ext)->bdecl);
	case ant_dfun:
		return ast_decl_is_abstract(((ast_dfun_t *)node->ext)->bdecl);
	case ant_darray:
		return ast_decl_is_abstract(((ast_darray_t *)node->ext)->bdecl);
	default:
		assert(false);
		return false;
	}
}

/** Create AST integer literal expression.
 *
 * @param reint Place to store pointer to new integer literal expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_eint_create(ast_eint_t **reint)
{
	ast_eint_t *eint;

	eint = calloc(1, sizeof(ast_eint_t));
	if (eint == NULL)
		return ENOMEM;

	eint->node.ext = eint;
	eint->node.ntype = ant_eint;

	*reint = eint;
	return EOK;
}

/** Print AST integer literal expression.
 *
 * @param eint Integer literal expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_eint_print(ast_eint_t *eint, FILE *f)
{
	(void) eint;

	if (fprintf(f, "eint(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST integer literal expression.
 *
 * @param eint Integer literal expression
 */
static void ast_eint_destroy(ast_eint_t *eint)
{
	free(eint);
}

/** Get first token of AST integer literal expression.
 *
 * @param eint Integer literal expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_eint_first_tok(ast_eint_t *eint)
{
	return &eint->tlit;
}

/** Get last token of AST integer literal expression.
 *
 * @param eint Integer literal expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_eint_last_tok(ast_eint_t *eint)
{
	return &eint->tlit;
}

/** Create AST character literal expression.
 *
 * @param rechar Place to store pointer to new character literal expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_echar_create(ast_echar_t **rechar)
{
	ast_echar_t *echar;

	echar = calloc(1, sizeof(ast_echar_t));
	if (echar == NULL)
		return ENOMEM;

	echar->node.ext = echar;
	echar->node.ntype = ant_echar;

	*rechar = echar;
	return EOK;
}

/** Print AST character literal expression.
 *
 * @param echar Character literal expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_echar_print(ast_echar_t *echar, FILE *f)
{
	(void) echar;

	if (fprintf(f, "echar(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST character literal expression.
 *
 * @param echar Character literal expression
 */
static void ast_echar_destroy(ast_echar_t *echar)
{
	free(echar);
}

/** Get first token of AST character literal expression.
 *
 * @param echar Character literal expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_echar_first_tok(ast_echar_t *echar)
{
	return &echar->tlit;
}

/** Get last token of AST character literal expression.
 *
 * @param echar Character literal expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_echar_last_tok(ast_echar_t *echar)
{
	return &echar->tlit;
}

/** Create AST string literal expression.
 *
 * @param restring Place to store pointer to new string literal expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_estring_create(ast_estring_t **restring)
{
	ast_estring_t *estring;

	estring = calloc(1, sizeof(ast_estring_t));
	if (estring == NULL)
		return ENOMEM;

	estring->node.ext = estring;
	estring->node.ntype = ant_estring;
	list_initialize(&estring->lits);

	*restring = estring;
	return EOK;
}

/** Append literal to string literal expression.
 *
 * @param estring String literal expression
 * @param dlit Data for string literal token
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_estring_append(ast_estring_t *estring, void *dlit)
{
	ast_estring_lit_t *lit;

	lit = calloc(1, sizeof(ast_estring_lit_t));
	if (lit == NULL)
		return ENOMEM;

	lit->tlit.data = dlit;
	lit->estring = estring;

	list_append(&lit->lstring, &estring->lits);
	return EOK;
}

/** Return first string literal in string literal expression.
 *
 * @param estring String literal expression
 * @return First literal or @c NULL
 */
ast_estring_lit_t *ast_estring_first(ast_estring_t *estring)
{
	link_t *link;

	link = list_first(&estring->lits);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_estring_lit_t, lstring);
}

/** Return next string literal in string literal expression.
 *
 * @param lit Current literal
 * @return Next literal or @c NULL
 */
ast_estring_lit_t *ast_estring_next(ast_estring_lit_t *lit)
{
	link_t *link;

	link = list_next(&lit->lstring, &lit->estring->lits);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_estring_lit_t, lstring);
}

/** Return last string literal in string literal expression.
 *
 * @param estring String literal expression
 * @return First literal or @c NULL
 */
ast_estring_lit_t *ast_estring_last(ast_estring_t *estring)
{
	link_t *link;

	link = list_last(&estring->lits);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_estring_lit_t, lstring);
}

/** Print AST string literal expression.
 *
 * @param estring String literal expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_estring_print(ast_estring_t *estring, FILE *f)
{
	ast_estring_lit_t *lit;

	if (fprintf(f, "estring(") < 0)
		return EIO;

	lit = ast_estring_first(estring);
	if (lit != NULL) {
		if (fprintf(f, "lit") < 0)
			return EIO;
	}

	lit = ast_estring_next(lit);
	while (lit != NULL) {
		if (fprintf(f, ", lit") < 0)
			return EIO;
		lit = ast_estring_next(lit);
	}

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST string literal expression.
 *
 * @param estring String literal expression
 */
static void ast_estring_destroy(ast_estring_t *estring)
{
	ast_estring_lit_t *lit;

	lit = ast_estring_first(estring);
	while (lit != NULL) {
		list_remove(&lit->lstring);
		free(lit);
		lit = ast_estring_first(estring);
	}

	free(estring);
}

/** Get first token of AST string literal expression.
 *
 * @param estring String literal expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_estring_first_tok(ast_estring_t *estring)
{
	ast_estring_lit_t *lit;

	lit = ast_estring_first(estring);
	return &lit->tlit;
}

/** Get last token of AST string literal expression.
 *
 * @param estring String literal expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_estring_last_tok(ast_estring_t *estring)
{
	ast_estring_lit_t *lit;

	lit = ast_estring_last(estring);
	return &lit->tlit;
}

/** Create AST identifier expression.
 *
 * @param reident Place to store pointer to new identifier expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_eident_create(ast_eident_t **reident)
{
	ast_eident_t *eident;

	eident = calloc(1, sizeof(ast_eident_t));
	if (eident == NULL)
		return ENOMEM;

	eident->node.ext = eident;
	eident->node.ntype = ant_eident;

	*reident = eident;
	return EOK;
}

/** Print AST identifier expression.
 *
 * @param eident Identifier expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_eident_print(ast_eident_t *eident, FILE *f)
{
	(void)eident;
	if (fprintf(f, "eident(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST identifier expression.
 *
 * @param eident Identifier expression
 */
static void ast_eident_destroy(ast_eident_t *eident)
{
	free(eident);
}

/** Get first token of AST identifier expression.
 *
 * @param eident Identifier expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_eident_first_tok(ast_eident_t *eident)
{
	return &eident->tident;
}

/** Get last token of AST identifier expression.
 *
 * @param eident Identifier expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_eident_last_tok(ast_eident_t *eident)
{
	return &eident->tident;
}

/** Create AST parenthesized expression.
 *
 * @param reparen Place to store pointer to new parenthesized expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_eparen_create(ast_eparen_t **reparen)
{
	ast_eparen_t *eparen;

	eparen = calloc(1, sizeof(ast_eparen_t));
	if (eparen == NULL)
		return ENOMEM;

	eparen->node.ext = eparen;
	eparen->node.ntype = ant_eparen;

	*reparen = eparen;
	return EOK;
}

/** Print AST parenthesized expression.
 *
 * @param eparen Parenthesized expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_eparen_print(ast_eparen_t *eparen, FILE *f)
{
	int rc;

	(void) eparen;

	if (fprintf(f, "eparen(") < 0)
		return EIO;

	rc = ast_tree_print(eparen->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST parenthesized expression.
 *
 * @param eparen Parenthesized expression
 */
static void ast_eparen_destroy(ast_eparen_t *eparen)
{
	ast_tree_destroy(eparen->bexpr);
	free(eparen);
}

/** Get first token of AST parenthesized expression.
 *
 * @param eparen Parenthesized expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_eparen_first_tok(ast_eparen_t *eparen)
{
	return &eparen->tlparen;
}

/** Get last token of AST parenthesized expression.
 *
 * @param eparen parenthesized expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_eparen_last_tok(ast_eparen_t *eparen)
{
	return &eparen->trparen;
}

/** Create AST binary operator expression.
 *
 * @param rebinop Place to store pointer to new binary operator expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ebinop_create(ast_ebinop_t **rebinop)
{
	ast_ebinop_t *ebinop;

	ebinop = calloc(1, sizeof(ast_ebinop_t));
	if (ebinop == NULL)
		return ENOMEM;

	ebinop->node.ext = ebinop;
	ebinop->node.ntype = ant_ebinop;

	*rebinop = ebinop;
	return EOK;
}

/** Print AST binary operator expression.
 *
 * @param ebinop Binary operator expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_ebinop_print(ast_ebinop_t *ebinop, FILE *f)
{
	int rc;

	(void) ebinop;

	if (fprintf(f, "ebinop(") < 0)
		return EIO;

	rc = ast_tree_print(ebinop->larg, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ",") < 0)
		return EIO;

	rc = ast_tree_print(ebinop->rarg, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST binary operator expression.
 *
 * @param ebinop Binary operator expression
 */
static void ast_ebinop_destroy(ast_ebinop_t *ebinop)
{
	ast_tree_destroy(ebinop->larg);
	ast_tree_destroy(ebinop->rarg);
	free(ebinop);
}

/** Get first token of AST binary operator expression.
 *
 * @param ebinop Binary operator expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_ebinop_first_tok(ast_ebinop_t *ebinop)
{
	return ast_tree_first_tok(ebinop->larg);
}

/** Get last token of AST binary operator expression.
 *
 * @param ebinop Binary operator expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ebinop_last_tok(ast_ebinop_t *ebinop)
{
	return ast_tree_last_tok(ebinop->rarg);
}

/** Create AST ternary conditional expression.
 *
 * @param retcond Place to store pointer to new ternary conditional expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_etcond_create(ast_etcond_t **retcond)
{
	ast_etcond_t *etcond;

	etcond = calloc(1, sizeof(ast_etcond_t));
	if (etcond == NULL)
		return ENOMEM;

	etcond->node.ext = etcond;
	etcond->node.ntype = ant_etcond;

	*retcond = etcond;
	return EOK;
}

/** Print AST ternary conditional expression.
 *
 * @param etcond Ternary conditional expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_etcond_print(ast_etcond_t *etcond, FILE *f)
{
	int rc;

	(void) etcond;

	if (fprintf(f, "etcond(") < 0)
		return EIO;

	rc = ast_tree_print(etcond->cond, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ",") < 0)
		return EIO;

	rc = ast_tree_print(etcond->targ, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ",") < 0)
		return EIO;

	rc = ast_tree_print(etcond->farg, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST ternary conditional expression.
 *
 * @param etcond Ternary conditional expression
 */
static void ast_etcond_destroy(ast_etcond_t *etcond)
{
	ast_tree_destroy(etcond->cond);
	ast_tree_destroy(etcond->targ);
	ast_tree_destroy(etcond->farg);
	free(etcond);
}

/** Get first token of AST ternary conditional expression.
 *
 * @param etcond Ternary conditional expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_etcond_first_tok(ast_etcond_t *etcond)
{
	return ast_tree_first_tok(etcond->cond);
}

/** Get last token of AST ternary conditional expression.
 *
 * @param etcond Ternary conditional expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_etcond_last_tok(ast_etcond_t *etcond)
{
	return ast_tree_last_tok(etcond->farg);
}

/** Create AST comma expression.
 *
 * @param recomma Place to store pointer to new comma expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ecomma_create(ast_ecomma_t **recomma)
{
	ast_ecomma_t *ecomma;

	ecomma = calloc(1, sizeof(ast_ecomma_t));
	if (ecomma == NULL)
		return ENOMEM;

	ecomma->node.ext = ecomma;
	ecomma->node.ntype = ant_ecomma;

	*recomma = ecomma;
	return EOK;
}

/** Print AST comma expression.
 *
 * @param ecomma Comma expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_ecomma_print(ast_ecomma_t *ecomma, FILE *f)
{
	int rc;

	(void) ecomma;

	if (fprintf(f, "ecomma(") < 0)
		return EIO;

	rc = ast_tree_print(ecomma->larg, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ",") < 0)
		return EIO;

	rc = ast_tree_print(ecomma->rarg, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST comma expression.
 *
 * @param ecomma Comma expression
 */
static void ast_ecomma_destroy(ast_ecomma_t *ecomma)
{
	ast_tree_destroy(ecomma->larg);
	ast_tree_destroy(ecomma->rarg);
	free(ecomma);
}

/** Get first token of AST comma expression.
 *
 * @param ecomma Comma expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_ecomma_first_tok(ast_ecomma_t *ecomma)
{
	return ast_tree_first_tok(ecomma->larg);
}

/** Get last token of AST comma expression.
 *
 * @param ecomma Comma expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ecomma_last_tok(ast_ecomma_t *ecomma)
{
	return ast_tree_last_tok(ecomma->rarg);
}

/** Create AST function call expression.
 *
 * @param refuncall Place to store pointer to new function call expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_efuncall_create(ast_efuncall_t **refuncall)
{
	ast_efuncall_t *efuncall;

	efuncall = calloc(1, sizeof(ast_efuncall_t));
	if (efuncall == NULL)
		return ENOMEM;

	efuncall->node.ext = efuncall;
	efuncall->node.ntype = ant_efuncall;
	list_initialize(&efuncall->args);

	*refuncall = efuncall;
	return EOK;
}

/** Append entry to function call argument list.
 *
 * @param efuncall Function call expression
 * @param dcomma Data for preceding comma token or @c NULL
 * @param expr Argument expression
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_efuncall_append(ast_efuncall_t *efuncall, void *dcomma,
    ast_node_t *expr)
{
	ast_efuncall_arg_t *arg;

	arg = calloc(1, sizeof(ast_efuncall_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->tcomma.data = dcomma;
	arg->expr = expr;

	arg->efuncall = efuncall;
	list_append(&arg->lfuncall, &efuncall->args);
	return EOK;
}

/** Return first argument in function call expression.
 *
 * @param efuncall Function call expression
 * @return First argument or @c NULL
 */
ast_efuncall_arg_t *ast_efuncall_first(ast_efuncall_t *efuncall)
{
	link_t *link;

	link = list_first(&efuncall->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_efuncall_arg_t, lfuncall);
}

/** Return next argument in function call expression.
 *
 * @param arg Function call argument
 * @return Next argument or @c NULL
 */
ast_efuncall_arg_t *ast_efuncall_next(ast_efuncall_arg_t *arg)
{
	link_t *link;

	link = list_next(&arg->lfuncall, &arg->efuncall->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_efuncall_arg_t, lfuncall);
}

/** Print AST function call expression.
 *
 * @param efuncall Function call expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_efuncall_print(ast_efuncall_t *efuncall, FILE *f)
{
	int rc;

	(void) efuncall;

	if (fprintf(f, "efuncall(") < 0)
		return EIO;

	rc = ast_tree_print(efuncall->fexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST function call expression.
 *
 * @param efuncall Function call expression
 */
static void ast_efuncall_destroy(ast_efuncall_t *efuncall)
{
	ast_efuncall_arg_t *arg;

	ast_tree_destroy(efuncall->fexpr);

	arg = ast_efuncall_first(efuncall);
	while (arg != NULL) {
		list_remove(&arg->lfuncall);
		ast_tree_destroy(arg->expr);
		free(arg);

		arg = ast_efuncall_first(efuncall);
	}

	free(efuncall);
}

/** Get first token of AST function call expression.
 *
 * @param efuncall Function call expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_efuncall_first_tok(ast_efuncall_t *efuncall)
{
	return ast_tree_first_tok(efuncall->fexpr);
}

/** Get last token of AST function call expression.
 *
 * @param efuncall Function call expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_efuncall_last_tok(ast_efuncall_t *efuncall)
{
	return &efuncall->trparen;
}

/** Create AST index expression.
 *
 * @param reindex Place to store pointer to new index expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_eindex_create(ast_eindex_t **reindex)
{
	ast_eindex_t *eindex;

	eindex = calloc(1, sizeof(ast_eindex_t));
	if (eindex == NULL)
		return ENOMEM;

	eindex->node.ext = eindex;
	eindex->node.ntype = ant_eindex;

	*reindex = eindex;
	return EOK;
}

/** Print AST index expression.
 *
 * @param eindex Index expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_eindex_print(ast_eindex_t *eindex, FILE *f)
{
	int rc;

	(void) eindex;

	if (fprintf(f, "eindex(") < 0)
		return EIO;

	rc = ast_tree_print(eindex->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ",") < 0)
		return EIO;

	rc = ast_tree_print(eindex->iexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST index expression.
 *
 * @param eindex Index expression
 */
static void ast_eindex_destroy(ast_eindex_t *eindex)
{
	ast_tree_destroy(eindex->bexpr);
	ast_tree_destroy(eindex->iexpr);
	free(eindex);
}

/** Get first token of AST index expression.
 *
 * @param eindex Index expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_eindex_first_tok(ast_eindex_t *eindex)
{
	return ast_tree_first_tok(eindex->bexpr);
}

/** Get last token of AST index expression.
 *
 * @param eindex Index expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_eindex_last_tok(ast_eindex_t *eindex)
{
	return &eindex->trbracket;
}

/** Create AST dereference expression.
 *
 * @param rederef Place to store pointer to new dereference expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ederef_create(ast_ederef_t **rederef)
{
	ast_ederef_t *ederef;

	ederef = calloc(1, sizeof(ast_ederef_t));
	if (ederef == NULL)
		return ENOMEM;

	ederef->node.ext = ederef;
	ederef->node.ntype = ant_ederef;

	*rederef = ederef;
	return EOK;
}

/** Print AST dereference expression.
 *
 * @param ederef Dereference expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_ederef_print(ast_ederef_t *ederef, FILE *f)
{
	int rc;

	(void) ederef;

	if (fprintf(f, "ederef(") < 0)
		return EIO;

	rc = ast_tree_print(ederef->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST dereference expression.
 *
 * @param ederef Dereference expression
 */
static void ast_ederef_destroy(ast_ederef_t *ederef)
{
	ast_tree_destroy(ederef->bexpr);
	free(ederef);
}

/** Get first token of AST dereference expression.
 *
 * @param ederef Dereference expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_ederef_first_tok(ast_ederef_t *ederef)
{
	return &ederef->tasterisk;
}

/** Get last token of AST dereference expression.
 *
 * @param ederef Dereference expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ederef_last_tok(ast_ederef_t *ederef)
{
	return ast_tree_last_tok(ederef->bexpr);
}

/** Create AST address expression.
 *
 * @param readdr Place to store pointer to new address expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_eaddr_create(ast_eaddr_t **readdr)
{
	ast_eaddr_t *eaddr;

	eaddr = calloc(1, sizeof(ast_eaddr_t));
	if (eaddr == NULL)
		return ENOMEM;

	eaddr->node.ext = eaddr;
	eaddr->node.ntype = ant_eaddr;

	*readdr = eaddr;
	return EOK;
}

/** Print AST address expression.
 *
 * @param eaddr Sizeof expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_eaddr_print(ast_eaddr_t *eaddr, FILE *f)
{
	int rc;

	(void) eaddr;

	if (fprintf(f, "eaddr(") < 0)
		return EIO;

	rc = ast_tree_print(eaddr->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST address expression.
 *
 * @param eaddr Sizeof expression
 */
static void ast_eaddr_destroy(ast_eaddr_t *eaddr)
{
	ast_tree_destroy(eaddr->bexpr);
	free(eaddr);
}

/** Get first token of AST address expression.
 *
 * @param eaddr Dereference expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_eaddr_first_tok(ast_eaddr_t *eaddr)
{
	return &eaddr->tamper;
}

/** Get last token of AST address expression.
 *
 * @param eaddr Address expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_eaddr_last_tok(ast_eaddr_t *eaddr)
{
	return ast_tree_last_tok(eaddr->bexpr);
}

/** Create AST sizeof expression.
 *
 * @param resizeof Place to store pointer to new sizeof expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_esizeof_create(ast_esizeof_t **resizeof)
{
	ast_esizeof_t *esizeof;

	esizeof = calloc(1, sizeof(ast_esizeof_t));
	if (esizeof == NULL)
		return ENOMEM;

	esizeof->node.ext = esizeof;
	esizeof->node.ntype = ant_esizeof;

	*resizeof = esizeof;
	return EOK;
}

/** Print AST sizeof expression.
 *
 * @param esizeof Sizeof expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_esizeof_print(ast_esizeof_t *esizeof, FILE *f)
{
	int rc;

	(void) esizeof;

	if (fprintf(f, "esizeof(") < 0)
		return EIO;

	rc = ast_tree_print(esizeof->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST sizeof expression.
 *
 * @param esizeof Sizeof expression
 */
static void ast_esizeof_destroy(ast_esizeof_t *esizeof)
{
	ast_tree_destroy(esizeof->bexpr);
	free(esizeof);
}

/** Get first token of AST sizeof expression.
 *
 * @param esizeof Sizeof expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_esizeof_first_tok(ast_esizeof_t *esizeof)
{
	return &esizeof->tsizeof;
}

/** Get last token of AST sizeo expression.
 *
 * @param eaddr Sizeof expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_esizeof_last_tok(ast_esizeof_t *esizeof)
{
	return ast_tree_last_tok(esizeof->bexpr);
}

/** Create AST cast expression.
 *
 * @param recast Place to store pointer to new cast expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ecast_create(ast_ecast_t **recast)
{
	ast_ecast_t *ecast;

	ecast = calloc(1, sizeof(ast_ecast_t));
	if (ecast == NULL)
		return ENOMEM;

	ecast->node.ext = ecast;
	ecast->node.ntype = ant_ecast;

	*recast = ecast;
	return EOK;
}

/** Print AST cast expression.
 *
 * @param ecast Cast expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_ecast_print(ast_ecast_t *ecast, FILE *f)
{
	int rc;

	(void) ecast;

	if (fprintf(f, "ecast(") < 0)
		return EIO;

	rc = ast_tree_print(ecast->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST cast expression.
 *
 * @param ecast Cast expression
 */
static void ast_ecast_destroy(ast_ecast_t *ecast)
{
	ast_tree_destroy(ecast->bexpr);
	free(ecast);
}

/** Get first token of AST cast expression.
 *
 * @param ecast Cast expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_ecast_first_tok(ast_ecast_t *ecast)
{
	return &ecast->tlparen;
}

/** Get last token of AST sizeo expression.
 *
 * @param eaddr Cast expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ecast_last_tok(ast_ecast_t *ecast)
{
	return ast_tree_last_tok(ecast->bexpr);
}

/** Create AST member expression.
 *
 * @param remember Place to store pointer to new member expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_emember_create(ast_emember_t **remember)
{
	ast_emember_t *emember;

	emember = calloc(1, sizeof(ast_emember_t));
	if (emember == NULL)
		return ENOMEM;

	emember->node.ext = emember;
	emember->node.ntype = ant_emember;

	*remember = emember;
	return EOK;
}

/** Print AST member expression.
 *
 * @param emember Member expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_emember_print(ast_emember_t *emember, FILE *f)
{
	int rc;

	(void) emember;

	if (fprintf(f, "emember(") < 0)
		return EIO;

	rc = ast_tree_print(emember->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST member expression.
 *
 * @param emember Member expression
 */
static void ast_emember_destroy(ast_emember_t *emember)
{
	ast_tree_destroy(emember->bexpr);
	free(emember);
}

/** Get first token of AST member expression.
 *
 * @param emember Member expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_emember_first_tok(ast_emember_t *emember)
{
	return ast_tree_first_tok(emember->bexpr);
}

/** Get last token of AST member expression.
 *
 * @param emember Member expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_emember_last_tok(ast_emember_t *emember)
{
	return &emember->tperiod;
}

/** Create AST indirect member expression.
 *
 * @param reindmember Place to store pointer to new indirect member expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_eindmember_create(ast_eindmember_t **reindmember)
{
	ast_eindmember_t *eindmember;

	eindmember = calloc(1, sizeof(ast_eindmember_t));
	if (eindmember == NULL)
		return ENOMEM;

	eindmember->node.ext = eindmember;
	eindmember->node.ntype = ant_eindmember;

	*reindmember = eindmember;
	return EOK;
}

/** Print AST indirect member expression.
 *
 * @param eindmember Indirect member expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_eindmember_print(ast_eindmember_t *eindmember, FILE *f)
{
	int rc;

	(void) eindmember;

	if (fprintf(f, "eindmember(") < 0)
		return EIO;

	rc = ast_tree_print(eindmember->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST indirect member expression.
 *
 * @param eindmember Indirect member expression
 */
static void ast_eindmember_destroy(ast_eindmember_t *eindmember)
{
	ast_tree_destroy(eindmember->bexpr);
	free(eindmember);
}

/** Get first token of AST indirect member expression.
 *
 * @param eindmember Indirect member expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_eindmember_first_tok(ast_eindmember_t *eindmember)
{
	return ast_tree_first_tok(eindmember->bexpr);
}

/** Get last token of AST indirect member expression.
 *
 * @param eindmember Indirect member expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_eindmember_last_tok(ast_eindmember_t *eindmember)
{
	return &eindmember->tarrow;
}

/** Create AST unary sign expression.
 *
 * @param reusign Place to store pointer to new unary sign expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_eusign_create(ast_eusign_t **reusign)
{
	ast_eusign_t *eusign;

	eusign = calloc(1, sizeof(ast_eusign_t));
	if (eusign == NULL)
		return ENOMEM;

	eusign->node.ext = eusign;
	eusign->node.ntype = ant_eusign;

	*reusign = eusign;
	return EOK;
}

/** Print AST unary sign expression.
 *
 * @param eusign Unary sign expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_eusign_print(ast_eusign_t *eusign, FILE *f)
{
	int rc;

	(void) eusign;

	if (fprintf(f, "eusign(") < 0)
		return EIO;

	rc = ast_tree_print(eusign->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST unary sign expression.
 *
 * @param eusign Unary sign expression
 */
static void ast_eusign_destroy(ast_eusign_t *eusign)
{
	ast_tree_destroy(eusign->bexpr);
	free(eusign);
}

/** Get first token of AST unary sign expression.
 *
 * @param eusign Unary sign expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_eusign_first_tok(ast_eusign_t *eusign)
{
	return &eusign->tsign;
}

/** Get last token of AST unary sign expression.
 *
 * @param eusign Unary sign expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_eusign_last_tok(ast_eusign_t *eusign)
{
	return ast_tree_last_tok(eusign->bexpr);
}

/** Create AST logical not expression.
 *
 * @param relnot Place to store pointer to new logical not expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_elnot_create(ast_elnot_t **relnot)
{
	ast_elnot_t *elnot;

	elnot = calloc(1, sizeof(ast_elnot_t));
	if (elnot == NULL)
		return ENOMEM;

	elnot->node.ext = elnot;
	elnot->node.ntype = ant_elnot;

	*relnot = elnot;
	return EOK;
}

/** Print AST logical not expression.
 *
 * @param elnot Logical not expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_elnot_print(ast_elnot_t *elnot, FILE *f)
{
	int rc;

	(void) elnot;

	if (fprintf(f, "elnot(") < 0)
		return EIO;

	rc = ast_tree_print(elnot->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST logical not expression.
 *
 * @param elnot Logical not expression
 */
static void ast_elnot_destroy(ast_elnot_t *elnot)
{
	ast_tree_destroy(elnot->bexpr);
	free(elnot);
}

/** Get first token of AST logical not expression.
 *
 * @param elnot Logical not expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_elnot_first_tok(ast_elnot_t *elnot)
{
	return &elnot->tlnot;
}

/** Get last token of AST logical not expression.
 *
 * @param elnot Logical not expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_elnot_last_tok(ast_elnot_t *elnot)
{
	return ast_tree_last_tok(elnot->bexpr);
}

/** Create AST bitwise not expression.
 *
 * @param rebnot Place to store pointer to new bitwise not expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ebnot_create(ast_ebnot_t **rebnot)
{
	ast_ebnot_t *ebnot;

	ebnot = calloc(1, sizeof(ast_ebnot_t));
	if (ebnot == NULL)
		return ENOMEM;

	ebnot->node.ext = ebnot;
	ebnot->node.ntype = ant_ebnot;

	*rebnot = ebnot;
	return EOK;
}

/** Print AST bitwise not expression.
 *
 * @param ebnot Bitwise not expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_ebnot_print(ast_ebnot_t *ebnot, FILE *f)
{
	int rc;

	(void) ebnot;

	if (fprintf(f, "ebnot(") < 0)
		return EIO;

	rc = ast_tree_print(ebnot->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST bitwise not expression.
 *
 * @param ebnot Bitwise not expression
 */
static void ast_ebnot_destroy(ast_ebnot_t *ebnot)
{
	ast_tree_destroy(ebnot->bexpr);
	free(ebnot);
}

/** Get first token of AST bitwise not expression.
 *
 * @param ebnot Bitwise not expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_ebnot_first_tok(ast_ebnot_t *ebnot)
{
	return &ebnot->tbnot;
}

/** Get last token of AST bitwise not expression.
 *
 * @param ebnot Bitwise not expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ebnot_last_tok(ast_ebnot_t *ebnot)
{
	return ast_tree_last_tok(ebnot->bexpr);
}

/** Create AST pre-adjustment expression.
 *
 * @param repreadj Place to store pointer to new pre-adjustment expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_epreadj_create(ast_epreadj_t **repreadj)
{
	ast_epreadj_t *epreadj;

	epreadj = calloc(1, sizeof(ast_epreadj_t));
	if (epreadj == NULL)
		return ENOMEM;

	epreadj->node.ext = epreadj;
	epreadj->node.ntype = ant_epreadj;

	*repreadj = epreadj;
	return EOK;
}

/** Print AST pre-adjustment expression.
 *
 * @param epreadj Pre-adjustment expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_epreadj_print(ast_epreadj_t *epreadj, FILE *f)
{
	int rc;

	(void) epreadj;

	if (fprintf(f, "epreadj(") < 0)
		return EIO;

	rc = ast_tree_print(epreadj->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST pre-adjustment expression.
 *
 * @param epreadj Pre-adjustment expression
 */
static void ast_epreadj_destroy(ast_epreadj_t *epreadj)
{
	ast_tree_destroy(epreadj->bexpr);
	free(epreadj);
}

/** Get first token of AST pre-adjustment expression.
 *
 * @param epreadj Pre-adjustment expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_epreadj_first_tok(ast_epreadj_t *epreadj)
{
	return &epreadj->tadj;
}

/** Get last token of AST pre-adjustment expression.
 *
 * @param epreadj Pre-adjustment expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_epreadj_last_tok(ast_epreadj_t *epreadj)
{
	return ast_tree_last_tok(epreadj->bexpr);
}

/** Create AST pre-adjustment expression.
 *
 * @param repostadj Place to store pointer to new pre-adjustment expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_epostadj_create(ast_epostadj_t **repostadj)
{
	ast_epostadj_t *epostadj;

	epostadj = calloc(1, sizeof(ast_epostadj_t));
	if (epostadj == NULL)
		return ENOMEM;

	epostadj->node.ext = epostadj;
	epostadj->node.ntype = ant_epostadj;

	*repostadj = epostadj;
	return EOK;
}

/** Print AST pre-adjustment expression.
 *
 * @param epostadj Pre-adjustment expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_epostadj_print(ast_epostadj_t *epostadj, FILE *f)
{
	int rc;

	(void) epostadj;

	if (fprintf(f, "epostadj(") < 0)
		return EIO;

	rc = ast_tree_print(epostadj->bexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST pre-adjustment expression.
 *
 * @param epostadj Pre-adjustment expression
 */
static void ast_epostadj_destroy(ast_epostadj_t *epostadj)
{
	ast_tree_destroy(epostadj->bexpr);
	free(epostadj);
}

/** Get first token of AST pre-adjustment expression.
 *
 * @param epostadj Pre-adjustment expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_epostadj_first_tok(ast_epostadj_t *epostadj)
{
	return ast_tree_first_tok(epostadj->bexpr);
}

/** Get last token of AST pre-adjustment expression.
 *
 * @param epostadj Pre-adjustment expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_epostadj_last_tok(ast_epostadj_t *epostadj)
{
	return &epostadj->tadj;
}

/** Create AST break.
 *
 * @param rbreak Place to store pointer to new break
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_break_create(ast_break_t **rbreak)
{
	ast_break_t *abreak;

	abreak = calloc(1, sizeof(ast_break_t));
	if (abreak == NULL)
		return ENOMEM;

	abreak->node.ext = abreak;
	abreak->node.ntype = ant_break;

	*rbreak = abreak;
	return EOK;
}

/** Print AST break.
 *
 * @param abreak Break statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_break_print(ast_break_t *abreak, FILE *f)
{
	(void) abreak;

	if (fprintf(f, "break()") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST break.
 *
 * @param abreak Break statement
 */
static void ast_break_destroy(ast_break_t *abreak)
{
	free(abreak);
}

/** Get first token of AST break.
 *
 * @param abreak Break
 * @break First token or @c NULL
 */
static ast_tok_t *ast_break_first_tok(ast_break_t *abreak)
{
	return &abreak->tbreak;
}

/** Get last token of AST break.
 *
 * @param abreak Break
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_break_last_tok(ast_break_t *abreak)
{
	return &abreak->tscolon;
}

/** Create AST continue.
 *
 * @param rcontinue Place to store pointer to new continue
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_continue_create(ast_continue_t **rcontinue)
{
	ast_continue_t *acontinue;

	acontinue = calloc(1, sizeof(ast_continue_t));
	if (acontinue == NULL)
		return ENOMEM;

	acontinue->node.ext = acontinue;
	acontinue->node.ntype = ant_continue;

	*rcontinue = acontinue;
	return EOK;
}

/** Print AST continue.
 *
 * @param acontinue Continue statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_continue_print(ast_continue_t *acontinue, FILE *f)
{
	(void) acontinue;

	if (fprintf(f, "continue()") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST continue.
 *
 * @param acontinue Continue statement
 */
static void ast_continue_destroy(ast_continue_t *acontinue)
{
	free(acontinue);
}

/** Get first token of AST continue.
 *
 * @param acontinue Continue
 * @return First token or @c NULL
 */
static ast_tok_t *ast_continue_first_tok(ast_continue_t *acontinue)
{
	return &acontinue->tcontinue;
}

/** Get last token of AST continue.
 *
 * @param acontinue Continue
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_continue_last_tok(ast_continue_t *acontinue)
{
	return &acontinue->tscolon;
}

/** Create AST goto.
 *
 * @param rgoto Place to store pointer to new goto
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_goto_create(ast_goto_t **rgoto)
{
	ast_goto_t *agoto;

	agoto = calloc(1, sizeof(ast_goto_t));
	if (agoto == NULL)
		return ENOMEM;

	agoto->node.ext = agoto;
	agoto->node.ntype = ant_goto;

	*rgoto = agoto;
	return EOK;
}

/** Print AST goto.
 *
 * @param agoto Goto statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_goto_print(ast_goto_t *agoto, FILE *f)
{
	(void) agoto;

	if (fprintf(f, "goto()") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST goto.
 *
 * @param agoto Goto statement
 */
static void ast_goto_destroy(ast_goto_t *agoto)
{
	free(agoto);
}

/** Get first token of AST goto.
 *
 * @param agoto Goto
 * @return First token or @c NULL
 */
static ast_tok_t *ast_goto_first_tok(ast_goto_t *agoto)
{
	return &agoto->tgoto;
}

/** Get last token of AST goto.
 *
 * @param agoto Goto
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_goto_last_tok(ast_goto_t *agoto)
{
	return &agoto->tscolon;
}

/** Create AST return.
 *
 * @param rreturn Place to store pointer to new return
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_return_create(ast_return_t **rreturn)
{
	ast_return_t *areturn;

	areturn = calloc(1, sizeof(ast_return_t));
	if (areturn == NULL)
		return ENOMEM;

	areturn->node.ext = areturn;
	areturn->node.ntype = ant_return;

	*rreturn = areturn;
	return EOK;
}

/** Print AST return.
 *
 * @param areturn Return statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_return_print(ast_return_t *areturn, FILE *f)
{
	int rc;

	if (fprintf(f, "return(") < 0)
		return EIO;
	rc = ast_tree_print(areturn->arg, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST return.
 *
 * @param areturn Return statement
 */
static void ast_return_destroy(ast_return_t *areturn)
{
	ast_tree_destroy(areturn->arg);
	free(areturn);
}

/** Get first token of AST return.
 *
 * @param areturn Return
 * @return First token or @c NULL
 */
static ast_tok_t *ast_return_first_tok(ast_return_t *areturn)
{
	return &areturn->treturn;
}

/** Get last token of AST return.
 *
 * @param areturn Return
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_return_last_tok(ast_return_t *areturn)
{
	return &areturn->tscolon;
}

/** Create AST if statement.
 *
 * @param rif Place to store pointer to new if statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_if_create(ast_if_t **rif)
{
	ast_if_t *aif;

	aif = calloc(1, sizeof(ast_if_t));
	if (aif == NULL)
		return ENOMEM;

	aif->node.ext = aif;
	aif->node.ntype = ant_if;

	*rif = aif;
	return EOK;
}

/** Print AST if statement.
 *
 * @param aif If statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_if_print(ast_if_t *aif, FILE *f)
{
	int rc;

	if (fprintf(f, "if(") < 0)
		return EIO;
	rc = ast_tree_print(aif->cond, f);
	if (fprintf(f, ",") < 0)
		return EIO;
	if (fprintf(f, ",") < 0)
		return EIO;
	rc = ast_block_print(aif->tbranch, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ",") < 0)
		return EIO;
	rc = ast_block_print(aif->fbranch, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST if statement.
 *
 * @param aif If statement
 */
static void ast_if_destroy(ast_if_t *aif)
{
	ast_tree_destroy(aif->cond);
	ast_block_destroy(aif->tbranch);
	ast_block_destroy(aif->fbranch);
	free(aif);
}

/** Get first token of AST if statement.
 *
 * @param aif If statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_if_first_tok(ast_if_t *aif)
{
	return &aif->tif;
}

/** Get last token of AST if statement.
 *
 * @param aif If statement
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_if_last_tok(ast_if_t *aif)
{
	if (aif->fbranch != NULL)
		return ast_block_last_tok(aif->fbranch);
	else
		return ast_block_last_tok(aif->tbranch);
}

/** Create AST while loop statement.
 *
 * @param rwhile Place to store pointer to new while loop statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_while_create(ast_while_t **rwhile)
{
	ast_while_t *awhile;

	awhile = calloc(1, sizeof(ast_while_t));
	if (awhile == NULL)
		return ENOMEM;

	awhile->node.ext = awhile;
	awhile->node.ntype = ant_while;

	*rwhile = awhile;
	return EOK;
}

/** Print AST while loop statement.
 *
 * @param awhile While loop statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_while_print(ast_while_t *awhile, FILE *f)
{
	int rc;

	if (fprintf(f, "while(") < 0)
		return EIO;
	rc = ast_tree_print(awhile->cond, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ",") < 0)
		return EIO;
	rc = ast_block_print(awhile->body, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST while loop statement.
 *
 * @param awhile While loop statement
 */
static void ast_while_destroy(ast_while_t *awhile)
{
	ast_tree_destroy(awhile->cond);
	ast_block_destroy(awhile->body);
	free(awhile);
}

/** Get first token of AST while loop statement.
 *
 * @param awhile While loop statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_while_first_tok(ast_while_t *awhile)
{
	return &awhile->twhile;
}

/** Get last token of AST while loop statement.
 *
 * @param awhile While loop statement
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_while_last_tok(ast_while_t *awhile)
{
	return ast_block_last_tok(awhile->body);
}

/** Create AST do loop statement.
 *
 * @param rdo Place to store pointer to new do loop statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_do_create(ast_do_t **rdo)
{
	ast_do_t *ado;

	ado = calloc(1, sizeof(ast_do_t));
	if (ado == NULL)
		return ENOMEM;

	ado->node.ext = ado;
	ado->node.ntype = ant_do;

	*rdo = ado;
	return EOK;
}

/** Print AST do loop statement.
 *
 * @param ado Do loop statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_do_print(ast_do_t *ado, FILE *f)
{
	int rc;

	if (fprintf(f, "do(") < 0)
		return EIO;

	rc = ast_block_print(ado->body, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ",") < 0)
		return EIO;

	rc = ast_tree_print(ado->cond, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST do loop statement.
 *
 * @param ado Do loop statement
 */
static void ast_do_destroy(ast_do_t *ado)
{
	ast_block_destroy(ado->body);
	ast_tree_destroy(ado->cond);
	free(ado);
}

/** Get first token of AST do loop statement.
 *
 * @param ado Do loop statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_do_first_tok(ast_do_t *ado)
{
	return &ado->tdo;
}

/** Get last token of AST do loop statement.
 *
 * @param ado Do loop statementOS
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_do_last_tok(ast_do_t *ado)
{
	return &ado->tscolon;
}

/** Create AST for statement.
 *
 * @param rfor Place to store pointer to new for statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_for_create(ast_for_t **rfor)
{
	ast_for_t *afor;

	afor = calloc(1, sizeof(ast_for_t));
	if (afor == NULL)
		return ENOMEM;

	afor->node.ext = afor;
	afor->node.ntype = ant_for;

	*rfor = afor;
	return EOK;
}

/** Print AST for statement.
 *
 * @param afor For statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_for_print(ast_for_t *afor, FILE *f)
{
	int rc;

	if (fprintf(f, "for(") < 0)
		return EIO;
	rc = ast_tree_print(afor->linit, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ",") < 0)
		return EIO;
	rc = ast_tree_print(afor->lcond, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ",") < 0)
		return EIO;
	rc = ast_tree_print(afor->lnext, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ",") < 0)
		return EIO;
	rc = ast_block_print(afor->body, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST for statement.
 *
 * @param afor For statement
 */
static void ast_for_destroy(ast_for_t *afor)
{
	ast_tree_destroy(afor->linit);
	ast_tree_destroy(afor->lcond);
	ast_tree_destroy(afor->lnext);
	ast_block_destroy(afor->body);
	free(afor);
}

/** Get first token of AST for statement.
 *
 * @param afor For statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_for_first_tok(ast_for_t *afor)
{
	return &afor->tfor;
}

/** Get last token of AST for statement.
 *
 * @param afor For statement
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_for_last_tok(ast_for_t *afor)
{
	return ast_block_last_tok(afor->body);
}

/** Create AST switch statement.
 *
 * @param rswitch Place to store pointer to new switch statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_switch_create(ast_switch_t **rswitch)
{
	ast_switch_t *aswitch;

	aswitch = calloc(1, sizeof(ast_switch_t));
	if (aswitch == NULL)
		return ENOMEM;

	aswitch->node.ext = aswitch;
	aswitch->node.ntype = ant_switch;

	*rswitch = aswitch;
	return EOK;
}

/** Print AST switch statement.
 *
 * @param aswitch Switch statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_switch_print(ast_switch_t *aswitch, FILE *f)
{
	int rc;

	if (fprintf(f, "switch(") < 0)
		return EIO;

	rc = ast_tree_print(aswitch->sexpr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ",") < 0)
		return EIO;

	rc = ast_block_print(aswitch->body, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST switch statement.
 *
 * @param aswitch Sswitch statement
 */
static void ast_switch_destroy(ast_switch_t *aswitch)
{
	ast_tree_destroy(aswitch->sexpr);
	ast_block_destroy(aswitch->body);
	free(aswitch);
}

/** Get first token of AST switch statement.
 *
 * @param aswitch Switch statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_switch_first_tok(ast_switch_t *aswitch)
{
	return &aswitch->tswitch;
}

/** Get last token of AST switch statement.
 *
 * @param aswitch Switch statement
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_switch_last_tok(ast_switch_t *aswitch)
{
	return ast_block_last_tok(aswitch->body);
}

/** Create AST case label.
 *
 * @param rclabel Place to store pointer to new case label
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_clabel_create(ast_clabel_t **rclabel)
{
	ast_clabel_t *clabel;

	clabel = calloc(1, sizeof(ast_clabel_t));
	if (clabel == NULL)
		return ENOMEM;

	clabel->node.ext = clabel;
	clabel->node.ntype = ant_clabel;

	*rclabel = clabel;
	return EOK;
}

/** Print AST case label.
 *
 * @param clabel Case label
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_clabel_print(ast_clabel_t *clabel, FILE *f)
{
	(void) clabel;

	if (fprintf(f, "clabel()") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST case label.
 *
 * @param aswitch Switch statement
 */
static void ast_clabel_destroy(ast_clabel_t *clabel)
{
	ast_tree_destroy(clabel->cexpr);
	free(clabel);
}

/** Get first token of AST case label.
 *
 * @param clabel Case label
 * @return First token or @c NULL
 */
static ast_tok_t *ast_clabel_first_tok(ast_clabel_t *clabel)
{
	return &clabel->tcase;
}

/** Get last token of AST case label
 *
 * @param clabel Case label
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_clabel_last_tok(ast_clabel_t *clabel)
{
	return &clabel->tcolon;
}

/** Create AST goto label.
 *
 * @param rglabel Place to store pointer to new goto label
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_glabel_create(ast_glabel_t **rglabel)
{
	ast_glabel_t *glabel;

	glabel = calloc(1, sizeof(ast_glabel_t));
	if (glabel == NULL)
		return ENOMEM;

	glabel->node.ext = glabel;
	glabel->node.ntype = ant_glabel;

	*rglabel = glabel;
	return EOK;
}

/** Print AST goto label.
 *
 * @param glabel Goto label
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_glabel_print(ast_glabel_t *glabel, FILE *f)
{
	(void) glabel;

	if (fprintf(f, "glabel()") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST goto label.
 *
 * @param aswitch Switch statement
 */
static void ast_glabel_destroy(ast_glabel_t *glabel)
{
	free(glabel);
}

/** Get first token of AST goto label.
 *
 * @param glabel Goto label
 * @return First token or @c NULL
 */
static ast_tok_t *ast_glabel_first_tok(ast_glabel_t *glabel)
{
	return &glabel->tlabel;
}

/** Get last token of AST goto label
 *
 * @param glabel Goto label
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_glabel_last_tok(ast_glabel_t *glabel)
{
	return &glabel->tcolon;
}

/** Create AST expression statement.
 *
 * @param rstexpr Place to store pointer to new expression statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_stexpr_create(ast_stexpr_t **rstexpr)
{
	ast_stexpr_t *astexpr;

	astexpr = calloc(1, sizeof(ast_stexpr_t));
	if (astexpr == NULL)
		return ENOMEM;

	astexpr->node.ext = astexpr;
	astexpr->node.ntype = ant_stexpr;

	*rstexpr = astexpr;
	return EOK;
}

/** Print AST expression statement.
 *
 * @param astexpr Expression statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_stexpr_print(ast_stexpr_t *astexpr, FILE *f)
{
	int rc;

	if (fprintf(f, "stexpr(") < 0)
		return EIO;
	rc = ast_tree_print(astexpr->expr, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST expression statement.
 *
 * @param astexpr Expression statement
 */
static void ast_stexpr_destroy(ast_stexpr_t *astexpr)
{
	free(astexpr);
}

/** Get first token of AST expression statement.
 *
 * @param astexpr Expression statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_stexpr_first_tok(ast_stexpr_t *astexpr)
{
	return ast_tree_first_tok(astexpr->expr);
}

/** Get last token of AST expression statement.
 *
 * @param astexpr Expression statement
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_stexpr_last_tok(ast_stexpr_t *astexpr)
{
	return &astexpr->tscolon;
}

/** Create AST declaration statement.
 *
 * @param rstdecln Place to store pointer to new function definition
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_stdecln_create(ast_stdecln_t **rstdecln)
{
	ast_stdecln_t *stdecln;

	stdecln = calloc(1, sizeof(ast_stdecln_t));
	if (stdecln == NULL)
		return ENOMEM;

	stdecln->node.ext = stdecln;
	stdecln->node.ntype = ant_stdecln;

	*rstdecln = stdecln;
	return EOK;
}

/** Print AST declaration statement.
 *
 * @param stdecln Declaration statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_stdecln_print(ast_stdecln_t *stdecln, FILE *f)
{
	int rc;

	if (fprintf(f, "stdecln(") < 0)
		return EIO;

	rc = ast_tree_print(&stdecln->dspecs->node, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ", ") < 0)
		return EIO;

	rc = ast_dlist_print(stdecln->dlist, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST function definition.
 *
 * @param stdecln Function definition
 */
static void ast_stdecln_destroy(ast_stdecln_t *stdecln)
{
	ast_dspecs_destroy(stdecln->dspecs);
	ast_dlist_destroy(stdecln->dlist);
	ast_tree_destroy(stdecln->init);

	free(stdecln);
}

/** Get first token of AST declaration statement.
 *
 * @param stdecln Declaration statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_stdecln_first_tok(ast_stdecln_t *stdecln)
{
	return ast_dspecs_first_tok(stdecln->dspecs);
}

/** Get last token of AST declaration statement.
 *
 * @param stdecln Declaration statement
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_stdecln_last_tok(ast_stdecln_t *stdecln)
{
	return &stdecln->tscolon;
}

/** Create AST null statement.
 *
 * @param rstnull Place to store pointer to new null statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_stnull_create(ast_stnull_t **rstnull)
{
	ast_stnull_t *astnull;

	astnull = calloc(1, sizeof(ast_stnull_t));
	if (astnull == NULL)
		return ENOMEM;

	astnull->node.ext = astnull;
	astnull->node.ntype = ant_stnull;

	*rstnull = astnull;
	return EOK;
}

/** Print AST null statement.
 *
 * @param astnull Null statement
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_stnull_print(ast_stnull_t *astnull, FILE *f)
{
	(void) astnull;

	if (fprintf(f, "stnull()") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST null statement.
 *
 * @param astnull Null statement
 */
static void ast_stnull_destroy(ast_stnull_t *astnull)
{
	free(astnull);
}

/** Get first token of AST null statement.
 *
 * @param astnull Null statement
 * @return First token or @c NULL
 */
static ast_tok_t *ast_stnull_first_tok(ast_stnull_t *astnull)
{
	return &astnull->tscolon;
}

/** Get last token of AST null statement.
 *
 * @param astnull Null statement
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_stnull_last_tok(ast_stnull_t *astnull)
{
	return &astnull->tscolon;
}

/** Print AST tree.
 *
 * @param node Root node
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int ast_tree_print(ast_node_t *node, FILE *f)
{
	switch (node->ntype) {
	case ant_block:
		return ast_block_print((ast_block_t *)node->ext, f);
	case ant_gdecln:
		return ast_gdecln_print((ast_gdecln_t *)node->ext, f);
	case ant_module:
		return ast_module_print((ast_module_t *)node->ext, f);
	case ant_sclass:
		return ast_sclass_print((ast_sclass_t *)node->ext, f);
	case ant_tqual:
		return ast_tqual_print((ast_tqual_t *)node->ext, f);
	case ant_tsbasic:
		return ast_tsbasic_print((ast_tsbasic_t *)node->ext, f);
	case ant_tsident:
		return ast_tsident_print((ast_tsident_t *)node->ext, f);
	case ant_tsrecord:
		return ast_tsrecord_print((ast_tsrecord_t *)node->ext, f);
	case ant_tsenum:
		return ast_tsenum_print((ast_tsenum_t *)node->ext, f);
	case ant_fspec:
		return ast_fspec_print((ast_fspec_t *)node->ext, f);
	case ant_sqlist:
		return ast_sqlist_print((ast_sqlist_t *)node->ext, f);
	case ant_dspecs:
		return ast_dspecs_print((ast_dspecs_t *)node->ext, f);
	case ant_dident:
		return ast_dident_print((ast_dident_t *)node->ext, f);
	case ant_dnoident:
		return ast_dnoident_print((ast_dnoident_t *)node->ext, f);
	case ant_dparen:
		return ast_dparen_print((ast_dparen_t *)node->ext, f);
	case ant_dptr:
		return ast_dptr_print((ast_dptr_t *)node->ext, f);
	case ant_dfun:
		return ast_dfun_print((ast_dfun_t *)node->ext, f);
	case ant_darray:
		return ast_darray_print((ast_darray_t *)node->ext, f);
	case ant_dlist:
		return ast_dlist_print((ast_dlist_t *)node->ext, f);
	case ant_eint:
		return ast_eint_print((ast_eint_t *)node->ext, f);
	case ant_echar:
		return ast_echar_print((ast_echar_t *)node->ext, f);
	case ant_estring:
		return ast_estring_print((ast_estring_t *)node->ext, f);
	case ant_eident:
		return ast_eident_print((ast_eident_t *)node->ext, f);
	case ant_eparen:
		return ast_eparen_print((ast_eparen_t *)node->ext, f);
	case ant_ebinop:
		return ast_ebinop_print((ast_ebinop_t *)node->ext, f);
	case ant_etcond:
		return ast_etcond_print((ast_etcond_t *)node->ext, f);
	case ant_ecomma:
		return ast_ecomma_print((ast_ecomma_t *)node->ext, f);
	case ant_efuncall:
		return ast_efuncall_print((ast_efuncall_t *)node->ext, f);
	case ant_eindex:
		return ast_eindex_print((ast_eindex_t *)node->ext, f);
	case ant_ederef:
		return ast_ederef_print((ast_ederef_t *)node->ext, f);
	case ant_eusign:
		return ast_eusign_print((ast_eusign_t *)node->ext, f);
	case ant_eaddr:
		return ast_eaddr_print((ast_eaddr_t *)node->ext, f);
	case ant_esizeof:
		return ast_esizeof_print((ast_esizeof_t *)node->ext, f);
	case ant_ecast:
		return ast_ecast_print((ast_ecast_t *)node->ext, f);
	case ant_emember:
		return ast_emember_print((ast_emember_t *)node->ext, f);
	case ant_eindmember:
		return ast_eindmember_print((ast_eindmember_t *)node->ext, f);
	case ant_elnot:
		return ast_elnot_print((ast_elnot_t *)node->ext, f);
	case ant_ebnot:
		return ast_ebnot_print((ast_ebnot_t *)node->ext, f);
	case ant_epreadj:
		return ast_epreadj_print((ast_epreadj_t *)node->ext, f);
	case ant_epostadj:
		return ast_epostadj_print((ast_epostadj_t *)node->ext, f);
	case ant_break:
		return ast_break_print((ast_break_t *)node->ext, f);
	case ant_continue:
		return ast_continue_print((ast_continue_t *)node->ext, f);
	case ant_goto:
		return ast_goto_print((ast_goto_t *)node->ext, f);
	case ant_return:
		return ast_return_print((ast_return_t *)node->ext, f);
	case ant_if:
		return ast_if_print((ast_if_t *)node->ext, f);
	case ant_while:
		return ast_while_print((ast_while_t *)node->ext, f);
	case ant_do:
		return ast_do_print((ast_do_t *)node->ext, f);
	case ant_for:
		return ast_for_print((ast_for_t *)node->ext, f);
	case ant_switch:
		return ast_switch_print((ast_switch_t *)node->ext, f);
	case ant_clabel:
		return ast_clabel_print((ast_clabel_t *)node->ext, f);
	case ant_glabel:
		return ast_glabel_print((ast_glabel_t *)node->ext, f);
	case ant_stexpr:
		return ast_stexpr_print((ast_stexpr_t *)node->ext, f);
	case ant_stdecln:
		return ast_stdecln_print((ast_stdecln_t *)node->ext, f);
	case ant_stnull:
		return ast_stnull_print((ast_stnull_t *)node->ext, f);
	}

	return EINVAL;
}

/** Destroy AST tree.
 *
 * @param node Root node
 */
void ast_tree_destroy(ast_node_t *node)
{
	if (node == NULL)
		return;

	switch (node->ntype) {
	case ant_block:
		ast_block_destroy((ast_block_t *)node->ext);
		break;
	case ant_gdecln:
		ast_gdecln_destroy((ast_gdecln_t *)node->ext);
		break;
	case ant_module:
		ast_module_destroy((ast_module_t *)node->ext);
		break;
	case ant_sclass:
		ast_sclass_destroy((ast_sclass_t *)node->ext);
		break;
	case ant_tqual:
		ast_tqual_destroy((ast_tqual_t *)node->ext);
		break;
	case ant_tsbasic:
		ast_tsbasic_destroy((ast_tsbasic_t *)node->ext);
		break;
	case ant_tsident:
		ast_tsident_destroy((ast_tsident_t *)node->ext);
		break;
	case ant_tsrecord:
		ast_tsrecord_destroy((ast_tsrecord_t *)node->ext);
		break;
	case ant_tsenum:
		ast_tsenum_destroy((ast_tsenum_t *)node->ext);
		break;
	case ant_fspec:
		ast_fspec_destroy((ast_fspec_t *)node->ext);
		break;
	case ant_sqlist:
		ast_sqlist_destroy((ast_sqlist_t *)node->ext);
		break;
	case ant_dspecs:
		ast_dspecs_destroy((ast_dspecs_t *)node->ext);
		break;
	case ant_dident:
		ast_dident_destroy((ast_dident_t *)node->ext);
		break;
	case ant_dnoident:
		ast_dnoident_destroy((ast_dnoident_t *)node->ext);
		break;
	case ant_dparen:
		ast_dparen_destroy((ast_dparen_t *)node->ext);
		break;
	case ant_dptr:
		ast_dptr_destroy((ast_dptr_t *)node->ext);
		break;
	case ant_dfun:
		ast_dfun_destroy((ast_dfun_t *)node->ext);
		break;
	case ant_darray:
		ast_darray_destroy((ast_darray_t *)node->ext);
		break;
	case ant_dlist:
		ast_dlist_destroy((ast_dlist_t *)node->ext);
		break;
	case ant_eint:
		ast_eint_destroy((ast_eint_t *)node->ext);
		break;
	case ant_echar:
		ast_echar_destroy((ast_echar_t *)node->ext);
		break;
	case ant_estring:
		ast_estring_destroy((ast_estring_t *)node->ext);
		break;
	case ant_eident:
		ast_eident_destroy((ast_eident_t *)node->ext);
		break;
	case ant_eparen:
		ast_eparen_destroy((ast_eparen_t *)node->ext);
		break;
	case ant_ebinop:
		ast_ebinop_destroy((ast_ebinop_t *)node->ext);
		break;
	case ant_etcond:
		ast_etcond_destroy((ast_etcond_t *)node->ext);
		break;
	case ant_ecomma:
		ast_ecomma_destroy((ast_ecomma_t *)node->ext);
		break;
	case ant_efuncall:
		ast_efuncall_destroy((ast_efuncall_t *)node->ext);
		break;
	case ant_eindex:
		ast_eindex_destroy((ast_eindex_t *)node->ext);
		break;
	case ant_ederef:
		ast_ederef_destroy((ast_ederef_t *)node->ext);
		break;
	case ant_eusign:
		ast_eusign_destroy((ast_eusign_t *)node->ext);
		break;
	case ant_eaddr:
		ast_eaddr_destroy((ast_eaddr_t *)node->ext);
		break;
	case ant_esizeof:
		ast_esizeof_destroy((ast_esizeof_t *)node->ext);
		break;
	case ant_ecast:
		ast_ecast_destroy((ast_ecast_t *)node->ext);
		break;
	case ant_emember:
		ast_emember_destroy((ast_emember_t *)node->ext);
		break;
	case ant_eindmember:
		ast_eindmember_destroy((ast_eindmember_t *)node->ext);
		break;
	case ant_elnot:
		ast_elnot_destroy((ast_elnot_t *)node->ext);
		break;
	case ant_ebnot:
		ast_ebnot_destroy((ast_ebnot_t *)node->ext);
		break;
	case ant_epreadj:
		ast_epreadj_destroy((ast_epreadj_t *)node->ext);
		break;
	case ant_epostadj:
		ast_epostadj_destroy((ast_epostadj_t *)node->ext);
		break;
	case ant_break:
		ast_break_destroy((ast_break_t *)node->ext);
		break;
	case ant_continue:
		ast_continue_destroy((ast_continue_t *)node->ext);
		break;
	case ant_goto:
		ast_goto_destroy((ast_goto_t *)node->ext);
		break;
	case ant_return:
		ast_return_destroy((ast_return_t *)node->ext);
		break;
	case ant_if:
		return ast_if_destroy((ast_if_t *)node->ext);
	case ant_while:
		return ast_while_destroy((ast_while_t *)node->ext);
	case ant_do:
		return ast_do_destroy((ast_do_t *)node->ext);
	case ant_for:
		return ast_for_destroy((ast_for_t *)node->ext);
	case ant_switch:
		return ast_switch_destroy((ast_switch_t *)node->ext);
	case ant_clabel:
		return ast_clabel_destroy((ast_clabel_t *)node->ext);
	case ant_glabel:
		return ast_glabel_destroy((ast_glabel_t *)node->ext);
	case ant_stexpr:
		return ast_stexpr_destroy((ast_stexpr_t *)node->ext);
	case ant_stdecln:
		return ast_stdecln_destroy((ast_stdecln_t *)node->ext);
	case ant_stnull:
		return ast_stnull_destroy((ast_stnull_t *)node->ext);
	}
}

ast_tok_t *ast_tree_first_tok(ast_node_t *node)
{
	switch (node->ntype) {
	case ant_block:
		return ast_block_first_tok((ast_block_t *)node->ext);
	case ant_gdecln:
		return ast_gdecln_first_tok((ast_gdecln_t *)node->ext);
	case ant_module:
		return ast_module_first_tok((ast_module_t *)node->ext);
	case ant_sclass:
		return ast_sclass_first_tok((ast_sclass_t *)node->ext);
	case ant_tqual:
		return ast_tqual_first_tok((ast_tqual_t *)node->ext);
	case ant_tsbasic:
		return ast_tsbasic_first_tok((ast_tsbasic_t *)node->ext);
	case ant_tsident:
		return ast_tsident_first_tok((ast_tsident_t *)node->ext);
	case ant_tsrecord:
		return ast_tsrecord_first_tok((ast_tsrecord_t *)node->ext);
	case ant_tsenum:
		return ast_tsenum_first_tok((ast_tsenum_t *)node->ext);
	case ant_fspec:
		return ast_fspec_first_tok((ast_fspec_t *)node->ext);
	case ant_sqlist:
		return ast_sqlist_first_tok((ast_sqlist_t *)node->ext);
	case ant_dspecs:
		return ast_dspecs_first_tok((ast_dspecs_t *)node->ext);
	case ant_dident:
		return ast_dident_first_tok((ast_dident_t *)node->ext);
	case ant_dnoident:
		return ast_dnoident_first_tok((ast_dnoident_t *)node->ext);
	case ant_dparen:
		return ast_dparen_first_tok((ast_dparen_t *)node->ext);
	case ant_dptr:
		return ast_dptr_first_tok((ast_dptr_t *)node->ext);
	case ant_dfun:
		return ast_dfun_first_tok((ast_dfun_t *)node->ext);
	case ant_darray:
		return ast_darray_first_tok((ast_darray_t *)node->ext);
	case ant_dlist:
		return ast_dlist_first_tok((ast_dlist_t *)node->ext);
	case ant_eint:
		return ast_eint_first_tok((ast_eint_t *)node->ext);
	case ant_echar:
		return ast_echar_first_tok((ast_echar_t *)node->ext);
	case ant_estring:
		return ast_estring_first_tok((ast_estring_t *)node->ext);
	case ant_eident:
		return ast_eident_first_tok((ast_eident_t *)node->ext);
	case ant_eparen:
		return ast_eparen_first_tok((ast_eparen_t *)node->ext);
	case ant_ebinop:
		return ast_ebinop_first_tok((ast_ebinop_t *)node->ext);
	case ant_etcond:
		return ast_etcond_first_tok((ast_etcond_t *)node->ext);
	case ant_ecomma:
		return ast_ecomma_first_tok((ast_ecomma_t *)node->ext);
	case ant_efuncall:
		return ast_efuncall_first_tok((ast_efuncall_t *)node->ext);
	case ant_eindex:
		return ast_eindex_first_tok((ast_eindex_t *)node->ext);
	case ant_ederef:
		return ast_ederef_first_tok((ast_ederef_t *)node->ext);
	case ant_eusign:
		return ast_eusign_first_tok((ast_eusign_t *)node->ext);
	case ant_eaddr:
		return ast_eaddr_first_tok((ast_eaddr_t *)node->ext);
	case ant_esizeof:
		return ast_esizeof_first_tok((ast_esizeof_t *)node->ext);
	case ant_ecast:
		return ast_ecast_first_tok((ast_ecast_t *)node->ext);
	case ant_emember:
		return ast_emember_first_tok((ast_emember_t *)node->ext);
	case ant_eindmember:
		return ast_eindmember_first_tok((ast_eindmember_t *)node->ext);
	case ant_elnot:
		return ast_elnot_first_tok((ast_elnot_t *)node->ext);
	case ant_ebnot:
		return ast_ebnot_first_tok((ast_ebnot_t *)node->ext);
	case ant_epreadj:
		return ast_epreadj_first_tok((ast_epreadj_t *)node->ext);
	case ant_epostadj:
		return ast_epostadj_first_tok((ast_epostadj_t *)node->ext);
	case ant_break:
		return ast_break_first_tok((ast_break_t *)node->ext);
	case ant_continue:
		return ast_continue_first_tok((ast_continue_t *)node->ext);
	case ant_goto:
		return ast_goto_first_tok((ast_goto_t *)node->ext);
	case ant_return:
		return ast_return_first_tok((ast_return_t *)node->ext);
	case ant_if:
		return ast_if_first_tok((ast_if_t *)node->ext);
	case ant_while:
		return ast_while_first_tok((ast_while_t *)node->ext);
	case ant_do:
		return ast_do_first_tok((ast_do_t *)node->ext);
	case ant_for:
		return ast_for_first_tok((ast_for_t *)node->ext);
	case ant_switch:
		return ast_switch_first_tok((ast_switch_t *)node->ext);
	case ant_clabel:
		return ast_clabel_first_tok((ast_clabel_t *)node->ext);
	case ant_glabel:
		return ast_glabel_first_tok((ast_glabel_t *)node->ext);
	case ant_stexpr:
		return ast_stexpr_first_tok((ast_stexpr_t *)node->ext);
	case ant_stdecln:
		return ast_stdecln_first_tok((ast_stdecln_t *)node->ext);
	case ant_stnull:
		return ast_stnull_first_tok((ast_stnull_t *)node->ext);
	}

	assert(false);
	return NULL;
}

ast_tok_t *ast_tree_last_tok(ast_node_t *node)
{
	switch (node->ntype) {
	case ant_block:
		return ast_block_last_tok((ast_block_t *)node->ext);
	case ant_gdecln:
		return ast_gdecln_last_tok((ast_gdecln_t *)node->ext);
	case ant_module:
		return ast_module_last_tok((ast_module_t *)node->ext);
	case ant_sclass:
		return ast_sclass_last_tok((ast_sclass_t *)node->ext);
	case ant_tqual:
		return ast_tqual_last_tok((ast_tqual_t *)node->ext);
	case ant_tsbasic:
		return ast_tsbasic_last_tok((ast_tsbasic_t *)node->ext);
	case ant_tsident:
		return ast_tsident_last_tok((ast_tsident_t *)node->ext);
	case ant_tsrecord:
		return ast_tsrecord_last_tok((ast_tsrecord_t *)node->ext);
	case ant_tsenum:
		return ast_tsenum_last_tok((ast_tsenum_t *)node->ext);
	case ant_fspec:
		return ast_fspec_last_tok((ast_fspec_t *)node->ext);
	case ant_sqlist:
		return ast_sqlist_last_tok((ast_sqlist_t *)node->ext);
	case ant_dspecs:
		return ast_dspecs_last_tok((ast_dspecs_t *)node->ext);
	case ant_dident:
		return ast_dident_last_tok((ast_dident_t *)node->ext);
	case ant_dnoident:
		return ast_dnoident_last_tok((ast_dnoident_t *)node->ext);
	case ant_dparen:
		return ast_dparen_last_tok((ast_dparen_t *)node->ext);
	case ant_dptr:
		return ast_dptr_last_tok((ast_dptr_t *)node->ext);
	case ant_dfun:
		return ast_dfun_last_tok((ast_dfun_t *)node->ext);
	case ant_darray:
		return ast_darray_last_tok((ast_darray_t *)node->ext);
	case ant_dlist:
		return ast_dlist_last_tok((ast_dlist_t *)node->ext);
	case ant_eint:
		return ast_eint_last_tok((ast_eint_t *)node->ext);
	case ant_echar:
		return ast_echar_last_tok((ast_echar_t *)node->ext);
	case ant_estring:
		return ast_estring_last_tok((ast_estring_t *)node->ext);
	case ant_eident:
		return ast_eident_last_tok((ast_eident_t *)node->ext);
	case ant_eparen:
		return ast_eparen_last_tok((ast_eparen_t *)node->ext);
	case ant_ebinop:
		return ast_ebinop_last_tok((ast_ebinop_t *)node->ext);
	case ant_etcond:
		return ast_etcond_last_tok((ast_etcond_t *)node->ext);
	case ant_ecomma:
		return ast_ecomma_last_tok((ast_ecomma_t *)node->ext);
	case ant_efuncall:
		return ast_efuncall_last_tok((ast_efuncall_t *)node->ext);
	case ant_eindex:
		return ast_eindex_last_tok((ast_eindex_t *)node->ext);
	case ant_ederef:
		return ast_ederef_last_tok((ast_ederef_t *)node->ext);
	case ant_eusign:
		return ast_eusign_last_tok((ast_eusign_t *)node->ext);
	case ant_eaddr:
		return ast_eaddr_last_tok((ast_eaddr_t *)node->ext);
	case ant_esizeof:
		return ast_esizeof_last_tok((ast_esizeof_t *)node->ext);
	case ant_ecast:
		return ast_ecast_last_tok((ast_ecast_t *)node->ext);
	case ant_emember:
		return ast_emember_last_tok((ast_emember_t *)node->ext);
	case ant_eindmember:
		return ast_eindmember_last_tok((ast_eindmember_t *)node->ext);
	case ant_elnot:
		return ast_elnot_last_tok((ast_elnot_t *)node->ext);
	case ant_ebnot:
		return ast_ebnot_last_tok((ast_ebnot_t *)node->ext);
	case ant_epreadj:
		return ast_epreadj_last_tok((ast_epreadj_t *)node->ext);
	case ant_epostadj:
		return ast_epostadj_last_tok((ast_epostadj_t *)node->ext);
	case ant_break:
		return ast_break_last_tok((ast_break_t *)node->ext);
	case ant_continue:
		return ast_continue_last_tok((ast_continue_t *)node->ext);
	case ant_goto:
		return ast_goto_last_tok((ast_goto_t *)node->ext);
	case ant_return:
		return ast_return_last_tok((ast_return_t *)node->ext);
	case ant_if:
		return ast_if_last_tok((ast_if_t *)node->ext);
	case ant_while:
		return ast_while_last_tok((ast_while_t *)node->ext);
	case ant_do:
		return ast_do_last_tok((ast_do_t *)node->ext);
	case ant_for:
		return ast_for_last_tok((ast_for_t *)node->ext);
	case ant_switch:
		return ast_switch_last_tok((ast_switch_t *)node->ext);
	case ant_clabel:
		return ast_clabel_last_tok((ast_clabel_t *)node->ext);
	case ant_glabel:
		return ast_glabel_last_tok((ast_glabel_t *)node->ext);
	case ant_stexpr:
		return ast_stexpr_last_tok((ast_stexpr_t *)node->ext);
	case ant_stdecln:
		return ast_stdecln_last_tok((ast_stdecln_t *)node->ext);
	case ant_stnull:
		return ast_stnull_last_tok((ast_stnull_t *)node->ext);
	}

	assert(false);
	return NULL;
}
