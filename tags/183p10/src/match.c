/**
 * \file match.c
 *
 * \brief Matching of object names.
 *
 * These are the PennMUSH name-matching routines, fully re-entrant.
 *  match_result(who,name,type,flags) - return match, AMBIGUOUS, or NOTHING
 *  noisy_match_result(who,name,type,flags) - return match or NOTHING,
 *      and notify player on failures
 *  last_match_result(who,name,type,flags) - return match or NOTHING,
 *      and return the last match found in ambiguous situations
 *
 * who = dbref of player to match for
 * name = string to match on
 * type = preferred type of match (TYPE_THING, etc.) or NOTYPE
 * flags = a set of bits indicating what kind of matching to do
 *
 * flags are defined in match.h, but here they are for reference:
 * MAT_CHECK_KEYS       - check locks when matching
 * MAT_GLOBAL           - match in master room
 * MAT_REMOTES          - match things not nearby
 * MAT_NEAR             - match things nearby
 * MAT_CONTROL          - do a control check after matching
 * MAT_ME               - match "me"
 * MAT_HERE             - match "here"
 * MAT_ABSOLUTE         - match "#dbref"
 * MAT_PLAYER           - match a player's name
 * MAT_NEIGHBOR         - match something in the same room
 * MAT_POSSESSION       - match something I'm carrying
 * MAT_EXIT             - match an exit
 * MAT_CARRIED_EXIT     - match a carried exit (rare)
 * MAT_CONTAINER        - match a container I'm in
 * MAT_REMOTE_CONTENTS  - match the contents of a remote location
 * MAT_ENGLISH          - match natural english 'my 2nd flower'
 * MAT_EVERYTHING       - me,here,absolute,player,neighbor,possession,exit
 * MAT_NEARBY           - everything near
 * MAT_OBJECTS          - me,absolute,player,neigbor,possession
 * MAT_NEAR_THINGS      - objects near
 * MAT_REMOTE           - absolute,player,remote_contents,exit,remotes
 * MAT_LIMITED          - absolute,player,neighbor
 */

#include "copyrite.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "conf.h"
#include "mushdb.h"
#include "externs.h"
#include "case.h"
#include "match.h"
#include "parse.h"
#include "flags.h"
#include "dbdefs.h"
#include "confmagic.h"

static dbref match_result_internal
  (dbref who, const char *name, int type, long flags);
static dbref simple_matches(dbref who, const char *name, long flags);
static int parse_english(char **name, long *flags);
static dbref match_me(const dbref who, const char *name);
static dbref match_here(const dbref who, const char *name);
/** Convenience alias for parse_objid */
#define match_absolute(name) parse_objid(name)
static dbref match_player(const dbref matcher, const char *match_name);
static dbref match_pmatch(const dbref matcher, const char *match_name);
static dbref choose_thing(const dbref match_who, const int preferred_type,
                          long int flags, dbref thing1, dbref thing2);
extern int check_alias(const char *command, const char *list);  /* game.c */


/** A wrapper for returning a match, AMBIGUOUS, or NOTHING.
 * This function attempts to match a name for who, and 
 * can return the matched dbref, AMBIGUOUS, or NOTHING.
 * \param who the looker.
 * \param name name to try to match.
 * \param type type of object to match.
 * \param flags match flags.
 * \return dbref of matched object, or AMBIGUOUS, or NOTHING.
 */
dbref
match_result(const dbref who, const char *name, const int type,
             const long flags)
{
  return match_result_internal(who, name, type, flags);
}

/** A noisy wrapper for returning a match or NOTHING.
 * This function attempts to match a name for who, and 
 * can return the matched dbref or NOTHING (in ambiguous cases,
 * NOTHING is returned). If no match is made, the looker is notified
 * of the failure to match or ambiguity.
 * \param who the looker.
 * \param name name to try to match.
 * \param type type of object to match.
 * \param flags match flags.
 * \return dbref of matched object, or  NOTHING.
 */
dbref
noisy_match_result(const dbref who, const char *name, const int type,
                   const long flags)
{
  return match_result_internal(who, name, type, flags | MAT_NOISY);
}

/** A noisy wrapper for returning a match or NOTHING.
 * This function attempts to match a name for who, and 
 * can return the matched dbref or NOTHING. In ambiguous cases,
 * the last matched thing is returned.
 * \param who the looker.
 * \param name name to try to match.
 * \param type type of object to match.
 * \param flags match flags.
 * \return dbref of matched object, or  NOTHING.
 */
dbref
last_match_result(const dbref who, const char *name, const int type,
                  const long flags)
{
  return match_result_internal(who, name, type, flags | MAT_LAST);
}

/** Wrapper for a noisy match with control checks.
 * This function performs a noisy_match_result() and then checks that
 * the looker controls the matched object before returning it.
 * If the control check fails, the looker is notified and NOTHING
 * is returned.
 * \param player the looker.
 * \param name name to try to match.
 * \return dbref of matched controlled object, or NOTHING.
 */
dbref
match_controlled(dbref player, const char *name)
{
  dbref match;
  match = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING);
  if (GoodObject(match) && !controls(player, match)) {
    notify(player, T("Permission denied."));
    return NOTHING;
  } else {
    return match;
  }
}

/* The real work. Here's the spec:
 * str  --> "me"
 *      --> "here"
 *      --> "#dbref"
 *      --> "*player"
 *      --> adj-phrase name
 *      --> name
 * adj-phrase --> adj
 *            --> adj count
 *            --> count
 * adj  --> "my", "me" (restrict match to inventory)
 *      --> "here", "this", "this here" (restrict match to neighbor objects)
 *      --> "toward" (restrict match to exits)
 * count --> 1st, 21st, etc.
 *       --> 2nd, 22nd, etc.
 *       --> 3rd, 23rd, etc.
 *       --> 4th, 10th, etc.
 * name --> exit_alias
 *      --> full_obj_name
 *      --> partial_obj_name
 *
 * 1. Look for exact matches and return immediately:
 *  a. "me" if requested
 *  b. "here" if requested
 *  c. #dbref, possibly with a control check
 *  c. *player
 * 2. Parse for adj-phrases and restrict further matching and/or
 *    remember the object count
 * 3. Look for matches (remote contents, neighbor, inventory, exits, 
 *    containers, carried exits)
 *  a. If we don't have an object count, collect the number of exact
 *     and partial matches and the best partial match.
 *  b. If we do have an object count, collect the nth exact match
 *     and the nth match (exact or partial). number of matches is always
 *     0 or 1.
 * 4. Make decisions
 *  a. If we got a single exact match, return it
 *  b. If we got multiple exact matches, complain
 *  c. If we got no exact matches, but a single partial match, return it
 *  d. If we got multiple partial matches, complain
 *  e. If we got no matches, complain
 */

#define MATCH_NONE      0x0     /**< No matches were found */
#define MATCH_EXACT     0x1     /**< At least one exact match found */
#define MATCH_PARTIAL   0x2     /**< At least one partial match found, no exact */
/** Prototype for matching functions */
#define MATCH_FUNC_PROTO(fun_name) \
  /* ARGSUSED */ /* try to keep lint happy */ \
  static int fun_name(const dbref who, const char *name, const int type, \
      const long flags, dbref first, \
      dbref *match, int *exact_matches_to_go, int *matches_to_go)
/** Common declaration for matching functions */
#define MATCH_FUNC(fun_name) \
  static int fun_name(const dbref who, const char *name, const int type, \
      const long flags, dbref first __attribute__ ((__unused__)), \
      dbref *match, int *exact_matches_to_go, int *matches_to_go)
/** Macro to execute matching and store some results */
#define RUN_MATCH_FUNC(fun,first) \
   { \
    result = fun(who, name, type, flags, first, &match, \
          &exact_matches_to_go, &matches_to_go); \
    if (result == MATCH_EXACT) { \
      exact_match = match; \
      /* If it's the nth exact match, we're done */ \
      if (matchnum && !exact_matches_to_go) \
        goto finished; \
      /* If it's the n'th match, remember it */ \
      if (matchnum && !matches_to_go) \
        last_match = match; \
    } else if (result == MATCH_PARTIAL) { \
      if (!matchnum || !matches_to_go) \
        last_match = match; \
    } \
   }

MATCH_FUNC_PROTO(match_possession);
MATCH_FUNC_PROTO(match_neighbor);
MATCH_FUNC_PROTO(match_exit);
MATCH_FUNC_PROTO(match_exit_internal);
MATCH_FUNC_PROTO(match_container);
MATCH_FUNC_PROTO(match_list);

static dbref
match_result_internal(dbref who, const char *xname, int type, long flags)
{
  dbref match = NOTHING, last_match = NOTHING, exact_match = NOTHING;
  int exact_matches_to_go, matches_to_go;
  int matchnum = 0;
  int result;
  char *name, *sname;

  /* The quick ones that can never be ambiguous */
  match = simple_matches(who, xname, flags);
  if (GoodObject(match))
    return match;

  sname = name = mush_strdup(xname, "mri.string");

  if (flags & MAT_ENGLISH) {
    /* Check for adjective phrases */
    matchnum = parse_english(&name, &flags);
  }

  /* Perform matching. We've already had flags restricted by any
   * adjective phrases. If matchnum is set, collect the matchnum'th
   * exact match (and stop) and the matchnum'th match (exact or partial, 
   * and store this in case we don't get enough exact matches).
   * If not, collect the number of exact and partial matches and the 
   * last exact and partial matches.
   */
  exact_matches_to_go = matches_to_go = matchnum;
  if (flags & MAT_POSSESSION)
    RUN_MATCH_FUNC(match_possession, NOTHING);
  if (flags & MAT_NEIGHBOR)
    RUN_MATCH_FUNC(match_neighbor, NOTHING);
  if (flags & MAT_REMOTE_CONTENTS)
    RUN_MATCH_FUNC(match_possession, NOTHING);
  if (flags & MAT_EXIT)
    RUN_MATCH_FUNC(match_exit, NOTHING);
  if (flags & MAT_CONTAINER)
    RUN_MATCH_FUNC(match_container, NOTHING);
  if (flags & MAT_CARRIED_EXIT)
    RUN_MATCH_FUNC(match_exit_internal, who);

finished:
  /* Set up the default match_result behavior */
  if (matchnum) {
    /* nth exact match? */
    if (!exact_matches_to_go)
      match = exact_match;
    else if (GoodObject(last_match))
      match = last_match;       /* nth exact-or-partial match, or nothing? */
    /* This shouldn't happen, but just in case we have a valid match,
     * and an invalid last_match in the matchnum case, fall through and
     * use the match.
     */
  } else if (GoodObject(exact_match)) {
    /* How many exact matches? */
    if (exact_matches_to_go == -1)
      match = exact_match;      /* Good */
    else if (flags & MAT_LAST)
      match = exact_match;      /* Good enough */
    else
      match = AMBIGUOUS;
  } else {
    if (!matches_to_go)
      match = NOTHING;          /* No matches */
    else if (matches_to_go == -1)
      match = last_match;       /* Good */
    else if (flags & MAT_LAST)
      match = last_match;       /* Good enough */
    else
      match = AMBIGUOUS;
  }

  /* Handle noisy_match_result */
  if (flags & MAT_NOISY) {
    mush_free(sname, "mri.string");
    switch (match) {
    case NOTHING:
      notify(who, T("I can't see that here."));
      return NOTHING;
    case AMBIGUOUS:
      notify(who, T("I don't know which one you mean!"));
      return NOTHING;
    default:
      return match;
    }
  }
  mush_free(sname, "mri.string");
  return match;
}

static dbref
simple_matches(dbref who, const char *name, long flags)
{
  dbref match = NOTHING;
  if (flags & MAT_ME) {
    match = match_me(who, name);
    if (GoodObject(match))
      return match;
  }
  if (flags & MAT_HERE) {
    match = match_here(who, name);
    if (GoodObject(match))
      return match;
  }
  if (!(flags & MAT_NEAR) || Long_Fingers(who)) {
    if (flags & MAT_ABSOLUTE) {
      match = match_absolute(name);
      if (GoodObject(match)) {
        if (flags & MAT_CONTROL) {
          /* Check for control */
          if (controls(who, match) || nearby(who, match))
            return match;
        } else {
          return match;
        }
      }
    }
    if (flags & MAT_PLAYER) {
      match = match_player(who, name);
      if (GoodObject(match))
        return match;
    }
    if (flags & MAT_PMATCH) {
      match = match_pmatch(who, name);
      if (GoodObject(match))
        return match;
    }
  } else {
    /* We're doing a nearby match and the player doesn't have
     * long_fingers, so it's a controlled absolute
     */
    match = match_absolute(name);
    if (GoodObject(match) && (controls(who, match) || nearby(who, match)))
      return match;
  }
  return NOTHING;
}


/* 
 * adj-phrase --> adj
 *            --> adj count
 *            --> count
 * adj  --> "my", "me" (restrict match to inventory)
 *      --> "here", "this", "this here" (restrict match to neighbor objects)
 *      --> "toward" (restrict match to exits)
 * count --> 1st, 21st, etc.
 *       --> 2nd, 22nd, etc.
 *       --> 3rd, 23rd, etc.
 *       --> 4th, 10th, etc.
 *
 * We return the count, we position the pointer at the end of the adj-phrase
 * (or at the beginning, if we fail), and we modify the flags if there
 * are restrictions
 */
static int
parse_english(char **name, long *flags)
{
  int saveflags = *flags;
  char *savename = *name;
  char *mname;
  char *e;
  int count = 0;

  /* Handle restriction adjectives first */
  if (*flags & MAT_NEIGHBOR) {
    if (!strncasecmp(*name, "this here ", 10)) {
      *name += 10;
      *flags &= ~(MAT_POSSESSION | MAT_EXIT);
    } else if (!strncasecmp(*name, "here ", 5)
               || !strncasecmp(*name, "this ", 5)) {
      *name += 5;
      *flags &=
        ~(MAT_POSSESSION | MAT_EXIT | MAT_REMOTE_CONTENTS | MAT_CONTAINER);
    }
  }
  if ((*flags & MAT_POSSESSION) && (!strncasecmp(*name, "my ", 3)
                                    || !strncasecmp(*name, "me ", 3))) {
    *name += 3;
    *flags &= ~(MAT_NEIGHBOR | MAT_EXIT | MAT_CONTAINER | MAT_REMOTE_CONTENTS);
  }
  if ((*flags & MAT_EXIT) && (!strncasecmp(*name, "toward ", 7))) {
    *name += 7;
    *flags &=
      ~(MAT_NEIGHBOR | MAT_POSSESSION | MAT_CONTAINER | MAT_REMOTE_CONTENTS);
  }

  while (**name == ' ')
    (*name)++;

  /* If the name was just 'toward' (with no object name), reset 
   * everything and press on.
   */
  if (!**name) {
    *name = savename;
    *flags = saveflags;
    return 0;
  }

  /* Handle count adjectives */
  if (!isdigit((unsigned char) **name)) {
    /* Quick exit */
    return 0;
  }
  mname = strchr(*name, ' ');
  if (!mname) {
    /* Quick exit - count without a noun */
    return 0;
  }
  /* Ok, let's see if we can get a count adjective */
  savename = *name;
  *mname = '\0';
  count = strtoul(*name, &e, 10);
  if (e && *e) {
    if (count < 1) {
      count = -1;
    } else if ((count > 10) && (count < 14)) {
      if (strcasecmp(e, "th"))
        count = -1;
    } else if ((count % 10) == 1) {
      if (strcasecmp(e, "st"))
        count = -1;
    } else if ((count % 10) == 2) {
      if (strcasecmp(e, "nd"))
        count = -1;
    } else if ((count % 10) == 3) {
      if (strcasecmp(e, "rd"))
        count = -1;
    } else if (strcasecmp(e, "th")) {
      count = -1;
    }
  } else
    count = -1;
  *mname = ' ';
  if (count < 0) {
    /* An error (like '0th' or '12nd') - this wasn't really a count
     * adjective. Reset and press on. */
    *name = savename;
    return 0;
  }
  /* We've got a count adjective */
  *name = mname + 1;
  while (**name == ' ')
    (*name)++;
  return count;
}


static dbref
match_me(const dbref who, const char *name)
{
  return (!strcasecmp(name, "me")) ? who : NOTHING;
}

static dbref
match_here(const dbref who, const char *name)
{
  return (!strcasecmp(name, "here") && GoodObject(Location(who))) ?
    Location(who) : NOTHING;
}


static dbref
match_player(const dbref matcher, const char *match_name)
{
  dbref match;
  const char *p;

  if (*match_name != LOOKUP_TOKEN)
    return NOTHING;
  for (p = match_name + 1; isspace((unsigned char) *p); p++) ;
  /* If lookup_player fails, try a partial match on connected
   * players, 2.0 style. That can return match, NOTHING, AMBIGUOUS
   */
  match = lookup_player(p);
  return (match != NOTHING) ? match : visible_short_page(matcher, p);
}

static dbref
match_pmatch(const dbref matcher, const char *match_name)
{
  dbref match;
  const char *p;

  for (p = match_name; isspace((unsigned char) *p); p++) ;
  /* If lookup_player fails, try a partial match on connected
   * players, 2.0 style. That can return match, NOTHING, AMBIGUOUS
   */
  match = lookup_player(p);
  return (match != NOTHING) ? match : visible_short_page(matcher, p);
}

/* Run down a contents list and try to match */
MATCH_FUNC(match_list)
{
  dbref absolute;
  dbref alias_match;
  int match_type = MATCH_NONE;
  int nth_match = (*exact_matches_to_go != 0);

  /* If we were given an absolute dbref, remember it */
  absolute = match_absolute(name);
  /* If we were given a player name, remember it */
  alias_match = lookup_player(name);

  DOLIST(first, first) {
    if (first == absolute) {
      /* Got an absolute match, return it */
      *match = first;
      (*exact_matches_to_go)--;
      (*matches_to_go)--;
      return MATCH_EXACT;
    } else if (can_interact(first, who, INTERACT_MATCH) &&
               (!strcasecmp(Name(first), name) ||
                (GoodObject(alias_match) && (alias_match == first)))) {
      /* An exact match, but there may be others */
      (*exact_matches_to_go)--;
      (*matches_to_go)--;
      if (nth_match) {
        if (!(*exact_matches_to_go)) {
          /* We're done */
          *match = first;
          return MATCH_EXACT;
        }
      } else {
        if (match_type == MATCH_EXACT)
          *match = choose_thing(who, type, flags, *match, first);
        else
          *match = first;
        match_type = MATCH_EXACT;
      }
    } else if ((match_type != MATCH_EXACT) && string_match(Name(first), name)
               && can_interact(first, who, INTERACT_MATCH)) {
      /* A partial match, and we haven't done an exact match yet */
      (*matches_to_go)--;
      if (nth_match) {
        if (!(*matches_to_go))
          *match = first;
      } else if (match_type == MATCH_PARTIAL)
        *match = choose_thing(who, type, flags, *match, first);
      else
        *match = first;
      match_type = MATCH_PARTIAL;
    }
  }
  /* If we've made the nth partial match in this round, there's none to go */
  if (nth_match && *matches_to_go < 0)
    *matches_to_go = 0;
  return match_type;
}

MATCH_FUNC(match_exit)
{
  dbref loc;
  loc = (IsRoom(who)) ? who : Location(who);
  if (flags & MAT_REMOTES) {
    if (GoodObject(loc))
      return match_exit_internal(who, name, type, flags, Zone(loc),
                                 match, exact_matches_to_go, matches_to_go);
    else
      return NOTHING;
  } else if (flags & MAT_GLOBAL)
    return match_exit_internal(who, name, type, flags, MASTER_ROOM,
                               match, exact_matches_to_go, matches_to_go);
  return match_exit_internal(who, name, type, flags, loc,
                             match, exact_matches_to_go, matches_to_go);
}

MATCH_FUNC(match_exit_internal)
{
  dbref exit_tmp;
  dbref absolute;
  int match_type = MATCH_NONE;
  int nth_match = (*exact_matches_to_go != 0);

  if (!GoodObject(first) || !IsRoom(first) || !name || !*name)
    return NOTHING;
  /* Store an absolute dbref match if given */
  absolute = match_absolute(name);
  DOLIST(exit_tmp, Exits(first)) {

    if ((exit_tmp == absolute) && (can_interact(exit_tmp, who, INTERACT_MATCH))) {
      /* Absolute match. Return immediately */
      *match = exit_tmp;
      (*exact_matches_to_go)--;
      (*matches_to_go)--;
      return MATCH_EXACT;
    } else if (check_alias(name, Name(exit_tmp))
               && (can_interact(exit_tmp, who, INTERACT_MATCH))) {
      /* Matched an exit alias, but there may be more */
      (*exact_matches_to_go)--;
      (*matches_to_go)--;
      if (nth_match) {
        if (!(*exact_matches_to_go)) {
          /* We're done */
          *match = exit_tmp;
          return MATCH_EXACT;
        }
      } else {
        if (match_type == MATCH_EXACT)
          *match = choose_thing(who, type, flags, *match, exit_tmp);
        else
          *match = exit_tmp;
        match_type = MATCH_EXACT;
      }
    }
  }
  /* If we've made the nth partial match in this round, there's none to go */
  if (nth_match && *matches_to_go < 0)
    *matches_to_go = 0;
  return match_type;
}


MATCH_FUNC(match_possession)
{
  if (!GoodObject(who))
    return NOTHING;
  return match_list(who, name, type, flags, Contents(who), match,
                    exact_matches_to_go, matches_to_go);
}

MATCH_FUNC(match_container)
{
  if (!GoodObject(who))
    return NOTHING;
  return match_list(who, name, type, flags, Location(who), match,
                    exact_matches_to_go, matches_to_go);
}

MATCH_FUNC(match_neighbor)
{
  dbref loc;
  if (!GoodObject(who))
    return NOTHING;
  loc = Location(who);
  if (!GoodObject(loc))
    return NOTHING;
  return match_list(who, name, type, flags, Contents(loc), match,
                    exact_matches_to_go, matches_to_go);
}


static dbref
choose_thing(const dbref match_who, const int preferred_type, long flags,
             dbref thing1, dbref thing2)
{
  int has1;
  int has2;
  /* If there's only one valid thing, return it */
  /* (Apologies to Theodor Geisel) */
  if (thing1 == NOTHING)
    return thing2;
  else if (thing2 == NOTHING)
    return thing1;

  /* If a type is given, and only one thing is of that type, return it */
  if (preferred_type != NOTYPE) {
    if (Typeof(thing1) & preferred_type) {
      if (!(Typeof(thing2) & preferred_type))
        return thing1;
    } else if (Typeof(thing2) & preferred_type)
      return thing2;
  }

  /* If we've asked for a basic lock check, and only one passes, use that */
  if (flags & MAT_CHECK_KEYS) {
    has1 = could_doit(match_who, thing1);
    has2 = could_doit(match_who, thing2);
    if (has1 && !has2)
      return thing1;
    else if (has2 && !has1)
      return thing2;
  }

  /* No luck. Return the higher dbref */
  return (thing1 > thing2 ? thing1 : thing2);
}
