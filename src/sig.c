/**
 * \file sig.c
 *
 * \brief Signal handling routines for PennMUSH.
 *
 *
 */

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif
#include <fcntl.h>
#include <unistd.h>

#include "conf.h"
#include "mysocket.h"
#include "sig.h"

int sigrecv_fd = -1;
int signotifier_fd = -1;

#ifndef HAVE_SIGPROCMASK
static Sigfunc saved_handlers[NSIG];
#endif

#ifndef WIN32
/* Since we install restartable signal handler calls, we have to have a way to
 * tell the
 * main game loop that a signal has been received. Use a pipe, or on linux, an
 * eventfd. */

/** Set up signal notification pipeline. Should only be called once. */
void
sigrecv_setup(void)
{
#ifdef HAVE_EVENTFD
  sigrecv_fd = signotifier_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (sigrecv_fd < 0)
    perror("sigrecv_setup: eventfd");
#else
  int fds[2];

#ifdef HAVE_PIPE2
  if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) < 0) {
    perror("sigrecv_setup: pipe2");
    return;
  }

  sigrecv_fd = fds[0];
  signotifier_fd = fds[1];

#else
  int flags;

  if (pipe(fds) < 0) {
    perror("sigrecv_setup: pipe");
    return;
  }
  sigrecv_fd = fds[0];
  signotifier_fd = fds[1];
  set_close_exec(sigrecv_fd);
  make_nonblocking(sigrecv_fd);
  set_close_exec(signotifier_fd);
  make_nonblocking(signotifier_fd);
#endif
#endif
}

/** Called by signal handler functions to announce a signal has been
 * received. */
void
sigrecv_notify(void)
{
  int64_t data = 1;
  if (write(signotifier_fd, &data, sizeof data) < 0) {
    perror("sigrecv_notify: write");
  }
}

/** Called by shovechars() to acknowledge that a signal has been received.
 */
void
sigrecv_ack(void)
{
  int64_t data;
  if (read(sigrecv_fd, &data, sizeof data) < 0) {
    if (errno != EAGAIN)
      perror("sigrecv_ack: read");
  }
}

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
#else /* No sigaction, drat. */
  return signal(signo, func);
#endif
}

/** Reinstall a signal handler.
 * \param signo the signal number.
 * \param func signal handler function to reload.
 */
void
reload_sig_handler(int signo __attribute__((__unused__)),
                   Sigfunc func __attribute__((__unused__)))
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
#else /* No sigaction, drat. */
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
