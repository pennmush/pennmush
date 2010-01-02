/**
 * \file unparse.c
 *
 * \brief Convert lots of things into strings for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <string.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#include <stdio.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "flags.h"
#include "lock.h"
#include "attrib.h"
#include "ansi.h"
#include "pueblo.h"
#include "parse.h"
#include "confmagic.h"

/* Hack added by Thorvald for object_header Pueblo */
static int couldunparse;

/** Format an object's name (and dbref and flags).
 * This is a wrapper for real_unparse() that conditionally applies
 * pueblo xch_cmd tags to make the object's name a hyperlink to examine
 * it.
 * \param player the looker.
 * \param loc the object being looked at.
 * \return static formatted object name string.
 */
const char *
unparse_object(dbref player, dbref loc)
{
  static PUEBLOBUFF;
  const char *result;
  result = real_unparse(player, loc, 0, 0, 0);
  if (couldunparse) {
    PUSE;
    tag_wrap("A", tprintf("XCH_CMD=\"examine #%d\"", loc), result);
    PEND;
    return pbuff;
  } else {
    return result;
  }
}

/** Format an object's name, obeying MYOPIC/ownership rules.
 * This is a wrapper for real_unparse() that conditionally applies
 * pueblo xch_cmd tags to make the object's name a hyperlink to examine
 * it.
 * \param player the looker.
 * \param loc the object being looked at.
 * \return static formatted object name string.
 */
const char *
unparse_object_myopic(dbref player, dbref loc)
{
  static PUEBLOBUFF;
  const char *result;
  result = real_unparse(player, loc, 1, 0, 1);
  if (couldunparse) {
    PUSE;
    tag_wrap("A", tprintf("XCH_CMD=\"examine #%d\"", loc), result);
    PEND;
    return pbuff;
  } else {
    return result;
  }
}

/** Format an object's name, obeying MYOPIC/ownership and NAMEFORMAT.
 * \verbatim
 * Like unparse_object, but tell real_unparse to use @NAMEFORMAT if present
 * This should only be used if we're looking at our container, to prevent
 * confusion in matching, since you can't match containers with their
 * names anyway, but only with 'here'.
 * \endverbatim
 * \param player the looker.
 * \param loc the object being looked at.
 * \return static formatted object name string.
 */
const char *
unparse_room(dbref player, dbref loc)
{
  static PUEBLOBUFF;
  const char *result;
  result = real_unparse(player, loc, 1, 1, 1);
  if (couldunparse) {
    PUSE;
    tag_wrap("A", tprintf("XCH_CMD=\"examine #%d\"", loc), result);
    PEND;
    return pbuff;
  } else {
    return result;
  }
}

/** Format an object's name in several ways.
 * This function does the real work of converting a dbref to a
 * name, possibly including the dbref and flags, possibly using
 * a nameformat or nameaccent if present.
 * \param player the looker.
 * \param loc dbref of the object being looked at.
 * \param obey_myopic if 0, always show Name(#xxxFLAGS); if 1, don't
 * do so if player is MYOPIC or doesn't own loc.
 * \param use_nameformat if 1, apply a NAMEFORMAT attribute if available.
 * \param use_nameaccent if 1, apply a NAMEACCENT attribute if available.
 * \return address of a static buffer containing the formatted name.
 */
const char *
real_unparse(dbref player, dbref loc, int obey_myopic, int use_nameformat,
             int use_nameaccent)
{
  static char buf[BUFFER_LEN], *bp;
  static char tbuf1[BUFFER_LEN];
  char *p;

  couldunparse = 0;
  if (!(GoodObject(loc) || (loc == NOTHING) || (loc == AMBIGUOUS) ||
        (loc == HOME)))
    return T("*NOTHING*");
  switch (loc) {
  case NOTHING:
    return T("*NOTHING*");
  case AMBIGUOUS:
    return T("*VARIABLE*");
  case HOME:
    return T("*HOME*");
  default:
    if (use_nameaccent)
      strcpy(tbuf1, accented_name(loc));
    else
      strcpy(tbuf1, Name(loc));
    if (IsExit(loc) && obey_myopic) {
      if ((p = strchr(tbuf1, ';')))
        *p = '\0';
    }
    if ((Can_Examine(player, loc) || can_link_to(player, loc) ||
         JumpOk(loc) || ChownOk(loc) || DestOk(loc)) &&
        (!Myopic(player) || !obey_myopic)) {
      /* show everything */
      if (SUPPORT_PUEBLO)
        couldunparse = 1;
      bp = buf;
      if (ANSI_NAMES && ShowAnsi(player))
        safe_format(buf, &bp, "%s%s%s(#%d%s)", ANSI_HILITE, tbuf1,
                    ANSI_END, loc, unparse_flags(loc, player));
      else
        safe_format(buf, &bp, "%s(#%d%s)", tbuf1, loc,
                    unparse_flags(loc, player));
      *bp = '\0';
    } else {
      /* show only the name */
      if (ANSI_NAMES && ShowAnsi(player)) {
        bp = buf;
        safe_format(buf, &bp, "%s%s%s", ANSI_HILITE, tbuf1, ANSI_END);
        *bp = '\0';
      } else
        strcpy(buf, tbuf1);
    }
  }
  /* buf now contains the default formatting of the name. If we
   * have @nameaccent, though, we might change to that.
   */
  if (use_nameformat && nameformat(player, loc, tbuf1, buf))
    return tbuf1;
  else
    return buf;
}

/** Build the name of loc as seen by a player inside it, but only
 * if it has a NAMEFORMAT.
 * This function needs to avoid using a static buffer, so pass in 
 * a pointer to an allocated BUFFER_LEN array.
 * \param player the looker.
 * \param loc dbref of location being looked at.
 * \param tbuf1 address to store formatted name of loc.
 * \param defname the name as it would be formatted without NAMEFORMAT.
 * \retval 1 a NAMEFORMAT was found, and tbuf1 contains formatted name.
 * \retval 0 no NAMEFORMAT on loc, tbuf1 is undefined.
 */
int
nameformat(dbref player, dbref loc, char *tbuf1, char *defname)
{
  ATTR *a;
  char *wsave[10], *rsave[NUMQ];
  char *arg, *bp, *arg2;
  char const *sp;
  char *save;

  int j;
  a = atr_get(loc, "NAMEFORMAT");
  if (a) {
    arg = (char *) mush_malloc(BUFFER_LEN, "string");
    arg2 = (char *) mush_malloc(BUFFER_LEN, "string");
    if (!arg)
      mush_panic("Unable to allocate memory in nameformat");
    save_global_regs("nameformat", rsave);
    for (j = 0; j < 10; j++) {
      wsave[j] = global_eval_context.wenv[j];
      global_eval_context.wenv[j] = NULL;
    }
    for (j = 0; j < NUMQ; j++)
      global_eval_context.renv[j][0] = '\0';
    strcpy(arg, unparse_dbref(loc));
    global_eval_context.wenv[0] = arg;
    strcpy(arg2, defname);
    global_eval_context.wenv[1] = arg2;
    sp = save = safe_atr_value(a);
    bp = tbuf1;
    process_expression(tbuf1, &bp, &sp, loc, player, player,
                       PE_DEFAULT, PT_DEFAULT, NULL);
    *bp = '\0';
    free(save);
    for (j = 0; j < 10; j++) {
      global_eval_context.wenv[j] = wsave[j];
    }
    restore_global_regs("nameformat", rsave);
    mush_free(arg, "string");
    mush_free(arg2, "string");
    return 1;
  } else {
    /* No @nameformat attribute */
    return 0;
  }
}

/** Give a string representation of a dbref.
 * \param num value to stringify
 * \return address of static buffer containing stringified value.
 */
char *
unparse_dbref(dbref num)
{
  /* Not BUFFER_LEN, but no dbref will come near this long */
  static char str[SBUF_LEN];
  char *strp;

  strp = str;
  safe_dbref(num, str, &strp);
  *strp = '\0';
  return str;
}

/** Give a string representation of an integer.
 * \param num value to stringify
 * \return address of static buffer containing stringified value.
 */
char *
unparse_integer(intmax_t num)
{
  static char str[SBUF_LEN];
  char *strp;

  strp = str;
  safe_integer_sbuf(num, str, &strp);
  *strp = '\0';
  return str;
}

/** Give a string representation of an unsigned integer.
 * \param num value to stringify
 * \return address of static buffer containing stringified value.
 */
char *
unparse_uinteger(uintmax_t num)
{
  static char str[128];
#ifndef PRIuMAX
  /* Probably not right */
#define PRIuMAX "lld"
#endif
  sprintf(str, "%" PRIuMAX, num);
  return str;
}

/** Give a string representation of a number.
 * \param num value to stringify
 * \return address of static buffer containing stringified value.
 */
char *
unparse_number(NVAL num)
{
  /* 100 is NOT large enough for even the huge floats */
  static char str[1000];        /* Should be large enough for even the HUGE floats */
  char *p;
  snprintf(str, 1000, "%.*f", FLOAT_PRECISION, num);

  if ((p = strchr(str, '.'))) {
    p += strlen(p);
    while (p[-1] == '0')
      p--;
    if (p[-1] == '.')
      p--;
    *p = '\0';
  }
  return str;
}

/** Return the name of an object, applying NAMEACCENT if set.
 * \param thing dbref of object.
 * \return address of static buffer containing object name, with accents
 * if object has a valid NAMEACCENT attribute.
 */
const char *
accented_name(dbref thing)
{
  ATTR *na;
  static char fbuf[BUFFER_LEN];

  na = atr_get(thing, "NAMEACCENT");
  if (!na)
    return Name(thing);
  else {
    char tbuf[BUFFER_LEN];
    char *bp = fbuf;
    size_t len;

    strcpy(tbuf, atr_value(na));

    len = strlen(Name(thing));

    if (len != strlen(tbuf))
      return Name(thing);

    safe_accent(Name(thing), tbuf, len, fbuf, &bp);
    *bp = '\0';

    return fbuf;
  }
}
