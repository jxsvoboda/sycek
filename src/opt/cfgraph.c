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
