/* BASICloader by Richard Cavell
 * version 1.0
 *
 * BASICloader.c
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

  /* Begin user-modifiable values */

#define DEFAULT_STEP  10
#define DEFAULT_LOWERCASE 0
#define DEFAULT_OUTPUT_FILENAME "typein.bas"

  /* End user-modifiable values */

static void
fail_msg(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    /* Ignore the return value of vfprintf() here, as the program is failing. */
    (void) vfprintf(stderr, fmt, args);
    va_end(args);

    exit(EXIT_FAILURE);
}

static void
fail_perror(const char *fmt, ...)
{
    /* We have to save this in case the call to vfprintf()
       is successful and replaces errno with a success code */
    const int saved_errno = errno;

    va_list args;
    va_start(args, fmt);
    /* Ignore the return value of vfprintf() here, as the program is failing. */
    (void) vfprintf(stderr, fmt, args);
    va_end(args);

    errno = saved_errno;

    perror("");

    exit(EXIT_FAILURE);
}

static void
xprintf(const char *fmt, ...)
{
    int failed = 0;

    va_list args;
    va_start(args, fmt);
    /* Ignore the return value of vfprintf() here, as the program is failing. */
    if (vfprintf(stderr, fmt, args) < 0)
        failed = 1;
    va_end(args);

    if (failed)
        fail_perror("%s", "vfprintf() within xprintf() failed");
}

static void
print_help(const char *invocation)
{
    xprintf("%s", "BASICloader v1.0 by Richard Cavell\n");
    xprintf("%s%s%s", "Usage: ", invocation, " [options] input filename\n");
    xprintf("%s",         "  -h or --help      this help text\n");
    xprintf("%s",         "  -i or --info      what this program does\n");
    xprintf("%s",         "  -l or --license   your license to use this software\n");
    xprintf("%s",         "  -v or --version   version number\n");
    xprintf("%c", '\n');
    xprintf("%s",         "  -b or --begin     Beginning memory location\n");
    xprintf("%s%s%s",     "  -o or --output    name of output file (default \"",
                                           DEFAULT_OUTPUT_FILENAME, "\")\n");
    xprintf("%s",         "  -p or --preamble  Initial commands at the top of the program\n");
    xprintf("%s%i%s",     "  -s or --step      BASIC line numbers step size (default ",
                                           DEFAULT_STEP, ")\n");
    xprintf("%s",         "  -w or --lowercase Output the program in lowercase (default is uppercase)\n");
    xprintf("%s",         "  --                stop processing options\n");
                  /* There is also --uppercase but it's not listed here */

    exit(EXIT_SUCCESS);
}

static void
print_info(void)
{
    xprintf("%s", "This program is a type-in generator.\n\n");
    xprintf("%s", "You supply a machine-language program, and\n");
    xprintf("%s", "this program will create a BASIC program\n");
    xprintf("%s", "that will poke the machine-language program\n");
    xprintf("%s", "into the computer's memory.\n\n");
    xprintf("%s", "See https://github.com/richardcavell/BASICloader\n");

    exit(EXIT_SUCCESS);
}

static void
print_license(void)
{
    xprintf("%s", "Copyright 2025 Richard Cavell\n\n");
    xprintf("%s", "Permission is hereby granted, free of charge,"
            " to any person obtaining a copy of this software"
            " and associated documentation files (the “Software”),"
            " to deal in the Software without restriction,"
            " including without limitation the rights to use, copy,"
            " modify, merge, publish, distribute, sublicense, and/or"
            " sell copies of the Software, and to permit persons to"
            " whom the Software is furnished to do so, subject to"
            " the following conditions:\n\n");
    xprintf("%s", "The above copyright notice and this permission notice"
            " shall be included in all copies or substantial"
            " portions of the Software.\n\n");
    xprintf("%s", "THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF"
            " ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT"
            " LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS"
            " FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO"
            " EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE"
            " FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN"
            " AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,"
            " OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE"
            " OR OTHER DEALINGS IN THE SOFTWARE.\n");

    exit(EXIT_SUCCESS);
}

static void
print_version(void)
{
    xprintf("%s", "1.0\n");
    exit(EXIT_SUCCESS);
}

static void
emit(const char *output_filename, FILE *output_fp, const char *fmt, ...)
{
    int vfp_code = 0;

    va_list args;
    va_start(args, fmt);
    vfp_code = vfprintf(output_fp, fmt, args);
    va_end(args);

    if (vfp_code < 0)
        fail_perror("%s%s%s%i%c", "Couldn't emit a line to the output file, \"",
                                  output_filename,
                                  "\".\nError code: ",
                                  errno,
                                  '\n');
}

static long
get_line_no(long step)
{
    static long line_no = 0;

    line_no += step;

    return line_no;
}

static void
emit_given_preamble(const char *output_filename, FILE *output_fp, long step, const char *preamble)
{
    emit(output_filename, output_fp, "%i%s%c", get_line_no(step), preamble, '\n');
}

static void
emit_preamble(const char *output_filename, FILE *output_fp, long step)
{
    emit (output_filename, output_fp,
          "%i%s", get_line_no(step), " REM CREATED BY BASICLOADER\n");
    emit (output_filename, output_fp,
          "%i%s", get_line_no(step), " REM    BY RICHARD CAVELL\n");
}

static void
emit_loop(const char *output_filename, FILE *output_fp, long step,
          long begin, long end)
{
    long line_no = 0;
    long begin_loop = 0;

    emit (output_filename, output_fp,
           "%i I = %i\n", get_line_no(step), begin);
    emit (output_filename, output_fp,
           "%i CS = 0\n", begin_loop = get_line_no(step));
    emit (output_filename, output_fp,
           "%i FOR X = 1 TO 10\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i READ A\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i POKE I, A\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i CS = CS + A\n", get_line_no(step));
    line_no = get_line_no(step);
    emit (output_filename, output_fp,
           "%i IF (I=%i) THEN GOTO %i\n", line_no,
                                          end,
                                          line_no + 3 * step);
    emit (output_filename, output_fp,
           "%i I = I + 1\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i NEXT X\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i READ L: READ S\n", get_line_no(step));

    line_no = get_line_no(step);
    emit (output_filename, output_fp,
           "%i IF (CS <> S) THEN GOTO %i\n", line_no,
                                        line_no + 3 * step);
    emit (output_filename, output_fp,
           "%i IF (I<%i) THEN GOTO %i\n", get_line_no(step), end, begin_loop);
    emit (output_filename, output_fp,
           "%i EXEC %i\n", get_line_no(step), begin);
    emit (output_filename, output_fp,
           "%i END\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i PRINT \"CHECKSUM ERROR IN LINE\";L\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i END\n", get_line_no(step));
}

static void
emit_data(FILE *input_fp, const char *output_filename, FILE *output_fp, long step)
{
    int  data_count = 0;
    long line_no    = 0;
    int  checksum   = 0;
    int  d          = 0;

    /* Should be a value from 0-255 or EOF */
    while((d = fgetc(input_fp)) != EOF)
    {
        if (data_count == 0)  /* start a new line */
        {
            emit(output_filename, output_fp,
                 "%i DATA ",
                 line_no = get_line_no(step));

            checksum = 0;
        }
        else
            emit(output_filename, output_fp, "%s", ", ");

        emit(output_filename, output_fp, "%i", d);
        checksum += d;
        data_count++;

        if (data_count == 10)
        {
            /* Add the line number and checksum */
            emit(output_filename, output_fp, "%s%li%s%i%c",
                                             ", ",
                                             line_no,
                                             ", ",
                                             checksum,
                                             '\n');
            data_count = 0;
        }
    }

    if ((data_count > 0) && (d == EOF))
    {
        /* Add the line number and checksum to the last line */
        emit(output_filename, output_fp, "%s%li%s%i%c",
                                         ", ",
                                         line_no,
                                         ", ",
                                         checksum,
                                         '\n');
    }
}

static void
convert_output_to_lowercase(const char *output_fname)
{
    FILE *output_fp = fopen(output_fname, "r+");
    int ch;
    long pos;

    if (output_fp == NULL)
    {
        fail_perror("%s%s%s%i%c", "Couldn't open file \"",
                                  *output_fname,
                                  "\" for lowercase substitution.\nError code: ",
                                  errno,
                                  '\n');
    }

    while ((ch = fgetc(output_fp)) != EOF)
    {
        pos = ftell(output_fp);
        fseek(output_fp, pos-1, SEEK_SET);
        fputc(tolower(ch), output_fp);
        fseek(output_fp, pos, SEEK_SET);
    }

    if (fclose(output_fp) == EOF)
    {
        fail_perror("%s%s%s%i%c", "Closure of file \"",
                                  output_fname,
                                  "\" failed.\nError code: ",
                                  errno,
                                  '\n');
    }
}

int
main(int argc, char *argv[])
{
    const char *invocation = argv[0];  /* invocation points to the name
                                          of this program */

    const char *input_fname = NULL;
    FILE       *input_fp    = NULL;

    const char *output_fname = DEFAULT_OUTPUT_FILENAME;
    FILE       *output_fp    = NULL;

    long begin  = -1;         /* This must be provided */
    long length = -1;         /* This is measurable */
    long end    = -1;         /* This is calculated from the first two */

    long step   = DEFAULT_STEP;

    int lowercase = DEFAULT_LOWERCASE;

    const char *preamble = NULL;

    int stop_processing_options = 0;

    if (invocation == NULL)  /* The early Amigas would do this */
        fail_msg("Cannot process command line arguments.\n");

    (void) argc;  /* we don't use this */
    ++argv;

    while (stop_processing_options == 0 &&
           (*argv != NULL) && (**argv == '-'))
    {
        if (strcmp(*argv, "-h") == 0 ||
            strcmp(*argv, "--help") == 0)
                print_help(invocation);

        else if (strcmp(*argv, "-i") == 0 ||
                 strcmp(*argv, "--info") == 0)
                     print_info();

        else if (strcmp(*argv, "-l") == 0 ||
                 strcmp(*argv, "--license") == 0)
                     print_license();

        else if (strcmp(*argv, "-v") == 0 ||
                 strcmp(*argv, "--version") == 0)
                     print_version();

        else if (strcmp(*argv, "-o") == 0 ||
                 strcmp(*argv, "--output") == 0)
                     {
                         output_fname = *++argv;
                         if (output_fname == NULL)
                             fail_msg("%s", "No output filename was provided\n");
                     }

        else if (strcmp(*argv, "-w") == 0 ||
                 strcmp(*argv, "--lowercase") == 0)
                     lowercase = 1;

        else if (strcmp(*argv, "--uppercase") == 0)  /* This is for symmetry */
                     lowercase = 0;

        else if (strcmp(*argv, "-s") == 0 ||
                 strcmp(*argv, "--step") == 0)
                     {
                         const char *arg = *++argv;
                         char *endptr = NULL;

                         if (arg == NULL)
                             fail_msg("%s", "No step size was provided\n");

                         step = strtol(arg, &endptr, 10); /* no need for non-decimals */
                         if (*endptr != '\0')
                             fail_msg("%s",
                               "The provided step size was not a decimal number\n");
                         else if (step < 1 || step > 59999)
                             fail_msg("%s%i%s", "The provided step size of ",
                                                step,
                                                " was outside of the range 1...59999");
                     }
        else if (strcmp(*argv, "-b") == 0 ||
                 strcmp(*argv, "--begin") == 0)
                     {
                         const char *arg = *++argv;
                         char *endptr = NULL;

                         if (arg == NULL)
                             fail_msg("%s", "No beginning line number was provided\n");

                         begin = strtol(arg, &endptr, 0); /* we can use non-decimals */
                         if (*endptr != '\0')
                             fail_msg("%s",
                               "The provided begin memory location is unclear\n");
                     }
        else if (strcmp(*argv, "-p") == 0 ||
                 strcmp(*argv, "--preamble") == 0)
                     {
                         preamble = *++argv;
                         if (preamble == NULL)
                             fail_msg("%s", "No preamble was provided\n");
                     }
        else if (strcmp(*argv, "--") == 0)
                     stop_processing_options = 1;
        else
        {
            fail_msg("%s%s%s", "Unrecognized option \"", *argv, "\".\n");
        }

        ++argv;
    }

    if (*argv == NULL)
        fail_msg("%s", "No input filename was provided\n"); /* Exits abnormally */

    if (begin == -1)
        fail_msg("%s", "No beginning memory location was provided\n");

    input_fname  = *argv;
    input_fp     = fopen(*argv, "r");

    if (input_fp == NULL)
    {
        fail_perror("%s%s%s%i%c", "Couldn't open file \"",
                                  *argv,
                                  "\".\nError code: ",
                                  errno,
                                  '\n');
    }

    if (fseek(input_fp, 0, SEEK_END) == -1)
    {
        fail_perror("%s%s%s%i%c", "Couldn't seek the end of file \"",
                                  *argv,
                                  "\".\nError code: ",
                                  errno,
                                  '\n');
    }

    length = ftell(input_fp);
    if (length == -1)
    {
        fail_perror("%s%s%s%i%c", "Couldn't get size of input file \"",
                                  *argv,
                                  "\".\nError code: ",
                                  errno,
                                  '\n');
    }

    rewind(input_fp);

    end = begin + length - 1;

    output_fp = fopen(output_fname, "w");

    if (output_fp == NULL)
    {
        fail_perror("%s%s%s%i%c", "Couldn't open file \"",
                                  output_fname,
                                  "\" for output.\nError code: ",
                                  errno,
                                  '\n');
    }

    if (preamble)
        emit_given_preamble(output_fname, output_fp, step, preamble);

    emit_preamble(output_fname, output_fp, step);
    emit_loop(output_fname, output_fp, step, begin, end);
    emit_data(input_fp, output_fname, output_fp, step);

    if (fclose(output_fp) == EOF)
    {
        fail_perror("%s%s%s%i%c", "Closure of file \"",
                                  output_fname,
                                  "\" failed.\nError code: ",
                                  errno,
                                  '\n');
    }

    if (fclose(input_fp) == EOF)
    {
        fail_perror("%s%s%s%i%c", "Closure of file \"",
                                  input_fname,
                                  "\" failed.\nError code: ",
                                  errno,
                                  '\n');
    }

    if (lowercase)
        convert_output_to_lowercase(output_fname);

    return EXIT_SUCCESS;
}
