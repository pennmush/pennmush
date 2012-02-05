/**
 * \file flags.c
 *
 * \brief Flags and powers (and sometimes object types) in PennMUSH
 *
 *
 * Functions to cope with flags and powers (and also object types,
 * in some cases).
 *
 * Flag functions actually involve with several related entities:
 *  Flag spaces (FLAGSPACE objects)
 *  Flag definitions (FLAG objects)
 *  Bitmasks representing sets of flags (object_flag_type's). The
 *    bits involved may differ between dbs.
 *  Strings of space-separated flag names. This is a string representation
 *    of a bitmask, suitable for display and storage
 *  Strings of flag characters
 *
 */

#include "config.h"

#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <string.h>
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "command.h"
#include "attrib.h"
#include "mushdb.h"
#include "parse.h"
#include "match.h"
#include "ptab.h"
#include "htab.h"
#include "privtab.h"
#include "game.h"
#include "flags.h"
#include "dbdefs.h"
#include "lock.h"
#include "log.h"
#include "dbio.h"
#include "sort.h"
#include "mymalloc.h"
#include "oldflags.h"
#include "confmagic.h"


static bool can_set_flag(dbref player, dbref thing, FLAG *flagp, int negate);
static FLAG *letter_to_flagptr(FLAGSPACE *n, char c, int type);
static void flag_add(FLAGSPACE *n, const char *name, FLAG *f);
static bool has_flag_ns(FLAGSPACE *n, dbref thing, FLAG *f);

static FLAG *flag_read(PENNFILE *in);
static FLAG *flag_read_oldstyle(PENNFILE *in);
static void flag_read_all_oldstyle(PENNFILE *in, const char *ns);
static void flag_write(PENNFILE *out, FLAG *f, const char *name);
static FLAG *flag_hash_lookup(FLAGSPACE *n, const char *name, int type);
static FLAG *clone_flag(FLAG *f);
static FLAG *new_flag(void);
static void flag_add_additional(FLAGSPACE *n);
static char *list_aliases(FLAGSPACE *n, FLAG *given);
static void realloc_object_flag_bitmasks(FLAGSPACE *n);
static FLAG *match_flag_ns(FLAGSPACE *n, const char *name);

/* Flag bitset cache data structures. All objects with the same flags
   set share the same storage space. */

struct flagbucket {
  object_flag_type key;
  int refcount;
  struct flagbucket *next;
};

struct flagcache {
  int size;
  int zero_refcount;
  int entries;
  object_flag_type zero;
  struct flagbucket **buckets;
  slab *flagset_slab;
};

static struct flagcache *new_flagcache(FLAGSPACE *, int);
static void free_flagcache(struct flagcache *);
static object_flag_type flagcache_find_ns(FLAGSPACE *, object_flag_type);

slab *flagbucket_slab = NULL;

PTAB ptab_flag;                 /**< Table of flags by name, inc. aliases */
PTAB ptab_power;                /**< Table of powers by name, inc. aliases */
HASHTAB htab_flagspaces;                /**< Hash of flagspaces */
slab *flag_slab = NULL;
extern PTAB ptab_command;       /* Uses flag bitmasks */

/** Attempt to find a flagspace from its name */
#define Flagspace_Lookup(n,ns)  if (!(n = (FLAGSPACE *)hashfind(ns,&htab_flagspaces))) mush_panic("Unable to locate flagspace");

/** This is the old default flag table. We still use it when we have to
 * convert old dbs, but once you have a converted db, it's the flag
 * table in the db that counts, not this one.
 */
/* Name     Letter   Type(s)   Flag   Perms   Negate_Perm */
static FLAG flag_table[] = {
  {"CHOWN_OK", 'C', NOTYPE, CHOWN_OK, F_ANY, F_ANY},
  {"DARK", 'D', NOTYPE, DARK, F_ANY, F_ANY},
  {"GOING", 'G', NOTYPE, GOING, F_INTERNAL, F_INTERNAL},
  {"HAVEN", 'H', NOTYPE, HAVEN, F_ANY, F_ANY},
  {"TRUST", 'I', NOTYPE, INHERIT, F_INHERIT, F_INHERIT},
  {"LINK_OK", 'L', NOTYPE, LINK_OK, F_ANY, F_ANY},
  {"OPAQUE", 'O', NOTYPE, LOOK_OPAQUE, F_ANY, F_ANY},
  {"QUIET", 'Q', NOTYPE, QUIET, F_ANY, F_ANY},
  {"STICKY", 'S', NOTYPE, STICKY, F_ANY, F_ANY},
  {"UNFINDABLE", 'U', NOTYPE, UNFIND, F_ANY, F_ANY},
  {"VISUAL", 'V', NOTYPE, VISUAL, F_ANY, F_ANY},
  {"WIZARD", 'W', NOTYPE, WIZARD, F_INHERIT | F_WIZARD | F_LOG,
   F_INHERIT | F_WIZARD},
  {"SAFE", 'X', NOTYPE, SAFE, F_ANY, F_ANY},
  {"AUDIBLE", 'a', NOTYPE, AUDIBLE, F_ANY, F_ANY},
  {"DEBUG", 'b', NOTYPE, DEBUGGING, F_ANY, F_ANY},
  {"NO_WARN", 'w', NOTYPE, NOWARN, F_ANY, F_ANY},
  {"ENTER_OK", 'e', NOTYPE, ENTER_OK, F_ANY, F_ANY},
  {"HALT", 'h', NOTYPE, HALT, F_ANY, F_ANY},
  {"NO_COMMAND", 'n', NOTYPE, NO_COMMAND, F_ANY, F_ANY},
  {"LIGHT", 'l', NOTYPE, LIGHT, F_ANY, F_ANY},
  {"ROYALTY", 'r', NOTYPE, ROYALTY, F_INHERIT | F_ROYAL | F_LOG,
   F_INHERIT | F_ROYAL},
  {"TRANSPARENT", 't', NOTYPE, TRANSPARENTED, F_ANY, F_ANY},
  {"VERBOSE", 'v', NOTYPE, VERBOSE, F_ANY, F_ANY},
  {"ANSI", 'A', TYPE_PLAYER, PLAYER_ANSI, F_ANY, F_ANY},
  {"COLOR", 'C', TYPE_PLAYER, PLAYER_COLOR, F_ANY, F_ANY},
  {"MONITOR", 'M', TYPE_PLAYER | TYPE_ROOM | TYPE_THING, 0, F_ANY, F_ANY},
  {"NOSPOOF", '"', TYPE_PLAYER, PLAYER_NOSPOOF, F_ANY | F_ODARK,
   F_ANY | F_ODARK},
  {"SHARED", 'Z', TYPE_PLAYER, PLAYER_ZONE, F_ANY, F_ANY},
  {"TRACK_MONEY", '\0', TYPE_PLAYER, 0, F_ANY, F_ANY},
  {"CONNECTED", 'c', TYPE_PLAYER, PLAYER_CONNECT, F_INTERNAL | F_MDARK,
   F_INTERNAL | F_MDARK},
  {"GAGGED", 'g', TYPE_PLAYER, PLAYER_GAGGED, F_WIZARD, F_WIZARD},
  {"MYOPIC", 'm', TYPE_PLAYER, PLAYER_MYOPIC, F_ANY, F_ANY},
  {"TERSE", 'x', TYPE_PLAYER | TYPE_THING, PLAYER_TERSE, F_ANY, F_ANY},
  {"JURY_OK", 'j', TYPE_PLAYER, PLAYER_JURY, F_ROYAL, F_ROYAL},
  {"JUDGE", 'J', TYPE_PLAYER, PLAYER_JUDGE, F_ROYAL, F_ROYAL},
  {"FIXED", 'F', TYPE_PLAYER, PLAYER_FIXED, F_WIZARD, F_WIZARD},
  {"UNREGISTERED", '?', TYPE_PLAYER, PLAYER_UNREG, F_ROYAL, F_ROYAL},
  {"ON-VACATION", 'o', TYPE_PLAYER, PLAYER_VACATION, F_ANY, F_ANY},
  {"SUSPECT", 's', TYPE_PLAYER, PLAYER_SUSPECT, F_WIZARD | F_MDARK | F_LOG,
   F_WIZARD | F_MDARK},
  {"PARANOID", '\0', TYPE_PLAYER, PLAYER_PARANOID, F_ANY | F_ODARK,
   F_ANY | F_ODARK},
  {"NOACCENTS", '~', TYPE_PLAYER, PLAYER_NOACCENTS, F_ANY, F_ANY},
  {"DESTROY_OK", 'd', TYPE_THING, THING_DEST_OK, F_ANY, F_ANY},
  {"PUPPET", 'p', TYPE_THING, THING_PUPPET, F_ANY, F_ANY},
  {"NO_LEAVE", 'N', TYPE_THING, THING_NOLEAVE, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_THING | TYPE_ROOM, 0, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_THING | TYPE_ROOM, 0, F_ANY, F_ANY},
  {"ABODE", 'A', TYPE_ROOM, ROOM_ABODE, F_ANY, F_ANY},
  {"FLOATING", 'F', TYPE_ROOM, ROOM_FLOATING, F_ANY, F_ANY},
  {"JUMP_OK", 'J', TYPE_ROOM, ROOM_JUMP_OK, F_ANY, F_ANY},
  {"NO_TEL", 'N', TYPE_ROOM, ROOM_NO_TEL, F_ANY, F_ANY},
  {"UNINSPECTED", 'u', TYPE_ROOM, ROOM_UNINSPECT, F_ROYAL, F_ROYAL},
  {"CLOUDY", 'x', TYPE_EXIT, EXIT_CLOUDY, F_ANY, F_ANY},
  {"GOING_TWICE", '\0', NOTYPE, GOING_TWICE, F_INTERNAL | F_DARK,
   F_INTERNAL | F_DARK},
  {"KEEPALIVE", 'k', TYPE_PLAYER, 0, F_ANY, F_ANY},
  {"NO_LOG", '\0', NOTYPE, 0, F_WIZARD | F_MDARK | F_LOG, F_WIZARD | F_MDARK},
  {"OPEN_OK", '\0', TYPE_ROOM, 0, F_ANY, F_ANY},
  {NULL, '\0', 0, 0, 0, 0}
};

/** The old table to kludge multi-type toggles. Now used only
 * for conversion.
 */
static FLAG hack_table[] = {
  {"MONITOR", 'M', TYPE_PLAYER, PLAYER_MONITOR, F_ROYAL, F_ROYAL},
  {"MONITOR", 'M', TYPE_THING, THING_LISTEN, F_ANY, F_ANY},
  {"MONITOR", 'M', TYPE_ROOM, ROOM_LISTEN, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_THING, THING_INHEARIT, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_ROOM, ROOM_INHEARIT, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_THING, THING_Z_TEL, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_ROOM, ROOM_Z_TEL, F_ANY, F_ANY},
  {NULL, '\0', 0, 0, 0, 0}
};


/** A table of types, as if they were flags. Some functions that
 * expect flags also accept, for historical reasons, types.
 */
static FLAG type_table[] = {
  {"PLAYER", 'P', TYPE_PLAYER, TYPE_PLAYER, F_INTERNAL, F_INTERNAL},
  {"ROOM", 'R', TYPE_ROOM, TYPE_ROOM, F_INTERNAL, F_INTERNAL},
  {"EXIT", 'E', TYPE_EXIT, TYPE_EXIT, F_INTERNAL, F_INTERNAL},
  {"THING", 'T', TYPE_THING, TYPE_THING, F_INTERNAL, F_INTERNAL},
  {NULL, '\0', 0, 0, 0, 0}
};

/** A table of types, as privileges. */
static PRIV type_privs[] = {
  {"PLAYER", 'P', TYPE_PLAYER, TYPE_PLAYER},
  {"ROOM", 'R', TYPE_ROOM, TYPE_ROOM},
  {"EXIT", 'E', TYPE_EXIT, TYPE_EXIT},
  {"THING", 'T', TYPE_THING, TYPE_THING},
  {NULL, '\0', 0, 0}
};

/** The old default aliases for flags. This table is only used in conversion
 * of old databases. Once a database is converted, the alias list in the
 * database is what counts.
 */
static FLAG_ALIAS flag_alias_tab[] = {
  {"INHERIT", "TRUST"},
  {"TRACE", "DEBUG"},
  {"NOWARN", "NO_WARN"},
  {"NOCOMMAND", "NO_COMMAND"},
  {"LISTENER", "MONITOR"},
  {"WATCHER", "MONITOR"},
  {"ZONE", "SHARED"},
  {"COLOUR", "COLOR"},
  {"JURYOK", "JURY_OK"},
#ifdef VACATION_FLAG
  {"VACATION", "ON-VACATION"},
#endif
  {"DEST_OK", "DESTROY_OK"},
  {"NOLEAVE", "NO_LEAVE"},
  {"TEL_OK", "JUMP_OK"},
  {"TELOK", "JUMP_OK"},
  {"TEL-OK", "JUMP_OK"},
  {"^", "LISTEN_PARENT"},

  {NULL, NULL}
};

/** This is the old defaultpowr table. We still use it when we
 * have to convert old dbs, but once you have a converted db,
 * it's the power table in the db that counts, not this one.
 */
/*   Name      Flag   */
static FLAG power_table[] = {
  {"Announce", '\0', NOTYPE, CAN_WALL, F_WIZARD | F_LOG, F_WIZARD},
  {"Boot", '\0', NOTYPE, CAN_BOOT, F_WIZARD | F_LOG, F_WIZARD},
  {"Builder", '\0', NOTYPE, CAN_BUILD, F_WIZARD | F_LOG, F_WIZARD},
  {"Cemit", '\0', NOTYPE, CEMIT, F_WIZARD | F_LOG, F_WIZARD},
  {"Chat_Privs", '\0', NOTYPE, CHAT_PRIVS, F_WIZARD | F_LOG, F_WIZARD},
  {"Functions", '\0', NOTYPE, GLOBAL_FUNCS, F_WIZARD | F_LOG, F_WIZARD},
  {"Guest", '\0', NOTYPE, IS_GUEST, F_WIZARD | F_LOG, F_WIZARD},
  {"Halt", '\0', NOTYPE, HALT_ANYTHING, F_WIZARD | F_LOG, F_WIZARD},
  {"Hide", '\0', NOTYPE, CAN_HIDE, F_WIZARD | F_LOG, F_WIZARD},
  {"Idle", '\0', NOTYPE, UNLIMITED_IDLE, F_WIZARD | F_LOG, F_WIZARD},
  {"Immortal", '\0', NOTYPE, NO_PAY | NO_QUOTA | UNKILLABLE, F_WIZARD,
   F_WIZARD},
  {"Link_Anywhere", '\0', NOTYPE, LINK_ANYWHERE, F_WIZARD | F_LOG, F_WIZARD},
  {"Login", '\0', NOTYPE, LOGIN_ANYTIME, F_WIZARD | F_LOG, F_WIZARD},
  {"Long_Fingers", '\0', NOTYPE, LONG_FINGERS, F_WIZARD | F_LOG, F_WIZARD},
  {"No_Pay", '\0', NOTYPE, NO_PAY, F_WIZARD | F_LOG, F_WIZARD},
  {"No_Quota", '\0', NOTYPE, NO_QUOTA, F_WIZARD | F_LOG, F_WIZARD},
  {"Open_Anywhere", '\0', NOTYPE, OPEN_ANYWHERE, F_WIZARD | F_LOG, F_WIZARD},
  {"Pemit_All", '\0', NOTYPE, PEMIT_ALL, F_WIZARD | F_LOG, F_WIZARD},
  {"Player_Create", '\0', NOTYPE, CREATE_PLAYER, F_WIZARD | F_LOG, F_WIZARD},
  {"Poll", '\0', NOTYPE, SET_POLL, F_WIZARD | F_LOG, F_WIZARD},
  {"Queue", '\0', NOTYPE, HUGE_QUEUE, F_WIZARD | F_LOG, F_WIZARD},
  {"Quotas", '\0', NOTYPE, CHANGE_QUOTAS, F_WIZARD | F_LOG, F_WIZARD},
  {"Search", '\0', NOTYPE, SEARCH_EVERYTHING, F_WIZARD | F_LOG, F_WIZARD},
  {"See_All", '\0', NOTYPE, SEE_ALL, F_WIZARD | F_LOG, F_WIZARD},
  {"See_Queue", '\0', NOTYPE, PS_ALL, F_WIZARD | F_LOG, F_WIZARD},
  {"Tport_Anything", '\0', NOTYPE, TEL_OTHER, F_WIZARD | F_LOG, F_WIZARD},
  {"Tport_Anywhere", '\0', NOTYPE, TEL_ANYWHERE, F_WIZARD | F_LOG, F_WIZARD},
  {"Unkillable", '\0', NOTYPE, UNKILLABLE, F_WIZARD | F_LOG, F_WIZARD},
  {"Can_spoof", '\0', NOTYPE, CAN_NSPEMIT, F_WIZARD | F_LOG, F_WIZARD},
  {NULL, '\0', 0, 0, 0, 0}
};

/** A table of aliases for powers. */
static FLAG_ALIAS power_alias_tab[] = {
  {"@cemit", "Cemit"},
  {"@wall", "Announce"},
  {"wall", "Announce"},
  {"Can_nspemit", "Can_spoof"},
  {NULL, NULL}
};

/** The table of flag privilege bits. */
static PRIV flag_privs[] = {
  {"trusted", '\0', F_INHERIT, F_INHERIT},
  {"owned", '\0', F_OWNED, F_OWNED},
  {"royalty", '\0', F_ROYAL, F_ROYAL},
  {"wizard", '\0', F_WIZARD, F_WIZARD},
  {"god", '\0', F_GOD, F_GOD},
  {"internal", '\0', F_INTERNAL, F_INTERNAL},
  {"dark", '\0', F_DARK, F_DARK},
  {"mdark", '\0', F_MDARK, F_MDARK},
  {"odark", '\0', F_ODARK, F_ODARK},
  {"disabled", '\0', F_DISABLED, F_DISABLED},
  {"log", '\0', F_LOG, F_LOG},
  {"event", '\0', F_EVENT, F_EVENT},
  {NULL, '\0', 0, 0}
};

/*---------------------------------------------------------------------------
 * Flag definition functions, including flag hash table handlers
 */

/** Convenience function to return a pointer to a flag struct
 * given the name.
 * \param name name of flag to find.
 * \return poiner to flag structure, or NULL.
 */
FLAG *
match_flag(const char *name)
{
  return (FLAG *) match_flag_ns(hashfind("FLAG", &htab_flagspaces), name);
}

/** Convenience function to return a pointer to a flag struct
 * given the name.
 * \param name name of flag to find.
 * \return poiner to flag structure, or NULL.
 */
FLAG *
match_power(const char *name)
{
  return (FLAG *) match_flag_ns(hashfind("POWER", &htab_flagspaces), name);
}

/** Convenience function to return a pointer to a flag struct
 * given the name.
 * \param name name of flag to find.
 * \return poiner to flag structure, or NULL.
 */
static FLAG *
match_flag_ns(FLAGSPACE *n, const char *name)
{
  return (FLAG *) ptab_find(n->tab, name);
}

/** Given a flag name and mask of types, return a pointer to a flag struct.
 * This function first attempts to match the flag name to a flag of the
 * right type. If that fails, it tries to match flag characters if the
 * name is a single character. If all else fails, it tries to match
 * against an object type name.
 * \param n pointer to flagspace to search.
 * \param name name of flag to find.
 * \param type mask of desired flag object types.
 * \return pointer to flag structure, or NULL.
 */
static FLAG *
flag_hash_lookup(FLAGSPACE *n, const char *name, int type)
{
  FLAG *f;

  f = match_flag_ns(n, name);
  if (f && !(f->perms & F_DISABLED)) {
    if (f->type & type)
      return f;
    return NULL;
  }

  /* If the name is a single character, search the flag characters */
  if (name && *name && !*(name + 1)) {
    if ((f = letter_to_flagptr(n, *name, type)))
      return f;
  }

  if (n->tab == &ptab_flag) {
    /* provided for backwards compatibility: type flag checking */
    if (n->flag_table == flag_table) {
      for (f = type_table; f->name != NULL; f++)
        if (string_prefix(name, f->name))
          return f;
    }
  }

  return NULL;
}

/* Allocate a new FLAG definition */
static FLAG *
new_flag(void)
{
  FLAG *f;

  if (flag_slab == NULL)
    flag_slab = slab_create("flags", sizeof(FLAG));
  f = slab_malloc(flag_slab, NULL);
  if (!f)
    mush_panic("Unable to allocate memory for a new flag!\n");
  return f;
}

/* Deallocate all flag-related memory */
static void
clear_all_flags(FLAGSPACE *n)
{
  FLAG *f;

  for (f = ptab_firstentry(n->tab); f; f = ptab_nextentry(n->tab)) {
    f->perms = DECR_FLAG_REF(f->perms);
    if (FLAG_REF(f->perms) == 0) {
      mush_free((void *) f->name, "flag.name");
      slab_free(flag_slab, f);
    }
  }

  ptab_free(n->tab);

  /* Finally, the flags array */
  if (n->flags)
    mush_free(n->flags, "flagspace.flags");
  n->flags = NULL;
  n->flagbits = 0;

}

static FLAG *
clone_flag(FLAG *f)
{
  FLAG *clone = new_flag();
  clone->name = mush_strdup(f->name, "flag.name");
  clone->letter = f->letter;
  clone->type = f->type;
  clone->bitpos = f->bitpos;
  clone->perms = f->perms;
  clone->negate_perms = f->negate_perms;
  return clone;
}

/* This is a stub function to add a flag. It performs no error-checking,
 * so it's up to you to be sure you're adding a flag that's properly
 * set up and that'll work ok. If called with autopos == 0, this
 * auto-allocates the next bitpos. Otherwise, bitpos is ignored and
 * f->bitpos is used.
 */
static void
flag_add(FLAGSPACE *n, const char *name, FLAG *f)
{
  /* If this flag has no bitpos assigned, assign it the next one.
   * We could improve this algorithm to use the next available
   * slot after deletions, too, but this will do for now.
   */

  /* Can't have more than 255 references to the same flag */
  if (FLAG_REF(f->perms) == 0xFFU)
    return;

  if (f->bitpos < 0)
    f->bitpos = n->flagbits;

  f->perms = INCR_FLAG_REF(f->perms);

  /* Insert the flag in the ptab by the given name (maybe an alias) */
  ptab_insert_one(n->tab, name, f);

  /* Is this a canonical flag (as opposed to an alias?)
   * If it's an alias, we're done.
   * A canonical flag has either been given a new bitpos
   * or has not yet been stored in the flags array.
   * (An alias would have a previously used bitpos that's already
   * indexing a flag in the flags array)
   */
  if ((f->bitpos >= n->flagbits) || (n->flags[f->bitpos] == NULL)) {
    /* It's a canonical flag */
    int i;
    if (f->bitpos >= n->flagbits) {
      /* Oops, we need a bigger array */
      n->flags =
        mush_realloc(n->flags, (f->bitpos + 1) * sizeof(FLAG *),
                     "flagspace.flags");
      if (!n->flags)
        mush_panic("Unable to reallocate flags array!\n");

      /* Make sure the new space is full of NULLs */
      for (i = n->flagbits; i <= f->bitpos; i++)
        n->flags[i] = NULL;
    }
    /* Put the canonical flag in the flags array */
    n->flags[f->bitpos] = f;
    n->flagbits = f->bitpos + 1;
    if (n->flagbits % 8 == 1) {
      /* We've crossed over a byte boundary, so we need to realloc
       * all the flags on all our objects to get them an additional
       * byte.
       */
      realloc_object_flag_bitmasks(n);
    }
  }
}

/** Locate a specific byte given a bit position */
static inline uint32_t
FlagByte(uint32_t x)
{
  return x / 8;
}

/** Locate a specific bit within a byte given a bit position */
static inline uint32_t
FlagBit(uint32_t x)
{
  return 7 - (x % 8);
}

/** How many bytes do we need for a flag bitmask? */
static inline uint32_t
FlagBytes(const FLAGSPACE *n)
{
  return (n->flagbits + 7) / 8;
}

static object_flag_type
extend_bitmask(FLAGSPACE *n, object_flag_type old, int oldlen)
{
  object_flag_type grown = slab_malloc(n->cache->flagset_slab, NULL);
  memset(grown, 0, FlagBytes(n));
  memcpy(grown, old, oldlen);
  return grown;
}

struct flagpair {
  object_flag_type orig;
  object_flag_type grown;
  struct flagpair *next;
};

static void
realloc_object_flag_bitmasks(FLAGSPACE *n)
{
  dbref it;
  struct flagcache *oldcache;
  struct flagpair *migrate, *m;
  slab *flagpairs;
  int i, numbytes;

  numbytes = FlagBytes(n);

  oldcache = n->cache;
  n->cache = new_flagcache(n, (double) oldcache->size * 1.1);

  flagpairs = slab_create("flagpairs", sizeof *migrate);
  migrate = slab_malloc(flagpairs, NULL);
  migrate->orig = oldcache->zero;
  migrate->grown = n->cache->zero;
  migrate->next = NULL;

  /* Grow all current flagsets, and store the old/new locations */
  for (i = 0; i < n->cache->size; i += 1) {
    struct flagbucket *b;

    for (b = n->cache->buckets[i]; b; b = b->next) {
      object_flag_type grown;
      struct flagpair *newpair;

      grown = extend_bitmask(n, b->key, numbytes - 1);
      flagcache_find_ns(n, grown);

      newpair = slab_malloc(flagpairs, NULL);
      newpair->orig = b->key;
      newpair->grown = grown;
      newpair->next = migrate;
      migrate = newpair;
    }
  }

  /* Now adjust pointers in the db from old to new. This has poor
     big-O performance, but isn't done very often, so we can live with it. */
  for (it = 0; it < db_top; it += 1) {
    for (m = migrate; m; m = m->next) {
      if (n->tab == &ptab_flag) {
        if (Flags(it) == m->orig) {
          Flags(it) = m->grown;
          break;
        } else if (Powers(it) == m->orig) {
          Powers(it) = m->grown;
          break;
        }
      }
    }
  }
  slab_destroy(flagpairs);
  free_flagcache(oldcache);
}


/* Read in a flag from a file and return it */
static FLAG *
flag_read_oldstyle(PENNFILE *in)
{
  FLAG *f;
  char *c;
  c = mush_strdup(getstring_noalloc(in), "flag.name");
  if (!strcmp(c, "FLAG ALIASES")) {
    mush_free(c, "flag.name");
    return NULL;                /* We're done */
  }
  f = new_flag();
  f->name = c;
  c = (char *) getstring_noalloc(in);
  f->letter = *c;
  f->bitpos = -1;
  f->type = getref(in);
  f->perms = getref(in);
  f->negate_perms = getref(in);
  return f;
}

static FLAG *
flag_alias_read_oldstyle(PENNFILE *in, char *alias, FLAGSPACE *n)
{
  FLAG *f;
  char *c;
  /* Real name first */
  c = mush_strdup(getstring_noalloc(in), "flag alias");
  if (!strcmp(c, "END OF FLAGS")) {
    mush_free(c, "flag alias");
    return NULL;                /* We're done */
  }
  f = match_flag_ns(n, c);
  if (!f) {
    /* Corrupt db. Recover as well as we can. */
    do_rawlog(LT_ERR,
              "FLAG READ: flag alias %s matches no known flag. Skipping aliases.",
              c);
    mush_free(c, "flag alias");
    do {
      c = (char *) getstring_noalloc(in);
    } while (strcmp(c, "END OF FLAGS"));
    return NULL;
  } else
    mush_free(c, "flag alias");

  /* Get the alias name */
  strcpy(alias, getstring_noalloc(in));
  return f;
}

/** Read flags and aliases from the database. This function expects
 * to receive file pointer that's already reading in a database file
 * and pointing at the start of the flag table. It reads the flags,
 * reads the aliases, and then does any additional flag adding that
 * needs to happen.
 * \param in file pointer to read from.
 * \param ns name of namespace to search.
 */
static void
flag_read_all_oldstyle(PENNFILE *in, const char *ns)
{
  FLAG *f;
  FLAGSPACE *n;
  char alias[BUFFER_LEN];

  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, "FLAG READ: Unable to locate flagspace %s.", ns);
    return;
  }
  /* If we are reading flags from the db, they are definitive. */
  clear_all_flags(n);
  while ((f = flag_read_oldstyle(in))) {
    flag_add(n, f->name, f);
  }
  /* Assumes we'll always have at least one alias */
  while ((f = flag_alias_read_oldstyle(in, alias, n))) {
    flag_add(n, alias, f);
  }
  flag_add_additional(n);
}

/* Read in a flag from a file and return it */
static FLAG *
flag_read(PENNFILE *in)
{
  FLAG *f;
  char *c;
  char *tmp;

  db_read_this_labeled_string(in, "name", &tmp);
  c = mush_strdup(tmp, "flag.name");
  f = new_flag();
  f->name = c;
  db_read_this_labeled_string(in, "letter", &tmp);
  f->letter = *tmp;
  f->bitpos = -1;
  db_read_this_labeled_string(in, "type", &tmp);
  f->type = string_to_privs(type_privs, tmp, 0);
  db_read_this_labeled_string(in, "perms", &tmp);
  f->perms = F_REF_NOT & string_to_privs(flag_privs, tmp, 0);
  db_read_this_labeled_string(in, "negate_perms", &tmp);
  f->negate_perms = string_to_privs(flag_privs, tmp, 0);
  return f;
}

static FLAG *
flag_alias_read(PENNFILE *in, char *alias, FLAGSPACE *n)
{
  FLAG *f;
  char *c;
  char *tmp;
  /* Real name first */
  db_read_this_labeled_string(in, "name", &tmp);
  c = mush_strdup(tmp, "flag alias");
  f = match_flag_ns(n, c);
  if (!f) {
    /* Corrupt db. Recover as well as we can. */
    do_rawlog(LT_ERR,
              "FLAG READ: flag alias %s matches no known flag. Skipping this alias.",
              c);
    mush_free(c, "flag alias");
    (void) getstring_noalloc(in);
    return NULL;
  } else
    mush_free(c, "flag alias");

  /* Get the alias name */
  db_read_this_labeled_string(in, "alias", &tmp);
  strcpy(alias, tmp);
  return f;
}

/** Read flags and aliases from the database. This function expects
 * to receive file pointer that's already reading in a database file
 * and pointing at the start of the flag table. It reads the flags,
 * reads the aliases, and then does any additional flag adding that
 * needs to happen.
 * \param in file pointer to read from.
 * \param ns name of namespace to search.
 */
void
flag_read_all(PENNFILE *in, const char *ns)
{
  FLAG *f;
  FLAGSPACE *n;
  char alias[BUFFER_LEN];
  int count, found = 0;

  if (!(globals.indb_flags & DBF_LABELS)) {
    flag_read_all_oldstyle(in, ns);
    return;
  }

  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, "FLAG READ: Unable to locate flagspace %s.", ns);
    return;
  }
  /* If we are reading flags from the db, they are definitive. */
  clear_all_flags(n);
  db_read_this_labeled_int(in, "flagcount", &count);
  for (;;) {
    int c;

    c = penn_fgetc(in);
    penn_ungetc(c, in);

    if (c != ' ')
      break;

    found++;

    if ((f = flag_read(in)))
      flag_add(n, f->name, f);
  }

  if (found != count)
    do_rawlog(LT_ERR,
              "WARNING: Actual number of flags (%d) different than expected count (%d).",
              found, count);

  /* Assumes we'll always have at least one alias */
  db_read_this_labeled_int(in, "flagaliascount", &count);
  for (found = 0;;) {
    int c;

    c = penn_fgetc(in);
    penn_ungetc(c, in);

    if (c != ' ')
      break;

    found++;

    if ((f = flag_alias_read(in, alias, n)))
      flag_add(n, alias, f);
  }
  if (found != count)
    do_rawlog(LT_ERR,
              "WARNING: Actual number of flag aliases (%d) different than expected count (%d).",
              found, count);

  flag_add_additional(n);
}


/* Write a flag out to a file */
static void
flag_write(PENNFILE *out, FLAG *f, const char *name)
{
  db_write_labeled_string(out, " name", name);
  db_write_labeled_string(out, "  letter", tprintf("%c", f->letter));
  db_write_labeled_string(out, "  type", privs_to_string(type_privs, f->type));
  db_write_labeled_string(out, "  perms",
                          privs_to_string(flag_privs, F_REF_NOT & f->perms));
  db_write_labeled_string(out, "  negate_perms",
                          privs_to_string(flag_privs, f->negate_perms));
}


/* Write a flag alias out to a file */
static void
flag_alias_write(PENNFILE *out, FLAG *f, const char *name)
{
  db_write_labeled_string(out, " name", f->name);
  db_write_labeled_string(out, "  alias", name);
}

/** Write flags and aliases to the database. This function expects
 * to receive file pointer that's already writing in a database file.
 * It writes the flags, writes the aliases.
 * \param out file pointer to write to.
 * \param ns the namespace (FLAG/POWER) to write
 */
void
flag_write_all(PENNFILE *out, const char *ns)
{
  int i, count;
  FLAG *f;
  FLAGSPACE *n;
  const char *flagname;

  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, "FLAG WRITE: Unable to locate flagspace %s.", ns);
    return;
  }
  /* Write out canonical flags first */
  count = 0;
  for (i = 0; i < n->flagbits; i++) {
    if (n->flags[i])
      count++;
  }
  db_write_labeled_int(out, "flagcount", count);
  for (i = 0; i < n->flagbits; i++) {
    if (n->flags[i])
      flag_write(out, n->flags[i], n->flags[i]->name);
  }
  /* Now write out aliases. An alias is a flag in the ptab whose
   * name isn't the same as the name of the canonical flag in its
   * bit position
   */
  count = 0;
  f = ptab_firstentry_new(n->tab, &flagname);
  while (f) {
    if (strcmp(n->flags[f->bitpos]->name, flagname))
      count++;
    f = ptab_nextentry_new(n->tab, &flagname);
  }
  db_write_labeled_int(out, "flagaliascount", count);
  f = ptab_firstentry_new(n->tab, &flagname);
  while (f) {
    if (strcmp(n->flags[f->bitpos]->name, flagname)) {
      /* This is an alias! */
      flag_alias_write(out, f, flagname);
    }
    f = ptab_nextentry_new(n->tab, &flagname);
  }
}

/** Initialize the flagspaces.
 */
void
init_flagspaces(void)
{
  FLAGSPACE *flags;

  hashinit(&htab_flagspaces, 4);
  flags = mush_malloc(sizeof(FLAGSPACE), "flagspace");
  flags->name = strdup("FLAG");
  flags->tab = &ptab_flag;
  ptab_init(&ptab_flag);
  flags->flagbits = 0;
  flags->flags = NULL;
  flags->flag_table = flag_table;
  flags->flag_alias_table = flag_alias_tab;
  flags->cache = new_flagcache(flags, (sizeof flag_table / sizeof(FLAG)) * 4);
  hashadd("FLAG", (void *) flags, &htab_flagspaces);
  flags = mush_malloc(sizeof(FLAGSPACE), "flagspace");
  flags->name = strdup("POWER");
  flags->tab = &ptab_power;
  ptab_init(&ptab_power);
  flags->flagbits = 0;
  flags->flags = NULL;
  flags->flag_table = power_table;
  flags->flag_alias_table = power_alias_tab;
  flags->cache = new_flagcache(flags, (sizeof power_table / sizeof(FLAG)) * 2);
  hashadd("POWER", (void *) flags, &htab_flagspaces);
}


/** Initialize a flag table with defaults.
 * This function loads the standard flags as a baseline
 * (and for dbs that haven't yet converted).
 * \param ns name of flagspace to initialize.
 */
void
init_flag_table(const char *ns)
{
  FLAG *f, *cf;
  FLAG_ALIAS *a;
  FLAGSPACE *n;

  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, "FLAG INIT: Unable to locate flagspace %s.", ns);
    return;
  }

  ptab_start_inserts(n->tab);
  /* do regular flags first */
  for (f = n->flag_table; f->name; f++) {
    cf = clone_flag(f);
    cf->bitpos = -1;
    flag_add(n, cf->name, cf);
  }
  ptab_end_inserts(n->tab);
  /* now add in the aliases */
  for (a = n->flag_alias_table; a->alias; a++) {
    if ((f = match_flag_ns(n, a->realname)))
      flag_add(n, a->alias, f);
    else
      do_rawlog(LT_ERR,
                "FLAG INIT: flag alias %s matches no known flag.", a->alias);
  }
  flag_add_additional(n);
}

/* This is where the developers will put flag_add statements to create
 * new flags in future penn versions. Hackers should avoid this,
 * and use local_flags() in flaglocal.c instead.
 */
static void
flag_add_additional(FLAGSPACE *n)
{
  FLAG *f;
  FLAGSPACE *flags;

  if (n->tab == &ptab_flag) {
    add_flag("KEEPALIVE", 'k', TYPE_PLAYER, F_ANY, F_ANY);
    add_flag("MISTRUST", 'm', TYPE_THING | TYPE_EXIT | TYPE_ROOM, F_INHERIT,
             F_INHERIT);
    add_flag("ORPHAN", 'i', NOTYPE, F_ANY, F_ANY);
    add_flag("HEAVY", '\0', NOTYPE, F_ROYAL, F_ANY);
    add_flag("TRACK_MONEY", '\0', TYPE_PLAYER, F_ANY, F_ANY);
    add_flag("LOUD", '\0', NOTYPE, F_ROYAL, F_ANY);
    add_flag("HEAR_CONNECT", '\0', TYPE_PLAYER, F_ROYAL, F_ANY);
    add_flag("NO_LOG", '\0', NOTYPE, F_WIZARD | F_MDARK | F_LOG,
             F_WIZARD | F_MDARK);
    add_flag("OPEN_OK", '\0', TYPE_ROOM, F_ANY, F_ANY);
    if ((f = match_flag("LISTEN_PARENT")))
      f->type |= TYPE_PLAYER;
    if ((f = match_flag("TERSE")))
      f->type |= TYPE_THING;
    if ((f = match_flag("PUPPET")))
      f->type |= TYPE_ROOM;
    if ((f = match_flag("SUSPECT")))
      f->type = NOTYPE;
    if ((f = match_flag("CHOWN_OK")))
      f->type = TYPE_THING | TYPE_ROOM | TYPE_EXIT;
    if ((f = match_flag("NOSPOOF"))) {
      f->type = NOTYPE;
      f->letter = '"';
    }
    if ((f = match_flag("PARANOID"))) {
      f->type = NOTYPE;
      f->letter = '\0';
    }
    f = add_flag("CHAN_USEFIRSTMATCH", '\0', NOTYPE, F_INHERIT, F_INHERIT);
    flags = hashfind("FLAG", &htab_flagspaces);
    if (!match_flag("CHAN_FIRSTMATCH"))
      flag_add(flags, "CHAN_FIRSTMATCH", f);
    if (!match_flag("CHAN_MATCHFIRST"))
      flag_add(flags, "CHAN_MATCHFIRST", f);
    if ((f = match_flag("SUSPECT")))
      f->perms |= F_LOG;
    if ((f = match_flag("WIZARD")))
      f->perms |= F_LOG;
    if ((f = match_flag("ROYALTY")))
      f->perms |= F_LOG;

  } else if (n->tab == &ptab_power) {
    if (!(globals.indb_flags & DBF_POWERS_LOGGED)) {
      int i;
      for (i = 0; i < n->flagbits; i++)
        n->flags[i]->perms |= F_LOG;
    }
    flags = hashfind("POWER", &htab_flagspaces);
    f = add_power("Sql_Ok", '\0', NOTYPE, F_WIZARD | F_LOG, F_ANY);
    if (!match_power("Use_SQL"))
      flag_add(flags, "Use_SQL", f);
    if ((f = match_power("Can_nspemit")) && !match_power("Can_spoof")) {
      /* The "Can_nspemit" power was renamed "Can_spoof"... */
      mush_free((void *) f->name, "flag.name");
      f->name = mush_strdup("Can_spoof", "flag.name");
      flag_add(flags, "Can_spoof", f);
    } else if ((f = match_power("Can_spoof")) && !match_power("Can_nspemit")) {
      /* ... but make sure "Can_nspemit" remains as an alias */
      flag_add(flags, "Can_nspemit", f);
    }
    add_power("Debit", '\0', NOTYPE, F_WIZARD | F_LOG, F_ANY);
    add_power("Pueblo_Send", '\0', NOTYPE, F_WIZARD | F_LOG, F_ANY);
    add_power("Many_Attribs", '\0', NOTYPE, F_WIZARD | F_LOG, F_ANY);
    add_power("hook", '\0', NOTYPE, F_WIZARD | F_LOG, F_ANY);
    add_power("Can_dark", '\0', TYPE_PLAYER, F_WIZARD | F_LOG, F_ANY);
    /* Aliases for other servers */
    if ((f = match_power("tport_anything")) && !match_power("tel_anything"))
      flag_add(flags, "tel_anything", f);
    if ((f = match_power("tport_anywhere")) && !match_power("tel_anywhere"))
      flag_add(flags, "tel_anywhere", f);
    if ((f = match_power("no_money")) && !match_power("free_money"))
      flag_add(flags, "free_money", f);
    if ((f = match_power("no_quota")) && !match_power("free_quota"))
      flag_add(flags, "free_quota", f);
    if ((f = match_power("debit")) && !match_power("steal_money"))
      flag_add(flags, "steal_money", f);
  }

  local_flags(n);
}

/** Extract object type from old-style flag value.
 * Before 1.7.7p5, object types were stored in the lowest 3 bits of the
 * flag value. Now they get their own place in the object structure,
 * but if we're reading an older database, we need to extract the types
 * from the old flag value.
 * \param old_flags an old-style flag bitmask.
 * \return a type bitflag.
 */
int
type_from_old_flags(long old_flags)
{
  switch (old_flags & OLD_TYPE_MASK) {
  case OLD_TYPE_PLAYER:
    return TYPE_PLAYER;
  case OLD_TYPE_ROOM:
    return TYPE_ROOM;
  case OLD_TYPE_EXIT:
    return TYPE_EXIT;
  case OLD_TYPE_THING:
    return TYPE_THING;
  case OLD_TYPE_GARBAGE:
    return TYPE_GARBAGE;
  }
  /* If we get here, we're in trouble. */
  return -1;
}

/** Extract flags from old-style flag and toggle values.
 * This function takes the flag and toggle bitfields from older databases,
 * allocates a new flag bitmask, and populates it appropriately
 * by looking up each flag/toggle value in the old flag table.
 * It also works for powers (in which case old_toggles should be 0).
 * \param ns flagspace in which to locate values.
 * \param old_flags an old-style flag bitmask.
 * \param old_toggles an old-style toggle bitmask.
 * \param type the object type.
 * \return a newly allocated flag bitmask representing the flags and toggles.
 */
object_flag_type
flags_from_old_flags(const char *ns, long old_flags, long old_toggles, int type)
{
  FLAG *f, *newf;
  FLAGSPACE *n;
  object_flag_type bitmask;

  Flagspace_Lookup(n, ns);
  bitmask = new_flag_bitmask_ns(n);
  for (f = n->flag_table; f->name; f++) {
    if (f->type == NOTYPE) {
      if (f->bitpos & old_flags) {
        newf = match_flag_ns(n, f->name);
        bitmask = set_flag_bitmask_ns(n, bitmask, newf->bitpos);
      }
    } else if (f->type & type) {
      if (f->bitpos & old_toggles) {
        newf = match_flag_ns(n, f->name);
        bitmask = set_flag_bitmask_ns(n, bitmask, newf->bitpos);
      }
    }
  }
  for (f = hack_table; f->name; f++) {
    if ((f->type & type) && (f->bitpos & old_toggles)) {
      newf = match_flag_ns(n, f->name);
      bitmask = set_flag_bitmask_ns(n, bitmask, newf->bitpos);
    }
  }
  return bitmask;
}

/** Macro to detrmine if flag f's name is n */
#define is_flag(f,n)    (!strcmp(f->name,n))

/* Given a single character, return the matching flag definition */
static FLAG *
letter_to_flagptr(FLAGSPACE *n, char c, int type)
{
  FLAG *f;
  int i;
  for (i = 0; i < n->flagbits; i++)
    if ((f = n->flags[i])) {
      if ((n->tab == &ptab_flag) && ((f->letter == c) && (f->type & type)))
        return f;
    }
  /* Do we need to do this? */
  return NULL;
}


/*----------------------------------------------------------------------
 * Functions for managing bitmasks. All flagsets in a given space are
 * cached; objects with the same flags set share the same memory. Thus,
 * they are read-only. Setting or clearing a flag results in a new
 * bitmask, and a dereference of the old one. Copying a flag set in a
 * @clone is just a matter of incrementing the refcount.
 */

static struct flagcache *
new_flagcache(FLAGSPACE *n, int initial_size)
{
  struct flagcache *cache;

  cache = mush_malloc(sizeof *cache, "flagset.cache");

  initial_size = next_prime_after(initial_size);
  cache->size = initial_size;
  cache->entries = 0;
  cache->zero_refcount = 0;

  cache->flagset_slab = slab_create("flagset", FlagBytes(n));
  cache->zero = slab_malloc(cache->flagset_slab, NULL);
  memset(cache->zero, 0, FlagBytes(n));
  cache->buckets =
    mush_calloc(initial_size, sizeof(struct flagbucket *),
                "flagset.cache.bucketarray");
  return cache;
}

static void
free_flagcache(struct flagcache *cache)
{
  int i;
  for (i = 0; i < cache->size; i += 1) {
    struct flagbucket *b, *n;
    for (b = cache->buckets[i]; b; b = n) {
      n = b->next;
      slab_free(flagbucket_slab, b);
    }
  }

  slab_destroy(cache->flagset_slab);
  mush_free(cache->buckets, "flagset.cache.bucketarray");
  mush_free(cache, "flagset.cache");
}

static uint32_t
fc_hash(const FLAGSPACE *n, const object_flag_type f)
{
  uint32_t h = 0, i, len;

  for (i = 0, len = FlagBytes(n); i < len; i += 1)
    h = (h << 5) + h + f[i];

  return h;
}

static inline bool
fc_eq(const FLAGSPACE *n, const object_flag_type f1, const object_flag_type f2)
{
  return memcmp(f1, f2, FlagBytes(n)) == 0;
}

/** Returns a pointer to the cached copy of this flag bitset. If the flagset isn't already in the cache, inserts it. */
static object_flag_type
flagcache_find_ns(FLAGSPACE *n, const object_flag_type f)
{
  uint32_t h;
  struct flagbucket *b;

  if (flagbucket_slab == NULL)
    flagbucket_slab = slab_create("flagcache entries", sizeof *b);

  h = fc_hash(n, f);

  if (h == 0) {
    n->cache->zero_refcount += 1;
    return n->cache->zero;
  }

  h %= n->cache->size;

  for (b = n->cache->buckets[h]; b; b = b->next) {
    if (fc_eq(n, f, b->key)) {
      b->refcount += 1;
      return b->key;
    }
  }

  /* Add new entry */
  b = slab_malloc(flagbucket_slab, n->cache->buckets[h]);
  b->refcount = 1;
  b->key = f;
  b->next = n->cache->buckets[h];
  n->cache->entries += 1;
  n->cache->buckets[h] = b;
  return f;
}

static object_flag_type
flagcache_find(const char *ns, const object_flag_type f)
{
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  return flagcache_find_ns(n, f);
}

static void
flagcache_delete(FLAGSPACE *n, const object_flag_type f)
{
  uint32_t h;
  struct flagbucket *b, *p;

  h = fc_hash(n, f);

  if (h == 0) {
    n->cache->zero_refcount -= 1;
    return;
  }

  h %= n->cache->size;

  for (b = n->cache->buckets[h], p = NULL; b; p = b, b = b->next) {
    if (fc_eq(n, f, b->key)) {
      b->refcount -= 1;
      if (b->refcount == 0) {
        /* Free the flagset */
        if (!p) {
          /* First entry in chain */
          n->cache->buckets[h] = b->next;
          goto cleanup;         /* Not evil */
        } else {
          p->next = b->next;
          goto cleanup;
        }
      } else
        break;
    }
  }
  return;
cleanup:
  n->cache->entries -= 1;
  slab_free(n->cache->flagset_slab, b->key);
  slab_free(flagbucket_slab, b);
}

void
flag_stats(dbref player)
{
  FLAGSPACE *n;

  for (n = hash_firstentry(&htab_flagspaces); n;
       n = hash_nextentry(&htab_flagspaces)) {
    int maxref = 0, i, uniques = 0, maxlen = 0;

    notify_format(player, T("Stats for flagspace %s:"), n->name);
    notify_format(player,
                  T("  %d entries in flag table. Flagsets are %d bytes long."),
                  n->flagbits, (int) FlagBytes(n));
    notify_format(player,
                  T
                  ("  %d different cached flagsets. %d objects with no flags set."),
                  n->cache->entries, n->cache->zero_refcount);
    notify(player, T(" Stats for flagset slab:"));
    slab_describe(player, n->cache->flagset_slab);
    for (i = 0; i < n->cache->size; i += 1) {
      struct flagbucket *b;
      int len = 0;
      for (b = n->cache->buckets[i]; b; b = b->next) {
        if (b->refcount > maxref)
          maxref = b->refcount;
        if (b->refcount == 1)
          uniques += 1;
        len += 1;
      }
      if (len > maxlen)
        maxlen = len;
    }
    notify_format(player,
                  T
                  ("  %d objects share the most common set of flags.\n  %d objects have unique flagsets."),
                  maxref, uniques);
    notify_format(player,
                  T
                  ("  Cache hashtable has %d buckets. Longest collision chain is %d elements."),
                  n->cache->size, maxlen);
  }
}

/* Returns a newly allocated, unmanaged copy of the given flagset */
static object_flag_type
copy_flag_bitmask(FLAGSPACE *n, const object_flag_type orig)
{
  object_flag_type copy;
  int len;

  len = FlagBytes(n);
  copy = slab_malloc(n->cache->flagset_slab, NULL);
  memcpy(copy, orig, len);

  return copy;
}

/** Return a zeroed out, managed flagset
 * \param n the flagspace to use.
 * \return a managed flagset with all bits set to zero.
 */
object_flag_type
new_flag_bitmask_ns(FLAGSPACE *n)
{
  n->cache->zero_refcount += 1;
  return n->cache->zero;
}

/** Return a zeroed out, managed flagset
 * \param ns the name of the flagspace to use.
 * \return a managed flagset with all bits set to zero.
 */
object_flag_type
new_flag_bitmask(const char *ns)
{
  FLAGSPACE *n;

  Flagspace_Lookup(n, ns);
  return new_flag_bitmask_ns(n);
}

/** Copy a managed flag bitmask.
 * \param ns name of flagspace to use.
 * \param given a flag bitmask.
 * \return a managed clone of the given bitmask.
 */
object_flag_type
clone_flag_bitmask(const char *ns, const object_flag_type given)
{
  return flagcache_find(ns, given);
}

/** Dereference a managed flagset and possibly deallocate it.
 * \param ns the flagspace the flagset is in.
 * \param bitmask the flagset
 */
void
destroy_flag_bitmask(const char *ns, const object_flag_type bitmask)
{
  FLAGSPACE *n;

  Flagspace_Lookup(n, ns);
  flagcache_delete(n, bitmask);
}

/** Add a flag into a flagset.
 * This function sets a particular bit in a bitmask (e.g. bit 42), by
 * computing the appropriate byte, and the appropriate bit within the
 * byte, and setting it. It's used when replacing the current flagset
 * on an obect, so it decrements the original's reference count and
 * deletes it if needed.
 *
 * \param n the flagspace the flagset is in.
 * \param bitmask a managed flagset.
 * \param bit the bit to set.
 * \return A managed flagset with the new flag set.
 */
object_flag_type
set_flag_bitmask_ns(FLAGSPACE *n, const object_flag_type bitmask, int bit)
{
  int bytepos, bitpos;
  object_flag_type copy, managed_copy;

  if (!bitmask)
    return NULL;

  bytepos = FlagByte(bit);
  bitpos = FlagBit(bit);
  copy = copy_flag_bitmask(n, bitmask);
  *(copy + bytepos) |= (1 << bitpos);
  managed_copy = flagcache_find_ns(n, copy);
  if (managed_copy != copy)
    slab_free(n->cache->flagset_slab, copy);
  flagcache_delete(n, bitmask);
  return managed_copy;
}

/** Add a flag into a flagset.
 * This function sets a particular bit in a bitmask (e.g. bit 42), by
 * computing the appropriate byte, and the appropriate bit within the
 * byte, and setting it. It's used when replacing the current flagset
 * on an obect, so it decrements the original's reference count and
 * deletes it if needed.
 *
 * \param ns the name of the flagspace the flagset is in.
 * \param bitmask a managed flagset.
 * \param bit the bit to set.
 * \return A managed flagset with the new flag set.
 */
object_flag_type
set_flag_bitmask(const char *ns, const object_flag_type bitmask, int bit)
{
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  return set_flag_bitmask_ns(n, bitmask, bit);
}

/** Remove a flag from a flagset.
 * This function clears a particular bit in a bitmask (e.g. bit 42),
 * by computing the appropriate byte, and the appropriate bit within
 * the byte, and clearing it. It's used hen replacing the current
 * flagset on an object, so it decrements the original's reference
 * count and deletes it if needed.
 *
 * \param n the flagspace the flagset is in.
 * \param bitmask a managed flagset.
 * \param bit the bit to clear.
 * \return A managed flagset with the flag cleared
 */
object_flag_type
clear_flag_bitmask_ns(FLAGSPACE *n, const object_flag_type bitmask, int bit)
{
  int bytepos, bitpos;
  object_flag_type copy, managed_copy;

  if (!bitmask)
    return NULL;

  bytepos = FlagByte(bit);
  bitpos = FlagBit(bit);

  copy = copy_flag_bitmask(n, bitmask);
  *(copy + bytepos) &= ~(1 << bitpos);
  managed_copy = flagcache_find_ns(n, copy);
  if (managed_copy != copy)
    slab_free(n->cache->flagset_slab, copy);
  flagcache_delete(n, bitmask);
  return managed_copy;
}

/** Remove a flag from a flagset.
 * This function clears a particular bit in a bitmask (e.g. bit 42),
 * by computing the appropriate byte, and the appropriate bit within
 * the byte, and clearing it. It's used hen replacing the current
 * flagset on an object, so it decrements the original's reference
 * count and deletes it if needed.
 *
 * \param ns the name of the flagspace the flagset is in.
 * \param bitmask a managed flagset.
 * \param bit the bit to clear.
 * \return A managed flagset with the flag cleared
 */
object_flag_type
clear_flag_bitmask(const char *ns, const object_flag_type bitmask, int bit)
{
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  return clear_flag_bitmask_ns(n, bitmask, bit);
}


/** Test a bit in a bitmask.
 * This function tests a particular bit in a bitmask (e.g. bit 42),
 * by computing the appropriate byte, and the appropriate bit within the byte,
 * and testing it.
 * \param bitmask a flagset.
 * \param bitpos the bit to test.
 * \retval 1 bit is set.
 * \retval 0 bit is not set.
 */
bool
has_bit(const object_flag_type flags, int bitpos)
{
  int bytepos, bits_in_byte;
  /* Garbage objects, for example, have no bits set */
  if (!flags)
    return 0;
  bytepos = FlagByte(bitpos);
  bits_in_byte = FlagBit(bitpos);
  return *(flags + bytepos) & (1 << bits_in_byte);
}

/** Test a set of bits in one bitmask against all those in another.
 * This function determines if one bitmask contains (at least)
 * all of the bits set in another bitmask.
 * \param ns name of namespace to search.
 * \param source the flagset to test.
 * \param bitmask the flagset containing the bits to look for.
 * \retval 1 all bits in bitmask are set in source.
 * \retval 0 at least one bit in bitmask is not set in source.
 */
bool
has_all_bits(const char *ns, const object_flag_type source,
             const object_flag_type bitmask)
{
  unsigned int i;
  int ok = 1;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  for (i = 0; i < FlagBytes(n); i++)
    ok &= ((*(bitmask + i) & *(source + i)) == *(bitmask + i));
  return ok;
}

/** Test to see if a bitmask is entirely 0 bits.
 * \param ns name of namespace to search.
 * \param source the bitmask to test.
 * \retval 1 all bits in bitmask are 0.
 * \retval 0 at least one bit in bitmask is 1.
 */
bool
null_flagmask(const char *ns, const object_flag_type source)
{
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  return n->cache->zero == source;
}

/** Test a set of bits in one bitmask against any of those in another.
 * This function determines if one bitmask contains any
 * of the bits set in another bitmask.
 * \param ns name of namespace to search.
 * \param source the bitmask to test.
 * \param bitmask the bitmask containing the bits to look for.
 * \retval 1 at least one bit in bitmask is set in source.
 * \retval 0 no bits in bitmask are set in source.
 */
bool
has_any_bits(const char *ns, const object_flag_type source,
             const object_flag_type bitmask)
{
  unsigned int i;
  int ok = 0;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  for (i = 0; i < FlagBytes(n); i++)
    ok |= (*(bitmask + i) & *(source + i));
  return ok;
}

/** Produce a space-separated list of flag names, given a bitmask.
 * This function returns the string representation of a flag bitmask.
 * \param ns name of namespace to search.
 * \param bitmask a flag bitmask.
 * \param privs dbref for privilege checking for flag visibility.
 * \param thing object for which bitmask is the flag bitmask.
 * \return string representation of bitmask (list of flags).
 */
const char *
bits_to_string(const char *ns, object_flag_type bitmask, dbref privs,
               dbref thing)
{
  FLAG *f;
  FLAGSPACE *n;
  int i;
  int first = 1;
  static char buf[BUFFER_LEN];
  char *bp;

  Flagspace_Lookup(n, ns);
  bp = buf;
  for (i = 0; i < n->flagbits; i++) {
    if ((f = n->flags[i])) {
      if (has_bit(bitmask, f->bitpos) &&
          (!GoodObject(thing) || Can_See_Flag(privs, thing, f))) {
        if (!first)
          safe_chr(' ', buf, &bp);
        safe_str(f->name, buf, &bp);
        first = 0;
      }
    }
  }
  *bp = '\0';
  return buf;
}

/** Convert a flag list string to a flagset.
 * Given a space-separated list of flag names, convert them to
 * a cached bitmask array and return it.
 * \param ns name of namespace to search.
 * \param str list of flag names.
 * \return a managed flagset.
 */
object_flag_type
string_to_bits(const char *ns, const char *str)
{
  object_flag_type bitmask;
  char *copy, *s, *sp;
  FLAG *f;
  FLAGSPACE *n;

  Flagspace_Lookup(n, ns);
  bitmask = new_flag_bitmask_ns(n);
  if (!str || !*str)
    return bitmask;             /* We're done, then */
  copy = mush_strdup(str, "flagstring");
  s = trim_space_sep(copy, ' ');
  while (s) {
    sp = split_token(&s, ' ');
    if (!(f = match_flag_ns(n, sp)))
      /* Now what do we do? Ignore it? */
      continue;
    bitmask = set_flag_bitmask_ns(n, bitmask, f->bitpos);
  }
  mush_free(copy, "flagstring");
  return bitmask;
}


/*----------------------------------------------------------------------
 * Functions for working with flags on objects
 */


/** Check an object for a flag.
 * This function tests to see if an object has a flag. It is the
 * function to use for this purpose from outside of this file.
 * \param ns name of flagspace to use.
 * \param thing object to check.
 * \param flag name of flag to check for (a string).
 * \param type allowed types of flags to check for.
 * \retval >0 object has the flag.
 * \retval 0 object does not have the flag.
 */
bool
has_flag_in_space_by_name(const char *ns, dbref thing, const char *flag,
                          int type)
{
  FLAG *f;
  FLAGSPACE *n;
  n = hashfind(ns, &htab_flagspaces);
  f = flag_hash_lookup(n, flag, type);
  if (!f)
    return 0;
  return has_flag_ns(n, thing, f);
}

static bool
has_flag_ns(FLAGSPACE *n, dbref thing, FLAG *f)
{
  if (!GoodObject(thing) || IsGarbage(thing))
    return 0;
  return (n->tab == &ptab_flag) ?
    has_bit(Flags(thing), f->bitpos) : has_bit(Powers(thing), f->bitpos);
}

static bool
can_set_flag_generic(dbref player, dbref thing, FLAG *flagp, int negate)
{
  int myperms;
  if (!flagp || !GoodObject(player) || !GoodObject(thing))
    return 0;
  myperms = negate ? flagp->negate_perms : flagp->perms;
  if ((myperms & F_INTERNAL) || (myperms & F_DISABLED))
    return 0;
  if (!(flagp->type & Typeof(thing)))
    return 0;
  if ((myperms & F_INHERIT) && !Wizard(player) &&
      (!Inheritable(player) || !Owns(player, thing)))
    return 0;
  if ((myperms & F_WIZARD) && !Wizard(player))
    return 0;
  else if ((myperms & F_ROYAL) && !Hasprivs(player))
    return 0;
  else if ((myperms & F_GOD) && !God(player))
    return 0;
  return 1;
}

static bool
can_set_power(dbref player, dbref thing, FLAG *flagp, int negate)
{
  if (!can_set_flag_generic(player, thing, flagp, negate))
    return 0;
  if (Hasprivs(thing) && (is_flag(flagp, "GUEST"))) {
    notify(player, T("You can't make admin into guests."));
    return 0;
  }
  return 1;

}


static bool
can_set_flag(dbref player, dbref thing, FLAG *flagp, int negate)
{
  if (!can_set_flag_generic(player, thing, flagp, negate))
    return 0;

  /* You've got to *own* something (or be Wizard) to set it
   * chown_ok or dest_ok. This prevents subversion of the
   * zone-restriction on @chown and @dest
   */
  if (is_flag(flagp, "CHOWN_OK") || is_flag(flagp, "DESTROY_OK")) {
    if (!Owns(player, thing) && !Wizard(player))
      return 0;
    else
      return 1;
  }

  /* Checking for the SHARED flag. If you set this, the player had
   * better be zone-locked!
   */
  if (!negate && is_flag(flagp, "SHARED") &&
      (getlock(thing, Zone_Lock) == TRUE_BOOLEXP)) {
    notify(player,
           T("You must @lock/zone before you can set a player SHARED."));
    return 0;
  }

  /* special case for the privileged flags. We do need to check
   * for royalty setting flags on objects they don't own, because
   * the controls check will not stop the flag set if the royalty
   * player is in a zone. Also, only God can set the wizbit on
   * players.
   */
  if (Wizard(thing) && is_flag(flagp, "GAGGED"))
    return 0;                   /* can't gag wizards/God */
  if (God(player))              /* God can do (almost) anything) */
    return 1;
  /* Make sure we don't accidentally permission-check toggles when
   * checking priv bits.
   */
  /* A wiz can set things he owns WIZ, but nothing else. */
  if (is_flag(flagp, "WIZARD") && !negate)
    return (Wizard(player) && Owns(player, thing) && !IsPlayer(thing));
  /* A wiz can unset the WIZ bit on any non-player */
  if (is_flag(flagp, "WIZARD") && negate)
    return (Wizard(player) && !IsPlayer(thing));
  /* Wizards can set or unset anything royalty. Royalty can set anything
   * they own royalty, but cannot reset their own bits. */
  if (is_flag(flagp, "ROYALTY")) {
    return (!Guest(thing) && (Wizard(player) || (Royalty(player) &&
                                                 Owns(player, thing)
                                                 && !IsPlayer(thing))));
  }
  return 1;
}

/** Return a list of flag symbols that one object can see on another.
 * \param thing object to list flag symbols for.
 * \param player looker, for permission checking.
 * \return a string containing all the visible type and flag symbols.
 */
const char *
unparse_flags(dbref thing, dbref player)
{
  /* print out the flag symbols (letters) */
  static char buf[BUFFER_LEN];
  char *p;
  FLAG *f;
  int i;
  FLAGSPACE *n;

  Flagspace_Lookup(n, "FLAG");
  p = buf;
  switch (Typeof(thing)) {
  case TYPE_GARBAGE:
    *p = '\0';
    return buf;
  case TYPE_ROOM:
    *p++ = 'R';
    break;
  case TYPE_EXIT:
    *p++ = 'E';
    break;
  case TYPE_THING:
    *p++ = 'T';
    break;
  case TYPE_PLAYER:
    *p++ = 'P';
    break;
  }
  for (i = 0; i < n->flagbits; i++) {
    if ((f = n->flags[i])) {
      if (has_flag_ns(n, thing, f) && Can_See_Flag(player, thing, f)
          && f->letter)
        *p++ = f->letter;
    }
  }
  *p = '\0';
  return buf;
}

/** Return the object's type and its flag list for examine.
 * \param thing object to list flag symbols for.
 * \param player looker, for permission checking.
 * \return a string containing all the visible type and flag symbols.
 */
const char *
flag_description(dbref player, dbref thing)
{
  static char buf[BUFFER_LEN];
  char *bp;
  bp = buf;
  safe_str(T("Type: "), buf, &bp);
  safe_str(privs_to_string(type_privs, Typeof(thing)), buf, &bp);
  safe_str(T(" Flags: "), buf, &bp);
  safe_str(bits_to_string("FLAG", Flags(thing), player, thing), buf, &bp);
  *bp = '\0';
  return buf;
}



/** Print out the flags for a decompile.
 * \param player looker, for permission checking.
 * \param thing object being decompiled.
 * \param name name by which object is referred to in the decompile.
 * \param ns name of namespace to search.
 * \param command name of command used to set the 'flag'
 * \param prefix string to prefix each line of output with
 */
void
decompile_flags_generic(dbref player, dbref thing, const char *name,
                        const char *ns, const char *command, const char *prefix)
{
  FLAG *f;
  int i;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  for (i = 0; i < n->flagbits; i++)
    if ((f = n->flags[i])) {
      if (has_flag_ns(n, thing, f) && Can_See_Flag(player, thing, f)
          && !(f->perms & F_INTERNAL))
        notify_format(player, "%s%s %s = %s", prefix, command, name, f->name);
    }
}


/** Set or clear flags on an object, without permissions/hear checking.
 * This function is for server internal use, only, when a flag should
 * be set or cleared unequivocally.
 * \param ns name of namespace to search.
 * \param thing object on which to set or clear flag.
 * \param flag name of flag to set or clear.
 * \param negate if 1, clear the flag, if 0, set the flag.
 */
void
twiddle_flag_internal(const char *ns, dbref thing, const char *flag, int negate)
{
  FLAG *f;
  FLAGSPACE *n;

  if (IsGarbage(thing))
    return;
  n = hashfind(ns, &htab_flagspaces);
  if (!n)
    return;
  f = flag_hash_lookup(n, flag, Typeof(thing));
  if (f && (n->flag_table != type_table)) {
    if (n->tab == &ptab_flag) {
      Flags(thing) = negate ? clear_flag_bitmask_ns(n, Flags(thing), f->bitpos)
        : set_flag_bitmask_ns(n, Flags(thing), f->bitpos);
    } else {
      Powers(thing) =
        negate ? clear_flag_bitmask_ns(n, Powers(thing), f->bitpos)
        : set_flag_bitmask_ns(n, Powers(thing), f->bitpos);
    }
  }
}


/** Set or clear flags on an object, with full permissions/hear checking.
 * \verbatim
 * This function is used to set and clear flags through @set and the like.
 * It does permission checking and handles the "is now listening" messages.
 * \endverbatim
 * \param player the enactor.
 * \param thing object on which to set or clear flag.
 * \param flag name of flag to set or clear.
 * \param negate if 1, clear the flag, if 0, set the flag.
 * \param hear 1 if object is a hearer.
 * \param listener 1 if object is a listener.
 */
void
set_flag(dbref player, dbref thing, const char *flag, int negate,
         int hear, int listener)
{
  FLAG *f;
  char tbuf1[BUFFER_LEN];
  char *tp;
  FLAGSPACE *n;
  int current;
  dbref safe_orator = orator;

  n = hashfind("FLAG", &htab_flagspaces);
  if (!n) {
    notify_format(player, T("Internal error: Unable to find flagspace '%s'!"),
                  "FLAG");
    return;
  }
  if ((f = flag_hash_lookup(n, flag, Typeof(thing))) == NULL) {
    notify_format(player, T("%s - I don't recognize that flag."), flag);
    return;
  }

  if (!can_set_flag(player, thing, f, negate)) {
    notify(player, T("Permission denied."));
    return;
  }
  /* The only players who can be Dark are wizards. */
  if (is_flag(f, "DARK") && !negate && Alive(thing) && !Wizard(thing)
      && !has_power_by_name(thing, "Can_dark", NOTYPE)) {
    notify(player, T("Permission denied."));
    return;
  }

  current = sees_flag("FLAG", player, thing, f->name);

  if (negate)
    Flags(thing) = clear_flag_bitmask_ns(n, Flags(thing), f->bitpos);
  else
    Flags(thing) = set_flag_bitmask_ns(n, Flags(thing), f->bitpos);

  orator = thing;

  if (negate) {
    /* log if necessary */
    if (f->perms & F_LOG)
      do_log(LT_WIZ, player, thing, "%s FLAG CLEARED", f->name);
    if (f->perms & F_EVENT)
      queue_event(player, "OBJECT`FLAG", "%s,%s,%s,%d,%s", unparse_objid(thing),
                  f->name, "FLAG", 0, "CLEARED");
    /* notify the area if something stops listening, but only if it
       was listening before */
    if (!IsPlayer(thing) && (hear || listener) &&
        !Hearer(thing) && !Listener(thing)) {
      tp = tbuf1;
      safe_format(tbuf1, &tp, T("%s is no longer listening."), Name(thing));
      *tp = '\0';
      if (GoodObject(Location(thing)))
        notify_except(Location(thing), NOTHING, tbuf1, NA_INTER_PRESENCE);
      notify_except(thing, NOTHING, tbuf1, 0);
    }
    if (is_flag(f, "AUDIBLE")) {
      switch (Typeof(thing)) {
      case TYPE_EXIT:
        if (Audible(Source(thing))) {
          tp = tbuf1;
          safe_format(tbuf1, &tp, T("Exit %s is no longer broadcasting."),
                      Name(thing));
          *tp = '\0';
          notify_except(Source(thing), NOTHING, tbuf1, 0);
        }
        break;
      case TYPE_ROOM:
        notify_except(thing, NOTHING,
                      T("Audible exits in this room have been deactivated."),
                      0);
        break;
      case TYPE_THING:
      case TYPE_PLAYER:
        notify_except(thing, thing,
                      T("This room is no longer broadcasting."), 0);
        notify(thing, T("Your contents can no longer be heard from outside."));
        break;
      }
    }
    if (is_flag(f, "QUIET") || (!AreQuiet(player, thing))) {
      tp = tbuf1;
      safe_str(Name(thing), tbuf1, &tp);
      safe_str(" - ", tbuf1, &tp);
      safe_str(f->name, tbuf1, &tp);
      if (!current)
        safe_str(T(" (already)"), tbuf1, &tp);
      safe_str(T(" reset."), tbuf1, &tp);
      *tp = '\0';
      notify(player, tbuf1);
    }
  } else {

    /* log if necessary */
    if (f->perms & F_LOG)
      do_log(LT_WIZ, player, thing, "%s FLAG SET", f->name);
    if (f->perms & F_EVENT)
      queue_event(player, "OBJECT`FLAG", "%s,%s,%s,%d,%s", unparse_objid(thing),
                  f->name, "FLAG", 1, "SET");
    if (is_flag(f, "TRUST") && GoodObject(Zone(thing)))
      notify(player, T("Warning: Setting trust flag on zoned object"));
    if (is_flag(f, "SHARED"))
      check_zone_lock(player, thing, 1);
    /* notify area if something starts listening */
    if (!IsPlayer(thing) &&
        (is_flag(f, "PUPPET") || is_flag(f, "MONITOR")) && !hear && !listener) {
      tp = tbuf1;
      safe_format(tbuf1, &tp, T("%s is now listening."), Name(thing));
      *tp = '\0';
      if (GoodObject(Location(thing)))
        notify_except(Location(thing), NOTHING, tbuf1, NA_INTER_PRESENCE);
      notify_except(thing, NOTHING, tbuf1, 0);
    }
    /* notify for audible exits */
    if (is_flag(f, "AUDIBLE")) {
      switch (Typeof(thing)) {
      case TYPE_EXIT:
        if (Audible(Source(thing))) {
          tp = tbuf1;
          safe_format(tbuf1, &tp, T("Exit %s is now broadcasting."),
                      Name(thing));
          *tp = '\0';
          notify_except(Source(thing), NOTHING, tbuf1, 0);
        }
        break;
      case TYPE_ROOM:
        notify_except(thing, NOTHING,
                      T("Audible exits in this room have been activated."), 0);
        break;
      case TYPE_PLAYER:
      case TYPE_THING:
        notify_except(thing, thing, T("This room is now broadcasting."), 0);
        notify(thing, T("Your contents can now be heard from outside."));
        break;
      }
    }
    if (is_flag(f, "QUIET") || (!AreQuiet(player, thing))) {
      tp = tbuf1;
      safe_str(Name(thing), tbuf1, &tp);
      safe_str(" - ", tbuf1, &tp);
      safe_str(f->name, tbuf1, &tp);
      if (current)
        safe_str(T(" (already)"), tbuf1, &tp);
      safe_str(T(" set."), tbuf1, &tp);
      *tp = '\0';
      notify(player, tbuf1);
    }
  }

  orator = safe_orator;
}


/** Set or clear powers on an object, with full permissions checking.
 * \verbatim
 * This function is used to set and clear powers through @power and the like.
 * It does permission checking.
 * \endverbatim
 * \param player the enactor.
 * \param thing object on which to set or clear flag.
 * \param flag name of flag to set or clear.
 * \param negate if 1, clear the flag, if 0, set the flag.
 */
void
set_power(dbref player, dbref thing, const char *flag, int negate)
{
  FLAG *f;
  FLAGSPACE *n;
  char tbuf1[BUFFER_LEN], *tp;
  int current;

  n = hashfind("POWER", &htab_flagspaces);
  if (!n) {
    notify_format(player, T("Internal error: Unable to find flagspace '%s'!"),
                  "POWER");
    return;
  }

  if ((f = flag_hash_lookup(n, flag, Typeof(thing))) == NULL) {
    notify_format(player, T("%s - I don't recognize that power."), flag);
    return;
  }

  if (!can_set_power(player, thing, f, negate)) {
    notify(player, T("Permission denied."));
    return;
  }

  current = sees_flag("POWER", player, thing, f->name);

  if (negate)
    Powers(thing) = clear_flag_bitmask_ns(n, Powers(thing), f->bitpos);
  else
    Powers(thing) = set_flag_bitmask_ns(n, Powers(thing), f->bitpos);

  if (!AreQuiet(player, thing)) {
    tp = tbuf1;
    if (negate) {
      if (current) {
        safe_format(tbuf1, &tp, T("%s - %s removed."), Name(thing), f->name);
      } else {
        safe_format(tbuf1, &tp, T("%s - %s (already) removed."), Name(thing),
                    f->name);
      }
    } else {
      if (current) {
        safe_format(tbuf1, &tp, T("%s - %s (already) granted."), Name(thing),
                    f->name);
      } else {
        safe_format(tbuf1, &tp, T("%s - %s granted."), Name(thing), f->name);
      }
    }
    *tp = '\0';
    notify(player, tbuf1);
  }

  if (f->perms & F_LOG)
    do_log(LT_WIZ, player, thing, "%s POWER %s", f->name,
           negate ? T("CLEARED") : T("SET"));
  if (f->perms & F_EVENT) {
    queue_event(player, "OBJECT`FLAG", "%s,%s,%s,%d,%s", unparse_objid(thing),
                f->name, "POWER", !negate, (negate ? "CLEARED" : "SET"));
  }
}

/** Check if an object has one or all of a list of flag characters.
 * This function is used by orflags and andflags to check to see
 * if an object has one or all of the flags signified by a list
 * of flag characters.
 * \param ns name of namespace to search.
 * \param player the object checking, for permissions
 * \param it the object on which to check for flags.
 * \param fstr string of flag characters to check for.
 * \param type 0=orflags, 1=andflags.
 * \retval 1 object has any (or all) flags.
 * \retval 0 object has no (or not all) flags.
 * \retval -1 invalid flag specified
 */
int
flaglist_check(const char *ns, dbref player, dbref it, const char *fstr,
               int type)
{
  char *s;
  FLAG *fp;
  int negate = 0, temp = 0;
  int ret = type;
  FLAGSPACE *n;

  if (!GoodObject(it))
    return 0;
  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, "FLAG: Unable to locate flagspace %s", ns);
    return 0;
  }
  for (s = (char *) fstr; *s; s++) {
    /* Check for a negation sign. If we find it, we note it and
     * increment the pointer to the next character.
     */
    if (*s == '!') {
      negate = 1;
      s++;
    } else {
      negate = 0;               /* It's important to clear this at appropriate times;
                                 * else !Dc means (!D && !c), instead of (!D && c). */
    }
    if (!*s)
      /* We got a '!' that wasn't followed by a letter.
       * Fail the check. */
      return -1;
    /* Find the flag. */
    fp = letter_to_flagptr(n, *s, Typeof(it));
    if (!fp) {
      if (n->tab == &ptab_flag) {
        /* Maybe *s is a type specifier (P, T, E, R). These aren't really
         * flags, but we grandfather them in to preserve old code
         */
        if ((*s == 'T') || (*s == 'R') || (*s == 'E') || (*s == 'P')) {
          temp = (*s == 'T') ? (Typeof(it) == TYPE_THING) :
            ((*s == 'R') ? (Typeof(it) == TYPE_ROOM) :
             ((*s == 'E') ? (Typeof(it) == TYPE_EXIT) :
              (Typeof(it) == TYPE_PLAYER)));
          if ((type == 1) && ((negate && temp) || (!negate && !temp)))
            return 0;
          else if ((type == 0) && ((!negate && temp) || (negate && !temp)))
            ret |= 1;
        } else {
          /* Either we got a '!' that wasn't followed by a letter, or
           * we couldn't find that flag. For AND, since we've failed
           * a check, we can return false. Otherwise we just go on.
           */
          return -1;
        }
      } else {
        if (type == 1)
          return 0;
        else
          continue;
      }
    } else {
      /* does the object have this flag? */
      temp = (has_flag_ns(n, it, fp) && Can_See_Flag(player, it, fp));
      if ((type == 1) && ((negate && temp) || (!negate && !temp))) {
        /* Too bad there's no NXOR function...
         * At this point we've either got a flag and we don't want
         * it, or we don't have a flag and we want it. Since it's
         * AND, we return false.
         */
        ret = 0;
      } else if ((type == 0) && ((!negate && temp) || (negate && !temp))) {
        /* We've found something we want, in an OR. We OR a
         * true with the current value.
         */
        ret |= 1;
      }
      /* Otherwise, we don't need to do anything. */
    }
  }
  return ret;
}

/** Check if an object has one or all of a list of flag names.
 * This function is used by orlflags and andlflags to check to see
 * if an object has one or all of the flags signified by a list
 * of flag names.
 * \param ns name of namespace to search.
 * \param player the object checking, for permissions
 * \param it the object on which to check for flags.
 * \param fstr string of flag names, space-separated, to check for.
 * \param type 0=orlflags, 1=andlflags.
 * \retval 1 object has any (or all) flags.
 * \retval 0 object has no (or not all) flags.
 * \retval -1 invalid flag specified
 */
int
flaglist_check_long(const char *ns, dbref player, dbref it, const char *fstr,
                    int type)
{
  char *s, *copy, *sp;
  FLAG *fp;
  int negate = 0, temp = 0;
  int ret = type;
  FLAGSPACE *n;

  if (!GoodObject(it))
    return 0;
  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, "FLAG: Unable to locate flagspace %s", ns);
    return 0;
  }
  copy = mush_strdup(fstr, "flaglistlong");
  sp = trim_space_sep(copy, ' ');
  while (sp) {
    s = split_token(&sp, ' ');
    /* Check for a negation sign. If we find it, we note it and
     * increment the pointer to the next character.
     */
    if (*s == '!') {
      negate = 1;
      s++;
    } else {
      negate = 0;               /* It's important to clear this at appropriate times;
                                 * else !D c means (!D && !c), instead of (!D && c). */
    }
    if (!*s) {
      /* We got a '!' that wasn't followed by a string.
       * Fail the check. */
      ret = -1;
      break;
    }
    /* Find the flag. */
    if (!(fp = flag_hash_lookup(n, s, Typeof(it)))) {
      /* Either we got a '!' that wasn't followed by a letter, or
       * we couldn't find that flag. For AND, since we've failed
       * a check, we can return false. Otherwise we just go on.
       */
      ret = -1;
      break;
    } else {
      /* does the object have this flag? There's a special case
       * here, as we want (for consistency with flaglist_check)
       * to allow types to match as well
       */
      int in_flags = (n->tab == &ptab_flag);
      if (in_flags && !strcmp(fp->name, "PLAYER"))
        temp = IsPlayer(it);
      else if (in_flags && !strcmp(fp->name, "THING"))
        temp = IsThing(it);
      else if (in_flags && !strcmp(fp->name, "ROOM"))
        temp = IsRoom(it);
      else if (in_flags && !strcmp(fp->name, "EXIT"))
        temp = IsExit(it);
      else
        temp = (has_flag_ns(n, it, fp) && Can_See_Flag(player, it, fp));
      if ((type == 1) && ((negate && temp) || (!negate && !temp))) {
        /* Too bad there's no NXOR function...
         * At this point we've either got a flag and we don't want
         * it, or we don't have a flag and we want it. Since it's
         * AND, we return false.
         */
        ret = 0;
      } else if ((type == 0) && ((!negate && temp) || (negate && !temp))) {
        /* We've found something we want, in an OR. We OR a
         * true with the current value.
         */
        ret |= 1;
      }
      /* Otherwise, we don't need to do anything. */
    }
  }
  mush_free(copy, "flaglistlong");
  return ret;
}


/** Can a player see a flag?
 * \param ns name of the flagspace to use.
 * \param privs looker.
 * \param thing object on which to look for flag.
 * \param name name of flag to look for.
 * \retval 1 object has the flag and looker can see it.
 * \retval 0 looker can not see flag on object.
 */
bool
sees_flag(const char *ns, dbref privs, dbref thing, const char *name)
{
  /* Does thing have the flag named name && can privs see it? */
  FLAG *f;
  FLAGSPACE *n;
  n = hashfind(ns, &htab_flagspaces);
  if ((f = flag_hash_lookup(n, name, Typeof(thing))) == NULL)
    return 0;
  return has_flag_ns(n, thing, f) && Can_See_Flag(privs, thing, f);
}


/** A hacker interface for adding a flag.
 * \verbatim
 * This function is used to add a new flag to the game. It's
 * called by @flag (via do_flag_add()), and is the right function to
 * call in flaglocal.c if you're writing a hardcoded system patch that
 * needs to add its own flags. It will not add the same flag twice.
 * \endverbatim
 * \param ns name of namespace to add flag to.
 * \param name flag name.
 * \param letter flag character (or ascii 0)
 * \param type mask of object types to which the flag applies.
 * \param perms mask of permissions to see/set the flag.
 * \param negate_perms mask of permissions to clear the flag.
 */
FLAG *
add_flag_generic(const char *ns, const char *name, const char letter, int type,
                 int perms, int negate_perms)
{
  FLAG *f;
  FLAGSPACE *n;
  Flagspace_Lookup(n, ns);
  /* Don't double-add */
  if ((f = match_flag_ns(n, strupper(name)))) {
    if (strcasecmp(f->name, name) == 0)
      return f;
  }
  f = new_flag();
  f->name = mush_strdup(strupper(name), "flag.name");
  f->letter = letter;
  f->type = type;
  f->perms = perms;
  f->negate_perms = negate_perms;
  f->bitpos = -1;
  flag_add(n, f->name, f);
  return f;
}


/*--------------------------------------------------------------------------
 * MUSHcode interface
 */

/** User interface to list flags.
 * \verbatim
 * This function implements @flag/list.
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param arg wildcard pattern of flag names to list, or NULL for all.
 * \param lc if 1, list flags in lowercase.
 * \param label label to prefix to list.
 */
void
do_list_flags(const char *ns, dbref player, const char *arg, int lc,
              const char *label)
{
  char *b = list_all_flags(ns, arg, player, 0x3);
  notify_format(player, "%s: %s", label, lc ? strlower(b) : b);
}

/** User interface to show flag detail.
 * \verbatim
 * This function implements @flag <flag>.
 * \endverbatim
 * \param ns name of namespace to search.
 * \param player the enactor.
 * \param name name of the flag to describe.
 */
void
do_flag_info(const char *ns, dbref player, const char *name)
{
  FLAG *f;
  FLAGSPACE *n;
  /* Find the flagspace */
  if (!(n = (FLAGSPACE *) hashfind(ns, &htab_flagspaces))) {
    do_rawlog(LT_ERR, "FLAG: Unable to locate flagspace %s", ns);
    return;
  }

  /* Find the flag */
  f = flag_hash_lookup(n, name, NOTYPE);
  if (!f && God(player))
    f = match_flag_ns(n, name);
  if (!f) {
    notify_format(player, T("No such %s."), strlower(ns));
    return;
  }
  notify_format(player, "%9s: %s", T("Name"), f->name);
  notify_format(player, "%9s: %c", T("Character"), f->letter);
  notify_format(player, "%9s: %s", T("Aliases"), list_aliases(n, f));
  notify_format(player, "%9s: %s", T("Type(s)"),
                privs_to_string(type_privs, f->type));
  notify_format(player, "%9s: %s", T("Perms"),
                privs_to_string(flag_privs, f->perms));
  notify_format(player, "%9s: %s", T("ResetPrms"),
                privs_to_string(flag_privs, f->negate_perms));
}

/** Change the permissions on a flag.
 * \verbatim
 * This is the user-interface to @flag/restrict, which uses this syntax:
 *
 * @flag/restrict <flag> = <perms>, <negate_perms>
 *
 * If no comma is given, use <perms> for both.
 * \endverbatim
 * \param ns name of namespace to search.
 * \param player the enactor.
 * \param name name of flag to modify.
 * \param args_right array of arguments on the right of the equal sign.
 */
void
do_flag_restrict(const char *ns, dbref player, const char *name,
                 char *args_right[])
{
  FLAG *f;
  FLAGSPACE *n;
  int perms, negate_perms;

  if (!God(player)) {
    notify(player, T("You don't have enough magic for that."));
    return;
  }
  n = hashfind(ns, &htab_flagspaces);
  if (!(f = flag_hash_lookup(n, name, NOTYPE))) {
    notify_format(player, T("No such %s."), strlower(ns));
    return;
  }
  if (!args_right[1] || !*args_right[1]) {
    notify_format(player, T("How do you want to restrict that %s?"),
                  strlower(ns));
    return;
  }
  if (!strcasecmp(args_right[1], "any")) {
    perms = F_ANY;
  } else {
    perms = string_to_privs(flag_privs, args_right[1], 0);
    if ((!perms) || (perms & (F_INTERNAL | F_DISABLED))) {
      notify(player, T("I don't understand those permissions."));
      return;
    }
  }
  if (args_right[2] && *args_right[2]) {
    if (!strcasecmp(args_right[2], "any")) {
      negate_perms = F_ANY;
    } else {
      negate_perms = string_to_privs(flag_privs, args_right[2], 0);
      if ((!negate_perms) || (negate_perms & (F_INTERNAL | F_DISABLED))) {
        notify(player, T("I don't understand those permissions."));
        return;
      }
    }
  } else {
    negate_perms = string_to_privs(flag_privs, args_right[1], 0);
  }
  f->perms = perms;
  f->negate_perms = negate_perms;
  notify_format(player, T("Permissions on %s %s set."), f->name, strlower(ns));
}

/** Change the type of a flag.
 * \verbatim
 * This is the user-interface to @flag/type, which uses this syntax:
 *
 * @flag/type <flag> = <type(s)>
 *
 * We refuse to change the type of a flag if there are objects
 * with the flag that would no longer be of the correct type.
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of flag to modify.
 * \param type_string list of types.
 */
void
do_flag_type(const char *ns, dbref player, const char *name, char *type_string)
{
  FLAG *f;
  FLAGSPACE *n;
  int type;
  dbref it;

  if (!God(player)) {
    notify(player, T("You don't have enough magic for that."));
    return;
  }
  n = hashfind(ns, &htab_flagspaces);
  if (!(f = flag_hash_lookup(n, name, NOTYPE))) {
    notify_format(player, T("No such %s."), strlower(ns));
    return;
  }
  if (!type_string || !*type_string) {
    notify_format(player, T("What type do you want to make that %s?"),
                  strlower(ns));
    return;
  }
  if (!strcasecmp(type_string, "any")) {
    type = NOTYPE;
  } else {
    type = string_to_privs(type_privs, type_string, 0);
    if (!type) {
      notify(player, T("I don't understand the list of types."));
      return;
    }
    /* Are there any objects with the flag that don't match these
     * types?
     */
    for (it = 0; it < db_top; it++) {
      if (!(type & Typeof(it)) && has_flag_ns(n, it, f)) {
        notify_format(player,
                      T
                      ("Objects of other types already have this %s set. Search for them and remove it first."),
                      strlower(ns));
        return;
      }
    }
  }
  f->type = type;
  notify_format(player, T("Type of %s %s set."), f->name, strlower(ns));
}

/** Add a new flag
 * \verbatim
 * This function implements @flag/add, which uses this syntax:
 *
 * @flag/add <flag> = <letter>, <type(s)>, <perms>, <negate_perms>
 *
 * <letter> defaults to none. If given, it must not match an existing
 *   flag character that could apply to the given type
 * <type(s)> defaults to NOTYPE
 * <perms> defaults to any
 * <negate_perms> defaults to <perms>
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of the flag to add.
 * \param args_right array of arguments on the right side of the equal sign.
 */
void
do_flag_add(const char *ns, dbref player, const char *name, char *args_right[])
{
  char letter = '\0';
  int type = NOTYPE;
  int perms = F_ANY;
  int negate_perms = F_ANY;
  FLAG *f;
  FLAGSPACE *n;

  if (!God(player)) {
    notify(player, T("You don't have enough magic for that."));
    return;
  }
  if (!name || !*name) {
    notify_format(player, T("You must provide a name for the %s."),
                  strlower(ns));
    return;
  }
  if (strlen(name) == 1) {
    notify_format(player, T("%s names must be longer than one character."),
                  strinitial(ns));
    return;
  }
  if (strchr(name, ' ')) {
    notify_format(player, T("%s names may not contain spaces."),
                  strinitial(ns));
    return;
  }
  if (!good_flag_name(strupper(name))) {
    notify_format(player, T("That's not a valid %s name."), strlower(ns));
    return;
  }
  Flagspace_Lookup(n, ns);
  /* Do we have a letter? */
  if (!args_right) {
    notify(player, T("You must provide more information."));
    return;
  }
  if (args_right[1]) {
    if (strlen(args_right[1]) > 1) {
      notify_format(player, T("%s characters must be single characters."),
                    strinitial(ns));
      return;
    }
    letter = *args_right[1];
    /* Do we have a type? */
    if (args_right[2]) {
      if (*args_right[2] && strcasecmp(args_right[2], "any"))
        type = string_to_privs(type_privs, args_right[2], 0);
      if (!type) {
        notify(player, T("I don't understand the list of types."));
        return;
      }
    }
    /* Is this letter already in use for this type? */
    if (*args_right[1]) {
      if ((f = letter_to_flagptr(n, *args_right[1], type))) {
        notify_format(player, T("Letter conflicts with the %s %s."), f->name,
                      strlower(ns));
        return;
      }
    }
    /* Do we have perms? */
    if (args_right[3] && *args_right[3]) {
      if (!strcasecmp(args_right[3], "any")) {
        perms = F_ANY;
      } else {
        perms = string_to_privs(flag_privs, args_right[3], 0);
        if ((!perms) || (perms & (F_INTERNAL | F_DISABLED))) {
          notify(player, T("I don't understand those permissions."));
          return;
        }
      }
    }
    if (args_right[4] && *args_right[4]) {
      if (!strcasecmp(args_right[4], "any")) {
        negate_perms = F_ANY;
      } else {
        negate_perms = string_to_privs(flag_privs, args_right[4], 0);
        if ((!negate_perms) || (negate_perms & (F_INTERNAL | F_DISABLED))) {
          notify(player, T("I don't understand those permissions."));
          return;
        }
      }
    } else
      negate_perms = perms;
  }
  /* Ok, let's do it. */
  add_flag_generic(ns, name, letter, type, perms, negate_perms);
  /* Did it work? */
  if (match_flag_ns(n, name))
    do_flag_info(ns, player, name);
  else
    notify_format(player, T("Unknown failure adding %s."), strlower(ns));
}

/** Alias a flag.
 * \verbatim
 * This function implements the @flag/alias commmand.
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of the flag to alias.
 * \param alias name of the alias.
 */
void
do_flag_alias(const char *ns, dbref player, const char *name, const char *alias)
{
  FLAG *f, *af;
  FLAGSPACE *n;
  int delete = 0;
  if (!God(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  if (!alias || !*alias) {
    notify(player, T("You must provide a name for the alias."));
    return;
  }
  if (*alias == '!') {
    delete = 1;
    alias++;
  }
  if (strlen(alias) <= 1) {
    notify_format(player, T("%s aliases must be longer than one character."),
                  strinitial(ns));
    return;
  }
  if (strchr(alias, ' ')) {
    notify_format(player, T("%s aliases may not contain spaces."),
                  strinitial(ns));
    return;
  }
  n = hashfind(ns, &htab_flagspaces);
  if (!n) {
    notify_format(player, T("Internal error: Unknown flag space '%s'!"), ns);
    return;
  }

  af = match_flag_ns(n, alias);
  if (!delete && af) {
    notify_format(player, T("That alias already matches the %s %s."),
                  af->name, strlower(ns));
    return;
  }
  f = match_flag_ns(n, name);
  if (!f) {
    notify_format(player, T("I don't know that %s."), strlower(ns));
    return;
  }
  if (f->perms & F_DISABLED) {
    notify_format(player, T("That %s is disabled."), strlower(ns));
    return;
  }
  if (delete && !af) {
    notify_format(player, T("That isn't an alias of the %s %s."),
                  f->name, strlower(ns));
    return;
  }
  if (delete) {
    /* Delete the alias in the ptab if it's really an alias! */
    if (!strcasecmp(n->flags[f->bitpos]->name, alias)) {
      notify_format(player, T("That's the %s's name, not an alias."),
                    strlower(ns));
      return;
    }
    ptab_delete(n->tab, alias);
    if (match_flag_ns(n, alias))
      notify(player, T("Unknown failure deleting alias."));
    else
      do_flag_info(ns, player, f->name);
  } else {
    /* Insert the flag in the ptab by the given alias */
    if (alias_flag_generic(ns, name, alias))
      do_flag_info(ns, player, alias);
    else
      notify(player, T("Unknown failure adding alias."));
  }
}

 /** Add a new alias for a flag.
  * \param ns name of the flagspace to use.
  * \param name name of the flag
  * \param alias new alias for the flag
  * \retval 1 alias added successfully
  * \retval 0 failed to add alias
  */
int
alias_flag_generic(const char *ns, const char *name, const char *alias)
{
  FLAG *f;
  FLAGSPACE *n;

  Flagspace_Lookup(n, ns);

  f = match_flag_ns(n, name);
  if (!f) {
    return 0;                   /* no such flag 'name' */
  }

  if (ptab_find_exact(n->tab, strupper(alias))) {
    return 0;                   /* a flag called 'alias' already exists */
  }

  if (FLAG_REF(f->perms) == 0xFFU)
    return 0;                   /* Too many copies already */

  f->perms = INCR_FLAG_REF(f->perms);

  ptab_insert_one(n->tab, strupper(alias), f);

  return (match_flag_ns(n, alias) ? 1 : 0);
}


/** Change a flag's letter.
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of the flag.
 * \param letter The new alias, or an empty string to remove the alias.
 */
void
do_flag_letter(const char *ns, dbref player, const char *name,
               const char *letter)
{
  FLAG *f;
  FLAGSPACE *n;

  if (!God(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  Flagspace_Lookup(n, ns);
  f = match_flag_ns(n, name);
  if (!f) {
    notify_format(player, T("I don't know that %s."), strlower(ns));
    return;
  }
  if (letter && *letter) {
    FLAG *other;

    if (strlen(letter) > 1) {
      notify_format(player, T("%s characters must be single characters."),
                    strinitial(ns));
      return;
    }

    if ((other = letter_to_flagptr(n, *letter, f->type))) {
      notify_format(player, T("Letter conflicts with the %s %s."),
                    other->name, strlower(ns));
      return;
    }

    f->letter = *letter;
    notify_format(player, T("Letter for %s %s set to '%c'."),
                  strlower(ns), f->name, *letter);
  } else {                      /* Clear a flag */
    f->letter = '\0';
    notify_format(player, T("Letter for %s %s cleared."), strlower(ns),
                  f->name);
  }
}


/** Disable a flag.
 * \verbatim
 * This function implements @flag/disable.
 * Only God can do this, and it makes the flag effectively
 * unusuable - it's invisible to all but God, and can't be set, cleared,
 * or tested against.
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of the flag to disable.
 */
void
do_flag_disable(const char *ns, dbref player, const char *name)
{
  FLAG *f;
  FLAGSPACE *n;
  if (!God(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  Flagspace_Lookup(n, ns);
  f = match_flag_ns(n, name);
  if (!f) {
    notify_format(player, T("I don't know that %s."), strlower(ns));
    return;
  }
  if (f->perms & F_DISABLED) {
    notify_format(player, T("That %s is already disabled."), strlower(ns));
    return;
  }
  /* Do it. */
  f->perms |= F_DISABLED;
  notify_format(player, T("%s %s disabled."), strinitial(ns), f->name);
}

/** Delete a flag.
 * \verbatim
 * This function implements @flag/delete.
 * Only God can do this, and clears the flag on everyone
 * and then removes it and its aliases from the tables.
 * Danger, Will Robinson!
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of the flag to delete.
 */
void
do_flag_delete(const char *ns, dbref player, const char *name)
{
  FLAG *f, *tmpf;
  const char *flagname;
  dbref i;
  int got_one;
  FLAGSPACE *n;

  if (!God(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  n = hashfind(ns, &htab_flagspaces);
  if (!n) {
    notify_format(player, T("Internal error: Unknown flagspace '%s'!"), ns);
    return;
  }
  f = ptab_find_exact(n->tab, name);
  if (!f) {
    notify_format(player, T("I don't know that %s."), strlower(ns));
    return;
  }
  if (f->perms & F_INTERNAL) {
    notify(player, T("There are probably easier ways to crash your MUSH."));
    return;
  }
  /* Remove aliases. Convoluted because ptab_delete probably trashes
   * the firstentry/nextentry stuff
   */
  do {
    got_one = 0;
    tmpf = ptab_firstentry_new(n->tab, &flagname);
    while (tmpf) {
      if (!strcmp(tmpf->name, f->name) &&
          strcmp(n->flags[f->bitpos]->name, flagname)) {
        ptab_delete(n->tab, flagname);
        got_one = 1;
        break;
      }
      tmpf = ptab_nextentry_new(n->tab, &flagname);
    }
  } while (got_one);
  /* Reset the flag on all objects */
  for (i = 0; i < db_top; i++) {
    if (n->tab == &ptab_flag)
      Flags(i) = clear_flag_bitmask_ns(n, Flags(i), f->bitpos);
    else
      Powers(i) = clear_flag_bitmask_ns(n, Powers(i), f->bitpos);
  }
  /* Remove the flag's entry in flags */
  n->flags[f->bitpos] = NULL;
  /* Remove the flag from the ptab */
  ptab_delete(n->tab, f->name);
  notify_format(player, T("%s %s deleted."), strinitial(ns), f->name);
  /* Free the flag. */
  mush_free((void *) f->name, "flag.name");
  slab_free(flag_slab, f);
}

/** Enable a disabled flag.
 * \verbatim
 * This function implements @flag/enable.
 * Only God can do this, and it reverses /disable.
 * \endverbatim
 * \param ns name of the flagspace to use.
 * \param player the enactor.
 * \param name name of the flag to enable.
 */
void
do_flag_enable(const char *ns, dbref player, const char *name)
{
  FLAG *f;
  FLAGSPACE *n;
  if (!God(player)) {
    notify(player, T("You don't look like God."));
    return;
  }
  Flagspace_Lookup(n, ns);
  f = match_flag_ns(n, name);
  if (!f) {
    notify_format(player, T("I don't know that %s."), strlower(ns));
    return;
  }
  if (!(f->perms & F_DISABLED)) {
    notify_format(player, T("That %s is not disabled."), strlower(ns));
    return;
  }
  /* Do it. */
  f->perms &= ~F_DISABLED;
  notify_format(player, T("%s %s enabled."), strinitial(ns), f->name);
}


static char *
list_aliases(FLAGSPACE *n, FLAG *given)
{
  FLAG *f;
  static char buf[BUFFER_LEN];
  char *bp;
  const char *flagname;
  int first = 1;

  bp = buf;
  f = ptab_firstentry_new(n->tab, &flagname);
  while (f) {
    if (!strcmp(given->name, f->name) &&
        strcmp(n->flags[f->bitpos]->name, flagname)) {
      /* This is an alias! */
      if (!first)
        safe_chr(' ', buf, &bp);
      first = 0;
      safe_str(flagname, buf, &bp);
    }
    f = ptab_nextentry_new(n->tab, &flagname);
  }
  *bp = '\0';
  return buf;
}


/** Return a list of all flags.
 * \param ns name of namespace to search.
 * \param name wildcard to match against flag names, or NULL for all.
 * \param privs the looker, for permission checking.
 * \param which a bitmask of 0x1 (flag chars) and 0x2 (flag names).
 */
char *
list_all_flags(const char *ns, const char *name, dbref privs, int which)
{
  FLAG *f;
  char **ptrs;
  int i, numptrs = 0;
  static char buf[BUFFER_LEN];
  char *bp;
  int disallowed;
  FLAGSPACE *n;

  Flagspace_Lookup(n, ns);
  disallowed = God(privs) ? F_INTERNAL : (F_INTERNAL | F_DISABLED);
  if (!Hasprivs(privs))
    disallowed |= (F_DARK | F_MDARK);
  ptrs = (char **) malloc(n->flagbits * sizeof(char *));
  for (i = 0; i < n->flagbits; i++) {
    if ((f = n->flags[i]) && !(f->perms & disallowed)) {
      if (!name || !*name || quick_wild(name, f->name))
        ptrs[numptrs++] = (char *) f->name;
    }
  }
  do_gensort(privs, ptrs, NULL, numptrs, ALPHANUM_LIST);
  bp = buf;
  for (i = 0; i < numptrs; i++) {
    switch (which) {
    case 0x3:
      if (i)
        safe_strl(", ", 2, buf, &bp);
      safe_str(ptrs[i], buf, &bp);
      f = match_flag_ns(n, ptrs[i]);
      if (!f)
        break;
      if (f->letter != '\0')
        safe_format(buf, &bp, " (%c)", f->letter);
      if (f->perms & F_DISABLED)
        safe_str(T(" (disabled)"), buf, &bp);
      break;
    case 0x2:
      if (i)
        safe_chr(' ', buf, &bp);
      safe_str(ptrs[i], buf, &bp);
      break;
    case 0x1:
      f = match_flag_ns(n, ptrs[i]);
      if (f && (f->letter != '\0'))
        safe_chr(f->letter, buf, &bp);
      break;
    }
  }
  *bp = '\0';
  free(ptrs);
  return buf;
}

const char *
flag_list_to_lock_string(object_flag_type flags, object_flag_type powers)
{
  FLAGSPACE *n;
  FLAG *f;
  int i, first = 1;
  char buff[BUFFER_LEN];
  char *bp;

  bp = buff;
  if (flags) {
    Flagspace_Lookup(n, "FLAG");
    for (i = 0; i < n->flagbits; i++) {
      if ((f = n->flags[i]) && has_bit(flags, f->bitpos)) {
        if (!first)
          safe_chr('|', buff, &bp);
        safe_format(buff, &bp, "FLAG^%s", f->name);
        first = 0;
      }
    }
  }

  if (powers) {
    Flagspace_Lookup(n, "POWER");
    for (i = 0; i < n->flagbits; i++) {
      if ((f = n->flags[i]) && has_bit(powers, f->bitpos)) {
        if (!first)
          safe_chr('|', buff, &bp);
        safe_format(buff, &bp, "POWER^%s", f->name);
        first = 0;
      }
    }
  }

  if (first) {
    return "";
  }

  *bp = '\0';
  return tprintf("(%s)", buff);
}


/*--------------------------------------------------------------------------
 * Powers
 */


/** Return the object's power for examine.
 * \param player looker, for permission checking.
 * \param thing object to list powers for.
 * \return a string containing all the visible power names on the object.
 */
const char *
power_description(dbref player, dbref thing)
{
  static char buf[BUFFER_LEN];
  char *bp;
  bp = buf;
  safe_str(bits_to_string("POWER", Powers(thing), player, thing), buf, &bp);
  *bp = '\0';
  return buf;
}

/** Lookup table for good_flag_name */
extern char atr_name_table[UCHAR_MAX + 1];

/** Is s a good flag name? We allow the same characters we do in attribute
 *  names, and the same size limit.
 * \param s a string to test
 * \return 1 valid name
 * \return 0 invalid name
 */
int
good_flag_name(char const *s)
{
  const unsigned char *a;
  int len = 0;
  if (!s || !*s)
    return 0;
  for (a = (const unsigned char *) s; *a; a++, len++)
    if (!atr_name_table[*a])
      return 0;
  if (*(s + len - 1) == '`')
    return 0;
  return len <= ATTRIBUTE_NAME_LIMIT;
}
