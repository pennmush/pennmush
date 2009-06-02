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
#include "patches.h"
#include "mysocket.h"
#include "ident.h"
#include "strtree.h"
#include "log.h"
#include "mymalloc.h"

#include "extchat.h"
extern CHAN *channels;
#include "extmail.h"
#include "attrib.h"
#include "game.h"
#include "confmagic.h"

static int under_limit = 1;


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

/** Types of text renderings we can do in notify_anything(). */
enum na_type {
  NA_ASCII = 0,                 /**< Plain old ascii. */
  NA_ANSI,                      /**< ANSI flag */
  NA_COLOR,                     /**< ANSI and COLOR flags */
  NA_PUEBLO,                    /**< html */
  NA_PASCII,                    /**< Player without any of the above */
  NA_TANSI,                     /**< Like above with telnet-aware client */
  NA_TCOLOR,                    /**< Like above with telnet-aware client */
  NA_TPASCII,                   /**< Like above with telnet-aware client */
  NA_NANSI,                     /**< ANSI and NOACCENTS */
  NA_NCOLOR,                    /**< ANSI, COLOR, NOACCENTS */
  NA_NPUEBLO,                   /**< html & NOACCENTS */
  NA_NPASCII                    /**< NOACCENTS */
};

/** Number of possible message text renderings */
#define MESSAGE_TYPES 12

/* These should be removed. I can't imagine anyone uses them in hacks,
 * but I'm leaving them here just in case... */
#define TA_BGC 0        /**< Text attribute background color */
#define TA_FGC 1        /**< Text attribute foreground color */
#define TA_BOLD 2       /**< Text attribute bold/hilite */
#define TA_REV 3        /**< Text attribute reverse/inverse */
#define TA_BLINK 4      /**< Text attribute blinking/flashing */
#define TA_ULINE 5      /**< Text attribute underline */


static int na_depth = 0;

/** A place to store a rendered message. */
struct notify_strings {
  unsigned char *message;       /**< The message text. */
  size_t len;                   /**< Length of message. */
  int made;                     /**< True if message has been rendered. */
};

static enum na_type notify_type(DESC *d);
static void free_strings(struct notify_strings messages[]);
static void zero_strings(struct notify_strings messages[]);
static int output_ansichange(ansi_data *states, int *ansi_ptr,
                             const unsigned char **ptr, char *buff, char **bp);
static unsigned char *notify_makestring(const char *message,
                                        struct notify_strings messages[],
                                        enum na_type type, int flags);

static void
zero_strings(struct notify_strings messages[])
{
  int n;
  for (n = 0; n < MESSAGE_TYPES; n++) {
    messages[n].message = NULL;
    messages[n].len = 0;
    messages[n].made = 0;
  }
}

static void
free_strings(struct notify_strings messages[])
{
  int n;
  for (n = 0; n < MESSAGE_TYPES; n++)
    if (messages[n].message)
      mush_free(messages[n].message, "string");
}

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

static unsigned char *
notify_makestring(const char *message, struct notify_strings messages[],
                  enum na_type type, int flags)
{
  char *o;
  const unsigned char *p;
  char *t;
  int color = 0;
  int strip = 0;
  int pueblo = 0;
  static char tbuf[BUFFER_LEN];
  char *bp;

  static ansi_data states[BUFFER_LEN];
  int ansi_ptr, ansifix;
  ansi_ptr = 0;
  ansifix = 0;

  /* Everything is explicitly off by default */
  states[0].bits = 0;
  states[0].offbits = 0;
  states[0].fore = 0;
  states[0].back = 0;

  if (messages[type].made) {
    if (flags & NA_PROMPT) {
      bp = tbuf;
      safe_str((char *) messages[type].message, tbuf, &bp);
      safe_chr((char) IAC, tbuf, &bp);
      safe_chr((char) GOAHEAD, tbuf, &bp);
      *bp = '\0';
      return (unsigned char *) tbuf;
    } else
      return messages[type].message;
  }
  messages[type].made = 1;

  p = (unsigned char *) message;
  o = tbuf;
  t = o;

  /* Since well over 50% is this type, we do it quick */
  switch (type) {
  case NA_ASCII:
    while (*p) {
      switch (*p) {
      case TAG_START:
        while (*p && *p != TAG_END)
          p++;
        break;
      case '\r':
      case BEEP_CHAR:
        break;
      case ESC_CHAR:
        while (*p && *p != 'm')
          p++;
        break;
      default:
        safe_chr(*p, t, &o);
      }
      p++;
    }
    *o = '\0';
    messages[type].message = (unsigned char *) mush_strdup(tbuf, "string");
    messages[type].len = o - tbuf;
    if (flags & NA_PROMPT) {
      bp = tbuf;
      safe_str((char *) messages[type].message, tbuf, &bp);
      safe_chr((char) IAC, tbuf, &bp);
      safe_chr((char) GOAHEAD, tbuf, &bp);
      *bp = '\0';
      return (unsigned char *) tbuf;
    }
    return messages[type].message;
  case NA_NPASCII:
    strip = 1;
  case NA_PASCII:
  case NA_TPASCII:
    /* PLAYER Ascii. Different output. \n is \r\n */
    if (type == NA_NPASCII)
      strip = 1;
    while (*p) {
      switch (*p) {
      case IAC:
        if (type == NA_TPASCII)
          safe_strl("\xFF\xFF", 2, t, &o);
        else if (strip)
          safe_str(accent_table[IAC].base, t, &o);
        else
          safe_chr((char) IAC, t, &o);
        break;
      case TAG_START:
        while (*p && *p != TAG_END)
          p++;
        break;
      case ESC_CHAR:
        while (*p && *p != 'm')
          p++;
        break;
      case '\r':
        break;
      case '\n':
        safe_strl("\r\n", 2, t, &o);
        break;
      default:
        if (strip && accent_table[(unsigned char) *p].base)
          safe_str(accent_table[(unsigned char) *p].base, t, &o);
        else
          safe_chr(*p, t, &o);
      }
      p++;
    }
    *o = '\0';
    messages[type].message = (unsigned char *) mush_strdup(tbuf, "string");
    messages[type].len = o - tbuf;
    if (flags & NA_PROMPT) {
      bp = tbuf;
      safe_str((char *) messages[type].message, tbuf, &bp);
      safe_chr((char) IAC, tbuf, &bp);
      safe_chr((char) GOAHEAD, tbuf, &bp);
      *bp = '\0';
      return (unsigned char *) tbuf;
    }
    return messages[type].message;

  case NA_PUEBLO:
  case NA_NPUEBLO:
    pueblo = 1;
    /* FALLTHROUGH */
  case NA_COLOR:
  case NA_TCOLOR:
  case NA_NCOLOR:
    color = 1;
    /* FALLTHROUGH */
  case NA_ANSI:
  case NA_TANSI:
  case NA_NANSI:
    if (type == NA_NCOLOR || type == NA_NANSI || type == NA_NPUEBLO)
      strip = 1;
    while (*p) {
      switch ((unsigned char) *p) {
      case IAC:
        if (type == NA_TANSI || type == NA_TCOLOR)
          safe_strl("\xFF\xFF", 2, t, &o);
        else if (strip && accent_table[IAC].base)
          safe_str(accent_table[IAC].base, t, &o);
        else
          safe_chr((char) IAC, t, &o);
        break;
      case TAG_START:
        if (*(p + 1) == MARKUP_COLOR) {
          ansifix += output_ansichange(states, &ansi_ptr, &p, t, &o);
        } else if (pueblo && (*(p + 1) == MARKUP_HTML)) {
          safe_chr('<', t, &o);
          /* Skip over the 'p' for Pueblo */
          p += 2;
          while ((*p) && (*p != TAG_END)) {
            safe_chr(*p, t, &o);
            p++;
          }
          safe_chr('>', t, &o);
        } else {
          while (*p && *p != TAG_END)
            p++;
        }
        break;
      case TAG_END:
        /* Should never be seen alone */
        break;
      case '\r':
        break;
      case ESC_CHAR:
        /* After the ansi changes, I really hope we don't encounter this. */
        ansifix += output_ansichange(states, &ansi_ptr, &p, t, &o);
        break;
      default:
        if (pueblo) {
          if (strip) {
            /* Even if we're NOACCENTS, we must still translate a few things */
            switch ((unsigned char) *p) {
            case '\n':
            case '&':
            case '<':
            case '>':
            case '"':
              safe_str(accent_table[(unsigned char) *p].entity, t, &o);
              break;
            default:
              if (accent_table[(unsigned char) *p].base)
                safe_str(accent_table[(unsigned char) *p].base, t, &o);
              else
                safe_chr(*p, t, &o);
              break;
            }
          } else if (accent_table[(unsigned char) *p].entity)
            safe_str(accent_table[(unsigned char) *p].entity, t, &o);
          else
            safe_chr(*p, t, &o);
        } else {
          /* Non-pueblo */
          if ((unsigned char) *p == '\n')
            safe_str("\r\n", t, &o);
          else if (strip && accent_table[(unsigned char) *p].base)
            safe_str(accent_table[(unsigned char) *p].base, t, &o);
          else
            safe_chr(*p, t, &o);
        }
      }
      if (*p)
        p++;
    }
    /* We possibly have some unclosed ansi. Force an
     * ANSI_NORMAL for now. */
    if (ansifix || (ansi_ptr && safe_str(ANSI_RAW_NORMAL, t, &o))) {
      int sub = 7;
      char *ptr;
      int q;
      if (flags & NA_PROMPT)
        sub += 2;
      ptr = t + BUFFER_LEN - sub;
      for (q = 20; q > 0 && *ptr != ESC_CHAR; q--, ptr--) ;
      if (pueblo) {
        for (q = 20; q > 0 && *ptr != ESC_CHAR && *ptr != '<'; q--, ptr--) ;
      } else {
        for (q = 20; q > 0 && *ptr != ESC_CHAR; q--, ptr--) ;
      }
      if (q > 0) {
        o = ptr;
      } else {
        o = t + BUFFER_LEN - sub;
      }
      safe_str(ANSI_RAW_NORMAL, t, &o);
      *o = '\0';
    }

    break;
  }

  *o = '\0';
  messages[type].message = (unsigned char *) mush_strdup(tbuf, "string");
  messages[type].len = o - tbuf;
  if (flags & NA_PROMPT) {
    bp = tbuf;
    safe_str((char *) messages[type].message, tbuf, &bp);
    safe_chr((char) IAC, tbuf, &bp);
    safe_chr((char) GOAHEAD, tbuf, &bp);
    *bp = '\0';
    return (unsigned char *) tbuf;
  }
  return messages[type].message;
}

/*--------------------------------------------------------------
 * Iterators for notify_anything.
 * notify_anything calls these functions repeatedly to get the
 * next object to notify, passing in the last object notified.
 * On the first pass, it passes in NOTHING. When it finally
 * receives NOTHING back, it stops.
 */

/** notify_anthing() iterator for a single dbref.
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

/** notify_anthing() iterator for following a contents/exit chain.
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

/** notify_anthing() iterator for a location and its contents.
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

/** notify_anthing() iterator for a contents/exit chain, with a dbref to skip.
 * \param current last dbref from iterator.
 * \param data memory address containing array of two dbrefs: the start of the chain and the dbref to skip.
 * \return dbref of next object to notify, or NOTHING when done.
 */
dbref
na_nextbut(dbref current, void *data)
{
  dbref *dbrefs = data;

  do {
    if (current == NOTHING)
      current = dbrefs[0];
    else
      current = Next(current);
  } while (current == dbrefs[1]);
  return current;
}

/** notify_anthing() iterator for a location and its contents, with a dbref to skip.
 * \param current last dbref from iterator.
 * \param data memory address containing array of two dbrefs: the location and the dbref to skip.
 * \return dbref of next object to notify, or NOTHING when done.
 */
dbref
na_except(dbref current, void *data)
{
  dbref *dbrefs = data;

  do {
    if (current == NOTHING)
      current = dbrefs[0];
    else if (current == dbrefs[0])
      current = Contents(current);
    else
      current = Next(current);
  } while (current == dbrefs[1]);
  return current;
}

/** notify_anthing() iterator for a location and its contents, with 2 dbrefs to skip.
 * \param current last dbref from iterator.
 * \param data memory address containing array of three dbrefs: the location and the dbrefs to skip.
 * \return dbref of next object to notify, or NOTHING when done.
 */
dbref
na_except2(dbref current, void *data)
{
  dbref *dbrefs = data;

  do {
    if (current == NOTHING)
      current = dbrefs[0];
    else if (current == dbrefs[0])
      current = Contents(current);
    else
      current = Next(current);
  } while ((current == dbrefs[1]) || (current == dbrefs[2]));
  return current;
}

/** notify_anthing() iterator for a location and its contents, with N dbrefs to skip.
 * \param current last dbref from iterator.
 * \param data memory address containing array of three or more values: the number of dbrefs to skip, the location, and the dbrefs to skip.
 * \return dbref of next object to notify, or NOTHING when done.
 */
dbref
na_exceptN(dbref current, void *data)
{
  dbref *dbrefs = data;
  int i, check;

  do {
    if (current == NOTHING)
      current = dbrefs[1];
    else if (current == dbrefs[1])
      current = Contents(current);
    else
      current = Next(current);
    check = 0;
    for (i = 2; i < dbrefs[0] + 2; i++)
      if (current == dbrefs[i])
        check = 1;
  } while (check);
  return current;
}


static enum na_type
notify_type(DESC *d)
{
  enum na_type poutput;
  int strip;

  if (!d->connected) {
    /* These are the settings used at, e.g., the connect screen,
     * when there's no connected player yet. If you want to use
     * ansified connect screens, you'd probably change NA_NPASCII
     * to NA_NCOLOR (for no accents) or NA_COLOR (for accents). 
     * We don't recommend it. If you want to use accented characters,
     * change NA_NPUEBLO and NA_NPASCII to NA_PUEBLO and NA_PASCII,
     * respectively. That's not so bad.
     */
    return (d->conn_flags & CONN_HTML) ? NA_NPUEBLO : NA_NPASCII;
  }

  /* At this point, we have a connected player on the descriptor */
  strip = IS(d->player, TYPE_PLAYER, "NOACCENTS");

  if (d->conn_flags & CONN_HTML) {
    poutput = strip ? NA_NPUEBLO : NA_PUEBLO;
  } else if (ShowAnsi(d->player)) {
    if (ShowAnsiColor(d->player)) {
      if (strip)
        poutput = NA_NCOLOR;
      else
        poutput = (d->conn_flags & CONN_TELNET) ? NA_TCOLOR : NA_COLOR;
    } else {
      if (strip)
        poutput = NA_NANSI;
      else
        poutput = (d->conn_flags & CONN_TELNET) ? NA_TANSI : NA_ANSI;
    }
  } else {
    if (strip)
      poutput = NA_NPASCII;
    else
      poutput = (d->conn_flags & CONN_TELNET) ? NA_TPASCII : NA_PASCII;
  }
  return poutput;
}

/** Send a message to a series of dbrefs.
 * This key function takes a speaker's utterance and looks up each
 * object that should hear it. For each, it may need to render
 * the utterance in a different fashion (with or without ansi, html,
 * accents), but we cache each rendered version for efficiency.
 * \param speaker dbref of object producing the message.
 * \param func pointer to iterator function to look up each receiver.
 * \param fdata initial data to pass to func.
 * \param nsfunc function to call to do NOSPOOF formatting, or NULL.
 * \param flags flags to pass in (such as NA_INTERACT)
 * \param message message to render and transmit.
 * \param loc location the message was sent to.
 */
void
notify_anything_loc(dbref speaker, na_lookup func,
                    void *fdata, char *(*nsfunc) (dbref, na_lookup func, void *,
                                                  int), int flags,
                    const char *message, dbref loc)
{
  dbref target;
  dbref passalong[3];
  struct notify_strings messages[MESSAGE_TYPES];
  struct notify_strings nospoofs[MESSAGE_TYPES];
  struct notify_strings paranoids[MESSAGE_TYPES];
  int i, j;
  DESC *d;
  enum na_type poutput;
  unsigned char *pstring;
  size_t plen;
  char *bp;
  ATTR *a;
  char *asave;
  char const *ap;
  char *preserve[NUMQ];
  int havespoof = 0;
  int havepara = 0;
  char *wsave[10];
  char *tbuf1 = NULL, *nospoof = NULL, *paranoid = NULL, *msgbuf;
  static dbref puppet = NOTHING;
  int nsflags;

  if (!message || *message == '\0' || !func)
    return;

  /* Depth check */
  if (na_depth > 7)
    return;
  na_depth++;

  /* Only allocate these buffers when needed */
  for (i = 0; i < MESSAGE_TYPES; i++) {
    messages[i].message = NULL;
    messages[i].made = 0;
    nospoofs[i].message = NULL;
    nospoofs[i].made = 0;
    paranoids[i].message = NULL;
    paranoids[i].made = 0;
  }

  msgbuf = mush_strdup(message, "string");

  target = NOTHING;

  while ((target = func(target, fdata)) != NOTHING) {
    if ((flags & NA_PONLY) && !IsPlayer(target))
      continue;

    if (IsPlayer(target)) {
      if (!Connected(target) && options.login_allow && under_limit)
        continue;

      if (flags & NA_INTERACTION) {
        int pass_interact = 1;
        if ((flags & NA_INTER_SEE) &&
            !can_interact(speaker, target, INTERACT_SEE))
          pass_interact = 0;
        if (pass_interact && (flags & NA_INTER_PRESENCE) &&
            !can_interact(speaker, target, INTERACT_PRESENCE))
          pass_interact = 0;
        if (pass_interact && (flags & NA_INTER_HEAR) &&
            !can_interact(speaker, target, INTERACT_HEAR))
          pass_interact = 0;
        if (pass_interact && (flags & NA_INTER_LOCK) &&
            !Pass_Interact_Lock(speaker, target))
          pass_interact = 0;
        if (!pass_interact)
          continue;
      }

      for (d = descriptor_list; d; d = d->next) {
        if (d->connected && d->player == target) {
          poutput = notify_type(d);

          if ((flags & NA_PONLY) && (poutput != NA_PUEBLO))
            continue;

          if (!(flags & NA_SPOOF)
              && (nsfunc && ((Nospoof(target) && (target != speaker))
                             || (flags & NA_NOSPOOF)))) {
            if (Paranoid(target) || (flags & NA_PARANOID)) {
              if (!havepara) {
                paranoid = nsfunc(speaker, func, fdata, 1);
                havepara = 1;
              }
              pstring = notify_makestring(paranoid, paranoids, poutput, flags);
              plen = u_strlen(pstring);
            } else {
              if (!havespoof) {
                nospoof = nsfunc(speaker, func, fdata, 0);
                havespoof = 1;
              }
              pstring = notify_makestring(nospoof, nospoofs, poutput, flags);
              plen = u_strlen(pstring);
            }
            queue_newwrite(d, pstring, plen);
          }

          pstring = notify_makestring(msgbuf, messages, poutput, flags);
          plen = u_strlen(pstring);
          if (pstring && *pstring)
            queue_newwrite(d, pstring, plen);

          if (!((flags & NA_NOENTER) || ((flags & NA_PROMPT)
                                         && !(d->conn_flags &
                                              CONN_PROMPT_NEWLINES)))) {
            if ((poutput == NA_PUEBLO) || (poutput == NA_NPUEBLO)) {
              if (flags & NA_NOPENTER)
                queue_newwrite(d, (unsigned char *) "\n", 1);
              else
                queue_newwrite(d, (unsigned char *) "<BR>\n", 5);
            } else {
              queue_newwrite(d, (unsigned char *) "\r\n", 2);
            }
          }
        }
      }
    } else if (Puppet(target) &&
               ((Location(target) != Location(Owner(target))) ||
                Verbose(target) ||
                (flags & NA_MUST_PUPPET)) &&
               ((flags & NA_PUPPET) || !(flags & NA_NORELAY))) {
      dbref last = puppet;

      if (flags & NA_INTERACTION) {
        int pass_interact = 1;
        if ((flags & NA_INTER_SEE) &&
            !can_interact(speaker, target, INTERACT_SEE))
          pass_interact = 0;
        if (pass_interact && (flags & NA_INTER_PRESENCE) &&
            !can_interact(speaker, target, INTERACT_PRESENCE))
          pass_interact = 0;
        if (pass_interact && (flags & NA_INTER_HEAR) &&
            !can_interact(speaker, target, INTERACT_HEAR))
          pass_interact = 0;
        if (!pass_interact)
          continue;
      }

      puppet = target;
      if (!tbuf1)
        tbuf1 = (char *) mush_malloc(BUFFER_LEN, "string");
      bp = tbuf1;
      safe_str(Name(target), tbuf1, &bp);
      safe_str("> ", tbuf1, &bp);
      *bp = '\0';
      flags &= ~NA_PROMPT;      /* No prompts to puppets */
      notify_anything(GOD, na_one, &Owner(target), NULL,
                      NA_NOENTER | NA_PUPPET2 | NA_NORELAY | flags, tbuf1);

      nsflags = 0;
      if (!(flags & NA_SPOOF)) {
        if (Nospoof(target))
          nsflags |= NA_NOSPOOF;
        if (Paranoid(target))
          nsflags |= NA_PARANOID;
      }
      notify_anything(speaker, na_one, &Owner(target), ns_esnotify,
                      flags | nsflags | NA_NORELAY | NA_PUPPET2, msgbuf);
      puppet = last;
    }
    if ((flags & NA_NOLISTEN)
        || (!PLAYER_LISTEN && IsPlayer(target))
        || IsExit(target))
      continue;

    /* do @listen stuff */
    a = atr_get_noparent(target, "LISTEN");
    if (a) {
      char match_space[BUFFER_LEN * 2];
      ssize_t match_space_len = BUFFER_LEN * 2;

      if (!tbuf1)
        tbuf1 = (char *) mush_malloc(BUFFER_LEN, "string");
      strcpy(tbuf1, atr_value(a));
      if (AF_Regexp(a)
          ? regexp_match_case_r(tbuf1,
                                (char *) notify_makestring(msgbuf, messages,
                                                           NA_ASCII, 0),
                                AF_Case(a), global_eval_context.wnxt, 10,
                                match_space, match_space_len)
          : wild_match_case_r(tbuf1,
                              (char *) notify_makestring(msgbuf, messages,
                                                         NA_ASCII, 0),
                              AF_Case(a), global_eval_context.wnxt, 10,
                              match_space, match_space_len)) {
        if (eval_lock(speaker, target, Listen_Lock))
          if (PLAYER_AHEAR || (!IsPlayer(target))) {
            if (speaker != target)
              charge_action(speaker, target, "AHEAR");
            else
              charge_action(speaker, target, "AMHEAR");
            charge_action(speaker, target, "AAHEAR");
          }
        if (!(flags & NA_NORELAY) && (loc != target) &&
            !filter_found(target,
                          (char *) notify_makestring(msgbuf, messages,
                                                     NA_ASCII, 0), 1)) {
          passalong[0] = target;
          passalong[1] = target;
          passalong[2] = Owner(target);
          a = atr_get(target, "INPREFIX");
          if (a) {
            for (j = 0; j < 10; j++)
              wsave[j] = global_eval_context.wenv[j];
            global_eval_context.wenv[0] = (char *) msgbuf;
            for (j = 1; j < 10; j++)
              global_eval_context.wenv[j] = NULL;
            save_global_regs("inprefix_save", preserve);
            asave = safe_atr_value(a);
            ap = asave;
            bp = tbuf1;
            process_expression(tbuf1, &bp, &ap, target, speaker, speaker,
                               PE_DEFAULT, PT_DEFAULT, NULL);
            if (bp != tbuf1)
              safe_chr(' ', tbuf1, &bp);
            safe_str(msgbuf, tbuf1, &bp);
            *bp = 0;
            free(asave);
            restore_global_regs("inprefix_save", preserve);
            for (j = 0; j < 10; j++)
              global_eval_context.wenv[j] = wsave[j];
          }
          notify_anything(speaker, Puppet(target) ? na_except2 : na_except,
                          passalong, NULL, flags | NA_NORELAY | NA_PUPPET,
                          (a) ? tbuf1 : msgbuf);
        }
      }
    }
    /* if object is flagged LISTENER, check for ^ listen patterns
     *    * unlike normal @listen, don't pass the message on.
     *    */

    if ((ThingListen(target) || RoomListen(target))
        && eval_lock(speaker, target, Listen_Lock)
      )
      atr_comm_match(target, speaker, '^', ':',
                     (char *) notify_makestring(msgbuf, messages, NA_ASCII, 0),
                     0, 1, NULL, NULL, NULL);

    /* If object is flagged AUDIBLE and has a @FORWARDLIST, send
     *  stuff on */
    if ((!(flags & NA_NORELAY) || (flags & NA_PUPPET)) && Audible(target)
        && ((a = atr_get_noparent(target, "FORWARDLIST")) != NULL)
        && !filter_found(target, msgbuf, 0)) {
      notify_list(speaker, target, "FORWARDLIST", msgbuf, flags);

    }
  }

  for (i = 0; i < MESSAGE_TYPES; i++) {
    if (messages[i].message)
      mush_free((Malloc_t) messages[i].message, "string");
    if (nospoofs[i].message)
      mush_free((Malloc_t) nospoofs[i].message, "string");
    if (paranoids[i].message)
      mush_free((Malloc_t) paranoids[i].message, "string");
  }
  if (nospoof)
    mush_free((Malloc_t) nospoof, "string");
  if (paranoid)
    mush_free((Malloc_t) paranoid, "string");
  if (tbuf1)
    mush_free((Malloc_t) tbuf1, "string");
  mush_free((Malloc_t) msgbuf, "string");
  na_depth--;
}

/** Send a message to a series of dbrefs.
 * This key function takes a speaker's utterance and looks up each
 * object that should hear it. For each, it may need to render
 * the utterance in a different fashion (with or without ansi, html,
 * accents), but we cache each rendered version for efficiency.
 * \param speaker dbref of object producing the message.
 * \param func pointer to iterator function to look up each receiver.
 * \param fdata initial data to pass to func.
 * \param nsfunc function to call to do NOSPOOF formatting, or NULL.
 * \param flags flags to pass in (such as NA_INTERACT)
 * \param message message to render and transmit.
 */
void
notify_anything(dbref speaker, na_lookup func,
                void *fdata, char *(*nsfunc) (dbref, na_lookup func, void *,
                                              int), int flags,
                const char *message)
{
  dbref loc;

  if (GoodObject(speaker))
    loc = Location(speaker);
  else
    loc = NOTHING;

  notify_anything_loc(speaker, func, fdata, nsfunc, flags, message, loc);
}

/** Basic 'notify player with message */
#define notify(p,m)           notify_anything(orator, na_one, &(p), NULL, 0, m)

/** Notify a player with a formatted string, easy version.
 * This is a safer replacement for notify(player, tprintf(fmt, ...))
 * \param speaker dbref of object producing the message.
 * \param func pointer to iterator function to look up each receiver.
 * \param fdata initial data to pass to func.
 * \param nsfunc function to call to do NOSPOOF formatting, or NULL.
 * \param flags flags to pass in (such as NA_INTERACT)
 * \param fmt format string.
 */
void WIN32_CDECL
notify_format(dbref player, const char *fmt, ...)
{
#ifdef HAS_VSNPRINTF
  char buff[BUFFER_LEN];
#else
  char buff[BUFFER_LEN * 3];
#endif
  va_list args;
  va_start(args, fmt);
#ifdef HAS_VSNPRINTF
  vsnprintf(buff, sizeof buff, fmt, args);
#else
  vsprintf(buff, fmt, args);
#endif
  buff[BUFFER_LEN - 1] = '\0';
  va_end(args);
  notify(player, buff);
}


/** Notify a player with a formatted string, full version.
 * This is a safer replacement for notify(player, tprintf(fmt, ...))
 * \param speaker dbref of object producing the message.
 * \param func pointer to iterator function to look up each receiver.
 * \param fdata initial data to pass to func.
 * \param nsfunc function to call to do NOSPOOF formatting, or NULL.
 * \param flags flags to pass in (such as NA_INTERACT)
 * \param fmt format string.
 */
void WIN32_CDECL
notify_anything_format(dbref speaker, na_lookup func,
                       void *fdata, char *(*nsfunc) (dbref, na_lookup func,
                                                     void *, int), int flags,
                       const char *fmt, ...)
{
#ifdef HAS_VSNPRINTF
  char buff[BUFFER_LEN];
#else
  char buff[BUFFER_LEN * 3];
#endif
  va_list args;
  va_start(args, fmt);
#ifdef HAS_VSNPRINTF
  vsnprintf(buff, sizeof buff, fmt, args);
#else
  vsprintf(buff, fmt, args);
#endif
  buff[BUFFER_LEN - 1] = '\0';
  va_end(args);
  notify_anything(speaker, func, fdata, nsfunc, flags, buff);
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
  char tbuf1[BUFFER_LEN], *bp;
  dbref fwd;
  ATTR *a;
  int nsflags;

  a = atr_get(thing, atr);
  if (!a)
    return;
  orig = safe_atr_value(a);
  fwdstr = trim_space_sep(orig, ' ');

  bp = tbuf1;
  nsflags = 0;
  if (!(flags & NA_NOPREFIX)) {
    make_prefixstr(thing, msg, tbuf1);
    if (!(flags & NA_SPOOF)) {
      if (Nospoof(thing))
        nsflags |= NA_NOSPOOF;
      if (Paranoid(thing))
        nsflags |= NA_PARANOID;
    }
  } else {
    safe_str(msg, tbuf1, &bp);
    *bp = 0;
  }

  while ((curr = split_token(&fwdstr, ' ')) != NULL) {
    if (is_objid(curr)) {
      fwd = parse_objid(curr);
      if (GoodObject(fwd) && !IsGarbage(fwd) && (thing != fwd)
          && Can_Forward(thing, fwd)) {
        if (IsRoom(fwd)) {
          notify_anything(speaker, na_loc, &fwd, ns_esnotify,
                          flags | nsflags | NA_NORELAY, tbuf1);
        } else {
          notify_anything(speaker, na_one, &fwd, ns_esnotify,
                          flags | nsflags | NA_NORELAY, tbuf1);
        }
      }
    }
  }
  free((Malloc_t) orig);
}

/** Wrapper to notify a single player with a message, unconditionally.
 * \param player player to notify.
 * \param msg message to send.
 */
void
raw_notify(dbref player, const char *msg)
{
  notify_anything(GOD, na_one, &player, NULL, NA_NOLISTEN, msg);
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
#ifdef HAS_VSNPRINTF
  (void) vsnprintf(tbuf1, sizeof tbuf1, fmt, args);
#else
  (void) vsprintf(tbuf1, fmt, args);
#endif
  va_end(args);
  tbuf1[BUFFER_LEN - 1] = '\0';

  DESC_ITER_CONN(d) {
    ok = 1;
    if (flag1)
      ok = ok && flaglist_check_long("FLAG", GOD, d->player, flag1, 0);
    if (flag2)
      ok = ok && flaglist_check_long("FLAG", GOD, d->player, flag2, 0);
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
  p->nxt = 0;
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
      mush_free((Malloc_t) t->buf, "text_block_buff");
    slab_free(text_block_slab, t);
  }
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

  if (n == 0)
    return;

  p = make_text_block(b, n);
  p->nxt = 0;
  *q->tail = p;
  q->tail = &p->nxt;
}

static int
flush_queue(struct text_queue *q, int n)
{
  struct text_block *p;
  int really_flushed = 0;
  n += strlen(flushed_message);
  while (n > 0 && (p = q->head)) {
    n -= p->nchars;
    really_flushed += p->nchars;
    q->head = p->nxt;
#ifdef DEBUG
    do_rawlog(LT_ERR, "free_text_block(0x%x) at 1.", p);
#endif                          /* DEBUG */
    free_text_block(p);
  }
  p =
    make_text_block((unsigned char *) flushed_message, strlen(flushed_message));
  p->nxt = q->head;
  q->head = p;
  if (!p->nxt)
    q->tail = &p->nxt;
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
  }
  q->tail = &q->head->nxt;
  /* Set up the flushed message if we can */
  if (q->head->nchars + n < MAX_OUTPUT)
    add_to_queue(q, (unsigned char *) flushed_message, n);
  /* Return the total size of the message */
  return q->head->nchars + n;
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
  struct notify_strings messages[MESSAGE_TYPES];
  unsigned char *s;
  PUEBLOBUFF;
  size_t len;

  if ((n == 2) && (b[0] == '\r') && (b[1] == '\n')) {
    if ((d->conn_flags & CONN_HTML))
      queue_newwrite(d, (unsigned char *) "<BR>\n", 5);
    else
      queue_newwrite(d, b, 2);
    return n;
  }
  if (n > BUFFER_LEN)
    n = BUFFER_LEN;

  memcpy(buff, b, n);
  buff[n] = '\0';

  zero_strings(messages);

  if (d->conn_flags & CONN_HTML) {
    PUSE;
    tag_wrap("SAMP", NULL, buff);
    PEND;
    s = notify_makestring(pbuff, messages, NA_PUEBLO, 0);
    len = messages[NA_PUEBLO].len;
  } else {
    s = notify_makestring(buff, messages, notify_type(d), 0);
    len = messages[notify_type(d)].len;
  }
  queue_newwrite(d, s, len);
  free_strings(messages);
  return n;
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
  if (SUPPORT_PUEBLO && (d->conn_flags & CONN_HTML))
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
  queue_string(d, s);
  return queue_eol(d);
}

/** Add a string to a descriptor's text queue.
 * \param d pointer to descriptor to send to.
 * \param s string to queue.
 * \return number of characters queued.
 */
int
queue_string(DESC *d, const char *s)
{
  unsigned char *n;
  enum na_type poutput;
  struct notify_strings messages[MESSAGE_TYPES];
  dbref target;
  int ret;

  zero_strings(messages);

  target = d->player;

  poutput = notify_type(d);

  n = notify_makestring(s, messages, poutput, 0);
  ret = queue_newwrite(d, n, messages[poutput].len);
  free_strings(messages);
  return ret;
}


/** Free all text queues associated with a descriptor.
 * \param d pointer to descriptor.
 */
void
freeqs(DESC *d)
{
  struct text_block *cur, *next;
  cur = d->output.head;
  while (cur) {
    next = cur->nxt;
#ifdef DEBUG
    do_rawlog(LT_ERR, "free_text_block(0x%x) at 3.", cur);
#endif                          /* DEBUG */
    free_text_block(cur);
    cur = next;
  }
  d->output.head = 0;
  d->output.tail = &d->output.head;

  cur = d->input.head;
  while (cur) {
    next = cur->nxt;
#ifdef DEBUG
    do_rawlog(LT_ERR, "free_text_block(0x%x) at 4.", cur);
#endif                          /* DEBUG */
    free_text_block(cur);
    cur = next;
  }
  d->input.head = 0;
  d->input.tail = &d->input.head;

  if (d->raw_input) {
    mush_free((Malloc_t) d->raw_input, "descriptor_raw_input");
  }
  d->raw_input = 0;
  d->raw_input_at = 0;
}

/** A notify_anything function for formatting speaker data for NOSPOOF.
 *  * \param speaker the speaker.
 *   * \param func unused.
 *    * \param fdata unused.
 *     * \param para if 1, format for paranoid nospoof; if 0, normal nospoof.
 *      * \return formatted string.
 *       */
char *
ns_esnotify(dbref speaker, na_lookup func __attribute__ ((__unused__)),
            void *fdata __attribute__ ((__unused__)), int para)
{
  char *dest, *bp;
  bp = dest = mush_malloc(BUFFER_LEN, "string");

  if (!GoodObject(speaker))
    *dest = '\0';
  else if (para) {
    if (speaker == Owner(speaker))
      safe_format(dest, &bp, "[%s(#%d)] ", Name(speaker), speaker);
    else
      safe_format(dest, &bp, "[%s(#%d)'s %s(#%d)] ", Name(Owner(speaker)),
                  Owner(speaker), Name(speaker), speaker);
  } else
    safe_format(dest, &bp, "[%s:] ", spname(speaker));
  *bp = '\0';
  return dest;
}
