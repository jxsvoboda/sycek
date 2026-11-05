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
 * Strings.
 */

#include <stddef.h>
#include <string.h>

unsigned long strtoul(const char *str, char **eptr, int base)
{
	(void)str;
	(void)eptr;
	(void)base;
	return 0;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	(void)dest;
	(void)src;
	(void)n;
	return NULL;
}

void *memmove(void *dest, const void *src, size_t n)
{
	(void)dest;
	(void)src;
	(void)n;
	return NULL;
}

void *memset(void *buf, int c, size_t n)
{
	(void)buf;
	(void)c;
	(void)n;
	return NULL;
}

char *strchr(const char *s, int c)
{
	(void)s;
	(void)c;
	return NULL;
}

int strcmp(const char *a, const char *b)
{
	(void)a;
	(void)b;
	return 0;
}

int strncmp(const char *a, const char *b, size_t n)
{
	(void)a;
	(void)b;
	(void)n;
	return 0;
}

char *strdup(const char *str)
{
	(void)str;
	return NULL;
}

size_t strlen(const char *str)
{
	(void)str;
	return 0;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	(void)dest;
	(void)src;
	(void)n;
}

char *strrchr(const char *str, int c)
{
	(void)str;
	(void)c;
	return NULL;
}
