/*
 * Packed attribute in this position is ignored. For the outermost
 * structure GCC emits a warning, but not for the inner structure.
 * We should always emit an error.
 */
typedef struct {
	struct {
		int x;
	} bar __attribute__((packed));
} foo_t;
