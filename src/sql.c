/**
 * \file sql.c
 *
 * \brief Code to support PennMUSH connection to SQL databases.
 *
 * \verbatim
 * Each sql database we support must define its own set of the
 * following functions:
 *
 *  penn_<db>_sql_init
 *  penn_<db>_sql_connected
 *  penn_<db>_sql_errormsg
 *  penn_<db>_sql_shutdown
 *  penn_<db>_sql_query
 *  penn_<db>_free_sql_query
 *
 * We define generic functions (named as above, but without <db>_)
 * that determine the platform and call the appropriate platform-specific
 * function. We also define the softcode interfaces:
 *
 *  fun_sql_escape
 *  fun_sql
 *  fun_mapsql
 *  cmd_sql
 *
 * \endverbatim
 */

#include "copyrite.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#define sleep Sleep
#endif

#ifdef HAVE_MYSQL
#include <mysql.h>
#include <errmsg.h>
static MYSQL *mysql_connp = NULL;
#endif

#ifdef HAVE_POSTGRESQL
#include <libpq-fe.h>
static PGconn *postgres_connp = NULL;
#endif

#ifdef HAVE_SQLITE3
#include "sqlite3.h"
static sqlite3 *sqlite3_connp = NULL;
#endif

#include "ansi.h"
#include "command.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "function.h"
#include "log.h"
#include "match.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "notify.h"
#include "parse.h"
#include "strutil.h"
#include "charconv.h"
#include "mushsql.h"
#include "charclass.h"

/* Supported platforms */
typedef enum {
  SQL_PLATFORM_DISABLED = -1,
  SQL_PLATFORM_MYSQL = 1,
  SQL_PLATFORM_POSTGRESQL,
  SQL_PLATFORM_SQLITE3
} sqlplatform;

/* Number of times to try a connection */
#define SQL_RETRY_TIMES 3

#define sql_test_result(qres)                                                  \
  if (!qres) {                                                                 \
    if (affected_rows >= 0) {                                                  \
    } else if (!sql_connected()) {                                             \
      safe_str(T("#-1 SQL ERROR: NO DATABASE CONNECTED"), buff, bp);           \
    } else {                                                                   \
      safe_format(buff, bp, T("#-1 SQL ERROR: %s"), sql_error());              \
    }                                                                          \
    return;                                                                    \
  }

#ifdef HAVE_MYSQL
static MYSQL_RES *penn_mysql_sql_query(const char *, int *);
static void penn_mysql_free_sql_query(MYSQL_RES *qres);
static int penn_mysql_sql_init(void);
static void penn_mysql_sql_shutdown(void);
static int penn_mysql_sql_connected(void);
#endif
#ifdef HAVE_POSTGRESQL
static PGresult *penn_pg_sql_query(const char *, int *);
static void penn_pg_free_sql_query(PGresult *qres);
static int penn_pg_sql_init(void);
static void penn_pg_sql_shutdown(void);
static int penn_pg_sql_connected(void);
#endif
#ifdef HAVE_SQLITE3
static int penn_sqlite3_sql_init(void);
static void penn_sqlite3_sql_shutdown(void);
static int penn_sqlite3_sql_connected(void);
static sqlite3_stmt *penn_sqlite3_sql_query(const char *, int *);
static void penn_sqlite3_free_sql_query(sqlite3_stmt *);
#endif
static sqlplatform sql_platform(void);
static char *sql_sanitize(const char *res);
#define SANITIZE(s, n) ((s && *s) ? mush_strdup(sql_sanitize(s), n) : NULL)

static char *
SANITIZEUTF8(const char *restrict s, const char *restrict n)
{
  if (s && *s) {
    const char *san = sql_sanitize(s);
    return utf8_to_latin1(san, strlen(san), NULL, 0, n);
  } else {
    return NULL;
  }
}

/* A helper function to translate SQL_PLATFORM into one of our
 * supported platform codes. We remember this value, so a reboot
 * is necessary to change it.
 */
static sqlplatform
sql_platform(void)
{
  static sqlplatform platform = SQL_PLATFORM_DISABLED;
#ifdef HAVE_MYSQL
  if (!strcasecmp(SQL_PLATFORM, "mysql"))
    platform = SQL_PLATFORM_MYSQL;
#endif
#ifdef HAVE_POSTGRESQL
  if (!strcasecmp(SQL_PLATFORM, "postgres"))
    platform = SQL_PLATFORM_POSTGRESQL;
  if (!strcasecmp(SQL_PLATFORM, "postgresql"))
    platform = SQL_PLATFORM_POSTGRESQL;
#endif
#ifdef HAVE_SQLITE3
  if (!strcasecmp(SQL_PLATFORM, "sqlite"))
    platform = SQL_PLATFORM_SQLITE3;
  if (!strcasecmp(SQL_PLATFORM, "sqlite3"))
    platform = SQL_PLATFORM_SQLITE3;
#endif
  return platform;
}

static char *
sql_sanitize(const char *res)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  const char *rp = res;

  if (!res || !*res) {
    buff[0] = '\0';
    return buff;
  }

  for (; *rp; rp++) {
    if (char_isprint(*rp) || *rp == '\n' || *rp == '\t' || *rp == ESC_CHAR ||
        *rp == TAG_START || *rp == TAG_END || *rp == BEEP_CHAR) {
      *bp++ = *rp;
    }
  }
  *bp = '\0';

  if (has_markup(buff)) {
    ansi_string *as = parse_ansi_string(buff);
    bp = buff;
    safe_ansi_string(as, 0, as->len, buff, &bp);
    free_ansi_string(as);
    *bp = '\0';
  }

  return buff;
}

/* Initialize a connection to an SQL database */
static int
sql_init(void)
{
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    return penn_mysql_sql_init();
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    return penn_pg_sql_init();
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    return penn_sqlite3_sql_init();
#endif
  default:
    return 0;
  }
}

/* Check if a connection exists */
static int
sql_connected(void)
{
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    return penn_mysql_sql_connected();
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    return penn_pg_sql_connected();
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    return penn_sqlite3_sql_connected();
#endif
  default:
    return 0;
  }
}

/* Return an error string if needed */
static const char *
sql_error(void)
{
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    return mysql_error(mysql_connp);
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    return PQerrorMessage(postgres_connp);
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    return sqlite3_errmsg(sqlite3_connp);
#endif
  default:
    return NULL;
  }
}

/** Shut down a connection to an SQL database */
void
sql_shutdown(void)
{
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    penn_mysql_sql_shutdown();
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    penn_pg_sql_shutdown();
    break;
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    penn_sqlite3_sql_shutdown();
    break;
#endif
  default:
    return;
  }
}

static void
free_sql_query(void *queryp __attribute__((__unused__)))
{
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    penn_mysql_free_sql_query((MYSQL_RES *) queryp);
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    penn_pg_free_sql_query((PGresult *) queryp);
    break;
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    penn_sqlite3_free_sql_query((sqlite3_stmt *) queryp);
    break;
#endif
  default:
    return;
  }
}

static void *
sql_query(const char *query_str __attribute__((__unused__)),
          int *affected_rows __attribute__((__unused__)))
{
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    return penn_mysql_sql_query(query_str, affected_rows);
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    return penn_pg_sql_query(query_str, affected_rows);
    break;
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    return penn_sqlite3_sql_query(query_str, affected_rows);
#endif
  default:
    return NULL;
  }
}

FUNCTION(fun_sql_escape)
{
  char bigbuff[BUFFER_LEN * 2 + 1];
  int chars_written;
  if (sql_platform() == SQL_PLATFORM_DISABLED) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!Sql_Ok(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (!args[0] || !*args[0])
    return;
  if (!sql_connected()) {
    sql_init();
    if (!sql_connected()) {
      notify(executor, T("No SQL database connection."));
      safe_str("#-1", buff, bp);
      return;
    }
  }
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    chars_written =
      mysql_real_escape_string(mysql_connp, bigbuff, args[0], arglens[0]);
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    chars_written =
      PQescapeStringConn(postgres_connp, bigbuff, args[0], arglens[0], NULL);
    break;
#endif
  case SQL_PLATFORM_SQLITE3: {
    sqlite3_stmt *escaper;
    char *utf8;
    int status, ulen;

    escaper =
      prepare_statement(sqlite3_connp, "VALUES (quote(?))", "fun.sqlescape");
    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    if (!utf8) {
      safe_str("#-1 ENCODING ERROR", buff, bp);
      return;
    }
    sqlite3_bind_text(escaper, 1, utf8, ulen, free_string);
    do {
      status = sqlite3_step(escaper);
      if (status == SQLITE_ROW) {
        const char *escaped;
        char *latin1;
        int elen;
        escaped = (const char *) sqlite3_column_text(escaper, 0);
        elen = sqlite3_column_bytes(escaper, 0);
        latin1 = utf8_to_latin1_us(escaped, elen, &chars_written, 0, "string");
        latin1[chars_written - 1] = '\0';
        chars_written -= 2;
        mush_strncpy(bigbuff, latin1 + 1, sizeof bigbuff);
        mush_free(latin1, "string");
      }
    } while (is_busy_status(status));
    sqlite3_reset(escaper);
    if (status != SQLITE_ROW) {
      safe_str("#-1 ENCODING ERROR", buff, bp);
      return;
    }
  } break;
  default:
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (chars_written == 0)
    return;
  else if (chars_written < BUFFER_LEN)
    safe_str(sql_sanitize(bigbuff), buff, bp);
  else
    safe_str(T("#-1 TOO LONG"), buff, bp);
}

COMMAND(cmd_mapsql)
{
#ifdef HAVE_MYSQL
  MYSQL_FIELD *fields = NULL;
#endif
  void *qres;
  int affected_rows = -1;
  int rownum;
  int numfields;
  int numrows;
  int useable_fields = 0;
  PE_REGS *pe_regs = NULL;
  char *names[MAX_STACK_ARGS];
  char *cells[MAX_STACK_ARGS];
  char tbuf[BUFFER_LEN];
  char strrownum[20];
  char *s;
  int i, a;
  dbref thing;
  int dofieldnames = SW_ISSET(sw, SWITCH_COLNAMES);
  int donotify = SW_ISSET(sw, SWITCH_NOTIFY);
  dbref triggerer = executor;
  int spoof = SW_ISSET(sw, SWITCH_SPOOF);
  int queue_type = QUEUE_DEFAULT | (queue_entry->queue_type & QUEUE_EVENT);

  if (!arg_right || !*arg_right) {
    notify(executor, T("What do you want to query?"));
    return;
  }

  /* Find and fetch the attribute, first. */
  mush_strncpy(tbuf, arg_left, sizeof tbuf);

  s = strchr(tbuf, '/');
  if (!s) {
    notify(executor, T("I need to know what attribute to trigger."));
    return;
  }
  *(s++) = '\0';
  upcasestr(s);

  thing = noisy_match_result(executor, tbuf, NOTYPE, MAT_EVERYTHING);

  if (thing == NOTHING) {
    return;
  }

  if (!controls(executor, thing)) {
    if (spoof || !(Owns(executor, thing) && LinkOk(thing))) {
      notify(executor, T("Permission denied."));
      return;
    }
  }

  if (spoof)
    triggerer = enactor;

  if (God(thing) && !God(executor)) {
    notify(executor, T("You can't trigger God!"));
    return;
  }

  for (a = 0; a < MAX_STACK_ARGS; a++) {
    cells[a] = NULL;
    names[a] = NULL;
  }

  /* Do the query. */
  qres = sql_query(arg_right, &affected_rows);

  if (!qres) {
    if (affected_rows >= 0) {
      notify_format(executor, T("SQL: %d rows affected."), affected_rows);
    } else if (!sql_connected()) {
      notify(executor, T("No SQL database connection."));
    } else {
      notify_format(executor, T("SQL: Error: %s"), sql_error());
    }
    return;
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    affected_rows = mysql_affected_rows(mysql_connp);
    numfields = mysql_num_fields(qres);
    numrows = INT_MAX; /* Using mysql_use_result() doesn't know the number
                          of rows ahead of time. */
    fields = mysql_fetch_fields(qres);
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    numfields = PQnfields(qres);
    numrows = PQntuples(qres);
    break;
#endif
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
  default:
    goto finished;
  }

  if (numfields > 0) {
    if (numfields > (MAX_STACK_ARGS - 1))
      useable_fields = MAX_STACK_ARGS - 1;
    else
      useable_fields = numfields;
  }
  pe_regs = pe_regs_create(PE_REGS_ARG | PE_REGS_Q, "cmd_mapsql");
  for (rownum = 0; rownum < numrows; rownum++) {
#ifdef HAVE_MYSQL
    MYSQL_ROW row_p = NULL;
    if (sql_platform() == SQL_PLATFORM_MYSQL) {
      row_p = mysql_fetch_row(qres);
      if (!row_p)
        break;
    }
#endif
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW) {
        notify_format(executor, T("SQL: Error: %s"), sqlite3_errstr(retcode));
        break;
      }
    }

    if (numfields > 0) {
      for (i = 0; i < useable_fields; i++) {
        switch (sql_platform()) {
#ifdef HAVE_MYSQL
        case SQL_PLATFORM_MYSQL:
          cells[i + 1] = SANITIZE(row_p[i], "sql_row");
          names[i + 1] = SANITIZE(fields[i].name, "sql_fieldname");
          break;
#endif
#ifdef HAVE_POSTGRESQL
        case SQL_PLATFORM_POSTGRESQL:
          cells[i + 1] = SANITIZE(PQgetvalue(qres, rownum, i), "sql_row");
          names[i + 1] = SANITIZE(PQfname(qres, i), "sql_fieldname");
          break;
#endif
        case SQL_PLATFORM_SQLITE3:
          cells[i + 1] =
            SANITIZEUTF8((char *) sqlite3_column_text(qres, i), "sql_row");
          names[i + 1] = SANITIZEUTF8((char *) sqlite3_column_name(qres, i),
                                      "sql_fieldname");
          break;
        default:
          /* Not reached, shuts up compiler */
          break;
        }
      }

      if ((rownum == 0) && dofieldnames) {
        /* Queue 0: <names> */
        snprintf(strrownum, 20, "%d", 0);
        names[0] = strrownum;
        for (i = 0; i < useable_fields + 1; i++) {
          pe_regs_setenv(pe_regs, i, names[i]);
        }
        pe_regs_qcopy(pe_regs, queue_entry->pe_info->regvals);
        queue_attribute_base_priv(thing, s, triggerer, 0, pe_regs, queue_type,
                                  executor, NULL, NULL);
      }

      /* Queue the rest. */
      snprintf(strrownum, 20, "%d", rownum + 1);
      cells[0] = strrownum;
      pe_regs_clear(pe_regs);
      for (i = 0; i < useable_fields + 1; i++) {
        pe_regs_setenv(pe_regs, i, cells[i]);
        if (i && !is_strict_integer(names[i]))
          pe_regs_set(pe_regs, PE_REGS_ARG, names[i], cells[i]);
      }
      pe_regs_qcopy(pe_regs, queue_entry->pe_info->regvals);
      queue_attribute_base_priv(thing, s, triggerer, 0, pe_regs, queue_type,
                                executor, NULL, NULL);
      for (i = 0; i < useable_fields; i++) {
        if (cells[i + 1])
          mush_free(cells[i + 1], "sql_row");
        if (names[i + 1])
          mush_free(names[i + 1], "sql_fieldname");
      }
    } else {
      /* What to do if there are no fields? This should be an error?. */
      /* notify_format(executor, T("Row %d: NULL"), rownum + 1); */
    }
  }
  if (donotify) {
    parse_que(executor, executor, "@notify me", NULL);
  }

finished:
  if (pe_regs)
    pe_regs_free(pe_regs);
  free_sql_query(qres);
}

COMMAND(cmd_sql)
{
#ifdef HAVE_MYSQL
  MYSQL_FIELD *fields = NULL;
#endif
  void *qres;
  int affected_rows = -1;
  int rownum;
  int numfields;
  int numrows;
  bool free_cell = 0;
  char *cell = NULL;
  char *name = NULL;
  char tbuf[BUFFER_LEN];
  char *tbp;
  ansi_string *as;
  int i;

  if (sql_platform() == SQL_PLATFORM_DISABLED) {
    notify(executor, T("No SQL database connection."));
    return;
  }

  if (!arg_left || !*arg_left) {
    notify(executor, T("What do you want to query?"));
    return;
  }

  qres = sql_query(arg_left, &affected_rows);

  if (!qres) {
    if (affected_rows >= 0) {
      notify_format(executor, T("SQL: %d rows affected."), affected_rows);
    } else if (!sql_connected()) {
      notify(executor, T("No SQL database connection."));
    } else {
      notify_format(executor, T("SQL: Error: %s"), sql_error());
    }
    return;
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    affected_rows = mysql_affected_rows(mysql_connp);
    numfields = mysql_num_fields(qres);
    numrows = INT_MAX; /* Using mysql_use_result() doesn't know the number
                          of rows ahead of time. */
    fields = mysql_fetch_fields(qres);
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    numfields = PQnfields(qres);
    numrows = PQntuples(qres);
    break;
#endif
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
  default:
    goto finished;
  }

  for (rownum = 0; rownum < numrows; rownum++) {
#ifdef HAVE_MYSQL
    MYSQL_ROW row_p = NULL;
    if (sql_platform() == SQL_PLATFORM_MYSQL) {
      row_p = mysql_fetch_row(qres);
      if (!row_p)
        break;
    }
#endif
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW) {
        notify_format(executor, T("SQL: Error: %s"), sqlite3_errstr(retcode));
        break;
      }
    }

    if (numfields > 0) {
      for (i = 0; i < numfields; i++) {
        switch (sql_platform()) {
#ifdef HAVE_MYSQL
        case SQL_PLATFORM_MYSQL:
          cell = row_p[i];
          name = fields[i].name;
          break;
#endif
#ifdef HAVE_POSTGRESQL
        case SQL_PLATFORM_POSTGRESQL:
          cell = PQgetvalue(qres, rownum, i);
          name = PQfname(qres, i);
          break;
#endif
        case SQL_PLATFORM_SQLITE3: {
          const char *c;
          int clen;
          c = (const char *) sqlite3_column_text(qres, i);
          clen = sqlite3_column_bytes(qres, i);
          cell = utf8_to_latin1(c, clen, NULL, 0, "string");
          name = (char *) sqlite3_column_name(qres, i);
          free_cell = 1;
        } break;
        default:
          /* Not reached, shuts up compiler */
          break;
        }
        if (cell && *cell) {
          char *newcell = sql_sanitize(cell);
          if (strchr(newcell, TAG_START) || strchr(newcell, ESC_CHAR)) {
            /* Either old or new style ANSI string. */
            tbp = tbuf;
            as = parse_ansi_string(newcell);
            safe_ansi_string(as, 0, as->len, tbuf, &tbp);
            *tbp = '\0';
            free_ansi_string(as);
            if (free_cell) {
              mush_free(cell, "string");
              free_cell = 0;
            }
            cell = tbuf;
          }
        }
        notify_format(executor, T("Row %d, Field %s: %s"), rownum + 1, name,
                      (cell && *cell) ? cell : "NULL");
        if (free_cell) {
          mush_free(cell, "string");
          free_cell = 0;
        }
      }
    } else {
      notify_format(executor, T("Row %d: NULL"), rownum + 1);
    }
  }

finished:
  if (free_cell) {
    mush_free(cell, "string");
  }
  free_sql_query(qres);
}

FUNCTION(fun_mapsql)
{
  void *qres;
  ufun_attrib ufun;
  const char *osep = " ";
  int affected_rows;
  int rownum;
  int numfields, numrows;
  char rbuff[BUFFER_LEN];
  int funccount = 0;
  int do_fieldnames = 0;
  int i;
  int useable_fields = 0;
  char **fieldnames = NULL;
  char *cell = NULL;
  bool free_cell = 0;
  PE_REGS *pe_regs = NULL;
#ifdef HAVE_MYSQL
  MYSQL_FIELD *fields = NULL;
#endif
  if (sql_platform() == SQL_PLATFORM_DISABLED) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!Sql_Ok(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!args[0] || !*args[0])
    return;

  if (!fetch_ufun_attrib(args[0], executor, &ufun, UFUN_DEFAULT))
    return;
  if (nargs > 2) {
    /* we have an output separator in args[2]. */
    osep = args[2];
  }

  if (nargs > 3) {
    /* args[3] contains a boolean, if we should pass
     * the field names first. */
    do_fieldnames = parse_boolean(args[3]);
  }

  qres = sql_query(args[1], &affected_rows);
  sql_test_result(qres);
  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    numfields = mysql_num_fields(qres);
    numrows = INT_MAX;
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    numfields = PQnfields(qres);
    numrows = PQntuples(qres);
    break;
#endif
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
  default:
    goto finished;
  }

  if (numfields < (MAX_STACK_ARGS - 1))
    useable_fields = numfields;
  else
    useable_fields = MAX_STACK_ARGS - 1;

  fieldnames = mush_calloc(sizeof(char *), useable_fields, "sql_fieldnames");

  pe_regs = pe_regs_create(PE_REGS_ARG, "fun_mapsql");
#ifdef HAVE_MYSQL
  if (sql_platform() == SQL_PLATFORM_MYSQL)
    fields = mysql_fetch_fields(qres);
#endif
  for (i = 0; i < useable_fields; i++) {
    switch (sql_platform()) {
#ifdef HAVE_MYSQL
    case SQL_PLATFORM_MYSQL:
      fieldnames[i] =
        mush_strdup(sql_sanitize(fields[i].name), "sql_fieldname");
      break;
#endif
#ifdef HAVE_POSTGRESQL
    case SQL_PLATFORM_POSTGRESQL:
      fieldnames[i] =
        mush_strdup(sql_sanitize(PQfname(qres, i)), "sql_fieldname");
      break;
#endif
    case SQL_PLATFORM_SQLITE3: {
      const char *s = sql_sanitize((const char *) sqlite3_column_name(qres, i));
      fieldnames[i] = utf8_to_latin1(s, strlen(s), NULL, 0, "sql_fieldname");
    } break;
    default:
      break;
    }
  }
  if (do_fieldnames) {
    pe_regs_setenv(pe_regs, 0, "0");
    for (i = 0; i < useable_fields; i++)
      pe_regs_setenv_nocopy(pe_regs, i + 1, fieldnames[i]);
    if (call_ufun(&ufun, rbuff, executor, enactor, pe_info, pe_regs))
      goto finished;
    safe_str(rbuff, buff, bp);
  }

  for (rownum = 0; rownum < numrows; rownum++) {
#ifdef HAVE_MYSQL
    MYSQL_ROW row_p = NULL;
    if (sql_platform() == SQL_PLATFORM_MYSQL) {
      row_p = mysql_fetch_row(qres);
      if (!row_p)
        break;
    }
#endif
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW)
        goto finished;
    }
    if (rownum > 0 || do_fieldnames) {
      safe_str(osep, buff, bp);
    }
    pe_regs_clear(pe_regs);
    pe_regs_setenv(pe_regs, 0, unparse_integer(rownum + 1));
    for (i = 0; i < useable_fields; i++) {
      switch (sql_platform()) {
#ifdef HAVE_MYSQL
      case SQL_PLATFORM_MYSQL:
        cell = row_p[i];
        break;
#endif
#ifdef HAVE_POSTGRESQL
      case SQL_PLATFORM_POSTGRESQL:
        cell = PQgetvalue(qres, rownum, i);
        break;
#endif
      case SQL_PLATFORM_SQLITE3: {
        const char *c = (const char *) sqlite3_column_text(qres, i);
        int clen = sqlite3_column_bytes(qres, i);
        cell = utf8_to_latin1(c, clen, NULL, 0, "string");
        free_cell = 1;
      } break;
      default:
        break;
      }
      if (cell && *cell) {
        /* TODO: Fix this mess */
        char *newcell = sql_sanitize(cell);
        if (free_cell) {
          mush_free(cell, "string");
          free_cell = 0;
          cell = newcell;
        }
      }
      if (cell && *cell) {
        pe_regs_setenv(pe_regs, i + 1, cell);
        if (*fieldnames[i] && !is_strict_integer(fieldnames[i]))
          pe_regs_set(pe_regs, PE_REGS_ARG, fieldnames[i], cell);
      }
    }
    /* Now call the ufun. */
    if (call_ufun(&ufun, rbuff, executor, enactor, pe_info, pe_regs))
      goto finished;
    if (safe_str(rbuff, buff, bp) && funccount == pe_info->fun_invocations)
      goto finished;
    funccount = pe_info->fun_invocations;
  }
finished:
  if (free_cell) {
    mush_free(cell, "string");
  }
  if (pe_regs) {
    pe_regs_free(pe_regs);
  }
  if (fieldnames) {
    for (i = 0; i < useable_fields; i++) {
      mush_free(fieldnames[i], "sql_fieldname");
    }
    mush_free(fieldnames, "sql_fieldnames");
  }
  free_sql_query(qres);
}

FUNCTION(fun_sql)
{
  void *qres;
  const char *rowsep = " ";
  const char *fieldsep = " ";
  char tbuf[BUFFER_LEN], *tbp;
  char *cell = NULL;
  int affected_rows;
  int rownum;
  int i;
  int numfields, numrows;
  ansi_string *as;
  char *qreg_save = NULL;
  bool free_cell = 0;

  if (sql_platform() == SQL_PLATFORM_DISABLED) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (!Sql_Ok(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!args[0] || !*args[0])
    return;

  if (nargs >= 2) {
    /* we have a row separator in args[1]. */
    rowsep = args[1];
  }

  if (nargs >= 3) {
    /* we have a field separator in args[2]. Got to parse it */
    fieldsep = args[2];
  }

  if (nargs >= 4) {
    /* return affected rows to the qreg in args[3]. */
    if (args[3][0]) {
      if (ValidQregName(args[3])) {
        qreg_save = args[3];
      }
    }
  }

  qres = sql_query(args[0], &affected_rows);
  if (qreg_save && affected_rows >= 0) {
    PE_Setq(pe_info, qreg_save, unparse_number(affected_rows));
  }
  sql_test_result(qres);
  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    numfields = mysql_num_fields(qres);
    numrows = INT_MAX;
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    numfields = PQnfields(qres);
    numrows = PQntuples(qres);
    break;
#endif
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
  default:
    goto finished;
  }

  for (rownum = 0; rownum < numrows; rownum++) {
#ifdef HAVE_MYSQL
    MYSQL_ROW row_p = NULL;
    if (sql_platform() == SQL_PLATFORM_MYSQL) {
      row_p = mysql_fetch_row(qres);
      if (!row_p)
        break;
    }
#endif
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW)
        break;
    }
    if (rownum > 0)
      safe_str(rowsep, buff, bp);
    for (i = 0; i < numfields; i++) {
      if (i > 0) {
        if (safe_str(fieldsep, buff, bp))
          goto finished;
      }
      switch (sql_platform()) {
#ifdef HAVE_MYSQL
      case SQL_PLATFORM_MYSQL:
        cell = row_p[i];
        break;
#endif
#ifdef HAVE_POSTGRESQL
      case SQL_PLATFORM_POSTGRESQL:
        cell = PQgetvalue(qres, rownum, i);
        break;
#endif
      case SQL_PLATFORM_SQLITE3: {
        const char *c = (const char *) sqlite3_column_text(qres, i);
        int clen = sqlite3_column_bytes(qres, i);
        cell = utf8_to_latin1(c, clen, NULL, 0, "string");
        free_cell = 1;
      } break;
      default:
        break;
      }
      if (cell && *cell) {
        char *newcell = sql_sanitize(cell);
        if (free_cell) {
          free_cell = 0;
          mush_free(cell, "string");
        }
        cell = newcell;
        if (strchr(cell, TAG_START) || strchr(cell, ESC_CHAR)) {
          /* Either old or new style ANSI string. */
          tbp = tbuf;
          as = parse_ansi_string(cell);
          safe_ansi_string(as, 0, as->len, tbuf, &tbp);
          *tbp = '\0';
          free_ansi_string(as);
          cell = tbuf;
        }
        if (safe_str(cell, buff, bp)) {
          goto finished; /* We filled the buffer, best stop */
        }
      }
      if (free_cell) {
        mush_free(cell, "string");
        free_cell = 0;
      }
    }
  }
finished:
  if (free_cell) {
    mush_free(cell, "string");
  }
  free_sql_query(qres);
}

/* MYSQL-specific functions */
#ifdef HAVE_MYSQL

static void
penn_mysql_sql_shutdown(void)
{
  if (!mysql_connp)
    return;
  mysql_close(mysql_connp);
  mysql_connp = NULL;
}

static int
penn_mysql_sql_connected(void)
{
  return mysql_connp ? 1 : 0;
}

static int
penn_mysql_sql_init(void)
{
  int retries = SQL_RETRY_TIMES;
  static time_t last_retry = 0;
  char sql_host[BUFFER_LEN], *p;
  int sql_port = 3306;
  time_t curtime;

#ifdef HAVE_GETSERVBYNAME
  /* Get port from /etc/services if present just in case it's been
     changed from the usual default. */
  struct servent *s = getservbyname("mysql", "tcp");
  if (s) {
    sql_port = s->s_port;
  }
#endif

  /* Only retry at most once per minute. */
  curtime = time(NULL);
  if (curtime < (last_retry + 60))
    return 0;
  last_retry = curtime;

  /* If we are already connected, drop and retry the connection, in
   * case for some reason the server went away.
   */
  if (penn_mysql_sql_connected()) {
    penn_mysql_sql_shutdown();
  }

  /* Parse SQL_HOST into sql_host and sql_port */
  mush_strncpy(sql_host, SQL_HOST, BUFFER_LEN);
  if ((p = strchr(sql_host, ':'))) {
    *p++ = '\0';
    sql_port = atoi(p);
    if (!sql_port)
      sql_port = 3306;
  }

  while (retries && !mysql_connp) {
    /* Try to connect to the database host. If we have specified
     * localhost, use the Unix domain socket instead.
     */
    mysql_connp = mysql_init(NULL);

    if (!mysql_real_connect(mysql_connp, sql_host, SQL_USER, SQL_PASS, SQL_DB,
                            sql_port, 0, 0)) {
      do_rawlog(LT_ERR, "Failed mysql connection: %s\n",
                mysql_error(mysql_connp));
      queue_event(SYSEVENT, "SQL`CONNECTFAIL", "%s,%s", "mysql",
                  mysql_error(mysql_connp));
      penn_mysql_sql_shutdown();
      sleep(1);
    }
    retries--;
  }

  if (penn_mysql_sql_connected()) {
    queue_event(SYSEVENT, "SQL`CONNECT", "%s", "mysql");
    last_retry = 0;
  }

  return penn_mysql_sql_connected();
}

static MYSQL_RES *
penn_mysql_sql_query(const char *q_string, int *affected_rows)
{
  MYSQL_RES *qres;
  int fail;

  if (affected_rows)
    *affected_rows = -1;

  /* Make sure we have something to query, first. */
  if (!q_string || !*q_string)
    return NULL;

  /* If we have no connection, and we don't have auto-reconnect on
   * (or we try to auto-reconnect and we fail), return NULL.
   */
  if (!penn_mysql_sql_connected()) {
    penn_mysql_sql_init();
    if (!penn_mysql_sql_connected()) {
      return NULL;
    }
  }

  /* Send the query. If it returns non-zero, we have an error. */
  fail = mysql_real_query(mysql_connp, q_string, strlen(q_string));
  if (fail && (mysql_errno(mysql_connp) == CR_SERVER_GONE_ERROR)) {
    if (mysql_connp) {
      queue_event(SYSEVENT, "SQL`DISCONNECT", "%s,%s", "mysql",
                  mysql_error(mysql_connp));
    }
    /* If it's CR_SERVER_GONE_ERROR, the server went away.
     * Try reconnecting. */
    penn_mysql_sql_init();
    if (mysql_connp) {
      fail = mysql_real_query(mysql_connp, q_string, strlen(q_string));
    }
  }
  /* If we still fail, it's an error. */
  if (fail) {
    return NULL;
  }

  /* Get the result */
  qres = mysql_use_result(mysql_connp);
  if (!qres) {
    if (mysql_field_count(mysql_connp) == 0) {
      /* We didn't expect data back, so see if we modified anything */
      if (affected_rows)
        *affected_rows = mysql_affected_rows(mysql_connp);
      return NULL;
    } else {
      /* Oops, we should have had data! */
      return NULL;
    }
  }
  return qres;
}

static void
penn_mysql_free_sql_query(MYSQL_RES *qres)
{
  /* "When using mysql_use_result(), you must execute
     mysql_fetch_row() until a NULL value is returned, otherwise, the
     unfetched rows are returned as part of the result set for you
     next query." Ewww. */
  while (mysql_fetch_row(qres)) {
  }
  mysql_free_result(qres);
  /* A single query can return multiple result sets; read and discard
     any that are remaining. */
  while (mysql_more_results(mysql_connp)) {
    if (mysql_next_result(mysql_connp) == 0) {
      qres = mysql_use_result(mysql_connp);
      if (qres) {
        while (mysql_fetch_row(qres)) {
        }
        mysql_free_result(qres);
      }
    }
  }
}

#endif

#ifdef HAVE_POSTGRESQL
static void
penn_pg_sql_shutdown(void)
{
  if (!penn_pg_sql_connected())
    return;
  PQfinish(postgres_connp);
  postgres_connp = NULL;
}

static int
penn_pg_sql_connected(void)
{
  return postgres_connp ? 1 : 0;
}

static int
penn_pg_sql_init(void)
{
  int retries = SQL_RETRY_TIMES;
  static time_t last_retry = 0;
  char sql_host[256], *p;
  char sql_port[256];
  time_t curtime;

  /* Only retry at most once per minute. */
  curtime = time(NULL);
  if (curtime < (last_retry + 60))
    return 0;
  last_retry = curtime;

  /* If we are already connected, drop and retry the connection, in
   * case for some reason the server went away.
   */
  if (penn_pg_sql_connected()) {
    penn_pg_sql_shutdown();
  }

  /* Parse SQL_HOST into sql_host and sql_port */
  mush_strncpy(sql_host, SQL_HOST, sizeof sql_host);
  strcpy(sql_port, "5432");

#ifdef HAVE_GETSERVBYNAME
  {
    /* Get port from /etc/services if present just in case it's been
       changed from the usual default. */
    struct servent *s = getservbyname("postgresql", "tcp");
    if (s) {
      snprintf(sql_port, sizeof sql_port, "%d", s->s_port);
    }
  }
#endif

  if ((p = strchr(sql_host, ':'))) {
    *p++ = '\0';
    if (*p) {
      strcpy(sql_port, p);
    }
  }

  while (retries && !postgres_connp) {
    /* Try to connect to the database host. */
    char conninfo[BUFFER_LEN];
    snprintf(conninfo, sizeof conninfo,
             "host=%s port=%s dbname=%s user=%s password=%s", sql_host,
             sql_port, SQL_DB, SQL_USER, SQL_PASS);
    postgres_connp = PQconnectdb(conninfo);
    if (PQstatus(postgres_connp) != CONNECTION_OK) {
      do_rawlog(LT_ERR, "Failed postgresql connection to %s: %s\n",
                PQdb(postgres_connp), PQerrorMessage(postgres_connp));
      queue_event(SYSEVENT, "SQL`CONNECTFAIL", "%s,%s", "postgresql",
                  PQerrorMessage(postgres_connp));
      penn_pg_sql_shutdown();
      sleep(1);
    }
    retries--;
  }

  if (penn_pg_sql_connected()) {
    queue_event(SYSEVENT, "SQL`CONNECT", "%s", "postgresql");
    last_retry = 0;
  }

  return penn_pg_sql_connected();
}

static PGresult *
penn_pg_sql_query(const char *q_string, int *affected_rows)
{
  PGresult *qres;

  /* No affected rows by default */
  if (affected_rows)
    *affected_rows = -1;

  /* Make sure we have something to query, first. */
  if (!q_string || !*q_string)
    return NULL;

  /* If we have no connection, and we don't have auto-reconnect on
   * (or we try to auto-reconnect and we fail), return NULL.
   */
  if (!penn_pg_sql_connected()) {
    penn_pg_sql_init();
    if (!sql_connected()) {
      return NULL;
    }
  }

  /* Send the query. If it returns non-zero, we have an error. */
  qres = PQexec(postgres_connp, q_string);
  if (!qres || (PQresultStatus(qres) != PGRES_COMMAND_OK &&
                PQresultStatus(qres) != PGRES_TUPLES_OK)) {
    /* Serious error, try one more time */
    if (postgres_connp) {
      queue_event(SYSEVENT, "SQL`DISCONNECT", "%s,%s", "postgresql",
                  PQerrorMessage(postgres_connp));
    }
    penn_pg_sql_init();
    if (penn_pg_sql_connected())
      qres = PQexec(postgres_connp, q_string);
    if (!qres || (PQresultStatus(qres) != PGRES_COMMAND_OK &&
                  PQresultStatus(qres) != PGRES_TUPLES_OK)) {
      return NULL;
    }
  }

  if (PQresultStatus(qres) == PGRES_COMMAND_OK) {
    *affected_rows = atoi(PQcmdTuples(qres));
    return NULL;
  }

  return qres;
}

static void
penn_pg_free_sql_query(PGresult *qres)
{
  PQclear(qres);
}
#endif /* HAVE_POSTGRESQL */

void sql_regexp_fun(sqlite3_context *, int, sqlite3_value **);

#ifdef HAVE_PTHREAD_ATFORK
/* Close before a fork. Next query will reopen the database */
static void
penn_sqlite3_prefork(void)
{
  if (sqlite3_connp) {
    sqlite3_close_v2(sqlite3_connp);
    sqlite3_connp = NULL;
  }
}
#endif

static int
penn_sqlite3_sql_init(void)
{
  int status;
  sqlite3_connp = NULL;
  if ((status = sqlite3_open_v2(SQL_DB, &sqlite3_connp,
                                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                NULL)) != SQLITE_OK) {
    do_rawlog(LT_ERR, "sqlite3: Failed to open %s: %s", SQL_DB,
              sqlite3_connp ? sqlite3_errmsg(sqlite3_connp)
                            : sqlite3_errstr(status));
    queue_event(SYSEVENT, "SQL`CONNECTFAIL", "%s,%s", "sqlite3",
                sqlite3_connp ? sqlite3_errmsg(sqlite3_connp)
                              : sqlite3_errstr(status));
    if (sqlite3_connp) {
      sqlite3_close(sqlite3_connp);
      sqlite3_connp = NULL;
    }
    return 0;
  } else {
#ifdef HAVE_ICU
    // Delete the ICU version
    sqlite3_create_function(sqlite3_connp, "regexp", 2,
                            SQLITE_ANY | SQLITE_DETERMINISTIC, NULL, NULL, NULL,
                            NULL);
#endif
    if ((status = sqlite3_create_function(
           sqlite3_connp, "regexp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
           sql_regexp_fun, NULL, NULL)) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to register sqlite3 regexp() function: %s",
                sqlite3_errmsg(sqlite3_connp));
    }

    queue_event(SYSEVENT, "SQL`CONNECT", "%s", "sqlite3");
#ifdef HAVE_PTHREAD_ATFORK
    static bool atfork = 0;
    if (!atfork) {
      pthread_atfork(penn_sqlite3_prefork, NULL, NULL);
      atfork = 1;
    }
#endif
    return 1;
  }
}

static void
penn_sqlite3_sql_shutdown(void)
{
  sqlite3_close_v2(sqlite3_connp);
  sqlite3_connp = NULL;
}

static int
penn_sqlite3_sql_connected(void)
{
  return sqlite3_connp ? 1 : 0;
}

static sqlite3_stmt *
penn_sqlite3_sql_query(const char *query, int *affected_rows)
{
  int q_len, u_len, retcode;
  sqlite3_stmt *statement = NULL;
  char *q_utf8;

  if (!penn_sqlite3_sql_connected()) {
    penn_sqlite3_sql_init();
    if (!penn_sqlite3_sql_connected())
      return NULL;
  }

  q_len = strlen(query);
  q_utf8 = latin1_to_utf8(query, q_len, &u_len, "string");
  retcode = sqlite3_prepare_v2(sqlite3_connp, q_utf8, u_len, &statement, NULL);
  mush_free(q_utf8, "string");

  *affected_rows = -1; /* Can't find this out yet */

  if (retcode == SQLITE_OK)
    return statement;
  else
    return NULL;
}

static void
penn_sqlite3_free_sql_query(sqlite3_stmt *stmt)
{
  sqlite3_finalize(stmt);
}
