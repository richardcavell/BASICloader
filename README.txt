BASICloader
Version : in development, don't use it yet
(c) 2017 Richard John Foster Cavell

BASICloader reads in a machine language program, and creates a BASIC
program that will construct that machine language program in memory,
and then execute it.

The BASICloader program is designed to be run on any operating system
that has a command line. It should work on Linux, MacOS, Windows, UNIX,
or almost any other operating system that can compile standard C.

It is written in standard C (C89) and does not rely on any external
libraries or header files outside of the standard C library. The Makefile
is intended for GNU Make. If you don't have GNU Make, you will have to
compile BASICloader manually. This won't be difficult.

Currently BASICloader supports the following target computers:

  * TRS-80 Color Computer 1 and 2
  * TRS-80 Color Computer 1 and 2 with Extended Color BASIC
  * TRS-80 Color Computer 3
  * Dragon 32 and Dragon 64
  * Commodore 64

A number of computer models that are similar to the above machines may
be able to run the output as well.

This BASIC program output by BASICloader incorporates the machine language
program as data. If run on the target machine, it will poke the machine
language program into the correct memory locations, checking that it is
doing so correctly, and then it will execute the program.

The output of BASICloader will therefore look like the old "type-in"
programs from 1980s computer magazines.

The start location defaults to a value that is sensible for the target
machine.  The exec location defaults to being equal to the start value.

The output of this program is licensed to you under the following license:

  1.  You may use the output of this program, for free, for any worthwhile
      or moral purpose.
  2.  You should attribute me and the BASICloader program where that does
      not cause an unreasonable burden on you.

You should not allow people to assume that you wrote the BASIC code yourself.

If you want to use the output on an emulated Color Computer using XRoar,
try these commands:

    $ ./BASICloader --machine coco XXX.bin
    $ xroar -machine coco2b -run LOADER.BAS

If you want to use the output on an emulated Dragon 64 using XRoar,
try these commands:

    $ ./BASICloader --machine coco XXX.bin
    $ xroar -machine dragon64 -run LOADER.BAS

If you want to use the output on an emulated C64 using VICE:

    $ ./BASICloader --machine c64_lc XXX.bin
    $ petcat -w2 -o loader.prg loader.bas
    $ c1541 -format loader,"88 2a" d64 loader.d64 -write loader.prg loader
    $ x64 -autostart loader.d64

Richard Cavell
