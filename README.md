Sycek
=====

Sycek aims to provide tools based around a modular C language parser.
Currently there's one tool 'ccheck', a C code style checker. It aims
to support the [HelenOS coding style][1] for use with [HelenOS][2]
and other projects. It can report and fix coding style issues.
Sycek is available under an MIT-style license.


Compiling
---------
You need Linux or similar OS with a working compiler toolchain.
Simply type:

    $ make


Running
-------
Ccheck runs as a pure parser in the sense it does not actually preprocess
or compile the code. This means it does not recurse into the included files,
thus there is no need for you to provide path to includes, defines or
anything like that.

To check a single file and report issues, simply type:

    $ ./ccheck <path-to-file>

to check a file, attempt to fix issues and report remaining issues, type:

    $ ./ccheck --fix <path-to-file>

The original file will be saved as <path-to-file>.orig

Ccheck returns an exit code of zero if it was able to parse the file
successfully (regardles whether it found style issues), non-zero
if it encountered a fatal error (e.g. was not able to properly parse the file)

An example output message from ccheck:

    <test.h:43:28-30:int>: Expected whitespace after ','.

This means when checking the file test.h ccheck found a token `int` on line
43, columns 28-30. The `int` token was following a comma, but there was
no space between the comma and the `int`.  This a formatting issue that
ccheck can fix automatically.

If an output line starts with Error:

    Error: <./file_input.c:34:36:=> unexpected, expected '{' or ';'.

This means that ccheck failed to parse the file. On line 34, column 36
it expected a `{` or `;`, but found a `<=` token. This means either
that ccheck cannot parse this source file yet, or the file has incorrect
syntax.

Accepted syntax
---------------
ccheck has a good understanding of the C language (C89, C99, C11, but *not*
K & R). However for some particular syntax that is valid C but has bad style
ccheck will fail with an Error instead of just reporting or fixing the style
issue.

Examples of bad style that triggers a parse error include:

  * Gratuitous `;` (e.g. empty declaration or null statement)
  * Empty statement as for loop iteration statement (ie. `for(a;b;)`)
  * Any `\` character outside of a preprocessor block
  * Gratuitous nested block
  * Any use of null statement (`;`) except as the body of a while loop
    or in the header of a for loop

ccheck also understands some compiler-specific extensions, such as

  * GCC inline assembler
  * GCC `register ... asm(...)` variable register assignment
  * GCC attribute syntax
  * GCC's `__int128`, `__restrict__`

Finally, ccheck supports some extensions to the C syntax that are
exploited via the preprocessor. I.e., it understands certain specific
uses of macros where the macros alter the language grammar. These are
uses outside of e.g. function-like or accessor-like macros.

Examples of supported macro-based C syntax extensions include:

 * Symbolic variables or macros that expand to a string literal
 * Macros that take a type name (instead of an expression) as an argument
 * Loop macros
 * Macros that declare a global object
 * Macros that declare a struct or union member
 * Macros that stand as the header of a function definition
 * Symbolic variables used as a type specifier/qualifier

Using ccheck for your project
-----------------------------
It's easy to use ccheck if you are starting from scratch. Applying it
to an existing codebase other than HelenOS is likely to require some,
possibly non-trivial, changes to that code base before it would be fully
parsable by ccheck.

[1]: http://www.helenos.org/wiki/CStyle
[2]: http://www.helenos.org/
