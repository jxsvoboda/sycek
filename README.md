Sycek
=====

Sycek aims to provide tools based around a modular C language parser.
Currently there's one tool `ccheck`, a C code style checker. It is used
to check compliance with the [HelenOS coding style][1] by the
[HelenOS][2] project and other, smaller projects (e.g. [Timrec][3]).

It can report and fix coding style issues such as

  * spacing issues
  * indentation issues
  * invalid characters (e.g. `\` in C code)
  * vertical spacing
  * block comment formatting
  * loop style

it will also report (potential) bugs such as:

  * misplaced `__attribute__` with no effect

Sycek is available under an MIT-style license.

Downloading
-----------
You can get the latest Sycek version from Github by typing

    $ git clone https://github.com/jxsvoboda/sycek sycek
    $ cd sycek

Compiling
---------
You need Linux or similar OS with a working compiler toolchain.
To build Sycek, simply type

    $ make

Cross-compiling for HelenOS
---------------------------

Sycek is available as a HelenOS package, via Coastline. This means you can
get the pre-compiled package or use HSCT to build the Sycek package
automatically.

In this section we describe how to cross-compile Sycek for HelenOS manually,
which is useful if you are developing Sycek.

You need a built HelenOS workspace and a working cross-compiler toolchain.
If you don't have one, you need to do something like

    $ cd ..
    $ git clone https://github.com/HelenOS/helenos.git helenos
    $ cd helenos
    $ sudo tools/toolchain.sh amd64
    $ make PROFILE=amd64

You may need to have some development packages installed. For details,
see http://www.helenos.org/wiki/UsersGuide/CompilingFromSource

Next you need to setup XCW tools which we use for the cross-compilation:

    $ PATH=$PATH:$PWD/tools/xcw/bin

Now go to your Sycek workspace and off we go:

    $ cd ../sycek
    $ make test-hos

This will build the HelenOS binaries, install then to the HelenOS workspace
and start emulation. Once in HelenOS start Ccheck by typing

    # ccheck <arguments...>

If you want to only build the binaries without installing, type

    $ make hos

If you want to only build and install the binaries without starting emulation,
type

    $ make install-hos

Now you need to go to root of your HelenOS workspace and type `make` to
re-build the OS image.

Running
-------
Ccheck runs as a pure parser in the sense that it does not actually preprocess
or compile the code. This means it does not recurse into the included header
files, so there is no need for you to provide path to includes, defines or
anything like that.

To check a single file and report issues, simply type:

    $ ./ccheck <path-to-file>

Many (but not all) issues can be fixed automatically. To check a file,
attempt to fix issues and report remaining issues, type:

    $ ./ccheck --fix <path-to-file>

The original file will be saved as `<path-to-file>.orig`

Ccheck returns an exit code of zero if it was able to parse the file
successfully (regardles whether it found style issues), non-zero
if it encountered a fatal error (e.g. was not able to properly parse the file)

An example output message from ccheck:

    <test.h:43:28-30:int>: Expected whitespace after ','.

This means when checking the file test.h, ccheck found a token `int` on line
43, columns 28-30. The `int` token was following a comma, but there was
no space between the comma and the `int`.  This a formatting issue that
ccheck can fix automatically.

If an output line starts with Error:

    Error: <./file_input.c:34:36:=> unexpected, expected '{' or ';'.

This means that ccheck failed to parse the file. On line 34, column 36
it expected a `{` or `;`, but found an `=` token. This means either
that ccheck cannot parse this source file yet, or the file has incorrect
syntax.

You can opt to disable particular groups of checks. This can be useful,
for example, if you have a code base that does not use HelenOS formatting
style, but you would still like to use ccheck to look for other issues.
To disable a group of checks, use `-d <check>`. Available groups are

  * `fmt` Formatting
  * `hdr` Header style
  * `invchar` Invalid characters
  * `loop` Loop style
  * `sclass` Storage class issues

You can also use ccheck-run.sh to check or fix all .c/.h files under
a certain directory.

Accepted syntax
---------------
ccheck has a good understanding of the C language (C89, C99, C11, but *not*
K & R). However for some particular syntax that is valid C, but has bad style,
ccheck will currently fail with an Error instead of just reporting or fixing
the style issue.

Bad style that currently triggers a parse error:

  * Gratuitous `;` (e.g. empty declaration or null statement)
  * Gratuitous nested block
  * Any use of null statement (`;`) except as the body of a while loop
    or in the header of a for loop

Apart from standard C, ccheck also understands some compiler-specific
extensions, such as

  * GCC (extended) inline assembler
  * GCC `register ... asm(...)` variable register assignment
  * GCC attribute syntax
  * GCC's `__int128`, `__restrict__`

ccheck also understands the C++ `extern "C"` declaration embedded in
a header file.

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
 * Symbolic variable or macro call used as attribute at the end of a global
   declaration header

Using ccheck for your project
-----------------------------
It's easy to use ccheck if you are starting from scratch. Applying it
to an existing codebase other than HelenOS is likely to require some,
possibly non-trivial, changes to that code base before it would be fully
parsable by ccheck.

[1]: http://www.helenos.org/wiki/CStyle
[2]: http://www.helenos.org/
[3]: https://github.com/jxsvoboda/timrec
