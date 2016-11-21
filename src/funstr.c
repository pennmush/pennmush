/**
 * \file funstr.c
 *
 * \brief String functions for mushcode.
 *
 *
 */

#include "copyrite.h"

#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include "ansi.h"
#include "attrib.h"
#include "case.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "flags.h"
#include "htab.h"
#include "lock.h"
#include "markup.h"
#include "match.h"
#include "mushdb.h"
#include "notify.h"
#include "parse.h"
#include "pueblo.h"
#include "sort.h"
#include "strutil.h"


#ifdef WIN32
#pragma warning( disable : 4761)        /* NJG: disable warning re conversion */
#endif

#define MAX_COLS 32  /**< Maximum number of columns for align() */
static int wraplen(char *str, size_t maxlen);
static int align_one_line(char *buff, char **bp, int ncols,
                          int cols[MAX_COLS], int calign[MAX_COLS],
                          char *ptrs[MAX_COLS],
                          ansi_string *as[MAX_COLS], ansi_data adata[MAX_COLS],
                          int linenum, char *fieldsep, int fslen, char *linesep,
                          int lslen, char filler);
static int comp_gencomp(dbref executor, char *left, char *right,
                        const char *type);
void init_pronouns(void);

/** Return an indicator of a player's gender.
 * \param player player whose gender is to be checked.
 * \retval 0 neuter.
 * \retval 1 female.
 * \retval 2 male.
 * \retval 3 plural.
 */
int
get_gender(dbref player)
{
  ATTR *a;

  a = atr_get(player, "SEX");

  if (!a)
    return 0;

  switch (*atr_value(a)) {
  case 'T':
  case 't':
  case 'P':
  case 'p':
    return 3;
  case 'M':
  case 'm':
    return 2;
  case 'F':
  case 'f':
  case 'W':
  case 'w':
    return 1;
  default:
    return 0;
  }
}

char *subj[4];  /**< Subjective pronouns */
char *poss[4];  /**< Possessive pronouns */
char *obj[4];   /**< Objective pronouns */
char *absp[4];  /**< Absolute possessive pronouns */

/** Macro to set a pronoun entry based on whether we're translating or not */
#define SET_PRONOUN(p,v,u)  p = strdup((translate) ? (v) : (u))

/** Initialize the pronoun translation strings.
 * This function sets up the values of the arrays of subjective,
 * possessive, objective, and absolute possessive pronouns with
 * locale-appropriate values.
 */
void
init_pronouns(void)
{
  int translate = 0;
#ifdef LC_MESSAGES
  char *loc;
  if ((loc = setlocale(LC_MESSAGES, NULL))) {
    if (strcmp(loc, "C") && strncmp(loc, "en", 2))
      translate = 1;
  }
#endif
  SET_PRONOUN(subj[0], T("pronoun:neuter,subjective"), "it");
  SET_PRONOUN(subj[1], T("pronoun:feminine,subjective"), "she");
  SET_PRONOUN(subj[2], T("pronoun:masculine,subjective"), "he");
  SET_PRONOUN(subj[3], T("pronoun:plural,subjective"), "they");
  SET_PRONOUN(poss[0], T("pronoun:neuter,possessive"), "its");
  SET_PRONOUN(poss[1], T("pronoun:feminine,possessive"), "her");
  SET_PRONOUN(poss[2], T("pronoun:masculine,possessive"), "his");
  SET_PRONOUN(poss[3], T("pronoun:plural,possessive"), "their");
  SET_PRONOUN(obj[0], T("pronoun:neuter,objective"), "it");
  SET_PRONOUN(obj[1], T("pronoun:feminine,objective"), "her");
  SET_PRONOUN(obj[2], T("pronoun:masculine,objective"), "him");
  SET_PRONOUN(obj[3], T("pronoun:plural,objective"), "them");
  SET_PRONOUN(absp[0], T("pronoun:neuter,absolute possessive"), "its");
  SET_PRONOUN(absp[1], T("pronoun:feminine,absolute possessive"), "hers");
  SET_PRONOUN(absp[2], T("pronoun:masculine,absolute possessive"), "his");
  SET_PRONOUN(absp[3], T("pronoun:plural,absolute possessive "), "theirs");
}

#undef SET_PRONOUN

/* ARGSUSED */
FUNCTION(fun_isword)
{
  /* is every character a letter? */
  char *p;
  if (!args[0] || !*args[0]) {
    safe_chr('0', buff, bp);
    return;
  }
  for (p = args[0]; *p; p++) {
    if (!isalpha(*p)) {
      safe_chr('0', buff, bp);
      return;
    }
  }
  safe_chr('1', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_capstr)
{
  char *p = args[0];
  WALK_ANSI_STRING(p) {
    *p = UPCASE(*p);
    break;
  }
  safe_strl(args[0], arglens[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_art)
{
  /* checks a word and returns the appropriate article, "a" or "an" */
  char c;

  if (!*args[0]) {
    safe_chr('a', buff, bp);
    return;
  }
  c = DOWNCASE(*args[0]);
  if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
    safe_str("an", buff, bp);
  else
    safe_chr('a', buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_subj)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(subj[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_poss)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(poss[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_obj)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(obj[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_aposs)
{
  dbref thing;

  thing = match_thing(executor, args[0]);
  if (thing == NOTHING) {
    safe_str(T(e_match), buff, bp);
    return;
  }
  safe_str(absp[get_gender(thing)], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_alphamax)
{
  int j, m = 0;

  for (j = 1; j < nargs; j++) {
    if (strcoll(args[m], args[j]) < 0) {
      m = j;
    }
  }
  safe_strl(args[m], arglens[m], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_alphamin)
{
  int j, m = 0;

  for (j = 1; j < nargs; j++) {
    if (strcoll(args[m], args[j]) > 0) {
      m = j;
    }
  }
  safe_strl(args[m], arglens[m], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_strlen)
{
  safe_integer(ansi_strlen(args[0]), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_mid)
{
  ansi_string *as;
  int pos, len;

  if (!is_integer(args[1]) || !is_integer(args[2])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  pos = parse_integer(args[1]);
  len = parse_integer(args[2]);

  if (pos < 0) {
    safe_str(T(e_range), buff, bp);
    free_ansi_string(as);
    return;
  }

  if (len < 0) {
    pos = pos + len + 1;
    if (pos < 0)
      pos = 0;
    len = -len;
  }

  safe_ansi_string(as, pos, len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_left)
{
  int len;
  ansi_string *as;

  if (!is_integer(args[1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  len = parse_integer(args[1]);

  if (len < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  safe_ansi_string(as, 0, len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_right)
{
  int len;
  ansi_string *as;

  if (!is_integer(args[1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  len = parse_integer(args[1]);

  if (len < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  if (len > as->len)
    safe_strl(args[0], arglens[0], buff, bp);
  else
    safe_ansi_string(as, as->len - len, len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_delete)
{
  ansi_string *as;
  int pos, num;

  if (!is_integer(args[1]) || !is_integer(args[2])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }

  pos = parse_integer(args[1]);
  num = parse_integer(args[2]);

  if (pos < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);

  if (pos > as->len || num == 0) {
    safe_strl(args[0], arglens[0], buff, bp);
    free_ansi_string(as);
    return;
  }

  if (num < 0) {
    pos += num + 1;
    if (pos < 0)
      pos = 0;
  }

  ansi_string_delete(as, pos, num);
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_str_rep_or_ins)
{
  ansi_string *dst, *src;
  int start = 0, len = 0;
  int srcarg;
  bool inserting = 1;

  if (!is_integer(args[1])) {
    safe_str(T(e_ints), buff, bp);
    return;
  }
  start = parse_integer(args[1]);

  if (start < 0) {
    safe_str(T(e_argrange), buff, bp);
    return;
  }

  if (!strcmp(called_as, "STRREPLACE")) {
    /* strreplace - arg 2 is a length, arg
     * 3 is the string to add */
    if (!is_integer(args[2])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    len = parse_integer(args[2]);
    if (len < 0) {
      safe_str(T(e_argrange), buff, bp);
      return;
    }
    srcarg = 3;
    inserting = 0;
  } else {
    /* strinsert() has no length, arg 2 is
     * the string to add */
    srcarg = 2;
  }

  dst = parse_ansi_string(args[0]);
  if (start > dst->len) {
    safe_strl(args[0], arglens[0], buff, bp);
    if (inserting)
      safe_strl(args[srcarg], arglens[srcarg], buff, bp);

    free_ansi_string(dst);
    return;
  }

  src = parse_ansi_string(args[srcarg]);

  ansi_string_replace(dst, start, len, src);

  safe_ansi_string(dst, 0, dst->len, buff, bp);
  free_ansi_string(dst);
  free_ansi_string(src);
}

extern int sort_order;          /* from sort.c */

static int
comp_gencomp(dbref executor, char *left, char *right, const char *type)
{
  int c;
  c = gencomp(executor, left, right, type);
  return (c > 0 ? 1 : (c < 0 ? -1 : 0)) * sort_order;
}

/* ARGSUSED */
FUNCTION(fun_comp)
{
  char type = 'A';

  if (nargs == 3 && !(args[2] && *args[2])) {
    safe_str(T("#-1 INVALID THIRD ARGUMENT"), buff, bp);
    return;
  } else if (nargs == 3) {
    type = UPCASE(*args[2]);
  }

  switch (type) {
  case 'A':                    /* Case-sensitive lexicographic */
    {
      safe_integer(comp_gencomp(executor, args[0], args[1], ALPHANUM_LIST),
                   buff, bp);
      return;
    }
  case 'I':                    /* Case-insensitive lexicographic */
    {
      safe_integer(comp_gencomp
                   (executor, args[0], args[1], INSENS_ALPHANUM_LIST), buff,
                   bp);
      return;
    }
  case 'N':                    /* Integers */
    if (!is_strict_integer(args[0]) || !is_strict_integer(args[1])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    safe_integer(comp_gencomp(executor, args[0], args[1], NUMERIC_LIST), buff,
                 bp);
    return;
  case 'F':
    if (!is_strict_number(args[0]) || !is_strict_number(args[1])) {
      safe_str(T(e_nums), buff, bp);
      return;
    }
    safe_integer(comp_gencomp(executor, args[0], args[1], FLOAT_LIST), buff,
                 bp);
    return;
  case 'D':
    {
      dbref a, b;
      a = parse_objid(args[0]);
      b = parse_objid(args[1]);
      if (a == NOTHING || b == NOTHING) {
        safe_str(T("#-1 INVALID DBREF"), buff, bp);
        return;
      }
      safe_integer(comp_gencomp(executor, args[0], args[1], DBREF_LIST), buff,
                   bp);
      return;
    }
  default:
    safe_str(T("#-1 INVALID THIRD ARGUMENT"), buff, bp);
    return;
  }
}

/* ARGSUSED */
FUNCTION(fun_pos)
{
  char *pos;

  pos = strstr(args[1], args[0]);
  if (pos)
    safe_integer(pos - args[1] + 1, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_lpos)
{
  char c = ' ';
  int first = 1, n;

  if (args[1][0])
    c = args[1][0];

  for (n = 0; n < arglens[0]; n++)
    if (args[0][n] == c) {
      if (first)
        first = 0;
      else
        safe_chr(' ', buff, bp);
      safe_integer(n, buff, bp);
    }
}

/* ARGSUSED */
FUNCTION(fun_strmatch)
{
  char *ret[36];
  int matches;
  int i;
  char *qregs[NUMQ];
  int nqregs;
  char match_space[BUFFER_LEN * 2];
  ssize_t match_space_len = BUFFER_LEN * 2;

  /* matches a wildcard pattern for an _entire_ string */

  if (nargs > 2) {
    matches = wild_match_case_r(args[1], args[0], 0, ret,
                                NUMQ, match_space, match_space_len, NULL, 0);
    safe_boolean(matches, buff, bp);

    if (matches) {
      /* Now, assign the captures, if we have returns. */
      nqregs = list2arr(qregs, NUMQ, args[2], ' ', 0);

      for (i = 0; i < nqregs; i++) {
        if (ValidQregName(qregs[i])) {
          PE_Setq(pe_info, qregs[i], ret[i]);
        } else if (qregs[i][0] != '-' || qregs[i][1]) {
          safe_str(T(e_badregname), buff, bp);
        }
      }
    }
  } else {
    matches = wild_match_case_r(args[1], args[0], 0, NULL, 0, NULL, 0, NULL, 0);
    safe_boolean(matches, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_strcat)
{
  int j;

  for (j = 0; j < nargs; j++)
    safe_strl(args[j], arglens[j], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_flip)
{
  ansi_string *as;
  as = parse_ansi_string(args[0]);
  flip_ansi_string(as);
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}


/* ARGSUSED */
FUNCTION(fun_merge)
{
  /* given s1, s2, and a list of characters, for each character in s1,
   * if the char is in the list, replace it with the corresponding
   * char in s2.
   */

  int i, j;
  size_t len;
  char *ptr = args[0];
  char matched[UCHAR_MAX + 1];
  ansi_string *as;

  memset(matched, 0, sizeof matched);

  /* find the characters to look for */
  if (!args[2] || !*args[2])
    matched[' '] = 1;
  else {
    char *p;
    for (p = remove_markup(args[2], &len); p && *p; p++)
      matched[*p] = 1;
  }

  as = parse_ansi_string(args[1]);

  /* do length checks first */
  if (as->len != ansi_strlen(ptr)) {
    safe_str(T("#-1 STRING LENGTHS MUST BE EQUAL"), buff, bp);
    free_ansi_string(as);
    return;
  }

  /* walk strings, copy from the appropriate string */
  i = 0;
  ptr = args[0];
  while (*ptr) {
    switch (*ptr) {
    case ESC_CHAR:
      while (*ptr && *ptr != 'm') {
        safe_chr(*(ptr++), buff, bp);
      }
      safe_chr(*(ptr++), buff, bp);
      break;
    case TAG_START:
    case TAG_END:
      while (*ptr && *ptr != TAG_END) {
        safe_chr(*(ptr++), buff, bp);
      }
      safe_chr(*(ptr++), buff, bp);
      break;
    default:
      if (matched[*ptr]) {
        j = 0;
        while (*ptr && matched[*ptr]) {
          ptr++;
          j++;
        }
        if (j != 0) {
          safe_ansi_string(as, i, j, buff, bp);
          i += j;
        }
      } else {
        i++;
        safe_chr(*(ptr++), buff, bp);
      }
    }
  }
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_tr)
{
  /* given str, s1, s2, for each character in str, if the char
   * is in s1, replace it with the char at the same index in s2.
   */

  char charmap[256];
  char instr[BUFFER_LEN], outstr[BUFFER_LEN];
  char *ip, *op;
  size_t i, len;
  char cur, dest;
  char *c;
  ansi_string *as;

  /* Initialize */
  for (i = 0; i < 256; i++) {
    charmap[i] = (char) i;
  }

#define goodchr(x) (isprint(x) || (x == '\n'))
  /* Convert ranges in input string, and check that
   * we don't receive a nonprinting char such as
   * beep() */
  ip = instr;
  c = remove_markup(args[1], NULL);
  while (*c) {
    cur = *c;
    if (!goodchr(cur)) {
      safe_str(T("#-1 TR CANNOT ACCEPT NONPRINTING CHARS"), buff, bp);
      return;
    }
    /* Tack it onto the string */
    /* Do we have a range? */
    if (*(c + 1) && *(c + 1) == '-' && *(c + 2)) {
      dest = *(c + 2);
      if (!goodchr(dest)) {
        safe_str(T("#-1 TR CANNOT ACCEPT NONPRINTING CHARS"), buff, bp);
        return;
      }
      if (dest > cur) {
        for (; cur <= dest; cur++) {
          if (goodchr(cur))
            safe_chr((char) cur, instr, &ip);
        }
      } else {
        for (; cur >= dest; cur--) {
          if (goodchr(cur))
            safe_chr((char) cur, instr, &ip);
        }
      }
      c += 3;
    } else {
      safe_chr((char) cur, instr, &ip);
      c++;
    }
  }
  *ip = '\0';

  /* Convert ranges in output string, and check that
   * we don't receive a nonprinting char such as
   * beep() */
  op = outstr;
  c = remove_markup(args[2], NULL);
  while (*c) {
    cur = *c;
    if (!goodchr(cur)) {
      safe_str(T("#-1 TR CANNOT ACCEPT NONPRINTING CHARS"), buff, bp);
      return;
    }
    /* Tack it onto the string */
    /* Do we have a range? */
    if (*(c + 1) && *(c + 1) == '-' && *(c + 2)) {
      dest = *(c + 2);
      if (!goodchr(dest)) {
        safe_str(T("#-1 TR CANNOT ACCEPT NONPRINTING CHARS"), buff, bp);
        return;
      }
      if (dest > cur) {
        for (; cur <= dest; cur++) {
          if (goodchr(cur))
            safe_chr((char) cur, outstr, &op);
        }
      } else {
        for (; cur >= dest; cur--) {
          if (goodchr(cur))
            safe_chr((char) cur, outstr, &op);
        }
      }
      c += 3;
    } else {
      safe_chr((char) cur, outstr, &op);
      c++;
    }
  }
  *op = '\0';
#undef goodchr

  if ((ip - instr) != (op - outstr)) {
    safe_str(T("#-1 STRING LENGTHS MUST BE EQUAL"), buff, bp);
    return;
  }

  len = ip - instr;

  for (i = 0; i < len; i++)
    charmap[instr[i]] = outstr[i];

  /* walk the string, translating characters */
  as = parse_ansi_string(args[0]);
  len = as->len;
  for (i = 0; i < len; i++) {
    as->text[i] = charmap[as->text[i]];
  }
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_lcstr)
{
  char *p;
  p = args[0];
  WALK_ANSI_STRING(p) {
    *p = DOWNCASE(*p);
    p++;
  }
  safe_str(args[0], buff, bp);
  return;
}

/* ARGSUSED */
FUNCTION(fun_ucstr)
{
  char *p;
  p = args[0];
  WALK_ANSI_STRING(p) {
    *p = UPCASE(*p);
    p++;
  }
  safe_str(args[0], buff, bp);
  return;
}

/* ARGSUSED */
FUNCTION(fun_repeat)
{
  int times;
  char *ap;

  if (!is_integer(args[1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  times = parse_integer(args[1]);
  if (times < 0) {
    safe_str(T("#-1 ARGUMENT MUST BE NON-NEGATIVE INTEGER"), buff, bp);
    return;
  }
  if (!*args[0])
    return;

  /* Special-case repeating one character */
  if (arglens[0] == 1) {
    safe_fill(args[0][0], times, buff, bp);
    return;
  }

  /* Do the repeat in O(lg n) time. */
  /* This takes advantage of the fact that we're given a BUFFER_LEN
   * buffer for args[0] that we are free to trash.  Huzzah! */
  ap = args[0] + arglens[0];
  while (times) {
    if (times & 1) {
      if (safe_strl(args[0], arglens[0], buff, bp)) {
        char *ts, *te;
        ts = strrchr(buff, TAG_START);
        te = strrchr(buff, TAG_END);
        if (ts && te && ts > te) {
          *ts = '\0';
          bp = &ts;
        }
        break;
      }
    }
    times = times >> 1;
    if (safe_str(args[0], args[0], &ap)) {
      char *ts, *te;
      *ap = '\0';
      ts = strrchr(args[0], TAG_START);
      te = strrchr(args[0], TAG_END);
      if (times > 1)
        times = 1;
      if (ts && te && ts > te)
        *ts = '\0';
    } else {
      *ap = '\0';
    }
    arglens[0] = strlen(args[0]);
  }
}

/* ARGSUSED */
FUNCTION(fun_scramble)
{
  ansi_string *as;

  if (!*args[0])
    return;

  as = parse_ansi_string(args[0]);
  scramble_ansi_string(as);

  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_ljust)
{
  /* pads a string with trailing blanks (or other fill character) */

  size_t spaces, len;
  ansi_string *as;
  int fillq, fillr, i;
  char fillstr[BUFFER_LEN], *fp;

  if (!is_uinteger(args[1])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  len = ansi_strlen(args[0]);
  spaces = parse_uinteger(args[1]);
  if (spaces >= BUFFER_LEN)
    spaces = BUFFER_LEN - 1;

  if (len >= spaces) {
    /* Check to see if we should truncate */
    if (nargs > 3 && parse_boolean(args[3])) {
      if (has_markup(args[0])) {
        as = parse_ansi_string(args[0]);
        safe_ansi_string(as, 0, spaces, buff, bp);
        free_ansi_string(as);
      } else {
        safe_strl(args[0], spaces, buff, bp);
      }
    } else {
      safe_strl(args[0], arglens[0], buff, bp);
    }
    return;
  }

  spaces -= len;

  if (!args[2] || !*args[2]) {
    /* Fill with spaces */
    safe_strl(args[0], arglens[0], buff, bp);
    safe_fill(' ', spaces, buff, bp);
    return;
  }
  len = ansi_strlen(args[2]);
  if (!len) {
    safe_str(T("#-1 FILL ARGUMENT MAY NOT BE ZERO-LENGTH"), buff, bp);
    return;
  }
  safe_strl(args[0], arglens[0], buff, bp);
  as = parse_ansi_string(args[2]);
  fillq = spaces / len;
  fillr = spaces % len;
  fp = fillstr;
  for (i = 0; i < fillq; i++)
    safe_ansi_string(as, 0, as->len, fillstr, &fp);
  safe_ansi_string(as, 0, fillr, fillstr, &fp);
  *fp = '\0';
  free_ansi_string(as);
  safe_str(fillstr, buff, bp);

}

/* ARGSUSED */
FUNCTION(fun_rjust)
{
  /* pads a string with leading blanks (or other fill character) */

  size_t spaces, len;
  ansi_string *as;
  int fillq, fillr, i;
  char fillstr[BUFFER_LEN], *fp;


  if (!is_uinteger(args[1])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  len = ansi_strlen(args[0]);
  spaces = parse_uinteger(args[1]);
  if (spaces >= BUFFER_LEN)
    spaces = BUFFER_LEN - 1;

  if (len >= spaces) {
    /* Check to see if we should truncate */
    if (nargs > 3 && parse_boolean(args[3])) {
      if (has_markup(args[0])) {
        as = parse_ansi_string(args[0]);
        safe_ansi_string(as, 0, spaces, buff, bp);
        free_ansi_string(as);
      } else {
        safe_strl(args[0], spaces, buff, bp);
      }
    } else {
      safe_strl(args[0], arglens[0], buff, bp);
    }
    return;
  }

  spaces -= len;

  if (!args[2] || !*args[2]) {
    /* Fill with spaces */
    safe_fill(' ', spaces, buff, bp);
    safe_strl(args[0], arglens[0], buff, bp);
    return;
  }
  len = ansi_strlen(args[2]);
  if (!len) {
    safe_str(T("#-1 FILL ARGUMENT MAY NOT BE ZERO-LENGTH"), buff, bp);
    return;
  }
  as = parse_ansi_string(args[2]);
  fillq = spaces / len;
  fillr = spaces % len;
  fp = fillstr;
  for (i = 0; i < fillq; i++)
    safe_ansi_string(as, 0, as->len, fillstr, &fp);
  safe_ansi_string(as, 0, fillr, fillstr, &fp);
  *fp = '\0';
  free_ansi_string(as);
  safe_str(fillstr, buff, bp);
  safe_strl(args[0], arglens[0], buff, bp);

}

/* ARGSUSED */
FUNCTION(fun_center)
{
  /* pads a string with leading blanks (or other fill string) */
  size_t width, len, lsp, rsp, filllen;
  int fillq, fillr, i;
  char fillstr[BUFFER_LEN], *fp;
  ansi_string *as;

  if (!is_uinteger(args[1])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  width = parse_uinteger(args[1]);
  len = ansi_strlen(args[0]);
  if (len >= width) {
    safe_strl(args[0], arglens[0], buff, bp);
    return;
  }
  lsp = rsp = (width - len) / 2;
  rsp += (width - len) % 2;
  if (lsp >= BUFFER_LEN)
    lsp = rsp = BUFFER_LEN - 1;

  if ((!args[2] || !*args[2]) && (!args[3] || !*args[3])) {
    /* Fast case for default fill with spaces */
    safe_fill(' ', lsp, buff, bp);
    safe_strl(args[0], arglens[0], buff, bp);
    safe_fill(' ', rsp, buff, bp);
    return;
  }

  /* args[2] contains the possibly ansi, multi-char fill string */
  filllen = ansi_strlen(args[2]);
  if (!filllen) {
    safe_str(T("#-1 FILL ARGUMENT MAY NOT BE ZERO-LENGTH"), buff, bp);
    return;
  }
  as = parse_ansi_string(args[2]);
  fillq = lsp / filllen;
  fillr = lsp % filllen;
  fp = fillstr;
  for (i = 0; i < fillq; i++)
    safe_ansi_string(as, 0, as->len, fillstr, &fp);
  safe_ansi_string(as, 0, fillr, fillstr, &fp);
  *fp = '\0';
  free_ansi_string(as);
  safe_str(fillstr, buff, bp);
  safe_strl(args[0], arglens[0], buff, bp);
  /* If we have args[3], that's the right-side fill string */
  if (nargs > 3) {
    if (args[3] && *args[3]) {
      filllen = ansi_strlen(args[3]);
      if (!filllen) {
        safe_str(T("#-1 FILL ARGUMENT MAY NOT BE ZERO-LENGTH"), buff, bp);
        return;
      }
      as = parse_ansi_string(args[3]);
      fillq = rsp / filllen;
      fillr = rsp % filllen;
      fp = fillstr;
      for (i = 0; i < fillq; i++)
        safe_ansi_string(as, 0, as->len, fillstr, &fp);
      safe_ansi_string(as, 0, fillr, fillstr, &fp);
      *fp = '\0';
      free_ansi_string(as);
      safe_str(fillstr, buff, bp);
    } else {
      /* Null args[3], fill right side with spaces */
      safe_fill(' ', rsp, buff, bp);
    }
    return;
  }
  /* No args[3], so we flip args[2] */
  filllen = ansi_strlen(args[2]);
  as = parse_ansi_string(args[2]);
  fillq = rsp / filllen;
  fillr = rsp % filllen;
  fp = fillstr;
  for (i = 0; i < fillq; i++)
    safe_ansi_string(as, 0, as->len, fillstr, &fp);
  safe_ansi_string(as, 0, fillr, fillstr, &fp);
  *fp = '\0';
  free_ansi_string(as);
  as = parse_ansi_string(fillstr);
  flip_ansi_string(as);
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_foreach)
{
  /* Like map(), but it operates on a string, rather than on a list,
   * calling a user-defined function for each character in the string.
   * No delimiter is inserted between the results.
   */

  ansi_string *as;
  char *lp;
  char cbuf[BUFFER_LEN], *cp;
  PE_REGS *pe_regs;
  int placenr = 0;
  int funccount;
  char *oldbp;
  char start, end;
  char result[BUFFER_LEN];
  char placestr[10];
  ufun_attrib ufun;

  if (nargs >= 3) {
    if (!delim_check(buff, bp, nargs, args, 3, &start))
      return;
  }

  if (nargs == 4) {
    if (!delim_check(buff, bp, nargs, args, 4, &end))
      return;
  } else {
    end = '\0';
  }

  if (!fetch_ufun_attrib
      (args[0], executor, &ufun, UFUN_DEFAULT | UFUN_REQUIRE_ATTR))
    return;

  as = parse_ansi_string(args[1]);

  lp = as->text;
  if (nargs >= 3) {
    lp = strchr(as->text, start);

    if (!lp) {
      safe_str(args[1], buff, bp);
      free_ansi_string(as);
      return;
    }

    safe_ansi_string(as, 0, (lp++ - as->text), buff, bp);
    placenr = (lp - as->text);
  }

  cbuf[1] = '\0';

  oldbp = *bp;
  funccount = pe_info->fun_invocations;
  pe_regs = pe_regs_create(PE_REGS_ARG, "fun_foreach");
  pe_regs_setenv_nocopy(pe_regs, 0, cbuf);
  pe_regs_setenv_nocopy(pe_regs, 1, placestr);
  while (*lp) {
    if (*lp == end) {
      lp++;
      break;
    }
    /* Set env */
    cp = cbuf;
    safe_ansi_string(as, (lp - as->text), 1, cbuf, &cp);
    *cp = '\0';
    snprintf(placestr, 10, "%d", placenr++);
    lp++;
    if (call_ufun(&ufun, result, executor, enactor, pe_info, pe_regs))
      break;

    safe_str(result, buff, bp);

    /* Make sure we're actually doing stuff. */
    if (*bp == oldbp && pe_info->fun_invocations == funccount)
      break;
    oldbp = *bp;
    funccount = pe_info->fun_invocations;
  }
  placenr++;
  if (*lp)
    safe_ansi_string(as, (lp - as->text), as->len, buff, bp);
  pe_regs_free(pe_regs);
  free_ansi_string(as);
}

extern char escaped_chars[UCHAR_MAX + 1];

/* ARGSUSED */
FUNCTION(fun_decompose)
{
  /* This function simply returns a decompose'd version of
   * the included string, such that
   * s(decompose(str)) == str, down to the last space, tab,
   * and newline. */
  safe_decompose_str(args[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_secure)
{
  /* this function smashes all occurences of "unsafe" characters in a string.
   * "unsafe" characters are defined by the escaped_chars table.
   * these characters get replaced by spaces
   */
  char *p;

  for (p = args[0]; *p; p++)
    if (escaped_chars[*p])
      *p = ' ';

  safe_strl(args[0], arglens[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_escape)
{
  char *s;

  if (arglens[0]) {
    safe_chr('\\', buff, bp);
    for (s = args[0]; *s; s++) {
      if ((s != args[0]) && escaped_chars[*s])
        safe_chr('\\', buff, bp);
      safe_chr((char) *s, buff, bp);
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_trim)
{
  /* Similar to squish() but it doesn't trim spaces in the center, and
   * takes a delimiter argument and trim style.
   */


  enum trim_style { TRIM_LEFT, TRIM_RIGHT, TRIM_BOTH } trim;
  int trim_style_arg, trim_char_arg;
  ansi_string *as;
  int s, e;
  char *delims;
  char totrim[0x100];

  memset(totrim, 0, 0x100);

  /* Alas, PennMUSH and TinyMUSH used different orders for the arguments.
   * We'll give the users an option about it
   */
  if (!strcmp(called_as, "TRIMTINY")) {
    trim_style_arg = 1;
    trim_char_arg = 2;
  } else if (!strcmp(called_as, "TRIMPENN")) {
    trim_style_arg = 2;
    trim_char_arg = 1;
  } else if (TINY_TRIM_FUN) {
    trim_style_arg = 1;
    trim_char_arg = 2;
  } else {
    trim_style_arg = 2;
    trim_char_arg = 1;
  }

  /* If a trim style is provided, it must be the third argument. */
  if (nargs > trim_style_arg) {
    switch (*args[trim_style_arg]) {
    case 'l':
    case 'L':
      trim = TRIM_LEFT;
      break;
    case 'r':
    case 'R':
      trim = TRIM_RIGHT;
      break;
    default:
      trim = TRIM_BOTH;
      break;
    }
  } else
    trim = TRIM_BOTH;

  if (nargs > trim_char_arg && args[trim_char_arg] && *args[trim_char_arg]) {
    delims = args[trim_char_arg];
    while (*delims) {
      totrim[*delims] = 1;
      delims++;
    }
  } else {
    totrim[' '] = 1;
  }

  /* We will never need to check for buffer length overrunning, since
   * we will always get a smaller string. Thus, we can copy at the
   * same time we skip stuff.
   */

  as = parse_ansi_string(args[0]);
  s = 0;
  e = as->len;
  if (trim != TRIM_LEFT) {
    while (e > 0 && totrim[as->text[e - 1]])
      e--;
  }
  if (trim != TRIM_RIGHT) {
    while (s < e && totrim[as->text[s]])
      s++;
  }
  safe_ansi_string(as, s, e - s, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_lit)
{
  /* Just returns the argument, literally */
  safe_strl(args[0], arglens[0], buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_squish)
{
  /* zaps leading and trailing spaces, and reduces other spaces to a single
   * space. This only applies to the literal space character, and not to
   * tabs, newlines, etc.
   * We do not need to check for buffer length overflows, since we're
   * never going to end up with a longer string.
   */

  int i, j;
  char sep;
  int insep = 1;
  ansi_string *as;

  /* Figure out the character to squish */
  if (!delim_check(buff, bp, nargs, args, 2, &sep))
    return;

  as = parse_ansi_string(args[0]);

  /* get rid of trailing spaces first, so we don't have to worry about
   * them later.
   */
  for (i = as->len - 1; i >= 0; i--) {
    if (as->text[i] != sep)
      break;
  }
  as->len = i + 1;
  /* Now trim leading and sequences */
  for (i = 0, j = 0; i < as->len; i++) {
    if (as->text[i] == sep) {
      if (insep)
        continue;
      insep = 1;
    } else {
      insep = 0;
    }
    if (i != j) {
      as->text[j] = as->text[i];
      if (as->markup) {
        as->markup[j] = as->markup[i];
      }
    }
    j++;
  }
  as->len = j;
  as->text[j] = '\0';
  safe_ansi_string(as, 0, as->len, buff, bp);
  free_ansi_string(as);
}

/* ARGSUSED */
FUNCTION(fun_space)
{
  size_t s;

  if (!is_strict_uinteger(args[0])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  s = parse_uinteger(args[0]);
  safe_fill(' ', s, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_beep)
{
  int k;

  /* this function prints 1 to 5 beeps. The alert character '\a' is
   * an ANSI C invention; non-ANSI-compliant implementations may ignore
   * the '\' character and just print an 'a', or do something else nasty,
   * so we define it to be something reasonable in ansi.h.
   */

  if (nargs) {
    if (!is_integer(args[0])) {
      safe_str(T(e_int), buff, bp);
      return;
    }
    k = parse_integer(args[0]);
  } else
    k = 1;

  if ((k <= 0) || (k > 5)) {
    safe_str(T(e_range), buff, bp);
    return;
  }
  safe_fill(BEEP_CHAR, k, buff, bp);
}

FUNCTION(fun_ord)
{
  int c;

  if (!args[0] || !args[0][0] || arglens[0] != 1) {
    safe_str(T("#-1 FUNCTION (ORD) EXPECTS ONE CHARACTER"), buff, bp);
    return;
  }

  c = args[0][0];

  if (isprint(c)) {
    safe_integer(c, buff, bp);
  } else {
    safe_str(T("#-1 UNPRINTABLE CHARACTER"), buff, bp);
  }
}

FUNCTION(fun_chr)
{
  int c;

  if (!is_strict_uinteger(args[0])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  c = parse_integer(args[0]);
  if (c < 0 || c > UCHAR_MAX)
    safe_str(T("#-1 THIS ISN'T UNICODE"), buff, bp);
  else if (isprint(c))
    safe_chr(c, buff, bp);
  else
    safe_str(T("#-1 UNPRINTABLE CHARACTER"), buff, bp);

}

FUNCTION(fun_accent)
{
  if (arglens[0] != arglens[1]) {
    safe_str(T("#-1 STRING LENGTHS MUST BE EQUAL"), buff, bp);
    return;
  }
  safe_accent(args[0], args[1], arglens[0], buff, bp);
}

FUNCTION(fun_stripaccents)
{
  int n;
  for (n = 0; n < arglens[0]; n++) {
    if (accent_table[args[0][n]].base)
      safe_str(accent_table[args[0][n]].base, buff, bp);
    else
      safe_chr(args[0][n], buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_edit)
{
  int i, j;
  char *needle;
  size_t nlen;
  char *search, *ptr;
  ansi_string *orig, *repl;

  orig = parse_ansi_string(args[0]);

  for (i = 1; i < nargs - 1; i += 2) {
    needle = remove_markup(args[i], &nlen);
    nlen--;
    repl = parse_ansi_string(args[i + 1]);
    if (strcmp(needle, "$") == 0) {
      ansi_string_insert(orig, orig->len, repl);
    } else if (strcmp(needle, "^") == 0) {
      ansi_string_insert(orig, 0, repl);
    } else if (nlen == 0) {
      /* Annoying. Stick repl between each character */
      /* Since this is inserts, we're working *backwards* */
      for (j = orig->len - 1; j > 0; j--) {
        ansi_string_insert(orig, j, repl);
      }
    } else {
      search = orig->text;
      /* Find each occurrence */
      while ((ptr = strstr(search, needle)) != NULL) {
        if ((ptr - orig->text) > orig->len)
          break;
        /* Perform the replacement */
        if (ansi_string_replace(orig, ptr - orig->text, nlen, repl))
          break;
        search = ptr + repl->len;
      }
    }
    free_ansi_string(repl);
  }

  safe_ansi_string(orig, 0, orig->len, buff, bp);
  free_ansi_string(orig);
}

FUNCTION(fun_brackets)
{
  char *str;
  int rbrack, lbrack, rbrace, lbrace, lcurl, rcurl;

  lcurl = rcurl = rbrack = lbrack = rbrace = lbrace = 0;
  str = args[0];                /* The string to count the brackets in */
  while (*str) {
    switch (*str) {
    case '[':
      lbrack++;
      break;
    case ']':
      rbrack++;
      break;
    case '(':
      lbrace++;
      break;
    case ')':
      rbrace++;
      break;
    case '{':
      lcurl++;
      break;
    case '}':
      rcurl++;
      break;
    default:
      break;
    }
    str++;
  }
  safe_format(buff, bp, "%d %d %d %d %d %d", lbrack, rbrack,
              lbrace, rbrace, lcurl, rcurl);
}

/* Returns the length of str up to the first return character,
 * or else the last space, or else -1.
 */
static int
wraplen(char *str, size_t maxlen)
{
  size_t i, last = -1;

  /* If there's a newline in the first maxlen chars, return its position.
   * Otherwise, return the whole text if it's shorter than maxlen,
   * the position of the last space before maxlen (if there is one) or,
   * if all else fails, -1 to linewrap a word, hyphenated. */

  for (i = 0; i < maxlen; i++) {
    if (!str[i])
      return i;
    else if ((str[i] == '\n') || (str[i] == '\r'))
      return i;
    else if (str[i] == ' ')
      last = i;
  }

  /* Check to make sure the text wasn't hard-wrapped at the end of
   * this line anyway */
  if ((str[i] == '\n') || (str[i] == '\r') || (str[i] == '\0'))
    return i;

  return last;
}

FUNCTION(fun_wrap)
{
/*  args[0]  =  text to be wrapped (required)
 *  args[1]  =  line width (width) (required)
 *  args[2]  =  width of first line (width1st)
 *  args[3]  =  output delimiter (linesep) (btwn lines)
 */

  char *pstr;                   /* start of string */
  ansi_string *as;
  ansi_string *hyphen = NULL;
  const char *pend;             /* end of string */
  int linewidth, width1st, width;
  int linenr = 0;
  const char *linesep;
  int ansilen;

  if (!args[0] || !*args[0])
    return;

  if (ansi_strlen(args[0]) == 0) {
    safe_str(args[0], buff, bp);
    return;
  }

  if (!int_check(buff, bp, nargs, args, 2, &width, 72))
    return;

  if (!int_check(buff, bp, nargs, args, 3, &width1st, width))
    return;

  if (width1st == 0)
    width1st = width;

  if (nargs > 3)
    linesep = args[3];
  else
    linesep = "\n";

  if (width < 2 || width1st < 2) {
    safe_str(T("#-1 WIDTH TOO SMALL"), buff, bp);
    return;
  }

  as = parse_ansi_string(args[0]);
  pstr = as->text;
  pend = as->text + as->len;

  linewidth = width1st;

  while (pstr < pend) {
    if (linenr++ == 1)
      linewidth = width;
    if ((linenr > 1) && linesep && *linesep)
      safe_str(linesep, buff, bp);

    ansilen = wraplen(pstr, linewidth);

    if (ansilen < 0) {
      /* word doesn't fit on one line, so cut it */
      if (hyphen == NULL)
        hyphen = parse_ansi_string("-");
      if (!ansi_string_insert(as, pstr - as->text + linewidth - 1, hyphen))
        pend++;
      safe_ansi_string(as, pstr - as->text, linewidth, buff, bp);
      pstr += linewidth;        /* move to start of next line */
    } else {
      /* normal line */
      safe_ansi_string(as, pstr - as->text, ansilen, buff, bp);
      if (pstr[ansilen] == '\r')
        ++ansilen;
      pstr += ansilen + 1;      /* move to start of next line */
    }
  }
  free_ansi_string(as);
  if (hyphen)
    free_ansi_string(hyphen);
}

/* Alignment types. */
#define AL_LEFT 1    /**< Align left (<) */
#define AL_RIGHT 2   /**< Align right (>) */
#define AL_CENTER 3  /**< Align center (-) */
#define AL_FULL 4    /**< Full justify (_) */
#define AL_WPFULL 5  /**< Paragraph full-justify (=) */
#define AL_TYPE 0x0F /**< Only the bottom 4 bits are used for the type. */
/* Flags */
#define AL_REPEAT 0x100  /**< Repeat column (.) */
#define AL_COALESCE_LEFT 0x200  /**< Coalesce empty column with column to left (`) */
#define AL_COALESCE_RIGHT 0x400  /**< Coalesce empty column with column to right (') */
#define AL_NOFILL 0x800  /**< No filler on the right of this. ($) */
#define AL_TRUNC_EACH 0x1000 /**< Truncate each (%r-separated) line instead of wrapping (x) */
#define AL_TRUNC_ALL 0x2000 /**< Truncate the entire column output, at column width or %r, whichever comes first (X) */
#define AL_NOCOLSEP 0x4000 /**< Don't add a column separator after this column */

static int
align_one_line(char *buff, char **bp, int ncols,
               int cols[MAX_COLS], int calign[MAX_COLS], char *ptrs[MAX_COLS],
               ansi_string *as[MAX_COLS], ansi_data adata[MAX_COLS],
               int linenum, char *fieldsep, int fslen,
               char *linesep, int lslen, char filler)
{
  static char line[BUFFER_LEN];
  static char segment[BUFFER_LEN];
  char *sp;
  char *ptr;
  char *lp;
  char *lastspace;
  int i, j, k;
  int spacesneeded, numspaces, spacecount;
  int iswpfull;
  int len;
  int cols_done;
  int skipspace;
  bool needsep = 0;

  lp = line;
  memset(line, filler, BUFFER_LEN);
  cols_done = 0;
  for (i = 0; i < ncols; i++) {
    /* Skip 0-width and negative columns */
    if (cols[i] <= 0) {
      cols_done++;
      continue;
    }
    /* Is the next column AL_COALESCE_LEFT and has it run out of
     * text? If so, do the coalesce now. First, find the next column we
     * haven't already coalesced.
     */
    for (j = i + 1; j < ncols; j++) {
      if (cols[j] > 0)
        break;
    }
    if ((j < ncols) &&
        (!(calign[j] & AL_REPEAT) &&
         (calign[j] & AL_COALESCE_LEFT) && (!ptrs[j] || !*ptrs[j]))) {
      /* To coalesce left on this line, modify the left column's
       * width and set the current column width to 0 (which we can
       * teach it to skip). */
      /* If the next column is marked NOFILL or NOCOLSEP, we inherit those. */
      calign[i] |= (calign[j] & (AL_NOFILL | AL_NOCOLSEP));
      cols[i] += cols[j] + fslen;
      cols[j] = 0;
    }
    if (!ptrs[i] || !*ptrs[i]) {
      if (calign[i] & AL_REPEAT) {
        ptrs[i] = as[i]->text;
      } else {
        if (calign[i] & AL_COALESCE_RIGHT) {
          /* To coalesce right on this line,
           * modify the current column's width to 0, modify the right
           * column's width, and continue on to processing the next
           * column
           */
          for (j = i + 1; j < ncols; j++) {
            if (cols[j] > 0) {
              cols[j] += cols[i] + fslen;
              cols[i] = 0;
              break;
            }
          }
          if (cols[i] > 0) {
            /* We didn't have a column to coalesce with */
            calign[i] &= ~AL_COALESCE_RIGHT;
          } else {
            cols_done++;
            continue;
          }
        }
        if (needsep && fslen)
          safe_str(fieldsep, line, &lp);
        needsep = !(calign[i] & AL_NOCOLSEP);
        if (!(calign[i] & AL_NOFILL)) {
          if (HAS_ANSI(adata[i])) {
            write_ansi_data(&adata[i], line, &lp);
          }
          lp += cols[i];
          if (HAS_ANSI(adata[i])) {
            write_ansi_close(line, &lp);
          }
        }
        cols_done++;
        continue;
      }
    }
    if (calign[i] & AL_REPEAT) {
      cols_done++;
    }
    for (len = 0, ptr = ptrs[i], lastspace = NULL; len < cols[i]; ptr++, len++) {
      if ((!*ptr) || (*ptr == '\n'))
        break;
      if (isspace(*ptr)) {
        lastspace = ptr;
      }
    }
    /* Fixes align(3,123 1 1 1 1) */
    if (isspace((int) *ptr)) {
      lastspace = ptr;
    }
    skipspace = 0;
    sp = segment;
    if (!*ptr) {
      if (len > 0) {
        safe_ansi_string(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      ptrs[i] = ptr;
    } else if (*ptr == '\n') {
      if (len > 0) {
        safe_ansi_string(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      if (calign[i] & AL_TRUNC_ALL)
        ptrs[i] = strchr(ptr, '\0');
      else
        ptrs[i] = ptr + 1;
    } else if (lastspace) {
      char *tptr;

      ptr = lastspace;
      skipspace = 1;
      for (tptr = ptr; *tptr && tptr >= ptrs[i] && isspace(*tptr); tptr--) ;
      len = (tptr - ptrs[i]) + 1;
      if (len > 0) {
        safe_ansi_string(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      if (calign[i] & AL_TRUNC_ALL)
        ptrs[i] = strchr(ptr, '\0');
      else if (calign[i] & AL_TRUNC_EACH) {
        tptr = strchr(ptr, '\n');
        if (tptr == NULL)
          ptrs[i] = strchr(ptr, '\0');
        else
          ptrs[i] = tptr + 1;
      } else
        ptrs[i] = lastspace;
    } else {
      if (len > 0) {
        safe_ansi_string(as[i], ptrs[i] - (as[i]->text), len, segment, &sp);
      }
      if (calign[i] & AL_TRUNC_ALL)
        ptrs[i] = strchr(ptr, '\0');
      else if (calign[i] & AL_TRUNC_EACH) {
        char *tptr = strchr(ptr, '\n');
        if (tptr == NULL)
          ptrs[i] = strchr(ptr, '\0');
        else
          ptrs[i] = tptr + 1;
      } else
        ptrs[i] = ptr;
    }
    *sp = '\0';

    if (needsep && fslen)
      safe_str(fieldsep, line, &lp);
    needsep = !(calign[i] & AL_NOCOLSEP);
    if (HAS_ANSI(adata[i])) {
      write_ansi_data(&adata[i], line, &lp);
    }
    switch (calign[i] & AL_TYPE) {
    case AL_FULL:
    case AL_WPFULL:
      /* This is stupid: If it's full justify and not a hard break, then
       * we stretch spaces. If it is a hard break, then we fall through
       * to left-align.
       */
      iswpfull = (calign[i] & AL_TYPE) == AL_WPFULL;
      /* For a word processor full justify, # of spaces needed needs to be
       * less than half of the lenth.
       */
      spacesneeded = cols[i] - len;
      numspaces = 0;
      for (j = 0; segment[j]; j++) {
        if (isspace((int) segment[j])) {
          numspaces++;
        }
      }
      if (spacesneeded > 0 &&
          (!iswpfull || (cols[i] / spacesneeded) >= 2) && numspaces > 0) {
        spacecount = 0;
        for (j = 0; segment[j]; j++) {
          /* Copy the char over. */
          safe_chr(segment[j], line, &lp);
          /* If it's a space, expand it. */
          if (isspace((int) segment[j])) {
            k = (spacesneeded / numspaces);
            if (spacecount < (spacesneeded % numspaces)) {
              k++;
              spacecount++;
            }
            for (; k > 0; k--) {
              safe_chr(segment[j], line, &lp);
            }
          }
        }
        break;
      }
    default:                   /* Left-align */
      safe_str(segment, line, &lp);
      /* Don't fill if we're set NOFILL */
      if (!(calign[i] & AL_NOFILL)) {
        lp += cols[i] - len;
      }
      break;
    case AL_RIGHT:
      /* Don't fill if we're set NOFILL and this is an empty line. */
      if (*segment || !(calign[i] & AL_NOFILL)) {
        lp += cols[i] - len;
      }
      safe_str(segment, line, &lp);
      break;
    case AL_CENTER:
      j = cols[i] - len;
      lp += j >> 1;
      safe_str(segment, line, &lp);
      /* Don't fill if we're set NOFILL */
      if (!(calign[i] & AL_NOFILL)) {
        lp += (j >> 1) + (j & 1);
      }
      break;
    }
    if (HAS_ANSI(adata[i])) {
      write_ansi_close(line, &lp);
    }
    if ((lp - line) > BUFFER_LEN)
      lp = (line + BUFFER_LEN - 1);
    if (skipspace)
      for (; *ptrs[i] && (*ptrs[i] != '\n') && isspace(*ptrs[i]); ptrs[i]++) ;
  }

  if (cols_done == ncols)
    return 0;
  if ((lp - line) > BUFFER_LEN)
    lp = (line + BUFFER_LEN - 1);
  *lp = '\0';
  if (linenum > 0 && lslen > 0)
    safe_str(linesep, buff, bp);
  safe_str(line, buff, bp);
  return 1;
}

FUNCTION(fun_align)
{
  int nline;
  char *ptr;
  int ncols;
  int i;
  static int cols[MAX_COLS];
  static int calign[MAX_COLS];
  static ansi_string *as[MAX_COLS];
  static ansi_data adata[MAX_COLS];
  static char *ptrs[MAX_COLS];
  char *ansistr;
  char filler;
  char *fieldsep;
  int fslen;
  char *linesep;
  int lslen;
  int totallen = 0;

  filler = ' ';
  fieldsep = (char *) " ";
  linesep = (char *) "\n";

  memset(adata, 0, sizeof(adata));

  /* Get column widths */
  ncols = 0;
  for (ptr = args[0]; *ptr; ptr++) {
    while (isspace(*ptr))
      ptr++;
    if (*ptr == '>') {
      calign[ncols] = AL_RIGHT;
      ptr++;
    } else if (*ptr == '-') {
      calign[ncols] = AL_CENTER;
      ptr++;
    } else if (*ptr == '<') {
      calign[ncols] = AL_LEFT;
      ptr++;
    } else if (*ptr == '_') {
      calign[ncols] = AL_FULL;
      ptr++;
    } else if (*ptr == '=') {
      calign[ncols] = AL_WPFULL;
      ptr++;
    } else if (isdigit(*ptr)) {
      calign[ncols] = AL_LEFT;
    } else {
      safe_str(T("#-1 INVALID ALIGN STRING"), buff, bp);
      return;
    }
    for (i = 0; *ptr && isdigit(*ptr); ptr++) {
      i *= 10;
      i += *ptr - '0';
    }
    while (*ptr && !isspace(*ptr)) {
      switch (*ptr) {
      case '.':
        calign[ncols] |= AL_REPEAT;
        break;
      case '`':
        calign[ncols] |= AL_COALESCE_LEFT;
        break;
      case '\'':
        calign[ncols] |= AL_COALESCE_RIGHT;
        break;
      case '$':
        calign[ncols] |= AL_NOFILL;
        break;
      case 'x':
        calign[ncols] |= AL_TRUNC_EACH;
        break;
      case 'X':
        calign[ncols] |= AL_TRUNC_ALL;
        break;
      case '#':
        calign[ncols] |= AL_NOCOLSEP;
        break;
      case '(':
        ptr++;
        ansistr = ptr;
        while (*ptr && *ptr != ')')
          ptr++;
        if (*ptr != ')') {
          safe_str(T("#-1 INVALID ALIGN STRING"), buff, bp);
          return;
        }
        *ptr = '\0';
        define_ansi_data(&adata[ncols], ansistr);
        break;
      }
      ptr++;
    }

    cols[ncols++] = i;
    if (!*ptr)
      break;
  }

  for (i = 0; i < ncols; i++) {
    if (cols[i] < 0) {
      safe_str(T("#-1 CANNOT HAVE COLUMNS OF NEGATIVE SIZE"), buff, bp);
      return;
    }
    if (cols[i] > BUFFER_LEN) {
      safe_str(T("#-1 CANNOT HAVE COLUMNS THAT LARGE"), buff, bp);
      return;
    }
    if (0 && (calign[i] & AL_REPEAT))
      calign[i] &= ~(AL_TRUNC_EACH | AL_TRUNC_ALL);     /* Don't allow trunc + repeat */
    totallen += cols[i];
  }
  if (totallen > BUFFER_LEN) {
    safe_str(T("#-1 CANNOT HAVE COLUMNS THAT LARGE"), buff, bp);
    return;
  }

  if (ncols < 1) {
    safe_str(T("#-1 NOT ENOUGH COLUMNS FOR ALIGN"), buff, bp);
    return;
  }
  if (ncols > MAX_COLS) {
    safe_str(T("#-1 TOO MANY COLUMNS FOR ALIGN"), buff, bp);
    return;
  }
  if (strcmp(called_as, "LALIGN")) {
    /* each column is a separate arg */
    if (nargs < (ncols + 1) || nargs > (ncols + 4)) {
      safe_str(T("#-1 INVALID NUMBER OF ARGUMENTS TO ALIGN"), buff, bp);
      return;
    }
    if (nargs >= (ncols + 2)) {
      if (!args[ncols + 1] || strlen(args[ncols + 1]) > 1) {
        safe_str(T("#-1 FILLER MUST BE ONE CHARACTER"), buff, bp);
        return;
      }
      if (*args[ncols + 1]) {
        filler = *(args[ncols + 1]);
      }
    }
    if (nargs >= (ncols + 3)) {
      fieldsep = args[ncols + 2];
    }
    if (nargs >= (ncols + 4)) {
      linesep = args[ncols + 3];
    }

    fslen = strlen(fieldsep);
    lslen = strlen(linesep);

    for (i = 0; i < MAX_COLS; i++) {
      as[i] = NULL;
    }
    for (i = 0; i < ncols; i++) {
      as[i] = parse_ansi_string(args[i + 1]);
      ptrs[i] = as[i]->text;
    }
  } else {
    /* columns are in args[1] as an args[2]-separated list */
    char delim, *s;
    if (!delim_check(buff, bp, nargs, args, 3, &delim))
      return;
    if (do_wordcount(args[1], delim) != ncols) {
      safe_str(T("#-1 INVALID NUMBER OF ARGUMENTS TO ALIGN"), buff, bp);
      return;
    }
    if (nargs > 3) {
      if (!args[3] || strlen(args[3]) > 1) {
        safe_str(T("#-1 FILLER MUST BE ONE CHARACTER"), buff, bp);
        return;
      }
      if (*args[3])
        filler = *(args[3]);
    }
    if (nargs > 4)
      fieldsep = args[4];
    if (nargs > 5)
      linesep = args[5];

    fslen = strlen(fieldsep);
    lslen = strlen(linesep);

    for (i = 0; i < MAX_COLS; i++) {
      as[i] = NULL;
    }
    s = trim_space_sep(args[1], delim);
    for (i = 0; i < ncols; i++) {
      as[i] = parse_ansi_string(split_token(&s, delim));
      ptrs[i] = as[i]->text;
    }
  }

  nline = 0;
  while (1) {
    if (!align_one_line(buff, bp, ncols, cols, calign, ptrs,
                        as, adata, nline++, fieldsep, fslen,
                        linesep, lslen, filler))
      break;
  }
  **bp = '\0';
  for (i = 0; i < ncols; i++) {
    free_ansi_string(as[i]);
    ptrs[i] = as[i]->text;
  }
  return;
}

FUNCTION(fun_speak)
{
  ufun_attrib transufun;
  ufun_attrib nullufun;
  dbref speaker = NOTHING;
  char *speaker_str;
  const char *speaker_name;
  char *open, *close;
  char *start, *end = NULL;
  bool transform = 0, null = 0, say = 0, starting_fragment = 0;
  int funccount;
  int fragment = 0;
  char *say_string;
  char *string;
  char rbuff[BUFFER_LEN];
  PE_REGS *pe_regs;

  if (*args[0] == '&') {
    speaker_str = args[0];
    speaker_name = args[0] + 1;
  } else {
    speaker = match_thing(executor, args[0]);
    if (speaker == NOTHING || speaker == AMBIGUOUS) {
      safe_str(T(e_match), buff, bp);
      return;
    }
    speaker_str = unparse_dbref(speaker);
    speaker_name = accented_name(speaker);
  }

  if (!args[1] || !*args[1])
    return;

  string = args[1];

  if (nargs > 2 && *args[2] != '\0' && *args[2] != ' ')
    say_string = args[2];
  else
    say_string = (char *) "says,";

  if (nargs > 3) {
    if (args[3]) {
      /* we have a transform attr */
      transform = 1;
      if (!fetch_ufun_attrib(args[3], executor, &transufun, UFUN_DEFAULT)) {
        safe_str(T(e_atrperm), buff, bp);
        return;
      }
      if (nargs > 4) {
        if (args[4]) {
          /* we have an attr to use when transform returns an empty string */
          null = 1;
          if (!fetch_ufun_attrib(args[4], executor, &nullufun, UFUN_DEFAULT)) {
            safe_str(T(e_atrperm), buff, bp);
            return;
          }
        }
      }
    }
  }

  if (nargs < 6 || !args[5])
    open = (char *) "\"";
  else
    open = args[5];
  if (nargs < 7 || !args[6])
    close = open;
  else
    close = args[6];


  switch (*string) {
  case ':':
    safe_str(speaker_name, buff, bp);
    string++;
    safe_chr(' ', buff, bp);
    while (*string == ' ')
      string++;
    break;
  case ';':
    string++;
    safe_str(speaker_name, buff, bp);
    if (*string == ' ') {
      /* pose it instead */
      safe_chr(' ', buff, bp);
      while (*string == ' ')
        string++;
    }
    break;
  case '|':
    string++;
    break;
  case '"':
    if (CHAT_STRIP_QUOTE)
      string++;
    /* FALL THROUGH */
  default:
    say = 1;
    break;
  }

  start = strstr(string, open);

  if (!transform || (!say && !start)) {
    /* nice and easy */
    if (say)
      safe_format(buff, bp, "%s %s \"%s\"", speaker_name, say_string, string);
    else
      safe_str(string, buff, bp);
    return;
  }

  /* If we're in say mode, prefix the buffer */
  if (say) {
    if (speaker != NOTHING)
      safe_str(accented_name(speaker), buff, bp);
    else
      safe_str(speaker_name, buff, bp);
    safe_chr(' ', buff, bp);
    safe_str(say_string, buff, bp);
    safe_chr(' ', buff, bp);
    fragment = -1;
    /* Special case: In say mode, if the delim isn't a quote, then
     * we just pretend that we're otherwise in pose mode
     */
    if (strcmp(open, "\"")) {
      say = 0;
    }
  }

  /* If the first char of the string is an open quote, copy it,
   * skip it, and we're good to transform. Otherwise, if
   * we're not in say mode, copy up to the first open quote
   * and then transform. There must be an open quote somewhere
   * if we're not in say mode.
   */
  if (string_prefix(string, open)) {
    if (say)
      safe_str(open, buff, bp);
    start += strlen(open);
  } else if (!say && start) {
    /* start points to first open quote, and after first char */
    safe_str(chopstr(string, start - string + 1), buff, bp);
    fragment = 0;
    /* Don't add the quote in */
    start += strlen(open);
  } else {
    /* We're in say mode and the first char isn't open, start there */
    start = string;
    starting_fragment = 1;
  }

  pe_regs = pe_regs_create(PE_REGS_ARG, "fun_speak");
  while (start && *start) {
    fragment++;
    /* Transform to the next close, or to the end of the string. */
    if ((end = strstr(start, close)) != NULL)
      *end++ = '\0';
    pe_regs_setenv_nocopy(pe_regs, 0, start);
    pe_regs_setenv_nocopy(pe_regs, 1, speaker_str);
    pe_regs_setenv(pe_regs, 2, unparse_integer(fragment));
    if (call_ufun(&transufun, rbuff, executor, enactor, pe_info, pe_regs))
      break;
    pe_regs_clear(pe_regs);
    funccount = pe_info->fun_invocations;
    if (!*rbuff && (null == 1)) {
      pe_regs_setenv_nocopy(pe_regs, 0, speaker_str);
      pe_regs_setenv(pe_regs, 1, unparse_integer(fragment));
      if (call_ufun(&nullufun, rbuff, executor, enactor, pe_info, pe_regs))
        break;
      pe_regs_clear(pe_regs);
    }
    if (*rbuff)
      safe_str(rbuff, buff, bp);
    if (((*bp - buff) >= BUFFER_LEN - 1) &&
        (pe_info->fun_invocations == funccount))
      break;
    if (end && *end) {
      if (!starting_fragment) {
        if (say)
          safe_str(close, buff, bp);
        starting_fragment = 0;
      }
    }
    if (!end || !*end)
      break;
    start = strstr(end, open);
    if (start) {
      /* Copy text not being transformed. */
      if (start && *start && (start - end > (ptrdiff_t) strlen(open)))
        safe_str(chopstr(end, start - end + 1), buff, bp);
      start += strlen(open);
      end = NULL;
    } else {
      /* No more opens, so we're done, and end has the rest */
      break;
    }
    if (((*bp - buff) >= BUFFER_LEN - 1) &&
        (pe_info->fun_invocations == funccount))
      break;
  }
  if (end && *end)
    safe_str(end, buff, bp);
  pe_regs_free(pe_regs);
}

/* ARGSUSED */
FUNCTION(fun_render)
{
  int flags = 0;
  char *word, *list;

  list = trim_space_sep(args[1], ' ');

  do {
    word = split_token(&list, ' ');
    if (!word || !*word)
      continue;
    if (string_prefix("ansi", word)) {
      if (Can_Nspemit(executor)) {
        flags |= MSG_XTERM256;
      } else {
        safe_str(T(e_perm), buff, bp);
        return;
      }
    } else if (string_prefix("noaccents", word))
      flags |= MSG_STRIPACCENTS;
    else if (string_prefix("markup", word))
      flags |= MSG_MARKUP;
    else if (string_prefix("html", word))
      flags |= MSG_PUEBLO;
    else {
      safe_str(T("#-1 INVALID SECOND ARGUMENT"), buff, bp);
      return;
    }
  } while (list);

  if (!flags)
    safe_str(remove_markup(args[0], NULL), buff, bp);
  else
    safe_str(render_string(args[0], flags), buff, bp);
}
