/*
 * Abstract syntax tree
 */

#include <adt/list.h>
#include <assert.h>
#include <ast.h>
#include <merrno.h>
#include <stdio.h>
#include <stdlib.h>

static int ast_block_print(ast_block_t *, FILE *);

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
 * @return EOK on sucess, ENOMEM if out of memory
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


/** Create AST function definition.
 *
 * @param ftspec Function type specifier
 * @param fdecl Function declarator
 * @param body Body or @c NULL
 * @param rfundef Place to store pointer to new function definition
 *
 * @return EOK on sucess, ENOMEM if out of memory
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

/** Create AST block.
 *
 * @param braces Whether the block has braces or not
 * @param rblock Place to store pointer to new block
 *
 * @return EOK on sucess, ENOMEM if out of memory
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
 * @param block Block
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

	if (fprintf(f, "block(%s", block->braces == ast_braces ? "{" : "") < 0)
		return EIO;

	stmt = ast_block_first(block);
	while (stmt != NULL) {
		ast_tree_print(stmt, f);
		stmt = ast_block_next(stmt);
	}
	if (fprintf(f, "%s)", block->braces == ast_braces ? "}" : "") < 0)
		return EIO;
	return EOK;
}

/** Create AST builtin type specifier.
 *
 * @param rtsbuiltin Place to store pointer to new builtin type specifier
 *
 * @return EOK on sucess, ENOMEM if out of memory
 */
int ast_tsbuiltin_create(ast_tsbuiltin_t **rtsbuiltin)
{
	ast_tsbuiltin_t *atsbuiltin;

	atsbuiltin = calloc(1, sizeof(ast_tsbuiltin_t));
	if (atsbuiltin == NULL)
		return ENOMEM;

	atsbuiltin->node.ext = atsbuiltin;
	atsbuiltin->node.ntype = ant_tsbuiltin;

	*rtsbuiltin = atsbuiltin;
	return EOK;
}

/** Print AST builtin type specifier.
 *
 * @param atsbuiltin Builtin type specifier
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int ast_tsbuiltin_print(ast_tsbuiltin_t *atsbuiltin, FILE *f)
{
	(void)atsbuiltin;/* XXX */
	if (fprintf(f, "tsbuiltin(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

/** Create AST identifier type specifier.
 *
 * @param rtsident Place to store pointer to new identifier type specifier
 *
 * @return EOK on sucess, ENOMEM if out of memory
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

/** Create AST identifier declarator.
 *
 * @param rdident Place to store pointer to new identifier declarator
 *
 * @return EOK on sucess, ENOMEM if out of memory
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
 * @return EOK on sucess, ENOMEM if out of memory
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
 * @return EOK on sucess, ENOMEM if out of memory
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
 * @return EOK on sucess, ENOMEM if out of memory
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

/** Create AST return.
 *
 * @param rreturn Place to store pointer to new return
 *
 * @return EOK on sucess, ENOMEM if out of memory
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
	case ant_module:
		return ast_module_print((ast_module_t *)node->ext, f);
	case ant_tsbuiltin:
		return ast_tsbuiltin_print((ast_tsbuiltin_t *)node->ext, f);
	case ant_tsident:
		return ast_tsident_print((ast_tsident_t *)node->ext, f);
	case ant_dident:
		return ast_dident_print((ast_dident_t *)node->ext, f);
	case ant_dnoident:
		return ast_dnoident_print((ast_dnoident_t *)node->ext, f);
	case ant_dparen:
		return ast_dparen_print((ast_dparen_t *)node->ext, f);
	case ant_dptr:
		return ast_dptr_print((ast_dptr_t *)node->ext, f);
	case ant_return:
		return ast_return_print((ast_return_t *)node->ext, f);
	default:
		break;
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
