/* BASICloader.c
 *
 * A program by Richard Cavell (c) 2017
 * https://github.com/richardcavell/BASICloader
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>

enum machine_type
{
  default_machine = 0,
  coco,
  c64
};

enum format_type
{
  default_format = 0,
  binary,
  format_coco,
  dragon,
  prg
};

enum case_type
{
  default_case = 0,
  upper,
  lower,
  mixed
};

        /* User-modifiable values */

#define           DEFAULT_MACHINE            coco
#define           DEFAULT_FORMAT             binary
#define           DEFAULT_CASE               upper

#define           DEFAULT_OUTPUT_FILENAME    "LOADER.BAS"
#define    C64_LC_DEFAULT_OUTPUT_FILENAME    "loader"

#define     DEFAULT_START_LINE_NUMBER        MIN_BASIC_LINE_NUMBER
#define     TYPABLE_START_LINE_NUMBER        10
#define     DEFAULT_BASIC_LINE_STEP_SIZE     1
#define     TYPABLE_BASIC_LINE_STEP_SIZE     10
#define         MAX_BASIC_LINES              10000
#define         MAX_BASIC_PROG_SIZE          65000
#define             BASIC_LINE_WRAP_POS      70
#define                CS_DATA_PER_LINE      15

#define       C64_DEFAULT_START_ADDRESS      0x8000
#define      COCO_DEFAULT_START_ADDRESS      0x3e00

#define             SCRATCH_SIZE             300

        /* End of user-modifiable values */

#define MAX_MACHINE_LANGUAGE_BINARY_SIZE 65536

#define HIGHEST_RAM_ADDRESS 0xffff
#define HIGHEST_32K_ADDRESS 0x7fff
#define HIGHEST_16K_ADDRESS 0x3fff
#define HIGHEST_8K_ADDRESS  0x1fff
#define HIGHEST_4K_ADDRESS  0x0fff

#define C64_MAX_BASIC_LINE_LENGTH   79
#define COCO_MAX_BASIC_LINE_LENGTH 249

#define MIN_BASIC_LINE_NUMBER 0
#define MAX_BASIC_LINE_NUMBER 63999

#define UCHAR_MAX_8_BIT 255

#define DRAGON_HEADER 10
#define COCO_AMBLE 5

static void
fail(const char *fmt, ...)
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
warning_fail(void)
{
  fail("Couldn't print warning to standard output");
}

static void
warning(const char *fmt, ...)
{
  va_list ap;
  int res = 0;

  if (printf("Warning: ") < 0)
    warning_fail();

  va_start(ap, fmt);
  res = vprintf(fmt, ap);
  va_end(ap);

  if (printf("\n") < 0)
    warning_fail();

  if (res < 0)
    warning_fail();
}

static void
inc_line_number(unsigned int *first_line,
                unsigned int *line,
                unsigned int *step)
{
  if(*first_line == 1)
  {
    *first_line = 0;
  }
  else
  {
    unsigned int old_line = *line;

    *line += *step;

    if (*line < old_line)
      fail("Internal error: Line number overflow");
  }

#if (MIN_BASIC_LINE_NUMBER > 0)
  if (*line < MIN_BASIC_LINE_NUMBER)
    fail("Internal error: Line number error in inc_line_number()");
#endif

#if (MAX_BASIC_LINE_NUMBER > UINT_MAX)
  if (*line > MAX_BASIC_LINE_NUMBER)
    fail("The BASIC line numbers have become too large");
#endif
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
           fail("Internal error: Unhandled machine type in check_pos()");
  }

  if (*pos > max_basic_line_length)
    fail("Internal error: BASIC maximum line length exceeded");
}

static void
caseify(char *s, enum case_type cse)
{
  while (*s)
  {
    switch (cse)
    {
      case upper:  *s = (char) toupper(*s);                   break;
      case lower:  *s = (char) tolower(*s);                   break;
      default:                                                break;
    }

    ++s;
  }
}

static void
inc_line_count(unsigned int *line_count)
{
  if (*line_count == UINT_MAX)
    fail("Internal error: Line count has overflowed");

  ++*line_count;

#if (MAX_BASIC_LINES < UINT_MAX)
  if (*line_count > MAX_BASIC_LINES)
    fail("Line count has exceeded the set limit");
#endif
}

#define EMIT_FAIL -1
#define TOO_LARGE -2

static int
vemit(FILE *fp, enum case_type cse, const char *fmt, va_list ap)
{
  char scratch[SCRATCH_SIZE];
  long int osize;
  int rval = 0;

  rval = vsprintf(scratch, fmt, ap);

  if (rval < 0)
    return EMIT_FAIL; /* Todo: Give this its own code */

  caseify(scratch, cse);

  if (fprintf(fp, "%s", scratch) < 0)
    return EMIT_FAIL;

  osize = ftell(fp);

  if (osize < 0)
    return EMIT_FAIL; /* Todo: Give this its own code */

  if (osize > MAX_BASIC_PROG_SIZE)
    return TOO_LARGE;

  return rval;
}

static void
check_rval(int rval)
{
  if (rval == EMIT_FAIL)
    fail("Couldn't write to output file. Error number %d", errno);

  if (rval == TOO_LARGE)
    fail("Generated BASIC program is too large");
}

static unsigned int
emit(FILE *fp, enum case_type cse, const char *fmt, ...)
{
  int bytes = 0;
  va_list ap;

  va_start(ap, fmt);
  bytes = vemit(fp, cse, fmt, ap);
  va_end(ap);

  check_rval(bytes);

  return (unsigned int) bytes;
}

static void
emit_datum(FILE *fp,
           unsigned int d,
           enum machine_type machine,
           enum case_type cse,
           int typable,
           unsigned int *first_line,
           unsigned int *line_count,
           unsigned int *line,
           unsigned int *step,
           unsigned int *pos)
{
  if (*pos > BASIC_LINE_WRAP_POS)
  {
    (void) emit(fp, cse, "\n");
    inc_line_count(line_count);
    *pos = 0;
  }

  if (*pos == 0)
  {
    inc_line_number(first_line, line, step);
    *pos = emit(fp, cse,
                    "%u DATA%s",
                    *line,
                    typable ? " " : "" );
  }
  else
  {
    *pos += emit(fp, cse, typable ? ", " : ",");
  }

  *pos += emit(fp, cse, "%u", d);

  check_pos(pos, machine);
}

static void
emit_line(FILE *fp,
          unsigned int *pos,
                   int typable,
          unsigned int *first_line,
          unsigned int *line_count,
          unsigned int *line,
          unsigned int *step,
          enum machine_type machine,
          enum case_type cse,
          const char *fmt, ...)
{
  int bytes = 0;
  va_list ap;

  (void) typable;

  inc_line_number(first_line, line, step);

  *pos = emit(fp, cse, "%u ", *line);

  va_start(ap, fmt);
  bytes = vemit(fp, cse, fmt, ap);
  va_end(ap);

  check_rval(bytes);

  *pos += (unsigned int) bytes;
  check_pos(pos, machine);

  (void) emit(fp, cse, "\n");
  inc_line_count(line_count);
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

#if (HIGHEST_RAM_ADDRESS != USHRT_MAX)
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
get_m_arg(char **pargv[], const char *shrt, const char *lng,
          enum machine_type *machine)
{
  int matched = 0;
  char *name = NULL;

  puts((*pargv)[0]);

  if ((matched = get_str_arg(pargv, shrt, lng, &name)))
  {
    if (*machine != default_machine)
      fail("You can only set the target architecture once");

         if (strcmp(name, "coco") == 0)      *machine = coco;
    else if (strcmp(name, "c64") == 0)       *machine = c64;
    else fail("Unknown machine %s", (*pargv)[0]);
  }

  return matched;
}

static int
get_f_arg(char **pargv[], const char *shrt, const char *lng,
          enum format_type *fmt)
{
  int matched = 0;
  char *opt = NULL;

  if ((matched = get_str_arg(pargv, shrt, lng, &opt)))
  {
    if (*fmt != default_format)
      fail("You can only set the file format once");

         if (strcmp(opt, "binary") == 0)     *fmt = binary;
    else if (strcmp(opt, "coco") == 0)       *fmt = format_coco;
    else if (strcmp(opt, "dragon") == 0)     *fmt = dragon;
    else if (strcmp(opt, "prg") == 0)        *fmt = prg;
    else fail("Unknown file format %s", (*pargv)[0]);
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
    if (*cse != default_case)
      fail("You can only set the output case once");

         if (strcmp(opt, "upper") == 0)      *cse = upper;
    else if (strcmp(opt, "lower") == 0)      *cse = lower;
    else if (strcmp(opt, "mixed") == 0)      *cse = mixed;
    else fail("Unknown case %s", (*pargv)[0]);
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
  puts("  -k  --checksum  Checksum included in each line of DATA");
  puts("  -x  --extbas    Assume Extended Color BASIC");
  puts("  -c  --case      Output case (upper/lower/mixed)");
  puts("  -r  --remarks   Add remarks to the program");
  puts("  -f  --format    Input file format");
  puts("  -d  --diag      Print info about the generated program");
  puts("  -h  --help      This help information");
  puts("  -?  --options   Short explanation of command line options");
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

static char *
machine_name(enum machine_type machine)
{
  char *s = NULL;

  switch(machine)
  {
    case coco:      s = "coco";      break;
    case c64:       s = "c64";       break;
    default:        fail("Internal error: Unhandled machine"
                         " in machine_name()");
  }

  return s;
}

static char *
format_name(enum format_type format)
{
  char *s = NULL;

  switch(format)
  {
    case binary:           s = "binary";    break;
    case format_coco:      s = "coco";      break;
    case dragon:           s = "dragon";    break;
    case prg:              s = "prg";       break;
    default:               fail("Internal error: Unhandled format"
                                " in format_name()");
  }

  return s;
}

static char *
case_name(enum case_type cse)
{
  char *s = NULL;

  switch(cse)
  {
    case upper:     s = "uppercase";    break;
    case lower:     s = "lowercase";    break;
    case mixed:     s = "mixed case";   break;
    default:        fail("Internal error: Unhandled case in case_name()");
  }

  return s;
}

static void
options(void)
{
  print_version();
  puts("");
  printf("Default target architecture : %s\n", machine_name(DEFAULT_MACHINE));
  printf("Default input format        : %s\n", format_name(DEFAULT_FORMAT));
  printf("Default output case is      : %s\n", case_name(DEFAULT_CASE));

    puts("");
    puts("Available command line switches are :");
    puts("");
    puts("  -m  --machine   Target machine");
    puts("                   (coco, c64)");
    puts("");
    puts("  -f  --format    Input file format");
    puts("                   (binary, coco, dragon, prg)");
    puts("");
    puts("  -c  --case      Output case");
    puts("                   (upper, lower, mixed)");
    puts("");

  printf("Default output filename     : %s\n",
                                        DEFAULT_OUTPUT_FILENAME);
  printf("Default output filename     : %s (with --machine c64 --case lower)\n",
                                        C64_LC_DEFAULT_OUTPUT_FILENAME);
    puts("To use the output with VICE or petcat, use"
                                        " --machine c64 --case lower");
    exit(EXIT_SUCCESS);
}

static void
info(void)
{
  print_version();
  puts("");
  puts("BASICloader reads in a machine language binary, and then constructs");
  puts("a BASIC program that will run on the selected target architecture.");
  puts("");
  puts("The BASIC program so produced contains DATA statements that represent");
  puts("the machine language given to BASICloader when the BASIC program");
  puts("was generated.");
  puts("");
  puts("When run on the target architecture, the BASIC program will poke");
  puts("that machine language into memory, and execute it.");
  puts("");
  puts("BASICloader therefore generates programs similar to the type-in programs");
  puts("printed in 1980s computer magazines.");
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
  enum machine_type machine = default_machine;
  enum format_type format = default_format;
  enum case_type cse = default_case;
  FILE *fp = NULL;
  FILE *ofp = NULL;
  char *fname = NULL;
  char *ofname = NULL;
  unsigned int first_line = 1;
  unsigned int line = 0;
  unsigned int line_count = 0;
  unsigned int step = DEFAULT_BASIC_LINE_STEP_SIZE;
  unsigned int pos = 0;
  unsigned short int start = 0;
  unsigned short int end   = 0;
  unsigned short int exec  = 0;
  long int size = 0;
  long int osize = 0;
  int diagnostics = 0;
  int nowarn = 0;
  int typable = 0;
  int checksum = 0;
  int remarks = 0;
  int extbas = 0;

#if (UCHAR_MAX < UCHAR_MAX_8_BIT)
    /* This code might be used with retro machines and compilers */
    fail("This machine cannot process 8-bit bytes");
#endif

  if (UCHAR_MAX > INT_MAX)
    fail("Internal error: Cannot safely promote unsigned char to signed int");

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
      else if (    match_arg (argv[0], "-?", "--options"))
             options();
      else if (
                   get_str_arg (&argv, "-o", "--output",   &ofname)
                || get_shrt_arg(&argv, "-s", "--start",    &start)
                || get_shrt_arg(&argv, "-e", "--exec",     &exec)
                || get_m_arg   (&argv, "-m", "--machine",  &machine)
                || get_sw_arg(argv[0], "-n", "--nowarn",   &nowarn)
                || get_sw_arg(argv[0], "-t", "--typable",  &typable)
                || get_sw_arg(argv[0], "-x", "--extbas",   &extbas)
                || get_sw_arg(argv[0], "-r", "--remarks",  &remarks)
                || get_sw_arg(argv[0], "-d", "--diag",     &diagnostics)
                || get_sw_arg(argv[0], "-k", "--checksum", &checksum)
                || get_c_arg   (&argv, "-c", "--case",     &cse)
                || get_f_arg   (&argv, "-f", "--format",   &format)
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

  if (format == default_format)
    format = DEFAULT_FORMAT;

  if (format == prg && machine != c64)
    warning("PRG file format should only be used with the c64 target");

  if (format == dragon && machine != coco)
    warning("Dragon file format should only be used with the coco target");

  if (format == format_coco && machine != coco)
    warning("Coco file format should only be used with the coco target");

  if (cse == default_case)
    cse = DEFAULT_CASE;

  if (extbas && machine != coco)
    warning("Extended Color BASIC option should only be used"
            " with the coco target");

  if (checksum)
    typable = 1;

  if (fname == NULL)
    fail("You must specify an input file");

  if (ofname == NULL)
  {
    if (machine == c64 && cse == lower)
      ofname = C64_LC_DEFAULT_OUTPUT_FILENAME;
    else
      ofname = DEFAULT_OUTPUT_FILENAME;
  }

  errno = 0;

  fp = fopen(fname, "r");

  if (fp == NULL)
    fail("Could not open file %s. Error number %d", fname, errno);

  if (fseek(fp, 0L, SEEK_END))
    fail("Could not operate on file %s. Error number %d", fname, errno);

  size = ftell(fp);

  if (size < 0)
    fail("Could not get size of file %s. Error number %d", fname, errno);

  if (size == 0)
    fail("File %s is empty", fname);

  if (format == prg && size < 3)
    fail("With PRG file format selected,\n"
         "input file %s must be at least 3 bytes long", fname);

  if (format == dragon && size < DRAGON_HEADER + 1)
    fail("With Dragon file format selected,\n"
         "input file %s must be at least %d bytes long",
                     fname, DRAGON_HEADER + 1);

  if (format == format_coco && size < 2 * COCO_AMBLE + 1)
    fail("With Coco file format selected,\n"
         "input file %s must be at least %d bytes long",
                     fname, 2 * COCO_AMBLE + 1);

  if (size > MAX_MACHINE_LANGUAGE_BINARY_SIZE)
    fail("Input file %s is too large", fname);

  if (fseek(fp, 0L, SEEK_SET) < 0)
    fail("Could not rewind file %s", fname);

  if (format == prg)
  {
    int lo = 0, hi = 0;
    unsigned short int st = 0;

    lo = fgetc(fp);

    if (lo != EOF)
      hi = fgetc(fp);

    if (lo == EOF || hi == EOF)
      fail("Couldn't read from input file");

    size -= 2;
    st = (unsigned short int)
            ((unsigned char) hi * 256 + (unsigned char) lo);

    if (st == 0x0801)
      fail("Input PRG file %s is unsuitable for use with BASICloader\n"
           "Is it a BASIC program?");

    if (start && st != start)
      fail("Input PRG file %s gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    start = st;
  }

  if (format == dragon)
  {
    unsigned char d[DRAGON_HEADER];
    int i = 0;
    unsigned short int st = 0;
    unsigned short int ex = 0;

    for (i = 0; i < DRAGON_HEADER; ++i)
    {
      int c = fgetc(fp);

      if (c == EOF)
        fail("Couldn't read from file %s. Error code %d", fname, errno);

      d[i] = (unsigned char) c;
    }

    if (d[i] != 0x55 || d[8] != 0xAA)
      fail("Input file %s doesn't appear to be a Dragon DOS file", fname);

    if (d[i] == 0x1)
      fail("Input Dragon DOS file %s appears to be a BASIC program", fname);

    if (d[i] != 0x2)
      warning("Input Dragon DOS file %s"
             " has an unknown FILETYPE\n", fname);

    size -= DRAGON_HEADER;

    if (d[4] * 256 + d[5] != size)
      fail("Input Dragon DOS file %s header"
           "gives incorrect length", fname);

    st = (unsigned short int) (d[2] * 256 + d[3]);
    ex = (unsigned short int) (d[6] * 256 + d[7]);

    if (start && st != start)
      fail("Input Dragon DOS file %s gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    if (exec && ex != exec)
      fail("Input Dragon DOS file %s gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)", ex, exec);

    start = st;
    exec  = ex;
  }

  if (format == format_coco)
  {
    unsigned short int st = 0;
    unsigned short int ex = 0;
    unsigned short int ln = 0;
    unsigned int i = 0;
    unsigned char d[COCO_AMBLE];

    for (i = 0; i < COCO_AMBLE; ++i)
    {
      int c = fgetc(fp);

      if (c == EOF)
        fail("Could not read from file %s. Error code %d", fname, errno);

      d[i] = (unsigned char) c;
    }

    if (d[0] != 0x0)
      fail("Input file %s is not properly formed as a Coco file (bad header)",
                       fname);

    ln = (unsigned short int) (d[1] * 256 + d[2]);
    st = (unsigned short int) (d[3] * 256 + d[4]);

    if (fseek(fp, -5L, SEEK_END) < 0)
      fail("Couldn't operate on file %s. Error number %d", fname, errno);

    for (i = 0; i < COCO_AMBLE; ++i)
    {
      int c = fgetc(fp);

      if (c == EOF)
        fail("Could not read from file %s. Error code %d", fname, errno);

      d[i] = (unsigned char) c;
    }

    if (d[0] == 0x0)
      fail("Input file %s is segmented, and cannot be used", fname);

    if (d[0] != 0xff ||
        d[1] != 0x0  ||
        d[2] != 0x0)
      fail("Input file %s is not properly formed as a Coco file (bad tail)",
                       fname);

    ex = (unsigned short int) (d[3] * 256 + d[4]);

    if (ln + 2 * COCO_AMBLE != size)
      fail("Input file %s header length (%u) does not match file size (%u)",
                       fname, ln + 2 * COCO_AMBLE, size);

    if (start && st != start)
      fail("Input Coco file %s gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    if (exec && ex != exec)
      fail("Input Coco file %s gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)", ex, exec);

    if (fseek(fp, 5L, SEEK_SET) < 0)
      fail("Couldn't operate on file %s. Error number %d", fname, errno);

    start = st;
    exec = ex;
    size -= 2 * COCO_AMBLE;
  }

  if (start == 0)
  {
    switch(machine)
    {
      case coco:
           start = COCO_DEFAULT_START_ADDRESS;
           break;

      case c64:
           start = C64_DEFAULT_START_ADDRESS;
           break;

      default:
           fail("Internal error: Unhandled machine in start switch");
    }
  }

  if (exec == 0)
    exec = start;

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

  if (machine == coco && !nowarn)
  {
         if (end > HIGHEST_32K_ADDRESS)
      warning("Program requires 64K of RAM");
    else if (end > HIGHEST_16K_ADDRESS)
      warning("Program requires at least 32K of RAM");
    else if (end > HIGHEST_8K_ADDRESS)
      warning("Program requires at least 16K of RAM");
    else if (end > HIGHEST_4K_ADDRESS)
      warning("Program requires at least 8K of RAM");
  }

  if (exec < start || exec > end)
  {
    fail("The exec location given is not within the start and"
         " end of the binary blob that will be loaded into memory.");
  }

  line = (typable) ? TYPABLE_START_LINE_NUMBER :
                     DEFAULT_START_LINE_NUMBER;

  step = (typable) ? TYPABLE_BASIC_LINE_STEP_SIZE :
                     DEFAULT_BASIC_LINE_STEP_SIZE;

  if (machine == coco && extbas)
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              (typable) ?
               "CLEAR 200, %d" : "CLEAR200,%d", start - 1);

  if (machine == c64)
        emit_line(ofp, &pos, typable, &first_line, &line_count,
                  &line, &step, machine, cse,
                  (typable) ?
                  "POKE 55, %d:POKE 56, %d" :
                  "POKE55,%d:POKE56,%d", start%256, start/256);

  if (remarks)
  {
    char s[SCRATCH_SIZE];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int use_time = strftime(s, sizeof s, "%d %B %Y", tm) > 0;

    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "REM   THIS PROGRAM WAS");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "REM GENERATED BY BASICLOADER");

    if (use_time)
      emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "REM   ON %-15s", s);

    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "REM SEE GITHUB.COM/");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "REM      RICHARDCAVELL");
  }

  if (!typable)
  {
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "FORP=%dTO%d:READA:POKEP,A", start, end);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "IFA<>PEEK(P)THENGOTO%d",line+3*step);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "NEXT:%s%d:END", machine == coco ? "EXEC" : "SYS", exec);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "PRINT\"ERROR!\":END");
  }

  if (typable && !checksum)
  {
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "FOR P=%d TO %d", start, end);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "READ A");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "POKE P,A");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "IF A<>PEEK(P) THEN GOTO %d",line+5*step);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "NEXT P");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "%s %d", machine == coco ? "EXEC" : "SYS", exec);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "END");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "PRINT \"ERROR!\"");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "END");
  }

  if (checksum)
  {
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "P = %u", start);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "Q = %u", end);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "READ L, CS");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "C = 0");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "J = Q - P");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "IF J > 15 THEN J = 15");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "FOR I = 0 TO J");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "READ A");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "POKE P,A");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "IF A<>PEEK(P) THEN GOTO %u", line + 12 * step);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "C = C + A");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "P = P + 1");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "NEXT I");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "IF C <> CS THEN GOTO %u", line + 5 * step);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "IF P < Q THEN GOTO %u", line - 11 * step);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "%s %u", machine == c64 ? "SYS" : "EXEC", exec);
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "END");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "PRINT \"THERE IS AN ERROR\"");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "PRINT \"ON LINE \";L");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "END");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "PRINT \"ERROR WHILE POKING MEMORY\"");
    emit_line(ofp, &pos, typable, &first_line, &line_count, &line, &step, machine, cse,
              "END");
  }

  if (!checksum)
  {
    int c = 0;

    while ((c = fgetc(fp)) != EOF)
    {
#if (UCHAR_MAX != UCHAR_MAX_8_BIT)
      if ((unsigned char) c > UCHAR_MAX_8_BIT)
        fail("Input file contains a value that's too high"
             " for an 8-bit machine");
#endif

      emit_datum(ofp, (unsigned char) c, machine, cse, typable,
                 &first_line, &line_count, &line, &step, &pos);
    }
  }
  else
  {
    unsigned char d[CS_DATA_PER_LINE];
    int c = 0;
    unsigned int i = 0;
    unsigned int cs = 0;

    for (cs = 0, i = 0; !feof(fp);)
    {
      while (i < CS_DATA_PER_LINE && !feof(fp))
      {
        c = fgetc(fp);

#if (UCHAR_MAX != UCHAR_MAX_8_BIT)
        if ((unsigned char) c > UCHAR_MAX_8_BIT)
          fail("Input file contains a value that's too high"
               " for an 8-bit machine");
#endif

        if (c != EOF)
        {
          d[i] = (unsigned char) c;
          cs += d[i];
          ++i;
        }
      }

      if (i > 0)
      {
        unsigned int j = 0;

        emit_datum(ofp, line + step, machine, cse, typable,
                   &first_line, &line_count, &line, &step, &pos);

        emit_datum(ofp, cs, machine, cse, typable,
                   &first_line, &line_count, &line, &step, &pos);

        for (j = 0; j < i; ++j)
          emit_datum(ofp, d[j], machine, cse, typable,
                     &first_line, &line_count, &line, &step, &pos);
      }
    }
  }


  if (pos > 0)
  {
    (void) emit(ofp, cse, "\n");
    inc_line_count(&line_count);
  }

  osize = ftell(ofp);

  if (osize < 0)
    fail("Error while measuring size of output file");

  if (osize > MAX_BASIC_PROG_SIZE)
    fail("Generated BASIC program size exceeds internal limit");

  if (fclose(ofp))
    fail("Couldn't close file %s", ofname);

  printf("BASIC program has been generated -> %s\n", ofname);

  if (diagnostics)
  {
    printf("Output is for the %s target architecture%s\n",
                               machine_name(machine),
                               extbas ? " (with Extended BASIC)" : "");

    printf("The program is %s, %s form%s"
           " and with%s program comments\n",
                               case_name(cse),
                               typable ? "typable" : "compact",
                               checksum ? " with checksumming" : "",
                               remarks ? "" : "out");
    printf("  Start location : 0x%x (%u)\n", start, start);
    printf("  Exec location  : 0x%x (%u)\n", exec, exec);
    printf("  End location   : 0x%x (%u)\n", end, end);

    if (size > 15)
    printf("  Blob size      : 0x%lx (%lu) bytes\n", size, size);
    else
    printf("  Blob size      : %lu bytes\n", size);
    printf("  BASIC program  : %u lines (%lu characters)\n",
                               line_count, osize);
  }

  if (fclose(fp))
    fail("Couldn't close file %s", fname);

  return EXIT_SUCCESS;
}
