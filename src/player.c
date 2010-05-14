/**
 * \file player.c
 *
 * \brief Player creation and connection for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"
#include <stdio.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <stdlib.h>
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
#include <fcntl.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "access.h"
#include "mymalloc.h"
#include "log.h"
#include "dbdefs.h"
#include "flags.h"
#include "lock.h"
#include "parse.h"

#ifdef HAS_CRYPT
#ifdef I_CRYPT
#include <crypt.h>
#else
extern char *crypt(const char *, const char *);
#endif
#endif

#include "extmail.h"
#include "confmagic.h"

dbref email_register_player
  (const char *name, const char *email, const char *host, const char *ip);
static dbref make_player
  (const char *name, const char *password, const char *host, const char *ip);
void do_password(dbref player, dbref cause, const char *old,
                 const char *newobj);

static const char pword_attr[] = "XYXXY";

extern struct db_stat_info current_state;

/** Check a player's password against a given string.
 * \param player dbref of player.
 * \param password plaintext password string to check.
 * \retval 1 password matches (or player has no password).
 * \retval 0 password fails to match.
 */
int
password_check(dbref player, const char *password)
{
  ATTR *a;
  char *saved;
  char *passwd;

  /* read the password and compare it */
  if (!(a = atr_get_noparent(player, pword_attr)))
    return 1;                   /* No password attribute */

  saved = strdup(atr_value(a));

  if (!saved)
    return 0;

  passwd = mush_crypt(password);

  if (strcmp(passwd, saved) != 0) {
    /* Nope. Try non-SHS. */
#ifdef HAS_CRYPT
    /* shs encryption didn't match. Try crypt(3) */
    if (strcmp(crypt(password, "XX"), saved) != 0)
      /* Nope */
#endif                          /* HAS_CRYPT */
      /* crypt() didn't work. Try plaintext, being sure to not
       * allow unencrypted entry of encrypted password */
      if ((strcmp(saved, password) != 0)
          || (strlen(password) < 4)
          || ((password[0] == 'X') && (password[1] == 'X'))) {
        /* Nothing worked. You lose. */
        free(saved);
        return 0;
      }
    /* Something worked. Change password to SHS-encrypted */
    (void) atr_add(player, pword_attr, passwd, GOD, 0);
  }
  free(saved);
  return 1;
}

/** Check to see if someone can connect to a player.
 * \param name name of player to connect to.
 * \param password password of player to connect to.
 * \param host host from which connection is being attempted.
 * \param ip ip address from which connection is being attempted.
 * \param errbuf buffer to return connection errors.
 * \return dbref of connected player object or NOTHING for failure
 * (with reason for failure returned in errbuf).
 */
dbref
connect_player(const char *name, const char *password, const char *host,
               const char *ip, char *errbuf)
{
  dbref player;

  /* Default error */
  strcpy(errbuf,
         T("Either that player does not exist, or has a different password."));

  if (!name || !*name)
    return NOTHING;

  /* validate name */
  if ((player = lookup_player(name)) == NOTHING)
    return NOTHING;

  /* See if player is allowed to connect like this */
  if (Going(player) || Going_Twice(player)) {
    do_log(LT_CONN, 0, 0,
           "Connection to GOING player %s not allowed from %s (%s)", name,
           host, ip);
    return NOTHING;
  }
  if (Guest(player)
      && (!Site_Can_Guest(host, player) || !Site_Can_Guest(ip, player))) {
    if (!Deny_Silent_Site(host, AMBIGUOUS) && !Deny_Silent_Site(ip, AMBIGUOUS)) {
      do_log(LT_CONN, 0, 0,
             "Connection to %s (GUEST) not allowed from %s (%s)", name,
             host, ip);
      strcpy(errbuf, T("Guest connections not allowed."));
    }
    return NOTHING;
  } else if (!Guest(player)
             && (!Site_Can_Connect(host, player)
                 || !Site_Can_Connect(ip, player))) {
    if (!Deny_Silent_Site(host, player) && !Deny_Silent_Site(ip, player)) {
      do_log(LT_CONN, 0, 0,
             "Connection to %s (Non-GUEST) not allowed from %s (%s)", name,
             host, ip);
      strcpy(errbuf, T("Player connections not allowed."));
    }
    return NOTHING;
  }
  /* validate password */
  if (!Guest(player))
    if (!password_check(player, password)) {
      /* Increment count of login failures */
      ModTime(player)++;
      check_lastfailed(player, host);
      return NOTHING;
    }
  /* If it's a Guest player, and already connected, search the
   * db for another Guest player to connect them to. */
  if (Guest(player)) {
    /* Enforce guest limit */
    player = guest_to_connect(player);
    if (!GoodObject(player)) {
      do_log(LT_CONN, 0, 0, "Can't connect to a guest (too many connected)");
      strcpy(errbuf, T("Too many guests are connected now."));
      return NOTHING;
    }
  }
  if (Suspect_Site(host, player) || Suspect_Site(ip, player)) {
    do_log(LT_CONN, 0, 0,
           "Connection from Suspect site. Setting %s(#%d) suspect.",
           Name(player), player);
    set_flag_internal(player, "SUSPECT");
  }
  return player;
}

/** Attempt to create a new player object.
 * \param name name of player to create.
 * \param password initial password of created player.
 * \param host host from which creation is attempted.
 * \param ip ip address from which creation is attempted.
 * \return dbref of created player, NOTHING if bad name, AMBIGUOUS if bad
 *  password.
 */
dbref
create_player(const char *name, const char *password, const char *host,
              const char *ip)
{
  if (!ok_player_name(name, NOTHING, NOTHING)) {
    do_log(LT_CONN, 0, 0, "Failed creation (bad name) from %s", host);
    return NOTHING;
  }
  if (!ok_password(password)) {
    do_log(LT_CONN, 0, 0, "Failed creation (bad password) from %s", host);
    return AMBIGUOUS;
  }
  if (DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
    /* Oops, out of db space! */
    do_log(LT_CONN, 0, 0, "Failed creation (no db space) from %s", host);
    return NOTHING;
  }
  /* else he doesn't already exist, create him */
  return make_player(name, password, host, ip);
}

/* The HAS_SENDMAIL ifdef is kept here as a hint to metaconfig */
#ifdef MAILER
#undef HAVE_SENDMAIL
#define HAVE_SENDMAIL 1
#undef SENDMAIL
#define SENDMAIL MAILER
#endif

#ifdef HAVE_SENDMAIL

/** Size of the elems array */
#define NELEMS (sizeof(elems)-1)

/** Attempt to register a new player at the connect screen.
 * If registration is allowed, a new player object is created with
 * a random password which is emailed to the registering player.
 * \param name name of player to register.
 * \param email email address to send registration details.
 * \param host host from which registration is being attempted.
 * \param ip ip address from which registration is being attempted.
 * \return dbref of created player or NOTHING if creation failed.
 */
dbref
email_register_player(const char *name, const char *email, const char *host,
                      const char *ip)
{
  char *p;
  char passwd[BUFFER_LEN];
  static char elems[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int i, len;
  dbref player;
  FILE *fp;

  if (!ok_player_name(name, NOTHING, NOTHING)) {
    do_log(LT_CONN, 0, 0, "Failed registration (bad name) from %s", host);
    return NOTHING;
  }
  /* Make sure that the email address is valid. A valid address must
   * contain either an @ or a !
   * Also, to prevent someone from using the MUSH to mailbomb another site,
   * let's make sure that the site to which the user wants the email
   * sent is also allowed to use the register command.
   * If there's an @, we check whatever's after the last @
   * (since @foo.bar:user@host is a valid email)
   * If not, we check whatever comes before the first !
   */
  if ((p = strrchr(email, '@'))) {
    p++;
    if (!Site_Can_Register(p)) {
      if (!Deny_Silent_Site(p, AMBIGUOUS)) {
        do_log(LT_CONN, 0, 0,
               "Failed registration (bad site in email: %s) from %s",
               email, host);
      }
      return NOTHING;
    }
  } else if ((p = strchr(email, '!'))) {
    *p = '\0';
    if (!Site_Can_Register(email)) {
      *p = '!';
      if (!Deny_Silent_Site(email, AMBIGUOUS)) {
        do_log(LT_CONN, 0, 0,
               "Failed registration (bad site in email: %s) from %s",
               email, host);
      }
      return NOTHING;
    } else
      *p = '!';
  } else {
    if (!Deny_Silent_Site(host, AMBIGUOUS)) {
      do_log(LT_CONN, 0, 0, "Failed registration (bad email: %s) from %s",
             email, host);
    }
    return NOTHING;
  }

  if (DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
    /* Oops, out of db space! */
    do_log(LT_CONN, 0, 0, "Failed registration (no db space) from %s", host);
    return NOTHING;
  }

  /* Come up with a random password of length 7-12 chars */
  len = get_random32(7, 12);
  for (i = 0; i < len; i++)
    passwd[i] = elems[get_random32(0, NELEMS - 1)];
  passwd[len] = '\0';

  /* If we've made it here, we can send the email and create the
   * character. Email first, since that's more likely to go bad.
   * Some security precautions we'll take:
   *  1) We'll use sendmail -t, so we don't pass user-given values to a shell.
   */

  release_fd();
  if ((fp =
#ifdef __LCC__
       (FILE *)
#endif
       popen(tprintf("%s -t", SENDMAIL), "w")) == NULL) {
    do_log(LT_CONN, 0, 0,
           "Failed registration of %s by %s: unable to open sendmail",
           name, email);
    reserve_fd();
    return NOTHING;
  }
  fprintf(fp, "Subject: ");
  fprintf(fp, T("[%s] Registration of %s\n"), MUDNAME, name);
  fprintf(fp, "To: %s\n", email);
  fprintf(fp, "Precedence: junk\n");
  fprintf(fp, "\n");
  fprintf(fp, T("This is an automated message.\n"));
  fprintf(fp, "\n");
  fprintf(fp, T("Your requested player, %s, has been created.\n"), name);
  fprintf(fp, T("The password is %s\n"), passwd);
  fprintf(fp, "\n");
  fprintf(fp, T("To access this character, connect to %s and type:\n"),
          MUDNAME);
  fprintf(fp, "\tconnect \"%s\" %s\n", name, passwd);
  fprintf(fp, "\n");
  pclose(fp);
  reserve_fd();
  /* Ok, all's well, make a player */
  player = make_player(name, passwd, host, ip);
  (void) atr_add(player, "REGISTERED_EMAIL", email, GOD, 0);
  return player;
}
#else
dbref
email_register_player(const char *name, const char *email, const char *host,
                      const char *ip __attribute__ ((__unused__)))
{
  do_log(LT_CONN, 0, 0, "Failed registration (no sendmail) from %s", host);
  do_log(LT_CONN, 0, 0, "Requested character: '%s'. Email address: %s\n",
         name, email);
  return NOTHING;
}
#endif                          /* !HAVE_SENDMAIL */

static dbref
make_player(const char *name, const char *password, const char *host,
            const char *ip)
{
  dbref player;
  char temp[SBUF_LEN];
  char *flaglist, *flagname;
  char flagbuff[BUFFER_LEN];

  player = new_object();

  /* initialize everything */
  set_name(player, name);
  Location(player) = PLAYER_START;
  Home(player) = PLAYER_START;
  Owner(player) = player;
  Parent(player) = NOTHING;
  Type(player) = TYPE_PLAYER;
  Flags(player) = new_flag_bitmask("FLAG");
  strcpy(flagbuff, options.player_flags);
  flaglist = trim_space_sep(flagbuff, ' ');
  if (*flaglist != '\0') {
    while (flaglist) {
      flagname = split_token(&flaglist, ' ');
      twiddle_flag_internal("FLAG", player, flagname, 0);
    }
  }
  if (Suspect_Site(host, player) || Suspect_Site(ip, player))
    set_flag_internal(player, "SUSPECT");
  set_initial_warnings(player);
  /* Modtime tracks login failures */
  ModTime(player) = (time_t) 0;
  (void) atr_add(player, "XYXXY", mush_crypt(password), GOD, 0);
  giveto(player, START_BONUS);  /* starting bonus */
  (void) atr_add(player, "LAST", show_time(mudtime, 0), GOD, 0);
  (void) atr_add(player, "LASTSITE", host, GOD, 0);
  (void) atr_add(player, "LASTIP", ip, GOD, 0);
  (void) atr_add(player, "LASTFAILED", " ", GOD, 0);
  sprintf(temp, "%d", START_QUOTA);
  (void) atr_add(player, "RQUOTA", temp, GOD, 0);
  (void) atr_add(player, "ICLOC", EMPTY_ATTRS ? "" : " ", GOD,
                 AF_MDARK | AF_PRIVATE | AF_WIZARD | AF_NOCOPY);
  (void) atr_add(player, "MAILCURF", "0", GOD,
                 AF_LOCKED | AF_NOPROG | AF_WIZARD);
  add_folder_name(player, 0, "inbox");
  /* link him to PLAYER_START */
  PUSH(player, Contents(PLAYER_START));

  add_player(player);
  add_lock(GOD, player, Basic_Lock, parse_boolexp(player, "=me", Basic_Lock),
           LF_DEFAULT);
  add_lock(GOD, player, Enter_Lock, parse_boolexp(player, "=me", Basic_Lock),
           LF_DEFAULT);
  add_lock(GOD, player, Use_Lock, parse_boolexp(player, "=me", Basic_Lock),
           LF_DEFAULT);

  current_state.players++;

  local_data_create(player);

  return player;
}


/** Change a player's password.
 * \verbatim
 * This function implements @password.
 * \endverbatim
 * \param player the executor.
 * \param cause the enactor.
 * \param old player's current password.
 * \param newobj player's desired new password.
 */
void
do_password(dbref player, dbref cause, const char *old, const char *newobj)
{
  if (!global_eval_context.process_command_port) {
    char old_eval[BUFFER_LEN];
    char new_eval[BUFFER_LEN];
    char const *sp;
    char *bp;

    sp = old;
    bp = old_eval;
    process_expression(old_eval, &bp, &sp, player, player, cause,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';
    old = old_eval;

    sp = newobj;
    bp = new_eval;
    process_expression(new_eval, &bp, &sp, player, player, cause,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';
    newobj = new_eval;
  }

  if (!password_check(player, old)) {
    notify(player, T("The old password that you entered was incorrect."));
  } else if (!ok_password(newobj)) {
    notify(player, T("Bad new password."));
  } else {
    (void) atr_add(player, "XYXXY", mush_crypt(newobj), GOD, 0);
    notify(player, T("You have changed your password."));
  }
}

/** Processing related to players' last connections.
 * Here we check to see if a player gets a paycheck, tell them their
 * last connection site, and update all their LAST* attributes.
 * \param player dbref of player.
 * \param host hostname of player's current connection.
 * \param ip ip address of player's current connection.
 */
void
check_last(dbref player, const char *host, const char *ip)
{
  char *s;
  ATTR *a;
  ATTR *h;
  char last_time[MAX_COMMAND_LEN / 8];
  char last_place[MAX_COMMAND_LEN];

  /* compare to last connect see if player gets salary */
  s = show_time(mudtime, 0);
  a = atr_get_noparent(player, "LAST");
  if (a && (strncmp(atr_value(a), s, 10) != 0))
    giveto(player, Paycheck(player));
  /* tell the player where he last connected from */
  if (!Guest(player)) {
    h = atr_get_noparent(player, "LASTSITE");
    if (h && a) {
      strcpy(last_place, atr_value(h));
      strcpy(last_time, atr_value(a));
      notify_format(player, T("Last connect was from %s on %s."),
                    last_place, last_time);
    }
    /* How about last failed connection */
    h = atr_get_noparent(player, "LASTFAILED");
    if (h && a) {
      strcpy(last_place, atr_value(h));
      if (strlen(last_place) > 2)
        notify_format(player, T("Last FAILED connect was from %s."),
                      last_place);
    }
  }
  /* if there is no Lastsite, then the player is newly created.
   * the extra variables are a kludge to work around some weird
   * behavior involving uncompress.
   */

  /* set the new attributes */
  (void) atr_add(player, "LAST", s, GOD, 0);
  (void) atr_add(player, "LASTSITE", host, GOD, 0);
  (void) atr_add(player, "LASTIP", ip, GOD, 0);
  (void) atr_add(player, "LASTFAILED", " ", GOD, 0);
}


/** Update the LASTFAILED attribute on a failed connection.
 * \param player dbref of player.
 * \param host host from which connection attempt failed.
 */
void
check_lastfailed(dbref player, const char *host)
{
  char last_place[BUFFER_LEN], *bp;

  bp = last_place;
  safe_format(last_place, &bp, T("%s on %s"), host, show_time(mudtime, 0));
  *bp = '\0';
  (void) atr_add(player, "LASTFAILED", last_place, GOD, 0);
}
