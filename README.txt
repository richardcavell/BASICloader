BASICloader
Version : in development, don't use it yet
(c) 2017-2019 Richard John Foster Cavell

BASICloader is a type-in program generator.

BASICloader reads in a machine language program, and creates a BASIC
program that will construct that machine language program in memory,
and then execute it.

The BASICloader program is designed to be run on any operating system
that has a command line. It should work on Linux, MacOS, Windows, UNIX,
or any other operating system that can compile standard C.

It is written in standard C (C89) and does not rely on any external
libraries or header files outside of the standard C library. The Makefile
is intended for GNU Make. If you don't have GNU Make, you will have to
compile BASICloader manually. This won't be difficult. There is only one
source file.

Currently BASICloader supports the following target computers:

  * TRS-80 Color Computer 1, 2 and 3
  * TRS-80 Color Computer 1, 2 and 3 with Extended Color BASIC
  * Dragon 32 and Dragon 64
  * Commodore 64

A number of computer models that are similar to the above machines will
be able to run the output as well. The Color Computer can be referred to
as "Coco".

The BASIC program that is output by BASICloader incorporates the machine
language program as data. If run on the target machine, it will poke the
machine language program into the correct memory locations, optionally
checking that it is doing so correctly, and then (optionally) it will
execute the program.

The output of BASICloader will therefore look like the old "type-in"
programs from 1980s computer magazines. The core of the program is a loop
that uses the READ keyword and the POKE command. The program will contain
DATA statements as well.

The input file must be a machine language program. It can be a plain binary,
an RS-DOS Coco file, a Dragon DOS file, or a Commodore 64 PRG file. The Coco,
Dragon and PRG formats specify the start location (where the binary blob is
loaded to). The Coco and Dragon formats specify an exec location as well
(where the execution begins). For binary and PRG files, we assume that
execution starts at the first (lowest) memory location.

BASICloader will correctly obtain start and exec information, if available
in the chosen input format.

If you do not specify a start location, it defaults to a value that
is sensible for the target machine.  The exec location defaults to being
equal to the start value.

You can read more about the command line options in the file Options.txt,
and you can read about technical information in Technical_info.txt

The output of this program is licensed to you under the following license:

  1.  You may use the output of this program, for free, for any worthwhile
      or moral purpose.
  2.  You should try to attribute me and the BASICloader program, where
      that is not unreasonable.

You should not allow people to assume that you wrote the BASIC code yourself.

If you want to use the output on an emulated Color Computer using XRoar,
try these commands:

    $ ./BASICloader --machine coco XXX.bin
    $ xroar -machine coco2b -run LOADER.BAS

If you want to use the output on an emulated Dragon 64 using XRoar,
try these commands:

    $ ./BASICloader --machine dragon XXX.bin
    $ xroar -machine dragon64 -run LOADER.BAS

If you want to use the output on an emulated C64 using VICE:

    $ ./BASICloader --machine c64 --case lower XXX.bin
    $ petcat -2 -w2 -o loader.prg loader
    $ c1541 -format loader,"88 2a" d64 loader.d64 -write loader.prg loader
    $ x64 -autostart loader.d64

Richard Cavell
