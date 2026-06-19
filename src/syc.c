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
 * C compiler / static checker
 */

#include <comp.h>
#include <file_input.h>
#include <lexer.h>
#include <merrno.h>
#include <object/linker.h>
#include <parser.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <test/cgen.h>
#include <test/cgtype.h>
#include <test/comp.h>
#include <test/ir.h>
#include <test/scope.h>
#include <test/irlexer.h>
#include <test/z80/isel.h>
#include <test/z80/ralloc.h>
#include <test/z80/z80ic.h>

static void print_syntax(void)
{
	(void)printf("C compiler / static checker\n");
	(void)printf("syntax:\n"
	    "\tsyc [options] <file>... Compile / check the specified file(s)\n"
	    "\tsyc --test Run internal unit tests\n"
	    "compiler options:\n"
	    "\t--dump-ast Dump internal abstract syntax tree\n"
	    "\t--dump-toks Dump tokenized source file\n"
	    "\t--dump-ir Dump intermediate representation\n"
	    "\t--dump-vric Dump instruction code with virtual registers\n"
	    "\t--dump-obj Dump binary object\n"
	    "\t--no-emit Do not emit binary object, stop after compile stage\n"
	    "\t--no-link Do not link, stop after binary object emission\n"
	    "\t--no-tape Do not make a tape image, stop after link stage\n"
	    "\t--out=<fname> Output file name\n"
	    "code generation options:\n"
	    "\t--lvalue-args Make function arguments writable/addressable\n"
	    "\t--int-promotion Enable integer promotion\n"
	    "linker options:\n"
	    "\t--no-link-range-error Disable link error if binary is "
	    "too large\n");
}

/** Replace filename extension with a different one.
 *
 * Extension is considered to be the part of filename after the last
 * period '.'. This part is replaced with @a newext. If file has no
 * extension, @a newext is added after a period.
 *
 * @param fname File name
 * @param newext New extension
 * @param rnewname Place to store pointer to newly constructed name
 * @return EOK on success, ENOMEM if out of memory
 */
static int ext_replace(const char *fname, const char *newext,
    char **rnewname)
{
	char *period;
	char *basename = NULL;
	char *newname = NULL;
	size_t nchars;
	int rv;

	period = strrchr(fname, '.');

	/* Compute number of characters to copy (this excludes the '.') */
	if (period != NULL)
		nchars = (size_t)(period - fname);
	else
		nchars = strlen(fname);

	/* Copy just the base name */
	basename = malloc(nchars + 1);
	if (basename == NULL)
		goto error;

	strncpy(basename, fname, nchars);
	basename[nchars] = '\0';

	rv = asprintf(&newname, "%s.%s", basename, newext);
	if (rv < 0) {
		newname = NULL;
		goto error;
	}

	free(basename);
	*rnewname = newname;
	return EOK;
error:
	if (basename != NULL)
		free(basename);
	return ENOMEM;
}

/** Remove filename externsion.
 *
 * @param fname File name
 * @param rnewname Place to store pointer to newly constructed name
 * @return EOK on success, ENOMEM if out of memory
 */
static int ext_remove(const char *fname, char **rnewname)
{
	char *period;
	char *basename = NULL;
	size_t nchars;

	period = strrchr(fname, '.');

	/* Compute number of characters to copy (this excludes the '.') */
	if (period != NULL)
		nchars = (size_t)(period - fname);
	else
		nchars = strlen(fname);

	/* Copy just the base name */
	basename = malloc(nchars + 1);
	if (basename == NULL)
		goto error;

	memcpy(basename, fname, nchars);
	basename[nchars] = '\0';

	*rnewname = basename;
	return EOK;
error:
	return ENOMEM;
}

/** Construct output file name from input file name.
 *
 * @param infname Input file name
 * @param flags Compiler flags
 * @param routfname Place to store pointer to output file name.
 * @return EOK on success or an error code
 */
static int construct_outfname(const char *infname, comp_flags_t flags,
    char **routfname)
{
	int rc;

	if ((flags & compf_no_emit) != compf_none) {
		rc = ext_replace(infname, "asm", routfname);
		if (rc != EOK)
			goto error;
	} else if ((flags & compf_no_link) != compf_none) {
		rc = ext_replace(infname, "obj", routfname);
		if (rc != EOK)
			goto error;
	} else if ((flags & compf_no_tape) != compf_none) {
		rc = ext_replace(infname, "bin", routfname);
		if (rc != EOK)
			goto error;
	} else {
		rc = ext_replace(infname, "tzx", routfname);
		if (rc != EOK)
			goto error;
	}

	return EOK;
error:
	return rc;
}

/** Compile one input file.
 *
 * @param comp Compiler
 * @param fname Input file name
 * @param flags Compiler flags
 * @param cfglags Code generator flags
 *
 * @return EOK on succcess or an error code
 */
static int compile_file(comp_t *comp, const char *fname, comp_flags_t flags,
    cgen_flags_t cgflags)
{
	int rc;
	int rv;
	comp_module_t *module = NULL;
	comp_mtype_t mtype;
	file_input_t finput;
	FILE *f = NULL;
	FILE *outf = NULL;
	FILE *mapf = NULL;
	char *outfname = NULL;
	char *mapfname = NULL;
	char *progname = NULL;
	char *ext;

	ext = strrchr(fname, '.');
	if (ext == NULL) {
		(void)fprintf(stderr, "File '%s' has no extension.\n", fname);
		rc = EINVAL;
		goto error;
	}

	if (strcmp(ext, ".c") == 0 || strcmp(ext, ".C") == 0) {
		mtype = cmt_csrc;
	} else if (strcmp(ext, ".h") == 0 || strcmp(ext, ".H") == 0) {
		mtype = cmt_chdr;
	} else if (strcmp(ext, ".ir") == 0 || strcmp(ext, ".IR") == 0) {
		mtype = cmt_ir;
	} else if (strcmp(ext, ".obj") == 0 || strcmp(ext, ".OBJ") == 0) {
		mtype = cmt_obj;
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

	rc = construct_outfname(fname, flags, &outfname);
	if (rc != EOK)
		goto error;

	if ((flags & compf_no_link) != compf_none ||
	    (flags & compf_no_emit) != compf_none) {
		outf = fopen(outfname, "wb");
		if (outf == NULL) {
			(void)fprintf(stderr, "Cannot open '%s'.\n", outfname);
			rc = EIO;
			goto error;
		}
	}

	file_input_init(&finput, f, fname);

	if (mtype == cmt_obj) {
		rc = comp_module_create_from_obj(comp, fname, &module);
		if (rc != EOK)
			goto error;
	} else {
		rc = comp_module_create(comp, &lexer_file_input, &finput, mtype,
		    fname, &module);
		if (rc != EOK)
			goto error;
	}

	comp->cgflags = cgflags;

	if ((flags & compf_dump_ast) != compf_none) {
		rc = comp_module_dump_ast(module, stdout);
		if (rc != EOK)
			goto error;

		rv = printf("\n");
		if (rv < 0) {
			rc = EIO;
			goto error;
		}
	}

	if ((flags & compf_dump_toks) != compf_none) {
		rc = comp_module_dump_toks(module, stdout);
		if (rc != EOK)
			goto error;

		rv = printf("\n");
		if (rv < 0) {
			rc = EIO;
			goto error;
		}
	}

	if ((flags & compf_dump_ir) != compf_none) {
		rc = comp_module_dump_ir(module, stdout);
		if (rc != EOK)
			goto error;
	}

	if ((flags & compf_dump_vric) != compf_none) {
		rc = comp_module_dump_vric(module, stdout);
		if (rc != EOK)
			goto error;
	}

	if ((flags & compf_no_emit) != compf_none) {
		rc = comp_module_compile(module, outf);
		if (rc != EOK)
			goto error;
	} else {
		rc = comp_module_emit(module, outf);
		if (rc != EOK)
			goto error;
	}

	if ((flags & compf_dump_obj) != compf_none) {
		rc = comp_module_dump_obj(module, stdout);
		if (rc != EOK)
			goto error;
	}

	if (fflush(outf) < 0) {
		(void)fprintf(stderr, "Error writing to '%s'.\n", outfname);
		rc = EIO;
		goto error;
	}

	(void)fclose(f);
	if (outf != NULL)
		(void)fclose(outf);
	free(outfname);
	if (mapfname != NULL)
		free(mapfname);
	if (progname != NULL)
		free(progname);

	return EOK;
error:
	comp_module_destroy(module);
	if (f != NULL)
		(void)fclose(f);
	if (outf != NULL)
		(void)fclose(outf);
	if (mapf != NULL)
		(void)fclose(mapf);
	if (mapfname != NULL) {
		(void) remove(mapfname);
		free(mapfname);
	}
	if (outfname != NULL) {
		(void) remove(outfname);
		free(outfname);
	}
	if (progname != NULL)
		free(progname);
	return rc;
}

/** Link modules into a single binary.
 *
 * @param comp Compiler
 * @param outfn Output file name
 * @param flags Compiler flags
 * @param lflags Linker flags
 *
 * @return EOK on succcess or an error code
 */
static int link_binary(comp_t *comp, const char *outfn, comp_flags_t flags)
{
	int rc;
	FILE *f = NULL;
	FILE *outf = NULL;
	FILE *mapf = NULL;
	char *outfname = NULL;
	char *mapfname = NULL;
	char *tapefname = NULL;
	char *progname = NULL;
	comp_module_t *module;

	if ((flags & compf_no_emit) != compf_none)
		return EOK;
	if ((flags & compf_no_link) != compf_none)
		return EOK;

	if (outfn == NULL) {
		module = comp_module_first(comp);
		if (comp_module_next(module) != NULL) {
			(void)fprintf(stderr, "When linking multiple files, "
			    "you must specify output file name with "
			    "--out=<fname>.\n");
			rc = EINVAL;
			goto error;
		}

		rc = construct_outfname(module->fname, flags, &outfname);
		if (rc != EOK)
			goto error;
	} else {
		outfname = strdup(outfn);
		if (outfname == NULL) {
			rc = ENOMEM;
			goto error;
		}
	}

	if ((flags & compf_no_tape) != compf_none) {
		outf = fopen(outfname, "wb");
		if (outf == NULL) {
			(void)fprintf(stderr, "Cannot open '%s'.\n", outfname);
			rc = EIO;
			goto error;
		}
	}

	rc = comp_link(comp, outf);
	if (rc != EOK)
		goto error;

	rc = ext_replace(outfname, "map", &mapfname);
	if (rc != EOK)
		goto error;

	mapf = fopen(mapfname, "wt");
	if (mapf == NULL) {
		rc = EIO;
		goto error;
	}

	rc = comp_save_map(comp, mapf);
	if (rc != EOK)
		goto error;

	if (fflush(mapf) < 0) {
		(void)fprintf(stderr, "Error writing to '%s'.\n",
		    mapfname);
		rc = EIO;
		goto error;
	}

	(void)fclose(mapf);
	mapf = NULL;

	if ((flags & compf_no_tape) == compf_none) {
		rc = ext_remove(outfname, &progname);
		if (rc != EOK)
			goto error;

		rc = comp_make_tape(comp, progname);
		if (rc != EOK)
			goto error;

		rc = ext_replace(outfname, "tzx", &tapefname);
		if (rc != EOK)
			goto error;

		rc = comp_save_tape(comp, tapefname);
		if (rc != EOK)
			goto error;
	}

	if (outf != NULL && fflush(outf) < 0) {
		(void)fprintf(stderr, "Error writing to '%s'.\n", outfname);
		rc = EIO;
		goto error;
	}

	if (outf != NULL)
		(void)fclose(outf);

	if (tapefname != NULL)
		free(tapefname);
	if (mapfname != NULL)
		free(mapfname);
	if (progname != NULL)
		free(progname);
	if (outfname != NULL)
		free(outfname);
	return EOK;
error:
	if (f != NULL)
		(void)fclose(f);
	if (outf != NULL)
		(void)fclose(outf);
	if (mapf != NULL)
		(void)fclose(mapf);
	if (outfname != NULL) {
		(void)remove(outfname);
		free(outfname);
	}
	if (mapfname != NULL) {
		(void) remove(mapfname);
		free(mapfname);
	}
	if (tapefname != NULL) {
		(void)remove(tapefname);
		free(tapefname);
	}
	if (progname != NULL)
		free(progname);
	return rc;
}

int main(int argc, char *argv[])
{
	int rc;
	int rv;
	int i;
	comp_flags_t flags = compf_none;
	cgen_flags_t cgflags = cgf_none;
	obj_linker_flags_t lflags = lf_none;
	comp_t *comp = NULL;
	const char *outfname = NULL;

	if (argc < 2) {
		print_syntax();
		return 1;
	}

	if (argc == 2 && strcmp(argv[1], "--test") == 0) {
		/* Run tests */
		rc = test_cgen();
		rv = printf("test_cgen -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_comp();
		rv = printf("test_comp -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_cgtype();
		rv = printf("test_cgtype -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_ir();
		rv = printf("test_ir -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_ir_lexer();
		rv = printf("test_ir -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_scope();
		rv = printf("test_scope -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_z80ic();
		rv = printf("test_z80ic -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_z80_isel();
		rv = printf("test_z80_isel -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rc = test_z80_ralloc();
		rv = printf("test_z80_ralloc -> %d\n", rc);
		if (rc != EOK || rv < 0)
			return 1;

		rv = printf("Tests passed.\n");
		if (rv < 0)
			return 1;
		return 0;
	}

	i = 1;
	while (argc > i && argv[i][0] == '-') {
		if (strcmp(argv[i], "--dump-ast") == 0) {
			++i;
			flags |= compf_dump_ast;
		} else if (strcmp(argv[i], "--dump-toks") == 0) {
			++i;
			flags |= compf_dump_toks;
		} else if (strcmp(argv[i], "--dump-ir") == 0) {
			++i;
			flags |= compf_dump_ir;
		} else if (strcmp(argv[i], "--dump-vric") == 0) {
			++i;
			flags |= compf_dump_vric;
		} else if (strcmp(argv[i], "--dump-obj") == 0) {
			++i;
			flags |= compf_dump_obj;
		} else if (strcmp(argv[i], "--no-emit") == 0) {
			++i;
			flags |= compf_no_emit;
		} else if (strcmp(argv[i], "--no-link") == 0) {
			++i;
			flags |= compf_no_link;
		} else if (strcmp(argv[i], "--no-tape") == 0) {
			++i;
			flags |= compf_no_tape;
		} else if (strcmp(argv[i], "--lvalue-args") == 0) {
			++i;
			cgflags |= cgf_lvalue_args;
		} else if (strcmp(argv[i], "--int-promotion") == 0) {
			++i;
			cgflags |= cgf_int_promotion;
		} else if (strcmp(argv[i], "--fatal-warn") == 0) {
			++i;
			cgflags |= cgf_fatal_warn;
		} else if (strncmp(argv[i], "--out=", strlen("--out=")) == 0) {
			outfname = argv[i] + strlen("--out=");
			++i;
		} else if (strcmp(argv[i], "--no-link-range-error") == 0) {
			++i;
			lflags |= lf_no_range_error;
		} else if (strcmp(argv[i], "-") == 0) {
			++i;
			break;
		} else {
			(void)fprintf(stderr, "Invalid option.\n");
			return 1;
		}
	}

	if (argc <= i) {
		(void)fprintf(stderr, "Argument missing.\n");
		return 1;
	}

	rc = comp_create(&comp);
	if (rc != EOK) {
		(void)fprintf(stderr, "Failed creating compiler.\n");
		return 1;
	}

	comp->lflags = lflags;

	while (i < argc) {
		rc = compile_file(comp, argv[i++], flags, cgflags);
		if (rc != EOK) {
			comp_destroy(comp);
			return 1;
		}
	}

	rc = link_binary(comp, outfname, flags);
	if (rc != EOK) {
		comp_destroy(comp);
		return 1;
	}

	comp_destroy(comp);

	return 0;
}
