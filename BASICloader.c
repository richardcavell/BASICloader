/* BASICloader by Richard Cavell
 * (in development, don't use it yet)
 *
 * BASICloader.c
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

  /* Begin user-modifiable values */

#define DEFAULT_STEP  10

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
    xprintf("%s%s%s",     "  -o or --output    name of output file (default \"",
                                           DEFAULT_OUTPUT_FILENAME, "\")\n");
    xprintf("%s",         "  -b or --begin     Beginning memory location\n");
    xprintf("%s%i%s",     "  -s or --step      BASIC line numbers step size (default ",
                                           DEFAULT_STEP, ")\n");
    xprintf("%s",         "  --                stop processing options\n");

    exit(EXIT_SUCCESS);
}

static void
print_info(void)
{
    xprintf("This program is a type-in generator.\n\n");
    xprintf("You supply a machine-language program, and\n");
    xprintf("this program will create a BASIC program\n");
    xprintf("that will poke the machine-language program\n");
    xprintf("into the computer's memory.\n\n");
    xprintf("See https://github.com/richardcavell/BASICloader\n");

    exit(EXIT_SUCCESS);
}

static void
print_license(void)
{
    xprintf("Copyright 2025 Richard Cavell\n\n");
    xprintf("Permission is hereby granted, free of charge,"
            " to any person obtaining a copy of this software"
            " and associated documentation files (the “Software”),"
            " to deal in the Software without restriction,"
            " including without limitation the rights to use, copy,"
            " modify, merge, publish, distribute, sublicense, and/or"
            " sell copies of the Software, and to permit persons to"
            " whom the Software is furnished to do so, subject to"
            " the following conditions:\n\n");
    xprintf("The above copyright notice and this permission notice"
            " shall be included in all copies or substantial"
            " portions of the Software.\n\n");
    xprintf("THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF"
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
    xprintf("1.0\n");
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
emit_preamble(const char *output_filename, FILE *output_fp, long step)
{
    emit (output_filename, output_fp,
          "%i rem created by basicloader\n", get_line_no(step));
    emit (output_filename, output_fp,
          "%i rem   by richard cavell\n", get_line_no(step));
}

static void
emit_loop(const char *output_filename, FILE *output_fp, long step,
          long begin, long end)
{
/*
This is what we should end up with:

30 I = 8192
40 CS = 0
50 FOR X = 1 TO 10
60 READ A
70 POKE I, A
80 I = I + 1
90 CS = CS + A
100 IF (I=8193) THEN 120
110 NEXT X
120 READ L: READ S
130 IF (CS <> S) THEN 150
140 IF (I<8193) THEN 40
150 EXEC 8192
160 PRINT "CHECKSUM ERROR IN LINE ";L
170 END
*/
    long line_no = 0;
    long begin_loop = 0;

    emit (output_filename, output_fp,
           "%i i = %i\n", get_line_no(step), begin);
    emit (output_filename, output_fp,
           "%i cs = 0\n", begin_loop = get_line_no(step));
    emit (output_filename, output_fp,
           "%i for x = 1 to 10\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i read a\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i i = poke i, a\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i i = i + 1\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i cs = cs + a\n", get_line_no(step));

    line_no = get_line_no(step);
    emit (output_filename, output_fp,
           "%i if (i=%i) then goto %i\n", line_no,
                                          end,
                                          line_no + 2 * step);
    emit (output_filename, output_fp,
           "%i next x\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i read l: read s\n", get_line_no(step));

    line_no = get_line_no(step);
    emit (output_filename, output_fp,
           "%i if (cs <> s) then %i\n", line_no,
                                        line_no + 3 * step);
    emit (output_filename, output_fp,
           "%i if (i<%i) then %i\n", get_line_no(step), end, begin_loop);
    emit (output_filename, output_fp,
           "%i exec %i\n", get_line_no(step), begin);
    emit (output_filename, output_fp,
           "%i print \"checksum error in line\";l\n", get_line_no(step));
    emit (output_filename, output_fp,
           "%i end\n", get_line_no(step));
}

static void
emit_data(const char *output_filename, FILE *output_fp, long step)
{
    int  data_count = 0;
    long line_no    = 0;
    int  checksum   = 0;
    int  d          = 0;

    while(d != EOF)
    {
        d = fgetc(output_fp);

        if (data_count == 0) /* starting a new line */
        {
            emit (output_filename, output_fp,
                  "%i data ",
                  line_no = get_line_no(step));

            checksum = 0;
        }
        else
        {
            emit (output_filename, output_fp, ", ");
        }

        emit(output_filename, output_fp, "%i", d);
        data_count++;

        if (data_count == 10 || d == EOF)
        {
            /* Add the line number and checksum */
            emit(output_filename, output_fp, "%i%s%i%c",
                                             line_no,
                                             ", ",
                                             checksum,
                                             '\n');
            data_count = 0;
        }
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
                     }
        else if (strcmp(*argv, "-b") == 0 ||
                 strcmp(*argv, "--begin") == 0)
                     {
                         const char *arg = *++argv;
                         char *endptr = NULL;

                         if (arg == NULL)
                             fail_msg("%s", "No beginning line number was provided\n");

                         begin = strtol(arg, &endptr, 10); /* no need for non-decimals */
                         if (*endptr != '\0')
                             fail_msg("%s",
                               "The provided beginning line number was not a decimal number\n");
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
        fail_msg("No beginning memory location was provided\n");

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

    emit_preamble(output_fname, output_fp, step);
    emit_loop(output_fname, output_fp, step, begin, end);
    emit_data(output_fname, output_fp, step);

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

    return EXIT_SUCCESS;
}
