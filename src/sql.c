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
#include "config.h"
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <winsock.h>
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

#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "log.h"
#include "parse.h"
#include "command.h"
#include "function.h"
#include "mushdb.h"
#include "confmagic.h"
#include "ansi.h"
#include "match.h"

/* Supported platforms */
typedef enum { SQL_PLATFORM_DISABLED = -1,
  SQL_PLATFORM_MYSQL = 1,
  SQL_PLATFORM_POSTGRESQL,
  SQL_PLATFORM_SQLITE3
} sqlplatform;

/* Number of times to try a connection */
#define SQL_RETRY_TIMES 3

#define sql_test_result(qres) \
  if (!qres) { \
    if (affected_rows >= 0) { \
    } else if (!sql_connected()) { \
      safe_str(T("#-1 SQL ERROR: NO DATABASE CONNECTED"), buff, bp); \
    } else { \
      safe_format(buff, bp, T("#-1 SQL ERROR: %s"), sql_error()); \
    } \
    return; \
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
free_sql_query(void *queryp __attribute__ ((__unused__)))
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
sql_query(const char *query_str __attribute__ ((__unused__)), int *affected_rows
          __attribute__ ((__unused__)))
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
  if (chars_written < BUFFER_LEN)
    safe_str(bigbuff, buff, bp);
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
  PE_REGS *pe_regs = NULL;
  char *names[10];
  char *cells[10];
  char tbuf[BUFFER_LEN];
  char strrownum[20];
  char *s;
  int i, a;
  dbref thing;
  int dofieldnames = SW_ISSET(sw, SWITCH_COLNAMES);
  int donotify = SW_ISSET(sw, SWITCH_NOTIFY);

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

  if (!controls(executor, thing) && !(Owns(executor, thing) && LinkOk(thing))) {
    notify(executor, T("Permission denied."));
    return;
  }

  if (God(thing) && !God(executor)) {
    notify(executor, T("You can't trigger God!"));
    return;
  }

  for (a = 0; a < 10; a++) {
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
    numrows = INT_MAX;          /* Using mysql_use_result() doesn't know the number
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
#ifdef HAVE_SQLITE3
  case SQL_PLATFORM_SQLITE3:
    numfields = sqlite3_column_count(qres);
    numrows = INT_MAX;
    break;
#endif
  default:
    goto finished;
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
      if (retcode == SQLITE_DONE)
        break;
      else if (retcode != SQLITE_ROW) {
        notify_format(executor, T("SQL: Error: %s"), sql_error());
        break;
      }
    }
#endif

    if (numfields > 0) {
      for (i = 0; i < numfields && i < 9; i++) {
        switch (sql_platform()) {
#ifdef HAVE_MYSQL
        case SQL_PLATFORM_MYSQL:
          cells[i + 1] = row_p[i];
          names[i + 1] = fields[i].name;
          break;
#endif
#ifdef HAVE_POSTGRESQL
        case SQL_PLATFORM_POSTGRESQL:
          cells[i + 1] = PQgetvalue(qres, rownum, i);
          names[i + 1] = PQfname(qres, i);
          break;
#endif
#ifdef HAVE_SQLITE3
        case SQL_PLATFORM_SQLITE3:
          cells[i + 1] = (char *) sqlite3_column_text(qres, i);
          names[i + 1] = (char *) sqlite3_column_name(qres, i);
          break;
#endif
        default:
          /* Not reached, shuts up compiler */
          break;
        }
      }

      if ((rownum == 0) && dofieldnames) {
        /* Queue 0: <names> */
        snprintf(strrownum, 20, "%d", 0);
        names[0] = strrownum;
        for (i = 0; i < (numfields + 1) && i < 10; i++) {
          pe_regs_setenv(pe_regs, i, names[i]);
        }
        queue_attribute_base(thing, s, executor, 0, pe_regs, 0);
      }

      /* Queue the rest. */
      snprintf(strrownum, 20, "%d", rownum + 1);
      cells[0] = strrownum;
      pe_regs_clear(pe_regs);
      for (i = 0; i < (numfields + 1) && i < 10; i++) {
        pe_regs_setenv(pe_regs, i, cells[i]);
      }
      pe_regs_qcopy(pe_regs, queue_entry->pe_info->regvals);
      queue_attribute_base(thing, s, executor, 0, pe_regs, 0);
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
  char *cell = NULL;
  char *name = NULL;
  char tbuf[BUFFER_LEN];
  char *tbp;
  ansi_string *as;
  int i;

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
    numrows = INT_MAX;          /* Using mysql_use_result() doesn't know the number
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
        notify_format(executor, T("SQL: Error: %s"), sql_error());
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
        notify_format(executor, T("Row %d, Field %s: %s"),
                      rownum + 1, name, (cell && *cell) ? cell : "NULL");
      }
    } else
      notify_format(executor, T("Row %d: NULL"), rownum + 1);
  }

finished:
  free_sql_query(qres);
}

FUNCTION(fun_mapsql)
{
  void *qres;
  ufun_attrib ufun;
  char *osep = (char *) " ";
  int affected_rows;
  int rownum;
  int numfields, numrows;
  char rbuff[BUFFER_LEN];
  int funccount = 0;
  int do_fieldnames = 0;
  int i, j;
  char buffs[9][BUFFER_LEN];
  char *tbp;
  char *cell = NULL;
  PE_REGS *pe_regs = NULL;
  ansi_string *as;
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

  pe_regs = pe_regs_create(PE_REGS_ARG, "fun_mapsql");
  if (do_fieldnames) {
    pe_regs_setenv(pe_regs, 0, unparse_integer(0));
#ifdef HAVE_MYSQL
    if (sql_platform() == SQL_PLATFORM_MYSQL)
      fields = mysql_fetch_fields(qres);
#endif
    for (i = 0; i < numfields && i < 9; i++) {
      switch (sql_platform()) {
#ifdef HAVE_MYSQL
      case SQL_PLATFORM_MYSQL:
        pe_regs_setenv(pe_regs, i + 1, fields[i].name);
        break;
#endif
#ifdef HAVE_POSTGRESQL
      case SQL_PLATFORM_POSTGRESQL:
        pe_regs_setenv(pe_regs, i + 1, PQfname(qres, i));
        break;
#endif
#ifdef HAVE_SQLITE3
      case SQL_PLATFORM_SQLITE3:
        pe_regs_setenv(pe_regs, i + 1, (char *) sqlite3_column_name(qres, i));
        break;
#endif
      default:
        break;
      }
    }
    for (j = 0; j < (i + 1); j++) {
    }
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
    for (i = 0; (i < numfields) && (i < 9); i++) {
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
        if (strchr(cell, TAG_START) || strchr(cell, ESC_CHAR)) {
          /* Either old or new style ANSI string. */
          tbp = buffs[i];
          as = parse_ansi_string(cell);
          safe_ansi_string(as, 0, as->len, buffs[i], &tbp);
          *tbp = '\0';
          free_ansi_string(as);
          cell = buffs[i];
        }
      }
      if (cell)
        pe_regs_setenv(pe_regs, i + 1, cell);
    }
    /* Now call the ufun. */
    if (call_ufun(&ufun, rbuff, executor, enactor, pe_info, pe_regs))
      goto finished;
    if (safe_str(rbuff, buff, bp)
        && funccount == pe_info->fun_invocations)
      goto finished;
    funccount = pe_info->fun_invocations;
  }
finished:
  if (pe_regs)
    pe_regs_free(pe_regs);
  free_sql_query(qres);
}

FUNCTION(fun_sql)
{
  void *qres;
  char *rowsep = (char *) " ";
  char *fieldsep = (char *) " ";
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
        if (strchr(cell, TAG_START) || strchr(cell, ESC_CHAR)) {
          /* Either old or new style ANSI string. */
          tbp = tbuf;
          as = parse_ansi_string(cell);
          safe_ansi_string(as, 0, as->len, tbuf, &tbp);
          *tbp = '\0';
          free_ansi_string(as);
          cell = tbuf;
        }
        if (safe_str(cell, buff, bp))
          goto finished;        /* We filled the buffer, best stop */
      }
    }
  }
finished:
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

    if (!mysql_real_connect
        (mysql_connp, sql_host, SQL_USER, SQL_PASS, SQL_DB, sql_port, 0, 0)) {
      do_rawlog(LT_ERR, "Failed mysql connection: %s\n",
                mysql_error(mysql_connp));
      queue_event(SYSEVENT, "SQL`CONNECTFAIL", "%s,%s",
                  "mysql", mysql_error(mysql_connp));
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
      queue_event(SYSEVENT, "SQL`DISCONNECT", "%s,%s",
                  "mysql", mysql_error(mysql_connp));
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
  while (mysql_fetch_row(qres)) ;
  mysql_free_result(qres);
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
  char sql_host[BUFFER_LEN], *p;
  char sql_port[BUFFER_LEN];
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
      queue_event(SYSEVENT, "SQL`CONNECTFAIL", "%s,%s",
                  "postgresql", PQerrorMessage(postgres_connp));
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
      queue_event(SYSEVENT, "SQL`DISCONNECT", "%s,%s",
                  "postgresql", PQerrorMessage(postgres_connp));
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
#endif                          /* HAVE_POSTGRESQL */

#ifdef HAVE_SQLITE3

static int
penn_sqlite3_sql_init(void)
{

  sqlite3_connp = NULL;
  if (sqlite3_open(SQL_DB, &sqlite3_connp) != SQLITE_OK) {
    do_rawlog(LT_ERR, "sqlite3: Failed to open %s: %s", SQL_DB, sql_error());
    queue_event(SYSEVENT, "SQL`CONNECTFAIL", "%s,%s", "sqlite3", sql_error());
    if (sqlite3_connp) {
      sqlite3_close(sqlite3_connp);
      sqlite3_connp = NULL;
    }
    return 0;
  } else {
    queue_event(SYSEVENT, "SQL`CONNECT", "%s", "sqlite3");
    return 1;
  }
}

static void
penn_sqlite3_sql_shutdown(void)
{
  sqlite3_close(sqlite3_connp);
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
  int q_len, retcode;
  const char *eoq = NULL;
  sqlite3_stmt *statement = NULL;

  if (!penn_sqlite3_sql_connected()) {
    penn_sqlite3_sql_init();
    if (!penn_sqlite3_sql_connected())
      return NULL;
  }

  q_len = strlen(query);
#if SQLITE3_VERSION_NUMBER >= 30003010
  retcode = sqlite3_prepare_v2(sqlite3_connp, query, q_len, &statement, &eoq);
#else
  retcode = sqlite3_prepare(sqlite3_connp, query, q_len, &statement, &eoq);
#endif

  *affected_rows = -1;          /* Can't find this out yet */

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
#endif                          /* HAVE_SQLITE3 */
