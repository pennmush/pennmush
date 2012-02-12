/**
 * \file notify.c
 *
 * \brief Notification of objects with messages, for PennMUSH.
 *
 * The functions in this file are primarily concerned with maintaining
 * queues of blocks of text to transmit to a player descriptor.
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#include <io.h>
#else                           /* !WIN32 */
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <sys/ioctl.h>
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#endif
#ifdef I_NETDB
#include <netdb.h>
#endif
#ifdef I_SYS_PARAM
#include <sys/param.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#endif                          /* !WIN32 */
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <limits.h>
#ifdef I_FLOATINGPOINT
#include <floatingpoint.h>
#endif

#include "conf.h"
#include "mushdb.h"
#include "externs.h"
#include "flags.h"
#include "dbdefs.h"
#include "lock.h"
#include "help.h"
#include "match.h"
#include "ansi.h"
#include "pueblo.h"
#include "parse.h"
#include "access.h"
#include "version.h"
#include "mysocket.h"
#include "strtree.h"
#include "log.h"
#include "mymalloc.h"

#include "extchat.h"
extern CHAN *channels;
#include "extmail.h"
#include "attrib.h"
#include "game.h"
#include "confmagic.h"


/* When the mush gets a new connection, it tries sending a telnet
 * option negotiation code for setting client-side line-editing mode
 * to it. If it gets a reply, a flag in the descriptor struct is
 * turned on indicated telnet-awareness.
 *
 * If the reply indicates that the client supports linemode, further
 * instructions as to what linemode options are to be used is sent.
 * Those options: Client-side line editing, and expanding literal
 * client-side-entered tabs into spaces.
 *
 * Option negotation requests sent by the client are processed,
 * with the only one we confirm rather than refuse outright being
 * suppress-go-ahead, since a number of telnet clients try it.
 *
 * The character 255 is the telnet option escape character, so when it
 * is sent to a telnet-aware client by itself (Since it's also often y-umlaut)
 * it must be doubled to escape it for the client. This is done automatically,
 * and is the original purpose of adding telnet option support.
 */

/* Telnet codes */
#define IAC 255                 /**< telnet: interpret as command */
#define GOAHEAD 249             /**< telnet: go-ahead */

/** Iterate through a list of descriptors, and do something with those
 * that are connected.
 */
#define DESC_ITER_CONN(d) \
        for(d = descriptor_list;(d);d=(d)->next) \
          if((d)->connected)

static const char *flushed_message = "\r\n<Output Flushed>\x1B[0m\r\n";

extern DESC *descriptor_list;
#ifdef WIN32
static WSADATA wsadata;
#endif

static struct text_block *make_text_block(const unsigned char *s, int n);
void free_text_block(struct text_block *t);
void add_to_queue(struct text_queue *q, const unsigned char *b, int n);
static int flush_queue(struct text_queue *q, int n);
int queue_write(DESC *d, const unsigned char *b, int n);
int queue_newwrite(DESC *d, const unsigned char *b, int n);
int queue_string(DESC *d, const char *s);
int queue_string_eol(DESC *d, const char *s);
int queue_eol(DESC *d);
void freeqs(DESC *d);
int process_output(DESC *d);

static int str_type(const char *str);
static int notify_type(DESC *d);
static int output_ansichange(ansi_data *states, int *ansi_ptr,
                             const unsigned char **ptr, char *buff, char **bp);

static int na_depth = 0; /**< Counter to prevent too much notify_anything recursion */

/** Complete list of possible groupings of MSG_* flags.
 * These are all the different kinds of messages we may produce to send to a
 * player. Note that we don't have any  MSG_PUEBLO | MSG_TELNET groups - the
 * Telnet char is always escaped for Pueblo clients.
 */
#define MSGTYPE_ORIGINAL MSG_INTERNAL

#define MSGTYPE_PASCII MSG_PLAYER

#define MSGTYPE_ANSI (MSG_PLAYER | MSG_ANSI)
#define MSGTYPE_PUEBLO (MSG_PLAYER | MSG_PUEBLO)
#define MSGTYPE_PUEBLOANSI (MSG_PLAYER | MSG_PUEBLO | MSG_ANSI)

#define MSGTYPE_TPASCII (MSG_PLAYER | MSG_TELNET)
#define MSGTYPE_TANSI (MSG_PLAYER | MSG_ANSI | MSG_TELNET)

#define MSGTYPE_NPASCII (MSG_PLAYER | MSG_STRIPACCENTS)
#define MSGTYPE_NANSI (MSG_PLAYER | MSG_ANSI | MSG_STRIPACCENTS)
#define MSGTYPE_NPUEBLO (MSG_PLAYER | MSG_PUEBLO | MSG_STRIPACCENTS)
#define MSGTYPE_NPUEBLOANSI (MSG_PLAYER | MSG_PUEBLO | MSG_ANSI | MSG_STRIPACCENTS)

#define MSGTYPE_TNPASCII (MSG_PLAYER | MSG_TELNET | MSG_STRIPACCENTS)
#define MSGTYPE_TNANSI (MSG_PLAYER | MSG_ANSI | MSG_TELNET | MSG_STRIPACCENTS)

/** Corresponding NA_* defines for each MSGTYPE_* with incrementing values */
enum na_type {
  NA_ORIGINAL = 0,    /**< String used internally. No chars stripped, internal markup */
  NA_PASCII,          /**< CRLF lineendings, accents, no ansi */
  NA_ANSI,            /**< CRLF lineendings, accents, ansi */
  NA_PUEBLO,          /**< Pueblo, no ansi */
  NA_PUEBLOANSI,      /**< Pueblo, with ansi */
  NA_TPASCII,         /**< CRLF lineendings, accents, no ansi, Telnet-aware */
  NA_TANSI,           /**< CRLF lineendings, accents, ansi, Telnet-aware */
  NA_NPASCII,         /**< CRLF lineendings, no accents, no ansi */
  NA_NANSI,           /**< CRLF lineendings, no accents, ansi */
  NA_NPUEBLO,         /**< Pueblo, no accents, no ansi */
  NA_NPUEBLOANSI,     /**< Pueblo, no accents, ansi */
  NA_TNPASCII,        /**< CRLF lineendings, no accents, no ansi, telnet-aware */
  NA_TNANSI           /**< CRLF lineendings, noaccents, ansi, telnet-aware */
};

static enum na_type msg_to_na(int output_type);

/** Number of possible message text renderings */
#define MESSAGE_TYPES 13

/** A place to store a single rendering of a message. */
struct notify_strings {
  unsigned char *message;  /**< The message text. */
  size_t len;              /**< Length of message. */
  int made;                /**< True if message has been rendered. */
};

/** A message, in every possible rendering */
struct notify_message {
  struct notify_strings strs[MESSAGE_TYPES];  /**< The message, in a bunch of formats */
  int type; /**< MSG_* flags for the types of chars possibly present in the original string */
};

/** Every possible rendering of a message, plus the nospoof and paranoid prefixes */
struct notify_message_group {
  struct notify_message messages;  /**< Message being notified */
  struct notify_message nospoofs;  /**< Non-paranoid Nospoof prefix */
  struct notify_message paranoids; /**< Paranoid Nospoof prefix */
};

static void init_notify_message_group(struct notify_message_group
                                      *real_message);
static void notify_anything_sub(dbref speaker, na_lookup func, void *fdata,
                                dbref *skips, int flags,
                                struct notify_message_group *message,
                                const char *prefix, dbref loc,
                                struct format_msg *format);

static void notify_internal(dbref target, dbref speaker, dbref *skips,
                            int flags, struct notify_message_group *message,
                            struct notify_message *prefix, dbref loc,
                            struct format_msg *format);
static unsigned char *make_nospoof(dbref speaker, int paranoid);
static void make_prefix_str(dbref thing, dbref enactor, const char *msg,
                            char *tbuf1);

static unsigned char *notify_makestring(struct notify_message *message,
                                        int output_type);
static unsigned char *notify_makestring_nocache(unsigned char *message,
                                                int output_type);

/** Check which kinds of markup or special characters a string may contain.
 * This is used to avoid generating message types we don't need. For
 * instance, if a string doesn't contain any ANSI, we don't need to
 * waste time and memory creating a seperate copy for ANSI-aware players,
 * since it won't look any different to the copy created for non-ANSI
 * players.
 * \param str the string to check
 * \return a bitwise int of MSG_* flags possibly required for the msg
 */
#define CHECK_FOR_HTML
static int
str_type(const char *str)
{
  int type = MSG_ALL_PLAYER;
#ifdef CHECK_FOR_HTML
  char *p;
  type = (MSG_ALL_PLAYER & ~(MSG_PUEBLO | MSG_STRIPACCENTS));
#endif                          /* CHECK_FOR_HTML */

  if (strstr(str, MARKUP_START "c") == NULL) {
    /* No ANSI */
    type &= ~MSG_ANSI;
  }
#ifdef CHECK_FOR_HTML

  /* I'm not sure whether checking for HTML entities/accented characters
   * here will cost more time than it saves later (in not duplicating
   * unnecessary additional string renderings). But here's the code,
   * just in case.
   */
  p = (char *) str;
  while (*p) {
    if (*p == '\n')
      type |= MSG_PUEBLO;
    else if (accent_table[(unsigned char) *p].base) {
      type |= MSG_PUEBLO | MSG_STRIPACCENTS;
      break;
    }
    p++;
  }
  if (!(type & MSG_PUEBLO) && strstr(str, MARKUP_START "p") != NULL)
    type |= MSG_PUEBLO;

#endif                          /* CHECK_FOR_HTML */

  if (strchr(str, IAC) == NULL) {
    /* No Telnet IAC chars to be escaped */
    type &= ~MSG_TELNET;
  }

  /* There's no point checking for \n and removing MSG_PLAYER - we'll never
   * be caching values without MSG_PLAYER anyway */

  return type;
}

/** Bitwise MSG_* flags of the type of message to send to a particular descriptor.
 * Used by notify_makestring() to make a suitable string to send to the player.
 * \param d descriptor to check
 * \return bitwise MSG_* flags giving the type of message to send
 */
static int
notify_type(DESC *d)
{
  int type = MSG_PLAYER;

  if (!d->connected) {
    /* These are the settings used at, e.g., the connect screen,
     * when there's no connected player yet.
     */
    type |= MSG_ANSI;
    if (d->conn_flags & CONN_HTML)
      type |= MSG_PUEBLO;
    else if (d->conn_flags & CONN_TELNET)
      type |= MSG_TELNET;
    return type;
  }

  /* At this point, we have a connected player on the descriptor */
  if (IS(d->player, TYPE_PLAYER, "NOACCENTS"))
    type |= MSG_STRIPACCENTS;

  if (d->conn_flags & CONN_HTML) {
    type |= MSG_PUEBLO;
  } else if (d->conn_flags & CONN_TELNET) {
    type |= MSG_TELNET;
  }

  if (ShowAnsi(d->player)) {
    type |= MSG_ANSI;
  }

  return type;
}

/** output the appropriate raw ansi tags when markup is found in a string.
 * Used by render_string().
 * \param states
 * \param ansi_ptr
 * \param ptr
 * \param buff
 * \param bp
 * \return 0 if data was written successfully, 1 on failure
 */
static int
output_ansichange(ansi_data *states, int *ansi_ptr,
                  const unsigned char **ptr, char *buff, char **bp)
{
  const unsigned char *p = *ptr;
  int newaptr = *ansi_ptr;
  int retval = 0;
  ansi_data cur = states[*ansi_ptr];

  /* This is color */
  while (*p &&
         ((*p == TAG_START && *(p + 1) == MARKUP_COLOR) || (*p == ESC_CHAR))) {
    switch (*p) {
    case TAG_START:
      p += 2;
      if (*p != '/') {
        newaptr++;
        define_ansi_data(&(states[newaptr]), (const char *) p);
      } else {
        if (*(p + 1) == 'a') {
          newaptr = 0;
        } else {
          if (newaptr > 0)
            newaptr--;
        }
      }
      while (*p && *p != TAG_END)
        p++;
      break;
    case ESC_CHAR:
      newaptr++;
      read_raw_ansi_data(&states[newaptr], (const char *) p);
      while (*p && *p != 'm')
        p++;
      break;
    }
    if (newaptr > 0)
      nest_ansi_data(&(states[newaptr - 1]), &(states[newaptr]));
    /* Advance past the tag ending, if there's more. */
    if (*p && ((*(p + 1) == TAG_START && *(p + 2) == MARKUP_COLOR) ||
               (*(p + 1) == ESC_CHAR)))
      p++;
  }
  /* Do we print anything? */
  if (*p && *ptr != p) {
    retval = write_raw_ansi_data(&cur, &(states[newaptr]), buff, bp);
    *(ansi_ptr) = newaptr;
  }
  *ptr = p;
  return retval;
}

/*--------------------------------------------------------------
 * Iterators for notify_anything.
 * notify_anything calls these functions repeatedly to get the
 * next object to notify, passing in the last object notified.
 * On the first pass, it passes in NOTHING. When it finally
 * receives NOTHING back, it stops.
 */

/** notify_anything() iterator for a single dbref.
 * \param current last dbref from iterator.
 * \param data memory address containing first object in chain.
 * \return dbref of next object to notify, or NOTHING when done.
 */
dbref
na_one(dbref current, void *data)
{
  if (current == NOTHING)
    return *((dbref *) data);
  else
    return NOTHING;
}

/** notify_anything() iterator for following a contents/exit chain.
 * \param current last dbref from iterator.
 * \param data memory address containing first object in chain.
 * \return dbref of next object to notify, or NOTHING when done.
 */
dbref
na_next(dbref current, void *data)
{
  if (current == NOTHING)
    return *((dbref *) data);
  else
    return Next(current);
}

/** notify_anything() iterator for a location and its contents.
 * \param current last dbref from iterator.
 * \param data memory address containing dbref of location.
 * \return dbref of next object to notify, or NOTHING when done.
 */
dbref
na_loc(dbref current, void *data)
{
  dbref loc = *((dbref *) data);
  if (current == NOTHING)
    return loc;
  else if (current == loc)
    return Contents(current);
  else
    return Next(current);
}

/** Initialize a notify_message_group with NULL/zero values
 */
static void
init_notify_message_group(struct notify_message_group *real_message)
{
  int i;

  for (i = 0; i < MESSAGE_TYPES; i++) {
    real_message->messages.strs[i].message = NULL;
    real_message->messages.strs[i].made = 0;
    real_message->messages.strs[i].len = 0;

    real_message->nospoofs.strs[i].message = NULL;
    real_message->nospoofs.strs[i].made = 0;
    real_message->nospoofs.strs[i].len = 0;

    real_message->paranoids.strs[i].message = NULL;
    real_message->paranoids.strs[i].made = 0;
    real_message->paranoids.strs[i].len = 0;
  }
  real_message->messages.type = 0;
  real_message->nospoofs.type = 0;
  real_message->paranoids.type = 0;
}

/** Evaluate an object's @prefix and store the result in a buffer.
 * If the attribute doesn't exist, a default prefix is used.
 * \param thing object with prefix attribute.
 * \param enactor object causing the evaluation
 * \param msg message.
 * \param tbuf1 destination buffer.
 */
static void
make_prefix_str(dbref thing, dbref enactor, const char *msg, char *tbuf1)
{
  char *bp;
  PE_REGS *pe_regs;
  tbuf1[0] = '\0';

  pe_regs = pe_regs_create(PE_REGS_ARG, "make_prefix_str");
  pe_regs_setenv_nocopy(pe_regs, 0, msg);

  if (!call_attrib(thing, "PREFIX", tbuf1, enactor, NULL, pe_regs)
      || *tbuf1 == '\0') {
    bp = tbuf1;
    safe_format(tbuf1, &bp, T("From %s, "),
                Name(IsExit(thing) ? Source(thing) : thing));
    *bp = '\0';
  } else {
    bp = strchr(tbuf1, '\0');
    safe_chr(' ', tbuf1, &bp);
    *bp = '\0';
  }
  pe_regs_free(pe_regs);

  return;
}

/** Return the appropriate NA_* flag for a bitwise group of MSG_* flags */
static enum na_type
msg_to_na(int output_type)
{
  switch (output_type) {
  case MSGTYPE_ORIGINAL:
    return NA_ORIGINAL;
  case MSGTYPE_PASCII:
    return NA_PASCII;
  case MSGTYPE_ANSI:
    return NA_ANSI;
  case MSGTYPE_PUEBLO:
  case (MSGTYPE_PUEBLO | MSG_TELNET):
    return NA_PUEBLO;
  case MSGTYPE_PUEBLOANSI:
  case (MSGTYPE_PUEBLOANSI | MSG_TELNET):
    return NA_PUEBLOANSI;
  case MSGTYPE_TPASCII:
    return NA_TPASCII;
  case MSGTYPE_TANSI:
    return NA_TANSI;
  case MSGTYPE_NPASCII:
    return NA_NPASCII;
  case MSGTYPE_NANSI:
    return NA_NANSI;
  case MSGTYPE_NPUEBLO:
  case (MSGTYPE_NPUEBLO | MSG_TELNET):
    return NA_NPUEBLO;
  case MSGTYPE_NPUEBLOANSI:
  case (MSGTYPE_NPUEBLOANSI | MSG_TELNET):
    return NA_NPUEBLOANSI;
  case MSGTYPE_TNPASCII:
    return NA_TNPASCII;
  case MSGTYPE_TNANSI:
    return NA_TNANSI;
  }

  /* we should never get here. */
  do_rawlog(LT_ERR, "Invalid MSG_* flag setting '%d' in msg_to_na",
            output_type);
  return NA_PASCII;
}

/** Make a nospoof prefix for speaker, possibly for paranoid nospoof
 * \param speaker the object speaking
 * \param paranoid make a paranoid nospoof prefix, instead of regular nospoof?
 * \return pointer to nospoof prefix
 */
static unsigned char *
make_nospoof(dbref speaker, int paranoid)
{
  char *dest, *bp;
  bp = dest = mush_malloc(BUFFER_LEN, "notify_str");

  if (!GoodObject(speaker))
    *dest = '\0';
  else if (paranoid) {
    if (speaker == Owner(speaker))
      safe_format(dest, &bp, "[%s(#%d)] ", Name(speaker), speaker);
    else
      safe_format(dest, &bp, T("[%s(#%d)'s %s(#%d)] "), Name(Owner(speaker)),
                  Owner(speaker), Name(speaker), speaker);
  } else
    safe_format(dest, &bp, "[%s:] ", spname(speaker));
  *bp = '\0';
  return (unsigned char *) dest;
}

/** Render a string to the given format. Returns pointer to a STATIC buffer.
 * Used by notify_makestring() to render a string for output to a player's
 * client, and by the softcode render() function.
 * \param message the string to render
 * \param output_type bitwise MSG_* flags for how to render the message
 * \return pointer to static string
 */
unsigned char *
render_string(unsigned char *message, int output_type)
{
  static char buff[BUFFER_LEN];
  static char *bp;
  const unsigned char *p;

  static ansi_data states[BUFFER_LEN];
  int ansi_ptr, ansifix;
  ansi_ptr = 0;
  ansifix = 0;

  /* Everything is explicitly off by default */
  states[0].bits = 0;
  states[0].offbits = 0;
  states[0].fore = 0;
  states[0].back = 0;

  bp = buff;

  if (output_type == MSG_INTERNAL) {
    strcpy(buff, (char *) message);
    return (unsigned char *) buff;
  }

  for (p = message; *p; p++) {
    switch (*p) {
    case TAG_START:
      if (*(p + 1) == MARKUP_COLOR) {
        /* ANSI colors */
        if ((output_type & MSG_ANSI)) {
          /* Translate internal markup to ANSI tags */
          ansifix += output_ansichange(states, &ansi_ptr, &p, buff, &bp);
        } else if (output_type & MSG_MARKUP) {
          /* Preserve internal markup */
          while (*p && *p != TAG_END) {
            safe_chr(*p, buff, &bp);
            p++;
          }
          safe_chr(TAG_END, buff, &bp);
        } else {
          /* Strip ANSI */
          while (*p && *p != TAG_END)
            p++;
        }
      } else if (*(p + 1) == MARKUP_HTML) {
        /* Pueblo markup */
        if (output_type & MSG_PUEBLO) {
          /* Output as HTML */
          safe_chr('<', buff, &bp);
          /* Skip over TAG_START and MARKUP_HTML */
          p += 2;
          while (*p && *p != TAG_END) {
            safe_chr(*p, buff, &bp);
            p++;
          }
          safe_chr('>', buff, &bp);
        } else if (output_type & MSG_MARKUP) {
          /* Preserve internal markup */
          while (*p && *p != TAG_END) {
            safe_chr(*p, buff, &bp);
            p++;
          }
          safe_chr(TAG_END, buff, &bp);
        } else {
          /* Strip */
          while (*p && *p != TAG_END)
            p++;
        }
      } else {
        /* Unknown markup type; strip */
        while (*p && *p != TAG_END)
          p++;
      }
      break;
    case TAG_END:
      if (output_type & MSG_MARKUP)
        safe_chr(*p, buff, &bp);
      break;                    /* Skip over TAG_ENDs */
    case ESC_CHAR:
      /* After the ansi changes, I really hope we don't encounter this. */
      if ((output_type & MSG_ANSI)) {
        ansifix += output_ansichange(states, &ansi_ptr, &p, buff, &bp);
      } else {
        /* Skip over tag */
        while (*p && *p != 'm')
          p++;
      }
      break;
    case '\r':
      break;
    case IAC:
      if (output_type & MSG_STRIPACCENTS) {
        safe_str(accent_table[*p].base, buff, &bp);
      } else if (output_type & MSG_PUEBLO) {
        safe_str(accent_table[*p].entity, buff, &bp);
      } else if (output_type & MSG_TELNET) {
        safe_strl("\xFF\xFF", 2, buff, &bp);
      } else {
        safe_chr(*p, buff, &bp);
      }
      break;
    default:
      if (output_type & MSG_PUEBLO) {
        if (output_type & MSG_STRIPACCENTS) {
          switch (*p) {
          case '\n':
          case '&':
          case '<':
          case '>':
          case '"':
            safe_str(accent_table[*p].entity, buff, &bp);
            break;
          default:
            if (accent_table[*p].base) {
              safe_str(accent_table[*p].base, buff, &bp);
            } else {
              safe_chr(*p, buff, &bp);
            }
            break;
          }
        } else if (accent_table[*p].entity) {
          safe_str(accent_table[*p].entity, buff, &bp);
        } else {
          safe_chr(*p, buff, &bp);
        }
      } else if (*p == '\n' && (output_type & MSG_PLAYER)) {
        safe_strl("\r\n", 2, buff, &bp);
      } else if (output_type & MSG_STRIPACCENTS && accent_table[*p].base) {
        safe_str(accent_table[*p].base, buff, &bp);
      } else {
        safe_chr(*p, buff, &bp);
      }
      break;
    }
  }

  *bp = '\0';

  /* We possibly have some unclosed ansi. Force an
   * ANSI_NORMAL for now. */
  if (ansifix || (ansi_ptr && safe_str(ANSI_RAW_NORMAL, buff, &bp))) {
    int sub = 7;
    char *ptr;
    int q;

    ptr = buff + BUFFER_LEN - sub;
    for (q = 20; q > 0 && *ptr != ESC_CHAR; q--, ptr--) ;
    if (output_type & MSG_PUEBLO) {
      for (q = 20; q > 0 && *ptr != ESC_CHAR && *ptr != '<'; q--, ptr--) ;
    } else {
      for (q = 20; q > 0 && *ptr != ESC_CHAR; q--, ptr--) ;
    }
    if (q > 0) {
      bp = ptr;
    } else {
      bp = buff + BUFFER_LEN - sub;
    }
    safe_str(ANSI_RAW_NORMAL, buff, &bp);
    *bp = '\0';
  }

  *bp = '\0';

  return (unsigned char *) buff;

}

/** Render a message into a given format, if we haven't already done so, and cache the result.
 * If we've already cached the string in the requested format, return that.
 * Otherwise, render it, cache and return the newly cached version. Calls
 * render_string() to actually do the rendering, and strdup()s the result.
 * \param message a notify_message structure, with the original message and cached copies
 * \param output_type MSG_* flags of how to render the message
 * \return pointer to the cached, rendered string
 */
static unsigned char *
notify_makestring(struct notify_message *message, int output_type)
{
  enum na_type msgtype;
  unsigned char *newstr;

  if (output_type & MSG_PLAYER)
    output_type = (output_type & (message->type | MSG_PLAYER));

  msgtype = msg_to_na(output_type);

  if (message->strs[msgtype].made) {
    return message->strs[msgtype].message;
  }

  /* Render the message */
  newstr = render_string(message->strs[0].message, output_type);

  /* Save the new message */
  message->strs[msgtype].made = 1;
  message->strs[msgtype].message =
    (unsigned char *) mush_strdup((char *) newstr, "notify_str");
  message->strs[msgtype].len = u_strlen(newstr);

  return message->strs[msgtype].message;

}

/** Render a message in a given format and return the new message.
 * Does not cache the results like notify_makestring() - used for messages
 * which have been formatted through a ufun, and are thus different for
 * every object which hears them.
 * \param message the message to render
 * \param output_type MSG_* flags of how to render the msg
 * \return pointer to the newly rendered, strdup()'d string
 */
static unsigned char *
notify_makestring_nocache(unsigned char *message, int output_type)
{
  return (unsigned char *) mush_strdup((char *)
                                       render_string(message, output_type),
                                       "notify_str");
}

/* notify_except() is #define'd to notify_except2() */

/** Notify all objects in a location, except 2, and propagate the sound.
 * \param loc where to emit the sound
 * \param exc1 first object to not notify
 * \param exc2 second object to not notify, or NOTHING
 * \param msg the message to send
 * \param flags NA_* flags
 */
void
notify_except2(dbref loc, dbref exc1, dbref exc2, const char *msg, int flags)
{
  dbref skips[3];

  if (exc1 == NOTHING)
    exc1 = exc2;

  skips[0] = exc1;
  skips[1] = exc2;
  skips[2] = NOTHING;

  notify_anything(orator, na_loc, &loc, (exc1 == NOTHING) ? NULL : skips,
                  flags | NA_PROPAGATE, msg, NULL, loc, NULL);

}

/** Public function to notify one or more objects with a message.
 * This function is a wrapper around notify_anything_sub, which prepares a char*
 * message into a notify_message_group struct.
 * \param speaker the object which created the message
 * \param func lookup function to figure out who to tell
 * \param fdata data to pass to func
 * \param skips pointer to an array of dbrefs not to notify, or NULL
 * \param flags NA_* flags to limit/modify how the message is sent
 * \param message the message to send
 * \param prefix a prefix to show before the message, or NULL
 * \param loc where the sound is coming from, or AMBIGUOUS to use speaker's loc
 * \param format a format_msg structure (obj/attr/args) to ufun to generate the message
 */
void
notify_anything(dbref speaker, na_lookup func, void *fdata, dbref *skips,
                int flags, const char *message, const char *prefix, dbref loc,
                struct format_msg *format)
{
  struct notify_message_group real_message;
  int i;

  /* If we have no message, or noone to notify, do nothing */
  if (!message || !*message || !func)
    return;

  /* Don't recurse too much */
  if (na_depth > 7)
    return;

  /* Do it */
  init_notify_message_group(&real_message);
  real_message.messages.strs[0].message = (unsigned char *) message;
  real_message.messages.strs[0].made = 1;
  real_message.messages.strs[0].len = strlen(message);
  real_message.messages.type = str_type(message);

  if (loc == AMBIGUOUS)
    loc = speech_loc(speaker);

  notify_anything_sub(speaker, func, fdata, skips, flags, &real_message, prefix,
                      loc, format);

  /* Cleanup */
  for (i = 0; i < MESSAGE_TYPES; i++) {
    if (i && real_message.messages.strs[i].made)
      mush_free(real_message.messages.strs[i].message, "notify_str");
    if (real_message.nospoofs.strs[i].made)
      mush_free(real_message.nospoofs.strs[i].message, "notify_str");
    if (real_message.paranoids.strs[i].made)
      mush_free(real_message.paranoids.strs[i].message, "notify_str");
  }

}


/** Notify one or more objects with a message.
 * Calls an na_lookup func to figure out which objects to notify and, if the
 * object isn't in 'skips', calls notify_internal to send the message.
 * \param speaker the object which created the message
 * \param func lookup function to figure out who to tell
 * \param fdata data to pass to func
 * \param skips pointer to an array of dbrefs not to notify, or NULL
 * \param flags NA_* flags to limit/modify how the message is sent
 * \param message the message to send
 * \param prefix a prefix to show before the message, or NULL
 * \param loc where the sound is coming from
 * \param format a format_msg structure (obj/attr/args) to ufun to generate the message
 */
static void
notify_anything_sub(dbref speaker, na_lookup func, void *fdata, dbref *skips,
                    int flags, struct notify_message_group *message,
                    const char *prefix, dbref loc, struct format_msg *format)
{
  dbref target = NOTHING;
  struct notify_message *real_prefix = NULL;

  /* Make sure we have a message and someone to tell */
  if (!message || !func)
    return;

  /* Don't recurse too much */
  if (na_depth > 7)
    return;

  na_depth++;
  if (prefix && *prefix) {
    int i;

    real_prefix = mush_malloc(sizeof(struct notify_message), "notify_message");
    real_prefix->strs[0].message = (unsigned char *) prefix;
    real_prefix->strs[0].made = 1;
    real_prefix->strs[0].len = strlen(prefix);
    real_prefix->type = str_type(prefix);
    for (i = 1; i < MESSAGE_TYPES; i++) {
      real_prefix->strs[i].message = NULL;
      real_prefix->strs[i].made = 0;
      real_prefix->strs[i].len = 0;
    }

  }
  /* Tell everyone */
  while ((target = func(target, fdata)) != NOTHING) {
    if (IsExit(target))
      continue;                 /* Exits can't hear anything directly */
    if (skips != NULL) {
      int i;
      for (i = 0; skips[i] != NOTHING && skips[i] != target; i++) ;

      if (skips[i] != NOTHING)
        continue;
    }
    notify_internal(target, speaker, skips, flags, message, real_prefix, loc,
                    format);
  }

  if (real_prefix != NULL) {
    int i;

    for (i = 1; i < MESSAGE_TYPES; i++) {
      if (real_prefix->strs[i].made)
        mush_free(real_prefix->strs[i].message, "notify_str");
    }
    mush_free(real_prefix, "notify_message");
  }

  na_depth--;

}

#define PUPPET_FLAGS(na_flags)  ((na_flags | NA_PUPPET_MSG | NA_NORELAY) & ~NA_PROMPT)
#define RELAY_FLAGS(na_flags)  ((na_flags | NA_PUPPET_OK | NA_NORELAY) & ~NA_PROMPT)


/** Notify a single object with a message. May recurse by calling itself or
 * notify_anything[_sub] to do more notifications, for puppet, \@forwardlist,
 * or to propagate the sound into an object's contents or through an audible
 * exit. If format is non-NULL, all messages are passed through obj/attr with
 * the given args prior to being displayed to a player, matched against
 * \@listens or sent to a puppet (but NOT propagated to other locations).
 * If format->obj is ambiguous (#-2), get the attr from the target, otherwise
 * use the obj given. Transformed strings are not cached.
 * \param target object to notify
 * \param speaker object creating the sound
 * \param skips array of dbrefs not to notify when propagating sound, or NULL
 * \param flags NA_* flags for how to generate the sound
 * \param message the message, with nospoof and paranoid prefixes
 * \param prefix a prefix to show before the message
 * \param loc the location the sound is generated
 * \param format an obj/attr/args to format the message with
 */
static void
notify_internal(dbref target, dbref speaker, dbref *skips, int flags,
                struct notify_message_group *message,
                struct notify_message *prefix, dbref loc,
                struct format_msg *format)
{
  int output_type = MSG_INTERNAL; /**< The way to render the message for the current target/descriptor */
  int last_output_type = -1; /**< For players, the way the msg was rendered for the previous descriptor */
  unsigned char *spoofstr = NULL; /**< Pointer to the rendered nospoof prefix to use */
  int spooflen = 0; /**< Length of the rendered nospoof prefix */
  unsigned char *msgstr = NULL; /**< Pointer to the rendered message */
  int msglen = 0; /**< Length of the rendered message */
  unsigned char *prefixstr = NULL;
  int prefixlen = 0;
  static char buff[BUFFER_LEN], *bp; /**< Buffer used for processing the format attr */
  unsigned char *formatmsg = NULL; /**< Pointer to the rendered, formatted message. Must be free()d! */
  int cache = 1; /**< Are we using a cached version of the message? */
  int prompt = 0; /**< Show a prompt? */
  int heard = 1; /**< After formatting, did this object hear something? */
  DESC *d; /**< descriptor to loop through connected players */
  int listen_lock_checked = 0, listen_lock_passed = 0; /**< Has the Listen \@lock been checked/passed? */
  ATTR *a; /**< attr pointer, for \@listen and \@infilter */

  /* Check interact locks */
  if (flags & NA_INTERACTION) {
    if ((flags & NA_INTER_SEE)
        && !can_interact(speaker, target, INTERACT_SEE, NULL))
      return;
    if ((flags & NA_INTER_PRESENCE) &&
        !can_interact(speaker, target, INTERACT_PRESENCE, NULL))
      return;
    if ((flags & NA_INTER_HEAR) &&
        !can_interact(speaker, target, INTERACT_HEAR, NULL))
      return;
    if ((flags & NA_INTER_LOCK) && !Pass_Interact_Lock(speaker, target, NULL))
      return;
  }

  /* At this point, the message can definitely be heard by the object, so we need to figure out
   * the correct message it should hear, possibly formatted through a ufun */
  if (format != NULL
      && (format->thing == AMBIGUOUS || RealGoodObject(format->thing))
      && format->attr && *format->attr) {
    /* Format the message through a ufun */
    ufun_attrib ufun;
    dbref src;

    if (format->thing == AMBIGUOUS) {
      src = target;
    } else {
      src = format->thing;
    }
    bp = buff;
    safe_dbref(src, buff, &bp);
    safe_chr('/', buff, &bp);
    safe_str(format->attr, buff, &bp);
    *bp = '\0';

    if (fetch_ufun_attrib
        (buff, speaker, &ufun,
         (UFUN_OBJECT | UFUN_REQUIRE_ATTR |
          (format->checkprivs ? 0 : UFUN_IGNORE_PERMS)))) {
      PE_REGS *pe_regs = NULL;
      int i;

      cache = 0;
      if (format->numargs || (format->targetarg >= 0 && format->targetarg < 10)) {
        pe_regs = pe_regs_create(PE_REGS_ARG, "notify_internal");
        for (i = 0; i < format->numargs && i < 10; i++) {
          pe_regs_setenv_nocopy(pe_regs, i, format->args[i]);
        }
        if (format->targetarg >= 0 && format->targetarg < 10)
          pe_regs_setenv(pe_regs, format->targetarg, unparse_dbref(target));
      }

      call_ufun(&ufun, buff, src, speaker, NULL, pe_regs);
      if (pe_regs)
        pe_regs_free(pe_regs);

      /* Even if the format attr returns nothing, we must continue because the
       * sound must still be propagated to other objects, which may hear
       * something. We just don't display sound to the object or trigger
       * its listen patterns, etc */
      if (!*buff) {
        heard = 0;
      }
    }
  }


  if (IsPlayer(target)) {
    /* Make sure the player is connected, and we have something to show him */
    if (Connected(target) && (heard || (flags & NA_PROMPT))) {
      /* Send text to the player's descriptors */
      for (d = descriptor_list; d; d = d->next) {
        if (!d->connected || d->player != target)
          continue;
        output_type = notify_type(d);

        if (heard && prefix != NULL) {
          /* Figure out */
          if (!prefixstr || output_type != last_output_type) {
            prefixstr = notify_makestring(prefix, output_type);
            prefixlen = u_strlen(prefixstr);
          }
        } else {
          prefixlen = 0;
        }

        /* Figure out if the player needs to see a Nospoof prefix */
        if (heard && !(flags & NA_SPOOF)
            && ((flags & NA_NOSPOOF) || (Nospoof(target)
                                         && ((target != speaker)
                                             || Paranoid(target))))) {
          if (Paranoid(target) || (flags & NA_PARANOID)) {
            if (!message->paranoids.strs[0].made) {
              message->paranoids.strs[0].message = make_nospoof(speaker, 1);
              message->paranoids.strs[0].made = 1;
              message->paranoids.strs[0].len =
                u_strlen(message->paranoids.strs[0].message);
              message->paranoids.type =
                str_type((const char *) message->paranoids.strs[0].message);
            }
            spoofstr = notify_makestring(&message->paranoids, output_type);
            spooflen = u_strlen(spoofstr);
          } else {
            if (!message->nospoofs.strs[0].made) {
              message->nospoofs.strs[0].message = make_nospoof(speaker, 0);
              message->nospoofs.strs[0].made = 1;
              message->nospoofs.strs[0].len =
                u_strlen(message->nospoofs.strs[0].message);
              message->nospoofs.type =
                str_type((const char *) message->nospoofs.strs[0].message);
            }
            spoofstr = notify_makestring(&message->nospoofs, output_type);
            spooflen = u_strlen(spoofstr);
          }
        } else {
          spooflen = 0;
        }

        /* No point re-rendering this string if we're outputting to an identical client */
        if (heard) {
          if (!msgstr || output_type != last_output_type) {
            if (cache) {
              msgstr = notify_makestring(&message->messages, output_type);
            } else {
              if (formatmsg)
                mush_free(formatmsg, "notify_str");
              msgstr = formatmsg =
                notify_makestring_nocache((unsigned char *) buff, output_type);
            }
            msglen = u_strlen(msgstr);
          }
          last_output_type = output_type;

          if (msglen) {
            if (prefixlen)      /* send prefix */
              queue_newwrite(d, prefixstr, prefixlen);
            if (spooflen)       /* send nospoof prefix */
              queue_newwrite(d, spoofstr, spooflen);
            queue_newwrite(d, msgstr, msglen);  /* send message */
          }
        }

        prompt = ((flags & NA_PROMPT) && (d->conn_flags & CONN_TELNET));
        if (prompt) {           /* send prompt */
          queue_newwrite(d, (unsigned char *) "\xFF\xF9", 2);
        }

        if ((!(flags & NA_NOENTER) && msglen && heard && !prompt)
            || (prompt && (d->conn_flags & CONN_PROMPT_NEWLINES))) {
          /* send lineending */
          if ((output_type & MSG_PUEBLO)) {
            if (flags & NA_NOPENTER)
              queue_newwrite(d, (unsigned char *) "\n", 1);
            else
              queue_newwrite(d, (unsigned char *) "<BR>\n", 5);
          } else {
            queue_newwrite(d, (unsigned char *) "\r\n", 2);
          }
        }
      }                         /* for loop */
      if (formatmsg) {
        mush_free(formatmsg, "notify_str");
        formatmsg = NULL;
      }
    }                           /* Connected(target) */
  } else if (heard && Puppet(target)
             && ((flags & NA_MUST_PUPPET) || Verbose(target)
                 || (Location(target) != Location(Owner(target))))
             && ((flags & NA_PUPPET_OK) || !(flags & NA_NORELAY))) {
    /* Puppet */
    int nospoof_flags = 0;
    char puppref[BUFFER_LEN];
    char *pp = puppref;
    safe_str(Name(target), puppref, &pp);
    safe_str("> ", puppref, &pp);
    *pp = '\0';

    /* Show "Puppet> " prompt */
    notify_anything(speaker, na_one, &Owner(target), NULL,
                    PUPPET_FLAGS(flags) | NA_SPOOF | NA_NOENTER, puppref, NULL,
                    loc, NULL);

    if (Nospoof(target)) {
      nospoof_flags |= NA_NOSPOOF;
      if (Paranoid(target))
        nospoof_flags |= NA_PARANOID;
    }

    /* And the message. If the puppet's message wasn't formatted through a
     * ufun, use the already-generated cached version of the message to save
     * time/memory. Otherwise, use the specific, formatted message the
     * puppet saw */
    if (cache) {
      notify_internal(Owner(target), speaker, NULL,
                      PUPPET_FLAGS(flags) | nospoof_flags, message, prefix, loc,
                      NULL);
    } else {
      notify_anything(speaker, na_one, &Owner(target), NULL,
                      PUPPET_FLAGS(flags) | nospoof_flags, buff,
                      (prefix ? (char *) prefix->strs[0].message : NULL), loc,
                      NULL);
    }
  }

  if ((flags & NA_PROPAGATE)
      || (!(flags & NA_NOLISTEN) && (PLAYER_LISTEN || !IsPlayer(target))
          && !IsExit(target))) {
    char *fullmsg, *fp = NULL;

    /* Prompts aren't propagated */
    flags &= ~NA_PROMPT;

    /* Figure out which message to use for listens */
    if (cache)
      msgstr = notify_makestring(&message->messages, MSG_INTERNAL);
    else
      msgstr = formatmsg =
        notify_makestring_nocache((unsigned char *) buff, MSG_INTERNAL);

    if (prefix) {
      /* Add the prefix to the beginning */
      fullmsg = mush_malloc(BUFFER_LEN, "notify_str");
      fp = fullmsg;
      safe_str((char *) notify_makestring(prefix, MSG_INTERNAL), fullmsg, &fp);
      safe_str((char *) msgstr, fullmsg, &fp);
      *fp = '\0';
    } else {
      fullmsg = (char *) msgstr;
    }

    if (heard) {
      /* Check @listen */
      a = atr_get_noparent(target, "LISTEN");
      if (a) {
        char match_space[BUFFER_LEN * 2];
        ssize_t match_space_len = BUFFER_LEN * 2;
        char *lenv[10];
        char *atrval;

        atrval = safe_atr_value(a);

        if (AF_Regexp(a)
            ? regexp_match_case_r(atrval, fullmsg,
                                  AF_Case(a), lenv, 10,
                                  match_space, match_space_len, NULL)
            : wild_match_case_r(atrval, fullmsg,
                                AF_Case(a), lenv, 10,
                                match_space, match_space_len, NULL)) {
          if (!listen_lock_checked)
            listen_lock_passed = eval_lock(speaker, target, Listen_Lock);
          if (listen_lock_passed) {
            int i;
            PE_REGS *pe_regs;

            pe_regs = pe_regs_create(PE_REGS_ARG, "notify");
            for (i = 0; i < 10; i++) {
              if (lenv[i]) {
                pe_regs_setenv_nocopy(pe_regs, i, lenv[i]);
              }
            }
            if (PLAYER_AHEAR || (!IsPlayer(target))) {
              if (speaker != target)
                queue_attribute_base(target, "AHEAR", speaker, 0, pe_regs, 0);
              else
                queue_attribute_base(target, "AMHEAR", speaker, 0, pe_regs, 0);
              queue_attribute_base(target, "AAHEAR", speaker, 0, pe_regs, 0);
            }
            pe_regs_free(pe_regs);
          }
          free(atrval);

          if (!(flags & NA_NORELAY) && (loc != target) &&
              Contents(target) != NOTHING
              && !filter_found(target, speaker, fullmsg, 1)) {
            /* Forward the sound to the object's contents */
            char inprefix[BUFFER_LEN];

            a = atr_get(target, "INPREFIX");
            if (a) {
              char *ip;
              PE_REGS *pe_regs = pe_regs_create(PE_REGS_ARG, "notify");

              pe_regs_setenv_nocopy(pe_regs, 0, (char *) msgstr);
              if (call_attrib
                  (target, "INPREFIX", inprefix, speaker, NULL, pe_regs)) {
                ip = strchr(inprefix, '\0');
                safe_chr(' ', inprefix, &ip);
                *ip = '\0';
              }
              pe_regs_free(pe_regs);
            }
            notify_anything_sub(speaker, na_next,
                                &Contents(target), skips,
                                RELAY_FLAGS(flags), message,
                                (a) ? inprefix : NULL, loc, format);
          }
        }
      }

      /* if object is flagged MONITOR, check for ^ listen patterns
       * unlike normal @listen, don't pass the message on.
       */

      if (has_flag_by_name(target, "MONITOR", NOTYPE)) {
        if (!listen_lock_checked)
          listen_lock_passed = eval_lock(speaker, target, Listen_Lock);
        if (listen_lock_passed) {
          atr_comm_match(target, speaker, '^', ':',
                         fullmsg, 0, 1, NULL, NULL, 0, NULL, NULL,
                         QUEUE_DEFAULT);
        }
      }

      /* If object is flagged AUDIBLE and has a @FORWARDLIST, send it on */
      if ((!(flags & NA_NORELAY) || (flags & NA_PUPPET_OK)) && Audible(target)
          && atr_get(target, "FORWARDLIST") != NULL
          && !filter_found(target, speaker, fullmsg, 0)) {
        notify_list(speaker, target, "FORWARDLIST", fullmsg, flags);
      }
    }

    if ((flags & NA_PROPAGATE) && !(flags & NA_NORELAY) && Audible(target)) {
      char propprefix[BUFFER_LEN];

      /* Propagate sound */
      if (IsRoom(target)) {
        dbref exit;
        DOLIST(exit, Exits(target)) {
          if (Audible(exit)) {
            if (VariableExit(exit))
              loc = find_var_dest(speaker, exit);
            else if (HomeExit(exit))
              loc = Home(speaker);
            else
              loc = Destination(exit);

            if (!RealGoodObject(loc))
              continue;         /* unlinked, variable dests that resolve to bad things */
            if (filter_found(exit, speaker, fullmsg, 0))
              continue;
            /* Need to make the prefix for each exit */
            make_prefix_str(exit, speaker, fullmsg, propprefix);
            notify_anything_sub(speaker, na_next, &Contents(loc), skips,
                                RELAY_FLAGS(flags), message, propprefix, loc,
                                format);
          }
        }
      } else if (target == loc && !filter_found(target, speaker, fullmsg, 0)) {
        dbref pass[2];

        pass[0] = target;
        pass[1] = NOTHING;
        loc = Location(target);
        make_prefix_str(target, speaker, fullmsg, propprefix);
        notify_anything_sub(speaker, na_next, &Contents(loc), pass,
                            RELAY_FLAGS(flags), message, propprefix, loc,
                            format);
      }
    }

    if (fp != NULL) {
      mush_free(fullmsg, "notify_str");
    }
  }

  if (formatmsg)
    mush_free(formatmsg, "notify_str");

}

/** Notify a player with a formatted string, easy version.
 * This is a safer replacement for notify(player, tprintf(fmt, ...))
 * \param player the player to notify
 * \param fmt the format string
 * \param ... format args
 */
void WIN32_CDECL
notify_format(dbref player, const char *fmt, ...)
{
  char buff[BUFFER_LEN];
  va_list args;

  va_start(args, fmt);
  mush_vsnprintf(buff, sizeof buff, fmt, args);
  va_end(args);

  notify(player, buff);
}

/** Send a message to a list of dbrefs on an attribute on an object.
 * Be sure we don't send a message to the object itself!
 * \param speaker message speaker
 * \param thing object containing attribute with list of dbrefs
 * \param atr attribute with list of dbrefs
 * \param msg message to transmit
 * \param flags bitmask of notification option flags
 */
void
notify_list(dbref speaker, dbref thing, const char *atr, const char *msg,
            int flags)
{
  char *fwdstr, *orig, *curr;
  char tbuf1[BUFFER_LEN], *prefix = NULL;
  dbref fwd;
  ATTR *a;

  a = atr_get(thing, atr);
  if (!a)
    return;
  orig = safe_atr_value(a);
  fwdstr = trim_space_sep(orig, ' ');

  tbuf1[0] = '\0';
  if (!(flags & NA_NOPREFIX)) {
    make_prefix_str(thing, speaker, msg, tbuf1);
    prefix = tbuf1;
    if (!(flags & NA_SPOOF)) {
      if (Nospoof(thing))
        flags |= NA_NOSPOOF;
      if (Paranoid(thing))
        flags |= NA_PARANOID;
    }
  }

  flags |= NA_NORELAY;
  flags &= ~NA_PROPAGATE;

  while ((curr = split_token(&fwdstr, ' ')) != NULL) {
    if (is_objid(curr)) {
      fwd = parse_objid(curr);
      if (RealGoodObject(fwd) && (thing != fwd) && Can_Forward(thing, fwd)) {
        if (IsRoom(fwd)) {
          notify_anything(speaker, na_loc, &fwd, NULL, flags, msg,
                          prefix, AMBIGUOUS, NULL);
        } else {
          notify_anything(speaker, na_one, &fwd, NULL, flags, msg,
                          prefix, AMBIGUOUS, NULL);

        }
      }
    }
  }
  free(orig);
}

/** Notify all connected players with the given flag(s).
 * If no flags are given, everyone is notified. If one flag list is given,
 * all connected players with some flag in that list are notified.
 * If two flag lists are given, all connected players with at least one flag
 * in each list are notified.
 * \param flag1 first required flag list or NULL
 * \param flag2 second required flag list or NULL
 * \param fmt format string for message to notify.
 */
void WIN32_CDECL
flag_broadcast(const char *flag1, const char *flag2, const char *fmt, ...)
{
  va_list args;
  char tbuf1[BUFFER_LEN];
  DESC *d;
  int ok;

  va_start(args, fmt);
  mush_vsnprintf(tbuf1, sizeof tbuf1, fmt, args);
  va_end(args);

  DESC_ITER_CONN(d) {
    ok = 1;
    if (flag1)
      ok = ok && (flaglist_check_long("FLAG", GOD, d->player, flag1, 0) == 1);
    if (flag2)
      ok = ok && (flaglist_check_long("FLAG", GOD, d->player, flag2, 0) == 1);
    if (ok) {
      queue_string_eol(d, tbuf1);
      process_output(d);
    }
  }
}

slab *text_block_slab = NULL; /**< Slab for 'struct text_block' allocations */

static struct text_block *
make_text_block(const unsigned char *s, int n)
{
  struct text_block *p;
  if (text_block_slab == NULL) {
    text_block_slab = slab_create("output lines", sizeof(struct text_block));
    /* See what stats are like on M*U*S*H, maybe change */
    slab_set_opt(text_block_slab, SLAB_ALLOC_FIRST_FIT, 1);
    slab_set_opt(text_block_slab, SLAB_ALWAYS_KEEP_A_PAGE, 1);
  }
  p = slab_malloc(text_block_slab, NULL);
  if (!p)
    mush_panic("Out of memory");
  p->buf = mush_malloc(n, "text_block_buff");
  if (!p->buf)
    mush_panic("Out of memory");

  memcpy(p->buf, s, n);
  p->nchars = n;
  p->start = p->buf;
  p->nxt = NULL;
  return p;
}

/** Free a text_block structure.
 * \param t pointer to text_block to free.
 */
void
free_text_block(struct text_block *t)
{
  if (t) {
    if (t->buf)
      mush_free(t->buf, "text_block_buff");
    slab_free(text_block_slab, t);
  }
}

/** Initialize a text_queue structure.
 */
void
init_text_queue(struct text_queue *q)
{
  if (!q)
    return;
  q->head = q->tail = NULL;
  return;
}

/** Add a new chunk of text to a player's output queue.
 * \param q pointer to text_queue to add the chunk to.
 * \param b text to add to the queue.
 * \param n length of text to add.
 */
void
add_to_queue(struct text_queue *q, const unsigned char *b, int n)
{
  struct text_block *p;

  if (n == 0 || !q)
    return;

  p = make_text_block(b, n);

  if (!q->head) {
    q->head = q->tail = p;
  } else {
    q->tail->nxt = p;
    q->tail = p;
  }
}

static int
flush_queue(struct text_queue *q, int n)
{
  struct text_block *p;
  int really_flushed = 0, flen;

  flen = strlen(flushed_message);
  n += flen;

  while (n > 0 && (p = q->head)) {
    n -= p->nchars;
    really_flushed += p->nchars;
    q->head = p->nxt;
    if (q->tail == p)
      q->tail = NULL;
#ifdef DEBUG
    do_rawlog(LT_ERR, "free_text_block(0x%x) at 1.", p);
#endif                          /* DEBUG */
    free_text_block(p);
  }
  p = make_text_block((unsigned char *) flushed_message, flen);
  p->nxt = q->head;
  q->head = p;
  if (!q->tail)
    q->tail = p;
  really_flushed -= p->nchars;
  return really_flushed;
}

#ifdef HAS_OPENSSL
static int
ssl_flush_queue(struct text_queue *q)
{
  struct text_block *p;
  int n = strlen(flushed_message);
  /* Remove all text blocks except the first one. */
  if (q->head) {
    while ((p = q->head->nxt)) {
      q->head->nxt = p->nxt;
#ifdef DEBUG
      do_rawlog(LT_ERR, "free_text_block(0x%x) at 1.", p);
#endif                          /* DEBUG */
      free_text_block(p);
    }
    q->tail = q->head;
    /* Set up the flushed message if we can */
    if (q->head->nchars + n < MAX_OUTPUT)
      add_to_queue(q, (unsigned char *) flushed_message, n);
    /* Return the total size of the message */
    return q->head->nchars + n;
  }
  return 0;
}
#endif

/** Render and add text to the queue associated with a given descriptor.
 * \param d pointer to descriptor to receive the text.
 * \param b text to send.
 * \param n length of b.
 * \return number of characters added.
 */
int
queue_write(DESC *d, const unsigned char *b, int n)
{
  char buff[BUFFER_LEN];
  unsigned char *s;
  int output_type;
  PUEBLOBUFF;
  size_t len;

  if ((n == 2) && (b[0] == '\r') && (b[1] == '\n')) {
    return queue_eol(d);
  }
  if (n > BUFFER_LEN)
    n = BUFFER_LEN;

  memcpy(buff, b, n);
  buff[n] = '\0';

  output_type = notify_type(d);

  if (output_type & MSG_PUEBLO) {
    PUSE;
    tag_wrap("SAMP", NULL, buff);
    PEND;
    s = render_string((unsigned char *) pbuff, output_type);
  } else {
    s = render_string((unsigned char *) buff, output_type);
  }
  len = u_strlen(s);
  queue_newwrite(d, s, len);

  return len;
}

/** Add text to the queue associated with a given descriptor.
 * This is the low-level function that works with already-rendered
 * text.
 * \param d pointer to descriptor to receive the text.
 * \param b text to send.
 * \param n length of b.
 * \return number of characters added.
 */
int
queue_newwrite(DESC *d, const unsigned char *b, int n)
{
  int space;

  space = MAX_OUTPUT - d->output_size - n;
  if (space < SPILLOVER_THRESHOLD) {
    process_output(d);
    space = MAX_OUTPUT - d->output_size - n;
    if (space < 0) {
#ifdef HAS_OPENSSL
      if (d->ssl) {
        /* Now we have a problem, as SSL works in blocks and you can't
         * just partially flush stuff.
         */
        d->output_size = ssl_flush_queue(&d->output);
      } else
#endif
        d->output_size -= flush_queue(&d->output, -space);
    }
  }
  add_to_queue(&d->output, b, n);
  d->output_size += n;
  return n;
}

/** Add an end-of-line to a descriptor's text queue.
 * \param d pointer to descriptor to send the eol to.
 * \return number of characters queued.
 */
int
queue_eol(DESC *d)
{
  if ((d->conn_flags & CONN_HTML))
    return queue_newwrite(d, (unsigned char *) "<BR>\n", 5);
  else
    return queue_newwrite(d, (unsigned char *) "\r\n", 2);
}

/** Add a string and an end-of-line to a descriptor's text queue.
 * \param d pointer to descriptor to send to.
 * \param s string to queue.
 * \return number of characters queued.
 */
int
queue_string_eol(DESC *d, const char *s)
{
  int num = 0;
  num = queue_string(d, s);
  return num + queue_eol(d);
}

/** Add a string to a descriptor's text queue.
 * \param d pointer to descriptor to send to.
 * \param s string to queue.
 * \return number of characters queued.
 */
int
queue_string(DESC *d, const char *s)
{
  unsigned char *rendered;
  int output_type;
  int ret;

  output_type = notify_type(d);
  rendered = render_string((unsigned char *) s, output_type);
  ret = queue_newwrite(d, rendered, u_strlen(rendered));

  return ret;
}


/** Free all text queues associated with a descriptor.
 * \param d pointer to descriptor.
 */
void
freeqs(DESC *d)
{
  struct text_block *cur, *next;

  for (cur = d->output.head; cur; cur = next) {
    next = cur->nxt;
#ifdef DEBUG
    do_rawlog(LT_ERR, "free_text_block(0x%x) at 3.", cur);
#endif                          /* DEBUG */
    free_text_block(cur);
  }
  d->output.head = d->output.tail = NULL;

  for (cur = d->input.head; cur; cur = next) {
    next = cur->nxt;
#ifdef DEBUG
    do_rawlog(LT_ERR, "free_text_block(0x%x) at 4.", cur);
#endif                          /* DEBUG */
    free_text_block(cur);
  }
  d->input.head = d->input.tail = NULL;

  if (d->raw_input)
    mush_free(d->raw_input, "descriptor_raw_input");
  d->raw_input = 0;
  d->raw_input_at = 0;
}
