/**
 * \file extchat.h
 *
 * \brief Header file for the PennMUSH chat system
 *
 * \verbatim
 * Header file for Javelin's extended @chat system
 * Based on the Battletech MUSE comsystem ported to PennMUSH by Kalkin
 *
 * Why:
 *  In the old system, channels were represented by bits set in a
 *  4-byte int on the db object. This had disadvantages - a limit
 *  of 32 channels, and players could find themselves on null channels.
 *  In addition, the old system required recompiles to permanently
 *  add channels, since the chaninfo was in the source.
 * How:
 *  Channels are a structure in a linked list.
 *  Each channel stores a whole bunch of info, including who's
 *  on it.
 *  We read/write this list using a chatdb file.
 *  We also maintain a linked list of channels that the user is
 *   connected to on the db object, which we set up at load time.
 *
 * User interface:
 * @chat channel = message
 * +channel message
 * @channel/on channel [= player] (or @channel channel = on)  do_channel()
 * @channel/off channel [= player] do_channel()
 * @channel/who channel do_channel()
 * @channel/title channel=title do_chan_title()
 * @channel/list do_chan_list()
 * @channel/add channel do_chan_admin()
 * @channel/priv channel = <privlist>  do_chan_admin()
 *  Privlist being: wizard, admin, private, moderated, etc.
 * @channel/joinlock channel = lock
 * @channel/speaklock channel = lock
 * @channel/modlock channel = lock
 * @channel/delete channel
 * @channel/quiet channel = yes/no
 * @channel/wipe channel
 * @channel/buffer channel = <maxlines>
 * @channel/recall channel [= <lines>]
 *
 * \endverbatim
 */

#ifndef __EXTCHAT_H
#define __EXTCHAT_H


#include "boolexp.h"
#include "bufferq.h"

#define CU_TITLE_LEN (options.chan_title_len)

/** A channel user.
 * This structure represents an object joined to a chat channel.
 * Each chat channel maintains a linked list of users.
 */
struct chanuser {
  dbref who;                    /**< Dbref of joined object */
  privbits type;                /**< Bitflags for this user */
  char *title;                  /**< User's channel title */
  struct chanuser *next;        /**< Pointer to next user in list */
};

/* Flags and macros for channel users */
#define CU_QUIET    0x1         /* Do not hear connection messages */
#define CU_HIDE     0x2         /* Do not appear on the user list */
#define CU_GAG      0x4         /* Do not hear any messages */
#define CU_COMBINE  0x8         /* Combine connect/disconnect messages */
#define CU_DEFAULT_FLAGS 0x0

/* channel_broadcast flags */
#define CB_SPEECH     0x01      /* This is player speech */
#define CB_POSE       0x02      /* This is a pose */
#define CB_SEMIPOSE   0x04      /* This is a semipose */
#define CB_EMIT       0x08      /* This is an emit */
#define CB_TYPE       0x0F      /* Type of a message. */
#define CB_CHECKQUIET 0x10      /* Check for quiet flag on recipients */
#define CB_NOSPOOF    0x20      /* Use nospoof emits */
#define CB_PRESENCE   0x40      /* This is a presence message, not sound */
#define CB_QUIET      0x80      /* Do not prepend the <Channel> name */
#define CB_NOCOMBINE  0x100     /* Don't send this message to players with
                                 * their channels set COMBINE */

#define CUdbref(u) ((u)->who)
#define CUtype(u) ((u)->type)
#define CUtitle(u) ((u)->title)
#define CUnext(u) ((u)->next)
#define Chanuser_Quiet(u)       (CUtype(u) & CU_QUIET)
#define Chanuser_Hide(u) ((CUtype(u) & CU_HIDE) || (IsPlayer(CUdbref(u)) && hidden(CUdbref(u))))
#define Chanuser_Gag(u) (CUtype(u) & CU_GAG)
#define Chanuser_Combine(u) (CUtype(u) & CU_COMBINE)

/* This is a chat channel */
#define CHAN_NAME_LEN 31
#define CHAN_TITLE_LEN 256
/** A chat channel.
 * This structure represents a MUSH chat channel. Channels are organized
 * into a sorted linked list.
 */
struct channel {
  char *name;                   /**< Channel name */
  char title[CHAN_TITLE_LEN];   /**< Channel description */
  privbits type;                /**< Channel flags */
  int cost;             /**< What it cost to make this channel */
  dbref creator;                /**< This is who paid the cost for the channel */
  dbref mogrifier;              /**< This is the object that mogrifies the channel text. */
  int num_users;                /**< Number of connected users */
  int max_users;                /**< Maximum allocated users */
  struct chanuser *users;       /**< Linked list of current users */
  unsigned long int num_messages;       /**< How many messages handled by this chan since startup */
  boolexp joinlock;     /**< Who may join */
  boolexp speaklock;    /**< Who may speak */
  boolexp modifylock;   /**< Who may change things and boot people */
  boolexp seelock;      /**< Who can see this in a list */
  boolexp hidelock;     /**< Who may hide from view */
  struct channel *next;         /**< Next channel in linked list */
  BUFFERQ *bufferq;             /**< Pointer to channel recall buffer queue */
};

/** A list of channels on an object.
 * This structure is a linked list of channels that is associated
 * with each object
 */
struct chanlist {
  CHAN *chan;                   /**< Channel data */
  struct chanlist *next;        /**< Next channel in list */
};

#define Chanlist(x) ((struct chanlist *)get_objdata(x, "CHANNELS"))
#define s_Chanlist(x, y) set_objdata(x, "CHANNELS", (void *)y)

/* Channel type flags and macros */
#define CHANNEL_PLAYER  0x1U    /* Players may join */
#define CHANNEL_OBJECT  0x2U    /* Objects may join */
#define CHANNEL_DISABLED 0x4U   /* Channel is turned off */
#define CHANNEL_QUIET   0x8U    /* No broadcasts connect/disconnect */
#define CHANNEL_ADMIN   0x10U   /* Wizard and royalty only ok */
#define CHANNEL_WIZARD  0x20U   /* Wizard only ok */
#define CHANNEL_CANHIDE 0x40U   /* Can non-DARK Wizards hide here? */
#define CHANNEL_OPEN    0x80U   /* Can you speak if you're not joined? */
#define CHANNEL_NOTITLES 0x100U /* Don't show titles of speakers */
#define CHANNEL_NONAMES 0x200U  /* Don't show names of speakers */
#define CHANNEL_NOCEMIT 0x400U  /* Disallow @cemit */
#define CHANNEL_INTERACT 0x800U /* Filter channel output through interactions */
#define CHANNEL_DEFAULT_FLAGS   (CHANNEL_PLAYER)
#define CHANNEL_COST (options.chan_cost)
#define MAX_PLAYER_CHANS (options.max_player_chans)
#define MAX_CHANNELS (options.max_channels)

#define ChanName(c) ((c)->name)
#define ChanType(c) ((c)->type)
#define ChanTitle(c) ((c)->title)
#define ChanCreator(c) ((c)->creator)
#define ChanMogrifier(c) ((c)->mogrifier)
#define ChanCost(c) ((c)->cost)
#define ChanNumUsers(c) ((c)->num_users)
#define ChanMaxUsers(c) ((c)->max_users)
#define ChanUsers(c) ((c)->users)
#define ChanNext(c) ((c)->next)
#define ChanNumMsgs(c) ((c)->num_messages)
#define ChanJoinLock(c) ((c)->joinlock)
#define ChanSpeakLock(c) ((c)->speaklock)
#define ChanModLock(c) ((c)->modifylock)
#define ChanSeeLock(c) ((c)->seelock)
#define ChanHideLock(c) ((c)->hidelock)
#define ChanBufferQ(c) ((c)->bufferq)
#define Channel_Quiet(c)        (ChanType(c) & CHANNEL_QUIET)
#define Channel_Open(c) (ChanType(c) & CHANNEL_OPEN)
#define Channel_Object(c) (ChanType(c) & CHANNEL_OBJECT)
#define Channel_Player(c) (ChanType(c) & CHANNEL_PLAYER)
#define Channel_Disabled(c) (ChanType(c) & CHANNEL_DISABLED)
#define Channel_Wizard(c) (ChanType(c) & CHANNEL_WIZARD)
#define Channel_Admin(c) (ChanType(c) & CHANNEL_ADMIN)
#define Channel_CanHide(c) (ChanType(c) & CHANNEL_CANHIDE)
#define Channel_NoTitles(c) (ChanType(c) & CHANNEL_NOTITLES)
#define Channel_NoNames(c) (ChanType(c) & CHANNEL_NONAMES)
#define Channel_NoCemit(c) (ChanType(c) & CHANNEL_NOCEMIT)
#define Channel_Interact(c) (ChanType(c) & CHANNEL_INTERACT)
#define Chan_Ok_Type(c,o) \
        ((IsPlayer(o) && Channel_Player(c)) || \
         (IsThing(o) && Channel_Object(c)))
#define Chan_Can(p,t) \
     (!(t & CHANNEL_DISABLED) && (!(t & CHANNEL_WIZARD) || Wizard(p)) && \
      (!(t & CHANNEL_ADMIN) || Hasprivs(p) || (has_power_by_name(p,"CHAT_PRIVS",NOTYPE))))
/* Who can change channel privileges to type t */
#define Chan_Can_Priv(p,t) (Wizard(p) || Chan_Can(p,t))
#define Chan_Can_Access(c,p) (Chan_Can(p,ChanType(c)))
#define Chan_Can_Join(c,p) \
     (Chan_Can_Access(c,p) && \
     (eval_chan_lock(c,p, CLOCK_JOIN)))
#define Chan_Can_Speak(c,p) \
     (Chan_Can_Access(c,p) && \
     (eval_chan_lock(c,p, CLOCK_SPEAK)))
#define Chan_Can_Cemit(c,p) \
     (!Channel_NoCemit(c) && Chan_Can_Speak(c,p))
#define Chan_Can_Modify(c,p) \
     (Wizard(p) || (ChanCreator(c) == (p)) || \
     (!Guest(p) && Chan_Can_Access(c,p) && \
     (eval_chan_lock(c,p, CLOCK_MOD))))
#define Chan_Can_See(c,p) \
     (Hasprivs(p) || See_All(p) || (Chan_Can_Access(c,p) && \
     (eval_chan_lock(c,p, CLOCK_SEE))))
#define Chan_Can_Hide(c,p) \
     (Can_Hide(p) || (Channel_CanHide(c) && Chan_Can_Access(c,p) && \
     (eval_chan_lock(c,p, CLOCK_HIDE))))
#define Chan_Can_Nuke(c,p) (Wizard(p) || (ChanCreator(c) == (p)))
#define Chan_Can_Decomp(c,p) (See_All(p) || (ChanCreator(c) == (p)))



     /* For use in channel matching */
enum cmatch_type { CMATCH_NONE, CMATCH_EXACT, CMATCH_PARTIAL, CMATCH_AMBIG };
#define CMATCHED(i) (((i) == CMATCH_EXACT) | ((i) == CMATCH_PARTIAL))

enum clock_type { CLOCK_JOIN, CLOCK_SPEAK, CLOCK_SEE, CLOCK_HIDE, CLOCK_MOD };

/* Some globals */
extern int num_channels;
CHANUSER *onchannel(dbref who, CHAN *c);
void init_chatdb(void);
int load_chatdb(PENNFILE *fp);
int save_chatdb(PENNFILE *fp);
void do_cemit(dbref player, const char *name, const char *msg, int flags);
void do_chan_user_flags
  (dbref player, char *name, const char *isyn, int flag, int silent);
void do_chan_wipe(dbref player, const char *name);
void do_chan_lock
  (dbref player, const char *name, const char *lockstr,
   enum clock_type whichlock);
void do_chan_what(dbref player, const char *partname);
void do_chan_desc(dbref player, const char *name, const char *title);
void do_chan_title(dbref player, const char *name, const char *title);
void do_chan_recall(dbref player, const char *name, char *lineinfo[],
                    int quiet);
void do_chan_buffer(dbref player, const char *name, const char *lines);
void init_chat(void);
void do_channel
  (dbref player, const char *name, const char *target, const char *com);
void do_chat(dbref player, CHAN *chan, const char *arg1);
enum chan_admin_op { CH_ADMIN_ADD, CH_ADMIN_DEL, CH_ADMIN_RENAME,
  CH_ADMIN_PRIV
};
void do_chan_admin(dbref player, char *name, const char *perms,
                   enum chan_admin_op flag);
enum cmatch_type find_channel(const char *p, CHAN **chan, dbref player);
enum cmatch_type find_channel_partial(const char *p, CHAN **chan, dbref player);
void do_channel_list(dbref player, const char *partname);
int do_chat_by_name
  (dbref player, const char *name, const char *msg, int source);
void do_chan_decompile(dbref player, const char *name, int brief);
void do_chan_chown(dbref player, const char *name, const char *newowner);
const char *channel_description(dbref player);


int eval_chan_lock(CHAN *c, dbref p, enum clock_type type);

/** Ways to match channels by partial name */
enum chan_match_type {
  PMATCH_ALL,  /**< Match all channels */
  PMATCH_OFF,  /**< Match channels user isn't on */
  PMATCH_ON    /**< Match channels user is on */
};


/* Chat db flags */
#define CDB_SPIFFY 0x01         /* Has mogrifier and buffer */

#endif                          /* __EXTCHAT_H */
