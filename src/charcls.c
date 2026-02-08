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
 * Character classification (C language)
 */

#include <assert.h>
#include <charcls.h>
#include <stdbool.h>
#include <stdint.h>

/** Determine if character is a letter (C language)
 *
 * @param c Character
 *
 * @return @c true if c is a letter (C language), @c false otherwise
 */
bool is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/** Determine if character is a number (C language)
 *
 * @param c Character
 *
 * @return @c true if @a c is a number (C language), @c false otherwise
 */
bool is_num(char c)
{
	return (c >= '0' && c <= '9');
}

/** Determine if character is alphanumeric (C language)
 *
 * @param c Character
 *
 * @return @c true if @a c is alphanumeric (C language), @c false otherwise
 */
bool is_alnum(char c)
{
	return is_alpha(c) || is_num(c);
}

/** Determine if character is an octal digit
 *
 * @param c Character
 *
 * @return @c true if @a c is a octal digit, @c false otherwise
 */
bool is_octdigit(char c)
{
	return c >= '0' && c <= '7';
}

/** Determine if character is a hexadecimal digit
 *
 * @param c Character
 *
 * @return @c true if @a c is a hexadecimal digit, @c false otherwise
 */
bool is_hexdigit(char c)
{
	return is_num(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/** Determine if character is a digit in the specified base
 *
 * @param c Character
 * @param b Base (8, 10 or 16)
 *
 * @return @c true if @a c is a hexadecimal digit, @c false otherwise
 */
bool is_digit(char c, int base)
{
	switch (base) {
	case 8:
		return is_octdigit(c);
	case 10:
		return is_num(c);
	case 16:
		return is_hexdigit(c);
	default:
		assert(false);
		return false;
	}
}

/** Determine if character can begin a C identifier
 *
 * @param c Character
 *
 * @return @c true if @a c can begin a C identifier, @c false otherwise
 */
bool is_idbegin(char c)
{
	return is_alpha(c) || (c == '_');
}

/** Determine if character can continue a C identifier
 *
 * @param c Character
 *
 * @return @c true if @a c can continue a C identifier, @c false otherwise
 */
bool is_idcnt(char c)
{
	return is_alnum(c) || (c == '_');
}

/** Determine if character is printable.
 *
 * A character that is part of a multibyte sequence is not printable.
 * @note This function assumes that the input is ASCII/UTF-8
 * @param c Character
 * @return @c true iff the character is printable
 */
bool is_print(char c)
{
	uint8_t b;

	b = (uint8_t) c;
	return (b >= 32) && (b < 127);
}

/** Determine if character is a forbidden control character.
 *
 * This can only determine basic ASCII control characters.
 * Only allowed control characters are Tab, Line Feed (a.k.a. newline).
 *
 * @return @c true iff the character is a forbidden control character
 */
bool is_bad_ctrl(char c)
{
	uint8_t b;

	b = (uint8_t) c;

	if (b < 32 && b != '\t' && b != '\n')
		return true;
	if (b == 127)
		return true;

	return false;
}

/** Get the value of a hexadecimal digit.
 *
 * @param c Hexadecimal digit
 * @return Digit value
 */
unsigned cc_hexdigit_val(char c)
{
	/* Note: this will not work in non-ASCII environments */

	if (is_num(c)) {
		return (uint8_t)c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return 10 + (uint8_t)(c - 'a');
	} else {
		assert(c >= 'A');
		assert(c <= 'F');
		return 10 + (uint8_t)(c - 'A');
	}
}

/** Get the value of a decimal digit.
 *
 * @param c Hexadecimal digit
 * @return Digit value
 */
unsigned cc_decdigit_val(char c)
{
	assert(is_num(c));

	/* Note: this will not work in non-ASCII environments */
	return (uint8_t)c - '0';
}

/** Get the value of an octal digit.
 *
 * @param c Hexadecimal digit
 * @return Digit value
 */
unsigned cc_octdigit_val(char c)
{
	assert(is_octdigit(c));

	/* Note: this will not work in non-ASCII environments */
	return (uint8_t)c - '0';
}
