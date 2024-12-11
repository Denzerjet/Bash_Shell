
/*
 * shell.l: lexical analyzer for shell
 */

%{

#include <string.h>

#include "read_line.h"
#include "shell.h"
#include "y.tab.h"

extern char *read_line();

int mygetc(FILE *f) {
  static char *p;
  char ch;
  if (!isatty(0)) {
    // stdin is not a tty. Call real getc

    return getc(f);
  }

  // stdin is a tty. Call our read_line.

  if ((p == NULL) || (*p == 0)) {
    char *s = read_line();
    p = s;
  }

  ch = *p;
  p++;
  return ch;
}

#undef getc
#define getc(f) mygetc(f)

static void yyunput (int c,char *buf_ptr);

void myunputc(int c) {
  unput(c);
}

char* expand_variables(char* original_word) {
  int beg_ind = -1;
  int end_ind = -1;

  int needed_space = strlen(original_word);

  char* expanded_word = malloc(1000);

  if (expanded_word == NULL) {
    perror("malloc");
    exit(1);
  }

  int i = 0;
  int expanded_ind = 0;

  while (i < strlen(original_word)) {
    if (original_word[i] == '$') {
      if ((i + 1 < strlen(original_word) - 1) &&
          (original_word[i+1] == '{')) {
        beg_ind = i;
      }
    }
    
    if ((beg_ind != -1) && (original_word[i] == '}')) {
      end_ind = i;

      // count number of chars between the brackets

      int num_chars = end_ind - beg_ind - 2;

      // extract the variable name

      char* var_name = malloc(num_chars + 1);
      var_name[num_chars] = '\0';


      strncpy(var_name, original_word + beg_ind + 2, num_chars);

      // if its a special variable

      if (!strcmp(var_name, "!")) {
        exit(1);
      }
      else if (!strcmp(var_name, "$")) {
        free(var_name);

        int shell_pid = getpid();
        
        int digits = shell_pid;

        int num_digits = 0;

        while (digits > 0) {
          digits /= 10;
          ++num_digits;
        }

        char* shell_pid_str = malloc(num_digits + 1);

        if (shell_pid_str == NULL) {
          perror("malloc");
          exit(1);
        }

        sprintf(shell_pid_str, "%d", shell_pid);
        shell_pid_str[num_digits] = '\0';

        strcpy(expanded_word + expanded_ind - 1 - 2, shell_pid_str);
        expanded_ind += num_digits - 1 - 2;
        free(shell_pid_str);

        i = end_ind + 1;

        beg_ind = -1;
        end_ind = -1;

      }
      else if (!strcmp(var_name, "SHELL")) {
        free(var_name);

        char* rel_path = g_argv[0];
        char* abs_path = realpath(rel_path, NULL);

        int value_len = strlen(abs_path);

        strcpy(expanded_word + expanded_ind - 5 - 2, abs_path);
        expanded_ind += value_len - 5 - 2;
        free(abs_path);

        i = end_ind + 1;
        beg_ind = -1;
        end_ind = -1;
      }
      else if (!strcmp(var_name, "_")) {
        free(var_name);

        int value_len = strlen(g_last_arg);

        strcpy(expanded_word + expanded_ind - 1 - 2, g_last_arg);
        expanded_ind += value_len - 1 - 2;

        i = end_ind + 1;
        beg_ind = -1;
        end_ind = -1;
      }
      else {
        
        // not special, normal env var

        char* value = getenv(var_name);


        if (value == NULL) {
          value = "";
        }

        // take the output and put it into expanded_word

        int value_len = strlen(value);

        strcpy(expanded_word + expanded_ind - strlen(var_name) - 2, value);
        expanded_ind += value_len - strlen(var_name) - 2;
        free(var_name);

        i = end_ind + 1;

        // reset beg & end indexes
        
        beg_ind = -1;
        end_ind = -1;
      }
    }
    else {
      expanded_word[expanded_ind] = original_word[i]; 
      ++expanded_ind;
      ++i;
    }
  }

  expanded_word[expanded_ind] = '\0';

  return expanded_word;
}

void source(char* file_name, bool init) {
  FILE *src_fp = fopen(file_name, "r");

  if (src_fp == NULL) {
    if (init == true) { 

      // do nothing

      // this if is for personal clarity

    }
    else {
      fprintf(stderr, "source: cannot source %s\n", file_name);
    }
  }
  else {
    g_prompts_off = true;
    YY_BUFFER_STATE src_buffer = yy_create_buffer(src_fp, YY_BUF_SIZE);
    yypush_buffer_state(src_buffer);
    yyparse();
    yypop_buffer_state();
    
    // error check
    
    fclose(src_fp);
    g_prompts_off = false;

    if (init == true) {
      yyrestart(stdin);
    }
  }
}

%}

%option noyywrap noinput

%%

\$\(.*\) {
  // read from pipe using read(fd, buffer, sizeof(buffer));
  // use malloc for buffer then free it after

  int pin[2] = {-1, -1};

  if (pipe(pin) == -1) {
    perror("pipe");
    exit(1);
  }

  int pout[2] = {-1, -1};

  if (pipe(pout) == -1) {
    perror("pipe");
    exit(1);
  }

  int default_in = dup(0);
  int default_out = dup(1);

  char* subshell_cmd = malloc(strlen(yytext) - 3 + 7);

  if (subshell_cmd == NULL) {
    perror("malloc");
    exit(1);
  }

  strncpy(subshell_cmd, yytext + 2, strlen(yytext) - 3);
  strcpy(subshell_cmd + strlen(yytext) - 3, "\nexit\n\0");

  if (write(pin[1], subshell_cmd, strlen(subshell_cmd)) == -1) {
    perror("write");
    exit(1);
  }

  if (close(pin[1]) == -1) {
    perror("close");
    exit(1);
  }

  int ret = fork();

  if (ret == -1) {
    perror("fork");
    exit(1);
  }

  if (ret == 0) {
  
    // child

    if ((close(default_in) == -1) ||
        (close(default_out) == -1)) {
      perror("close");
      exit(1);
    }

    if (dup2(pin[0], 0) == -1) {
      perror("dup2");
      exit(1);
    }

    if (close(pin[0]) == -1) { 
      perror("close");
      exit(1);
    }
    
    if (dup2(pout[1], 1) == -1) {
      perror("dup2");
      exit(1);
    }

    if ((close(pout[1]) == -1) ||
        (close(pout[0]) == -1)) {
      perror("close");
      exit(1);
    }

    char *args[] = { "/proc/self/exe", NULL };

    execvp(args[0], args);
    perror("execvp");
    exit(1);
  }

  if (close(pin[0])) {
    perror("close");
    exit(1);
  }

  if (close(pout[1])) {
    perror("close");
    exit(1);
  }

  free(subshell_cmd);

  int buf_size = 2000;
  char *buffer = malloc(buf_size);
  
  if (buffer == NULL) {
    perror("malloc");
    exit(1);
  }

  // buf_size -1 for null terminator

  int bytes_read = read(pout[0], buffer, buf_size - 1); 

  if (close(pout[0]) == -1) {
    perror("close");
    exit(1);
  }

  if (bytes_read == -1) {
    perror("read");
    exit(1);
  }

  if ((close(default_in) == -1) ||
      (close(default_out) == -1)) {
    perror("close");
    exit(1);
  }

  for (int i = bytes_read - 1; i >= 0; --i) {
    if (buffer[i] == '\n') {
      myunputc(' ');
    }
    else {
      myunputc(buffer[i]);
    }
  }

  free(buffer);
}

\".*\" {
  // preserve all (most) characters in double quotes
  // escapes appear as \ unless they are followed by a special character?...

  // yytext - this is the word that is captured

  int length = strlen(yytext);
  char* new_text = malloc(length - 2 + 1);
  new_text[length - 2] = '\0';

  if (new_text == NULL) {
    perror("malloc");
    exit(1);
  }

  int src_itr = 0;
  int dest_itr = 0;

  while (src_itr < length) {
    if ((src_itr != 0) && (src_itr != length - 1)) {
      new_text[dest_itr] = yytext[src_itr];
      ++dest_itr;
    }

    ++src_itr;
  }

  yylval.string = strdup(new_text);

  free(new_text);

  return QUOTED_WORD;
}

(\\.|[^ \t\n<>&|])+ {
  char* buffer = malloc(strlen(yytext) + 1);
  buffer[strlen(yytext)] = '\0';
  int len = 0;

  if (buffer == NULL) {
    perror("malloc");
    exit(1);
  }

  int i = 0;
  char escape = '\\';

  while (i < strlen(yytext)) {
    if (yytext[i] == escape) {
      buffer[len] = yytext[i+1];
      ++len;
      i += 2;
    }
    else {
      buffer[len] = yytext[i];
      ++len;
      ++i;
    }
  }

  char* filtered_word = malloc(len + 1);
  filtered_word[len] = '\0';
  strncpy(filtered_word, buffer, len);
  yylval.string = strdup(filtered_word);
  free(filtered_word);
  free(buffer);

  char* expanded_word = expand_variables(yylval.string);
  
  free(yylval.string);
  yylval.string = strdup(expanded_word);
  free(expanded_word);

  // tilde stuff!

  len = 0;
  i = 0;

  buffer = malloc(800);
  
  if (buffer == NULL) {
    perror("malloc");
    exit(1);
  }

  // if ~ is by itself or before / its user home

  // if ~ is before a word its that users home

  while (i < strlen(yylval.string)) {
    if (yylval.string[i] == '~') {
      if ((i + 1 >= strlen(yylval.string)) ||
          (yylval.string[i + 1] == '/')) {
        char* home_dir = getenv("HOME");

        strcpy(buffer + len, home_dir);
        len += strlen(home_dir);

        if (yylval.string[i + 1] == '/') {
          ++i;
        }

        ++i;
      }
      else {
        char* home_path = "/homes/";

        strcpy(buffer + len, home_path);
        len += strlen(home_path);

        ++i;
      }
    }
    else {
      buffer[len] = yylval.string[i];
      ++len;
      ++i;
    }
  }

  filtered_word = malloc(len + 1);
  filtered_word[len] = '\0';
  strncpy(filtered_word, buffer, len);
  free(yylval.string);
  yylval.string = strdup(filtered_word);
  free(filtered_word);
  free(buffer);

  return WORD;
}

source\ [^ \t\n<>&|]+ {
  char* file_name = malloc(strlen(yytext) - 7 + 1); 

  for (int i = 7; i < strlen(yytext); ++i) {
    file_name[i - 7] = yytext[i];
  }

  source(file_name, false);
  free(file_name);
}

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

"|" {
  // pipe
  return PIPE;
}


"2>" {
  // redirect stderr
  return STDERR;
}

">>&" {
  // redirect stdout and stderr to append to file?
  return APPENDBOTH;
}

">&" {
  // redirect stdout and stderr
  return BOTH;
}

">>" {
  // append to file
  return APPEND;
}

"&" {
  // background task
  return AMPERSAND;
}

">" {
  return STDOUT;
}

"<" {
  // stdin
  return STDIN;
}
