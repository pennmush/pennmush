#pragma once

/** \file mythread.h
 *
 * \brief Wrapper for OS-specific thread routines and atomic types.
 *
 * Note: Can't use C11 threads due to lack of support.
 */

#if defined(HAVE_PTHREADS)

/* Wrappers for pthread routines */

#include <pthread.h>

typedef void * THREAD_RETURN_TYPE;
typedef void *(*thread_func)(void *);
typedef pthread_t thread_id;
typedef pthread_mutex_t penn_mutex;
#define THREAD_RETURN \
  do { mark_finished(pthread_self()); return NULL; } while (0)
typedef pthread_key_t thread_local_id;

#elif defined(WIN32)

/* Wrappers for Win32 threads. */

#include <Windows.h>

typedef unsigned THREAD_RETURN_TYPE;
typedef unsigned (__stdcall *thread_func)(void *);
typedef HANDLE thread_id;
typedef CRITICAL_SECTION penn_mutex;
#define THREAD_RETURN                                                   \
  do { mark_finished(GetCurrentThread())); return 0; } while (0)
typedef DWORD thread_local_id;

#else

#error "No thread support"

#endif

void thread_init(void);
void thread_cleanup(void);
int run_thread(thread_id *, thread_func, void *, bool);
void exit_thread(THREAD_RETURN_TYPE);
int join_thread(thread_id, THREAD_RETURN_TYPE *);
void mark_finished(thread_id);
int reap_threads(void);

/* Mutexes are recursive if second argument is true. (Win32 ones
 * always are).  Try not to use them. */
int mutex_init(penn_mutex *, bool);
int mutex_destroy(penn_mutex *);
int mutex_lock(penn_mutex *);
int mutex_unlock(penn_mutex *);

/* Thread local storage */
int tl_create(thread_local_id *, void (*)(void *));
int tl_destroy(thread_local_id);
void * tl_get(thread_local_id);
int tl_set(thread_local_id, void *);

/* Atomic ints */

#if defined(__STDC_NO_ATOMICS__) || defined(_MSC_VER)

#if defined(_MSC_VER)

/* Windows MSVC++ functions */
typedef long atomic_int;
typedef LONGLONG atomic_int_fast64_t;
#define atomic_fetch_add(lptr, num) InterlockedExchangeAdd(lptr, num)
#define atomic_fetch_add64(llptr, num) InterlockedExchangeAdd64(llptr, num)
#define atomic_increment(lptr) InterlockedIncrement(lptr)
#define atomic_decrement(lptr) InterlockedDecrement(lptr)
#define atomic_load(lptr) *(lptr)

#elif defined(__GNUC__)

/* Legacy GCC intrinsics */

typedef int atomic_int;
typedef int_fast64_t atomic_int_fast64_t;
#define atomic_fetch_add(lptr, num) __sync_fetch_and_add(lptr, num)
#define atomic_fetch_add64(lptr, num) atomic_fetch_add(lptr, num)
#define atomic_increment(lptr) __sync_fetch_and_add(lptr, 1)
#define atomic_decrement(lptr) __sync_fetch_and_sub(lptr, 1)
#define atomic_load(lptr) *(lptr)

#else
#error "No atomic type support"
#endif

#elif defined(HAVE_STDATOMIC_H)
/* C11 atomics */
#include <stdatomic.h>

#define atomic_fetch_add64(ptr, n) atomic_fetch_add(ptr, n)
#define atomic_increment(lptr) atomic_fetch_add(lptr, 1)
#define atomic_decrement(lptr) atomic_fetch_sub(lptr, 1)

#elif defined(__GNUC__)

/* Legacy GCC intrinsics */

typedef int atomic_int;
typedef int_fast64_t atomic_int_fast64_t;
#define atomic_fetch_add(lptr, num) __sync_fetch_and_add(lptr, num)
#define atomic_fetch_add64(lptr, num) atomic_fetch_add(lptr, num)
#define atomic_increment(lptr) __sync_fetch_and_add(lptr, 1)
#define atomic_decrement(lptr) __sync_fetch_and_sub(lptr, 1)
#define atomic_load(lptr) *(lptr)

#else
#error "No atomic type support"
#endif

