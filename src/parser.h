/*
 * Copyright 2025 Jiri Svoboda
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
 * Parser
 */

#ifndef PARSER_H
#define PARSER_H

#include <types/ast.h>
#include <types/parser.h>

extern int parser_create(parser_input_ops_t *, void *, void *, unsigned,
    bool, parser_t **);
extern void parser_destroy(parser_t *);
extern int parser_process_module(parser_t *, ast_module_t **);
extern int parser_process_global_decln(parser_t *, ast_node_t **);
extern int parser_process_block(parser_t *, ast_block_t **);
extern int parser_process_stmt(parser_t *, ast_node_t **);
extern int parser_process_if_elseif(parser_t *, ast_if_t *);
extern int parser_process_if_else(parser_t *, ast_if_t *);
extern int parser_process_do_while(parser_t *, ast_do_t *);
extern bool parser_ttype_ignore(lexer_toktype_t);

#endif
