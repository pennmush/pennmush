/**
 * \file help.c
 *
 * \brief The PennMUSH help system.
 *
 *
 */
#include "help.h"

/* This might have to be uncommented on some linux distros... */
/* #define _XOPEN_SOURCE 600 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pcre.h>
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

#define LINE_SIZE 90
#define TOPIC_NAME_LEN 30

struct help_entry {
  char *name;
  char *body;
  int bodylen;
};

HASHTAB help_files; /**< Help filenames hash table */

sqlite3 *help_db = NULL;

static int help_init = 0;

static void do_new_spitfile(dbref, const char *, help_file *);
static const char *string_spitfile(help_file *help_dat, char *arg1);

static bool help_entry_exists(help_file *, const char *);
static struct help_entry *help_find_entry(help_file *help_dat,
                                          const char *the_topic);
static void help_free_entry(struct help_entry *);
static char **list_matching_entries(const char *pattern, help_file *help_dat,
                                    int *len);
static void free_entry_list(char **, int len);

static const char *normalize_entry(help_file *help_dat, const char *arg1);

static void help_delete_entries(help_file *h);
static void help_populate_entries(help_file *h);

static bool is_index_entry(const char *, int *);
static char *entries_from_offset(help_file *, int);

/** Linked list of help topic names. */
typedef struct TLIST {
  char topic[TOPIC_NAME_LEN + 1]; /**< Name of topic */
  struct TLIST *next;             /**< Pointer to next list entry */
} tlist;

tlist *top = NULL; /**< Pointer to top of linked list of topic names */

extern bool help_wild(const char *restrict tstr, const char *restrict dstr);

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
  
  searcher = prepare_statement(help_db,
                               "SELECT name, snippet(helpfts, 0, '" ANSI_UNDERSCORE "', '" ANSI_END "', '...', 10) FROM helpfts JOIN topics ON topics.bodyid = helpfts.rowid WHERE helpfts MATCH ? AND topics.catid = (SELECT id FROM categories WHERE name = ?) AND main = 1 ORDER BY name",
                               "help.search");

  utf8 = latin1_to_utf8(_term, strlen(_term), &ulen, "string");
  sqlite3_bind_text(searcher, 1, utf8, ulen, free_string);
  sqlite3_bind_text(searcher, 2, h->command, strlen(h->command), SQLITE_TRANSIENT);

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
        topic = (char *)sqlite3_column_text(searcher, 0);
        topiclen = sqlite3_column_bytes(searcher, 0);
        safe_strl(topic, topiclen, results, &rp);
      } else {
        topic = (char *)sqlite3_column_text(searcher, 0);
        snippet = (char *)sqlite3_column_text(searcher, 1);
        snippet = utf8_to_latin1(snippet, &snippetlen, "help.search.results");
        notify_format(executor, "%s%s%s: %s", ANSI_HILITE, topic, ANSI_END, snippet);
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

  if (SW_ISSET(sw, SWITCH_SEARCH)) {
    int matches;
    help_search(executor, h, arg_left, NULL, &matches);
    if (matches == 0) {
      notify(executor, T("No matches."));
    }
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
      do_new_spitfile(executor, *entries, h);
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
    if (help_entry_exists(h, arg_left)) {
      do_new_spitfile(executor, arg_left, h);
    } else if (is_index_entry(arg_left, &offset)) {
      char *entries = entries_from_offset(h, offset);
      if (!entries) {
          notify_format(executor, T("No entry for '%s'."),
                        strupper(arg_left));
        return;
      }
      notify_format(executor, "%s%s%s", ANSI_HILITE, strupper(arg_left),
                    ANSI_END);
      if (SUPPORT_PUEBLO)
        notify_noenter(executor, open_tag("SAMP"));
      notify(executor, entries);
      if (SUPPORT_PUEBLO)
        notify(executor, close_tag("SAMP"));
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
        do_new_spitfile(executor, *entries, h);
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

/** Initialize the helpfile hashtable, which contains the names of thes
 * help files.
 */
void
init_help_files(void)
{
  char *errstr = NULL;
  int r;

  help_db = open_sql_db(options.help_db, 0);
  if (!help_db) {
    return;
  }
  r = sqlite3_exec(help_db,
                   "DROP TABLE IF EXISTS helpfts;"
                   "DROP TABLE IF EXISTS topics;"
                   "DROP TABLE IF EXISTS entries;"
                   "DROP TABLE IF EXISTS categories;"
                   "CREATE TABLE categories(id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL UNIQUE);"
                   "CREATE TABLE entries(id INTEGER NOT NULL PRIMARY KEY, body TEXT);"
                   "CREATE TABLE topics(catid INTEGER NOT NULL, name TEXT NOT NULL COLLATE NOCASE, bodyid INTEGER NOT NULL, main INTEGER DEFAULT 0, PRIMARY KEY(catid, name), FOREIGN KEY(catid) REFERENCES categories(id), FOREIGN KEY(bodyid) REFERENCES entries(id) ON DELETE CASCADE);"
                   "CREATE INDEX topics_body_idx ON topics(bodyid);"
                   "CREATE VIRTUAL TABLE helpfts USING fts5(body, content=entries, content_rowid=id);"
                   "CREATE TRIGGER entries_ai AFTER INSERT ON entries BEGIN INSERT INTO helpfts(rowid, body) VALUES (new.id, new.body); END;"
                   "CREATE TRIGGER entries_ad AFTER DELETE ON entries BEGIN INSERT INTO helpfts(helpfts, rowid, body) VALUES ('delete', old.id, old.body); END",
                   NULL, NULL, &errstr);
  if (r != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to create help database: %s\n", errstr);
    sqlite3_free(errstr);
    return;
  }
  init_vocab();
  hashinit(&help_files, 8);
  help_init = 1;
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
  h->command = mush_strdup(strupper(command_name), "help_file.command");
  h->file = mush_strdup(filename, "help_file.filename");
  h->admin = admin;

  add_cat = prepare_statement(help_db,
                              "INSERT OR IGNORE INTO categories(name) VALUES (?)",
                              "help.add.category");
  sqlite3_bind_text(add_cat, 1, h->command, strlen(h->command),
                    SQLITE_TRANSIENT);
  do {
    status = sqlite3_step(add_cat);
  } while (is_busy_status(status));
  sqlite3_reset(add_cat);
  help_populate_entries(h);
  (void) command_add(h->command, CMD_T_ANY | CMD_T_NOPARSE, NULL, 0, "SEARCH",
                     cmd_helpcmd);
  hashadd(h->command, h, &help_files);
}

/** Remove existing entries from a help file.
 */
static void
help_delete_entries(help_file *h)
{
  sqlite3_stmt *deleter;
  int status;

  deleter = prepare_statement(help_db,
                              "WITH all_entries(id) AS (SELECT bodyid FROM topics WHERE catid = (SELECT id FROM categories WHERE name = ?)) DELETE FROM entries WHERE id IN all_entries",
                              "help.delete.index");

  sqlite3_bind_text(deleter, 1, h->command, strlen(h->command),
                    SQLITE_TRANSIENT);
  do {
    status = sqlite3_step(deleter);
  } while (is_busy_status(status));
  sqlite3_reset(deleter);
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
    delete_vocab_cat(curr->command);
    help_delete_entries(curr);
    help_populate_entries(curr);
  }
  sqlite3_exec(help_db, "INSERT INTO helpfts(helpfts) VALUES ('optimize');"
               "VACUUM",
               NULL, NULL, NULL);
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
      delete_vocab_cat(curr->command);
      help_delete_entries(curr);
      help_populate_entries(curr);
      retval = 1;
    }
  }
  return retval;
}

static void
do_new_spitfile(dbref player, const char *the_topic, help_file *help_dat)
{
  struct help_entry *entry = NULL;
  int default_topic = 0;

  if (*the_topic == '\0') {
    default_topic = 1;
    the_topic = help_dat->command;
  }

  entry = help_find_entry(help_dat, the_topic);
  if (!entry && default_topic) {
    entry = help_find_entry(help_dat, "help");
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
help_entry_exists(help_file *help_dat, const char *the_topic)
{
  char name[BUFFER_LEN];
  sqlite3_stmt *finder;
  int status;
  char *like;
  int namelen;
  
  finder = prepare_statement(help_db,
                             "SELECT rowid FROM topics WHERE catid = (SELECT id FROM categories WHERE name = ?) AND name LIKE ? ESCAPE '$' ORDER BY name",
                             "help.entry.exists");

  like = escape_like(the_topic, '$', NULL);
  namelen = snprintf(name, sizeof name, "%s%%", like);
  free_string(like);

  sqlite3_bind_text(finder, 1, help_dat->command, strlen(help_dat->command),
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(finder, 2, name, namelen, SQLITE_TRANSIENT);

  do {
    status = sqlite3_step(finder);
    if (status == SQLITE_ROW) {
      sqlite3_reset(finder);
      return 1;
    }
  } while (is_busy_status(status));
  sqlite3_reset(finder);
  return 0;
}

static struct help_entry *
help_find_entry(help_file *help_dat, const char *the_topic)
{
  char name[BUFFER_LEN];
  sqlite3_stmt *finder;
  int status;
  char *like;
  int namelen;
  
  finder = prepare_statement(help_db,
                             "SELECT name, body FROM topics JOIN entries ON topics.bodyid = entries.id WHERE topics.catid = (SELECT id FROM categories WHERE name = ?) AND name LIKE ? ESCAPE '$' ORDER BY name",
                             "help.find.entry");

  like = escape_like(the_topic, '$', NULL);
  namelen = snprintf(name, sizeof name, "%s%%", like);
  free_string(like);
  
  sqlite3_bind_text(finder, 1, help_dat->command, strlen(help_dat->command),
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(finder, 2, name, namelen, SQLITE_TRANSIENT);

  do {
    status = sqlite3_step(finder);
    if (status == SQLITE_ROW) {
      struct help_entry *entry = mush_malloc(sizeof *entry, "help.entry");
      entry->name = mush_strdup((const char *)sqlite3_column_text(finder, 0),
                                "help.entry.name");
      entry->body = utf8_to_latin1((const char *)sqlite3_column_text(finder, 1),
                                   &entry->bodylen, "help.entry.body");
      sqlite3_reset(finder);
      return entry;
    }
  } while (is_busy_status(status));
  sqlite3_reset(finder);
  return NULL;
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
  int64_t entryid = 0;
  sqlite3_stmt *query;
  int status;

  if (!top) {
    return;
  }
  
  query = prepare_statement(help_db, "INSERT INTO entries(body) VALUES (?)",
                                  "help.insert.body");
  sqlite3_bind_text(query, 1, body, strlen(body), SQLITE_TRANSIENT);
  do {
    status = sqlite3_step(query);
  } while (is_busy_status(status));
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Unable to insert help entry body: %s\n",
              sqlite3_errstr(status));
    sqlite3_reset(query);
    return;
  }
  entryid = sqlite3_last_insert_rowid(help_db);
  sqlite3_reset(query);

  query = prepare_statement(help_db,
                            "INSERT INTO topics(catid, name, bodyid, main) VALUES ((SELECT id FROM categories WHERE name = ?), ?, ?, ?)",
                            "help.insert.topic");
  sqlite3_bind_text(query, 1, h->command, strlen(h->command),
                    SQLITE_TRANSIENT);
  sqlite3_bind_int64(query, 3, entryid);
  
  for (tlist *cur = top, *nextptr; cur; cur = nextptr) {
    int status;
    int primary = (cur->next == NULL);
    nextptr = cur->next;
    add_vocab(cur->topic, h->command);
    sqlite3_bind_text(query, 2, cur->topic, strlen(cur->topic),
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(query, 4, primary);
    do {
      status = sqlite3_step(query);
    } while (is_busy_status(status));
    if (status != SQLITE_DONE) {
      do_rawlog(LT_ERR, "Unable to insert help topic %s: %s",
                cur->topic, sqlite3_errstr(status));
    }
    sqlite3_reset(query);   
    free(cur);
  }
  top = NULL;
}

static void
help_populate_entries(help_file *h)
{
  bool in_topic;
  int i, ntopics;
  char *s, *topic;
  char the_topic[TOPIC_NAME_LEN + 1];
  char line[LINE_SIZE + 1];
  FILE *rfp;
  tlist *cur;
  sqlite3 *sqldb;
  char *errmsg;
  char *body = NULL;
  int bodylen = 0;
  int num_topics = 0;
  
  /* Quietly ignore null values for the file */
  if (!h || !h->file)
    return;
  if ((rfp = fopen(h->file, FOPEN_READ)) == NULL) {
    do_rawlog(LT_ERR, "Can't open %s for reading: %s", h->file,
              strerror(errno));
    return;
  }

  if (h->admin) {
    do_rawlog(LT_WIZ, "Indexing file %s (admin topics)", h->file);
  } else {
    do_rawlog(LT_WIZ, "Indexing file %s", h->file);
  }
  ntopics = 0;
  in_topic = 0;

  sqldb = get_shared_db();
  if (sqlite3_exec(sqldb, "BEGIN TRANSACTION", NULL, NULL, &errmsg)
      != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to begin help transaction: %s", errmsg);
    sqlite3_free(errmsg);
    fclose(rfp);
    return;
  }

  if (sqlite3_exec(help_db, "BEGIN TRANSACTION", NULL, NULL, &errmsg)
      != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to begin help transaction: %s", errmsg);
    sqlite3_free(errmsg);
    fclose(rfp);
    sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
    return;
  }
  
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
        sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return;
      }
      if (isspace(line[0]))
        continue;
      if (line[0] != '&') {
        do_rawlog(LT_ERR, "Malformed help file %s doesn't start with &",
                  h->file);
        fclose(rfp);
        sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return;
      }
    }
    if (line[0] == '&') {
      ++ntopics;
      if (!in_topic) {
        /* Finish up last entry */
        if (ntopics > 1) {
          write_topic(h, body);
        }
        bodylen = 0;
        mush_free(body, "help.entry.body");
        body = NULL;
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
        cur = (tlist *) malloc(sizeof(tlist));
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
  write_topic(h, body);
  mush_free(body, "help.entry.body");
  fclose(rfp);
  do_rawlog(LT_WIZ, "%d topics indexed.", num_topics);
  h->entries = num_topics;
  
  while (1) {
    int status = sqlite3_exec(sqldb, "COMMIT TRANSACTION", NULL, NULL, &errmsg);
    if (status == SQLITE_OK) {
      break;
    } else if (is_busy_status(status)) {
      sqlite3_free(errmsg);
    } else {
      do_rawlog(LT_ERR, "Unable to commit help vocab transaction: %s",
                errmsg);
      sqlite3_free(errmsg);
      sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      break;
    }
  }
  while (1) {
    int status = sqlite3_exec(help_db, "COMMIT TRANSACTION", NULL, NULL, &errmsg);
    if (status == SQLITE_OK) {
      break;
    } else if (is_busy_status(status)) {
      sqlite3_free(errmsg);
    } else {
      do_rawlog(LT_ERR, "Unable to commit help table transaction: %s",
                errmsg);
      sqlite3_free(errmsg);
      sqlite3_exec(help_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      break;
    }
  }
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
  help_file *h;
  char *entries;
  char *sep = " ";

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

  entries = help_search(executor, h, args[1], sep, NULL);
  if (entries)
    safe_str(entries, buff, bp);
  else
    safe_str("#-1", buff, bp);
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

  entry = help_find_entry(help_dat, the_topic);
  if (!entry) {
    return T("#-1 NO ENTRY");
  }
  safe_strl(entry->body, entry->bodylen, buff, &bp);
  help_free_entry(entry);
  *bp = '\0';
  return buff;
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
    entry = help_find_entry(help_dat, the_topic);
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

  buff = mush_calloc(help_dat->entries, sizeof(char *), "help.search");

  lister = prepare_statement(help_db,
                             "SELECT name FROM topics WHERE catid = (SELECT id FROM categories WHERE name = ?) AND name LIKE ? ESCAPE '$' ORDER BY name",
                             "help.list.entries");
  sqlite3_bind_text(lister, 1, help_dat->command, strlen(help_dat->command),
                    SQLITE_TRANSIENT);
  like = glob_to_like(patcopy, '$', &likelen);
  sqlite3_bind_text(lister, 2, like, likelen, free_string);
  mush_free(patcopy, "string");

  do {
    status = sqlite3_step(lister);
    if (status == SQLITE_ROW) {
      buff[matches++] = mush_strdup((const char *)sqlite3_column_text(lister, 0),
                               "help.entry.name");
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

/* Generate a page of the index of the help file (The old pre-generated 'help
 * entries' tables), 0-indexed.
 */
static char *
entries_from_offset(help_file *h, int off)
{
  enum { ENTRIES_PER_PAGE = 48, LONG_TOPIC = 25 };
  static char buff[BUFFER_LEN];
  char *bp;
  int count = 0;
  char *entry1, *entry2, *entry3;
  int n = 0;
  int page = 0;
  char **entries;
  int nentries;
  
  bp = buff;

  /* Not all pages contain the same number of entries (due to topics with
   * long names taking up more than one spot in the list), so we have to
   * calculate the entire thing, from start to finish, to ensure the starting
   * point for each page, and the total number of pages used, is totally
   * accurate. Sigh.
   */
  entries = list_matching_entries("*", h, &nentries);
  
  for (page = 0; n < nentries; page++) {
    count = 0;
    while (count <= ENTRIES_PER_PAGE && n < h->entries) {

      if (n >= nentries)
        break;
      entry1 = entries[n];
      n += 1;

      if (entry1[0] == '&')
        entry1 += 1;

      if (n >= nentries) {
        /* Last record */
        if (page == off) {
          safe_chr(' ', buff, &bp);
          safe_str(entry1, buff, &bp);
          safe_chr('\n', buff, &bp);
          count += 1;
        }
        break;
      }
      entry2 = entries[n];
      n += 1;

      if (entry2[0] == '&')
        entry2 += 1;

      if (strlen(entry1) > LONG_TOPIC) {
        if (strlen(entry2) > LONG_TOPIC) {
          if (page == off)
            safe_format(buff, &bp, " %-76.76s\n", entry1);
          n -= 1;
          count += 1;
        } else {
          if (page == off)
            safe_format(buff, &bp, " %-51.51s %-25.25s\n", entry1, entry2);
          count += 2;
        }
      } else {
        if (strlen(entry2) > LONG_TOPIC) {
          if (page == off)
            safe_format(buff, &bp, " %-25.25s %-51.51s\n", entry1, entry2);
          count += 2;
        } else {
          if (n < nentries) {
            entry3 = entries[n];
            if (entry3[0] == '&')
              entry3 += 1;
          } else
            entry3 = "";

          if (!*entry3 || strlen(entry3) > LONG_TOPIC) {
            if (page == off)
              safe_format(buff, &bp, " %-25.25s %-25.25s\n", entry1, entry2);
            count += 2;
          } else {
            if (page == off)
              safe_format(buff, &bp, " %-25.25s %-25.25s %-25.25s\n", entry1,
                          entry2, entry3);
            n += 1;
            count += 3;
          }
        }
      }
    }
    if (page == off)
      safe_chr('\n', buff, &bp);
  }

  /* There are 'page' pages in total */
  if (off < (page - 1)) {
    if (page == (off + 2)) {
      safe_format(buff, &bp, "For more, see ENTRIES-%d\n", off + 2);
    } else if (page > (off + 2)) {
      safe_format(buff, &bp, "For more, see ENTRIES-%d through %d\n", off + 2,
                  page);
    }
  }

  *bp = '\0';

  if (entries) {
    free_entry_list(entries, nentries);
  }
  
  if (bp == buff)
    return NULL;

  return buff;
}

extern const unsigned char *tables;

static bool
is_index_entry(const char *topic, int *offset)
{
  static pcre *entry_re = NULL;
  static pcre_extra *extra = NULL;
  int ovec[33], ovecsize = 33;
  int r;

  if (strcasecmp(topic, "entries") == 0 || strcasecmp(topic, "&entries") == 0) {
    *offset = 0;
    return 1;
  }

  if (!entry_re) {
    const char *errptr = NULL;
    int erroffset = 0;
    entry_re = pcre_compile("^&?entries-([0-9]+)$", PCRE_CASELESS, &errptr,
                            &erroffset, tables);
    extra = pcre_study(entry_re, pcre_study_flags, &errptr);
  }

  if ((r = pcre_exec(entry_re, extra, topic, strlen(topic), 0, 0, ovec,
                     ovecsize)) == 2) {
    char buff[BUFFER_LEN];
    pcre_copy_substring(topic, ovec, r, 1, buff, BUFFER_LEN);
    *offset = parse_integer(buff) - 1;
    if (*offset < 0)
      *offset = 0;
    return 1;
  } else
    return 0;
}
