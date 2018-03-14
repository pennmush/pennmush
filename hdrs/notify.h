/**
 * \file notify.h
 *
 * \brief Header file for the various notify_* functions and their helpers.
 */

#ifndef __NOTIFY_H
#define __NOTIFY_H

#include "mushtype.h"
#include "conf.h"

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

extern dbref orator;

#if defined(HAVE_GETTEXT) && !defined(DONT_TRANSLATE)
/** Macro for a translated string */
#define T(str) gettext(str)
/** Macro to note that a string has a translation but not to translate */
#define N_(str) gettext_noop(str)
#else
#define T(str) str
#define N_(str) str
#endif

char *WIN32_CDECL tprintf(const char *fmt, ...)
  __attribute__((__format__(__printf__, 1, 2)));

/* The #defs for our notify_anything hacks.. Errr. Functions */
#define NA_NORELAY 0x0001        /**< Don't relay sound */
#define NA_NOENTER 0x0002        /**< No newline at end */
#define NA_NOLISTEN 0x0004       /**< Implies NORELAY. Sorta. */
#define NA_NOPENTER 0x0010       /**< No newline, Pueblo-stylee. UNUSED. */
#define NA_PONLY 0x0020          /**< Pueblo-only. UNUSED. */
#define NA_PUPPET_OK 0x0040      /**< Ok to puppet */
#define NA_PUPPET_MSG 0x0080     /**< Message to a player from a puppet */
#define NA_MUST_PUPPET 0x0100    /**< Ok to puppet even in same room */
#define NA_INTER_HEAR 0x0200     /**< Message is auditory in nature */
#define NA_INTER_SEE 0x0400      /**< Message is visual in nature */
#define NA_INTER_PRESENCE 0x0800 /**< Message is about presence */
#define NA_NOSPOOF 0x1000        /**< Message comes via a NOSPOOF object. */
#define NA_PARANOID 0x2000       /**< Message comes via a PARANOID object. */
#define NA_NOPREFIX 0x4000       /**< Don't use \@prefix when forwarding */
#define NA_SPOOF 0x8000          /**< \@ns* message, overrides NOSPOOF */
#define NA_INTER_LOCK                                                          \
  0x10000 /**< Message subject to \@lock/interact even if not otherwise marked \
             */
#define NA_INTERACTION                                                         \
  (NA_INTER_HEAR | NA_INTER_SEE | NA_INTER_PRESENCE |                          \
   NA_INTER_LOCK)         /**< Message follows interaction rules */
#define NA_PROMPT 0x20000 /**< Message is a prompt, add GOAHEAD */
#define NA_PROPAGATE                                                           \
  0x40000 /**< Propagate this sound through audible exits/things */
#define NA_RELAY_ONCE 0x80000 /**< Relay a propagated sound just once */

/* notify.c */

/** Bitwise options for render_string() */
#define MSG_INTERNAL                                                           \
  0x00 /**< Original string containing internal markup, \\n lineendings */
#define MSG_PLAYER                                                             \
  0x01 /**< Being sent to a player. Uses \\r\\n lineendings, not \\n */
/* Any text sent to a player will be (MSG_PLAYER | modifiers below) */
#define MSG_PUEBLO 0x02 /**< HTML entities, Pueblo tags as HTML */
#define MSG_TELNET                                                             \
  0x04 /**< Output to telnet-aware connection. Escape char 255 */
#define MSG_STRIPACCENTS 0x08 /**< Strip/downgrade accents */

#define MSG_MARKUP                                                             \
  0x10 /**< Leave markup in internal format, rather than stripping/converting  \
          */
#define MSG_ANSI2 0x20    /**< Ansi-highlight only */
#define MSG_ANSI16 0x40   /**< 16 bit Color */
#define MSG_XTERM256 0x80 /**< XTERM 256 Color */
#ifndef WITHOUT_WEBSOCKETS
#define MSG_WEBSOCKETS   0x10000000
#endif /* undef WITHOUT_WEBSOCKETS */

/**
 * \verbatim
 * <font color="..." bgcolor=".."></font> (currently unusued)
 * \endverbatim
 */
#define MSG_FONTTAGS 0x100

#define MSG_PLAYER_COLORS                                                      \
  (MSG_ANSI2 | MSG_ANSI16 |                                                    \
   MSG_XTERM256) /* All possible player-renderings of color */
#define MSG_ANY_ANSI                                                           \
  (MSG_ANSI2 | MSG_ANSI16 | MSG_XTERM256) /* Any form of ANSI tag */

#define MSG_ALL_PLAYER                                                         \
  (MSG_PLAYER | MSG_PLAYER_COLORS | MSG_PUEBLO | MSG_TELNET | MSG_STRIPACCENTS)

/** A notify_anything lookup function type definition */
typedef dbref (*na_lookup)(dbref, void *);

/** Used by notify_anything() for formatting a message through a ufun for each
 * listener */
struct format_msg {
  dbref thing; /**< Object to ufun an attr from. Use AMBIGUOUS for the target */
  char *attr;  /**< Attribute to ufun */
  int checkprivs; /**< Check that the speaker has permission to get the attr? */
  int targetarg;  /**< The arg to set to the target's dbref, or -1 to not */
  int numargs;    /**< Number of arguments in args to pass to the ufun */
  char *args[MAX_STACK_ARGS]; /**< Array of arguments to pass to ufun */
};
const char *render_string(const char *message, int output_type);
void notify_list(dbref speaker, dbref thing, const char *atr, const char *msg,
                 int flags, dbref skip);

/* No longer passes an ns_func, all things will use the same nospoof function.
 * Where a NULL ns_func was used before, now just
 * pass NA_SPOOF in the flags */
void notify_anything(dbref executor, dbref speaker, na_lookup func, void *fdata,
                     dbref *skips, int flags, const char *message,
                     const char *prefix, dbref loc, struct format_msg *format);
void notify_except2(dbref executor, dbref first, dbref exc1, dbref exc2,
                    const char *msg, int flags);
/**< Notify all objects in a single location, with one exception */
#define notify_except(executor, loc, exc, msg, flags)                          \
  notify_except2(executor, loc, exc, NOTHING, msg, flags)

dbref na_one(dbref current, void *data);
dbref na_next(dbref current, void *data);
dbref na_loc(dbref current, void *data);
dbref na_channel(dbref current, void *data);

#define notify_flags(p, m, f)                                                  \
  notify_anything(orator, orator, na_one, &(p), NULL, f, m, NULL, AMBIGUOUS,   \
                  NULL)
#define raw_notify(p, m)                                                       \
  notify_anything(GOD, GOD, na_one, &(p), NULL, NA_NOLISTEN | NA_SPOOF, m,     \
                  NULL, AMBIGUOUS, NULL)

/**< Basic 'notify player with message */
#define notify(p, m) notify_flags(p, m, NA_SPOOF)
/**< Notify player as a prompt */
#define notify_prompt(p, m) notify_flags(p, m, NA_PROMPT | NA_SPOOF)
/**< Notify puppet with message, even if owner's there */
#define notify_must_puppet(p, m) notify_flags(p, m, NA_MUST_PUPPET | NA_SPOOF)
/**< Notify puppet with message as prompt, even if owner's there */
#define notify_prompt_must_puppet(p, m)                                        \
  notify_flags(p, m, NA_MUST_PUPPET | NA_PROMPT | NA_SPOOF)
/**< Notify player with message, as if from somethign specific */
#define notify_by(s, p, m)                                                     \
  notify_anything(s, s, na_one, &(p), NULL, NA_SPOOF, m, NULL, AMBIGUOUS, NULL)
/**< Notfy player with message, but only puppet propagation */
#define notify_noecho(p, m)                                                    \
  notify_flags(p, m, NA_NORELAY | NA_PUPPET_OK | NA_SPOOF)
/**< Notify player with message if they're not set QUIET */
#define quiet_notify(p, m)                                                     \
  if (!IsQuiet(p))                                                             \
  notify(p, m)
/**< Notify player but don't send \n */
#define notify_noenter_by(s, p, m)                                             \
  notify_anything(s, s, na_one, &(p), NULL, NA_NOENTER | NA_SPOOF, m, NULL,    \
                  AMBIGUOUS, NULL)
#define notify_noenter(p, m) notify_noenter_by(GOD, p, m)
/**< Notify player but don't send <BR> if they're using Pueblo */
#define notify_nopenter_by(s, p, m)                                            \
  notify_anything(s, s, na_one, &(p), NULL, NA_NOPENTER | NA_SPOOF, m, NULL,   \
                  AMBIGUOUS, NULL)
#define notify_nopenter(p, m) notify_nopenter_by(GOD, p, m)
/* Notify with a printf-style format */
void notify_format(dbref player, const char *fmt, ...)
  __attribute__((__format__(__printf__, 2, 3)));

#endif /* __NOTIFY_H */
