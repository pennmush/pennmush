/**
 * Various routines for signal handling.
 */

#ifndef __SIG_H
#define __SIG_H

/** Type definition for signal handlers */
typedef void (*Sigfunc) (int);

/* Set up a signal handler. Use instead of signal() */
Sigfunc install_sig_handler(int signo, Sigfunc f);

/* Call from in a signal handler to re-install the handler. Does nothing
   with persistent signals */
void reload_sig_handler(int signo, Sigfunc f);

/* Ignore a signal. Like i_s_h with SIG_IGN) */
void ignore_signal(int signo);

/* Block one signal temporarily. */
void block_a_signal(int signo);

/* Unblock a signal */
void unblock_a_signal(int signo);

/* Block all signals en masse. */
void block_signals(void);

#endif                          /* __SIG_H */
