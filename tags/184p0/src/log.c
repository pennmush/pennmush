/**
 * \file log.c
 *
 * \brief Logging for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <errno.h>

#include "conf.h"
#include "externs.h"
#include "flags.h"
#include "dbdefs.h"
#include "htab.h"
#include "bufferq.h"
#include "log.h"
#include "confmagic.h"

static char *quick_unparse(dbref object);
static void start_log(FILE ** fp, const char *filename);
static void end_log(const char *filename);

BUFFERQ *activity_bq = NULL;

HASHTAB htab_logfiles;  /**< Hash table of logfile names and descriptors */

/* log file pointers */
FILE *connlog_fp;  /**< Connect log */
FILE *checklog_fp; /**< Checkpoint log */
FILE *wizlog_fp;   /**< Wizard log */
FILE *tracelog_fp; /**< Trace log */
FILE *cmdlog_fp;   /**< Command log */

/* From wait.c */
int lock_file(FILE *);
int unlock_file(FILE *);

static char *
quick_unparse(dbref object)
{
  static char buff[BUFFER_LEN], *bp;

  switch (object) {
  case NOTHING:
    strcpy(buff, T("*NOTHING*"));
    break;
  case AMBIGUOUS:
    strcpy(buff, T("*VARIABLE*"));
    break;
  case HOME:
    strcpy(buff, T("*HOME*"));
    break;
  default:
    bp = buff;
    safe_format(buff, &bp, "%s(#%d%s)",
                Name(object), object, unparse_flags(object, GOD));
    *bp = '\0';
  }

  return buff;
}

static void
start_log(FILE ** fp, const char *filename)
{
  static int ht_initialized = 0;
  FILE *f;

  if (!filename || !*filename) {
    *fp = stderr;
  } else {
    if (!ht_initialized) {
      hashinit(&htab_logfiles, 8);
      ht_initialized = 1;
    }
    if ((f = (FILE *) hashfind(strupper(filename), &htab_logfiles))) {
      /* We've already opened this file, so just use that pointer */
      *fp = f;
    } else {

      *fp = fopen(filename, "a");
      if (*fp == NULL) {
        fprintf(stderr, "WARNING: cannot open log %s\n", filename);
        *fp = stderr;
      } else {
        hashadd(strupper(filename), (void *) *fp, &htab_logfiles);
        fprintf(*fp, "START OF LOG.\n");
        fflush(*fp);
      }
    }
  }
}

/** Open all logfiles.
 */
void
start_all_logs(void)
{
  start_log(&connlog_fp, CONNLOG);
  start_log(&checklog_fp, CHECKLOG);
  start_log(&wizlog_fp, WIZLOG);
  start_log(&tracelog_fp, TRACELOG);
  start_log(&cmdlog_fp, CMDLOG);
}

/** Redirect stderr to a error log file and close stdout and stdin. 
 * Should be called after start_all_logs().
 * \param log name of logfile to redirect stderr to.
 */
void
redirect_streams(void)
{
  FILE *errlog_fp;

  fprintf(stderr, "Redirecting stderr to %s\n", ERRLOG);
  errlog_fp = fopen(ERRLOG, "a");
  if (!errlog_fp) {
    fprintf(stderr, "Unable to open %s. Error output to stderr.\n", ERRLOG);
  } else {
    fclose(errlog_fp);
    if (!freopen(ERRLOG, "a", stderr)) {
      printf(T("Ack!  Failed reopening stderr!"));
      exit(1);
    }
    setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
  }
#ifndef DEBUG_BYTECODE
  fclose(stdout);
#endif
  fclose(stdin);
}


static void
end_log(const char *filename)
{
  FILE *fp;
  if (!filename || !*filename)
    return;
  if ((fp = (FILE *) hashfind(strupper(filename), &htab_logfiles))) {
    lock_file(fp);
    fprintf(fp, "END OF LOG.\n");
    fflush(fp);
    fclose(fp);                 /* Implicit lock removal */
    hashdelete(strupper(filename), &htab_logfiles);
  }
}

/** Close all logfiles.
 */
void
end_all_logs(void)
{
  const char *name, *next;
  name = hash_firstentry_key(&htab_logfiles);
  while (name) {
    next = hash_nextentry_key(&htab_logfiles);
    end_log(name);
    name = next;
  }
}


/** Log a raw message.
 * take a log type and format list and args, write to appropriate logfile.
 * log types are defined in log.h
 * \param logtype type of log to print message to.
 * \param fmt format string for message.
 */
void WIN32_CDECL
do_rawlog(int logtype, const char *fmt, ...)
{
  struct tm *ttm;
  char timebuf[18];
  char tbuf1[BUFFER_LEN + 50];
  va_list args;
  FILE *f = NULL;
  va_start(args, fmt);

#ifdef HAS_VSNPRINTF
  (void) vsnprintf(tbuf1, sizeof tbuf1, fmt, args);
#else
  (void) vsprintf(tbuf1, fmt, args);
#endif
  tbuf1[BUFFER_LEN - 1] = '\0';
  va_end(args);

  ttm = localtime(&mudtime);

  strftime(timebuf, sizeof timebuf, "[%m/%d %H:%M:%S]", ttm);

  switch (logtype) {
  case LT_ERR:
    f = stderr;
    break;
  case LT_HUH:
  case LT_CMD:
    start_log(&cmdlog_fp, CMDLOG);
    f = cmdlog_fp;
    break;
  case LT_WIZ:
    start_log(&wizlog_fp, WIZLOG);
    f = wizlog_fp;
    break;
  case LT_CONN:
    start_log(&connlog_fp, CONNLOG);
    f = connlog_fp;
    break;
  case LT_TRACE:
    start_log(&tracelog_fp, TRACELOG);
    f = tracelog_fp;
    break;
  case LT_CHECK:
    start_log(&checklog_fp, CHECKLOG);
    f = checklog_fp;
    break;
  default:
    f = stderr;
    break;
  }
  lock_file(f);
  fprintf(f, "%s %s\n", timebuf, tbuf1);
  fflush(f);
  unlock_file(f);
}

/** Log a message, with useful information.
 * take a log type and format list and args, write to appropriate logfile.
 * log types are defined in log.h. Unlike do_rawlog, this version
 * tags messages with prefixes, and uses dbref information passed to it.
 * \param logtype type of log to print message to.
 * \param player dbref that generated the log message.
 * \param object second dbref involved in log message (e.g. force logs)
 * \param fmt mesage format string.
 */
void WIN32_CDECL
do_log(int logtype, dbref player, dbref object, const char *fmt, ...)
{
  /* tbuf1 had 50 extra chars because we might pass this function
   * both a label string and a command which could be up to BUFFER_LEN
   * in length - for example, when logging @forces
   */
  char tbuf1[BUFFER_LEN + 50];
  va_list args;
  char unp1[BUFFER_LEN], unp2[BUFFER_LEN];

  va_start(args, fmt);

#ifdef HAS_VSNPRINTF
  (void) vsnprintf(tbuf1, sizeof tbuf1, fmt, args);
#else
  (void) vsprintf(tbuf1, fmt, args);
#endif
  va_end(args);

  switch (logtype) {
  case LT_ERR:
    do_rawlog(logtype, "RPT: %s", tbuf1);
    break;
  case LT_CMD:
    if (!has_flag_by_name(player, "NO_LOG", NOTYPE)) {
      strcpy(unp1, quick_unparse(player));
      if (GoodObject(object)) {
        strcpy(unp2, quick_unparse(object));
        do_rawlog(logtype, "CMD: %s %s / %s: %s",
                  (Suspect(player) ? "SUSPECT" : ""), unp1, unp2, tbuf1);
      } else {
        strcpy(unp2, quick_unparse(Location(player)));
        do_rawlog(logtype, "CMD: %s %s in %s: %s",
                  (Suspect(player) ? "SUSPECT" : ""), unp1, unp2, tbuf1);
      }
    }
    break;
  case LT_WIZ:
    strcpy(unp1, quick_unparse(player));
    if (GoodObject(object)) {
      strcpy(unp2, quick_unparse(object));
      do_rawlog(logtype, "WIZ: %s --> %s: %s", unp1, unp2, tbuf1);
    } else {
      do_rawlog(logtype, "WIZ: %s: %s", unp1, tbuf1);
    }
    break;
  case LT_CONN:
    do_rawlog(logtype, "NET: %s", tbuf1);
    break;
  case LT_TRACE:
    do_rawlog(logtype, "TRC: %s", tbuf1);
    break;
  case LT_CHECK:
    do_rawlog(logtype, "%s", tbuf1);
    break;
  case LT_HUH:
    if (!controls(player, Location(player))) {
      strcpy(unp1, quick_unparse(player));
      strcpy(unp2, quick_unparse(Location(player)));
      do_rawlog(logtype, "HUH: %s in %s [%s]: %s",
                unp1, unp2,
                (GoodObject(Location(player))) ?
                Name(Owner(Location(player))) : T("bad object"), tbuf1);
    }
    break;
  default:
    do_rawlog(LT_ERR, "ERR: %s", tbuf1);
  }
}

/** Wipe out a game log. This is intended for those emergencies where
 * the log has grown out of bounds, overflowing the disk quota, etc.
 * Because someone with the god password can use this command to wipe
 * out 'intrusion' traces, we also require the log_wipe_passwd given
 * in mush.cnf
 * \param player the enactor.
 * \param logtype type of log to wipe.
 * \param str password for wiping logs.
 */
void
do_logwipe(dbref player, int logtype, char *str)
{
  if (strcmp(str, LOG_WIPE_PASSWD)) {
    const char *lname;
    switch (logtype) {
    case LT_CONN:
      lname = "connection";
      break;
    case LT_CHECK:
      lname = "checkpoint";
      break;
    case LT_CMD:
      lname = "command";
      break;
    case LT_TRACE:
      lname = "trace";
      break;
    case LT_WIZ:
      lname = "wizard";
      break;
    default:
      lname = "unspecified";
    }
    notify(player, T("Wrong password."));
    do_log(LT_WIZ, player, NOTHING,
           "Invalid attempt to wipe the %s log, password %s", lname, str);
    return;
  }
  switch (logtype) {
  case LT_CONN:
    end_log(CONNLOG);
    unlink(CONNLOG);
    start_log(&connlog_fp, CONNLOG);
    do_log(LT_ERR, player, NOTHING, "Connect log wiped.");
    break;
  case LT_CHECK:
    end_log(CHECKLOG);
    unlink(CHECKLOG);
    start_log(&checklog_fp, CHECKLOG);
    do_log(LT_ERR, player, NOTHING, "Checkpoint log wiped.");
    break;
  case LT_CMD:
    end_log(CMDLOG);
    unlink(CMDLOG);
    start_log(&cmdlog_fp, CMDLOG);
    do_log(LT_ERR, player, NOTHING, "Command log wiped.");
    break;
  case LT_TRACE:
    end_log(TRACELOG);
    unlink(TRACELOG);
    start_log(&tracelog_fp, TRACELOG);
    do_log(LT_ERR, player, NOTHING, "Trace log wiped.");
    break;
  case LT_WIZ:
    end_log(WIZLOG);
    unlink(WIZLOG);
    start_log(&wizlog_fp, WIZLOG);
    do_log(LT_ERR, player, NOTHING, "Wizard log wiped.");
    break;
  default:
    notify(player, T("That is not a valid log."));
    return;
  }
  notify(player, T("Log wiped."));
}


/** Log a message to the activity log.
 * \param type message type (an LA_* constant)
 * \param player object responsible for the message.
 * \param action message to log.
 */
void
log_activity(int type, dbref player, const char *action)
{
  if (!activity_bq)
    activity_bq = allocate_bufferq(ACTIVITY_LOG_SIZE);
  add_to_bufferq(activity_bq, type, player, action);
}

/** Retrieve the last logged message from the activity log.
 * \return last logged message or an empty string.
 */
const char *
last_activity(void)
{
  if (!activity_bq)
    return "";
  else
    return BufferQLast(activity_bq);
}

/** Retrieve the type of the last logged message from the activity log.
 * \return last type of last logged message or -1.
 */
int
last_activity_type(void)
{
  if (!activity_bq)
    return -1;
  else
    return BufferQLastType(activity_bq);
}


/** Dump out (to a player or the error log) the activity buffer queue.
 * \param player player to receive notification, if notifying.
 * \param num_lines number of lines of buffer to dump (0 = all).
 * \param dump if 1, dump to error log; if 0, notify player.
 */
void
notify_activity(dbref player, int num_lines, int dump)
{
  int type;
  dbref plr;
  time_t timestamp;
  char *buf;
  char *p = NULL;
  char *stamp;
  int skip;
  const char *typestr;

  if (!activity_bq)
    return;

  if (dump || !num_lines)
    num_lines = BufferQNum(activity_bq);
  skip = BufferQNum(activity_bq) - num_lines;

  if (dump)
    do_rawlog(LT_ERR, "Dumping recent activity:");
  else
    notify(player, T("GAME: Recall from activity log"));

  do {
    buf = iter_bufferq(activity_bq, &p, &plr, &type, &timestamp);
    if (skip <= 0) {
      if (buf) {
        stamp = show_time(timestamp, 0);
        switch (type) {
        case LA_CMD:
          typestr = "CMD";
          break;
        case LA_PE:
          typestr = "EXP";
          break;
        case LA_LOCK:
          typestr = "LCK";
          break;
        default:
          typestr = "???";
          break;
        }

        if (dump)
          do_rawlog(LT_ERR, "[%s/#%d/%s] %s", stamp, plr, typestr, buf);
        else
          notify_format(player, "[%s/#%d/%s] %s", stamp, plr, typestr, buf);
      }
    }
    skip--;
  } while (buf);


  if (!dump)
    notify(player, T("GAME: End recall"));
}

/* Wrapper for perror */
void
penn_perror(const char *err)
{
  do_rawlog(LT_ERR, "%s:%s", err, strerror(errno));
}
