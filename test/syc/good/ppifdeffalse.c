/*
 * #ifdef condition that evaluates to false skips over (extra) characters
 * following a directive and does not include text lines into ouput source
 * code.
 */

#ifdef FOO
This line is skipped.

/* Extra characters are ignored when skipping. */
#ifdef A B
#endif X
Also skipped.
#ifndef A
#endif
Also skipped 2.

#undef A B
Still skipped.
#endif
