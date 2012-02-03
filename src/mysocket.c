/**
 * \file mysocket.c
 *
 * \brief Socket routines for PennMUSH.
 *
 *
 */
#include "copyrite.h"

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#endif

#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif

#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif

#ifdef I_NETINET_IN
#ifdef WIN32
#undef EINTR
#endif
#include <netinet/in.h>
#else
#ifdef I_SYS_IN
#include <sys/in.h>
#endif
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifdef I_NETINET_TCP
#include <netinet/tcp.h>
#endif

#ifdef I_ARPA_INET
#include <arpa/inet.h>
#endif

#ifdef I_UNISTD
#include <unistd.h>
#endif

#ifndef HAS_GETADDRINFO
#ifdef I_ARPA_NAMESER
#include <arpa/nameser.h>
#endif
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

#ifdef I_NETDB
#include <netdb.h>
#endif

#if !defined(HAVE_H_ERRNO) && !defined(WIN32)
extern int h_errno;
#endif

#include <errno.h>

#ifdef I_FCNTL
#include <fcntl.h>
#endif

#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

#ifdef I_SYS_TIME
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

#ifdef I_SYS_SELECT
#include <sys/select.h>
#endif

#include "conf.h"
#include "externs.h"
#include "mymalloc.h"
#include "mysocket.h"
#include "confmagic.h"

static int connect_nonb
  (int sockfd, const struct sockaddr *saptr, socklen_t salen, bool nonb);

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
  hints.ai_family = AF_UNSPEC;  /* Try to use IPv6 if available */
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

    if (myiterface && myilen > 0 && myiterface->sa_family == server->ai_family) {
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
  hints.ai_family = AF_INET;    /* OpenBSD apparently doesn't properly
                                   map IPv4 connections to IPv6 servers. */
#else
  hints.ai_family = AF_UNSPEC;  /* Try to use IPv6 if available */
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
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
      penn_perror("setsockopt (Possibly ignorable)");
      continue;
    }

    if (bind(s, server->ai_addr, server->ai_addrlen) == 0)
      break;                    /* Success */

#ifdef WIN32
    if (WSAGetLastError() == WSAEADDRINUSE) {
#else
    if (errno == EADDRINUSE) {
#endif
      fprintf(stderr,
              "Another process (Possibly another copy of this mush?) appears to be using port %hu. Aborting.\n",
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
 * \param socktyp the type of socket
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

  if (connect_nonb(s, (const struct sockaddr *) &addr, sizeof addr, 1) == 0)
    return s;
  else {
    close(s);
    return -1;
  }
}


#endif

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

  flags |= O_NDELAY;

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
 */
/* ARGSUSED */
void
set_keepalive(int s __attribute__ ((__unused__)), int keepidle
              __attribute__ ((__unused__)))
{
#ifdef SO_KEEPALIVE
  int keepalive = 1;

  /* enable TCP keepalive */
  if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
                 (void *) &keepalive, sizeof(keepalive)) == -1)
    fprintf(stderr, "[%d] could not set SO_KEEPALIVE: %s\n", s,
            strerror(errno));

  /* And set the ping time to something reasonable instead of the
     default 2 hours. Linux and possibly others use TCP_KEEPIDLE to do
     this. OS X and possibly others use TCP_KEEPALIVE. */
#if defined(TCP_KEEPIDLE)
  if (setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE,
                 (void *) &keepidle, sizeof(keepidle)) == -1)
    fprintf(stderr, "[%d] could not set TCP_KEEPIDLE: %s\n", s,
            strerror(errno));
#elif defined(TCP_KEEPALIVE)
  if (setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE,
                 (void *) &keepidle, sizeof(keepidle)) == -1)
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

/** Wait up to N seconds for a non-blocking connect to establish.
 * \param s the socket
 * \param secs timeout value
 * \return -1 on error, 0 if the socket is not yet connected, >0 on success
 */
int
wait_for_connect(int s, int secs)
{
  int res;
#ifdef HAVE_POLL
  struct pollfd ev;

  ev.fd = s;
  ev.events = POLLOUT;
  if ((res = poll(&ev, 1, secs)) <= 0) {
    if (res == 0)
      errno = EINPROGRESS;
    return res;
  } else {
    errno = ENOTCONN;
    return ev.revents & POLLOUT;
  }
#else
  fd_set wrs;
  struct timeval timeout, *to;

  FD_ZERO(&wrs);
  FD_SET(s, &wrs);
  timeout.tv_sec = secs;
  timeout.tv_usec = 0;
  if (secs >= 0)
    to = &timeout;
  if ((res = select(s + 1, NULL, &wrs, NULL, to)) <= 0) {
#ifndef WIN32
    if (res == 0)
      errno = EINPROGRESS;
#endif
    return res;
  } else
    return FD_ISSET(s, &wrs);
#endif
}

/* The following functions are from W. Ridhard Steven's libgai,
 * modified by Shawn Wagner for PennMUSH. These aren't full
 * implementations- they don't handle unix-domain sockets or named
 * services, because we don't need them for what we're doing.  */

#ifdef AF_INET
#define IPv4
#endif
#ifdef AF_INET6
#define IPv6
#endif

#ifndef HAS_INET_PTON
/* This is from the BIND 4.9.4 release, modified to compile by itself */

/* Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */


#define IN6ADDRSZ       16
#define INADDRSZ         4
#define INT16SZ          2

#ifndef AF_INET6
#define AF_INET6        AF_MAX+1        /* just to let this compile */
#endif

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static const char *inet_ntop4(const unsigned char *src, char *dst, size_t size);
static const char *inet_ntop6(const unsigned char *src, char *dst, size_t size);

/* char *
 * inet_ntop(af, src, dst, size)
 *      convert a network format address to presentation format.
 * return:
 *      pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *      Paul Vixie, 1996.
 */
const char *
inet_ntop(int af, const void *src, char *dst, size_t size)
{
  switch (af) {
  case AF_INET:
    return (inet_ntop4(src, dst, size));
  case AF_INET6:
    return (inet_ntop6(src, dst, size));
  default:
    errno = EAFNOSUPPORT;
    return (NULL);
  }
  /* NOTREACHED */
}

/* const char *
 * inet_ntop4(src, dst, size)
 *      format an IPv4 address, more or less like inet_ntoa()
 * return:
 *      `dst' (as a const)
 * notes:
 *      (1) uses no statics
 *      (2) takes a unsigned char* not an in_addr as input
 * author:
 *      Paul Vixie, 1996.
 */
static const char *
inet_ntop4(const unsigned char *src, char *dst, size_t size)
{
  static const char fmt[] = "%u.%u.%u.%u";
  char tmp[sizeof "255.255.255.255"];

  sprintf(tmp, fmt, src[0], src[1], src[2], src[3]);
  if (strlen(tmp) > size) {
    errno = ENOSPC;
    return (NULL);
  }
  strcpy(dst, tmp);
  return (dst);
}

/* const char *
 * inet_ntop6(src, dst, size)
 *      convert IPv6 binary address into presentation (printable) format
 * author:
 *      Paul Vixie, 1996.
 */
static const char *
inet_ntop6(const unsigned char *src, char *dst, size_t size)
{
  /*
   * Note that int32_t and int16_t need only be "at least" large enough
   * to contain a value of the specified size.  On some systems, like
   * Crays, there is no such thing as an integer variable with 16 bits.
   * Keep this in mind if you think this function should have been coded
   * to use pointer overlays.  All the world's not a VAX.
   */
  char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
  struct {
    int base, len;
  } best, cur;
  unsigned int words[IN6ADDRSZ / INT16SZ];
  int i;

  /*
   * Preprocess:
   *    Copy the input (bytewise) array into a wordwise array.
   *    Find the longest run of 0x00's in src[] for :: shorthanding.
   */
  memset(words, 0, sizeof words);
  for (i = 0; i < IN6ADDRSZ; i++)
    words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
  best.base = -1;
  cur.base = -1;
  for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
    if (words[i] == 0) {
      if (cur.base == -1)
        cur.base = i, cur.len = 1;
      else
        cur.len++;
    } else {
      if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len)
          best = cur;
        cur.base = -1;
      }
    }
  }
  if (cur.base != -1) {
    if (best.base == -1 || cur.len > best.len)
      best = cur;
  }
  if (best.base != -1 && best.len < 2)
    best.base = -1;

  /*
   * Format the result.
   */
  tp = tmp;
  for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
    /* Are we inside the best run of 0x00's? */
    if (best.base != -1 && i >= best.base && i < (best.base + best.len)) {
      if (i == best.base)
        *tp++ = ':';
      continue;
    }
    /* Are we following an initial run of 0x00s or any real hex? */
    if (i != 0)
      *tp++ = ':';
    /* Is this address an encapsulated IPv4? */
    if (i == 6 && best.base == 0 &&
        (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
      if (!inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
        return (NULL);
      tp += strlen(tp);
      break;
    }
    sprintf(tp, "%x", words[i]);
    tp += strlen(tp);
  }
  /* Was it a trailing run of 0x00's? */
  if (best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
    *tp++ = ':';
  *tp++ = '\0';

  /*
   * Check for overflow, copy, and we're done.
   */
  if ((size_t) (tp - tmp) > size) {
    errno = ENOSPC;
    return (NULL);
  }
  strcpy(dst, tmp);
  return (dst);
}


/* This is from the BIND 4.9.4 release, modified to compile by itself */

/* Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */


/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static int inet_pton4(const char *src, unsigned char *dst);
static int inet_pton6(const char *src, unsigned char *dst);

/* int
 * inet_pton(af, src, dst)
 *      convert from presentation format (which usually means ASCII printable)
 *      to network format (which is usually some kind of binary format).
 * return:
 *      1 if the address was valid for the specified address family
 *      0 if the address wasn't valid (`dst' is untouched in this case)
 *      -1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *      Paul Vixie, 1996.
 */
int
inet_pton(int af, const char *src, void *dst)
{
  switch (af) {
  case AF_INET:
    return (inet_pton4(src, dst));
  case AF_INET6:
    return (inet_pton6(src, dst));
  default:
    errno = EAFNOSUPPORT;
    return (-1);
  }
  /* NOTREACHED */
}

/* int
 * inet_pton4(src, dst)
 *      like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *      1 if `src' is a valid dotted quad, else 0.
 * notice:
 *      does not touch `dst' unless it's returning 1.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, unsigned char *dst)
{
  static const char digits[] = "0123456789";
  int saw_digit, octets, ch;
  unsigned char tmp[INADDRSZ], *tp;

  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;
  while ((ch = *src++) != '\0') {
    const char *pch;

    if ((pch = strchr(digits, ch)) != NULL) {
      unsigned int new = *tp * 10 + (pch - digits);

      if (new > 255)
        return (0);
      *tp = new;
      if (!saw_digit) {
        if (++octets > 4)
          return (0);
        saw_digit = 1;
      }
    } else if (ch == '.' && saw_digit) {
      if (octets == 4)
        return (0);
      *++tp = 0;
      saw_digit = 0;
    } else
      return (0);
  }
  if (octets < 4)
    return (0);
  /* bcopy(tmp, dst, INADDRSZ); */
  memcpy(dst, tmp, INADDRSZ);
  return (1);
}

/* int
 * inet_pton6(src, dst)
 *      convert presentation level address to network order binary form.
 * return:
 *      1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *      (1) does not touch `dst' unless it's returning 1.
 *      (2) :: in a full address is silently ignored.
 * credit:
 *      inspired by Mark Andrews.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton6(const char *src, unsigned char *dst)
{
  static const char xdigits_l[] = "0123456789abcdef",
    xdigits_u[] = "0123456789ABCDEF";
  unsigned char tmp[IN6ADDRSZ], *tp, *endp, *colonp;
  const char *xdigits, *curtok;
  int ch, saw_xdigit;
  unsigned int val;

  memset((tp = tmp), 0, IN6ADDRSZ);
  endp = tp + IN6ADDRSZ;
  colonp = NULL;
  /* Leading :: requires some special handling. */
  if (*src == ':')
    if (*++src != ':')
      return (0);
  curtok = src;
  saw_xdigit = 0;
  val = 0;
  while ((ch = *src++) != '\0') {
    const char *pch;

    if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
      pch = strchr((xdigits = xdigits_u), ch);
    if (pch != NULL) {
      val <<= 4;
      val |= (pch - xdigits);
      if (val > 0xffff)
        return (0);
      saw_xdigit = 1;
      continue;
    }
    if (ch == ':') {
      curtok = src;
      if (!saw_xdigit) {
        if (colonp)
          return (0);
        colonp = tp;
        continue;
      }
      if (tp + INT16SZ > endp)
        return (0);
      *tp++ = (unsigned char) (val >> 8) & 0xff;
      *tp++ = (unsigned char) val & 0xff;
      saw_xdigit = 0;
      val = 0;
      continue;
    }
    if (ch == '.' && ((tp + INADDRSZ) <= endp) && inet_pton4(curtok, tp) > 0) {
      tp += INADDRSZ;
      saw_xdigit = 0;
      break;                    /* '\0' was seen by inet_pton4(). */
    }
    return (0);
  }
  if (saw_xdigit) {
    if (tp + INT16SZ > endp)
      return (0);
    *tp++ = (unsigned char) (val >> 8) & 0xff;
    *tp++ = (unsigned char) val & 0xff;
  }
  if (colonp != NULL) {
    /*
     * Since some memmove()'s erroneously fail to handle
     * overlapping regions, we'll do the shift by hand.
     */
    const int n = tp - colonp;
    int i;

    for (i = 1; i <= n; i++) {
      endp[-i] = colonp[n - i];
      colonp[n - i] = 0;
    }
    tp = endp;
  }
  if (tp != endp)
    return (0);
  /* bcopy(tmp, dst, IN6ADDRSZ); */
  memcpy(dst, tmp, IN6ADDRSZ);
  return (1);
}
#endif

#ifndef HAS_GETNAMEINFO
static int gn_ipv46(char *, size_t, char *, size_t, void *, size_t,
                    int, int, int);

/*
 * Handle either an IPv4 or an IPv6 address and port.
 */

/* include gn_ipv46 */
static int
gn_ipv46(char *host, size_t hostlen, char *serv, size_t servlen,
         void *aptr, size_t alen, int family, int port, int flags)
{
  char *ptr;
  struct hostent *hptr;

  if (hostlen > 0) {
    if (flags & NI_NUMERICHOST) {
      if (inet_ntop(family, aptr, host, hostlen) == NULL)
        return (1);
    } else {
      hptr = gethostbyaddr(aptr, alen, family);
      if (hptr != NULL && hptr->h_name != NULL) {
        if (flags & NI_NOFQDN) {
          if ((ptr = strchr(hptr->h_name, '.')) != NULL)
            *ptr = 0;           /* overwrite first dot */
        }
#ifdef HAS_SNPRINTF
        snprintf(host, hostlen, "%s", hptr->h_name);
#else
        strncpy(host, hptr->h_name, hostlen);
        host[hostlen - 1] = '\0';
#endif
      } else {
        if (flags & NI_NAMEREQD)
          return (1);
        if (inet_ntop(family, aptr, host, hostlen) == NULL)
          return (1);
      }
    }
  }

  if (servlen > 0) {
    if (flags & NI_NUMERICSERV) {
#ifdef HAS_SNPRINTF
      snprintf(serv, servlen, "%hu", ntohs((unsigned short) port));
#else
      sprintf(serv, "%hu", ntohs((unsigned short) port));
#endif

    } else {
      /* We're not bothering with getservbyport */
#ifdef HAS_SNPRINTF
      snprintf(serv, servlen, "%hu", ntohs((unsigned short) port));
#else
      sprintf(serv, "%hu", ntohs((unsigned short) port));
#endif
    }
  }
  return (0);
}

/* end gn_ipv46 */


/* include getnameinfo */
int
getnameinfo(const struct sockaddr *sa, socklen_t salen
            __attribute__ ((__unused__)), char *host, size_t hostlen,
            char *serv, size_t servlen, int flags)
{

  switch (sa->sa_family) {
#ifdef  IPv4
  case AF_INET:{
      struct sockaddr_in *sain = (struct sockaddr_in *) sa;

      return (gn_ipv46(host, hostlen, serv, servlen,
                       &sain->sin_addr, sizeof(struct in_addr),
                       AF_INET, sain->sin_port, flags));
    }
#endif

#ifdef  HAVE_SOCKADDR_IN6
  case AF_INET6:{
      struct sockaddr_in6 *sain = (struct sockaddr_in6 *) sa;

      return (gn_ipv46(host, hostlen, serv, servlen,
                       &sain->sin6_addr, sizeof(struct in6_addr),
                       AF_INET6, sain->sin6_port, flags));
    }
#endif

  default:
    return (1);
  }
}

/* end getnameinfo */

#endif

#ifndef HAS_GETADDRINFO

/* include ga1 */
struct search {
  const char *host;             /**< hostname or address string */
  int family;                   /**< AF_xxx */
};

static int ga_aistruct(struct addrinfo ***, const struct addrinfo *,
                       const void *, int);
static struct addrinfo *ga_clone(struct addrinfo *);
static int ga_echeck(const char *, const char *, int, int, int, int);
static int ga_nsearch(const char *, const struct addrinfo *, struct search *);
static int ga_port(struct addrinfo *, int, int);
static int ga_serv(struct addrinfo *, const struct addrinfo *, const char *);


int
getaddrinfo(const char *hostname, const char *servname,
            const struct addrinfo *hintsp, struct addrinfo **result)
{
  int rc, error, nsearch;
  char **ap, *canon;
  struct hostent *hptr;
  struct search search[3], *sptr;
  struct addrinfo hints, *aihead, **aipnext;

  /*
   * If we encounter an error we want to free() any dynamic memory
   * that we've allocated.  This is our hack to simplify the code.
   */
#define error(e) { error = (e); goto bad; }

  aihead = NULL;                /* initialize automatic variables */
  aipnext = &aihead;
  canon = NULL;

  if (hintsp == NULL) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
  } else
    hints = *hintsp;            /* struct copy */

  /* 4first some basic error checking */
  if ((rc = ga_echeck(hostname, servname, hints.ai_flags, hints.ai_family,
                      hints.ai_socktype, hints.ai_protocol)) != 0)
    error(rc);

  /* end ga1 */

  /* include ga3 */
  /* 4remainder of function for IPv4/IPv6 */
  nsearch = ga_nsearch(hostname, &hints, &search[0]);
  for (sptr = &search[0]; sptr < &search[nsearch]; sptr++) {
#ifdef  IPv4
    /* 4check for an IPv4 dotted-decimal string */
    if (isdigit((unsigned char) sptr->host[0])) {
      struct in_addr inaddr;

      if (inet_pton(AF_INET, sptr->host, &inaddr) == 1) {
        if (hints.ai_family != AF_UNSPEC && hints.ai_family != AF_INET)
          error(EAI_ADDRFAMILY);
        if (sptr->family != AF_INET)
          continue;             /* ignore */
        rc = ga_aistruct(&aipnext, &hints, &inaddr, AF_INET);
        if (rc != 0)
          error(rc);
        continue;
      }
    }
#endif

#ifdef  HAVE_SOCKADDR_IN6
    /* 4check for an IPv6 hex string */
    if ((isxdigit((unsigned char) sptr->host[0]) || sptr->host[0] == ':') &&
        (strchr(sptr->host, ':') != NULL)) {
      struct in6_addr in6addr;

      if (inet_pton(AF_INET6, sptr->host, &in6addr) == 1) {
        if (hints.ai_family != AF_UNSPEC && hints.ai_family != AF_INET6)
          error(EAI_ADDRFAMILY);
        if (sptr->family != AF_INET6)
          continue;             /* ignore */
        rc = ga_aistruct(&aipnext, &hints, &in6addr, AF_INET6);
        if (rc != 0)
          error(rc);
        continue;
      }
    }
#endif
/* end ga3 */
/* include ga4 */
    /* 4remainder of for() to look up hostname */
#ifdef  HAVE_SOCKADDR_IN6
    if ((_res.options & RES_INIT) == 0)
      res_init();               /* need this to set _res.options */
#endif

    if (nsearch == 2) {
#ifdef  HAVE_SOCKADDR_IN6
      _res.options &= ~RES_USE_INET6;
#endif
      hptr = gethostbyname2(sptr->host, sptr->family);
    } else {
#ifdef  HAVE_SOCKADDR_IN6
      if (sptr->family == AF_INET6)
        _res.options |= RES_USE_INET6;
      else
        _res.options &= ~RES_USE_INET6;
#endif
      hptr = gethostbyname(sptr->host);
    }
    if (hptr == NULL) {
      if (nsearch == 2)
        continue;               /* failure OK if multiple searches */

      switch (h_errno) {
      case HOST_NOT_FOUND:
        error(EAI_NONAME);
      case TRY_AGAIN:
        error(EAI_AGAIN);
      case NO_RECOVERY:
        error(EAI_FAIL);
      case NO_DATA:
        error(EAI_NODATA);
      default:
        error(EAI_NONAME);
      }
    }

    /* 4check for address family mismatch if one specified */
    if (hints.ai_family != AF_UNSPEC && hints.ai_family != hptr->h_addrtype)
      error(EAI_ADDRFAMILY);

    /* 4save canonical name first time */
    if (hostname != NULL && hostname[0] != '\0' &&
        (hints.ai_flags & AI_CANONNAME) && canon == NULL) {
      if ((canon = strdup(hptr->h_name)) == NULL)
        error(EAI_MEMORY);
    }

    /* 4create one addrinfo{} for each returned address */
    for (ap = hptr->h_addr_list; *ap != NULL; ap++) {
      rc = ga_aistruct(&aipnext, &hints, *ap, hptr->h_addrtype);
      if (rc != 0)
        error(rc);
    }
  }
  if (aihead == NULL)
    error(EAI_NONAME);          /* nothing found */
  /* end ga4 */

/* include ga5 */
  /* 4return canonical name */
  if (hostname != NULL && hostname[0] != '\0' && hints.ai_flags & AI_CANONNAME) {
    if (canon != NULL)
      aihead->ai_canonname = canon;     /* strdup'ed earlier */
    else {
      if ((aihead->ai_canonname = strdup(search[0].host)) == NULL)
        error(EAI_MEMORY);
    }
  }

  /* 4now process the service name */
  if (servname != NULL && servname[0] != '\0') {
    if ((rc = ga_serv(aihead, &hints, servname)) != 0)
      error(rc);
  }

  *result = aihead;             /* pointer to first structure in linked list */
  return (0);

bad:
  freeaddrinfo(aihead);         /* free any alloc'ed memory */
  return (error);
}

/* end ga5 */

/*
 * Basic error checking of flags, family, socket type, and protocol.
 */

/* include ga_echeck */
static int
ga_echeck(const char *hostname, const char *servname,
          int flags, int family, int socktype, int protocol
          __attribute__ ((__unused__)))
{
  if (flags & ~(AI_PASSIVE | AI_CANONNAME))
    return (EAI_BADFLAGS);      /* unknown flag bits */

  if (hostname == NULL || hostname[0] == '\0') {
    if (servname == NULL || servname[0] == '\0')
      return (EAI_NONAME);      /* host or service must be specified */
  }

  switch (family) {
  case AF_UNSPEC:
    break;
#ifdef  IPv4
  case AF_INET:
    if (socktype != 0 &&
        (socktype != SOCK_STREAM &&
         socktype != SOCK_DGRAM && socktype != SOCK_RAW))
      return (EAI_SOCKTYPE);    /* invalid socket type */
    break;
#endif
#ifdef  HAVE_SOCKADDR_IN6
  case AF_INET6:
    if (socktype != 0 &&
        (socktype != SOCK_STREAM &&
         socktype != SOCK_DGRAM && socktype != SOCK_RAW))
      return (EAI_SOCKTYPE);    /* invalid socket type */
    break;
#endif
  default:
    return (EAI_FAMILY);        /* unknown protocol family */
  }
  return (0);
}

/* end ga_echeck */


/*
 * Set up the search[] array with the hostnames and address families
 * that we are to look up.
 */

/* include ga_nsearch1 */
static int
ga_nsearch(const char *hostname, const struct addrinfo *hintsp,
           struct search *search)
{
  int nsearch = 0;

  if (hostname == NULL || hostname[0] == '\0') {
    if (hintsp->ai_flags & AI_PASSIVE) {
      /* 4no hostname and AI_PASSIVE: implies wildcard bind */
      switch (hintsp->ai_family) {
#ifdef  IPv4
      case AF_INET:
        search[nsearch].host = "0.0.0.0";
        search[nsearch].family = AF_INET;
        nsearch++;
        break;
#endif
#ifdef  HAVE_SOCKADDR_IN6
      case AF_INET6:
        search[nsearch].host = "0::0";
        search[nsearch].family = AF_INET6;
        nsearch++;
        break;
#endif
      case AF_UNSPEC:
#ifdef  HAVE_SOCKADDR_IN6
        search[nsearch].host = "0::0";  /* IPv6 first, then IPv4 */
        search[nsearch].family = AF_INET6;
        nsearch++;
#endif
#ifdef  IPv4
        search[nsearch].host = "0.0.0.0";
        search[nsearch].family = AF_INET;
        nsearch++;
#endif
        break;
      }
      /* end ga_nsearch1 */
      /* include ga_nsearch2 */
    } else {
      /* 4no host and not AI_PASSIVE: connect to local host */
      switch (hintsp->ai_family) {
#ifdef  IPv4
      case AF_INET:
        search[nsearch].host = "localhost";     /* 127.0.0.1 */
        search[nsearch].family = AF_INET;
        nsearch++;
        break;
#endif
#ifdef  HAVE_SOCKADDR_IN6
      case AF_INET6:
        search[nsearch].host = "0::1";
        search[nsearch].family = AF_INET6;
        nsearch++;
        break;
#endif
      case AF_UNSPEC:
#ifdef  HAVE_SOCKADDR_IN6
        search[nsearch].host = "0::1";  /* IPv6 first, then IPv4 */
        search[nsearch].family = AF_INET6;
        nsearch++;
#endif
#ifdef  IPv4
        search[nsearch].host = "localhost";
        search[nsearch].family = AF_INET;
        nsearch++;
#endif
        break;
      }
    }
    /* end ga_nsearch2 */
    /* include ga_nsearch3 */
  } else {                      /* host is specified */
    switch (hintsp->ai_family) {
#ifdef  IPv4
    case AF_INET:
      search[nsearch].host = hostname;
      search[nsearch].family = AF_INET;
      nsearch++;
      break;
#endif
#ifdef  HAVE_SOCKADDR_IN6
    case AF_INET6:
      search[nsearch].host = hostname;
      search[nsearch].family = AF_INET6;
      nsearch++;
      break;
#endif
    case AF_UNSPEC:
#ifdef  HAVE_SOCKADDR_IN6
      search[nsearch].host = hostname;
      search[nsearch].family = AF_INET6;        /* IPv6 first */
      nsearch++;
#endif
#ifdef  IPv4
      search[nsearch].host = hostname;
      search[nsearch].family = AF_INET; /* then IPv4 */
      nsearch++;
#endif
      break;
    }
  }
  if (nsearch < 1 || nsearch > 2)
    return -1;
  return (nsearch);
}

/* end ga_nsearch3 */


/*
 * Create and fill in an addrinfo{}.
 */

#define AI_CLONE 4

/* include ga_aistruct1 */
int
ga_aistruct(struct addrinfo ***paipnext, const struct addrinfo *hintsp,
            const void *addr, int family)
{
  struct addrinfo *ai;

  if ((ai = calloc(1, sizeof(struct addrinfo))) == NULL)
    return (EAI_MEMORY);
  ai->ai_next = NULL;
  ai->ai_canonname = NULL;
  **paipnext = ai;
  *paipnext = &ai->ai_next;

  if ((ai->ai_socktype = hintsp->ai_socktype) == 0)
    ai->ai_flags |= AI_CLONE;

  ai->ai_protocol = hintsp->ai_protocol;
/* end ga_aistruct1 */

/* include ga_aistruct2 */
  switch ((ai->ai_family = family)) {
#ifdef  IPv4
  case AF_INET:{
      struct sockaddr_in *sinptr;

      /* 4allocate sockaddr_in{} and fill in all but port */
      if ((sinptr = calloc(1, sizeof(struct sockaddr_in))) == NULL)
        return (EAI_MEMORY);
#ifdef  HAVE_SOCKADDR_SA_LEN
      sinptr->sin_len = sizeof(struct sockaddr_in);
#endif
      sinptr->sin_family = AF_INET;
      memcpy(&sinptr->sin_addr, addr, sizeof(struct in_addr));
      ai->ai_addr = (struct sockaddr *) sinptr;
      ai->ai_addrlen = sizeof(struct sockaddr_in);
      break;
    }
#endif                          /* IPV4 */
#ifdef  HAVE_SOCKADDR_IN6
  case AF_INET6:{
      struct sockaddr_in6 *sin6ptr;

      /* 4allocate sockaddr_in6{} and fill in all but port */
      if ((sin6ptr = calloc(1, sizeof(struct sockaddr_in6))) == NULL)
        return (EAI_MEMORY);
#ifdef  HAVE_SOCKADDR_SA_LEN
      sin6ptr->sin6_len = sizeof(struct sockaddr_in6);
#endif
      sin6ptr->sin6_family = AF_INET6;
      memcpy(&sin6ptr->sin6_addr, addr, sizeof(struct in6_addr));
      ai->ai_addr = (struct sockaddr *) sin6ptr;
      ai->ai_addrlen = sizeof(struct sockaddr_in6);
      break;
    }
#endif                          /* IPV6 */

  }
  return (0);
}

/* end ga_aistruct2 */

/*
 * This function handles the service string.
 */

/* include ga_serv */
int
ga_serv(struct addrinfo *aihead, const struct addrinfo *hintsp,
        const char *serv)
{
  int port, rc, nfound;

  nfound = 0;
  if (isdigit((unsigned char) serv[0])) {       /* check for port number string first */
    port = (int) htons((unsigned short) atoi(serv));
    if (hintsp->ai_socktype) {
      /* 4caller specifies socket type */
      if ((rc = ga_port(aihead, port, hintsp->ai_socktype)) < 0)
        return (EAI_MEMORY);
      nfound += rc;
    } else {
      /* 4caller does not specify socket type */
      if ((rc = ga_port(aihead, port, SOCK_STREAM)) < 0)
        return (EAI_MEMORY);
      nfound += rc;
      if ((rc = ga_port(aihead, port, SOCK_DGRAM)) < 0)
        return (EAI_MEMORY);
      nfound += rc;
    }
  }

  if (nfound == 0) {
    if (hintsp->ai_socktype == 0)
      return (EAI_NONAME);      /* all calls to getservbyname() failed */
    else
      return (EAI_SERVICE);     /* service not supported for socket type */
  }
  return (0);
}

/* end ga_serv */


/*
 * Go through all the addrinfo structures, checking for a match of the
 * socket type and filling in the socket type, and then the port number
 * in the corresponding socket address structures.
 *
 * The AI_CLONE flag works as follows.  Consider a multihomed host with
 * two IP addresses and no socket type specified by the caller.  After
 * the "host" search there are two addrinfo structures, one per IP address.
 * Assuming a service supported by both TCP and UDP (say the daytime
 * service) we need to return *four* addrinfo structures:
 *              IP#1, SOCK_STREAM, TCP port,
 *              IP#1, SOCK_DGRAM, UDP port,
 *              IP#2, SOCK_STREAM, TCP port,
 *              IP#2, SOCK_DGRAM, UDP port.
 * To do this, when the "host" loop creates an addrinfo structure, if the
 * caller has not specified a socket type (hintsp->ai_socktype == 0), the
 * AI_CLONE flag is set.  When the following function finds an entry like
 * this it is handled as follows: If the entry's ai_socktype is still 0,
 * this is the first use of the structure, and the ai_socktype field is set.
 * But, if the entry's ai_socktype is nonzero, then we clone a new addrinfo
 * structure and set it's ai_socktype to the new value.  Although we only
 * need two socket types today (SOCK_STREAM and SOCK_DGRAM) this algorithm
 * will handle any number.  Also notice that Posix.1g requires all socket
 * types to be nonzero.
 */

/* include ga_port */
int
ga_port(struct addrinfo *aihead, int port, int socktype)
                /* port must be in network byte order */
{
  int nfound = 0;
  struct addrinfo *ai;

  for (ai = aihead; ai != NULL; ai = ai->ai_next) {
    if (ai->ai_flags & AI_CLONE) {
      if (ai->ai_socktype != 0) {
        if ((ai = ga_clone(ai)) == NULL)
          return (-1);          /* memory allocation error */
        /* ai points to newly cloned entry, which is what we want */
      }
    } else if (ai->ai_socktype != socktype)
      continue;                 /* ignore if mismatch on socket type */

    ai->ai_socktype = socktype;

    switch (ai->ai_family) {
#ifdef  IPv4
    case AF_INET:
      ((struct sockaddr_in *) ai->ai_addr)->sin_port = port;
      nfound++;
      break;
#endif
#ifdef  HAVE_SOCKADDR_IN6
    case AF_INET6:
      ((struct sockaddr_in6 *) ai->ai_addr)->sin6_port = port;
      nfound++;
      break;
#endif
    }
  }
  return (nfound);
}

/* end ga_port */

/*
 * Clone a new addrinfo structure from an existing one.
 */

/* include ga_clone */
struct addrinfo *
ga_clone(struct addrinfo *ai)
{
  struct addrinfo *new;

  if ((new = calloc(1, sizeof(struct addrinfo))) == NULL)
    return (NULL);

  new->ai_next = ai->ai_next;
  ai->ai_next = new;

  new->ai_flags = 0;            /* make sure AI_CLONE is off */
  new->ai_family = ai->ai_family;
  new->ai_socktype = ai->ai_socktype;
  new->ai_protocol = ai->ai_protocol;
  new->ai_canonname = NULL;
  new->ai_addrlen = ai->ai_addrlen;
  if ((new->ai_addr = malloc(ai->ai_addrlen)) == NULL)
    return (NULL);
  memcpy(new->ai_addr, ai->ai_addr, ai->ai_addrlen);

  return (new);
}

/* end ga_clone */
#endif                          /* HAS_GETADDRINFO */

/*
 * Return a string containing some additional information after an
 * error from getaddrinfo().
 */
#ifndef HAS_GAI_STRERROR

const char *
gai_strerror(int err)
{
  switch (err) {
  case EAI_ADDRFAMILY:
    return ("address family for host not supported");
  case EAI_AGAIN:
    return ("temporary failure in name resolution");
  case EAI_BADFLAGS:
    return ("invalid flags value");
  case EAI_FAIL:
    return ("non-recoverable failure in name resolution");
  case EAI_FAMILY:
    return ("address family not supported");
  case EAI_MEMORY:
    return ("memory allocation failure");
  case EAI_NODATA:
    return ("no address associated with host");
  case EAI_NONAME:
    return ("host nor service provided, or not known");
  case EAI_SERVICE:
    return ("service not supported for socket type");
  case EAI_SOCKTYPE:
    return ("socket type not supported");
  case EAI_SYSTEM:
    return ("system error");
  default:
    return ("unknown getaddrinfo() error");
  }
}
#endif                          /* HAS_GAI_STRERROR */

#ifndef HAS_GETADDRINFO

/* include freeaddrinfo */
void
freeaddrinfo(struct addrinfo *aihead)
{
  struct addrinfo *ai, *ainext;

  for (ai = aihead; ai != NULL; ai = ainext) {
    if (ai->ai_addr != NULL)
      free(ai->ai_addr);        /* socket address structure */

    if (ai->ai_canonname != NULL)
      free(ai->ai_canonname);

    ainext = ai->ai_next;       /* can't fetch ai_next after free() */
    free(ai);                   /* the addrinfo{} itself */
  }
}

/* end freeaddrinfo */

#endif                          /* HAS_GETADDRINFO */
