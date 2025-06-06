/* BASICloader by Richard Cavell
 * (in development, don't use it yet)
 *
 * BASICloader.c
 */

#include <stdio.h>
#include <stdlib.h>

  /* Begin user-modifiable values */



  /* End user-modifiable values */

static void
fail_string(const char *message)
{
    fprintf(stderr, "%s", message);

    exit(EXIT_FAILURE);
}

static void
fail_2string(const char *message, const char *message2)
{
    fprintf(stderr, "%s%s", message, message2);

    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    int exit_code = EXIT_SUCCESS;

    const char *invocation = argv[0];  /* invocation points to the name
                                          of this program */
    FILE *input_fp;

    ++argv;

    if (argc < 2) fail_string("No input file given"); /* Exits abnormally */

    input_fp = fopen(*argv, "r");

    if (input_fp==NULL)
        fail_2string("Couldn't open file ", *argv);

    (void) (invocation);

    return exit_code;
}
