/**
 * \file move.c
 *
 * \brief Movement commands for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "match.h"
#include "flags.h"
#include "lock.h"
#include "dbdefs.h"
#include "parse.h"
#include "log.h"
#include "command.h"
#include "cmds.h"
#include "game.h"
#include "confmagic.h"

void moveit(dbref what, dbref where, int nomovemsgs,
            dbref enactor, const char *cause);
static void send_contents(dbref loc, dbref dest);
static void maybe_dropto(dbref loc, dbref dropto);
static void add_follower(dbref leader, dbref follower);
static void add_following(dbref follower, dbref leader);
static void add_follow(dbref leader, dbref follower, int noisy);
static void del_follower(dbref leader, dbref follower);
static void del_following(dbref follower, dbref leader);
static void del_follow(dbref follower, dbref leader, int noisy);
static char *list_followers(dbref player);
static char *list_following(dbref player);
static int is_following(dbref follower, dbref leader);
static void follower_command(dbref leader, dbref loc, const char *com,
                             dbref towards);

/** A convenience wrapper for enter_room().
 * \param what object to move.
 * \param where location to move it to.
 * \param enactor the enactor
 * \param cause the reason for the object to move, for events
 */
void
moveto(dbref what, dbref where, dbref enactor, const char *cause)
{
  enter_room(what, where, 0, enactor, cause);
}


/** Send an object somewhere.
 * \param what object to move.
 * \param where location to move it to.
 * \param nomovemsgs if 1, don't show movement messages.
 * \param enactor the enactor
 * \param cause the reason for the object moving, for events
 */
void
moveit(dbref what, dbref where, int nomovemsgs,
       dbref enactor, const char *cause)
{
  dbref loc, old;
  dbref absloc, absold;
  bool whereSeeswhat, oldSeeswhat;

  /* Don't move something into something it's holding */
  if (recursive_member(where, what, 0))
    return;

  whereSeeswhat = Can_Locate(where, what);

  /* remove what from old loc */
  absold = absolute_room(what);
  if ((loc = old = Location(what)) != NOTHING) {
    Contents(loc) = remove_first(Contents(loc), what);
  }
  /* test for special cases */
  switch (where) {
  case NOTHING:
    Location(what) = NOTHING;
    return;                     /* NOTHING doesn't have contents */
  case HOME:
    where = Home(what);         /* home */
    safe_tel(what, where, nomovemsgs, enactor, cause);
    return;
    /*NOTREACHED */
    break;
  }

  /* now put what in where */
  PUSH(what, Contents(where));

  oldSeeswhat = (old < 0) || Can_Locate(old, what);

  Location(what) = where;
  absloc = absolute_room(what);
  if (!WIZ_NOAENTER || !(Wizard(what) && DarkLegal(what)))
    if ((where != NOTHING) && (old != where)) {
      did_it_with(what, what, NULL, NULL, "OXMOVE", NULL,
                  NULL, old, where, old, NA_INTER_HEAR);
      if (Hearer(what)) {
        if (GoodObject(where) && oldSeeswhat) {
          did_it_with(what, old, "LEAVE", NULL, "OLEAVE", T("has left."),
                      "ALEAVE", old, where, NOTHING, NA_INTER_PRESENCE);
        } else {
          did_it_interact(what, old, "LEAVE", NULL, "OLEAVE", T("has left."),
                          "ALEAVE", old, NA_INTER_PRESENCE);
        }
        /* If the player is leaving a zone, do zone messages */
        /* The tricky bit here is that we only care about the zone of
         * the outermost contents */
        if (GoodObject(absold) && GoodObject(Zone(absold)) &&
            (!GoodObject(absloc) || !GoodObject(Zone(absloc)) ||
             (Zone(absloc) != Zone(absold))))
          did_it_interact(what, Zone(absold), "ZLEAVE", NULL, "OZLEAVE", NULL,
                          "AZLEAVE", old, NA_INTER_SEE);
        if (GoodObject(old) && !IsRoom(old))
          did_it_interact(what, old, NULL, NULL, "OXLEAVE", NULL, NULL, where,
                          NA_INTER_SEE);
        if (!IsRoom(where))
          did_it_interact(what, where, NULL, NULL, "OXENTER", NULL, NULL, old,
                          NA_INTER_SEE);
        /* If the player is entering a new zone, do zone messages */
        if (GoodObject(absloc) && GoodObject(Zone(absloc)) &&
            (!GoodObject(absold) || !GoodObject(Zone(absold)) ||
             (Zone(absloc) != Zone(absold))))
          did_it_interact(what, Zone(absloc), "ZENTER", NULL, "OZENTER", NULL,
                          "AZENTER", where, NA_INTER_SEE);
        if (GoodObject(old) && whereSeeswhat) {
          did_it_with(what, where, "ENTER", NULL, "OENTER", T("has arrived."),
                      "AENTER", where, old, NOTHING, NA_INTER_PRESENCE);
        } else {
          did_it_interact(what, where, "ENTER", NULL, "OENTER",
                          T("has arrived."), "AENTER", where,
                          NA_INTER_PRESENCE);
        }
      } else {
        /* non-listeners only trigger the actions not the messages */
        did_it(what, old, NULL, NULL, NULL, NULL, "ALEAVE", old);
        if (GoodObject(absold) && GoodObject(Zone(absold)) &&
            (!GoodObject(absloc) || !GoodObject(Zone(absloc)) ||
             (Zone(absloc) != Zone(absold))))
          did_it(what, Zone(absold), NULL, NULL, NULL, NULL, "AZLEAVE", old);
        if (GoodObject(absloc) && GoodObject(Zone(absloc)) &&
            (!GoodObject(absold) || !GoodObject(Zone(absold)) ||
             (Zone(absloc) != Zone(absold))))
          did_it(what, Zone(absloc), NULL, NULL, NULL, NULL, "AZENTER", where);
        did_it(what, where, NULL, NULL, NULL, NULL, "AENTER", where);
      }
    }
  if (!nomovemsgs)
    did_it_with(what, what, "MOVE", NULL, "OMOVE", NULL,
                "AMOVE", where, where, old, NA_INTER_SEE);
  queue_event(enactor, "OBJECT`MOVE", "%s,%s,%s,%d,%s",
              unparse_objid(what),
              unparse_objid(where),
              unparse_objid(old), nomovemsgs ? 1 : 0, cause);
}

/** A dropper is an object that can hear and has a connected owner */
#define Dropper(thing) (Hearer(thing) && Connected(Owner(thing)))

static void
send_contents(dbref loc, dbref dest)
{
  dbref first;
  dbref rest;
  first = Contents(loc);

  /* blast locations of everything in list.
   *
   * Not now, as object`move depends on it. */
  /*
     Contents(loc) = NOTHING;
     DOLIST(rest, first) {
     Location(rest) = NOTHING;
     }
   */

  while (first != NOTHING) {
    rest = Next(first);
    if (!(Dropper(first) || !eval_lock(first, loc, Dropto_Lock))) {
      enter_room(first, Sticky(first) ? HOME : dest, 0, SYSEVENT, "dropto");
    }
    first = rest;
  }

  /*
     Contents(loc) = reverse(Contents(loc));
   */
}

static void
maybe_dropto(dbref loc, dbref dropto)
{
  dbref thing;
  if (loc == dropto)
    return;                     /* bizarre special case */
  if (!IsRoom(loc))
    return;
  /* check for players */
  DOLIST(thing, Contents(loc)) {
    if (Dropper(thing))
      return;
  }

  /* no players, send everything to the dropto */
  send_contents(loc, dropto);
}

/** Enter a container.
 * \param player object entering the container.
 * \param loc container to enter.
 * \param nomovemsgs if 1, don't give movement messages.
 * \param enactor object that caused this moving
 * \param cause what command or event caused this move
 */
void
enter_room(dbref player, dbref loc, int nomovemsgs,
           dbref enactor, const char *cause)
{
  dbref old;
  dbref dropto;
  static int deep = 0;
  if (deep++ > 15) {
    deep--;
    return;
  }
  if (!GoodObject(player)) {
    deep--;
    return;
  }
  /* check for room == HOME */
  if (loc == HOME)
    loc = Home(player);

  if (!Mobile(player)) {
    do_rawlog(LT_ERR, "ERROR: Non object moved!! %d\n", player);
    deep--;
    return;
  }
  if (IsExit(loc)) {
    do_rawlog(LT_ERR, "ERROR: Attempt to move %d to exit %d\n", player, loc);
    deep--;
    return;
  }
  if (loc == player) {
    do_rawlog(LT_ERR, "ERROR: Attempt to move player %d into itself\n", player);
    deep--;
    return;
  }
  if (recursive_member(loc, player, 0)) {
    do_rawlog(LT_ERR,
              "ERROR: Attempt to move player %d into carried object %d\n",
              player, loc);
    deep--;
    return;
  }
  /* get old location */
  old = Location(player);

  /* go there */
  moveit(player, loc, nomovemsgs, enactor, cause);

  /* if old location has STICKY dropto, send stuff through it */

  if ((loc != old) && Dropper(player) &&
      (old != NOTHING) && (IsRoom(old)) &&
      ((dropto = Location(old)) != NOTHING) && Sticky(old))
    maybe_dropto(old, dropto);


  /* autolook */
  look_room(player, loc, LOOK_AUTO, NULL);
  deep--;
}


/** Teleport player to location while removing items they shouldn't take.
 * \param player player to teleport.
 * \param dest location to teleport player to.
 * \param nomovemsgs if 1, don't show movement messages
 * \param enactor the enactor
 * \param cause what command or event caused this move
 */
void
safe_tel(dbref player, dbref dest, int nomovemsgs,
         dbref enactor, const char *cause)
{
  dbref first;
  dbref rest;
  if (dest == HOME)
    dest = Home(player);
  if (Owner(Location(player)) == Owner(dest)) {
    enter_room(player, dest, nomovemsgs, enactor, cause);
    return;
  }
  first = Contents(player);
  Contents(player) = NOTHING;

  /* blast locations of everything in list */
  DOLIST(rest, first) {
    Location(rest) = NOTHING;
  }

  while (first != NOTHING) {
    rest = Next(first);
    /* if thing is ok to take then move to player else send home.
     * thing is not okay to move if it's STICKY and its home is not
     * the player.
     */
    if (!controls(player, first)
        && (Sticky(first)
            && (Home(first) != player)))
      enter_room(first, HOME, nomovemsgs, enactor, cause);
    else {
      PUSH(first, Contents(player));
      Location(first) = player;
    }
    first = rest;
  }
  Contents(player) = reverse(Contents(player));
  enter_room(player, dest, nomovemsgs, enactor, cause);
}

/** Can a player go in a given direction?
 * This checks to see if there's a go-able direction. It doesn't
 * check whether the GOTO command is restricted. That should be
 * done by the command parser.
 * \param player dbref of mover.
 * \param direction name of direction to move.
 */
int
can_move(dbref player, const char *direction)
{
  int ok;
  if (!strcasecmp(direction, "home")) {
    ok = command_check_byname(player, "HOME", NULL);
  } else {
    /* otherwise match on exits - don't use GoodObject here! */
    ok =
      (match_result
       (player, direction, TYPE_EXIT,
        MAT_ENGLISH | MAT_EXIT | MAT_TYPE) != NOTHING);
  }
  return ok;                    /* Written like this due to overeager compiler */
}

dbref
find_var_dest(dbref player, dbref exit_obj)
{
  /* This is used to evaluate the u-function DESTINATION on an exit with
   * a VARIABLE (ambiguous) link.
   */
  char buff[BUFFER_LEN];
  /* We'd like a DESTINATION attribute, but we'll settle for EXITTO,
   * for portability
   */
  if (!call_attrib(exit_obj, "DESTINATION", buff, player, NULL, NULL) &&
      !call_attrib(exit_obj, "EXITTO", buff, player, NULL, NULL))
    return NOTHING;

  if (!buff[0])
    return NOTHING;

  return parse_objid(buff);
}


/** The move command.
 * \param player the enactor.
 * \param direction name of direction to move.
 * \param type type of motion to check (global, zone, neither).
 */
void
do_move(dbref player, const char *direction, enum move_type type,
        NEW_PE_INFO *pe_info)
{
  dbref exit_m, loc, var_dest;
  if (!strcasecmp(direction, "home") && can_move(player, "home")) {
    /* send him home */
    /* but steal all his possessions */
    if (!Mobile(player) || !GoodObject(Home(player)) ||
        recursive_member(Home(player), player, 0)
        || (player == Home(player))) {
      notify(player, T("Bad destination."));
      return;
    }
    if ((loc = Location(player)) != NOTHING && !Dark(player) && !Dark(loc)) {
      char msg[BUFFER_LEN];
      sprintf(msg, T("%s goes home."), Name(player));
      /* tell everybody else */
      notify_except(loc, player, msg, NA_INTER_SEE);
    }
    /* give the player the messages */
    notify(player, T("There's no place like home..."));
    notify(player, T("There's no place like home..."));
    notify(player, T("There's no place like home..."));
    safe_tel(player, HOME, 0, player, "home");
  } else {
    int matchtype;
    /* find the exit */
    if (type == MOVE_TELEPORT)
      matchtype = MAT_ABSOLUTE | MAT_TYPE;
    else {
      matchtype = MAT_ENGLISH | MAT_EXIT | MAT_CHECK_KEYS | MAT_TYPE;
      if (type == MOVE_GLOBAL)
        matchtype |= MAT_GLOBAL;
      else if (type == MOVE_ZONE)
        matchtype |= MAT_REMOTES;
    }
    exit_m = match_result(player, direction, TYPE_EXIT, matchtype);
    switch (exit_m) {
    case NOTHING:
      /* try to force the object */
      notify(player, T("You can't go that way."));
      break;
    case AMBIGUOUS:
      notify(player, T("I don't know which way you mean!"));
      break;
    default:
      /* we got one */
      /* check to see if we're allowed to pass */
      if (!eval_lock_with(player, Location(player), Leave_Lock, pe_info)) {
        fail_lock(player, Location(player), Leave_Lock,
                  T("You can't go that way."), NOTHING);
        return;
      }

      if (could_doit(player, exit_m, pe_info)) {
        switch (Destination(exit_m)) {
        case HOME:
          var_dest = Home(player);
          break;
        case AMBIGUOUS:
          var_dest = find_var_dest(player, exit_m);
          /* Only allowed if the owner of the exit could link to var_dest */
          if (!GoodObject(var_dest) || !can_link_to(exit_m, var_dest, pe_info)) {
            notify_format(player,
                          T
                          ("Variable exit destination #%d is invalid or not permitted."),
                          var_dest);
            return;
          }
          break;
        default:
          var_dest = Destination(exit_m);
        }

        if (!GoodObject(var_dest)) {
          do_rawlog(LT_ERR,
                    "Exit #%d destination became %d during move.\n",
                    exit_m, var_dest);
          notify(player, T("Exit destination is invalid."));
          return;
        }
        if (recursive_member(var_dest, player, 0)) {
          notify(player, T("Exit destination is invalid."));
          return;
        }
        did_it(player, exit_m, "SUCCESS", NULL, "OSUCCESS", NULL,
               "ASUCCESS", NOTHING);
        did_it(player, exit_m, "DROP", NULL, "ODROP", NULL, "ADROP", var_dest);
        switch (Typeof(var_dest)) {

        case TYPE_ROOM:
          /* Remember the current room */
          loc = Location(player);
          /* Move the leader */
          enter_room(player, var_dest, 0, player, "move");
          /* Move the followers if the leader is elsewhere */
          if (Location(player) != loc)
            follower_command(player, loc, "GOTO", exit_m);
          break;
        case TYPE_PLAYER:
        case TYPE_THING:
          if (IsGarbage(var_dest)) {
            notify(player, T("You can't go that way."));
            return;
          }
          if (Location(var_dest) == NOTHING)
            return;
          /* Remember the current room */
          loc = Location(player);
          /* Move the leader */
          safe_tel(player, var_dest, 0, player, "move");
          /* Move the followers if the leader is elsewhere */
          if (Location(player) != loc)
            follower_command(player, loc, "GOTO", exit_m);
          break;
        case TYPE_EXIT:
          notify(player, T("This feature coming soon."));
          break;
        }
      } else
        fail_lock(player, exit_m, Basic_Lock, T("You can't go that way."),
                  NOTHING);
      break;
    }
  }
}

/** Move an exit to the first position in the room's exit list.
 * \verbatim
 * This implements @firstexit.
 * \endverbatim
 * \param player the enactor.
 * \param what name of exit to promote.
 */
void
do_firstexit(dbref player, const char **what)
{
  dbref thing;
  dbref loc;
  int i;

  for (i = 1; i < MAX_ARG && what[i]; i++) {
    if ((thing =
         noisy_match_result(player, what[i], TYPE_EXIT,
                            MAT_ENGLISH | MAT_EXIT | MAT_TYPE)) == NOTHING)
      continue;
    loc = Home(thing);
    if (!controls(player, loc)) {
      notify(player, T("You cannot modify exits in that room."));
      continue;
    }
    Exits(loc) = remove_first(Exits(loc), thing);
    Source(thing) = loc;
    PUSH(thing, Exits(loc));
    notify_format(player, T("%s is now the first exit in %s."), Name(thing),
                  unparse_object(player, loc));
  }
}


/** The get command.
 * \param player the enactor.
 * \param what name of object to get.
 */
void
do_get(dbref player, const char *what, NEW_PE_INFO *pe_info)
{
  dbref loc = Location(player);
  dbref thing;
  char tbuf1[BUFFER_LEN], tbuf2[BUFFER_LEN], *tp;
  long match_flags = MAT_NEIGHBOR | MAT_CHECK_KEYS | MAT_NEAR | MAT_ENGLISH;

  if (!IsRoom(loc) && !EnterOk(loc) && !controls(player, loc)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (Long_Fingers(player))
    match_flags |= MAT_ABSOLUTE;
  if (match_result(player, what, TYPE_THING, match_flags) == NOTHING) {
    if (POSSESSIVE_GET) {
      dbref box;
      const char *boxname;
      char objnamebuf[BUFFER_LEN], *objname;
      boxname = what;
      strcpy(objnamebuf, what);
      objname = objnamebuf;
      /* take care of possessive get (stealing) */
      box = parse_match_possessor(player, &objname, 0);
      if (box == NOTHING) {
        notify(player, T("I don't see that here."));
        return;
      } else if (box == AMBIGUOUS) {
        notify_format(player, T("I can't tell which %s."), boxname);
        return;
      }
      thing =
        match_result_relative(player, box, objname, NOTYPE, MAT_OBJ_CONTENTS);
      if (thing == NOTHING) {
        notify(player, T("I don't see that here."));
        return;
      } else if (thing == AMBIGUOUS) {
        notify_format(player, T("I can't tell which %s."), what);
        return;
      }
      /* to steal something, you have to be able to get it, and the
       * object must be ENTER_OK and not take-locked against you.
       */
      if (could_doit(player, thing, pe_info) &&
          (POSSGET_ON_DISCONNECTED ||
           (!IsPlayer(Location(thing)) ||
            Connected(Location(thing)))) &&
          (controls(player, thing) ||
           (EnterOk(Location(thing)) &&
            eval_lock_with(player, Location(thing), Take_Lock, pe_info)))) {
        notify_format(Location(thing),
                      T("%s was taken from you."), Name(thing));
        notify_format(thing, T("%s took you."), Name(player));
        tp = tbuf1;
        safe_format(tbuf1, &tp, T("You take %s from %s."), Name(thing),
                    Name(Location(thing)));
        *tp = '\0';
        tp = tbuf2;
        safe_format(tbuf2, &tp, T("takes %s from %s."), Name(thing),
                    Name(Location(thing)));
        *tp = '\0';
        moveto(thing, player, player, "get");
        did_it(player, thing, "SUCCESS", tbuf1, "OSUCCESS", tbuf2, "ASUCCESS",
               NOTHING);
        did_it_with(player, player, "RECEIVE", NULL, "ORECEIVE", NULL,
                    "ARECEIVE", NOTHING, thing, NOTHING, NA_INTER_HEAR);
      } else
        fail_lock(player, thing, Basic_Lock,
                  T("You can't take that from there."), NOTHING);
    } else {
      notify(player, T("I don't see that here."));
    }
    return;
  } else {
    if ((thing = noisy_match_result(player, what, TYPE_THING, match_flags))
        != NOTHING) {
      if (Location(thing) == player) {
        notify(player, T("You already have that!"));
        return;
      }
      if (Location(player) == thing) {
        notify(player, T("It's all around you!"));
        return;
      }
      if (recursive_member(player, thing, 0)) {
        notify(player, T("Bad destination."));
        return;
      }
      switch (Typeof(thing)) {
      case TYPE_PLAYER:
      case TYPE_THING:
        if (thing == player) {
          notify(player, T("You cannot get yourself!"));
          return;
        }
        if (!eval_lock_with(player, Location(thing), Take_Lock, pe_info)) {
          fail_lock(player, Location(thing), Take_Lock,
                    T("You can't take that from there."), NOTHING);
          return;
        }
        if (could_doit(player, thing, pe_info)) {
          moveto(thing, player, player, "get");
          notify_format(thing, T("%s took you."), Name(player));
          tp = tbuf1;
          safe_format(tbuf1, &tp, T("You take %s."), Name(thing));
          *tp = '\0';
          tp = tbuf2;
          safe_format(tbuf2, &tp, T("takes %s."), Name(thing));
          *tp = '\0';
          did_it(player, thing, "SUCCESS", tbuf1, "OSUCCESS", tbuf2,
                 "ASUCCESS", NOTHING);
          did_it_with(player, player, "RECEIVE", NULL, "ORECEIVE", NULL,
                      "ARECEIVE", NOTHING, thing, NOTHING, NA_INTER_HEAR);
        } else
          fail_lock(player, thing, Basic_Lock, T("You can't pick that up."),
                    NOTHING);
        break;
      case TYPE_EXIT:
        notify(player, T("You can't pick up exits."));
        return;
      default:
        notify(player, T("You can't take that!"));
        break;
      }
    }
  }
}


/** Drop an object.
 * \param player the enactor.
 * \param name name of object to drop.
 */
void
do_drop(dbref player, const char *name, NEW_PE_INFO *pe_info)
{
  dbref loc;
  dbref thing;
  char tbuf1[BUFFER_LEN], tbuf2[BUFFER_LEN], *tp;
  if ((loc = Location(player)) == NOTHING)
    return;
  switch (thing =
          match_result(player, name, TYPE_THING | TYPE_PLAYER,
                       MAT_POSSESSION | MAT_ENGLISH | MAT_TYPE)) {
  case NOTHING:
    notify(player, T("You don't have that!"));
    return;
  case AMBIGUOUS:
    notify(player, T("I don't know which you mean!"));
    return;
  default:
    if (Location(thing) != player) {
      /* Shouldn't ever happen. */
      notify(player, T("You can't drop that."));
      return;
    } else if (IsExit(thing)) {
      notify(player, T("Sorry, you can't drop exits."));
      return;
    } else if (!eval_lock_with(player, thing, Drop_Lock, pe_info)) {
      fail_lock(player, thing, Drop_Lock,
                T("You can't seem to get rid of that."), NOTHING);
      return;
    } else if (IsRoom(loc) && !eval_lock_with(player, loc, Drop_Lock, pe_info)) {
      fail_lock(player, loc, Drop_Lock,
                T("You can't seem to drop things here."), NOTHING);
      return;
    } else if (Sticky(thing) && !Fixed(thing)) {
      notify(thing, T("Dropped."));
      safe_tel(thing, HOME, 0, player, "drop");
    } else if ((Location(loc) != NOTHING) && IsRoom(loc) && !Sticky(loc)
               && eval_lock_with(thing, loc, Dropto_Lock, pe_info)) {
      /* location has immediate dropto */
      notify_format(thing, T("%s drops you."), Name(player));
      moveto(thing, Location(loc), player, "drop");
    } else {
      notify_format(thing, T("%s drops you."), Name(player));
      moveto(thing, loc, player, "drop");
    }
    break;
  }
  tp = tbuf1;
  safe_format(tbuf1, &tp, T("You drop %s."), Name(thing));
  *tp = '\0';
  tp = tbuf2;
  safe_format(tbuf2, &tp, T("drops %s."), Name(thing));
  *tp = '\0';
  did_it(player, thing, "DROP", tbuf1, "ODROP", tbuf2, "ADROP", NOTHING);
}

/** The empty command.
 * This command causes the player to attempt to move everything in
 * the thing to the location of the thing.
 * Thing must be in player's inventory or in player's location.
 * For each item in thing, movement is allowed if one of these is true:
 * (a) thing is inside player, and player is allowed to get thing's item
 * (b) thing is next to player, player is allowed to get thing's item,
 *     and player is allowed to drop item in player's location.
 * We do not consider the cases of forcing the object to drop the items,
 * teleporting the items out, or forcing the items to leave;
 * 'empty' implies that the items pass through the player's hands.
 *
 * There is a choice to be made here with regard to locks - do we
 * check locks on the thing (e.g. enter locks) and its location
 * (e.g. drop locks) once or each time? If we choose once, we break
 * locks that might make decisions based on the number of items there.
 * If we choose multiple, we risk running side effects more than once.
 * We choose multiple, as that's what would happen if the
 * player did it manually.
 * \param player the enactor.
 * \param what the name of the object to empty.
 */
void
do_empty(dbref player, const char *what, NEW_PE_INFO *pe_info)
{
  dbref player_loc;
  dbref thing, thing_loc;
  dbref item;
  int empty_ok;
  int count = 0;
  int next;

  if ((player_loc = Location(player)) == NOTHING)
    return;
  thing =
    noisy_match_result(player, what, TYPE_THING | TYPE_PLAYER,
                       MAT_NEAR_THINGS | MAT_ENGLISH | MAT_TYPE);
  if (!GoodObject(thing))
    return;
  thing_loc = Location(thing);

  /* Object to empty must be in player's inventory or location */
  if ((thing_loc != player) && (thing_loc != player_loc)) {
    notify(player, T("You can't empty that from here."));
    return;
  }
  for (item = first_visible(player, Contents(thing)); GoodObject(item);
       item = first_visible(player, next)) {
    next = Next(item);
    if (IsExit(item))
      continue;                 /* No dropping exits */
    empty_ok = 0;
    if (player == thing) {
      /* empty me: You don't need to get what's in your inventory already */
      if (eval_lock_with(player, item, Drop_Lock, pe_info) &&
          (!IsRoom(thing_loc)
           || eval_lock_with(player, thing_loc, Drop_Lock, pe_info)))
        empty_ok = 1;
    }
    /* Check that player can get stuff from thing */
    else if (controls(player, thing) || (EnterOk(thing)
                                         && eval_lock_with(player, thing,
                                                           Enter_Lock,
                                                           pe_info))) {
      /* Check that player can get item */
      if (!could_doit(player, item, pe_info)) {
        /* Send failure message if set, otherwise be quiet */
        fail_lock(player, thing, Basic_Lock, NULL, NOTHING);
        continue;
      }
      /* Now check for dropping in the destination */
      /* Thing is in player's inventory - sufficient */
      if (thing_loc == player)
        empty_ok = 1;
      /* Thing is in player's location - player must also be able to drop */
      else if (eval_lock_with(player, item, Drop_Lock, pe_info) &&
               (!IsRoom(thing_loc)
                || eval_lock_with(player, thing_loc, Drop_Lock, pe_info)))
        empty_ok = 1;
    }
    /* Now do the work, if we should. That includes triggering messages */
    if (empty_ok) {
      char tbuf1[BUFFER_LEN], tbuf2[BUFFER_LEN], *tp;

      count++;
      /* Get messages */
      if (thing != player) {
        notify_format(thing, T("%s was taken from you."), Name(item));
        notify_format(item, T("%s took you."), Name(player));
        tp = tbuf1;
        safe_format(tbuf1, &tp, T("You take %s from %s."), Name(item),
                    Name(thing));
        *tp = '\0';
        tp = tbuf2;
        safe_format(tbuf2, &tp, T("takes %s from %s."), Name(item),
                    Name(thing));
        *tp = '\0';
        moveto(item, player, player, "empty");
        did_it(player, item, "SUCCESS", tbuf1, "OSUCCESS", tbuf2, "ASUCCESS",
               NOTHING);
        did_it_with(player, player, "RECEIVE", NULL, "ORECEIVE", NULL,
                    "ARECEIVE", NOTHING, item, NOTHING, NA_INTER_HEAR);
      }
      /* Drop messages */
      if (thing_loc != player) {
        if (Sticky(item) && !Fixed(item)) {
          safe_tel(thing, HOME, 0, player, "empty");
        } else if ((Location(thing_loc) != NOTHING) && IsRoom(thing_loc)
                   && !Sticky(thing_loc)
                   && eval_lock_with(item, thing_loc, Dropto_Lock, pe_info)) {
          /* location has immediate dropto */
          notify_format(item, T("%s drops you."), Name(player));
          moveto(item, Location(thing_loc), player, "empty");
        } else {
          notify_format(item, T("%s drops you."), Name(player));
          moveto(item, thing_loc, player, "empty");
        }
        tp = tbuf1;
        safe_format(tbuf1, &tp, T("You drop %s."), Name(item));
        *tp = '\0';
        tp = tbuf2;
        safe_format(tbuf2, &tp, T("drops %s."), Name(item));
        *tp = '\0';
        did_it(player, item, "DROP", tbuf1, "ODROP", tbuf2, "ADROP", NOTHING);
      }
    }
  }
  if (count == 1)
    notify_format(player, T("You remove 1 object from %s."), Name(thing));
  else
    notify_format(player, T("You remove %d objects from %s."),
                  count, Name(thing));

  return;
}


/** The enter command.
 * \param player the enactor.
 * \param what name of object to enter.
 */
void
do_enter(dbref player, const char *what, NEW_PE_INFO *pe_info)
{
  dbref thing;
  dbref loc;
  long match_flags = MAT_NEIGHBOR | MAT_ENGLISH | MAT_EXIT;

  if (Hasprivs(player))
    match_flags |= MAT_ABSOLUTE;
  if ((thing = noisy_match_result(player, what, TYPE_THING, match_flags))
      == NOTHING)
    return;
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    notify(player, T("Permission denied."));
    return;
  case TYPE_EXIT:
    do_move(player, what, MOVE_NORMAL, pe_info);
    return;
  default:
    /* Remember the current room */
    loc = Location(player);
    /* Only privileged players may enter something remotely */
    if ((Location(thing) != loc) && !Hasprivs(player)) {
      notify(player, T("I don't see that here."));
      return;
    }
    /* the object must pass the lock. Also, the thing being entered */
    /* has to be controlled, or must be enter_ok */
    if (!((EnterOk(thing) || controls(player, thing)) &&
          (eval_lock_with(player, thing, Enter_Lock, pe_info))
        )) {
      fail_lock(player, thing, Enter_Lock, T("Permission denied."), NOTHING);
      return;
    }
    if (thing == player) {
      notify(player, T("Sorry, you must remain beside yourself!"));
      return;
    }
    /* Move the leader */
    safe_tel(player, thing, 0, player, "enter");
    /* Move the followers if the leader is elsewhere */
    if (Location(player) != loc)
      follower_command(player, loc, "ENTER", thing);
    break;
  }
}

/** The leave command.
 * \param player the enactor.
 */
void
do_leave(dbref player, NEW_PE_INFO *pe_info)
{
  dbref loc;
  loc = Location(player);
  if (IsRoom(loc) || IsGarbage(loc) || IsGarbage(Location(loc))
      || NoLeave(loc)
      || !eval_lock_with(player, loc, Leave_Lock, pe_info)
    ) {
    fail_lock(player, loc, Leave_Lock, T("You can't leave."), NOTHING);
    return;
  }
  enter_room(player, Location(loc), 0, player, "leave");
  if (Location(player) != loc)
    follower_command(player, loc, "leave", NOTHING);
}

/** Is direction a global exit?
 * \param player looker.
 * \param direction name of exit.
 * \retval 1 direction is a global exit.
 * \retval 0 direction is not a global exit.
 */
int
global_exit(dbref player, const char *direction)
{
  return (GoodObject
          (match_result(player, direction, TYPE_EXIT, MAT_GLOBAL | MAT_EXIT)));
}

/** Is direction a remote exit?
 * \param player looker.
 * \param direction name of exit.
 * \retval 1 direction is a remote exit.
 * \retval 0 direction is not a remote exit.
 */
int
remote_exit(dbref player, const char *direction)
{
  return (GoodObject
          (match_result(player, direction, TYPE_EXIT, MAT_REMOTES | MAT_EXIT)));
}

/** Wrapper for exit movement.
 * We check local exit, then zone exit, then global. If nothing is
 * matched, treat it as local so player will get an error message.
 * \param player the mover.
 * \param command direction to move.
 */
void
move_wrapper(dbref player, const char *command, NEW_PE_INFO *pe_info)
{
  if (!Mobile(player))
    return;
  if (can_move(player, command))
    do_move(player, command, MOVE_NORMAL, pe_info);
  else if ((Zone(Location(player)) != NOTHING) && remote_exit(player, command))
    do_move(player, command, MOVE_ZONE, pe_info);
  else if ((Location(player) != MASTER_ROOM)
           && global_exit(player, command))
    do_move(player, command, MOVE_GLOBAL, pe_info);
  else
    do_move(player, command, MOVE_NORMAL, pe_info);
}

/* Routines for dealing with the follow commands */

/** The follow command.
 * \verbatim
 * follow <arg> tries to start following
 * follow alone lists who you're following
 * \endverbatim
 * \param player the enactor.
 * \param arg name of object to follow.
 */
void
do_follow(dbref player, const char *arg, NEW_PE_INFO *pe_info)
{
  dbref leader;
  if (arg && *arg) {
    /* Who do we want to follow? */
    leader = match_result(player, arg, NOTYPE, MAT_NEARBY);
    if (leader == AMBIGUOUS) {
      notify(player, T("I can't tell which one to follow."));
      return;
    }
    if (!GoodObject(leader) || !GoodObject(Location(player))
        || (IsPlayer(leader) && !Connected(leader))
        || ((DarkLegal(leader)
             || (Dark(Location(player)) && !Light(leader)))
            && !See_All(player))) {
      notify(player, T("You don't see that here."));
      return;
    }
    if (!Mobile(leader)) {
      notify(player, T("You can only follow players and things."));
      return;
    }
    if (leader == player) {
      notify(player, T("You chase your tail for a while and feel silly."));
      return;
    }
    /* Are we already following them? */
    if (is_following(player, leader)) {
      notify_format(player, T("You're already following %s."), Name(leader));
      return;
    }
    /* Ok, are we allowed to follow them? */
    if (!eval_lock_with(player, leader, Follow_Lock, pe_info)) {
      fail_lock(player, leader, Follow_Lock,
                T("You're not allowed to follow."), Location(player));
      return;
    }
    /* Ok, looks good */
    add_follow(leader, player, 1);
  } else {
    /* List followers */
    notify_format(player, T("You are following: %s"), list_following(player));
    notify_format(player, T("You are followed by: %s"), list_followers(player));
  }
}

/** The unfollow command.
 * \verbatim
 * unfollow <arg> removes someone from your following list
 * unfollow alone removes everyone from your following list.
 * \endverbatim
 * \param player the enactor.
 * \param arg object to stop following.
 */
void
do_unfollow(dbref player, const char *arg)
{
  dbref leader;
  if (arg && *arg) {
    /* Who do we want to stop following? */
    leader = match_result(player, arg, NOTYPE, MAT_OBJECTS);
    if (leader == AMBIGUOUS) {
      notify(player, T("I can't tell which one to stop following."));
      return;
    }
    if (!GoodObject(leader)) {
      notify(player, T("I don't see that here."));
      return;
    }
    /* Are we following them? */
    if (!is_following(player, leader)) {
      notify_format(player, T("You're not following %s."), Name(leader));
      return;
    }
    /* Ok, looks good */
    del_follow(leader, player, 1);
  } else {
    /* Stop following everyone */
    clear_following(player, 1);
    notify(player, T("You stop following anyone."));
  }
}


/** The dismiss command.
 * \verbatim
 * dismiss <arg> removes someone from your followers list
 * dismiss alone removes everyone from your followers list.
 * \endverbatim
 * \param player the enactor.
 * \param arg name of object to dismiss.
 */
void
do_dismiss(dbref player, const char *arg)
{
  dbref follower;
  if (arg && *arg) {
    /* Who do we want to stop leading? */
    follower = match_result(player, arg, NOTYPE, MAT_OBJECTS);
    if (!GoodObject(follower)) {
      notify(player, T("I don't recognize who you want to dismiss."));
      return;
    }
    /* Are we leading them? */
    if (!is_following(follower, player)) {
      notify_format(player, T("%s isn't following you."), Name(follower));
      return;
    }
    /* Ok, looks good */
    del_follow(player, follower, 1);
  } else {
    /* Stop leading everyone */
    clear_followers(player, 1);
    notify(player, T("You dismiss all your followers."));
  }
}

/** The desert command.
 * \verbatim
 * desert <arg> removes someone from your followers and following list
 * desert alone removes everyone from both lists
 * \endverbatim
 * \param player the enactor.
 * \param arg name of object to desert.
 */
void
do_desert(dbref player, const char *arg)
{
  dbref who;
  if (arg && *arg) {
    /* Who do we want to stop leading? */
    who = match_result(player, arg, NOTYPE, MAT_OBJECTS);
    if (!GoodObject(who)) {
      notify(player, T("I don't recognize who you want to desert."));
      return;
    }
    /* Are we following or leading them? */
    if (!is_following(who, player)
        && !is_following(player, who)) {
      notify_format(player,
                    T("%s isn't following you, nor vice versa."), Name(who));
      return;
    }
    /* Ok, looks good */
    del_follow(player, who, 1);
    del_follow(who, player, 1);
  } else {
    /* Stop leading everyone */
    clear_followers(player, 1);
    clear_following(player, 1);
    notify(player, T("You desert everyone you're leading or following."));
  }
}

/* Add someone to a player's FOLLOWERS attribute */
static void
add_follower(dbref leader, dbref follower)
{
  ATTR *a;
  char tbuf1[BUFFER_LEN];
  char *bp;
  a = atr_get_noparent(leader, "FOLLOWERS");
  if (!a) {
    (void) atr_add(leader, "FOLLOWERS", unparse_dbref(follower), GOD, 0);
  } else {
    bp = tbuf1;
    safe_str(atr_value(a), tbuf1, &bp);
    safe_chr(' ', tbuf1, &bp);
    safe_dbref(follower, tbuf1, &bp);
    *bp = '\0';
    (void) atr_add(leader, "FOLLOWERS", tbuf1, GOD, 0);
  }
}

/* Add someone to a player's FOLLOWING attribute */
static void
add_following(dbref follower, dbref leader)
{
  ATTR *a;
  char tbuf1[BUFFER_LEN];
  char *bp;
  a = atr_get_noparent(follower, "FOLLOWING");
  if (!a) {
    (void) atr_add(follower, "FOLLOWING", unparse_dbref(leader), GOD, 0);
  } else {
    bp = tbuf1;
    safe_str(atr_value(a), tbuf1, &bp);
    safe_chr(' ', tbuf1, &bp);
    safe_dbref(leader, tbuf1, &bp);
    *bp = '\0';
    (void) atr_add(follower, "FOLLOWING", tbuf1, GOD, 0);
  }
}

static void
add_follow(dbref leader, dbref follower, int noisy)
{
  char msg[BUFFER_LEN];
  add_follower(leader, follower);
  add_following(follower, leader);
  if (noisy) {
    strcpy(msg, tprintf(T("You begin following %s."), Name(leader)));
    notify_format(leader, T("%s begins following you."), Name(follower));
    did_it(follower, leader, "FOLLOW", msg, "OFOLLOW", NULL,
           "AFOLLOW", NOTHING);
  }
}


/* Delete someone from a player's FOLLOWERS attribute */
static void
del_follower(dbref leader, dbref follower)
{
  ATTR *a;
  char tbuf1[BUFFER_LEN];
  char flwr[BUFFER_LEN];
  a = atr_get_noparent(leader, "FOLLOWERS");
  if (!a)
    return;                     /* No followers, so no deletion */
  /* Let's take it apart and put it back together w/o follower */
  strcpy(flwr, unparse_dbref(follower));
  strcpy(tbuf1, atr_value(a));
  (void) atr_add(leader, "FOLLOWERS", remove_word(tbuf1, flwr, ' '), GOD, 0);
}

/* Delete someone from a player's FOLLOWING attribute */
static void
del_following(dbref follower, dbref leader)
{
  ATTR *a;
  char tbuf1[BUFFER_LEN];
  char ldr[BUFFER_LEN];
  a = atr_get_noparent(follower, "FOLLOWING");
  if (!a)
    return;                     /* Not following, so no deletion */
  /* Let's take it apart and put it back together w/o leader */
  strcpy(ldr, unparse_dbref(leader));
  strcpy(tbuf1, atr_value(a));
  (void) atr_add(follower, "FOLLOWING", remove_word(tbuf1, ldr, ' '), GOD, 0);
}

static void
del_follow(dbref leader, dbref follower, int noisy)
{
  char msg[BUFFER_LEN];
  del_follower(leader, follower);
  del_following(follower, leader);
  if (noisy) {
    strcpy(msg, tprintf(T("You stop following %s."), Name(leader)));
    notify_format(leader, T("%s stops following you."), Name(follower));
    did_it(follower, leader, "UNFOLLOW", msg, "OUNFOLLOW",
           NULL, "AUNFOLLOW", NOTHING);
  }
}

/* Return a list of names of players who are my followers, comma-separated */
static char *
list_followers(dbref player)
{
  ATTR *a;
  char tbuf1[BUFFER_LEN];
  char *s, *sp;
  static char buff[BUFFER_LEN];
  char *bp;
  dbref who;
  int first = 1;
  a = atr_get_noparent(player, "FOLLOWERS");
  if (!a)
    return (char *) "";
  strcpy(tbuf1, atr_value(a));
  bp = buff;
  s = trim_space_sep(tbuf1, ' ');
  while (s) {
    sp = split_token(&s, ' ');
    who = parse_dbref(sp);
    if (GoodObject(who)) {
      if (!first)
        safe_str(", ", buff, &bp);
      safe_str(Name(who), buff, &bp);
      first = 0;
    }
  }
  *bp = '\0';
  return buff;
}

/* Return a list of names of players who I'm following, comma-separated */
static char *
list_following(dbref player)
{
  ATTR *a;
  char tbuf1[BUFFER_LEN];
  char *s, *sp;
  static char buff[BUFFER_LEN];
  char *bp;
  dbref who;
  int first = 1;
  a = atr_get_noparent(player, "FOLLOWING");
  if (!a)
    return (char *) "";
  strcpy(tbuf1, atr_value(a));
  bp = buff;
  s = trim_space_sep(tbuf1, ' ');
  while (s) {
    sp = split_token(&s, ' ');
    who = parse_dbref(sp);
    if (GoodObject(who)) {
      if (!first)
        safe_str(", ", buff, &bp);
      safe_str(Name(who), buff, &bp);
      first = 0;
    }
  }
  *bp = '\0';
  return buff;
}

/* Is follower following leader? */
static int
is_following(dbref follower, dbref leader)
{
  ATTR *a;
  char *s, *sp;
  char tbuf1[BUFFER_LEN];
  /* There are probably fewer dbrefs on the follower's FOLLOWING list
   * than the leader's FOLLOWERS list, so we check the former
   */
  a = atr_get_noparent(follower, "FOLLOWING");
  if (!a)
    return 0;                   /* Following no one */
  strcpy(tbuf1, atr_value(a));
  s = trim_space_sep(tbuf1, ' ');
  while (s) {
    sp = split_token(&s, ' ');
    if (parse_dbref(sp) == leader)
      return 1;
  }
  return 0;
}

/** Clear a player's followers list.
 * \param leader dbref of player whose list is to be cleared.
 * \param noisy if 1, notify the player.
 */
void
clear_followers(dbref leader, int noisy)
{
  ATTR *a;
  char *s, *sp;
  char tbuf1[BUFFER_LEN];
  dbref flwr;
  a = atr_get_noparent(leader, "FOLLOWERS");
  if (!a)
    return;                     /* No one's following me */
  strcpy(tbuf1, atr_value(a));
  s = trim_space_sep(tbuf1, ' ');
  while (s) {
    sp = split_token(&s, ' ');
    flwr = parse_dbref(sp);
    if (GoodObject(flwr)) {
      del_following(flwr, leader);
      if (noisy)
        notify_format(flwr, T("You stop following %s."), Name(leader));
    }
  }
  (void) atr_clr(leader, "FOLLOWERS", GOD);
}

/** Clear a player's following list.
 * \param follower dbref of player whose list is to be cleared.
 * \param noisy if 1, notify the player.
 */
void
clear_following(dbref follower, int noisy)
{
  ATTR *a;
  char *s, *sp;
  char tbuf1[BUFFER_LEN];
  dbref ldr;
  a = atr_get_noparent(follower, "FOLLOWING");
  if (!a)
    return;                     /* I'm not following anyone */
  strcpy(tbuf1, atr_value(a));
  s = trim_space_sep(tbuf1, ' ');
  while (s) {
    sp = split_token(&s, ' ');
    ldr = parse_dbref(sp);
    if (GoodObject(ldr)) {
      del_follower(ldr, follower);
      if (noisy)
        notify_format(ldr, T("%s stops following you."), Name(follower));
    }
  }
  (void) atr_clr(follower, "FOLLOWING", GOD);
}

/* For all of a leader's followers who are in the same room as the
 * leader, run the same command the leader just ran.
 */
static void
follower_command(dbref leader, dbref loc, const char *com, dbref toward)
{
  dbref follower;
  ATTR *a;
  char *s, *sp;
  char tbuf1[BUFFER_LEN];
  char combuf[BUFFER_LEN];
  if (!com || !*com)
    return;
  if (toward != NOTHING)
    sprintf(combuf, "%s #%d", com, toward);
  else
    strcpy(combuf, com);
  a = atr_get_noparent(leader, "FOLLOWERS");
  if (!a)
    return;                     /* No followers */
  strcpy(tbuf1, atr_value(a));
  s = tbuf1;
  while (s) {
    sp = split_token(&s, ' ');
    follower = parse_dbref(sp);
    if (GoodObject(follower) && (Location(follower) == loc)
        && (Connected(follower) || IsThing(follower))
        && (!(DarkLegal(leader)
              || (Dark(Location(follower)) && !Light(leader)))
            || See_All(follower))) {
      /* This is a follower who was in the room with the leader. Follow. */
      notify_format(follower, T("You follow %s."), Name(leader));
      parse_que(follower, leader, combuf, NULL);
    }
  }
}
