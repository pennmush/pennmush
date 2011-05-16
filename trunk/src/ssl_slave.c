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

#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <event.h>
#include <event2/dns.h>
#include <event2/bufferevent_ssl.h>

#include "conf.h"
#include "externs.h"
#include "myssl.h"
#include "mysocket.h"
#include "log.h"
#include "ssl_slave.h"

#include "confmagic.h"

/* 0 for no debugging messages, 1 for connection related, 2 for every read/write */
#define SSL_DEBUG_LEVEL 1

pid_t ssl_slave_pid = -1;
enum ssl_slave_state ssl_slave_state = SSL_SLAVE_DOWN;
static pid_t parent_pid = -1;
int ssl_sock;
static struct event_base *main_loop = NULL;
static struct evdns_base *resolver = NULL;
static void event_cb(struct bufferevent *bev, short e, void *data);

enum conn_state {
  C_SSL_CONNECTING,
  C_HOSTNAME_LOOKUP,
  C_LOCAL_CONNECTING,
  C_ESTABLISHED,
  C_SHUTTINGDOWN
};

struct conn {
  enum conn_state state;
  int local_fd;
  int remote_fd;
  union sockaddr_u remote_addr;
  socklen_t remote_addrlen;
  char *remote_host;
  char *remote_ip;
  SSL *ssl;
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
      break;
      }
  }
  free_conn(c);
}

/** Address to hostname lookup wrapper */
static struct evdns_request *
evdns_getnameinfo(struct evdns_base *base, const struct sockaddr *addr, int flags,
		  evdns_callback_type callback, void *data)
{
  if (addr->sa_family == AF_INET) {
    const struct sockaddr_in *a = (const struct sockaddr_in*) addr;
#if SSL_DEBUG_LEVEL > 1
    do_rawlog(LT_CONN, "ssl_slave: Remote connection is IPv4");
#endif
    return evdns_base_resolve_reverse(base, &a->sin_addr, flags, callback, data);
  } else if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6 *a = (const struct sockaddr_in6*) addr;
#if SSL_DEBUG_LEVEL > 1
    do_rawlog(LT_CONN, "ssl_slave: Remote connection is IPv6");
#endif
    return evdns_base_resolve_reverse_ipv6(base, &a->sin6_addr, flags, callback, data);
  } else {
    do_rawlog(LT_ERR, "ssl_slave: Attempt to resolve unknown socket family %d", addr->sa_family);
    return NULL;
  }
}

/* libevent callback functions */

/** Read from one buffer and write the results to the other */
static void
pipe_cb(struct bufferevent *from_bev, void *data)
{
  struct conn *c = data;
  char buff[BUFFER_LEN];
  size_t len;
  struct bufferevent *to_bev = NULL;

  if (c->local_bev == from_bev) {
#if SSL_DEBUG_LEVEL > 1
    do_rawlog(LT_TRACE, "ssl_slave: got data from mush");
#endif
    to_bev = c->remote_bev;
  } else {
#if SSL_DEBUG_LEVEL > 1
    do_rawlog(LT_TRACE, "ssl_slave: got data from SSL");
#endif
    to_bev = c->local_bev;
  }

  len = bufferevent_read(from_bev, buff, sizeof buff);

#if SSL_DEBUG_LEVEL > 1
  do_rawlog(LT_TRACE, "ssl_slave: read %zu bytes", len);
#endif

  if (to_bev && len > 0) {
    if (bufferevent_write(to_bev, buff, len) < 0)
      do_rawlog(LT_ERR, "ssl_slave: write failed!");
  }
}

/** Called after the local connection to the mush has established */
static void
local_connected(struct conn *c)
{
  char *hostid;
  int len;

#if SSL_DEBUG_LEVEL > 0
  do_rawlog(LT_CONN, "ssl_slave: Local connection attempt completed. Setting up pipe.");
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
static void
address_resolved(int result, char type, int count, int ttl __attribute__((__unused__)),
		 void *addresses, void *data)
{
  const char *hostname;
  struct conn *c = data;
  struct sockaddr_un addr;
  struct hostname_info *ipaddr;

  if (result != DNS_ERR_NONE || !addresses || type != DNS_PTR || count == 0) {
    do_rawlog(LT_ERR, "ssl_slave: Hostname lookup failed: %s. type = %d, count = %d", evdns_err_to_string(result),
	      (int)type, count);
    delete_conn(c);
    return;
  }

  hostname = ((const char **)addresses)[0];

  c->remote_host = strdup(hostname);
  c->resolver_req = NULL;
  ipaddr = ip_convert(&c->remote_addr.addr, c->remote_addrlen);
  c->remote_ip = strdup(ipaddr->hostname);  

#if SSL_DEBUG_LEVEL > 0
  do_rawlog(LT_CONN, "ssl_slave: resolved hostname as '%s(%s)'. Opening local connection to mush.", hostname, ipaddr->hostname);
#endif

  c->state = C_LOCAL_CONNECTING;

  addr.sun_family = AF_LOCAL;
  strncpy(addr.sun_path, options.socket_file, sizeof(addr.sun_path) - 1);
  c->local_bev = bufferevent_socket_new(main_loop, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  bufferevent_socket_connect(c->local_bev, (struct sockaddr *)&addr, sizeof addr);
  bufferevent_setcb(c->local_bev, NULL, NULL, event_cb, data);
  bufferevent_enable(c->local_bev, EV_WRITE);
}

/** Called after the SSL connection and initial handshaking is complete. */
static void
ssl_connected(struct conn *c)
{
  X509 *peer;

#if SSL_DEBUG_LEVEL > 0
  do_rawlog(LT_CONN, "ssl_slave: SSL connection attempt completed. Resolving remote host name.");
#endif

  /* Successful accept. Log peer certificate, if any. */
  if ((peer = SSL_get_peer_certificate(c->ssl))) {
    if (SSL_get_verify_result(c->ssl) == X509_V_OK) {
      char buf[256];
      /* The client sent a certificate which verified OK */
      X509_NAME_oneline(X509_get_subject_name(peer), buf, 256);
      do_rawlog(LT_CONN, "SSL client certificate accepted: %s", buf);
    }
  }

  c->state = C_HOSTNAME_LOOKUP;
  c->resolver_req = evdns_getnameinfo(resolver, &c->remote_addr.addr, 0, address_resolved, c);
}

/** Called on successful connections and errors */
static void
event_cb(struct bufferevent *bev, short e, void *data)
{
  struct conn *c = data;

#if SSL_DEBUG_LEVEL > 1
  do_rawlog(LT_TRACE, "ssl_slave: event callback triggered with flags 0x%hx", e);
#endif

  if (e & BEV_EVENT_CONNECTED) {
    if (c->local_bev == bev) {
      local_connected(c);
    } else {
      ssl_connected(c);
    }
  } else if (e & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    if (c->local_bev == bev) {
      /* Mush side of the connection went away. Flush SSL buffer and shut down. */
#if SSL_DEBUG_LEVEL > 0
      do_rawlog(LT_ERR, "ssl_slave: Lost local connection.");
#endif
      bufferevent_disable(c->local_bev, EV_READ | EV_WRITE);
      bufferevent_free(c->local_bev);
      c->local_bev = NULL;
      c->state = C_SHUTTINGDOWN;
      if (c->remote_bev) {
	bufferevent_disable(c->remote_bev, EV_READ);
	bufferevent_flush(c->remote_bev, EV_WRITE, BEV_FINISHED);
	delete_conn(c);
      }
    } else {
      /* Remote side of the connetion went away. Flush mush buffer and shut down. */
#if SSL_DEBUG_LEVEL > 0
      do_rawlog(LT_ERR, "ssl_slave: Lost SSL connection.");
#endif
      bufferevent_disable(c->remote_bev, EV_READ | EV_WRITE);
      bufferevent_free(c->remote_bev);
      c->remote_bev = NULL;
      c->state = C_SHUTTINGDOWN;
      if (c->local_bev) {
	bufferevent_disable(c->local_bev, EV_READ);
	bufferevent_flush(c->local_bev, EV_WRITE, BEV_FINISHED);
	delete_conn(c);
      }
    }
  }
}

/* Called when a new connection is made on the ssl port */
static void
new_conn_cb(evutil_socket_t s, short flags __attribute__((__unused__)), void *data __attribute__((__unused__)))
{
  struct conn *c;

  /* Accept a connection and do SSL handshaking */

#if SSL_DEBUG_LEVEL > 0
  do_rawlog(LT_CONN, "Got new connection on SSL port.");
#endif

  c = alloc_conn();

  if (connections) 
    connections->prev = c;
  c->next = connections;
  connections = c;

  c->state = C_SSL_CONNECTING;

  c->remote_addrlen = sizeof c->remote_addr;
  c->remote_fd = accept(s, &c->remote_addr.addr, &c->remote_addrlen);
  if (c->remote_fd < 0) {
    do_rawlog(LT_ERR, "ssl_slave: accept: %s", strerror(errno));
    delete_conn(c);
    return;
  }

  c->ssl = ssl_alloc_struct();
  c->remote_bev = bufferevent_openssl_socket_new(main_loop, c->remote_fd, c->ssl, BUFFEREVENT_SSL_ACCEPTING, 
						 BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  if (!c->remote_bev) {
    do_rawlog(LT_ERR, "ssl_slave: Unable to make SSL bufferevent!");
    delete_conn(c);
    return;
  }
  bufferevent_setcb(c->remote_bev, NULL, NULL, event_cb, c);
  bufferevent_enable(c->remote_bev, EV_WRITE);
}

/** Called periodically to ensure the parent mush is still there. */
static void
check_parent(evutil_socket_t fd __attribute__((__unused__)),
	     short what __attribute__((__unused__)),
	     void *arg __attribute__((__unused__)))
{
  if (getppid() != parent_pid) {
    do_rawlog(LT_ERR, T("ssl_slave: Parent mush process exited unexpectedly! Shutting down."));
    event_base_loopbreak(main_loop);
  }
}

/* Shut down gracefully on a SIGTERM */
static void
shutdown_cb(evutil_socket_t fd __attribute__((__unused__)),
	     short what __attribute__((__unused__)),
	     void *arg __attribute__((__unused__)))
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

/** Create a new SSL slave.
 * \param port The port to listen on for SSL connections.
 * \return File descriptor to listen to connections on. 
 */
int
make_ssl_slave(Port_t port)
{
  parent_pid = getpid();
  
  if ((ssl_slave_pid = fork()) == 0) {
    struct event *watch_parent, *sigterm_handler;
    struct timeval parent_timeout = { .tv_sec = 5, .tv_usec = 0 };
    struct event *ssl_listener;
    struct conn *c, *n;

    if (!ssl_init
	(options.ssl_private_key_file, options.ssl_ca_file,
	 options.ssl_require_client_cert)) {
      do_rawlog(LT_ERR, "SSL initialization failure\n");
      exit(EXIT_FAILURE);
    }

    main_loop = event_base_new();
    resolver = evdns_base_new(main_loop, 1);

    /* Listen for incoming connections on the SSL port */
    ssl_sock = make_socket(port, SOCK_STREAM, NULL, NULL, SSL_IP_ADDR);        
    ssl_listener = event_new(main_loop, ssl_sock, EV_READ | EV_PERSIST, new_conn_cb, NULL);
    event_add(ssl_listener, NULL);

    /* Run every 5 seconds to see if the parent mush process is still around. */
    watch_parent = event_new(main_loop, -1, EV_TIMEOUT | EV_PERSIST, check_parent, NULL);
    event_add(watch_parent, &parent_timeout);

    /* Catch shutdown requests from the parent mush */
    sigterm_handler = event_new(main_loop, SIGTERM, EV_SIGNAL | EV_PERSIST, shutdown_cb, NULL);
    event_add(sigterm_handler, NULL);

    do_rawlog(LT_ERR, T("ssl_slave: starting event loop using %s."), event_base_get_method(main_loop));
    event_base_dispatch(main_loop);
    do_rawlog(LT_ERR, T("ssl_slave: shutting down."));

    close(ssl_sock);

    for (c = connections; c; c = n) {
      n = c->next;
      free_conn(c);
    }
  
    exit(EXIT_SUCCESS);
  } else if (ssl_slave_pid < 0) {
    do_rawlog(LT_ERR, "Failure to fork ssl_slave: %s", strerror(errno));
    return -1;
  } else {
    ssl_slave_state = SSL_SLAVE_RUNNING;
    do_rawlog(LT_ERR, "Spawning ssl_slave, pid %d", ssl_slave_pid);
    return make_unix_socket(options.socket_file, SOCK_STREAM);
  }  
}

void
kill_ssl_slave(void)
{
  if (ssl_slave_pid > 0) {
    kill(ssl_slave_pid, SIGTERM);
    ssl_slave_state = SSL_SLAVE_DOWN;
  }
}

#endif
