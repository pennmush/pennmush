/**
 * \file command.c
 *
 * \brief Parsing of commands.
 *
 * Sets up a hash table for the commands, and parses input for commands.
 * This implementation is almost totally by Thorvald Natvig, with
 * various mods by Javelin and others over time.
 *
 */

#include "copyrite.h"
#include "config.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "dbdefs.h"
#include "mushdb.h"
#include "game.h"
#include "match.h"
#include "attrib.h"
#include "extmail.h"
#include "parse.h"
#include "access.h"
#include "version.h"
#include "ptab.h"
#include "htab.h"
#include "strtree.h"
#include "function.h"
#include "command.h"
#include "mymalloc.h"
#include "flags.h"
#include "log.h"
#include "sort.h"
#include "cmds.h"
#include "confmagic.h"

PTAB ptab_command;      /**< Prefix table for command names. */
PTAB ptab_command_perms;        /**< Prefix table for command permissions */

HASHTAB htab_reserved_aliases;  /**< Hash table for reserved command aliases */

slab *command_slab = NULL; /**< slab for command_info structs */

static const char *command_isattr(char *command);
static int switch_find(COMMAND_INFO *cmd, const char *sw);
static void strccat(char *buff, char **bp, const char *from);
static COMMAND_INFO *clone_command(char *original, char *clone);
static int has_hook(struct hook_data *hook);
extern int global_fun_invocations;       /**< Counter for function invocations */
extern int global_fun_recursions;       /**< Counter for function recursion */

SWITCH_VALUE *dyn_switch_list = NULL;
int switch_bytes = 0;
size_t num_switches = 0;

enum command_load_state { CMD_LOAD_BUILTIN,
  CMD_LOAD_LOCAL,
  CMD_LOAD_DONE
};
static enum command_load_state command_state = CMD_LOAD_BUILTIN;
static StrTree switch_names;

int run_hook(dbref player, dbref cause, struct hook_data *hook,
             char *saveregs[], int save);

int run_hook_override(COMMAND_INFO *cmd, dbref player, const char *commandraw);

const char *CommandLock = "CommandLock";

/** The list of standard commands. Additional commands can be added
 * at runtime with command_add().
 */
COMLIST commands[] = {

  {"@COMMAND",
   "ADD ALIAS CLONE DELETE EQSPLIT LSARGS RSARGS NOEVAL ON OFF QUIET ENABLE DISABLE RESTRICT NOPARSE",
   cmd_command,
   CMD_T_PLAYER | CMD_T_EQSPLIT, 0, 0},
  {"@@", NULL, cmd_null, CMD_T_ANY | CMD_T_NOPARSE, 0, 0},
  {"@ALLHALT", NULL, cmd_allhalt, CMD_T_ANY, "WIZARD", "HALT"},
  {"@ALLQUOTA", "QUIET", cmd_allquota, CMD_T_ANY, "WIZARD", "QUOTA"},
  {"@ASSERT", NULL, cmd_assert, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE, 0,
   0},
  {"@ATRLOCK", NULL, cmd_atrlock, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@ATRCHOWN", NULL, cmd_atrchown, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},

  {"@ATTRIBUTE", "ACCESS DELETE RENAME RETROACTIVE", cmd_attribute,
   CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@BOOT", "PORT ME SILENT", cmd_boot, CMD_T_ANY, 0, 0},
  {"@BREAK", NULL, cmd_break, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE, 0,
   0},
  {"@CEMIT", "NOEVAL NOISY SILENT SPOOF", cmd_cemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@CHANNEL",
   "LIST ADD DELETE RENAME MOGRIFIER NAME PRIVS QUIET NOISY DECOMPILE DESCRIBE CHOWN WIPE MUTE UNMUTE GAG UNGAG HIDE UNHIDE WHAT TITLE BRIEF RECALL BUFFER SET",
   cmd_channel,
   CMD_T_ANY | CMD_T_SWITCHES | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_RS_ARGS,
   0, 0},
  {"@CHAT", NULL, cmd_chat, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@CHOWNALL", "PRESERVE", cmd_chownall, CMD_T_ANY | CMD_T_EQSPLIT, "WIZARD",
   0},

  {"@CHOWN", "PRESERVE", cmd_chown,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@CHZONEALL", "PRESERVE", cmd_chzoneall, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},

  {"@CHZONE", "PRESERVE", cmd_chzone,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@CONFIG", "SET SAVE LOWERCASE LIST", cmd_config, CMD_T_ANY | CMD_T_EQSPLIT,
   0, 0},
  {"@CPATTR", "CONVERT NOFLAGCOPY", cmd_cpattr,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS,
   0, 0},
  {"@CREATE", NULL, cmd_create,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED,
   0, 0},
  {"@CLONE", "PRESERVE", cmd_clone,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED,
   0, 0},

  {"@CLOCK", "JOIN SPEAK MOD SEE HIDE", cmd_clock,
   CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@DBCK", NULL, cmd_dbck, CMD_T_ANY, "WIZARD", 0},

  {"@DECOMPILE", "DB NAME PREFIX TF FLAGS ATTRIBS SKIPDEFAULTS", cmd_decompile,
   CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@DESTROY", "OVERRIDE", cmd_destroy, CMD_T_ANY, 0, 0},

  {"@DIG", "TELEPORT", cmd_dig,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@DISABLE", NULL, cmd_disable, CMD_T_ANY, "WIZARD", 0},

  {"@DOING", "HEADER", cmd_doing,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_NOGAGGED, 0, 0},
  {"@DOLIST", "NOTIFY DELIMIT", cmd_dolist,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE, 0, 0},
  {"@DRAIN", "ALL ANY", cmd_notify_drain, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@DUMP", "PARANOID DEBUG", cmd_dump, CMD_T_ANY, "WIZARD", 0},

  {"@EDIT", "FIRST CHECK", cmd_edit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE |
   CMD_T_NOGAGGED, 0, 0},
  {"@ELOCK", NULL, cmd_elock, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   0, 0},
  {"@EMIT", "ROOM NOEVAL SILENT SPOOF", cmd_emit, CMD_T_ANY | CMD_T_NOGAGGED, 0,
   0},
  {"@ENABLE", NULL, cmd_enable, CMD_T_ANY | CMD_T_NOGAGGED, "WIZARD", 0},

  {"@ENTRANCES", "EXITS THINGS PLAYERS ROOMS", cmd_entrances,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@EUNLOCK", NULL, cmd_eunlock, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"@FIND", NULL, cmd_find,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@FIRSTEXIT", NULL, cmd_firstexit, CMD_T_ANY | CMD_T_ARGS, 0, 0},
  {"@FLAG", "ADD TYPE LETTER LIST RESTRICT DELETE ALIAS DISABLE ENABLE",
   cmd_flag,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},

  {"@FORCE", "NOEVAL", cmd_force, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   0, 0},
  {"@FUNCTION",
   "ALIAS BUILTIN CLONE DELETE ENABLE DISABLE PRESERVE RESTORE RESTRICT",
   cmd_function,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@GREP", "LIST PRINT ILIST IPRINT", cmd_grep,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE | CMD_T_NOGAGGED, 0, 0},
  {"@HALT", "ALL PID", cmd_halt, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@HIDE", "NO OFF YES ON", cmd_hide, CMD_T_ANY, 0, 0},
  {"@HOOK", "LIST AFTER BEFORE IGNORE OVERRIDE", cmd_hook,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS,
   "WIZARD", "hook"},
  {"@INCLUDE", NULL, cmd_include,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@KICK", NULL, cmd_kick, CMD_T_ANY, "WIZARD", 0},

  {"@LEMIT", "NOEVAL SILENT SPOOF", cmd_lemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@LINK", "PRESERVE", cmd_link, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0,
   0},
  {"@LISTMOTD", NULL, cmd_listmotd, CMD_T_ANY, 0, 0},

  {"@LIST",
   "LOWERCASE MOTD LOCKS FLAGS FUNCTIONS POWERS COMMANDS ATTRIBS ALLOCATIONS",
   cmd_list,
   CMD_T_ANY, 0, 0},
  {"@LOCK", NULL, cmd_lock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_SWITCHES | CMD_T_NOGAGGED, 0, 0},
  {"@LOG", "CHECK CMD CONN ERR TRACE WIZ", cmd_log,
   CMD_T_ANY | CMD_T_NOGAGGED, "WIZARD", 0},
  {"@LOGWIPE", "CHECK CMD CONN ERR TRACE WIZ", cmd_logwipe,
   CMD_T_ANY | CMD_T_NOGAGGED | CMD_T_GOD, 0, 0},
  {"@LSET", NULL, cmd_lset,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@MAIL",
   "NOEVAL NOSIG STATS DSTATS FSTATS DEBUG NUKE FOLDERS UNFOLDER LIST READ UNREAD CLEAR UNCLEAR STATUS PURGE FILE TAG UNTAG FWD FORWARD SEND SILENT URGENT",
   cmd_mail, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},

  {"@MALIAS",
   "SET CREATE DESTROY DESCRIBE RENAME STATS CHOWN NUKE ADD REMOVE LIST ALL WHO MEMBERS USEFLAG SEEFLAG",
   cmd_malias, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},

  {"@MESSAGE", "NOEVAL SPOOF", cmd_message,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@MOTD", "CONNECT LIST WIZARD DOWN FULL", cmd_motd,
   CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@MVATTR", "CONVERT NOFLAGCOPY", cmd_mvattr,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS,
   0, 0},
  {"@NAME", NULL, cmd_name, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED
   | CMD_T_NOGUEST, 0, 0},
  {"@NEWPASSWORD", NULL, cmd_newpassword, CMD_T_ANY | CMD_T_EQSPLIT
   | CMD_T_RS_NOPARSE, "WIZARD", 0},
  {"@NOTIFY", "ALL ANY", cmd_notify_drain, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@NSCEMIT", "NOEVAL NOISY SILENT SPOOF", cmd_cemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NSEMIT", "ROOM NOEVAL SILENT SPOOF", cmd_emit, CMD_T_ANY | CMD_T_NOGAGGED,
   0, 0},
  {"@NSLEMIT", "NOEVAL SILENT SPOOF", cmd_lemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NSOEMIT", "NOEVAL SPOOF", cmd_oemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NSPEMIT", "LIST SILENT NOISY NOEVAL", cmd_pemit,
   CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@NSPROMPT", "SILENT NOISY NOEVAL", cmd_prompt,
   CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@NSREMIT", "LIST NOEVAL NOISY SILENT SPOOF", cmd_remit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NSZEMIT", NULL, cmd_zemit, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   0, 0},
  {"@NUKE", NULL, cmd_nuke, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"@OEMIT", "NOEVAL SPOOF", cmd_oemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@OPEN", NULL, cmd_open,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@PARENT", NULL, cmd_parent, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@PASSWORD", NULL, cmd_password, CMD_T_PLAYER | CMD_T_EQSPLIT
   | CMD_T_NOPARSE | CMD_T_RS_NOPARSE | CMD_T_NOGUEST, 0, 0},
  {"@PCREATE", NULL, cmd_pcreate, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0,
   0},

  {"@PEMIT", "LIST CONTENTS SILENT NOISY NOEVAL PORT SPOOF", cmd_pemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@POLL", "CLEAR", cmd_poll, CMD_T_ANY, 0, 0},
  {"@POOR", NULL, cmd_poor, CMD_T_ANY, 0, 0},
  {"@POWER", "ADD TYPE LETTER LIST RESTRICT DELETE ALIAS DISABLE ENABLE",
   cmd_power, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@PROMPT", "SILENT NOISY NOEVAL SPOOF", cmd_prompt,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@PS", "ALL SUMMARY COUNT QUICK", cmd_ps, CMD_T_ANY, 0, 0},
  {"@PURGE", NULL, cmd_purge, CMD_T_ANY, 0, 0},
  {"@QUOTA", "ALL SET", cmd_quota, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@READCACHE", NULL, cmd_readcache, CMD_T_ANY, "WIZARD", 0},
  {"@RECYCLE", "OVERRIDE", cmd_destroy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"@REMIT", "LIST NOEVAL NOISY SILENT SPOOF", cmd_remit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@REJECTMOTD", NULL, cmd_rejectmotd, CMD_T_ANY, "WIZARD", 0},
  {"@RESTART", "ALL", cmd_restart, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@RWALL", "NOEVAL EMIT", cmd_rwall, CMD_T_ANY, "WIZARD ROYALTY", 0},
  {"@SCAN", "ROOM SELF ZONE GLOBALS", cmd_scan,
   CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@SEARCH", NULL, cmd_search,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, 0, 0},
  {"@SELECT", "NOTIFY REGEXP", cmd_select,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, 0, 0},
  {"@SET", NULL, cmd_set, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@SHUTDOWN", "PANIC REBOOT PARANOID", cmd_shutdown, CMD_T_ANY, "WIZARD", 0},
#if defined(HAVE_MYSQL) || defined(HAVE_POSTGRESQL) || defined(HAVE_SQLITE3)
  {"@SQL", NULL, cmd_sql, CMD_T_ANY, "WIZARD", "SQL_OK"},
#else
  {"@SQL", NULL, cmd_unimplemented, CMD_T_ANY, "WIZARD", "SQL_OK"},
#endif
  {"@SITELOCK", "BAN CHECK REGISTER REMOVE NAME", cmd_sitelock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, "WIZARD", 0},
  {"@STATS", "CHUNKS FREESPACE PAGING REGIONS TABLES", cmd_stats,
   CMD_T_ANY, 0, 0},

  {"@SWEEP", "CONNECTED HERE INVENTORY EXITS", cmd_sweep, CMD_T_ANY, 0, 0},
  {"@SWITCH", "NOTIFY FIRST ALL REGEXP", cmd_switch,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE |
   CMD_T_NOGAGGED, 0, 0},
  {"@SQUOTA", NULL, cmd_squota, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},

  {"@TELEPORT", "SILENT INSIDE", cmd_teleport,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@TRIGGER", NULL, cmd_trigger,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@ULOCK", NULL, cmd_ulock, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   0, 0},
  {"@UNDESTROY", NULL, cmd_undestroy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@UNLINK", NULL, cmd_unlink, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"@UNLOCK", NULL, cmd_unlock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_SWITCHES | CMD_T_NOGAGGED, 0, 0},
  {"@UNRECYCLE", NULL, cmd_undestroy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@UPTIME", "MORTAL", cmd_uptime, CMD_T_ANY, 0, 0},
  {"@UUNLOCK", NULL, cmd_uunlock, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@VERB", NULL, cmd_verb, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@VERSION", NULL, cmd_version, CMD_T_ANY, 0, 0},
  {"@WAIT", "PID UNTIL", cmd_wait, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE,
   0, 0},
  {"@WALL", "NOEVAL EMIT", cmd_wall, CMD_T_ANY, "WIZARD ROYALTY", "ANNOUNCE"},

  {"@WARNINGS", NULL, cmd_warnings, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@WCHECK", "ALL ME", cmd_wcheck, CMD_T_ANY, 0, 0},
  {"@WHEREIS", NULL, cmd_whereis, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@WIPE", NULL, cmd_wipe, CMD_T_ANY, 0, 0},
  {"@WIZWALL", "NOEVAL EMIT", cmd_wizwall, CMD_T_ANY, "WIZARD", 0},
  {"@WIZMOTD", NULL, cmd_wizmotd, CMD_T_ANY, "WIZARD", 0},
  {"@ZEMIT", NULL, cmd_zemit, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   0, 0},

  {"BUY", NULL, cmd_buy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"BRIEF", NULL, cmd_brief, CMD_T_ANY, 0, 0},
  {"DESERT", NULL, cmd_desert, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"DISMISS", NULL, cmd_dismiss, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"DROP", NULL, cmd_drop, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"EXAMINE", "ALL BRIEF DEBUG MORTAL PARENT", cmd_examine, CMD_T_ANY, 0, 0},
  {"EMPTY", NULL, cmd_empty, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, 0, 0},
  {"ENTER", NULL, cmd_enter, CMD_T_ANY, 0, 0},

  {"FOLLOW", NULL, cmd_follow,
   CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, 0, 0},

  {"GET", NULL, cmd_get, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, 0, 0},
  {"GIVE", "SILENT", cmd_give, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0,
   0},
  {"GOTO", NULL, cmd_goto, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"HOME", NULL, cmd_home, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"INVENTORY", NULL, cmd_inventory, CMD_T_ANY, 0, 0},

  {"KILL", NULL, cmd_kill, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"LOOK", "OUTSIDE", cmd_look, CMD_T_ANY, 0, 0},
  {"LEAVE", NULL, cmd_leave, CMD_T_PLAYER | CMD_T_THING, 0, 0},

  {"PAGE", "LIST NOEVAL PORT OVERRIDE", cmd_page,
   CMD_T_ANY | CMD_T_RS_NOPARSE | CMD_T_NOPARSE | CMD_T_EQSPLIT |
   CMD_T_NOGAGGED, 0, 0},
  {"POSE", "NOEVAL NOSPACE", cmd_pose, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"SCORE", NULL, cmd_score, CMD_T_ANY, 0, 0},
  {"SAY", "NOEVAL", cmd_say, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"SEMIPOSE", "NOEVAL", cmd_semipose, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"SLAY", NULL, cmd_slay, CMD_T_ANY, 0, 0},

  {"TEACH", NULL, cmd_teach, CMD_T_ANY | CMD_T_NOPARSE, 0, 0},
  {"THINK", "NOEVAL", cmd_think, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"UNFOLLOW", NULL, cmd_unfollow,
   CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, 0, 0},
  {"USE", NULL, cmd_use, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"WHISPER", "LIST NOISY SILENT NOEVAL", cmd_whisper,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"WITH", "NOEVAL ROOM", cmd_with, CMD_T_PLAYER | CMD_T_THING | CMD_T_EQSPLIT,
   0, 0},

  {"WHO", NULL, cmd_who, CMD_T_ANY, 0, 0},
  {"DOING", NULL, cmd_who_doing, CMD_T_ANY, 0, 0},
  {"SESSION", NULL, cmd_session, CMD_T_ANY, 0, 0},

/* ATTRIB_SET is an undocumented command - it's sugar to make it possible
 * enable/disable attribute setting with &XX or @XX
 */
  {"ATTRIB_SET", NULL, command_atrset,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_INTERNAL, 0, 0},

/* A way to stop people starting commands with functions */
  {"WARN_ON_MISSING", NULL, cmd_warn_on_missing,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL, 0, 0},

/* A way to let people override the Huh? message */
  {"HUH_COMMAND", NULL, cmd_huh_command,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL, 0, 0},

/* A way to let people override the unimplemented message */
  {"UNIMPLEMENTED_COMMAND", NULL, cmd_unimplemented,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL, 0, 0},

  {NULL, NULL, NULL, 0, 0, 0}
};


/* switch_list is defined in switchinc.c */
#include "switchinc.c"

/** Table of command permissions/restrictions. */
struct command_perms_t command_perms[] = {
  {"player", CMD_T_PLAYER},
  {"thing", CMD_T_THING},
  {"exit", CMD_T_EXIT},
  {"room", CMD_T_ROOM},
  {"any", CMD_T_ANY},
  {"god", CMD_T_GOD},
  {"nobody", CMD_T_DISABLED},
  {"nogagged", CMD_T_NOGAGGED},
  {"noguest", CMD_T_NOGUEST},
  {"nofixed", CMD_T_NOFIXED},
  {"logargs", CMD_T_LOGARGS},
  {"logname", CMD_T_LOGNAME},
#ifdef DANGEROUS
  {"listed", CMD_T_LISTED},
  {"switches", CMD_T_SWITCHES},
  {"internal", CMD_T_INTERNAL},
  {"ls_space", CMD_T_LS_SPACE},
  {"ls_noparse", CMD_T_LS_NOPARSE},
  {"rs_space", CMD_T_RS_SPACE},
  {"rs_noparse", CMD_T_RS_NOPARSE},
  {"eqsplit", CMD_T_EQSPLIT},
  {"ls_args", CMD_T_LS_ARGS},
  {"rs_args", CMD_T_RS_ARGS},
#endif
  {NULL, 0}
};

static void
strccat(char *buff, char **bp, const char *from)
{
  if (*buff)
    safe_str(", ", buff, bp);
  safe_str(from, buff, bp);
}

/* Comparison function for bsearch() */
static int
switch_cmp(const void *a, const void *b)
{
  const char *name = a;
  const SWITCH_VALUE *sw = b;
  return strcmp(name, sw->name);
}

/* This has different semantics than a prefix table, or we'd use that. */
static int
switch_find(COMMAND_INFO *cmd, const char *sw)
{
  SWITCH_VALUE *sw_val;

  if (!sw || !*sw || !dyn_switch_list)
    return 0;

  if (!cmd) {
    sw_val = bsearch(sw, dyn_switch_list, num_switches, sizeof(SWITCH_VALUE),
                     switch_cmp);
    if (sw_val)
      return sw_val->value;
    else
      return 0;
  } else {
    size_t len = strlen(sw);
    if (!cmd->sw.mask)
      return 0;
    sw_val = dyn_switch_list;
    while (sw_val->name) {
      if (SW_ISSET(cmd->sw.mask, sw_val->value)
          && (strncmp(sw_val->name, sw, len) == 0))
        return sw_val->value;
      sw_val++;
    }
  }
  return 0;
}

/** Test if a particular switch was given, using name
 * \param sw the switch mask to test
 * \param name the name of the switch to test for.
 */
bool
SW_BY_NAME(switch_mask sw, const char *name)
{
  int idx = switch_find(NULL, name);
  if (idx)
    return SW_ISSET(sw, idx);
  else
    return false;
}

/** Allocate and populate a COMMAND_INFO structure.
 * This function generates a new COMMAND_INFO structure, populates it
 * with given values, and returns a pointer. It should not be used
 * for local hacks - use command_add() instead.
 * \param name command name.
 * \param type types of objects that can use the command.
 * \param flagstr list of flags names to restrict command
 * \param powerstr list of power names to restrict command
 * \param sw mask of switches the command accepts.
 * \param func function to call when the command is executed.
 * \return pointer to a newly allocated COMMAND_INFO structure.
 */
COMMAND_INFO *
make_command(const char *name, int type,
             const char *flagstr, const char *powerstr,
             const char *sw, command_func func)
{
  COMMAND_INFO *cmd;
  cmd = slab_malloc(command_slab, NULL);
  memset(cmd, 0, sizeof(COMMAND_INFO));
  cmd->name = name;
  cmd->cmdlock = TRUE_BOOLEXP;
  cmd->restrict_message = NULL;
  cmd->func = func;
  cmd->type = type;
  switch (command_state) {
  case CMD_LOAD_BUILTIN:
    cmd->sw.names = sw;
    break;
  case CMD_LOAD_LOCAL:{
      char sw_copy[BUFFER_LEN];
      char *pos;
      cmd->sw.names = sw;
      mush_strncpy(sw_copy, sw, BUFFER_LEN);
      pos = sw_copy;
      while (pos) {
        char *thisone = split_token(&pos, ' ');
        st_insert(thisone, &switch_names);
      }
      break;
    }
  case CMD_LOAD_DONE:{
      switch_mask mask = switchmask(sw);
      if (mask) {
        cmd->sw.mask = SW_ALLOC();
        SW_COPY(cmd->sw.mask, mask);
      } else
        cmd->sw.mask = NULL;
    }
  }
  cmd->hooks.before.obj = NOTHING;
  cmd->hooks.before.attrname = NULL;
  cmd->hooks.after.obj = NOTHING;
  cmd->hooks.after.attrname = NULL;
  cmd->hooks.ignore.obj = NOTHING;
  cmd->hooks.ignore.attrname = NULL;
  cmd->hooks.override.obj = NOTHING;
  cmd->hooks.override.attrname = NULL;
  /* Restrict with no flags/powers, then manually parse flagstr and powerstr
     separately and add to restriction, to avoid issues with flags/powers with
     the same name (HALT flag and Halt power) */
  restrict_command(NOTHING, cmd, "");
  if ((flagstr && *flagstr) || (powerstr && *powerstr)) {
    char buff[BUFFER_LEN];
    char *bp, *one, list[BUFFER_LEN], *p;
    int first = 1;
    bp = buff;
    if (cmd->cmdlock != TRUE_BOOLEXP) {
      safe_chr('(', buff, &bp);
      safe_str(unparse_boolexp(NOTHING, cmd->cmdlock, UB_DBREF), buff, &bp);
      safe_str(")&", buff, &bp);
      free_boolexp(cmd->cmdlock);
    }
    if (flagstr && *flagstr) {
      strcpy(list, flagstr);
      p = trim_space_sep(list, ' ');
      while ((one = split_token(&p, ' '))) {
        if (!first)
          safe_chr('|', buff, &bp);
        first = 0;
        safe_str("FLAG^", buff, &bp);
        safe_str(one, buff, &bp);
      }
    }
    if (powerstr && *powerstr) {
      strcpy(list, powerstr);
      p = trim_space_sep(list, ' ');
      while ((one = split_token(&p, ' '))) {
        if (!first)
          safe_chr('|', buff, &bp);
        first = 0;
        safe_str("POWER^", buff, &bp);
        safe_str(one, buff, &bp);
      }
    }
    *bp = '\0';
    cmd->cmdlock = parse_boolexp(NOTHING, buff, CommandLock);

  }
  return cmd;
}

/** Add a new command to the command table.
 * This function is the top-level function for adding a new command.
 * \param name name of the command.
 * \param type types of objects that can use the command.
 * \param flagstr space-separated list of flags sufficient to use command.
 * \param powerstr space-separated list of powers sufficient to use command.
 * \param switchstr space-separated list of switches for the command.
 * \param func function to call when command is executed.
 * \return pointer to a newly allocated COMMAND_INFO entry or NULL.
 */
COMMAND_INFO *
command_add(const char *name, int type, const char *flagstr,
            const char *powerstr, const char *switchstr, command_func func)
{
  ptab_insert_one(&ptab_command, name,
                  make_command(name, type, flagstr, powerstr, switchstr, func));
  return command_find(name);
}

/* Add a new command from a .cnf file's "add_command" statement */
int
cnf_add_command(char *name, char *opts)
{
  COMMAND_INFO *command;
  int flags = 0;
  char *p, *one;

  if (opts && *opts) {
    p = trim_space_sep(opts, ' ');
    while ((one = split_token(&p, ' '))) {
      if (string_prefix("noparse", one)) {
        flags |= CMD_T_NOPARSE;
      } else if (string_prefix("rsargs", one)) {
        flags |= CMD_T_RS_ARGS;
      } else if (string_prefix("lsargs", one)) {
        flags |= CMD_T_LS_ARGS;
      } else if (string_prefix("eqsplit", one)) {
        flags |= CMD_T_EQSPLIT;
      } else {
        return 0;               /* unknown option */
      }
    }
  }

  name = trim_space_sep(name, ' ');
  upcasestr(name);
  command = command_find(name);
  if (command || !ok_command_name(name))
    return 0;
  command_add(mush_strdup(name, "command_add"), flags, NULL, 0,
              (flags & CMD_T_NOPARSE ? NULL : "NOEVAL"), cmd_unimplemented);
  return (command_find(name) != NULL);
}

/** Search for a command by (partial) name.
 * This function searches the command table for a (partial) name match
 * and returns a pointer to the COMMAND_INFO entry. It returns NULL
 * if the name given is a reserved alias or if no command table entry
 * matches.
 * \param name name of command to match.
 * \return pointer to a COMMAND_INFO entry, or NULL.
 */
COMMAND_INFO *
command_find(const char *name)
{

  char cmdname[BUFFER_LEN];
  strcpy(cmdname, name);
  upcasestr(cmdname);
  if (hash_find(&htab_reserved_aliases, cmdname))
    return NULL;
  return (COMMAND_INFO *) ptab_find(&ptab_command, cmdname);
}

/** Search for a command by exact name.
 * This function searches the command table for an exact name match
 * and returns a pointer to the COMMAND_INFO entry. It returns NULL
 * if the name given is a reserved alias or if no command table entry
 * matches.
 * \param name name of command to match.
 * \return pointer to a COMMAND_INFO entry, or NULL.
 */
COMMAND_INFO *
command_find_exact(const char *name)
{

  char cmdname[BUFFER_LEN];
  strcpy(cmdname, name);
  upcasestr(cmdname);
  if (hash_find(&htab_reserved_aliases, cmdname))
    return NULL;
  return (COMMAND_INFO *) ptab_find_exact(&ptab_command, cmdname);
}

/** Convert a switch string to a switch mask.
 * Given a space-separated list of switches in string form, return
 * a pointer to a static switch mask.
 * \param switches list of switches as a string.
 * \return pointer to a static switch mask.
 */
switch_mask
switchmask(const char *switches)
{
  static switch_mask sw = NULL;
  static int sm_bytes = 0;
  char buff[BUFFER_LEN];
  char *p, *s;
  int switchnum;

  if (sm_bytes < switch_bytes) {
    sw = mush_realloc(sw, switch_bytes, "cmd.switch.vector");
    sm_bytes = switch_bytes;
  }

  SW_ZERO(sw);
  if (!switches || !switches[0])
    return NULL;
  strcpy(buff, switches);
  p = buff;
  while (p) {
    s = split_token(&p, ' ');
    switchnum = switch_find(NULL, s);
    if (!switchnum)
      return NULL;
    else
      SW_SET(sw, switchnum);
  }
  return sw;
}

/** Add an alias to the table of reserved aliases.
 * This function adds an alias to the table of reserved aliases, preventing
 * it from being matched for standard commands. It's typically used to
 * insure that 'e' will match a global 'east;e' exit rather than the
 * 'examine' command.
 * \param a alias to reserve.
 */
void
reserve_alias(const char *a)
{
  static char placeholder[2] = "x";
  hashadd(strupper(a), (void *) placeholder, &htab_reserved_aliases);
}

/** Initialize command tables (before reading config file).
 * This function performs command table initialization that should take place
 * before the configuration file has been read. It initializes the
 * command prefix table and the reserved alias table, inserts all of the
 * commands from the commands array into the prefix table, initializes
 * the command permission prefix table, and inserts all the permissions
 * from the command_perms array into the prefix table. Finally, it
 * calls local_commands() to do any cmdlocal.c work.
 */
void
command_init_preconfig(void)
{
  struct command_perms_t *c;
  COMLIST *cmd;
  SWITCH_VALUE *sv;
  static int done = 0;
  if (done == 1)
    return;
  done = 1;

  ptab_init(&ptab_command);
  hashinit(&htab_reserved_aliases, 16);

  /* Build initial switch table. */
  st_init(&switch_names);
  for (sv = switch_list; sv->name; sv++)
    st_insert(sv->name, &switch_names);

  command_slab = slab_create("commands", sizeof(COMMAND_INFO));
  reserve_aliases();

  ptab_start_inserts(&ptab_command);
  command_state = CMD_LOAD_BUILTIN;
  for (cmd = commands; cmd->name; cmd++) {
    if (cmd->switches) {
      char sw_copy[BUFFER_LEN];
      char *pos;
      strcpy(sw_copy, cmd->switches);
      pos = sw_copy;
      while (pos) {
        char *sw = split_token(&pos, ' ');
        st_insert(sw, &switch_names);
      }
    }
    ptab_insert(&ptab_command, cmd->name,
                make_command(cmd->name, cmd->type,
                             cmd->flagstr, cmd->powers,
                             cmd->switches, cmd->func));
  }
  ptab_end_inserts(&ptab_command);

  ptab_init(&ptab_command_perms);
  ptab_start_inserts(&ptab_command_perms);
  for (c = command_perms; c->name; c++)
    ptab_insert(&ptab_command_perms, c->name, c);
  ptab_end_inserts(&ptab_command_perms);

  command_state = CMD_LOAD_LOCAL;
  local_commands();
}

struct bst_data {
  SWITCH_VALUE *table;
  size_t n;
  size_t start;
};

static void
build_switch_table(const char *sw, int count __attribute__ ((__unused__)),
                   void *d)
{
  SWITCH_VALUE *s;
  struct bst_data *data = d;

  for (s = switch_list; s->name; s++) {
    if (strcmp(s->name, sw) == 0) {
      data->table[data->n++] = *s;
      return;
    }
  }
  /* Not in switchinc.c table */
  data->table[data->n].value = data->start++;
  data->table[data->n++].name = mush_strdup(sw, "switch.name");
}

/** Initialize commands (after reading config file).
 * This function performs command initialization that should take place
 * after the configuration file has been read.
 * Currently, there isn't any.
 */
void
command_init_postconfig(void)
{
  struct bst_data sw_data;
  COMMAND_INFO *c;

  command_state = CMD_LOAD_DONE;

  /* First make the switch table */
  dyn_switch_list = mush_calloc(switch_names.count + 2, sizeof(SWITCH_VALUE),
                                "cmd_switch_table");
  if (!dyn_switch_list)
    mush_panic("Unable to allocate command switch table");
  sw_data.table = dyn_switch_list;
  sw_data.n = 0;
  sw_data.start = sizeof switch_list / sizeof(SWITCH_VALUE);
  st_walk(&switch_names, build_switch_table, &sw_data);
  num_switches = sw_data.start;
  dyn_switch_list[sw_data.n].name = NULL;
  st_flush(&switch_names);
  switch_bytes = ceil((double) num_switches / 8.0);

  /* Then convert the list of switch names in all commands to masks */
  for (c = ptab_firstentry(&ptab_command); c; c = ptab_nextentry(&ptab_command)) {
    const char *switchstr = c->sw.names;
    if (switchstr) {
      c->sw.mask = SW_ALLOC();
      SW_COPY(c->sw.mask, switchmask(switchstr));
    }
  }

  return;
}


/** Alias a command.
 * Given a command name and an alias for it, install the alias.
 * \param command canonical command name.
 * \param alias alias to associate with command.
 * \retval 0 failure (couldn't locate command).
 * \retval 1 success.
 */
int
alias_command(const char *command, const char *alias)
{
  COMMAND_INFO *cmd;

  /* Make sure the alias doesn't exit already */
  if (command_find_exact(alias))
    return 0;

  /* Look up the original */
  cmd = command_find_exact(command);
  if (!cmd)
    return 0;

  ptab_insert_one(&ptab_command, strupper(alias), cmd);
  return 1;
}

/* This is set to true for EQ_SPLIT commands that actually have a rhs.
 * Used in @teleport, ATTRSET and possibly other checks. It's ugly.
 * Blame Talek for it. ;)
 */
int rhs_present;

/** Parse the command arguments into arrays.
 * This function does the real work of parsing command arguments into
 * argument arrays. It is called separately to parse the left and
 * right sides of commands that are split at equal signs.
 * \param player the enactor.
 * \param cause the dbref causing the execution.
 * \param from pointer to address of where to parse arguments from.
 * \param to string to store parsed arguments into.
 * \param argv array of parsed arguments.
 * \param cmd pointer to command data.
 * \param right_side if true, parse on the right of the =. Otherwise, left.
 * \param forcenoparse if true, do no evaluation during parsing.
 */
void
command_argparse(dbref player, dbref cause, char **from, char *to,
                 char *argv[], COMMAND_INFO *cmd, int right_side,
                 int forcenoparse)
{
  int parse, split, args, i, done;
  char *t, *f;
  char *aold;

  f = *from;

  parse =
    (right_side) ? (cmd->type & CMD_T_RS_NOPARSE) : (cmd->type & CMD_T_NOPARSE);
  if (parse || forcenoparse)
    parse = PE_NOTHING;
  else
    parse = PE_DEFAULT | PE_COMMAND_BRACES;

  if (right_side)
    split = PT_NOTHING;
  else
    split = (cmd->type & CMD_T_EQSPLIT) ? PT_EQUALS : PT_NOTHING;

  if (right_side) {
    if (cmd->type & CMD_T_RS_ARGS)
      args = (cmd->type & CMD_T_RS_SPACE) ? PT_SPACE : PT_COMMA;
    else
      args = 0;
  } else {
    if (cmd->type & CMD_T_LS_ARGS)
      args = (cmd->type & CMD_T_LS_SPACE) ? PT_SPACE : PT_COMMA;
    else
      args = 0;
  }

  if ((parse == PE_NOTHING) && args)
    parse = PE_COMMAND_BRACES;

  i = 1;
  done = 0;
  *to = '\0';

  if (args) {
    t = to + 1;
  } else {
    t = to;
  }

  while (*f && !done) {
    aold = t;
    while (*f == ' ')
      f++;
    if (process_expression(to, &t, (const char **) &f, player, cause, cause,
                           parse, (split | args),
                           global_eval_context.pe_info)) {
      done = 1;
    }
    *t = '\0';
    if (args) {
      argv[i] = aold;
      if (*f)
        f++;
      i++;
      /* Because we test on f, not t. This was causing a bug wherein
       * trying to build a commandraw with multiple rsargs, including
       * one massive one, was causing a crash.
       */
      if ((t - to) < (BUFFER_LEN - 1))
        t++;
      if (i == MAX_ARG)
        done = 1;
    }
    if (split && (*f == '=')) {
      rhs_present = 1;
      f++;
      *from = f;
      done = 1;
    }
  }

  *from = f;

  if (args)
    while (i < MAX_ARG)
      argv[i++] = NULL;
}

/** Determine whether a command is an attribute to set an attribute.
 * Is this command an attempt to set an attribute like @VA or &NUM?
 * If so, return the attrib's name. Otherwise, return NULL
 * \param command command string (first word of input).
 * \return name of the attribute to be set, or NULL.
 */
static const char *
command_isattr(char *command)
{
  ATTR *a;
  char buff[BUFFER_LEN];
  char *f, *t;

  if (((command[0] == '&') && (command[1])) ||
      ((command[0] == '@') && (command[1] == '_') && (command[2]))) {
    /* User-defined attributes: @_NUM or &NUM */
    if (command[0] == '@')
      return command + 2;
    else
      return command + 1;
  } else if (command[0] == '@') {
    f = command + 1;
    buff[0] = '@';
    t = buff + 1;
    while ((*f) && (*f != '/'))
      *t++ = *f++;
    *t = '\0';
    /* @-commands have priority over @-attributes with the same name */
    if (command_find(buff))
      return NULL;
    a = atr_match(buff + 1);
    if (a)
      return AL_NAME(a);
  }
  return NULL;
}

/** A handy macro to free up the command_parse-allocated variables */
#define command_parse_free_args	  \
    mush_free(command, "string_command"); \
    mush_free(swtch, "string_swtch"); \
    mush_free(ls, "string_ls"); \
    mush_free(rs, "string_rs"); \
    mush_free(switches, "string_switches"); \
    if (sw) SW_FREE(sw)

/** Parse commands.
 * Parse the commands. This is the big one!
 * We distinguish parsing of input sent from a player at a socket
 * (in which case attribute values to set are not evaluated) and
 * input sent in any other way (in which case attribute values to set
 * are evaluated, and attributes are set NO_COMMAND).
 * Return NULL if the command was recognized and handled, the evaluated
 * text to match against $-commands otherwise.
 * \param player the enactor.
 * \param cause dbref that caused the command to be executed.
 * \param string the input to be parsed.
 * \param fromport if true, command was typed by a player at a socket.
 * \return NULL if a command was handled, otherwise the evaluated input.
 */
char *
command_parse(dbref player, dbref cause, char *string, int fromport)
{
  char *command, *swtch, *ls, *rs, *switches;
  static char commandraw[BUFFER_LEN];
  static char exit_command[BUFFER_LEN], *ec;
  char *lsa[MAX_ARG];
  char *rsa[MAX_ARG];
  char *ap, *swp;
  const char *attrib, *replacer;
  COMMAND_INFO *cmd;
  char *p, *t, *c, *c2;
  char command2[BUFFER_LEN];
  char b;
  int switchnum;
  switch_mask sw = NULL;
  char switch_err[BUFFER_LEN], *se;
  int noeval;
  int noevtoken = 0;
  char *retval;

  rhs_present = 0;

  command = mush_malloc(BUFFER_LEN, "string_command");
  swtch = mush_malloc(BUFFER_LEN, "string_swtch");
  ls = mush_malloc(BUFFER_LEN, "string_ls");
  rs = mush_malloc(BUFFER_LEN, "string_rs");
  switches = mush_malloc(BUFFER_LEN, "string_switches");
  if (!command || !swtch || !ls || !rs || !switches)
    mush_panic("Couldn't allocate memory in command_parse");
  p = string;
  replacer = NULL;
  attrib = NULL;
  cmd = NULL;
  c = command;
  /* All those one character commands.. Sigh */

  global_fun_invocations = global_fun_recursions = 0;
  if (*p == NOEVAL_TOKEN) {
    noevtoken = 1;
    p = string + 1;
    string = p;
    memmove(global_eval_context.ccom, (char *) global_eval_context.ccom + 1,
            BUFFER_LEN - 1);
  }
  if (*p == '[') {
    if ((cmd = command_find("WARN_ON_MISSING"))) {
      if (!(cmd->type & CMD_T_DISABLED)) {
        cmd->func(cmd, player, cause, sw, string, NULL, NULL, ls, lsa, rs, rsa);
        command_parse_free_args;
        return NULL;
      }
    }
  }
  switch (*p) {
  case '\0':
    /* Just in case. You never know */
    command_parse_free_args;
    return NULL;
  case SAY_TOKEN:
    replacer = "SAY";
#if 0
    /* Messes up hooks when chat_strip_quote is yes. See bug #6677 */
    if (CHAT_STRIP_QUOTE)
      p--;                      /* Since 'say' strips out the '"' */
#endif
    break;
  case POSE_TOKEN:
    replacer = "POSE";
    break;
  case SEMI_POSE_TOKEN:
    if (*(p + 1) && *(p + 1) == ' ')
      replacer = "POSE";
    else
      replacer = "SEMIPOSE";
    break;
  case EMIT_TOKEN:
    replacer = "@EMIT";
    break;
  case CHAT_TOKEN:
#ifdef CHAT_TOKEN_ALIAS
  case CHAT_TOKEN_ALIAS:
#endif
    /* parse_chat() destructively modifies the command to replace
     * the first space with a '=' if the command is an actual
     * chat command */
    if (parse_chat(player, p + 1) && command_check_byname(player, "@CHAT")) {
      /* This is a "+chan foo" chat style
       * We set noevtoken to keep its noeval way, and
       * set the cmd to allow @hook. */
      replacer = "@CHAT";
      noevtoken = 1;
    }
    break;
  case NUMBER_TOKEN:
    /* parse_force() destructively modifies the command to replace
     * the first space with a '=' if the command is an actual @force */
    if (Mobile(player) && parse_force(p)) {
      /* This is a "#obj foo" force style
       * We set noevtoken to keep its noeval way, and
       * set the cmd to allow @hook. */
      replacer = "@FORCE";
      noevtoken = 1;
    }
  }

  if (replacer) {
    cmd = command_find(replacer);
    if (*p != NUMBER_TOKEN)
      p++;
  } else {
    /* At this point, we have not done a replacer, so we continue with the
     * usual processing. Exits have next priority.  We still pass them
     * through the parser so @hook on GOTO can work on them.
     */
    if (strcasecmp(p, "home") && can_move(player, p)) {
      ec = exit_command;
      safe_str("GOTO ", exit_command, &ec);
      safe_str(p, exit_command, &ec);
      *ec = '\0';
      p = string = exit_command;
      noevtoken = 1;            /* But don't parse the exit name! */
    }
    c = command;
    while (*p == ' ')
      p++;
    process_expression(command, &c, (const char **) &p, player, cause, cause,
                       noevtoken ? PE_NOTHING :
                       ((PE_DEFAULT & ~PE_FUNCTION_CHECK) |
                        PE_COMMAND_BRACES), PT_SPACE,
                       global_eval_context.pe_info);
    *c = '\0';
    strcpy(commandraw, command);
    upcasestr(command);

    /* Catch &XX and @XX attribute pairs. If that's what we've got,
     * use the magical ATTRIB_SET command
     */
    attrib = command_isattr(command);
    if (attrib) {
      cmd = command_find("ATTRIB_SET");
    } else {
      c = command;
      while ((*c) && (*c != '/'))
        c++;
      b = *c;
      *c = '\0';
      cmd = command_find(command);
      *c = b;
      /* Is this for internal use? If so, players can't use it! */
      if (cmd && (cmd->type & CMD_T_INTERNAL))
        cmd = NULL;
    }
  }

  /* Test if this either isn't a command, or is a disabled one
   * If so, return Fully Parsed string for further processing.
   */
  if (!cmd || (cmd->type & CMD_T_DISABLED)) {
    c2 = commandraw + strlen(commandraw);
    if (*p) {
      if (*p == ' ') {
        safe_chr(' ', commandraw, &c2);
        p++;
      }
      process_expression(commandraw, &c2, (const char **) &p, player, cause,
                         cause, noevtoken ? PE_NOTHING :
                         ((PE_DEFAULT & ~PE_FUNCTION_CHECK) |
                          PE_COMMAND_BRACES), PT_DEFAULT,
                         global_eval_context.pe_info);
    }
    *c2 = '\0';
    command_parse_free_args;
    return commandraw;
  } else if (!command_check(player, cmd, 1)) {
    /* No permission to use command, stop processing */
    command_parse_free_args;
    return NULL;
  }

  /* Set up commandraw for future use. This will contain the canonicalization
   * of the command name and will later have the parsed rest of the input
   * appended at the position pointed to by c2.
   */
  c2 = c;
  if (replacer) {
    /* These commands don't allow switches, and need a space
     * added after their canonical name
     */
    c2 = commandraw;
    safe_str(cmd->name, commandraw, &c2);
    safe_chr(' ', commandraw, &c2);
  } else if (*c2 == '/') {
    /* Oh... DAMN */
    c2 = commandraw;
    strcpy(switches, commandraw);
    safe_str(cmd->name, commandraw, &c2);
    t = strchr(switches, '/');
    safe_str(t, commandraw, &c2);
  } else {
    c2 = commandraw;
    safe_str(cmd->name, commandraw, &c2);
  }

  /* Parse out any switches */
  sw = SW_ALLOC();
  swp = switches;
  *swp = '\0';
  se = switch_err;

  t = NULL;

  /* Don't parse switches for one-char commands */
  if (!replacer) {
    while (*c == '/') {
      t = swtch;
      c++;
      while ((*c) && (*c != ' ') && (*c != '/'))
        *t++ = *c++;
      *t = '\0';
      switchnum = switch_find(cmd, upcasestr(swtch));
      if (!switchnum) {
        if (cmd->type & CMD_T_SWITCHES) {
          if (*swp)
            strcat(swp, " ");
          strcat(swp, swtch);
        } else {
          if (se == switch_err)
            safe_format(switch_err, &se,
                        T("%s doesn't know switch %s."), cmd->name, swtch);
        }
      } else {
        SW_SET(sw, switchnum);
      }
    }
  }

  *se = '\0';
  if (!t)
    SW_SET(sw, SWITCH_NONE);
  if (noevtoken)
    SW_SET(sw, SWITCH_NOEVAL);

  /* If we're calling ATTRIB_SET, the switch is the attribute name */
  if (attrib)
    swp = (char *) attrib;
  else if (!*swp)
    swp = NULL;

  strcpy(command2, p);
  if (*p == ' ')
    p++;
  ap = p;

  /* noeval and direct players.
   * If the noeval switch is set:
   *  (1) if we split on =, and an = is present, eval lhs, not rhs
   *  (2) if we split on =, and no = is present, do not eval arg
   *  (3) if we don't split on =, do not eval arg
   * Special case for ATTRIB_SET by a directly connected player:
   * Treat like noeval, except for #2. Eval arg if no =.
   */

  if ((cmd->func == command_atrset) && fromport) {
    /* Special case: eqsplit, noeval of rhs only */
    command_argparse(player, cause, &p, ls, lsa, cmd, 0, 0);
    command_argparse(player, cause, &p, rs, rsa, cmd, 1, 1);
    SW_SET(sw, SWITCH_NOEVAL);  /* Needed for ATTRIB_SET */
  } else {
    noeval = SW_ISSET(sw, SWITCH_NOEVAL) || noevtoken;
    if (cmd->type & CMD_T_EQSPLIT) {
      char *savep = p;
      command_argparse(player, cause, &p, ls, lsa, cmd, 0, noeval);
      if (noeval && !noevtoken && *p) {
        /* oops, we have a right hand side, should have evaluated */
        p = savep;
        command_argparse(player, cause, &p, ls, lsa, cmd, 0, 0);
      }
      command_argparse(player, cause, &p, rs, rsa, cmd, 1, noeval);
    } else {
      command_argparse(player, cause, &p, ls, lsa, cmd, 0, noeval);
    }
  }


  /* Finish setting up commandraw, for hooks and %u */
  p = command2;
  if (attrib) {
    safe_chr('/', commandraw, &c2);
    safe_str(swp, commandraw, &c2);
  }
  if (*p && (*p == ' ')) {
    safe_chr(' ', commandraw, &c2);
    p++;
  }
  if (cmd->type & CMD_T_ARGS) {
    int lsa_index;
    if (lsa[1]) {
      safe_str(lsa[1], commandraw, &c2);
      for (lsa_index = 2; (lsa_index < MAX_ARG) && lsa[lsa_index]; lsa_index++) {
        safe_chr(',', commandraw, &c2);
        safe_str(lsa[lsa_index], commandraw, &c2);
      }
    }
  } else {
    safe_str(ls, commandraw, &c2);
  }
  if (cmd->type & CMD_T_EQSPLIT) {
    if (rhs_present) {
      safe_chr('=', commandraw, &c2);
      if (cmd->type & CMD_T_RS_ARGS) {
        int rsa_index;
        /* This is counterintuitive, but rsa[]
         * starts at 1. */
        if (rsa[1]) {
          safe_str(rsa[1], commandraw, &c2);
          for (rsa_index = 2; (rsa_index < MAX_ARG) && rsa[rsa_index];
               rsa_index++) {
            safe_chr(',', commandraw, &c2);
            safe_str(rsa[rsa_index], commandraw, &c2);
          }
        }
      } else {
        safe_str(rs, commandraw, &c2);
      }
    }
  }
#ifdef NEVER
  /* We used to do this, but we're not sure why */
  process_expression(commandraw, &c2, (const char **) &p, player, cause,
                     cause, noevtoken ? PE_NOTHING :
                     ((PE_DEFAULT & ~PE_EVALUATE) |
                      PE_COMMAND_BRACES), PT_DEFAULT, NULL);
#endif
  *c2 = '\0';
  mush_strncpy(global_eval_context.ucom, commandraw, BUFFER_LEN);

  retval = NULL;
  if (cmd->func == NULL) {
    do_rawlog(LT_ERR, "No command vector on command %s.", cmd->name);
    command_parse_free_args;
    return NULL;
  } else {
    /* If we have a hook/ignore that returns false, we don't do the command */
    if (run_command
        (cmd, player, cause, commandraw, sw, switch_err, string, swp, ap, ls,
         lsa, rs, rsa)) {
      retval = NULL;
    } else {
      retval = commandraw;
    }
  }

  command_parse_free_args;
  return retval;
}

#undef command_parse_free_args

int
run_command(COMMAND_INFO *cmd, dbref player, dbref cause,
            const char *commandraw, switch_mask sw, char switch_err[BUFFER_LEN],
            const char *string, char *swp, char *ap, char *ls,
            char *lsa[MAX_ARG], char *rs, char *rsa[MAX_ARG])
{

  char *saveregs[NUMQ];

  if (!cmd)
    return 0;

  init_global_regs(saveregs);

  if (!run_hook(player, cause, &cmd->hooks.ignore, saveregs, 1)) {
    free_global_regs("hook.regs", saveregs);
    return 0;
  }

  /* If we have a hook/override, we use that instead */
  if (!run_hook_override(cmd, player, commandraw)) {
    /* Otherwise, we do hook/before, the command, and hook/after */
    /* But first, let's see if we had an invalid switch */
    if (switch_err && *switch_err) {
      notify(player, switch_err);
      return 1;
    }
    run_hook(player, cause, &cmd->hooks.before, saveregs, 1);
    cmd->func(cmd, player, cause, sw, string, swp, ap, ls, lsa, rs, rsa);
    run_hook(player, cause, &cmd->hooks.after, saveregs, 0);
  }
  /* Either way, we might log */
  if (cmd->type & CMD_T_LOGARGS)
    if (cmd->func == cmd_password || cmd->func == cmd_newpassword
        || cmd->func == cmd_pcreate)
      do_log(LT_CMD, player, cause, "%s %s=***", cmd->name,
             (cmd->func == cmd_password ? "***" : ls));
    else
      do_log(LT_CMD, player, cause, "%s", commandraw);
  else if (cmd->type & CMD_T_LOGNAME)
    do_log(LT_CMD, player, cause, "%s", cmd->name);

  free_global_regs("hook.regs", saveregs);
  return 1;

}

/** Execute the huh_command when no command is matched.
 * \param player the enactor.
 * \param cause dbref that caused the command to be executed.
 * \param string the input given.
 */
void
generic_command_failure(dbref player, dbref cause, char *string)
{
  COMMAND_INFO *cmd;

  if ((cmd = command_find("HUH_COMMAND")) && !(cmd->type & CMD_T_DISABLED)) {
    run_command(cmd, player, cause, "HUH_COMMAND", NULL, NULL, string, NULL,
                NULL, string, NULL, NULL, NULL);
  }
}

#define add_restriction(r, j) \
        if(lockstr != tp && j) \
          safe_chr(j, lockstr, &tp); \
        safe_str(r, lockstr, &tp)

/** Add a restriction to a command.
 * Given a command name and a restriction, apply the restriction to the
 * command in addition to whatever its usual restrictions are.
 * This is used by the configuration file startup in conf.c
 * Valid restrictions are:
 * \verbatim
 *   nobody     disable the command
 *   nogagged   can't be used by gagged players
 *   nofixed    can't be used by fixed players
 *   noguest    can't be used by guests
 *   admin      can only be used by royalty or wizards
 *   wizard     can only be used by wizards
 *   god        can only be used by god
 *   noplayer   can't be used by players, just objects/rooms/exits
 *   logargs    log name and arguments when command is run
 *   logname    log just name when command is run
 * \endverbatim
 * Return 1 on success, 0 on failure.
 * \param name name of command to restrict.
 * \param restriction space-separated string of restrictions
 * \retval 1 successfully restricted command.
 * \retval 0 failure (unable to find command name).
 */
int
restrict_command(dbref player, COMMAND_INFO *command, const char *xrestriction)
{
  struct command_perms_t *c;
  char *message, *restriction, *rsave;
  int clear;
  FLAG *f;
  char lockstr[BUFFER_LEN];
  char *tp;
  int make_boolexp = 0;
  boolexp key;
  object_flag_type flags = NULL, powers = NULL;

  if (!command)
    return 0;

  /* Allow empty restrictions when we first load commands, as we parse off the CMD_T_* flags */
  if (GoodObject(player) && (!xrestriction || !*xrestriction))
    return 0;

  if (command->restrict_message) {
    mush_free((void *) command->restrict_message, "cmd_restrict_message");
    command->restrict_message = NULL;
  }
  rsave = restriction = mush_strdup(xrestriction, "rc.string");
  message = strchr(restriction, '"');
  if (message) {
    *(message++) = '\0';
    if ((message = trim_space_sep(message, ' ')) && *message)
      command->restrict_message = mush_strdup(message, "cmd_restrict_message");
  }

  if (command->cmdlock != TRUE_BOOLEXP) {
    free_boolexp(command->cmdlock);
    command->cmdlock = TRUE_BOOLEXP;
  }

  key = parse_boolexp(player, restriction, CommandLock);
  if (key != TRUE_BOOLEXP) {
    /* A valid boolexp lock. Hooray. */
    command->cmdlock = key;
    mush_free(rsave, "rc.string");
    return 1;
  }

  flags = new_flag_bitmask("FLAG");
  powers = new_flag_bitmask("POWER");

  /* Parse old-style restriction into a boolexp */
  while (restriction && *restriction) {
    if ((tp = strchr(restriction, ' ')))
      *tp++ = '\0';

    clear = 0;
    if (*restriction == '!') {
      restriction++;
      clear = 1;
    }

    if (!strcasecmp(restriction, "noplayer")) {
      /* Pfft. And even !noplayer works. */
      clear = !clear;
      restriction += 2;
    }

    /* Gah. I love backwards compatiblity. */
    if (!strcasecmp(restriction, "admin")) {
      make_boolexp = 1;
      if (clear) {
        f = match_flag("ROYALTY");
        clear_flag_bitmask(flags, f->bitpos);
        f = match_flag("WIZARD");
        clear_flag_bitmask(flags, f->bitpos);
      } else {
        f = match_flag("ROYALTY");
        set_flag_bitmask(flags, f->bitpos);
        f = match_flag("WIZARD");
        set_flag_bitmask(flags, f->bitpos);
      }
    } else if ((c = ptab_find(&ptab_command_perms, restriction))) {
      if (clear)
        command->type &= ~c->type;
      else
        command->type |= c->type;
    } else if ((f = match_flag(restriction))) {
      make_boolexp = 1;
      if (clear)
        clear_flag_bitmask(flags, f->bitpos);
      else {
        set_flag_bitmask(flags, f->bitpos);
      }
    } else if ((f = match_power(restriction))) {
      make_boolexp = 1;
      if (clear)
        clear_flag_bitmask(powers, f->bitpos);
      else {
        set_flag_bitmask(powers, f->bitpos);
      }
    }
    restriction = tp;
  }

  if ((command->type &
       (CMD_T_GOD | CMD_T_NOGAGGED | CMD_T_NOGUEST | CMD_T_NOFIXED)))
    make_boolexp = 1;
  else if ((command->type & CMD_T_ANY) != CMD_T_ANY)
    make_boolexp = 1;

  mush_free(rsave, "rc.string");

  /* And now format what we have into a lock string, if necessary */
  if (!make_boolexp) {
    destroy_flag_bitmask(flags);
    destroy_flag_bitmask(powers);
    return 1;
  }

  tp = lockstr;
  safe_str(flag_list_to_lock_string(flags, powers), lockstr, &tp);
  if ((command->type & CMD_T_ANY) != CMD_T_ANY) {
    char join = '\0';

    if (lockstr != tp)
      safe_chr('&', lockstr, &tp);
    safe_chr('(', lockstr, &tp);

    /* Type-locked command */
    if (command->type & CMD_T_PLAYER) {
      add_restriction("TYPE^PLAYER", join);
      join = '|';
    }
    if (command->type & CMD_T_THING) {
      add_restriction("TYPE^THING", join);
      join = '|';
    }
    if (command->type & CMD_T_ROOM) {
      add_restriction("TYPE^ROOM", join);
      join = '|';
    }
    if (command->type & CMD_T_EXIT) {
      add_restriction("TYPE^EXIT", join);
      join = '|';
    }
    if (join)
      safe_chr(')', lockstr, &tp);
  }
  if (command->type & CMD_T_GOD) {
    add_restriction(tprintf("=#%d", GOD), '&');
  }
  if (command->type & CMD_T_NOGUEST) {
    add_restriction("!POWER^GUEST", '&');
  }
  if (command->type & CMD_T_NOGAGGED) {
    add_restriction("!FLAG^GAGGED", '&');
  }
  if (command->type & CMD_T_NOFIXED) {
    add_restriction("!FLAG^FIXED", '&');
  }
  /* CMD_T_DISABLED and CMD_T_LOG* are checked for in command->types, and not part of the boolexp */
  *tp = '\0';

  key = parse_boolexp(player, lockstr, CommandLock);
  command->cmdlock = key;

  destroy_flag_bitmask(flags);
  destroy_flag_bitmask(powers);
  return 1;
}

#undef add_restriction

/** Command stub for \@command/add-ed commands.
 * This does nothing more than notify the player
 * with "This command has not been implemented."
 */
COMMAND(cmd_unimplemented)
{

  if (strcmp(cmd->name, "UNIMPLEMENTED_COMMAND") != 0 &&
      (cmd = command_find("UNIMPLEMENTED_COMMAND")) &&
      !(cmd->type & CMD_T_DISABLED)) {
    run_command(cmd, player, cause, "UNIMPLEMENTED_COMMAND", sw, NULL, raw,
                NULL, args_raw, arg_left, args_left, arg_right, args_right);
  } else {
    /* Either we were already in UNIMPLEMENTED_COMMAND, or we couldn't find it */
    notify(player, T("This command has not been implemented."));
  }
}

/** Adds a user-added command
 * \verbatim
 * This code implements @command/add, which adds a
 * command with cmd_unimplemented as a stub
 * \endverbatim
 * \param player the enactor
 * \param name the name
 * \param flags CMD_T_* flags
 */
void
do_command_add(dbref player, char *name, int flags)
{
  COMMAND_INFO *command;

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  name = trim_space_sep(name, ' ');
  upcasestr(name);
  command = command_find(name);
  if (!command) {
    if (!ok_command_name(name)) {
      notify(player, T("Bad command name."));
    } else {
      command_add(mush_strdup(name, "command_add"),
                  flags, NULL,
                  0, (flags & CMD_T_NOPARSE ? NULL : "NOEVAL"),
                  cmd_unimplemented);
      notify_format(player, T("Command %s added."), name);
    }
  } else {
    notify_format(player, T("Command %s already exists."), command->name);
  }
}

void
do_command_clone(dbref player, char *original, char *clone)
{
  COMMAND_INFO *cmd;

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  upcasestr(original);
  upcasestr(clone);

  cmd = command_find(original);
  if (!cmd) {
    notify(player, T("No such command."));
    return;
  } else if (!ok_command_name(clone) || command_find(clone)) {
    notify(player, T("Bad command name."));
    return;
  }

  clone_command(original, clone);
  notify(player, T("Command cloned."));

}

static COMMAND_INFO *
clone_command(char *original, char *clone)
{

  COMMAND_INFO *c1, *c2;

  upcasestr(original);
  upcasestr(clone);

  c1 = command_find(original);
  c2 = command_find(clone);
  if (!c1 || c2)
    return NULL;

  c2 =
    make_command(mush_strdup(clone, "command_add"), c1->type, NULL, NULL,
                 c1->sw.names, c1->func);
  c2->sw.mask = c1->sw.mask;
  if (c1->restrict_message)
    c2->restrict_message =
      mush_strdup(c1->restrict_message, "cmd_restrict_message");
  if (c2->cmdlock != TRUE_BOOLEXP)
    free_boolexp(c2->cmdlock);
  c2->cmdlock = dup_bool(c1->cmdlock);

  if (c1->hooks.before.obj)
    c2->hooks.before.obj = c1->hooks.before.obj;
  if (c1->hooks.before.attrname)
    c2->hooks.before.attrname =
      mush_strdup(c1->hooks.before.attrname, "hook.attr");

  if (c1->hooks.after.obj)
    c2->hooks.after.obj = c1->hooks.after.obj;
  if (c1->hooks.after.attrname)
    c2->hooks.after.attrname =
      mush_strdup(c1->hooks.after.attrname, "hook.attr");

  if (c1->hooks.ignore.obj)
    c2->hooks.ignore.obj = c1->hooks.ignore.obj;
  if (c1->hooks.ignore.attrname)
    c2->hooks.ignore.attrname =
      mush_strdup(c1->hooks.ignore.attrname, "hook.attr");

  if (c1->hooks.override.obj)
    c2->hooks.override.obj = c1->hooks.override.obj;
  if (c1->hooks.override.attrname)
    c2->hooks.override.attrname =
      mush_strdup(c1->hooks.override.attrname, "hook.attr");

  ptab_insert_one(&ptab_command, clone, c2);
  return command_find(clone);
}


/** Deletes a user-added command
 * \verbatim
 * This code implements @command/delete, which deletes a
 * command added via @command/add
 * \endverbatim
 * \param player the enactor
 * \param name name of the command to delete
 */
void
do_command_delete(dbref player, char *name)
{
  int acount;
  char alias[BUFFER_LEN];
  COMMAND_INFO *cptr;
  COMMAND_INFO *command;

  if (!God(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  upcasestr(name);
  command = command_find_exact(name);
  if (!command) {
    notify(player, T("No such command."));
    return;
  }
  if (strcasecmp(command->name, name) == 0) {
    /* This is the command, not an alias */
    if (command->func != cmd_unimplemented || !strcmp(command->name, "@SQL")) {
      notify(player,
             T
             ("You can't delete built-in commands. @command/disable instead."));
      return;
    } else {
      acount = 0;
      cptr = ptab_firstentry_new(&ptab_command, alias);
      while (cptr) {
        if (cptr == command) {
          ptab_delete(&ptab_command, alias);
          acount++;
          cptr = ptab_firstentry_new(&ptab_command, alias);
        } else
          cptr = ptab_nextentry_new(&ptab_command, alias);
      }
      mush_free((void *) command->name, "command_add");
      slab_free(command_slab, command);
      if (acount > 1)
        notify_format(player, T("Removed %s and aliases from command table."),
                      name);
      else
        notify_format(player, T("Removed %s from command table."), name);
    }
  } else {
    /* This is an alias. Just remove it */
    ptab_delete(&ptab_command, name);
    notify_format(player, T("Removed %s from command table."), name);
  }
}

/** Definition of the \@command command.
 * This is the only command which should be defined in this
 * file, because it uses variables from this file, etc.
 */
COMMAND(cmd_command)
{
  COMMAND_INFO *command;
  SWITCH_VALUE *sw_val;
  char buff[BUFFER_LEN];
  char *bp = buff;

  if (!arg_left[0]) {
    notify(player, T("You must specify a command."));
    return;
  }
  if (SW_ISSET(sw, SWITCH_ADD)) {
    int flags = CMD_T_ANY;
    flags |= SW_ISSET(sw, SWITCH_NOPARSE) ? CMD_T_NOPARSE : 0;
    flags |= SW_ISSET(sw, SWITCH_RSARGS) ? CMD_T_RS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_LSARGS) ? CMD_T_LS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_LSARGS) ? CMD_T_LS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_EQSPLIT) ? CMD_T_EQSPLIT : 0;
    if (SW_ISSET(sw, SWITCH_NOEVAL))
      notify(player,
             T
             ("WARNING: /NOEVAL no longer creates a Noparse command.\n         Use /NOPARSE if that's what you meant."));
    do_command_add(player, arg_left, flags);
    return;
  }
  if (SW_ISSET(sw, SWITCH_ALIAS)) {
    if (Wizard(player)) {
      if (!ok_command_name(upcasestr(arg_right))) {
        notify(player, T("I can't alias a command to that!"));
      } else if (!alias_command(arg_left, arg_right)) {
        notify(player, T("Unable to set alias."));
      } else {
        if (!SW_ISSET(sw, SWITCH_QUIET))
          notify(player, T("Alias set."));
      }
    } else {
      notify(player, T("Permission denied."));
    }
    return;
  }
  if (SW_ISSET(sw, SWITCH_CLONE)) {
    do_command_clone(player, arg_left, arg_right);
    return;
  }

  if (SW_ISSET(sw, SWITCH_DELETE)) {
    do_command_delete(player, arg_left);
    return;
  }
  command = command_find(arg_left);
  if (!command) {
    notify(player, T("No such command."));
    return;
  }
  if (Wizard(player)) {
    if (SW_ISSET(sw, SWITCH_ON) || SW_ISSET(sw, SWITCH_ENABLE))
      command->type &= ~CMD_T_DISABLED;
    else if (SW_ISSET(sw, SWITCH_OFF) || SW_ISSET(sw, SWITCH_DISABLE))
      command->type |= CMD_T_DISABLED;

    if (SW_ISSET(sw, SWITCH_RESTRICT)) {
      if (!arg_right || !arg_right[0]) {
        notify(player, T("How do you want to restrict the command?"));
        return;
      }

      if (!restrict_command(player, command, arg_right))
        notify(player, T("Restrict attempt failed."));
    }

    if ((command->func == cmd_command) && (command->type & CMD_T_DISABLED)) {
      notify(player, T("@command is ALWAYS enabled."));
      command->type &= ~CMD_T_DISABLED;
    }
  }
  if (!SW_ISSET(sw, SWITCH_QUIET)) {
    notify_format(player,
                  T("Name       : %s (%s)"), command->name,
                  (command->type & CMD_T_DISABLED) ? "Disabled" : "Enabled");
    buff[0] = '\0';
    bp = buff;
    if (command->type & CMD_T_SWITCHES)
      strccat(buff, &bp, "Switches");
    if (command->type & CMD_T_EQSPLIT)
      strccat(buff, &bp, "Eqsplit");
    if (command->type & CMD_T_LOGARGS)
      strccat(buff, &bp, "LogArgs");
    else if (command->type & CMD_T_LOGNAME)
      strccat(buff, &bp, "LogName");
    *bp = '\0';
    notify_format(player, T("Flags      : %s"), buff);
    buff[0] = '\0';
    notify_format(player, T("Lock       : %s"),
                  unparse_boolexp(player, command->cmdlock, UB_DBREF));
    if (command->sw.mask) {
      bp = buff;
      for (sw_val = dyn_switch_list; sw_val->name; sw_val++)
        if (SW_ISSET(command->sw.mask, sw_val->value))
          strccat(buff, &bp, sw_val->name);
      *bp = '\0';
      notify_format(player, T("Switches   : %s"), buff);
    } else
      notify(player, T("Switches   :"));
    buff[0] = '\0';
    bp = buff;
    if (command->type & CMD_T_LS_ARGS) {
      if (command->type & CMD_T_LS_SPACE)
        strccat(buff, &bp, "Space-Args");
      else
        strccat(buff, &bp, "Args");
    }
    if (command->type & CMD_T_LS_NOPARSE)
      strccat(buff, &bp, "Noparse");
    if (command->type & CMD_T_EQSPLIT) {
      *bp = '\0';
      notify_format(player, T("Leftside   : %s"), buff);
      buff[0] = '\0';
      bp = buff;
      if (command->type & CMD_T_RS_ARGS) {
        if (command->type & CMD_T_RS_SPACE)
          strccat(buff, &bp, "Space-Args");
        else
          strccat(buff, &bp, "Args");
      }
      if (command->type & CMD_T_RS_NOPARSE)
        strccat(buff, &bp, "Noparse");
      *bp = '\0';
      notify_format(player, T("Rightside  : %s"), buff);
    } else {
      *bp = '\0';
      notify_format(player, T("Arguments  : %s"), buff);
    }
    do_hook_list(player, arg_left);
  }
}

/** Display a list of defined commands.
 * This function sends a player the list of commands.
 * \param player the enactor.
 * \param lc if true, list is in lowercase rather than uppercase.
 */
void
do_list_commands(dbref player, int lc)
{
  char *b = list_commands();
  notify_format(player, T("Commands: %s"), lc ? strlower(b) : b);
}

/** Return a list of defined commands.
 * This function returns a space-separated list of commands as a string.
 */
char *
list_commands(void)
{
  COMMAND_INFO *command;
  char *ptrs[BUFFER_LEN / 2];
  static char buff[BUFFER_LEN];
  char *bp;
  int nptrs = 0, i;
  command = (COMMAND_INFO *) ptab_firstentry(&ptab_command);
  while (command) {
    ptrs[nptrs] = (char *) command->name;
    nptrs++;
    command = (COMMAND_INFO *) ptab_nextentry(&ptab_command);
  }

  do_gensort(0, ptrs, NULL, nptrs, ALPHANUM_LIST);
  bp = buff;
  safe_str(ptrs[0], buff, &bp);
  for (i = 1; i < nptrs; i++) {
    if (gencomp((dbref) 0, ptrs[i], ptrs[i - 1], ALPHANUM_LIST) > 0) {
      safe_chr(' ', buff, &bp);
      safe_str(ptrs[i], buff, &bp);
    }
  }
  *bp = '\0';
  return buff;
}


/* Check command permissions. Return 1 if player can use command,
 * 0 otherwise, and maybe be noisy about it.
 */
int
command_check(dbref player, COMMAND_INFO *cmd, int noisy)
{

  /* If disabled, return silently */
  if (cmd->type & CMD_T_DISABLED)
    return 0;

  if (eval_boolexp(player, cmd->cmdlock, player)) {
    return 1;
  } else {
    if (noisy) {
      if (cmd->restrict_message)
        notify(player, cmd->restrict_message);
      else
        notify(player, T("Permission denied."));
    }
    return 0;
  }
}

/** Determine whether a player can use a command.
 * This function checks whether a player can use a command.
 * If the command is disallowed, the player is informed.
 * \param player player whose privileges are checked.
 * \param name name of command.
 * \retval 0 player may not use command.
 * \retval 1 player may use command.
 */
int
command_check_byname(dbref player, const char *name)
{
  COMMAND_INFO *cmd;
  cmd = command_find(name);
  if (!cmd)
    return 0;
  return command_check(player, cmd, 1);
}

/** Determine whether a player can use a command.
 * This function checks whether a player can use a command.
 * If the command is disallowed, the player is informed.
 * \param player player whose privileges are checked.
 * \param name name of command.
 * \retval 0 player may not use command.
 * \retval 1 player may use command.
 */
int
command_check_byname_quiet(dbref player, const char *name)
{
  COMMAND_INFO *cmd;
  cmd = command_find(name);
  if (!cmd)
    return 0;
  return command_check(player, cmd, 0);
}

static int
has_hook(struct hook_data *hook)
{
  if (!hook || !GoodObject(hook->obj) || IsGarbage(hook->obj))
    return 0;
  return 1;
}


/** Run a command hook.
 * This function runs a hook before or after a command execution.
 * \param player the enactor.
 * \param cause dbref that caused command to execute.
 * \param hook pointer to the hook.
 * \param saveregs array to store a copy of the final q-registers.
 * \param save if true, use saveregs to store a ending q-registers.
 * \retval 1 Hook doesn't exist, or evaluates to a non-false value
 * \retval 0 Hook exists and evaluates to a false value
 */
int
run_hook(dbref player, dbref cause, struct hook_data *hook, char *saveregs[],
         int save)
{
  ATTR *atr;
  char *code;
  const char *cp;
  char buff[BUFFER_LEN], *bp;
  char *origregs[NUMQ];

  if (!has_hook(hook))
    return 1;

  atr = atr_get(hook->obj, hook->attrname);

  if (!atr)
    return 1;

  code = safe_atr_value(atr);
  if (!code)
    return 1;
  add_check("hook.code");

  save_global_regs("run_hook", origregs);
  restore_global_regs("hook.regs", saveregs);

  cp = code;
  bp = buff;

  process_expression(buff, &bp, &cp, hook->obj, cause, player, PE_DEFAULT,
                     PT_DEFAULT, NULL);
  *bp = '\0';

  if (save)
    save_global_regs("hook.regs", saveregs);
  restore_global_regs("run_hook", origregs);

  mush_free(code, "hook.code");
  return parse_boolean(buff);
}

int
run_hook_override(COMMAND_INFO *cmd, dbref player, const char *commandraw)
{

  if (!has_hook(&cmd->hooks.override))
    return 0;

  if (cmd->hooks.override.attrname) {
    return one_comm_match(cmd->hooks.override.obj, player,
                          cmd->hooks.override.attrname, commandraw);
  } else {
    return atr_comm_match(cmd->hooks.override.obj, player, '$', ':', commandraw,
                          0, 1, NULL, NULL, NULL);
  }
}

/* Add/modify a hook from a cnf file */
int
cnf_hook_command(char *command, char *opts)
{
  dbref thing;
  char *attrname, *p, *one;
  enum hook_type flag;
  COMMAND_INFO *cmd;
  struct hook_data *h;

  if (!opts || !*opts)
    return 0;

  cmd = command_find(command);
  if (!cmd)
    return 0;

  p = trim_space_sep(opts, ' ');
  if (!(one = split_token(&p, ' ')))
    return 0;

  if (string_prefix("before", one)) {
    flag = HOOK_BEFORE;
    h = &cmd->hooks.before;
  } else if (string_prefix("after", one)) {
    flag = HOOK_AFTER;
    h = &cmd->hooks.after;
  } else if (string_prefix("override", one)) {
    flag = HOOK_OVERRIDE;
    h = &cmd->hooks.override;
  } else if (string_prefix("ignore", one)) {
    flag = HOOK_IGNORE;
    h = &cmd->hooks.ignore;
  } else {
    return 0;
  }

  if (!(one = split_token(&p, ' '))) {
    /* Clear existing hook */
    h->obj = NOTHING;
    if (h->attrname) {
      mush_free(h->attrname, "hook.attr");
      h->attrname = NULL;
    }
    return 1;
  }

  if ((attrname = strchr(one, '/')) == NULL) {
    if (flag != HOOK_OVERRIDE) {
      return 0;                 /* attribute required */
    }
  } else {
    *attrname++ = '\0';
    upcasestr(attrname);
  }

  if (!is_strict_integer(one))
    return 0;

  thing = (dbref) parse_integer(one);
  if (!GoodObject(thing) || IsGarbage(thing))
    return 0;

  if (attrname && !good_atr_name(attrname))
    return 0;

  h->obj = thing;
  if (h->attrname) {
    mush_free(h->attrname, "hook.attr");
  }

  if (attrname)
    h->attrname = mush_strdup(attrname, "hook.attr");
  else
    h->attrname = NULL;
  return 1;

}

/** Set up or remove a command hook.
 * \verbatim
 * This is the top-level function for @hook. If an object and attribute
 * are given, establishes a hook; if neither are given, removes a hook.
 * \endverbatim
 * \param player the enactor.
 * \param command command to hook.
 * \param obj name of object containing the hook attribute.
 * \param attrname of hook attribute on obj.
 * \param flag type of hook
 */
void
do_hook(dbref player, char *command, char *obj, char *attrname,
        enum hook_type flag)
{
  COMMAND_INFO *cmd;
  struct hook_data *h;

  cmd = command_find(command);
  if (!cmd) {
    notify(player, T("No such command."));
    return;
  }
  if ((cmd->func == cmd_password) || (cmd->func == cmd_newpassword)) {
    notify(player, T("Hooks not allowed with that command."));
    return;
  }

  if (flag == HOOK_BEFORE)
    h = &cmd->hooks.before;
  else if (flag == HOOK_AFTER)
    h = &cmd->hooks.after;
  else if (flag == HOOK_IGNORE)
    h = &cmd->hooks.ignore;
  else if (flag == HOOK_OVERRIDE)
    h = &cmd->hooks.override;
  else {
    notify(player, T("Unknown hook type"));
    return;
  }

  if (!obj && !attrname) {
    notify_format(player, T("Hook removed from %s."), cmd->name);
    h->obj = NOTHING;
    if (h->attrname) {
      mush_free(h->attrname, "hook.attr");
      h->attrname = NULL;
    }
  } else if (!obj || !*obj
             || (flag != HOOK_OVERRIDE && (!attrname || !*attrname))) {
    if (flag == HOOK_OVERRIDE) {
      notify(player, T("You must give an object."));
    } else {
      notify(player, T("You must give both an object and attribute."));
    }
  } else {
    dbref objdb = match_thing(player, obj);
    if (!GoodObject(objdb)) {
      notify(player, T("Invalid hook object."));
      return;
    }
    h->obj = objdb;
    if (h->attrname)
      mush_free(h->attrname, "hook.attr");
    if (!attrname || !*attrname) {
      h->attrname = NULL;
    } else {
      h->attrname = mush_strdup(strupper(attrname), "hook.attr");
    }
    notify_format(player, T("Hook set for %s"), cmd->name);
  }
}


/** List command hooks.
 * \verbatim
 * This is the top-level function for @hook/list, @list/hooks, and
 * the hook-listing part of @command.
 * \endverbatim
 * \param player the enactor.
 * \param command command to list hooks on.
 */
void
do_hook_list(dbref player, char *command)
{
  COMMAND_INFO *cmd;

  if (!command || !*command) {
    /* Show all commands with hooks */
    char *ptrs[BUFFER_LEN / 2];
    static char buff[BUFFER_LEN];
    char *bp;
    int i, count = 0;

    for (cmd = (COMMAND_INFO *) ptab_firstentry(&ptab_command); cmd;
         cmd = (COMMAND_INFO *) ptab_nextentry(&ptab_command)) {
      if (has_hook(&cmd->hooks.ignore) || has_hook(&cmd->hooks.override)
          || has_hook(&cmd->hooks.before) || has_hook(&cmd->hooks.after)) {
        ptrs[count] = (char *) cmd->name;
        count++;
      }
    }
    if (count == 0) {
      notify(player, T("There are no hooks currently set."));
      return;
    }
    do_gensort(0, ptrs, NULL, count, ALPHANUM_LIST);
    bp = buff;
    safe_str(T("The following commands have hooks: "), buff, &bp);
    for (i = 0; i < count; i++) {
      if (i > 0 && gencomp((dbref) 0, ptrs[i], ptrs[i - 1], ALPHANUM_LIST) <= 0)
        continue;
      if (i && i == (count - 1))
        safe_str(" and ", buff, &bp);
      else if (i)
        safe_str(", ", buff, &bp);
      safe_str(ptrs[i], buff, &bp);
    }
    *bp = '\0';
    notify(player, buff);
  } else {
    cmd = command_find(command);
    if (!cmd) {
      notify(player, T("No such command."));
      return;
    }
    if (Wizard(player) || has_power_by_name(player, "HOOK", NOTYPE)) {
      if (GoodObject(cmd->hooks.before.obj))
        notify_format(player, "@hook/before: #%d/%s",
                      cmd->hooks.before.obj, cmd->hooks.before.attrname);
      if (GoodObject(cmd->hooks.after.obj))
        notify_format(player, "@hook/after: #%d/%s", cmd->hooks.after.obj,
                      cmd->hooks.after.attrname);
      if (GoodObject(cmd->hooks.ignore.obj))
        notify_format(player, "@hook/ignore: #%d/%s",
                      cmd->hooks.ignore.obj, cmd->hooks.ignore.attrname);
      if (GoodObject(cmd->hooks.override.obj))
        notify_format(player, "@hook/override: #%d/%s",
                      cmd->hooks.override.obj, cmd->hooks.override.attrname);
    }
  }
}
