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
 * Pathname manipulation
 */

#include <pathname.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Get directory component from pathname.
 *
 * Return the directory component of pathname. If the pathname does not
 * contain any directory components, return "." (the current directory).
 *
 * @param pathname Pathname
 * @return Directory name (newly allocated string) or @c NULL
 */
char *pathname_get_dirname(const char *pathname)
{
	char *sep;
	char *dirname;
	size_t off;

	sep = strrchr(pathname, '/');
	if (sep != NULL) {
		off = sep - pathname;
		dirname = malloc(off + 1);
		if (dirname == NULL)
			return NULL;

		memcpy(dirname, pathname, off);
		dirname[off] = '\0';
	} else {
		dirname = strdup(".");
	}

	return dirname;
}

/** Determine if pathname is absolute.
 *
 * @param pathname Pathname
 * @return @c true iff pathname is absolute
 */
bool pathname_is_absolute(const char *pathname)
{
	return pathname[0] == '/';
}

/** Compose two paths together.
 *
 * If @a sub is relative, it is resolved relative to @a base. If @a sub
 * is absolute, it is left as is.
 *
 * @param base Base path
 * @param sub Subordinate path
 * @reurn Newly allocated string or @c NULL.
 */
char *pathname_compose(const char *base, const char *sub)
{
	char *pathname;
	int rv;

	if (pathname_is_absolute(sub)) {
		pathname = strdup(sub);
		return pathname;
	}

	rv = asprintf(&pathname, "%s/%s", base, sub);
	if (rv < 0)
		return NULL;

	return pathname;
}
