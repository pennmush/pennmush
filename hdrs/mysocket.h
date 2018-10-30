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

#include <stddef.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef WIN32
#ifndef FD_SETSIZE
#define FD_SETSIZE 256
#endif
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")

typedef int socklen_t;

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
  const char *hostname; /**< Host's name */
  const char *port;     /**< Host's source port */
};

/** A union for sockaddr manipulation */
union sockaddr_u {
  struct sockaddr addr;   /**< A sockaddr structure */
  char data[MAXSOCKADDR]; /**< A byte array representation */
};

/* What htons expects. Is this even used anymore? */
typedef unsigned short Port_t;

struct hostname_info *hostname_convert(struct sockaddr *host, int len);
struct hostname_info *ip_convert(struct sockaddr *host, int len);

/* Open a socket for listening */
int make_socket(Port_t port, int socktype, union sockaddr_u *addr,
                socklen_t *len, const char *host);
/* Connect somewhere using TCP */
int make_socket_conn(const char *host, int socktype,
                     struct sockaddr *myiterface, socklen_t myilen, Port_t port,
                     bool nonb);

int make_unix_socket(const char *filename, int socktype);
int connect_unix_socket(const char *filename, int socktype);

ssize_t send_with_creds(int, void *, size_t);
ssize_t recv_with_creds(int, void *, size_t, int *, int *);

void make_nonblocking(int s);
void make_blocking(int s);
void set_keepalive(int s, int timeout);
void set_close_exec(int s);
bool is_blocking_err(int);

/* Win32 uses closesocket() to close a socket, and so will we */
#ifndef WIN32
#define closesocket(s) close(s)
#else
extern BOOL GetErrorMessage(const DWORD dwError, LPTSTR lpszError,
                            const UINT nMaxError);
#endif

/* Telnet codes. Consider using <arpa/telnet.h> instead? */
#define IAC 255        /**< interpret as command: */
#define NOP 241        /**< no operation */
#define AYT 246        /**< are you there? */
#define DONT 254       /**< you are not to use option */
#define DO 253         /**< please, you use option */
#define WONT 252       /**< I won't use option */
#define WILL 251       /**< I will use option */
#define SB 250         /**< interpret as subnegotiation */
#define SE 240         /**< end sub negotiation */
#define TN_SGA 3       /**< Suppress go-ahead */
#define TN_LINEMODE 34 /**< Line mode */
#define TN_NAWS 31     /**< Negotiate About Window Size */
#define TN_TTYPE 24    /**< Ask for termial type information */
#define TN_MSSP                                                                \
  70 /**< Send MSSP info (http://tintin.sourceforge.net/mssp/)                 \
      */
#define TN_CHARSET 42            /**< Negotiate Character Set (RFC 2066) */
#define MSSP_VAR 1               /**< MSSP option name */
#define MSSP_VAL 2               /**< MSSP option value */
#define TN_SB_CHARSET_REQUEST 1  /**< Charset subnegotiation REQUEST */
#define TN_SB_CHARSET_ACCEPTED 2 /**< Charset subnegotiation ACCEPTED */
#define TN_SB_CHARSET_REJECTED 3 /**< Charset subnegotiation REJECTED */
#define TN_GMCP                                                                \
  201 /**< Generic MUD Communication Protocol; see                             \
         http://www.gammon.com.au/gmcp */

#endif /* MYSOCKET_H */
