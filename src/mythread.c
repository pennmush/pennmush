/** \file mythread.c
 *
 * \brief Implementations of OS-independent thread functions
 */

#include <stdlib.h>
#include "mythread.h"
#include "confmagic.h"

extern penn_mutex desc_mutex;
extern penn_mutex queue_mutex;
extern penn_mutex site_mutex;

#ifdef HAVE_PTHREADS
static void init_pthreads(void);
static void dest_pthreads(void);
#endif

int
thread_init(void)
{
#ifdef HAVE_PTHREADS
  init_pthreads();
#endif
  mutex_init(&desc_mutex);
  mutex_init(&queue_mutex);
  mutex_init(&site_mutex);
  return 0;
}

int
thread_cleanup(void)
{
  mutex_destroy(&desc_mutex);
  mutex_destroy(&queue_mutex);
  mutex_destroy(&site_mutex);
#ifdef HAVE_PTHREADS
  dest_pthreads();
#endif
  return 0;
}

#ifdef HAVE_PTHREADS
/* Pthreads wrappers */

static pthread_mutexattr_t recattr;
static pthread_attr_t detached;

static void
init_pthreads(void)
{
  pthread_mutexattr_init(&recattr);
  pthread_mutexattr_settype(&recattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_attr_init(&detached);
  pthread_attr_setdetachstate(&detached, PTHREAD_CREATE_DETACHED);
}

static void
dest_pthreads(void)
{
  pthread_mutexattr_destroy(&recattr);
  pthread_attr_destroy(&detached);
}

int
run_thread(thread_id *id, thread_func f, void *arg, bool detach) {
  pthread_attr_t *attr = detach ? &detached : NULL;
  return pthread_create(id, attr, f, arg);
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
  return pthread_mutex_init(mut, &recattr);
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
  InitializeCriticalSectionAndSpinCount(mut, 50);
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
