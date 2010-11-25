/**
 * \file wild.c
 *
 * \brief Wildcard matching routings for PennMUSH
 *
 * Written by T. Alexander Popiel, 24 June 1993
 * Last modified by Javelin, 2002-2003
 *
 * Thanks go to Andrew Molitor for debugging
 * Thanks also go to Rich $alz for code to benchmark against
 *
 * Rather thoroughly rewritten by Walker, 2010
 *
 * Copyright (c) 1993,2000 by T. Alexander Popiel
 * This code is available under the terms of the GPL,
 * see http://www.gnu.org/copyleft/gpl.html for details.
 *
 * This code is included in PennMUSH under the PennMUSH
 * license by special dispensation from the author,
 * T. Alexander Popiel.  If you wish to include this
 * code in other packages, but find the GPL too onerous,
 * then please feel free to contact the author at
 * popiel@wolfskeep.com to work out agreeable terms.
 */

#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "copyrite.h"
#include "conf.h"
#include "case.h"
#include "externs.h"
#include "ansi.h"
#include "mymalloc.h"
#include "parse.h"
#include "mypcre.h"
#include "confmagic.h"

/** Force a char to be lowercase */
#define FIXCASE(a) (DOWNCASE(a))
/** Check for equality of characters, maybe case-sensitive */
#define EQUAL(cs,a,b) ((cs) ? (a == b) : (FIXCASE(a) == FIXCASE(b)))
/** Check for inequality of characters, maybe case-sensitive */
#define NOTEQUAL(cs,a,b) ((cs) ? (a != b) : (FIXCASE(a) != FIXCASE(b)))
/** Maximum number of wildcarded arguments */
#define NUMARGS (10)

const unsigned char *tables = NULL;  /** Pointer to character tables */

/** Do a wildcard match, without remembering the wild data.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param tstr pattern to match against.
 * \param dstr string to check.
 * \retval 1 dstr matches the tstr pattern.
 * \retval 0 dstr does not match the tstr pattern.
 */
bool
quick_wild(const char *restrict tstr, const char *restrict dstr)
{
  return quick_wild_new(tstr, dstr, 0);
}

/** Do a wildcard match, possibly case-sensitive, without memory.
 *
 * This probably crashes if fed NULLs instead of strings, too.
 *
 * \param tstr pattern to match against.
 * \param dstr string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \retval 1 dstr matches the tstr pattern.
 * \retval 0 dstr does not match the tstr pattern.
 */
bool
quick_wild_new(const char *restrict tstr, const char *restrict dstr, bool cs)
{
  return wild_match_test(tstr, dstr, cs, NULL, 0);
}

static bool
real_atr_wild(const char *restrict tstr,
              const char *restrict dstr,
              int *invokes);
/** Do an attribute name wildcard match.
 *
 * This probably crashes if fed NULLs instead of strings, too.
 * The special thing about this one is that ` doesn't match normal
 * wildcards; you have to use ** to match embedded `.  Also, patterns
 * ending in ` are treated as patterns ending in `*, and empty patterns
 * are treated as *.
 *
 * \param tstr pattern to match against.
 * \param dstr string to check.
 * \retval 1 dstr matches the tstr pattern.
 * \retval 0 dstr does not match the tstr pattern.
 */
bool
atr_wild(const char *restrict tstr, const char *restrict dstr)
{
  int invokes = 10000;
  return real_atr_wild(tstr, dstr, &invokes);
}

static bool
real_atr_wild(const char *restrict tstr,
              const char *restrict dstr,
              int *invokes)
{
  int starcount;
  if (*invokes > 0) {
    (*invokes)--;
  } else {
    return 0;
  }

  if (!*tstr)
    return !strchr(dstr, '`');

  while (*tstr != '*') {
    switch (*tstr) {
    case '?':
      /* Single character match.  Return false if at
       * end of data.
       */
      if (!*dstr || *dstr == '`')
        return 0;
      break;
    case '`':
      /* Delimiter match.  Special handling if at end of pattern. */
      if (*dstr != '`')
        return 0;
      if (!tstr[1])
        return !strchr(dstr + 1, '`');
      break;
    case '\\':
      /* Escape character.  Move up, and force literal
       * match of next character.
       */
      tstr++;
      /* FALL THROUGH */
    default:
      /* Literal character.  Check for a match.
       * If matching end of data, return true.
       */
      if (NOTEQUAL(0, *dstr, *tstr))
        return 0;
      if (!*dstr)
        return 1;
    }
    tstr++;
    dstr++;
  }

  /* Skip over '*'. */
  tstr++;
  starcount = 1;

  /* Skip over wildcards. */
  while (starcount < 2 && ((*tstr == '?') || (*tstr == '*'))) {
    if (*tstr == '?') {
      if (!*dstr || *dstr == '`')
        return 0;
      dstr++;
      starcount = 0;
    } else
      starcount++;
    tstr++;
  }

  /* Skip over long strings of '*'. */
  while (*tstr == '*')
    tstr++;

  /* Return true on trailing '**'. */
  if (!*tstr)
    return starcount == 2 || !strchr(dstr, '`');

  if (*tstr == '?') {
    /* Scan for possible matches. */
    while (*dstr) {
      if (*dstr != '`' && real_atr_wild(tstr + 1, dstr + 1, invokes))
        return 1;
      dstr++;
      if (*invokes <= 0) return 0;
    }
  } else {
    /* Skip over a backslash in the pattern string if it is there. */
    if (*tstr == '\\')
      tstr++;

    /* Scan for possible matches. */
    while (*dstr) {
      if (EQUAL(0, *dstr, *tstr)) {
        if (!*(tstr + 1) && *(dstr + 1))
          return 0;             /* No more in pattern string, but more in target */
        if (real_atr_wild(tstr + 1, dstr + 1, invokes))
          return 1;
      }
      if (*invokes <= 0) return 0;
      if (starcount < 2 && *dstr == '`')
        return 0;
      dstr++;
    }
  }
  return 0;
}

/** Wildcard match, possibly case-sensitive, and remember the wild match
 * start+lengths.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param s pattern to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \param matches An int[nmatches*2] to store positions into. The result will
 *                be [0start, 0len, 1start, 1len, 2start, 2len, ...]
 * \param nmatches Number of elements ary can hold, divided by 2.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
bool
wild_match_test(const char *restrict pat, const char *restrict str, bool cs,
                int *matches, int nmatches)
{
  int i, pi;
  bool globbing = 0; /* Are we in a glob match right now? */
  int pbase = 0, sbase = 0; /* Guaranteed matched so far. */
  int matchi = 0, mbase = 0;
  int slen = strlen(str);

  for (i = 0; i < nmatches; i++) {
    matches[i*2] = -1;
    matches[i*2+1] = 0;
  }

  char pbuff[BUFFER_LEN];
  char tbuff[BUFFER_LEN];
  if (!cs) {
    strncpy(pbuff, remove_markup(pat, NULL), BUFFER_LEN);
    strncpy(tbuff, remove_markup(str, NULL), BUFFER_LEN);

    upcasestr(pbuff);
    upcasestr(tbuff);

    pat = pbuff;
    str = tbuff;
  }

  for (i = 0, pi = 0; (sbase + i) < slen;) {
    switch (pat[pbase+pi]) {
    case '?':
      /* No complaints here. Auto-match */
      if (matchi < nmatches) {
        matches[matchi*2] = sbase+i;
        matches[matchi*2+1] = 1;
      }
      matchi++;
      pi++; i++;
      break;
    case '*':
      /* Everything so far is guaranteed matched. */
      pbase += pi;
      sbase += i;
      globbing = 1;
      i = pi = 0;
      /* Skip past multiple globs. */
      while (pat[pbase] == '*') {
        pbase++;
        mbase = matchi++;
        if (mbase < nmatches) {
          matches[mbase*2] = sbase;
          matches[mbase*2+1] = 0;
        }
      }
      if (!pat[pbase]) {
        /* This pattern is the last thing, we match the rest. */
        if (mbase < nmatches) {
          matches[mbase*2+1] = slen - sbase;
        }
        return 1;
      }
      break;
    case '\\':
      /* Literal match of the next character, which may be a * or ?. */
      pi++;
    default:
      if (str[sbase+i] == pat[pbase+pi]) {
        pi++; i++;
        break;
      }
    case 0: /* Pattern is too short to match */
      /* If we're dealing with a glob, advance it by 1 character. */
      if (globbing) {
        if (mbase < nmatches) {
          /* Up glob length */
          matches[mbase*2+1]++;
        }
        sbase++;
        /* Reset post-glob matches. */
        i = pi = 0;
        matchi = mbase + 1;
      } else {
        return 0;
      }
    }
  }
  while (pat[pbase+pi] == '*') pi++;
  return !pat[pbase+pi];
}

/** Wildcard match, possibly case-sensitive, and remember the wild data
 *  in matches, using the data buffer to store them.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param s pattern to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \param matches An array to store the grabs in
 * \param nmatches Number of elements ary can hold
 * \param data Buffer used to hold the matches. The elements of ary
 *    are set to pointers into this buffer.
 * \param len The number of bytes in data. Twice the length of d should
 *    be enough.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
bool
wild_match_case_r(const char *restrict s, const char *restrict d, bool cs,
                  char **matches, int nmatches, char *data, int len)
{
  int results[BUFFER_LEN * 2];
  int n;

  ansi_string *as;
  char *buff, *maxbuff;
  char *bp;
  int curlen, spaceleft;

  if (wild_match_test(s, d, cs, results, BUFFER_LEN)) {
    /* Populate everything. Oi.
     *
     * Given that our best best is safe_ansi_string, but that data can be
     * 2x BUFFER_LEN, we have to do ugly math in order to safely use
     * safe_ansi_string while still using all of data. Hence all the
     * math with buff and bp. We sorely need a better string system
     * than inherent BUFFER_LENs everywhere.
     *
     * bp is always pointing to the right spot. We have to point buff
     * at either bp, or at (data+len) - BUFFER_LEN, whichever is lowest.
     */
    if (nmatches > 0 && data && len > 0) {
      /* For speed purposes, make sure we have an ansi'd string before
       * we actually use parse_ansi_string */
      if (has_markup(d)) {
        as = parse_ansi_string(d);
        bp = data;
        maxbuff = data + len - BUFFER_LEN;

        for (n = 0;
             (n < BUFFER_LEN) && (results[n*2] >= 0)
             && n < nmatches && bp < (data+len);
             n++) {
          /* Eww, eww, eww, EWWW! */
          buff = bp;
          if (buff > maxbuff) buff = maxbuff;

          matches[n] = bp;
          safe_ansi_string(as, results[n*2], results[n*2+1], buff, &bp);
          *(bp++) = '\0';
        }
        free_ansi_string(as);
      } else {
        for (n = 0, curlen = 0;
             (results[n*2] >= 0) && n < nmatches && curlen < len;
             n++) {
          spaceleft = len - curlen;
          if (results[n*2+1] < spaceleft) {
            spaceleft = results[n*2+1];
          }
          matches[n] = data + curlen;
          strncpy(data + curlen, d + results[n*2], spaceleft);
          curlen += spaceleft;
          data[curlen++] = '\0';
        }
      }
      for (; n < nmatches; n++) {
        matches[n] = NULL;
      }
    }
    return 1;
  } else {
    /* Unset matches. */
    for (n = 0; n < nmatches; n++) {
      matches[n] = NULL;
    }
  }
  return 0;
}

/** Regexp match, possibly case-sensitive, and remember matched subexpressions.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param s regexp to match against.
 * \param val string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive
 * \param matches array to store matched subexpressions in
 * \param nmatches the size of the matches array
 * \param data buffer space to copy matches into. The elements of
 *   array point into here
 * \param len The size of data
 * \retval 1 d matches s
 * \retval 0 d doesn't match s
 */
bool
regexp_match_case_r(const char *restrict s, const char *restrict val, bool cs,
                    char **matches, size_t nmatches, char *data, ssize_t len)
{
  pcre *re;
  pcre_extra *extra;
  size_t i;
  const char *errptr;
  ansi_string *as;
  const char *d;
  size_t delenn;
  int erroffset;
  int offsets[99];
  int subpatterns;
  int totallen = 0;

  for (i = 0; i < nmatches; i++)
    matches[i] = NULL;

  if ((re = pcre_compile(s, (cs ? 0 : PCRE_CASELESS), &errptr, &erroffset,
                         tables)) == NULL) {
    /*
     * This is a matching error. We have an error message in
     * errptr that we can ignore, since we're doing
     * command-matching.
     */
    return 0;
  }
  add_check("pcre");

  /* The ansi string */
  as = parse_ansi_string(val);
  delenn = as->len;
  d = as->text;

  extra = default_match_limit();
  /*
   * Now we try to match the pattern. The relevant fields will
   * automatically be filled in by this.
   */
  if ((subpatterns = pcre_exec(re, extra, d, delenn, 0, 0, offsets, 99))
      < 0) {
    free_ansi_string(as);
    mush_free(re, "pcre");
    return 0;
  }
  /* If we had too many subpatterns for the offsets vector, set the number
   * to 1/3 of the size of the offsets vector
   */
  if (subpatterns == 0)
    subpatterns = 33;

  /*
   * Now we fill in our args vector. Note that in regexp matching,
   * 0 is the entire string matched, and the parenthesized strings
   * go from 1 to 9. We DO PRESERVE THIS PARADIGM, for consistency
   * with other languages.
   */
  for (i = 0; i < nmatches && (int) i < subpatterns && totallen < len; i++) {
    /* This is more annoying than a jumping flea up the nose. Since
     * ansi_pcre_copy_substring() uses buff, bp instead of char *, len,
     * we have to mangle bp and 'buff' by hand. Sound easy? We also
     * have to make sure that 'buff' + len < BUFFER_LEN. Particularly since
     * matchspace is 2*BUFFER_LEN
     */
    char *buff = data + totallen;
    char *bp = buff;
    matches[i] = bp;
    if ((len - totallen) < BUFFER_LEN) {
      buff = data + len - BUFFER_LEN;
    }
    ansi_pcre_copy_substring(as, offsets, subpatterns, (int) i, 1, buff, &bp);
    *(bp++) = '\0';
    totallen = bp - data;
  }

  free_ansi_string(as);
  mush_free(re, "pcre");
  return 1;
}


/** Regexp match, possibly case-sensitive, and with no memory.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param s regexp to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
bool
quick_regexp_match(const char *restrict s, const char *restrict d, bool cs)
{
  pcre *re;
  pcre_extra *extra;
  const char *sptr;
  size_t slen;
  const char *errptr;
  int erroffset;
  int offsets[99];
  int r;
  int flags = 0;                /* There's a PCRE_NO_AUTO_CAPTURE flag to turn all raw
                                   ()'s into (?:)'s, which would be nice to use,
                                   except that people might use backreferences in
                                   their patterns. Argh. */

  if (!cs)
    flags |= PCRE_CASELESS;

  if ((re = pcre_compile(s, flags, &errptr, &erroffset, tables)) == NULL) {
    /*
     * This is a matching error. We have an error message in
     * errptr that we can ignore, since we're doing
     * command-matching.
     */
    return 0;
  }
  add_check("pcre");
  sptr = remove_markup(d, &slen);
  extra = default_match_limit();
  /*
   * Now we try to match the pattern. The relevant fields will
   * automatically be filled in by this.
   */
  r = pcre_exec(re, extra, sptr, slen - 1, 0, 0, offsets, 99);

  mush_free(re, "pcre");

  return r >= 0;
}

/** Regexp match of a pre-compiled regexp, with no memory.
 * \param re the regular expression
 * \param subj the string to match against.
 * \return true or false
 */
bool
qcomp_regexp_match(const pcre * re, const char *subj)
{
  int len;
  int offsets[99];
  pcre_extra *extra;

  if (!re || !subj)
    return false;

  len = strlen(subj);
  extra = default_match_limit();
  return pcre_exec(re, extra, subj, len, 0, 0, offsets, 99) >= 0;
}


/** Either an order comparison or a wildcard match with no memory.
 *
 *
 * \param s pattern to match against.
 * \param d string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
bool
local_wild_match_case(const char *restrict s, const char *restrict d, bool cs)
{
  if (s && *s) {
    switch (*s) {
    case '>':
      s++;
      if (is_number(s) && is_number(d))
        return (parse_number(s) < parse_number(d));
      else
        return (strcoll(s, d) < 0);
    case '<':
      s++;
      if (is_number(s) && is_number(d))
        return (parse_number(s) > parse_number(d));
      else
        return (strcoll(s, d) > 0);
    default:
      return quick_wild_new(s, d, cs);
    }
  } else
    return (!d || !*d) ? 1 : 0;
}

/** Does a string contain a wildcard character (* or ?)?
 * Not used by the wild matching routines, but suitable for outside use.
 * \param s string to check.
 * \retval 1 s contains a * or ?
 * \retval 0 s does not contain a * or ?
 */
bool
wildcard(const char *s)
{
  if (strchr(s, '*') || strchr(s, '?'))
    return 1;
  return 0;
}
