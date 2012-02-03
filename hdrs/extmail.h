/**
 * \file extmail.h
 *
 * \brief header for Javelin's extended mailer
 */

#ifndef _EXTMAIL_H
#define _EXTMAIL_H
/* Some of this isn't implemented yet, but heralds the future! */
#define M_MSGREAD       0x0001U
#define M_UNREAD        0x0FFEU
#define M_CLEARED       0x0002U
#define M_URGENT        0x0004U
#define M_MASS          0x0008U
#define M_EXPIRE        0x0010U
#define M_RECEIPT       0x0020U
#define M_TAG           0x0040U
#define M_FORWARD       0x0080U
/* 0x0100 - 0x0F00 reserved for folder numbers */
#define M_FMASK         0xF0FFU
#define M_ALL           0x1000U /* In mail_selector, all msgs in all folders */
#define M_MSUNREAD      0x2000U /* Mail selectors */
#define M_REPLY         0x4000U
#define M_FOLDER        0x8000U /* In mail selector, all msgs in cur folder */

#define MAX_FOLDERS     15
#define FOLDER_NAME_LEN (BUFFER_LEN / 30)
#define FolderBit(f) (256 * (f))
#define Urgent(m)       (m->read & M_URGENT)
#define Mass(m)         (m->read & M_MASS)
#define Expire(m)       (m->read & M_EXPIRE)
#define Receipt(m)      (m->read & M_RECEIPT)
#define Forward(m)      (m->read & M_FORWARD)
#define Reply(m)        (m->read & M_REPLY)
#define Tagged(m)       (m->read & M_TAG)
#define Folder(m)       ((m->read & ~M_FMASK) >> 8U)
#define Read(m)         (m->read & M_MSGREAD)
#define Cleared(m)      (m->read & M_CLEARED)
#define Unread(m)       (!Read(m))
#define All(ms)         (ms.flags & M_ALL)
#define AllInFolder(ms) (ms.flags & M_FOLDER)
#define MSFolder(ms)    ((int)((ms.flags & ~M_FMASK) >> 8U))

/** A mail selection.
 * This structure maintains information about a selected list of
 * messages. Messages can be selected in several ways.
 */
struct mail_selector {
  int low;              /**< Minimum message number */
  int high;             /**< Maximum message number */
  mail_flag flags;      /**< Message flags */
  dbref player;         /**< Message sender's dbref */
  int days;             /**< Target message age in days */
  int day_comp;         /**< Direction of comparison to target age */
};

typedef int folder_array[MAX_FOLDERS + 1];
#define FA_Init(fa) \
  do { \
  int nfolders; \
  for (nfolders = 0; nfolders <= MAX_FOLDERS; nfolders++)	\
    fa[nfolders] = 0; \
  } while (0)

#define SUBJECT_COOKIE  '/'
#define SUBJECT_LEN     60

#define MDBF_SUBJECT    0x1
#define MDBF_ALIASES    0x2

/* Database ends with ***END OF DUMP*** not *** END OF DUMP *** */
#define MDBF_NEW_EOD    0x4

/* Database contains sender ctimes */
#define MDBF_SENDERCTIME        0x8

/* From extmail.c */
extern struct mail *maildb;
extern void set_player_folder(dbref player, int fnum);
extern void add_folder_name(dbref player, int fld, const char *name);
extern struct mail *find_exact_starting_point(dbref player);
extern void check_mail(dbref player, int folder, int silent);
extern void check_all_mail(dbref player);
int dump_mail(PENNFILE *fp);
int load_mail(PENNFILE *fp);
extern void mail_init(void);
extern int mdb_top;
extern int can_mail(dbref player);
extern void do_mail(dbref player, char *arg1, char *arg2);
enum mail_stats_type { MSTATS_COUNT, MSTATS_READ, MSTATS_SIZE };
extern void do_mail_stats(dbref player, char *name, enum mail_stats_type full);
extern void do_mail_debug(dbref player, const char *action, const char *victim);
extern void do_mail_nuke(dbref player);
extern void do_mail_change_folder(dbref player, char *fld, char *newname);
extern void do_mail_unfolder(dbref player, char *fld);
extern void do_mail_list(dbref player, const char *msglist);
extern void do_mail_read(dbref player, char *msglist);
extern void do_mail_review(dbref player, const char *name, const char *msglist);
extern void do_mail_retract(dbref player, const char *name,
                            const char *msglist);
extern void do_mail_clear(dbref player, const char *msglist);
extern void do_mail_unclear(dbref player, const char *msglist);
extern void do_mail_unread(dbref player, const char *msglist);
extern void do_mail_status(dbref player, const char *msglist,
                           const char *status);
extern void do_mail_purge(dbref player);
extern void do_mail_file(dbref player, char *msglist, char *folder);
extern void do_mail_tag(dbref player, const char *msglist);
extern void do_mail_untag(dbref player, const char *msglist);
extern void do_mail_fwd(dbref player, char *msglist, char *tolist);
extern void do_mail_send
  (dbref player, char *tolist, char *message, mail_flag flags,
   int silent, int nosig);

#endif                          /* _EXTMAIL_H */
