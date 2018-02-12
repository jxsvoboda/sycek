#!/bin/bash

#
# Copyright 2017 Jiri Svoboda
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

ccheck=./ccheck
#ccheck="./ccheck --fix"

srepcnt=0
snorepcnt=0
fcnt=0

find . -name '*.[ch]' | (
while read fname; do
	$ccheck $fname >/tmp/ccheck.out 2>&1
	rc=$?
	if [ .$rc == .0 ]; then
		if [ -s /tmp/ccheck.out ] ; then
			srepcnt=$((srepcnt + 1))
			cat /tmp/ccheck.out
		else
#			echo "Succeeded $fname"
			snorepcnt=$((snorepcnt + 1))
		fi
	else
		fcnt=$((fcnt + 1))
		cat /tmp/ccheck.out
	fi
done

echo "Checked files with issues: $srepcnt"
echo "Checked files without issues: $snorepcnt"
echo "Not checked files: $fcnt"
)