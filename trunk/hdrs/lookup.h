/**
 * \file lookup.h
 * \brief Prototypes and data structures for talking with info_slave
 *
 * Must be #included after mysocket.h and ident.h to get appropriate
 * types and constants.
 *
 * netmush and info_slave use UDP datagrams to talk to each
 * other. Each datagram is one recv/send, with a max size of
 * something like 8K (Less than that on OS X in practice). We're well under
 * that. Using datagrams instead of streams vastly simplies the communication
 * code. We should have done it years ago.
 */

#ifndef LOOKUP_H
#define LOOKUP_H

#include "copyrite.h"

/** Datagram sent to info_slave from the mush */
struct request_dgram {
  int fd; /**< The socket descriptor, used as an id number */
  union sockaddr_u local; /**< The sockaddr struct for the local address */
  union sockaddr_u remote; /**< The sockaddr struct for the remote address */
  socklen_t llen; /**< Length of local address */
  socklen_t rlen; /**< Length of remote address */
  int use_dns; /**< True to do hostname lookup */
  int timeout; /**< Timeout in seconds for lookups */
};

#define IPADDR_LEN 128
#define HOSTNAME_LEN 256
#define IDENT_LEN 128

/** Datagram sent by info_slave back to the mush */
struct response_dgram {
  int fd; /**< The socket descriptor, used as an id number */
  char ipaddr[IPADDR_LEN]; /**< The ip address of the connection */
  char hostname[HOSTNAME_LEN]; /**< The resolved hostname of the connection */
  Port_t connected_to; /**< The port connected to. */
};

extern pid_t info_slave_pid;
extern int info_slave;
extern time_t info_queue_time;
extern bool info_slave_halted;

enum is_state { INFO_SLAVE_DOWN, INFO_SLAVE_READY, INFO_SLAVE_PENDING };

extern enum is_state info_slave_state;

void init_info_slave(void);
void query_info_slave(int fd);
void update_pending_info_slaves(void);
void reap_info_slave(void);
void kill_info_slave(void);

#endif                          /* LOOKUP_H */
