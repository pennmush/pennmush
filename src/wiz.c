/**
 * \file wiz.c
 *
 * \brief Wizard commands in PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <string.h>
#include <math.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef WIN32
#include <windows.h>
#include "process.h"
#endif
#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "match.h"
#include "access.h"
#include "parse.h"
#include "mymalloc.h"
#include "flags.h"
#include "lock.h"
#include "log.h"
#include "game.h"
#include "command.h"
#include "dbdefs.h"
#include "extmail.h"
#include "boolexp.h"
#include "ansi.h"


#include "confmagic.h"

dbref find_entrance(dbref door);
struct db_stat_info *get_stats(dbref owner);
dbref find_player_by_desc(int port);
char *password_hash(const char *password, const char *algo);


#ifndef WIN32
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#endif

/** \@search data */
struct search_spec {
  dbref owner;  /**< Limit to this owner, if specified */
  int type;     /**< Limit to this type */
  dbref parent; /**< Limit to children of this parent */
  dbref zone;   /**< Limit to those in this zone */
  dbref entrances;           /**< Objects linked here, for \@entrances */
  char flags[BUFFER_LEN];    /**< Limit to those with these flags */
  char lflags[BUFFER_LEN];    /**< Limit to those with these flags */
  char powers[BUFFER_LEN];   /**< Limit to those with these powers */
  char eval[BUFFER_LEN];   /**< Limit to those where this evals true */
  char name[BUFFER_LEN];  /**< Limit to those prefix-matching this name */
  dbref low;    /**< Limit to dbrefs here or higher */
  dbref high;   /**< Limit to dbrefs here or lower */
  int start;  /**< Limited results: start at this one. */
  int count;  /**< Limited results: return this many */
  int end;    /**< Limited results: return until this one.*/
  boolexp lock;  /**< Boolexp to check against the objects. */
  char cmdstring[BUFFER_LEN]; /**< Find objects who respond to this $-command */
  char listenstring[BUFFER_LEN]; /**< Find objects who respond to this ^-listen */
};

static int tport_dest_ok(dbref player, dbref victim, dbref dest,
                         NEW_PE_INFO *pe_info);
static int tport_control_ok(dbref player, dbref victim, dbref loc);
static int mem_usage(dbref thing);
static int raw_search(dbref player, struct search_spec *spec,
                      dbref **result, NEW_PE_INFO *pe_info);
static void init_search_spec(struct search_spec *spec);
static int fill_search_spec(dbref player, const char *owner, int nargs,
                            const char **args, struct search_spec *spec);
static void














sitelock_player(dbref player, const char *name, dbref who, uint32_t can,
                uint32_t cant);


#ifdef INFO_SLAVE
void kill_info_slave(void);
#endif

extern char confname[BUFFER_LEN];
extern char errlog[BUFFER_LEN];

/** Create a player by Wizard fiat.
 * \verbatim
 * This implements @pcreate.
 * \endverbatim
 * \param creator the enactor.
 * \param player_name name of player to create.
 * \param player_password password for player.
 * \param try_dbref if non-empty, the garbage object to use for the new player.
 * \return dbref of created player object, or NOTHING if failure.
 */
dbref
do_pcreate(dbref creator, const char *player_name, const char *player_password,
           char *try_dbref)
{
  dbref player;

  if (!Create_Player(creator)) {
    notify(creator, T("You do not have the power over body and mind!"));
    return NOTHING;
  }
  if (!can_pay_fees(creator, 0))
    return NOTHING;

  if (!make_first_free_wrapper(creator, try_dbref)) {
    return NOTHING;
  }

  player =
    create_player(NULL, creator, player_name, player_password, "None", "None");
  if (player == NOTHING) {
    notify_format(creator, T("Failure creating '%s' (bad name)"), player_name);
    return NOTHING;
  }
  if (player == AMBIGUOUS) {
    notify_format(creator, T("Failure creating '%s' (bad password)"),
                  player_name);
    return NOTHING;
  }
  notify_format(creator, T("New player '%s' (#%d) created with password '%s'"),
                player_name, player, player_password);
  do_log(LT_WIZ, creator, player, "Player creation");
  queue_event(creator, "PLAYER`CREATE", "%s,%s,%s",
              unparse_objid(player), Name(player), "pcreate");
  return player;
}

/** Set or check a player's quota.
 * \verbatim
 * This implements @quota and @squota.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 name of player whose quota should be set or checked.
 * \param arg2 amount to set or adjust quota, ignored if checking.
 * \param set_q if 1, set quota; if 0, check quota.
 */
void
do_quota(dbref player, const char *arg1, const char *arg2, int set_q)
{
  dbref who, thing;
  int owned, limit, adjust;

  /* determine the victim */
  if (!arg1 || !*arg1 || !strcmp(arg1, "me"))
    who = player;
  else {
    who = lookup_player(arg1);
    if (who == NOTHING) {
      notify(player, T("No such player."));
      return;
    }
  }

  /* check permissions */
  if (!Wizard(player) && set_q) {
    notify(player, T("Only wizards may change a quota."));
    return;
  }
  if (!Do_Quotas(player) && !See_All(player) && (player != who)) {
    notify(player, T("You can't look at someone else's quota."));
    return;
  }
  /* count up all owned objects */
  owned = -1;                   /* a player is never included in his own
                                 * quota */
  for (thing = 0; thing < db_top; thing++) {
    if (Owner(thing) == who)
      if (!IsGarbage(thing))
        ++owned;
  }

  /* the quotas of priv'ed players are unlimited and cannot be set. */
  if (NoQuota(who) || !USE_QUOTA) {
    notify_format(player, T("Objects: %d   Limit: UNLIMITED"), owned);
    return;
  }
  /* if we're not doing a change, determine the mortal's quota limit.
   * RQUOTA is the objects _left_, not the quota itself.
   */

  if (!set_q) {
    limit = get_current_quota(who);
    notify_format(player, T("Objects: %d   Limit: %d"), owned, owned + limit);
    return;
  }
  /* set a new quota */
  if (!arg2 || !*arg2) {
    limit = get_current_quota(who);
    notify_format(player, T("Objects: %d   Limit: %d"), owned, owned + limit);
    notify(player, T("What do you want to set the quota to?"));
    return;
  }
  adjust = ((*arg2 == '+') || (*arg2 == '-'));
  if (adjust)
    limit = owned + get_current_quota(who) + atoi(arg2);
  else
    limit = atoi(arg2);
  if (limit < owned)            /* always have enough quota for your objects */
    limit = owned;

  (void) atr_add(Owner(who), "RQUOTA", tprintf("%d", limit - owned), GOD, 0);

  notify_format(player, T("Objects: %d   Limit: %d"), owned, limit);
}


/** Check or set quota globally.
 * \verbatim
 * This implements @allquota.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 new quota limit, as a string.
 * \param quiet if 1, don't display every player's quota.
 */
void
do_allquota(dbref player, const char *arg1, int quiet)
{
  int oldlimit, limit, owned;
  dbref who, thing;

  if (!God(player)) {
    notify(player, T("Who do you think you are, GOD?"));
    return;
  }
  if (!arg1 || !*arg1) {
    limit = -1;
  } else if (!is_strict_integer(arg1)) {
    notify(player, T("You can only set quotas to a number."));
    return;
  } else {
    limit = parse_integer(arg1);
    if (limit < 0) {
      notify(player, T("You can only set quotas to a positive number."));
      return;
    }
  }

  for (who = 0; who < db_top; who++) {
    if (!IsPlayer(who))
      continue;

    /* count up all owned objects */
    owned = -1;                 /* a player is never included in his own
                                 * quota */
    for (thing = 0; thing < db_top; thing++) {
      if (Owner(thing) == who)
        if (!IsGarbage(thing))
          ++owned;
    }

    if (NoQuota(who)) {
      if (!quiet)
        notify_format(player, T("%s: Objects: %d   Limit: UNLIMITED"),
                      Name(who), owned);
      continue;
    }
    if (!quiet) {
      oldlimit = get_current_quota(who);
      notify_format(player, T("%s: Objects: %d   Limit: %d"),
                    Name(who), owned, oldlimit);
    }
    if (limit != -1) {
      if (limit <= owned)
        (void) atr_add(who, "RQUOTA", "0", GOD, 0);
      else
        (void) atr_add(who, "RQUOTA", tprintf("%d", limit - owned), GOD, 0);
    }
  }
  if (limit == -1)
    notify(player, T("Quotas not changed."));
  else
    notify_format(player, T("All quotas changed to %d."), limit);
}

static int
tport_dest_ok(dbref player, dbref victim, dbref dest, NEW_PE_INFO *pe_info)
{
  /* can player legitimately send something to dest */

  if (Tel_Anywhere(player))
    return 1;

  if (controls(player, dest))
    return 1;

  /* beyond this point, if you don't control it and it's not a room, no hope */
  if (!IsRoom(dest))
    return 0;

  /* Check for a teleport lock. It fails if the player is not wiz or
   * royalty, and the room is tport-locked against the victim, and the
   * victim does not control the room.
   */
  if (!eval_lock_with(victim, dest, Tport_Lock, pe_info))
    return 0;

  if (JumpOk(dest))
    return 1;

  return 0;
}

static int
tport_control_ok(dbref player, dbref victim, dbref loc)
{
  /* can player legitimately move victim from loc */

  if (God(victim) && !God(player))
    return 0;

  if (Tel_Anything(player))
    return 1;

  if (controls(player, victim))
    return 1;

  /* mortals can't @tel HEAVY players just on basis of location ownership */

  if (controls(player, loc) && (!Heavy(victim) || Owns(player, victim)))
    return 1;

  return 0;
}

/** Teleport something somewhere.
 * \verbatim
 * This implements @tel.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 the object to teleport (or location if no object given)
 * \param arg2 the location to teleport to.
 * \param silent if 1, don't trigger teleport messagse.
 * \param inside if 1, always \@tel to inventory, even of a player
 * \param pe_info the pe_info for lock checks, etc
 */
void
do_teleport(dbref player, const char *arg1, const char *arg2, int silent,
            int inside, NEW_PE_INFO *pe_info)
{
  dbref victim;
  dbref destination;
  dbref loc;
  const char *to;
  dbref absroom;                /* "absolute room", for NO_TEL check */

  /* get victim, destination */
  if (*arg2 == '\0') {
    victim = player;
    to = arg1;
  } else {
    if ((victim =
         noisy_match_result(player, arg1, NOTYPE,
                            MAT_OBJECTS | MAT_ENGLISH)) == NOTHING) {
      return;
    }
    to = arg2;
  }

  if (IsRoom(victim)) {
    notify(player, T("You can't teleport rooms."));
    return;
  }
  if (IsGarbage(victim)) {
    notify(player, T("Garbage belongs in the garbage dump."));
    return;
  }
  /* get destination */

  if (!strcasecmp(to, "home")) {
    /* If the object is @tel'ing itself home, treat it the way we'd
     * treat a 'home' command
     */
    if (player == victim) {
      if (command_check_byname(victim, "HOME", NULL))
        safe_tel(victim, HOME, silent, player, "teleport");
      return;
    } else {
      destination = Home(victim);
    }
  } else {
    destination = match_result(player, to, NOTYPE, MAT_EVERYTHING);
  }

  switch (destination) {
  case NOTHING:
    notify(player, T("No match."));
    break;
  case AMBIGUOUS:
    notify(player, T("I don't know which destination you mean!"));
    break;
  case HOME:
    destination = Home(victim);
    /* FALL THROUGH */
  default:
    /* check victim, destination types, teleport if ok */
    if (!GoodObject(destination) || IsGarbage(destination)) {
      notify(player, T("Bad destination."));
      return;
    }
    if (recursive_member(destination, victim, 0)
        || (victim == destination)) {
      notify(player, T("Bad destination."));
      return;
    }
    if (!Tel_Anywhere(player) && IsPlayer(victim) && IsPlayer(destination)) {
      notify(player, T("Bad destination."));
      return;
    }
    if (IsExit(victim)) {
      /* Teleporting an exit means moving its source */
      if (!IsRoom(destination)) {
        notify(player, T("Exits can only be teleported to other rooms."));
        return;
      }
      if (Going(destination)) {
        notify(player,
               T("You can't move an exit to someplace that's crumbling."));
        return;
      }
      if (!GoodObject(Home(victim)))
        loc = find_entrance(victim);
      else
        loc = Home(victim);
      /* Unlike normal teleport, you must control the destination
       * or have the open_anywhere power
       */
      if (!tport_control_ok(player, victim, loc) ||
          !can_open_from(player, destination, pe_info)) {
        notify(player, T("Permission denied."));
        return;
      }
      /* Remove it from its old room */
      Exits(loc) = remove_first(Exits(loc), victim);
      /* Put it into its new room */
      Source(victim) = destination;
      PUSH(victim, Exits(destination));
      if (!Quiet(player) && !(Quiet(victim) && (Owner(victim) == player)))
        notify(player, T("Teleported."));
      return;
    }
    loc = Location(victim);

    /* if royal or wiz and destination is player, tel to location unless
     * using @tel/inside
     */
    if (IsPlayer(destination) && Tel_Anywhere(player) && IsPlayer(victim)
        && !inside) {
      if (!silent && loc != Location(destination))
        did_it_with(victim, victim, NULL, NULL, "OXTPORT", NULL, NULL, loc,
                    player, NOTHING, NA_INTER_HEAR);
      safe_tel(victim, Location(destination), silent, player, "teleport");
      if (!silent && loc != Location(destination))
        did_it_with(victim, victim, "TPORT", NULL, "OTPORT", NULL, "ATPORT",
                    Location(destination), player, loc, NA_INTER_HEAR);
      return;
    }
    /* check needed for NOTHING. Especially important for unlinked exits */
    if ((absroom = Location(victim)) == NOTHING) {
      notify(victim, T("You're in the Void. This is not a good thing."));
      /* At this point, they're in a bad location, so let's check
       * if home is valid before sending them there. */
      if (!GoodObject(Home(victim)))
        Home(victim) = PLAYER_START;
      do_move(victim, "home", MOVE_NORMAL, pe_info);
      return;
    } else {
      /* valid location, perform other checks */

      /* if player is inside himself, send him home */
      if (absroom == victim) {
        notify(player, T("What are you doing inside of yourself?"));
        if (Home(victim) == absroom)
          Home(victim) = PLAYER_START;
        do_move(victim, "home", MOVE_NORMAL, pe_info);
        return;
      }
      /* find the "absolute" room */
      absroom = absolute_room(victim);

      if (absroom == NOTHING) {
        notify(victim, T("You're in the void - sending you home."));
        if (Home(victim) == Location(victim))
          Home(victim) = PLAYER_START;
        do_move(victim, "home", MOVE_NORMAL, pe_info);
        return;
      }
      /* if there are a lot of containers, send him home */
      if (absroom == AMBIGUOUS) {
        notify(victim, T("You're in too many containers."));
        if (Home(victim) == Location(victim))
          Home(victim) = PLAYER_START;
        do_move(victim, "home", MOVE_NORMAL, pe_info);
        return;
      }
      /* note that we check the NO_TEL status of the victim rather
       * than the player that issued the command. This prevents someone
       * in a NO_TEL room from having one of his objects @tel him out.
       * The control check, however, is detemined by command-giving
       * player. */

      /* now check to see if the absolute room is set NO_TEL */
      if (NoTel(absroom) && !controls(player, absroom)
          && !Tel_Anywhere(player)) {
        notify(player, T("Teleports are not allowed in this room."));
        return;
      }

      /* Check leave lock on room, if necessary */
      if (!controls(player, absroom) && !Tel_Anywhere(player) &&
          !eval_lock_with(player, absroom, Leave_Lock, pe_info)) {
        fail_lock(player, absroom, Leave_Lock,
                  T("Teleports are not allowed in this room."), NOTHING);
        return;
      }

      /* Now check the Z_TEL status of the victim's room.
       * Just like NO_TEL above, except that if the room (or its
       * Zone Master Room, if any) is Z_TEL,
       * the destination must also be a room in the same zone
       */
      if (GoodObject(Zone(absroom)) && (ZTel(absroom) || ZTel(Zone(absroom)))
          && !controls(player, absroom) && !Tel_Anywhere(player)
          && (Zone(absroom) != Zone(destination))) {
        notify(player,
               T("You may not teleport out of the zone from this room."));
        return;
      }
    }

    if (!IsExit(destination)) {
      if (tport_control_ok(player, victim, Location(victim)) &&
          tport_dest_ok(player, victim, destination, pe_info)
          && (Tel_Anything(player) ||
              (Tel_Anywhere(player) && (player == victim)) ||
              (destination == Owner(victim)) ||
              (!Fixed(Owner(victim)) && !Fixed(player)))) {
        if (!silent && loc != destination)
          did_it_with(victim, victim, NULL, NULL, "OXTPORT", NULL, NULL, loc,
                      player, NOTHING, NA_INTER_HEAR);
        safe_tel(victim, destination, silent, player, "teleport");
        if (!silent && loc != destination)
          did_it_with(victim, victim, "TPORT", NULL, "OTPORT", NULL, "ATPORT",
                      destination, player, loc, NA_INTER_HEAR);
        if ((victim != player) && !(Puppet(victim) &&
                                    (Owner(victim) == Owner(player)))) {
          if (!Quiet(player) && !(Quiet(victim) && (Owner(victim) == player)))
            notify(player, T("Teleported."));
        }
        return;
      }
      /* we can't do it */
      fail_lock(player, destination, Enter_Lock, T("Permission denied."),
                Location(player));
      return;
    } else {
      /* attempted teleport to an exit */
      if (!tport_control_ok(player, victim, Location(victim))) {
        notify(player, T("Permission denied."));
        if (victim != player)
          notify_format(victim,
                        T("%s tries to impose his will on you and fails."),
                        Name(player));
        return;
      }
      if (Fixed(Owner(victim)) || Fixed(player)) {
        notify(player, T("Permission denied."));
        return;
      }
      if (!Tel_Anywhere(player) && !controls(player, destination) &&
          !nearby(player, destination) && !nearby(victim, destination)) {
        notify(player, T("Permission denied."));
        return;
      } else {
        char absdest[SBUF_LEN];
        strcpy(absdest, tprintf("#%d", destination));
        do_move(victim, absdest, MOVE_TELEPORT, pe_info);
      }
    }
  }
}

/** Force an object to run a command.
 * \verbatim
 * This implements @force.
 * \endverbatim
 * \param player the enactor.
 * \param caller the caller.
 * \param what name of the object to force.
 * \param command command to force the object to run.
 * \param queue_type QUEUE_* flags for the type of queue to run
 * \param queue_entry the queue_entry the command was run in
 */
void
do_force(dbref player, dbref caller, const char *what, char *command,
         int queue_type, MQUE *queue_entry)
{
  dbref victim;

  if ((victim = match_controlled(player, what)) == NOTHING) {
    notify(player, T("Sorry."));
    return;
  }
  if (options.log_forces) {
    if (Wizard(player)) {
      /* Log forces by wizards */
      if (Owner(victim) != Owner(player))
        do_log(LT_WIZ, player, victim, "** FORCE: %s", command);
      else
        do_log(LT_WIZ, player, victim, "FORCE: %s", command);
    } else if (Wizard(Owner(victim))) {
      /* Log forces of wizards */
      do_log(LT_WIZ, player, victim, "** FORCE WIZ-OWNED: %s", command);
    }
  }
  if (God(victim) && !God(player)) {
    notify(player, T("You can't force God!"));
    return;
  }

  /* force victim to do command */
  if (queue_type != QUEUE_DEFAULT)
    new_queue_actionlist(victim, player, caller, command, queue_entry,
                         PE_INFO_SHARE, queue_type, NULL);
  else
    new_queue_actionlist(victim, player, player, command, queue_entry,
                         PE_INFO_CLONE, QUEUE_DEFAULT, NULL);
}

/** Parse a force token command, but don't force with it.
 * \verbatim
 * This function hacks up something of the form "#<dbref> <action>",
 * finding the two args, and returns 1 if it's a sensible force,
 * otherwise 0. We know only that the command starts with #.
 * \endverbatim
 * \param command the command to parse.
 * \retval 1 sensible force command
 * \retval 0 command failed (no action given, etc.)
 */
int
parse_force(char *command)
{
  char *s;

  s = command + 1;
  while (*s && !isspace((unsigned char) *s)) {
    if (!isdigit((unsigned char) *s))
      return 0;                 /* #1a is no good */
    s++;
  }
  if (!*s)
    return 0;                   /* dbref with no action is no good */
  *s = '=';                     /* Replace the first space with = so we have #3= <action> */
  return 1;
}


extern struct db_stat_info current_state;

/** Count up the number of objects of each type owned.
 * \param owner player to count for (or ANY_OWNER for all).
 * \return pointer to a static db_stat_info structure.
 */
struct db_stat_info *
get_stats(dbref owner)
{
  dbref i;
  static struct db_stat_info si;

  if (owner == ANY_OWNER)
    return &current_state;

  si.total = si.rooms = si.exits = si.things = si.players = si.garbage = 0;
  for (i = 0; i < db_top; i++) {
    if (owner == ANY_OWNER || owner == Owner(i)) {
      si.total++;
      if (IsGarbage(i)) {
        si.garbage++;
      } else {
        switch (Typeof(i)) {
        case TYPE_ROOM:
          si.rooms++;
          break;
        case TYPE_EXIT:
          si.exits++;
          break;
        case TYPE_THING:
          si.things++;
          break;
        case TYPE_PLAYER:
          si.players++;
          break;
        default:
          break;
        }
      }
    }
  }
  return &si;
}

/** The stats command.
 * \verbatim
 * This implements @stats.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player to check object stats for.
 */
void
do_stats(dbref player, const char *name)
{
  struct db_stat_info *si;
  dbref owner;

  if (*name == '\0')
    owner = ANY_OWNER;
  else if (*name == '#') {
    owner = atoi(&name[1]);
    if (!GoodObject(owner))
      owner = NOTHING;
    else if (!IsPlayer(owner))
      owner = NOTHING;
  } else if (strcasecmp(name, "me") == 0)
    owner = player;
  else
    owner = lookup_player(name);
  if (owner == NOTHING) {
    notify_format(player, T("%s: No such player."), name);
    return;
  }
  if (!Search_All(player)) {
    if (owner != ANY_OWNER && owner != player) {
      notify(player, T("You need a search warrant to do that!"));
      return;
    }
  }
  si = get_stats(owner);
  if (owner == ANY_OWNER) {
    notify_format(player,
                  T
                  ("%d objects = %d rooms, %d exits, %d things, %d players, %d garbage."),
                  si->total, si->rooms, si->exits, si->things, si->players,
                  si->garbage);
    if (first_free != NOTHING)
      notify_format(player, T("The next object to be created will be #%d."),
                    first_free);
  } else {
    notify_format(player,
                  T("%d objects = %d rooms, %d exits, %d things, %d players."),
                  si->total - si->garbage, si->rooms, si->exits, si->things,
                  si->players);
  }
}

/** Reset a player's password.
 * \verbatim
 * This implements @newpassword.
 * \endverbatim
 * \param executor the executor.
 * \param enactor the enactor.
 * \param name the name of the player whose password is to be reset.
 * \param password the new password for the player.
 * \param queue_entry the queue entry the command was executed in
 */
void
do_newpassword(dbref executor, dbref enactor,
               const char *name, const char *password, MQUE *queue_entry)
{
  dbref victim;

  if (!queue_entry->port) {
    char pass_eval[BUFFER_LEN];
    char const *sp;
    char *bp;

    sp = password;
    bp = pass_eval;
    process_expression(pass_eval, &bp, &sp, executor, executor, enactor,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';
    password = pass_eval;
  }

  if ((victim = lookup_player(name)) == NOTHING) {
    notify(executor, T("No such player."));
  } else if (*password != '\0' && !ok_password(password)) {
    /* Wiz can set null passwords, but not bad passwords */
    notify(executor, T("Bad password."));
  } else if (God(victim) && !God(executor)) {
    notify(executor, T("You cannot change that player's password."));
  } else {
    /* it's ok, do it */
    (void) atr_add(victim, "XYXXY", password_hash(password, NULL), GOD, 0);
    notify_format(executor, T("Password for %s changed."), Name(victim));
    notify_format(victim, T("Your password has been changed by %s."),
                  Name(executor));
    do_log(LT_WIZ, executor, victim, "*** NEWPASSWORD ***");
  }
}

/** Disconnect a player, forcibly.
 * \verbatim
 * This implements @boot.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the player or descriptor to boot.
 * \param flag the type of booting to do.
 * \param silent suppress msg telling the player he's been booted?
 * \param queue_entry the queue entry the command was executed in
 */
void
do_boot(dbref player, const char *name, enum boot_type flag, int silent,
        MQUE *queue_entry)
{
  dbref victim = NOTHING;
  DESC *d = NULL;
  int count = 0;
  int priv = Can_Boot(player);

  switch (flag) {
  case BOOT_NAME:
    victim = noisy_match_result(player, name, TYPE_PLAYER,
                                MAT_PMATCH | MAT_TYPE | MAT_ME);
    if (victim == NOTHING) {
      notify(player, T("No such connected player."));
      return;
    } else if (victim == player) {
      flag = BOOT_SELF;
    }
    break;
  case BOOT_SELF:
    victim = player;
    break;
  case BOOT_DESC:
    if (!is_strict_integer(name)) {
      notify(player, T("Invalid port."));
      return;
    }
    d = port_desc(parse_integer(name));
    if (!d || (!priv && (!d->connected || d->player != player))) {
      if (priv)
        notify(player, T("There is noone connected on that descriptor."));
      else
        notify(player, T("You can't boot other people!"));
      return;
    }
    victim = (d->connected ? d->player : AMBIGUOUS);
    if (d->descriptor == queue_entry->port) {
      notify(player, T("If you want to quit, use QUIT."));
      return;
    }
    break;
  }

  if (God(victim) && !God(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (victim != player && !priv) {
    notify(player, T("You can't boot other people!"));
    return;
  }

  if (flag == BOOT_DESC) {
    if (GoodObject(victim)) {
      if (!silent)
        notify(victim, T("You are politely shown to the door."));
      if (player == victim)
        notify(player, T("You boot a duplicate self."));
      else
        notify_format(player, T("You booted %s off!"), Name(victim));
    } else {
      notify_format(player, T("You booted unconnected port %s!"), name);
    }
    do_log(LT_WIZ, player, victim, "*** BOOT ***");
    boot_desc(d, "boot", player);
    return;
  }

  /* Doing @boot <player>, or @boot/me */
  count = boot_player(victim, (flag == BOOT_SELF), silent, player);
  if (count) {
    if (flag != BOOT_SELF) {
      do_log(LT_WIZ, player, victim, "*** BOOT ***");
      notify_format(player, T("You booted %s off!"), Name(victim));
    }
  } else {
    if (flag == BOOT_SELF)
      notify(player,
             T
             ("None of your connections are idle. If you want to quit, use QUIT."));
    else
      notify(player, T("That player is not online."));
  }

}

/** Chown all of a player's objects.
 * \verbatim
 * This implements @chownall
 * \endverbatim
 * \param player the enactor.
 * \param name name of player whose objects are to be chowned.
 * \param target name of new owner for objects.
 * \param preserve if 1, keep privileges and don't halt objects.
 */
void
do_chownall(dbref player, const char *name, const char *target, int preserve)
{
  int i;
  dbref victim;
  dbref n_target;
  int count = 0;

  if (!Wizard(player)) {
    notify(player, T("Try asking them first!"));
    return;
  }
  if ((victim =
       noisy_match_result(player, name, TYPE_PLAYER, MAT_LIMITED | MAT_TYPE))
      == NOTHING)
    return;

  if (!target || !*target) {
    n_target = player;
  } else {
    if ((n_target =
         noisy_match_result(player, target, TYPE_PLAYER,
                            MAT_LIMITED | MAT_TYPE)) == NOTHING)
      return;
  }

  for (i = 0; i < db_top; i++) {
    if ((Owner(i) == victim) && (!IsPlayer(i))) {
      chown_object(player, i, n_target, preserve);
      count++;
    }
  }

  /* change quota (this command is wiz only and we can assume that
   * we intend for the recipient to get all the objects, so we
   * don't do a quota check earlier.
   */
  change_quota(victim, count);
  change_quota(n_target, -count);

  notify_format(player, T("Ownership changed for %d objects."), count);
}

/** Change the zone of all of a player's objects.
 * \verbatim
 * This implements @chzoneall.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player whose objects should be rezoned.
 * \param target string containing new zone master for objects.
 * \param preserve was /preserve given?
 */
void
do_chzoneall(dbref player, const char *name, const char *target, bool preserve)
{
  int i;
  dbref victim;
  dbref zone;
  int count = 0;

  if (!Wizard(player)) {
    notify(player, T("You do not have the power to change reality."));
    return;
  }
  if ((victim =
       noisy_match_result(player, name, TYPE_PLAYER, MAT_LIMITED | MAT_TYPE))
      == NOTHING)
    return;

  if (!target || !*target) {
    notify(player, T("No zone specified."));
    return;
  }
  if (!strcasecmp(target, "none"))
    zone = NOTHING;
  else {
    switch (zone = match_result(player, target, NOTYPE, MAT_EVERYTHING)) {
    case NOTHING:
      notify(player, T("I can't seem to find that."));
      return;
    case AMBIGUOUS:
      notify(player, T("I don't know which one you mean!"));
      return;
    }
  }

  /* Okay, now that we know we're not going to spew all sorts of errors,
   * call the normal do_chzone for all the relevant objects.  This keeps
   * consistency on things like flag resetting, etc... */
  for (i = 0; i < db_top; i++) {
    if (Owner(i) == victim && Zone(i) != zone) {
      count += do_chzone(player, unparse_dbref(i), target, 0, preserve, NULL);
    }
  }
  notify_format(player, T("Zone changed for %d objects."), count);
}

/*-----------------------------------------------------------------------
 * Nasty management: @kick, examine/debug
 */

/** Execute a number of commands off the queue immediately.
 * \verbatim
 * This implements @kick, which is nasty.
 * \endverbatim
 * \param player the enactor.
 * \param num string containing number of commands to run from the queue.
 */
void
do_kick(dbref player, const char *num)
{
  int n;

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (!num || !*num) {
    notify(player, T("How many commands do you want to execute?"));
    return;
  }
  n = atoi(num);

  if (n <= 0) {
    notify(player, T("Number out of range."));
    return;
  }
  n = do_top(n);

  notify_format(player, T("%d commands executed."), n);
}

/** examine/debug.
 * This implements examine/debug, which provides some raw values for
 * object structure elements of an examined object.
 * \param player the enactor.
 * \param name name of object to examine.
 */
void
do_debug_examine(dbref player, const char *name)
{
  dbref thing;

  if (!Hasprivs(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  /* find it */
  thing = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(thing))
    return;

  notify(player, object_header(player, thing));
  notify_format(player, T("Flags value: %s"),
                bits_to_string("FLAG", Flags(thing), GOD, NOTHING));
  notify_format(player, T("Powers value: %s"),
                bits_to_string("POWER", Powers(thing), GOD, NOTHING));

  notify_format(player, T("Next: %d"), Next(thing));
  notify_format(player, T("Contents: %d"), Contents(thing));
  notify_format(player, T("Pennies: %d"), Pennies(thing));

  switch (Typeof(thing)) {
  case TYPE_PLAYER:
    break;
  case TYPE_THING:
    notify_format(player, T("Location: %d"), Location(thing));
    notify_format(player, T("Home: %d"), Home(thing));
    break;
  case TYPE_EXIT:
    notify_format(player, T("Destination: %d"), Location(thing));
    notify_format(player, T("Source: %d"), Source(thing));
    break;
  case TYPE_ROOM:
    notify_format(player, T("Drop-to: %d"), Location(thing));
    notify_format(player, T("Exits: %d"), Exits(thing));
    break;
  case TYPE_GARBAGE:
    break;
  default:
    notify(player, T("Bad object type."));
  }
}

/*-------------------------------------------------------------------------
 * Powers stuff
 */

/** Set a power on an object.
 * \verbatim
 * This implements @power.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the object on which to set the power.
 * \param power name of the power to set.
 */
void
do_power(dbref player, const char *name, const char *power)
{
  int revoke_it = 0;
  char powerbuff[BUFFER_LEN], *p, *f;
  dbref thing;

  if (!power || !*power) {
    /* @power <power> */
    do_flag_info("POWER", player, name);
    return;
  }
  if (!Wizard(player)) {
    notify(player, T("Only wizards may grant powers."));
    return;
  }
  if ((thing = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING)
    return;
  if (Unregistered(thing)) {
    notify(player, T("You can't grant powers to unregistered players."));
    return;
  }
  if (God(thing) && !God(player)) {
    notify(player, T("God is already all-powerful."));
    return;
  }

  strcpy(powerbuff, power);
  p = trim_space_sep(powerbuff, ' ');
  if (*p == '\0') {
    notify(player, T("You must specify a power to set."));
    return;
  }
  do {
    f = split_token(&p, ' ');
    revoke_it = 0;
    if (*f == NOT_TOKEN && *(f + 1)) {
      revoke_it = 1;
      f++;
    }
    set_power(player, thing, f, revoke_it);
  } while (p);

}

/*----------------------------------------------------------------------------
 * Search functions
 */

/** User command to search the db for matching objects.
 * \verbatim
 * This implements @search.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 name of player whose objects are to be searched.
 * \param arg3 additional search arguments.
 */
void
do_search(dbref player, const char *arg1, char **arg3)
{
  char tbuf[BUFFER_LEN], *arg2 = tbuf, *tbp;
  dbref *results = NULL;
  char *s;
  int nresults;
  struct search_spec spec;

  /* parse first argument into two */
  if (!arg1 || *arg1 == '\0')
    arg1 = "me";

  /* First argument is a player, so we could have a quoted name */
  if (*arg1 == '\"') {
    for (; *arg1 && ((*arg1 == '\"') || isspace((unsigned char) *arg1));
         arg1++) ;
    strcpy(tbuf, arg1);
    while (*arg2 && (*arg2 != '\"')) {
      while (*arg2 && (*arg2 != '\"'))
        arg2++;
      if (*arg2 == '\"') {
        *arg2++ = '\0';
        while (*arg2 && isspace((unsigned char) *arg2))
          arg2++;
        break;
      }
    }
  } else {
    strcpy(tbuf, arg1);
    while (*arg2 && !isspace((unsigned char) *arg2))
      arg2++;
    if (*arg2)
      *arg2++ = '\0';
    while (*arg2 && isspace((unsigned char) *arg2))
      arg2++;
  }

  if (!*arg2) {
    if (!arg3[1] || !*arg3[1])
      arg2 = (char *) "";       /* arg1 */
    else {
      arg2 = (char *) arg1;     /* arg2=arg3 */
      tbuf[0] = '\0';
    }
  }
  {
    const char *myargs[MAX_ARG];
    int i;
    int j = 2;

    myargs[0] = arg2;
    myargs[1] = arg3[1];
    for (i = 2; i < INT_MAX && (arg3[i] != NULL); i++) {
      if ((s = strchr(arg3[i], '='))) {
        *s++ = '\0';
        myargs[j++] = arg3[i];
        myargs[j++] = s;
      } else {
        myargs[j++] = arg3[i];
      }
    }
    if (fill_search_spec(player, tbuf, j, myargs, &spec) < 0) {
      if (spec.lock != TRUE_BOOLEXP)
        free_boolexp(spec.lock);
      return;
    }

    nresults = raw_search(player, &spec, &results, NULL);
  }


  if (nresults == 0) {
    notify(player, T("Nothing found."));
  } else if (nresults > 0) {
    /* Split the results up by type and report. */
    int n;
    int nthings = 0, nexits = 0, nrooms = 0, nplayers = 0, ngarbage = 0;
    dbref *things, *exits, *rooms, *players, *garbage;

    things = mush_calloc(nresults, sizeof(dbref), "dbref_list");
    exits = mush_calloc(nresults, sizeof(dbref), "dbref_list");
    rooms = mush_calloc(nresults, sizeof(dbref), "dbref_list");
    players = mush_calloc(nresults, sizeof(dbref), "dbref_list");
    garbage = mush_calloc(nresults, sizeof(dbref), "dbref_list");

    for (n = 0; n < nresults; n++) {
      switch (Typeof(results[n])) {
      case TYPE_THING:
        things[nthings++] = results[n];
        break;
      case TYPE_EXIT:
        exits[nexits++] = results[n];
        break;
      case TYPE_ROOM:
        rooms[nrooms++] = results[n];
        break;
      case TYPE_PLAYER:
        players[nplayers++] = results[n];
        break;
      case TYPE_GARBAGE:
        garbage[ngarbage++] = results[n];
        break;
      default:
        /* Unknown type. Ignore. */
        do_rawlog(LT_ERR, "Weird type for dbref #%d", results[n]);
      }
    }

    if (nrooms) {
      notify(player, T("\nROOMS:"));
      for (n = 0; n < nrooms; n++) {
        tbp = tbuf;
        safe_format(tbuf, &tbp, T("%s [owner: "),
                    object_header(player, rooms[n]));
        safe_str(object_header(player, Owner(rooms[n])), tbuf, &tbp);
        safe_chr(']', tbuf, &tbp);
        *tbp = '\0';
        notify(player, tbuf);
      }
    }

    if (nexits) {
      dbref from, to;

      notify(player, T("\nEXITS:"));
      for (n = 0; n < nexits; n++) {
        tbp = tbuf;
        if (Source(exits[n]) == NOTHING)
          from = NOTHING;
        else
          from = Source(exits[n]);
        to = Destination(exits[n]);
        safe_format(tbuf, &tbp, T("%s [from "),
                    object_header(player, exits[n]));
        safe_str((from == NOTHING) ? T("NOWHERE") : object_header(player, from),
                 tbuf, &tbp);
        safe_str(T(" to "), tbuf, &tbp);
        safe_str((to == NOTHING) ? T("NOWHERE") : object_header(player, to),
                 tbuf, &tbp);
        safe_chr(']', tbuf, &tbp);
        *tbp = '\0';
        notify(player, tbuf);
      }
    }

    if (nthings) {
      notify(player, T("\nTHINGS:"));
      for (n = 0; n < nthings; n++) {
        tbp = tbuf;
        safe_format(tbuf, &tbp, T("%s [owner: "),
                    object_header(player, things[n]));
        safe_str(object_header(player, Owner(things[n])), tbuf, &tbp);
        safe_chr(']', tbuf, &tbp);
        *tbp = '\0';
        notify(player, tbuf);
      }
    }

    if (nplayers) {
      int is_wizard = Search_All(player) || See_All(player);
      notify(player, T("\nPLAYERS:"));
      for (n = 0; n < nplayers; n++) {
        tbp = tbuf;
        safe_str(object_header(player, players[n]), tbuf, &tbp);
        if (is_wizard)
          safe_format(tbuf, &tbp,
                      T(" [location: %s]"),
                      object_header(player, Location(players[n])));
        *tbp = '\0';
        notify(player, tbuf);
      }
    }

    if (ngarbage) {
      notify(player, T("\nGARBAGE:"));
      for (n = 0; n < ngarbage; n++) {
        tbp = tbuf;
        if (ANSI_NAMES && ShowAnsi(player))
          notify_format(player, T("%sGarbage%s(#%d)"), ANSI_HILITE, ANSI_END,
                        garbage[n]);
        else
          notify_format(player, T("Garbage(#%d)"), garbage[n]);
      }
    }

    notify(player, T("----------  Search Done  ----------"));
    if (ngarbage)
      notify_format(player,
                    T
                    ("Totals: Rooms...%d  Exits...%d  Things...%d  Players...%d  Garbage...%d"),
                    nrooms, nexits, nthings, nplayers, ngarbage);
    else
      notify_format(player,
                    T
                    ("Totals: Rooms...%d  Exits...%d  Things...%d  Players...%d"),
                    nrooms, nexits, nthings, nplayers);
    mush_free(rooms, "dbref_list");
    mush_free(exits, "dbref_list");
    mush_free(things, "dbref_list");
    mush_free(players, "dbref_list");
    mush_free(garbage, "dbref_list");
  }
  if (results)
    mush_free(results, "search_results");
}

FUNCTION(fun_lsearch)
{
  int nresults;
  int return_count = 0;
  dbref *results = NULL;
  int rev = !strcmp(called_as, "LSEARCHR");
  struct search_spec spec;

  if (!command_check_byname(executor, "@search", pe_info)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (called_as[0] == 'N') {
    /* Return the count, not the values */
    return_count = 1;
  }

  if (!strcmp(called_as, "CHILDREN") || !strcmp(called_as, "NCHILDREN")) {
    const char *myargs[2];
    myargs[0] = "PARENT";
    myargs[1] = args[0];
    if (fill_search_spec(executor, NULL, 2, myargs, &spec) < 0) {
      if (spec.lock != TRUE_BOOLEXP)
        free_boolexp(spec.lock);
      safe_str("#-1", buff, bp);
      return;
    }

    nresults = raw_search(executor, &spec, &results, pe_info);
  } else {
    if (fill_search_spec(executor, args[0], nargs - 1,
                         (const char **) (args + 1), &spec) < 0) {
      if (spec.lock != TRUE_BOOLEXP)
        free_boolexp(spec.lock);
      safe_str("#-1", buff, bp);
      return;
    }

    nresults = raw_search(executor, &spec, &results, pe_info);
  }

  if (return_count) {
    safe_integer(nresults, buff, bp);
  } else if (nresults == 0) {
    notify(executor, T("Nothing found."));
  } else {
    int first = 1, n;
    if (!rev) {
      for (n = 0; n < nresults; n++) {
        if (first)
          first = 0;
        else if (safe_chr(' ', buff, bp))
          break;
        if (safe_dbref(results[n], buff, bp))
          break;
      }
    } else {
      for (n = nresults - 1; n >= 0; n--) {
        if (first)
          first = 0;
        else if (safe_chr(' ', buff, bp))
          break;
        if (safe_dbref(results[n], buff, bp))
          break;
      }
    }
  }
  if (results)
    mush_free(results, "search_results");
}

/** Find the entrances to a room.
 * \verbatim
 * This implements @entrances, which finds things linked to an object
 * (typically exits, but can be any type).
 * \endverbatim
 * \param player the enactor.
 * \param where name of object to find entrances on.
 * \param argv array of arguments for dbref range limitation.
 * \param types what type of 'entrances' to find.
 */
void
do_entrances(dbref player, const char *where, char *argv[], int types)
{
  dbref place;
  struct search_spec spec;
  int rooms, things, exits, players;
  int nresults, n;
  dbref *results = NULL;

  rooms = things = exits = players = 0;

  if (!where || !*where) {
    place = speech_loc(player);
  } else {
    place = noisy_match_result(player, where, NOTYPE, MAT_EVERYTHING);
  }
  if (!GoodObject(place))
    return;

  init_search_spec(&spec);
  spec.entrances = place;

  /* determine range */
  if (argv[1] && *argv[1])
    spec.low = atoi(argv[1]);
  if (spec.low < 0)
    spec.low = 0;
  if (argv[2] && *argv[2])
    spec.high = atoi(argv[2]) + 1;
  if (spec.high > db_top)
    spec.high = db_top;

  spec.type = types;

  nresults =
    raw_search(controls(player, place) ? GOD : player, &spec, &results, NULL);
  for (n = 0; n < nresults; n++) {
    switch (Typeof(results[n])) {
    case TYPE_EXIT:
      notify_format(player,
                    T("%s [from: %s]"), object_header(player, results[n]),
                    object_header(player, Source(results[n])));
      exits++;
      break;
    case TYPE_ROOM:
      notify_format(player, T("%s [dropto]"),
                    object_header(player, results[n]));
      rooms++;
      break;
    case TYPE_THING:
    case TYPE_PLAYER:
      notify_format(player, T("%s [home]"), object_header(player, results[n]));
      if (IsThing(results[n]))
        things++;
      else
        players++;
      break;
    }
  }

  if (results)
    mush_free(results, "search_results");

  if (!nresults)
    notify(player, T("Nothing found."));
  else {
    notify(player, T("----------  Entrances Done  ----------"));
    notify_format(player,
                  T
                  ("Totals: Rooms...%d  Exits...%d  Things...%d  Players...%d"),
                  rooms, exits, things, players);
  }
}

/* ARGSUSED */
FUNCTION(fun_entrances)
{
  /* All args are optional.
   * The first argument is the dbref to check (default = this room)
   * The second argument to this function is a set of characters:
   * (a)ll (default), (e)xits, (t)hings, (p)layers, (r)ooms
   * The third and fourth args limit the range of dbrefs (default=0,db_top)
   */
  dbref where = Location(executor);
  struct search_spec spec;
  int nresults, n;
  dbref *results = NULL;
  char *p;

  if (!command_check_byname(executor, "@entrances", pe_info)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  init_search_spec(&spec);

  if (nargs > 0)
    where = match_result(executor, args[0], NOTYPE, MAT_EVERYTHING);
  else
    where = speech_loc(executor);
  if (!GoodObject(where)) {
    safe_str(T("#-1 INVALID LOCATION"), buff, bp);
    return;
  }
  spec.entrances = where;
  spec.type = 0;
  if (nargs > 1 && args[1] && *args[1]) {
    p = args[1];
    while (*p) {
      switch (*p) {
      case 'a':
      case 'A':
        spec.type = NOTYPE;
        break;
      case 'e':
      case 'E':
        spec.type |= TYPE_EXIT;
        break;
      case 't':
      case 'T':
        spec.type |= TYPE_THING;
        break;
      case 'p':
      case 'P':
        spec.type |= TYPE_PLAYER;
        break;
      case 'r':
      case 'R':
        spec.type |= TYPE_ROOM;
        break;
      default:
        safe_str(T("#-1 INVALID SECOND ARGUMENT"), buff, bp);
        return;
      }
      p++;
    }
  }
  if (!spec.type)
    spec.type = NOTYPE;

  if (nargs > 2) {
    if (is_strict_integer(args[2])) {
      spec.low = parse_integer(args[2]);
    } else if (is_dbref(args[2])) {
      spec.low = parse_dbref(args[2]);
    } else {
      safe_str(T(e_ints), buff, bp);
      return;
    }
  }
  if (nargs > 3) {
    if (is_strict_integer(args[3])) {
      spec.high = parse_integer(args[3]);
    } else if (is_dbref(args[3])) {
      spec.high = parse_dbref(args[3]);
    } else {
      safe_str(T(e_ints), buff, bp);
      return;
    }
  }
  if (!GoodObject(spec.low))
    spec.low = 0;
  if (!GoodObject(spec.high))
    spec.high = db_top - 1;

  nresults =
    raw_search(controls(executor, where) ? GOD : executor, &spec, &results,
               pe_info);
  for (n = 0; n < nresults; n++) {
    if (n) {
      if (safe_chr(' ', buff, bp))
        break;
    }
    if (safe_dbref(results[n], buff, bp))
      break;
  }

  if (results)
    mush_free(results, "search_results");

}

/* ARGSUSED */
FUNCTION(fun_quota)
{
  int owned;
  /* Tell us player's quota */
  dbref thing;
  dbref who;
  who =
    noisy_match_result(executor, args[0], TYPE_PLAYER,
                       MAT_TYPE | MAT_PMATCH | MAT_ME);
  if ((who == NOTHING) || !IsPlayer(who)) {
    safe_str("#-1", buff, bp);
    return;
  }
  if (!(Do_Quotas(executor) || See_All(executor)
        || controls(executor, who))) {
    notify(executor, T("You can't see someone else's quota!"));
    safe_str("#-1", buff, bp);
    return;
  }
  if (NoQuota(who)) {
    /* Unlimited, but return a big number to be sensible */
    safe_str("99999", buff, bp);
    return;
  }
  /* count up all owned objects */
  owned = -1;                   /* a player is never included in his own
                                 * quota */
  for (thing = 0; thing < db_top; thing++) {
    if (Owner(thing) == who)
      if (!IsGarbage(thing))
        ++owned;
  }

  safe_integer(owned + get_current_quota(who), buff, bp);
  return;
}

static void
sitelock_player(dbref player, const char *name, dbref who, uint32_t can,
                uint32_t cant)
{
  dbref target;
  ATTR *a;
  int attrcount = 0;


  if ((target = noisy_match_result(player, name, TYPE_PLAYER,
                                   MAT_ABSOLUTE | MAT_PMATCH | MAT_TYPE)) ==
      NOTHING)
    return;

  a = atr_get(target, "LASTIP");
  if (a && add_access_sitelock(player, atr_value(a), who, can, cant)) {
    attrcount++;
    do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s", atr_value(a));
  }
  a = atr_get(target, "LASTSITE");
  if (a && add_access_sitelock(player, atr_value(a), who, can, cant)) {
    attrcount++;
    do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s", atr_value(a));
  }
  if (attrcount) {
    write_access_file();
    notify_format(player, T("Sitelocked %d known addresses for %s"), attrcount,
                  Name(target));
  } else {
    notify_format(player, T("Unable to sitelock %s: No known ip/host to ban."),
                  Name(target));
  }

}

/** Modify access rules for a site.
 * \verbatim
 * This implements @sitelock.
 * \endverbatim
 * \param player the enactor.
 * \param site name of site.
 * \param opts access rules to apply.
 * \param who string containing dbref of player to whom rule applies.
 * \param type sitelock operation to do.
 * \param psw was the /player switch given?
 */
void
do_sitelock(dbref player, const char *site, const char *opts, const char *who,
            enum sitelock_type type, int psw)
{
  if (!Wizard(player)) {
    notify(player, T("Your delusions of grandeur have been noted."));
    return;
  }
  if (opts && *opts) {
    uint32_t can, cant;
    dbref whod = AMBIGUOUS;
    /* Options form of the command. */
    if (!site || !*site) {
      notify(player, T("What site did you want to lock?"));
      return;
    }
    can = cant = 0;
    if (!parse_access_options(opts, NULL, &can, &cant, player)) {
      notify(player, T("No valid options found."));
      return;
    }
    if (who && *who) {          /* Specify a character */
      whod = lookup_player(who);
      if (!GoodObject(whod)) {
        notify(player, T("Who do you want to lock?"));
        return;
      }
    }
    if (psw) {
      sitelock_player(player, site, whod, can, cant);
      return;
    }
    if (add_access_sitelock(player, site, whod, can, cant)) {
      write_access_file();
      if (whod != AMBIGUOUS) {
        notify_format(player,
                      T("Site %s access options for %s(%s) set to %s"),
                      site, Name(whod), unparse_dbref(whod), opts);
        do_log(LT_WIZ, player, NOTHING,
               "*** SITELOCK *** %s for %s(%s) --> %s", site,
               Name(whod), unparse_dbref(whod), opts);
      } else {
        notify_format(player, T("Site %s access options set to %s"), site,
                      opts);
        do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s --> %s", site,
               opts);
      }
      return;
    }
  } else {
    /* Backward-compatible non-options form of the command,
     * or @sitelock/name
     */
    switch (type) {
    case SITELOCK_LIST:
      /* List bad sites */
      do_list_access(player);
      return;
    case SITELOCK_REGISTER:
      if (psw) {
        sitelock_player(player, site, AMBIGUOUS, ACS_REGISTER, ACS_CREATE);
        return;
      }
      if (add_access_sitelock
          (player, site, AMBIGUOUS, ACS_REGISTER, ACS_CREATE)) {
        write_access_file();
        notify_format(player, T("Site %s locked"), site);
        do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s", site);
      }
      break;
    case SITELOCK_ADD:
      if (psw) {
        sitelock_player(player, site, AMBIGUOUS, 0, ACS_CREATE);
        return;
      }
      if (add_access_sitelock(player, site, AMBIGUOUS, 0, ACS_CREATE)) {
        write_access_file();
        notify_format(player, T("Site %s locked"), site);
        do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s", site);
      }
      break;
    case SITELOCK_BAN:
      if (psw) {
        sitelock_player(player, site, AMBIGUOUS, 0, ACS_DEFAULT);
        return;
      }
      if (add_access_sitelock(player, site, AMBIGUOUS, 0, ACS_DEFAULT)) {
        write_access_file();
        notify_format(player, T("Site %s banned"), site);
        do_log(LT_WIZ, player, NOTHING, "*** SITELOCK *** %s", site);
      }
      break;
    case SITELOCK_CHECK:{
        struct access *ap;
        char tbuf[BUFFER_LEN], *bp;
        int rulenum;
        if (!site || !*site) {
          do_list_access(player);
          return;
        }
        ap = site_check_access(site, AMBIGUOUS, &rulenum);
        bp = tbuf;
        format_access(ap, rulenum, AMBIGUOUS, tbuf, &bp);
        *bp = '\0';
        notify(player, tbuf);
        break;
      }
    case SITELOCK_REMOVE:{
        int n = 0;
        if (psw) {
          ATTR *a;
          dbref target;
          if ((target = noisy_match_result(player, site, TYPE_PLAYER,
                                           MAT_ABSOLUTE | MAT_PMATCH |
                                           MAT_TYPE)) == NOTHING)
            return;
          if ((a = atr_get(target, "LASTIP")))
            n += remove_access_sitelock(atr_value(a));
          if ((a = atr_get(target, "LASTSITE")))
            n += remove_access_sitelock(atr_value(a));
        } else {
          n = remove_access_sitelock(site);
        }
        if (n > 0)
          write_access_file();
        notify_format(player, T("%d sitelocks removed."), n);
        break;
      }
    }
  }
}

/** Edit the list of restricted player names
 * \verbatim
 * This implements @sitelock/name
 * \endverbatim
 * \param player the player doing the command.
 * \param name the name (Actually, wildcard pattern) to restrict.
 */
void
do_sitelock_name(dbref player, const char *name)
{
  FILE *fp, *fptmp;
  char buffer[BUFFER_LEN];
  char *p;

  if (!Wizard(player)) {
    notify(player, T("Your delusions of grandeur have been noted."));
    return;
  }

  release_fd();

  if (!name || !*name) {
    /* List bad names */
    if ((fp = fopen(NAMES_FILE, FOPEN_READ)) == NULL) {
      notify(player, T("Unable to open names file."));
    } else {
      notify(player, T("Any name matching these wildcard patterns is banned:"));
      while (fgets(buffer, sizeof buffer, fp)) {
        if ((p = strchr(buffer, '\r')) != NULL)
          *p = '\0';
        else if ((p = strchr(buffer, '\n')) != NULL)
          *p = '\0';
        notify(player, buffer);
      }
      fclose(fp);
    }
  } else if (name[0] == '!') {  /* Delete a name */
    if ((fp = fopen(NAMES_FILE, FOPEN_READ)) != NULL) {
      if ((fptmp = fopen("tmp.tmp", FOPEN_WRITE)) == NULL) {
        notify(player, T("Unable to delete name."));
        fclose(fp);
      } else {
        while (fgets(buffer, sizeof buffer, fp)) {
          if ((p = strchr(buffer, '\r')) != NULL)
            *p = '\0';
          else if ((p = strchr(buffer, '\n')) != NULL)
            *p = '\0';
          if (strcasecmp(buffer, name + 1) == 0)
            /* Replace the name with #NAME, to allow things like
               keeping track of unlocked feature names. */
            fprintf(fptmp, "#%s\n", buffer);
          else
            fprintf(fptmp, "%s\n", buffer);
        }
        fclose(fp);
        fclose(fptmp);
        if (rename_file("tmp.tmp", NAMES_FILE) == 0) {
          notify(player, T("Name removed."));
          do_log(LT_WIZ, player, NOTHING, "*** UNLOCKED NAME *** %s", name + 1);
        } else {
          notify(player, T("Unable to delete name."));
        }
      }
    } else
      notify(player, T("Unable to delete name."));
  } else {                      /* Add a name */
    if ((fp = fopen(NAMES_FILE, FOPEN_READ)) != NULL) {
      if ((fptmp = fopen("tmp.tmp", FOPEN_WRITE)) == NULL) {
        notify(player, T("Unable to lock name."));
      } else {
        /* Read the names file, looking for #NAME and writing it
           without the commenting #. Otherwise, add the new name
           to the end of the file unless it's already present */
        char commented[BUFFER_LEN + 1];
        int found = 0;
        commented[0] = '#';
        strcpy(commented + 1, name);
        while (fgets(buffer, sizeof buffer, fp) != NULL) {
          if ((p = strchr(buffer, '\r')) != NULL)
            *p = '\0';
          else if ((p = strchr(buffer, '\n')) != NULL)
            *p = '\0';
          if (strcasecmp(commented, buffer) == 0) {
            fprintf(fptmp, "%s\n", buffer + 1);
            found = 1;
          } else {
            fprintf(fptmp, "%s\n", buffer);
            if (strcasecmp(name, buffer) == 0)
              found = 1;
          }
        }
        if (!found)
          fprintf(fptmp, "%s\n", name);
        fclose(fp);
        fclose(fptmp);

        if (rename_file("tmp.tmp", NAMES_FILE) == 0) {
          notify_format(player, T("Name %s locked."), name);
          do_log(LT_WIZ, player, NOTHING, "*** NAMELOCK *** %s", name);
        } else
          notify(player, T("Unable to lock name."));
      }
    }
  }
  reserve_fd();
}

/*-----------------------------------------------------------------
 * Functions which give memory information on objects or players.
 * Source code originally by Kalkin, modified by Javelin
 */

static int
mem_usage(dbref thing)
{
  int k;
  ATTR *m;
  lock_list *l;
  k = sizeof(struct object);    /* overhead */
  k += strlen(Name(thing)) + 1; /* The name */
  for (m = List(thing); m; m = AL_NEXT(m)) {
    k += sizeof(ATTR);
    if (AL_STR(m) && *AL_STR(m))
      k += u_strlen(AL_STR(m)) + 1;
    /* NOTE! In the above, we're getting the size of the
     * compressed attrib, not the uncompressed one (as Kalkin did)
     * because (1) it's more efficient, (2) it's more accurate
     * since that's how the object is in memory. This relies on
     * compressed attribs being terminated with \0's, which they
     * are in compress.c. If that changes, this breaks.
     */
  }
  for (l = Locks(thing); l; l = l->next) {
    k += sizeof(lock_list);
    k += sizeof_boolexp(l->key);
  }
  return k;
}

/* ARGSUSED */
FUNCTION(fun_objmem)
{
  dbref thing;
  if (!Search_All(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (!strcasecmp(args[0], "me"))
    thing = executor;
  else if (!strcasecmp(args[0], "here"))
    thing = Location(executor);
  else {
    thing = noisy_match_result(executor, args[0], NOTYPE, MAT_OBJECTS);
  }
  if (!GoodObject(thing)) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  if (!Can_Examine(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  safe_integer(mem_usage(thing), buff, bp);
}



/* ARGSUSED */
FUNCTION(fun_playermem)
{
  int tot = 0;
  dbref thing;
  dbref j;

  if (!Search_All(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (!strcasecmp(args[0], "me") && IsPlayer(executor))
    thing = executor;
  else if (*args[0] && *args[0] == '*')
    thing = lookup_player(args[0] + 1);
  else if (*args[0] && *args[0] == '#')
    thing = atoi(args[0] + 1);
  else
    thing = lookup_player(args[0]);
  if (!GoodObject(thing) || !IsPlayer(thing)) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  if (!Can_Examine(executor, thing)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  for (j = 0; j < db_top; j++)
    if (Owner(j) == thing)
      tot += mem_usage(j);
  safe_integer(tot, buff, bp);
}

/** Initialize a search_spec struct with blank/default values */
static void
init_search_spec(struct search_spec *spec)
{
  spec->zone = spec->parent = spec->owner = spec->entrances = ANY_OWNER;
  spec->type = NOTYPE;
  strcpy(spec->flags, "");
  strcpy(spec->lflags, "");
  strcpy(spec->powers, "");
  strcpy(spec->eval, "");
  strcpy(spec->name, "");
  spec->low = 0;
  spec->high = INT_MAX;
  spec->start = 1;              /* 1-indexed */
  spec->count = 0;
  spec->lock = TRUE_BOOLEXP;
  strcpy(spec->cmdstring, "");
  strcpy(spec->listenstring, "");
}

static int
fill_search_spec(dbref player, const char *owner, int nargs, const char **args,
                 struct search_spec *spec)
{
  int n;
  const char *class, *restriction;

  init_search_spec(spec);

  /* set limits on who we search */
  if (!owner || !*owner)
    spec->owner = (See_All(player)
                   || Search_All(player)) ? ANY_OWNER : Owner(player);
  else if (strcasecmp(owner, "all") == 0)
    spec->owner = ANY_OWNER;    /* Will only show visual objects for mortals */
  else if (strcasecmp(owner, "me") == 0)
    spec->owner = Owner(player);
  else
    spec->owner = lookup_player(owner);
  if (spec->owner == NOTHING) {
    notify(player, T("Unknown owner."));
    return -1;
  }
  // An odd number of search classes is invalid.
  if (nargs % 2) {
    notify(player, T("Invalid search class+restriction format."));
    return -1;
  }

  for (n = 0; n < nargs - 1; n += 2) {
    class = args[n];
    restriction = args[n + 1];
    /* A special old-timey kludge */
    if (class && !*class && restriction && *restriction) {
      if (isdigit((unsigned char) *restriction)
          || ((*restriction == '#') && *(restriction + 1)
              && isdigit((unsigned char) *(restriction + 1)))) {
        size_t offset = 0;
        if (*restriction == '#')
          offset = 1;
        spec->high = parse_integer(restriction + offset);
        continue;
      }
    }
    if (!class || !*class || !restriction)
      continue;
    if (isdigit((unsigned char) *class) || ((*class == '#') && *(class + 1)
                                            && isdigit((unsigned char)
                                                       *(class + 1)))) {
      size_t offset = 0;
      if (*class == '#')
        offset = 1;
      spec->low = parse_integer(class + offset);
      if (isdigit((unsigned char) *restriction)
          || ((*restriction == '#') && *(restriction + 1)
              && isdigit((unsigned char) *(restriction + 1)))) {
        offset = 0;
        if (*restriction == '#')
          offset = 1;
        spec->high = parse_integer(restriction + offset);
      }
      continue;
    }
    /* Figure out the class */
    /* Old-fashioned way to select everything */
    if (string_prefix("none", class))
      continue;
    if (string_prefix("mindb", class)) {
      size_t offset = 0;
      if (*restriction == '#')
        offset = 1;
      spec->low = parse_integer(restriction + offset);
      continue;
    } else if (string_prefix("maxdb", class)) {
      size_t offset = 0;
      if (*restriction == '#')
        offset = 1;
      spec->high = parse_integer(restriction + offset);
      continue;
    }

    if (string_prefix("type", class)) {
      if (string_prefix("things", restriction)
          || string_prefix("objects", restriction)) {
        spec->type = TYPE_THING;
      } else if (string_prefix("rooms", restriction)) {
        spec->type = TYPE_ROOM;
      } else if (string_prefix("exits", restriction)) {
        spec->type = TYPE_EXIT;
      } else if (string_prefix("rooms", restriction)) {
        spec->type = TYPE_ROOM;
      } else if (string_prefix("players", restriction)) {
        spec->type = TYPE_PLAYER;
      } else if (string_prefix("garbage", restriction)) {
        spec->type = TYPE_GARBAGE;
      } else {
        notify(player, T("Unknown type."));
        return -1;
      }
    } else if (string_prefix("things", class)
               || string_prefix("objects", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_THING;
    } else if (string_prefix("exits", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_EXIT;
    } else if (string_prefix("rooms", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_ROOM;
    } else if (string_prefix("players", class)) {
      strcpy(spec->name, restriction);
      spec->type = TYPE_PLAYER;
    } else if (string_prefix("name", class)) {
      strcpy(spec->name, restriction);
    } else if (string_prefix("start", class)) {
      spec->start = parse_integer(restriction);
      if (spec->start < 1) {
        notify(player, T("Invalid start index"));
        return -1;
      }
    } else if (string_prefix("count", class)) {
      spec->count = parse_integer(restriction);
      if (spec->count < 1) {
        notify(player, T("Invalid count index"));
        return -1;
      }
    } else if (string_prefix("parent", class)) {
      if (!*restriction) {
        spec->parent = NOTHING;
        continue;
      }
      if (!is_objid(restriction)) {
        notify(player, T("Unknown parent."));
        return -1;
      }
      spec->parent = parse_objid(restriction);
      if (!GoodObject(spec->parent)) {
        notify(player, T("Unknown parent."));
        return -1;
      }
    } else if (string_prefix("zone", class)) {
      if (!*restriction) {
        spec->zone = NOTHING;
        continue;
      }
      if (!is_objid(restriction)) {
        notify(player, T("Unknown zone."));
        return -1;
      }
      spec->zone = parse_objid(restriction);
      if (!GoodObject(spec->zone)) {
        notify(player, T("Unknown zone."));
        return -1;
      }
    } else if (string_prefix("elock", class)) {
      spec->lock = parse_boolexp(player, restriction, "Search");
      if (spec->lock == TRUE_BOOLEXP) {
        notify(player, T("I don't understand that key."));
        return -1;
      }
    } else if (string_prefix("eval", class)) {
      strcpy(spec->eval, restriction);
    } else if (string_prefix("command", class)) {
      strcpy(spec->cmdstring, restriction);
    } else if (string_prefix("listen", class)) {
      strcpy(spec->listenstring, restriction);
    } else if (string_prefix("ethings", class) ||
               string_prefix("eobjects", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_THING;
    } else if (string_prefix("eexits", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_EXIT;
    } else if (string_prefix("erooms", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_ROOM;
    } else if (string_prefix("eplayers", class)) {
      strcpy(spec->eval, restriction);
      spec->type = TYPE_PLAYER;
    } else if (string_prefix("powers", class)) {
      /* Handle the checking later.  */
      if (!restriction || !*restriction) {
        notify(player, T("You must give a list of power names."));
        return -1;
      }
      strcpy(spec->powers, restriction);
    } else if (string_prefix("flags", class)) {
      /* Handle the checking later.  */
      if (!restriction || !*restriction) {
        notify(player, T("You must give a string of flag characters."));
        return -1;
      }
      strcpy(spec->flags, restriction);
    } else if (string_prefix("lflags", class)) {
      /* Handle the checking later.  */
      if (!restriction || !*restriction) {
        notify(player, T("You must give a list of flag names."));
        return -1;
      }
      strcpy(spec->lflags, restriction);
    } else {
      notify(player, T("Unknown search class."));
      return -1;
    }
  }
  spec->end = spec->start + spec->count;
  return 0;
}

/* Does the actual searching */
static int
raw_search(dbref player, struct search_spec *spec,
           dbref **result, NEW_PE_INFO *pe_info)
{
  size_t result_size;
  size_t nresults = 0;
  int n;
  int is_wiz;
  int count = 0;
  int ret = 0;
  int vis_only = 0;
  ATTR *a;
  char lbuff[BUFFER_LEN];

  is_wiz = Search_All(player) || See_All(player);

  /* vis_only: @searcher doesn't have see_all, and can only examine
   * objects that they pass the CanExamine() check for.
   */
  if (!is_wiz && spec->owner != Owner(player)) {
    vis_only = 1;

    /* For Zones: If the player passes the zone lock on a shared player,
     * they are considered to be able to examine everything of that player,
     * so do not need vis_only. */
    if (GoodObject(spec->owner) && ZMaster(spec->owner)) {
      vis_only = !eval_lock_with(player, spec->owner, Zone_Lock, pe_info);
    }
  }

  /* make sure player has money to do the search -
   * But only if this does an eval or lock search. */
  if (((spec->lock != TRUE_BOOLEXP) && is_eval_lock(spec->lock)) ||
      spec->cmdstring[0] || spec->listenstring[0] || spec->eval[0]) {
    if (!payfor(player, FIND_COST)) {
      notify_format(player, T("Searches cost %d %s."), FIND_COST,
                    ((FIND_COST == 1) ? MONEY : MONIES));
      if (spec->lock != TRUE_BOOLEXP)
        free_boolexp(spec->lock);
      return -1;
    }
  }

  result_size = (db_top / 4) + 1;
  *result = mush_calloc(result_size, sizeof(dbref), "search_results");
  if (!*result)
    mush_panic("Couldn't allocate memory in search!");

  if (spec->low < 0) {
    spec->low = 0;
  }
  if (spec->high >= db_top)
    spec->high = db_top - 1;
  for (n = spec->low; n <= spec->high && n < db_top; n++) {
    if (IsGarbage(n) && spec->type != TYPE_GARBAGE)
      continue;
    if (spec->owner != ANY_OWNER && Owner(n) != spec->owner)
      continue;
    if (vis_only && !Can_Examine(player, n))
      continue;
    if (spec->type != NOTYPE && Typeof(n) != spec->type)
      continue;
    if (spec->zone != ANY_OWNER && Zone(n) != spec->zone)
      continue;
    if (spec->parent != ANY_OWNER && Parent(n) != spec->parent)
      continue;
    if (spec->entrances != ANY_OWNER) {
      if ((Mobile(n) ? Home(n) : Location(n)) != spec->entrances)
        continue;
    }
    if (*spec->name && !string_match(Name(n), spec->name))
      continue;
    if (*spec->flags
        && (flaglist_check("FLAG", player, n, spec->flags, 1) != 1))
      continue;
    if (*spec->lflags
        && (flaglist_check_long("FLAG", player, n, spec->lflags, 1) != 1))
      continue;
    if (*spec->powers
        && (flaglist_check_long("POWER", player, n, spec->powers, 1) != 1))
      continue;
    if (spec->lock != TRUE_BOOLEXP
        && !eval_boolexp(n, spec->lock, player, pe_info))
      continue;
    if (spec->cmdstring[0] &&
        !atr_comm_match(n, player, '$', ':', spec->cmdstring, 1, 0,
                        NULL, NULL, 0, NULL, NULL, QUEUE_DEFAULT))
      continue;
    if (spec->listenstring[0]) {
      ret = 0;
      /* do @listen stuff */
      a = atr_get_noparent(n, "LISTEN");
      if (a) {
        strcpy(lbuff, atr_value(a));
        ret = AF_Regexp(a)
          ? regexp_match_case_r(lbuff, spec->listenstring,
                                AF_Case(a), NULL, 0, NULL, 0, NULL)
          : wild_match_case_r(lbuff, spec->listenstring,
                              AF_Case(a), NULL, 0, NULL, 0, NULL);
      }
      if (!ret &&
          !atr_comm_match(n, player, '^', ':', spec->listenstring, 1, 0,
                          NULL, NULL, 0, NULL, NULL, QUEUE_DEFAULT))
        continue;
    }
    if (*spec->eval) {
      char *ebuf1;
      const char *ebuf2;
      char tbuf1[BUFFER_LEN];
      char *bp;
      int per;

      ebuf1 = replace_string("##", unparse_dbref(n), spec->eval);
      ebuf2 = ebuf1;
      bp = tbuf1;
      per = process_expression(tbuf1, &bp, &ebuf2, player, player, player,
                               PE_DEFAULT, PT_DEFAULT, pe_info);
      mush_free(ebuf1, "replace_string.buff");
      if (per)
        goto exit_sequence;
      *bp = '\0';
      if (!parse_boolean(tbuf1))
        continue;
    }

    /* Only include the matching dbrefs from start to start+count */
    count++;
    if (count < spec->start) {
      continue;
    }
    if (spec->count && count >= spec->end) {
      continue;
    }

    if (nresults >= result_size) {
      dbref *newresults;
      result_size *= 2;
      newresults = (dbref *) realloc(*result, sizeof(dbref) * result_size);
      if (!newresults)
        mush_panic("Couldn't reallocate memory in search!");
      *result = newresults;
    }

    (*result)[nresults++] = (dbref) n;
  }

exit_sequence:
  if (spec->lock != TRUE_BOOLEXP)
    free_boolexp(spec->lock);
  return (int) nresults;
}
