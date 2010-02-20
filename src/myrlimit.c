/**
 * \file myrlimit.c
 *
 * \brief Resource limit utilities
 *
 * This file provides routines for modifying system resource limits
 * with getrlimit/setrlimit, and similar stuff.
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef WIN32
#define FD_SETSIZE 256
#include <windows.h>
#include <winsock.h>
#include <io.h>
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <limits.h>
#include <errno.h>

#include "conf.h"
#include "version.h"
#include "externs.h"
#include "confmagic.h"

#ifdef HAS_GETRLIMIT
void init_rlimit(void);
#endif
int how_many_fds(void);

#ifdef WIN32
static WSADATA wsadata;
#endif

#ifdef HAS_GETRLIMIT
void
init_rlimit(void)
{
  /* Unlimit file descriptors. */
  /* Ultrix 4.4 and others may have getrlimit but may not be able to
   * change number of file descriptors
   */
#ifdef RLIMIT_NOFILE
  struct rlimit rlp;

  if (getrlimit(RLIMIT_NOFILE, &rlp)) {
    penn_perror("init_rlimit: getrlimit()");
    return;
  }
  /* This check seems dumb, but apparently FreeBSD may return 0 for
   * the max # of descriptors!
   */
  if (rlp.rlim_max > rlp.rlim_cur) {
    rlp.rlim_cur = rlp.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &rlp))
      penn_perror("init_rlimit: setrlimit()");
  }
#endif
  return;
}
#endif                          /* HAS_GETRLIMIT */

int
how_many_fds(void)
{
  /* Determine how many open file descriptors we're allowed
   * In order, we'll try:
   * 1. sysconf(_SC_OPEN_MAX) - POSIX.1
   * 2. OPEN_MAX constant - POSIX.1 limits.h
   * 3. getdtablesize - BSD (which Config maps to ulimit or NOFILE if needed)
   */
  static int open_max = 0;
#ifdef WIN32
  int iMaxSocketsAllowed;
  unsigned short wVersionRequested = MAKEWORD(1, 1);
  int err;
#endif
  if (open_max)
    return open_max;
#ifdef WIN32
  /* Typically, WIN32 allows many open sockets, but won't perform
   * well if too many are used. The best approach is to give the
   * admin a single point of control (MAX_LOGINS in MUSH.CNF) and then
   * allow a few more connections than that here for clients to get access
   * to an E-mail address or at least a title. 2 is an arbitrary number.
   *
   * If max_logins is set to 0 in mush.cnf (unlimited logins),
   * we'll allocate 120 sockets for now.
   *
   * wsadata.iMaxSockets isn't valid for WinSock versions 2.0
   * and later, but we are requesting version 1.1, so it's valid.
   */

  /* Need to init Windows Sockets to get socket data */
  err = WSAStartup(wVersionRequested, &wsadata);
  if (err) {
    printf("Error %i on WSAStartup\n", err);
    exit(1);
  }
  iMaxSocketsAllowed = options.max_logins ? (2 * options.max_logins) : 120;
  if (wsadata.iMaxSockets < iMaxSocketsAllowed)
    iMaxSocketsAllowed = wsadata.iMaxSockets;
  return iMaxSocketsAllowed;
#else
#ifdef HAS_SYSCONF
  errno = 0;
  if ((open_max = sysconf(_SC_OPEN_MAX)) < 0) {
    if (errno == 0)             /* Value was indeterminate */
      open_max = 0;
  }
  if (open_max)
    return open_max;
#endif
#ifdef OPEN_MAX
  open_max = OPEN_MAX;
  return open_max;
#endif
  /* Caching getdtablesize is dangerous, since it's affected by
   * getrlimit, so we don't.
   */
  open_max = 0;
  return getdtablesize();
#endif                          /* WIN 32 */
}
