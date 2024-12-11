#include "command.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell.h"

char **g_env_var_array = NULL;
int g_env_var_array_length = 0;

/*
 *  Initialize a command_t
 */

void create_command(command_t *command) {
  command->single_commands = NULL;

  command->out_file = NULL;
  command->in_file = NULL;
  command->err_file = NULL;

  command->append_out = false;
  command->append_err = false;
  command->background = false;

  command->num_single_commands = 0;
} /* create_command() */

/*
 *  Insert a single command into the list of single commands in a command_t
 */

void insert_single_command(command_t *command, single_command_t *simp) {
  if (simp == NULL) {
    return;
  }

  command->num_single_commands++;

  // single_commands is only an array of pointers, not of
  // single_command structs

  int new_size = command->num_single_commands * sizeof(single_command_t *);
  command->single_commands =
      (single_command_t **)realloc(command->single_commands, new_size);
  command->single_commands[command->num_single_commands - 1] = simp;
} /* insert_single_command() */

/*
 *  Free a command and its contents
 */

void free_command(command_t *command) {
  for (int i = 0; i < command->num_single_commands; i++) {

    // this deletes what the pointer points to, the
    // single_command_t structs

    free_single_command(command->single_commands[i]);
  }

  if (command->single_commands != NULL) {
    free(command->single_commands);
  }

  if (command->out_file) {
    free(command->out_file);
    command->out_file = NULL;
  }

  if (command->in_file) {
    free(command->in_file);
    command->in_file = NULL;
  }

  if (command->err_file) {
    free(command->err_file);
    command->err_file = NULL;
  }

  command->append_out = false;
  command->append_err = false;
  command->background = false;

  free(command);
} /* free_command() */

/*
 *  Print the contents of the command in a pretty way
 */

void print_command(command_t *command) {
  printf("\n\n");
  printf("              COMMAND TABLE                \n");
  printf("\n");
  printf("  #   single Commands\n");
  printf("  --- ----------------------------------------------------------\n");

  // iterate over the single commands and print them nicely

  for (int i = 0; i < command->num_single_commands; i++) {
    printf("  %-3d ", i);
    print_single_command(command->single_commands[i]);
  }

  printf("\n\n");
  printf("  Output       Input        Error        Background\n");
  printf("  ------------ ------------ ------------ ------------\n");
  printf("  %-12s %-12s %-12s %-12s\n",
         command->out_file ? command->out_file : "default",
         command->in_file ? command->in_file : "default",
         command->err_file ? command->err_file : "default",
         command->background ? "YES" : "NO");
  printf("\n\n");
} /* print_command() */

/*
 *  Execute a command
 */

void execute_command(command_t *command) {
  // Don't do anything if there are no single commands

  if (command->single_commands == NULL) {
    return;
  }

  // Add execution here
  // For every single command fork a new process
  // Setup i/o redirection
  // and call exec

  int default_in = dup(0);
  int default_out = dup(1);
  int default_err = dup(2);

  if ((default_in == -1) || (default_out == -1) || (default_err == -1)) {
    perror("dup");
    exit(1);
  }

  // set the initial input

  int input_fd = -1;
  if (command->in_file != NULL) {
    // input_fd = open(command->in_file,
    // I assume that if they try and use an in_file that DNE
    // then I need to print an error message like normal bash does
    // i'll assume simplicity and then add that later

    input_fd = open(command->in_file, O_RDONLY);

    if (input_fd == -1) {
      perror("open");
      exit(1);
    }
  } else {
    input_fd = dup(default_in);

    if (input_fd == -1) {
      perror("dup");
      exit(1);
    }
  }

  int ret = -1;
  int output_fd = -1;
  int err_fd = -1;

  for (int i = 0; i < command->num_single_commands; i++) {
    // redirect input
    // copies the first argument fd into the second
    // the second becomes a copy of the first

    if (dup2(input_fd, 0) == -1) {
      perror("dup2");
      exit(1);
    }

    if (close(input_fd) == -1) {
      perror("close");
      exit(1);
    }

    // setup output

    if (i == command->num_single_commands - 1) {
      // last single command

      if (command->out_file != NULL) {
        // always create if it DNE

        if (command->append_out) {
          // open with append

          output_fd =
              open(command->out_file, O_WRONLY | O_CREAT | O_APPEND, 0644);

          if (output_fd == -1) {
            perror("open");
            exit(1);
          }
        } else {
          // open without append

          output_fd =
              open(command->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

          if (output_fd == -1) {
            perror("open");
            exit(1);
          }
        }
      } else {
        // use default out

        output_fd = dup(default_out);

        if (output_fd == -1) {
          perror("dup");
          exit(1);
        }
      }

      // do the same for stderr
      // for: ls ls | grep thiswillfindnomatches
      // it just says the error for ls, it doesn't continue in the chain
      // if a cmd fails, so stderr seems to only matter for the last cmd
      // dunno how it will be handled if a exec cmd fails before
      // the end of a command chain...

      if (command->err_file != NULL) {
        if (command->append_err) {
          err_fd = open(command->err_file, O_WRONLY | O_CREAT | O_APPEND, 0644);

          if (err_fd == -1) {
            perror("open");
            exit(1);
          }
        } else {
          err_fd = open(command->err_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

          if (err_fd == -1) {
            perror("open");
            exit(1);
          }
        }
      } else {
        err_fd = dup(default_err);

        if (err_fd == -1) {
          perror("dup");
          exit(1);
        }
      }

      if (dup2(err_fd, 2) == -1) {
        perror("dup2");
        exit(1);
      }

      if (close(err_fd) == -1) {
        perror("close");
        exit(1);
      }
    } else {
      // not last single command
      // create pipe

      int pipe_fds[2] = {-1, -1};

      if (pipe(pipe_fds) == -1) {
        perror("pipe");
        exit(1);
      }

      output_fd = pipe_fds[1];
      input_fd = pipe_fds[0];
    }

    // redirect output

    if (dup2(output_fd, 1) == -1) {
      perror("dup2");
      exit(1);
    }

    if (close(output_fd) == -1) {
      perror("close");
      exit(1);
    }

    // i dont think I redirect error except for the last
    // command
    // redirect

    char *argument = command->single_commands[i]->arguments[0];

    if (!strcmp(argument, "setenv")) {
      if (command->single_commands[i]->num_args > 3) {
        fprintf(stderr, "setenv: too many arguments");
      } else {
        char *var_name = command->single_commands[i]->arguments[1];
        char *var_val = command->single_commands[i]->arguments[2];

        int name_len = strlen(var_name);

        char *env_var = malloc(name_len + strlen(var_val) + 2);

        strcpy(env_var, var_name);
        env_var[strlen(var_name)] = '=';
        strcpy(env_var + strlen(var_name) + 1, var_val);

        putenv(env_var);

        g_env_var_array_length++;
        g_env_var_array = (char **)realloc(
            g_env_var_array, (g_env_var_array_length) * sizeof(char *));
        g_env_var_array[g_env_var_array_length - 1] = env_var;
      }
    } else if (!strcmp(argument, "unsetenv")) {
      if (command->single_commands[i]->num_args > 2) {
        fprintf(stderr, "unsetenv: too many arguments");
      } else {
        char *var_name = command->single_commands[i]->arguments[1];

        unsetenv(var_name);
      }
    } else if (!strcmp(argument, "cd")) {
      if (command->single_commands[i]->num_args > 2) {
        fprintf(stderr, "cd: too many arguments");
      } else {
        char *dir = command->single_commands[i]->arguments[1];
        if (dir == NULL) {
          dir = getenv("HOME");
        }

        if (chdir(dir) == -1) {
          fprintf(stderr, "cd: can't cd to %s\n", dir);
        }
      }
    } else {
      ret = fork();

      if (ret == -1) {
        perror("fork");
        exit(1);
      }

      if (ret == 0) {
        if ((close(default_in) == -1) || (close(default_out) == -1) ||
            (close(default_err) == -1)) {
          perror("close");
          exit(1);
        }

        // should never return

        if (!strcmp(command->single_commands[i]->arguments[0], "printenv")) {
          int itr = 0;
          char *env_var = environ[itr];

          while (env_var != NULL) {
            printf("%s\n", env_var);
            ++itr;
            env_var = environ[itr];
          }

          exit(0);
        } else {
          execvp(command->single_commands[i]->arguments[0],
                 command->single_commands[i]->arguments);
          perror("execvp");
          exit(1);
        }
      }
    }
  }

  if ((dup2(default_in, 0) == -1) || (dup2(default_out, 1) == -1) ||
      (dup2(default_err, 2) == -1)) {
    perror("dup2");
    exit(1);
  }

  if ((close(default_in) == -1) || (close(default_out) == -1) ||
      (close(default_err) == -1)) {
    perror("close");
    exit(1);
  }

  if (!command->background) {
    // g_last_executed_pid = ret;
    // printf("just set g_last_executed inside of command.c to %d\n", ret);

    int prev_errno = errno;
    if (waitpid(ret, NULL, 0) == -1) {
      if (errno == ECHILD) {

        // Do nothing, sigaction picked it up

        // Restore the errno

        errno = prev_errno;
      } else {
        perror("waitpid");
        exit(1);
      }
    }
  }

  // print prompt again if isatty()

  if ((isatty(STDIN_FILENO)) && (g_prompt_printed == false)) {
    print_prompt();
  } else {
    g_prompt_printed = false;
  }
} /* execute_command() */
