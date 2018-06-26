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

static int ast_aspec_print(ast_aspec_t *, FILE *);
static int ast_aslist_print(ast_aslist_t *, FILE *);
static void ast_aslist_destroy(ast_aslist_t *);
static int ast_mattr_print(ast_mattr_t *, FILE *);
static void ast_mattr_destroy(ast_mattr_t *);
static ast_tok_t *ast_mattr_first_tok(ast_mattr_t *);
static ast_tok_t *ast_mattr_last_tok(ast_mattr_t *);
static int ast_block_print(ast_block_t *, FILE *);
static ast_tok_t *ast_block_last_tok(ast_block_t *);
static int ast_dlist_print(ast_dlist_t *, FILE *);
static int ast_idlist_print(ast_idlist_t *, FILE *);
static int ast_cinit_print(ast_cinit_t *, FILE *);
static void ast_cinit_destroy(ast_cinit_t *);
static ast_tok_t *ast_cinit_last_tok(ast_cinit_t *);
static void ast_idlist_destroy(ast_idlist_t *);
static void ast_malist_destroy(ast_malist_t *);
static ast_tok_t *ast_dspecs_first_tok(ast_dspecs_t *);
static void ast_dspecs_destroy(ast_dspecs_t *);
static ast_tok_t *ast_aspec_first_tok(ast_aspec_t *);
static ast_tok_t *ast_aspec_last_tok(ast_aspec_t *);
static void ast_aspec_destroy(ast_aspec_t *);
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
	(void) s; /* for Clang analyzer */

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
 * @param idlist Init-declarator list
 * @param malist Macro attribute list or @c NULL
 * @param body Body or @c NULL
 * @param rgdecln Place to store pointer to new global declaration
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_gdecln_create(ast_dspecs_t *dspecs, ast_idlist_t *idlist,
    ast_malist_t *malist, ast_block_t *body, ast_gdecln_t **rgdecln)
{
	ast_gdecln_t *gdecln;

	gdecln = calloc(1, sizeof(ast_gdecln_t));
	if (gdecln == NULL)
		return ENOMEM;

	gdecln->dspecs = dspecs;
	gdecln->idlist = idlist;
	gdecln->malist = malist;
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

	rc = ast_idlist_print(gdecln->idlist, f);
	if (rc != EOK)
		return rc;

	if (gdecln->body != NULL) {
		if (fprintf(f, ", ") < 0)
			return EIO;
		rc = ast_block_print(gdecln->body, f);
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
	ast_idlist_destroy(gdecln->idlist);
	ast_malist_destroy(gdecln->malist);
	ast_block_destroy(gdecln->body);

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

/** Create AST macro-based declaration.
 *
 * @param rmdecln Place to store pointer to new macro-based declaration
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_mdecln_create(ast_mdecln_t **rmdecln)
{
	ast_mdecln_t *mdecln;

	mdecln = calloc(1, sizeof(ast_mdecln_t));
	if (mdecln == NULL)
		return ENOMEM;

	list_initialize(&mdecln->args);

	mdecln->node.ext = mdecln;
	mdecln->node.ntype = ant_mdecln;

	*rmdecln = mdecln;
	return EOK;
}

/** Append entry to macro-based declaration argument list.
 *
 * @param mdecln Macro-based declaration
 * @param expr Argument expression
 * @param dcomma Data for separating comma token or @c NULL
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_mdecln_append(ast_mdecln_t *mdecln, ast_node_t *expr, void *dcomma)
{
	ast_mdecln_arg_t *arg;

	arg = calloc(1, sizeof(ast_mdecln_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->expr = expr;
	arg->tcomma.data = dcomma;

	arg->mdecln = mdecln;
	list_append(&arg->lmdecln, &mdecln->args);
	return EOK;
}

/** Return first argument in macro-based declaration.
 *
 * @param mdecln Macro-based declaration
 * @return First argument or @c NULL
 */
ast_mdecln_arg_t *ast_mdecln_first(ast_mdecln_t *mdecln)
{
	link_t *link;

	link = list_first(&mdecln->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mdecln_arg_t, lmdecln);
}

/** Return next argument in macro-based declaration.
 *
 * @param arg Call argument
 * @return Next argument or @c NULL
 */
ast_mdecln_arg_t *ast_mdecln_next(ast_mdecln_arg_t *arg)
{
	link_t *link;

	link = list_next(&arg->lmdecln, &arg->mdecln->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mdecln_arg_t, lmdecln);
}


/** Print AST macro-based declaration.
 *
 * @param mdecln Macro-based declaration
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_mdecln_print(ast_mdecln_t *mdecln, FILE *f)
{
	int rc;

	if (fprintf(f, "mdecln(") < 0)
		return EIO;

	if (mdecln->dspecs != NULL) {
		rc = ast_tree_print(&mdecln->dspecs->node, f);
		if (rc != EOK)
			return rc;
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST macro-based declaration.
 *
 * @param mdecln Macro-based declaration
 */
static void ast_mdecln_destroy(ast_mdecln_t *mdecln)
{
	ast_mdecln_arg_t *arg;

	if (mdecln == NULL)
		return;

	arg = ast_mdecln_first(mdecln);
	while (arg != NULL) {
		list_remove(&arg->lmdecln);
		ast_tree_destroy(arg->expr);
		free(arg);

		arg = ast_mdecln_first(mdecln);
	}

	ast_dspecs_destroy(mdecln->dspecs);
	free(mdecln);
}

/** Get first token of AST macro-based declaration.
 *
 * @param mdecln Macro-based declaration
 * @return First token or @c NULL
 */
static ast_tok_t *ast_mdecln_first_tok(ast_mdecln_t *mdecln)
{
	if (mdecln->dspecs != NULL)
		return ast_dspecs_first_tok(mdecln->dspecs);
	else
		return &mdecln->tname;
}

/** Get last token of AST macro-based declaration.
 *
 * @param mdecln Global macro-based declaration
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_mdecln_last_tok(ast_mdecln_t *mdecln)
{
	return &mdecln->trparen;
}

/** Create AST global macro-based declaration.
 *
 * @param rgmdecln Place to store pointer to new global macro-based declaration
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_gmdecln_create(ast_gmdecln_t **rgmdecln)
{
	ast_gmdecln_t *gmdecln;

	gmdecln = calloc(1, sizeof(ast_gmdecln_t));
	if (gmdecln == NULL)
		return ENOMEM;

	gmdecln->node.ext = gmdecln;
	gmdecln->node.ntype = ant_gmdecln;

	*rgmdecln = gmdecln;
	return EOK;
}

/** Print AST global macro-based declaration.
 *
 * @param gmdecln Global macro-based declaration
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_gmdecln_print(ast_gmdecln_t *gmdecln, FILE *f)
{
	int rc;

	if (fprintf(f, "gmdecln(") < 0)
		return EIO;

	if (gmdecln->mdecln != NULL) {
		rc = ast_tree_print(&gmdecln->mdecln->node, f);
		if (rc != EOK)
			return rc;
	}

	if (gmdecln->body != NULL) {
		if (fprintf(f, ", ") < 0)
			return EIO;
		rc = ast_block_print(gmdecln->body, f);
		if (rc != EOK)
			return rc;
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST global macro-based declaration.
 *
 * @param gmdecln Global macro-based declaration
 */
static void ast_gmdecln_destroy(ast_gmdecln_t *gmdecln)
{
	ast_mdecln_destroy(gmdecln->mdecln);
	ast_block_destroy(gmdecln->body);
	free(gmdecln);
}

/** Get first token of AST global macro-based declaration.
 *
 * @param gmdecln Global macro-based declaration
 * @return First token or @c NULL
 */
static ast_tok_t *ast_gmdecln_first_tok(ast_gmdecln_t *gmdecln)
{
	return ast_mdecln_first_tok(gmdecln->mdecln);
}

/** Get last token of AST global macro-based declaration.
 *
 * @param gmdecln Global macro-based declaration
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_gmdecln_last_tok(ast_gmdecln_t *gmdecln)
{
	return &gmdecln->tscolon;
}

/** Create AST extern "C" declaration.
 *
 * @param rexternc Place to store pointer to new extern "C" declaration
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_externc_create(ast_externc_t **rexternc)
{
	ast_externc_t *externc;

	externc = calloc(1, sizeof(ast_externc_t));
	if (externc == NULL)
		return ENOMEM;

	externc->node.ext = externc;
	externc->node.ntype = ant_externc;
	list_initialize(&externc->decls);

	*rexternc = externc;
	return EOK;
}

/** Append declaration to module.
 *
 * @param externc C++ extern "C" declaration
 * @param decl Declaration
 */
void ast_externc_append(ast_externc_t *externc, ast_node_t *decl)
{
	list_append(&decl->llist, &externc->decls);
	decl->lnode = &externc->node;
}

/** Return first declaration in extern "C".
 *
 * @param externc C++ extern "C" declaration
 * @return First declaration or @c NULL
 */
ast_node_t *ast_externc_first(ast_externc_t *externc)
{
	link_t *link;

	link = list_first(&externc->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return next declaration in extern "C".
 *
 * @param externc C++ extern "C" declaration
 * @return Next declaration or @c NULL
 */
ast_node_t *ast_externc_next(ast_node_t *node)
{
	link_t *link;
	ast_externc_t *externc;

	assert(node->lnode->ntype == ant_externc);
	externc = (ast_externc_t *) node->lnode->ext;

	link = list_next(&node->llist, &externc->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return last declaration in extern "C".
 *
 * @param externc C++ extern "C" declaration
 * @return Last declaration or @c NULL
 */
ast_node_t *ast_externc_last(ast_externc_t *externc)
{
	link_t *link;

	link = list_last(&externc->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return previous declaration in extern "C".
 *
 * @param externc C++ extern "C" declaration
 * @return Previous declaration or @c NULL
 */
ast_node_t *ast_externc_prev(ast_node_t *node)
{
	link_t *link;
	ast_externc_t *externc;

	assert(node->lnode->ntype == ant_module);
	externc = (ast_externc_t *) node->lnode->ext;

	link = list_prev(&node->llist, &externc->decls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}


/** Print AST extern "C" declaration.
 *
 * @param externc extern "C" declaration
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_externc_print(ast_externc_t *externc, FILE *f)
{
	ast_node_t *decl;

	if (fprintf(f, "extern \"C\"(") < 0)
		return EIO;

	decl = ast_externc_first(externc);
	while (decl != NULL) {
		ast_tree_print(decl, f);
		decl = ast_externc_next(decl);
	}

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST extern "C" declaration.
 *
 * @param externc C++ extern "C" declaration
 */
static void ast_externc_destroy(ast_externc_t *externc)
{
	ast_node_t *decl;

	decl = ast_externc_first(externc);
	while (decl != NULL) {
		list_remove(&decl->llist);
		ast_tree_destroy(decl);
		decl = ast_externc_first(externc);
	}

	free(externc);
}

/** Get first token of AST extern "C" declaration.
 *
 * @param externc C++ extern "C" declaration
 * @return First token or @c NULL
 */
static ast_tok_t *ast_externc_first_tok(ast_externc_t *externc)
{
	return &externc->textern;
}

/** Get last token of AST extern "C" declaration.
 *
 * @param externc C++ extern "C" declaration
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_externc_last_tok(ast_externc_t *externc)
{
	return &externc->trbrace;
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
 * @param dscolon Semicolon token data
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

/** Append element using macro declaration to record type specifier.
 *
 * @param tsrecord Record type specifier
 * @param mdecln Macro declaration
 * @param dscolon Semicolon token data
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsrecord_append_mdecln(ast_tsrecord_t *tsrecord, ast_mdecln_t *mdecln,
    void *dscolon)
{
	ast_tsrecord_elem_t *elem;

	elem = calloc(1, sizeof(ast_tsrecord_elem_t));
	if (elem == NULL)
		return ENOMEM;

	elem->mdecln = mdecln;
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

	if (tsrecord->aslist1 != NULL) {
		if (fprintf(f, ", ") < 0)
			return EIO;

		rc = ast_aslist_print(tsrecord->aslist1, f);
		if (rc != EOK)
			return rc;
	}

	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		if (elem->sqlist != NULL) {
			rc = ast_tree_print(&elem->sqlist->node, f);
			if (rc != EOK)
				return rc;
		}

		if (elem->mdecln != NULL) {
			rc = ast_tree_print(&elem->mdecln->node, f);
			if (rc != EOK)
				return rc;
		}

		elem = ast_tsrecord_next(elem);
	}

	if (tsrecord->aslist2 != NULL) {
		if (fprintf(f, ", ") < 0)
			return EIO;

		rc = ast_aslist_print(tsrecord->aslist2, f);
		if (rc != EOK)
			return rc;
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

	ast_aslist_destroy(tsrecord->aslist1);

	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		list_remove(&elem->ltsrecord);
		ast_sqlist_destroy(elem->sqlist);
		ast_dlist_destroy(elem->dlist);
		ast_mdecln_destroy(elem->mdecln);
		free(elem);
		elem = ast_tsrecord_first(tsrecord);
	}

	ast_aslist_destroy(tsrecord->aslist2);
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
 * @param init Initializer expression or @c NULL
 * @param dcomma Data for comma token or @c NULL
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tsenum_append(ast_tsenum_t *tsenum, void *dident, void *dequals,
    ast_node_t *init, void *dcomma)
{
	ast_tsenum_elem_t *elem;

	elem = calloc(1, sizeof(ast_tsenum_elem_t));
	if (elem == NULL)
		return ENOMEM;

	elem->tident.data = dident;
	elem->tequals.data = dequals;
	elem->init = init;
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
		ast_tree_destroy(elem->init);
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

/** Create AST register assignment.
 *
 * @param rregassign Place to store pointer to new register assignment
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_regassign_create(ast_regassign_t **rregassign)
{
	ast_regassign_t *regassign;

	regassign = calloc(1, sizeof(ast_regassign_t));
	if (regassign == NULL)
		return ENOMEM;

	regassign->node.ext = regassign;
	regassign->node.ntype = ant_regassign;

	*rregassign = regassign;
	return EOK;
}

/** Print AST register assignment.
 *
 * @param regassign Register assignment
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_regassign_print(ast_regassign_t *regassign, FILE *f)
{
	(void) regassign;

	if (fprintf(f, "regassign()") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST register assignment.
 *
 * @param regassign Register assignment
 */
static void ast_regassign_destroy(ast_regassign_t *regassign)
{
	free(regassign);
}

/** Get first token of AST register assignment.
 *
 * @param regassign Register assignment
 * @return First token or @c NULL
 */
static ast_tok_t *ast_regassign_first_tok(ast_regassign_t *regassign)
{
	return &regassign->tasm;
}

/** Get last token of AST register assignment.
 *
 * @param regassign Register assignment
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_regassign_last_tok(ast_regassign_t *regassign)
{
	return &regassign->trparen;
}

/** Create AST attribute specifier list.
 *
 * @param raslist Place to store pointer to new attribute specifier list
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_aslist_create(ast_aslist_t **raslist)
{
	ast_aslist_t *aslist;

	aslist = calloc(1, sizeof(ast_aslist_t));
	if (aslist == NULL)
		return ENOMEM;

	list_initialize(&aslist->aspecs);

	aslist->node.ext = aslist;
	aslist->node.ntype = ant_aslist;

	*raslist = aslist;
	return EOK;
}

/** Append attribute specifier to attribute specifier list.
 *
 * @param aslist Attribute specifier list
 * @param aspec Attribute specifier
 */
void ast_aslist_append(ast_aslist_t *aslist, ast_aspec_t *aspec)
{
	aspec->aslist = aslist;
	list_append(&aspec->laslist, &aslist->aspecs);
}

/** Return first attribute specifier in attribute specifier list.
 *
 * @param aslist Attribute specifier list
 * @return First attribute specifier or @c NULL
 */
ast_aspec_t *ast_aslist_first(ast_aslist_t *aslist)
{
	link_t *link;

	link = list_first(&aslist->aspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_t, laslist);
}

/** Return next attribute specifier in attribute specifier list.
 *
 * @param aspec Current attribute specifier
 * @return Next attribute specifier or @c NULL
 */
ast_aspec_t *ast_aslist_next(ast_aspec_t *aspec)
{
	link_t *link;

	link = list_next(&aspec->laslist, &aspec->aslist->aspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_t, laslist);
}

/** Return last attribute specifier in attribute specifier list.
 *
 * @param aslist Attribute specifier list
 * @return Last attribute specifier or @c NULL
 */
ast_aspec_t *ast_aslist_last(ast_aslist_t *aslist)
{
	link_t *link;

	link = list_last(&aslist->aspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_t, laslist);
}

/** Return previous attribute specifier in attribute specifier list.
 *
 * @param aspec Current attribute specifier
 * @return Previous attribute specifier or @c NULL
 */
ast_aspec_t *ast_aslist_prev(ast_aspec_t *aspec)
{
	link_t *link;

	link = list_prev(&aspec->laslist, &aspec->aslist->aspecs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_t, laslist);
}

/** Print AST attribute specifier list.
 *
 * @param aslist Attribute specifier list
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_aslist_print(ast_aslist_t *aslist, FILE *f)
{
	ast_aspec_t *aspec;
	bool first;
	int rc;

	if (fprintf(f, "aslist(") < 0)
		return EIO;

	first = true;
	aspec = ast_aslist_first(aslist);
	while (aspec != NULL) {
		if (!first) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}

		if (fprintf(f, "aspec(") < 0)
			return EIO;

		rc = ast_aspec_print(aspec, f);
		if (rc != EOK)
			return rc;

		if (fprintf(f, ")") < 0)
			return EIO;

		first = false;
		aspec = ast_aslist_next(aspec);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST attribute specifier list.
 *
 * @param aslist Attribute specifier list
 */
static void ast_aslist_destroy(ast_aslist_t *aslist)
{
	ast_aspec_t *aspec;

	if (aslist == NULL)
		return;

	aspec = ast_aslist_first(aslist);
	while (aspec != NULL) {
		list_remove(&aspec->laslist);
		ast_aspec_destroy(aspec);

		aspec = ast_aslist_first(aslist);
	}

	free(aslist);
}

/** Get first token of AST attribute specifier list.
 *
 * @param aslist Attribute specifier list
 * @return First token or @c NULL
 */
static ast_tok_t *ast_aslist_first_tok(ast_aslist_t *aslist)
{
	ast_aspec_t *aspec;

	aspec = ast_aslist_first(aslist);
	if (aspec == NULL)
		return NULL;

	return ast_aspec_first_tok(aspec);
}

/** Get last token of AST attribute specifier list.
 *
 * @param aslist Attribute specifier list
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_aslist_last_tok(ast_aslist_t *aslist)
{
	ast_aspec_t *aspec;

	aspec = ast_aslist_last(aslist);
	if (aspec == NULL)
		return NULL;

	return ast_aspec_last_tok(aspec);
}

/** Create AST attribute specifier.
 *
 * @param raspec Place to store pointer to new attribute specifier
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_aspec_create(ast_aspec_t **raspec)
{
	ast_aspec_t *aspec;

	aspec = calloc(1, sizeof(ast_aspec_t));
	if (aspec == NULL)
		return ENOMEM;

	list_initialize(&aspec->attrs);

	aspec->node.ext = aspec;
	aspec->node.ntype = ant_aspec;

	*raspec = aspec;
	return EOK;
}

/** Append attribute to attribute specifier.
 *
 * @param aspec Attribute specifier
 * @param attr Attribute
 */
void ast_aspec_append(ast_aspec_t *aspec, ast_aspec_attr_t *attr)
{
	attr->aspec = aspec;
	list_append(&attr->lspec, &aspec->attrs);
}

/** Return first attribute in attribute specifier.
 *
 * @param aspec Attribute specifier
 * @return First attribute or @c NULL
 */
ast_aspec_attr_t *ast_aspec_first(ast_aspec_t *aspec)
{
	link_t *link;

	link = list_first(&aspec->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_attr_t, lspec);
}

/** Return next attribute in attribute specifier.
 *
 * @param attr Current attribute
 * @return Next attribute or @c NULL
 */
ast_aspec_attr_t *ast_aspec_next(ast_aspec_attr_t *attr)
{
	link_t *link;

	link = list_next(&attr->lspec, &attr->aspec->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_attr_t, lspec);
}

/** Return last attribute in attribute specifier.
 *
 * @param aspec Attribute specifier
 * @return Last attribute or @c NULL
 */
ast_aspec_attr_t *ast_aspec_last(ast_aspec_t *aspec)
{
	link_t *link;

	link = list_last(&aspec->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_attr_t, lspec);
}

/** Return previous attribute in attribute specifier.
 *
 * @param attr Current attribute
 * @return Previous attribute or @c NULL
 */
ast_aspec_attr_t *ast_aspec_prev(ast_aspec_attr_t *attr)
{
	link_t *link;

	link = list_prev(&attr->lspec, &attr->aspec->attrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_attr_t, lspec);
}

/** Print AST attribute specifier.
 *
 * @param aspec Attribute specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_aspec_print(ast_aspec_t *aspec, FILE *f)
{
	ast_aspec_attr_t *attr;
	ast_aspec_param_t *param;
	bool first;
	bool first_p;

	if (fprintf(f, "aspec(") < 0)
		return EIO;

	first = true;
	attr = ast_aspec_first(aspec);
	while (attr != NULL) {
		if (!first) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}

		if (fprintf(f, "attr(") < 0)
			return EIO;

		first_p = true;
		param = ast_aspec_attr_first(attr);
		while (param != NULL) {
			if (!first_p) {
				if (fprintf(f, ", ") < 0)
					return EIO;
			}
			if (fprintf(f, "param") < 0)
				return EIO;

			param = ast_aspec_attr_next(param);
		}

		if (fprintf(f, ")") < 0)
			return EIO;

		first = false;
		attr = ast_aspec_next(attr);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST attribute specifier.
 *
 * @param aspec Attribute specifier
 */
static void ast_aspec_destroy(ast_aspec_t *aspec)
{
	ast_aspec_attr_t *attr;

	if (aspec == NULL)
		return;

	attr = ast_aspec_first(aspec);
	while (attr != NULL) {
		list_remove(&attr->lspec);
		ast_aspec_attr_destroy(attr);

		attr = ast_aspec_first(aspec);
	}

	free(aspec);
}

/** Get first token of AST attribute specifier.
 *
 * @param aspec Attribute specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_aspec_first_tok(ast_aspec_t *aspec)
{
	return &aspec->tattr;
}

/** Get last token of AST attribute specifier.
 *
 * @param aspec Attribute specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_aspec_last_tok(ast_aspec_t *aspec)
{
	return &aspec->trparen2;
}

/** Create attribute.
 *
 * @param rattr Place to store pointer to new attribute
 * @return EOK on success or error code
 */
int ast_aspec_attr_create(ast_aspec_attr_t **rattr)
{
	ast_aspec_attr_t *attr;

	attr = calloc(1, sizeof(ast_aspec_attr_t));
	if (attr == NULL)
		return ENOMEM;

	list_initialize(&attr->params);

	*rattr = attr;
	return EOK;
}

/** Destroy attribute.
 *
 * @param rattr Place to store pointer to new attribute
 * @return EOK on success or error code
 */
void ast_aspec_attr_destroy(ast_aspec_attr_t *attr)
{
	ast_aspec_param_t *param;

	if (attr == NULL)
		return;

	param = ast_aspec_attr_first(attr);
	while (param != NULL) {
		list_remove(&param->lattr);
		ast_tree_destroy(param->expr);
		free(param);
		param = ast_aspec_attr_first(attr);
	}

	free(attr);
}

/** Append parameter to attribute.
 *
 * @param attr Attribute
 * @param expr Parameter expression
 * @param dcomma Data for separating comma token (except for the last
 *               parameter)
 * @return EOK on success or error code
 */
int ast_aspec_attr_append(ast_aspec_attr_t *attr, ast_node_t *expr,
    void *dcomma)
{
	ast_aspec_param_t *param;

	param = calloc(1, sizeof(ast_aspec_param_t));
	if (param == NULL)
		return ENOMEM;

	param->attr = attr;
	list_append(&param->lattr, &attr->params);

	param->expr = expr;
	param->tcomma.data = dcomma;

	return EOK;
}

/** Return first attribute parameter.
 *
 * @param attr Attribute
 * @return First parameter or @c NULL
 */
ast_aspec_param_t *ast_aspec_attr_first(ast_aspec_attr_t *attr)
{
	link_t *link;

	link = list_first(&attr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_param_t, lattr);
}

/** Return next attribute parameter.
 *
 * @param param Current parameter
 * @return Next attribute or @c NULL
 */
ast_aspec_param_t *ast_aspec_attr_next(ast_aspec_param_t *param)
{
	link_t *link;

	link = list_next(&param->lattr, &param->attr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_param_t, lattr);
}

/** Return last attribute parameter.
 *
 * @param attr Attribute
 * @return Last parameter or @c NULL
 */
ast_aspec_param_t *ast_aspec_attr_last(ast_aspec_attr_t *attr)
{
	link_t *link;

	link = list_last(&attr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_param_t, lattr);
}

/** Return previous attribute parameter.
 *
 * @param param Current parameter
 * @return Previous parameter or @c NULL
 */
ast_aspec_param_t *ast_aspec_attr_prev(ast_aspec_param_t *param)
{
	link_t *link;

	link = list_prev(&param->lattr, &param->attr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_aspec_param_t, lattr);
}

/** Create AST macro attribute list.
 *
 * @param rmalist Place to store pointer to new macro attribute list
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_malist_create(ast_malist_t **rmalist)
{
	ast_malist_t *malist;

	malist = calloc(1, sizeof(ast_malist_t));
	if (malist == NULL)
		return ENOMEM;

	list_initialize(&malist->mattrs);

	malist->node.ext = malist;
	malist->node.ntype = ant_malist;

	*rmalist = malist;
	return EOK;
}

/** Append attribute specifier to macro attribute list.
 *
 * @param malist Macro attribute list
 * @param mattr Macro attribute
 */
void ast_malist_append(ast_malist_t *malist, ast_mattr_t *mattr)
{
	mattr->malist = malist;
	list_append(&mattr->lmattrs, &malist->mattrs);
}

/** Return first macro attribute in macro attribute list.
 *
 * @param malist Macro attribute list
 * @return First macro attribute or @c NULL
 */
ast_mattr_t *ast_malist_first(ast_malist_t *malist)
{
	link_t *link;

	link = list_first(&malist->mattrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_t, lmattrs);
}

/** Return next macro attribute in macro attribute list.
 *
 * @param mattr Current maccro attribute
 * @return Next macro attribute or @c NULL
 */
ast_mattr_t *ast_malist_next(ast_mattr_t *mattr)
{
	link_t *link;

	link = list_next(&mattr->lmattrs, &mattr->malist->mattrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_t, lmattrs);
}

/** Return last macro attribute in macro attribute list.
 *
 * @param malist Macro attribute list
 * @return First macro attribute or @c NULL
 */
ast_mattr_t *ast_malist_last(ast_malist_t *malist)
{
	link_t *link;

	link = list_last(&malist->mattrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_t, lmattrs);
}

/** Return previous macro attribute in macro attribute list.
 *
 * @param mattr Current maccro attribute
 * @return Previous macro attribute or @c NULL
 */
ast_mattr_t *ast_malist_prev(ast_mattr_t *mattr)
{
	link_t *link;

	link = list_prev(&mattr->lmattrs, &mattr->malist->mattrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_t, lmattrs);
}

/** Print AST macro attribute list.
 *
 * @param malist Macro attribute list
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_malist_print(ast_malist_t *malist, FILE *f)
{
	ast_mattr_t *mattr;
	bool first;
	int rc;

	if (fprintf(f, "malist(") < 0)
		return EIO;

	first = true;
	mattr = ast_malist_first(malist);
	while (mattr != NULL) {
		if (!first) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}

		if (fprintf(f, "mattr(") < 0)
			return EIO;

		rc = ast_mattr_print(mattr, f);
		if (rc != EOK)
			return rc;

		if (fprintf(f, ")") < 0)
			return EIO;

		first = false;
		mattr = ast_malist_next(mattr);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST macro attribute list.
 *
 * @param malist Macro attribute list
 */
static void ast_malist_destroy(ast_malist_t *malist)
{
	ast_mattr_t *mattr;

	if (malist == NULL)
		return;

	mattr = ast_malist_first(malist);
	while (mattr != NULL) {
		list_remove(&mattr->lmattrs);
		ast_mattr_destroy(mattr);

		mattr = ast_malist_first(malist);
	}

	free(malist);
}

/** Get first token of AST macro attribute list.
 *
 * @param malist Macro attribute list
 * @return First token or @c NULL
 */
static ast_tok_t *ast_malist_first_tok(ast_malist_t *malist)
{
	ast_mattr_t *mattr;

	mattr = ast_malist_first(malist);
	if (mattr == NULL)
		return NULL;

	return ast_mattr_first_tok(mattr);
}

/** Get last token of AST macro attribute list.
 *
 * @param malist Macro attribute list
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_malist_last_tok(ast_malist_t *malist)
{
	ast_mattr_t *mattr;

	mattr = ast_malist_last(malist);
	if (mattr == NULL)
		return NULL;

	return ast_mattr_last_tok(mattr);
}

/** Create AST macro attribute.
 *
 * @param rmattr Place to store pointer to new macro attribute
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_mattr_create(ast_mattr_t **rmattr)
{
	ast_mattr_t *mattr;

	mattr = calloc(1, sizeof(ast_mattr_t));
	if (mattr == NULL)
		return ENOMEM;

	list_initialize(&mattr->params);

	mattr->node.ext = mattr;
	mattr->node.ntype = ant_mattr;

	*rmattr = mattr;
	return EOK;
}

/** Append attribute to macro attribute.
 *
 * @param mattr Macro attribute
 * @param expr Parameter expression
 * @param dcomma Data for ',' token or @c NULL
 */
int ast_mattr_append(ast_mattr_t *mattr, ast_node_t *expr, void *dcomma)
{
	ast_mattr_param_t *param;

	param = calloc(1, sizeof(ast_mattr_param_t));
	if (param == NULL)
		return ENOMEM;

	param->mattr = mattr;
	param->expr = expr;
	param->tcomma.data = dcomma;

	list_append(&param->lparams, &mattr->params);

	return EOK;
}

/** Return first parameter in macro attribute.
 *
 * @param mattr Macro attribute
 * @return First parameter or @c NULL
 */
ast_mattr_param_t *ast_mattr_first(ast_mattr_t *mattr)
{
	link_t *link;

	link = list_first(&mattr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_param_t, lparams);
}

/** Return next parameter in macro attribute.
 *
 * @param attr Current attribute
 * @return Next parameter or @c NULL
 */
ast_mattr_param_t *ast_mattr_next(ast_mattr_param_t *param)
{
	link_t *link;

	link = list_next(&param->lparams, &param->mattr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_param_t, lparams);
}

/** Return last parameter in macro attribute.
 *
 * @param mattr Macro attribute
 * @return Last parameter or @c NULL
 */
ast_mattr_param_t *ast_mattr_last(ast_mattr_t *mattr)
{
	link_t *link;

	link = list_last(&mattr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_param_t, lparams);
}

/** Return previous parameter in macro attribute.
 *
 * @param attr Current parameter
 * @return Previous parameter or @c NULL
 */
ast_mattr_param_t *ast_mattr_prev(ast_mattr_param_t *param)
{
	link_t *link;

	link = list_prev(&param->lparams, &param->mattr->params);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_mattr_param_t, lparams);
}

/** Print AST macro attribute.
 *
 * @param mattr Macro attribute
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_mattr_print(ast_mattr_t *mattr, FILE *f)
{
	ast_mattr_param_t *param;
	bool first;

	if (fprintf(f, "mattr(") < 0)
		return EIO;

	first = true;
	param = ast_mattr_first(mattr);
	while (param != NULL) {
		if (!first) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}

		if (fprintf(f, "param") < 0)
			return EIO;

		first = false;
		param = ast_mattr_next(param);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST macro attribute.
 *
 * @param mattr Macro attribute
 */
static void ast_mattr_destroy(ast_mattr_t *mattr)
{
	ast_mattr_param_t *param;

	if (mattr == NULL)
		return;

	param = ast_mattr_first(mattr);
	while (param != NULL) {
		list_remove(&param->lparams);
		ast_tree_destroy(param->expr);
		free(param);

		param = ast_mattr_first(mattr);
	}

	free(mattr);
}

/** Get first token of AST macro attribute.
 *
 * @param mattr Macro attribute
 * @return First token or @c NULL
 */
static ast_tok_t *ast_mattr_first_tok(ast_mattr_t *mattr)
{
	return &mattr->tname;
}

/** Get last token of AST macro attribute.
 *
 * @param mattr Macro attribute
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_mattr_last_tok(ast_mattr_t *mattr)
{
	return &mattr->trparen;
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

	if (sqlist == NULL)
		return;

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

/** Determine if specifier-qualifier list contains a record type specifier.
 *
 * @param sqlist Specifier-qualifier list
 * @return @c true if @a sqlist contains a record type specifier, @c false
 *         otherwise
 */
bool ast_sqlist_has_tsrecord(ast_sqlist_t *sqlist)
{
	ast_node_t *sq;

	sq = ast_sqlist_first(sqlist);
	while (sq != NULL) {
		if (sq->ntype == ant_tsrecord)
			return true;
		sq = ast_sqlist_next(sq);
	}

	return false;
}

/** Create AST type qualifier list.
 *
 * @param rtqlist Place to store pointer to new type qualifier list
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_tqlist_create(ast_tqlist_t **rtqlist)
{
	ast_tqlist_t *tqlist;

	tqlist = calloc(1, sizeof(ast_tqlist_t));
	if (tqlist == NULL)
		return ENOMEM;

	list_initialize(&tqlist->elems);

	tqlist->node.ext = tqlist;
	tqlist->node.ntype = ant_tqlist;

	*rtqlist = tqlist;
	return EOK;
}

/** Append element to type qualifier list.
 *
 * @param tqlist Type qualifier list
 * @param elem Specifier or qualifier
 */
void ast_tqlist_append(ast_tqlist_t *tqlist, ast_node_t *elem)
{
	list_append(&elem->llist, &tqlist->elems);
	elem->lnode = &tqlist->node;
}

/** Return first element in type qualifier list.
 *
 * @param tqlist Type qualifier list
 * @return First element or @c NULL
 */
ast_node_t *ast_tqlist_first(ast_tqlist_t *tqlist)
{
	link_t *link;

	link = list_first(&tqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return next element in type qualifier list.
 *
 * @param node Current element
 * @return Next element or @c NULL
 */
ast_node_t *ast_tqlist_next(ast_node_t *node)
{
	link_t *link;
	ast_tqlist_t *tqlist;

	assert(node->lnode->ntype == ant_tqlist);
	tqlist = (ast_tqlist_t *) node->lnode->ext;

	link = list_next(&node->llist, &tqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return last element in type qualifier list.
 *
 * @param tqlist Type qualifier list
 * @return Last element or @c NULL
 */
ast_node_t *ast_tqlist_last(ast_tqlist_t *tqlist)
{
	link_t *link;

	link = list_last(&tqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Return previous element in type qualifier list.
 *
 * @param node Current element
 * @return Previous element or @c NULL
 */
ast_node_t *ast_tqlist_prev(ast_node_t *node)
{
	link_t *link;
	ast_tqlist_t *tqlist;

	assert(node->lnode->ntype == ant_tqlist);
	tqlist = (ast_tqlist_t *) node->lnode->ext;

	link = list_prev(&node->llist, &tqlist->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_node_t, llist);
}

/** Print AST type qualifier list.
 *
 * @param tqlist Type qualifier list
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tqlist_print(ast_tqlist_t *tqlist, FILE *f)
{
	ast_node_t *elem;
	int rc;

	if (fprintf(f, "tqlist(") < 0)
		return EIO;

	elem = ast_tqlist_first(tqlist);
	while (elem != NULL) {
		rc = ast_tree_print(elem, f);
		if (rc != EOK)
			return rc;

		elem = ast_tqlist_next(elem);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST type qualifier list.
 *
 * @param tqlist Type qualifier list
 */
static void ast_tqlist_destroy(ast_tqlist_t *tqlist)
{
	ast_node_t *elem;

	if (tqlist == NULL)
		return;

	elem = ast_tqlist_first(tqlist);
	while (elem != NULL) {
		list_remove(&elem->llist);
		ast_tree_destroy(elem);
		elem = ast_tqlist_first(tqlist);
	}

	free(tqlist);
}

/** Get first token of AST type qualifier list.
 *
 * @param tqlist Type qualifier list
 * @return First token or @c NULL
 */
static ast_tok_t *ast_tqlist_first_tok(ast_tqlist_t *tqlist)
{
	return ast_tree_first_tok(ast_tqlist_first(tqlist));
}

/** Get last token of AST type qualifier list.
 *
 * @param tqlist Type qualifier list
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_tqlist_last_tok(ast_tqlist_t *tqlist)
{
	return ast_tree_last_tok(ast_tqlist_last(tqlist));
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

	if (dspecs == NULL)
		return;

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
	ast_tqlist_destroy(adptr->tqlist);
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
 * @param aslist Attribute specifier list or @c NULL
 * @param dcomma Data for comma token or @c NULL
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dfun_append(ast_dfun_t *dfun, ast_dspecs_t *dspecs, ast_node_t *decl,
    ast_aslist_t *aslist, void *dcomma)
{
	ast_dfun_arg_t *arg;

	arg = calloc(1, sizeof(ast_dfun_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->dspecs = dspecs;
	arg->decl = decl;
	arg->aslist = aslist;
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

		if (arg->aslist != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;

			rc = ast_aslist_print(arg->aslist, f);
			if (rc != EOK)
				return rc;
		}

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
		ast_aslist_destroy(arg->aslist);
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
	ast_tree_destroy(darray->asize);
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
 * @param have_bitwidth @c true if we havee dcolon and bitwidth
 * @param dcolon Colon token data or @c NULL
 * @param bitwidth Bit width expression or @c NULL
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dlist_append(ast_dlist_t *dlist, void *dcomma, ast_node_t *decl,
    bool have_bitwidth, void *dcolon, ast_node_t *bitwidth)
{
	ast_dlist_entry_t *entry;

	entry = calloc(1, sizeof(ast_dlist_entry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->tcomma.data = dcomma;
	entry->decl = decl;
	entry->have_bitwidth = have_bitwidth;
	entry->tcolon.data = dcolon;
	entry->bitwidth = bitwidth;

	entry->dlist = dlist;
	list_append(&entry->ldlist, &dlist->decls);
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
 * @param entry Current entry
 * @return Next entry or @c NULL
 */
ast_dlist_entry_t *ast_dlist_next(ast_dlist_entry_t *entry)
{
	link_t *link;

	link = list_next(&entry->ldlist, &entry->dlist->decls);
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
 * @param entry Current entry
 * @return Previous entry or @c NULL
 */
ast_dlist_entry_t *ast_dlist_prev(ast_dlist_entry_t *entry)
{
	link_t *link;

	link = list_prev(&entry->ldlist, &entry->dlist->decls);
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

	if (dlist == NULL)
		return;

	entry = ast_dlist_first(dlist);
	while (entry != NULL) {
		list_remove(&entry->ldlist);
		ast_tree_destroy(entry->decl);
		ast_tree_destroy(entry->bitwidth);
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

/** Create AST init-declarator list.
 *
 * @param rdfun Place to store pointer to new init-declarator list
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_idlist_create(ast_idlist_t **ridlist)
{
	ast_idlist_t *idlist;

	idlist = calloc(1, sizeof(ast_idlist_t));
	if (idlist == NULL)
		return ENOMEM;

	list_initialize(&idlist->idecls);

	idlist->node.ext = idlist;
	idlist->node.ntype = ant_idlist;

	*ridlist = idlist;
	return EOK;
}

/** Append entry to init-declarator list.
 *
 * @param idlist Init-declarator list
 * @param dcomma Data for preceding comma token or @c NULL
 * @param decl Declarator
 * @param regassign Register assignment or @c NULL
 * @param aslist Attribute specifier list or @c NULL
 * @param have_init @c true if we have an initializer
 * @param dassign Data for assign token or @c NULL
 * @param init Initializer
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_idlist_append(ast_idlist_t *idlist, void *dcomma, ast_node_t *decl,
    ast_regassign_t *regassign, ast_aslist_t *aslist, bool have_init,
    void *dassign, ast_node_t *init)
{
	ast_idlist_entry_t *entry;

	entry = calloc(1, sizeof(ast_idlist_entry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->tcomma.data = dcomma;
	entry->decl = decl;
	entry->regassign = regassign;
	entry->aslist = aslist;
	entry->have_init = have_init;
	entry->tassign.data = dassign;
	entry->init = init;

	entry->idlist = idlist;
	list_append(&entry->lidlist, &idlist->idecls);
	return EOK;
}

/** Return first init-declarator list entry.
 *
 * @param idlist Init-declarator list
 * @return First entry or @c NULL
 */
ast_idlist_entry_t *ast_idlist_first(ast_idlist_t *idlist)
{
	link_t *link;

	link = list_first(&idlist->idecls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_idlist_entry_t, lidlist);
}

/** Return next init-declarator list entry.
 *
 * @param entry Current entry
 * @return Next entry or @c NULL
 */
ast_idlist_entry_t *ast_idlist_next(ast_idlist_entry_t *entry)
{
	link_t *link;

	link = list_next(&entry->lidlist, &entry->idlist->idecls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_idlist_entry_t, lidlist);
}

/** Return last init-declarator list entry.
 *
 * @param idlist Init-declarator list
 * @return Last entry or @c NULL
 */
ast_idlist_entry_t *ast_idlist_last(ast_idlist_t *idlist)
{
	link_t *link;

	link = list_last(&idlist->idecls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_idlist_entry_t, lidlist);
}

/** Return previous init-declarator list entry.
 *
 * @param entry Current entry
 * @return Previous entry or @c NULL
 */
ast_idlist_entry_t *ast_idlist_prev(ast_idlist_entry_t *entry)
{
	link_t *link;

	link = list_prev(&entry->lidlist, &entry->idlist->idecls);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_idlist_entry_t, lidlist);
}

/** Print AST init-declarator list.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_idlist_print(ast_idlist_t *idlist, FILE *f)
{
	ast_idlist_entry_t *entry;
	int rc;

	if (fprintf(f, "idlist(") < 0)
		return EIO;

	entry = ast_idlist_first(idlist);
	while (entry != NULL) {
		rc = ast_tree_print(entry->decl, f);
		if (rc != EOK)
			return EIO;

		if (entry->aslist != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;

			rc = ast_aslist_print(entry->aslist, f);
			if (rc != EOK)
				return EIO;
		}

		if (entry->have_init) {
			if (fprintf(f, " = ") < 0)
				return EIO;

			rc = ast_tree_print(entry->init, f);
			if (rc != EOK)
				return EIO;
		}

		entry = ast_idlist_next(entry);

		if (entry != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST init-declarator list.
 *
 * @param block Block
 */
static void ast_idlist_destroy(ast_idlist_t *idlist)
{
	ast_idlist_entry_t *entry;

	if (idlist == NULL)
		return;

	entry = ast_idlist_first(idlist);
	while (entry != NULL) {
		list_remove(&entry->lidlist);
		ast_tree_destroy(entry->decl);
		ast_regassign_destroy(entry->regassign);
		ast_aslist_destroy(entry->aslist);
		ast_tree_destroy(entry->init);
		free(entry);

		entry = ast_idlist_first(idlist);
	}

	free(idlist);
}

/** Get first token of AST init-declarator list.
 *
 * @param idlist Init-declarator list
 * @return First token or @c NULL
 */
static ast_tok_t *ast_idlist_first_tok(ast_idlist_t *idlist)
{
	ast_idlist_entry_t *entry;
	ast_tok_t *tok;

	/* Try first token of first entry */
	entry = ast_idlist_first(idlist);
	tok = ast_tree_first_tok(entry->decl);
	if (tok != NULL)
		return tok;

	/* See if there are at least two entries */
	entry = ast_idlist_next(entry);
	if (entry == NULL)
		return NULL;

	/* Return separating comma */
	return &entry->tcomma;
}

/** Get last token of AST init-declarator list.
 *
 * @param idlist Init-declarator list
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_idlist_last_tok(ast_idlist_t *idlist)
{
	ast_idlist_entry_t *entry;
	ast_tok_t *tok;

	/* Try last token of last entry */
	entry = ast_idlist_last(idlist);
	tok = ast_tree_last_tok(entry->decl);
	if (tok != NULL)
		return tok;

	/* See if there are at least two entries */
	if (ast_idlist_prev(entry) == NULL)
		return NULL;

	/* Return separating comma */
	return &entry->tcomma;
}

/** Create AST type name.
 *
 * @param rtypename Place to store pointer to new type name
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_typename_create(ast_typename_t **rtypename)
{
	ast_typename_t *atypename;

	atypename = calloc(1, sizeof(ast_typename_t));
	if (atypename == NULL)
		return ENOMEM;

	atypename->node.ext = atypename;
	atypename->node.ntype = ant_typename;

	*rtypename = atypename;
	return EOK;
}

/** Print AST type name.
 *
 * @param atypename Type name
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_typename_print(ast_typename_t *atypename, FILE *f)
{
	int rc;

	(void) atypename;

	if (fprintf(f, "typename(") < 0)
		return EIO;

	rc = ast_dspecs_print(atypename->dspecs, f);
	if (rc != EOK)
		return rc;

	rc = ast_tree_print(atypename->decl, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST type name.
 *
 * @param atypename Type name
 */
static void ast_typename_destroy(ast_typename_t *atypename)
{
	if (atypename == NULL)
		return;

	ast_dspecs_destroy(atypename->dspecs);
	ast_tree_destroy(atypename->decl);
	free(atypename);
}

/** Get first token of AST type name.
 *
 * @param atypename Type name
 * @return First token or @c NULL
 */
static ast_tok_t *ast_typename_first_tok(ast_typename_t *atypename)
{
	return ast_dspecs_first_tok(atypename->dspecs);
}

/** Get last token of AST type name
 *
 * @param eaddr Type name
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_typename_last_tok(ast_typename_t *atypename)
{
	return ast_tree_last_tok(atypename->decl);
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
	while (lit != NULL) {
		if (fprintf(f, "lit") < 0)
			return EIO;

		lit = ast_estring_next(lit);

		if (lit != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
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

/** Create AST concatenation expression.
 *
 * @param reconcat Place to store pointer to new concatenation expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_econcat_create(ast_econcat_t **reconcat)
{
	ast_econcat_t *econcat;

	econcat = calloc(1, sizeof(ast_econcat_t));
	if (econcat == NULL)
		return ENOMEM;

	econcat->node.ext = econcat;
	econcat->node.ntype = ant_econcat;
	list_initialize(&econcat->elems);

	*reconcat = econcat;
	return EOK;
}

/** Append literal to concatenation expression.
 *
 * @param econcat Concatenation expression
 * @param bexpr Base expression
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_econcat_append(ast_econcat_t *econcat, ast_node_t *bexpr)
{
	ast_econcat_elem_t *elem;

	elem = calloc(1, sizeof(ast_econcat_elem_t));
	if (elem == NULL)
		return ENOMEM;

	elem->bexpr = bexpr;
	elem->econcat = econcat;

	list_append(&elem->lelems, &econcat->elems);
	return EOK;
}

/** Return first element in concatenation expression.
 *
 * @param econcat Concatenation expression
 * @return First element or @c NULL
 */
ast_econcat_elem_t *ast_econcat_first(ast_econcat_t *econcat)
{
	link_t *link;

	link = list_first(&econcat->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_econcat_elem_t, lelems);
}

/** Return next element in concatenation expression.
 *
 * @param elem Current element
 * @return Next element or @c NULL
 */
ast_econcat_elem_t *ast_econcat_next(ast_econcat_elem_t *elem)
{
	link_t *link;

	link = list_next(&elem->lelems, &elem->econcat->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_econcat_elem_t, lelems);
}

/** Return last element in concatenation expression.
 *
 * @param econcat Concatenation expression
 * @return First element or @c NULL
 */
ast_econcat_elem_t *ast_econcat_last(ast_econcat_t *econcat)
{
	link_t *link;

	link = list_last(&econcat->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_econcat_elem_t, lelems);
}

/** Print AST concatenation expression.
 *
 * @param econcat Concatenation expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_econcat_print(ast_econcat_t *econcat, FILE *f)
{
	ast_econcat_elem_t *elem;

	if (fprintf(f, "econcat(") < 0)
		return EIO;

	elem = ast_econcat_first(econcat);
	while (elem != NULL) {
		if (fprintf(f, "elem") < 0)
			return EIO;

		elem = ast_econcat_next(elem);

		if (elem != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST concatenation expression.
 *
 * @param econcat Concatenation expression
 */
static void ast_econcat_destroy(ast_econcat_t *econcat)
{
	ast_econcat_elem_t *elem;

	elem = ast_econcat_first(econcat);
	while (elem != NULL) {
		list_remove(&elem->lelems);
		ast_tree_destroy(elem->bexpr);
		free(elem);
		elem = ast_econcat_first(econcat);
	}

	free(econcat);
}

/** Get first token of AST concatenation expression.
 *
 * @param econcat Concatenation expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_econcat_first_tok(ast_econcat_t *econcat)
{
	ast_econcat_elem_t *elem;

	elem = ast_econcat_first(econcat);
	return ast_tree_first_tok(elem->bexpr);
}

/** Get last token of AST concatenation expression.
 *
 * @param econcat Concatenation expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_econcat_last_tok(ast_econcat_t *econcat)
{
	ast_econcat_elem_t *elem;

	elem = ast_econcat_last(econcat);
	return ast_tree_last_tok(elem->bexpr);
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

/** Create AST call expression.
 *
 * @param recall Place to store pointer to new call expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ecall_create(ast_ecall_t **recall)
{
	ast_ecall_t *ecall;

	ecall = calloc(1, sizeof(ast_ecall_t));
	if (ecall == NULL)
		return ENOMEM;

	ecall->node.ext = ecall;
	ecall->node.ntype = ant_ecall;
	list_initialize(&ecall->args);

	*recall = ecall;
	return EOK;
}

/** Append entry to call argument list.
 *
 * @param ecall Call expression
 * @param dcomma Data for preceding comma token or @c NULL
 * @param arg Argument (expression or type name)
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ecall_append(ast_ecall_t *ecall, void *dcomma, ast_node_t *arg)
{
	ast_ecall_arg_t *earg;

	earg = calloc(1, sizeof(ast_ecall_arg_t));
	if (earg == NULL)
		return ENOMEM;

	earg->tcomma.data = dcomma;
	earg->arg = arg;

	earg->ecall = ecall;
	list_append(&earg->lcall, &ecall->args);
	return EOK;
}

/** Return first argument in call expression.
 *
 * @param ecall Call expression
 * @return First argument or @c NULL
 */
ast_ecall_arg_t *ast_ecall_first(ast_ecall_t *ecall)
{
	link_t *link;

	link = list_first(&ecall->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_ecall_arg_t, lcall);
}

/** Return next argument in call expression.
 *
 * @param arg Call argument
 * @return Next argument or @c NULL
 */
ast_ecall_arg_t *ast_ecall_next(ast_ecall_arg_t *arg)
{
	link_t *link;

	link = list_next(&arg->lcall, &arg->ecall->args);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_ecall_arg_t, lcall);
}

/** Print AST call expression.
 *
 * @param ecall Call expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_ecall_print(ast_ecall_t *ecall, FILE *f)
{
	int rc;
	ast_ecall_arg_t *arg;

	(void) ecall;

	if (fprintf(f, "ecall(") < 0)
		return EIO;

	rc = ast_tree_print(ecall->fexpr, f);
	if (rc != EOK)
		return rc;

	arg = ast_ecall_first(ecall);
	while (arg != NULL) {
		if (fprintf(f, ", ") < 0)
			return EIO;

		rc = ast_tree_print(arg->arg, f);
		if (rc != EOK)
			return rc;

		arg = ast_ecall_next(arg);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST call expression.
 *
 * @param ecall Call expression
 */
static void ast_ecall_destroy(ast_ecall_t *ecall)
{
	ast_ecall_arg_t *arg;

	ast_tree_destroy(ecall->fexpr);

	arg = ast_ecall_first(ecall);
	while (arg != NULL) {
		list_remove(&arg->lcall);
		ast_tree_destroy(arg->arg);
		free(arg);

		arg = ast_ecall_first(ecall);
	}

	free(ecall);
}

/** Get first token of AST call expression.
 *
 * @param ecall Call expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_ecall_first_tok(ast_ecall_t *ecall)
{
	return ast_tree_first_tok(ecall->fexpr);
}

/** Get last token of AST call expression.
 *
 * @param ecall Call expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ecall_last_tok(ast_ecall_t *ecall)
{
	return &ecall->trparen;
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
 * @param eaddr Address expression
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
 * @param eaddr Address expression
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

	if (esizeof->bexpr != NULL) {
		rc = ast_tree_print(esizeof->bexpr, f);
		if (rc != EOK)
			return rc;
	}

	if (esizeof->atypename != NULL) {
		rc = ast_typename_print(esizeof->atypename, f);
		if (rc != EOK)
			return rc;
	}

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
	ast_typename_destroy(esizeof->atypename);
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

/** Get last token of AST sizeof expression.
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
	ast_dspecs_destroy(ecast->dspecs);
	ast_tree_destroy(ecast->decl);
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

/** Get last token of AST cast expression.
 *
 * @param ecast Cast expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ecast_last_tok(ast_ecast_t *ecast)
{
	return ast_tree_last_tok(ecast->bexpr);
}


/** Create AST compound literal expression.
 *
 * @param recliteral Place to store pointer to new compound literal expression
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_ecliteral_create(ast_ecliteral_t **recliteral)
{
	ast_ecliteral_t *ecliteral;

	ecliteral = calloc(1, sizeof(ast_ecliteral_t));
	if (ecliteral == NULL)
		return ENOMEM;

	ecliteral->node.ext = ecliteral;
	ecliteral->node.ntype = ant_ecliteral;

	*recliteral = ecliteral;
	return EOK;
}

/** Print AST compound literal expression.
 *
 * @param ecliteral Compound literal expression
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_ecliteral_print(ast_ecliteral_t *ecliteral, FILE *f)
{
	int rc;

	(void) ecliteral;

	if (fprintf(f, "ecliteral(") < 0)
		return EIO;

	rc = ast_cinit_print(ecliteral->cinit, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Destroy AST compound literal expression.
 *
 * @param ecliteral Compound literal expression
 */
static void ast_ecliteral_destroy(ast_ecliteral_t *ecliteral)
{
	ast_dspecs_destroy(ecliteral->dspecs);
	ast_tree_destroy(ecliteral->decl);
	ast_cinit_destroy(ecliteral->cinit);
	free(ecliteral);
}

/** Get first token of AST compound literal expression.
 *
 * @param ecliteral Compound literal expression
 * @return First token or @c NULL
 */
static ast_tok_t *ast_ecliteral_first_tok(ast_ecliteral_t *ecliteral)
{
	return &ecliteral->tlparen;
}

/** Get last token of AST compound literal expression.
 *
 * @param ecliteral Compound literal expression
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_ecliteral_last_tok(ast_ecliteral_t *ecliteral)
{
	return ast_cinit_last_tok(ecliteral->cinit);
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
	if (elnot != NULL)
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

/** Create AST compound initializer.
 *
 * @param rcinit Place to store pointer to new compound initializer
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_cinit_create(ast_cinit_t **rcinit)
{
	ast_cinit_t *cinit;

	cinit = calloc(1, sizeof(ast_cinit_t));
	if (cinit == NULL)
		return ENOMEM;

	list_initialize(&cinit->elems);

	cinit->node.ext = cinit;
	cinit->node.ntype = ant_cinit;

	*rcinit = cinit;
	return EOK;
}

/** Append element to compound initializer.
 *
 * @param cinit Compound initializer
 * @param elem Compound initializer element
 */
void ast_cinit_append(ast_cinit_t *cinit, ast_cinit_elem_t *elem)
{
	elem->cinit = cinit;
	list_append(&elem->lcinit, &cinit->elems);
}

/** Return first element in compound initializer.
 *
 * @param cinit Compound initializer
 * @return First element or @c NULL
 */
ast_cinit_elem_t *ast_cinit_first(ast_cinit_t *cinit)
{
	link_t *link;

	link = list_first(&cinit->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_cinit_elem_t, lcinit);
}

/** Return next element in compound initializer.
 *
 * @param elem Current element
 * @return Next element or @c NULL
 */
ast_cinit_elem_t *ast_cinit_next(ast_cinit_elem_t *elem)
{
	link_t *link;

	link = list_next(&elem->lcinit, &elem->cinit->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_cinit_elem_t, lcinit);
}

/** Create compound initializer element.
 *
 * @param relem Place to store pointer to new compound initializer element
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_cinit_elem_create(ast_cinit_elem_t **relem)
{
	ast_cinit_elem_t *elem;

	elem = calloc(1, sizeof(ast_cinit_elem_t));
	if (elem == NULL)
		return ENOMEM;

	list_initialize(&elem->accs);

	*relem = elem;
	return EOK;
}

/** Destroy AST compound initializer element.
 *
 * @param elem Compound initializer element
 */
void ast_cinit_elem_destroy(ast_cinit_elem_t *elem)
{
	ast_cinit_acc_t *acc;

	if (elem == NULL)
		return;

	acc = ast_cinit_elem_first(elem);
	while (acc != NULL) {
		if (acc->atype == aca_index)
			ast_tree_destroy(acc->index);
		list_remove(&acc->laccs);
		free(acc);
		acc = ast_cinit_elem_first(elem);
	}

	ast_tree_destroy(elem->init);
	free(elem);
}

/** Append index accessor to compound initializer element.
 *
 * @param elem Compound initializer element
 * @param dlbracket Left bracket token data
 * @param index Index expression
 * @param drbracket Right bracket token data
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_cinit_elem_append_index(ast_cinit_elem_t *elem, void *dlbracket,
    ast_node_t *index, void *drbracket)
{
	ast_cinit_acc_t *acc;

	acc = calloc(1, sizeof(ast_cinit_acc_t));
	if (acc == NULL)
		return ENOMEM;

	acc->atype = aca_index;
	acc->tlbracket.data = dlbracket;
	acc->index = index;
	acc->trbracket.data = drbracket;

	acc->elem = elem;
	list_append(&acc->laccs, &elem->accs);
	return EOK;
}

/** Append member accessor to compound initializer element.
 *
 * @param elem Compound initializer element
 * @param dperiod Period token data
 * @param dmember Member token data
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_cinit_elem_append_member(ast_cinit_elem_t *elem, void *dperiod,
    void *dmember)
{
	ast_cinit_acc_t *acc;

	acc = calloc(1, sizeof(ast_cinit_acc_t));
	if (acc == NULL)
		return ENOMEM;

	acc->atype = aca_member;
	acc->tperiod.data = dperiod;
	acc->tmember.data = dmember;

	acc->elem = elem;
	list_append(&acc->laccs, &elem->accs);
	return EOK;
}

/** Return first accessor in compound initializer element.
 *
 * @param elem Compound initializer element
 * @return First accessor or @c NULL
 */
ast_cinit_acc_t *ast_cinit_elem_first(ast_cinit_elem_t *elem)
{
	link_t *link;

	link = list_first(&elem->accs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_cinit_acc_t, laccs);
}

/** Return next accessor in compound initializer element.
 *
 * @param acc Current accessor
 * @return Next element or @c NULL
 */
ast_cinit_acc_t *ast_cinit_elem_next(ast_cinit_acc_t *acc)
{
	link_t *link;

	link = list_next(&acc->laccs, &acc->elem->accs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_cinit_acc_t, laccs);
}

/** Print AST compound initializer.
 *
 * @param cinit Compound initializer
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_cinit_print(ast_cinit_t *cinit, FILE *f)
{
	ast_cinit_elem_t *elem;
	ast_cinit_acc_t *acc;
	int rc;

	if (fprintf(f, "cinit(") < 0)
		return EIO;

	elem = ast_cinit_first(cinit);
	while (elem != NULL) {
		acc = ast_cinit_elem_first(elem);
		while (acc != NULL) {
			if (acc->atype == aca_index) {
				if (fprintf(f, "[") < 0)
					return EIO;
				rc = ast_tree_print(acc->index, f);
				if (rc != EOK)
					return rc;
				if (fprintf(f, "]") < 0)
					return EIO;
			} else {
				if (fprintf(f, ".member") < 0)
					return EIO;
			}

			acc = ast_cinit_elem_next(acc);
		}

		rc = ast_tree_print(elem->init, f);
		if (rc != EOK)
			return rc;

		elem = ast_cinit_next(elem);
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST compound initializer.
 *
 * @param cinit Compound initializer
 */
static void ast_cinit_destroy(ast_cinit_t *cinit)
{
	ast_cinit_elem_t *elem;

	elem = ast_cinit_first(cinit);
	while (elem != NULL) {
		list_remove(&elem->lcinit);
		ast_cinit_elem_destroy(elem);
		elem = ast_cinit_first(cinit);
	}

	free(cinit);
}

/** Get first token of AST compound initializer.
 *
 * @param cinit Compound initializer
 * @return First token or @c NULL
 */
static ast_tok_t *ast_cinit_first_tok(ast_cinit_t *cinit)
{
	return &cinit->tlbrace;
}

/** Get last token of AST compound initializer.
 *
 * @param cinit Compound initializer
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_cinit_last_tok(ast_cinit_t *cinit)
{
	return &cinit->trbrace;
}

/** Create AST asm statement.
 *
 * @param rasm Place to store pointer to new asm statement
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_asm_create(ast_asm_t **rasm)
{
	ast_asm_t *aasm;

	aasm = calloc(1, sizeof(ast_asm_t));
	if (aasm == NULL)
		return ENOMEM;

	list_initialize(&aasm->out_ops);
	list_initialize(&aasm->in_ops);
	list_initialize(&aasm->clobbers);
	list_initialize(&aasm->labels);

	aasm->node.ext = aasm;
	aasm->node.ntype = ant_asm;

	*rasm = aasm;
	return EOK;
}

/** Append output operand to asm statement.
 *
 * @param aasm Asm statement
 * @param have_symname @c true if we have a symbolic name in brackets
 * @param dlbracket '[' token data (if @c have_symname is true)
 * @param dsymname Symbolic name token data (if @c have_symname is true)
 * @param drbracket ']' token data (if @c have_symname is true)
 * @param dconstraint Constraint token data
 * @param dlparen '(' token data
 * @param expr Expression
 * @param drparen ')' token data
 * @param dcomma ',' token data (except for the last operand)
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_asm_append_out_op(ast_asm_t *aasm, bool have_symname, void *dlbracket,
    void *dsymname, void *drbracket, void *dconstraint, void *dlparen,
    ast_node_t *expr, void *drparen, void *dcomma)
{
	ast_asm_op_t *aop;

	aop = calloc(1, sizeof(ast_asm_op_t));
	if (aop == NULL)
		return ENOMEM;

	aop->have_symname = have_symname;
	if (have_symname) {
		aop->tlbracket.data = dlbracket;
		aop->tsymname.data = dsymname;
		aop->trbracket.data = drbracket;
	}

	aop->tconstraint.data = dconstraint;
	aop->tlparen.data = dlparen;
	aop->expr = expr;
	aop->trparen.data = drparen;
	aop->tcomma.data = dcomma;

	aop->aasm = aasm;
	list_append(&aop->lasm, &aasm->out_ops);
	return EOK;
}

/** Append input operand to asm statement.
 *
 * @param aasm Asm statement
 * @param have_symname @c true if we have a symbolic name in brackets
 * @param dlbracket '[' token data (if @c have_symname is true)
 * @param dsymname Symbolic name token data (if @c have_symname is true)
 * @param drbracket ']' token data (if @c have_symname is true)
 * @param dconstraint Constraint token data
 * @param dlparen '(' token data
 * @param expr Expression
 * @param drparen ')' token data
 * @param dcomma ',' token data (except for the last operand)
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_asm_append_in_op(ast_asm_t *aasm, bool have_symname, void *dlbracket,
    void *dsymname, void *drbracket, void *dconstraint, void *dlparen,
    ast_node_t *expr, void *drparen, void *dcomma)
{
	ast_asm_op_t *aop;

	aop = calloc(1, sizeof(ast_asm_op_t));
	if (aop == NULL)
		return ENOMEM;

	aop->have_symname = have_symname;
	if (have_symname) {
		aop->tlbracket.data = dlbracket;
		aop->tsymname.data = dsymname;
		aop->trbracket.data = drbracket;
	}

	aop->tconstraint.data = dconstraint;
	aop->tlparen.data = dlparen;
	aop->expr = expr;
	aop->trparen.data = drparen;
	aop->tcomma.data = dcomma;

	aop->aasm = aasm;
	list_append(&aop->lasm, &aasm->in_ops);
	return EOK;
}

/** Append clobber list element to asm statement.
 *
 * @param aasm Asm statement
 * @param dclobber Clobber token data
 * @param dcomma ',' token data
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_asm_append_clobber(ast_asm_t *aasm, void *dclobber, void *dcomma)
{
	ast_asm_clobber_t *aclobber;

	aclobber = calloc(1, sizeof(ast_asm_clobber_t));
	if (aclobber == NULL)
		return ENOMEM;

	aclobber->tclobber.data = dclobber;
	aclobber->tcomma.data = dcomma;

	aclobber->aasm = aasm;
	list_append(&aclobber->lasm, &aasm->clobbers);
	return EOK;
}

/** Append label list element to asm statement.
 *
 * @param aasm Asm statement
 * @param dlabel Label token data
 * @param dcomma ',' token data
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_asm_append_label(ast_asm_t *aasm, void *dlabel, void *dcomma)
{
	ast_asm_label_t *alabel;

	alabel = calloc(1, sizeof(ast_asm_label_t));
	if (alabel == NULL)
		return ENOMEM;

	alabel->tlabel.data = dlabel;
	alabel->tcomma.data = dcomma;

	alabel->aasm = aasm;
	list_append(&alabel->lasm, &aasm->labels);
	return EOK;
}

/** Return first output operand in asm statement.
 *
 * @param aasm Asm statement
 * @return First output operand or @c NULL
 */
ast_asm_op_t *ast_asm_first_out_op(ast_asm_t *aasm)
{
	link_t *link;

	link = list_first(&aasm->out_ops);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_op_t, lasm);
}

/** Return next output operand in asm statement.
 *
 * @param op Current operand
 * @return Next operand or @c NULL
 */
ast_asm_op_t *ast_asm_next_out_op(ast_asm_op_t *op)
{
	link_t *link;

	link = list_next(&op->lasm, &op->aasm->out_ops);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_op_t, lasm);
}

/** Return first input operand in asm statement.
 *
 * @param aasm Asm statement
 * @return First input operand or @c NULL
 */
ast_asm_op_t *ast_asm_first_in_op(ast_asm_t *aasm)
{
	link_t *link;

	link = list_first(&aasm->in_ops);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_op_t, lasm);
}

/** Return next input operand in asm statement.
 *
 * @param op Current operand
 * @return Next operand or @c NULL
 */
ast_asm_op_t *ast_asm_next_in_op(ast_asm_op_t *op)
{
	link_t *link;

	link = list_next(&op->lasm, &op->aasm->in_ops);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_op_t, lasm);
}

/** Return first clobber list element in asm statement.
 *
 * @param aasm Asm statement
 * @return First clobber list element or @c NULL
 */
ast_asm_clobber_t *ast_asm_first_clobber(ast_asm_t *aasm)
{
	link_t *link;

	link = list_first(&aasm->clobbers);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_clobber_t, lasm);
}

/** Return next clobber list element in asm statement.
 *
 * @param clobber Current clobber list element
 * @return Next clobber list element or @c NULL
 */
ast_asm_clobber_t *ast_asm_next_clobber(ast_asm_clobber_t *clobber)
{
	link_t *link;

	link = list_next(&clobber->lasm, &clobber->aasm->clobbers);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_clobber_t, lasm);
}

/** Return first label list element in asm statement.
 *
 * @param aasm Asm statement
 * @return First label list element or @c NULL
 */
ast_asm_label_t *ast_asm_first_label(ast_asm_t *aasm)
{
	link_t *link;

	link = list_first(&aasm->labels);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_label_t, lasm);
}

/** Return next label list element in asm statement.
 *
 * @param label Current label list element
 * @return Next label list element or @c NULL
 */
ast_asm_label_t *ast_asm_next_label(ast_asm_label_t *label)
{
	link_t *link;

	link = list_next(&label->lasm, &label->aasm->labels);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_asm_label_t, lasm);
}

/** Print AST asm statement.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_asm_print(ast_asm_t *aasm, FILE *f)
{
	ast_asm_op_t *out_op;
	ast_asm_op_t *in_op;
	ast_asm_clobber_t *clobber;
	ast_asm_label_t *label;
	int rc;

	if (fprintf(f, "asm(") < 0)
		return EIO;

	out_op = ast_asm_first_out_op(aasm);
	while (out_op != NULL) {
		if (fprintf(f, "out(") < 0)
			return EIO;

		rc = ast_tree_print(out_op->expr, f);
		if (rc != EOK)
			return EIO;

		if (fprintf(f, ")") < 0)
			return EIO;

		out_op = ast_asm_next_out_op(out_op);

		if (out_op != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	in_op = ast_asm_first_in_op(aasm);
	while (in_op != NULL) {
		if (fprintf(f, "in(") < 0)
			return EIO;

		rc = ast_tree_print(in_op->expr, f);
		if (rc != EOK)
			return EIO;

		if (fprintf(f, ")") < 0)
			return EIO;

		in_op = ast_asm_next_in_op(in_op);

		if (in_op != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	clobber = ast_asm_first_clobber(aasm);
	while (clobber != NULL) {
		if (fprintf(f, "clobber()") < 0)
			return EIO;

		clobber = ast_asm_next_clobber(clobber);

		if (clobber != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	label = ast_asm_first_label(aasm);
	while (label != NULL) {
		if (fprintf(f, "in()") < 0)
			return EIO;

		label = ast_asm_next_label(label);

		if (label != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST asm statement.
 *
 * @param block Block
 */
static void ast_asm_destroy(ast_asm_t *aasm)
{
	ast_asm_op_t *out_op;
	ast_asm_op_t *in_op;
	ast_asm_clobber_t *clobber;
	ast_asm_label_t *label;

	ast_tree_destroy(aasm->atemplate);

	out_op = ast_asm_first_out_op(aasm);
	while (out_op != NULL) {
		list_remove(&out_op->lasm);
		ast_tree_destroy(out_op->expr);
		free(out_op);
		out_op = ast_asm_first_out_op(aasm);
	}

	in_op = ast_asm_first_in_op(aasm);
	while (in_op != NULL) {
		list_remove(&in_op->lasm);
		ast_tree_destroy(in_op->expr);
		free(in_op);
		in_op = ast_asm_first_in_op(aasm);
	}

	clobber = ast_asm_first_clobber(aasm);
	while (clobber != NULL) {
		list_remove(&clobber->lasm);
		free(clobber);
		clobber = ast_asm_first_clobber(aasm);
	}

	label = ast_asm_first_label(aasm);
	while (label != NULL) {
		list_remove(&label->lasm);
		free(label);
		label = ast_asm_first_label(aasm);
	}

	free(aasm);
}

/** Get first token of AST enum type specifier.
 *
 * @param aasm enum type specifier
 * @return First token or @c NULL
 */
static ast_tok_t *ast_asm_first_tok(ast_asm_t *aasm)
{
	return &aasm->tasm;
}

/** Get last token of AST enum type specifier.
 *
 * @param aasm enum type specifier
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_asm_last_tok(ast_asm_t *aasm)
{
	return &aasm->tscolon;
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

	if (areturn->arg != NULL) {
		rc = ast_tree_print(areturn->arg, f);
		if (rc != EOK)
			return rc;
	}

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
	list_initialize(&aif->elseifs);

	*rif = aif;
	return EOK;
}

/** Append else-if part to if statement.
 *
 * @param aif If statement
 * @param delse Else token data
 * @param dif If token data
 * @param dlparen Left parenthesis token data
 * @param cond Condition expression
 * @param ebranch Else-if branch
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_if_append(ast_if_t *aif, void *delse, void *dif, void *dlparen,
    ast_node_t *cond, void *drparen, ast_block_t *ebranch)
{
	ast_elseif_t *elseif;

	elseif = calloc(1, sizeof(ast_elseif_t));
	if (elseif == NULL)
		return ENOMEM;

	elseif->telse.data = delse;
	elseif->tif.data = dif;
	elseif->tlparen.data = dlparen;
	elseif->cond = cond;
	elseif->trparen.data = drparen;
	elseif->ebranch = ebranch;

	elseif->aif = aif;
	list_append(&elseif->lif, &aif->elseifs);
	return EOK;
}

/** Return first else-if part.
 *
 * @param aif If statement
 * @return First element or @c NULL
 */
ast_elseif_t *ast_if_first(ast_if_t *aif)
{
	link_t *link;

	link = list_first(&aif->elseifs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_elseif_t, lif);
}

/** Return next element in record type specifier.
 *
 * @param eir Current element
 * @return Next element or @c NULL
 */
ast_elseif_t *ast_if_next(ast_elseif_t *eif)
{
	link_t *link;

	link = list_next(&eif->lif, &eif->aif->elseifs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, ast_elseif_t, lif);
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
	if (rc != EOK)
		return rc;
	if (fprintf(f, ",") < 0)
		return EIO;
	if (fprintf(f, ",") < 0)
		return EIO;
	rc = ast_block_print(aif->tbranch, f);
	if (rc != EOK)
		return rc;
	if (fprintf(f, ",") < 0)
		return EIO;
	if (aif->fbranch != NULL) {
		rc = ast_block_print(aif->fbranch, f);
		if (rc != EOK)
			return rc;
	}
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
	ast_elseif_t *elseif;

	ast_tree_destroy(aif->cond);
	ast_block_destroy(aif->tbranch);

	elseif = ast_if_first(aif);
	while (elseif != NULL) {
		ast_tree_destroy(elseif->cond);
		ast_block_destroy(elseif->ebranch);
		list_remove(&elseif->lif);
		free(elseif);
		elseif = ast_if_first(aif);
	}

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

	if (afor->linit != NULL) {
		rc = ast_tree_print(afor->linit, f);
		if (rc != EOK)
			return rc;
	}

	if (fprintf(f, ",") < 0)
		return EIO;

	if (afor->lcond != NULL) {
		rc = ast_tree_print(afor->lcond, f);
		if (rc != EOK)
			return rc;
	}

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
	ast_dspecs_destroy(afor->dspecs);
	ast_idlist_destroy(afor->idlist);
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
	ast_tree_destroy(astexpr->expr);
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

	rc = ast_idlist_print(stdecln->idlist, f);
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
	ast_idlist_destroy(stdecln->idlist);

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

/** Create AST loop macro invocation.
 *
 * @param rlmacro Place to store pointer to new loop macro invocation
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_lmacro_create(ast_lmacro_t **rlmacro)
{
	ast_lmacro_t *lmacro;

	lmacro = calloc(1, sizeof(ast_lmacro_t));
	if (lmacro == NULL)
		return ENOMEM;

	lmacro->node.ext = lmacro;
	lmacro->node.ntype = ant_lmacro;

	*rlmacro = lmacro;
	return EOK;
}

/** Print AST loop macro invocation.
 *
 * @param lmacro loop macro invocation
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_lmacro_print(ast_lmacro_t *lmacro, FILE *f)
{
	int rc;

	if (fprintf(f, "lmacro(") < 0)
		return EIO;

	rc = ast_tree_print(lmacro->expr, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ", ") < 0)
		return EIO;

	rc = ast_block_print(lmacro->body, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Destroy AST loop macro invocation.
 *
 * @param lmacro loop macro invocation
 */
static void ast_lmacro_destroy(ast_lmacro_t *lmacro)
{
	ast_tree_destroy(lmacro->expr);
	ast_block_destroy(lmacro->body);

	free(lmacro);
}

/** Get first token of AST loop macro invocation.
 *
 * @param lmacro loop macro invocation
 * @return First token or @c NULL
 */
static ast_tok_t *ast_lmacro_first_tok(ast_lmacro_t *lmacro)
{
	return ast_tree_first_tok(lmacro->expr);
}

/** Get last token of AST loop macro invocation.
 *
 * @param lmacro loop macro invocation
 * @return Last token or @c NULL
 */
static ast_tok_t *ast_lmacro_last_tok(ast_lmacro_t *lmacro)
{
	return ast_block_last_tok(lmacro->body);
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
	case ant_mdecln:
		return ast_mdecln_print((ast_mdecln_t *)node->ext, f);
	case ant_gmdecln:
		return ast_gmdecln_print((ast_gmdecln_t *)node->ext, f);
	case ant_externc:
		return ast_externc_print((ast_externc_t *)node->ext, f);
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
	case ant_regassign:
		return ast_regassign_print((ast_regassign_t *)node->ext, f);
	case ant_aslist:
		return ast_aslist_print((ast_aslist_t *)node->ext, f);
	case ant_aspec:
		return ast_aspec_print((ast_aspec_t *)node->ext, f);
	case ant_malist:
		return ast_malist_print((ast_malist_t *)node->ext, f);
	case ant_mattr:
		return ast_mattr_print((ast_mattr_t *)node->ext, f);
	case ant_sqlist:
		return ast_sqlist_print((ast_sqlist_t *)node->ext, f);
	case ant_tqlist:
		return ast_tqlist_print((ast_tqlist_t *)node->ext, f);
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
	case ant_idlist:
		return ast_idlist_print((ast_idlist_t *)node->ext, f);
	case ant_typename:
		return ast_typename_print((ast_typename_t *)node->ext, f);
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
	case ant_econcat:
		return ast_econcat_print((ast_econcat_t *)node->ext, f);
	case ant_ebinop:
		return ast_ebinop_print((ast_ebinop_t *)node->ext, f);
	case ant_etcond:
		return ast_etcond_print((ast_etcond_t *)node->ext, f);
	case ant_ecomma:
		return ast_ecomma_print((ast_ecomma_t *)node->ext, f);
	case ant_ecall:
		return ast_ecall_print((ast_ecall_t *)node->ext, f);
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
	case ant_ecliteral:
		return ast_ecliteral_print((ast_ecliteral_t *)node->ext, f);
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
	case ant_cinit:
		return ast_cinit_print((ast_cinit_t *)node->ext, f);
	case ant_asm:
		return ast_asm_print((ast_asm_t *)node->ext, f);
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
	case ant_lmacro:
		return ast_lmacro_print((ast_lmacro_t *)node->ext, f);
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
	case ant_mdecln:
		ast_mdecln_destroy((ast_mdecln_t *)node->ext);
		break;
	case ant_gmdecln:
		ast_gmdecln_destroy((ast_gmdecln_t *)node->ext);
		break;
	case ant_externc:
		ast_externc_destroy((ast_externc_t *)node->ext);
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
	case ant_regassign:
		ast_regassign_destroy((ast_regassign_t *)node->ext);
		break;
	case ant_aslist:
		ast_aslist_destroy((ast_aslist_t *)node->ext);
		break;
	case ant_aspec:
		ast_aspec_destroy((ast_aspec_t *)node->ext);
		break;
	case ant_malist:
		ast_malist_destroy((ast_malist_t *)node->ext);
		break;
	case ant_mattr:
		ast_mattr_destroy((ast_mattr_t *)node->ext);
		break;
	case ant_sqlist:
		ast_sqlist_destroy((ast_sqlist_t *)node->ext);
		break;
	case ant_tqlist:
		ast_tqlist_destroy((ast_tqlist_t *)node->ext);
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
	case ant_idlist:
		ast_idlist_destroy((ast_idlist_t *)node->ext);
		break;
	case ant_typename:
		ast_typename_destroy((ast_typename_t *)node->ext);
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
	case ant_econcat:
		ast_econcat_destroy((ast_econcat_t *)node->ext);
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
	case ant_ecall:
		ast_ecall_destroy((ast_ecall_t *)node->ext);
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
	case ant_ecliteral:
		ast_ecliteral_destroy((ast_ecliteral_t *)node->ext);
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
	case ant_cinit:
		ast_cinit_destroy((ast_cinit_t *)node->ext);
		break;
	case ant_asm:
		ast_asm_destroy((ast_asm_t *)node->ext);
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
	case ant_lmacro:
		return ast_lmacro_destroy((ast_lmacro_t *)node->ext);
	}
}

ast_tok_t *ast_tree_first_tok(ast_node_t *node)
{
	switch (node->ntype) {
	case ant_block:
		return ast_block_first_tok((ast_block_t *)node->ext);
	case ant_gdecln:
		return ast_gdecln_first_tok((ast_gdecln_t *)node->ext);
	case ant_mdecln:
		return ast_mdecln_first_tok((ast_mdecln_t *)node->ext);
	case ant_gmdecln:
		return ast_gmdecln_first_tok((ast_gmdecln_t *)node->ext);
	case ant_externc:
		return ast_externc_first_tok((ast_externc_t *)node->ext);
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
	case ant_regassign:
		return ast_regassign_first_tok((ast_regassign_t *)node->ext);
	case ant_aslist:
		return ast_aslist_first_tok((ast_aslist_t *)node->ext);
	case ant_aspec:
		return ast_aspec_first_tok((ast_aspec_t *)node->ext);
	case ant_malist:
		return ast_malist_first_tok((ast_malist_t *)node->ext);
	case ant_mattr:
		return ast_mattr_first_tok((ast_mattr_t *)node->ext);
	case ant_sqlist:
		return ast_sqlist_first_tok((ast_sqlist_t *)node->ext);
	case ant_tqlist:
		return ast_tqlist_first_tok((ast_tqlist_t *)node->ext);
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
	case ant_idlist:
		return ast_idlist_first_tok((ast_idlist_t *)node->ext);
	case ant_typename:
		return ast_typename_first_tok((ast_typename_t *)node->ext);
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
	case ant_econcat:
		return ast_econcat_first_tok((ast_econcat_t *)node->ext);
	case ant_ebinop:
		return ast_ebinop_first_tok((ast_ebinop_t *)node->ext);
	case ant_etcond:
		return ast_etcond_first_tok((ast_etcond_t *)node->ext);
	case ant_ecomma:
		return ast_ecomma_first_tok((ast_ecomma_t *)node->ext);
	case ant_ecall:
		return ast_ecall_first_tok((ast_ecall_t *)node->ext);
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
	case ant_ecliteral:
		return ast_ecliteral_first_tok((ast_ecliteral_t *)node->ext);
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
	case ant_cinit:
		return ast_cinit_first_tok((ast_cinit_t *)node->ext);
	case ant_asm:
		return ast_asm_first_tok((ast_asm_t *)node->ext);
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
	case ant_lmacro:
		return ast_lmacro_first_tok((ast_lmacro_t *)node->ext);
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
	case ant_mdecln:
		return ast_mdecln_last_tok((ast_mdecln_t *)node->ext);
	case ant_gmdecln:
		return ast_gmdecln_last_tok((ast_gmdecln_t *)node->ext);
	case ant_externc:
		return ast_externc_last_tok((ast_externc_t *)node->ext);
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
	case ant_regassign:
		return ast_regassign_last_tok((ast_regassign_t *)node->ext);
	case ant_aslist:
		return ast_aslist_last_tok((ast_aslist_t *)node->ext);
	case ant_aspec:
		return ast_aspec_last_tok((ast_aspec_t *)node->ext);
	case ant_malist:
		return ast_malist_last_tok((ast_malist_t *)node->ext);
	case ant_mattr:
		return ast_mattr_last_tok((ast_mattr_t *)node->ext);
	case ant_sqlist:
		return ast_sqlist_last_tok((ast_sqlist_t *)node->ext);
	case ant_tqlist:
		return ast_tqlist_last_tok((ast_tqlist_t *)node->ext);
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
	case ant_idlist:
		return ast_idlist_last_tok((ast_idlist_t *)node->ext);
	case ant_typename:
		return ast_typename_last_tok((ast_typename_t *)node->ext);
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
	case ant_econcat:
		return ast_econcat_last_tok((ast_econcat_t *)node->ext);
	case ant_ebinop:
		return ast_ebinop_last_tok((ast_ebinop_t *)node->ext);
	case ant_etcond:
		return ast_etcond_last_tok((ast_etcond_t *)node->ext);
	case ant_ecomma:
		return ast_ecomma_last_tok((ast_ecomma_t *)node->ext);
	case ant_ecall:
		return ast_ecall_last_tok((ast_ecall_t *)node->ext);
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
	case ant_ecliteral:
		return ast_ecliteral_last_tok((ast_ecliteral_t *)node->ext);
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
	case ant_cinit:
		return ast_cinit_last_tok((ast_cinit_t *)node->ext);
	case ant_asm:
		return ast_asm_last_tok((ast_asm_t *)node->ext);
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
	case ant_lmacro:
		return ast_lmacro_last_tok((ast_lmacro_t *)node->ext);
	}

	assert(false);
	return NULL;
}
