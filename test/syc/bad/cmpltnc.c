/*
 * Pointers being compared are not constant (<).
 */

int i;
int j;

int r = (int)(&i < &j);
