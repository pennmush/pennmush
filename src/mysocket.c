/**
 * \file mysocket.c
 *
 * \brief Socket routines for PennMUSH.
 *
 *
 */

#define _GNU_SOURCE

#include "copyrite.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_NETINET_IN_H
#ifdef WIN32
#undef EINTR
#endif
#include <netinet/in.h>
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef HAVE_GETADDRINFO
#ifndef WIN32
#ifdef __APPLE__
#ifdef __APPLE_CC__
#include <nameser.h>
#endif
#endif
#ifndef __CYGWIN__
#include <resolv.h>
#endif
#endif
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif

#include "mysocket.h"

#include "conf.h"
#include "log.h"
#include "mymalloc.h"
#include "wait.h"

/* TODO: Hack until we move mush_panic() somewhere more reasonable. */
void mush_panic(const char *);

static int connect_nonb(int sockfd, const struct sockaddr *saptr,
                        socklen_t salen, bool nonb);

bool
is_blocking_err(int code)
{
#ifdef WIN32
  return code == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK;
#else
  if (code == EWOULDBLOCK)
    return 1;
  if (code == EINTR)
    return 1;
#ifdef EAGAIN
  if (code == EAGAIN)
    return 1;
#endif
  return 0;
#endif
}

#ifndef SLAVE
/** Given a sockaddr structure, try to look up and return hostname info.
 * If we can't get a hostname from DNS (or if we're not using DNS),
 * we settle for the IP address.
 * \param host pointer to a sockaddr structure.
 * \param len length of host structure.
 * \return static hostname_info structure with hostname and port.
 */
struct hostname_info *
hostname_convert(struct sockaddr *host, int len)
{
  static struct hostname_info hi;
  static char hostname[NI_MAXHOST];
  static char port[NI_MAXSERV];

  if (getnameinfo(host, len, hostname, sizeof hostname, port, sizeof port,
                  (USE_DNS ? 0 : NI_NUMERICHOST) | NI_NUMERICSERV) != 0) {
    return NULL;
  }
  hi.hostname = hostname;
  hi.port = port;
  return &hi;
}
#endif

/** Given a sockaddr structure, try to look up and return ip address info.
 * \param host pointer to a sockaddr structure.
 * \param len length of host structure.
 * \return static hostname_info structure with ip address and port.
 */
struct hostname_info *
ip_convert(struct sockaddr *host, int len)
{
  static struct hostname_info hi;
  static char hostname[NI_MAXHOST];
  static char port[NI_MAXSERV];

  if (getnameinfo(host, len, hostname, sizeof hostname, port, sizeof port,
                  NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
    return NULL;
  }
  hi.hostname = hostname;
  hi.port = port;
  return &hi;
}

/** Open a connection to a given host and port. Basically
 * tcp_connect from UNPv1
 * \param host hostname or ip address to connect to, as a string.
 * \param socktype The type of socket - SOCK_STREAM or SOCK_DGRAM
 * \param myiterface pointer to sockaddr structure for specific interface.
 * \param myilen length of myiterface
 * \param port port to connect to.
 * \param nonb true to do a nonblocking connect in the background.
 * \return file descriptor for connected socket, or -1 for failure.
 */
int
make_socket_conn(const char *host, int socktype, struct sockaddr *myiterface,
                 socklen_t myilen, Port_t port, bool nonb)
{
  struct addrinfo hints, *server, *save;
  char cport[NI_MAXSERV];
  int s;
  int res;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC; /* Try to use IPv6 if available */
  hints.ai_socktype = socktype;

  sprintf(cport, "%hu", port);

  if ((res = getaddrinfo(host, cport, &hints, &server)) != 0) {
    lock_file(stderr);
    fprintf(stderr, "In getaddrinfo: %s\n", gai_strerror(res));
    fprintf(stderr, "Host: %s Port %hu\n", host, port);
    fflush(stderr);
    unlock_file(stderr);
    return -1;
  }

  if (!server) {
    lock_file(stderr);
    fprintf(stderr, "Couldn't get address for host %s port %hu\n", host, port);
    fflush(stderr);
    unlock_file(stderr);
    return -1;
  }

  save = server;

  do {
    s = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (s < 0)
      continue;

    if (myiterface && myilen > 0 &&
        myiterface->sa_family == server->ai_family) {
      /* Bind to a specific interface. Don't even try for the case of
       * an IPv4 socket and an IPv6 interface. Happens with ident, which
       * seems to work okay without the bind(). */
      if (bind(s, myiterface, myilen) < 0)
        penn_perror("bind failed (Possibly harmless)");
    }

    if (connect_nonb(s, server->ai_addr, server->ai_addrlen, nonb) == 0)
      break;

#ifdef DEBUG
    penn_perror("connect failed (Probably harmless)");
#endif

    closesocket(s);

  } while ((server = server->ai_next) != NULL);

  freeaddrinfo(save);

  if (server == NULL) {
    lock_file(stderr);
    fprintf(stderr, "Couldn't connect to %s on port %hu\n", host, port);
    fflush(stderr);
    unlock_file(stderr);
    return -1;
  }
  return s;
}

/** Start listening on a given port. Basically tcp_listen
 * from UNPv1
 * \param port port to listen on.
 * \param socktype the type of socket - SOCK_STREAM or SOCK_DGRAM
 * \param addr pointer to sockaddr to copy address data to.
 * \param len length of addr.
 * \param host hostname or address to listen on, as a string.
 * \return file descriptor of listening socket, or -1 for failure.
 */
int
make_socket(Port_t port, int socktype, union sockaddr_u *addr, socklen_t *len,
            const char *host)
{
  int s, opt, ipv = 4;
  /* Use getaddrinfo() to fill in the sockaddr fields. This
   * effectively makes us IPv6 capable if the host is. Much of this is
   * lifted from listing 11.9 (tcp_listen()) in W. Richard Steven's
   * Unix Network Programming, vol 1.  If getaddrinfo() isn't
   * present on the system, we'll use our own version, also from UNPv1. */
  struct addrinfo *server, *save, hints;
  char portbuf[NI_MAXSERV], *cport;
  int res;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_PASSIVE;
#ifdef FORCE_IPV4
  hints.ai_family = AF_INET; /* OpenBSD apparently doesn't properly
                                map IPv4 connections to IPv6 servers. */
#else
  hints.ai_family = AF_UNSPEC; /* Try to use IPv6 if available */
#endif
  hints.ai_socktype = socktype;

  if (port > 0) {
    sprintf(portbuf, "%hu", port);
    cport = portbuf;
  } else
    cport = NULL;

  if (strlen(host) == 0)
    host = NULL;

  if ((res = getaddrinfo(host, cport, &hints, &server)) != 0) {
    fprintf(stderr, "In getaddrinfo: %s\n", gai_strerror(res));
    fprintf(stderr, "Host: %s Port %hu\n", host, port);
    exit(3);
  }

  save = server;

  if (!server) {
    fprintf(stderr, "Couldn't get address for host %s port %hu\n", host, port);
    exit(3);
  }

  do {
    s = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (s < 0)
      continue;

    opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) <
        0) {
      penn_perror("setsockopt (Possibly ignorable)");
      continue;
    }
#ifdef IPV6_V6ONLY
    if (server->ai_family == AF_INET6 && host == NULL) {
      opt = 0;
      if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &opt, sizeof opt) <
          0) {
        penn_perror("setsockopt (Possibly ignorable)");
      }
    }
#endif

    if (bind(s, server->ai_addr, server->ai_addrlen) == 0)
      break; /* Success */

#ifdef WIN32
    if (WSAGetLastError() == WSAEADDRINUSE) {
#else
    if (errno == EADDRINUSE) {
#endif
      fprintf(stderr, "Another process (Possibly another copy of this mush?) "
                      "appears to be using port %hu. Aborting.\n",
              port);
      exit(1);
    }

    penn_perror("binding stream socket (Possibly ignorable)");
    closesocket(s);
  } while ((server = server->ai_next) != NULL);

  if (server == NULL) {
    fprintf(stderr, "Couldn't bind to port %d\n", port);
    exit(4);
  }

  ipv = (server->ai_family == AF_INET) ? 4 : 6;
  if (addr) {
    memcpy(addr->data, server->ai_addr, server->ai_addrlen);
    if (len)
      *len = server->ai_addrlen;
  }

  freeaddrinfo(save);
  fprintf(stderr, "Listening on port %d using IPv%d.\n", port, ipv);
  fflush(stderr);
  listen(s, 5);
  return s;
}

#ifndef WIN32

/** Create a unix-domain socket .
 * \param filename The name of the socket file.
 * \param socktype The type of socket.
 * \return a fd for the socket, or -1 on error.
 */
int
make_unix_socket(const char *filename, int socktype)
{
  int s;
  struct sockaddr_un addr;

  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_LOCAL;
  strncpy(addr.sun_path, filename, sizeof(addr.sun_path) - 1);

  unlink(filename);

  if ((s = socket(AF_LOCAL, socktype, 0)) < 0) {
    perror("socket");
    return -1;
  }

  if (bind(s, (const struct sockaddr *) &addr, sizeof addr) < 0) {
    perror("bind");
    close(s);
    return -1;
  }

  if (listen(s, 5) < 0) {
    perror("listen");
    close(s);
    return -1;
  }

  fprintf(stderr, "Listening on socket file %s (fd %d)\n", filename, s);

  return s;
}

/** Connect to a unix-domain socket
 * \param filename The name of the socket file.
 * \param socktype the type of socket
 * \return a fd for the socket or -1 on error.
 */
int
connect_unix_socket(const char *filename, int socktype)
{
  int s;
  struct sockaddr_un addr;

  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_LOCAL;
  strncpy(addr.sun_path, filename, sizeof(addr.sun_path) - 1);

  if ((s = socket(AF_LOCAL, socktype, 0)) < 0) {
    perror("socket");
    return -1;
  }

  if (connect_nonb(s, (const struct sockaddr *) &addr, sizeof addr, 1) == 0) {
    return s;
  } else {
    int savederrno = errno;
    close(s);
    errno = savederrno;
    return -1;
  }
}
#endif

/* Send data to a unix socket, including sending credentials (uid,
 * pid, etc.) if they must be explicently sent. Used by ssl_slave.
 * You can't lie about credentials unless you're root, so we don't
 * even take them as parameters.
 *
 * \param s the socket descriptor
 * \param buf the buffer to write
 * \param len size of the buffer
 * \return number of bytes written or -1 on failure.
 */
ssize_t
send_with_creds(int s, void *buf, size_t len)
{
  ssize_t slen;
/* Linux and OS X can get credentials on the receiving end via a
   getsockopt() call. Use it instead of sendmsg() because it's a lot
   simpler. */
#if 0
  /* Sample sendmsg() credential passing using linux structs. */
  {
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct ucred *creds;
    uint8_t credbuf[CMSG_SPACE(sizeof *creds)] = { 0 };
    struct iovec iov;
    int sockopt = 1;

    memset(&msg, 0, sizeof msg);

    iov.iov_base = buf;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = credbuf;
    msg.msg_controllen = sizeof credbuf;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_CREDENTIALS;
    cmsg->cmsg_len = CMSG_LEN(sizeof *creds);
    creds = (struct ucred *) CMSG_DATA(cmsg);
    creds->pid = getpid();
    creds->uid = geteuid();
    creds->gid = getegid();

    if (setsockopt(s, SOL_SOCKET, SO_PASSCRED, &sockopt, sizeof sockopt) < 0)
      perror("setsockopt SO_PASSCRED");

    slen = sendmsg(s, &msg, 0);
  }
#else
  slen = send(s, buf, len, 0);
#endif
  return slen;
}

#if defined(__CYGWIN__) || defined(WIN32)
/* There is probably a better way to actually fix (instead of ignore) the
 * lack of MSG_DONTWAIT on cygwin, but since I doubt anyone is actually
 * using the SSL_SLAVE code on cygwin, I'm not bothering. Can be looked into
 * if this assumption is proved wrong. */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#endif

/* Read from a unix socket, getting unix credentials from the other end. Use for
 * authentication when acceping local connections from ssl_slave or the like.
 * \param s the socket descriptor
 * \param buf The buffer to read into
 * \param len the length of the buffer
 * \param remote_pid Non-null pointer that returns the remote side's pid or -1
 * if unable to read credentials.
 * \param remote_uid Non-null pointer that returns the remote side's uid or -1
 * if unable to read credentials.
 * \return the number of bytes read, or -1 on read failure
 */
ssize_t
recv_with_creds(int s, void *buf, size_t len, int *remote_pid, int *remote_uid)
{
  /* Only implemented on linux and OS X so far because different OSes
     support slightly different fields and ways of doing this. Argh.
     We'll prefer getsockopt() approaches over recvmsg() when
     supported. */

  *remote_pid = -1;
  *remote_uid = -1;

#if defined(linux)
  {
    struct ucred creds;
    socklen_t credlen = sizeof creds;

    if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &creds, &credlen) < 0) {
      perror("getsockopt SO_PEERCRED");
    } else {
      *remote_pid = creds.pid;
      *remote_uid = creds.uid;
    }
  }
#elif defined(__NetBSD__)
  {
    struct unpcbid creds;
    socklen_t credlen = sizeof creds;
    if (getsockopt(s, 0, LOCAL_PEEREID, &creds, &credlen) < 0) {
      perror("getsockopt LOCAL_PEEREID");
    } else {
      *remote_pid = creds.unp_pid;
      *remote_uid = creds.unp_euid;
    }
  }
#elif defined(__OpenBSD__)
  {
    struct sockpeercred creds;
    socklen_t credlen = sizeof creds;
    if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &creds, &credlen) < 0) {
      perror("getsockopt SO_PEERCRED");
    } else {
      *remote_pid = creds.pid;
      *remote_uid = creds.uid;
    }
  }
#elif defined(HAVE_STRUCT_XUCRED)
  { /* FreeBSD and OS X */
    struct xucred creds;
    socklen_t credlen = sizeof creds;
    if (getsockopt(s, 0, LOCAL_PEERCRED, &creds, &credlen) < 0) {
      perror("getsockopt LOCAL_PEERCRED");
    } else {
      /* OS X doesn't pass the pid of the process on the other end of
         the socket. */
      *remote_uid = creds.cr_uid;
    }
  }
#endif

  return recv(s, buf, len, MSG_DONTWAIT);
}

/** Make a socket do nonblocking i/o.
 * \param s file descriptor of socket.
 */
void
make_nonblocking(int s)
{
#ifdef WIN32
  unsigned long arg = 1;
  if (ioctlsocket(s, FIONBIO, &arg) == -1) {
    penn_perror("make_nonblocking: ioctlsocket");
#ifndef SLAVE
    mush_panic("Fatal network error!");
#else
    exit(1);
#endif
  }
#else
  int flags;

  if ((flags = fcntl(s, F_GETFL, 0)) == -1) {
    penn_perror("make_nonblocking: fcntl");
#ifndef SLAVE
    mush_panic("Fatal network error!");
#else
    exit(1);
#endif
  }

  flags |= O_NONBLOCK;

  if (fcntl(s, F_SETFL, flags) == -1) {
    penn_perror("make_nonblocking: fcntl");
#ifndef SLAVE
    mush_panic("Fatal network error!");
#else
    exit(1);
#endif
  }
#endif
}

/** Make a socket do blocking i/o.
 * \param s file descriptor of socket.
 */
void
make_blocking(int s)
{
#ifdef WIN32
  unsigned long arg = 0;
  if (ioctlsocket(s, FIONBIO, &arg) == -1) {
    penn_perror("make_blocking: ioctlsocket");
#ifndef SLAVE
    mush_panic("Fatal network error");
#else
    exit(1);
#endif
  }
#else
  int flags;

  if ((flags = fcntl(s, F_GETFL, 0)) == -1) {
    penn_perror("make_blocking: fcntl");
#ifndef SLAVE
    mush_panic("Fatal network error!");
#else
    exit(1);
#endif
  }

  flags &= ~O_NDELAY;
  if (fcntl(s, F_SETFL, flags) == -1) {
    penn_perror("make_nonblocking: fcntl");
#ifndef SLAVE
    mush_panic("Fatal network error!");
#else
    exit(1);
#endif
  }
#endif
}

/** Enable TCP keepalive on the given socket if we can.
 * \param s socket.
 * \param keepidle how often to send keepalive
 */
/* ARGSUSED */
void
set_keepalive(int s __attribute__((__unused__)),
              int keepidle __attribute__((__unused__)))
{
#ifdef SO_KEEPALIVE
  int keepalive = 1;

  /* enable TCP keepalive */
  if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (void *) &keepalive,
                 sizeof(keepalive)) == -1)
    fprintf(stderr, "[%d] could not set SO_KEEPALIVE: %s\n", s,
            strerror(errno));

/* And set the ping time to something reasonable instead of the
   default 2 hours. Linux and possibly others use TCP_KEEPIDLE to do
   this. OS X and possibly others use TCP_KEEPALIVE. */
#if defined(TCP_KEEPIDLE)
  if (setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, (void *) &keepidle,
                 sizeof(keepidle)) == -1)
    fprintf(stderr, "[%d] could not set TCP_KEEPIDLE: %s\n", s,
            strerror(errno));
#elif defined(TCP_KEEPALIVE)
  if (setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE, (void *) &keepidle,
                 sizeof(keepidle)) == -1)
    fprintf(stderr, "[%d] could not set TCP_KEEPALIVE: %s\n", s,
            strerror(errno));
#endif
#endif
  return;
}

/** Connect a socket, possibly making it nonblocking first.
 * From UNP, with changes. If nsec > 0, we set the socket
 * nonblocking and connect with timeout. The socket is STILL
 * nonblocking after it returns. If nsec == 0, a normal blocking
 * connect is done.
 * \param sockfd file descriptor of socket.
 * \param saptr pointer to sockaddr structure with connection data.
 * \param salen length of saptr.
 * \param nonb true for a nonblocking connect in the background
 * \retval 0 success.
 * \retval -1 failure.
 */
int
connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen,
             bool nonb)
{
  int n;

  if (nonb)
    make_nonblocking(sockfd);

  if ((n = connect(sockfd, (const struct sockaddr *) saptr, salen)) < 0)
#ifdef WIN32
    if (n == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
#else
    if (errno != EINPROGRESS)
#endif
      return -1;

  return 0;
}
