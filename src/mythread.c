/** \file mythread.c
 *
 * \brief Implementations of OS-independent thread functions
 */

#include <stdlib.h>
#include "mythread.h"
#include "confmagic.h"

extern penn_mutex desc_mutex;
extern penn_mutex queue_mutex;
extern penn_mutex sql_mutex;
extern penn_mutex site_mutex;
extern penn_mutex mem_mutex;

#ifdef HAVE_PTHREADS
static void init_pthreads(void);
static void dest_pthreads(void);
#endif

/** Initialize thread system and global thread-related variables.
 *  Should only be called once at start of program.
*/
void
thread_init(void)
{
#ifdef HAVE_PTHREADS
  init_pthreads();
#endif
  mutex_init(&desc_mutex, 0);
  mutex_init(&queue_mutex, 1);
  mutex_init(&sql_mutex, 1);
  mutex_init(&site_mutex, 0);
  mutex_init(&mem_mutex, 0);
}

/** Destroy global thread-related variables and deinitialize thread
 *  system.  Should only be called once at end of program.
 */
void
thread_cleanup(void)
{
  mutex_destroy(&desc_mutex);
  mutex_destroy(&queue_mutex);
  mutex_destroy(&sql_mutex);
  mutex_destroy(&site_mutex);
  mutex_destroy(&mem_mutex);
#ifdef HAVE_PTHREADS
  dest_pthreads();
#endif
}

#ifdef HAVE_PTHREADS
/* Pthreads wrappers */

static pthread_mutexattr_t recattr; /**< mutex attr for recursive mutexes */
static pthread_attr_t detached; /**< thread attr for starting detached */

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

/** Launch a new thread.
 *
 * \param id pointer to the thread id (For later joining)
 * \param f function to run in new thread.
 * \param arg argument to pass to the function.
 * \param detach true to start out detached.
 * \return 0 on success, other value on error.
 */
int
run_thread(thread_id *id, thread_func f, void *arg, bool detach) {
  pthread_attr_t *attr = detach ? &detached : NULL;
  return pthread_create(id, attr, f, arg);
}

/** Called by a thread function to exit the thread.
 *
 * \param retval the return value.
 */
void
exit_thread(THREAD_RETURN_TYPE retval)
{
  pthread_exit(retval);
}

/** Wait for a given thread to exit and capture its return value.
 *
 * \param id the thread to wait for.
 * \param retval where to store the return value.
 * \return 0 on success, other value on error.
 */
int
join_thread(thread_id id, THREAD_RETURN_TYPE *retval)
{
  return pthread_join(id, retval);
}

/** Initialize a mutex.
 *
 * \param mut the mutex to initialize.
 * \param recur true if the mutex should be recursive.
 * \return 0 on success, other value on error.
 */
int
mutex_init(penn_mutex *mut, bool recur)
{
  return pthread_mutex_init(mut, recur ? &recattr : NULL);
}

/** Destroy a mutex. It shouldn't be used after being destroyed.
 *
 * \param mut the mutex to destroy.
 * \return 0 on success, other value on error.
 */
int
mutex_destroy(penn_mutex *mut)
{
  return pthread_mutex_destroy(mut);
}

/** Lock a mutex. Will block until acquired.
 *
 * \param mut the mutex to lock.
 * \return 0 on success, other value on error.
 */
int
mutex_lock(penn_mutex *mut)
{
  return pthread_mutex_lock(mut);
}

/** Unlock a locked mutex.
 *
 * \param mut the mutex to unlock.
 * \return 0 on success, other value on error.
 */
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
    CloseHandle(id);
    return -1;
  }
  *retval = exitcode;
  CloseHandle(id);
  return 0;
}

int
mutex_init(penn_mutex *mut, bool recur __attribute__((__unused__)))
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
