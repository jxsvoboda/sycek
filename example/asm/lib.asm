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
 * Library module written in assembly that will be linked with
 * module compiled from putpixel.c
 */

global @_putpixel;

/*
 * HL = x
 * DE = y
 */
proc @_putpixel
begin
	/* BC := x */
	ld C, L;
	ld B, H;

	/* DE := y * 32 == y << 5 */
	sla E;
	rl D;
	sla E;
	rl D;
	sla E;
	rl D;
	sla E;
	rl D;
	sla E;
	rl D;

	/* A := x/8 */
	ld A, C;
	srl A;
	srl A;
	srl A;

	/* HL := 0x4000 + x/8 */
	ld H, 0x40;
	ld L, A;

	/* HL := 0x4000 + x / 8 + y * 32 */
	add HL, DE;

	/*
	 * live registers: BC, HL
	 * swap bits 5-7 and 8-10 of HL using DE for temporary storage
	 */

	/* D(0:2) := L(7:5) */
	ld A, L;
	rlc A;
	rlc A;
	rlc A;
	and 0x07;
	ld D, A;

	/* D := H(7:3) L(7:5) */
	ld A, H;
	and 0xf8;
	or D;
	ld D, A;

	/* E(7:5) := H(2:0) */
	ld A, H;
	rrc A;
	rrc A;
	rrc A;
	and 0xe0;
	ld E, A;

	/* E := H(2:0) L(4:0) */
	ld A, L;
	and 0x1f;
	or E;
	ld E, A;

	ld H, D;
	ld L, E;

	/*
	 * BC = x
	 * HL = pixel address
	 */

	/*
	 * Now computing bit pattern 0x80 >> (x & 0x7)
	 */

	ld A, C;
	and 0x7;
	ld C, A; /* C := x & 0x7 */

	ld A, 0x80;

	/* if x & 0x7 == 0, no shifting */
	jr Z, %noshift;

	/* take number 0x80 and shift it right (x & 0x7) times */
	ld B, C;
%shiftloop:
	srl A;
	djnz %shiftloop;
%noshift:
	/* write prepared bit pattern */
	ld (HL), A;
	ret;
end;
