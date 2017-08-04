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

enum machine_type {
  default_machine = 0,
  coco,
  cocoext,
  c64,
  c64petcat
};

#define DEFAULT_MACHINE                      coco
#define C64_DEFAULT_OUTPUT_FILENAME          "LOADER"
#define C64PETCAT_DEFAULT_OUTPUT_FILENAME    "loader.bas"
#define COCO_DEFAULT_OUTPUT_FILENAME         "LOADER.BAS"

#define MIN_BASIC_LINE_NUMBER                0
#define MAX_BASIC_LINE_NUMBER                63999
#define DEFAULT_BASIC_LINE_STEP_SIZE         1

#define C64_MAX_BASIC_LINE_LENGTH            79
#define COCO_MAX_BASIC_LINE_LENGTH           249
#define BASIC_LINE_WRAP_POS                  70

#define UCHAR_MAX_8_BIT                      255

#define C64_DEFAULT_START_ADDRESS            0x8000
#define COCO_DEFAULT_START_ADDRESS           0x3e00
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
get_line_number(void)
{
  unsigned int old_line = line;

  line += step;

  if (line > MAX_BASIC_LINE_NUMBER || line < old_line)
    fail("Line number overflow"); /* Program is probably too long anyway*/

  return old_line;
}

static void
emit_fail(void)
{
  fail("Couldn't write to output file. Error number %d", errno);
}

static unsigned int
emit(FILE *fp, const char *fmt, ...)
{
  int bytes = 0;
  va_list ap;

  va_start (ap,fmt);

  bytes = vfprintf(fp, fmt, ap);
  if (bytes < 0)
    emit_fail();

  va_end(ap);

  return (unsigned int) bytes;
}

static void
emit_datum(FILE *fp, unsigned char c, enum machine_type machine)
{
  static unsigned int length = (unsigned int) -1;
  unsigned int max_basic_line_length = 0;

  switch(machine)
  {
    case coco:
    case cocoext:
           max_basic_line_length = COCO_MAX_BASIC_LINE_LENGTH;
           break;

    case c64:
    case c64petcat:
           max_basic_line_length = C64_MAX_BASIC_LINE_LENGTH;
           break;
    default:
           fail("Internal error");
  }

  if (length == (unsigned int) -1)
  {
    length = emit(fp, "%u %s", get_line_number(),
                      (machine == c64petcat) ? "data" : "DATA" );
  }
  else if (length > BASIC_LINE_WRAP_POS)
  {
    emit(fp, "\n");
    length = emit(fp, "%u %s", get_line_number(),
                      (machine == c64petcat) ? "data" : "DATA" );
  }
  else
  {
    length += emit(fp, ",");
  }

  length += emit(fp, "%u", c);

  if (length > max_basic_line_length)
    fail("Internal error: BASIC maximum line length exceeded");
}

static void
emit_line(FILE *fp, const char *fmt, ...)
{
  va_list ap;

  (void) emit(fp, "%d ", get_line_number());

  va_start(ap, fmt);
  if (vfprintf(fp, fmt, ap) < 0)
    emit_fail();
  va_end(ap);

  (void) emit(fp, "\n");
}

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
get_m_arg(char **pargv[], const char *shrt, const char *lng,
          enum machine_type *machine)
{
  if (match_arg((*pargv)[0], shrt, lng))
  {
    if ((*pargv)[1] == NULL)
      fail("You must supply a string argument to %s", (*pargv)[0]);

    if (*machine != default_machine)
      fail("You can only set option %s once", (*pargv)[0]);

         if (strcmp((*pargv)[1], "coco") == 0)      *machine = coco;
    else if (strcmp((*pargv)[1], "cocoext") == 0)   *machine = cocoext;
    else if (strcmp((*pargv)[1], "c64") == 0)       *machine = c64;
    else if (strcmp((*pargv)[1], "c64petcat") == 0) *machine = c64petcat;
    else fail("Unknown parameter to %s", (*pargv)[0]);

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
  puts("https://github.com/richardcavell/BASICloader");
  puts("Usage: BASICloader [options] [filename]");
  puts("-o --output    Output file");
  puts("-m --machine   Target machine (coco, cocoext, c64, c64petcat)");
  puts("-s --start     Start location");
  puts("-e --exec      Exec location");
  puts("-w --warnings  Warn about RAM requirements (coco/cocoext)");
  puts("-h --help      Help");
  puts("-i --info      Info");
  puts("-v --version   Version");

  exit(EXIT_SUCCESS);
}

static void
version(void)
{
  puts("BASICloader (under development)");
  exit(EXIT_SUCCESS);
}

static void
info(void)
{
  puts("BASICloader reads in a binary file and constructs a BASIC program");
  puts("that will poke that binary file into memory and then execute it.");
  puts("");
  printf("The default machine is : ");
  switch(DEFAULT_MACHINE)
  {
    case coco:      printf("coco\n");      break;
    case cocoext:   printf("cocoext\n");   break;
    case c64:       printf("c64\n");       break;
    case c64petcat: printf("c64petcat\n"); break;
  }
  puts("");
  puts("Available target architectures are :");
  puts("");
  puts("         coco   TRS-80 Color Computer (any version)");
  puts("                or Dragon (any version)");
  puts("                Default output filename: LOADER.BAS");
  puts("");
  puts("      cocoext   TRS-80 Color Computer 1 or 2 with Extended BASIC");
  puts("                or Color Computer 3 (any version) or Dragon (any version)");
  puts("                Default output filename: LOADER.BAS");
  puts("");
  puts("          c64   Commodore 64 (or any compatible computer)");
  puts("                The program will use uppercase throughout");
  puts("                Default output filename: LOADER");
  puts("");
  puts("    c64petcat   Commodore 64 (or any compatible computer)");
  puts("                The program will use lowercase throughout");
  puts("                Default output filename: loader.bas");
  puts("");

  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
  enum machine_type machine = default_machine;
  FILE *fp = NULL;
  FILE *ofp = NULL;
  char *fname = NULL;
  char *ofname = NULL;
  unsigned short int start = 0;
  unsigned short int end   = 0;
  unsigned short int exec  = 0;
  long int size = 0;
  int warnings = 0;
  int c = 0;

  (void) argc; /* Suppress compiler warning about unused variable */

  if (*argv)
    while (*++argv)
    {
           if (    match_arg (argv[0], "-h", "--help"))
             help();
      else if (    match_arg (argv[0], "-v", "--version"))
             version();
      else if (    match_arg (argv[0], "-i", "--info"))
             info();
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
      case cocoext:
             ofname = COCO_DEFAULT_OUTPUT_FILENAME;
             break;
      case c64:
             ofname = C64_DEFAULT_OUTPUT_FILENAME;
             break;
      case c64petcat:
             ofname = C64PETCAT_DEFAULT_OUTPUT_FILENAME;
             break;
      default:
             fail("Internal error");
      }

  if (start == 0)
  {
         if (machine == coco || machine == cocoext)
               start = COCO_DEFAULT_START_ADDRESS;
    else if (machine == c64 || machine == c64petcat)
               start = C64_DEFAULT_START_ADDRESS;
  }

  if (exec == 0)
    exec = start;

  if (warnings && (exec < start || exec > end))
    puts("Warning: The exec location given is not within the start"
         " and end of the binary blob that will be loaded into memory.");

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
    fail("The program would overflow the 64K RAM limit");
  }
  else if ( warnings &&
            (machine == coco || machine == cocoext) )
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

  if (machine == cocoext)
    emit_line(ofp, "CLEAR200,%d", start - 1);

  switch(machine)
  {
    case coco:
    case cocoext:
           emit_line(ofp, "FORP=%dTO%d:READA:POKEP,A:IFA<>PEEK(P)THEN"
                   "PRINT\"ERROR!\"ELSENEXT:EXEC%d", start, end, exec);
           break;
    case c64:
           emit_line(ofp, "FORP=%dTO%d:READA:POKEP,A", start, end);
           emit_line(ofp, "IFA<>PEEK(P)THENPRINT\"ERROR!\"ELSENEXT:SYS%d",
                                                                    exec);
           break;
    case c64petcat:
           emit_line(ofp, "forp=%dto%d:reada:pokep,a", start, end);
           emit_line(ofp, "ifa<>peek(p)thenprint\"error!\"elsenext:sys%d",
                                                                    exec);
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

    emit_datum(ofp, d, machine);
  }

  (void) emit(ofp, "\n");

  if (fclose(fp) != 0)
    fail("Couldn't close file %s", fname);

  if (fclose(ofp) != 0)
    fail("Couldn't close file %s", ofname);

  return EXIT_SUCCESS;
}
