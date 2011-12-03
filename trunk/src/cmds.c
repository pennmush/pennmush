/**
 * \file cmds.c
 *
 * \brief Definitions of commands.
 * This file is a set of functions that defines commands. The parsing
 * of commands is elsewhere (command.c), as are the implementations
 * of most of the commands (throughout the source.)
 *
 */

#include "copyrite.h"
#include "config.h"

#include <string.h>

#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "mushdb.h"
#include "match.h"
#include "game.h"
#include "attrib.h"
#include "extmail.h"
#include "malias.h"
#include "parse.h"
#include "access.h"
#include "version.h"
#include "lock.h"
#include "function.h"
#include "command.h"
#include "flags.h"
#include "log.h"
#include "confmagic.h"

/* External Stuff */
void do_poor(dbref player, char *arg1);
void do_list_memstats(dbref player);

#define DOL_NOTIFY 2            /**< dolist/notify bitflag */
#define DOL_DELIM 4             /**< dolist/delim bitflag */

void do_list(dbref player, char *arg, int lc, int which);
void do_writelog(dbref player, char *str, int ltype);
void do_readcache(dbref player);
void do_uptime(dbref player, int mortal);
extern int config_set(const char *opt, char *val, int source, int restrictions);

void do_list_allocations(dbref player);

/** Is there a right-hand side of the equal sign? From command.c */
extern int rhs_present;

COMMAND(cmd_allhalt)
{
  do_allhalt(executor);
}

COMMAND(cmd_allquota)
{
  do_allquota(executor, arg_left, SW_ISSET(sw, SWITCH_QUIET));
}

COMMAND(cmd_atrlock)
{
  do_atrlock(executor, arg_left, arg_right);
}

COMMAND(cmd_attribute)
{
  if (SW_ISSET(sw, SWITCH_ACCESS)) {
    if (Wizard(executor))
      do_attribute_access(executor, arg_left, arg_right,
                          SW_ISSET(sw, SWITCH_RETROACTIVE));
    else
      notify(executor, T("Permission denied."));
  } else if (SW_ISSET(sw, SWITCH_DELETE)) {
    if (Wizard(executor))
      do_attribute_delete(executor, arg_left);
    else
      notify(executor, T("Permission denied."));
  } else if (SW_ISSET(sw, SWITCH_RENAME)) {
    if (Wizard(executor))
      do_attribute_rename(executor, arg_left, arg_right);
    else
      notify(executor, T("Permission denied."));
  } else if (SW_ISSET(sw, SWITCH_LIMIT)) {
    if (Wizard(executor))
      do_attribute_limit(executor, arg_left, AF_RLIMIT, arg_right);
    else
      notify(executor, T("Permission denied."));
  } else if (SW_ISSET(sw, SWITCH_ENUM)) {
    if (Wizard(executor))
      do_attribute_limit(executor, arg_left, AF_ENUM, arg_right);
    else
      notify(executor, T("Permission denied."));
  } else
    do_attribute_info(executor, arg_left);
}

COMMAND(cmd_atrchown)
{
  do_atrchown(executor, arg_left, arg_right);
}

COMMAND(cmd_boot)
{

  int silent = (SW_ISSET(sw, SWITCH_SILENT));

  if (SW_ISSET(sw, SWITCH_ME))
    do_boot(executor, (char *) NULL, BOOT_SELF, silent, queue_entry);
  else if (SW_ISSET(sw, SWITCH_PORT))
    do_boot(executor, arg_left, BOOT_DESC, silent, queue_entry);
  else
    do_boot(executor, arg_left, BOOT_NAME, silent, queue_entry);

}

COMMAND(cmd_break)
{
  if (parse_boolean(arg_left)) {
    queue_entry->queue_type |= QUEUE_BREAK;
    if (arg_right && *arg_right) {
      new_queue_actionlist(executor, enactor, caller, arg_right, queue_entry,
                           PE_INFO_SHARE, QUEUE_INPLACE, NULL);
    }
  }
}

COMMAND(cmd_assert)
{
  if (!parse_boolean(arg_left)) {
    queue_entry->queue_type |= QUEUE_BREAK;
    if (arg_right && *arg_right) {
      new_queue_actionlist(executor, enactor, caller, arg_right, queue_entry,
                           PE_INFO_SHARE, QUEUE_INPLACE, NULL);
    }
  }
}

COMMAND(cmd_retry)
{
  char buff[BUFFER_LEN], *bp;
  const char *sp;
  PE_REGS *pe_regs = NULL;
  PE_REGS *pr;
  int a;

  if (!parse_boolean(arg_left))
    return;

  if (rhs_present) {
    /* Now, to evaluate all of rsargs. Blah. */
    pe_regs = pe_regs_create(PE_REGS_ARG, "cmd_retry");
    for (a = 0; a < 10; a++) {
      sp = args_right[a + 1];
      if (sp) {
        bp = buff;
        if (process_expression(buff, &bp, &sp, executor, caller, enactor,
                               PE_DEFAULT, PT_DEFAULT, queue_entry->pe_info)) {
          pe_regs_free(pe_regs);
          return;
        }
        *bp = '\0';
        pe_regs_setenv(pe_regs, a, buff);
      }
    }
    /* Find the pe_regs relevant to this queue entry, and copy our
     * new args onto it */
    for (pr = queue_entry->pe_info->regvals; pr; pr = pr->prev) {
      if (pr->flags & PE_REGS_ARG) {
        pe_regs_copystack(pr, pe_regs, PE_REGS_ARG, 1);
        break;
      }
    }
  }
  queue_entry->queue_type |= QUEUE_RETRY;
  if (pe_regs)
    pe_regs_free(pe_regs);
}

COMMAND(cmd_chownall)
{
  do_chownall(executor, arg_left, arg_right, SW_ISSET(sw, SWITCH_PRESERVE));
}

COMMAND(cmd_chown)
{
  do_chown(executor, arg_left, arg_right, SW_ISSET(sw, SWITCH_PRESERVE),
           queue_entry->pe_info);
}

COMMAND(cmd_chzoneall)
{
  do_chzoneall(executor, arg_left, arg_right, SW_ISSET(sw, SWITCH_PRESERVE));
}

COMMAND(cmd_chzone)
{
  do_chzone(executor, arg_left, arg_right, 1, SW_ISSET(sw, SWITCH_PRESERVE),
            queue_entry->pe_info);
}

COMMAND(cmd_config)
{
  if (SW_ISSET(sw, SWITCH_SET) || SW_ISSET(sw, SWITCH_SAVE)) {
    if (!Wizard(executor)) {
      notify(executor, T("You can't remake the world in your image."));
      return;
    }
    if (!arg_left || !*arg_left) {
      notify(executor, T("What did you want to set?"));
      return;
    }
    {
      int source = SW_ISSET(sw, SWITCH_SAVE) ? 2 : 1;
      if (source == 2) {
        if (!God(executor)) {
          /* Only god can alter the original config file. */
          notify(executor, T("You can't remake the world in your image."));
          return;
        }
      }
      if (!config_set(arg_left, arg_right, source, 0)
          && !config_set(arg_left, arg_right, source, 1))
        notify(executor, T("Couldn't set that option."));
      else {
        if (source == 2) {
#ifdef HAVE_ED
          notify(executor, T("Option set and saved."));
#else
          notify(executor, T("Option set but not saved (Saves disabled.)"));
#endif
        } else
          notify(executor, T("Option set."));
      }
    }
  } else
    do_config_list(executor, arg_left, SW_ISSET(sw, SWITCH_LOWERCASE));
}

COMMAND(cmd_cpattr)
{
  do_cpattr(executor, arg_left, args_right, 0, SW_ISSET(sw, SWITCH_NOFLAGCOPY));
}

COMMAND(cmd_create)
{

  int cost = 0;
  char *newdbref;

  if (args_right[1] && *args_right[1])
    cost = parse_integer(args_right[1]);

  if (args_right[2] && *args_right[2])
    newdbref = args_right[2];
  else
    newdbref = (char *) NULL;

  do_create(executor, arg_left, cost, newdbref);
}

COMMAND(cmd_clone)
{
  if (SW_ISSET(sw, SWITCH_PRESERVE))
    do_clone(executor, arg_left, args_right[1], SWITCH_PRESERVE, args_right[2],
             queue_entry->pe_info);
  else
    do_clone(executor, arg_left, args_right[1], SWITCH_NONE, args_right[2],
             queue_entry->pe_info);
}

COMMAND(cmd_dbck)
{
  do_dbck(executor);
}

COMMAND(cmd_decompile)
{
  char prefix[BUFFER_LEN];
  int flags = 0, dbflags = 0;
  *prefix = '\0';
  if (SW_ISSET(sw, SWITCH_SKIPDEFAULTS))
    flags |= DEC_SKIPDEF;
  if (SW_ISSET(sw, SWITCH_TF)) {
    /* @dec/tf overrides =<prefix> */
    ATTR *a;
    if (((a = atr_get_noparent(executor, "TFPREFIX")) != NULL) &&
        AL_STR(a) && *AL_STR(a)) {
      strcpy(prefix, atr_value(a));
    } else {
      strcpy(prefix, "FugueEdit > ");
    }
    flags |= DEC_TF;            /* Don't decompile attr flags */
  } else {
    strcpy(prefix, arg_right);
  }

  if (SW_ISSET(sw, SWITCH_DB) || SW_ISSET(sw, SWITCH_TF))
    flags |= DEC_DB;
  if (SW_ISSET(sw, SWITCH_NAME))
    flags &= ~DEC_DB;
  if (SW_ISSET(sw, SWITCH_FLAGS))
    dbflags |= DEC_FLAG;
  if (SW_ISSET(sw, SWITCH_ATTRIBS))
    dbflags |= DEC_ATTR;
  if (!dbflags)
    dbflags = DEC_FLAG | DEC_ATTR;

  do_decompile(executor, arg_left, prefix, flags | dbflags);
}

COMMAND(cmd_teach)
{
  do_teach(executor, arg_left, SW_ISSET(sw, SWITCH_LIST), queue_entry);
}

COMMAND(cmd_destroy)
{
  do_destroy(executor, arg_left, (SW_ISSET(sw, SWITCH_OVERRIDE)),
             queue_entry->pe_info);
}

COMMAND(cmd_dig)
{
  do_dig(executor, arg_left, args_right, (SW_ISSET(sw, SWITCH_TELEPORT)),
         queue_entry->pe_info);
}

COMMAND(cmd_disable)
{
  do_enable(executor, arg_left, 0);
}

COMMAND(cmd_doing)
{
  if (SW_ISSET(sw, SWITCH_HEADER))
    do_poll(executor, arg_left, 0);
  else
    do_doing(executor, arg_left);
}

COMMAND(cmd_dolist)
{
  unsigned int flags = 0;
  if (SW_ISSET(sw, SWITCH_NOTIFY))
    flags |= DOL_NOTIFY;
  if (SW_ISSET(sw, SWITCH_DELIMIT))
    flags |= DOL_DELIM;
  do_dolist(executor, arg_left, arg_right, enactor, flags, queue_entry);
}

COMMAND(cmd_dump)
{
  if (SW_ISSET(sw, SWITCH_PARANOID))
    do_dump(executor, arg_left, DUMP_PARANOID);
  else if (SW_ISSET(sw, SWITCH_DEBUG))
    do_dump(executor, arg_left, DUMP_DEBUG);
  else
    do_dump(executor, arg_left, DUMP_NORMAL);
}

COMMAND(cmd_edit)
{
  int type = EDIT_DEFAULT;

  if (SW_ISSET(sw, SWITCH_FIRST))
    type |= EDIT_FIRST;
  if (SW_ISSET(sw, SWITCH_CHECK))
    type |= EDIT_CHECK;
  if (SW_ISSET(sw, SWITCH_QUIET))
    type |= EDIT_QUIET;

  do_gedit(executor, arg_left, args_right, type);
}

COMMAND(cmd_elock)
{
  do_lock(executor, arg_left, arg_right, Enter_Lock);
}

COMMAND(cmd_emit)
{
  int spflags = (!strcmp(cmd->name, "@NSEMIT")
                 && Can_Nspemit(executor) ? PEMIT_SPOOF : 0);

  SPOOF(executor, enactor, sw);

  if (SW_ISSET(sw, SWITCH_ROOM))
    do_lemit(executor, arg_left,
             (SW_ISSET(sw, SWITCH_SILENT) ? PEMIT_SILENT : 0) | spflags,
             queue_entry->pe_info);
  else
    do_emit(executor, arg_left, spflags, queue_entry->pe_info);
}

COMMAND(cmd_enable)
{
  do_enable(executor, arg_left, 1);
}

COMMAND(cmd_entrances)
{
  int types = 0;

  if (SW_ISSET(sw, SWITCH_EXITS))
    types |= TYPE_EXIT;
  if (SW_ISSET(sw, SWITCH_THINGS))
    types |= TYPE_THING;
  if (SW_ISSET(sw, SWITCH_PLAYERS))
    types |= TYPE_PLAYER;
  if (SW_ISSET(sw, SWITCH_ROOMS))
    types |= TYPE_ROOM;
  if (!types)
    types = NOTYPE;
  do_entrances(executor, arg_left, args_right, types);
}

COMMAND(cmd_eunlock)
{
  do_unlock(executor, arg_left, Enter_Lock);
}

COMMAND(cmd_find)
{
  do_find(executor, arg_left, args_right);
}

COMMAND(cmd_firstexit)
{
  do_firstexit(executor, (const char **) args_left);
}

COMMAND(cmd_flag)
{
  if (SW_ISSET(sw, SWITCH_LIST))
    do_list_flags("FLAG", executor, arg_left, 0, T("Flags"));
  else if (SW_ISSET(sw, SWITCH_ADD))
    do_flag_add("FLAG", executor, arg_left, args_right);
  else if (SW_ISSET(sw, SWITCH_DELETE))
    do_flag_delete("FLAG", executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ALIAS))
    do_flag_alias("FLAG", executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_RESTRICT))
    do_flag_restrict("FLAG", executor, arg_left, args_right);
  else if (SW_ISSET(sw, SWITCH_DISABLE))
    do_flag_disable("FLAG", executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ENABLE))
    do_flag_enable("FLAG", executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_LETTER))
    do_flag_letter("FLAG", executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_TYPE))
    do_flag_type("FLAG", executor, arg_left, args_right[1]);
  else
    do_flag_info("FLAG", executor, arg_left);
}

COMMAND(cmd_force)
{
  int queue_type = QUEUE_DEFAULT;

  if (SW_ISSET(sw, SWITCH_INPLACE))
    queue_type = QUEUE_RECURSE;
  else if (SW_ISSET(sw, SWITCH_INLINE))
    queue_type = QUEUE_INPLACE;
  if (queue_type != QUEUE_DEFAULT) {
    if (SW_ISSET(sw, SWITCH_NOBREAK))
      queue_type |= QUEUE_NO_BREAKS;
    if (SW_ISSET(sw, SWITCH_CLEARREGS))
      queue_type |= QUEUE_CLEAR_QREG;
    if (SW_ISSET(sw, SWITCH_LOCALIZE))
      queue_type |= QUEUE_PRESERVE_QREG;
  }

  do_force(executor, caller, arg_left, arg_right, queue_type, queue_entry);
}

COMMAND(cmd_function)
{
  if (SW_ISSET(sw, SWITCH_DELETE))
    do_function_delete(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ENABLE))
    do_function_toggle(executor, arg_left, 1);
  else if (SW_ISSET(sw, SWITCH_DISABLE))
    do_function_toggle(executor, arg_left, 0);
  else if (SW_ISSET(sw, SWITCH_RESTRICT))
    do_function_restrict(executor, arg_left, args_right[1],
                         SW_ISSET(sw, SWITCH_BUILTIN));
  else if (SW_ISSET(sw, SWITCH_RESTORE))
    do_function_restore(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ALIAS))
    alias_function(executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_CLONE))
    do_function_clone(executor, arg_left, args_right[1]);
  else {
    int split;
    char *saved;
    split = 0;
    saved = NULL;
    if (args_right[1] && *args_right[1] && !(args_right[2] && *args_right[2])) {
      split = 1;
      saved = args_right[2];
      if ((args_right[2] = strchr(args_right[1], '/')) == NULL) {
        notify(executor, T("#-1 INVALID SECOND ARGUMENT"));
        return;
      }
      *args_right[2]++ = '\0';
    }
    if (args_right[1] && *args_right[1])
      do_function(executor, arg_left, args_right,
                  SW_ISSET(sw, SWITCH_PRESERVE));
    else if (arg_left && *arg_left)
      do_function_report(executor, arg_left);
    else
      do_function(executor, NULL, NULL, 0);
    if (split) {
      if (args_right[2])
        *--args_right[2] = '/';
      args_right[2] = saved;
    }
  }
}

COMMAND(cmd_grep)
{
  int flags = 0;
  int print = 0;

  if (SW_ISSET(sw, SWITCH_IPRINT) || SW_ISSET(sw, SWITCH_ILIST)
      || SW_ISSET(sw, SWITCH_NOCASE))
    flags |= GREP_NOCASE;

  if (SW_ISSET(sw, SWITCH_REGEXP))
    flags |= GREP_REGEXP;
  else if (SW_ISSET(sw, SWITCH_WILD))
    flags |= GREP_WILD;

  if (SW_ISSET(sw, SWITCH_IPRINT) || SW_ISSET(sw, SWITCH_PRINT))
    print = 1;

  do_grep(executor, arg_left, arg_right, print, flags);
}

COMMAND(cmd_halt)
{
  if (SW_ISSET(sw, SWITCH_ALL))
    do_allhalt(executor);
  else if (SW_BY_NAME(sw, "PID"))
    do_haltpid(executor, arg_left);
  else
    do_halt1(executor, arg_left, arg_right);
}

COMMAND(cmd_hide)
{
  int status = 2;
  if (SW_ISSET(sw, SWITCH_NO) || SW_ISSET(sw, SWITCH_OFF))
    status = 0;
  else if (SW_ISSET(sw, SWITCH_YES) || SW_ISSET(sw, SWITCH_ON))
    status = 1;
  hide_player(executor, status, arg_left);
}

COMMAND(cmd_hook)
{
  enum hook_type flags;
  int queue_type = QUEUE_DEFAULT;

  if (SW_ISSET(sw, SWITCH_INPLACE))
    queue_type = QUEUE_RECURSE | QUEUE_CLEAR_QREG;
  else if (SW_ISSET(sw, SWITCH_INLINE)) {
    queue_type = QUEUE_INPLACE;
    if (SW_ISSET(sw, SWITCH_NOBREAK))
      queue_type |= QUEUE_NO_BREAKS;
    if (SW_ISSET(sw, SWITCH_CLEARREGS))
      queue_type |= QUEUE_CLEAR_QREG;
    if (SW_ISSET(sw, SWITCH_LOCALIZE))
      queue_type |= QUEUE_PRESERVE_QREG;
  }


  if (SW_ISSET(sw, SWITCH_AFTER))
    flags = HOOK_AFTER;
  else if (SW_ISSET(sw, SWITCH_BEFORE))
    flags = HOOK_BEFORE;
  else if (SW_ISSET(sw, SWITCH_IGNORE))
    flags = HOOK_IGNORE;
  else if (SW_ISSET(sw, SWITCH_OVERRIDE))
    flags = HOOK_OVERRIDE;
  else if (SW_ISSET(sw, SWITCH_LIST)) {
    do_hook_list(executor, arg_left, 1);
    return;
  } else {
    notify(executor, T("You must give a switch for @hook."));
    return;
  }
  if (queue_type != QUEUE_DEFAULT) {
    if (flags != HOOK_OVERRIDE) {
      notify(executor,
             T("You can only use /inplace and /inline with /override."));
      return;
    }
  }
  do_hook(executor, arg_left, args_right[1], args_right[2], flags, queue_type);
}

COMMAND(cmd_huh_command)
{
  notify(executor, T("Huh?  (Type \"help\" for help.)"));
}

COMMAND(cmd_home)
{
  if (!Mobile(executor))
    return;
  do_move(executor, "home", MOVE_NORMAL, queue_entry->pe_info);
}

COMMAND(cmd_kick)
{
  do_kick(executor, arg_left);
}

COMMAND(cmd_lemit)
{
  int flags = SILENT_OR_NOISY(sw, SILENT_PEMIT);
  if (!strcmp(cmd->name, "@NSLEMIT") && Can_Nspemit(executor))
    flags |= PEMIT_SPOOF;

  SPOOF(executor, enactor, sw);
  do_lemit(executor, arg_left, flags, queue_entry->pe_info);
}

COMMAND(cmd_link)
{
  do_link(executor, arg_left, arg_right, SW_ISSET(sw, SWITCH_PRESERVE),
          queue_entry->pe_info);
}

COMMAND(cmd_listmotd)
{
  do_motd(executor, MOTD_LIST, "");
}

COMMAND(cmd_list)
{
  int lc;
  int which = 3;
  char *fwhich[3] = { "builtin", "local", "all" };
  lc = SW_ISSET(sw, SWITCH_LOWERCASE);
  if (SW_ISSET(sw, SWITCH_ALL))
    which = 3;
  else if (SW_ISSET(sw, SWITCH_LOCAL))
    which = 2;
  else if (SW_ISSET(sw, SWITCH_BUILTIN))
    which = 1;

  if (SW_ISSET(sw, SWITCH_MOTD))
    do_motd(executor, MOTD_LIST, "");
  else if (SW_ISSET(sw, SWITCH_FUNCTIONS))
    do_list_functions(executor, lc, fwhich[which - 1]);
  else if (SW_ISSET(sw, SWITCH_COMMANDS))
    do_list_commands(executor, lc, which);
  else if (SW_ISSET(sw, SWITCH_ATTRIBS))
    do_list_attribs(executor, lc);
  else if (SW_ISSET(sw, SWITCH_LOCKS))
    do_list_locks(executor, arg_left, lc, T("Locks"));
  else if (SW_ISSET(sw, SWITCH_FLAGS))
    do_list_flags("FLAG", executor, arg_left, lc, T("Flags"));
  else if (SW_ISSET(sw, SWITCH_POWERS))
    do_list_flags("POWER", executor, arg_left, lc, T("Powers"));
  else if (SW_ISSET(sw, SWITCH_ALLOCATIONS))
    do_list_allocations(executor);
  else
    do_list(executor, arg_left, lc, which);
}

COMMAND(cmd_lock)
{
  if ((switches) && (switches[0]))
    do_lock(executor, arg_left, arg_right, switches);
  else
    do_lock(executor, arg_left, arg_right, Basic_Lock);
}

static enum log_type
logtype_from_switch(switch_mask sw, enum log_type def)
{
  enum log_type type;

  if (SW_ISSET(sw, SWITCH_CHECK))
    type = LT_CHECK;
  else if (SW_ISSET(sw, SWITCH_CMD))
    type = LT_CMD;
  else if (SW_ISSET(sw, SWITCH_CONN))
    type = LT_CONN;
  else if (SW_ISSET(sw, SWITCH_ERR))
    type = LT_ERR;
  else if (SW_ISSET(sw, SWITCH_TRACE))
    type = LT_TRACE;
  else if (SW_ISSET(sw, SWITCH_WIZ))
    type = LT_WIZ;
  else
    type = def;

  return type;
}

COMMAND(cmd_log)
{
  enum log_type type = logtype_from_switch(sw, LT_CMD);

  if (SW_ISSET(sw, SWITCH_RECALL)) {
    int lines = parse_integer(arg_left);
    do_log_recall(executor, type, lines);
  } else
    do_writelog(executor, arg_left, type);
}

COMMAND(cmd_logwipe)
{
  enum log_type type = logtype_from_switch(sw, LT_ERR);
  do_logwipe(executor, type, arg_left);
}

COMMAND(cmd_lset)
{
  do_lset(executor, arg_left, arg_right);
}

COMMAND(cmd_mail)
{
  int urgent = SW_ISSET(sw, SWITCH_URGENT);
  int silent = SW_ISSET(sw, SWITCH_SILENT);
  int nosig = SW_ISSET(sw, SWITCH_NOSIG);
  /* First, mail commands that can be used even if you're gagged */
  if (SW_ISSET(sw, SWITCH_STATS))
    do_mail_stats(executor, arg_left, MSTATS_COUNT);
  else if (SW_ISSET(sw, SWITCH_DSTATS))
    do_mail_stats(executor, arg_left, MSTATS_READ);
  else if (SW_ISSET(sw, SWITCH_FSTATS))
    do_mail_stats(executor, arg_left, MSTATS_SIZE);
  else if (SW_ISSET(sw, SWITCH_DEBUG))
    do_mail_debug(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_NUKE))
    do_mail_nuke(executor);
  else if (SW_ISSET(sw, SWITCH_FOLDERS))
    do_mail_change_folder(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_UNFOLDER))
    do_mail_unfolder(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_LIST))
    do_mail_list(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_READ))
    do_mail_read(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_UNREAD))
    do_mail_unread(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_REVIEW))
    do_mail_review(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_RETRACT))
    do_mail_retract(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_STATUS))
    do_mail_status(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_CLEAR))
    do_mail_clear(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_UNCLEAR))
    do_mail_unclear(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_PURGE))
    do_mail_purge(executor);
  else if (SW_ISSET(sw, SWITCH_FILE))
    do_mail_file(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_TAG))
    do_mail_tag(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_UNTAG))
    do_mail_untag(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_FWD) || SW_ISSET(sw, SWITCH_FORWARD)
           || SW_ISSET(sw, SWITCH_SEND) || silent || urgent || nosig) {
    /* These commands are not allowed to gagged players */
    if (Gagged(executor)) {
      notify(executor, T("You cannot do that while gagged."));
      return;
    }
    if (SW_ISSET(sw, SWITCH_FWD))
      do_mail_fwd(executor, arg_left, arg_right);
    else if (SW_ISSET(sw, SWITCH_FORWARD))
      do_mail_fwd(executor, arg_left, arg_right);
    else if (SW_ISSET(sw, SWITCH_SEND) || silent || urgent || nosig)
      do_mail_send(executor, arg_left, arg_right,
                   urgent ? M_URGENT : 0, silent, nosig);
  } else
    do_mail(executor, arg_left, arg_right);     /* Does its own gagged check */
}


COMMAND(cmd_malias)
{
  if (SW_ISSET(sw, SWITCH_LIST))
    do_malias_list(executor);
  else if (SW_ISSET(sw, SWITCH_ALL))
    do_malias_all(executor);
  else if (SW_ISSET(sw, SWITCH_MEMBERS) || SW_ISSET(sw, SWITCH_WHO))
    do_malias_members(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_CREATE))
    do_malias_create(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_SET))
    do_malias_set(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_DESTROY))
    do_malias_destroy(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ADD))
    do_malias_add(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_REMOVE))
    do_malias_remove(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_DESCRIBE))
    do_malias_desc(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_RENAME))
    do_malias_rename(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_STATS))
    do_malias_stats(executor);
  else if (SW_ISSET(sw, SWITCH_CHOWN))
    do_malias_chown(executor, arg_left, arg_right);
  else if (SW_ISSET(sw, SWITCH_USEFLAG))
    do_malias_privs(executor, arg_left, arg_right, 0);
  else if (SW_ISSET(sw, SWITCH_SEEFLAG))
    do_malias_privs(executor, arg_left, arg_right, 1);
  else if (SW_ISSET(sw, SWITCH_NUKE))
    do_malias_nuke(executor);
  else
    do_malias(executor, arg_left, arg_right);
}

COMMAND(cmd_message)
{
  char *message;
  char *attrib;
  unsigned int flags = PEMIT_LIST;
  int numargs, i;
  char *args[10];
  enum emit_type type;

  for (numargs = 1; args_right[numargs] && numargs < 13; numargs++) ;

  switch (numargs) {
  case 1:
    notify(executor, T("@message them with what?"));
    return;
  case 2:
    notify(executor, T("Use what attribute for the @message?"));
    return;
  }
  if (!*arg_left) {
    notify(executor, T("@message who?"));
    return;
  }

  if (SW_ISSET(sw, SWITCH_REMIT))
    type = EMIT_REMIT;
  else if (SW_ISSET(sw, SWITCH_OEMIT))
    type = EMIT_OEMIT;
  else
    type = EMIT_PEMIT;

  if (SW_ISSET(sw, SWITCH_NOSPOOF) && Can_Nspemit(executor))
    flags |= PEMIT_SPOOF;

  message = args_right[1];
  attrib = args_right[2];

  for (i = 0; (i + 3) < numargs; i++) {
    args[i] = args_right[i + 3];
  }

  SPOOF(executor, enactor, sw);

  do_message(executor, arg_left, attrib, message, type, flags, i, args,
             queue_entry->pe_info);
}

COMMAND(cmd_motd)
{
  if (SW_ISSET(sw, SWITCH_CONNECT))
    do_motd(executor, MOTD_MOTD, arg_left);
  else if (SW_ISSET(sw, SWITCH_LIST))
    do_motd(executor, MOTD_LIST, "");
  else if (SW_ISSET(sw, SWITCH_WIZARD))
    do_motd(executor, MOTD_WIZ, arg_left);
  else if (SW_ISSET(sw, SWITCH_DOWN))
    do_motd(executor, MOTD_DOWN, arg_left);
  else if (SW_ISSET(sw, SWITCH_FULL))
    do_motd(executor, MOTD_FULL, arg_left);
  else
    do_motd(executor, MOTD_MOTD, arg_left);
}

COMMAND(cmd_mvattr)
{
  do_cpattr(executor, arg_left, args_right, 1, SW_ISSET(sw, SWITCH_NOFLAGCOPY));
}

COMMAND(cmd_name)
{
  do_name(executor, arg_left, arg_right);
}

COMMAND(cmd_newpassword)
{
  do_newpassword(executor, enactor, arg_left, arg_right, queue_entry);
}

COMMAND(cmd_nuke)
{
  do_destroy(executor, arg_left, 1, queue_entry->pe_info);
}

COMMAND(cmd_oemit)
{
  int spflags = (!strcmp(cmd->name, "@NSOEMIT")
                 && Can_Nspemit(executor) ? PEMIT_SPOOF : 0);
  SPOOF(executor, enactor, sw);
  do_oemit_list(executor, arg_left, arg_right, spflags, NULL,
                queue_entry->pe_info);
}

COMMAND(cmd_open)
{
  do_open(executor, arg_left, args_right, queue_entry->pe_info);
}

COMMAND(cmd_parent)
{
  do_parent(executor, arg_left, arg_right, queue_entry->pe_info);
}

COMMAND(cmd_password)
{
  do_password(executor, enactor, arg_left, arg_right, queue_entry);
}

COMMAND(cmd_pcreate)
{
  char *newdbref;

  if (args_right[2] && *args_right[2])
    newdbref = args_right[2];
  else
    newdbref = NULL;

  do_pcreate(executor, arg_left, args_right[1], newdbref);
}

COMMAND(cmd_pemit)
{
  int flags = SILENT_OR_NOISY(sw, SILENT_PEMIT);

  if (SW_ISSET(sw, SWITCH_PORT)) {
    if (SW_ISSET(sw, SWITCH_LIST))
      flags |= PEMIT_LIST;
    do_pemit_port(executor, arg_left, arg_right, flags);
    return;
  }

  if (!strcmp(cmd->name, "@NSPEMIT") && Can_Nspemit(executor))
    flags |= PEMIT_SPOOF;

  SPOOF(executor, enactor, sw);

  if (SW_ISSET(sw, SWITCH_CONTENTS)) {
    do_remit(executor, arg_left, arg_right, flags, NULL, queue_entry->pe_info);
    return;
  }
  if (SW_ISSET(sw, SWITCH_LIST)) {
    flags |= PEMIT_LIST;
    if (!SW_ISSET(sw, SWITCH_NOISY))
      flags |= PEMIT_SILENT;
  }
  do_pemit(executor, arg_left, arg_right, flags, NULL, queue_entry->pe_info);
}

COMMAND(cmd_prompt)
{
  int flags = SILENT_OR_NOISY(sw, SILENT_PEMIT) | PEMIT_PROMPT | PEMIT_LIST;

  if (!strcmp(cmd->name, "@NSPEMIT") && Can_Nspemit(executor))
    flags |= PEMIT_SPOOF;

  SPOOF(executor, enactor, sw);
  do_pemit(executor, arg_left, arg_right, flags, NULL, queue_entry->pe_info);
}

COMMAND(cmd_poll)
{
  do_poll(executor, arg_left, SW_ISSET(sw, SWITCH_CLEAR));
}

COMMAND(cmd_poor)
{
  do_poor(executor, arg_left);
}

COMMAND(cmd_power)
{
  if (SW_ISSET(sw, SWITCH_LIST))
    do_list_flags("POWER", executor, arg_left, 0, T("Powers"));
  else if (SW_ISSET(sw, SWITCH_ADD))
    do_flag_add("POWER", executor, arg_left, args_right);
  else if (SW_ISSET(sw, SWITCH_DELETE))
    do_flag_delete("POWER", executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ALIAS))
    do_flag_alias("POWER", executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_RESTRICT))
    do_flag_restrict("POWER", executor, arg_left, args_right);
  else if (SW_ISSET(sw, SWITCH_DISABLE))
    do_flag_disable("POWER", executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_ENABLE))
    do_flag_enable("POWER", executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_LETTER))
    do_flag_letter("POWER", executor, arg_left, args_right[1]);
  else if (SW_ISSET(sw, SWITCH_TYPE))
    do_flag_type("POWER", executor, arg_left, args_right[1]);
  else
    do_power(executor, arg_left, args_right[1]);
}

COMMAND(cmd_ps)
{
  if (SW_ISSET(sw, SWITCH_ALL))
    do_queue(executor, arg_left, QUEUE_ALL);
  else if (SW_ISSET(sw, SWITCH_SUMMARY) || SW_ISSET(sw, SWITCH_COUNT))
    do_queue(executor, arg_left, QUEUE_SUMMARY);
  else if (SW_ISSET(sw, SWITCH_QUICK))
    do_queue(executor, arg_left, QUEUE_QUICK);
  else if (arg_left && *arg_left && is_strict_uinteger(arg_left))
    do_queue_single(executor, arg_left, SW_ISSET(sw, SWITCH_DEBUG));
  else
    do_queue(executor, arg_left, QUEUE_NORMAL);
}

COMMAND(cmd_purge)
{
  do_purge(executor);
}

COMMAND(cmd_quota)
{
  if (SW_ISSET(sw, SWITCH_ALL))
    do_allquota(executor, arg_left, (SW_ISSET(sw, SWITCH_QUIET)));
  else if (SW_ISSET(sw, SWITCH_SET))
    do_quota(executor, arg_left, arg_right, 1);
  else
    do_quota(executor, arg_left, "", 0);
}

COMMAND(cmd_readcache)
{
  do_readcache(executor);
}

COMMAND(cmd_remit)
{
  int flags = SILENT_OR_NOISY(sw, SILENT_PEMIT);

  if (SW_ISSET(sw, SWITCH_LIST))
    flags |= PEMIT_LIST;
  if (!strcmp(cmd->name, "@NSREMIT") && Can_Nspemit(executor))
    flags |= PEMIT_SPOOF;

  SPOOF(executor, enactor, sw);
  do_remit(executor, arg_left, arg_right, flags, NULL, queue_entry->pe_info);
}

COMMAND(cmd_rejectmotd)
{
  do_motd(executor, MOTD_DOWN, arg_left);
}

COMMAND(cmd_restart)
{
  if (SW_ISSET(sw, SWITCH_ALL))
    do_allrestart(executor);
  else
    do_restart_com(executor, arg_left);
}

COMMAND(cmd_rwall)
{
  do_wall(executor, arg_left, WALL_RW, SW_ISSET(sw, SWITCH_EMIT));
}


COMMAND(cmd_scan)
{
  int check = 0;

  if (SW_ISSET(sw, SWITCH_ROOM))
    check |= CHECK_NEIGHBORS | CHECK_HERE;
  if (SW_ISSET(sw, SWITCH_SELF))
    check |= CHECK_INVENTORY | CHECK_SELF;
  if (SW_ISSET(sw, SWITCH_ZONE))
    check |= CHECK_ZONE;
  if (SW_ISSET(sw, SWITCH_GLOBALS))
    check |= CHECK_GLOBAL;
  if (check == 0)
    check = CHECK_INVENTORY | CHECK_NEIGHBORS |
      CHECK_SELF | CHECK_HERE | CHECK_ZONE | CHECK_GLOBAL;
  do_scan(executor, arg_left, check);
}

COMMAND(cmd_search)
{
  do_search(executor, arg_left, args_right);
}

COMMAND(cmd_select)
{

  int queue_type = QUEUE_DEFAULT;

  if (SW_ISSET(sw, SWITCH_INPLACE))
    queue_type = QUEUE_RECURSE;
  else if (SW_ISSET(sw, SWITCH_INLINE))
    queue_type = QUEUE_INPLACE;
  if (queue_type != QUEUE_DEFAULT) {
    if (SW_ISSET(sw, SWITCH_NOBREAK))
      queue_type |= QUEUE_NO_BREAKS;
    if (SW_ISSET(sw, SWITCH_CLEARREGS))
      queue_type |= QUEUE_CLEAR_QREG;
    if (SW_ISSET(sw, SWITCH_LOCALIZE))
      queue_type |= QUEUE_PRESERVE_QREG;
  }

  do_switch(executor, arg_left, args_right, enactor, 1,
            SW_ISSET(sw, SWITCH_NOTIFY), SW_ISSET(sw, SWITCH_REGEXP),
            queue_type, queue_entry);

}

COMMAND(cmd_set)
{
  do_set(executor, arg_left, arg_right);
}

COMMAND(cmd_shutdown)
{
  enum shutdown_type paranoid;
  paranoid = SW_ISSET(sw, SWITCH_PARANOID) ? SHUT_PARANOID : SHUT_NORMAL;
  if (SW_ISSET(sw, SWITCH_REBOOT))
    do_reboot(executor, paranoid == SHUT_PARANOID);
  else if (SW_ISSET(sw, SWITCH_PANIC))
    do_shutdown(executor, SHUT_PANIC);
  else
    do_shutdown(executor, paranoid);
}

COMMAND(cmd_sitelock)
{
  int psw = SW_ISSET(sw, SWITCH_PLAYER);
  if (SW_ISSET(sw, SWITCH_BAN))
    do_sitelock(executor, arg_left, NULL, NULL, SITELOCK_BAN, psw);
  else if (SW_ISSET(sw, SWITCH_REGISTER))
    do_sitelock(executor, arg_left, NULL, NULL, SITELOCK_REGISTER, psw);
  else if (SW_ISSET(sw, SWITCH_NAME))
    do_sitelock_name(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_REMOVE))
    do_sitelock(executor, arg_left, NULL, NULL, SITELOCK_REMOVE, psw);
  else if (SW_ISSET(sw, SWITCH_CHECK))
    do_sitelock(executor, arg_left, NULL, NULL, SITELOCK_CHECK, psw);
  else if (!arg_left || !*arg_left)
    do_sitelock(executor, NULL, NULL, NULL, SITELOCK_LIST, psw);
  else
    do_sitelock(executor, arg_left, args_right[1], args_right[2], SITELOCK_ADD,
                psw);
}

COMMAND(cmd_stats)
{
  if (SW_ISSET(sw, SWITCH_TABLES))
    do_list_memstats(executor);
  else if (SW_ISSET(sw, SWITCH_CHUNKS)) {
    if (SW_ISSET(sw, SWITCH_REGIONS))
      chunk_stats(executor, CSTATS_REGION);
    else
      chunk_stats(executor, CSTATS_SUMMARY);
  } else if (SW_ISSET(sw, SWITCH_REGIONS))
    chunk_stats(executor, CSTATS_REGIONG);
  else if (SW_ISSET(sw, SWITCH_PAGING))
    chunk_stats(executor, CSTATS_PAGINGG);
  else if (SW_ISSET(sw, SWITCH_FREESPACE))
    chunk_stats(executor, CSTATS_FREESPACEG);
  else if (SW_ISSET(sw, SWITCH_FLAGS))
    flag_stats(executor);
  else
    do_stats(executor, arg_left);
}

COMMAND(cmd_sweep)
{
  if (SW_ISSET(sw, SWITCH_CONNECTED))
    do_sweep(executor, "connected");
  else if (SW_ISSET(sw, SWITCH_HERE))
    do_sweep(executor, "here");
  else if (SW_ISSET(sw, SWITCH_INVENTORY))
    do_sweep(executor, "inventory");
  else if (SW_ISSET(sw, SWITCH_EXITS))
    do_sweep(executor, "exits");
  else
    do_sweep(executor, arg_left);
}

COMMAND(cmd_switch)
{
  int queue_type = QUEUE_DEFAULT;

  if (SW_ISSET(sw, SWITCH_INPLACE))
    queue_type = QUEUE_RECURSE;
  else if (SW_ISSET(sw, SWITCH_INLINE))
    queue_type = QUEUE_INPLACE;
  if (queue_type != QUEUE_DEFAULT) {
    if (SW_ISSET(sw, SWITCH_NOBREAK))
      queue_type |= QUEUE_NO_BREAKS;
    if (SW_ISSET(sw, SWITCH_CLEARREGS))
      queue_type |= QUEUE_CLEAR_QREG;
    if (SW_ISSET(sw, SWITCH_LOCALIZE))
      queue_type |= QUEUE_PRESERVE_QREG;
  }

  do_switch(executor, arg_left, args_right, enactor, SW_ISSET(sw, SWITCH_FIRST),
            SW_ISSET(sw, SWITCH_NOTIFY), SW_ISSET(sw, SWITCH_REGEXP),
            queue_type, queue_entry);
}

COMMAND(cmd_squota)
{
  do_quota(executor, arg_left, arg_right, 1);
}


COMMAND(cmd_teleport)
{
  if (rhs_present && !*arg_right)
    notify(executor, T("You can't teleport to nothing!"));
  else
    do_teleport(executor, arg_left, arg_right, (SW_ISSET(sw, SWITCH_SILENT)),
                (SW_ISSET(sw, SWITCH_INSIDE)), queue_entry->pe_info);
}

COMMAND(cmd_include)
{

  int queue_type = QUEUE_INPLACE;

  if (SW_ISSET(sw, SWITCH_NOBREAK))
    queue_type |= QUEUE_NO_BREAKS;
  if (SW_ISSET(sw, SWITCH_CLEARREGS))
    queue_type |= QUEUE_CLEAR_QREG;
  if (SW_ISSET(sw, SWITCH_LOCALIZE))
    queue_type |= QUEUE_PRESERVE_QREG;

  do_include(executor, enactor, arg_left, args_right, queue_type, queue_entry);
}

COMMAND(cmd_trigger)
{
  do_trigger(executor, arg_left, args_right, queue_entry);
}

COMMAND(cmd_ulock)
{
  do_lock(executor, arg_left, arg_right, Use_Lock);
}

COMMAND(cmd_undestroy)
{
  do_undestroy(executor, arg_left);
}

COMMAND(cmd_unlink)
{
  do_unlink(executor, arg_left);
}

COMMAND(cmd_unlock)
{
  if ((switches) && (switches[0]))
    do_unlock(executor, arg_left, switches);
  else
    do_unlock(executor, arg_left, Basic_Lock);
}

COMMAND(cmd_uptime)
{
  do_uptime(executor, SW_ISSET(sw, SWITCH_MORTAL));
}

COMMAND(cmd_uunlock)
{
  do_unlock(executor, arg_left, Use_Lock);
}

COMMAND(cmd_verb)
{
  do_verb(executor, enactor, arg_left, args_right, queue_entry);
}

COMMAND(cmd_version)
{
  do_version(executor);
}

COMMAND(cmd_wait)
{
  if (SW_BY_NAME(sw, "PID"))
    do_waitpid(executor, arg_left, arg_right, SW_ISSET(sw, SWITCH_UNTIL));
  else
    do_wait(executor, enactor, arg_left, arg_right, SW_ISSET(sw, SWITCH_UNTIL),
            queue_entry);
}

COMMAND(cmd_wall)
{
  do_wall(executor, arg_left, WALL_ALL, SW_ISSET(sw, SWITCH_EMIT));
}

COMMAND(cmd_warnings)
{
  do_warnings(executor, arg_left, arg_right);
}

COMMAND(cmd_wcheck)
{
  if (SW_ISSET(sw, SWITCH_ALL))
    do_wcheck_all(executor);
  else if (SW_ISSET(sw, SWITCH_ME))
    do_wcheck_me(executor);
  else
    do_wcheck(executor, arg_left);
}

COMMAND(cmd_whereis)
{
  do_whereis(executor, arg_left);
}

COMMAND(cmd_wipe)
{
  do_wipe(executor, arg_left);
}

COMMAND(cmd_wizwall)
{
  do_wall(executor, arg_left, WALL_WIZ, SW_ISSET(sw, SWITCH_EMIT));
}

COMMAND(cmd_wizmotd)
{
  do_motd(executor, MOTD_WIZ, arg_left);
}

COMMAND(cmd_zemit)
{
  int flags = SILENT_OR_NOISY(sw, SILENT_PEMIT);
  if (!strcmp(cmd->name, "@NSZEMIT") && Can_Nspemit(executor))
    flags |= PEMIT_SPOOF;
  do_zemit(executor, arg_left, arg_right, flags);
}

COMMAND(cmd_brief)
{
  do_examine(executor, arg_left, EXAM_BRIEF, 0, 0, SW_ISSET(sw, SWITCH_OPAQUE));
}

COMMAND(cmd_drop)
{
  do_drop(executor, arg_left, queue_entry->pe_info);
}

COMMAND(cmd_examine)
{
  int all = SW_ISSET(sw, SWITCH_ALL);
  int parent = SW_ISSET(sw, SWITCH_PARENT);
  int opaque = SW_ISSET(sw, SWITCH_OPAQUE);
  if (SW_ISSET(sw, SWITCH_BRIEF))
    do_examine(executor, arg_left, EXAM_BRIEF, all, 0, opaque);
  else if (SW_ISSET(sw, SWITCH_DEBUG))
    do_debug_examine(executor, arg_left);
  else if (SW_ISSET(sw, SWITCH_MORTAL))
    do_examine(executor, arg_left, EXAM_MORTAL, all, parent, opaque);
  else
    do_examine(executor, arg_left, EXAM_NORMAL, all, parent, opaque);
}

COMMAND(cmd_empty)
{
  do_empty(executor, arg_left, queue_entry->pe_info);
}

COMMAND(cmd_enter)
{
  do_enter(executor, arg_left, queue_entry->pe_info);
}

COMMAND(cmd_dismiss)
{
  do_dismiss(executor, arg_left);
}

COMMAND(cmd_desert)
{
  do_desert(executor, arg_left);
}

COMMAND(cmd_follow)
{
  do_follow(executor, arg_left, queue_entry->pe_info);
}

COMMAND(cmd_unfollow)
{
  do_unfollow(executor, arg_left);
}

COMMAND(cmd_get)
{
  do_get(executor, arg_left, queue_entry->pe_info);
}

COMMAND(cmd_buy)
{
  char *from = NULL;
  char *forwhat = NULL;
  int price = -1;

  upcasestr(arg_left);

  from = strstr(arg_left, " FROM ");
  forwhat = strstr(arg_left, " FOR ");
  if (from) {
    *from = '\0';
    from += 6;
  }
  if (forwhat) {
    *forwhat = '\0';
    forwhat += 5;
  }
  if (forwhat && !is_strict_integer(forwhat)) {
    notify(executor, T("Buy for WHAT price?"));
    return;
  } else if (forwhat) {
    price = parse_integer(forwhat);
    if (price < 0) {
      notify(executor, T("You can't buy things by taking money."));
      return;
    }
  }

  if (from)
    from = trim_space_sep(from, ' ');
  do_buy(executor, arg_left, from, price, queue_entry->pe_info);
}

COMMAND(cmd_give)
{
  do_give(executor, arg_left, arg_right, (SW_ISSET(sw, SWITCH_SILENT)),
          queue_entry->pe_info);
}

COMMAND(cmd_goto)
{
  move_wrapper(executor, arg_left, queue_entry->pe_info);
}

COMMAND(cmd_inventory)
{
  do_inventory(executor);
}

COMMAND(cmd_kill)
{
  do_kill(executor, arg_left, atol(arg_right), 0);
}

COMMAND(cmd_look)
{
  do_look_at(executor, arg_left, (SW_ISSET(sw, SWITCH_OUTSIDE)),
             queue_entry->pe_info);
}

COMMAND(cmd_leave)
{
  do_leave(executor, queue_entry->pe_info);
}

COMMAND(cmd_page)
{
  if (SW_ISSET(sw, SWITCH_PORT))
    do_page_port(executor, arg_left, arg_right);
  else
    do_page(executor, arg_left, arg_right, SW_ISSET(sw, SWITCH_OVERRIDE),
            rhs_present, queue_entry->pe_info);
}

COMMAND(cmd_pose)
{
  do_pose(executor, arg_left, (SW_ISSET(sw, SWITCH_NOSPACE)),
          queue_entry->pe_info);
}

COMMAND(cmd_say)
{
  do_say(executor, arg_left, queue_entry->pe_info);
}

COMMAND(cmd_score)
{
  do_score(executor);
}

COMMAND(cmd_semipose)
{
  do_pose(executor, arg_left, 1, queue_entry->pe_info);
}

COMMAND(cmd_slay)
{
  do_kill(executor, arg_left, 0, 1);
}

COMMAND(cmd_think)
{
  notify(executor, arg_left);
}

COMMAND(cmd_whisper)
{
  do_whisper(executor, arg_left, arg_right,
             (SW_ISSET(sw, SWITCH_NOISY) || (!SW_ISSET(sw, SWITCH_SILENT)
                                             && NOISY_WHISPER)),
             queue_entry->pe_info);
}

COMMAND(cmd_use)
{
  do_use(executor, arg_left, queue_entry->pe_info);
}

COMMAND(command_atrset)
{
  dbref thing;

  if ((thing = match_controlled(executor, arg_left)) == NOTHING)
    return;

  /* If it's &attr obj, we must pass a NULL. If &attr obj=, pass "" */
  if (rhs_present) {
    do_set_atr(thing, switches, arg_right, executor,
               0x1 | (SW_ISSET(sw, SWITCH_NOEVAL) ? 0 : 0x02));
  } else {
    do_set_atr(thing, switches, NULL, executor, 0x1);
  }
}

COMMAND(cmd_null)
{
  return;
}

COMMAND(cmd_warn_on_missing)
{
  notify_format(Owner(executor),
                T
                ("No command found in code by %s - don't start code with functions."),
                unparse_dbref(executor));
  return;
}

COMMAND(cmd_who_doing)
{
  do_who_mortal(executor, arg_left);
}

COMMAND(cmd_session)
{
  if (Priv_Who(executor))
    do_who_session(executor, arg_left);
  else
    do_who_mortal(executor, arg_left);
}

COMMAND(cmd_who)
{
  if (Priv_Who(executor))
    do_who_admin(executor, arg_left);
  else
    do_who_mortal(executor, arg_left);
}
