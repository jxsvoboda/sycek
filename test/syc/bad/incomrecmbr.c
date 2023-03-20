/*
 * Record member has incomplete type
 */

struct foo;

struct bar {
	struct foo f;
} b;
