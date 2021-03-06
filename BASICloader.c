/*
      BASICloader.c

      A program by Richard Cavell (c) 2017-2019
      https://github.com/richardcavell/BASICloader

    Coding standards:

    * We strictly use C89 only
    * No external libraries, no POSIX, no GNU extensions
    * Everything in one file to make building easier
    * Every error that can reasonably be detected, is detected
    * This code is intended to be capable of being compiled with
      retro machines and compilers. Many of these machines and
      compilers are really limited, and predate *all* C standards.

    Ways you can modify this code:

    * Customizable values can be found further down

*/

/* All of these #includes are from the standard library */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* End of #includes */
/* Some important enumerations and associated text #defines */

enum architecture
{
    NO_ARCHITECTURE_CHOSEN = 0,
    COCO,
    DRAGON,
    C64
};

#define   COCO_TEXT "coco"
#define DRAGON_TEXT "dragon"
#define    C64_TEXT "c64"

enum file_format
{
    NO_FILE_FORMAT_CHOSEN = 0,
    BINARY,
    RS_DOS,
    DRAGON_DOS,
    PRG
};

#define     BINARY_TEXT "binary"
#define     RS_DOS_TEXT "rsdos"
#define DRAGON_DOS_TEXT "dragondos"
#define        PRG_TEXT "prg"

static 

enum case_choice    /* Note that "case" is a C keyword */
{
    NO_CASE_CHOSEN = 0,
    UPPERCASE,
    LOWERCASE,
    MIXED_CASE
};

#define  UPPERCASE_TEXT "upper"
#define  LOWERCASE_TEXT "lower"
#define MIXED_CASE_TEXT "mixed"

/* End of enumerations and associated text #defines */
/* Customizable values */

#define DEFAULT_OUTPUT_FILENAME            "LOADER.BAS"
#define DEFAULT_C64LC_OUTPUT_FILENAME      "loader"

#define DEFAULT_ARCHITECTURE               COCO
#define DEFAULT_INPUT_FILE_FORMAT          BINARY
#define DEFAULT_OUTPUT_CASE                UPPERCASE

#define DEFAULT_STARTING_LINE              0
#define DEFAULT_TYPABLE_STARTING_LINE      10
#define MAXIMUM_STARTING_LINE              63000

#define DEFAULT_STEP                       1
#define DEFAULT_TYPABLE_STEP               10
    /*  MINIMUM_STEP is defined below */
#define MAXIMUM_STEP                       60000

#define MAXIMUM_LINE_COUNT                 1000

#define MAXIMUM_LINE_LENGTH                75
#define MAXIMUM_CHECKSUMMED_DATA_PER_LINE  10

#define MAXIMUM_INPUT_FILE_SIZE            65000
#define MAXIMUM_BINARY_SIZE                65000
#define MAXIMUM_BASIC_PROGRAM_SIZE         60000

#define TEXT_BUFFER_SIZE                   300
#define PRINT_WARNINGS_TO_STDERR           0
#define STDOUT_FILENAME_SUBSTITUTE         "-"

#define   COCO_DEFAULT_START               0x3e00
#define DRAGON_DEFAULT_START               0x3e00
#define    C64_DEFAULT_START               0x8000

/* End of customizable values */
/* Values to help check that the customizable values are sensible */

#define MINIMUM_DEFAULT_OUTPUT_FILENAME_LENGTH          1
#define MINIMUM_DEFAULT_C64LC_OUTPUT_FILENAME_LENGTH    1
#define DEFAULT_STARTING_LINES_MUST_BE_POSITIVE         1
#define MINIMUM_STEP                                    1
#define MINIMUM_MAXIMUM_LINE_COUNT                      5
#define MINIMUM_MAXIMUM_LINE_LENGTH                    20
#define MINIMUM_CHECKSUMMED_DATA_PER_LINE               1
#define MINIMUM_MAXIMUM_INPUT_FILE_SIZE                 1
#define MINIMUM_MAXIMUM_BINARY_SIZE                     1
#define MINIMUM_MAXIMUM_BASIC_PROGRAM_SIZE             50
#define MINIMUM_TEXT_BUFFER_SIZE                      100
#define MINIMUM_STDOUT_FILENAME_SUBSTITUTE_LENGTH       1
#define LINE_COUNT_BENCHMARK                          100

/* End of values that help check the customizable values */
/* Architecture-specific values */

#define   COCO_MIN_LINE_NUMBER 0
#define DRAGON_MIN_LINE_NUMBER 0
#define    C64_MIN_LINE_NUMBER 0
#define        MIN_LINE_NUMBER 0

#define   COCO_MAX_LINE_NUMBER 63999
#define DRAGON_MAX_LINE_NUMBER 63999
#define    C64_MAX_LINE_NUMBER 63999
#define        MAX_LINE_NUMBER 63999

/* Not including the \n at the end, or any terminating zero, and
   starting counting from 1 */
#define   COCO_MAX_LINE_LENGTH 249
#define DRAGON_MAX_LINE_LENGTH 249
#define    C64_MAX_LINE_LENGTH  79
#define  MAX_BASIC_LINE_LENGTH 249

#define LOWEST_RAM_ADDRESS     0

#define HIGHEST_COCO_ADDRESS   HIGHEST_64K_ADDRESS
#define HIGHEST_DRAGON_ADDRESS HIGHEST_64K_ADDRESS
#define HIGHEST_C64_ADDRESS    HIGHEST_64K_ADDRESS
#define HIGHEST_RAM_ADDRESS    HIGHEST_64K_ADDRESS

/* End of architecture-specific values */
/* File format-specific values */

#define   RS_DOS_FILE_PREAMBLE_SIZE 5
#define  RS_DOS_FILE_POSTAMBLE_SIZE 5
#define DRAGON_DOS_FILE_HEADER_SIZE 9
#define        PRG_FILE_HEADER_SIZE 2

#define     BINARY_FILE_SIZE_MINIMUM 1
#define     RS_DOS_FILE_SIZE_MINIMUM (RS_DOS_FILE_PREAMBLE_SIZE  \
                                    + RS_DOS_FILE_POSTAMBLE_SIZE \
                                    + 1)
#define DRAGON_DOS_FILE_SIZE_MINIMUM (DRAGON_DOS_FILE_HEADER_SIZE + 1)
#define        PRG_FILE_SIZE_MINIMUM (PRG_FILE_HEADER_SIZE + 1)

/* End of file format-specific values */
/* 8-bit architecture-specific values */

#define HIGHEST_64K_ADDRESS 0xffff
#define HIGHEST_32K_ADDRESS 0x7fff
#define HIGHEST_16K_ADDRESS 0x3fff
#define HIGHEST_8K_ADDRESS  0x1fff
#define HIGHEST_4K_ADDRESS  0x0fff

#define EIGHT_BIT_BYTE_MAX 255

#define FILE_SIZE_MAX 65535

/* End of 8-bit architecture-specific values */
/* Typedefs and limits macros */

typedef unsigned short int boolean_type;
typedef unsigned short int line_number_type;
typedef unsigned short int line_number_step_type;
typedef unsigned short int line_counter_type;
typedef unsigned short int line_position_type;
typedef unsigned short int memory_location_type;
typedef unsigned short int file_size_type;

#define LINE_NUMBER_TYPE_MIN                                 0
#define LINE_NUMBER_TYPE_MAX       (line_number_type)       -1
#define LINE_NUMBER_STEP_TYPE_MAX  (line_number_step_type)  -1
#define LINE_COUNTER_TYPE_MAX      (line_counter_type)      -1
#define LINE_POSITION_TYPE_MAX     (line_position_type)     -1
#define MEMORY_LOCATION_TYPE_MAX   (memory_location_type)   -1
#define FILE_SIZE_TYPE_MAX         (file_size_type)         -1

/* End of typedefs and limits macros */
/* Code starts here */

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
check_uchar_type(void)
{
    if (UCHAR_MAX < EIGHT_BIT_BYTE_MAX)
        internal_error("This machine cannot process 8-bit bytes");
}

static void
check_user_defined_type_limits(void)
{
    if (LINE_NUMBER_TYPE_MIN > MIN_LINE_NUMBER)
        internal_error("Line number type has insufficient lower range");
     
    if (LINE_NUMBER_TYPE_MAX < MAX_LINE_NUMBER)
        internal_error("Line number type has insufficient upper range");

    if (LINE_COUNTER_TYPE_MAX < LINE_COUNT_BENCHMARK)
        internal_error("Line counter type has insufficient range");

    if (LINE_POSITION_TYPE_MAX < COCO_MAX_LINE_LENGTH   ||
        LINE_POSITION_TYPE_MAX < DRAGON_MAX_LINE_LENGTH ||
        LINE_POSITION_TYPE_MAX < C64_MAX_LINE_LENGTH    ||
        LINE_POSITION_TYPE_MAX < MAX_BASIC_LINE_LENGTH)
        internal_error("Line position type has insufficient range");

    if (MEMORY_LOCATION_TYPE_MAX < HIGHEST_RAM_ADDRESS)
        internal_error("Memory location type has insufficient range");

    if (FILE_SIZE_TYPE_MAX < FILE_SIZE_MAX)
        internal_error("File size type has insufficient range");
}

static void
check_default_starting_line(void)
{
    const char macro_name[] = "DEFAULT_STARTING_LINE";

    if (DEFAULT_STARTING_LINE < LINE_NUMBER_TYPE_MIN)
        internal_error("%s\n"
                       "is too low to be represented internally",
                       macro_name);

    if (DEFAULT_STARTING_LINE > LINE_NUMBER_TYPE_MAX)
        internal_error("%s\n"
                       "is too high to be represented internally",
                       macro_name);

    if (DEFAULT_STARTING_LINE < MIN_LINE_NUMBER)
        internal_error("%s\n"
                       "is below the minimum possible BASIC line number",
                       macro_name);

    if (DEFAULT_STARTING_LINE > MAX_LINE_NUMBER)
        internal_error("%s\n"
                       "is above the maximum possible BASIC line number",
                       macro_name);
}

static void
check_default_typable_starting_line(void)
{
    const char macro_name[] = "DEFAULT_TYPABLE_STARTING_LINE";

    if (DEFAULT_TYPABLE_STARTING_LINE < LINE_NUMBER_TYPE_MIN)
        internal_error("%s\n"
                       "is too low to be represented internally",
                       macro_name);

    if (DEFAULT_TYPABLE_STARTING_LINE > LINE_NUMBER_TYPE_MAX)
        internal_error("%s\n"
                       "is too high to be represented internally",
                       macro_name);

    if (DEFAULT_TYPABLE_STARTING_LINE < MIN_LINE_NUMBER)
        internal_error("%s\n"
                       "is below the minimum possible BASIC line number",
                       macro_name);

    if (DEFAULT_TYPABLE_STARTING_LINE > MAX_LINE_NUMBER)
        internal_error("%s\n"
                       "is above the maximum possible BASIC line number",
                       macro_name);
}

static void
check_maximum_starting_line(void)
{
    const char macro_name[] = "MAXIMUM_STARTING_LINE";

    if (MAXIMUM_STARTING_LINE < LINE_NUMBER_TYPE_MIN)
        internal_error("%s\n"
                       "is too low to be represented internally",
                       macro_name);

    if (MAXIMUM_STARTING_LINE > LINE_NUMBER_TYPE_MAX)
        internal_error("%s\n"
                       "is too high to be represented internally",
                       macro_name);

    if (MAXIMUM_STARTING_LINE > ULONG_MAX)
        internal_error("In get_line_number(), %s\n"
                       "cannot be safely converted to unsigned long",
                       macro_name);

    if (MAXIMUM_STARTING_LINE < MIN_LINE_NUMBER)
        internal_error("%s\n"
                       "is below the minimum possible BASIC line number",
                       macro_name);

    if (MAXIMUM_STARTING_LINE > MAX_LINE_NUMBER)
        internal_error("%s\n"
                       "is above the maximum possible BASIC line number",
                       macro_name);
}

static void
check_default_step(void)
{
    const char macro_name[] = "DEFAULT_STEP";

    if (DEFAULT_STEP < MINIMUM_STEP)
        internal_error("%s must be at least %u",
                       macro_name,
                       MINIMUM_STEP);

    if (DEFAULT_STEP > LINE_NUMBER_STEP_TYPE_MAX)
        internal_error("%s cannot be operated on internally",
                       macro_name);
}

static void
check_default_typable_step(void)
{
    const char macro_name[] = "DEFAULT_TYPABLE_STEP";

    if (DEFAULT_TYPABLE_STEP < MINIMUM_STEP)
        internal_error("%s must be at least %u",
                       macro_name,
                       MINIMUM_STEP);

    if (DEFAULT_TYPABLE_STEP > LINE_NUMBER_STEP_TYPE_MAX)
        internal_error("%s cannot be operated on internally",
                       macro_name);
}

static void
check_maximum_step(void)
{
    const char macro_name[] = "MAXIMUM_STEP";

    if (MAXIMUM_STEP < MINIMUM_STEP)
        internal_error("%s must be at least %u",
                       macro_name,
                       MINIMUM_STEP);

    if (MAXIMUM_STEP > ULONG_MAX)
        internal_error("%s cannot be operated on internally\n"
                       "in get_line_number_step()",
                       macro_name);

    if (MAXIMUM_STEP > LINE_NUMBER_STEP_TYPE_MAX)
        internal_error("%s cannot be operated on internally",
                       macro_name);
}

static void
check_maximum_line_count(void)
{
    const char macro_name[] = "MAXIMUM_LINE_COUNT";

    if (MAXIMUM_LINE_COUNT < MINIMUM_MAXIMUM_LINE_COUNT)
        internal_error("%s is too low",
                        macro_name);

    if (MAXIMUM_LINE_COUNT > LINE_COUNTER_TYPE_MAX)
        internal_error("%s cannot be operated on internally",
                        macro_name);
}

static void
check_maximum_line_length(void)
{
    const char macro_name[] = "MAXIMUM_LINE_LENGTH";

    if (MAXIMUM_LINE_LENGTH < MINIMUM_MAXIMUM_LINE_LENGTH)
        internal_error("%s is too low",
                        macro_name);
}

static void
check_maximum_checksummed_data_per_line()
{
    const char macro_name[] = "MAXIMUM_CHECKSUMMED_DATA_PER_LINE";

    if (MAXIMUM_CHECKSUMMED_DATA_PER_LINE
                    < MINIMUM_CHECKSUMMED_DATA_PER_LINE)
        internal_error("%s must be at least %d",
                        macro_name,
                        MINIMUM_CHECKSUMMED_DATA_PER_LINE);

    if (MAXIMUM_CHECKSUMMED_DATA_PER_LINE > USHRT_MAX)
        internal_error("%s cannot be represented internally",
                        macro_name);
}

static void
check_maximum_basic_program_size(void)
{
    const char macro_name[] = "MAXIMUM_BASIC_PROGRAM_SIZE";

    if (MAXIMUM_BASIC_PROGRAM_SIZE
                    < MINIMUM_MAXIMUM_BASIC_PROGRAM_SIZE)
        internal_error("%s is too low",
                        macro_name);

    if (MAXIMUM_BASIC_PROGRAM_SIZE > LONG_MAX)
        internal_error("%s cannot be operated on internally",
                        macro_name);
}

static void
check_maximum_input_file_size(void)
{
    char macro_name[] = "MAXIMUM_INPUT_FILE_SIZE";

    if (MAXIMUM_INPUT_FILE_SIZE < MINIMUM_MAXIMUM_INPUT_FILE_SIZE)
        internal_error("%s is too low. It should be at least %d",
                       macro_name,
                       MINIMUM_MAXIMUM_INPUT_FILE_SIZE);
    
    if (MAXIMUM_INPUT_FILE_SIZE > LONG_MAX)
        internal_error("%s cannot be operated on internally",
                        macro_name);
}

static void
check_maximum_binary_size(void)
{
    const char macro_name[] = "MAXIMUM_BINARY_SIZE";

    if (MAXIMUM_BINARY_SIZE < MINIMUM_MAXIMUM_BINARY_SIZE)
        internal_error("%s is too low. It should be at least %lu",
                        macro_name,
                        (unsigned long int) MINIMUM_MAXIMUM_BINARY_SIZE);

    if (MAXIMUM_BINARY_SIZE > LONG_MAX)
        internal_error("%s cannot be represented internally",
                        macro_name);
}

static void
check_text_buffer_size(void)
{
    const char macro_name[] = "TEXT_BUFFER_SIZE";

    if (TEXT_BUFFER_SIZE < MINIMUM_TEXT_BUFFER_SIZE)
        internal_error("%s is too low. It should be at least %u",
                        macro_name,
                        MINIMUM_TEXT_BUFFER_SIZE);

    if (TEXT_BUFFER_SIZE > INT_MAX)
        internal_error("%s cannot be operated on internally",
                        macro_name);
}

static void
check_default_starts(void)
{
    if (COCO_DEFAULT_START > HIGHEST_COCO_ADDRESS)
        internal_error("COCO_DEFAULT_START is higher than\n"
                       "is possible on the Color Computer");

    if (DRAGON_DEFAULT_START > HIGHEST_DRAGON_ADDRESS)
        internal_error("DRAGON_DEFAULT_START is higher than\n"
                       "is possible on the Dragon");

    if (C64_DEFAULT_START > HIGHEST_C64_ADDRESS)
        internal_error("C64_DEFAULT_START is higher than\n"
                       "is possible on the Commodore 64");
}

static void
error(const char *fmt, ...)
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
error_while_printing_warning(void)
{
    error("Couldn't print warning to standard output");
}

static void
warning(const char *fmt, ...)
{
    FILE *stream = PRINT_WARNINGS_TO_STDERR ? stderr : stdout;
    va_list ap;
    int vfprintf_return_value = 0;

    if (fprintf(stream, "Warning: ") < 0)
        error_while_printing_warning();

    va_start(ap, fmt);
    vfprintf_return_value = vfprintf(stream, fmt, ap);
    va_end(ap);

    if (vfprintf_return_value < 0)
        error_while_printing_warning();

    if (fprintf(stream, "\n") < 0)
        error_while_printing_warning();
}

static void
xputs(const char *s)
{
    if (puts(s) == EOF)
        error("Couldn't print string to standard output");
}

static void
xprintf(const char *fmt, ...)
{
    va_list ap;
    int vprintf_return_value = 0;

    va_start(ap, fmt);
    vprintf_return_value = vprintf(fmt, ap);
    va_end(ap);

    if (vprintf_return_value < 0)
        error("Couldn't print formatted string to standard output");
}

static void
print_version_text(void)
{
    xputs("BASICloader (under development)");
    xputs("(c) 2017 Richard Cavell");
}

static void
print_version(void)
{
    print_version_text();
    exit(EXIT_SUCCESS);
}

static void
print_help_text(void)
{
      print_version_text();
      xputs("https://github.com/richardcavell/BASICloader");
      xputs("");
      xputs("Usage: BASICloader [options] [filename]");
      xputs("");
      xputs("  -o  --output    Output filename");
    xprintf("  -m  --machine   Target machine (%s/%s/%s)\n",
                                               COCO_TEXT,
                                               DRAGON_TEXT,
                                               C64_TEXT);
    xprintf("  -f  --format    Input file format (%s/%s/%s/%s)\n",
                                                  BINARY_TEXT,
                                                  RS_DOS_TEXT,
                                                  DRAGON_DOS_TEXT,
                                                  PRG_TEXT);
    xprintf("  -c  --case      Output case (%s/%s)\n",
                                            UPPERCASE_TEXT,
                                            LOWERCASE_TEXT);
      xputs("  -t  --typable   Human-readable and human-typable");
      xputs("  -r  --remarks   Add remarks and date");
    xprintf("  -x  --extbas    Assume Extended Color BASIC (%s only)\n",
                                                            COCO_TEXT);
      xputs("  -k  --checksum  Calculate and use checksums");
      xputs("  -s  --start     Start memory location");
      xputs("  -e  --exec      Exec memory location");
      xputs("  -p  --print     Print the BASIC program to standard output");
      xputs("  -n  --nowarn    Turn warnings off");
      xputs("  -l  --license   Your license to use this program");
      xputs("  -i  --info      What this program does");
      xputs("  -h  --help      This help information");
      xputs("  -v  --version   Version number");
      xputs("  -a  --allopt    Show all options");
}

static void
print_help(void)
{
    print_help_text();
    exit(EXIT_SUCCESS);
}

static void
print_all_options(void)
{
    print_help_text();
    xputs("      --defaults  Print the default values for some switches");
    xputs("      --line      Starting line number");
    xputs("      --step      Line number step");
    xputs("      --diag      Print diagnostic information");
    xputs("      --majorv    Major version number");
    xputs("      --minorv    Minor version number");
    xputs("      --stdin     Read machine language file from standard input");
    exit(EXIT_SUCCESS);
}

static void
print_info(void)
{
    print_version_text();
    xputs("");
    xputs("BASICloader generates programs similar to the type-in programs");
    xputs("from 1980s computer magazines.");
    xputs("");
    xputs("It reads in a machine language program that is intended for one");
    xputs("of the target machines, and then constructs a BASIC program");
    xputs("that will run on that target machine.");
    xputs("");
    xputs("The BASIC program will contain a loop and some DATA statements.");
    xputs("When run, it will poke the machine language into memory, and");
    xputs("then execute it.");
    exit(EXIT_SUCCESS);
}

static void
print_license(void)
{
    xputs("BASICloader License");
    xputs("");
    xputs("(modified MIT License)");
    xputs("");
    xputs("Copyright (c) 2017 Richard Cavell");
    xputs("");
    xputs("Permission is hereby granted, free of charge,"
                   " to any person obtaining a copy");
    xputs("of this software and associated documentation files"
                   " (the \"Software\"), to deal");
    xputs("in the Software without restriction, including without"
                   " limitation the rights");
    xputs("to use, copy, modify, merge, publish, distribute,"
                   " sublicense, and/or sell");
    xputs("copies of the Software, and to permit persons to whom"
                   " the Software is");
    xputs("furnished to do so, subject to the following conditions:");
    xputs("");
    xputs("The above copyright notice and this permission notice"
                   " shall be included in all");
    xputs("copies or substantial portions of the Software.");
    xputs("");
    xputs("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY"
                   " OF ANY KIND, EXPRESS OR");
    xputs("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES"
                   " OF MERCHANTABILITY,");
    xputs("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT."
                   " IN NO EVENT SHALL THE");
    xputs("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,"
                   " DAMAGES OR OTHER");
    xputs("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR"
                   " OTHERWISE, ARISING FROM,");
    xputs("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE"
                   " OR OTHER DEALINGS IN THE");
    xputs("SOFTWARE.");
    xputs("");
    xputs("The output of this program is licensed to you under the"
                   " following license:");
    xputs("");
    xputs("1.  You may use the output of this program, for free,"
                   " for any worthwhile");
    xputs("    or moral purpose.");
    xputs("2.  You should try to attribute me and the BASICloader"
                   " program, where");
    xputs("    that is not unreasonable.");
    xputs("");
    xputs("You should not allow people to assume that you wrote"
                   " the BASIC code yourself.");
    exit(EXIT_SUCCESS);
}

static const char *
architecture_to_text(enum architecture target_architecture)
{
    const char *text = NULL;

    switch(target_architecture)
    {
        case COCO:    text = COCO_TEXT;    break;
        case DRAGON:  text = DRAGON_TEXT;  break;
        case C64:     text = C64_TEXT;     break;
        default:      internal_error("Unhandled architecture in"
                                     " architecture_to_text()");
    }

    return text;
}

static const char *
format_to_text(enum file_format format)
{
    const char *text = NULL;

    switch(format)
    {
        case BINARY:      text = BINARY_TEXT;      break;
        case RS_DOS:      text = RS_DOS_TEXT;      break;
        case DRAGON_DOS:  text = DRAGON_DOS_TEXT;  break;
        case PRG:         text = PRG_TEXT;         break;
        default:          internal_error("Unhandled format in"
                                         " format_to_text()");
    }

    return text;
}

static const char *
case_to_text(enum case_choice output_case)
{
    const char *text = NULL;

    switch(output_case)
    {
        case UPPERCASE:   text = UPPERCASE_TEXT;   break;
        case LOWERCASE:   text = LOWERCASE_TEXT;   break;
        case MIXED_CASE:  text = MIXED_CASE_TEXT;  break;
        default:          internal_error("Unhandled case in"
                                         " case_to_text()");
    }

    return text;
}

static void
print_defaults(void)
{
    xprintf("Output filename    : \"%s\"\n",
                    DEFAULT_OUTPUT_FILENAME);
    xprintf("                     \"%s\""
                    " (with --machine %s --case %s)\n",
                    DEFAULT_C64LC_OUTPUT_FILENAME,
                    C64_TEXT,
                    LOWERCASE_TEXT);
    xprintf("Target machine     : %s\n",
                    architecture_to_text(DEFAULT_ARCHITECTURE));
    xprintf("Input file format  : %s\n",
                    format_to_text(DEFAULT_INPUT_FILE_FORMAT));
    xprintf("Output case        : %s\n",
                    case_to_text(DEFAULT_OUTPUT_CASE));
    xprintf("Start location     : Hex=0x%lx Dec=%lu (%s)\n",
                    (unsigned long int) COCO_DEFAULT_START,
                    (unsigned long int) COCO_DEFAULT_START,
                    COCO_TEXT);
    xprintf("                   : Hex=0x%lx Dec=%lu (%s)\n",
                    (unsigned long int) DRAGON_DEFAULT_START,
                    (unsigned long int) DRAGON_DEFAULT_START,
                    DRAGON_TEXT);
    xprintf("                   : Hex=0x%lx Dec=%lu (%s)\n",
                    (unsigned long int) C64_DEFAULT_START,
                    (unsigned long int) C64_DEFAULT_START,
                    C64_TEXT);
      xputs("Exec location      : (Equal to start location)");
    xprintf("Starting line      : %lu\n",
                    (unsigned long int) DEFAULT_STARTING_LINE);
    xprintf("                   : %lu (with --typable)\n",
                    (unsigned long int) DEFAULT_TYPABLE_STARTING_LINE);
    xprintf("Starting step      : %lu\n",
                    (unsigned long int) DEFAULT_STEP);
    xprintf("                   : %lu (with --typable)\n",
                    (unsigned long int) DEFAULT_TYPABLE_STEP);
    exit(EXIT_SUCCESS);
}

static boolean_type
string_match(const char *actual_arg,
             const char *option_text)
{
    return actual_arg    != NULL &&
           option_text   != NULL &&
           strcmp(actual_arg, option_text) == 0;
}

static boolean_type
arg2_match(const char *arg_text,
           const char *short_option_text,
           const char *long_option_text)
{
    return string_match(arg_text, short_option_text) ||
           string_match(arg_text, long_option_text);
}

static int
is_option(const char *arg)
{
    const char dash = '-';
    return arg[0] == dash;
}

static const char *
get_output_filename(const char *arg1,
                    const char *arg2,
                    const char *output_filename)
{
    if (output_filename != NULL)
        error("You can only set option %s once", arg1);

    if (arg2 == NULL)
        error("You must supply a filename after %s", arg1);

    return arg2;
}

static enum architecture
get_target_architecture(const char         *arg1,
                        const char         *arg2,
                        enum architecture  target_architecture)
{
    if (target_architecture != NO_ARCHITECTURE_CHOSEN)
        error("You can only set %s once", arg1);

    if (arg2 == NULL)
        error("You must name a machine after %s", arg1);

         if (string_match(arg2, COCO_TEXT))    target_architecture = COCO;
    else if (string_match(arg2, DRAGON_TEXT))  target_architecture = DRAGON;
    else if (string_match(arg2, C64_TEXT))     target_architecture = C64;
    else error("Unknown target architecture \"%s\"", arg2);

    return target_architecture;
}

static enum file_format
get_format(const char        *arg1,
           const char        *arg2,
           enum file_format  format)
{
    if (format != NO_FILE_FORMAT_CHOSEN)
        error("You can only set %s once", arg1);

    if (arg2 == NULL)
        error("You must name a file format after %s", arg1);

         if (string_match(arg2, BINARY_TEXT))      format = BINARY;
    else if (string_match(arg2, RS_DOS_TEXT))      format = RS_DOS;
    else if (string_match(arg2, DRAGON_DOS_TEXT))  format = DRAGON_DOS;
    else if (string_match(arg2, PRG_TEXT))         format = PRG;
    else error("Unknown file format \"%s\"", arg2);

    return format;
}

static enum case_choice
get_case(const char        *arg1,
         const char        *arg2,
         enum case_choice  output_case)
{
    if (output_case != NO_CASE_CHOSEN)
        error("You can only set %s once", arg1);

    if (arg2 == NULL)
        error("You must name a case choice after %s", arg1);

         if (string_match(arg2, UPPERCASE_TEXT))   output_case = UPPERCASE;
    else if (string_match(arg2, LOWERCASE_TEXT))   output_case = LOWERCASE;
    else if (string_match(arg2, MIXED_CASE_TEXT))  output_case = MIXED_CASE;
    else error("Unknown case \"%s\"", arg2);

    return output_case;
}

static boolean_type
get_switch_state(const char    *arg,
                 boolean_type  sw)
{
    if (sw != 0)
        error("Option %s has already been set", arg);

    sw = 1;

    return sw;
}

static unsigned long int
arg_to_unsigned_long(const char         *pstring,
                     boolean_type       *ok,
                     unsigned long int  min,
                     unsigned long int  max)
{
    unsigned long int l = 0;
    char *endptr = NULL;
    int base = 0;

    errno = 0;

    if (pstring != NULL)
        while (*pstring == ' ')
            ++pstring;

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
            &&  l >= min
            &&  l <= max );

    return l;
}

static line_number_type
get_line_number(const char        *arg1,
                const char        *arg2,
                line_number_type  line_number,
                boolean_type      *line_set)
{
    boolean_type ok = 0;

    if (*line_set != 0)
        error("Option %s can only be set once", arg1);

    line_number = (line_number_type)
                   arg_to_unsigned_long(arg2,
                                        &ok,
                                        MIN_LINE_NUMBER,
                                        MAXIMUM_STARTING_LINE);

    if (ok == 0)
        error("%s takes a number from %i to %lu",
                                   arg1,
                      (signed int) MIN_LINE_NUMBER,
               (unsigned long int) MAXIMUM_STARTING_LINE);

    *line_set = 1;

    return line_number;
}

static line_number_step_type
get_line_number_step(const char             *arg1,
                     const char             *arg2,
                     line_number_step_type  step,
                     boolean_type           *step_set)
{
    boolean_type ok = 0;

    if (*step_set != 0)
        error("Option %s can only be set once", arg1);

    step = (line_number_step_type)
            arg_to_unsigned_long(arg2,
                                 &ok,
                                 MINIMUM_STEP,
                                 MAXIMUM_STEP);

    if (ok == 0)
        error("%s takes a number from %lu to %lu",
                                   arg1,
               (unsigned long int) MINIMUM_STEP,
               (unsigned long int) MAXIMUM_STEP);

    *step_set = 1;

    return step;
}

static memory_location_type
get_highest_ram_address(enum architecture target_architecture)
{
    memory_location_type highest_ram_address = HIGHEST_RAM_ADDRESS;

    switch(target_architecture)
    {
        case COCO:
            highest_ram_address = HIGHEST_COCO_ADDRESS;
            break;

        case DRAGON:
            highest_ram_address = HIGHEST_DRAGON_ADDRESS;
            break;

        case C64:
            highest_ram_address = HIGHEST_C64_ADDRESS;
            break;

        default:
            internal_error("Unhandled target architecture in"
                           " get_highest_ram_address()");
            break;
    }

    return highest_ram_address;
}

static memory_location_type
get_memory_location_type_arg(const char            *arg1,
                             const char            *arg2,
                             memory_location_type  mem,
                             boolean_type          *set)
{
    boolean_type ok = 0;

    if (*set != 0)
        error("Option %s can only be set once", arg1);

    mem = (memory_location_type)
            arg_to_unsigned_long(arg2,
                                 &ok,
                                 LOWEST_RAM_ADDRESS,
                                 HIGHEST_RAM_ADDRESS);

    if (ok == 0)
        error("%s takes a number from 0x%lx to 0x%lx",
               arg1,
               (unsigned long int) LOWEST_RAM_ADDRESS,
               (unsigned long int) HIGHEST_RAM_ADDRESS);

    *set = 1;

    return mem;
}

static enum architecture
set_target_architecture(enum architecture target_architecture)
{
    if (target_architecture == NO_ARCHITECTURE_CHOSEN)
        target_architecture =  DEFAULT_ARCHITECTURE;

    return target_architecture;
}

static void
check_input_file_format(enum architecture target_architecture,
                        enum file_format  input_file_format)
{
    if (input_file_format == PRG         && target_architecture != C64)
        error("File format \"%s\" should only be used"
              " with the \"%s\" target",
              PRG_TEXT,
              C64_TEXT);

    if (input_file_format == DRAGON_DOS  && target_architecture != DRAGON)
        error("File format \"%s\" should only be used"
              " with the \"%s\" target",
             DRAGON_DOS_TEXT,
             DRAGON_TEXT);

    if (input_file_format == RS_DOS      && target_architecture != COCO)
        error("File format \"%s\" should only be used"
              " with the \"%s\" target",
             RS_DOS_TEXT,
             COCO_TEXT);
}

static enum file_format
set_input_file_format(enum architecture  target_architecture,
                      enum file_format   input_file_format)
{
    if (input_file_format == NO_FILE_FORMAT_CHOSEN)
        input_file_format =  DEFAULT_INPUT_FILE_FORMAT;

    check_input_file_format(target_architecture, input_file_format);

    return input_file_format;
}

static void
check_output_case(enum architecture  target_architecture,
                  enum case_choice   output_case)
{
    if (output_case == LOWERCASE && target_architecture == COCO)
        error("Lowercase output is not useful"
              " for the \"%s\" target", COCO_TEXT);

    if (output_case == LOWERCASE && target_architecture == DRAGON)
        error("Lowercase output is not useful"
              " for the \"%s\" target", DRAGON_TEXT);

    if (output_case == MIXED_CASE)
        error("Mixed case output cannot be run on any current"
                        "target architecture");
}

static enum case_choice
set_output_case(enum architecture  target_architecture,
                enum case_choice   output_case)
{
    if (output_case == NO_CASE_CHOSEN)
        output_case =  DEFAULT_OUTPUT_CASE;

    check_output_case(target_architecture, output_case);

    return output_case;
}

static boolean_type
set_typable(boolean_type  typable,
            boolean_type  use_checksum)
{
    if (use_checksum)
        typable = 1;
    
    return typable;
}

static boolean_type
set_print_program(const char    *output_filename,
                  boolean_type  print_program)
{
    if (output_filename != NULL &&
        strcmp(output_filename, STDOUT_FILENAME_SUBSTITUTE) == 0)
           print_program = 1;

    return print_program;
}

static boolean_type
set_nowarn(boolean_type  no_warn,
           boolean_type  print_program)
{
    if (print_program)
        no_warn = 1;

    return no_warn;
}

static void
check_extended_basic(enum architecture  target_architecture,
                     boolean_type       extended_basic)
{
    if (extended_basic && target_architecture != COCO)
        error("Extended Color BASIC option should only be used"
             " with the \"%s\" target", COCO_TEXT);
}

static void
check_print_options(boolean_type  print_program,
                    boolean_type  print_diag)
{
    if (print_program != 0 && print_diag != 0)
        error("--print and --diag options cannot be used together");
}

static void
check_input_filename(const char    *input_filename,
                     boolean_type  read_stdin)
{
    if (input_filename == NULL && read_stdin == 0)
        error("You must specify an input file");

    if (input_filename != NULL && read_stdin != 0)
        error("You cannot give an input filename while using --stdin");
}

static void
check_output_filename(const char    *output_filename,
                      boolean_type  print_program)
{
    if (print_program   != 0    &&
        output_filename != NULL &&
        strcmp(output_filename, STDOUT_FILENAME_SUBSTITUTE) != 0)
        error("You cannot specify an output filename while using --print");
}

static const char *
set_output_filename(enum architecture  target_architecture,
                    enum case_choice   output_case,
                    const char         *output_filename,
                    boolean_type       print_program)
{
    if (print_program == 0 && output_filename == NULL)
    {
        if (target_architecture == C64 && output_case == LOWERCASE)
            output_filename = DEFAULT_C64LC_OUTPUT_FILENAME;
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

    if (read_stdin != 0)
    {
        input_file = stdin;
    }
    else
    {
        errno = 0;

        input_file = fopen(input_filename, "rb");

        if (input_file == NULL)
            error("Could not open file \"%s\". Error number %d",
                                         input_filename,    errno);
    }

    return input_file;
}

static long int
get_file_position(FILE        *file,
                  const char  *filename)
{
    long int file_size = 0;

    errno = 0;

    file_size = ftell(file);

    if (file_size < 0)
        error("Could not get size of file \"%s\". Error number %d",
                                            filename,          errno);

    return file_size;
}

static long int
get_input_file_size_min(enum file_format input_file_format)
{
    long int input_file_size_min = 0;

    switch(input_file_format)
    {
        case BINARY:
            input_file_size_min = BINARY_FILE_SIZE_MINIMUM;
            break;

        case RS_DOS:
            input_file_size_min = RS_DOS_FILE_SIZE_MINIMUM;
            break;

        case DRAGON_DOS:
            input_file_size_min = DRAGON_DOS_FILE_SIZE_MINIMUM;
            break;

        case PRG:
            input_file_size_min = PRG_FILE_SIZE_MINIMUM;
            break;

        default:
            error("Unhandled file format type in get_input_file_size_min()");
    }

    return input_file_size_min;
}

static void
check_input_file_size(long int          input_file_size,
                      const char        *input_filename,
                      enum file_format  input_file_format)
{
    if (input_file_size == 0)
        error("File \"%s\" is empty", input_filename);

    if (input_file_size < get_input_file_size_min(input_file_format))
        error("Input file \"%s\" is too short. Minimum file size for\n"
              "file format \"%s\" is %ld bytes, but input file is %ld bytes",
              input_filename,
              format_to_text(input_file_format),
              get_input_file_size_min(input_file_format),
              input_file_size);

    if (input_file_size > MAXIMUM_INPUT_FILE_SIZE)
        error("Input file \"%s\" is too large (maximum is %ld bytes",
                            input_filename,
                            (long int) MAXIMUM_INPUT_FILE_SIZE);
}

static long int
get_input_file_size(FILE              *input_file,
                    const char        *input_filename,
                    enum file_format  input_file_format)
{
    long int input_file_size = 0;

    errno = 0;

    if (fseek(input_file, 0L, SEEK_END) != 0)
        error("Could not find the end of file \"%s\". Error number %d",
                                                input_filename, errno);

    input_file_size = get_file_position(input_file, input_filename);

    if (fseek(input_file, 0L, SEEK_SET) != 0)
        error("Could not rewind file \"%s\". Error number %d",
                                       input_filename,    errno);

    check_input_file_size(input_file_size, input_filename, input_file_format);

    return input_file_size;
}

static void
check_blob_size(enum architecture  target_architecture,
                long int           blob_size,
                const char         *input_filename)
{
    long int max = MAXIMUM_BINARY_SIZE < 
                   get_highest_ram_address(target_architecture) ?
                   get_highest_ram_address(target_architecture) :
                   MAXIMUM_BINARY_SIZE;

    if (blob_size == 0)
        error("Input file \"%s\" contains no machine language content",
                            input_filename);

    if (blob_size > max)
        error("The machine language content of input file \"%s\" is"
              " too large\n"
              "(Maximum allowed is %lu)",
              input_filename,
              max);
}

static long int
calculate_blob_size(enum file_format  input_file_format,
                    long int          input_file_size)
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
            blob_size = input_file_size - (RS_DOS_FILE_PREAMBLE_SIZE
                                             + RS_DOS_FILE_POSTAMBLE_SIZE);
            break;

        default:
            internal_error("Unhandled file format in calculate_blob_size()");
            break;
    }

    return blob_size;
}

static unsigned char
getc_safe(FILE        *input_file,
          const char  *input_filename)
{
    int c = EOF;

    errno = 0;

    c = fgetc(input_file);

    if (c > EIGHT_BIT_BYTE_MAX)
        error("Byte read from input file \"%s\" is too high"
             " for an 8-bit machine", input_filename);

    if (c == EOF)
    {
         int eno = errno;

         if (feof(input_file))
         {
             error("Unexpected end of file while reading file \"%s\"",
                   input_filename);
         }
         else
         {
             error("Unexpected error while reading file \"%s\"\n"
                   "Error code %d",
                   input_filename,
                   eno);
         }
    }

    return (unsigned char) c;
}

static void
clear_uchar_array(unsigned char *p, size_t n)
{
    (void) memset(p, 0, n);
}

static void
getc_array(unsigned char  arr[],
           size_t         size,
           FILE           *input_file,
           const char     *input_filename)
{
    unsigned short int i = 0;

    clear_uchar_array(arr, size);

    for (i = 0; i < size; ++i)
        arr[i] = getc_safe(input_file, input_filename);
}

static memory_location_type
construct_16bit_value(unsigned char hi, unsigned char lo)
{
    return (memory_location_type) (hi * 256 + lo);
}

static void
set_file_position_error(const char *input_filename)
{
    error("Couldn't set file position indicator on file \"%s\".\n"
          "Error number %d",
           input_filename,
           errno);
}

static void
set_file_position_from_start(FILE        *input_file,
                             const char  *input_filename,
                             long int    position)
{
    if (fseek(input_file, position, SEEK_SET)
         < 0)
        set_file_position_error(input_filename);
}

static void
set_file_position_zero(FILE        *input_file,
                       const char  *input_filename)
{
    set_file_position_from_start(input_file, input_filename, 0);
}

static void
set_file_position_from_end(FILE        *input_file,
                           const char  *input_filename,
                           long int    position)
{
    if (fseek(input_file, -1 * position, SEEK_END)
         < 0)
        set_file_position_error(input_filename);
}

struct file_info
{
    boolean_type          start_given;
    memory_location_type  start;

    boolean_type          exec_given;
    memory_location_type  exec;

    boolean_type          length_given;
    file_size_type        length;
};

struct file_info
empty_file_info(void)
{
    struct file_info fi;

    fi.start_given     = 0;
    fi.start           = 0;

    fi.exec_given      = 0;
    fi.exec            = 0;

    fi.length_given    = 0;
    fi.length          = 0;

    return fi;
}

static struct file_info
process_rs_dos_header(FILE              *input_file,
                      const char        *input_filename,
                      struct file_info  fi)
{
    unsigned char preamble[RS_DOS_FILE_PREAMBLE_SIZE]  = { 0 };

    set_file_position_zero(input_file, input_filename);

    getc_array(preamble,
               sizeof preamble, 
               input_file, 
               input_filename);

    if (preamble[0] != 0x0)
        error("Input file \"%s\" is not properly formed\n"
              "as an RS-DOS file (bad header)", input_filename);

    fi.length_given = 1;
    fi.length       = construct_16bit_value(preamble[1], preamble[2]);

    fi.start_given  = 1;
    fi.start        = construct_16bit_value(preamble[3], preamble[4]);

    return fi;
}

static struct file_info
process_rs_dos_tail(FILE              *input_file,
                    const char        *input_filename,
                    struct file_info  fi)
{
    unsigned char postamble[RS_DOS_FILE_POSTAMBLE_SIZE] = { 0 };

    set_file_position_from_end(input_file,
                               input_filename, 
                               RS_DOS_FILE_POSTAMBLE_SIZE);

    getc_array(postamble,
               sizeof postamble,
               input_file,
               input_filename);

    if (postamble[0] == 0x0)
        error("Input RS-DOS file \"%s\" is segmented, and cannot be used",
                                   input_filename);

    if (postamble[0] != 0xff ||
        postamble[1] != 0x0  ||
        postamble[2] != 0x0)
      error("Input file \"%s\" is not properly formed\n"
            "as an RS-DOS file (bad tail)",
             input_filename);

    fi.exec_given = 1;
    fi.exec       = construct_16bit_value(postamble[3], postamble[4]);

    return fi;
} 

static struct file_info 
process_rs_dos_file_format(FILE        *input_file,
                           const char  *input_filename)
{
    struct file_info fi = empty_file_info();

    fi = process_rs_dos_header(input_file, input_filename, fi);
    fi = process_rs_dos_tail  (input_file, input_filename, fi);

    set_file_position_from_start(input_file,
                                 input_filename,
                                 RS_DOS_FILE_PREAMBLE_SIZE);

    return fi;
}

static void
check_dragon_dos_filetype(const char     *input_filename,
                          unsigned char  file_type)
{
    switch(file_type)
    {
        case 0x1:
            error("Input Dragon DOS file \"%s\" appears\n"
                  "to be a BASIC program",
                                           input_filename);
            break;

        case 0x2: /* Success */
            break;

        case 0x3:
            error("Input Dragon DOS file \"%s\"\n"
                  "is an unsupported file (possibly DosPlus)\n",
                                           input_filename);
            break;

        default:
            error("Input Dragon DOS file \"%s\""
                  "has an unknown FILETYPE ($%x)\n",
                                           input_filename,
                                           (unsigned int) file_type);
            break;
    }
}

static struct file_info
process_dragon_dos_file_format(FILE        *input_file,
                               const char  *input_filename)
{
    unsigned char header[DRAGON_DOS_FILE_HEADER_SIZE] = { 0 };
    struct file_info fi = empty_file_info();

    set_file_position_zero(input_file, input_filename);

    getc_array(header,
               sizeof header,
               input_file,
               input_filename);

    if (header[0] != 0x55 || header[8] != 0xAA)
        error("Input file \"%s\" doesn't appear to be a Dragon DOS file",
                            input_filename);

    check_dragon_dos_filetype(input_filename, header[1]);

    fi.length_given = 1;
    fi.length       = construct_16bit_value(header[4], header[5]);

    fi.start_given  = 1;
    fi.start        = construct_16bit_value(header[2], header[3]);

    fi.exec_given   = 1;
    fi.exec         = construct_16bit_value(header[6], header[7]);

    set_file_position_from_start(input_file,
                                 input_filename,
                                 DRAGON_DOS_FILE_HEADER_SIZE);

    return fi;
}

static struct file_info
process_prg_file_format(FILE        *input_file,
                        const char  *input_filename)
{
    unsigned char header[PRG_FILE_HEADER_SIZE] = { 0 };
    struct file_info fi = empty_file_info();

    set_file_position_zero(input_file, input_filename);

    getc_array(header,
               sizeof header,
               input_file,
               input_filename);

    fi.start_given = 1;
    fi.start       = construct_16bit_value(header[1], header[0]);

    if (fi.start == 0x0801)
        error("Input PRG file \"%s\" is unsuitable for use with BASICloader\n"
              "It appears to be a BASIC program, or a hybrid\n"
              "BASIC/machine language program. (This is a common issue).",
               input_filename);

    fi.exec_given   = 0;
    fi.length_given = 0;

    set_file_position_from_start(input_file,
                                 input_filename,
                                 PRG_FILE_HEADER_SIZE);

    return fi;
}

static struct file_info
process_binary_file_format(FILE        *input_file,
                           const char  *input_filename)
{
    struct file_info fi = empty_file_info();

    set_file_position_zero(input_file, input_filename);

    fi.start_given  = 0;
    fi.exec_given   = 0;
    fi.length_given = 0;

    set_file_position_zero(input_file, input_filename);

    return fi;
}

static struct file_info
extract_file_info(enum file_format   input_file_format,
                  FILE               *input_file,
                  const char         *input_filename)
{
    struct file_info fi = empty_file_info();

    switch(input_file_format)
    {
        case BINARY:
            fi = process_binary_file_format(input_file,
                                            input_filename);
            break;

        case RS_DOS:
            fi = process_rs_dos_file_format(input_file,
                                            input_filename);
            break;

        case DRAGON_DOS:
            fi = process_dragon_dos_file_format(input_file,
                                                input_filename);
            break;

        case PRG:
            fi = process_prg_file_format(input_file,
                                         input_filename);
            break;

        default:
            internal_error("Unhandled file format in"
                           " process_file_format()");
            break;
    }

    return fi;
}

static void
process_file_format(enum file_format      input_file_format,
                    FILE                  *input_file,
                    const char            *input_filename,
                    boolean_type          *start_set,
                    memory_location_type  *start,
                    boolean_type          *exec_set,
                    memory_location_type  *exec,
                    long int              blob_size)
{
    struct file_info fi = extract_file_info(input_file_format,
                                            input_file,
                                            input_filename);

    if (*start_set      != 0 &&
         fi.start_given != 0 &&
         fi.start       != *start)
      error("Input file \"%s\" gives a different start address ($%lx)\n"
           "to the one given at the command line ($%lx)",
                               input_filename,
                               (unsigned long int) fi.start,
                               (unsigned long int) *start);

     if (fi.start_given != 0 &&
             *start_set == 0)
     {
         *start_set = 1;
         *start = fi.start;
     }

     if (*exec_set     != 0 &&
         fi.exec_given != 0 &&
         fi.exec       != *exec)
         error("Input file \"%s\" gives a different exec address ($%lx)\n"
               "to the one given at the command line ($%lx)",
                          input_filename,
                          (unsigned long int) fi.exec,
                          (unsigned long int) *exec);

    if (fi.exec_given != 0 &&
            *exec_set == 0)
    {
        *exec_set = 1;
        *exec = fi.exec;
    }

    if (fi.length_given != 0 &&
        fi.length       != blob_size)
        error("Input file \"%s\" gives a different \"blob size\" ($%lx)\n"
              "to the measured blob size ($%lx)",
                            input_filename,
                            (unsigned long int) fi.length,
                            (unsigned long int) blob_size);
}

static memory_location_type
set_start_address(enum architecture     target_architecture,
                  boolean_type          *start_set,
                  memory_location_type  start)
{
    if (*start_set == 0)
    {
        switch(target_architecture)
        {
            case COCO:
                start = COCO_DEFAULT_START;
                break;

            case DRAGON:
                start = DRAGON_DEFAULT_START;
                break;

            case C64:
                start = C64_DEFAULT_START;
                break;

            default:
                internal_error("Unhandled target architecture"
                               " in set_start_address()");
        }

        *start_set = 1;
    }

    return start;
}

static void
check_start_address(enum architecture     target_architecture,
                    memory_location_type  start)
{
    if (start > get_highest_ram_address(target_architecture))
        internal_error("Start location is higher than the highest possible"
                       " RAM address\n"
                       "on the %s architecture",
                        architecture_to_text(target_architecture));
}

static memory_location_type
set_end_address(enum architecture     target_architecture,
                memory_location_type  start,
                long int              blob_size)
{
    memory_location_type end = (memory_location_type)
                               (start + blob_size - 1);

    if (end < start)
        error("The machine language blob will not fit in the RAM\n"
             "of the %s architecture",
             architecture_to_text(target_architecture));

    return end;
}

static void
check_end_address(enum architecture     target_architecture,
                  memory_location_type  end)
{
    if (end > get_highest_ram_address(target_architecture))
        error("The machine language blob would overflow the amount of RAM\n"
             "on the %s architecture",
             architecture_to_text(target_architecture));
}

static void
check_exec_address(enum architecture     target_architecture,
                   memory_location_type  exec,
                   memory_location_type  start,
                   memory_location_type  end)
{
    if (exec > get_highest_ram_address(target_architecture))
        internal_error("Exec location is higher than the highest possible"
                       " RAM address\n"
                       "on the \"%s\" target machine",
                       architecture_to_text(target_architecture));

    if (exec < start)
        error("The exec location ($%lx) is below\n"
              "the start location of the binary blob ($%lx)",
              (unsigned long int) exec,
              (unsigned long int) start);

    if (exec > end)
        error("The exec location ($%lx) is beyond\n"
              "the end location of the binary blob ($%lx)",
              (unsigned long int) exec,
              (unsigned long int) end);
}

static memory_location_type
set_exec_address(boolean_type          *exec_set,
                 memory_location_type  start,
                 memory_location_type  exec)
{
    if (*exec_set == 0)
    {
        *exec_set = 1;
        exec      = start;
    }

    return exec;
}

static FILE *
open_output_file(const char    *output_filename,
                 boolean_type  print_program)
{
    FILE *output_file = NULL;

    if (print_program != 0)
        output_file = stdout;
    else
    {
        output_file = fopen(output_filename, "w");

        if (output_file == NULL)
            error("Could not open file \"%s\". Error number %d", output_filename, errno);
    }

    return output_file;
}

static void
ram_requirement_warning(enum architecture  target_architecture,
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

static line_number_type
get_minimum_basic_line_number(enum architecture
                                   target_architecture)
{
    line_number_type minimum_basic_line_number = MIN_LINE_NUMBER;

    switch(target_architecture)
    {
        case COCO:
            minimum_basic_line_number = COCO_MIN_LINE_NUMBER;
            break;

        case DRAGON:
            minimum_basic_line_number = DRAGON_MIN_LINE_NUMBER;
            break;

        case C64:
            minimum_basic_line_number = C64_MIN_LINE_NUMBER;
            break;

        default:
            internal_error("Unhandled target architecture in\n"
                                   "get_minimum_basic_line_number()");
    }

    return minimum_basic_line_number;
}

static line_number_type
get_maximum_basic_line_number(enum architecture
                              target_architecture)
{
    line_number_type maximum_basic_line_number = MAX_LINE_NUMBER;

    switch(target_architecture)
    {
        case COCO:
            maximum_basic_line_number = COCO_MAX_LINE_NUMBER;
            break;

        case DRAGON:
            maximum_basic_line_number = DRAGON_MAX_LINE_NUMBER;
            break;

        case C64:
            maximum_basic_line_number = C64_MAX_LINE_NUMBER;
            break;

        default:
            internal_error("Unhandled target architecture in\n"
                           "get_maximum_basic_line_number()");
    }

    return maximum_basic_line_number;
}

static void
check_line_number(enum architecture target_architecture,
                  line_number_type line_number)
{
    if (line_number < get_minimum_basic_line_number(target_architecture))
        internal_error("Line number is below minimum");

    if (line_number > get_maximum_basic_line_number(target_architecture))
        error("The BASIC line numbers have become too large");
}

static line_number_type
set_line_number(enum architecture target_architecture,
                boolean_type      line_number_set,
                line_number_type  line_number,
                boolean_type      typable)
{
    if (line_number_set == 0)
        line_number = (typable) ?
                       DEFAULT_TYPABLE_STARTING_LINE :
                       DEFAULT_STARTING_LINE;

    check_line_number(target_architecture, line_number);

    return line_number;
}

static line_number_step_type
set_step(boolean_type           step_set,
         line_number_step_type  step,
         boolean_type           typable)
{
    if (step_set == 0)
        step = (typable) ?
                DEFAULT_TYPABLE_STEP :
                DEFAULT_STEP;

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
inc_line_number(enum architecture target_architecture,
                boolean_type           *line_incrementing_has_started,
                line_number_type       *line_number,
                line_number_step_type  *step)
{
    if (*line_incrementing_has_started != 0)
        safe_step_line_number(line_number, *step);

    *line_incrementing_has_started = 1;

    check_line_number(target_architecture, *line_number);
}

static void
inc_line_count(line_counter_type *line_count)
{
    if (*line_count == LINE_COUNTER_TYPE_MAX)
        internal_error("Line count overflow detected");

    ++*line_count;

#if (MAXIMUM_LINE_COUNT < LINE_COUNTER_TYPE_MAX)
    if (*line_count > MAXIMUM_LINE_COUNT)
        error("Line count has exceeded the maximum (%lu lines)",
                                  (unsigned long int) MAXIMUM_LINE_COUNT);
#endif
}

static line_position_type
get_architecture_maximum_basic_line_length(enum architecture target_architecture)
{
    line_position_type architecture_max_basic_line_length
            = MAX_BASIC_LINE_LENGTH;

    switch(target_architecture)
    {
        case COCO:
            architecture_max_basic_line_length = COCO_MAX_LINE_LENGTH;
            break;

        case DRAGON:
            architecture_max_basic_line_length = DRAGON_MAX_LINE_LENGTH;
            break;

        case C64:
            architecture_max_basic_line_length = C64_MAX_LINE_LENGTH;
            break;

        default:
            internal_error("Unhandled target architecture type\n"
                           "in get_architecture_max_basic_line_length()");
    }

    return architecture_max_basic_line_length;
}

static void
check_line_position(enum architecture  target_architecture,
                    line_position_type               line_position)
{
    if (line_position > MAXIMUM_LINE_LENGTH)
        internal_error("Maximum BASIC line length was not avoided");

    if (line_position > get_architecture_maximum_basic_line_length(target_architecture))
        internal_error("The maximum BASIC line length for the \"%s\" target architecture was exceeded",
                        architecture_to_text(target_architecture));
}

static void
increment_line_position(enum architecture  target_architecture,
                             line_position_type               *line_position)
{
    if (*line_position == LINE_POSITION_TYPE_MAX)
        internal_error("Line position overflow detected");

    ++(*line_position);

    check_line_position(target_architecture, *line_position);
}

static void
caseify_output_text(char                     *output_text_pointer,
                    enum case_choice  output_case)
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

            case NO_CASE_CHOSEN:
            default:
                internal_error("Unhandled case in caseify_output_text()");
                break;
        }

        ++output_text_pointer;
    }
}

static void
process_output_text(char                             *output_text_pointer,
                    enum architecture  target_architecture,
                    enum case_choice          output_case,
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
            error("Couldn't write to output text area");
            break;

        case EMIT_FAIL:
            error("Couldn't write to output file. Error number %d", errno);
            break;

        case FTELL_FAIL:
            error("Couldn't operate on output file. Error number %d", errno);
            break;

        case TOO_LARGE:
            error("Generated BASIC program is too large");
            break;

        default:
            internal_error("Unknown status in check_vemit_status()");
            break;
    }
}

static enum vemit_status
vemit(FILE                             *output_file,
      enum architecture  target_architecture,
      enum case_choice          output_case,
      long int                         *output_file_size,
      line_counter_type                *line_count,
      line_position_type               *line_position,
      const char                       *fmt,
      va_list                          ap)
{
    char      output_text[TEXT_BUFFER_SIZE] = "";
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

    /* TODO: separate these two */

    if (*output_file_size > MAXIMUM_BASIC_PROGRAM_SIZE ||
        *output_file_size > FILE_SIZE_MAX)
        return TOO_LARGE;

    return VE_SUCCESS;
}

static void
emit(FILE                             *output_file,
     enum architecture  target_architecture,
     enum case_choice          output_case,
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
          enum architecture  target_architecture,
          enum case_choice          output_case,
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

    inc_line_number(target_architecture,
                    line_incrementing_has_started,
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
           enum architecture  target_architecture,
           enum case_choice          output_case,
           long int                         *output_file_size,
           boolean_type                     typable,
           boolean_type                     *line_incrementing_has_started,
           line_counter_type                *line_count,
           line_number_type                 *line_number,
           line_number_step_type            *step,
           line_position_type               *line_position,
           unsigned long int                datum)
{
    char possible_output_buffer[TEXT_BUFFER_SIZE] = "";
    int sprintf_return_value = sprintf(possible_output_buffer,
                                       "%s%lu", typable ? ", " : ",", datum);

    if (sprintf_return_value < 0)
        error("Couldn't write to output text area in emit_datum()");

    if (sprintf_return_value >= (signed int) sizeof possible_output_buffer)
        internal_error("Output text buffer overrun");

    if ((*line_position + strlen(possible_output_buffer) > MAXIMUM_LINE_LENGTH)
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
        inc_line_number(target_architecture,
                        line_incrementing_has_started,
                        line_number,
                        step);
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
exec_command(enum architecture target_architecture)
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
                  enum architecture  target_architecture,
                  enum case_choice          output_case,
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
                           enum file_format  input_file_format,
                           const char                     *input_filename)
{
    long int remainder = input_file_size - get_file_position(input_file, input_filename);

    if   ((input_file_format == BINARY      && remainder != 0)
       || (input_file_format == RS_DOS      && remainder
                                               != RS_DOS_FILE_POSTAMBLE_SIZE)
       || (input_file_format == DRAGON_DOS  && remainder != 0)
       || (input_file_format == PRG         && remainder != 0))
            error("Unexpected remaining bytes in input file \"%s\"",
                                                            input_filename);
}

static void
close_file(FILE *file, const char *filename)
{
    if (file         != stdin &&
        file         != stdout &&
        fclose(file) != 0)
            error("Couldn't close file \"%s\"", filename);
}

static void
print_diagnostic_info(enum architecture  target_architecture,
                      enum case_choice          output_case,
                      boolean_type                     typable,
                      boolean_type                     remarks,
                      boolean_type                     extended_basic,
                      boolean_type                     use_checksum,
                      memory_location_type             start,
                      memory_location_type             exec,
                      memory_location_type             end,
                      long int                         blob_size,
                      line_counter_type                line_count,
                      long int                         output_file_size)
{
    xprintf("Output is for the %s target architecture%s\n",
                      architecture_to_text(target_architecture),
                      extended_basic ? " (with Extended BASIC)" : "");

    xprintf("The program is %s case, %s form%s"
           " and with%s program comments\n",
                               case_to_text(output_case),
                               typable ? "typable" : "compact",
                               use_checksum ? " with checksumming" : "",
                               remarks ? "" : "out");
    xprintf("  Start location : $%x (%u)\n", start, start);
    xprintf("  Exec location  : $%x (%u)\n", exec, exec);
    xprintf("  End location   : $%x (%u)\n", end, end);

    if (blob_size > 15)
    xprintf("  Blob size      : $%lx (%lu) bytes\n", blob_size, blob_size);
    else
    xprintf("  Blob size      : %lu bytes\n", blob_size);
    xprintf("  BASIC program  : %u lines (%lu characters)\n",
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

    enum architecture  target_architecture  = NO_ARCHITECTURE_CHOSEN;
    enum file_format   input_file_format    = NO_FILE_FORMAT_CHOSEN;
    enum case_choice   output_case          = NO_CASE_CHOSEN;

    boolean_type  typable         = 0;
    boolean_type  remarks         = 0;
    boolean_type  extended_basic  = 0;
    boolean_type  verify          = 0;
    boolean_type  use_checksum    = 0;
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

    check_uchar_type();
    check_user_defined_type_limits();
    check_default_starting_line();
    check_default_typable_starting_line();
    check_maximum_starting_line();
    check_default_step();
    check_default_typable_step();
    check_maximum_step();
    check_maximum_line_count();
    check_maximum_line_length();
    check_maximum_checksummed_data_per_line();
    check_maximum_basic_program_size();
    check_maximum_input_file_size();
    check_maximum_binary_size();
    check_text_buffer_size();
    check_default_starts();

    if (argc > 0)
        while (*++argv)
        {
                 if (arg2_match(argv[0], "-h", "--help"))
                print_help();
            else if (arg2_match(argv[0], NULL, "--defaults"))
                print_defaults();
            else if (arg2_match(argv[0], "-i", "--info"))
                print_info();
            else if (arg2_match(argv[0], "-l", "--license"))
                print_license();
            else if (arg2_match(argv[0], "-v", "--version"))
                print_version();
            else if (arg2_match(argv[0], "-a", "--allopt"))
                print_all_options();
            else if (arg2_match(argv[0], "-o", "--output"))
            {
                output_filename = get_output_filename(argv[0],
                                                      argv[1],
                                                      output_filename);
                ++argv;
            }
            else if (arg2_match(argv[0], "-m", "--machine"))
            {
                target_architecture = 
                        get_target_architecture(argv[0],
                                                argv[1],
                                                target_architecture);
                ++argv;
            }
            else if (arg2_match(argv[0], "-f", "--format"))
            {
                input_file_format = get_format(argv[0],
                                               argv[1],
                                               input_file_format);
                ++argv;
            }
            else if (arg2_match(argv[0], "-c", "--case"))
            {
                output_case = get_case(argv[0], argv[1], output_case);
                ++argv;
            }
            else if (arg2_match(argv[0], NULL, "--line"))
            {
                line_number = get_line_number(argv[0],
                                              argv[1],
                                              line_number,
                                              &line_number_set);
                ++argv;
            }
            else if (arg2_match(argv[0], NULL, "--step"))
            {
                step = get_line_number_step(argv[0],
                                            argv[1],
                                            step,
                                            &step_set);
                ++argv;
            }
            else if (arg2_match(argv[0], "-s", "--start"))
            {
                start = get_memory_location_type_arg(argv[0],
                                                     argv[1],
                                                     start,
                                                     &start_set);
                ++argv;
            }
            else if (arg2_match(argv[0], "-e", "--exec"))
            {
                exec = get_memory_location_type_arg(argv[0],
                                                    argv[1],
                                                    exec,
                                                    &exec_set);
                ++argv;
            }
            else if (arg2_match(argv[0], "-n", "--nowarn"))
                nowarn = get_switch_state(argv[0], nowarn);
            else if (arg2_match(argv[0], "-t", "--typable"))
                typable = get_switch_state(argv[0], typable);
            else if (arg2_match(argv[0], NULL, "--verify"))
                verify = get_switch_state(argv[0], verify);
            else if (arg2_match(argv[0], NULL, "--checksum"))
                use_checksum = get_switch_state(argv[0], use_checksum);
            else if (arg2_match(argv[0], NULL, "--extbas"))
                extended_basic = get_switch_state(argv[0], extended_basic);
            else if (arg2_match(argv[0], "-r", "--remarks"))
                remarks = get_switch_state(argv[0], remarks);
            else if (arg2_match(argv[0], "-p", "--print"))
                print_program = get_switch_state(argv[0], print_program);
            else if (arg2_match(argv[0], NULL, "--diag"))
                print_diag = get_switch_state(argv[0], print_diag);
            else if (arg2_match(argv[0], NULL, "--stdin"))
                read_stdin = get_switch_state(argv[0], read_stdin);
            else if (is_option(argv[0]))
                error("Unknown command line option %s", argv[0]);
            else
            {
                if (input_filename != NULL)
                    error("Only one input filename may be specified");

                input_filename = argv[0];
            }
        }

    target_architecture = set_target_architecture(target_architecture);
    input_file_format   = set_input_file_format(target_architecture,
                                                input_file_format);
    output_case         = set_output_case(target_architecture, output_case);
    typable             = set_typable(typable, use_checksum);
    print_program       = set_print_program(output_filename, print_program);
    nowarn              = set_nowarn(nowarn, print_program);

    check_extended_basic(target_architecture, extended_basic);
    check_print_options(print_program, print_diag);
    check_input_filename(input_filename, read_stdin);
    check_output_filename(output_filename, print_program);

    output_filename  = set_output_filename(target_architecture,
                                           output_case,
                                           output_filename,
                                           print_program);

    input_file       = open_input_file(input_filename, read_stdin);
    input_file_size  = get_input_file_size(input_file,
                                           input_filename,
                                           input_file_format);
    blob_size        = calculate_blob_size(input_file_format,
                                           input_file_size);

    check_blob_size(target_architecture, blob_size, input_filename);

    process_file_format(input_file_format,
                   input_file,
                   input_filename,
                   &start_set,
                   &start,
                   &exec_set,
                   &exec,
                   blob_size);

    start  = set_start_address(target_architecture, &start_set, start);
    end    = set_end_address(target_architecture, start, blob_size);
    exec   = set_exec_address(&exec_set, start, exec);

    check_start_address(target_architecture, start);
    check_end_address  (target_architecture, end);
    check_exec_address (target_architecture, exec, start, end);

    line_number     = set_line_number(target_architecture,
                                      line_number_set,
                                      line_number,
                                      typable);
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

    if (remarks != 0)
    {
        char          date_text[TEXT_BUFFER_SIZE];
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
/* Need to take typeability into account */
        EMITLINEA("REM   This program was")
        EMITLINEA("REM generated by BASICloader")
        if (use_date != 0)
        EMITLINEB("REM   on %-15s", date_text)
        EMITLINEA("REM See github.com/")
        EMITLINEA("REM      richardcavell")
    }

    if (typable == 0 && verify == 0)
    {
        EMITLINEE("FORP=%dTO%d:READA:POKEP,A:NEXT:%s%d:END",
                  start, end, exec_command(target_architecture), exec)
    }

    if (typable == 0 && verify != 0)
    {
        EMITLINEC("FORP=%dTO%d:READA:POKEP,A", start, end)
        EMITLINEB("IFA<>PEEK(P)THENGOTO%d",line_number+3*step)
        EMITLINEC("NEXT:%s%d:END", exec_command(target_architecture), exec)
        EMITLINEA("PRINT\"Error!\":END")
    }

    if (typable != 0 && use_checksum == 0 && verify == 0)
    {
        EMITLINEC("FOR P = %d TO %d", start, end)
        EMITLINEA("READ A")
        EMITLINEA("POKE P,A")
        EMITLINEA("NEXT P")
        EMITLINEC("%s %d", exec_command(target_architecture), exec)
        EMITLINEA("END")
    }

    if (typable != 0 && use_checksum == 0 && verify != 0)
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

    if (use_checksum != 0 && verify == 0)
    {
        EMITLINEB("P = %u", start)
        EMITLINEB("Q = %u", end)
        EMITLINEA("READ L, CS")
        EMITLINEA("C = 0")
        EMITLINEA("J = Q - P")
        EMITLINEC("IF J > %d THEN J = %d", MAXIMUM_CHECKSUMMED_DATA_PER_LINE,
                                           MAXIMUM_CHECKSUMMED_DATA_PER_LINE)
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

    if (use_checksum != 0 && verify != 0)
    {
        EMITLINEB("P = %u", start)
        EMITLINEB("Q = %u", end)
        EMITLINEA("READ L, CS")
        EMITLINEA("C = 0")
        EMITLINEA("J = Q - P")
        EMITLINEC("IF J > %d THEN J = %d", MAXIMUM_CHECKSUMMED_DATA_PER_LINE,
                                           MAXIMUM_CHECKSUMMED_DATA_PER_LINE)
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

    if (use_checksum == 0)
    {
        long int b = blob_size;

        while (b--)
            EMITDATUM(getc_safe(input_file, input_filename))
    }
    else
    {
        long int b = blob_size;

        while (b != 0)
        {
            unsigned char d[MAXIMUM_CHECKSUMMED_DATA_PER_LINE];
            unsigned short int i = 0;
            unsigned short int j = 0;
            unsigned long int cs = 0;

            for (cs = 0, i = 0; i < sizeof d && b > 0; ++i, --b)
            {
                d[i] = getc_safe(input_file, input_filename);
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
        xprintf("BASIC program has been generated -> \"%s\"\n", output_filename);

    if (print_diag != 0)
        print_diagnostic_info(target_architecture,
                              output_case,
                              typable,
                              remarks,
                              extended_basic,
                              use_checksum,
                              start,
                              exec,
                              end,
                              blob_size,
                              line_count,
                              output_file_size);

    return EXIT_SUCCESS;
}

