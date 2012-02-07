/**
 * \file dbdefs.h
 */

#ifndef __DBDEFS_H
#define __DBDEFS_H

#include <stdio.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include "mushdb.h"
#include "htab.h"
#include "chunk.h"

extern dbref first_free;        /* pointer to free list */

/*-------------------------------------------------------------------------
 * Database access macros
 */

/* References an whole object */
#define REFDB(x)        &db[x]

#define Name(x)         (db[(x)].name)
#define Flags(x)        (db[(x)].flags)
#define Owner(x)        (db[(x)].owner)

#define Location(x)     (db[(x)].location)
#define Zone(x)         (db[(x)].zone)

#define Contents(x) (db[(x)].contents)
#define Next(x)     (db[(x)].next)
#define Home(x)     (db[(x)].exits)
#define Exits(x)    (db[(x)].exits)
#define List(x)     (db[(x)].list)

/* These are only for exits */
#define Source(x)   (db[(x)].exits)
#define Destination(x) (db[(x)].location)

#define Locks(x)        (db[(x)].locks)

#define CreTime(x)      (db[(x)].creation_time)
#define ModTime(x)      (db[(x)].modification_time)

#define AttrCount(x)    (db[(x)].attrcount)

/* Moved from warnings.c because create.c needs it. */
#define Warnings(x)      (db[(x)].warnings)

#define Pennies(thing) (db[thing].penn)

#define Parent(x)  (db[(x)].parent)
#define Powers(x)  (db[(x)].powers)

/* Generic type check */
#define Type(x)   (db[(x)].type)
#define Typeof(x) (Type(x) & ~TYPE_MARKED)

/* Check for a specific one */
#define IsPlayer(x)     ((Typeof(x) & TYPE_PLAYER) == TYPE_PLAYER)
#define IsRoom(x)       ((Typeof(x) & TYPE_ROOM) == TYPE_ROOM)
#define IsThing(x)      ((Typeof(x) & TYPE_THING) == TYPE_THING)
#define IsExit(x)       ((Typeof(x) & TYPE_EXIT) == TYPE_EXIT)
/* Was Destroyed() */
#define IsGarbage(x)    ((Typeof(x) & TYPE_GARBAGE) == TYPE_GARBAGE)
#define Marked(x)       ((db[(x)].type & TYPE_MARKED) == TYPE_MARKED)

#define IS(thing,type,flag) \
                     ((Typeof(thing) == type) && has_flag_by_name(thing,flag,type))

#define GoodObject(x) ((x >= 0) && (x < db_top))

#define RealGoodObject(x) (GoodObject(x) && !IsGarbage(x))

/******* Player toggles */
#define Connected(x)    (IS(x, TYPE_PLAYER, "CONNECTED"))
#define Track_Money(x)       (IS(x, TYPE_PLAYER, "TRACK_MONEY"))
#define ZMaster(x)      (IS(x, TYPE_PLAYER, "ZONE"))
#define Unregistered(x) (IS(x, TYPE_PLAYER, "UNREGISTERED"))
#define Fixed(x)        (IS(Owner(x), TYPE_PLAYER, "FIXED"))
#define Vacation(x)     (IS(x, TYPE_PLAYER, "ON-VACATION"))

/* Flags that apply to players, and all their stuff,
 * so check the Owner() of the object.
 */

#define Terse(x)        (IS(Owner(x), TYPE_PLAYER, "TERSE") || IS(x, TYPE_THING, "TERSE"))
#define Myopic(x)       (IS(Owner(x), TYPE_PLAYER, "MYOPIC"))
#define Nospoof(x)      (IS(Owner(x),TYPE_PLAYER,"NOSPOOF") || has_flag_by_name(x,"NOSPOOF",NOTYPE))
#define Paranoid(x)      (IS(Owner(x),TYPE_PLAYER,"PARANOID") || has_flag_by_name(x,"PARANOID",NOTYPE))
#define Gagged(x)       (IS(Owner(x), TYPE_PLAYER, "GAGGED"))
#define ShowAnsi(x)     (IS(Owner(x), TYPE_PLAYER, "ANSI"))
#define ShowAnsiColor(x) (IS(Owner(x), TYPE_PLAYER, "COLOR"))

/******* Thing toggles */
#define DestOk(x)       (IS(x, TYPE_THING, "DESTROY_OK"))
#define NoLeave(x)      (IS(x, TYPE_THING, "NOLEAVE"))
#define ThingListen(x)  (IS(x, TYPE_THING, "MONITOR"))
#define ThingInhearit(x) \
                        (IS(x, TYPE_THING, "LISTEN_PARENT"))    /* 0x80 */
#define ThingZTel(x)            (IS(x, TYPE_THING, "Z_TEL"))

/******* Room toggles */
#define Floating(x)     (IS(x, TYPE_ROOM, "FLOATING"))  /* 0x8 */
#define Abode(x)        (IS(x, TYPE_ROOM, "ABODE"))     /* 0x10 */
#define JumpOk(x)       (IS(x, TYPE_ROOM, "JUMP_OK"))   /* 0x20 */
#define NoTel(x)        (IS(x, TYPE_ROOM, "NO_TEL"))    /* 0x40 */
#define RoomListen(x)   (IS(x, TYPE_ROOM, "LISTENER"))  /* 0x100 */
#define RoomZTel(x)             (IS(x, TYPE_ROOM, "Z_TEL"))     /* 0x200 */
#define RoomInhearit(x) (IS(x, TYPE_ROOM, "LISTEN_PARENT"))     /* 0x400 */

#define Uninspected(x)  (IS(x, TYPE_ROOM, "UNINSPECTED"))       /* 0x1000 */

#define ZTel(x) (ThingZTel(x) || RoomZTel(x))

/******* Exit toggles */
#define Cloudy(x)       (IS(x, TYPE_EXIT, "CLOUDY"))    /* 0x8 */
/* These must be passed exit dbrefs */
#define HomeExit(x)     (Destination(x) == HOME)
#define VariableExit(x) (Destination(x) == AMBIGUOUS)

/* Flags anything can have */

#define Audible(x)      (has_flag_by_name(x, "AUDIBLE", NOTYPE))
#define ChanUseFirstMatch(x) (has_flag_by_name(x, "CHAN_USEFIRSTMATCH", NOTYPE))
#define ChownOk(x)      (has_flag_by_name(x, "CHOWN_OK", NOTYPE))
#define Dark(x)         (has_flag_by_name(x, "DARK", NOTYPE))
#define Debug(x)        (has_flag_by_name(x, "DEBUG", NOTYPE))
#define EnterOk(x)      (has_flag_by_name(x, "ENTER_OK", NOTYPE))
#define Going(x)        (has_flag_by_name(x, "GOING", NOTYPE))
#define Going_Twice(x)  (has_flag_by_name(x, "GOING_TWICE", NOTYPE))
#define Halted(x)       (has_flag_by_name(x, "HALT", NOTYPE))
#define Haven(x)        (has_flag_by_name(x, "HAVEN", NOTYPE))
#define Heavy(x)        (has_flag_by_name(x, "HEAVY", NOTYPE))
#define Inherit(x)      (has_flag_by_name(x, "TRUST", NOTYPE))
#define Light(x)        (has_flag_by_name(x, "LIGHT", NOTYPE))
#define LinkOk(x)       (has_flag_by_name(x, "LINK_OK", NOTYPE))
#define OpenOk(x)       (has_flag_by_name(x, "OPEN_OK", TYPE_ROOM))
#define Loud(x)         (has_flag_by_name(x, "LOUD", NOTYPE))
#define Mistrust(x)     (has_flag_by_name(x, "MISTRUST", TYPE_THING|TYPE_EXIT|TYPE_ROOM))
#define NoCommand(x)    (has_flag_by_name(x, "NO_COMMAND", NOTYPE))
#define NoWarn(x)       (has_flag_by_name(x, "NO_WARN", NOTYPE))
#define Opaque(x)       (has_flag_by_name(x, "OPAQUE", NOTYPE))
#define Orphan(x)       (has_flag_by_name(x, "ORPHAN", NOTYPE))
#define Puppet(x)       (has_flag_by_name(x, "PUPPET", TYPE_THING|TYPE_ROOM))
#define Quiet(x)        (has_flag_by_name(x, "QUIET", NOTYPE))
#define Safe(x)         (has_flag_by_name(x, "SAFE", NOTYPE))
#define Sticky(x)       (has_flag_by_name(x, "STICKY", NOTYPE))
#define Suspect(x)      (has_flag_by_name(x,"SUSPECT", NOTYPE))
#define Transparented(x)      (has_flag_by_name(x, "TRANSPARENT", NOTYPE))
#define Unfind(x)       (has_flag_by_name(x, "UNFINDABLE", NOTYPE))
#define Verbose(x)      (has_flag_by_name(x, "VERBOSE", NOTYPE))
#define Visual(x)       (has_flag_by_name(x, "VISUAL", NOTYPE))
#define Can_Dark(x)     (Wizard(x) || has_power_by_name(x, "Can_Dark", NOTYPE))

/* Attribute flags */
#define AF_Internal(a) ((a)->flags & AF_INTERNAL)
#define AF_Wizard(a) ((a)->flags & AF_WIZARD)
#define AF_Locked(a) ((a)->flags & AF_LOCKED)
#define AF_Noprog(a) ((a)->flags & AF_NOPROG)
#define AF_Mdark(a) ((a)->flags & AF_MDARK)
#define AF_Private(a) ((a)->flags & AF_PRIVATE)
#define AF_Nocopy(a) ((a)->flags & AF_NOCOPY)
#define AF_Visual(a) ((a)->flags & AF_VISUAL)
#define AF_Regexp(a) ((a)->flags & AF_REGEXP)
#define AF_Case(a) ((a)->flags & AF_CASE)
#define AF_Safe(a) ((a)->flags & AF_SAFE)
#define AF_Command(a) ((a)->flags & AF_COMMAND)
#define AF_Listen(a) ((a)->flags & AF_LISTEN)
#define AF_Nodump(a) ((a)->flags & AF_NODUMP)
#define AF_Listed(a) ((a)->flags & AF_LISTED)
#define AF_Prefixmatch(a) ((a)->flags & AF_PREFIXMATCH)
#define AF_Veiled(a) ((a)->flags & AF_VEILED)
#define AF_Debug(a) ((a)->flags & AF_DEBUG)
#define AF_NoDebug(a) ((a)->flags & AF_NODEBUG)
#define AF_Nearby(a) ((a)->flags & AF_NEARBY)
#define AF_Public(a) ((a)->flags & AF_PUBLIC)
#define AF_Mhear(a) ((a)->flags & AF_MHEAR)
#define AF_Ahear(a) ((a)->flags & AF_AHEAR)

/* Non-mortal checks */
#define God(x)  ((x) == GOD)
#define Royalty(x)      (has_flag_by_name(x, "ROYALTY", NOTYPE))
#define Wizard(x)       (God(x) || has_flag_by_name(x,"WIZARD", NOTYPE))
#define Hasprivs(x)     (God(x) || Royalty(x) || Wizard(x))

#define IsQuiet(x)      (Quiet(x) || Quiet(Owner(x)))
#define AreQuiet(x,y)   (Quiet(x) || (Quiet(y) && (Owner(y) == x)))
#define Mobile(x)       (IsPlayer(x) || IsThing(x))
#define Alive(x)        (IsPlayer(x) || Puppet(x) || \
   (Audible(x) && atr_get_noparent(x,"FORWARDLIST")))
/* Was Dark() */
#define DarkLegal(x)    (Dark(x) && (Can_Dark(x) || !Alive(x)))



/* This is carefully ordered, from most to least likely. Hopefully. */
#define CanEval(x,y)    (!(SAFER_UFUN) || !Hasprivs(y) || God(x) || \
        ((Wizard(x) || (Royalty(x) && !Wizard(y))) && !God(y)))

/* AF_PUBLIC overrides SAFER_UFUN */
#define CanEvalAttr(x,y,a) (CanEval(x,y) || AF_Public(a))

/* Note that this is a utility to determine the objects which may or may */
/* not be controlled, rather than a strict check for the INHERIT flag */
#define Owns(p,x) (Owner(p) == Owner(x))


/* Was Inherit() */
#define Inheritable(x)  (IsPlayer(x) || Inherit(x) || \
                        Inherit(Owner(x)) || Wizard(x))

#define NoWarnable(x) (NoWarn(x) || NoWarn(Owner(x)))

/* Ancestor_Parent() - returns appropriate ancestor object */
#define Ancestor_Parent(x) (Orphan(x) ? NOTHING : \
    (IsRoom(x) ? ANCESTOR_ROOM : \
     (IsExit(x) ? ANCESTOR_EXIT : \
      (IsPlayer(x) ? ANCESTOR_PLAYER : \
       (IsThing(x) ? ANCESTOR_THING : NOTHING )))))


/*--------------------------------------------------------------------------
 * Other db stuff
 */

/** An object in the database.
 *
 */
struct object {
  const char *name;             /**< The name of the object */
  /** An overloaded pointer.
   * For things and players, points to container object.
   * For exits, points to destination.
   * For rooms, points to drop-to.
   */
  dbref location;
  dbref contents;               /**< Pointer to first item */
  /** An overloaded pointer.
   * For things and players, points to home.
   * For rooms, points to first exit.
   * For exits, points to source room.
   */
  dbref exits;
  dbref next;                   /**< pointer to next in contents/exits chain */
  dbref parent;                 /**< pointer to parent object */
  struct lock_list *locks;      /**< list of locks set on the object */
  dbref owner;                  /**< who controls this object */
  dbref zone;                   /**< zone master object number */
  int penn;                     /**< number of pennies object contains */
  warn_type warnings;           /**< bitflags of warning types */
  time_t creation_time;         /**< Time/date of object creation */
  /** Last modifiction time.
   * For players, the number of failed logins.
   * For other objects, the time/date of last modification to its attributes.
   */
  time_t modification_time;
  int attrcount;                /**< Number of attribs on the object */
  int type;                     /**< Object's type */
  object_flag_type flags;       /**< Pointer to flag bit array */
  object_flag_type powers;      /**< Pointer to power bit array */
  ALIST *list;                  /**< list of attributes on the object */
};

/** A structure to hold database statistics.
 * This structure is used by get_stats() in wiz.c to group
 * counts of various objects in the database.
 */
struct db_stat_info {
  int total;    /**< Total count */
  int players;  /**< Player count */
  int rooms;    /**< Room count */
  int exits;    /**< Exit count */
  int things;   /**< Thing count */
  int garbage;  /**< Garbage count */
};

extern struct object *db;
extern dbref db_top;

extern void *get_objdata(dbref thing, const char *keybase);
extern void *set_objdata(dbref thing, const char *keybase, void *data);
extern void clear_objdata(dbref thing);

#define DOLIST(var, first)\
    for((var) = (first); GoodObject((var)); (var) = Next(var))

#define PUSH(thing, locative) \
    ((Next(thing) = (locative)), (locative) = (thing))

#define DOLIST_VISIBLE(var, first, player)\
    for((var) = first_visible((player), (first)); GoodObject((var)); (var) = first_visible((player), Next(var)))

typedef uint32_t mail_flag;

/** A mail message.
 * This structure represents a single mail message in the linked list
 * of messages that comprises the mail database. Mail messages are
 * stored in a doubly-linked list sorted by message recipient.
 */
struct mail {
  struct mail *next;            /**< Pointer to next message */
  struct mail *prev;            /**< Pointer to previous message */
  dbref to;                     /**< Recipient dbref */
  dbref from;                   /**< Sender's dbref */
  time_t from_ctime;            /**< Sender's creation time */
  chunk_reference_t msgid;      /**< Message text, compressed */
  time_t time;                  /**< Message date/time */
  unsigned char *subject;       /**< Message subject, compressed */
  mail_flag read;               /**< Bitflags of message status */
};

typedef struct mail MAIL;


extern const char *EOD;

#define SPOOF(executor, enactor, sw) \
  if (SW_ISSET(sw, SWITCH_SPOOF) && (controls(executor, enactor) || Can_Nspemit(executor))) {\
    executor = enactor; orator = enactor; \
  }

#endif                          /* __DBDEFS_H */
