/**
 * \file help.c
 *
 * \brief The PennMUSH help system.
 *
 *
 */

/* This might have to be uncommented on some linux distros... */
/* #define _XOPEN_SOURCE 600 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "help.h"
#include "ansi.h"
#include "command.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "flags.h"
#include "htab.h"
#include "log.h"
#include "memcheck.h"
#include "mymalloc.h"
#include "notify.h"
#include "parse.h"
#include "pueblo.h"
#include "strutil.h"
#include "mushsql.h"
#include "charconv.h"
#include "game.h"
#include "mypcre.h"

#define HELPDB_APP_ID 0x42010FF1
#define HELPDB_VERSION 6
#define HELPDB_VERSIONS "6"

#define LINE_SIZE 8192
#define TOPIC_NAME_LEN 30

struct help_entry {
  char *name;
  char *body;
  int bodylen;
};

HASHTAB help_files; /**< Help filenames hash table */

sqlite3 *help_db = NULL;

static int help_init = 0;

static void do_new_spitfile(dbref, const char *, sqlite3_int64, help_file *);
static const char *string_spitfile(help_file *help_dat, char *arg1);

static bool help_entry_exists(help_file *, const char *, sqlite3_int64 *);
static struct help_entry *help_find_entry(help_file *help_dat, const char *,
                                          sqlite3_int64);
static void help_free_entry(struct help_entry *);
static char **list_matching_entries(const char *pattern, help_file *help_dat,
                                    int *len);
static void free_entry_list(char **, int len);

static const char *normalize_entry(help_file *help_dat, const char *arg1);

static bool help_delete_entries(help_file *h);
static bool help_populate_entries(help_file *h);
static bool help_build_index(help_file *h);

static bool is_index_entry(const char *, int *);
static char *entries_from_offset(help_file *, int);

static bool needs_rebuild(help_file *h, sqlite3_int64 *pcurrmodts);
static bool update_timestamp(help_file *h, sqlite3_int64 currmodts);

/** Linked list of help topic names. */
typedef struct TLIST {
  char topic[TOPIC_NAME_LEN + 1]; /**< Name of topic */
  struct TLIST *next;             /**< Pointer to next list entry */
} tlist;

tlist *top = NULL; /**< Pointer to top of linked list of topic names */

static char *
help_search(dbref executor, help_file *h, char *_term, char *delim,
            int *matches)
{
  char results[BUFFER_LEN];
  char *rp;
  sqlite3_stmt *searcher;
  int status;
  bool first = 1;
  int count = 0;
  char *utf8;
  int ulen;

  if (!_term || !*_term) {
    notify(executor, T("What do you want to search for?"));
    return NULL;
  }

  rp = results;

  searcher = prepare_statement(
    help_db,
    "SELECT name, snippet(helpfts, 0, '" ANSI_UNDERSCORE "', '" ANSI_END
    "', '...', 10) FROM helpfts JOIN topics ON topics.bodyid = helpfts.rowid "
    "WHERE helpfts MATCH ?1 AND topics.catid = (SELECT id FROM categories "
    "WHERE name = ?2) AND main = 1 ORDER BY name",
    "help.search");

  utf8 = latin1_to_utf8(_term, strlen(_term), &ulen, "string");
  sqlite3_bind_text(searcher, 1, utf8, ulen, free_string);
  sqlite3_bind_text(searcher, 2, h->command, -1, SQLITE_STATIC);

  do {
    status = sqlite3_step(searcher);
    if (status == SQLITE_ROW) {
      char *topic, *snippet;
      int topiclen, snippetlen;
      count += 1;
      if (delim) {
        if (first) {
          first = 0;
        } else {
          safe_str(delim, results, &rp);
        }
        topic = (char *) sqlite3_column_text(searcher, 0);
        topiclen = sqlite3_column_bytes(searcher, 0);
        safe_strl(topic, topiclen, results, &rp);
      } else {
        topic = (char *) sqlite3_column_text(searcher, 0);
        snippet = (char *) sqlite3_column_text(searcher, 1);
        snippetlen = sqlite3_column_bytes(searcher, 1);
        snippet =
          utf8_to_latin1(snippet, snippetlen, NULL, 1, "help.search.results");
        notify_format(executor, "%s%s%s: %s", ANSI_HILITE, topic, ANSI_END,
                      snippet);
        mush_free(snippet, "help.search.results");
      }
    }
  } while (status == SQLITE_ROW || is_busy_status(status));
  sqlite3_reset(searcher);

  if (matches) {
    *matches = count;
  }

  if (delim) {
    *rp = '\0';
    return mush_strdup(results, "help.search.results");
  } else {
    return NULL;
  }
}

static void
help_search_find(dbref player, help_file *h, char *arg_left)
{
  sqlite3_stmt *finder;
  char *pattern;
  int rc, len;
  bool first = 1;
  sqlite3_str *output;
  char *utf8 = latin1_to_utf8(arg_left, -1, NULL, "utf8.string");
  pattern = glob_to_like(utf8, '$', &len);
  mush_free(utf8, "utf8.string");

  finder = prepare_statement(
    help_db,
    "SELECT name FROM topics JOIN entries ON topics.bodyid = entries.id "
    "WHERE catid = (SELECT id FROM categories WHERE name = ?1) AND body LIKE "
    "'%' || ?2 || '%' ESCAPE '$' AND main = 1 ORDER BY name",
    "help.find.wildcard");

  sqlite3_bind_text(finder, 1, h->command, -1, SQLITE_STATIC);
  sqlite3_bind_text(finder, 2, pattern, len, free_string);
  output = sqlite3_str_new(help_db);

  do {
    rc = sqlite3_step(finder);
    if (rc == SQLITE_ROW) {
      const char *name;
      if (first) {
        first = 0;
      } else {
        sqlite3_str_appendall(output, ", ");
      }
      name = (const char *) sqlite3_column_text(finder, 0);
      sqlite3_str_appendall(output, name);
    }
  } while (rc == SQLITE_ROW);
  if (first) {
    notify(player, T("No matches."));
  } else {
    notify_format(player, T("Matches: %s"), sqlite3_str_value(output));
  }

  utf8 = sqlite3_str_finish(output);
  sqlite3_free(utf8);
  sqlite3_reset(finder);
}

COMMAND(cmd_helpcmd)
{
  help_file *h;
  char save[BUFFER_LEN];

  h = hashfind(cmd->name, &help_files);

  if (!h) {
    notify(executor, T("That command is unavailable."));
    return;
  }

  if (h->admin && !Hasprivs(executor)) {
    notify(executor, T("You don't look like an admin to me."));
    return;
  }

  if (SW_ISSET(sw, SWITCH_QUERY)) {
    char *results = NULL;
    char *delim = SW_ISSET(sw, SWITCH_BRIEF) ? ", " : NULL;
    int matches;
    results = help_search(executor, h, arg_left, delim, &matches);
    if (matches == 0) {
      notify(executor, T("No matches."));
    }
    if (results) {
      if (matches > 0) {
        notify_format(executor, "Matches: %s", results);
      }
      mush_free(results, "help.search.results");
    }
    return;
  }

  if (SW_ISSET(sw, SWITCH_SEARCH)) {
    help_search_find(executor, h, arg_left);
    return;
  }

  strcpy(save, arg_left);
  if (wildcard_count(arg_left, 1) == -1) {
    int len = 0;
    int aw = 0;
    char **entries;
    char *p;

    p = arg_left;
    while (*p && (isspace(*p) || *p == '*' || *p == '?')) {
      if (*p == '*')
        aw++;
      p++;
    }
    if (!*p && aw) {
      if ((*arg_left == '*') && *(arg_left + 1) == '\0') {
        notify(executor,
               T("You need to be more specific. Maybe you want 'help \\*'?"));
      } else {
        notify(executor, T("You need to be more specific."));
      }
      return;
    }

    entries = list_matching_entries(arg_left, h, &len);
    if (len == 0) {
      notify_format(executor, T("No entries matching '%s' were found."),
                    arg_left);
    } else if (len == 1) {
      do_new_spitfile(executor, *entries, -1, h);
    } else {
      char buff[BUFFER_LEN];
      char *bp;

      bp = buff;
      arr2list(entries, len, buff, &bp, ", ");
      *bp = '\0';
      notify_format(executor, T("Here are the entries which match '%s':\n%s"),
                    arg_left, buff);
    }
    if (entries) {
      free_entry_list(entries, len);
    }
  } else {
    int offset;
    sqlite3_int64 topicid = -1;
    if (*arg_left == '\0' || help_entry_exists(h, arg_left, &topicid)) {
      do_new_spitfile(executor, *arg_left == '\0' ? "" : NULL, topicid, h);
    } else if (is_index_entry(arg_left, &offset)) {
      char *entries = entries_from_offset(h, offset);
      if (!entries) {
        notify_format(executor, T("No entry for '%s'."), strupper(arg_left));
        return;
      }
      notify_format(executor, "%s%s%s", ANSI_HILITE, strupper(arg_left),
                    ANSI_END);
      if (SUPPORT_PUEBLO) {
        notify_noenter(executor, open_tag("SAMP"));
      }
      notify(executor, entries);
      if (SUPPORT_PUEBLO) {
        notify(executor, close_tag("SAMP"));
      }
      sqlite3_free(entries);
      return;
    } else {
      char pattern[BUFFER_LEN], *pp, *sp;
      char **entries;
      int len = 0;
      int type = 0;
      static const char digits[16] __attribute__((__aligned__(16))) =
        "0123456789";

      pp = pattern;

      for (sp = save; *sp; sp++) {
        if (isspace(*sp)) {
          if (type) {
            type = 0;
            *pp = '*';
            pp++;
            if (pp >= (pattern + BUFFER_LEN)) {
              notify_format(executor, T("No entry for '%s'"), arg_left);
              return;
            }
          }
        } else if (exists_in_ss(digits, 10, *sp)) {
          if (type == 1) {
            type = 2;
            *pp = '*';
            pp++;
            if (pp >= (pattern + BUFFER_LEN)) {
              notify_format(executor, T("No entry for '%s'"), arg_left);
              return;
            }
          }
        } else if (type != 1) {
          type = 1;
        }
        *pp = *sp;
        pp++;
        if (pp >= (pattern + BUFFER_LEN)) {
          notify_format(executor, T("No entry for '%s'"), arg_left);
          return;
        }
      }
      *pp = '\0';
      entries = list_matching_entries(pattern, h, &len);
      if (len == 0) {
        char *suggestion = suggest_name(arg_left, h->command);
        if (suggestion) {
          notify_format(executor, "No %s entry for '%s'. Did you mean '%s'?",
                        h->command, arg_left, suggestion);
          mush_free(suggestion, "string");
        } else {
          notify_format(executor, T("No entry for '%s'"), arg_left);
        }
      } else if (len == 1) {
        do_new_spitfile(executor, *entries, -1, h);
      } else {
        char buff[BUFFER_LEN];
        char *bp;
        bp = buff;
        arr2list(entries, len, buff, &bp, ", ");
        *bp = '\0';
        notify_format(executor, T("Here are the entries which match '%s':\n%s"),
                      arg_left, buff);
      }
      if (entries) {
        free_entry_list(entries, len);
      }
    }
  }
}

#ifdef HAVE_PTHREAD_ATFORK
static bool relaunch = 0;
static void
helpdb_prefork(void)
{
  if (help_db) {
    close_sql_db(help_db);
    help_db = NULL;
    relaunch = 1;
  } else {
    relaunch = 0;
  }
}

static void
helpdb_postfork_parent(void)
{
  if (relaunch) {
    help_db = open_sql_db(options.help_db, 1);
  }
}

#endif

static bool
help_optimize(void *vptr __attribute__((__unused__)))
{
  if (help_db) {
    return optimize_db(help_db);
  } else {
    return false;
  }
}

/** Initialize the helpfile hashtable, which contains the names of thes
 * help files.
 */
void
init_help_files(void)
{
  char *errstr = NULL;
  int status;
  int id = 0, version = 0;

  help_db = open_sql_db(options.help_db, 0);
  if (!help_db) {
    return;
  }
  if (get_sql_db_id(help_db, &id, &version) != 0) {
    do_rawlog(LT_ERR,
              "Unable to read application_id and user_version from help_db");
  }
  if (id != 0 && id != HELPDB_APP_ID) {
    do_rawlog(LT_ERR,
              "Help database used for something else, application id 0x%x.",
              (unsigned) id);
    return;
  }

  if (id == 0 || version != HELPDB_VERSION) {
    do_rawlog(LT_ERR, "Creating help_db tables");
    status = sqlite3_exec(
      help_db,
      "BEGIN TRANSACTION;"
      "DROP TABLE IF EXISTS helpfts;"
      "DROP TABLE IF EXISTS index_starts;"
      "DROP TABLE IF EXISTS topics;"
      "DROP TABLE IF EXISTS entries;"
      "DROP TABLE IF EXISTS files;"
      "DROP TABLE IF EXISTS categories;"
      "DROP TABLE IF EXISTS suggest;"
      "DROP TABLE IF EXISTS suggest_keys;"
      "DROP TABLE IF EXISTS sqlite_stat1;"
      "DROP TABLE IF EXISTS sqlite_stat4;"
      "PRAGMA application_id = 0x42010FF1;"
      "PRAGMA user_version = " HELPDB_VERSIONS ";"
      "CREATE TABLE categories(id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT "
      "NULL UNIQUE);"
      "CREATE TABLE files(id INTEGER NOT NULL PRIMARY KEY, filename TEXT NOT "
      "NULL, modified INTEGER NOT NULL);"
      "CREATE TABLE entries(id INTEGER NOT NULL PRIMARY KEY, body TEXT);"
      "CREATE TABLE topics(catid INTEGER NOT NULL, name TEXT NOT NULL COLLATE "
      "NOCASE, bodyid INTEGER NOT NULL, main INTEGER DEFAULT 0, PRIMARY "
      "KEY(catid, name), FOREIGN KEY(catid) REFERENCES categories(id), FOREIGN "
      "KEY(bodyid) REFERENCES entries(id) ON DELETE CASCADE);"
      "CREATE INDEX topics_idx_bodyid ON topics(bodyid);"
      "CREATE TABLE index_starts(catid INTEGER NOT NULL, pageno INTEGER NOT "
      "NULL, topic TEXT NOT NULL COLLATE NOCASE, PRIMARY KEY(catid, pageno), "
      "FOREIGN KEY(catid, topic) REFERENCES topics(catid, name) ON DELETE "
      "CASCADE) WITHOUT ROWID;"
      "CREATE VIRTUAL TABLE helpfts USING fts5(body, content='entries', "
      "content_rowid='id', tokenize=\"porter unicode61 tokenchars '@+'\");"
      "CREATE TRIGGER entries_ai AFTER INSERT ON entries BEGIN INSERT INTO "
      "helpfts(rowid, body) VALUES (new.id, new.body); END;"
      "CREATE TRIGGER entries_ad AFTER DELETE ON entries BEGIN INSERT INTO "
      "helpfts(helpfts, rowid, body) VALUES ('delete', old.id, old.body); END;"
      "CREATE VIRTUAL TABLE suggest USING spellfix1;"
      "CREATE TABLE suggest_keys(id INTEGER NOT NULL PRIMARY KEY, "
      "cat TEXT NOT NULL UNIQUE);"
      "COMMIT TRANSACTION",
      NULL, NULL, &errstr);
    if (status != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to create help database: %s\n", errstr);
      sqlite3_free(errstr);
      sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      return;
    }
  }
  sq_register_loop(26 * 60 * 60 + 300, help_optimize, NULL, NULL);
  init_private_vocab();
  hashinit(&help_files, 8);
#ifdef HAVE_PTHREAD_ATFORK
  pthread_atfork(helpdb_prefork, helpdb_postfork_parent, NULL);
#endif
  help_init = 1;
}

/** Clean up help files on exit */
void
close_help_files(void)
{
  if (help_db) {
    close_sql_db(help_db);
    help_db = NULL;
  }
}

static bool
build_help_file(help_file *h)
{
  sqlite3 *sqldb = get_shared_db();
  sqlite3_int64 currmodts = 0;
  int status;

  if (needs_rebuild(h, &currmodts)) {
    sqlite3_stmt *add_cat;

    sqlite3_exec(sqldb, "BEGIN TRANSACTION", NULL, NULL, NULL);
    sqlite3_exec(help_db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    delete_private_vocab_cat(h->command);

    add_cat = prepare_statement(
      sqldb,
      "INSERT INTO suggest_keys(cat) VALUES (upper(?)) ON CONFLICT DO NOTHING",
      "suggest.addcat");
    sqlite3_bind_text(add_cat, 1, h->command, -1, SQLITE_STATIC);
    status = sqlite3_step(add_cat);
    if (status != SQLITE_DONE) {
      do_rawlog(LT_ERR, "Unable to add %s to suggestions: %s", h->command,
                sqlite3_errmsg(sqldb));
      sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      return 0;
    }

    if (help_delete_entries(h) && help_populate_entries(h) &&
        help_build_index(h) && update_timestamp(h, currmodts)) {
      sqlite3_exec(help_db,
                   "INSERT INTO helpfts(helpfts) VALUES ('optimize');"
                   "COMMIT TRANSACTION",
                   NULL, NULL, NULL);
      sqlite3_exec(sqldb, "COMMIT TRANSACTION", NULL, NULL, NULL);
      return 1;
    } else {
      do_rawlog(LT_ERR, "Unable to rebuild help database for %s", h->command);
      sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      return 0;
    }
  } else {
    return 0;
  }
}

/** Add new help command. This function is
 * the basis for the help_command directive in mush.cnf. It creates
 * a new help entry for the hash table, builds a help index,
 * and adds the new command to the command table.
 * \param command_name name of help command to add.
 * \param filename name of the help file to use for this command.
 * \param admin if 1, this command reads admin topics, rather than standard.
 */
void
add_help_file(const char *command_name, const char *filename, int admin)
{
  help_file *h;
  sqlite3_stmt *add_cat;
  int status;

  if (help_init == 0)
    init_help_files();

  if (!command_name || !*command_name) {
    do_rawlog(LT_ERR, "Missing help_command name ignored.");
    return;
  }

  if (!filename || !*filename) {
    do_rawlog(LT_ERR, "Missing help_command filename for '%s'.", command_name);
    return;
  }

  /* If there's already an entry for it, complain */
  h = hashfind(strupper(command_name), &help_files);
  if (h) {
    do_rawlog(LT_ERR, "Duplicate help_command %s ignored.", command_name);
    return;
  }

  h = mush_malloc(sizeof *h, "help_file.entry");
  h->command = strupper_a(command_name, "help_file.command");
  h->file = mush_strdup(filename, "help_file.filename");
  h->admin = admin;

  add_cat = prepare_statement_cache(
    help_db, "INSERT INTO categories(name) VALUES (?) ON CONFLICT DO NOTHING",
    "help.add.category", 0);
  sqlite3_bind_text(add_cat, 1, h->command, -1, SQLITE_STATIC);
  sqlite3_step(add_cat);
  sqlite3_finalize(add_cat);

  if (!build_help_file(h)) {
    sqlite3 *sqldb = get_shared_db();
    sqlite3_stmt *topics, *add_suggest;

    sqlite3_exec(sqldb, "BEGIN TRANSACTION", NULL, NULL, NULL);

    topics =
      prepare_statement_cache(help_db,
                              "SELECT name FROM topics WHERE catid = (SELECT "
                              "id FROM categories WHERE name = ?)",
                              "help.add.vocab", 0);
    sqlite3_bind_text(topics, 1, h->command, -1, SQLITE_STATIC);

    add_suggest = prepare_statement(
      sqldb,
      "INSERT INTO suggest_keys(cat) VALUES (upper(?)) ON CONFLICT DO NOTHING",
      "suggest.addcat");
    sqlite3_bind_text(add_suggest, 1, h->command, -1, SQLITE_STATIC);
    sqlite3_step(add_suggest);

    add_suggest =
      prepare_statement_cache(sqldb,
                              "INSERT INTO suggest(word, langid) VALUES "
                              "(lower(?1), (SELECT id FROM suggest_keys "
                              "WHERE cat = upper(?2)))",
                              "help.suggest.insert", 0);
    sqlite3_bind_text(add_suggest, 2, h->command, -1, SQLITE_STATIC);

    do {
      status = sqlite3_step(topics);
      if (status == SQLITE_ROW) {
        const char *word = (const char *) sqlite3_column_text(topics, 0);
        int wlen = sqlite3_column_bytes(topics, 0);

        sqlite3_bind_text(add_suggest, 1, word, wlen, SQLITE_STATIC);
        sqlite3_step(add_suggest);
        sqlite3_reset(add_suggest);
      }
    } while (status == SQLITE_ROW || is_busy_status(status));
    sqlite3_finalize(topics);
    sqlite3_finalize(add_suggest);
    sqlite3_exec(sqldb, "COMMIT TRANSACTION", NULL, NULL, NULL);
  }
  (void) command_add(h->command, CMD_T_ANY | CMD_T_NOPARSE, NULL, 0,
                     "BRIEF QUERY SEARCH", cmd_helpcmd);
  hashadd(h->command, h, &help_files);
}

/** Remove existing entries from a help file.
 */
static bool
help_delete_entries(help_file *h)
{
  sqlite3_stmt *deleter;
  int status;

  deleter = prepare_statement_cache(
    help_db,
    "WITH all_entries(id) AS (SELECT bodyid FROM topics "
    "WHERE catid = (SELECT id FROM categories WHERE name = "
    "?)) DELETE FROM entries WHERE id IN all_entries",
    "help.delete.index", 0);

  sqlite3_bind_text(deleter, 1, h->command, -1, SQLITE_STATIC);
  status = sqlite3_step(deleter);
  sqlite3_finalize(deleter);
  return status == SQLITE_DONE;
}

static bool
needs_rebuild(help_file *h, sqlite3_int64 *pcurrmodts)
{
  sqlite3_stmt *timestamp;
  sqlite3_int64 savedmodts, currmodts;
  struct stat s;
  int status;
  const char *fname;

  if (stat(h->file, &s) < 0) {
    return 1;
  }

  currmodts = s.st_mtime;
  if (pcurrmodts) {
    *pcurrmodts = currmodts;
  }

  timestamp =
    prepare_statement_cache(help_db,
                            "SELECT filename, modified FROM files WHERE id = "
                            "(SELECT id FROM categories WHERE name = ?)",
                            "needs.rebuild.ts", 0);
  sqlite3_bind_text(timestamp, 1, h->command, -1, SQLITE_STATIC);
  status = sqlite3_step(timestamp);
  if (status != SQLITE_ROW) {
    goto cleanup;
  }

  fname = (const char *) sqlite3_column_text(timestamp, 0);
  if (strcmp(fname, h->file) != 0) {
    goto cleanup;
  }

  savedmodts = sqlite3_column_int64(timestamp, 1);
  sqlite3_finalize(timestamp);
  return currmodts != savedmodts;

cleanup:
  sqlite3_finalize(timestamp);
  return 1;
}

static bool
update_timestamp(help_file *h, sqlite3_int64 currmodts)
{
  sqlite3_stmt *updater;
  int status;

  updater = prepare_statement_cache(
    help_db,
    "INSERT INTO files(id, filename, modified) VALUES "
    "((SELECT id FROM categories WHERE name = ?1), ?2, ?3) ON CONFLICT (id) DO "
    "UPDATE SET filename=excluded.filename, modified=excluded.modified",
    "help.update.ts", 0);

  sqlite3_bind_text(updater, 1, h->command, -1, SQLITE_STATIC);
  sqlite3_bind_text(updater, 2, h->file, -1, SQLITE_STATIC);
  sqlite3_bind_int64(updater, 3, currmodts);
  status = sqlite3_step(updater);
  sqlite3_finalize(updater);

  if (status == SQLITE_DONE) {
    return 1;
  } else {
    do_rawlog(LT_ERR, "Unable to update help file timestamp: %s",
              sqlite3_errstr(status));
    return 0;
  }
}

/** Rebuild a help file table.
 * \verbatim
 * This command implements @readcache.
 * \endverbatim
 * \param player the enactor.
 */
void
help_rebuild(dbref player)
{
  help_file *curr;

  for (curr = hash_firstentry(&help_files); curr;
       curr = hash_nextentry(&help_files)) {
    build_help_file(curr);
  }
  if (player != NOTHING) {
    notify(player, T("Help files reindexed."));
    do_rawlog(LT_WIZ, "Help files reindexed by %s(#%d)", Name(player), player);
  } else {
    do_rawlog(LT_WIZ, "Help files reindexed.");
  }
}

/** Rebuild a single help file index. Used in inotify reindexing.
 * \param filename the name of the help file to reindex.
 * \return true if a help file was reindexed, false otherwise.
 */
bool
help_rebuild_by_name(const char *filename)
{
  help_file *curr;
  bool retval = 0;

  for (curr = hash_firstentry(&help_files); curr;
       curr = hash_nextentry(&help_files)) {
    if (strcmp(curr->file, filename) == 0) {
      if (build_help_file(curr)) {
        retval = 1;
      }
    }
  }
  return retval;
}

static void
do_new_spitfile(dbref player, const char *the_topic, sqlite3_int64 topicid,
                help_file *help_dat)
{
  struct help_entry *entry = NULL;
  int default_topic = 0;

  if (the_topic && *the_topic == '\0') {
    default_topic = 1;
    the_topic = help_dat->command;
    topicid = -1;
  }

  entry = help_find_entry(help_dat, the_topic, topicid);
  if (!entry && default_topic) {
    entry = help_find_entry(help_dat, "help", -1);
  }

  if (!entry) {
    notify_format(player, T("No entry for '%s'."), the_topic);
    return;
  }

  /* ANSI topics */
  notify_format(player, "%s%s%s", ANSI_HILITE, entry->name, ANSI_END);

  if (SUPPORT_PUEBLO) {
    notify_noenter(player, open_tag("SAMP"));
  }
  notify_noenter(player, entry->body);
  if (SUPPORT_PUEBLO) {
    notify(player, close_tag("SAMP"));
  }
  help_free_entry(entry);
}

static bool
help_entry_exists(help_file *help_dat, const char *the_topic,
                  sqlite3_int64 *topicid)
{
  char *name;
  sqlite3_stmt *finder;
  int status;
  char *like;

  finder = prepare_statement(
    help_db,
    "SELECT rowid FROM topics WHERE catid = (SELECT id FROM categories WHERE "
    "name = ?1) AND name LIKE ?2 ESCAPE '$' ORDER BY name LIMIT 1",
    "help.entry.exists");

  like = escape_like(the_topic, '$', NULL);
  name = sqlite3_mprintf("%s%%", like);
  free_string(like);

  sqlite3_bind_text(finder, 1, help_dat->command, -1, SQLITE_STATIC);
  sqlite3_bind_text(finder, 2, name, -1, sqlite3_free);
  status = sqlite3_step(finder);
  if (status == SQLITE_ROW && topicid) {
    *topicid = sqlite3_column_int64(finder, 0);
  }
  sqlite3_reset(finder);
  return status == SQLITE_ROW;
}

static struct help_entry *
help_find_entry(help_file *help_dat, const char *the_topic,
                sqlite3_int64 topicid)
{
  char *name;
  sqlite3_stmt *finder;
  int status;

  if (!the_topic && topicid == -1) {
    return NULL;
  }

  if (topicid == -1) {
    char *like;

    finder = prepare_statement(help_db,
                               "SELECT name, body FROM topics JOIN entries ON "
                               "topics.bodyid = entries.id "
                               "WHERE topics.catid = (SELECT id FROM "
                               "categories WHERE name = ?1) AND name "
                               "LIKE ?2 ESCAPE '$' ORDER BY name LIMIT 1",
                               "help.find.entry.by_name");
    like = escape_like(the_topic, '$', NULL);
    name = sqlite3_mprintf("%s%%", like);
    free_string(like);

    sqlite3_bind_text(finder, 1, help_dat->command, -1, SQLITE_STATIC);
    sqlite3_bind_text(finder, 2, name, -1, sqlite3_free);
  } else {
    finder = prepare_statement(help_db,
                               "SELECT name, body FROM topics JOIN entries ON "
                               "topics.bodyid = entries.id "
                               "WHERE topics.rowid = ?",
                               "help.find.entry.by_id");
    sqlite3_bind_int64(finder, 1, topicid);
  }

  status = sqlite3_step(finder);
  if (status == SQLITE_ROW) {
    const char *body;
    int bodylen;
    struct help_entry *entry = mush_malloc(sizeof *entry, "help.entry");
    entry->name = mush_strdup((const char *) sqlite3_column_text(finder, 0),
                              "help.entry.name");
    body = (const char *) sqlite3_column_text(finder, 1);
    bodylen = sqlite3_column_bytes(finder, 1);
    entry->body =
      utf8_to_latin1(body, bodylen, &entry->bodylen, 1, "help.entry.body");
    sqlite3_reset(finder);
    return entry;
  } else {
    sqlite3_reset(finder);
    return NULL;
  }
}

static void
help_free_entry(struct help_entry *h)
{
  mush_free(h->name, "help.entry.name");
  mush_free(h->body, "help.entry.body");
  mush_free(h, "help.entry");
}

static void
write_topic(help_file *h, const char *body)
{
  sqlite3 *sqldb = get_shared_db();
  int64_t entryid = 0;
  sqlite3_stmt *query, *add_suggest;
  int status;

  if (!top) {
    return;
  }

  query = prepare_statement(help_db, "INSERT INTO entries(body) VALUES (?)",
                            "help.insert.body");
  sqlite3_bind_text(query, 1, body, -1, SQLITE_STATIC);
  do {
    status = sqlite3_step(query);
  } while (is_busy_status(status));
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Unable to insert help entry body: %s\n",
              sqlite3_errmsg(help_db));
    sqlite3_reset(query);
    return;
  }
  entryid = sqlite3_last_insert_rowid(help_db);
  sqlite3_reset(query);

  query = prepare_statement(
    help_db,
    "INSERT INTO topics(catid, name, bodyid, main) VALUES "
    "((SELECT id FROM categories WHERE name = ?1), ?2, ?3, ?4)",
    "help.insert.topic");
  sqlite3_bind_text(query, 1, h->command, -1, SQLITE_STATIC);
  sqlite3_bind_int64(query, 3, entryid);

  add_suggest = prepare_statement(sqldb,
                                  "INSERT INTO suggest(word, langid) VALUES "
                                  "(lower(?1), (SELECT id FROM suggest_keys "
                                  "WHERE cat = upper(?2)))",
                                  "help.suggest.insert");
  sqlite3_bind_text(add_suggest, 2, h->command, -1, SQLITE_STATIC);

  for (tlist *cur = top, *nextptr; cur; cur = nextptr) {
    int status;
    int primary = (cur->next == NULL);
    nextptr = cur->next;

    sqlite3_bind_text(query, 2, cur->topic, -1, SQLITE_STATIC);
    sqlite3_bind_int(query, 4, primary);
    status = sqlite3_step(query);
    if (status != SQLITE_DONE) {
      do_rawlog(
        LT_ERR,
        "Unable to insert help topic %s: %s (Possible duplicate entry?)",
        cur->topic, sqlite3_errmsg(help_db));
    } else {
      sqlite3_bind_text(add_suggest, 1, cur->topic, -1, SQLITE_STATIC);
      sqlite3_step(add_suggest);
      sqlite3_reset(add_suggest);
    }
    sqlite3_reset(query);

    free(cur);
  }

  top = NULL;
}

static bool
help_populate_entries(help_file *h)
{
  bool in_topic;
  int i, ntopics;
  char *s, *topic;
  char the_topic[TOPIC_NAME_LEN + 1];
  char line[LINE_SIZE + 1];
  FILE *rfp;
  tlist *cur;
  char *body = NULL;
  int bodylen = 0;
  int num_topics = 0;

  /* Quietly ignore null values for the file */
  if (!h || !h->file) {
    return 0;
  }
  if ((rfp = fopen(h->file, FOPEN_READ)) == NULL) {
    do_rawlog(LT_ERR, "Can't open %s for reading: %s", h->file,
              strerror(errno));
    return 0;
  }

  if (h->admin) {
    do_rawlog(LT_WIZ, "Indexing file %s (admin topics)", h->file);
  } else {
    do_rawlog(LT_WIZ, "Indexing file %s", h->file);
  }
  ntopics = 0;
  in_topic = 0;

#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(fileno(rfp), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  while (fgets(line, LINE_SIZE, rfp) != NULL) {
    if (ntopics == 0) {
      /* Looking for the first topic, but we'll ignore blank lines */
      if (!line[0]) {
        /* Someone's feeding us /dev/null? */
        do_rawlog(LT_ERR, "Malformed help file %s doesn't start with &",
                  h->file);
        fclose(rfp);
        return 0;
      }
      if (isspace(line[0]))
        continue;
      if (line[0] != '&') {
        do_rawlog(LT_ERR, "Malformed help file %s doesn't start with &",
                  h->file);
        fclose(rfp);
        return 0;
      }
    }
    if (line[0] == '&') {
      ++ntopics;
      if (!in_topic) {
        /* Finish up last entry */
        if (ntopics > 1) {
          write_topic(h, body);
          bodylen = 0;
          if (body) {
            mush_free(body, "help.entry.body");
            body = NULL;
          }
        }
        in_topic = true;
      }
      /* parse out the topic */
      /* Get the beginning of the topic string */
      for (topic = &line[1];
           (*topic == ' ' || *topic == '\t') && *topic != '\0'; topic++)
        ;

      /* Get the topic */
      strcpy(the_topic, "");
      for (i = -1, s = topic; *s != '\n' && *s != '\0'; s++) {
        if (i >= TOPIC_NAME_LEN - 1)
          break;
        if (*s != ' ' || the_topic[i] != ' ')
          the_topic[++i] = *s;
      }
      if ((h->admin && the_topic[0] == '&') ||
          (!h->admin && the_topic[0] != '&')) {
        the_topic[++i] = '\0';
        cur = malloc(sizeof(tlist));
        strcpy(cur->topic, the_topic + (the_topic[0] == '&' ? 1 : 0));
        cur->next = top;
        top = cur;
        num_topics += 1;
      }
    } else {
      int oldlen = bodylen;
      in_topic = false;
      bodylen += strlen(line);
      body = mush_realloc(body, bodylen + 1, "help.entry.body");
      strcpy(body + oldlen, line);
    }
  }

  /* Handle last topic */
  if (body) {
    write_topic(h, body);
    mush_free(body, "help.entry.body");
  }
  fclose(rfp);
  do_rawlog(LT_WIZ, "%d topics indexed.", num_topics);

  {
    sqlite3_stmt *clear;
    clear = prepare_statement(help_db, "INSERT INTO entries(body) VALUES (?)",
                              "help.insert.body");
    if (clear) {
      close_statement(clear);
    }
    clear = prepare_statement(
      help_db,
      "INSERT INTO topics(catid, name, bodyid, main) VALUES "
      "((SELECT id FROM categories WHERE name = ?1), ?2, ?3, ?4)",
      "help.insert.topic");
    if (clear) {
      close_statement(clear);
    }
  }

  return 1;
}

/* ARGSUSED */
FUNCTION(fun_textfile)
{
  help_file *h;

  h = hashfind(strupper(args[0]), &help_files);
  if (!h) {
    safe_str(T("#-1 NO SUCH FILE"), buff, bp);
    return;
  }
  if (h->admin && !Hasprivs(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (wildcard_count(args[1], 1) == -1) {
    char **entries;
    int len = 0;
    entries = list_matching_entries(args[1], h, &len);
    if (len == 0) {
      safe_str(T("No matching help topics."), buff, bp);
    } else {
      arr2list(entries, len, buff, bp, ", ");
    }
    if (entries) {
      free_entry_list(entries, len);
    }
  } else {
    safe_str(string_spitfile(h, args[1]), buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_textentries)
{
  help_file *h;
  char **entries;
  int len = 0;
  const char *sep = " ";

  h = hashfind(strupper(args[0]), &help_files);
  if (!h) {
    safe_str(T("#-1 NO SUCH FILE"), buff, bp);
    return;
  }
  if (h->admin && !Hasprivs(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (nargs > 2)
    sep = args[2];

  entries = list_matching_entries(args[1], h, &len);
  if (entries) {
    arr2list(entries, len, buff, bp, sep);
    free_entry_list(entries, len);
  }
}

/* ARGSUSED */
FUNCTION(fun_textsearch)
{
  sqlite3_stmt *finder;
  char *pattern;
  int len;
  int count = 0;
  char *utf8;
  const char *osep = " ";

  help_file *h;
  h = hashfind(strupper(args[0]), &help_files);
  if (!h) {
    safe_str(T("#-1 NO SUCH FILE"), buff, bp);
    return;
  }
  if (h->admin && !Hasprivs(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (nargs > 2)
    osep = args[2];

  utf8 = latin1_to_utf8(args[1], -1, NULL, "utf8.string");
  pattern = glob_to_like(utf8, '$', &len);
  mush_free(utf8, "utf8.string");

  finder = prepare_statement(
    help_db,
    "SELECT name FROM topics JOIN entries ON topics.bodyid = entries.id "
    "WHERE catid = (SELECT id FROM categories WHERE name = ?1) AND body LIKE "
    "'%' || ?2 || '%' ESCAPE '$' AND main = 1 ORDER BY name",
    "help.find.wildcard");

  sqlite3_bind_text(finder, 1, h->command, -1, SQLITE_STATIC);
  sqlite3_bind_text(finder, 2, pattern, len, free_string);

  while (sqlite3_step(finder) == SQLITE_ROW) {
    const char *name;
    name = (const char *) sqlite3_column_text(finder, 0);
    if (count) {
      safe_str(osep, buff, bp);
    }
    count++;
    safe_str(name, buff, bp);
  }

  sqlite3_reset(finder);
}

static const char *
normalize_entry(help_file *help_dat, const char *arg1)
{
  static char the_topic[LINE_SIZE + 2];

  if (*arg1 == '\0')
    arg1 = (char *) "help";
  else if (*arg1 == '&')
    return T("#-1 INVALID ENTRY");
  if (help_dat->admin)
    snprintf(the_topic, LINE_SIZE, "&%s", arg1);
  else
    mush_strncpy(the_topic, arg1, LINE_SIZE);
  return the_topic;
}

static const char *
string_spitfile(help_file *help_dat, char *arg1)
{
  struct help_entry *entry = NULL;
  char the_topic[LINE_SIZE + 2];
  static char buff[BUFFER_LEN];
  char *bp = buff;
  int offset = 0;

  strcpy(the_topic, normalize_entry(help_dat, arg1));

  if (is_index_entry(the_topic, &offset)) {
    char *entries = entries_from_offset(help_dat, offset);

    if (!entries)
      return T("#-1 NO ENTRY");
    else
      return entries;
  }

  entry = help_find_entry(help_dat, the_topic, -1);
  if (!entry) {
    return T("#-1 NO ENTRY");
  }
  safe_strl(entry->body, entry->bodylen, buff, &bp);
  help_free_entry(entry);
  *bp = '\0';
  return buff;
}

static int
get_help_nentries(help_file *h)
{
  int count = 0;
  sqlite3_stmt *total;
  total = prepare_statement(help_db,
                            "SELECT count(*) FROM topics WHERE catid = (SELECT "
                            "id FROM categories WHERE name = ?)",
                            "help.topics.count");
  if (!total) {
    return 0;
  }
  sqlite3_bind_text(total, 1, h->command, -1, SQLITE_STATIC);
  int status = sqlite3_step(total);
  if (status == SQLITE_ROW) {
    count = sqlite3_column_int(total, 0);
  }
  sqlite3_reset(total);
  return count;
}

/** Return a string with all help entries that match a pattern */
static char **
list_matching_entries(const char *pattern, help_file *help_dat, int *len)
{
  char **buff;
  sqlite3_stmt *lister;
  int status;
  char *patcopy = mush_strdup(pattern, "string");
  char *like;
  int likelen;
  int matches = 0;

  if (wildcard_count(patcopy, 1) >= 0) {
    /* Quick way out, use the other kind of matching */
    char the_topic[LINE_SIZE + 2];
    struct help_entry *entry = NULL;
    strcpy(the_topic, normalize_entry(help_dat, patcopy));
    mush_free(patcopy, "string");
    entry = help_find_entry(help_dat, the_topic, -1);
    if (!entry) {
      *len = 0;
      return NULL;
    } else {
      *len = 1;
      buff = mush_calloc(1, sizeof(char *), "help.search");
      buff[0] = mush_strdup(entry->name, "help.entry.name");
      return buff;
    }
  }

  buff =
    mush_calloc(get_help_nentries(help_dat), sizeof(char *), "help.search");

  lister = prepare_statement(
    help_db,
    "SELECT name FROM topics WHERE catid = (SELECT id FROM categories WHERE "
    "name = ?1) AND name LIKE ?2 ESCAPE '$' ORDER BY name",
    "help.list.entries");
  sqlite3_bind_text(lister, 1, help_dat->command, -1, SQLITE_STATIC);
  like = glob_to_like(patcopy, '$', &likelen);
  sqlite3_bind_text(lister, 2, like, likelen, free_string);
  mush_free(patcopy, "string");

  do {
    status = sqlite3_step(lister);
    if (status == SQLITE_ROW) {
      buff[matches++] = mush_strdup(
        (const char *) sqlite3_column_text(lister, 0), "help.entry.name");
    }
  } while (status == SQLITE_ROW || is_busy_status(status));
  sqlite3_reset(lister);
  *len = matches;
  return buff;
}

static void
free_entry_list(char **entries, int len)
{
  for (int n = 0; n < len; n += 1) {
    mush_free(entries[n], "help.entry.name");
  }
  mush_free(entries, "help.search");
}

enum { ENTRIES_PER_PAGE = 48, LONG_TOPIC = 25 };

static bool
help_build_index(help_file *h)
{
  sqlite3_stmt *lister, *adder;
  int status;
  int page = 1;
  int count = ENTRIES_PER_PAGE - 1;

  lister = prepare_statement_cache(
    help_db,
    "SELECT name FROM topics WHERE catid = (SELECT id FROM "
    "categories WHERE name = ?) ORDER BY name",
    "help.entries.build.index", 0);
  if (!lister) {
    return 0;
  }
  adder = prepare_statement_cache(
    help_db,
    "INSERT INTO index_starts(catid, pageno, topic) VALUES ((SELECT id FROM "
    "categories WHERE name = ?1),?2,?3)",
    "help.entries.insert", 0);
  if (!adder) {
    sqlite3_finalize(lister);
    return 0;
  }

  sqlite3_bind_text(lister, 1, h->command, -1, SQLITE_STATIC);
  sqlite3_bind_text(adder, 1, h->command, -1, SQLITE_STATIC);

  do {
    status = sqlite3_step(lister);
    if (status == SQLITE_ROW) {
      if (++count == ENTRIES_PER_PAGE) {
        const char *t = (const char *) sqlite3_column_text(lister, 0);
        sqlite3_bind_int(adder, 2, page);
        sqlite3_bind_text(adder, 3, t, -1, SQLITE_TRANSIENT);
        status = sqlite3_step(adder);
        if (status != SQLITE_DONE) {
          do_rawlog(LT_ERR, "While building entries database for %s: %s",
                    h->command, sqlite3_errmsg(help_db));
          break;
        }
        sqlite3_reset(adder);
        page += 1;
        count = 0;
      }
      status = SQLITE_ROW;
    }
  } while (status == SQLITE_ROW);
  sqlite3_finalize(lister);
  sqlite3_finalize(adder);
  return status == SQLITE_DONE;
}

/* Generate a page of the index of the help file (The old pre-generated 'help
 * entries' tables), 1-indexed.
 */
static char *
entries_from_offset(help_file *h, int off)
{

  sqlite3_stmt *indexer;
  sqlite3_str *res;
  int fmtwidths[3];
  int col = 0, pages = 0, status;
  int ncols = 3, colspace = 0;

  indexer = prepare_statement(help_db,
                              "SELECT count(*) FROM index_starts WHERE catid = "
                              "(SELECT id FROM categories WHERE name = ?)",
                              "help.entries.count");
  sqlite3_bind_text(indexer, 1, h->command, -1, SQLITE_STATIC);

  status = sqlite3_step(indexer);
  if (status == SQLITE_ROW) {
    pages = sqlite3_column_int(indexer, 0);
  }
  sqlite3_reset(indexer);

  if (pages == 0 || off > pages) {
    return NULL;
  }

  /* Window functions would be useful here */
  indexer = prepare_statement(
    help_db,
    "WITH cat(id) AS (SELECT id FROM categories WHERE name = ?1) "
    "SELECT t.name"
    "     , lead(length(t.name), 1, 0) OVER (ORDER BY t.name)"
    "     , lead(length(t.name), 2, 0) OVER (ORDER BY t.name)"
    "FROM topics AS t "
    "JOIN cat ON t.catid = cat.id "
    "JOIN index_starts AS i ON cat.id = i.catid "
    "WHERE t.name >= i.topic AND i.pageno = ?2 "
    "ORDER BY t.name "
    "LIMIT ?3",
    "help.entries.page");

  if (!indexer) {
    return NULL;
  }

  sqlite3_bind_text(indexer, 1, h->command, -1, SQLITE_STATIC);
  sqlite3_bind_int(indexer, 2, off);
  sqlite3_bind_int(indexer, 3, ENTRIES_PER_PAGE);

  res = sqlite3_str_new(help_db);

  while (1) {
    const char *entry;

    status = sqlite3_step(indexer);
    if (status != SQLITE_ROW) {
      break;
    }

    entry = (const char *) sqlite3_column_text(indexer, 0);

    if (col == 0) {
      int len0, len1, len2;
      len0 = sqlite3_column_bytes(indexer, 0);
      len1 = sqlite3_column_int(indexer, 1);
      len2 = sqlite3_column_int(indexer, 2);
      colspace = 0;
      if (len0 > LONG_TOPIC) {
        if (len1 > LONG_TOPIC) {
          fmtwidths[0] = 75;
          ncols = 1;
        } else {
          fmtwidths[0] = 50;
          fmtwidths[1] = 25;
          ncols = 2;
          colspace = 1;
        }
      } else if (len1 > LONG_TOPIC) {
        fmtwidths[0] = 25;
        fmtwidths[1] = 50;
        ncols = 2;
      } else if (len2 > LONG_TOPIC) {
        fmtwidths[0] = 25;
        fmtwidths[1] = 25;
        ncols = 2;
      } else {
        fmtwidths[0] = 25;
        fmtwidths[1] = 25;
        fmtwidths[2] = 25;
        ncols = 3;
      }
    }
    sqlite3_str_appendf(res, " %-*.*s", fmtwidths[col], fmtwidths[col], entry);
    sqlite3_str_appendchar(res, colspace, ' ');
    col += 1;
    if (col == ncols) {
      sqlite3_str_appendchar(res, 1, '\n');
      col = 0;
    }
  }
  sqlite3_reset(indexer);

  /* There are 'pages' pages in total */
  if (off < pages) {
    if (pages == (off + 1)) {
      sqlite3_str_appendf(res, "\nFor more, see ENTRIES-%d", pages);
    } else if (pages > (off + 1)) {
      sqlite3_str_appendf(res, "\nFor more, see ENTRIES-%d through %d", off + 1,
                          pages);
    }
  }

  return sqlite3_str_finish(res);
}

extern const unsigned char *tables;

static bool
is_index_entry(const char *topic, int *offset)
{
  static pcre2_code *entry_re = NULL;
  static pcre2_match_data *entry_md = NULL;
  int r;

  if (strcasecmp(topic, "entries") == 0 || strcasecmp(topic, "&entries") == 0) {
    *offset = 1;
    return 1;
  }

  if (!entry_re) {
    int errcode;
    PCRE2_SIZE erroffset;
    entry_re = pcre2_compile(
      (const PCRE2_UCHAR *) "^&?entries-([0-9]+)$", PCRE2_ZERO_TERMINATED,
      re_compile_flags | PCRE2_CASELESS | PCRE2_NO_UTF_CHECK, &errcode,
      &erroffset, re_compile_ctx);
    pcre2_jit_compile(entry_re, PCRE2_JIT_COMPLETE);
    entry_md = pcre2_match_data_create_from_pattern(entry_re, NULL);
  }

  if ((r = pcre2_match(entry_re, (const PCRE2_UCHAR *) topic, strlen(topic), 0,
                       re_match_flags, entry_md, re_match_ctx)) == 2) {
    char buff[BUFFER_LEN];
    PCRE2_SIZE bufflen = BUFFER_LEN;
    pcre2_substring_copy_bynumber(entry_md, 1, (PCRE2_UCHAR *) buff, &bufflen);
    *offset = parse_integer(buff);
    if (*offset <= 0) {
      *offset = 1;
    }
    return 1;
  } else {
    return 0;
  }
}

#define MAX_SUGGESTIONS 500000

/** Add a word to the vocabulary list for a given category.
 *
 * \param name The word to add, in UTF-8.
 * \param category The category of the word, in UTF-8.
 */
bool
add_vocab(const char *name, const char *category)
{
  sqlite3_stmt *inserter;
  int status;

  inserter = prepare_statement(
    help_db,
    "INSERT INTO suggest_keys(cat) VALUES (upper(?)) ON CONFLICT DO NOTHING",
    "suggest.user.addcat");
  if (inserter) {
    int status;
    sqlite3_bind_text(inserter, 1, category, -1, SQLITE_STATIC);
    do {
      status = sqlite3_step(inserter);
    } while (is_busy_status(status));
    sqlite3_reset(inserter);
  } else {
    return 0;
  }

  inserter = prepare_statement(
    help_db,
    "SELECT count(*) FROM suggest_vocab WHERE word = lower(?1) AND langid = "
    "(SELECT id FROM suggest_keys WHERE cat = upper(?2))",
    "suggest.user.duplicate");
  if (!inserter) {
    return 0;
  }
  sqlite3_bind_text(inserter, 1, name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(inserter, 2, category, -1, SQLITE_TRANSIENT);
  status = sqlite3_step(inserter);
  if (status == SQLITE_ROW) {
    int c = sqlite3_column_int(inserter, 0);
    sqlite3_reset(inserter);
    if (c > 0) {
      return 0;
    }
  } else {
    sqlite3_reset(inserter);
    return 0;
  }

  inserter = prepare_statement(help_db, "SELECT count(*) FROM suggest_vocab",
                               "suggest.user.count");
  if (!inserter) {
    return 0;
  }
  status = sqlite3_step(inserter);
  if (status == SQLITE_ROW) {
    int c = sqlite3_column_int(inserter, 0);
    sqlite3_reset(inserter);
    if (c > MAX_SUGGESTIONS) {
      return 0;
    }
  } else {
    sqlite3_reset(inserter);
    return 0;
  }

  inserter =
    prepare_statement(help_db,
                      "INSERT INTO suggest(word, langid) SELECT lower(?1), id "
                      "FROM suggest_keys WHERE cat = upper(?2)",
                      "suggest.user.insert");
  if (inserter) {
    sqlite3_bind_text(inserter, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(inserter, 2, category, -1, SQLITE_TRANSIENT);
    status = sqlite3_step(inserter);
    sqlite3_reset(inserter);
    return status == SQLITE_DONE;
  }

  return 0;
}

/* Delete a word from the given category's vocabulary list.
 *
 * \param name The word to delete, in UTF-8.
 * \param category The category of the word, in UTF-8.
 */
bool
delete_vocab(const char *name, const char *category)
{
  sqlite3_stmt *deleter;

  deleter =
    prepare_statement(help_db,
                      "DELETE FROM suggest WHERE word = lower(?1) AND langid = "
                      "(SELECT id FROM suggest_keys WHERE cat = upper(?2))",
                      "suggest.user.delete");
  if (deleter) {
    int status;
    sqlite3_bind_text(deleter, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(deleter, 2, category, -1, SQLITE_STATIC);
    status = sqlite3_step(deleter);
    sqlite3_reset(deleter);
    return status == SQLITE_DONE;
  }
  return 0;
}

void
add_dict_words(void)
{
  int r;
  FILE *words;
  char line[BUFFER_LEN];
  char *errmsg;
  sqlite3_stmt *adder, *timestamp;
  struct stat s;
  sqlite3_int64 savedmodts = 0, currmodts;

  if (options.dict_file[0] == '\0') {
    return;
  }

  if (stat(options.dict_file, &s) < 0) {
    do_rawlog(LT_ERR, "Unable to stat word list %s: %s", options.dict_file,
              strerror(errno));
    return;
  }
  currmodts = s.st_mtime;

  timestamp = prepare_statement_cache(
    help_db, "SELECT modified FROM files WHERE filename = ?",
    "words.needs.rebuild", 0);
  if (!timestamp) {
    return;
  }

  sqlite3_bind_text(timestamp, 1, options.dict_file, -1, SQLITE_STATIC);
  r = sqlite3_step(timestamp);
  if (r == SQLITE_ROW) {
    savedmodts = sqlite3_column_int64(timestamp, 0);
  }
  sqlite3_finalize(timestamp);

  if (r == SQLITE_ROW && currmodts == savedmodts) {
    do_rawlog(LT_ERR, "Using cached copy of dict_file words.");
    return;
  }

  r = sqlite3_exec(
    help_db,
    "BEGIN TRANSACTION;"
    "INSERT INTO suggest_keys(cat) VALUES ('WORDS') ON CONFLICT DO NOTHING;"
    "DELETE FROM suggest WHERE langid = (SELECT id FROM suggest_keys WHERE "
    "cat "
    "= 'WORDS');"
    "CREATE TEMP TABLE wordslist(word TEXT NOT NULL PRIMARY KEY, id);",
    NULL, NULL, &errmsg);
  if (r != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to populate words suggestions: %s\n", errmsg);
    sqlite3_free(errmsg);
    return;
  }

  if (savedmodts == 0) {
    timestamp = prepare_statement_cache(
      help_db, "INSERT INTO files(modified, filename) VALUES (?1, ?2)",
      "update.words.timestamp", 0);
  } else {
    timestamp = prepare_statement_cache(
      help_db, "UPDATE files SET modified = ?1 WHERE filename = ?2",
      "update.words.timestamp", 0);
  }
  if (!timestamp) {
    sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    return;
  }
  sqlite3_bind_int64(timestamp, 1, currmodts);
  sqlite3_bind_text(timestamp, 2, options.dict_file, -1, SQLITE_STATIC);
  sqlite3_step(timestamp);
  sqlite3_finalize(timestamp);

  adder = prepare_statement_cache(
    help_db,
    "INSERT INTO wordslist(word,id) "
    "VALUES (lower(?), (SELECT id FROM "
    "suggest_keys WHERE cat = 'WORDS')) ON CONFLICT DO NOTHING",
    "suggest.init.words", 0);
  if (!adder) {
    sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    return;
  }

  words = fopen(options.dict_file, "r");
  if (!words) {
    do_rawlog(LT_ERR, "Unable to open words file %s\n", options.dict_file);
    sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    sqlite3_finalize(adder);
    return;
  }

  do_rawlog(LT_ERR, "Reading word list from %s", options.dict_file);

  while (fgets(line, BUFFER_LEN, words)) {
    char *nl = strchr(line, '\n');
    if (nl) {
      *nl = '\0';
      sqlite3_bind_text(adder, 1, line, -1, SQLITE_TRANSIENT);
      sqlite3_step(adder);
      sqlite3_reset(adder);
    } else {
      /* Really long line in a words file? Ignore it. */
      while (fgets(line, BUFFER_LEN, words)) {
        if (strchr(line, '\n')) {
          break;
        }
      }
    }
  }
  fclose(words);
  sqlite3_finalize(adder);
  r = sqlite3_exec(
    help_db,
    "INSERT INTO suggest(word, langid) SELECT word, id FROM wordslist;"
    "DROP TABLE wordslist;"
    "COMMIT TRANSACTION",
    NULL, NULL, &errmsg);
  if (r != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to populate word suggestions: %s", errmsg);
    sqlite3_free(errmsg);
    sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    return;
  }
  do_rawlog(LT_ERR, "Done reading words.");
}

FUNCTION(fun_suggest)
{
  const char *sep = " ";
  sqlite3_stmt *words;
  char *cat8, *word8;
  int catlen, wordlen;
  int status;
  bool first = 1;
  int top = 20;

  if (nargs >= 3) {
    sep = args[2];
  }

  if (nargs == 4) {
    if (!is_integer(args[3])) {
      safe_str(T(e_int), buff, bp);
      return;
    }
    top = parse_integer(args[3]);
  }

  cat8 = latin1_to_utf8(args[0], arglens[0], &catlen, "string");
  word8 = latin1_to_utf8(args[1], arglens[1], &wordlen, "string");

  words = prepare_statement(
    help_db,
    "SELECT upper(word) FROM suggest WHERE word MATCH ?1 AND langid = (SELECT "
    "id FROM suggest_keys WHERE cat = upper(?2)) AND top=?3",
    "suggest.find.all");

  sqlite3_bind_text(words, 1, word8, wordlen, free_string);
  sqlite3_bind_text(words, 2, cat8, catlen, free_string);
  sqlite3_bind_int(words, 3, top);

  do {
    status = sqlite3_step(words);
    if (status == SQLITE_ROW) {
      const char *word = (const char *) sqlite3_column_text(words, 0);
      wordlen = sqlite3_column_bytes(words, 0);
      int word1len;
      char *word1 = utf8_to_latin1_us(word, wordlen, &word1len, 0, "string");
      if (first) {
        first = 0;
      } else {
        safe_str(sep, buff, bp);
      }
      safe_strl(word1, word1len, buff, bp);
      mush_free(word1, "string");
    }
  } while (status == SQLITE_ROW || is_busy_status(status));
  sqlite3_reset(words);
}

COMMAND(cmd_suggest)
{
  char *cat8, *word8;

  if (SW_ISSET(sw, SWITCH_ADD)) {
    if (!Wizard(executor)) {
      notify(executor, "Your suggestion is not welcome.");
    } else if (*arg_left && *arg_right) {
      cat8 = latin1_to_utf8(arg_left, strlen(arg_left), NULL, "string");
      word8 = latin1_to_utf8(arg_right, strlen(arg_right), NULL, "string");
      if (add_vocab(word8, cat8)) {
        notify(executor, "Suggestion vocabulary word added.");
      } else {
        notify(executor, "Unable to add word.");
      }
      mush_free(cat8, "string");
      mush_free(word8, "string");
    } else {
      notify(executor, "What did you want to add?");
    }
  } else if (SW_ISSET(sw, SWITCH_DELETE)) {
    if (!Wizard(executor)) {
      notify(executor, "Permission denied.");
    } else if (*arg_left && *arg_right) {
      cat8 = latin1_to_utf8(arg_left, strlen(arg_left), NULL, "string");
      word8 = latin1_to_utf8(arg_right, strlen(arg_right), NULL, "string");
      if (delete_vocab(word8, cat8)) {
        notify(executor, "Suggestion vocabulary word deleted.");
      } else {
        notify(executor, "Unable to delete word.");
      }
      mush_free(cat8, "string");
      mush_free(word8, "string");
    } else {
      notify(executor, "What did you want to delete?");
    }
  } else {
    sqlite3_stmt *cats;
    int status;
    int count = 0;

    cats = prepare_statement(
      help_db, "SELECT cat FROM suggest_keys ORDER BY cat", "suggest.list");
    notify(executor, "Vocabulary suggestion categories:");
    do {
      status = sqlite3_step(cats);
      if (status == SQLITE_ROW) {
        const char *name = (const char *) sqlite3_column_text(cats, 0);
        int nlen = sqlite3_column_bytes(cats, 0);
        char *cat1 = utf8_to_latin1_us(name, nlen, NULL, 0, "string");
        count += 1;
        notify_format(executor, "\t%s", cat1);
        mush_free(cat1, "string");
      }
    } while (status == SQLITE_ROW || is_busy_status(status));
    sqlite3_reset(cats);
    if (count == 0) {
      notify(executor, "None found.");
    }
  }
}
