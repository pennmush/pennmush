/**
 * \file game.h
 *
 * \brief Prototypes for a whole bunch of stuff with nowhere else to go
 */

/* Command handlers */

#ifndef __GAME_H
#define __GAME_H

#include "mushtype.h"

/* @scan flags */
#define CHECK_INVENTORY  0x010  /*<< Check player's inventory for $-commands */
#define CHECK_NEIGHBORS  0x020  /*<< Check objects in player's location for $-commands, including player */
#define CHECK_SELF       0x040  /*<< Check player for $-commands */
#define CHECK_HERE       0x080  /*<< Check player's location for $-commands */
#define CHECK_ZONE       0x100  /*<< Check player's ZMT/ZMR for $-commands */
#define CHECK_GLOBAL     0x200  /*<< Check for Master Room $-commands */
#define CHECK_BREAK      0x400  /*<< Do no further checks after a round of $-command matching succeeds */
/* Does NOT include CHECK_BREAK */
#define CHECK_ALL (CHECK_INVENTORY | CHECK_NEIGHBORS | CHECK_SELF | CHECK_HERE | CHECK_ZONE | CHECK_GLOBAL)

/* hash table stuff */
extern void init_func_hashtab(void);    /* eval.c */
extern void init_aname_table(void);     /* atr_tab.c */
extern void init_flagspaces(void);      /* flags.c */
extern void init_flag_table(const char *ns);    /* flags.c */
extern void init_pronouns(void);        /* funstr.c */

/* From bsd.c */
void fcache_init(void);
void fcache_load(dbref player);
void hide_player(dbref player, int hide, char *victim);
enum motd_type { MOTD_MOTD, MOTD_WIZ, MOTD_DOWN, MOTD_FULL, MOTD_LIST };
void do_motd(dbref player, enum motd_type key, const char *message);
void do_poll(dbref player, const char *message, int clear);
void do_page_port(dbref executor, const char *pc, const char *msg);
void do_pemit_port(dbref player, const char *pc, const char *msg, int flags);
/* From cque.c */
void do_wait
  (dbref executor, dbref enactor, char *arg1, const char *cmd, bool until,
   MQUE *parent_queue);
void do_waitpid(dbref, const char *, const char *, bool);
enum queue_type { QUEUE_ALL, QUEUE_NORMAL, QUEUE_SUMMARY, QUEUE_QUICK };
void do_queue(dbref player, const char *what, enum queue_type flag);
void do_queue_single(dbref player, char *pidstr, bool debug);
void do_halt1(dbref player, const char *arg1, const char *arg2);
void do_haltpid(dbref, const char *);
void do_allhalt(dbref player);
void do_allrestart(dbref player);
void do_restart(void);
void do_restart_com(dbref player, const char *arg1);

/* From command.c */
enum hook_type { HOOK_BEFORE, HOOK_AFTER, HOOK_IGNORE, HOOK_OVERRIDE };
extern void do_hook(dbref player, char *command, char *obj, char *attrname,
                    enum hook_type flag, int inplace);
extern void do_hook_list(dbref player, char *command, bool verbose);


/* From compress.c */
#if (COMPRESSION_TYPE > 0)
int init_compress(PENNFILE *f);
#endif

/* From game.c */
enum dump_type { DUMP_NORMAL, DUMP_DEBUG, DUMP_PARANOID };
extern void do_dump(dbref player, char *num, enum dump_type flag);
enum shutdown_type { SHUT_NORMAL, SHUT_PANIC, SHUT_PARANOID };
extern void do_shutdown(dbref player, enum shutdown_type panic_flag);
extern void do_dolist(dbref executor, char *list, char *command,
                      dbref enactor, unsigned int flags, MQUE *queue_entry);


/* From look.c */
enum exam_type { EXAM_NORMAL, EXAM_BRIEF, EXAM_MORTAL };
extern void do_examine(dbref player, const char *name, enum exam_type flag,
                       int all, int parent, int opaque);
extern void do_inventory(dbref player);
extern void do_find(dbref player, const char *name, char **argv);
extern void do_whereis(dbref player, const char *name);
extern void do_score(dbref player);
extern void do_sweep(dbref player, const char *arg1);
extern void do_entrances(dbref player, const char *where, char **argv,
                         int types);
enum dec_type { DEC_NORMAL, DEC_DB = 1, DEC_FLAG = 2, DEC_ATTR =
    4, DEC_SKIPDEF = 8, DEC_TF = 16
};
extern void do_decompile(dbref player, const char *name, const char *prefix,
                         int dec_type);

/* From move.c */
extern void do_get(dbref player, const char *what, NEW_PE_INFO *pe_info);
extern void do_drop(dbref player, const char *name, NEW_PE_INFO *pe_info);
extern void do_enter(dbref player, const char *what, NEW_PE_INFO *pe_info);
extern void do_leave(dbref player, NEW_PE_INFO *pe_info);
extern void do_empty(dbref player, const char *what, NEW_PE_INFO *pe_info);
extern void do_firstexit(dbref player, const char **what);

/* From player.c */
extern void do_password(dbref executor, dbref enactor,
                        const char *old, const char *newobj, MQUE *queue_entry);

/* From predicat.c */
extern void do_switch
  (dbref executor, char *expression, char **argv, dbref enactor, int first,
   int notifyme, int regexp, int queue_type, MQUE *queue_entry);
extern void do_verb(dbref executor, dbref enactor, char *arg1, char **argv,
                    MQUE *queue_entry);
extern void do_grep(dbref player, char *obj, char *lookfor, int flag,
                    int insensitive);

/* From rob.c */
extern void do_kill(dbref player, const char *what, int cost, int slay);
extern void do_give(dbref player, char *recipient, char *amnt, int silent,
                    NEW_PE_INFO *pe_info);
extern void do_buy(dbref player, char *item, char *from, int price,
                   NEW_PE_INFO *pe_info);

/* From set.c */
extern void do_name(dbref player, const char *name, char *newname);
extern void do_chown
  (dbref player, const char *name, const char *newobj, int preserve,
   NEW_PE_INFO *pe_info);
extern int do_chzone(dbref player, const char *name, const char *newobj,
                     bool noisy, bool preserve, NEW_PE_INFO *pe_info);
extern int do_set(dbref player, const char *name, char *flag);
extern void do_cpattr
  (dbref player, char *oldpair, char **newpair, int move, int noflagcopy);
#define EDIT_DEFAULT 0  /**< Edit all occurrences. Default. */
#define EDIT_FIRST   1  /**< Only edit the first occurrence in each attribute. */
#define EDIT_CHECK   2  /**< Don't actually edit the attr, just show what would happen if we did */
#define EDIT_QUIET   4  /**< Don't show new values, just report total changes */

extern void do_gedit(dbref player, char *it, char **argv, int flags);
extern void do_trigger(dbref player, char *object, char **argv,
                       MQUE *queue_entry);
extern void do_use(dbref player, const char *what, NEW_PE_INFO *pe_info);
extern void do_parent(dbref player, char *name, char *parent_name,
                      NEW_PE_INFO *pe_info);
extern void do_wipe(dbref player, char *name);

/* From speech.c */
extern void do_say(dbref player, const char *message, NEW_PE_INFO *pe_info);
extern void do_whisper
  (dbref player, const char *arg1, const char *arg2, int noisy,
   NEW_PE_INFO *pe_info);
extern void do_pose(dbref player, const char *tbuf1, int nospace,
                    NEW_PE_INFO *pe_info);
enum wall_type { WALL_ALL, WALL_RW, WALL_WIZ };
void do_wall(dbref player, const char *message, enum wall_type target,
             int emit);
void do_page(dbref executor, const char *arg1, const char *arg2,
             int override, int has_eq, NEW_PE_INFO *pe_info);
#define PEMIT_SILENT 0x1  /**< Don't show confirmation msg to speaker */
#define PEMIT_LIST   0x2  /**< Recipient is a list of names */
#define PEMIT_SPOOF  0x4  /**< Show sound as being from %#, not %! */
#define PEMIT_PROMPT 0x8  /**< Add a telnet GOAHEAD to the end. For \@prompt */
extern void do_emit(dbref player, const char *message, int flags,
                    NEW_PE_INFO *pe_info);
extern void do_pemit(dbref player, char *target, const char *message, int flags,
                     struct format_msg *format, NEW_PE_INFO *pe_info);
#define do_pemit_list(player,target,message,flags,pe_info) \
        do_pemit(player,target,message,flags|PEMIT_LIST,NULL,pe_info)
extern void do_remit(dbref player, char *rooms, const char *message,
                     int flags, struct format_msg *format,
                     NEW_PE_INFO *pe_info);
extern void do_lemit(dbref player, const char *message, int flags,
                     NEW_PE_INFO *pe_info);
extern void do_zemit(dbref player, const char *target, const char *message,
                     int flags);
extern void do_oemit_list(dbref player, char *list, const char *message,
                          int flags, struct format_msg *format,
                          NEW_PE_INFO *pe_info);
extern void do_teach(dbref player, const char *tbuf1, int list,
                     MQUE *parent_queue);

/* From wiz.c */
extern void do_debug_examine(dbref player, const char *name);
extern void do_enable(dbref player, const char *param, int state);
extern void do_kick(dbref player, const char *num);
extern void do_search(dbref player, const char *arg1, char **arg3);
extern dbref do_pcreate
  (dbref creator, const char *player_name, const char *player_password,
   char *try_dbref);
extern void do_quota(dbref player, const char *arg1, const char *arg2,
                     int set_q);
extern void do_allquota(dbref player, const char *arg1, int quiet);
extern void do_teleport
  (dbref player, const char *arg1, const char *arg2, int silent, int inside,
   NEW_PE_INFO *pe_info);
extern void do_force(dbref player, dbref caller, const char *what,
                     char *command, int queue_type, MQUE *queue_entry);
extern void do_stats(dbref player, const char *name);
extern void do_newpassword
  (dbref executor, dbref enactor, const char *name, const char *password,
   MQUE *queue_entry);
enum boot_type { BOOT_NAME, BOOT_DESC, BOOT_SELF };
extern void do_boot(dbref player, const char *name, enum boot_type flag,
                    int silent, MQUE *queue_entry);
extern void do_chzoneall(dbref player, const char *name, const char *target,
                         bool preserve);
extern int parse_force(char *command);
extern void do_power(dbref player, const char *name, const char *power);
enum sitelock_type { SITELOCK_ADD, SITELOCK_REMOVE, SITELOCK_BAN,
  SITELOCK_CHECK, SITELOCK_LIST, SITELOCK_REGISTER
};
extern void do_sitelock(dbref player, const char *site, const char *opts,
                        const char *charname, enum sitelock_type type, int psw);
extern void do_sitelock_name(dbref player, const char *name);
extern void do_chownall
  (dbref player, const char *name, const char *target, int preserve);
extern void do_reboot(dbref player, int flag);

/* From destroy.c */
extern void do_dbck(dbref player);
extern void do_destroy(dbref player, char *name, int confirm,
                       NEW_PE_INFO *pe_info);

/* From timer.c */
void init_timer(void);
void signal_cpu_limit(int signo);

struct squeue *sq_register(time_t w, sq_func f, void *d, const char *ev);
struct squeue *sq_register_in(int n, sq_func f, void *d, const char *ev);
void sq_register_loop(int n, sq_func f, void *d, const char *ev);
void sq_cancel(struct squeue *sq);
bool sq_run_one(void);
bool sq_run_all(void);

void init_sys_events(void);

/* From version.c */
extern void do_version(dbref player);

#endif                          /* __GAME_H */
