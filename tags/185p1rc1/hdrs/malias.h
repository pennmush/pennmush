/**
 * \file malias.h
 *
 * \brief header file for global mailing aliases/lists
 */

#ifndef _MALIAS_H
#define _MALIAS_H


#define MALIAS_TOKEN    '+'     /**< Initial char for alias names */

#define ALIAS_MEMBERS   0x1     /**< Only those on the alias */
#define ALIAS_ADMIN     0x2     /**< Only admin/powered */
#define ALIAS_OWNER     0x4     /**< Only the owner */

/** A mail alias.
 * This structure represents a mail alias (or mailing list).
 */
struct mail_alias {
  char *name;           /**< Name of the alias */
  unsigned char *desc;  /**< Description */
  int size;             /**< Size of the members array */
  dbref *members;       /**< Pointer to an array of dbrefs of list members */
  int nflags;           /**< Permissions for who can use/see alias name */
  int mflags;           /**< Permissions for who can list alias members */
  dbref owner;          /**< Who owns (controls) this alias */
};


/* From malias.c */
struct mail_alias *get_malias(dbref player, char *alias);
int ismember(struct mail_alias *m, dbref player);
void do_malias_privs(dbref player, char *alias, char *privs, int typs);
void do_malias_mprivs(dbref player, char *alias, char *privs);
extern void do_malias(dbref player, char *arg1, char *arg2);
extern void do_malias_create(dbref player, char *alias, char *tolist);
extern void do_malias_members(dbref player, char *alias);
extern void do_malias_list(dbref player);
extern void do_malias_desc(dbref player, char *alias, char *desc);
extern void do_malias_chown(dbref player, char *alias, char *owner);
extern void do_malias_rename(dbref player, char *alias, char *newname);
extern void do_malias_destroy(dbref player, char *alias);
extern void do_malias_all(dbref player);
extern void do_malias_stats(dbref player);
extern void do_malias_nuke(dbref player);
extern void do_malias_add(dbref player, char *alias, char *tolist);
extern void do_malias_remove(dbref player, char *alias, char *tolist);
void load_malias(PENNFILE *fp);
void save_malias(PENNFILE *fp);
extern void malias_cleanup(dbref player);
extern void do_malias_set(dbref player, char *alias, char *tolist);
#else                           /* MAIL_ALIASES */

/* We still need this one */
void load_malias(FILE * fp);

#endif
