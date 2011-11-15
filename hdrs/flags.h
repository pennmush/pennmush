/** \file flags.h
 *
 * \brief flag and powers stuff
 */

#ifndef __FLAGS_H
#define __FLAGS_H

#include "confmagic.h"
#include "mushtype.h"
#include "ptab.h"
#include "dbio.h"

typedef struct flag_info FLAG;

/** A flag.
 * This structure represents a flag in the table of flags that are
 * available for setting on objects in the game.
 */
struct flag_info {
  const char *name;     /**< Name of the flag */
  char letter;          /**< Flag character, which may be nul */
  int type;             /**< Bitflags of object types this flag applies to */
  int bitpos;           /**< Bit position assigned to this flag for now */
  uint32_t perms;            /**< Bitflags of who can set this flag */
  uint32_t negate_perms;     /**< Bitflags of who can clear this flag */
};

typedef struct flag_alias FLAG_ALIAS;

/** A flag alias.
 * A simple structure that associates an alias with a canonical flag name.
 */
struct flag_alias {
  const char *alias;            /**< The alias name */
  const char *realname;         /**< The real name of the flag */
};

typedef struct flagspace FLAGSPACE;
struct flagcache;

/** A flagspace.
 * A structure that contains all the information necessary to manage
 * a set of flags, powers, or whatever.
 */
struct flagspace {
  const char *name;             /**< The name of this flagspace */
  PTAB *tab;                    /**< Prefix table storing flags by name/alias */
  FLAG **flags;                 /**< Variable-length array of pointers to canonical flags, indexed by bit */
  int flagbits;                 /**< Current length of the flags array */
  FLAG *flag_table;             /**< Pointer to flag table */
  FLAG_ALIAS *flag_alias_table; /**< Pointer to flag alias table */
  struct flagcache *cache;      /**< Cache of all set flag bitsets */
};

/* From flags.c */
bool has_flag_in_space_by_name(const char *ns, dbref thing,
                               const char *flag, int type);
static inline bool
has_flag_by_name(dbref thing, const char *flag, int type)
{
  return has_flag_in_space_by_name("FLAG", thing, flag, type);
}

static inline bool
has_power_by_name(dbref thing, const char *flag, int type)
{
  return has_flag_in_space_by_name("POWER", thing, flag, type);
}

const char *unparse_flags(dbref thing, dbref player);
const char *flag_description(dbref player, dbref thing);
bool sees_flag(const char *ns, dbref privs, dbref thing, const char *name);
void set_flag(dbref player, dbref thing, const char *flag, int negate,
              int hear, int listener);
void set_power(dbref player, dbref thing, const char *flag, int negate);
const char *power_description(dbref player, dbref thing);
int flaglist_check(const char *ns, dbref player, dbref it,
                   const char *fstr, int type);
int flaglist_check_long(const char *ns, dbref player, dbref it,
                        const char *fstr, int type);
FLAG *match_flag(const char *name);
FLAG *match_power(const char *name);
const char *flag_list_to_lock_string(object_flag_type flags,
                                     object_flag_type powers);

void twiddle_flag_internal(const char *ns, dbref thing, const char *flag,
                           int negate);
object_flag_type new_flag_bitmask_ns(FLAGSPACE *);
object_flag_type new_flag_bitmask(const char *ns);
object_flag_type clone_flag_bitmask(const char *ns,
                                    const object_flag_type given);
void destroy_flag_bitmask(const char *ns, const object_flag_type bitmask);
object_flag_type set_flag_bitmask_ns(FLAGSPACE *n,
                                     const object_flag_type bitmask, int bit);
object_flag_type set_flag_bitmask(const char *ns,
                                  const object_flag_type bitmask, int bit);
object_flag_type clear_flag_bitmask_ns(FLAGSPACE *n,
                                       const object_flag_type bitmask, int bit);
object_flag_type clear_flag_bitmask(const char *ns,
                                    const object_flag_type bitmask, int bit);
bool has_bit(const object_flag_type bitmask, int bitpos);
bool has_all_bits(const char *ns, const object_flag_type source,
                  const object_flag_type bitmask);
bool null_flagmask(const char *ns, const object_flag_type source);
bool has_any_bits(const char *ns, const object_flag_type source,
                  const object_flag_type bitmask);
object_flag_type string_to_bits(const char *ns, const char *str);
const char *bits_to_string(const char *ns, object_flag_type bitmask,
                           dbref privs, dbref thing);
void flag_write_all(PENNFILE *, const char *);
void flag_read_all(PENNFILE *, const char *);
int type_from_old_flags(long old_flags);
object_flag_type flags_from_old_flags(const char *ns, long old_flags,
                                      long old_toggles, int type);
FLAG *add_flag_generic(const char *ns, const char *name,
                       const char letter, int type, int perms,
                       int negate_perms);
#define add_flag(n,l,t,p,x) add_flag_generic("FLAG",n,l,t,p,x)
#define add_power(n,l,t,p,x) add_flag_generic("POWER",n,l,t,p,x)
int alias_flag_generic(const char *ns, const char *name, const char *alias);
#define alias_flag(n,a) alias_flag_generic("FLAG",n,a);
#define alias_power(n,a) alias_flag_generic("POWER",n,a);
void do_list_flags(const char *ns, dbref player, const char *arg, int lc,
                   const char *label);
char *list_all_flags(const char *ns, const char *name, dbref privs, int which);
void do_flag_info(const char *ns, dbref player, const char *name);
void do_flag_delete(const char *ns, dbref player, const char *name);
void do_flag_disable(const char *ns, dbref player, const char *name);
void do_flag_alias(const char *ns, dbref player, const char *name,
                   const char *alias);
void do_flag_enable(const char *ns, dbref player, const char *name);
void do_flag_restrict(const char *ns, dbref player, const char *name,
                      char *args_right[]);
void do_flag_type(const char *ns, dbref player, const char *name,
                  char *type_string);
void do_flag_add(const char *ns, dbref player, const char *name,
                 char *args_right[]);
void do_flag_letter(const char *ns, dbref player, const char *name,
                    const char *letter);
const char *power_to_string(int pwr);
void decompile_flags_generic(dbref player, dbref thing, const char *name,
                             const char *ns, const char *command,
                             const char *prefix);
int good_flag_name(char const *s);
#define decompile_flags(p,t,n,r) decompile_flags_generic(p,t,n,"FLAG","@set",r)
#define decompile_powers(p,t,n,r) decompile_flags_generic(p,t,n,"POWER","@power",r)
#define has_all_flags_by_mask(x,bm) has_all_bits("FLAG",Flags(x),bm)
#define has_any_flags_by_mask(x,bm) has_any_bits("FLAG",Flags(x),bm)
#define has_all_powers_by_mask(x,bm) has_all_bits("POWER",Powers(x),bm)
#define has_any_powers_by_mask(x,bm) has_any_bits("POWER",Powers(x),bm)
#define set_flag_internal(t,f) twiddle_flag_internal("FLAG",t,f,0)
#define clear_flag_internal(t,f) twiddle_flag_internal("FLAG",t,f,1)
#define set_power_internal(t,f) twiddle_flag_internal("POWER",t,f,0)
#define clear_power_internal(t,f) twiddle_flag_internal("POWER",t,f,1)

void flag_stats(dbref);

/*---------------------------------------------------------------------
 * Object types (no longer part of the flags)
 */

#define TYPE_ROOM       0x1
#define TYPE_THING      0x2
#define TYPE_EXIT       0x4
#define TYPE_PLAYER     0x8
#define TYPE_GARBAGE    0x10
#define TYPE_MARKED     0x20
#define NOTYPE          0xFFFF



/*--------------------------------------------------------------------------
 * Flag permissions
 */

#define F_ANY           0x10U    /**< can be set by anyone - obsolete now */
#define F_INHERIT       0x20U    /**< must pass inherit check */
#define F_OWNED         0x40U    /**< can be set on owned objects */
#define F_ROYAL         0x80U    /**< can only be set by royalty */
#define F_WIZARD        0x100U   /**< can only be set by wizards */
#define F_GOD           0x200U   /**< can only be set by God */
#define F_INTERNAL      0x400U   /**< only the game can set this */
#define F_DARK          0x800U   /**< only God can see this flag */
#define F_MDARK         0x1000U  /**< admin/God can see this flag */
#define F_ODARK         0x2000U  /**< owner/admin/God can see this flag */
#define F_DISABLED      0x4000U  /**< flag can't be used */
#define F_LOG           0x8000U  /**< Log when the flag is set/cleared */
#define F_EVENT         0x10000U /**< Trigger an event wen a flag is set/cleared */

#define F_MAX           0x00800000U /**< Largest allowed flag bit */


/* Flags can be in the flaglist multiple times, thanks to aliases. Keep
   a reference count of how many times, and free memory when it goes to 0. */
#define F_REF_MASK      0xFF000000U /**< Mask to get the reference count */
#define F_REF_NOT       0x00FFFFFFU /**< Everything but */
#define FLAG_REF(r)     (((r) & F_REF_MASK) >> 30)
#define ZERO_FLAG_REF(r) ((r) & F_REF_NOT)
#define INCR_FLAG_REF(r) ((r) + (1 << 30))
#define DECR_FLAG_REF(r) ((r) - (1 << 30))


/*--------------------------------------------------------------------------
 * Powers table
 */

#define CAN_BUILD       0x10    /* can use builder commands */
#define TEL_ANYWHERE    0x20    /* teleport self anywhere */
#define TEL_OTHER       0x40    /* teleport someone else */
#define SEE_ALL         0x80    /* can examine all and use priv WHO */
#define NO_PAY          0x100   /* Needs no money */
#define CHAT_PRIVS      0x200   /* can use restricted channels */
#define CAN_HIDE        0x400   /* can go DARK on the WHO list */
#define LOGIN_ANYTIME   0x800   /* not affected by restricted logins */
#define UNLIMITED_IDLE  0x1000  /* no inactivity timeout */
#define LONG_FINGERS    0x2000  /* can grab stuff remotely */
#define CAN_BOOT        0x4000  /* can boot off players */
#define CHANGE_QUOTAS   0x8000  /* can change other players' quotas */
#define SET_POLL        0x10000 /* can change the poll */
#define HUGE_QUEUE      0x20000 /* queue limit of db_top + 1 */
#define PS_ALL          0x40000 /* look at anyone's queue */
#define HALT_ANYTHING   0x80000 /* do @halt on others, and @allhalt */
#define SEARCH_EVERYTHING  0x100000     /* @stats, @search, @entrances all */
#define GLOBAL_FUNCS    0x200000        /* add global functions */
#define CREATE_PLAYER   0x400000        /* @pcreate */
#define IS_GUEST        0x800000        /* Guest, restrict access */
#define CAN_WALL        0x1000000       /* @wall */
#define CEMIT           0x2000000       /* Was: Can @cemit */
#define UNKILLABLE      0x4000000       /* Cannot be killed */
#define PEMIT_ALL       0x8000000       /* Can @pemit to HAVEN players */
#define NO_QUOTA        0x10000000      /* Has no quota restrictions */
#define LINK_ANYWHERE   0x20000000      /* Can @link an exit to any room */
#define OPEN_ANYWHERE   0x40000000      /* Can @open an exit from any room */
#define CAN_NSPEMIT     0x80000000      /* Can use @nspemit and nspemit() */

/* These powers are obsolete, but are kept around to implement
 * DBF_SPLIT_IMMORTAL
 */
#define CAN_DEBUG       0x4000000       /* Can set/unset the debug flag */
#define IMMORTAL        0x100   /* cannot be killed, uses no money */
#endif                          /* __FLAGS_H */
