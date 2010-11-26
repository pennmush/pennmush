/* mail.h - header for Javelin's extended mailer */

#ifndef _EXTMAIL_H
#define _EXTMAIL_H
/* Some of this isn't implemented yet, but heralds the future! */

/* Per-message flags */
#define M_URGENT        0x0004U
#define M_FORWARD       0x0080U

/* Individual mailbox flags */
#define M_MSGREAD       0x0001U
#define M_UNREAD        0x0FFEU
#define M_CLEARED       0x0002U
#define M_TAG           0x0040U

/* 0x0100 - 0x0F00 reserved for folder numbers */
#define M_FMASK         0xF0FFU
#define M_ALL           0x1000U /* In mail_selector, all msgs in all folders */
#define M_MSUNREAD      0x2000U /* Mail selectors */
#define M_REPLY         0x4000U
#define M_FOLDER        0x8000U /* In mail selector, all msgs in cur folder */

#define MAX_FOLDERS     15
#define FOLDER_NAME_LEN (BUFFER_LEN / 30)
#define FolderBit(f) (256 * (f))
#define Urgent(m)       (m->msg->flags & M_URGENT)
#define Mass(m)         (m->msg->flags & M_MASS)
#define Expire(m)       (m->msg->flags & M_EXPIRE)
#define Receipt(m)      (m->msg->flags & M_RECEIPT)
#define Forward(m)      (m->msg->flags & M_FORWARD)
#define Reply(m)        (m->flags & M_REPLY)
#define Tagged(m)       (m->flags & M_TAG)
static inline int 
Folder(MAIL *m) {
  return m->folder;
}
#define Read(m)         (m->flags & M_MSGREAD)
#define Cleared(m)      (m->flags & M_CLEARED)
#define Unread(m)       (!Read(m))

/* Mail selector access macros */
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
MAIL *maildb;
MAILBOX *get_mailbox(dbref player);
void set_player_folder(dbref player, int fnum);
void add_folder_name(dbref player, int fld, const char *name);
struct mail *find_exact_starting_point(dbref player);
void check_mail(dbref player, int folder, int silent);
void check_all_mail(dbref player);
int dump_mail(PENNFILE *fp);
int load_mail(PENNFILE *fp);
void mail_init(void);
int mdb_top;
int can_mail(dbref player);
void do_mail(dbref player, char *arg1, char *arg2);
enum mail_stats_type { MSTATS_COUNT, MSTATS_READ, MSTATS_SIZE };
void do_mail_stats(dbref player, char *name, enum mail_stats_type full);
void do_mail_debug(dbref player, const char *action, const char *victim);
void do_mail_nuke(dbref player);
void do_mail_change_folder(dbref player, char *fld, char *newname);
void do_mail_unfolder(dbref player, char *fld);
void do_mail_list(dbref player, const char *msglist);
void do_mail_read(dbref player, char *msglist);
void do_mail_clear(dbref player, const char *msglist);
void do_mail_unclear(dbref player, const char *msglist);
void do_mail_unread(dbref player, const char *msglist);
void do_mail_status(dbref player, const char *msglist,
                           const char *status);
void do_mail_purge(dbref player);
void do_mail_file(dbref player, char *msglist, char *folder);
void do_mail_tag(dbref player, const char *msglist);
void do_mail_untag(dbref player, const char *msglist);
void do_mail_fwd(dbref player, char *msglist, char *tolist);
void do_mail_send
  (dbref player, char *tolist, char *message, mail_flag flags,
   int silent, int nosig);

#endif                          /* _EXTMAIL_H */
