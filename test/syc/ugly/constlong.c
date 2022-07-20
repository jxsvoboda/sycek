/*
 * Constant should be long / long long
 */

/* Constant is marked as int (no suffix), but should be long */
int int_should_be_long = 0x10000;

/* Constant is marked as int (no suffix), but should be long long */
int int_should_be_longlong = 0x123456789;

/** Constant is marked as long (L suffix), but should be long long */
long long_should_be_longlong = 0x123456789L;
