/**
 * \file game.c
 *
 * \brief The main game driver.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#ifdef WIN32
#include <process.h>
#include <windows.h>
#undef OPAQUE                   /* Clashes with flags.h */
void Win32MUSH_setup(void);
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <errno.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "game.h"
#include "attrib.h"
#include "match.h"
#include "case.h"
#include "extmail.h"
#include "extchat.h"
#ifdef HAS_OPENSSL
#include "myssl.h"
#endif
#include "getpgsiz.h"
#include "parse.h"
#include "access.h"
#include "version.h"
#include "strtree.h"
#include "command.h"
#include "htab.h"
#include "ptab.h"
#include "intmap.h"
#include "log.h"
#include "lock.h"
#include "dbdefs.h"
#include "flags.h"
#include "function.h"
#include "help.h"
#include "dbio.h"
#include "mypcre.h"
#ifndef WIN32
#include "wait.h"
#endif
#include "ansi.h"
#include "mymalloc.h"
#ifdef hpux
#include <sys/syscall.h>
#define getrusage(x,p)   syscall(SYS_GETRUSAGE,x,p)
#endif                          /* fix to HP-UX getrusage() braindamage */

#include "confmagic.h"

/* declarations */
GLOBALTAB globals = { 0, "", 0, 0, 0, 0, 0, 0, 0, 0 };

static int epoch = 0;
static int reserved;                    /**< Reserved file descriptor */
static dbref *errdblist = NULL; /**< List of dbrefs to return errors from */
static dbref *errdbtail = NULL;        /**< Pointer to end of errdblist */
#define ERRDB_INITIAL_SIZE 5
static int errdbsize = ERRDB_INITIAL_SIZE; /**< Current size of errdblist array */
static void errdb_grow(void);


extern void initialize_mt(void);
extern const unsigned char *tables;
extern void conf_default_set(void);
static bool dump_database_internal(void);
static PENNFILE *db_open(const char *);
static PENNFILE *db_open_write(const char *);
static int fail_commands(dbref player);
void do_readcache(dbref player);
int check_alias(const char *command, const char *list);
static int list_check(dbref thing, dbref player, char type,
                      char end, char *str, int just_match);
int alias_list_check(dbref thing, const char *command, const char *type);
int loc_alias_check(dbref loc, const char *command, const char *type);
void do_poor(dbref player, char *arg1);
void do_writelog(dbref player, char *str, int ltype);
void bind_and_queue(dbref player, dbref cause, char *action, const char *arg,
                    const char *placestr);
void do_scan(dbref player, char *command, int flag);
void do_list(dbref player, char *arg, int lc);
void do_dolist(dbref player, char *list, char *command,
               dbref cause, unsigned int flags);
void do_uptime(dbref player, int mortal);
static char *make_new_epoch_file(const char *basename, int the_epoch);
#ifdef HAS_GETRUSAGE
void rusage_stats(void);
#endif

void do_list_memstats(dbref player);
void do_list_allocations(dbref player);
void st_stats_header(dbref player);
void st_stats(dbref player, StrTree *root, const char *name);
void do_timestring(char *buff, char **bp, const char *format,
                   unsigned long secs);

extern void create_minimal_db(void);    /* From db.c */

dbref orator = NOTHING;  /**< Last dbref to issue a speech command */

#ifdef COMP_STATS
extern void compress_stats(long *entries,
                           long *mem_used,
                           long *total_uncompressed, long *total_compressed);
#endif


pid_t forked_dump_pid = -1;

/** Open /dev/null to reserve a file descriptor that can be reused later. */
void
reserve_fd(void)
{
#ifndef WIN32
  reserved = open("/dev/null", O_RDWR);
#endif
}

/** Release the reserved file descriptor for other use. */
void
release_fd(void)
{
#ifndef WIN32
  close(reserved);
#endif
}

/** User command to dump the database.
 * \verbatim
 * This implements the @dump command.
 * \endverbatim
 * \param player the enactor, for permission checking.
 * \param num checkpoint interval, as a string.
 * \param flag type of dump.
 */
void
do_dump(dbref player, char *num, enum dump_type flag)
{
  if (Wizard(player)) {
#ifdef ALWAYS_PARANOID
    if (1) {
#else
    if (flag != DUMP_NORMAL) {
#endif
      /* want to do a scan before dumping each object */
      globals.paranoid_dump = (flag == DUMP_DEBUG ? 2 : 1);
      if (num && *num) {
        /* checkpoint interval given */
        globals.paranoid_checkpt = atoi(num);
        if ((globals.paranoid_checkpt < 1)
            || (globals.paranoid_checkpt >= db_top)) {
          notify(player, T("Permission denied. Invalid checkpoint interval."));
          globals.paranoid_dump = 0;
          return;
        }
      } else {
        /* use a default interval */
        globals.paranoid_checkpt = db_top / 5;
        if (globals.paranoid_checkpt < 1)
          globals.paranoid_checkpt = 1;
      }
      if (flag == DUMP_PARANOID) {
        notify_format(player, T("Paranoid dumping, checkpoint interval %d."),
                      globals.paranoid_checkpt);
        do_rawlog(LT_CHECK,
                  "*** PARANOID DUMP *** done by %s(#%d),\n",
                  Name(player), player);
      } else {
        notify_format(player, T("Debug dumping, checkpoint interval %d."),
                      globals.paranoid_checkpt);
        do_rawlog(LT_CHECK,
                  "*** DEBUG DUMP *** done by %s(#%d),\n",
                  Name(player), player);
      }
      do_rawlog(LT_CHECK, "\tcheckpoint interval %d, at %s",
                globals.paranoid_checkpt, show_time(mudtime, 0));
    } else {
      /* normal dump */
      globals.paranoid_dump = 0;        /* just to be safe */
      notify(player, T("Dumping..."));
      do_rawlog(LT_CHECK, "** DUMP ** done by %s(#%d) at %s",
                Name(player), player, show_time(mudtime, 0));
    }
    fork_and_dump(1);
    globals.paranoid_dump = 0;
  } else {
    notify(player, T("Sorry, you are in a no dumping zone."));
  }
}


/** Print global variables to the trace log.
 * This function is used for error reporting.
 */
void
report(void)
{
  if (GoodObject(global_eval_context.cplr))
    do_rawlog(LT_TRACE, "TRACE: Cmd:%s\tby #%d at #%d",
              global_eval_context.ccom, global_eval_context.cplr,
              Location(global_eval_context.cplr));
  else
    do_rawlog(LT_TRACE, "TRACE: Cmd:%s\tby #%d", global_eval_context.ccom,
              global_eval_context.cplr);
  notify_activity(NOTHING, 0, 1);
}

#ifdef HAS_GETRUSAGE
/** Log process statistics to the error log.
 */
void
rusage_stats(void)
{
  struct rusage usage;
  int pid;
  int psize;

  pid = getpid();
  psize = getpagesize();
  getrusage(RUSAGE_SELF, &usage);

  do_rawlog(LT_ERR, "Process statistics:");
  do_rawlog(LT_ERR, "Time used:   %10ld user   %10ld sys",
            (long) usage.ru_utime.tv_sec, (long) usage.ru_stime.tv_sec);
  do_rawlog(LT_ERR, "Max res mem: %10ld pages  %10ld bytes",
            usage.ru_maxrss, (usage.ru_maxrss * psize));
  do_rawlog(LT_ERR, "Integral mem:%10ld shared %10ld private %10ld stack",
            usage.ru_ixrss, usage.ru_idrss, usage.ru_isrss);
  do_rawlog(LT_ERR,
            "Page faults: %10ld hard   %10ld soft    %10ld swapouts",
            usage.ru_majflt, usage.ru_minflt, usage.ru_nswap);
  do_rawlog(LT_ERR, "Disk I/O:    %10ld reads  %10ld writes",
            usage.ru_inblock, usage.ru_oublock);
  do_rawlog(LT_ERR, "Network I/O: %10ld in     %10ld out", usage.ru_msgrcv,
            usage.ru_msgsnd);
  do_rawlog(LT_ERR, "Context swi: %10ld vol    %10ld forced",
            usage.ru_nvcsw, usage.ru_nivcsw);
  do_rawlog(LT_ERR, "Signals:     %10ld", usage.ru_nsignals);
}

#endif                          /* HAS_GETRUSAGE */

/** User interface to shut down the MUSH.
 * \verbatim
 * This implements the @shutdown command.
 * \endverbatim
 * \param player the enactor, for permission checking.
 * \param flag type of shutdown to perform.
 */
void
do_shutdown(dbref player, enum shutdown_type flag)
{
  if (flag == SHUT_PANIC && !God(player)) {
    notify(player, T("It takes a God to make me panic."));
    return;
  }
  flag_broadcast(0, 0, T("GAME: Shutdown by %s"), Name(player));
  do_log(LT_ERR, player, NOTHING, "SHUTDOWN by %s(%s)\n",
         Name(player), unparse_dbref(player));

  if (flag == SHUT_PANIC) {
    mush_panic("@shutdown/panic");
  } else {
    if (flag == SHUT_PARANOID) {
      globals.paranoid_checkpt = db_top / 5;
      if (globals.paranoid_checkpt < 1)
        globals.paranoid_checkpt = 1;
      globals.paranoid_dump = 1;
    }
    shutdown_flag = 1;
  }
}

jmp_buf db_err;

static bool
dump_database_internal(void)
{
  char realdumpfile[2048];
  char realtmpfl[2048];
  char tmpfl[2048];
  PENNFILE *f = NULL;

#ifndef PROFILING
#ifndef WIN32
  ignore_signal(SIGPROF);
#endif
#endif

  if (setjmp(db_err)) {
    /* The dump failed. Disk might be full or something went bad with the
       compression slave. Boo! */
    do_rawlog(LT_ERR, "ERROR! Database save failed.");
    flag_broadcast("WIZARD ROYALTY", 0,
                   T("GAME: ERROR! Database save failed!"));
    if (f)
      penn_fclose(f);
#ifndef PROFILING
#ifdef HAS_ITIMER
    install_sig_handler(SIGPROF, signal_cpu_limit);
#endif
#endif
    return false;
  } else {
    local_dump_database();

#ifdef ALWAYS_PARANOID
    globals.paranoid_checkpt = db_top / 5;
    if (globals.paranoid_checkpt < 1)
      globals.paranoid_checkpt = 1;
#endif

    sprintf(realdumpfile, "%s%s", globals.dumpfile, options.compresssuff);
    strcpy(tmpfl, make_new_epoch_file(globals.dumpfile, epoch));
    sprintf(realtmpfl, "%s%s", tmpfl, options.compresssuff);

    if ((f = db_open_write(tmpfl)) != NULL) {
      switch (globals.paranoid_dump) {
      case 0:
#ifdef ALWAYS_PARANOID
        db_paranoid_write(f, 0);
#else
        db_write(f, 0);
#endif
        break;
      case 1:
        db_paranoid_write(f, 0);
        break;
      case 2:
        db_paranoid_write(f, 1);
        break;
      }
      penn_fclose(f);
      if (rename_file(realtmpfl, realdumpfile) < 0) {
        penn_perror(realtmpfl);
        longjmp(db_err, 1);
      }
    } else {
      penn_perror(realtmpfl);
      longjmp(db_err, 1);
    }
    sprintf(realdumpfile, "%s%s", options.mail_db, options.compresssuff);
    strcpy(tmpfl, make_new_epoch_file(options.mail_db, epoch));
    sprintf(realtmpfl, "%s%s", tmpfl, options.compresssuff);
    if (mdb_top >= 0) {
      if ((f = db_open_write(tmpfl)) != NULL) {
        dump_mail(f);
        penn_fclose(f);
        if (rename_file(realtmpfl, realdumpfile) < 0) {
          penn_perror(realtmpfl);
          longjmp(db_err, 1);
        }
      } else {
        penn_perror(realtmpfl);
        longjmp(db_err, 1);
      }
    }
    sprintf(realdumpfile, "%s%s", options.chatdb, options.compresssuff);
    strcpy(tmpfl, make_new_epoch_file(options.chatdb, epoch));
    sprintf(realtmpfl, "%s%s", tmpfl, options.compresssuff);
    if ((f = db_open_write(tmpfl)) != NULL) {
      save_chatdb(f);
      penn_fclose(f);
      if (rename_file(realtmpfl, realdumpfile) < 0) {
        penn_perror(realtmpfl);
        longjmp(db_err, 1);
      }
    } else {
      penn_perror(realtmpfl);
      longjmp(db_err, 1);
    }
    time(&globals.last_dump_time);
  }

#ifndef PROFILING
#ifdef HAS_ITIMER
  install_sig_handler(SIGPROF, signal_cpu_limit);
#endif
#endif

  return true;
}

/** Crash gracefully.
 * This function is called when something disastrous happens - typically
 * a failure to malloc memory or a signal like segfault.
 * It logs the fault, does its best to dump a panic database, and
 * exits abruptly. This function does not return.
 * \param message message to log to the error log.
 */
void
mush_panic(const char *message)
{
  const char *panicfile = options.crash_db;
  PENNFILE *f = NULL;
  static int already_panicking = 0;

  if (already_panicking) {
    do_rawlog(LT_ERR,
              "PANIC: Attempted to panic because of '%s' while already panicking. Run in circles, scream and shout!",
              message);
    abort();
  }

  already_panicking = 1;
  do_rawlog(LT_ERR, "PANIC: %s", message);
  report();
  flag_broadcast(0, 0, T("EMERGENCY SHUTDOWN: %s"), message);

  /* turn off signals */
  block_signals();

  /* shut down interface */
  emergency_shutdown();

  /* dump panic file if we have a database read. */
  if (globals.database_loaded) {
    if (setjmp(db_err)) {
      /* Dump failed. We're in deep doo-doo */
      do_rawlog(LT_ERR, "CANNOT DUMP PANIC DB. OOPS.");
      abort();
    } else {
      if ((f = penn_fopen(panicfile, FOPEN_WRITE)) == NULL) {
        do_rawlog(LT_ERR, "CANNOT OPEN PANIC FILE, YOU LOSE");
        _exit(135);
      } else {
        do_rawlog(LT_ERR, "DUMPING: %s", panicfile);
        db_write(f, DBF_PANIC);
        dump_mail(f);
        save_chatdb(f);
        penn_fclose(f);
        do_rawlog(LT_ERR, "DUMPING: %s (done)", panicfile);
      }
    }
  } else {
    do_rawlog(LT_ERR, "Skipping panic dump because database isn't loaded.");
  }
  abort();
}

/** Crash gracefully.
 * Calls mush_panic() with its arguments formatted.
 * \param msg printf()-style format string.
 */
void
mush_panicf(const char *msg, ...)
{
#ifdef HAS_VSNPRINTF
  char c[BUFFER_LEN];
#else
  char c[BUFFER_LEN * 3];
#endif
  va_list args;

  va_start(args, msg);

#ifdef HAS_VSNPRINTF
  vsnprintf(c, sizeof c, msg, args);
#else
  vsprintf(c, msg, args);
#endif
  c[BUFFER_LEN - 1] = '\0';
  va_end(args);

  mush_panic(c);
  _exit(136);                   /* Not reached but kills warnings */
}

/** Dump the database.
 * This function is a wrapper for dump_database_internal() that does
 * a little logging before and after the dump.
 */
void
dump_database(void)
{
  epoch++;

  do_rawlog(LT_ERR, "DUMPING: %s.#%d#", globals.dumpfile, epoch);
  if (dump_database_internal())
    do_rawlog(LT_ERR, "DUMPING: %s.#%d# (done)", globals.dumpfile, epoch);
}

/** Dump a database, possibly by forking the process.
 * This function calls dump_database_internal() to dump the MUSH
 * databases. If we're configured to do so, it forks first, so that
 * the child process can perform the dump while the parent continues
 * to run the MUSH for the players. If we can't fork, this function
 * warns players online that a dump is taking place and the game
 * may pause.
 * \param forking if 1, attempt a forking dump.
 */
void
fork_and_dump(int forking)
{
  pid_t child;
  bool nofork, status, split;
  epoch++;

#ifdef LOG_CHUNK_STATS
  chunk_stats(NOTHING, 0);
  chunk_stats(NOTHING, 1);
#endif
  do_rawlog(LT_CHECK, "CHECKPOINTING: %s.#%d#", globals.dumpfile, epoch);
  if (NO_FORK)
    nofork = 1;
  else
    nofork = !forking || (globals.paranoid_dump == 2);  /* Don't fork for dump/debug */
#if defined(WIN32) || !defined(HAVE_FORK)
  nofork = 1;
#endif
  split = 0;
  if (!nofork && chunk_num_swapped()) {
#ifndef WIN32
    /* Try to clone the chunk swapfile. */
    if (chunk_fork_file()) {
      split = 1;
    } else {
      /* Ack, can't fork, 'cause we have stuff on disk... */
      do_log(LT_ERR, 0, 0,
             "fork_and_dump: Data are swapped to disk, so nonforking dumps will be used.");
      flag_broadcast("WIZARD", 0,
                     T
                     ("DUMP: Data are swapped to disk, so nonforking dumps will be used."));
      nofork = 1;
    }
#endif
  }
  if (!nofork) {
#ifndef WIN32
#ifdef HAVE_FORK
    child = fork();
#else
    /* Never actually reached, since nofork is set to 1 if HAVE_FORK is
     *  not defined. */
    child = -1;
#endif
    if (child < 0) {
      /* Oops, fork failed. Let's do a nofork dump */
      do_log(LT_ERR, 0, 0,
             "fork_and_dump: fork() failed! Dumping nofork instead.");
      if (DUMP_NOFORK_MESSAGE && *DUMP_NOFORK_MESSAGE)
        flag_broadcast(0, 0, "%s", DUMP_NOFORK_MESSAGE);
      child = 0;
      nofork = 1;
      if (split) {
        split = 0;
        chunk_fork_done();
      }
    } else if (child > 0) {
      forked_dump_pid = child;
      lower_priority_by(child, 8);
      chunk_fork_parent();
    } else {
      chunk_fork_child();
    }
#endif                          /* WIN32 */
  } else {
    if (DUMP_NOFORK_MESSAGE && *DUMP_NOFORK_MESSAGE)
      flag_broadcast(0, 0, "%s", DUMP_NOFORK_MESSAGE);
    child = 0;
  }
  if (nofork || (!nofork && child == 0)) {
    /* in the child */
    release_fd();
    status = dump_database_internal();
#ifndef WIN32
    if (split)
      chunk_fork_done();
#endif
    if (!nofork) {
      _exit(status ? 0 : 1);    /* d_d_i() returns true on success but exit code should be 0 on success */
    } else {
      reserve_fd();
      if (status && DUMP_NOFORK_COMPLETE && *DUMP_NOFORK_COMPLETE)
        flag_broadcast(0, 0, "%s", DUMP_NOFORK_COMPLETE);
    }
  }
#ifdef LOG_CHUNK_STATS
  chunk_stats(NOTHING, 5);
#endif
}

/** Start up the MUSH.
 * This function does all of the work that's necessary to start up
 * MUSH objects and code for the game. It sets up player aliases,
 * fixes null object names, and triggers all object startups.
 */
void
do_restart(void)
{
  dbref thing;
  ATTR *s;
  char buf[BUFFER_LEN];
  char *bp;
  int j;

  /* Do stuff that needs to be done for players only: add stuff to the
   * alias table, and refund money from queued commands at shutdown.
   */
  for (thing = 0; thing < db_top; thing++) {
    if (IsPlayer(thing)) {
      if ((s = atr_get_noparent(thing, "ALIAS")) != NULL) {
        bp = buf;
        safe_str(atr_value(s), buf, &bp);
        *bp = '\0';
        add_player_alias(thing, buf);
      }
    }
  }

  /* Once we load all that, then we can trigger the startups and
   * begin queueing commands. Also, let's make sure that we get
   * rid of null names.
   */
  for (j = 0; j < 10; j++)
    global_eval_context.wnxt[j] = NULL;
  for (j = 0; j < NUMQ; j++)
    global_eval_context.rnxt[j] = NULL;

  /* Initialize the regexp patterns to nothing */
  global_eval_context.re_code = NULL;
  global_eval_context.re_subpatterns = -1;
  global_eval_context.re_offsets = NULL;
  global_eval_context.re_from = NULL;

  for (thing = 0; thing < db_top; thing++) {
    if (Name(thing) == NULL) {
      if (IsGarbage(thing))
        set_name(thing, "Garbage");
      else {
        do_log(LT_ERR, NOTHING, NOTHING, "Null name on object #%d", thing);
        set_name(thing, "XXXX");
      }
    }
    if (STARTUPS && !IsGarbage(thing) && !(Halted(thing))) {
      (void) queue_attribute_noparent(thing, "STARTUP", thing);
      do_top(5);
    }
  }
}

void init_names(void);
extern struct db_stat_info current_state;
void init_queue(void);

/** Initialize game structures and read the most of the configuration file.
 * This function runs before we read in the databases. It is responsible
 * for recording the MUSH start time, setting up all the hash and
 * prefix tables and similar structures, and reading the portions of the
 * config file that don't require database load.
 * \param conf file name of the configuration file.
 */
void
init_game_config(const char *conf)
{
  int a;
  pid_t mypid = -1;

  /* initialize random number generator */
  initialize_mt();

  init_queue();

  global_eval_context.process_command_port = 0;
  global_eval_context.break_called = 0;
  global_eval_context.cplr = NOTHING;
  strcpy(global_eval_context.ccom, "");

  for (a = 0; a < 10; a++) {
    global_eval_context.wenv[a] = NULL;
    global_eval_context.wnxt[a] = NULL;
  }
  for (a = 0; a < NUMQ; a++) {
    global_eval_context.renv[a][0] = '\0';
    global_eval_context.rnxt[a] = NULL;
  }

  /* set MUSH start time */
  globals.start_time = time((time_t *) 0);
  if (!globals.first_start_time)
    globals.first_start_time = globals.start_time;

  conf_default_set();

  /* Initialize the attribute chunk storage */
  chunk_init();

  /* initialize all the hash and prefix tables */
  init_flagspaces();
  init_flag_table("FLAG");
  init_flag_table("POWER");
  init_func_hashtab();
  init_ansi_codes();
  init_aname_table();
  init_atr_name_tree();
  init_locks();
  init_names();
  init_pronouns();
  command_init_preconfig();

  memset(&current_state, 0, sizeof current_state);

  /* Load all the config file stuff except restrict_* */
  local_configs();
  config_file_startup(conf, 0);
  start_all_logs();
  redirect_streams();


#ifdef HAVE_GETPID
  mypid = getpid();
#endif

  do_rawlog(LT_ERR, "%s", VERSION);
  do_rawlog(LT_ERR, "MUSH restarted, PID %d, at %s",
            (int) mypid, show_time(globals.start_time, 0));
}

/** Post-db-load configuration.
 * This function contains code that should be run after dbs are loaded
 * (usually because we need to have the flag table loaded, or because they
 * run last). It reads in the portions of the config file that rely
 * on flags being defined.
 * \param conf file name of the configuration file.
 */
void
init_game_postdb(const char *conf)
{
  /* access file stuff */
  read_access_file();
  /* set up signal handlers for the timer */
  init_timer();
  /* Commands and functions require the flag table for restrictions */
  command_init_postconfig();
  function_init_postconfig();
  attr_init_postconfig();
  /* Load further restrictions from config file */
  config_file_startup(conf, 1);
  validate_config();
  /* Call Local Startup */
  local_startup();
  /* everything else ok. Restart all objects. */
  do_restart();
#ifdef HAS_OPENSSL
  /* Set up ssl */
  if (!ssl_init
      (options.ssl_private_key_file, options.ssl_ca_file,
       options.ssl_require_client_cert)) {
    fprintf(stderr, "SSL initialization failure\n");
    options.ssl_port = 0;       /* Disable ssl */
  }
#endif
}

extern int dbline;

/** Read the game databases.
 * This function reads in the object, mail, and chat databases.
 * \retval -1 error.
 * \retval 0 success.
 */
int
init_game_dbs(void)
{
  PENNFILE *f;
  int c;
  const char *infile, *outfile;
  const char *mailfile;
  int panicdb;

#ifdef WIN32
  Win32MUSH_setup();            /* create index files, copy databases etc. */
#endif

  infile = restarting ? options.output_db : options.input_db;
  outfile = options.output_db;
  mailfile = options.mail_db;
  strcpy(globals.dumpfile, outfile);

  /* read small text files into cache */
  fcache_init();

  if (setjmp(db_err) == 1) {
    do_rawlog(LT_ERR, "Couldn't open %s! Creating minimal world.", infile);
    init_compress(NULL);
    create_minimal_db();
    return 0;
  } else {
    f = db_open(infile);
    c = penn_fgetc(f);
    if (c == EOF) {
      do_rawlog(LT_ERR, "Couldn't read %s! Creating minimal world.", infile);
      init_compress(NULL);
      create_minimal_db();
      return 0;
    }

    penn_ungetc(c, f);
  }

  if (setjmp(db_err) == 0) {
    /* ok, read it in */
    do_rawlog(LT_ERR, "ANALYZING: %s", infile);
    if (init_compress(f) < 0) {
      do_rawlog(LT_ERR, "ERROR LOADING %s", infile);
      return -1;
    }
    do_rawlog(LT_ERR, "ANALYZING: %s (done)", infile);

    /* everything ok */
    penn_fclose(f);

    f = db_open(infile);
    if (!f)
      return -1;

    /* ok, read it in */
    do_rawlog(LT_ERR, "LOADING: %s", infile);
    dbline = 0;
    if (db_read(f) < 0) {
      do_rawlog(LT_ERR, "ERROR LOADING %s", infile);
      penn_fclose(f);
      return -1;
    }
    do_rawlog(LT_ERR, "LOADING: %s (done)", infile);

    /* If there's stuff at the end of the db, we may have a panic
     * format db, with everything shoved together. In that case,
     * don't close the file
     */
    panicdb = ((globals.indb_flags & DBF_PANIC) && !penn_feof(f));

    if (!panicdb)
      penn_fclose(f);

    /* complain about bad config options */
    if (!GoodObject(PLAYER_START) || (!IsRoom(PLAYER_START)))
      do_rawlog(LT_ERR, "WARNING: Player_start (#%d) is NOT a room.",
                PLAYER_START);
    if (!GoodObject(MASTER_ROOM) || (!IsRoom(MASTER_ROOM)))
      do_rawlog(LT_ERR, "WARNING: Master room (#%d) is NOT a room.",
                MASTER_ROOM);
    if (!GoodObject(BASE_ROOM) || (!IsRoom(BASE_ROOM)))
      do_rawlog(LT_ERR, "WARNING: Base room (#%d) is NOT a room.", BASE_ROOM);
    if (!GoodObject(DEFAULT_HOME) || (!IsRoom(DEFAULT_HOME)))
      do_rawlog(LT_ERR, "WARNING: Default home (#%d) is NOT a room.",
                DEFAULT_HOME);
    if (!GoodObject(GOD) || (!IsPlayer(GOD)))
      do_rawlog(LT_ERR, "WARNING: God (#%d) is NOT a player.", GOD);

    /* read mail database */
    mail_init();

    if (panicdb) {
      do_rawlog(LT_ERR, "LOADING: Trying to get mail from %s", infile);
      if (load_mail(f) <= 0) {
        do_rawlog(LT_ERR, "FAILED: Reverting to normal maildb");
        penn_fclose(f);
        panicdb = 0;
      }
    }

    if (!panicdb) {
      f = db_open(mailfile);
      /* okay, read it in */
      if (f) {
        do_rawlog(LT_ERR, "LOADING: %s", mailfile);
        dbline = 0;
        load_mail(f);
        do_rawlog(LT_ERR, "LOADING: %s (done)", mailfile);
        penn_fclose(f);
      }
    }

    init_chatdb();

    if (panicdb) {
      do_rawlog(LT_ERR, "LOADING: Trying to get chat from %s", infile);
      if (load_chatdb(f) <= 0) {
        do_rawlog(LT_ERR, "FAILED: Reverting to normal chatdb");
        penn_fclose(f);
        panicdb = 0;
      }
    }

    if (!panicdb) {
      f = db_open(options.chatdb);
      if (f) {
        do_rawlog(LT_ERR, "LOADING: %s", options.chatdb);
        dbline = 0;
        if (load_chatdb(f)) {
          do_rawlog(LT_ERR, "LOADING: %s (done)", options.chatdb);
        } else {
          do_rawlog(LT_ERR, "ERROR LOADING %s", options.chatdb);
          return -1;
        }
        penn_fclose(f);
      }
    } else                      /* Close the panicdb file handle */
      penn_fclose(f);

  } else {
    do_rawlog(LT_ERR, "ERROR READING DATABASE");
    return -1;
  }

  return 0;
}

/** Read cached text files.
 * \verbatim
 * This implements the @readcache function.
 * \endverbatim
 * \param player the enactor, for permission checking.
 */
void
do_readcache(dbref player)
{
  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  fcache_load(player);
  help_reindex(player);
}

/** Check each attribute on each object in x for a $command matching cptr */
#define list_match(x)        list_check(x, player, '$', ':', cptr, 0)
/** Check each attribute on x for a $command matching cptr */
#define cmd_match(x)         atr_comm_match(x, player, '$', ':', cptr, 0, 1, NULL, NULL, &errdb)
#define MAYBE_ADD_ERRDB(errdb)  \
        do { \
          if (GoodObject(errdb) && errdblist) { \
            if ((errdbtail - errdblist) >= errdbsize) \
              errdb_grow(); \
            if ((errdbtail - errdblist) < errdbsize) { \
              *errdbtail = errdb; \
              errdbtail++; \
            } \
            errdb = NOTHING; \
          } \
         } while(0)

/** Attempt to tell if the command is a @password or @newpassword, so
  * that the password isn't logged by Suspect or log_commands
  * \param cmd The command to check
  * \return A sanitized version of the command suitable for logging.
  */
static char *
passwd_filter(const char *cmd)
{
  static int initialized = 0;
  static pcre *pass_ptn, *newpass_ptn;
  char *buff, *bp;
  int ovec[20];
  size_t cmdlen;
  int matched;

  if (!initialized) {
    const char *errptr;
    int eo;

    pass_ptn = pcre_compile("^(@pass.*?)\\s([^=]*)=(.*)",
                            PCRE_CASELESS, &errptr, &eo, tables);
    if (!pass_ptn)
      do_log(LT_ERR, GOD, GOD, "pcre_compile: %s", errptr);
    newpass_ptn = pcre_compile("^(@(?:newp|pcreate)[^=]*)=(.*)",
                               PCRE_CASELESS, &errptr, &eo, tables);
    if (!newpass_ptn)
      do_log(LT_ERR, GOD, GOD, "pcre_compile: %s", errptr);
    initialized = 1;
  }

  bp = buff = alloc_buf();

  cmdlen = strlen(cmd);

  if ((matched = pcre_exec(pass_ptn, NULL, cmd, cmdlen, 0, 0, ovec, 20)) > 0) {
    /* It's a password */
    pcre_copy_substring(cmd, ovec, matched, 1, buff, BUFFER_LEN);
    bp = buff + strlen(buff);
    safe_chr(' ', buff, &bp);
    safe_fill('*', ovec[5] - ovec[4], buff, &bp);
    safe_chr('=', buff, &bp);
    safe_fill('*', ovec[7] - ovec[6], buff, &bp);
  } else if ((matched = pcre_exec(newpass_ptn, NULL, cmd, cmdlen, 0, 0,
                                  ovec, 20)) > 0) {
    pcre_copy_substring(cmd, ovec, matched, 1, buff, BUFFER_LEN);
    bp = buff + strlen(buff);
    safe_chr('=', buff, &bp);
    safe_fill('*', ovec[5] - ovec[4], buff, &bp);
  } else {
    safe_strl(cmd, cmdlen, buff, &bp);
  }
  *bp = '\0';
  return buff;
}


/** Attempt to match and execute a command.
 * This function performs some sanity checks and then attempts to
 * run a command. It checks, in order: built-in commands,
 * enter aliases, leave aliases, $commands on neighboring objects or
 * the player, $commands on the container, $commands on inventory,
 * exits in the zone master room, $commands on objects in the ZMR,
 * $commands on the ZMO, $commands on the player's zone, exits in the
 * master room, and $commands on objectrs in the master room.
 *
 * When a command is directly input from a socket, we don't parse
 * the value in attribute sets.
 *
 * \param player the enactor.
 * \param command command to match and execute.
 * \param cause object which caused the command to be executed.
 * \param from_port if 1, the command was direct input from a socket.
 */
void
process_command(dbref player, char *command, dbref cause, int from_port)
{
  int a;
  char *p;                      /* utility */

  char unp[BUFFER_LEN];         /* unparsed command */
  /* general form command arg0=arg1,arg2...arg10 */
  char temp[BUFFER_LEN];        /* utility */
  int i;                        /* utility */
  char *cptr;
  dbref errdb;
  dbref check_loc;
  COMMAND_INFO *cmd;

  if (!errdblist)
    if (!(errdblist = GC_MALLOC_ATOMIC(errdbsize * sizeof(dbref))))
      mush_panic("Unable to allocate memory in process_command()!");

  errdbtail = errdblist;
  errdb = NOTHING;
  if (!command) {
    do_log(LT_ERR, NOTHING, NOTHING, "ERROR: No command!!!");
    return;
  }
  /* robustify player */
  if (!GoodObject(player)) {
    do_log(LT_ERR, NOTHING, NOTHING, "process_command bad player #%d", player);
    return;
  }

  /* Destroyed objects shouldn't execute commands */
  if (IsGarbage(player))
    /* No message - nobody to tell, and it's too easy to do to log. */
    return;
  /* Halted objects can't execute commands */
  /* And neither can halted players if the command isn't from_port */
  if (Halted(player) && (!IsPlayer(player) || !from_port)) {
    notify_format(Owner(player),
                  T("Attempt to execute command by halted object #%d"), player);
    return;
  }
  /* Players, things, and exits should not have invalid locations. This check
   * must be done _after_ the destroyed-object check.
   */
  check_loc = IsExit(player) ? Source(player) : (IsRoom(player) ? player :
                                                 Location(player));
  if (!GoodObject(check_loc) || IsGarbage(check_loc)) {
    notify_format(Owner(player),
                  T("Invalid location on command execution: %s(#%d)"),
                  Name(player), player);
    do_log(LT_ERR, NOTHING, NOTHING,
           "Command attempted by %s(#%d) in invalid location #%d.",
           Name(player), player, Location(player));
    if (Mobile(player))
      moveto(player, PLAYER_START);     /* move it someplace valid */
  }
  orator = player;

  /* eat leading whitespace */
  while (*command && isspace((unsigned char) *command))
    command++;

  /* eat trailing whitespace */
  p = command + strlen(command) - 1;
  while (isspace((unsigned char) *p) && (p >= command))
    p--;
  *++p = '\0';

  /* ignore null commands that aren't from players */
  if ((!command || !*command) && !from_port)
    return;

  {
    char *msg = passwd_filter(command);

    log_activity(LA_CMD, player, msg);
    if (options.log_commands || Suspect(player))
      do_log(LT_CMD, player, NOTHING, "%s", msg);
    if (Verbose(player))
      raw_notify(Owner(player), tprintf("#%d] %s", player, msg));
  }

  strcpy(unp, command);

  cptr = command_parse(player, cause, command, from_port);
  if (cptr) {
    mush_strncpy(global_eval_context.ucom, cptr, BUFFER_LEN);
    a = 0;
    if (!Gagged(player)) {

      if (Mobile(player)) {
        /* if the "player" is an exit or room, no need to do these checks */
        /* try matching enter aliases */
        if (check_loc != NOTHING && (cmd = command_find("ENTER")) &&
            !(cmd->type & CMD_T_DISABLED) &&
            (i = alias_list_check(Contents(check_loc), cptr, "EALIAS")) != -1) {
          if (command_check(player, cmd, 1)) {
            sprintf(temp, "#%d", i);
            run_command(cmd, player, cause, tprintf("ENTER #%d", i), NULL, NULL,
                        tprintf("ENTER #%d", i), NULL, NULL, temp, NULL, NULL,
                        NULL);
          }
          goto done;
        }
        /* if that didn't work, try matching leave aliases */
        if (!IsRoom(check_loc) && (cmd = command_find("LEAVE"))
            && !(cmd->type & CMD_T_DISABLED)
            && (loc_alias_check(check_loc, cptr, "LALIAS"))) {
          if (command_check(player, cmd, 1))
            run_command(cmd, player, cause, "LEAVE", NULL, NULL, "LEAVE", NULL,
                        NULL, NULL, NULL, NULL, NULL);
          goto done;
        }
      }

      /* try matching user defined functions before chopping */

      /* try objects in the player's location, the location itself,
       * and objects in the player's inventory.
       */
      if (GoodObject(check_loc)) {
        a += list_match(Contents(check_loc));
        if (check_loc != player) {
          a += cmd_match(check_loc);
          MAYBE_ADD_ERRDB(errdb);
        }
      }
      if (check_loc != player)
        a += list_match(Contents(player));

      /* now do check on zones */
      if ((!a) && (Zone(check_loc) != NOTHING)) {
        if (IsRoom(Zone(check_loc))) {
          /* zone of player's location is a zone master room,
           * so we check for exits and commands
           */
          /* check zone master room exits */
          if (remote_exit(player, cptr) && (cmd = command_find("GOTO"))
              && !(cmd->type & CMD_T_DISABLED)) {
            if (!Mobile(player) || !command_check(player, cmd, 1)) {
              goto done;
            } else {
              run_command(cmd, player, cause, tprintf("GOTO %s", cptr), NULL,
                          NULL, tprintf("GOTO %s", cptr), NULL, NULL, cptr,
                          NULL, NULL, NULL);
              goto done;
            }
          } else
            a += list_match(Contents(Zone(check_loc)));
        } else {
          a += cmd_match(Zone(check_loc));
          MAYBE_ADD_ERRDB(errdb);
        }
      }
      /* if nothing matched with zone master room/zone object, try
       * matching zone commands on the player's personal zone
       */
      if ((!a) && (Zone(player) != NOTHING) &&
          (Zone(check_loc) != Zone(player))) {
        if (IsRoom(Zone(player)))
          /* Player's personal zone is a zone master room, so we
           * also check commands on objects in that room
           */
          a += list_match(Contents(Zone(player)));
        else {
          a += cmd_match(Zone(player));
          MAYBE_ADD_ERRDB(errdb);
        }
      }
      /* end of zone stuff */
      /* check global exits only if no other commands are matched */
      if ((!a) && (check_loc != MASTER_ROOM)) {
        if (global_exit(player, cptr) && (cmd = command_find("GOTO"))
            && !(cmd->type & CMD_T_DISABLED)) {
          if (!Mobile(player) || !command_check(player, cmd, 1))
            goto done;
          else {
            run_command(cmd, player, cause, tprintf("GOTO %s", cptr), NULL,
                        NULL, tprintf("GOTO %s", cptr), NULL, NULL, cptr, NULL,
                        NULL, NULL);
            goto done;
          }
        } else
          /* global user-defined commands checked if all else fails.
           * May match more than one command in the master room.
           */
          a += list_match(Contents(MASTER_ROOM));
      }
      /* end of master room check */
    }                           /* end of special checks */
    if (!a) {
      /* Do we have any error dbs queued up, and if so, do any
       * have associated failure messages?
       */
      if ((errdblist == errdbtail) || (!fail_commands(player)))
        /* Nope. This is totally unmatched, run generic failure */
        generic_command_failure(player, cause, unp);
    }
  }

  /* command has been executed. Free up memory. */

done:
  errdblist = errdbtail = NULL;
  errdbsize = ERRDB_INITIAL_SIZE;
}


COMMAND(cmd_with)
{
  dbref what;
  char *cptr = arg_right;
  dbref errdb;

  what = noisy_match_result(player, arg_left, NOTYPE, MAT_NEARBY);
  if (!GoodObject(what))
    return;

  errdbtail = errdblist;
  errdb = NOTHING;
  if (!SW_ISSET(sw, SWITCH_ROOM)) {
    /* Run commands on a single object */
    if (!cmd_match(what)) {
      MAYBE_ADD_ERRDB(errdb);
      notify(player, T("No matching command."));
    }
  } else {
    /* Run commands on objects in a masterish room */

    if (!IsRoom(what)) {
      notify(player, T("Make room! Make room!"));
      return;
    }

    if (!list_match(Contents(what)))
      notify(player, T("No matching command."));
  }
}

/* now undef everything that needs to be */
#undef list_match
#undef cmd_match

/** Check to see if a string matches part of a semicolon-separated list.
 * \param command string to match.
 * \param list semicolon-separated list of aliases to match against.
 * \retval 1 string matched an alias.
 * \retval 0 string failed to match an alias.
 */
int
check_alias(const char *command, const char *list)
{
  /* check if a string matches part of a semi-colon separated list */
  const char *p;
  while (*list) {
    for (p = command; (*p && DOWNCASE(*p) == DOWNCASE(*list)
                       && *list != EXIT_DELIMITER); p++, list++) ;
    if (*p == '\0') {
      while (isspace((unsigned char) *list))
        list++;
      if (*list == '\0' || *list == EXIT_DELIMITER)
        return 1;               /* word matched */
    }
    /* didn't match. check next word in list */
    while (*list && *list++ != EXIT_DELIMITER) ;
    while (isspace((unsigned char) *list))
      list++;
  }
  /* reached the end of the list without matching anything */
  return 0;
}

/** Match a command or listen pattern against a list of things.
 * This function iterates through a list of things (using the object
 * Next pointer, so typically this is a list of contents of an object),
 * and checks each for an attribute matching a command/listen pattern.
 * \param thing first object on list.
 * \param player the enactor.
 * \param type type of attribute to match ('$' or '^')
 * \param end character that signals the end of the matchable portion (':')
 * \param str string to match against the attributes.
 * \param just_match if 1, don't execute the command on match.
 * \retval 1 a match was made.
 * \retval 0 no match was made.
 */
static int
list_check(dbref thing, dbref player, char type, char end, char *str,
           int just_match)
{
  int match = 0;
  dbref errdb = NOTHING;

  while (thing != NOTHING) {
    if (atr_comm_match
        (thing, player, type, end, str, just_match, 1, NULL, NULL, &errdb))
      match = 1;
    else {
      MAYBE_ADD_ERRDB(errdb);
    }
    thing = Next(thing);
  }
  return (match);
}

/** Match a command against an attribute of aliases.
 * This function iterates through a list of things (using the object
 * Next pointer, so typically this is a list of contents of an object),
 * and checks each for an attribute of aliases that might be matched
 * (as in EALIAS).
 * \param thing first object on list.
 * \param command command to attempt to match.
 * \param type name of attribute of aliases to match against.
 * \return dbref of first matching object, or -1 if none.
 */
int
alias_list_check(dbref thing, const char *command, const char *type)
{
  ATTR *a;
  char alias[BUFFER_LEN];

  while (thing != NOTHING) {
    a = atr_get_noparent(thing, type);
    if (a) {
      strcpy(alias, atr_value(a));
      if (check_alias(command, alias) != 0)
        return thing;           /* matched an alias */
    }
    thing = Next(thing);
  }
  return -1;
}

/** Check a command against a list of aliases on a location
 * (as for LALIAS).
 * \param loc location with attribute of aliases.
 * \param command command to attempt to match.
 * \param type name of attribute of aliases to match against.
 * \retval 1 successful match.
 * \retval 0 failure.
 */
int
loc_alias_check(dbref loc, const char *command, const char *type)
{
  ATTR *a;
  char alias[BUFFER_LEN];
  a = atr_get_noparent(loc, type);
  if (a) {
    strcpy(alias, atr_value(a));
    return (check_alias(command, alias));
  } else
    return 0;
}

/** Can an object hear?
 * This function determines if a given object can hear. A Hearer is:
 * a connected player, a puppet, an AUDIBLE object with a FORWARDLIST
 * attribute, or an object with a LISTEN attribute.
 * \param thing object to check.
 * \retval 1 object can hear.
 * \retval 0 object can't hear.
 */
int
Hearer(dbref thing)
{
  ALIST *ptr;
  int cmp;

  if (Connected(thing) || Puppet(thing))
    return 1;
  for (ptr = List(thing); ptr; ptr = AL_NEXT(ptr)) {
    if (Audible(thing) && (strcmp(AL_NAME(ptr), "FORWARDLIST") == 0))
      return 1;
    cmp = strcoll(AL_NAME(ptr), "LISTEN");
    if (cmp == 0)
      return 1;
    if (cmp > 0)
      break;
  }
  return 0;
}

/** Might an object be responsive to commands?
 * This function determines if a given object might pick up a $command.
 * That is, if it has any attributes with $commands on them that are
 * not set no_command.
 * \param thing object to check.
 * \retval 1 object responds to commands.
 * \retval 0 object doesn't respond to commands.
 */
int
Commer(dbref thing)
{
  ALIST *ptr;

  for (ptr = List(thing); ptr; ptr = AL_NEXT(ptr)) {
    if (AF_Command(ptr) && !AF_Noprog(ptr))
      return (1);
  }
  return (0);
}

/** Is an object listening?
 * This function determines if a given object is a Listener. A Listener
 * is a thing or room that has the MONITOR flag set.
 * \param thing object to check.
 * \retval 1 object is a Listener.
 * \retval 0 object isn't listening with ^patterns.
 */
int
Listener(dbref thing)
{
  /* If a monitor flag is set on a room or thing, it's a listener.
   * Otherwise not (even if ^patterns are present)
   */
  return has_flag_by_name(thing, "MONITOR", NOTYPE);
}

/** Reset all players' money.
 * \verbatim
 * This function implements the @poor command. It probably belongs in
 * rob.c, though.
 * \endverbatim
 * \param player the enactor, for permission checking.
 * \param arg1 the amount of money to reset all players to.
 */
void
do_poor(dbref player, char *arg1)
{
  int amt = atoi(arg1);
  dbref a;
  if (!God(player)) {
    notify(player, T("Only God can cause financial ruin."));
    return;
  }
  for (a = 0; a < db_top; a++)
    if (IsPlayer(a))
      s_Pennies(a, amt);
  notify_format(player,
                T
                ("The money supply of all players has been reset to %d %s."),
                amt, MONIES);
  do_log(LT_WIZ, player, NOTHING,
         "** POOR done ** Money supply reset to %d %s.", amt, MONIES);
}


/** User interface to write a message to a log.
 * \verbatim
 * This function implements @log.
 * \endverbatim
 * \param player the enactor.
 * \param str message to write to the log.
 * \param ltype type of log to write to.
 */
void
do_writelog(dbref player, char *str, int ltype)
{
  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  do_rawlog(ltype, "LOG: %s(#%d%s): %s", Name(player), player,
            unparse_flags(player, GOD), str);

  notify(player, T("Logged."));
}

/** Bind occurences of '##' in "action" to "arg", then run "action".
 * \param player the enactor.
 * \param cause object that caused command to run.
 * \param action command string which may contain tokens.
 * \param arg value for ## token.
 * \param placestr value for #@ token.
 */
void
bind_and_queue(dbref player, dbref cause, char *action,
               const char *arg, const char *placestr)
{
  char *repl, *command;
  const char *replace[2];

  replace[0] = arg;
  replace[1] = placestr;

  repl = replace_string2(standard_tokens, replace, action);

  command = strip_braces(repl);

  parse_que(player, command, cause);
}

/** Would the scan command find an matching attribute on x for player p? */
#define ScanFind(p,x)  \
  (Can_Examine(p,x) && \
      ((num = atr_comm_match(x, p, '$', ':', command, 1, 1, atrname, &ptr, NULL)) != 0))

/** Scan for matches of $commands.
 * This function scans for possible matches of user-def'd commands from the
 * viewpoint of player, and return as a string.
 * It assumes that atr_comm_match() returns atrname with a leading space.
 * \param player the object from whose viewpoint to scan.
 * \param command the command to scan for matches to.
 * \return string of obj/attrib pairs with matching $commands.
 */
char *
scan_list(dbref player, char *command)
{
  char *tbuf, *tp;
  dbref thing;
  char atrname[BUFFER_LEN];
  char *ptr;
  int num;

  tbuf = tp = alloc_buf();

  if (!GoodObject(Location(player))) {
    strcpy(tbuf, T("#-1 INVALID LOCATION"));
    return tbuf;
  }
  if (!command || !*command) {
    strcpy(tbuf, T("#-1 NO COMMAND"));
    return tbuf;
  }
  tp = tbuf;
  ptr = atrname;
  DOLIST(thing, Contents(Location(player))) {
    if (ScanFind(player, thing)) {
      *ptr = '\0';
      safe_str(atrname, tbuf, &tp);
      ptr = atrname;
    }
  }
  ptr = atrname;
  if (ScanFind(player, Location(player))) {
    *ptr = '\0';
    safe_str(atrname, tbuf, &tp);
  }
  ptr = atrname;
  DOLIST(thing, Contents(player)) {
    if (ScanFind(player, thing)) {
      *ptr = '\0';
      safe_str(atrname, tbuf, &tp);
      ptr = atrname;
    }
  }
  /* zone checks */
  ptr = atrname;
  if (Zone(Location(player)) != NOTHING) {
    if (IsRoom(Zone(Location(player)))) {
      /* zone of player's location is a zone master room */
      if (Location(player) != Zone(player)) {
        DOLIST(thing, Contents(Zone(Location(player)))) {
          if (ScanFind(player, thing)) {
            *ptr = '\0';
            safe_str(atrname, tbuf, &tp);
            ptr = atrname;
          }
        }
      }
    } else {
      /* regular zone object */
      if (ScanFind(player, Zone(Location(player)))) {
        *ptr = '\0';
        safe_str(atrname, tbuf, &tp);
      }
    }
  }
  ptr = atrname;
  if ((Zone(player) != NOTHING)
      && (Zone(player) != Zone(Location(player)))) {
    /* check the player's personal zone */
    if (IsRoom(Zone(player))) {
      if (Location(player) != Zone(player)) {
        DOLIST(thing, Contents(Zone(player))) {
          if (ScanFind(player, thing)) {
            *ptr = '\0';
            safe_str(atrname, tbuf, &tp);
            ptr = atrname;
          }
        }
      }
    } else if (ScanFind(player, Zone(player))) {
      *ptr = '\0';
      safe_str(atrname, tbuf, &tp);
    }
  }
  ptr = atrname;
  if ((Location(player) != MASTER_ROOM)
      && (Zone(Location(player)) != MASTER_ROOM)
      && (Zone(player) != MASTER_ROOM)) {
    /* try Master Room stuff */
    DOLIST(thing, Contents(MASTER_ROOM)) {
      if (ScanFind(player, thing)) {
        *ptr = '\0';
        safe_str(atrname, tbuf, &tp);
        ptr = atrname;
      }
    }
  }
  *tp = '\0';
  if (*tbuf && *tbuf == ' ')
    return tbuf + 1;            /* atrname comes with leading spaces */
  return tbuf;
}

/** User interface to scan for $command matches.
 * \verbatim
 * This function implements @scan.
 * \endverbatim
 * \param player the enactor.
 * \param command command to scan for matches to.
 * \param flag bitflags for where to scan.
 */
void
do_scan(dbref player, char *command, int flag)
{
  /* scan for possible matches of user-def'ed commands */
  char atrname[BUFFER_LEN];
  char *ptr;
  dbref thing;
  int num;
  char save_ccom[BUFFER_LEN];

  ptr = atrname;
  if (!GoodObject(Location(player))) {
    notify(player, T("Sorry, you are in an invalid location."));
    return;
  }
  if (!command || !*command) {
    notify(player, T("What command do you want to scan for?"));
    return;
  }
  strcpy(save_ccom, global_eval_context.ccom);
  memmove(global_eval_context.ccom, (char *) global_eval_context.ccom + 5,
          BUFFER_LEN - 5);
  if (flag & CHECK_NEIGHBORS) {
    notify(player, T("Matches on contents of this room:"));
    DOLIST(thing, Contents(Location(player))) {
      if (ScanFind(player, thing)) {
        *ptr = '\0';
        notify_format(player,
                      "%s  [%d:%s]", unparse_object(player, thing),
                      num, atrname);
        ptr = atrname;
      }
    }
  }
  ptr = atrname;
  if (flag & CHECK_HERE) {
    if (ScanFind(player, Location(player))) {
      *ptr = '\0';
      notify_format(player, T("Matched here: %s  [%d:%s]"),
                    unparse_object(player, Location(player)), num, atrname);
    }
  }
  ptr = atrname;
  if (flag & CHECK_INVENTORY) {
    notify(player, T("Matches on carried objects:"));
    DOLIST(thing, Contents(player)) {
      if (ScanFind(player, thing)) {
        *ptr = '\0';
        notify_format(player, "%s  [%d:%s]",
                      unparse_object(player, thing), num, atrname);
        ptr = atrname;
      }
    }
  }
  ptr = atrname;
  if (flag & CHECK_SELF) {
    if (ScanFind(player, player)) {
      *ptr = '\0';
      notify_format(player, T("Matched self: %s  [%d:%s]"),
                    unparse_object(player, player), num, atrname);
    }
  }
  ptr = atrname;
  if (flag & CHECK_ZONE) {
    /* zone checks */
    if (Zone(Location(player)) != NOTHING) {
      if (IsRoom(Zone(Location(player)))) {
        /* zone of player's location is a zone master room */
        if (Location(player) != Zone(player)) {
          notify(player, T("Matches on zone master room of location:"));
          DOLIST(thing, Contents(Zone(Location(player)))) {
            if (ScanFind(player, thing)) {
              *ptr = '\0';
              notify_format(player, "%s  [%d:%s]",
                            unparse_object(player, thing), num, atrname);
              ptr = atrname;
            }
          }
        }
      } else {
        /* regular zone object */
        if (ScanFind(player, Zone(Location(player)))) {
          *ptr = '\0';
          notify_format(player,
                        T("Matched zone of location: %s  [%d:%s]"),
                        unparse_object(player,
                                       Zone(Location(player))), num, atrname);
        }
      }
    }
    ptr = atrname;
    if ((Zone(player) != NOTHING)
        && (Zone(player) != Zone(Location(player)))) {
      /* check the player's personal zone */
      if (IsRoom(Zone(player))) {
        if (Location(player) != Zone(player)) {
          notify(player, T("Matches on personal zone master room:"));
          DOLIST(thing, Contents(Zone(player))) {
            if (ScanFind(player, thing)) {
              *ptr = '\0';
              notify_format(player, "%s  [%d:%s]",
                            unparse_object(player, thing), num, atrname);
              ptr = atrname;
            }
          }
        }
      } else if (ScanFind(player, Zone(player))) {
        *ptr = '\0';
        notify_format(player, T("Matched personal zone: %s  [%d:%s]"),
                      unparse_object(player, Zone(player)), num, atrname);
      }
    }
  }
  ptr = atrname;
  if ((flag & CHECK_GLOBAL)
      && (Location(player) != MASTER_ROOM)
      && (Zone(Location(player)) != MASTER_ROOM)
      && (Zone(player) != MASTER_ROOM)) {
    /* try Master Room stuff */
    notify(player, T("Matches on objects in the Master Room:"));
    DOLIST(thing, Contents(MASTER_ROOM)) {
      if (ScanFind(player, thing)) {
        *ptr = '\0';
        notify_format(player, "%s  [%d:%s]",
                      unparse_object(player, thing), num, atrname);
        ptr = atrname;
      }
    }
  }
  strcpy(global_eval_context.ccom, save_ccom);
}

#define DOL_NOTIFY 2   /**< Add a notify after a dolist */
#define DOL_DELIM 4    /**< Specify a delimiter to a dolist */

/** Execute a command for each element of a list.
 * \verbatim
 * This function implements @dolist.
 * \endverbatim
 * \param player the enactor.
 * \param list string containing the list to iterate over.
 * \param command command to run for each list element.
 * \param cause object which caused this command to be run.
 * \param flags command switch flags.
 */
void
do_dolist(dbref player, char *list, char *command, dbref cause,
          unsigned int flags)
{
  char *curr, *objstring;
  char outbuf[BUFFER_LEN];
  char *bp;
  int place;
  char placestr[10];
  int j;
  char delim = ' ';
  if (!command || !*command) {
    notify(player, T("What do you want to do with the list?"));
    if (flags & DOL_NOTIFY)
      parse_que(player, "@notify me", cause);
    return;
  }

  if (flags & DOL_DELIM) {
    if (list[1] != ' ') {
      notify(player, T("Separator must be one character."));
      if (flags & DOL_NOTIFY)
        parse_que(player, "@notify me", cause);
      return;
    }
    delim = list[0];
  }

  /* set up environment for any spawned commands */
  for (j = 0; j < 10; j++)
    global_eval_context.wnxt[j] = global_eval_context.wenv[j];
  for (j = 0; j < NUMQ; j++)
    global_eval_context.rnxt[j] = global_eval_context.renv[j];
  bp = outbuf;
  if (flags & DOL_DELIM)
    list += 2;
  place = 0;
  objstring = trim_space_sep(list, delim);
  if (objstring && !*objstring) {
    /* Blank list */
    if (flags & DOL_NOTIFY)
      parse_que(player, "@notify me", cause);
    return;
  }

  while (objstring) {
    curr = split_token(&objstring, delim);
    place++;
    sprintf(placestr, "%d", place);
    bind_and_queue(player, cause, command, curr, placestr);
  }

  *bp = '\0';
  if (flags & DOL_NOTIFY) {
    /*  Execute a '@notify me' so the object knows we're done
     *  with the list execution. We don't execute dequeue_semaphores()
     *  directly, since we want the command to be queued
     *  _after_ the list has executed.
     */
    parse_que(player, "@notify me", cause);
  }
}

static void linux_uptime(dbref player) __attribute__ ((__unused__));
static void unix_uptime(dbref player) __attribute__ ((__unused__));
static void win32_uptime(dbref player) __attribute__ ((__unused__));

static void
linux_uptime(dbref player __attribute__ ((__unused__)))
{
#ifdef linux
  /* Use /proc files instead of calling the external uptime program on linux */
  char tbuf1[BUFFER_LEN];
  FILE *fp;
  char line[128];               /* Overkill */
  char *nl;
  pid_t pid;
  int psize;
#ifdef HAS_GETRUSAGE
  struct rusage usage;
#endif

  /* Current time */
  {
    struct tm *t;
    t = localtime(&mudtime);
    strftime(tbuf1, sizeof tbuf1, "Server uptime: %I:%M%p ", t);
    nl = tbuf1 + strlen(tbuf1);
  }
  /* System uptime */
  fp = fopen("/proc/uptime", "r");
  if (fp) {
    time_t uptime;
    const char *fmt;
    if (fgets(line, sizeof line, fp)) {
      /* First part of this line is uptime in seconds.milliseconds. We
         only care about seconds. */
      uptime = strtol(line, NULL, 10);
      if (uptime > 86400)
        fmt = "up $d days, $2h:$2M,";
      else
        fmt = "up $2h:$2M,";
      do_timestring(tbuf1, &nl, fmt, uptime);
    } else {
      safe_str("Unknown uptime,", tbuf1, &nl);
    }
    fclose(fp);
  } else {
    safe_str("Unknown uptime,", tbuf1, &nl);
  }

  /* Now load averages */
  fp = fopen("/proc/loadavg", "r");
  if (fp) {
    if (fgets(line, sizeof line, fp)) {
      double load[3];
      char *x, *l = line;
      load[0] = strtod(l, &x);
      l = x;
      load[1] = strtod(l, &x);
      l = x;
      load[2] = strtod(l, NULL);
      safe_format(tbuf1, &nl, " load average: %.2f, %.2f, %.2f",
                  load[0], load[1], load[2]);
    } else {
      safe_str("Unknown load", tbuf1, &nl);
    }
    fclose(fp);
  } else {
    safe_str("Unknown load", tbuf1, &nl);
  }

  *nl = '\0';
  notify(player, tbuf1);

  /* do process stats */
  pid = getpid();
  psize = getpagesize();
  notify_format(player,
                "\nProcess ID:  %10u        %10d bytes per page", pid, psize);

  /* Linux's getrusage() is mostly unimplemented. Just has times, page faults
     and swapouts. We use /proc/self/status */
#ifdef HAS_GETRUSAGE
  getrusage(RUSAGE_SELF, &usage);
  notify_format(player, "Time used:   %10ld user   %10ld sys",
                usage.ru_utime.tv_sec, usage.ru_stime.tv_sec);
  notify_format(player,
                "Page faults: %10ld hard   %10ld soft    %10ld swapouts",
                usage.ru_majflt, usage.ru_minflt, usage.ru_nswap);
#endif
  fp = fopen("/proc/self/status", "r");
  if (!fp)
    return;
  /* Skip lines we don't care about. */
  while (fgets(line, sizeof line, fp) != NULL) {
    static const char *fields[] = {
      "VmSize:", "VmRSS:", "VmData:", "VmStk:", "VmExe:", "VmLib:",
      "SigPnd:", "SigBlk:", "SigIgn:", "SigCgt:", NULL
    };
    int n;
    for (n = 0; fields[n]; n++) {
      size_t len = strlen(fields[n]);
      if (strncmp(line, fields[n], len) == 0) {
        if ((nl = strchr(line, '\n')) != NULL)
          *nl = '\0';
        notify(player, line);
      }
    }
  }

  fclose(fp);

#endif
}

static void
unix_uptime(dbref player __attribute__ ((__unused__)))
{
#ifndef WIN32
#ifdef HAVE_UPTIME
  FILE *fp;
  char c;
  int i;
#endif
#ifdef HAS_GETRUSAGE
  struct rusage usage;
#endif
  char tbuf1[BUFFER_LEN];
  pid_t pid;
  int psize;

#ifdef HAVE_UPTIME
  fp =
#ifdef __LCC__
    (FILE *)
#endif
    popen(UPTIME, "r");

  /* just in case the system is screwy */
  if (fp == NULL) {
    notify(player, T("Error -- cannot execute uptime."));
    do_rawlog(LT_ERR, "** ERROR ** popen for @uptime returned NULL.");
    return;
  }
  /* print system uptime */
  for (i = 0; (c = getc(fp)) != '\n' && c != EOF; i++)
    tbuf1[i] = c;
  tbuf1[i] = '\0';
  pclose(fp);

  notify(player, tbuf1);
#endif                          /* HAVE_UPTIME */

  /* do process stats */

  pid = getpid();
  psize = getpagesize();
  notify_format(player,
                "\nProcess ID:  %10u        %10d bytes per page", pid, psize);


#ifdef HAS_GETRUSAGE
  getrusage(RUSAGE_SELF, &usage);
  notify_format(player, "Time used:   %10ld user   %10ld sys",
                (long) usage.ru_utime.tv_sec, (long) usage.ru_stime.tv_sec);
  notify_format(player, "Max res mem: %10ld pages  %10ld bytes",
                usage.ru_maxrss, (usage.ru_maxrss * psize));
  notify_format(player,
                "Integral mem:%10ld shared %10ld private %10ld stack",
                usage.ru_ixrss, usage.ru_idrss, usage.ru_isrss);
  notify_format(player,
                "Page faults: %10ld hard   %10ld soft    %10ld swapouts",
                usage.ru_majflt, usage.ru_minflt, usage.ru_nswap);
  notify_format(player, "Disk I/O:    %10ld reads  %10ld writes",
                usage.ru_inblock, usage.ru_oublock);
  notify_format(player, "Network I/O: %10ld in     %10ld out",
                usage.ru_msgrcv, usage.ru_msgsnd);
  notify_format(player, "Context swi: %10ld vol    %10ld forced",
                usage.ru_nvcsw, usage.ru_nivcsw);
  notify_format(player, "Signals:     %10ld", usage.ru_nsignals);
#endif                          /* HAS_GETRUSAGE */
#endif
}

static void
win32_uptime(dbref player __attribute__ ((__unused__)))
{                               /* written by NJG */
#ifdef WIN32
  MEMORYSTATUS memstat;
  double mem;
  memstat.dwLength = sizeof(memstat);
  GlobalMemoryStatus(&memstat);
  notify(player, "---------- Windows memory usage ------------");
  notify_format(player, "%10ld %% memory in use", memstat.dwMemoryLoad);
  mem = memstat.dwAvailPhys / 1024.0 / 1024.0;
  notify_format(player, "%10.3f Mb free physical memory", mem);
  mem = memstat.dwTotalPhys / 1024.0 / 1024.0;
  notify_format(player, "%10.3f Mb total physical memory", mem);
  mem = memstat.dwAvailPageFile / 1024.0 / 1024.0;
  notify_format(player, "%10.3f Mb available in the paging file ", mem);
  mem = memstat.dwTotalPageFile / 1024.0 / 1024.0;
  notify_format(player, "%10.3f Mb total paging file size", mem);
#endif
}


/** Report on server uptime.
 * \verbatim
 * This command implements @uptime.
 * \endverbatim
 * \param player the enactor.
 * \param mortal if 1, show mortal display, even if player is privileged.
 */
void
do_uptime(dbref player, int mortal)
{
  char tbuf1[BUFFER_LEN];
  struct tm *when;
  ldiv_t secs;

  when = localtime(&globals.first_start_time);
  strftime(tbuf1, sizeof tbuf1, "%a %b %d %X %Z %Y", when);
  notify_format(player, "%13s: %s", T("Up since"), tbuf1);

  when = localtime(&globals.start_time);
  strftime(tbuf1, sizeof tbuf1, "%a %b %d %X %Z %Y", when);
  notify_format(player, "%13s: %s", T("Last reboot"), tbuf1);

  notify_format(player, "%13s: %d", T("Total reboots"), globals.reboot_count);

  when = localtime(&mudtime);
  strftime(tbuf1, sizeof tbuf1, "%a %b %d %X %Z %Y", when);
  notify_format(player, "%13s: %s", T("Time now"), tbuf1);

  if (globals.last_dump_time > 0) {
    when = localtime(&globals.last_dump_time);
    strftime(tbuf1, sizeof tbuf1, "%a %b %d %X %Z %Y", when);
    notify_format(player, "%29s: %s", T("Time of last database save"), tbuf1);
  }

  /* calculate times until various events */
  when = localtime(&options.dump_counter);
  strftime(tbuf1, sizeof tbuf1, "%X", when);
  secs = ldiv((long) difftime(options.dump_counter, mudtime), 60);
  notify_format(player,
                T("%29s: %ld minutes %ld seconds, at %s."),
                T("Time until next database save"), secs.quot, secs.rem, tbuf1);

  when = localtime(&options.dbck_counter);
  strftime(tbuf1, sizeof tbuf1, "%X", when);
  secs = ldiv((long) difftime(options.dbck_counter, mudtime), 60);
  notify_format(player,
                T("%29s: %ld minutes %ld seconds, at %s."),
                T("Time until next dbck check"), secs.quot, secs.rem, tbuf1);

  when = localtime(&options.purge_counter);
  strftime(tbuf1, sizeof tbuf1, "%X", when);
  secs = ldiv((long) difftime(options.purge_counter, mudtime), 60);
  notify_format(player,
                T("%29s: %ld minutes %ld seconds, at %s."),
                T("Time until next purge"), secs.quot, secs.rem, tbuf1);

  if (options.warn_interval) {
    when = localtime(&options.warn_counter);
    strftime(tbuf1, sizeof tbuf1, "%X", when);
    secs = ldiv((long) difftime(options.warn_counter, mudtime), 60);
    notify_format(player,
                  T("%29s: %ld minutes %ld seconds, at %s."),
                  T("Time until next @warnings"), secs.quot, secs.rem, tbuf1);
  }

  {
    /* 86400 == seconds in 1 day. 3600 == seconds in 1 hour */
    long days;
    ldiv_t hours, mins;

    secs = ldiv((long) difftime(mudtime, globals.first_start_time), 86400);
    days = secs.quot;
    hours = ldiv(secs.rem, 3600);
    mins = ldiv(hours.rem, 60);


    notify_format(player,
                  T
                  ("PennMUSH Uptime: %ld days %ld hours %ld minutes %ld seconds"),
                  days, hours.quot, mins.quot, mins.rem);
  }
  /* Mortals, go no further! */
  if (!Wizard(player) || mortal)
    return;

  /* Mortals, go no further! */
  if (!Wizard(player) || mortal)
    return;
#if defined(linux)
  linux_uptime(player);
#elif defined(WIN32)
  win32_uptime(player);
#else
  unix_uptime(player);
#endif

  if (God(player))
    notify_activity(player, 0, 0);
}


/* Open a db file, which may be compressed, and return a file pointer. These probably should be moved into db.c or
 a new dbio.c */
static PENNFILE *
db_open(const char *fname)
{
  PENNFILE *pf;
  char filename[BUFFER_LEN];

  snprintf(filename, sizeof filename, "%s%s", fname, options.compresssuff);

  pf = GC_MALLOC(sizeof *pf);

#ifdef HAVE_LIBZ
  if (*options.uncompressprog && strcmp(options.uncompressprog, "gunzip") == 0) {
    pf->type = PFT_GZFILE;
    pf->handle.g = gzopen(filename, "rb");
    if (!pf->handle.g) {
      do_rawlog(LT_ERR, "Unable to open %s with libz: %s\n", filename,
                strerror(errno));
      GC_FREE(pf);
      longjmp(db_err, 1);
    }
    return pf;
  }
#endif

#ifndef WIN32
  if (*options.uncompressprog) {
    pf->type = PFT_PIPE;
    /* We do this because on some machines (SGI Irix, for example),
     * the popen will not return NULL if the mailfile isn't there.
     */

    if (access(filename, R_OK) == 0) {
      pf->handle.f =
#ifdef __LCC__
        (FILE *)
#endif
        popen(tprintf("%s < '%s'", options.uncompressprog, filename), "r");
      /* Force the pipe to be fully buffered */
      if (pf->handle.f) {
        setvbuf(pf->handle.f, NULL, _IOFBF, BUFSIZ);
      } else
        do_rawlog(LT_ERR, "Unable to run '%s < %s': %s",
                  options.uncompressprog, filename, strerror(errno));
    } else {
      GC_FREE(pf);
      longjmp(db_err, 1);
    }
  } else
#endif                          /* WIN32 */
  {
    pf->type = PFT_FILE;
    pf->handle.f = fopen(filename, FOPEN_READ);
    if (!pf->handle.f)
      do_rawlog(LT_ERR, "Unable to open %s: %s\n", filename, strerror(errno));
#ifdef HAVE_POSIX_FADVISE
    else if (pf->handle.f)
      posix_fadvise(fileno(pf->handle.f), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  }
  if (!pf->handle.f) {
    GC_FREE(pf);
    longjmp(db_err, 1);
  }
  return pf;
}

/* Open a file or pipe (if compressing) for writing */
static PENNFILE *
db_open_write(const char *fname)
{
  PENNFILE *pf;
  char workdir[BUFFER_LEN];
  char filename[BUFFER_LEN];

  snprintf(filename, sizeof filename, "%s%s", fname, options.compresssuff);

  /* Be safe in case our game directory was removed and restored,
   * in which case our inode is screwy
   */
#ifdef WIN32
  if (GetCurrentDirectory(BUFFER_LEN, workdir)) {
    if (SetCurrentDirectory(workdir) < 0)
#else
  if (getcwd(workdir, BUFFER_LEN)) {
    if (chdir(workdir) < 0)
#endif
      fprintf(stderr,
              "chdir to %s failed in db_open_write, errno %d (%s)\n",
              workdir, errno, strerror(errno));
  } else {
    /* If this fails, we probably can't write to a log, either, though */
    fprintf(stderr,
            "getcwd failed during db_open_write, errno %d (%s)\n",
            errno, strerror(errno));
  }

  pf = GC_MALLOC(sizeof *pf);

#ifdef HAVE_LIBZ
  if (*options.compressprog && strcmp(options.compressprog, "gzip") == 0) {
    pf->type = PFT_GZFILE;
    pf->handle.g = gzopen(filename, "wb");
    if (!pf->handle.g) {
      do_rawlog(LT_ERR, "Unable to open %s with libz: %s\n", filename,
                strerror(errno));
      GC_FREE(pf);
      longjmp(db_err, 1);
    }
    return pf;
  }
#endif

#ifndef WIN32
  if (*options.compressprog) {
    pf->type = PFT_PIPE;
    pf->handle.f =
#ifdef __LCC__
      (FILE *)
#endif
      popen(tprintf("%s > '%s'", options.compressprog, filename), "w");
    /* Force the pipe to be fully buffered */
    if (pf->handle.f) {
      setvbuf(pf->handle.f, NULL, _IOFBF, BUFSIZ);
    } else
      do_rawlog(LT_ERR, "Unable to run '%s > %s': %s",
                options.compressprog, filename, strerror(errno));

  } else
#endif                          /* WIN32 */
  {
    pf->type = PFT_FILE;
    pf->handle.f = fopen(filename, "wb");
    if (!pf->handle.f)
      do_rawlog(LT_ERR, "Unable to open %s: %s\n", filename, strerror(errno));
  }
  if (!pf->handle.f) {
    GC_FREE(pf);
    longjmp(db_err, 1);
  }
  return pf;
}


/** List various goodies.
 * \verbatim
 * This function implements @list.
 * \endverbatim
 * \param player the enactor.
 * \param arg what to list.
 * \param lc if 1, list in lowercase.
 */
void
do_list(dbref player, char *arg, int lc)
{
  if (!arg || !*arg)
    notify(player, T("I don't understand what you want to @list."));
  else if (string_prefix("commands", arg))
    do_list_commands(player, lc);
  else if (string_prefix("functions", arg))
    do_list_functions(player, lc);
  else if (string_prefix("motd", arg))
    do_motd(player, MOTD_LIST, "");
  else if (string_prefix("attribs", arg))
    do_list_attribs(player, lc);
  else if (string_prefix("flags", arg))
    do_list_flags("FLAG", player, "", lc, T("Flags"));
  else if (string_prefix("powers", arg))
    do_list_flags("POWER", player, "", lc, T("Powers"));
  else if (string_prefix("locks", arg))
    do_list_locks(player, NULL, lc, T("Locks"));
  else if (string_prefix("allocations", arg))
    do_list_allocations(player);
  else
    notify(player, T("I don't understand what you want to @list."));
}

extern HASHTAB htab_function;
extern HASHTAB htab_user_function;
extern HASHTAB htab_player_list;
extern HASHTAB htab_reserved_aliases;
extern HASHTAB help_files;
extern HASHTAB htab_objdata;
extern HASHTAB htab_objdata_keys;
extern HASHTAB htab_locks;
extern HASHTAB local_options;
extern StrTree atr_names;
extern StrTree lock_names;
extern StrTree object_names;
extern PTAB ptab_command;
extern PTAB ptab_attrib;
extern PTAB ptab_flag;
extern intmap *queue_map, *descs_by_fd;

/** Reports stats on various in-memory data structures.
 * \param player the enactor.
 */
void
do_list_memstats(dbref player)
{
  notify(player, "Hash Tables:");
  hash_stats_header(player);
  hash_stats(player, &htab_function, "Functions");
  hash_stats(player, &htab_user_function, "@Functions");
  hash_stats(player, &htab_player_list, "Players");
  hash_stats(player, &htab_reserved_aliases, "Aliases");
  hash_stats(player, &help_files, "HelpFiles");
  hash_stats(player, &htab_objdata, "ObjData");
  hash_stats(player, &htab_objdata_keys, "ObjDataKeys");
  hash_stats(player, &htab_locks, "@locks");
  hash_stats(player, &local_options, "ConfigOpts");
  notify(player, "Prefix Trees:");
  ptab_stats_header(player);
  ptab_stats(player, &ptab_attrib, "AttrPerms");
  ptab_stats(player, &ptab_command, "Commands");
  ptab_stats(player, &ptab_flag, "Flags");
  notify(player, "String Trees:");
  st_stats_header(player);
  st_stats(player, &atr_names, "AttrNames");
  st_stats(player, &object_names, "ObjNames");
  st_stats(player, &lock_names, "LockNames");
  notify(player, "Integer Maps:");
  im_stats_header(player);
  im_stats(player, queue_map, "Queue IDs");
  im_stats(player, descs_by_fd, "Connections");

#if (COMPRESSION_TYPE >= 3) && defined(COMP_STATS)
  if (Wizard(player)) {
    long items, used, total_comp, total_uncomp;
    double percent;
    compress_stats(&items, &used, &total_uncomp, &total_comp);
    notify(player, "---------- Internal attribute compression  ----------");
    notify_format(player,
                  "%10ld compression table items used, "
                  "taking %ld bytes.", items, used);
    notify_format(player, "%10ld bytes in text before compression. ",
                  total_uncomp);
    notify_format(player, "%10ld bytes in text AFTER  compression. ",
                  total_comp);
    percent = ((float) (total_comp)) / ((float) total_uncomp) * 100.0;
    notify_format(player,
                  "%10.0f %% text    compression ratio (lower is better). ",
                  percent);
    percent =
      ((float) (total_comp + used + (32768L * sizeof(char *)))) /
      ((float) total_uncomp) * 100.0;
    notify_format(player,
                  "%10.0f %% OVERALL compression ratio (lower is better). ",
                  percent);
    notify_format(player,
                  "          (Includes table items, and table of words pointers of %ld bytes)",
                  32768L * sizeof(char *));
    if (percent >= 100.0)
      notify(player,
             "          " "(Compression ratio improves with larger database)");
  }
#endif
}

static char *
make_new_epoch_file(const char *basename, int the_epoch)
{
  static char result[BUFFER_LEN];       /* STATIC! */
  /* Unlink the last the_epoch and create a new one */
  sprintf(result, "%s.#%d#", basename, the_epoch - 1);
  unlink(result);
  sprintf(result, "%s.#%d#", basename, the_epoch);
  return result;
}


/* Given a list of dbrefs on which a command has matched but been
 * denied by a lock, queue up the COMMAND`*FAILURE attributes, if
 * any.
 */
static int
fail_commands(dbref player)
{
  dbref *obj = errdblist;
  int size = errdbtail - errdblist;
  int matched = 0;
  while (size--) {
    matched += fail_lock(player, *obj, Command_Lock, NULL, NOTHING);
    obj++;
  }
  errdbtail = errdblist;
  return (matched > 0);
}

/* Increase the size of the errdblist - up to some maximum */
static void
errdb_grow(void)
{
  dbref *newerrdb;
  if (errdbsize >= 50)
    return;                     /* That's it, no more, forget it */
  newerrdb = GC_REALLOC(errdblist, (errdbsize + 1) * sizeof(dbref));
  if (newerrdb) {
    errdblist = newerrdb;
    errdbtail = errdblist + errdbsize;
    errdbsize += 1;
  }
}
