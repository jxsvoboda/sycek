#
# Copyright 2026 Jiri Svoboda
#
# Permission is hereby granted, free of charge, to any person obtaining
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#

CC     = gcc
CFLAGS_common = -std=c99 -D_GNU_SOURCE -O0 -ggdb -Wall -Wextra -Wmissing-prototypes \
         -Werror -Wpedantic -I src
CFLAGS = $(CFLAGS_common) -I src/hcompat
LIBS   =

CC_hos = helenos-cc
CFLAGS_hos = $(CFLAGS_common)
LD_hos = helenos-ld
LIBS_hos = $(LIBS)
PREFIX_hos = `helenos-bld-config --install-dir`
INSTALL = install

CPP_z80 = gcc
CPPFLAGS_z80 = -nostdinc -E -I lib/clib/include -I src -I src/hcompat
LIBS_z80 = lib/clib/src/stubs.z80.pp.obj

bkqual = $$(date '+%Y-%m-%d')

sources_common = \
    src/ast.c \
    src/file_input.c \
    src/str_input.c \
    src/charcls.c \
    src/lexer.c \
    src/parser.c \
    src/src_pos.c

sources_ccheck_common = \
    $(sources_common) \
    src/checker.c \
    src/ccheck.c \
    src/test/ast.c \
    src/test/checker.c \
    src/test/lexer.c \
    src/test/parser.c

sources_syc_common = \
    $(sources_common) \
    src/cgen.c \
    src/cgenum.c \
    src/cgrec.c \
    src/cgtype.c \
    src/comp.c \
    src/ir.c \
    src/irlexer.c \
    src/irparser.c \
    src/labels.c \
    src/object/linker.c \
    src/object/object.c \
    src/object/reloc.c \
    src/object/section.c \
    src/object/symbol.c \
    src/scope.c \
    src/syc.c \
    src/symbols.c \
    src/tape/basic_linebuf.c \
    src/tape/maker.c \
    src/tape/tape.c \
    src/tape/tzx.c \
    src/test/cgen.c \
    src/test/cgtype.c \
    src/test/comp.c \
    src/test/ir.c \
    src/test/irlexer.c \
    src/test/scope.c \
    src/test/z80/isel.c \
    src/test/z80/ralloc.c \
    src/test/z80/z80ic.c \
    src/z80/argloc.c \
    src/z80/emit.c \
    src/z80/iclexer.c \
    src/z80/icparser.c \
    src/z80/isel.c \
    src/z80/ralloc.c \
    src/z80/varmap.c \
    src/z80/z80ic.c

sources_z80test_common = \
    ext/z80.c \
    src/file_input.c \
    src/src_pos.c \
    src/z80/z80test/scrlexer.c \
    src/z80/z80test/symbols.c \
    src/z80/z80test/z80dep.c \
    src/z80/z80test/z80test.c

sources_hcompat = \
    src/hcompat/adt/list.c \
    src/hcompat/byteorder.c

sources_ccheck = \
    $(sources_ccheck_common) \
    $(sources_hcompat)

sources_ccheck_hos = \
    $(sources_ccheck_common)

sources_ccheck_z80 = \
    $(sources_ccheck_common) \
    $(sources_hcompat)

sources_syc = \
    $(sources_syc_common) \
    $(sources_hcompat)

sources_syc_hos = \
    $(sources_syc_common)

sources_syc_z80 = \
    $(sources_syc_common) \
    $(sources_hcompat)

sources_z80test = \
    $(sources_z80test_common) \
    $(sources_hcompat)

sources_z80test_z80 = \
    $(sources_z80test_common) \
    $(sources_hcompat)

binary_ccheck = ccheck
binary_ccheck_hos = ccheck-hos
binary_ccheck_z80 = ccheck-z80.bin
mapfile_ccheck_z80 = ccheck-z80.map
ccheck = ./$(binary_ccheck)

binary_syc = syc
binary_syc_hos = syc-hos
binary_syc_z80 = syc-z80.bin
mapfile_syc_z80 = syc-z80.map
syc = ./$(binary_syc)
sycflags = --lvalue-args --int-promotion --no-tape

binary_z80test = z80test
binary_z80test_hos = z80test-hos
binary_z80test_z80 = z80test-z80.bin
mapfile_z80test_z80 = z80test-z80.map
z80test = ./$(binary_z80test)

objects_ccheck_hos = $(sources_ccheck_hos:.c=.hos.o)
objects_ccheck = $(sources_ccheck:.c=.o)
objects_ccheck_z80 = $(sources_ccheck_z80:.c=.z80.pp.obj)

objects_syc = $(sources_syc:.c=.o)
objects_syc_hos = $(sources_syc_hos:.c=.hos.o)
objects_syc_z80 = $(sources_syc_z80:.c=.z80.pp.obj)

objects_z80test = $(sources_z80test:.c=.o)
objects_z80test_hos = $(sources_z80test_hos:.c=.hos.o)
objects_z80test_z80 = $(sources_z80test_z80:.c=.z80.pp.obj)

headers = $(wildcard src/*.h src/*/*.h src/*/*/*.h src/*/*/*/*.h)
lib_headers = $(wildcard lib/clib/include/*.h)

test_good_ins = $(wildcard test/ccheck/good/*-in.c)
test_good_out_diffs = $(test_good_ins:-in.c=-out.txt.diff)
test_bad_ins = $(wildcard test/ccheck/bad/*-in.c)
test_bad_errs = $(test_bad_ins:-in.c=-err-t.txt)
test_bad_err_diffs = $(test_bad_ins:-in.c=-err.txt.diff)
test_ugly_ins = $(wildcard test/ccheck/ugly/*-in.c)
test_ugly_fixed_diffs = $(test_ugly_ins:-in.c=-fixed.c.diff)
test_ugly_out_diffs = $(test_ugly_ins:-in.c=-out.txt.diff)
test_ugly_h_ins = $(wildcard test/ccheck/ugly/*-in.h)
test_ugly_h_fixed_diffs = $(test_ugly_h_ins:-in.h=-fixed.h.diff)
test_ugly_h_out_diffs = $(test_ugly_h_ins:-in.h=-out.txt.diff)
test_vg_outs = \
    $(test_good_ins:-in.c=-vg.txt) \
    $(test_ugly_ins:-in.c=-vg.txt) \
    $(test_ugly_h_ins:-in.h=-vg.txt)
test_outs = $(test_good_fixed_diffs) $(test_good_out_diffs) \
    $(test_bad_err_diffs) $(test_bad_errs) $(test_ugly_fixed_diffs) \
    $(test_ugly_h_fixed_diffs) $(test_ugly_err_diffs) $(test_ugly_out_diffs) \
    $(test_ugly_h_out_diffs) $(test_vg_outs) \
    test/ccheck/all.diff test/test-int.out test/test-syc-int.out test/selfcheck.out
test_syc_good_srcs = $(wildcard test/syc/good/*.c)
test_syc_good_scripts = $(wildcard test/syc/good/*.scr)
test_syc_good_objs = $(test_syc_good_srcs:.c=.obj)
test_syc_good_z80ts = $(test_syc_good_scripts:.scr=-z80t.txt)
test_syc_good_bins = $(test_syc_good_srcs:.c=.bin)
test_syc_good_maps = $(test_syc_good_srcs:.c=.map)
test_syc_good_taps = $(test_syc_good_srcs:.c=.tap)
test_syc_bad_srcs = $(wildcard test/syc/bad/*.c)
test_syc_bad_diffs = $(test_syc_bad_srcs:.c=.txt.diff)
test_syc_ugly_srcs = $(wildcard test/syc/ugly/*.c)
test_syc_ugly_objs = $(test_syc_ugly_srcs:.c=.obj)
test_syc_ugly_diffs = $(test_syc_ugly_srcs:.c=.txt.diff)
test_syc_vg_outs = \
    $(test_syc_good_srcs:.c=-vg.txt) \
    $(test_syc_ugly_srcs:.c=-vg.txt)
test_syc_outs = $(test_syc_good_objs) $(test_syc_bad_diffs) \
    $(test_syc_ugly_objs) $(test_syc_ugly_diffs) $(test_syc_vg_outs) \
    test/syc/all.diff
test_syc_z80_outs = $(test_syc_good_z80ts) $(test_syc_good_objs) \
    $(test_syc_good_maps) $(test_syc_good_taps)
test_asm_good_srcs = $(wildcard test/asm/good/*.asm)
test_asm_good_maps = $(test_asm_good_srcs:.asm=.map)
test_asm_good_tzxs = $(test_asm_good_srcs:.asm=.tzx)
test_asm_outs = $(test_asm_good_maps) $(test_asm_good_tzxs)
test_linker_good_z80ts = \
    test/linker/good/local/test-z80t.txt
test_linker_good_outs = \
    test/linker/good/local/a.obj \
    test/linker/good/local/b.obj \
    test/linker/good/local/test.bin \
    test/linker/good/local/test.map

example_srcs = \
	example/fillscr.c \
	example/mul16.c \
	example/test.c
example_bins = $(example_srcs:.c=.bin)
example_objs = $(example_srcs:.c=.obj)
example_maps = $(example_srcs:.c=.map)
example_tzxs = $(example_srcs:.c=.tzx)
example_irs = $(example_srcs:.c=.ir)
example_vrics = $(example_srcs:.c=.vric)
example_irirs = $(example_srcs:.c=.ir.ir)
example_irobjs = $(example_irirs:.ir.ir=.ir.obj)
example_outs = example/lib.o $(example_objs) $(example_bins) \
    $(example_maps) $(example_tzxs) $(example_irs) $(example_vrics) \
    $(example_irirs) $(example_irobjs)

all: $(binary_ccheck) $(binary_syc) $(binary_z80test)

$(binary_ccheck): $(objects_ccheck)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(binary_syc): $(objects_syc)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(binary_z80test): $(objects_z80test)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(objects_ccheck): $(headers)
$(objects_syc): $(headers)
$(objects_z80test): $(headers)

hos: $(binary_ccheck_hos) $(binary_syc_hos) $(binary_z80test_hos)

$(binary_ccheck_hos): $(objects_ccheck_hos)
	$(LD_hos) $(CFLAGS_hos) -o $@ $^ $(LIBS_hos)

$(binary_syc_hos): $(objects_syc_hos)
	$(LD_hos) $(CFLAGS_hos) -o $@ $^ $(LIBS_hos)

$(binary_z80test_hos): $(objects_z80test_hos)
	$(LD_hos) $(CFLAGS_hos) -o $@ $^ $(LIBS_hos)

$(objects_ccheck_hos): $(headers)
$(objects_syc_hos): $(headers)
$(objects_z80test_hos): $(headers)

%.hos.o: %.c
	$(CC_hos) -c $(CFLAGS_hos) -o $@ $<

install-hos: hos
	mkdir -p $(PREFIX_hos)/app
	$(INSTALL) -T $(binary_ccheck_hos) $(PREFIX_hos)/app/ccheck
	$(INSTALL) -T $(binary_syc_hos) $(PREFIX_hos)/app/syc
	$(INSTALL) -T $(binary_z80test_hos) $(PREFIX_hos)/app/z80test

uninstall-hos:
	rm -f $(PREFIX_hos)/app/ccheck

test-hos: install-hos
	helenos-test

z80: $(binary_ccheck_z80) $(binary_syc_z80) $(binary_z80test_z80)

objects_z80 = $(objecs_ccheck_z80) $(objects_syc_z80) $(objects_z80test_z80)
z80objs: $(objects_z80)

%.z80.pp.c: %.c
	$(CPP_z80) $(CPPFLAGS_z80) $< >$@ || rm -f $@

%.z80.pp.obj: %.z80.pp.c $(syc)
	$(syc) $(sycflags) --no-link --fatal-warn $<

$(binary_ccheck_z80): $(LIBS_z80) $(objects_ccheck_z80)
	$(syc) --no-tape --no-link-range-error --out=$@ $^

$(binary_syc_z80): $(LIBS_z80) $(objects_syc_z80)
	$(syc) --no-tape --no-link-range-error --out=$@ $^

$(binary_z80test_z80): $(LIBS_z80) $(objects_z80test_z80)
	$(syc) --no-tape --no-link-range-error --out=$@ $^

$(objects_ccheck_z80): $(headers) $(lib_headers)
$(objects_syc_z80): $(headers) $(lib_headers)
$(objects_z80test_z80): $(headers) $(lib_headers)

clean:
	rm -f $(objects_ccheck) $(objects_ccheck_hos) $(objects_ccheck_z80) \
	$(objects_syc) $(objects_syc_hos) $(objects_syc_z80) \
	$(objects_z80test) $(objects_z80test_hos) $(objects_z80test_z80) \
	$(binary_ccheck) $(binary_ccheck_hos) $(binary_ccheck_z80) \
	$(binary_syc) $(binary_syc_hos) $(binary_syc_z80) \
	$(binary_z80test) $(binary_z80test_hos) $(binary_z80test_z80) \
	$(mapfile_syc_z80) $(mapfile_ccheck_z80) $(mapfile_z80test_z80) \
	$(test_outs) $(test_syc_outs) $(test_syc_z80_outs) \
	$(test_asm_outs) $(test_linker_good_outs) \
\
	$(example_outs)

clean_z80:
	rm -f $(objects_z80)

test/ccheck/good/%-out-t.txt: test/ccheck/good/%-in.c $(ccheck)
	$(ccheck) $< >$@

test/ccheck/good/%-out.txt.diff: /dev/null test/ccheck/good/%-out-t.txt
	diff -u $^ >$@ || (rm $@ ; false)

test/ccheck/good/%-vg.txt: test/ccheck/good/%-in.c $(ccheck)
	valgrind $(ccheck) $< 2>$@ || (rm $@ ; false)
	grep -q 'no leaks are possible' $@ || (rm $@ ; false)

test/ccheck/bad/%-err-t.txt: test/ccheck/bad/%-in.c $(ccheck)
	-$(ccheck) $< 2>$@

test/ccheck/bad/%-err.txt.diff: test/ccheck/bad/%-err.txt test/ccheck/bad/%-err-t.txt
	diff -u $^ >$@ || (rm $@ ; false)

test/ccheck/ugly/%-fixed-t.c: test/ccheck/ugly/%-in.c $(ccheck)
	cp $< $@
	$(ccheck) --fix $@
	rm -f $@.orig

test/ccheck/ugly/%-fixed-t.h: test/ccheck/ugly/%-in.h $(ccheck)
	cp $< $@
	$(ccheck) --fix $@
	rm -f $@.orig

test/ccheck/ugly/%-fixed.c.diff: test/ccheck/ugly/%-fixed.c test/ccheck/ugly/%-fixed-t.c
	diff -u $^ >$@ || (rm $@ ; false)

test/ccheck/ugly/%-fixed.h.diff: test/ccheck/ugly/%-fixed.h test/ccheck/ugly/%-fixed-t.h
	diff -u $^ >$@ || (rm $@ ; false)

test/ccheck/ugly/%-out-t.txt: test/ccheck/ugly/%-in.c $(ccheck)
	$(ccheck) $< >$@

test/ccheck/ugly/%-out-t.txt: test/ccheck/ugly/%-in.h $(ccheck)
	$(ccheck) $< >$@

test/ccheck/ugly/%-out.txt.diff: test/ccheck/ugly/%-out.txt test/ccheck/ugly/%-out-t.txt
	diff -u $^ >$@ || (rm $@ ; false)

test/ccheck/ugly/%-vg.txt: test/ccheck/ugly/%-in.c $(ccheck)
	valgrind $(ccheck) $< >/dev/null 2>$@  || (rm $@ ; false)
	grep -q 'no leaks are possible' $@ || (rm $@ ; false)

test/ccheck/ugly/%-vg.txt: test/ccheck/ugly/%-in.h $(ccheck)
	valgrind $(ccheck) $< >/dev/null 2>$@ || (rm $@ ; false)
	grep -q 'no leaks are possible' $@ || (rm $@ ; false)

test/ccheck/all.diff: $(test_good_out_diffs) $(test_bad_err_diffs) \
    $(test_ugly_fixed_diffs) $(test_ugly_h_fixed_diffs) \
    $(test_ugly_out_diffs) $(test_ugly_h_out_diffs) \
    $(test_vg_out_diffs)
	cat $^ > $@

test/syc/bad/%-t.txt: test/syc/bad/%.c $(syc)
	-$(syc) $(sycflags) $< 2>$@

test/syc/bad/%.txt.diff: test/syc/bad/%.txt test/syc/bad/%-t.txt
	diff -u $^ >$@ || (rm $@ ; false)

test/syc/good/%-vg.txt: test/syc/good/%.c $(syc)
	valgrind $(syc) $(sycflags) --no-link $< 2>$@ || (rm $@ ; false)
	grep -q 'no leaks are possible' $@ || (rm $@ ; false)

test/syc/ugly/%-t.txt: test/syc/ugly/%.c $(syc)
	$(syc) $(sycflags) --no-link $< 2>$@

test/syc/ugly/%.txt.diff: test/syc/ugly/%.txt test/syc/ugly/%-t.txt
	diff -u $^ >$@ || (rm $@ ; false)

test/syc/ugly/%-vg.txt: test/syc/ugly/%.c $(syc)
	valgrind $(syc) $(sycflags) --no-link $< 2>$@ || (rm $@ ; false)
	grep -q 'no leaks are possible' $@ || (rm $@ ; false)

test/syc/good/%.obj: test/syc/good/%.c $(syc)
	$(syc) $(sycflags) --no-link $<

test/syc/good/%.bin: test/syc/good/%.c $(syc)
	$(syc) $(sycflags) $<

test/syc/good/%-z80t.txt: test/syc/good/%.scr test/syc/good/%.bin $(z80test)
	cd test/syc/good && ../../../$(z80test) -s ../../../$< >../../../$@ || (rm ../../../$@ ; false)

test/syc/all.diff: $(test_syc_bad_diffs) $(test_syc_ugly_diffs)
	cat $^ > $@

test/asm/good/%.map: test/asm/good/%.asm $(syc)
	$(syc) $<

test/asm/good/%.tzx: test/asm/good/%.asm $(syc)
	$(syc) $<

test/linker/good/local/%.obj: test/linker/good/local/%.c
	$(syc) $(sycflags) --no-link $<

test/linker/good/local/test.bin: test/linker/good/local/a.obj test/linker/good/local/b.obj
	$(syc) $(sycflags) --out=$@ $^

test/linker/good/local/test-z80t.txt: test/linker/good/local/test.scr test/linker/good/local/test.bin $(z80test)
	cd test/linker/good/local && ../../../../$(z80test) -s ../../../../$< >../../../../$@ || (rm ../../../../$@ ; false)

# Run ccheck internal unit tests
test/test-int.out: $(ccheck)
	$(ccheck) --test >test/test-int.out

# Run syc internal unit tests
test/test-syc-int.out: $(syc)
	$(syc) --test >test/test-syc-int.out

selfcheck: test/selfcheck.out

test/selfcheck.out: $(ccheck)
	PATH=$$PATH:$$PWD ./ccheck-run.sh src lib > $@ || (cat $@ ; rm $@ ; false)

#
# Compiler example
#
example/%.ir: example/%.c $(syc)
	$(syc) --no-link --dump-ir $< >$@
example/%.ir.ir: example/%.ir
	cp $< $@
example/%.ir.obj: example/%.ir.ir $(syc)
	$(syc) --no-link $<
example/%.vric: example/%.c $(syc)
	$(syc) --no-link --dump-vric $< >$@
example/%.bin: example/%.c example/lib.asm
	syc --out=$@ --no-tape $^
example/%.tzx: example/%.c example/lib.asm
	syc --out=$@ $^

examples: $(syc) $(example_tzxs) $(example_irs) $(example_vrics) \
    $(example_irobjs)

#
# Note that if any of the diffs is not empty, that diff command will
# return non-zero exit code, failing the make
#
test: test/test-int.out test/test-syc-int.out test/ccheck/all.diff \
    test/syc/all.diff $(test_vg_outs) $(test_syc_vg_outs) $(test_asm_outs) \
    test/selfcheck.out
test_z80: $(test_syc_good_objs) $(test_syc_good_z80ts) \
    $(test_linker_good_z80ts) z80objs

test_l: $(test_linker_good_z80ts)

backup: clean
	cd .. && tar czf sycek-$(bkqual).tar.gz trunk
	cd .. && rm -f sycek-latest.tar.gz && ln -s sycek-$(bkqual).tar.gz \
	    sycek-latest.tar.gz
