#ifndef PARSER_H
#define PARSER_H

#include <types/ast.h>
#include <types/parser.h>

extern int parser_create(parser_input_ops_t *, void *, parser_t **);
extern void parser_destroy(parser_t *);
extern int parser_process_module(parser_t *, ast_node_t **);

#endif
