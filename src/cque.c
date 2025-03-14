/**
 * \file cque.c
 *
 * \brief Queue for PennMUSH.
 *
 *
 */

#include "copyrite.h"

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_SSE2
#include <emmintrin.h>
#endif

#ifdef HAVE_SSSE3
#include <tmmintrin.h>
#endif

#include "ansi.h"
#include "attrib.h"
#include "case.h"
#include "command.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "flags.h"
#include "function.h"
#include "game.h"
#include "intmap.h"
#include "log.h"
#include "match.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "parse.h"
#include "ptab.h"
#include "strtree.h"
#include "strutil.h"
#include "mushsql.h"

intmap *queue_map = NULL; /**< Intmap for looking up queue entries by pid */
static uint32_t top_pid = 1;
#define MAX_PID (1U << 15)

static MQUE *qfirst = NULL, *qlast = NULL, *qwait = NULL;
static MQUE *qsemfirst = NULL, *qsemlast = NULL;

static int add_to_generic(dbref player, int am, const char *name,
                          uint32_t flags);
static int add_to(dbref player, int am);
static int add_to_sem(dbref player, int am, const char *name);
static int queue_limit(dbref player);
void free_qentry(MQUE *point);
static int pay_queue(dbref player, const char *command);
void wait_que(dbref executor, int waittill, char *command, dbref enactor,
              dbref sem, const char *semattr, int until, MQUE *parent_queue);
int que_next(void);

static void show_queue(dbref player, dbref victim, int q_type, int q_quiet,
                       int q_all, MQUE *q_ptr, int *tot, int *self, int *del);
static void show_queue_single(dbref player, MQUE *q, int q_type);
static void show_queue_env(dbref player, MQUE *q);
static void do_raw_restart(dbref victim);
static int waitable_attr(dbref thing, const char *atr);
static void shutdown_a_queue(MQUE **head, MQUE **tail);
static int do_entry(MQUE *entry, int include_recurses);
static MQUE *new_queue_entry(NEW_PE_INFO *pe_info);
void init_queue(void);
int execute_one_semaphore(dbref thing, char const *aname, PE_REGS *pe_regs);

/* Keep track of the last 15 minutes worth of queue activity per second */
enum { QUEUE_LOAD_SECS = 900 };
int32_t queue_load_record[QUEUE_LOAD_SECS]
  __attribute__((__aligned__(16))) = {0};

double average32(const int32_t *arr, int count);

/* From game.c, for report() */
extern char report_cmd[BUFFER_LEN];
extern dbref report_dbref;

/** Attribute flags to be set or checked on attributes to be used
 * as semaphores.
 */
#define SEMAPHORE_FLAGS (AF_LOCKED | AF_PRIVATE | AF_NOCOPY | AF_NODUMP)

/** Queue initializtion function. Must be called before anything
 * is added to the queue.
 */
void
init_queue(void)
{
  queue_map = im_new();
}

/** Returns true if the attribute on thing can be used as a semaphore.
 * atr should be given in UPPERCASE.
 */
static int
waitable_attr(dbref thing, const char *atr)
{
  ATTR *a;
  if (!atr || !*atr)
    return 0;
  a = atr_get_noparent(thing, atr);
  if (!a) { /* Attribute isn't set */
    a = atr_match(atr);
    if (!a) /* It's not a built in attribute */
      return 1;
    return !strcmp(AL_NAME(a), "SEMAPHORE"); /* Only allow SEMAPHORE for now */
  } else { /* Attribute is set. Check for proper owner and flags and value */
    if ((AL_CREATOR(a) == GOD) && (AL_FLAGS(a) == SEMAPHORE_FLAGS)) {
      char *v = atr_value(a);
      if (!*v || is_strict_integer(v))
        return 1;
      else
        return 0;
    } else {
      return 0;
    }
  }
  return 0; /* Not reached */
}

/** Incrememt an integer attribute.
 * \param player the object the attribute is on
 * \param am the amount to incrememnt by
 * \param name the name of the attribute to increment
 * \param flags the attribute flags to set on the attr
 * \retval the new value of the attribute
 */
static int
add_to_generic(dbref player, int am, const char *name, uint32_t flags)
{
  int num = 0;
  ATTR *a;
  char buff[MAX_COMMAND_LEN];
  a = atr_get_noparent(player, name);
  if (a)
    num = parse_integer(atr_value(a));
  num += am;
  /* We set the attribute's value to 0 even if we're going to clear
   * it later, because clearing it may fail (perhaps someone's also
   * foolishly using it as a branch in an attribute tree)
   */
  snprintf(buff, sizeof buff, "%d", num);
  (void) atr_add(player, name, buff, GOD, flags);
  if (!num) {
    (void) atr_clr(player, name, GOD);
  }
  return num;
}

/** Wrapper for add_to_generic() to incremement a player's QUEUE attribute.
 * \param player object whose QUEUE should be incremented
 * \param am amount to increment the QUEUE by
 * \retval new value of QUEUE
 */
static int
add_to(dbref player, int am)
{
  sqlite3 *sqldb;
  sqlite3_stmt *adder;
  int status;
  int newam = -1;

  if (QUEUE_PER_OWNER) {
    player = Owner(player);
  }

  sqldb = get_shared_db();
  adder = prepare_statement(
    sqldb, "UPDATE objects SET queue = queue + ? WHERE dbref = ? RETURNING queue",
    "queue.add");
  sqlite3_bind_int(adder, 1, am);
  sqlite3_bind_int(adder, 2, player);

  do {
    status = sqlite3_step(adder);
    if (status == SQLITE_ROW) {
      newam = sqlite3_column_int(adder, 0);
    }
  } while (status == SQLITE_ROW || is_busy_status(status));
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Unable to update queue for #%d: %s", player,
              sqlite3_errstr(status));
    newam = -1;
  }
  sqlite3_reset(adder);
  return newam;
}

/** Wrapper for add_to_generic() to incrememnt an attribute when a
 * semaphore is queued.
 * \param player object whose attribute should be incremented
 * \param am amount to increment the attr by
 * \param name attr to increment, or NULL to use the default (SEMAPHORE)
 * \retval new value of attr
 */
static int
add_to_sem(dbref player, int am, const char *name)
{
  return add_to_generic(player, am, name ? name : "SEMAPHORE", SEMAPHORE_FLAGS);
}

/** Increment an object's queue by 1, and then return 1 if he has exceeded his
 * queue limit.
 * \param player objects whose queue should be incremented
 * \retval 1 player has exceeded his queue limit
 * \retval 0 player has not exceeded his queue limit
 */
static int
queue_limit(dbref player)
{
  int nlimit;

  nlimit = add_to(player, 1);
  if (HugeQueue(player))
    return nlimit > (QUEUE_QUOTA + db_top);
  else
    return nlimit > QUEUE_QUOTA;
}

/** Free a queue entry.
 * \param entry queue entry to free.
 */
void
free_qentry(MQUE *entry)
{
  MQUE *tmp;

  if (entry->inplace) {
    tmp = entry->inplace;
    entry->inplace = NULL;
    free_qentry(tmp);
  }

  if (entry->next && (entry->queue_type & QUEUE_INPLACE)) {
    tmp = entry->next;
    entry->next = NULL;
    free_qentry(tmp);
  }

  if (entry->action_list) {
    mush_free(entry->action_list, "mque.action_list");
    entry->action_list = NULL;
  }

  if (entry->semaphore_attr) {
    mush_free(entry->semaphore_attr, "mque.semaphore_attr");
    entry->semaphore_attr = NULL;
  }

  free_pe_info(entry->pe_info);

  /* Shouldn't happen, but to be safe... */
  if (entry->save_attrname) {
    mush_free(entry->save_attrname, "string");
  }

  if (entry->pid) { /* INPLACE queue entries have no pid */
    im_delete(queue_map, entry->pid);
  }

  if (entry->regvals) { /* Nested pe_regs */
    pe_regs_free(entry->regvals);
  }

  mush_free(entry, "mque");
}

static int
pay_queue(dbref player, const char *command)
{
  int estcost;
  estcost =
    QUEUE_COST +
    (QUEUE_LOSS ? ((get_random_u32(0, QUEUE_LOSS - 1) == 0) ? 1 : 0) : 0);
  if (!quiet_payfor(player, estcost)) {
    notify_format(Owner(player),
                  T("Not enough money to queue command for %s(#%d)."),
                  AName(player, AN_SYS, NULL), player);
    return 0;
  }
  if (!NoPay(player) && (estcost != QUEUE_COST) && Track_Money(Owner(player))) {
    notify_format(Owner(player),
                  T("GAME: Object %s(%s) lost a %s to queue loss."),
                  AName(player, AN_SYS, NULL), unparse_dbref(player), MONEY);
  }
  if (queue_limit(QUEUE_PER_OWNER ? Owner(player) : player)) {
    notify_format(Owner(player), T("Runaway object: %s(%s). Commands halted."),
                  AName(player, AN_SYS, NULL), unparse_dbref(player));
    do_log(LT_TRACE, player, player, "Runaway object %s executing: %s",
           unparse_dbref(player), command);
    /* Refund the queue costs */
    giveto(player, QUEUE_COST);
    add_to(player, -1);
    /* wipe out that object's queue and set it HALT */
    do_halt(Owner(player), "", player);
    set_flag_internal(player, "HALT");
    return 0;
  }
  return 1;
}

static uint32_t
next_pid(void)
{
  uint32_t pid = top_pid;

  if (im_count(queue_map) >= (int) MAX_PID) {
    do_rawlog(
      LT_ERR,
      "There are %ld queue entries! That's too many. Failing to add another.",
      (long) im_count(queue_map));
    return 0;
  }

  while (1) {
    if (pid > MAX_PID)
      pid = 1;
    if (im_exists(queue_map, pid))
      pid++;
    else {
      top_pid = pid + 1;
      return pid;
    }
  }
}

static MQUE *
new_queue_entry(NEW_PE_INFO *pe_info)
{
  MQUE *entry;

  entry = mush_malloc(sizeof *entry, "mque");
  if (!entry)
    mush_panic("Unable to allocate memory in new_queue_entry");
  entry->executor = NOTHING;
  entry->enactor = NOTHING;
  entry->caller = NOTHING;

  if (pe_info)
    entry->pe_info = pe_info;
  else
    entry->pe_info = make_pe_info("pe_info-new_queue_entry");

  entry->inplace = NULL;
  entry->next = NULL;

  entry->semaphore_obj = NOTHING;
  entry->semaphore_attr = NULL;
  entry->wait_until = 0;
  entry->pid = 0;
  entry->action_list = NULL;
  entry->queue_type = QUEUE_DEFAULT;
  entry->port = 0;
  entry->save_attrname = NULL;
  entry->regvals = NULL;

  return entry;
}

/** A non-printing char used internally to delimit args for events during
 * arg parsing
 */
#define EVENT_DELIM_CHAR '\x11'

/** If EVENT_HANDLER config option is set to a valid dbref, try triggering
 * its handler attribute
 * \param enactor The enactor who caused it.
 * \param event The event. No spaces, only alphanumerics and dashes.
 * \param fmt A comma-deliminated string defining printf-style args.
 * \param ... The args passed to argstring.
 * \retval 1 The event had a handler attribute.
 * \retval 0 No event handler or no attribute for the given event.
 */
bool
queue_event(dbref enactor, const char *event, const char *fmt, ...)
{
  char myfmt[BUFFER_LEN];
  char buff[BUFFER_LEN * 4];
  va_list args;
  char *s, *snext;
  ATTR *a;
  PE_REGS *pe_regs;
  int argcount = 0;
  char *wenv[MAX_STACK_ARGS];
  int i, len;
  MQUE *tmp;
  int pid;

  /* Make sure we have an event to call, first. */
  if (!GoodObject(EVENT_HANDLER) || IsGarbage(EVENT_HANDLER) ||
      Halted(EVENT_HANDLER)) {
    return 0;
  }

  /* <0 means system event, -1. Just in case, this also covers
   * Garbage and !GoodObject enactors. */
  if (!GoodObject(enactor) || IsGarbage(enactor)) {
    enactor = -1;
  }

  a = atr_get_noparent(EVENT_HANDLER, event);
  if (!(a && AL_STR(a) && *AL_STR(a))) {
    /* Nonexistant or empty attrib. */
    return 0;
  }

  /* Because Event is so easy to run away. */
  if (!pay_queue(EVENT_HANDLER, event)) {
    return 0;
  }

  /* Fetch the next available pid. */
  pid = next_pid();
  if (pid == 0) {
    /* Too many queue entries */
    notify(Owner(EVENT_HANDLER), T("Queue entry table full. Try again later."));
    return 0;
  }

  /* We have an event to call. Yay! */
  for (i = 0; i < MAX_STACK_ARGS; i++)
    wenv[i] = NULL;

  /* Prep myfmt: Replace all commas with delim chars. */
  snprintf(myfmt, BUFFER_LEN, "%s", fmt);
  s = myfmt;

  if (*s)
    argcount++; /* At least one arg. */
  while ((s = strchr(s, ',')) != NULL) {
    *(s++) = EVENT_DELIM_CHAR;
    argcount++;
  }

  /* Maximum number of args available (%0-%9 stack) */
  if (argcount > MAX_STACK_ARGS)
    argcount = MAX_STACK_ARGS;

  if (argcount > 0) {
    /* Build the arguments. */
    va_start(args, fmt);
    mush_vsnprintf(buff, sizeof buff, myfmt, args);
    buff[(BUFFER_LEN * 4) - 1] = '\0';
    va_end(args);

    len = strlen(buff);
    for (i = 0, s = buff; i < argcount && s; i++, s = snext) {
      snext = strchr(s, EVENT_DELIM_CHAR);
      if ((snext ? (snext - s) : (len - (s - buff))) > BUFFER_LEN) {
        /* It's theoretically possible to have an arg that's longer than
         * BUFFER_LEN */
        s[BUFFER_LEN - 1] = '\0';
      }
      if (snext) {
        *(snext++) = '\0';
      }
      wenv[i] = s;
    }
  }

  /* Let's queue this mother. */

  /* Build tmp. */
  tmp = new_queue_entry(NULL);
  tmp->pid = pid;
  tmp->executor = EVENT_HANDLER;
  tmp->enactor = enactor;
  tmp->caller = enactor;
  tmp->queue_type |= QUEUE_EVENT;

  /* safe_atr_value returns a mush_strdup'd buffer, which is freed in
   * free_qentry */
  tmp->action_list = safe_atr_value(a, "mque.action_list");

  /* Set up %0-%9 */
  if (tmp->pe_info->regvals == NULL) {
    tmp->pe_info->regvals = pe_regs_create(PE_REGS_QUEUE, "queue_event");
  }
  pe_regs = tmp->pe_info->regvals;
  for (i = 0; i < MAX_STACK_ARGS; i++) {
    if (wenv[i]) {
      pe_regs_setenv(pe_regs, i, wenv[i]);
    }
  }

  /* Hmm, should events queue ahead of anything else?
   * For now, yes, but leaving code here anyway.
   */
  if (qlast) {
    qlast->next = tmp;
    qlast = tmp;
  } else {
    qlast = qfirst = tmp;
  }

  /* All good! */
  im_insert(queue_map, tmp->pid, tmp);

  return 1;
}

/** Add a new queue entry: Either in place, or onto the player/object queues
 * This function adds a new entry to the back of the player or
 * object command queues (depending on whether the call was
 * caused by a player or an object).
 * \param queue_entry the queue entry to insert
 * \param parent_queue the parent queue entry, used for placing inplace queues,
 * or NULL
 */
void
insert_que(MQUE *queue_entry, MQUE *parent_queue)
{
  if (!IsPlayer(queue_entry->executor) && (Halted(queue_entry->executor))) {
    free_qentry(queue_entry);
    return;
  }

  if (queue_entry->queue_type & QUEUE_INPLACE && !parent_queue) {
    /* ruh-roh. Can't run inplace if we don't have a parent queue. But this
       should never happen. We can either just quit now, or we can queue it
       instead of running inplace. For now, let's just quit. */
    free_qentry(queue_entry);
    return;
  }

  if (!(queue_entry->queue_type & QUEUE_INPLACE)) {
    if (!pay_queue(queue_entry->executor, queue_entry->action_list)) {
      /* make sure player can afford to do it */
      free_qentry(queue_entry);
      return;
    }
    queue_entry->pid = next_pid();
    if (queue_entry->pid == 0) {
      /* Too many queue entries */
      /* Should this be notifying the enactor instead? */
      notify(queue_entry->executor,
             T("Queue entry table full. Try again later."));
      free_qentry(queue_entry);
      return;
    }
  }

  switch (
    (queue_entry->queue_type & (QUEUE_PLAYER | QUEUE_OBJECT | QUEUE_INPLACE))) {
  case QUEUE_PLAYER:
  case QUEUE_OBJECT:
    if (qlast) {
      qlast->next = queue_entry;
      qlast = queue_entry;
    } else {
      qlast = qfirst = queue_entry;
    }
    break;
  case QUEUE_INPLACE:
    if (parent_queue->inplace) {
      MQUE *tmp;
      tmp = parent_queue->inplace;
      while (tmp->next)
        tmp = tmp->next;
      tmp->next = queue_entry;
    } else {
      parent_queue->inplace = queue_entry;
    }
    break;
  default:
    /* Oops. This shouldn't happen; make sure we don't leave
     * a queue entry in limbo... */
    do_rawlog(LT_ERR, "Queue entry with invalid type!");
    free_qentry(queue_entry);
    return;
  }
  if (queue_entry->pid)
    im_insert(queue_map, queue_entry->pid, queue_entry);
}

/** Replacement for parse_que and inplace_queue_actionlist - queue an action
 * list
 *
 * Queue the given actionlist for executor to run.
 * \param executor object queueing the action list
 * \param enactor object which caused the action list to queue
 * \param caller for \@force/inplace queues, the object using \@force.
 * Otherwise, same as enactor.
 * \param actionlist the actionlist of cmds to queue
 * \param parent_queue the parent queue entry which caused this queueing, or
 * NULL
 * \param flags a bitwise collection of PE_INFO_* flags that determine the
 * environment for the new queue
 * \param queue_type bitwise collection of the QUEUE_* flags, to determine which
 * queue this goes in
 * \param pe_regs the pe_regs for the queue entry
 * \param fromattr The attribute the actionlist is queued from
 */
void
new_queue_actionlist_int(dbref executor, dbref enactor, dbref caller,
                         char *actionlist, MQUE *parent_queue, int flags,
                         int queue_type, PE_REGS *pe_regs, char *fromattr)
{
  NEW_PE_INFO *pe_info;
  MQUE *queue_entry;

  if (!(queue_type & QUEUE_INPLACE)) {
    /* Check the object isn't halted */
    if (!IsPlayer(executor) && Halted(executor)) {
      return;
    }

    /* Remove all QUEUE_* flags which aren't safe for non-inplace queues */
    queue_type =
      (queue_type & (QUEUE_NODEBUG | QUEUE_DEBUG | QUEUE_DEBUG_PRIVS |
                     QUEUE_NOLIST | QUEUE_PRIORITY));
    queue_type |= (((GoodObject(enactor) && IsPlayer(enactor)) ||
                    (queue_type & QUEUE_PRIORITY))
                     ? QUEUE_PLAYER
                     : QUEUE_OBJECT);
    if (flags & PE_INFO_SHARE) {
      /* You can only share the pe_info for an inplace queue entry. Since you've
       * asked us
       * to share for a fully queued entry, you're an idiot, because it will
       * crash. I know;
       * I'm an idiot too, and it crashed when I did it as well. I'll fix it for
       * you; aren't
       * you lucky? */
      do_rawlog(LT_ERR,
                "Attempt to create a non-inplace queue entry using a "
                "shared pe_info by #%d from %s",
                executor,
                (fromattr ? fromattr : "the socket, or an unknown attribute"));
      flags = PE_INFO_CLONE; /* The closest we can come to what you asked for */
    }
  }

  pe_info =
    pe_info_from((parent_queue ? parent_queue->pe_info : NULL), flags, pe_regs);

  queue_entry = new_queue_entry(pe_info);
  queue_entry->executor = executor;
  queue_entry->enactor = enactor;
  queue_entry->caller = caller;
  queue_entry->action_list = mush_strdup(actionlist, "mque.action_list");
  queue_entry->queue_type = queue_type;
  if (pe_regs && (flags & PE_INFO_SHARE)) {
    queue_entry->regvals = pe_regs;
    /* We used to do this instead, but we actually don't need to, and it
     * leads to errors with nested inplace queues.
     queue_entry->regvals =
     pe_regs_create(pe_regs->flags, "new_queue_actionlist");
     pe_regs_copystack(queue_entry->regvals, pe_regs, PE_REGS_QUEUE, 0);
     */
  }

  if (fromattr) {
    if (queue_type & QUEUE_INPLACE && pe_info->attrname) {
      queue_entry->save_attrname = mush_strdup(pe_info->attrname, "string");
    }
    if (queue_entry->pe_info->attrname) {
      mush_free(queue_entry->pe_info->attrname, "string");
    }
    queue_entry->pe_info->attrname = mush_strdup(fromattr, "string");
  }

  if (parent_queue && (parent_queue->queue_type & QUEUE_EVENT))
    queue_type |= QUEUE_EVENT;

  insert_que(queue_entry, parent_queue);
}

void
parse_que_attr(dbref executor, dbref enactor, char *actionlist,
               PE_REGS *pe_regs, ATTR *a, bool force_debug)
{
  int flags = QUEUE_DEFAULT;
  char abuff[2048];

  if (force_debug) {
    flags |= QUEUE_DEBUG;
  } else if (AF_NoDebug(a)) {
    flags |= QUEUE_NODEBUG;
  } else if (AF_Debug(a)) {
    flags |= QUEUE_DEBUG;
  }

  snprintf(abuff, sizeof abuff, "#%d/%s", executor, AL_NAME(a));
  new_queue_actionlist_int(executor, enactor, enactor, actionlist, NULL,
                           PE_INFO_DEFAULT, flags, pe_regs, abuff);
}

int
queue_include_attribute(dbref thing, const char *atrname, dbref executor,
                        dbref enactor, dbref caller, char **args,
                        int queue_type, MQUE *parent_queue)
{
  ATTR *a;
  char *start, *command;
  int noparent = 0;
  PE_REGS *pe_regs = NULL;
  int i;
  char abuff[2048];

  a = queue_attribute_getatr(thing, atrname, noparent);
  if (!a)
    return 0;
  if (!Can_Read_Attr(executor, thing, a))
    return 0;

  start = safe_atr_value(a, "atrval.queue-attr");
  command = start;
  /* Trim off $-command or ^-command prefix */
  if (*command == '$' || *command == '^') {
    command = strchr_unescaped(command, ':');
    if (!command)
      /* Oops, had '$' or '^', but no ':' */
      command = start;
    else
      /* Skip the ':' */
      command++;
  }
  pe_regs = pe_regs_create(PE_REGS_NEWATTR, "queue_include_attribute");
  if (args != NULL) {
    pe_regs->flags |= PE_REGS_ARG;
    for (i = 0; i < MAX_STACK_ARGS; i++) {
      if (args[i] && *args[i]) {
        pe_regs_setenv(pe_regs, i, args[i]);
      }
    }
  } else {
    pe_regs->flags |= PE_REGS_ARGPASS;
  }

  if (AF_NoDebug(a)) {
    queue_type |= QUEUE_NODEBUG;
  } else if (AF_Debug(a)) {
    queue_type |= QUEUE_DEBUG;
  } else {
    /* Inherit debug style from parent queue */
    queue_type |= (parent_queue->queue_type & (QUEUE_DEBUG | QUEUE_NODEBUG));
  }
  if (parent_queue->queue_type & QUEUE_EVENT) {
    queue_type |= QUEUE_EVENT;
  }

  snprintf(abuff, sizeof abuff, "#%d/%s", thing, atrname);
  new_queue_actionlist_int(executor, enactor, caller, command, parent_queue,
                           PE_INFO_SHARE, queue_type, pe_regs, abuff);

  /* pe_regs is freed later when the new queue is freed */
  mush_free(start, "atrval.queue-attr");
  return 1;
}

/** Enqueue the action part of an attribute.
 * This function is a front-end to parse_que() that takes an attribute,
 * removes ^....: or $....: from its value, and queues what's left.
 * \param executor object containing the attribute.
 * \param atrname attribute name.
 * \param enactor the enactor.
 * \param noparent if true, parents of executor are not checked for atrname.
 * \param pe_regs the pe_regs args for the queue entry
 * \param flags QUEUE_* flags
 * \param priv object to use for priv checks, or NOTHING to do none
 * \retval 0 failure.
 * \retval 1 success.
 */
int
queue_attribute_base_priv(dbref executor, const char *atrname, dbref enactor,
                          int noparent, PE_REGS *pe_regs, int flags, dbref priv,
                          MQUE *parent_queue, const char *input)
{
  ATTR *a;

  a = queue_attribute_getatr(executor, atrname, noparent);
  if (!a)
    return 0;
  if (RealGoodObject(priv) && !Can_Read_Attr(priv, executor, a))
    return 0;
  queue_attribute_useatr(executor, a, enactor, pe_regs, flags, parent_queue,
                         input);
  return 1;
}

/** Wrapper to get an attribute for queueing, possibly checking parents
 * \param executor object the attr is on
 * \param atrname attribute to get
 * \param noparent skip parents?
 * \return the attr to queue
 */
ATTR *
queue_attribute_getatr(dbref executor, const char *atrname, int noparent)
{
  return (noparent ? atr_get_noparent(executor, strupper(atrname))
                   : atr_get(executor, strupper(atrname)));
}

/** Queue an attribute.
 * This function queues an action list from an attribute, skipping the
 * $-command or ^-listen prefix if present.
 * \param executor object queueing the action list
 * \param a attribute with the action list to queue
 * \param enactor enactor causing the queueing
 * \param pe_regs a pe_regs struct to use for the new queue entry
 * \param flags QUEUE_* flags to use for the queue entry
 * \return 1
 */
int
queue_attribute_useatr(dbref executor, ATTR *a, dbref enactor, PE_REGS *pe_regs,
                       int flags, MQUE *parent_queue, const char *input)
{
  char *command;
  int queue_type = QUEUE_DEFAULT | flags;
  char cmd_buff[BUFFER_LEN];
  char abuff[2048];
  char *args[MAX_STACK_ARGS];
  char match_space[BUFFER_LEN * 2];
  ssize_t match_space_len = BUFFER_LEN * 2;

  if (input) {
    /* Attempt to match input against the attribute, accept either. */
    if (atr_single_match_r(a, AF_COMMAND | AF_LISTEN, ':', input, args,
                           match_space, match_space_len, cmd_buff, pe_regs)) {
      command = cmd_buff;
    } else {
      return 1;
    }
  } else {
    strncpy(cmd_buff, atr_value(a), BUFFER_LEN);
    command = cmd_buff;
    /* Trim off $-command or ^-command prefix */
    if (*command == '$' || *command == '^') {
      while (*command && *command != ':') {
        if (*command == '\\' && *(command + 1))
          command++;
        command++;
      }
      if (!command) {
        /* Oops, had '$' or '^', but no unescaped ':' */
        command = cmd_buff;
      } else {
        /* Skip the ':' */
        command++;
      }
    }
  }

  if (AF_NoDebug(a)) {
    queue_type |= QUEUE_NODEBUG;
  } else if (AF_Debug(a)) {
    queue_type |= QUEUE_DEBUG;
  }

  snprintf(abuff, sizeof abuff, "#%d/%s", executor, AL_NAME(a));
  new_queue_actionlist_int(executor, enactor, enactor, command, parent_queue,
                           PE_INFO_DEFAULT, queue_type, pe_regs, abuff);
  return 1;
}

/** Queue an entry on the wait or semaphore queues.
 * This function creates and adds a queue entry to the wait queue
 * or the semaphore queue. Wait queue entries are sorted by when
 * they're due to expire; semaphore queue entries are just added
 * to the back of the queue.
 * \param executor the enqueuing object.
 * \param waittill time to wait, or 0.
 * \param command command to enqueue.
 * \param enactor object that caused command to be enqueued.
 * \param sem object to serve as a semaphore, or NOTHING.
 * \param semattr attribute to serve as a semaphore, or NULL (to use SEMAPHORE).
 * \param until 1 if we wait until an absolute time.
 * \param parent_queue the queue entry the \@wait command was executed in
 */
void
wait_que(dbref executor, int waittill, char *command, dbref enactor, dbref sem,
         const char *semattr, int until, MQUE *parent_queue)
{
  MQUE *tmp;
  NEW_PE_INFO *pe_info;
  int pid;
  int queue_type = QUEUE_DEFAULT;
  if (parent_queue && (parent_queue->queue_type & QUEUE_EVENT))
    queue_type |= QUEUE_EVENT;
  if (waittill == 0) {
    if (sem != NOTHING)
      add_to_sem(sem, -1, semattr);
    new_queue_actionlist(executor, enactor, enactor, command, parent_queue,
                         PE_INFO_CLONE, queue_type, NULL);
    return;
  }
  if (!pay_queue(executor, command)) /* make sure player can afford to do it */
    return;
  pid = next_pid();
  if (pid == 0) {
    notify(executor, T("Queue entry table full. Try again later."));
    return;
  }
  if (parent_queue)
    pe_info = pe_info_from(parent_queue->pe_info, PE_INFO_CLONE, NULL);
  else
    pe_info = NULL;
  tmp = new_queue_entry(pe_info);
  tmp->action_list = mush_strdup(command, "mque.action_list");
  tmp->pid = pid;
  tmp->executor = executor;
  tmp->enactor = enactor;
  tmp->caller = enactor;
  tmp->queue_type |= queue_type;

  if (until) {
    tmp->wait_until = (time_t) waittill;
  } else {
    if (waittill >= 0)
      tmp->wait_until = mudtime + waittill;
    else
      tmp->wait_until = 0; /* semaphore wait without a timeout */
  }
  tmp->semaphore_obj = sem;
  if (sem == NOTHING) {
    /* No semaphore, put on normal wait queue, sorted by time */
    MQUE *point, *trail = NULL;

    for (point = qwait; point && (point->wait_until <= tmp->wait_until);
         point = point->next)
      trail = point;

    tmp->next = point;
    if (trail != NULL)
      trail->next = tmp;
    else
      qwait = tmp;
  } else {

    /* Put it on the end of the semaphore queue */
    tmp->semaphore_attr =
      mush_strdup(semattr ? semattr : "SEMAPHORE", "mque.semaphore_attr");
    if (qsemlast != NULL) {
      qsemlast->next = tmp;
      qsemlast = tmp;
    } else {
      qsemfirst = qsemlast = tmp;
    }
  }
  im_insert(queue_map, tmp->pid, tmp);
}

void
update_queue_load()
{
  static time_t last_mudtime = 0;
  time_t diff;

  if (last_mudtime == 0) {
    memset(queue_load_record, 0, sizeof(queue_load_record));
    last_mudtime = mudtime;
    return;
  }

  diff = mudtime - last_mudtime;
  last_mudtime = mudtime;

  if (diff <= 0) {
    /* No changes, or we're getting pushed back in time by ntpd, dst, or
     * similar? */
    return;
  }

  if (diff >= QUEUE_LOAD_SECS) {
    /* Wow, likely a major change in time from ntp or a poor dst
     * implementation, unlikely we actually slept this long. We'll just quietly
     * pretend nothing major happened, and only shift the load by 1 second. */
    diff = 1;
  }

  /* Advance the queue load average count */
  memmove(queue_load_record + diff, queue_load_record,
          sizeof(queue_load_record) - (diff * sizeof(int32_t)));
  memset(queue_load_record, 0, sizeof(int32_t) * diff);
}

/** Check for queued commands. This is called whenever we expect to need
 * new queued commands. (via que_next)
 */
void
queue_update(void)
{
  static time_t last_mudtime = 0;
  MQUE *trail = NULL, *point, *next;

  if (mudtime == last_mudtime) {
    /* Only run once per second at most. */
    return;
  }
  last_mudtime = mudtime;

  /* check regular @wait queue */
  while (qwait && qwait->wait_until <= mudtime) {
    point = qwait;
    qwait = point->next;
    point->next = NULL;
    point->wait_until = 0;
    if (qlast) {
      qlast->next = point;
      qlast = point;
    } else {
      qlast = qfirst = point;
    }
  }

  /* check for semaphore Zwait timeouts */
  for (point = qsemfirst, trail = NULL; point; point = next) {
    if (point->wait_until == 0 || point->wait_until > mudtime) {
      next = (trail = point)->next;
      continue; /* skip non-timed and those that haven't gone off yet */
    }
    if (trail != NULL)
      trail->next = next = point->next;
    else
      qsemfirst = next = point->next;
    if (point == qsemlast)
      qsemlast = trail;
    add_to_sem(point->semaphore_obj, -1, point->semaphore_attr);
    point->semaphore_obj = NOTHING;
    point->next = NULL;
    if (qlast) {
      qlast->next = point;
      qlast = point;
    } else {
      qlast = qfirst = point;
    }
  }
}

/** Execute some commands from the top of the queue.
 * This function dequeues and executes commands on the normal
 * priority (player) queue.
 * \param ncom number of commands to execute.
 * \return number of commands executed.
 */
int
do_top(int ncom)
{
  int i;
  MQUE *entry;

  for (i = 0; i < ncom; i++) {
    if (!qfirst)
      return i;

    /* We must dequeue before execution, so that things like
     * queued @kick or @ps get a sane queue image.
     */
    entry = qfirst;
    if (!(qfirst = entry->next))
      qlast = NULL;
    do_entry(entry, 0);
    free_qentry(entry);
  }
  return i;
}

void
run_user_input(dbref player, int port, char *input)
{
  MQUE *entry;

  entry = new_queue_entry(NULL);
  entry->action_list = mush_strdup(input, "mque.action_list");
  entry->enactor = player;
  entry->executor = player;
  entry->caller = player;
  entry->port = port;
  entry->queue_type = QUEUE_SOCKET | QUEUE_NOLIST;
  do_entry(entry, 0);
  free_qentry(entry);
}

void
run_http_command(dbref player, int port, char *method, NEW_PE_INFO *pe_info)
{
  MQUE *entry;
  char include_cmd[MAX_COMMAND_LEN];

  snprintf(include_cmd, MAX_COMMAND_LEN, "@include #%d/%s", player, method);

  entry = new_queue_entry(pe_info);
  entry->action_list = mush_strdup(include_cmd, "mque.action_list");
  entry->enactor = player;
  entry->executor = player;
  entry->caller = player;
  entry->port = port;
  entry->queue_type = QUEUE_SOCKET | QUEUE_NOLIST;
  do_entry(entry, 0);
  free_qentry(entry);
}

/* Return 1 if an @break needs to propagate up to the calling q entry, 0
 * otherwise */
static int
do_entry(MQUE *entry, int include_recurses)
{
  dbref executor;
  char tbuf[BUFFER_LEN + SSE_OFFSET] = {'\0'};
  int inplace_break_called = 0;
  char *r;
  char const *s;
  MQUE *tmp;
  int pt_flag = PT_SEMI;
  PE_REGS *pe_regs;

  if (entry->queue_type & QUEUE_NOLIST)
    pt_flag = PT_NOTHING;

  executor = entry->executor;
  if (!RealGoodObject(executor))
    return 0;

  if (!(entry->queue_type & (QUEUE_SOCKET | QUEUE_INPLACE))) {
    giveto(executor, QUEUE_COST);
    add_to(executor, -1);
  }

  if (!IsPlayer(executor) && Halted(executor))
    return 0;

  queue_load_record[0] += 1;

  s = entry->action_list;
  if (!include_recurses) {
    start_cpu_timer();
    /* These vars are used in report() if mush_panic() is called, to print
     * useful debug info */
    if (entry->pe_info->cmd_raw) {
      strcpy(report_cmd, entry->pe_info->cmd_raw);
    } else {
      report_cmd[0] = '\0';
    }
    report_dbref = executor;
  }

  while (!cpu_time_limit_hit && *s) {
    char rbuff[BUFFER_LEN];
    r = rbuff;
    process_expression(rbuff, &r, &s, executor, entry->caller, entry->enactor,
                       PE_NOTHING, pt_flag, entry->pe_info);
    *r = '\0';
    if (entry->pe_info->cmd_raw) {
      mush_free(entry->pe_info->cmd_raw, "string");
    }
    entry->pe_info->cmd_raw = mush_strdup(rbuff, "string");

    if (*s == ';')
      s++;
    /* process_command() destructively modifies the cmd, so we need to copy it
     */
    if (has_markup(entry->pe_info->cmd_raw)) {
      /* Remove any markup, as it can cause bleed when split by
       * command_argparse() */
      strcpy(tbuf, remove_markup(entry->pe_info->cmd_raw, NULL));
    } else {
      strcpy(tbuf, entry->pe_info->cmd_raw);
    }

    process_command(executor, tbuf, entry);
    while (entry->inplace) {
      tmp = entry->inplace;
      /* We have a new queue to process, via @include, @break, @switch/inplace
       * or similar */
      if (include_recurses < 50) {
        switch (tmp->queue_type & (QUEUE_PRESERVE_QREG | QUEUE_CLEAR_QREG)) {
        case QUEUE_PRESERVE_QREG:
          pe_regs =
            pe_regs_localize(entry->pe_info, PE_REGS_LOCALQ, "do_entry");
          break;
        case QUEUE_CLEAR_QREG:
          clear_allq(entry->pe_info);
          pe_regs = NULL;
          break;
        case (QUEUE_CLEAR_QREG | QUEUE_PRESERVE_QREG):
          pe_regs = pe_regs_localize(
            entry->pe_info, PE_REGS_LOCALQ | PE_REGS_QSTOP, "do_entry");
          break;
        default:
          pe_regs = NULL;
        }
        if (tmp->regvals) {
          /* PE_INFO_SHARE - This comes after the localizing. */
          tmp->regvals->prev = tmp->pe_info->regvals;
          tmp->pe_info->regvals = tmp->regvals;
          inplace_break_called = do_entry(tmp, include_recurses + 1);
          tmp->pe_info->regvals = tmp->regvals->prev;
          tmp->regvals->prev = NULL;
        } else {
          inplace_break_called = do_entry(tmp, include_recurses + 1);
        }
        if (tmp->queue_type & QUEUE_NO_BREAKS) {
          inplace_break_called = 0;
        }
        if (pe_regs) {
          pe_regs_restore(entry->pe_info, pe_regs);
          pe_regs_free(pe_regs);
        }
        /* Propagate qreg values. This is because this was a queue entry with
         * a new pe_info. We use pe_regs_qcopy for this. */
        if (tmp->queue_type & QUEUE_PROPAGATE_QREG) {
          if (tmp->pe_info->regvals && !(entry->pe_info->regvals)) {
            entry->pe_info->regvals = pe_regs_create(PE_REGS_QUEUE, "do_entry");
          }
          if (tmp->pe_info->regvals) {
            pe_regs_qcopy(entry->pe_info->regvals, tmp->pe_info->regvals);
          }
        }
        if (tmp->save_attrname) {
          if (tmp->pe_info->attrname) {
            mush_free(tmp->pe_info->attrname, "string");
          }
          tmp->pe_info->attrname = tmp->save_attrname;
          tmp->save_attrname = NULL;
        }
      }
      entry->inplace = tmp->next;
      tmp->next = NULL;
      free_qentry(tmp);
      if (inplace_break_called)
        break;
    }
    if ((entry->queue_type & QUEUE_BREAK) || inplace_break_called)
      break;
    if (entry->queue_type & QUEUE_RETRY) {
      s = entry->action_list;
      entry->queue_type &= ~QUEUE_RETRY;
    }
  }

  if (!include_recurses)
    reset_cpu_timer();

  return ((entry->queue_type & QUEUE_BREAK) || inplace_break_called);
}

/** Determine whether it's time to run a queued command.
 * This function returns the number of milliseconds we expect to wait
 * before it's time to run a queued command.
 * If there are commands in the player queue, that's 0.
 * If there are commands in the object queue, that's 1000.
 * Otherwise, we check wait and semaphore queues to see what's next.
 * \return seconds left before a queue entry will be ready.
 */
uint64_t
queue_msecs_till_next(void)
{
  uint64_t min, curr;
  MQUE *point;
  /* If there are commands in the player queue, they should be run
   * immediately.
   */
  if (qfirst != NULL)
    return 0;

  /* Arbitrarily high wait */
  min = SECS_TO_MSECS(500);

  /* Check out the wait and semaphore queues, looking for the smallest
   * wait value. Return that - 1, since commands get moved to the player
   * queue when they have one second to go.
   */

  /* Wait queue is in sorted order so we only have to look at the first
     item on it. Anything else is wasted time. */
  if (qwait) {
    curr = SECS_TO_MSECS(difftime(qwait->wait_until, mudtime));
    if (curr < min)
      min = curr;
  }

  for (point = qsemfirst; point; point = point->next) {
    if (point->wait_until == 0) /* no timeout */
      continue;
    curr = SECS_TO_MSECS(difftime(point->wait_until, mudtime));
    if (curr < min) {
      min = curr;
    }
  }

  return min;
}

static int
drain_helper(dbref player __attribute__((__unused__)), dbref thing,
             dbref parent __attribute__((__unused__)),
             char const *pattern __attribute__((__unused__)), ATTR *atr,
             void *args __attribute__((__unused__)))
{
  if (waitable_attr(thing, AL_NAME(atr)))
    (void) atr_clr(thing, AL_NAME(atr), GOD);
  return 0;
}

/** Notify a semaphore, with PE_REGS to integrate to its own pe_regs.
 * This function dequeues an entry in the semaphore queue and executes
 * it.
 * \param thing object serving as semaphore.
 * \param aname attribute serving as semaphore.
 * \param pe_regs Q-registers (or other Q-regs)
 * \retval 1 Successfully notified an existing entry.
 * \retval 0 No existing entry.
 */
int
execute_one_semaphore(dbref thing, char const *aname, PE_REGS *pe_regs)
{
  MQUE **point;
  MQUE *entry;

  /* Go through the semaphore queue and do it */
  point = &qsemfirst;
  while (*point) {
    entry = *point;
    if (entry->semaphore_obj != thing ||
        (aname && strcmp(entry->semaphore_attr, aname))) {
      point = &(entry->next);
      continue;
    }

    /* Remove the queue entry from the semaphore list */
    *point = entry->next;
    entry->next = NULL;
    if (qsemlast == entry) {
      qsemlast = qsemfirst;
      if (qsemlast)
        while (qsemlast->next)
          qsemlast = qsemlast->next;
    }

    /* Update bookkeeping */
    add_to_sem(entry->semaphore_obj, -1, entry->semaphore_attr);

    if (pe_regs) {
      if (entry->pe_info == NULL) {
        entry->pe_info = make_pe_info("pe_info-execute_one_semaphore");
      }
      pe_regs_copystack(entry->pe_info->regvals, pe_regs, PE_REGS_QUEUE, 1);
    }

    /* And enqueue */
    if (qlast) {
      qlast->next = entry;
      qlast = entry;
    } else {
      qlast = qfirst = entry;
    }
    return 1;
  }
  return 0;
}

/** Drain or notify a semaphore.
 * This function dequeues an entry in the semaphore queue and either
 * discards it (drain) or executes it (notify). Maybe more than one.
 * \param thing object serving as semaphore.
 * \param aname attribute serving as semaphore.
 * \param count number of entries to dequeue.
 * \param all if 1, dequeue all entries.
 * \param drain if 1, drain rather than notify the entries.
 */
void
dequeue_semaphores(dbref thing, char const *aname, int count, int all,
                   int drain)
{

  MQUE **point;
  MQUE *entry;

  if (all)
    count = INT_MAX;

  /* Go through the semaphore queue and do it */
  point = &qsemfirst;
  while (*point && count > 0) {
    entry = *point;
    if (entry->semaphore_obj != thing ||
        (aname && strcmp(entry->semaphore_attr, aname))) {
      point = &(entry->next);
      continue;
    }

    /* Remove the queue entry from the semaphore list */
    *point = entry->next;
    entry->next = NULL;
    if (qsemlast == entry) {
      qsemlast = qsemfirst;
      if (qsemlast)
        while (qsemlast->next)
          qsemlast = qsemlast->next;
    }

    /* Update bookkeeping */
    count--;
    add_to_sem(entry->semaphore_obj, -1, entry->semaphore_attr);

    /* Dispose of the entry as appropriate: discard if @drain, or put
     * into either the player or the object queue. */
    if (drain) {
      giveto(entry->executor, QUEUE_COST);
      add_to(entry->executor, -1);
      free_qentry(entry);
    } else {
      if (qlast) {
        qlast->next = entry;
        qlast = entry;
      } else {
        qlast = qfirst = entry;
      }
    }
  }

  /* If @drain/all, clear the relevant attribute(s) */
  if (drain && all) {
    if (aname)
      (void) atr_clr(thing, aname, GOD);
    else
      atr_iter_get(GOD, thing, "**", AIG_NONE, drain_helper, NULL);
  }

  /* If @notify and count was higher than the number of queue entries,
   * make the semaphore go negative.  This does not apply to
   * @notify/any or @notify/all. */
  if (!drain && aname && !all && count > 0)
    add_to_sem(thing, -count, aname);
}

COMMAND(cmd_notify_drain)
{
  int drain;
  char *pos;
  char const *aname;
  dbref thing;
  int count;
  int all;
  int i;
  PE_REGS *pe_regs;

  /* Figure out whether we're in notify or drain */
  drain = (cmd->name[1] == 'D');

  /* Make sure they gave an object ref */
  if (!arg_left || !*arg_left) {
    notify(executor, T("You must specify an object to use for the semaphore."));
    return;
  }

  /* Figure out which attribute we're using */
  pos = strchr(arg_left, '/');
  if (pos) {
    if (SW_ISSET(sw, SWITCH_ANY)) {
      notify(
        executor,
        T("You may not specify a semaphore attribute with the ANY switch."));
      return;
    }
    *pos++ = '\0';
    upcasestr(pos);
    aname = pos;
  } else {
    if (SW_ISSET(sw, SWITCH_ANY)) {
      aname = NULL;
    } else {
      aname = "SEMAPHORE";
    }
  }

  /* Figure out which object we're using */
  thing = noisy_match_result(executor, arg_left, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(thing))
    return;
  /* must control something or have it link_ok in order to use it as
   * as a semaphore.
   */
  if ((!controls(executor, thing) && !LinkOk(thing)) ||
      (aname && !waitable_attr(thing, aname))) {
    notify(executor, T("Permission denied."));
    return;
  }

  /* Figure out how many times to notify */
  if (SW_ISSET(sw, SWITCH_SETQ)) {
    pe_regs = pe_regs_create(PE_REGS_Q, "cmd_notify_drain");
    for (i = 1; args_right[i]; i += 2) {
      if (args_right[i + 1]) {
        pe_regs_set(pe_regs, PE_REGS_Q | PE_REGS_NOCOPY, args_right[i],
                    args_right[i + 1]);
      } else {
        pe_regs_set(pe_regs, PE_REGS_Q | PE_REGS_NOCOPY, args_right[i], "");
      }
    }
    if (execute_one_semaphore(thing, aname, pe_regs)) {
      quiet_notify(executor, T("Notified."));
    } else {
      notify_format(executor, T("No such semaphore entry to notify."));
    }
    pe_regs_free(pe_regs);
  } else {
    all = SW_ISSET(sw, SWITCH_ALL);
    if (args_right[1] && *args_right[1]) {
      if (all) {
        notify(executor,
               T("You may not specify a semaphore count with the ALL switch."));
        return;
      }
      if (!is_strict_uinteger(args_right[1])) {
        notify(executor, T("The semaphore count must be an integer."));
        return;
      }
      count = parse_integer(args_right[1]);
    } else {
      if (drain)
        all = 1;
      if (all)
        count = INT_MAX;
      else
        count = 1;
    }

    dequeue_semaphores(thing, aname, count, all, drain);

    if (drain) {
      quiet_notify(executor, T("Drained."));
    } else {
      quiet_notify(executor, T("Notified."));
    }
  }
}

/** Softcode interface to add a command to the wait or semaphore queue.
 * \verbatim
 * This is the top-level function for @wait.
 * \endverbatim
 * \param executor the executor
 * \param enactor the object causing the command to be added.
 * \param arg1 the wait time, semaphore object/attribute, or both. Modified!
 * \param cmd command to queue.
 * \param until if 1, wait until an absolute time.
 * \param parent_queue the parent queue entry to take env/qreg from
 */
void
do_wait(dbref executor, dbref enactor, char *arg1, const char *cmd, bool until,
        MQUE *parent_queue)
{
  dbref thing;
  char *tcount = NULL, *aname = NULL;
  int waitfor, num;
  ATTR *a;

  if (is_strict_integer(arg1)) {
    /* normal wait */
    wait_que(executor, parse_integer(arg1), (char *) cmd, enactor, NOTHING,
             NULL, until, parent_queue);
    return;
  }
  /* semaphore wait with optional timeout */

  /* find the thing to wait on */
  aname = strchr(arg1, '/');
  if (aname)
    *aname++ = '\0';
  if ((thing = noisy_match_result(executor, arg1, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING) {
    return;
  }

  /* aname is either time, attribute or attribute/time.
   * After this:
   * tcount will hold string timeout or NULL for none
   * aname will hold attribute name.
   */
  if (aname) {
    tcount = strchr(aname, '/');
    if (!tcount) {
      if (is_strict_integer(aname)) { /* Timeout */
        tcount = aname;
        aname = (char *) "SEMAPHORE";
      } else { /* Attribute */
        upcasestr(aname);
      }
    } else { /* attribute/timeout */
      *tcount++ = '\0';
      upcasestr(aname);
    }
  } else {
    aname = (char *) "SEMAPHORE";
  }

  if ((!controls(executor, thing) && !LinkOk(thing)) ||
      (aname && !waitable_attr(thing, aname))) {
    notify(executor, T("Permission denied."));
    return;
  }
  /* get timeout, default of -1 */
  if (tcount && *tcount)
    waitfor = parse_integer(tcount);
  else
    waitfor = -1;
  add_to_sem(thing, 1, aname);
  a = atr_get_noparent(thing, aname);
  if (a)
    num = parse_integer(atr_value(a));
  else
    num = 0;
  if (num <= 0) {
    thing = NOTHING;
    waitfor = -1; /* just in case there was a timeout given */
  }
  wait_que(executor, waitfor, (char *) cmd, enactor, thing, aname, until,
           parent_queue);
}

/** Interface to \@wait/pid; modifies the wait times of queue
 * entries.
 * \param player the object doing the command.
 * \param pidstr the process id to modify.
 * \param timestr the new timeout.
 * \param until true if timeout is an absolute time.
 */
void
do_waitpid(dbref player, const char *pidstr, const char *timestr, bool until)
{
  uint32_t pid;
  MQUE *q, *tmp, *last;
  bool found;

  if (!is_strict_uinteger(pidstr)) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  pid = parse_uint32(pidstr, NULL, 10);
  q = im_find(queue_map, pid);

  if (!q) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  if (!controls(player, q->executor) && !HaltAny(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (q->semaphore_obj != NOTHING && q->wait_until == 0) {
    notify(player,
           T("You cannot adjust the timeout of an indefinite semaphore."));
    return;
  }

  if (!is_strict_integer(timestr)) {
    notify(player, T("That is not a valid timestamp."));
    return;
  }

  if (until) {
    int when;

    when = parse_integer(timestr);

    if (when < 0)
      when = 0;

    q->wait_until = (time_t) when;

  } else {
    int offset = parse_integer(timestr);

    /* If timestr looks like +NNN or -NNN, add or subtract a number
       of seconds to the current timeout. Otherwise, change timeout.
     */
    if (timestr[0] == '+' || timestr[0] == '-')
      q->wait_until += offset;
    else
      q->wait_until = mudtime + offset;

    if (q->wait_until < 0)
      q->wait_until = 0;
  }

  /* Now adjust it in the wait queue. Not a clever approach, but I
     wrote it at 3 am and clever was not an option. */
  found = false;
  for (tmp = qwait, last = NULL; tmp; last = tmp, tmp = tmp->next) {
    if (tmp == q) {
      if (last)
        last->next = q->next;
      else
        qwait = qwait->next;
      found = true;
      break;
    }
  }
  if (found) {
    found = false;
    for (tmp = qwait, last = NULL; tmp; last = tmp, tmp = tmp->next) {
      if (tmp->wait_until > q->wait_until) {
        if (last) {
          last->next = q;
          q->next = tmp;
        } else {
          q->next = qwait;
          qwait = q;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      if (last)
        last->next = q;
      else
        qwait = q;
      q->next = NULL;
    }
  }

  notify_format(player, T("Queue entry with pid %u updated."),
                (unsigned int) pid);
}

FUNCTION(fun_pidinfo)
{
  char *r, *s;
  const char *osep = " ";
  char *fields, field[] = "queue player time object attribute command";
  uint32_t pid;
  MQUE *q;
  bool first = true;

  if (!is_strict_uinteger(args[0])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }

  pid = parse_uint32(args[0], NULL, 10);
  q = im_find(queue_map, pid);

  if (!q) {
    safe_str(T("#-1 INVALID PID"), buff, bp);
    return;
  }

  if (!controls(executor, q->executor) && !LookQueue(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if ((nargs > 1) && args[1] && *args[1]) {
    fields = args[1];
  } else {
    fields = field;
  }

  if (nargs == 3)
    osep = args[2];

  s = trim_space_sep(fields, ' ');
  do {
    r = split_token(&s, ' ');
    if (string_prefix("queue", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      if (GoodObject(q->semaphore_obj))
        safe_str("semaphore", buff, bp);
      else
        safe_str("wait", buff, bp);
    } else if (string_prefix("player", r) || string_prefix("executor", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      safe_dbref(q->executor, buff, bp);
    } else if (string_prefix("time", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      if (q->wait_until == 0)
        safe_integer(-1, buff, bp);
      else
        safe_integer(difftime(q->wait_until, mudtime), buff, bp);
    } else if (string_prefix("object", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      safe_dbref(q->semaphore_obj, buff, bp);
    } else if (string_prefix("attribute", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      if (GoodObject(q->semaphore_obj)) {
        safe_str(q->semaphore_attr, buff, bp);
      } else {
        safe_dbref(NOTHING, buff, bp);
      }
    } else if (string_prefix("command", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      safe_str(q->action_list, buff, bp);
    }
  } while (s);
}

#define LPIDS_WAIT 1
#define LPIDS_SEMAPHORE 2
#define LPIDS_INDEPENDENT 4
#define LPIDS_TYPES (LPIDS_WAIT | LPIDS_SEMAPHORE)
FUNCTION(fun_lpids)
{
  /* Can be called as LPIDS or GETPIDS */
  MQUE *tmp;
  int qmask = 0;
  dbref thing = NOTHING;
  dbref player = NOTHING;
  char *attrib = NULL;
  bool first = true;
  const char *list;
  char *elem;

  if (strcasecmp(called_as, "LPIDS") == 0) {
    /* lpids(player[,type]) */
    if (args[0] && *args[0]) {
      if (strcasecmp(args[0], "all") == 0) {
        if (LookQueue(executor))
          player = NOTHING;
        else
          player = executor;
      } else {
        player = match_thing(executor, args[0]);
        if (!GoodObject(player)) {
          safe_str(T(e_notvis), buff, bp);
          return;
        }
        if (!LookQueue(executor) &&
            !(Owns(executor, player) || controls(executor, player))) {
          safe_str(T(e_perm), buff, bp);
          return;
        }
      }
    } else if (!LookQueue(executor)) {
      player = executor;
    }
    if (nargs > 1 && args[1] && *args[1]) {
      list = args[1];
      while (list && *list) {
        elem = next_in_list(&list);
        if (strcasecmp("wait", elem) == 0)
          qmask |= LPIDS_WAIT;
        else if (strcasecmp("semaphore", elem) == 0)
          qmask |= LPIDS_SEMAPHORE;
        else if (strcasecmp("independent", elem) == 0)
          qmask |= LPIDS_INDEPENDENT;
        else {
          safe_str(T("#-1 INVALID ARGUMENT"), buff, bp);
          return;
        }
      }
    }
    if (!(qmask & LPIDS_TYPES))
      qmask |= LPIDS_TYPES;
  } else {
    /* getpids(obj[/attrib]) */
    qmask = LPIDS_SEMAPHORE;
    attrib = strchr(args[0], '/');
    if (attrib)
      *attrib++ = '\0';
    thing = match_thing(executor, args[0]);
    if (!GoodObject(thing)) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
    if (!(LookQueue(executor) || (controls(executor, thing)))) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
  }
  if (qmask & LPIDS_WAIT) {
    for (tmp = qwait; tmp; tmp = tmp->next) {
      if (GoodObject(player) && GoodObject(tmp->executor) &&
          ((qmask & LPIDS_INDEPENDENT) ? (tmp->executor != player)
                                       : !Owns(tmp->executor, player))) {
        continue;
      }
      if (!first)
        safe_chr(' ', buff, bp);
      safe_integer(tmp->pid, buff, bp);
      first = false;
    }
  }
  if (qmask & LPIDS_SEMAPHORE) {
    for (tmp = qsemfirst; tmp; tmp = tmp->next) {
      if (GoodObject(player) && GoodObject(tmp->executor) &&
          ((qmask & LPIDS_INDEPENDENT) ? (tmp->executor != player)
                                       : !Owns(tmp->executor, player)))
        continue;
      if (GoodObject(thing) && (tmp->semaphore_obj != thing))
        continue;
      if (attrib && *attrib && strcasecmp(tmp->semaphore_attr, attrib))
        continue;
      if (!first)
        safe_chr(' ', buff, bp);
      safe_integer(tmp->pid, buff, bp);
      first = false;
    }
  }
}

static void
show_queue(dbref player, dbref victim, int q_type, int q_quiet, int q_all,
           MQUE *q_ptr, int *tot, int *self, int *del)
{
  MQUE *tmp;
  for (tmp = q_ptr; tmp; tmp = tmp->next) {
    (*tot)++;
    if (!GoodObject(tmp->executor))
      (*del)++;
    else if (q_all || (Owner(tmp->executor) == victim)) {
      if ((LookQueue(player) || Owns(tmp->executor, player))) {
        (*self)++;
        if (!q_quiet)
          show_queue_single(player, tmp, q_type);
      }
    }
  }
}

/* Show a single queue entry */
static void
show_queue_single(dbref player, MQUE *q, int q_type)
{
  switch (q_type) {
  case 1: /* wait queue */
    notify_format(player, "(Pid: %u) [%ld]%s: %s", (unsigned int) q->pid,
                  (long) difftime(q->wait_until, mudtime),
                  unparse_object(player, q->executor, AN_UNPARSE),
                  q->action_list);
    break;
  case 2: /* semaphore queue */
    if (q->wait_until != 0) {
      notify_format(player, "(Pid: %u) [#%d/%s/%ld]%s: %s",
                    (unsigned int) q->pid, q->semaphore_obj, q->semaphore_attr,
                    (long) difftime(q->wait_until, mudtime),
                    unparse_object(player, q->executor, AN_UNPARSE),
                    q->action_list);
    } else {
      notify_format(player, "(Pid: %u) [#%d/%s]%s: %s", (unsigned int) q->pid,
                    q->semaphore_obj, q->semaphore_attr,
                    unparse_object(player, q->executor, AN_UNPARSE),
                    q->action_list);
    }
    break;
  default: /* player or object queue */
    notify_format(player, "(Pid: %u) %s: %s", (unsigned int) q->pid,
                  unparse_object(player, q->executor, AN_UNPARSE),
                  q->action_list);
  }
}

/* @ps/debug dump */
static void
show_queue_env(dbref player, MQUE *q)
{
  PE_REGS *regs;
  int i = 0;
  PTAB qregs;
  const char *qreg_name;
  char *qreg_val;
  int level;

  notify_format(player, "Environment:\n %%#: #%-8d %%!: #%-8d %%@: #%d",
                q->enactor, q->executor, q->caller);

  /* itext/inum for a @dolist-added queue entry. */
  level = PE_Get_Ilev(q->pe_info);
  if (level >= 0) {
    for (i = 0; i <= level; i += 1)
      notify_format(player, " %%i%d (Position %d) : %s", i,
                    PE_Get_Inum(q->pe_info, i), PE_Get_Itext(q->pe_info, i));
  }

  /* stext for a @switch-added queue entry. */
  level = PE_Get_Slev(q->pe_info);
  if (level >= 0) {
    for (i = 0; i <= level; i += 1)
      notify_format(player, " %%$%d : %s", i, PE_Get_Stext(q->pe_info, i));
  }

  /* %0 - %9 */
  if (PE_Get_Envc(q->pe_info)) {
    notify(player, "Arguments: ");
    for (i = 0; i < MAX_STACK_ARGS; i += 1) {
      const char *arg = PE_Get_Env(q->pe_info, i);
      if (arg)
        notify_format(player, " %%%d : %s", i, arg);
    }
  }

  /* Q registers */
  ptab_init(&qregs);
  ptab_start_inserts(&qregs);
  for (regs = q->pe_info->regvals; regs; regs = regs->prev) {
    PE_REG_VAL *val;
    for (val = regs->vals; val; val = val->next) {
      if ((val->type & PE_REGS_STR) && (val->type & PE_REGS_Q) &&
          *(val->val.sval))
        ptab_insert(&qregs, val->name, (char *) val->val.sval);
    }
    if (regs->flags & PE_REGS_QSTOP)
      break;
  }
  ptab_end_inserts(&qregs);

  if (qregs.len) {
    notify(player, "Registers:");
    for (qreg_val = ptab_firstentry_new(&qregs, &qreg_name); qreg_val;
         qreg_val = ptab_nextentry_new(&qregs, &qreg_name)) {
      int len = strlen(qreg_name);
      if (len > 1) {
        int spacer = 19 - len;
        notify_format(player, " %%q<%s>%-*c: %s", qreg_name, spacer, ' ',
                      qreg_val);
      } else
        notify_format(player, " %%q%-20s : %s", qreg_name, qreg_val);
    }
  }
  ptab_free(&qregs);
}

/** Display a player's queued commands.
 * \verbatim
 * This is the top-level function for @ps.
 * \endverbatim
 * \param player the enactor.
 * \param what name of player whose queue is to be displayed.
 * \param flag type of display. 0 - normal, 1 - all, 2 - summary, 3 - quick
 */
void
do_queue(dbref player, const char *what, enum queue_type flag)
{
  dbref victim = NOTHING;
  int all = 0;
  int quick = 0;
  int dpq = 0, dwq = 0, dsq = 0;
  int pq = 0, wq = 0, sq = 0;
  int tpq = 0, twq = 0, tsq = 0;
  if (flag == QUEUE_SUMMARY || flag == QUEUE_QUICK)
    quick = 1;
  if (flag == QUEUE_ALL || flag == QUEUE_SUMMARY) {
    all = 1;
    victim = player;
  } else if (LookQueue(player)) {
    if (!what || !*what)
      victim = player;
    else {
      victim = match_result(player, what, TYPE_PLAYER,
                            MAT_PLAYER | MAT_ABSOLUTE | MAT_ME | MAT_TYPE);
    }
  } else {
    victim = player;
  }

  switch (victim) {
  case NOTHING:
    notify(player, T("I couldn't find that player."));
    break;
  case AMBIGUOUS:
    notify(player, T("I don't know who you mean!"));
    break;
  default:

    if (!quick) {
      if (all)
        notify(player, T("Queue for : all"));
      else
        notify_format(player, T("Queue for : %s"), AName(victim, AN_SYS, NULL));
    }
    victim = Owner(victim);
    if (!quick)
      notify(player, T("Command Queue:"));
    show_queue(player, victim, 0, quick, all, qfirst, &tpq, &pq, &dpq);
    if (!quick)
      notify(player, T("Wait Queue:"));
    show_queue(player, victim, 1, quick, all, qwait, &twq, &wq, &dwq);
    if (!quick)
      notify(player, T("Semaphore Queue:"));
    show_queue(player, victim, 2, quick, all, qsemfirst, &tsq, &sq, &dsq);
    if (!quick)
      notify(player, T("------------  Queue Done  ------------"));
    notify_format(player,
                  T("Totals: Player...%d/%d[%ddel]  "
                    "Wait...%d/%d[%ddel]  Semaphore...%d/%d"),
                  pq, tpq, dpq, wq, twq, dwq, sq, tsq);
    notify_format(player, T("Load average (1/5/15 minutes): %.2f %.2f %.2f"),
                  average32(queue_load_record, 60),
                  average32(queue_load_record, 300),
                  average32(queue_load_record, 900));
  }
}

/** Display info for a single queue entry.
 * \verbatim
 * This is the top-level function for @ps <pid>.
 * \endverbatim
 * \param player the enactor.
 * \param pidstr the pid for the queue entry to show.
 * \param debug true to display expanded queue environment information.
 */
void
do_queue_single(dbref player, char *pidstr, bool debug)
{
  uint32_t pid;
  MQUE *q;

  if (!is_strict_uinteger(pidstr)) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  pid = parse_uint32(pidstr, NULL, 10);
  q = im_find(queue_map, pid);
  if (!q) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  if (!LookQueue(player) && Owner(player) != Owner(q->executor)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (GoodObject(q->semaphore_obj))
    show_queue_single(player, q, 2);
  else if (q->wait_until > 0)
    show_queue_single(player, q, 1);
  else
    show_queue_single(player, q, 0);

  if (debug)
    show_queue_env(player, q);
}

/** Halt an object, internal use.
 * This function is used to halt objects by other hardcode.
 * See do_halt1() for the softcode interface.
 * \param owner the enactor.
 * \param ncom command to queue after halting.
 * \param victim object to halt.
 */
void
do_halt(dbref owner, const char *ncom, dbref victim)
{
  MQUE *tmp, *trail = NULL, *point, *next;
  int num = 0;
  dbref player;
  if (victim == NOTHING)
    player = owner;
  else
    player = victim;
  if (!Quiet(Owner(player)))
    notify_format(Owner(player), "%s: %s(#%d)", T("Halted"),
                  AName(player, AN_SYS, NULL), player);
  for (tmp = qfirst; tmp; tmp = tmp->next) {
    if (GoodObject(tmp->executor) &&
        ((tmp->executor == player) || (Owner(tmp->executor) == player))) {
      num--;
      giveto(player, QUEUE_COST);
      tmp->executor = NOTHING;
    }
  }
  /* remove wait q stuff */
  for (point = qwait; point; point = next) {
    if (((point->executor == player) || (GoodObject(point->executor) && (Owner(point->executor) == player)))) {
      num--;
      giveto(player, QUEUE_COST);
      if (trail)
        trail->next = next = point->next;
      else
        qwait = next = point->next;
      free_qentry(point);
    } else {
      next = (trail = point)->next;
    }
  }

  /* clear semaphore queue */

  for (point = qsemfirst, trail = NULL; point; point = next) {
    if (((point->executor == player) || (Owner(point->executor) == player))) {
      num--;
      giveto(player, QUEUE_COST);
      if (trail)
        trail->next = next = point->next;
      else
        qsemfirst = next = point->next;
      if (point == qsemlast)
        qsemlast = trail;
      add_to_sem(point->semaphore_obj, -1, point->semaphore_attr);
      free_qentry(point);
    } else
      next = (trail = point)->next;
  }

  add_to(player, num);
  if (ncom && *ncom) {
    new_queue_actionlist(player, player, player, (char *) ncom, NULL,
                         PE_INFO_DEFAULT, QUEUE_DEFAULT, NULL);
  }
}

/** Halt an object, softcode interface.
 * \verbatim
 * This is the top-level function for @halt.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 string representing object to halt.
 * \param arg2 option string representing command to queue after halting.
 */
void
do_halt1(dbref player, const char *arg1, const char *arg2)
{
  dbref victim;
  if (*arg1 == '\0')
    do_halt(player, "", player);
  else {
    if ((victim = noisy_match_result(player, arg1, NOTYPE,
                                     MAT_OBJECTS | MAT_HERE)) == NOTHING)
      return;
    if (!Owns(player, victim) && !HaltAny(player)) {
      notify(player, T("Permission denied."));
      return;
    }
    if (arg2 && *arg2 && !controls(player, victim)) {
      notify(player, T("You may not use @halt obj=command on this object."));
      return;
    }
    /* If victim's a player, we halt all of their objects */
    /* If not, we halt victim and set the HALT flag if no new command */
    /* was given */
    do_halt(player, arg2, victim);
    if (IsPlayer(victim)) {
      if (victim == player) {
        notify(player, T("All of your objects have been halted."));
      } else {
        notify_format(player, T("All objects for %s have been halted."),
                      AName(victim, AN_SYS, NULL));
        notify_format(victim, T("All of your objects have been halted by %s."),
                      AName(player, AN_SYS, NULL));
      }
    } else {
      if (Owner(victim) != player) {
        char owner[BUFFER_LEN];
        char obj[BUFFER_LEN];
        strcpy(owner, AName(Owner(victim), AN_SYS, NULL));
        strcpy(obj, AName(victim, AN_SYS, NULL));
        notify_format(player, "%s: %s's %s(%s)", T("Halted"), owner, obj,
                      unparse_dbref(victim));
        notify_format(Owner(victim), "%s: %s(%s), by %s", T("Halted"), obj,
                      unparse_dbref(victim), AName(player, AN_SYS, NULL));
      }
      if (arg2 && *arg2 == '\0')
        set_flag_internal(victim, "HALT");
    }
  }
}

/** Halt a particular pid
 * \param player the enactor.
 * \param arg1 string representing the pid to halt.
 */
void
do_haltpid(dbref player, const char *arg1)
{
  uint32_t pid;
  MQUE *q;
  dbref victim;
  if (!is_strict_uinteger(arg1)) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  pid = parse_uint32(arg1, NULL, 10);
  q = im_find(queue_map, pid);
  if (!q) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  victim = q->executor;
  if (!controls(player, victim) && !HaltAny(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  /* Instead of trying to track what queue this entry currently
     belongs to, flag it as halted and just not execute it when its
     turn comes up (Or show it in @ps, etc.).  Exception is for
     semaphores, which otherwise might wait forever. */
  q->executor = NOTHING;
  if (q->semaphore_attr) {
    MQUE *last = NULL, *tmp;
    for (tmp = qsemfirst; tmp; last = tmp, tmp = tmp->next) {
      if (tmp == q) {
        if (last)
          last->next = tmp->next;
        else
          qsemfirst = tmp->next;
        if (qsemlast == tmp)
          qsemlast = last;
        break;
      }
    }

    giveto(victim, QUEUE_COST);
    add_to_sem(q->semaphore_obj, -1, q->semaphore_attr);
    free_qentry(q);
  }

  notify_format(player, T("Queue entry with pid %u halted."),
                (unsigned int) pid);
}

/** Halt all objects in the database.
 * \param player the enactor.
 */
void
do_allhalt(dbref player)
{
  dbref victim;
  if (!HaltAny(player)) {
    notify(player,
           T("You do not have the power to bring the world to a halt."));
    return;
  }
  for (victim = 0; victim < db_top; victim++) {
    if (IsPlayer(victim)) {
      notify_format(victim, T("Your objects have been globally halted by %s"),
                    AName(player, AN_SYS, NULL));
      do_halt(victim, "", victim);
    }
  }
}

/** Restart all objects in the database.
 * \verbatim
 * A restart is a halt and then triggering the @startup.
 * \endverbatim
 * \param player the enactor.
 */
void
do_allrestart(dbref player)
{
  dbref thing;
  if (!HaltAny(player)) {
    notify(player, T("You do not have the power to restart the world."));
    return;
  }
  do_allhalt(player);
  for (thing = 0; thing < db_top; thing++) {
    if (!IsGarbage(thing) && !(Halted(thing))) {
      (void) queue_attribute_base(thing, "STARTUP", thing, 1, NULL,
                                  QUEUE_PRIORITY);
      do_top(5);
    }
    if (IsPlayer(thing)) {
      notify_format(thing, T("Your objects are being globally restarted by %s"),
                    AName(player, AN_SYS, NULL));
    }
  }
}

static void
do_raw_restart(dbref victim)
{
  dbref thing;
  if (IsPlayer(victim)) {
    for (thing = 0; thing < db_top; thing++) {
      if ((Owner(thing) == victim) && !IsGarbage(thing) && !(Halted(thing)))
        (void) queue_attribute_noparent(thing, "STARTUP", thing);
    }
  } else {
    /* A single object */
    if (!IsGarbage(victim) && !(Halted(victim)))
      (void) queue_attribute_noparent(victim, "STARTUP", victim);
  }
}

/** Restart an object.
 * \param player the enactor.
 * \param arg1 string representing the object to restart.
 */
void
do_restart_com(dbref player, const char *arg1)
{
  dbref victim;
  if (*arg1 == '\0') {
    do_halt(player, "", player);
    do_raw_restart(player);
  } else {
    if ((victim = noisy_match_result(player, arg1, NOTYPE, MAT_OBJECTS)) ==
        NOTHING)
      return;
    if (!Owns(player, victim) && !HaltAny(player)) {
      notify(player, T("Permission denied."));
      return;
    }
    if (Owner(victim) != player) {
      if (IsPlayer(victim)) {
        notify_format(player, T("All objects for %s are being restarted."),
                      AName(victim, AN_SYS, NULL));
        notify_format(victim,
                      T("All of your objects are being restarted by %s."),
                      AName(player, AN_SYS, NULL));
      } else {
        char owner[BUFFER_LEN];
        char obj[BUFFER_LEN];
        strcpy(owner, AName(Owner(victim), AN_SYS, NULL));
        strcpy(obj, AName(victim, AN_SYS, NULL));
        notify_format(player, T("Restarting: %s's %s(%s)"), owner, obj,
                      unparse_dbref(victim));
        notify_format(Owner(victim), T("Restarting: %s(%s), by %s"), obj,
                      unparse_dbref(victim), AName(player, AN_SYS, NULL));
      }
    } else {
      if (victim == player)
        notify(player, T("All of your objects are being restarted."));
      else
        notify_format(player, T("Restarting: %s(%s)"),
                      AName(victim, AN_SYS, NULL), unparse_dbref(victim));
    }
    do_halt(player, "", victim);
    do_raw_restart(victim);
  }
}

/** Dequeue all queue entries, refunding deposits.
 * This function dequeues all entries in all queues, without executing
 * them and refunds queue deposits. It's called at shutdown.
 */
void
shutdown_queues(void)
{
  shutdown_a_queue(&qfirst, &qlast);
  shutdown_a_queue(&qsemfirst, &qsemlast);
  shutdown_a_queue(&qwait, NULL);
}

static void
shutdown_a_queue(MQUE **head, MQUE **tail)
{
  MQUE *entry;
  /* Drain out a queue */
  while (*head) {
    entry = *head;
    if (!(*head = entry->next) && tail)
      *tail = NULL;
    if (GoodObject(entry->executor) && !IsGarbage(entry->executor)) {
      giveto(entry->executor, QUEUE_COST);
      add_to(entry->executor, -1);
    }
    free_qentry(entry);
  }
}

/** Averages an array of 32-bit integers.
 *
 * When compiling with SSE2 support, uses a vectorized code path that
 * takes only a few iterations to sum up the counts used to compute a 15
 * minute queue load average, instead of 900 from the plain scalar
 * version. Is it not nifty?
 *
 * \param nums The numbers. Must be aligned to 16 bytes.
 * \param len The length of the array
 * \return The average
 */
double
average32(const int32_t *nums, int len)
{
#ifdef HAVE_SSE2
  int chunks, n, total = 0;
  __m128i totals1, totals2, totals3, totals4, zero;

  chunks = len / 16;

  zero = _mm_setzero_si128();
  totals1 = totals2 = totals3 = totals4 = zero;

  /* 16-element chunks */
  for (n = 0; n < chunks; n += 1) {
    __m128i chunk1, chunk2, chunk3, chunk4;
    chunk1 = _mm_load_si128((__m128i *) (nums + (n * 16)));
    chunk2 = _mm_load_si128((__m128i *) (nums + (n * 16) + 4));
    chunk3 = _mm_load_si128((__m128i *) (nums + (n * 16) + 8));
    chunk4 = _mm_load_si128((__m128i *) (nums + (n * 16) + 12));
    totals1 = _mm_add_epi32(totals1, chunk1);
    totals2 = _mm_add_epi32(totals2, chunk2);
    totals3 = _mm_add_epi32(totals3, chunk3);
    totals4 = _mm_add_epi32(totals4, chunk4);
  }

  n = chunks * 16;

  /* Possible trailing 8-element chunk */
  if (len - n >= 8) {
    __m128i chunk1, chunk2;
    chunk1 = _mm_load_si128((__m128i *) (nums + n));
    chunk2 = _mm_load_si128((__m128i *) (nums + n + 4));
    totals1 = _mm_add_epi32(totals1, chunk1);
    totals2 = _mm_add_epi32(totals2, chunk2);
    n += 8;
  }

  /* Possible trailing 4-element chunk */
  if (len - n >= 4) {
    __m128i chunk = _mm_load_si128((__m128i *) (nums + n));
    totals3 = _mm_add_epi32(totals3, chunk);
    n += 4;
  }

  /* Sum up all the totals vectors */
  totals1 = _mm_add_epi32(totals1, totals2);
  totals3 = _mm_add_epi32(totals3, totals4);
  totals1 = _mm_add_epi32(totals1, totals3);

/* And sum the sums */
#ifdef HAVE_SSSE3
  /* SSSE3 version */
  totals1 = _mm_hadd_epi32(totals1, zero);
  totals1 = _mm_hadd_epi32(totals1, zero);
  total = _mm_cvtsi128_si32(totals1);
#else
  /* SSE2 version */
  totals2 = _mm_shuffle_epi32(totals1, _MM_SHUFFLE(1, 0, 3, 2));
  totals1 = _mm_add_epi32(totals1, totals2);
  totals2 = _mm_shuffle_epi32(totals1, _MM_SHUFFLE(2, 3, 0, 1));
  totals1 = _mm_add_epi32(totals1, totals2);
  total = _mm_cvtsi128_si32(totals1);
#endif

  /* Sum up the remaining trailing elements */
  for (; n < len; n += 1)
    total += nums[n];

  return (double) total / (double) len;

#else /* Non-SSE2 version */

  int n;
  int32_t total = 0;

  for (n = 0; n < len; n += 1)
    total += nums[n];

  return (double) total / (double) len;

#endif
}
