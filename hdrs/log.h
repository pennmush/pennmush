/**
 * \file log.h
 *
 * \brief Stuff used for logging. Duh.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include "bufferq.h"
#include "mushtype.h"

/* log types */
enum log_type {
  LT_ERR,   /**< Error log, for most output */
  LT_CMD,   /**< \@-commands which are CMD_T_LOGNAME are printed here */
  LT_WIZ,   /**< Stuff done by Wizards is logged here */
  LT_CONN,  /**< Logging of Connections */
  LT_TRACE, /**< Debugging log */
  LT_CHECK, /**< No idea? */
  LT_HUH    /**< Logs of huh_command's */
};

/** Log levels. Used for syslog logging. */
enum log_level {
  MLOG_EMERG,   /**< MUSH is unusable */
  MLOG_ALERT,   /**< action must be taken immediately */
  MLOG_CRIT,    /**< critical conditions */
  MLOG_ERR,     /**< error conditions */
  MLOG_WARNING, /**< warning conditions */
  MLOG_NOTICE,  /**< normal but significant condition */
  MLOG_INFO,    /**< informational message */
  MLOG_DEBUG    /**< debug-level message */
};

/** A logfile stream */
struct log_stream {
  enum log_type type;   /**< Log type */
  const char *name;     /**< String to refer to log */
  const char *filename; /**< Filename to log to */
  FILE *fp;             /**< Pointer returned by fopen() to write to */
  BUFFERQ *buffer;      /**< bufferq to store recently logged strings in */
  const char
    *event; /**< name of an event attribute to queue with the message. */
};

/* From log.c */
struct log_stream *lookup_log(enum log_type);
void start_all_logs(void);
void end_all_logs(void);
void reopen_logs(void);
void WIN32_CDECL do_log(enum log_type logtype, dbref player, dbref object,
                        const char *fmt, ...)
  __attribute__((__format__(__printf__, 4, 5)));
void do_rawlog_vlvl(enum log_type logtype, enum log_level loglevel,
                    const char *fmt, va_list args);
void WIN32_CDECL do_rawlog_lvl(enum log_type logtype, enum log_level loglevel,
                               const char *fmt, ...)
  __attribute__((__format__(__printf__, 3, 4)));
void WIN32_CDECL do_rawlog(enum log_type logtype, const char *fmt, ...)
  __attribute__((__format__(__printf__, 2, 3)));

enum logwipe_policy { LOGWIPE_WIPE, LOGWIPE_TRIM, LOGWIPE_ROTATE };
void do_logwipe(dbref, enum log_type, const char *, enum logwipe_policy);
void do_log_recall(dbref, enum log_type, int);

/* Activity log types */
enum log_act_type { LA_CMD, LA_PE, LA_LOCK };
#define ACTIVITY_LOG_SIZE 3 /* In BUFFER_LEN-size lines */
void log_activity(enum log_act_type type, dbref player, const char *action);
void notify_activity(dbref player, int num_lines, int dump);
const char *last_activity(void);
int last_activity_type(void);

void penn_perror(const char *);

#endif /* LOG_H */
