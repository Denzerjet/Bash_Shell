
/*
 * shell.y: parser for shell
 */

%code requires 
{

}

%union
{
  char * string;
}

%token <string> WORD PIPE QUOTED_WORD
%token NOTOKEN NEWLINE STDOUT STDIN STDERR BOTH APPEND APPENDBOTH AMPERSAND

%{

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include "command.h"
#include "single_command.h"
#include "shell.h"

void yyerror(const char * s);
int yylex();

%}

%%

goal:
      entire_command_list
      ;

entire_command_list:
      entire_command_list entire_command
  |   entire_command
  ;

entire_command:
      single_command_list io_modifier_list NEWLINE {
        execute_command(g_current_command);

        if (g_last_arg != NULL) {
          free(g_last_arg);
          g_last_arg = NULL;
        }

        if (g_current_command->num_single_commands != 0) {
          single_command_t* last_single_command = g_current_command->single_commands[g_current_command->num_single_commands - 1];
          g_last_arg = last_single_command->arguments[last_single_command->num_args - 1];
          g_last_arg = strdup(g_last_arg);
        }

        // free g_current_command and g_current_single_command
        free_command(g_current_command);
        g_current_command = NULL;
        // need to free this as after every single command
        // i malloc for the next, so after the last one
        // g_sing_cmd will be malloced
        free_single_command(g_current_single_command);
        g_current_single_command = NULL;
        // allocate and run the create() functions for
        // them again to prepare for the next command
        g_current_command = (command_t *) malloc(sizeof(command_t));

        if (g_current_command == NULL) {
          perror("malloc");
          exit(1);
        }

        g_current_single_command = (single_command_t *) malloc(sizeof(single_command_t));

        if (g_current_single_command == NULL) {
          perror("malloc");
          exit(1);
        }
        
        create_command(g_current_command);
        create_single_command(g_current_single_command);
      }
  |   NEWLINE {
        // print prompt again if isatty()?
        if (isatty(STDIN_FILENO)) {
          print_prompt();
        }
      }
  ;

single_command_list:
      single_command_list PIPE single_command
  |   single_command
  ;

single_command:
      executable argument_list {
        if (strcmp(g_current_single_command->arguments[0], "exit") == 0) {
          exit(0);
        }
        insert_single_command(g_current_command, g_current_single_command);
        // free_single_command(g_current_single_command);
        // do not free single_command, g_cur_cmd only
        // holds a list of ptrs, not the sing_cmds themselves
        // as far as I understand, the ptrs are stored
        // inside of command, so I can just malloc
        // here for the next command
        g_current_single_command = (single_command_t *) malloc(sizeof(single_command_t));

        if (g_current_single_command == NULL) {
          perror("malloc");
          exit(1);
        }

        create_single_command(g_current_single_command);
      }
  ;

argument_list:
      argument_list argument
  |  /* can be empty */
  ;

argument:
      WORD {
        // insert_argument(g_current_single_command, $1);

        if ((strchr($1, '?') == NULL) && (strchr($1, '*') == NULL)) {
          insert_argument(g_current_single_command, $1);
        }
        else {
          int og_args = g_current_single_command->num_args;
          expand_wildcards("", $1);

          if (og_args == g_current_single_command->num_args) {
            insert_argument(g_current_single_command, $1);
          }
        }
      }
  |   QUOTED_WORD {
        insert_argument(g_current_single_command, $1);
      }
  ;

executable:
      WORD {
        // insert_argument(g_current_single_command, $1);

        if ((strchr($1, '?') == NULL) && (strchr($1, '*') == NULL)) {
          insert_argument(g_current_single_command, $1);
        }
        else {
          int og_args = g_current_single_command->num_args;
          expand_wildcards("", $1);

          if (og_args == g_current_single_command->num_args) {
            insert_argument(g_current_single_command, $1);
          }
        }
      }
  |   QUOTED_WORD {
        insert_argument(g_current_single_command, $1);
      }
  ;

io_modifier_list:
      io_modifier_list io_modifier
  |  /* can be empty */   
  ;

io_modifier:
      STDOUT WORD {
        if (g_current_command->out_file != NULL) {
          printf("Ambiguous output redirect.\n");
          reset_shell();
        }
        else {
          g_current_command->out_file = $2;
        }
      }
  |   STDIN WORD {
        if (g_current_command->in_file != NULL) { 
           printf("Ambiguous input redirect.\n");
           reset_shell();
        }
        else {
           g_current_command->in_file = $2;
        }
      }
  |   STDERR WORD {
        g_current_command->err_file = $2;
      }
  |   BOTH WORD {
        if (g_current_command->out_file != NULL) {
          printf("Ambiguous output redirect.\n");
          reset_shell();
        }
        else {
          g_current_command->out_file = $2;
        }

        g_current_command->err_file = strdup($2);
      }
  |   APPEND WORD {
        if (g_current_command->out_file != NULL) {
          printf("Ambiguous output redirect.\n");
          reset_shell();
        }
        else {
          g_current_command->out_file = $2;
        }

        g_current_command->append_out = true;
      }
  |   APPENDBOTH WORD {
        if (g_current_command->out_file != NULL) {
          printf("Ambiguous output redirect.\n");
          reset_shell();
        }
        else {
          g_current_command->out_file = $2;
        }

        g_current_command->err_file = strdup($2);
        g_current_command->append_out = true;
        g_current_command->append_err = true;
      }
  |   AMPERSAND {
        g_current_command->background = true;
      }
  ;


%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
