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
void do_switch(dbref player, char *expression, char **argv,
               dbref cause, int first, int notifyme, int regexp);
void do_verb(dbref player, dbref cause, char *arg1, char **argv);
static int grep_util_helper(dbref player, dbref thing, dbref parent,
                            char const *pattern, ATTR *atr, void *args);
static int grep_helper(dbref player, dbref thing, dbref parent,
                       char const *pattern, ATTR *atr, void *args);
void do_grep(dbref player, char *obj, char *lookfor, int flag, int insensitive);
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
#ifdef HAS_VSNPRINTF
  static char buff[BUFFER_LEN];
#else
  static char buff[BUFFER_LEN * 3];     /* safety margin */
#endif
  va_list args;

  va_start(args, fmt);

#ifdef HAS_VSNPRINTF
  vsnprintf(buff, sizeof buff, fmt, args);
#else
  vsprintf(buff, fmt, args);
#endif

  buff[BUFFER_LEN - 1] = '\0';
  va_end(args);
  return (buff);
}

/** lock evaluation -- determines if player passes lock on thing, for
 * the purposes of picking up an object or moving through an exit.
 * \param player to check against lock.
 * \param thing thing to check the basic lock on.
 * \retval 1 player passes lock.
 * \retval 0 player fails lock.
 */
int
could_doit(dbref player, dbref thing)
{
  if (!IsRoom(thing) && Location(thing) == NOTHING)
    return 0;
  return (eval_lock(player, thing, Basic_Lock));
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
  /* Bunch o' nulls */
  static char *myenv[10] = { NULL };
  return real_did_it(player, thing, what, def, owhat, odef, awhat, loc, myenv,
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
  char *myenv[10] = { NULL };
  char e0[SBUF_LEN], e1[SBUF_LEN], *ep;

  if (env0 != NOTHING) {
    ep = e0;
    safe_dbref(env0, e0, &ep);
    *ep = '\0';
    myenv[0] = e0;
  }

  if (env1 != NOTHING) {
    ep = e1;
    safe_dbref(env1, e1, &ep);
    *ep = '\0';
    myenv[1] = e1;
  }

  return real_did_it(player, thing, what, def, owhat, odef, awhat, loc, myenv,
                     flags);
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
  static char *myenv[10] = { NULL };
  return real_did_it(player, thing, what, def, owhat, odef, awhat, loc, myenv,
                     flags);
}

/** Take an action on an object and trigger attributes.
 * \verbatim
 * executes the @attr, @oattr, @aattr for a command - gives a message
 * to the enactor and others in the room with the enactor, and executes
 * an action. We load wenv with the values in myenv.
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
 * \param myenv copy of the environment.
 * \param flags flags controlling type of interaction involved.
 * \retval 0 no attributes were present, only defaults were used if given.
 * \retval 1 some attributes were evaluated and used.
 */
int
real_did_it(dbref player, dbref thing, const char *what, const char *def,
            const char *owhat, const char *odef, const char *awhat, dbref loc,
            char *myenv[10], int flags)
{

  ATTR *d;
  char buff[BUFFER_LEN], *bp, *sp, *asave;
  char const *ap;
  int j;
  char *preserves[10];
  char *preserveq[NUMQ];
  dbref preserve_orator = orator;
  int need_pres = 0;
  int attribs_used = 0;

  loc = (loc == NOTHING) ? Location(player) : loc;
  orator = player;
  /* only give messages if the location is good */
  if (GoodObject(loc)) {

    /* message to player */
    if (what && *what) {
      d = atr_get(thing, what);
      if (d) {
        attribs_used = 1;
        if (!need_pres) {
          need_pres = 1;
          save_global_regs("did_it_save", preserveq);
          save_global_env("did_it_save", preserves);
        }
        restore_global_env("did_it", myenv);
        asave = safe_atr_value(d);
        ap = asave;
        bp = buff;
        process_expression(buff, &bp, &ap, thing, player, player,
                           PE_DEFAULT, PT_DEFAULT, NULL);
        *bp = '\0';
        notify_by(thing, player, buff);
        free(asave);
      } else if (def && *def)
        notify_by(thing, player, def);
    }
    /* message to neighbors */
    if (!DarkLegal(player)) {
      if (owhat && *owhat) {
        d = atr_get(thing, owhat);
        if (d) {
          attribs_used = 1;
          if (!need_pres) {
            need_pres = 1;
            save_global_regs("did_it_save", preserveq);
            save_global_env("did_it_save", preserves);
          }
          restore_global_env("did_it", myenv);
          asave = safe_atr_value(d);
          ap = asave;
          bp = buff;
          if (!((d)->flags & AF_NONAME)) {
            safe_str(Name(player), buff, &bp);
            if (!((d)->flags & AF_NOSPACE))
              safe_chr(' ', buff, &bp);
          }
          sp = bp;
          process_expression(buff, &bp, &ap, thing, player, player,
                             PE_DEFAULT, PT_DEFAULT, NULL);
          *bp = '\0';
          if (bp != sp)
            notify_except2(Contents(loc), player, thing, buff, flags);
          free(asave);
        } else {
          if (odef && *odef) {
            notify_except2(Contents(loc), player, thing,
                           tprintf("%s %s", Name(player), odef), flags);
          }
        }
      }
    }
  }
  if (need_pres) {
    restore_global_regs("did_it_save", preserveq);
    restore_global_env("did_it_save", preserves);
  }
  for (j = 0; j < 10; j++)
    global_eval_context.wnxt[j] = myenv[j];
  for (j = 0; j < NUMQ; j++)
    global_eval_context.rnxt[j] = NULL;
  if (awhat && *awhat)
    attribs_used = queue_attribute(thing, awhat, player) || attribs_used;
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
    if (can_interact(thing, player, INTERACT_SEE)) {
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
  if (!can_interact(thing, player, INTERACT_SEE))
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
    if (eval_boolexp(who, c, what))
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
  char *upname;

  upname = strupper(name);
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
 *   Names may not start with certain tokens, or be "me", "home", "here"
 * \param n name to check.
 * \retval 1 name is valid.
 * \retval 0 name is not valid.
 */
int
ok_name(const char *n)
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
          && *name != NOT_TOKEN && strcasecmp((char *) name, "me")
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

  if (!ok_name(name) || strlen(name) >= (size_t) PLAYER_NAME_LIMIT)
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
  if (forbidden_name(name) && !((lookup == thing) ||
                                (GoodObject(player) && Wizard(player))))
    return 0;

  return ((lookup == NOTHING) || (lookup == thing));
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
 * It may not begin with " : ; & ] \ and # (the special tokens).
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
  case '&':
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
 * It must start with uppercase alpha or punctuation and may contain only
 * uppercase alpha, numbers, or punctuation thereafter.
 * It must contain at least one uppercase alpha.
 * It may not begin with " : ; & ] \ and # (the special tokens).
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
  default:
    if (!isupper((unsigned char) *name) && !ispunct((unsigned char) *name))
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
 * \param player the enactor.
 * \param expression the expression to test against cases.
 * \param argv array of cases and actions.
 * \param cause the object that caused this code to run.
 * \param first if 1, run only first matching case; if 0, run all matching cases.
 * \param notifyme if 1, perform a notify after executing matched cases.
 * \param regexp if 1, do regular expression matching; if 0, wildcard globbing.
 */
void
do_switch(dbref player, char *expression, char **argv, dbref cause,
          int first, int notifyme, int regexp)
{
  int any = 0, a;
  char buff[BUFFER_LEN], *bp;
  char const *ap;
  char *tbuf1;

  if (!argv[1])
    return;

  /* set up environment for any spawned commands */
  for (a = 0; a < 10; a++)
    global_eval_context.wnxt[a] = global_eval_context.wenv[a];
  for (a = 0; a < NUMQ; a++)
    global_eval_context.rnxt[a] = global_eval_context.renv[a];

  /* now try a wild card match of buff with stuff in coms */
  for (a = 1;
       !(first && any) && (a < (MAX_ARG - 1)) && argv[a] && argv[a + 1];
       a += 2) {
    /* eval expression */
    ap = argv[a];
    bp = buff;
    process_expression(buff, &bp, &ap, player, cause, cause,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';

    /* check for a match */
    if (regexp ? quick_regexp_match(buff, expression, 0)
        : local_wild_match(buff, expression)) {
      any = 1;
      tbuf1 = replace_string("#$", expression, argv[a + 1]);
      parse_que(player, tbuf1, cause);
      mush_free(tbuf1, "replace_string.buff");
    }
  }

  /* do default if nothing has been matched */
  if ((a < MAX_ARG) && !any && argv[a]) {
    tbuf1 = replace_string("#$", expression, argv[a]);
    parse_que(player, tbuf1, cause);
    mush_free(tbuf1, "replace_string.buff");
  }

  /* Pop on @notify me, if requested */
  if (notifyme)
    parse_que(player, "@notify me", cause);
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
                      MAT_NEIGHBOR | MAT_POSSESSION | MAT_ENGLISH | (exits ?
                                                                     MAT_EXIT :
                                                                     0));
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
  ATTR *d;
  char buff[BUFFER_LEN], *bp, *asave;
  char const *ap;
  struct tm *ptr;

  if (message && *message) {
    d = atr_get(target, message);
    if (d) {
      asave = safe_atr_value(d);
      ap = asave;
      bp = buff;
      process_expression(buff, &bp, &ap, target, player, player,
                         PE_DEFAULT, PT_DEFAULT, NULL);
      *bp = '\0';
      free(asave);
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
 * \param player the enactor.
 * \param cause the object causing this command to run.
 * \param arg1 the object to read verb attributes from.
 * \param argv the array of remaining arguments to the verb command.
 */
void
do_verb(dbref player, dbref cause, char *arg1, char **argv)
{
  dbref victim;
  dbref actor;
  int i;
  char *wsave[10];

  /* find the object that we want to read the attributes off
   * (the object that was the victim of the command)
   */

  /* our victim object can be anything */
  victim = match_result(player, arg1, NOTYPE, MAT_EVERYTHING);

  if (!GoodObject(victim)) {
    notify(player, T("What was the victim of the verb?"));
    return;
  }
  /* find the object that executes the action */

  if (!argv || !argv[1] || !*argv[1]) {
    notify(player, T("What do you want to do with the verb?"));
    return;
  }
  actor = match_result(player, argv[1], NOTYPE, MAT_EVERYTHING);

  if (!GoodObject(actor)) {
    notify(player, T("What do you want to do the verb?"));
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

  if (!(Wizard(player) ||
        (controls(player, victim) && controls(player, actor)) ||
        ((controls(cause, actor) && Can_Examine(player, victim))))) {
    notify(player, T("Permission denied."));
    return;
  }
  /* We're okay.  Send out messages. */

  for (i = 0; i < 10; i++) {
    wsave[i] = global_eval_context.wenv[i];
    global_eval_context.wenv[i] = argv[i + 7];
  }

  real_did_it(actor, victim,
              upcasestr(argv[2]), argv[3], upcasestr(argv[4]), argv[5],
              NULL, Location(actor), global_eval_context.wenv, NA_INTER_HEAR);

  for (i = 0; i < 10; i++)
    global_eval_context.wenv[i] = wsave[i];

  /* Now we copy our args into the stack, and do the command. */

  for (i = 0; i < 10; i++)
    global_eval_context.wnxt[i] = argv[i + 7];

  if (argv[6] && *argv[6])
    queue_attribute(victim, upcasestr(argv[6]), actor);
}

/** Structure for passing arguments to grep_util_helper().
 */
struct guh_args {
  char *buff;           /**< Buffer for output */
  char *bp;             /**< Pointer to buff's current position */
  char *lookfor;        /**< Pattern to grep for */
  int sensitive;        /**< If 1, case-sensitive match; if 0, insensitive */
};

static int
grep_util_helper(dbref player __attribute__ ((__unused__)),
                 dbref thing __attribute__ ((__unused__)),
                 dbref parent __attribute__ ((__unused__)),
                 char const *pattern
                 __attribute__ ((__unused__)), ATTR *atr, void *args)
{
  struct guh_args *guh = args;
  int found = 0;
  char *s;
  int len;

  s = (char *) atr_value(atr);  /* warning: static */
  len = strlen(guh->lookfor);
  found = 0;
  while (*s && !found) {
    if ((guh->sensitive && !strncmp(guh->lookfor, s, len)) ||
        (!guh->sensitive && !strncasecmp(guh->lookfor, s, len)))
      found = 1;
    else
      s++;
  }
  if (found) {
    if (guh->bp != guh->buff)
      safe_chr(' ', guh->buff, &guh->bp);
    safe_str(AL_NAME(atr), guh->buff, &guh->bp);
  }
  return found;
}

static int
wildgrep_util_helper(dbref player __attribute__ ((__unused__)),
                     dbref thing __attribute__ ((__unused__)),
                     dbref parent __attribute__ ((__unused__)),
                     char const *pattern
                     __attribute__ ((__unused__)), ATTR *atr, void *args)
{
  struct guh_args *guh = args;
  int found = 0;

  if (quick_wild_new(guh->lookfor, atr_value(atr), guh->sensitive)) {
    if (guh->bp != guh->buff)
      safe_chr(' ', guh->buff, &guh->bp);
    safe_str(AL_NAME(atr), guh->buff, &guh->bp);
    found = 1;
  }
  return found;
}

/** Utility function for grep funtions/commands.
 * This function returns a list of attributes on an object that
 * match a name pattern and contain another string.
 * \param player the enactor.
 * \param thing object to check attributes on.
 * \param pattern wildcard or substring pattern for attributes to check.
 * \param lookfor string to find within each attribute.
 * \param sensitive if 1, case-sensitive matching; if 0, case-insensitive.
 * \param wild if 1, wildcard matching, if 0, substring
 * \return string containing list of attribute names with matching data.
 */
char *
grep_util(dbref player, dbref thing, char *pattern, char *lookfor,
          int sensitive, int wild)
{
  struct guh_args guh;

  guh.buff = (char *) mush_malloc(BUFFER_LEN + 1, "grep_util.buff");
  guh.bp = guh.buff;
  guh.lookfor = lookfor;
  guh.sensitive = sensitive;
  (void) atr_iter_get(player, thing, pattern, 0, 0, wild ? wildgrep_util_helper
                      : grep_util_helper, &guh);
  *guh.bp = '\0';
  return guh.buff;
}

/** Structure for grep_helper() arguments. */
struct gh_args {
  char *lookfor;        /**< Pattern to look for. */
  int len;              /**< Length of lookfor. */
  int insensitive;      /**< if 1, case-insensitive matching; if 0, sensitive */
};

static int
grep_helper(dbref player, dbref thing __attribute__ ((__unused__)),
            dbref parent __attribute__ ((__unused__)),
            char const *pattern
            __attribute__ ((__unused__)), ATTR *atr, void *args)
{
  struct gh_args *gh = args;
  int found;
  char *s;
  char tbuf1[BUFFER_LEN];
  char buf[BUFFER_LEN];
  char *tbp;

  found = 0;
  tbp = tbuf1;

  s = (char *) atr_value(atr);  /* warning: static */
  while (s && *s) {
    if ((gh->insensitive && !strncasecmp(gh->lookfor, s, gh->len)) ||
        (!gh->insensitive && !strncmp(gh->lookfor, s, gh->len))) {
      found = 1;
      strncpy(buf, s, gh->len);
      buf[gh->len] = '\0';
      s += gh->len;
      safe_format(tbuf1, &tbp, "%s%s%s", ANSI_HILITE, buf, ANSI_END);
    } else {
      safe_chr(*s, tbuf1, &tbp);
      s++;
    }
  }
  *tbp = '\0';

  /* if we got it, display it */
  if (found)
    notify_format(player, "%s%s [#%d%s]:%s %s",
                  ANSI_HILITE, AL_NAME(atr),
                  Owner(AL_CREATOR(atr)),
                  privs_to_letters(attr_privs_view, AL_FLAGS(atr)),
                  ANSI_END, tbuf1);
  return found;
}

/** The grep command
 * \verbatim
 * This implements @grep.
 * \endverbatim
 * \param player the enactor.
 * \param obj string containing obj/attr pattern to grep through.
 * \param lookfor unparsed string to search for.
 * \param flag if 0, return attribute names; if 1, return attrib text.
 * \param insensitive if 1, case-insensitive match; if 0, sensitive.
 */
void
do_grep(dbref player, char *obj, char *lookfor, int flag, int insensitive)
{
  struct gh_args gh;
  dbref thing;
  char *pattern;
  int len;
  char *tp;

  if ((len = strlen(lookfor)) < 1) {
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
  if (!Can_Examine(player, thing)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (flag) {
    gh.lookfor = lookfor;
    gh.len = len;
    gh.insensitive = insensitive;
    if (!atr_iter_get(player, thing, pattern, 0, 0, grep_helper, &gh))
      notify(player, T("No matching attributes."));
  } else {
    tp = grep_util(player, thing, pattern, lookfor, !insensitive, 0);
    notify_format(player, T("Matches of '%s' on %s(#%d): %s"), lookfor,
                  Name(thing), thing, tp);
    mush_free(tp, "grep_util.buff");
  }
}
