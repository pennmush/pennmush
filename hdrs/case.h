/**
 * \file case.h
 *
 * \brief Routines for upper/lower casing characters
 */

#ifndef CASE_H
#define CASE_H

#ifdef HAVE_ICU
#include <unicode/uchar.h>
#else
#include "punicode/utypes.h"
#endif
#include <ctype.h>

#ifdef HAVE_SAFE_TOUPPER
#define DOWNCASE(x) tolower(x) /**< Returns 'x' lowercased */
#define UPCASE(x) toupper(x)   /**< Returns 'x' uppercased */
#else
#define DOWNCASE(x)                                                            \
  (isupper(x) ? tolower(x) : (x)) /**< Returns 'x' lowercased */
#define UPCASE(x)                                                              \
  (islower(x) ? toupper(x) : (x)) /**< Returns 'x' uppercased                  \
                                   */
#endif

/* Functions that only case map ASCII characters and return others unchanged. */

static inline UChar32
ascii_toupper(UChar32 c)
{
  if (c < 128) {
    return UPCASE(c);
  } else {
    return c;
  }
}

static inline UChar32
ascii_tolower(UChar32 c)
{
  if (c < 128) {
    return DOWNCASE(c);
  } else {
    return c;
  }
}

/* Unicode mappings for a single codepoint to single codepoint. For more
 * flexible case folding, use the allocating functions that work on entire
 * strings that are in charconv.h (Should they be moved here? */

static inline UChar32
uni_toupper(UChar32 c)
{
#ifdef HAVE_ICU
  return u_toupper(c);
#else
  return ascii_toupper(c);
#endif
}

static inline UChar32
uni_tolower(UChar32 c)
{
#ifdef HAVE_ICU
  return u_tolower(c);
#else
  return ascii_tolower(c);
#endif
}

static inline UChar32
uni_totitle(UChar32 c)
{
#ifdef HAVE_ICU
  return u_totitle(c);
#else
  return ascii_toupper(c);
#endif
}

#endif /* CASE_H */
