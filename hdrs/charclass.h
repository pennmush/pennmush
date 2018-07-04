/** \file charclass.h
 *
 * \brief Character classfication functions, optionally unicode-aware
 */

#pragma once

#ifdef HAVE_ICU
#include <unicode/uchar.h>
#else
#include <ctype.h>
#include "punicode/utypes.h"
#endif

/* Latin-1 overlaps the first 256 Unicode code points, so use Unicode
   tests if available instead of relying on server locale. */

static inline bool
char_isprint(char c)
{
#ifdef HAVE_ICU
  return u_isprint((UChar32) c);
#else
  return isprint(c);
#endif
}

static inline bool
ascii_isprint(char c)
{
  return c < 128 && char_isprint(c);
}
