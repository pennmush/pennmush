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
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#ifdef HAVE_SSE42
#include <emmintrin.h>
#include <nmmintrin.h>
#endif

#include "compile.h"
#include "mushtype.h"
#include "myutf8.h"
#include "sqlite3.h"

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
char *next_utoken(char *str, UChar32 sep);
char *split_token(char **sp, char sep);
char *split_utoken(char **sp, UChar32 sep);
char *chopstr(const char *str, size_t lim);
bool string_prefix(const char *restrict string, const char *restrict prefix);
bool string_prefixe(const char *restrict string, const char *restrict prefix);
const char *string_match(const char *src, const char *sub);
char *strupper(const char *s);
char *strupper_a(const char *s, const char *name) __attribute_malloc__;
char *strupper_r(const char *restrict s, char *restrict d, size_t len);
char *strlower(const char *s);
char *strlower_a(const char *s, const char *name) __attribute_malloc__;
char *strlower_r(const char *restrict s, char *restrict d, size_t len);
char *strinitial(const char *s) __attribute__((__deprecated__));
char *strinitial_r(const char *restrict s, char *restrict d, size_t len);
char *upcasestr(char *s);
char *skip_space(const char *s);
char *seek_char(const char *s, char c);
char *seek_uchar(const char *s, UChar32 c);
char *mush_strndup(const char *src, size_t len,
                   const char *check) __attribute_malloc__;
char *mush_strndup_cp(const char *src, size_t len,
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

/* Append an ASCII character to the end of a BUFFER_LEN long string. */
int safe_chr(char c, char *buff, char **bp);
/* Append a Unicode character to the end of a BUFFER_LEN long string. */
int safe_uchar(UChar32 c, char *buff, char **bp);

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
/* Append a Unicode string to a UTF-8 buffer */
int safe_utf8(const char *s, char *buff, char **bp);
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

/* Append a UChar32 to a sqlite3_str object */
void sqlite3_str_appenduchar(sqlite3_str *str, UChar32 c);

/* pennstr growable string builder. Currently a thin wrapper over
   the sqlite3_str API */

typedef sqlite3_str pennstr;

pennstr *ps_new(void) __attribute_malloc__;
void ps_free(pennstr *);
char *ps_finish(pennstr *);
void ps_free_str(char *);
void ps_safe_number(pennstr *, NVAL);
void ps_safe_format(pennstr *, const char *fmt, ...)
  __attribute__((__format__(__printf__, 2, 3)));
void ps_safe_strl_cp(pennstr *, const char *, int);

/** Append a single ASCII character to a pennstr */
static inline void
ps_safe_chr(pennstr *ps, char c)
{
  sqlite3_str_appendchar(ps, 1, c);
}

/** Append N copies of an ASCII character to a pennstr */
static inline void
ps_safe_fill(pennstr *ps, char c, int n)
{
  sqlite3_str_appendchar(ps, n, c);
}

/** Append a single Unicode character to a pennstr */
static inline void
ps_safe_uchar(pennstr *ps, UChar32 c)
{
  sqlite3_str_appenduchar(ps, c);
}

/** Append a UTF-8 string to a pennstr */
static inline void
ps_safe_str(pennstr *ps, const char *s)
{
  /* Utf-8 string */
  sqlite3_str_appendall(ps, s);
}

/** Append an integer to a pennstr */
static inline void
ps_safe_integer(pennstr *ps, sqlite3_int64 i)
{
  sqlite3_str_appendf(ps, "%lld", i);
}

/** Append an unsigned integer to a pennstr */
static inline void
ps_safe_uinteger(pennstr *ps, sqlite3_uint64 i)
{
  sqlite3_str_appendf(ps, "%llu", i);
}

/** Append a dbref to a pennstr */
static inline void
ps_safe_dbref(pennstr *ps, dbref d)
{
  sqlite3_str_appendf(ps, "#%d", d);
}

/** Return the current UTF-8 string managed by the pennstr. Any
 * further ps_safe-XXX() functions called on this pennstr invalidate
 * the pointer. */
static inline const char *
ps_str(pennstr *ps)
{
  return sqlite3_str_value(ps);
}

/** Return the current length of the pennstr string in bytes */
static inline int
ps_len(pennstr *ps)
{
  return sqlite3_str_length(ps);
}

/** Append a pennstr buffer to a UTF-8 BUFFER_LEN string.
 *
 * \return 0 on success, 1 on failure
 */
static inline int
safe_pennstr(pennstr *ps, char *buff, char **bp)
{
  return safe_strl(ps_str(ps), ps_len(ps), buff, bp);
}

/** Reset the pennstr to an empty string */
static inline void
ps_reset(pennstr *ps)
{
  sqlite3_str_reset(ps);
}

char *mush_strncpy(char *restrict, const char *, size_t);

char *replace_string(const char *restrict old, const char *restrict newbit,
                     const char *restrict string) __attribute_malloc__;

char *replace_string2(const char *const old[2], const char *const newbits[2],
                      const char *restrict string) __attribute_malloc__;

char *copy_up_to(char *RESTRICT dest, const char *RESTRICT src, char c);
char *trim_space_sep(char *str, char sep);
int do_wordcount(char *str, char sep);
int do_uwordcount(char *str, UChar32 sep);
char *remove_word(char *list, char *word, char sep);
char *remove_uword(char *list, char *word, UChar32 sep) __attribute_malloc__;
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
