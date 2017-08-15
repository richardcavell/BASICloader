/* BASICloader.c
 *
 * A program by Richard Cavell (c) 2017
 * https://github.com/richardcavell/BASICloader
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

enum machine_type
{
  default_machine = 0,
  coco,
  c64
};

enum case_type
{
  default_case = 0,
  upper,
  lower,
  both
};

#define           DEFAULT_MACHINE            coco
#define           DEFAULT_CASE               upper

#define       C64_DEFAULT_OUTPUT_FILENAME    "LOADER.BAS"
#define    C64_LC_DEFAULT_OUTPUT_FILENAME    "loader.bas"
#define      COCO_DEFAULT_OUTPUT_FILENAME    "LOADER.BAS"

#define         MIN_BASIC_LINE_NUMBER        0
#define     TYPABLE_START_LINE_NUMBER        10
#define         MAX_BASIC_LINE_NUMBER        63999
#define     DEFAULT_BASIC_LINE_STEP_SIZE     1
#define     TYPABLE_BASIC_LINE_STEP_SIZE     10

#define     C64_MAX_BASIC_LINE_LENGTH        79
#define    COCO_MAX_BASIC_LINE_LENGTH        249
#define             BASIC_LINE_WRAP_POS      70

#define       C64_DEFAULT_START_ADDRESS      0x8000
#define      COCO_DEFAULT_START_ADDRESS      0x3e00
#define   MAX_MACHINE_LANGUAGE_BINARY_SIZE   65536
#define         HIGHEST_RAM_ADDRESS          0xffff
#define         HIGHEST_32K_ADDRESS          0x7fff
#define         HIGHEST_16K_ADDRESS          0x3fff
#define          HIGHEST_8K_ADDRESS          0x1fff
#define          HIGHEST_4K_ADDRESS          0x0fff

#define             SCRATCH_SIZE             300
#define            UCHAR_MAX_8_BIT           255

static void
fail(const char *fmt,...)
{
  va_list ap;

  (void) fprintf(stderr, "Error: ");

  va_start(ap, fmt);
  (void) vfprintf(stderr, fmt, ap);
  va_end(ap);

  (void) fprintf(stderr, "\n");

  exit(EXIT_FAILURE);
}

static void
emit_fail(void)
{
  fail("Couldn't write to output file. Error number %d", errno);
}

static unsigned int
next_line_number(unsigned int *first_line,
                 unsigned int *line,
                 unsigned int step)
{
  unsigned int old_line = *line;

  if(*first_line == 1)
    *first_line = 0;
  else
    *line += step;

  if (*line < old_line)
    fail("Line number overflow");

  if (*line > MAX_BASIC_LINE_NUMBER)
    fail("The BASIC line numbers have become too large");

  return *line;
}

static void
check_pos(unsigned int *pos, enum machine_type machine)
{
  unsigned int max_basic_line_length = 0;

  switch(machine)
  {
    case coco:
           max_basic_line_length = COCO_MAX_BASIC_LINE_LENGTH;
           break;

    case c64:
           max_basic_line_length = C64_MAX_BASIC_LINE_LENGTH;
           break;

    default:
           fail("Internal error");
  }

  if (*pos > max_basic_line_length)
    fail("Internal error: BASIC maximum line length exceeded");
}

static void
caseify(char *s, enum case_type cse)
{
  while (*s)
  {
    switch(cse)
    {
      case upper:
           *s = (char) toupper(*s);
           break;
      case lower:
           *s = (char) tolower(*s);
           break;
      default:
           break;
    }

    ++s;
  }
}

static unsigned int
emit(FILE *fp, enum case_type cse, const char *fmt, ...)
{
  char scratch[SCRATCH_SIZE];
  int bytes = 0;
  va_list ap;

  va_start(ap, fmt);
  bytes = vsprintf(scratch, fmt, ap);
  va_end(ap);

  if (bytes < 0)
    emit_fail();

  caseify(scratch, cse);

  if (fprintf(fp, "%s", scratch) < 0)
    emit_fail();

  return (unsigned int) bytes;
}

static void
emit_datum(FILE *fp,
           unsigned char c,
           enum machine_type machine,
           enum case_type cse,
           int typable,
           unsigned int *first_line,
           unsigned int *line,
           unsigned int step,
           unsigned int *pos)
{
  if (*pos > BASIC_LINE_WRAP_POS)
  {
    (void) emit(fp, cse, "\n");
    *pos = 0;
  }

  if (*pos == 0)
  {
    *pos = emit(fp, cse,
                    "%u DATA%s",
                    next_line_number(first_line, line, step),
                    typable ? " " : "" );
  }
  else
  {
    *pos += emit(fp, cse, typable ? ", " : ",");
  }

  *pos += emit(fp, cse, "%u", c);

  check_pos(pos, machine);
}

static void
emit_line(FILE *fp,
          unsigned int *pos,
          unsigned int *first_line,
          unsigned int *line,
          unsigned int step,
          enum machine_type machine,
          enum case_type cse,
          const char *fmt, ...)
{
  char scratch[SCRATCH_SIZE];
  int bytes = 0;
  va_list ap;

  *pos = emit(fp, cse, "%u ", next_line_number(first_line, line, step));

  va_start(ap, fmt);
  bytes = vsprintf(scratch, fmt, ap);
  va_end(ap);

  if (bytes < 0)
    emit_fail();

  *pos += (unsigned int) bytes;
  check_pos(pos, machine);

  (void) emit(fp, cse, "%s\n", scratch);
  *pos = 0;
}

static int
match_arg(const char *arg, const char *shrt, const char *lng)
{
  return (   strcmp(arg, shrt)==0
          || strcmp(arg,  lng)==0 );
}

static unsigned short int
get_short(const char *s, int *ok)
{
  char *endptr = NULL;
  unsigned long int l = 0;

  errno = 0;

  if (s != NULL)
    l = strtoul(s, &endptr, 0);

  *ok = ( s != NULL
          && endptr
          && (*endptr=='\0')
          && (errno == 0)
          && (l <= USHRT_MAX) );

  return (unsigned short int) l;
}

static int
get_shrt_arg(char **pargv[], const char *shrt, const char *lng,
             unsigned short int *ps)
{
  int matched;

  if ((matched = match_arg((*pargv)[0], shrt, lng)))
  {
    int ok = 0;

    if (*ps)
      fail("Option %s can only be set once", (*pargv)[0]);

    *ps = get_short((*pargv)[1], &ok);

    if (!ok)
      fail("Invalid argument to %s", (*pargv)[0]);

#if (USHRT_MAX > HIGHEST_RAM_ADDRESS)
    if (*ps > HIGHEST_RAM_ADDRESS)
      fail("%s cannot be greater than %x", (*pargv)[0], HIGHEST_RAM_ADDRESS);
#endif

    ++(*pargv);
  }

  return matched;
}

static int
get_str_arg(char **pargv[], const char *shrt, const char *lng,
            char **s)
{
  int matched;

  if ((matched = match_arg((*pargv)[0], shrt, lng)))
  {
    if ((*pargv)[1] == NULL)
      fail("You must supply a string argument to %s", (*pargv)[0]);

    if (*s)
      fail("You can only set option %s once", (*pargv)[0]);

    *s = (*pargv)[1];

    ++(*pargv);
  }

  return matched;
}

static int
get_c_arg(char **pargv[], const char *shrt, const char *lng,
          enum case_type *cse)
{
  int matched = 0;
  char *opt = NULL;

  if ((matched = get_str_arg(pargv, shrt, lng, &opt)))
  {
         if (strcmp(opt, "upper") == 0)      *cse = upper;
    else if (strcmp(opt, "lower") == 0)      *cse = lower;
    else if (strcmp(opt, "both") == 0)       *cse = both;
    else fail("Unknown case option %s", (*pargv)[0]);
  }

  return matched;
}

static int
get_m_arg(char **pargv[], const char *shrt, const char *lng,
          enum machine_type *machine)
{
  int matched = 0;
  char *name = NULL;

  if ((matched = get_str_arg(pargv, shrt, lng, &name)))
  {
         if (strcmp(name, "coco") == 0)      *machine = coco;
    else if (strcmp(name, "c64") == 0)       *machine = c64;
    else fail("Unknown machine %s", (*pargv)[0]);
  }

  return matched;
}

static int
get_sw_arg(const char *arg, const char *shrt, const char *lng,
           int *sw)
{
  int matched = 0;

  if ((matched = match_arg(arg, shrt, lng)))
    *sw = 1;

  return matched;
}

static void
print_version(void)
{
  puts("BASICloader (under development)");
  puts("(c) 2017 Richard Cavell");
  puts("https://github.com/richardcavell/BASICloader");
}

static void
version(void)
{
  print_version();
  exit(EXIT_SUCCESS);
}

static void
help(void)
{
  print_version();
  puts("Usage: BASICloader [options] [filename]");
  puts("  -o  --output    Output file");
  puts("  -m  --machine   Target machine");
  puts("  -s  --start     Start location");
  puts("  -e  --exec      Exec location");
  puts("  -n  --nowarn    Don't warn about RAM requirements");
  puts("  -t  --typable   Unpacked, one instruction per line");
  puts("  -x  --extbas    Assume Extended Color BASIC");
  puts("  -c  --case      Output case (lower/upper/both)");
  puts("  -r  --remarks   Add remarks to the program");
  puts("  -h  --help      This help information");
  puts("  -i  --info      What this program does");
  puts("  -l  --license   Your license to use this program");
  puts("  -v  --version   Version of this software");
  exit(EXIT_SUCCESS);
}

static void
license(void)
{
  puts("BASICloader License");
  puts("");
  puts("(\"MIT License\")");
  puts("");
  puts("Copyright (c) 2017 Richard Cavell");
  puts("");
  puts("Permission is hereby granted, free of charge, to any person obtaining a copy");
  puts("of this software and associated documentation files (the \"Software\"), to deal");
  puts("in the Software without restriction, including without limitation the rights");
  puts("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell");
  puts("copies of the Software, and to permit persons to whom the Software is");
  puts("furnished to do so, subject to the following conditions:");
  puts("");
  puts("The above copyright notice and this permission notice shall be included in all");
  puts("copies or substantial portions of the Software.");
  puts("");
  puts("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR");
  puts("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,");
  puts("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE");
  puts("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER");
  puts("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,");
  puts("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE");
  puts("SOFTWARE.");
  exit(EXIT_SUCCESS);
}

static void
list(void)
{
  printf("The default target is : ");
  switch(DEFAULT_MACHINE)
  {
    case coco:      printf("coco\n");      break;
    case c64:       printf("c64\n");       break;
    default:        fail("Internal error");
  }
  printf("The default output is : ");
  switch(DEFAULT_CASE)
  {
    case upper:     printf("uppercase\n"); break;
    case lower:     printf("lowercase\n"); break;
    case both:      printf("both\n");      break;
    default:        fail("Internal error");
  }

    puts("");
    puts("Available target architectures are :");
    puts("         coco   TRS-80 Color Computer (any model)");
    puts("                or Dragon (any model)");
  printf("                Default output filename: %s\n",
                             COCO_DEFAULT_OUTPUT_FILENAME);
    puts("");
    puts("          c64   Commodore 64 (or any compatible computer)");
  printf("                Default output filename: %s\n",
                              C64_DEFAULT_OUTPUT_FILENAME);
  printf("                (or %s with --case lower)\n",
                              C64_LC_DEFAULT_OUTPUT_FILENAME);
    puts("");
    puts("Use --case lower to use C64 output with VICE and petcat");

  exit(EXIT_SUCCESS);
}

static void
info(void)
{
  print_version();
  puts("");
  puts("BASICloader reads in a machine language binary, and then constructs");
  puts("a BASIC program that will run on the target architecture.");
  puts("");
  puts("The BASIC program so produced contains DATA statements that represent");
  puts("the machine language given to BASICloader when the BASIC program");
  puts("was generated.");
  puts("");
  puts("When run on the target architecture, the BASIC program will poke");
  puts("that machine language into memory, and execute it.");
  puts("");

  list();

  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
  enum machine_type machine = default_machine;
  FILE *fp = NULL;
  FILE *ofp = NULL;
  char *fname = NULL;
  char *ofname = NULL;
  unsigned int first_line = 1;
  unsigned int line = 0;
  unsigned int step = DEFAULT_BASIC_LINE_STEP_SIZE;
  unsigned int pos = 0;
  unsigned short int start = 0;
  unsigned short int end   = 0;
  unsigned short int exec  = 0;
  long int size = 0;
  int nowarn = 0;
  int typable = 0;
  int remarks = 0;
  int extbas = 0;
  enum case_type cse = default_case;
  int c = 0;

#if (UCHAR_MAX < UCHAR_MAX_8_BIT)
    fail("This machine cannot process 8-bit bytes");
#endif

  if (argc > 0)
    while (*++argv)
    {
           if (    match_arg (argv[0], "-h", "--help"))
             help();
      else if (    match_arg (argv[0], "-i", "--info"))
             info();
      else if (    match_arg (argv[0], "-v", "--version"))
             version();
      else if (    match_arg (argv[0], "-l", "--license"))
             license();
      else if (
                   get_str_arg (&argv, "-o", "--output",   &ofname)
                || get_shrt_arg(&argv, "-s", "--start",    &start)
                || get_shrt_arg(&argv, "-e", "--exec",     &exec)
                || get_m_arg   (&argv, "-m", "--machine",  &machine)
                || get_sw_arg(argv[0], "-n", "--nowarn",   &nowarn)
                || get_sw_arg(argv[0], "-t", "--typable",  &typable)
                || get_sw_arg(argv[0], "-x", "--extbas",   &extbas)
                || get_sw_arg(argv[0], "-r", "--remarks",  &remarks)
                || get_c_arg   (&argv, "-c", "--case",     &cse)
              )
            ;
      else if (argv[0][0]=='-')
             fail("Unknown command line option %s", argv[0]);
      else
           {
             if (fname)
               fail("Only one input filename may be specified");

             fname = argv[0];
           }
    }

  if (machine == default_machine)
    machine = DEFAULT_MACHINE;

  if (extbas && machine != coco)
    fail("Extended Color BASIC can only be selected with the coco target");

  if (cse == default_case)
    cse = DEFAULT_CASE;

  if (fname == NULL)
    fail("You must specify an input file");

  if (ofname == NULL)
      switch(machine)
      {
      case coco:
             ofname = COCO_DEFAULT_OUTPUT_FILENAME;
             break;
      case c64:
             ofname = (cse == lower) ?
               C64_LC_DEFAULT_OUTPUT_FILENAME :
               C64_DEFAULT_OUTPUT_FILENAME;
             break;
      default:
             fail("Internal error");
      }

  if (start == 0)
  {
         if (machine == coco)
               start = COCO_DEFAULT_START_ADDRESS;
    else if (machine == c64)
               start = C64_DEFAULT_START_ADDRESS;
  }

  if (exec == 0)
    exec = start;

  errno = 0;

  fp = fopen(fname, "r");

  if (fp == NULL)
    fail("Could not open file %s. Error number %d", fname, errno);

  if (fseek(fp, 0L, SEEK_END))
    fail("Could not operate on file %s. Error number %d", fname, errno);

  size = ftell(fp);

  if (size < 0)
    fail("Could not operate on file %s. Error number %d", fname, errno);

  if (size == 0)
    fail("File %s is empty", fname);

  if (size > MAX_MACHINE_LANGUAGE_BINARY_SIZE)
    fail("Input file %s is too large", fname);

  if (fseek(fp, 0L, SEEK_SET) < 0)
    fail("Could not rewind file %s", fname);

  ofp = fopen(ofname, "w");

  if (ofp == NULL)
    fail("Could not open file %s. Error number %d", fname, errno);

  end = (unsigned short int) (start + size - 1);

  if ( /* Suppress warning about comparison always false */
#if (HIGHEST_RAM_ADDRESS != USHRT_MAX)
    (end > HIGHEST_RAM_ADDRESS) ||
#endif
     (end < start) )
  {
    fail("The machine language blob would overflow the 64K RAM limit");
  }
  else if ( !nowarn &&
            (machine == coco) )
  {
    if (end > HIGHEST_32K_ADDRESS)
      puts("Warning: Program requires 64K of RAM");
    else if (end > HIGHEST_16K_ADDRESS)
      puts("Warning: Program requires at least 32K of RAM");
    else if (end > HIGHEST_8K_ADDRESS)
      puts("Warning: Program requires at least 16K of RAM");
    else if (end > HIGHEST_4K_ADDRESS)
      puts("Warning: Program requires at least 8K of RAM");
  }

  if (exec < start || exec > end)
  {
    puts("Warning: The exec location given is not within the start and");
    puts("end of the binary blob that will be loaded into memory.");
  }

  line = (typable) ? TYPABLE_START_LINE_NUMBER :
                     MIN_BASIC_LINE_NUMBER;

  step = (typable) ? TYPABLE_BASIC_LINE_STEP_SIZE :
                     DEFAULT_BASIC_LINE_STEP_SIZE;

  if (machine == coco && extbas)
    emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
              "CLEAR200,%d", start - 1);

  if (machine == c64)
        emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                  "POKE55,%d:POKE56,%d", start%256, start/256);

  if (remarks)
  {
    char s[60];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int use_time = strftime(s, sizeof s, "%d %B %Y", tm) > 0;
    unsigned int i = 0;

    for (i = 0; i < sizeof s; ++i)
      s[i] = (char) ((cse == lower) ? tolower(s[i]) : toupper(s[i]) );

    emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
              "REM   THIS PROGRAM WAS");
    emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
              "REM GENERATED BY BASICLOADER");

    if (use_time)
      emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
              "REM   ON %-15s", s);

    emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
              "REM SEE GITHUB.COM/");
    emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
              "REM      RICHARDCAVELL");
  }

  if (!typable)
  {
    switch (machine)
    {
      case coco:
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "FORP=%dTO%d:READA:POKEP,A", start, end);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "IFA<>PEEK(P)THENGOTO%d",line+3*step);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "NEXT:EXEC%d:END",exec);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "PRINT\"ERROR!\":END");
             break;

      case c64:
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "FORP=%dTO%d:READA:POKEP,A", start, end);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "IFA<>PEEK(P)THENGOTO%d",line+3*step);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "NEXT:SYS%d:END",exec);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "PRINT\"ERROR!\":END");
             break;

      default:
             fail("Internal error");
    }
  }
  else
  {
    switch(machine)
    {
      case coco:
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "FOR P=%d TO %d", start, end);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "READ A");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "POKE P,A");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "IF A<>PEEK(P) THEN GOTO %d",line+5*step);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "NEXT P");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "EXEC %d", exec);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "END");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "PRINT \"ERROR!\"");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "END");
             break;

      case c64:
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "FOR P=%d TO %d", start, end);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "READ A");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "POKE P,A");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "IF A<>PEEK(P) THEN GOTO %d",line+5*step);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "NEXT P");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "SYS %d",exec);
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "END");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "PRINT \"ERROR!\"");
             emit_line(ofp, &pos, &first_line, &line, step, machine, cse,
                       "END");
             break;

      default:
             fail("Internal error");
    }
  }

  while ((c = fgetc(fp)) != EOF)
  {
    unsigned char d = (unsigned char) c;

#if (UCHAR_MAX != UCHAR_MAX_8_BIT)
    if (d > UCHAR_MAX_8_BIT)
      fail("Input file contains a value that's too high for an 8-bit machine");
#endif

    emit_datum(ofp, d, machine, cse, typable,
               &first_line, &line, step, &pos);
  }

  if (pos > 0)
    (void) emit(ofp, cse, "\n");

  if (fclose(ofp))
    fail("Couldn't close file %s", ofname);

  printf("BASIC program has been generated -> %s\n", ofname);

  if (fclose(fp))
    fail("Couldn't close file %s", fname);

  return EXIT_SUCCESS;
}
