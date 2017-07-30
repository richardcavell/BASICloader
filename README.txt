BASICloader
Version : in development, don't use it yet
(c) 2017 Richard John Foster Cavell

BASICloader creates a COLOR BASIC program that will construct a machine
language program in memory, and then execute it.

The BASICloader program is designed to be run on any operating system
that has a command line. It should work on any UNIX-like operating system,
such as Linux or MacOS, or Windows if you have a command line in your
Windows installation. It is written in standard C (C89) and does not rely
on any external libraries or header files outside of the standard C library.

BASICloader reads in a Color Computer machine language program (the kind
produced by asm6809 using the -b option).

It outputs a COLOR BASIC program that can be run on a Color Computer 2.
This BASIC program incorporates the machine language program as data.
If run, it will poke the machine language program into the correct memory
locations, checking that it is doing so correctly, and then it will execute
the program.

The output of this program will therefore typically look like the old
"type-in" programs from 1980s computer magazines.

Richard Cavell




