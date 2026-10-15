#ifndef TESTAROSSA_SHELL_H
#define TESTAROSSA_SHELL_H

/* Print the start message and the first prompt. */
void shell_start(void);

/* Handle one key: edit the line, or run the command on Enter. */
void shell_key(int c);

/* Text-mode main loop: start the shell, then feed it keys. Never returns. */
void shell_run(void);

#endif
