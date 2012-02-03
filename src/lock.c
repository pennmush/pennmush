/**
 * \file lock.c
 *
 * \brief Locks for PennMUSH.
 *
 * \verbatim
 *
 * This is the core of Ralph Melton's rewrite of the @lock system.
 * These are some of the underlying assumptions:
 *
 * 1) Locks are checked many more times than they are set, so it is
 * quite worthwhile to spend time when setting locks if it expedites
 * checking locks later.
 *
 * 2) Most possible locks are never used. For example, in the days
 * when there were only basic locks, use locks, and enter locks, in
 * one database of 15000 objects, there were only about 3500 basic
 * locks, 400 enter locks, and 400 use locks.
 * Therefore, it is important to make the case where no lock is present
 * efficient both in time and in memory.
 *
 * 3) It is far more common to have the server itself check for locks
 * than for people to check for locks in MUSHcode. Therefore, it is
 * reasonable to incur a minor slowdown for checking locks in MUSHcode
 * in order to speed up the server's checking.
 *
 * \endverbatim
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "externs.h"
#include "boolexp.h"
#include "mushdb.h"
#include "attrib.h"
#include "dbdefs.h"
#include "lock.h"
#include "match.h"
#include "log.h"
#include "flags.h"
#include "mymalloc.h"
#include "strtree.h"
#include "privtab.h"
#include "parse.h"
#include "htab.h"
#include "confmagic.h"


/* If any lock_type ever contains the character '|', reading in locks
 * from the db will break.
 */
lock_type Basic_Lock = "Basic";     /**< Name of basic lock */
lock_type Enter_Lock = "Enter";     /**< Name of enter lock */
lock_type Use_Lock = "Use";         /**< Name of use lock */
lock_type Zone_Lock = "Zone";       /**< Name of zone lock */
lock_type Page_Lock = "Page";       /**< Name of page lock */
lock_type Tport_Lock = "Teleport";  /**< Name of teleport lock */
lock_type Speech_Lock = "Speech";   /**< Name of speech lock */
lock_type Listen_Lock = "Listen";   /**< Name of listen lock */
lock_type Command_Lock = "Command"; /**< Name of command lock */
lock_type Parent_Lock = "Parent";   /**< Name of parent lock */
lock_type Link_Lock = "Link";       /**< Name of link lock */
lock_type Leave_Lock = "Leave";     /**< Name of leave lock */
lock_type Drop_Lock = "Drop";       /**< Name of drop lock */
lock_type Give_Lock = "Give";       /**< Name of give lock */
lock_type From_Lock = "From";       /**< Name of from lock */
lock_type Pay_Lock = "Pay";         /**< Name of pay lock */
lock_type Receive_Lock = "Receive"; /**< Name of receive lock */
lock_type Mail_Lock = "Mail";       /**< Name of mail lock */
lock_type Follow_Lock = "Follow";   /**< Name of follow lock */
lock_type Examine_Lock = "Examine"; /**< Name of examine lock */
lock_type Chzone_Lock = "Chzone";   /**< Name of chzone lock */
lock_type Forward_Lock = "Forward"; /**< Name of forward lock */
lock_type Control_Lock = "Control"; /**< Name of control lock */
lock_type Dropto_Lock = "Dropto";   /**< Name of dropto lock */
lock_type Destroy_Lock = "Destroy"; /**< Name of destroy lock */
lock_type Interact_Lock = "Interact"; /**< Name of interaction lock */
lock_type MailForward_Lock = "MailForward"; /**< Name of mailforward lock */
lock_type Take_Lock = "Take";       /**< Name of take lock */
lock_type Open_Lock = "Open";       /**< Name of open lock */
lock_type Filter_Lock = "Filter";   /**< Name of filter lock */
lock_type InFilter_Lock = "InFilter"; /**< Name of infilter lock */

 /** Table of lock names and permissions */
lock_list lock_types[] = {
  {"Basic", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Enter", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Use", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Zone", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Page", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Teleport", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Speech", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Listen", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Command", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Parent", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Link", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Leave", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Drop", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Give", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"From", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Pay", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Receive", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Mail", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Follow", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Examine", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Chzone", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Forward", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Control", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Dropto", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Destroy", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Interact", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"MailForward", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Take", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Open", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Filter", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"InFilter", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {NULL, TRUE_BOOLEXP, GOD, 0, NULL}
};

HASHTAB htab_locks;

 /**
  * \verbatim
  * Table of base attributes associated with success and failure of
  * locks. These are the historical ones; we automatically generate
  * such attribute names for those that aren't in this table using
  * <lock>_LOCK`<message>
  * \endverbatim
  */
const LOCKMSGINFO lock_msgs[] = {
  {"Basic", "SUCCESS", "FAILURE"},
  {"Enter", "ENTER", "EFAIL"},
  {"Use", "USE", "UFAIL"},
  {"Leave", "LEAVE", "LFAIL"},
  {NULL, NULL, NULL}
};

/** Table of lock permissions */
PRIV lock_privs[] = {
  {"visual", 'v', LF_VISUAL, LF_VISUAL},
  {"no_inherit", 'i', LF_PRIVATE, LF_PRIVATE},
  {"no_clone", 'c', LF_NOCLONE, LF_NOCLONE},
  {"wizard", 'w', LF_WIZARD, LF_WIZARD},
  /*  {"owner", 'o', LF_OWNER, LF_OWNER}, */
  {"locked", '+', LF_LOCKED, LF_LOCKED},
  {NULL, '\0', 0, 0}
};

StrTree lock_names;  /**< String tree of lock names */

static void free_one_lock_list(lock_list *ll);
static lock_type check_lock_type(dbref player, dbref thing, lock_type name);
static int delete_lock(dbref player, dbref thing, lock_type type);
static int can_write_lock(dbref player, dbref thing, lock_list *lock);
static lock_list *getlockstruct_noparent(dbref thing, lock_type type);

slab *lock_slab = NULL;
static lock_list *next_free_lock(const void *hint);
static void free_lock(lock_list *ll);

extern int unparsing_boolexp;

static int
lock_compare(const void *a, const void *b)
{
  const lock_list *la = *(lock_list *const *) a, *lb = *(lock_list *const *) b;
  return strcmp(la->type, lb->type);
}

/** Return a list of all available locks
 * \param buff the buffer
 * \param bp a pointer to the current position in the buffer.
 * \param name if not NULL, only show locks with this prefix
 */
void
list_locks(char *buff, char **bp, const char *name)
{
  lock_list **locks, *lk;
  bool first = 1;
  int nlocks = 0, n;

  locks = mush_calloc(htab_locks.entries, sizeof(lock_list), "lock.list");

  for (lk = hash_firstentry(&htab_locks); lk; lk = hash_nextentry(&htab_locks)) {
    /* Skip those that don't match */
    if (name && !string_prefix(lk->type, name))
      continue;
    locks[nlocks++] = lk;
  }

  qsort(locks, nlocks, sizeof lk, lock_compare);

  for (n = 0; n < nlocks; n += 1) {
    if (first) {
      first = 0;
    } else {
      safe_chr(' ', buff, bp);
    }
    safe_str(strupper(locks[n]->type), buff, bp);
  }

  mush_free(locks, "lock.list");
}

/** User interface to list locks.
 * \verbatim
 * This function implements @list/locks.
 * \endverbatim
 * \param player the enactor.
 * \param arg wildcard pattern of flag names to list, or NULL for all.
 * \param lc if 1, list flags in lowercase.
 * \param label label to prefix to list.
 */
void
do_list_locks(dbref player, const char *arg, int lc, const char *label)
{
  char buff[BUFFER_LEN];
  char *bp = buff;
  list_locks(buff, &bp, arg);
  *bp = '\0';
  notify_format(player, "%s: %s", label, lc ? strlower(buff) : buff);
}


/** Return a list of lock flag characters.
 * \param ll pointer to a lock.
 * \return string of lock flag characters.
 */
const char *
lock_flags(lock_list *ll)
{
  return privs_to_letters(lock_privs, L_FLAGS(ll));
}

/** List all lock flag characters on a buffer
 * \param buff The buffer
 * \param bp Pointer to a position in the buffer.
 */

void
list_lock_flags(char *buff, char **bp)
{
  int i;
  for (i = 0; lock_privs[i].name; i++) {
    if (lock_privs[i].letter)
      safe_chr(lock_privs[i].letter, buff, bp);
  }
}

/** List all lock flag names on a buffer
 * \param buff The buffer
 * \param bp Pointer to a position in the buffer.
 */

void
list_lock_flags_long(char *buff, char **bp)
{
  int i;
  int first = 1;
  for (i = 0; lock_privs[i].name; i++) {
    if (!first)
      safe_chr(' ', buff, bp);
    first = 0;
    safe_str(lock_privs[i].name, buff, bp);
  }
}

/** Return a list of lock flag names.
 * \param ll pointer to a lock.
 * \return string of lock flag names, space-separated.
 */
const char *
lock_flags_long(lock_list *ll)
{
  return privs_to_string(lock_privs, L_FLAGS(ll));
}


static int
string_to_lockflag(dbref player, char const *p, privbits *flag)
{
  privbits f;
  f = string_to_privs(lock_privs, p, 0);
  if (!f)
    return -1;
  if (!See_All(player) && (f & LF_WIZARD))
    return -1;
  *flag = f;
  return 0;
}

/** Initialize the lock strtree. */
void
init_locks(void)
{
  lock_list *ll;
  st_init(&lock_names, "LockNameTree");

  hashinit(&htab_locks, 25);

  for (ll = lock_types; ll->type && *ll->type; ll++)
    hashadd(strupper(ll->type), ll, &htab_locks);

  local_locks();
}


/** Add a new lock to the table.
 * \param name The name of the lock
 * \param flags The default flags.
 */
void
define_lock(lock_type name, privbits flags)
{
  lock_list *newlock;

  newlock = mush_malloc(sizeof *newlock, "lock");
  newlock->type = mush_strdup(strupper(name), "lock.name");
  newlock->flags = flags;
  newlock->creator = GOD;
  newlock->key = TRUE_BOOLEXP;
  newlock->next = NULL;
  hashadd((char *) newlock->type, newlock, &htab_locks);
}

static int
can_write_lock(dbref player, dbref thing, lock_list *lock)
{
  if (God(player))
    return 1;
  if (God(thing))
    return 0;
  if (Wizard(player))
    return 1;
  if (L_FLAGS(lock) & LF_WIZARD)
    return 0;
  if ((L_FLAGS(lock) & LF_OWNER) && player != Owner(thing))
    return 0;
  if ((L_FLAGS(lock) & LF_LOCKED) && player != lock->creator &&
      Owner(player) != lock->creator)
    return 0;
  return 1;
}


static lock_list *
next_free_lock(const void *hint)
{
  if (lock_slab == NULL) {
    lock_slab = slab_create("locks", sizeof(lock_list));
    slab_set_opt(lock_slab, SLAB_ALLOC_BEST_FIT, 1);
  }
  return slab_malloc(lock_slab, hint);
}

static void
free_lock(lock_list *ll)
{
  slab_free(lock_slab, ll);
}

/** Given a lock type, find a lock, possibly checking parents.
 * \param thing object on which lock is to be found.
 * \param type type of lock to find.
 * \return pointer to boolexp of lock.
 */
boolexp
getlock(dbref thing, lock_type type)
{
  struct lock_list *ll = getlockstruct(thing, type);
  if (!ll)
    return TRUE_BOOLEXP;
  else
    return L_KEY(ll);
}

/** Given a lock type, find a lock without checking parents.
 * \param thing object on which lock is to be found.
 * \param type type of lock to find.
 * \return pointer to boolexp of lock.
 */
boolexp
getlock_noparent(dbref thing, lock_type type)
{
  struct lock_list *ll = getlockstruct_noparent(thing, type);
  if (!ll)
    return TRUE_BOOLEXP;
  else
    return L_KEY(ll);
}

lock_list *
getlockstruct(dbref thing, lock_type type)
{
  lock_list *ll;
  dbref p = thing, ancestor = NOTHING;
  int cmp, count = 0, ancestor_in_chain = 0;

  if (GoodObject(thing))
    ancestor = Ancestor_Parent(thing);
  do {
    for (; GoodObject(p); p = Parent(p)) {
      if (count++ > 100)
        return NULL;
      if (p == ancestor)
        ancestor_in_chain = 1;
      ll = Locks(p);
      while (ll && L_TYPE(ll)) {
        cmp = strcasecmp(L_TYPE(ll), type);
        if (cmp == 0)
          return (p != thing && (ll->flags & LF_PRIVATE)) ? NULL : ll;
        else if (cmp > 0)
          break;
        ll = ll->next;
      }
    }
    p = ancestor;
  } while (!ancestor_in_chain && !Orphan(thing) && GoodObject(ancestor));
  return NULL;
}

static lock_list *
getlockstruct_noparent(dbref thing, lock_type type)
{
  lock_list *ll = Locks(thing);
  int cmp;

  while (ll && L_TYPE(ll)) {
    cmp = strcasecmp(L_TYPE(ll), type);
    if (cmp == 0)
      return ll;
    else if (cmp > 0)
      break;
    ll = ll->next;
  }
  return NULL;
}


/** Determine if a lock type is one of the standard types or not.
 * \param type type of lock to check.
 * \return canonical lock type or NULL
 */
lock_type
match_lock(lock_type type)
{
  lock_list *ll;
  ll = hashfind(strupper(type), &htab_locks);
  if (ll)
    return ll->type;
  else
    return NULL;
}

/** Return the proper entry from lock_types, or NULL.
 * \param type of lock to look up.
 * \return lock_types entry for lock.
 */
const lock_list *
get_lockproto(lock_type type)
{
  return hashfind(strupper(type), &htab_locks);
}

/** Add a lock to an object (primitive).
 * Set the lock type on thing to boolexp.
 * This is a primitive routine, to be called by other routines.
 * It will go somewhat wonky if given a NULL boolexp.
 * It will allocate memory if called with a string that is not already
 * in the lock table.
 * \param player the enactor, for permission checking.
 * \param thing object on which to set the lock.
 * \param type type of lock to set.
 * \param key lock boolexp pointer (should not be NULL!).
 * \param flags lock flags.
 * \retval 0 failure.
 * \retval 1 success.
 */
int
add_lock(dbref player, dbref thing, lock_type type, boolexp key, privbits flags)
{
  lock_list *ll, **t;

  if (!GoodObject(thing)) {
    return 0;
  }

  ll = getlockstruct_noparent(thing, type);

  if (ll) {
    if (!can_write_lock(player, thing, ll)) {
      free_boolexp(key);
      return 0;
    }
    /* We're replacing an existing lock. */
    free_boolexp(ll->key);
    ll->key = key;
    ll->creator = player;
    if (flags != LF_DEFAULT)
      ll->flags = flags;
  } else {
    ll = next_free_lock(Locks(thing));
    if (!ll) {
      /* Oh, this sucks */
      do_log(LT_ERR, 0, 0, "Unable to malloc memory for lock_list!");
    } else {
      lock_type real_type = st_insert(type, &lock_names);
      ll->type = real_type;
      ll->key = key;
      ll->creator = player;
      if (flags == LF_DEFAULT) {
        const lock_list *l2 = get_lockproto(real_type);
        if (l2)
          ll->flags = l2->flags;
        else
          ll->flags = 0;
      } else {
        ll->flags = flags;
      }
      if (!can_write_lock(player, thing, ll)) {
        st_delete(real_type, &lock_names);
        free_boolexp(key);
        return 0;
      }
      t = &Locks(thing);
      while (*t && strcasecmp(L_TYPE(*t), L_TYPE(ll)) < 0)
        t = &L_NEXT(*t);
      L_NEXT(ll) = *t;
      *t = ll;
    }
  }
  return 1;
}

/** Add a lock to an object on db load.
 * Set the lock type on thing to boolexp.
 * Used only on db load, when we can't safely test the player's
 * permissions because they're not loaded yet.
 * This is a primitive routine, to be called by other routines.
 * It will go somewhat wonky if given a NULL boolexp.
 * It will allocate memory if called with a string that is not already
 * in the lock table.
 * \param player lock creator.
 * \param thing object on which to set the lock.
 * \param type type of lock to set.
 * \param key lock boolexp pointer (should not be NULL!).
 * \param flags lock flags.
 * \retval 0 failure.
 */
int
add_lock_raw(dbref player, dbref thing, lock_type type, boolexp key,
             privbits flags)
{
  lock_list *ll, **t;
  lock_type real_type = type;

  if (!GoodObject(thing)) {
    return 0;
  }

  ll = next_free_lock(Locks(thing));
  if (!ll) {
    /* Oh, this sucks */
    do_log(LT_ERR, 0, 0, "Unable to malloc memory for lock_list!");
  } else {
    real_type = st_insert(type, &lock_names);
    ll->type = real_type;
    ll->key = key;
    ll->creator = player;
    if (flags == LF_DEFAULT) {
      const lock_list *l2 = get_lockproto(real_type);
      if (l2)
        ll->flags = l2->flags;
      else
        ll->flags = 0;
    } else {
      ll->flags = flags;
    }
    t = &Locks(thing);
    while (*t && strcasecmp(L_TYPE(*t), L_TYPE(ll)) < 0)
      t = &L_NEXT(*t);
    L_NEXT(ll) = *t;
    *t = ll;
  }
  return 1;
}

/* Very primitive. */
static void
free_one_lock_list(lock_list *ll)
{
  if (ll == NULL)
    return;
  free_boolexp(ll->key);
  st_delete(ll->type, &lock_names);
  free_lock(ll);
}

/** Delete a lock from an object (primitive).
 * Another primitive routine.
 * \param player the enactor, for permission checking.
 * \param thing object on which to remove the lock.
 * \param type type of lock to remove.
 */
int
delete_lock(dbref player, dbref thing, lock_type type)
{
  lock_list *ll, **llp;
  if (!GoodObject(thing)) {
    return 0;
  }
  llp = &(Locks(thing));
  while (*llp && strcasecmp((*llp)->type, type) != 0) {
    llp = &((*llp)->next);
  }
  if (*llp != NULL) {
    if (can_write_lock(player, thing, *llp)) {
      ll = *llp;
      *llp = ll->next;
      free_one_lock_list(ll);
      return 1;
    } else
      return 0;
  } else
    return 1;
}

/** Free all locks in a list.
 * Used by the object destruction routines.
 * \param ll pointer to list of locks.
 */
void
free_locks(lock_list *ll)
{
  lock_list *ll2;
  while (ll) {
    ll2 = ll->next;
    free_one_lock_list(ll);
    ll = ll2;
  }
}


/** Check to see that the lock type is a valid type.
 * If it's not in our lock table, it's not valid,
 * unless it begins with 'user:' or an abbreviation thereof,
 * in which case the lock type is the part after the :.
 * As an extra check, we don't allow '|' in lock names because it
 * will confuse our db-reading routines.
 *
 *
 * \param player the enactor, for notification.
 * \param thing object on which to check the lock.
 * \param name name of lock type.
 * \return lock type, or NULL.
 */
static lock_type
check_lock_type(dbref player, dbref thing, lock_type name)
{
  lock_type ll;
  char user_name[BUFFER_LEN];
  char *colon;

  /* Special-case for basic locks. */
  if (!name || !*name)
    return Basic_Lock;

  /* Normal locks. */
  ll = match_lock(name);
  if (ll != NULL)
    return ll;

  /* If the lock is set, it's allowed, whether it exists normally or not. */
  if (getlock(thing, name) != TRUE_BOOLEXP)
    return name;
  /* Check to see if it's a well-formed user-defined lock. */

  if (!string_prefix(name, "User:")) {
    notify(player, T("Unknown lock type."));
    return NULL;
  }
  if (strchr(name, '|')) {
    notify(player, T("The character \'|\' may not be used in lock names."));
    return NULL;
  }
  mush_strncpy(user_name, name, BUFFER_LEN);
  colon = strchr(user_name, ':');
  if (colon)                    /* Should always be true */
    *colon = '\0';

  if (!good_atr_name(user_name)) {
    notify(player, T("That is not a valid lock name."));
    return NULL;
  }

  return strchr(name, ':') + 1;
}

/** Unlock a lock (user interface).
 * \verbatim
 * This implements @unlock.
 * \endverbatim
 * \param player the enactor.
 * \param name name of object to unlock.
 * \param type type of lock to unlock.
 */
void
do_unlock(dbref player, const char *name, lock_type type)
{
  dbref thing;
  lock_type real_type;

  /* check for '@unlock <object>/<atr>'  */
  if (strchr(name, '/')) {
    do_atrlock(player, name, "off");
    return;
  }
  if ((thing = match_controlled(player, name)) != NOTHING) {
    if ((real_type = check_lock_type(player, thing, type)) != NULL) {
      if (getlock(thing, real_type) == TRUE_BOOLEXP) {
        if (!AreQuiet(player, thing))
          notify_format(player, T("%s(%s) - %s (already) unlocked."),
                        Name(thing), unparse_dbref(thing), real_type);
      } else if (delete_lock(player, thing, real_type)) {
        if (!AreQuiet(player, thing))
          notify_format(player, T("%s(%s) - %s unlocked."), Name(thing),
                        unparse_dbref(thing), real_type);
        if (!IsPlayer(thing))
          ModTime(thing) = mudtime;
      } else
        notify(player, T("Permission denied."));
    }
  }
}

/** Set/lock a lock (user interface).
 * \verbatim
 * This implements @lock.
 * \endverbatim
 * \param player the enactor.
 * \param name name of object to lock.
 * \param keyname key to lock the lock to, as a string.
 * \param type type of lock to lock.
 */
void
do_lock(dbref player, const char *name, const char *keyname, lock_type type)
{
  lock_type real_type;
  dbref thing;
  boolexp key;

  /* check for '@lock <object>/<atr>'  */
  if (strchr(name, '/')) {
    do_atrlock(player, name, "on");
    return;
  }
  if (!keyname || !*keyname) {
    do_unlock(player, name, type);
    return;
  }
  switch (thing = match_result(player, name, NOTYPE, MAT_EVERYTHING)) {
  case NOTHING:
    notify(player, T("I don't see what you want to lock!"));
    return;
  case AMBIGUOUS:
    notify(player, T("I don't know which one you want to lock!"));
    return;
  default:
    if (!controls(player, thing)) {
      notify(player, T("You can't lock that!"));
      return;
    }
    if (IsGarbage(thing)) {
      notify(player, T("Why would you want to lock garbage?"));
      return;
    }
    break;
  }

  key = parse_boolexp(player, keyname, type);

  /* do the lock */
  if (key == TRUE_BOOLEXP) {
    notify(player, T("I don't understand that key."));
  } else {
    if ((real_type = check_lock_type(player, thing, type)) != NULL) {
      /* everything ok, do it */
      if (add_lock(player, thing, real_type, key, LF_DEFAULT)) {
        if (!AreQuiet(player, thing))
          notify_format(player, T("%s(%s) - %s locked."), Name(thing),
                        unparse_dbref(thing), real_type);
        if (!IsPlayer(thing))
          ModTime(thing) = mudtime;
      } else {
        notify(player, T("Permission denied."));
        /*  Done by a failed add_lock()  // free_boolexp(key); */
      }
    } else
      free_boolexp(key);
  }
}

/** Copy the locks from one object to another.
 * \param player the enactor.
 * \param orig the source object.
 * \param clone the destination object.
 */
void
clone_locks(dbref player, dbref orig, dbref clone)
{
  lock_list *ll;
  for (ll = Locks(orig); ll; ll = ll->next) {
    if (!(L_FLAGS(ll) & LF_NOCLONE))
      add_lock(player, clone, L_TYPE(ll), dup_bool(L_KEY(ll)), L_FLAGS(ll));
  }
}


/** Evaluate a lock.
 * Evaluate lock ltype on thing for player.
 * \param player dbref attempting to pass the lock.
 * \param thing object containing the lock.
 * \param ltype type of lock to check.
 * \param pe_info the pe_info to use when evaluating softcode in the lock
 * \retval 1 player passes the lock.
 * \retval 0 player does not pass the lock.
 */
int
eval_lock_with(dbref player, dbref thing, lock_type ltype, NEW_PE_INFO *pe_info)
{
  boolexp b = getlock(thing, ltype);
  /* Prevent overwriting a static buffer in unparse_boolexp() */
  if (!unparsing_boolexp)
    log_activity(LA_LOCK, thing, unparse_boolexp(player, b, UB_DBREF));
  return eval_boolexp(player, b, thing, pe_info);
}

/* eval_lock(player,thing,ltype) is #defined to eval_lock_with(player,thing,ltype,NULL) */

/** Evaluate a lock, saving/clearing the env (%0-%9) and qreg (%q*) first,
 ** and restoring them after.
 */
int
eval_lock_clear(dbref player, dbref thing, lock_type ltype,
                NEW_PE_INFO *pe_info)
{
  if (!pe_info)
    return eval_lock_with(player, thing, ltype, NULL);
  else {
    PE_REGS *pe_regs;
    int result;

    pe_regs = pe_regs_localize(pe_info, PE_REGS_ISOLATE, "eval_lock_clear");

    /* Run the lock */
    result = eval_lock_with(player, thing, ltype, pe_info);

    /* Restore q-regs */
    pe_regs_restore(pe_info, pe_regs);
    pe_regs_free(pe_regs);
    return result;
  }
}

/** Active a lock's failure attributes.
 * \param player dbref failing to pass the lock.
 * \param thing object containing the lock.
 * \param ltype type of lock failed.
 * \param def default message if there is no appropriate failure attribute.
 * \param loc location in which action is taking place.
 * \retval 1 some attribute on the object was actually evaluated.
 * \retval 0 no attributes were evaluated (only defaults used).
 */
int
fail_lock(dbref player, dbref thing, lock_type ltype, const char *def,
          dbref loc)
{
  const LOCKMSGINFO *lm;
  char atr[BUFFER_LEN];
  char oatr[BUFFER_LEN];
  char aatr[BUFFER_LEN];
  char realdef[BUFFER_LEN];
  char *bp;

  if (def)
    strcpy(realdef, def);       /* Because a lot of default msgs use tprintf */
  else
    realdef[0] = '\0';

  /* Find the lock's failure attribute, if it's there */
  for (lm = lock_msgs; lm->type; lm++) {
    if (!strcmp(lm->type, ltype))
       break;
  }
  if (lm->type) {
    strcpy(atr, lm->failbase);
    bp = oatr;
    safe_format(oatr, &bp, "O%s", lm->failbase);
    *bp = '\0';
    strcpy(aatr, oatr);
    aatr[0] = 'A';
  } else {
    /* Oops, it's not in the table. So we construct them on these lines:
     * <LOCKNAME>_LOCK`<type>FAILURE
     */
    bp = atr;
    safe_format(atr, &bp, "%s_LOCK`FAILURE", ltype);
    *bp = '\0';
    bp = oatr;
    safe_format(oatr, &bp, "%s_LOCK`OFAILURE", ltype);
    *bp = '\0';
    bp = aatr;
    safe_format(aatr, &bp, "%s_LOCK`AFAILURE", ltype);
    *bp = '\0';
  }
  /* Now do the work */
  upcasestr(atr);
  upcasestr(oatr);
  upcasestr(aatr);
  return did_it(player, thing, atr, realdef, oatr, NULL, aatr, loc);
}


/** Determine if a lock is visual.
 * \param thing object containing the lock.
 * \param ltype type of lock to check.
 * \retval (non-zero) lock is visual.
 * \retval 0 lock is not visual.
 */
bool
lock_visual(dbref thing, lock_type ltype)
{
  lock_list *l = getlockstruct(thing, ltype);
  if (l)
    return l->flags & LF_VISUAL;
  else
    return 0;
}

/** Set flags on a lock (user interface).
 * \verbatim
 * This implements @lset.
 * \endverbatim
 * \param player the enactor.
 * \param what string in the form obj/lock.
 * \param flags list of flags to set.
 */
void
do_lset(dbref player, char *what, char *flags)
{
  dbref thing;
  lock_list *l;
  char *lname;
  privbits flag;
  bool unset = 0;

  if ((lname = strchr(what, '/')) == NULL) {
    notify(player, T("No lock name given."));
    return;
  }
  *lname++ = '\0';

  if ((thing = match_controlled(player, what)) == NOTHING)
    return;

  if (*flags == '!') {
    unset = 1;
    flags++;
  }

  if (string_to_lockflag(player, flags, &flag) < 0) {
    notify(player, T("Unrecognized lock flag."));
    return;
  }

  l = getlockstruct_noparent(thing, lname);
  if (!l || !Can_Read_Lock(player, thing, L_TYPE(l))) {
    notify(player, T("No such lock."));
    return;
  }

  if (!can_write_lock(player, thing, l)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (unset)
    L_FLAGS(l) &= ~flag;
  else
    L_FLAGS(l) |= flag;

  if (!Quiet(player) && !(Quiet(thing) && (Owner(thing) == player)))
    notify_format(player, "%s/%s - %s.", Name(thing), L_TYPE(l),
                  unset ? T("lock flags unset") : T("lock flags set"));
  if (!IsPlayer(thing))
    ModTime(thing) = mudtime;
}

/** Check to see if an object has a good zone lock set.
 * If it doesn't have a lock at all, set one of '=Zone'.
 * \param player The object responsible for having the lock checked.
 * \param zone the object whose lock needs to be checked.
 * \param noisy if 1, notify player of automatic locking
 */
void
check_zone_lock(dbref player, dbref zone, int noisy)
{
  boolexp key = getlock(zone, Zone_Lock);
  if (key == TRUE_BOOLEXP) {
    add_lock(GOD, zone, Zone_Lock, parse_boolexp(zone, "=me", Zone_Lock),
             LF_DEFAULT);
    if (noisy) {
      notify_format(player,
                    T
                    ("Unlocked zone %s - automatically zone-locking to itself"),
                    unparse_object(player, zone));
    }
  } else if (!noisy) {
    return;
  } else if (eval_lock(Location(player), zone, Zone_Lock)) {
    /* Does #0 and #2 pass it? If so, probably trivial elock */
    if (eval_lock(PLAYER_START, zone, Zone_Lock) &&
        eval_lock(MASTER_ROOM, zone, Zone_Lock)) {
      notify_format(player,
                    T("Zone %s really should have a more secure zone-lock."),
                    unparse_object(player, zone));
    } else {
      /* Probably inexact zone lock */
      notify_format(player,
                    T
                    ("Warning: Zone %s may have loose zone lock. Lock zones to =player, not player"),
                    unparse_object(player, zone));
    }
  }
}

void
purge_locks(void)
{
  dbref thing;

  for (thing = 0; thing < db_top; thing++) {
    lock_list *ll;
    for (ll = Locks(thing); ll; ll = L_NEXT(ll))
      L_KEY(ll) = cleanup_boolexp(L_KEY(ll));
  }
}
