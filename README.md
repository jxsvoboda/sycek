Sycek
=====

Sycek aims to provide tools based around a modular C language frontend.
The available tools are

  * `ccheck` a C code style checker
  * `syc` a C compiler for ZX Spectrum, lint, static checker (WIP)
  * `z80test` a simple test harness / Z80 emulator

Sycek is available under an MIT-style license.

Ccheck
------
`ccheck` is a C code style checker. It is used to check compliance with
the [HelenOS coding style][1] by the [HelenOS][2] project and other,
smaller projects (e.g. [Timrec][3]).

It can report and fix coding style issues such as

  * spacing issues
  * indentation issues
  * invalid characters (e.g. `\` in C code)
  * vertical spacing
  * block comment formatting
  * declaration style
  * loop style

it will also report (potential) bugs such as:

  * misplaced `__attribute__` with no effect
  * non-static function defined in a header
  * non-static variable defined in a header

Sycek is available under an MIT-style license.

Syc
---
NOTE: This is a work in progress. Only a subset of C is implemented.

`syc` is meant to work as a compiler, lint-like tool and static checker.
Currently it can compile a subset of C for the Sinclair ZX Spectrum
(Zilog Z80 processor).

`syc` also complements `ccheck` checking for certain programming
iand C style issues that cannot be reliably detected withough actually
preprocessing and compiling the source code.

`syc` can thus be used as a lint / checker / shadow compiler.
See the section Syc as a checker for details.

z80test
-------
Z80test is a command-line tool for executing and testing Z80 code snippets.
For more details, see [Z80test documentation](src/z80/z80test/README.md).

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

Using ccheck
------------
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

  * `attr` Attribute issues
  * `decl` Declaration style
  * `estmt` Empty declaration or statement
  * `fmt` Formatting
  * `hdr` Header style
  * `invchar` Invalid characters
  * `loop` Loop style
  * `sclass` Storage class issues

You can also use ccheck-run.sh to check or fix all .c/.h files under
a certain directory.

Accepted syntax
---------------
ccheck has a good understanding of the C language (C89, C99, C11, C17,
but *not* K & R). Apart from standard C, ccheck also understands some
compiler-specific extensions, such as

  * GCC (extended) inline assembler
  * GCC `register ... asm(...)` variable register assignment
  * GCC attribute syntax
  * GCC's `__int128`, `__restrict__`

ccheck also understands the C++ `extern "C"` declaration embedded in
a header file.

ccheck can recognize Doxygen-style comments (`/**` and `/**<`) and check
they are spaced apart from the text within. It will also warn about
`/**<` incorrectly placed at the beginning of a line.

Since ccheck does not expand macros, thus it can fail to parse a source file
that uses the C preprocessor in a way that alters the language syntax.
Ccheck, however, supports some specific use cases of macros altering
the language syntax.

These use cases are:

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

Using Syc
---------
Syc is a C cross-compiler under construction, targetting the Sinclair
ZX Spectrum platform / Zilog Z80 processor. Coverage of C language features
is incomplete, with some important things still missing.

Specifically, these language features are supported:

 * All statements (except inline assembly)
 * Function declaration, definition, calling functions
 * Function arguments are passed via registers and the stack
   (any number of arguments of total size up to appprox. 128 bytes)
 * Most arithmetic operators
 * Signed and unsigned 8-bit (char) to 64-bit (long long) integer types
   and integer type conversions, usual arithmetic conversions
 * Basic support for pointers
 * Typedef
 * Struct, union, enum

These are NOT supported yet:

 * Constant expressions
 * Complex initializers
 * Arrays
 * Bit fields
 * Floating point
 * Integer promotion
 * Division and modulus (`/`, `%`)
 * Pointer difference
 * Character and string literals
 * Variadic functions
 * Ternary operator
 * Lvalue arguments
 * Passing or returning struct/union by value
 * Variable-length arrays
 * Sizeof, alignof

syc only starts after preprocessing stage (i.e. there must not be any
preprocessor directives in the C source file) and outputs an .asm
file in the end. This is supposed to be consumed by a Z80 assembler,
such as z80asm from z88dk project.

Running `syc` without arguments will print a syntax help. You can compile
and example C source file by typing

    $ ./syc example/test.c

which will produce `example/test.asm`. (Note that the file **must** have
a `.c` or `.C` extension). We can convert it to a tape file using tools from
the z88dk project

    $ z80asm +zx --origin=32768 -b -m example/test.asm example/lib.asm
    $ appmake +zx --org=32768 -b example/test.bin

(Note that example/test.c, specifically, refres to an assembly function
from example/lib.asm)

You can just type `make examples` to build all the examples automatically.
This produces a number of `.tap` files. We can transfer a `.tap` file to a real
Spectrum or open it in an emulator (such as [GZX][4]).

For example, select the `fillscr.tap` tape file and load it using the
BASIC command

    LOAD ""

This will load the binary and execute its main function, filling the screen
with black pixels.

The `example/test.c` file's main function does nothing interesting, instead
it contains a number of functions that can be individually executed as
User Service Routines. Let's take the add_const() function as an example.

Select the tape file `example/test.tap` and load it using the basic command

    LOAD ""CODE

this skips the BASIC loader and loads just the machine code block.
Consult the file `example/test.map` to determine the address where
the function `add_const` starts at and convert it from hex to decimal.
Let's say it starts at address 0x802a = 32810. This function computes
the value of the expression 1 + 2 + 3 and returns it. Because the function
is declared with the attribute `usr`, it will return the value in the BC
register, where BASIC expects value from the USR. We can test it with
the basic command

    PRINT USR 32810

this should print `6`.

There are options available to print out the
program in various compilation stages to the standard output:

 * `--dump-ast` Dump internal abstract syntax tree
 * `--dump-toks` Dump tokenized source file
 * `--dump-ir` Dump intermediate representation
 * `--dump-vric` Dump instruction code before register allocation

User service routines
---------------------
Any function without arguments can be called using the BASIC USR function,
but to return value, it must return a 16-bit integer and it must be
declared with the attribute `usr`. For example:

    unsigned my_usr(void) __attribute__((usr))
    {
            return 42;
    }

This is because user service routines must return value in the BC register
pair. Normal functions return 16-bit values in the HL register pair.

Syc as a checker
----------------
Syc strives to produce uparalleled diagnostic coverage, warning about 
all the potential programming errors that would be typically reported by
compilers, lint-like tools, as well as complementing ccheck in the C style
area.

It can detect the following types of problems and style issues:

 * declaration of '...' shadows a wider-scope declaration
 * gratuitous nested block
 * out of order declaration specifiers (such as `int long unsigned`,
   `int typedef`, `volatile restrict const`).
 * truth value used as an integer
 * using anything but `_Bool` or a thruth value where a truth value is
   expected
 * computed expression value is not used
 * specifically, ignoring return value of a function (TODO:
   this can be supressed on a per-function basis)
 * unused local variable
 * unused goto label
 * unused local struct/union tag
 * constant should be long (or long long)
 * unsigned comparison of mixed-sign integers
 * bitwise operation on signed integers
 * conversion may loose significant digits
 * implicit conversion between incompatible pointer types
 * implicit conversion from integer to pointer
 * implicit conversion between enum types
 * suspicious arithmetic/logic operation involving enums
 * comparison of different enum types
 * comparison of enum and non-enum type
 * converting to pointer from integer of different size
 * pointer should be the left operand while indexing
 * type definition in a non-global scope
 * definition of struct/union/enum shadows a wider-scope struct/union/enum
   definition
 * definition of struct/union/enum in non-global scope
 * definition of struct/union/enum inside another struct/union definition
 * definition of struct/union/enum inside parameter list will not be visible
   outside of function declaration/definition
 * struct/union/enum definition inside a cast
 * mixing arguments with and without an identifier
 * useless type in empty declaration
 * multiple declarations of function/variable/struct/union
 * declaration of function/variable/struct/union follows definition
 * variable not used since forward declaration

### Strict truth type

While C has the type `bool` (or `_Bool`), logic operations produce and
consume `int`, due to historic reasons. Syc pretends that C actually
has a built-in boolean type that is produced and consumed by logic
operations. It behaves just like `int`, except that trying to implicitly
convert between values of this type (which we call `truth values`)
and another type (e.g. `int`) produces a warning. Thus we can enforce
strict use of truth type while still allowing standards-compliant programs
to compile (by ignoring warnings).

For example:

    int i = 1 < 0;

will produce a warning, because we are converting a truth value (produced
by the comparison operator,) and implicitly converting it to an int.

Conversely:

    int i;
    if (i) {
	    return;
    }

will produce a warning, because we use int in a place where a truth value
is expected. (Should be changed to e.g. `if (i != 0)` or, if i is supposed
to be a boolean variable, its type needs to be changed to `bool`.

### Strict enum types

In the C standard enums are mostly interchangeable with integer types.
Implicit conversion from/to integer or arithmetic on enum types is allowed.
This can lead to errors. Syc pretends that enums are strictly typed.
Implicit conversion from/to other type (e.g. integer) will produce
a warning.

Arithmetic on strict enum types: It is allowed to add an integer
to an enum or to subtract integer from an enum (but not vice vesa),
the result is an enum. (Pre/post inc-/decrement also work).

It is allowed to subtract two values of the same enum type (the result
is an integer).

It is also allowed to compare two values of the same enum type.
Other operations (e.g. mutiplication, adding two enums and so on)
will produce a warning.

Trying to use an enum where a logic value is expected will also
produce a warning.

Enum types that do not have a tag, typedef or instance are not considered
strict. They are considered just a collection of integer constants.
For example:

    enum {
	    e1 = 1
    };

    int i = e1;

will not produce a warning, because e1 is considered to be just
an integer constant. On the other hand:

    typedef enum {
	    e1
    } e_t;

    e_t x = 1;
    int y = e1;

will produce two warnings, one for each assignment, because the type
e_t is considered a strict enum type and its members e1 are also
considered strictly enum values.

Intermediate Representation
---------------------------
The intermediate representation is a simple, but full-blown low-level
programming language. You can, for example, redirect the output of `--dump-ir`
to an `.ir` file and then run `syc <file>.ir` and it will produce the
exact same output as when run on the original C source file. You can also
write an IR file by hand and then compile it.

Developer Notes
---------------
These are notes on maintaining Sycek code base.

The ccheck regression tests (`test/ccheck/*/*-in.c`) are split into
three groups, good, bad and ugly. Good tests should result in a clean
run of ccheck with no output (i.e. no error and no issues found).
Bad tests should result in a fatal error and a specific error message
(`test/ccheck/bad/*-err.txt`) printed to standard error output.
Ugly tests should result in ccheck finding specific issues
(`test/ccheck/ugly/*-out.txt`) and if ccheck run with `--fix`, it should
transform the source code to look like `test/ugly/*-fixed.c`).

The syc regression tests (`test/syc/*/*.c`) are also split into three
groups, good bad and ugly. Good tests should result in a clean compilation
with no errors or warnings produced. Bad tests should result in a compilation
error and a specific error message (`test/syc/bad/*.txt`) printed to standard
error output. Ugly tests should result in successful compilation with
specific warnings (`test/syc/ugly/*.txt`) printed to the standard error
output. (NOTE: ugly syc tests are not implemented yet.)

After making any changes run `make test` command which runs a number of tests

 * Runs `ccheck` on the source code (self-test)
 * Runs `ccheck` internal unit tests (very sparse)
 * Runs `ccheck` on the regression tests in `test/ccheck` and checks output
 * Runs `ccheck` under Valgrind on all tests in `test/ccheck/` (except bad)
   and verifies that all memory blocks have been freed
 * Runs `syc` internal unit tests
 * Runs `syc` on the regression tests in `test/syc` and checks output
 * Runs `syc` under Valgrind on all tests in `test/syc/` (except bad)
   and verifies that all memory blocks have been freed

Everything should finish successfully (exit code from `make` should be zero).

To test functionality of generated Z80 code run `make test_z80` which
 * Compiles all sources under `test/syc/good` with Syc into `.asm` files
 * Assembles all of them using `z80asm` into binary files
 * Runs `z80test` for all `.scr` files under `test/syc/good` which verifies
   correct function of the generated code

For this to work you need to have `z80asm` from z88dk. Note that this
requirement is only temporary and will be removed once Syc can produce
binary files directly.

Run Clang Analyzer using the command

    $ make clean && scan-build make

which must finish without any bugs found.

[1]: http://www.helenos.org/wiki/CStyle
[2]: http://www.helenos.org/
[3]: https://github.com/jxsvoboda/timrec
[4]: https://github.com/jxsvoboda/gzx
