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
 * Map indices to sets of variables
 */

#include <assert.h>
#include <merrno.h>
#include <opt/cfgraph.h>
#include <opt/mapvarset.h>
#include <stdlib.h>

/** Create varset map.
 *
 * @param cfg Control flow graph
 * @param max_idx Maximum index
 * @param rmap Place to store pointer to new varset map.
 * @return EOk on success or an error code
 */
int map_varset_create(cfgraph_t *cfg, uint32_t max_idx, map_varset_t **rmap)
{
	map_varset_t *map;

	map = calloc(1, sizeof(map_varset_t));
	if (map == NULL)
		return ENOMEM;

	map->cfgraph = cfg;
	map->varset = calloc((size_t)max_idx, sizeof(bitset_t *));
	if (map->varset == NULL) {
		free(map);
		return ENOMEM;
	}

	*rmap = map;
	return EOK;
}

/** Destroy varset map.
 *
 * @param map varset map or @c NULL
 */
void map_varset_destroy(map_varset_t *map)
{
	if (map == NULL)
		return;

	free(map->varset);
	free(map);
}
