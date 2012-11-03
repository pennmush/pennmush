/**
 * \file ptab.c
 *
 * \brief Prefix tables for PennMUSH.
 *
 *
 */
#include "config.h"
#include "copyrite.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "conf.h"
#include "externs.h"
#include "ptab.h"
#include "confmagic.h"

static int ptab_find_exact_nun(PTAB *tab, const char *key);
static int WIN32_CDECL ptab_cmp(const void *, const void *);

/** A ptab entry. */
typedef struct ptab_entry {
  void *data;                   /**< pointer to data */
  char key[BUFFER_LEN];         /**< the index key */
} ptab_entry;

/** The memory usage of a ptab entry, not including data. */
#define PTAB_SIZE (sizeof(struct ptab_entry) - BUFFER_LEN)

/** Initialize a ptab.
 * \param tab pointer to a ptab.
 */
void
ptab_init(PTAB *tab)
{
  if (!tab)
    return;
  tab->state = 0;
  tab->maxlen = tab->len = tab->current = 0;
  tab->tab = NULL;
}

/** Free all entries in a ptab.
 * \param tab pointer to a ptab.
 */
void
ptab_free(PTAB *tab)
{
  if (!tab)
    return;
  if (tab->tab) {
    size_t n;
    for (n = 0; n < tab->len; n++)
      mush_free(tab->tab[n], "ptab.entry");
    mush_free(tab->tab, "ptab");
  }
  tab->tab = NULL;
  tab->state = 0;
  tab->maxlen = tab->len = tab->current = 0;
}

/** Search a ptab for an entry that prefix-matches a given key.
 * We search through unique prefixes of keys in the table to try
 * to match the key we're looking for.
 * \param tab pointer to a ptab.
 * \param key key to search for.
 * \return void pointer to ptab data indexed by key, or NULL if none.
 */
void *
ptab_find(PTAB *tab, const char *key)
{
  if (!tab || !key || !*key || tab->state)
    return NULL;

  if (tab->len < 10) {          /* Just do a linear search for small tables */
    size_t n;
    for (n = 0; n < tab->len; n++) {
      if (string_prefix(tab->tab[n]->key, key)) {
        if (n + 1 < tab->len && string_prefix(tab->tab[n + 1]->key, key))
          return NULL;
        else
          return tab->tab[n]->data;
      }
    }
  } else {                      /* Binary search of the index */
    size_t left = 0;
    int cmp;
    size_t right = tab->len - 1;

    while (1) {
      size_t n = (left + right) / 2;

      if (left > right)
        break;

      cmp = strcasecmp(key, tab->tab[n]->key);

      if (cmp == 0) {
        return tab->tab[n]->data;
      } else if (cmp < 0) {
        int m;
        size_t i;
        /* We need to catch the first unique prefix */
        if (string_prefix(tab->tab[n]->key, key)) {
          for (m = (int) n - 1; m >= 0; m--) {
            if (string_prefix(tab->tab[m]->key, key)) {
              if (strcasecmp(tab->tab[m]->key, key) == 0)
                return tab->tab[m]->data;
            } else
              break;
          }
          /* Non-unique prefix */
          if (m != (int) n - 1)
            return NULL;
          for (i = n + 1; i < tab->len; i++) {
            if (string_prefix(tab->tab[i]->key, key)) {
              if (strcasecmp(tab->tab[i]->key, key) == 0)
                return tab->tab[i]->data;
            } else
              break;
          }
          if (i != n + 1)
            return NULL;
          return tab->tab[n]->data;
        }
        if (left == right)
          break;
        if (n == 0)
          break;
        right = n - 1;
      } else {                  /* cmp > 0 */
        if (left == right)
          break;
        left = n + 1;
      }
    }
  }
  return NULL;
}

/** Search a ptab for an entry that exactly matches a given key.
 * We search through full names of keys in the table to try
 * to match the key we're looking for.
 * \param tab pointer to a ptab.
 * \param key key to search for.
 * \return void pointer to ptab data indexed by key, or NULL if none.
 */
void *
ptab_find_exact(PTAB *tab, const char *key)
{
  int n = ptab_find_exact_nun(tab, key);
  if (n < 0)
    return NULL;
  else
    return tab->tab[n]->data;
}

static int
ptab_find_exact_nun(PTAB *tab, const char *key)
{
  size_t n;

  if (!tab || !key || tab->state)
    return -1;

  if (tab->len < 10) {          /* Just do a linear search for small tables */
    int cmp;
    for (n = 0; n < tab->len; n++) {
      cmp = strcasecmp(tab->tab[n]->key, key);
      if (cmp == 0)
        return (int) n;
      else if (cmp > 0)
        return -1;
    }
  } else {                      /* Binary search of the index */
    size_t left = 0;
    int cmp;
    size_t right = tab->len - 1;

    while (1) {
      n = (left + right) / 2;

      if (left > right)
        break;

      cmp = strcasecmp(key, tab->tab[n]->key);

      if (cmp == 0)
        return (int) n;
      if (left == right)
        break;
      if (cmp < 0) {
        if (n == 0)
          break;
        right = n - 1;
      } else                    /* cmp > 0 */
        left = n + 1;
    }
  }
  return -1;
}

static void
delete_entry(PTAB *tab, size_t n)
{
  mush_free(tab->tab[n], "ptab.entry");
  if (tab->len == 0)
    return;

  /* If we're deleting the last item in the list, just decrement the length.
   *  Otherwise, we have to fill in the hole
   */
  if (n < tab->len - 1) {
    size_t i;
    for (i = n + 1; i < tab->len; i++)
      tab->tab[i - 1] = tab->tab[i];
  }
  tab->len--;
}

/** Delete a ptab entry indexed by key.
 * \param tab pointer to a ptab.
 * \param key key to search for.
 */
void
ptab_delete(PTAB *tab, const char *key)
{
  int n = ptab_find_exact_nun(tab, key);
  if (n >= 0)
    delete_entry(tab, (size_t) n);
  return;
}

/** Put a ptab into insertion state.
 * \param tab pointer to a ptab.
 */
void
ptab_start_inserts(PTAB *tab)
{
  if (!tab)
    return;
  tab->state = 1;
}

static int WIN32_CDECL
ptab_cmp(const void *a, const void *b)
{
  const struct ptab_entry *const *ra = a;
  const struct ptab_entry *const *rb = b;

  return strcasecmp((*ra)->key, (*rb)->key);
}

/** Complete the ptab insertion process by re-sorting the entries.
 * \param tab pointer to a ptab.
 */
void
ptab_end_inserts(PTAB *tab)
{
  struct ptab_entry **tmp;
  if (!tab)
    return;
  tab->state = 0;
  qsort(tab->tab, tab->len, sizeof(struct ptab_entry *), ptab_cmp);

  tmp = realloc(tab->tab, (tab->len + 10) * sizeof(struct ptab_entry *));
  if (!tmp)
    return;
  tab->tab = tmp;
  tab->maxlen = tab->len + 10;
}

/** Grow a table */
static void
ptab_grow(PTAB *tab)
{
  struct ptab_entry **tmp;
  size_t oldmaxlen;

  if (!tab)
    return;

  oldmaxlen = tab->maxlen;

  if (tab->maxlen == 0)
    tab->maxlen = 200;
  else
    tab->maxlen *= 2;
  tmp = realloc(tab->tab, tab->maxlen * sizeof(struct ptab_entry **));
  if (tab->tab == NULL)
    add_check("ptab");
  if (!tmp) {
    tab->maxlen = oldmaxlen;
    return;
  }
  memset(tmp + tab->len, 0, tab->maxlen - tab->len);
  tab->tab = tmp;
}

/** Insert an entry into a ptab. This needs to be bracketed between 
 * calls to ptab_start_inserts() and ptab_end_inserts(), and is meant
 * for mass additions to the table. To insert a single isolated entry,
 * see ptab_insert_one().
 * \param tab pointer to a ptab.
 * \param key key to insert entry under.
 * \param data pointer to entry data.
 */
void
ptab_insert(PTAB *tab, const char *key, void *data)
{
  size_t klen;

  if (!tab || tab->state != 1)
    return;

  if (tab->len >= tab->maxlen)
    ptab_grow(tab);

  klen = strlen(key) + 1;

  tab->tab[tab->len] = mush_malloc(PTAB_SIZE + klen, "ptab.entry");

  tab->tab[tab->len]->data = data;
  memcpy(tab->tab[tab->len]->key, key, klen);
  tab->len++;

}

/** Insert an entry into a ptab. This should be used for inserting single
 * entries. To insert multiple entries at a time, see ptab_insert().
 * \param tab the table
 * \param key the key
 * \param data data pointer
 */
void
ptab_insert_one(PTAB *tab, const char *key, void *data)
{
  size_t len, n;

  if (!tab)
    return;

  if (tab->state) {
    /* In the middle of a ptab_start_inserts()/ptab_end_inserts() block */
    ptab_insert(tab, key, data);
    return;
  }

  if (tab->len + 1 >= tab->maxlen)
    ptab_grow(tab);

  /* Figure out where to insert at */
  for (n = 0; n < tab->len; n++) {
    int m = strcasecmp(tab->tab[n]->key, key);
    if (m == 0)                 /* Duplicate entry. */
      return;
    else if (m > 0)
      break;
  }

  /* Move everything after the location down one */
  if (n < tab->len) {
    size_t m;
    for (m = tab->len - 1; m >= n; m--) {
      tab->tab[m + 1] = tab->tab[m];
      if (m == 0)
        break;
    }
  }

  /* And splice in the new one */
  len = strlen(key) + 1;
  tab->tab[n] = mush_malloc(PTAB_SIZE + len, "ptab.entry");
  tab->tab[n]->data = data;
  memcpy(tab->tab[n]->key, key, len);
  tab->len++;

}

/** Return the data (and optionally the key) of the first entry in a ptab.
 * This function resets the 'current' index in the ptab to the start
 * of the table.
 * \param tab pointer to a ptab.
 * \param key If non-NULL, is set to the entry's key name.
 * \return void pointer to data from first entry, or NULL if none.
 */
void *
ptab_firstentry_new(PTAB *tab, const char **key)
{
  if (!tab || tab->len == 0)
    return NULL;
  tab->current = 1;
  if (key)
    *key = tab->tab[0]->key;
  return tab->tab[0]->data;
}

/** Return the data (and optionally the key) of the next entry in a ptab.
 * This function increments the 'current' index in the ptab.
 * \param tab pointer to a ptab.
 * \param key If non-NULL, is set to the entry's key name.
 * \return void pointer to data from next entry, or NULL if none.
 */
void *
ptab_nextentry_new(PTAB *tab, const char **key)
{
  if (!tab || tab->current >= tab->len)
    return NULL;
  if (key)
    *key = tab->tab[tab->current]->key;
  return tab->tab[tab->current++]->data;
}

/** Header for report of ptab stats.
 * \param player player to notify with table header.
 */
void
ptab_stats_header(dbref player)
{
  notify_format(player, "Table      Entries AvgComparisons %39s", "~Memory");
}

/** Data for one line of report of ptab stats.
 * \param player player to notify with table.
 * \param tab pointer to ptab to summarize stats for.
 * \param pname name of ptab, for row header.
 */
void
ptab_stats(dbref player, PTAB *tab, const char *pname)
{
  size_t m, n;

  m = sizeof(struct ptab_entry *) * tab->maxlen;

  for (n = 0; n < tab->len; n++)
    m += PTAB_SIZE + strlen(tab->tab[n]->key) + 1;

  notify_format(player, "%-10s %7d %14.3f %39d", pname, (int) tab->len,
                log((double) tab->len), (int) m);
}
