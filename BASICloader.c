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
  NO_MACHINE_CHOSEN = 0,
  COCO,
  DRAGON,
  C64
};

enum input_file_format_type
{
  NO_FORMAT_CHOSEN = 0,
  BINARY,
  RSDOS,
  DRAGONDOS,
  PRG
};

enum output_case_type
{
  NO_CASE_CHOSEN = 0,
  UPPER,
  LOWER,
  MIXED
};

        /* User-modifiable values */

#define           DEFAULT_MACHINE            COCO
#define           DEFAULT_FORMAT             BINARY
#define           DEFAULT_CASE               UPPER

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
#define    DRAGON_DEFAULT_START_ADDRESS      0x3e00

#define             SCRATCH_SIZE             300

        /* End of user-modifiable values */

#define MAX_MACHINE_LANGUAGE_BINARY_SIZE 65536

#define HIGHEST_RAM_ADDRESS 0xffff
#define HIGHEST_32K_ADDRESS 0x7fff
#define HIGHEST_16K_ADDRESS 0x3fff
#define HIGHEST_8K_ADDRESS  0x1fff
#define HIGHEST_4K_ADDRESS  0x0fff

#define C64_MAX_BASIC_LINE_LENGTH    79
#define COCO_MAX_BASIC_LINE_LENGTH   249
#define DRAGON_MAX_BASIC_LINE_LENGTH 249

#define MIN_BASIC_LINE_NUMBER 0
#define MAX_BASIC_LINE_NUMBER 63999

#define UCHAR_MAX_8_BIT 255

#define COCO_AMBLE 5
#define DRAGON_HEADER 9
#define PRG_HEADER 2

    /* In maintaining this code, remember that this code is intended to
       be capable of being used with retro machines and compilers. Many
       of these machines and compilers are really limited, and predate
       *all* C standards. */

typedef unsigned short int bool_type;
typedef unsigned short int line_number_type;
typedef unsigned short int step_type;
typedef unsigned short int pos_type;
typedef unsigned short int line_counter_type;
typedef unsigned short int loc_type;

#define LINE_NUMBER_TYPE_MAX   ((line_number_type) -1)
#define POS_TYPE_MAX           ((pos_type) -1)
#define LINE_COUNTER_TYPE_MAX  ((line_counter_type) -1)
#define LOC_MAX                ((loc_type) -1)

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

  (void) fprintf(stderr, "\nPlease report this to Richard Cavell\n"
                         "at richardcavell@mail.com\n");

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

static unsigned char
xgetc(FILE *input_file, const char *input_filename)
{
  int c = fgetc(input_file);

#if (UCHAR_MAX != UCHAR_MAX_8_BIT)
  if (c > UCHAR_MAX_8_BIT)
    fail("Input file contains a value that's too high"
         " for an 8-bit machine");
#endif

  if (c == EOF)
    fail("Unexpected end of file while reading file \"%s\". Error code %d", input_filename, errno);

  return (unsigned char) c;
}

static void
fill_array(unsigned char arr[], unsigned int size, FILE *input_file, const char *input_filename)
{
  unsigned short int i = 0;

  for (i = 0; i < size; ++i)
    arr[i] = xgetc(input_file, input_filename);
}

static void
check_line_number(line_number_type line_number)
{
#if (MIN_BASIC_LINE_NUMBER > 0)
  if (line_number < MIN_BASIC_LINE_NUMBER)
    internal_error("Line number is below minimum");
#endif

#if (MAX_BASIC_LINE_NUMBER != LINE_NUMBER_TYPE_MAX)
  if (line_number > MAX_BASIC_LINE_NUMBER)
    fail("The BASIC line numbers have become too large");
#endif
}

static void
inc_line_number(bool_type         *line_incrementing_has_started,
                line_number_type  *line_number,
                step_type         *step)
{
  if(*line_incrementing_has_started == 1)
  {
    line_number_type old_line = *line_number;

    *line_number = (line_number_type) (*line_number + *step);

    if (*line_number < old_line
        || *line_number + *step > LINE_NUMBER_TYPE_MAX)
        internal_error("Line number overflow");
  }

  *line_incrementing_has_started = 1;

  check_line_number(*line_number);
}

static void
inc_line_count(line_counter_type *line_count)
{
  if (*line_count == LINE_COUNTER_TYPE_MAX)
    internal_error("Line count is about to overflow");

  ++*line_count;

#if (MAX_BASIC_LINES != LINE_COUNTER_TYPE_MAX)
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
    case COCO:
      max_basic_line_length = COCO_MAX_BASIC_LINE_LENGTH;
      break;

    case DRAGON:
      max_basic_line_length = DRAGON_MAX_BASIC_LINE_LENGTH;
      break;

    case C64:
      max_basic_line_length = C64_MAX_BASIC_LINE_LENGTH;
      break;

    default:
      internal_error("Unhandled machine type in check_pos()");
  }

  if (*pos > max_basic_line_length)
    internal_error("BASIC maximum line length exceeded");
}

static void
process_output_text(char                   *output_text,
                    enum machine_type      machine,
                    enum output_case_type  output_case,
                    line_counter_type      *line_count,
                    pos_type               *pos)
{
  while (*output_text != '\0')
  {
    switch (output_case)
    {
      case UPPER:  *output_text = (char) toupper(*output_text);  break;
      case LOWER:  *output_text = (char) tolower(*output_text);  break;
      default:                                                   break;
    }

    if (*output_text == '\n')
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

    ++output_text;
  }
}

enum vemit_status
{
  VE_SUCCESS = 0,
  SCRATCH_FAIL,
  EMIT_FAIL,
  FTELL_FAIL,
  TOO_LARGE
};

static void
check_status(enum vemit_status status)
{
  switch(status)
  {
    case VE_SUCCESS:
      break;

    case SCRATCH_FAIL:
      fail("Couldn't write to scratch area");
      break;

    case EMIT_FAIL:
      fail("Couldn't write to output file. Error number %d", errno);
      break;

    case FTELL_FAIL:
      fail("Couldn't operate on output file. Error number %d", errno);
      break;

    case TOO_LARGE:
      fail("Generated BASIC program is too large");
      break;

    default:
      internal_error("Unhandled return value in check_vt()");
      break;
  }
}

static enum vemit_status
vemit(FILE                   *output_file,
      enum machine_type      machine,
      enum output_case_type  output_case,
      line_counter_type      *line_count,
      pos_type               *pos,
      const char             *fmt,
      va_list                ap)
{
  char      output_text[SCRATCH_SIZE];
  long int  output_file_size;
  int       vsp_rval = vsprintf(output_text, fmt, ap);

  if (vsp_rval < 0)
    return SCRATCH_FAIL;

  if (vsp_rval >= SCRATCH_SIZE)
    internal_error("Scratch buffer overrun");

  process_output_text(output_text, machine, output_case, line_count, pos);

  if (fprintf(output_file, "%s", output_text) < 0)
    return EMIT_FAIL;

  output_file_size = ftell(output_file);

  if (output_file_size < 0)
    return FTELL_FAIL;

  if (output_file_size > MAX_BASIC_PROG_SIZE)
    return TOO_LARGE;

  return VE_SUCCESS;
}

static void
emit(FILE                   *output_file,
     enum machine_type      machine,
     enum output_case_type  output_case,
     line_counter_type      *line_count,
     pos_type               *pos,
     const char             *fmt,
     ...)
{
  enum vemit_status status = VE_SUCCESS;
  va_list ap;

  va_start(ap, fmt);
  status = vemit(output_file, machine, output_case, line_count, pos, fmt, ap);
  va_end(ap);

  check_status(status);
}

static void
emit_datum(FILE                   *output_file,
           enum machine_type      machine,
           enum output_case_type  output_case,
           bool_type              typable,
           bool_type              *line_incrementing_has_started,
           line_counter_type      *line_count,
           line_number_type       *line_number,
           step_type              *step,
           pos_type               *pos,
           unsigned long int      datum)
{
  char scratch[SCRATCH_SIZE];
  int spr_rval = sprintf(scratch, "%s%lu", typable ? ", " : ",", datum);

  if (spr_rval < 0)
    fail("Couldn't write to scratch area in emit_datum()");

  if (spr_rval >= SCRATCH_SIZE)
    internal_error("Scratch buffer overrun");

  if (*pos + strlen(scratch) > BASIC_LINE_WRAP_POS)
    emit(output_file, machine, output_case, line_count, pos, "\n");

  if (*pos == 0)
  {
    inc_line_number(line_incrementing_has_started, line_number, step);
    emit(output_file,
         machine,
         output_case,
         line_count,
         pos,
         "%hu DATA%s%lu",
         *line_number,
         typable ? " " : "",
         datum);

    if (*pos > BASIC_LINE_WRAP_POS)
      internal_error("BASIC_LINE_WRAP_POS is set too low");
  }
  else
  {
    emit(output_file, machine, output_case, line_count, pos, "%s", scratch);

    if (*pos > BASIC_LINE_WRAP_POS)
      internal_error("BASIC_LINE_WRAP_POS was not avoided");
  }
}

static void
emit_line(FILE                   *output_file,
          enum machine_type      machine,
          enum output_case_type  output_case,
          bool_type              typable,
          bool_type              *line_incrementing_has_started,
          line_counter_type      *line_count,
          line_number_type       *line_number,
          step_type              *step,
          pos_type               *pos,
          const char             *fmt, ...)
{
  va_list ap;
  enum vemit_status status;

  (void) typable;

  if (*pos != 0)
    internal_error("Line emission did not start at position zero");

  inc_line_number(line_incrementing_has_started, line_number, step);

  emit(output_file, machine, output_case, line_count, pos, "%hu ", *line_number);

  va_start(ap, fmt);
  status = vemit(output_file, machine, output_case, line_count, pos, fmt, ap);
  va_end(ap);

  check_status(status);

  emit(output_file, machine, output_case, line_count, pos, "\n");
}

static bool_type
arg_match(const char *arg, const char *shrt, const char *lng)
{
  return (   strcmp(arg, shrt)==0
          || strcmp(arg,  lng)==0 );
}

static bool_type
match_string_arg(char **pargv[], const char *shrt, const char *lng,
            const char **s)
{
  bool_type matched = arg_match((*pargv)[0], shrt, lng);

  if (matched == 1)
  {
    if (*s != NULL)
      fail("You can only set option %s once", (*pargv)[0]);

    if ((*pargv)[1] == NULL)
      fail("You must supply a string argument to %s", (*pargv)[0]);

    *s = (*pargv)[1];

    ++(*pargv);
  }

  return matched;
}

static unsigned short int
get_ushort(const char *text, bool_type *ok)
{
  char *endptr = NULL;
  unsigned long int l = 0;
  int base = 0;

  errno = 0;

  if (text != NULL && text[0] == '$')
  {
    base = 16;
    ++text;
  }

  if (text != NULL)
    l = strtoul(text, &endptr, base);

  *ok = ( text != NULL
          && *text != '\0'
          && endptr != NULL
          && (*endptr=='\0')
          && (errno == 0)
          && strtol(text,NULL,base) >= 0
          && (l <= USHRT_MAX) );

  return (unsigned short int) l;
}

static bool_type
match_loc_type_arg(char **pargv[], const char *shrt, const char *lng,
             unsigned short int *ps, bool_type *set)
{
  bool_type matched = arg_match((*pargv)[0], shrt, lng);

  if (matched == 1)
  {
    bool_type ok = 0;

    if (*set != 0)
      fail("Option %s can only be set once", (*pargv)[0]);

    *ps = get_ushort((*pargv)[1], &ok);

    if (ok == 0)
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
match_switch_arg(const char *arg, const char *shrt, const char *lng,
           bool_type *sw)
{
  bool_type matched = arg_match(arg, shrt, lng);

  if (matched == 1)
  {
    if (*sw == 1)
      fail("Option %s has already been set", arg);

    *sw = 1;
  }

  return matched;
}

static bool_type
match_machine_arg(char **pargv[], const char *shrt, const char *lng,
                  enum machine_type *machine)
{
  const char *name = NULL;
  bool_type matched = match_string_arg(pargv, shrt, lng, &name);

  if (matched == 1)
  {
    if (*machine != NO_MACHINE_CHOSEN)
      fail("You can only set the target architecture once");

         if (strcmp(name, "coco") == 0)   *machine = COCO;
    else if (strcmp(name, "dragon") == 0) *machine = DRAGON;
    else if (strcmp(name, "c64") == 0)    *machine = C64;
    else fail("Unknown machine \"%s\"", (*pargv)[0]);
  }

  return matched;
}

static bool_type
match_format_arg(char **pargv[], const char *shrt, const char *lng,
                 enum input_file_format_type *fmt)
{
  const char *opt = NULL;
  bool_type matched =match_string_arg(pargv, shrt, lng, &opt);

  if (matched == 1)
  {
    if (*fmt != NO_FORMAT_CHOSEN)
      fail("You can only set the file format once");

         if (strcmp(opt, "binary") == 0)  *fmt = BINARY;
    else if (strcmp(opt, "rsdos") == 0)   *fmt = RSDOS;
    else if (strcmp(opt, "dragon") == 0)  *fmt = DRAGONDOS;
    else if (strcmp(opt, "prg") == 0)     *fmt = PRG;
    else fail("Unknown file format \"%s\"", (*pargv)[0]);
  }

  return matched;
}

static bool_type
match_case_arg(char **pargv[], const char *shrt, const char *lng,
               enum output_case_type *output_case)
{
  const char *opt = NULL;
  bool_type matched = match_string_arg(pargv, shrt, lng, &opt);

  if (matched == 1)
  {
    if (*output_case != NO_CASE_CHOSEN)
      fail("You can only set the output case once");

         if (strcmp(opt, "upper") == 0)  *output_case = UPPER;
    else if (strcmp(opt, "lower") == 0)  *output_case = LOWER;
    else if (strcmp(opt, "mixed") == 0)  *output_case = MIXED;
    else fail("Unknown case \"%s\"", (*pargv)[0]);
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
  puts("  -o  --output    Output filename");
  puts("  -m  --machine   Target machine (coco/dragon/c64)");
  puts("  -f  --format    Input file format (binary/rsdos/dragon/prg)");
  puts("  -c  --case      Output case (upper/lower)");
  puts("  -r  --remarks   Add remarks to output program");
  puts("  -t  --typable   Unpack the program and use spaces");
  puts("  -y  --verify    Verify the success of each POKE");
  puts("  -k  --checksum  Calculate and verify checksums");
  puts("  -x  --extbas    Assume Extended Color BASIC (coco only)");
  puts("  -s  --start     Start memory location");
  puts("  -e  --exec      Exec memory location");
  puts("  -p  --print     Print some diagnostic info");
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
machine_to_text(enum machine_type machine)
{
  const char *name = NULL;

  switch(machine)
  {
    case COCO:    name = "coco";    break;
    case DRAGON:  name = "dragon";  break;
    case C64:     name = "c64";     break;
    default:      internal_error("Unhandled machine in machine_to_text()");
  }

  return name;
}

static const char *
format_to_text(enum input_file_format_type format)
{
  const char *name = NULL;

  switch(format)
  {
    case BINARY:     name = "binary";  break;
    case RSDOS:      name = "rsdos";   break;
    case DRAGONDOS:  name = "dragon";  break;
    case PRG:        name = "prg";     break;
    default:         internal_error("Unhandled format in format_to_text()");
  }

  return name;
}

static const char *
case_to_text(enum output_case_type output_case)
{
  const char *name = NULL;

  switch(output_case)
  {
    case UPPER:  name = "uppercase";   break;
    case LOWER:  name = "lowercase";   break;
    case MIXED:  name = "mixed case";  break;
    default:     internal_error("Unhandled case in case_to_text()");
  }

  return name;
}

static void
defaults(void)
{
  printf("Default target architecture : %s\n",
                                        machine_to_text(DEFAULT_MACHINE));
  printf("Default input format        : %s\n", format_to_text(DEFAULT_FORMAT));
  printf("Default output is           : %s\n", case_to_text(DEFAULT_CASE));
  printf("Default output filename     : \"%s\"\n", DEFAULT_OUTPUT_FILENAME);
  printf("Default output filename     : \"%s\""
                                        " (with --machine c64 --case lower)\n",
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
  const char *input_filename  = NULL;
  FILE       *input_file      = NULL;

  const char *output_filename = NULL;
  FILE       *output_file     = NULL;

  long int  input_file_size   = 0;
  long int  output_file_size  = 0;
  long int  blob_size         = 0;
  long int  remainder         = 0;

  enum machine_type            machine            = NO_MACHINE_CHOSEN;
  enum input_file_format_type  input_file_format  = NO_FORMAT_CHOSEN;
  enum output_case_type        output_case        = NO_CASE_CHOSEN;

  bool_type extended_basic = 0;
  bool_type typable        = 0;
  bool_type verify         = 0;
  bool_type checksum       = 0;
  bool_type remarks        = 0;
  bool_type nowarn         = 0;
  bool_type print_diag     = 0;

  bool_type          line_incrementing_has_started = 0;
  line_counter_type  line_count                    = 0;
  line_number_type   line_number                   = 0;

  step_type step = DEFAULT_BASIC_LINE_STEP_SIZE;

  pos_type pos = 0;

  bool_type start_set = 0;
  loc_type  start     = 0;
  loc_type  end       = 0;
  bool_type exec_set  = 0;
  loc_type  exec      = 0;

#if (UCHAR_MAX < UCHAR_MAX_8_BIT)
    fail("This machine cannot process 8-bit bytes");
#endif

  if (UCHAR_MAX > INT_MAX)
    internal_error("Cannot safely promote unsigned char to signed int");

  if (LINE_NUMBER_TYPE_MAX > INT_MAX
     || LINE_COUNTER_TYPE_MAX > INT_MAX
     || LOC_MAX > INT_MAX)
        internal_error("printf() promotions are unsafe");

  if (LINE_NUMBER_TYPE_MAX < MAX_BASIC_LINE_NUMBER)
    fail("line_number_type does not have adequate range");

  if (POS_TYPE_MAX < COCO_MAX_BASIC_LINE_LENGTH
     || POS_TYPE_MAX < DRAGON_MAX_BASIC_LINE_LENGTH
     || POS_TYPE_MAX < C64_MAX_BASIC_LINE_LENGTH)
    fail("pos_type does not have adequate range");

  if (LINE_COUNTER_TYPE_MAX < MAX_BASIC_LINES)
    fail("line_counter_type does not have adequate range");

  if (LOC_MAX < HIGHEST_RAM_ADDRESS)
    internal_error("Cannot safely calculate RAM addresses");

  if (LONG_MAX < MAX_MACHINE_LANGUAGE_BINARY_SIZE)
    internal_error("Cannot safely measure file sizes");

  if (LONG_MAX < HIGHEST_RAM_ADDRESS)
    internal_error("Cannot safely measure length of target machine files");

  if (argc > 0)
    while (*++argv)
    {
           if (    arg_match (argv[0], "-h", "--help"))
             help();
      else if (    arg_match (argv[0], "-d", "--defaults"))
             defaults();
      else if (    arg_match (argv[0], "-i", "--info"))
             info();
      else if (    arg_match (argv[0], "-l", "--license"))
             license();
      else if (    arg_match (argv[0], "-v", "--version"))
             version();
      else if (
               match_string_arg(&argv, "-o", "--output",   &output_filename)
          || match_loc_type_arg(&argv, "-s", "--start",    &start, &start_set)
          || match_loc_type_arg(&argv, "-e", "--exec",     &exec,  &exec_set)
          || match_switch_arg(argv[0], "-n", "--nowarn",   &nowarn)
          || match_switch_arg(argv[0], "-t", "--typable",  &typable)
          || match_switch_arg(argv[0], "-y", "--verify",   &verify)
          || match_switch_arg(argv[0], "-k", "--checksum", &checksum)
          || match_switch_arg(argv[0], "-x", "--extbas",   &extended_basic)
          || match_switch_arg(argv[0], "-r", "--remarks",  &remarks)
          || match_switch_arg(argv[0], "-p", "--print",    &print_diag)
          || match_machine_arg (&argv, "-m", "--machine",  &machine)
          || match_format_arg  (&argv, "-f", "--format",   &input_file_format)
          || match_case_arg    (&argv, "-c", "--case",     &output_case)
            )
          ;
      else if (argv[0][0]=='-')
             fail("Unknown command line option %s", argv[0]);
      else
           {
             if (input_filename != NULL)
               fail("Only one input filename may be specified");

             input_filename = argv[0];
           }
    }

  if (machine == NO_MACHINE_CHOSEN)
    machine = DEFAULT_MACHINE;

  if (input_file_format == NO_FORMAT_CHOSEN)
    input_file_format = DEFAULT_FORMAT;

  if (input_file_format == PRG && machine != C64)
    fail("PRG file format should only be used with the c64 target");

  if (input_file_format == DRAGONDOS && machine != DRAGON)
    fail("Dragon file format should only be used with the dragon target");

  if (input_file_format == RSDOS && machine != COCO)
    fail("RSDOS file format should only be used with the coco target");

  if (extended_basic && machine != COCO)
    fail("Extended Color BASIC option should only be used"
         " with the coco target");

  if (output_case == NO_CASE_CHOSEN)
    output_case = DEFAULT_CASE;

  if (output_case == LOWER && machine == COCO)
    fail("Lowercase output is not useful for a Coco target");

  if (output_case == LOWER && machine == DRAGON)
    fail("Lowercase output is not useful for a Dragon target");

  if (output_case == MIXED)
    fail("There is presently no target for mixed case output");

  if (output_filename == NULL)
  {
    if (machine == C64 && output_case == LOWER)
      output_filename = C64_LC_DEFAULT_OUTPUT_FILENAME;
    else
      output_filename = DEFAULT_OUTPUT_FILENAME;
  }

  if (checksum == 1)
    typable = 1;

  if (input_filename == NULL)
    fail("You must specify an input file");

  errno = 0;

  input_file = fopen(input_filename, "r");

  if (input_file == NULL)
    fail("Could not open file \"%s\". Error number %d", input_filename, errno);

  if (fseek(input_file, 0L, SEEK_END))
    fail("Could not find the end of file \"%s\". Error number %d",
                                         input_filename, errno);

  input_file_size = ftell(input_file);

  if (input_file_size < 0)
    fail("Could not get size of file \"%s\". Error number %d",
                                     input_filename, errno);

  if (input_file_size == 0)
    fail("File \"%s\" is empty", input_filename);

  if (input_file_format == PRG && input_file_size < 3)
    fail("With PRG file format selected,\n"
         "input file \"%s\" must be at least 3 bytes long", input_filename);

  if (input_file_format == DRAGONDOS && input_file_size < DRAGON_HEADER + 1)
    fail("With Dragon file format selected,\n"
         "input file \"%s\" must be at least %d bytes long",
                     input_filename, DRAGON_HEADER + 1);

  if (input_file_format == RSDOS && input_file_size < 2 * COCO_AMBLE + 1)
    fail("With RS-DOS file format selected,\n"
         "input file \"%s\" must be at least %d bytes long",
                     input_filename, 2 * COCO_AMBLE + 1);

  if (input_file_size > MAX_MACHINE_LANGUAGE_BINARY_SIZE)
    fail("Input file \"%s\" is too large", input_filename);

  if (fseek(input_file, 0L, SEEK_SET) < 0)
    fail("Could not rewind file \"%s\"", input_filename);

  if (input_file_format == BINARY)
    blob_size = input_file_size;

  if (input_file_format == PRG)
  {
    unsigned char header[PRG_HEADER];
    loc_type st = 0;

    fill_array(header, sizeof header, input_file, input_filename);

    blob_size = input_file_size - 2;

    st = (loc_type) (header[1] * 256 + header[0]);

    if (st == 0x0801)
      fail("Input PRG file \"%s\" is unsuitable for use with BASICloader\n"
           "Is it a BASIC program, or a hybrid"
           " BASIC/machine language program?");

    if (start_set == 1 && st != start)
      fail("Input PRG file \"%s\" gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    if (exec_set == 1 && st != exec)
      fail("Input PRG file \"%s\" gives a different exec address ($%x)\n"
           "to the exec address given at the command line ($%x)", st, exec);

    start = st;
    start_set = 1;

    if (fseek(input_file, (long int) PRG_HEADER, SEEK_SET) < 0)
      fail("Couldn't operate on file \"%s\". Error number %d",
                                     input_filename, errno);
  }

  if (input_file_format == DRAGONDOS)
  {
    unsigned char header[DRAGON_HEADER];
    loc_type st = 0;
    loc_type ex = 0;

    fill_array(header, sizeof header, input_file, input_filename);

    if (header[0] != 0x55 || header[8] != 0xAA)
      fail("Input file \"%s\" doesn't appear to be a Dragon DOS file",
                       input_filename);

    switch(header[1])
    {
      case 0x1:
        fail("Input Dragon DOS file \"%s\" appears to be a BASIC program",
                                      input_filename);
        break;

      case 0x2:
        break;

      case 0x3:
       fail("Input Dragon DOS file \"%s\""
            " is an unsupported DosPlus file\n", input_filename);
        break;

      default:
        fail("Input Dragon DOS file \"%s\""
             " has an unknown FILETYPE\n", input_filename);
        break;
    }

    blob_size = input_file_size - DRAGON_HEADER;

    if (header[4] * 256 + header[5] != blob_size)
      fail("The header of input Dragon DOS file \"%s\""
           " gives incorrect length", input_filename);

    st = (loc_type) (header[2] * 256 + header[3]);
    ex = (loc_type) (header[6] * 256 + header[7]);

    if (start_set == 1 && st != start)
      fail("Input Dragon DOS file \"%s\" gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    if (exec_set == 1 && ex != exec)
      fail("Input Dragon DOS file \"%s\" gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)", ex, exec);

    if (ex < st)
      fail("The exec location in the header of the Dragon DOS file \"%s\"\n"
           "($%x) is below the start location of the binary blob ($%x)",
           input_filename, exec, start);

    if (ex > st + blob_size)
      fail("The exec location in the header of the Dragon DOS file \"%s\"\n"
           "($%x) is beyond the end location of the binary blob ($%x)",
           input_filename, exec, st + blob_size);

    start = st;
    start_set = 1;

    exec  = ex;
    exec_set = 1;
  }

  if (input_file_format == RSDOS)
  {
    loc_type st = 0;
    loc_type ex = 0;
    loc_type ln = 0;
    unsigned char amble[COCO_AMBLE];

    fill_array(amble, sizeof amble, input_file, input_filename);

    if (amble[0] != 0x0)
      fail("Input file \"%s\" is not properly formed as an "
           "RS-DOS file (bad header)", input_filename);

    ln = (loc_type) (amble[1] * 256 + amble[2]);

    blob_size = input_file_size - 2 * COCO_AMBLE;

    if (ln != blob_size)
      fail("Input file \"%s\" length (%u) given in the header\n"
           "does not match measured length (%u)",
                       input_filename, ln, blob_size);

    st = (loc_type) (amble[3] * 256 + amble[4]);

    if (start_set == 1 && st != start)
      fail("Input RS-DOS file \"%s\" gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, start);

    start = st;
    start_set = 1;

    if (fseek(input_file, (long int) -COCO_AMBLE, SEEK_END) < 0)
      fail("Couldn't operate on file \"%s\". Error number %d",
                                     input_filename, errno);

    fill_array(amble, sizeof amble, input_file, input_filename);

    if (amble[0] == 0x0)
      fail("Input RS-DOS file \"%s\" is segmented, and cannot be used",
                              input_filename);

    if (amble[0] != 0xff ||
        amble[1] != 0x0  ||
        amble[2] != 0x0)
      fail("Input file \"%s\" is not properly formed as an RS-DOS file (bad tail)",
                       input_filename);

    ex = (loc_type) (amble[3] * 256 + amble[4]);

    if (exec == 1 && ex != exec)
      fail("Input RS-DOS file \"%s\" gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)",
                              input_filename, ex, exec);

    if (ex < st)
      fail("The exec location in the tail of the RS-DOS file \"%s\"\n"
           "($%x) is below the start location of the binary blob ($%x)",
           input_filename, exec, start);

    if (ex > st + blob_size)
      fail("The exec location in the tail of the RS-DOS file \"%s\"\n"
           "($%x) is beyond the end location of the binary blob ($%x)",
           input_filename, exec, st + blob_size);

    exec = ex;

    if (fseek(input_file, (long int) COCO_AMBLE, SEEK_SET) < 0)
      fail("Couldn't operate on file \"%s\". Error number %d",
                                     input_filename, errno);
  }

  if (start_set == 0)
  {
    switch(machine)
    {
      case COCO:
           start = COCO_DEFAULT_START_ADDRESS;
           break;

      case DRAGON:
           start = DRAGON_DEFAULT_START_ADDRESS;
           break;

      case C64:
           start = C64_DEFAULT_START_ADDRESS;
           break;

      default:
           internal_error("Unhandled machine in start switch");
    }

    start_set = 1;
  }

  if (exec_set == 0)
  {
    exec     = start;
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

  output_file = fopen(output_filename, "w");

  if (output_file == NULL)
    fail("Could not open file \"%s\". Error number %d", input_filename, errno);

  if (
#if (HIGHEST_RAM_ADDRESS != USHRT_MAX)
    (end > HIGHEST_RAM_ADDRESS) ||
#endif
    (start + blob_size - 1 > LOC_MAX) ||
     (end < start) )
  {
    fail("The machine language blob would overflow the 64K RAM limit");
  }

  if (machine == COCO && !nowarn)
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

  if (machine == DRAGON && !nowarn)
  {
         if (end > HIGHEST_32K_ADDRESS)
           warning("Program requires 64K of RAM");
  }

  line_number = typable ? TYPABLE_START_LINE_NUMBER :
                          DEFAULT_START_LINE_NUMBER;

#if (MIN_BASIC_LINE_NUMBER != 0)
  if (line_number < MIN_BASIC_LINE_NUMBER)
    internal_error("Starting line number is below the minimum");
#endif

  step = typable ? TYPABLE_BASIC_LINE_STEP_SIZE :
                   DEFAULT_BASIC_LINE_STEP_SIZE;

#define EMITLINEA(A)     emit_line(output_file, machine, output_case, typable,\
 &line_incrementing_has_started, &line_count, &line_number, &step, &pos,\
 A);

#define EMITLINEB(A,B)   emit_line(output_file, machine, output_case, typable,\
 &line_incrementing_has_started, &line_count, &line_number, &step, &pos,\
 A, B);

#define EMITLINEC(A,B,C) emit_line(output_file, machine, output_case, typable,\
 &line_incrementing_has_started, &line_count, &line_number, &step, &pos,\
 A, B, C);

#define EMITLINEE(A,B,C,D,E) emit_line(output_file, machine, output_case,\
typable, &line_incrementing_has_started, &line_count, &line_number, &step,\
&pos, A, B, C, D, E);

  if (machine == DRAGON || (machine == COCO && extended_basic))
    EMITLINEB(typable ? "CLEAR 200, %d" :
                        "CLEAR200,%d",
              start - 1)

  if (machine == C64)
    EMITLINEC(typable ? "POKE 55,%d:POKE 56,%d" :
                        "POKE55,%d:POKE56,%d",
              start%256, start/256)

  if (remarks == 1)
  {
    char scratch[SCRATCH_SIZE];
    time_t t = 0;
    struct tm *tm;
    bool_type use_date = 1;

    errno = 0;

    t = time(NULL);

    if (t != (time_t) -1 && errno == 0)
    {
      tm = localtime(&t);

      if (tm != NULL)
      {
        if (strftime(scratch, sizeof scratch, "%d %B %Y", tm) == 0)
        {
          use_date = 0;
          warning("Couldn't format date");
        }
      }
      else
      {
        use_date = 0;
        warning("Couldn't convert the current time to local time");
      }
    }
    else
    {
      use_date = 0;
      warning("Couldn't get the current time. Error number : %d\n", errno);
    }

    EMITLINEA("REM   This program was")
    EMITLINEA("REM generated by BASICloader")
    if (use_date == 1)
    EMITLINEB("REM   on %-15s", scratch)
    EMITLINEA("REM See github.com/")
    EMITLINEA("REM      richardcavell")
  }

  if (typable == 0 && verify == 0)
  {
    EMITLINEE("FORP=%dTO%d:READA:POKEP,A:NEXT:%s%d:END",
              start, end, (machine == COCO || machine == DRAGON) ?
                          "EXEC" : "SYS", exec)
  }

  if (typable == 0 && verify == 1)
  {
    EMITLINEC("FORP=%dTO%d:READA:POKEP,A", start, end)
    EMITLINEB("IFA<>PEEK(P)THENGOTO%d",line_number+3*step)
    EMITLINEC("NEXT:%s%d:END", (machine == COCO || machine == DRAGON) ?
                               "EXEC" : "SYS", exec)
    EMITLINEA("PRINT\"Error!\":END")
  }

  if (typable == 1 && checksum == 0 && verify == 0)
  {
    EMITLINEC("FOR P=%d TO %d", start, end)
    EMITLINEA("READ A")
    EMITLINEA("POKE P,A")
    EMITLINEA("NEXT P")
    EMITLINEC("%s %d", (machine == COCO || machine == DRAGON) ?
              "EXEC" : "SYS", exec)
    EMITLINEA("END")
  }

  if (typable == 1 && checksum == 0 && verify == 1)
  {
    EMITLINEC("FOR P=%d TO %d", start, end)
    EMITLINEA("READ A")
    EMITLINEA("POKE P,A")
    EMITLINEB("IF A<>PEEK(P) THEN GOTO %d",line_number+5*step)
    EMITLINEA("NEXT P")
    EMITLINEC("%s %d", (machine == COCO || machine == DRAGON) ?
              "EXEC" : "SYS", exec)
    EMITLINEA("END")
    EMITLINEA("PRINT \"Error!\"")
    EMITLINEA("END")
  }

  if (checksum == 1 && verify == 0)
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
    EMITLINEA("C = C + A")
    EMITLINEA("P = P + 1")
    EMITLINEA("NEXT I")
    EMITLINEB("IF C <> CS THEN GOTO %u", line_number + 5 * step)
    EMITLINEB("IF P < Q THEN GOTO %u", line_number - 11 * step)
    EMITLINEC("%s %u", machine == C64 ? "SYS" : "EXEC", exec)
    EMITLINEA("END")
    EMITLINEA("PRINT \"There is an error\"")
    EMITLINEA("PRINT \"on line\";L;\"!\"")
    EMITLINEA("END")
  }

  if (checksum == 1 && verify == 1)
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
    EMITLINEB("IF A<>PEEK(P) THEN GOTO %u", line_number + 12 * step)
    EMITLINEA("C = C + A")
    EMITLINEA("P = P + 1")
    EMITLINEA("NEXT I")
    EMITLINEB("IF C <> CS THEN GOTO %u", line_number + 5 * step)
    EMITLINEB("IF P < Q THEN GOTO %u", line_number - 11 * step)
    EMITLINEC("%s %u", machine == C64 ? "SYS" : "EXEC", exec)
    EMITLINEA("END")
    EMITLINEA("PRINT \"There is an error\"")
    EMITLINEA("PRINT \"on line\";L;\"!\"")
    EMITLINEA("END")
    EMITLINEA("PRINT \"Error while poking memory!\"")
    EMITLINEA("END")
  }

#define EMITDATUM(A) emit_datum(output_file, machine, output_case, typable,\
&line_incrementing_has_started, &line_count, &line_number, &step, &pos,\
A);

  if (checksum == 0)
  {
    long int b = blob_size;

    while (b--)
      EMITDATUM(xgetc(input_file, input_filename))
  }
  else
  {
    long int b = blob_size;

    while (b != 0)
    {
      unsigned char d[CS_DATA_PER_LINE];
      unsigned short int i = 0;
      unsigned short int j = 0;
      unsigned long int cs = 0;

      for (cs = 0, i = 0; i < CS_DATA_PER_LINE && b > 0; ++i, --b)
      {
        d[i] = xgetc(input_file, input_filename);
        cs += d[i];
      }

      EMITDATUM((unsigned long int) (line_number + step))
      EMITDATUM(cs)

      for (j = 0; j < i; ++j)
        EMITDATUM(d[j])

      if (pos > 0)
        emit(output_file, machine, output_case, &line_count, &pos, "\n");
    }
  }

  remainder = input_file_size - ftell(input_file);

  if   ((input_file_format == BINARY     && remainder != 0)
     || (input_file_format == RSDOS      && remainder != 5)
     || (input_file_format == DRAGONDOS  && remainder != 0)
     || (input_file_format == PRG        && remainder != 0))
    fail("Unexpected remaining bytes in input file \"%s\"", input_filename);

  if (fclose(input_file) != 0)
    fail("Couldn't close file \"%s\"", input_filename);

  if (pos > 0)
    emit(output_file, machine, output_case, &line_count, &pos, "\n");

  output_file_size = ftell(output_file);

  if (output_file_size < 0)
    fail("Couldn't measure output file size");

  if (output_file_size > MAX_BASIC_PROG_SIZE)
    fail("Generated program is too large");

  if (fclose(output_file) != 0)
    fail("Couldn't close file \"%s\"", output_filename);

  printf("BASIC program has been generated -> \"%s\"\n", output_filename);

  if (print_diag == 1)
  {
    printf("Output is for the %s target architecture%s\n",
                              machine_to_text(machine),
                              extended_basic ? " (with Extended BASIC)" : "");

    printf("The program is %s, %s form%s"
           " and with%s program comments\n",
                               case_to_text(output_case),
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
                               line_count, output_file_size);
  }

  return EXIT_SUCCESS;
}
