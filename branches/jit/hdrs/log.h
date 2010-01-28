#ifndef LOG_H
#define LOG_H

/* log types */
#define LT_ERR    0
#define LT_CMD    1
#define LT_WIZ    2
#define LT_CONN   3
#define LT_TRACE  4
#define LT_RPAGE  5             /* Obsolete */
#define LT_CHECK  6
#define LT_HUH    7

/* From log.c */
extern void start_all_logs(void);
extern void end_all_logs(void);
extern void redirect_streams(void);
extern void WIN32_CDECL do_log
  (int logtype, dbref player, dbref object, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 4, 5)));
extern void WIN32_CDECL do_rawlog(int logtype, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 2, 3)));
extern void do_logwipe(dbref player, int logtype, char *str);

/* Activity log types */
#define LA_CMD  0
#define LA_PE   1
#define LA_LOCK 2
#define ACTIVITY_LOG_SIZE 3     /* In BUFFER_LEN-size lines */
extern void log_activity(int type, dbref player, const char *action);
extern void notify_activity(dbref player, int num_lines, int dump);
extern const char *last_activity(void);
extern int last_activity_type(void);

#endif                          /* LOG_H */
