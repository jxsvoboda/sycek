#
# Copyright 2018 Jiri Svoboda
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
         -Werror -I src
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
    src/comp.c \
    src/ir.c \
    src/scope.c \
    src/syc.c \
    src/test/cgen.c \
    src/test/comp.c \
    src/test/ir.c \
    src/test/scope.c \
    src/test/z80/isel.c \
    src/test/z80/ralloc.c \
    src/test/z80/z80ic.c \
    src/z80/isel.c \
    src/z80/ralloc.c \
    src/z80/z80ic.c

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

binary_ccheck = ccheck
binary_ccheck_hos = ccheck-hos
ccheck = ./$(binary_ccheck)

binary_syc = syc
binary_syc_hos = syc-hos
syc = ./$(binary_syc)

objects_ccheck = $(sources_ccheck:.c=.o)
objects_ccheck_hos = $(sources_ccheck_hos:.c=.hos.o)

objects_syc = $(sources_syc:.c=.o)
objects_syc_hos = $(sources_syc_hos:.c=.hos.o)

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
    $(text_ugly_h_out_diffs) $(test_vg_outs) \
    test/ccheck/all.diff test/test-int.out test/test-syc-int.out test/selfcheck.out
example_outs = example/test.asm example/test.o example/test.bin \
    example/test.map example/test.tap example/test.ir example/test.vric

all: $(binary_ccheck) $(binary_syc)

$(binary_ccheck): $(objects_ccheck)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(binary_syc): $(objects_syc)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(objects_ccheck): $(headers)
$(objects_syc): $(headers)

hos: $(binary_ccheck_hos) $(binary_syc_hos)

$(binary_ccheck_hos): $(objects_ccheck_hos)
	$(LD_hos) $(CFLAGS_hos) -o $@ $^ $(LIBS_hos)

$(binary_syc_hos): $(objects_syc_hos)
	$(LD_hos) $(CFLAGS_hos) -o $@ $^ $(LIBS_hos)

$(objects_ccheck_hos): $(headers)
$(objects_syc_hos): $(headers)

%.hos.o: %.c
	$(CC_hos) -c $(CFLAGS_hos) -o $@ $<

install-hos: hos
	mkdir -p $(PREFIX_hos)/app
	$(INSTALL) -T $(binary_ccheck_hos) $(PREFIX_hos)/app/ccheck

uninstall-hos:
	rm -f $(PREFIX_hos)/app/ccheck

test-hos: install-hos
	helenos-test

clean:
	rm -f $(objects_ccheck) $(objects_ccheck_hos) $(objects_syc) \
	$(objects_syc_hos) $(binary_ccheck) $(binary_ccheck_hos) \
	$(binary_syc) $(binary_syc_hos) $(test_outs) $(example_outs)

test/ccheck/good/%-out-t.txt: test/ccheck/good/%-in.c $(ccheck)
	$(ccheck) $< >$@

test/ccheck/good/%-out.txt.diff: /dev/null test/ccheck/good/%-out-t.txt
	diff -u $^ >$@

test/ccheck/good/%-vg.txt: test/ccheck/good/%-in.c $(ccheck)
	valgrind $(ccheck) $^ 2>$@
	grep -q 'no leaks are possible' $@

test/ccheck/bad/%-err-t.txt: test/ccheck/bad/%-in.c $(ccheck)
	-$(ccheck) $< 2>$@

test/ccheck/bad/%-err.txt.diff: test/ccheck/bad/%-err.txt test/ccheck/bad/%-err-t.txt
	diff -u $^ >$@

test/ccheck/ugly/%-fixed-t.c: test/ccheck/ugly/%-in.c $(ccheck)
	cp $< $@
	$(ccheck) --fix $@
	rm -f $@.orig

test/ccheck/ugly/%-fixed-t.h: test/ccheck/ugly/%-in.h $(ccheck)
	cp $< $@
	$(ccheck) --fix $@
	rm -f $@.orig

test/ccheck/ugly/%-fixed.c.diff: test/ccheck/ugly/%-fixed.c test/ccheck/ugly/%-fixed-t.c
	diff -u $^ >$@

test/ccheck/ugly/%-fixed.h.diff: test/ccheck/ugly/%-fixed.h test/ccheck/ugly/%-fixed-t.h
	diff -u $^ >$@

test/ccheck/ugly/%-out-t.txt: test/ccheck/ugly/%-in.c $(ccheck)
	$(ccheck) $< >$@

test/ccheck/ugly/%-out-t.txt: test/ccheck/ugly/%-in.h $(ccheck)
	$(ccheck) $< >$@

test/ccheck/ugly/%-out.txt.diff: test/ccheck/ugly/%-out.txt test/ccheck/ugly/%-out-t.txt
	diff -u $^ >$@

test/ccheck/ugly/%-vg.txt: test/ccheck/ugly/%-in.c $(ccheck)
	valgrind $(ccheck) $< >/dev/null 2>$@
	grep -q 'no leaks are possible' $@

test/ccheck/ugly/%-vg.txt: test/ccheck/ugly/%-in.h $(ccheck)
	valgrind $(ccheck) $< >/dev/null 2>$@
	grep -q 'no leaks are possible' $@

test/ccheck/all.diff: $(test_good_out_diffs) $(test_bad_err_diffs) \
    $(test_ugly_fixed_diffs) $(test_ugly_h_fixed_diffs) \
    $(test_ugly_out_diffs) $(test_ugly_h_out_diffs) \
    $(test_vg_out_diffs)
	cat $^ > $@

# Run ccheck internal unit tests
test/test-int.out: $(ccheck)
	$(ccheck) --test >test/test-int.out

# Run syc internal unit tests
test/test-syc-int.out: $(syc)
	$(syc) --test >test/test-syc-int.out

selfcheck: test/selfcheck.out

test/selfcheck.out: $(ccheck)
	PATH=$$PATH:$$PWD ./ccheck-run.sh src > $@
	grep "^Ccheck passed." $@

#
# Compiler example
#
example/test.asm: example/test.c $(syc)
	$(syc) $<
example/test.ir: example/test.c $(syc)
	$(syc) --dump-ir $< >$@
example/test.vric: example/test.c $(syc)
	$(syc) --dump-vric $< >$@
example/test.bin: example/test.asm
	z80asm +zx --origin=32768 -b -m $<
example/test.tap: example/test.bin
	appmake +zx --org=32768 -b $<
examples: example/test.tap example/test.ir example/test.vric

#
# Note that if any of the diffs is not empty, that diff command will
# return non-zero exit code, failing the make
#
test: test/test-int.out test/test-syc-int.out test/ccheck/all.diff \
    $(test_vg_outs) test/selfcheck.out

backup: clean
	cd .. && tar czf sycek-$(bkqual).tar.gz trunk
	cd .. && rm -f sycek-latest.tar.gz && ln -s sycek-$(bkqual).tar.gz \
	    sycek-latest.tar.gz
