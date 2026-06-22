/*
 * Copyright 2026 Jiri Svoboda
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
 * Compiler
 */

#include <adt/list.h>
#include <assert.h>
#include <ast.h>
#include <cgen.h>
#include <comp.h>
#include <ir.h>
#include <irlexer.h>
#include <irparser.h>
#include <lexer.h>
#include <merrno.h>
#include <object/linker.h>
#include <object/object.h>
#include <parser.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <symbols.h>
#include <tape/maker.h>
#include <tape/tape.h>
#include <tape/tzx.h>
#include <z80/emit.h>
#include <z80/iclexer.h>
#include <z80/icparser.h>
#include <z80/isel.h>
#include <z80/ralloc.h>
#include <z80/z80ic.h>

static void comp_parser_read_tok(void *, void *, unsigned, bool,
    lexer_tok_t *);
static void *comp_parser_next_tok(void *, void *);
static void *comp_parser_tok_data(void *, void *);
static comp_tok_t *comp_module_first_tok(comp_module_t *);
static comp_tok_t *comp_next_tok(comp_tok_t *);
static void comp_remove_token(comp_tok_t *);

static parser_input_ops_t comp_parser_input = {
	.read_tok = comp_parser_read_tok,
	.next_tok = comp_parser_next_tok,
	.tok_data = comp_parser_tok_data
};

static void comp_ir_parser_read_tok(void *, ir_lexer_tok_t *);
static void comp_ir_parser_next_tok(void *);

static ir_parser_input_ops_t comp_ir_parser_input = {
	.read_tok = comp_ir_parser_read_tok,
	.next_tok = comp_ir_parser_next_tok,
};

static void comp_z80ic_parser_read_tok(void *, z80ic_lexer_tok_t *);
static void comp_z80ic_parser_next_tok(void *);

static z80ic_parser_input_ops_t comp_z80ic_parser_input = {
	.read_tok = comp_z80ic_parser_read_tok,
	.next_tok = comp_z80ic_parser_next_tok,
};

enum {
	org_default = 0x8000u
};

/** Return compiler module type as string.
 *
 * @param mtype Module type
 * @return Module type as string (e.g. ".c")
 */
static const char *comp_mtype_str(comp_mtype_t mtype)
{
	switch (mtype) {
	case cmt_csrc:
		return ".c";
	case cmt_chdr:
		return ".h";
	case cmt_ir:
		return ".ir";
	case cmt_ic:
		return ".ic";
	case cmt_obj:
		return ".obj";
	}

	return "unknown";
}

/** Create compiler module.
 *
 * @param comp Compiler
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param mtype Module type
 * #param fname File name
 * @param rmodule Place to store new compiler module.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int comp_module_create(comp_t *comp, lexer_input_ops_t *input_ops,
    void *input_arg, comp_mtype_t mtype, const char *fname,
    comp_module_t **rmodule)
{
	comp_module_t *module = NULL;
	lexer_t *lexer = NULL;
	ir_lexer_t *ir_lexer = NULL;
	z80ic_lexer_t *ic_lexer = NULL;
	symbols_t *symbols = NULL;
	int rc;

	module = calloc(1, sizeof(comp_module_t));
	if (module == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = symbols_create(&symbols);
	if (rc != EOK)
		goto error;

	if (mtype == cmt_csrc || mtype == cmt_chdr)  {
		/* C language lexer */
		rc = lexer_create(input_ops, input_arg, &lexer);
		if (rc != EOK) {
			assert(rc == ENOMEM);
			goto error;
		}
	} else if (mtype == cmt_ic) {
		/* IC language lexer */
		rc = z80ic_lexer_create(input_ops, input_arg, &ic_lexer);
		if (rc != EOK) {
			assert(rc == ENOMEM);
			goto error;
		}
	} else {
		/* IR language lexer */
		rc = ir_lexer_create(input_ops, input_arg, &ir_lexer);
		if (rc != EOK) {
			assert(rc == ENOMEM);
			goto error;
		}
	}

	module->fname = strdup(fname);
	if (module->fname == NULL) {
		rc = ENOMEM;
		goto error;
	}

	module->comp = comp;
	list_append(&module->lmods, &comp->mods);

	module->lexer = lexer;
	module->ir_lexer = ir_lexer;
	module->ic_lexer = ic_lexer;
	module->mtype = mtype;
	module->symbols = symbols;

	list_initialize(&module->toks);

	*rmodule = module;
	return EOK;
error:
	symbols_destroy(symbols);
	if (lexer != NULL)
		lexer_destroy(lexer);
	if (module != NULL)
		free(module);
	return rc;
}

/** Create compiler module from object file.
 *
 * @param comp Compiler
 * #param fname File name
 * @param rmodule Place to store new compiler module.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int comp_module_create_from_obj(comp_t *comp, const char *fname,
    comp_module_t **rmodule)
{
	comp_module_t *module = NULL;
	FILE *objf = NULL;
	int rc;

	module = calloc(1, sizeof(comp_module_t));
	if (module == NULL) {
		rc = ENOMEM;
		goto error;
	}

	module->fname = strdup(fname);
	if (module->fname == NULL) {
		rc = ENOMEM;
		goto error;
	}

	objf = fopen(fname, "rb");
	if (objf == NULL) {
		(void)fprintf(stderr, "Error opening '%s'.\n",
		    fname);
		rc = EIO;
		goto error;
	}

	rc = obj_object_load_obj(objf, fname, &module->object);
	if (rc != EOK)
		goto error;

	(void)fclose(objf);
	objf = NULL;

	module->comp = comp;
	list_append(&module->lmods, &comp->mods);

	module->mtype = cmt_obj;
	list_initialize(&module->toks);

	*rmodule = module;
	return EOK;
error:
	if (objf != NULL)
		(void)fclose(objf);
	if (module != NULL && module->fname != NULL)
		free(module->fname);
	if (module != NULL)
		free(module);
	return rc;
}

/** Destroy compiler module.
 *
 * @param comp Compiler
 */
void comp_module_destroy(comp_module_t *module)
{
	comp_tok_t *tok;

	if (module == NULL)
		return;

	tok = comp_module_first_tok(module);
	while (tok != NULL) {
		comp_remove_token(tok);
		tok = comp_module_first_tok(module);
	}

	if (module->ast != NULL)
		ast_tree_destroy(&module->ast->node);

	symbols_destroy(module->symbols);
	ir_module_destroy(module->ir);
	z80ic_module_destroy(module->vric);
	z80ic_module_destroy(module->ic);
	z80ic_lexer_destroy(module->ic_lexer);
	obj_object_destroy(module->object);
	ir_lexer_destroy(module->ir_lexer);
	lexer_destroy(module->lexer);

	list_remove(&module->lmods);
	free(module->fname);
	free(module);
}

/** Get first module in compiler.
 *
 * @param comp Compiler
 * @return First module or @c NULL if there are none
 */
comp_module_t *comp_module_first(comp_t *comp)
{
	link_t *link;

	link = list_first(&comp->mods);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, comp_module_t, lmods);
}

/** Get nex module in compiler.
 *
 * @param cur Current module
 * @return Next module or @c NULL if cur is the last one
 */
comp_module_t *comp_module_next(comp_module_t *cur)
{
	link_t *link;

	link = list_next(&cur->lmods, &cur->comp->mods);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, comp_module_t, lmods);
}

/** Create a compiler token.
 *
 * @param tok Lexer token
 * @param rctok Place to store pointer to new compiler token
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int comp_tok_new(lexer_tok_t *tok, comp_tok_t **rctok)
{
	comp_tok_t *ctok;

	ctok = calloc(1, sizeof(comp_tok_t));
	if (ctok == NULL)
		return ENOMEM;

	ctok->tok = *tok;
	*rctok = ctok;
	return EOK;
}

/** Append a token to compiler module.
 *
 * @param module Compiler module
 * @param tok Lexer token
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int comp_module_append(comp_module_t *module, lexer_tok_t *tok)
{
	comp_tok_t *ctok;
	int rc;

	rc = comp_tok_new(tok, &ctok);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		return rc;
	}

	ctok->mod = module;
	list_append(&ctok->ltoks, &module->toks);

	return EOK;
}

/** Remove a token from the source code.
 *
 * @param tok Token to remove
 */
static void comp_remove_token(comp_tok_t *tok)
{
	list_remove(&tok->ltoks);
	lexer_free_tok(&tok->tok);
	free(tok);
}

/** Create compiler.
 *
 * @param rcomp Place to store new compiler.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int comp_create(comp_t **rcomp)
{
	comp_t *comp = NULL;
	int rc;

	comp = calloc(1, sizeof(comp_t));
	if (comp == NULL) {
		rc = ENOMEM;
		goto error;
	}

	list_initialize(&comp->mods);
	*rcomp = comp;
	return EOK;
error:
	return rc;
}

/** Destroy compiler.
 *
 * @param comp Compiler
 */
void comp_destroy(comp_t *comp)
{
	comp_module_t *module;

	module = comp_module_first(comp);
	while (module != NULL) {
		comp_module_destroy(module);
		module = comp_module_first(comp);
	}

	obj_object_destroy(comp->linked_object);
	tape_destroy(comp->tape);
	free(comp);
}

/** Lex a module.
 *
 * @param module Compiler module
 * @return EOK on success, or error code
 */
static int comp_module_lex(comp_module_t *module)
{
	bool done;
	lexer_tok_t tok;
	int rc;

	done = false;
	while (!done) {
		rc = lexer_get_tok(module->lexer, &tok);
		if (rc != EOK)
			return rc;

		rc = comp_module_append(module, &tok);
		if (rc != EOK) {
			lexer_free_tok(&tok);
			return rc;
		}

		if (tok.ttype == ltt_eof)
			done = true;
	}

	module->lexed = true;
	return EOK;
}

/** Get first token in a compiler module.
 *
 * @param mod Compiler module
 * @return First token or @c NULL if the list is empty
 */
static comp_tok_t *comp_module_first_tok(comp_module_t *mod)
{
	link_t *link;

	link = list_first(&mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, comp_tok_t, ltoks);
}

/** Get next token in a compiler module
 *
 * @param tok Current token or @c NULL
 * @return Next token or @c NULL
 */
static comp_tok_t *comp_next_tok(comp_tok_t *tok)
{
	link_t *link;

	if (tok == NULL)
		return NULL;

	link = list_next(&tok->ltoks, &tok->mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, comp_tok_t, ltoks);
}

/** Parse an IR module.
 *
 * @param module Compiler module
 * @return EOK on success or error code
 */
static int comp_ir_module_parse(comp_module_t *module)
{
	ir_module_t *irmod;
	ir_parser_t *parser = NULL;
	comp_ir_parser_input_t pinput;
	int rc;

	pinput.ir_lexer = module->ir_lexer;
	pinput.have_tok = false;

	rc = ir_parser_create(&comp_ir_parser_input, &pinput, &parser);
	if (rc != EOK)
		return rc;

	rc = ir_parser_process_module(parser, &irmod);
	if (rc != EOK)
		goto error;

	module->ir = irmod;
	ir_parser_destroy(parser);

	return EOK;
error:
	ir_parser_destroy(parser);
	return rc;
}

/** Parse an S (symbolic instruction code) module.
 *
 * @param module Compiler module
 * @return EOK on success or error code
 */
static int comp_z80ic_module_parse(comp_module_t *module)
{
	z80ic_module_t *icmod;
	z80ic_parser_t *parser = NULL;
	comp_z80ic_parser_input_t pinput;
	int rc;

	pinput.ic_lexer = module->ic_lexer;
	pinput.have_tok = false;

	rc = z80ic_parser_create(&comp_z80ic_parser_input, &pinput, &parser);
	if (rc != EOK)
		return rc;

	rc = z80ic_parser_process_module(parser, &icmod);
	if (rc != EOK)
		goto error;

	module->ic = icmod;
	z80ic_parser_destroy(parser);

	return EOK;
error:
	z80ic_parser_destroy(parser);
	return rc;
}

/** Make sure compiler tokenized source is available.
 *
 * If source hasn't been tokenized yet, do it now.
 *
 * @param module Compiler module
 * @return EOK on success or error code
 */
static int comp_module_build_toks(comp_module_t *module)
{
	int rc;

	if (!module->lexed) {
		rc = comp_module_lex(module);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Run all compiler steps needed to get IR.
 *
 * If some parts are already built, they are skipped.
 *
 * @param module Compiler module
 * @return EOK on success or an error code
 */
int comp_module_make_ir(comp_module_t *module)
{
	int rc;
	parser_t *parser = NULL;
	comp_parser_input_t pinput;
	cgen_t *cgen = NULL;

	if (module->mtype == cmt_ir && module->ir == NULL) {
		rc = comp_ir_module_parse(module);
		if (rc != EOK)
			goto error;
	}

	if (module->ir == NULL) {
		rc = comp_module_build_toks(module);
		if (rc != EOK)
			return rc;

		rc = cgen_create(&cgen);
		if (rc != EOK)
			goto error;

		/* Different arithmetic types not implemented yet */
		cgen->arith_width = 16;

		/* Configure code generator. */
		cgen->flags = module->comp->cgflags;

		rc = cgen_module(cgen, &comp_parser_input, &pinput,
		    comp_module_first_tok(module), module->symbols,
		    &module->ir);
		if (rc != EOK)
			goto error;

		/* Check for non-fatal code generator errors */
		if (cgen->error) {
			rc = EINVAL;
			goto error;
		}

		module->ast = cgen->astmod;

		cgen_destroy(cgen);
		parser_destroy(parser);
		cgen = NULL;
	}

	return EOK;
error:
	cgen_destroy(cgen);
	return rc;
}

/** Run all compiler steps needed to get VRIC.
 *
 * If some parts are already built, they are skipped.
 *
 * @param module Compiler module
 * @return EOK on success or an error code
 */
int comp_module_make_vric(comp_module_t *module)
{
	int rc;
	z80_isel_t *isel = NULL;

	rc = comp_module_make_ir(module);
	if (rc != EOK)
		goto error;

	if (module->vric == NULL) {
		rc = z80_isel_create(&isel);
		if (rc != EOK)
			goto error;

		rc = z80_isel_module(isel, module->ir, &module->vric);
		if (rc != EOK)
			goto error;

		z80_isel_destroy(isel);
		isel = NULL;
	}

	return EOK;
error:
	z80_isel_destroy(isel);
	return rc;
}

/** Run all compiler steps needed to get IC.
 *
 * @param module Compiler module
 * @return EOK on success or an error code
 */
int comp_module_make_ic(comp_module_t *module)
{
	int rc;
	z80_ralloc_t *ralloc = NULL;

	if (module->mtype == cmt_ic && module->ic == NULL) {
		rc = comp_z80ic_module_parse(module);
		if (rc != EOK)
			goto error;
	}

	if (module->ic == NULL) {
		rc = comp_module_make_vric(module);
		if (rc != EOK)
			goto error;

		rc = z80_ralloc_create(&ralloc);
		if (rc != EOK)
			goto error;

		rc = z80_ralloc_module(ralloc, module->vric, &module->ic);
		if (rc != EOK)
			goto error;

		z80_ralloc_destroy(ralloc);
		ralloc = NULL;
	}

	return EOK;
error:
	return rc;
}

/** Compile module (but do not emit binary instructions).
 *
 * @param module Compiler module
 * @param outf Output file (for writing assembly)
 * @return EOK on success or an error code
 */
int comp_module_compile(comp_module_t *module, FILE *outf)
{
	int rc;
	z80_emit_t *emit = NULL;

	rc = comp_module_make_ic(module);
	if (rc != EOK)
		goto error;

	if (outf != NULL) {
		rc = comp_module_dump_ic(module, outf);
		if (rc != EOK)
			goto error;
	}

	return EOK;
error:
	z80_emit_destroy(emit);
	return rc;
}

/** Emit binary instructions (but do not link).
 *
 * @param module Compiler module
 * @param outf Output file (for writing assembly)
 * @return EOK on success or an error code
 */
int comp_module_emit(comp_module_t *module, FILE *outf)
{
	int rc;
	z80_emit_t *emit = NULL;

	if (module->mtype == cmt_obj) {
		/* nothing to do */
		return EOK;
	}

	if (module->object == NULL) {
		rc = comp_module_make_ic(module);
		if (rc != EOK)
			goto error;

		rc = z80_emit_create(&emit);
		if (rc != EOK)
			goto error;

		rc = z80_emit_module(emit, module->ic, module->fname,
		    &module->object);
		if (rc != EOK)
			goto error;

		z80_emit_destroy(emit);
		emit = NULL;
	}

	if (outf != NULL) {
		rc = obj_object_save_obj(module->object, outf);
		if (rc != EOK)
			goto error;
	}

	return EOK;
error:
	z80_emit_destroy(emit);
	return rc;
}

/** Perform linking.
 *
 * @param comp Compiler
 * @param outf Output file (for writing linked binary) or @c NULL
 * @return EOK on success or an error code
 */
int comp_link(comp_t *comp, FILE *outf)
{
	int rc;
	obj_linker_t *linker = NULL;
	comp_module_t *module;

	if (comp->linked_object != NULL)
		return EOK;

	rc = obj_linker_create(comp->lflags, &linker);
	if (rc != EOK)
		goto error;

	/* Add objects from all modules as sources. */
	module = comp_module_first(comp);
	while (module != NULL) {
		rc = obj_linker_add_src(linker, module->object);
		if (rc != EOK) {
			(void)fprintf(stderr, "Error adding link source.\n");
			goto error;
		}

		module = comp_module_next(module);
	}

	rc = obj_linker_set_origin(linker, org_default);
	if (rc != EOK)
		goto error;

	rc = obj_linker_link(linker, &comp->linked_object);
	if (rc != EOK)
		goto error;

	if (false)
		(void)obj_object_dump(comp->linked_object, stdout);

	if (outf != NULL) {
		rc = obj_object_save_bin(comp->linked_object, outf);
		if (rc != EOK)
			goto error;
	}

	obj_linker_destroy(linker);
	return EOK;
error:
	obj_linker_destroy(linker);
	return rc;
}

/** Save map of linked executable into a z80asm compatible map file.
 *
 * @param comp Compiler
 * @param outf Output file (for writing symbol map)
 * @return EOK on success or an error code
 */
int comp_save_map(comp_t *comp, FILE *outf)
{
	if (comp->linked_object == NULL)
		return EINVAL;

	return obj_object_save_map(comp->linked_object, outf);
}

/** Make binary executable into a tape image.
 *
 * @param comp Compiler
 * @parma name Program name
 * @return EOK on success or an error code
 */
int comp_make_tape(comp_t *comp, const char *name)
{
	if (comp->linked_object == NULL)
		return EINVAL;

	return tape_make_from_object(comp->linked_object, name, &comp->tape);
}

/** Save map of linked executable into a z80asm compatible map file.
 *
 * @param comp Compiler
 * @param fname Output file name
 * @return EOK on success or an error code
 */
int comp_save_tape(comp_t *comp, const char *fname)
{
	if (comp->linked_object == NULL)
		return EINVAL;

	return tape_save_tzx(comp->tape, fname);
}

/** Dump AST.
 *
 * @param module Compiler module
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_module_dump_ast(comp_module_t *module, FILE *f)
{
	int rc;

	switch (module->mtype) {
	case cmt_csrc:
	case cmt_chdr:
		break;
	case cmt_ir:
	case cmt_ic:
	case cmt_obj:
		(void)fprintf(stderr, "Error: Cannot dump AST for '%s' file.\n",
		    comp_mtype_str(module->mtype));
		return EINVAL;
	}

	rc = comp_module_make_ir(module);
	if (rc != EOK)
		return rc;

	return ast_tree_print(&module->ast->node, f);
}

/** Dump tokenized source.
 *
 * @param module Compiler module
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_module_dump_toks(comp_module_t *module, FILE *f)
{
	comp_tok_t *tok;
	int rc;

	switch (module->mtype) {
	case cmt_csrc:
	case cmt_chdr:
		break;
	case cmt_ir:
	case cmt_ic:
	case cmt_obj:
		(void)fprintf(stderr, "Error: Cannot dump tokens for "
		    "'%s' file.\n", comp_mtype_str(module->mtype));
		return EINVAL;
	}

	rc = comp_module_build_toks(module);
	if (rc != EOK)
		return rc;

	tok = comp_module_first_tok(module);
	while (tok->tok.ttype != ltt_eof) {
		rc = lexer_dprint_tok(&tok->tok, f);
		if (rc != EOK)
			return rc;

		if (tok->tok.ttype == ltt_newline) {
			if (fputc('\n', f) < 0) {
				rc = EIO;
				return rc;
			}
		}

		tok = comp_next_tok(tok);
	}

	return EOK;
}

/** Dump intermediate representation.
 *
 * @param module Compiler module
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_module_dump_ir(comp_module_t *module, FILE *f)
{
	int rc;

	switch (module->mtype) {
	case cmt_csrc:
	case cmt_chdr:
	case cmt_ir:
		break;
	case cmt_ic:
	case cmt_obj:
		(void)fprintf(stderr, "Error: Cannot dump IR for "
		    "'%s' file.\n", comp_mtype_str(module->mtype));
		return EINVAL;
	}

	rc = comp_module_make_ir(module);
	if (rc != EOK)
		return rc;

	assert(module->ir != NULL);
	rc = ir_module_print(module->ir, f);
	return rc;
}

/** Dump instruction code with virtual registers.
 *
 * @param module Compiler module
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_module_dump_vric(comp_module_t *module, FILE *f)
{
	int rc;

	switch (module->mtype) {
	case cmt_csrc:
	case cmt_chdr:
	case cmt_ir:
		break;
	case cmt_ic:
	case cmt_obj:
		(void)fprintf(stderr, "Error: Cannot dump VRIC for "
		    "'%s' file.\n", comp_mtype_str(module->mtype));
		return EINVAL;
	}

	rc = comp_module_make_vric(module);
	if (rc != EOK)
		return rc;

	assert(module->vric != NULL);
	rc = z80ic_module_print(module->vric, f);
	return rc;
}

/** Dump instruction code.
 *
 * @param module Compiler module
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_module_dump_ic(comp_module_t *module, FILE *f)
{
	int rc;

	assert(module->ic != NULL);
	rc = z80ic_module_print(module->ic, f);
	return rc;
}

/** Dump binary object.
 *
 * @param module Compiler module
 * @param f Output file
 * @return EOK on success or error code
 */
int comp_module_dump_obj(comp_module_t *module, FILE *f)
{
	int rc;

	if (module->object == NULL) {
		(void)fprintf(stderr, "Error: Object not built.\n");
		return EINVAL;
	}

	assert(module->object != NULL);
	rc = obj_object_dump(module->object, f);
	return rc;
}

/** Parser function to read input token from compiler.
 *
 * @param apinput Compiler parser input (comp_parser_input_t *)
 * @param atok Compiler token (comp_tok_t *)
 * @param indlvl Indentation level to annotate the token with
 * @param seccont Secondary continuation flag to annotate the token with
 * @param ltok Place to store token
 */
static void comp_parser_read_tok(void *apinput, void *atok,
    unsigned indlvl, bool seccont, lexer_tok_t *ltok)
{
	comp_parser_input_t *pinput = (comp_parser_input_t *)apinput;
	comp_tok_t *tok = (comp_tok_t *)atok;

	(void) pinput;
	(void) indlvl;
	(void) seccont;
	*ltok = tok->tok;
	/* Pass pointer to compiler token down to comp_parser_tok_data */
	ltok->udata = tok;
}

/** Parser function to get next input token from compiler.
 *
 * @param apinput Compiler parser input (comp_parser_input_t *)
 * @param atok Compiler token (comp_tok_t *)
 * @return Pointer to next token
 */
static void *comp_parser_next_tok(void *apinput, void *atok)
{
	comp_parser_input_t *pinput = (comp_parser_input_t *)apinput;
	comp_tok_t *tok = (comp_tok_t *)atok;
	comp_tok_t *ntok;

	(void) pinput;

	if (tok->tok.ttype != ltt_eof)
		ntok = comp_next_tok(tok);
	else
		ntok = tok;

	return (void *) ntok;
}

/** Get user data for a token.
 *
 * Return a pointer to the token. We can do this since we keep the
 * tokens in memory all the time.
 *
 * @param arg Compiler parser input (comp_parser_input_t)
 * @param tok Token
 * @param tok Place to store token
 */
static void *comp_parser_tok_data(void *apinput, void *tok)
{
	(void)apinput;

	/* Set this as user data for the AST token */
	return tok;
}

/** IR parser function to read current input token from compiler.
 *
 * @param apinput Compiler parser input (comp_ir_parser_input_t *)
 * @param ltok Place to store token
 */
static void comp_ir_parser_read_tok(void *apinput, ir_lexer_tok_t *itok)
{
	comp_ir_parser_input_t *pinput = (comp_ir_parser_input_t *)apinput;
	int rc;

	if (pinput->have_tok == false) {
		rc = ir_lexer_get_tok(pinput->ir_lexer, &pinput->itok);
		if (rc != EOK)
			itok->ttype = itt_invalid;

		pinput->have_tok = true;
	}

	*itok = pinput->itok;
}

/** IR parser function to advance to the next input token from compiler.
 *
 * @param apinput Compiler parser input (comp_ir_parser_input_t *)
 */
static void comp_ir_parser_next_tok(void *apinput)
{
	comp_ir_parser_input_t *pinput = (comp_ir_parser_input_t *)apinput;
	ir_lexer_tok_t itok;

	if (pinput->have_tok == false) {
		comp_ir_parser_read_tok(apinput, &itok);
		ir_lexer_free_tok(&itok);
	}

	pinput->have_tok = false;
}

/** Z80 IC parser function to read current input token from compiler.
 *
 * @param apinput Compiler parser input (comp_z80ic_parser_input_t *)
 * @param ltok Place to store token
 */
static void comp_z80ic_parser_read_tok(void *apinput, z80ic_lexer_tok_t *itok)
{
	comp_z80ic_parser_input_t *pinput =
	    (comp_z80ic_parser_input_t *)apinput;
	int rc;

	if (pinput->have_tok == false) {
		rc = z80ic_lexer_get_tok(pinput->ic_lexer, &pinput->itok);
		if (rc != EOK)
			itok->ttype = ztt_invalid;

		pinput->have_tok = true;
	}

	*itok = pinput->itok;
}

/** Z80 IC parser function to advance to the next input token from compiler.
 *
 * @param apinput Compiler parser input (comp_z80ic_parser_input_t *)
 */
static void comp_z80ic_parser_next_tok(void *apinput)
{
	comp_z80ic_parser_input_t *pinput =
	    (comp_z80ic_parser_input_t *)apinput;
	z80ic_lexer_tok_t itok;

	if (pinput->have_tok == false) {
		comp_z80ic_parser_read_tok(apinput, &itok);
		z80ic_lexer_free_tok(&itok);
	}

	pinput->have_tok = false;
}
