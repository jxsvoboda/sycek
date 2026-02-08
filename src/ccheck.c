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
 * C-style checker tool
 */

#include <checker.h>
#include <file_input.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <test/ast.h>
#include <test/checker.h>
#include <test/lexer.h>
#include <test/parser.h>

static void print_syntax(void)
{
	(void)printf("C-style checker\n");
	(void)printf("syntax:\n"
	    "\tccheck [options] <file> Check C-style in the specified file\n"
	    "\tccheck --test Run internal unit tests\n"
	    "options:\n"
	    "\t--fix Attempt to fix issues instead of just reporting them\n"
	    "\t--dump-ast Dump internal abstract syntax tree\n"
	    "\t--dump-toks Dump tokenized source file\n"
	    "\t-d <check> Disable a particular group of checks\n"
	    "\t  (attr, decl, estmt, fmt, hdr, invchar, loop, sclass)\n");
}

static int check_file(const char *fname, checker_flags_t flags,
    checker_cfg_t *cfg)
{
	int rc;
	int rv;
	checker_t *checker = NULL;
	checker_mtype_t mtype;
	char *bkname;
	const char *ext;
	file_input_t finput;
	FILE *f = NULL;

	ext = strrchr(fname, '.');
	if (ext == NULL) {
		(void)fprintf(stderr, "File '%s' has no extension.\n", fname);
		rc = EINVAL;
		goto error;
	}

	if (strcmp(ext, ".c") == 0 || strcmp(ext, ".C") == 0) {
		mtype = cmod_c;
	} else if (strcmp(ext, ".h") == 0 || strcmp(ext, ".H") == 0) {
		mtype = cmod_header;
	} else {
		(void)fprintf(stderr, "Unknown file extension '%s'.\n", ext);
		rc = EINVAL;
		goto error;
	}

	f = fopen(fname, "rt");
	if (f == NULL) {
		(void)fprintf(stderr, "Cannot open '%s'.\n", fname);
		rc = ENOENT;
		goto error;
	}

	file_input_init(&finput, f, fname);

	rc = checker_create(&lexer_file_input, &finput, mtype, cfg, &checker);
	if (rc != EOK)
		goto error;

	if ((flags & cf_dump_ast) != 0) {
		rc = checker_dump_ast(checker, stdout);
		if (rc != EOK)
			goto error;

		rv = printf("\n");
		if (rv < 0) {
			rc = EIO;
			goto error;
		}
	}

	if ((flags & cf_dump_toks) != 0) {
		rc = checker_dump_toks(checker, stdout);
		if (rc != EOK)
			goto error;

		rv = printf("\n");
		if (rv < 0) {
			rc = EIO;
			goto error;
		}
	}

	rc = checker_run(checker, (flags & cf_fix) != 0);
	if (rc != EOK)
		goto error;

	(void)fclose(f);
	f = NULL;

	if ((flags & cf_fix) != 0) {
		if (asprintf(&bkname, "%s.orig", fname) < 0) {
			rc = ENOMEM;
			goto error;
		}

		if (rename(fname, bkname) < 0) {
			(void)fprintf(stderr, "Error renaming '%s' to '%s'.\n",
			    fname, bkname);
			rc = EIO;
			goto error;
		}

		f = fopen(fname, "wt");
		if (f == NULL) {
			(void)fprintf(stderr, "Cannot open '%s' for writing.\n",
			    fname);
			rc = EIO;
			goto error;
		}

		rc = checker_print(checker, f);
		if (rc != EOK)
			goto error;

		if (fclose(f) < 0) {
			f = NULL;
			(void)fprintf(stderr, "Error writing '%s'.\n", fname);
			rc = EIO;
			goto error;
		}

		f = NULL;
	}

	checker_destroy(checker);

	return EOK;
error:
	if (checker != NULL)
		checker_destroy(checker);
	if (f != NULL)
		(void)fclose(f);
	return rc;
}

/** Disable a check group in configuration based on check name.
 *
 * @param cfg Configuration to alter
 * @param check_name Name of check to disable
 *
 * @return EOK on success, EINVAL if no such check exists
 */
static int check_disable(checker_cfg_t *cfg, const char *check_name)
{
	if (strcmp(check_name, "attr") == 0) {
		cfg->attr = false;
	} else if (strcmp(check_name, "decl") == 0) {
		cfg->decl = false;
	} else if (strcmp(check_name, "estmt") == 0) {
		cfg->estmt = false;
	} else if (strcmp(check_name, "fmt") == 0) {
		cfg->fmt = false;
	} else if (strcmp(check_name, "hdr") == 0) {
		cfg->hdr = false;
	} else if (strcmp(check_name, "invchar") == 0) {
		cfg->invchar = false;
	} else if (strcmp(check_name, "loop") == 0) {
		cfg->loop = false;
	} else if (strcmp(check_name, "nblock") == 0) {
		cfg->nblock = false;
	} else if (strcmp(check_name, "sclass") == 0) {
		cfg->sclass = false;
	} else {
		(void)fprintf(stderr, "Invalid check name '%s'.\n", check_name);
		return EINVAL;
	}

	return EOK;
}

int main(int argc, char *argv[])
{
	int rc;
	int i;
	checker_flags_t flags = cf_none;
	checker_cfg_t cfg;

	checker_cfg_init(&cfg);

	(void)argc;
	(void)argv;

	if (argc < 2) {
		print_syntax();
		return 1;
	}

	if (argc == 2 && strcmp(argv[1], "--test") == 0) {
		/* Run tests */
		rc = test_lexer();
		(void)printf("test_lexer -> %d\n", rc);
		if (rc != EOK)
			return 1;

		rc = test_ast();
		(void)printf("test_ast -> %d\n", rc);
		if (rc != EOK)
			return 1;

		rc = test_parser();
		(void)printf("test_parser -> %d\n", rc);
		if (rc != EOK)
			return 1;

		rc = test_checker();
		(void)printf("test_checker -> %d\n", rc);
		if (rc != EOK)
			return 1;
	} else {
		i = 1;
		while (argc > i && argv[i][0] == '-') {
			if (strcmp(argv[i], "--fix") == 0) {
				++i;
				flags |= cf_fix;
			} else if (strcmp(argv[i], "--dump-ast") == 0) {
				++i;
				flags |= cf_dump_ast;
			} else if (strcmp(argv[i], "--dump-toks") == 0) {
				++i;
				flags |= cf_dump_toks;
			} else if (strcmp(argv[i], "-") == 0) {
				++i;
				break;
			} else if (strcmp(argv[i], "-d") == 0) {
				++i;
				if (argc <= i) {
					(void)fprintf(stderr,
					    "Option '-d' needs an argument.\n");
					return 1;
				}

				rc = check_disable(&cfg, argv[i]);
				if (rc != EOK)
					return 1;
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

		rc = check_file(argv[i], flags, &cfg);
	}

	if (rc != EOK)
		return 1;

	return 0;
}
