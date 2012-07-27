
/**
 * \file extmail.c
 *
 * \brief The PennMUSH built-in mail system.
 *
 * \verbatim
 *---------------------------------------------------------------
 * extmail.c - Javelin's improved @mail system
 * Based on Amberyl's linked list mailer
 *
 * Summary of mail command syntax:
 * Sending:
 *   @mail[/sendswitch] player-list = message
 *     sendswitches: /silent, /urgent
 *     player-list is a space-separated list of players, aliases, or msg#'s
 *     to reply to. Players can be names or dbrefs. Aliases start with *
 * Reading/Handling:
 *   @mail[/readswitch] [msg-list [= target]]
 *     With no readswitch, @mail reads msg-list (same as /read)
 *     With no readswitch and no msg-list, @mail lists all messages (/list)
 *     readswitches: /list, /read, /fwd (requires target list of players
 *      to forward messages to), /file (requires target folder to file
 *      to), /tag, /untag, /clear, /unclear, /purge (no msg-list),
 *      /count
 *     Assumes messages in current folder, set by @folder or @mail/folder
 *     msg-list can be one of: a message number, a message range,
 *     (2-3, 4-, -6), sender references (*player), date comparisons
 *     (~0, >2, <5), or the strings "urgent", "tagged", "cleared",
 *     "read", "unread", "all", or "folder"
 *     You can also use 1:2 (folder 1, message 2) and 1:2-3 for ranges.
 * Admin stuff:
 *   @mail[/switch] [player]
 *     Switches include: nuke (used to be "purge"), [efd]stats, debug
 *
 * THEORY OF OPERATION:
 *  Prior to pl11, mail was an unsorted linked list. When mail was sent,
 * it was added onto the end. To read mail, you scanned the whole list.
 * This is still how origmail.c works.
 *  As of pl11, extmail.c maintains mail as a sorted linked list, sorted
 * by recipient and order of receipt. This makes sending mail less
 * efficient (because you have to scan the list to figure out where to
 * insert), but reading/checking/deleting more efficient,
 * because once you've found where the player's mail starts, you just
 * read from there.
 *  That wouldn't be so exciting unless there was a fast way to find
 * where a player's mail chain started. Fortunately, there is. We
 * record that information for connected players when they connect,
 * on their descriptor. So, when connected players do reading/etc,
 * it's O(1). Sending to a connected player is O(1). Sending to an
 * unconnected player still requires scanning (O(n)), but you send once,
 * and read/list/delete etc, multiple times.
 *  And just to make the sending to disconnected players faster,
 * instead of scanning the whole maildb to find the insertion point,
 * we start the scan from the chain of the connected player with the
 * closest db# to the target player. This scales up very well.
 *--------------------------------------------------------------------
 * \endverbatim
 */

#include "config.h"
#include "copyrite.h"

#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <ctype.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "match.h"
#include "extmail.h"
#include "function.h"
#include "malias.h"
#include "attrib.h"
#include "parse.h"
#include "mymalloc.h"
#include "ansi.h"
#include "pueblo.h"
#include "flags.h"
#include "log.h"
#include "lock.h"
#include "command.h"
#include "dbio.h"
#include "confmagic.h"


extern int do_convtime(const char *str, struct tm *ttm);        /* funtime.c */

static void do_mail_flags
  (dbref player, const char *msglist, mail_flag flag, bool negate);
static char *mail_list_time(const char *the_time, bool flag);
static MAIL *mail_fetch(dbref player, int num);
static MAIL *real_mail_fetch(dbref player, int num, int folder);
static MAIL *mailfun_fetch(dbref player, int nargs, char *arg1, char *arg2);
static void count_mail(dbref player,
                       int folder, int *rcount, int *ucount, int *ccount);
static int real_send_mail(dbref player,
                          dbref target, char *subject, char *message,
                          mail_flag flags, int silent, int nosig);
static void send_mail(dbref player,
                      dbref target, char *subject, char *message,
                      mail_flag flags, int silent, int nosig);
static int send_mail_alias(dbref player,
                           char *aname, char *subject,
                           char *message, mail_flag flags, int silent,
                           int nosig);
static void filter_mail(dbref from, dbref player, char *subject,
                        char *message, int mailnumber, mail_flag flags);
static MAIL *find_insertion_point(dbref player);
static int get_folder_number(dbref player, char *name);
static char *get_folder_name(dbref player, int fld);
static int player_folder(dbref player);
static int parse_folder(dbref player, char *folder_string);
static int mail_match(dbref player, MAIL *mp, struct mail_selector ms, int num);
static int parse_msglist
  (const char *msglist, struct mail_selector *ms, dbref player);
static int parse_message_spec
  (dbref player, const char *s, int *msglow, int *msghigh, int *folder);
static char *status_chars(MAIL *mp);
static char *status_string(MAIL *mp);
static int sign(int x);
static char *get_message(MAIL *mp);
static unsigned char *get_compressed_message(MAIL *mp);
static char *get_subject(MAIL *mp);
static char *get_sender(MAIL *mp, int full);
static int was_sender(dbref player, MAIL *mp);

MAIL *maildb;            /**< The head of the mail list */
MAIL *tail_ptr;          /**< The end of the mail list */

slab *mail_slab; /**< slab for 'struct mail' allocations */

#define HEAD  maildb     /**< The head of the mail list */
#define TAIL  tail_ptr   /**< The end of the mail list */

/** A line of...dashes! */
#define DASH_LINE  \
  "-----------------------------------------------------------------------------"

int mdb_top = 0;                /**< total number of messages in mail db */

/*-------------------------------------------------------------------------*
 *   User mail functions (these are called from game.c)
 *
 * do_mail - cases without a /switch.
 * do_mail_send - sending mail
 * do_mail_read - read messages
 * do_mail_list - list messages
 * do_mail_review - read messages sent to others
 * do_mail_retract - delete unread messages sent to others
 * do_mail_flags - tagging, untagging, clearing, unclearing of messages
 * do_mail_file - files messages into a new folder
 * do_mail_fwd - forward messages to another player(s)
 * do_mail_count - count messages
 * do_mail_purge - purge cleared messages
 * do_mail_change_folder - change current folder
 * do_mail_unfolder - remove a folder name from MAILFOLDERS
 * do_mail_subject - set the current mail subject
 *-------------------------------------------------------------------------*/

/* Return the uncompressed text of a @mail in a static buffer */
static char *
get_message(MAIL *mp)
{
  static char text[BUFFER_LEN * 2];
  unsigned char tbuf[BUFFER_LEN * 2];

  if (!mp)
    return NULL;

  chunk_fetch(mp->msgid, tbuf, sizeof tbuf);
  strcpy(text, uncompress(tbuf));
  return text;
}

/* Return the compressed text of a @mail in a static buffer */
static unsigned char *
get_compressed_message(MAIL *mp)
{
  static unsigned char text[BUFFER_LEN * 2];

  if (!mp)
    return NULL;

  chunk_fetch(mp->msgid, (unsigned char *) text, sizeof text);
  return text;
}

/* Return the subject of a mail message, or (no subject) */
static char *
get_subject(MAIL *mp)
{
  static char sbuf[SUBJECT_LEN + 1];
  char *p;
  if (mp->subject) {
    strncpy(sbuf, uncompress(mp->subject), SUBJECT_LEN);
    sbuf[SUBJECT_LEN] = '\0';
    /* Stop at a return or a tab */
    for (p = sbuf; *p; p++) {
      if ((*p == '\r') || (*p == '\n') || (*p == '\t')) {
        *p = '\0';
        break;
      }
      if (!isprint((unsigned char) *p)) {
        *p = ' ';
      }
    }
  } else
    strcpy(sbuf, T("(no subject)"));
  return sbuf;
}

/* Return the name of the mail sender. */
static char *
get_sender(MAIL *mp, int full)
{
  static char tbuf1[BUFFER_LEN], *bp;
  bp = tbuf1;
  if (!GoodObject(mp->from))
    safe_str(T("!Purged!"), tbuf1, &bp);
  else if (!was_sender(mp->from, mp))
    safe_str(T("!Purged!"), tbuf1, &bp);
  else if (IsPlayer(mp->from) || !full)
    safe_str(Name(mp->from), tbuf1, &bp);
  else
    safe_format(tbuf1, &bp, T("%s (#%d, owner: %s)"), Name(mp->from),
                mp->from, Name(Owner(mp->from)));
  *bp = '\0';
  return tbuf1;
}

/* Was this player the sender of this message? */
static int
was_sender(dbref player, MAIL *mp)
{
  /* If the dbrefs don't match, fail. */
  if (mp->from != player)
    return 0;
  /* If we don't know the creation time of the sender, succeed. */
  if (!mp->from_ctime)
    return 1;
  /* Succeed if and only if the creation times match. */
  return (mp->from_ctime == CreTime(player));
}

/** Check if player can use the \@mail command without sending an error
 *  message. This is purely for use when forced by bsd.c so it won't send
 *  "Guests can't do that" to guests if \@mail is noguest.
 */
int
can_mail(dbref player)
{
  return command_check_byname_quiet(player, "@MAIL", NULL);
}

/** Change folders or rename a folder.
 * \verbatim
 * This implements @mail/folder
 * \endverbatim
 * \param player the enactor.
 * \param fld string containing folder number or name.
 * \param newname string containing folder name, if renaming.
 */
void
do_mail_change_folder(dbref player, char *fld, char *newname)
{
  int pfld;
  char *p;

  if (!fld || !*fld) {
    /* Check mail in all folders */
    for (pfld = MAX_FOLDERS; pfld >= 0; pfld--)
      check_mail(player, pfld, 1);
    pfld = player_folder(player);
    notify_format(player,
                  T("MAIL: Current folder is %d [%s]."), pfld,
                  get_folder_name(player, pfld));
    return;
  }
  pfld = parse_folder(player, fld);
  if (pfld < 0) {
    notify(player, T("MAIL: What folder is that?"));
    return;
  }
  if (newname && *newname) {
    /* We're changing a folder name here */
    if (strlen(newname) > FOLDER_NAME_LEN) {
      notify(player, T("MAIL: Folder name too long"));
      return;
    }
    for (p = newname; p && *p; p++) {
      if (!isalnum((unsigned char) *p)) {
        notify(player, T("MAIL: Illegal folder name"));
        return;
      }
    }
    add_folder_name(player, pfld, newname);
    notify_format(player, T("MAIL: Folder %d now named '%s'"), pfld, newname);
  } else {
    /* Set a new folder */
    set_player_folder(player, pfld);
    notify_format(player,
                  T("MAIL: Current folder set to %d [%s]."), pfld,
                  get_folder_name(player, pfld));
  }
}

/** Remove a folder name.
 * \verbatim
 * This implements @mail/unfolder
 * \endverbatim
 * \param player the enactor.
 * \param fld string containing folder number or name.
 */
void
do_mail_unfolder(dbref player, char *fld)
{
  int pfld;

  if (!fld || !*fld) {
    notify(player, T("MAIL: You must specify a folder name or number"));
    return;
  }
  pfld = parse_folder(player, fld);
  if (pfld < 0) {
    notify(player, T("MAIL: What folder is that?"));
    return;
  }
  add_folder_name(player, pfld, NULL);
  notify_format(player, T("MAIL: Folder %d now has no name"), pfld);
}


/** Tag a set of mail messages.
 * \param player the enactor.
 * \param msglist string specifying messages to tag.
 */
void
do_mail_tag(dbref player, const char *msglist)
{
  do_mail_flags(player, msglist, M_TAG, 0);
}

/** Clear a set of mail messages.
 * \param player the enactor.
 * \param msglist string specifying messages to clear.
 */
void
do_mail_clear(dbref player, const char *msglist)
{
  do_mail_flags(player, msglist, M_CLEARED, 0);
}

/** Untag a set of mail messages.
 * \param player the enactor.
 * \param msglist string specifying messages to untag.
 */
void
do_mail_untag(dbref player, const char *msglist)
{
  do_mail_flags(player, msglist, M_TAG, 1);
}

/** Unclear a set of mail messages.
 * \param player the enactor.
 * \param msglist string specifying messages to unclear.
 */
void
do_mail_unclear(dbref player, const char *msglist)
{
  do_mail_flags(player, msglist, M_CLEARED, 1);
}

/** Unread a set of mail messages.
 * \param player the enactor
 * \param msglist string specifying messages to unclear.
 */
void
do_mail_unread(dbref player, const char *msglist)
{
  do_mail_flags(player, msglist, M_MSGREAD, 1);
}

/** Change the status for a set of mail messages.
 * \param player the enactor
 * \param msglist string specifying messages to unclear.
 * \param status the status to set for the messages
 */
void
do_mail_status(dbref player, const char *msglist, const char *status)
{
  int flag;
  bool negate = 0;

  if (!status || !*status) {
    notify(player, T("MAIL: What do you want to do with the messages?"));
    return;
  }

  if (string_prefix("read", status) || string_prefix("unread", status))
    flag = M_MSGREAD;
  else if (string_prefix("cleared", status)
           || string_prefix("uncleared", status))
    flag = M_CLEARED;
  else if (string_prefix("tagged", status) || string_prefix("untagged", status))
    flag = M_TAG;
  else {
    notify(player, T("MAIL: Unknown status."));
    return;
  }

  if (*status == 'u' || *status == 'U')
    negate = 1;

  do_mail_flags(player, msglist, flag, negate);
}


/** Set or clear a flag on a set of messages.
 * \param player the enactor.
 * \param msglist string representing list of messages to operate on.
 * \param flag flag to set or clear.
 * \param negate if 1, clear the flag; if 0, set the flag.
 */
static void
do_mail_flags(dbref player, const char *msglist, mail_flag flag, bool negate)
{
  MAIL *mp;
  struct mail_selector ms;
  mail_flag folder;
  folder_array i;
  int notified = 0, j = 0;

  if (!parse_msglist(msglist, &ms, player)) {
    return;
  }
  FA_Init(i);
  folder = AllInFolder(ms) ? player_folder(player) : MSFolder(ms);
  for (mp = find_exact_starting_point(player);
       mp && (mp->to == player); mp = mp->next) {
    if ((mp->to == player) && (All(ms) || (Folder(mp) == folder))) {
      i[Folder(mp)]++;
      if (mail_match(player, mp, ms, i[Folder(mp)])) {
        j++;
        if (negate) {
          mp->read &= ~flag;
        } else {
          mp->read |= flag;
        }
        switch (flag) {
        case M_TAG:
          if (All(ms)) {
            if (!notified) {
              if (negate)
                notify(player,
                       T("MAIL: All messages in all folders untagged."));
              else
                notify(player, T("MAIL: All messages in all folders tagged."));
              notified++;
            }
          } else {
            if (negate) {
              notify_format(player, T("MAIL: Msg #%d:%d untagged"),
                            (int) Folder(mp), i[Folder(mp)]);
            } else {
              notify_format(player, T("MAIL: Msg #%d:%d tagged"),
                            (int) Folder(mp), i[Folder(mp)]);
            }
          }
          break;
        case M_CLEARED:
          if (All(ms)) {
            if (!notified) {
              if (negate) {
                notify(player,
                       T("MAIL: All messages in all folders uncleared."));
              } else {
                notify(player, T("MAIL: All messages in all folders cleared."));
              }
              notified++;
            }
          } else {
            if (Unread(mp) && !negate) {
              notify_format(player,
                            T
                            ("MAIL: Unread Msg #%d:%d cleared! Use @mail/unclear %d:%d to recover."),
                            (int) Folder(mp), i[Folder(mp)], (int) Folder(mp),
                            i[Folder(mp)]);
            } else {
              notify_format(player,
                            (negate ? T("MAIL: Msg #%d:%d uncleared.") :
                             T("MAIL: Msg #%d:%d cleared.")), (int) Folder(mp),
                            i[Folder(mp)]);
            }
          }
          break;
        case M_MSGREAD:
          if (All(ms)) {
            if (!notified) {
              if (negate) {
                notify(player, T("MAIL: All messages in all folders unread."));
              } else {
                notify(player,
                       T("MAIL: All messages in all folders marked as read."));
              }
              notified++;
            }
          } else {
            if (negate) {
              notify_format(player, T("MAIL: Msg #%d:%d unread"),
                            (int) Folder(mp), i[Folder(mp)]);
            } else {
              notify_format(player, T("MAIL: Msg #%d:%d marked as read"),
                            (int) Folder(mp), i[Folder(mp)]);
            }
          }
          break;
        }
      }
    }
  }
  if (!j) {
    /* ran off the end of the list without finding anything */
    notify(player, T("MAIL: You don't have any matching messages!"));
  }
  return;
}

/** File messages into a folder.
 * \verbatim
 * This implements @mail/file.
 * \endverbatim
 * \param player the enactor.
 * \param msglist list of messages to file.
 * \param folder name or number of folder to put messages in.
 */
void
do_mail_file(dbref player, char *msglist, char *folder)
{
  MAIL *mp;
  struct mail_selector ms;
  int foldernum;
  mail_flag origfold;
  folder_array i;
  int notified = 0, j = 0;

  if (!parse_msglist(msglist, &ms, player)) {
    return;
  }
  if ((foldernum = parse_folder(player, folder)) == -1) {
    notify(player, T("MAIL: Invalid folder specification"));
    return;
  }
  FA_Init(i);
  origfold = AllInFolder(ms) ? player_folder(player) : MSFolder(ms);
  for (mp = find_exact_starting_point(player);
       mp && (mp->to == player); mp = mp->next) {
    if ((mp->to == player) && (All(ms) || (Folder(mp) == origfold))) {
      i[Folder(mp)]++;
      if (mail_match(player, mp, ms, i[Folder(mp)])) {
        j++;
        mp->read &= M_FMASK;    /* Clear the folder */
        mp->read &= ~M_CLEARED; /* Unclear it if it was marked cleared */
        mp->read |= FolderBit(foldernum);
        if (All(ms)) {
          if (!notified) {
            notify_format(player,
                          T("MAIL: All messages filed in folder %d [%s]"),
                          foldernum, get_folder_name(player, foldernum));
            notified++;
          }
        } else
          notify_format(player,
                        T("MAIL: Msg %d:%d filed in folder %d [%s]"),
                        (int) origfold, i[origfold], foldernum,
                        get_folder_name(player, foldernum));
      }
    }
  }
  if (!j) {
    /* ran off the end of the list without finding anything */
    notify(player, T("MAIL: You don't have any matching messages!"));
  }
  return;
}

/** Read mail messages.
 * This displays the contents of a set of mail messages.
 * \param player the enactor.
 * \param msglist list of messages to read.
 */
void
do_mail_read(dbref player, char *msglist)
{
  MAIL *mp;
  char tbuf1[BUFFER_LEN];
  char folderheader[BUFFER_LEN];
  struct mail_selector ms;
  mail_flag folder;
  folder_array i;
  int j = 0;

  if (!parse_msglist(msglist, &ms, player)) {
    return;
  }
  folder = AllInFolder(ms) ? player_folder(player) : MSFolder(ms);
  FA_Init(i);
  for (mp = find_exact_starting_point(player);
       mp && (mp->to == player); mp = mp->next) {
    if ((mp->to == player) && (All(ms) || Folder(mp) == folder)) {
      i[Folder(mp)]++;
      if (mail_match(player, mp, ms, i[Folder(mp)])) {
        /* Read it */
        j++;
        if (SUPPORT_PUEBLO) {
          notify_noenter(player, open_tag("SAMP"));
          snprintf(folderheader, BUFFER_LEN,
                   "%c%cA XCH_HINT=\"List messages in this folder\" XCH_CMD=\"@mail/list %d:1-\"%c%s%c%c/A%c",
                   TAG_START, MARKUP_HTML, (int) Folder(mp), TAG_END,
                   T("Folder:"), TAG_START, MARKUP_HTML, TAG_END);
        } else
          mush_strncpy(folderheader, T("Folder:"), BUFFER_LEN);
        notify(player, DASH_LINE);
        mush_strncpy(tbuf1, get_sender(mp, 1), BUFFER_LEN);
        notify_format(player,
                      T
                      ("From: %-55s %s\nDate: %-25s   %s %2d   Message: %d\nStatus: %s"),
                      tbuf1, ((*tbuf1 != '!') && IsPlayer(mp->from)
                              && Connected(mp->from)
                              && (!hidden(mp->from)
                                  || Priv_Who(player))) ? T(" (Conn)") :
                      "      ", show_time(mp->time, 0), folderheader,
                      (int) Folder(mp), i[Folder(mp)], status_string(mp));
        notify_format(player, T("Subject: %s"), get_subject(mp));
        notify(player, DASH_LINE);
        if (SUPPORT_PUEBLO)
          notify_noenter(player, close_tag("SAMP"));
        strcpy(tbuf1, get_message(mp));
        notify(player, tbuf1);
        if (SUPPORT_PUEBLO)
          notify(player, wrap_tag("SAMP", DASH_LINE));
        else
          notify(player, DASH_LINE);
        if (Unread(mp))
          mp->read |= M_MSGREAD;        /* mark message as read */
      }
    }
  }
  if (!j) {
    /* ran off the end of the list without finding anything */
    notify(player, T("MAIL: You don't have that many matching messages!"));
  }
  return;
}

/** List the flags, number, sender, subject, and date of messages in a
 * concise format.
 * \param player the enactor.
 * \param msglist list of messages to list.
 */
void
do_mail_list(dbref player, const char *msglist)
{
  char subj[30];
  char sender[30];
  MAIL *mp;
  struct mail_selector ms;
  mail_flag folder;
  folder_array i;

  if (!parse_msglist(msglist, &ms, player)) {
    return;
  }
  FA_Init(i);
  folder = AllInFolder(ms) ? player_folder(player) : MSFolder(ms);
  if (SUPPORT_PUEBLO)
    notify_noenter(player, open_tag("SAMP"));
  notify_format(player,
                T
                ("---------------------------  MAIL (folder %2d)  ------------------------------"),
                (int) folder);
  for (mp = find_exact_starting_point(player); mp && (mp->to == player);
       mp = mp->next) {
    if ((mp->to == player) && (All(ms) || Folder(mp) == folder)) {
      i[Folder(mp)]++;
      if (mail_match(player, mp, ms, i[Folder(mp)])) {
        /* list it */
        if (SUPPORT_PUEBLO)
          notify_noenter(player,
                         tprintf
                         ("%c%cA XCH_CMD=\"@mail/read %d:%d\" XCH_HINT=\"Read message %d in folder %d\"%c",
                          TAG_START, MARKUP_HTML, (int) Folder(mp),
                          i[Folder(mp)], i[Folder(mp)], (int) Folder(mp),
                          TAG_END));
        strcpy(subj, chopstr(get_subject(mp), 28));
        strcpy(sender, chopstr(get_sender(mp, 0), 12));
        notify_format(player, "[%s] %2d:%-3d %c%-12s  %-*s %s",
                      status_chars(mp), (int) Folder(mp), i[Folder(mp)],
                      ((*sender != '!') && (Connected(mp->from) &&
                                            (!hidden(mp->from)
                                             || Priv_Who(player)))
                       ? '*' : ' '), sender, 30, subj,
                      mail_list_time(show_time(mp->time, 0), 1));
        if (SUPPORT_PUEBLO)
          notify_noenter(player, tprintf("%c%c/A%c", TAG_START,
                                         MARKUP_HTML, TAG_END));
      }
    }
  }
  notify(player, DASH_LINE);
  if (SUPPORT_PUEBLO)
    notify(player, close_tag("SAMP"));
  return;
}

FUNCTION(fun_maillist)
{
  MAIL *mp;
  struct mail_selector ms;
  mail_flag folder;
  folder_array i;
  dbref player;
  int matches = 0;

  if (nargs == 2) {
    player = match_result(executor, args[0], TYPE_PLAYER,
                          MAT_ME | MAT_ABSOLUTE | MAT_PMATCH | MAT_TYPE);
    if (!GoodObject(player)) {
      safe_str(T(e_match), buff, bp);
      return;
    } else if (!controls(executor, player)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
  } else {
    player = executor;
  }

  if (!parse_msglist((nargs ? args[nargs - 1] : ""), &ms, player)) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  FA_Init(i);
  folder = AllInFolder(ms) ? player_folder(player) : MSFolder(ms);
  for (mp = find_exact_starting_point(player); mp && (mp->to == player);
       mp = mp->next) {
    if ((mp->to == player) && (All(ms) || Folder(mp) == folder)) {
      i[Folder(mp)]++;
      if (mail_match(player, mp, ms, i[Folder(mp)])) {
        if (matches)
          safe_chr(' ', buff, bp);
        safe_integer((int) Folder(mp), buff, bp);
        safe_chr(':', buff, bp);
        safe_integer(i[Folder(mp)], buff, bp);
        matches++;
      }
    }
  }
}

/** Review mail messages.  
 * This displays the contents of a set of mail messages sent by one player to another
 * \param player the enactor.
 * \param target the player to review.
 * \param msglist list of messages to read.
 */
void
do_mail_reviewread(dbref player, dbref target, const char *msglist)
{
  MAIL *mp;
  char tbuf1[BUFFER_LEN];
  struct mail_selector ma, ms;
  int i, j;

  /* Initialize listing mail selector for all messages from player */
  ma.low = 0;
  ma.high = 0;
  ma.flags = 0x00FF | M_ALL;
  ma.days = -1;
  ma.day_comp = 0;
  ma.player = player;
  /* Initialize another mail selector with msglist */
  if (!parse_msglist(msglist, &ms, player)) {
    return;
  }
  /* Switch mail selector to player, all folders */
  ms.player = player;
  ms.flags = M_ALL;
  /* Initialize i (message index), j (messages read) */
  i = j = 0;
  for (mp = find_exact_starting_point(target);
       mp && (mp->to == target); mp = mp->next) {
    if (mail_match(player, mp, ma, 0)) {
      /* This was a listed message */
      i++;
      if (mail_match(player, mp, ms, i)) {
        /* Read it */
        j++;
        notify(player, DASH_LINE);
        mush_strncpy(tbuf1, get_sender(mp, 1), BUFFER_LEN);
        notify_format(player,
                      T
                      ("From: %-55s %s\nDate: %-25s   Folder: NA   Message: %d\nStatus: %s"),
                      tbuf1, ((*tbuf1 != '!') && IsPlayer(mp->from)
                              && Connected(mp->from)
                              && (!hidden(mp->from)
                                  || Priv_Who(player))) ? T(" (Conn)") :
                      "      ", show_time(mp->time, 0), i, status_string(mp));
        notify_format(player, T("Subject: %s"), get_subject(mp));
        notify(player, DASH_LINE);
        if (SUPPORT_PUEBLO)
          notify_noenter(player, close_tag("SAMP"));
        strcpy(tbuf1, get_message(mp));
        notify(player, tbuf1);
        if (SUPPORT_PUEBLO)
          notify(player, wrap_tag("SAMP", DASH_LINE));
        else
          notify(player, DASH_LINE);
      }
    }
  }
  if (!j) {
    /* ran off the end of the list without finding anything */
    notify(player, T("MAIL: No matching messages."));
  }
  return;
}

/** List the flags, number, sender, subject, and date of another
 * player's messages in a concise format. (Hacked from do_mail_list.)
 * \param player the enactor.
 * \param target the player to review.
 * \param msglist list of messages to list.
 */
void
do_mail_reviewlist(dbref player, dbref target)
{
  char subj[30];
  char sender[30];
  MAIL *mp;
  struct mail_selector ms;
  int i;

  /* Initialize mail selector */
  ms.low = 0;
  ms.high = 0;
  ms.flags = 0x00FF | M_ALL;
  ms.days = -1;
  ms.day_comp = 0;
  ms.player = player;
  /* Initialize i (messages found) */
  i = 0;
  if (SUPPORT_PUEBLO)
    notify_noenter(player, open_tag("SAMP"));
  notify_format(player,
                T
                ("--------------------   MAIL: %-27s   ------------------"),
                Name(target));
  for (mp = find_exact_starting_point(target); mp && (mp->to == target);
       mp = mp->next) {
    if (mail_match(player, mp, ms, i)) {
      /* list it */
      i++;
      if (SUPPORT_PUEBLO)
        notify_noenter(player,
                       tprintf
                       ("%c%cA XCH_CMD=\"@mail/review %s=%d\" XCH_HINT=\"Read message %d sent to %s\"%c",
                        TAG_START, MARKUP_HTML, Name(target),
                        i, i, Name(target), TAG_END));
      strcpy(subj, chopstr(get_subject(mp), 28));
      strcpy(sender, chopstr(get_sender(mp, 0), 12));
      notify_format(player, "[%s]    %-3d %c%-12s  %-*s %s",
                    status_chars(mp), i,
                    ((*sender != '!') && (Connected(mp->from) &&
                                          (!hidden(mp->from)
                                           || Priv_Who(player)))
                     ? '*' : ' '), sender, 30, subj,
                    mail_list_time(show_time(mp->time, 0), 1));
      if (SUPPORT_PUEBLO)
        notify_noenter(player, tprintf("%c%c/A%c", TAG_START,
                                       MARKUP_HTML, TAG_END));
    }
  }
  notify(player, DASH_LINE);
  if (SUPPORT_PUEBLO)
    notify(player, close_tag("SAMP"));
  return;
}

/** Review mail.
 * \verbatim
 * This implements @mail/review.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player whose mail to review.
 * \param msglist list of messages.
 */
void
do_mail_review(dbref player, const char *name, const char *msglist)
{
  dbref target;

  if (!name || !*name) {
    notify(player,
           T("MAIL: I can't figure out whose mail you want to review."));
    return;
  }
  if ((target = lookup_player(name)) == NOTHING) {
    notify(player, T("MAIL: I couldn't find that player."));
    return;
  }
  if (!msglist || !*msglist) {
    do_mail_reviewlist(player, target);
  } else {
    do_mail_reviewread(player, target, msglist);
  }
  return;
}

/** Retract specified mail.
 * \verbatim
 * This implements @mail/retract.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player whose mail to retract.
 * \param msglist list of messages.
 */
void
do_mail_retract(dbref player, const char *name, const char *msglist)
{
  MAIL *mp, *nextp;
  struct mail_selector ma, ms;
  int i, j;
  dbref target;

  if (!name || !*name) {
    notify(player,
           T("MAIL: I can't figure out whose mail you want to retract."));
    return;
  }
  if ((target = lookup_player(name)) == NOTHING) {
    notify(player, T("MAIL: I couldn't find that player."));
    return;
  }

  /* Initialize mail selector for all messages */
  ma.low = 0;
  ma.high = 0;
  ma.flags = 0x00FF | M_ALL;
  ma.days = -1;
  ma.day_comp = 0;
  ma.player = player;
  /* Initialize another mail selector with msglist */
  if (!parse_msglist(msglist, &ms, player)) {
    return;
  }
  /* Switch mail selector to target, all folders */
  ms.player = player;
  ms.flags = M_ALL;
  /* Initialize i (messages listed), and j (messages retracted) */
  i = j = 0;
  for (mp = find_exact_starting_point(target);
       mp && (mp->to == target); mp = nextp) {
    nextp = mp->next;
    if (mail_match(player, mp, ma, 0)) {
      /* was in message list */
      i++;
      if (mail_match(player, mp, ms, i)) {
        /* matches msglist, retract if unread */
        j++;
        if (Read(mp)) {
          notify_format(player, T("MAIL: Message %d has been read."), i);
        } else {
          /* Delete this one */
          /* head and tail of the list are special */
          if (mp == HEAD)
            HEAD = mp->next;
          else if (mp == TAIL)
            TAIL = mp->prev;
          /* relink the list */
          if (mp->prev != NULL)
            mp->prev->next = mp->next;
          if (mp->next != NULL)
            mp->next->prev = mp->prev;
          /* save the pointer */
          nextp = mp->next;
          /* then wipe */
          notify_format(player, T("MAIL: Message %d has been retracted."), i);
          mdb_top--;
          free(mp->subject);
          chunk_delete(mp->msgid);
          slab_free(mail_slab, mp);
        }
      }
    }
  }
  if (!j) {
    /* ran off the end of the list without finding anything */
    notify(player, T("MAIL: No matching messages."));
  }
  return;
}

static char *
mail_list_time(const char *the_time, bool flag /* 1 for no year */ )
{
  static char newtime[BUFFER_LEN];
  const char *p;
  char *q;
  int i;
  p = the_time;
  q = newtime;
  if (!p || !*p)
    return NULL;
  /* Format of the_time is: day mon dd hh:mm:ss yyyy */
  /* Chop out :ss */
  for (i = 0; i < 16; i++) {
    if (*p)
      *q++ = *p++;
  }
  if (!flag) {
    for (i = 0; i < 3; i++) {
      if (*p)
        p++;
    }
    for (i = 0; i < 5; i++) {
      if (*p)
        *q++ = *p++;
    }
  }
  *q = '\0';
  return newtime;
}


/** Expunge mail that's marked for deletion.
 * \verbatim
 * This implements @mail/purge.
 * \endverbatim
 * \param player the enactor.
 */
void
do_mail_purge(dbref player)
{
  MAIL *mp, *nextp;

  /* Go through player's mail, and remove anything marked cleared */
  for (mp = find_exact_starting_point(player);
       mp && (mp->to == player); mp = nextp) {
    if ((mp->to == player) && Cleared(mp)) {
      /* Delete this one */
      /* head and tail of the list are special */
      if (mp == HEAD)
        HEAD = mp->next;
      else if (mp == TAIL)
        TAIL = mp->prev;
      /* relink the list */
      if (mp->prev != NULL)
        mp->prev->next = mp->next;
      if (mp->next != NULL)
        mp->next->prev = mp->prev;
      /* save the pointer */
      nextp = mp->next;
      /* then wipe */
      mdb_top--;
      free(mp->subject);
      chunk_delete(mp->msgid);
      slab_free(mail_slab, mp);
    } else {
      nextp = mp->next;
    }
  }
  set_objdata(player, "MAIL", NULL);
  if (command_check_byname(player, "@MAIL", NULL))
    notify(player, T("MAIL: Mailbox purged."));
  return;
}

/** Forward mail messages to someone(s) else.
 * \verbatim
 * This implements @mail/forward.
 * \endverbatim
 * \param player the enactor.
 * \param msglist list of messages to forward.
 * \param tolist list of recipients to forwared to.
 */
void
do_mail_fwd(dbref player, char *msglist, char *tolist)
{
  MAIL *mp;
  MAIL *last;
  struct mail_selector ms;
  int num;
  mail_flag folder;
  folder_array i;
  const char *head;
  MAIL *temp;
  dbref target;
  int num_recpts = 0;
  const char **start;
  char *current;
  start = &head;

  if (!parse_msglist(msglist, &ms, player)) {
    return;
  }
  if (!tolist || !*tolist) {
    notify(player, T("MAIL: To whom should I forward?"));
    return;
  }
  folder = AllInFolder(ms) ? player_folder(player) : MSFolder(ms);
  /* Mark the player's last message. This prevents a loop if
   * the forwarding command happens to forward a message back
   * to the player itself
   */
  last = mp = find_exact_starting_point(player);
  if (!last) {
    notify(player, T("MAIL: You have no messages to forward."));
    return;
  }
  while (last->next && (last->next->to == player))
    last = last->next;

  FA_Init(i);
  while (mp && (mp->to == player) && (mp != last->next)) {
    if ((mp->to == player) && (All(ms) || (Folder(mp) == folder))) {
      i[Folder(mp)]++;
      if (mail_match(player, mp, ms, i[Folder(mp)])) {
        /* forward it to all players listed */
        head = tolist;
        while (head && *head) {
          current = next_in_list(start);
          /* Now locate a target */
          num = atoi(current);
          if (num) {
            /* reply to a mail message */
            temp = mail_fetch(player, num);
            if (!temp) {
              notify(player, T("MAIL: You can't reply to nonexistant mail."));
            } else {
              char tbuf1[BUFFER_LEN];
              unsigned char tbuf2[BUFFER_LEN];
              mush_strncpy(tbuf1, uncompress(mp->subject), BUFFER_LEN);
              u_strncpy(tbuf2, get_compressed_message(mp), BUFFER_LEN);
              send_mail(player, temp->from, tbuf1, (char *) tbuf2,
                        M_FORWARD | M_REPLY, 1, 0);
              num_recpts++;
            }
          } else {
            /* forwarding to a player */
            target =
              match_result(player, current, TYPE_PLAYER,
                           MAT_ME | MAT_ABSOLUTE | MAT_PMATCH | MAT_TYPE);
            if (!GoodObject(target) || !IsPlayer(target)) {
              notify_format(player, T("No such unique player: %s."), current);
            } else {
              char tbuf1[BUFFER_LEN];
              unsigned char tbuf2[BUFFER_LEN];
              mush_strncpy(tbuf1, uncompress(mp->subject), BUFFER_LEN);
              u_strncpy(tbuf2, get_compressed_message(mp), BUFFER_LEN);
              send_mail(player, target, tbuf1, (char *) tbuf2, M_FORWARD, 1, 0);
              num_recpts++;
            }
          }
        }
      }
    }
    mp = mp->next;
  }
  notify_format(player, T("MAIL: %d messages forwarded."), num_recpts);
}

/** Send a mail message.
 * \param player the enactor.
 * \param tolist list of recipients.
 * \param message message text.
 * \param flags flags to apply to the message.
 * \param silent if 1, don't notify sender for each message sent.
 * \param nosig if 1, don't apply sender's MAILSIGNATURE.
 */
void
do_mail_send(dbref player, char *tolist, char *message, mail_flag flags,
             int silent, int nosig)
{
  const char *head;
  int num;
  dbref target;
  mail_flag mail_flags;
  char sbuf[SUBJECT_LEN + 1], *sb, *mb;
  int i = 0, subject_given = 0;
  const char **start;
  char *current;
  start = &head;

  if (!tolist || !*tolist) {
    notify(player, T("MAIL: I can't figure out who you want to mail to."));
    return;
  }
  if (!message || !*message) {
    notify(player, T("MAIL: I can't figure out what you want to send."));
    return;
  }
  sb = sbuf;
  mb = message;                 /* Save the message pointer */
  while (*message && (i < SUBJECT_LEN)) {
    if (*message == SUBJECT_COOKIE) {
      if (*(message + 1) == SUBJECT_COOKIE) {
        *sb++ = *message;
        message += 2;
        i += 1;
      } else
        break;
    } else {
      *sb++ = *message++;
      i += 1;
    }
  }
  *sb = '\0';
  if (*message && (*message == SUBJECT_COOKIE)) {
    message++;
    subject_given = 1;
  } else
    message = mb;               /* Rewind the pointer to the beginning */
  /* Parse the player list */
  head = tolist;
  while (head && *head) {
    mail_flags = flags;
    current = next_in_list(start);
    /* Now locate a target */
    if (is_strict_integer(current)) {
      /* reply to a mail message */
      MAIL *temp;

      num = parse_integer(current);

      temp = mail_fetch(player, num);
      if (!temp) {
        notify(player, T("MAIL: You can't reply to nonexistent mail."));
        return;
      }
      if (subject_given)
        send_mail(player, temp->from, sbuf, message, mail_flags, silent, nosig);
      else
        send_mail(player, temp->from, uncompress(temp->subject), message,
                  mail_flags | M_REPLY, silent, nosig);
    } else {
      /* send a new mail message */
      target =
        match_result(player, current, TYPE_PLAYER,
                     MAT_ME | MAT_ABSOLUTE | MAT_PLAYER);
      if (!GoodObject(target))
        target = lookup_player(current);
      if (!GoodObject(target))
        target = short_page(current);
      if (!GoodObject(target) || !IsPlayer(target)) {
        if (!send_mail_alias
            (player, current, sbuf, message, mail_flags, silent, nosig))
          notify_format(player, T("No such unique player: %s."), current);
      } else
        send_mail(player, target, sbuf, message, mail_flags, silent, nosig);
    }
  }
}

/*-------------------------------------------------------------------------*
 *   Admin mail functions
 *
 * do_mail_nuke - clear & purge mail for a player, or all mail in db.
 * do_mail_stat - stats on mail for a player, or for all db.
 * do_mail_debug - fix mail with a sledgehammer
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*
 *   Basic mail functions
 *-------------------------------------------------------------------------*/
static MAIL *
mail_fetch(dbref player, int num)
{
  /* get an arbitrary mail message in the current folder */
  return real_mail_fetch(player, num, player_folder(player));
}

static MAIL *
real_mail_fetch(dbref player, int num, int folder)
{
  MAIL *mp;
  int i = 0;

  for (mp = find_exact_starting_point(player); mp != NULL; mp = mp->next) {
    if (mp->to > player)
      break;
    if ((mp->to == player) && ((folder < 0)
                               || (Folder(mp) == (mail_flag) folder)))
      i++;
    if (i == num)
      return mp;
  }
  return NULL;
}


static void
count_mail(dbref player, int folder, int *rcount, int *ucount, int *ccount)
{
  /* returns count of read, unread, & cleared messages as rcount, ucount,
   * ccount. folder=-1 returns for all folders */

  MAIL *mp;
  int rc, uc, cc;

  cc = rc = uc = 0;
  for (mp = find_exact_starting_point(player);
       mp && (mp->to == player); mp = mp->next) {
    if ((mp->to == player) && ((folder == -1) ||
                               (Folder(mp) == (mail_flag) folder))) {
      if (Cleared(mp))
        cc++;
      else if (Read(mp))
        rc++;
      else
        uc++;
    }
  }
  *rcount = rc;
  *ucount = uc;
  *ccount = cc;
}

/* ARGSUSED */
FUNCTION(fun_mailsend)
{
  /* mailsend(<target>,[<subject>/]<message>) */
  if ((fun->flags & FN_NOSIDEFX) || Gagged(executor) ||
      !command_check_byname(executor, "@MAIL", pe_info))
    safe_str(T(e_perm), buff, bp);
  else if (FUNCTION_SIDE_EFFECTS)
    do_mail_send(executor, args[0], args[1], 0, 1, 0);
  else
    safe_str(T(e_disabled), buff, bp);
}

static void
send_mail(dbref player, dbref target, char *subject, char *message,
          mail_flag flags, int silent, int nosig)
{
  /* send a message to a target, consulting the target's mailforward.
   * If mailforward isn't set, just deliver to targt.
   * If mailforward is set, deliver to the list if we're allowed
   *  (but don't check mailforward further!)
   */
  ATTR *a;
  int good = 0;

  a = atr_get_noparent(target, "MAILFORWARDLIST");
  if (!a) {
    /* Easy, no forwarding */
    real_send_mail(player, target, subject, message, flags, silent, nosig);
    return;
  } else {
    /* We have a forward list. Run through it. */
    char *fwdstr, *orig, *curr;
    dbref fwd;
    orig = safe_atr_value(a);
    fwdstr = trim_space_sep(orig, ' ');
    while ((curr = split_token(&fwdstr, ' ')) != NULL) {
      if (is_objid(curr)) {
        fwd = parse_objid(curr);
        if (GoodObject(fwd) && Can_MailForward(target, fwd)) {
          good +=
            real_send_mail(player, fwd, subject, message, flags, 1, nosig);
        } else
          notify_format(target, T("Failed attempt to forward @mail to #%d"),
                        fwd);
      }
    }
    free(orig);
  }
  if (!silent) {
    if (good)
      notify_format(player,
                    T("MAIL: You sent your message to %s."), Name(target));
    else
      notify_format(player,
                    T
                    ("MAIL: Your message was not sent to %s due to a mail forwarding problem."),
                    Name(target));
  }
}

static int
can_mail_to(dbref player, dbref target)
{
  if (!can_mail(player)) {
    return 0;
  }
  if (!(Hasprivs(player) || eval_lock(player, target, Mail_Lock))) {
    return 0;
  }
  return 1;
}

static int
real_send_mail(dbref player, dbref target, char *subject, char *message,
               mail_flag flags, int silent, int nosig)
{
  /* deliver a mail message to a target, period */

  MAIL *newp, *mp;
  int rc, uc, cc;
  char sbuf[BUFFER_LEN];
  ATTR *a;
  char *cp;

  if (!IsPlayer(target)) {
    if (!silent)
      notify(player, T("MAIL: You cannot send mail to non-existent people."));
    return 0;
  }
  if (!strcasecmp(message, "clear")) {
    notify(player,
           T("MAIL: You probably don't wanna send mail saying 'clear'."));
    return 0;
  }
  if (!(Hasprivs(player) || eval_lock(player, target, Mail_Lock))) {
    if (!silent) {
      cp = sbuf;
      safe_format(sbuf, &cp,
                  T("MAIL: %s is not accepting mail from you right now."),
                  Name(target));
      *cp = '\0';
    } else {
      cp = NULL;
    }
    fail_lock(player, target, Mail_Lock, cp, NOTHING);
    return 0;
  }
  count_mail(target, 0, &rc, &uc, &cc);
  if ((rc + uc + cc) >= MAIL_LIMIT) {
    if (!silent)
      notify_format(player, T("MAIL: %s's mailbox is full. Can't send."),
                    Name(target));
    return 0;
  }

  /* Where do we insert it? After mp, wherever that is.
   * This can return NULL if there are no messages or
   * if we insert at the head of the list
   */
  mp = find_insertion_point(target);

  /* initialize the appropriate fields */
  newp = slab_malloc(mail_slab, mp);
  newp->to = target;
  newp->from = player;
  newp->from_ctime = CreTime(player);

  /* Deal with the subject */
  cp = remove_markup(subject, NULL);
  if (subject && cp && *cp)
    strcpy(sbuf, cp);
  else
    strcpy(sbuf, T("(no subject)"));
  if ((flags & M_FORWARD) && !string_prefix(sbuf, "Fwd:"))
    newp->subject = compress(chopstr(tprintf("Fwd: %s", sbuf), SUBJECT_LEN));
  else if ((flags & M_REPLY) && !string_prefix(sbuf, "Re:"))
    newp->subject = compress(chopstr(tprintf("Re: %s", sbuf), SUBJECT_LEN));
  else if ((a = atr_get_noparent(player, "MAILSUBJECT")) != NULL)
    /* Don't bother to uncompress a->value */
    newp->subject = u_strdup(AL_STR(a));
  else
    newp->subject = compress(sbuf);
  if (flags & M_FORWARD) {
    /* Forwarding passes the message already compressed */
    size_t len = strlen(message) + 1;
    newp->msgid = chunk_create((unsigned char *) message, len, 1);
  } else {
    uint16_t len;
    unsigned char *text;
    char buff[BUFFER_LEN], newmsg[BUFFER_LEN], *nm = newmsg;

    safe_str(message, newmsg, &nm);
    if (!nosig
        && call_attrib(player, "MAILSIGNATURE", buff, player, NULL, NULL))
      safe_str(buff, newmsg, &nm);
    *nm = '\0';
    text = compress(newmsg);
    len = u_strlen(text) + 1;
    newp->msgid = chunk_create(text, len, 1);
    free(text);
  }

  newp->time = mudtime;
  newp->read = flags & M_FMASK; /* Send to folder 0 */

  if (mp) {
    newp->prev = mp;
    newp->next = mp->next;
    if (mp == TAIL)
      TAIL = newp;
    else
      mp->next->prev = newp;
    mp->next = newp;
  } else {
    if (HEAD) {
      /* Insert at the front */
      newp->next = HEAD;
      newp->prev = NULL;
      HEAD->prev = newp;
      HEAD = newp;
    } else {
      /* This is the first message in the maildb */
      HEAD = newp;
      TAIL = newp;
      newp->prev = NULL;
      newp->next = NULL;
    }
  }

  mdb_top++;

  /* notify people */
  if (!silent) {
    if (can_mail_to(target, player)) {
      notify_format(player,
                    T("MAIL: You sent your message to %s."), Name(target));
    } else {
      notify_format(player,
                    T
                    ("MAIL: You sent your message to %s, but they can't mail you!"),
                    Name(target));
    }
  }
  notify_format(target,
                T("MAIL: You have a new message (%d) from %s."),
                rc + uc + cc + 1, Name(player));

  /* Check @mailfilter */
  filter_mail(player, target, subject, message, rc + uc + cc + 1, flags);

  if (AMAIL_ATTR && (atr_get_noparent(target, "AMAIL"))
      && (player != target) && Hasprivs(target))
    did_it(player, target, NULL, NULL, NULL, NULL, "AMAIL", NOTHING);

  return 1;
}


/** Wipe the entire maildb.
 * \param player the enactor.
 */
void
do_mail_nuke(dbref player)
{
  MAIL *mp, *nextp;

  if (!God(player)) {
    notify(player, T("The postal service issues a warrant for your arrest."));
    return;
  }
  /* walk the list */
  for (mp = HEAD; mp != NULL; mp = nextp) {
    nextp = mp->next;
    if (mp->subject)
      free(mp->subject);
    chunk_delete(mp->msgid);
    slab_free(mail_slab, mp);
  }

  HEAD = TAIL = NULL;
  mdb_top = 0;

  do_log(LT_ERR, 0, 0, "** MAIL PURGE ** done by %s(#%d).",
         Name(player), player);
  notify(player, T("You annihilate the post office. All messages cleared."));
}

/** Low-level mail sanity checking or debugging.
 * "how to fix mail with a sledgehammer".
 * \param player the enactor.
 * \param action action to take (sanity, clear, fix)
 * \param victim name of player whose mail is to be checked (NULL to check all)
 */
void
do_mail_debug(dbref player, const char *action, const char *victim)
{
  dbref target;
  MAIL *mp, *nextp;
  int i;

  if (!Wizard(player)) {
    notify(player, T("Go get some bugspray."));
    return;
  }
  if (string_prefix("clear", action)) {
    target =
      match_result(player, victim, TYPE_PLAYER, MAT_PMATCH | MAT_ABSOLUTE);
    if (target == NOTHING) {
      notify_format(player, T("%s: No such player."), victim);
      return;
    }
    do_mail_clear(target, "ALL");
    do_mail_purge(target);
    notify_format(player, T("Mail cleared for %s(#%d)."), Name(target), target);
    return;
  } else if (string_prefix("sanity", action)) {
    for (i = 0, mp = HEAD; mp != NULL; i++, mp = mp->next) {
      if (!GoodObject(mp->to))
        notify_format(player, T("Bad object #%d has mail."), mp->to);
      else if (!IsPlayer(mp->to))
        notify_format(player, T("%s(#%d) has mail but is not a player."),
                      Name(mp->to), mp->to);
    }
    if (i != mdb_top) {
      notify_format(player,
                    T
                    ("Mail database top is %d, actual message count is %d. Fixing."),
                    mdb_top, i);
      mdb_top = i;
    }
    notify(player, T("Mail sanity check completed."));
  } else if (string_prefix("fix", action)) {
    for (i = 0, mp = HEAD; mp != NULL; i++, mp = nextp) {
      if (!GoodObject(mp->to) || !IsPlayer(mp->to)) {
        notify_format(player, T("Fixing mail for #%d."), mp->to);
        /* Delete this one */
        /* head and tail of the list are special */
        if (mp == HEAD)
          HEAD = mp->next;
        else if (mp == TAIL)
          TAIL = mp->prev;
        /* relink the list */
        if (mp->prev != NULL)
          mp->prev->next = mp->next;
        if (mp->next != NULL)
          mp->next->prev = mp->prev;
        /* save the pointer */
        nextp = mp->next;
        /* then wipe */
        mdb_top--;
        if (mp->subject)
          free(mp->subject);
        chunk_delete(mp->msgid);
        slab_free(mail_slab, mp);
      } else if (!GoodObject(mp->from)) {
        /* Oops, it's from a player whose dbref is out of range!
         * We'll make it appear to be from #0 instead because there's
         * no really good choice
         */
        mp->from = 0;
        nextp = mp->next;
      } else {
        nextp = mp->next;
      }
    }
    notify(player, T("Mail sanity fix completed."));
  } else {
    notify(player, T("That is not a debugging option."));
    return;
  }
}

/** Display mail database statistics.
 * \verbatim
 * This implements @mail/stat, @mail/dstat, @mail/fstat.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player whose mail stats are to be checked (NULL for all)
 * \param full level of verbosity: MSTATS_COUNT = just count messages,
 * MSTATS_READ = break down by read/unread, MSTATS_SIZE = include
 * space usage.
 */
void
do_mail_stats(dbref player, char *name, enum mail_stats_type full)
{
  dbref target;
  int fc, fr, fu, tc, tr, tu, fchars, tchars, cchars;
  char last[50];
  MAIL *mp;

  fc = fr = fu = tc = tr = tu = cchars = fchars = tchars = 0;

  /* find player */

  if (*name == '\0') {
    if (Wizard(player))
      target = AMBIGUOUS;
    else
      target = player;
  } else {
    target =
      match_result(player, name, TYPE_PLAYER,
                   MAT_TYPE | MAT_ABSOLUTE | MAT_PMATCH | MAT_ME);
    if (!GoodObject(target))
      target = NOTHING;
  }

  if ((target == NOTHING) || ((target == AMBIGUOUS) && !Wizard(player))) {
    notify_format(player, T("%s: No such player."), name);
    return;
  }
  if (!Wizard(player) && (target != player)) {
    notify(player, T("The post office protects privacy!"));
    return;
  }
  /* this comand is computationally expensive */

  if (target == AMBIGUOUS) {    /* stats for all */
    if (full == MSTATS_COUNT) {
      notify_format(player,
                    T("There are %d messages in the mail spool."), mdb_top);
      return;
    } else if (full == MSTATS_READ) {
      for (mp = HEAD; mp != NULL; mp = mp->next) {
        if (Cleared(mp))
          fc++;
        else if (Read(mp))
          fr++;
        else
          fu++;
      }
      notify_format(player,
                    T
                    ("MAIL: There are %d msgs in the mail spool, %d unread, %d cleared."),
                    fc + fr + fu, fu, fc);
      return;
    } else {
      for (mp = HEAD; mp != NULL; mp = mp->next) {
        if (Cleared(mp)) {
          fc++;
          cchars += strlen(get_message(mp));
        } else if (Read(mp)) {
          fr++;
          fchars += strlen(get_message(mp));
        } else {
          fu++;
          tchars += strlen(get_message(mp));
        }
      }
      notify_format(player,
                    T
                    ("MAIL: There are %d old msgs in the mail spool, totalling %d characters."),
                    fr, fchars);
      notify_format(player,
                    T
                    ("MAIL: There are %d new msgs in the mail spool, totalling %d characters."),
                    fu, tchars);
      notify_format(player,
                    T
                    ("MAIL: There are %d cleared msgs in the mail spool, totalling %d characters."),
                    fc, cchars);
      return;
    }
  }
  /* individual stats */

  if (full == MSTATS_COUNT) {
    /* just count number of messages */
    for (mp = HEAD; mp != NULL; mp = mp->next) {
      if (was_sender(target, mp))
        fr++;
      if (mp->to == target)
        tr++;
    }
    notify_format(player, T("%s sent %d messages."), Name(target), fr);
    notify_format(player, T("%s has %d messages."), Name(target), tr);
    return;
  }
  /* more detailed message count */
  for (mp = HEAD; mp != NULL; mp = mp->next) {
    if (was_sender(target, mp)) {
      if (Cleared(mp))
        fc++;
      else if (Read(mp))
        fr++;
      else
        fu++;
      if (full == MSTATS_SIZE)
        fchars += strlen(get_message(mp));
    }
    if (mp->to == target) {
      if (!tr && !tu)
        strcpy(last, show_time(mp->time, 0));
      if (Cleared(mp))
        tc++;
      else if (Read(mp))
        tr++;
      else
        tu++;
      if (full == MSTATS_SIZE)
        tchars += strlen(get_message(mp));
    }
  }

  notify_format(player, T("Mail statistics for %s:"), Name(target));

  if (full == MSTATS_READ) {
    notify_format(player, T("%d messages sent, %d unread, %d cleared."),
                  fc + fr + fu, fu, fc);
    notify_format(player, T("%d messages received, %d unread, %d cleared."),
                  tc + tr + tu, tu, tc);
  } else {
    notify_format(player,
                  T
                  ("%d messages sent, %d unread, %d cleared, totalling %d characters."),
                  fc + fr + fu, fu, fc, fchars);
    notify_format(player,
                  T
                  ("%d messages received, %d unread, %d cleared, totalling %d characters."),
                  tc + tr + tu, tu, tc, tchars);
  }

  if (tc + tr + tu > 0)
    notify_format(player, T("Last is dated %s"), last);
  return;
}


/** Main mail wrapper.
 * \verbatim
 * This implements the @mail command when called with no switches.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 left-hand argument (before the =)
 * \param arg2 right-hand argument (after the =)
 */
void
do_mail(dbref player, char *arg1, char *arg2)
{
  dbref sender;
  char *p;
  /* Force player to be a real player, but keep track of the
   * enactor in case we're sending mail, which objects can do
   */
  sender = player;
  player = Owner(player);
  if (!arg1 || !*arg1) {
    if (arg2 && *arg2) {
      notify(player, T("MAIL: Invalid mail command."));
      return;
    }
    /* just the "@mail" command */
    do_mail_list(player, "");
    return;
  }
  /* purge a player's mailbox */
  if (!strcasecmp(arg1, "purge")) {
    do_mail_purge(player);
    return;
  }
  /* clear message */
  if (!strcasecmp(arg1, "clear")) {
    do_mail_clear(player, arg2);
    return;
  }
  if (!strcasecmp(arg1, "unclear")) {
    do_mail_unclear(player, arg2);
    return;
  }
  if (arg2 && *arg2) {
    /* Sending mail */
    if (Gagged(sender))
      notify(sender, T("You cannot do that while gagged."));
    else
      do_mail_send(sender, arg1, arg2, 0, 0, 0);
  } else {
    /* Must be reading or listing mail - no arg2 */
    if (((p = strchr(arg1, ':')) && (*(p + 1) == '\0'))
        || !(isdigit((unsigned char) *arg1) && !strchr(arg1, '-')))
      do_mail_list(player, arg1);
    else
      do_mail_read(player, arg1);
  }
  return;
}

/*-------------------------------------------------------------------------*
 *   Auxiliary functions
 *-------------------------------------------------------------------------*/
/* ARGSUSED */
FUNCTION(fun_folderstats)
{
  /* This function can take one of four formats:
   * folderstats() -> returns stats for my current folder
   * folderstats(folder#) -> returns stats for my folder folder#
   * folderstats(player) -> returns stats for player's current folder
   * folderstats(player,folder#) -> returns stats for player's folder folder#
   */
  dbref player;
  int rc, uc, cc;

  switch (nargs) {
  case 0:
    count_mail(executor, player_folder(executor), &rc, &uc, &cc);
    break;
  case 1:
    if (!is_strict_integer(args[0])) {
      /* handle the case of wanting to count the number of messages */
      if ((player =
           noisy_match_result(executor, args[0], TYPE_PLAYER,
                              MAT_ME | MAT_ABSOLUTE | MAT_PMATCH | MAT_TYPE)) ==
          NOTHING) {
        safe_str(T("#-1 NO SUCH PLAYER"), buff, bp);
        return;
      } else if (!controls(executor, player)) {
        safe_str(T(e_perm), buff, bp);
        return;
      } else {
        count_mail(player, player_folder(player), &rc, &uc, &cc);
      }
    } else {
      count_mail(executor, parse_integer(args[0]), &rc, &uc, &cc);
    }
    break;
  case 2:
    if ((player =
         noisy_match_result(executor, args[0], TYPE_PLAYER,
                            MAT_ME | MAT_ABSOLUTE | MAT_PMATCH | MAT_TYPE)) ==
        NOTHING) {
      safe_str(T("#-1 NO SUCH PLAYER"), buff, bp);
      return;
    } else if (!controls(executor, player)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (!is_integer(args[1])) {
      safe_str(T("#-1 FOLDER MUST BE INTEGER"), buff, bp);
      return;
    }
    count_mail(player, parse_integer(args[1]), &rc, &uc, &cc);
    break;
  default:
    /* This should never happen */
    return;
  }

  safe_integer(rc, buff, bp);
  safe_chr(' ', buff, bp);
  safe_integer(uc, buff, bp);
  safe_chr(' ', buff, bp);
  safe_integer(cc, buff, bp);
  return;
}

/* ARGSUSED */
FUNCTION(fun_mail)
{
  /* mail([<player>,] [<folder #>:]<message #>)
   * mail() --> return total # of messages for executor
   * mail(<player>) --> return total # of messages for player
   */

  MAIL *mp;
  dbref player;
  int rc, uc, cc;

  if (nargs == 0) {
    count_mail(executor, -1, &rc, &uc, &cc);
    safe_integer(rc + uc + cc, buff, bp);
    return;
  }
  /* Try mail(<player>) */
  if (nargs == 1) {
    player =
      match_result(executor, args[0], TYPE_PLAYER,
                   MAT_ME | MAT_ABSOLUTE | MAT_PMATCH | MAT_TYPE);
    if (GoodObject(player)) {
      if (!controls(executor, player)) {
        safe_str(T(e_perm), buff, bp);
      } else {
        count_mail(player, -1, &rc, &uc, &cc);
        safe_integer(rc, buff, bp);
        safe_chr(' ', buff, bp);
        safe_integer(uc, buff, bp);
        safe_chr(' ', buff, bp);
        safe_integer(cc, buff, bp);
      }
      return;
    }
  }
  /* That didn't work. Ok, try mailfun_fetch */
  mp = mailfun_fetch(executor, nargs, args[0], args[1]);
  if (mp) {
    safe_str(get_message(mp), buff, bp);
    return;
  }
  safe_str(T("#-1 INVALID MESSAGE OR PLAYER"), buff, bp);
  return;
}

/* A helper routine used by all the mail*() functions
 * We parse the following format:
 *  func([<player>,] [<folder #>:]<message #>)
 * and return the matching message or NULL
 */
static MAIL *
mailfun_fetch(dbref player, int nargs, char *arg1, char *arg2)
{
  dbref target;
  int msg;
  int folder;

  if (nargs == 1) {
    /* Simply a message number or folder:message */
    if (parse_message_spec(player, arg1, &msg, NULL, &folder))
      return real_mail_fetch(player, msg, folder);
    else {
      return NULL;
    }
  } else {
    /* Both a target and a message */
    if ((target =
         noisy_match_result(player, arg1, TYPE_PLAYER,
                            MAT_ME | MAT_ABSOLUTE | MAT_PLAYER | MAT_TYPE)) ==
        NOTHING) {
      return NULL;
    } else if (!controls(player, target)) {
      notify(player, T("Permission denied"));
      return NULL;
    }
    if (parse_message_spec(target, arg2, &msg, NULL, &folder))
      return real_mail_fetch(target, msg, folder);
    else {
      notify(player, T("Invalid message specification"));
      return NULL;
    }
  }
  /* NOTREACHED */
  return NULL;
}


/* ARGSUSED */
FUNCTION(fun_mailfrom)
{
  MAIL *mp;

  mp = mailfun_fetch(executor, nargs, args[0], args[1]);
  if (!mp)
    safe_str("#-1", buff, bp);
  else if (!was_sender(mp->from, mp))
    safe_str("#-1", buff, bp);
  else
    safe_dbref(mp->from, buff, bp);
  return;
}


/* ARGSUSED */
FUNCTION(fun_mailstats)
{
  /* Copied from extmail.c: do_mail_stats with changes to refer to
   * args[0] rather than name, etc
   *
   * Todo: Change it so it is just mailstats and depending on what
   * it is called as, it will change if it is doing a fstats, dstats,
   * or stats. Looks like stats -> full=0, dstats -> full=1, fstats ->
   * full=2
   */

  /* mail database statistics */

  dbref target;
  int fc, fr, fu, tc, tr, tu, fchars, tchars, cchars;
  char last[50];
  MAIL *mp;
  int full;

  /* Figure out how we were called */
  if (string_prefix(called_as, "mailstats")) {
    full = 0;
  } else if (string_prefix(called_as, "maildstats")) {
    full = 1;
  } else if (string_prefix(called_as, "mailfstats")) {
    full = 2;
  } else {
    safe_str(T("#-? fun_mailstats called with invalid called_as!"), buff, bp);
    return;
  }

  fc = fr = fu = tc = tr = tu = cchars = fchars = tchars = 0;

  /* find player */
  if (*args[0] == '\0') {
    if Wizard
      (executor)
        target = AMBIGUOUS;
    else
      target = executor;
  } else {
    target =
      match_result(executor, args[0], TYPE_PLAYER,
                   MAT_TYPE | MAT_ABSOLUTE | MAT_PMATCH | MAT_ME);
    if (!GoodObject(target))
      target = NOTHING;
  }

  if (!GoodObject(target) || !IsPlayer(target)) {
    notify_format(executor, T("%s: No such player."), args[0]);
    return;
  }
  if (!controls(executor, target)) {
    notify(executor, T("The post office protects privacy!"));
    return;
  }

  if (target == AMBIGUOUS) {    /* stats for all */
    if (full == 0) {
      /* FORMAT
       * total mail
       */
      safe_integer(mdb_top, buff, bp);
      return;
    } else if (full == 1) {
      for (mp = HEAD; mp != NULL; mp = mp->next) {
        if (Cleared(mp))
          fc++;
        else if (Read(mp))
          fr++;
        else
          fu++;
      }
      /* FORMAT
       * sent, sent_unread, sent_cleared
       */
      safe_format(buff, bp, "%d %d %d", fc + fr + fu, fu, fc);
    } else {
      for (mp = HEAD; mp != NULL; mp = mp->next) {
        if (Cleared(mp)) {
          fc++;
          cchars += strlen(get_message(mp));
        } else if (Read(mp)) {
          fr++;
          fchars += strlen(get_message(mp));
        } else {
          fu++;
          tchars += strlen(get_message(mp));
        }
      }
      /* FORMAT
       * sent_read, sent_read_characters,
       * sent_unread, sent_unread_characters,
       * sent_clear, sent_clear_characters
       */
      safe_format(buff, bp, "%d %d %d %d %d %d",
                  fr, fchars, fu, tchars, fc, cchars);
      return;
    }
  }

  /* individual stats */

  if (full == 0) {
    /* just count number of messages */
    for (mp = HEAD; mp != NULL; mp = mp->next) {
      if (was_sender(target, mp))
        fr++;
      if (mp->to == target)
        tr++;
    }
    /* FORMAT
     * sent, received
     */
    safe_format(buff, bp, "%d %d", fr, tr);
    return;
  }
  /* more detailed message count */
  for (mp = HEAD; mp != NULL; mp = mp->next) {
    if (was_sender(target, mp)) {
      if (Cleared(mp))
        fc++;
      else if (Read(mp))
        fr++;
      else
        fu++;
      if (full == 2)
        fchars += strlen(get_message(mp));
    }
    if (mp->to == target) {
      if (!tr && !tu)
        strcpy(last, show_time(mp->time, 0));
      if (Cleared(mp))
        tc++;
      else if (Read(mp))
        tr++;
      else
        tu++;
      if (full == 2)
        tchars += strlen(get_message(mp));
    }
  }

  if (full == 1) {
    /* FORMAT
     * sent, sent_unread, sent_cleared
     * received, rec_unread, rec_cleared
     */
    safe_format(buff, bp, "%d %d %d %d %d %d",
                fc + fr + fu, fu, fc, tc + tr + tu, tu, tc);
  } else {
    /* FORMAT
     * sent, sent_unread, sent_cleared, sent_bytes
     * received, rec_unread, rec_cleared, rec_bytes
     */
    safe_format(buff, bp, "%d %d %d %d %d %d %d %d",
                fc + fr + fu, fu, fc, fchars, tc + tr + tu, tu, tc, tchars);
  }
}


/* ARGSUSED */
FUNCTION(fun_mailtime)
{
  MAIL *mp;

  mp = mailfun_fetch(executor, nargs, args[0], args[1]);
  if (!mp)
    safe_str("#-1", buff, bp);
  else
    safe_str(show_time(mp->time, 0), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_mailstatus)
{
  MAIL *mp;

  mp = mailfun_fetch(executor, nargs, args[0], args[1]);
  if (!mp)
    safe_str("#-1", buff, bp);
  else
    safe_str(status_chars(mp), buff, bp);
  return;
}


/* ARGSUSED */
FUNCTION(fun_mailsubject)
{
  MAIL *mp;

  mp = mailfun_fetch(executor, nargs, args[0], args[1]);
  if (!mp)
    safe_str("#-1", buff, bp);
  else
    safe_str(uncompress(mp->subject), buff, bp);
  return;
}

/** Save mail to disk.
 * \param fp pointer to filehandle to save mail to.
 * \return number of mail messages saved.
 */
int
dump_mail(PENNFILE *fp)
{
  MAIL *mp;
  int count = 0;
  int mail_flags = 0;

  mail_flags += MDBF_SUBJECT;
  mail_flags += MDBF_ALIASES;
  mail_flags += MDBF_NEW_EOD;
  mail_flags += MDBF_SENDERCTIME;

  if (mail_flags)
    penn_fprintf(fp, "+%d\n", mail_flags);

  save_malias(fp);

  penn_fprintf(fp, "%d\n", mdb_top);

  for (mp = HEAD; mp != NULL; mp = mp->next) {
    putref(fp, mp->to);
    putref(fp, mp->from);
    putref(fp, mp->from_ctime);
    putstring(fp, show_time(mp->time, 0));
    if (mp->subject)
      putstring(fp, uncompress(mp->subject));
    else
      putstring(fp, "");
    putstring(fp, get_message(mp));
    putref(fp, mp->read);
    count++;
  }

  penn_fputs(EOD, fp);

  if (count != mdb_top) {
    do_log(LT_ERR, 0, 0, "MAIL: Count of messages is %d, mdb_top is %d.",
           count, mdb_top);
    mdb_top = count;            /* Doesn't help if we forked, but oh well */
  }
  return count;
}



/** Find the first message in a player's mail chain, or NULL if none.
 * We first try to find a good place to start by looking for a connected
 * player with a dbref nearest to our target player. Then it's a linear
 * search.
 * \param player the player to search for.
 * \return pointer to first message in their mail chain, or NULL.
 */
MAIL *
find_exact_starting_point(dbref player)
{
  static MAIL *mp;

  if (!HEAD)
    return NULL;

  mp = get_objdata(player, "MAIL");
  if (!mp) {
    /* Player hasn't already done something that looks up their mail. */
    if (HEAD->to > player)
      return NULL;              /* No mail chain */
    for (mp = HEAD; mp && (mp->to < player); mp = mp->next) ;
  } else {
    while (mp && (mp->to >= player))
      mp = mp->prev;
    if (!mp)
      mp = HEAD;
    while (mp && (mp->to < player))
      mp = mp->next;
  }
  if (mp && (mp->to == player)) {
    set_objdata(player, "MAIL", mp);
    return mp;
  } else
    return NULL;
}


/* Find the place where new mail to this player should go (after):
 *  1. The last message in the player's mail chain, or
 *  2. The last message before where the player's chain should start, or
 *  3. NULL (meaning TAIL)
 */
static MAIL *
find_insertion_point(dbref player)
{
  static MAIL *mp;

  if (!HEAD)
    return NULL;
  mp = get_objdata(player, "MAIL");
  if (!mp) {
    if (HEAD->to > player)
      return NULL;              /* No mail chain */
    for (mp = TAIL; mp && (mp->to > player); mp = mp->prev) ;
  } else {
    while (mp && (mp->to <= player))
      mp = mp->next;
    if (!mp)
      mp = TAIL;
    while (mp && (mp->to > player))
      mp = mp->prev;
  }
  return mp;
}


/** Initialize the mail database pointers */
void
mail_init(void)
{
  static bool init_called = 0;
  if (!init_called) {
    init_called = 1;
    mdb_top = 0;
    mail_slab = slab_create("mail messages", sizeof(struct mail));
    slab_set_opt(mail_slab, SLAB_HINTLESS_THRESHOLD, 5);
    HEAD = NULL;
    TAIL = NULL;
  }
}

/** Load mail from disk.
 * \param fp pointer to filehandle from which to load mail.
 */
int
load_mail(PENNFILE *fp)
{
  char nbuf1[8];
  unsigned char *tbuf = NULL;
  unsigned char *text;
  size_t len;
  int mail_top = 0;
  int mail_flags = 0;
  int i = 0;
  MAIL *mp, *tmpmp;
  int done = 0;
  char sbuf[BUFFER_LEN];
  struct tm ttm;

  /* find out how many messages we should be loading */
  penn_fgets(nbuf1, sizeof(nbuf1), fp);
  /* If it starts with +, it's telling us the mail db flags */
  if (*nbuf1 == '+') {
    mail_flags = atoi(nbuf1 + 1);
    /* If the flags indicates aliases, we'll read them now */
    if (mail_flags & MDBF_ALIASES) {
      load_malias(fp);
    }
    penn_fgets(nbuf1, sizeof(nbuf1), fp);
  }
  mail_top = atoi(nbuf1);
  if (!mail_top) {
    /* mail_top could be 0 from an error or actually be 0. */
    if (nbuf1[0] == '0' && nbuf1[1] == '\n') {
      char buff[20];
      if (!penn_fgets(buff, sizeof buff, fp))
        do_rawlog(LT_ERR, "MAIL: Missing end-of-dump marker in mail database.");
      else if (strcmp(buff, (mail_flags & MDBF_NEW_EOD)
                      ? "***END OF DUMP***\n" : "*** END OF DUMP ***\n") == 0)
        return 1;
      else
        do_rawlog(LT_ERR, "MAIL: Trailing garbage in the mail database.");
    }
    return 0;
  }
  /* first one is a special case */
  mp = slab_malloc(mail_slab, NULL);
  mp->to = getref(fp);
  mp->from = getref(fp);
  if (mail_flags & MDBF_SENDERCTIME)
    mp->from_ctime = (time_t) getref(fp);
  else
    mp->from_ctime = 0;         /* No one will have this creation time */

  if (do_convtime(getstring_noalloc(fp), &ttm))
    mp->time = mktime(&ttm);
  else                          /* do_convtime failed. Odd. */
    mp->time = mudtime;

  if (mail_flags & MDBF_SUBJECT) {
    tbuf = compress(getstring_noalloc(fp));
  }
  text = compress(getstring_noalloc(fp));
  len = u_strlen(text) + 1;
  mp->msgid = chunk_create(text, len, 1);
  free(text);
  if (mail_flags & MDBF_SUBJECT)
    mp->subject = tbuf;
  else {
    strcpy(sbuf, get_message(mp));
    mp->subject = compress(chopstr(sbuf, SUBJECT_LEN));
  }
  mp->read = getref(fp);
  mp->next = NULL;
  mp->prev = NULL;
  HEAD = mp;
  TAIL = mp;
  i++;

  /* now loop through */
  for (; i < mail_top; i++) {
    mp = slab_malloc(mail_slab, NULL);
    mp->to = getref(fp);
    mp->from = getref(fp);
    if (mail_flags & MDBF_SENDERCTIME)
      mp->from_ctime = (time_t) getref(fp);
    else
      mp->from_ctime = 0;       /* No one will have this creation time */
    if (do_convtime(getstring_noalloc(fp), &ttm))
      mp->time = mktime(&ttm);
    else                        /* do_convtime failed. Odd. */
      mp->time = mudtime;
    if (mail_flags & MDBF_SUBJECT)
      tbuf = compress(getstring_noalloc(fp));
    else
      tbuf = NULL;
    text = compress(getstring_noalloc(fp));
    len = u_strlen(text) + 1;
    mp->msgid = chunk_create(text, len, 1);
    free(text);
    if (tbuf)
      mp->subject = tbuf;
    else {
      mush_strncpy(sbuf, get_message(mp), BUFFER_LEN);
      mp->subject = compress(chopstr(sbuf, SUBJECT_LEN));
    }
    mp->read = (uint32_t) getref(fp);

    /* We now to a sorted insertion, sorted by recipient db# */
    if (mp->to >= TAIL->to) {
      /* Pop it onto the end */
      mp->next = NULL;
      mp->prev = TAIL;
      TAIL->next = mp;
      TAIL = mp;
    } else {
      /* Search for where to put it */
      mp->prev = NULL;
      for (done = 0, tmpmp = HEAD; tmpmp && !done; tmpmp = tmpmp->next) {
        if (tmpmp->to > mp->to) {
          /* Insert before tmpmp */
          mp->next = tmpmp;
          mp->prev = tmpmp->prev;
          if (tmpmp->prev) {
            /* tmpmp isn't HEAD */
            tmpmp->prev->next = mp;
          } else {
            /* tmpmp is HEAD */
            HEAD = mp;
          }
          tmpmp->prev = mp;
          done = 1;
        }
      }
      if (!done) {
        /* This is bad */
        do_rawlog(LT_ERR, "MAIL: bad code.");
      }
    }
  }

  mdb_top = i;

  if (i != mail_top) {
    do_rawlog(LT_ERR, "MAIL: mail_top is %d, only read in %d messages.",
              mail_top, i);
  }
  {
    char buff[20];
    if (!penn_fgets(buff, sizeof buff, fp))
      do_rawlog(LT_ERR, "MAIL: Missing end-of-dump marker in mail database.");
    else if (strcmp(buff, (mail_flags & MDBF_NEW_EOD)
                    ? EOD : "*** END OF DUMP ***\n") != 0)
      /* There's still stuff. Icky. */
      do_rawlog(LT_ERR, "MAIL: Trailing garbage in the mail database.");
  }

  do_mail_debug(GOD, "fix", "");
  slab_set_opt(mail_slab, SLAB_ALLOC_BEST_FIT, 1);
  return mdb_top;
}

static int
get_folder_number(dbref player, char *name)
{
  ATTR *a;
  char str[BUFFER_LEN], pat[BUFFER_LEN], *res, *p;
  /* Look up a folder name and return the appopriate number */
  a = (ATTR *) atr_get_noparent(player, "MAILFOLDERS");
  if (!a)
    return -1;
  mush_strncpy(str, atr_value(a), BUFFER_LEN);
  snprintf(pat, BUFFER_LEN, ":%s:", strupper(name));
  res = strstr(str, pat);
  if (!res)
    return -1;
  res += 2 + strlen(name);
  p = res;
  while (isdigit((unsigned char) *p))
    p++;
  *p = '\0';
  return atoi(res);
}

static char *
get_folder_name(dbref player, int fld)
{
  static char str[BUFFER_LEN];
  char pat[BUFFER_LEN];
  static char *old;
  char *r;
  ATTR *a;

  /* Get the name of the folder, or "nameless" */
  sprintf(pat, "%d:", fld);
  old = NULL;
  a = (ATTR *) atr_get_noparent(player, "MAILFOLDERS");
  if (!a) {
    strcpy(str, "unnamed");
    return str;
  }
  strcpy(str, atr_value(a));
  old = (char *) string_match(str, pat);
  if (old) {
    r = old + strlen(pat);
    while (*r != ':')
      r++;
    *r = '\0';
    return old + strlen(pat);
  } else {
    strcpy(str, "unnamed");
    return str;
  }
}

/** Assign a name to a folder.
 * \param player dbref of player whose folder is to be named.
 * \param fld folder number.
 * \param name new name for folder.
 */
void
add_folder_name(dbref player, int fld, const char *name)
{
  char *old, *res, *r;
  char *new, *pat, *str, *tbuf;
  ATTR *a;

  /* Muck with the player's MAILFOLDERS attrib to add a string of the form:
   * number:name:number to it, replacing any such string with a matching
   * number.
   */
  new = (char *) mush_malloc(BUFFER_LEN, "string");
  pat = (char *) mush_malloc(BUFFER_LEN, "string");
  str = (char *) mush_malloc(BUFFER_LEN, "string");
  tbuf = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!new || !pat || !str || !tbuf)
    mush_panic("Failed to allocate strings in add_folder_name");

  if (name && *name)
    snprintf(new, BUFFER_LEN, "%d:%s:%d ", fld, strupper(name), fld);
  else
    strcpy(new, " ");
  sprintf(pat, "%d:", fld);
  /* get the attrib and the old string, if any */
  old = NULL;
  a = (ATTR *) atr_get_noparent(player, "MAILFOLDERS");
  if (a) {
    strcpy(str, atr_value(a));
    old = (char *) string_match(str, pat);
  }
  if (old && *old) {
    mush_strncpy(tbuf, str, BUFFER_LEN);
    r = old;
    while (!isspace((unsigned char) *r))
      r++;
    *r = '\0';
    res = replace_string(old, new, tbuf);       /* mallocs mem! */
  } else {
    r = res = (char *) mush_malloc(BUFFER_LEN + 1, "replace_string.buff");
    if (a)
      safe_str(str, res, &r);
    safe_str(new, res, &r);
    *r = '\0';
  }
  /* put the attrib back */
  (void) atr_add(player, "MAILFOLDERS", res, GOD,
                 AF_WIZARD | AF_NOPROG | AF_LOCKED);
  mush_free(res, "replace_string.buff");
  mush_free(new, "string");
  mush_free(pat, "string");
  mush_free(str, "string");
  mush_free(tbuf, "string");
}

static int
player_folder(dbref player)
{
  /* Return the player's current folder number. If they don't have one, set
   * it to 0 */
  ATTR *a;

  a = (ATTR *) atr_get_noparent(player, "MAILCURF");
  if (!a) {
    set_player_folder(player, 0);
    return 0;
  }
  return atoi(atr_value(a));
}

/** Set a player's current mail folder to a given folder.
 * \param player the player whose current folder is to be set.
 * \param fnum folder number to make current.
 */
void
set_player_folder(dbref player, int fnum)
{
  /* Set a player's folder to fnum */
  ATTR *a;
  char tbuf1[BUFFER_LEN];

  sprintf(tbuf1, "%d", fnum);
  a = (ATTR *) atr_match("MAILCURF");
  if (a)
    (void) atr_add(player, a->name, tbuf1, GOD, a->flags);
  else                          /* Shouldn't happen, but... */
    (void) atr_add(player, "MAILCURF", tbuf1, GOD,
                   AF_WIZARD | AF_NOPROG | AF_LOCKED);
}

static int
parse_folder(dbref player, char *folder_string)
{
  int fnum;

  /* Given a string, return a folder #, or -1 The string is just a number,
   * for now. Later, this will be where named folders are handled */
  if (!folder_string || !*folder_string)
    return -1;
  if (isdigit((unsigned char) *folder_string)) {
    fnum = atoi(folder_string);
    if ((fnum < 0) || (fnum > MAX_FOLDERS))
      return -1;
    else
      return fnum;
  }
  /* Handle named folders here */
  return get_folder_number(player, folder_string);
}

static int
mail_match(dbref player, MAIL *mp, struct mail_selector ms, int num)
{
  int diffdays;
  mail_flag mpflag;

  /* Does a piece of mail match the mail_selector? */
  if (ms.low && num < ms.low)
    return 0;
  if (ms.high && num > ms.high)
    return 0;
  if (ms.player && !was_sender(ms.player, mp))
    return 0;
  mpflag = Read(mp) ? mp->read : (mp->read | M_MSUNREAD);
  if (!All(ms) && !(ms.flags & mpflag))
    return 0;
  if (AllInFolder(ms) && (Folder(mp) == (mail_flag) player_folder(player)))
    return 1;
  if (ms.days != -1) {
    /* Get the time now, subtract mp->time, and compare the results with
     * ms.days (in manner of ms.day_comp) */
    diffdays = (int) (difftime(mudtime, mp->time) / 86400);
    if (sign(diffdays - ms.days) != ms.day_comp)
      return 0;
    else
      return 1;
  }
  return 1;
}

static int
parse_msglist(const char *msglist, struct mail_selector *ms, dbref player)
{
  char *p;
  char tbuf1[BUFFER_LEN];
  int folder;

  /* Take a message list, and return the appropriate mail_selector setup. For
   * now, msglists are quite restricted. That'll change once all this is
   * working. Returns 0 if couldn't parse, and also notifys player of why. */
  /* Initialize the mail selector - this matches all messages */
  ms->low = 0;
  ms->high = 0;
  ms->flags = 0x00FF | M_MSUNREAD | M_FOLDER;
  ms->player = 0;
  ms->days = -1;
  ms->day_comp = 0;
  /* Now, parse the message list */
  if (!msglist || !*msglist) {
    /* All messages in current folder */
    return 1;
  }
  /* Don't mess with msglist itself */
  strncpy(tbuf1, msglist, BUFFER_LEN - 1);
  p = tbuf1;
  while (p && *p && isspace((unsigned char) *p))
    p++;
  if (!p || !*p)
    return 1;                   /* all messages in current folder */

  if (isdigit((unsigned char) *p) || *p == '-') {
    if (!parse_message_spec(player, p, &ms->low, &ms->high, &folder)) {
      notify(player, T("MAIL: Invalid message specification"));
      return 0;
    }
    /* remove current folder when other folder specified */
    ms->flags &= ~M_FOLDER;
    ms->flags |= FolderBit(folder);
  } else if (*p == '~') {
    /* exact # of days old */
    p++;
    if (!p || !*p) {
      notify(player, T("MAIL: Invalid age"));
      return 0;
    }
    if (!is_integer(p)) {
      notify(player, T("MAIL: Message ages must be integers"));
      return 0;
    }
    ms->day_comp = 0;
    ms->days = atoi(p);
    if (ms->days < 0) {
      notify(player, T("MAIL: Invalid age"));
      return 0;
    }
  } else if (*p == '<') {
    /* less than # of days old */
    p++;
    if (!p || !*p) {
      notify(player, T("MAIL: Invalid age"));
      return 0;
    }
    if (!is_integer(p)) {
      notify(player, T("MAIL: Message ages must be integers"));
      return 0;
    }
    ms->day_comp = -1;
    ms->days = atoi(p);
    if (ms->days < 0) {
      notify(player, T("MAIL: Invalid age"));
      return 0;
    }
  } else if (*p == '>') {
    /* greater than # of days old */
    p++;
    if (!p || !*p) {
      notify(player, T("MAIL: Invalid age"));
      return 0;
    }
    if (!is_integer(p)) {
      notify(player, T("MAIL: Message ages must be integers"));
      return 0;
    }
    ms->day_comp = 1;
    ms->days = atoi(p);
    if (ms->days < 0) {
      notify(player, T("MAIL: Invalid age"));
      return 0;
    }
  } else if (*p == '#') {
    /* From db# */
    if (!is_objid(p)) {
      notify(player, T("MAIL: Invalid dbref #"));
      return 0;
    }
    ms->player = parse_objid(p);
    if (!GoodObject(ms->player) || !(ms->player)) {
      notify(player, T("MAIL: Invalid dbref #"));
      return 0;
    }
  } else if (*p == '*') {
    /* From player name */
    p++;
    if (!p || !*p) {
      notify(player, T("MAIL: Invalid player"));
      return 0;
    }
    ms->player = lookup_player(p);
    if (ms->player == NOTHING) {
      notify(player, T("MAIL: Invalid player"));
      return 0;
    }
  } else if (!strcasecmp(p, "all")) {
    ms->flags = M_ALL;
  } else if (!strcasecmp(p, "folder")) {
    ms->flags |= M_FOLDER;
  } else if (!strcasecmp(p, "urgent")) {
    ms->flags = M_URGENT | M_FOLDER;
  } else if (!strcasecmp(p, "unread")) {
    ms->flags = M_MSUNREAD | M_FOLDER;
  } else if (!strcasecmp(p, "read")) {
    ms->flags = M_MSGREAD | M_FOLDER;
  } else if (!strcasecmp(p, "clear") || !strcasecmp(p, "cleared")) {
    ms->flags = M_CLEARED | M_FOLDER;
  } else if (!strcasecmp(p, "tag") || !strcasecmp(p, "tagged")) {
    ms->flags = M_TAG;
  } else if (!strcasecmp(p, "mass")) {
    ms->flags = M_MASS;
  } else if (!strcasecmp(p, "me")) {
    ms->player = player;
  } else {
    notify(player, T("MAIL: Invalid message specification"));
    return 0;
  }
  return 1;
}

static char *
status_chars(MAIL *mp)
{
  /* Return a short description of message flags */
  static char res[10];
  char *p;

  res[0] = '\0';
  p = res;
  *p++ = Read(mp) ? '-' : 'N';
  *p++ = Cleared(mp) ? 'C' : '-';
  *p++ = Urgent(mp) ? 'U' : '-';
  /* *p++ = Mass(mp) ? 'M' : '-'; */
  *p++ = Forward(mp) ? 'F' : '-';
  *p++ = Tagged(mp) ? '+' : '-';
  *p = '\0';
  return res;
}

static char *
status_string(MAIL *mp)
{
  /* Return a longer description of message flags */
  static char tbuf1[BUFFER_LEN];
  char *tp;

  tp = tbuf1;
  if (Read(mp))
    safe_str(T("Read "), tbuf1, &tp);
  else
    safe_str(T("Unread "), tbuf1, &tp);
  if (Cleared(mp))
    safe_str(T("Cleared "), tbuf1, &tp);
  if (Urgent(mp))
    safe_str(T("Urgent "), tbuf1, &tp);
  if (Mass(mp))
    safe_str(T("Mass "), tbuf1, &tp);
  if (Forward(mp))
    safe_str(T("Fwd "), tbuf1, &tp);
  if (Tagged(mp))
    safe_str(T("Tagged"), tbuf1, &tp);
  *tp = '\0';
  return tbuf1;
}

/** Scan all mail folders for unread mail.
 * \param player player to check for new mail (and to notify).
 */
void
check_all_mail(dbref player)
{
  int folder;                   /* which folder */
  int rc;                       /* read messages */
  int uc;                       /* unread messages */
  int cc;                       /* cleared messages */
  int subtotal;                 /* total messages per iteration */
  int total = 0;                /* total messages */

  /* search through all the folders. */

  for (folder = 0; folder <= MAX_FOLDERS; folder++) {
    count_mail(player, folder, &rc, &uc, &cc);
    subtotal = rc + uc + cc;
    if (subtotal > 0) {
      notify_format(player,
                    T
                    ("MAIL: %d messages in folder %d [%s] (%d unread, %d cleared)."),
                    subtotal, folder, get_folder_name(player, folder), uc, cc);
      total += subtotal;
      if (folder == 0 && (subtotal + 5) > MAIL_LIMIT)
        notify_format(player,
                      T("MAIL: Warning! Limit on inbox messages is %d!"),
                      MAIL_LIMIT);
    }
  }

  if (!total)
    notify(player, T("\nMAIL: You have no mail.\n"));
  return;
}

/** Check for new mail.
 * \param player player to check for new mail (and to notify).
 * \param folder folder number to check.
 * \param silent if 1, don't tell player if they have no mail.
 */
void
check_mail(dbref player, int folder, int silent)
{
  int rc;                       /* read messages */
  int uc;                       /* unread messages */
  int cc;                       /* cleared messages */
  int total;

  /* just count messages */
  count_mail(player, folder, &rc, &uc, &cc);
  total = rc + uc + cc;
  if (total > 0)
    notify_format(player,
                  T
                  ("MAIL: %d messages in folder %d [%s] (%d unread, %d cleared)."),
                  total, folder, get_folder_name(player, folder), uc, cc);
  else if (!silent)
    notify(player, T("\nMAIL: You have no mail.\n"));
  if ((folder == 0) && (total + 5 > MAIL_LIMIT))
    notify_format(player, T("MAIL: Warning! Limit on inbox messages is %d!"),
                  MAIL_LIMIT);
  return;
}

static int
sign(int x)
{
  if (x == 0) {
    return 0;
  } else if (x < 0) {
    return -1;
  } else {
    return 1;
  }
}

/* See if we've been given something of the form [f:]m1[-m2]
 * If so, return 1 and set f and mlow and mhigh
 * If not, return 0
 * If msghigh is given as NULL, don't allow ranges
 * Used in parse_msglist, fun_mail and relatives.
 */
static int
parse_message_spec(dbref player, const char *s, int *msglow, int *msghigh,
                   int *folder)
{
  char buf[BUFFER_LEN];
  char *p, *q;
  if (!s || !*s)
    return 0;
  strcpy(buf, s);
  if ((p = strchr(buf, ':'))) {
    *p++ = '\0';
    if (!is_integer(buf))
      return 0;
    *folder = parse_integer(buf);
    if (msghigh && (q = strchr(p, '-'))) {
      /* f:low-high */
      *q++ = '\0';
      if (!*p)
        *msglow = 0;
      else if (!is_integer(p))
        return 0;
      else {
        *msglow = parse_integer(p);
        if (*msglow == 0)
          *msglow = -1;
      }
      if (!*q)
        *msghigh = 0;
      else if (!is_integer(q))
        return 0;
      else {
        *msghigh = parse_integer(q);
        if (*msghigh == 0)
          *msghigh = -1;
      }
    } else {
      /* f:m */
      if (!*p) {
        /* f: */
        *msglow = 0;
        if (msghigh)
          *msghigh = INT_MAX;
      } else {
        if (!is_integer(p))
          return 0;
        *msglow = parse_integer(p);
        if (*msglow == 0)
          *msglow = -1;
        if (msghigh)
          *msghigh = *msglow;
      }
    }
    if (*msglow < 0 || (msghigh && *msghigh < 0) || *folder < 0
        || *folder > MAX_FOLDERS)
      return 0;
  } else {
    /* No folder spec */
    *folder = player_folder(player);
    if (msghigh && (q = strchr(buf, '-'))) {
      /* low-high */
      *q++ = '\0';
      if (!*buf)
        *msglow = 0;
      else if (!is_integer(buf))
        return 0;
      else {
        *msglow = parse_integer(buf);
        if (*msglow == 0)
          *msglow = -1;
      }
      if (!*q)
        *msghigh = 0;
      else if (!is_integer(q))
        return 0;
      else {
        *msghigh = parse_integer(q);
        if (*msghigh == 0)
          *msghigh = -1;
      }
    } else {
      /* m */
      if (!is_integer(buf))
        return 0;
      *msglow = parse_integer(buf);
      if (*msglow == 0)
        *msglow = -1;
      if (msghigh)
        *msghigh = *msglow;
    }
    if (*msglow < 0 || (msghigh && *msghigh < 0))
      return 0;
  }
  return 1;
}

static int
send_mail_alias(dbref player, char *aname, char *subject, char *message,
                mail_flag flags, int silent, int nosig)
{
  struct mail_alias *m;
  int i;

  /* send a mail message to each player on an alias */
  /* We return 0 if this wasn't an alias */
  m = get_malias(player, aname);
  if (!m)
    return 0;
  /* Is it an alias they can use? */
  if (!((m->owner == player) || (m->nflags == 0) ||
        (Hasprivs(player)) ||
        ((m->nflags & ALIAS_MEMBERS) && ismember(m, player))))
    return 0;

  /* If they are not allowed to see the people on the alias, then
   * we must treat this as a case of silent mailing.
   */
  if (!((m->owner == player) || (m->mflags == 0) ||
        (Hasprivs(player)) ||
        ((m->mflags & ALIAS_MEMBERS) && ismember(m, player)))) {
    silent = 1;
    notify_format(player,
                  T("You sent your message to the '%s' alias"), m->name);
  }

  for (i = 0; i < m->size; i++) {
    send_mail(player, m->members[i], subject, message, flags, silent, nosig);
  }
  return 1;                     /* Success */
}


/** Event for @mailfilter.
 * \param from the @mailing player.
 * \param player the player to act on.
 * \param subject the subject of the @mail.
 * \param message the body of the @mail.
 * \param mailnumber the number of the @mail to file.
 * \param flags the flags of the @mail.
 */
void
filter_mail(dbref from, dbref player, char *subject,
            char *message, int mailnumber, mail_flag flags)
{
  ATTR *f;
  char buff[BUFFER_LEN];
  char buf[FOLDER_NAME_LEN + 1];
  int j = 0;
  static char tbuf1[6];
  PE_REGS *pe_regs;

  /* Does the player have a @mailfilter? */
  f = atr_get(player, "MAILFILTER");
  if (!f)
    return;

  if (flags & M_URGENT)
    tbuf1[j++] = 'U';
  if (flags & M_FORWARD)
    tbuf1[j++] = 'F';
  if (flags & M_REPLY)
    tbuf1[j++] = 'R';
  tbuf1[j] = '\0';

  pe_regs = pe_regs_create(PE_REGS_ARG, "filter_mail");
  pe_regs_setenv(pe_regs, 0, unparse_dbref(from));
  pe_regs_setenv_nocopy(pe_regs, 1, subject);
  pe_regs_setenv_nocopy(pe_regs, 2, message);
  pe_regs_setenv_nocopy(pe_regs, 3, tbuf1);
  call_attrib(player, "MAILFILTER", buff, from, NULL, pe_regs);
  pe_regs_free(pe_regs);

  if (*buff) {
    sprintf(buf, "0:%d", mailnumber);
    do_mail_file(player, buf, buff);
  }
}
