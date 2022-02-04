/*
 * Copyright 2022 Jiri Svoboda
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
 * Test identifier scope
 */

#include <scope.h>
#include <string.h>
#include <merrno.h>
#include <test/scope.h>

/** Test creating and destroying a scope.
 *
 * @return EOK on success or non-zero error code
 */
static int test_scope_create_destroy(void)
{
	int rc;
	scope_t *scope = NULL;

	rc = scope_create(NULL, &scope);
	if (rc != EOK)
		return rc;

	scope_destroy(scope);
	return EOK;
}

/** Test inserting a global symbol.
 *
 * @return EOK on success or non-zero error code
 */
static int test_scope_insert_gsym(void)
{
	int rc;
	scope_t *scope = NULL;
	scope_member_t *member;

	rc = scope_create(NULL, &scope);
	if (rc != EOK)
		goto error;

	rc = scope_insert_gsym(scope, "a");
	if (rc != EOK)
		goto error;

	rc = scope_insert_gsym(scope, "a");
	if (rc != EEXIST)
		goto error;

	member = scope_first(scope);
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "a") != 0)
		goto error;

	if (member->mtype != sm_gsym)
		goto error;

	scope_destroy(scope);
	return EOK;
error:
	if (scope != NULL)
		scope_destroy(scope);
	return EINVAL;
}

/** Test inserting an argument.
 *
 * @return EOK on success or non-zero error code
 */
static int test_scope_insert_arg(void)
{
	int rc;
	scope_t *scope = NULL;
	scope_member_t *member;

	rc = scope_create(NULL, &scope);
	if (rc != EOK)
		goto error;

	rc = scope_insert_arg(scope, "a", "%0");
	if (rc != EOK)
		goto error;

	rc = scope_insert_arg(scope, "a", "%1");
	if (rc != EEXIST)
		goto error;

	member = scope_first(scope);
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "a") != 0)
		goto error;
	if (member->mtype != sm_arg)
		goto error;
	if (member->mtype != sm_arg)
		goto error;

	scope_destroy(scope);
	return EOK;
error:
	if (scope != NULL)
		scope_destroy(scope);
	return EINVAL;
}

/** Test inserting a local variable.
 *
 * @return EOK on success or non-zero error code
 */
static int test_scope_insert_lvar(void)
{
	int rc;
	scope_t *scope = NULL;
	scope_member_t *member;

	rc = scope_create(NULL, &scope);
	if (rc != EOK)
		goto error;

	rc = scope_insert_lvar(scope, "a", "%a");
	if (rc != EOK)
		goto error;

	rc = scope_insert_lvar(scope, "a", "%a");
	if (rc != EEXIST)
		goto error;

	member = scope_first(scope);
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "a") != 0)
		goto error;
	if (member->mtype != sm_lvar)
		goto error;

	scope_destroy(scope);
	return EOK;
error:
	if (scope != NULL)
		scope_destroy(scope);
	return EINVAL;
}

/** Test scope_first() and scope_next().
 *
 * @return EOK on success or non-zero error code
 */
static int test_scope_first_next(void)
{
	int rc;
	scope_t *scope = NULL;
	scope_member_t *member;

	rc = scope_create(NULL, &scope);
	if (rc != EOK)
		goto error;

	member = scope_first(scope);
	if (member != NULL)
		goto error;

	rc = scope_insert_lvar(scope, "a", "%a");
	if (rc != EOK)
		goto error;

	rc = scope_insert_lvar(scope, "b", "%b");
	if (rc != EOK)
		goto error;

	member = scope_first(scope);
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "a") != 0)
		goto error;

	member = scope_next(member);
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "b") != 0)
		goto error;

	member = scope_next(member);
	if (member != NULL)
		goto error;

	scope_destroy(scope);
	return EOK;
error:
	if (scope != NULL)
		scope_destroy(scope);
	return EINVAL;
}

/** Test scope_lookup_local().
 *
 * @return EOK on success or non-zero error code
 */
static int test_scope_lookup_local(void)
{
	int rc;
	scope_t *parent = NULL;
	scope_t *child = NULL;
	scope_member_t *member;

	rc = scope_create(NULL, &parent);
	if (rc != EOK)
		goto error;

	member = scope_lookup_local(parent, "a");
	if (member != NULL)
		goto error;

	rc = scope_create(parent, &child);
	if (rc != EOK)
		goto error;

	member = scope_lookup_local(child, "a");
	if (member != NULL)
		goto error;

	rc = scope_insert_lvar(child, "a", "%a");
	if (rc != EOK)
		goto error;

	member = scope_lookup_local(child, "a");
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "a") != 0)
		goto error;
	if (member->scope != child)
		goto error;

	scope_destroy(child);
	scope_destroy(parent);
	return EOK;
error:
	if (child != NULL)
		scope_destroy(child);
	if (parent != NULL)
		scope_destroy(parent);
	return EINVAL;
}

/** Test scope_lookup().
 *
 * @return EOK on success or non-zero error code
 */
static int test_scope_lookup(void)
{
	int rc;
	scope_t *parent = NULL;
	scope_t *child = NULL;
	scope_member_t *member;

	rc = scope_create(NULL, &parent);
	if (rc != EOK)
		goto error;

	member = scope_lookup(parent, "a");
	if (member != NULL)
		goto error;

	rc = scope_insert_lvar(parent, "a", "%a");
	if (rc != EOK)
		goto error;

	member = scope_lookup(parent, "a");
	if (member == NULL)
		goto error;

	rc = scope_create(parent, &child);
	if (rc != EOK)
		goto error;

	member = scope_lookup(child, "a");
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "a") != 0)
		goto error;
	if (member->scope != parent)
		goto error;

	rc = scope_insert_lvar(child, "a", "%a");
	if (rc != EOK)
		goto error;

	member = scope_lookup(child, "a");
	if (member == NULL)
		goto error;

	if (strcmp(member->ident, "a") != 0)
		goto error;
	if (member->scope != child)
		goto error;

	scope_destroy(child);
	scope_destroy(parent);
	return EOK;
error:
	if (child != NULL)
		scope_destroy(child);
	if (parent != NULL)
		scope_destroy(parent);
	return EINVAL;
}

/** Run identifier scope tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_scope(void)
{
	int rc;

	rc = test_scope_create_destroy();
	if (rc != EOK)
		return rc;

	rc = test_scope_insert_gsym();
	if (rc != EOK)
		return rc;

	rc = test_scope_insert_arg();
	if (rc != EOK)
		return rc;

	rc = test_scope_insert_lvar();
	if (rc != EOK)
		return rc;

	rc = test_scope_first_next();
	if (rc != EOK)
		return rc;

	rc = test_scope_lookup_local();
	if (rc != EOK)
		return rc;

	rc = test_scope_lookup();
	if (rc != EOK)
		return rc;

	return EOK;
}
