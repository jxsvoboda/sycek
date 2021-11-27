/*
 * Test compund literals
 */

/* Trivial compound literal */
foo_t foo = (foo_t) { };

/* Compound literal with dot accessors */
foo_t foo = (struct foo) { .a = 1, .b = 2 };
