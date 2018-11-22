/**
 * \file plyrlist.c
 *
 * \brief Player list management for PennMUSH.
 *
 *
 */

#include "copyrite.h"

#include <string.h>
#include <stdlib.h>

#include "attrib.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "mushdb.h"
#include "parse.h"
#include "strutil.h"
#include "mushsql.h"
#include "log.h"
#include "charconv.h"

static int hft_initialized = 0;
static void init_hft(void);

static void
init_hft(void)
{
  if (!hft_initialized) {
    char *errmsg = NULL;
    sqlite3 *sqldb = get_shared_db();

    if (sqlite3_exec(sqldb,
                     "CREATE TABLE players(name TEXT NOT NULL PRIMARY KEY, "
                     "dbref INTEGER NOT NULL, FOREIGN KEY(dbref) REFERENCES "
                     "objects(dbref) ON DELETE CASCADE) WITHOUT ROWID;"
                     "CREATE INDEX plyr_dbref_idx ON players(dbref)",
                     NULL, NULL, &errmsg) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to create players table: %s", errmsg);
      sqlite3_free(errmsg);
    } else {
      hft_initialized = 1;
    }
  }
}

/** Clear the player list htab. */
void
clear_players(void)
{
  if (hft_initialized) {
    char *errmsg = NULL;
    sqlite3 *sqldb = get_shared_db();

    if (sqlite3_exec(sqldb, "DELETE FROM players", NULL, NULL, &errmsg) !=
        SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to wipe players table: %s", errmsg);
      sqlite3_free(errmsg);
    }
  } else {
    init_hft();
  }
}

/* name is assumed to be latin-1 */
static void
add_player_name(sqlite3 *sqldb, const char *name, dbref player)
{
  sqlite3_stmt *adder;
  int ulen;
  char *utf8;
  int status;

  init_hft();

  adder = prepare_statement(
    sqldb, "INSERT INTO players(name, dbref) VALUES(upper(?), ?)",
    "plyrlist.add");
  if (!adder) {
    return;
  }

  utf8 = latin1_to_utf8(name, strlen(name), &ulen, "string");
  sqlite3_bind_text(adder, 1, utf8, ulen, free_string);
  sqlite3_bind_int(adder, 2, player);

  do {
    status = sqlite3_step(adder);
  } while (is_busy_status(status));

  if (status != SQLITE_DONE && status != SQLITE_CONSTRAINT) {
    do_rawlog(LT_ERR, "Unable to execute players add query for #%d/%s: %s (%d)",
              player, name, sqlite3_errmsg(sqldb), status);
  }

  sqlite3_reset(adder);
}

/** Add a player to the player list htab.
 * \param player dbref of player to add.
 */
void
add_player(dbref player)
{
  add_player_name(get_shared_db(), Name(player), player);
}

/** Add a player's alias list to the player list htab.
 * \param player dbref of player to add.
 * \param alias list of names ot use as hash table keys for player,
 * semicolon-separated.
 */
void
add_player_alias(dbref player, const char *alias, bool intransaction)
{
  char tbuf1[BUFFER_LEN], *s, *sp;
  int status;
  char *errmsg;
  sqlite3 *sqldb = get_shared_db();

  init_hft();

  if (!intransaction) {
    status = sqlite3_exec(sqldb, "BEGIN TRANSACTION", NULL, NULL, &errmsg);
    if (status != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to start players add alias transaction: %s",
                errmsg);
      sqlite3_free(errmsg);
      return;
    }
  }

  mush_strncpy(tbuf1, alias, BUFFER_LEN);
  s = trim_space_sep(tbuf1, ALIAS_DELIMITER);
  while (s) {
    sp = split_token(&s, ALIAS_DELIMITER);
    while (sp && *sp && *sp == ' ')
      sp++;
    if (sp && *sp) {
      add_player_name(sqldb, sp, player);
    }
  }

  if (!intransaction) {
    status = sqlite3_exec(sqldb, "COMMIT TRANSACTION", NULL, NULL, &errmsg);
    if (status != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to commit players add alias transaction: %s",
                errmsg);
      sqlite3_free(errmsg);
      sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      return;
    }
  }
}

/** Look up a player in the player list htab (or by dbref).
 * \param name name of player to find.
 * \return dbref of player, or NOTHING.
 */
dbref
lookup_player(const char *name)
{
  dbref d;

  if (!name || !*name)
    return NOTHING;
  if (*name == NUMBER_TOKEN) {
    d = parse_objid(name);
    if (GoodObject(d) && IsPlayer(d))
      return d;
    else
      return NOTHING;
  }
  if (*name == LOOKUP_TOKEN)
    name++;
  return lookup_player_name(name);
}

/** Look up a player in the player list htab only.
 * \param name name of player to find.
 * \return dbref of player, or NOTHING.
 */
dbref
lookup_player_name(const char *name)
{
  sqlite3_stmt *looker;
  dbref d = NOTHING;
  char *utf8;
  int ulen;
  int status;
  sqlite3 *sqldb = get_shared_db();

  if (!hft_initialized) {
    return NOTHING;
  }

  looker =
    prepare_statement(sqldb, "SELECT dbref FROM players WHERE name = upper(?)",
                      "plyrlist.lookup");
  if (!looker) {
    return NOTHING;
  }

  utf8 = latin1_to_utf8(name, strlen(name), &ulen, "string");
  sqlite3_bind_text(looker, 1, utf8, ulen, free_string);

  do {
    status = sqlite3_step(looker);
  } while (is_busy_status(status));

  if (status == SQLITE_ROW) {
    d = sqlite3_column_int(looker, 0);
  } else if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Unable to run players lookup query for %s: %s", name,
              sqlite3_errmsg(sqldb));
  }
  sqlite3_reset(looker);
  return d;
}

/** Remove a player from the player list htab.
 * \param player dbref of player to remove.
 * \param alias key to remove if given.
 */
void
delete_player(dbref player)
{
  sqlite3_stmt *deleter;
  int status;
  sqlite3 *sqldb = get_shared_db();

  init_hft();

  deleter = prepare_statement(sqldb, "DELETE FROM players WHERE dbref = ?",
                              "plyrlist.delete");
  if (!deleter) {
    return;
  }

  sqlite3_bind_int(deleter, 1, player);
  status = sqlite3_step(deleter);
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR,
              "Unable to execute query to delete players names for #%d: %s",
              player, sqlite3_errmsg(sqldb));
  }
  sqlite3_reset(deleter);
}

/** Reset all of a player's player list entries (names/aliases).
 * This is called when a player changes name or alias.
 * We remove all their old entries, and add back their new ones.
 * \param player dbref of player
 * \param name player's new name
 * \param alias player's new aliases
 */
void
reset_player_list(dbref player, const char *name, const char *alias)
{
  char tbuf[BUFFER_LEN];
  int status;
  char *errmsg;
  sqlite3 *sqldb = get_shared_db();

  init_hft();

  if (!name) {
    name = Name(player);
  }

  if (alias) {
    mush_strncpy(tbuf, alias, sizeof tbuf);
  } else {
    /* We are not changing aliases, just name, but we need to get the
     * aliases anyway, since we may change name to something that's
     * in the alias, and thus must not be deleted.
     */
    ATTR *a = atr_get_noparent(player, "ALIAS");
    if (a) {
      mush_strncpy(tbuf, atr_value(a), sizeof tbuf);
    } else {
      tbuf[0] = '\0';
    }
  }

  status = sqlite3_exec(sqldb, "BEGIN TRANSACTION", NULL, NULL, &errmsg);
  if (status != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to start players replace transaction: %s",
              errmsg);
    sqlite3_free(errmsg);
    return;
  }

  /* Delete all the old stuff */
  delete_player(player);
  /* Add in the new stuff */
  add_player_name(sqldb, name, player);
  add_player_alias(player, tbuf, 1);

  status = sqlite3_exec(sqldb, "COMMIT TRANSACTION", NULL, NULL, &errmsg);
  if (status != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to commit players replace transaction: %s",
              errmsg);
    sqlite3_free(errmsg);
    sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
  }
}
