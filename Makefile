#
# Copyright 2023 Jiri Svoboda
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
    src/cgtype.c \
    src/comp.c \
    src/ir.c \
    src/irlexer.c \
    src/irparser.c \
    src/labels.c \
    src/scope.c \
    src/syc.c \
    src/symbols.c \
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
    src/hcompat/adt/list.c

sources_ccheck = \
    $(sources_ccheck_common) \
    $(sources_hcompat)

sources_ccheck_hos = \
    $(sources_ccheck_common)

sources_syc = \
    $(sources_syc_common) \
    $(sources_hcompat)

sources_syc_hos = \
    $(sources_syc_common)

sources_z80test = \
    $(sources_z80test_common) \
    $(sources_hcompat)

binary_ccheck = ccheck
binary_ccheck_hos = ccheck-hos
ccheck = ./$(binary_ccheck)

binary_syc = syc
binary_syc_hos = syc-hos
syc = ./$(binary_syc)

binary_z80test = z80test
binary_z80test_hos = z80test-hos
z80test = ./$(binary_z80test)

objects_ccheck = $(sources_ccheck:.c=.o)
objects_ccheck_hos = $(sources_ccheck_hos:.c=.hos.o)

objects_syc = $(sources_syc:.c=.o)
objects_syc_hos = $(sources_syc_hos:.c=.hos.o)

objects_z80test = $(sources_z80test:.c=.o)
objects_z80test_hos = $(sources_z80test_hos:.c=.hos.o)

headers = $(wildcard src/*.h src/*/*.h src/*/*/*.h)

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
test_syc_good_asms = $(test_syc_good_srcs:.c=.asm)
test_syc_good_z80ts = $(test_syc_good_scripts:.scr=-z80t.txt)
test_syc_good_bins = $(test_syc_good_srcs:.c=.bin)
test_syc_good_maps = $(test_syc_good_srcs:.c=.map)
test_syc_good_taps = $(test_syc_good_srcs:.c=.tap)
test_syc_bad_srcs = $(wildcard test/syc/bad/*.c)
test_syc_bad_diffs = $(test_syc_bad_srcs:.c=.txt.diff)
test_syc_ugly_srcs = $(wildcard test/syc/ugly/*.c)
test_syc_ugly_asms = $(test_syc_ugly_srcs:.c=.asm)
test_syc_ugly_diffs = $(test_syc_ugly_srcs:.c=.txt.diff)
test_syc_vg_outs = \
    $(test_syc_good_srcs:.c=-vg.txt) \
    $(test_syc_ugly_srcs:.c=-vg.txt)
test_syc_outs = $(test_syc_good_asms) $(test_syc_bad_diffs) \
    $(test_syc_ugly_asms) $(test_syc_ugly_diffs) $(test_syc_vg_outs) \
    test/syc/all.diff
test_syc_z80_outs = $(test_syc_good_z80ts) $(test_syc_good_bins) \
    $(test_syc_good_maps) $(test_syc_good_taps)

example_srcs = \
	example/fillscr.c \
	example/mul16.c \
	example/test.c
example_asms = $(example_srcs:.c=.asm)
example_bins = $(example_asms:.asm=.bin)
example_os = $(example_asms:.asm=.o)
example_maps = $(example_asms:.asm=.map)
example_taps = $(example_asms:.asm=.tap)
example_irs = $(example_srcs:.c=.ir)
example_vrics = $(example_srcs:.c=.vric)
example_irirs = $(example_srcs:.c=.ir.ir)
example_irasms = $(example_irirs:.ir.ir=.ir.asm)
example_outs = example/lib.o $(example_asms) $(example_os) $(example_bins) \
    $(example_maps) $(example_taps) $(example_irs) $(example_vrics) \
    $(example_irirs) $(example_irasms)

all: $(binary_ccheck) $(binary_syc) $(binary_z80test)

$(binary_ccheck): $(objects_ccheck)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(binary_syc): $(objects_syc)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(binary_z80test): $(objects_z80test)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(objects_ccheck): $(headers)
$(objects_syc): $(headers)

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

clean:
	rm -f $(objects_ccheck) $(objects_ccheck_hos) $(objects_syc) \
	$(objects_syc_hos) $(objects_z80test) $(objects_z80test_host) \
	$(binary_ccheck) $(binary_ccheck_hos) \
	$(binary_syc) $(binary_syc_hos) \
	$(binary_z80test) $(binary_z80test_hos) \
	$(test_outs) $(test_syc_outs) $(test_syc_z80_outs) \
	$(example_outs)

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
	-$(syc) $< 2>$@

test/syc/bad/%.txt.diff: test/syc/bad/%.txt test/syc/bad/%-t.txt
	diff -u $^ >$@ || (rm $@ ; false)

test/syc/good/%-vg.txt: test/syc/good/%.c $(syc)
	valgrind $(syc) $< 2>$@ || (rm $@ ; false)
	grep -q 'no leaks are possible' $@ || (rm $@ ; false)

test/syc/ugly/%-t.txt: test/syc/ugly/%.c $(syc)
	$(syc) $< 2>$@

test/syc/ugly/%.txt.diff: test/syc/ugly/%.txt test/syc/ugly/%-t.txt
	diff -u $^ >$@ || (rm $@ ; false)

test/syc/ugly/%-vg.txt: test/syc/ugly/%.c $(syc)
	valgrind $(syc) $< 2>$@ || (rm $@ ; false)
	grep -q 'no leaks are possible' $@ || (rm $@ ; false)

test/syc/good/%.asm: test/syc/good/%.c $(syc)
	$(syc) $<

test/syc/good/%.bin: test/syc/good/%.asm
	z80asm +zx -m --origin=0x8000 $<

test/syc/good/%-z80t.txt: test/syc/good/%.scr test/syc/good/%.bin $(z80test)
	cd test/syc/good && ../../../$(z80test) -s ../../../$< >../../../$@ || (rm ../../../$@ ; false)

test/syc/all.diff: $(test_syc_bad_diffs) $(test_syc_ugly_diffs)
	cat $^ > $@

# Run ccheck internal unit tests
test/test-int.out: $(ccheck)
	$(ccheck) --test >test/test-int.out

# Run syc internal unit tests
test/test-syc-int.out: $(syc)
	$(syc) --test >test/test-syc-int.out

selfcheck: test/selfcheck.out

test/selfcheck.out: $(ccheck)
	PATH=$$PATH:$$PWD ./ccheck-run.sh src > $@ || (cat $@ ; rm $@ ; false)

#
# Compiler example
#
example/%.asm: example/%.c $(syc)
	$(syc) $<
example/%.ir: example/%.c $(syc)
	$(syc) --dump-ir $< >$@
example/%.ir.ir: example/%.ir
	cp $< $@
example/%.ir.asm: example/%.ir.ir $(syc)
	$(syc) $<
example/%.vric: example/%.c $(syc)
	$(syc) --dump-vric $< >$@
example/%.bin: example/%.asm example/lib.asm
	z80asm +zx --origin=32768 -b -m $^
example/%.tap: example/%.bin
	appmake +zx --org=32768 -b $<

examples: $(example_asms) $(example_taps) $(example_irs) $(example_vrics) \
    $(example_irasms)

#
# Note that if any of the diffs is not empty, that diff command will
# return non-zero exit code, failing the make
#
test: test/test-int.out test/test-syc-int.out test/ccheck/all.diff \
    test/syc/all.diff $(test_vg_outs) $(test_syc_vg_outs) \
    test/selfcheck.out
test_z80: $(test_syc_good_asms) $(test_syc_good_z80ts)

backup: clean
	cd .. && tar czf sycek-$(bkqual).tar.gz trunk
	cd .. && rm -f sycek-latest.tar.gz && ln -s sycek-$(bkqual).tar.gz \
	    sycek-latest.tar.gz
