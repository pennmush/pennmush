/**
 * \file speech.c
 *
 * \brief Speech-related commands in PennMUSH.
 *
 *
 */
/* speech.c */

#include "copyrite.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "conf.h"
#include "externs.h"
#include "ansi.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "lock.h"
#include "flags.h"
#include "log.h"
#include "match.h"
#include "attrib.h"
#include "parse.h"
#include "game.h"
#include "mypcre.h"
#include "sort.h"
#include "confmagic.h"

static dbref speech_loc(dbref thing);
void propagate_sound(dbref thing, const char *msg);
static void do_audible_stuff(dbref loc, dbref *excs, int numexcs,
                             const char *msg);
static void do_one_remit(dbref player, const char *target, const char *msg,
                         int flags);
dbref na_zemit(dbref current, void *data);

const char *
spname(dbref thing)
{
  /* if FULL_INVIS is defined, dark wizards and dark objects will be
   * Someone and Something, respectively.
   */

  if (FULL_INVIS && DarkLegal(thing)) {
    if (IsPlayer(thing))
      return "Someone";
    else
      return "Something";
  } else {
    return accented_name(thing);
  }
}

/** Can player pemit to target?
 * You can pemit if you're pemit_all, if you're pemitting to yourself,
 * if you're pemitting to a non-player, or if you pass target's
 * pagelock and target isn't HAVEN.
 * \param player dbref attempting to pemit.
 * \param target target dbref to pemit to.
 * \retval 1 player may pemit to target.
 * \retval 0 player may not pemit to target.
 */
int
okay_pemit(dbref player, dbref target)
{
  if (Pemit_All(player))
    return 1;
  if (IsPlayer(target) && Haven(target))
    return 0;
  if (!eval_lock(player, target, Page_Lock)) {
    fail_lock(player, target, Page_Lock, NULL, NOTHING);
    return 0;
  }
  return 1;
}

static dbref
speech_loc(dbref thing)
{
  /* This is the place where speech, poses, and @emits by thing should be
   * heard. For things and players, it's the loc; For rooms, it's the room
   * itself; for exits, it's the source. */
  if (!GoodObject(thing))
    return NOTHING;
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    return thing;
  case TYPE_EXIT:
    return Home(thing);
  default:
    return Location(thing);
  }
}

/** The teach command.
 * \param player the enactor.
 * \param cause the object causing the command to run.
 * \param tbuf1 the command being taught.
 */
void
do_teach(dbref player, dbref cause, const char *tbuf1)
{
  dbref loc;
  static int recurse = 0;
  char *command;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player) && !eval_lock(player, loc, Speech_Lock)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  if (recurse) {
    /* Somebody tried to teach the teach command. Cute. Dumb. */
    notify(player, T("You can't teach 'teach', sorry."));
    recurse = 0;
    return;
  }

  if (!tbuf1 || !*tbuf1) {
    notify(player, T("What command do you want to teach?"));
    return;
  }

  recurse = 1;                  /* Protect use from recursive teach */
  notify_except(Contents(loc), NOTHING,
                tprintf(T("%s types --> %s%s%s"), spname(player),
                        ANSI_HILITE, tbuf1, ANSI_END), NA_INTER_HEAR);
  command = mush_strdup(tbuf1, "string");       /* process_command is destructive */
  process_command(player, command, cause, 1);
  mush_free(command, "string");
  recurse = 0;                  /* Ok, we can be called again safely */
}

/** The say command.
 * \param player the enactor.
 * \param tbuf1 the message to say.
 */
void
do_say(dbref player, const char *tbuf1)
{
  dbref loc;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player) && !eval_lock(player, loc, Speech_Lock)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  if (*tbuf1 == SAY_TOKEN && CHAT_STRIP_QUOTE)
    tbuf1++;

  /* notify everybody */
  notify_format(player, T("You say, \"%s\""), tbuf1);
  notify_except(Contents(loc), player,
                tprintf(T("%s says, \"%s\""), spname(player), tbuf1),
                NA_INTER_HEAR);
}

/** The oemit(/list) command.
 * \verbatim
 * This implements @oemit and @oemit/list.
 * \endverbatim
 * \param player the enactor.
 * \param list the list of dbrefs to oemit from the emit.
 * \param message the message to emit.
 * \param flags PEMIT_* flags.
 */
void
do_oemit_list(dbref player, char *list, const char *message, int flags)
{
  char *temp, *p, *s;
  dbref who;
  dbref pass[12], locs[10];
  int i, oneloc = 0;
  int na_flags = NA_INTER_HEAR;

  /* If no message, further processing is pointless.
   * If no list, they should have used @remit. */
  if (!message || !*message || !list || !*list)
    return;

  orator = player;
  pass[0] = 0;
  /* Find out what room to do this in. If they supplied a db# before
   * the '/', then oemit to anyone in the room who's not on list.
   * Otherwise, oemit to every location which has at least one of the
   * people in the list. This is intended for actions which involve
   * players who are in different rooms, e.g.:
   *
   * X (in #0) fires an arrow at Y (in #2).
   *
   * X sees: You fire an arrow at Y. (pemit to X)
   * Y sees: X fires an arrow at you! (pemit to Y)
   * #0 sees: X fires an arrow at Y. (oemit/list to X Y)
   * #2 sees: X fires an arrow at Y. (from the same oemit)
   */
  /* Find out what room to do this in. They should have supplied a db#
   * before the '/'. */
  if ((temp = strchr(list, '/'))) {
    *temp++ = '\0';
    pass[1] = noisy_match_result(player, list, NOTYPE, MAT_EVERYTHING);
    if (!GoodObject(pass[1])) {
      notify(player, T("I can't find that room."));
      return;
    }

    if (!Loud(player) && !eval_lock(player, pass[1], Speech_Lock)) {
      fail_lock(player, pass[1], Speech_Lock, T("You may not speak there!"),
                NOTHING);
      return;
    }

    oneloc = 1;                 /* we are only oemitting to one location */
  } else {
    temp = list;
  }

  s = trim_space_sep(temp, ' ');
  while (s) {
    p = split_token(&s, ' ');
    /* If a room was given, we match relative to the room */
    if (oneloc)
      who = match_result(pass[1], p, NOTYPE, MAT_POSSESSION | MAT_ABSOLUTE);
    else
      who = noisy_match_result(player, p, NOTYPE, MAT_OBJECTS);
    /* pass[0] tracks the number of valid players we've found.
     * pass[1] is the given room (possibly nothing right now)
     * pass[2..12] are dbrefs of players
     * locs[0..10] are corresponding dbrefs of locations
     */
    if (GoodObject(who) && GoodObject(Location(who))
        && (Loud(player) || eval_lock(player, Location(who), Speech_Lock))
      ) {
      if (pass[0] < 10) {
        locs[pass[0]] = Location(who);
        pass[pass[0] + 2] = who;
        pass[0]++;
      } else {
        notify(player, T("Too many people to oemit to."));
        break;
      }
    }
  }

  /* Sort the list of rooms to oemit to so we don't oemit to the same
   * room twice */
  qsort((void *) locs, pass[0], sizeof(locs[0]), dbref_comp);

  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  for (i = 0; i < pass[0]; i++) {
    if (i != 0 && locs[i] == locs[i - 1])
      continue;
    pass[1] = locs[i];
    notify_anything_loc(orator, na_exceptN, pass, ns_esnotify, na_flags,
                        message, locs[i]);
    do_audible_stuff(pass[1], &pass[2], pass[0], message);
  }
}


/** The whisper command.
 * \param player the enactor.
 * \param arg1 name of the object to whisper to.
 * \param arg2 message to whisper.
 * \param noisy if 1, others overhear that a whisper has occurred.
 */
void
do_whisper(dbref player, const char *arg1, const char *arg2, int noisy)
{
  dbref who;
  int key;
  const char *gap;
  char *tbuf, *tp;
  char *p;
  dbref good[100];
  int gcount = 0;
  const char *head;
  int overheard;
  char *current;
  const char **start;

  if (!arg1 || !*arg1) {
    notify(player, T("Whisper to whom?"));
    return;
  }
  if (!arg2 || !*arg2) {
    notify(player, T("Whisper what?"));
    return;
  }
  tp = tbuf = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!tbuf)
    mush_panic("Unable to allocate memory in do_whisper_list");

  overheard = 0;
  head = arg1;
  start = &head;
  /* Figure out what kind of message */
  gap = " ";
  switch (*arg2) {
  case SEMI_POSE_TOKEN:
    gap = "";
  case POSE_TOKEN:
    key = 1;
    arg2++;
    break;
  default:
    key = 2;
    break;
  }

  *tp = '\0';
  /* Make up a list of good and bad names */
  while (head && *head) {
    current = next_in_list(start);
    who = match_result(player, current, TYPE_PLAYER, MAT_NEAR_THINGS |
                       MAT_CONTAINER);
    if (!GoodObject(who) || !can_interact(player, who, INTERACT_HEAR)) {
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
      if (GoodObject(who))
        notify_format(player, T("%s can't hear you."), Name(who));
    } else {
      /* A good whisper */
      good[gcount++] = who;
      if (gcount >= 100) {
        notify(player, T("Too many people to whisper to."));
        break;
      }
    }
  }

  *tp = '\0';
  if (*tbuf)
    notify_format(player, T("Unable to whisper to:%s"), tbuf);

  if (!gcount) {
    mush_free(tbuf, "string");
    return;
  }

  /* Drunk wizards... */
  if (Dark(player))
    noisy = 0;

  /* Set up list of good names */
  tp = tbuf;
  safe_str(T(" to "), tbuf, &tp);
  for (who = 0; who < gcount; who++) {
    if (noisy && (get_random32(0, 100) < (uint32_t) WHISPER_LOUDNESS))
      overheard = 1;
    safe_itemizer(who + 1, (who == gcount - 1), ",", T("and"), " ", tbuf, &tp);
    safe_str(Name(good[who]), tbuf, &tp);
  }
  *tp = '\0';

  if (key == 1) {
    notify_format(player, (gcount > 1) ? T("%s sense: %s%s%s") :
                  T("%s senses: %s%s%s"), tbuf + 4, Name(player), gap, arg2);
    p = tprintf("You sense: %s%s%s", Name(player), gap, arg2);
  } else {
    notify_format(player, T("You whisper, \"%s\"%s."), arg2, tbuf);
    p = tprintf(T("%s whispers%s: %s"), Name(player),
                gcount > 1 ? tbuf : "", arg2);
  }

  for (who = 0; who < gcount; who++) {
    notify_must_puppet(good[who], p);
    if (Location(good[who]) != Location(player))
      overheard = 0;
  }
  if (overheard) {
    dbref first = Contents(Location(player));
    if (!GoodObject(first))
      return;
    p = tprintf(T("%s whispers%s."), Name(player), tbuf);
    DOLIST(first, first) {
      overheard = 1;
      for (who = 0; who < gcount; who++) {
        if ((first == player) || (first == good[who])) {
          overheard = 0;
          break;
        }
      }
      if (overheard)
        notify_noecho(first, p);
    }
  }
  mush_free(tbuf, "string");
}

/** Send an @message to a list of dbrefs, using <attr> to format it
 * if present.
 * The list is destructively modified.
 * \param player the enactor.
 * \param list the list of players to pemit to, destructively modified.
 * \param attrib the ufun attribute to use to format the message.
 * \param message the default message.
 * \param flags PEMIT_* flags
 * \param numargs The number of arguments for the ufun.
 * \param ... The arguments for the ufun.
 */
void
do_message_list(dbref player, dbref enactor, char *list, char *attrname,
                char *message, int flags, int numargs, char *argv[])
{
  const char *start;
  char *current;
  char plist[BUFFER_LEN], *pp;
  dbref victim;
  int first = 0;
  ATTR *attrib;

  start = list;

  pp = plist;
  *pp = '\0';

  while (start && *start) {
    current = next_in_list(&start);
    if (*current == '*')
      current = current + 1;
    victim = noisy_match_result(player, current, NOTYPE, MAT_EVERYTHING);
    if (GoodObject(victim) && !IsGarbage(victim)) {
      /* Can we evaluate its <attribute> ? */

      attrib = atr_get(victim, upcasestr(attrname));
      if (attrib && CanEvalAttr(player, victim, attrib)) {
        if (flags & PEMIT_SPOOF) {
          messageformat(victim, attrname, enactor, NA_SPOOF, numargs, argv);
        } else {
          messageformat(victim, attrname, enactor, 0, numargs, argv);
        }
      } else {
        if (!first) {
          safe_chr(' ', plist, &pp);
        }
        first = 0;
        safe_dbref(victim, plist, &pp);
      }
    }
  }
  if (plist[0]) {
    *pp = '\0';
    do_pemit_list(enactor, plist, message, flags);
  }
}

/** Send a message to a list of dbrefs. To avoid repeated generation
 * of the NOSPOOF string, we set it up the first time we encounter
 * something Nospoof, and then check for it thereafter.
 * The list is destructively modified.
 * \param player the enactor.
 * \param list the list of players to pemit to, destructively modified.
 * \param message the message to pemit.
 * \param flags PEMIT_* flags
 */
void
do_pemit_list(dbref player, char *list, const char *message, int flags)
{
  char *bp, *p;
  char *nsbuf, *nspbuf;
  const char *l;
  dbref who;
  int nospoof;

  /* If no list or no message, further processing is pointless. */
  if (!message || !*message || !list || !*list)
    return;

  nspbuf = nsbuf = NULL;
  nospoof = (flags & PEMIT_SPOOF) ? 0 : 1;
  list[BUFFER_LEN - 1] = '\0';
  l = trim_space_sep(list, ' ');

  while (l && *l && (p = next_in_list(&l))) {
    who = noisy_match_result(player, p, NOTYPE, MAT_EVERYTHING);
    if (GoodObject(who) && okay_pemit(player, who)) {
      if (nospoof && Nospoof(who)) {
        if (Paranoid(who)) {
          if (!nspbuf) {
            bp = nspbuf = mush_malloc(BUFFER_LEN, "string");
            if (player == Owner(player))
              safe_format(nspbuf, &bp, "[%s(#%d)->] %s", Name(player),
                          player, message);
            else
              safe_format(nspbuf, &bp, "[%s(#%d)'s %s(#%d)->] %s",
                          Name(Owner(player)), Owner(player),
                          Name(player), player, message);
            *bp = '\0';
          }
          if (flags & PEMIT_PROMPT)
            notify_prompt(who, nspbuf);
          else
            notify(who, nspbuf);
        } else {
          if (!nsbuf) {
            bp = nsbuf = mush_malloc(BUFFER_LEN, "string");
            safe_format(nsbuf, &bp, "[%s->] %s", Name(player), message);
            *bp = '\0';
          }
          if (flags & PEMIT_PROMPT)
            notify_prompt(who, nsbuf);
          else
            notify(who, nsbuf);
        }
      } else {
        if (flags & PEMIT_PROMPT)
          notify_prompt_must_puppet(who, message);
        else
          notify_must_puppet(who, message);
      }
    }
  }
  if (nsbuf)
    mush_free(nsbuf, "string");
  if (nspbuf)
    mush_free(nspbuf, "string");

}

/** Send a message to an object.
 * \param player the enactor.
 * \param arg1 the name of the object to pemit to.
 * \param arg2 the message to pemit.
 * \param flags PEMIT_* flags.
 */
void
do_pemit(dbref player, const char *arg1, const char *arg2, int flags)
{
  dbref who;
  int silent, nospoof;

  if (!arg2 || !*arg2)
    return;

  silent = (flags & PEMIT_SILENT) ? 1 : 0;
  nospoof = (flags & PEMIT_SPOOF) ? 0 : 1;

  switch (who = match_result(player, arg1, NOTYPE,
                             MAT_OBJECTS | MAT_HERE | MAT_CONTAINER)) {
  case NOTHING:
    notify(player, T("I don't see that here."));
    break;
  case AMBIGUOUS:
    notify(player, T("I don't know who you mean!"));
    break;
  default:
    if (!okay_pemit(player, who)) {
      notify_format(player,
                    T("I'm sorry, but %s wishes to be left alone now."),
                    Name(who));
      return;
    }
    if (!silent)
      notify_format(player, T("You pemit \"%s\" to %s."), arg2, Name(who));
    if (nospoof && Nospoof(who)) {
      if (Paranoid(who)) {
        if (player == Owner(player))
          notify_format(who, "[%s(#%d)->%s] %s", Name(player), player,
                        Name(who), arg2);
        else
          notify_format(who, "[%s(#%d)'s %s(#%d)->%s] %s",
                        Name(Owner(player)), Owner(player),
                        Name(player), player, Name(who), arg2);
      } else
        notify_format(who, "[%s->%s] %s", Name(player), Name(who), arg2);
    } else {
      notify_must_puppet(who, arg2);
    }
    break;
  }
}

/** The pose and semipose command.
 * \param player the enactor.
 * \param tbuf1 the message to pose.
 * \param space if 1, put a space between name and pose; if 0, don't (semipose)
 */
void
do_pose(dbref player, const char *tbuf1, int space)
{
  dbref loc;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player) && !eval_lock(player, loc, Speech_Lock)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  /* notify everybody */
  if (!space)
    notify_except(Contents(loc), NOTHING,
                  tprintf("%s %s", spname(player), tbuf1), NA_INTER_HEAR);
  else
    notify_except(Contents(loc), NOTHING,
                  tprintf("%s%s", spname(player), tbuf1), NA_INTER_HEAR);
}

/** The *wall commands.
 * \param player the enactor.
 * \param message message to broadcast.
 * \param target type of broadcast (all, royalty, wizard)
 * \param emit if 1, this is a wallemit.
 */
void
do_wall(dbref player, const char *message, enum wall_type target, int emit)
{
  const char *gap = "", *prefix;
  const char *mask;
  int pose = 0;

  /* Only @wall is available to those with the announce power.
   * Only @rwall is available to royalty.
   */
  if (!(Wizard(player) ||
        ((target == WALL_ALL) && Can_Announce(player)) ||
        ((target == WALL_RW) && Royalty(player)))) {
    notify(player, T("Posing as a wizard could be hazardous to your health."));
    return;
  }
  /* put together the message and figure out what type it is */
  if (!emit) {
    gap = " ";
    switch (*message) {
    case SAY_TOKEN:
      if (CHAT_STRIP_QUOTE)
        message++;
      break;
    case SEMI_POSE_TOKEN:
      gap = "";
    case POSE_TOKEN:
      pose = 1;
      message++;
      break;
    }
  }

  if (!*message) {
    notify(player, T("What did you want to say?"));
    return;
  }
  if (target == WALL_WIZ) {
    /* to wizards only */
    mask = "WIZARD";
    prefix = WIZWALL_PREFIX;
  } else if (target == WALL_RW) {
    /* to wizards and royalty */
    mask = "WIZARD ROYALTY";
    prefix = RWALL_PREFIX;
  } else {
    /* to everyone */
    mask = NULL;
    prefix = WALL_PREFIX;
  }

  /* broadcast the message */
  if (pose)
    flag_broadcast(mask, 0, "%s %s%s%s", prefix, Name(player), gap, message);
  else if (emit)
    flag_broadcast(mask, 0, "%s [%s]: %s", prefix, Name(player), message);
  else
    flag_broadcast(mask, 0,
                   "%s %s %s, \"%s\"", prefix, Name(player),
                   target == WALL_ALL ? T("shouts") : T("says"), message);
}

/** messageformat. This is the wrapper that makes calling PAGEFORMAT,
 *  CHATFORMAT, etc easy.
 *
 * \param player The victim to call it on.
 * \param attribute The attribute on the player to call.
 * \param enactor The enactor who caused the message.
 * \param flags NA_INTER_HEAR and NA_SPOOF
 * \param arg0 First argument
 * \param arg4 Last argument.
 * \retval 1 The player had the fooformat attribute.
 * \retval 0 The default message was sent.
 */
int
vmessageformat(dbref player, const char *attribute, dbref enactor, int flags,
               int numargs, ...)
{
  va_list ap;
  char *s;
  int i;
  char *argv[10];

  va_start(ap, numargs);

  for (i = 0; i < 10; i++) {
    if (i < numargs) {
      /* Pop another char * off the stack. */
      s = va_arg(ap, char *);
      argv[i] = s;
    } else {
      argv[i] = NULL;
    }
  }
  va_end(ap);

  return messageformat(player, attribute, enactor, flags, numargs, argv);
}

/** messageformat. This is the wrapper that makes calling PAGEFORMAT,
 *  CHATFORMAT, etc easy.
 *
 * \param player The victim to call it on.
 * \param attribute The attribute on the player to call.
 * \param enactor The enactor who caused the message.
 * \param flags NA_INTER_HEAR and NA_SPOOF
 * \param arg0 First argument
 * \param arg4 Last argument.
 * \retval 1 The player had the fooformat attribute.
 * \retval 0 The default message was sent.
 */
int
messageformat(dbref player, const char *attribute, dbref enactor, int flags,
              int numargs, char *argv[])
{
  /* It's only static because I expect this thing to get
   * called a LOT, so it may or may not save time. */
  static char messbuff[BUFFER_LEN];

  *messbuff = '\0';
  if (call_attrib(player, attribute, (const char **) argv, numargs,
                  messbuff, enactor, NULL)) {
    /* We have a returned value. Notify the player. */
    if (*messbuff)
      notify_anything(enactor, na_one, &player, ns_esnotify, flags, messbuff);
    return 1;
  } else {
    return 0;
  }
}

/** The page command.
 * \param player the enactor.
 * \param arg1 the list of players to page.
 * \param arg2 the message to page.
 * \param cause the object that caused the command to run.
 * \param noeval if 1, page/noeval.
 * \param override if 1, page/override.
 * \param has_eq if 1, the command had an = in it.
 */
void
do_page(dbref player, const char *arg1, const char *arg2, dbref cause,
        int noeval, int override, int has_eq)
{
  dbref target;
  const char *message;
  const char *gap;
  int key;
  char *tbuf, *tp;
  char *tbuf2, *tp2;
  char *namebuf, *nbp;
  dbref good[100];
  int gcount = 0;
  char *msgbuf, *mb;
  char *nsbuf = NULL, *tosend;
  char *head;
  char *hp = NULL;
  const char **start;
  char *current;
  int i;
  int repage = 0;
  int fails_lock;
  int is_haven;
  ATTR *a;
  char *alias;

  tp2 = tbuf2 = (char *) mush_malloc(BUFFER_LEN, "page_buff");
  if (!tbuf2)
    mush_panic("Unable to allocate memory in do_page");

  nbp = namebuf = (char *) mush_malloc(BUFFER_LEN, "page_buff");

  if (*arg1 && has_eq) {
    /* page to=[msg]. Always evaluate to, maybe evaluate msg */
    process_expression(tbuf2, &tp2, &arg1, player, cause, cause,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *tp2 = '\0';
    head = tbuf2;
    message = arg2;
  } else if (arg2 && *arg2) {
    /* page =msg */
    message = arg2;
    repage = 1;
  } else {
    /* page msg */
    message = arg1;
    repage = 1;
  }
  if (repage) {
    a = atr_get_noparent(player, "LASTPAGED");
    if (!a || !*((hp = head = safe_atr_value(a)))) {
      notify(player, T("You haven't paged anyone since connecting."));
      mush_free(tbuf2, "page_buff");
      mush_free(namebuf, "page_buff");
      return;
    }
    if (!message || !*message) {
      notify_format(player, T("You last paged %s."), head);
      mush_free(tbuf2, "page_buff");
      mush_free(namebuf, "page_buff");
      if (hp)
        free(hp);
      return;
    }
  }

  tp = tbuf = (char *) mush_malloc(BUFFER_LEN, "page_buff");
  if (!tbuf)
    mush_panic("Unable to allocate memory in do_page");

  if (override && !Pemit_All(player)) {
    notify(player, T("Try again after you get the pemit_all power."));
    override = 0;
  }

  start = (const char **) &head;
  while (head && *head && (gcount < 99)) {
    current = next_in_list(start);
    target = lookup_player(current);
    if (!GoodObject(target))
      target = short_page(current);
    if (target == NOTHING) {
      notify_format(player,
                    T("I can't find who you're trying to page with: %s"),
                    current);
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
    } else if (target == AMBIGUOUS) {
      notify_format(player,
                    T("I'm not sure who you want to page with: %s"), current);
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
    } else {
      fails_lock = !(override || eval_lock(player, target, Page_Lock));
      is_haven = !override && Haven(target);
      if (!Connected(target) || (Dark(target) && (is_haven || fails_lock))) {
        /* A player isn't connected if they aren't connected, or if
         * they're DARK and HAVEN, or DARK and the pagelock fails. */
        page_return(player, target, "Away", "AWAY",
                    tprintf(T("%s is not connected."), Name(target)));
        if (fails_lock)
          fail_lock(player, target, Page_Lock, NULL, NOTHING);
        safe_chr(' ', tbuf, &tp);
        safe_str_space(current, tbuf, &tp);
      } else if (is_haven) {
        page_return(player, target, "Haven", "HAVEN",
                    tprintf(T("%s is not accepting any pages."), Name(target)));
        safe_chr(' ', tbuf, &tp);
        safe_str_space(Name(target), tbuf, &tp);
      } else if (fails_lock) {
        page_return(player, target, "Haven", "HAVEN",
                    tprintf(T("%s is not accepting your pages."),
                            Name(target)));
        fail_lock(player, target, Page_Lock, NULL, NOTHING);
        safe_chr(' ', tbuf, &tp);
        safe_str_space(Name(target), tbuf, &tp);
      } else {
        /* This is a good page */
        good[gcount] = target;
        gcount++;
      }
    }
  }

  /* Reset tbuf2 to use later */
  tp2 = tbuf2;

  /* We now have an array of good[] dbrefs, a gcount of the good ones,
   * and a tbuf with bad ones.
   */

  /* We don't know what the heck's going on here, but we're not paging
   * anyone, this looks like a spam attack. */
  if (gcount == 99) {
    notify(player, T("You're trying to page too many people at once."));
    mush_free(tbuf, "page_buff");
    mush_free(tbuf2, "page_buff");
    mush_free(namebuf, "page_buff");
    if (hp)
      free(hp);
    return;
  }

  /* We used to stick 'Unable to page' on at the start, but this is
   * faster for the 90% of the cases where there isn't a bad name
   * That may sound high, but, remember, we (almost) never have a bad
   * name if we're repaging, which is probably 75% of all pages */
  *tp = '\0';
  if (*tbuf)
    notify_format(player, T("Unable to page:%s"), tbuf);

  if (!gcount) {
    /* Well, that was a total waste of time. */
    mush_free(tbuf, "page_buff");
    mush_free(tbuf2, "page_buff");
    mush_free(namebuf, "page_buff");
    if (hp)
      free(hp);
    return;
  }

  /* Okay, we have a real page, the player can pay for it, and it's
   * actually going to someone. We're in this for keeps now. */

  /* Evaluate the message if we need to. */
  if (noeval) {
    msgbuf = NULL;
  } else {
    mb = msgbuf = (char *) mush_malloc(BUFFER_LEN, "page_buff");
    if (!msgbuf)
      mush_panic("Unable to allocate memory in do_page");

    process_expression(msgbuf, &mb, &message, player, cause, cause,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *mb = '\0';
    message = msgbuf;
  }

  if (Haven(player))
    notify(player, T("You are set HAVEN and cannot receive pages."));

  /* Figure out what kind of message */
  global_eval_context.wenv[0] = (char *) message;
  gap = " ";
  switch (*message) {
  case SEMI_POSE_TOKEN:
    gap = "";
  case POSE_TOKEN:
    key = 1;
    message++;
    break;
  default:
    key = 3;
    break;
  }

  /* Reset tbuf and tbuf2 to use later */
  tp = tbuf;
  tp2 = tbuf2;

  /* namebuf is used to hold a fancy formatted list of names,
   * with commas and the word 'and' , if needed. */
  /* tbuf holds a space-separated list of names for repaging */

  /* Set up a pretty formatted list. */
  for (i = 0; i < gcount; i++) {
    if (i)
      safe_chr(' ', tbuf, &tp);
    safe_str_space(Name(good[i]), tbuf, &tp);
    safe_itemizer(i + 1, (i == gcount - 1), ",", T("and"), " ", namebuf, &nbp);
    safe_str(Name(good[i]), namebuf, &nbp);
  }
  *tp = '\0';
  *nbp = '\0';
  (void) atr_add(player, "LASTPAGED", tbuf, GOD, 0);

  /* Reset tbuf to use later */
  tp = tbuf;

  /* Figure out the 'name' of the player */
  if ((alias = shortalias(player)) && *alias && PAGE_ALIASES)
    current = tprintf("%s (%s)", Name(player), alias);
  else
    current = (char *) Name(player);

  /* Now, build the thing we want to send to the pagees,
   * and put it in tbuf */

  /* Build the header */
  if (key == 1) {
    safe_str(T("From afar"), tbuf, &tp);
    if (gcount > 1) {
      safe_str(T(" (to "), tbuf, &tp);
      safe_str(namebuf, tbuf, &tp);
      safe_chr(')', tbuf, &tp);
    }
    safe_str(", ", tbuf, &tp);
    safe_str(current, tbuf, &tp);
    safe_str(gap, tbuf, &tp);
  } else {
    safe_str(current, tbuf, &tp);
    safe_str(T(" pages"), tbuf, &tp);
    if (gcount > 1) {
      safe_chr(' ', tbuf, &tp);
      safe_str(namebuf, tbuf, &tp);
    }
    safe_str(": ", tbuf, &tp);
  }
  /* Tack on the message */
  safe_str(message, tbuf, &tp);
  *tp = '\0';

  tp2 = tbuf2;
  for (i = 0; i < gcount; i++) {
    if (i)
      safe_chr(' ', tbuf2, &tp2);
    safe_dbref(good[i], tbuf2, &tp2);
  }
  *tp2 = '\0';
  /* Figure out the one success message, and send it */
  tosend = mush_malloc(BUFFER_LEN, "page_buff");
  if (key == 1) {
    snprintf(tosend, BUFFER_LEN, T("Long distance to %s: %s%s%s"), namebuf,
             Name(player), gap, message);
  } else {
    snprintf(tosend, BUFFER_LEN, T("You paged %s with '%s'"), namebuf, message);
  }
  if (!vmessageformat(player, "OUTPAGEFORMAT", player, 0, 5, message,
                      (key == 1) ? (*gap ? ":" : ";") : "\"",
                      (alias && *alias) ? alias : "", tbuf2, tosend)) {
    notify(player, tosend);
  }
  mush_free(tosend, "page_buff");

  /* And send the page to everyone. */
  for (i = 0; i < gcount; i++) {
    tosend = tbuf;
    if (!IsPlayer(player) && Nospoof(good[i])) {
      if (nsbuf == NULL) {
        nsbuf = mush_malloc(BUFFER_LEN, "page buffer");
        snprintf(nsbuf, BUFFER_LEN, "[#%d] %s", player, tbuf);
      }
      tosend = nsbuf;
    }
    if (!vmessageformat(good[i], "PAGEFORMAT", player, 0, 5, message,
                        (key == 1) ? (*gap ? ":" : ";") : "\"",
                        (alias && *alias) ? alias : "", tbuf2, tbuf)) {
      /* Player doesn't have Pageformat, or it eval'd to 0 */
      notify(good[i], tosend);
    }

    page_return(player, good[i], "Idle", "IDLE", NULL);
  }

  mush_free(tbuf, "page_buff");
  mush_free(tbuf2, "page_buff");
  mush_free(namebuf, "page_buff");
  if (msgbuf)
    mush_free(msgbuf, "page_buff");
  if (nsbuf)
    mush_free(nsbuf, "page_buff");
  if (hp)
    free(hp);
}


/** Does a message match a filter pattern on an object?
 * \param thing object with the filter.
 * \param msg message to match.
 * \param flag if 0, filter; if 1, infilter.
 * \retval 1 message matches filter.
 * \retval 0 message does not match filter.
 */
int
filter_found(dbref thing, const char *msg, int flag)
{
  char *filter;
  ATTR *a;
  char *p, *bp;
  char *temp;                   /* need this so we don't leak memory
                                 * by failing to free the storage
                                 * allocated by safe_uncompress
                                 */
  int i;
  int matched = 0;

  if (!flag)
    a = atr_get(thing, "FILTER");
  else
    a = atr_get(thing, "INFILTER");
  if (!a)
    return matched;

  filter = safe_atr_value(a);
  temp = filter;

  for (i = 0; (i < MAX_ARG) && !matched; i++) {
    p = bp = filter;
    process_expression(p, &bp, (char const **) &filter, 0, 0, 0,
                       PE_NOTHING, PT_COMMA, NULL);
    if (*filter == ',')
      *filter++ = '\0';
    if (*p == '\0' && *filter == '\0')  /* No more filters */
      break;
    if (*p == '\0')             /* Empty filter */
      continue;
    if (AF_Regexp(a))
      matched = quick_regexp_match(p, msg, AF_Case(a));
    else
      matched = local_wild_match_case(p, msg, AF_Case(a));
  }

  free(temp);
  return matched;
}

/** Copy a message into a buffer, adding an object's PREFIX attribute.
 * \param thing object with prefix attribute.
 * \param msg message.
 * \param tbuf1 destination buffer.
 */
void
make_prefixstr(dbref thing, const char *msg, char *tbuf1)
{
  char *bp, *asave;
  char const *ap;
  char *wsave[10], *preserve[NUMQ];
  ATTR *a;
  int j;

  a = atr_get(thing, "PREFIX");

  bp = tbuf1;

  if (!a) {
    safe_str(T("From "), tbuf1, &bp);
    safe_str(Name(IsExit(thing) ? Source(thing) : thing), tbuf1, &bp);
    safe_str(", ", tbuf1, &bp);
  } else {
    for (j = 0; j < 10; j++) {
      wsave[j] = global_eval_context.wenv[j];
      global_eval_context.wenv[j] = NULL;
    }
    global_eval_context.wenv[0] = (char *) msg;
    save_global_regs("prefix_save", preserve);
    asave = safe_atr_value(a);
    ap = asave;
    process_expression(tbuf1, &bp, &ap, thing, orator, orator,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    free(asave);
    restore_global_regs("prefix_save", preserve);
    for (j = 0; j < 10; j++)
      global_eval_context.wenv[j] = wsave[j];
    if (bp != tbuf1)
      safe_chr(' ', tbuf1, &bp);
  }
  safe_str(msg, tbuf1, &bp);
  *bp = '\0';
  return;
}

/** pass a message on, for AUDIBLE, prepending a prefix, unless the
 * message matches a filter pattern.
 * \param thing object to check for filter and prefix.
 * \param msg message to pass.
 */
void
propagate_sound(dbref thing, const char *msg)
{
  char tbuf1[BUFFER_LEN];
  dbref loc = Location(thing);
  dbref pass[2];

  if (!GoodObject(loc))
    return;

  /* check to see that filter doesn't suppress message */
  if (filter_found(thing, msg, 0))
    return;

  /* figure out the prefix */
  make_prefixstr(thing, msg, tbuf1);

  /* Exits pass the message on to things in the next room.
   * Objects pass the message on to the things outside.
   * Don't tell yourself your own message.
   */

  if (IsExit(thing)) {
    notify_anything(orator, na_next, &Contents(loc), NULL, NA_INTER_HEAR,
                    tbuf1);
  } else {
    pass[0] = Contents(loc);
    pass[1] = thing;
    notify_anything(orator, na_nextbut, pass, NULL, NA_INTER_HEAR, tbuf1);
  }
}

static void
do_audible_stuff(dbref loc, dbref *excs, int numexcs, const char *msg)
{
  dbref e;
  int exclude = 0;
  int i;

  if (!Audible(loc))
    return;

  if (IsRoom(loc)) {
    DOLIST(e, Exits(loc)) {
      if (Audible(e))
        propagate_sound(e, msg);
    }
  } else {
    for (i = 0; i < numexcs; i++)
      if (*(excs + i) == loc)
        exclude = 1;
    if (!exclude)
      propagate_sound(loc, msg);
  }
}

/** notify_anthing() wrapper to notify everyone in a location except one
 * object.
 * \param first object in location to notify.
 * \param exception dbref of object not to notify, or NOTHING.
 * \param msg message to send.
 * \param flags flags to pass to notify_anything().
 */
void
notify_except(dbref first, dbref exception, const char *msg, int flags)
{
  dbref loc;
  dbref pass[2];

  if (!GoodObject(first))
    return;
  loc = Location(first);
  if (!GoodObject(loc))
    return;

  if (exception == NOTHING)
    exception = AMBIGUOUS;

  pass[0] = loc;
  pass[1] = exception;

  notify_anything(orator, na_except, pass, ns_esnotify, flags, msg);

  do_audible_stuff(loc, &pass[1], 1, msg);
}

/** notify_anthing() wrapper to notify everyone in a location except two
 * objects.
 * \param first object in location to notify.
 * \param exc1 dbref of one object not to notify, or NOTHING.
 * \param exc2 dbref of another object not to notify, or NOTHING.
 * \param msg message to send.
 * \param flags interaction flags to control type of interaction.
 */
void
notify_except2(dbref first, dbref exc1, dbref exc2, const char *msg, int flags)
{
  dbref loc;
  dbref pass[3];

  if (!GoodObject(first))
    return;
  loc = Location(first);
  if (!GoodObject(loc))
    return;

  if (exc1 == NOTHING)
    exc1 = AMBIGUOUS;
  if (exc2 == NOTHING)
    exc2 = AMBIGUOUS;

  pass[0] = loc;
  pass[1] = exc1;
  pass[2] = exc2;

  notify_anything(orator, na_except2, pass, ns_esnotify, flags, msg);

  do_audible_stuff(loc, &pass[1], 2, msg);
}

/** The think command.
 * \param player the enactor.
 * \param message the message to think.
 */
void
do_think(dbref player, const char *message)
{
  notify(player, message);
}


/** The emit command.
 * \verbatim
 * This implements @emit.
 * \endverbatim
 * \param player the enactor.
 * \param tbuf1 the message to emit.
 * \param flags bitmask of notification flags.
 */
void
do_emit(dbref player, const char *tbuf1, int flags)
{
  dbref loc;
  int na_flags = NA_INTER_HEAR;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player) && !eval_lock(player, loc, Speech_Lock)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  /* notify everybody */
  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  notify_anything(player, na_loc, &loc, ns_esnotify, na_flags, tbuf1);

  do_audible_stuff(loc, NULL, 0, tbuf1);
}

/** Remit a message to a single room.
 * \param player the enactor.
 * \param target string containing dbref of room to remit in.
 * \param msg message to emit.
 * \param flags PEMIT_* flags
 */
static void
do_one_remit(dbref player, const char *target, const char *msg, int flags)
{
  dbref room;
  int na_flags = NA_INTER_HEAR;
  room = match_result(player, target, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(room)) {
    notify(player, T("I can't find that."));
  } else {
    if (IsExit(room)) {
      notify(player, T("There can't be anything in that!"));
    } else if (!okay_pemit(player, room)) {
      notify_format(player,
                    T("I'm sorry, but %s wishes to be left alone now."),
                    Name(room));
    } else if (!Loud(player) && !eval_lock(player, room, Speech_Lock)) {
      fail_lock(player, room, Speech_Lock, T("You may not speak there!"),
                NOTHING);
    } else {
      if (!(flags & PEMIT_SILENT) && (Location(player) != room)) {
        const char *rmno;
        rmno = unparse_object(player, room);
        notify_format(player, T("You remit, \"%s\" in %s"), msg, rmno);
      }
      if (flags & PEMIT_SPOOF)
        na_flags |= NA_SPOOF;
      notify_anything_loc(player, na_loc, &room, ns_esnotify, na_flags,
                          msg, room);
      do_audible_stuff(room, NULL, 0, msg);
    }
  }
}

/** Remit a message
 * \verbatim
 * This implements @remit.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 string containing dbref(s) of rooms to remit it.
 * \param arg2 message to emit.
 * \param flags for remit.
 */
void
do_remit(dbref player, char *arg1, const char *arg2, int flags)
{
  if (flags & PEMIT_LIST) {
    /* @remit/list */
    char *current;
    arg1 = trim_space_sep(arg1, ' ');
    while ((current = split_token(&arg1, ' ')) != NULL)
      do_one_remit(player, current, arg2, flags);
  } else {
    do_one_remit(player, arg1, arg2, flags);
  }
}

/** Emit a message to the absolute location of enactor.
 * \param player the enactor.
 * \param tbuf1 message to emit.
 * \param flags bitmask of notification flags.
 */
void
do_lemit(dbref player, const char *tbuf1, int flags)
{
  /* give a message to the "absolute" location of an object */
  dbref room;
  int na_flags = NA_INTER_HEAR;
  int silent = (flags & PEMIT_SILENT) ? 1 : 0;

  /* only players and things may use this command */
  if (!Mobile(player))
    return;

  room = absolute_room(player);
  if (!GoodObject(room) || !IsRoom(room)) {
    notify(player, T("Too many containers."));
    return;
  } else if (!Loud(player) && !eval_lock(player, room, Speech_Lock)) {
    fail_lock(player, room, Speech_Lock, T("You may not speak there!"),
              NOTHING);
    return;
  } else {
    if (!silent && (Location(player) != room))
      notify_format(player, T("You lemit: \"%s\""), tbuf1);
    if (flags & PEMIT_SPOOF)
      na_flags |= NA_SPOOF;
    notify_anything(player, na_loc, &room, ns_esnotify, na_flags, tbuf1);
  }
}

/** notify_anything() function for zone emits.
 * \param current unused.
 * \param data array of notify data.
 * \return last object in zone, or NOTHING.
 */
dbref
na_zemit(dbref current __attribute__ ((__unused__)), void *data)
{
  dbref this;
  dbref room;
  dbref *dbrefs = data;
  this = dbrefs[0];
  do {
    if (this == NOTHING) {
      for (room = dbrefs[1]; room < db_top; room++) {
        if (IsRoom(room) && (Zone(room) == dbrefs[2])
            && (Loud(dbrefs[3]) || eval_lock(dbrefs[3], room, Speech_Lock))
          )
          break;
      }
      if (!(room < db_top))
        return NOTHING;
      this = room;
      dbrefs[1] = room + 1;
    } else if (IsRoom(this)) {
      this = Contents(this);
    } else {
      this = Next(this);
    }
  } while ((this == NOTHING) || (this == dbrefs[4]));
  dbrefs[0] = this;
  return this;
}

/** The zemit command.
 * \verbatim
 * This implements @zemit and @nszemit.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 string containing dbref of ZMO.
 * \param arg2 message to emit.
 * \param flags bitmask of notificati flags.
 */
void
do_zemit(dbref player, const char *arg1, const char *arg2, int flags)
{
  const char *where;
  dbref zone;
  dbref pass[5];
  int na_flags = NA_INTER_HEAR;

  zone = match_result(player, arg1, NOTYPE, MAT_ABSOLUTE);
  if (!GoodObject(zone)) {
    notify(player, T("Invalid zone."));
    return;
  }
  if (!controls(player, zone)) {
    notify(player, T("Permission denied."));
    return;
  }

  where = unparse_object(player, zone);
  notify_format(player, T("You zemit, \"%s\" in zone %s"), arg2, where);

  pass[0] = NOTHING;
  pass[1] = 0;
  pass[2] = zone;
  pass[3] = player;
  pass[4] = player;
  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  notify_anything(player, na_zemit, pass, ns_esnotify, na_flags, arg2);
}
