#ifndef KSEARCH_CLI_H
#define KSEARCH_CLI_H

/* Parses argv and dispatches to the right command. Returns a process exit code. */
int cli_run(int argc, char **argv);

#endif
