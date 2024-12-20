#include "tty_raw_mode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

/*
 * Sets terminal into raw mode.
 * This causes having the characters available
 * immediately instead of waiting for a newline.
 * Also there is no automatic echo.
 */

void tty_raw_mode(void) {
  struct termios tty_attr = {0};

  tcgetattr(0, &tty_attr);

  /* Set raw mode. */
  tty_attr.c_lflag &= (~(ICANON | ECHO));
  tty_attr.c_cc[VTIME] = 0;
  tty_attr.c_cc[VMIN] = 1;

  tcsetattr(0, TCSANOW, &tty_attr);
} /* tty_raw_mode() */
