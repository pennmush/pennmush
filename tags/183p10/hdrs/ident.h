/*
   ** ident.h
   **
   ** Author: Peter Eriksson <pen@lysator.liu.se>
   ** Intruder: Pär Emanuelsson <pell@lysator.liu.se>
   ** Vandal: Shawn Wagner <raevnos@pennmush.org>
 */

#ifndef __IDENT_H__
#define __IDENT_H__

#include "config.h"

#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif

#ifdef I_SYS_SELECT
#include <sys/select.h>
#endif

#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif

#include "mysocket.h"

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef IDBUFSIZE
#define IDBUFSIZE 2048
#endif

  /* Using the "auth" service name would be better, but windoze probably
     doesn't support any notion of getservyname or like functions.
   */
#ifndef IDPORT
#define IDPORT  113
#endif

  /** Ident call result.
   * This structure stores the result of an ident call.
   */
  typedef struct {
    char *identifier;           /**< Normally user name */
    char *opsys;                /**< Operating system */
    char *charset;              /**< Character set */
  } IDENT;

/* High-level calls */

  extern char *ident_id(int fd, int *timeout);

  extern IDENT *ident_query(struct sockaddr *laddr, socklen_t llen,
                            struct sockaddr *raddr, socklen_t rlen,
                            int *timeout);

  void ident_free(IDENT * id);

#ifdef  __cplusplus
}
#endif
#endif
