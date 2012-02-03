/**
 * \file predicat.c
 *
 * \brief Predicates for testing various conditions in PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "lock.h"
#include "flags.h"
#include "match.h"
#include "ansi.h"
#include "parse.h"
#include "dbdefs.h"
#include "privtab.h"
#include "mymalloc.h"
#include "confmagic.h"

int forbidden_name(const char *name);
static void grep_add_attr(char *buff, char **bp, dbref player, int count,
                          ATTR *attr, char *atrval);
static int pay_quota(dbref, int);
extern PRIV attr_privs_view[];

/** A generic function to generate a formatted string. The
 * return value is a statically allocated buffer.
 *
 * \param fmt format string.
 * \return formatted string.
 */
char *WIN32_CDECL
tprintf(const char *fmt, ...)
{
  static char buff[BUFFER_LEN];
  va_list args;

  va_start(args, fmt);
  mush_vsnprintf(buff, sizeof buff, fmt, args);
  va_end(args);

  return buff;
}

/** lock evaluation -- determines if player passes lock on thing, for
 * the purposes of picking up an object or moving through an exit.
 * \param player to check against lock.
 * \param thing thing to check the basic lock on.
 * \param pe_info the pe_info for Basic lock evaluation
 * \retval 1 player passes lock.
 * \retval 0 player fails lock.
 */
int
could_doit(dbref player, dbref thing, NEW_PE_INFO *pe_info)
{
  if (!IsRoom(thing) && Location(thing) == NOTHING)
    return 0;
  return (eval_lock_with(player, thing, Basic_Lock, pe_info));
}

/** Check for CHARGES on thing and, if present, lower.
 * \param thing object being used.
 * \retval 0 charges was set to 0
 * \retval 1 charges not set, or was > 0
 */
int
charge_action(dbref thing)
{
  ATTR *b;
  char tbuf2[BUFFER_LEN];
  int num;

  /* check if object has # of charges */
  b = atr_get_noparent(thing, "CHARGES");

  if (!b) {
    return 1;                   /* no CHARGES */
  } else {
    strcpy(tbuf2, atr_value(b));
    num = atoi(tbuf2);
    if (num > 0) {
      /* charges left, decrement and execute */
      (void) atr_add(thing, "CHARGES", tprintf("%d", num - 1),
                     Owner(b->creator), 0);
      return 1;
    } else {
      /* no charges left, try to execute runout */
      return 0;
    }
  }
}


/** A wrapper for real_did_it that clears the environment first.
 * \param player the enactor.
 * \param thing object being triggered.
 * \param what message attribute for enactor.
 * \param def default message to enactor.
 * \param owhat message attribute for others.
 * \param odef default message to others.
 * \param awhat action attribute to trigger.
 * \param loc location in which action is taking place.
 * \retval 0 no attributes were evaluated (only defaults used).
 * \retval 1 some attributes were evaluated.
 */
int
did_it(dbref player, dbref thing, const char *what, const char *def,
       const char *owhat, const char *odef, const char *awhat, dbref loc)
{
  return real_did_it(player, thing, what, def, owhat, odef, awhat, loc, NULL,
                     NA_INTER_HEAR);
}

/** A wrapper for real_did_it that can set %0 and %1 to dbrefs.
 * \param player the enactor.
 * \param thing object being triggered.
 * \param what message attribute for enactor.
 * \param def default message to enactor.
 * \param owhat message attribute for others.
 * \param odef default message to others.
 * \param awhat action attribute to trigger.
 * \param loc location in which action is taking place.
 * \param env0 dbref to pass as %0, or NOTHING.
 * \param env1 dbref to pass as %1, or NOTHING.
 * \param flags interaction flags to pass to real_did_it.
 * \retval 0 no attributes were present, only defaults were used if given.
 * \retval 1 some attributes were evaluated and used.
 */
int
did_it_with(dbref player, dbref thing, const char *what, const char *def,
            const char *owhat, const char *odef, const char *awhat,
            dbref loc, dbref env0, dbref env1, int flags)
{
  PE_REGS *pe_regs = pe_regs_create(PE_REGS_ARG, "did_it_with");
  int retval;

  if (env0 != NOTHING) {
    pe_regs_setenv(pe_regs, 0, unparse_dbref(env0));
  }
  if (env1 != NOTHING) {
    pe_regs_setenv(pe_regs, 1, unparse_dbref(env1));
  }
  retval = real_did_it(player, thing, what, def, owhat, odef, awhat, loc,
                       pe_regs, flags);

  pe_regs_free(pe_regs);
  return retval;
}


/** A wrapper for did_it that can pass interaction flags.
 * \param player the enactor.
 * \param thing object being triggered.
 * \param what message attribute for enactor.
 * \param def default message to enactor.
 * \param owhat message attribute for others.
 * \param odef default message to others.
 * \param awhat action attribute to trigger.
 * \param loc location in which action is taking place.
 * \param flags interaction flags to pass to real_did_it.
 * \retval 0 no attributes were present, only defaults were used if given.
 * \retval 1 some attributes were evaluated and used.
 */
int
did_it_interact(dbref player, dbref thing, const char *what, const char *def,
                const char *owhat, const char *odef, const char *awhat,
                dbref loc, int flags)
{
  /* Bunch o' nulls */
  return real_did_it(player, thing, what, def, owhat, odef, awhat, loc, NULL,
                     flags);
}

/** Take an action on an object and trigger attributes.
 * \verbatim
 * executes the @attr, @oattr, @aattr for a command - gives a message
 * to the enactor and others in the room with the enactor, and executes
 * an action. We optionally load pe_regs into the queue.
 * \endverbatim
 *
 * \param player the enactor.
 * \param thing object being triggered.
 * \param what message attribute for enactor.
 * \param def default message to enactor.
 * \param owhat message attribute for others.
 * \param odef default message to others.
 * \param awhat action attribute to trigger.
 * \param loc location in which action is taking place.
 * \param pe_regs the pe_regs arguments for the evaluation/queueing
 * \param flags flags controlling type of interaction involved.
 * \retval 0 no attributes were present, only defaults were used if given.
 * \retval 1 some attributes were evaluated and used.
 */
int
real_did_it(dbref player, dbref thing, const char *what, const char *def,
            const char *owhat, const char *odef, const char *awhat, dbref loc,
            PE_REGS *pe_regs, int flags)
{

  char buff[BUFFER_LEN], *bp;
  dbref preserve_orator = orator;
  int attribs_used = 0;
  NEW_PE_INFO *pe_info = NULL;
  ufun_attrib ufun;

  if (!pe_info) {
    pe_info = make_pe_info("pe_info-real_did_it2");
  }

  loc = (loc == NOTHING) ? Location(player) : loc;
  orator = player;
  /* only give messages if the location is good */
  if (GoodObject(loc)) {

    /* message to player */
    if (what && *what) {
      if (fetch_ufun_attrib
          (what, thing, &ufun,
           UFUN_LOCALIZE | UFUN_REQUIRE_ATTR | UFUN_IGNORE_PERMS)) {
        attribs_used = 1;
        if (!call_ufun(&ufun, buff, thing, player, pe_info, pe_regs) && buff[0])
          notify_by(thing, player, buff);
      } else if (def && *def)
        notify_by(thing, player, def);
    }
    /* message to neighbors */
    if (!DarkLegal(player)) {
      if (owhat && *owhat
          && fetch_ufun_attrib(owhat, thing, &ufun,
                               UFUN_LOCALIZE | UFUN_REQUIRE_ATTR |
                               UFUN_IGNORE_PERMS | UFUN_NAME)) {
        attribs_used = 1;
        if (!call_ufun(&ufun, buff, thing, player, pe_info, pe_regs) && buff[0])
          notify_except2(loc, player, thing, buff, flags);
      } else if (odef && *odef) {
        bp = buff;
        safe_format(buff, &bp, "%s %s", Name(player), odef);
        *bp = '\0';
        notify_except2(loc, player, thing, buff, flags);
      }
    }
  }
  if (pe_info) {
    free_pe_info(pe_info);
  }

  if (awhat && *awhat)
    attribs_used = queue_attribute_base(thing, awhat, player, 0, pe_regs, 0)
      || attribs_used;
  orator = preserve_orator;
  return attribs_used;
}

/** Return the first object near another object that is visible to a player.
 *
 * BEWARE:
 *
 * first_visible() does not behave as intended. It _should_ return the first
 * object in `thing' that is !DARK. However, because of the controls() check
 * the function will return a DARK object if the player owns it.
 *
 * The behavior is left as is because so many functions in fundb.c rely on
 * the incorrect behavior to return expected values. The lv*() functions
 * also make rewriting this fairly pointless.
 *
 * \param player the looker.
 * \param thing an object in the location to be inspected.
 * \return dbref of first visible object or NOTHING.
 */
dbref
first_visible(dbref player, dbref thing)
{
  int lck = 0;
  int ldark;
  dbref loc;

  if (!GoodObject(thing) || IsRoom(thing))
    return NOTHING;
  loc = IsExit(thing) ? Source(thing) : Location(thing);
  if (!GoodObject(loc))
    return NOTHING;
  ldark = IsPlayer(loc) ? Opaque(loc) : Dark(loc);

  while (GoodObject(thing)) {
    if (can_interact(thing, player, INTERACT_SEE, NULL)) {
      if (DarkLegal(thing) || (ldark && !Light(thing))) {
        if (!lck) {
          if (See_All(player) || (loc == player) || controls(player, loc))
            return thing;
          lck = 1;
        }
        if (controls(player, thing))    /* this is what causes DARK objects to show */
          return thing;
      } else {
        return thing;
      }
    }
    thing = Next(thing);
  }
  return thing;
}



/** Can a player see something?
 * \param player the looker.
 * \param thing object to be seen.
 * \param can_see_loc 1 if player can see the location, 0 if location is dark.
 * \retval 1 player can see thing.
 * \retval 0 player can not see thing.
 */
int
can_see(dbref player, dbref thing, int can_see_loc)
{
  if (!can_interact(thing, player, INTERACT_SEE, NULL))
    return 0;

  /*
   * 1) your own body isn't listed in a 'look' 2) exits aren't listed in a
   * 'look' 3) unconnected (sleeping) players aren't listed in a 'look'
   */
  if (player == thing || IsExit(thing) ||
      (IsPlayer(thing) && !Connected(thing)))
    return 0;

  /* if thing is in a room set LIGHT, it can be seen */
  else if (IS(Location(thing), TYPE_ROOM, "LIGHT"))
    return 1;

  /* if the room is non-dark, you can see objects which are light or non-dark */
  else if (can_see_loc)
    return (Light(thing) || !DarkLegal(thing));

  /* otherwise room is dark and you can only see lit things */
  else
    return (Light(thing));
}

/** Can a player control a thing?
 * The control rules are, in order:
 *   Only God controls God.
 *   Wizards control everything else.
 *   Nothing else controls a wizard, and only royalty control royalty.
 *   Mistrusted objects control only themselves.
 *   Objects with the same owner control each other, unless the
 *     target object is TRUST and the would-be controller isn't.
 *   If ZMOs allow control, and you pass the ZMO, you control.
 *   If the owner is a Zone Master, and you pass the ZM, you control.
 *   If you pass the control lock, you control.
 *   Otherwise, no dice.
 * \param who object attempting to control.
 * \param what object to be controlled.
 * \retval 1 who controls what.
 * \retval 0 who doesn't control what.
 */
int
controls(dbref who, dbref what)
{
  boolexp c;

  if (!GoodObject(what))
    return 0;

  if (Guest(who))
    return 0;

  if (what == who)
    return 1;

  if (God(what))
    return 0;

  if (Wizard(who))
    return 1;

  if (Wizard(what) || (Hasprivs(what) && !Hasprivs(who)))
    return 0;

  if (Mistrust(who))
    return 0;

  if (Owns(who, what) && (!Inheritable(what) || Inheritable(who)))
    return 1;

  if (Inheritable(what) || IsPlayer(what))
    return 0;

  if (!ZONE_CONTROL_ZMP && (Zone(what) != NOTHING) &&
      eval_lock(who, Zone(what), Zone_Lock))
    return 1;

  if (ZMaster(Owner(what)) && !IsPlayer(what) &&
      eval_lock(who, Owner(what), Zone_Lock))
    return 1;

  c = getlock_noparent(what, Control_Lock);
  if (c != TRUE_BOOLEXP) {
    if (eval_boolexp(who, c, what, NULL))
      return 1;
  }
  return 0;
}

/** Can someone pay for something (in cash and quota)?
 * Does who have enough pennies to pay for something, and if something
 * is being built, does who have enough quota? Wizards, roys
 * aren't subject to either. This function not only checks that they
 * can afford it, but actually charges them.
 * \param who player attempting to pay.
 * \param pennies cost in pennies.
 * \retval 1 who can pay.
 * \retval 0 who can't pay.
 */
int
can_pay_fees(dbref who, int pennies)
{

  if (Guest(who)) {
    notify(who, T("Sorry, you aren't allowed to build."));
    return 0;
  }

  /* check database size -- EVERYONE is subject to this! */
  if (DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
    notify(who, T("Sorry, there is no more room in the database."));
    return 0;
  }
  /* Can they afford it? */
  if (!NoPay(who) && (Pennies(Owner(who)) < pennies)) {
    notify_format(who, T("Sorry, you don't have enough %s."), MONIES);
    return 0;
  }
  /* check building quota */
  if (!pay_quota(who, QUOTA_COST)) {
    notify(who, T("Sorry, your building quota has run out."));
    return 0;
  }

  /* charge */
  payfor(who, pennies);

  return 1;
}

/** Transfer pennies to an object's owner.
 * \param who recipient.
 * \param pennies amount of pennies to give.
 */
void
giveto(dbref who, int pennies)
{
  if (NoPay(who))
    return;                     /* Giving to a NoPay object or owner */
  who = Owner(who);
  if ((Pennies(who) + pennies) > Max_Pennies(who))
    s_Pennies(who, Max_Pennies(who));
  else
    s_Pennies(who, Pennies(who) + pennies);
}

/** Debit a player's pennies, if they can afford it.
 * \param who player to debit.
 * \param cost number of pennies to debit.
 * \retval 1 player successfully debited.
 * \retval 0 player can't afford the cost.
 */
int
payfor(dbref who, int cost)
{
  /* subtract cost from who's pennies */
  int tmp;
  dbref owner;
  if ((cost == 0) || NoPay(who))
    return 1;
  owner = Owner(who);
  if ((tmp = Pennies(owner)) >= cost) {
    if (Track_Money(owner)) {
      notify_format(owner, T("GAME: %s(%s) spent %d %s."),
                    Name(who), unparse_dbref(who), cost,
                    (cost == 1) ? MONEY : MONIES);
    }
    s_Pennies(owner, tmp - cost);
    return 1;
  } else {
    if (Track_Money(owner)) {
      notify_format(owner, T("GAME: %s(%s) tried to spend %d %s."),
                    Name(who), unparse_dbref(who), cost,
                    (cost == 1) ? MONEY : MONIES);
    }
    return 0;
  }
}

/** Debit a player's pennies, if they can afford it.
 * \param who player to debit.
 * \param cost number of pennies to debit.
 * \retval 1 player successfully debited.
 * \retval 0 player can't afford the cost.
 */
int
quiet_payfor(dbref who, int cost)
{
  /* subtract cost from who's pennies */
  int tmp;
  if (NoPay(who))
    return 1;
  who = Owner(who);
  if ((tmp = Pennies(who)) >= cost) {
    s_Pennies(who, tmp - cost);
    return 1;
  } else
    return 0;
}

/** Retrieve the amount of quote remaining to a player.
 * Figure out a player's quota. Add the RQUOTA attribute if he doesn't
 * have one already. This function returns the REMAINING quota, not
 * the TOTAL limit.
 * \param who player to check.
 * \return player's remaining quota.
 */
int
get_current_quota(dbref who)
{
  ATTR *a;
  int i;
  int limit;
  int owned = 0;

  /* if he's got an RQUOTA attribute, his remaining quota is that */
  a = atr_get_noparent(Owner(who), "RQUOTA");
  if (a)
    return parse_integer(atr_value(a));

  /* else, count up his objects. If he has less than the START_QUOTA,
   * then his remaining quota is that minus his number of current objects.
   * Otherwise, it's his current number of objects. Add the attribute
   * if he doesn't have it.
   */

  for (i = 0; i < db_top; i++)
    if (Owner(i) == Owner(who))
      owned++;
  owned--;                      /* don't count the player himself */

  if (owned <= START_QUOTA)
    limit = START_QUOTA - owned;
  else
    limit = owned;

  (void) atr_add(Owner(who), "RQUOTA", tprintf("%d", limit), GOD, 0);

  return limit;
}


/** Add or subtract from a player's quota.
 * \param who object whose owner has the quota changed.
 * \param payment amount to add to quota (may be negative).
 */
void
change_quota(dbref who, int payment)
{
  (void) atr_add(Owner(who), "RQUOTA",
                 tprintf("%d", get_current_quota(who) + payment), GOD, 0);
}

/** Debit a player's quota, if they can afford it.
 * \param who player whose quota is to be debitted.
 * \param cost amount of quota to be charged.
 * \retval 1 quota successfully debitted.
 * \retval 0 not enough quota to debit.
 */
static int
pay_quota(dbref who, int cost)
{
  int curr;

  /* figure out how much we have, and if it's big enough */
  curr = get_current_quota(who);

  if (USE_QUOTA && !NoQuota(who) && (curr - cost < 0))  /* not enough */
    return 0;

  change_quota(who, -cost);

  return 1;
}

/** Is a name in the forbidden names file?
 * \param name name to check.
 * \retval 1 name is forbidden.
 * \retval 0 name is not forbidden.
 */
int
forbidden_name(const char *name)
{
  char buf[BUFFER_LEN], *newlin, *ptr;
  FILE *fp;

  fp = fopen(NAMES_FILE, FOPEN_READ);
  if (!fp)
    return 0;
  while (fgets(buf, sizeof buf, fp)) {
    upcasestr(buf);
    /* step on the newline */
    if ((newlin = strchr(buf, '\r')))
      *newlin = '\0';
    else if ((newlin = strchr(buf, '\n')))
      *newlin = '\0';
    ptr = buf;
    if (name && ptr && quick_wild(ptr, name)) {
      fclose(fp);
      return 1;
    }
  }
  fclose(fp);
  return 0;
}

/** Is a name valid for an object?
 * This involves several checks.
 *   Names may not have leading or trailing spaces.
 *   Names must be only printable characters.
 *   Names may not exceed the length limit.
 *   Names may not start with certain tokens, or be "home", "here", or (for non-exits) "me"
 * \param n name to check.
 * \param is_exit is the name for an exit/exit alias?
 * \retval 1 name is valid.
 * \retval 0 name is not valid.
 */
int
ok_name(const char *n, int is_exit)
{
  const unsigned char *p, *name = (const unsigned char *) n;

  if (!name || !*name)
    return 0;

  /* No leading spaces */
  if (isspace((unsigned char) *name))
    return 0;

  /* only printable characters */
  for (p = name; p && *p; p++) {
    if (!isprint((unsigned char) *p))
      return 0;
    if (ONLY_ASCII_NAMES && *p > 127)
      return 0;
    if (strchr("[]%\\=&|", *p))
      return 0;
  }

  /* No trailing spaces */
  p--;
  if (isspace((unsigned char) *p))
    return 0;

  /* Not too long */
  if (u_strlen(name) >= OBJECT_NAME_LIMIT)
    return 0;

  /* No magic cookies */
  return (name
          && *name
          && *name != LOOKUP_TOKEN
          && *name != NUMBER_TOKEN
          && *name != NOT_TOKEN && (is_exit || strcasecmp((char *) name, "me"))
          && strcasecmp((char *) name, "home")
          && strcasecmp((char *) name, "here"));
}

/** Is a name a valid player name when applied by player to thing?
 * Player names must be valid object names, but also not forbidden (unless
 * the player is a wizard). They are
 * subject to a different length limit, and subject to more stringent
 * restrictions on valid characters. Finally, it can't be the same as
 * an existing player name or alias unless it's one of theirs.
 * \param name name to check.
 * \param player player for permission checks.
 * \param thing player who will get the name.
 * \retval 1 name is valid for players.
 * \retval 0 name is not valid for players.
 */
int
ok_player_name(const char *name, dbref player, dbref thing)
{
  const unsigned char *scan, *good;
  dbref lookup;

  if (!ok_name(name, 0) || strlen(name) >= (size_t) PLAYER_NAME_LIMIT)
    return 0;

  good = (unsigned char *) (PLAYER_NAME_SPACES ? " `$_-.,'" : "`$_-.,'");

  /* Make sure that the name contains legal characters only */
  for (scan = (unsigned char *) name; scan && *scan; scan++) {
    if (isalnum((unsigned char) *scan))
      continue;
    if (!strchr((char *) good, *scan))
      return 0;
  }

  lookup = lookup_player(name);

  /* A player may only change to a forbidden name if they're already
     using that name. */
  if (forbidden_name(name)) {
    if (!((GoodObject(player) && Wizard(player))
          || (GoodObject(thing) && lookup == thing))) {
      return 0;
    }
  }

  return ((lookup == NOTHING) || (lookup == thing));
}

/** Is name a valid new name for thing, when set by player?
 * \verbatim
 * Parses names and aliases for players/exits, validating each. If everything is valid,
 * the new name and alias are set into newname and newalias, with memory malloc'd as necessary.
 * For things/rooms, no parsing is done, and ok_name is called on the entire string to validate.
 * For players and exits, if name takes the format <name>; then newname is set to <name> and
 * newalias to ";", to signify that the existing alias should be cleared. If name contains a name and
 * valid aliases, newname and newalias are set accordingly.
 * \endverbatim
 * \param name the new name to set
 * \param player the player setting the name, for permission checks
 * \param thing object getting the name, or NOTHING for new objects
 * \param type type of object getting the name (necessary for new exits)
 * \param newname pointer to place the new name, once validated
 * \param newalias pointer to place the alias in, if any
 * \retval 1 name and any given aliases are valid
 * \retval 0 invalid name
 * \retval OPAE_INVALID invalid aliases
 * \retval OPAE_TOOMANY too many aliases for player
 */
int
ok_object_name(char *name, dbref player, dbref thing, int type, char **newname,
               char **newalias)
{
  char *bon, *eon;
  char nbuff[BUFFER_LEN], abuff[BUFFER_LEN];
  char *ap = abuff;
  int aliases = 0;
  int empty = 0;

  strncpy(nbuff, name, BUFFER_LEN - 1);
  nbuff[BUFFER_LEN - 1] = '\0';
  memset(abuff, 0, BUFFER_LEN);

  /* First, check for a quoted player name */
  if (type == TYPE_PLAYER && *name == '"') {
    /* Quoted player name, no aliases allowed */
    bon = nbuff;
    bon++;
    eon = bon;
    while (*eon && *eon != '"')
      eon++;
    if (*eon)
      *eon = '\0';
    if (!ok_player_name(bon, player, thing))
      return 0;
    *newname = mush_strdup(bon, "name.newname");
    return 1;
  }

  if (type & (TYPE_THING | TYPE_ROOM)) {
    /* No aliases in the name */
    if (!ok_name(nbuff, 0))
      return 0;
    *newname = mush_strdup(nbuff, "name.newname");
    return 1;
  }

  /* A player or exit name, with aliases allowed.
   * Possible things to parse:
   * <name>  - just a new name
   * <name>; - new name with trailing ; to clear alias
   * <name>;<alias1>[;<aliasN>] - name with one or more aliases, separated by ;
   */

  /* Validate name first */
  bon = nbuff;
  if ((eon = strchr(bon, ALIAS_DELIMITER))) {
    *eon++ = '\0';
    aliases++;
  }
  if (!
      (type ==
       TYPE_PLAYER ? ok_player_name(bon, player, thing) : ok_name(bon, 1)))
    return 0;

  *newname = mush_strdup(bon, "name.newname");

  if (aliases) {
    /* We had aliases, so parse them */
    while (eon) {
      if (empty)
        return OPAE_NULL;       /* Null alias only valid as a single, final alias */
      bon = eon;
      if ((eon = strchr(bon, ALIAS_DELIMITER))) {
        *eon++ = '\0';
      }
      while (*bon && *bon == ' ')
        bon++;
      if (!*bon) {
        empty = 1;              /* empty alias, should only happen if we have no proper aliases */
        continue;
      }
      if (!
          (type ==
           TYPE_PLAYER ? ok_player_name(bon, player, thing) : ok_name(bon,
                                                                      1))) {
        *newalias = mush_strdup(bon, "name.newname");   /* So we can report the invalid alias */
        return OPAE_INVALID;
      }
      if (aliases > 1) {
        safe_chr(ALIAS_DELIMITER, abuff, &ap);
      }
      safe_str(bon, abuff, &ap);
      aliases++;
    }
  }
  *ap = '\0';

  if (aliases) {
    if (!Wizard(player) && type == TYPE_PLAYER && aliases > MAX_ALIASES)
      return OPAE_TOOMANY;
    if (*abuff) {
      /* We have actual aliases */
      *newalias = mush_strdup(abuff, "name.newname");
    } else {
      ap = abuff;
      safe_chr(ALIAS_DELIMITER, abuff, &ap);
      *ap = '\0';
      /* We just want to clear the existing alias */
      *newalias = mush_strdup(abuff, "name.newname");
    }
  }

  return 1;
}


/** Is a alias a valid player alias-list for thing?
 * It must be a semicolon-separated list of valid player names
 * with no more than than MAX_ALIASES names, if the player isn't
 * a wizard.
 * \param alias list to check.
 * \param player player for permission checks.
 * \param thing player who is being aliased.
 * \return One of the OPAE_* constants defined in hdrs/attrib.h
 */
int
ok_player_alias(const char *alias, dbref player, dbref thing)
{
  char tbuf1[BUFFER_LEN], *s, *sp;
  int cnt = 0;

  if (!alias || !*alias)
    return OPAE_NULL;

  strncpy(tbuf1, alias, BUFFER_LEN - 1);
  tbuf1[BUFFER_LEN - 1] = '\0';
  s = trim_space_sep(tbuf1, ALIAS_DELIMITER);
  while (s) {
    sp = split_token(&s, ALIAS_DELIMITER);
    while (sp && *sp && *sp == ' ')
      sp++;
    if (!sp || !*sp)
      return OPAE_NULL;         /* No null aliases */
    if (!ok_player_name(sp, player, thing))
      return OPAE_INVALID;
    cnt++;
  }
  if (Wizard(player))
    return OPAE_SUCCESS;
  if (cnt > MAX_ALIASES)
    return OPAE_TOOMANY;
  return OPAE_SUCCESS;
}


/** Is a password acceptable?
 * Acceptable passwords must be non-null and must contain only
 * printable characters and no whitespace.
 * \param password password to check.
 * \retval 1 password is acceptable.
 * \retval 0 password is not acceptable.
 */
int
ok_password(const char *password)
{
  const unsigned char *scan;
  if (password == NULL)
    return 0;

  if (*password == '\0')
    return 0;

  for (scan = (const unsigned char *) password; *scan; scan++) {
    if (!(isprint(*scan) && !isspace(*scan))) {
      return 0;
    }
  }

  return 1;
}

/** Is a name ok for a command?
 * It must contain only uppercase alpha, numbers, or punctuation.
 * It must contain at least one uppercase alpha.
 * It may not begin with " : ; & [ ] \ and # (the special tokens).
 * \param name name to check.
 * \retval 1 name is acceptable.
 * \retval 0 name is not acceptable.
 */
int
ok_command_name(const char *name)
{
  const unsigned char *p;
  int cnt = 0;
  /* First char: uppercase alphanum or legal punctuation */
  switch ((unsigned char) *name) {
  case SAY_TOKEN:
  case POSE_TOKEN:
  case SEMI_POSE_TOKEN:
  case EMIT_TOKEN:
  case NOEVAL_TOKEN:
  case NUMBER_TOKEN:
  case DEBUG_TOKEN:
  case '&':
  case '[':
    return 0;
  default:
    if (!isupper((unsigned char) *name) && !isdigit((unsigned char) *name)
        && !ispunct((unsigned char) *name))
      return 0;
  }
  /* Everything else must be printable and non-space, and we need
   * to find at least one uppercase alpha
   */
  for (p = (unsigned char *) name; p && *p; p++) {
    if (isspace(*p))
      return 0;
    if (isupper(*p))
      cnt++;
  }
  if (!cnt)
    return 0;
  /* Not too long */
  if (strlen(name) >= COMMAND_NAME_LIMIT)
    return 0;
  return 1;
}

/** Is a name ok for a function?
 * It must contain only uppercase alpha, numbers or punctuation, must
 * contain at least one uppercase alpha, and may not begin with
 * " : ; & ] \ or # (the special tokens).
 * \param name name to check.
 * \retval 1 name is acceptable.
 * \retval 0 name is not acceptable.
 */
int
ok_function_name(const char *name)
{
  const unsigned char *p;
  int cnt = 0;
  /* First char: uppercase alpha or legal punctuation */
  switch ((unsigned char) *name) {
  case SAY_TOKEN:
  case POSE_TOKEN:
  case SEMI_POSE_TOKEN:
  case EMIT_TOKEN:
  case NOEVAL_TOKEN:
  case NUMBER_TOKEN:
  case '&':
    return 0;
  }
  /* Everything else must be printable and non-space, and we need
   * to find at least one uppercase alpha
   */
  for (p = (unsigned char *) name; p && *p; p++) {
    if (isspace(*p) || !isprint(*p))
      return 0;
    if (isupper(*p))
      cnt++;
  }
  if (!cnt)
    return 0;
  /* Not too long */
  if (strlen(name) >= COMMAND_NAME_LIMIT)
    return 0;
  return 1;
}

/** Does params contain only acceptable HTML tag attributes?
 * Right now, this means: filter out SEND and XCH_CMD if
 * the player isn't privileged. Params may contain a space-separated
 * list of tag=value pairs. It's probably possible to fool this
 * checking. Needs more work, or removing HTML support.
 * \param player player using the attribute, or NOTHING for internal.
 * \param params the attributes to use.
 * \retval 1 params is acceptable.
 * \retval 0 params is not accpetable.
 */
int
ok_tag_attribute(dbref player, const char *params)
{
  const unsigned char *p, *q;

  if (!GoodObject(player) || Can_Pueblo_Send(player))
    return 1;
  p = (const unsigned char *) params;
  while (*p) {
    while (*p && isspace(*p))
      p++;
    q = p;
    while (*q && *q != '=')
      q++;
    if (*q) {
      size_t n = q - p;
      /* Invalid params for non-priv'd. Turn to a hashtable if we ever
         get more? */
      if (strncasecmp((char *) p, "SEND", n) == 0
          || strncasecmp((char *) p, "XCH_CMD", n) == 0)
        return 0;
      while (*q && isspace(*q))
        q++;
      while (*q && !isspace(*q))
        q++;
      p = q;
    } else
      return 0;                 /* Malformed param without an = */

  }
  return 1;
}


/** The switch command.
 * \verbatim
 * For lack of better place the @switch code is here.
 * @switch expression=args
 * \endverbatim
 * \param executor the executor.
 * \param expression the expression to test against cases.
 * \param argv array of cases and actions.
 * \param enactor the object that caused this code to run.
 * \param first if 1, run only first matching case; if 0, run all matching cases.
 * \param notifyme if 1, perform a notify after executing matched cases.
 * \param regexp if 1, do regular expression matching; if 0, wildcard globbing.
 * \param queue_type the type of queue to run any new commands as
 * \param queue_entry the queue entry \@switch is being run in
 */
void
do_switch(dbref executor, char *expression, char **argv, dbref enactor,
          int first, int notifyme, int regexp, int queue_type,
          MQUE *queue_entry)
{
  int any = 0, a;
  char buff[BUFFER_LEN], *bp;
  char const *ap;
  char *tbuf1;
  PE_REGS *pe_regs;

  if (!argv[1])
    return;

  pe_regs = pe_regs_create(PE_REGS_SWITCH | PE_REGS_CAPTURE, "do_switch");
  pe_regs_clear(pe_regs);
  pe_regs_set(pe_regs, PE_REGS_SWITCH, "t0", expression);

  /* now try a wild card match of buff with stuff in coms */
  for (a = 1;
       !(first && any) && (a < (MAX_ARG - 1)) && argv[a] && argv[a + 1];
       a += 2) {
    /* eval expression */
    ap = argv[a];
    bp = buff;
    if (process_expression(buff, &bp, &ap, executor, enactor, enactor,
                           PE_DEFAULT, PT_DEFAULT, queue_entry->pe_info)) {
      pe_regs_free(pe_regs);
      return;
    }
    *bp = '\0';
    /* check for a match */
    pe_regs_clear_type(pe_regs, PE_REGS_CAPTURE);
    if (regexp ? regexp_match_case_r(buff, expression, 0,
                                     NULL, 0, NULL, 0, pe_regs)
        : local_wild_match(buff, expression, pe_regs)) {
      tbuf1 = replace_string("#$", expression, argv[a + 1]);
      if (!any) {
        /* Add the new switch context to the parent queue... */
        any = 1;
      }
      if (queue_type != QUEUE_DEFAULT) {
        new_queue_actionlist(executor, enactor, enactor, tbuf1, queue_entry,
                             PE_INFO_SHARE, queue_type, pe_regs);
      } else {
        new_queue_actionlist(executor, enactor, enactor, tbuf1, queue_entry,
                             PE_INFO_CLONE, QUEUE_DEFAULT, pe_regs);
      }
      mush_free(tbuf1, "replace_string.buff");
    }
  }
  /* do default if nothing has been matched */
  if ((a < MAX_ARG) && !any && argv[a]) {
    tbuf1 = replace_string("#$", expression, argv[a]);
    if (!any) {
      /* Add the new switch context to the parent queue... */
      any = 1;
    }
    if (queue_type != QUEUE_DEFAULT) {
      new_queue_actionlist(executor, enactor, enactor, tbuf1, queue_entry,
                           PE_INFO_SHARE, queue_type, pe_regs);
    } else {
      new_queue_actionlist(executor, enactor, enactor, tbuf1, queue_entry,
                           PE_INFO_CLONE, QUEUE_DEFAULT, pe_regs);
    }
    mush_free(tbuf1, "replace_string.buff");
  }
  pe_regs_free(pe_regs);
  if (queue_type == QUEUE_DEFAULT) {
    if (notifyme)
      parse_que(executor, enactor, "@notify me", NULL);
  }
}

/** Parse possessive matches for the possessor.
 * This function parses strings of the form "Sam's bag" and attempts
 * to match "Sam". It returns NOTHING if
 * there's no possessive 's in the string. It destructively modifies
 * the string (terminating after the possessor name) and modifies the pointer
 * to the string to point at the name of the contained object.
 * \param player the enactor/looker.
 * \param str a pointer to a string to check for possessive matches.
 * \param exits if true, match for exits, as well as things/players
 * \return matching dbref or NOTHING or AMBIGUOUS.
 */
dbref
parse_match_possessor(dbref player, char **str, int exits)
{
  const char *box;              /* name of container */
  char *obj;                    /* name of object */

  box = *str;

  /* check to see if we have an 's sequence */
  if ((obj = strchr(box, '\'')) == NULL)
    return NOTHING;
  *obj++ = '\0';                /* terminate */
  if ((*obj == '\0') || ((*obj != 's') && (*obj != 'S')))
    return NOTHING;
  /* skip over the 's' and whitespace */
  do {
    obj++;
  } while (isspace((unsigned char) *obj));
  *str = obj;

  /* we already have a terminating null, so we're okay to just do matches */
  return match_result(player, box, NOTYPE,
                      MAT_NEAR_THINGS | MAT_ENGLISH | (exits ? MAT_EXIT : 0));
}


/** Autoreply messages for pages (HAVEN, IDLE, AWAY).
 * \param player the paging player.
 * \param target the paged player.
 * \param type type of message to return.
 * \param message name of attribute containing the message.
 * \param def default message to return.
 */
void
page_return(dbref player, dbref target, const char *type,
            const char *message, const char *def)
{
  char buff[BUFFER_LEN];
  struct tm *ptr;

  if (message && *message) {
    if (call_attrib(target, message, buff, player, NULL, NULL)) {
      if (*buff) {
        ptr = (struct tm *) localtime(&mudtime);
        notify_format(player, T("%s message from %s: %s"), type,
                      Name(target), buff);
        if (!Haven(target))
          notify_format(target,
                        T("[%d:%02d] %s message sent to %s."), ptr->tm_hour,
                        ptr->tm_min, type, Name(player));
      }
    } else if (def && *def)
      notify(player, def);
  }
}

/** Returns the apparent location of object.
 * This is the location for players and things, source for exits, and
 * NOTHING for rooms.
 * \param thing object to get location of.
 * \return apparent location of object (NOTHING for rooms).
 */
dbref
where_is(dbref thing)
{
  if (!GoodObject(thing))
    return NOTHING;
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    return NOTHING;
  case TYPE_EXIT:
    return Home(thing);
  default:
    return Location(thing);
  }
}

/** Are two objects near each other?
 * Returns 1 if obj1 is "nearby" object2. "Nearby" is a commutative
 * relation defined as:
 *   obj1 is in the same room as obj2, obj1 is being carried by
 *   obj2, or obj1 is carrying obj2.
 * Returns 0 if object isn't nearby or the input is invalid.
 * \param obj1 first object.
 * \param obj2 second object.
 * \retval 1 the objects are near each other.
 * \retval 0 the objects are not near each other.
 */
int
nearby(dbref obj1, dbref obj2)
{
  dbref loc1, loc2;

  if (!GoodObject(obj1) || !GoodObject(obj2))
    return 0;
  if (IsRoom(obj1) && IsRoom(obj2))
    return 0;
  loc1 = where_is(obj1);
  if (loc1 == obj2)
    return 1;
  loc2 = where_is(obj2);
  if ((loc2 == obj1) || (loc2 == loc1))
    return 1;
  return 0;
}

/** User-defined verbs.
 * \verbatim
 * This implements the @verb command.
 * \endverbatim
 * \param executor the executor.
 * \param enactor the object causing this command to run.
 * \param arg1 the object to read verb attributes from.
 * \param argv the array of remaining arguments to the verb command.
 */
void
do_verb(dbref executor, dbref enactor, char *arg1, char **argv,
        MQUE *queue_entry)
{
  dbref victim;
  dbref actor;
  int i;
  PE_REGS *pe_regs = NULL;

  /* find the object that we want to read the attributes off
   * (the object that was the victim of the command)
   */

  /* our victim object can be anything */
  victim = match_result(executor, arg1, NOTYPE, MAT_EVERYTHING);

  if (!GoodObject(victim)) {
    notify(executor, T("What was the victim of the verb?"));
    return;
  }
  /* find the object that executes the action */

  if (!argv || !argv[1] || !*argv[1]) {
    notify(executor, T("What do you want to do with the verb?"));
    return;
  }
  actor = match_result(executor, argv[1], NOTYPE, MAT_EVERYTHING);

  if (!GoodObject(actor)) {
    notify(executor, T("What do you want to do the verb?"));
    return;
  }
  /* Control check is fascist.
   * First check: we don't want <actor> to do something involuntarily.
   *   Both victim and actor have to be controlled by the thing which did
   *   the @verb (for speed we do a WIZARD check first), or: cause controls
   *   actor plus the second check is passed.
   * Second check: we need read access to the attributes.
   *   Either the player controls victim or the player
   *   must be priviledged, or the victim has to be VISUAL.
   */

  if (!(Wizard(executor) ||
        (controls(executor, victim) && controls(executor, actor)) ||
        ((controls(enactor, actor) && Can_Examine(executor, victim))))) {
    notify(executor, T("Permission denied."));
    return;
  }
  /* We're okay.  Send out messages. */

  pe_regs = pe_regs_create(PE_REGS_ARG | PE_REGS_Q, "do_verb");
  for (i = 0; i < 10; i++) {
    if (argv[i + 7]) {
      pe_regs_setenv_nocopy(pe_regs, i, argv[i + 7]);
    }
  }
  pe_regs_qcopy(pe_regs, queue_entry->pe_info->regvals);

  real_did_it(actor, victim,
              upcasestr(argv[2]), argv[3], upcasestr(argv[4]), argv[5],
              NULL, Location(actor), pe_regs, NA_INTER_HEAR);

  /* Now we copy our args into the stack, and do the command. */

  if (argv[6] && *argv[6])
    queue_attribute_base(victim, upcasestr(argv[6]), actor, 0, pe_regs, 0);

  pe_regs_free(pe_regs);
}

/** Helper data for regexp \@greps */
struct regrep_data {
  pcre *re;             /**< Pointer to compiled regular expression */
  pcre_extra *study;    /**< Pointer to studied data about re */
  char *buff;           /**< Buffer to store regrep results, or NULL to report to player */
  char **bp;            /**< Pointer to address of insertion point in buff, or NULL */
  int count;            /**< Number of matches found */
};

/** Helper data for non-regexp \@greps */
struct grep_data {
  char *findstr;        /**< String to find */
  int findlen;          /**< Length of findstr */
  char *buff;           /**< Buffer to store regrep results, or NULL to report to player */
  char **bp;            /**< Pointer to address of insertion point in buff, or NULL */
  int count;            /**< Number of matches found */
  int flags;            /**< Type of grep: wildcard, case-sensitive */
};

static void
grep_add_attr(char *buff, char **bp, dbref player, int count, ATTR *attr,
              char *atrval)
{

  if (buff) {
    if (count)
      safe_chr(' ', buff, bp);
    safe_str(AL_NAME(attr), buff, bp);
  } else {
    notify_format(player, "%s%s [#%d%s]:%s %s",
                  ANSI_HILITE, AL_NAME(attr),
                  Owner(AL_CREATOR(attr)),
                  privs_to_letters(attr_privs_view, AL_FLAGS(attr)),
                  ANSI_END, atrval);
  }
}

extern const unsigned char *tables;

static int
grep_helper(dbref player, dbref thing __attribute__ ((__unused__)),
            dbref parent __attribute__ ((__unused__)),
            char const *pattern
            __attribute__ ((__unused__)), ATTR *attr, void *args)
{
  struct grep_data *gd = args;
  char *s;
  char b1[BUFFER_LEN], b2[BUFFER_LEN];
  char *tp = b1;
  int matched = 0;
  int cs;

  cs = ((gd->flags & GREP_NOCASE) == 0);
  s = (char *) atr_value(attr); /* warning: static */
  if (gd->flags & GREP_WILD) {
    if ((matched = quick_wild_new(gd->findstr, s, cs))) {
      /* Since, in order for a wildcard match to succeed, the _entire
         attribute_ value had to match the pattern, not just a substring,
         highlighting is totally pointless */
      strcpy(b1, s);
    }
  } else {
    while (s && *s) {
      if (!
          (cs ? strncmp(s, gd->findstr, gd->findlen) :
           strncasecmp(s, gd->findstr, gd->findlen))) {
        matched = 1;
        strncpy(b2, s, gd->findlen);
        b2[gd->findlen] = '\0';
        s += gd->findlen;
        safe_format(b1, &tp, "%s%s%s", ANSI_HILITE, b2, ANSI_END);
      } else {
        safe_chr(*s, b1, &tp);
        s++;
      }
    }
    *tp = '\0';
  }

  if (!matched)
    return 0;

  grep_add_attr(gd->buff, gd->bp, player, gd->count, attr, b1);
  gd->count++;
  return 1;
}

static int
regrep_helper(dbref player, dbref thing __attribute__ ((__unused__)),
              dbref parent __attribute__ ((__unused__)),
              char const *pattern
              __attribute__ ((__unused__)), ATTR *attr, void *args)
{
  struct regrep_data *rgd = args;
  char *s;
  int offsets[99];
  int subpatterns, search = 0;
  ansi_string *orig, *repl;
  char rbuff[BUFFER_LEN];
  char *rbp = rbuff;

  s = atr_value(attr);
  orig = parse_ansi_string(s);
  if ((subpatterns =
       pcre_exec(rgd->re, rgd->study, orig->text, orig->len, search, 0, offsets,
                 99))
      < 0) {
    free_ansi_string(orig);
    return 0;
  }
  while (subpatterns >= 0) {
    safe_str(ANSI_HILITE, rbuff, &rbp);
    ansi_pcre_copy_substring(orig, offsets, subpatterns, 0, 0, rbuff, &rbp);
    safe_str(ANSI_END, rbuff, &rbp);
    *rbp = '\0';
    if (offsets[0] >= search) {
      repl = parse_ansi_string(rbuff);

      /* Do the replacement */
      ansi_string_replace(orig, offsets[0], offsets[1] - offsets[0], repl);

      /* Advance search */
      if (search == offsets[1]) {
        search = offsets[0] + repl->len;
        search++;
      } else {
        search = offsets[0] + repl->len;
      }

      free_ansi_string(repl);
      rbp = rbuff;
      if (search >= orig->len)
        break;
      subpatterns =
        pcre_exec(rgd->re, rgd->study, orig->text, orig->len, search, 0,
                  offsets, 99);
    }
  }
  safe_ansi_string(orig, 0, orig->len, rbuff, &rbp);
  *rbp = '\0';
  free_ansi_string(orig);
  grep_add_attr(rgd->buff, rgd->bp, player, rgd->count, attr, rbuff);
  rgd->count++;
  return 1;
}

int
grep_util(dbref player, dbref thing, char *attrs, char *findstr, char *buff,
          char **bp, int flags)
{
  if (!findstr || !*findstr) {
    if (buff)
      safe_str(T("#-1 INVALID GREP PATTERN"), buff, bp);
    else
      notify(player, T("What pattern do you want to grep for?"));
    return 0;
  }

  if (!attrs || !*attrs)
    attrs = "**";

  if (flags & GREP_REGEXP) {
    /* regexp grep */
    struct regrep_data rgd;
    const char *errptr;
    int erroffset;
    int reflags = 0;
    bool free_study = false;

    if (flags & GREP_NOCASE)
      reflags |= PCRE_CASELESS;

    if ((rgd.re = pcre_compile(findstr, reflags,
                               &errptr, &erroffset, tables)) == NULL) {
      /* Matching error. */
      if (buff) {
        safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
        safe_str(errptr, buff, bp);
      } else {
        notify_format(player, T("Invalid regexp: %s"), errptr);
      }
      return 0;
    }
    add_check("pcre");
    rgd.study = pcre_study(rgd.re, 0, &errptr);
    if (errptr != NULL) {
      if (buff) {
        safe_str(T("#-1 REGEXP ERROR: "), buff, bp);
        safe_str(errptr, buff, bp);
      } else {
        notify_format(player, T("Invalid regexp: %s"), errptr);
      }
      mush_free(rgd.re, "pcre");
      return 0;
    }
    if (rgd.study) {
      add_check("pcre.extra");
      free_study = true;
      set_match_limit(rgd.study);
    } else {
      rgd.study = default_match_limit();
    }
    rgd.buff = buff;
    rgd.bp = bp;
    rgd.count = 0;

    atr_iter_get(player, thing, attrs, 0, 0, regrep_helper, (void *) &rgd);
    if (free_study)
      mush_free(rgd.study, "pcre.extra");
    mush_free(rgd.re, "pcre");

    return rgd.count;
  } else {
    /* Wildcard or plain substring grep */
    struct grep_data gd;
    gd.findstr = findstr;
    gd.findlen = strlen(findstr);
    gd.buff = buff;
    gd.bp = bp;
    gd.count = 0;
    gd.flags = flags;

    atr_iter_get(player, thing, attrs, 0, 0, grep_helper, (void *) &gd);

    return gd.count;
  }

}

/** The grep command
 * \verbatim
 * This implements @grep.
 * \endverbatim
 * \param player the enactor.
 * \param obj string containing obj/attr pattern to grep through.
 * \param lookfor unparsed string to search for.
 * \param print if 0, show attribute names; if 1, show attrib text.
 * \param flags type of grep: wild, regexp, nocase
 */
void
do_grep(dbref player, char *obj, char *lookfor, int print, int flags)
{
  dbref thing;
  char *pattern;

  if (!lookfor || !*lookfor) {
    notify(player, T("What pattern do you want to grep for?"));
    return;
  }
  /* find the attribute pattern */
  pattern = strchr(obj, '/');
  if (!pattern)
    pattern = (char *) "*";     /* set it to global match */
  else
    *pattern++ = '\0';

  /* now we've got the object. match for it. */
  if ((thing = noisy_match_result(player, obj, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING)
    return;

  if (print) {
    if (!grep_util(player, thing, pattern, lookfor, NULL, NULL, flags))
      notify(player, T("No matches."));
  } else {
    char buff[BUFFER_LEN];
    char *bp = buff;

    if (grep_util(player, thing, pattern, lookfor, buff, &bp, flags)) {
      *bp = '\0';
      notify_format(player, T("Matches of '%s' on %s(#%d): %s"), lookfor,
                    Name(thing), thing, buff);
    } else
      notify(player, T("No matches."));
  }
}
