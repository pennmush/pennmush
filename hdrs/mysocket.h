/**
 * \file mysocket.h
 *
 * \brief Stuff relating to sockets.
 *
 * \verbatim
 * Define required structures and constants if they're needed.
 * Most of these are in Posix 1001.g, but getnameinfo isn't (though
 * it's in at least one RFC). Anyways, all this gives us IP version
 * independance.
 * \endverbatim
 */



#ifndef __MYSOCKET_H
#define __MYSOCKET_H

#include "copyrite.h"
#include "config.h"
#include "confmagic.h"

#ifdef WIN32
#ifndef FD_SETSIZE
#define FD_SETSIZE 256
#endif
#include <winsock.h>
#include <io.h>
#undef EINTR                    /* Clashes with errno.h */
#define EINTR WSAEINTR
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EINPROGRESS WSAEINPROGRESS
#define ETIMEDOUT WSAETIMEDOUT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#define ENOSPC          28
#define MAXHOSTNAMELEN 32
#pragma comment( lib, "wsock32.lib")
#pragma comment( lib, "winmm.lib")
#pragma comment( lib, "advapi32.lib")
#endif

/* This number taken from Stevens. It's the size of the largest possible
 * sockaddr_* struct. Since that includes unix-domain sockets, this
 * gives us lots of buffer space. */
#ifndef MAXSOCKADDR
#define MAXSOCKADDR 128
#endif

/** Information about a host.
 */
struct hostname_info {
  const char *hostname;         /**< Host's name */
  const char *port;             /**< Host's source port */
};

/** A union for sockaddr manipulation */
union sockaddr_u {
  struct sockaddr addr;         /**< A sockaddr structure */
  char data[MAXSOCKADDR];       /**< A byte array representation */
};

/* What htons expects. Is this even used anymore? */
typedef unsigned short Port_t;

struct hostname_info *hostname_convert(struct sockaddr *host, int len);
struct hostname_info *ip_convert(struct sockaddr *host, int len);


/* Open a socket for listening */
int make_socket
  (Port_t port, int socktype, union sockaddr_u *addr, socklen_t *len,
   const char *host);
/* Connect somewhere using TCP */
int make_socket_conn(const char *host, int socktype,
                     struct sockaddr *myiterface, socklen_t myilen, Port_t port,
                     bool nonb);

int make_unix_socket(const char *filename, int socktype);
int connect_unix_socket(const char *filename, int socktype);

int wait_for_connect(int, int);
void make_nonblocking(int s);
void make_blocking(int s);
void set_keepalive(int s, int timeout);
/* Win32 uses closesocket() to close a socket, and so will we */
#ifndef WIN32
#define closesocket(s)  close(s)
#else
extern BOOL GetErrorMessage(const DWORD dwError, LPTSTR lpszError, const UINT
                            nMaxError);
#endif


#ifndef HAS_GETHOSTBYNAME2
#define gethostbyname2(host, type) gethostbyname((host))
#endif

#ifndef HAS_INET_PTON
int inet_pton(int, const char *, void *);
const char *inet_ntop(int, const void *, char *, size_t);
#endif

#ifndef HAS_GETADDRINFO
/** addrinfo structure for systems without it.
 * Everything here really belongs in <netdb.h>.
 * These defines are separate for now, to avoid having to modify the
 * system's header.
 */

struct addrinfo {
  int ai_flags;                 /**< AI_PASSIVE, AI_CANONNAME */
  int ai_family;                /**< PF_xxx */
  int ai_socktype;              /**< SOCK_xxx */
  int ai_protocol;              /**< IPPROTO_xxx for IPv4 and IPv6 */
  size_t ai_addrlen;            /**< length of ai_addr */
  char *ai_canonname;           /**< canonical name for host */
  struct sockaddr *ai_addr;     /**< binary address */
  struct addrinfo *ai_next;     /**< next structure in linked list */
};

struct addrinfo;
                        /* following for getaddrinfo() */
#define AI_PASSIVE               1      /* socket is intended for bind() + listen() */
#define AI_CANONNAME     2      /* return canonical name */

                        /* error returns */
#define EAI_ADDRFAMILY   1      /* address family for host not supported */
#define EAI_AGAIN                2      /* temporary failure in name resolution */
#define EAI_BADFLAGS     3      /* invalid value for ai_flags */
#define EAI_FAIL                 4      /* non-recoverable failure in name resolution */
#define EAI_FAMILY               5      /* ai_family not supported */
#define EAI_MEMORY               6      /* memory allocation failure */
#define EAI_NODATA               7      /* no address associated with host */
#define EAI_NONAME               8      /* host nor service provided, or not known */
#define EAI_SERVICE              9      /* service not supported for ai_socktype */
#define EAI_SOCKTYPE    10      /* ai_socktype not supported */
#define EAI_SYSTEM              11      /* system error returned in errno */


int getaddrinfo(const char *hostname, const char *servname,
                const struct addrinfo *hintsp, struct addrinfo **result);
/* If we don't have getaddrinfo, we won't have these either... */

void freeaddrinfo(struct addrinfo *old);

#endif                          /* HAS_GETADDRINFO */

#ifndef HAS_GAI_STRERROR
const char *gai_strerror(int errval);
#endif

#ifndef HAS_GETNAMEINFO         /* following for getnameinfo() */
#ifndef __APPLE__
/* Apple has these in netdb.h */
#define NI_MAXHOST        1025  /* max hostname returned */
#define NI_MAXSERV          32  /* max service name returned */

#define NI_NOFQDN            1  /* do not return FQDN */
#define NI_NUMERICHOST   2      /* return numeric form of hostname */
#define NI_NAMEREQD          4  /* return error if hostname not found */
#define NI_NUMERICSERV   8      /* return numeric form of service name */
#define NI_DGRAM            16  /* datagram service for getservbyname() */
#endif

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
                size_t hostlen, char *serv, size_t servlen, int flags);
#endif                          /* HAS_GETNAMEINFO */

#endif                          /* MYSOCKET_H */
