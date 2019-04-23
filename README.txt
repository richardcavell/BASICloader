README.txt
BASICloader version : Still in development
(c) 2017-2019 Richard John Foster Cavell

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
are code for a machine-language program. The main part of the program is a
loop that pokes these values into memory.

BASICloader is designed to automatically construct the BASIC program. The
program can then be published in printed form, and typed in by an enthusiastic
computer user.

You still have to create the machine-language program, of course. BASICloader
will take this program as input. It can take many different types of input
file.

BASICloader has many features and is quite versatile in the sort of output
that it can generate.

For more information, take a look in the Documents folder.

Richard Cavell
