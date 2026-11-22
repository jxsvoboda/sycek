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

#include <assert.h>
#include <merrno.h>
#include <opt/cfgraph.h>
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
