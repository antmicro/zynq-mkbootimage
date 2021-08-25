# Zynq mkbootimage project organization

This file is meant to help contributors getting familiar with organization of this project.

## Source tree
The project source tree has the following structure:

```
/ - the root project directory
  LICENSE       - projects license file (BSD 2-clause)
  Makefile      - supports `make [all|exbootimage|makebootimage|format|test]`
  README.md     - this file
  .clang-format - project formatter config

tests/ - tests
  tester.sh - testing script

src/ - project source code
  bif.c         - BIF file parser
  bootrom.c     - boot image generator
  common.c      - common tool routines used by the whole project
  common.h      - as above + definitions of error codes
  exbootimage.c - main routine of `exbootimage` and its most important routines
  mkbootimage.c - main routine of `mkbootimage`

src/arch/ - architecture-specific header initializers
  common.c - common initilization routines
  zynq.c   - routines for Zynq
  zynqmp.c - routines for ZynqMP

src/file/ - sources for file formats that need special operations
  bitstream.c - bitstream creation and extraction routines
  elf.c       - routines extracting ELF information
```

Most of header files haven't been mentioned since they have the same roles as their corresponding C files.

## Code style
The code style is specified in `.clang-format` file, generally it resembles the Linux style with two spaces wide indent and 100 characters column limit.

To assure correctness of formatting, run
```
make format
```
before committing and pushing your changes.

It is recommended to use `clang-format` version 12 as it is used by the formatting checks on GitHub, any newer version should be appropriate as well though.

## Tests
To start tests, run:
```
make test
```

All kinds of tests are implemented as routines in `tests/tester.sh`.
Tests-specific files are put into subdirectories of `tests/` and named after functionality they are testing.

Further details on implementing new tests can be found in the comments of the `tests/tester.sh` script itself.

## Error codes
The error codes are defined in `common.h` and used by the whole project.
They are values of the `error` type, and so almost every routine that can fail has the `error` return value.

All the error message follow the schema below:
```
error: [where: ] <message>
```

For BIF parser the `[where: ]` clause is `filename:line:column: `, for image extractor it is a hexadecimal offset of a wrong value encountered in a boot image file.
If the place in which the error occured cannot be precisely addressed, the `[where: ]` clause can be omitted.

All errors are printed with the use of `errorf` routine, which is of the same type as `printf`, only it prints the output to `stderr` and begins every message with an `error: ` prefix.
The only exception is the BIF parser which uses `perrof` routine which uses lexer state to specify the error source.

The error messages should be printed as early as the error is detected and the error code should be returned and passed to the caller, until it is handled or returned as an exit value in `main`.
