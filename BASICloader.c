/*
      BASICloader.c

      A program by Richard Cavell (c) 2017
      https://github.com/richardcavell/BASICloader

    Coding standards:

    * We strictly use C89
    * No external libraries, no POSIX, no GNU extensions
    * Everything in one file to make building easier
    * Every error that can reasonably be detected, is detected
    * This code is intended to be capable of being compiled with
      retro machines and compilers. Many of these machines and
      compilers are really limited, and predate *all* C standards.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

        /* User-modifiable values */

#define DEFAULT_TARGET_ARCHITECTURE COCO
#define DEFAULT_INPUT_FILE_FORMAT   BINARY
#define DEFAULT_OUTPUT_CASE         UPPERCASE

#define        DEFAULT_OUTPUT_FILENAME "LOADER.BAS"
#define C64_LC_DEFAULT_OUTPUT_FILENAME "loader"

#define         DEFAULT_STARTING_BASIC_LINE_NUMBER 0
#define TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER 10
#define         MAXIMUM_STARTING_BASIC_LINE_NUMBER 63000

#define         DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE 1
#define TYPABLE_DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE 10
#define         MAXIMUM_BASIC_LINE_NUMBER_STEP_SIZE 60000

#define MAXIMUM_BASIC_LINE_COUNT   1000
#define MAXIMUM_BASIC_PROGRAM_SIZE 40000

#define MAXIMUM_BASIC_LINE_LENGTH 75
#define CHECKSUMMED_DATA_PER_LINE 10

#define   COCO_DEFAULT_START_MEMORY_LOCATION 0x3e00
#define DRAGON_DEFAULT_START_MEMORY_LOCATION 0x3e00
#define    C64_DEFAULT_START_MEMORY_LOCATION 0x8000

#define OUTPUT_TEXT_BUFFER_SIZE 300

#define MAX_INPUT_FILE_SIZE 65000
#define MAX_MACHINE_LANGUAGE_BINARY_SIZE 65000

#define STDOUT_FILENAME_SUBSTITUTE "-"
#define PRINT_WARNINGS_TO_STDERR 0

        /* End of user-modifiable values */

#define HIGHEST_RAM_ADDRESS 0xffff
#define HIGHEST_64K_ADDRESS 0xffff
#define HIGHEST_32K_ADDRESS 0x7fff
#define HIGHEST_16K_ADDRESS 0x3fff
#define HIGHEST_8K_ADDRESS  0x1fff
#define HIGHEST_4K_ADDRESS  0x0fff

#define EIGHT_BIT_UCHAR_MAX 255

#define   COCO_MAX_BASIC_LINE_LENGTH 249
#define DRAGON_MAX_BASIC_LINE_LENGTH 249
#define    C64_MAX_BASIC_LINE_LENGTH  79

#define MIN_BASIC_LINE_NUMBER 0
#define MAX_BASIC_LINE_NUMBER 63999

#define MINIMUM_BASIC_LINE_NUMBER_STEP_SIZE 1

#define LINE_COUNT_BENCHMARK 100

#define TARGET_ARCHITECTURE_FILE_SIZE_MAX 65535

#define   RS_DOS_FILE_PREAMBLE_SIZE 5
#define  RS_DOS_FILE_POSTAMBLE_SIZE 5
#define DRAGON_DOS_FILE_HEADER_SIZE 9
#define        PRG_FILE_HEADER_SIZE 2

#define     BINARY_FILE_SIZE_MINIMUM 0
#define     RS_DOS_FILE_SIZE_MINIMUM (RS_DOS_FILE_PREAMBLE_SIZE + RS_DOS_FILE_POSTAMBLE_SIZE)
#define DRAGON_DOS_FILE_SIZE_MINIMUM DRAGON_DOS_FILE_HEADER_SIZE
#define        PRG_FILE_SIZE_MINIMUM PRG_FILE_HEADER_SIZE

typedef unsigned short int boolean_type;
typedef unsigned short int line_number_type;
typedef unsigned short int line_number_step_type;
typedef unsigned short int line_position_type;
typedef unsigned short int line_counter_type;
typedef unsigned short int memory_location_type;
typedef unsigned short int target_architecture_file_size_type;

#define LINE_NUMBER_TYPE_MAX                    (line_number_type)                    -1
#define LINE_NUMBER_STEP_TYPE_MAX               (line_number_step_type)               -1
#define LINE_POSITION_TYPE_MAX                  (line_position_type)                  -1
#define LINE_COUNTER_TYPE_MAX                   (line_counter_type)                   -1
#define MEMORY_LOCATION_TYPE_MAX                (memory_location_type)                -1
#define TARGET_ARCHITECTURE_FILE_SIZE_TYPE_MAX  (target_architecture_file_size_type)  -1

enum target_architecture_choice
{
    NO_TARGET_ARCHITECTURE_CHOSEN = 0,
    COCO,
    DRAGON,
    C64
};

#define   COCO_TEXT "coco"
#define DRAGON_TEXT "dragon"
#define    C64_TEXT "c64"

enum input_file_format_choice
{
    NO_INPUT_FILE_FORMAT_CHOSEN = 0,
    BINARY,
    RS_DOS,
    DRAGON_DOS,
    PRG
};

#define     BINARY_TEXT "binary"
#define     RS_DOS_TEXT "rsdos"
#define DRAGON_DOS_TEXT "dragon"
#define        PRG_TEXT "prg"

enum output_case_choice
{
    NO_OUTPUT_CASE_CHOSEN = 0,
    UPPERCASE,
    LOWERCASE,
    MIXED_CASE
};

#define  UPPERCASE_TEXT "upper"
#define  LOWERCASE_TEXT "lower"
#define MIXED_CASE_TEXT "mixed"

static void
internal_error(const char *fmt, ...)
{
    va_list ap;

    (void) fprintf(stderr, "Internal error: ");

    va_start(ap, fmt);
    (void) vfprintf(stderr, fmt, ap);
    va_end(ap);

    (void) fprintf(stderr, "\n"
                           "Please report this to Richard Cavell\n"
                           "at richardcavell@mail.com\n");

    exit(EXIT_FAILURE);
}

static void
check_type_limits(void)
{
    if (UCHAR_MAX < EIGHT_BIT_UCHAR_MAX)
        internal_error("This machine cannot process 8-bit bytes");
}

static void
check_user_defined_type_limits(void)
{
    if (LINE_NUMBER_TYPE_MAX < MAX_BASIC_LINE_NUMBER)
        internal_error("Line number type has insufficient range");

    if (LINE_POSITION_TYPE_MAX < COCO_MAX_BASIC_LINE_LENGTH   ||
        LINE_POSITION_TYPE_MAX < DRAGON_MAX_BASIC_LINE_LENGTH ||
        LINE_POSITION_TYPE_MAX < C64_MAX_BASIC_LINE_LENGTH)
        internal_error("Line position type has insufficient range");

    if (LINE_COUNTER_TYPE_MAX < LINE_COUNT_BENCHMARK)
        internal_error("Line counter type has insufficient range");

    if (MEMORY_LOCATION_TYPE_MAX < HIGHEST_RAM_ADDRESS)
        internal_error("Memory location type has insufficient range");

    if (TARGET_ARCHITECTURE_FILE_SIZE_TYPE_MAX < TARGET_ARCHITECTURE_FILE_SIZE_MAX)
        internal_error("Target architecture file size type has insufficient range");
}

static void
check_default_starting_basic_line_number_macro(void)
{
    if (DEFAULT_STARTING_BASIC_LINE_NUMBER < MIN_BASIC_LINE_NUMBER)
        internal_error("DEFAULT_STARTING_BASIC_LINE_NUMBER\n"
                       " is below the minimum possible BASIC line number");

    if (DEFAULT_STARTING_BASIC_LINE_NUMBER > MAX_BASIC_LINE_NUMBER)
        internal_error("DEFAULT_STARTING_BASIC_LINE_NUMBER\n"
                       " is above the maximum possible BASIC line number");

    if (DEFAULT_STARTING_BASIC_LINE_NUMBER > LINE_NUMBER_TYPE_MAX)
        internal_error("DEFAULT_STARTING_BASIC_LINE_NUMBER cannot be operated on internally");
}

static void
check_typable_default_starting_basic_line_number_macro(void)
{
    if (TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER < MIN_BASIC_LINE_NUMBER)
        internal_error("TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER\n"
                       " is below the minimum possible BASIC line number");

    if (TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER > MAX_BASIC_LINE_NUMBER)
        internal_error("TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER\n"
                       " is above the maximum possible BASIC line number");

    if (TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER > LINE_NUMBER_TYPE_MAX)
        internal_error("TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER cannot be operated on internally");
}

static void
check_maximum_starting_basic_line_number_macro(void)
{
    if (MAXIMUM_STARTING_BASIC_LINE_NUMBER < MIN_BASIC_LINE_NUMBER)
        internal_error("MAXIMUM_STARTING_BASIC_LINE_NUMBER\n"
                       " is below the minimum possible BASIC line number");

    if (MAXIMUM_STARTING_BASIC_LINE_NUMBER > MAX_BASIC_LINE_NUMBER)
        internal_error("MAXIMUM_STARTING_BASIC_LINE_NUMBER\n"
                       " is above the maximum possible BASIC line number");

    if (MAXIMUM_STARTING_BASIC_LINE_NUMBER > LINE_NUMBER_TYPE_MAX)
        internal_error("MAXIMUM_STARTING_BASIC_LINE_NUMBER cannot be operated on internally");
}

static void
check_default_basic_line_number_step_size_macro(void)
{
    if (DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE < MINIMUM_BASIC_LINE_NUMBER_STEP_SIZE)
        internal_error("DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE must be at least %u",
                       MINIMUM_BASIC_LINE_NUMBER_STEP_SIZE);

    if (DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE > LINE_NUMBER_STEP_TYPE_MAX)
        internal_error("DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE cannot be operated on internally");
}

static void
check_typable_default_basic_line_number_step_size_macro(void)
{
    if (TYPABLE_DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE < MINIMUM_BASIC_LINE_NUMBER_STEP_SIZE)
        internal_error("TYPABLE_DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE must be at least %u",
                       MINIMUM_BASIC_LINE_NUMBER_STEP_SIZE);

    if (TYPABLE_DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE > LINE_NUMBER_STEP_TYPE_MAX)
        internal_error("TYPABLE_DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE cannot be operated on internally");
}

static void
check_maximum_basic_line_number_step_size_macro(void)
{
    if (MAXIMUM_BASIC_LINE_NUMBER_STEP_SIZE > LINE_NUMBER_STEP_TYPE_MAX)
        internal_error("MAXIMUM_BASIC_LINE_NUMBER_STEP_SIZE cannot be operated on internally");
}

static void
check_basic_program_parameter_macros(void)
{
    if (MAXIMUM_BASIC_LINE_COUNT > LINE_COUNTER_TYPE_MAX)
        internal_error("MAXIMUM_BASIC_LINE_COUNT cannot be operated on internally");

    if (MAXIMUM_BASIC_PROGRAM_SIZE > LONG_MAX)
        internal_error("MAXIMUM_BASIC_PROGRAM_SIZE cannot be operated on internally");

    if (CHECKSUMMED_DATA_PER_LINE < 1)
        internal_error("CHECKSUMMED_DATA_PER_LINE must be at least 1");

    if (CHECKSUMMED_DATA_PER_LINE > USHRT_MAX)
        internal_error("CHECKSUMMED_DATA_PER_LINE cannot be represented internally");
}

static void
check_memory_location_macros(void)
{
    if (COCO_DEFAULT_START_MEMORY_LOCATION > HIGHEST_64K_ADDRESS)
        internal_error("COCO_DEFAULT_START_MEMORY_LOCATION is higher than is possible on the Color Computer");

    if (DRAGON_DEFAULT_START_MEMORY_LOCATION > HIGHEST_64K_ADDRESS)
        internal_error("DRAGON_DEFAULT_START_MEMORY_LOCATION is higher than is possible on the Dragon");

    if (C64_DEFAULT_START_MEMORY_LOCATION > HIGHEST_64K_ADDRESS)
        internal_error("C64_DEFAULT_START_MEMORY_LOCATION is higher than is possible on the Commodore 64");
}

static void
check_output_text_buffer_size_macro(void)
{
    if (OUTPUT_TEXT_BUFFER_SIZE < 100)
        internal_error("OUTPUT_TEXT_BUFFER_SIZE is too low");

    if (OUTPUT_TEXT_BUFFER_SIZE > INT_MAX)
        internal_error("OUTPUT_TEXT_BUFFER_SIZE cannot be operated on internally");
}

static void
check_max_input_file_size_macro(void)
{
    if (MAX_INPUT_FILE_SIZE > LONG_MAX)
        internal_error("MAX_INPUT_FILE_SIZE cannot be operated on internally");
}

static void
check_max_machine_language_binary_size_macro(void)
{
    if (MAX_MACHINE_LANGUAGE_BINARY_SIZE > LONG_MAX)
        internal_error("MAX_MACHINE_LANGUAGE_BINARY_SIZE cannot be represented internally");
}

static void
check_target_architecture_file_size_max_macro(void)
{
    if (TARGET_ARCHITECTURE_FILE_SIZE_MAX > LONG_MAX)
        internal_error("TARGET_ARCHITECTURE_FILE_SIZE_MAX is too high to be internally operated on");
}

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
fail_while_printing_warning(void)
{
    fail("Couldn't print warning to standard output");
}

static void
warning(const char *fmt, ...)
{
    va_list ap;
    FILE *stream = PRINT_WARNINGS_TO_STDERR ? stderr : stdout;
    int vfprintf_return_value = 0;

    if (fprintf(stream, "Warning: ") < 0)
        fail_while_printing_warning();

    va_start(ap, fmt);
    vfprintf_return_value = vfprintf(stream, fmt, ap);
    va_end(ap);

    if (vfprintf_return_value < 0)
        fail_while_printing_warning();

    if (fprintf(stream, "\n") < 0)
        fail_while_printing_warning();
}

static void
print_version_text(void)
{
    puts("BASICloader (under development)");
    puts("(c) 2017 Richard Cavell");
}

static void
display_version(void)
{
    print_version_text();

    exit(EXIT_SUCCESS);
}

static void
display_help(void)
{
    print_version_text();
    puts("https://github.com/richardcavell/BASICloader");
    puts("");
    puts("Usage: BASICloader [options] [filename]");
    puts("");
    puts("  -o  --output    Output filename");
    puts("  -m  --machine   Target machine (coco/dragon/c64)");
    puts("  -f  --format    Input file format (binary/rsdos/dragon/prg)");
    puts("  -c  --case      Output case (upper/lower)");
    puts("  -t  --typable   Unpack the BASIC program and use spaces");
    puts("  -r  --remarks   Add remarks and date");
    puts("      --extbas    Assume Extended Color BASIC (coco only)");
    puts("      --verify    Verify the success of each POKE");
    puts("      --checksum  Calculate checksums");
    puts("  -s  --start     Start memory location");
    puts("  -e  --exec      Exec memory location");
    puts("  -p  --print     Print the BASIC program to standard output");
    puts("  -n  --nowarn    Turn warnings off");
    puts("  -l  --license   Your license to use this program");
    puts("  -i  --info      What this program does");
    puts("  -h  --help      This help information");
    puts("  -v  --version   Version number");
    puts("");
    puts("For advanced options and more description, see the Options.txt file");

    exit(EXIT_SUCCESS);
}

static void
display_info(void)
{
    puts("BASICloader reads in a machine language binary, and then constructs");
    puts("a BASIC program that will run on the selected target architecture.");
    puts("");
    puts("The BASIC program so produced contains DATA statements that represent");
    puts("the machine language program given to BASICloader when the BASIC program");
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
display_license(void)
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

static const char *
target_architecture_to_text(enum target_architecture_choice target_architecture)
{
    const char *text = NULL;

    switch(target_architecture)
    {
        case COCO:    text = COCO_TEXT;    break;
        case DRAGON:  text = DRAGON_TEXT;  break;
        case C64:     text = C64_TEXT;     break;
        default:      internal_error("Unhandled target architecture in target_architecture_to_text()");
    }

    return text;
}

static const char *
format_to_text(enum input_file_format_choice format)
{
    const char *text = NULL;

    switch(format)
    {
        case BINARY:      text = BINARY_TEXT;      break;
        case RS_DOS:      text = RS_DOS_TEXT;      break;
        case DRAGON_DOS:  text = DRAGON_DOS_TEXT;  break;
        case PRG:         text = PRG_TEXT;         break;
        default:          internal_error("Unhandled format in format_to_text()");
    }

    return text;
}

static const char *
case_to_text(enum output_case_choice output_case)
{
    const char *text = NULL;

    switch(output_case)
    {
        case UPPERCASE:   text = UPPERCASE_TEXT;   break;
        case LOWERCASE:   text = LOWERCASE_TEXT;   break;
        case MIXED_CASE:  text = MIXED_CASE_TEXT;  break;
        default:          internal_error("Unhandled case in case_to_text()");
    }

    return text;
}

static void
display_defaults(void)
{
    printf("Default target architecture : %s\n"    , target_architecture_to_text(DEFAULT_TARGET_ARCHITECTURE));
    printf("Default input format        : %s\n"    , format_to_text(DEFAULT_INPUT_FILE_FORMAT));
    printf("Default output case is      : %s\n"    , case_to_text(DEFAULT_OUTPUT_CASE));
    printf("Default output filename     : \"%s\"\n", DEFAULT_OUTPUT_FILENAME);
    printf("Default output filename     : \"%s\" (with --machine c64 --case lower)\n",
                                                     C64_LC_DEFAULT_OUTPUT_FILENAME);

    exit(EXIT_SUCCESS);
}

static boolean_type
arg_match(const char *arg_text,
          const char *option_name)
{
    return arg_text    != NULL &&
           option_name != NULL &&
           strcmp(arg_text, option_name) == 0;
}

static boolean_type
arg2_match(const char *arg_text,
           const char *short_option_name,
           const char *long_option_name)
{
    return arg_match(arg_text, short_option_name)
        || arg_match(arg_text, long_option_name);
}

static int
unknown_arg(const char *arg)
{
  const int dash = '-';
  return arg[0] == dash;
}

static void
get_output_filename(const char *arg1,
                    const char *arg2,
                    const char **output_filename)
{
    if (*output_filename != NULL)
        fail("You can only set option %s once", arg1);

    if (arg2 == NULL)
        fail("You must supply a filename after %s", arg1);

    *output_filename = arg2;
}

static void
get_target_architecture_arg(const char                       *arg1,
                            const char                       *arg2,
                            enum target_architecture_choice  *target_architecture)
{
    if (*target_architecture != NO_TARGET_ARCHITECTURE_CHOSEN)
        fail("You can only set %s once", arg1);

         if (strcmp(arg2, COCO_TEXT) == 0)    *target_architecture = COCO;
    else if (strcmp(arg2, DRAGON_TEXT) == 0)  *target_architecture = DRAGON;
    else if (strcmp(arg2, C64_TEXT) == 0)     *target_architecture = C64;
    else fail("Unknown target architecture \"%s\"", arg2);
}

static void
get_format(const char                     *arg1,
           const char                     *arg2,
           enum input_file_format_choice  *format)
{
    if (*format != NO_INPUT_FILE_FORMAT_CHOSEN)
        fail("You can only set %s once", arg1);

         if (strcmp(arg2, BINARY_TEXT) == 0)      *format = BINARY;
    else if (strcmp(arg2, RS_DOS_TEXT) == 0)      *format = RS_DOS;
    else if (strcmp(arg2, DRAGON_DOS_TEXT) == 0)  *format = DRAGON_DOS;
    else if (strcmp(arg2, PRG_TEXT) == 0)         *format = PRG;
    else fail("Unknown file format \"%s\"", arg2);
}

static void
get_case(const char               *arg1,
         const char               *arg2,
         enum output_case_choice  *output_case)
{
    if (*output_case != NO_OUTPUT_CASE_CHOSEN)
        fail("You can only set %s once", arg1);

         if (strcmp(arg2, UPPERCASE_TEXT) == 0)   *output_case = UPPERCASE;
    else if (strcmp(arg2, LOWERCASE_TEXT) == 0)   *output_case = LOWERCASE;
    else if (strcmp(arg2, MIXED_CASE_TEXT) == 0)  *output_case = MIXED_CASE;
    else fail("Unknown case \"%s\"", arg2);
}

static void
set_switch(const char    *arg,
           boolean_type  *sw)
{
    if (*sw == 1)
        fail("Option %s has already been set", arg);

    *sw = 1;
}

static unsigned long int
string_to_unsigned_long(const char         *pstring,
                        boolean_type       *ok,
                        unsigned long int  max)
{
    unsigned long int l = 0;
    char *endptr = NULL;
    int base = 0;

    errno = 0;

    if (pstring != NULL && pstring[0] == '$')
    {
        base = 16;
        ++pstring;
    }
    else
        base = 0;

    if (pstring != NULL)
        l = strtoul(pstring, &endptr, base);

    *ok = (     pstring    != NULL
            && *pstring    != '\0'
            &&  endptr     != NULL
            && *endptr     == '\0'
            &&  errno      == 0
            &&  pstring[0] != '-'
            &&  l <= max );

  return l;
}

static void
get_line_number(const char        *arg1,
                const char        *arg2,
                line_number_type  *line_number,
                boolean_type      *line_set)
{
    boolean_type ok = 0;

    if (*line_set != 0)
        fail("Option %s can only be set once", arg1);

    *line_number = (line_number_type) string_to_unsigned_long(arg2, &ok,
                                      MAXIMUM_STARTING_BASIC_LINE_NUMBER);

    if (ok == 0)
        fail("%s takes a number from %u to %u", arg1,
             MIN_BASIC_LINE_NUMBER,
             MAXIMUM_STARTING_BASIC_LINE_NUMBER);

    *line_set = 1;
}

static void
get_line_number_step(const char             *arg1,
                     const char             *arg2,
                     line_number_step_type  *step,
                     boolean_type           *step_set)
{
    boolean_type ok = 0;

    if (*step_set != 0)
        fail("Option %s can only be set once", arg1);

    *step = (line_number_step_type) string_to_unsigned_long(arg2, &ok,
                                    MAXIMUM_BASIC_LINE_NUMBER_STEP_SIZE);

    if (ok == 0)
        fail("%s takes a number from %u to %u", arg1,
             MINIMUM_BASIC_LINE_NUMBER_STEP_SIZE,
             MAXIMUM_BASIC_LINE_NUMBER_STEP_SIZE);

    *step_set = 1;
}

static void
get_memory_location_type_arg(const char            *arg1,
                             const char            *arg2,
                             memory_location_type  *pmem,
                             boolean_type          *set)
{
    boolean_type ok = 0;

    if (*set != 0)
        fail("Option %s can only be set once", arg1);

    *pmem = (memory_location_type) string_to_unsigned_long(arg2, &ok,
                                   HIGHEST_RAM_ADDRESS);

    if (ok == 0)
        fail("%s takes a number up to 0x%x",
             arg1, MEMORY_LOCATION_TYPE_MAX);

    *set = 1;
}

static enum target_architecture_choice
set_target_architecture(enum target_architecture_choice target_architecture)
{
    if (target_architecture == NO_TARGET_ARCHITECTURE_CHOSEN)
        target_architecture =  DEFAULT_TARGET_ARCHITECTURE;

    return target_architecture;
}

static void
check_input_file_format(enum target_architecture_choice target_architecture,
                        enum input_file_format_choice   input_file_format)
{
    if (input_file_format == PRG         && target_architecture != C64)
        fail("File format \"%s\" should only be used with the \"%s\" target",
             PRG_TEXT, C64_TEXT);

    if (input_file_format == DRAGON_DOS  && target_architecture != DRAGON)
        fail("File format \"%s\" should only be used with the \"%s\" target",
             DRAGON_DOS_TEXT, DRAGON_TEXT);

    if (input_file_format == RS_DOS      && target_architecture != COCO)
        fail("File format \"%s\" should only be used with the \"%s\" target",
             RS_DOS_TEXT, COCO_TEXT);
}

static enum input_file_format_choice
set_input_file_format(enum target_architecture_choice  target_architecture,
                      enum input_file_format_choice    input_file_format)
{
    if (input_file_format == NO_INPUT_FILE_FORMAT_CHOSEN)
        input_file_format =  DEFAULT_INPUT_FILE_FORMAT;

    check_input_file_format(target_architecture, input_file_format);

    return input_file_format;
}

static void
check_output_case(enum target_architecture_choice target_architecture,
                  enum output_case_choice         output_case)
{
    if (output_case == LOWERCASE && target_architecture == COCO)
        fail("Lowercase output is not useful for the \"%s\" target", COCO_TEXT);

    if (output_case == LOWERCASE && target_architecture == DRAGON)
        fail("Lowercase output is not useful for the \"%s\" target", DRAGON_TEXT);

    if (output_case == MIXED_CASE)
        fail("There is presently no target for mixed case output");
}

static enum output_case_choice
set_output_case(enum target_architecture_choice  target_architecture,
                enum output_case_choice          output_case)
{
    if (output_case == NO_OUTPUT_CASE_CHOSEN)
        output_case =  DEFAULT_OUTPUT_CASE;

    check_output_case(target_architecture, output_case);

    return output_case;
}

static boolean_type
set_nowarn(boolean_type nowarn, boolean_type print_program)
{
    return (print_program) ? 1 : nowarn;
}

static boolean_type
set_typable(boolean_type typable, boolean_type checksum)
{
    return (checksum) ? 1 : typable;
}

static boolean_type
set_print_program(const char    *output_filename,
                  boolean_type  print_program)
{
    return (output_filename != NULL &&
            strcmp(output_filename, STDOUT_FILENAME_SUBSTITUTE) == 0) ? 1 : print_program;
}

static void
check_extended_basic(enum target_architecture_choice  target_architecture,
                     boolean_type                     extended_basic)
{
    if (extended_basic && target_architecture != COCO)
        fail("Extended Color BASIC option should only be used"
             " with the \"%s\" target", COCO_TEXT);
}

static void
check_print_options(boolean_type  print_program,
                    boolean_type  print_diag)
{
    if (print_program == 1 && print_diag == 1)
        fail("--print and --diag options cannot be used together");
}

static void
check_input_filename(const char    *input_filename,
                     boolean_type  read_stdin)
{
    if (input_filename == NULL && read_stdin == 0)
        fail("You must specify an input file");

    if (input_filename != NULL && read_stdin == 1)
        fail("You cannot give an input filename while using --stdin");
}

static const char *
set_output_filename(enum target_architecture_choice  target_architecture,
                    enum output_case_choice          output_case,
                    const char                       *output_filename,
                    boolean_type                     print_program)
{
    if (print_program == 1 && output_filename != NULL && strcmp(output_filename, STDOUT_FILENAME_SUBSTITUTE) != 0)
        fail("You cannot specify an output filename with option --print");

    if (print_program == 0 && output_filename == NULL)
    {
        if (target_architecture == C64 && output_case == LOWERCASE)
            output_filename = C64_LC_DEFAULT_OUTPUT_FILENAME;
        else
            output_filename = DEFAULT_OUTPUT_FILENAME;
    }

    return output_filename;
}

static FILE *
open_input_file(const char    *input_filename,
                boolean_type  read_stdin)
{
    FILE *input_file = NULL;

    if (read_stdin == 1)
    {
        input_file = stdin;
    }
    else
    {
        errno = 0;

        input_file = fopen(input_filename, "r");

        if (input_file == NULL)
            fail("Could not open file \"%s\". Error number %d", input_filename, errno);
    }

    return input_file;
}

static long int
get_file_position(FILE        *file,
                  const char  *filename)
{
    long int file_size;

    errno = 0;

    file_size = ftell(file);

    if (file_size < 0)
        fail("Could not get size of file \"%s\". Error number %d", filename, errno);

    return file_size;
}

static void
check_input_file_size(long int                       input_file_size,
                      const char                     *input_filename,
                      enum input_file_format_choice  input_file_format)
{
    long int input_file_size_min = 0;

    if (input_file_size == 0)
        fail("File \"%s\" is empty", input_filename);

    switch(input_file_format)
    {
        case BINARY:
                input_file_size_min = BINARY_FILE_SIZE_MINIMUM;
                break;

        case PRG:
                input_file_size_min = PRG_FILE_SIZE_MINIMUM;
                break;

        case DRAGON_DOS:
                input_file_size_min = DRAGON_DOS_FILE_SIZE_MINIMUM;
                break;

        case RS_DOS:
                input_file_size_min = RS_DOS_FILE_SIZE_MINIMUM;
                break;

        default:
                fail("Unhandled file format type in check_input_file_size()");
    }

    if (input_file_size < input_file_size_min)
        fail("Minimum input file size for file format \"%s\" is %lu\n"
             "Input file \"%s\" is %lu bytes long",
             format_to_text(input_file_format), input_file_size_min,
             input_filename, input_file_size);

    if (input_file_size > MAX_INPUT_FILE_SIZE)
        fail("Input file \"%s\" is too large", input_filename);
}

static long int
get_input_file_size(FILE                           *input_file,
                    const char                     *input_filename,
                    enum input_file_format_choice  input_file_format)
{
    long int input_file_size = 0;

    errno = 0;

    if (fseek(input_file, 0L, SEEK_END))
        fail("Could not find the end of file \"%s\". Error number %d", input_filename, errno);

    input_file_size = get_file_position(input_file, input_filename);

    if (fseek(input_file, 0L, SEEK_SET) < 0)
        fail("Could not rewind file \"%s\". Error number %d", input_filename, errno);

    check_input_file_size(input_file_size, input_filename, input_file_format);

    return input_file_size;
}

static void
check_blob_size(long int    blob_size,
                const char  *input_filename)
{
    if (blob_size == 0)
        fail("Input file \"%s\" contains no machine language content", input_filename);

    if (blob_size > MAX_MACHINE_LANGUAGE_BINARY_SIZE)
        fail("The machine language content of input file \"%s\" is too large", input_filename);
}

static long int
get_blob_size(const char                     *input_filename,
              enum input_file_format_choice  input_file_format,
              long int                       input_file_size)
{
    long int blob_size = 0;

    switch(input_file_format)
    {
        case BINARY:
            blob_size = input_file_size;
            break;

        case PRG:
            blob_size = input_file_size - PRG_FILE_HEADER_SIZE;
            break;

        case DRAGON_DOS:
            blob_size = input_file_size - DRAGON_DOS_FILE_HEADER_SIZE;
            break;

        case RS_DOS:
            blob_size = input_file_size - (RS_DOS_FILE_PREAMBLE_SIZE + RS_DOS_FILE_POSTAMBLE_SIZE);
            break;

        default:
            internal_error("Unhandled file format in get_blob_size()");
            break;
    }

    check_blob_size(blob_size, input_filename);

    return blob_size;
}

static unsigned char
get_character_from_input_file(FILE        *input_file,
                              const char  *input_filename)
{
    int input_file_character;

    errno = 0;

    input_file_character = fgetc(input_file);

    if (input_file_character > EIGHT_BIT_UCHAR_MAX)
        fail("Byte read from input file \"%s\" is too high"
             " for an 8-bit machine", input_filename);

    if (input_file_character == EOF)
        fail("Unexpected end of file while reading file \"%s\". Error code %d",
              input_filename, errno);

    return (unsigned char) input_file_character;
}

static void
get_header_or_preamble_or_postamble(unsigned char  arr[],
                                    size_t         size,
                                    FILE           *input_file,
                                    const char     *input_filename)
{
    unsigned short int i = 0;

    for (i = 0; i < size; ++i)
        arr[i] = get_character_from_input_file(input_file, input_filename);
}

static memory_location_type
construct_16bit_value(unsigned char hi, unsigned char lo)
{
    return (memory_location_type) (hi * 256 + lo);
}

static void
process_rs_dos_header(FILE                  *input_file,
                      const char            *input_filename,
                      boolean_type          *start_set,
                      memory_location_type  *start,
                      boolean_type          *exec_set,
                      memory_location_type  *exec,
                      long int              blob_size,
                      boolean_type          nowarn)
{
    memory_location_type st = 0;
    memory_location_type ex = 0;
    target_architecture_file_size_type ln = 0;
    unsigned char  preamble[RS_DOS_FILE_PREAMBLE_SIZE];
    unsigned char postamble[RS_DOS_FILE_POSTAMBLE_SIZE];

    (void) nowarn;

    get_header_or_preamble_or_postamble(preamble, sizeof preamble, input_file, input_filename);

    if (preamble[0] != 0x0)
      fail("Input file \"%s\" is not properly formed as an "
           "RS-DOS file (bad header)", input_filename);

    ln = construct_16bit_value(preamble[1], preamble[2]);

    if (ln != blob_size)
      fail("Input file \"%s\" length (%u) given in the header\n"
           "does not match measured length (%u)",
                       input_filename, ln, blob_size);

    st = construct_16bit_value(preamble[3], preamble[4]);

    if (*start_set == 1 && st != *start)
      fail("Input RS-DOS file \"%s\" gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, *start);

    *start = st;
    *start_set = 1;

    if (fseek(input_file, (long int) (0 - RS_DOS_FILE_POSTAMBLE_SIZE), SEEK_END) < 0)
      fail("Couldn't operate on file \"%s\". Error number %d",
                                     input_filename, errno);

    get_header_or_preamble_or_postamble(postamble,
                                        sizeof postamble,
                                        input_file,
                                        input_filename);

    if (postamble[0] == 0x0)
      fail("Input RS-DOS file \"%s\" is segmented, and cannot be used",
                                input_filename);

    if (postamble[0] != 0xff ||
        postamble[1] != 0x0  ||
        postamble[2] != 0x0)
      fail("Input file \"%s\" is not properly formed as an RS-DOS file (bad tail)",
                         input_filename);

    ex = construct_16bit_value(postamble[3], postamble[4]);

    if (*exec_set == 1 && ex != *exec)
      fail("Input RS-DOS file \"%s\" gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)",
                              input_filename, ex, exec);

    if (ex < st)
      fail("The exec location in the tail of the RS-DOS file \"%s\"\n"
           "($%x) is below the start location of the binary blob ($%x)",
           input_filename, *exec, *start);

    if (ex > st + blob_size)
      fail("The exec location in the tail of the RS-DOS file \"%s\"\n"
           "($%x) is beyond the end location of the binary blob ($%x)",
           input_filename, *exec, st + blob_size);

    *exec = ex;

    if (fseek(input_file, (long int) RS_DOS_FILE_PREAMBLE_SIZE, SEEK_SET) < 0)
      fail("Couldn't operate on file \"%s\". Error number %d",
                                     input_filename, errno);
}

static void
process_dragon_dos_header(FILE                  *input_file,
                          const char            *input_filename,
                          boolean_type          *start_set,
                          memory_location_type  *start,
                          boolean_type          *exec_set,
                          memory_location_type  *exec,
                          long int              blob_size,
                          boolean_type          nowarn)
{
    unsigned char header[DRAGON_DOS_FILE_HEADER_SIZE];
    memory_location_type st = 0;
    memory_location_type ex = 0;

    (void) nowarn;

    get_header_or_preamble_or_postamble(header,
                                        sizeof header,
                                        input_file,
                                        input_filename);

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
                 " is an unsupported file (possibly DosPlus)\n", input_filename);
            break;

        default:
            fail("Input Dragon DOS file \"%s\""
                 " has an unknown FILETYPE\n", input_filename);
            break;
    }

    if (construct_16bit_value(header[4], header[5]) != blob_size)
      fail("The header of input Dragon DOS file \"%s\""
           " gives incorrect length", input_filename);

    st = construct_16bit_value(header[2], header[3]);
    ex = construct_16bit_value(header[6], header[7]);

    if (*start_set == 1 && st != *start)
      fail("Input Dragon DOS file \"%s\" gives a different start address ($%x)\n"
           "to the one given at the command line ($%x)", st, *start);

    if (*exec_set == 1 && ex != *exec)
      fail("Input Dragon DOS file \"%s\" gives a different exec address ($%x)\n"
           "to the one given at the command line ($%x)", ex, *exec);

    if (ex < st)
      fail("The exec location in the header of the Dragon DOS file \"%s\"\n"
           "($%x) is below the start location of the binary blob ($%x)",
           input_filename, *exec, *start);

    if (ex > st + blob_size)
      fail("The exec location in the header of the Dragon DOS file \"%s\"\n"
           "($%x) is beyond the end location of the binary blob ($%x)",
           input_filename, *exec, st + blob_size);

    *start = st;
    *start_set = 1;

    *exec  = ex;
    *exec_set = 1;
}

static void
process_prg_header(FILE                  *input_file,
                   const char            *input_filename,
                   boolean_type          *start_set,
                   memory_location_type  *start,
                   boolean_type          *exec_set,
                   memory_location_type  *exec,
                   long int              blob_size,
                   boolean_type          nowarn)
{
    unsigned char header[PRG_FILE_HEADER_SIZE];
    memory_location_type st = 0;

    get_header_or_preamble_or_postamble(header,
                                        sizeof header,
                                        input_file,
                                        input_filename);

    st = construct_16bit_value(header[1], header[0]);

    if (st == 0x0801)
        fail("Input PRG file \"%s\" is unsuitable for use with BASICloader\n"
             "It is likely to be a BASIC program, or a hybrid"
             " BASIC/machine language program", input_filename);

    if (nowarn == 0 && *start_set == 1 && st != *start)
        warning("Input PRG file \"%s\" gives a different start address ($%x)\n"
             "to the one given at the command line ($%x)", input_filename, st, *start);

    if (nowarn == 0 && *exec_set == 1 && st != *exec)
        warning("Exec address given at the command line ($%x) is not the"
                "same as the start address ($%x) of input PRG file \"%s\"\n", *exec, st, input_filename);

    *start     = st;
    *start_set = 1;

    (void) blob_size;

    if (fseek(input_file, (long int) PRG_FILE_HEADER_SIZE, SEEK_SET) < 0)
        fail("Couldn't operate on file \"%s\". Error number %d", input_filename, errno);
}

static void
process_header(enum input_file_format_choice  input_file_format,
               FILE                           *input_file,
               const char                     *input_filename,
               boolean_type                   *start_set,
               memory_location_type           *start,
               boolean_type                   *exec_set,
               memory_location_type           *exec,
               long int                       blob_size,
               boolean_type                   nowarn)
{
    switch(input_file_format)
    {
        case BINARY:
                break;

        case RS_DOS:
                    process_rs_dos_header(input_file,
                                          input_filename,
                                          start_set,
                                          start,
                                          exec_set,
                                          exec,
                                          blob_size,
                                          nowarn);
                break;

        case DRAGON_DOS:
                process_dragon_dos_header(input_file,
                                          input_filename,
                                          start_set,
                                          start,
                                          exec_set,
                                          exec,
                                          blob_size,
                                          nowarn);
                break;

        case PRG:
                       process_prg_header(input_file,
                                          input_filename,
                                          start_set,
                                          start,
                                          exec_set,
                                          exec,
                                          blob_size,
                                          nowarn);
                break;

        default:
                internal_error("Unhandled file format in process_header()");
                break;
    }
}

static memory_location_type
set_start_address(enum target_architecture_choice  target_architecture,
                  boolean_type                     start_set,
                  memory_location_type             start)
{
    if (start_set == 0)
    {
        switch(target_architecture)
        {
            case COCO:
                start = COCO_DEFAULT_START_MEMORY_LOCATION;
                break;

            case DRAGON:
                start = DRAGON_DEFAULT_START_MEMORY_LOCATION;
                break;

            case C64:
                start = C64_DEFAULT_START_MEMORY_LOCATION;
                break;

            default:
                internal_error("Unhandled target architecture in set_start_address()");
        }
    }

#if (HIGHEST_RAM_ADDRESS < MEMORY_LOCATION_TYPE_MAX)
    if (start > HIGHEST_RAM_ADDRESS)
        internal_error("Start location is higher than the highest possible RAM address");
#endif

    return start;
}

static memory_location_type
set_end_address(memory_location_type  start,
                long int              blob_size)
{
    long int end = start + blob_size - 1;

#if (HIGHEST_RAM_ADDRESS < MEMORY_LOCATION_TYPE_MAX)
    if (end > HIGHEST_RAM_ADDRESS)
        fail("The machine language blob would overflow the RAM limit");
#endif

#if (LONG_MAX > MEMORY_LOCATION_TYPE_MAX)
    if (end > MEMORY_LOCATION_TYPE_MAX)
        fail("The machine language blob would overflow the RAM limit");
#endif

    if (end < start)
        fail("The machine language blob would overflow the RAM limit");

    return (memory_location_type) end;
}

static memory_location_type
set_exec_address(boolean_type          exec_set,
                 memory_location_type  start,
                 memory_location_type  exec,
                 memory_location_type  end)
{
    if (exec_set == 0)
        exec     = start;

#if (HIGHEST_RAM_ADDRESS < MEMORY_LOCATION_TYPE_MAX)
    if (exec > HIGHEST_RAM_ADDRESS)
        internal_error("Exec location is higher than the highest possible RAM address");
#endif

    if (exec < start)
        fail("The exec location given ($%x) is below\n"
             "the start location of the binary blob ($%x)", exec, start);

    if (exec > end)
        fail("The exec location given ($%x) is beyond\n"
             "the end location of the binary blob ($%x)", exec, end);

    return exec;
}

static FILE *
open_output_file(const char    *output_filename,
                 boolean_type  print_program)
{
    FILE *output_file = NULL;

    if (print_program == 1)
        output_file = stdout;
    else
    {
        output_file = fopen(output_filename, "w");

        if (output_file == NULL)
            fail("Could not open file \"%s\". Error number %d", output_filename, errno);
    }

    return output_file;
}

static void
ram_requirement_warning(enum target_architecture_choice  target_architecture,
                        boolean_type                     nowarn,
                        memory_location_type             end)
{
    if (target_architecture == COCO && nowarn == 0)
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

    if (target_architecture == DRAGON && nowarn == 0)
    {
        if (end > HIGHEST_32K_ADDRESS)
            warning("Program requires 64K of RAM");
    }
}

static void
check_line_number(line_number_type line_number)
{
    (void) line_number;

#if (MIN_BASIC_LINE_NUMBER > 0)
    if (line_number < MIN_BASIC_LINE_NUMBER)
        internal_error("Line number is below minimum");
#endif

#if (MAX_BASIC_LINE_NUMBER < LINE_NUMBER_TYPE_MAX)
    if (line_number > MAX_BASIC_LINE_NUMBER)
        fail("The BASIC line numbers have become too large");
#endif
}

static line_number_type
set_line_number(boolean_type      line_number_set,
                line_number_type  line_number,
                boolean_type      typable)
{
    if (line_number_set == 0)
        line_number = typable ?
                       TYPABLE_DEFAULT_STARTING_BASIC_LINE_NUMBER :
                       DEFAULT_STARTING_BASIC_LINE_NUMBER;

    check_line_number(line_number);

    return line_number;
}

static line_number_step_type
set_step(boolean_type           step_set,
         line_number_step_type  step,
         boolean_type           typable)
{
    if (step_set == 0)
        step = typable ?
                TYPABLE_DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE :
                DEFAULT_BASIC_LINE_NUMBER_STEP_SIZE;

    return step;
}

static void
safe_step_line_number(line_number_type       *line_number,
                      line_number_step_type  step)
{
    while (step--)
    {
        if (*line_number == LINE_NUMBER_TYPE_MAX)
            internal_error("Line number overflow");

        ++*line_number;
    }
}

static void
inc_line_number(boolean_type           *line_incrementing_has_started,
                line_number_type       *line_number,
                line_number_step_type  *step)
{
    if (*line_incrementing_has_started == 1)
        safe_step_line_number(line_number, *step);

    *line_incrementing_has_started = 1;

    check_line_number(*line_number);
}

static void
inc_line_count(line_counter_type *line_count)
{
    if (*line_count == LINE_COUNTER_TYPE_MAX)
        internal_error("Line count overflow detected");

    ++*line_count;

#if (MAXIMUM_BASIC_LINE_COUNT != LINE_COUNTER_TYPE_MAX)
    if (*line_count > MAXIMUM_BASIC_LINE_COUNT)
        fail("Line count has exceeded the set maximum");
#endif
}

static line_position_type
get_architecture_maximum_basic_line_length(enum target_architecture_choice target_architecture)
{
    line_position_type architecture_max_basic_line_length = COCO_MAX_BASIC_LINE_LENGTH;

    switch(target_architecture)
    {
        case COCO:
            architecture_max_basic_line_length = COCO_MAX_BASIC_LINE_LENGTH;
            break;

        case DRAGON:
            architecture_max_basic_line_length = DRAGON_MAX_BASIC_LINE_LENGTH;
            break;

        case C64:
            architecture_max_basic_line_length = C64_MAX_BASIC_LINE_LENGTH;
            break;

        default:
            internal_error("Unhandled target architecture type in get_architecture_max_basic_line_length()");
    }

    return architecture_max_basic_line_length;
}

static void
check_line_position(enum target_architecture_choice  target_architecture,
                    line_position_type               line_position)
{
    if (line_position > MAXIMUM_BASIC_LINE_LENGTH)
        internal_error("Maximum BASIC line length was not avoided");

    if (line_position > get_architecture_maximum_basic_line_length(target_architecture))
        internal_error("The maximum BASIC line length for the \"%s\" target architecture was exceeded",
                        target_architecture_to_text(target_architecture));
}

static void
increment_line_position(enum target_architecture_choice  target_architecture,
                             line_position_type               *line_position)
{
    if (*line_position == LINE_POSITION_TYPE_MAX)
        internal_error("Line position overflow detected");

    ++(*line_position);

    check_line_position(target_architecture, *line_position);
}

static void
caseify_output_text(char                     *output_text_pointer,
                    enum output_case_choice  output_case)
{
    while (*output_text_pointer != '\0')
    {
        switch (output_case)
        {
            case UPPERCASE:
                *output_text_pointer = (char) toupper(*output_text_pointer);
                break;

            case LOWERCASE:
                *output_text_pointer = (char) tolower(*output_text_pointer);
                break;

            case MIXED_CASE:
                break;

            case NO_OUTPUT_CASE_CHOSEN:
            default:
                internal_error("Unhandled case in caseify_output_text()");
                break;
        }

        ++output_text_pointer;
    }
}

static void
process_output_text(char                             *output_text_pointer,
                    enum target_architecture_choice  target_architecture,
                    enum output_case_choice          output_case,
                    line_counter_type                *line_count,
                    line_position_type               *line_position)
{
    caseify_output_text(output_text_pointer, output_case);

    while (*output_text_pointer != '\0')
    {
        if (*output_text_pointer == '\n')
        {
            inc_line_count(line_count);
            *line_position = 0;
        }
        else
            increment_line_position(target_architecture, line_position);

        ++output_text_pointer;
    }
}

enum vemit_status
{
    VE_SUCCESS = 0,
    VSPRINTF_FAIL,
    EMIT_FAIL,
    FTELL_FAIL,
    TOO_LARGE
};

static void
check_vemit_status(enum vemit_status status)
{
    switch(status)
    {
        case VE_SUCCESS:
            break;

        case VSPRINTF_FAIL:
            fail("Couldn't write to output text area");
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
            internal_error("Unknown status in check_vemit_status()");
            break;
    }
}

static enum vemit_status
vemit(FILE                             *output_file,
      enum target_architecture_choice  target_architecture,
      enum output_case_choice          output_case,
      long int                         *output_file_size,
      line_counter_type                *line_count,
      line_position_type               *line_position,
      const char                       *fmt,
      va_list                          ap)
{
    char      output_text[OUTPUT_TEXT_BUFFER_SIZE] = "";
    int       vsprintf_return_value = vsprintf(output_text, fmt, ap);
    int       fprintf_return_value = 0;

    if (vsprintf_return_value < 0)
        return VSPRINTF_FAIL;

    if (vsprintf_return_value >= (signed int) sizeof output_text)
        internal_error("Output text buffer overrun");

    process_output_text(output_text,
                        target_architecture,
                        output_case,
                        line_count,
                        line_position);

    fprintf_return_value = fprintf(output_file, "%s", output_text);

    if (fprintf_return_value < 0 || fprintf_return_value != vsprintf_return_value)
        return EMIT_FAIL;

    *output_file_size += fprintf_return_value;

    if (output_file != stdout)
    {
        long int  ftell_return_value = ftell(output_file);

        if (ftell_return_value < 0)
            return FTELL_FAIL;

        if (*output_file_size != ftell_return_value)
            return EMIT_FAIL;
    }

    if (*output_file_size > MAXIMUM_BASIC_PROGRAM_SIZE ||
        *output_file_size > TARGET_ARCHITECTURE_FILE_SIZE_MAX)
        return TOO_LARGE;

    return VE_SUCCESS;
}

static void
emit(FILE                             *output_file,
     enum target_architecture_choice  target_architecture,
     enum output_case_choice          output_case,
     long int                         *output_file_size,
     line_counter_type                *line_count,
     line_position_type               *line_position,
     const char                       *fmt,
     ...)
{
    enum vemit_status status = VE_SUCCESS;
    va_list ap;

    va_start(ap, fmt);
    status = vemit(output_file,
                   target_architecture,
                   output_case,
                   output_file_size,
                   line_count,
                   line_position,
                   fmt,
                   ap);
    va_end(ap);

    check_vemit_status(status);
}

static void
emit_line(FILE                             *output_file,
          enum target_architecture_choice  target_architecture,
          enum output_case_choice          output_case,
          long int                         *output_file_size,
          boolean_type                     *line_incrementing_has_started,
          line_counter_type                *line_count,
          line_number_type                 *line_number,
          line_number_step_type            *step,
          line_position_type               *line_position,
          const char                       *fmt,
          ...)
{
    va_list ap;
    enum vemit_status status;

    if (*line_position != 0)
        internal_error("Line emission did not start at position zero");

    inc_line_number(line_incrementing_has_started,
                    line_number,
                    step);

    emit(output_file,
         target_architecture,
         output_case,
         output_file_size,
         line_count,
         line_position,
         "%u ",
         *line_number);

    va_start(ap, fmt);
    status = vemit(output_file,
                   target_architecture,
                   output_case,
                   output_file_size,
                   line_count,
                   line_position,
                   fmt,
                   ap);
    va_end(ap);

    check_vemit_status(status);

    emit(output_file,
         target_architecture,
         output_case,
         output_file_size,
         line_count,
         line_position,
         "\n");
}

static void
emit_datum(FILE                             *output_file,
           enum target_architecture_choice  target_architecture,
           enum output_case_choice          output_case,
           long int                         *output_file_size,
           boolean_type                     typable,
           boolean_type                     *line_incrementing_has_started,
           line_counter_type                *line_count,
           line_number_type                 *line_number,
           line_number_step_type            *step,
           line_position_type               *line_position,
           unsigned long int                datum)
{
    char possible_output_buffer[OUTPUT_TEXT_BUFFER_SIZE] = "";
    int sprintf_return_value = sprintf(possible_output_buffer,
                                       "%s%lu", typable ? ", " : ",", datum);

    if (sprintf_return_value < 0)
        fail("Couldn't write to output text area in emit_datum()");

    if (sprintf_return_value >= OUTPUT_TEXT_BUFFER_SIZE)
        internal_error("Output text buffer overrun");

    if ((*line_position + strlen(possible_output_buffer) > MAXIMUM_BASIC_LINE_LENGTH)
      || *line_position + strlen(possible_output_buffer) > get_architecture_maximum_basic_line_length(target_architecture))
        emit(output_file,
             target_architecture,
             output_case,
             output_file_size,
             line_count,
             line_position,
             "\n");

    if (*line_position == 0)
    {
        inc_line_number(line_incrementing_has_started, line_number, step);
        emit(output_file,
             target_architecture,
             output_case,
             output_file_size,
             line_count,
             line_position,
             "%u DATA%s%lu",
             *line_number,
             typable ? " " : "",
             datum);
    }
    else
    {
        emit(output_file,
             target_architecture,
             output_case,
             output_file_size,
             line_count,
             line_position,
             "%s",
             possible_output_buffer);
    }
}

static const char *
exec_command(enum target_architecture_choice target_architecture)
{
    const char *text = NULL;

    switch(target_architecture)
    {
        case COCO:
        case DRAGON:
            text = "EXEC";
            break;

        case C64:
            text = "SYS";
            break;

        default:
            internal_error("Unhandled target architecture in exec_command()");
            break;
    }

    return text;
}

static void
newline_if_needed(FILE                             *output_file,
                  enum target_architecture_choice  target_architecture,
                  enum output_case_choice          output_case,
                  long int                         *output_file_size,
                  line_counter_type                *line_count,
                  line_position_type               *line_position)
{
    if (*line_position > 0)
        emit(output_file,
             target_architecture,
             output_case,
             output_file_size,
             line_count,
             line_position,
             "\n");
}

static void
check_input_file_remainder(FILE                           *input_file,
                           long int                       input_file_size,
                           enum input_file_format_choice  input_file_format,
                           const char                     *input_filename)
{
    long int remainder = input_file_size - get_file_position(input_file, input_filename);

    if   ((input_file_format == BINARY      && remainder != 0)
       || (input_file_format == RS_DOS      && remainder != RS_DOS_FILE_POSTAMBLE_SIZE)
       || (input_file_format == DRAGON_DOS  && remainder != 0)
       || (input_file_format == PRG         && remainder != 0))
            fail("Unexpected remaining bytes in input file \"%s\"", input_filename);
}

static void
close_file(FILE *file, const char *filename)
{
    if (file         != stdin &&
        file         != stdout &&
        fclose(file) != 0)
            fail("Couldn't close file \"%s\"", filename);
}

static void
print_diagnostic_info(enum target_architecture_choice  target_architecture,
                      enum output_case_choice          output_case,
                      boolean_type                     typable,
                      boolean_type                     remarks,
                      boolean_type                     extended_basic,
                      boolean_type                     checksum,
                      memory_location_type             start,
                      memory_location_type             exec,
                      memory_location_type             end,
                      long int                         blob_size,
                      line_counter_type                line_count,
                      long int                         output_file_size)
{
    printf("Output is for the %s target architecture%s\n",
                      target_architecture_to_text(target_architecture),
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

int main(int argc, char *argv[])
{
    const char *input_filename  = NULL;
    FILE       *input_file      = NULL;

    const char *output_filename = NULL;
    FILE       *output_file     = NULL;

    long int  input_file_size   = 0;
    long int  output_file_size  = 0;
    long int  blob_size         = 0;

    enum target_architecture_choice  target_architecture  = NO_TARGET_ARCHITECTURE_CHOSEN;
    enum input_file_format_choice    input_file_format    = NO_INPUT_FILE_FORMAT_CHOSEN;
    enum output_case_choice          output_case          = NO_OUTPUT_CASE_CHOSEN;

    boolean_type  typable         = 0;
    boolean_type  remarks         = 0;
    boolean_type  extended_basic  = 0;
    boolean_type  verify          = 0;
    boolean_type  checksum        = 0;
    boolean_type  read_stdin      = 0;
    boolean_type  print_program   = 0;
    boolean_type  print_diag      = 0;
    boolean_type  nowarn          = 0;

    boolean_type       line_incrementing_has_started  = 0;
    line_counter_type  line_count                     = 0;
    boolean_type       line_number_set                = 0;
    line_number_type   line_number                    = 0;

    boolean_type           step_set  = 0;
    line_number_step_type  step      = 0;

    line_position_type    line_position  = 0;

    boolean_type          start_set  = 0;
    memory_location_type  start      = 0;
    boolean_type          exec_set   = 0;
    memory_location_type  exec       = 0;
    memory_location_type  end        = 0;

    check_type_limits();
    check_user_defined_type_limits();
    check_default_starting_basic_line_number_macro();
    check_typable_default_starting_basic_line_number_macro();
    check_maximum_starting_basic_line_number_macro();
    check_default_basic_line_number_step_size_macro();
    check_typable_default_basic_line_number_step_size_macro();
    check_maximum_basic_line_number_step_size_macro();
    check_basic_program_parameter_macros();
    check_memory_location_macros();
    check_output_text_buffer_size_macro();
    check_max_input_file_size_macro();
    check_max_machine_language_binary_size_macro();
    check_target_architecture_file_size_max_macro();

    if (argc > 0)
        while (*++argv)
        {
                 if (arg2_match(argv[0], "-h", "--help"))
                display_help();
            else if (arg2_match(argv[0], "-d", "--defaults"))
                display_defaults();
            else if (arg2_match(argv[0], "-i", "--info"))
                display_info();
            else if (arg2_match(argv[0], "-l", "--license"))
                display_license();
            else if (arg2_match(argv[0], "-v", "--version"))
                display_version();
            else if (arg2_match(argv[0], "-o", "--output"))
            {
                get_output_filename(argv[0], argv[1], &output_filename);
                ++argv;
            }
            else if (arg2_match(argv[0], "-m", "--machine"))
            {
                get_target_architecture_arg(argv[0], argv[1], &target_architecture);
                ++argv;
            }
            else if (arg2_match(argv[0], "-f", "--format"))
            {
                get_format(argv[0], argv[1], &input_file_format);
                ++argv;
            }
            else if (arg2_match(argv[0], "-c", "--case"))
            {
                get_case(argv[0], argv[1], &output_case);
                ++argv;
            }
            else if (arg2_match(argv[0], NULL, "--line"))
            {
                get_line_number(argv[0], argv[1], &line_number, &line_number_set);
                ++argv;
            }
            else if (arg2_match(argv[0], NULL, "--step"))
            {
                get_line_number_step(argv[0], argv[1], &step, &step_set);
                ++argv;
            }
            else if (arg2_match(argv[0], "-s", "--start"))
            {
                get_memory_location_type_arg(argv[0], argv[1], &start, &start_set);
                ++argv;
            }
            else if (arg2_match(argv[0], "-e", "--exec"))
            {
                get_memory_location_type_arg(argv[0], argv[1], &exec, &exec_set);
                ++argv;
            }
            else if (arg2_match(argv[0], "-n", "--nowarn"))
                set_switch(argv[0], &nowarn);
            else if (arg2_match(argv[0], "-t", "--typable"))
                set_switch(argv[0], &typable);
            else if (arg2_match(argv[0], NULL, "--verify"))
                set_switch(argv[0], &verify);
            else if (arg2_match(argv[0], NULL, "--checksum"))
                set_switch(argv[0], &checksum);
            else if (arg2_match(argv[0], NULL, "--extbas"))
                set_switch(argv[0], &extended_basic);
            else if (arg2_match(argv[0], "-r", "--remarks"))
                set_switch(argv[0], &remarks);
            else if (arg2_match(argv[0], "-p", "--print"))
                set_switch(argv[0], &print_program);
            else if (arg2_match(argv[0], NULL, "--diag"))
                set_switch(argv[0], &print_diag);
            else if (arg2_match(argv[0], NULL, "--stdin"))
                set_switch(argv[0], &read_stdin);
            else if (unknown_arg(argv[0]))
                fail("Unknown command line option %s", argv[0]);
            else
            {
                if (input_filename != NULL)
                    fail("Only one input filename may be specified");

                input_filename = argv[0];
            }
        }

    target_architecture = set_target_architecture(target_architecture);
    input_file_format   = set_input_file_format(target_architecture, input_file_format);
    output_case         = set_output_case(target_architecture, output_case);
    print_program       = set_print_program(output_filename, print_program);
    nowarn              = set_nowarn(nowarn, print_program);
    typable             = set_typable(typable, checksum);

    check_extended_basic(target_architecture, extended_basic);
    check_print_options(print_program, print_diag);

    check_input_filename(input_filename, read_stdin);
    output_filename  = set_output_filename(target_architecture, output_case, output_filename, print_program);

    input_file       = open_input_file(input_filename, read_stdin);
    input_file_size  = get_input_file_size(input_file, input_filename, input_file_format);
    blob_size        = get_blob_size(input_filename, input_file_format, input_file_size);

    process_header(input_file_format,
                   input_file,
                   input_filename,
                   &start_set,
                   &start,
                   &exec_set,
                   &exec,
                   blob_size,
                   nowarn);

    start           = set_start_address(target_architecture, start_set, start);
    end             = set_end_address(start, blob_size);
    exec            = set_exec_address(exec_set, start, exec, end);

    line_number     = set_line_number(line_number_set, line_number, typable);
    step            = set_step(step_set, step, typable);

    ram_requirement_warning(target_architecture, nowarn, end);

    output_file     = open_output_file(output_filename, print_program);

#define EMITLINEA(A)\
    emit_line(output_file, target_architecture, output_case, &output_file_size,\
    &line_incrementing_has_started, &line_count, &line_number, &step, &line_position,\
    A);

#define EMITLINEB(A,B)\
    emit_line(output_file, target_architecture, output_case, &output_file_size,\
    &line_incrementing_has_started, &line_count, &line_number, &step, &line_position,\
    A, B);

#define EMITLINEC(A,B,C)\
    emit_line(output_file, target_architecture, output_case, &output_file_size,\
    &line_incrementing_has_started, &line_count, &line_number, &step, &line_position,\
    A, B, C);

#define EMITLINEE(A,B,C,D,E)\
    emit_line(output_file, target_architecture, output_case, &output_file_size,\
    &line_incrementing_has_started, &line_count, &line_number, &step, &line_position,\
    A, B, C, D, E);

    if (target_architecture == DRAGON
    || (target_architecture == COCO && extended_basic))
        EMITLINEB(typable ? "CLEAR 200, %d" :
                            "CLEAR200,%d",
                  start - 1)

    if (target_architecture == C64)
        EMITLINEC(typable ? "POKE 55,%d:POKE 56,%d" :
                            "POKE55,%d:POKE56,%d",
                  start%256, start/256)

    if (remarks == 1)
    {
        char          date_text[OUTPUT_TEXT_BUFFER_SIZE];
        time_t        t = 0;
        struct tm     *tm;
        boolean_type  use_date = 1;

        errno = 0;

        t = time(NULL);

        if (t != (time_t) -1 && errno == 0)
        {
            tm = localtime(&t);

            if (tm != NULL)
            {
                if (strftime(date_text, sizeof date_text, "%d %B %Y", tm) == 0)
                {
                    use_date = 0;
                    if (nowarn == 0)
                        warning("Couldn't format date");
                }
            }
            else
            {
                use_date = 0;
                if (nowarn == 0)
                    warning("Couldn't convert the current time to local time");
            }
        }
        else
        {
            use_date = 0;
            if (nowarn == 0)
                warning("Couldn't get the current time. Error number : %d\n", errno);
        }

        EMITLINEA("REM   This program was")
        EMITLINEA("REM generated by BASICloader")
        if (use_date == 1)
        EMITLINEB("REM   on %-15s", date_text)
        EMITLINEA("REM See github.com/")
        EMITLINEA("REM      richardcavell")
    }

    if (typable == 0 && verify == 0)
    {
        EMITLINEE("FORP=%dTO%d:READA:POKEP,A:NEXT:%s%d:END",
                  start, end, exec_command(target_architecture), exec)
    }

    if (typable == 0 && verify == 1)
    {
        EMITLINEC("FORP=%dTO%d:READA:POKEP,A", start, end)
        EMITLINEB("IFA<>PEEK(P)THENGOTO%d",line_number+3*step)
        EMITLINEC("NEXT:%s%d:END", exec_command(target_architecture), exec)
        EMITLINEA("PRINT\"Error!\":END")
    }

    if (typable == 1 && checksum == 0 && verify == 0)
    {
        EMITLINEC("FOR P = %d TO %d", start, end)
        EMITLINEA("READ A")
        EMITLINEA("POKE P,A")
        EMITLINEA("NEXT P")
        EMITLINEC("%s %d", exec_command(target_architecture), exec)
        EMITLINEA("END")
    }

    if (typable == 1 && checksum == 0 && verify == 1)
    {
        EMITLINEC("FOR P = %d TO %d", start, end)
        EMITLINEA("READ A")
        EMITLINEA("POKE P,A")
        EMITLINEB("IF A<>PEEK(P) THEN GOTO %d",line_number+5*step)
        EMITLINEA("NEXT P")
        EMITLINEC("%s %d", exec_command(target_architecture), exec)
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
        EMITLINEC("IF J > %d THEN J = %d", CHECKSUMMED_DATA_PER_LINE, CHECKSUMMED_DATA_PER_LINE)
        EMITLINEA("FOR I = 0 TO J")
        EMITLINEA("READ A")
        EMITLINEA("POKE P,A")
        EMITLINEA("C = C + A")
        EMITLINEA("P = P + 1")
        EMITLINEA("NEXT I")
        EMITLINEB("IF C <> CS THEN GOTO %u", line_number + 5 * step)
        EMITLINEB("IF P < Q THEN GOTO %u", line_number - 11 * step)
        EMITLINEC("%s %u", exec_command(target_architecture), exec)
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
        EMITLINEC("IF J > %d THEN J = %d", CHECKSUMMED_DATA_PER_LINE, CHECKSUMMED_DATA_PER_LINE)
        EMITLINEA("FOR I = 0 TO J")
        EMITLINEA("READ A")
        EMITLINEA("POKE P,A")
        EMITLINEB("IF A<>PEEK(P) THEN GOTO %u", line_number + 12 * step)
        EMITLINEA("C = C + A")
        EMITLINEA("P = P + 1")
        EMITLINEA("NEXT I")
        EMITLINEB("IF C <> CS THEN GOTO %u", line_number + 5 * step)
        EMITLINEB("IF P < Q THEN GOTO %u", line_number - 11 * step)
        EMITLINEC("%s %u", exec_command(target_architecture), exec)
        EMITLINEA("END")
        EMITLINEA("PRINT \"There is an error\"")
        EMITLINEA("PRINT \"on line\";L;\"!\"")
        EMITLINEA("END")
        EMITLINEA("PRINT \"Error while poking memory!\"")
        EMITLINEA("END")
    }

#define EMITDATUM(A) emit_datum(output_file, target_architecture,\
output_case, &output_file_size, typable, &line_incrementing_has_started,\
&line_count, &line_number, &step, &line_position, A);

    if (checksum == 0)
    {
        long int b = blob_size;

        while (b--)
            EMITDATUM(get_character_from_input_file(input_file, input_filename))
    }
    else
    {
        long int b = blob_size;

        while (b != 0)
        {
            unsigned char d[CHECKSUMMED_DATA_PER_LINE];
            unsigned short int i = 0;
            unsigned short int j = 0;
            unsigned long int cs = 0;

            for (cs = 0, i = 0; i < sizeof d && b > 0; ++i, --b)
            {
                d[i] = get_character_from_input_file(input_file, input_filename);
                cs += d[i];
            }

            EMITDATUM((unsigned long int) (line_number + step))
            EMITDATUM(cs)

            for (j = 0; j < i; ++j)
                EMITDATUM(d[j])

            newline_if_needed(output_file,
                              target_architecture,
                              output_case,
                              &output_file_size,
                              &line_count,
                              &line_position);
        }
    }

    newline_if_needed(output_file,
                      target_architecture,
                      output_case,
                      &output_file_size,
                      &line_count,
                      &line_position);

    check_input_file_remainder(input_file,
                               input_file_size,
                               input_file_format,
                               input_filename);

    close_file(output_file, output_filename);
    close_file(input_file,  input_filename);

    if (print_program == 0)
        printf("BASIC program has been generated -> \"%s\"\n", output_filename);

    if (print_diag == 1)
        print_diagnostic_info(target_architecture,
                              output_case,
                              typable,
                              remarks,
                              extended_basic,
                              checksum,
                              start,
                              exec,
                              end,
                              blob_size,
                              line_count,
                              output_file_size);

    return EXIT_SUCCESS;
}

