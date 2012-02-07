/**
 * \file destroy.c
 *
 * \brief Destroying objects and consistency checking.
 *
 * This file has two main parts. One part is the functions for destroying
 * objects and getting objects off of the free list. The major public
 * functions here are do_destroy(), free_get(), and purge().
 *
 * The other part is functions for checking the consistency of the
 * database, and repairing any inconsistencies that are found. The
 * major function in this group is dbck().
 *
 *
 * These lengthy comments are by Ralph Melton, December 1995.
 *
 * First, a discourse on the theory of how we handle destruction.
 *
 * We want to maintain the following invariants:
 * 1. All destroyed objects are on the free list. (linked through the next
 *    fields.)
 * 2. All objects on the free list are destroyed objects.
 * 3. No undestroyed object has its next, contents, location, or home
 *    field pointing to a destroyed object.
 * 4. No object's zone or parent is a destroyed object.
 * 5. No object's owner is a destroyed object.
 *
 * For the sake of efficiency, we allow indirect locks and other locks to
 * refer to destroyed objects; boolexp.c had better be able to cope with
 * these.
 *
 *
 * There are three logically distinct parts to destroying an object:
 *
 * Part 1: we do all the permissions checks, check for the SAFE flag
 * and the override switch, and decide that yes, we are going to destroy
 * this object.
 *
 * Part 2: we eliminate all the links from other objects in the database
 * to this object. This processing may depend on the object's type.
 *
 * Part 3: (logically concurrent with part 2, and must happen together
 * with part 2) Remove any commands the object may have in the queue,
 * free all the storage associated with the object, set the name to
 * 'Garbage', and set this object to be a destroyed object, and put it
 * on the free list. This process is independent of object type.
 *
 * Note that phases 2 and 3 do not have to happen immediately after Phase 1.
 * To allow some delay, we set the object GOING, and then process it on
 * the check that happens every ten minutes.
 *
 */

#include "config.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "copyrite.h"
#include "conf.h"
#include "mushdb.h"
#include "match.h"
#include "externs.h"
#include "log.h"
#include "game.h"
#include "extmail.h"
#include "malias.h"
#include "attrib.h"
#include "dbdefs.h"
#include "flags.h"
#include "lock.h"
#include "confmagic.h"
#include "parse.h"



dbref first_free = NOTHING;   /**< Object at top of free list */

static dbref what_to_destroy(dbref player, char *name, int confirm,
                             NEW_PE_INFO *pe_info);
static void pre_destroy(dbref player, dbref thing);
static void free_object(dbref thing);
static void empty_contents(dbref thing);
static void clear_thing(dbref thing);
static void clear_player(dbref thing);
static void clear_room(dbref thing);
static void clear_exit(dbref thing);

static void check_fields(void);
static void check_connected_rooms(void);
static void mark_connected(dbref loc);
static void check_connected_marks(void);
static void mark_contents(dbref loc);
static void check_contents(void);
static void check_locations(void);
static void check_zones(void);
static int attribute_owner_helper
  (dbref player, dbref thing, dbref parent, char const *pattern, ATTR *atr,
   void *args);

extern void remove_all_obj_chan(dbref thing);
extern void chan_chownall(dbref old, dbref new);

extern struct db_stat_info current_state;

/** Mark an object */
#define SetMarked(x)    Type(x) |= TYPE_MARKED
/** Unmark an object */
#define ClearMarked(x)  Type(x) &= ~TYPE_MARKED


/* Section I: do_destroy() and related functions. This section is where
 * the human interface of do_destroy() should largely be determined.
 */

/* Methinks it's time to consider human interfaces criteria for destruction
 * as well as to consider the invariants that need to be maintained.
 *
 * My major criteria are these (with no implied ranking, since I haven't
 * decided how they balance out):
 *
 * 1) It's easy to destroy things you intend to destroy.
 *
 * 2) It's easy to correct from destroying things that you don't intend
 * to destroy. This includes both typos and realizing that you didn't mean
 * to destroy that. This principle requires two sub-principles:
 *      a) The player gets notified when something 'important' gets
 *         marked to be destroyed--and gets told .what. is marked.
 *      b) The actual destruction of the important thing is delayed
 *         long enough that you can recover from it.
 *
 * 3) You can't destroy something you don't have the proper privileges to
 * destroy. (Obvious, but still worth writing down.)
 *
 * To try to achieve a reasonable balance between items 1) and 2), we
 * have the following design:
 * Everything is finally destroyed on the second purge after the @destroy
 * command is done, unless it is set !GOING in the meantime.
 * @destroying an object while it is set GOING destroys it immediately.
 *
 * Let me introduce a little jargon for this discussion:
 *      pre-destroying an object == setting it GOING, running the @adestroy.
 *              (Pre-destroying corresponds to phase 1 above.)
 *      purging an object == actually irrevocably making it vanish.
 *              (This corresponds to phases 2 and 3 above.)
 *      undestroying an object == setting it !GOING, etc.
 *
 * We would also like to have an @adestroy attribute that contains
 * code to be executed when the object is destroyed. This is
 * complicated by the fact that the object is going to be
 * destroyed. To work around this, we run the @adestroy when the
 * object is pre-destroyed, not when it's actually purged. This
 * introduces the possibility that the adestroy may be invoked for
 * something that is then undestroyed. To compensate for that, we run
 * the @startup attribute when something is undestroyed.
 *
 * Another issue is how to run the @adestroy for objects that are
 * destroyed as a consequence of other objects being destroyed. For
 * example, when rooms are destroyed, any exits leading from those
 * rooms are also destroyed, and when a player is destroyed, !SAFE
 * objects they own may also be destroyed.
 *
 * To handle this, we do the following:
 * pre-destroying a room pre-destroys all its exits.
 * pre-destroying a player pre-destroys all the objects that will be purged
 * when that player is purged.
 *
 * This requires the following about undestroys:
 * undestroying an exit undestroys its source room.
 * undestroying any object requires undestroying its owner.
 *
 * But it also seems to require the following in order to make '@destroy
 * foo; @undestroy foo' a no-op for all foo:
 * undestroying a room undestroys all its exits.
 * undestroying a player undestroys all its GOING things.
 *
 * Now, consider this scenario:
 * Player A owns room #1. Player B owns exit #2, whose source is room #1.
 * Player B owns thing #3. Player A and player B are both pre-destroyed;
 * none of the objects are set SAFE. Thing #3 is then undestroyed.
 *
 * If you trace through the dependencies, you find that this involves
 * undestroying all the objects, including both players! Is that what
 * we want? It seems to me that it would be very surprising in practice.
 *
 * To reconcile this, we introduce the following compromise.
 * undestroying a room undestroys all exits in the room that are not owned
 *      by a GOING player or set SAFE..
 * undestroying a player undestroys all objects he owns that are not exits
 *      in a GOING room that he does not own.
 *
 * In this way, the propagation of previous scenario would die out at exit
 * #2, which would stay GOING. Metaphorically, there are two 'votes' for
 * its destruction: the destruction of room #1, and the destruction of
 * player B. Undestroying player B by undestroying thing #3 removes one
 * of the 'votes' for exit #2's destruction, but there would still be
 * the vote from room #1.
 */


/** Determine what object to destroy and if we're allowed.
 * Do all matching and permissions checking. Returns the object to be
 * destroyed if all the permissions checks are successful, otherwise
 * return NOTHING.
 */
static dbref
what_to_destroy(dbref player, char *name, int confirm, NEW_PE_INFO *pe_info)
{
  dbref thing;

  if (Guest(player)) {
    notify(player, T("I'm sorry, Dave, I'm afraid I can't do that."));
    return NOTHING;
  }

  thing = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING);
  if (thing == NOTHING)
    return NOTHING;

  if (IsGarbage(thing)) {
    notify(player, T("Destroying that again is hardly necessary."));
    return NOTHING;
  }
  if (God(thing)) {
    notify(player, T("Destroying God would be blasphemous."));
    return NOTHING;
  }
  /* To destroy, you must either:
   * 1. Control it
   * 2. Control its source or destination if it's an exit
   * 3. Be dealing with a dest-ok thing and pass its lock/destroy
   */
  if (!controls(player, thing) &&
      !(IsExit(thing) &&
        (controls(player, Destination(thing)) ||
         controls(player, Source(thing)))) && !(DestOk(thing)
                                                && eval_lock_with(player, thing,
                                                                  Destroy_Lock,
                                                                  pe_info))) {
    notify(player, T("Permission denied."));
    return NOTHING;
  }
  if (thing == PLAYER_START || thing == MASTER_ROOM || thing == BASE_ROOM ||
      thing == DEFAULT_HOME || God(thing)) {
    notify(player, T("That is too special to be destroyed."));
    return NOTHING;
  }
  if (REALLY_SAFE) {
    if (Safe(thing) && !DestOk(thing)) {
      notify(player,
             T
             ("That object is set SAFE. You must set it !SAFE before destroying it."));
      return NOTHING;
    }
  } else {                      /* REALLY_SAFE */
    if (Safe(thing) && !DestOk(thing) && !confirm) {
      notify(player, T("That object is marked SAFE. Use @nuke to destroy it."));
      return NOTHING;
    }
  }
  /* check to make sure there's no accidental destruction */
  if (!confirm && !Owns(player, thing) && !DestOk(thing)) {
    notify(player,
           T("That object does not belong to you. Use @nuke to destroy it."));
    return NOTHING;
  }
  /* what kind of thing we are destroying? */
  switch (Typeof(thing)) {
  case TYPE_PLAYER:
    if (!IsPlayer(player)) {
      notify(player, T("Programs don't kill people; people kill people!"));
      return NOTHING;
    }
    /* The only player a player can own() is themselves...
     * If they somehow manage to own() another player, they can't
     * nuke that one either...which seems like a good plan, although
     * the error message is a bit confusing. -DTC
     */
    if (!Wizard(player)) {
      notify(player, T("Sorry, no suicide allowed."));
      return NOTHING;
    }
    /* Already checked for God(thing), so use Wizard() */
    if (Wizard(thing) && !God(player)) {
      notify(player, T("Even you can't do that!"));
      return NOTHING;
    }
    if (Connected(thing)) {
      notify(player,
             T("How gruesome. You may not destroy players who are connected."));
      return NOTHING;
    }
    if (!confirm) {
      notify(player, T("You must use @nuke to destroy a player."));
      return NOTHING;
    }
    break;
  case TYPE_THING:
    if (!confirm && Wizard(thing)) {
      notify(player,
             T("That object is set WIZARD. You must use @nuke to destroy it."));
      return NOTHING;
    }
    break;
  case TYPE_ROOM:
    break;
  case TYPE_EXIT:
    break;
  }
  return thing;

}


/** User interface to destroy an object.
 * \verbatim
 * This is the top-level function for @destroy.
 * \endverbatim
 * \param player the enactor.
 * \param name name of object to destroy.
 * \param confirm if 1, called with /override (or nuke).
 */
void
do_destroy(dbref player, char *name, int confirm, NEW_PE_INFO *pe_info)
{
  dbref thing;
  thing = what_to_destroy(player, name, confirm, pe_info);
  if (!GoodObject(thing))
    return;

  /* If thing has already been marked for destruction, go ahead and
   * destroy immediately.
   */
  if (Going(thing)) {
    free_object(thing);
    purge_locks();
    notify(player, T("Destroyed."));
    return;
  }
  /* Present informative messages. */
  if (!REALLY_SAFE && Safe(thing))
    notify(player,
           T
           ("Warning: Target is set SAFE, but scheduling for destruction anyway."));
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    /* wait until dbck */
    notify_except(thing, NOTHING,
                  T("The room shakes and begins to crumble."), NA_SPOOF);
    if (Owns(player, thing))
      notify_format(player,
                    T("You will be rewarded shortly for %s."),
                    object_header(player, thing));
    else {
      notify_format(player,
                    T
                    ("The wrecking ball is on its way for %s's %s and its exits."),
                    Name(Owner(thing)), object_header(player, thing));
      notify_format(Owner(thing),
                    T("%s has scheduled your room %s to be destroyed."),
                    Name(player), object_header(Owner(thing), thing));
    }
    break;
  case TYPE_PLAYER:
    /* wait until dbck */
    notify_format(player,
                  (DESTROY_POSSESSIONS ?
                   (REALLY_SAFE ?
                    T
                    ("%s and all their (non-SAFE) objects are scheduled to be destroyed.")
                    :
                    T
                    ("%s and all their objects are scheduled to be destroyed."))
                   : T("%s is scheduled to be destroyed.")),
                  object_header(player, thing));
    break;
  case TYPE_THING:
    if (!Owns(player, thing)) {
      notify_format(player, T("%s's %s is scheduled to be destroyed."),
                    Name(Owner(thing)), object_header(player, thing));
      if (!DestOk(thing))
        notify_format(Owner(thing),
                      T("%s has scheduled your %s for destruction."),
                      Name(player), object_header(Owner(thing), thing));
    } else {
      notify_format(player, T("%s is scheduled to be destroyed."),
                    object_header(player, thing));
    }
    break;
  case TYPE_EXIT:
    if (!Owns(player, thing)) {
      notify_format(Owner(thing),
                    T("%s has scheduled your %s for destruction."),
                    Name(player), object_header(Owner(thing), thing));
      notify_format(player,
                    T("%s's %s is scheduled to be destroyed."),
                    Name(Owner(thing)), object_header(player, thing));
    } else
      notify_format(player,
                    T("%s is scheduled to be destroyed."),
                    object_header(player, thing));
    break;
  default:
    do_log(LT_ERR, NOTHING, NOTHING, "Surprising type in do_destroy.");
    return;
  }

  pre_destroy(player, thing);
  return;
}


/** Spare an object from slated destruction.
 * \verbatim
 * This is the top-level function for @undestroy.
 * Not undestroy, quite--it's actually 'remove it from its status as about
 * to be destroyed.'
 * \endverbatim
 * \param player the enactor.
 * \param name name of object to be spared.
 */
void
do_undestroy(dbref player, char *name)
{
  dbref thing;
  thing = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(thing)) {
    return;
  }
  if (!controls(player, thing)) {
    notify(player, T("Alas, your efforts of mercy are in vain."));
    return;
  }
  if (undestroy(player, thing)) {
    notify_format(Owner(thing),
                  T("Your %s has been spared from destruction."),
                  object_header(Owner(thing), thing));
    if (player != Owner(thing)) {
      notify_format(player,
                    T("%s's %s has been spared from destruction."),
                    Name(Owner(thing)), object_header(player, thing));
    }
  } else {
    notify(player, T("That can't be undestroyed."));
  }
}



/* Section II: Functions that manage the actual work of destroying
 * Objects.
 */

/* Schedule something to be destroyed, run @adestroy, etc. */
static void
pre_destroy(dbref player, dbref thing)
{
  dbref tmp;
  if (Going(thing) || IsGarbage(thing)) {
    /* we've already covered this thing. No need to do so again. */
    return;
  }
  set_flag_internal(thing, "GOING");
  clear_flag_internal(thing, "GOING_TWICE");

  /* Present informative messages, and do recursive destruction. */
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    DOLIST(tmp, Exits(thing)) {
      pre_destroy(player, tmp);
    }
    break;
  case TYPE_PLAYER:
    if (DESTROY_POSSESSIONS) {
      for (tmp = 0; tmp < db_top; tmp++) {
        if (Owner(tmp) == thing &&
            (tmp != thing) && (!REALLY_SAFE || !Safe(thing))) {
          pre_destroy(player, tmp);
        }
      }
    }
    break;
  case TYPE_THING:
    break;
  case TYPE_EXIT:
    /* This is the only case in which we might end up destroying something
     * whose owner hasn't already been notified. */
    if ((Owner(thing) != Owner(Source(thing))) && Going(Source(thing))) {
      if (!Owns(player, thing)) {
        notify_format(Owner(thing),
                      T("%s has scheduled your %s for destruction."),
                      Name(player), object_header(Owner(thing), thing));
      }
    }
    break;
  default:
    do_log(LT_ERR, NOTHING, NOTHING, "Surprising type in pre_destroy.");
    return;
  }

  if (ADESTROY_ATTR) {
    did_it(player, thing, NULL, NULL, NULL, NULL, "ADESTROY", NOTHING);
  }

  return;

}


/** Spare an object from destruction.
 * Not undestroy, quite--it's actually 'remove it from its status as about
 * to be destroyed.' This is the internal function used in hardcode.
 * \param player the enactor.
 * \param thing dbref of object to be spared.
 * \return 1 successful undestruction.
 * \return 0 thing is not a valid object to undestroy.
 */
int
undestroy(dbref player, dbref thing)
{
  dbref tmp;
  if (!Going(thing) || IsGarbage(thing)) {
    return 0;
  }
  clear_flag_internal(thing, "GOING");
  clear_flag_internal(thing, "GOING_TWICE");
  if (!Halted(thing))
    (void) queue_attribute_noparent(thing, "STARTUP", thing);
  /* undestroy owner, if need be. */
  if (Going(Owner(thing))) {
    if (Owner(thing) != player) {
      notify_format(player,
                    T("%s has been spared from destruction."),
                    object_header(player, Owner(thing)));
      notify_format(Owner(thing),
                    T("You have been spared from destruction by %s."),
                    Name(player));
    } else {
      notify(player, T("You have been spared from destruction."));
    }
    (void) undestroy(player, Owner(thing));
  }
  switch (Typeof(thing)) {
  case TYPE_PLAYER:
    if (DESTROY_POSSESSIONS)
      /* Undestroy all objects owned by players, except exits that are in
       * rooms owned by other players that are set GOING, since those will
       * be purged when the room is purged.
       */
      for (tmp = 0; tmp < db_top; tmp++) {
        if (Owns(thing, tmp) &&
            (tmp != thing) &&
            !(IsExit(tmp) && !Owns(thing, Source(tmp)) && Going(Source(tmp)))) {
          (void) undestroy(player, tmp);
        }
      }
    break;
  case TYPE_THING:
    break;
  case TYPE_EXIT:
    /* undestroy containing room. */
    if (Going(Source(thing))) {
      (void) undestroy(player, Source(thing));
      notify_format(player,
                    T("The room %s has been spared from destruction."),
                    object_header(player, Source(thing)));
      if (Owner(Source(thing)) != player) {
        notify_format(Owner(Source(thing)),
                      T("The room %s has been spared from destruction by %s."),
                      object_header(Owner(Source(thing)), Source(thing)),
                      Name(player));
      }
    }
    break;
  case TYPE_ROOM:
    /* undestroy exits in this room, except exits that are going to be
     * destroyed anyway due to a GOING player.
     */
    DOLIST(tmp, Exits(thing)) {
      if (DESTROY_POSSESSIONS ? (!Going(Owner(tmp)) || Safe(tmp)) : 1) {
        (void) undestroy(player, tmp);
      }
    }
    break;
  default:
    do_log(LT_ERR, NOTHING, NOTHING, "Surprising type in undestroy.");
    return 0;
  }
  return 1;
}


/* Does the real work of freeing all the memory and unlinking an object.
 * This is going to have to be very tightly coupled with the implementation;
 * if the database format changes, this will likely have to change too.
 */
static void
free_object(dbref thing)
{
  dbref i, loc;
  const char *type;
  if (!GoodObject(thing))
    return;
  local_data_free(thing);
  switch (Typeof(thing)) {
  case TYPE_THING:
    type = "THING";
    clear_thing(thing);
    break;
  case TYPE_PLAYER:
    type = "PLAYER";
    clear_player(thing);
    break;
  case TYPE_EXIT:
    type = "EXIT";
    clear_exit(thing);
    break;
  case TYPE_ROOM:
    type = "ROOM";
    clear_room(thing);
    break;
  default:
    do_log(LT_ERR, NOTHING, NOTHING, "Unknown type on #%d in free_object.",
           thing);
    return;
  }
  /* We queue the object-destroy event. Since the event will deal with an
   * object that doesn't exist anymore, we pass it what information we can,
   * but as strings.
   * It's okay to pass it information that will be freed shortly.
   *
   * Information needed:
   *   dbref, type, owner dbref, parent dbref, zone dbref
   */
  queue_event(SYSEVENT, "OBJECT`DESTROY",
              "%s,%s,%s,%s,%s,%s",
              unparse_objid(thing), Name(thing), type,
              unparse_objid(Owner(thing)),
              unparse_objid(Parent(thing)), unparse_objid(Zone(thing)));

  change_quota(Owner(thing), QUOTA_COST);
  do_halt(thing, "", thing);
  /* The equivalent of an @drain/any/all: */
  dequeue_semaphores(thing, NULL, INT_MAX, 1, 1);

  /* if something is zoned or parented or linked or chained or located
   * to/in destroyed object, undo */
  for (i = 0; i < db_top; i++) {
    if (Zone(i) == thing) {
      Zone(i) = NOTHING;
    }
    if (Parent(i) == thing) {
      Parent(i) = NOTHING;
    }
    if (Home(i) == thing) {
      switch (Typeof(i)) {
      case TYPE_PLAYER:
      case TYPE_THING:
        Home(i) = DEFAULT_HOME;
        break;
      case TYPE_EXIT:
        /* Huh.  An exit that claims to be from here, but wasn't linked
         * in properly. */
        do_rawlog(LT_ERR,
                  "ERROR: Exit %s leading from invalid room #%d destroyed.",
                  unparse_object(GOD, i), thing);
        free_object(i);
        break;
      case TYPE_ROOM:
        /* Hrm.  It claims we're an exit from it, but we didn't agree.
         * Clean it up anyway. */
        do_log(LT_ERR, NOTHING, NOTHING,
               "Found a destroyed exit #%d in room #%d", thing, i);
        break;
      }
    }
    /* The location check MUST be done AFTER the home check. */
    if (Location(i) == thing) {
      switch (Typeof(i)) {
      case TYPE_PLAYER:
      case TYPE_THING:
        /* Huh.  It thought it was here, but we didn't agree. */
        moveto(i, Home(i), SYSEVENT, "container destroyed");
        break;
      case TYPE_EXIT:
        /* If our destination is destroyed, then we relink to the
         * source room (so that the exit can't be stolen). Yes, it's
         * inconsistent with the treatment of exits leading from
         * destroyed rooms, but it's a lot better than turning exits
         * into nasty limbo exits.
         */
        Destination(i) = Source(i);
        break;
      case TYPE_ROOM:
        /* Just remove a dropto. */
        Location(i) = NOTHING;
        break;
      }
    }
    if (Next(i) == thing) {
      Next(i) = NOTHING;
    }
  }

  /* chomp chomp */
  atr_free_all(thing);
  List(thing) = NULL;
  /* don't eat name otherwise examine will crash */

  free_locks(Locks(thing));
  Locks(thing) = NULL;

  s_Pennies(thing, 0);
  Owner(thing) = GOD;
  Parent(thing) = NOTHING;
  Zone(thing) = NOTHING;
  remove_all_obj_chan(thing);

  switch (Typeof(thing)) {
    /* Make absolutely sure we are removed from Location's content or
       exit list. If we are in a room we own and destroy_possessions
       is yes, this can happen, causing much ickyness: All garbage
       items would be in DEFAULT_HOME. */
  case TYPE_PLAYER:
  case TYPE_THING:
    loc = Location(thing);
    if (GoodObject(loc))
      Contents(loc) = remove_first(Contents(loc), thing);
    if (Typeof(thing) == TYPE_THING)
      current_state.things--;
    else
      current_state.players--;
    break;
  case TYPE_EXIT:              /* This probably won't be needed, but lets make sure */
    loc = Source(thing);
    if (GoodObject(loc))
      Exits(loc) = remove_first(Exits(loc), thing);
    current_state.exits--;
    break;
  case TYPE_ROOM:
    current_state.rooms--;
    break;
  default:
    /* Do nothing for rooms. */
    break;
  }

  Type(thing) = TYPE_GARBAGE;
  destroy_flag_bitmask("FLAG", Flags(thing));
  Flags(thing) = NULL;
  destroy_flag_bitmask("POWER", Powers(thing));
  Powers(thing) = NULL;
  Location(thing) = NOTHING;
  set_name(thing, "Garbage");
  Exits(thing) = NOTHING;
  Home(thing) = NOTHING;
  CreTime(thing) = 0;           /* Prevents it from matching objids */

  clear_objdata(thing);

  Next(thing) = first_free;
  first_free = thing;

  current_state.garbage++;

}

static void
empty_contents(dbref thing)
{
  /* Destroy any exits they may be carrying, send everything else home. */
  dbref first;
  dbref rest;
  dbref target;
  notify_except(thing, NOTHING,
                T
                ("The floor disappears under your feet, you fall through NOTHINGness and then:"),
                NA_SPOOF);
  first = Contents(thing);
  Contents(thing) = NOTHING;
  /* send all objects to nowhere */
  DOLIST(rest, first) {
    Location(rest) = NOTHING;
  }
  /* now send them home */
  while (first != NOTHING) {
    rest = Next(first);
    /* if home is in thing set it to limbo */
    switch (Typeof(first)) {
    case TYPE_EXIT:            /* if holding exits, destroy it */
      free_object(first);
      break;
    case TYPE_THING:           /* move to home */
    case TYPE_PLAYER:
      /* Make sure the home is a reasonable object. */
      if (!GoodObject(Home(first)) || IsExit(Home(first)) ||
          Home(first) == thing)
        Home(first) = DEFAULT_HOME;
      target = Home(first);
      /* If home isn't a good place to send it, send it to DEFAULT_HOME. */
      if (!GoodObject(target) || recursive_member(target, first, 0))
        target = DEFAULT_HOME;
      if (target != NOTHING) {
        /* Use moveto() on everything so that AENTER and such
         * are all triggered properly. */
        moveto(first, target, SYSEVENT, "container destroyed");
      }
      break;
    }
    first = rest;
  }
}

static void
clear_thing(dbref thing)
{
  dbref loc;
  int a;
  /* Remove object from room's contents */
  loc = Location(thing);
  if (loc != NOTHING) {
    Contents(loc) = remove_first(Contents(loc), thing);
  }
  /* Remove object from any following chains */
  clear_followers(thing, 0);
  clear_following(thing, 0);
  /* give player money back */
  giveto(Owner(thing), (a = Pennies(thing)));
  empty_contents(thing);
  clear_flag_internal(thing, "PUPPET");
  if (!Quiet(thing) && !Quiet(Owner(thing)))
    notify_format(Owner(thing),
                  T("You get your %d %s deposit back for %s."),
                  a, ((a == 1) ? MONEY : MONIES),
                  object_header(Owner(thing), thing));
}

static void
clear_player(dbref thing)
{
  dbref i;
  ATTR *atemp;
  char alias[BUFFER_LEN + 1];

  /* Clear out mail. */
  do_mail_clear(thing, NULL);
  do_mail_purge(thing);
  malias_cleanup(thing);

  /* Chown any chat channels they own to God */
  chan_chownall(thing, GOD);

  /* Clear out names from the player list */
  delete_player(thing, NULL);
  if ((atemp = atr_get_noparent(thing, "ALIAS")) != NULL) {
    strcpy(alias, atr_value(atemp));
    delete_player(thing, alias);
  }
  /* Do all the thing-esque manipulations. */
  clear_thing(thing);

  /* Deal with objects owned by the player. */
  for (i = 0; i < db_top; i++) {
    if (Owner(i) == thing && i != thing) {
      if (DESTROY_POSSESSIONS ? (REALLY_SAFE ? Safe(i) : 0) : 1) {
        chown_object(GOD, i, GOD, 0);
      } else {
        free_object(i);
      }
    }
  }
}

static void
clear_room(dbref thing)
{
  dbref first, rest;
  /* give player money back */
  giveto(Owner(thing), ROOM_COST);
  empty_contents(thing);
  /* Remove exits */
  first = Exits(thing);
  Source(thing) = NOTHING;
  /* set destination of all exits to nothing */
  DOLIST(rest, first) {
    Destination(rest) = NOTHING;
  }
  /* Clear all exits out of exit list */
  while (first != NOTHING) {
    rest = Next(first);
    if (IsExit(first)) {
      free_object(first);
    }
    first = rest;
  }
}


static void
clear_exit(dbref thing)
{
  dbref loc;
  loc = Source(thing);
  if (GoodObject(loc)) {
    Exits(loc) = remove_first(Exits(loc), thing);
  };
  giveto(Owner(thing), EXIT_COST);
}

int
make_first_free_wrapper(dbref player, char *newdbref)
{
  dbref thing;

  if (!newdbref || !*newdbref)
    return 1;

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return 0;
  }
  thing = parse_dbref(newdbref);
  if (thing == NOTHING || !GoodObject(thing) || !IsGarbage(thing)) {
    notify(player, T("That is not a valid dbref."));
    return 0;
  }

  if (!make_first_free(thing)) {
    notify(player, T("Unable to create object with that dbref."));
    return 0;
  }

  return 1;
}

/** If object is in the free list, move it to the very beginning.
 * \param object dbref of object to move
 * \return 1 if object is moved successfully, 0 otherwise
 */
int
make_first_free(dbref object)
{
  dbref curr;
  dbref prev = NOTHING;

  if (first_free == NOTHING || !GoodObject(object) || !IsGarbage(object))
    return 0;                   /* no garbage, or object isn't garbage */
  else if (first_free == object)
    return 1;                   /* object is already at the head of the queue */
  for (curr = first_free; Next(curr); curr = Next(curr)) {
    if (curr == object) {
      Next(prev) = Next(curr);
      Next(curr) = first_free;
      first_free = curr;
      return 1;
    } else
      prev = curr;
  }
  return 0;

}

/** Return a cleaned up object off the free list or NOTHING.
 * \return a garbage object or NOTHING.
 */
dbref
free_get(void)
{
  dbref newobj;
  if (first_free == NOTHING)
    return (NOTHING);
  newobj = first_free;
  first_free = Next(first_free);
  /* Make sure this object really should be in free list */
  if (!IsGarbage(newobj)) {
    static int nrecur = 0;
    dbref temp;
    if (nrecur++ == 20) {
      first_free = NOTHING;
      report();
      do_rawlog(LT_ERR, "ERROR: Removed free list and continued");
      return (NOTHING);
    }
    report();
    do_rawlog(LT_TRACE, "ERROR: Object #%d should not be free", newobj);
    do_rawlog(LT_TRACE, "ERROR: Corrupt free list, fixing");
    fix_free_list();
    temp = free_get();
    nrecur--;
    return (temp);
  }
  /* free object name */
  set_name(newobj, NULL);
  return (newobj);
}

/** Build the free list with a sledgehammer.
 * Only do this when it's actually necessary.
 * Since we only do it if things are corrupted, we do not free any memory.
 * Presumably, this will only waste a reasonable amount of memory, since
 * it's only called in exceptional cases.
 */
void
fix_free_list(void)
{
  dbref thing;
  first_free = NOTHING;
  for (thing = 0; thing < db_top; thing++) {
    if (IsGarbage(thing)) {
      Next(thing) = first_free;
      first_free = thing;
    }
  }
}

/** Destroy all the objects we said we would destroy later. */
void
purge(void)
{
  dbref thing;
  for (thing = 0; thing < db_top; thing++) {
    if (IsGarbage(thing)) {
      continue;
    } else if (Going(thing)) {
      if (Going_Twice(thing)) {
        free_object(thing);
      } else {
        set_flag_internal(thing, "GOING_TWICE");
      }
    } else {
      continue;
    }
  }
  purge_locks();
}


/** Destroy objects slated for destruction.
 * \verbatim
 * This is the top-level function for @purge.
 * \endverbatim
 * \param player the enactor.
 */
void
do_purge(dbref player)
{
  if (Wizard(player)) {
    purge();
    notify(player, T("Purge complete."));
  } else
    notify(player, T("Sorry, you are a mortal."));
}



/* Section III: dbck() and related functions. */

/** The complete db checkup.
 */
void
dbck(void)
{
  check_fields();
  check_contents();
  check_locations();
  check_connected_rooms();
  check_zones();
  local_dbck();
  validate_config();
}

/* Do sanity checks on non-destroyed objects. */
static void
check_fields(void)
{
  dbref thing;
  for (thing = 0; thing < db_top; thing++) {
    if (IsGarbage(thing)) {
      /* The only relevant thing is that the Next field ought to be pointing
       * to a destroyed object.
       */
      dbref next;
      next = Next(thing);
      if ((!GoodObject(next) || !IsGarbage(next)) && (next != NOTHING)) {
        do_rawlog(LT_ERR, "ERROR: Invalid next pointer #%d from object %s",
                  next, unparse_object(GOD, thing));
        Next(thing) = NOTHING;
        fix_free_list();
      }
      continue;
    } else {
      /* Do sanity checks on non-destroyed objects */
      dbref zone, loc, parent, home, owner, next;
      zone = Zone(thing);
      if (GoodObject(zone) && IsGarbage(zone))
        Zone(thing) = NOTHING;
      parent = Parent(thing);
      if (GoodObject(parent) && IsGarbage(parent))
        Parent(thing) = NOTHING;
      owner = Owner(thing);
      if (!GoodObject(owner) || IsGarbage(owner) || !IsPlayer(owner)) {
        do_rawlog(LT_ERR, "ERROR: Invalid object owner on %s(%d)",
                  Name(thing), thing);
        report();
        Owner(thing) = GOD;
      }
      next = Next(thing);
      if ((!GoodObject(next) || IsGarbage(next)) && (next != NOTHING)) {
        do_rawlog(LT_ERR, "ERROR: Invalid next pointer #%d from object %s",
                  next, unparse_object(GOD, thing));
        Next(thing) = NOTHING;
      }
      /* This next bit has to be type-specific because of different uses
       * of the home and location fields.
       */
      home = Home(thing);
      loc = Location(thing);
      switch (Typeof(thing)) {
      case TYPE_PLAYER:
      case TYPE_THING:
        if (!GoodObject(home) || IsGarbage(home) || IsExit(home))
          Home(thing) = DEFAULT_HOME;
        if (!GoodObject(loc) || IsGarbage(loc) || IsExit(loc)) {
          moveto(thing, Home(thing), SYSEVENT, "dbck");
        }
        break;
      case TYPE_EXIT:
        if (Contents(thing) != NOTHING) {
          /* Eww.. Exits can't have contents. Bad news */
          Contents(thing) = NOTHING;
          do_rawlog(LT_ERR,
                    "ERROR: Exit %s has a contents list. Wiping it out.",
                    unparse_object(GOD, thing));
        }
        if (!GoodObject(loc)
            && !((loc == NOTHING) || (loc == AMBIGUOUS) || (loc == HOME))) {
          /* Bad news. We're linked to a really impossible object.
           * Relink to our source
           */
          Destination(thing) = Source(thing);
          do_rawlog(LT_ERR,
                    "ERROR: Exit %s leading to invalid room #%d relinked to its source room.",
                    unparse_object(GOD, thing), home);
        } else if (GoodObject(loc) && IsGarbage(loc)) {
          /* If our destination is destroyed, then we relink to the
           * source room (so that the exit can't be stolen). Yes, it's
           * inconsistent with the treatment of exits leading from
           * destroyed rooms, but it's a lot better than turning exits
           * into nasty limbo exits.
           */
          Destination(thing) = Source(thing);
          do_rawlog(LT_ERR,
                    "ERROR: Exit %s leading to garbage room #%d relinked to its source room.",
                    unparse_object(GOD, thing), home);
        }
        /* This must come last */
        if (!GoodObject(home) || !IsRoom(home)) {
          /* If our home is destroyed, just destroy the exit. */
          do_rawlog(LT_ERR,
                    "ERROR: Exit %s leading from invalid room #%d destroyed.",
                    unparse_object(GOD, thing), home);
          free_object(thing);
        }
        break;
      case TYPE_ROOM:
        if (GoodObject(home) && IsGarbage(home)) {
          /* Eww. Destroyed exit. This isn't supposed to happen. */
          do_log(LT_ERR, NOTHING, NOTHING,
                 "Found a destroyed exit #%d in room #%d", home, thing);
        }
        if (GoodObject(loc) && (IsGarbage(loc) || IsExit(loc))) {
          /* Just remove a dropto. */
          Location(thing) = NOTHING;
        }
        break;
      }
      /* Check attribute ownership. If the attribute is owned by
       * an invalid dbref, change its ownership to God.
       */
      if (!IsGarbage(thing))
        atr_iter_get(GOD, thing, "**", 0, 0, attribute_owner_helper, NULL);
    }
  }
}

static int
attribute_owner_helper(dbref player __attribute__ ((__unused__)),
                       dbref thing __attribute__ ((__unused__)),
                       dbref parent __attribute__ ((__unused__)),
                       char const *pattern
                       __attribute__ ((__unused__)), ATTR *atr, void *args
                       __attribute__ ((__unused__)))
{
  if (!GoodObject(AL_CREATOR(atr)))
    AL_CREATOR(atr) = GOD;
  return 0;
}

static void
check_connected_rooms(void)
{
  dbref room;
  for (room = 0; room < db_top; ++room)
    if ((room == BASE_ROOM) || (IsRoom(room) && Floating(room)))
      mark_connected(room);
  check_connected_marks();
}

static void
mark_connected(dbref loc)
{
  dbref thing;
  if (!GoodObject(loc) || !IsRoom(loc) || Marked(loc))
    return;
  SetMarked(loc);
  /* recursively trace */
  for (thing = Exits(loc); thing != NOTHING; thing = Next(thing))
    mark_connected(Destination(thing));
}

static void
check_connected_marks(void)
{
  dbref loc;
  for (loc = 0; loc < db_top; loc++)
    if (!IsGarbage(loc) && Marked(loc))
      ClearMarked(loc);
    else if (IsRoom(loc)) {
      if (!Name(loc)) {
        do_log(LT_ERR, NOTHING, NOTHING, "ERROR: no name for room #%d.", loc);
        set_name(loc, "XXXX");
      }
      if (!Going(loc) && !Floating(loc) && !NoWarnable(loc) &&
          (!EXITS_CONNECT_ROOMS || (Exits(loc) == NOTHING))) {
        notify_format(Owner(loc), T("You own a disconnected room, %s"),
                      object_header(Owner(loc), loc));
      }
    }
}

/* Warn about objects without @lock/zone used as zones */
static void
check_zones(void)
{
  dbref n, zone = NOTHING, tmp;
  int zone_depth;

  for (n = 0; n < db_top; n++) {
    if (IsGarbage(n))
      continue;
    zone = Zone(n);
    if (!GoodObject(zone))
      continue;
    if (ZONE_CONTROL_ZMP && !IsPlayer(zone))
      continue;
    if (zone != n)              /* Objects can be zoned to themselves */
      for (zone_depth = MAX_ZONES, tmp = Zone(zone);
           zone_depth-- && GoodObject(tmp); tmp = Zone(tmp)) {
        if (tmp == n) {
          notify_format(Owner(n),
                        T("You own an object in a circular zone chain: %s"),
                        object_header(Owner(n), n));
          break;
        }
        if (tmp == Zone(tmp))   /* Object zoned to itself */
          break;
      }

    if (Marked(zone))
      continue;
    if (getlock(zone, Zone_Lock) == TRUE_BOOLEXP)
      SetMarked(zone);
  }

  for (n = 0; n < db_top; n++) {
    if (!IsGarbage(n) && Marked(n)) {
      ClearMarked(n);
      notify_format(Owner(n),
                    T
                    ("You own an object without a @lock/zone being used as a zone: %s"),
                    object_header(Owner(n), n));
    }
  }
}

/** In this macro, field must be an lvalue whose evaluation has
 * no side effects and results in a dbref to be checked.
 * All hell will break loose if this is not so.
 */
#define CHECK(field)            \
  if ((field) != NOTHING) { \
     if (!GoodObject(field) || IsGarbage(field)) { \
       do_rawlog(LT_ERR, "Bad reference #%d from %s severed.", \
                 (field), unparse_object(GOD, thing)); \
       (field) = NOTHING; \
     } else if (IsRoom(field)) { \
       do_rawlog(LT_ERR, "Reference to room #%d from %s severed.", \
                 (field), unparse_object(GOD, thing)); \
       (field) = NOTHING; \
     } else if (Marked(field)) {  \
       do_rawlog(LT_ERR, "Multiple references to %s. Reference from #%d severed.", \
                 unparse_object(GOD, (field)), thing); \
       (field) = NOTHING; \
     } else { \
       SetMarked(field); \
       mark_contents(field); \
     } \
  }

/* An auxiliary function for check_contents. */
static void
mark_contents(dbref thing)
{
  if (!GoodObject(thing) || IsGarbage(thing))
    return;

  SetMarked(thing);
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    CHECK(Exits(thing));
    CHECK(Contents(thing));
    break;
  case TYPE_PLAYER:
  case TYPE_THING:
    CHECK(Contents(thing));
    CHECK(Next(thing));
    break;
  case TYPE_EXIT:
    CHECK(Next(thing));
    break;
  default:
    do_rawlog(LT_ERR, "Bad object type found for %s in mark_contents",
              unparse_object(GOD, thing));
    break;
  }
}

#undef CHECK

/* Check that for every thing, player, and exit, you can trace exactly one
 * path to that object from a room by following the exits field of rooms,
 * the next field of non-rooms, and the contents field of non-exits.
 */
static void
check_contents(void)
{
  dbref thing;
  for (thing = 0; thing < db_top; thing++) {
    if (IsRoom(thing)) {
      mark_contents(thing);
    }
  }
  for (thing = 0; thing < db_top; thing++) {
    if (!IsRoom(thing) && !IsGarbage(thing) && !Marked(thing)) {
      do_rawlog(LT_ERR, "Object %s not pointed to by anything.",
                unparse_object(GOD, thing));
      notify_format(Owner(thing),
                    T("You own an object %s that was \'orphaned\'."),
                    object_header(Owner(thing), thing));
      /* We try to fix this by trying to send players and things to
       * their current location, to their home, or to DEFAULT_HOME, in
       * that order, and relinking exits to their source.
       */
      Next(thing) = NOTHING;
      switch (Typeof(thing)) {
      case TYPE_PLAYER:
      case TYPE_THING:
        if (GoodObject(Location(thing)) &&
            !IsGarbage(Location(thing)) && Marked(Location(thing))) {
          PUSH(thing, Contents(Location(thing)));
        } else if (GoodObject(Home(thing)) &&
                   !IsGarbage(Home(thing)) && Marked(Home(thing))) {
          Contents(Location(thing)) =
            remove_first(Contents(Location(thing)), thing);
          PUSH(thing, Contents(Home(thing)));
          Location(thing) = Home(thing);
        } else {
          Contents(Location(thing)) =
            remove_first(Contents(Location(thing)), thing);
          PUSH(thing, Contents(DEFAULT_HOME));
          Location(thing) = DEFAULT_HOME;
        }
        moveto(thing, Location(thing), SYSEVENT, "dbck");
        /* If we've managed to reconnect it, then we've reconnected
         * its contents. */
        mark_contents(Contents(thing));
        notify_format(Owner(thing), T("It was moved to %s."),
                      object_header(Owner(thing), Location(thing)));
        do_rawlog(LT_ERR, "Moved to %s.", unparse_object(GOD, Location(thing)));
        break;
      case TYPE_EXIT:
        if (GoodObject(Source(thing)) && IsRoom(Source(thing))) {
          PUSH(thing, Exits(Source(thing)));
          notify_format(Owner(thing), T("It was moved to %s."),
                        object_header(Owner(thing), Source(thing)));
          do_rawlog(LT_ERR, "Moved to %s.", unparse_object(GOD, Source(thing)));
        } else {
          /* Just destroy the exit. */
          Source(thing) = NOTHING;
          notify(Owner(thing), T("It was destroyed."));
          do_rawlog(LT_ERR, "Orphaned exit destroyed.");
          free_object(thing);
        }
        break;
      case TYPE_ROOM:
        /* We should never get here. */
        do_log(LT_ERR, NOTHING, NOTHING, "Disconnected room. So what?");
        break;
      default:
        do_log(LT_ERR, NOTHING, NOTHING,
               "Surprising type on #%d found in check_cycles.", thing);
        break;
      }
    }
  }
  for (thing = 0; thing < db_top; thing++) {
    if (!IsGarbage(thing))
      ClearMarked(thing);
  }
}


/* Check that every player and thing occurs in the contents list of its
 * location, and that every exit occurs in the exit list of its source.
 */
static void
check_locations(void)
{
  dbref thing;
  dbref loc;
  for (loc = 0; loc < db_top; loc++) {
    if (!IsExit(loc)) {
      for (thing = Contents(loc); thing != NOTHING; thing = Next(thing)) {
        if (!Mobile(thing)) {
          do_rawlog(LT_ERR,
                    "ERROR: Contents of object %d corrupt at object %d cleared",
                    loc, thing);
          /* Remove this from the list and start over. */
          Contents(loc) = remove_first(Contents(loc), thing);
          thing = Contents(loc);
          continue;
        } else if (Location(thing) != loc) {
          /* Well, it would fit here, and it can't be elsewhere because
           * we've done a check_contents already, so let's just put it
           * here.
           */
          do_rawlog(LT_ERR,
                    "Incorrect location on object %s. Reset to #%d.",
                    unparse_object(GOD, thing), loc);
          Location(thing) = loc;
        }
        SetMarked(thing);
      }
    }
    if (IsRoom(loc)) {
      for (thing = Exits(loc); thing != NOTHING; thing = Next(thing)) {
        if (!IsExit(thing)) {
          do_rawlog(LT_ERR,
                    "ERROR: Exits of room %d corrupt at object %d cleared",
                    loc, thing);
          /* Remove this from the list and start over. */
          Exits(loc) = remove_first(Exits(loc), thing);
          thing = Exits(loc);
          continue;
        } else if (Source(thing) != loc) {
          do_rawlog(LT_ERR,
                    "Incorrect source on exit %s. Reset to #%d.",
                    unparse_object(GOD, thing), loc);
        }
      }
    }
  }


  for (thing = 0; thing < db_top; thing++)
    if (!IsGarbage(thing) && Marked(thing))
      ClearMarked(thing);
    else if (Mobile(thing)) {
      do_rawlog(LT_ERR, "ERROR DBCK: Moved object %d", thing);
      moveto(thing, DEFAULT_HOME, SYSEVENT, "dbck");
    }
}


/** Database checkup, user interface.
 * \verbatim
 * This is the top-level function for @dbck. Automatic checks should
 * call dbck(), not this.
 * \endverbatim
 * \param player the enactor.
 */
void
do_dbck(dbref player)
{
  if (!Wizard(player)) {
    notify(player, T("Silly mortal, chicks are for kids!"));
    return;
  }
  notify(player, T("GAME: Performing database consistency check."));
  do_log(LT_WIZ, player, NOTHING, "DBCK done.");
  dbck();
  notify(player, T("GAME: Database consistency check complete."));
}
