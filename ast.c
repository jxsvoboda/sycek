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
static int ast_dlist_print(ast_dlist_t *, FILE *);

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
 * @param fundef Function definition
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_sclass_print(ast_sclass_t *sclass, FILE *f)
{
	const char *s;

	switch (sclass->sctype) {
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
	default:
		assert(false);
		s = "<invalid>";
		break;
	}

	if (fprintf(f, "sclass(%s)", s) < 0)
		return EIO;

	return EOK;
}

/** Create AST function definition.
 *
 * @param ftspec Function type specifier
 * @param fdecl Function declarator
 * @param body Body or @c NULL
 * @param rfundef Place to store pointer to new function definition
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_fundef_create(ast_node_t *ftspec, ast_node_t *fdecl, ast_block_t *body,
    ast_fundef_t **rfundef)
{
	ast_fundef_t *fundef;

	fundef = calloc(1, sizeof(ast_fundef_t));
	if (fundef == NULL)
		return ENOMEM;

	fundef->ftspec = ftspec;
	fundef->fdecl = fdecl;
	fundef->body = body;

	fundef->node.ext = fundef;
	fundef->node.ntype = ant_fundef;

	*rfundef = fundef;
	return EOK;
}

/** Print AST function definition.
 *
 * @param fundef Function definition
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_fundef_print(ast_fundef_t *fundef, FILE *f)
{
	int rc;

	if (fprintf(f, "fundef(") < 0)
		return EIO;

	rc = ast_tree_print(fundef->ftspec, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ", ") < 0)
		return EIO;

	rc = ast_tree_print(fundef->fdecl, f);
	if (rc != EOK)
		return rc;

	if (fundef->body != NULL) {
		if (fprintf(f, ", ") < 0)
			return EIO;
		rc = ast_block_print(fundef->body, f);
		if (rc != EOK)
			return rc;
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
}

/** Create AST type definition.
 *
 * @param tspec Type specifier
 * @param dlist Declarator list
 * @param rtypedef Place to store pointer to new type definition
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_typedef_create(ast_node_t *tspec, ast_dlist_t *dlist,
    ast_typedef_t **rtypedef)
{
	ast_typedef_t *atypedef;

	atypedef = calloc(1, sizeof(ast_typedef_t));
	if (atypedef == NULL)
		return ENOMEM;

	atypedef->tspec = tspec;
	atypedef->dlist = dlist;

	atypedef->node.ext = atypedef;
	atypedef->node.ntype = ant_typedef;

	*rtypedef = atypedef;
	return EOK;
}

/** Print AST type definition.
 *
 * @param TYPEdef Type definition
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_typedef_print(ast_typedef_t *atypedef, FILE *f)
{
	int rc;

	if (fprintf(f, "typedef(") < 0)
		return EIO;

	rc = ast_tree_print(atypedef->tspec, f);
	if (rc != EOK)
		return rc;

	rc = ast_dlist_print(atypedef->dlist, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
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
	(void)tsbasic;/* XXX */
	if (fprintf(f, "tsbasic(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
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
	(void)atsident;/* XXX */
	if (fprintf(f, "tsident(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
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
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tsrecord_print(ast_tsrecord_t *tsrecord, FILE *f)
{
	ast_tsrecord_elem_t *elem;
	int rc;

	if (fprintf(f, "tsrecord(%s", tsrecord->rtype == ar_struct ? "struct" :
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
 * @return First statement or @c NULL
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
	(void)adident;/* XXX */
	if (fprintf(f, "dident(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Create AST no-identifier declartor.
 *
 * @param rdnoident Place to store pointer to new no-identifier declartor
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

/** Print AST no-identifier declartor.
 *
 * @param adnoident Identifier specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_dnoident_print(ast_dnoident_t *adnoident, FILE *f)
{
	(void)adnoident;/* XXX */
	if (fprintf(f, "dnoident(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
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
	(void)adparen;/* XXX */
	if (fprintf(f, "dparen(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
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
	(void)adptr;/* XXX */
	if (fprintf(f, "dptr(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
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
 * @param tspec Argument type specifier
 * @param decl Argument declarator
 * @param dcomma Data for comma token or @c NULL
 * @return EOK on success, ENOMEM if out of memory
 */
int ast_dfun_append(ast_dfun_t *dfun, ast_node_t *tspec, ast_node_t *decl,
    void *dcomma)
{
	ast_dfun_arg_t *arg;

	arg = calloc(1, sizeof(ast_dfun_arg_t));
	if (arg == NULL)
		return ENOMEM;

	arg->tspec = tspec;
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
	ast_dfun_arg_t *elem;

	if (fprintf(f, "dfun(") < 0)
		return EIO;

	elem = ast_dfun_first(dfun);
	while (elem != NULL) {
		if (fprintf(f, "elem") < 0)
			return EIO;

		elem = ast_dfun_next(elem);

		if (elem != NULL) {
			if (fprintf(f, ", ") < 0)
				return EIO;
		}
	}

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
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

/** Print AST function declarator.
 *
 * @param block Block
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_darray_print(ast_darray_t *darray, FILE *f)
{
	(void) darray;

	if (fprintf(f, "darray(") < 0)
		return EIO;

	if (fprintf(f, ")") < 0)
		return EIO;

	return EOK;
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

	if (fprintf(f, "dlist(") < 0)
		return EIO;

	entry = ast_dlist_first(dlist);
	while (entry != NULL) {
		if (fprintf(f, "decl") < 0)
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
	(void)areturn;
	if (fprintf(f, "return(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
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
	case ant_fundef:
		return ast_fundef_print((ast_fundef_t *)node->ext, f);
	case ant_typedef:
		return ast_typedef_print((ast_typedef_t *)node->ext, f);
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
	case ant_sqlist:
		return ast_sqlist_print((ast_sqlist_t *)node->ext, f);
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
	case ant_return:
		return ast_return_print((ast_return_t *)node->ext, f);
	}

	return EINVAL;
}

/** Destroy AST tree.
 *
 * @param node Root node
 */
void ast_tree_destroy(ast_node_t *node)
{
	free(node->ext);
}
