/*
 * A type qualifier is specified more than once, directly or via
 * typedef. This behaves as if it were specified only once.
 * But since it probably is a mistake, we generate a warning.
 */

/*
 * Duplicate type qualifier in declaration specifier list.
 */

const const int i;
volatile volatile int j;

typedef const int cint_t;
typedef char *restrict rptr_t;
typedef volatile int vint_t;

const cint_t ccint;
restrict rptr_t p;
volatile vint_t vvi;

/*
 * Duplicate type qualifier in pointer declarator's type qualifier list.
 */

int *const const pcc;
int *restrict restrict rrp;
int *volatile volatile pvv;
