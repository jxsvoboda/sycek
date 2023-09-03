/*
 * Excess braces around scalar initializer
 *
 * The C standard (C89) does not say anything about there being
 * more braces than needed (and the grammar says there can be any number
 * of braces). So we assume that excess braces (even multiple levels)
 * are allowed. We warn about them, though.
 */

int a = { 1 };
int b = { { 1 } };
