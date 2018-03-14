/** SSL slave controller related code. */

#include "copyrite.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"
#include "log.h"
#include "mysocket.h"
#include "parse.h"
#include "sig.h"
#include "ssl_slave.h"
#include "wait.h"

pid_t ssl_slave_pid = -1;
enum ssl_slave_state ssl_slave_state = SSL_SLAVE_DOWN;

extern int maxd;

bool ssl_slave_halted = false;
enum {
  MAX_ATTEMPTS =
    5 /**< Error out after this many startup attempts in 60 seconds */
};

#ifdef SSL_SLAVE
static int startup_attempts = 0;
static time_t startup_window;

int ssl_slave_ctl_fd = -1;

/** Create a new SSL slave.
 * \param port The port to listen on for SSL connections.
 * \return 0 on success, -1 on failure
 */
int
make_ssl_slave(void)
{
  int fds[2];

  if (ssl_slave_state != SSL_SLAVE_DOWN) {
    do_rawlog(LT_ERR,
              "Attempt to start ssl slave when a copy is already running.");
    return -1;
  }

  if (ssl_slave_halted) {
    do_rawlog(LT_ERR, "Attempt to start disabled ssl slave.");
    return -1;
  }

  if (startup_attempts == 0)
    time(&startup_window);

  startup_attempts += 1;

  if (startup_attempts > MAX_ATTEMPTS) {
    time_t now;

    time(&now);
    if (difftime(now, startup_window) <= 60.0) {
      do_rawlog(LT_ERR, "Disabling ssl_slave due to too many errors.");
      ssl_slave_halted = true;
      return -1;
    } else {
      /* Reset counter */
      startup_window = now;
      startup_attempts = 0;
    }
  }

  if (pipe(fds) < 0) {
    do_rawlog(LT_ERR, "Unable to create pipe to speak to ssl_slave: %s",
              strerror(errno));
    return -1;
  }

  if (fds[0] >= maxd)
    maxd = fds[0] + 1;
  if (fds[1] >= maxd)
    maxd = fds[1] + 1;

  if ((ssl_slave_pid = fork()) == 0) {
    /* Set up and exec ssl_slave */
    int n, errfd = -1, connfd = -1;
    struct log_stream *lg;

    /* Close all open files but LT_CONN and LT_ERR, and assign them as
       stdout and stderr, respectively. */

    /* If called on startup, maxd is 0 but log files and such have
       been opened. Use a reasonable max descriptor. If called because
       ssl_slave went down, maxd will be set properly already. */
    if (!maxd)
      maxd = 20;

    lg = lookup_log(LT_ERR);
    if (lg)
      errfd = fileno(lg->fp);

    lg = lookup_log(LT_CONN);
    if (lg)
      connfd = fileno(lg->fp);

    dup2(fds[0], 0); /* stdin */
    dup2(connfd, 1); /* stdout */
    dup2(errfd, 2);  /* stderr */

    for (n = 3; n < maxd; n++)
      close(n);

    execl("./ssl_slave", "ssl_slave", "for", MUDNAME, NULL);
    penn_perror("execing ssl slave");
    return EXIT_FAILURE;
  } else if (ssl_slave_pid < 0) {
    do_rawlog(LT_ERR, "Failure to fork ssl_slave: %s", strerror(errno));
    return -1;
  } else {
    struct ssl_slave_config cf;

    ssl_slave_ctl_fd = fds[1];
    close(fds[0]);

    /* Set up arguments to the slave */
    memset(&cf, 0, sizeof cf);
    strcpy(cf.socket_file, options.socket_file);
    strcpy(cf.ssl_ip_addr, SSL_IP_ADDR);
    cf.normal_port = options.port;
    cf.ssl_port = options.ssl_port;
    strcpy(cf.private_key_file, options.ssl_private_key_file);
    strcpy(cf.ca_file, options.ssl_ca_file);
    strcpy(cf.ca_dir, options.ssl_ca_dir);
    cf.require_client_cert = options.ssl_require_client_cert;
    cf.keepalive_timeout = options.keepalive_timeout;

    if (write(ssl_slave_ctl_fd, &cf, sizeof cf) < 0) {
      do_rawlog(LT_ERR, "Unable to send ssl_slave config options: %s",
                strerror(errno));
      return -1;
    }

    ssl_slave_state = SSL_SLAVE_RUNNING;
    do_rawlog(LT_ERR, "Spawning ssl_slave, communicating over %s, pid %d.",
              options.socket_file, ssl_slave_pid);
    return 0;
  }

  return -1;
}

void
kill_ssl_slave(void)
{
  if (ssl_slave_pid > 0) {
    WAIT_TYPE my_stat;

    do_rawlog(LT_ERR, "Terminating ssl_slave pid %d", ssl_slave_pid);

    block_a_signal(SIGCHLD);
    kill(ssl_slave_pid, SIGTERM);
    mush_wait(ssl_slave_pid, &my_stat, 0);
    unblock_a_signal(SIGCHLD);
    close(ssl_slave_ctl_fd);
    ssl_slave_pid = -1;
    ssl_slave_state = SSL_SLAVE_DOWN;
  }
}

#endif
