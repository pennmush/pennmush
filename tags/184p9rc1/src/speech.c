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

static void do_one_remit(dbref player, const char *target, const char *msg,
                         int flags, struct format_msg *format,
                         NEW_PE_INFO *pe_info);
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
 * \param dofails If nonzero, send failure message 'def' or run fail_lock()
 * \param def show a default message if there is no appropriate failure message?
 * \param pe_info the pe_info for page lock evaluation
 * \retval 1 player may pemit to target.
 * \retval 0 player may not pemit to target.
 */
int
okay_pemit(dbref player, dbref target, int dofails, int def,
           NEW_PE_INFO *pe_info)
{
  char defmsg[BUFFER_LEN];
  char *dp = NULL;
  if (Pemit_All(player))
    return 1;

  if (dofails && def) {
    dp = defmsg;
    safe_format(defmsg, &dp,
                T("I'm sorry, but %s wishes to be left alone now."),
                Name(target));
    *dp = '\0';
    dp = defmsg;
  }

  if (IsPlayer(target) && Haven(target)) {
    if (dofails && def)
      notify(player, dp);
    return 0;
  }
  if (!eval_lock_with(player, target, Page_Lock, pe_info)) {
    if (dofails) {
      fail_lock(player, target, Page_Lock, dp, NOTHING);
    }
    return 0;
  }
  return 1;
}

/** This is the place where speech, poses, and @emits by thing should be
 *  heard. For things and players, it's the loc; for rooms, it's the room
 *  itself; for exits, it's the source.
 */
dbref
speech_loc(dbref thing)
{
  if (!RealGoodObject(thing))
    return NOTHING;
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    return thing;
  case TYPE_EXIT:
    return Source(thing);
  default:
    return Location(thing);
  }
}

/** The teach command.
 * \param player the enactor.
 * \param tbuf1 the command being taught.
 * \param list is tbuf1 an action list, or a single command?
 * \param parent_queue the queue entry to run the command in
 */
void
do_teach(dbref player, const char *tbuf1, int list, MQUE *parent_queue)
{
  dbref loc;
  int flags = QUEUE_RECURSE;
  char lesson[BUFFER_LEN], *lp;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player)
      && !eval_lock_with(player, loc, Speech_Lock, parent_queue->pe_info)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  if (!tbuf1 || !*tbuf1) {
    notify(player, T("What command do you want to teach?"));
    return;
  }

  if (!list)
    flags |= QUEUE_NOLIST;

  lp = lesson;
  safe_format(lesson, &lp, T("%s types --> %s%s%s"), spname(player),
              ANSI_HILITE, tbuf1, ANSI_END);
  *lp = '\0';
  notify_anything(player, na_loc, &loc, NULL, NA_INTER_HEAR | NA_PROPAGATE,
                  lesson, NULL, loc, NULL);
  new_queue_actionlist(player, parent_queue->enactor, player, (char *) tbuf1,
                       parent_queue, PE_INFO_SHARE, flags, NULL);
}

/** The say command.
 * \param player the enactor.
 * \param message the message to say.
 * \param pe_info pe_info to eval speechmod with
 */
void
do_say(dbref player, const char *message, NEW_PE_INFO *pe_info)
{
  dbref loc;
  PE_REGS *pe_regs;
  char modmsg[BUFFER_LEN];
  char says[BUFFER_LEN];
  char *sp;
  int mod = 0;
  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player) && !eval_lock_with(player, loc, Speech_Lock, pe_info)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  if (*message == SAY_TOKEN && CHAT_STRIP_QUOTE)
    message++;

  pe_regs = pe_regs_create(PE_REGS_ARG, "do_say");
  pe_regs_setenv_nocopy(pe_regs, 0, message);
  pe_regs_setenv_nocopy(pe_regs, 1, "\"");
  modmsg[0] = '\0';

  if (call_attrib(player, "SPEECHMOD", modmsg, player, pe_info, pe_regs)
      && *modmsg != '\0')
    mod = 1;
  pe_regs_free(pe_regs);

  /* notify everybody */
  notify_format(player, T("You say, \"%s\""), (mod ? modmsg : message));
  sp = says;
  safe_format(says, &sp, T("%s says, \"%s\""), spname(player),
              (mod ? modmsg : message));
  *sp = '\0';
  notify_except(loc, player, says, NA_INTER_HEAR);
}

/** The oemit(/list) command.
 * \verbatim
 * This implements @oemit and @oemit/list.
 * \endverbatim
 * \param player the enactor.
 * \param list the list of dbrefs to oemit from the emit.
 * \param message the message to emit.
 * \param flags PEMIT_* flags.
 * \param format a format_msg structure to pass to notify_anything() from \@message
 * \param pe_info the pe_info to use for evaluating speech locks
 */
void
do_oemit_list(dbref player, char *list, const char *message, int flags,
              struct format_msg *format, NEW_PE_INFO *pe_info)
{
  char *temp, *p;
  const char *s;
  dbref who;
  dbref room;
  int matched = 0;
  dbref pass[11];
  dbref locs[10];
  int i, oneloc = 0;
  int na_flags = NA_INTER_HEAR | NA_PROPAGATE;

  /* If no message, further processing is pointless.
   * If no list, they should have used @remit. */
  if (!message || !*message || !list || !*list)
    return;

  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;

  for (i = 0; i < 11; i++)
    pass[i] = NOTHING;

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
    room = noisy_match_result(player, list, NOTYPE, MAT_EVERYTHING);
    if (!GoodObject(room)) {
      notify(player, T("I can't find that room."));
      return;
    }

    if (!Loud(player) && !eval_lock_with(player, room, Speech_Lock, pe_info)) {
      fail_lock(player, room, Speech_Lock, T("You may not speak there!"),
                NOTHING);
      return;
    }

    oneloc = 1;                 /* we are only oemitting to one location */
  } else {
    temp = list;
  }

  s = temp;
  while (s && *s) {
    p = next_in_list(&s);
    /* If a room was given, we match relative to the room */
    if (oneloc)
      who = match_result_relative(player, room, p, NOTYPE, MAT_OBJ_CONTENTS);
    else
      who = noisy_match_result(player, p, NOTYPE, MAT_OBJECTS);
    /* matched tracks the number of valid players we've found.
     * room is the given room (possibly nothing right now)
     * pass[0..10] are dbrefs of players
     * locs[0..10] are corresponding dbrefs of locations
     * pass[11] is always NOTHING
     */
    if (GoodObject(who) && GoodObject(Location(who))
        && (Loud(player) || (oneloc && Location(who) == room) ||
            eval_lock_with(player, Location(who), Speech_Lock, pe_info))
      ) {
      if (matched < 10) {
        locs[matched] = Location(who);
        pass[matched] = who;
        matched++;
      } else {
        notify(player, T("Too many people to oemit to."));
        break;
      }
    }
  }

  if (!matched) {
    if (oneloc) {
      /* A specific location was given, but there were no matching objects to
       * omit, so just remit */
      notify_anything(orator, na_loc, &room, NULL, na_flags, message, NULL,
                      room, format);
    } else {
      notify(player, T("No matching objects."));
    }
    return;
  }

  /* Sort the list of rooms to oemit to so we don't oemit to the same
   * room twice */
  qsort((void *) locs, matched, sizeof(locs[0]), dbref_comp);

  for (i = 0; i < matched; i++) {
    if (i != 0 && locs[i] == locs[i - 1])
      continue;
    notify_anything(orator, na_loc, &locs[i], pass, na_flags, message, NULL,
                    locs[i], format);
  }

}


/** The whisper command.
 * \param player the enactor.
 * \param arg1 name of the object to whisper to.
 * \param arg2 message to whisper.
 * \param noisy if 1, others overhear that a whisper has occurred.
 * \param pe_info the pe_info for evaluating interact locks
 */
void
do_whisper(dbref player, const char *arg1, const char *arg2, int noisy,
           NEW_PE_INFO *pe_info)
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
    mush_panic("Unable to allocate memory in do_whisper");

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
    if (!GoodObject(who) || !can_interact(player, who, INTERACT_HEAR, pe_info)) {
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

/** Send an \@message to a list of dbrefs, using an attribute to format it
 * if present.
 * \param executor the executor.
 * \param list the list of players to pemit to, destructively modified.
 * \param attrname the attribute to use to format the message.
 * \param message the default message.
 * \param type the type of emit to send (pemit/remit/oemit)
 * \param flags PEMIT_* flags
 * \param numargs The number of arguments for the ufun.
 * \param argv The arguments for the ufun.
 * \param pe_info the pe_info for lock checks, etc
 */
void
do_message(dbref executor, char *list, char *attrname,
           char *message, enum emit_type type, int flags, int numargs,
           char *argv[], NEW_PE_INFO *pe_info)
{
  struct format_msg format;
  dbref thing;
  char *p;
  int i;

  if (!attrname || !*attrname)
    return;

  format.checkprivs = 1;
  format.thing = AMBIGUOUS;

  p = attrname;

  if ((p = strchr(attrname, '/')) != NULL) {
    *p++ = '\0';
    if (*attrname && strcmp(attrname, "#-2")) {
      thing = noisy_match_result(executor, attrname, NOTYPE, MAT_EVERYTHING);
      if (thing == NOTHING)
        return;
      format.thing = thing;
    }
  } else
    p = attrname;

  format.attr = p;
  format.numargs = numargs;
  format.targetarg = -1;

  for (i = 0; i < numargs; i++) {
    format.args[i] = argv[i];
    if (!strcmp(argv[i], "##"))
      format.targetarg = i;
  }

  switch (type) {
  case EMIT_REMIT:
    do_remit(executor, list, message, flags, &format, pe_info);
    break;
  case EMIT_OEMIT:
    do_oemit_list(executor, list, message, flags, &format, pe_info);
    break;
  case EMIT_PEMIT:
    do_pemit(executor, list, message, flags, &format, pe_info);
    break;
  }

}

/** Send a message to an object.
 * \param player the enactor.
 * \param target the name(s) of the object(s) to pemit to.
 * \param message the message to pemit.
 * \param flags PEMIT_* flags.
 * \param format a format_msg structure to pass to notify_anything() from \@message
 * \param pe_info the pe_info for lock checks, etc
 */
void
do_pemit(dbref player, char *target, const char *message, int flags,
         struct format_msg *format, NEW_PE_INFO *pe_info)
{
  dbref who, last = NOTHING;
  int na_flags = NA_MUST_PUPPET;
  const char *l = NULL;
  char *p;
  int one = 1;
  int count = 0;

  if (!target || !*target || !message || !*message)
    return;

  if (flags & PEMIT_SPOOF)
    na_flags = NA_SPOOF;
  if (flags & PEMIT_PROMPT)
    na_flags = NA_PROMPT;

  if (flags & PEMIT_LIST) {
    l = trim_space_sep(target, ' ');
    p = next_in_list(&l);
    one = 0;
  } else {
    p = target;
  }

  do {
    who = noisy_match_result(player, p, NOTYPE, MAT_EVERYTHING);
    if (who == NOTHING)
      continue;
    if (!okay_pemit(player, who, 1, one, pe_info))
      continue;
    count++;
    last = who;
    notify_anything(orator, na_one, &who, NULL, na_flags, message, NULL,
                    AMBIGUOUS, format);
  } while (!one && l && *l && (p = next_in_list(&l)));


  if (!(flags & PEMIT_SILENT) && count) {
    if (count > 1)
      notify_format(player, T("You pemit \"%s\" to %d objects."), message,
                    count);
    else if (last != player)
      notify_format(player, T("You pemit \"%s\" to %s."), message, Name(last));
  }

}

/** The pose and semipose command.
 * \param player the enactor.
 * \param tbuf1 the message to pose.
 * \param nospace if 1, omit space between name and pose (semipose); if 0, include space (pose)
 * \param pe_info the pe_info for speechmod, lock checks, etc
 */
void
do_pose(dbref player, const char *tbuf1, int nospace, NEW_PE_INFO *pe_info)
{
  dbref loc;
  char tbuf2[BUFFER_LEN], message[BUFFER_LEN], *mp;
  PE_REGS *pe_regs;
  int mod = 0;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player) && !eval_lock_with(player, loc, Speech_Lock, pe_info)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  pe_regs = pe_regs_create(PE_REGS_ARG, "do_pose");
  pe_regs_setenv_nocopy(pe_regs, 0, tbuf1);
  pe_regs_setenv_nocopy(pe_regs, 1, nospace ? ";" : ":");
  tbuf2[0] = '\0';

  if (call_attrib(player, "SPEECHMOD", tbuf2, player, pe_info, pe_regs)
      && *tbuf2 != '\0')
    mod = 1;

  pe_regs_free(pe_regs);

  mp = message;
  safe_format(message, &mp, (nospace ? "%s%s" : "%s %s"), spname(player),
              (mod ? tbuf2 : tbuf1));
  *mp = '\0';

  notify_anything(player, na_loc, &loc, NULL, NA_INTER_HEAR | NA_PROPAGATE,
                  message, NULL, loc, NULL);
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
 * \param flags NA_* flags to send in addition to NA_INTER_HEAR and NA_SPOOF
 * \param numargs the number of arguments to the attribute
 * \param ... the arguments to the attribute
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
 * \param flags flags NA_* flags to send in addition to NA_INTER_HEAR and NA_SPOOF
 * \param numargs number of arguments in argv
 * \param argv array of arguments
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
  PE_REGS *pe_regs;
  int i;
  int ret;

  flags |= NA_INTER_HEAR | NA_SPOOF;

  *messbuff = '\0';
  pe_regs = pe_regs_create(PE_REGS_ARG, "messageformat");
  for (i = 0; i < numargs && i < 10; i++) {
    pe_regs_setenv_nocopy(pe_regs, i, argv[i]);
  }
  ret = call_attrib(player, attribute, messbuff, enactor, NULL, pe_regs);
  pe_regs_free(pe_regs);
  if (ret) {
    /* We have a returned value. Notify the player. */
    if (*messbuff)
      notify_anything(enactor, na_one, &player, NULL, flags, messbuff, NULL,
                      AMBIGUOUS, NULL);
    return 1;
  } else {
    return 0;
  }
}

/** The page command.
 * \param executor the executor.
 * \param arg1 the list of players to page.
 * \param arg2 the message to page.
 * \param override if 1, page/override.
 * \param has_eq if 1, the command had an = in it.
 * \param pe_info the pe_info to use when evaluating locks, idle/away/haven msg, etc
 */
void
do_page(dbref executor, const char *arg1, const char *arg2, int override,
        int has_eq, NEW_PE_INFO *pe_info)
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
  char alias[BUFFER_LEN], *ap;

  tp2 = tbuf2 = (char *) mush_malloc(BUFFER_LEN, "page_buff");
  if (!tbuf2)
    mush_panic("Unable to allocate memory in do_page");

  nbp = namebuf = (char *) mush_malloc(BUFFER_LEN, "page_buff");

  if (*arg1 && has_eq) {
    /* page to=[msg] */
    head = (char *) arg1;
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
    a = atr_get_noparent(executor, "LASTPAGED");
    if (!a || !*((hp = head = safe_atr_value(a)))) {
      notify(executor, T("You haven't paged anyone since connecting."));
      mush_free(tbuf2, "page_buff");
      mush_free(namebuf, "page_buff");
      return;
    }
    if (!message || !*message) {
      start = (const char **) &head;
      while (head && *head) {
        current = next_in_list(start);
        if (is_objid(current))
          target = parse_objid(current);
        else
          target = lookup_player(current);
        if (RealGoodObject(target)) {
          good[gcount] = target;
          gcount++;
        }
      }
      if (!gcount) {
        notify(executor, T("I can't find who you last paged."));
      } else {
        for (repage = 1; repage <= gcount; repage++) {
          safe_itemizer(repage, (repage == gcount), ",", T("and"), " ", tbuf2,
                        &tp2);
          safe_str(Name(good[repage - 1]), tbuf2, &tp2);
        }
        *tp2 = '\0';
        notify_format(executor, T("You last paged %s."), tbuf2);
      }
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

  if (override && !Pemit_All(executor)) {
    notify(executor, T("Try again after you get the pemit_all power."));
    override = 0;
  }

  start = (const char **) &head;
  while (head && *head && (gcount < 99)) {
    current = next_in_list(start);
    target = lookup_player(current);
    if (!GoodObject(target))
      target = short_page(current);
    if (target == NOTHING) {
      notify_format(executor,
                    T("I can't find who you're trying to page with: %s"),
                    current);
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
    } else if (target == AMBIGUOUS) {
      notify_format(executor,
                    T("I'm not sure who you want to page with: %s"), current);
      safe_chr(' ', tbuf, &tp);
      safe_str_space(current, tbuf, &tp);
    } else {
      fails_lock = !(override
                     || eval_lock_with(executor, target, Page_Lock, pe_info));
      is_haven = !override && Haven(target);
      if (!Connected(target) || (Dark(target) && (is_haven || fails_lock))) {
        /* A player isn't connected if they aren't connected, or if
         * they're DARK and HAVEN, or DARK and the pagelock fails. */
        page_return(executor, target, "Away", "AWAY",
                    tprintf(T("%s is not connected."), Name(target)));
        if (fails_lock)
          fail_lock(executor, target, Page_Lock, NULL, NOTHING);
        safe_chr(' ', tbuf, &tp);
        safe_str_space(Name(target), tbuf, &tp);
      } else if (is_haven) {
        page_return(executor, target, "Haven", "HAVEN",
                    tprintf(T("%s is not accepting any pages."), Name(target)));
        safe_chr(' ', tbuf, &tp);
        safe_str_space(Name(target), tbuf, &tp);
      } else if (fails_lock) {
        page_return(executor, target, "Haven", "HAVEN",
                    tprintf(T("%s is not accepting your pages."),
                            Name(target)));
        fail_lock(executor, target, Page_Lock, NULL, NOTHING);
        safe_chr(' ', tbuf, &tp);
        safe_str_space(Name(target), tbuf, &tp);
      } else {
        /* This is a good page */
        good[gcount] = target;
        gcount++;
      }
    }
  }

  /* We now have an array of good[] dbrefs, a gcount of the good ones,
   * and a tbuf with bad ones.
   */

  if (gcount == 99) {
    /* We don't know what the heck's going on here, but we're not paging
     * anyone, this looks like a spam attack. */
    notify(executor, T("You're trying to page too many people at once."));
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
    notify_format(executor, T("Unable to page:%s"), tbuf);

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

  if (Haven(executor))
    notify(executor, T("You are set HAVEN and cannot receive pages."));

  /* Figure out what kind of message */
  /*G_E_C.wenv[0] = (char *) message; <-- Not sure why this was done */
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
  /* tbuf holds a space-separated list of objids for repaging */

  /* Set up a pretty formatted list. */
  for (i = 0; i < gcount; i++) {
    if (i)
      safe_chr(' ', tbuf, &tp);
    safe_dbref(good[i], tbuf, &tp);
    safe_chr(':', tbuf, &tp);
    safe_integer(CreTime(good[i]), tbuf, &tp);
    safe_itemizer(i + 1, (i == gcount - 1), ",", T("and"), " ", namebuf, &nbp);
    safe_str(Name(good[i]), namebuf, &nbp);
  }
  *tp = '\0';
  *nbp = '\0';
  (void) atr_add(executor, "LASTPAGED", tbuf, GOD, 0);

  /* Reset tbuf to use later */
  tp = tbuf;

  /* Figure out the 'name' of the player */
  if ((ap = shortalias(executor)) && *ap) {
    strcpy(alias, ap);
    if (PAGE_ALIASES && strcasecmp(ap, Name(executor)))
      current = tprintf("%s (%s)", Name(executor), alias);
    else
      current = (char *) Name(executor);
  } else {
    alias[0] = '\0';
    current = (char *) Name(executor);
  }

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
             Name(executor), gap, message);
  } else {
    snprintf(tosend, BUFFER_LEN, T("You paged %s with '%s'"), namebuf, message);
  }
  if (!vmessageformat(executor, "OUTPAGEFORMAT", executor, 0, 5, message,
                      (key == 1) ? (*gap ? ":" : ";") : "\"",
                      (*alias) ? alias : "", tbuf2, tosend)) {
    notify(executor, tosend);
  }
  mush_free(tosend, "page_buff");

  /* And send the page to everyone. */
  for (i = 0; i < gcount; i++) {
    tosend = tbuf;
    if (!IsPlayer(executor) && Nospoof(good[i])) {
      if (nsbuf == NULL) {
        nsbuf = mush_malloc(BUFFER_LEN, "page buffer");
        snprintf(nsbuf, BUFFER_LEN, "[#%d] %s", executor, tbuf);
      }
      tosend = nsbuf;
    }
    if (!vmessageformat(good[i], "PAGEFORMAT", executor, 0, 5, message,
                        (key == 1) ? (*gap ? ":" : ";") : "\"",
                        (*alias) ? alias : "", tbuf2, tbuf)) {
      /* Player doesn't have Pageformat, or it eval'd to 0 */
      notify(good[i], tosend);
    }

    page_return(executor, good[i], "Idle", "IDLE", NULL);
    if (!okay_pemit(good[i], executor, 0, 0, pe_info)) {
      notify_format(executor,
                    T("You paged %s, but they are unable to page you."),
                    Name(good[i]));
    }
  }

  mush_free(tbuf, "page_buff");
  mush_free(tbuf2, "page_buff");
  mush_free(namebuf, "page_buff");
  if (nsbuf)
    mush_free(nsbuf, "page_buff");
  if (hp)
    free(hp);
}


/** Does a message match a filter pattern on an object?
 * \param thing object with the filter.
 * \param speaker object responsible for msg.
 * \param msg message to match.
 * \param flag if 0, filter; if 1, infilter.
 * \retval 1 message matches filter.
 * \retval 0 message does not match filter.
 */
int
filter_found(dbref thing, dbref speaker, const char *msg, int flag)
{
  char *filter;
  ATTR *a;
  char *p, *bp;
  char *temp;
  int i;
  int matched = 0;

  NEW_PE_INFO *pe_info = make_pe_info("pe_info-filter_found");
  pe_regs_setenv(pe_info->regvals, 0, msg);

  if (!flag) {
    if (!eval_lock_with(speaker, thing, Filter_Lock, pe_info)) {
      free_pe_info(pe_info);
      return 1;                 /* thing's @lock/filter not passed */
    }
    a = atr_get(thing, "FILTER");
  } else {
    if (!eval_lock_with(speaker, thing, InFilter_Lock, pe_info)) {
      free_pe_info(pe_info);
      return 1;                 /* thing's @lock/infilter not passed */
    }
    a = atr_get(thing, "INFILTER");
  }
  free_pe_info(pe_info);

  if (!a)
    return matched;

  temp = filter = safe_atr_value(a);

  for (i = 0; (i < MAX_ARG) && !matched; i++) {
    p = bp = filter;
    if (process_expression(p, &bp, (char const **) &filter, 0, 0, 0,
                           PE_NOTHING, PT_COMMA, NULL))
      break;
    if (*filter == ',')
      *filter++ = '\0';
    if (*p == '\0' && *filter == '\0')  /* No more filters */
      break;
    if (*p == '\0')             /* Empty filter */
      continue;
    if (AF_Regexp(a))
      matched = quick_regexp_match(p, msg, AF_Case(a));
    else
      matched = local_wild_match_case(p, msg, AF_Case(a), NULL);
  }

  free(temp);
  return matched;
}

/** The emit command.
 * \verbatim
 * This implements @emit.
 * \endverbatim
 * \param player the enactor.
 * \param message the message to emit.
 * \param flags bitmask of notification flags.
 * \param pe_info pe_info for lock checks, speechmod, etc
 */
void
do_emit(dbref player, const char *message, int flags, NEW_PE_INFO *pe_info)
{
  dbref loc;
  int na_flags = NA_INTER_HEAR | NA_PROPAGATE;
  char msgmod[BUFFER_LEN];
  PE_REGS *pe_regs;

  loc = speech_loc(player);
  if (!GoodObject(loc))
    return;

  if (!Loud(player) && !eval_lock_with(player, loc, Speech_Lock, pe_info)) {
    fail_lock(player, loc, Speech_Lock, T("You may not speak here!"), NOTHING);
    return;
  }

  pe_regs = pe_regs_create(PE_REGS_ARG, "do_emit");
  pe_regs_setenv_nocopy(pe_regs, 0, message);
  pe_regs_setenv_nocopy(pe_regs, 1, "|");
  msgmod[0] = '\0';

  if (call_attrib(player, "SPEECHMOD", msgmod, player, pe_info, pe_regs)
      && *msgmod != '\0')
    message = msgmod;
  pe_regs_free(pe_regs);

  /* notify everybody */
  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  notify_anything(player, na_loc, &loc, NULL, na_flags, message, NULL, loc,
                  NULL);
}

/** Remit a message to a single room.
 * \param player the enactor.
 * \param target string containing dbref of room to remit in.
 * \param msg message to emit.
 * \param flags PEMIT_* flags
 * \param pe_info pe_info for locks/permission checks
 */
static void
do_one_remit(dbref player, const char *target, const char *msg, int flags,
             struct format_msg *format, NEW_PE_INFO *pe_info)
{
  dbref room;
  int na_flags = NA_INTER_HEAR | NA_PROPAGATE;
  room = match_result(player, target, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(room)) {
    notify(player, T("I can't find that."));
  } else {
    if (IsExit(room)) {
      notify(player, T("There can't be anything in that!"));
    } else if (!okay_pemit(player, room, 1, 1, pe_info)) {
      /* Do nothing, but do it well */
    } else if (!Loud(player)
               && !eval_lock_with(player, room, Speech_Lock, pe_info)) {
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
      notify_anything(orator, na_loc, &room, NULL, na_flags, msg, NULL, room,
                      format);
    }
  }
}

/** Remit a message
 * \verbatim
 * This implements @remit.
 * \endverbatim
 * \param player the enactor.
 * \param rooms string containing dbref(s) of rooms to remit it.
 * \param message message to emit.
 * \param flags for remit.
 * \param format a format_msg structure to pass to notify_anything() from \@message
 * \param pe_info pe_info for locks/permission checks
 */
void
do_remit(dbref player, char *rooms, const char *message, int flags,
         struct format_msg *format, NEW_PE_INFO *pe_info)
{
  if (flags & PEMIT_LIST) {
    /* @remit/list */
    char *current;
    rooms = trim_space_sep(rooms, ' ');
    while ((current = split_token(&rooms, ' ')) != NULL)
      do_one_remit(player, current, message, flags, format, pe_info);
  } else {
    do_one_remit(player, rooms, message, flags, format, pe_info);
  }
}

/** Emit a message to the absolute location of enactor.
 * \param player the enactor.
 * \param message message to emit.
 * \param flags bitmask of notification flags.
 * \param pe_info pe_info for locks/permission checks
 */
void
do_lemit(dbref player, const char *message, int flags, NEW_PE_INFO *pe_info)
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
  } else if (!Loud(player)
             && !eval_lock_with(player, room, Speech_Lock, pe_info)) {
    fail_lock(player, room, Speech_Lock, T("You may not speak there!"),
              NOTHING);
    return;
  } else {
    if (!silent && (Location(player) != room))
      notify_format(player, T("You lemit: \"%s\""), message);
    if (flags & PEMIT_SPOOF)
      na_flags |= NA_SPOOF;
    notify_anything(player, na_loc, &room, NULL, na_flags, message, NULL, room,
                    NULL);
  }
}

/** notify_anything() function for zone emits.
 * \param current unused.
 * \param data array of notify data.
 * \return last object in zone, or NOTHING.
 */
dbref
na_zemit(dbref current, void *data)
{
  dbref room;
  dbref *dbrefs = data;
  do {
    if (current == NOTHING) {
      for (room = dbrefs[0]; room < db_top; room++) {
        if (IsRoom(room) && (Zone(room) == dbrefs[1])
            && (Loud(dbrefs[2]) || eval_lock(dbrefs[2], room, Speech_Lock))
          )
          break;
      }
      if (!(room < db_top))
        return NOTHING;
      current = room;
      dbrefs[0] = room + 1;
    } else if (IsRoom(current)) {
      current = Contents(current);
    } else {
      current = Next(current);
    }
  } while (current == NOTHING);
  if (dbrefs[3] == current)
    dbrefs[3] = NOTHING;
  return current;
}

/** The zemit command.
 * \verbatim
 * This implements @zemit and @nszemit.
 * \endverbatim
 * \param player the executor.
 * \param target string containing dbref of ZMO.
 * \param message message to emit.
 * \param flags bitmask of notification flags.
 */
void
do_zemit(dbref player, const char *target, const char *message, int flags)
{
  const char *where;
  dbref zone;
  dbref pass[4];
  int na_flags = NA_INTER_HEAR;

  zone = match_result(player, target, NOTYPE, MAT_ABSOLUTE);
  if (!GoodObject(zone)) {
    notify(player, T("Invalid zone."));
    return;
  }
  if (!controls(player, zone)) {
    notify(player, T("Permission denied."));
    return;
  }

  pass[0] = 0;
  pass[1] = zone;
  pass[2] = player;
  pass[3] = speech_loc(player);
  if (flags & PEMIT_SPOOF)
    na_flags |= NA_SPOOF;
  notify_anything(player, na_zemit, &pass, NULL, na_flags, message, NULL,
                  NOTHING, NULL);


  if (!(flags & PEMIT_SILENT) && pass[3] != NOTHING) {
    where = unparse_object(player, zone);
    notify_format(player, T("You zemit, \"%s\" in zone %s"), message, where);
  }

}
