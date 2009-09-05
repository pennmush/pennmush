/**
 * \file info_slave.c
 *
 * \brief The information slave process.
 *
 * When running PennMUSH under Unix, a second process (info_slave) is
 * started and the server farms out DNS and ident lookups to the
 * info_slave, and reads responses from the info_slave
 * asynchronously. Communication between server and slave is by means
 * of datagrams on a connected UDP socket.
 *
 * info_slave takes one argument, the descriptor of the local socket.
 *
 */
#include "copyrite.h"
#include "config.h"

#ifdef WIN32
#error "info_slave is not currently supported on Windows"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#endif
#ifdef I_NETDB
#include <netdb.h>
#endif
#include <ctype.h>
#include <string.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef HAVE_SYS_EVENT_H
#include <sys/event.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <errno.h>
#include <signal.h>

#include "conf.h"
#include "externs.h"
#include "wait.h"
#include "ident.h"
#include "mysocket.h"
#include "lookup.h"

#include "confmagic.h"

#ifndef ENOTSUP
#define ENOTSUP EPERM
#endif

void reap_children(void);
static void reaper(int);
int eventwait_init(void);
int eventwait_watch_fd_read(int);
int eventwait_watch_parent_exit(void);
int eventwait_watch_child_exit(void);
int eventwait(void);

void fputerr(const char *);

enum methods { METHOD_KQUEUE, METHOD_POLL, METHOD_SELECT };

enum methods method;

/** How many simultaneous lookup processes can be running? If more
 *  attempts are made after this limit has been reached, the main
 *  slave processes does them sequentially until some of the subslaves
 *  exit. */
enum { MAX_SLAVES = 5 };
sig_atomic_t children = 0;
pid_t child_pids[MAX_SLAVES];

int
main(void)
{
  struct request_dgram req;
  struct response_dgram resp;
  ssize_t len;
  pid_t child, netmush = -2;
  char localport[NI_MAXSERV];

  if (new_process_group() < 0)
    penn_perror("making new process group");

#ifdef HAVE_GETPPID
  netmush = getppid();
#endif

  if (eventwait_init() < 0) {
    penn_perror("init_eventwait");
    return EXIT_FAILURE;
  }

  if (eventwait_watch_fd_read(0) < 0) {
    penn_perror("eventwait_add_fd");
    return EXIT_FAILURE;
  }

  if (eventwait_watch_parent_exit() < 0) {
    penn_perror("eventwait_add_fd");
    return EXIT_FAILURE;
  }

  if (eventwait_watch_child_exit() < 0) {
    penn_perror("eventwait_watch_child_exit");
    return EXIT_FAILURE;
  }

  for (;;) {
    /* grab a request datagram */
    int ev = eventwait();

    if (ev == 0)
      len = recv(0, &req, sizeof req, 0);
    else if (ev == (int) netmush) {
      /* Parent process exited. Exit too. */
      fputerr
        ("info_slave: Parent mush process exited unexpectedly! Shutting down.");
      return EXIT_SUCCESS;
    } else if (ev < 0) {
      /* Error? */
      if (errno != EINTR) {
        penn_perror("eventwait");
        return EXIT_FAILURE;
      } else
        continue;
    } else                      /* ev == 0 */
      continue;

    if (len == -1 && errno == EINTR)
      continue;
    else if (len != (int) sizeof req) {
      /* This shouldn't happen. */
      penn_perror("reading request datagram");
      return EXIT_FAILURE;
    }

    if (children < MAX_SLAVES) {
#ifdef HAVE_FORK
      child = fork();
      if (child < 0) {
        /* Just do the lookup in the main info_slave */
        penn_perror("unable to fork; doing lookup in master slave");
      } else if (child > 0) {
        /* Parent info_slave; wait for the next request. */
        children++;
        continue;
      }
#else
      child = 1;
#endif
    } else
      child = 1;

    /* Now in the child info_slave or the master with a failed fork. Do a
     * lookup and send back to the mush.
     */

    memset(&resp, 0, sizeof resp);
    resp.fd = req.fd;

    if (getnameinfo(&req.remote.addr, req.rlen, resp.ipaddr,
                    sizeof resp.ipaddr, NULL, 0,
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
      strcpy(resp.ipaddr, "An error occured");

    if (getnameinfo(&req.local.addr, req.llen, NULL, 0, localport,
                    sizeof localport, NI_NUMERICHOST | NI_NUMERICSERV) != 0)
      resp.connected_to = -1;
    else
      resp.connected_to = strtol(localport, NULL, 10);

    if (req.use_ident) {
      IDENT *ident_result;
      int timeout = req.timeout;
      ident_result =
        ident_query(&req.local.addr, req.llen, &req.remote.addr, req.rlen,
                    &timeout);
      if (ident_result && ident_result->identifier) {
        strncpy(resp.ident, ident_result->identifier, sizeof resp.ident);
        resp.ident[(sizeof resp.ident) - 1] = '\0';
      }

      if (ident_result)
        ident_free(ident_result);
    }

    if (req.use_dns) {
      if (getnameinfo(&req.remote.addr, req.rlen, resp.hostname,
                      sizeof resp.hostname, NULL, 0, NI_NUMERICSERV) != 0)
        strcpy(resp.hostname, resp.ipaddr);
    } else
      strcpy(resp.hostname, resp.ipaddr);

    len = send(1, &resp, sizeof resp, 0);

    /* Should never happen. */
    if (len != (int) sizeof resp) {
      penn_perror("error writing packet");
      return EXIT_FAILURE;
    }

    if (child == 0)
      return EXIT_SUCCESS;
  }

  return EXIT_SUCCESS;
}

void
reap_children(void)
{
  WAIT_TYPE status;
  while (mush_wait(-1, &status, WNOHANG) > 0)
    children--;
}

static void
reaper(int signo)
{
  reap_children();
  reload_sig_handler(signo, reaper);
}

/* Event watching code that tries to use various system-dependant ways
 * of waiting for a variety of events.  In particular, on BSD
 * (Including OS X) systems, it uses kqueue()/kevent() to wait for a
 * fd to be readable or a process to exit.  On others, it uses poll(2)
 * or select(2) with a timeout and periodic checking of getppid() to
 * see if the parent netmush process still exists.
 */

#define HAVE_SELECT

#ifdef HAVE_SELECT
static fd_set readers;
static int maxd = 0;
static pid_t parent_pid = 0;
#endif

#ifdef HAVE_KQUEUE
static int kqueue_id = -1;
#endif

#ifdef HAVE_POLL
static struct pollfd *poll_fds = NULL;
static int pollfd_len = 0;
#endif


/** Initialize event loop 
 * \return 0 on success, -1 on failure 
 */
int
eventwait_init(void)
{
#ifdef HAVE_KQUEUE
  kqueue_id = kqueue();
  lock_file(stderr);
  fputs("info_slave: trying kqueue event loop... ", stderr);
  if (kqueue_id < 0) {
    unlock_file(stderr);
    penn_perror("error");
  } else {
    fputs("ok. Using kqueue!\n", stderr);
    unlock_file(stderr);
  }

  if (kqueue_id >= 0) {
    method = METHOD_KQUEUE;
    return 0;
  } else
#endif
#ifdef HAVE_POLL
  if (1) {
    fputerr("info_slave: trying poll event loop... ok. Using poll.");
    method = METHOD_POLL;
    return 0;
  } else
#endif
#ifdef HAVE_SELECT
  if (1) {
    fputerr("info_slave: trying select event loop... ok. Using select.");
    FD_ZERO(&readers);
    method = METHOD_SELECT;
    return 0;
  } else
#endif
  {
    fputerr("info_slave: No working event loop method!");
    errno = ENOTSUP;
    return -1;
  }
}

/** Add a file descriptor to check for read events.
 * Any number of descriptors can be added.
 * \param fd the descriptor
 * \return 0 on success, -1 on failure
 */
int
eventwait_watch_fd_read(int fd)
{
  switch (method) {
#ifdef HAVE_KQUEUE
  case METHOD_KQUEUE:{
      struct kevent add;
      struct timespec timeout;

      memset(&add, 0, sizeof add);
      add.ident = fd;
      add.flags = EV_ADD | EV_ENABLE;
      add.filter = EVFILT_READ;
      timeout.tv_sec = 0;
      timeout.tv_nsec = 0;

      return kevent(kqueue_id, &add, 1, NULL, 0, &timeout);
    }
#endif
#ifdef HAVE_POLL
  case METHOD_POLL:
    poll_fds = realloc(poll_fds, sizeof(struct pollfd) * (pollfd_len + 1));
    poll_fds[pollfd_len].fd = fd;
    poll_fds[pollfd_len].events = POLLIN;
    pollfd_len += 1;
    return 0;
#endif
#ifdef HAVE_SELECT
  case METHOD_SELECT:
    FD_SET(fd, &readers);
    if (fd >= maxd)
      maxd = fd + 1;
    return 0;
#endif
  default:
    return -1;
  }
}

/** Monitor parent process for exiting. 
 * \return 0 on success, -1 on error.
 */
int
eventwait_watch_parent_exit(void)
{
  pid_t parent;


#ifdef HAVE_GETPPID
  parent = getppid();
#else
  errno = ENOTSUP;
  return -1;
#endif

  switch (method) {
#ifdef HAVE_KQUEUE
  case METHOD_KQUEUE:{
      struct kevent add;
      struct timespec timeout;

      memset(&add, 0, sizeof(add));
      add.ident = parent;
      add.flags = EV_ADD | EV_ENABLE;
      add.filter = EVFILT_PROC;
      add.fflags = NOTE_EXIT;

      timeout.tv_sec = 0;
      timeout.tv_nsec = 0;

      return kevent(kqueue_id, &add, 1, NULL, 0, &timeout);
    }
#endif
#ifdef HAVE_POLL
  case METHOD_POLL:            /* Fall through */
#endif
#ifdef HAVE_SELECT
  case METHOD_SELECT:
    parent_pid = parent;
    return 0;
#endif
  default:
    errno = ENOTSUP;
    return -1;
  }
}

/* Arrange to automatically reap exited child processes */
int
eventwait_watch_child_exit(void)
{
  switch (method) {
#ifdef HAVE_KQUEUE
  case METHOD_KQUEUE:{
      struct kevent add;
      struct timespec timeout;

#ifdef HAVE_SIGPROCMASK
      sigset_t chld_mask;

      sigemptyset(&chld_mask);
      sigaddset(&chld_mask, SIGCHLD);
#endif

      memset(&add, 0, sizeof(add));
      add.filter = EVFILT_SIGNAL;
      add.ident = SIGCHLD;
      add.flags = EV_ADD | EV_ENABLE;

      timeout.tv_sec = 0;
      timeout.tv_nsec = 0;

      if (sigprocmask(SIG_BLOCK, &chld_mask, NULL) < 0)
        return -1;

      return kevent(kqueue_id, &add, 1, NULL, 0, &timeout);
    }
#endif
  default:
    install_sig_handler(SIGCHLD, reaper);
    return 0;
  }
}

/** Wait for an event to occur. Only returns on error or when something
 * happens.
 * \return The file descriptor or pid of a triggered event, or -1 on error.
 */
int
eventwait(void)
{
  switch (method) {
#ifdef HAVE_KQUEUE
  case METHOD_KQUEUE:{
      struct kevent triggered[2];
      int res;

      while (1) {
        res = kevent(kqueue_id, NULL, 0, triggered, 2, NULL);
        if (res == 1) {
          if (triggered[0].filter == EVFILT_SIGNAL) {
            reap_children();
            continue;
          } else
            return triggered[0].ident;
        } else if (res == 2) {
          if (triggered[0].filter == EVFILT_SIGNAL) {
            reap_children();
            return triggered[1].ident;
          } else if (triggered[1].filter == EVFILT_SIGNAL) {
            reap_children();
            return triggered[0].ident;
          }
        } else if (res < 0)
          return -1;
      }
    }
#endif
#if defined(HAVE_POLL)
  case METHOD_POLL:{
      /* It's more complex to use poll(), since it can only poll
       * file descriptor events, not process events too. Wake up every
       * 5 seconds to see if the given pid has turned into 1.
       */
      int timeout;
      int res;

      if (parent_pid > 0)
        timeout = 5000;
      else
        timeout = -1;

      while (1) {
        res = poll(poll_fds, pollfd_len, timeout);
        if (res > 0) {
          int n;
          for (n = 0; n < pollfd_len; n++)
            if (poll_fds[n].revents & POLLIN)
              return poll_fds[n].fd;
        } else if (res == 0 && parent_pid) {
#ifdef HAVE_GETPPID
          if (getppid() == 1)
            /* Parent rocess no longer exists; parent is now init */
            return parent_pid;
#endif
        } else if (res < 0)
          return -1;
      }
    }
#endif
#ifdef HAVE_SELECT
  case METHOD_SELECT:{
      /* It's more complex to use select(), since it can only poll
       * file descriptor events, not process events too. Wake up every
       * 5 seconds to see if the given pid has turned into 1.
       */
      struct timeval timeout, *tout;
      int res;

      if (parent_pid > 0)
        tout = &timeout;
      else
        tout = NULL;

      while (1) {
        fd_set local_readers;
        int n;

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        FD_ZERO(&local_readers);

        for (n = 0; n < maxd; n++)
          if (FD_ISSET(n, &readers))
            FD_SET(n, &local_readers);

        res = select(maxd, &local_readers, NULL, NULL, tout);
        if (res > 0) {
          int n;
          for (n = 0; n < maxd; n++)
            if (FD_ISSET(n, &local_readers))
              return n;
        } else if (res == 0 && parent_pid) {
#ifdef HAVE_GETPPID
          if (getppid() == 1)
            /* Parent process no longer exists; parent is now init */
            return parent_pid;
#endif
        } else if (res < 0)
          return -1;
      }
    }
#endif
  default:
    return -1;
  }
}

/* Wrappers for perror */
void
penn_perror(const char *err)
{
  lock_file(stderr);
  fprintf(stderr, "info_slave: %s: %s\n", err, strerror(errno));
  unlock_file(stderr);
}

/* Wrapper for fputs(foo,stderr) */
void
fputerr(const char *msg)
{
  lock_file(stderr);
  fputs(msg, stderr);
  fputc('\n', stderr);
  unlock_file(stderr);
}
