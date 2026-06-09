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
 * BASIC program encoding
 *
 * Lines are encoded in order, every line consists of:
 *   - line number as 16-bit binary big-endian number (1 to 9999)
 *   - line number (number of bytes that follow) as 16-bit little-endian number
 *   - text of the line itself
 *   - carriage return (0x0D)
 *
 * Line text encoding:
 *   - Numbers are represented in text followed by 0x0E + 5 bytes floating
 *     point
 */

#ifndef TYPES_TAPE_BASIC_H
#define TYPES_TAPE_BASIC_H

/** BASIC tokens */
typedef enum {
	btt_rnd = 0xa5,
	btt_inkey_str = 0xa6,
	btt_pi = 0xa7,
	btt_fn = 0xa8,
	btt_point = 0xa9,
	btt_screen_str = 0xaa,
	btt_attr = 0xab,
	btt_at = 0xac,
	btt_tab = 0xad,
	btt_val_str = 0xae,
	btt_code = 0xaf,
	btt_val = 0xb0,
	btt_len = 0xb1,
	btt_sin = 0xb2,
	btt_cos = 0xb3,
	btt_tan = 0xb4,
	btt_asn = 0xb5,
	btt_acs = 0xb6,
	btt_atn = 0xb7,
	btt_ln = 0xb8,
	btt_exp = 0xb9,
	btt_int = 0xba,
	btt_sqr = 0xbb,
	btt_sgn = 0xbc,
	btt_abs = 0xbd,
	btt_peek = 0xbe,
	btt_in = 0xbf,
	btt_usr = 0xc0,
	btt_str_str = 0xc1,
	btt_chr_str = 0xc2,
	btt_not = 0xc3,
	btt_bin = 0xc4,
	btt_or = 0xc5,
	btt_and = 0xc6,
	btt_lteq = 0xc7,
	btt_gteq = 0xc8,
	btt_neq = 0xc9,
	btt_line = 0xca,
	btt_then = 0xcb,
	btt_to = 0xcc,
	btt_step = 0xcd,
	btt_def_fn = 0xce,
	btt_cat = 0xcf,
	btt_format = 0xd0,
	btt_move = 0xd1,
	btt_erase = 0xd2,
	btt_open_chan = 0xd3,
	btt_close_chan = 0xd4,
	btt_merge = 0xd5,
	btt_verify = 0xd6,
	btt_beep = 0xd7,
	btt_circle = 0xd8,
	btt_ink = 0xd9,
	btt_paper = 0xda,
	btt_flash = 0xdb,
	btt_bright = 0xdc,
	btt_inverse = 0xdd,
	btt_over = 0xde,
	btt_out = 0xdf,
	btt_lprint = 0xe0,
	btt_llist = 0xe1,
	btt_stop = 0xe2,
	btt_read = 0xe3,
	btt_data = 0xe4,
	btt_restore = 0xe5,
	btt_new = 0xe6,
	btt_border = 0xe7,
	btt_continue = 0xe8,
	btt_dim = 0xe9,
	btt_rem = 0xea,
	btt_for = 0xeb,
	btt_go_to = 0xec,
	btt_go_sub = 0xed,
	btt_input = 0xee,
	btt_load = 0xef,
	btt_list = 0xf0,
	btt_let = 0xf1,
	btt_pause = 0xf2,
	btt_next = 0xf3,
	btt_poke = 0xf4,
	btt_print = 0xf5,
	btt_plot = 0xf6,
	btt_run = 0xf7,
	btt_save = 0xf8,
	btt_randomize = 0xf9,
	btt_if = 0xfa,
	btt_cls = 0xfb,
	btt_draw = 0xfc,
	btt_clear = 0xfd,
	btt_return = 0xfe,
	btt_copy = 0xff
} basic_token_t;

#endif
