/**
 * \file conf.h
 *
 * \brief Routines for Penn's \@config system, and options used in Penn's internals.
 */

#ifndef __PENNCONF_H
#define __PENNCONF_H

#include "copyrite.h"
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include "options.h"
#include "mushtype.h"
#include "htab.h"

#define PLAYER_NAME_LIMIT (options.player_name_len) /**< limit on player name length */

#define OBJECT_NAME_LIMIT 256 /**< limit on (non-player) object name length */

#define ATTRIBUTE_NAME_LIMIT 1024 /**< Limit on attribute name length */

#define COMMAND_NAME_LIMIT 64  /**< Loose limit on command/function name length */

/* magic cookies */
#define LOOKUP_TOKEN '*'  /**< Token that denotes player name in object matching */
#define NUMBER_TOKEN '#'  /**< Dbref token */
#define ARG_DELIMITER '=' /**< No longer used */

/* magic command cookies */
#define SAY_TOKEN '"'    /**< One-char alias for SAY command */
#define POSE_TOKEN ':'   /**< One-char alias for POSE command */
#define SEMI_POSE_TOKEN ';'   /**< One-char alias for SEMIPOSE command */
#define EMIT_TOKEN '\\'   /**< One-char alias for @EMIT command */
#define CHAT_TOKEN '+'   /**< One-char alias for @CHAT command */
#define NOEVAL_TOKEN ']'   /**< Prefix that prevents cmd evaluation */
#define DEBUG_TOKEN '}'   /**< Prefix that enables debug for the queue entry */

#define ALIAS_DELIMITER ';'  /**< Delimiter in player/exit aliases */
/* No longer used, but kept for hackers */
#define EXIT_DELIMITER ALIAS_DELIMITER

#define QUIT_COMMAND "QUIT"
#define WHO_COMMAND "WHO"
#define LOGOUT_COMMAND "LOGOUT"
#define INFO_COMMAND "INFO"
#define INFO_VERSION "1.1"
#define DOING_COMMAND "DOING"
#define SESSION_COMMAND "SESSION"
#define IDLE_COMMAND "IDLE"
#define MSSPREQUEST_COMMAND "MSSP-REQUEST"

#define GET_COMMAND "GET"
#define POST_COMMAND "POST"

#define PREFIX_COMMAND "OUTPUTPREFIX"
#define SUFFIX_COMMAND "OUTPUTSUFFIX"
#define PUEBLO_COMMAND "PUEBLOCLIENT "

/* These CAN be modified, but it's heavily NOT suggested */
#define PUEBLO_SEND "</xch_mudtext><img xch_mode=purehtml><xch_page clear=text>\n"
#define PUEBLO_HELLO "This world is Pueblo 1.10 Enhanced.\r\n"


/** How much pending outgoing text can be queued up on a socket before
 * the dreaded 'Output flushed' message shows up? Used to be 16k, now 1m.
 */
#define MAX_OUTPUT (1024*1024)
/** How much output buffer space must be left before we flush the
 * buffer? Reportedly, using '0' fixes problems with Win32 port,
 * and may be more efficient in network use. Using (MAX_OUTPUT / 2)
 * is how it's been done in the past. You get to pick.
 */
#define SPILLOVER_THRESHOLD     0
/* #define SPILLOVER_THRESHOLD  (MAX_OUTPUT / 2) */
#define COMMAND_TIME_MSEC 1000  /* time slice length in milliseconds */
#define COMMAND_BURST_SIZE 100  /* commands allowed per user in a burst */
#define COMMANDS_PER_TIME 1     /* commands per time slice after burst */


/* From conf.c */
bool config_file_startup(const char *conf, int restrictions);
void config_file_checks(void);

void do_config_list(dbref player, const char *type, int lc);

typedef struct options_table OPTTAB;

typedef int (*config_func) (const char *opt, const char *val, void *loc,
                            int maxval, int source);

#define CP_OVERRIDDEN 1         /* Set by .cnf file */
#define CP_OPTIONAL   2         /* Doesn't complain if it's missing. */
#define CP_CONFIGSET  4         /* Overridden/set by @config/set */
#define CP_GODONLY    8         /* Only God can see. */

/** Runtime configuration parameter.
 * This structure represents a runtime configuration option.
 */
typedef struct confparm {
  const char *name;             /**< name of option. */
  config_func handler;          /**< the function handler. */
  void *loc;                    /**< place to put this option. */
  int max;                      /**< max: string length, integer value. */
  int flags;                    /**< Has the default been overridden? */
  const char *group;            /**< The option's group name */
} PENNCONF;


/** Runtime configuration options.
 * This large structure stores all of the runtime configuration options
 * that are typically set in mush.cnf.
 */
struct options_table {
  char mud_name[128];   /**< The name of the mush */
  char mud_url[256];   /**< The name of the mush */
  int port;             /**< The port to listen for connections */
  int ssl_port;         /**< The port to listen for SSL connections */
  char socket_file[256];  /**< The socket filename to use for SSL slave */
  char input_db[256];   /**< Name of the input database file */
  char output_db[256];  /**< Name of the output database file */
  char crash_db[256];   /**< Name of the panic database file */
  char mail_db[256];    /**< Name of the mail database file */
  dbref player_start;   /**< The room in which new players are created */
  dbref master_room;    /**< The master room for global commands/exits */
  dbref ancestor_room;  /**< The ultimate parent room */
  dbref ancestor_exit;  /**< The ultimate parent exit */
  dbref ancestor_thing; /**< The ultimate parent thing */
  dbref ancestor_player; /**< The ultimate parent player */
  dbref event_handler;  /**< The object events. */
  int connect_fail_limit; /**< Maximum number of connect fails in 10 mins. */
  int idle_timeout;     /**< Maximum idle time allowed, in minutes */
  int unconnected_idle_timeout; /**< Maximum idle time for connections without dbrefs, in minutes */
  int keepalive_timeout; /**< Number of seconds between TCP keepalive pings */
  int dump_interval;    /**< Interval between database dumps, in seconds */
  char dump_message[256]; /**< Message shown at start of nonforking dump */
  char dump_complete[256]; /**< Message shown at end of nonforking dump */
  time_t dump_counter;  /**< Time since last dump */
  int max_logins;       /**< Maximum total logins allowed at once */
  int max_guests;       /**< Maximum guests logins allowed at once */
  int max_named_qregs;  /**< Maximum # of non-a-z0-9 qregs per pe_regs. */
  int whisper_loudness; /**< % chance that a noisy whisper is overheard */
  int page_aliases;     /**< Does page include aliases? */
  int paycheck;         /**< Number of pennies awarded each day of connection */
  int guest_paycheck;   /**< Paycheck for guest connections */
  int starting_money;   /**< Number of pennies for newly created players */
  int starting_quota;   /**< Object quota for newly created players */
  int player_queue_limit; /**< Maximum commands a player can queue at once */
  int queue_chunk;      /**< Number of commands run from queue when no input from sockets is waiting */
  int active_q_chunk;   /**< Number of commands run from queue when input from sockets is waiting */
  int func_nest_lim;    /**< Maximum function recursion depth */
  int func_invk_lim;    /**< Maximum number of function invocations */
  int call_lim;         /**< Maximum parser calls allowed in a queue cycle */
  char log_wipe_passwd[256];    /**< Password for logwipe command */
  char money_singular[32];      /**< Currency unit name, singular */
  char money_plural[32];        /**< Currency unit name, plural */
  char compressprog[256];       /**< Program to compress database dumps */
  char uncompressprog[256];     /**< Program to uncompress database dumps */
  char compresssuff[256];       /**< Suffix for compressed dump files */
  char chatdb[256];             /**< Name of the chat database file */
  int max_player_chans;         /**< Number of channels a player can create */
  int max_channels;             /**< Total maximum allowed channels */
  int chan_title_len;           /**< Maximum length of a player's channel title */
  int chan_cost;                /**< Cost to create a channel */
  int noisy_cemit;              /**< Is \@cemit noisy by default? */
  char connect_file[2][256];    /**< Names of text and html connection files */
  char motd_file[2][256];       /**< Names of text and html motd files */
  char wizmotd_file[2][256];    /**< Names of text and html wizmotd files */
  char newuser_file[2][256];    /**< Names of text and html new user files */
  char register_file[2][256];   /**< Names of text and html registration files */
  char quit_file[2][256];       /**< Names of text and html disconnection files */
  char down_file[2][256];       /**< Names of text and html server down files */
  char full_file[2][256];       /**< Names of text and html server full files */
  char guest_file[2][256];      /**< Names of text and html guest files */
  int log_commands;     /**< Should we log all commands? */
  int log_forces;       /**< Should we log force commands? */
  int support_pueblo;   /**< Should the MUSH send Pueblo tags? */
  int login_allow;      /**< Are mortals allowed to log in? */
  int guest_allow;      /**< Are guests allowed to log in? */
  int create_allow;     /**< Can new players be created? */
  int reverse_shs;      /**< Should the SHS routines assume little-endian byte order? */
  char player_flags[BUFFER_LEN];        /**< Space-separated list of flags to set on newly created players. */
  char room_flags[BUFFER_LEN];          /**< Space-separated list of flags to set on newly created rooms. */
  char exit_flags[BUFFER_LEN];          /**< Space-separated list of flags to set on newly created exits. */
  char thing_flags[BUFFER_LEN];         /**< Space-separated list of flags to set on newly created things. */
  char channel_flags[BUFFER_LEN];       /**< Space-separated list of flags to set on newly created channels. */
  int warn_interval;    /**< Interval between warning checks */
  time_t warn_counter;  /**< Time since last warning check */
  dbref base_room;      /**< Room which floating checks consider as the base */
  dbref default_home;   /**< Home for the homeless */
  int use_dns;          /**< Should we use DNS lookups? */
  int safer_ufun;       /**< Should we require security for ufun calls? */
  char dump_warning_1min[256];  /**< 1 minute nonforking dump warning message */
  char dump_warning_5min[256];  /**< 5 minute nonforking dump warning message */
  int noisy_whisper;    /**< Does whisper default to whisper/noisy? */
  int possessive_get;   /**< Can possessive get be used? */
  int possessive_get_d; /**< Can possessive get be used on disconnected players? */
  int really_safe;      /**< Does the SAFE flag protect objects from nuke */
  int destroy_possessions;      /**< Are the possessions of a nuked player nuked? */
  int null_eq_zero;     /**< Is null string treated as 0 in math functions? */
  int tiny_booleans;    /**< Do strings and db#'s evaluate as false, like TinyMUSH? */
  int tiny_trim_fun;    /**< Does the trim function take arguments in TinyMUSH order? */
  int tiny_math;        /**< Can you use strings in math functions, like TinyMUSH? */
  int adestroy;         /**< Is the adestroy attribute available? */
  int amail;            /**< Is the amail attribute available? */
  int mail_limit;       /**< Maximum number of mail messages per player */
  int player_listen;    /**< Does listen work on players? */
  int player_ahear;     /**< Does ahear work on players? */
  int startups;         /**< Is startup run on startups? */
  int room_connects;    /**< Do players trigger aconnect/adisconnect on their location? */
  int ansi_names;       /**< Are object names shown in bold? */
  int comma_exit_list;  /**< Should exit lists be itemized? */
  int count_all;        /**< Are hidden players included in total player counts? */
  int exits_connect_rooms;      /**< Does the presence of an exit make a room connected? */
  int zone_control;     /**< Are only ZMPs allowed to determine zone-based control? */
  int link_to_object;   /**< Can exits be linked to objects? */
  int owner_queues;     /**< Are queues tracked by owner or individual object? */
  int wiz_noaenter;     /**< Do DARK wizards trigger aenters? */
  char ip_addr[64];     /**< What ip address should the server bind to? */
  char ssl_ip_addr[64]; /**< What ip address should the server bind to? */
  int player_name_spaces;       /**< Can players have multiword names? */
  int max_aliases;              /**< Maximum allowed aliases per player */
  int forking_dump;     /**< Should we fork to dump? */
  int restrict_building;        /**< Is the builder power required to build? */
  int free_objects;     /**< If builder power is required, can you create without it? */
  int flags_on_examine; /**< Are object flags shown when it's examined? */
  int ex_public_attribs;        /**< Are visual attributes shown on examine? */
  int full_invis;       /**< Are DARK wizards anonymous? */
  int silent_pemit;     /**< Does pemit default to pemit/silent? */
  dbref max_dbref;      /**< Maximum allowable database size */
  int chat_strip_quote; /**< Should we strip initial quotes in chat? */
  char wizwall_prefix[256];     /**< Prefix for wizwall announcements */
  char rwall_prefix[256];       /**< Prefix for rwall announcements */
  char wall_prefix[256];        /**< Prefix for wall announcements */
  int announce_connects;        /**< Should dis/connects be announced? */
  char access_file[256];        /**< Name of file of access control rules */
  char names_file[256]; /**< Name of file of forbidden player names */
  int object_cost;      /**< Cost to create an object */
  int exit_cost;        /**< Cost to create an exit */
  int link_cost;        /**< Cost to link an exit */
  int room_cost;        /**< Cost to dig a room */
  int queue_cost;       /**< Deposit to queue a command */
  int quota_cost;       /**< Number of objects per quota unit */
  int find_cost;        /**< Cost to create an object */
  int kill_default_cost;        /**< Default cost to use 'kill' */
  int kill_min_cost;    /**< Minimum cost to use 'kill' */
  int kill_bonus;       /**< Percentage of cost paid to victim of 'kill' */
  int queue_loss;       /**< 1/queue_loss chance of a command costing a penny */
  int max_pennies;      /**< Maximum pennies a player can have */
  int max_guest_pennies;        /**< Maximum pennies a guest can have */
  int max_depth;        /**< Maximum container depth */
  int max_parents;      /**< Maximum parent depth */
  int purge_interval;   /**< Time between automatic purges */
  time_t purge_counter; /**< Time since last automatic purge */
  int dbck_interval;    /**< Time between automatic dbcks */
  time_t dbck_counter;  /**< Time since last automatic dbck */
  int max_attrcount;    /**< Maximum number of attributes per object */
  int float_precision;  /**< Precision of floating point display */
  int player_name_len;  /**< Maximum length of player names */
  int queue_entry_cpu_time;     /**< Maximum cpu time allowed per queue entry */
  int ascii_names;      /**< Are object names restricted to ascii characters? */
  char chunk_swap_file[256];    /**< Name of the attribute swap file */
  int chunk_swap_initial; /**< Disc space to reserve for the swap file, in kibibytes */
  int chunk_cache_memory;       /**< Memory to use for the attribute cache */
  int chunk_migrate_amount;     /**< Number of attrs to migrate each second */
  int read_remote_desc; /**< Can players read DESCRIBE attribute remotely? */
  char ssl_private_key_file[256];       /**< File to load the server's cert from */
  char ssl_ca_file[256];        /**< File to load the CA certs from */
  int ssl_require_client_cert;  /**< Are clients required to present certs? */
  int mem_check;        /**< Turn on the memory allocation checker? */
  int use_quota;        /**< Are quotas enabled? */
  int empty_attrs;      /**< Are empty attributes preserved? */
  int function_side_effects; /**< Turn on side effect functions? */
  char error_log[256]; /**< File to log connections */
  char connect_log[256]; /**< File to log connections */
  char wizard_log[256]; /**< File to log wizard commands */
  char command_log[256]; /**< File to log suspect commands */
  char trace_log[256]; /**< File to log trace data */
  char checkpt_log[256]; /**< File to log checkpoint data */
  char sql_platform[256]; /**< Type of SQL server, or "disabled" */
  char sql_host[256]; /**< Hostname of sql server */
  char sql_username[256]; /**< Username for sql */
  char sql_password[256]; /**< Password for sql */
  char sql_database[256]; /**< Database for sql */
};

typedef struct mssp MSSP;
/** MUD Server Status Protocol options
 * Holds various bits of information about the MUSH which can be
 * collected by bots/crawlers. See http://tintin.sourceforge.net/mssp/
 * for details. Linked list.
 */
struct mssp {
  char *name;  /**< Name of the MSSP option */
  char *value; /**< Value of the MSSP option */
  MSSP *next;  /**< Pointer to next MSSP option in linked list */
};

extern OPTTAB options;
extern HASHTAB local_options;
extern MSSP *mssp;

extern PENNCONF *add_config(const char *name, config_func handler, void *loc,
                            int max, const char *group);
extern PENNCONF *new_config(void);
extern PENNCONF *get_config(const char *name);
extern void validate_config(void);


int cf_bool(const char *opt, const char *val, void *loc, int maxval,
            int from_cmd);
int cf_str(const char *opt, const char *val, void *loc, int maxval,
           int from_cmd);
int cf_int(const char *opt, const char *val, void *loc, int maxval,
           int from_cmd);
int cf_dbref(const char *opt, const char *val, void *loc, int maxval,
             int from_cmd);
int cf_flag(const char *opt, const char *val, void *loc, int maxval,
            int from_cmd);
int cf_time(const char *opt, const char *val, void *loc, int maxval,
            int from_cmd);

/* Config group viewing permissions */
#define CGP_GOD         0x1  /**< Config group can only be seen by God */
#define CGP_WIZARD      0x3  /**< Config group can only be seen by Wizards */
#define CGP_ADMIN       0x7  /**< Config group can only be seen by Wiz/Roy */
#define Can_View_Config_Group(p,g) \
        (!(g->viewperms) || (God(p) && (g->viewperms & CGP_GOD)) || \
         (Wizard(p) && (g->viewperms & CGP_WIZARD)) || \
         (Hasprivs(p) && (g->viewperms & CGP_ADMIN)))

int can_view_config_option(dbref player, PENNCONF *opt);


#define DUMP_INTERVAL       (options.dump_interval)
#define DUMP_NOFORK_MESSAGE  (options.dump_message)
#define DUMP_NOFORK_COMPLETE (options.dump_complete)
#define CONNECT_FAIL_LIMIT   (options.connect_fail_limit)
#define INACTIVITY_LIMIT    (options.idle_timeout)
#define UNCONNECTED_LIMIT    (options.unconnected_idle_timeout)

#define MAX_LOGINS      (options.max_logins)
#define MAX_GUESTS      (options.max_guests)
#define MAX_NAMED_QREGS (options.max_named_qregs)

/* dbrefs are in the conf file */

#define TINYPORT         (options.port)
#define SSLPORT          (options.ssl_port)
#define PLAYER_START     (options.player_start)
#define MASTER_ROOM      (options.master_room)
#define ANCESTOR_ROOM           (options.ancestor_room)
#define ANCESTOR_EXIT           (options.ancestor_exit)
#define ANCESTOR_THING          (options.ancestor_thing)
#define ANCESTOR_PLAYER         (options.ancestor_player)
#define EVENT_HANDLER    (options.event_handler)
#define MONEY            (options.money_singular)
#define MONIES           (options.money_plural)
#define WHISPER_LOUDNESS        (options.whisper_loudness)
#define PAGE_ALIASES    (options.page_aliases)

#define START_BONUS      (options.starting_money)
#define PAY_CHECK        (options.paycheck)
#define GUEST_PAY_CHECK        (options.guest_paycheck)
#define START_QUOTA      (options.starting_quota)
#define LOG_WIPE_PASSWD  (options.log_wipe_passwd)
#define SUPPORT_PUEBLO   (options.support_pueblo)

#define QUEUE_QUOTA      (options.player_queue_limit)

#define MUDNAME          (options.mud_name)
#define MUDURL           (options.mud_url)
#define DEF_DB_IN        (options.input_db)
#define DEF_DB_OUT       (options.output_db)

#define BASE_ROOM        (options.base_room)
#define DEFAULT_HOME     (options.default_home)

#define PURGE_INTERVAL   (options.purge_interval)
#define DBCK_INTERVAL    (options.dbck_interval)
#define MAX_PARENTS (options.max_parents)
#define MAX_ZONES (30)
#define MAX_DEPTH (options.max_depth)
#define MAX_PENNIES (options.max_pennies)
#define MAX_GUEST_PENNIES (options.max_guest_pennies)
#define DBTOP_MAX (options.max_dbref)
#define QUEUE_LOSS (options.queue_loss)
#define KILL_BONUS (options.kill_bonus)
#define KILL_MIN_COST (options.kill_min_cost)
#define KILL_BASE_COST (options.kill_default_cost)
#define FIND_COST (options.find_cost)
#define QUOTA_COST (options.quota_cost)
#define QUEUE_COST (options.queue_cost)
#define ROOM_COST (options.room_cost)
#define LINK_COST (options.link_cost)
#define EXIT_COST (options.exit_cost)
#define OBJECT_COST (options.object_cost)
#define GOD ((dbref) 1)
#define ANNOUNCE_CONNECTS (options.announce_connects)
#define ACCESS_FILE (options.access_file)
#define NAMES_FILE (options.names_file)
#define SILENT_PEMIT (options.silent_pemit)
#define PLAYER_LISTEN (options.player_listen)
#define PLAYER_AHEAR (options.player_ahear)
#define STARTUPS (options.startups)
#define FULL_INVIS (options.full_invis)
#define EX_PUBLIC_ATTRIBS (options.ex_public_attribs)
#define FLAGS_ON_EXAMINE (options.flags_on_examine)
#define FREE_OBJECTS (options.free_objects)
#define RESTRICTED_BUILDING (options.restrict_building)
#define NO_FORK (!options.forking_dump)
#define PLAYER_NAME_SPACES (options.player_name_spaces)
#define MAX_ALIASES (options.max_aliases)
#define SAFER_UFUN (options.safer_ufun)
#define NOISY_WHISPER (options.noisy_whisper)
#define POSSESSIVE_GET (options.possessive_get)
#define POSSGET_ON_DISCONNECTED (options.possessive_get_d)
#define REALLY_SAFE (options.really_safe)
#define DESTROY_POSSESSIONS (options.destroy_possessions)
#define NULL_EQ_ZERO (options.null_eq_zero)
#define TINY_BOOLEANS (options.tiny_booleans)
#define TINY_TRIM_FUN (options.tiny_trim_fun)
#define ADESTROY_ATTR (options.adestroy)
#define AMAIL_ATTR (options.amail)
#define MAIL_LIMIT (options.mail_limit)
#define ROOM_CONNECTS (options.room_connects)
#define ANSI_NAMES (options.ansi_names)
#define COMMA_EXIT_LIST (options.comma_exit_list)
#define COUNT_ALL (options.count_all)
#define EXITS_CONNECT_ROOMS (options.exits_connect_rooms)
#define ZONE_CONTROL_ZMP (options.zone_control)
#define WIZWALL_PREFIX (options.wizwall_prefix)
#define RWALL_PREFIX (options.rwall_prefix)
#define CHAT_STRIP_QUOTE (options.chat_strip_quote)
#define WALL_PREFIX (options.wall_prefix)
#define NO_LINK_TO_OBJECT (!options.link_to_object)
#define QUEUE_PER_OWNER (options.owner_queues)
#define WIZ_NOAENTER (options.wiz_noaenter)
#define USE_DNS (options.use_dns)
#define MUSH_IP_ADDR (options.ip_addr)
#define SSL_IP_ADDR (options.ssl_ip_addr)
#define MAX_ATTRCOUNT (options.max_attrcount)
#define HARD_MAX_ATTRCOUNT 1000000
#define FLOAT_PRECISION (options.float_precision)
#define RECURSION_LIMIT (options.func_nest_lim)
#define FUNCTION_LIMIT (options.func_invk_lim)
#define CALL_LIMIT (options.call_lim)
#define TINY_MATH (options.tiny_math)
#define ONLY_ASCII_NAMES (options.ascii_names)
#define USE_QUOTA (options.use_quota)
#define EMPTY_ATTRS (options.empty_attrs)
#define FUNCTION_SIDE_EFFECTS (options.function_side_effects)
#define ERRLOG (options.error_log)
#define CONNLOG (options.connect_log)
#define WIZLOG (options.wizard_log)
#define CMDLOG (options.command_log)
#define TRACELOG (options.trace_log)
#define CHECKLOG (options.checkpt_log)
#define SQL_PLATFORM (options.sql_platform)
#define SQL_HOST (options.sql_host)
#define SQL_DB (options.sql_database)
#define SQL_USER (options.sql_username)
#define SQL_PASS (options.sql_password)

#define CHUNK_SWAP_FILE (options.chunk_swap_file)
#define CHUNK_CACHE_MEMORY (options.chunk_cache_memory)
#define CHUNK_MIGRATE_AMOUNT (options.chunk_migrate_amount)

#define READ_REMOTE_DESC (options.read_remote_desc)

typedef struct globals_table GLOBALTAB;

/** State of the MUSH
 * There is only ever one instance of this struct, which holds
 * info on the current state of the MUSH in memory
 */
struct globals_table {
  int database_loaded;        /**< True after the database has been read. */
  char dumpfile[200];         /**< File name to dump database to */
  time_t start_time;          /**< MUSH start time (since process exec'd) */
  time_t first_start_time;    /**< MUSH start time (since last shutdown) */
  time_t last_dump_time;      /**< Time of last successful db save */
  int reboot_count;           /**< Number of reboots so far */
  int paranoid_dump;          /**< if paranoid, scan before dumping */
  int paranoid_checkpt;       /**< write out an okay message every x objs */
  long indb_flags;            /**< flags set in the input database */
  int on_second;              /**< is it time for per-second processes? */
};

extern GLOBALTAB globals;

#endif                          /* __PENN_CONF_H */
