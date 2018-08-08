/** \file charclass.h
 *
 * \brief Unicode character classification functions.
 */

#pragma once
#ifdef HAVE_ICU
#include <unicode/uchar.h>
#else
#include "punicode/utypes.h"
#endif
#include <ctype.h>

/* Latin-1 overlaps the first 256 Unicode code points, so use Unicode
   tests if available instead of relying on server locale. */

/* Define for testing */
#define USE_PCRE_CLASS

/* Unicode-aware character classification functions with a PCRE backend */
bool re_isprint(UChar32 c);
bool re_isspace(UChar32 c);
bool re_islower(UChar32 c);
bool re_isupper(UChar32 c);
bool re_isdigit(UChar32 c);
bool re_isalnum(UChar32 c);
bool re_isalpha(UChar32 c);

/* Unicode-aware character classification functions that use ICU and
   fall back to PCRE */
static inline bool
uni_isprint(UChar32 c)
{
#if !defined(HAVE_ICU) || defined(USE_PCRE_CLASS)
  return re_isprint(c);
#else
  return u_hasBinaryProperty(c, UCHAR_POSIX_PRINT);
#endif
}

static inline bool
uni_isspace(UChar32 c)
{
#if !defined(HAVE_ICU) || defined(USE_PCRE_CLASS)
  return re_isspace(c);
#else
  return u_hasBinaryProperty(c, UCHAR_WHITE_SPACE);
#endif
}

static inline bool
uni_islower(UChar32 c)
{
#if !defined(HAVE_ICU) || defined(USE_PCRE_CLASS)
  return re_islower(c);
#else
  return u_hasBinaryProperty(c, UCHAR_LOWERCASE);
#endif
}

static inline bool
uni_isupper(UChar32 c)
{
#if !defined(HAVE_ICU) || defined(USE_PCRE_CLASS)
  return re_isupper(c);
#else
  return u_hasBinaryProperty(c, UCHAR_UPPERCASE);
#endif
}

static inline bool
uni_isdigit(UChar32 c)
{
#if !defined(HAVE_ICU) || defined(USE_PCRE_CLASS)
  return re_isdigit(c);
#else
  return u_charType(c) == U_DECIMAL_DIGIT_NUMBER;
#endif
}

static inline bool
uni_isalnum(UChar32 c)
{
#if !defined(HAVE_ICU) || defined(USE_PCRE_CLASS)
  return re_isalnum(c);
#else
  return u_hasBinaryProperty(c, UCHAR_POSIX_ALNUM);
#endif
}

static inline bool
uni_isalpha(UChar32 c)
{
#if !defined(HAVE_ICU) || defined(USE_PCRE_CLASS)
  return re_isalpha(c);
#else
  return u_hasBinaryProperty(c, UCHAR_ALPHABETIC);
#endif
}

/* Character classification functions that require ASCII */

static inline bool
ascii_isprint(UChar32 c)
{
  return c < 128 && isprint(c);
}

static inline bool
ascii_isspace(UChar32 c)
{
  return c < 128 && isspace(c);
}

static inline bool
ascii_islower(UChar32 c)
{
  return c < 128 && islower(c);
}

static inline bool
ascii_isupper(UChar32 c)
{
  return c < 128 && isupper(c);
}

static inline bool
ascii_isdigit(UChar32 c)
{
  return c < 128 && isdigit(c);
}

static inline bool
ascii_isalnum(UChar32 c)
{
  return c < 128 && isalnum(c);
}

static inline bool
ascii_isalpha(UChar32 c)
{
  return c < 128 && isalpha(c);
}
