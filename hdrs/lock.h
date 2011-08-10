/**
 * \file lock.h
 *
 * \brief Header for \@locks
 */


#include "copyrite.h"

#ifndef __LOCK_H
#define __LOCK_H

#include "mushtype.h"
#include "conf.h"
#include "boolexp.h"

/* I'm using a string for a lock type instead of a magic-cookie int for
 * several reasons:
 * 1) I don't think it will hurt efficiency that much. I'll profile it
 * to check.
 * 2) It will make debugging much easier to see lock types that can easily
 * be interpreted by a human.
 * 3) It allows the possibility of having arbitrary user-defined locks.
 */

/** A list of locks set on an object.
 * An object's locks are represented as a linked list of these structures.
 */
struct lock_list {
  lock_type type;               /**< Type of lock */
  boolexp key;          /**< Lock value ("key") */
  dbref creator;                /**< Dbref of lock creator */
  privbits flags;                       /**< Lock flags */
  struct lock_list *next;       /**< Pointer to next lock in object's list */
};

/* Our table of lock types, attributes, and default flags */
typedef struct lock_msg_info LOCKMSGINFO;
/** A lock.
 * This structure represents a lock in the table of lock types
 */
struct lock_msg_info {
  lock_type type;               /**< Type of lock */
  const char *succbase;         /**< Base name of success attribute */
  const char *failbase;         /**< Base name of failure attribute */
};

/* Lock flags, set via @lset */
#define LF_VISUAL       0x001U  /**< Anyone can see this lock with lock()/elock() */
#define LF_PRIVATE      0x002U  /**< This lock doesn't get inherited */
#define LF_WIZARD       0x004U  /**< Only wizards can set/unset this lock */
#define LF_LOCKED       0x008U  /**< Only the lock's owner can set/unset it */
#define LF_NOCLONE      0x010U  /**< This lock isn't copied in @clone */
#define LF_OX           0x020U  /**< This lock's success messages includes OX*. */
#define LF_NOSUCCACTION 0x040U  /**< This lock doesn't have an @a-action for success. */
#define LF_NOFAILACTION 0x080U  /**< This lock doesn't have an @a-action for failure */
#define LF_OWNER        0x100U  /**< Lock can only be set/unset by object's owner */
#define LF_DEFAULT      0x200U  /**< Use default flags when setting lock */

/* lock.c */
boolexp getlock(dbref thing, lock_type type);
boolexp getlock_noparent(dbref thing, lock_type type);
lock_type match_lock(lock_type type);
const lock_list *get_lockproto(lock_type type);
int add_lock(dbref player, dbref thing, lock_type type, boolexp key,
             privbits flags);
int add_lock_raw(dbref player, dbref thing, lock_type type,
                 boolexp key, privbits flags);
void free_locks(lock_list *ll);
int eval_lock_with(dbref player, dbref thing, lock_type ltype,
                   NEW_PE_INFO *pe_info);
#define eval_lock(player,thing,ltype) eval_lock_with(player,thing,ltype,NULL)
int eval_lock_clear(dbref player, dbref thing, lock_type ltype,
                    NEW_PE_INFO *pe_info);
int fail_lock(dbref player, dbref thing, lock_type ltype, const char *def,
              dbref loc);
void do_unlock(dbref player, const char *name, lock_type type);
void do_lock(dbref player, const char *name, const char *keyname,
             lock_type type);
void init_locks(void);
void clone_locks(dbref player, dbref orig, dbref clone);
void do_lset(dbref player, char *what, char *flags);
void do_list_locks(dbref player, const char *arg, int lc, const char *label);
void list_locks(char *buff, char **bp, const char *name);
const char *lock_flags(lock_list *ll);
const char *lock_flags_long(lock_list *ll);
void list_lock_flags(char *buff, char **bp);
void list_lock_flags_long(char *buff, char **bp);
lock_list *getlockstruct(dbref thing, lock_type type);
void check_zone_lock(dbref player, dbref zone, int noisy);
void define_lock(lock_type name, privbits flags);
void purge_locks(void);
#define L_FLAGS(lock) ((lock)->flags)
#define L_CREATOR(lock) ((lock)->creator)
#define L_TYPE(lock) ((lock)->type)
#define L_KEY(lock) ((lock)->key)
#define L_NEXT(lock) ((lock)->next)
/* can p read/evaluate lock l on object x? */
bool lock_visual(dbref, lock_type);
#define Can_Read_Lock(p,x,l)   \
    (See_All(p) || controls(p,x) || ((Visual(x) || lock_visual(x, l)) && \
     eval_lock(p,x,Examine_Lock)))
/* The actual magic cookies. */
extern lock_type Basic_Lock;
extern lock_type Enter_Lock;
extern lock_type Use_Lock;
extern lock_type Zone_Lock;
extern lock_type Page_Lock;
extern lock_type Tport_Lock;
extern lock_type Speech_Lock;   /* Who can speak aloud in me */
extern lock_type Listen_Lock;   /* Who can trigger ^s/ahears on me */
extern lock_type Command_Lock;  /* Who can use $commands on me */
extern lock_type Parent_Lock;   /* Who can @parent to me */
extern lock_type Link_Lock;     /* Who can @link to me */
extern lock_type Leave_Lock;    /* Who can leave me */
extern lock_type Drop_Lock;     /* Who can drop me */
extern lock_type Give_Lock;     /* Who can give me */
extern lock_type From_Lock;     /* Who can give to me */
extern lock_type Pay_Lock;      /* Who can give money to me */
extern lock_type Receive_Lock;  /* What can be given to me */
extern lock_type Mail_Lock;     /* Who can @mail me */
extern lock_type Follow_Lock;   /* Who can follow me */
extern lock_type Examine_Lock;  /* Who can examine visual me */
extern lock_type Chzone_Lock;   /* Who can @chzone to this object? */
extern lock_type Forward_Lock;  /* Who can @forwardlist to object? */
extern lock_type Control_Lock;  /* Who can control this object? */
extern lock_type Dropto_Lock;   /* Who follows the dropto of this room? */
extern lock_type Destroy_Lock;  /* Who can @dest me if I'm dest_ok? */
extern lock_type Interact_Lock;
extern lock_type MailForward_Lock;      /* Who can forward mail to me */
extern lock_type Take_Lock;     /* Who can take from the contents of this object? */
extern lock_type Open_Lock;     /* who can @open exits in this room? */
extern lock_type Filter_Lock;   /* Who can be forwarded by audible objects */
extern lock_type InFilter_Lock; /* Whose sound is played inside listening objects */

#endif                          /* __LOCK_H */
