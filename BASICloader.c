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

#define DEFAULT_OUTPUT_FILENAME              "LOADER.BAS"

#define MIN_BASIC_LINE_NUMBER                0
#define MAX_BASIC_LINE_NUMBER                63999
#define DEFAULT_BASIC_LINE_STEP_SIZE         1

#define COCO_MAX_BASIC_LINE_LENGTH           240
#define C64_MAX_BASIC_LINE_LENGTH            75
#define MAX_BASIC_LINE_LENGTH                COCO_MAX_BASIC_LINE_LENGTH

#define DEFAULT_START_ADDRESS                0x3f00
#define MAX_MACHINE_LANGUAGE_BINARY_SIZE     65536
#define HIGHEST_RAM_ADDRESS                  0xffff
#define HIGHEST_32K_ADDRESS                  0x7fff
#define HIGHEST_16K_ADDRESS                  0x3fff
#define HIGHEST_8K_ADDRESS                   0x1fff
#define HIGHEST_4K_ADDRESS                   0x0fff

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

static unsigned int line = MIN_BASIC_LINE_NUMBER;
static unsigned int step = DEFAULT_BASIC_LINE_STEP_SIZE;

static unsigned int
get_line(void)
{
  unsigned int old_line = line;

  line += step;

  if (line > MAX_BASIC_LINE_NUMBER || line < old_line)
    fail("Line number overflow"); /* Program is probably too long anyway*/

  return old_line;
}

static void
emit_line(FILE *fp, const char *fmt, ...)
{
  va_list ap;

  if (fprintf(fp, "%d ", get_line()) < 0)
    fail("Couldn't write to file");

  va_start(ap, fmt);
  if (vfprintf(fp, fmt, ap) < 0)
    fail("Couldn't write to file");
  va_end(ap);

  if (fprintf(fp, "\n") < 0)
    fail("Couldn't write to file");
}

/*
static void
emit_datum(FILE *fp, unsigned char c)
{
}
*/

static unsigned short int
get_short(const char *s, int *ok)
{
  char *endptr = NULL;
  unsigned long int l = 0;

  if (s == NULL)
  {
    *ok = 0;
  }
  else
  {
    errno = 0;

    l = strtoul(s, &endptr, 0);

    *ok = ( (endptr && *endptr=='\0')
            && (errno == 0)
            && (l <= USHRT_MAX) );
  }

  return (unsigned short int) l;
}

static int
match_arg(const char *arg, const char *shrt, const char *lng)
{
  return (   strcmp(arg, shrt)==0
          || strcmp(arg, lng) ==0 );
}

static int
get_shrt_arg(char **pargv[], const char *shrt, const char *lng,
        unsigned short int *ps)
{
  if (match_arg((*pargv)[0], shrt, lng))
  {
    int ok = 0;

    if (*ps)
      fail("Option %d can only be set once", (*pargv)[0]);

    *ps = get_short((*pargv)[1], &ok);

    if (!ok)
      fail("Invalid argument to %s", (*pargv)[0]);

#if (USHRT_MAX != HIGHEST_RAM_ADDRESS)
    if (*ps > HIGHEST_RAM_ADDRESS)
      fail("%s cannot be greater than %x", (*pargv)[0], HIGHEST_RAM_ADDRESS);
#endif

    ++(*pargv);

    return 1;
  }

  return 0;
}

static int
get_str_arg(char **pargv[], const char *shrt, const char *lng,
            char **s)
{
  if (match_arg((*pargv)[0], shrt, lng))
  {
    if ((*pargv)[1] == NULL)
      fail("You must supply a string argument to %s", (*pargv)[0]);

    if (*s)
      fail("You can only set option %s once", (*pargv)[0]);

    *s = (*pargv)[1];

    ++(*pargv);

    return 1;
  }

  return 0;
}

static int
get_sw_arg(const char *arg, const char *shrt, const char *lng,
           int *sw)
{
  if (match_arg(arg, shrt, lng))
  {
    *sw = 1;
    return 1;
  }

  return 0;
}

static void
help(void)
{
  puts("BASICloader (under development)");
  puts("(c) 2017 Richard Cavell");
  puts("Usage: BASICloader [options] [filename]");
  puts("-o --output    Output file");
  puts("-s --start     Start location");
  puts("-e --exec      Exec location");
  puts("-x --extended  Assume Extended BASIC");
  puts("-w --warnings  Warn about RAM requirements");
  puts("-h --help      Help");
  puts("-v --version   Version");
  puts("If no output filename is specified, the default is LOADER.BAS");
  exit(EXIT_SUCCESS);
}

static void
version(void)
{
  puts("BASICloader (under development)");
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
  FILE *fp = NULL;
  FILE *ofp = NULL;
  char *fname = NULL;
  char *ofname = NULL;
  unsigned short int start = 0;
  unsigned short int end   = 0;
  unsigned short int exec  = 0;
  long int size = 0;
  int extended = 0;
  int warnings = 0;

  (void) argc; /* Suppress compiler warning about unused variable */

  if (*argv)
    while (*++argv)
    {
           if (    match_arg (argv[0], "-h", "--help"))
             help();
      else if (    match_arg (argv[0], "-v", "--version"))
             version();
      else if (
                   get_str_arg (&argv, "-o", "--output",   &ofname)
                || get_shrt_arg(&argv, "-s", "--start",    &start)
                || get_shrt_arg(&argv, "-e", "--exec",     &exec)
                || get_sw_arg(argv[0], "-x", "--extended", &extended)
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

  if (fname == NULL)
    fail("You must specify an input file");

  if (ofname == NULL)
    ofname = DEFAULT_OUTPUT_FILENAME;

  if (start == 0)
    start = DEFAULT_START_ADDRESS;

  if (exec == 0)
    exec = start;

  errno = 0;

  fp = fopen(fname, "r");

  if (fp == NULL)
    fail("Could not open file %s. Error number %d", fname, errno);

  if (fseek(fp, 0L, SEEK_END))
  {
    fail("Could not operate on file %s. Error number %d", fname, errno);
  }

  size = ftell(fp);

  if (size < 0)
    fail("Could not operate on file %s. Error number %d", fname, errno);

  if (size > MAX_MACHINE_LANGUAGE_BINARY_SIZE)
    fail("Input file %s is too large", fname);

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
    fail("The program would overflow the 64K RAM limit");
  }
  else if (warnings && end > HIGHEST_32K_ADDRESS)
  {
    puts("Warning: Program requires at least 32K of RAM");
  }
  else if (warnings && end > HIGHEST_16K_ADDRESS)
  {
    puts("Warning: Program requires at least 16K of RAM");
  }
  else if (warnings && end > HIGHEST_8K_ADDRESS)
  {
    puts("Warning: Program requires at least 8K of RAM");
  }
  else if (warnings && end > HIGHEST_4K_ADDRESS)
  {
    puts("Warning: Program requires at least 4K of RAM");
  }

  if (extended)
    emit_line(ofp, "CLEAR200,%d", start - 1);

  emit_line(ofp, "FORP=%dTO%d:READA:POKEP,A:IFA<>PEEK(P)THEN"
            "PRINT\"ERROR!\"ELSENEXT:EXEC%d", start, end, exec);

  if (fclose(fp) != 0)
    fail("Couldn't close file %s", fname);

  if (fclose(ofp) != 0)
    fail("Couldn't close file %s", ofname);

  return EXIT_SUCCESS;
}
