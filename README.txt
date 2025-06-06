BASICloader by Richard Cavell version 1.0
Copyright (c) 2025
README.txt

BASICloader is intended to be used only by knowledgeable computer programmers.

This program is a magazine type-in generator.

In the 1980s, there were many 8-bit machines that could understand and execute BASIC as
well as machine language. Many nifty programs were written in machine language. There
were popular magazines for each of the popular computer systems. In order to distribute
these machine language programs in a form that ordinary people could use, magazines
would include type-in programs. They would look like this:

10 FOR I = 8192 TO 9108
20 READ A
30 POKE I,A
40 NEXT I
50 DATA 100,108,32,68,25,101,203
60 DATA 96,95,101,32,99,108,23
      ...etc...

BASICloader generates these type-in programs. You still have to provide it with the
machine language program, of course.

In general, you should only be using BASICloader with machine language that you created
yourself. It might not be proper to publish someone else's code in this way without their
permission.

Please don't try to pretend that you wrote the BASIC portion if you didn't. You should give
me and BASICloader the credit, if you reasonably can.

This program is licensed to you under the "MIT License".

You can see this project any time at https://github.com/richardcavell/BASICloader

You can contact me at richardcavell@mail.com

Richard Cavell
