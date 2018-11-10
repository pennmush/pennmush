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
#include "command.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "access.h"
#include "attrib.h"
#include "cmds.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "extmail.h"
#include "flags.h"
#include "function.h"
#include "game.h"
#include "htab.h"
#include "log.h"
#include "match.h"
#include "memcheck.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "parse.h"
#include "ptab.h"
#include "sort.h"
#include "strtree.h"
#include "strutil.h"
#include "version.h"
#include "tests.h"

PTAB ptab_command;       /**< Prefix table for command names. */
PTAB ptab_command_perms; /**< Prefix table for command permissions */

HASHTAB htab_reserved_aliases; /**< Hash table for reserved command aliases */

static const char *command_isattr(char *command);
static int switch_find(COMMAND_INFO *cmd, const char *sw);
static void strccat(char *buff, char **bp, const char *from);
static COMMAND_INFO *clone_command(char *original, char *clone);
static int has_hook(struct hook_data *hook);
extern int global_fun_invocations; /**< Counter for function invocations */
extern int global_fun_recursions;  /**< Counter for function recursion */

SWITCH_VALUE *dyn_switch_list = NULL;
int switch_bytes = 0;
size_t num_switches = 0;

enum command_load_state { CMD_LOAD_BUILTIN, CMD_LOAD_LOCAL, CMD_LOAD_DONE };
static enum command_load_state command_state = CMD_LOAD_BUILTIN;
static StrTree switch_names;

int run_hook(dbref executor, dbref enactor, struct hook_data *hook,
             NEW_PE_INFO *pe_info);

int run_cmd_hook(struct hook_data *hook, dbref executor, const char *commandraw,
                 MQUE *from_queue, PE_REGS *pe_regs);
void do_command_clone(dbref player, char *original, char *clone);

static const char CommandLock[] = "CommandLock";

char *parse_chat_alias(dbref player, char *command); /* from extchat.c */

/** The list of standard commands. Additional commands can be added
 * at runtime with command_add().
 */
COMLIST commands[] = {

  {"@COMMAND",
   "ADD ALIAS CLONE DELETE EQSPLIT LSARGS RSARGS NOEVAL ON OFF "
   "QUIET ENABLE DISABLE RESTRICT NOPARSE RSNOPARSE",
   cmd_command, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@@", NULL, cmd_null, CMD_T_ANY | CMD_T_NOPARSE, 0, 0},
  {"@ALLHALT", NULL, cmd_allhalt, CMD_T_ANY, "WIZARD", "HALT"},
  {"@ALLQUOTA", "QUIET", cmd_allquota, CMD_T_ANY, "WIZARD", "QUOTA"},
  {"@ASSERT", "INLINE QUEUED", cmd_assert,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE | CMD_T_RS_BRACE, 0, 0},
  {"@ATRLOCK", NULL, cmd_atrlock, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@ATRCHOWN", NULL, cmd_atrchown, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},

  {"@ATTRIBUTE", "ACCESS DELETE RENAME RETROACTIVE LIMIT ENUM DECOMPILE",
   cmd_attribute, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@BOOT", "PORT ME SILENT", cmd_boot, CMD_T_ANY, 0, 0},
  {"@BREAK", "INLINE QUEUED", cmd_break,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE | CMD_T_RS_BRACE, 0, 0},
  {"@SKIP", "IFELSE", cmd_ifelse,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, 0, 0},
  {"@IFELSE", NULL, cmd_ifelse,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, 0, 0},
  {"@CEMIT", "NOEVAL NOISY SILENT SPOOF", cmd_cemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@CHANNEL",
   "LIST ADD DELETE RENAME MOGRIFIER NAME PRIVS QUIET DECOMPILE "
   "DESCRIBE CHOWN WIPE MUTE UNMUTE GAG UNGAG HIDE UNHIDE WHAT "
   "TITLE BRIEF RECALL BUFFER COMBINE UNCOMBINE ON JOIN OFF LEAVE "
   "WHO",
   cmd_channel, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_RS_ARGS, 0,
   0},
  {"@CHAT", NULL, cmd_chat, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@CHOWNALL", "PRESERVE THINGS ROOMS EXITS", cmd_chownall,
   CMD_T_ANY | CMD_T_EQSPLIT, "WIZARD", 0},

  {"@CHOWN", "PRESERVE", cmd_chown, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   0, 0},
  {"@CHZONEALL", "PRESERVE", cmd_chzoneall, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},

  {"@CHZONE", "PRESERVE", cmd_chzone,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@CONFIG", "SET SAVE LOWERCASE LIST", cmd_config, CMD_T_ANY | CMD_T_EQSPLIT,
   0, 0},
  {"@CPATTR", "CONVERT NOFLAGCOPY", cmd_cpattr,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@CREATE", NULL, cmd_create,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@CLONE", "PRESERVE", cmd_clone,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},

  {"@CLOCK", "JOIN SPEAK MOD SEE HIDE", cmd_clock, CMD_T_ANY | CMD_T_EQSPLIT, 0,
   0},
  {"@DBCK", NULL, cmd_dbck, CMD_T_ANY, "WIZARD", 0},

  {"@DECOMPILE", "DB NAME PREFIX TF FLAGS ATTRIBS SKIPDEFAULTS", cmd_decompile,
   CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@DESTROY", "OVERRIDE", cmd_destroy, CMD_T_ANY, 0, 0},

  {"@DIG", "TELEPORT", cmd_dig,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@DISABLE", NULL, cmd_disable, CMD_T_ANY, "WIZARD", 0},
  {"@DOLIST", "NOTIFY DELIMIT INPLACE INLINE LOCALIZE CLEARREGS NOBREAK",
   cmd_dolist, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE | CMD_T_RS_BRACE, 0,
   0},
  {"@DRAIN", "ALL ANY", cmd_notify_drain,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@DUMP", "PARANOID DEBUG NOFORK", cmd_dump, CMD_T_ANY, "WIZARD", 0},

  {"@EDIT", "FIRST CHECK QUIET REGEXP NOCASE ALL", cmd_edit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE |
     CMD_T_NOGAGGED,
   0, 0},
  {"@ELOCK", NULL, cmd_elock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_DEPRECATED, 0, 0},
  {"@EMIT", "NOEVAL SPOOF", cmd_emit, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@ENABLE", NULL, cmd_enable, CMD_T_ANY | CMD_T_NOGAGGED, "WIZARD", 0},

  {"@ENTRANCES", "EXITS THINGS PLAYERS ROOMS", cmd_entrances,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@EUNLOCK", NULL, cmd_eunlock, CMD_T_ANY | CMD_T_NOGAGGED | CMD_T_DEPRECATED,
   0, 0},
  {"@FIND", NULL, cmd_find,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@FIRSTEXIT", NULL, cmd_firstexit, CMD_T_ANY | CMD_T_ARGS, 0, 0},
  {"@FLAG",
   "ADD TYPE LETTER LIST RESTRICT DELETE ALIAS DISABLE ENABLE DEBUG DECOMPILE",
   cmd_flag, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},

  {"@FORCE", "NOEVAL INPLACE INLINE LOCALIZE CLEARREGS NOBREAK", cmd_force,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_RS_BRACE, 0, 0},
  {"@FUNCTION",
   "ALIAS BUILTIN CLONE DELETE ENABLE DISABLE PRESERVE RESTORE RESTRICT",
   cmd_function, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0,
   0},
  {"@GREP", "LIST PRINT ILIST IPRINT REGEXP WILD NOCASE PARENT", cmd_grep,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE | CMD_T_NOGAGGED, 0, 0},
  {"@HALT", "ALL NOEVAL PID", cmd_halt,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_BRACE, 0, 0},
  {"@HIDE", "NO OFF YES ON", cmd_hide, CMD_T_ANY, 0, 0},
  {"@HOOK",
   "LIST AFTER BEFORE EXTEND IGSWITCH IGNORE OVERRIDE INPLACE INLINE "
   "LOCALIZE CLEARREGS NOBREAK",
   cmd_hook, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, "WIZARD", "hook"},
  {"@HTTP", "DELETE POST PUT", cmd_fetch,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED | CMD_T_NOGUEST,
   0, 0},
  {"@INCLUDE", "LOCALIZE CLEARREGS NOBREAK", cmd_include,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@KICK", NULL, cmd_kick, CMD_T_ANY, "WIZARD", 0},

  {"@LEMIT", "NOEVAL NOISY SILENT SPOOF", cmd_lemit, CMD_T_ANY | CMD_T_NOGAGGED,
   0, 0},
  {"@LINK", "PRESERVE", cmd_link, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0,
   0},
  {"@LISTMOTD", NULL, cmd_motd, CMD_T_ANY, 0, 0},

  {"@LIST",
   "LOWERCASE MOTD LOCKS FLAGS FUNCTIONS POWERS COMMANDS ATTRIBS "
   "ALLOCATIONS ALL BUILTIN LOCAL",
   cmd_list, CMD_T_ANY, 0, 0},
  {"@LOCK", NULL, cmd_lock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_SWITCHES | CMD_T_NOGAGGED, 0, 0},
  {"@LOG", "CHECK CMD CONN ERR TRACE WIZ RECALL", cmd_log,
   CMD_T_ANY | CMD_T_NOGAGGED, "WIZARD", 0},
  {"@LOGWIPE", "CHECK CMD CONN ERR TRACE WIZ ROTATE TRIM WIPE", cmd_logwipe,
   CMD_T_ANY | CMD_T_NOGAGGED | CMD_T_GOD, 0, 0},
  {"@LSET", NULL, cmd_lset, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@MAIL",
   "NOEVAL NOSIG STATS CSTATS DSTATS FSTATS DEBUG NUKE FOLDERS "
   "UNFOLDER LIST READ UNREAD CLEAR UNCLEAR STATUS PURGE FILE TAG "
   "UNTAG FWD FORWARD SEND SILENT URGENT REVIEW RETRACT",
   cmd_mail, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@MALIAS",
   "SET CREATE DESTROY DESCRIBE RENAME STATS CHOWN NUKE ADD REMOVE "
   "LIST ALL WHO MEMBERS USEFLAG SEEFLAG",
   cmd_malias, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@MAPSQL", "NOTIFY COLNAMES SPOOF", cmd_mapsql, CMD_T_ANY | CMD_T_EQSPLIT, 0,
   0},
  {"@MESSAGE", "NOEVAL SPOOF NOSPOOF REMIT OEMIT SILENT NOISY", cmd_message,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@MONIKER", NULL, cmd_moniker, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@MOTD", "CONNECT LIST WIZARD DOWN FULL CLEAR", cmd_motd,
   CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@MVATTR", "CONVERT NOFLAGCOPY", cmd_mvattr,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@NAME", NULL, cmd_name,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_NOGUEST, 0, 0},
  {"@NEWPASSWORD", "GENERATE", cmd_newpassword,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE, "WIZARD", 0},
  {"@NOTIFY", "ALL ANY SETQ", cmd_notify_drain,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@NSCEMIT", "NOEVAL NOISY SILENT", cmd_cemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NSEMIT", "ROOM NOEVAL SILENT", cmd_emit, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@NSLEMIT", "NOEVAL NOISY SILENT", cmd_lemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NSOEMIT", "NOEVAL", cmd_oemit, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED,
   0, 0},
  {"@NSPEMIT", "LIST SILENT NOISY NOEVAL", cmd_pemit, CMD_T_ANY | CMD_T_EQSPLIT,
   0, 0},
  {"@NSPROMPT", "SILENT NOISY NOEVAL", cmd_prompt, CMD_T_ANY | CMD_T_EQSPLIT, 0,
   0},
  {"@NSREMIT", "LIST NOEVAL NOISY SILENT", cmd_remit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NSZEMIT", "NOISY SILENT", cmd_zemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@NUKE", NULL, cmd_nuke, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"@OEMIT", "NOEVAL SPOOF", cmd_oemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@OPEN", NULL, cmd_open,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0, 0},
  {"@PARENT", NULL, cmd_parent, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@PASSWORD", NULL, cmd_password,
   CMD_T_PLAYER | CMD_T_EQSPLIT | CMD_T_NOPARSE | CMD_T_RS_NOPARSE |
     CMD_T_NOGUEST,
   0, 0},
  {"@PCREATE", NULL, cmd_pcreate, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0,
   0},

  {"@PEMIT", "LIST CONTENTS SILENT NOISY NOEVAL PORT SPOOF", cmd_pemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@POLL", "CLEAR", cmd_poll, CMD_T_ANY, 0, 0},
  {"@POOR", NULL, cmd_poor, CMD_T_ANY, 0, 0},
  {"@POWER",
   "ADD TYPE LETTER LIST RESTRICT DELETE ALIAS DISABLE ENABLE DECOMPILE",
   cmd_power, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@PROMPT", "SILENT NOISY NOEVAL SPOOF", cmd_prompt,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@PS", "ALL SUMMARY COUNT QUICK DEBUG", cmd_ps, CMD_T_ANY, 0, 0},
  {"@PURGE", NULL, cmd_purge, CMD_T_ANY, 0, 0},
  {"@QUOTA", "ALL SET", cmd_quota, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@READCACHE", NULL, cmd_readcache, CMD_T_ANY, "WIZARD", 0},
  {"@RECYCLE", "OVERRIDE", cmd_destroy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"@REMIT", "LIST NOEVAL NOISY SILENT SPOOF", cmd_remit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@REJECTMOTD", "CLEAR", cmd_motd, CMD_T_ANY, "WIZARD", 0},
  {"@RESPOND", "HEADER TYPE", cmd_respond,
   CMD_T_ANY | CMD_T_NOGAGGED | CMD_T_EQSPLIT, 0, 0},
  {"@RESTART", "ALL", cmd_restart, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@RETRY", NULL, cmd_retry,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE |
     CMD_T_NOGAGGED,
   0, 0},
  {"@RWALL", "NOEVAL EMIT", cmd_rwall, CMD_T_ANY, "WIZARD ROYALTY", 0},
  {"@SCAN", "ROOM SELF ZONE GLOBALS", cmd_scan, CMD_T_ANY | CMD_T_NOGAGGED, 0,
   0},
  {"@SEARCH", NULL, cmd_search,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, 0, 0},
  {"@SELECT", "NOTIFY REGEXP INPLACE INLINE LOCALIZE CLEARREGS NOBREAK",
   cmd_select, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE, 0,
   0},
  {"@SET", NULL, cmd_set, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@SOCKSET", NULL, cmd_sockset,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_RS_ARGS, 0, 0},
  {"@SHUTDOWN", "PANIC REBOOT PARANOID", cmd_shutdown, CMD_T_ANY, "WIZARD", 0},
  {"@SLAVE", "RESTART", cmd_slave, CMD_T_ANY, "WIZARD", 0},
  {"@SQL", NULL, cmd_sql, CMD_T_ANY, "WIZARD", "SQL_OK"},
  {"@SITELOCK", "BAN CHECK REGISTER REMOVE NAME PLAYER", cmd_sitelock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, "WIZARD", 0},
  {"@STATS", "CHUNKS FREESPACE PAGING REGIONS TABLES FLAGS", cmd_stats,
   CMD_T_ANY, 0, 0},
  {"@SUGGEST", "ADD DELETE LIST", cmd_suggest, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@SWEEP", "CONNECTED HERE INVENTORY EXITS", cmd_sweep, CMD_T_ANY, 0, 0},
  {"@SWITCH",
   "NOTIFY FIRST ALL REGEXP INPLACE INLINE LOCALIZE CLEARREGS NOBREAK",
   cmd_switch,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_RS_NOPARSE |
     CMD_T_NOGAGGED,
   0, 0},
  {"@SQUOTA", NULL, cmd_squota, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},

  {"@TELEPORT", "SILENT INSIDE LIST", cmd_teleport,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"@TRIGGER", "CLEARREGS SPOOF INLINE NOBREAK LOCALIZE INPLACE MATCH",
   cmd_trigger, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS | CMD_T_NOGAGGED, 0,
   0},
  {"@ULOCK", NULL, cmd_ulock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_DEPRECATED, 0, 0},
  {"@UNDESTROY", NULL, cmd_undestroy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@UNLINK", NULL, cmd_unlink, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"@UNLOCK", NULL, cmd_unlock,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_SWITCHES | CMD_T_NOGAGGED, 0, 0},
  {"@UNRECYCLE", NULL, cmd_undestroy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@UPTIME", "MORTAL", cmd_uptime, CMD_T_ANY, 0, 0},
  {"@UUNLOCK", NULL, cmd_uunlock, CMD_T_ANY | CMD_T_NOGAGGED | CMD_T_DEPRECATED,
   0, 0},
  {"@VERB", NULL, cmd_verb, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_ARGS, 0, 0},
  {"@VERSION", NULL, cmd_version, CMD_T_ANY, 0, 0},
  {"@WAIT", "PID UNTIL", cmd_wait,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_RS_NOPARSE | CMD_T_RS_BRACE, 0, 0},
  {"@WALL", "NOEVAL EMIT", cmd_wall, CMD_T_ANY, "WIZARD ROYALTY", "ANNOUNCE"},

  {"@WARNINGS", NULL, cmd_warnings, CMD_T_ANY | CMD_T_EQSPLIT, 0, 0},
  {"@WCHECK", "ALL ME", cmd_wcheck, CMD_T_ANY, 0, 0},
  {"@WHEREIS", NULL, cmd_whereis, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@WIPE", NULL, cmd_wipe, CMD_T_ANY, 0, 0},
  {"@WIZWALL", "NOEVAL EMIT", cmd_wizwall, CMD_T_ANY, "WIZARD", 0},
  {"@WIZMOTD", "CLEAR", cmd_motd, CMD_T_ANY, "WIZARD", 0},
  {"@ZEMIT", "NOISY SILENT", cmd_zemit,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},

  {"BUY", NULL, cmd_buy, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"BRIEF", "OPAQUE", cmd_brief, CMD_T_ANY, 0, 0},
  {"DESERT", NULL, cmd_desert, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"DISMISS", NULL, cmd_dismiss, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"DROP", NULL, cmd_drop, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"EXAMINE", "ALL BRIEF DEBUG MORTAL OPAQUE PARENT", cmd_examine, CMD_T_ANY, 0,
   0},
  {"EMPTY", NULL, cmd_empty, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, 0, 0},
  {"ENTER", NULL, cmd_enter, CMD_T_ANY, 0, 0},

  {"FOLLOW", NULL, cmd_follow, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, 0,
   0},

  {"GET", NULL, cmd_get, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED, 0, 0},
  {"GIVE", "SILENT", cmd_give, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0,
   0},
  {"GOTO", NULL, cmd_goto, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"HOME", NULL, cmd_home, CMD_T_PLAYER | CMD_T_THING, 0, 0},
  {"INVENTORY", NULL, cmd_inventory, CMD_T_ANY, 0, 0},

  {"LOOK", "OUTSIDE OPAQUE", cmd_look, CMD_T_ANY, 0, 0},
  {"LEAVE", NULL, cmd_leave, CMD_T_PLAYER | CMD_T_THING, 0, 0},

  {"PAGE", "LIST NOEVAL PORT OVERRIDE", cmd_page,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"POSE", "NOEVAL NOSPACE", cmd_pose, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"SCORE", NULL, cmd_score, CMD_T_ANY, 0, 0},
  {"SAY", "NOEVAL", cmd_say, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"SEMIPOSE", "NOEVAL", cmd_semipose, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"TEACH", "LIST", cmd_teach, CMD_T_ANY | CMD_T_NOPARSE, 0, 0},
  {"THINK", "NOEVAL", cmd_think, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"UNFOLLOW", NULL, cmd_unfollow, CMD_T_PLAYER | CMD_T_THING | CMD_T_NOGAGGED,
   0, 0},
  {"USE", NULL, cmd_use, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {"WHISPER", "LIST NOISY SILENT NOEVAL", cmd_whisper,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"WITH", "NOEVAL ROOM", cmd_with, CMD_T_PLAYER | CMD_T_THING | CMD_T_EQSPLIT,
   0, 0},

  {"WHO", NULL, cmd_who, CMD_T_ANY, 0, 0},
  {"DOING", NULL, cmd_who_doing, CMD_T_ANY, 0, 0},
  {"SESSION", NULL, cmd_session, CMD_T_ANY, 0, 0},

  /* ATTRIB_SET is an undocumented command - it's sugar to make it possible
   * to enable/disable attribute setting with &XX or @XX
   */
  {"ATTRIB_SET", NULL, command_atrset,
   CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED | CMD_T_INTERNAL, 0, 0},

  /* A way to stop people starting commands with functions */
  {"WARN_ON_MISSING", NULL, cmd_warn_on_missing,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL | CMD_T_NOP, 0, 0},

  /* A way to let people override the Huh? message */
  {"HUH_COMMAND", NULL, cmd_huh_command,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL | CMD_T_NOP, 0, 0},

  /* A way to let people override the unimplemented message */
  {"UNIMPLEMENTED_COMMAND", NULL, cmd_unimplemented,
   CMD_T_ANY | CMD_T_NOPARSE | CMD_T_INTERNAL | CMD_T_NOP, 0, 0},

  {"ADDCOM", NULL, cmd_addcom, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"DELCOM", NULL, cmd_delcom, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"@CLIST", "FULL", cmd_clist, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},
  {"COMTITLE", NULL, cmd_comtitle, CMD_T_ANY | CMD_T_EQSPLIT | CMD_T_NOGAGGED, 0, 0},
  {"COMLIST", NULL, cmd_comlist, CMD_T_ANY | CMD_T_NOGAGGED, 0, 0},

  {NULL, NULL, NULL, 0, 0, 0}};

/* switch_list is defined in switchinc.c */
#include "switchinc.c"

/** Table of command permissions/restrictions. */
struct command_perms_t command_perms[] = {{"player", CMD_T_PLAYER},
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
                                          {NULL, 0}};

static void
strccat(char *buff, char **bp, const char *from)
{
  if (*buff)
    safe_str(", ", buff, bp);
  safe_str(from, buff, bp);
}

TEST_GROUP(strccat) {
  char buff[BUFFER_LEN];
  char *bp = buff;
  *bp = '\0';
  strccat(buff, &bp, "foo");
  *bp = '\0';
  TEST("strccat.1", strcmp(buff, "foo") == 0);
  strccat(buff, &bp, "bar");
  *bp = '\0';
  TEST("strccat.2", strcmp(buff, "foo, bar") == 0);
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
      if (SW_ISSET(cmd->sw.mask, sw_val->value) &&
          (strncmp(sw_val->name, sw, len) == 0))
        return sw_val->value;
      sw_val++;
    }
  }
  return 0;
}

TEST_GROUP(switch_find) {
  TEST("switch_find.1", switch_find(NULL, "LIST") > 0);
  TEST("switch_find.2", switch_find(NULL, "NOTASWITCHEVERTHISMEEANSYOU") == 0);
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

TEST_GROUP(SW_BY_NAME) {
  // TEST SW_BY_NAME REQUIRES switch_find switchmask
  switch_mask mask = switchmask("NOEVAL LIST");
  TEST("SW_BY_NAME.1", SW_BY_NAME(mask, "LIST"));
  TEST("SW_BY_NAME.2", SW_BY_NAME(mask, "NOTASWITCHEVERTHISMEANSYOU") == false);
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
make_command(const char *name, int type, const char *flagstr,
             const char *powerstr, const char *sw, command_func func)
{
  COMMAND_INFO *cmd;
  cmd = mush_malloc_zero(sizeof *cmd, "command");
  cmd->name = name;
  cmd->cmdlock = TRUE_BOOLEXP;
  cmd->restrict_message = NULL;
  cmd->func = func;
  cmd->type = type;
  switch (command_state) {
  case CMD_LOAD_BUILTIN:
    cmd->sw.names = sw;
    break;
  case CMD_LOAD_LOCAL: {
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
  case CMD_LOAD_DONE: {
    switch_mask mask = switchmask(sw);
    if (mask) {
      cmd->sw.mask = SW_ALLOC();
      SW_COPY(cmd->sw.mask, mask);
    } else
      cmd->sw.mask = NULL;
  }
  }
  cmd->hooks.before = NULL;
  cmd->hooks.after = NULL;
  cmd->hooks.ignore = NULL;
  cmd->hooks.override = NULL;
  cmd->hooks.extend = NULL;
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
      if (strcasecmp("noparse", one) == 0) {
        flags |= CMD_T_NOPARSE;
      } else if (strcasecmp("rsargs", one) == 0) {
        flags |= CMD_T_RS_ARGS;
      } else if (strcasecmp("lsargs", one) == 0) {
        flags |= CMD_T_LS_ARGS;
      } else if (strcasecmp("eqsplit", one) == 0) {
        flags |= CMD_T_EQSPLIT;
      } else if (strcasecmp("rsnoparse", one) == 0) {
        flags |= CMD_T_RS_NOPARSE;
      } else {
        return 0; /* unknown option */
      }
    }
  }

  name = trim_space_sep(name, ' ');
  upcasestr(name);
  command = command_find(name);
  if (command || !ok_command_name(name))
    return 0;
  command_add(mush_strdup(name, "command.name"), flags, NULL, 0,
              (flags & CMD_T_NOPARSE ? NULL : "NOEVAL"), cmd_unimplemented);
  return command_find(name) != NULL;
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
  strupper_r(name, cmdname, sizeof cmdname);
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
  strupper_r(name, cmdname, sizeof cmdname);
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

  if (!sw || sm_bytes < switch_bytes) {
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
    else {
      if (switchnum <= max_switch)
        switch_list[switchnum - 1].used = 1;
      SW_SET(sw, switchnum);
    }
  }
  return sw;
}

TEST_GROUP(switchmask) {
  // TEST switchmask REQUIRES switch_find split_token
  switch_mask mask = switchmask("NOEVAL LIST");
  TEST("switchmask.1", mask != NULL);
  TEST("switchmask.2", mask && SW_ISSET(mask, SWITCH_LIST));
  TEST("switchmask.3", mask && SW_ISSET(mask, SWITCH_SPOOF) == 0);
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
  static const char placeholder[] = "x";
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
  st_init(&switch_names, "SwitchNameTree");
  for (sv = switch_list; sv->name; sv++)
    st_insert(sv->name, &switch_names);

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
                make_command(cmd->name, cmd->type, cmd->flagstr, cmd->powers,
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
build_switch_table(const char *sw, int count __attribute__((__unused__)),
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
 */
void
command_init_postconfig(void)
{
  struct bst_data sw_data;
  COMMAND_INFO *c;
  SWITCH_VALUE *s;

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
  num_switches =
    sw_data.start - 1; /* Don't count the trailing NULL-name switch */
  dyn_switch_list[sw_data.n].name = NULL;
  st_flush(&switch_names);
  switch_bytes = ceil((double) (num_switches + 1) / 8.0);

  /* Then convert the list of switch names in all commands to masks */
  for (c = ptab_firstentry(&ptab_command); c;
       c = ptab_nextentry(&ptab_command)) {
    const char *switchstr = c->sw.names;
    if (switchstr) {
      c->sw.mask = SW_ALLOC();
      SW_COPY(c->sw.mask, switchmask(switchstr));
    }
  }

  /* Warn about unused switch names */
  for (s = switch_list; s->name; s++) {
    if (!s->used)
      do_rawlog(LT_CMD, "Warning: Switch '%s' is defined but not used.",
                s->name);
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
 * \param executor the executor.
 * \param enactor the enactor
 * \param caller the caller
 * \param pe_info the pe_info to use to evaluate the args
 * \param from pointer to address of where to parse arguments from.
 * \param to string to store parsed arguments into.
 * \param argv array of parsed arguments.
 * \param cmd pointer to command data.
 * \param right_side if true, parse on the right of the =. Otherwise, left.
 * \param forcenoparse if true, do no evaluation during parsing.
 * \param pe_flags default pe_flags, used for debug/no_debug action lists
 */
void
command_argparse(dbref executor, dbref enactor, dbref caller,
                 NEW_PE_INFO *pe_info, char **from, char *to, char *argv[],
                 COMMAND_INFO *cmd, int right_side, int forcenoparse,
                 int pe_flags)
{
  int parse, split, args, i, done;
  char *t, *f;
  char *aold;

  f = *from;

  parse =
    (right_side) ? (cmd->type & CMD_T_RS_NOPARSE) : (cmd->type & CMD_T_NOPARSE);
  if (parse || forcenoparse) {
    if (right_side && (cmd->type & CMD_T_RS_BRACE)) {
      parse = PE_COMMAND_BRACES;
    } else {
      parse = PE_NOTHING;
    }
  } else {
    parse = PE_DEFAULT | PE_COMMAND_BRACES | pe_flags;
  }

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
    if (process_expression(to, &t, (const char **) &f, executor, caller,
                           enactor, parse, (split | args), pe_info)) {
      done = 1;
    }
    /* If t is pointing at or past the last element, this is the last arg. */
    if ((t - to) >= BUFFER_LEN - 1) {
      t = to + BUFFER_LEN - 1;
      done = 1;
    }
    *(t++) = '\0';
    if (args) {
      argv[i] = aold;
      if (*f)
        f++;
      i++;
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
#define command_parse_free_args                                                \
  mush_free(command, "string_command");                                        \
  mush_free(swtch, "string_swtch");                                            \
  mush_free(ls, "string_ls");                                                  \
  mush_free(rs, "string_rs");                                                  \
  mush_free(switches, "string_switches");                                      \
  if (sw)                                                                      \
  SW_FREE(sw)

/** Parse commands.
 * Parse the commands. This is the big one!
 * We distinguish parsing of input sent from a player at a socket
 * (in which case attribute values to set are not evaluated) and
 * input sent in any other way (in which case attribute values to set
 * are evaluated, and attributes are set NO_COMMAND).
 * Return NULL if the command was recognized and handled, the evaluated
 * text to match against $-commands otherwise.
 * \param player the enactor.
 * \param string the input to be parsed.
 * \param queue_entry the queue_entry the command is a part of
 * \return NULL if a command was handled, otherwise the evaluated input.
 */
char *
command_parse(dbref player, char *string, MQUE *queue_entry)
{
  char *command, *swtch, *ls, *rs, *switches;
  static char commandraw[BUFFER_LEN];
  static char exit_command[BUFFER_LEN], *ec;
  /* TODO: Fix this as some commands actually modify lsp/rsp so we can't just
   * make empty const, but if the value of empty were to ever change, things
   * would probably break... */
  static char *empty = "";
  char *lsa[MAX_ARG] = {NULL};
  char *rsa[MAX_ARG] = {NULL};
  char *lsp = empty;
  char *rsp = empty;
  char *ap, *swp;
  const char *attrib, *replacer;
  COMMAND_INFO *cmd;
  char *p, *t, *c, *c2;
  char command2[BUFFER_LEN];
  char b;
  bool parse_switches = 1;
  int switchnum;
  switch_mask sw = NULL;
  char switch_err[BUFFER_LEN], *se;
  int noeval;
  int noevtoken = 0;
  char *retval;
  NEW_PE_INFO *pe_info = queue_entry->pe_info;
  int pe_flags = 0;
  int skip_char = 1;
  bool is_chat = 0;

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
    memmove(pe_info->cmd_raw, (char *) pe_info->cmd_raw + 1,
            strlen(pe_info->cmd_raw));
  }

  if (*p == '[') {
    if ((cmd = command_find("WARN_ON_MISSING")) &&
        !(cmd->type & CMD_T_DISABLED)) {
      run_command(cmd, player, queue_entry->enactor, "WARN_ON_MISSING", NULL,
                  NULL, string, NULL, string, string, NULL, NULL, NULL,
                  queue_entry);
      command_parse_free_args;
      return NULL;
    }
  }

  if (queue_entry->queue_type & QUEUE_DEBUG_PRIVS)
    pe_flags = PE_DEBUG;
  else if (queue_entry->queue_type & QUEUE_NODEBUG)
    pe_flags = PE_NODEBUG;
  else if (queue_entry->queue_type & QUEUE_DEBUG)
    pe_flags = PE_DEBUG;

  if (*p == CHAT_TOKEN || (CHAT_TOKEN_ALIAS && *p == CHAT_TOKEN_ALIAS)) {
    /* parse_chat() destructively modifies the command to replace
     * the first space with a '=' if the command is an actual
     * chat command */
    if (parse_chat(player, p + 1) &&
        command_check_byname(player, "@CHAT", queue_entry->pe_info)) {
      *p = CHAT_TOKEN;
      is_chat = 1;
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
    /* This is a "+chan foo" chat style
     * We set noevtoken to keep its noeval way, and
     * set the cmd to allow @hook. */
    if (is_chat) {
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
    skip_char = 0;
  }

  if (replacer)
    parse_switches = 0;

  if (USE_MUXCOMM) {
    if (!replacer && (replacer = parse_chat_alias(player, p)) &&
        command_check_byname(player, replacer, pe_info)) {
      noevtoken = 1;
      skip_char = 0;
      if (!strcmp(replacer, "@CHAT")) {
        /* Don't parse switches for @chat. Do for @channel. */
        parse_switches = 0;
      }
    }
  }

  if (replacer) {
    cmd = command_find(replacer);
    if (skip_char)
      p++;
    strcpy(command, p);
    if (parse_switches && *p == '/') {
      while (*p && *p != ' ') {
        p++;
      }
      while (*p == ' ')
        p++;
    }
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
      noevtoken = 1; /* But don't parse the exit name! */
    }
    c = command;
    while (*p == ' ')
      p++;
    process_expression(command, &c, (const char **) &p, player,
                       queue_entry->caller, queue_entry->enactor,
                       noevtoken ? PE_NOTHING
                                 : ((PE_DEFAULT & ~PE_FUNCTION_CHECK) |
                                    pe_flags | PE_COMMAND_BRACES),
                       PT_SPACE, pe_info);
    *c = '\0';
    mush_strncpy(commandraw, command, sizeof commandraw);
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
      process_expression(commandraw, &c2, (const char **) &p, player,
                         queue_entry->caller, queue_entry->enactor,
                         noevtoken ? PE_NOTHING
                                   : ((PE_DEFAULT & ~PE_FUNCTION_CHECK) |
                                      pe_flags | PE_COMMAND_BRACES),
                         PT_DEFAULT, pe_info);
    }
    *c2 = '\0';
    command_parse_free_args;
    return commandraw;
  } else if (!command_check_with(player, cmd, 1, queue_entry->pe_info)) {
    /* No permission to use command, stop processing */
    command_parse_free_args;
    return NULL;
  }

  /* Set up commandraw for future use. This will contain the canonicalization
   * of the command name and will later have the parsed rest of the input
   * appended at the position pointed to by c2.
   */
  c2 = c;
  if (parse_switches && *c2 == '/') {
    /* Oh... DAMN */
    c2 = commandraw;
    strcpy(switches, commandraw);
    safe_str(cmd->name, commandraw, &c2);
    t = strchr(switches, '/');
    safe_str(t, commandraw, &c2);
  } else {
    c2 = commandraw;
    safe_str(cmd->name, commandraw, &c2);
    if (replacer)
      safe_chr(' ', commandraw, &c2);
  }

  /* Parse out any switches */
  sw = SW_ALLOC();
  swp = switches;
  *swp = '\0';
  se = switch_err;

  t = NULL;

  /* Don't parse switches for one-char commands */
  if (parse_switches) {
    while (*c == '/') {
      char tmp[BUFFER_LEN];
      t = swtch;
      c++;
      while ((*c) && (*c != ' ') && (*c != '/')) {
        *t++ = *c++;
      }
      *t = '\0';
      switchnum = switch_find(cmd, strupper_r(swtch, tmp, sizeof tmp));
      if (!switchnum) {
        if (cmd->type & CMD_T_SWITCHES) {
          if (*swp) {
            strcat(swp, " ");
          }
          strcat(swp, swtch);
        } else {
          if (se == switch_err) {
            safe_format(switch_err, &se, T("%s doesn't know switch %s."),
                        cmd->name, swtch);
          }
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

  if ((cmd->func == command_atrset) &&
      (queue_entry->queue_type & QUEUE_NOLIST)) {
    /* Special case: eqsplit, noeval of rhs only */
    command_argparse(player, queue_entry->enactor, queue_entry->caller, pe_info,
                     &p, ls, lsa, cmd, 0, 0, pe_flags);
    command_argparse(player, queue_entry->enactor, queue_entry->caller, pe_info,
                     &p, rs, rsa, cmd, 1, 1, pe_flags);
    SW_SET(sw, SWITCH_NOEVAL); /* Needed for ATTRIB_SET */
  } else {
    noeval = SW_ISSET(sw, SWITCH_NOEVAL) || noevtoken;
    if (cmd->type & CMD_T_EQSPLIT) {
      char *savep = p;
      command_argparse(player, queue_entry->enactor, queue_entry->caller,
                       pe_info, &p, ls, lsa, cmd, 0, noeval, pe_flags);
      if (noeval && !noevtoken && *p) {
        /* oops, we have a right hand side, should have evaluated */
        p = savep;
        command_argparse(player, queue_entry->enactor, queue_entry->caller,
                         pe_info, &p, ls, lsa, cmd, 0, 0, pe_flags);
      }
      command_argparse(player, queue_entry->enactor, queue_entry->caller,
                       pe_info, &p, rs, rsa, cmd, 1, noeval, pe_flags);
    } else {
      command_argparse(player, queue_entry->enactor, queue_entry->caller,
                       pe_info, &p, ls, lsa, cmd, 0, noeval, pe_flags);
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
      for (lsa_index = 2; (lsa_index < MAX_ARG) && lsa[lsa_index];
           lsa_index++) {
        safe_chr(',', commandraw, &c2);
        safe_str(lsa[lsa_index], commandraw, &c2);
      }
    }
  } else {
    lsp = ls;
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
        rsp = rs;
        safe_str(rs, commandraw, &c2);
      }
    }
  }
  *c2 = '\0';
  if (queue_entry->pe_info->cmd_evaled) {
    mush_free(queue_entry->pe_info->cmd_evaled, "string");
  }
  queue_entry->pe_info->cmd_evaled = mush_strdup(commandraw, "string");

  retval = NULL;
  if (cmd->func == NULL) {
    do_rawlog(LT_ERR, "No command vector on command %s.", cmd->name);
    command_parse_free_args;
    return NULL;
  } else {
    /* If we have a hook/ignore that returns false, we don't do the command */
    if (run_command(cmd, player, queue_entry->enactor, commandraw, sw,
                    switch_err, string, swp, ap, lsp, lsa, rsp, rsa,
                    queue_entry)) {
      retval = NULL;
    } else {
      retval = commandraw;
    }
  }

  command_parse_free_args;
  return retval;
}

#undef command_parse_free_args

/** Run a built-in command, with associated hooks
 * \param cmd the command to run
 * \param executor Dbref of object running command
 * \param enactor Dbref of object which caused command to run
 * \param cmd_evaled The evaluated command, for %u
 * \param sw Switch mask
 * \param switch_err Error message if invalid switches were used, or NULL/'\0'
 * if not
 * \param cmd_raw The unevaluated command, for %c
 * \param swp Switches, as a char array (used for CMD_T_SWITCHES commands)
 * \param ap Entire arg string (lhs, =, rhs)
 * \param ls The leftside arg, if the command has a single left arg
 * \param lsa Array of leftside args, if CMD_T_LS_ARGS
 * \param rs The rightside arg, if the command has a single rhs arg
 * \param rsa Array of rhs args, if CMD_T_RS_ARGS
 * \param queue_entry The queue entry the command is being run in
 * \retval 1 command has been successfully handled
 * \retval 0 command hasn't been run (due to \@hook/ignore)
 */
int
run_command(COMMAND_INFO *cmd, dbref executor, dbref enactor,
            const char *cmd_evaled, switch_mask sw, char switch_err[BUFFER_LEN],
            const char *cmd_raw, char *swp, char *ap, char *ls,
            char *lsa[MAX_ARG], char *rs, char *rsa[MAX_ARG], MQUE *queue_entry)
{
  NEW_PE_INFO *pe_info;
  char nop_arg[BUFFER_LEN];
  int i, j;

  if (!cmd)
    return 0;

  if (cmd->type & CMD_T_DEPRECATED) {
    notify_format(Owner(executor),
                  T("Deprecated command %s being used on object #%d."),
                  cmd->name, executor);
  }

  /* Create a pe_info for the hooks, which share q-registers */
  pe_info = make_pe_info("pe_info-run_command");
  pe_info->cmd_evaled = mush_strdup(cmd_evaled, "string");
  pe_info->cmd_raw = mush_strdup(cmd_raw, "string");

  /* Set each command arg into a named stack variable, for use in hooks */
  if (ap && *ap)
    pe_regs_set(pe_info->regvals, PE_REGS_ARG | PE_REGS_NOCOPY, "args", ap);

  if (swp && *swp)
    pe_regs_set(pe_info->regvals, PE_REGS_ARG | PE_REGS_NOCOPY, "switches",
                swp);

  /* ls, before the = */
  if (cmd->type & CMD_T_LS_ARGS) {
    char argname[20];
    j = 0;
    for (i = 1; i < MAX_ARG; i++) {
      if (lsa[i] && *lsa[i]) {
        snprintf(argname, sizeof argname, "lsa%d", i);
        pe_regs_set(pe_info->regvals, PE_REGS_ARG | PE_REGS_NOCOPY, argname,
                    lsa[i]);
        j = i;
      }
    }
    if (j)
      pe_regs_set_int(pe_info->regvals, PE_REGS_ARG, "lsac", j);
  } else if (ls && *ls) {
    pe_regs_set(pe_info->regvals, PE_REGS_ARG | PE_REGS_NOCOPY, "ls", ls);
  }
  if (cmd->type & CMD_T_EQSPLIT) {
    /* The = itself */
    if (rhs_present)
      pe_regs_set(pe_info->regvals, PE_REGS_ARG | PE_REGS_NOCOPY, "equals",
                  "=");
    /* rs, after the = */
    if (cmd->type & CMD_T_RS_ARGS) {
      char argname[20];
      j = 0;
      for (i = 1; i < MAX_ARG; i++) {
        if (rsa[i] && *rsa[i]) {
          snprintf(argname, sizeof argname, "rsa%d", i);
          pe_regs_set(pe_info->regvals, PE_REGS_ARG | PE_REGS_NOCOPY, argname,
                      rsa[i]);
          j = i;
        }
      }
      if (j)
        pe_regs_set_int(pe_info->regvals, PE_REGS_ARG, "rsac", j);
    } else if (rs && *rs) {
      pe_regs_set(pe_info->regvals, PE_REGS_ARG | PE_REGS_NOCOPY, "rs", rs);
    }
  }

  if ((cmd->type & CMD_T_NOP) && ap && *ap) {
    /* Done this way because another call to tprintf during
     * run_cmd_hook will blitz the string */
    snprintf(nop_arg, sizeof nop_arg, "%s %s", cmd->name, ap);
  } else {
    nop_arg[0] = '\0';
  }

  if (!run_hook(executor, enactor, cmd->hooks.ignore, pe_info)) {
    free_pe_info(pe_info);
    return 0;
  }

  /* If we have a hook/override, we use that instead */
  if (!run_cmd_hook(cmd->hooks.override, executor, cmd_evaled, queue_entry,
                    pe_info->regvals) &&
      !((cmd->type & CMD_T_NOP) && *ap &&
        run_cmd_hook(cmd->hooks.override, executor, nop_arg, queue_entry,
                     pe_info->regvals))) {
    /* Otherwise, we do hook/before, the command, and hook/after */
    /* But first, let's see if we had an invalid switch */
    if (switch_err && *switch_err) {
      if (run_cmd_hook(cmd->hooks.extend, executor, cmd_evaled, queue_entry,
                       pe_info->regvals)) {
        free_pe_info(pe_info);
        return 1;
      }
      notify(executor, switch_err);
      free_pe_info(pe_info);
      return 1;
    }
    run_hook(executor, enactor, cmd->hooks.before, pe_info);
    cmd->func(cmd, executor, enactor, enactor, sw, cmd_raw, swp, ap, ls, lsa,
              rs, rsa, queue_entry);
    run_hook(executor, enactor, cmd->hooks.after, pe_info);
  }
  /* Either way, we might log */
  if (cmd->type & CMD_T_LOGARGS)
    if (cmd->func == cmd_password || cmd->func == cmd_newpassword ||
        cmd->func == cmd_pcreate)
      do_log(LT_CMD, executor, enactor, "%s %s=***", cmd->name,
             (cmd->func == cmd_password ? "***" : ls));
    else
      do_log(LT_CMD, executor, enactor, "%s", cmd_evaled);
  else if (cmd->type & CMD_T_LOGNAME)
    do_log(LT_CMD, executor, enactor, "%s", cmd->name);

  free_pe_info(pe_info);
  return 1;
}

/** Execute the huh_command when no command is matched.
 * \param executor the executor running the command.
 * \param enactor dbref that caused the command to be executed.
 * \param string the input given.
 * \param queue_entry the queue entry the invalid cmd was run from
 */
void
generic_command_failure(dbref executor, dbref enactor, char *string,
                        MQUE *queue_entry)
{
  COMMAND_INFO *cmd;

  if ((cmd = command_find("HUH_COMMAND")) && !(cmd->type & CMD_T_DISABLED)) {
    run_command(cmd, executor, enactor, "HUH_COMMAND", NULL, NULL, string, NULL,
                string, string, NULL, NULL, NULL, queue_entry);
  }
}

#define add_restriction(r, j)                                                  \
  if (lockstr != tp && j)                                                      \
    safe_chr(j, lockstr, &tp);                                                 \
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
 * \param player player attempting to restrict command, or NOTHING for
 * restrictions set when cmds are loaded
 * \param command the command being restricted
 * \param xrestriction either a space-separated string of restrictions, or an
 * \@lock-style restriction
 * \retval 1 successfully restricted command.
 * \retval 0 failure (unable to find command name).
 */
int
restrict_command(dbref player, COMMAND_INFO *command, const char *xrestriction)
{
  struct command_perms_t *c;
  char *message, *restriction, *rsave;
  int clear;
  const FLAG *f;
  char lockstr[BUFFER_LEN];
  char *tp;
  int make_boolexp = 0;
  boolexp key;
  object_flag_type flags = NULL, powers = NULL;

  if (!command)
    return 0;

  /* Allow empty restrictions when we first load commands, as we parse off the
   * CMD_T_* flags */
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
        flags = clear_flag_bitmask("FLAG", flags, f->bitpos);
        f = match_flag("WIZARD");
        flags = clear_flag_bitmask("FLAG", flags, f->bitpos);
      } else {
        f = match_flag("ROYALTY");
        flags = set_flag_bitmask("FLAG", flags, f->bitpos);
        f = match_flag("WIZARD");
        flags = set_flag_bitmask("FLAG", flags, f->bitpos);
      }
    } else if ((c = ptab_find(&ptab_command_perms, restriction))) {
      if (clear)
        command->type &= ~c->type;
      else
        command->type |= c->type;
    } else if ((f = match_flag(restriction))) {
      make_boolexp = 1;
      if (clear)
        flags = clear_flag_bitmask("FLAG", flags, f->bitpos);
      else {
        flags = set_flag_bitmask("FLAG", flags, f->bitpos);
      }
    } else if ((f = match_power(restriction))) {
      make_boolexp = 1;
      if (clear)
        powers = clear_flag_bitmask("POWER", powers, f->bitpos);
      else {
        powers = set_flag_bitmask("POWER", powers, f->bitpos);
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
    destroy_flag_bitmask("FLAG", flags);
    destroy_flag_bitmask("POWER", powers);
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
    char tmp[20];
    snprintf(tmp, sizeof tmp, "=#%d", GOD);
    add_restriction(tmp, '&');
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
  /* CMD_T_DISABLED and CMD_T_LOG* are checked for in command->types, and not
   * part of the boolexp */
  *tp = '\0';

  key = parse_boolexp(player, lockstr, CommandLock);
  command->cmdlock = key;

  destroy_flag_bitmask("FLAG", flags);
  destroy_flag_bitmask("POWER", powers);
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
    run_command(cmd, executor, enactor, "UNIMPLEMENTED_COMMAND", sw, NULL, raw,
                NULL, args_raw, arg_left, args_left, arg_right, args_right,
                queue_entry);
  } else {
    /* Either we were already in UNIMPLEMENTED_COMMAND, or we couldn't find it
     */
    notify(executor, T("This command has not been implemented."));
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
      char *switches = NULL;
      if ((flags & (CMD_T_NOPARSE | CMD_T_RS_NOPARSE)) !=
          (CMD_T_NOPARSE | CMD_T_RS_NOPARSE))
        switches = "NOEVAL";
      command_add(mush_strdup(name, "command_add"), flags, NULL, 0, switches,
                  cmd_unimplemented);
      notify_format(player, T("Command %s added."), name);
    }
  } else {
    notify_format(player, T("Command %s already exists."), command->name);
  }
}

/** Clone a built-in command.
 * \verbatim
 * This code implements @command/clone, which clones a built-in command.
 * \endverbatim
 * \param player the enactor
 * \param original name of command to clone
 * \param clone name of new command
 */
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

/** Create a new \@hook.
 * \verbatim
 * Allocates memory for a new\@hook and initializes it,
 *  possibly from an existing hook.
 * \endverbatim
 * \param from Hook to initialize from, or NULL
 * \return pointer to a newly-allocated hook
 */
static struct hook_data *
new_hook(struct hook_data *from)
{
  struct hook_data *newhook = mush_malloc(sizeof(struct hook_data), "hook");

  if (from) {
    newhook->obj = from->obj;
    if (from->attrname) {
      newhook->attrname = mush_strdup(from->attrname, "hook.attr");
    } else {
      newhook->attrname = NULL;
    }
    newhook->inplace = from->inplace;
  } else {
    newhook->obj = NOTHING;
    newhook->attrname = NULL;
    newhook->inplace = QUEUE_DEFAULT;
  }

  return newhook;
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

  c2 = make_command(mush_strdup(clone, "command_add"), c1->type, NULL, NULL,
                    c1->sw.names, c1->func);
  c2->sw.mask = c1->sw.mask;
  if (c1->restrict_message)
    c2->restrict_message =
      mush_strdup(c1->restrict_message, "cmd_restrict_message");
  if (c2->cmdlock != TRUE_BOOLEXP)
    free_boolexp(c2->cmdlock);
  c2->cmdlock = dup_bool(c1->cmdlock);

  if (c1->hooks.before)
    c2->hooks.before = new_hook(c1->hooks.before);

  if (c1->hooks.after)
    c2->hooks.after = new_hook(c1->hooks.after);

  if (c1->hooks.ignore)
    c2->hooks.ignore = new_hook(c1->hooks.ignore);

  if (c1->hooks.override)
    c2->hooks.override = new_hook(c1->hooks.override);

  if (c1->hooks.extend)
    c2->hooks.extend = new_hook(c1->hooks.extend);

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
  const char *alias;
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
    if (command->func != cmd_unimplemented ||
        !strcmp(command->name, "UNIMPLEMENTED_COMMAND")) {
      notify(
        player,
        T("You can't delete built-in commands. @command/disable instead."));
      return;
    } else {
      acount = 0;
      cptr = ptab_firstentry_new(&ptab_command, &alias);
      while (cptr) {
        if (cptr == command) {
          ptab_delete(&ptab_command, alias);
          acount++;
          cptr = ptab_firstentry_new(&ptab_command, &alias);
        } else
          cptr = ptab_nextentry_new(&ptab_command, &alias);
      }
      mush_free((char *) command->name, "command.name");
      mush_free(command, "command");
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
    notify(executor, T("You must specify a command."));
    return;
  }
  if (SW_ISSET(sw, SWITCH_ADD)) {
    int flags = CMD_T_ANY;
    flags |= SW_ISSET(sw, SWITCH_NOPARSE) ? CMD_T_NOPARSE : 0;
    flags |= SW_ISSET(sw, SWITCH_RSARGS) ? CMD_T_RS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_LSARGS) ? CMD_T_LS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_LSARGS) ? CMD_T_LS_ARGS : 0;
    flags |= SW_ISSET(sw, SWITCH_EQSPLIT) ? CMD_T_EQSPLIT : 0;
    flags |= SW_ISSET(sw, SWITCH_RSNOPARSE) ? CMD_T_RS_NOPARSE : 0;
    if (SW_ISSET(sw, SWITCH_NOEVAL))
      notify(executor,
             T("WARNING: /NOEVAL no longer creates a Noparse command.\n        "
               " Use /NOPARSE if that's what you meant."));
    do_command_add(executor, arg_left, flags);
    return;
  }
  if (SW_ISSET(sw, SWITCH_ALIAS)) {
    if (Wizard(executor)) {
      if (!ok_command_name(upcasestr(arg_right))) {
        notify(executor, T("I can't alias a command to that!"));
      } else if (!alias_command(arg_left, arg_right)) {
        notify(executor, T("Unable to set alias."));
      } else {
        if (!SW_ISSET(sw, SWITCH_QUIET))
          notify(executor, T("Alias set."));
      }
    } else {
      notify(executor, T("Permission denied."));
    }
    return;
  }
  if (SW_ISSET(sw, SWITCH_CLONE)) {
    do_command_clone(executor, arg_left, arg_right);
    return;
  }

  if (SW_ISSET(sw, SWITCH_DELETE)) {
    do_command_delete(executor, arg_left);
    return;
  }
  command = command_find(arg_left);
  if (!command) {
    notify(executor, T("No such command."));
    return;
  }
  if (Wizard(executor)) {
    if (SW_ISSET(sw, SWITCH_ON) || SW_ISSET(sw, SWITCH_ENABLE))
      command->type &= ~CMD_T_DISABLED;
    else if (SW_ISSET(sw, SWITCH_OFF) || SW_ISSET(sw, SWITCH_DISABLE))
      command->type |= CMD_T_DISABLED;

    if (SW_ISSET(sw, SWITCH_RESTRICT)) {
      if (!arg_right || !arg_right[0]) {
        notify(executor, T("How do you want to restrict the command?"));
        return;
      }

      if (!restrict_command(executor, command, arg_right))
        notify(executor, T("Restrict attempt failed."));
    }

    if ((command->func == cmd_command) && (command->type & CMD_T_DISABLED)) {
      notify(executor, T("@command is ALWAYS enabled."));
      command->type &= ~CMD_T_DISABLED;
    }
  }
  if (!SW_ISSET(sw, SWITCH_QUIET)) {
    notify_format(executor, T("Name       : %s (%s)"), command->name,
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
    if (command->type & CMD_T_DEPRECATED)
      strccat(buff, &bp, "Deprecated");
    *bp = '\0';
    notify_format(executor, T("Flags      : %s"), buff);
    buff[0] = '\0';
    notify_format(executor, T("Lock       : %s"),
                  unparse_boolexp(executor, command->cmdlock, UB_DBREF));
    if (command->restrict_message)
      notify_format(executor, T("Failure Msg: %s"), command->restrict_message);
    if (command->sw.mask) {
      bp = buff;
      for (sw_val = dyn_switch_list; sw_val->name; sw_val++)
        if (SW_ISSET(command->sw.mask, sw_val->value))
          strccat(buff, &bp, sw_val->name);
      *bp = '\0';
      notify_format(executor, T("Switches   : %s"), buff);
    } else
      notify(executor, T("Switches   :"));
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
      notify_format(executor, T("Leftside   : %s"), buff);
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
      notify_format(executor, T("Rightside  : %s"), buff);
    } else {
      *bp = '\0';
      notify_format(executor, T("Arguments  : %s"), buff);
    }
    do_hook_list(executor, arg_left, 0);
  }
}

/** Display a list of defined commands.
 * This function sends a player the list of commands.
 * \param player the enactor.
 * \param lc if true, list is in lowercase rather than uppercase.
 * \param type 3 = show all, 2 = show only local \@commands, 1 = show only
 * built-in \@commands
 */
void
do_list_commands(dbref player, int lc, int type)
{
  char *b = list_commands(type);
  notify_format(player, T("Commands: %s"), lc ? strlower(b) : b);
}

/** Return a list of defined commands.
 * This function returns a space-separated list of commands as a string.
 * \param type 3 = show all, 2 = show only local \@commands, 1 = show only
 * built-in \@commands
 */
char *
list_commands(int type)
{
  COMMAND_INFO *command;
  char *ptrs[BUFFER_LEN / 2];
  static char buff[BUFFER_LEN];
  char *bp;
  int nptrs = 0, i;

  command = (COMMAND_INFO *) ptab_firstentry(&ptab_command);
  while (command) {
    if (type == 3 || (type == 1 && command->func != cmd_unimplemented) ||
        (type == 2 && command->func == cmd_unimplemented)) {
      ptrs[nptrs] = (char *) command->name;
      nptrs++;
    }
    command = (COMMAND_INFO *) ptab_nextentry(&ptab_command);
  }

  if (!nptrs)
    return "";

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

/** Check command permissions. Return 1 if player can use command,
 * 0 otherwise, and maybe be noisy about it.
 * \param player the player trying to use the command
 * \param cmd the command to check
 * \param noisy should we report failure to the player?
 * \param pe_info the pe_info to use for evaluating the command's lock
 * \return 1 if player can use command, 0 if not
 */
int
command_check_with(dbref player, COMMAND_INFO *cmd, int noisy,
                   NEW_PE_INFO *pe_info)
{

  /* If disabled, return silently */
  if (cmd->type & CMD_T_DISABLED)
    return 0;

  if (eval_boolexp(player, cmd->cmdlock, player, pe_info)) {
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
 * \param pe_info pe_info to use for evaluating cmd lock
 * \retval 0 player may not use command.
 * \retval 1 player may use command.
 */
int
command_check_byname(dbref player, const char *name, NEW_PE_INFO *pe_info)
{
  COMMAND_INFO *cmd;
  cmd = command_find(name);
  if (!cmd)
    return 0;
  return command_check_with(player, cmd, 1, pe_info);
}

/** Determine whether a player can use a command.
 * This function checks whether a player can use a command.
 * If the command is disallowed, the player is informed.
 * \param player player whose privileges are checked.
 * \param name name of command.
 * \param pe_info pe_info to use for evaluating cmd lock
 * \retval 0 player may not use command.
 * \retval 1 player may use command.
 */
int
command_check_byname_quiet(dbref player, const char *name, NEW_PE_INFO *pe_info)
{
  COMMAND_INFO *cmd;
  cmd = command_find(name);
  if (!cmd)
    return 0;
  return command_check_with(player, cmd, 0, pe_info);
}

/** Is a particular hook set, and valid?
 * \param hook the hook to check
 * \return 1 if valid, 0 if not
 */
static int
has_hook(struct hook_data *hook)
{
  if (!hook || !GoodObject(hook->obj) || IsGarbage(hook->obj))
    return 0;
  return 1;
}

/** Run a command hook.
 * This function runs a hook before or after a command execution.
 * \param executor the executor.
 * \param enactor dbref that caused command to execute.
 * \param hook pointer to the hook.
 * \param pe_info pe_info to evaluate hook with
 * \retval 1 Hook doesn't exist, or evaluates to a non-false value
 * \retval 0 Hook exists and evaluates to a false value
 */
int
run_hook(dbref executor, dbref enactor, struct hook_data *hook,
         NEW_PE_INFO *pe_info)
{
  ATTR *atr;
  char *code;
  const char *cp;
  char buff[BUFFER_LEN], *bp;

  if (!has_hook(hook))
    return 1;

  atr = atr_get(hook->obj, hook->attrname);

  if (!atr)
    return 1;

  code = safe_atr_value(atr, "hook.code");
  if (!code)
    return 1;

  cp = code;
  bp = buff;

  process_expression(buff, &bp, &cp, hook->obj, enactor, executor, PE_DEFAULT,
                     PT_DEFAULT, pe_info);
  *bp = '\0';

  mush_free(code, "hook.code");
  return parse_boolean(buff);
}

/** Run the \@hook/override for a command, if set
 * \param cmd the command with the hook
 * \param executor the player running the command
 * \param commandraw the evaluated command string to match against the hook
 * \param from_queue the queue entry the command is being executed in
 * \param pe_regs pe_regs containing named stack variables for each command part
 * \return 1 if the hook has been run successfully, 0 otherwise
 */
int
run_cmd_hook(struct hook_data *hook, dbref executor, const char *commandraw,
             MQUE *from_queue, PE_REGS *pe_regs)
{
  int queue_type = QUEUE_DEFAULT;

  if (!has_hook(hook))
    return 0;

  queue_type = hook->inplace;

  if (from_queue && (from_queue->queue_type & QUEUE_DEBUG_PRIVS))
    queue_type |= QUEUE_DEBUG_PRIVS;

  if (hook->attrname) {
    return one_comm_match(hook->obj, executor, hook->attrname, commandraw,
                          from_queue, queue_type, pe_regs);
  } else {
    return atr_comm_match(hook->obj, executor, '$', ':', commandraw, 0, 1, NULL,
                          NULL, 0, NULL, from_queue, queue_type, pe_regs);
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
  struct hook_data **h;
  int inplace = QUEUE_DEFAULT;

  if (!opts || !*opts)
    return 0;

  cmd = command_find(command);
  if (!cmd)
    return 0;

  p = trim_space_sep(opts, ' ');
  if (!(one = split_token(&p, ' ')))
    return 0;

  if (strcasecmp("before", one) == 0) {
    flag = HOOK_BEFORE;
    h = &cmd->hooks.before;
  } else if (strcasecmp("after", one) == 0) {
    flag = HOOK_AFTER;
    h = &cmd->hooks.after;
  } else if (strcasecmp("override/inplace", one) == 0) {
    flag = HOOK_OVERRIDE;
    h = &cmd->hooks.override;
    inplace = QUEUE_INPLACE;
  } else if (strcasecmp("override", one) == 0) {
    flag = HOOK_OVERRIDE;
    h = &cmd->hooks.override;
  } else if (strcasecmp("ignore", one) == 0) {
    flag = HOOK_IGNORE;
    h = &cmd->hooks.ignore;
  } else if (strcasecmp("extend/inplace", one) == 0) {
    flag = HOOK_EXTEND;
    h = &cmd->hooks.extend;
    inplace = QUEUE_INPLACE;
  } else if (strcasecmp("extend", one) == 0) {
    flag = HOOK_EXTEND;
    h = &cmd->hooks.extend;
  } else {
    return 0;
  }

  if (!(one = split_token(&p, ' '))) {
    /* Clear existing hook */
    if (*h) {
      if ((*h)->attrname) {
        mush_free((*h)->attrname, "hook.attr");
        (*h)->attrname = NULL;
      }
      mush_free(*h, "hook");
      *h = NULL;
    }
    return 1;
  }

  if ((attrname = strchr(one, '/')) == NULL) {
    if (flag != HOOK_OVERRIDE) {
      return 0; /* attribute required */
    }
  } else {
    *attrname++ = '\0';
    upcasestr(attrname);
  }

  /* Account for #dbref */
  if (*one == '#')
    one++;

  if (!is_strict_integer(one))
    return 0;

  thing = (dbref) parse_integer(one);
  if (!GoodObject(thing) || IsGarbage(thing))
    return 0;

  if (attrname && !good_atr_name(attrname))
    return 0;

  if (*h == NULL) {
    *h = new_hook(NULL);
  } else if ((*h)->attrname) {
    mush_free((*h)->attrname, "hook.attr");
  }

  (*h)->obj = thing;

  if (attrname)
    (*h)->attrname = mush_strdup(attrname, "hook.attr");
  else
    (*h)->attrname = NULL;
  (*h)->inplace = inplace;
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
 * \param queue_type If override hook, QUEUE_* flags telling whether/how to run
 * it inplace
 */
void
do_hook(dbref player, char *command, char *obj, char *attrname,
        enum hook_type flag, int queue_type)
{
  COMMAND_INFO *cmd;
  struct hook_data **h;

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
  else if (flag == HOOK_EXTEND)
    h = &cmd->hooks.extend;
  else {
    notify(player, T("Unknown hook type"));
    return;
  }

  if (!obj && !attrname) {
    notify_format(player, T("Hook removed from %s."), cmd->name);
    if (*h) {
      if ((*h)->attrname) {
        mush_free((*h)->attrname, "hook.attr");
        (*h)->attrname = NULL;
      }
      mush_free(*h, "hook");
      *h = NULL;
    }
  } else if (!obj || !*obj ||
             ((flag != HOOK_OVERRIDE && flag != HOOK_EXTEND) &&
              (!attrname || !*attrname))) {
    if (flag == HOOK_OVERRIDE || flag == HOOK_EXTEND) {
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
    if (!(*h))
      *h = new_hook(NULL);
    (*h)->obj = objdb;
    if ((*h)->attrname)
      mush_free((*h)->attrname, "hook.attr");
    if (!attrname || !*attrname) {
      (*h)->attrname = NULL;
    } else {
      (*h)->attrname = strupper_a(attrname, "hook.attr");
    }
    (*h)->inplace = queue_type;
    notify_format(player, T("Hook set for %s."), cmd->name);
  }
}

/** List command hooks.
 * \verbatim
 * This is the top-level function for @hook/list, @list/hooks, and
 * the hook-listing part of @command.
 * \endverbatim
 * \param player the enactor.
 * \param command command to list hooks on.
 * \param verbose Report failures?
 */
void
do_hook_list(dbref player, char *command, bool verbose)
{
  COMMAND_INFO *cmd;
  int count = 0;

  if (!command || !*command) {
    /* Show all commands with hooks */
    char *ptrs[BUFFER_LEN / 2];
    static char buff[BUFFER_LEN];
    char *bp;
    int i;

    for (cmd = (COMMAND_INFO *) ptab_firstentry(&ptab_command); cmd;
         cmd = (COMMAND_INFO *) ptab_nextentry(&ptab_command)) {
      if (has_hook(cmd->hooks.ignore) || has_hook(cmd->hooks.override) ||
          has_hook(cmd->hooks.before) || has_hook(cmd->hooks.after)) {
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
      char override_inplace[BUFFER_LEN], *op;
      char extend_inplace[BUFFER_LEN], *ep;
      op = override_inplace;
      ep = extend_inplace;
      if (cmd->hooks.override &&
          (cmd->hooks.override->inplace & QUEUE_INPLACE)) {
        if ((cmd->hooks.override->inplace &
             (QUEUE_RECURSE | QUEUE_CLEAR_QREG)) ==
            (QUEUE_RECURSE | QUEUE_CLEAR_QREG))
          safe_str("/inplace", override_inplace, &op);
        else {
          safe_str("/inline", override_inplace, &op);
          if (cmd->hooks.override->inplace & QUEUE_NO_BREAKS)
            safe_str("/nobreak", override_inplace, &op);
          if (cmd->hooks.override->inplace & QUEUE_PRESERVE_QREG)
            safe_str("/localize", override_inplace, &op);
          if (cmd->hooks.override->inplace & QUEUE_CLEAR_QREG)
            safe_str("/clearregs", override_inplace, &op);
        }
      }
      *op = '\0';

      if (cmd->hooks.extend && (cmd->hooks.extend->inplace & QUEUE_INPLACE)) {
        if ((cmd->hooks.extend->inplace & (QUEUE_RECURSE | QUEUE_CLEAR_QREG)) ==
            (QUEUE_RECURSE | QUEUE_CLEAR_QREG))
          safe_str("/inplace", extend_inplace, &ep);
        else {
          safe_str("/inline", extend_inplace, &ep);
          if (cmd->hooks.extend->inplace & QUEUE_NO_BREAKS)
            safe_str("/nobreak", extend_inplace, &ep);
          if (cmd->hooks.extend->inplace & QUEUE_PRESERVE_QREG)
            safe_str("/localize", extend_inplace, &ep);
          if (cmd->hooks.extend->inplace & QUEUE_CLEAR_QREG)
            safe_str("/clearregs", extend_inplace, &ep);
        }
      }
      *ep = '\0';

      if (cmd->hooks.before && GoodObject(cmd->hooks.before->obj)) {
        count++;
        notify_format(player, "@hook/before: #%d/%s", cmd->hooks.before->obj,
                      cmd->hooks.before->attrname);
      }
      if (cmd->hooks.after && GoodObject(cmd->hooks.after->obj)) {
        count++;
        notify_format(player, "@hook/after: #%d/%s", cmd->hooks.after->obj,
                      cmd->hooks.after->attrname);
      }
      if (cmd->hooks.ignore && GoodObject(cmd->hooks.ignore->obj)) {
        count++;
        notify_format(player, "@hook/ignore: #%d/%s", cmd->hooks.ignore->obj,
                      cmd->hooks.ignore->attrname);
      }
      if (cmd->hooks.override && GoodObject(cmd->hooks.override->obj)) {
        count++;
        notify_format(player, "@hook/override%s: #%d/%s", override_inplace,
                      cmd->hooks.override->obj, cmd->hooks.override->attrname);
      }
      if (cmd->hooks.extend && GoodObject(cmd->hooks.extend->obj)) {
        count++;
        notify_format(player, "@hook/extend%s: #%d/%s", extend_inplace,
                      cmd->hooks.extend->obj, cmd->hooks.extend->attrname);
      }
      if (!count && verbose)
        notify(player, T("That command has no hooks."));
    } else if (verbose) {
      notify(player, T("Permission denied."));
    }
  }
}
