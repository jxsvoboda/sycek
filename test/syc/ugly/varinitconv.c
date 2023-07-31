/*
 * When initializing a global variable, the value of the expression
 * should generate warnings from implicit conversion to the type
 * of the variable being initialized.
 *
 * Here, the expression is long and does not fit into 'int', so it
 * should generate 'Warning: Number changed in conversion.'
 */

int a = 256l * 256;
