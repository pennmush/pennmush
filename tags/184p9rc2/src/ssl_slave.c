/** SSL slave related code. */

#include "copyrite.h"
#include "config.h"


#ifdef SSL_SLAVE

#ifndef HAS_OPENSSL
#error "ssl_slave requires OpenSSL!"
#endif

#ifndef HAVE_LIBEVENT
#error "ssl_slave requires libevent!"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <event.h>
#include <event2/dns.h>
#include <event2/bufferevent_ssl.h>

#include "conf.h"
#include "externs.h"
#include "myssl.h"
#include "mysocket.h"
#include "SFMT.h"
#include "ssl_slave.h"

#include "confmagic.h"

void errprintf(FILE *, const char *, ...)
  __attribute__ ((__format__(__printf__, 2, 3)));
void errputs(FILE *, const char *);

/* 0 for no debugging messages, 1 for connection related, 2 for every read/write */
#define SSL_DEBUG_LEVEL 1

pid_t parent_pid = -1;
int ssl_sock = -1;
int keepalive_timeout = 300;
const char *socket_file = NULL;
struct event_base *main_loop = NULL;
struct evdns_base *resolver = NULL;
void event_cb(struct bufferevent *bev, short e, void *data);

enum conn_state {
  C_SSL_CONNECTING,
  C_HOSTNAME_LOOKUP,
  C_LOCAL_CONNECTING,
  C_ESTABLISHED,
  C_SHUTTINGDOWN
};

struct conn {
  enum conn_state state;
  socklen_t remote_addrlen;
  union sockaddr_u remote_addr;
  char *remote_host;
  char *remote_ip;
  struct bufferevent *local_bev;
  struct bufferevent *remote_bev;
  struct evdns_request *resolver_req;
  struct conn *next;
  struct conn *prev;
};

struct conn *connections = NULL;

/* General utility routines */

/** Allocate a new connection object */
struct conn *
alloc_conn(void)
{
  struct conn *c;
  c = malloc(sizeof *c);
  memset(c, 0, sizeof *c);
  return c;
}

/** Free a connection object.
 * \param c the object to free
 */
void
free_conn(struct conn *c)
{
  if (c->local_bev)
    bufferevent_free(c->local_bev);
  if (c->remote_bev)
    bufferevent_free(c->remote_bev);
  if (c->remote_host)
    free(c->remote_host);
  if (c->remote_ip)
    free(c->remote_ip);
  if (c->resolver_req)
    evdns_cancel_request(resolver, c->resolver_req);
  free(c);
}

/** Remove a connection object from the list of maintained
 * connections.
 * \param c the object to clean up.
 */
void
delete_conn(struct conn *c)
{
  struct conn *curr, *nxt;

  for (curr = connections; curr; curr = nxt) {
    nxt = curr->next;
    if (curr == c) {
      if (curr->prev) {
        curr->prev->next = nxt;
        if (nxt)
          nxt->prev = curr->prev;
      } else {
        connections = nxt;
        if (connections)
          connections->prev = NULL;
      }
      free_conn(c);
      break;
    }
  }
}

/** Address to hostname lookup wrapper */
struct evdns_request *
evdns_getnameinfo(struct evdns_base *base, const struct sockaddr *addr,
                  int flags, evdns_callback_type callback, void *data)
{
  if (addr->sa_family == AF_INET) {
    const struct sockaddr_in *a = (const struct sockaddr_in *) addr;
#if SSL_DEBUG_LEVEL > 1
    errputs(stdout, "Remote connection is IPv4.");
#endif
    return evdns_base_resolve_reverse(base, &a->sin_addr, flags, callback,
                                      data);
  } else if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6 *a = (const struct sockaddr_in6 *) addr;
#if SSL_DEBUG_LEVEL > 1
    errputs(stdout, "Remote connection is IPv6.");
#endif
    return evdns_base_resolve_reverse_ipv6(base, &a->sin6_addr, flags, callback,
                                           data);
  } else {
    errprintf(stdout,
              "ssl_slave: Attempt to resolve unknown socket family %d\n",
              addr->sa_family);
    return NULL;
  }
}

/* libevent callback functions */

/** Read from one buffer and write the results to the other */
void
pipe_cb(struct bufferevent *from_bev, void *data)
{
  struct conn *c = data;
  char buff[BUFFER_LEN];
  size_t len;
  struct bufferevent *to_bev = NULL;

  if (c->local_bev == from_bev) {
#if SSL_DEBUG_LEVEL > 1
    errputs(stdout, "got data from mush.");
#endif
    to_bev = c->remote_bev;
  } else {
#if SSL_DEBUG_LEVEL > 1
    errputs(stdout, "got data from SSL");
#endif
    to_bev = c->local_bev;
  }

  len = bufferevent_read(from_bev, buff, sizeof buff);

#if SSL_DEBUG_LEVEL > 1
  errprintf(stdout, "ssl_slave: read %zu bytes.\n", len);
#endif

  if (to_bev && len > 0) {
    if (bufferevent_write(to_bev, buff, len) < 0)
      errputs(stderr, "write failed!");
  }
}

/** Called after the local connection to the mush has established */
void
local_connected(struct conn *c)
{
  char *hostid;
  int len;

#if SSL_DEBUG_LEVEL > 0
  errputs(stdout,
          "Local connection attempt completed. Setting up pipe.");
#endif
  bufferevent_setcb(c->local_bev, pipe_cb, NULL, event_cb, c);
  bufferevent_enable(c->local_bev, EV_READ | EV_WRITE);
  bufferevent_setcb(c->remote_bev, pipe_cb, NULL, event_cb, c);
  bufferevent_enable(c->remote_bev, EV_READ | EV_WRITE);
  /* If these aren't set, just buffers till lots of data is ready */
  bufferevent_setwatermark(c->remote_bev, EV_READ, 0, 1);
  bufferevent_setwatermark(c->remote_bev, EV_WRITE, 0, 1);

  c->state = C_ESTABLISHED;

  /* Now pass the remote host and IP to the mush as the very first line it gets */
  len = strlen(c->remote_host) + strlen(c->remote_ip) + 3;
  hostid = malloc(len + 1);
  sprintf(hostid, "%s^%s\r\n", c->remote_ip, c->remote_host);
  bufferevent_write(c->local_bev, hostid, len);
  free(hostid);
}

/** Called after the remote hostname has been resolved. */
void
address_resolved(int result, char type, int count, int ttl
                 __attribute__ ((__unused__)), void *addresses, void *data)
{
  struct conn *c = data;
  struct sockaddr_un addr;
  struct hostname_info *ipaddr;

  c->resolver_req = NULL;

  if (result != DNS_ERR_NONE || !addresses || type != DNS_PTR || count == 0) {
    ipaddr = ip_convert(&c->remote_addr.addr, c->remote_addrlen);
    c->remote_host = strdup(ipaddr->hostname);
    c->remote_ip = strdup(ipaddr->hostname);
  } else {
    const char *hostname = ((const char **) addresses)[0];
    c->remote_host = strdup(hostname);
    ipaddr = ip_convert(&c->remote_addr.addr, c->remote_addrlen);
    c->remote_ip = strdup(ipaddr->hostname);
  }

#if SSL_DEBUG_LEVEL > 0
  errprintf(stdout,
            "ssl_slave: resolved hostname as '%s(%s)'. Opening local connection to mush.\n",
            c->remote_host, c->remote_ip);
#endif

  c->state = C_LOCAL_CONNECTING;

  addr.sun_family = AF_LOCAL;
  strncpy(addr.sun_path, socket_file, sizeof(addr.sun_path) - 1);
  c->local_bev =
    bufferevent_socket_new(main_loop, -1,
                           BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  bufferevent_socket_connect(c->local_bev, (struct sockaddr *) &addr,
                             sizeof addr);
  bufferevent_setcb(c->local_bev, NULL, NULL, event_cb, data);
  bufferevent_enable(c->local_bev, EV_WRITE);
}

/** Called after the SSL connection and initial handshaking is complete. */
void
ssl_connected(struct conn *c)
{
  X509 *peer;
  SSL *ssl;

#if SSL_DEBUG_LEVEL > 0
  errputs(stdout,
          "SSL connection attempt completed. Resolving remote host name.");
  errprintf(stdout, "ssl_slave: ssl error code: %ld\n",
	    bufferevent_get_openssl_error(c->remote_bev));	  
#endif

  bufferevent_set_timeouts(c->remote_bev, NULL, NULL);

  ssl = bufferevent_openssl_get_ssl(c->remote_bev);

  /* Successful accept. Log peer certificate, if any. */
  if ((peer = SSL_get_peer_certificate(ssl))) {
    if (SSL_get_verify_result(ssl) == X509_V_OK) {
      char buf[256];
      /* The client sent a certificate which verified OK */
      X509_NAME_oneline(X509_get_subject_name(peer), buf, 256);
      errprintf(stdout, "SSL client certificate accepted: %s\n", buf);
    }
  }

  c->state = C_HOSTNAME_LOOKUP;
  c->resolver_req =
    evdns_getnameinfo(resolver, &c->remote_addr.addr, 0, address_resolved, c);
}

/** Called on successful connections and errors */
void
event_cb(struct bufferevent *bev, short e, void *data)
{
  struct conn *c = data;
  uint32_t error_conditions =
    BEV_EVENT_EOF | BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT;

#if SSL_DEBUG_LEVEL > 1
  errprintf(stdout, "ssl_slave: event callback triggered with flags 0x%hx\n",
            e);
#endif

  if (e & BEV_EVENT_CONNECTED) {
    if (c->local_bev == bev) {
      local_connected(c);
    } else {
      ssl_connected(c);
    }
  } else if (e & error_conditions) {
    if (c->local_bev == bev) {
      /* Mush side of the connection went away. Flush SSL buffer and shut down. */
#if SSL_DEBUG_LEVEL > 0
      errprintf(stdout,
                "ssl_slave: Lost local connection. State: %d, reason 0x%hx.\n",
                c->state, e);
#endif
      bufferevent_disable(c->local_bev, EV_READ | EV_WRITE);
      bufferevent_free(c->local_bev);
      c->local_bev = NULL;
      c->state = C_SHUTTINGDOWN;
      if (c->remote_bev) {
        bufferevent_disable(c->remote_bev, EV_READ);
        bufferevent_flush(c->remote_bev, EV_WRITE, BEV_FINISHED);
	SSL_shutdown(bufferevent_openssl_get_ssl(c->remote_bev));
      }
      delete_conn(c);
    } else {
      /* Remote side of the connection went away. Flush mush buffer and shut down. */
#if SSL_DEBUG_LEVEL > 0
      errprintf(stdout,
                "ssl_slave: Lost SSL connection. State: %d, reason 0x%hx.\n",
                c->state, e);
#endif
      bufferevent_disable(c->remote_bev, EV_READ | EV_WRITE);
      bufferevent_free(c->remote_bev);
      c->remote_bev = NULL;
      c->state = C_SHUTTINGDOWN;
      if (c->local_bev) {
        bufferevent_disable(c->local_bev, EV_READ);
        bufferevent_flush(c->local_bev, EV_WRITE, BEV_FINISHED);
      }
      delete_conn(c);
    }
  }
}

/* Called when a new connection is made on the ssl port */
static void
new_conn_cb(evutil_socket_t s, short flags
            __attribute__ ((__unused__)), void *data
            __attribute__ ((__unused__)))
{
  struct conn *c;
  int fd;
  struct timeval handshake_timeout = {.tv_sec = 60,.tv_usec = 0 };
  SSL *ssl;

  /* Accept a connection and do SSL handshaking */

#if SSL_DEBUG_LEVEL > 0
  errputs(stdout, "Got new connection on SSL port.");
#endif

  c = alloc_conn();

  if (connections)
    connections->prev = c;
  c->next = connections;
  connections = c;

  c->state = C_SSL_CONNECTING;

  c->remote_addrlen = sizeof c->remote_addr;
  fd = accept(s, &c->remote_addr.addr, &c->remote_addrlen);
  if (fd < 0) {
    errprintf(stderr, "ssl_slave: accept: %s\n", strerror(errno));
    delete_conn(c);
    return;
  }
  set_keepalive(fd, keepalive_timeout);
  make_nonblocking(fd);
  ssl = ssl_alloc_struct();
  c->remote_bev =
    bufferevent_openssl_socket_new(main_loop, fd, ssl,
                                   BUFFEREVENT_SSL_ACCEPTING,
                                   BEV_OPT_CLOSE_ON_FREE |
                                   BEV_OPT_DEFER_CALLBACKS);
  if (!c->remote_bev) {
    errputs(stderr, "Unable to make SSL bufferevent!");
    SSL_free(ssl);
    delete_conn(c);
    return;
  }

  bufferevent_setcb(c->remote_bev, NULL, NULL, event_cb, c);
  bufferevent_set_timeouts(c->remote_bev, &handshake_timeout,
                           &handshake_timeout);
  bufferevent_enable(c->remote_bev, EV_WRITE);
}

/** Called periodically to ensure the parent mush is still there. */
static void
check_parent(evutil_socket_t fd __attribute__ ((__unused__)),
             short what __attribute__ ((__unused__)),
             void *arg __attribute__ ((__unused__)))
{
  if (getppid() != parent_pid) {
    errputs(stderr,
            "Parent mush process exited unexpectedly! Shutting down.");
    event_base_loopbreak(main_loop);
  }
}

/* Shut down gracefully on a SIGTERM */
static void
shutdown_cb(evutil_socket_t fd __attribute__ ((__unused__)),
            short what __attribute__ ((__unused__)),
            void *arg __attribute__ ((__unused__)))
{
  struct conn *c;
  for (c = connections; c; c = c->next) {
    c->state = C_SHUTTINGDOWN;
    if (c->remote_bev) {
      bufferevent_disable(c->remote_bev, EV_READ);
      bufferevent_flush(c->remote_bev, EV_WRITE, BEV_FINISHED);
    }
    if (c->local_bev) {
      bufferevent_disable(c->local_bev, EV_READ);
      bufferevent_flush(c->local_bev, EV_WRITE, BEV_FINISHED);
    }
  }
  event_base_loopexit(main_loop, NULL);
}

int
main(int argc, char **argv)
{
  char *ssl_ipaddr;
  int ssl_port;
  char *ssl_private_key_file, *ssl_ca_file;
  int ssl_require_client_cert;
  struct event *watch_parent, *sigterm_handler;
  struct timeval parent_timeout = {.tv_sec = 5,.tv_usec = 0 };
  struct event *ssl_listener;
  struct conn *c, *n;

  if (argc != 8) {
    errprintf(stderr, "ssl_slave expects 8 arguments, got %d!\n", argc - 1);
    return EXIT_FAILURE;
  }

  socket_file = argv[1];
  ssl_ipaddr = argv[2];
  ssl_port = strtol(argv[3], NULL, 10);
  ssl_private_key_file = argv[4];
  ssl_ca_file = argv[5];
  ssl_require_client_cert = strtol(argv[6], NULL, 10);
  keepalive_timeout = strtol(argv[7], NULL, 10);

  init_gen_rand(getpid());
  parent_pid = getppid();

  if (!ssl_init(ssl_private_key_file, ssl_ca_file, ssl_require_client_cert)) {
    errputs(stderr, "SSL initialization failure!");
    exit(EXIT_FAILURE);
  }

  main_loop = event_base_new();
  resolver = evdns_base_new(main_loop, 1);

  /* Listen for incoming connections on the SSL port */
  ssl_sock = make_socket(ssl_port, SOCK_STREAM, NULL, NULL, ssl_ipaddr);
  ssl_listener =
    event_new(main_loop, ssl_sock, EV_READ | EV_PERSIST, new_conn_cb, NULL);
  event_add(ssl_listener, NULL);

  /* Run every 5 seconds to see if the parent mush process is still around. */
  watch_parent =
    event_new(main_loop, -1, EV_TIMEOUT | EV_PERSIST, check_parent, NULL);
  event_add(watch_parent, &parent_timeout);

  /* Catch shutdown requests from the parent mush */
  sigterm_handler =
    event_new(main_loop, SIGTERM, EV_SIGNAL | EV_PERSIST, shutdown_cb, NULL);
  event_add(sigterm_handler, NULL);

  errprintf(stderr, "ssl_slave: starting event loop using %s.\n",
            event_base_get_method(main_loop));
  event_base_dispatch(main_loop);
  errputs(stderr, "shutting down.");

  close(ssl_sock);

  evdns_base_free(resolver, 0);

  for (c = connections; c; c = n) {
    n = c->next;
    if (c->remote_bev)
      SSL_shutdown(bufferevent_openssl_get_ssl(c->remote_bev));
    free_conn(c);
  }

  return EXIT_SUCCESS;
}

const char *
time_string(void)
{
  static char buffer[100];
  time_t now;
  struct tm *ltm;
  
  now = time(NULL);
  ltm = localtime(&now);
  strftime(buffer, 100, "%m/%d %T", ltm);

  return buffer;
}

/* Wrappers for perror */
void
penn_perror(const char *err)
{
  lock_file(stderr);
  fprintf(stderr, "[%s] ssl_slave: %s: %s\n", time_string(), err, strerror(errno));
  unlock_file(stderr);
}

void
errprintf(FILE * fp, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  lock_file(fp);
  vfprintf(fp, fmt, ap);
  fflush(fp);
  unlock_file(fp);
  va_end(ap);
}

/* Wrapper for fputs(foo,stderr) */
void
errputs(FILE * fp, const char *msg)
{
  lock_file(fp);
  fprintf(fp, "[%s] ssl_slave: %s\n", time_string(), msg);
  unlock_file(fp);
}


#endif
