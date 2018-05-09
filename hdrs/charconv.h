#ifndef CHARCONV_H
#define CHARCONV_H

/* Basic functions for converting strings between character sets. */

bool valid_utf8(const char *);

char *latin1_to_utf8(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;
char *latin1_to_utf8_tn(const char * restrict, int, int *, bool, const char * restrict) __attribute_malloc__;
char *utf8_to_latin1(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;

enum normalization_type {
  NORM_NFC, NORM_NFD, NORM_NFKC, NORM_NFKD
};
char* normalize_utf8(const char * restrict, int, int *, const char * restrict,
                     enum normalization_type) __attribute_malloc__;

#ifdef HAVE_ICU
#include <unicode/utypes.h>

/* Additional character set conversion functions. */

char *utf16_to_utf8(const UChar *, int, int *, const char *) __attribute_malloc__;
UChar *utf8_to_utf16(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;

char *utf32_to_utf8(const UChar32 *, int, int *, const char *) __attribute_malloc__;
UChar32 *utf8_to_utf32(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;

char *utf16_to_latin1(const UChar *, int, int *, const char *) __attribute_malloc__;
UChar *latin1_to_utf16(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;

char *utf32_to_latin1(const UChar32 *, int, int *, const char *) __attribute_malloc__;
UChar32 *latin1_to_utf32(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;

/* Case conversions */
char *latin1_to_lower(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;
char *latin1_to_upper(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;
char *utf8_to_lower(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;
char *utf8_to_upper(const char * restrict, int, int *, const char * restrict) __attribute_malloc__;

#endif

#endif
