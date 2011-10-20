/**
 * \file match.c
 *
 * \brief Matching of object names.
 *
 * \verbatim
 * These are the PennMUSH name-matching routines, fully re-entrant.
 *  match_result_relative(who,where,name,type,flags) return match, AMBIGUOUS or NOTHING
 *  match_result(who,name,type,flags) - return match, AMBIGUOUS, or NOTHING
 *  noisy_match_result(who,name,type,flags) - return match or NOTHING,
 *      and notify player on failures
 *  last_match_result(who,name,type,flags) - return match or NOTHING,
 *      and return the last match found in ambiguous situations
 *
 *  match_result_internal() does the legwork for all of the above.
 *
 * who = dbref of player to match for
 * where = dbref of object to match relative to. For all functions which don't take a 'where' arg, use 'who'.
 * name = string to match on
 * type = preferred type(s) of match (TYPE_THING, etc.) or NOTYPE
 * flags = a set of bits indicating what kind of matching to do
 *
 * flags are defined in match.h, but here they are for reference:
 * MAT_CHECK_KEYS       - prefer objects whose Basic lock 'who' passes
 * MAT_GLOBAL           - match in master room
 * MAT_REMOTES          - match ZMR exits
 * MAT_NEAR             - match things nearby
 * MAT_CONTROL          - only match objects 'who' controls
 * MAT_ME               - match "me"
 * MAT_HERE             - match "here"
 * MAT_ABSOLUTE         - match any <#dbref>
 * MAT_PMATCH           - match <playerName> or *<playerName>
 * MAT_PLAYER           - match *<playerName>
 * MAT_NEIGHBOR         - match something in 'where's location
 * MAT_POSSESSION       - match something in 'where's inventory
 * MAT_EXIT             - match an exit in 'where's location
 * MAT_CARRIED_EXIT     - match an exit in the room 'where'
 * MAT_CONTAINER        - match the name of 'where's location
 * MAT_REMOTE_CONTENTS  - matches the same as MAT_POSSESSION
 * MAT_ENGLISH          - match natural english 'my 2nd flower'
 * MAT_TYPE             - match only objects of the given type(s)
 * MAT_EXACT            - only do full-name matching, no partial names
 * MAT_EVERYTHING       - me,here,absolute,player,neighbor,possession,exit
 * MAT_NEARBY           - everything,near
 * MAT_OBJECTS          - me,absolute,player,neigbor,possession
 * MAT_NEAR_THINGS      - objects,near
 * MAT_REMOTE           - absolute,player,remote_contents,exit,remotes
 * MAT_LIMITED          - absolute,player,neighbor
 * MAT_CONTENTS         - only match objects located inside 'where'
 * MAT_OBJ_CONTENTS     - possession,player,absolute,english,contents
 * \endverbatim
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
#include "attrib.h"

static int parse_english(char **name, long *flags);
static dbref match_player(dbref who, const char *name, int partial);
extern int check_alias(const char *command, const char *list);  /* game.c */
static dbref choose_thing(const dbref who, const int preferred_type, long flags,
                          dbref thing1, dbref thing2);
static dbref match_result_internal(dbref who, dbref where, const char *xname,
                                   int type, long flags);


dbref
noisy_match_result(const dbref who, const char *name, const int type,
                   const long flags)
{
  dbref match;

  match = match_result(who, name, type, flags | MAT_NOISY);
  if (!GoodObject(match))
    return NOTHING;
  else
    return match;
}

dbref
last_match_result(const dbref who, const char *name, const int type,
                  const long flags)
{
  return match_result(who, name, type, flags | MAT_LAST);
}

dbref
match_controlled(dbref player, const char *name)
{
  return noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING | MAT_CONTROL);
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
 *  d. *player
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


/* MATCHED() is called from inside the MATCH_LIST macro. full is 1 if the
  match was full/exact, and 0 if it was partial */
#define MATCHED(full) \
  { \
    if (!MATCH_CONTROLS) { \
      /* Found a matching object, but we lack necessary control */ \
      nocontrol = 1; \
      continue; \
    } \
    if (!final) { \
      bestmatch = BEST_MATCH; \
      if (bestmatch != match) { \
        /* Previously matched item won over due to type, @lock, etc, checks */ \
        continue; \
      } \
      if (full) { \
        if (exact) { \
          /* Another exact match */ \
          curr++; \
        } else { \
          /* Ignore any previous partial matches now we have an exact match */ \
          exact = 1; \
          curr = 1; \
          right_type = 0; \
        } \
      } else { \
        /* Another partial match */ \
        curr++; \
      } \
      if (type != NOTYPE && (Typeof(bestmatch) & type)) \
        right_type++; \
    } else { \
      curr++; \
      if (curr == final) { \
        /* we've successfully found the Nth item */ \
        bestmatch = match; \
        done = 1; \
        break; \
      } \
    } \
  }

/* MATCH_LIST is called from inside the match_result function. start is the
  dbref to begin matching at (we loop through using DOLIST()) */
#define MATCH_LIST(start) \
  { \
    if (done) \
      break; /* already found the Nth object we needed */ \
    match = start; \
    DOLIST(match, match) { \
      if (!MATCH_TYPE) { \
        /* Exact-type match required, but failed */ \
        continue; \
      } else if (match == abs) { \
        /* absolute dbref match in list */ \
        MATCHED(1); \
      } else if (!can_interact(match, who, INTERACT_MATCH, NULL)) { \
        /* Not allowed to match this object */ \
        continue; \
      } else if (match_aliases(match, name) || (!IsExit(match) && !strcasecmp(Name(match), name))) { \
        /* exact name match */ \
        MATCHED(1); \
      } else if (!(flags & MAT_EXACT) && (!exact || !GoodObject(bestmatch)) && !IsExit(match) && string_match(Name(match), name)) { \
        /* partial name match */ \
        MATCHED(0); \
      } \
    } \
  }

#define MATCH_CONTROLS (!(flags & MAT_CONTROL) || controls(who, match))

#define MATCH_TYPE ((type & Typeof(match)) ? 1 : ((flags & MAT_TYPE) ? 0 : -1))

#define MATCH_CONTENTS (!(flags & MAT_CONTENTS) || (Location(match) == where))

#define BEST_MATCH choose_thing(who, type, flags, bestmatch, match)

static dbref
choose_thing(const dbref who, const int preferred_type, long flags,
             dbref thing1, dbref thing2)
{
  int key;
  /* If there's only one valid thing, return it */
  /* Rather convoluted to ensure we always return AMBIGUOUS, not NOTHING, if we have one of each */
  /* (Apologies to Theodor Geisel) */
  if (!GoodObject(thing1) && !GoodObject(thing2)) {
    if (thing1 == NOTHING)
      return thing2;
    else
      return thing1;
  } else if (!GoodObject(thing1)) {
    return thing2;
  } else if (!GoodObject(thing2)) {
    return thing1;
  }

  /* If a type is given, and only one thing is of that type, return it */
  if (preferred_type != NOTYPE) {
    if (Typeof(thing1) & preferred_type) {
      if (!(Typeof(thing2) & preferred_type)) {
        return thing1;
      }
    } else if (Typeof(thing2) & preferred_type) {
      return thing2;
    }
  }

  if (flags & MAT_CHECK_KEYS) {
    key = could_doit(who, thing1, NULL);
    if (!key && could_doit(who, thing2, NULL)) {
      return thing2;
    } else if (key && !could_doit(who, thing2, NULL)) {
      return thing1;
    }
  }
  /* No luck. Return last match */
  return thing2;
}

static dbref
match_player(dbref who, const char *name, int partial)
{
  dbref match;

  if (*name == LOOKUP_TOKEN) {
    name++;
  }

  while (isspace((unsigned char) *name)) {
    name++;
  }

  match = lookup_player(name);
  if (match != NOTHING) {
    return match;
  }
  return (GoodObject(who) && partial ? visible_short_page(who, name) : NOTHING);
}

int
match_aliases(dbref match, const char *name)
{

  if (!IsPlayer(match) && !IsExit(match)) {
    return 0;
  }

  if (IsExit(match) && check_alias(name, Name(match)))
    return 1;
  else {
    char tbuf1[BUFFER_LEN];
    ATTR *a = atr_get_noparent(match, "ALIAS");
    if (!a)
      return 0;
    mush_strncpy(tbuf1, atr_value(a), BUFFER_LEN);
    return check_alias(name, tbuf1);
  }
}

dbref
match_result(dbref who, const char *xname, int type, long flags)
{
  return match_result_internal(who, who, xname, type, flags);
}

dbref
match_result_relative(dbref who, dbref where, const char *xname, int type,
                      long flags)
{
  return match_result_internal(who, where, xname, type, flags);
}

/* The object 'who' is trying to find something called 'xname' relative to the object 'where'.
 * In most cases, 'who' and 'where' will be the same object. */
static dbref
match_result_internal(dbref who, dbref where, const char *xname, int type,
                      long flags)
{
  dbref match;                  /* object we're currently checking for a match */
  dbref loc;                    /* location of 'where' */
  dbref bestmatch = NOTHING;    /* the best match we've found so bar */
  dbref abs = parse_objid(xname);       /* try to match xname as a dbref/objid */
  int final = 0;                /* the Xth object we want, with english matching (5th foo) */
  int curr = 0;                 /* the number of matches found so far, when 'final' is used */
  int nocontrol = 0;            /* set when we've matched an object, but don't control it and MAT_CONTROL is given */
  int right_type = 0;           /* number of objects of preferred type found, when we have a type but MAT_TYPE isn't given */
  int exact = 0;                /* set to 1 when we've found an exact match, not just a partial one */
  int done = 0;                 /* set to 1 when we're using final, and have found the Xth object */
  int goodwhere = RealGoodObject(where);
  char *name, *sname;           /* name contains the object name searched for, after english matching tokens are stripped from xname */

  if (!goodwhere)
    loc = NOTHING;
  else if (IsRoom(where))
    loc = where;
  else if (IsExit(where))
    loc = Source(where);
  else
    loc = Location(where);

  if (((flags & MAT_NEAR) && !goodwhere)
      || ((flags & MAT_CONTENTS) && !goodwhere)) {
    /* It can't be nearby/in where's contents if where is invalid */
    if ((flags & MAT_NOISY) && GoodObject(who)) {
      notify(who, T("I can't see that here."));
    }
    return NOTHING;
  }

  /* match "me" */
  match = where;
  if (goodwhere && MATCH_TYPE && (flags & MAT_ME) && !(flags & MAT_CONTENTS)
      && !strcasecmp(xname, "me")) {
    if (MATCH_CONTROLS)
      return match;
    else
      nocontrol = 1;
  }

  /* match "here" */
  match = (goodwhere ? (IsRoom(where) ? NOTHING : Location(where)) : NOTHING);
  if ((flags & MAT_HERE) && !(flags & MAT_CONTENTS)
      && !strcasecmp(xname, "here") && GoodObject(match)
      && MATCH_TYPE) {
    if (MATCH_CONTROLS) {
      return match;
    } else {
      nocontrol = 1;
    }
  }

  /* match *<player>, or <player> */
  if (((flags & MAT_PMATCH) ||
       ((flags & MAT_PLAYER) && *xname == LOOKUP_TOKEN)) &&
      ((type & TYPE_PLAYER) || !(flags & MAT_TYPE))) {
    match = match_player(who, xname, !(flags & MAT_EXACT));
    if (MATCH_CONTENTS) {
      if (GoodObject(match)) {
        if (!(flags & MAT_NEAR) || Long_Fingers(who) ||
            (nearby(who, match) || controls(who, match))) {
          if (MATCH_CONTROLS) {
            return match;
          } else {
            nocontrol = 1;
          }
        }
      } else {
        bestmatch = BEST_MATCH;
      }
    }
  }

  /* dbref match */
  match = abs;
  if (RealGoodObject(match) && (flags & MAT_ABSOLUTE) && MATCH_TYPE
      && MATCH_CONTENTS) {
    if (!(flags & MAT_NEAR) || Long_Fingers(who)
        || (nearby(who, match) || controls(who, match))) {
      /* valid dbref match */
      if (MATCH_CONTROLS) {
        return match;
      } else {
        nocontrol = 1;
      }
    }
  }

  sname = name = mush_strdup(xname, "mri.string");
  if (flags & MAT_ENGLISH) {
    /* English-style matching */
    final = parse_english(&name, &flags);
  }

  while (1) {
    if (goodwhere && ((flags & (MAT_POSSESSION | MAT_REMOTE_CONTENTS)))) {
      MATCH_LIST(Contents(where));
    }
    if (GoodObject(loc) && (flags & MAT_NEIGHBOR) && !(flags & MAT_CONTENTS)
        && loc != where) {
      MATCH_LIST(Contents(loc));
    }
    if ((type & TYPE_EXIT) || !(flags & MAT_TYPE)) {
      if (GoodObject(loc) && IsRoom(loc) && (flags & MAT_EXIT)) {
        if ((flags & MAT_REMOTES) && !(flags & (MAT_NEAR | MAT_CONTENTS))
            && GoodObject(Zone(loc)) && IsRoom(Zone(loc))) {
          MATCH_LIST(Exits(Zone(loc)));
        }
        if ((flags & MAT_GLOBAL) && !(flags & (MAT_NEAR | MAT_CONTENTS))) {
          MATCH_LIST(Exits(MASTER_ROOM));
        }
        if (GoodObject(loc) && IsRoom(loc)) {
          MATCH_LIST(Exits(loc));
        }
      }
    }
    if ((flags & MAT_CONTAINER) && !(flags & MAT_CONTENTS) && goodwhere) {
      MATCH_LIST(loc);
    }
    if ((type & TYPE_EXIT) || !(flags & MAT_TYPE)) {
      if ((flags & MAT_CARRIED_EXIT) && goodwhere && IsRoom(where)
          && ((loc != where) || !(flags & MAT_EXIT))) {
        MATCH_LIST(Exits(where));
      }
    }
    break;
  }

  if (!GoodObject(bestmatch) && final) {
    /* we never found the Nth item */
    bestmatch = NOTHING;
  } else if (!final && curr > 1) {
    /* If we had a preferred type, and only found 1 of that type, give that, otherwise ambiguous */
    if (right_type != 1 && !(flags & MAT_LAST)) {
      bestmatch = AMBIGUOUS;
    }
  }

  if (!GoodObject(bestmatch) && (flags & MAT_NOISY) && GoodObject(who)) {
    /* give error message */
    if (bestmatch == AMBIGUOUS) {
      notify(who, T("I don't know which one you mean!"));
    } else if (nocontrol) {
      notify(who, T("Permission denied."));
    } else {
      notify(who, T("I can't see that here."));
    }
  }

  mush_free(sname, "mri.string");

  return bestmatch;
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
  if ((*flags & (MAT_EXIT | MAT_CARRIED_EXIT))
      && (!strncasecmp(*name, "toward ", 7))) {
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
