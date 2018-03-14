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

HASHTAB help_files; /**< Help filenames hash table */

static int help_init = 0;

static void do_new_spitfile(dbref player, char *arg1, help_file *help_dat);
static const char *string_spitfile(help_file *help_dat, char *arg1);
static help_indx *help_find_entry(help_file *help_dat, const char *the_topic);
static char **list_matching_entries(const char *pattern, help_file *help_dat,
                                    int *len, bool nospace);
static void free_entry_list(char **);
static const char *normalize_entry(help_file *help_dat, const char *arg1);

static void help_build_index(help_file *h, int restricted);

static bool is_index_entry(const char *, int *);
static char *entries_from_offset(help_file *, int);

/** Linked list of help topic names. */
typedef struct TLIST {
  char topic[TOPIC_NAME_LEN + 1]; /**< Name of topic */
  struct TLIST *next;             /**< Pointer to next list entry */
} tlist;

tlist *top = NULL; /**< Pointer to top of linked list of topic names */

help_indx *topics = NULL; /**< Pointer to linked list of topic indexes */
unsigned num_topics = 0;  /**< Number of topics loaded */
unsigned top_topics = 0;  /**< Maximum number of topics loaded */

static void write_topic(long int p);

extern bool help_wild(const char *restrict tstr, const char *restrict dstr);

static char *help_search(dbref executor, help_file *h, char *_term,
                         char *delim);

static char *
help_search(dbref executor, help_file *h, char *_term, char *delim)
{
  static char results[BUFFER_LEN];
  char *rp;
  char searchterm[BUFFER_LEN] = {'\0'}, *st;
  char topic[TOPIC_NAME_LEN + 1] = {'\0'};
  char line[LINE_SIZE + 1] = {'\0'}, *l;
  char cleanline[LINE_SIZE + 1] = {'\0'}, *cl;
  char buff[BUFFER_LEN] = {'\0'}, *bp;
  int n;
  size_t i;
  help_indx *entry;
  FILE *fp;

  memset(results, 0, BUFFER_LEN);

  rp = results;

  if (!delim || !*delim)
    delim = " ";

  if (!_term || !*_term) {
    notify(executor, T("What do you want to search for?"));
    return NULL;
  }

  if (!h->indx || h->entries == 0) {
    notify(executor, T("Sorry, that command is temporarily unvailable."));
    do_rawlog(LT_ERR, "No index for %s.", h->command);
    return NULL;
  }

  st = _term;
  while (*st && (isspace(*st) || *st == '*' || *st == '?')) {
    st++;
  }
  if (!*st) {
    notify(executor, T("You need to be more specific."));
    return NULL;
  }

  st = searchterm;
  safe_chr('*', searchterm, &st);
  safe_str(_term, searchterm, &st);
  safe_chr('*', searchterm, &st);
  *st = '\0';

  if (strchr(searchterm, '\n') || strchr(searchterm, '\r')) {
    notify(executor, T("You can't search for mulitple lines."));
    return NULL;
  }

  if ((fp = fopen(h->file, FOPEN_READ)) == NULL) {
    notify(executor, T("Sorry, that command is temporarily unavailable."));
    do_log(LT_ERR, 0, 0, "Can't open text file %s for reading", h->file);
    return NULL;
  }
  for (i = 0; i < h->entries; i++) {
    entry = &h->indx[i];
    bp = buff;
    if (fseek(fp, entry->pos, 0) < 0L) {
      notify(executor, T("Sorry, that command is temporarily unavailable."));
      do_rawlog(LT_ERR, "Seek error in file %s", h->file);
      fclose(fp);
      return NULL;
    }
    strcpy(topic, strupper(entry->topic + (*entry->topic == '&')));
    for (n = 0; n < BUFFER_LEN; n++) {
      if (fgets(line, LINE_SIZE, fp) == NULL)
        break;
      if (line[0] == '&' || line[0] == '\n') {
        if (i && bp > buff) {
          *bp = '\0';
          if (quick_wild(searchterm, buff)) {
            /* Match */
            if (rp != results)
              safe_str(delim, results, &rp);
            safe_str(topic, results, &rp);
            break;
          }
        }
        if (line[0] == '\n') {
          bp = buff;
          continue;
        } else {
          break;
        }
      } else {
        l = line;
        while (*l && isspace(*l))
          l++;
        for (cl = cleanline; *l; cl++) {
          if (!isspace(*l)) {
            *cl = *l++;
          } else {
            *cl = ' ';
            while (*l && isspace(*l))
              l++;
          }
        }
        *cl = '\0';
        if (bp > buff && !isspace(*(bp - 1)))
          safe_chr(' ', buff, &bp);
        if (safe_str(cleanline, buff, &bp)) {
          *bp = '\0';
          if (quick_wild(searchterm, buff)) {
            /* Match */
            if (rp != results)
              safe_str(delim, results, &rp);
            safe_str(topic, results, &rp);
            break;
          }
          bp = buff;
          safe_str(cleanline, buff, &bp);
        }
      }
    }
  }
  fclose(fp);
  return results;
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
    char *r = help_search(executor, h, arg_left, ", ");
    if (!r)
      return;
    if (*r)
      notify_format(executor, T("Here are the entries which match '%s':\n%s"),
                    arg_left, r);
    else
      notify(executor, T("No matches."));
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

    entries = list_matching_entries(arg_left, h, &len, 0);
    if (len == 0)
      notify_format(executor, T("No entries matching '%s' were found."),
                    arg_left);
    else if (len == 1)
      do_new_spitfile(executor, *entries, h);
    else {
      char buff[BUFFER_LEN];
      char *bp;

      bp = buff;
      arr2list(entries, len, buff, &bp, ", ");
      *bp = '\0';
      notify_format(executor, T("Here are the entries which match '%s':\n%s"),
                    arg_left, buff);
    }
    free_entry_list(entries);
  } else {
    help_indx *entry = NULL;
    int offset = 0;
    entry = help_find_entry(h, arg_left);
    if (entry) {
      do_new_spitfile(executor, arg_left, h);
    } else if (is_index_entry(arg_left, &offset)) {
      char *entries = entries_from_offset(h, offset);
      if (!entries) {
        notify_format(executor, T("No entry for '%s'."), strupper(arg_left));
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
      entries = list_matching_entries(pattern, h, &len, 1);
      if (len == 0)
        notify_format(executor, T("No entry for '%s'"), arg_left);
      else if (len == 1)
        do_new_spitfile(executor, *entries, h);
      else {
        char buff[BUFFER_LEN];
        char *bp;
        bp = buff;
        arr2list(entries, len, buff, &bp, ", ");
        *bp = '\0';
        notify_format(executor, T("Here are the entries which match '%s':\n%s"),
                      arg_left, buff);
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
  h->entries = 0;
  h->indx = NULL;
  h->admin = admin;
  help_build_index(h, h->admin);
  if (!h->indx) {
    do_rawlog(LT_ERR, "Missing index for help_command %s", command_name);
    mush_free(h->command, "help_file.command");
    mush_free(h->file, "help_file.filename");
    mush_free(h, "help_file.entry");
    return;
  }
  (void) command_add(h->command, CMD_T_ANY | CMD_T_NOPARSE, NULL, 0, "SEARCH",
                     cmd_helpcmd);
  hashadd(h->command, h, &help_files);
}

/** Rebuild a help file index.
 * \verbatim
 * This command implements @readcache.
 * \endverbatim
 * \param player the enactor.
 */
void
help_reindex(dbref player)
{
  help_file *curr;

  for (curr = hash_firstentry(&help_files); curr;
       curr = hash_nextentry(&help_files)) {
    if (curr->indx) {
      mush_free(curr->indx, "help_index");
      curr->indx = NULL;
      curr->entries = 0;
    }
    help_build_index(curr, curr->admin);
  }
  if (player != NOTHING) {
    notify(player, T("Help files reindexed."));
    do_rawlog(LT_WIZ, "Help files reindexed by %s(#%d)", Name(player), player);
  } else
    do_rawlog(LT_WIZ, "Help files reindexed.");
}

/** Rebuild a single help file index. Used in inotify reindexing.
 * \param filename the name of the help file to reindex.
 * \return true if a help file was reindexed, false otherwise.
 */
bool
help_reindex_by_name(const char *filename)
{
  help_file *curr;
  bool retval = 0;

  for (curr = hash_firstentry(&help_files); curr;
       curr = hash_nextentry(&help_files)) {
    if (strcmp(curr->file, filename) == 0) {
      if (curr->indx)
        mush_free(curr->indx, "help_index");
      curr->indx = NULL;
      curr->entries = 0;
      help_build_index(curr, curr->admin);
      retval = 1;
    }
  }
  return retval;
}

static void
do_new_spitfile(dbref player, char *arg1, help_file *help_dat)
{
  help_indx *entry = NULL;
  FILE *fp;
  char line[LINE_SIZE + 1];
  char the_topic[LINE_SIZE + 2];
  int default_topic = 0;
  size_t n;

  if (*arg1 == '\0') {
    default_topic = 1;
    arg1 = (char *) help_dat->command;
  } else if (*arg1 == '&') {
    notify(player, T("Help topics don't start with '&'."));
    return;
  }
  if (strlen(arg1) > LINE_SIZE)
    *(arg1 + LINE_SIZE) = '\0';

  if (help_dat->admin) {
    sprintf(the_topic, "&%s", arg1);
  } else
    strcpy(the_topic, arg1);

  if (!help_dat->indx || help_dat->entries == 0) {
    notify(player, T("Sorry, that command is temporarily unvailable."));
    do_rawlog(LT_ERR, "No index for %s.", help_dat->command);
    return;
  }

  entry = help_find_entry(help_dat, the_topic);
  if (!entry && default_topic)
    entry = help_find_entry(help_dat, (help_dat->admin ? "&help" : "help"));

  if (!entry) {
    notify_format(player, T("No entry for '%s'."), arg1);
    return;
  }

  if ((fp = fopen(help_dat->file, FOPEN_READ)) == NULL) {
    notify(player, T("Sorry, that function is temporarily unavailable."));
    do_log(LT_ERR, 0, 0, "Can't open text file %s for reading", help_dat->file);
    return;
  }
  if (fseek(fp, entry->pos, 0) < 0L) {
    notify(player, T("Sorry, that function is temporarily unavailable."));
    do_rawlog(LT_ERR, "Seek error in file %s", help_dat->file);
    fclose(fp);
    return;
  }
  strcpy(the_topic, strupper(entry->topic + (*entry->topic == '&')));
  /* ANSI topics */
  notify_format(player, "%s%s%s", ANSI_HILITE, the_topic, ANSI_END);

  if (SUPPORT_PUEBLO)
    notify_noenter(player, open_tag("SAMP"));
  for (n = 0; n < BUFFER_LEN; n++) {
    if (fgets(line, LINE_SIZE, fp) == NULL)
      break;
    if (line[0] == '&')
      break;
    notify_noenter(player, line);
  }
  if (SUPPORT_PUEBLO)
    notify(player, close_tag("SAMP"));
  fclose(fp);
  if (n >= BUFFER_LEN)
    notify_format(player, T("%s output truncated."), help_dat->command);
}

static help_indx *
help_find_entry(help_file *help_dat, const char *the_topic)
{
  help_indx *entry = NULL;

  if (help_dat->entries < 10) { /* Just do a linear search for small files */
    size_t n;
    for (n = 0; n < help_dat->entries; n++) {
      if (string_prefix(help_dat->indx[n].topic, the_topic)) {
        entry = &help_dat->indx[n];
        break;
      }
    }
  } else { /* Binary search of the index */
    int left = 0;
    int cmp;
    int right = help_dat->entries - 1;

    while (1) {
      int n = (left + right) / 2;

      if (left > right)
        break;

      cmp = strcasecmp(the_topic, help_dat->indx[n].topic);

      if (cmp == 0) {
        entry = &help_dat->indx[n];
        break;
      } else if (cmp < 0) {
        /* We need to catch the first prefix */
        if (string_prefix(help_dat->indx[n].topic, the_topic)) {
          int m;
          for (m = n - 1; m >= 0; m--) {
            if (!string_prefix(help_dat->indx[m].topic, the_topic))
              break;
          }
          entry = &help_dat->indx[m + 1];
          break;
        }
        if (left == right)
          break;
        right = n - 1;
      } else { /* cmp > 0 */
        if (left == right)
          break;
        left = n + 1;
      }
    }
  }
  return entry;
}

static void
write_topic(long int p)
{
  tlist *cur, *nextptr;
  help_indx *temp;
  for (cur = top; cur; cur = nextptr) {
    nextptr = cur->next;
    if (num_topics >= top_topics) {
      top_topics += top_topics / 2 + 20;
      if (topics)
        topics = (help_indx *) realloc(topics, top_topics * sizeof(help_indx));
      else
        topics = (help_indx *) malloc(top_topics * sizeof(help_indx));
      if (!topics) {
        mush_panic("Out of memory");
      }
    }
    temp = &topics[num_topics++];
    temp->pos = p;
    strcpy(temp->topic, cur->topic);
    free(cur);
  }
  top = NULL;
}

static int WIN32_CDECL topic_cmp(const void *s1, const void *s2);
static int WIN32_CDECL
topic_cmp(const void *s1, const void *s2)
{
  const help_indx *a = s1;
  const help_indx *b = s2;

  return strcasecmp(a->topic, b->topic);
}

static void
help_build_index(help_file *h, int restricted)
{
  long bigpos, pos = 0;
  bool in_topic;
  int i, lineno, ntopics;
  char *s, *topic;
  char the_topic[TOPIC_NAME_LEN + 1];
  char line[LINE_SIZE + 1];
  FILE *rfp;
  tlist *cur;

  /* Quietly ignore null values for the file */
  if (!h || !h->file)
    return;
  if ((rfp = fopen(h->file, FOPEN_READ)) == NULL) {
    do_rawlog(LT_ERR, "Can't open %s for reading: %s", h->file,
              strerror(errno));
    return;
  }

  if (restricted)
    do_rawlog(LT_WIZ, "Indexing file %s (admin topics)", h->file);
  else
    do_rawlog(LT_WIZ, "Indexing file %s", h->file);
  topics = NULL;
  num_topics = 0;
  top_topics = 0;
  bigpos = 0L;
  lineno = 0;
  ntopics = 0;

  in_topic = 0;

#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(fileno(rfp), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  while (fgets(line, LINE_SIZE, rfp) != NULL) {
    ++lineno;
    if (ntopics == 0) {
      /* Looking for the first topic, but we'll ignore blank lines */
      if (!line[0]) {
        /* Someone's feeding us /dev/null? */
        do_rawlog(LT_ERR, "Malformed help file %s doesn't start with &",
                  h->file);
        fclose(rfp);
        return;
      }
      if (isspace(line[0]))
        continue;
      if (line[0] != '&') {
        do_rawlog(LT_ERR, "Malformed help file %s doesn't start with &",
                  h->file);
        fclose(rfp);
        return;
      }
    }
    if (line[0] == '&') {
      ++ntopics;
      if (!in_topic) {
        /* Finish up last entry */
        if (ntopics > 1) {
          write_topic(pos);
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
      if ((restricted && the_topic[0] == '&') ||
          (!restricted && the_topic[0] != '&')) {
        the_topic[++i] = '\0';
        cur = (tlist *) malloc(sizeof(tlist));
        strcpy(cur->topic, the_topic);
        cur->next = top;
        top = cur;
      }
    } else {
      if (in_topic) {
        pos = bigpos;
      }
      in_topic = false;
    }
    bigpos = ftell(rfp);
  }

  /* Handle last topic */
  write_topic(pos);
  if (topics)
    qsort(topics, num_topics, sizeof(help_indx), topic_cmp);
  h->entries = num_topics;
  h->indx = topics;
  add_check("help_index");
  fclose(rfp);
  do_rawlog(LT_WIZ, "%d topics indexed.", num_topics);
  return;
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
    entries = list_matching_entries(args[1], h, &len, 0);
    if (len == 0)
      safe_str(T("No matching help topics."), buff, bp);
    else
      arr2list(entries, len, buff, bp, ", ");
    free_entry_list(entries);
  } else
    safe_str(string_spitfile(h, args[1]), buff, bp);
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

  entries = list_matching_entries(args[1], h, &len, 0);
  if (entries) {
    arr2list(entries, len, buff, bp, sep);
    free_entry_list(entries);
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

  entries = help_search(executor, h, args[1], sep);
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
  help_indx *entry = NULL;
  FILE *fp;
  char line[LINE_SIZE + 1];
  char the_topic[LINE_SIZE + 2];
  size_t n;
  static char buff[BUFFER_LEN];
  char *bp;
  int offset = 0;

  strcpy(the_topic, normalize_entry(help_dat, arg1));

  if (!help_dat->indx || help_dat->entries == 0)
    return T("#-1 NO INDEX FOR FILE");

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

  if ((fp = fopen(help_dat->file, FOPEN_READ)) == NULL) {
    return T("#-1 UNAVAILABLE");
  }
  if (fseek(fp, entry->pos, 0) < 0L) {
    fclose(fp);
    return T("#-1 UNAVAILABLE");
  }
  bp = buff;
  for (n = 0; n < BUFFER_LEN; n++) {
    if (fgets(line, LINE_SIZE, fp) == NULL)
      break;
    if (line[0] == '&')
      break;
    safe_str(line, buff, &bp);
  }
  *bp = '\0';
  fclose(fp);
  return buff;
}

/** Return a string with all help entries that match a pattern */
static char **
list_matching_entries(const char *pattern, help_file *help_dat, int *len,
                      bool nospace)
{
  char **buff;
  int offset;
  size_t n;

  if (help_dat->admin)
    offset = 1; /* To skip the leading & */
  else
    offset = 0;

  if (wildcard_count((char *) pattern, 1) >= 0) {
    /* Quick way out, use the other kind of matching */
    char the_topic[LINE_SIZE + 2];
    help_indx *entry = NULL;
    strcpy(the_topic, normalize_entry(help_dat, pattern));
    if (!help_dat->indx || help_dat->entries == 0) {
      *len = 0;
      return NULL;
    }
    entry = help_find_entry(help_dat, the_topic);
    if (!entry) {
      *len = 0;
      return NULL;
    } else {
      *len = 1;
      buff = mush_malloc(sizeof(char **), "help.search");
      *buff = entry->topic + offset;
      return buff;
    }
  }

  buff = mush_calloc(help_dat->entries, sizeof(char *), "help.search");
  *len = 0;

  for (n = 0; n < help_dat->entries; n++)
    if ((nospace ? help_wild(pattern, help_dat->indx[n].topic + offset)
                 : quick_wild(pattern, help_dat->indx[n].topic + offset))) {
      buff[*len] = help_dat->indx[n].topic + offset;
      *len += 1;
    }

  return buff;
}

static void
free_entry_list(char **entries)
{
  if (entries)
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
  size_t n = 0;
  int page = 0;

  bp = buff;

  /* Not all pages contain the same number of entries (due to topics with
   * long names taking up more than one spot in the list), so we have to
   * calculate the entire thing, from start to finish, to ensure the starting
   * point for each page, and the total number of pages used, is totally
   * accurate. Sigh.
   */
  for (page = 0; n < h->entries; page++) {
    count = 0;
    while (count <= ENTRIES_PER_PAGE && n < h->entries) {

      if (n >= h->entries)
        break;
      entry1 = h->indx[n].topic;
      n += 1;

      if (entry1[0] == '&')
        entry1 += 1;

      if (n >= h->entries) {
        /* Last record */
        if (page == off) {
          safe_chr(' ', buff, &bp);
          safe_str(entry1, buff, &bp);
          safe_chr('\n', buff, &bp);
          count += 1;
        }
        break;
      }
      entry2 = h->indx[n].topic;
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
          if (n < h->entries) {
            entry3 = h->indx[n].topic;
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
