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
#define CONNLOG_VERSION 2
#define CONNLOG_VERSIONS "2"

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
          "reason TEXT, FOREIGN KEY(addrid) REFERENCES addrs(id));"
          "CREATE INDEX conn_dbref_idx ON connections(dbref);"
          "CREATE INDEX conn_addr_idx ON connections(addrid);"
          "CREATE TABLE checkpoint(id INTEGER NOT NULL PRIMARY KEY, timestamp "
          "INTEGER NOT NULL);"
          "INSERT INTO checkpoint VALUES (1, strftime('%s', 'now'))",
          NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to build connlog database: %s", err);
      sqlite3_free(err);
      goto error_cleanup;
    }
  } else if (version == 1) {
    do_rawlog(LT_ERR, "Upgrading connlog db from 1 to 2");
    if (sqlite3_exec(
          connlog_db,
          "BEGIN TRANSACTION;"
          "CREATE TABLE backup AS SELECT * FROM connections;"
          "DROP TABLE connections;"
          "CREATE TABLE addrs(id INTEGER NOT NULL PRIMARY KEY, ipaddr TEXT NOT "
          "NULL UNIQUE, hostname TEXT NOT NULL);"
          "CREATE TABLE connections(id INTEGER NOT NULL PRIMARY KEY, dbref "
          "INTEGER NOT NULL DEFAULT -1, name TEXT, addrid INTEGER NOT NULL,"
          "reason TEXT, FOREIGN KEY(addrid) REFERENCES addrs(id));"
          "CREATE INDEX conn_dbref_idx ON connections(dbref);"
          "CREATE INDEX conn_addr_idx ON connections(addrid);"
          "INSERT OR REPLACE INTO addrs(ipaddr, hostname) SELECT ipaddr, "
          "hostname FROM backup;"
          "INSERT INTO connections(id, dbref, name, reason, addrid) SELECT id, "
          "dbref, name, reason, (SELECT id FROM addrs WHERE addrs.ipaddr = "
          "backup.ipaddr) FROM backup;"
          "DROP TABLE backup;"
          "PRAGMA user_version = " CONNLOG_VERSIONS ";"
          "COMMIT TRANSACTION;"
          "VACUUM",
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
          "UPDATE connections SET reason = 'unexpected shutdown' WHERE id IN "
          "(SELECT id FROM timestamps WHERE disconn = 2147483647);"
          "UPDATE timestamps SET disconn = (SELECT timestamp FROM checkpoint "
          "WHERE id = 1) WHERE disconn = 2147483647;"
          "COMMIT TRANSACTION",
          NULL, NULL, &err) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to update past logins: %s", err);
      sqlite3_free(err);
      goto error_cleanup;
    }
  }

  sq_register_loop(90, checkpoint_event, NULL, NULL);

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
                     "UPDATE connections SET reason = 'shutdown' WHERE id IN "
                     "(SELECT id FROM timestamps WHERE disconn = 2147483647);"
                     "UPDATE timestamps SET disconn = strftime('%s', 'now') "
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
 * \return a unique id for the connection.
 */
int64_t
connlog_connection(const char *ip, const char *host)
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
              sqlite3_errstr(status));
    sqlite3_exec(connlog_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    return -1;
  }

  adder = prepare_statement(
    connlog_db, "INSERT OR IGNORE INTO addrs(ipaddr, hostname) VALUES (?, ?)",
    "connlog.connection.addr");
  sqlite3_bind_text(adder, 1, ip, strlen(ip), SQLITE_TRANSIENT);
  sqlite3_bind_text(adder, 2, host, strlen(host), SQLITE_TRANSIENT);
  do {
    status = sqlite3_step(adder);
  } while (is_busy_status(status));

  adder = prepare_statement(connlog_db,
                            "INSERT INTO connections(id, addrid) VALUES (?, "
                            "(SELECT id FROM addrs WHERE ipaddr = ?))",
                            "connlog.connection.connection");
  sqlite3_bind_int64(adder, 1, id);
  sqlite3_bind_text(adder, 2, ip, strlen(ip), SQLITE_TRANSIENT);
  do {
    status = sqlite3_step(adder);
  } while (is_busy_status(status));
  sqlite3_reset(adder);
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Failed to record connection from %s: %s", ip,
              sqlite3_errstr(status));
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
              sqlite3_errstr(status));
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
  sqlite3_stmt *disco_c, *disco_t;
  int status;

  if (id == -1) {
    return;
  }

  disco_t = prepare_statement(
    connlog_db,
    "UPDATE timestamps SET disconn = strftime('%s', 'now') WHERE id = ?",
    "connlog.disconn.1");
  disco_c = prepare_statement(connlog_db,
                              "UPDATE connections SET reason = ? WHERE id = ?",
                              "connlog.disconn.2");
  sqlite3_bind_int64(disco_t, 1, id);
  sqlite3_bind_text(disco_c, 1, reason, strlen(reason), SQLITE_TRANSIENT);
  sqlite3_bind_int64(disco_c, 2, id);

  sqlite3_exec(connlog_db, "BEGIN TRANSACTION", NULL, NULL, NULL);
  do {
    status = sqlite3_step(disco_t);
  } while (is_busy_status(status));
  sqlite3_reset(disco_t);
  if (status == SQLITE_DONE) {
    do {
      status = sqlite3_step(disco_c);
    } while (is_busy_status(status));
    sqlite3_reset(disco_c);
    if (status == SQLITE_DONE) {
      sqlite3_exec(connlog_db, "COMMIT TRANSACTION", NULL, NULL, NULL);
      return;
    }
  }
  sqlite3_exec(connlog_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
}

FUNCTION(fun_connlog)
{
  char query[BUFFER_LEN];
  char *qp = query;
  sqlite3_stmt *search;
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

  if (!options.use_connlog) {
    safe_str(T("#-1 FUNCTION DISABLED"), buff, bp);
    return;
  }

  if (strcasecmp(args[0], "all") == 0) {
    player = -1;
  } else if (strcasecmp(args[0], "logged in") == 0) {
    player = -2;
  } else if (strcasecmp(args[0], "not logged in") == 0) {
    player = -3;
  } else {
    player = noisy_match_result(executor, args[0], TYPE_PLAYER,
                                MAT_ME | MAT_ABSOLUTE | MAT_PMATCH);
    if (!GoodObject(player) || !IsPlayer(player)) {
      safe_str("#-1 NOT A PLAYER", buff, bp);
      return;
    }
  }

  safe_str("SELECT dbref, connections.id FROM timestamps JOIN connections ON "
           "timestamps.id = connections.id JOIN addrs ON addrs.id = "
           "connections.addrid WHERE",
           query, &qp);
  if (player >= 0) {
    safe_format(query, &qp, " dbref = %d", player);
    first_constraint = 0;
  } else if (player == -2) {
    safe_str(" dbref != -1", query, &qp);
    first_constraint = 0;
  } else if (player == -3) {
    safe_str(" dbref = -1", query, &qp);
    first_constraint = 0;
  }

  while (idx < nargs - 1) {
    if (strcmp(args[idx], "between") == 0) {
      int starttime, endtime;

      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        return;
      } else if (nargs <= idx + 2) {
        safe_str("#-1 BETWEEN MISSING RANGE", buff, bp);
        return;
      } else if (!is_strict_integer(args[idx + 1]) ||
                 !is_strict_integer(args[idx + 2])) {
        safe_str(T(e_ints), buff, bp);
        return;
      }

      starttime = parse_integer(args[idx + 1]);
      endtime = parse_integer(args[idx + 2]);

      if (first_constraint) {
        first_constraint = 0;
      } else {
        safe_str(" AND", query, &qp);
      }
      safe_format(query, &qp, " (conn <= %d AND disconn >= %d)", endtime,
                  starttime);
      time_constraint = 1;
      idx += 3;
    } else if (strcmp(args[idx], "at") == 0) {
      int when;

      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        return;
      } else if (nargs <= idx + 1) {
        safe_str("#-1 AT MISSING TIME", buff, bp);
        return;
      } else if (!is_strict_integer(args[idx + 1])) {
        safe_str(T(e_int), buff, bp);
        return;
      }
      when = parse_integer(args[idx + 1]);
      if (first_constraint) {
        first_constraint = 0;
      } else {
        safe_str(" AND", query, &qp);
      }
      safe_format(query, &qp, " (conn <= %d AND disconn >= %d)", when, when);
      time_constraint = 1;
      idx += 2;
    } else if (strcasecmp(args[idx], "before") == 0) {
      int when;
      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        return;
      } else if (nargs <= idx + 1) {
        safe_str("#-1 BEFORE MISSING TIME", buff, bp);
        return;
      } else if (!is_strict_integer(args[idx + 1])) {
        safe_str(T(e_int), buff, bp);
        return;
      }
      when = parse_integer(args[idx + 1]);
      if (first_constraint) {
        first_constraint = 0;
      } else {
        safe_str(" AND", query, &qp);
      }
      safe_format(query, &qp, " conn < %d", when);
      time_constraint = 1;
      idx += 2;
    } else if (strcasecmp(args[idx], "after") == 0) {
      int when;
      if (time_constraint) {
        safe_str("#-1 TOO MANY CONSTRAINTS", buff, bp);
        return;
      } else if (nargs <= idx + 1) {
        safe_str("#-1 AFTER MISSING TIME", buff, bp);
        return;
      } else if (!is_strict_integer(args[idx + 1])) {
        safe_str(T(e_int), buff, bp);
        return;
      }
      when = parse_integer(args[idx + 1]);
      if (first_constraint) {
        first_constraint = 0;
      } else {
        safe_str(" AND", query, &qp);
      }
      safe_format(query, &qp, " (conn > %d OR (conn <= %d AND disconn >= %d))",
                  when, when, when);
      time_constraint = 1;
      idx += 2;
    } else if (strcasecmp(args[idx], "ip") == 0) {
      char *escaped;
      int len;
      if (nargs <= idx + 1) {
        safe_str("#-1 IP MISSING PATTERN", buff, bp);
        return;
      } else if (ip) {
        safe_str("#-1 DUPLICATE CONSTRAINT", buff, bp);
        mush_free(ip, "string");
        if (host) {
          mush_free(host, "string");
        }
        return;
      }
      escaped = glob_to_like(args[idx + 1], '$', &len);
      ip = latin1_to_utf8(escaped, len, &iplen, "string");
      mush_free(escaped, "string");
      if (first_constraint) {
        first_constraint = 0;
      } else {
        safe_str(" AND", query, &qp);
      }
      safe_str(" ipaddr LIKE @ipaddr ESCAPE '$'", query, &qp);
      idx += 2;
    } else if (strcasecmp(args[idx], "hostname") == 0) {
      char *escaped;
      int len;
      if (nargs <= idx + 1) {
        safe_str("#-1 HOSTNAME MISSING PATTERN", buff, bp);
        return;
      } else if (host) {
        safe_str("#-1 DUPLICATE CONSTRAINT", buff, bp);
        mush_free(host, "string");
        if (ip) {
          mush_free(ip, "string");
        }
        return;
      }
      escaped = glob_to_like(args[idx + 1], '$', &len);
      host = latin1_to_utf8(escaped, len, &hostlen, "string");
      mush_free(escaped, "string");
      if (first_constraint) {
        first_constraint = 0;
      } else {
        safe_str(" AND", query, &qp);
      }
      safe_str(" hostname LIKE @hostname ESCAPE '$'", query, &qp);
      idx += 2;
    } else {
      safe_str("#-1 INVALID TIME SPEC", buff, bp);
      return;
    }
  }

  if (idx == nargs - 1) {
    sep = args[idx];
  }

  safe_str(" ORDER BY connections.id", query, &qp);
  *qp = '\0';

  search = prepare_statement_cache(connlog_db, query, "connlog.fun.list", 0);
  if (!search) {
    safe_str("#-1 SQLITE ERROR", buff, bp);
    do_rawlog(LT_ERR, "Failed to compile query: %s", query);
    return;
  }

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
    safe_format(buff, bp, "#-1 SQLITE ERROR %s", sqlite3_errstr(status));
  }
  sqlite3_finalize(search);
}

FUNCTION(fun_connrecord)
{
  int64_t id;
  const char *sep = " ";
  sqlite3_stmt *rec;
  int status;

  if (!is_strict_int64(args[0])) {
    safe_str(T(e_int), buff, bp);
  }

  id = parse_int64(args[0], NULL, 10);

  if (nargs == 2) {
    sep = args[1];
  }

  rec = prepare_statement(
    connlog_db,
    "SELECT dbref, ifnull(name, '-'), ipaddr, hostname, conn, disconn, "
    "ifnull(reason, '-') FROM timestamps JOIN connections ON timestamps.id = "
    "connections.id JOIN addrs ON connections.addrid = "
    "addrs.id WHERE timestamps.id = ?",
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
  } else if (status != SQLITE_DONE) {
    safe_format(buff, bp, "#-1 SQLITE ERROR %s", sqlite3_errstr(status));
  } else {
    safe_str("#-1 NO SUCH RECORD", buff, bp);
  }
  sqlite3_reset(rec);
}
