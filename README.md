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
To check a single file and report issues, type:

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

NOTE
----
ccheck is still in progress and cannot successfully parse all or even most
valid C source code (not to mention code using preprocessor magic). It can
typically parse header files (unless they contain inline function definitions
or preprocessor magic).

[1]: http://www.helenos.org/wiki/CStyle
[2]: http://www.helenos.org/
