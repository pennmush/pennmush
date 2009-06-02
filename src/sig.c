/**
 * \file sig.c
 *
 * \brief Signal handling routines for PennMUSH.
 *
 *
 */

#include "config.h"
#include <signal.h>
#include "conf.h"
#include "externs.h"
#include "confmagic.h"

#ifndef HAVE_SIGPROCMASK
static Sigfunc saved_handlers[NSIG];
#endif

/** Our own version of signal().
 * We're going to rewrite the signal() function in terms of
 * sigaction, where available, to ensure consistent semantics.
 * We want signal handlers to remain installed, and we want
 * signals (except SIGALRM) to restart system calls which they
 * interrupt. This is how bsd signals work, and what we'd like.
 * This function is essentially example 10.12 from Stevens'
 * _Advanced Programming in the Unix Environment_.
 * \param signo signal number.
 * \param func signal handler function to install.
 * \return former signal handler for signo.
 */
Sigfunc
install_sig_handler(int signo, Sigfunc func)
{
#ifdef HAVE_SIGACTION
  struct sigaction act, oact;
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
#ifdef SA_RESTART
  act.sa_flags |= SA_RESTART;
#endif
  if (sigaction(signo, &act, &oact) < 0)
    return SIG_ERR;
  return oact.sa_handler;
#else                           /* No sigaction, drat. */
  return signal(signo, func);
#endif
}

/** Reinstall a signal handler.
 * \param signo the signal number.
 * \param func signal handler function to reload.
 */
void
reload_sig_handler(int signo __attribute__ ((__unused__)),
                   Sigfunc func __attribute__ ((__unused__)))
{
#if !(defined(HAVE_SIGACTION) || defined(SIGNALS_KEPT))
  signal(signo, func);
#endif
}

/** Set a signal to be ignored.
 * \param signo signal number to ignore.
 */
void
ignore_signal(int signo)
{
#ifdef HAVE_SIGACTION
  struct sigaction act;
  act.sa_handler = SIG_IGN;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(signo, &act, NULL);
#else                           /* No sigaction, drat. */
  signal(signo, SIG_IGN);
#endif
}

/** Set a signal to block.
 * These don't really work right without sigprocmask(), but we try.
 * \param signo signal number to block.
 */
void
block_a_signal(int signo)
{
#ifdef HAVE_SIGPROCMASK
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, signo);
  sigprocmask(SIG_BLOCK, &mask, NULL);
#else
  if (signo > 0 && signo < NSIG)
    saved_handlers[signo] = signal(signo, SIG_IGN);
#endif
}

/** Unblock a signal.
 * These don't really work right without sigprocmask(), but we try.
 * \param signo signal number to unblock.
 */
void
unblock_a_signal(int signo)
{
#ifdef HAVE_SIGPROCMASK
  sigset_t mask;
  if (signo >= 0 && signo < NSIG) {
    sigemptyset(&mask);
    sigaddset(&mask, signo);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
  }
#else
  if (signo >= 0 && signo < NSIG)
    signal(signo, saved_handlers[signo]);
#endif
}

/** Block all signals.
 */
void
block_signals(void)
{
#ifdef HAVE_SIGPROCMASK
  sigset_t mask;
  sigfillset(&mask);
  sigprocmask(SIG_BLOCK, &mask, NULL);
#elif defined(WIN32)
  /* The only signals Windows knows about. Can these even /be/ ignored? */
  saved_handlers[SIGABRT] = signal(SIGABRT, SIG_IGN);
  saved_handlers[SIGFPE] = signal(SIGFPE, SIG_IGN);
  saved_handlers[SIGILL] = signal(SIGILL, SIG_IGN);
  saved_handlers[SIGINT] = signal(SIGINT, SIG_IGN);
  saved_handlers[SIGSEGV] = signal(SIGSEGV, SIG_IGN);
  saved_handlers[SIGTERM] = signal(SIGTERM, SIG_IGN);
#else
  int i;
  for (i = 0; i < NSIG; i++)
    saved_handlers[i] = signal(i, SIG_IGN);
#endif
}
