/*
 * Enum that has a tag, typedef or instance is strict.
 */

/* Enum with a tag */
enum etag {
	e1, e2
};

/* Enum with an instance */
enum {
	f1, f2
} f;

/* Non-strict enum */
enum {
	i1
};

/* Enum with a typedef */
typedef enum {
	g1, g2
} g_t;

void foo(void)
{
	int i;

	/* We should get a conversion warning in each case */
	i = e1;
	i = f1;
	i = g1;

	/* Suspicious arithmetic operation, as if i1 was an integer */
	i = i1 - e1;
}
