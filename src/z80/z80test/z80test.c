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
 * Z80 test
 *
 * Run a single function on an emulated Z80 CPU.
 *
 * Loads a binary file, starts executing from a specified address until
 * control returns to the calling function. Print final statistics
 * (such as consumed T-states) and register contents.
 *
 * This is useful for automated testing of machine code (e.g. code
 * generated by a compiler) as well as benchmarking code performance.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_input.h"
#include "scrlexer.h"
#include "symbols.h"
#include "types/z80/z80test/regmem.h"
#include "../ext/z80.h"
#include "z80test.h"

enum {
	mem_size = 0x10000,
	max_cycles = 1000000
};

uint8_t *mem;
uint32_t instr_cnt;

uint32_t ifetch_cnt;
uint32_t dread_cnt;
uint32_t dwrite_cnt;
uint32_t pin_cnt;
uint32_t pout_cnt;
bool quiet = false;

symbols_t *symbols = NULL;

static int binary_load(const char *fname, uint16_t org, bool quiet)
{
	FILE *f;
	size_t nr;

	f = fopen(fname, "rb");
	if (f == NULL) {
		fprintf(stderr, "Error opening '%s'.\n", fname);
		return -1;
	}

	nr = fread(mem + org, 1, mem_size - org, f);
	if (nr == 0) {
		fprintf(stderr, "Error reading '%s'.\n", fname);
		return -1;
	}

	if (!quiet)
		printf("Read %zu bytes of code at 0x%x.\n", nr, org);

	fclose(f);
	return 0;
}

static int mapfile_load(const char *fname, bool quiet)
{
	int rc;

	rc = symbols_mapfile_load(symbols, fname);
	if (rc != 0) {
		fprintf(stderr, "Error loading '%s'.\n", fname);
		return EIO;
	}

	if (!quiet)
		printf("Loaded map file '%s'.\n", fname);

	return 0;
}

static void cpu_setup(void)
{
	uoc = 0;
	smc = 0;
	z80_clock = 0;
	z80_init_tables();
	z80_reset();
}

static int do_call(uint16_t addr)
{
	cpus.PC = addr;
	cpus.SP = 0xfff0;

	while (cpus.SP <= 0xfff0 && z80_clock < max_cycles) {
		z80_execinstr();
		++instr_cnt;
	}

	if (z80_clock >= max_cycles) {
		printf("Error: CPU cycle limit exceeded.\n");
		return EDOM;
	}

	return 0;
}

static void syntax_error(void)
{
	fprintf(stderr, "Syntax error.\n");
	fprintf(stderr, "Usage: z80test [<options>]\n");
	fprintf(stderr, "\t-s <script>  Script to execute\n");
	fprintf(stderr, "\t-q           Quiet mode\n");
	exit(1);
}

/** Return @c true if token type is to be ignored when parsing.
 *
 * @param ttype Token type
 * @return @c true iff token of type @a ttype is ignored during parsing
 */
static bool script_ttype_ignore(scr_lexer_toktype_t ttype)
{
	return ttype == stt_space || ttype == stt_tab ||
	    ttype == stt_newline || ttype == stt_comment ||
	    ttype == stt_invchar;
}

static void script_read_tok(script_t *script, scr_lexer_tok_t *rtok)
{
	*rtok = script->tok;
}

static void script_next_tok(script_t *script)
{
	int rc;

	rc = scr_lexer_get_tok(script->lexer, &script->tok);
	assert(rc == 0);
}

/** Return valid input token skipping tokens that should be ignored.
 *
 * At the same time we read the token contents into the provided buffer @a rtok
 *
 * @param parser IR parser
 * @param rtok Place to store next lexer token
 */
static void script_next_input_tok(script_t *script, scr_lexer_tok_t *rtok)
{
	script_read_tok(script, rtok);
	while (script_ttype_ignore(rtok->ttype)) {
		script_next_tok(script);
		script_read_tok(script, rtok);
	}
}

/** Return type of next token.
 *
 * @param script Script
 * @return Type of next token being parsed
 */
static scr_lexer_toktype_t script_next_ttype(script_t *script)
{
	scr_lexer_tok_t ltok;

	script_next_input_tok(script, &ltok);
	return ltok.ttype;
}

/** Read next token.
 *
 * @param script Script
 * @param tok Place to store token
 */
static void script_read_next_tok(script_t *script, scr_lexer_tok_t *tok)
{
	script_next_input_tok(script, tok);
}

static int script_dprint_next_tok(script_t *script, FILE *f)
{
	scr_lexer_tok_t tok;

	script_read_next_tok(script, &tok);
	return scr_lexer_dprint_tok(&tok, f);
}

/** Skip over current token.
 *
 * @param script Script
 */
static void script_skip(script_t *script)
{
	scr_lexer_tok_t tok;

	/* Find non-ignored token */
	script_next_input_tok(script, &tok);
	scr_lexer_free_tok(&tok);

	/* Skip over */
	script_next_tok(script);
}

/** Match a particular token type.
 *
 * If the type of the next token is @a mtype, skip over it. Otherwise
 * generate an error.
 *
 * @param script Script
 * @param mtype Expected token type
 *
 * @return Zero on success, EINVAL if token does not have expected type
 */
static int script_match(script_t *script, scr_lexer_toktype_t mtype)
{
	scr_lexer_toktype_t stt;

	stt = script_next_ttype(script);
	if (stt != mtype) {
		fprintf(stderr, "Error: ");
		script_dprint_next_tok(script, stderr);
		fprintf(stderr, " unexpected, expected %s.\n",
		    scr_lexer_str_ttype(mtype));
		return EINVAL;
	}

	script_skip(script);
	return 0;
}

static int script_eval_expr(script_t *script, uint64_t *eval)
{
	scr_lexer_tok_t tok;
	symbol_t *symbol;
	int64_t sval = 0;
	int64_t oval;
	bool have_ident = false;
	int rc;

	script_read_next_tok(script, &tok);

	if (tok.ttype == stt_ident) {
		symbol = symbols_lookup(symbols, tok.text);
		if (symbol == NULL) {
			fprintf(stderr, "Error: ");
			script_dprint_next_tok(script, stderr);
			fprintf(stderr, " is not a known symbol.\n");
			return ENOENT;
		}

		sval = symbol->addr;
		script_skip(script);
		have_ident = true;
	}

	if (have_ident) {
		*eval = sval;
		return 0;
	}

	rc = scr_lexer_number_val(&tok, &oval);
	if (rc != 0) {
		fprintf(stderr, "Error: ");
		script_dprint_next_tok(script, stderr);
		fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	script_skip(script);
	*eval = sval + oval;
	return 0;
}

/** Parse register/memory operand.
 *
 * Parse a register/memory operand from the script.
 *
 * @param script Script
 * @param regmem Place to store the register/memory operand
 */
static int script_parse_rm(script_t *script, regmem_t *regmem)
{
	scr_lexer_tok_t tok;
	scr_lexer_toktype_t ttype;
	uint64_t eval;
	int rc;

	script_read_next_tok(script, &tok);

	switch (tok.ttype) {
	case stt_byte:
	case stt_word:
	case stt_dword:
	case stt_qword:
		ttype = tok.ttype;
		script_skip(script);

		rc = script_match(script, stt_ptr);
		if (rc != 0)
			return EINVAL;

		rc = script_match(script, stt_lparen);
		if (rc != 0)
			return EINVAL;

		rc = script_eval_expr(script, &eval);
		if (rc != 0)
			return rc;

		regmem->addr = (uint16_t)eval;

		rc = script_match(script, stt_rparen);
		if (rc != 0)
			return EINVAL;

		switch (ttype) {
		case stt_byte:
			regmem->rmtype = rm_byte_ptr;
			break;
		case stt_word:
			regmem->rmtype = rm_word_ptr;
			break;
		case stt_dword:
			regmem->rmtype = rm_dword_ptr;
			break;
		case stt_qword:
			regmem->rmtype = rm_qword_ptr;
			break;
		default:
			break;
		}
		break;
	case stt_AF:
		script_skip(script);
		regmem->rmtype = rm_AF;
		break;
	case stt_BC:
		script_skip(script);
		regmem->rmtype = rm_BC;
		break;
	case stt_DE:
		script_skip(script);
		regmem->rmtype = rm_DE;
		break;
	case stt_HL:
		script_skip(script);
		regmem->rmtype = rm_HL;
		break;
	default:
		fprintf(stderr, "Error: ");
		script_dprint_next_tok(script, stderr);
		fprintf(stderr, " is not a valid register/memory operand.\n");
		return EINVAL;
	}

	return 0;
}

/** Read register/memory.
 *
 * Read the value of a register/memory operand.
 * Optionally, also print it.
 *
 * @param regmem Register/memory operand
 * @param print @c true to print the register/memory
 * @param val Place to store the value
 */
static int regmem_read(regmem_t *regmem, bool print, uint64_t *val)
{
	uint16_t addr;

	addr = regmem->addr;

	switch (regmem->rmtype) {
	case rm_byte_ptr:
		*val = mem[addr];
		if (print) {
			printf("byte ptr (0x%x) == 0x%x\n", addr,
			    (unsigned)*val);
		}
		break;
	case rm_word_ptr:
		*val = mem[addr] + (((uint16_t)mem[addr + 1]) << 8);
		if (print) {
			printf("word ptr (0x%x) == 0x%x\n", addr,
			    (unsigned)*val);
		}
		break;
	case rm_dword_ptr:
		*val = mem[addr] + (((uint32_t)mem[addr + 1]) << 8) +
		    (((uint32_t)mem[addr + 2]) << 16) +
		    (((uint32_t)mem[addr + 3]) << 24);
		if (print) {
			printf("dword ptr (0x%x) == 0x%x\n", addr,
			    (unsigned)*val);
		}
		break;
	case rm_qword_ptr:
		*val = mem[addr] + (((uint64_t)mem[addr + 1]) << 8) +
		    (((uint64_t)mem[addr + 2]) << 16) +
		    (((uint64_t)mem[addr + 3]) << 24) +
		    (((uint64_t)mem[addr + 4]) << 32) +
		    (((uint64_t)mem[addr + 5]) << 40) +
		    (((uint64_t)mem[addr + 6]) << 48) +
		    (((uint64_t)mem[addr + 7]) << 56);
		if (print) {
			printf("qword ptr (0x%x) == 0x%lx\n", addr,
			    (unsigned long)*val);
		}
		break;
	case rm_AF:
		*val = z80_getAF();
		if (print) {
			printf("AF == 0x%x\n", (unsigned)*val);
		}
		break;
	case rm_BC:
		*val = z80_getBC();
		if (print) {
			printf("BC == 0x%x\n", (unsigned)*val);
		}
		break;
	case rm_DE:
		*val = z80_getDE();
		if (print) {
			printf("DE == 0x%x\n", (unsigned)*val);
		}
		break;
	case rm_HL:
		*val = z80_getHL();
		if (print) {
			printf("HL == 0x%x\n", (unsigned)*val);
		}
		break;
	default:
		assert(false);
		return EINVAL;
	}

	return 0;
}

/** Write register/memory.
 *
 * Set value of register/memory operand.
 *
 * @param regmem Register/memory operand
 * @param val Value
 */
static int regmem_write(regmem_t *regmem, uint64_t val)
{
	uint16_t addr;

	addr = regmem->addr;

	switch (regmem->rmtype) {
	case rm_byte_ptr:
		mem[addr] = val;
		break;
	case rm_word_ptr:
		mem[addr] = val & 0xff;
		mem[addr + 1] = val >> 8;
		break;
	case rm_dword_ptr:
		mem[addr] = val & 0xff;
		mem[addr + 1] = val >> 8;
		mem[addr + 2] = val >> 16;
		mem[addr + 3] = val >> 24;
		break;
	case rm_qword_ptr:
		mem[addr] = val & 0xff;
		mem[addr + 1] = val >> 8;
		mem[addr + 2] = val >> 16;
		mem[addr + 3] = val >> 24;
		mem[addr + 4] = val >> 32;
		mem[addr + 5] = val >> 40;
		mem[addr + 6] = val >> 48;
		mem[addr + 7] = val >> 56;
		break;
	case rm_AF:
		cpus.r[rA] = val >> 8;
		cpus.F = val & 0xff;
		break;
	case rm_BC:
		cpus.r[rB] = val >> 8;
		cpus.r[rC] = val & 0xff;
		break;
	case rm_DE:
		cpus.r[rD] = val >> 8;
		cpus.r[rE] = val & 0xff;
		break;
	case rm_HL:
		cpus.r[rH] = val >> 8;
		cpus.r[rL] = val & 0xff;
		break;
	default:
		assert(false);
		return EINVAL;
	}

	return 0;
}

static int script_do_call(script_t *script)
{
	uint64_t eval;
	uint16_t addr;
	int rc;

	script_skip(script);

	rc = script_eval_expr(script, &eval);
	if (rc != 0)
		return EINVAL;

	addr = (uint16_t)eval;

	if (!quiet)
		printf("Call 0x%x\n", addr);
	return do_call(addr);
}

static int script_do_ld(script_t *script)
{
	regmem_t rm;
	uint64_t eval;
	int rc;

	script_skip(script);

	rc = script_parse_rm(script, &rm);
	if (rc != 0)
		return EINVAL;

	rc = script_match(script, stt_comma);
	if (rc != 0)
		return EINVAL;

	rc = script_eval_expr(script, &eval);
	if (rc != 0)
		return EINVAL;

	rc = regmem_write(&rm, eval);
	if (rc != 0)
		return EINVAL;

	return 0;
}

static int script_do_ldbin(script_t *script)
{
	scr_lexer_tok_t tok;
	char *fname;
	int64_t nval;
	uint16_t addr;
	int rc;

	script_skip(script);

	script_read_next_tok(script, &tok);
	rc = scr_lexer_string_text(&tok, &fname);
	if (rc != 0) {
		fprintf(stderr, "Error: ");
		script_dprint_next_tok(script, stderr);
		fprintf(stderr, " is not a valid string literal.\n");
		return rc;
	}

	script_skip(script);

	rc = script_match(script, stt_comma);
	if (rc != 0)
		return rc;

	script_read_next_tok(script, &tok);
	rc = scr_lexer_number_val(&tok, &nval);
	if (rc != 0) {
		fprintf(stderr, "Error: ");
		script_dprint_next_tok(script, stderr);
		fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	addr = (uint16_t)nval;

	script_skip(script);

	rc = binary_load(fname, addr, quiet);
	if (rc < 0)
		return 1;

	free(fname);
	return 0;
}

static int script_do_mapfile(script_t *script)
{
	scr_lexer_tok_t tok;
	char *fname;
	int rc;

	script_skip(script);

	script_read_next_tok(script, &tok);
	rc = scr_lexer_string_text(&tok, &fname);
	if (rc != 0) {
		fprintf(stderr, "Error: ");
		script_dprint_next_tok(script, stderr);
		fprintf(stderr, " is not a valid number.\n");
		return rc;
	}

	script_skip(script);

	rc = mapfile_load(fname, quiet);
	if (rc < 0)
		return 1;

	free(fname);
	return 0;
}

static int script_do_print(script_t *script)
{
	uint64_t val;
	regmem_t rm;
	int rc;

	script_skip(script);

	rc = script_parse_rm(script, &rm);
	if (rc != 0)
		return rc;

	rc = regmem_read(&rm, true, &val);
	(void) val;

	return rc;
}

static int script_do_verify(script_t *script)
{
	regmem_t rm;
	uint64_t rmval;
	uint64_t eval;
	int rc;

	script_skip(script);

	rc = script_parse_rm(script, &rm);
	if (rc != 0)
		return rc;

	rc = regmem_read(&rm, false, &rmval);

	rc = script_match(script, stt_comma);
	if (rc != 0)
		return EINVAL;

	rc = script_eval_expr(script, &eval);
	if (rc != 0)
		return EINVAL;

	if (rmval != eval) {
		printf("Verification failed! (0x%lx != 0x%lx)\n",
		    rmval, eval);
		return EINVAL;
	}

	return rc;
}

static int script_process_cmd(script_t *script)
{
	scr_lexer_toktype_t stt;
	int rc = 0;

	stt = script_next_ttype(script);
	switch (stt) {
	case stt_call:
		rc = script_do_call(script);
		break;
	case stt_ld:
		rc = script_do_ld(script);
		break;
	case stt_ldbin:
		rc = script_do_ldbin(script);
		break;
	case stt_mapfile:
		rc = script_do_mapfile(script);
		break;
	case stt_print:
		rc = script_do_print(script);
		break;
	case stt_verify:
		rc = script_do_verify(script);
		break;
	default:
		fprintf(stderr, "Error: ");
		script_dprint_next_tok(script, stderr);
		fprintf(stderr, " unexpected, expected command.\n");
		return EINVAL;
	}

	if (rc != 0)
		return rc;

	rc = script_match(script, stt_scolon);
	return rc;
}

static int script_process(const char *fname)
{
	scr_lexer_t *lexer = NULL;
	file_input_t finput;
	script_t script;
	scr_lexer_toktype_t stt;
	FILE *f;
	int rc;

	f = fopen(fname, "rt");
	if (f == NULL) {
		fprintf(stderr, "Cannot open '%s'.\n", fname);
		rc = ENOENT;
		goto error;
	}

	file_input_init(&finput, f, fname);

	rc = scr_lexer_create(&lexer_file_input, &finput, &lexer);
	if (rc != 0)
		goto error;

	script.lexer = lexer;
	script_next_tok(&script);

	stt = script_next_ttype(&script);
	while (stt != stt_eof) {
		rc = script_process_cmd(&script);
		if (rc != 0)
			goto error;

		stt = script_next_ttype(&script);
	}

	scr_lexer_destroy(lexer);
	return 0;
error:
	if (lexer != NULL)
		scr_lexer_destroy(lexer);
	return rc;
}

int main(int argc, char *argv[])
{
	int rc;
	const char *scr_fname = NULL;

	--argc;
	++argv;
	while (argc > 0) {
		if (strcmp(*argv, "-s") == 0) {
			++argv;
			--argc;
			if (argc <= 0)
				syntax_error();
			scr_fname = *argv;
		} else if (strcmp(*argv, "-q") == 0) {
			quiet = true;
		} else {
			syntax_error();
		}

		++argv;
		--argc;
	}

	mem = calloc(mem_size, 1);
	if (mem == NULL) {
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}

	rc = symbols_create(&symbols);
	if (rc != 0) {
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}

	if (scr_fname == NULL) {
		fprintf(stderr, "Script file name not specified.\n");
		return 1;
	}

	if (!quiet)
		printf("Initialize CPU.\n");

	cpu_setup();
	cpus.PC = 0;
	cpus.SP = 0xfff0;
	mem[0xfff0] = 0xff;
	mem[0xfff1] = 0xff;
	mem[0xfff2] = 0xff;
	mem[0xfff3] = 0xff;
	mem[0xfff4] = 0xff;
	mem[0xfff5] = 0xff;
	cpus.r[rB] = 0xff;
	cpus.r[rC] = 0xff;
	cpus.r[rD] = 0xff;
	cpus.r[rE] = 0xff;

	instr_cnt = 0;

	rc = script_process(scr_fname);
	if (rc != 0)
		return 1;

	if (!quiet) {
		printf("T states: %lu\n", z80_clock);
		printf("Instruction cycles: %u\n", instr_cnt);
		printf("Instruction bytes read: %u\n", ifetch_cnt);
		printf("Data bytes read: %u\n", dread_cnt);
		printf("Data bytes written: %u\n", dwrite_cnt);
		printf("Port reads: %u\n", pin_cnt);
		printf("Port writes: %u\n", pout_cnt);
	}

	symbols_destroy(symbols);

	return 0;
}
