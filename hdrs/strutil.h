/**
 * \file strutil.h
 *
 * \brief Header file for various string manipulation functions.
 */

#ifndef __STRUTIL_H
#define __STRUTIL_H

#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include "compile.h"
#include "mushtype.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */

#ifdef HAVE_SSE42
#include <emmintrin.h>
#include <nmmintrin.h>
#endif

extern const char *const standard_tokens[2]; /* ## and #@ */

#ifndef HAVE_STRCASECMP
#ifdef HAVE__STRICMP
#define strcasecmp(s1, s2) _stricmp((s1), (s2))
#else
int strcasecmp(const char *s1, const char *s2);
#endif
#endif

#ifndef HAVE_STRNCASECMP
#ifdef HAVE__STRNICMP
#define strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))
#else
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
#endif

char *next_token(char *str, char sep);
char *split_token(char **sp, char sep);
char *chopstr(const char *str, size_t lim);
bool string_prefix(const char *restrict string, const char *restrict prefix);
bool string_prefixe(const char *restrict string, const char *restrict prefix);
const char *string_match(const char *src, const char *sub);
char *strupper(const char *s);
char *strlower(const char *s);
char *strinitial(const char *s);
char *upcasestr(char *s);
char *skip_space(const char *s);
char *seek_char(const char *s, char c);
char *mush_strndup(const char *src, size_t len,
                   const char *check) __attribute_malloc__;
int mush_vsnprintf(char *, size_t, const char *, va_list);

#ifndef HAVE_STRDUP
char *strdup(const char *s) __attribute_malloc__;
#endif
char *mush_strdup(const char *s, const char *check) __attribute_malloc__;

#ifdef HAVE__STRNCOLL
#define strncoll(s1, s2, n) _strncoll((s1), (s2), (n))
#else
int strncoll(const char *s1, const char *s2, size_t t);
#endif

#ifdef HAVE__STRICOLL
#define strcasecoll(s1, s2) _stricoll((s1), (s2))
#else
int strcasecoll(const char *s1, const char *s2);
#endif

#ifdef HAVE__STRNICOLL
#define strncasecoll(s1, s2, n) _strnicoll((s1), (s2), (n))
#else
int strncasecoll(const char *s1, const char *s2, size_t t);
#endif

size_t remove_trailing_whitespace(char *, size_t);

/** Append a character to the end of a BUFFER_LEN long string. */
int safe_chr(char c, char *buff, char **bp);

/* Like sprintf */
int safe_format(char *buff, char **bp, const char *restrict fmt, ...)
  __attribute__((__format__(__printf__, 3, 4)));
/* Append an int to the end of a buffer */
int safe_integer(intmax_t i, char *buff, char **bp);
int safe_uinteger(uintmax_t, char *buff, char **bp);
/* Same, but for a SBUF_LEN buffer, not BUFFER_LEN */
#define SBUF_LEN 128 /**< A short buffer */
int safe_integer_sbuf(intmax_t i, char *buff, char **bp);
/* Append a NVAL to a string */
int safe_number(NVAL n, char *buff, char **bp);
/* Append a dbref to a buffer */
int safe_dbref(dbref d, char *buff, char **bp);
/* Append a string to a buffer */
int safe_str(const char *s, char *buff, char **bp);
/* Append a string to a buffer, sticking it in quotes if there's a space */
int safe_str_space(const char *s, char *buff, char **bp);
/* Append len characters of a string to a buffer */
int safe_strl(const char *s, size_t len, char *buff, char **bp);
int safe_hexchar(char c, char *buff, char **bp);
/* Append a base16 encoded block of bytes to a buffer */
int safe_hexstr(uint8_t *data, int len, char *buff, char **bp);
/** Append a boolean to the end of a string */
#define safe_boolean(x, buf, bufp) safe_chr((x) ? '1' : '0', (buf), (bufp))
int safe_time_t(time_t t, char *buff, char **bp);

/* Append N copies of the character X to the end of a string */
int safe_fill(char x, size_t n, char *buff, char **bp);
int safe_fill_to(char x, size_t n, char *buff);
/* Append an accented string */
int safe_accent(const char *restrict base, const char *restrict tmplate,
                size_t len, char *buff, char **bp);

char *mush_strncpy(char *restrict, const char *, size_t);

char *replace_string(const char *restrict old, const char *restrict newbit,
                     const char *restrict string) __attribute_malloc__;

char *replace_string2(const char *const old[2], const char *const newbits[2],
                      const char *restrict string) __attribute_malloc__;

char *copy_up_to(char *RESTRICT dest, const char *RESTRICT src, char c);
char *trim_space_sep(char *str, char sep);
int do_wordcount(char *str, char sep);
char *remove_word(char *list, char *word, char sep);
char *next_in_list(const char **head);
void safe_itemizer(int cur_num, int done, const char *delim,
                   const char *conjoin, const char *space, char *buff,
                   char **bp);
char *show_time(time_t t, bool utc);
char *show_tm(struct tm *t);

/* Functions to work on strings holding key:value pairs or a single
 * default value  */
const char *keystr_find_full(const char *restrict map, const char *restrict key,
                             const char *restrict deflt, char delim);
static inline const char *
keystr_find(const char *restrict map, const char *restrict key)
{
  return keystr_find_full(map, key, NULL, ':');
}

static inline const char *
keystr_find_d(const char *restrict map, const char *restrict key,
              const char *restrict deflt)
{
  return keystr_find_full(map, key, deflt, ':');
}

/** Return true if a character is in a short string.
 *
 * A short string: Is 16-byte aligned. Has at least 16 bytes
 * available in the array.
 *
 * \param ss a short string.
 * \param len The number of characters used in the string. Cannot be more than
 * 16.
 * \param c The character to look for.
 */

static inline bool
exists_in_ss(const char ss[static 16], int len, char c)
{
#ifdef HAVE_SSE42
  __m128i a = _mm_cvtsi32_si128(c);
  __m128i b = _mm_load_si128((const __m128i *) ss);

  return _mm_cmpestrc(a, 1, b, len, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY);

#else
  /* Scalar approach */
  for (int n = 0; n < len; n += 1) {
    if (ss[n] == c)
      return true;
  }
  return false;
#endif
}

#endif /* __STRUTIL_H */
