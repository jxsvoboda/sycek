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
 * Control flow graph.
 *
 * Control flow graph is an abstract representation of a procedure.
 * It can be used with different instruction sets (IR, Z80IC, ...).
 * The procedure consists of a set of basic blocks connected with arcs.
 */

#include <adt/bitset.h>
#include <adt/list.h>
#include <assert.h>
#include <merrno.h>
#include <opt/cfgraph.h>
#include <opt/mapvarset.h>
#include <stdlib.h>

/** Create control flow graph.
 *
 * @param rcfg Place to store pointer to new control flow graph.
 * @return EOk on success or an error code
 */
int cfgraph_create(cfgraph_t **rcfg)
{
	cfgraph_t *cfg;

	cfg = calloc(1, sizeof(cfgraph_t));
	if (cfg == NULL)
		return ENOMEM;

	list_initialize(&cfg->bblocks);
	cfg->next_bb_idx = 0;
	cfg->nvars = 10; /* XXX */

	*rcfg = cfg;
	return EOK;
}

/** Destroy control flow graph.
 *
 * @param cfg Control flow graph
 */
void cfgraph_destroy(cfgraph_t *cfg)
{
	free(cfg);
}

/** Return first basic block.
 *
 * @param cfg Control flow graph
 * @return First basic block or @c NULL if there are none
 */
cfgraph_bblock_t *cfgraph_bblock_first(cfgraph_t *cfg)
{
	link_t *link;

	link = list_first(&cfg->bblocks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_bblock_t, lbblocks);
}

/** Return next basic block.
 *
 * @param cur Current basic block
 * @return Next basic block or @c NULL @a cur is the last block
 */
cfgraph_bblock_t *cfgraph_bblock_next(cfgraph_bblock_t *cur)
{
	link_t *link;

	link = list_next(&cur->lbblocks, &cur->cfg->bblocks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_bblock_t, lbblocks);
}

/** Return arc from first predecessor of basic block.
 *
 * @param block Basic block
 * @return Arc from first predecessor of @a block or @c NULL if there are none
 */
cfgraph_arc_t *cfgraph_pred_first(cfgraph_bblock_t *bblock)
{
	link_t *link;

	link = list_first(&bblock->preds);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_arc_t, ldest);
}

/** Return arc from next predecessor of basic block.
 *
 * @param cur Arc from current predecessor
 * @return Arc from next predecessor or @c NULL if @a cur is the last arc
 */
cfgraph_arc_t *cfgraph_pred_next(cfgraph_arc_t *cur)
{
	link_t *link;

	link = list_next(&cur->ldest, &cur->dest->preds);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_arc_t, ldest);
}

/** Return arc to first successor of basic block.
 *
 * @param block Basic block
 * @return Arc to first successor of @a block or @c NULL if there are none
 */
cfgraph_arc_t *cfgraph_succ_first(cfgraph_bblock_t *bblock)
{
	link_t *link;

	link = list_first(&bblock->succs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_arc_t, lsrc);
}

/** Return arc to next successor of basic block.
 *
 * @param cur Arc to current successor
 * @return Arc to next successor or @c NULL if @a cur is the last arc
 */
cfgraph_arc_t *cfgraph_succ_next(cfgraph_arc_t *cur)
{
	link_t *link;

	link = list_next(&cur->lsrc, &cur->src->succs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_arc_t, lsrc);
}

/** Return first instruction in basic block.
 *
 * @param bblock Basic block
 * @return First instruction or @c NULL if there are none
 */
cfgraph_instr_t *cfgraph_instr_first(cfgraph_bblock_t *bblock)
{
	link_t *link;

	link = list_first(&bblock->instrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_instr_t, linstrs);
}

/** Return next instruction in basic block.
 *
 * @param cur Current instruction
 * @return Next instruction or @c NULL @a cur is the instruction
 */
cfgraph_instr_t *cfgraph_instr_next(cfgraph_instr_t *cur)
{
	link_t *link;

	link = list_next(&cur->linstrs, &cur->bblock->instrs);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cfgraph_instr_t, linstrs);
}

/*** XXX Compute instruction operands (GEN/KILL sets).
 *
 * @param instr Instruction
 * @param opbuf Operands structure where to store the information
 */
void cfgraph_instr_get_operands(cfgraph_instr_t *instr,
    cfgraph_instr_operands_t *opbuf)
{
	(void)instr;

	bitset_empty(opbuf->gen);
	bitset_empty(opbuf->kill);

	/* XXX */
	bitset_set(opbuf->gen, 0, 1);
	bitset_set(opbuf->kill, 1, 1);
}

/** Create basic block operand information structure.
 *
 * @param cfg Control flow graph
 * @param roperands Place to store pointer to new operands structure
 * @return EOK on success or an error code
 */
static int cfgraph_bblock_operands_create(cfgraph_t *cfg,
    cfgraph_bblock_operands_t **roperands)
{
	cfgraph_bblock_operands_t *operands;
	int rc;

	operands = calloc(1, sizeof(cfgraph_bblock_operands_t));
	if (operands == NULL)
		return ENOMEM;

	rc = map_varset_create(cfg, cfg->next_bb_idx, &operands->gen);
	if (rc != EOK)
		goto error;

	rc = map_varset_create(cfg, cfg->next_bb_idx, &operands->kill);
	if (rc != EOK)
		goto error;

	*roperands = operands;
	return EOK;
error:
	if (operands->gen != NULL)
		map_varset_destroy(operands->gen);
	free(operands);
	return rc;
}

int cfgraph_get_bblock_operands(cfgraph_t *cfg,
    cfgraph_bblock_operands_t **roperands)
{
	cfgraph_bblock_operands_t *operands = NULL;
	cfgraph_instr_operands_t *iop = NULL;
	cfgraph_bblock_t *bblock;
	cfgraph_instr_t *instr;
	int rc;

	rc = cfgraph_bblock_operands_create(cfg, &operands);
	if (rc != EOK)
		goto error;

	rc = cfgraph_instr_operands_create(cfg, &iop);
	if (rc != EOK)
		goto error;

	/* For each basic block. */
	bblock = cfgraph_bblock_first(cfg);
	while (bblock != NULL) {
		/* GEN[b] := {} */
		bitset_empty(operands->gen->varset[(size_t)bblock->idx]);
		/* KILL[b] := {} */
		bitset_empty(operands->kill->varset[(size_t)bblock->idx]);

		/* For each instruction in basic block. */
		instr = cfgraph_instr_first(bblock);
		while (instr != NULL) {
			/* Get instruction GEN/KILL sets. */
			cfgraph_instr_get_operands(instr, iop);

			/* GEN[b] := (GEN[b] - KILL[i]) U GEN[i] */
			bitset_diff_2(iop->kill,
			    operands->gen->varset[(size_t)bblock->idx]);
			bitset_union_2(iop->gen,
			    operands->gen->varset[(size_t)bblock->idx]);

			/* KILL[b] := KILL[b] U KILL[i] */
			bitset_union_2(iop->kill,
			    operands->kill->varset[(size_t)bblock->idx]);
			instr = cfgraph_instr_next(instr);
		}

		bblock = cfgraph_bblock_next(bblock);
	}

	cfgraph_instr_operands_destroy(iop);
	*roperands = operands;
	return EOK;
error:
	if (operands != NULL)
		cfgraph_bblock_operands_destroy(operands);
	return rc;
}

/** Destroy basci block operand information.
 *
 * @param operands Basic block operand information
 */
void cfgraph_bblock_operands_destroy(cfgraph_bblock_operands_t *operands)
{
	map_varset_destroy(operands->gen);
	map_varset_destroy(operands->kill);
	free(operands);
}

/** Create empty worklist.
 *
 * @param rworklist Place to store pointer to new worklist
 * @return EOK on success, ENOMEM if out of memory
 */
int cfgraph_worklist_create(cfgraph_worklist_t **rworklist)
{
	cfgraph_worklist_t *worklist;

	worklist = calloc(1, sizeof(cfgraph_worklist_t));
	if (worklist == NULL)
		return ENOMEM;

	list_initialize(&worklist->entries);
	*rworklist = worklist;
	return EOK;
}

/** Destroy worklist.
 *
 * @param worklist Worklist
 */
void cfgraph_worklist_destroy(cfgraph_worklist_t *worklist)
{
	cfgraph_bblock_t *bblock;

	bblock = cfgraph_worklist_get_first(worklist);
	while (bblock != NULL) {
		cfgraph_worklist_remove_first(worklist);
		bblock = cfgraph_worklist_get_first(worklist);
	}

	free(worklist);
}

/** Insert basic block (node) into worklist.
 *
 * @param worklist Worklist
 * @param bblock Basic block
 * @return EOK on success, ENOMEM if out of memory
 */
int cfgraph_worklist_insert(cfgraph_worklist_t *worklist,
    cfgraph_bblock_t *bblock)
{
	cfgraph_wlentry_t *entry;

	entry = calloc(1, sizeof(cfgraph_wlentry_t));
	if (entry == NULL)
		return ENOMEM;

	entry->worklist = worklist;
	entry->bblock = bblock;
	list_append(&entry->lentries, &worklist->entries);
	return EOK;
}

/** Get first basic block from worklist.
 *
 * @param worklist Worklist
 * @return First basic block in worklist or @c NULL if worklist is empty
 */
cfgraph_bblock_t *cfgraph_worklist_get_first(cfgraph_worklist_t *worklist)
{
	link_t *link;
	cfgraph_wlentry_t *entry;

	link = list_first(&worklist->entries);
	if (link == NULL)
		return NULL;

	entry = list_get_instance(link, cfgraph_wlentry_t, lentries);
	return entry->bblock;
}

/** Remove first basic block from worklist.
 *
 * @param worklist Worklist
 */
void cfgraph_worklist_remove_first(cfgraph_worklist_t *worklist)
{
	link_t *link;
	cfgraph_wlentry_t *entry;

	link = list_first(&worklist->entries);
	assert(link != NULL);

	entry = list_get_instance(link, cfgraph_wlentry_t, lentries);
	list_remove(&entry->lentries);
	free(entry);
}

/** Insert all basic blocks from control flow graph to worklist.
 *
 * @param worklist Worklist
 * @param cfg Control flow graph
 * @return EOK on success or an error code
 */
int cfgraph_worklist_insert_all(cfgraph_worklist_t *worklist, cfgraph_t *cfg)
{
	cfgraph_bblock_t *bblock;
	int rc;

	bblock = cfgraph_bblock_first(cfg);
	while (bblock != NULL) {
		rc = cfgraph_worklist_insert(worklist, bblock);
		if (rc != EOK)
			return rc;

		bblock = cfgraph_bblock_next(bblock);
	}

	return EOK;
}

/** Create instruction operand information structure.
 *
 * @param cfg Control flow graph
 * @param roperands Place to store pointer to new operands structure
 * @return EOK on success or an error code
 */
int cfgraph_instr_operands_create(cfgraph_t *cfg,
    cfgraph_instr_operands_t **roperands)
{
	cfgraph_instr_operands_t *operands;
	int rc;

	operands = calloc(1, sizeof(cfgraph_instr_operands_t));
	if (operands == NULL)
		return ENOMEM;

	rc = bitset_create(cfg->nvars, &operands->gen);
	if (rc != EOK)
		goto error;

	rc = bitset_create(cfg->nvars, &operands->kill);
	if (rc != EOK)
		goto error;

	*roperands = operands;
	return EOK;
error:
	if (operands->gen != NULL)
		bitset_destroy(operands->gen);
	free(operands);
	return rc;
}

/** Destroy instruction operand information.
 *
 * @param operands Instruction operand information
 */
void cfgraph_instr_operands_destroy(cfgraph_instr_operands_t *operands)
{
	bitset_destroy(operands->gen);
	bitset_destroy(operands->kill);
	free(operands);
}
