#include "single_command.h"

#include <assert.h>
#include <dirent.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

/*
 *  Initialize a single command
 */

void create_single_command(single_command_t *simp) {
  simp->arguments = NULL;
  simp->num_args = 0;
} /* create_single_command() */

/*
 *  Free everything in a single command
 */

void free_single_command(single_command_t *simp) {
  for (int i = 0; i < simp->num_args; i++) {
    free(simp->arguments[i]);
    simp->arguments[i] = NULL;
  }

  if (simp->arguments != NULL) {
    free(simp->arguments);
  }

  free(simp);
} /* free_single_command() */

/*
 * Sorts an array of strings
 * used for sorting a dir listing
 * when wildcarding
 */

char **sort_array_strings(char **array, int num_entries) {
  char **sorted_array = (char **)malloc(num_entries * sizeof(char *));

  int num_inserted = 0;
  int index_inserted_arr = -1;

  while (num_inserted < num_entries) {
    char *next_insert = NULL;

    for (int i = 0; i < num_entries; ++i) {
      char *string_it = array[i];

      if (string_it != NULL) {
        if (next_insert == NULL) {
          next_insert = string_it;
          index_inserted_arr = i;
        } else if (strcmp(next_insert, string_it) > 0) {
          next_insert = string_it;
          index_inserted_arr = i;
        }
      }
    }

    sorted_array[num_inserted] = next_insert;
    array[index_inserted_arr] = NULL;
    ++num_inserted;
  }

  return sorted_array;
} /* sort_array_strings() */

/*
 * Expands the wildcards ? and *
 */

void expand_wildcards(char *prefix, char *suffix) {
  if (suffix[0] == 0) {
    insert_argument(g_current_single_command, strdup(prefix));
    return;
  }

  char *s = strchr(suffix, '/');

  char component[1024] = "";

  if (s != NULL) {
    strncpy(component, suffix, s - suffix);
    component[s - suffix] = '\0';
    suffix = s + 1;
  } else {
    strcpy(component, suffix);
    component[strlen(suffix)] = '\0';
    suffix = suffix + strlen(suffix);
  }

  char new_prefix[1030] = "";

  if ((strchr(component, '?') == NULL) && (strchr(component, '*') == NULL)) {
    if ((strlen(component) == 1) && (component[0] == '/')) {
      expand_wildcards(component, suffix);
    } else {
      if ((strlen(prefix) == 1) && (prefix[0] == '/')) {
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, component);
        expand_wildcards(new_prefix, suffix);
        return;
      }

      snprintf(new_prefix, sizeof(new_prefix), "%s/%s", prefix, component);
      expand_wildcards(new_prefix, suffix);
      return;
    }
  }

  // convert wildcard to regular expression

  // 3 because null term, then ^ and $

  char *regex = (char *)malloc(2 * strlen(component) + 3);
  char *arg_pos = component;
  char *regex_pos = regex;

  *regex_pos++ = '^';

  while (*arg_pos) {
    if (*arg_pos == '*') {
      *regex_pos++ = '.';
      *regex_pos++ = '*';
    } else if (*arg_pos == '?') {
      *regex_pos++ = '.';
    } else if (*arg_pos == '.') {
      *regex_pos++ = '\\';
      *regex_pos++ = '.';
    } else {
      *regex_pos++ = *arg_pos;
    }
    arg_pos++;
  }

  *regex_pos++ = '$';
  *regex_pos++ = '\0';

  // compile regular expression

  regex_t re = {0};
  int status = regcomp(&re, regex, REG_EXTENDED);

  if (status != 0) {
    perror("compile");
    return;
  }

  // List directory and add matches

  DIR *dir = NULL;

  if (strlen(prefix) == 0) {
    dir = opendir(".");
  } else {
    dir = opendir(prefix);
  }

  if (dir == NULL) {
    return;
  }

  struct dirent *ent = NULL;
  int max_entries = 20;
  int num_entries = 0;
  char **array = (char **)malloc(max_entries * sizeof(char *));

  while ((ent = readdir(dir)) != NULL) {

    regmatch_t match[1] = {{0, 0}};

    // looking for one match within the entry

    size_t nmatch = 1;

    if (regexec(&re, ent->d_name, nmatch, match, 0) == 0) {
      if (ent->d_name[0] == '.') {
        if (component[0] == '.') {
          if (num_entries == max_entries) {
            max_entries *= 2;
            array = realloc(array, max_entries * sizeof(char *));
            assert(array != NULL);
          }

          array[num_entries] = strdup(ent->d_name);
          ++num_entries;
        }
      } else {
        if (num_entries == max_entries) {
          max_entries *= 2;
          array = realloc(array, max_entries * sizeof(char *));
          assert(array != NULL);
        }

        array[num_entries] = strdup(ent->d_name);
        ++num_entries;
      }
    }
  }

  char **sorted_array = sort_array_strings(array, num_entries);

  for (int j = 0; j < num_entries; ++j) {
    free(array[j]);
  }

  free(array);
  array = NULL;

  for (int i = 0; i < num_entries; ++i) {
    if ((strlen(prefix) == 0) ||
        ((strlen(prefix) == 1) && (prefix[0] == '/'))) {
      sprintf(new_prefix, "%s%s", prefix, sorted_array[i]);
      expand_wildcards(new_prefix, suffix);
    } else {
      sprintf(new_prefix, "%s/%s", prefix, sorted_array[i]);
      expand_wildcards(new_prefix, suffix);
    }
  }

  free(sorted_array);

  // only free array because I don't strdup the
  // array elements when inserting args, so freeing the
  // single command will free those strings later

  free(regex);
  regfree(&re);
  closedir(dir);
} /* expand_wildcards() */

/*
 *  Insert an argument into the list of arguments in a single command
 */

void insert_argument(single_command_t *simp, char *argument) {
  if (argument == NULL) {
    return;
  }

  simp->num_args++;
  simp->arguments =
      (char **)realloc(simp->arguments, (simp->num_args + 1) * sizeof(char *));

  if (simp->arguments == NULL) {
    perror("realloc");
    exit(1);
  }

  simp->arguments[simp->num_args - 1] = argument;
  simp->arguments[simp->num_args] = NULL;
} /* insert_argument() */

/*
 *  Print a single command in a pretty format
 */

void print_single_command(single_command_t *simp) {
  for (int i = 0; i < simp->num_args; i++) {
    printf("\"%s\" \t", simp->arguments[i]);
  }

  printf("\n\n");
} /* print_single_command() */
