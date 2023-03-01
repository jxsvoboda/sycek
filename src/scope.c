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
 * Identifier scope
 */

#include <cgtype.h>
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
 * @param scope Scope or @c NULL
 */
void scope_destroy(scope_t *scope)
{
	scope_member_t *member;

	if (scope == NULL)
		return;

	member = scope_first(scope);
	while (member != NULL) {
		list_remove(&member->lmembers);
		switch (member->mtype) {
		case sm_gsym:
			break;
		case sm_arg:
			free(member->m.arg.vident);
			break;
		case sm_lvar:
			free(member->m.lvar.vident);
			break;
		case sm_tdef:
			break;
		}

		cgtype_destroy(member->cgtype);
		free(member);
		member = scope_first(scope);
	}

	free(scope);
}

/** Insert global symbol to identifier scope.
 *
 * @param scope Scope
 * @param tident Identifier token
 * @param cgtype C type of the global symbol
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         identifier is already present in the scope
 */
int scope_insert_gsym(scope_t *scope, lexer_tok_t *tident, cgtype_t *cgtype)
{
	scope_member_t *member;
	cgtype_t *dtype = NULL;
	int rc;

	member = scope_lookup_local(scope, tident->text);
	if (member != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	member = calloc(1, sizeof(scope_member_t));
	if (member == NULL)
		return ENOMEM;

	rc = cgtype_clone(cgtype, &dtype);
	if (rc != EOK) {
		free(member);
		return ENOMEM;
	}

	member->tident = tident;
	member->cgtype = dtype;
	member->mtype = sm_gsym;
	member->scope = scope;
	list_append(&member->lmembers, &scope->members);
	return EOK;
}

/** Insert function argument to identifier scope.
 *
 * @param scope Scope
 * @param tident Identifier token
 * @param cgtype C type of the argument
 * @param vident Argument variable identifer (e.g. '%0')
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         identifier is already present in the scope
 */
int scope_insert_arg(scope_t *scope, lexer_tok_t *tident, cgtype_t *cgtype,
    const char *vident)
{
	scope_member_t *member;
	char *dvident;
	cgtype_t *dtype = NULL;
	int rc;

	member = scope_lookup_local(scope, tident->text);
	if (member != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	member = calloc(1, sizeof(scope_member_t));
	if (member == NULL)
		return ENOMEM;

	dvident = strdup(vident);
	if (dvident == NULL) {
		free(member);
		return ENOMEM;
	}

	rc = cgtype_clone(cgtype, &dtype);
	if (rc != EOK) {
		free(dvident);
		free(member);
		return ENOMEM;
	}

	member->tident = tident;
	member->cgtype = dtype;
	member->mtype = sm_arg;
	member->m.arg.vident = dvident;
	member->scope = scope;
	list_append(&member->lmembers, &scope->members);
	return EOK;
}

/** Insert local variable to identifier scope.
 *
 * @param scope Scope
 * @param tident Identifier token
 * @param cgtype C type of the variable
 * @param vident IR variable identifer (e.g. '%foo')
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         identifier is already present in the scope
 */
int scope_insert_lvar(scope_t *scope, lexer_tok_t *tident, cgtype_t *cgtype,
    const char *vident)
{
	scope_member_t *member;
	char *dvident;
	cgtype_t *dtype = NULL;
	int rc;

	member = scope_lookup_local(scope, tident->text);
	if (member != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	member = calloc(1, sizeof(scope_member_t));
	if (member == NULL)
		return ENOMEM;

	dvident = strdup(vident);
	if (dvident == NULL) {
		free(member);
		return ENOMEM;
	}

	rc = cgtype_clone(cgtype, &dtype);
	if (rc != EOK) {
		free(dvident);
		free(member);
		return ENOMEM;
	}

	member->tident = tident;
	member->cgtype = dtype;
	member->mtype = sm_lvar;
	member->m.lvar.vident = dvident;
	member->scope = scope;
	list_append(&member->lmembers, &scope->members);
	return EOK;
}

/** Insert typedef to identifier scope.
 *
 * @param scope Scope
 * @param tident Identifier token
 * @param cgtype C type to which the typedef expands
 * @return EOK on success, ENOMEM if out of memory, EEXIST if the
 *         identifier is already present in the scope
 */
int scope_insert_tdef(scope_t *scope, lexer_tok_t *tident, cgtype_t *cgtype)
{
	scope_member_t *member;
	cgtype_t *dtype = NULL;
	int rc;

	member = scope_lookup_local(scope, tident->text);
	if (member != NULL) {
		/* Identifier already exists */
		return EEXIST;
	}

	member = calloc(1, sizeof(scope_member_t));
	if (member == NULL)
		return ENOMEM;

	rc = cgtype_clone(cgtype, &dtype);
	if (rc != EOK) {
		free(member);
		return ENOMEM;
	}

	member->tident = tident;
	member->cgtype = dtype;
	member->mtype = sm_tdef;
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
 * @return Next member or @c NULL if @a cur was the last one
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
		if (strcmp(member->tident->text, ident) == 0)
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
