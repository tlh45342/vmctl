/*
 * main.c - vmctl entry point
 *
 * Supported modes:
 *
 *     vmctl vm create
 *     vmctl import build.vmctl
 *     vmctl < build.vmctl
 *     vmctl
 */

#include "cli.h"
#include "config.h"
#include "session.h"

#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    vmctl_session_t session;
    int rc;

    session_init(&session);

    if (config_load_default(&session) != 0) {
        session_shutdown(&session);
        return 1;
    }

    if (argc > 1)
        rc = cli_run_argv(&session, argc, argv);
    else if (isatty(fileno(stdin)))
        rc = cli_run(&session, stdin, 1);
    else
        rc = cli_run(&session, stdin, 0);

    session_shutdown(&session);
    return rc == 2 ? 0 : rc;
}
