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
CFLAGS = -std=c99 -D_GNU_SOURCE -O0 -ggdb -Wall -Wextra -Wmissing-prototypes \
         -Werror -I src
LIBS   =

bkqual = $$(date '+%Y-%m-%d')

sources = \
    src/adt/list.c \
    src/ast.c \
    src/checker.c \
    src/file_input.c \
    src/lexer.c \
    src/main.c \
    src/parser.c \
    src/src_pos.c \
    src/str_input.c \
    src/test/ast.c \
    src/test/checker.c \
    src/test/lexer.c \
    src/test/parser.c

binary = ccheck
ccheck = ./$(binary)

objects = $(sources:.c=.o)
headers = $(wildcard *.h */*.h */*/*.h)

test_good_ins = $(wildcard test/good/*-in.c)
test_good_out_diffs = $(test_good_ins:-in.c=-out.txt.diff)
test_bad_ins = $(wildcard test/bad/*-in.c)
test_bad_errs = $(test_bad_ins:-in.c=-err-t.txt)
test_bad_err_diffs = $(test_bad_ins:-in.c=-err.txt.diff)
test_ugly_ins = $(wildcard test/ugly/*-in.c)
test_ugly_fixed_diffs = $(test_ugly_ins:-in.c=-fixed.c.diff)
test_ugly_out_diffs = $(test_ugly_ins:-in.c=-out.txt.diff)
test_outs = $(test_good_fixed_diffs) $(test_good_out_diffs) \
    $(test_bad_err_diffs) $(test_bad_errs) $(test_ugly_fixed_diffs) \
    $(test_ugly_err_diffs) $(test_ugly_out_diffs) test/all.diff \
    test/test-int.out

all: $(binary)

$(binary): $(objects)
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

$(objects): $(headers)

cstyle: $(binary)
	./selfcheck.sh

clean:
	rm -f $(objects) $(binary) $(test_outs)

test/good/%-out-t.txt: test/good/%-in.c $(ccheck)
	./ccheck $< >$@

test/good/%-out.txt.diff: /dev/null test/good/%-out-t.txt
	diff -u $^ >$@

test/bad/%-err-t.txt: test/bad/%-in.c $(ccheck)
	-./ccheck $< 2>$@

test/bad/%-err.txt.diff: test/bad/%-err.txt test/bad/%-err-t.txt
	diff -u $^ >$@

test/ugly/%-fixed-t.c: test/ugly/%-in.c $(ccheck)
	cp $< $@
	$(ccheck) --fix $@
	rm -f $@.orig

test/ugly/%-fixed.c.diff: test/ugly/%-fixed.c test/ugly/%-fixed-t.c
	diff -u $^ >$@

test/ugly/%-out-t.txt: test/ugly/%-in.c $(ccheck)
	./ccheck $< >$@

test/ugly/%-out.txt.diff: test/ugly/%-out.txt test/ugly/%-out-t.txt
	diff -u $^ >$@


test/all.diff: $(test_good_out_diffs) $(test_bad_err_diffs) \
    $(test_ugly_fixed_diffs) $(test_ugly_out_diffs)
	cat $^ > $@

# Run internal unit tests
test/test-int.out: $(ccheck)
	$(ccheck) --test >test/test-int.out

#
# Note that if any of the diffs is not empty, that diff command will
# return non-zero exit code, failing the make
#
test: test/test-int.out test/all.diff

backup: clean
	cd .. && tar czf sycek-$(bkqual).tar.gz trunk
	cd .. && rm -f sycek-latest.tar.gz && ln -s sycek-$(bkqual).tar.gz \
	    sycek-latest.tar.gz
