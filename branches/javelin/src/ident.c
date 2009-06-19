/**
 * \file ident.c
 *
 * \brief High-level calls to the ident library.
 *
 * Author: Pär Emanuelsson <pell@lysator.liu.se>
 * Hacked by: Peter Eriksson <pen@lysator.liu.se>
 * 
 * Many changes by Shawn Wagner to be protocol independent
 * for PennMUSH
 */

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#include <errno.h>
#ifndef WIN32
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#else
#ifdef I_SYS_IN
#include <sys/in.h>
#endif
#endif
#ifdef I_ARPA_INET
#include <arpa/inet.h>
#endif
#include <netdb.h>
#endif                          /* WIN32 */
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef I_UNISTD
#include <unistd.h>
#endif

#include "conf.h"
#include "externs.h"
#include "attrib.h"
#include "ident.h"
#include "mymalloc.h"
#include "mysocket.h"
#include "confmagic.h"

  /* Low-level calls and macros */

/** Structure to track an ident connection. */
typedef struct {
  int fd;               /**< file descriptor to read from. */
  char buf[IDBUFSIZE];  /**< buffer to hold ident data. */
} ident_t;


static ident_t *id_open(struct sockaddr *faddr,
                        socklen_t flen,
                        struct sockaddr *laddr, socklen_t llen, int *timeout);

static int id_query(ident_t *id,
                    struct sockaddr *laddr,
                    socklen_t llen,
                    struct sockaddr *faddr, socklen_t flen, int *timeout);

static void id_close(ident_t *id);

static int id_parse(ident_t *id, int *timeout, IDENT ** ident);
static IDENT *ident_lookup(int fd, int *timeout);


/* Do a complete ident query and return result */
static IDENT *
ident_lookup(int fd, int *timeout)
{
  union sockaddr_u localaddr, remoteaddr;
  socklen_t llen, rlen, len;

  len = sizeof remoteaddr;
  if (getpeername(fd, (struct sockaddr *) remoteaddr.data, &len) < 0)
    return NULL;
  llen = len;

  len = sizeof localaddr;
  if (getsockname(fd, (struct sockaddr *) localaddr.data, &len) < 0)
    return NULL;
  rlen = len;

  return ident_query(&localaddr.addr, llen, &remoteaddr.addr, rlen, timeout);
}

/** Perform an ident query and return the result.
 * \param laddr local socket address data.
 * \param llen local socket address data length.
 * \param raddr remote socket address data.
 * \param rlen remote socket address data length.
 * \param timeout pointer to timeout value for query.
 * \return ident responses in IDENT pointer, or NULL.
 */
IDENT *
ident_query(struct sockaddr * laddr, socklen_t llen,
            struct sockaddr * raddr, socklen_t rlen, int *timeout)
{
  int res;
  ident_t *id;
  IDENT *ident = NULL;

  if (timeout && *timeout < 0)
    *timeout = 0;

  id = id_open(raddr, rlen, laddr, llen, timeout);

  if (!id) {
#ifndef WIN32
    errno = EINVAL;
#endif
    return NULL;
  }

  res = id_query(id, raddr, rlen, laddr, llen, timeout);

  if (res < 0) {
    id_close(id);
    return NULL;
  }

  res = id_parse(id, timeout, &ident);

  if (res < 0) {
    id_close(id);
    return NULL;
  }

  id_close(id);
  return ident;                 /* At last! */
}

/** Perform an ident lookup and return the remote identifier as a
 * newly allocated string. This function allocates memory that
 * should be freed by the caller.
 * \param fd socket to use for ident lookup.
 * \param timeout pointer to timeout value for lookup.
 * \return allocated string containing identifier, or NULL.
 */
char *
ident_id(int fd, int *timeout)
{
  IDENT *ident;
  char *id = NULL;
  if (timeout && *timeout < 0)
    *timeout = 0;
  ident = ident_lookup(fd, timeout);
  if (ident && ident->identifier && *ident->identifier)
    id = strdup(ident->identifier);
  ident_free(ident);
  return id;
}

/** Free an IDENT structure and all elements.
 * \param id pointer to IDENT structure to free.
 */
void
ident_free(IDENT * id)
{
  if (!id)
    return;
  if (id->identifier)
    free(id->identifier);
  if (id->opsys)
    free(id->opsys);
  if (id->charset)
    free(id->charset);
  free(id);
}

/* id_open.c Establish/initiate a connection to an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se>
   ** Fixes: Pär Emanuelsson <pell@lysator.liu.se> */

static ident_t *
id_open(struct sockaddr *faddr, socklen_t flen,
        struct sockaddr *laddr, socklen_t llen, int *timeout)
{
  ident_t *id;
  char host[NI_MAXHOST];
  union sockaddr_u myinterface;
#ifndef WIN32
  int tmperrno;
#endif

  if ((id = malloc(sizeof *id)) == NULL)
    return NULL;

  memset(id, 0, sizeof *id);

  if (getnameinfo(faddr, flen, host, sizeof host, NULL, 0,
                  NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
    penn_perror("id_open: getnameinfo");
    free(id);
    return NULL;
  }

  /* Make sure we connect from the right interface. Changing the pointer
     directly doesn't seem to work. So... */
  memcpy(&myinterface.data, laddr, llen);
  if (myinterface.addr.sa_family == AF_INET)
    ((struct sockaddr_in *) &myinterface.addr)->sin_port = 0;
#ifdef HAVE_SOCKADDR_IN6        /* Bleah, I wanted to avoid stuff like this */
  else if (myinterface.addr.sa_family == AF_INET6)
    ((struct sockaddr_in6 *) &myinterface.addr)->sin6_port = 0;
#endif

  id->fd = make_socket_conn(host, SOCK_STREAM, &myinterface.addr, llen,
                            IDPORT, timeout ? *timeout : 0);

  if (id->fd < 0)               /* Couldn't connect to an ident server */
    goto ERROR_BRANCH;

  return id;

ERROR_BRANCH:
#ifndef WIN32
  tmperrno = errno;             /* Save, so close() won't erase it */
#endif
  id_close(id);
#ifndef WIN32
  errno = tmperrno;
#endif
  return 0;
}


/* id_close.c Close a connection to an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se> */

static void
id_close(ident_t *id)
{
  closesocket(id->fd);
  free(id);
}


/* id_query.c Transmit a query to an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se>
   ** Slight modifications by Alan Schwartz */


static int
id_query(ident_t *id, struct sockaddr *laddr, socklen_t llen,
         struct sockaddr *faddr, socklen_t flen, int *timeout)
{
  int res;
  char buf[80];
  char fport[NI_MAXSERV], lport[NI_MAXSERV];
  time_t now, after;

  if (getnameinfo(laddr, llen, NULL, 0, lport, sizeof lport,
                  NI_NUMERICHOST | NI_NUMERICSERV) < 0) {
    penn_perror("id_query: getnameinfo");
    return -1;
  }
  if (getnameinfo(faddr, flen, NULL, 0, fport, sizeof fport,
                  NI_NUMERICHOST | NI_NUMERICSERV) < 0) {
    penn_perror("id_query: getnameinfo2");
    return -1;
  }

  snprintf(buf, sizeof buf, "%s , %s\r\n", lport, fport);

  time(&now);
  if ((res = wait_for_connect(id->fd, timeout ? *timeout : -1)) <= 0) {
    if (res < 0)
      penn_perror("id_query: wait_for_connect");
    return -1;
  }
  time(&after);

  if (timeout) {
    *timeout -= (int) difftime(after, now);
    *timeout = *timeout < 0 ? 0 : *timeout;
  }

  make_blocking(id->fd);


  while (1) {
    if (timeout) {
      struct timeval to;
      socklen_t to_len;

      to.tv_sec = *timeout;
      to.tv_usec = 0;
      to_len = sizeof to;
      if (setsockopt(id->fd, SOL_SOCKET, SO_SNDTIMEO, &to, to_len) < 0) {
        penn_perror("id_query: setsockopt");
        return -1;
      }
    }

    now = time(NULL);
    res = send(id->fd, buf, strlen(buf), 0);
    after = time(NULL);
    if (timeout) {
      *timeout -= (int) difftime(after, now);
      *timeout = *timeout < 0 ? 0 : *timeout;
      if (*timeout == 0)
        return -1;
    }
    if (res < 0) {
      if (errno == EINTR)
        continue;
      else if (errno != EAGAIN && errno != EWOULDBLOCK)
        penn_perror("id_query: send");
      return -1;
    } else
      return res;
  }
}

/* id_parse.c Receive and parse a reply from an IDENT server
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se>
   ** Fiddling: Pär Emanuelsson <pell@lysator.liu.se> */

static char *
xstrtok(char *RESTRICT cp, const char *RESTRICT cs, char *RESTRICT dc)
{
  static char *bp = 0;

  if (cp)
    bp = cp;

  /*
   ** No delimitor cs - return whole buffer and point at end
   */
  if (!cs) {
    while (*bp)
      bp++;
    return NULL;
  }
  /*
   ** Skip leading spaces
   */
  while (isspace((unsigned char) *bp))
    bp++;

  /*
   ** No token found?
   */
  if (!*bp)
    return NULL;

  cp = bp;
  bp += strcspn(bp, cs);
  /*  while (*bp && !strchr(cs, *bp))
     bp++;
   */
  /* Remove trailing spaces */
  *dc = *bp;
  for (dc = bp - 1; dc > cp && isspace((unsigned char) *dc); dc--) ;
  *++dc = '\0';

  bp++;

  return cp;
}


static int
id_parse(ident_t *id, int *timeout, IDENT ** ident)
{
  char c, *cp, *tmp_charset = NULL;
  int res = 0, lp, fp;
  size_t pos, len;

#ifndef WIN32
  errno = 0;
#endif

  tmp_charset = 0;

  if (!id || !ident)
    return -1;

  *ident = malloc(sizeof(IDENT));

  if (!*ident)
    return -1;

  memset(*ident, 0, sizeof **ident);

  pos = strlen(id->buf);
  len = IDBUFSIZE - pos;

  do {
    time_t now, after;
    if (timeout) {
      struct timeval to;
      socklen_t to_len;

      to.tv_sec = *timeout;
      to.tv_usec = 0;
      to_len = sizeof to;

      if (setsockopt(id->fd, SOL_SOCKET, SO_RCVTIMEO, &to, to_len) < 0) {
        penn_perror("id_parse: setsockopt");
        return -1;
      }
    }

    now = time(NULL);
    res = recv(id->fd, id->buf + pos, len, 0);
    after = time(NULL);
    if (timeout) {
      *timeout -= (int) difftime(after, now);
      *timeout = *timeout < 0 ? 0 : *timeout;
      if (*timeout == 0)
        return -1;
    }

    if (res < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      else if (errno == EINTR)
        continue;
      else
        return -1;
    }

    len -= res;
    pos += res;
  } while (pos < len && res > 0 && id->buf[pos - 1] != '\n');

  if (id->buf[pos - 1] != '\n')
    return 0;

  id->buf[pos++] = '\0';

  /* Get first field (<lport> , <fport>) */
  cp = xstrtok(id->buf, ":", &c);
  if (!cp) {
    return -2;
  }

  if ((res = sscanf(cp, " %d , %d", &lp, &fp)) != 2) {
    (*ident)->identifier = strdup(cp);
    return -2;
  }
  /* Get second field (USERID or ERROR) */
  cp = xstrtok(NULL, ":", &c);
  if (!cp) {
    return -2;
  }
  if (strcmp(cp, "ERROR") == 0) {
    cp = xstrtok(NULL, "\n\r", &c);
    if (!cp)
      return -2;

    (*ident)->identifier = strdup(cp);

    return 2;
  } else if (strcmp(cp, "USERID") == 0) {
    /* Get first subfield of third field <opsys> */
    cp = xstrtok(NULL, ",:", &c);
    if (!cp) {
      return -2;
    }
    (*ident)->opsys = strdup(cp);

    /* We have a second subfield (<charset>) */
    if (c == ',') {
      cp = xstrtok(NULL, ":", &c);
      if (!cp)
        return -2;

      tmp_charset = cp;

      (*ident)->charset = strdup(cp);

      /* We have even more subfields - ignore them */
      if (c == ',')
        xstrtok(NULL, ":", &c);
    }
    if (tmp_charset && strcmp(tmp_charset, "OCTET") == 0)
      cp = xstrtok(NULL, NULL, &c);
    else
      cp = xstrtok(NULL, "\n\r", &c);

    (*ident)->identifier = strdup(cp);
    return 1;
  } else {
    (*ident)->identifier = strdup(cp);
    return -3;
  }
}
