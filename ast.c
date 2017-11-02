#include <adt/list.h>
#include <ast.h>
#include <merrno.h>
#include <stdio.h>
#include <stdlib.h>

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

void ast_module_append(ast_module_t *module, ast_node_t *decl)
{
	list_append(&decl->llist, &module->decls);
}

static int ast_module_print(ast_module_t *module, FILE *f)
{
	(void)module;
	if (fprintf(f, "module(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}

int ast_fundef_create(ast_type_t *ftype, ast_ident_t *fident,
    ast_fundef_t **rfundef)
{
	ast_fundef_t *fundef;

	fundef = calloc(1, sizeof(ast_fundef_t));
	if (fundef == NULL)
		return ENOMEM;

	(void) ftype;
	(void) fident;

	fundef->node.ext = fundef;
	fundef->node.ntype = ant_fundef;
	*rfundef = fundef;
	return EOK;
}

static int ast_fundef_print(ast_fundef_t *fundef, FILE *f)
{
	(void)fundef;
	if (fprintf(f, "fundef(") < 0)
		return EIO;
	if (fprintf(f, ")") < 0)
		return EIO;
	return EOK;
}


int ast_tree_print(ast_node_t *node, FILE *f)
{
	switch (node->ntype) {
	case ant_module:
		return ast_module_print((ast_module_t *)node->ext, f);
	case ant_fundef:
		return ast_fundef_print((ast_fundef_t *)node->ext, f);
	default:
		break;
	}

	return EINVAL;
}

void ast_tree_destroy(ast_node_t *node)
{
	free(node->ext);
}
