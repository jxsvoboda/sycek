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

#include <adt/bitset.h>
#include <merrno.h>
#include <opt/cfgraph.h>
#include <opt/mapvarset.h>
#include <opt/reachdef.h>
#include <stdlib.h>

/** Compute reaching definitions on a control flow graph.
 *
 * @param cfg Control flow graph
 * @param rreachdef Place to store pointer to new reaching definitions
 * @return EOK on success or an error code
 */
int reachdef_compute(cfgraph_t *cfg, reachdef_t **rreachdef)
{
	reachdef_t *reachdef = NULL;
	cfgraph_bblock_operands_t *bbop = NULL;
	cfgraph_worklist_t *changed = NULL;
	cfgraph_bblock_t *block;
	cfgraph_bblock_t *pblock;
	cfgraph_arc_t *pred;
	cfgraph_arc_t *succ;
	bitset_t *oldout = NULL;
	bitset_t *tmp = NULL;
	map_varset_t *rdin = NULL;
	map_varset_t *rdout = NULL;
	int cmp;
	int rc;

	reachdef = calloc(1, sizeof(reachdef_t));
	if (reachdef == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = cfgraph_get_bblock_operands(cfg, &bbop);
	if (rc != EOK)
		goto error;

	rc = bitset_create(cfg->nvars, &oldout);
	if (rc != EOK)
		goto error;

	rc = bitset_create(cfg->nvars, &tmp);
	if (rc != EOK)
		goto error;

	rc = map_varset_create(cfg, cfg->next_bb_idx, &rdin);
	if (rc != EOK)
		goto error;

	rc = map_varset_create(cfg, cfg->next_bb_idx, &rdout);
	if (rc != EOK)
		goto error;

	rc = cfgraph_worklist_create(&changed);
	if (rc != EOK)
		goto error;

	/* Insert all nodes. */
	rc = cfgraph_worklist_insert_all(changed, cfg);
	if (rc != EOK)
		goto error;

	block = cfgraph_worklist_get_first(changed);
	while (block != NULL) {
		cfgraph_worklist_remove_first(changed);

		/* IN[block] := emptyset */
		bitset_empty(rdin->varset[(size_t)block->idx]);

		/*
		 * Calculate IN[block] from predecessors' OUT[pred].
		 */
		pred = cfgraph_pred_first(block);
		while (pred != NULL) {
			pblock = pred->src;

			// IN[block] = IN[block] U OUT[pred]
			bitset_union_2(rdout->varset[(size_t)pblock->idx],
			    rdin->varset[(size_t)block->idx]);

			pred = cfgraph_pred_next(pred);
		}

		/* oldout := OUT[block] */
		bitset_copy(rdout->varset[(size_t)block->idx], oldout);

		/* OUT[block] := GEN[block] U (IN[block] - KILL[block]) */
		bitset_diff_3(rdin->varset[(size_t)block->idx],
		    bbop->kill->varset[(size_t)block->idx], tmp);
		bitset_union_3(bbop->gen->varset[(size_t)block->idx],
		    tmp, rdout->varset[(size_t)block->idx]);

		/* if (OUT[block] != oldout) */
		cmp = bitset_cmp(rdout->varset[(size_t)block->idx],
		    oldout);
		if (cmp != 0) {
			succ = cfgraph_succ_first(block);
			while (succ != NULL) {
				rc = cfgraph_worklist_insert(changed,
				    succ->dest);
				if (rc != EOK)
					goto error;

				succ = cfgraph_succ_next(succ);
			}
		}
		block = cfgraph_worklist_get_first(changed);
	}

	cfgraph_bblock_operands_destroy(bbop);
	bitset_destroy(oldout);
	map_varset_destroy(rdin);
	map_varset_destroy(rdout);
	cfgraph_worklist_destroy(changed);
	*rreachdef = reachdef;
	return EOK;
error:
	if (bbop != NULL)
		cfgraph_bblock_operands_destroy(bbop);
	if (oldout != NULL)
		bitset_destroy(oldout);
	if (rdin != NULL)
		map_varset_destroy(rdin);
	if (rdout != NULL)
		map_varset_destroy(rdout);
	if (reachdef != NULL)
		free(reachdef);
	if (changed != NULL)
		cfgraph_worklist_destroy(changed);
	return rc;
}
