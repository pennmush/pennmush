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
#include <limits.h>
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

struct log_stream;

#define LOG_BUFFER_SIZE 1

static char *quick_unparse(dbref object);
static void start_log(struct log_stream *);
static void end_log(struct log_stream *);

BUFFERQ *activity_bq = NULL;

HASHTAB htab_logfiles;  /**< Hash table of logfile names and descriptors */

#define NLOGS 7

struct log_stream logs[NLOGS] = {
  {LT_ERR, "error", ERRLOG, NULL, NULL},
  {LT_CMD, "command", CMDLOG, NULL, NULL},
  {LT_WIZ, "wizard", WIZLOG, NULL, NULL},
  {LT_CONN, "connection", CONNLOG, NULL, NULL},
  {LT_TRACE, "trace", TRACELOG, NULL, NULL},
  {LT_CHECK, "checkpoint", CHECKLOG, NULL, NULL},
  {LT_HUH, "huh", CMDLOG, NULL, NULL},
};

struct log_stream *
lookup_log(enum log_type type)
{
  int n;
  for (n = 0; n < NLOGS; n++)
    if (logs[n].type == type)
      return logs + n;
  return NULL;
}


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
start_log(struct log_stream *log)
{
  static int ht_initialized = 0;
  FILE *f;

  if (!log->filename || !*log->filename) {
    log->fp = stderr;
  } else {
    if (!ht_initialized) {
      hashinit(&htab_logfiles, 8);
      ht_initialized = 1;
    }
    if ((f = hashfind(strupper(log->filename), &htab_logfiles))) {
      /* We've already opened this file for another log, so just use that pointer */
      log->fp = f;
    } else {
      log->fp = fopen(log->filename, "a");
      if (log->fp == NULL) {
        fprintf(stderr, "WARNING: cannot open log %s: %s\n", log->filename,
                strerror(errno));
        log->fp = stderr;
      } else {
        hashadd(strupper(log->filename), log->fp, &htab_logfiles);
        fprintf(log->fp, "START OF LOG.\n");
        fflush(log->fp);
      }
    }
  }
  if (!log->buffer)
    log->buffer = allocate_bufferq(LOG_BUFFER_SIZE);
}

/** Open all logfiles.
 */
void
start_all_logs(void)
{
  int n;

  for (n = 0; n < NLOGS; n++)
    start_log(logs + n);
}

/** Redirect stderr to an error log file and close stdout and stdin.
 * Should be called after start_all_logs().
 */
void
redirect_streams(void)
{
  FILE *fp;

  fprintf(stderr, "Redirecting stderr to %s\n", ERRLOG);
  fp = fopen(ERRLOG, "a");
  if (!fp) {
    fprintf(stderr, "Unable to open %s. Error output to stderr.\n", ERRLOG);
  } else {
    fclose(fp);
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
end_log(struct log_stream *log)
{
  FILE *fp;

  if (!log->filename || !*log->filename || !log->fp)
    return;
  if ((fp = hashfind(strupper(log->filename), &htab_logfiles))) {
    int n;

    lock_file(fp);
    fprintf(fp, "END OF LOG.\n");
    fflush(fp);
    for (n = 0; n < NLOGS; n++)
      if (log->fp == fp)
        log->fp = NULL;
    fclose(fp);                 /* Implicit lock removal */
    free_bufferq(log->buffer);
    log->fp = NULL;
    log->buffer = NULL;
    hashdelete(strupper(log->filename), &htab_logfiles);
  }
}

/** Close all logfiles.
 */
void
end_all_logs(void)
{
  int n;
  for (n = 0; n < NLOGS; n++)
    end_log(logs + n);
}


/** Log a raw message.
 * take a log type and format list and args, write to appropriate logfile.
 * log types are defined in log.h
 * \param logtype type of log to print message to.
 * \param fmt format string for message.
 */
void WIN32_CDECL
do_rawlog(enum log_type logtype, const char *fmt, ...)
{
  struct log_stream *log;
  struct tm *ttm;
  char timebuf[18];
  char tbuf1[BUFFER_LEN + 50];
  va_list args;

  va_start(args, fmt);
  mush_vsnprintf(tbuf1, sizeof tbuf1, fmt, args);
  va_end(args);

  time(&mudtime);
  ttm = localtime(&mudtime);

  strftime(timebuf, sizeof timebuf, "[%m/%d %H:%M:%S]", ttm);

  log = lookup_log(logtype);

  if (!log->fp) {
    fprintf(stderr, "Attempt to write to %s log before it was started!\n",
            log->name);
    start_log(log);
  }

  lock_file(log->fp);
  fprintf(log->fp, "%s %s\n", timebuf, tbuf1);
  fflush(log->fp);
  unlock_file(log->fp);
  add_to_bufferq(log->buffer, logtype, GOD, tbuf1);
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
do_log(enum log_type logtype, dbref player, dbref object, const char *fmt, ...)
{
  /* tbuf1 had 50 extra chars because we might pass this function
   * both a label string and a command which could be up to BUFFER_LEN
   * in length - for example, when logging @forces
   */
  char tbuf1[BUFFER_LEN + 50];
  va_list args;
  char unp1[BUFFER_LEN], unp2[BUFFER_LEN];

  va_start(args, fmt);
  mush_vsnprintf(tbuf1, sizeof tbuf1, fmt, args);
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

/** Recall lines from a log.
 *
 * \param player the enactor.
 * \param type the log to recall from.
 * \param lines the number of lines to recall. 0 for all.
 */
void
do_log_recall(dbref player, enum log_type type, int lines)
{
  dbref dummy_dbref = NOTHING;
  int dummy_type = 0, nlines = 0;
  time_t dummy_ts;
  char *line, *p;
  struct log_stream *log;

  if (lines <= 0)
    lines = INT_MAX;

  log = lookup_log(type);

  if (lines != INT_MAX) {
    p = NULL;
    while (iter_bufferq(log->buffer, &p, &dummy_dbref, &dummy_type, &dummy_ts))
      nlines += 1;
  } else
    nlines = INT_MAX;

  notify(player, T("Begin log recall."));
  p = NULL;
  while ((line =
          iter_bufferq(log->buffer, &p, &dummy_dbref, &dummy_type,
                       &dummy_ts))) {
    if (nlines <= lines)
      notify(player, line);
    nlines -= 1;
  }
  notify(player, T("End log recall."));
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
do_logwipe(dbref player, enum log_type logtype, char *str)
{
  struct log_stream *log;

  log = lookup_log(logtype);

  if (strcmp(str, LOG_WIPE_PASSWD)) {
    notify(player, T("Wrong password."));
    do_log(LT_WIZ, player, NOTHING,
           "Invalid attempt to wipe the %s log, password %s", log->name, str);
    return;
  }
  switch (logtype) {
  case LT_CONN:
  case LT_CHECK:
  case LT_CMD:
  case LT_TRACE:
  case LT_WIZ:
    end_log(log);
    unlink(log->filename);
    start_log(log);
    do_log(LT_ERR, player, NOTHING, "%s log wiped.", log->name);
    break;
  default:
    notify(player, T("That is not a clearable log."));
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
log_activity(enum log_act_type type, dbref player, const char *action)
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
