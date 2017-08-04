BASICloader
Version : in development, don't use it yet
(c) 2017 Richard John Foster Cavell

BASICloader reads in a machine language program, and creates a BASIC
program that will construct that machine language program in memory,
and then execute it.

The BASICloader program is designed to be run on any operating system
that has a command line. It should work on any UNIX-like operating system,
Linux, MacOS, or Windows. It is written in standard C (C89) and does not rely
on any external libraries or header files outside of the standard C library.
You might have to compile it by hand if you do not have GNU Make
on your operating system.

BASICloader reads in any machine language program designed to be run on the
TRS-80 Color Computer, Tano Dragon, or Commodore 64 (or a machine that is
similar to these).  This machine language program must be (for Coco/Dragon)
valid raw 6809 or 6309 code (the kind produced by asm6809 using the -b
option), or (for the Commodore 64) raw 6502 code.

BASICloader outputs a BASIC program that can be run on the target machine.
This BASIC program incorporates the machine language program as data.
If run, it will poke the machine language program into the correct memory
locations, checking that it is doing so correctly, and then it will execute
the program.

The output of this program will therefore typically look like the old
"type-in" programs from 1980s computer magazines.

If you want to use the output on an emulated Coco or Dragon using XRoar:

    $ ./BASICloader --machine coco XXX.bin
    $ xroar -run LOADER.BAS

If you want to use the output on an emulated C64 using VICE:

    $ ./BASICloader --machine c64petcat XXX.bin
    $ petcat -w2 -o loader.prg loader.bas
    $ c1541 -format loader,"88 2a" d64 loader.d64 -write loader.prg loader
    $ x64 -autostart loader.d64

Richard Cavell
