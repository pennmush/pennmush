/*
 *    portmsg - generate a message on a port, then close connection
 *
 *      Usage:  portmsg file port
 *
 *              When a telnet client connects to the specified port, the
 *              text from the file will be echoed to the user.  After a
 *              short delay the connection will close.
 *
 * Derived from ftpd by Klaas @ {RUD, LPSwat}, original ftpd copyright
 * message follows:
 *
 * Copyright (c) 1985, 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This version extensively modified by Javelin (Alan Schwartz)
 * and Raevnos (Shawn Wagner) to conform to PennMUSH standards.
 */

/* If you have multiple IP addresses and want to bind to only one,
 * uncomment this:
 */
/*#define SINGLE_IP_ADDR "your.address.goes.here" */

#include "config.h"
#include <stdio.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <limits.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#endif
#ifdef I_SYS_PARAM
#include <sys/param.h>
#endif
#include <signal.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#include <string.h>
#include <signal.h>

#include "conf.h"
#include "externs.h"
#include "mysocket.h"
#include "wait.h"

#ifndef SINGLE_IP_ADDR
const char *host_ip = "";
#else
const char *host_ip = SINGLE_IP_ADDR;
#endif

static void wait_on_child(int sig);
static void
lostconn(int sig)
  NORETURN;

    enum { MAX_CONNECTIONS = 15 };
    sig_atomic_t connections = 0;

    static void
     wait_on_child(int sig)
{
  WAIT_TYPE status;

  while (mush_wait(0, &status, WNOHANG) > 0)
    connections--;

  if (connections < 0)
    connections = 0;

  reload_sig_handler(sig, wait_on_child);
}

void
lostconn(int sig __attribute__ ((__unused__)))
{
  exit(1);
}

int
main(int argc, char **argv)
{
  int msgfd;
  struct stat statBuf;
  Port_t port;
  char *msg;
  int sockfd, newsockfd;
  socklen_t addrlen = 0;
  union sockaddr_u their_addr;

  if (argc != 3) {
    fprintf(stderr, "Usage: portmsg file port\n");
    return 1;
  }
  port = atoi(argv[2]);
  if (port == 0) {
    fprintf(stderr, "error: bad port number [%s]\n", argv[2]);
    return 1;
  }
  if ((msgfd = open(argv[1], O_RDONLY)) < 0) {
    fprintf(stderr, "error: cannot open message file [%s]: %s\n", argv[1],
            strerror(errno));
    return 1;
  }
  /* read the message */
  fstat(msgfd, &statBuf);
  if (statBuf.st_size <= 0) {
    fprintf(stderr, "error: message file [%s] is empty\n", argv[1]);
    return 1;
  }
  msg = (char *) malloc(statBuf.st_size);
  if (read(msgfd, msg, statBuf.st_size) != statBuf.st_size) {
    fprintf(stderr, "error: cannot read message file [%s]\n", argv[1]);
    return 1;
  }

  /* become a daemon */
  switch (fork()) {
  case -1:
    perror("can't fork");
    return 1;
  case 0:
    break;
  default:
    return 0;
  }
#ifdef HAVE_SETSID
  if (setsid() < 0)
    perror("Unable to create new session id (Harmless)");
#else
  if (new_process_group() < 0)
    perror("Unable to set new process group (Probably harmless)");
#endif

#ifdef USE_TIOCNOTTY
  if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
    ioctl(fd, TIOCNOTTY, NULL);
    close(fd);
  }
#endif

  install_sig_handler(SIGCHLD, wait_on_child);

  if ((sockfd = make_socket(port, SOCK_STREAM, NULL, NULL, host_ip)) < 0) {
    perror("can't make socket");
    return 1;
  }

main_again:
  if (connections > MAX_CONNECTIONS) {
    sleep(1);
    goto main_again;
  }
  addrlen = sizeof(their_addr);
  newsockfd = accept(sockfd, &their_addr.addr, &addrlen);
  if (newsockfd < 0) {
    if (errno == EINTR)
      goto main_again;
    perror("Couldn't accept connection");
    return 1;
  }
  connections++;
  switch (fork()) {
  case -1:
    perror("server can't fork");
    return 1;
  case 0:
    /* child process */
    install_sig_handler(SIGPIPE, lostconn);
    ignore_signal(SIGCHLD);
    send(newsockfd, msg, statBuf.st_size, 0);
    sleep(5);
    closesocket(newsockfd);
    break;
  default:
    closesocket(newsockfd);
    goto main_again;
  }

  return 0;
}

/* Wrappers for perror */
void
penn_perror(const char *err)
{
  fprintf(stderr, "portmsg: %s: %s\n", err, strerror(errno));
}
