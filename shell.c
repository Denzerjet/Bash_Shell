#include "shell.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command.h"
#include "single_command.h"
#include "y.tab.h"

command_t *g_current_command = NULL;
single_command_t *g_current_single_command = NULL;
bool g_prompt_printed = false;
bool g_prompts_off = false;
int g_last_background_pid = 0;
int g_last_executed_pid = 0;
char **g_argv = NULL;
char *g_last_arg = NULL;

/*
 * Reset global commands
 */

void reset_shell() {
  free_command(g_current_command);
  g_current_command = NULL;

  free_single_command(g_current_single_command);
  g_current_single_command = NULL;

  g_current_command = (command_t *)malloc(sizeof(command_t));
  g_current_single_command =
      (single_command_t *)malloc(sizeof(single_command_t));

  if (g_current_command == NULL) {
    perror("malloc");
    exit(1);
  }

  if (g_current_single_command == NULL) {
    perror("malloc");
    exit(1);
  }

  create_command(g_current_command);
  create_single_command(g_current_single_command);

  if (isatty(STDIN_FILENO)) {
    print_prompt();
  }
} /* reset_shell() */

int yyparse(void);

/*
 *  Prints shell prompt
 */

void print_prompt() {
  if (g_prompts_off == false) {
    printf("myshell>");
    fflush(stdout);
  }
} /* print_prompt() */

/*
 * Continues program on SIGINT
 */

void termination_handler(int signum) {
  // For Werror unused var

  (void)signum;

  if (isatty(STDIN_FILENO)) {
    printf("\n");
    print_prompt();
    g_prompt_printed = true;
  }
} /* termination_handler() */

/*
 * Acknowledges child processes
 * that have finished.
 */

void child_collector(int signum) {
  (void)signum;
  int pid = -1;

  while ((pid = waitpid(-1, NULL, WNOHANG) > 0)) {
    if ((pid != 1) && (pid != -1)) {
      printf("%d exited\n", pid);
    }
  }
} /* child_collector() */

/*
 *  This main is simply an entry point for the program which sets up
 *  memory for the rest of the program and the turns control over to
 *  yyparse and never returns
 */

int main(int argc, char *argv[]) {
  (void)argc;
  g_argv = argv;
  g_current_command = (command_t *)malloc(sizeof(command_t));
  g_current_single_command =
      (single_command_t *)malloc(sizeof(single_command_t));

  create_command(g_current_command);
  create_single_command(g_current_single_command);

  struct sigaction signal_action = {.sa_handler = termination_handler,
                                    .sa_mask = {{0}},
                                    .sa_flags = SA_RESTART};

  sigemptyset(&signal_action.sa_mask);

  int error = sigaction(SIGINT, &signal_action, NULL);

  if (error) {
    perror("sigaction");
    exit(1);
  }

  struct sigaction grim_reaper = {
      .sa_handler = child_collector, .sa_mask = {{0}}, .sa_flags = SA_RESTART};

  sigemptyset(&grim_reaper.sa_mask);

  int error_grim = sigaction(SIGCHLD, &grim_reaper, NULL);

  if (error_grim) {
    perror("sigaction");
    exit(1);
  }

  char *shellrc = ".shellrc";
  char *home_shellrc = malloc(strlen(getenv("HOME")) + 10);
  strcpy(home_shellrc, getenv("HOME"));
  strcpy(home_shellrc + strlen(getenv("HOME")), "/.shellrc");

  source(shellrc, true);
  source(home_shellrc, true);

  if (isatty(STDIN_FILENO)) {
    print_prompt();
  }

  yyparse();
} /* main() */
