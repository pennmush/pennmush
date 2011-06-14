/**
 * \file wait.c
 *
 * \brief Process and process-group control functions.
 */

#include "config.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "wait.h"

void penn_perror(const char *);

/** Portable wait
 * \param child pid of specific child proccess to wait for. Only meaningful if HAVE_WAITPID is defined. 
 * \param status pointer to store the child process's exit status in.
 * \param flags optional flags to pass to waitpid() or wait3().
 * \return pid of child process that exited, or -1.
 */
pid_t
mush_wait(pid_t child __attribute__ ((__unused__)),
          WAIT_TYPE *status __attribute__ ((__unused__)),
          int flags __attribute__ ((__unused__)))
{

#if defined(HAVE_WAITPID)
  return waitpid(child, status, flags);
#elif defined(HAVE_WAIT3)
  return wait3(status, flags, NULL);
#elif defined(HAVE_WAIT)
  /* Wait as long as it's okay to block */
  if (flags == 0)
    return wait(status);
  else
    return -1;
#else
  /* Not implemented */
  return -1;
#endif
}

/** Portable setpgid()/setpgrp()
 * \param pid process to change group. 0 for current process.
 * \param pgrp new process group.
 * \return 0 on success, -1 on failure.
 */
int
set_process_group(pid_t pid __attribute__ ((__unused__)),
                  pid_t pgrp __attribute__ ((__unused__)))
{
#if defined(HAVE_SETPGID)

  return setpgid(pid, pgrp);

#elif defined(HAVE_SETPGRP)

#ifndef VOID_SETPGRP
  return setpgrp(pid, pgrp);
#else
  return setpgrp();
#endif

#else
  return 0;
#endif
}

/** Create a new process group if possible.
 * Equivalent to set_process_group(0, getpid()).
 * \return 0 on success, -1 on failure
 */
int
new_process_group(void)
{
  return set_process_group(0, getpid());
}

/** Create a new process session if possible.
 * See documentation for setsid(2). If that system call
 * is not available, make a new process group.
 * \return 0 on success, -1 on failure
 */
int
new_process_session(void)
{
#if defined(HAVE_SETSID)
  return setsid();
#else
  /* Close enough for government work */
  return new_process_group();
#endif
}


/**
 * Lowers the scheduling priority of a process by a given amount. Note that
 * on Unix, the higher the number, the lower the priority.
 * \param pid the process id of the process to change.
 * \param prio how much to add to the priority.
 * \return 0 on success, -1 on error.
 */
int
lower_priority_by(pid_t pid, int prio)
{
  int newprio = 0;

#ifdef HAVE_GETPRIORITY
  errno = 0;
  if ((newprio = getpriority(PRIO_PROCESS, pid)) < 0) {
    if (errno != 0)
      return newprio;
  }
#endif
  newprio += prio;

#ifdef HAVE_SETPRIORITY
  {
    int ret = setpriority(PRIO_PROCESS, pid, newprio);
    if (ret < 0)
      perror("setpriority");
    return ret;
  }
#else
  return 0;
#endif
}

/* This stuff is here because info_slave and netmud both use it
   and I don't want to duplicate code, and putting it anywhere else
   will draw it too much stuff into info_slave */

static int
lock_fp(FILE * f, bool what)
{
#if defined(HAVE_FCNTL) && !defined(WIN32)
  struct flock lock;
  int ret;

  memset(&lock, 0, sizeof lock);
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (what)
    lock.l_type = F_WRLCK;
  else
    lock.l_type = F_UNLCK;

  ret = fcntl(fileno(f), F_SETLKW, &lock);
  if (ret < 0)
    perror("fcntl");

  return ret;
#else
  return -1;
#endif
}

/** Obtain an exclusive advisory lock on a file pointer. Can block.
 * \param f the file to lock
 * \return 0 on success, -1 on failure.
 */
int
lock_file(FILE * f)
{
  return lock_fp(f, 1);
}

/** Release a lock on a file pointer.
 * \param f the file to lock
 * \return 0 on success, -1 on failure.
 */
int
unlock_file(FILE * f)
{
  return lock_fp(f, 0);
}
