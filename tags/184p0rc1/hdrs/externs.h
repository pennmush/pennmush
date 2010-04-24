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
#ifdef I_LIBINTL
#include <libintl.h>
#endif
#if defined(HAVE_GETTEXT) && !defined(DONT_TRANSLATE)
/** Macro for a translated string */
#define T(str) gettext(str)
/** Macro to note that a string has a translation but not to translate */
#define N_(str) gettext_noop(str)
#else
#define T(str) str
#define N_(str) str
#endif
#include "config.h"
#include "copyrite.h"
#include "compile.h"
#include "mushtype.h"
#include "dbdefs.h"
#include "confmagic.h"
#include "mypcre.h"

#ifndef HAVE_STRCASECMP
#ifdef HAVE__STRICMP
#define strcasecmp(s1,s2) _stricmp((s1), (s2))
#else
int strcasecmp(const char *s1, const char *s2);
#endif
#endif

#ifndef HAVE_STRNCASECMP
#ifdef HAVE__STRNICMP
#define strncasecmp(s1,s2,n) _strnicmp((s1), (s2), (n))
#else
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
#endif

/* these symbols must be defined by the interface */
extern time_t mudtime;

#define FOPEN_READ "rb"      /**< Arguments to fopen when reading */
#define FOPEN_WRITE "wb"     /**< Arguments to fopen when writing */

extern int shutdown_flag;       /* if non-zero, interface should shut down */
void emergency_shutdown(void);
void boot_desc(DESC *d);        /* remove a player */
int boot_player(dbref player, int idleonly, int slilent);
DESC *player_desc(dbref player);        /* find descriptors */
DESC *inactive_desc(dbref player);      /* find descriptors */
DESC *port_desc(int port);      /* find descriptors */
void WIN32_CDECL flag_broadcast(const char *flag1,
                                const char *flag2, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 3, 4)));

void raw_notify(dbref player, const char *msg);
void notify_list(dbref speaker, dbref thing, const char *atr,
                 const char *msg, int flags);
dbref short_page(const char *match);
dbref visible_short_page(dbref player, const char *match);
void do_doing(dbref player, const char *message);

/* the following symbols are provided by game.c */
void process_command(dbref player, char *command, dbref cause, int from_port);
int init_game_dbs(void);
void init_game_postdb(const char *conf);
void init_game_config(const char *conf);
void dump_database(void);
void NORETURN mush_panic(const char *message);
void NORETURN mush_panicf(const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 1, 2)));
char *scan_list(dbref player, char *command);


#ifdef WIN32
/* From timer.c */
void init_timer(void);
#endif                          /* WIN32 */

/* From log.c */
void penn_perror(const char *);

/* From wait.c */
int lock_file(FILE *);
int unlock_file(FILE *);

/* From bsd.c */
extern FILE *connlog_fp;
extern FILE *checklog_fp;
extern FILE *wizlog_fp;
extern FILE *tracelog_fp;
extern FILE *cmdlog_fp;
extern int restarting;
#ifdef SUN_OS
int f_close(FILE * file);
/** SunOS fclose macro */
#define fclose(f) f_close(f);
#endif
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
/* sql.c */
void sql_shutdown(void);

/* The #defs for our notify_anything hacks.. Errr. Functions */
#define NA_NORELAY      0x0001  /**< Don't relay sound */
#define NA_NOENTER      0x0002  /**< No newline at end */
#define NA_NOLISTEN     0x0004  /**< Implies NORELAY. Sorta. */
#define NA_NOPENTER     0x0010  /**< No newline, Pueblo-stylee */
#define NA_PONLY        0x0020  /**< Pueblo-only */
#define NA_PUPPET       0x0040  /**< Ok to puppet */
#define NA_PUPPET2      0x0080  /**< Message to a player from a puppet */
#define NA_MUST_PUPPET  0x0100  /**< Ok to puppet even in same room */
#define NA_INTER_HEAR   0x0200  /**< Message is auditory in nature */
#define NA_INTER_SEE    0x0400  /**< Message is visual in nature */
#define NA_INTER_PRESENCE  0x0800       /**< Message is about presence */
#define NA_NOSPOOF        0x1000        /**< Message comes via a NO_SPOOF object. */
#define NA_PARANOID       0x2000        /**< Message comes via a PARANOID object. */
#define NA_NOPREFIX       0x4000        /**< Don't use @prefix when forwarding */
#define NA_SPOOF        0x8000  /**< @ns* message, overrides NOSPOOF */
#define NA_INTER_LOCK  0x10000  /**< Message subject to @lock/interact even if not otherwise marked */
#define NA_INTERACTION  (NA_INTER_HEAR|NA_INTER_SEE|NA_INTER_PRESENCE|NA_INTER_LOCK)    /**< Message follows interaction rules */
#define NA_PROMPT       0x20000  /**< Message is a prompt, add GOAHEAD */

/** A notify_anything lookup function type definition */
typedef dbref (*na_lookup) (dbref, void *);
void notify_anything(dbref speaker, na_lookup func,
                     void *fdata,
                     char *(*nsfunc) (dbref,
                                      na_lookup func,
                                      void *, int), int flags,
                     const char *message);
void notify_anything_format(dbref speaker, na_lookup func,
                            void *fdata,
                            char *(*nsfunc) (dbref,
                                             na_lookup func,
                                             void *, int), int flags,
                            const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 6, 7)));
void notify_anything_loc(dbref speaker, na_lookup func,
                         void *fdata,
                         char *(*nsfunc) (dbref,
                                          na_lookup func,
                                          void *, int), int flags,
                         const char *message, dbref loc);
dbref na_one(dbref current, void *data);
dbref na_next(dbref current, void *data);
dbref na_loc(dbref current, void *data);
dbref na_nextbut(dbref current, void *data);
dbref na_except(dbref current, void *data);
dbref na_except2(dbref current, void *data);
dbref na_exceptN(dbref current, void *data);
dbref na_channel(dbref current, void *data);
char *ns_esnotify(dbref speaker, na_lookup func, void *fdata, int para);

/** Basic 'notify player with message */
#define notify(p,m)           notify_anything(orator, na_one, &(p), NULL, 0, m)
/** Notify player as a prompt */
#define notify_prompt(p,m)           notify_anything(orator, na_one, &(p), NULL, NA_PROMPT, m)
/** Notify puppet with message, even if owner's there */
#define notify_must_puppet(p,m)           notify_anything(orator, na_one, &(p), NULL, NA_MUST_PUPPET, m)
/** Notify puppet with message as prompt, even if owner's there */
#define notify_prompt_must_puppet(p,m)           notify_anything(orator, na_one, &(p), NULL, NA_MUST_PUPPET|NA_PROMPT, m)
/** Notify player with message, as if from somethign specific */
#define notify_by(t,p,m)           notify_anything(t, na_one, &(p), NULL, 0, m)
/** Notfy player with message, but only puppet propagation */
#define notify_noecho(p,m)    notify_anything(orator, na_one, &(p), NULL, NA_NORELAY | NA_PUPPET, m)
/** Notify player with message if they're not set QUIET */
#define quiet_notify(p,m)     if (!IsQuiet(p)) notify(p,m)
/** Notify player but don't send \n */
#define notify_noenter_by(t,a,b) notify_anything(t, na_one, &(a), NULL, NA_NOENTER, b)
#define notify_noenter(a,b) notify_noenter_by(GOD, a, b)
/** Notify player but don't send <BR> if they're using Pueblo */
#define notify_nopenter_by(t,a,b) notify_anything(t, na_one, &(a), NULL, NA_NOPENTER, b)
#define notify_nopenter(a,b) notify_nopenter_by(GOD, a, b)
/* Notify with a printf-style format */
void notify_format(dbref player, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 2, 3)));

/* From command.c */
void generic_command_failure(dbref player, dbref cause, char *string);

/* From compress.c */
/* Define this to get some statistics on the attribute compression
 * in @stats/tables. Only for word-based compression (COMPRESSION_TYPE 3 or 4
 */
/* #define COMP_STATS /* */
#if (COMPRESSION_TYPE != 0)
unsigned char *
text_compress(char const *s)
  __attribute_malloc__;
#define compress(str) text_compress(str)
    char *text_uncompress(unsigned char const *s);
#define uncompress(str) text_uncompress(str)
    char *safe_uncompress(unsigned char const *s) __attribute_malloc__;
#else
extern char ucbuff[];
#define init_compress(f) 0
#define compress(s) ((unsigned char *)strdup(s))
#define uncompress(s) (strcpy(ucbuff, (char *) s))
#define safe_uncompress(s) (strdup((char *) s))
#endif

/* From cque.c */
struct _ansi_string;
struct real_pcre;
struct eval_context {
  char *wenv[10];                 /**< working environment (%0-%9) */
  char renv[NUMQ][BUFFER_LEN];    /**< working registers q0-q9,qa-qz */
  char *wnxt[10];                 /**< environment to shove into the queue */
  char *rnxt[NUMQ];               /**< registers to shove into the queue */
  int process_command_port;   /**< port number that a command came from */
  dbref cplr;                     /**< initiating player */
  char ccom[BUFFER_LEN];          /**< raw command */
  char ucom[BUFFER_LEN];      /**< evaluated command */
  int break_called;           /**< Has the break command been called? */
  char break_replace[BUFFER_LEN];  /**< What to replace the break with */
  int include_called;           /**< Has the include command been called? */
  char include_replace[BUFFER_LEN];  /**< What to replace the include with */
  char *include_wenv[10];       /**< Working env for include */
  struct real_pcre *re_code;              /**< The compiled re */
  int re_subpatterns;         /**< The number of re subpatterns */
  int *re_offsets;            /**< The offsets for the subpatterns */
  struct _ansi_string *re_from;             /**< The positions of the subpatterns */
};

typedef struct eval_context EVAL_CONTEXT;
extern EVAL_CONTEXT global_eval_context;

void do_second(void);
int do_top(int ncom);
void do_halt(dbref owner, const char *ncom, dbref victim);
void parse_que(dbref player, const char *command, dbref cause);
int queue_attribute_base
  (dbref executor, const char *atrname, dbref enactor, int noparent);
ATTR *queue_attribute_getatr(dbref executor, const char *atrname, int noparent);
int queue_attribute_useatr(dbref executor, ATTR *a, dbref enactor);
int inplace_queue_attribute(dbref thing, const char *atrname,
                            dbref enactor, int rsargs);

/** Queue the code in an attribute, including parent objects */
#define queue_attribute(a,b,c) queue_attribute_base(a,b,c,0)
/** Queue the code in an attribute, excluding parent objects */
#define queue_attribute_noparent(a,b,c) queue_attribute_base(a,b,c,1)
void dequeue_semaphores(dbref thing, char const *aname, int count,
                        int all, int drain);
void shutdown_queues(void);

/* Regexp saving helpers */
struct re_save {
  struct real_pcre *re_code;              /**< The compiled re */
  int re_subpatterns;         /**< The number of re subpatterns */
  int *re_offsets;            /**< The offsets for the subpatterns */
  struct _ansi_string *re_from;             /**< The positions of the subpatterns */
};

/* From create.c */
dbref do_dig(dbref player, const char *name, char **argv, int tport);
dbref do_create(dbref player, char *name, int cost, char *newdbref);
dbref do_real_open(dbref player, const char *direction,
                   const char *linkto, dbref pseudo);
void do_open(dbref player, const char *direction, char **links);
void do_link(dbref player, const char *name, const char *room_name,
             int preserve);
void do_unlink(dbref player, const char *name);
dbref do_clone(dbref player, char *name, char *newname, int preserve,
               char *newdbref);

/* From funtime.c */
int etime_to_secs(char *str1, int *secs);

/* From game.c */
void report(void);
int Hearer(dbref thing);
int Commer(dbref thing);
int Listener(dbref thing);
extern dbref orator;
int parse_chat(dbref player, char *command);
void fork_and_dump(int forking);
void reserve_fd(void);
void release_fd(void);


/* From look.c */
/** Enumeration of types of looks that can be performed */
enum look_type { LOOK_NORMAL, LOOK_TRANS, LOOK_AUTO, LOOK_CLOUDYTRANS,
  LOOK_CLOUDY
};
void look_room(dbref player, dbref loc, enum look_type style);
void do_look_around(dbref player);
void do_look_at(dbref player, const char *name, int key);
char *decompose_str(char *what);

/* From memcheck.c */
void add_check(const char *ref);
void del_check(const char *ref, const char *filename, int line);
void list_mem_check(dbref player);
void log_mem_check(void);

/* From move.c */
void enter_room(dbref player, dbref loc, int nomovemsgs);
int can_move(dbref player, const char *direction);
/** Enumeration of types of movements that can be performed */
enum move_type { MOVE_NORMAL, MOVE_GLOBAL, MOVE_ZONE };
void do_move(dbref player, const char *direction, enum move_type type);
void moveto(dbref what, dbref where);
void safe_tel(dbref player, dbref dest, int nomovemsgs);
int global_exit(dbref player, const char *direction);
int remote_exit(dbref loc, const char *direction);
void move_wrapper(dbref player, const char *command);
void do_follow(dbref player, const char *arg);
void do_unfollow(dbref player, const char *arg);
void do_desert(dbref player, const char *arg);
void do_dismiss(dbref player, const char *arg);
void clear_followers(dbref leader, int noisy);
void clear_following(dbref follower, int noisy);

/* From mycrypt.c */
char *mush_crypt(const char *key);

/* From player.c */
int password_check(dbref player, const char *password);
dbref lookup_player(const char *name);
dbref lookup_player_name(const char *name);
/* from player.c */
dbref create_player(const char *name, const char *password,
                    const char *host, const char *ip);
dbref connect_player(const char *name, const char *password,
                     const char *host, const char *ip, char *errbuf);
void check_last(dbref player, const char *host, const char *ip);
void check_lastfailed(dbref player, const char *host);

/* From parse.c */
bool is_number(const char *str);
bool is_strict_number(const char *str);
bool is_strict_integer(const char *str);
#ifdef HAVE_ISNORMAL
#define is_good_number(n) isnormal((n))
#else
bool is_good_number(NVAL val);
#endif

/* From plyrlist.c */
void clear_players(void);
void add_player(dbref player);
void add_player_alias(dbref player, const char *alias);
void delete_player(dbref player, const char *alias);
void reset_player_list(dbref player, const char *oldname, const char *oldalias,
                       const char *name, const char *alias);

/* From predicat.c */
char *WIN32_CDECL tprintf(const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 1, 2)));

int could_doit(dbref player, dbref thing);
int did_it(dbref player, dbref thing, const char *what,
           const char *def, const char *owhat, const char *odef,
           const char *awhat, dbref loc);
int did_it_with(dbref player, dbref thing, const char *what,
                const char *def, const char *owhat, const char *odef,
                const char *awhat, dbref loc, dbref env0, dbref env1,
                int flags);
int did_it_interact(dbref player, dbref thing, const char *what,
                    const char *def, const char *owhat,
                    const char *odef, const char *awhat, dbref loc, int flags);
int real_did_it(dbref player, dbref thing, const char *what,
                const char *def, const char *owhat, const char *odef,
                const char *awhat, dbref loc, char *myenv[10], int flags);
int can_see(dbref player, dbref thing, int can_see_loc);
int controls(dbref who, dbref what);
int can_pay_fees(dbref who, int pennies);
void giveto(dbref who, int pennies);
int payfor(dbref who, int cost);
int quiet_payfor(dbref who, int cost);
int nearby(dbref obj1, dbref obj2);
int get_current_quota(dbref who);
void change_quota(dbref who, int payment);
int ok_name(const char *name);
int ok_command_name(const char *name);
int ok_function_name(const char *name);
int ok_player_name(const char *name, dbref player, dbref thing);
int ok_player_alias(const char *alias, dbref player, dbref thing);
int ok_password(const char *password);
int ok_tag_attribute(dbref player, const char *params);
dbref parse_match_possessor(dbref player, char **str, int exits);
void page_return(dbref player, dbref target, const char *type,
                 const char *message, const char *def);
char *grep_util(dbref player, dbref thing, char *pattern,
                char *lookfor, int sensitive, int wild);
dbref where_is(dbref thing);
int charge_action(dbref thing);
dbref first_visible(dbref player, dbref thing);

/* From rob.c */
void s_Pennies(dbref thing, int amount);

/* From set.c */
void chown_object(dbref player, dbref thing, dbref newowner, int preserve);
void do_include(dbref player, char *object, char **argv);
/* From speech.c */
int okay_pemit(dbref player, dbref target);
int vmessageformat(dbref player, const char *attribute,
                   dbref executor, int flags, int nargs, ...);
int messageformat(dbref player, const char *attribute,
                  dbref executor, int flags, int nargs, char *argv[]);
void do_message_list(dbref player, dbref enactor, char *list, char *attrname,
                     char *message, int flags, int numargs, char *argv[]);
const char *spname(dbref thing);
void notify_except(dbref first, dbref exception, const char *msg, int flags);
void notify_except2(dbref first, dbref exc1, dbref exc2,
                    const char *msg, int flags);
/* Return thing/PREFIX + msg */
void make_prefixstr(dbref thing, const char *msg, char *tbuf1);
int filter_found(dbref thing, const char *msg, int flag);

/* From strutil.c */
char *next_token(char *str, char sep);
char *split_token(char **sp, char sep);
char *chopstr(const char *str, size_t lim);
int string_prefix(const char *restrict string, const char *restrict prefix);
const char *string_match(const char *src, const char *sub);
char *strupper(const char *s);
char *strlower(const char *s);
char *strinitial(const char *s);
char *upcasestr(char *s);
char *skip_space(const char *s);
#ifdef HAVE_STRCHRNUL
#define seek_char(s,c) strchrnul((s),(c))
#else
char *seek_char(const char *s, char c);
#endif
size_t u_strlen(const unsigned char *s);
unsigned char *u_strncpy
  (unsigned char *restrict target, const unsigned char *restrict source,
   size_t len);


/** Unsigned char strdup. Why is this a macro when the others functions? */
#define u_strdup(x) (unsigned char *)strdup((const char *) x)
#ifndef HAVE_STRDUP
char *
strdup(const char *s)
  __attribute_malloc__;
#endif
    char *mush_strdup(const char *s, const char *check) __attribute_malloc__;

#ifdef HAVE__STRNCOLL
#define strncoll(s1,s2,n) _strncoll((s1), (s2), (n))
#else
    int strncoll(const char *s1, const char *s2, size_t t);
#endif

#ifdef HAVE__STRICOLL
#define strcasecoll(s1,s2) _stricoll((s1), (s2))
#else
    int strcasecoll(const char *s1, const char *s2);
#endif

#ifdef HAVE__STRNICOLL
#define strncasecoll(s1,s2,n) _strnicoll((s1), (s2), (n))
#else
    int strncasecoll(const char *s1, const char *s2, size_t t);
#endif

/** Append a character to the end of a BUFFER_LEN long string.
 * You shouldn't use arguments with side effects with this macro.
 */
#define safe_chr(x, buf, bp) \
                    ((*(bp) - (buf) >= BUFFER_LEN - 1) ? \
                        1 : (*(*(bp))++ = (x), 0))
/* Like sprintf */
    int safe_format(char *buff, char **bp, const char *restrict fmt, ...)
  __attribute__ ((__format__(__printf__, 3, 4)));
/* Append an int to the end of a buffer */
    int safe_integer(intmax_t i, char *buff, char **bp);
    int safe_uinteger(uintmax_t, char *buff, char **bp);
/* Same, but for a SBUF_LEN buffer, not BUFFER_LEN */
#define SBUF_LEN 128    /**< A short buffer */
    int safe_integer_sbuf(intmax_t i, char *buff, char **bp);
/* Append a NVAL to a string */
    int safe_number(NVAL n, char *buff, char **bp);
/* Append a dbref to a buffer */
    int safe_dbref(dbref d, char *buff, char **bp);
/* Append a string to a buffer */
    int safe_str(const char *s, char *buff, char **bp);
/* Append a string to a buffer, sticking it in quotes if there's a space */
    int safe_str_space(const char *s, char *buff, char **bp);
/* Append len characters of a string to a buffer */
    int safe_strl(const char *s, size_t len, char *buff, char **bp);
/** Append a boolean to the end of a string */
#define safe_boolean(x, buf, bufp) \
                safe_chr((x) ? '1' : '0', (buf), (bufp))
/* Append N copies of the character X to the end of a string */
    int safe_fill(char x, size_t n, char *buff, char **bp);
/* Append an accented string */
    int safe_accent(const char *restrict base,
                    const char *restrict tmplate, size_t len, char *buff,
                    char **bp);

    char *mush_strncpy(char *restrict, const char *, size_t);

    char *replace_string
      (const char *restrict old, const char *restrict newbit,
       const char *restrict string) __attribute_malloc__;
    char *replace_string2(const char *old[2], const char *newbits[2],
                          const char *restrict string)
 __attribute_malloc__;
    extern const char *standard_tokens[2];      /* ## and #@ */
    char *trim_space_sep(char *str, char sep);
    int do_wordcount(char *str, char sep);
    char *remove_word(char *list, char *word, char sep);
    char *next_in_list(const char **head);
    void safe_itemizer(int cur_num, int done, const char *delim,
                       const char *conjoin, const char *space,
                       char *buff, char **bp);
    char *show_time(time_t t, bool utc);
    char *show_tm(struct tm *t);


/** This structure associates html entities and base ascii representations */
    typedef struct {
      const char *base;         /**< Base ascii representation */
      const char *entity;       /**< HTML entity */
    } accent_info;

    extern accent_info accent_table[];

    int ansi_strlen(const char *string);
    int ansi_strnlen(const char *string, size_t numchars);

/* From unparse.c */
    const char *real_unparse
      (dbref player, dbref loc, int obey_myopic, int use_nameformat,
       int use_nameaccent);
    extern const char *unparse_object(dbref player, dbref loc);
/** For back compatibility, an alias for unparse_object */
#define object_header(p,l) unparse_object(p,l)
    const char *unparse_object_myopic(dbref player, dbref loc);
    const char *unparse_room(dbref player, dbref loc);
    int nameformat(dbref player, dbref loc, char *tbuf1, char *defname);
    const char *accented_name(dbref thing);

/* From utils.c */
    void parse_attrib(dbref player, char *str, dbref *thing, ATTR **attrib);
    void parse_anon_attrib(dbref player, char *str, dbref *thing,
                           ATTR **attrib);
    void free_anon_attrib(ATTR *attrib);
    typedef struct _ufun_attrib {
      dbref thing;
      char contents[BUFFER_LEN];
      int pe_flags;
      char *errmess;
    } ufun_attrib;
    bool fetch_ufun_attrib(char *attrname, dbref executor,
                           ufun_attrib * ufun, bool accept_lambda);
    bool call_ufun(ufun_attrib * ufun, char **wenv_args, int wenv_argc,
                   char *ret, dbref executor, dbref enactor, PE_Info *pe_info);
    bool call_attrib(dbref thing, const char *attrname,
                     const char *wenv_args[], int wenv_argc, char *ret,
                     dbref enactor, PE_Info *pe_info);
    bool member(dbref thing, dbref list);
    bool recursive_member(dbref disallow, dbref from, int count);
    dbref remove_first(dbref first, dbref what);
    dbref reverse(dbref list);
    void *mush_malloc(size_t bytes, const char *check) __attribute_malloc__;
    void *mush_calloc(size_t count, size_t size,
                      const char *check) __attribute_malloc__;
#define mush_realloc(ptr, size, tag) \
  mush_realloc_where((ptr), (size), (tag), __FILE__, __LINE__)
    void *mush_realloc_where(void *restrict ptr, size_t newsize,
                             const char *restrict check,
                             const char *restrict filename, int line);
#define mush_free(ptr,tag) mush_free_where((ptr), (tag), __FILE__, __LINE__)
    void mush_free_where(void *restrict ptr, const char *restrict check,
                         const char *restrict filename, int line);
    uint32_t get_random32(uint32_t low, uint32_t high);
    char *fullalias(dbref it);
    char *shortalias(dbref it);
    char *shortname(dbref it);
    dbref absolute_room(dbref it);
    int can_interact(dbref from, dbref to, int type);


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
    bool local_wild_match_case(const char *restrict s,
                               const char *restrict d, bool cs);
    bool wildcard(const char *s);
    bool quick_wild_new(const char *restrict tstr,
                        const char *restrict dstr, bool cs);
    bool regexp_match_case_r(const char *restrict s, const char *restrict d,
                             bool cs, char **, size_t, char *restrict, ssize_t);
    bool quick_regexp_match(const char *restrict s,
                            const char *restrict d, bool cs);
    bool qcomp_regexp_match(const pcre * re, const char *s);
    bool wild_match_case_r(const char *restrict s,
                           const char *restrict d, bool cs,
                           char **ary, size_t max, char *ata, ssize_t len);
    bool quick_wild(const char *restrict tsr, const char *restrict dstr);
    bool atr_wild(const char *restrict tstr, const char *restrict dstr);
/** Default (case-insensitive) local wildcard match */
#define local_wild_match(s,d) local_wild_match_case(s, d, 0)

/** Types of lists */

    extern char ALPHANUM_LIST[];
    extern char INSENS_ALPHANUM_LIST[];
    extern char DBREF_LIST[];
    extern char NUMERIC_LIST[];
    extern char FLOAT_LIST[];
    extern char DBREF_NAME_LIST[];
    extern char DBREF_NAMEI_LIST[];
    extern char DBREF_IDLE_LIST[];
    extern char DBREF_CONN_LIST[];
    extern char DBREF_CTIME_LIST[];
    extern char DBREF_OWNER_LIST[];
    extern char DBREF_LOCATION_LIST[];
    extern char DBREF_ATTR_LIST[];
    extern char DBREF_ATTRI_LIST[];
    extern char *UNKNOWN_LIST;

/* From function.c and other fun*.c */
    char *strip_braces(char const *line);

    void save_regexp_context(struct re_save *);
    void restore_regexp_context(struct re_save *);
    void save_global_regs(const char *funcname, char *preserve[]);
    void restore_global_regs(const char *funcname, char *preserve[]);
    void save_partial_global_reg(const char *funcname, char
                                 *preserve[], int num);
    void restore_partial_global_regs(const char *funcname, char
                                     *preserve[]);
    void free_global_regs(const char *funcname, char *preserve[]);
    void init_global_regs(char *preserve[]);
    void load_global_regs(char *preserve[]);
    void save_global_env(const char *funcname, char *preserve[]);
    void restore_global_env(const char *funcname, char *preserve[]);
    void save_global_nxt(const char *funcname, char *preservew[],
                         char *preserver[], char *valw[], char *valr[]);
    void restore_global_nxt(const char *funcname, char *preservew[],
                            char *preserver[], char *valw[], char *valr[]);
    int delim_check(char *buff, char **bp, int nfargs, char **fargs,
                    int sep_arg, char *sep);
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

 /* local.c */
    void local_startup(void);
    void local_configs(void);
    void local_locks(void);
    void local_dump_database(void);
    void local_dbck(void);
    void local_shutdown(void);
    void local_timer(void);
    void local_connect(dbref player, int isnew, int num);
    void local_disconnect(dbref player, int num);
    void local_data_create(dbref object);
    void local_data_clone(dbref clone, dbref source);
    void local_data_free(dbref object);
    int local_can_interact_first(dbref from, dbref to, int type);
    int local_can_interact_last(dbref from, dbref to, int type);

    /* flaglocal.c */
    void local_flags(FLAGSPACE *flags);

/* sig.c */
    /** Type definition for signal handlers */
    typedef void (*Sigfunc) (int);
/* Set up a signal handler. Use instead of signal() */
    Sigfunc install_sig_handler(int signo, Sigfunc f);
/* Call from in a signal handler to re-install the handler. Does nothing
   with persistent signals */
    void reload_sig_handler(int signo, Sigfunc f);
/* Ignore a signal. Like i_s_h with SIG_IGN) */
    void ignore_signal(int signo);
/* Block one signal temporarily. */
    void block_a_signal(int signo);
/* Unblock a signal */
    void unblock_a_signal(int signo);
/* Block all signals en masse. */
    void block_signals(void);
#endif                          /* __EXTERNS_H */
