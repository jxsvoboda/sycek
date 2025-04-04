/*
 * Copyright 2024 Jiri Svoboda
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
 * Compiler types
 */

#ifndef TYPES_COMP_H
#define TYPES_COMP_H

#include <adt/list.h>
#include <types/ast.h>
#include <types/cgen.h>
#include <types/ir.h>
#include <types/irlexer.h>
#include <types/lexer.h>
#include <types/symbols.h>
#include <types/z80/z80ic.h>

/** Compiler token */
typedef struct comp_tok {
	/** Containing compiler module */
	struct comp_module *mod;
	/** Link in list of tokens */
	link_t ltoks;
	/** Lexer token */
	lexer_tok_t tok;
} comp_tok_t;

/** Compiler module type */
typedef enum {
	/** C source file */
	cmt_csrc,
	/** C header file */
	cmt_chdr,
	/** IR file */
	cmt_ir
} comp_mtype_t;

/** Compiler module */
typedef struct comp_module {
	/** Containing compiler */
	struct comp *comp;
	/** Tokens */
	list_t toks; /* of comp_tok_t */
	/** Module AST */
	ast_module_t *ast;
	/** Module symbols */
	symbols_t *symbols;
	/** Module IR */
	ir_module_t *ir;
	/** Module Z80 IC with virtual registers */
	z80ic_module_t *vric;
	/** Module Z80 IC */
	z80ic_module_t *ic;
} comp_module_t;

/** Compiler */
typedef struct comp {
	/** C lexer or @c NULL */
	lexer_t *lexer;
	/** IR lexer or @c NULL */
	ir_lexer_t *ir_lexer;
	/** Module */
	comp_module_t *mod;
	/** Module type */
	comp_mtype_t mtype;
	/** Code generator flags */
	cgen_flags_t cgflags;
} comp_t;

/** Compiler parser input */
typedef struct {
	int dummy;
} comp_parser_input_t;

/** Compiler IR parser input */
typedef struct {
	ir_lexer_t *ir_lexer;
	ir_lexer_tok_t itok;
	bool have_tok;
} comp_ir_parser_input_t;

/** Compiler flags */
typedef enum {
	/** Dump internal AST */
	compf_dump_ast = 0x1,
	/** Dump tokenized source file */
	compf_dump_toks = 0x2,
	/** Dump intermediate representation */
	compf_dump_ir = 0x4,
	/** Dump instruction code with virtual registers */
	compf_dump_vric = 0x8
} comp_flags_t;

#endif
