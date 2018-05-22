
/** A record type */
typedef struct {
	/* Comment inside rectord type specifier. Also, a simple member */
	unsigned x;
	/** Member with bit width specified */
	unsigned y : 2;
	/** Member with bit width specified using an expresson */
	unsigned z : 1 + 1;
	/** Anonymous bit padding */
	unsigned : 2;
} foo_t;
