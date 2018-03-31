/** \file mythread.c
 *
 * \brief Implementations of OS-independent thread functions
 */

#include <stdlib.h>
#include "mythread.h"
#include "confmagic.h"

struct thread_list {
  thread_id id;
  bool finished;
  struct thread_list *next;
};

penn_mutex thread_mutex;
/* List of non-joinable threads. They get joined anyways to keep helgrind happy. */
struct thread_list *running_threads = NULL;

extern penn_mutex desc_mutex;
extern penn_mutex queue_mutex;
extern penn_mutex sql_mutex;
extern penn_mutex site_mutex;
extern penn_mutex mem_mutex;
extern penn_mutex od_mutex;
extern penn_mutex pe_mutex;
extern penn_mutex log_mutex;

extern thread_local_id su_id;
extern thread_local_id tp_id;
extern thread_local_id rng_id;

static void add_to_list(thread_id id)
{
  struct thread_list *t = malloc(sizeof *t);
  t->id = id;
  t->finished = 0;
  mutex_lock(&thread_mutex);
  t->next = running_threads;
  running_threads = t;
  mutex_unlock(&thread_mutex);
}

/** Mark a thread as done and ready to be cleaned up. */
void
mark_finished(thread_id id)
{
  struct thread_list *t;
  mutex_lock(&thread_mutex);
  for (t = running_threads; t; t = t->next) {
    if (t->id == id) {
      t->finished = 1;
      break;
    }
  }
  mutex_unlock(&thread_mutex);
}

/** Clean up all pending finished threads */
int
reap_threads(void)
{
  struct thread_list *t, *n, *p = NULL;
  int count = 0;
  mutex_lock(&thread_mutex);
  for (t = running_threads; t; t = n) {
    n = t->next;
    if (t->finished) {
      THREAD_RETURN_TYPE r;
      if (join_thread(t->id, &r) == 0) {
        count += 1;
        free(t);
        if (p) {
          p->next = n;
        } else {
          running_threads = n;
        }
      } else {
        p = t;
      }
    } else {
      p = t;
    }
  }
  mutex_unlock(&thread_mutex);
  return count;
}

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
  mutex_init(&thread_mutex, 0);
  mutex_init(&desc_mutex, 1);
  mutex_init(&queue_mutex, 1);
  mutex_init(&sql_mutex, 1);
  mutex_init(&site_mutex, 0);
  mutex_init(&mem_mutex, 0);
  mutex_init(&od_mutex, 0);
  mutex_init(&pe_mutex, 0);
  mutex_init(&log_mutex, 0);
  tl_create(&su_id, free);
  tl_create(&tp_id, free);
  tl_create(&rng_id, free);
}

/** Destroy global thread-related variables and deinitialize thread
 *  system.  Should only be called once at end of program.
 */
void
thread_cleanup(void)
{
  mutex_destroy(&thread_mutex);
  mutex_destroy(&desc_mutex);
  mutex_destroy(&queue_mutex);
  mutex_destroy(&sql_mutex);
  mutex_destroy(&site_mutex);
  mutex_destroy(&mem_mutex);
  mutex_destroy(&od_mutex);
  mutex_destroy(&pe_mutex);
  mutex_destroy(&log_mutex);
  tl_destroy(su_id);
  tl_destroy(tp_id);
  tl_destroy(rng_id);
#ifdef HAVE_PTHREADS
  dest_pthreads();
#endif
}

#ifdef HAVE_PTHREADS
/* Pthreads wrappers */

static pthread_mutexattr_t recattr; /**< mutex attr for recursive */

static void
init_pthreads(void)
{
  pthread_mutexattr_init(&recattr);
  pthread_mutexattr_settype(&recattr, PTHREAD_MUTEX_RECURSIVE);
}

static void
dest_pthreads(void)
{
  pthread_mutexattr_destroy(&recattr);
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
  int r = pthread_create(id, NULL, f, arg);
  if (r == 0 && detach) {
    add_to_list(*id);
  }
  return r;
}

/** Called by a thread function to exit the thread.
 *
 * \param retval the return value.
 */
void
exit_thread(THREAD_RETURN_TYPE retval)
{
  mark_finished(pthread_self());
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

/** Creat a new thread local storage key.
 *
 * \param key pointer to the key variable.
 * \param free_fun function to call on stored value when thread exits.
 * \return 0 on success, other on error.
 */
int
tl_create(thread_local_id *key, void (*free_fun)(void *))
{
  return pthread_key_create(key, free_fun);
}

/** Destroy a thread local storage key.
 *
 * \param key tls key to clean up.
 * \return 0 on success, other on error.
 */
int
tl_destroy(thread_local_id key)
{
  return pthread_key_delete(key);
}

/** Return the value associated with the current thread's
 * tls key.
 *
 * \param key to return value for.
 * \return pointer to value, or NULL.
 */
void *
tl_get(thread_local_id key)
{
  return pthread_getspecific(key);
}

/** Associate a pointer with the current thread's tls key.
 *
 * \param key key to store value for.
 * \param data pointer to store.
 * \return 0 for success, other on error.
 */
int
tl_set(thread_local_id key, void *data)
{
  return pthread_setspecific(key, data);
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
      add_to_list(*id);
    }
    return 0;
  }
}

void
exit_thread(THREAD_RETURN_TYPE retval)
{
  mark_finished(GetCurrentThread());
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

int
tl_create(thread_local_id *key, void (*free_fun)(void *))
{
  *key = FlsAlloc(free_fun);
  if (*key == FLS_OUT_OF_INDEXES) {
    return -1;
  } else {
    return 0;
  }
}

int
tl_destroy(thread_local_id key)
{
  return !FlsFree(key);
}

void *
tl_get(thread_local_id key)
{
  return FlsGetValue(key);
}

int
tl_set(thread_local_id key, void *data)
{
  return !FlsSetValue(key, data);
}


#endif
