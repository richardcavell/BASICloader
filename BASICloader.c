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
  rsdos,
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
#define             BASIC_LINE_WRAP_POS      75
#define                CS_DATA_PER_LINE      10

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

#define DRAGON_HEADER 9
#define COCO_AMBLE 5

    /* In maintaining this code, remember that this code is intended to
       be capable of being used with retro machines and compilers. Many
       of these machines and compilers are really limited, and predate
       *all* C standards. */

typedef unsigned short int bool_type;
typedef unsigned short int line_type;
#define LINE_TYPE_MAX ((line_type) -1)
typedef line_type step_type;
typedef unsigned short int pos_type;
#define POS_TYPE_MAX ((pos_type) -1)
typedef unsigned short int counter_type;
#define COUNTER_MAX ((counter_type) -1)
typedef unsigned short int loc_type;
#define LOC_MAX ((loc_type) -1)

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
internal_error(const char *fmt, ...)
{
  va_list ap;

  (void) fprintf(stderr, "Internal error: ");

  va_start(ap, fmt);
  (void) vfprintf(stderr, fmt, ap);
  va_end(ap);

  fprintf(stderr, "\nPlease report this to Richard Cavell\n"
                  "at richardcavell@mail.com");

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

  if (res < 0)
    warning_fail();

  if (printf("\n") < 0)
    warning_fail();
}

static void
check_line_number(line_type *line)
{
#if (MIN_BASIC_LINE_NUMBER > 0)
  if (*line < MIN_BASIC_LINE_NUMBER)
    internal_error("Line number is below minimum");
#endif

#if (MAX_BASIC_LINE_NUMBER != LINE_TYPE_MAX)
  if (*line > MAX_BASIC_LINE_NUMBER)
    fail("The BASIC line numbers have become too large");
#endif
}

static void
inc_line_number(bool_type *line_incrementing_has_started,
                line_type *line,
                step_type *step)
{
  if(*line_incrementing_has_started)
  {
    line_type old_line = *line;

    *line = (line_type) (*line + *step);

    if (*line < old_line || *line + *step > LINE_TYPE_MAX)
      internal_error("Line number overflow");
  }

  *line_incrementing_has_started = 1;

  check_line_number(line);
}

static void
inc_line_count(counter_type *line_count)
{
  if (*line_count == COUNTER_MAX)
    internal_error("Line count is about to overflow");

  ++*line_count;

#if (MAX_BASIC_LINES != COUNTER_MAX)
  if (*line_count > MAX_BASIC_LINES)
    fail("Line count has exceeded the set limit");
#endif
}

static void
check_pos(pos_type *pos, enum machine_type machine)
{
  pos_type max_basic_line_length = 0;

  switch(machine)
  {
    case coco:
      max_basic_line_length = COCO_MAX_BASIC_LINE_LENGTH;
      break;

    case c64:
      max_basic_line_length = C64_MAX_BASIC_LINE_LENGTH;
      break;

    default:
      internal_error("Unhandled machine type in check_pos()");
  }

  if (*pos > max_basic_line_length)
    internal_error("BASIC maximum line length exceeded");
}

static void
process_scratch(char              *s,
                enum machine_type machine,
                enum case_type    cse,
                counter_type      *line_count,
                pos_type          *pos)
{
  while (*s)
  {
    switch (cse)
    {
      case upper:  *s = (char) toupper(*s);  break;
      case lower:  *s = (char) tolower(*s);  break;
      default:                               break;
    }

    if (*s == '\n')
    {
      inc_line_count(line_count);
      *pos = 0;
    }
    else
    {
      if (*pos == POS_TYPE_MAX)
        internal_error("Line position will overflow");

      ++(*pos);
      check_pos(pos, machine);
    }

    ++s;
  }
}

enum vemit_status
{
  ve_success = 0,
  scratch_fail,
  emit_fail,
  ftell_fail,
  too_large
};

static void
check_status(enum vemit_status status)
{
  switch(status)
  {
    case ve_success:
      break;

    case scratch_fail:
      fail("Couldn't write to scratch area");
      break;

    case emit_fail:
      fail("Couldn't write to output file. Error number %d", errno);
      break;

    case ftell_fail:
      fail("Couldn't operate on output file. Error number %d", errno);
      break;

    case too_large:
      fail("Generated BASIC program is too large");
      break;

    default:
      internal_error("Unhandled return value in check_vt()");
      break;
  }
}

static enum vemit_status
vemit(FILE              *fp,
      enum machine_type machine,
      enum case_type    cse,
      counter_type      *line_count,
      pos_type          *pos,
      const char        *fmt,
      va_list           ap)
{
  char scratch[SCRATCH_SIZE];
  long int osize;
  int vsp_rval = vsprintf(scratch, fmt, ap);

  if (vsp_rval < 0)
    return scratch_fail;

  if (vsp_rval >= SCRATCH_SIZE)
    internal_error("Scratch buffer overrun");

  process_scratch(scratch, machine, cse, line_count, pos);

  if (fprintf(fp, "%s", scratch) < 0)
    return emit_fail;

  osize = ftell(fp);

  if (osize < 0)
    return ftell_fail;

  if (osize > MAX_BASIC_PROG_SIZE)
    return too_large;

  return ve_success;
}

static void
emit(FILE              *fp,
     enum machine_type machine,
     enum case_type    cse,
     counter_type      *line_count,
     pos_type          *pos,
     const char        *fmt,
     ...)
{
  enum vemit_status status = ve_success;
  va_list ap;

  va_start(ap, fmt);
  status = vemit(fp, machine, cse, line_count, pos, fmt, ap);
  va_end(ap);

  check_status(status);
}

static void
emit_datum(FILE              *fp,
           enum machine_type machine,
           enum case_type    cse,
           bool_type         typable,
           bool_type         *line_incrementing_has_started,
           counter_type      *line_count,
           line_type         *line,
           step_type         *step,
           pos_type          *pos,
           unsigned long int datum)
{
  char scratch[SCRATCH_SIZE];
  int spr_rval = sprintf(scratch, "%s%lu", typable ? ", " : ",", datum);

  if (spr_rval < 0)
    fail("Couldn't write to scratch area in emit_datum()");

  if (spr_rval >= SCRATCH_SIZE)
    internal_error("Scratch buffer overrun");

  if (*pos + strlen(scratch) > BASIC_LINE_WRAP_POS)
    emit(fp, machine, cse, line_count, pos, "\n");

  if (*pos == 0)
  {
    inc_line_number(line_incrementing_has_started, line, step);
    emit(fp,
         machine,
         cse,
         line_count,
         pos,
         "%hu DATA%s%lu",
         *line,
         typable ? " " : "",
         datum);

    if (*pos > BASIC_LINE_WRAP_POS)
      internal_error("BASIC_LINE_WRAP_POS is set too low");
  }
  else
  {
    emit(fp, machine, cse, line_count, pos, "%s", scratch);

    if (*pos > BASIC_LINE_WRAP_POS)
      internal_error("BASIC_LINE_WRAP_POS was not avoided");
  }
}

static void
emit_line(FILE              *fp,
          enum machine_type machine,
          enum case_type    cse,
          bool_type         typable,
          bool_type         *line_incrementing_has_started,
          counter_type      *line_count,
          line_type         *line,
          step_type         *step,
          pos_type          *pos,
          const char        *fmt, ...)
{
  va_list ap;
  enum vemit_status status;

  (void) typable;

  if (*pos != 0)
    internal_error("Line emission did not start at position zero");

  inc_line_number(line_incrementing_has_started, line, step);

  emit(fp, machine, cse, line_count, pos, "%hu ", *line);

  va_start(ap, fmt);
  status = vemit(fp, machine, cse, line_count, pos, fmt, ap);
  va_end(ap);

  check_status(status);

  emit(fp, machine, cse, line_count, pos, "\n");
}

static bool_type
match_arg(const char *arg, const char *shrt, const char *lng)
{
  return (   strcmp(arg, shrt)==0
          || strcmp(arg,  lng)==0 );
}

static bool_type
get_str_arg(char **pargv[], const char *shrt, const char *lng,
            const char **s)
{
  bool_type matched;

  if ((matched = match_arg((*pargv)[0], shrt, lng)))
  {
    if (*s)
      fail("You can only set option %s once", (*pargv)[0]);

    if ((*pargv)[1] == NULL)
      fail("You must supply a string argument to %s", (*pargv)[0]);

    *s = (*pargv)[1];

    ++(*pargv);
  }

  return matched;
}

static unsigned short int
get_ushort(const char *s, bool_type *ok)
{
  char *endptr = NULL;
  unsigned long int l = 0;

  errno = 0;

  if (s != NULL)
    l = strtoul(s, &endptr, 0);

  *ok = ( s != NULL
          && *s != '\0'
          && strtol(s,NULL,0) >= 0
          && endptr
          && (*endptr=='\0')
          && (errno == 0)
          && (l <= USHRT_MAX) );

  return (unsigned short int) l;
}

static bool_type
get_shrt_arg(char **pargv[], const char *shrt, const char *lng,
             unsigned short int *ps, bool_type *set)
{
  bool_type matched;

  if ((matched = match_arg((*pargv)[0], shrt, lng)))
  {
    bool_type ok = 0;

    if (*set)
      fail("Option %s can only be set once", (*pargv)[0]);

    *ps = get_ushort((*pargv)[1], &ok);

    if (!ok)
      fail("Invalid argument to %s", (*pargv)[0]);

#if (HIGHEST_RAM_ADDRESS != USHRT_MAX)
    if (*ps > HIGHEST_RAM_ADDRESS)
      fail("%s cannot be greater than %x", (*pargv)[0], HIGHEST_RAM_ADDRESS);
#endif

    ++(*pargv);

    *set = 1;
  }

  return matched;
}

static bool_type
get_sw_arg(const char *arg, const char *shrt, const char *lng,
           bool_type *sw)
{
  bool_type matched = 0;

  if ((matched = match_arg(arg, shrt, lng)))
  {
    if (*sw)
      fail("Option %s has already been set", arg);

    *sw = 1;
  }

  return matched;
}

static bool_type
get_m_arg(char **pargv[], const char *shrt, const char *lng,
          enum machine_type *machine)
{
  bool_type matched = 0;
  const char *name = NULL;

  if ((matched = get_str_arg(pargv, shrt, lng, &name)))
  {
    if (*machine != default_machine)
      fail("You can only set the target architecture once");

         if (strcmp(name, "coco") == 0)   *machine = coco;
    else if (strcmp(name, "c64") == 0)    *machine = c64;
    else fail("Unknown machine %s", (*pargv)[0]);
  }

  return matched;
}

static bool_type
get_f_arg(char **pargv[], const char *shrt, const char *lng,
          enum format_type *fmt)
{
  bool_type matched = 0;
  const char *opt = NULL;

  if ((matched = get_str_arg(pargv, shrt, lng, &opt)))
  {
    if (*fmt != default_format)
      fail("You can only set the file format once");

         if (strcmp(opt, "binary") == 0)  *fmt = binary;
    else if (strcmp(opt, "rsdos") == 0)   *fmt = rsdos;
    else if (strcmp(opt, "dragon") == 0)  *fmt = dragon;
    else if (strcmp(opt, "prg") == 0)     *fmt = prg;
    else fail("Unknown file format %s", (*pargv)[0]);
  }

  return matched;
}

static bool_type
get_c_arg(char **pargv[], const char *shrt, const char *lng,
          enum case_type *cse)
{
  bool_type matched = 0;
  const char *opt = NULL;

  if ((matched = get_str_arg(pargv, shrt, lng, &opt)))
  {
    if (*cse != default_case)
      fail("You can only set the output case once");

         if (strcmp(opt, "upper") == 0)  *cse = upper;
    else if (strcmp(opt, "lower") == 0)  *cse = lower;
    else if (strcmp(opt, "mixed") == 0)  *cse = mixed;
    else fail("Unknown case %s", (*pargv)[0]);
  }

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
help(void)
{
  print_version();
  puts("Usage: BASICloader [options] [filename]");
  puts("");
  puts("  -o  --output    Output file");
  puts("  -m  --machine   Target machine (coco/c64)");
  puts("  -f  --format    Input file format (binary/rsdos/dragon/prg)");
  puts("  -c  --case      Output case (upper/lower)");
  puts("  -r  --remarks   Add remarks to the program");
  puts("  -t  --typable   Unpacked, one instruction per line");
  puts("  -k  --checksum  Include checksums in the DATA");
  puts("  -x  --extbas    Assume Extended Color BASIC");
  puts("  -s  --start     Start location");
  puts("  -e  --exec      Exec location");
  puts("  -p  --print     Print diagnostic info about the generated program");
  puts("  -n  --nowarn    Don't warn about RAM requirements");
  puts("  -d  --defaults  Print option defaults");
  puts("  -l  --license   Your license to use these programs");
  puts("  -i  --info      Info about what this program does");
  puts("  -h  --help      This help information");
  puts("  -v  --version   Version of this program");
  puts("");
  exit(EXIT_SUCCESS);
}

static const char *
machine_name(enum machine_type machine)
{
  const char *s = NULL;

  switch(machine)
  {
    case coco:  s = "coco";  break;
    case c64:   s = "c64";   break;
    default:    internal_error("Unhandled machine in machine_name()");
  }

  return s;
}

static const char *
format_name(enum format_type format)
{
  const char *s = NULL;

  switch(format)
  {
    case binary:  s = "binary";  break;
    case rsdos:   s = "rsdos";   break;
    case dragon:  s = "dragon";  break;
    case prg:     s = "prg";     break;
    default:      internal_error("Unhandled format in format_name()");
  }

  return s;
}

static const char *
case_name(enum case_type cse)
{
  const char *s = NULL;

  switch(cse)
  {
    case upper:  s = "uppercase";   break;
    case lower:  s = "lowercase";   break;
    case mixed:  s = "mixed case";  break;
    default:     internal_error("Unhandled case in case_name()");
  }

  return s;
}

static void
defaults(void)
{
  printf("Default target architecture : %s\n", machine_name(DEFAULT_MACHINE));
  printf("Default input format        : %s\n", format_name(DEFAULT_FORMAT));
  printf("Default output case is      : %s\n", case_name(DEFAULT_CASE));
  printf("Default output filename     : %s\n",
                                        DEFAULT_OUTPUT_FILENAME);
  printf("Default output filename     : %s (with --machine c64 --case lower)\n",
                                        C64_LC_DEFAULT_OUTPUT_FILENAME);
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

static void
license(void)
{
  puts("BASICloader License");
  puts("");
  puts("(modified MIT License)");
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
  puts("");
  puts("The output of this program is licensed to you under the following license:");
  puts("");
  puts("1.  You may use the output of this program, for free, for any worthwhile");
  puts("    or moral purpose.");
  puts("2.  You should try to attribute me and the BASICloader program, where");
  puts("    that is not unreasonable.");
  puts("");
  puts("You should not allow people to assume that you wrote the BASIC code yourself.");
  exit(EXIT_SUCCESS);
}

static void
version(void)
{
  print_version();
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
  enum machine_type machine = default_machine;
  enum format_type format = default_format;
  enum case_type cse = default_case;
  const char *fname = NULL;
  const char *ofname = NULL;
  FILE *fp = NULL;
  FILE *ofp = NULL;
  bool_type    line_incrementing_has_started = 0;
  counter_type line_count = 0;
  line_type    line = 0;
  step_type    step = DEFAULT_BASIC_LINE_STEP_SIZE;
  pos_type pos = 0;
  loc_type start = 0;
  loc_type end   = 0;
  loc_type exec  = 0;
  bool_type start_set = 0;
  bool_type exec_set = 0;
  long int size = 0;
  long int blob_size = 0;
  long int osize = 0;
  long int remainder = 0;
  bool_type print_diag = 0;
  bool_type nowarn = 0;
  bool_type typable = 0;
  bool_type checksum = 0;
  bool_type remarks = 0;
  bool_type extbas = 0;

#if (UCHAR_MAX < UCHAR_MAX_8_BIT)
    fail("This machine cannot process 8-bit bytes");
#endif

  if (LINE_TYPE_MAX < MAX_BASIC_LINE_NUMBER)
    fail("line_type does not have adequate range");

  if (POS_TYPE_MAX < COCO_MAX_BASIC_LINE_LENGTH)
    fail("pos_type does not have adequate range");

  if (COUNTER_MAX < MAX_BASIC_LINES)
    fail("counter_type does not have adequate range");

  if (UCHAR_MAX > INT_MAX)
    internal_error("Cannot safely promote unsigned char to signed int");

  if (UCHAR_MAX * 256 + 256 - 1 < LOC_MAX)
    internal_error("Cannot safely calculate loc_type from two unsigned chars");

  if (LOC_MAX < HIGHEST_RAM_ADDRESS)
    internal_error("Cannot safely handle RAM addresses");

  if (LOC_MAX > INT_MAX)
    internal_error("Printf() promotions are unsafe");

  if (ULONG_MAX < MAX_MACHINE_LANGUAGE_BINARY_SIZE)
    internal_error("Cannot safely measure file sizes");

  if (LONG_MAX < HIGHEST_RAM_ADDRESS)
    internal_error("Cannot safely measure length of target machine files");

  if (argc > 0)
    while (*++argv)
    {
           if (    match_arg (argv[0], "-h", "--help"))
             help();
      else if (    match_arg (argv[0], "-d", "--defaults"))
             defaults();
      else if (    match_arg (argv[0], "-i", "--info"))
             info();
      else if (    match_arg (argv[0], "-l", "--license"))
             license();
      else if (    match_arg (argv[0], "-v", "--version"))
             version();
      else if (
                   get_str_arg (&argv, "-o", "--output",   &ofname)
                || get_shrt_arg(&argv, "-s", "--start",    &start, &start_set)
                || get_shrt_arg(&argv, "-e", "--exec",     &exec,  &exec_set)
                || get_sw_arg(argv[0], "-n", "--nowarn",   &nowarn)
                || get_sw_arg(argv[0], "-t", "--typable",  &typable)
                || get_sw_arg(argv[0], "-k", "--checksum", &checksum)
                || get_sw_arg(argv[0], "-x", "--extbas",   &extbas)
                || get_sw_arg(argv[0], "-r", "--remarks",  &remarks)
                || get_sw_arg(argv[0], "-p", "--print",    &print_diag)
                || get_m_arg   (&argv, "-m", "--machine",  &machine)
                || get_f_arg   (&argv, "-f", "--format",   &format)
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

  if (format == default_format)
    format = DEFAULT_FORMAT;

  if (format == prg && machine != c64)
    fail("PRG file format should only be used with the c64 target");

  if (format == dragon && machine != coco)
    fail("Dragon file format should only be used with the coco target");

  if (format == rsdos && machine != coco)
    fail("RSDOS file format should only be used with the coco target");

  if (extbas && machine != coco)
    fail("Extended Color BASIC option should only be used"
         " with the coco target");

  if (cse == default_case)
    cse = DEFAULT_CASE;

  if (cse == lower && machine == coco)
    fail("Lowercase output is not useful for a Coco target");

  if (cse == mixed)
    fail("There is presently no target for mixed case output");

  if (ofname == NULL)
  {
    if (machine == c64 && cse == lower)
      ofname = C64_LC_DEFAULT_OUTPUT_FILENAME;
    else
      ofname = DEFAULT_OUTPUT_FILENAME;
  }

  if (checksum)
    typable = 1;

  if (fname == NULL)
    fail("You must specify an input file");

  errno = 0;

  fp = fopen(fname, "r");

  if (fp == NULL)
    fail("Could not open file %s. Error number %d", fname, errno);

  if (fseek(fp, 0L, SEEK_END))
    fail("Could not find the end of file %s. Error number %d", fname, errno);

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

  if (format == rsdos && size < 2 * COCO_AMBLE + 1)
    fail("With RS-DOS file format selected,\n"
         "input file %s must be at least %d bytes long",
                     fname, 2 * COCO_AMBLE + 1);

  if (size > MAX_MACHINE_LANGUAGE_BINARY_SIZE)
    fail("Input file %s is too large", fname);

  if (fseek(fp, 0L, SEEK_SET) < 0)
    fail("Could not rewind file %s", fname);

  if (format == binary)
    blob_size = size;

  if (format == prg)
  {
    int lo = 0, hi = 0;
    loc_type st = 0;

    lo = fgetc(fp);

    if (lo != EOF)
      hi = fgetc(fp);

    if (lo == EOF || hi == EOF)
      fail("Couldn't read from input file");

    blob_size = size - 2;
    st = (unsigned short int)
            ((unsigned char) hi * 256 + (unsigned char) lo);

    if (st == 0x0801)
      fail("Input PRG file %s is unsuitable for use with BASICloader\n"
           "Is it a BASIC program, or a hybrid BASIC/machine language program?");

    if (start_set && st != start)
      fail("Input PRG file %s gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    if (exec_set && st != exec)
      fail("Input PRG file %s gives a different start address ($%x)\n"
           "to the exec address given at the command line ($%x)", st, exec);

    start = st;
    start_set = 1;

    if (fseek(fp, 2L, SEEK_SET) < 0)
      fail("Couldn't operate on file %s. Error number %d", fname, errno);
  }

  if (format == dragon)
  {
    unsigned char d[DRAGON_HEADER];
    unsigned short int i = 0;
    loc_type st = 0;
    loc_type ex = 0;

    for (i = 0; i < DRAGON_HEADER; ++i)
    {
      int c = fgetc(fp);

      if (c == EOF)
        fail("Couldn't read from file %s. Error code %d", fname, errno);

      d[i] = (unsigned char) c;
    }

    if (d[0] != 0x55 || d[8] != 0xAA)
      fail("Input file %s doesn't appear to be a Dragon DOS file", fname);

    if (d[1] == 0x1)
      fail("Input Dragon DOS file %s appears to be a BASIC program", fname);

    if (d[1] == 0x3)
      warning("Input Dragon DOS file %s"
             " is an unsupported DosPlus FILETYPE\n", fname);

    if (d[1] != 0x2)
      warning("Input Dragon DOS file %s"
             " has an unknown FILETYPE\n", fname);

    blob_size = size - DRAGON_HEADER;

    if (d[4] * 256 + d[5] != blob_size)
      fail("Input Dragon DOS file %s header"
           "gives incorrect length", fname);

    st = (loc_type) (d[2] * 256 + d[3]);
    ex = (loc_type) (d[6] * 256 + d[7]);

    if (start_set && st != start)
      fail("Input Dragon DOS file %s gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    if (exec_set && ex != exec)
      fail("Input Dragon DOS file %s gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)", ex, exec);

    if (ex < st)
      fail("The exec location in the header of the Dragon DOS file %s\n"
           "($%x) is below the start location of the binary blob ($%x)",
           fname, exec, start);

    if (ex > st + blob_size)
      fail("The exec location in the header of the Dragon DOS file %s\n"
           "($%x) is beyond the end location of the binary blob ($%x)",
           fname, exec, st + blob_size);

    start = st;
    start_set = 1;

    exec  = ex;
    exec_set = 1;
  }

  if (format == rsdos)
  {
    loc_type st = 0;
    loc_type ex = 0;
    long int ln = 0;
    unsigned short int i = 0;
    unsigned char d[COCO_AMBLE];

    for (i = 0; i < COCO_AMBLE; ++i)
    {
      int c = fgetc(fp);

      if (c == EOF)
        fail("Could not read from file %s. Error code %d", fname, errno);

      d[i] = (unsigned char) c;
    }

    if (d[0] != 0x0)
      fail("Input file %s is not properly formed as an RS-DOS file (bad header)",
                       fname);

    ln = ((loc_type) d[1] * 256 + d[2]);

    blob_size = size - 2 * COCO_AMBLE;

    if (ln != blob_size)
      fail("Input file %s length (%u) given in the header\n"
           "does not match measured length (%u)",
                       fname, ln, blob_size);

    st = (loc_type) (d[3] * 256 + d[4]);

    if (start_set && st != start)
      fail("Input RS-DOS file %s gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    start = st;
    start_set = 1;

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
      fail("Input RS-DOS file %s is segmented, and cannot be used", fname);

    if (d[0] != 0xff ||
        d[1] != 0x0  ||
        d[2] != 0x0)
      fail("Input file %s is not properly formed as an RS-DOS file (bad tail)",
                       fname);

    ex = (unsigned short int) (d[3] * 256 + d[4]);

    if (exec && ex != exec)
      fail("Input RS-DOS file %s gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)", fname, ex, exec);

    if (ex < st)
      fail("The exec location in the tail of the RS-DOS file %s\n"
           "($%x) is below the start location of the binary blob ($%x)",
           fname, exec, start);

    if (ex > st + blob_size)
      fail("The exec location in the tail of the RS-DOS file %s\n"
           "($%x) is beyond the end location of the binary blob ($%x)",
           fname, exec, st + blob_size);

    exec = ex;

    if (fseek(fp, 5L, SEEK_SET) < 0)
      fail("Couldn't operate on file %s. Error number %d", fname, errno);
  }

  if (start_set == 0)
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
           internal_error("Unhandled machine in start switch");
    }

    start_set = 1;
  }

  if (exec_set == 0)
  {
    exec = start;
    exec_set = 1;
  }

  if (start + blob_size - 1 > LOC_MAX)
    fail("The machine language blob would overflow the 64K RAM limit");

  end = (loc_type) (start + blob_size - 1);

  if (exec < start)
    fail("The exec location given ($%x) is below\n"
         "the start location of the binary blob ($%x)", exec, start);

  if (exec > end)
    fail("The exec location given ($%x) is beyond\n"
         "the end location of the binary blob ($%x)", exec, end);

  ofp = fopen(ofname, "w");

  if (ofp == NULL)
    fail("Could not open file %s. Error number %d", fname, errno);

  if ( /* Suppress warning about comparison always false */
#if (HIGHEST_RAM_ADDRESS != USHRT_MAX)
    (end > HIGHEST_RAM_ADDRESS) ||
#endif
    (start + blob_size - 1 > LOC_MAX) ||
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

  line = (typable) ? TYPABLE_START_LINE_NUMBER :
                     DEFAULT_START_LINE_NUMBER;

#if (MIN_BASIC_LINE_NUMBER != 0)
  if (line < MIN_BASIC_LINE_NUMBER)
    internal_error("Starting line number is below the minimum");
#endif

  step = (typable) ? TYPABLE_BASIC_LINE_STEP_SIZE :
                     DEFAULT_BASIC_LINE_STEP_SIZE;

#define EMITLINEA(A)     emit_line(ofp, machine, cse, typable,\
 &line_incrementing_has_started, &line_count, &line, &step, &pos,\
 A);

#define EMITLINEB(A,B)   emit_line(ofp, machine, cse, typable,\
 &line_incrementing_has_started, &line_count, &line, &step, &pos,\
 A, B);

#define EMITLINEC(A,B,C) emit_line(ofp, machine, cse, typable,\
 &line_incrementing_has_started, &line_count, &line, &step, &pos,\
 A, B, C);

  if (machine == coco && extbas)
    EMITLINEB(typable ? "CLEAR 200, %d" :
                        "CLEAR200,%d",
              start - 1)

  if (machine == c64)
    EMITLINEC(typable ? "POKE 55,%d:POKE 56,%d" :
                        "POKE55,%d:POKE56,%d",
              start%256, start/256)

  if (remarks)
  {
    char s[SCRATCH_SIZE];
    time_t t = 0;
    struct tm *tm;
    int use_date = 0;

    errno = 0;
    t = time(NULL);
    if (t == (time_t) -1 || errno)
      fail("Couldn't get the current time. Error number : %d\n", errno);

    tm = localtime(&t);
    if (tm == NULL)
      fail("Couldn't convert the current time to local time");

    use_date = strftime(s, sizeof s, "%d %B %Y", tm) > 0;
    if (use_date == 0)
      warning("Couldn't format date");

    EMITLINEA("REM   This program was")
    EMITLINEA("REM Generated by BASICloader")
    if (use_date)
      EMITLINEB("REM   on %-15s", s)
    EMITLINEA("REM See github.com/")
    EMITLINEA("REM      richardcavell")
  }

  if (!typable)
  {
    EMITLINEC("FORP=%dTO%d:READA:POKEP,A", start, end)
    EMITLINEB("IFA<>PEEK(P)THENGOTO%d",line+3*step)
    EMITLINEC("NEXT:%s%d:END", machine == coco ? "EXEC" : "SYS", exec)
    EMITLINEA("PRINT\"Error!\":END")
  }

  if (typable && !checksum)
  {
    EMITLINEC("FOR P=%d TO %d", start, end)
    EMITLINEA("READ A")
    EMITLINEA("POKE P,A")
    EMITLINEB("IF A<>PEEK(P) THEN GOTO %d",line+5*step)
    EMITLINEA("NEXT P")
    EMITLINEC("%s %d", machine == coco ? "EXEC" : "SYS", exec)
    EMITLINEA("END")
    EMITLINEA("PRINT \"Error!\"")
    EMITLINEA("END")
  }

  if (checksum)
  {
    EMITLINEB("P = %u", start)
    EMITLINEB("Q = %u", end)
    EMITLINEA("READ L, CS")
    EMITLINEA("C = 0")
    EMITLINEA("J = Q - P")
    EMITLINEC("IF J > %d THEN J = %d", CS_DATA_PER_LINE, CS_DATA_PER_LINE)
    EMITLINEA("FOR I = 0 TO J")
    EMITLINEA("READ A")
    EMITLINEA("POKE P,A")
    EMITLINEB("IF A<>PEEK(P) THEN GOTO %u", line + 12 * step)
    EMITLINEA("C = C + A")
    EMITLINEA("P = P + 1")
    EMITLINEA("NEXT I")
    EMITLINEB("IF C <> CS THEN GOTO %u", line + 5 * step)
    EMITLINEB("IF P < Q THEN GOTO %u", line - 11 * step)
    EMITLINEC("%s %u", machine == c64 ? "SYS" : "EXEC", exec)
    EMITLINEA("END")
    EMITLINEA("PRINT \"There is an error\"")
    EMITLINEA("PRINT \"on line \";L")
    EMITLINEA("END")
    EMITLINEA("PRINT \"Error while poking memory\"")
    EMITLINEA("END")
  }

#define EMITDATUM(A) emit_datum(ofp, machine, cse, typable,\
&line_incrementing_has_started, &line_count, &line, &step, &pos,\
A);

  if (!checksum)
  {
    int c = 0;
    long int b = blob_size;

    while (b--)
    {
      c = fgetc(fp);

      if (c == EOF)
        fail("Couldn't read from file %s", fname);

#if (UCHAR_MAX != UCHAR_MAX_8_BIT)
      if ((unsigned char) c > UCHAR_MAX_8_BIT)
        fail("Input file contains a value that's too high"
             " for an 8-bit machine");
#endif

      EMITDATUM((unsigned char) c)
    }
  }
  else
  {
    long int b = blob_size;

    while (b)
    {
      unsigned char d[CS_DATA_PER_LINE];
      unsigned short int i = 0;
      unsigned short int j = 0;
      unsigned long int cs = 0;
      unsigned long int old_cs = 0;
      int c = 0;

      for (cs = 0, i = 0; i < CS_DATA_PER_LINE && b; ++i, --b)
      {
        c = fgetc(fp);

#if (UCHAR_MAX != UCHAR_MAX_8_BIT)
        if ((unsigned char) c > UCHAR_MAX_8_BIT)
          fail("Input file contains a value that's too high"
               " for an 8-bit machine");
#endif

        if (c == EOF)
          fail("Error reading from file %s", ofname);

        d[i] = (unsigned char) c;
        old_cs = cs;
        cs += d[i];
        if (cs < old_cs)
          internal_error("Checksum overflow");
      }

      EMITDATUM((unsigned long int) (line + step))
      EMITDATUM(cs)

      for (j = 0; j < i; ++j)
        EMITDATUM(d[j])

      if (pos > 0)
        emit(ofp, machine, cse, &line_count, &pos, "\n");
    }
  }

  remainder = size - ftell(fp);

  if   ((format == binary  && remainder != 0)
     || (format == rsdos   && remainder != 5)
     || (format == dragon  && remainder != 0)
     || (format == prg     && remainder != 0))
    fail("Unexpected remaining bytes in input binary file %s", fname);

  if (fclose(fp))
    fail("Couldn't close file %s", fname);

  if (pos > 0)
    emit(ofp, machine, cse, &line_count, &pos, "\n");

  osize = ftell(ofp);

  if (osize < 0)
    fail("Couldn't measure output file size");

  if (osize > MAX_BASIC_PROG_SIZE)
    fail("Generated program is too large");

  if (fclose(ofp))
    fail("Couldn't close file %s", ofname);

  printf("BASIC program has been generated -> %s\n", ofname);

  if (print_diag)
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
    printf("  Start location : $%x (%u)\n", start, start);
    printf("  Exec location  : $%x (%u)\n", exec, exec);
    printf("  End location   : $%x (%u)\n", end, end);

    if (blob_size > 15)
    printf("  Blob size      : $%lx (%lu) bytes\n", blob_size, blob_size);
    else
    printf("  Blob size      : %lu bytes\n", blob_size);
    printf("  BASIC program  : %u lines (%lu characters)\n",
                               line_count, osize);
  }

  return EXIT_SUCCESS;
}
