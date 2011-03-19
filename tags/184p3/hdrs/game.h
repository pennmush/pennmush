/* game.h */
/* Command handlers */

#ifndef __GAME_H
#define __GAME_H

/* Miscellaneous flags */
#define CHECK_INVENTORY            0x10
#define CHECK_NEIGHBORS            0x20
#define CHECK_SELF                 0x40
#define CHECK_HERE                 0x80
#define CHECK_ZONE                 0x100
#define CHECK_GLOBAL               0x200

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
void do_page_port(dbref player, dbref cause, const char *pc, const char *msg,
                  bool eval_msg);
void do_pemit_port(dbref player, const char *pc, const char *msg, int flags);
/* From cque.c */
void do_wait
  (dbref player, dbref cause, char *arg1, const char *cmd, bool until);
void do_waitpid(dbref, const char *, const char *, bool);
enum queue_type { QUEUE_ALL, QUEUE_NORMAL, QUEUE_SUMMARY, QUEUE_QUICK };
void do_queue(dbref player, const char *what, enum queue_type flag);
void do_queue_single(dbref player, char *pidstr);
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
extern void do_hook_list(dbref player, char *command);


/* From compress.c */
#if (COMPRESSION_TYPE > 0)
int init_compress(PENNFILE *f);
#endif

/* From conf.c */
extern int config_file_startup(const char *conf, int restrictions);

/* From game.c */
enum dump_type { DUMP_NORMAL, DUMP_DEBUG, DUMP_PARANOID };
extern void do_dump(dbref player, char *num, enum dump_type flag);
enum shutdown_type { SHUT_NORMAL, SHUT_PANIC, SHUT_PARANOID };
extern void do_shutdown(dbref player, enum shutdown_type panic_flag);

/* From look.c */
enum exam_type { EXAM_NORMAL, EXAM_BRIEF, EXAM_MORTAL };
extern void do_examine(dbref player, const char *name, enum exam_type flag,
                       int all, int parent);
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
extern void do_get(dbref player, const char *what);
extern void do_drop(dbref player, const char *name);
extern void do_enter(dbref player, const char *what);
extern void do_leave(dbref player);
extern void do_empty(dbref player, const char *what);
extern void do_firstexit(dbref player, const char **what);

/* From player.c */
extern void do_password(dbref player, dbref cause,
                        const char *old, const char *newobj);

/* From predicat.c */
extern void do_switch
  (dbref player, char *expression, char **argv, dbref cause, int first,
   int notifyme, int regexp, int inplace);
extern void do_verb(dbref player, dbref cause, char *arg1, char **argv);
extern void do_grep
  (dbref player, char *obj, char *lookfor, int flag, int insensitive);

/* From rob.c */
extern void do_kill(dbref player, const char *what, int cost, int slay);
extern void do_give(dbref player, char *recipient, char *amnt, int silent);
extern void do_buy(dbref player, char *item, char *from, int price);

/* From set.c */
extern void do_name(dbref player, const char *name, char *newname);
extern void do_chown
  (dbref player, const char *name, const char *newobj, int preserve);
extern int do_chzone(dbref player, const char *name, const char *newobj,
                     bool noisy, bool preserve);
extern int do_set(dbref player, const char *name, char *flag);
extern void do_cpattr
  (dbref player, char *oldpair, char **newpair, int move, int noflagcopy);
enum edit_type { EDIT_FIRST, EDIT_ALL };
extern void do_gedit(dbref player, char *it, char **argv,
                     enum edit_type target, int doit);
extern void do_trigger(dbref player, char *object, char **argv);
extern void do_use(dbref player, const char *what);
extern void do_parent(dbref player, char *name, char *parent_name);
extern void do_wipe(dbref player, char *name);

/* From speech.c */
extern void do_say(dbref player, const char *tbuf1);
extern void do_whisper
  (dbref player, const char *arg1, const char *arg2, int noisy);
extern void do_whisper_list
  (dbref player, const char *arg1, const char *arg2, int noisy);
extern void do_pose(dbref player, const char *tbuf1, int space);
enum wall_type { WALL_ALL, WALL_RW, WALL_WIZ };
void do_wall(dbref player, const char *message, enum wall_type target,
             int emit);
void do_page(dbref player, const char *arg1, const char *arg2,
             dbref cause, int noeval, int override, int has_eq);
void do_think(dbref player, const char *message);
#define PEMIT_SILENT 0x1
#define PEMIT_LIST   0x2
#define PEMIT_SPOOF  0x4
#define PEMIT_PROMPT 0x8
extern void do_emit(dbref player, const char *tbuf1, int flags);
extern void do_pemit
  (dbref player, const char *arg1, const char *arg2, int flags);
extern void do_pemit_list(dbref player, char *list, const char *message,
                          int flags);
extern void do_remit(dbref player, char *arg1, const char *arg2, int flags);
extern void do_lemit(dbref player, const char *tbuf1, int flags);
extern void do_zemit(dbref player, const char *arg1, const char *arg2,
                     int flags);
extern void do_oemit_list(dbref player, char *arg1, const char *arg2,
                          int flags);
extern void do_teach(dbref player, dbref cause, const char *tbuf1);

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
  (dbref player, const char *arg1, const char *arg2, int silent, int inside);
extern void do_force(dbref player, dbref caller, const char *what, char *command,
                     int inplace);
extern void do_stats(dbref player, const char *name);
extern void do_newpassword
  (dbref player, dbref cause, const char *name, const char *password);
enum boot_type { BOOT_NAME, BOOT_DESC, BOOT_SELF };
extern void do_boot(dbref player, const char *name, enum boot_type flag,
                    int silent);
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
extern void do_destroy(dbref player, char *name, int confirm);

/* From timer.c */
void init_timer(void);
void signal_cpu_limit(int signo);

typedef bool(*sq_func) (void *);
void sq_register(time_t w, sq_func f, void *d, const char *ev);
void sq_register_in(int n, sq_func f, void *d, const char *ev);
void sq_register_loop(int n, sq_func f, void *d, const char *ev);
bool sq_run_one(void);
bool sq_run_all(void);

void init_sys_events(void);

/* From version.c */
extern void do_version(dbref player);

#endif                          /* __GAME_H */
