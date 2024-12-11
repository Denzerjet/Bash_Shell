#ifndef SHELL_H
#define SHELL_H

#include "command.h"
#include "single_command.h"

#include <error.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void print_prompt();
void child_collector(int signum);
void source(char *file_name, bool init);
char *expand_variables(char *original_word);

extern command_t *g_current_command;
extern single_command_t *g_current_single_command;
extern bool g_prompt_printed;
extern bool g_prompts_off;
extern char **environ;
extern int g_last_background_pid;
extern int g_last_executed_pid;
extern char **g_argv;
extern char *g_last_arg;

void reset_shell();
#endif // SHELL_H
