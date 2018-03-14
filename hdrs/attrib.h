/**
 * \file attrib.h
 *
 * \brief Attribute-related prototypes and constants.
 */

#ifndef _ATTRIB_H
#define _ATTRIB_H

#include "chunk.h"
#include "compile.h"
#include "dbio.h"
#include "mushtype.h"

/** An attribute on an object.
 * This structure represents an attribute set on an object.
 * Attributes form a linked list on an object, sorted alphabetically.
 */
struct attr {
  char const *name;       /**< Name of attribute */
  uint32_t flags;         /**< Attribute flags */
  chunk_reference_t data; /**< The attribute's value, compressed */
  dbref creator;          /**< The attribute's creator's dbref */
  ATTR *next;             /**< Pointer to next attribute in list */
};

/** An alias for an attribute.
 */
typedef struct atr_alias {
  const char *alias;    /**< The alias. */
  const char *realname; /**< The attribute's canonical name. */
} ATRALIAS;

/* Stuff that's actually in atr_tab.c */
ATTR *aname_hash_lookup(const char *name);
int alias_attribute(const char *atr, const char *alias);
void do_attribute_limit(dbref player, char *name, int type, char *pattern);
void do_attribute_access(dbref player, char *name, char *perms,
                         int retroactive);
void do_attribute_delete(dbref player, char *name);
void do_attribute_rename(dbref player, char *old, char *newname);
void do_attribute_info(dbref player, char *name);
void do_list_attribs(dbref player, int lc);
void do_decompile_attribs(dbref player, char *pattern, int retroactive);
char *list_attribs(void);
void attr_init_postconfig(void);
const char *check_attr_value(dbref player, const char *name, const char *value);
int cnf_attribute_access(char *attrname, char *opts);

void add_new_attr(char *name, uint32_t flags);
/* From attrib.c */

/** atr_add(), atr_clr() error codes */
typedef enum {
  AE_OKAY = 0,     /**< Success */
  AE_ERROR = -1,   /**< general failure */
  AE_SAFE = -2,    /**< attempt to overwrite a safe attribute */
  AE_BADNAME = -3, /**< invalid name */
  AE_TOOMANY = -4, /**< too many attribs */
  AE_TREE = -5,    /**< unable to delete/create entire tree */
  AE_NOTFOUND = -6 /** No such attribute */
} atr_err;

int good_atr_name(char const *s);
ATTR *atr_match(char const *string);
ATTR *atr_sub_branch(ATTR *branch);
ATTR *atr_sub_branch_prev(ATTR *branch);
void atr_new_add(dbref thing, char const *RESTRICT atr, char const *RESTRICT s,
                 dbref player, uint32_t flags, uint8_t derefs, bool makeroots);
atr_err atr_add(dbref thing, char const *RESTRICT atr, char const *RESTRICT s,
                dbref player, uint32_t flags);
atr_err atr_clr(dbref thing, char const *atr, dbref player);
atr_err wipe_atr(dbref thing, char const *atr, dbref player);
ATTR *atr_get(dbref thing, char const *atr);
ATTR *atr_get_noparent(dbref thing, char const *atr);
typedef int (*aig_func)(dbref, dbref, dbref, const char *, ATTR *, void *);
int atr_iter_get(dbref player, dbref thing, char const *name, int mortal,
                 int regexp, aig_func func, void *args);
int atr_iter_get_parent(dbref player, dbref thing, char const *name, int mortal,
                        int regexp, aig_func func, void *args);
int atr_pattern_count(dbref player, dbref thing, const char *name, int doparent,
                      int mortal, int regexp);
ATTR *atr_complete_match(dbref player, char const *atr, dbref privs);
void atr_free_all(dbref thing);
void atr_cpy(dbref dest, dbref source);
char const *convert_atr(int oldatr);
int atr_comm_match(dbref thing, dbref player, int type, int end,
                   char const *str, int just_match, int check_locks,
                   char *atrname, char **abp, int show_child, dbref *errobj,
                   MQUE *from_queue, int queue_type, PE_REGS *pe_regs_parent);
int one_comm_match(dbref thing, dbref player, const char *atr, const char *str,
                   MQUE *from_queue, int queue_type, PE_REGS *pe_regs_parent);
int do_set_atr(dbref thing, char const *RESTRICT atr, char const *RESTRICT s,
               dbref player, uint32_t flags);
void do_atrlock(dbref player, char const *src, char const *action);
void do_atrchown(dbref player, char const *arg1, char const *arg2);
int string_to_atrflag(dbref player, const char *p, privbits *bits);
int string_to_atrflagsets(dbref player, const char *p, privbits *setbits,
                          privbits *clrbits);
const char *atrflag_to_string(privbits mask);
void init_atr_name_tree(void);

void attr_read_all(PENNFILE *f);
void attr_write_all(PENNFILE *f);

int can_read_attr_internal(dbref player, dbref obj, ATTR *attr);
int can_write_attr_internal(dbref player, dbref obj, ATTR *attr, int safe);
bool can_edit_attr(dbref player, dbref thing, const char *attrname);
const char *atr_get_compressed_data(ATTR *atr);
char *atr_value(ATTR *atr);
char *safe_atr_value(ATTR *atr, char *check) __attribute_malloc__;

void unanchored_regexp_attr_check(dbref thing, ATTR *atr, dbref player);

/* possible attribute flags */
/* 2013-03-03 MG: Several attrflags are marked "OBSOLETE! Leave here
 * but don't use". I am fairly certainly they've been like that so
 * long it's now irrelevant, so am re-using them. If stuff explodes,
 * you know who to go to for your refund.
 */
#define AF_QUIET 0x1U        /**< Don't show a confirmation when setting. */
#define AF_INTERNAL 0x2U     /**< no one can see it or set it */
#define AF_WIZARD 0x4U       /**< Only wizards can change it */
#define AF_UNUNSED1 0x8U     /**< UNUSED! */
#define AF_LOCKED 0x10U      /**< Only creator of attrib can change it. */
#define AF_NOPROG 0x20U      /**< Won't be searched for $-commands. */
#define AF_MDARK 0x40U       /**< Only wizards can see it */
#define AF_PRIVATE 0x80U     /**< Children don't inherit it */
#define AF_NOCOPY 0x100U     /**< atr_cpy (for \@clone) doesn't copy it */
#define AF_VISUAL 0x200U     /**< Everyone can see this attribute */
#define AF_REGEXP 0x400U     /**< Match $/^ patterns using regexps */
#define AF_CASE 0x800U       /**< Match $/^ patterns case-sensitively */
#define AF_SAFE 0x1000U      /**< This attribute may not be modified */
#define AF_ROOT 0x2000U      /**< INTERNAL: Root of an attribute tree */
#define AF_RLIMIT 0x4000U    /**< Attr value must match a regular expression */
#define AF_ENUM 0x8000U      /**< Attr value must be one of a given set */
#define AF_UNUSED2 0x10000U  /**< UNUSED!  */
#define AF_COMMAND 0x20000U  /**< INTERNAL: value starts with $ */
#define AF_LISTEN 0x40000U   /**< INTERNAL: value starts with ^ */
#define AF_NODUMP 0x80000U   /**< INTERNAL: attribute is not saved */
#define AF_UNUSED3 0x100000U /**< UNUSED */
#define AF_PREFIXMATCH 0x200000U /**< Subject to prefix-matching */
#define AF_VEILED 0x400000U      /**< On ex, show presence, not value */
#define AF_DEBUG 0x800000U       /**< Show debug when evaluated */
#define AF_NEARBY 0x1000000U     /**< Override AF_VISUAL if remote */
#define AF_PUBLIC 0x2000000U     /**< Override SAFER_UFUN */
#define AF_ANON                                                                \
  0x4000000U                   /**< INTERNAL: Attribute doesn't really         \
                                  exist in the database */
#define AF_NONAME 0x8000000U   /**< No name in did_it */
#define AF_NOSPACE 0x10000000U /**< No space in did_it */
#define AF_MHEAR 0x20000000U   /**< ^-listens can be triggered by %! */
#define AF_AHEAR 0x40000000U   /**< ^-listens can be triggered by anyone */
#define AF_NODEBUG 0x80000000U /**< Don't show debug when evaluated */

/* !!! All 32 bits in the attribute flags field are in use. Don't add
 * more. */

/* Obsolete attr flag definitons, only kept for oooold db updates */
#define AF_ODARK 0x1U
#define AF_NUKED 0x8U
#define AF_STATIC 0x10000U
#define AF_LISTED 0x100000U

extern ATTR attr[]; /**< external predefined attributes. */

#define AL_ATTR(alist) (alist)
#define AL_NAME(alist) ((alist)->name)
#define AL_STR(alist) (atr_get_compressed_data((alist)))
/** The raw length of the (possibly compressed) attribute text. */
#define AL_STRLEN(alist) ((alist)->data ? chunk_len((alist)->data) : 0)
#define AL_NEXT(alist) ((alist)->next)
#define AL_CREATOR(alist) ((alist)->creator)
#define AL_FLAGS(alist) ((alist)->flags)
#define AL_DEREFS(alist) ((alist)->data ? chunk_derefs((alist)->data) : 0)

#endif /* __ATTRIB_H */
