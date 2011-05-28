/** SSL slave controller related code. */

#include "copyrite.h"
#include "config.h"

#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <signal.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"
#include "externs.h"
#include "log.h"
#include "mysocket.h"
#include "ssl_slave.h"
#include "parse.h"
#include "wait.h"
#include "confmagic.h"


pid_t ssl_slave_pid = -1;
enum ssl_slave_state ssl_slave_state = SSL_SLAVE_DOWN;

extern int maxd;

static int startup_attempts = 0;
static time_t startup_window;
bool ssl_slave_halted = false;
enum {
  MAX_ATTEMPTS = 5 /**< Error out after this many startup attempts in 60 seconds */
}; 


#ifdef SSL_SLAVE

/** Create a new SSL slave.
 * \param port The port to listen on for SSL connections.
 * \return 0 on success, -1 on failure
 */
int
make_ssl_slave(void)
{
  if (ssl_slave_state != SSL_SLAVE_DOWN) {
    do_rawlog(LT_ERR, "Attempt to start ssl slave when a copy is already running.");
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

  if ((ssl_slave_pid = fork()) == 0) {
    /* Set up and exec ssl_slave */
    char *args[9];
    int n, errfd;

    /* Close extranenous file descriptors */
    errfd = fileno(stderr);
    for (n = 0; n < maxd; n++) {
      if (n == errfd)
	continue;
      close(n);
    }

    /* Set up arguments to the slave */
    args[0] = "ssl_slave";
    args[1] = options.socket_file;
    args[2] = SSL_IP_ADDR;
    args[3] = strdup(unparse_integer(options.ssl_port));
    args[4] = options.ssl_private_key_file;
    args[5] = options.ssl_ca_file;
    args[6] = options.ssl_require_client_cert ? "1" : "0";
    args[7] = strdup(unparse_integer(options.keepalive_timeout));
    args[8] = NULL;

    execv("./ssl_slave", args);
    penn_perror("execing ssl slave");
    return EXIT_FAILURE;
  } else if (ssl_slave_pid < 0) {
    do_rawlog(LT_ERR, "Failure to fork ssl_slave: %s", strerror(errno));
    return -1;
  } else {
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
    ssl_slave_pid = -1;
    ssl_slave_state = SSL_SLAVE_DOWN;
  }
}

#endif
