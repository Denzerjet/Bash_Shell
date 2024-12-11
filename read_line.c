#include "read_line.h"

#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tty_raw_mode.h"

// extern void tty_raw_mode(void);

int g_line_index = 0;
int g_line_length = 0;
char g_line_buffer[MAX_BUFFER_LINE] = "";
int g_history_index = 0;
char **g_history = NULL;
int g_history_length = 0;
int g_history_search_index = -1;
char *g_history_regex = NULL;

/*
 *  Prints usage for read_line
 */

void read_line_print_usage() {
  char *usage = "\n"
                " ctrl-?       Print usage\n"
                " Backspace    Deletes last character\n"
                " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
} /* read_line_print_usage() */

/*
 * Input a line with some basic editing.
 * Allows for ctrl-D, ctrl-H, right arrow,
 * left arrow, home and end key.
 */

char *read_line() {

  // Set terminal in raw mode

  tty_raw_mode();

  g_line_length = 0;

  // Read one line until enter is typed

  while (1) {

    // Read one character in raw mode.

    char ch = '\0';
    read(0, &ch, 1);

    if (ch >= 32) {
      if (g_line_index < g_line_length) {
        memmove(g_line_buffer + g_line_index + 1, g_line_buffer + g_line_index,
                g_line_length - g_line_index + 1);
      }

      // It is a printable character.

      // Do echo

      write(1, &ch, 1);

      // If max number of character reached return.

      if (g_line_length == (MAX_BUFFER_LINE - 2)) {
        break;
      }

      // add char to buffer.

      g_line_buffer[g_line_index] = ch;

      g_line_index++;
      g_line_length++;

      if (g_line_index < g_line_length) {
        for (int j = g_line_index; j < g_line_length; ++j) {
          write(1, &g_line_buffer[j], 1);
        }

        ch = 8;
        for (int j = g_line_index; j < g_line_length; ++j) {
          write(1, &ch, 1);
        }
      }

    } else if (ch == 10) {
      // <Enter> was typed. Return line
      // Print newline

      write(1, &ch, 1);
      g_history_index = 0;
      g_history_search_index = -1;
      free(g_history_regex);
      g_history_regex = NULL;

      break;
    } else if (ch == 18) {
      if (g_line_length == 0) {
        // don't search if its empty

        continue;
      }

      // i think ctrl-c will always quit the read_line()
      // call anyways so no need to add an exit

      // get rid of the whole line
      int i = 0;
      for (i = 0; i < g_line_length; i++) {
        ch = 8;
        write(1, &ch, 1);
      }

      // Print spaces on top

      for (i = 0; i < g_line_length; i++) {
        ch = ' ';
        write(1, &ch, 1);
      }

      // Print backspaces

      for (i = 0; i < g_line_length; i++) {
        ch = 8;
        write(1, &ch, 1);
      }

      if (g_history_search_index == -2) {
        // don't let the user cycle through
        g_line_length = 0;
        g_line_index = 0;

        continue;
      }

      // searching, match the regex
      if (g_history_search_index == -1) {
        g_history_regex = (char *)malloc(g_line_length + 1);

        if (g_history_regex == NULL) {
          perror(g_history_regex);
          exit(1);
        }

        strncpy(g_history_regex, g_line_buffer, g_line_length);
        g_history_regex[g_line_length] = '\0';
      }

      // regex stuff

      regex_t re = {0};
      int status = regcomp(&re, g_history_regex, REG_EXTENDED);

      if (status != 0) {
        perror("compile");
        exit(1);
      }

      // if nothing found then display nothing
      // but always show what the user typed as
      // being searched below

      if (g_history_search_index == -1) {
        g_history_search_index = g_history_length - 1;
      }

      size_t nmatch = 1;
      regmatch_t match[1] = {{0, 0}};

      for (int j = g_history_search_index; j >= 0; --j) {
        if (regexec(&re, g_history[j], nmatch, match, 0) == 0) {
          // write out the matched line

          strcpy(g_line_buffer, g_history[j]);
          g_line_length = strlen(g_line_buffer);
          g_line_index = g_line_length;

          write(1, g_line_buffer, g_line_length);

          g_history_search_index--;

          if (g_history_search_index == -1) {
            g_history_search_index = -2;
          }

          break;
        }
      }

      // if something found display it

      regfree(&re);
    } else if (ch == 1) {

      // ctrl-A

      ch = 8;
      for (int i = 0; i < g_line_index; ++i) {
        write(1, &ch, 1);
      }

      g_line_index = 0;
    } else if (ch == 5) {
      // ctrl-E

      for (int i = g_line_index; i < g_line_length; ++i) {
        ch = g_line_buffer[i];
        write(1, &ch, 1);
      }

      g_line_index = g_line_length;
    } else if (ch == 31) {
      // ctrl-?

      read_line_print_usage();
      g_line_buffer[0] = 0;
      break;
    } else if (ch == 4) {
      if ((g_line_length == 0) || (g_line_length == g_line_index)) {
        continue;
      }

      if (g_line_index < g_line_length) {
        memmove(g_line_buffer + g_line_index, g_line_buffer + g_line_index + 1,
                g_line_length - g_line_index);
      }

      // Remove one character from buffer

      g_line_length--;
      if (g_line_index > 0) {
        g_line_index--;
      }

      for (int j = g_line_index + 1; j < g_line_length; ++j) {
        write(1, &g_line_buffer[j], 1);
      }
      ch = ' ';
      write(1, &ch, 1);

      ch = 8;
      for (int j = g_line_index; j < g_line_length + 1; ++j) {
        write(1, &ch, 1);
      }
    } else if (ch == 8) {
      if (g_line_length == 0) {
        continue;
      }

      // <backspace> was typed. Remove previous character read.

      // ctrl-H

      // Go back one character

      ch = 8;
      write(1, &ch, 1);

      if (g_line_index < g_line_length) {
        memmove(g_line_buffer + g_line_index - 1, g_line_buffer + g_line_index,
                g_line_length - g_line_index + 1);
      }

      // Remove one character from buffer

      g_line_length--;
      g_line_index--;

      for (int j = g_line_index; j < g_line_length; ++j) {
        write(1, &g_line_buffer[j], 1);
      }

      ch = ' ';
      write(1, &ch, 1);

      ch = 8;
      for (int j = g_line_index; j < g_line_length + 1; ++j) {
        write(1, &ch, 1);
      }
    } else if (ch == 27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.

      char ch1 = '\0';
      char ch2 = '\0';
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if ((ch1 == 91) && (ch2 == 65)) {
        if (g_history_length == 0) {
          continue;
        }

        if (g_history_index >= g_history_length) {

          // do not let them scroll up in history
          // if they are already at the last entry
          // g_history_index will equal the length
          // when they're at the last entry

          continue;
        }
        // Up arrow. Print next line in history.

        // Erase old line
        // Print backspaces

        int i = 0;
        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // Print spaces on top

        for (i = 0; i < g_line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }

        // Print backspaces

        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // Copy line from history

        int active_index = -1;

        if (g_history_length == 1) {
          active_index = 0;
        } else {
          active_index = g_history_length - g_history_index - 1;
        }

        strcpy(g_line_buffer, g_history[active_index]);
        g_line_length = strlen(g_line_buffer);
        g_line_index = g_line_length;

        g_history_index = g_history_index + 1;

        // echo line

        write(1, g_line_buffer, g_line_length);
      } else if ((ch1 == 91) && (ch2 == 66)) {

        // down arrow

        if (g_history_length == 0) {
          continue;
        }

        if (g_history_index <= 0) {
          continue;
        }

        // Erase old line
        // Print backspaces

        int i = 0;
        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // Print spaces on top

        for (i = 0; i < g_line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }

        // Print backspaces

        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        if (g_history_index == g_history_length) {

          // if they want to stop browsing history show
          // a blank line.

          g_line_index = 0;
          g_line_length = 0;
          g_history_index--;

          continue;
        }

        // Copy line from history

        int active_index = -1;

        g_history_index--;

        if (g_history_index < g_history_length) {
          active_index = g_history_length - g_history_index;
        } else {
          active_index = 0;
        }

        strcpy(g_line_buffer, g_history[active_index]);
        g_line_length = strlen(g_line_buffer);
        g_line_index = g_line_length;

        // echo line

        write(1, g_line_buffer, g_line_length);
      } else if ((ch1 == 91) && (ch2 == 68)) {

        // left arrow

        // Go back one character

        if (g_line_index > 0) {
          ch = 8;
          write(1, &ch, 1);

          g_line_index--;
        }
      } else if ((ch1 == 91) && (ch2 == 67)) {

        // right arrow

        if (g_line_index < g_line_length) {
          // ch = ' ';
          ch = g_line_buffer[g_line_index];
          write(1, &ch, 1);

          g_line_index++;
        }
      }
    }
  }

  // Add eol and null char at the end of string

  g_line_buffer[g_line_length] = 10;
  g_line_length++;
  g_line_buffer[g_line_length] = 0;

  g_line_length = 0;
  g_line_index = 0;

  char *line_to_save = malloc(strlen(g_line_buffer) + 1);
  strcpy(line_to_save, g_line_buffer);
  line_to_save[strlen(g_line_buffer) - 1] = '\0';

  g_history =
      (char **)realloc(g_history, (g_history_length + 1) * sizeof(char *));
  g_history[g_history_length] = line_to_save;
  g_history_length++;

  return g_line_buffer;
} /* read_line() */
