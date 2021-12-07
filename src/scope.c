/*
 * Copyright 2021 Jiri Svoboda
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
 * Identifier scope
 */

#include <merrno.h>
#include <scope.h>
#include <stdlib.h>
#include <string.h>

/** Create new identifier scope.
 *
 * @param parent Parent scope
 * @param rscope Place to store pointer to new scope
 * @return EOK on success, ENOMEM if out of memory
 */
int scope_create(scope_t *parent, scope_t **rscope)
{
	scope_t *scope;

	scope = calloc(1, sizeof(scope_t));
	if (scope == NULL)
		return ENOMEM;

	scope->parent = parent;
	list_initialize(&scope->members);
	*rscope = scope;
	return EOK;
}

/** Destroy identifier scope.
 *
 * @param scope Scope
 */
void scope_destroy(scope_t *scope)
{
	scope_member_t *member;

	member = scope_first(scope);
	while (member != NULL) {
		list_remove(&member->lmembers);
		free(member->ident);
		free(member);
		member = scope_first(scope);
	}

	free(scope);
}

/** Insert global symbol to identifier scope.
 *
 * @param scope Scope
 * @param ident Identifier
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         identifier is already present in the scope
 */
int scope_insert_gsym(scope_t *scope, const char *ident)
{
	scope_member_t *member;
	char *dident;

	member = scope_lookup_local(scope, ident);
	if (member != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	member = calloc(1, sizeof(scope_member_t));
	if (member == NULL)
		return ENOMEM;

	dident = strdup(ident);
	if (dident == NULL) {
		free(member);
		return ENOMEM;
	}

	member->ident = dident;
	member->mtype = sm_gsym;
	member->scope = scope;
	list_append(&member->lmembers, &scope->members);
	return EOK;
}

/** Insert function argument to identifier scope.
 *
 * @param scope Scope
 * @param ident Identifier
 * @param idx Argument variable identifer (e.g. '%0')
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         identifier is already present in the scope
 */
int scope_insert_arg(scope_t *scope, const char *ident, const char *vident)
{
	scope_member_t *member;
	char *dident;

	member = scope_lookup_local(scope, ident);
	if (member != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	member = calloc(1, sizeof(scope_member_t));
	if (member == NULL)
		return ENOMEM;

	dident = strdup(ident);
	if (dident == NULL) {
		free(member);
		return ENOMEM;
	}

	member->ident = dident;
	member->mtype = sm_arg;
	member->m.arg.vident = vident;
	member->scope = scope;
	list_append(&member->lmembers, &scope->members);
	return EOK;
}

/** Insert local variable to identifier scope.
 *
 * @param scope Scope
 * @param ident Identifier
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         identifier is already present in the scope
 */
int scope_insert_lvar(scope_t *scope, const char *ident)
{
	scope_member_t *member;
	char *dident;

	member = scope_lookup_local(scope, ident);
	if (member != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	member = calloc(1, sizeof(scope_member_t));
	if (member == NULL)
		return ENOMEM;

	dident = strdup(ident);
	if (dident == NULL) {
		free(member);
		return ENOMEM;
	}

	member->ident = dident;
	member->mtype = sm_lvar;
	member->scope = scope;
	list_append(&member->lmembers, &scope->members);
	return EOK;
}

/** Get first (local) scope member.
 *
 * @param scope Scope
 * @return First member or @c NULL if there are none
 */
scope_member_t *scope_first(scope_t *scope)
{
	link_t *link;

	link = list_first(&scope->members);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, scope_member_t, lmembers);
}

/** Get next (local) scope member.
 *
 * @param cur Current member
 * @return NExt member or @c NULL if @a cur was the last one
 */
scope_member_t *scope_next(scope_member_t *cur)
{
	link_t *link;

	link = list_next(&cur->lmembers, &cur->scope->members);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, scope_member_t, lmembers);
}

/** Look up identifier in a scope (but not in the ancestor scopes).
 *
 * @param scope Scope
 * @param ident Identifier
 * @return Member or @c NULL if identifier is not found
 */
scope_member_t *scope_lookup_local(scope_t *scope, const char *ident)
{
	scope_member_t *member;

	member = scope_first(scope);
	while (member != NULL) {
		if (strcmp(member->ident, ident) == 0)
			return member;

		member = scope_next(member);
	}

	return NULL;
}

/** Look up identifier in a scope (and in the ancestor scopes).
 *
 * @param scope Scope
 * @param ident Identifier
 * @return Member or @c NULL if identifier is not found
 */
scope_member_t *scope_lookup(scope_t *scope, const char *ident)
{
	scope_member_t *member;

	member = scope_lookup_local(scope, ident);
	if (member != NULL)
		return member;

	if (scope->parent == NULL)
		return NULL;

	member = scope_lookup(scope->parent, ident);
	if (member != NULL)
		return member;

	return NULL;
}
