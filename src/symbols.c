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
 *
 * Keep track of symbols being declared, defined, extern, used.
 * Symbols correspond to identifiers in the global scope.
 */

#include <cgtype.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>
#include <symbols.h>

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
	return EOK;
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
		cgtype_destroy(symbol->cgtype);
		free(symbol->irident);
		free(symbol);
		symbol = symbols_first(symbols);
	}

	free(symbols);
}

/** Insert new symbol to symbol index.
 *
 * @param symbols Symbol index
 * @param stype Symbol type
 * @param tok Identifier token that declared or defined symbol or @c NULL
 *            for anonymous symbol
 * @param irident IR identifier
 * @param rsymbol Place to store pointer to new symbol or @c NULL
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         symbol is already present
 */
int symbols_insert(symbols_t *symbols, symbol_type_t stype, comp_tok_t *tok,
    const char *irident, symbol_t **rsymbol)
{
	symbol_t *symbol;

	if (tok != NULL) {
		symbol = symbols_lookup(symbols, tok->tok.text);
		if (symbol != NULL) {
			/* Identifier already exists */
			return EEXIST;
		}
	}

	symbol = calloc(1, sizeof(symbol_t));
	if (symbol == NULL)
		return ENOMEM;

	symbol->ident = tok;
	symbol->stype = stype;
	symbol->irident = strdup(irident);
	if (symbol->irident == NULL) {
		free(symbol);
		return ENOMEM;
	}

	symbol->symbols = symbols;
	list_append(&symbol->lsyms, &symbols->syms);
	if (rsymbol != NULL)
		*rsymbol = symbol;
	return EOK;
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
		if (symbol->ident != NULL &&
		    strcmp(symbol->ident->tok.text, ident) == 0)
			return symbol;

		symbol = symbols_next(symbol);
	}

	return NULL;
}
