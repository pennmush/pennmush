/** \file mythread.c
 *
 * \brief Implementations of OS-independent thread functions
 */

#include <stdlib.h>
#include "mythread.h"
#include "confmagic.h"

#ifdef HAVE_PTHREADS
/* Pthreads wrappers */

int
thread_init(void)
{
  return 0;
}

int
thread_cleanup(void)
{
  return 0;
}

int
run_thread(thread_id *id, thread_func f, void *arg) {
  return pthread_create(id, NULL, f, arg);
}

void
exit_thread(THREAD_RETURN_TYPE retval)
{
  pthread_exit(retval);
}

int
join_thread(thread_id id, THREAD_RETURN_TYPE *retval)
{
  return pthread_join(id, retval);
}

int
mutex_init(penn_mutex *mut)
{
  return pthread_mutex_init(mut, NULL);
}

int
mutex_destroy(penn_mutex *mut)
{
  return pthread_mutex_destroy(mut);
}

int
mutex_lock(penn_mutex *mut)
{
  return pthread_mutex_lock(mut);
}

int
mutex_unlock(penn_mutex *mut)
{
  return pthread_mutex_unlock(mut);
}

#elif defined(WIN32)

#include <process.h>

/* Win32 wrappers */

/* TODO: test */

int
thread_init(void)
{
  return 0;
}

int
thread_cleanup(void)
{
  return 0;
}

int
run_thread(thread_id *id, thread_func f, void *arg) {
  uintptr_t h = _beginthreadex(NULL, 0, f, arg, 0, NULL);
  if (h == 0) {
    return -1;
  } else {
    *id = (HANDLE) h;
    return 0;
  }
}

void
exit_thread(THREAD_RETURN_TYPE retval)
{
  _endthreadex(retval);
}

int
join_thread(thread_id id, THREAD_RETURN_TYPE *retval)
{
  DWORD exitcode;

  if (WaitForSingleObject(id, INFINITE) != 0) {
    return -1;
  }
  if (GetExitCodeThread(id, &exitcode) == 0) {
    return -1;
  }
  *retval = exitcode;
  CloseHandle(id);
  return 0;
}

int
mutex_init(penn_mutex *mut)
{
  InitializeCriticalSection(mut);
  return 0;
}

int
mutex_destroy(penn_mutex *mut)
{
  DeleteCriticalSection(mut);
  return 0;
}

int
mutex_lock(penn_mutex *mut)
{
  EnterCriticalSection(mut);
  return 0;
}

int
mutex_unlock(penn_mutex *mut)
{
  LeaveCriticalSection(mut);
  return 0;
}


#endif
