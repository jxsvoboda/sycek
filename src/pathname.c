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
		off = (size_t)(sep - pathname);
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

/** Determine if pathname is basic name without directory components.
 *
 * @param pathname Pathname
 * @return @c true iff pathname does not contain directory components
 */
bool pathname_is_basic(const char *pathname)
{
	return strchr(pathname, '/') == NULL;
}

/** Determine if pathname exists.
 *
 * @param pathname Pathname
 * @return @c true iff pathname does not contain directory components
 */
bool pathname_exists(const char *pathname)
{
	FILE *f;

	/* This is not accurate, but it is portable. */
	f = fopen(pathname, "rb");
	if (f == NULL)
		return false;

	(void)fclose(f);
	return true;
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

/** Determine directory from which the executable was run.
 *
 * @param cmd Command used to run executable.
 * @param path_var Contents of PATH environment variable or @c NULL
 * @return Newly allocated string or @c NULL if executable directory
 *         cannot be determined.
 */
char *pathname_get_execdir(const char *cmd, const char *path_var)
{
	char *dpath;
	char *p;
	char *sep;
	char *exepath;
	char *dirname;

	if (!pathname_is_basic(cmd)) {
		/*
		 * Command is a relative or absolute pathname. Simply
		 * extract the directory name.
		 */
		return pathname_get_dirname(cmd);
	}

	if (path_var == NULL)
		return NULL;

	/* For each component of PATH look for 'cmd' in that directory. */

	dpath = strdup(path_var);
	if (dpath == NULL)
		return NULL;

	p = dpath;
	while (p[0] != '\0') {
		sep = strchr(p, ':');
		if (sep != NULL)
			sep[0] = '\0';

		exepath = pathname_compose(p, cmd);
		if (exepath == NULL) {
			free(dpath);
			return NULL;
		}

		/* More accurately we would test if it is executable. */
		if (pathname_exists(exepath)) {
			dirname = strdup(p);
			free(dpath);
			free(exepath);
			return dirname;
		}

		free(exepath);

		if (sep == NULL)
			break;

		p = sep + 1;
	}

	return NULL;
}
