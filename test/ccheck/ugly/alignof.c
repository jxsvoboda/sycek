/* Argument to '_Alignof' should be parenthesized. */
int i1 = _Alignof 0;

/* Argument to 'alignof' should be parenthesized. */
int i2 = alignof 0;

/*
 * Argument to '_Alignof' should be parenthesized.
 * Space expected before expression.
 */
int i3 = _Alignof+0;

/*
 * Argument to 'alignof' should be parenthesized.
 * Space expected before expression.
 */
int i4 = alignof+0;

/* Unexpected whitespace after '_Alignof'. */
int i5 = _Alignof (0);

/* Unexpected whitespace after 'alignof'. */
int i6 = alignof (0);

/* Unexpected whitespace after '('. */
/* Unexpected whitespace before ')'. */
int i7 = _Alignof( 0 );

/* Unexpected whitespace after '('. */
/* Unexpected whitespace before ')'. */
int i8 = alignof( 0 );

/* Unexpected whitespace after '_Alignof'. */
int i9 = _Alignof (int);

/* Unexpected whitespace after 'alignof'. */
int i10 = alignof (int);

/* Unexpected whitespace after '('. */
/* Unexpected whitespace before ')'. */
int i11 = _Alignof( int );

/* Unexpected whitespace after '('. */
/* Unexpected whitespace before ')'. */
int i12 = alignof( int );
