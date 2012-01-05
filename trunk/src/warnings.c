/**
 * \file warnings.c
 *
 * \brief Check topology and messages on PennMUSH objects and give warnings
 *
 *
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "copyrite.h"
#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "lock.h"
#include "flags.h"
#include "dbdefs.h"
#include "match.h"
#include "attrib.h"
#include "confmagic.h"


/* We might check for both locked and unlocked warnings if we can't
 * figure out a lock.
 */
#define W_UNLOCKED      0x1     /**< Check for unlocked-object warnings */
#define W_LOCKED        0x2     /**< Check for locked-object warnings */

#define W_EXIT_ONEWAY   0x1     /**< Find one-way exits */
#define W_EXIT_MULTIPLE 0x2     /**< Find multiple exits to same place */
#define W_EXIT_MSGS     0x4     /**< Find exits without messages */
#define W_EXIT_DESC     0x8     /**< Find exits without descs */
#define W_EXIT_UNLINKED 0x10    /**< Find unlinked exits */
/* Space for more exit stuff */
#define W_THING_MSGS    0x100   /**< Find things without messages */
#define W_THING_DESC    0x200   /**< Find things without descs */
/* Space for more thing stuff */
#define W_ROOM_DESC     0x1000  /**< Find rooms without descs */
/* Space for more room stuff */
#define W_PLAYER_DESC   0x10000 /**< Find players without descs */

#define W_LOCK_PROBS    0x100000        /**< Find bad locks */

/* Groups of warnings */
#define W_NONE          0      /**< No warnings */
/** Serious warnings only */
#define W_SERIOUS       (W_EXIT_UNLINKED|W_THING_DESC|W_ROOM_DESC|W_PLAYER_DESC|W_LOCK_PROBS)
/** Standard warnings: serious warnings plus others */
#define W_NORMAL        (W_SERIOUS|W_EXIT_ONEWAY|W_EXIT_MULTIPLE|W_EXIT_MSGS)
/** Extra warnings: standard warnings plus others */
#define W_EXTRA         (W_NORMAL|W_THING_MSGS)
/** All warnings */
#define W_ALL           (W_EXTRA|W_EXIT_DESC)


/** A structure representing a topology warning check. */
typedef struct a_tcheck {
  const char *name;     /**< Name of warning. */
  warn_type flag;       /**< Bitmask of warning. */
} tcheck;


extern int warning_lock_type(const boolexp l);
void complain(dbref player, dbref i, const char *name, const char *desc, ...)
  __attribute__ ((__format__(__printf__, 4, 5)));
extern void check_lock(dbref player, dbref i, const char *name, boolexp be);
static void ct_generic(dbref player, dbref i, warn_type flags);
static void ct_room(dbref player, dbref i, warn_type flags);
static void ct_exit(dbref player, dbref i, warn_type flags);
static void ct_player(dbref player, dbref i, warn_type flags);
static void ct_thing(dbref player, dbref i, warn_type flags);


static tcheck checklist[] = {
  {"none", W_NONE},             /* MUST BE FIRST! */
  {"exit-unlinked", W_EXIT_UNLINKED},
  {"thing-desc", W_THING_DESC},
  {"room-desc", W_ROOM_DESC},
  {"my-desc", W_PLAYER_DESC},
  {"exit-oneway", W_EXIT_ONEWAY},
  {"exit-multiple", W_EXIT_MULTIPLE},
  {"exit-msgs", W_EXIT_MSGS},
  {"thing-msgs", W_THING_MSGS},
  {"exit-desc", W_EXIT_DESC},
  {"lock-checks", W_LOCK_PROBS},

  /* These should stay at the end */
  {"serious", W_SERIOUS},
  {"normal", W_NORMAL},
  {"extra", W_EXTRA},
  {"all", W_ALL},
  {NULL, 0}
};

/** Issue a warning about an object.
 * \param player player to receive the warning notification.
 * \param i object the warning is about.
 * \param name name of the warnings.
 * \param desc a formatting string for the warning message.
 */
void
complain(dbref player, dbref i, const char *name, const char *desc, ...)
{
  char buff[BUFFER_LEN];
  va_list args;

  va_start(args, desc);
  mush_vsnprintf(buff, sizeof buff, desc, args);
  va_end(args);

  notify_format(player, T("Warning '%s' for %s:"),
                name, unparse_object(player, i));
  notify(player, buff);
}

static void
ct_generic(dbref player, dbref i, warn_type flags)
{
  if ((flags & W_LOCK_PROBS)) {
    lock_list *ll;
    for (ll = Locks(i); ll; ll = L_NEXT(ll)) {
      check_lock(player, i, L_TYPE(ll), L_KEY(ll));
    }
  }
}

static void
ct_room(dbref player, dbref i, warn_type flags)
{
  if ((flags & W_ROOM_DESC) && !atr_get(i, "DESCRIBE"))
    complain(player, i, "room-desc", T("room has no description"));
}

static void
ct_exit(dbref player, dbref i, warn_type flags)
{
  dbref j, src, dst;
  int count = 0;
  int lt;
  bool global_return = 0;

  /* i must be an exit, must be in a valid room, and must lead to a
   * different room
   * Remember, for exit i, Exits(i) = source room
   * and Location(i) = destination room
   */

  dst = Destination(i);
  if ((flags & W_EXIT_UNLINKED) && (dst == NOTHING))
    complain(player, i, "exit-unlinked",
             T("exit is unlinked; anyone can steal it"));

  if ((flags & W_EXIT_UNLINKED) && dst == AMBIGUOUS) {
    ATTR *a;
    const char *var = "DESTINATION";
    a = atr_get(i, "DESTINATION");
    if (!a)
      a = atr_get(i, "EXITTO");
    if (a)
      var = "EXITTO";
    if (!a)
      complain(player, i, "exit-unlinked",
               T("Variable exit has no %s attribute"), var);
    else {
      const char *x = atr_value(a);
      if (!x || !*x)
        complain(player, i, "exit-unlinked",
                 T("Variable exit has empty %s attribute"), var);
    }
  }

  if (!Dark(i)) {
    if (flags & W_EXIT_MSGS) {
      lt = warning_lock_type(getlock(i, Basic_Lock));
      if ((lt & W_UNLOCKED) &&
          (!atr_get(i, "OSUCCESS") || !atr_get(i, "ODROP") ||
           !atr_get(i, "SUCCESS")))
        complain(player, i, "exit-msgs",
                 T("possibly unlocked exit missing succ/osucc/odrop"));
      if ((lt & W_LOCKED) && !atr_get(i, "FAILURE"))
        complain(player, i, "exit-msgs",
                 T("possibly locked exit missing fail"));
    }
    if (flags & W_EXIT_DESC) {
      if (!atr_get(i, "DESCRIBE"))
        complain(player, i, "exit-desc", T("exit is missing description"));
    }
  }
  src = Source(i);
  if (!GoodObject(src) || !IsRoom(src))
    return;
  if (src == dst)
    return;
  /* Don't complain about exits linked to HOME or variable exits. */
  if (!GoodObject(dst))
    return;

  for (j = Exits(dst); GoodObject(j); j = Next(j)) {
    if (Location(j) == src)
      count++;
  }
  for (j = Exits(MASTER_ROOM); GoodObject(j); j = Next(j)) {
    if (Location(j) == src) {
      global_return = 1;
      count++;
    }
  }

  if (count <= 1 && flags & W_EXIT_ONEWAY) {
    if (global_return)
      complain(player, i, "exit-oneway",
               T("exit only has a global return exit"));
    else if (count == 0)
      complain(player, i, "exit-oneway", T("exit has no return exit"));
  } else if ((count > 1) && (flags & W_EXIT_MULTIPLE)) {
    if (global_return)
      complain(player, i, "exit-multiple",
               T("exit has multiple (%d) return exits including global exits"),
               count);
    else
      complain(player, i, "exit-multiple",
               T("exit has multiple (%d) return exits"), count);
  }
}



static void
ct_player(dbref player, dbref i, warn_type flags)
{
  if ((flags & W_PLAYER_DESC) && !atr_get(i, "DESCRIBE"))
    complain(player, i, "my-desc", T("player is missing description"));
}



static void
ct_thing(dbref player, dbref i, warn_type flags)
{
  int lt;

  /* Ignore carried objects */
  if (Location(i) == player)
    return;
  if ((flags & W_THING_DESC) && !atr_get(i, "DESCRIBE"))
    complain(player, i, "thing-desc", T("thing is missing description"));

  if (flags & W_THING_MSGS) {
    lt = warning_lock_type(getlock(i, Basic_Lock));
    if ((lt & W_UNLOCKED) &&
        (!atr_get(i, "OSUCCESS") || !atr_get(i, "ODROP") ||
         !atr_get(i, "SUCCESS") || !atr_get(i, "DROP")))
      complain(player, i, "thing-msgs",
               T("possibly unlocked thing missing succ/osucc/drop/odrop"));
    if ((lt & W_LOCKED) && !atr_get(i, "FAILURE"))
      complain(player, i, "thing-msgs",
               T("possibly locked thing missing fail"));
  }
}

/** Set up the default warnings on an object.
 * \param player object to set warnings on.
 */
void
set_initial_warnings(dbref player)
{
  Warnings(player) = W_NORMAL;
  return;
}

/** Set warnings on an object.
 * \verbatim
 * This implements @warnings obj=warning list
 * \endverbatim
 * \param player the enactor.
 * \param name name of object to set warnings on.
 * \param warns list of warnings to set, space-separated.
 */
void
do_warnings(dbref player, const char *name, const char *warns)
{
  dbref thing;
  warn_type w, old;

  switch (thing = match_result(player, name, NOTYPE, MAT_EVERYTHING)) {
  case NOTHING:
    notify(player, T("I don't see that object."));
    return;
  case AMBIGUOUS:
    notify(player, T("I don't know which one you mean."));
    return;
  default:
    if (!controls(player, thing)) {
      notify(player, T("Permission denied."));
      return;
    }
    if (IsGarbage(thing)) {
      notify(player, T("Why would you want to be warned about garbage?"));
      return;
    }
    break;
  }

  old = Warnings(thing);
  w = parse_warnings(player, warns);
  if (w != old) {
    Warnings(thing) = w;
    if (Warnings(thing))
      notify_format(player, T("@warnings set to: %s"),
                    unparse_warnings(Warnings(thing)));
    else
      notify(player, T("@warnings cleared."));
  } else {
    notify(player, T("@warnings not changed."));
  }
}

/** Given a list of warnings, return the bitmask that represents it.
 * \param player dbref to report errors to, or NOTHING.
 * \param warnings the string of warning names
 * \return a warning bitmask
 */
warn_type
parse_warnings(dbref player, const char *warnings)
{
  int found = 0;
  warn_type flags, negate_flags;
  char tbuf1[BUFFER_LEN];
  char *w, *s;
  tcheck *c;

  flags = W_NONE;
  negate_flags = W_NONE;
  if (warnings && *warnings) {
    strcpy(tbuf1, warnings);
    /* Loop through whatever's listed and add on those warnings */
    s = trim_space_sep(tbuf1, ' ');
    w = split_token(&s, ' ');
    while (w && *w) {
      found = 0;
      if (*w == '!') {
        /* Found a negated warning */
        w++;
        for (c = checklist; c->name; c++) {
          if (!strcasecmp(w, c->name)) {
            negate_flags |= c->flag;
            found++;
          }
        }
      } else {
        for (c = checklist; c->name; c++) {
          if (!strcasecmp(w, c->name)) {
            flags |= c->flag;
            found++;
          }
        }
      }
      /* At this point, we haven't matched any warnings. */
      if (!found && player != NOTHING) {
        notify_format(player, T("Unknown warning: %s"), w);
      }
      w = split_token(&s, ' ');
    }
    /* If we haven't matched anything, don't change the player's stuff */
    if (!found)
      return -1;
    return flags & ~negate_flags;
  } else
    return 0;
}

/** Given a warning bitmask, return a string of warnings on it.
 * \param warns the warnings.
 * \return pointer to statically allocated string listing warnings.
 */
const char *
unparse_warnings(warn_type warns)
{
  static char tbuf1[BUFFER_LEN];
  char *tp;
  int listsize, indexx;

  tp = tbuf1;

  /* Get the # of elements in checklist */
  listsize = sizeof(checklist) / sizeof(tcheck);

  /* Point c at last non-null in checklist, and go backwards */
  for (indexx = listsize - 2; warns && (indexx >= 0); indexx--) {
    warn_type the_flag = checklist[indexx].flag;
    if (!(the_flag & ~warns)) {
      /* Which is to say:
       * if the bits set on the_flag is a subset of the bits set on warns
       */
      safe_str(checklist[indexx].name, tbuf1, &tp);
      safe_chr(' ', tbuf1, &tp);
      /* If we've got a flag which subsumes smaller ones, don't
       * list the smaller ones
       */
      warns &= ~the_flag;
    }
  }
  *tp = '\0';
  return tbuf1;
}

static void
check_topology_on(dbref player, dbref i)
{
  warn_type flags;

  /* Skip it if it's NOWARN or the player checking is the owner and
   * is NOWARN.  Also skip GOING objects.
   */
  if (Going(i) || NoWarn(i))
    return;

  /* If the owner is checking, use the flags on the object, and fall back
   * on the owner's flags as default. If it's not the owner checking
   * (therefore, an admin), ignore the object flags, use the admin's flags
   */
  if (Owner(player) == Owner(i)) {
    if (!(flags = Warnings(i)))
      flags = Warnings(player);
  } else
    flags = Warnings(player);

  ct_generic(player, i, flags);

  switch (Typeof(i)) {
  case TYPE_ROOM:
    ct_room(player, i, flags);
    break;
  case TYPE_THING:
    ct_thing(player, i, flags);
    break;
  case TYPE_EXIT:
    ct_exit(player, i, flags);
    break;
  case TYPE_PLAYER:
    ct_player(player, i, flags);
    break;
  }
  return;
}


/** Loop through all objects and check their topology.  */
void
run_topology(void)
{
  int ndone;
  for (ndone = 0; ndone < db_top; ndone++) {
    if (!IsGarbage(ndone) && Connected(Owner(ndone)) && !NoWarn(Owner(ndone))) {
      check_topology_on(Owner(ndone), ndone);
    }
  }
}

/** Wizard command to check all objects.
 * \verbatim
 * This implements @wcheck/all.
 * \endverbatim
 * \param player the enactor.
 */
void
do_wcheck_all(dbref player)
{
  if (!Wizard(player)) {
    notify(player, T("You'd better check your wizbit first."));
    return;
  }
  notify(player, T("Running database topology warning checks"));
  run_topology();
  notify(player, T("Warning checks complete."));
}

/** Check warnings on a specific player by themselves.
 * \param player player checking warnings on their objects.
 */
void
do_wcheck_me(dbref player)
{
  int ndone;
  if (!Connected(player))
    return;
  for (ndone = 0; ndone < db_top; ndone++) {
    if ((Owner(ndone) == player) && !IsGarbage(ndone))
      check_topology_on(player, ndone);
  }
  notify(player, T("@wcheck complete."));
  return;
}


/** Check warnings on a specific object.
 * We check for ownership or hasprivs before allowing this.
 * \param player the enactor.
 * \param name name of object to check.
 */
void
do_wcheck(dbref player, const char *name)
{
  dbref thing;

  switch (thing = match_result(player, name, NOTYPE, MAT_EVERYTHING)) {
  case NOTHING:
    notify(player, T("I don't see that object."));
    return;
  case AMBIGUOUS:
    notify(player, T("I don't know which one you mean."));
    return;
  default:
    if (!(See_All(player) || (Owner(player) == Owner(thing)))) {
      notify(player, T("Permission denied."));
      return;
    }
    if (IsGarbage(thing)) {
      notify(player, T("Why would you want to be warned about garbage?"));
      return;
    }
    break;
  }

  check_topology_on(player, thing);
  notify(player, T("@wcheck complete."));
  return;
}
