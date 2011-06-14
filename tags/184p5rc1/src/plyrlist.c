/**
 * \file plyrlist.c
 *
 * \brief Player list management for PennMUSH.
 *
 *
 */

#include "config.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "copyrite.h"

#include "conf.h"
#include "externs.h"
#include "attrib.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "flags.h"
#include "htab.h"
#include "mymalloc.h"
#include "parse.h"
#include "confmagic.h"


/** Hash table of player names */
HASHTAB htab_player_list;
slab *player_dbref_slab = NULL;

static int hft_initialized = 0;
static void init_hft(void);
static void delete_dbref(void *);

/** Free a player_dbref struct. */
static void
delete_dbref(void *data)
{
  slab_free(player_dbref_slab, data);
}


static void
init_hft(void)
{
  hash_init(&htab_player_list, 256, delete_dbref);
  player_dbref_slab = slab_create("player list dbrefs", sizeof(dbref));
  hft_initialized = 1;
}

/** Clear the player list htab. */
void
clear_players(void)
{
  if (hft_initialized)
    hashflush(&htab_player_list, 256);
  else
    init_hft();
}


/** Add a player to the player list htab.
 * \param player dbref of player to add.
 */
void
add_player(dbref player)
{
  dbref *p;
  if (!hft_initialized)
    init_hft();
  p = slab_malloc(player_dbref_slab, NULL);
  if (!p)
    mush_panic("Unable to allocate memory in plyrlist!");
  *p = player;
  hashadd(strupper(Name(player)), p, &htab_player_list);
}

/** Add a player's alias list to the player list htab.
 * \param player dbref of player to add.
 * \param alias list of names ot use as hash table keys for player,
 * semicolon-separated.
 */
void
add_player_alias(dbref player, const char *alias)
{
  char tbuf1[BUFFER_LEN], *s, *sp;
  if (!hft_initialized)
    init_hft();
  if (!alias) {
    add_player(player);
    return;
  }
  mush_strncpy(tbuf1, alias, BUFFER_LEN);
  s = trim_space_sep(tbuf1, ALIAS_DELIMITER);
  while (s) {
    sp = split_token(&s, ALIAS_DELIMITER);
    while (sp && *sp && *sp == ' ')
      sp++;
    if (sp && *sp) {
      dbref *p;
      p = slab_malloc(player_dbref_slab, NULL);
      if (!p)
        mush_panic("Unable to allocate memory in plyrlist!");
      *p = player;
      hashadd(strupper(sp), p, &htab_player_list);
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
  dbref *p;
  if (hft_initialized) {
    p = hashfind(strupper(name), &htab_player_list);
    if (!p)
      return NOTHING;
    return *p;
  }
  return NOTHING;
}


/** Remove a player from the player list htab.
 * \param player dbref of player to remove.
 * \param alias key to remove if given.
 */
void
delete_player(dbref player, const char *alias)
{
  if (!hft_initialized) {
    init_hft();
    return;
  }
  if (alias) {
    /* This could be a compound alias, in which case we need to delete
     * them all, but we shouldn't delete the player's own name!
     */
    char tbuf1[BUFFER_LEN], *s, *sp;
    mush_strncpy(tbuf1, alias, BUFFER_LEN);
    s = trim_space_sep(tbuf1, ALIAS_DELIMITER);
    while (s) {
      sp = split_token(&s, ALIAS_DELIMITER);
      while (sp && *sp && *sp == ' ')
        sp++;
      if (sp && *sp && strcasecmp(sp, Name(player)))
        hashdelete(strupper(sp), &htab_player_list);
    }
  } else
    hashdelete(strupper(Name(player)), &htab_player_list);
}

/** Reset all of a player's player list entries (names/aliases).
 * This is called when a player changes name or alias.
 * We remove all their old entries, and add back their new ones.
 * \param player dbref of player
 * \param oldname player's former name (NULL if not changing)
 * \param oldalias player's former aliases (NULL if not changing)
 * \param name player's new name
 * \param alias player's new aliases
 */
void
reset_player_list(dbref player, const char *oldname, const char *oldalias,
                  const char *name, const char *alias)
{
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  if (!oldname)
    name = Name(player);
  if (oldalias) {
    mush_strncpy(tbuf1, oldalias, BUFFER_LEN);
    if (alias) {
      strncpy(tbuf2, alias, BUFFER_LEN - 1);
      tbuf2[BUFFER_LEN - 1] = '\0';
    } else {
      tbuf2[0] = '\0';
    }
  } else {
    /* We are not changing aliases, just name, but we need to get the
     * aliases anyway, since we may change name to something that's
     * in the alias, and thus must not be deleted.
     */
    ATTR *a = atr_get_noparent(player, "ALIAS");
    if (a) {
      mush_strncpy(tbuf1, atr_value(a), BUFFER_LEN);
    } else {
      tbuf1[0] = '\0';
    }
    strcpy(tbuf2, tbuf1);
  }
  /* Delete all the old stuff */
  delete_player(player, tbuf1);
  delete_player(player, NULL);
  /* Add in the new stuff */
  add_player_alias(player, name);
  add_player_alias(player, tbuf2);
}
