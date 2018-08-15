/** \file myutf8.h
 *
 * \brief Basic UTF-8 handling.
 */

#pragma once
#ifdef HAVE_ICU
#include <unicode/utypes.h>
#include <unicode/utf8.h>
#else
/* Use a stripped down copy of ICU 61.1 headers. */
#define U_NO_DEFAULT_INCLUDE_UTF_HEADERS 1
#include "punicode/utypes.h"
#include "punicode/utf8.h"
#endif

int gcbytes(const char *);
int strlen_gc(const char *);
int strnlen_gc(const char *, int);

/** Return the number of bytes the first codepoint in a utf-8 string takes. */
inline int
cpbytes(const char *s)
{
  int bytes = 0;
  U8_FWD_1(s, bytes, -1);
  return bytes;
}

int strlen_cp(const char *);

/** Return the number of bytes the first N codepoints in a utf-8 string take. */
inline int
strnlen_cp(const char *s, int n)
{
  int bytes = 0;
  U8_FWD_N(s, bytes, -1, n);
  return bytes;
}

/** Return the first codepoint in a UTF-8 string. Usually you'll want
 * to use U8_NEXT() directly, but sometimes it's handy to have as a
 * function instead of a macro.
 *
 * \param s the UTF-8 string
 * \return the codepoint, negative on an invalid byte sequence.
 */
inline UChar32
first_cp(const char *s)
{
  UChar32 c;
  int offset = 0;
  U8_NEXT(s, offset, -1, c);
  return c;
}
