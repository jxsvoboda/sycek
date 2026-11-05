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
 * Standard library.
 */

#include <stddef.h>
#include <stdlib.h>

void abort(void)
{
}

void *calloc(size_t n, size_t size)
{
	(void)n;
	(void)size;
}

void exit(int status)
{
	(void)status;
}

void free(void *ptr)
{
	(void)ptr;
}

char *getenv(const char *name)
{
	(void)name;
	return NULL;
}

void *malloc(size_t size)
{
	(void)size;
	return NULL;
}

void qsort(void *base, size_t n, size_t size, int (*compar)(const void *,
    const void *))
{
	(void)base;
	(void)n;
	(void)size;
	(void)compar;
}

void *realloc(void *ptr, size_t newsize)
{
	(void)ptr;
	(void)newsize;
	return NULL;
}
