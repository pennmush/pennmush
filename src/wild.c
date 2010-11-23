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

/* In our version of strstr and strcmp, a MATCH_ANY_CHAR matches anything. */
#define MATCH_ANY_CHAR '\x04'

/** Test if a test string matches a pattern string. If len is -1, then
 * it tests the entire string. If len is non-zero, it tests that many
 * characters of test and pat against each other.
 *
 * \param test A string to test.
 * \param pattern The pattern to test it against. It may have MATCH_ANY_CHAR
 * \param len The length of the pattern.
 * \retval 1 The test matches the pattern.
 * \retval 0 The test does not match the pattern.
 */
static bool
strmatchwildn(const char *test, const char *pattern, int len) {
  int i;
  for (i = 0; (i < len || len < 0) && test[i] && pattern[i]; i++) {
    if ((test[i] != pattern[i]) && (pattern[i] != MATCH_ANY_CHAR)) {
      return 0;
    }
  }
  return (i >= len || pattern[i] == test[i]);
}
#define strmatchwild(t,p) strmatchwildn(t,p,-1)

/** Find pattern string within test string. The pattern string may have
 *  MATCH_ANY_CHAR in it.
 *
 *  TODO: Optimize this with Knuth-Morris-Pratt or similar?
 * \param test A string to test.
 * \param testlen Length of <test>
 * \param pattern The pattern to test it against. It may have MATCH_ANY_CHAR
 * \retval 1 The test matches the pattern.
 * \retval 0 The test does not match the pattern.
 */
static const char *
strstrwildn(const char *test, int testlen,
            const char *pattern, int patlen) {
  int start = 0;
  int startWith = 0;
  int count = 0;
  int maxStart;
  /* For speed purposes, ignore leading MATCH_ANY_CHARs until a match is found,
   * then do math with it. Yay math.
   */
  while (*pattern == MATCH_ANY_CHAR) {
    startWith++;
    pattern++;
    patlen--;
  }
  start = startWith;

  maxStart = testlen - patlen;

  if (!*pattern) {
    /* The pattern is either empty or formed purely of MATCH_ANY_CHARs.
     * Easy. */
    return (testlen >= startWith) ? test : NULL;
  }

  /* Now, starting at <start>, look for *pattern.  We have already guaranteed
   * that pattern will not start with MATCH_ANY_CHARS.
   */
  while (start <= maxStart) {
    if (test[start] == pattern[0]) {
      for (count = 0; count < patlen; count++) {
        if (!((test[start+count] == pattern[count]) ||
             (pattern[count] == MATCH_ANY_CHAR))) {
          break;
        }
      }
      if (count >= patlen) {
        /* We have a match! */
        return test + start - startWith;
      }
    }
    start++;
  }
  return NULL;
}

#define strstrwild(t,p) strstrwildn(t,strlen(t),p,strlen(p))

#define WTYPE_CHAR '?'    /* Matches a ? that follows a * */
#define WTYPE_LITERAL ' ' /* Matches a literal string, including ?s. */
#define WTYPE_GLOB '*'    /* Matches 0 or more characters. */
#define WTYPE_NONE '\0'   /* Nothing more to match. End of string. */
typedef struct wild_match_info {
  short type;       /* Literal, Glob or Char */
  short matchcount; /* For WTYPE_LITERAL: How many ?s are in it? */
  short start, len; /* The start position and length of the found match */
  char *string;   /* For a literal, the string to match. */
} Wild_Match_Info;


/* Assumption: This is filling a wmi of BUFFER_LEN with a pattern that
 * is guaranteed <= BUFFER_LEN, meaning wmi will never overflow. */
static void
populate_match_info(Wild_Match_Info *wmi, char *pat) {
  int wmic = 0;
  char *str;

  while (*pat) {
    wmi[wmic].start = -1;
    wmi[wmic].len = 0;
    wmi[wmic].matchcount = 0;
    wmi[wmic].string = NULL;
    switch (*pat) {
    case '*':
      wmi[wmic].type = WTYPE_GLOB;
      wmic++;
      *(pat++) = '\0';
      break;
    case '?':
      /* The only way we get this is if this is either the first character,
       * or it follows a *. (Or is *???, etc). If it's the first character,
       * we want it to fall through to WTYPE_LITERAL. Otherwise it's a
       * WTYPE_CHAR */
      if (wmic > 0) {
        wmi[wmic].type = WTYPE_CHAR;
        wmic++;
        *(pat++) = '\0';
        break;
      }
    default:
      /* We need to remove '\'s. Sigh. */
      wmi[wmic].type = WTYPE_LITERAL;
      wmi[wmic].string = pat;

      /* Literals don't count as matches UNLESS they have ?s in them.
       * In which case, they have N matches.
       */
      wmi[wmic].matchcount = 0;
      for (str = pat; *pat && *pat != '*'; pat++) {
        if (*pat == '?') {
          *(str++) = MATCH_ANY_CHAR;
          wmi[wmic].matchcount++;
          wmi[wmic].len++;
        } else {
          if (*pat == '\\') {
            pat++;
            /* A \ at the end of the pattern is invalid, so we ignore. */
            if (!*pat) break;
          }
          *(str++) = *pat;
          wmi[wmic].len++;
        }
      }
      if (str < pat) {
        *(str) = '\0';
      }
      wmic++;
      break;
    }
  }
  wmi[wmic].type = WTYPE_NONE;
}

/** Return 1 if this and all subsequent wild_match_infos 
 * match the text.
 *
 * This is an attempt to do wild matching iteratively instead of
 * recursively.
 * We are utterly relying on strmatchwildn() and strstrwild() to do
 * the grunt work for us.
 */

static bool
wild_test_wmi(Wild_Match_Info *wmi, const char *test, int len) {
  Wild_Match_Info *wp;
  int minlen = 0;
  int i = 0;
  int endpoint = len;
  int idx = 0;
  const char *tmp;
  Wild_Match_Info *globi, *globstart, *globp;

  for (wp = wmi; wp->type != WTYPE_NONE;) {
    switch (wp->type) {
    case WTYPE_LITERAL:
      if (!strmatchwildn(test + idx, wp->string, wp->len))
        return 0;
      wp->start = idx;
      idx += wp->len;
      wp++;
      break;
    case WTYPE_CHAR:
    default:
      /* Should not happen! WTYPE_CHARs should only appear after
       * WTYPE_GLOB, and should be handled by the WTYPE_GLOB case, too.
       * So we return 0, because a borked WMI structure is a borked
       * wildmatch.
       */
      return 0;
    case WTYPE_GLOB:
      minlen = 0;
      i = 0;
      endpoint = len;
      /* This is how we handle wildcard grouping like *?**??*?:
       *
       * In such a string: The final glob is greedy, so gets everything
       * between the ?s before it and the ?s after it.
       */
      globi = globp = wp;
      for (globstart = wp; wp->type == WTYPE_CHAR ||
                       wp->type == WTYPE_GLOB; wp++) {
        globp = wp;
        if (wp->type == WTYPE_GLOB) {
          globi = wp;
        } else {
          minlen++;
        }
      }
      if (wp->type == WTYPE_NONE) {
        /* This globbing pattern is at the end of the string. Super-easy. */
        if ((len - idx) < minlen) {
          /* But the pattern don't fit. d'oh. */
          return 0;
        }
        endpoint = len;
      } else if (wp->type == WTYPE_LITERAL) {
        if ((wp+1)->type == WTYPE_NONE) {
          /* This literal must be at the very end, or it's no match.
           * We do the +1 to force checking the null terminator. */
          if (!strmatchwildn(test + len - wp->len, wp->string, wp->len + 1)) {
            return 0;
          }
          endpoint = len - wp->len;
        } else {
          tmp = strstrwildn(test + idx + minlen, len - (idx + minlen),
                            wp->string, wp->len);
          if (!tmp) {
            /* This will never match. */
            return 0;
          }
          /* We have a match! Yippee! */
          endpoint = tmp - test;
        }
        wp->start = endpoint;
      } else {
        /* Like, what, dude? How'd we get here? Invalid, yo! */
        return 0;
      }
      if (endpoint - idx < minlen) {
        return 0;
      }
      /* It's valid. If minlen > 0, populate the relevant WTYPE_CHARs and
       * then fill globi with the rest. */
      for (i = endpoint; globp >= globstart; globp = globp - 1) {
        if (globp->type == WTYPE_CHAR) {
          globp->start = --i;
          globp->len = 1;
          minlen--;
        } else if (globp == globi) {
          globp->start = idx + minlen;
          globp->len = i - globp->start;
          i = idx + minlen;
          globi = NULL;
        } else {
          globp->start = idx + minlen;
          globp->len = 0;
        }
      }
      idx = endpoint;
      if (wp->type == WTYPE_LITERAL) {
        idx += wp->len;
        wp++;
      }
      break;
    }
  }
  return (idx >= len);
}

/** Wildcard match, case sensitive. (Upcase before calling to make
 * case insensitive
 *
 * Don't call this directly, use wild_match_case_r.
 *
 * Returns 1 on match, 0 on no match. Resulting matches are stored using
 * (start, length) in the matches array.
 *
 * If it's buggy, blame Walker.
 */
static int
wild_test(char *pat, const char *restrict test,
          int *matches, int nmatches) {
  /* Maximum match data is BUFFER_LEN ?s. */
  /* We make this static simply because of how large it is. */
  static Wild_Match_Info wmis[BUFFER_LEN];
  int i, j, k;
  int matchcount;

  /* Set up wild_match_info */
  populate_match_info(wmis, pat);

  if (!wild_test_wmi(wmis, test, strlen(test))) {
    return 0;
  }

  /* Populate and return the matches. */
  for (i = 0, j = 0;
       i < BUFFER_LEN && j < nmatches && wmis[i].type != WTYPE_NONE;
       i++) {
    switch (wmis[i].type) {
    case WTYPE_CHAR:
    case WTYPE_GLOB:
      matches[j*2] = wmis[i].start;
      matches[j*2+1] = wmis[i].len;
      j++;
      break;
    case WTYPE_LITERAL:
      if (wmis[i].matchcount > 0) {
        matchcount = wmis[i].matchcount;
        /* This WTYPE_LITERAL has some MATCH_ANY_CHARs in it. */
        for (k = 0; k < wmis[i].len && matchcount > 0; k++) {
          if (wmis[i].string[k] == MATCH_ANY_CHAR) {
            matches[j*2] = wmis[i].start + k;
            matches[j*2+1] = 1;
            j++;
            matchcount--;
          }
        }
      }
      break;
    default:
      break;
    }
  }
  if (j < nmatches) {
    matches[j*2] = -1;
    matches[j*2+1] = 0;
  }
  return 1;
}

/** Wildcard match, possibly case-sensitive, and remember the wild match
 * start+lengths.
 *
 * This routine will cause crashes if fed NULLs instead of strings.
 *
 * \param pat pattern to match against.
 * \param test string to check.
 * \param cs if 1, case-sensitive; if 0, case-insensitive.
 * \param matches An int[nmatches*2] to store positions into. The result will
 *                be [0start, 0len, 1start, 1len, 2start, 2len, ...]
 * \param nmatches Number of elements ary can hold, divided by 2.
 * \retval 1 d matches s.
 * \retval 0 d doesn't match s.
 */
bool
wild_match_test(const char *restrict s, const char *restrict d, bool cs,
                int *matches, int nmatches)
{
  char pat[BUFFER_LEN];
  char test[BUFFER_LEN];
  strncpy(pat, remove_markup(s, NULL), BUFFER_LEN);
  strncpy(test, remove_markup(d, NULL), BUFFER_LEN);

  /* Set all 'start's to -1 and all 'len's to 0 */
  if (!cs) {
    upcasestr(pat);
    upcasestr(test);
  }

   return wild_test(pat, test, matches, nmatches);
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
 *    are set to pointers into this  buffer.
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
 * \param d string to check.
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
