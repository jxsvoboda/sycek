/* Argument to 'sizeof' should be parenthesized. */
int i1 = sizeof 0;

/*
 * Argument to 'sizeof' should be parenthesized.
 * Space expected before expression.
 */
int i2 = sizeof+0;

/* Unexpected whitespace after 'sizeof'. */
int i3 = sizeof (0);

/* Unexpected whitespace after '('. */
/* Unexpected whitespace before ')'. */
int i4 = sizeof( 0 );

/* Unexpected whitespace after 'sizeof'. */
int i5 = sizeof (int);

/* Unexpected whitespace after '('. */
/* Unexpected whitespace before ')'. */
int i6 = sizeof( int );
