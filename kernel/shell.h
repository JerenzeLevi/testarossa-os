#ifndef TESTAROSSA_SHELL_H
#define TESTAROSSA_SHELL_H

/* Run the interactive shell. Never returns. Reads keys from the keyboard
 * queue and sleeps with hlt when no key is waiting. */
void shell_run(void);

#endif
