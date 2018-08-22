/**
 * \file externs.h
 *
 * \brief Header file for external functions called from many source files.
 *
 *
 */

#ifndef __EXTERNS_H
#define __EXTERNS_H
/* Get the time_t definition that we use in prototypes here */
#include <time.h>
#include <stdarg.h>

#include "compile.h"
#include "copyrite.h"
#include "dbdefs.h"
#include "mushtype.h"
#include "mypcre.h"

/* these symbols must be defined by the interface */
extern time_t mudtime;

#define FOPEN_READ "rb"  /**< Arguments to fopen when reading */
#define FOPEN_WRITE "wb" /**< Arguments to fopen when writing */

extern int shutdown_flag; /* if non-zero, interface should shut down */
void emergency_shutdown(void);
void boot_desc(DESC *d, const char *cause,
               dbref executor); /* remove a player */
int boot_player(dbref player, int idleonly, int slilent, dbref booter);
const char *sockset(DESC *d, char *name, char *val);
const char *sockset_show(DESC *d, char *nl);

DESC *player_desc(dbref player);   /* find descriptors */
DESC *inactive_desc(dbref player); /* find descriptors */
DESC *port_desc(int port);         /* find descriptors */
void WIN32_CDECL flag_broadcast(const char *flag1, const char *flag2,
                                const char *fmt, ...)
  __attribute__((__format__(__printf__, 3, 4)));

dbref short_page(const char *match);
dbref visible_short_page(dbref player, const char *match);

/* the following symbols are provided by game.c */
void process_command(dbref executor, char *command, MQUE *queue_entry);
int init_game_dbs(void);
void init_game_postdb(const char *conf);
void init_game_config(const char *conf);
void dump_database(void);
void NORETURN mush_panic(const char *message);
void NORETURN mush_panicf(const char *fmt, ...)
  __attribute__((__format__(__printf__, 1, 2)));
char *scan_list(dbref executor, dbref looker, char *command, int flag);

#ifdef WIN32
/* From timer.c */
void init_timer(void);
#endif /* WIN32 */

/* From bsd.c */
extern FILE *connlog_fp;
extern FILE *checklog_fp;
extern FILE *wizlog_fp;
extern FILE *tracelog_fp;
extern FILE *cmdlog_fp;
extern int restarting;
int hidden(dbref player);
dbref guest_to_connect(dbref player);
void dump_reboot_db(void);
void close_ssl_connections(void);
DESC *least_idle_desc(dbref player, int priv);
int least_idle_time(dbref player);
int least_idle_time_priv(dbref player);
int most_conn_time(dbref player);
int most_conn_time_priv(dbref player);
char *least_idle_ip(dbref player);
char *least_idle_hostname(dbref player);
void do_who_mortal(dbref player, char *name);
void do_who_admin(dbref player, char *name);
void do_who_session(dbref player, char *name);
char *json_unescape_string(char *input);
char *json_escape_string(char *input);
void register_gmcp_handler(char *package, gmcp_handler_func func);
void send_oob(DESC *d, char *package, cJSON *data);

/* sql.c */
void sql_shutdown(void);

/* From command.c */
void generic_command_failure(dbref executor, dbref enactor, char *string,
                             MQUE *queue_entry);

/* From compress.c */
/* Define this to get some statistics on the attribute compression
 * in @stats/tables. Only for word-based compression (COMPRESSION_TYPE 3 or 4)
 */
/* #define COMP_STATS /* */

bool init_compress(PENNFILE *);
char *safe_uncompress(char const *) __attribute_malloc__;
char *text_uncompress(char const *);
char *text_compress(char const *) __attribute_malloc__;
#define compress text_compress
#define uncompress text_uncompress

/* From cque.c */

/* Queue types/flags */
#define QUEUE_DEFAULT                                                          \
  0x0000 /**< select QUEUE_PLAYER or QUEUE_OBJECT based on enactor's Type() */
#define QUEUE_PLAYER 0x0001 /**< command queued because of a player in-game */
#define QUEUE_OBJECT                                                           \
  0x0002 /**< command queued because of a non-player in-game */
#define QUEUE_SOCKET                                                           \
  0x0004 /**< command executed directly from a player's client */
#define QUEUE_INPLACE 0x0008 /**< inplace queue entry */
#define QUEUE_NO_BREAKS                                                        \
  0x0010 /**< Don't propagate \@breaks from this inplace queue */
#define QUEUE_PRESERVE_QREG                                                    \
  0x0020 /**< Preserve/restore q-registers before/after running this inplace   \
            queue */
#define QUEUE_CLEAR_QREG                                                       \
  0x0040 /**< Clear q-registers before running this inplace queue */
#define QUEUE_PROPAGATE_QREG                                                   \
  0x0080 /**< At the end of this inplace queue entry, copy our q-registers     \
            into the parent queue entry */
#define QUEUE_RESTORE_ENV 0x0100 /**< Obsolete */
#define QUEUE_NOLIST                                                           \
  0x0200 /**< Don't separate commands at semicolons, and don't parse rhs in    \
            &attr setting */
#define QUEUE_BREAK                                                            \
  0x0400 /**< set by \@break, stops further processing of queue entry */
#define QUEUE_RETRY                                                            \
  0x0800 /**< Set by \@retry, restart current queue entry from beginning,      \
            without recalling do_entry */
#define QUEUE_DEBUG                                                            \
  0x1000 /**< Show DEBUG info for queue (queued from a DEBUG attribute) */
#define QUEUE_NODEBUG                                                          \
  0x2000 /**< Don't show DEBUG info for queue (queued from a NO_DEBUG          \
            attribute) */
#define QUEUE_PRIORITY                                                         \
  0x4000 /**< Add to the priority (player) queue, even if from a non-player.   \
            For \@startups */
#define QUEUE_DEBUG_PRIVS                                                      \
  0x8000 /**< Show DEBUG info for queue if on an object %# can see debug from  \
            (queued via %-prefix) */
#define QUEUE_EVENT 0x10000 /**< Action list queued by the Event system */

#define QUEUE_RECURSE (QUEUE_INPLACE | QUEUE_NO_BREAKS | QUEUE_PRESERVE_QREG)

/* Used when generating a new pe_info from an existing pe_info. Used by
 * pe_info_from(). */
#define PE_INFO_DEFAULT 0x000  /**< create a new, empty pe_info */
#define PE_INFO_SHARE 0x001    /**< Share the existing pe_info */
#define PE_INFO_CLONE 0x002    /**< Clone entire pe_info */
#define PE_INFO_COPY_ENV 0x004 /**< Copy env-vars (%0-%9) from the parent */
#define PE_INFO_COPY_QREG                                                      \
  0x008 /**< Copy q-registers (%q*) from the parent pe_info */
#define PE_INFO_COPY_CMDS                                                      \
  0x010 /**< Copy values for %c and %u from the parent pe_info */

struct _ansi_string;

void do_second(void);
int do_top(int ncom);
void do_halt(dbref owner, const char *ncom, dbref victim);
#define SYSEVENT -1
bool queue_event(dbref enactor, const char *event, const char *fmt, ...)
  __attribute__((__format__(__printf__, 3, 4)));
void parse_que_attr(dbref executor, dbref enactor, char *actionlist,
                    PE_REGS *pe_regs, ATTR *a, bool force_debug);
void insert_que(MQUE *queue_entry, MQUE *parent_queue);

void new_queue_actionlist_int(dbref executor, dbref enactor, dbref caller,
                              char *actionlist, MQUE *queue_entry, int flags,
                              int queue_type, PE_REGS *pe_regs, char *fromattr);
#define parse_que(executor, enactor, actionlist, regs)                         \
  new_queue_actionlist(executor, enactor, enactor, actionlist, NULL,           \
                       PE_INFO_DEFAULT, QUEUE_DEFAULT, regs)
#define new_queue_actionlist(executor, enactor, caller, actionlist,            \
                             parent_queue, flags, queue_type, regs)            \
  new_queue_actionlist_int(executor, enactor, caller, actionlist,              \
                           parent_queue, flags, queue_type, regs, NULL)

int queue_attribute_base_priv(dbref executor, const char *atrname,
                              dbref enactor, int noparent, PE_REGS *pe_regs,
                              int flags, dbref priv, MQUE *parent_queue,
                              const char *input);
ATTR *queue_attribute_getatr(dbref executor, const char *atrname, int noparent);
int queue_attribute_useatr(dbref executor, ATTR *a, dbref enactor,
                           PE_REGS *pe_regs, int flags, MQUE *parent_queue,
                           const char *input);
int queue_include_attribute(dbref thing, const char *atrname, dbref executor,
                            dbref cause, dbref caller, char **args, int recurse,
                            MQUE *parent_queue);
void run_user_input(dbref player, int port, char *input);
void run_http_command(dbref player, int port, char *method,
                      NEW_PE_INFO *pe_info);

#define queue_attribute_base(ex, at, en, nop, pereg, flag)                     \
  queue_attribute_base_priv(ex, at, en, nop, pereg, flag, NOTHING, NULL, NULL)
/** Queue the code in an attribute, including parent objects */
#define queue_attribute(a, b, c)                                               \
  queue_attribute_base_priv(a, b, c, 0, NULL, 0, NOTHING, NULL, NULL)
/** Queue the code in an attribute, excluding parent objects */
#define queue_attribute_noparent(a, b, c)                                      \
  queue_attribute_base_priv(a, b, c, 1, NULL, 0, NOTHING, NULL, NULL)
void dequeue_semaphores(dbref thing, char const *aname, int count, int all,
                        int drain);
void shutdown_queues(void);

/* From create.c */
dbref do_dig(dbref player, const char *name, char **argv, int tport,
             NEW_PE_INFO *pe_info);
dbref do_create(dbref player, char *name, int cost, char *newdbref);
dbref do_real_open(dbref player, const char *direction, const char *linkto,
                   dbref pseudo, NEW_PE_INFO *pe_info);
void do_open(dbref player, const char *direction, char **links,
             NEW_PE_INFO *pe_info);
int do_link(dbref player, const char *name, const char *room_name, int preserve,
            NEW_PE_INFO *pe_info);
void do_unlink(dbref player, const char *name);
dbref do_clone(dbref player, char *name, char *newname, bool preserve,
               char *newdbref, NEW_PE_INFO *pe_info);

/* From funtime.c */
int etime_to_secs(char *input, int *secs, bool default_minutes);

/* From game.c */
void report(void);
int Hearer(dbref thing);
int Commer(dbref thing);
int Listener(dbref thing);
int parse_chat(dbref player, char *command);
bool fork_and_dump(int forking);
void reserve_fd(void);
void release_fd(void);
void do_scan(dbref player, char *command, int flag);

/* From look.c */
#define LOOK_NORMAL 0      /* You typed "look" */
#define LOOK_AUTO 1        /* Moving into a room */
#define LOOK_CLOUDY 2      /* Looking through an exit set CLOUDY */
#define LOOK_TRANS 4       /* Looking through an exit set TRANSPARENT */
#define LOOK_OUTSIDE 8     /* Using look/outside */
#define LOOK_NOCONTENTS 16 /* Using look/opaque */
#define LOOK_CLOUDYTRANS (LOOK_CLOUDY | LOOK_TRANS)

void look_room(dbref player, dbref loc, int key, NEW_PE_INFO *pe_info);
void do_look_around(dbref player);
void do_look_at(dbref player, const char *name, int key, NEW_PE_INFO *pe_info);
char *decompose_str(char *what);
int safe_decompose_str(char *what, char *buf, char **bp);

/* From move.c */
void enter_room(dbref player, dbref loc, int nomovemsgs, dbref enactor,
                const char *cause);
int can_move(dbref player, const char *direction);
/** Enumeration of types of movements that can be performed */
enum move_type {
  MOVE_NORMAL,
  /**< move through an exit in your location */
  MOVE_GLOBAL,  /**< Master Room exit */
  MOVE_ZONE,    /**< ZMR Exit */
  MOVE_TELEPORT /**< \@tel'd into an exit */
};
void do_move(dbref player, const char *direction, enum move_type type,
             NEW_PE_INFO *pe_info);
void moveto(dbref what, dbref where, dbref enactor, const char *cause);
void safe_tel(dbref player, dbref dest, int nomovemsgs, dbref enactor,
              const char *cause);
int global_exit(dbref player, const char *direction);
int remote_exit(dbref loc, const char *direction);
void move_wrapper(dbref player, const char *command, NEW_PE_INFO *pe_info);
void do_follow(dbref player, const char *arg, NEW_PE_INFO *pe_info);
void do_unfollow(dbref player, const char *arg);
void do_desert(dbref player, const char *arg);
void do_dismiss(dbref player, const char *arg);
void clear_followers(dbref leader, int noisy);
void clear_following(dbref follower, int noisy);
dbref find_var_dest(dbref player, dbref exit_obj, char *exit_name,
                    NEW_PE_INFO *pe_info);

/* From player.c */
extern const char *connect_fail_limit_exceeded;
int mark_failed(const char *ipaddr);
int count_failed(const char *ipaddr);
int check_fails(const char *ipaddr);
bool password_check(dbref player, const char *password);
dbref lookup_player(const char *name);
dbref lookup_player_name(const char *name);
/* from player.c */
dbref create_player(DESC *d, dbref executor, const char *name,
                    const char *password, const char *host, const char *ip);
dbref connect_player(DESC *d, const char *name, const char *password,
                     const char *host, const char *ip, char *errbuf);
void check_last(dbref player, const char *host, const char *ip);
void check_lastfailed(dbref player, const char *host);

/* From parse.c */
bool is_number(const char *str);
bool is_strict_number(const char *str);
bool is_strict_integer(const char *str);
bool is_strict_int64(const char *str);
bool is_integer_list(const char *str);
#ifdef HAVE_ISNORMAL
#define is_good_number(n) isnormal((n))
#else
bool is_good_number(NVAL val);
#endif

/* From plyrlist.c */
void clear_players(void);
void add_player(dbref player);
void add_player_alias(dbref player, const char *alias, bool intransaction);
void delete_player(dbref player);
void reset_player_list(dbref player, const char *name, const char *alias);

int could_doit(dbref player, dbref thing, NEW_PE_INFO *pe_info);
int did_it(dbref player, dbref thing, const char *what, const char *def,
           const char *owhat, const char *odef, const char *awhat, dbref loc,
           int an_flags);
int did_it_with(dbref player, dbref thing, const char *what, const char *def,
                const char *owhat, const char *odef, const char *awhat,
                dbref loc, dbref env0, dbref env1, int flags, int an_flags);
int did_it_interact(dbref player, dbref thing, const char *what,
                    const char *def, const char *owhat, const char *odef,
                    const char *awhat, dbref loc, int flags, int an_flags);
int real_did_it(dbref player, dbref thing, const char *what, const char *def,
                const char *owhat, const char *odef, const char *awhat,
                dbref loc, PE_REGS *pe_regs, int flags, int an_flags);
int can_see(dbref player, dbref thing, int can_see_loc);
int controls(dbref who, dbref what);
int can_pay_fees(dbref who, int pennies);
void giveto(dbref who, int pennies);
int payfor(dbref who, int cost);
int quiet_payfor(dbref who, int cost);
int nearby(dbref obj1, dbref obj2);
int get_current_quota(dbref who);
void change_quota(dbref who, int payment);
int ok_name(const char *name, int is_exit);
int ok_command_name(const char *name);
int ok_function_name(const char *name);
int ok_player_name(const char *name, dbref player, dbref thing);
/** Errors from ok_player_alias */
enum opa_error {
  OPAE_SUCCESS = 0,
  /**< Success */
  OPAE_INVALID,
  /**< Invalid alias */
  OPAE_TOOMANY,
  /**< Too many aliases already set */
  OPAE_NULL
  /**< Null alias */
};
enum opa_error ok_player_alias(const char *alias, dbref player, dbref thing);
enum opa_error ok_object_name(char *name, dbref player, dbref thing, int type,
                              char **newname, char **newalias);
int ok_password(const char *password);
int ok_tag_attribute(dbref player, const char *params);
dbref parse_match_possessor(dbref player, char **str, int exits);
void page_return(dbref player, dbref target, const char *type,
                 const char *message, const char *def, NEW_PE_INFO *pe_info);
dbref where_is(dbref thing);
int charge_action(dbref thing);
dbref first_visible(dbref player, dbref thing);

#define GREP_NOCASE 1 /**< Grep pattern is case-insensitive */
#define GREP_WILD 2   /**< Grep pattern is a glob pattern */
#define GREP_REGEXP 4 /**< Grep pattern is a regexp */
#define GREP_PARENT 8 /**< Check parent objects when grepping */
int grep_util(dbref player, dbref thing, char *attrs, char *findstr, char *buff,
              char **bp, int flags);

/* From rob.c */
void s_Pennies(dbref thing, int amount);

/* From set.c */
void chown_object(dbref player, dbref thing, dbref newowner, int preserve);
void do_include(dbref executor, dbref enactor, char *object, char **argv,
                int recurse, MQUE *parent_queue);
/* From speech.c */

/**< Type of emit to do. Used by \@message */
enum emit_type {
  EMIT_PEMIT,
  /**< pemit to given objects */
  EMIT_REMIT,
  /**< remit in given rooms */
  EMIT_OEMIT
  /**< emit to all objects in location except the given objects */
};

enum msgformat_response {
  MSGFORMAT_NONE = 0,
  /**< No messageformat set */
  MSGFORMAT_SENT = 1,
  /**< Message sent to player */
  MSGFORMAT_NULL = 2
  /**< Attribute existed but eval'd null */
};
dbref speech_loc(dbref thing);
int okay_pemit(dbref player, dbref target, int dofails, int def,
               NEW_PE_INFO *pe_info);
enum msgformat_response vmessageformat(dbref player, const char *attribute,
                                       dbref executor, int flags, int nargs,
                                       ...);
enum msgformat_response messageformat(dbref player, const char *attribute,
                                      dbref executor, int flags, int nargs,
                                      char *argv[]);
void do_message(dbref executor, dbref speaker, char *list, char *attrname,
                char *message, enum emit_type type, int flags, int numargs,
                char *argv[], NEW_PE_INFO *pe_info);

const char *spname_int(dbref thing, bool ansi);
#define spname(thing) spname_int(thing, 1)
int filter_found(dbref thing, dbref speaker, const char *msg, int flag);

/** This structure associates html entities and base ascii representations */
typedef struct {
  const char *base;   /**< Base ascii representation */
  const char *entity; /**< HTML entity */
} accent_info;

extern accent_info accent_table[];

/* From unparse.c */
const char *real_unparse(dbref player, dbref loc, int obey_myopic,
                         int use_nameformat, int use_nameaccent, int an_flag,
                         NEW_PE_INFO *pe_info);
extern const char *unparse_objid(dbref thing);
extern const char *unparse_object(dbref player, dbref loc, int an_flag);
/** For back compatibility, an alias for unparse_object */
#define object_header(p, l) unparse_object(p, l, AN_UNPARSE)
const char *unparse_object_myopic(dbref player, dbref loc, int an_flag);
const char *unparse_room(dbref player, dbref loc, NEW_PE_INFO *pe_info);
int nameformat(dbref player, dbref loc, char *tbuf1, char *defname,
               bool localize, NEW_PE_INFO *pe_info);
const char *accented_name(dbref thing);

/* From utils.c */
void parse_attrib(dbref player, char *str, dbref *thing, ATTR **attrib);
uint64_t now_msecs(); /* current milliseconds */
#define SECS_TO_MSECS(x) ((x) *1000UL)
#ifdef WIN32
void penn_gettimeofday(struct timeval *now); /* For platform agnosticism */
#else
#define penn_gettimeofday(now) gettimeofday((now), (struct timezone *) NULL)
#endif

/** Information about an attribute to ufun.
 * Prepared via fetch_ufun_attrib, used in call_ufun
 */
typedef struct _ufun_attrib {
  dbref thing;                            /**< Object with attribute */
  char contents[BUFFER_LEN + SSE_OFFSET]; /**< Attribute value */
  char attrname[ATTRIBUTE_NAME_LIMIT + 1];
  /**< Name of attribute */
  int pe_flags; /**< Flags to use when evaluating attr (for debug, no_debug) */
  const char *errmess; /**< Error message, if attr couldn't be retrieved */
  int ufun_flags;      /**< UFUN_* flags, for how to parse/eval the attr */
} ufun_attrib;

dbref next_parent(dbref thing, dbref current, int *parent_count,
                  int *use_ancestor);

/* Only 'attr', not 'obj/attr' */
#define UFUN_NONE 0
/* Does this string accept obj/attr? */
#define UFUN_OBJECT 0x01
/* If it accepts obj/attr, does it accept #lambda/attr? */
#define UFUN_LAMBDA 0x02
/* If this is set, a nonexistant attribute is an error, instead of empty. */
#define UFUN_REQUIRE_ATTR 0x04
/* When calling the ufun, don't check caller's perms */
#define UFUN_IGNORE_PERMS 0x08
/* When calling the ufun, save and restore the Q-registers. */
#define UFUN_LOCALIZE 0x10
/* When calling the ufun, add the object's name to the beginning, respecting
 * no_name */
#define UFUN_NAME 0x20
/* When calling ufun with UFUN_NAME, don't add a space after the name. Only to
 * be used by call_ufun! */
#define UFUN_NAME_NOSPACE 0x40
#define UFUN_DEFAULT (UFUN_OBJECT | UFUN_LAMBDA)
/* Don't localize %0-%9. For use in evaluation locks */
#define UFUN_SHARE_STACK 0x80
bool fetch_ufun_attrib(const char *attrstring, dbref executor,
                       ufun_attrib *ufun, int flags);
bool call_ufun_int(ufun_attrib *ufun, char *ret, dbref caller, dbref enactor,
                   NEW_PE_INFO *pe_info, PE_REGS *pe_regs, void *data);
#define call_ufun(ufun, ret, caller, enactor, pe_info, pe_regs)                \
  call_ufun_int(ufun, ret, caller, enactor, pe_info, pe_regs, NULL)
bool call_attrib(dbref thing, const char *attrname, char *ret, dbref enactor,
                 NEW_PE_INFO *pe_info, PE_REGS *pe_regs);
bool member(dbref thing, dbref list);
bool recursive_member(dbref disallow, dbref from, int count);
dbref remove_first(dbref first, dbref what);
dbref reverse(dbref list);
void initialize_rng(void);
uint32_t get_random_u32(uint32_t low, uint32_t high);
double get_random_d(void);
double get_random_d2(void);
char *fullalias(dbref it);
char *shortalias(dbref it);
char *shortname(dbref it);
dbref absolute_room(dbref it);
int can_interact(dbref from, dbref to, int type, NEW_PE_INFO *pe_info);

char *ansi_name(dbref thing, bool accents, bool *had_moniker, int maxlen);
/* From warnings.c */
void run_topology(void);
void do_warnings(dbref player, const char *name, const char *warns);
void do_wcheck(dbref player, const char *name);
void do_wcheck_me(dbref player);
void do_wcheck_all(dbref player);
void set_initial_warnings(dbref player);
const char *unparse_warnings(warn_type warnings);
warn_type parse_warnings(dbref player, const char *warnings);

/* From wild.c */
bool wild_match_test(const char *restrict s, const char *restrict d, bool cs,
                     int *matches, int nmatches);
bool local_wild_match_case(const char *restrict s, const char *restrict d,
                           bool cs, PE_REGS *pe_regs);
int wildcard_count(char *s, bool unescape);
/** Return 1 if s contains unescaped wildcards, 0 if not */
#define wildcard(s) (wildcard_count(s, 0) == -1)
bool quick_wild_new(const char *restrict tstr, const char *restrict dstr,
                    bool cs);
bool wild_match_case_r(const char *restrict s, const char *restrict d, bool cs,
                       char **ary, int max, char *ata, int len,
                       PE_REGS *pe_regs, int pe_reg_flags);
bool quick_wild(const char *restrict tsr, const char *restrict dstr);
bool atr_wild(const char *restrict tstr, const char *restrict dstr);

bool regexp_match_case_r(const char *restrict s, const char *restrict d,
                         bool cs, char **, size_t, char *restrict, ssize_t,
                         PE_REGS *pe_regs, int pe_reg_flags);
bool quick_regexp_match(const char *restrict s, const char *restrict d, bool cs,
                        const char **report_err);
bool qcomp_regexp_match(const pcre2_code *re, pcre2_match_data *md,
                        const char *s, PCRE2_SIZE);
/** Default (case-insensitive) local wildcard match */
#define local_wild_match(s, d, p) local_wild_match_case(s, d, 0, p)

/** Types of lists */

extern const char ALPHANUM_LIST[];
extern const char INSENS_ALPHANUM_LIST[];
extern const char DBREF_LIST[];
extern const char NUMERIC_LIST[];
extern const char FLOAT_LIST[];
extern const char DBREF_NAME_LIST[];
extern const char DBREF_NAMEI_LIST[];
extern const char DBREF_IDLE_LIST[];
extern const char DBREF_CONN_LIST[];
extern const char DBREF_CTIME_LIST[];
extern const char DBREF_OWNER_LIST[];
extern const char DBREF_LOCATION_LIST[];
extern const char DBREF_ATTR_LIST[];
extern const char DBREF_ATTRI_LIST[];
extern const char *const UNKNOWN_LIST;

/* From function.c and other fun*.c */
char *strip_braces(char const *line);

int delim_check(char *buff, char **bp, int nfargs, char **fargs, int sep_arg,
                char *sep);
bool int_check(char *buff, char **bp, int nfargs, char *fargs[], int check_arg,
               int *result, int def);

int get_gender(dbref player);
const char *do_get_attrib(dbref executor, dbref thing, const char *aname);

/* From destroy.c */
void do_undestroy(dbref player, char *name);
dbref free_get(void);
int make_first_free(dbref object);
int make_first_free_wrapper(dbref player, char *newdbref);
void fix_free_list(void);
void purge(void);
void do_purge(dbref player);

void dbck(void);
int undestroy(dbref player, dbref thing);

/* From db.c */

const char *set_name(dbref obj, const char *newname);
dbref new_object(void);

/* From filecopy.c */
int rename_file(const char *origname, const char *newname);
int trunc_file(FILE *);
int copy_file(FILE *, const char *, bool);
int copy_to_file(const char *, FILE *);
bool file_exists(const char *);

/* local.c */
void local_startup(void);
void local_configs(void);
void local_locks(void);
void local_dump_database(void);
void local_dbck(void);
void local_shutdown(void);
bool local_timer(void *data);
void local_connect(dbref player, int isnew, int num);
void local_disconnect(dbref player, int num);
void local_data_create(dbref object);
void local_data_clone(dbref clone, dbref source, int preserve);
void local_data_free(dbref object);
int local_can_interact_first(dbref from, dbref to, int type);
int local_can_interact_last(dbref from, dbref to, int type);

/* flaglocal.c */
void local_flags(FLAGSPACE *flags);

/* Functions for suggesting alternatives to misspelled names */
void init_private_vocab(void);
char *suggest_name(const char *name, const char *category);
void add_private_vocab(const char *name, const char *category);
void delete_private_vocab(const char *name, const char *category);
void delete_private_vocab_cat(const char *category);

char *suggest_word(const char *name, const char *category);
void add_dict_words(void);

#endif /* __EXTERNS_H */
