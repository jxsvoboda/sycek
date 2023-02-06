/*
 * Test C17-specific functionality
 */

struct foo {
	/* _Alignas() with constant expression argument inside struct */
	_Alignas ( 2 )uint8_t r[4];

	/* _Alignas() with type name argument inside struct */
	_Alignas ( unsigned long )int s;
};
