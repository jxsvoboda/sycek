/*
 * Copyright 2022 Jiri Svoboda
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
 * Character classification (C language)
 */

#ifndef CHARCLS_H
#define CHARCLS_H

#include <stdbool.h>

extern bool is_alpha(char);
extern bool is_num(char);
extern bool is_alnum(char);
extern bool is_octdigit(char);
extern bool is_hexdigit(char);
extern bool is_digit(char, int);
extern bool is_idbegin(char);
extern bool is_idcnt(char);
extern bool is_print(char);
extern bool is_bad_ctrl(char);
extern unsigned cc_hexdigit_val(char);
extern unsigned cc_decdigit_val(char);
extern unsigned cc_octdigit_val(char);

#endif
