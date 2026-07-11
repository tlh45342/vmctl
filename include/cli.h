#ifndef VMCTL_CLI_H
#define VMCTL_CLI_H
#include <stdio.h>
#include "session.h"
int cli_eval_line(vmctl_session_t *s,const char *line);
int cli_run(vmctl_session_t *s,FILE *in,int interactive);
int cli_run_argv(vmctl_session_t *s,int argc,char **argv);
#endif
