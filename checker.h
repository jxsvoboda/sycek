/*
 * Checker
 */

#ifndef CHECKER_H
#define CHECKER_H

#include <types/checker.h>
#include <types/lexer.h>

extern int checker_create(lexer_input_ops_t *, void *, checker_t **);
extern void checker_destroy(checker_t *);
extern int checker_run(checker_t *);

#endif
