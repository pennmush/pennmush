/**
 * \file oldflags.h
 *
 * \brief The bit values we used to use for flags and toggles in the old days
 * For backwards compatability when loading old databases.
 */

#ifndef __OLDFLAGS_H
#define __OLDFLAGS_H


/*--------------------------------------------------------------------------
 * Generic flags
 */

#define OLD_TYPE_ROOM       0x0
#define OLD_TYPE_THING      0x1
#define OLD_TYPE_EXIT       0x2
#define OLD_TYPE_PLAYER     0x3
#define OLD_TYPE_GARBAGE    0x6
#define OLD_NOTYPE          0x7 /* no particular type */
#define OLD_TYPE_MASK       0x7 /* room for expansion */

/* -- empty slot 0x8 -- */
#define WIZARD          0x10    /* gets automatic control */
#define LINK_OK         0x20    /* anybody can link to this room */
#define DARK            0x40    /* contents of room are not printed */
                                /* exit doesn't appear as 'obvious' */
#define VERBOSE         0x80    /* print out command before executing it */
#define STICKY          0x100   /* goes home when dropped */
#define TRANSPARENTED     0x200 /* can look through exit to see next room,
                                 * or room "long exit display.
                                 * We don't call it TRANSPARENT because
                                 * that's a Solaris macro
                                 */
#define HAVEN           0x400   /* this room disallows kills in it */
                                /* on a player, disallow paging, */
#define QUIET           0x800   /* On an object, will not emit 'set'
                                 * messages.. on a player.. will not see ANY
                                 * set messages
                                 */
#define HALT            0x1000  /* object cannot perform actions */
#define UNFIND          0x2000  /* object cannot be found (or found in */
#define GOING           0x4000  /* object is available for recycling */
#define ACCESSED        0x8000  /* Obsolete - only for conversion */
/* -- empty slot 0x8000, once your db is converted -- */
#define MARKED          0x10000 /* flag used to trace db checking of room
                                 * linkages. */
#define NOWARN          0x20000 /* Object will not cause warnings.
                                 * If set on a player, player will not
                                 * get warnings (independent of player's
                                 * @warning setting
                                 */

#define CHOWN_OK        0x40000 /* object can be 'stolen' and made yours */
#define ENTER_OK        0x80000 /* object basically acts like a room with
                                 * only one exit (leave), on players
                                 * means that items can be given freely, AND
                                 * taken from!
                                 */
#define VISUAL          0x100000        /* People other than owners can see
                                         * property list of object.
                                         */
#define LIGHT           0x200000        /* Visible in DARK rooms */

#define ROYALTY         0x400000        /* can ex, and @tel like a wizard */

#define LOOK_OPAQUE          0x800000   /* Objects inside object will not be
                                         * seen on a look.
                                         */
#define INHERIT         0x1000000       /* Inherit objects can force their
                                         * owners. Inherit players have all
                                         * objects inherit.
                                         */

#define DEBUGGING       0x2000000       /* returns parser evals */
#define SAFE            0x4000000       /* cannot be destroyed */
#define STARTUP         0x8000000       /* Used for converting old dbs */
/* -- empty slot 0x8000000, if your db is already converted -- */
#define AUDIBLE         0x10000000      /* rooms are flagged as having emitter
                                         * exits. exits act like emitters,
                                         * sound propagates to the exit dest.
                                         */
#define NO_COMMAND      0x20000000      /* don't check for $commands */

#define GOING_TWICE     0x40000000      /* Marked for destruction, but
                                         * spared once. */
/* -- empty slot 0x80000000 -- */

/*--------------------------------------------------------------------------
 * Player flags
 */

#define PLAYER_TERSE    0x8     /* suppress autolook messages */
#define PLAYER_MYOPIC   0x10    /* look at everything as if player
                                 * doesn't control it.
                                 */
#define PLAYER_NOSPOOF  0x20    /* sees origin of emits */
#define PLAYER_SUSPECT  0x40    /* notifies of a player's name changes,
                                 * (dis)connects, and possible logs
                                 * logs commands.
                                 */
#define PLAYER_GAGGED   0x80    /* can only move */
#define PLAYER_MONITOR  0x100   /* sees (dis)connects broadcasted */
#define PLAYER_CONNECT  0x200   /* connected to game */
#define PLAYER_ANSI     0x400   /* enable sending of ansi control
                                 * sequences (for examine).
                                 */
#define PLAYER_ZONE     0x800   /* Zone Master (zone control owner) */
#define PLAYER_JURY   0x1000    /* Jury_ok Player */
#define PLAYER_JUDGE  0x2000    /* Judge player */
#define PLAYER_FIXED  0x4000    /* can't @tel or home */
#define PLAYER_UNREG  0x8000    /* Not yet registered */
#define PLAYER_VACATION 0x10000 /* On vacation */
#define PLAYER_COLOR      0x80000       /* ANSI color ok */
#define PLAYER_NOACCENTS 0x100000       /* Strip accented text on output */
#define PLAYER_PARANOID 0x200000        /* Paranoid nospoof */



/*--------------------------------------------------------------------------
 * Thing flags
 */

#define THING_DEST_OK   0x8     /* can be destroyed by anyone */
#define THING_PUPPET    0x10    /* echoes to its owner */
#define THING_LISTEN    0x20    /* checks for ^ patterns */
#define THING_NOLEAVE   0x40    /* Can't be left */
#define THING_INHEARIT  0x80    /* checks parent chain for ^ patterns */
#define THING_Z_TEL     0x100   /* If set on ZMO players may only @tel
                                   within the zone */

/*--------------------------------------------------------------------------
 * Room flags
 */

#define ROOM_FLOATING   0x8     /* room is not linked to rest of
                                 * MUSH. Don't blather about it.
                                 */
#define ROOM_ABODE      0x10    /* players may link themselves here */
#define ROOM_JUMP_OK    0x20    /* anyone may @teleport to here */
#define ROOM_NO_TEL     0x40    /* mortals cannot @tel from here */
#define ROOM_TEMPLE     0x80    /* objects dropped here are sacrificed
                                 * (destroyed) and player gets money.
                                 * Now used only for conversion.
                                 */
#define ROOM_LISTEN    0x100    /* checks for ^ patterns */
#define ROOM_Z_TEL     0x200    /* If set on a room, players may
                                 * only @tel to another room in the
                                 * same zone
                                 */
#define ROOM_INHEARIT  0x400    /* checks parent chain for ^ patterns */

#define ROOM_UNINSPECT 0x1000   /* Not inspected */


/*--------------------------------------------------------------------------
 * Exit flags
 */

#define EXIT_CLOUDY     0x8     /* Looking through a cloudy transparent
                                 * exit shows the room's desc, not contents.
                                 * Looking through a cloudy !trans exit,
                                 * shows the room's contents, not desc
                                 */

#endif                          /* __OLDFLAGS_H */
