/**
 * \file timer.c
 *
 * \brief Timed events in PennMUSH.
 *
 *
 */
#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#ifdef I_UNISTD
#include <unistd.h>
#endif

#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "lock.h"
#include "extmail.h"
#include "match.h"
#include "flags.h"
#include "access.h"
#include "log.h"
#include "game.h"
#include "help.h"
#include "parse.h"
#include "attrib.h"
#include "confmagic.h"


static sig_atomic_t hup_triggered = 0;
static sig_atomic_t usr1_triggered = 0;

extern void inactivity_check(void);
extern void reopen_logs(void);
static void migrate_stuff(int amount);

#ifndef WIN32
void hup_handler(int);
void usr1_handler(int);

#endif
void dispatch(void);

#ifndef WIN32

/** Handler for HUP signal.
 * Do the minimal work here - set a global variable and reload the handler.
 * \param x unused.
 */
void
hup_handler(int x __attribute__ ((__unused__)))
{
  hup_triggered = 1;
  reload_sig_handler(SIGHUP, hup_handler);
}

/** Handler for USR1 signal.
 * Do the minimal work here - set a global variable and reload the handler.
 * \param x unused.
 */
void
usr1_handler(int x __attribute__ ((__unused__)))
{
  usr1_triggered = 1;
  reload_sig_handler(SIGUSR1, usr1_handler);
}

#endif                          /* WIN32 */

/** Set up signal handlers.
 */
void
init_timer(void)
{
#ifndef WIN32
  install_sig_handler(SIGHUP, hup_handler);
  install_sig_handler(SIGUSR1, usr1_handler);
#endif
#ifndef PROFILING
#ifdef HAS_ITIMER
#ifdef __CYGWIN__
  install_sig_handler(SIGALRM, signal_cpu_limit);
#else
  install_sig_handler(SIGPROF, signal_cpu_limit);
#endif
#endif
#endif
}

/** Migrate some number of chunks.
 * The requested amount is only a guideline; the actual amount
 * migrated will be more or less due to always migrating all the
 * attributes, locks, and mail on any given object together.
 * \param amount the suggested number of attributes to migrate.
 */
static void
migrate_stuff(int amount)
{
  static int start_obj = 0;
  static chunk_reference_t **refs = NULL;
  static int refs_size = 0;
  int end_obj;
  int actual;
  ATTR *aptr;
  lock_list *lptr;
  MAIL *mp;

  if (db_top == 0)
    return;

  end_obj = start_obj;
  actual = 0;
  do {
    for (aptr = List(end_obj); aptr; aptr = AL_NEXT(aptr))
      if (aptr->data != NULL_CHUNK_REFERENCE)
        actual++;
    for (lptr = Locks(end_obj); lptr; lptr = L_NEXT(lptr))
      if (L_KEY(lptr) != NULL_CHUNK_REFERENCE)
        actual++;
    if (IsPlayer(end_obj)) {
      for (mp = find_exact_starting_point(end_obj); mp; mp = mp->next)
        if (mp->msgid != NULL_CHUNK_REFERENCE)
          actual++;
    }
    end_obj = (end_obj + 1) % db_top;
  } while (actual < amount && end_obj != start_obj);

  if (actual == 0)
    return;

  if (!refs || actual > refs_size) {
    if (refs)
      mush_free(refs, "migration reference array");
    refs =
      mush_calloc(actual, sizeof(chunk_reference_t *),
                  "migration reference array");
    refs_size = actual;
    if (!refs)
      mush_panic("Could not allocate migration reference array");
  }
#ifdef DEBUG_MIGRATE
  do_rawlog(LT_TRACE, "Migrate asked %d, actual objects #%d to #%d for %d",
            amount, start_obj, (end_obj + db_top - 1) % db_top, actual);
#endif

  actual = 0;
  do {
    for (aptr = List(start_obj); aptr; aptr = AL_NEXT(aptr))
      if (aptr->data != NULL_CHUNK_REFERENCE) {
        refs[actual] = &(aptr->data);
        actual++;
      }
    for (lptr = Locks(start_obj); lptr; lptr = L_NEXT(lptr))
      if (L_KEY(lptr) != NULL_CHUNK_REFERENCE) {
        refs[actual] = &(lptr->key);
        actual++;
      }
    if (IsPlayer(start_obj)) {
      for (mp = find_exact_starting_point(start_obj); mp; mp = mp->next)
        if (mp->msgid != NULL_CHUNK_REFERENCE) {
          refs[actual] = &(mp->msgid);
          actual++;
        }
    }
    start_obj = (start_obj + 1) % db_top;
  } while (start_obj != end_obj);

  chunk_migration(actual, refs);
}

static void
idle_event(void *data __attribute__((__unused__)))
{
  inactivity_check();
  sq_register_in(60, idle_event, NULL, "PLAYER`INACTIVITY");
}

static void
purge_event(void *data __attribute__((__unused__)))
{
  global_eval_context.cplr = NOTHING;
  strcpy(global_eval_context.ccom, "purge");
  purge();
  strcpy(global_eval_context.ccom, "");
  options.purge_counter = mudtime + PURGE_INTERVAL;
  sq_register_in(PURGE_INTERVAL, purge_event, NULL, "DB`PURGE");
}

static void
dbck_event(void *data __attribute__((__unused__)))
{
  global_eval_context.cplr = NOTHING;
  strcpy(global_eval_context.ccom, "dbck");
  dbck();
  strcpy(global_eval_context.ccom, "");
  options.dbck_counter = mudtime + DBCK_INTERVAL;
  sq_register_in(DBCK_INTERVAL, dbck_event, NULL, "DB`DBCK");
}

static void
warning_event(void *data __attribute__((__unused__)))
{
  options.warn_counter = options.warn_interval + mudtime;
  strcpy(global_eval_context.ccom, "warnings");
  run_topology();
  strcpy(global_eval_context.ccom, "");
  sq_register_in(options.warn_interval, warning_event, NULL, "DB`WCHECK");
}

/** Handle events that may need handling.
 * This routine is polled from bsd.c. At any call, it can handle
 * the HUP and USR1 signals. At calls that are 'on the second',
 * it goes on to perform regular every-second processing and to
 * check whether it's time to do other periodic processes like
 * purge, dump, or inactivity checks.
 */
static void
on_every_second(void *data __attribute__((__unused__)))
{

  /* A HUP reloads configuration and reopens logs */
  if (hup_triggered) {
    do_rawlog(LT_ERR, "SIGHUP received: reloading .txt and .cnf files");
    config_file_startup(NULL, 0);
    config_file_startup(NULL, 1);
    fcache_load(NOTHING);
    help_reindex(NOTHING);
    read_access_file();
    reopen_logs();
    hup_triggered = 0;
  }
  /* A USR1 does a shutdown/reboot */
  if (usr1_triggered) {
    if (!queue_event(SYSEVENT, "SIGNAL`USR1", "%s", "")) {
      do_rawlog(LT_ERR, "SIGUSR1 received. Rebooting.");
      do_reboot(NOTHING, 0);      /* We don't return from this */
    }
    usr1_triggered = 0;         /* But just in case */
  }

  /*
  if (!globals.on_second)
    return;
  globals.on_second = 0;
  */

  mudtime = time(NULL);

  do_second();
  migrate_stuff(CHUNK_MIGRATE_AMOUNT);

  /* TO-DO: Move into separate events. */
  /* Database dump routines */
  if (options.dump_counter <= mudtime) {
    log_mem_check();
    options.dump_counter = options.dump_interval + mudtime;
    strcpy(global_eval_context.ccom, "dump");
    fork_and_dump(1);
    strcpy(global_eval_context.ccom, "");
    flag_broadcast(0, "ON-VACATION", "%s",
                   T
                   ("Your ON-VACATION flag is set! If you're back, clear it."));
  } else if (options.dump_counter - 60 == mudtime) {
    queue_event(SYSEVENT, "DUMP`1MIN", "%s,%d",
                options.dump_warning_1min, NO_FORK ? 0 : 1);
    if (NO_FORK && *options.dump_warning_1min) {
      flag_broadcast(0, 0, "%s", options.dump_warning_1min);
    }
  } else if (options.dump_counter - 300 == mudtime) {
    queue_event(SYSEVENT, "DUMP`5MIN", "%s,%d",
                options.dump_warning_5min, NO_FORK ? 0 : 1);
    if (NO_FORK && *options.dump_warning_5min) {
      flag_broadcast(0, 0, "%s", options.dump_warning_5min);
    }
  }
  sq_register_in(1, on_every_second, NULL, NULL);
}

void
init_sys_events(void) {
  time(&mudtime);
  sq_register(mudtime + 60, idle_event, NULL, "PLAYER`INACTIVITY");
  sq_register(mudtime + DBCK_INTERVAL, dbck_event, NULL, "DB`DBCK");
  sq_register(mudtime + PURGE_INTERVAL, purge_event, NULL, "DB`PURGE");
  sq_register(mudtime + options.warn_interval, warning_event, NULL, "DB`WCHECK");
  sq_register(mudtime, on_every_second, NULL, NULL); 
}

sig_atomic_t cpu_time_limit_hit = 0;  /** Was the cpu time limit hit? */
int cpu_limit_warning_sent = 0;  /** Have we issued a cpu limit warning? */

#ifndef PROFILING
#if defined(HAS_ITIMER)
/** Handler for PROF signal.
 * Do the minimal work here - set a global variable and reload the handler.
 * \param signo unused.
 */
void
signal_cpu_limit(int signo __attribute__ ((__unused__)))
{
  cpu_time_limit_hit = 1;
#ifdef __CYGWIN__
  reload_sig_handler(SIGALRM, signal_cpu_limit);
#else
  reload_sig_handler(SIGPROF, signal_cpu_limit);
#endif
}
#elif defined(WIN32)
#if _MSC_VER <= 1100 && !defined(UINT_PTR)
#define UINT_PTR UINT
#endif
UINT_PTR timer_id;
VOID CALLBACK
win32_timer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
  cpu_time_limit_hit = 1;
}
#endif
#endif
int timer_set = 0;      /**< Is a CPU timer set? */

/** Start the cpu timer (before running a command).
 */
void
start_cpu_timer(void)
{
#ifndef PROFILING
  cpu_time_limit_hit = 0;
  cpu_limit_warning_sent = 0;
  timer_set = 1;
#if defined(HAS_ITIMER)         /* UNIX way */
  {
    struct itimerval time_limit;
    if (options.queue_entry_cpu_time > 0) {
      ldiv_t t;
      /* Convert from milliseconds */
      t = ldiv(options.queue_entry_cpu_time, 1000);
      time_limit.it_value.tv_sec = t.quot;
      time_limit.it_value.tv_usec = t.rem * 1000;
      time_limit.it_interval.tv_sec = 0;
      time_limit.it_interval.tv_usec = 0;
#ifdef __CYGWIN__
      if (setitimer(ITIMER_REAL, &time_limit, NULL)) {
#else
      if (setitimer(ITIMER_PROF, &time_limit, NULL)) {
#endif                          /* __CYGWIN__ */
        penn_perror("setitimer");
        timer_set = 0;
      }
    } else
      timer_set = 0;
  }
#elif defined(WIN32)            /* Windoze way */
  if (options.queue_entry_cpu_time > 0)
    timer_id = SetTimer(NULL, 0, (unsigned) options.queue_entry_cpu_time,
                        (TIMERPROC) win32_timer);
  else
    timer_set = 0;
#endif                          /* HAS_ITIMER / WIN32 */
#endif                          /* PROFILING */
}

/** Reset the cpu timer (after running a command).
 */
void
reset_cpu_timer(void)
{
#ifndef PROFILING
  if (timer_set) {
#if defined(HAS_ITIMER)
    struct itimerval time_limit, time_left;
    time_limit.it_value.tv_sec = 0;
    time_limit.it_value.tv_usec = 0;
    time_limit.it_interval.tv_sec = 0;
    time_limit.it_interval.tv_usec = 0;
#ifdef __CYGWIN__
    if (setitimer(ITIMER_REAL, &time_limit, &time_left))
#else
    if (setitimer(ITIMER_PROF, &time_limit, &time_left))
#endif                          /* __CYGWIN__ */
      penn_perror("setitimer");
#elif defined(WIN32)
    KillTimer(NULL, timer_id);
#endif                          /* HAS_ITIMER / WIN32 */
  }
  cpu_time_limit_hit = 0;
  cpu_limit_warning_sent = 0;
  timer_set = 0;
#endif                          /* PROFILING */
}

 
/** System queue stuff. Timed events like dbcks and purges are handled
 *  through this system. */
 
struct squeue {
   sq_func fun;
   void *data;
  time_t when;
  char *event;
  struct squeue *next;
};

struct squeue *sq_head = NULL;

/** Register a callback function to be executed at a certain time.
 *  \param w when to run the event
 *  \param f the callback function
 *  \param d data to pass to the callback
 *  \param ev Softcode event to trigger at the same time.
 */
void
sq_register(time_t w, sq_func f, void *d, const char *ev) {
  struct squeue *sq;

  sq = mush_malloc(sizeof *sq, "squeue.node");

  sq->when = w;
  sq->fun = f;
  sq->data = d;
  if (ev)
    sq->event = mush_strdup(strupper(ev), "squeue.event");
  else
    sq->event = NULL;
  sq->next = NULL;
  
  if (!sq_head)
    sq_head = sq;
  else if (difftime(w, sq_head->when) <= 0) {
    sq->next = sq_head;
    sq_head = sq;
  } else {
    struct squeue *c, *prev = NULL;
    for (prev = sq_head, c = sq_head->next;
	 c;
	 prev = c, c = c->next) {      
      if (difftime(w, c->when) <= 0) {
	sq->next = c;
	prev->next = sq;
	return;	
      }
    }
    prev->next = sq;
  }
}

/** Register a callback function to be executed in N seconds.
 * \param n the number of seconds to run the callback after.
 * \param f the callback function.
 * \param d data to pass to the callback.
 * \param ev softcode event to trigger at the same time.
 */
void
sq_register_in(int n, sq_func f, void *d, const char *ev)
{
  time_t now;
  time(&now);
  sq_register(now + n, f, d, ev);
}

/** Execute a single pending system queue event.
 * \return true if work was done, false otherwise.
 */
bool
sq_run_one(void)
{
  time_t now;
  struct squeue *n;

  time(&now);

  if (sq_head) {
    if (difftime(sq_head->when, now) <= 0) {      
      sq_head->fun(sq_head->data);
      if (sq_head->event) {
	queue_event(SYSEVENT, sq_head->event, "%s", "");
	mush_free(sq_head->event, "squeue.event");
      }
      n = sq_head->next;
      mush_free(sq_head, "squeue.node");
      sq_head = n;
      return true;
    }
  }
  return false;
}

/** Run all pending system queue events.
 * \return true if work was done, false otherwise.
 */
bool
sq_run_all(void)
{
  bool r, any = false;

  do {
    r = sq_run_one();
    if (r)
      any = true;
  } while (r);
  return any;
}
