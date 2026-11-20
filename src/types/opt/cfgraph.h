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
 * Compute reaching definitions on a control flow graph.
 */

#ifndef TYPES_OPT_CFGRAPH_H
#define TYPES_OPT_CFGRAPH_H

#include <adt/list.h>

/** Control flow graph. */
typedef struct cfgraph {
	/** Basic blocks (cfgraph_bblock_t) */
	list_t bblocks;
} cfgraph_t;

/** Basic block. */
typedef struct cfgraph_bblock {
	/** Containing control flow graph. */
	struct cfgraph *cfg;
	/** Link to @c cfgraph->bblocks. */
	link_t lbblocks;
	/** List of predecessors/incoming arcs (cfgraph_arc_t) */
	list_t preds;
	/** List of successors/outgoing args (cfgraph_arc_t) */
	list_t succs;
	/** Instructions (cfgraph_instr_t). */
	list_t instrs;
} cfgraph_bblock_t;

/** Control flow graph arc. */
typedef struct cfgraph_arc {
	/** Source basic block */
	struct cfgraph_bblock *src;
	/** Link to src->succs */
	link_t lsrc;
	/** Destination basic block */
	struct cfgraph_bblock *dest;
	/** Link to dest->preds */
	link_t ldest;
} cfgraph_arc_t;

/** Instruction. */
typedef struct cfgraph_instr {
	/** Containing basic block. */
	struct cfgraph_bblock *bblock;
	/** Link to @c bblcok->instrs. */
	link_t linstrs;
} cfgraph_instr_t;

/** Worklist entry */
typedef struct cfgraph_wlentry {
	/** Containing worklist. */
	struct cfgraph_worklist *worklist;
	/** Link to @c worklist->entries */
	link_t lentries;
	/** Basic block. */
	cfgraph_bblock_t *bblock;
} cfgraph_wlentry_t;

/** Worklist of basic blocks. */
typedef struct cfgraph_worklist {
	/** Worklist entries (cfgraph_wlentry_t) */
	list_t entries;
} cfgraph_worklist_t;

#endif
