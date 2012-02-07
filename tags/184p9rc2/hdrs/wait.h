/**
 * \file wait.h
 *
 * \brief Process and process group control functions.
 *
 * Must be #included after config.h, <sys/types.h> and <unistd.h>
 */

#ifndef WAIT_H
#define WAIT_H

#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/* What does wait*() return? */
#ifdef HAS_WAITPID
typedef int WAIT_TYPE;
#else                           /* Use wait3 */
#ifdef UNION_WAIT
typedef union wait WAIT_TYPE;
#else
typedef int WAIT_TYPE;
#endif
#endif

/* Exit status of child processes */
pid_t mush_wait(pid_t child, WAIT_TYPE *stat, int flags);


/* process groups and sessions */
int set_process_group(pid_t, pid_t);
int new_process_group(void);
int new_process_session(void);

/* Priorities */
int lower_priority_by(pid_t, int);

#endif                          /* WAIT_H */
