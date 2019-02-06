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
 * Checker
 */

#ifndef TYPES_CHECKER_H
#define TYPES_CHECKER_H

#include <adt/list.h>
#include <stdbool.h>
#include <types/ast.h>
#include <types/lexer.h>

/** Checker token */
typedef struct {
	/** Containing checker module */
	struct checker_module *mod;
	/** Link in list of tokens */
	link_t ltoks;
	/** Lexer token */
	lexer_tok_t tok;
	/** Checked: if true, indlvl, lbegin and seccont are valid */
	bool checked;
	/** Indentation level assigned by checker */
	unsigned indlvl;
	/** Indentation level assigned by parser */
	unsigned pindlvl;
	/** Token is supposed to begin a new line */
	bool lbegin;
	/** Token, if begginning a line, is a secondary continuation */
	bool seccont;
	/** Secondary continuation flag assigned by checker */
	bool pseccont;
} checker_tok_t;

/** Checker module type */
typedef enum {
	/** C file */
	cmod_c,
	/** Header file */
	cmod_header
} checker_mtype_t;

/** Checker module */
typedef struct checker_module {
	/** Containing checker */
	struct checker *checker;
	/** Tokens */
	list_t toks; /* of checker_tok_t */
	/** Module AST */
	ast_module_t *ast;
} checker_module_t;

/** Checker configuration */
typedef struct {
	/** Attribute issues */
	bool attr;
	/** Declaration style */
	bool decl;
	/** Empty statement */
	bool estmt;
	/** Check formatting */
	bool fmt;
	/** Check header style */
	bool hdr;
	/** Check for invalid characters */
	bool invchar;
	/** Check loop style */
	bool loop;
	/** Nested block */
	bool nblock;
	/** Check storage class issues */
	bool sclass;
} checker_cfg_t;

/** Checker scope */
typedef struct {
	/** Scope indentation level, starting from zero */
	unsigned indlvl;
	/** This scope uses secondary indentation */
	bool secindent;
	/** Module */
	checker_module_t *mod;
	/** @c true to attempt to fix issues instead of reporting them */
	bool fix;
} checker_scope_t;

/** Checker */
typedef struct checker {
	/** Lexer */
	lexer_t *lexer;
	/** Module */
	checker_module_t *mod;
	/** Module type */
	checker_mtype_t mtype;
	/** Checker configuration */
	checker_cfg_t *cfg;
} checker_t;

/** Checker parser input */
typedef struct {
	int dummy;
} checker_parser_input_t;

/** Checker flags */
typedef enum {
	/** Attempt to fix issues instead of reporting them */
	cf_fix = 0x1,
	/** Dump internal AST */
	cf_dump_ast = 0x2,
	/** Dump tokenized source file */
	cf_dump_toks = 0x4
} checker_flags_t;

/** Allow or disallow null statement */
typedef enum {
	/** Disallow null statement */
	cns_disallow,
	/** Allow null statement */
	cns_allow
} checker_ns_allow_t;

#endif
