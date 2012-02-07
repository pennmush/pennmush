/**
 * \file extchat.c
 *
 * \brief The PennMUSH chat system
 *
 *
 */
#include "copyrite.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <stdarg.h>
#include "conf.h"
#include "externs.h"
#include "attrib.h"
#include "mushdb.h"
#include "match.h"
#include "flags.h"
#include "extchat.h"
#include "ansi.h"
#include "privtab.h"
#include "mymalloc.h"
#include "pueblo.h"
#include "parse.h"
#include "lock.h"
#include "log.h"
#include "game.h"
#include "dbdefs.h"
#include "function.h"
#include "command.h"
#include "dbio.h"
#include "intmap.h"
#include "confmagic.h"


static CHAN *new_channel(void);
static CHANLIST *new_chanlist(const void *hint);
static CHANUSER *new_user(dbref who, const void *hint);
static void free_channel(CHAN *c);
static void free_chanlist(CHANLIST *cl);
static void free_user(CHANUSER *u);
static int load_chatdb_oldstyle(PENNFILE *fp);
static int load_channel(PENNFILE *fp, CHAN *ch);
static int load_chanusers(PENNFILE *fp, CHAN *ch);
static int load_labeled_channel(PENNFILE *fp, CHAN *ch, int dbflags);
static int load_labeled_chanusers(PENNFILE *fp, CHAN *ch);
static void insert_channel(CHAN **ch);
static void remove_channel(CHAN *ch);
static void insert_obj_chan(dbref who, CHAN **ch);
static void remove_obj_chan(dbref who, CHAN *ch);
void remove_all_obj_chan(dbref thing);
static void chan_chown(CHAN *c, dbref victim);
void chan_chownall(dbref old, dbref newowner);
static int insert_user(CHANUSER *user, CHAN *ch);
static int remove_user(CHANUSER *u, CHAN *ch);
static int save_channel(PENNFILE *fp, CHAN *ch);
static int save_chanuser(PENNFILE *fp, CHANUSER *user);
static void channel_wipe(dbref player, CHAN *chan);
static int yesno(const char *str);
static int canstilladd(dbref player);
static enum cmatch_type find_channel_partial_on(const char *name, CHAN **chan,
                                                dbref player);
static enum cmatch_type find_channel_partial_off(const char *name, CHAN **chan,
                                                 dbref player);
static char *list_cuflags(CHANUSER *u, int verbose);
static void channel_join_self(dbref player, const char *name);
static void channel_leave_self(dbref player, const char *name);
static void do_channel_who(dbref player, CHAN *chan);
void chat_player_announce(dbref player, char *msg, int ungag);
enum ok_name { NAME_OK = 0, NAME_INVALID, NAME_TOO_LONG, NAME_NOT_UNIQUE };
static enum ok_name ok_channel_name(const char *n, CHAN *unique);
static void channel_send(CHAN *channel, dbref player, int flags,
                         const char *origmessage);
static void list_partial_matches(dbref player, const char *name,
                                 enum chan_match_type type);

const char *chan_speak_lock = "ChanSpeakLock";  /**< Name of speak lock */
const char *chan_join_lock = "ChanJoinLock";    /**< Name of join lock */
const char *chan_mod_lock = "ChanModLock";      /**< Name of modify lock */
const char *chan_see_lock = "ChanSeeLock";      /**< Name of see lock */
const char *chan_hide_lock = "ChanHideLock";    /**< Name of hide lock */

slab *chanlist_slab; /**< slab for 'struct chanlist' allocations */
slab *chanuser_slab; /**< slab for 'struct chanuser' allocations */

#define YES 1     /**< An affirmative. */
#define NO 0      /**< A negative. */
#define ERR -1    /**< An error. Clever, eh? */

/** Wrapper for insert_user() that generates a new CHANUSER and inserts it */
#define insert_user_by_dbref(who,chan) \
  insert_user(new_user(who, ChanUsers(chan)),chan)
/** Wrapper for remove_user() that searches for the CHANUSER to remove */
#define remove_user_by_dbref(who,chan) \
        remove_user(onchannel(who,chan),chan)

int num_channels;  /**< Number of channels defined */

CHAN *channels;    /**< Pointer to channel list */

extern int rhs_present;         /* from command.c */

/* Player must come before Admin and Wizard, otherwise @chan/what
 * will fail to report when Admin channels are also set Player */
static PRIV priv_table[] = {
  {"Disabled", 'D', CHANNEL_DISABLED, CHANNEL_DISABLED},
  {"Player", 'P', CHANNEL_PLAYER, CHANNEL_PLAYER},
  {"Admin", 'A', CHANNEL_ADMIN | CHANNEL_PLAYER, CHANNEL_ADMIN},
  {"Wizard", 'W', CHANNEL_WIZARD | CHANNEL_PLAYER, CHANNEL_WIZARD},
  {"Thing", 'T', CHANNEL_OBJECT, CHANNEL_OBJECT},
  {"Object", 'O', CHANNEL_OBJECT, CHANNEL_OBJECT},
  {"Quiet", 'Q', CHANNEL_QUIET, CHANNEL_QUIET},
  {"Open", 'o', CHANNEL_OPEN, CHANNEL_OPEN},
  {"Hide_Ok", 'H', CHANNEL_CANHIDE, CHANNEL_CANHIDE},
  {"NoTitles", 'T', CHANNEL_NOTITLES, CHANNEL_NOTITLES},
  {"NoNames", 'N', CHANNEL_NONAMES, CHANNEL_NONAMES},
  {"NoCemit", 'C', CHANNEL_NOCEMIT, CHANNEL_NOCEMIT},
  {"Interact", 'I', CHANNEL_INTERACT, CHANNEL_INTERACT},
  {NULL, '\0', 0, 0}
};

static PRIV chanuser_priv[] = {
  {"Quiet", 'Q', CU_QUIET, CU_QUIET},
  {"Hide", 'H', CU_HIDE, CU_HIDE},
  {"Gag", 'G', CU_GAG, CU_GAG},
  {"Combine", 'C', CU_COMBINE, CU_COMBINE},
  {NULL, '\0', 0, 0}
};


/** Get a player's CHANUSER entry if they're on a channel.
 * This function checks to see if a given player is on a given channel.
 * If so, it returns a pointer to their CHANUSER structure. If not,
 * returns NULL.
 * \param who player to test channel membership of.
 * \param ch pointer to channel to test membership on.
 * \return player's CHANUSER entry on the channel, or NULL.
 */
CHANUSER *
onchannel(dbref who, CHAN *ch)
{
  static CHANUSER *u;
  for (u = ChanUsers(ch); u; u = u->next) {
    if (CUdbref(u) == who) {
      return u;
    }
  }
  return NULL;
}


/** A macro to test if a channel exists and, if not, to notify. */
#define test_channel_fun(player,name,chan,buff,bp) \
   do { \
    chan = NULL; \
    switch (find_channel(name,&chan,player)) { \
    case CMATCH_NONE: \
      notify(player, T ("CHAT: I don't recognize that channel.")); \
      if (buff) \
        safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp); \
      return; \
    case CMATCH_AMBIG: \
      notify(player, T("CHAT: I don't know which channel you mean.")); \
      list_partial_matches(player, name, PMATCH_ALL); \
      if (buff) \
        safe_str(T("#-2 AMBIGUOUS CHANNEL"), buff, bp); \
      return; \
    case CMATCH_EXACT: \
    case CMATCH_PARTIAL: \
    default: \
      break; \
     } \
    } while (0)

#define test_channel(player,name,chan) test_channel_fun(player,name,chan,NULL,NULL);


/** A macro to test if a channel exists and player's on it, and,
 * if not, to notify. */
#define test_channel_on(player,name,chan) \
   do { \
    chan = NULL; \
    switch (find_channel_partial_on(name,&chan,player)) { \
    case CMATCH_NONE: \
      notify(player, T ("CHAT: I don't recognize that channel.")); \
      return; \
    case CMATCH_AMBIG: \
      notify(player, T("CHAT: I don't know which channel you mean.")); \
      list_partial_matches(player, name, PMATCH_ALL); \
      return; \
    case CMATCH_EXACT: \
    case CMATCH_PARTIAL: \
    default: \
      break; \
     } \
    } while (0)

/*----------------------------------------------------------
 * Loading and saving the chatdb
 * The chatdb's format is pretty straightforward
 * Return 1 on success, 0 on failure
 */

/** Initialize the chat database .*/
void
init_chatdb(void)
{
  static bool init_called = 0;
  if (!init_called) {
    init_called = 1;
    num_channels = 0;
    chanuser_slab = slab_create("channel users", sizeof(struct chanuser));
    chanlist_slab = slab_create("channel lists", sizeof(struct chanlist));
    slab_set_opt(chanuser_slab, SLAB_ALLOC_BEST_FIT, 1);
    slab_set_opt(chanlist_slab, SLAB_ALLOC_BEST_FIT, 1);
    channels = NULL;
  }
}

/** Load the chat database from a file.
 * \param fp pointer to file to read from.
 * \retval 1 success
 * \retval 0 failure
 */
static int
load_chatdb_oldstyle(PENNFILE *fp)
{
  int i;
  CHAN *ch;
  char buff[20];

  /* How many channels? */
  num_channels = getref(fp);
  if (num_channels > MAX_CHANNELS)
    return 0;

  /* Load all channels */
  for (i = 0; i < num_channels; i++) {
    if (penn_feof(fp))
      break;
    ch = new_channel();
    if (!ch)
      return 0;
    if (!load_channel(fp, ch)) {
      do_rawlog(LT_ERR, "Unable to load channel %d.", i);
      free_channel(ch);
      return 0;
    }
    insert_channel(&ch);
  }
  num_channels = i;

  /* Check for **END OF DUMP*** */
  if (!penn_fgets(buff, sizeof buff, fp))
    do_rawlog(LT_ERR, "CHAT: No end-of-dump marker in the chat database.");
  else if (strcmp(buff, EOD) != 0)
    do_rawlog(LT_ERR, "CHAT: Trailing garbage in the chat database.");

  return 1;
}

extern char db_timestamp[];

/** Load the chat database from a file.
 * \param fp pointer to file to read from.
 * \retval 1 success
 * \retval 0 failure
 */
int
load_chatdb(PENNFILE *fp)
{
  int i, flags;
  CHAN *ch;
  char buff[20];
  char *chat_timestamp;

  i = penn_fgetc(fp);
  if (i == EOF) {
    do_rawlog(LT_ERR, "CHAT: Invalid database format!");
    longjmp(db_err, 1);
  } else if (i != '+') {
    penn_ungetc(i, fp);
    return load_chatdb_oldstyle(fp);
  }

  i = penn_fgetc(fp);

  if (i != 'V') {
    do_rawlog(LT_ERR, "CHAT: Invalid database format!");
    longjmp(db_err, 1);
  }

  flags = getref(fp);

  db_read_this_labeled_string(fp, "savedtime", &chat_timestamp);

  if (strcmp(chat_timestamp, db_timestamp))
    do_rawlog(LT_ERR,
              "CHAT: warning: chatdb and game db were saved at different times!");

  /* How many channels? */
  db_read_this_labeled_int(fp, "channels", &num_channels);
  if (num_channels > MAX_CHANNELS) {
    do_rawlog(LT_ERR,
              "CHAT: Too many channels in chatdb (there are %d, max is %d)",
              num_channels, MAX_CHANNELS);
    return 0;
  }

  /* Load all channels */
  for (i = 0; i < num_channels; i++) {
    ch = new_channel();
    if (!ch) {
      do_rawlog(LT_ERR, "CHAT: Unable to allocate memory for channel %d!", i);
      return 0;
    }
    if (!load_labeled_channel(fp, ch, flags)) {
      do_rawlog(LT_ERR, "Unable to load channel %d.", i);
      free_channel(ch);
      return 0;
    }
    insert_channel(&ch);
  }
  num_channels = i;

  /* Check for **END OF DUMP*** */
  if (!penn_fgets(buff, sizeof buff, fp))
    do_rawlog(LT_ERR, "CHAT: No end-of-dump marker in the chat database.");
  else if (strcmp(buff, EOD) != 0)
    do_rawlog(LT_ERR, "CHAT: Trailing garbage in the chat database.");

  return 1;
}


/* Malloc memory for a new channel, and initialize it */
static CHAN *
new_channel(void)
{
  CHAN *ch;

  ch = mush_malloc(sizeof *ch, "channel");
  if (!ch)
    return NULL;
  ch->name = NULL;
  ch->title[0] = '\0';
  ChanType(ch) = CHANNEL_DEFAULT_FLAGS;
  ChanCreator(ch) = NOTHING;
  ChanMogrifier(ch) = NOTHING;
  ChanCost(ch) = CHANNEL_COST;
  ChanNext(ch) = NULL;
  ChanNumMsgs(ch) = 0;
  /* By default channels are public but mod-lock'd to the creator */
  ChanJoinLock(ch) = TRUE_BOOLEXP;
  ChanSpeakLock(ch) = TRUE_BOOLEXP;
  ChanSeeLock(ch) = TRUE_BOOLEXP;
  ChanHideLock(ch) = TRUE_BOOLEXP;
  ChanModLock(ch) = TRUE_BOOLEXP;
  ChanNumUsers(ch) = 0;
  ChanMaxUsers(ch) = 0;
  ChanUsers(ch) = NULL;
  ChanBufferQ(ch) = NULL;
  return ch;
}



/* Malloc memory for a new user, and initialize it */
static CHANUSER *
new_user(dbref who, const void *hint)
{
  CHANUSER *u;
  u = slab_malloc(chanuser_slab, hint);
  if (!u)
    mush_panic("Couldn't allocate memory in new_user in extchat.c");
  CUdbref(u) = who;
  CUtype(u) = CU_DEFAULT_FLAGS;
  CUtitle(u) = NULL;
  CUnext(u) = NULL;
  return u;
}

/* Free memory from a channel */
static void
free_channel(CHAN *c)
{
  CHANUSER *u, *unext;
  if (!c)
    return;
  free_boolexp(ChanJoinLock(c));
  free_boolexp(ChanSpeakLock(c));
  free_boolexp(ChanHideLock(c));
  free_boolexp(ChanSeeLock(c));
  free_boolexp(ChanModLock(c));
  if (ChanName(c))
    mush_free(ChanName(c), "channel.name");
  u = ChanUsers(c);
  while (u) {
    unext = u->next;
    free_user(u);
    u = unext;
  }
  mush_free(c, "channel");
  return;
}

/* Free memory from a channel user */
static void
free_user(CHANUSER *u)
{
  if (u)
    slab_free(chanuser_slab, u);
}

/* Load in a single channel into position i. Return 1 if
 * successful, 0 otherwise.
 */
static int
load_channel(PENNFILE *fp, CHAN *ch)
{
  ChanName(ch) = mush_strdup(getstring_noalloc(fp), "channel.name");
  if (penn_feof(fp))
    return 0;
  mush_strncpy(ChanTitle(ch), getstring_noalloc(fp), CHAN_TITLE_LEN);
  ChanType(ch) = (privbits) getref(fp);
  ChanCreator(ch) = getref(fp);
  ChanMogrifier(ch) = NOTHING;
  ChanCost(ch) = getref(fp);
  ChanNumMsgs(ch) = 0;
  ChanJoinLock(ch) = getboolexp(fp, chan_join_lock);
  ChanSpeakLock(ch) = getboolexp(fp, chan_speak_lock);
  ChanModLock(ch) = getboolexp(fp, chan_mod_lock);
  ChanSeeLock(ch) = getboolexp(fp, chan_see_lock);
  ChanHideLock(ch) = getboolexp(fp, chan_hide_lock);
  ChanNumUsers(ch) = getref(fp);
  ChanMaxUsers(ch) = ChanNumUsers(ch);
  ChanUsers(ch) = NULL;
  if (ChanNumUsers(ch) > 0)
    ChanNumUsers(ch) = load_chanusers(fp, ch);
  return 1;
}

/* Load in a single channel into position i. Return 1 if
 * successful, 0 otherwise.
 */
static int
load_labeled_channel(PENNFILE *fp, CHAN *ch, int dbflags)
{
  char *tmp;
  int i;
  dbref d;
  char *label, *value;

  db_read_this_labeled_string(fp, "name", &tmp);
  ChanName(ch) = mush_strdup(tmp, "channel.name");
  db_read_this_labeled_string(fp, "description", &tmp);
  mush_strncpy(ChanTitle(ch), tmp, CHAN_TITLE_LEN);
  db_read_this_labeled_int(fp, "flags", &i);
  ChanType(ch) = (privbits) i;
  db_read_this_labeled_dbref(fp, "creator", &d);
  ChanCreator(ch) = d;
  db_read_this_labeled_int(fp, "cost", &i);
  ChanCost(ch) = i;
  if (dbflags & CDB_SPIFFY) {
    db_read_this_labeled_int(fp, "buffer", &i);
    if (i)
      ChanBufferQ(ch) = allocate_bufferq(i);
    db_read_this_labeled_dbref(fp, "mogrifier", &d);
    ChanMogrifier(ch) = d;
  }
  ChanNumMsgs(ch) = 0;
  while (1) {
    db_read_labeled_string(fp, &label, &value);
    if (strcmp(label, "lock"))
      break;
    else if (strcmp(value, "join") == 0)
      ChanJoinLock(ch) = getboolexp(fp, chan_join_lock);
    else if (strcmp(value, "speak") == 0)
      ChanSpeakLock(ch) = getboolexp(fp, chan_speak_lock);
    else if (strcmp(value, "modify") == 0)
      ChanModLock(ch) = getboolexp(fp, chan_mod_lock);
    else if (strcmp(value, "see") == 0)
      ChanSeeLock(ch) = getboolexp(fp, chan_see_lock);
    else if (strcmp(value, "hide") == 0)
      ChanHideLock(ch) = getboolexp(fp, chan_hide_lock);
  }
  ChanNumUsers(ch) = parse_integer(value);
  ChanMaxUsers(ch) = ChanNumUsers(ch);
  ChanUsers(ch) = NULL;
  if (ChanNumUsers(ch) > 0)
    ChanNumUsers(ch) = load_labeled_chanusers(fp, ch);
  return 1;
}


/* Load the *channel's user list. Return number of users on success, or 0 */
static int
load_chanusers(PENNFILE *fp, CHAN *ch)
{
  int i, num = 0;
  CHANUSER *user;
  dbref player;
  char title[BUFFER_LEN];
  for (i = 0; i < ChanNumUsers(ch); i++) {
    player = getref(fp);
    /* Don't bother if the player isn't a valid dbref or the wrong type */
    if (GoodObject(player) && Chan_Ok_Type(ch, player)) {
      user = new_user(player, ChanUsers(ch));
      CUtype(user) = getref(fp);
      strncpy(title, getstring_noalloc(fp), BUFFER_LEN - 1);
      if (*title)
        CUtitle(user) = mush_strdup(title, "chan_user.title");
      else
        CUtitle(user) = NULL;
      CUnext(user) = NULL;
      if (insert_user(user, ch))
        num++;
    } else {
      /* But be sure to read (and discard) the player's info */
      do_log(LT_ERR, 0, 0, "Bad object #%d removed from channel %s",
             player, ChanName(ch));
      (void) getref(fp);
      (void) getstring_noalloc(fp);
    }
  }
  return num;
}

/* Load the *channel's user list. Return number of users on success, or 0 */
static int
load_labeled_chanusers(PENNFILE *fp, CHAN *ch)
{
  int i, num = 0, n;
  char *tmp;
  CHANUSER *user;
  dbref player;
  for (i = ChanNumUsers(ch); i > 0; i--) {
    db_read_this_labeled_dbref(fp, "dbref", &player);
    /* Don't bother if the player isn't a valid dbref or the wrong type */
    if (GoodObject(player) && Chan_Ok_Type(ch, player)) {
      user = new_user(player, ChanUsers(ch));
      db_read_this_labeled_int(fp, "flags", &n);
      CUtype(user) = n;
      db_read_this_labeled_string(fp, "title", &tmp);
      if (tmp && *tmp)
        CUtitle(user) = mush_strdup(tmp, "chan_user.title");
      else
        CUtitle(user) = NULL;
      CUnext(user) = NULL;
      if (insert_user(user, ch))
        num++;
    } else {
      /* But be sure to read (and discard) the player's info */
      do_log(LT_ERR, 0, 0, "Bad object #%d removed from channel %s",
             player, ChanName(ch));
      db_read_this_labeled_int(fp, "flags", &n);
      db_read_this_labeled_string(fp, "title", &tmp);
      ChanNumUsers(ch) -= 1;
    }
  }
  return num;
}


/* Insert the channel onto the list of channels, sorted by name */
static void
insert_channel(CHAN **ch)
{
  CHAN *p;
  char cleanname[CHAN_NAME_LEN];
  char cleanp[CHAN_NAME_LEN];

  if (!ch || !*ch)
    return;

  /* If there's no channels on the list, or if the first channel is already
   * alphabetically greater, ch should be the first entry on the list */
  /* No channels? */
  if (!channels) {
    channels = *ch;
    channels->next = NULL;
    return;
  }
  p = channels;
  /* First channel? */
  strcpy(cleanp, remove_markup(ChanName(p), NULL));
  strcpy(cleanname, remove_markup(ChanName(*ch), NULL));
  if (strcasecoll(cleanp, cleanname) > 0) {
    channels = *ch;
    channels->next = p;
    return;
  }
  /* Otherwise, find which user this user should be inserted after */
  while (p->next) {
    strcpy(cleanp, remove_markup(ChanName(p->next), NULL));
    if (strcasecoll(cleanp, cleanname) > 0)
      break;
    p = p->next;
  }
  (*ch)->next = p->next;
  p->next = *ch;
  return;
}

/* Remove a channel from the list, but don't free it */
static void
remove_channel(CHAN *ch)
{
  CHAN *p;

  if (!ch)
    return;
  if (!channels)
    return;
  if (channels == ch) {
    /* First channel */
    channels = ch->next;
    return;
  }
  /* Otherwise, find the channel before this one */
  for (p = channels; p->next && (p->next != ch); p = p->next) ;

  if (p->next) {
    p->next = ch->next;
  }
  return;
}

/* Insert the channel onto the list of channels on a given object,
 * sorted by name
 */
static void
insert_obj_chan(dbref who, CHAN **ch)
{
  CHANLIST *p;
  CHANLIST *tmp;
  char cleanname[CHAN_NAME_LEN];
  char cleanp[CHAN_NAME_LEN];

  if (!ch || !*ch)
    return;

  tmp = new_chanlist(Chanlist(who));
  if (!tmp)
    return;
  tmp->chan = *ch;
  strcpy(cleanname, remove_markup(ChanName(*ch), NULL));
  /* If there's no channels on the list, or if the first channel is already
   * alphabetically greater, chan should be the first entry on the list */
  /* No channels? */
  if (!Chanlist(who)) {
    tmp->next = NULL;
    s_Chanlist(who, tmp);
    return;
  }
  p = Chanlist(who);
  /* First channel? */
  strcpy(cleanp, remove_markup(ChanName(p->chan), NULL));
  if (strcasecoll(cleanp, cleanname) > 0) {
    tmp->next = p;
    s_Chanlist(who, tmp);
    return;
  } else if (!strcasecmp(cleanp, cleanname)) {
    /* Don't add the same channel twice! */
    free_chanlist(tmp);
  } else {
    /* Otherwise, find which channel this channel should be inserted after */
    while (p->next) {
      strcpy(cleanp, remove_markup(ChanName(p->next->chan), NULL));
      if (strcasecoll(cleanp, cleanname) >= 0)
        break;
      p = p->next;
    }
    if (p->next && !strcasecmp(cleanp, cleanname)) {
      /* Don't add the same channel twice! */
      free_chanlist(tmp);
    } else {
      tmp->next = p->next;
      p->next = tmp;
    }
  }
  return;
}

/* Remove a channel from the obj's chanlist, and free the chanlist ptr */
static void
remove_obj_chan(dbref who, CHAN *ch)
{
  CHANLIST *p, *q;

  if (!ch)
    return;
  p = Chanlist(who);
  if (!p)
    return;
  if (p->chan == ch) {
    /* First channel */
    s_Chanlist(who, p->next);
    free_chanlist(p);
    return;
  }
  /* Otherwise, find the channel before this one */
  for (; p->next && (p->next->chan != ch); p = p->next) ;

  if (p->next) {
    q = p->next;
    p->next = p->next->next;
    free_chanlist(q);
  }
  return;
}

/** Remove all channels from the obj's chanlist, freeing them.
 * \param thing object to have channels removed from.
 */
void
remove_all_obj_chan(dbref thing)
{
  CHANLIST *p, *nextp;
  for (p = Chanlist(thing); p; p = nextp) {
    nextp = p->next;
    if (ChanMogrifier(p->chan) == thing) {
      ChanMogrifier(p->chan) = NOTHING;
    }
    remove_user_by_dbref(thing, p->chan);
  }
  return;
}


static CHANLIST *
new_chanlist(const void *hint)
{
  CHANLIST *c;
  c = slab_malloc(chanlist_slab, hint);
  if (!c)
    return NULL;
  c->chan = NULL;
  c->next = NULL;
  return c;
}

static void
free_chanlist(CHANLIST *cl)
{
  slab_free(chanlist_slab, cl);
}


/* Insert the user onto the channel's list, sorted by the user's name */
static int
insert_user(CHANUSER *user, CHAN *ch)
{
  CHANUSER *p;

  if (!user || !ch)
    return 0;

  /* If there's no users on the list, or if the first user is already
   * alphabetically greater, user should be the first entry on the list */
  p = ChanUsers(ch);
  if (!p || (strcasecoll(Name(CUdbref(p)), Name(CUdbref(user))) > 0)) {
    user->next = ChanUsers(ch);
    ChanUsers(ch) = user;
  } else {
    /* Otherwise, find which user this user should be inserted after */
    for (;
         p->next
         && (strcasecoll(Name(CUdbref(p->next)), Name(CUdbref(user))) <= 0);
         p = p->next) ;
    if (CUdbref(p) == CUdbref(user)) {
      /* Don't add the same user twice! */
      slab_free(chanuser_slab, user);
      return 0;
    } else {
      user->next = p->next;
      p->next = user;
    }
  }
  insert_obj_chan(CUdbref(user), &ch);
  return 1;
}

/* Remove a user from a channel list, and free it */
static int
remove_user(CHANUSER *u, CHAN *ch)
{
  CHANUSER *p;
  dbref who;

  if (!ch || !u)
    return 0;
  p = ChanUsers(ch);
  if (!p)
    return 0;
  who = CUdbref(u);
  if (p == u) {
    /* First user */
    ChanUsers(ch) = p->next;
    free_user(u);
  } else {
    /* Otherwise, find the user before this one */
    for (; p->next && (p->next != u); p = p->next) ;

    if (p->next) {
      p->next = u->next;
      free_user(u);
    } else
      return 0;
  }

  /* Now remove the channel from the user's chanlist */
  remove_obj_chan(who, ch);
  ChanNumUsers(ch)--;
  return 1;
}


/** Write the chat database to disk.
 * \param fp pointer to file to write to.
 * \retval 1 success
 * \retval 0 failure
 */
int
save_chatdb(PENNFILE *fp)
{
  CHAN *ch;
  int default_flags = 0;
  default_flags += CDB_SPIFFY;

  /* How many channels? */
  penn_fprintf(fp, "+V%d\n", default_flags);
  db_write_labeled_string(fp, "savedtime", show_time(mudtime, 1));
  db_write_labeled_int(fp, "channels", num_channels);
  for (ch = channels; ch; ch = ch->next) {
    save_channel(fp, ch);
  }
  penn_fputs(EOD, fp);
  return 1;
}

/* Save a single channel. Return 1 if  successful, 0 otherwise.
 */
static int
save_channel(PENNFILE *fp, CHAN *ch)
{
  CHANUSER *cu;

  db_write_labeled_string(fp, " name", ChanName(ch));
  db_write_labeled_string(fp, "  description", ChanTitle(ch));
  db_write_labeled_int(fp, "  flags", ChanType(ch));
  db_write_labeled_dbref(fp, "  creator", ChanCreator(ch));
  db_write_labeled_int(fp, "  cost", ChanCost(ch));
  db_write_labeled_int(fp, "  buffer", bufferq_blocks(ChanBufferQ(ch)));
  db_write_labeled_dbref(fp, "  mogrifier", ChanMogrifier(ch));
  db_write_labeled_string(fp, "  lock", "join");
  putboolexp(fp, ChanJoinLock(ch));
  db_write_labeled_string(fp, "  lock", "speak");
  putboolexp(fp, ChanSpeakLock(ch));
  db_write_labeled_string(fp, "  lock", "modify");
  putboolexp(fp, ChanModLock(ch));
  db_write_labeled_string(fp, "  lock", "see");
  putboolexp(fp, ChanSeeLock(ch));
  db_write_labeled_string(fp, "  lock", "hide");
  putboolexp(fp, ChanHideLock(ch));
  db_write_labeled_int(fp, "  users", ChanNumUsers(ch));
  for (cu = ChanUsers(ch); cu; cu = cu->next)
    save_chanuser(fp, cu);
  return 1;
}

/* Save the channel's user list. Return 1 on success, 0 on failure */
static int
save_chanuser(PENNFILE *fp, CHANUSER *user)
{
  db_write_labeled_dbref(fp, "   dbref", CUdbref(user));
  db_write_labeled_int(fp, "    flags", CUtype(user));
  if (CUtitle(user))
    db_write_labeled_string(fp, "    title", CUtitle(user));
  else
    db_write_labeled_string(fp, "    title", "");
  return 1;
}

/*-------------------------------------------------------------*
 * Some utility functions:
 *  find_channel - given a name and a player, return a channel
 *  find_channel_partial - given a name and a player, return
 *    the first channel that matches name
 *  find_channel_partial_on - given a name and a player, return
 *    the first channel that matches name that player is on.
 *  onchannel - is player on channel?
 */

/** Removes markup and <>'s in channel names.
 * \param name The name to normalize.
 * \retval a pointer to a static buffer with the normalized name.
 */
static char *
normalize_channel_name(const char *name)
{
  static char cleanname[BUFFER_LEN];
  size_t len;

  cleanname[0] = '\0';

  if (!name || !*name)
    return cleanname;

  strcpy(cleanname, remove_markup(name, &len));
  len--;

  if (!*cleanname)
    return cleanname;

  if (cleanname[0] == '<' && cleanname[len - 1] == '>') {
    cleanname[len - 1] = '\0';
    return cleanname + 1;
  } else
    return cleanname;
}

/** Attempt to match a channel name for a player.
 * Given name and a chan pointer, set chan pointer to point to
 * channel if found (NULL otherwise), and return an indication
 * of how good the match was. If the player is not able to see
 * the channel, fail to match.
 * \param name name of channel to find.
 * \param chan pointer to address of channel structure to return.
 * \param player dbref to use for permission checks.
 * \retval CMATCH_EXACT exact match of channel name.
 * \retval CMATCH_PARTIAL partial match of channel name.
 * \retval CMATCH_AMBIG multiple match of channel name.
 * \retval CMATCH_NONE no match for channel name.
 */
enum cmatch_type
find_channel(const char *name, CHAN **chan, dbref player)
{
  CHAN *p;
  int count = 0;
  char *cleanname;
  char cleanp[CHAN_NAME_LEN];

  *chan = NULL;
  if (!name || !*name)
    return CMATCH_NONE;

  cleanname = normalize_channel_name(name);
  for (p = channels; p; p = p->next) {
    strcpy(cleanp, remove_markup(ChanName(p), NULL));
    if (!strcasecmp(cleanname, cleanp)) {
      *chan = p;
      if (Chan_Can_See(*chan, player) || onchannel(player, *chan))
        return CMATCH_EXACT;
      else
        return CMATCH_NONE;
    }
    if (string_prefix(cleanp, name)) {
      /* Keep the alphabetically first channel if we've got one */
      if (Chan_Can_See(p, player) || onchannel(player, p)) {
        if (!*chan)
          *chan = p;
        count++;
      }
    }
  }
  switch (count) {
  case 0:
    return CMATCH_NONE;
  case 1:
    return CMATCH_PARTIAL;
  }
  return CMATCH_AMBIG;
}


/** Attempt to match a channel name for a player.
 * Given name and a chan pointer, set chan pointer to point to
 * channel if found (NULL otherwise), and return an indication
 * of how good the match was. If the player is not able to see
 * the channel, fail to match. If the match is ambiguous, return
 * the first channel matched.
 * \param name name of channel to find.
 * \param chan pointer to address of channel structure to return.
 * \param player dbref to use for permission checks.
 * \retval CMATCH_EXACT exact match of channel name.
 * \retval CMATCH_PARTIAL partial match of channel name.
 * \retval CMATCH_AMBIG multiple match of channel name.
 * \retval CMATCH_NONE no match for channel name.
 */
enum cmatch_type
find_channel_partial(const char *name, CHAN **chan, dbref player)
{
  CHAN *p;
  int count = 0;
  char *cleanname;
  char cleanp[CHAN_NAME_LEN];

  *chan = NULL;
  if (!name || !*name)
    return CMATCH_NONE;
  cleanname = normalize_channel_name(name);
  for (p = channels; p; p = p->next) {
    strcpy(cleanp, remove_markup(ChanName(p), NULL));
    if (!strcasecmp(cleanname, cleanp)) {
      *chan = p;
      return CMATCH_EXACT;
    }
    if (string_prefix(cleanp, cleanname)) {
      /* If we've already found an ambiguous match that the
       * player is on, keep using that one. Otherwise, this is
       * our best candidate so far.
       */
      if (!*chan || (!onchannel(player, *chan) && onchannel(player, p)))
        *chan = p;
      count++;
    }
  }
  switch (count) {
  case 0:
    return CMATCH_NONE;
  case 1:
    return CMATCH_PARTIAL;
  }
  return CMATCH_AMBIG;
}

static void
list_partial_matches(dbref player, const char *name, enum chan_match_type type)
{
  CHAN *p;
  char *cleanname;
  char cleanp[CHAN_NAME_LEN];
  char buff[BUFFER_LEN], *bp;
  bp = buff;

  if (!name || !*name)
    return;

  safe_str(T("CHAT: Partial matches are:"), buff, &bp);
  cleanname = normalize_channel_name(name);
  for (p = channels; p; p = p->next) {
    if (!Chan_Can_See(p, player))
      continue;
    if ((type == PMATCH_ALL) || ((type == PMATCH_ON)
                                 ? ! !onchannel(player, p)
                                 : !onchannel(player, p))) {
      strcpy(cleanp, remove_markup(ChanName(p), NULL));
      if (string_prefix(cleanp, cleanname)) {
        safe_chr(' ', buff, &bp);
        safe_str(ChanName(p), buff, &bp);
      }
    }
  }

  safe_chr('\0', buff, &bp);
  notify(player, buff);

}




/** Attempt to match a channel name for a player.
 * Given name and a chan pointer, set chan pointer to point to
 * channel if found and player is on the channel (NULL otherwise),
 * and return an indication of how good the match was. If the player is
 * not able to see the channel, fail to match. If the match is ambiguous,
 * return the first channel matched.
 * \param name name of channel to find.
 * \param chan pointer to address of channel structure to return.
 * \param player dbref to use for permission checks.
 * \retval CMATCH_EXACT exact match of channel name.
 * \retval CMATCH_PARTIAL partial match of channel name.
 * \retval CMATCH_AMBIG multiple match of channel name.
 * \retval CMATCH_NONE no match for channel name.
 */
static enum cmatch_type
find_channel_partial_on(const char *name, CHAN **chan, dbref player)
{
  CHAN *p;
  int count = 0;
  char *cleanname;
  char cleanp[CHAN_NAME_LEN];

  *chan = NULL;
  if (!name || !*name)
    return CMATCH_NONE;
  cleanname = normalize_channel_name(name);
  for (p = channels; p; p = p->next) {
    if (onchannel(player, p)) {
      strcpy(cleanp, remove_markup(ChanName(p), NULL));
      if (!strcasecmp(cleanname, cleanp)) {
        *chan = p;
        return CMATCH_EXACT;
      }
      if (string_prefix(cleanp, cleanname) && onchannel(player, p)) {
        if (!*chan)
          *chan = p;
        count++;
      }
    }
  }
  switch (count) {
  case 0:
    return CMATCH_NONE;
  case 1:
    return CMATCH_PARTIAL;
  }
  return CMATCH_AMBIG;
}

/** Attempt to match a channel name for a player.
 * Given name and a chan pointer, set chan pointer to point to
 * channel if found and player is NOT on the channel (NULL otherwise),
 * and return an indication of how good the match was. If the player is
 * not able to see the channel, fail to match. If the match is ambiguous,
 * return the first channel matched.
 * \param name name of channel to find.
 * \param chan pointer to address of channel structure to return.
 * \param player dbref to use for permission checks.
 * \retval CMATCH_EXACT exact match of channel name.
 * \retval CMATCH_PARTIAL partial match of channel name.
 * \retval CMATCH_AMBIG multiple match of channel name.
 * \retval CMATCH_NONE no match for channel name.
 */
static enum cmatch_type
find_channel_partial_off(const char *name, CHAN **chan, dbref player)
{
  CHAN *p;
  int count = 0;
  char *cleanname;
  char cleanp[CHAN_NAME_LEN];

  *chan = NULL;
  if (!name || !*name)
    return CMATCH_NONE;
  cleanname = normalize_channel_name(name);
  for (p = channels; p; p = p->next) {
    if (!onchannel(player, p)) {
      strcpy(cleanp, remove_markup(ChanName(p), NULL));
      if (!strcasecmp(cleanname, cleanp)) {
        *chan = p;
        return CMATCH_EXACT;
      }
      if (string_prefix(cleanp, cleanname)) {
        if (!*chan)
          *chan = p;
        count++;
      }
    }
  }
  switch (count) {
  case 0:
    return CMATCH_NONE;
  case 1:
    return CMATCH_PARTIAL;
  }
  return CMATCH_AMBIG;
}

/*--------------------------------------------------------------*
 * User commands:
 *  do_channel - @channel/on,off,who
 *  do_chan_admin - @channel/add,delete,name,priv,quiet
 *  do_chan_desc
 *  do_chan_title
 *  do_chan_lock
 *  do_chan_boot
 *  do_chan_wipe
 */

/** User interface to channels.
 * \verbatim
 * This is one of the top-level functions for @channel.
 * It handles the /on, /off and /who switches. It also
 * parses and handles the older @channel <channel>=<command>
 * format, for the on, off, who, and wipe commands.
 * \endverbatim
 * \param player the enactor.
 * \param name name of channel.
 * \param target name of player to add/remove (NULL for self)
 * \param com channel command switch.
 */
void
do_channel(dbref player, const char *name, const char *target, const char *com)
{
  CHAN *chan = NULL;
  CHANUSER *u;
  dbref victim;

  if (!name || !*name) {
    notify(player, T("You need to specify a channel."));
    return;
  }
  if (!com || !*com) {
    notify(player, T("What do you want to do with the channel?"));
    return;
  }

  /* Quickly catch two common cases and handle separately */
  if (!target || !*target) {
    if (!strcasecmp(com, "on") || !strcasecmp(com, "join")) {
      channel_join_self(player, name);
      return;
    } else if (!strcasecmp(com, "off") || !strcasecmp(com, "leave")) {
      channel_leave_self(player, name);
      return;
    }
  }

  test_channel(player, name, chan);
  if (!Chan_Can_See(chan, player)) {
    if (onchannel(player, chan))
      notify_format(player,
                    T("CHAT: You can't do that with channel <%s>."),
                    ChanName(chan));
    else
      notify(player, T("CHAT: I don't recognize that channel."));
    return;
  }
  if (!strcasecmp(com, "who")) {
    do_channel_who(player, chan);
    return;
  } else if (!strcasecmp(com, "wipe")) {
    channel_wipe(player, chan);
    return;
  }
  /* It's on or off now */
  /* Determine who is getting added or deleted. If we don't have
   * an argument, we return, because we should've caught those above,
   * and this shouldn't happen.
   */
  if (!target || !*target) {
    notify(player, T("I don't understand what you want to do."));
    return;
  }

  victim = lookup_player(target);
  if (victim == NOTHING && Channel_Object(chan))
    victim = match_result(player, target, TYPE_THING, MAT_OBJECTS);

  if (!GoodObject(victim)) {
    notify(player, T("Invalid target."));
    return;
  }
  if (!strcasecmp("on", com) || !strcasecmp("join", com)) {
    if (!Chan_Ok_Type(chan, victim)) {
      notify_format(player,
                    T("Sorry, wrong type of thing for channel <%s>."),
                    ChanName(chan));
      return;
    }
    if (Guest(player)) {
      notify(player, T("Guests are not allowed to join channels."));
      return;
    }
    if (!controls(player, victim)) {
      notify(player, T("Invalid target."));
      return;
    }
    /* Is victim already on the channel? */
    if (onchannel(victim, chan)) {
      notify_format(player,
                    T("%s is already on channel <%s>."), Name(victim),
                    ChanName(chan));
      return;
    }
    /* Does victim pass the joinlock? */
    if (!Chan_Can_Join(chan, victim)) {
      if (Wizard(player)) {
        /* Wizards can override join locks */
        notify(player,
               T
               ("CHAT: Warning: Target does not meet channel join permissions! (joining anyway)"));
      } else {
        notify(player, T("Permission to join denied."));
        return;
      }
    }
    if (insert_user_by_dbref(victim, chan)) {
      notify_format(victim,
                    T("CHAT: %s joins you to channel <%s>."), Name(player),
                    ChanName(chan));
      notify_format(player,
                    T("CHAT: You join %s to channel <%s>."), Name(victim),
                    ChanName(chan));
      onchannel(victim, chan);
      ChanNumUsers(chan)++;
      if (!Channel_Quiet(chan) && !DarkLegal(victim)) {
        channel_send(chan, victim,
                     CB_CHECKQUIET | CB_PRESENCE | CB_POSE,
                     T("has joined this channel."));
      }
    } else {
      notify_format(player,
                    T("%s is already on channel <%s>."), Name(victim),
                    ChanName(chan));
    }
    return;
  } else if (!strcasecmp("off", com) || !strcasecmp("leave", com)) {
    /* You must control either the victim or the channel */
    if (!controls(player, victim) && !Chan_Can_Modify(chan, player)) {
      notify(player, T("Invalid target."));
      return;
    }
    if (Guest(player)) {
      notify(player, T("Guests may not leave channels."));
      return;
    }
    u = onchannel(victim, chan);
    if (remove_user(u, chan)) {
      if (!Channel_Quiet(chan) && !DarkLegal(victim)) {
        channel_send(chan, victim,
                     CB_CHECKQUIET | CB_PRESENCE | CB_POSE,
                     T("has left this channel."));
      }
      notify_format(victim,
                    T("CHAT: %s removes you from channel <%s>."),
                    Name(player), ChanName(chan));
      notify_format(player,
                    T("CHAT: You remove %s from channel <%s>."),
                    Name(victim), ChanName(chan));
    } else {
      notify_format(player, T("%s is not on channel <%s>."), Name(victim),
                    ChanName(chan));
    }
    return;
  } else {
    notify(player, T("I don't understand what you want to do."));
    return;
  }
}

static void
channel_join_self(dbref player, const char *name)
{
  CHAN *chan = NULL;

  if (Guest(player)) {
    notify(player, T("Guests are not allowed to join channels."));
    return;
  }

  switch (find_channel_partial_off(name, &chan, player)) {
  case CMATCH_NONE:
    if (find_channel_partial_on(name, &chan, player))
      notify_format(player, T("CHAT: You are already on channel <%s>."),
                    ChanName(chan));
    else
      notify(player, T("CHAT: I don't recognize that channel."));
    return;
  case CMATCH_AMBIG:
    notify(player, T("CHAT: I don't know which channel you mean."));
    list_partial_matches(player, name, PMATCH_OFF);
    return;
  default:
    break;
  }
  if (!Chan_Can_See(chan, player)) {
    notify(player, T("CHAT: I don't recognize that channel."));
    return;
  }
  if (!Chan_Ok_Type(chan, player)) {
    notify_format(player,
                  T("Sorry, wrong type of thing for channel <%s>."),
                  ChanName(chan));
    return;
  }
  /* Does victim pass the joinlock? */
  if (!Chan_Can_Join(chan, player)) {
    if (Wizard(player)) {
      /* Wizards can override join locks */
      notify(player,
             T
             ("CHAT: Warning: You don't meet channel join permissions! (joining anyway)"));
    } else {
      notify(player, T("Permission to join denied."));
      return;
    }
  }
  if (insert_user_by_dbref(player, chan)) {
    notify_format(player, T("CHAT: You join channel <%s>."), ChanName(chan));
    onchannel(player, chan);
    ChanNumUsers(chan)++;
    if (!Channel_Quiet(chan) && !DarkLegal(player))
      channel_send(chan, player,
                   CB_CHECKQUIET | CB_PRESENCE | CB_POSE,
                   T("has joined this channel."));
  } else {
    /* Should never happen */
    notify_format(player,
                  T("%s is already on channel <%s>."), Name(player),
                  ChanName(chan));
  }
}

static void
channel_leave_self(dbref player, const char *name)
{
  CHAN *chan = NULL;
  CHANUSER *u;

  if (Guest(player)) {
    notify(player, T("Guests are not allowed to leave channels."));
    return;
  }
  switch (find_channel_partial_on(name, &chan, player)) {
  case CMATCH_NONE:
    if (find_channel_partial_off(name, &chan, player)
        && Chan_Can_See(chan, player))
      notify_format(player, T("CHAT: You are not on channel <%s>."),
                    ChanName(chan));
    else
      notify(player, T("CHAT: I don't recognize that channel."));
    return;
  case CMATCH_AMBIG:
    notify(player, T("CHAT: I don't know which channel you mean."));
    list_partial_matches(player, name, PMATCH_ON);
    return;
  default:
    break;
  }
  u = onchannel(player, chan);
  if (remove_user(u, chan)) {
    if (!Channel_Quiet(chan) && !DarkLegal(player))
      channel_send(chan, player,
                   CB_CHECKQUIET | CB_PRESENCE | CB_POSE,
                   T("has left this channel."));
    notify_format(player, T("CHAT: You leave channel <%s>."), ChanName(chan));
  } else {
    /* Should never happen */
    notify_format(player, T("%s is not on channel <%s>."), Name(player),
                  ChanName(chan));
  }
}

/** Parse a chat token command, but don't chat with it.
 * \verbatim
 * This function hacks up something of the form "+<channel> <message>",
 * finding the two args, and returns 1 if the channel exists,
 * otherwise 0.
 * \endverbatim
 * \param player the enactor.
 * \param command the command to parse.
 * \retval 1 channel exists
 * \retval 0 chat failed (no such channel, etc.)
 */
int
parse_chat(dbref player, char *command)
{
  char *arg1;
  char *arg2;
  char *s;
  char ch;
  CHAN *c;

  s = command;
  arg1 = s;
  while (*s && !isspace((unsigned char) *s))
    s++;

  if (!*s)
    return 0;

  ch = *s;
  arg2 = s;
  *arg2++ = '\0';
  while (*arg2 && isspace((unsigned char) *arg2))
    arg2++;

  /* arg1 is channel name, arg2 is text. */
  switch (find_channel_partial_on(arg1, &c, player)) {
  case CMATCH_AMBIG:
  case CMATCH_EXACT:
  case CMATCH_PARTIAL:
    *s = '=';
    return 1;
  default:
    *s = ch;
    return 0;
  }
}


/** Chat on a channel, given its name.
 * \verbatim
 * This function parses a channel name and then calls do_chat()
 * to do the actual work of chatting. If it was called through
 * @chat, it fails noisily, but if it was called through +channel,
 * it fails silently so that the command can be matched against $commands.
 * \endverbatim
 * \param player the enactor.
 * \param name name of channel to speak on.
 * \param msg message to send to channel.
 * \param source 0 if from +channel, 1 if from the chat command.
 * \retval 1 successful chat.
 * \retval 0 failed chat.
 */
int
do_chat_by_name(dbref player, const char *name, const char *msg, int source)
{
  CHAN *c = NULL;
  enum cmatch_type res;
  if (!msg || !*msg) {
    if (source)
      notify(player, T("Don't you have anything to say?"));
    return 0;
  }
  /* First try to find a channel that the player's on. If that fails,
   * look for one that the player's not on.
   */
  res = find_channel_partial_on(name, &c, player);
  if (source && res == CMATCH_NONE) {
    /* Check for channels he's not on */
    res = find_channel_partial(name, &c, player);
  }
  switch (res) {
  case CMATCH_AMBIG:
    if (!ChanUseFirstMatch(player)) {
      notify(player, T("CHAT: I don't know which channel you mean."));
      list_partial_matches(player, name, PMATCH_ON);
      notify(player,
             T
             ("CHAT: You may wish to set the CHAN_USEFIRSTMATCH flag on yourself."));
      return 1;
    }
  case CMATCH_EXACT:
  case CMATCH_PARTIAL:
    do_chat(player, c, msg);
    return 1;
  case CMATCH_NONE:
    if (find_channel(name, &c, player) == CMATCH_NONE) {
      if (source)
        notify(player, T("CHAT: No such channel."));
      return 0;
    }
  }
  return 0;
}

/** Send a message to a channel.
 * This function does the real work of putting together a message
 * to send to a chat channel (which it then does via channel_broadcast()).
 * \param player the enactor.
 * \param chan pointer to the channel to speak on.
 * \param arg1 message to send.
 */
void
do_chat(dbref player, CHAN *chan, const char *arg1)
{
  CHANUSER *u;
  char type;
  bool canhear;

  if (!Chan_Ok_Type(chan, player)) {
    notify_format(player,
                  T
                  ("Sorry, you're not the right type to be on channel <%s>."),
                  ChanName(chan));
    return;
  }
  if (!Loud(player) && !Chan_Can_Speak(chan, player)) {
    if (Chan_Can_See(chan, player))
      notify_format(player,
                    T("Sorry, you're not allowed to speak on channel <%s>."),
                    ChanName(chan));
    else
      notify(player, T("No such channel."));
    return;
  }
  u = onchannel(player, chan);
  canhear = u ? !Chanuser_Gag(u) : 0;
  /* If the channel isn't open, you must hear it in order to speak */
  if (!Channel_Open(chan)) {
    if (!u) {
      notify(player, T("You must be on that channel to speak on it."));
      return;
    } else if (!canhear) {
      notify(player, T("You must stop gagging that channel to speak on it."));
      return;
    }
  }

  if (!*arg1) {
    notify(player, T("What do you want to say to that channel?"));
    return;
  }

  /* figure out what kind of message we have */
  type = ':';
  switch (*arg1) {
  case SEMI_POSE_TOKEN:
    type = ';';
    /* FALLTHRU */
  case POSE_TOKEN:
    arg1 = arg1 + 1;
    channel_send(chan, player, type == ';' ? CB_SEMIPOSE : CB_POSE, arg1);
    break;
  default:
    if (CHAT_STRIP_QUOTE && (*arg1 == SAY_TOKEN))
      arg1 = arg1 + 1;
    channel_send(chan, player, CB_SPEECH, arg1);
    break;
  }

  ChanNumMsgs(chan)++;
}

/** Emit on a channel.
 * \verbatim
 * This is the top-level function for @cemit.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the channel.
 * \param msg message to emit.
 * \param flags PEMIT_* flags.
 */
void
do_cemit(dbref player, const char *name, const char *msg, int flags)
{
  CHAN *chan = NULL;
  CHANUSER *u;
  int canhear;
  int override_checks = 0;

  if (!name || !*name) {
    notify(player, T("That is not a valid channel."));
    return;
  }
  switch (find_channel(name, &chan, player)) {
  case CMATCH_NONE:
    notify(player, T("I don't recognize that channel."));
    return;
  case CMATCH_AMBIG:
    notify(player, T("I don't know which channel you mean."));
    list_partial_matches(player, name, PMATCH_ALL);
    return;
  default:
    break;
  }
  if (!Chan_Can_See(chan, player)) {
    notify(player, T("CHAT: I don't recognize that channel."));
    return;
  }
  /* If the cemitter is both See_All and Pemit_All, always allow them
   * to @cemit, as they could iterate over connected players, examine
   * their channels, and pemit to them anyway.
   */
  if (See_All(player) && Pemit_All(player))
    override_checks = 1;
  if (!override_checks && !Chan_Ok_Type(chan, player)) {
    notify_format(player,
                  T
                  ("Sorry, you're not the right type to be on channel <%s>."),
                  ChanName(chan));
    return;
  }
  if (!override_checks && !Chan_Can_Cemit(chan, player)) {
    notify_format(player,
                  T("Sorry, you're not allowed to @cemit on channel <%s>."),
                  ChanName(chan));
    return;
  }
  u = onchannel(player, chan);
  canhear = u ? !Chanuser_Gag(u) : 0;
  /* If the channel isn't open, you must hear it in order to speak */
  if (!override_checks && !Channel_Open(chan)) {
    if (!u) {
      notify(player, T("You must be on that channel to speak on it."));
      return;
    } else if (!canhear) {
      notify(player, T("You must stop gagging that channel to speak on it."));
      return;
    }
  }

  if (!msg || !*msg) {
    notify(player, T("What do you want to emit?"));
    return;
  }
  if (!(flags & PEMIT_SILENT))
    channel_send(chan, player, CB_EMIT, msg);
  else
    channel_send(chan, player, CB_EMIT | CB_QUIET, msg);
  if (!canhear)
    notify_format(player, T("Cemit to channel %s: %s"), ChanName(chan), msg);
  ChanNumMsgs(chan)++;
  return;
}

/** Administrative channel commands.
 * \verbatim
 * This is one of top-level functions for @channel. This one handles
 * the /add, /delete, /rename, and /priv switches.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the channel.
 * \param perms the permissions to set on an added/priv'd channel, the newname for a renamed channel, or on/off for a quieted channel.
 * \param flag what to do with the channel.
 */
void
do_chan_admin(dbref player, char *name, const char *perms,
              enum chan_admin_op flag)
{
  CHAN *chan = NULL;
  privbits type;
  boolexp key;
  char old[BUFFER_LEN];
  char announcebuff[BUFFER_LEN];

  if (!name || !*name) {
    notify(player, T("You must specify a channel."));
    return;
  }
  if (Guest(player)) {
    notify(player, T("Guests may not modify channels."));
    return;
  }
  if ((flag > 1) && (!perms || !*perms)) {
    notify(player, T("What do you want to do with the channel?"));
    return;
  }
  /* Make sure we've got a unique channel name unless we're
   * adding a channel */
  if (flag)
    test_channel(player, name, chan);
  switch (flag) {
  case CH_ADMIN_ADD:
    /* add a channel */
    if (num_channels == MAX_CHANNELS) {
      notify(player, T("No more room for channels."));
      return;
    }
    switch (ok_channel_name(name, NULL)) {
    case NAME_INVALID:
      notify(player, T("Invalid name for a channel."));
      return;
    case NAME_TOO_LONG:
      notify(player, T("The channel needs a shorter name."));
      return;
    case NAME_NOT_UNIQUE:
      notify(player, T("The channel needs a more unique name."));
      return;
    case NAME_OK:
      break;
    }
    if (!Hasprivs(player) && !canstilladd(player)) {
      notify(player, T("You already own too many channels."));
      return;
    }
    /* get the permissions. Invalid specs default to the default */
    if (!perms || !*perms)
      type = string_to_privs(priv_table, options.channel_flags, 0);
    else
      type = string_to_privs(priv_table, perms, 0);
    if (!Chan_Can(player, type)) {
      notify(player, T("You can't create channels of that type."));
      return;
    }
    if (type & CHANNEL_DISABLED)
      notify(player, T("Warning: channel will be created disabled."));
    /* Can the player afford it? There's a cost */
    if (!payfor(Owner(player), CHANNEL_COST)) {
      notify_format(player, T("You can't afford the %d %s."), CHANNEL_COST,
                    MONIES);
      return;
    }
    /* Ok, let's do it */
    chan = new_channel();
    if (!chan) {
      notify(player, T("CHAT: No more memory for channels!"));
      giveto(Owner(player), CHANNEL_COST);
      return;
    }
    key = parse_boolexp(player, tprintf("=#%d", player), chan_mod_lock);
    if (!key) {
      mush_free(chan, "channel");
      notify(player, T("CHAT: No more memory for channels!"));
      giveto(Owner(player), CHANNEL_COST);
      return;
    }
    ChanModLock(chan) = key;
    num_channels++;
    if (type)
      ChanType(chan) = type;
    ChanCreator(chan) = Owner(player);
    ChanMogrifier(chan) = NOTHING;
    ChanName(chan) = mush_strdup(name, "channel.name");
    insert_channel(&chan);
    notify_format(player, T("CHAT: Channel <%s> created."), ChanName(chan));
    break;
  case CH_ADMIN_DEL:
    /* remove a channel */
    /* Check permissions. Wizards and owners can remove */
    if (!Chan_Can_Nuke(chan, player)) {
      notify(player, T("Permission denied."));
      return;
    }
    /* remove everyone from the channel */
    channel_wipe(player, chan);
    /* refund the owner's money */
    giveto(ChanCreator(chan), ChanCost(chan));
    /* zap the channel */
    remove_channel(chan);
    free_channel(chan);
    num_channels--;
    notify(player, T("Channel removed."));
    break;
  case CH_ADMIN_RENAME:
    /* rename a channel */
    /* Can the player do this? */
    if (!Chan_Can_Modify(chan, player)) {
      notify(player, T("Permission denied."));
      return;
    }
    switch (ok_channel_name(perms, chan)) {
    case NAME_INVALID:
      notify(player, T("Invalid name for a channel."));
      return;
    case NAME_TOO_LONG:
      notify(player, T("The channel needs a shorter name."));
      return;
    case NAME_NOT_UNIQUE:
      notify(player, T("The channel needs a more unique name."));
      return;
    case NAME_OK:
      break;
    }

    /* When we rename a channel, we actually remove it and re-insert it */
    strcpy(old, ChanName(chan));
    remove_channel(chan);
    if (ChanName(chan))
      mush_free(ChanName(chan), "channel.name");
    ChanName(chan) = mush_strdup(perms, "channel.name");
    insert_channel(&chan);
    snprintf(announcebuff, BUFFER_LEN, T("has renamed %s to %s."),
             old, ChanName(chan));
    channel_send(chan, player,
                 CB_CHECKQUIET | CB_PRESENCE | CB_POSE, announcebuff);
    notify(player, T("Channel renamed."));
    break;
  case CH_ADMIN_PRIV:
    /* change the permissions on a channel */
    if (!Chan_Can_Modify(chan, player)) {
      notify(player, T("Permission denied."));
      return;
    }
    /* get the permissions. Invalid specs default to no change */
    type = string_to_privs(priv_table, perms, ChanType(chan));
    if (!Chan_Can_Priv(player, type)) {
      notify(player, T("You can't make channels that type."));
      return;
    }
    if (type & CHANNEL_DISABLED)
      notify(player, T("Warning: channel will be disabled."));
    if (type == ChanType(chan)) {
      notify_format(player,
                    T
                    ("Invalid or same permissions on channel <%s>. No changes made."),
                    ChanName(chan));
    } else {
      ChanType(chan) = type;
      notify_format(player,
                    T("Permissions on channel <%s> changed."), ChanName(chan));
    }
    break;
  }
}

static enum ok_name
ok_channel_name(const char *n, CHAN *unique)
{
  /* is name valid for a channel? */
  const char *p;
  char name[BUFFER_LEN];
  CHAN *check;
  int res;

  if (!n || !*n)
    return NAME_INVALID;

  mush_strncpy(name, remove_markup(n, NULL), BUFFER_LEN);

  /* No leading spaces */
  if (isspace((unsigned char) *name))
    return NAME_INVALID;

  /* only printable characters */
  for (p = name; p && *p; p++) {
    if (!isprint((unsigned char) *p) || *p == '|')
      return NAME_INVALID;
  }

  /* No trailing spaces */
  p--;
  if (isspace((unsigned char) *p))
    return NAME_INVALID;

  if (strlen(name) > CHAN_NAME_LEN - 1)
    return NAME_TOO_LONG;

  res = find_channel(name, &check, GOD);
  if (res != CMATCH_NONE) {
    if (unique == NULL) {
      return NAME_NOT_UNIQUE;   /* Name must be totally unique */
    } else if (check != unique) {
      return NAME_NOT_UNIQUE;
    }
  }

  return NAME_OK;
}


/** Modify a user's settings on a channel.
 * \verbatim
 * This is one of top-level functions for @channel. This one
 * handles the /mute, /hide, and /gag switches, which control an
 * individual user's settings on a single channel.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the channel.
 * \param isyn a yes/no string.
 * \param flag CU_* option
 * \param silent if 1, no notification of actions.
 */
void
do_chan_user_flags(dbref player, char *name, const char *isyn, int flag,
                   int silent)
{
  CHAN *c = NULL;
  CHANUSER *u;
  CHANLIST *p;
  int setting = abs(yesno(isyn));
  p = NULL;

  if (!IsPlayer(player) && flag == CU_COMBINE) {
    notify(player, T("Only players can use that option."));
    return;
  }

  if (!name || !*name) {
    p = Chanlist(player);
    if (!p) {
      notify(player, T("You are not on any channels."));
      return;
    }
    silent = 1;
    switch (flag) {
    case CU_QUIET:
      notify(player, setting ? T("All channels have been muted.")
             : T("All channels have been unmuted."));
      break;
    case CU_HIDE:
      notify(player, setting ? T("You hide on all the channels you can.")
             : T("You unhide on all channels."));
      break;
    case CU_GAG:
      notify(player, setting ? T("All channels have been gagged.")
             : T("All channels have been ungagged."));
      break;
    case CU_COMBINE:
      notify(player, setting ? T("All channels have been combined.")
             : T("All channels have been uncombined."));
      break;
    }
  } else {
    test_channel_on(player, name, c);
  }

  /* channel loop */
  do {
    /* If we have a channel list at the start,
     * that means they didn't gave us a channel name,
     * so we now figure out c. */
    if (p != NULL) {
      c = p->chan;
      p = p->next;
    }

    u = onchannel(player, c);
    if (!u) {
      /* This should only happen if they gave us a bad name */
      if (!silent)
        notify_format(player, T("You are not on channel <%s>."), ChanName(c));
      return;
    }

    switch (flag) {
    case CU_QUIET:
      /* Mute */
      if (setting) {
        CUtype(u) |= CU_QUIET;
        if (!silent)
          notify_format(player,
                        T
                        ("You will no longer hear connection messages on channel <%s>."),
                        ChanName(c));
      } else {
        CUtype(u) &= ~CU_QUIET;
        if (!silent)
          notify_format(player,
                        T
                        ("You will now hear connection messages on channel <%s>."),
                        ChanName(c));
      }
      break;

    case CU_HIDE:
      /* Hide */
      if (setting) {
        if (!Chan_Can_Hide(c, player) && !Wizard(player)) {
          if (!silent)
            notify_format(player,
                          T("You are not permitted to hide on channel <%s>."),
                          ChanName(c));
        } else {
          CUtype(u) |= CU_HIDE;
          if (!silent)
            notify_format(player,
                          T
                          ("You no longer appear on channel <%s>'s who list."),
                          ChanName(c));
        }
      } else {
        CUtype(u) &= ~CU_HIDE;
        if (!silent)
          notify_format(player,
                        T("You now appear on channel <%s>'s who list."),
                        ChanName(c));
      }
      break;
    case CU_GAG:
      /* Gag */
      if (setting) {
        CUtype(u) |= CU_GAG;
        if (!silent)
          notify_format(player,
                        T
                        ("You will no longer hear messages on channel <%s>."),
                        ChanName(c));
      } else {
        CUtype(u) &= ~CU_GAG;
        if (!silent)
          notify_format(player,
                        T("You will now hear messages on channel <%s>."),
                        ChanName(c));
      }
      break;
    case CU_COMBINE:
      /* Combine */
      if (setting) {
        CUtype(u) |= CU_COMBINE;
        if (!silent)
          notify_format(player,
                        T
                        ("Connect messages on channel <%s> will now be combined with others."),
                        ChanName(c));
      } else {
        CUtype(u) &= ~CU_COMBINE;
        if (!silent)
          notify_format(player,
                        T
                        ("Connect messages on channel <%s> will no longer be combined with others."),
                        ChanName(c));
      }
      break;
    }
  } while (p != NULL);

  return;
}

/** Set a user's title for the channel.
 * \verbatim
 * This is one of the top-level functions for @channel. It handles
 * the /title switch.
 * \param player the enactor.
 * \param name the name of the channel.
 * \param title the player's new channel title.
 */
void
do_chan_title(dbref player, const char *name, const char *title)
{
  CHAN *c = NULL;
  CHANUSER *u;
  const char *scan;

  if (!name || !*name) {
    notify(player, T("You must specify a channel."));
    return;
  }

  test_channel(player, name, c);
  u = onchannel(player, c);
  if (!u) {
    notify_format(player, T("You are not on channel <%s>."), ChanName(c));
    return;
  }

  if (!rhs_present) {
    if (!CUtitle(u) || !*CUtitle(u))
      notify_format(player, T("You have no title set on <%s>."), ChanName(c));
    else
      notify_format(player, T("Your title on <%s> is '%s'."), ChanName(c),
                    CUtitle(u));
    return;
  }

  if (!title) {
    if (CUtitle(u)) {
      mush_free(CUtitle(u), "chan_user.title");
      CUtitle(u) = NULL;
    }
    if (!Quiet(player))
      notify_format(player, T("Title cleared for %schannel <%s>."),
                    Channel_NoTitles(c) ? "(NoTitles) " : "", ChanName(c));
    return;
  }

  if (ansi_strlen(title) > CU_TITLE_LEN) {
    notify(player, T("Title too long."));
    return;
  }
  scan = title;
  WALK_ANSI_STRING(scan) {
    /* Stomp newlines and other weird whitespace */
    if ((isspace((unsigned char) *scan) && (*scan != ' '))
        || (*scan == BEEP_CHAR)) {
      notify(player, T("Invalid character in title."));
      return;
    }
    scan++;
  }

  if (CUtitle(u))
    mush_free(CUtitle(u), "chan_user.title");
  CUtitle(u) = mush_strdup(title, "chan_user.title");

  if (!Quiet(player))
    notify_format(player, T("Title set for %schannel <%s>."),
                  Channel_NoTitles(c) ? "(NoTitles) " : "", ChanName(c));
  return;
}

/** List all the channels and their flags.
 * \verbatim
 * This is one of the top-level functions for @channel. It handles the
 * /list switch.
 * \endverbatim
 * \param player the enactor.
 * \param partname a partial channel name to match.
 */
void
do_channel_list(dbref player, const char *partname)
{
  CHAN *c;
  CHANUSER *u;
  char numusers[BUFFER_LEN];
  char cleanname[CHAN_NAME_LEN];
  char dispname[BUFFER_LEN];
  char *dp;
  int numblanks;

  if (SUPPORT_PUEBLO)
    notify_noenter(player, open_tag("SAMP"));
  notify_format(player, "%-30s %-5s %8s %-16s %-9s %-3s",
                T("Name"), T("Users"), T("Msgs"), T("Chan Type"), T("Status"),
                T("Buf"));
  for (c = channels; c; c = c->next) {
    strcpy(cleanname, remove_markup(ChanName(c), NULL));
    if (Chan_Can_See(c, player) && string_prefix(cleanname, partname)) {
      u = onchannel(player, c);
      if (SUPPORT_PUEBLO)
        snprintf(numusers, BUFFER_LEN,
                 "%c%cA XCH_CMD=\"@channel/who %s\" XCH_HINT=\"See who's on this channel now\"%c%5d%c%c/A%c",
                 TAG_START, MARKUP_HTML, cleanname, TAG_END, ChanNumUsers(c),
                 TAG_START, MARKUP_HTML, TAG_END);
      else
        sprintf(numusers, "%5d", ChanNumUsers(c));
      /* Display length is strlen(cleanname), but actual length is
       * strlen(ChanName(c)). There are two different cases:
       * 1. actual length <= 30. No problems.
       * 2. actual length > 30, we must reduce the number of
       * blanks we add by the (actual length-30) because our
       * %-30s is going to overflow as well.
       */
      dp = dispname;
      safe_str(ChanName(c), dispname, &dp);
      numblanks = 30 - strlen(cleanname);
      if (numblanks > 0)
        safe_fill(' ', numblanks, dispname, &dp);
      *dp = '\0';
      notify_format(player,
                    "%s %s %8ld [%c%c%c%c%c%c%c %c%c%c%c%c%c] [%-3s %c%c%c] %3d",
                    dispname, numusers, ChanNumMsgs(c),
                    Channel_Disabled(c) ? 'D' : '-',
                    Channel_Player(c) ? 'P' : '-',
                    Channel_Object(c) ? 'T' : '-',
                    Channel_Admin(c) ? 'A' : (Channel_Wizard(c) ? 'W' : '-'),
                    Channel_Quiet(c) ? 'Q' : '-',
                    Channel_CanHide(c) ? 'H' : '-', Channel_Open(c) ? 'o' : '-',
                    /* Locks */
                    ChanJoinLock(c) != TRUE_BOOLEXP ? 'j' : '-',
                    ChanSpeakLock(c) != TRUE_BOOLEXP ? 's' : '-',
                    ChanModLock(c) != TRUE_BOOLEXP ? 'm' : '-',
                    ChanSeeLock(c) != TRUE_BOOLEXP ? 'v' : '-',
                    ChanHideLock(c) != TRUE_BOOLEXP ? 'h' : '-',
                    /* Does the player own it? */
                    ChanCreator(c) == player ? '*' : '-',
                    /* User status */
                    u ? (Chanuser_Gag(u) ? T("Gag") : T("On")) : T("Off"),
                    (u && Chanuser_Quiet(u)) ? 'Q' : ' ',
                    (u && Chanuser_Hide(u)) ? 'H' : ' ',
                    (u && Chanuser_Combine(u)) ? 'C' : ' ',
                    bufferq_blocks(ChanBufferQ(c)));
    }
  }
  if (SUPPORT_PUEBLO)
    notify_noenter(player, close_tag("SAMP"));
}

static char *
list_cuflags(CHANUSER *u, int verbose)
{
  static char tbuf1[BUFFER_LEN];
  char *bp;

  /* We have to handle hide separately, since it can be the player */
  bp = tbuf1;
  if (verbose) {
    if (Chanuser_Hide(u))
      safe_str("Hide ", tbuf1, &bp);
    safe_str(privs_to_string(chanuser_priv, CUtype(u) & ~CU_HIDE), tbuf1, &bp);
  } else {
    if (Chanuser_Hide(u))
      safe_chr('H', tbuf1, &bp);
    safe_str(privs_to_letters(chanuser_priv, CUtype(u) & ~CU_HIDE), tbuf1, &bp);
  }
  *bp = '\0';
  return tbuf1;
}

/* ARGSUSED */
FUNCTION(fun_cflags)
{
  /* With one channel arg, returns list of set flags, as per
   * do_channel_list. Sample output: PQ, Oo, etc.
   * With two args (channel,object) return channel-user flags
   * for that object on that channel (a subset of GQH).
   * You must pass channel's @clock/see, and, in second case,
   * must be able to examine the object.
   */
  CHAN *c;
  CHANUSER *u;
  dbref thing;

  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 NO CHANNEL GIVEN"), buff, bp);
    return;
  }
  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  case CMATCH_AMBIG:
    safe_str(T("#-1 AMBIGUOUS CHANNEL NAME"), buff, bp);
    return;
  default:
    if (!Chan_Can_See(c, executor)) {
      safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
      return;
    }
    if (nargs == 1) {
      if (string_prefix(called_as, "CL"))
        safe_str(privs_to_string(priv_table, ChanType(c)), buff, bp);
      else
        safe_str(privs_to_letters(priv_table, ChanType(c)), buff, bp);
      return;
    }
    thing = match_thing(executor, args[1]);
    if (thing == NOTHING) {
      safe_str(T(e_match), buff, bp);
      return;
    }
    if (!Can_Examine(executor, thing)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    u = onchannel(thing, c);
    if (!u) {
      safe_str(T("#-1 NOT ON CHANNEL"), buff, bp);
      return;
    }
    safe_str(list_cuflags(u, string_prefix(called_as, "CL") ? 1 : 0), buff, bp);
    break;
  }
}

/* ARGSUSED */
FUNCTION(fun_cinfo)
{
  /* Can be called as CDESC, CBUFFER, CUSERS, CMSGS */
  CHAN *c;
  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 NO CHANNEL GIVEN"), buff, bp);
    return;
  }
  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  case CMATCH_AMBIG:
    safe_str(T("#-1 AMBIGUOUS CHANNEL NAME"), buff, bp);
    return;
  default:
    if (!Chan_Can_See(c, executor)) {
      safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
      return;
    }
    if (string_prefix(called_as, "CD")) {
      safe_str(ChanTitle(c), buff, bp);
    } else if (string_prefix(called_as, "CB")) {
      if (ChanBufferQ(c) != NULL) {
        safe_integer(BufferQSize(ChanBufferQ(c)), buff, bp);
      } else {
        safe_integer(0, buff, bp);
      }
    } else if (string_prefix(called_as, "CU")) {
      safe_integer(ChanNumUsers(c), buff, bp);
    } else if (string_prefix(called_as, "CM")) {
      safe_format(buff, bp, "%lu", ChanNumMsgs(c));
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_cbufferadd)
{
  CHAN *c;
  dbref victim;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  /* Person must be able to do nospoof cemits. */
  if (!command_check_byname(executor, "@cemit", pe_info)
      || fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  /* Find the channel. */
  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 NO CHANNEL GIVEN"), buff, bp);
    return;
  }
  /* Make sure we have text. */
  if (!args[1] || !*args[1]) {
    safe_str(T("#-1 NO TEXT GIVEN"), buff, bp);
    return;
  }

  victim = executor;
  /* Do we spoof somebody else? */
  if (nargs == 3 && parse_boolean(args[2])) {
    /* Person must be able to do nospoof cemits. */
    if (!command_check_byname(executor, "@nscemit", pe_info)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    victim = enactor;
  }
  /* Get the message. */
  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  case CMATCH_AMBIG:
    safe_str(T("#-1 AMBIGUOUS CHANNEL NAME"), buff, bp);
    return;
  default:
    if (!Chan_Can_Modify(c, executor)) {
      safe_str(T(e_perm), buff, bp);
    } else if (ChanBufferQ(c) != NULL) {
      add_to_bufferq(ChanBufferQ(c), 0, victim, args[1]);
    } else {
      safe_str(T("#-1 CHANNEL DOES NOT HAVE A BUFFER"), buff, bp);
    }
  }
}


/* ARGSUSED */
FUNCTION(fun_ctitle)
{
  /* ctitle(<channel>,<object>) returns the object's chantitle on that chan.
   * You must pass the channel's see-lock, and
   * either you must either be able to examine <object>, or
   * <object> must not be hidden, and either
   *   a) You must be on <channel>, or
   *   b) You must pass the join-lock
   */
  CHAN *c;
  CHANUSER *u;
  dbref thing;
  int ok;
  int can_ex;

  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 NO CHANNEL GIVEN"), buff, bp);
    return;
  }
  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  case CMATCH_AMBIG:
    safe_str(T("#-1 AMBIGUOUS CHANNEL NAME"), buff, bp);
    return;
  default:
    thing = match_thing(executor, args[1]);
    if (thing == NOTHING) {
      safe_str(T(e_match), buff, bp);
      return;
    }
    if (!Chan_Can_See(c, executor)) {
      safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
      return;
    }
    can_ex = Can_Examine(executor, thing);
    ok = (onchannel(executor, c) || Chan_Can_Join(c, executor));
    u = onchannel(thing, c);
    if (!u) {
      if (can_ex || ok)
        safe_str(T("#-1 NOT ON CHANNEL"), buff, bp);
      else
        safe_str(T(e_perm), buff, bp);
      return;
    }
    ok &= !Chanuser_Hide(u);
    if (!(can_ex || ok)) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (CUtitle(u))
      safe_str(CUtitle(u), buff, bp);
    break;
  }
}

/* ARGSUSED */
FUNCTION(fun_cstatus)
{
  /* cstatus(<channel>,<object>) returns the object's status on that chan.
   * You must pass the channel's see-lock, and
   * either you must either be able to examine <object>, or
   * you must be Priv_Who or <object> must not be hidden
   */
  CHAN *c;
  CHANUSER *u;
  dbref thing;

  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 NO CHANNEL GIVEN"), buff, bp);
    return;
  }
  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  case CMATCH_AMBIG:
    safe_str(T("#-1 AMBIGUOUS CHANNEL NAME"), buff, bp);
    return;
  default:
    thing = match_thing(executor, args[1]);
    if (thing == NOTHING) {
      safe_str(T(e_match), buff, bp);
      return;
    }
    if (!Chan_Can_See(c, executor)) {
      safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
      return;
    }
    u = onchannel(thing, c);
    if (!u || (!IsThing(thing) && !Connected(thing))) {
      /* Easy, they're not on it or a disconnected player */
      safe_str("Off", buff, bp);
      return;
    }
    /* They're on the channel, but maybe we can't see them? */
    if (Chanuser_Hide(u) &&
        !(Priv_Who(executor) || Can_Examine(executor, thing))) {
      safe_str("Off", buff, bp);
      return;
    }
    /* We can see them, so we report if they're On or Gag */
    safe_str(Chanuser_Gag(u) ? "Gag" : "On", buff, bp);
    return;
  }
}

FUNCTION(fun_cowner)
{
  /* Return the dbref of the owner of a channel. */
  CHAN *c;

  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 NO CHANNEL GIVEN"), buff, bp);
    return;
  }
  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    break;
  case CMATCH_AMBIG:
    safe_str(T("#-1 AMBIGUOUS CHANNEL NAME"), buff, bp);
    break;
  default:
    safe_dbref(ChanCreator(c), buff, bp);
  }

}

FUNCTION(fun_cmogrifier)
{
  /* Return the dbref of the mogrifier of a channel. */
  CHAN *c;

  if (!args[0] || !*args[0]) {
    safe_str(T("#-1 NO CHANNEL GIVEN"), buff, bp);
    return;
  }
  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    break;
  case CMATCH_AMBIG:
    safe_str(T("#-1 AMBIGUOUS CHANNEL NAME"), buff, bp);
    break;
  default:
    safe_dbref(ChanMogrifier(c), buff, bp);
  }

}


/* Remove all players from a channel, notifying them. This is the
 * utility routine for handling it. The command @channel/wipe
 * calls do_chan_wipe, below
 */
static void
channel_wipe(dbref player, CHAN *chan)
{
  CHANUSER *u, *nextu;
  dbref victim;
  /* This is easy. Just call remove_user on each user in the list */
  if (!chan)
    return;
  for (u = ChanUsers(chan); u; u = nextu) {
    nextu = u->next;
    victim = CUdbref(u);
    if (remove_user(u, chan))
      notify_format(victim, T("CHAT: %s has removed all users from <%s>."),
                    Name(player), ChanName(chan));
  }
  ChanNumUsers(chan) = 0;
  return;
}

/** Remove all players from a channel.
 * \verbatim
 * This is the top-level function for @channel/wipe, which removes all
 * players from a channel.
 * \endverbatim
 * \param player the enactor.
 * \param name name of channel to wipe.
 */
void
do_chan_wipe(dbref player, const char *name)
{
  CHAN *c;
  /* Find the channel */
  test_channel(player, name, c);
  /* Check permissions */
  if (!Chan_Can_Modify(c, player)) {
    notify(player, T("CHAT: Wipe that silly grin off your face instead."));
    return;
  }
  /* Wipe it */
  channel_wipe(player, c);
  notify_format(player, T("CHAT: Channel <%s> wiped."), ChanName(c));
  return;
}

/** Change the mogrifier of a channel.
 * \verbatim
 * This is the top-level function for @channel/mogrifier
 * \endverbatim
 * \param player the enactor.
 * \param name name of the channel.
 * \param newobj name of the new mogrifier object.
 */
void
do_chan_set_mogrifier(dbref player, const char *name, const char *newobj)
{
  CHAN *c;
  dbref it = NOTHING;
  /* Find the channel */
  test_channel(player, name, c);

  /* Only a channel modifier can do this. */
  if (!Chan_Can_Modify(c, player)) {
    notify(player, T("CHAT: Only a channel modifier can do that."));
    return;
  }

  /* Find the mogrifying object */
  if (newobj && *newobj) {
    if ((it = match_result(player, newobj, NOTYPE, MAT_EVERYTHING)) < 0) {
      if (it == NOTHING)
        notify(player, T("I can't see that here."));
      else if (it == AMBIGUOUS)
        notify(player, T("I don't know which thing you mean."));
      return;
    }
  } else if (ChanMogrifier(c) != NOTHING) {
    notify_format(player,
                  T("CHAT: Channel <%s> no longer mogrified by %s."),
                  ChanName(c), Name(ChanMogrifier(c)));
    ChanMogrifier(c) = NOTHING;
    return;
  } else {
    notify_format(player,
                  T("CHAT: Channel <%s> isn't being mogrified."), ChanName(c));
    return;
  }

  /* The player must be able to *control* the mogrifier. */
  if (!controls(player, it)) {
    notify(player, T("CHAT: You must control the mogrifier."));
    return;
  }
  ChanMogrifier(c) = it;
  notify_format(player,
                T("CHAT: Channel <%s> now mogrified by %s."), ChanName(c),
                Name(it));
  return;
}

/** Change the owner of a channel.
 * \verbatim
 * This is the top-level function for @channel/chown, which changes
 * ownership of a channel.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the channel.
 * \param newowner name of the new owner for the channel.
 */
void
do_chan_chown(dbref player, const char *name, const char *newowner)
{
  CHAN *c;
  dbref victim;
  /* Only a Wizard can do this */
  if (!Wizard(player)) {
    notify(player, T("CHAT: Only a Wizard can do that."));
    return;
  }
  /* Find the channel */
  test_channel(player, name, c);
  /* Find the victim */
  if (!newowner || ((victim = lookup_player(newowner)) == NOTHING)) {
    notify(player, T("CHAT: Invalid owner."));
    return;
  }
  /* We refund the original owner's money, but don't charge the
   * new owner.
   */
  chan_chown(c, victim);
  notify_format(player,
                T("CHAT: Channel <%s> now owned by %s."), ChanName(c),
                Name(ChanCreator(c)));
  return;
}

/** Chown all of a player's channels.
 * This function changes ownership of all of a player's channels. It's
 * usually used before destroying the player.
 * \param old dbref of old channel owner.
 * \param newowner dbref of new channel owner.
 */
void
chan_chownall(dbref old, dbref newowner)
{
  CHAN *c;

  /* Run the channel list. If a channel is owned by old, chown it
     silently to newowner */
  for (c = channels; c; c = c->next) {
    if (ChanCreator(c) == old)
      chan_chown(c, newowner);
  }
}

/* The actual chowning of a channel */
static void
chan_chown(CHAN *c, dbref victim)
{
  giveto(ChanCreator(c), ChanCost(c));
  ChanCreator(c) = victim;
  ChanCost(c) = 0;
  return;
}

/** Lock one of the channel's locks.
 * \verbatim
 * This is the top-level function for @clock.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the channel.
 * \param lockstr string representation of the lock value.
 * \param whichlock which lock is to be set.
 */
void
do_chan_lock(dbref player, const char *name, const char *lockstr,
             enum clock_type whichlock)
{
  CHAN *c;
  boolexp key;
  const char *ltype;

  /* Make sure the channel exists */
  test_channel(player, name, c);
  /* Make sure the player has permission */
  if (!Chan_Can_Modify(c, player)) {
    notify_format(player, T("CHAT: Channel <%s> resists."), ChanName(c));
    return;
  }
  /* Ok, let's do it */
  switch (whichlock) {
  case CLOCK_JOIN:
    ltype = chan_join_lock;
    break;
  case CLOCK_MOD:
    ltype = chan_mod_lock;
    break;
  case CLOCK_SEE:
    ltype = chan_see_lock;
    break;
  case CLOCK_HIDE:
    ltype = chan_hide_lock;
    break;
  case CLOCK_SPEAK:
    ltype = chan_speak_lock;
    break;
  default:
    ltype = "ChanUnknownLock";
  }

  if (!lockstr || !*lockstr) {
    /* Unlock it */
    key = TRUE_BOOLEXP;
  } else {
    key = parse_boolexp(player, lockstr, ltype);
    if (key == TRUE_BOOLEXP) {
      notify(player, T("CHAT: I don't understand that key."));
      return;
    }
  }
  switch (whichlock) {
  case CLOCK_JOIN:
    free_boolexp(ChanJoinLock(c));
    ChanJoinLock(c) = key;
    notify_format(player, (key == TRUE_BOOLEXP) ?
                  T("CHAT: Joinlock on <%s> reset.") :
                  T("CHAT: Joinlock on <%s> set."), ChanName(c));
    break;
  case CLOCK_SPEAK:
    free_boolexp(ChanSpeakLock(c));
    ChanSpeakLock(c) = key;
    notify_format(player, (key == TRUE_BOOLEXP) ?
                  T("CHAT: Speaklock on <%s> reset.") :
                  T("CHAT: Speaklock on <%s> set."), ChanName(c));
    break;
  case CLOCK_SEE:
    free_boolexp(ChanSeeLock(c));
    ChanSeeLock(c) = key;
    notify_format(player, (key == TRUE_BOOLEXP) ?
                  T("CHAT: Seelock on <%s> reset.") :
                  T("CHAT: Seelock on <%s> set."), ChanName(c));
    break;
  case CLOCK_HIDE:
    free_boolexp(ChanHideLock(c));
    ChanHideLock(c) = key;
    notify_format(player, (key == TRUE_BOOLEXP) ?
                  T("CHAT: Hidelock on <%s> reset.") :
                  T("CHAT: Hidelock on <%s> set."), ChanName(c));
    break;
  case CLOCK_MOD:
    free_boolexp(ChanModLock(c));
    ChanModLock(c) = key;
    notify_format(player, (key == TRUE_BOOLEXP) ?
                  T("CHAT: Modlock on <%s> reset.") :
                  T("CHAT: Modlock on <%s> set."), ChanName(c));
    break;
  }
  return;
}


/** A channel list with names and descriptions only.
 * \verbatim
 * This is the top-level function for @channel/what.
 * \endverbatim
 * \param player the enactor.
 * \param partname a partial name of channels to match.
 */
void
do_chan_what(dbref player, const char *partname)
{
  CHAN *c;
  int found = 0;
  char *cleanname;
  char cleanp[CHAN_NAME_LEN];

  cleanname = normalize_channel_name(partname);
  for (c = channels; c; c = c->next) {
    strcpy(cleanp, remove_markup(ChanName(c), NULL));
    if (string_prefix(cleanp, cleanname) && Chan_Can_See(c, player)) {
      notify(player, ChanName(c));
      notify_format(player, T("Description: %s"), ChanTitle(c));
      notify_format(player, T("Owner: %s"), Name(ChanCreator(c)));
      if (ChanMogrifier(c) != NOTHING) {
        notify_format(player, T("Mogrifier: %s (#%d)"),
                      Name(ChanMogrifier(c)), ChanMogrifier(c));
      }
      notify_format(player, T("Flags: %s"),
                    privs_to_string(priv_table, ChanType(c)));
      if (ChanBufferQ(c))
        notify_format(player,
                      T
                      ("Recall buffer: %db (%d full lines), with %d lines stored."),
                      BufferQSize(ChanBufferQ(c)),
                      bufferq_blocks(ChanBufferQ(c)),
                      bufferq_lines(ChanBufferQ(c)));
      found++;
    }
  }
  if (!found)
    notify(player, T("CHAT: I don't recognize that channel."));
}


/** A decompile of a channel.
 * \verbatim
 * This is the top-level function for @channel/decompile, which attempts
 * to produce all the MUSHcode necessary to recreate a channel and its
 * membership.
 * \param player the enactor.
 * \param name name of the channel.
 * \param brief if 1, don't include channel membership.
 */
void
do_chan_decompile(dbref player, const char *name, int brief)
{
  CHAN *c;
  CHANUSER *u;
  int found;
  char cleanname[BUFFER_LEN];
  char cleanp[CHAN_NAME_LEN];
  char rawp[BUFFER_LEN];

  found = 0;
  strcpy(cleanname, remove_markup(name, NULL));
  for (c = channels; c; c = c->next) {
    strcpy(cleanp, remove_markup(ChanName(c), NULL));
    if (string_prefix(cleanp, cleanname)) {
      found++;
      if (!(See_All(player) || Chan_Can_Modify(c, player)
            || (ChanCreator(c) == player))) {
        if (Chan_Can_See(c, player))
          notify_format(player,
                        T("CHAT: You don't have permission to decompile <%s>."),
                        ChanName(c));
        continue;
      }
      strcpy(rawp, ChanName(c));        /* Because decompose_str is destructive */
      notify_format(player, "@channel/add %s = %s", decompose_str(rawp),
                    privs_to_string(priv_table, ChanType(c)));
      notify_format(player, "@channel/chown %s = %s", cleanp,
                    Name(ChanCreator(c)));
      if (ChanMogrifier(c) != NOTHING) {
        notify_format(player, "@channel/mogrifier %s = #%d", cleanp,
                      ChanMogrifier(c));
      }
      if (ChanModLock(c) != TRUE_BOOLEXP)
        notify_format(player, "@clock/mod %s = %s", cleanp,
                      unparse_boolexp(player, ChanModLock(c), UB_MEREF));
      if (ChanHideLock(c) != TRUE_BOOLEXP)
        notify_format(player, "@clock/hide %s = %s", cleanp,
                      unparse_boolexp(player, ChanHideLock(c), UB_MEREF));
      if (ChanJoinLock(c) != TRUE_BOOLEXP)
        notify_format(player, "@clock/join %s = %s", cleanp,
                      unparse_boolexp(player, ChanJoinLock(c), UB_MEREF));
      if (ChanSpeakLock(c) != TRUE_BOOLEXP)
        notify_format(player, "@clock/speak %s = %s", cleanp,
                      unparse_boolexp(player, ChanSpeakLock(c), UB_MEREF));
      if (ChanSeeLock(c) != TRUE_BOOLEXP)
        notify_format(player, "@clock/see %s = %s", cleanp,
                      unparse_boolexp(player, ChanSeeLock(c), UB_MEREF));
      if (ChanTitle(c))
        notify_format(player, "@channel/desc %s = %s", cleanp, ChanTitle(c));
      if (ChanBufferQ(c))
        notify_format(player, "@channel/buffer %s = %d", cleanp,
                      bufferq_blocks(ChanBufferQ(c)));
      if (!brief) {
        for (u = ChanUsers(c); u; u = u->next) {
          if (!Chanuser_Hide(u) || Priv_Who(player)) {
            if (IsPlayer(CUdbref(u))) {
              notify_format(player, "@channel/on %s = *%s", cleanp,
                            Name(CUdbref(u)));
            } else {
              notify_format(player, "@channel/on %s = #%d", cleanp, CUdbref(u));
            }
          }
        }
      }
    }
  }
  if (!found)
    notify(player, T("CHAT: No channel matches that string."));
}

static void
do_channel_who(dbref player, CHAN *chan)
{
  char tbuf1[BUFFER_LEN];
  char *bp;
  CHANUSER *u;
  dbref who;
  int i = 0;
  bp = tbuf1;
  for (u = ChanUsers(chan); u; u = u->next) {
    who = CUdbref(u);
    if ((IsThing(who) || Connected(who)) &&
        (!Chanuser_Hide(u) || Priv_Who(player))) {
      i++;
      safe_itemizer(i, !(u->next), ",", T("and"), " ", tbuf1, &bp);
      safe_str(Name(who), tbuf1, &bp);
      if (IsThing(who))
        safe_format(tbuf1, &bp, "(#%d)", who);
      if (Chanuser_Hide(u) && Chanuser_Gag(u))
        safe_str(" (hidden,gagging)", tbuf1, &bp);
      else if (Chanuser_Hide(u))
        safe_str(" (hidden)", tbuf1, &bp);
      else if (Chanuser_Gag(u))
        safe_str(" (gagging)", tbuf1, &bp);
    }
  }
  *bp = '\0';
  if (!*tbuf1)
    notify(player, T("There are no connected players on that channel."));
  else {
    notify_format(player, T("Members of channel <%s> are:"), ChanName(chan));
    notify(player, tbuf1);
  }
}

/* ARGSUSED */
FUNCTION(fun_cwho)
{
  int first = 1;
  int matchcond = 0;
  int priv = 0;
  int skip_gagged = 0;
  int show;
  CHAN *chan = NULL;
  CHANUSER *u;
  dbref who;

  switch (find_channel(args[0], &chan, executor)) {
  case CMATCH_NONE:
    notify(executor, T("No such channel."));
    return;
  case CMATCH_AMBIG:
    notify(executor, T("I can't tell which channel you mean."));
    return;
  default:
    break;
  }

  if (nargs > 1 && args[1] && *args[1]) {
    if (!strcasecmp(args[1], "on"))
      matchcond = 0;
    else if (!strcasecmp(args[1], "off"))
      matchcond = 1;
    else if (!strcasecmp(args[1], "all"))
      matchcond = 2;
    else {
      safe_str(T("#-1 INVALID ARGUMENT"), buff, bp);
      return;
    }
  }

  if (nargs > 2 && parse_boolean(args[2]))
    skip_gagged = 1;

  /* Feh. We need to do some sort of privilege checking, so that
   * if mortals can't do '@channel/who wizard', they can't do
   * 'think cwho(wizard)' either. The first approach that comes to
   * mind is the following:
   * if (!ChannelPermit(privs,chan)) ...
   * Unfortunately, we also want objects to be able to check cwho()
   * on channels.
   * So, we check the owner, instead, because most uses of cwho()
   * are either in the Master Room owned by a wizard, or on people's
   * quicktypers.
   */

  if (!Chan_Can_See(chan, Owner(executor))
      && !Chan_Can_See(chan, executor)) {
    safe_str(T("#-1 NO PERMISSIONS FOR CHANNEL"), buff, bp);
    return;
  }

  priv = Priv_Who(executor);

  for (u = ChanUsers(chan); u; u = u->next) {
    who = CUdbref(u);
    show = 1;
    if (!IsThing(who) && matchcond != 2) {
      if (matchcond)
        show = !Connected(who) || (Chanuser_Hide(u) && !priv);
      else
        show = Connected(who) && (!Chanuser_Hide(u) || priv);
    }
    if (!show)
      continue;
    if (Chanuser_Gag(u) && skip_gagged)
      continue;
    if (first)
      first = 0;
    else
      safe_chr(' ', buff, bp);
    safe_dbref(who, buff, bp);
  }
}


/** Modify a channel's description.
 * \verbatim
 * This is the top-level function for @channel/desc, which sets a channel's
 * description.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the channel.
 * \param title description of the channel.
 */
void
do_chan_desc(dbref player, const char *name, const char *title)
{
  CHAN *c;
  /* Check new title length */
  if (title && strlen(title) > CHAN_TITLE_LEN - 1) {
    notify(player, T("CHAT: New description too long."));
    return;
  }
  /* Make sure the channel exists */
  test_channel(player, name, c);
  /* Make sure the player has permission */
  if (!Chan_Can_Modify(c, player)) {
    notify(player, "CHAT: Yeah, right.");
    return;
  }
  /* Ok, let's do it */
  if (!title || !*title) {
    ChanTitle(c)[0] = '\0';
    notify_format(player, T("CHAT: Channel <%s> description cleared."),
                  ChanName(c));
  } else {
    strcpy(ChanTitle(c), title);
    notify_format(player, T("CHAT: Channel <%s> description set."),
                  ChanName(c));
  }
}



static int
yesno(const char *str)
{
  if (!str || !*str)
    return ERR;
  switch (str[0]) {
  case 'y':
  case 'Y':
    return YES;
  case 'n':
  case 'N':
    return NO;
  case 'o':
  case 'O':
    switch (str[1]) {
    case 'n':
    case 'N':
      return YES;
    case 'f':
    case 'F':
      return NO;
    default:
      return ERR;
    }
  default:
    return ERR;
  }
}

/* Can this player still add channels, or have they created their
 * limit already?
 */
static int
canstilladd(dbref player)
{
  CHAN *c;
  int num = 0;
  for (c = channels; c; c = c->next) {
    if (ChanCreator(c) == player)
      num++;
  }
  return (num < MAX_PLAYER_CHANS);
}

extern DESC *descriptor_list;

/** Tell players on a channel when someone connects or disconnects.
 * \param player player that is connecting or disconnecting.
 * \param msg message to announce.
 * \param ungag if 1, remove any channel gags the player has.
 */
void
chat_player_announce(dbref player, char *msg, int ungag)
{
  DESC *d;
  CHAN *c;
  CHANUSER *up = NULL, *uv;
  char buff[BUFFER_LEN], *bp;
  char buff2[BUFFER_LEN], *bp2;
  dbref viewer;
  bool shared = false;
  int na_flags = NA_INTER_LOCK | NA_SPOOF | NA_INTER_PRESENCE;
  intmap *seen;
  struct format_msg format;
  char *accname;

  /* Use the regular channel_send() for all non-combined players. */
  for (c = channels; c; c = c->next) {
    up = onchannel(player, c);
    if (up) {
      if (!Channel_Quiet(c) && (Channel_Admin(c) || Channel_Wizard(c)
                                || (!Chanuser_Hide(up) && !Dark(player)))) {
        channel_send(c, player,
                     CB_NOCOMBINE | CB_CHECKQUIET | CB_PRESENCE | CB_POSE, msg);
      }
      if (ungag) {
        CUtype(up) &= ~CU_GAG;
      }
    }
  }

  seen = im_new();
  accname = mush_strdup(accented_name(player), "chat_announce.name");

  format.thing = AMBIGUOUS;
  format.attr = "CHATFORMAT";
  format.checkprivs = 0;
  format.numargs = 6;
  format.targetarg = -1;
  format.args[0] = "@";
  format.args[1] = buff2;
  /* args[2] and args[5] are set in the for loop below */
  format.args[3] = accname;
  format.args[4] = "";

  for (d = descriptor_list; d != NULL; d = d->next) {
    viewer = d->player;
    if (d->connected) {
      shared = false;
      bp = buff;
      bp2 = buff2;

      for (c = channels; c; c = c->next) {
        up = onchannel(player, c);
        uv = onchannel(viewer, c);
        if (up && uv) {
          if (!Channel_Quiet(c) && !Chanuser_Quiet(uv)
              && (Channel_Admin(c) || Channel_Wizard(c)
                  || (!Chanuser_Hide(up) && !Dark(player)))) {
            if (Chanuser_Combine(uv)) {
              shared = true;
              safe_str(ChanName(c), buff, &bp);
              safe_strl(" | ", 3, buff, &bp);
              safe_str(ChanName(c), buff2, &bp2);
              safe_chr('|', buff2, &bp2);
            }
          }
        }
        if (up && ungag)
          CUtype(up) &= ~CU_GAG;
      }

      bp -= 3;
      *bp = '\0';
      bp2--;
      *bp2 = '\0';

      if (shared && !im_exists(seen, viewer)) {
        char defmsg[BUFFER_LEN], *dmp;
        char shrtmsg[BUFFER_LEN], *smp;

        im_insert(seen, viewer, up);

        dmp = defmsg;
        smp = shrtmsg;


        safe_format(shrtmsg, &smp, "%s %s", accname, msg);
        *smp = '\0';
        format.args[2] = shrtmsg;

        safe_format(defmsg, &dmp, "<%s> %s %s", buff, accname, msg);
        *dmp = '\0';
        format.args[5] = defmsg;

        notify_anything(player, na_one, &viewer, NULL, na_flags, defmsg, NULL,
                        AMBIGUOUS, &format);

      }
    }
  }
  mush_free(accname, "chat_announce.name");

  im_destroy(seen);
}

/** Return a list of channels that the player is on.
 * \param player player whose channels are to be shown.
 * \return string listing player's channels, prefixed with Channels:
 */
const char *
channel_description(dbref player)
{
  static char buf[BUFFER_LEN];
  char *bp;
  CHANLIST *c;

  bp = buf;

  if (Chanlist(player)) {
    safe_str(T("Channels:"), buf, &bp);
    for (c = Chanlist(player); c; c = c->next) {
      safe_chr(' ', buf, &bp);
      safe_str(ChanName(c->chan), buf, &bp);
    }
  } else if (IsPlayer(player))
    safe_str(T("Channels: *NONE*"), buf, &bp);

  *bp = '\0';
  return buf;
}


FUNCTION(fun_channels)
{
  dbref it;
  char sep = ' ';
  CHAN *c;
  CHANLIST *cl;
  CHANUSER *u;
  int can_ex, priv_who;

  /* There are these possibilities:
   *  no args - just a list of all channels
   *   2 args - object, delimiter
   *   1 arg - either object or delimiter. If it's longer than 1 char,
   *           we treat it as an object.
   * You can see an object's channels if you can examine it.
   * Otherwise you can see only channels that you share with
   * it where it's not hidden.
   */
  if (nargs >= 1) {
    /* Given an argument, return list of channels it's on */
    it = match_result(executor, args[0], NOTYPE, MAT_EVERYTHING);
    if (GoodObject(it)) {
      int first = 1;
      if (!delim_check(buff, bp, nargs, args, 2, &sep))
        return;
      can_ex = Can_Examine(executor, it);
      priv_who = Priv_Who(executor);
      for (cl = Chanlist(it); cl; cl = cl->next) {
        if (can_ex || (Chan_Can_See(cl->chan, executor)
                       && (u = onchannel(it, cl->chan)) && (priv_who
                                                            ||
                                                            !Chanuser_Hide(u))))
        {
          if (!first)
            safe_chr(sep, buff, bp);
          safe_str(ChanName(cl->chan), buff, bp);
          first = 0;
        }
      }
      return;
    } else {
      /* args[0] didn't match. Maybe it's a delimiter? */
      if (arglens[0] > 1) {
        if (it == NOTHING)
          notify(executor, T("I can't see that here."));
        else if (it == AMBIGUOUS)
          notify(executor, T("I don't know which thing you mean."));
        return;
      } else if (!delim_check(buff, bp, nargs, args, 1, &sep))
        return;
    }
  }
  /* No arguments (except maybe delimiter) - return list of all channels */
  for (c = channels; c; c = c->next) {
    if (Chan_Can_See(c, executor)) {
      if (c != channels)
        safe_chr(sep, buff, bp);
      safe_str(ChanName(c), buff, bp);
    }
  }
  return;
}

FUNCTION(fun_clock)
{
  CHAN *c = NULL;
  char *p = NULL;
  boolexp lock_ptr = TRUE_BOOLEXP;
  int which_lock = 0;

  if ((p = strchr(args[0], '/'))) {
    *p++ = '\0';
  } else {
    p = (char *) "JOIN";
  }

  switch (find_channel(args[0], &c, executor)) {
  case CMATCH_NONE:
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  case CMATCH_AMBIG:
    safe_str(T("#-2 AMBIGUOUS CHANNEL MATCH"), buff, bp);
    return;
  default:
    break;
  }

  if (!strcasecmp(p, "JOIN")) {
    which_lock = CLOCK_JOIN;
    lock_ptr = ChanJoinLock(c);
  } else if (!strcasecmp(p, "SPEAK")) {
    which_lock = CLOCK_SPEAK;
    lock_ptr = ChanSpeakLock(c);
  } else if (!strcasecmp(p, "MOD")) {
    which_lock = CLOCK_MOD;
    lock_ptr = ChanModLock(c);
  } else if (!strcasecmp(p, "SEE")) {
    which_lock = CLOCK_SEE;
    lock_ptr = ChanSeeLock(c);
  } else if (!strcasecmp(p, "HIDE")) {
    which_lock = CLOCK_HIDE;
    lock_ptr = ChanHideLock(c);
  } else {
    safe_str(T("#-1 NO SUCH LOCK TYPE"), buff, bp);
    return;
  }

  if (nargs == 2) {
    if (FUNCTION_SIDE_EFFECTS) {
      if (!command_check_byname(executor, "@clock", pe_info)
          || fun->flags & FN_NOSIDEFX) {
        safe_str(T(e_perm), buff, bp);
        return;
      }
      do_chan_lock(executor, args[0], args[1], which_lock);
      return;
    } else {
      safe_str(T(e_disabled), buff, bp);
    }
  }

  if (Chan_Can_Decomp(c, executor)) {
    safe_str(unparse_boolexp(executor, lock_ptr, UB_MEREF), buff, bp);
    return;
  } else {
    safe_str(T(e_perm), buff, bp);
    return;
  }
}

/* ARGSUSED */
FUNCTION(fun_cemit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = PEMIT_SILENT;
  flags |= (ns ? PEMIT_SPOOF : 0);
  if (!command_check_byname(executor, ns ? "@nscemit" : "@cemit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (nargs == 3 && parse_boolean(args[2]))
    flags &= ~PEMIT_SILENT;
  orator = executor;
  do_cemit(executor, args[0], args[1], flags);
}

/* ARGSUSED */
FUNCTION(fun_crecall)
{
  CHAN *chan;
  CHANUSER *u;
  int start = -1, num_lines;
  bool recall_timestring = false;
  time_t recall_from = 0;
  char *p = NULL, *buf, *name;
  time_t timestamp;
  char *stamp;
  dbref speaker;
  int type;
  int first = 1;
  char sep;
  int showstamp = 0;

  name = args[0];
  if (!name || !*name) {
    safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  }

  if (!args[1] || !*args[1]) {
    num_lines = 10;             /* default */
  } else if (is_strict_integer(args[1])) {
    num_lines = parse_integer(args[1]);
    if (num_lines == 0)
      num_lines = INT_MAX;
  } else if (etime_to_secs(args[1], &num_lines)) {
    recall_timestring = 1;
    recall_from = (time_t) mudtime - num_lines;
  } else {
    safe_str(T(e_int), buff, bp);
    return;
  }
  if (!args[2] || !*args[2]) {
    /* nothing */
  } else if (is_integer(args[2])) {
    start = parse_integer(args[2]) - 1;
  } else {
    safe_str(T(e_int), buff, bp);
    return;
  }

  if (!delim_check(buff, bp, nargs, args, 4, &sep))
    return;

  if (nargs > 4 && args[4] && *args[4])
    showstamp = parse_boolean(args[4]);

  if (num_lines < 0) {
    safe_str(T(e_uint), buff, bp);
    return;
  }

  test_channel_fun(executor, name, chan, buff, bp);
  if (!Chan_Can_See(chan, executor)) {
    if (onchannel(executor, chan))
      safe_str(T(e_perm), buff, bp);
    else
      safe_str(T("#-1 NO SUCH CHANNEL"), buff, bp);
    return;
  }

  u = onchannel(executor, chan);
  if (!u && !Chan_Can_Access(chan, executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!ChanBufferQ(chan)) {
    safe_str(T("#-1 NO RECALL BUFFER"), buff, bp);
    return;
  }

  if (recall_timestring) {
    num_lines = 0;
    while ((buf =
            iter_bufferq(ChanBufferQ(chan), &p, &speaker, &type, &timestamp))) {
      if (timestamp >= recall_from)
        num_lines++;
    }
    p = NULL;
  }
  if (start < 0)
    start = BufferQNum(ChanBufferQ(chan)) - num_lines;
  if (isempty_bufferq(ChanBufferQ(chan))
      || BufferQNum(ChanBufferQ(chan)) <= start) {
    return;
  }

  while (start > 0) {
    buf = iter_bufferq(ChanBufferQ(chan), &p, &speaker, &type, &timestamp);
    start--;
  }
  while ((buf = iter_bufferq(ChanBufferQ(chan), &p, &speaker, &type,
                             &timestamp)) && num_lines > 0) {
    if (first)
      first = 0;
    else
      safe_chr(sep, buff, bp);
    if (!showstamp)
      safe_str(buf, buff, bp);
    else {
      stamp = show_time(timestamp, 0);
      safe_format(buff, bp, "[%s] %s", stamp, buf);
    }
    num_lines--;
  }


}

COMMAND(cmd_cemit)
{
  int flags = SILENT_OR_NOISY(sw, !options.noisy_cemit);
  if (!strcmp(cmd->name, "@NSCEMIT") && Can_Nspemit(executor))
    flags |= PEMIT_SPOOF;

  SPOOF(executor, enactor, sw);

  do_cemit(executor, arg_left, arg_right, flags);
}

COMMAND(cmd_channel)
{
  if (SW_ISSET(sw, SWITCH_LIST))
    do_channel_list(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ADD))
    do_chan_admin(executor, arg_left, args_right[1], CH_ADMIN_ADD);
  else if (SW_ISSET(sw, SWITCH_DELETE))
    do_chan_admin(executor, arg_left, args_right[1], CH_ADMIN_DEL);
  else if (SW_ISSET(sw, SWITCH_NAME))
    do_chan_admin(executor, arg_left, args_right[1], CH_ADMIN_RENAME);
  else if (SW_ISSET(sw, SWITCH_RENAME))
    do_chan_admin(executor, arg_left, args_right[1], CH_ADMIN_RENAME);
  else if (SW_ISSET(sw, SWITCH_PRIVS))
    do_chan_admin(executor, arg_left, args_right[1], CH_ADMIN_PRIV);
  else if (SW_ISSET(sw, SWITCH_RECALL))
    do_chan_recall(executor, arg_left, args_right, SW_ISSET(sw, SWITCH_QUIET));
  else if (SW_ISSET(sw, SWITCH_DECOMPILE))
    do_chan_decompile(executor, arg_left, SW_ISSET(sw, SWITCH_BRIEF));
  else if (SW_ISSET(sw, SWITCH_DESCRIBE))
    do_chan_desc(executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_TITLE))
    do_chan_title(executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_MOGRIFIER))
    do_chan_set_mogrifier(executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_CHOWN))
    do_chan_chown(executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_WIPE))
    do_chan_wipe(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_MUTE))
    do_chan_user_flags(executor, arg_left, args_right[1], CU_QUIET, 0);
  else if (SW_ISSET(sw, SWITCH_UNMUTE))
    do_chan_user_flags(executor, arg_left, "n", CU_QUIET, 0);
  else if (SW_ISSET(sw, SWITCH_HIDE))
    do_chan_user_flags(executor, arg_left, args_right[1], CU_HIDE, 0);
  else if (SW_ISSET(sw, SWITCH_UNHIDE))
    do_chan_user_flags(executor, arg_left, "n", CU_HIDE, 0);
  else if (SW_ISSET(sw, SWITCH_GAG))
    do_chan_user_flags(executor, arg_left, args_right[1], CU_GAG, 0);
  else if (SW_ISSET(sw, SWITCH_UNGAG))
    do_chan_user_flags(executor, arg_left, "n", CU_GAG, 0);
  else if (SW_ISSET(sw, SWITCH_COMBINE))
    do_chan_user_flags(executor, arg_left, args_right[1], CU_COMBINE, 0);
  else if (SW_ISSET(sw, SWITCH_UNCOMBINE))
    do_chan_user_flags(executor, arg_left, "n", CU_COMBINE, 0);
  else if (SW_ISSET(sw, SWITCH_WHAT))
    do_chan_what(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_BUFFER))
    do_chan_buffer(executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_ON) || SW_ISSET(sw, SWITCH_JOIN))
    do_channel(executor, arg_left, args_right[1], "ON");
  else if (SW_ISSET(sw, SWITCH_OFF) || SW_ISSET(sw, SWITCH_LEAVE))
    do_channel(executor, arg_left, args_right[1], "OFF");
  else if (SW_ISSET(sw, SWITCH_WHO))
    do_channel(executor, arg_left, args_right[1], "WHO");
  else
    do_channel(executor, arg_left, NULL, args_right[1]);
}

COMMAND(cmd_chat)
{
  do_chat_by_name(executor, arg_left, arg_right, 1);
}

COMMAND(cmd_clock)
{
  if (SW_ISSET(sw, SWITCH_JOIN))
    do_chan_lock(executor, arg_left, arg_right, CLOCK_JOIN);
  else if (SW_ISSET(sw, SWITCH_SPEAK))
    do_chan_lock(executor, arg_left, arg_right, CLOCK_SPEAK);
  else if (SW_ISSET(sw, SWITCH_MOD))
    do_chan_lock(executor, arg_left, arg_right, CLOCK_MOD);
  else if (SW_ISSET(sw, SWITCH_SEE))
    do_chan_lock(executor, arg_left, arg_right, CLOCK_SEE);
  else if (SW_ISSET(sw, SWITCH_HIDE))
    do_chan_lock(executor, arg_left, arg_right, CLOCK_HIDE);
  else
    notify(executor, T("You must specify a type of lock!"));
}

/**
 * \verbatim
 * Mogrify a value using u(<mogrifier>/<attrname>,<value>)
 * \endverbatim
 *
 * \param mogrifier The object doing the mogrification
 * \param attrname The attribute on mogrifier to call.
 * \param player the enactor
 * \param numargs the number of args in argv
 * \param argv array of args
 * \param orig the original string to mogrify
 * \retval Mogrified text.
 */
char *
mogrify(dbref mogrifier, const char *attrname,
        dbref player, int numargs, const char *argv[], const char *orig)
{
  static char buff[BUFFER_LEN];
  int i;
  PE_REGS *pe_regs;
  buff[0] = '\0';

  pe_regs = pe_regs_create(PE_REGS_ARG, "mogrify");
  for (i = 0; i < numargs; i++) {
    if (argv[i]) {
      pe_regs_setenv_nocopy(pe_regs, i, argv[i]);
    }
  }

  i = call_attrib(mogrifier, attrname, buff, player, NULL, pe_regs);

  pe_regs_free(pe_regs);

  if (i) {
    if (buff[0]) {
      return buff;
    }
  }

  snprintf(buff, BUFFER_LEN, "%s", orig);

  return buff;
}

/** Broadcast a message to a channel, using @chatformat if it's
 *  available, and mogrifying.
 * \param channel pointer to channel to broadcast to.
 * \param player message speaker.
 * \param flags broadcast flag mask (see CB_* constants in extchat.h)
 * \param fmt message format string.
 */
void
channel_send(CHAN *channel, dbref player, int flags, const char *origmessage)
{
/* flags:
 *     CB_CHECKQUIET CB_NOSPOOF CB_PRESENCE CB_POSE CB_EMIT CB_SPEECH
 *     CB_NOCOMBINE
 */

  /* These are static only to prevent them from chewing up
   * time to malloc/free, and chewing up space.
   */
  static char channame[BUFFER_LEN];
  static char title[BUFFER_LEN];
  static char playername[BUFFER_LEN];
  static char message[BUFFER_LEN];
  static char buff[BUFFER_LEN];
  static char speechtext[BUFFER_LEN];
  struct format_msg format;

  CHANUSER *u;
  CHANUSER *speaker;
  dbref current;
  char *bp;
  const char *blockstr = "";
  int na_flags = NA_INTER_LOCK;
  const char *someone = "Someone";
  dbref mogrifier = NOTHING;
  const char *ctype = NULL;
  const char *argv[10];
  int override_chatformat = 0;
  memset(argv, 0, sizeof(argv));

  /* Make sure we can write to the channel before doing anything */
  if (Channel_Disabled(channel))
    return;

  speaker = onchannel(player, channel);

  snprintf(channame, BUFFER_LEN, "<%s>", ChanName(channel));

  if (!Channel_NoTitles(channel) && speaker &&
      CUtitle(speaker) && *CUtitle(speaker)) {
    snprintf(title, BUFFER_LEN, "%s", CUtitle(speaker));
  } else {
    title[0] = '\0';
  }

  if (Channel_NoNames(channel)) {
    playername[0] = '\0';
  } else {
    snprintf(playername, BUFFER_LEN, "%s", accented_name(player));
  }
  if (!title[0] && !playername[0]) {
    snprintf(playername, BUFFER_LEN, "%s", someone);
  }

  if (flags & CB_PRESENCE) {
    ctype = "@";
  } else if (flags & CB_POSE) {
    ctype = ":";
  } else if (flags & CB_SEMIPOSE) {
    ctype = ";";
  } else if (flags & CB_EMIT) {
    ctype = "|";
  } else {
    ctype = "\"";
  }

  snprintf(speechtext, BUFFER_LEN, "says");

  snprintf(message, BUFFER_LEN, "%s", origmessage);

  if (GoodObject(ChanMogrifier(channel))) {
    if (eval_lock(player, ChanMogrifier(channel), Use_Lock)) {
      mogrifier = ChanMogrifier(channel);

      argv[0] = ctype;
      argv[1] = ChanName(channel);
      argv[2] = message;
      argv[3] = playername;
      argv[4] = title;

      blockstr = mogrify(mogrifier, "MOGRIFY`BLOCK", player, 6, argv, "");
      if (blockstr && *blockstr) {
        notify(player, blockstr);
        return;
      }
      /* Do we override chatformats? */
      if (parse_boolean
          (mogrify(mogrifier, "MOGRIFY`OVERRIDE", player, 6, argv, ""))) {
        override_chatformat = 1;
      }

      argv[1] = ChanName(channel);
      argv[2] = ctype;
      argv[3] = message;
      argv[4] = title;
      argv[5] = playername;

      argv[0] = channame;
      snprintf(channame, BUFFER_LEN, "%s",
               mogrify(mogrifier, "MOGRIFY`CHANNAME", player, 6, argv,
                       channame));

      argv[0] = title;
      snprintf(title, BUFFER_LEN, "%s",
               mogrify(mogrifier, "MOGRIFY`TITLE", player, 6, argv, title));

      argv[0] = playername;
      snprintf(playername, BUFFER_LEN, "%s",
               mogrify(mogrifier, "MOGRIFY`PLAYERNAME", player, 6, argv,
                       playername));

      if (flags & CB_SPEECH) {
        argv[0] = speechtext;
        snprintf(speechtext, BUFFER_LEN, "%s",
                 mogrify(mogrifier, "MOGRIFY`SPEECHTEXT", player, 6, argv,
                         speechtext));
      }

      argv[0] = message;
      snprintf(message, BUFFER_LEN, "%s",
               mogrify(mogrifier, "MOGRIFY`MESSAGE", player, 6, argv, message));
    }
  }

  bp = buff;

  *bp = '\0';

  if (!(flags & CB_QUIET)) {
    safe_str(channame, buff, &bp);
    safe_chr(' ', buff, &bp);
  }

  if (flags & CB_EMIT) {
    safe_str(message, buff, &bp);
  } else {
    if (!(flags & CB_PRESENCE)) {
      if (title[0]) {
        safe_str(title, buff, &bp);
        safe_chr(' ', buff, &bp);
      }
    }
    safe_str(playername, buff, &bp);
    switch (flags & CB_TYPE) {
    case CB_POSE:
      safe_chr(' ', buff, &bp);
    case CB_SEMIPOSE:
      safe_str(message, buff, &bp);
      break;
    case CB_SPEECH:
      safe_format(buff, &bp, " %s, \"%s\"", T(speechtext), message);
      break;
    }
  }
  *bp = '\0';

  /* @chatformat */
  if (flags & CB_PRESENCE) {
    snprintf(title, BUFFER_LEN, "%s", message);
    snprintf(message, BUFFER_LEN, "%s %s", playername, title);
    title[0] = '\0';
  }

  if (GoodObject(mogrifier)) {
    argv[0] = ctype;
    argv[1] = ChanName(channel);
    argv[2] = message;
    argv[3] = playername;
    argv[4] = title;
    argv[5] = buff;
    snprintf(buff, BUFFER_LEN, "%s",
             mogrify(mogrifier, "MOGRIFY`FORMAT", player, 6, argv, buff));
  }

  if (Channel_Interact(channel)) {
    na_flags |= (flags & CB_PRESENCE) ? NA_INTER_PRESENCE : NA_INTER_HEAR;
  }

  if (!(flags & CB_NOSPOOF)) {
    na_flags |= NA_SPOOF;
  }

  format.thing = AMBIGUOUS;
  format.attr = "CHATFORMAT";
  format.checkprivs = 0;
  format.numargs = 6;
  format.targetarg = -1;
  format.args[0] = (char *) ctype;
  format.args[1] = ChanName(channel);
  format.args[2] = message;
  format.args[3] = playername;
  format.args[4] = title;
  format.args[5] = buff;


  for (u = ChanUsers(channel); u; u = u->next) {
    current = CUdbref(u);

    if ((flags & CB_NOCOMBINE) && Chanuser_Combine(u)) {
      continue;
    }
    if (!(((flags & CB_CHECKQUIET) && Chanuser_Quiet(u)) ||
          Chanuser_Gag(u) || (IsPlayer(current) && !Connected(current)))) {
      notify_anything(player, na_one, &current, NULL, na_flags, buff, NULL,
                      AMBIGUOUS, (override_chatformat ? NULL : &format));
    }
  }

  if (ChanBufferQ(channel))
    add_to_bufferq(ChanBufferQ(channel), 0,
                   (flags & CB_NOSPOOF) ? player : NOTHING, buff);

  if (!(flags & CB_PRESENCE) && !speaker) {
    notify_format(player, T("To channel %s: %s"), ChanName(channel), buff);
  }
}

/** Recall past lines from the channel's buffer.
 * We try to recall no more lines that are requested by the player,
 * but sometimes we may have fewer to recall.
 * \verbatim
 * This is the top-level function for @chan/recall.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the channel.
 * \param lineinfo point to array containing lines, optional start
 * \param quiet if true, don't show timestamps.
 */
void
do_chan_recall(dbref player, const char *name, char *lineinfo[], int quiet)
{
  CHAN *chan;
  CHANUSER *u;
  char *lines;
  const char *startpos;
  int num_lines;
  int start = -1;
  time_t recall_from = 0;
  bool recall_timestring = false;
  int all;
  char *p = NULL, *buf;
  time_t timestamp;
  char *stamp;
  dbref speaker;
  int type;
  if (!name || !*name) {
    notify(player, T("You need to specify a channel."));
    return;
  }
  lines = lineinfo[1];
  startpos = lineinfo[2];
  if (startpos && *startpos) {
    if (!is_integer(startpos)) {
      notify(player, T("Which line do you want to start recall from?"));
      return;
    }
    start = parse_integer(startpos) - 1;
  }
  if (lines && *lines) {
    if (is_strict_integer(lines)) {
      num_lines = parse_integer(lines);
      if (num_lines == 0)
        num_lines = INT_MAX;
    } else if (etime_to_secs(lines, &num_lines)) {
      recall_timestring = 1;
      recall_from = (time_t) mudtime - num_lines;
    } else {
      notify(player, T("How many lines did you want to recall?"));
      return;
    }
  } else {
    num_lines = 10;             /* default value */
  }

  if (num_lines < 1) {
    notify(player, T("How many lines did you want to recall?"));
    return;
  }

  test_channel(player, name, chan);
  if (!Chan_Can_See(chan, player)) {
    if (onchannel(player, chan))
      notify_format(player,
                    T("CHAT: You can't do that with channel <%s>."),
                    ChanName(chan));
    else
      notify(player, T("CHAT: I don't recognize that channel."));
    return;
  }
  u = onchannel(player, chan);
  if (!u && !Chan_Can_Join(chan, player)) {
    notify(player,
           T("CHAT: You must be able to join a channel to recall from it."));
    return;
  }
  if (!ChanBufferQ(chan)) {
    notify(player, T("CHAT: That channel doesn't have a recall buffer."));
    return;
  }
  if (recall_timestring) {
    num_lines = 0;
    while (iter_bufferq(ChanBufferQ(chan), &p, &speaker, &type, &timestamp)) {
      if (timestamp >= recall_from)
        num_lines++;
    }
    p = NULL;
  }
  if (start < 0)
    start = BufferQNum(ChanBufferQ(chan)) - num_lines;
  if (isempty_bufferq(ChanBufferQ(chan))
      || (BufferQNum(ChanBufferQ(chan)) <= start)) {
    notify(player, T("CHAT: Nothing to recall."));
    return;
  }
  all = (start <= 0 && num_lines >= BufferQNum(ChanBufferQ(chan)));
  notify_format(player, T("CHAT: Recall from channel <%s>"), ChanName(chan));
  while (start > 0) {
    iter_bufferq(ChanBufferQ(chan), &p, &speaker, &type, &timestamp);
    start--;
  }
  while ((buf = iter_bufferq(ChanBufferQ(chan), &p, &speaker, &type,
                             &timestamp)) && num_lines > 0) {
    if (quiet)
      notify(player, buf);
    else {
      stamp = show_time(timestamp, 0);
      notify_format(player, "[%s] %s", stamp, buf);
    }
    num_lines--;
  }
  notify(player, T("CHAT: End recall"));
  if (!all)
    notify_format(player,
                  T
                  ("CHAT: To recall the entire buffer, use @chan/recall %s=0"),
                  ChanName(chan));
}

/** Set the size of a channel's buffer in maximum lines.
 * \verbatim
 * This is the top-level function for @chan/buffer.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the channel.
 * \param lines a string given the number of lines to buffer.
 */
void
do_chan_buffer(dbref player, const char *name, const char *lines)
{
  CHAN *chan;
  int size;
  if (!name || !*name) {
    notify(player, T("You need to specify a channel."));
    return;
  }
  if (!lines || !*lines || !is_strict_integer(lines)) {
    notify(player, T("You need to specify the number of lines to buffer."));
    return;
  }
  size = parse_integer(lines);
  if (size < 0 || size > 10) {
    notify(player, T("Invalid buffer size."));
    return;
  }
  test_channel(player, name, chan);
  if (!Chan_Can_Modify(chan, player)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (!size) {
    /* Remove a channel's buffer */
    if (ChanBufferQ(chan)) {
      free_bufferq(ChanBufferQ(chan));
      ChanBufferQ(chan) = NULL;
      notify_format(player,
                    T
                    ("CHAT: Channel buffering disabled for channel <%s>."),
                    ChanName(chan));
    } else {
      notify_format(player,
                    T
                    ("CHAT: Channel buffering already disabled for channel <%s>."),
                    ChanName(chan));
    }
  } else {
    if (ChanBufferQ(chan)) {
      /* Resize a buffer */
      ChanBufferQ(chan) = reallocate_bufferq(ChanBufferQ(chan), size);
      notify_format(player,
                    T("CHAT: Resizing buffer of channel <%s>"), ChanName(chan));
    } else {
      /* Start a new buffer */
      ChanBufferQ(chan) = allocate_bufferq(size);
      notify_format(player,
                    T("CHAT: Buffering enabled on channel <%s>."),
                    ChanName(chan));
    }
  }
}

/** Evaluate a channel lock with %0 set to the channel name.
 * \param c the channel to test.
 * \param p the object trying to pass the lock.
 * \param type the type of channel lock to test.
 * \return true or false
 */
int
eval_chan_lock(CHAN *c, dbref p, enum clock_type type)
{
  NEW_PE_INFO *pe_info;

  boolexp b = TRUE_BOOLEXP;
  int retval;
  if (!c || !GoodObject(p))
    return 0;
  switch (type) {
  case CLOCK_SEE:
    b = ChanSeeLock(c);
    break;
  case CLOCK_JOIN:
    b = ChanJoinLock(c);
    break;
  case CLOCK_SPEAK:
    b = ChanSpeakLock(c);
    break;
  case CLOCK_HIDE:
    b = ChanHideLock(c);
    break;
  case CLOCK_MOD:
    b = ChanModLock(c);
  }

  pe_info = make_pe_info("pe_info-eval_chan_lock");
  pe_regs_setenv_nocopy(pe_info->regvals, 0, ChanName(c));
  retval = eval_boolexp(p, b, p, pe_info);
  free_pe_info(pe_info);
  return retval;
}
