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
 * Disassembler
 */

#include <merrno.h>
#include <object/object.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_syntax(void)
{
	(void)printf("Disassembler\n");
	(void)printf("syntax:\n"
	    "\tsydis [options] <file> Disassemble file\n"
	    "disassembler options:\n"
	    "\tnone\n");
}

/** Disassemble one input file.
 *
 * @param fname Input file name
 *
 * @return EOK on succcess or an error code
 */
static int disassemble_file(const char *fname)
{
	obj_object_t *object = NULL;
	int rc;
	FILE *f = NULL;
	char *ext;

	ext = strrchr(fname, '.');
	if (ext == NULL) {
		(void)fprintf(stderr, "File '%s' has no extension.\n", fname);
		rc = EINVAL;
		goto error;
	}

	f = fopen(fname, "rb");
	if (f == NULL) {
		(void)fprintf(stderr, "Cannot open '%s'.\n", fname);
		rc = ENOENT;
		goto error;
	}

	if (strcmp(ext, ".obj") == 0 || strcmp(ext, ".OBJ") == 0) {
		printf("load object..\n");
		rc = obj_object_load_obj(f, fname, &object);
		if (rc != EOK)
			goto error;
	} else {
		(void)fprintf(stderr, "Unknown file extension '%s'.\n", ext);
		rc = EINVAL;
		goto error;
	}

	(void)obj_object_dump(object, stdout);

	(void)fclose(f);

	return EOK;
error:
	if (f != NULL)
		(void)fclose(f);
	return rc;
}

int main(int argc, char *argv[])
{
	int rc;
	int i;

	if (argc < 2) {
		print_syntax();
		return 1;
	}

	i = 1;
	while (argc > i && argv[i][0] == '-') {
		if (strcmp(argv[i], "--foo") == 0) {
			++i;
		} else {
			(void)fprintf(stderr, "Invalid option.\n");
			return 1;
		}
	}

	if (argc <= i) {
		(void)fprintf(stderr, "Argument missing.\n");
		return 1;
	}

	if (i < argc) {
		rc = disassemble_file(argv[i++]);
		if (rc != EOK)
			return 1;
	}

	if (i < argc) {
		(void)fprintf(stderr, "Unexpected argument.\n");
		return 1;
	}

	return 0;
}
