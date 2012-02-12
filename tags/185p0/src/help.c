/**
 * \file help.c
 *
 * \brief The PennMUSH help system.
 *
 *
 */
#include "config.h"
/* This might have to be uncommented on some linux distros... */
/* #define _XOPEN_SOURCE 600 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "conf.h"
#include "externs.h"
#include "command.h"
#include "htab.h"
#include "help.h"
#include "log.h"
#include "ansi.h"
#include "parse.h"
#include "pueblo.h"
#include "flags.h"
#include "dbdefs.h"
#include "mymalloc.h"
#include "confmagic.h"

HASHTAB help_files;  /**< Help filenames hash table */

static int help_init = 0;

static void do_new_spitfile(dbref player, char *arg1, help_file *help_dat);
static const char *string_spitfile(help_file *help_dat, char *arg1);
static help_indx *help_find_entry(help_file *help_dat, const char *the_topic);
static char **list_matching_entries(const char *pattern,
                                    help_file *help_dat, int *len,
                                    bool nospace);
static void free_entry_list(char **);
static const char *normalize_entry(help_file *help_dat, const char *arg1);

static void help_build_index(help_file *h, int restricted);

/** Linked list of help topic names. */
typedef struct TLIST {
  char topic[TOPIC_NAME_LEN + 1];       /**< Name of topic */
  struct TLIST *next;                   /**< Pointer to next list entry */
} tlist;

tlist *top = NULL;   /**< Pointer to top of linked list of topic names */

help_indx *topics = NULL;  /**< Pointer to linked list of topic indexes */
unsigned num_topics = 0;   /**< Number of topics loaded */
unsigned top_topics = 0;   /**< Maximum number of topics loaded */

static void write_topic(long int p);

extern bool help_wild(const char *restrict tstr, const char *restrict dstr);

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

  strcpy(save, arg_left);
  if (wildcard_count(arg_left, 1) == -1) {
    int len = 0;
    char **entries;

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
    entry = help_find_entry(h, arg_left);
    if (entry) {
      do_new_spitfile(executor, arg_left, h);
    } else {
      char pattern[BUFFER_LEN], *pp, *sp;
      char **entries;
      int len = 0;
      int type = 0;
      pp = pattern;
      for (sp = save; *sp; sp++) {
        if (isspace((unsigned char) *sp)) {
          if (type) {
            type = 0;
            *pp = '*';
            pp++;
            if (pp >= (pattern + BUFFER_LEN)) {
              notify_format(executor, T("No entry for '%s'"), arg_left);
              return;
            }
          }
        } else if (strchr("0123456789", *sp)) {
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

  if (!command_name || !filename || !*command_name || !*filename)
    return;

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
    mush_free(h->command, "help_file.command");
    mush_free(h->file, "help_file.filename");
    mush_free(h, "help_file.entry");
    return;
  }
  (void) command_add(h->command, CMD_T_ANY | CMD_T_NOPARSE, NULL, 0, NULL,
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

  for (curr = hash_firstentry(&help_files);
       curr; curr = hash_nextentry(&help_files)) {
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

  for (curr = hash_firstentry(&help_files);
       curr; curr = hash_nextentry(&help_files)) {
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
  char *p, line[LINE_SIZE + 1];
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
    return;
  }
  strcpy(the_topic, strupper(entry->topic + (*entry->topic == '&')));
  /* ANSI topics */
  if (ShowAnsi(player)) {
    char ansi_topic[LINE_SIZE + 10];
    sprintf(ansi_topic, "%s%s%s", ANSI_HILITE, the_topic, ANSI_END);
    notify(player, ansi_topic);
  } else
    notify(player, the_topic);

  if (SUPPORT_PUEBLO)
    notify_noenter(player, open_tag("SAMP"));
  for (n = 0; n < BUFFER_LEN; n++) {
    if (fgets(line, LINE_SIZE, fp) == NULL)
      break;
    if (line[0] == '&')
      break;
    if (line[0] == '\n') {
      notify(player, " ");
    } else {
      for (p = line; *p != '\0'; p++)
        if (*p == '\n')
          *p = '\0';
      notify(player, line);
    }
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
  } else {                      /* Binary search of the index */
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
      } else {                  /* cmp > 0 */
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
  size_t n;
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
      if (isspace((unsigned char) line[0]))
        continue;
      if (line[0] != '&') {
        do_rawlog(LT_ERR, "Malformed help file %s doesn't start with &",
                  h->file);
        fclose(rfp);
        return;
      }
    }
    n = strlen(line);
    if (line[n - 1] != '\n') {
      do_rawlog(LT_ERR, "Line %d of %s: line too long", lineno, h->file);
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
           (*topic == ' ' || *topic == '\t') && *topic != '\0'; topic++) ;

      /* Get the topic */
      strcpy(the_topic, "");
      for (i = -1, s = topic; *s != '\n' && *s != '\0'; s++) {
        if (i >= TOPIC_NAME_LEN - 1)
          break;
        if (*s != ' ' || the_topic[i] != ' ')
          the_topic[++i] = *s;
      }
      if ((restricted && the_topic[0] == '&')
          || (!restricted && the_topic[0] != '&')) {
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

  strcpy(the_topic, normalize_entry(help_dat, arg1));

  if (!help_dat->indx || help_dat->entries == 0)
    return T("#-1 NO INDEX FOR FILE");

  entry = help_find_entry(help_dat, the_topic);
  if (!entry) {
    return T("#-1 NO ENTRY");
  }

  if ((fp = fopen(help_dat->file, FOPEN_READ)) == NULL) {
    return T("#-1 UNAVAILABLE");
  }
  if (fseek(fp, entry->pos, 0) < 0L) {
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
    offset = 1;                 /* To skip the leading & */
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
