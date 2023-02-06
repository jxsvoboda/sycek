/*
 * Test C11-specific functionality
 */

/* _Atomic type qualifier */
_Atomic volatile int *p;

/* _Atomic() type specifier */
_Atomic(void *) q;

/* _Alignas() with constant expression argument */
_Alignas(2) uint8_t r[4];

/* _Alignas() with type name argument */
_Alignas(unsigned long) int s;
