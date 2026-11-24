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
 * Control flow graph
 */

#ifndef OPT_CFGRAPH_H
#define OPT_CFGRAPH_H

#include <types/opt/cfgraph.h>

extern int cfgraph_create(cfgraph_t **);
extern void cfgraph_destroy(cfgraph_t *);
extern cfgraph_bblock_t *cfgraph_bblock_first(cfgraph_t *);
extern cfgraph_bblock_t *cfgraph_bblock_next(cfgraph_bblock_t *);
extern cfgraph_arc_t *cfgraph_pred_first(cfgraph_bblock_t *);
extern cfgraph_arc_t *cfgraph_pred_next(cfgraph_arc_t *);
extern cfgraph_arc_t *cfgraph_succ_first(cfgraph_bblock_t *);
extern cfgraph_arc_t *cfgraph_succ_next(cfgraph_arc_t *);
extern cfgraph_instr_t *cfgraph_instr_first(cfgraph_bblock_t *);
extern cfgraph_instr_t *cfgraph_instr_next(cfgraph_instr_t *);
extern void cfgraph_instr_get_operands(cfgraph_instr_t *,
    cfgraph_instr_operands_t *);
extern int cfgraph_get_bblock_operands(cfgraph_t *,
    cfgraph_bblock_operands_t **);
extern void cfgraph_bblock_operands_destroy(cfgraph_bblock_operands_t *);
extern int cfgraph_worklist_create(cfgraph_worklist_t **);
extern void cfgraph_worklist_destroy(cfgraph_worklist_t *);
extern int cfgraph_worklist_insert(cfgraph_worklist_t *, cfgraph_bblock_t *);
extern cfgraph_bblock_t *cfgraph_worklist_get_first(cfgraph_worklist_t *);
extern int cfgraph_instr_operands_create(cfgraph_t *,
    cfgraph_instr_operands_t **);
extern void cfgraph_instr_operands_destroy(cfgraph_instr_operands_t *);
extern void cfgraph_worklist_remove_first(cfgraph_worklist_t *);
extern int cfgraph_worklist_insert_all(cfgraph_worklist_t *, cfgraph_t *);

#endif
