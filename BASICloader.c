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

enum machine_type
{
  default_machine = 0,
  coco,
  coco_ext,
  c64,
  c64_lc
};

#define           DEFAULT_MACHINE            coco

#define       C64_DEFAULT_OUTPUT_FILENAME    "LOADER.BAS"
#define    C64_LC_DEFAULT_OUTPUT_FILENAME    "loader.bas"
#define      COCO_DEFAULT_OUTPUT_FILENAME    "LOADER.BAS"

#define         MIN_BASIC_LINE_NUMBER        0
#define         MAX_BASIC_LINE_NUMBER        63999
#define     DEFAULT_BASIC_LINE_STEP_SIZE     1

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
  {
    *first_line = 0;
    *line = MIN_BASIC_LINE_NUMBER;
  }
  else
  {
    *line += step;
  }

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
    case coco_ext:
           max_basic_line_length = COCO_MAX_BASIC_LINE_LENGTH;
           break;

    case c64:
    case c64_lc:
           max_basic_line_length = C64_MAX_BASIC_LINE_LENGTH;
           break;

    default:
           fail("Internal error");
  }

  if (*pos > max_basic_line_length)
    fail("Internal error: BASIC maximum line length exceeded");
}

static unsigned int
emit(FILE *fp, const char *fmt, ...)
{
  int bytes = 0;
  va_list ap;

  va_start (ap, fmt);
  bytes = vfprintf(fp, fmt, ap);
  va_end(ap);

  if (bytes < 0)
    emit_fail();

  return (unsigned int) bytes;
}

static void
emit_datum(FILE *fp,
           unsigned char c,
           enum machine_type machine,
           unsigned int *first_line,
           unsigned int *line,
           unsigned int step,
           unsigned int *pos)
{
  if (*pos > BASIC_LINE_WRAP_POS)
  {
    (void) emit(fp, "\n");
    *pos = 0;
  }

  if (*pos == 0)
  {
    *pos = emit(fp, "%u %s",
                    next_line_number(first_line, line, step),
                    (machine == c64_lc) ? "data" : "DATA" );
  }
  else
  {
    *pos += emit(fp, ",");
  }

  *pos += emit(fp, "%u", c);

  check_pos(pos, machine);
}

static void
emit_line(FILE *fp,
          unsigned int *pos,
          unsigned int *first_line,
          unsigned int *line,
          unsigned int step,
          enum machine_type machine,
          const char *fmt, ...)
{
  va_list ap;
  int bytes = 0;

  *pos = emit(fp, "%d ", next_line_number(first_line, line, step));

  va_start(ap, fmt);
  bytes = vfprintf(fp, fmt, ap);
  va_end(ap);

  if (bytes < 0)
    emit_fail();

  *pos += (unsigned int) bytes;
  check_pos(pos, machine);

  (void) emit(fp, "\n");
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
get_m_arg(char **pargv[], const char *shrt, const char *lng,
          enum machine_type *machine)
{
  int matched = 0;
  char *name = NULL;

  if ((matched = get_str_arg(pargv, shrt, lng, &name)))
  {
         if (strcmp(name, "coco") == 0)      *machine = coco;
    else if (strcmp(name, "coco_ext") == 0)  *machine = coco_ext;
    else if (strcmp(name, "c64") == 0)       *machine = c64;
    else if (strcmp(name, "c64_lc") == 0)    *machine = c64_lc;
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
  puts("  -m  --machine   Target machine (coco, coco_ext, c64, c64_lc)");
  puts("  -s  --start     Start location");
  puts("  -e  --exec      Exec location");
  puts("  -w  --warnings  Warn about RAM requirements (coco/coco_ext)");
  puts("  -h  --help      This help information");
  puts("  -i  --info      What this program does");
  puts("  -l  --list      Target machine options");
  puts("  -c  --license   Your license to use this program");
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
  exit(EXIT_SUCCESS);
}

static void
list(void)
{
  printf("The default target is : ");
  switch(DEFAULT_MACHINE)
  {
    case coco:      printf("coco\n");      break;
    case coco_ext:  printf("coco_ext\n");  break;
    case c64:       printf("c64\n");       break;
    case c64_lc:    printf("c64_lc\n");    break;
    default:        fail("Internal error");
  }
    puts("Available target architectures are :");
    puts("         coco   TRS-80 Color Computer (any model)");
    puts("                or Dragon (any model)");
  printf("                Default output filename: %s\n",
                             COCO_DEFAULT_OUTPUT_FILENAME);
    puts("");
    puts("     coco_ext   TRS-80 Color Computer 1 With Extended BASIC");
    puts("                or Color Computer 2 with Extended BASIC");
    puts("                or Color Computer 3 (any version)");
    puts("                or Dragon (any model)");
  printf("                Default output filename: %s\n",
                             COCO_DEFAULT_OUTPUT_FILENAME);
    puts("");
    puts("          c64   Commodore 64 (or any compatible computer)");
    puts("                The program will use uppercase throughout");
  printf("                Default output filename: %s\n",
                              C64_DEFAULT_OUTPUT_FILENAME);
    puts("");
    puts("       c64_lc   Commodore 64 (or any compatible computer)");
    puts("                The program will use lowercase throughout");
    puts("                Use this option to work with petcat");
  printf("                Default output filename: %s\n",
                              C64_LC_DEFAULT_OUTPUT_FILENAME);
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
  int warnings = 0;
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
      else if (    match_arg (argv[0], "-l", "--list"))
             list();
      else if (    match_arg (argv[0], "-v", "--version"))
             version();
      else if (    match_arg (argv[0], "-c", "--license"))
             license();
      else if (
                   get_str_arg (&argv, "-o", "--output",   &ofname)
                || get_shrt_arg(&argv, "-s", "--start",    &start)
                || get_shrt_arg(&argv, "-e", "--exec",     &exec)
                || get_m_arg   (&argv, "-m", "--machine",  &machine)
                || get_sw_arg(argv[0], "-w", "--warnings", &warnings)
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

  if (fname == NULL)
    fail("You must specify an input file");

  if (ofname == NULL)
      switch(machine)
      {
      case coco:
      case coco_ext:
             ofname = COCO_DEFAULT_OUTPUT_FILENAME;
             break;
      case c64:
             ofname = C64_DEFAULT_OUTPUT_FILENAME;
             break;
      case c64_lc:
             ofname = C64_LC_DEFAULT_OUTPUT_FILENAME;
             break;
      default:
             fail("Internal error");
      }

  if (start == 0)
  {
         if (machine == coco || machine == coco_ext)
               start = COCO_DEFAULT_START_ADDRESS;
    else if (machine == c64 || machine == c64_lc)
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
  else if ( warnings &&
            (machine == coco || machine == coco_ext) )
  {
    if (end > HIGHEST_32K_ADDRESS)
      puts("Warning: Program requires at least 32K of RAM");
    else if (end > HIGHEST_16K_ADDRESS)
      puts("Warning: Program requires at least 16K of RAM");
    else if (end > HIGHEST_8K_ADDRESS)
      puts("Warning: Program requires at least 8K of RAM");
    else if (end > HIGHEST_4K_ADDRESS)
      puts("Warning: Program requires at least 4K of RAM");
  }

  if (exec < start || exec > end)
  {
    puts("Warning: The exec location given is not within the start and");
    puts("end of the binary blob that will be loaded into memory.");
  }

  if (machine == coco_ext)
    emit_line(ofp, &pos, &first_line, &line, step, machine,
                   "CLEAR200,%d", start - 1);

  switch(machine)
  {
    case coco:
    case coco_ext:
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "FORP=%dTO%d:READA:POKEP,A", start, end);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "IFA<>PEEK(P)THENGOTO%d",line+2*step);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "NEXT:EXEC%d:END",exec);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "PRINT\"ERROR!\":END");
           break;
    case c64:
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "FORP=%dTO%d:READA:POKEP,A", start, end);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "IFA<>PEEK(P)THENGOTO%d",line+2*step);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "NEXT:SYS%d:END",exec);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "PRINT\"ERROR!\":END");
           break;
    case c64_lc:
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "forp=%dto%d:reada:pokep,a", start, end);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "ifa<>peek(p)thengoto%d",line+2*step);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "next:sys%d:end",exec);
           emit_line(ofp, &pos, &first_line, &line, step, machine,
                     "print\"error!\":end");
           break;
    default:
           fail("Internal error");
  }

  while ((c = fgetc(fp)) != EOF)
  {
    unsigned char d = (unsigned char) c;

#if (UCHAR_MAX != UCHAR_MAX_8_BIT)
    if (d > UCHAR_MAX_8_BIT)
      fail("Input file contains a value that's too high for an 8-bit machine");
#endif

    emit_datum(ofp, d, machine,
               &first_line, &line, step, &pos);
  }

  if (pos > 0)
    (void) emit(ofp, "\n");

  if (fclose(ofp))
    fail("Couldn't close file %s", ofname);

  if (fclose(fp))
    fail("Couldn't close file %s", fname);

  return EXIT_SUCCESS;
}
