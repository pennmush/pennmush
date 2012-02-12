/**
 * \file match.h
 *
 * \brief object matching routines from match.c
 *
 * \verbatim
 *
 * These functions do the matching and return the result:
 * match_result() - returns match, NOTHING, or AMBIGUOUS
 * noisy_match_result() - notifies player, returns match or NOTHING
 * last_match_result() - returns a match or NOTHING
 * match_controlled() - returns match if player controls, or NOTHING
 *
 * \endverbatim
 */

#ifndef __MATCH_H
#define __MATCH_H

#include "copyrite.h"

/* match constants */
  /* match modifiers */
#define MAT_CHECK_KEYS       0x000001  /**< Prefer an object we pass the \@lock/basic of */
#define MAT_GLOBAL           0x000002  /**< Check exits in the Master Room */
#define MAT_REMOTES          0x000004  /**< Check ZMR exits */
#define MAT_CONTROL          0x000008  /**< Only matched controlled objects */
  /* individual things to match */
#define MAT_ME               0x000010  /**< Match the string "me" to the looker */
#define MAT_HERE             0x000020  /**< Match the string "here" to the looker's location */
#define MAT_ABSOLUTE         0x000040  /**< Match any object by dbref */
#define MAT_PLAYER           0x000080  /**< Match *<playername> */
#define MAT_NEIGHBOR         0x000100  /**< Match objects in the looker's location */
#define MAT_POSSESSION       0x000200  /**< Match object in the looker's inventory */
#define MAT_CONTENTS         0x000400  /**< Only match objects which are in the looker's contents */
#define MAT_EXIT             0x000800  /**< Match a local exit */
#define MAT_PMATCH           0x001000  /**< Match the name of a player, with or without a leading "*" */
  /* special things to match */
#define MAT_CARRIED_EXIT     0x002000  /**< Match an exit carried by the looker (a room) */
#define MAT_CONTAINER        0x004000  /**< Match the name of looker's location */
#define MAT_REMOTE_CONTENTS  0x008000  /**< Obsolete */
#define MAT_NEAR             0x010000  /**< Matched object must be nearby to looker */
#define MAT_ENGLISH          0x020000  /**< Do English-style matching (this here 1st foo, etc) */
  /* types of match results - used internally */
#define MAT_NOISY            0x040000  /**< Show a message on failure */
#define MAT_LAST             0x080000  /**< For ambiguous results, return the last match */
#define MAT_TYPE             0x100000  /**< Only match objects of the specified type(s) */
#define MAT_EXACT            0x200000  /**< Don't do partial name matches */

  /* groups of things to match */
#define MAT_EVERYTHING   (MAT_ME|MAT_HERE|MAT_ABSOLUTE|MAT_PLAYER| \
                          MAT_NEIGHBOR|MAT_POSSESSION|MAT_EXIT|MAT_ENGLISH)
#define MAT_NEARBY       (MAT_EVERYTHING|MAT_NEAR)
#define MAT_OBJECTS      (MAT_ME|MAT_ABSOLUTE|MAT_PLAYER|MAT_NEIGHBOR| \
                          MAT_POSSESSION)
#define MAT_NEAR_THINGS  (MAT_OBJECTS|MAT_NEAR)
#define MAT_LIMITED      (MAT_ABSOLUTE|MAT_PLAYER|MAT_NEIGHBOR)
#define MAT_REMOTE       (MAT_ABSOLUTE|MAT_PLAYER|MAT_REMOTE_CONTENTS|MAT_EXIT)
#define MAT_OBJ_CONTENTS (MAT_POSSESSION|MAT_PLAYER|MAT_ABSOLUTE|MAT_CONTENTS|MAT_ENGLISH)


/* Functions we can call */
dbref
 match_result(dbref who, const char *xname, int type, long flags);
dbref
 match_result_relative(dbref who, dbref where, const char *xname, int type,
                      long flags);
dbref noisy_match_result(const dbref who, const char *name,
                         const int type, const long flags);
dbref last_match_result(const dbref who, const char *name,
                        const int type, const long flags);
dbref match_controlled(dbref player, const char *name);


int match_aliases(dbref match, const char *name);

#define match_thing(player,name) \
  noisy_match_result((player), (name), NOTYPE, MAT_EVERYTHING)

#endif                          /* __MATCH_H */
