/**
 * \file info_master.c
 *
 * \brief mush-end functions for talking to info_slave
 */

#include "copyrite.h"
#include "config.h"

#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#if !defined(I_SYS_TIME) || defined(TIME_WITH_SYS_TIME)
#include <time.h>
#endif
#ifdef I_NETDB
#include <netdb.h>
#endif
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "conf.h"
#include "externs.h"
#include "access.h"
#include "mysocket.h"
#include "lookup.h"
#include "log.h"
#include "wait.h"


#ifdef INFO_SLAVE

#ifdef WIN32
#error "info_slave will not work on Windows."
#endif

#ifndef HAVE_SOCKETPAIR
#error "no supported communication options for talking with info_slave are available."
#endif

static bool make_info_slave(void);

static fd_set info_pending; /**< Keep track of fds pending a slave lookup */
static int pending_max = 0;
int info_slave = -1;
pid_t info_slave_pid = -1;      /**< Process id of the info_slave process */
enum is_state info_slave_state = INFO_SLAVE_DOWN;       /**< State of the info_slave process */
time_t info_queue_time; /**< Time of last write to slave */

static int startup_attempts = 0; /**< How many times has info_slave been started? */
static time_t startup_window;
#define MAX_ATTEMPTS 5 /**< Error out after this many startup attempts in 60 seconds */

bool info_slave_halted = false;

 /* From bsd.c */
extern int maxd;
DESC *initializesock(int s, char *addr, char *ip, int use_ssl);

/** Re-query lookups that have timed out */
void
update_pending_info_slaves(void)
{
  time_t now;
  int newsock;

  time(&now);

  if (info_slave_state == INFO_SLAVE_PENDING && now > info_queue_time + 30) {
    /* rerun any pending queries that got lost */
    info_queue_time = now;
    for (newsock = 0; newsock < pending_max; newsock++)
      if (FD_ISSET(newsock, &info_pending))
        query_info_slave(newsock);
  }
}

void
init_info_slave(void)
{
  FD_ZERO(&info_pending);
  make_info_slave();
}

bool
make_info_slave(void)
{
  int socks[2];
  pid_t child;
  int n;

  if (info_slave_state != INFO_SLAVE_DOWN) {
    if (info_slave_pid > 0)
      kill_info_slave();
    info_slave_state = INFO_SLAVE_DOWN;
  }

  if (startup_attempts == 0)
    time(&startup_window);

  startup_attempts += 1;

  if (startup_attempts > MAX_ATTEMPTS) {
    time_t now;

    time(&now);
    if (difftime(now, startup_window) <= 60.0) {
      /* Too many failed attempts to start info_slave in 1 minute */
      do_rawlog(LT_ERR, "Disabling info_slave due to too many errors.");
      info_slave_halted = true;
      return false;
    } else {
      /* Reset counter */
      startup_window = now;
      startup_attempts = 0;
    }
  }
#ifndef AF_LOCAL
  /* Use Posix.1g names. */
#define AF_LOCAL AF_UNIX
#endif

#ifdef HAVE_SOCKETPAIR
  if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, socks) < 0) {
    penn_perror("creating slave datagram socketpair");
    return false;
  }
  if (socks[0] >= maxd)
    maxd = socks[0] + 1;
  if (socks[1] >= maxd)
    maxd = socks[1] + 1;
#endif

  child = fork();
  if (child < 0) {
    penn_perror("forking info slave");
#ifdef HAVE_SOCKETPAIR
    closesocket(socks[0]);
    closesocket(socks[1]);
#endif
    return false;
  } else if (child > 0) {
    info_slave_state = INFO_SLAVE_READY;
    info_slave_pid = child;
#ifdef HAVE_SOCKETPAIR
    info_slave = socks[0];
    closesocket(socks[1]);
    do_rawlog(LT_ERR,
              "Spawning info slave, communicating using socketpair, pid %d.",
              child);
#endif
    make_nonblocking(info_slave);
  } else {
    int errfd = fileno(stderr);
    int dupfd;

    /* Close unneeded fds and sockets: Everything but stderr and the
       socket used to talk to the mush */
    for (n = 0; n < maxd; n++) {
      if (n == errfd)
        continue;
#ifdef HAVE_SOCKETPAIR
      if (n == socks[1])
        continue;
#endif
      close(n);
    }
    /* Reuse stdin and stdout for talking to the slave */
    dupfd = dup2(socks[1], 0);
    if (dupfd < 0) {
      penn_perror("dup2() of stdin in info_slave");
      exit(1);
    }

    dupfd = dup2(socks[1], 1);
    if (dupfd < 0) {
      penn_perror("dup2() of stdout in info_slave");
      exit(1);
    }

    close(socks[1]);

    execl("./info_slave", "info_slave", (char *) NULL);
    penn_perror("execing info slave");
    exit(1);
  }

  if (info_slave >= maxd)
    maxd = info_slave + 1;

  lower_priority_by(info_slave_pid, 4);

  for (n = 0; n < maxd; n++)
    if (FD_ISSET(n, &info_pending))
      query_info_slave(n);

  return true;
}

void
query_info_slave(int fd)
{
  struct request_dgram req;
  struct hostname_info *hi;
  char buf[BUFFER_LEN], *bp;
  ssize_t slen;

  FD_SET(fd, &info_pending);
  if (fd > pending_max)
    pending_max = fd + 1;

  info_queue_time = time(NULL);

  if (info_slave_state == INFO_SLAVE_DOWN) {
    if (!make_info_slave()) {
      FD_CLR(fd, &info_pending);
      closesocket(fd);          /* Just drop the connection if the slave gets halted.
                                   A subsequent reconnect will work. */
    }
    return;
  }


  memset(&req, 0, sizeof req);

  req.rlen = MAXSOCKADDR;
  if (getpeername(fd, (struct sockaddr *) req.remote.data, &req.rlen) < 0) {
    penn_perror("socket peer vanished");
    shutdown(fd, 2);
    closesocket(fd);
    FD_CLR(fd, &info_pending);
    return;
  }

  /* Check for forbidden sites before bothering with ident */
  bp = buf;
  hi = ip_convert(&req.remote.addr, req.rlen);
  safe_str(hi ? hi->hostname : "Not found", buf, &bp);
  *bp = '\0';
  if (Forbidden_Site(buf)) {
    char port[NI_MAXSERV];
    if (getnameinfo(&req.remote.addr, req.rlen, NULL, 0, port, sizeof port,
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
      penn_perror("getting remote port number");
    else {
      if (!Deny_Silent_Site(buf, AMBIGUOUS)) {
        do_log(LT_CONN, 0, 0, "[%d/%s] Refused connection (remote port %s)",
               fd, buf, port);
      }
    }
    closesocket(fd);
    FD_CLR(fd, &info_pending);
    return;
  }

  req.llen = MAXSOCKADDR;
  if (getsockname(fd, (struct sockaddr *) req.local.data, &req.llen) < 0) {
    penn_perror("socket self vanished");
    closesocket(fd);
    FD_CLR(fd, &info_pending);
    return;
  }

  req.fd = fd;
  req.use_dns = USE_DNS;

  slen = send(info_slave, &req, sizeof req, 0);
  if (slen < 0) {
    penn_perror("info slave query: write error");
    make_info_slave();
    return;
  } else if (slen != (int) sizeof req) {
    /* Shouldn't happen! */
    penn_perror("info slave query: partial packet");
    make_info_slave();
    return;
  }
  info_slave_state = INFO_SLAVE_PENDING;
}

extern const char *source_to_s(conn_source);

void
reap_info_slave(void)
{
  struct response_dgram resp;
  ssize_t len;
  char hostname[BUFFER_LEN], *hp;
  int n, count;
  conn_source source;

  if (info_slave_state != INFO_SLAVE_PENDING) {
    if (info_slave_state == INFO_SLAVE_DOWN)
      make_info_slave();
    return;
  }

  len = recv(info_slave, &resp, sizeof resp, 0);
  if (len < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
    return;
  else if (len < 0 || len != (int) sizeof resp) {
    penn_perror("reading info_slave response");
    return;
  }

  /* okay, now we have some info! */
  if (!FD_ISSET(resp.fd, &info_pending)) {
    /* Duplicate or spoof. Ignore. */
    return;
  }

  FD_CLR(resp.fd, &info_pending);

  /* See if we have any other pending queries and change state if not. */
  for (n = 0, count = 0; n < pending_max; n++)
    if (FD_ISSET(n, &info_pending))
      count++;

  if (count == 0) {
    info_slave_state = INFO_SLAVE_READY;
    pending_max = 0;
  }

  hp = hostname;
  if (resp.hostname[0])
    safe_str(resp.hostname, hostname, &hp);
  else
    safe_str(resp.ipaddr, hostname, &hp);
  *hp = '\0';

  if (Forbidden_Site(resp.ipaddr) || Forbidden_Site(hostname)) {
    if (!Deny_Silent_Site(resp.ipaddr, AMBIGUOUS)
        || !Deny_Silent_Site(hostname, AMBIGUOUS)) {
      do_log(LT_CONN, 0, 0, "[%d/%s/%s] Refused connection.", resp.fd,
             hostname, resp.ipaddr);
    }
    shutdown(resp.fd, 2);
    closesocket(resp.fd);
    return;
  }

  if (resp.connected_to == TINYPORT)
    source = CS_IP_SOCKET;
  else if (resp.connected_to == SSLPORT)
    source = CS_OPENSSL_SOCKET;
  else
    source = CS_UNKNOWN;

  do_log(LT_CONN, 0, 0, "[%d/%s/%s] Connection opened from %s.", resp.fd,
         hostname, resp.ipaddr, source_to_s(source));
  set_keepalive(resp.fd, options.keepalive_timeout);


  initializesock(resp.fd, hostname, resp.ipaddr, source);
}

/** Kill the info_slave process, typically at shutdown.
 */
void
kill_info_slave(void)
{
  WAIT_TYPE my_stat;

  if (info_slave_state != INFO_SLAVE_DOWN) {
    if (info_slave_pid > 0) {
      do_rawlog(LT_ERR, "Terminating info_slave pid %d", info_slave_pid);

      block_a_signal(SIGCHLD);
      closesocket(info_slave);
      kill(info_slave_pid, SIGTERM);
      mush_wait(info_slave_pid, &my_stat, 0);
      info_slave_pid = -1;
      unblock_a_signal(SIGCHLD);
    }
    info_slave_state = INFO_SLAVE_DOWN;
  }
}


#endif                          /* INFO_SLAVE */
