#ifndef CHARCONV_H
#define CHARCONV_H

#include "myutf8.h"

/* Basic functions for converting strings between Unicode encodings
   and Latin-1. _us versions are unsafe ones that require a
   well-formed UTF-8 string and don't do sanity checks. Ones that take
   UTF-16 and UTF-32 strings assume the string is well formed and
   again don't do sanity checks. This might change in the future to
   act the same as the UTF-8 ones. UTF-16 and UTF-32 byte ordering is
   native. */

bool valid_utf8(const char *);

char *sanitize_utf8(const char *restrict orig, int len, int *outlen,
                    const char *name) __attribute_malloc__;

char *latin1_to_utf8(const char *restrict, int, int *,
                     const char *) __attribute_malloc__;
char *latin1_to_utf8_tn(const char *restrict, int, int *, bool,
                        const char *) __attribute_malloc__;

char *utf8_to_latin1(const char *restrict, int, int *, bool,
                     const char *) __attribute_malloc__;
char *utf8_to_latin1_us(const char *restrict, int, int *, bool,
                        const char *) __attribute_malloc__;

char *utf16_to_utf8(const UChar *, int, int *,
                    const char *) __attribute_malloc__;
UChar *utf8_to_utf16(const char *restrict, int, int *,
                     const char *) __attribute_malloc__;
UChar *utf8_to_utf16_us(const char *restrict, int, int *,
                        const char *) __attribute_malloc__;

char *utf32_to_utf8(const UChar32 *, int, int *,
                    const char *) __attribute_malloc__;
UChar32 *utf8_to_utf32(const char *restrict, int, int *,
                       const char *) __attribute_malloc__;
UChar32 *utf8_to_utf32_us(const char *restrict, int, int *,
                          const char *) __attribute_malloc__;

UChar *latin1_to_utf16(const char *restrict, int, int *,
                       const char *) __attribute_malloc__;
char *utf16_to_latin1(const UChar *, int, int *, bool,
                      const char *) __attribute_malloc__;

UChar32 *latin1_to_utf32(const char *restrict, int, int *,
                         const char *) __attribute_malloc__;
char *utf32_to_latin1(const UChar32 *, int, int *, bool,
                      const char *) __attribute_malloc__;

#ifdef HAVE_ICU

/* Unicode normalization */
enum normalization_type { NORM_NFC, NORM_NFD, NORM_NFKC, NORM_NFKD };
UChar *normalize_utf16(enum normalization_type, const UChar *, int, int *,
                       const char *) __attribute_malloc__;
char *normalize_utf8(enum normalization_type, const char *restrict, int, int *,
                     const char *) __attribute_malloc__;

char *translate_utf8_to_latin1(const char *restrict, int, int *,
                               const char *) __attribute_malloc__;

/* Case conversions */
char *latin1_to_lower(const char *restrict, int, int *,
                      const char *) __attribute_malloc__;
char *latin1_to_upper(const char *restrict, int, int *,
                      const char *) __attribute_malloc__;
char *utf8_to_lower(const char *restrict, int, int *,
                    const char *) __attribute_malloc__;
char *utf8_to_upper(const char *restrict, int, int *,
                    const char *) __attribute_malloc__;

#endif

#endif
