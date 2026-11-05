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
 * Standard I/O
 */

#include <stddef.h>
#include <stdio.h>

#include "scr.h"

FILE *stdin;
FILE *stdout;
FILE *stderr;

int fclose(FILE *f)
{
	(void)f;
	return EOF;
}

int ferror(FILE *f)
{
	(void)f;
	return 0;
}

int fflush(FILE *f)
{
	(void)f;
	return EOF;
}

int fgetc(FILE *f)
{
	(void)f;
	return EOF;
}

FILE *fopen(const char *fname, const char *mode)
{
	(void)fname;
	(void)mode;
	return NULL;
}

int fprintf(FILE *f, const char *fmt, ...)
{
	(void)f;
	(void)fmt;
	return EOF;
}

int printf(char *fmt, ...)
{
	int cnt;

	cnt = 0;
	while (*fmt != '\0') {
		__scr_putchar(*fmt++);
		++cnt;
	}

	return cnt;
}

int fputc(int c, FILE *f)
{
	(void)c;
	(void)f;
	return EOF;
}

int fputs(const char *str, FILE *f)
{
	(void)str;
	(void)f;
	return EOF;
}

size_t fread(void *ptr, size_t size, size_t n, FILE *f)
{
	(void)ptr;
	(void)size;
	(void)n;
	(void)f;
	return 0;
}

int fseek(FILE *f, long offset, int whence)
{
	(void)f;
	(void)offset;
	(void)whence;
	return -1;
}

size_t fwrite(void *ptr, size_t size, size_t n, FILE *f)
{
	(void)ptr;
	(void)size;
	(void)n;
	(void)f;
	return 0;
}

int getchar(void)
{
	return EOF;
}

int getc(FILE *f)
{
	(void)f;
	return EOF;
}

int putchar(int c)
{
	(void)c;
	return EOF;
}

int remove(const char *fname)
{
	(void)fname;
	return -1;
}

int rename(const char *src, const char *dest)
{
	(void)src;
	(void)dest;
	return -1;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	(void)buf;
	(void)size;
	(void)fmt;
	return -1;
}

int asprintf(char **strp, const char *fmt, ...)
{
	(void)strp;
	(void)fmt;
	return -1;
}
