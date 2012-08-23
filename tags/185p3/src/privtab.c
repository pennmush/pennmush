/**
 * \file privtab.c
 *
 * \brief Privilege tables for PennMUSH.
 *
 * A privilege table is a respresentation of different privilege
 * flags with associated names, characters, and bitmasks.
 *
 */

#include "copyrite.h"
#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "mushtype.h"
#include "privtab.h"
#include "externs.h"
#include "confmagic.h"


/** Convert a string to a set of privilege bits, masked by an original set.
 * Given a privs table, a string, and an original set of privileges,
 * return a modified set of privileges by applying the privs in the
 * string to the original set of privileges. IF A SINGLE WORD STRING
 * IS GIVEN AND IT ISN'T THE NAME OF A PRIV, PARSE IT AS INDIVIDUAL
 * PRIV CHARS.
 * \param table pointer to a privtab.
 * \param str a space-separated string of privilege names to apply.
 * \param origprivs the original privileges.
 * \return a privilege bitmask.
 */
privbits
string_to_privs(PRIV *table, const char *str, privbits origprivs)
{
  PRIV *c;
  privbits yes = 0;
  privbits no = 0;
  privbits ltr = 0;
  char *p, *r;
  char tbuf1[BUFFER_LEN];
  bool not;
  int words = 0;

  if (!str || !*str)
    return origprivs;
  strcpy(tbuf1, str);
  r = trim_space_sep(tbuf1, ' ');
  while ((p = split_token(&r, ' '))) {
    words++;
    not = 0;
    if (*p == '!') {
      not = 1;
      if (!*++p)
        continue;
    }
    ltr = 0;
    if (strlen(p) == 1) {
      /* One-letter string is treated as a character if possible */
      ltr = letter_to_privs(table, p, 0);
      if (not)
        no |= ltr;
      else
        yes |= ltr;
    }
    /* If we didn't handle a one-char string as a character,
     * or if the string is longer than one char, use prefix-matching
     */
    if (!ltr) {
      for (c = table; c->name; c++) {
        if (string_prefix(c->name, p)) {
          if (not)
            no |= c->bits_to_set;
          else
            yes |= c->bits_to_set;
          break;
        }
      }
    }
  }
  /* If we made no changes, and were given one word, 
   * we probably were given letters instead */
  if (!no && !yes && (words == 1))
    return letter_to_privs(table, str, origprivs);
  return ((origprivs | yes) & ~no);
}

/** Convert a list to a set of privilege bits, masked by an original set.
 * Given a privs table, a list, and an original set of privileges,
 * return a modified set of privileges by applying the privs in the
 * string to the original set of privileges. No prefix-matching is
 * permitted in this list.
 * \param table pointer to a privtab.
 * \param str a space-separated string of privilege names to apply.
 * \param origprivs the original privileges.
 * \return a privilege bitmask.
 */
privbits
list_to_privs(PRIV *table, const char *str, privbits origprivs)
{
  PRIV *c;
  privbits yes = 0;
  privbits no = 0;
  char *p, *r;
  char tbuf1[BUFFER_LEN];
  bool not;
  int words = 0;

  if (!str || !*str)
    return origprivs;
  strcpy(tbuf1, str);
  r = trim_space_sep(tbuf1, ' ');
  while ((p = split_token(&r, ' '))) {
    words++;
    not = 0;
    if (*p == '!') {
      not = 1;
      if (!*++p)
        continue;
    }
    for (c = table; c->name; c++) {
      if (!strcasecmp(c->name, p)) {
        if (not)
          no |= c->bits_to_set;
        else
          yes |= c->bits_to_set;
        break;
      }
    }
  }
  return ((origprivs | yes) & ~no);
}


/** Convert a string to 2 sets of privilege bits, privs to set and
 * privs to clear.
 * \param table pointer to a privtab.
 * \param str a space-separated string of privilege names to apply.
 * \param setprivs pointer to address to store privileges to set.
 * \param clrprivs pointer to address to store privileges to clear.
 * \retval 1 string successfully parsed for bits with no errors.
 * \retval 0 string contained no privs
 * \retval -1 string at least one name matched no privs.
 */
int
string_to_privsets(PRIV *table, const char *str, privbits *setprivs,
                   privbits *clrprivs)
{
  PRIV *c;
  char *p, *r;
  char tbuf1[BUFFER_LEN];
  bool not;
  privbits ltr;
  int words = 0;
  int err = 0;
  int found = 0;

  *setprivs = *clrprivs = 0;
  if (!str || !*str)
    return 0;
  strcpy(tbuf1, str);
  r = trim_space_sep(tbuf1, ' ');
  while ((p = split_token(&r, ' '))) {
    words++;
    not = 0;
    if (*p == '!') {
      not = 1;
      if (!*++p) {
        err = 1;
        continue;
      }
    }
    ltr = 0;
    if (strlen(p) == 1) {
      /* One-letter string is treated as a character if possible */
      ltr = letter_to_privs(table, p, 0);
      if (not)
        *clrprivs |= ltr;
      else
        *setprivs |= ltr;
    }
    if (ltr) {
      found++;
    } else {
      for (c = table; c->name; c++) {
        if (string_prefix(c->name, p)) {
          found++;
          if (not)
            *clrprivs |= c->bits_to_set;
          else
            *setprivs |= c->bits_to_set;
          break;
        }
      }
    }
  }
  if (err || (words != found))
    return -1;
  return 1;
}

/** Convert a letter string to a set of privilege bits, masked by an original set.
 * Given a privs table, a letter string, and an original set of privileges,
 * return a modified set of privileges by applying the privs in the
 * string to the original set of privileges.
 * \param table pointer to a privtab.
 * \param str a string of privilege letters to apply.
 * \param origprivs the original privileges.
 * \return a privilege bitmask.
 */
privbits
letter_to_privs(PRIV *table, const char *str, privbits origprivs)
{
  PRIV *c;
  privbits yes = 0, no = 0;
  const char *p;
  bool not;

  if (!str || !*str)
    return origprivs;

  for (p = str; *p; p++) {
    not = 0;
    if (*p == '!') {
      not = 1;
      if (!*++p)
        break;
    }
    for (c = table; c->name; c++) {
      if (c->letter == *p) {
        if (not)
          no |= c->bits_to_set;
        else
          yes |= c->bits_to_set;
        break;
      }
    }
  }
  return ((origprivs | yes) & ~no);
}

/** Given a table and a bitmask, return a privs string (static allocation).
 * \param table pointer to a privtab.
 * \param privs bitmask of privileges.
 * \return statically allocated space-separated string of priv names.
 */
const char *
privs_to_string(PRIV *table, privbits privs)
{
  PRIV *c;
  static char buf[BUFFER_LEN];
  char *bp;

  bp = buf;
  for (c = table; c->name; c++) {
    if (privs & c->bits_to_show) {
      if (bp != buf)
        safe_chr(' ', buf, &bp);
      safe_str(c->name, buf, &bp);
      privs &= ~c->bits_to_set;
    }
  }
  *bp = '\0';
  return buf;
}


/** Given a table and a bitmask, return a privs letter string (static allocation).
 * \param table pointer to a privtab.
 * \param privs bitmask of privileges.
 * \return statically allocated string of priv letters.
 */
const char *
privs_to_letters(PRIV *table, privbits privs)
{
  PRIV *c;
  static char buf[BUFFER_LEN];
  char *bp;

  bp = buf;
  for (c = table; c->name; c++) {
    if ((privs & c->bits_to_show) && c->letter) {
      safe_chr(c->letter, buf, &bp);
      privs &= ~c->bits_to_set;
    }
  }
  *bp = '\0';
  return buf;
}
