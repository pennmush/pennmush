/** \file mythread.c
 *
 * \brief Implementations of OS-independent thread functions
 */

#include <stdlib.h>
#include "mythread.h"
#include "confmagic.h"

extern desc_mutex;
extern queue_mutex;

int
thread_init(void)
{
  mutex_init(&desc_mutex);
  mutex_init(&queue_mutex);
  return 0;
}

int
thread_cleanup(void)
{
  mutex_destroy(&desc_mutex);
  mutex_destroy(&queue_mutex);
  return 0;
}

#ifdef HAVE_PTHREADS
/* Pthreads wrappers */

int
run_thread(thread_id *id, thread_func f, void *arg, bool detach) {
  int r = pthread_create(id, NULL, f, arg);
  if (r == 0 && detach) {
    pthread_detach(*id);
  }
  return r;
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
  static pthread_mutexattr_t attr;
  static bool init = false;

  if (!init) {
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    init = true;
  }
  
  return pthread_mutex_init(mut, &attr);
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
run_thread(thread_id *id, thread_func f, void *arg, bool detach) {
  uintptr_t h = _beginthreadex(NULL, 0, f, arg, 0, NULL);
  if (h == 0) {
    return -1;
  } else {
    *id = (HANDLE) h;
    if (detach) {
      CloseHandle(*id);
    }
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
