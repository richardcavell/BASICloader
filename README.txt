README.txt
BASICloader version : Still in development.
(c) 2017-2019 Richard John Foster Cavell
https://github.com/richardcavell/BASICloader

BASICloader is a magazine type-in generator. It is free, open-source software
that is intended as a retro computing project.

There were many 8-bit computer systems in the 1980s. They typically had a
built-in BASIC interpreter, as well as having the ability to execute machine
language programs.

Magazines that were devoted to these 8-bit systems often had program source
code listings. The idea is that you would type in the program and run it.
Many of these programs looked like this:

    10 FOR I = 8192 TO 9432
    20 READ A
    30 POKE I,A
    40 NEXT I
    50 DATA 56, 23, 78, 102, 9, 207, 193, 8
    60 DATA 88, 0, 0, 0, 1, 107, 92, 93, 107
       ... etc ...

This is a BASIC program, which contains DATA statements. The DATA statements
are code for a machine-language program. The main part of this BASIC program
is a loop that pokes these values into memory. BASICloader creates BASIC
programs of this type.

When given a machine language program (or any other data), BASICloader will
automatically construct a BASIC program. The program that BASICloader creates,
if it is typed in and run, will set up that machine language program (or data)
in memory. The BASIC program can be published in printed form, and typed in
by an enthusiastic computer user.

You still have to create the machine-language program, of course. BASICloader
can take many different types of file as input.

BASICloader is quite versatile in the sort of output that it can generate.

Here are some of BASICloader's features:

    * Written entirely in standard C, so that it can be compiled and run
      on a wide range of operating systems and machines

    * Is specially written to detect problems with old compilers and
      unusual implementation choices

    * Has exactly one (long) source file, so that building and linking
      couldn't be simpler

    * Understands the file format native to each machine

    * Very customizable output, with many command-line switches

    * Detects every error and problem that can reasonably be detected

    * BASICloader and its output carry permissive licenses, so that you
      can use it widely in your own work

BASICloader can optionally:

    * Target one of several different machines (and BASIC dialects):

        * TRS-80 Color Computer 1, 2 and 3
        * Dragon 32 and 64
        * Commodore 64

    * Emit special instructions for Extended Color BASIC
      (For the Color Computer)

    * Output in uppercase, lowercase or mixed case

    * Add custom REM statements and build date

    * Produce and use checksums to ensure the program is typed correctly

    * Produce output that is compact (saves memory) or not (easier to
      read and type)

      ... and more!

For more information, take a look at FAQ.txt in the Documents folder.

You can find this project on the web at:
  https://github.com/richardcavell/BASICloader

Richard Cavell
