/**
 * \file set.c
 *
 * \brief PennMUSH commands that set parameters.
 *
 *
 */

#include "copyrite.h"
#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "match.h"
#include "attrib.h"
#include "ansi.h"
#include "command.h"
#include "mymalloc.h"
#include "flags.h"
#include "dbdefs.h"
#include "lock.h"
#include "log.h"
#include "game.h"
#include "confmagic.h"

static int chown_ok(dbref player, dbref thing, dbref newowner,
                    NEW_PE_INFO *pe_info);
void do_attrib_flags(dbref player, const char *obj, const char *atrname,
                     const char *flag);
static int af_helper(dbref player, dbref thing, dbref parent,
                     char const *pattern, ATTR *atr, void *args);
static int gedit_helper(dbref player, dbref thing, dbref parent,
                        char const *pattern, ATTR *atr, void *args);
static int wipe_helper(dbref player, dbref thing, dbref parent,
                       char const *pattern, ATTR *atr, void *args);
static void copy_attrib_flags(dbref player, dbref target, ATTR *atr, int flags);

extern int rhs_present;         /* from command.c */


/** Rename something.
 * \verbatim
 * This implements @name.
 * \endverbatim
 * \param player the enactor.
 * \param name current name of object to rename.
 * \param newname_ new name for object.
 */
void
do_name(dbref player, const char *name, char *newname_)
{
  dbref thing;
  char *myenv[10];
  int i;
  char *newname = NULL;
  char *alias = NULL;
  PE_REGS *pe_regs;

  if ((thing = match_controlled(player, name)) == NOTHING)
    return;

  /* check for bad name */
  if ((*newname_ == '\0') || strchr(newname_, '[')) {
    notify(player, T("Give it what new name?"));
    return;
  }
  switch (Typeof(thing)) {
  case TYPE_PLAYER:
    switch (ok_object_name
            (newname_, player, thing, TYPE_PLAYER, &newname, &alias)) {
    case 0:
      notify(player, T("You can't give a player that name."));
      return;
    case OPAE_TOOMANY:
      notify(player, T("Too many aliases."));
      mush_free(newname, "name.newname");
      return;
    case OPAE_INVALID:
      notify_format(player, T("'%s' is not a valid alias."), alias);
      mush_free(newname, "name.newname");
      mush_free(alias, "name.newname");
      return;
    }
    break;
  case TYPE_EXIT:
    if (ok_object_name(newname_, player, thing, TYPE_EXIT, &newname, &alias) <
        1) {
      notify(player, T("That is not a reasonable name."));
      if (newname)
        mush_free(newname, "name.newname");
      if (alias)
        mush_free(alias, "name.newname");
      return;
    }
    break;
  case TYPE_THING:
  case TYPE_ROOM:
    if (!ok_name(newname_, 0)) {
      notify(player, T("That is not a reasonable name."));
      return;
    }
    newname = mush_strdup(trim_space_sep(newname_, ' '), "name.newname");
    break;
  default:
    /* Should never occur */
    notify(player, T("I don't see that here."));
    return;
  }

  /* Actually change it */
  myenv[0] = (char *) mush_malloc(BUFFER_LEN, "string");
  myenv[1] = (char *) mush_malloc(BUFFER_LEN, "string");
  mush_strncpy(myenv[0], Name(thing), BUFFER_LEN);
  strcpy(myenv[1], newname);
  for (i = 2; i < 10; i++)
    myenv[i] = NULL;

  if (IsPlayer(thing)) {
    do_log(LT_CONN, 0, 0, "Name change by %s(#%d) to %s",
           Name(thing), thing, newname);
    if (Suspect(thing))
      flag_broadcast("WIZARD", 0,
                     T("Broadcast: Suspect %s changed name to %s."),
                     Name(thing), newname);
    reset_player_list(thing, Name(thing), NULL, newname, NULL);
  }
  set_name(thing, newname);
  if (alias) {
    if (*alias == ALIAS_DELIMITER) {
      do_set_atr(thing, "ALIAS", NULL, player, 0);
    } else {
      /* New alias to set */
      do_set_atr(thing, "ALIAS", alias, player, 0);
    }
    mush_free(alias, "name.newname");
  }

  queue_event(player, "OBJECT`RENAME", "%s,%s,%s",
              unparse_objid(thing), myenv[1], myenv[0]);

  if (!AreQuiet(player, thing))
    notify(player, T("Name set."));
  pe_regs = pe_regs_create(PE_REGS_ARG, "do_name");
  pe_regs_setenv_nocopy(pe_regs, 0, myenv[0]);
  pe_regs_setenv_nocopy(pe_regs, 1, myenv[1]);
  real_did_it(player, thing, NULL, NULL, "ONAME", NULL, "ANAME", NOTHING,
              pe_regs, NA_INTER_PRESENCE);
  pe_regs_free(pe_regs);
  mush_free(newname, "name.newname");
  mush_free(myenv[0], "string");
  mush_free(myenv[1], "string");

}

/** Change an object's owner.
 * \verbatim
 * This implements @chown.
 * \endverbatim
 * \param player the enactor.
 * \param name name of object to change owner of.
 * \param newobj name of new owner for object.
 * \param preserve if 1, preserve privileges and don't halt the object.
 * \param pe_info the pe_info for lock checks
 */
void
do_chown(dbref player, const char *name, const char *newobj, int preserve,
         NEW_PE_INFO *pe_info)
{
  dbref thing;
  dbref newowner = NOTHING;
  long match_flags = MAT_POSSESSION | MAT_HERE | MAT_EXIT | MAT_ABSOLUTE;


  /* check for '@chown <object>/<atr>=<player>'  */
  if (strchr(name, '/')) {
    do_atrchown(player, name, newobj);
    return;
  }
  if (Wizard(player))
    match_flags |= MAT_PLAYER;

  if ((thing = noisy_match_result(player, name, TYPE_THING, match_flags))
      == NOTHING)
    return;

  if (!*newobj || !strcasecmp(newobj, "me")) {
    newowner = player;
  } else {
    if ((newowner = lookup_player(newobj)) == NOTHING) {
      notify(player, T("I couldn't find that player."));
      return;
    }
  }

  if (IsPlayer(thing) && !God(player)) {
    notify(player, T("Players always own themselves."));
    return;
  }
  /* Permissions checking */
  if (!chown_ok(player, thing, newowner, pe_info)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (IsThing(thing) && !Hasprivs(player) &&
      !(GoodObject(Location(thing)) && (Location(thing) == player))) {
    notify(player, T("You must carry the object to @chown it."));
    return;
  }
  if (preserve && !Wizard(player)) {
    notify(player, T("You cannot @CHOWN/PRESERVE. Use normal @CHOWN."));
    return;
  }
  /* chowns to the zone master don't count towards fees */
  if (!ZMaster(newowner)) {
    /* Debit the owner-to-be */
    if (!can_pay_fees(newowner, Pennies(thing))) {
      /* not enough money or quota */
      if (newowner != player)
        notify(player,
               T
               ("That player doesn't have enough money or quota to receive that object."));
      return;
    }
    /* Credit the current owner */
    giveto(Owner(thing), Pennies(thing));
    change_quota(Owner(thing), QUOTA_COST);
  }
  chown_object(player, thing, newowner, preserve);
  notify(player, T("Owner changed."));
}

static int
chown_ok(dbref player, dbref thing, dbref newowner, NEW_PE_INFO *pe_info)
{
  /* Can't touch garbage */
  if (IsGarbage(thing))
    return 0;

  /* Wizards can do it all */
  if (Wizard(player))
    return 1;

  /* In order for non-wiz player to @chown thing to newowner,
   * player must control newowner or newowner must be a Zone Master
   * and player must pass its zone lock.
   *
   * In addition, one of the following must apply:
   *   1.  player owns thing, or
   *   2.  player controls Owner(thing), newowner is a zone master,
   *       and Owner(thing) passes newowner's zone-lock, or
   *   3.  thing is CHOWN_OK, and player holds thing if it's an object.
   *
   * The third condition is syntactic sugar to handle the situation
   * where Joe owns Box, an ordinary object, and Tool, an inherit object,
   * and ZMP, a Zone Master Player, is zone-locked to =tool.
   * In this case, if Joe doesn't pass ZMP's lock, we don't want
   *   Joe to be able to @fo Tool=@chown Box=ZMP
   */

  /* Does player control newowner, or is newowner a Zone Master and player
   * passes the lock?
   */
  if (!(controls(player, newowner) || (ZMaster(newowner)
                                       && eval_lock_with(player, newowner,
                                                         Zone_Lock, pe_info))))
    return 0;

  /* Target player is legitimate. Does player control the object? */
  if (Owns(player, thing))
    return 1;

  if (controls(player, Owner(thing)) && ZMaster(newowner)
      && eval_lock_with(Owner(thing), newowner, Zone_Lock, pe_info))
    return 1;

  if (ChownOk(thing) && (!IsThing(thing) || (Location(thing) == player)))
    return 1;

  return 0;
}


/** Actually change the ownership of something, and fix bits.
 * \param player the enactor.
 * \param thing object to change ownership of.
 * \param newowner new owner for thing.
 * \param preserve if 1, preserve privileges and don't halt.
 */
void
chown_object(dbref player, dbref thing, dbref newowner, int preserve)
{
  (void) undestroy(player, thing);
  if (God(player)) {
    Owner(thing) = newowner;
  } else {
    Owner(thing) = Owner(newowner);
  }
  /* Don't allow circular zones */
  Zone(thing) = NOTHING;
  if (GoodObject(Zone(newowner))) {
    dbref tmp;
    int ok_to_zone = 1;
    int zone_depth = MAX_ZONES;
    for (tmp = Zone(Zone(newowner)); GoodObject(tmp); tmp = Zone(tmp)) {
      if (tmp == thing) {
        notify(player, T("Circular zone broken."));
        ok_to_zone = 0;
        break;
      }
      if (tmp == Zone(tmp))     /* Ran into an object zoned to itself */
        break;
      zone_depth--;
      if (!zone_depth) {
        ok_to_zone = 0;
        notify(player, T("Overly deep zone chain broken."));
        break;
      }
    }
    if (ok_to_zone)
      Zone(thing) = Zone(newowner);
  }
  clear_flag_internal(thing, "CHOWN_OK");
  if (!preserve || !Wizard(player)) {
    clear_flag_internal(thing, "WIZARD");
    clear_flag_internal(thing, "ROYALTY");
    clear_flag_internal(thing, "TRUST");
    set_flag_internal(thing, "HALT");
    destroy_flag_bitmask("POWER", Powers(thing));
    Powers(thing) = new_flag_bitmask("POWER");
    do_halt(thing, "", thing);
  } else {
    if ((newowner != player) && Wizard(thing) && !God(player)) {
      notify_format(player,
                    T
                    ("Warning: WIZ flag reset on #%d because @CHOWN/PRESERVE is to a third party."),
                    thing);
      clear_flag_internal(thing, "WIZARD");
    }
    if (!null_flagmask("POWER", Powers(thing)) || Wizard(thing) ||
        Royalty(thing) || Inherit(thing))
      notify_format(player,
                    T
                    ("Warning: @CHOWN/PRESERVE on an object (#%d) with WIZ, ROY, INHERIT, or @power privileges."),
                    thing);
  }
}


/** Change an object's zone.
 * \verbatim
 * This implements @chzone.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the object to change zone of.
 * \param newobj name of new ZMO.
 * \param noisy if 1, notify player about success and failure.
 * \param preserve was the /preserve switch given?
 * \param pe_info the pe_info for lock and permission checks
 * \retval 0 failed to change zone.
 * \retval 1 successfully changed zone.
 */
int
do_chzone(dbref player, char const *name, char const *newobj, bool noisy,
          bool preserve, NEW_PE_INFO *pe_info)
{
  dbref thing;
  dbref zone;
  int has_lock;

  if ((thing = noisy_match_result(player, name, NOTYPE, MAT_NEARBY)) == NOTHING)
    return 0;

  if (!newobj || !*newobj || !strcasecmp(newobj, "none"))
    zone = NOTHING;
  else {
    if ((zone = noisy_match_result(player, newobj, NOTYPE, MAT_EVERYTHING))
        == NOTHING)
      return 0;
  }

  if (Zone(thing) == zone) {
    if (noisy)
      notify(player, T("That object is already in that zone."));
    return 0;
  }

  /* we do use ownership instead of control as a criterion because
   * we only want the owner to be able to rezone the object. Also,
   * this allows players to @chzone themselves to an object they own.
   */
  if (!(God(player) || (!God(thing) && Wizard(player)) || Owns(player, thing))) {
    if (noisy)
      notify(player, T("You don't have the power to shift reality."));
    return 0;
  }
  /* a player may change an object's zone to:
   * 1.  NOTHING
   * 2.  an object he owns
   * 3.  an object with a chzone-lock that the player passes.
   * Note that an object with no chzone-lock isn't valid
   */
  has_lock = (getlock(zone, Chzone_Lock) != TRUE_BOOLEXP);
  if (!(Wizard(player) || (zone == NOTHING) || Owns(player, zone) ||
        (has_lock && eval_lock_with(player, zone, Chzone_Lock, pe_info)))) {
    if (noisy) {
      if (has_lock) {
        fail_lock(player, zone, Chzone_Lock,
                  T("You cannot move that object to that zone."), NOTHING);
      } else {
        notify(player, T("You cannot move that object to that zone."));
      }
    }
    return 0;
  }
  /* Don't chzone object to itself for mortals! */
  if ((zone == thing) && !Hasprivs(player)) {
    if (noisy)
      notify(player, T("You shouldn't zone objects to themselves!"));
    return 0;
  }
  /* Don't allow circular zones */
  if (GoodObject(zone)) {
    dbref tmp;
    int zone_depth = MAX_ZONES;
    for (tmp = Zone(zone); GoodObject(tmp); tmp = Zone(tmp)) {
      if (tmp == thing) {
        notify(player, T("You can't make circular zones!"));
        return 0;
      }
      if (tmp == Zone(tmp))     /* Ran into an object zoned to itself */
        break;
      zone_depth--;
      if (!zone_depth) {
        notify(player, T("Overly deep zone chain."));
        return 0;
      }
    }
  }

  /* Don't allow chzone to objects without elocks!
   * If no lock is set, set a default lock (warn if zmo are used for control)
   * This checks for many trivial elocks (canuse/1, where &canuse=1)
   */
  if (zone != NOTHING)
    check_zone_lock(player, zone, noisy);

  /* Warn Wiz/Royals when they zone their stuff */
  if ((zone != NOTHING) && Hasprivs(Owner(thing))) {
    if (noisy)
      notify(player, T("Warning: @chzoning admin-owned object!"));
  }
  /* everything is okay, do the change */
  Zone(thing) = zone;

  /* If we're not unzoning, and we're working with a non-player object,
   * we'll remove wizard, royalty, inherit, and powers, for security, unless
   * a wizard is changing the zone and explicitly says not to.
   */
  if (!Wizard(player))
    preserve = 0;
  if (!preserve && ((zone != NOTHING) && !IsPlayer(thing))) {
    /* if the object is a player, resetting these flags is rather
     * inconvenient -- although this may pose a bit of a security
     * risk. Be careful when @chzone'ing wizard or royal players.
     */
    clear_flag_internal(thing, "WIZARD");
    clear_flag_internal(thing, "ROYALTY");
    clear_flag_internal(thing, "TRUST");
    destroy_flag_bitmask("POWER", Powers(thing));
    Powers(thing) = new_flag_bitmask("POWER");
  } else {
    if (noisy && (zone != NOTHING)) {
      if (Hasprivs(thing))
        notify(player, T("Warning: @chzoning a privileged player."));
      if (Inherit(thing))
        notify(player, T("Warning: @chzoning a TRUST player."));
    }
  }
  if (noisy)
    notify(player, T("Zone changed."));
  return 1;
}

/** Structure for af_helper() data. */
struct af_args {
  privbits setf;        /**< flag bits to set */
  privbits clrf;        /**< flag bits to clear */
  char *setflags;       /**< list of names of flags to set */
  char *clrflags;       /**< list of names of flags to clear */
};

static int
af_helper(dbref player, dbref thing,
          dbref parent __attribute__ ((__unused__)),
          char const *pattern
          __attribute__ ((__unused__)), ATTR *atr, void *args)
{
  struct af_args *af = args;

  /* We must be able to write to that attribute normally,
   * to prevent players from doing funky things to, say, LAST.
   * There is one special case - the resetting of the SAFE flag.
   */
  if (!(Can_Write_Attr(player, thing, AL_ATTR(atr)) ||
        ((af->clrf & AF_SAFE) &&
         Can_Write_Attr_Ignore_Safe(player, thing, AL_ATTR(atr))))) {
    notify_format(player, T("You cannot change that flag on %s/%s"),
                  Name(thing), AL_NAME(atr));
    return 0;
  }

  /* Clear flags first, then set flags */
  if (af->clrf) {
    AL_FLAGS(atr) &= ~af->clrf;
    if (!AreQuiet(player, thing))
      notify_format(player, T("%s/%s - %s reset."), Name(thing), AL_NAME(atr),
                    af->clrflags);
  }
  if (af->setf) {
    AL_FLAGS(atr) |= af->setf;
    if (!AreQuiet(player, thing))
      notify_format(player, T("%s/%s - %s set."), Name(thing), AL_NAME(atr),
                    af->setflags);
  }

  return 1;
}

static void
copy_attrib_flags(dbref player, dbref target, ATTR *atr, int flags)
{
  if (!atr)
    return;
  if (!Can_Write_Attr(player, target, AL_ATTR(atr))) {
    notify_format(player,
                  T("You cannot set attrib flags on %s/%s"), Name(target),
                  AL_NAME(atr));
    return;
  }
  if (AL_FLAGS(atr) & AF_ROOT)
    flags |= AF_ROOT;
  else
    flags &= ~AF_ROOT;
  AL_FLAGS(atr) = flags;
}

/** Set a flag on an attribute.
 * \param player the enactor.
 * \param obj the name of the object with the attribute.
 * \param atrname the name of the attribute.
 * \param flag the name of the flag to set or clear.
 */
void
do_attrib_flags(dbref player, const char *obj, const char *atrname,
                const char *flag)
{
  struct af_args af;
  dbref thing;
  const char *p;

  if ((thing = match_controlled(player, obj)) == NOTHING)
    return;

  if (!flag || !*flag) {
    notify(player, T("What flag do you want to set?"));
    return;
  }

  p = flag;
  /* Skip leading spaces */
  while (*p && isspace((unsigned char) *p))
    p++;

  af.setf = af.clrf = 0;
  if (string_to_atrflagsets(player, p, &af.setf, &af.clrf) < 0) {
    notify(player, T("Unrecognized attribute flag."));
    return;
  }
  if (af.clrf == 0 && af.setf == 0) {
    notify(player, T("What flag do you want to set?"));
    return;
  }

  af.clrflags = mush_strdup(atrflag_to_string(af.clrf), "af_flag list");
  af.setflags = mush_strdup(atrflag_to_string(af.setf), "af_flag list");
  if (!atr_iter_get(player, thing, atrname, 0, 0, af_helper, &af))
    notify(player, T("No attribute found to change."));
  mush_free(af.clrflags, "af_flag list");
  mush_free(af.setflags, "af_flag list");
}


/** Set a flag, attribute flag, or attribute.
 * \verbatim
 * This implements @set.
 * \endverbatim
 * \param player the enactor.
 * \param xname the first (left) argument to the command.
 * \param flag the second (right) argument to the command.
 * \retval 1 successful set.
 * \retval 0 failure to set.
 */
int
do_set(dbref player, const char *xname, char *flag)
{
  dbref thing;
  int her, listener, negate;
  char *p, *f, *name;
  char flagbuff[BUFFER_LEN];

  if (!xname || !*xname) {
    notify(player, T("I can't see that here."));
    return 0;
  }
  if (!flag || !*flag) {
    notify(player, T("What do you want to set?"));
    return 0;
  }

  name = mush_strdup(xname, "ds.string");

  /* check for attribute flag set first */
  if ((p = strchr(name, '/')) != NULL) {
    *p++ = '\0';
    do_attrib_flags(player, name, p, flag);
    mush_free(name, "ds.string");
    return 1;
  }
  /* find thing */
  if ((thing = match_controlled(player, name)) == NOTHING) {
    mush_free(name, "ds.string");
    return 0;
  }

  mush_free(name, "ds.string");

  if (God(thing) && !God(player)) {
    notify(player, T("Only God can set himself!"));
    return 0;
  }
  /* check for attribute set first */
  if ((p = strchr(flag, ':')) != NULL) {
    *p++ = '\0';
    if (!command_check_byname(player, "ATTRIB_SET", NULL)) {
      notify(player, T("You may not set attributes."));
      return 0;
    }
    return do_set_atr(thing, flag, p, player, 1);
  }
  /* we haven't set an attribute, so we must be setting flags */
  strcpy(flagbuff, flag);
  p = trim_space_sep(flagbuff, ' ');
  if (*p == '\0') {
    notify(player, T("You must specify a flag to set."));
    return 0;
  }
  do {
    her = Hearer(thing);        /* Must be in loop, can change! */
    listener = Listener(thing); /* Must be in loop, can change! */
    f = split_token(&p, ' ');
    negate = 0;
    if (*f == NOT_TOKEN && *(f + 1)) {
      negate = 1;
      f++;
    }
    set_flag(player, thing, f, negate, her, listener);
  } while (p);
  return 1;
}

/** Copy or move an attribute.
 * \verbatim
 * This implements @cpattr and @mvattr.
 * the command is of the format:
 * @cpattr oldobj/oldattr = newobj1/newattr1, newobj2/newattr2, etc.
 * \endverbatim
 * \param player the enactor.
 * \param oldpair the obj/attribute pair to copy from.
 * \param newpair array of obj/attribute pairs to copy to.
 * \param move if 1, move rather than copy.
 * \param noflagcopy if 1, don't copy associated flags.
 */
void
do_cpattr(dbref player, char *oldpair, char **newpair, int move, int noflagcopy)
{
  dbref oldobj, newobj;
  char tbuf1[BUFFER_LEN], tbuf2[BUFFER_LEN];
  int i;
  char *p, *q;
  ATTR *a;
  char *text;
  int copies = 0;

  /* must copy from something */
  if (!oldpair || !*oldpair) {
    notify(player, T("What do you want to copy from?"));
    return;
  }
  /* find the old object */
  strcpy(tbuf1, oldpair);
  p = strchr(tbuf1, '/');
  if (!p || !*p) {
    notify(player, T("What object do you want to copy the attribute from?"));
    return;
  }
  *p++ = '\0';
  oldobj = noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(oldobj))
    return;

  strcpy(tbuf2, p);
  p = tbuf2;
  /* find the old attribute */
  a = atr_get_noparent(oldobj, strupper(p));
  if (!a) {
    notify(player, T("No such attribute to copy from."));
    return;
  }
  /* check permissions to get it */
  if (!Can_Read_Attr(player, oldobj, a)) {
    notify(player, T("Permission to read attribute denied."));
    return;
  }
  /* we can read it. Copy the value. */
  text = safe_atr_value(a);

  /* now we loop through our new object pairs and copy, calling @set. */
  for (i = 1; i < MAX_ARG && (newpair[i] != NULL); i++) {
    if (!*newpair[i]) {
      notify(player, T("What do you want to copy to?"));
    } else {
      strcpy(tbuf1, newpair[i]);
      q = strchr(tbuf1, '/');
      if (!q || !*q) {
        q = (char *) AL_NAME(a);
      } else {
        *q++ = '\0';
      }
      newobj = noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING);
      if (GoodObject(newobj) &&
          ((newobj != oldobj) || strcasecmp(AL_NAME(a), q)) &&
          (do_set_atr(newobj, q, text, player, 1) == 1)) {
        copies++;
        /* copy the attribute flags too */
        if (!noflagcopy)
          copy_attrib_flags(player, newobj,
                            atr_get_noparent(newobj, strupper(q)), a->flags);
      }

    }
  }

  free(text);                   /* safe_uncompress malloc()s memory */
  if (copies) {
    notify_format(player, T("Attribute %s (%d copies)"),
                  (move ? T("moved") : T("copied")), copies);
    if (move)
      do_set_atr(oldobj, AL_NAME(a), NULL, player, 1);
  } else {
    notify_format(player, T("Unable to %s attribute."),
                  (move ? T("move") : T("copy")));
  }
  return;
}

/** Argument struct for gedit_helper */
struct gedit_args {
  int flags; /**< The type of edit */
  char *from; /**< What is going to be replaced? */
  char *to; /**< What it gets replaced with. */
  int edited; /**< Number of attributes edited */
  int skipped; /**< Number of attributes skipped */
};

static int
gedit_helper(dbref player, dbref thing,
             dbref parent __attribute__ ((__unused__)),
             char const *pattern
             __attribute__ ((__unused__)), ATTR *a, void *args)
{
  int ansi_long_flag = 0;
  const char *r;
  char *s, *val;
  char tbuf1[BUFFER_LEN], tbuf_ansi[BUFFER_LEN];
  char *tbufp, *tbufap;
  size_t rlen, vlen;
  struct gedit_args *gargs;
  int edited = 0;

  gargs = args;

  val = gargs->from;
  vlen = strlen(val);
  r = gargs->to ? gargs->to : "";
  rlen = strlen(r);

  tbufp = tbuf1;
  tbufap = tbuf_ansi;

  if (!a) {                     /* Shouldn't ever happen, but better safe than sorry */
    notify(player, T("No such attribute, try set instead."));
    return 0;
  }
  if (!Can_Write_Attr(player, thing, a)) {
    notify(player, T("You need to control an attribute to edit it."));
    gargs->skipped++;
    return 0;
  }
  s = (char *) atr_value(a);    /* warning: pointer to static buffer */

  if (vlen == 1 && *val == '$') {
    /* append */
    safe_str(s, tbuf1, &tbufp);
    safe_str(r, tbuf1, &tbufp);

    if (safe_format(tbuf_ansi, &tbufap, "%s%s%s%s", s, ANSI_HILITE, r,
                    ANSI_END))
      ansi_long_flag = 1;
    edited = 1;
  } else if (vlen == 1 && *val == '^') {
    /* prepend */
    safe_str(r, tbuf1, &tbufp);
    safe_str(s, tbuf1, &tbufp);

    if (safe_format(tbuf_ansi, &tbufap, "%s%s%s%s", ANSI_HILITE, r, ANSI_END,
                    s))
      ansi_long_flag = 1;
    edited = 1;
  } else if (!*val) {
    /* insert replacement string between every character */
    ansi_string *haystack;
    size_t last = 0;

    haystack = parse_ansi_string(s);

    /* Add one at the start */
    if (!safe_strl(r, rlen, tbuf1, &tbufp)) {
      edited++;
      if (!(gargs->flags & EDIT_FIRST)) {
        for (last = 0; last < (size_t) haystack->len; last++) {
          /* Add the next character */
          if (safe_ansi_string(haystack, last, 1, tbuf1, &tbufp)) {
            break;
          }
          if (!ansi_long_flag) {
            if (safe_ansi_string(haystack, last, 1, tbuf_ansi, &tbufap))
              ansi_long_flag = 1;
          }
          /* Copy in r */
          if (safe_str(r, tbuf1, &tbufp)) {
            break;
          }
          if (!ansi_long_flag) {
            if (safe_format(tbuf_ansi, &tbufap, "%s%s%s", ANSI_HILITE, r,
                            ANSI_END))
              ansi_long_flag = 1;
          }
        }
      }
    }
    free_ansi_string(haystack);
  } else {
    /* find and replace */
    ansi_string *haystack;
    size_t last = 0;
    char *p;
    int too_long = 0;

    haystack = parse_ansi_string(s);

    while (last < (size_t) haystack->len
           && (p = strstr(haystack->text + last, val)) != NULL) {
      edited = 1;
      if (safe_ansi_string(haystack, last, p - (haystack->text + last),
                           tbuf1, &tbufp)) {
        too_long = 1;
        break;
      }
      if (!ansi_long_flag) {
        if (safe_ansi_string(haystack, last, p - (haystack->text + last),
                             tbuf_ansi, &tbufap))
          ansi_long_flag = 1;
      }

      /* Copy in r */
      if (safe_str(r, tbuf1, &tbufp)) {
        too_long = 1;
        break;
      }
      if (!ansi_long_flag) {
        if (safe_format(tbuf_ansi, &tbufap, "%s%s%s", ANSI_HILITE, r, ANSI_END))
          ansi_long_flag = 1;
      }
      last = p - haystack->text + vlen;
      if (gargs->flags & EDIT_FIRST)
        break;
    }
    if (last < (size_t) haystack->len && !too_long) {
      safe_ansi_string(haystack, last, haystack->len, tbuf1, &tbufp);
      if (!ansi_long_flag) {
        if (safe_ansi_string(haystack, last, haystack->len, tbuf_ansi, &tbufap))
          ansi_long_flag = 1;
      }
    }
    free_ansi_string(haystack);
  }

  *tbufp = '\0';
  *tbufap = '\0';


  if (edited)
    gargs->edited++;
  else
    gargs->skipped++;

  if (!edited) {
    if (!(gargs->flags & EDIT_QUIET)) {
      notify_format(player, T("%s - Unchanged."), AL_NAME(a));
    }
  } else if (!(gargs->flags & EDIT_CHECK)) {
    if ((do_set_atr(thing, AL_NAME(a), tbuf1, player, 0) == 1) &&
        !(gargs->flags & EDIT_QUIET) && !AreQuiet(player, thing)) {
      if (!ansi_long_flag && ShowAnsi(player))
        notify_format(player, T("%s - Set: %s"), AL_NAME(a), tbuf_ansi);
      else
        notify_format(player, T("%s - Set: %s"), AL_NAME(a), tbuf1);
    }
  } else if (!(gargs->flags & EDIT_QUIET)) {
    /* We don't do it - we just pemit it. */
    if (!ansi_long_flag && ShowAnsi(player))
      notify_format(player, T("%s - Set: %s"), AL_NAME(a), tbuf_ansi);
    else
      notify_format(player, T("%s - Set: %s"), AL_NAME(a), tbuf1);
  }

  return 1;
}

/** Edit an attribute.
 * \verbatim
 * This implements @edit obj/attribute = {search}, {replace}
 * \endverbatim
 * \param player the enactor.
 * \param it the object/attribute pair.
 * \param argv array containing the search and replace strings.
 * \param flags type of \@edit to do
 */
void
do_gedit(dbref player, char *it, char **argv, int flags)
{
  dbref thing;
  char tbuf1[BUFFER_LEN];
  char *q;
  struct gedit_args args;


  if (!(it && *it)) {
    notify(player, T("I need to know what you want to edit."));
    return;
  }
  strcpy(tbuf1, it);
  q = strchr(tbuf1, '/');
  if (!(q && *q)) {
    notify(player, T("I need to know what you want to edit."));
    return;
  }
  *q++ = '\0';
  thing =
    noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING | MAT_CONTROL);

  if (thing == NOTHING)
    return;

  if (!argv[1] || !*argv[1]) {
    notify(player, T("Nothing to do."));
    return;
  }
  args.from = argv[1];
  args.to = argv[2];
  args.flags = flags;
  args.skipped = 0;
  args.edited = 0;

  if (!atr_iter_get(player, thing, q, 0, 0, gedit_helper, &args))
    notify(player, T("No matching attributes."));
  else if (flags & EDIT_QUIET)
    notify_format(player, T("%d attributes edited, %d skipped."), args.edited,
                  args.skipped);
}

/** Trigger an attribute.
 * \verbatim
 * This implements @trigger obj/attribute = list-of-arguments.
 * \endverbatim
 * \param player the enactor.
 * \param object the object/attribute pair.
 * \param argv array of arguments.
 * \param queue_entry parent queue entry
 */
void
do_trigger(dbref player, char *object, char **argv, MQUE *queue_entry)
{
  dbref thing;
  char *s;
  char tbuf1[BUFFER_LEN];
  PE_REGS *pe_regs;
  int i;

  strcpy(tbuf1, object);
  for (s = tbuf1; *s && (*s != '/'); s++) ;
  if (!*s) {
    notify(player, T("I need to know what attribute to trigger."));
    return;
  }
  *s++ = '\0';

  thing = noisy_match_result(player, tbuf1, NOTYPE, MAT_EVERYTHING);

  if (thing == NOTHING)
    return;

  if (!controls(player, thing) && !(Owns(player, thing) && LinkOk(thing))) {
    notify(player, T("Permission denied."));
    return;
  }
  if (God(thing) && !God(player)) {
    notify(player, T("You can't trigger God!"));
    return;
  }

  pe_regs = pe_regs_create(PE_REGS_ARG | PE_REGS_Q, "do_trigger");
  for (i = 0; i < 10; i++) {
    if (argv[i + 1]) {
      pe_regs_setenv_nocopy(pe_regs, i, argv[i + 1]);
    }
  }
  pe_regs_qcopy(pe_regs, queue_entry->pe_info->regvals);

  if (queue_attribute_base(thing, upcasestr(s), player, 0, pe_regs, 0)) {
    if (!AreQuiet(player, thing))
      notify_format(player, T("%s - Triggered."), Name(thing));
  } else {
    notify(player, T("No such attribute."));
  }
  pe_regs_free(pe_regs);
}


/** Include an attribute.
 * \verbatim
 * This implements @include obj/attribute
 * \endverbatim
 * \param executor the executor.
 * \param enactor the enactor.
 * \param object the object/attribute pair.
 * \param argv array of arguments.
 * \param queue_type QUEUE_* flags to use for the new queue entry
 * \param parent_queue the parent queue to include the new actionlist into
 */
void
do_include(dbref executor, dbref enactor, char *object, char **argv,
           int queue_type, MQUE *parent_queue)
{
  dbref thing;
  char *s;
  char tbuf1[BUFFER_LEN];

  strcpy(tbuf1, object);
  for (s = tbuf1; *s && (*s != '/'); s++) ;
  if (!*s) {
    notify(executor, T("I need to know what attribute to include."));
    return;
  }
  *s++ = '\0';

  thing = noisy_match_result(executor, tbuf1, NOTYPE, MAT_EVERYTHING);

  if (thing == NOTHING)
    return;

  if (God(thing) && !God(executor)) {
    notify(executor, T("You can't include God!"));
    return;
  }

  /* include modifies the stack, but only if arguments are given */
  if (!queue_include_attribute
      (thing, upcasestr(s), executor, enactor, enactor,
       (rhs_present ? argv + 1 : NULL), queue_type, parent_queue))
    notify(executor, T("No such attribute."));
}

/** The use command.
 * It's here for lack of a better place.
 * \param player the enactor.
 * \param what name of the object to use.
 */
void
do_use(dbref player, const char *what, NEW_PE_INFO *pe_info)
{
  dbref thing;

  /* if we pass the use key, do it */

  if ((thing =
       noisy_match_result(player, what, TYPE_THING,
                          MAT_NEAR_THINGS | MAT_ENGLISH)) != NOTHING) {
    if (!eval_lock_with(player, thing, Use_Lock, pe_info)) {
      fail_lock(player, thing, Use_Lock, T("Permission denied."), NOTHING);
      return;
    } else {
      did_it(player, thing, "USE", T("Used."), "OUSE", NULL,
             (charge_action(thing) ? "AUSE" : "RUNOUT"), NOTHING);
    }
  }
}

/** Parent an object to another.
 * \verbatim
 * This implements @parent.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the child object.
 * \param parent_name the name of the new parent object.
 */
void
do_parent(dbref player, char *name, char *parent_name, NEW_PE_INFO *pe_info)
{
  dbref thing;
  dbref parent;
  dbref check;
  int i;

  if ((thing =
       noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING)) == NOTHING)
    return;

  if (!parent_name || !*parent_name || !strcasecmp(parent_name, "none"))
    parent = NOTHING;
  else if ((parent = noisy_match_result(player, parent_name, NOTYPE,
                                        MAT_EVERYTHING)) == NOTHING)
    return;

  /* do control check */
  if (!controls(player, thing) && !(Owns(player, thing) && LinkOk(thing))) {
    notify(player, T("Permission denied."));
    return;
  }
  /* a player may change an object's parent to NOTHING or to an
   * object he owns, or one that is LINK_OK when the player passes
   * the parent lock
   * mod: also when the player controls the parent, it passes the parent lock
   * [removed owner and wizard check and added
   * control check (wich does those things
   * anyway, right?)]
   */
  if ((parent != NOTHING) && !controls(player, parent) && !(LinkOk(parent)
                                                            &&
                                                            eval_lock_with
                                                            (player, parent,
                                                             Parent_Lock,
                                                             pe_info))) {
    notify(player, T("Permission denied."));
    return;
  }
  /* check to make sure no recursion can happen */
  if (parent == thing) {
    notify(player, T("A thing cannot be its own ancestor!"));
    return;
  }
  if (parent != NOTHING) {
    for (i = 0, check = Parent(parent);
         (i < MAX_PARENTS) && (check != NOTHING); i++, check = Parent(check)) {
      if (check == thing) {
        notify(player, T("You are not allowed to be your own ancestor!"));
        return;
      }
    }
    if (i >= MAX_PARENTS) {
      notify(player, T("Too many ancestors."));
      return;
    }
  }
  /* everything is okay, do the change */
  Parent(thing) = parent;
  if (!AreQuiet(player, thing))
    notify(player, T("Parent changed."));
}

static int
wipe_helper(dbref player, dbref thing,
            dbref parent __attribute__ ((__unused__)),
            char const *pattern,
            ATTR *atr, void *args __attribute__ ((__unused__)))
{
  /* for added security, only God can modify wiz-only-modifiable
   * attributes using this command and wildcards.  Wiping a specific
   * attr still works, though.
   */
  int saved_count = AttrCount(thing);

  if (wildcard((char *) pattern) && AF_Wizard(atr) && !God(player))
    return 0;

  switch (wipe_atr(thing, AL_NAME(atr), player)) {
  case AE_SAFE:
    notify_format(player, T("Attribute %s is SAFE. Set it !SAFE to modify it."),
                  AL_NAME(atr));
    return 0;
  case AE_ERROR:
    notify_format(player, T("Unable to wipe attribute %s"), AL_NAME(atr));
    return 0;
  case AE_TREE:
    notify_format(player,
                  T
                  ("Attribute %s cannot be wiped because a child attribute cannot be wiped."),
                  AL_NAME(atr));
    /* Fall through */
  default:
    return saved_count - AttrCount(thing);
  }
}

/** Clear an attribute.
 * \verbatim
 * This implements @wipe.
 * \endverbatim
 * \param player the enactor.
 * \param name the object/attribute-pattern to wipe.
 */
void
do_wipe(dbref player, char *name)
{
  dbref thing;
  char *pattern;
  int wiped;

  if ((pattern = strchr(name, '/')) != NULL)
    *pattern++ = '\0';

  if ((thing = noisy_match_result(player, name, NOTYPE, MAT_NEARBY)) == NOTHING)
    return;

  /* this is too destructive of a command to be used by someone who
   * doesn't own the object. Thus, the check is on Owns not controls.
   */
  if (!Wizard(player) && !Owns(player, thing)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (God(thing) && !God(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  /* protect SAFE objects unless doing a non-wildcard pattern */
  if (Safe(thing) && !(pattern && *pattern && !wildcard(pattern))) {
    notify(player, T("That object is protected."));
    return;
  }

  wiped = atr_iter_get(player, thing, pattern, 0, 0, wipe_helper, NULL);
  switch (wiped) {
  case 0:
    notify(player, T("No attributes wiped."));
    break;
  case 1:
    notify(player, T("One attribute wiped."));
    break;
  default:
    notify_format(player, T("%d attributes wiped."), wiped);
  }
}
