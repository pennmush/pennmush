/**
 * \file cque.c
 *
 * \brief Queue for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "conf.h"
#include "command.h"
#include "mushdb.h"
#include "match.h"
#include "externs.h"
#include "parse.h"
#include "strtree.h"
#include "mymalloc.h"
#include "game.h"
#include "attrib.h"
#include "flags.h"
#include "function.h"
#include "case.h"
#include "dbdefs.h"
#include "log.h"
#include "intmap.h"
#include "confmagic.h"


EVAL_CONTEXT global_eval_context;

/** A queue entry.
 * This structure reprsents a queue entry on a linked list of queue
 * entries (a queue). It is used for all of the queues.
 */
typedef struct bque {
  struct bque *next;                    /**< pointer to next entry on queue */
  dbref player;                 /**< player who will do command */
  dbref queued;                 /**< object whose QUEUE gets incremented for this command */
  dbref cause;                  /**< player causing command (for %N) */
  dbref sem;                    /**< semaphore object to block on */
  char *semattr;                /**< semaphore attribute to block on */
  time_t left;                  /**< seconds left until execution */
  char *env[10];                /**< environment, from wild match */
  char *rval[NUMQ];             /**< environment, from setq() */
  char *comm;                   /**< command to be executed */
  uint32_t pid;                 /**< Process id of this entry */
} BQUE;

intmap *queue_map = NULL; /**< Intmap for looking up queue entries by pid */
static uint32_t top_pid = 1;
#define MAX_PID (1U << 15)

static BQUE *qfirst = NULL, *qlast = NULL, *qwait = NULL;
static BQUE *qlfirst = NULL, *qllast = NULL;
static BQUE *qsemfirst = NULL, *qsemlast = NULL;

static int add_to_generic(dbref player, int am, const char *name,
                          uint32_t flags);
static int add_to(dbref player, int am);
static int add_to_sem(dbref player, int am, const char *name);
static int queue_limit(dbref player);
void free_qentry(BQUE *point);
static int pay_queue(dbref player, const char *command);
void wait_que(dbref player, int waituntil, char *command,
              dbref cause, dbref sem, const char *semattr, int until);
int que_next(void);

static void show_queue(dbref player, dbref victim, int q_type,
                       int q_quiet, int q_all, BQUE *q_ptr, int *tot, int *self,
                       int *del);
static void do_raw_restart(dbref victim);
static int waitable_attr(dbref thing, const char *atr);
static void shutdown_a_queue(BQUE **head, BQUE **tail);
static int do_entry(BQUE *entry, int include_recurses);

extern sig_atomic_t cpu_time_limit_hit; /**< Have we used too much CPU? */

/** Attribute flags to be set or checked on attributes to be used
 * as semaphores.
 */
#define SEMAPHORE_FLAGS (AF_LOCKED | AF_PRIVATE | AF_NOCOPY | AF_NODUMP)

/** Queue initializtion function. Must  be called before anything
 * is added to the queue.
 */
void
init_queue(void)
{
  queue_map = im_new();
}

/* Returns true if the attribute on thing can be used as a semaphore.
 * atr should be given in UPPERCASE.
 */
static int
waitable_attr(dbref thing, const char *atr)
{
  ATTR *a;
  if (!atr || !*atr)
    return 0;
  a = atr_get_noparent(thing, atr);
  if (!a) {                     /* Attribute isn't set */
    a = atr_match(atr);
    if (!a)                     /* It's not a built in attribute */
      return 1;
    return !strcmp(AL_NAME(a), "SEMAPHORE");    /* Only allow SEMAPHORE for now */
  } else {                      /* Attribute is set. Check for proper owner and flags and value */
    if ((AL_CREATOR(a) == GOD) && (AL_FLAGS(a) == SEMAPHORE_FLAGS)) {
      char *v = atr_value(a);
      if (!*v || is_integer(v))
        return 1;
      else
        return 0;
    } else {
      return 0;
    }
  }
  return 0;                     /* Not reached */
}

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

static int
add_to(dbref player, int am)
{
  return add_to_generic(player, am, "QUEUE", NOTHING);
}

static int
add_to_sem(dbref player, int am, const char *name)
{
  return add_to_generic(player, am, name ? name : "SEMAPHORE", SEMAPHORE_FLAGS);
}

static int
queue_limit(dbref player)
{
  /* returns 1 if player has exceeded his queue limit, and always
     increments QUEUE by one. */
  int nlimit;

  nlimit = add_to(player, 1);
  if (HugeQueue(player))
    return nlimit > (QUEUE_QUOTA + db_top);
  else
    return nlimit > QUEUE_QUOTA;
}

/** Free a queue entry.
 * \param point queue entry to free.
 */
void
free_qentry(BQUE *point)
{
  int a;
  for (a = 0; a < 10; a++)
    if (point->env[a]) {
      mush_free(point->env[a], "cqueue.env");
    }
  for (a = 0; a < NUMQ; a++)
    if (point->rval[a]) {
      mush_free(point->rval[a], "cqueue.rval");
    }
  if (point->semattr)
    mush_free(point->semattr, "cqueue.semattr");
  if (point->comm)
    mush_free(point->comm, "cqueue.comm");
  im_delete(queue_map, point->pid);
  mush_free(point, "cqueue");
}

static int
pay_queue(dbref player, const char *command)
{
  int estcost;
  estcost =
    QUEUE_COST +
    (QUEUE_LOSS ? ((get_random32(0, QUEUE_LOSS - 1) == 0) ? 1 : 0) : 0);
  if (!quiet_payfor(player, estcost)) {
    notify_format(Owner(player),
                  T("Not enough money to queue command for %s(#%d)."),
                  Name(player), player);
    return 0;
  }
  if (!NoPay(player) && (estcost != QUEUE_COST) && Track_Money(Owner(player))) {
    char *preserve_wnxt[10];
    char *preserve_rnxt[NUMQ];
    char *val_wnxt[10];
    char *val_rnxt[NUMQ];
    char *preserves[10];
    char *preserveq[NUMQ];
    save_global_nxt("pay_queue_save", preserve_wnxt, preserve_rnxt, val_wnxt,
                    val_rnxt);
    save_global_regs("pay_queue_save", preserveq);
    save_global_env("pay_queue_save", preserves);
    notify_format(Owner(player),
                  T("GAME: Object %s(%s) lost a %s to queue loss."),
                  Name(player), unparse_dbref(player), MONEY);
    restore_global_regs("pay_queue_save", preserveq);
    restore_global_env("pay_queue_save", preserves);
    restore_global_nxt("pay_queue_save", preserve_wnxt, preserve_rnxt, val_wnxt,
                       val_rnxt);
  }
  if (queue_limit(QUEUE_PER_OWNER ? Owner(player) : player)) {
    notify_format(Owner(player),
                  T("Runaway object: %s(%s). Commands halted."),
                  Name(player), unparse_dbref(player));
    do_log(LT_TRACE, player, player, "Runaway object %s executing: %s",
           unparse_dbref(player), command);
    /* Refund the queue costs */
    giveto(player, QUEUE_COST);
    add_to(QUEUE_PER_OWNER ? Owner(player) : player, -1);
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
    do_rawlog(LT_ERR,
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

/** Add a new entry onto the player or object command queues.
 * This function adds a new entry to the back of the player or
 * object command queues (depending on whether the call was
 * caused by a player or an object).
 * \param player the enactor for the queued command.
 * \param command the command to enqueue.
 * \param cause the player or object causing the command to be queued.
 */
void
parse_que(dbref player, const char *command, dbref cause)
{
  int a;
  BQUE *tmp;
  int pid;
  if (!IsPlayer(player) && (Halted(player)))
    return;
  if (!pay_queue(player, command))      /* make sure player can afford to do it */
    return;
  pid = next_pid();
  if (pid == 0) {
    /* Too many queue entries */
    notify(player, T("Queue entry table full. Try again later."));
    return;
  }
  tmp = mush_malloc(sizeof *tmp, "cqueue");
  tmp->pid = pid;
  tmp->comm = mush_strdup(command, "cqueue.comm");
  tmp->semattr = NULL;
  tmp->player = player;
  tmp->queued = QUEUE_PER_OWNER ? Owner(player) : player;
  tmp->next = NULL;
  tmp->left = 0;
  tmp->cause = cause;
  for (a = 0; a < 10; a++)
    if (!global_eval_context.wnxt[a])
      tmp->env[a] = NULL;
    else {
      tmp->env[a] = mush_strdup(global_eval_context.wnxt[a], "cqueue.env");
    }
  for (a = 0; a < NUMQ; a++)
    if (!global_eval_context.rnxt[a] || !global_eval_context.rnxt[a][0])
      tmp->rval[a] = NULL;
    else {
      tmp->rval[a] = mush_strdup(global_eval_context.rnxt[a], "cqueue.rval");
    }
  if (IsPlayer(cause)) {
    if (qlast) {
      qlast->next = tmp;
      qlast = tmp;
    } else
      qlast = qfirst = tmp;
  } else {
    if (qllast) {
      qllast->next = tmp;
      qllast = tmp;
    } else
      qllast = qlfirst = tmp;
  }
  im_insert(queue_map, tmp->pid, tmp);
}

int
inplace_queue_attribute(dbref thing, const char *atrname, dbref enactor,
                        int rsargs)
{
  ATTR *a;
  char *start, *command;
  int noparent = 0;
  char *bp;

  a = queue_attribute_getatr(thing, atrname, noparent);
  if (!a)
    return 0;
  if (!Can_Read_Attr(enactor, thing, a))
    return 0;

  global_eval_context.include_called = 1 + rsargs;
  start = safe_atr_value(a);
  command = start;
  /* Trim off $-command or ^-command prefix */
  if (*command == '$' || *command == '^') {
    do {
      command = strchr(command + 1, ':');
    } while (command && command[-1] == '\\');
    if (!command)
      /* Oops, had '$' or '^', but no ':' */
      command = start;
    else
      /* Skip the ':' */
      command++;
  }
  bp = global_eval_context.include_replace;
  safe_str(command, global_eval_context.include_replace, &bp);
  *bp = '\0';
  free(start);
  return 1;
}

/** Enqueue the action part of an attribute.
 * This function is a front-end to parse_que() that takes an attribute,
 * removes ^....: or $....: from its value, and queues what's left.
 * \param executor object containing the attribute.
 * \param atrname attribute name.
 * \param enactor the enactor.
 * \param noparent if true, parents of executor are not checked for atrname.
 * \retval 0 failure.
 * \retval 1 success.
 */
int
queue_attribute_base(dbref executor, const char *atrname, dbref enactor,
                     int noparent)
{
  ATTR *a;

  a = queue_attribute_getatr(executor, atrname, noparent);
  if (!a)
    return 0;
  queue_attribute_useatr(executor, a, enactor);
  return 1;
}

ATTR *
queue_attribute_getatr(dbref executor, const char *atrname, int noparent)
{
  return (noparent ? atr_get_noparent(executor, strupper(atrname)) :
          atr_get(executor, strupper(atrname)));
}

int
queue_attribute_useatr(dbref executor, ATTR *a, dbref enactor)
{
  char *start, *command;
  start = safe_atr_value(a);
  command = start;
  /* Trim off $-command or ^-command prefix */
  if (*command == '$' || *command == '^') {
    do {
      command = strchr(command + 1, ':');
    } while (command && command[-1] == '\\');
    if (!command)
      /* Oops, had '$' or '^', but no ':' */
      command = start;
    else
      /* Skip the ':' */
      command++;
  }
  parse_que(executor, command, enactor);
  free(start);
  return 1;
}


/** Queue an entry on the wait or semaphore queues.
 * This function creates and adds a queue entry to the wait queue
 * or the semaphore queue. Wait queue entries are sorted by when
 * they're due to expire; semaphore queue entries are just added
 * to the back of the queue.
 * \param player the enqueuing object.
 * \param waittill time to wait, or 0.
 * \param command command to enqueue.
 * \param cause object that caused command to be enqueued.
 * \param sem object to serve as a semaphore, or NOTHING.
 * \param semattr attribute to serve as a semaphore, or NULL (to use SEMAPHORE).
 * \param until 1 if we wait until an absolute time.
 */
void
wait_que(dbref player, int waittill, char *command, dbref cause, dbref sem,
         const char *semattr, int until)
{
  BQUE *tmp;
  int a;
  int pid;
  if (waittill == 0) {
    if (sem != NOTHING)
      add_to_sem(sem, -1, semattr);
    parse_que(player, command, cause);
    return;
  }
  if (!pay_queue(player, command))      /* make sure player can afford to do it */
    return;
  pid = next_pid();
  if (pid == 0) {
    notify(player, T("Queue entry table full. Try again later."));
    return;
  }
  tmp = mush_malloc(sizeof *tmp, "cqueue");
  tmp->comm = mush_strdup(command, "cqueue.comm");
  tmp->pid = pid;
  tmp->player = player;
  tmp->queued = QUEUE_PER_OWNER ? Owner(player) : player;
  tmp->cause = cause;
  tmp->semattr = NULL;
  tmp->next = NULL;
  for (a = 0; a < 10; a++) {
    if (!global_eval_context.wnxt[a])
      tmp->env[a] = NULL;
    else {
      tmp->env[a] = mush_strdup(global_eval_context.wnxt[a], "cqueue.env");
    }
  }
  for (a = 0; a < NUMQ; a++) {
    if (!global_eval_context.rnxt[a] || !global_eval_context.rnxt[a][0])
      tmp->rval[a] = NULL;
    else {
      tmp->rval[a] = mush_strdup(global_eval_context.rnxt[a], "cqueue.rval");
    }
  }

  if (until) {
    tmp->left = (time_t) waittill;
  } else {
    if (waittill >= 0)
      tmp->left = mudtime + waittill;
    else
      tmp->left = 0;            /* semaphore wait without a timeout */
  }
  tmp->sem = sem;
  if (sem == NOTHING) {
    /* No semaphore, put on normal wait queue, sorted by time */
    BQUE *point, *trail = NULL;

    for (point = qwait;
         point && (point->left <= tmp->left); point = point->next)
      trail = point;

    tmp->next = point;
    if (trail != NULL)
      trail->next = tmp;
    else
      qwait = tmp;
  } else {

    /* Put it on the end of the semaphore queue */
    tmp->semattr =
      mush_strdup(semattr ? semattr : "SEMAPHORE", "cqueue.semattr");
    if (qsemlast != NULL) {
      qsemlast->next = tmp;
      qsemlast = tmp;
    } else {
      qsemfirst = qsemlast = tmp;
    }
  }
  im_insert(queue_map, tmp->pid, tmp);
}

/** Once-a-second check for queued commands.
 * This function is called every second to check for commands
 * on the wait queue or semaphore queue, and to move a command
 * off the low priority object queue and onto the normal priority
 * player queue.
 */
void
do_second(void)
{
  BQUE *trail = NULL, *point, *next;
  /* move contents of low priority queue onto end of normal one
   * this helps to keep objects from getting out of control since
   * its effects on other objects happen only after one second
   * this should allow @halt to be typed before getting blown away
   * by scrolling text.
   */
  if (qlfirst) {
    if (qlast)
      qlast->next = qlfirst;
    else
      qfirst = qlfirst;
    qlast = qllast;
    qllast = qlfirst = NULL;
  }
  /* check regular wait queue */

  while (qwait && qwait->left <= mudtime) {
    point = qwait;
    qwait = point->next;
    point->next = NULL;
    point->left = 0;
    if (IsPlayer(point->cause)) {
      if (qlast) {
        qlast->next = point;
        qlast = point;
      } else
        qlast = qfirst = point;
    } else {
      if (qllast) {
        qllast->next = point;
        qllast = point;
      } else
        qllast = qlfirst = point;
    }
  }

  /* check for semaphore timeouts */

  for (point = qsemfirst, trail = NULL; point; point = next) {
    if (point->left == 0 || point->left > mudtime) {
      next = (trail = point)->next;
      continue;                 /* skip non-timed and those that haven't gone off yet */
    }
    if (trail != NULL)
      trail->next = next = point->next;
    else
      qsemfirst = next = point->next;
    if (point == qsemlast)
      qsemlast = trail;
    add_to_sem(point->sem, -1, point->semattr);
    point->sem = NOTHING;
    point->next = NULL;
    if (IsPlayer(point->cause)) {
      if (qlast) {
        qlast->next = point;
        qlast = point;
      } else
        qlast = qfirst = point;
    } else {
      if (qllast) {
        qllast->next = point;
        qllast = point;
      } else
        qllast = qlfirst = point;
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
  BQUE *entry;

  for (i = 0; i < ncom; i++) {
    if (!qfirst) {
      strcpy(global_eval_context.ccom, "");
      return i;
    }
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

static int
do_entry(BQUE *entry, int include_recurses)
{
  int a;
  char tbuf[BUFFER_LEN];
  int break_count;
  int save_player;
  int local_break_called = 0;
  char *r;
  char const *s;
  if (GoodObject(entry->player) && !IsGarbage(entry->player)) {
    save_player = global_eval_context.cplr = entry->player;
    if (!global_eval_context.include_called) {
      giveto(global_eval_context.cplr, QUEUE_COST);
      add_to(entry->queued, -1);
    }
    entry->player = 0;
    if (IsPlayer(global_eval_context.cplr)
        || !Halted(global_eval_context.cplr)) {
      for (a = 0; a < 10; a++)
        global_eval_context.wenv[a] = entry->env[a];
      for (a = 0; a < NUMQ; a++) {
        if (entry->rval[a])
          strcpy(global_eval_context.renv[a], entry->rval[a]);
        else
          global_eval_context.renv[a][0] = '\0';
      }
      global_eval_context.process_command_port = 0;
      s = entry->comm;
      global_eval_context.break_called = 0;
      local_break_called = 0;
      break_count = 100;
      *(global_eval_context.break_replace) = '\0';
      if (!include_recurses)
        start_cpu_timer();
      while (!cpu_time_limit_hit && *s) {
        r = global_eval_context.ccom;
        process_expression(global_eval_context.ccom, &r, &s,
                           global_eval_context.cplr, entry->cause,
                           entry->cause, PE_NOTHING, PT_SEMI, NULL);
        *r = '\0';
        if (*s == ';')
          s++;
        strcpy(tbuf, global_eval_context.ccom);
        process_command(global_eval_context.cplr, tbuf, entry->cause, 0);
        if (global_eval_context.break_called) {
          global_eval_context.break_called = 0;
          local_break_called = 1;
          s = global_eval_context.break_replace;
          if (!*global_eval_context.break_replace)
            break;
          break_count--;
          if (!break_count) {
            notify(global_eval_context.cplr, T("@break recursion exceeded."));
            break;
          }
        }
        if (global_eval_context.include_called) {
          BQUE *tmp;
          /* @include was called. Check for recursion limit */
          if (include_recurses > 20) {
            notify(global_eval_context.cplr, T("@include recursion exceeded."));
            break;
          }
          if (!*global_eval_context.include_replace)
            break;
          /* Clone qentry */
          tmp = mush_malloc(sizeof *tmp, "cqueue");
          tmp->pid = entry->pid;
          tmp->semattr = NULL;
          tmp->player = save_player;
          tmp->queued = entry->queued;
          tmp->next = NULL;
          tmp->left = 0;
          tmp->cause = entry->cause;
          for (a = 0; a < 10; a++) {
            if (global_eval_context.include_called == 1) {
              tmp->env[a] =
                entry->env[a] ? mush_strdup(entry->env[a], "cqueue.env") : NULL;
            } else {
              if (global_eval_context.include_wenv[a]) {
                tmp->env[a] =
                  mush_strdup(global_eval_context.include_wenv[a],
                              "cqueue.env");
              } else {
                tmp->env[a] = NULL;
              }
            }
            if (global_eval_context.include_wenv[a]) {
              mush_free(global_eval_context.include_wenv[a], "include_wenv");
              global_eval_context.include_wenv[a] = NULL;
            }
          }
          for (a = 0; a < NUMQ; a++)
            if (!entry->rval[a])
              tmp->rval[a] = NULL;
            else {
              tmp->rval[a] = mush_strdup(entry->rval[a], "cqueue.rval");
            }
          global_eval_context.include_called = 0;
          /* Put the included actions in the clone */
          tmp->comm =
            mush_strdup(global_eval_context.include_replace, "cqueue.comm");
          local_break_called = do_entry(tmp, include_recurses + 1);
          for (a = 0; a < 10; a++)
            global_eval_context.wenv[a] = entry->env[a];
          free_qentry(tmp);
          if (local_break_called) {
            /* Propagate break */
            break;
          }
        }
      }
      if (!include_recurses)
        reset_cpu_timer();
    }
  }
  return local_break_called;
}

/** Determine whether it's time to run a queued command.
 * This function returns the number of seconds we expect to wait
 * before it's time to run a queued command.
 * If there are commands in the player queue, that's 0.
 * If there are commands in the object queue, that's 1.
 * Otherwise, we check wait and semaphore queues to see what's next.
 * \return seconds left before a queue entry will be ready.
 */
int
que_next(void)
{
  int min, curr;
  BQUE *point;
  /* If there are commands in the player queue, they should be run
   * immediately.
   */
  if (qfirst != NULL)
    return 0;
  /* If there are commands in the object queue, they should be run in
   * one second.
   */
  if (qlfirst != NULL)
    return 1;
  /* Check out the wait and semaphore queues, looking for the smallest
   * wait value. Return that - 1, since commands get moved to the player
   * queue when they have one second to go.
   */
  min = 5;

  /* Wait queue is in sorted order so we only have to look at the first
     item on it. Anything else is wasted time. */
  if (qwait) {
    curr = (int) difftime(qwait->left, mudtime);
    if (curr <= 2)
      return 1;
    if (curr < min)
      min = curr;
  }

  for (point = qsemfirst; point; point = point->next) {
    if (point->left == 0)       /* no timeout */
      continue;
    curr = (int) difftime(point->left, mudtime);
    if (curr <= 2)
      return 1;
    if (curr < min) {
      min = curr;
    }
  }

  return (min - 1);
}

static int
drain_helper(dbref player __attribute__ ((__unused__)), dbref thing,
             dbref parent __attribute__ ((__unused__)),
             char const *pattern __attribute__ ((__unused__)), ATTR *atr,
             void *args __attribute__ ((__unused__)))
{
  if (waitable_attr(thing, AL_NAME(atr)))
    (void) atr_clr(thing, AL_NAME(atr), GOD);
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
  BQUE **point;
  BQUE *entry;

  if (all)
    count = INT_MAX;

  /* Go through the semaphore queue and do it */
  point = &qsemfirst;
  while (*point && count > 0) {
    entry = *point;
    if (entry->sem != thing || (aname && strcmp(entry->semattr, aname))) {
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
    add_to_sem(entry->sem, -1, entry->semattr);

    /* Dispose of the entry as appropriate: discard if @drain, or put
     * into either the player or the object queue. */
    if (drain) {
      giveto(entry->player, QUEUE_COST);
      add_to(entry->queued, -1);
      free_qentry(entry);
    } else if (IsPlayer(entry->cause)) {
      if (qlast) {
        qlast->next = entry;
        qlast = entry;
      } else {
        qlast = qfirst = entry;
      }
    } else {
      if (qllast) {
        qllast->next = entry;
        qllast = entry;
      } else {
        qllast = qlfirst = entry;
      }
    }
  }

  /* If @drain/all, clear the relevant attribute(s) */
  if (drain && all) {
    if (aname)
      (void) atr_clr(thing, aname, GOD);
    else
      atr_iter_get(GOD, thing, "**", 0, 0, drain_helper, NULL);
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

  /* Figure out whether we're in notify or drain */
  drain = (cmd->name[1] == 'D');

  /* Make sure they gave an object ref */
  if (!arg_left || !*arg_left) {
    notify(player, T("You must specify an object to use for the semaphore."));
    return;
  }

  /* Figure out which attribute we're using */
  pos = strchr(arg_left, '/');
  if (pos) {
    if (SW_ISSET(sw, SWITCH_ANY)) {
      notify(player,
             T
             ("You may not specify a semaphore attribute with the ANY switch."));
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
  thing = noisy_match_result(player, arg_left, NOTYPE, MAT_EVERYTHING);
  if (!GoodObject(thing))
    return;
  /* must control something or have it link_ok in order to use it as
   * as a semaphore.
   */
  if ((!controls(player, thing) && !LinkOk(thing))
      || (aname && !waitable_attr(thing, aname))) {
    notify(player, T("Permission denied."));
    return;
  }

  /* Figure out how many times to notify */
  all = SW_ISSET(sw, SWITCH_ALL);
  if (arg_right && *arg_right) {
    if (all) {
      notify(player,
             T("You may not specify a semaphore count with the ALL switch."));
      return;
    }
    if (!is_uinteger(arg_right)) {
      notify(player, T("The semaphore count must be an integer."));
      return;
    }
    count = parse_integer(arg_right);
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
    quiet_notify(player, T("Drained."));
  } else {
    quiet_notify(player, T("Notified."));
  }
}

/** Softcode interface to add a command to the wait or semaphore queue.
 * \verbatim
 * This is the top-level function for @wait.
 * \endverbatim
 * \param player the enactor
 * \param cause the object causing the command to be added.
 * \param arg1 the wait time, semaphore object/attribute, or both. Modified!
 * \param cmd command to queue.
 * \param until if 1, wait until an absolute time.
 */
void
do_wait(dbref player, dbref cause, char *arg1, const char *cmd, bool until)
{
  dbref thing;
  char *tcount = NULL, *aname = NULL;
  int waitfor, num;
  ATTR *a;
  char *arg2;
  int j;
  for (j = 0; j < 10; j++)
    global_eval_context.wnxt[j] = global_eval_context.wenv[j];
  for (j = 0; j < NUMQ; j++)
    global_eval_context.rnxt[j] = global_eval_context.renv[j];

  arg2 = strip_braces(cmd);
  if (is_strict_integer(arg1)) {
    /* normal wait */
    wait_que(player, parse_integer(arg1), arg2, cause, NOTHING, NULL, until);
    mush_free(arg2, "strip_braces.buff");
    return;
  }
  /* semaphore wait with optional timeout */

  /* find the thing to wait on */
  aname = strchr(arg1, '/');
  if (aname)
    *aname++ = '\0';
  if ((thing =
       noisy_match_result(player, arg1, NOTYPE, MAT_EVERYTHING)) == NOTHING) {
    mush_free(arg2, "strip_braces.buff");
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
      if (is_strict_integer(aname)) {   /* Timeout */
        tcount = aname;
        aname = (char *) "SEMAPHORE";
      } else {                  /* Attribute */
        upcasestr(aname);
      }
    } else {                    /* attribute/timeout */
      *tcount++ = '\0';
      upcasestr(aname);
    }
  } else {
    aname = (char *) "SEMAPHORE";
  }

  if ((!controls(player, thing) && !LinkOk(thing))
      || (aname && !waitable_attr(thing, aname))) {
    notify(player, T("Permission denied."));
    mush_free(arg2, "strip_braces.buff");
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
    waitfor = -1;               /* just in case there was a timeout given */
  }
  wait_que(player, waitfor, arg2, cause, thing, aname, until);
  mush_free(arg2, "strip_braces.buff");
}

/** Interface to @wait/pid; modifies the wait times of queue
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
  BQUE *q, *tmp, *last;
  bool found;

  if (!is_uinteger(pidstr)) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  pid = parse_uint32(pidstr, NULL, 10);
  q = im_find(queue_map, pid);

  if (!q) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  if (!controls(player, q->player) && !HaltAny(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (q->sem != NOTHING && q->left == 0) {
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

    q->left = (time_t) when;

  } else {
    int offset = parse_integer(timestr);

    /* If timestr looks like +NNN or -NNN, add or subtract a number
       of seconds to the current timeout. Otherwise, change timeout.
     */
    if (timestr[0] == '+' || timestr[0] == '-')
      q->left += offset;
    else
      q->left = mudtime + offset;

    if (q->left < 0)
      q->left = 0;
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
      if (tmp->left > q->left) {
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
  char *osep, osepd[2] = { ' ', '\0' };
  char *fields, field[80] = "queue player time object attribute command";
  uint32_t pid;
  BQUE *q;
  bool first = true;

  if (!is_uinteger(args[0])) {
    safe_str(T(e_num), buff, bp);
    return;
  }

  pid = parse_uint32(args[0], NULL, 10);
  q = im_find(queue_map, pid);

  if (!q) {
    safe_str(T("#-1 INVALID PID"), buff, bp);
    return;
  }

  if (!controls(executor, q->player) && !LookQueue(executor)) {
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
  else {
    osep = osepd;
  }

  s = trim_space_sep(fields, ' ');
  do {
    r = split_token(&s, ' ');
    if (string_prefix("queue", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      if (GoodObject(q->sem))
        safe_str("semaphore", buff, bp);
      else
        safe_str("wait", buff, bp);
    } else if (string_prefix("player", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      safe_dbref(q->player, buff, bp);
    } else if (string_prefix("time", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      if (q->left == 0)
        safe_integer(-1, buff, bp);
      else
        safe_integer(difftime(q->left, mudtime), buff, bp);
    } else if (string_prefix("object", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      safe_dbref(q->sem, buff, bp);
    } else if (string_prefix("attribute", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      if (GoodObject(q->sem)) {
        safe_str(q->semattr, buff, bp);
      } else {
        safe_dbref(NOTHING, buff, bp);
      }
    } else if (string_prefix("command", r)) {
      if (!first)
        safe_str(osep, buff, bp);
      first = false;
      safe_str(q->comm, buff, bp);
    }
  } while (s);
}

FUNCTION(fun_lpids)
{
  /* Can be called as LPIDS or GETPIDS */
  BQUE *tmp;
  int qmask = 3;
  dbref thing = -1;
  dbref player = -1;
  char *attr = NULL;
  bool first = true;
  if (string_prefix(called_as, "LPIDS")) {
    /* lpids(player[,type]) */
    if (args[0] && *args[0]) {
      player = match_thing(executor, args[0]);
      if (!GoodObject(player)) {
        safe_str(T(e_notvis), buff, bp);
        return;
      }
      if (!(LookQueue(executor) || (Owner(player) == executor))) {
        safe_str(T(e_perm), buff, bp);
        return;
      }
    } else if (!LookQueue(executor)) {
      player = executor;
    }
    if ((nargs == 2) && args[1] && *args[1]) {
      if (*args[1] == 'W' || *args[1] == 'w')
        qmask = 1;
      else if (*args[1] == 'S' || *args[1] == 's')
        qmask = 2;
    }
  } else {
    /* getpids(obj[/attrib]) */
    qmask = 2;                  /* semaphores only */
    attr = strchr(args[0], '/');
    if (attr)
      *attr++ = '\0';
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

  if (qmask & 1) {
    for (tmp = qwait; tmp; tmp = tmp->next) {
      if (GoodObject(player) && (!Owns(tmp->player, player)))
        continue;
      if (!first)
        safe_chr(' ', buff, bp);
      safe_integer(tmp->pid, buff, bp);
      first = false;
    }
  }
  if (qmask & 2) {
    for (tmp = qsemfirst; tmp; tmp = tmp->next) {
      if (GoodObject(player) && (!Owns(tmp->player, player)))
        continue;
      if (GoodObject(thing) && (tmp->sem != thing))
        continue;
      if (attr && *attr && strcasecmp(tmp->semattr, attr))
        continue;
      if (!first)
        safe_chr(' ', buff, bp);
      safe_integer(tmp->pid, buff, bp);
      first = false;
    }
  }
}

static void
show_queue(dbref player, dbref victim, int q_type, int q_quiet,
           int q_all, BQUE *q_ptr, int *tot, int *self, int *del)
{
  BQUE *tmp;
  for (tmp = q_ptr; tmp; tmp = tmp->next) {
    (*tot)++;
    if (!GoodObject(tmp->player))
      (*del)++;
    else if (q_all || (Owner(tmp->player) == victim)) {
      if ((LookQueue(player)
           || Owns(tmp->player, player))) {
        (*self)++;
        if (q_quiet)
          continue;
        switch (q_type) {
        case 1:                /* wait queue */
          notify_format(player, "(Pid: %u) [%ld]%s: %s",
                        (unsigned int) tmp->pid, (long) difftime(tmp->left,
                                                                 mudtime),
                        unparse_object(player, tmp->player), tmp->comm);
          break;
        case 2:                /* semaphore queue */
          if (tmp->left != 0) {
            notify_format(player, "(Pid: %u) [#%d/%s/%ld]%s: %s",
                          (unsigned int) tmp->pid, tmp->sem, tmp->semattr,
                          (long) difftime(tmp->left, mudtime),
                          unparse_object(player, tmp->player), tmp->comm);
          } else {
            notify_format(player, "(Pid: %u) [#%d/%s]%s: %s",
                          (unsigned int) tmp->pid, tmp->sem, tmp->semattr,
                          unparse_object(player, tmp->player), tmp->comm);
          }
          break;
        default:               /* player or object queue */
          notify_format(player, "(Pid: %u) %s: %s", (unsigned int) tmp->pid,
                        unparse_object(player, tmp->player), tmp->comm);
        }
      }
    }
  }
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
  int dpq = 0, doq = 0, dwq = 0, dsq = 0;
  int pq = 0, oq = 0, wq = 0, sq = 0;
  int tpq = 0, toq = 0, twq = 0, tsq = 0;
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
        notify_format(player, T("Queue for : %s"), Name(victim));
    }
    victim = Owner(victim);
    if (!quick)
      notify(player, T("Player Queue:"));
    show_queue(player, victim, 0, quick, all, qfirst, &tpq, &pq, &dpq);
    if (!quick)
      notify(player, T("Object Queue:"));
    show_queue(player, victim, 0, quick, all, qlfirst, &toq, &oq, &doq);
    if (!quick)
      notify(player, T("Wait Queue:"));
    show_queue(player, victim, 1, quick, all, qwait, &twq, &wq, &dwq);
    if (!quick)
      notify(player, T("Semaphore Queue:"));
    show_queue(player, victim, 2, quick, all, qsemfirst, &tsq, &sq, &dsq);
    if (!quick)
      notify(player, T("------------  Queue Done  ------------"));
    notify_format(player,
                  T
                  ("Totals: Player...%d/%d[%ddel]  Object...%d/%d[%ddel]  Wait...%d/%d[%ddel]  Semaphore...%d/%d"),
                  pq, tpq, dpq, oq, toq, doq, wq, twq, dwq, sq, tsq);
  }
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
  BQUE *tmp, *trail = NULL, *point, *next;
  int num = 0;
  dbref player;
  if (victim == NOTHING)
    player = owner;
  else
    player = victim;
  quiet_notify(Owner(player),
               tprintf("%s: %s(#%d).", T("Halted"), Name(player), player));
  for (tmp = qfirst; tmp; tmp = tmp->next)
    if (GoodObject(tmp->player)
        && ((tmp->player == player)
            || (Owner(tmp->player) == player))) {
      num--;
      giveto(player, QUEUE_COST);
      tmp->player = NOTHING;
    }
  for (tmp = qlfirst; tmp; tmp = tmp->next)
    if (GoodObject(tmp->player)
        && ((tmp->player == player)
            || (Owner(tmp->player) == player))) {
      num--;
      giveto(player, QUEUE_COST);
      tmp->player = NOTHING;
    }
  /* remove wait q stuff */
  for (point = qwait; point; point = next) {
    if (((point->player == player)
         || (Owner(point->player) == player))) {
      num--;
      giveto(player, QUEUE_COST);
      if (trail)
        trail->next = next = point->next;
      else
        qwait = next = point->next;
      free_qentry(point);
    } else
      next = (trail = point)->next;
  }

  /* clear semaphore queue */

  for (point = qsemfirst, trail = NULL; point; point = next) {
    if (((point->player == player)
         || (Owner(point->player) == player))) {
      num--;
      giveto(player, QUEUE_COST);
      if (trail)
        trail->next = next = point->next;
      else
        qsemfirst = next = point->next;
      if (point == qsemlast)
        qsemlast = trail;
      add_to_sem(point->sem, -1, point->semattr);
      free_qentry(point);
    } else
      next = (trail = point)->next;
  }

  add_to(QUEUE_PER_OWNER ? Owner(player) : player, num);
  if (ncom && *ncom) {
    int j;
    for (j = 0; j < 10; j++)
      global_eval_context.wnxt[j] = global_eval_context.wenv[j];
    for (j = 0; j < NUMQ; j++)
      global_eval_context.rnxt[j] = global_eval_context.renv[j];
    parse_que(player, ncom, player);
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
    if ((victim =
         noisy_match_result(player, arg1, NOTYPE,
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
        notify_format(player,
                      T("All objects for %s have been halted."), Name(victim));
        notify_format(victim,
                      T("All of your objects have been halted by %s."),
                      Name(player));
      }
    } else {
      if (Owner(victim) != player) {
        notify_format(player, "%s: %s's %s(%s)", T("Halted"),
                      Name(Owner(victim)), Name(victim), unparse_dbref(victim));
        notify_format(Owner(victim), "%s: %s(%s), by %s", T("Halted"),
                      Name(victim), unparse_dbref(victim), Name(player));
      }
      if (*arg2 == '\0')
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
  BQUE *q;
  dbref victim;
  if (!is_uinteger(arg1)) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  pid = parse_uint32(arg1, NULL, 10);
  q = im_find(queue_map, pid);
  if (!q) {
    notify(player, T("That is not a valid pid!"));
    return;
  }

  victim = q->player;
  if (!controls(player, victim) && !HaltAny(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  /* Instead of trying to track what queue this entry currently
     belongs too, flag it as halted and just not execute it when its
     turn comes up (Or show it in @ps, etc.).  Exception is for
     semaphores, which otherwise might wait forever. */
  q->player = NOTHING;
  if (q->semattr) {
    BQUE *last = NULL, *tmp;
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
    add_to_sem(q->sem, -1, q->semattr);
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
      notify_format(victim,
                    T("Your objects have been globally halted by %s"),
                    Name(player));
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
      (void) queue_attribute_noparent(thing, "STARTUP", thing);
      do_top(5);
    }
    if (IsPlayer(thing)) {
      notify_format(thing,
                    T
                    ("Your objects are being globally restarted by %s"),
                    Name(player));
    }
  }
}

static void
do_raw_restart(dbref victim)
{
  dbref thing;
  if (IsPlayer(victim)) {
    for (thing = 0; thing < db_top; thing++) {
      if ((Owner(thing) == victim) && !IsGarbage(thing)
          && !(Halted(thing)))
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
    if ((victim =
         noisy_match_result(player, arg1, NOTYPE, MAT_OBJECTS)) == NOTHING)
      return;
    if (!Owns(player, victim) && !HaltAny(player)) {
      notify(player, T("Permission denied."));
      return;
    }
    if (Owner(victim) != player) {
      if (IsPlayer(victim)) {
        notify_format(player,
                      T("All objects for %s are being restarted."),
                      Name(victim));
        notify_format(victim,
                      T
                      ("All of your objects are being restarted by %s."),
                      Name(player));
      } else {
        notify_format(player,
                      T("Restarting: %s's %s(%s)"),
                      Name(Owner(victim)), Name(victim), unparse_dbref(victim));
        notify_format(Owner(victim), T("Restarting: %s(%s), by %s"),
                      Name(victim), unparse_dbref(victim), Name(player));
      }
    } else {
      if (victim == player)
        notify(player, T("All of your objects are being restarted."));
      else
        notify_format(player, T("Restarting: %s(%s)"), Name(victim),
                      unparse_dbref(victim));
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
  shutdown_a_queue(&qlfirst, &qllast);
  shutdown_a_queue(&qsemfirst, &qsemlast);
  shutdown_a_queue(&qwait, NULL);
}


static void
shutdown_a_queue(BQUE **head, BQUE **tail)
{
  BQUE *entry;
  /* Drain out a queue */
  while (*head) {
    entry = *head;
    if (!(*head = entry->next) && tail)
      *tail = NULL;
    if (GoodObject(entry->player) && !IsGarbage(entry->player)) {
      global_eval_context.cplr = entry->player;
      giveto(global_eval_context.cplr, QUEUE_COST);
      add_to(entry->queued, -1);
    }
    free_qentry(entry);
  }
}
