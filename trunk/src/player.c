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
#include "game.h"

#ifdef HAS_CRYPT
#ifdef I_CRYPT
#include <crypt.h>
#else
extern char *crypt(const char *, const char *);
#endif
#endif

#include "extmail.h"
#include "confmagic.h"


/* From mycrypt.c */
char *mush_crypt_sha0(const char *key);
char *password_hash(const char *key, const char *algo);
bool password_comp(const char *saved, const char *pass);

dbref email_register_player
  (DESC *d, const char *name, const char *email, const char *host,
   const char *ip);
static dbref make_player(const char *name, const char *password,
                         const char *host, const char *ip);

static const char pword_attr[] = "XYXXY";

extern struct db_stat_info current_state;

/* Long IP, just for ipv6.
 *
 * Pure ipv6: 8*4+7=39.
 * ipv6+ipv4 tunneling: 45.
 * +1 for null, rounded up to nearest multiple of 4.
 */
#define IP_LENGTH 48
/* How many failures to keep track of. */
#define FAIL_COUNT 100

/** For tracking failed login attempts by IPs */
typedef struct _fail_info {
  char ip[IP_LENGTH];     /**< The failed IP. Extra long, just for ipv6. */
  time_t failTime;        /**< The time of the last failed login attempt */
} Fail_Info;

/* This is a rotating buffer of the FAIL_COUNT most recent failures. */
static Fail_Info ipFails[FAIL_COUNT];
/* IPFAIL(x) counts x items _BACKWARDS_ from current position. */

static int failIdx = 0;
#define IPFAIL(x) ipFails[((failIdx + FAIL_COUNT) - x) % FAIL_COUNT]

static int failCount = 0;
const char *connect_fail_limit_exceeded =
  "This IP address has failed too many times. Please try again in 10 minutes.";

/** Check if the given IP has had too many failures to be allowed
 * to log in.
 * \param ipaddr The IP address to check.
 * \retval 1 Okay to log in.
 * \retval 0 Do not allow to log in.
 */
int
check_fails(const char *ipaddr)
{
  int i;
  int numFails = 0;
  time_t since = time(NULL) - 600;

  /* A connect_fail_limit of 0 means none. */
  if (!CONNECT_FAIL_LIMIT)
    return 1;

  for (i = 0; i < failCount; i++) {
    if (IPFAIL(i).failTime < since) {
      break;
    }
    if (!strncmp(ipaddr, IPFAIL(i).ip, IP_LENGTH)) {
      numFails++;
      if (numFails >= CONNECT_FAIL_LIMIT) {
        return 0;
      }
    }
  }
  return 1;
}

int
count_failed(const char *ipaddr)
{
  int i, numFails;
  time_t since = time(NULL) - 600;

  numFails = 0;
  for (i = 0; i < failCount; i++) {
    if (IPFAIL(i).failTime < since) {
      break;
    }
    if (!strncmp(ipaddr, IPFAIL(i).ip, IP_LENGTH)) {
      numFails++;
    }
  }
  return numFails;
}

/** Mark the given IP as a failure.
 * \param ipaddr The IP address to check.
 * \retval The # of fails the IP has had in the past 10 minutes.
 */
int
mark_failed(const char *ipaddr)
{
  failIdx++;
  failIdx %= FAIL_COUNT;

  if (failCount < FAIL_COUNT) {
    failCount++;
  }
  strncpy(IPFAIL(0).ip, ipaddr, IP_LENGTH);
  IPFAIL(0).failTime = time(NULL);

  return count_failed(ipaddr);
}


/** Check a player's password against a given string.
 *
 *  First checks new-style formatted password strings
 *  If that doesn't match, tries old-style SHA0 password
 *  strings, and upgrades the stored password.
 *  If that doesn't match, tries really-old-style crypt(3)
 *  password strings, and upgrades the stored password.
 *  If that doesn't work, you lose.
 *
 * \param player dbref of player.
 * \param password plaintext password string to check.
 * \retval 1 password matches (or player has no password).
 * \retval 0 password fails to match.
 */
bool
password_check(dbref player, const char *password)
{
  ATTR *a;
  char *saved;

  /* read the password and compare it */
  if (!(a = atr_get_noparent(player, pword_attr)))
    return 1;                   /* No password attribute */

  saved = strdup(atr_value(a));

  if (!saved)
    return 0;

  if (!password_comp(saved, password)) {
    /* Nope. Try SHA0. */
    char *passwd = mush_crypt_sha0(password);
    if (strcmp(saved, passwd) != 0) {
      /* Not SHA0 either. Try old-school crypt(); */
#ifdef HAS_CRYPT
      if (strcmp(crypt(password, "XX"), saved) != 0) {
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
#ifdef HAS_CRYPT
      }
#endif
    }
    /* Something worked. Change password to SHS-encrypted */
    do_rawlog(LT_CONN, "Updating password format for player #%d", player);
    (void) atr_add(player, pword_attr, password_hash(password, NULL), GOD, 0);
  }
  /* Success! */
  free(saved);
  return 1;
}

/** Check to see if someone can connect to a player.
 * \param d DESC the connect attempt is being made for
 * \param name name of player to connect to.
 * \param password password of player to connect to.
 * \param host host from which connection is being attempted.
 * \param ip ip address from which connection is being attempted.
 * \param errbuf buffer to return connection errors.
 * \return dbref of connected player object or NOTHING for failure
 * (with reason for failure returned in errbuf).
 */
dbref
connect_player(DESC *d, const char *name, const char *password,
               const char *host, const char *ip, char *errbuf)
{
  dbref player;
  int count;

  /* Default error */
  strcpy(errbuf,
         T("Either that player does not exist, or has a different password."));

  if (!name || !*name)
    return NOTHING;

  /* validate name */
  if ((player = lookup_player(name)) == NOTHING) {
    /* Invalid player names are failures, too. */
    count = mark_failed(ip);
    queue_event(SYSEVENT, "SOCKET`LOGINFAIL", "%d,%s,%d,%s,#%d,%s",
                d->descriptor, ip, count, "invalid player", -1, name);
    return NOTHING;
  }

  /* See if player is allowed to connect like this */
  if (Going(player) || Going_Twice(player)) {
    do_log(LT_CONN, 0, 0,
           "Connection to GOING player %s not allowed from %s (%s)",
           Name(player), host, ip);
    queue_event(SYSEVENT, "SOCKET`LOGINFAIL", "%d,%s,%d,%s,#%d", d->descriptor,
                ip, count_failed(ip), "player is going", player);
    return NOTHING;
  }
  /* Check sitelock patterns */
  if (Guest(player)
      && (!Site_Can_Guest(host, player) || !Site_Can_Guest(ip, player))) {
    if (!Deny_Silent_Site(host, AMBIGUOUS) && !Deny_Silent_Site(ip, AMBIGUOUS)) {
      do_log(LT_CONN, 0, 0,
             "Connection to %s (GUEST) not allowed from %s (%s)", name,
             host, ip);
      strcpy(errbuf, T("Guest connections not allowed."));
      count = mark_failed(ip);
      queue_event(SYSEVENT, "SOCKET`LOGINFAIL", "%d,%s,%d,%s,#%d",
                  d->descriptor, ip, count, "failed sitelock", player);
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
      count = mark_failed(ip);
      queue_event(SYSEVENT, "SOCKET`LOGINFAIL", "%d,%s,%d,%s,#%d",
                  d->descriptor, ip, count, "failed sitelock", player);
    }
    return NOTHING;
  }
  /* validate password */
  if (!Guest(player))
    if (!password_check(player, password)) {
      /* Increment count of login failures */
      ModTime(player)++;
      check_lastfailed(player, host);
      count = mark_failed(ip);
      queue_event(SYSEVENT, "SOCKET`LOGINFAIL", "%d,%s,%d,%s,#%d",
                  d->descriptor, ip, count, "invalid password", player);
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
      queue_event(SYSEVENT, "SOCKET`LOGINFAIL", "%d,%s,%d,%s,#%d",
                  d->descriptor, ip, count_failed(ip), "too many guests",
                  player);
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
 * \param d DESC the creation attempt is being made on (if from connect screen)
 * \dbref executor dbref of the object attempting to create a player (if \@pcreate)
 * \param name name of player to create.
 * \param password initial password of created player.
 * \param host host from which creation is attempted.
 * \param ip ip address from which creation is attempted.
 * \return dbref of created player, NOTHING if bad name, AMBIGUOUS if bad
 *  password.
 */
dbref
create_player(DESC *d, dbref executor, const char *name, const char *password,
              const char *host, const char *ip)
{
  if (!ok_player_name(name, executor, NOTHING)) {
    do_log(LT_CONN, 0, 0, "Failed creation (bad name) from %s", host);
    if (d) {
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, ip, mark_failed(ip), "create: bad name", name);
    }
    return NOTHING;
  }
  if (!ok_password(password)) {
    do_log(LT_CONN, 0, 0, "Failed creation (bad password) from %s", host);
    if (d) {
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, ip, mark_failed(ip),
                  "create: bad password", name);
    }
    return AMBIGUOUS;
  }
  if (DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
    /* Oops, out of db space! */
    do_log(LT_CONN, 0, 0, "Failed creation (no db space) from %s", host);
    if (d) {
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, ip, mark_failed(ip),
                  "create: no db space left to create!", name);
    }
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
email_register_player(DESC *d, const char *name, const char *email,
                      const char *host, const char *ip)
{
  char *p;
  char passwd[BUFFER_LEN];
  static char elems[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int i, len;
  int resend = 0;
  dbref player = NOTHING;
  FILE *fp;

  if (!check_fails(ip)) {
    return NOTHING;
  }

  if (!ok_player_name(name, NOTHING, NOTHING)) {
    /* Check for re-registration request */
    player = lookup_player(name);
    if (GoodObject(player)) {
      ATTR *a;
      a = atr_get(player, "LASTLOGOUT");
      if (!a) {
        a = atr_get(player, "REGISTERED_EMAIL");
        if (a && !strcasecmp(atr_value(a), email))
          resend = 1;
      }
    }
    if (!resend) {
      do_log(LT_CONN, 0, 0, "Failed registration (bad name) from %s", host);
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, ip, mark_failed(ip), "register: bad name",
                  name);
      return NOTHING;
    }
  }
  if (!resend) {
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
          queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                      d->descriptor, ip, mark_failed(ip),
                      "register: bad site in email", name);
        }
        return NOTHING;
      }
    } else if ((p = strchr(email, '!'))) {
      *p = '\0';
      if (!Site_Can_Register(email)) {
        if (!Deny_Silent_Site(email, AMBIGUOUS)) {
          *p = '!';
          do_log(LT_CONN, 0, 0,
                 "Failed registration (bad site in email: %s) from %s",
                 email, host);
          queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                      d->descriptor, ip, mark_failed(ip),
                      "register: bad site in email", name);
        }
        return NOTHING;
      } else {
        *p = '!';
      }
    } else {
      if (!Deny_Silent_Site(host, AMBIGUOUS)) {
        do_log(LT_CONN, 0, 0, "Failed registration (bad email: %s) from %s",
               email, host);
        queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                    d->descriptor, ip, mark_failed(ip),
                    "register: sitelocked host", name);
      }
      return NOTHING;
    }

    if (DBTOP_MAX && (db_top >= DBTOP_MAX + 1) && (first_free == NOTHING)) {
      /* Oops, out of db space! */
      do_log(LT_CONN, 0, 0, "Failed registration (no db space) from %s", host);
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, ip, count_failed(ip),
                  "register: no db space left to create!", name);
      return NOTHING;
    }
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
   *  2) We'll cross our fingers and hope nobody uses this to spam.
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
    queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                d->descriptor, ip, count_failed(ip),
                "register: Unable to open sendmail!", name);
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
  if (resend) {
    (void) atr_add(player, pword_attr, password_hash(passwd, NULL), GOD, 0);
    return player;
  } else {
    player = make_player(name, passwd, host, ip);
    queue_event(SYSEVENT, "PLAYER`CREATE", "%s,%s,%s,%d",
                unparse_objid(player), name, "register", d->descriptor);
    (void) atr_add(player, "REGISTERED_EMAIL", email, GOD, 0);
    return player;
  }
}
#else
dbref
email_register_player(DESC *d
                      __attribute__ ((__unused__)), const char *name,
                      const char *email, const char *host, const char *ip
                      __attribute__ ((__unused__)))
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
  (void) atr_add(player, pword_attr, password_hash(password, NULL), GOD, 0);
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
 * \param executor the executor.
 * \param enactor the enactor.
 * \param old player's current password.
 * \param newobj player's desired new password.
 * \param queue_entry the queue entry \@password is being executed in
 */
void
do_password(dbref executor, dbref enactor, const char *old, const char *newobj,
            MQUE *queue_entry)
{
  if (!queue_entry->port) {
    char old_eval[BUFFER_LEN];
    char new_eval[BUFFER_LEN];
    char const *sp;
    char *bp;

    sp = old;
    bp = old_eval;
    if (process_expression(old_eval, &bp, &sp, executor, executor, enactor,
                           PE_DEFAULT, PT_DEFAULT, NULL))
      return;
    *bp = '\0';
    old = old_eval;

    sp = newobj;
    bp = new_eval;
    if (process_expression(new_eval, &bp, &sp, executor, executor, enactor,
                           PE_DEFAULT, PT_DEFAULT, NULL))
      return;
    *bp = '\0';
    newobj = new_eval;
  }

  if (!password_check(executor, old)) {
    notify(executor, T("The old password that you entered was incorrect."));
  } else if (!ok_password(newobj)) {
    notify(executor, T("Bad new password."));
  } else {
    (void) atr_add(executor, pword_attr, password_hash(newobj, NULL), GOD, 0);
    notify(executor, T("You have changed your password."));
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
