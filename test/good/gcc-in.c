/*
 * Test supported GCC extensions
 */

/* Alternate form of restrict keyword */
void foo(char *__restrict__ c, int *__restrict__ i);

/*
 * Attribute syntax
 */
__attribute__((noreturn)) void bar(int);
void bar(int) __attribute__((noreturn));
void myprintf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
void foobar(__attribute__((unused)) int x);

typedef struct {
} __attribute__((packed)) foo_t;

int main(void)
{
	/* Basic assembler */
	asm (
	    "nop\n"
	);

	/* Extended assembler with output operands */
	asm (
	    "xor %0, %0\n"
	    : "=r" (dst)
	);

	/* Extended assembler with output and input operands */
	asm (
	    "mov %1, %0\n"
	    : "=r" (dst)
	    : "r" (src)
	);

	/* Extended assembler with output, input and clobber operands */
	asm (
	    "add %1, %0\n"
	    : "=r" (dst)
	    : "r" (src)
	    : "cc"
	);

	/*
	 * Extended assembler with output, input, clobber operands and goto
	 * labels
	 */
	asm goto (
	    "add %1, %0\n"
	    "jc %l[error]\n"
	    : "=r" (dst)
	    : "r" (src)
	    : "cc"
	    : error
	);

	/* Variable register assignment */
	register unsigned int x asm("o0");

	/* 128-bit integer */
	__int128 i128;
}
