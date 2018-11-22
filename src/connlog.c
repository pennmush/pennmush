/** \file connlog.c
 *
 * \brief Routines for the connect log sqlite database.
 */

#include <stdlib.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <limits.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "mushtype.h"
#include "dbdefs.h"
#include "conf.h"
#include "externs.h"
#include "game.h"
#include "parse.h"
#include "match.h"
#include "log.h"
#include "mushsql.h"
#include "function.h"
#include "strutil.h"
#include "mymalloc.h"
#include "charconv.h"

#define CONNLOG_APPID 0x42010FF2
#define CONNLOG_VERSION 4
#define CONNLOG_VERSIONS "4"

sqlite3 *connlog_db;

/** Update the current timestamp used to update disconnection times
 *  when coming back from a crash.
 */
static void
update_checkpoint(void)
{
  sqlite3_stmt *ts;
  int status;

  ts = prepare_statement(
    connlog_db,
    "UPDATE checkpoint SET timestamp = strftime('%s', 'now') WHERE id = 1",
    "connlog.timestamp");
  do {
    status = sqlite3_step(ts);
  } while (is_busy_status(status));
  sqlite3_reset(ts);
}

static bool
checkpoint_event(void *arg __attribute__((__unused__)))
{
  update_checkpoint();
  return 1;
}

#ifdef HAVE_PTHREAD_ATFORK
static bool relaunch = 0;
static void
conndb_prefork(void)
{
  if (connlog_db) {
    close_sql_db(connlog_db);
    connlog_db = NULL;
    relaunch = 1;
  } else {
    relaunch = 0;
  }
}

static void
conndb_postfork_parent(void)
{
  if (relaunch) {
    connlog_db = open_sql_db(options.connlog_db, 1);
  }
}

#endif

static bool
connlog_optimize(void *vptr __attribute__((__unused__)))
{
  if (connlog_db) {
    return optimize_db(connlog_db);
  } else {
    return false;
  }
}

/** Intialize connlog database.
 *
 * \param rebooting true if coming up from a reboot.
 * \return true if successful, false on error.
 */
bool
init_conndb(bool rebooting)
{
  int app_id, version;
  char *err;

  connlog_db = open_sql_db(options.connlog_db, 0);

  if (!connlog_db) {
    return 0;
  }

  if (get_sql_db_id(connlog_db, &app_id, &version) < 0) {
    goto error_cleanup;
  }

  if (app_id != 0 && app_id != CONNLOG_APPID) {
    do_rawlog(LT_ERR,
              "Connlog database used for something else, application id 0x%x",
              app_id);
    goto error_cleanup;
  }

  if (app_id == 0) {
    /* Sqlite 3.24 added the ability to have auxilary columns in RTree
       virtual tables. Consider folding the connections table into it
       to avoid a join? */
    do_rawlog(LT_ERR, "Building connlog database.");
    if (sqlite3_exec(
          connlog_db,
          "PRAGMA journal_mode = WAL;"
          "PRAGMA application_id = 0x42010FF2;"
          "PRAGMA user_version = " CONNLOG_VERSIONS ";"
          "DROP TABLE IF EXISTS connections;"
          "DROP TABLE IF EXISTS timestamps;"
          "DROP TABLE IF EXISTS checkpoint;"
          "DROP TABLE IF EXISTS addrs;"
          "CREATE VIRTUAL TABLE timestamps USING rtree_i32(id, conn, disconn);"
          "CREATE TABLE addrs(id INTEGER NOT NULL PRIMARY KEY, ipaddr TEXT NOT "
          "NULL UNIQUE, hostname TEXT NOT NULL);"
          "CREATE TABLE connections(id INTEGER NOT NULL PRIMARY KEY, dbref "
          "INTEGER NOT NULL DEFAULT -1, name TEXT, addrid INTEGER NOT NULL,"
          "reason TEXT, ssl INTEGER, websocket INTEGER, FOREIGN KEY(addrid) "
          "REFERENCES addrs(id));"
          "CREATE INDEX conn_dbref_idx ON connections(dbref);"
          "CREATE INDEX conn_addr_idx ON connections(addrid);"
          "CREATE TABLE checkpoint(id INTEGER NOT NULL PRIMARY KEY, timestamp "
          "INTEGER NOT NULL);"
          "INSERT INTO checkpoint VALUES (1, strftime('%s', 'now'));"
          "CREATE VIEW connlog(id, dbref, name, ipaddr, hostname, conn, "
          "disconn, reason, ssl, websocket) AS SELECT c.id, c.dbref, c.name, "
          "a.ipaddr, a.hostname, ts.conn, ts.disconn, c.reason, c.ssl, "
          "c.websocket FROM "
          "connections AS c "
          "JOIN timestamps AS ts ON c.id = ts.id JOIN addrs AS a ON c.addrid = "
          "a.id;"
          "CREATE TRIGGER conn_logout INSTEAD OF UPDATE OF disconn,reason ON "
          "connlog BEGIN UPDATE connections SET reason = NEW.reason WHERE id = "
          "NEW.id; UPDATE timestamps SET disconn = NEW.disconn WHERE id = "
          "NEW.id; "
          "END;",
          NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to build connlog database: %s", err);
      sqlite3_free(err);
      goto error_cleanup;
    }
  } else if (version == 1) {
    do_rawlog(LT_ERR, "Upgrading connlog db from 1 to %d", CONNLOG_VERSION);
    if (sqlite3_exec(
          connlog_db,
          "BEGIN TRANSACTION;"
          "CREATE TABLE backup AS SELECT * FROM connections;"
          "DROP TABLE connections;"
          "CREATE TABLE addrs(id INTEGER NOT NULL PRIMARY KEY, ipaddr TEXT NOT "
          "NULL UNIQUE, hostname TEXT NOT NULL);"
          "CREATE TABLE connections(id INTEGER NOT NULL PRIMARY KEY, dbref "
          "INTEGER NOT NULL DEFAULT -1, name TEXT, addrid INTEGER NOT NULL,"
          "reason TEXT, ssl INTEGER, websocket INTEGER, FOREIGN KEY(addrid) "
          "REFERENCES addrs(id));"
          "CREATE INDEX conn_dbref_idx ON connections(dbref);"
          "CREATE INDEX conn_addr_idx ON connections(addrid);"
          "INSERT OR REPLACE INTO addrs(ipaddr, hostname) SELECT ipaddr, "
          "hostname FROM backup;"
          "INSERT INTO connections(id, dbref, name, reason, addrid) SELECT id, "
          "dbref, name, reason, (SELECT id FROM addrs WHERE addrs.ipaddr = "
          "backup.ipaddr) FROM backup;"
          "DROP TABLE backup;"
          "CREATE VIEW connlog(id, dbref, name, ipaddr, hostname, conn, "
          "disconn, reason, ssl, websocket) AS SELECT c.id, c.dbref, c.name, "
          "a.ipaddr, "
          "a.hostname, ts.conn, ts.disconn, c.reason, c.ssl, c.websocket FROM "
          "connections AS c "
          "JOIN timestamps AS ts ON c.id = ts.id JOIN addrs AS a ON c.addrid = "
          "a.id;"
          "CREATE TRIGGER conn_logout INSTEAD OF UPDATE OF disconn,reason ON "
          "connlog BEGIN UPDATE connections SET reason = NEW.reason WHERE id = "
          "NEW.id; UPDATE timestamps SET disconn = NEW.disconn WHERE id = "
          "NEW.id; "
          "END;"
          "PRAGMA user_version = " CONNLOG_VERSIONS ";"
          "COMMIT TRANSACTION",
          NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Upgrade failed: %s", err);
      sqlite3_free(err);
      sqlite3_exec(connlog_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      goto error_cleanup;
    }
  } else if (version == 2) {
    do_rawlog(LT_ERR, "Upgrading connlog db from 2 to %d", CONNLOG_VERSION);
    if (sqlite3_exec(
          connlog_db,
          "BEGIN TRANSACTION;"
          "ALTER TABLE connections ADD COLUMN ssl INTEGER;"
          "ALTER TABLE connections ADD COLUMN websocket INTEGER;"
          "CREATE VIEW connlog(id, dbref, name, ipaddr, hostname, conn, "
          "disconn, "
          "reason, ssl, websocket) AS SELECT c.id, c.dbref, c.name, a.ipaddr, "
          "a.hostname, ts.conn, "
          "ts.disconn, c.reason, c.ssl, c.websocket FROM connections AS c JOIN "
          "timestamps AS ts ON "
          "c.id = ts.id JOIN addrs AS a ON c.addrid = a.id;"
          "CREATE TRIGGER conn_logout INSTEAD OF UPDATE OF disconn,reason ON "
          "connlog BEGIN UPDATE connections SET reason = NEW.reason WHERE id = "
          "NEW.id; UPDATE timestamps SET disconn = NEW.disconn WHERE id = "
          "NEW.id; "
          "END;"
          "PRAGMA user_version = " CONNLOG_VERSIONS ";"
          "COMMIT TRANSACTION",
          NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Upgrade failed: %s", err);
      sqlite3_free(err);
      sqlite3_exec(connlog_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      goto error_cleanup;
    }
  } else if (version == 3) {
    do_rawlog(LT_ERR, "Upgrading connlog db from 3 to %d", CONNLOG_VERSION);
    if (sqlite3_exec(
          connlog_db,
          "BEGIN TRANSACTION;"
          "ALTER TABLE connections ADD COLUMN ssl INTEGER;"
          "ALTER TABLE connections ADD COLUMN websocket INTEGER;"
          "DROP VIEW connlog;"
          "CREATE VIEW connlog(id, dbref, name, ipaddr, hostname, conn, "
          "disconn, reason, ssl, websocket) AS SELECT c.id, c.dbref, c.name, "
          "a.ipaddr, "
          "a.hostname, ts.conn, ts.disconn, c.reason, c.ssl, c.websocket FROM "
          "connections AS c "
          "JOIN timestamps AS ts ON c.id = ts.id JOIN addrs AS a ON c.addrid = "
          "a.id;"
          "CREATE TRIGGER conn_logout INSTEAD OF UPDATE OF disconn,reason ON "
          "connlog BEGIN UPDATE connections SET reason = NEW.reason WHERE id = "
          "NEW.id; UPDATE timestamps SET disconn = NEW.disconn WHERE id = "
          "NEW.id; "
          "END;"
          "PRAGMA user_version = " CONNLOG_VERSIONS ";"
          "COMMIT TRANSACTION",
          NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Upgrade failed: %s", err);
      sqlite3_free(err);
      sqlite3_exec(connlog_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      goto error_cleanup;
    }
  } else if (version > CONNLOG_VERSION) {
    do_rawlog(LT_ERR, "connlog db has an incompatible version!");
    goto error_cleanup;
  }

  if (!rebooting) {
    /* Clean up connections without a logged disconnection time. */
    if (sqlite3_exec(
          connlog_db,
          "BEGIN TRANSACTION;"
          "DELETE FROM connections WHERE id IN (SELECT id FROM timestamps "
          "WHERE conn > (SELECT timestamp FROM checkpoint WHERE id = 1));"
          "DELETE FROM timestamps WHERE conn > (SELECT timestamp FROM "
          "checkpoint WHERE id = 1);"
          "UPDATE connlog SET reason = 'unexpected shutdown', "
          "disconn = (SELECT timestamp FROM checkpoint "
          "WHERE id = 1) WHERE disconn = 2147483647;"
          "COMMIT TRANSACTION",
          NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to update past logins: %s", err);
      sqlite3_free(err);
      goto error_cleanup;
    }
  }

  sq_register_loop(90, checkpoint_event, NULL, NULL);
  sq_register_loop(25 * 60 * 60 + 300, connlog_optimize, NULL, NULL);

#ifdef HAVE_PTHREAD_ATFORK
  pthread_atfork(conndb_prefork, conndb_postfork_parent, NULL);
#endif

  return 1;

error_cleanup:
  close_sql_db(connlog_db);
  connlog_db = NULL;
  return 0;
}

/** Shut down the connlog database.
 *
 * \param rebooting true if in a @shutdown/reboot
 */
void
shutdown_conndb(bool rebooting)
{
  char *err;

  if (!connlog_db) {
    return;
  }

  if (!rebooting) {
    if (sqlite3_exec(connlog_db,
                     "BEGIN TRANSACTION;"
                     "UPDATE connlog SET reason = 'shutdown', disconn = "
                     "strftime('%s', 'now') "
                     "WHERE disconn = 2147483647;"
                     "COMMIT TRANSACTION",
                     NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to update connlog database: %s", err);
      sqlite3_free(err);
    }
  }
  close_sql_db(connlog_db);
  connlog_db = NULL;
}

/** Register a new connection in the connlog
 *
 * \param ip the ip address of the connection
 * \param host the hostname of the connection
 * \param ssl true if a SSL connection
 * \return a unique id for the connection.
 */
int64_t
connlog_connection(const char *ip, const char *host, bool ssl)
{
  sqlite3_stmt *adder;
  int status;
  int64_t id = -1;

  if (!options.use_connlog) {
    return -1;
  }

  if (sqlite3_exec(connlog_db, "BEGIN TRANSACTION", NULL, NULL, NULL) !=
      SQLITE_OK) {
    return -1;
  }
  adder = prepare_statement(connlog_db,
                            "INSERT INTO timestamps(conn, disconn) VALUES "
                            "(strftime('%s', 'now'), 2147483647)",
                            "connlog.connection.time");
  do {
    status = sqlite3_step(adder);
  } while (is_busy_status(status));
  sqlite3_reset(adder);
  if (status == SQLITE_DONE) {
    id = sqlite3_last_insert_rowid(connlog_db);
  } else {
    do_rawlog(LT_ERR, "Failed to record connection timestamp from %s: %s", ip,
              sqlite3_errmsg(connlog_db));
    sqlite3_exec(connlog_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    return -1;
  }

  adder = prepare_statement(
    connlog_db,
    "INSERT INTO addrs(ipaddr, hostname) VALUES (?, ?) ON CONFLICT (ipaddr) DO "
    "UPDATE SET hostname=excluded.hostname",
    "connlog.connection.addr");
  sqlite3_bind_text(adder, 1, ip, strlen(ip), SQLITE_TRANSIENT);
  sqlite3_bind_text(adder, 2, host, strlen(host), SQLITE_TRANSIENT);
  do {
    status = sqlite3_step(adder);
  } while (is_busy_status(status));
  sqlite3_reset(adder);

  adder = prepare_statement(
    connlog_db,
    "INSERT INTO connections(id, addrid, ssl, websocket) VALUES (?, "
    "(SELECT id FROM addrs WHERE ipaddr = ?), ?, 0)",
    "connlog.connection.connection");
  sqlite3_bind_int64(adder, 1, id);
  sqlite3_bind_text(adder, 2, ip, strlen(ip), SQLITE_TRANSIENT);
  sqlite3_bind_int(adder, 3, ssl);

  do {
    status = sqlite3_step(adder);
  } while (is_busy_status(status));
  sqlite3_reset(adder);
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Failed to record connection from %s: %s", ip,
              sqlite3_errmsg(connlog_db));
    sqlite3_exec(connlog_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    return -1;
  }
  sqlite3_exec(connlog_db, "COMMIT TRANSACTION", NULL, NULL, NULL);
  return id;
}

/** Register a login for a connlog record.
 *
 * \param id the connlog record being used.
 * \param player the dbref of the player logging in.
 */
void
connlog_login(int64_t id, dbref player)
{
  sqlite3_stmt *login;
  int status;

  if (id == -1) {
    return;
  }

  login = prepare_statement(
    connlog_db, "UPDATE connections SET dbref = ?, name = ? WHERE id = ?",
    "connlog.login");
  sqlite3_bind_int(login, 1, player);
  sqlite3_bind_text(login, 2, Name(player), strlen(Name(player)),
                    SQLITE_TRANSIENT);
  sqlite3_bind_int64(login, 3, id);
  do {
    status = sqlite3_step(login);
  } while (is_busy_status(status));
  sqlite3_reset(login);
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Failed to record login to #%d: %s", player,
              sqlite3_errmsg(connlog_db));
  }
}

/** Mark that a connection is using websockets */
void
connlog_set_websocket(int64_t id)
{
  sqlite3_stmt *ws;
  int status;

  if (id == -1) {
    return;
  }

  ws = prepare_statement(connlog_db,
                         "UPDATE connections SET websocket = 1 WHERE id = ?",
                         "connlog.websocket");
  sqlite3_bind_int64(ws, 1, id);
  do {
    status = sqlite3_step(ws);
  } while (is_busy_status(status));
  sqlite3_reset(ws);
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Failed to record websocket for connlog id %lld: %s",
              (long long) id, sqlite3_errmsg(connlog_db));
  }
}

/** Record a disconnection in the connlog
 *
 * \param id the connlog record
 * \param reason reason for the disconnect, in UTF-8.
 */
void
connlog_disconnection(int64_t id, const char *reason)
{
  sqlite3_stmt *disco;
  int status;

  if (id == -1) {
    return;
  }

  disco = prepare_statement(connlog_db,
                            "UPDATE connlog SET disconn = strftime('%s', "
                            "'now'), reason = ? WHERE id = ?",
                            "connlog.disconn");
  sqlite3_bind_text(disco, 1, reason, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(disco, 2, id);

  do {
    status = sqlite3_step(disco);
  } while (is_busy_status(status));
  sqlite3_reset(disco);
}

FUNCTION(fun_connlog)
{
  sqlite3_str *query = NULL;
  sqlite3_stmt *search;
  char *q = NULL;
  int status;
  const char *sep = "|";
  dbref player;
  bool first = 1;
  int64_t id;
  char *sbp = *bp;
  bool time_constraint = 0;
  int idx = 1;
  char *ip = NULL, *host = NULL;
  int iplen, hostlen;
  bool first_constraint = 1;
  bool count_only = 0;

  if (!options.use_connlog) {
    safe_str(T("#-1 FUNCTION DISABLED"), buff, bp);
    return;
  }

  if (sqlite3_stricmp(args[0], "all") == 0) {
    player = -1;
  } else if (sqlite3_stricmp(args[0], "logged in") == 0) {
    player = -2;
  } else if (sqlite3_stricmp(args[0], "not logged in") == 0) {
    player = -3;
  } else {
    player = noisy_match_result(executor, args[0], TYPE_PLAYER,
                                MAT_ME | MAT_ABSOLUTE | MAT_PMATCH);
    if (!GoodObject(player) || !IsPlayer(player)) {
      safe_str("#-1 NOT A PLAYER", buff, bp);
      return;
    }
  }

  for (idx = 1; idx < nargs; idx += 1) {
    if (sqlite3_stricmp(args[idx], "count") == 0) {
      count_only = 1;
      break;
    }
  }

  query = sqlite3_str_new(connlog_db);
  sqlite3_str_appendall(query, "SELECT");
  if (count_only) {
    sqlite3_str_appendall(query, " count(*)");
  } else {
    sqlite3_str_appendall(query, " dbref, id");
  }
  sqlite3_str_appendall(query, " FROM connlog WHERE");
  if (player >= 0) {
    sqlite3_str_appendf(query, " dbref = %d", player);
    first_constraint = 0;
  } else if (player == -2) {
    sqlite3_str_appendall(query, " dbref != -1");
    first_constraint = 0;
  } else if (player == -3) {
    sqlite3_str_appendall(query, " dbref = -1");
    first_constraint = 0;
  }

  for (idx = 1; idx < nargs - 1;) {
    if (sqlite3_stricmp(args[idx], "count") == 0) {
      idx += 1;
      continue;
    } else if (sqlite3_stricmp(args[idx], "between") == 0) {
      int starttime, endtime;

      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        goto error_cleanup;
      } else if (nargs <= idx + 2) {
        safe_str("#-1 BETWEEN MISSING RANGE", buff, bp);
        goto error_cleanup;
      } else if (!is_strict_integer(args[idx + 1]) ||
                 !is_strict_integer(args[idx + 2])) {
        safe_str(T(e_ints), buff, bp);
        goto error_cleanup;
      }

      starttime = parse_integer(args[idx + 1]);
      endtime = parse_integer(args[idx + 2]);

      if (endtime < starttime || starttime > mudtime) {
        if (count_only) {
          safe_chr('0', buff, bp);
        }
        goto error_cleanup;
      }

      if (first_constraint) {
        first_constraint = 0;
      } else {
        sqlite3_str_appendall(query, " AND");
      }
      sqlite3_str_appendf(query, " (conn <= %d AND disconn >= %d)", endtime,
                          starttime);
      time_constraint = 1;
      idx += 3;
    } else if (sqlite3_stricmp(args[idx], "at") == 0) {
      int when;

      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        goto error_cleanup;
      } else if (nargs <= idx + 1) {
        safe_str("#-1 AT MISSING TIME", buff, bp);
        goto error_cleanup;
      } else if (!is_strict_integer(args[idx + 1])) {
        safe_str(T(e_int), buff, bp);
        goto error_cleanup;
      }
      when = parse_integer(args[idx + 1]);
      if (when > mudtime) {
        if (count_only) {
          safe_chr('0', buff, bp);
        }
        goto error_cleanup;
      }
      if (first_constraint) {
        first_constraint = 0;
      } else {
        sqlite3_str_appendall(query, " AND");
      }
      sqlite3_str_appendf(query, " (conn <= %d AND disconn >= %d)", when, when);
      time_constraint = 1;
      idx += 2;
    } else if (sqlite3_stricmp(args[idx], "before") == 0) {
      int when;
      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        goto error_cleanup;
      } else if (nargs <= idx + 1) {
        safe_str("#-1 BEFORE MISSING TIME", buff, bp);
        goto error_cleanup;
      } else if (!is_strict_integer(args[idx + 1])) {
        safe_str(T(e_int), buff, bp);
        goto error_cleanup;
      }
      when = parse_integer(args[idx + 1]);
      if (first_constraint) {
        first_constraint = 0;
      } else {
        sqlite3_str_appendall(query, " AND");
      }
      sqlite3_str_appendf(query, " conn < %d", when);
      time_constraint = 1;
      idx += 2;
    } else if (sqlite3_stricmp(args[idx], "after") == 0) {
      int when;
      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        goto error_cleanup;
      } else if (nargs <= idx + 1) {
        safe_str("#-1 AFTER MISSING TIME", buff, bp);
        goto error_cleanup;
      } else if (!is_strict_integer(args[idx + 1])) {
        safe_str(T(e_int), buff, bp);
        goto error_cleanup;
      }
      when = parse_integer(args[idx + 1]);
      if (when >= mudtime) {
        if (count_only) {
          safe_chr('0', buff, bp);
        }
        goto error_cleanup;
      }
      if (first_constraint) {
        first_constraint = 0;
      } else {
        sqlite3_str_appendall(query, " AND");
      }
      sqlite3_str_appendf(query,
                          " (conn > %d OR (conn <= %d AND disconn >= %d))",
                          when, when, when);
      time_constraint = 1;
      idx += 2;
    } else if (sqlite3_stricmp(args[idx], "ip") == 0) {
      char *escaped;
      int len;
      if (nargs <= idx + 1) {
        safe_str("#-1 IP MISSING PATTERN", buff, bp);
        goto error_cleanup;
      } else if (ip) {
        safe_str("#-1 DUPLICATE CONSTRAINT", buff, bp);
        goto error_cleanup;
      }
      escaped = glob_to_like(args[idx + 1], '$', &len);
      ip = latin1_to_utf8(escaped, len, &iplen, "string");
      mush_free(escaped, "string");
      if (first_constraint) {
        first_constraint = 0;
      } else {
        sqlite3_str_appendall(query, " AND");
      }
      sqlite3_str_appendall(query, " ipaddr LIKE @ipaddr ESCAPE '$'");
      idx += 2;
    } else if (sqlite3_stricmp(args[idx], "hostname") == 0) {
      char *escaped;
      int len;
      if (nargs <= idx + 1) {
        safe_str("#-1 HOSTNAME MISSING PATTERN", buff, bp);
        goto error_cleanup;
      } else if (host) {
        safe_str("#-1 DUPLICATE CONSTRAINT", buff, bp);
        goto error_cleanup;
      }
      escaped = glob_to_like(args[idx + 1], '$', &len);
      host = latin1_to_utf8(escaped, len, &hostlen, "string");
      mush_free(escaped, "string");
      if (first_constraint) {
        first_constraint = 0;
      } else {
        sqlite3_str_appendall(query, " AND");
      }
      sqlite3_str_appendall(query, " hostname LIKE @hostname ESCAPE '$'");
      idx += 2;
    } else {
      safe_str("#-1 INVALID TIME SPEC", buff, bp);
      goto error_cleanup;
    }
  }
  if (idx == nargs - 1) {
    sep = args[idx];
  }

  sqlite3_str_appendall(query, " ORDER BY id");
  q = sqlite3_str_finish(query);
  query = NULL;

  search = prepare_statement_cache(connlog_db, q, "connlog.fun.list", 0);

  if (!search) {
    safe_str("#-1 SQLITE ERROR", buff, bp);
    do_rawlog(LT_ERR, "Failed to compile query: %s", q);
    sqlite3_free(q);
    goto error_cleanup;
  }

  sqlite3_free(q);

  if (ip) {
    idx = sqlite3_bind_parameter_index(search, "@ipaddr");
    sqlite3_bind_text(search, idx, ip, iplen, free_string);
  }

  if (host) {
    idx = sqlite3_bind_parameter_index(search, "@hostname");
    sqlite3_bind_text(search, idx, host, hostlen, free_string);
  }

  do {
    status = sqlite3_step(search);
    if (status == SQLITE_ROW) {
      if (count_only) {
        safe_integer(sqlite3_column_int64(search, 0), buff, bp);
        status = SQLITE_DONE;
        break;
      }

      player = sqlite3_column_int(search, 0);
      id = sqlite3_column_int64(search, 1);
      if (first) {
        first = 0;
      } else {
        safe_str(sep, buff, bp);
      }
      safe_dbref(player, buff, bp);
      safe_chr(' ', buff, bp);
      safe_integer(id, buff, bp);
    }
  } while (status == SQLITE_ROW || is_busy_status(status));

  if (status != SQLITE_DONE) {
    *bp = sbp;
    safe_format(buff, bp, "#-1 SQLITE ERROR %s", sqlite3_errmsg(connlog_db));
  }
  sqlite3_finalize(search);
  return;

error_cleanup:
  if (query) {
    sqlite3_str_reset(query);
    sqlite3_str_finish(query);
  }
  if (ip) {
    mush_free(ip, "string");
  }
  if (host) {
    mush_free(host, "string");
  }
}

FUNCTION(fun_connrecord)
{
  int64_t id;
  const char *sep = " ";
  sqlite3_stmt *rec;
  int status;

  if (!options.use_connlog) {
    safe_str(T("#-1 FUNCTION DISABLED"), buff, bp);
    return;
  }

  if (!is_strict_int64(args[0])) {
    safe_str(T(e_int), buff, bp);
    return;
  }

  id = parse_int64(args[0], NULL, 10);

  if (nargs == 2) {
    sep = args[1];
  }

  rec = prepare_statement(
    connlog_db,
    "SELECT dbref, ifnull(name, '-'), ipaddr, hostname, conn, disconn, "
    "ifnull(reason, '-'), ifnull(ssl, 0), ifnull(websocket, 0) FROM connlog "
    "WHERE id = ?",
    "connlog.fun.record");
  if (!rec) {
    safe_str("#-1 SQLITE ERROR", buff, bp);
    return;
  }

  sqlite3_bind_int64(rec, 1, id);
  do {
    status = sqlite3_step(rec);
  } while (is_busy_status(status));
  if (status == SQLITE_ROW) {
    int32_t disco;
    safe_dbref(sqlite3_column_int(rec, 0), buff, bp);
    safe_str(sep, buff, bp);
    safe_str((const char *) sqlite3_column_text(rec, 1), buff, bp);
    safe_str(sep, buff, bp);
    safe_str((const char *) sqlite3_column_text(rec, 2), buff, bp);
    safe_str(sep, buff, bp);
    safe_str((const char *) sqlite3_column_text(rec, 3), buff, bp);
    safe_str(sep, buff, bp);
    safe_integer(sqlite3_column_int(rec, 4), buff, bp);
    disco = sqlite3_column_int(rec, 5);
    safe_str(sep, buff, bp);
    if (disco == INT32_MAX) {
      safe_str("-1", buff, bp);
    } else {
      safe_integer(disco, buff, bp);
    }
    safe_str(sep, buff, bp);
    safe_str((const char *) sqlite3_column_text(rec, 6), buff, bp);
    safe_str(sep, buff, bp);
    safe_integer(sqlite3_column_int(rec, 7), buff, bp);
    safe_str(sep, buff, bp);
    safe_integer(sqlite3_column_int(rec, 8), buff, bp);
  } else if (status != SQLITE_DONE) {
    safe_format(buff, bp, "#-1 SQLITE ERROR %s", sqlite3_errmsg(connlog_db));
  } else {
    safe_str("#-1 NO SUCH RECORD", buff, bp);
  }
  sqlite3_reset(rec);
}

FUNCTION(fun_addrlog)
{
  sqlite3_stmt *stmt;
  sqlite3_str *query;
  char *utf8, *pat;
  int llen;
  bool count_only = 0, free_sep = 0, first = 1;
  char default_sep[] = "|";
  char *sep = default_sep;
  int n = 0, rc;
  char *sbp = *bp;

  if (!options.use_connlog) {
    safe_str(T("#-1 FUNCTION DISABLED"), buff, bp);
    return;
  }

  if (!See_All(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (sqlite3_stricmp(args[0], "count") == 0) {
    count_only = 1;
    n = 1;
  }

  if (nargs == n + 1) {
    safe_str(T("#-1 MISSING ARGUMENTS"), buff, bp);
    return;
  }

  query = sqlite3_str_new(connlog_db);
  sqlite3_str_appendall(query, "SELECT");
  if (count_only) {
    sqlite3_str_appendall(query, " count(*)");
  } else {
    sqlite3_str_appendall(query, " ipaddr, hostname");
  }
  sqlite3_str_appendall(query, " FROM addrs WHERE");
  if (sqlite3_stricmp(args[n], "ip") == 0) {
    sqlite3_str_appendall(query, " ipaddr");
  } else if (sqlite3_stricmp(args[n], "hostname") == 0) {
    sqlite3_str_appendall(query, " hostname");
  } else {
    char *s;
    safe_str(T("#-1 INVALID ARGUMENT"), buff, bp);
    s = sqlite3_str_finish(query);
    sqlite3_free(s);
    return;
  }

  utf8 = latin1_to_utf8(args[n + 1], arglens[n + 1], NULL, "utf8.string");
  pat = glob_to_like(utf8, '$', &llen);
  mush_free(utf8, "utf8.string");
  sqlite3_str_appendall(query, " LIKE ? ESCAPE '$'");

  if (nargs > n + 2) {
    sep = latin1_to_utf8(args[n + 2], arglens[n + 2], NULL, "utf8.string");
    free_sep = 1;
  }

  utf8 = sqlite3_str_finish(query);
  stmt = prepare_statement_cache(connlog_db, utf8, "addrlog.query", 0);
  sqlite3_free(utf8);

  if (!stmt) {
    safe_str(T("#-1 SQLITE ERROR"), buff, bp);
    mush_free(pat, "string");
    if (free_sep) {
      mush_free(sep, "utf8.string");
    }
    return;
  }

  sqlite3_bind_text(stmt, 1, pat, llen, free_string);

  do {
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
      const char *s1, *s2;

      if (count_only) {
        safe_integer(sqlite3_column_int64(stmt, 0), buff, bp);
        rc = SQLITE_DONE;
        break;
      }
      if (first) {
        first = 0;
      } else {
        safe_str(sep, buff, bp);
      }
      s1 = (const char *) sqlite3_column_text(stmt, 0);
      s2 = (const char *) sqlite3_column_text(stmt, 1);
      safe_format(buff, bp, "%s %s", s1, s2);
    }
  } while (is_busy_status(rc) || rc == SQLITE_ROW);

  if (rc != SQLITE_DONE) {
    *bp = sbp;
    safe_format(buff, bp, T("#-1 SQLITE ERROR: %s"),
                sqlite3_errmsg(connlog_db));
  }

  sqlite3_finalize(stmt);
  if (free_sep) {
    mush_free(sep, "utf8.string");
  }
}
