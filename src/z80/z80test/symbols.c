/*
 * Copyright 2023 Jiri Svoboda
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
 * Symbol index
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adt/list.h"
#include "symbols.h"

enum {
	max_id_len = 128
};

/** Create new symbol index.
 *
 * @param rsymnols Place to store pointer to new symbol index
 * @return EOK on success, ENOMEM if out of memory
 */
int symbols_create(symbols_t **rsymbols)
{
	symbols_t *symbols;

	symbols = calloc(1, sizeof(symbols_t));
	if (symbols == NULL)
		return ENOMEM;

	list_initialize(&symbols->syms);
	*rsymbols = symbols;
	return 0;
}

/** Destroy symbol index.
 *
 * @param symbols Symbol index
 */
void symbols_destroy(symbols_t *symbols)
{
	symbol_t *symbol;

	symbol = symbols_first(symbols);
	while (symbol != NULL) {
		list_remove(&symbol->lsyms);
		free(symbol->ident);
		free(symbol);
		symbol = symbols_first(symbols);
	}

	free(symbols);
}

/** Symbol to symbol index.
 *
 * @param symbols Symbol index
 * @param ident Identifier
 * @param addr Address
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         symbol is already present
 */
int symbols_insert(symbols_t *symbols, const char *ident, uint16_t addr)
{
	symbol_t *symbol;
	char *dident;

	symbol = symbols_lookup(symbols, ident);
	if (symbol != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	symbol = calloc(1, sizeof(symbol_t));
	if (symbol == NULL)
		return ENOMEM;

	dident = strdup(ident);
	if (dident == NULL) {
		free(symbol);
		return ENOMEM;
	}

	symbol->ident = dident;
	symbol->addr = addr;
	symbol->symbols = symbols;
	list_append(&symbol->lsyms, &symbols->syms);
	return 0;
}

/** Get first symbol.
 *
 * @param symbols Symbols
 * @return First symbol or @c NULL if there are none
 */
symbol_t *symbols_first(symbols_t *symbols)
{
	link_t *link;

	link = list_first(&symbols->syms);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, symbol_t, lsyms);
}

/** Get next symbol.
 *
 * @param cur Current symbol
 * @return Next symbol or @c NULL if @a cur was the last one
 */
symbol_t *symbols_next(symbol_t *cur)
{
	link_t *link;

	link = list_next(&cur->lsyms, &cur->symbols->syms);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, symbol_t, lsyms);
}

/** Look up symbol by name.
 *
 * @param symbols Symbol index
 * @param ident Identifier
 * @return symbol or @c NULL if symbol is not found
 */
symbol_t *symbols_lookup(symbols_t *symbols, const char *ident)
{
	symbol_t *symbol;

	symbol = symbols_first(symbols);
	while (symbol != NULL) {
		if (strcmp(symbol->ident, ident) == 0)
			return symbol;

		symbol = symbols_next(symbol);
	}

	return NULL;
}

/** Load symbols from Z80asm-compatible map file.
 *
 * @param symbols Symbol index where symbols should be inserted
 * @param fname Map file name
 * @return Zero on success or an error code
 */
int symbols_mapfile_load(symbols_t *symbols, const char *fname)
{
	FILE *f;
	uint16_t addr;
	char ident[max_id_len + 1];
	int i;
	int digit;
	int rc;
	int c;

	f = fopen(fname, "rt");
	if (f == NULL)
		return EIO;

	while (true) {
		/* Stop if not an identifier */
		c = fgetc(f);
		if (c != '_' && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z') &&
		    (c < '0' || c > '9'))
			break;

		i = 0;
		ident[i++] = '@';

		/* Read the identifier */
		while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '_') {
			if (i >= max_id_len) {
				printf("Identifier too long.\n");
				return EINVAL;
			}

			ident[i++] = c;
			c = fgetc(f);
		}

		ident[i++] = '\0';

		/* Skip whitespace and "= $" */
		while (c == ' ' || c == '\t') {
			c = fgetc(f);
		}

		if (c != '=')
			return EIO;
		c = fgetc(f);

		if (c != ' ')
			return EIO;
		c = fgetc(f);

		if (c != '$')
			return EIO;
		c = fgetc(f);

		/* Read the address */
		addr = 0;
		while ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
		    (c >= '0' && c <= '9')) {
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else
				digit = c - 'A' + 10;

			addr = addr * 16 + digit;
			c = fgetc(f);
		}

		/* Skip the rest of the line */
		while (c >= '\0' && c != '\n') {
			c = fgetc(f);
		}

		rc = symbols_insert(symbols, ident, addr);
		if (rc != 0)
			return rc;
	}

	fclose(f);
	return 0;
}
