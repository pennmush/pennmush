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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

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
#include <sqlite3.h>
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
#include "mythread.h"

/* Supported platforms */
typedef enum {
  SQL_PLATFORM_DISABLED = -1,
  SQL_PLATFORM_MYSQL = 1,
  SQL_PLATFORM_POSTGRESQL,
  SQL_PLATFORM_SQLITE3
} sqlplatform;

penn_mutex sql_mutex;

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
static char *sql_sanitize(const char * RESTRICT, const char * RESTRICT)
  __attribute_malloc__;
static int safe_sql_sanitize(const char * RESTRICT, char *, char **);
#define SANITIZE(s, n) ((s && *s) ? sql_sanitize(s, n) : NULL)

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

static int
safe_sql_sanitize(const char * RESTRICT res, char *buff, char **bp)
{
  char local[BUFFER_LEN];
  char *lbp = local;
  const char *rp = res;

  if (!res || !*res) {
    return 0;
  }

  for (; *rp; rp++) {
    if (isprint(*rp) || *rp == '\n' || *rp == '\t' || *rp == ESC_CHAR ||
        *rp == TAG_START || *rp == TAG_END || *rp == BEEP_CHAR) {
      safe_chr(*rp, local, &lbp);
    }
  }
  *lbp = '\0';

  if (has_markup(local)) {
    ansi_string *as = parse_ansi_string(local);
    lbp = local;
    safe_ansi_string(as, 0, as->len, local, &lbp);
    free_ansi_string(as);
  }

  return safe_strl(local, lbp - local, buff, bp);
}

char *
sql_sanitize(const char * RESTRICT res, const char * RESTRICT name)
{
  char *buff = mush_malloc(BUFFER_LEN, name);
  char *bp = buff;
  safe_sql_sanitize(res, buff, &bp);
  *bp = '\0';
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
#if defined(HAVE_SQLITE3) && defined(HAVE_MYSQL)
  case SQL_PLATFORM_SQLITE3:
    /* sqlite3 doesn't have a escape function. Use one of my MySQL's
     * if we can. */
    chars_written = mysql_escape_string(bigbuff, args[0], arglens[0]);
    break;
#endif
  default:
    safe_str(T(e_disabled), buff, bp);
    return;
  }
  if (chars_written == 0)
    return;
  else if (chars_written < BUFFER_LEN)
    safe_sql_sanitize(bigbuff, buff, bp);
  else
    safe_str(T("#-1 TOO LONG"), buff, bp);
}

struct sql_args {
  dbref executor;
  dbref triggerer;
  dbref thing;
  int queue_type;
  char *query;
  char *attr;
  PE_REGS *regvals;
  bool async;
  bool donotify;
  bool dofieldnames;
};

static struct sql_args *
new_sql_args(void)
{
  return calloc(1, sizeof(struct sql_args));  
}

static void
free_sql_args(struct sql_args *a)
{
  if (a->query) {
    free(a->query);
  }
  if (a->attr) {
    free(a->attr);    
  }
  if (a->regvals) {
    pe_regs_free(a->regvals);
  }

  free(a);
}

THREAD_RETURN_TYPE WIN32_STDCALL
mapsql_cmd_fun(void *arg)
{
  struct sql_args *sargs = arg;
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
  char strrownum[20];
  int i, a;

  for (a = 0; a < MAX_STACK_ARGS; a++) {
    cells[a] = NULL;
    names[a] = NULL;
  }
  
  /* Do the query. */
  qres = sql_query(sargs->query, &affected_rows);

  if (!qres) {
    if (!sargs->async) {
      if (affected_rows >= 0) {
        notify_format(sargs->executor, T("SQL: %d rows affected."), affected_rows);
      } else if (!sql_connected()) {
        notify(sargs->executor, T("No SQL database connection."));
      } else {
        notify_format(sargs->executor, T("SQL: Error: %s"), sql_error());
      }
    }
    free_sql_args(sargs);
    THREAD_RETURN;
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    numrows = mysql_num_rows(qres);
    numfields = mysql_num_fields(qres);
    fields = mysql_fetch_fields(qres);
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    numfields = PQnfields(qres);
    numrows = PQntuples(qres);
    break;
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
#endif
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
#ifdef HAVE_SQLITE3
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE) {
        break;
      } else if (retcode != SQLITE_ROW) {
        if (!sargs->async) {
          notify_format(sargs->executor, T("SQL: Error: %s"), sql_error());
        }
        break;
      }
    }
#endif
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
#ifdef HAVE_SQLITE3
        case SQL_PLATFORM_SQLITE3:
          cells[i + 1] =
            SANITIZE((char *) sqlite3_column_text(qres, i), "sql_row");
          names[i + 1] =
            SANITIZE((char *) sqlite3_column_name(qres, i), "sql_fieldname");
          break;
#endif
        default:
          /* Not reached, shuts up compiler */
          break;
        }
      }

      if ((rownum == 0) && sargs->dofieldnames) {
        /* Queue 0: <names> */
        snprintf(strrownum, sizeof strrownum, "%d", 0);
        names[0] = strrownum;
        for (i = 0; i < useable_fields + 1; i++) {
          pe_regs_setenv(pe_regs, i, names[i]);
        }
        pe_regs_qcopy(pe_regs, sargs->regvals);
        queue_attribute_base_priv(sargs->thing, sargs->attr,
                                  sargs->triggerer, 0, pe_regs,
                                  sargs->queue_type, sargs->executor);
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
      pe_regs_qcopy(pe_regs, sargs->regvals);
      queue_attribute_base_priv(sargs->thing, sargs->attr, sargs->triggerer,
                                0, pe_regs, sargs->queue_type,
                                sargs->executor);
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
  if (sargs->donotify) {
    parse_que(sargs->executor, sargs->executor, "@notify me", NULL);
  }

finished:
  if (pe_regs)
    pe_regs_free(pe_regs);
  free_sql_query(qres);
  free_sql_args(sargs);
  THREAD_RETURN;
}

COMMAND(cmd_mapsql)
{
  char tbuf[BUFFER_LEN];
  char *s;
  struct sql_args *sargs;
  thread_id id;
  dbref thing;
  bool spoof = SW_ISSET(sw, SWITCH_SPOOF);
  
  if (!arg_right || !*arg_right) {
    notify(executor, T("What do you want to query?"));
    return;
  }

  /* Find and fetch the attribute, first. */
  strncpy(tbuf, arg_left, BUFFER_LEN);

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

  if (God(thing) && !God(executor)) {
    notify(executor, T("You can't trigger God!"));
    return;
  }

  sargs = new_sql_args();
  
  sargs->executor = executor;
  if (spoof) {
    sargs->triggerer = enactor;
  } else {
    sargs->triggerer = executor;
  }
  sargs->query = strdup(arg_right);
  sargs->thing = thing;
  sargs->attr = strdup(tbuf);
  sargs->queue_type = QUEUE_DEFAULT | (queue_entry->queue_type & QUEUE_EVENT);
  sargs->regvals = pe_regs_create(PE_REGS_ARG | PE_REGS_Q, "cmd_mapsql");
  pe_regs_qcopy(sargs->regvals, queue_entry->pe_info->regvals);
  sargs->dofieldnames = SW_ISSET(sw, SWITCH_COLNAMES);
  sargs->donotify = SW_ISSET(sw, SWITCH_NOTIFY);

#ifdef ASYNC_SQL
  sargs->async = 1;
  run_thread(&id, mapsql_cmd_fun, sargs, true);
#else
  mapsql_cmd_fun(sargs);
#endif

}

THREAD_RETURN_TYPE WIN32_STDCALL
sql_cmd_func(void *arg)
{
  struct sql_args *sargs = arg;
#ifdef HAVE_MYSQL
  MYSQL_FIELD *fields = NULL;
#endif
  void *qres;
  int affected_rows = -1;
  int rownum;
  int numfields;
  int numrows;
  char *cell = NULL;
  char *name = NULL;
  char tbuf[BUFFER_LEN];
  char *tbp;
  ansi_string *as;
  int i;

  qres = sql_query(sargs->query, &affected_rows);

  if (sargs->async) {
    goto finished;
  }
  
  if (!qres) {
    if (affected_rows >= 0) {
      notify_format(sargs->executor, T("SQL: %d rows affected."), affected_rows);
    } else if (!sql_connected()) {
      notify(sargs->executor, T("No SQL database connection."));
    } else {
      notify_format(sargs->executor, T("SQL: Error: %s"), sql_error());
    }
    goto finished;    
  }

  /* Get results. A silent query (INSERT, UPDATE, etc.) will return NULL */
  switch (sql_platform()) {
#ifdef HAVE_MYSQL
  case SQL_PLATFORM_MYSQL:
    numrows = mysql_num_rows(qres);
    numfields = mysql_num_fields(qres);
    fields = mysql_fetch_fields(qres);
    break;
#endif
#ifdef HAVE_POSTGRESQL
  case SQL_PLATFORM_POSTGRESQL:
    numfields = PQnfields(qres);
    numrows = PQntuples(qres);
    break;
#endif
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
#endif
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
#ifdef HAVE_SQLITE3
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW) {
        notify_format(sargs->executor, T("SQL: Error: %s"), sql_error());
        break;
      }
    }
#endif

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
#ifdef HAVE_SQLITE3
        case SQL_PLATFORM_SQLITE3:
          cell = (char *) sqlite3_column_text(qres, i);
          name = (char *) sqlite3_column_name(qres, i);
          break;
#endif
        default:
          /* Not reached, shuts up compiler */
          break;
        }
        if (cell && *cell) {
          char cbuff[BUFFER_LEN];
          char *cbp = cbuff;
          safe_sql_sanitize(cell, cbuff, &cbp);
          *cbp = '\0';
          if (strchr(cell, TAG_START) || strchr(cell, ESC_CHAR)) {
            /* Either old or new style ANSI string. */
            tbp = tbuf;
            as = parse_ansi_string(cell);
            safe_ansi_string(as, 0, as->len, tbuf, &tbp);
            *tbp = '\0';
            free_ansi_string(as);
            cell = tbuf;
          }
        }
        notify_format(sargs->executor, T("Row %d, Field %s: %s"), rownum + 1, name,
                      (cell && *cell) ? cell : "NULL");
      }
    } else
      notify_format(sargs->executor, T("Row %d: NULL"), rownum + 1);
  }

finished:
  if (qres) {
    free_sql_query(qres);
  }
  free_sql_args(sargs);
  THREAD_RETURN;
}

COMMAND(cmd_sql)
{
  thread_id id;
  struct sql_args *sargs;
  
  if (sql_platform() == SQL_PLATFORM_DISABLED) {
    notify(executor, T("No SQL database connection."));
    return;
  }

  if (!arg_left || !*arg_left) {
    notify(executor, T("What do you want to query?"));
    return;
  }

  sargs = new_sql_args();
  sargs->executor = executor;
  sargs->query = strdup(arg_left);

#ifdef ASYNC_SQL
  if (SW_ISSET(sw, SWITCH_ASYNC)) {
    sargs->async = 1;
    run_thread(&id, sql_cmd_func, sargs, true);
  } else {
    sql_cmd_func(sargs);
  }
#else
  sql_cmd_func(sargs);
#endif
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
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
#endif
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
        sql_sanitize(fields[i].name, "sql_fieldname");
      break;
#endif
#ifdef HAVE_POSTGRESQL
    case SQL_PLATFORM_POSTGRESQL:
      fieldnames[i] =
        sql_sanitize(PQfname(qres, i), "sql_fieldname");
      break;
#endif
#ifdef HAVE_SQLITE3
    case SQL_PLATFORM_SQLITE3:
      fieldnames[i] =
        sql_sanitize(sqlite3_column_name(qres, i), "sql_fieldname");
      break;
#endif
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
#ifdef HAVE_SQLITE3
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW)
        goto finished;
    }
#endif
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
#ifdef HAVE_SQLITE3
      case SQL_PLATFORM_SQLITE3:
        cell = (char *) sqlite3_column_text(qres, i);
        break;
#endif
      default:
        break;
      }
      if (cell && *cell) {
        cell = sql_sanitize(cell, "sql_cell");
        pe_regs_setenv(pe_regs, i + 1, cell);
        if (*fieldnames[i] && !is_strict_integer(fieldnames[i])) {
          pe_regs_set(pe_regs, PE_REGS_ARG, fieldnames[i], cell);
        }
        mush_free(cell, "sql_cell");
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
  if (pe_regs)
    pe_regs_free(pe_regs);
  if (fieldnames) {
    for (i = 0; i < useable_fields; i++)
      mush_free(fieldnames[i], "sql_fieldname");
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
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
#endif
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
#ifdef HAVE_SQLITE3
    if (sql_platform() == SQL_PLATFORM_SQLITE3) {
      int retcode = sqlite3_step(qres);
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW)
        break;
    }
#endif
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
#ifdef HAVE_SQLITE3
      case SQL_PLATFORM_SQLITE3:
        cell = (char *) sqlite3_column_text(qres, i);
        break;
#endif
      default:
        break;
      }
      if (cell && *cell) {
        char cbuff[BUFFER_LEN];
        char *cbp = cbuff;
        safe_sql_sanitize(cell, cbuff, &cbp);
        *cbp = '\0';
        if (strchr(cbuff, TAG_START) || strchr(cbuff, ESC_CHAR)) {
          /* Either old or new style ANSI string. */
          tbp = tbuf;
          as = parse_ansi_string(cell);
          safe_ansi_string(as, 0, as->len, tbuf, &tbp);
          *tbp = '\0';
          free_ansi_string(as);
          cell = tbuf;
        }
        if (safe_str(cell, buff, bp))
          goto finished; /* We filled the buffer, best stop */
      }
    }
  }
finished:
  free_sql_query(qres);
}

/* MYSQL-specific functions */
#ifdef HAVE_MYSQL

/* MySQL thread safety: https://dev.mysql.com/doc/refman/5.7/en/c-api-threaded-clients.html */

static void
penn_mysql_sql_shutdown(void)
{
  mutex_lock(&sql_mutex);
  if (mysql_connp) {
    mysql_close(mysql_connp);
    mysql_connp = NULL;
  }
  mutex_unlock(&sql_mutex);
}

static int
penn_mysql_sql_connected(void)
{
  int c;
  mutex_lock(&sql_mutex);
  c = mysql_connp ? 1 : 0;
  mutex_unlock(&sql_mutex);
  return c;
}

static int
penn_mysql_sql_init(void)
{
  int retries = SQL_RETRY_TIMES;
  static time_t last_retry = 0;
  char sql_host[BUFFER_LEN], *p;
  int sql_port = 3306;
  time_t curtime;

  /* Only retry at most once per minute. */
  curtime = time(NULL);
  if (curtime < (last_retry + 60))
    return 0;
  last_retry = curtime;

  if (!mysql_thread_safe()) {
    do_rawlog(LT_ERR, "WARNING: MySQL library not thread safe. Do not use @mapsql or @sql/async!");
  }
  
  mutex_lock(&sql_mutex);
  
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
    mutex_unlock(&sql_mutex);
    return 1;
  } else {
    mutex_unlock(&sql_mutex);
    return 0;
  }
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

  mutex_lock(&sql_mutex);
  
  /* If we have no connection, and we don't have auto-reconnect on
   * (or we try to auto-reconnect and we fail), return NULL.
   */
  if (!penn_mysql_sql_connected()) {
    penn_mysql_sql_init();
    if (!penn_mysql_sql_connected()) {
      mutex_unlock(&sql_mutex);
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
    mutex_unlock(&sql_mutex);
    return NULL;
  }

  /* Get the result */
  qres = mysql_store_result(mysql_connp);
  if (!qres) {
    if (mysql_field_count(mysql_connp) == 0) {
      /* We didn't expect data back, so see if we modified anything */
      if (affected_rows) {
        *affected_rows = mysql_affected_rows(mysql_connp);
      }
      mutex_unlock(&sql_mutex);
      return NULL;
    } else {
      /* Oops, we should have had data! */
      mutex_unlock(&sql_mutex);
      return NULL;
    }
  }
  mutex_unlock(&sql_mutex);
  return qres;
}

static void
penn_mysql_free_sql_query(MYSQL_RES *qres)
{
  mysql_free_result(qres);
}

#endif

#ifdef HAVE_POSTGRESQL

/* PostgreSQL thread safety: https://www.postgresql.org/docs/9.3/static/libpq-threading.html */

static void
penn_pg_sql_shutdown(void)
{
  mutex_lock(&sql_mutex);
  if (penn_pg_sql_connected()) {
    PQfinish(postgres_connp);
    postgres_connp = NULL;
  }
  mutex_unlock(&sql_mutex);
}

static int
penn_pg_sql_connected(void)
{
  int c;
  mutex_lock(&sql_mutex);
  c = postgres_connp ? 1 : 0;
  mutex_unlock(&sql_mutex);
  return c;
}

static int
penn_pg_sql_init(void)
{
  int retries = SQL_RETRY_TIMES;
  static time_t last_retry = 0;
  char sql_host[BUFFER_LEN], *p;
  char sql_port[BUFFER_LEN];
  time_t curtime;

  /* Only retry at most once per minute. */
  curtime = time(NULL);
  if (curtime < (last_retry + 60))
    return 0;
  last_retry = curtime;

  if (!PQisthreadsafe()) {
    do_rawlog(LT_ERR, "WARNING: Postgres library is not thread safe! Do not use @mapsql or @sql/async!");
  }
  
  mutex_lock(&sql_mutex);
  
  /* If we are already connected, drop and retry the connection, in
   * case for some reason the server went away.
   */
  if (penn_pg_sql_connected()) {
    penn_pg_sql_shutdown();
  }

  /* Parse SQL_HOST into sql_host and sql_port */
  mush_strncpy(sql_host, SQL_HOST, BUFFER_LEN);
  strcpy(sql_port, "5432");
  if ((p = strchr(sql_host, ':'))) {
    *p++ = '\0';
    if (*p)
      strcpy(sql_port, p);
  }

  while (retries && !postgres_connp) {
    /* Try to connect to the database host. */
    char conninfo[BUFFER_LEN];
    snprintf(conninfo, BUFFER_LEN,
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
    mutex_unlock(&sql_mutex);
    return 1;
  } else {
    mutex_unlock(&sql_mutex);
    return 0;
  }
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

  mutex_lock(&sql_mutex);
  
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
      mutex_unlock(&sql_mutex);
      return NULL;
    }
  }
  mutex_unlock(&sql_mutex);

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

#ifdef HAVE_SQLITE3

/* Sqlite3 thread safety: https://sqlite.org/threadsafe.html
 *
 * Rely heavily on FULLMUTEX mode, but protect things that modify sqlite3_connp
 */

static int
penn_sqlite3_sql_init(void)
{  
  sqlite3_connp = NULL;

  if (!sqlite3_threadsafe()) {
        do_rawlog(LT_ERR, "WARNING: Sqlite3 library not thread safe. Do not use @mapsql or @sql/async!");
  }

  mutex_lock(&sql_mutex);
  
  if (sqlite3_open_v2(SQL_DB, &sqlite3_connp,
                      SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
    do_rawlog(LT_ERR, "sqlite3: Failed to open %s: %s", SQL_DB, sql_error());
    queue_event(SYSEVENT, "SQL`CONNECTFAIL", "%s,%s", "sqlite3", sql_error());
    if (sqlite3_connp) {
      sqlite3_close(sqlite3_connp);
      sqlite3_connp = NULL;
    }
    mutex_unlock(&sql_mutex);
    return 0;
  } else {
    mutex_unlock(&sql_mutex);
    queue_event(SYSEVENT, "SQL`CONNECT", "%s", "sqlite3");
    return 1;
  }
}

static void
penn_sqlite3_sql_shutdown(void)
{
  mutex_lock(&sql_mutex);
  sqlite3_close_v2(sqlite3_connp);
  sqlite3_connp = NULL;
  mutex_unlock(&sql_mutex);
}

static int
penn_sqlite3_sql_connected(void)
{
  int c;
  mutex_lock(&sql_mutex);
  c = sqlite3_connp ? 1 : 0;
  mutex_unlock(&sql_mutex);
  return c;
}

static sqlite3_stmt *
penn_sqlite3_sql_query(const char *query, int *affected_rows)
{
  int q_len, retcode;
  const char *eoq = NULL;
  sqlite3_stmt *statement = NULL;

  mutex_lock(&sql_mutex);
  
  if (!penn_sqlite3_sql_connected()) {
    penn_sqlite3_sql_init();
    if (!penn_sqlite3_sql_connected()) {
      mutex_unlock(&sql_mutex);
      return NULL;
    }
  }

  q_len = strlen(query);
#if SQLITE3_VERSION_NUMBER >= 30003010
  retcode = sqlite3_prepare_v2(sqlite3_connp, query, q_len, &statement, &eoq);
#else
  retcode = sqlite3_prepare(sqlite3_connp, query, q_len, &statement, &eoq);
#endif

  mutex_unlock(&sql_mutex);
  
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
#endif /* HAVE_SQLITE3 */
