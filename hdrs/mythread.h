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
#define THREAD_RETURN return NULL

#elif defined(WIN32)

/* Wrappers for Win32 threads. */

#include <Windows.h>

typedef unsigned THREAD_RETURN_TYPE;
typedef unsigned (__stdcall *thread_func)(void *);
typedef HANDLE thread_id;
typedef CRITICAL_SECTION penn_mutex;
#define THREAD_RETURN return 0

#else

#error "No thread support"

#endif

int thread_init(void);
int thread_cleanup(void);
int run_thread(thread_id *, thread_func, void *, bool);
void exit_thread(THREAD_RETURN_TYPE);
int join_thread(thread_id, THREAD_RETURN_TYPE *);

/* Mutexes are recursive */
int mutex_init(penn_mutex *);
int mutex_destroy(penn_mutex *);
int mutex_lock(penn_mutex *);
int mutex_unlock(penn_mutex *);

/* Atomic ints */

#if defined(__STDC_NO_ATOMICS__) || defined(_MSC_VER)

#ifdef _MSC_VER

/* Windows MSVC++ intrisics */
#include <intrin.h>
typedef long atomic_long;
#define atomic_fetch_add(lptr, num) _InterlockedExchangeAdd(lptr, num)
#define atomic_load(lptr) *(lptr)

#else
#error "No atomic type support"
#endif

#elif defined(HAVE_STDATOMIC_H)
/* C11 atomics */
#include <stdatomic.h>
#else
#error "No atomic type support"
#endif
