/**
 * \file charconv.c
 *
 * \brief Character set conversion functions.
 */

#include "copyrite.h"

#include "mysocket.h"
#include "mymalloc.h"
#include "charconv.h"
#include "log.h"

#ifdef HAVE_SSE2
#include <string.h>
#include <emmintrin.h>
#ifdef WIN32
#include <intrin.h>
#endif
#endif

#ifdef HAVE_SSE42
#include <nmmintrin.h>
#endif

#if defined(WIN32) && !defined(HAVE_FFS)

/* Windows version of ffs() */

int
ffs(int i)
{
  unsigned long pos;

  if (_BitScanForward(&pos, (unsigned long) i))
    return (int) pos;
  else
    return 0;
}

#define HAVE_FFS
#endif

/**
 * Convert a latin-1 encoded string to utf-8.
 *
 *
 * \param s the latin-1 string.
 * \param latin the length of the string.
 * \param outlen the number of bytes of the returned string, NOT counting the
 * trailing nul.
 * \param telnet true if we should handle telnet escape sequences.
 * \return a newly allocated utf-8 string.
 */
char *
latin1_to_utf8(const char *latin, int len, int *outlen, bool telnet)
{
  int bytes = 1;
  int outbytes = 0;

  const unsigned char *s = (const unsigned char *) latin;

#ifdef HAVE_SSE42
  __m128i zeros = _mm_setzero_si128();
  const char ar[16] __attribute__((aligned(16))) = {0x01, 0x7F};
  __m128i ascii_range = _mm_load_si128((__m128i *) ar);
#endif

  /* Compute the number of bytes in the output string */
  for (int n = 0; n < len;) {

#ifdef HAVE_SSE42
    __m128i chunk = _mm_loadu_si128((__m128i *) (s + n));
    int eos = _mm_cmpistri(zeros, chunk,
                           _SIDD_UBYTE_OPS + _SIDD_CMP_EQUAL_EACH +
                             _SIDD_LEAST_SIGNIFICANT);
    int ascii = _mm_cvtsi128_si32(
      _mm_cmpistrm(ascii_range, chunk, _SIDD_UBYTE_OPS + _SIDD_CMP_RANGES));
    ascii = _mm_popcnt_u32(ascii); // Assume that SSE4.2 means popcnt too
    bytes += ascii;
    bytes += 2 * (eos - ascii);
    n += eos;
#else

#ifdef HAVE_SSE2
    /* Handle a chunk of 16 chars all together */
    if ((len - n) >= 16) {
      __m128i chunk = _mm_loadu_si128((__m128i *) (s + n));
      unsigned int eightbits = _mm_movemask_epi8(chunk);
      /* For best results on CPUs with popcount instructions, compile
         with appropriate -march=XXX setting or -mpopcnt. But those
         have SSE4.2 which is better yet.  */
      int set = __builtin_popcount(eightbits);
      bytes += set * 2;
      bytes += 16 - set;
      n += 16;
      continue;
    }
#endif

    if (s[n] < 128)
      bytes += 1;
    else
      bytes += 2;
    n += 1;
#endif
  }

  unsigned char *utf8 = mush_malloc(bytes, "string");

  unsigned char *u = utf8;

#define ENCODE_CHAR(u, c)                                                      \
  do {                                                                         \
    *u++ = 0xC0 | (c >> 6);                                                    \
    *u++ = 0x80 | (c & 0x3F);                                                  \
  } while (0)

  for (int n = 0; n < len;) {

#if defined(HAVE_SSE42)
    // Use SSE4.2 instructions to find the first 8 bit character and
    // copy everything before that point in a chunk. Faster than the
    // SSE2 version that follows.

    __m128i chunk = _mm_loadu_si128((__m128i *) (s + n));
    int eos = _mm_cmpistri(zeros, chunk,
                           _SIDD_UBYTE_OPS + _SIDD_CMP_EQUAL_EACH +
                             _SIDD_LEAST_SIGNIFICANT);
    int nonascii =
      _mm_cmpistri(ascii_range, chunk,
                   _SIDD_UBYTE_OPS + _SIDD_CMP_RANGES +
                     _SIDD_NEGATIVE_POLARITY + _SIDD_LEAST_SIGNIFICANT);
    // printf("For fragment '%.16s': n=%d, nonascii=%d, eos=%d\n",
    // s + n, n, nonascii, eos);
    if (nonascii == 16 && eos == 16) {
      // puts("Copying complete chunk");
      _mm_storeu_si128((__m128i *) u, chunk);
      n += 16;
      u += 16;
      outbytes += 16;
      continue;
    } else if (nonascii == 1) {
      // puts("Copying single leading ascii char");
      *u++ = s[n++];
      outbytes += 1;
      continue;
    } else if (nonascii > 0) {
      // puts("Copying partial chunk");
      int nlen = nonascii;
      memcpy(u, s + n, nlen);
      n += nlen;
      u += nlen;
      outbytes += nlen;
      if (n >= len)
        break;
    }

#elif defined(HAVE_SSE2) && defined(HAVE_FFS)

    if ((len - n) >= 16) {
      __m128i chunk = _mm_loadu_si128((__m128i *) (s + n));
      int nonascii = ffs(_mm_movemask_epi8(chunk));
      if (nonascii == 0) {
        // Copy a chunk of 16 ascii characters all at once
        _mm_storeu_si128((__m128i *) u, chunk);
        n += 16;
        u += 16;
        outbytes += 16;
        continue;
      } else if (nonascii > 1) {
        // Copy up to the first 8 bit character and process it in the
        // rest of the loop.
        nonascii -= 1;
        memcpy(u, s + n, nonascii);
        n += nonascii;
        u += nonascii;
        outbytes += nonascii;
      }
    }
#endif

    if (telnet && s[n] == IAC) {
      /* Single IAC is the start of a telnet sequence. Double IAC IAC is
       * an escape for a single byte. Either way it should never appear
       * at the end of a string by itself. */
      n += 1;
      switch (s[n]) {
      case IAC:
        ENCODE_CHAR(u, IAC);
        n += 1;
        outbytes += 2;
        break;
      case SB:
        *u++ = IAC;
        outbytes += 1;
        while (s[n] != SE) {
          *u++ = s[n];
          outbytes += 1;
          n += 1;
        }
        *u++ = SE;
        n += 1;
        outbytes += 1;
        break;
      case DO:
      case DONT:
      case WILL:
      case WONT:
        *u++ = IAC;
        *u++ = s[n];
        *u++ = s[n + 1];
        n += 2;
        outbytes += 3;
        break;
      case NOP:
        *u++ = IAC;
        *u++ = NOP;
        n += 1;
        outbytes += 2;
        break;
      default:
        /* This should never be reached. */
        do_rawlog(LT_ERR, "Invalid telnet sequence character %X", s[n]);
      }
    } else if (s[n] < 128) {
      *u++ = s[n++];
      outbytes += 1;
    } else {
      ENCODE_CHAR(u, s[n]);
      n += 1;
      outbytes += 2;
    }
  }
  *u = '\0';
  if (outlen)
    *outlen = outbytes;
  return (char *) utf8;
#undef ENCODE_CHAR
}

/**
 * Convert a UTF-8 encoded string to Latin-1
 *
 * \param utf8 a valid utf-8 string
 * \param outlen the length of the returned string including trailing nul
 * \return a newly allocated latin-1 string
 */
char *
utf8_to_latin1(const char *utf8, int *outlen)
{
  const unsigned char *u = (const unsigned char *) utf8;
  int bytes = 1;
  int ulen = 0;
  int n;

#ifdef HAVE_SSE42
  __m128i zeros = _mm_setzero_si128();
  const unsigned char maskarray[16] __attribute__((aligned(16))) = {
    0x1, 0x7F, 0xC1, 0xDF, 0xE1, 0xEF, 0xF1, 0xF7};
  __m128i mask = _mm_load_si128((__m128i *) maskarray);

  while (1) {
    __m128i chunk = _mm_loadu_si128((__m128i *) u);
    int r = _mm_cvtsi128_si32(
      _mm_cmpistrm(mask, chunk, _SIDD_UBYTE_OPS + _SIDD_CMP_RANGES));
    int chars = _mm_popcnt_u32(r);
    bytes += chars;
    int eos = _mm_cmpistri(zeros, chunk,
                           _SIDD_UBYTE_OPS + _SIDD_CMP_EQUAL_EACH +
                             _SIDD_LEAST_SIGNIFICANT);
    // fprintf("Chunk '%.16s': chars = %d, eos = %d\n",
    //	    u, chars, eos);
    u += 16;
    if (eos != 16) {
      ulen += eos;
      break;
    } else {
      ulen += 16;
    }
  }

#else

  while (*u) {
    if (*u < 128) {
      bytes += 1;
    } else if ((*u & 0xC0) == 0x80) {
      // Skip continuation bytes
    } else {
      bytes += 1;
    }
    u += 1;
    ulen += 1;
  }
#endif

  if (outlen)
    *outlen = bytes;

  char *s = mush_malloc(bytes, "string");
  unsigned char *p = (unsigned char *) s;

#ifdef HAVE_SSE42
  const char ar[16] __attribute__((aligned(16))) = {0x01, 0x7F};
  __m128i ascii_range = _mm_load_si128((__m128i *) ar);
#endif

  for (n = 0; n < ulen;) {

#if defined(HAVE_SSE42)
    // Use SSE4.2 instructions to find the first 8 bit character and
    // copy everything before that point in a chunk. Faster than the
    // SSE2 version that follows.

    __m128i chunk = _mm_loadu_si128((__m128i *) (utf8 + n));
    int eos = _mm_cmpistri(zeros, chunk,
                           _SIDD_UBYTE_OPS + _SIDD_CMP_EQUAL_EACH +
                             _SIDD_LEAST_SIGNIFICANT);
    int nonascii =
      _mm_cmpistri(ascii_range, chunk,
                   _SIDD_UBYTE_OPS + _SIDD_CMP_RANGES +
                     _SIDD_NEGATIVE_POLARITY + _SIDD_LEAST_SIGNIFICANT);
    //    printf("For fragment '%.16s': n=%d, nonascii=%d, eos=%d\n",
    //	   utf8 + n, n, nonascii, eos);
    if (nonascii == 16 && eos == 16) {
      // puts("Copying complete chunk");
      _mm_storeu_si128((__m128i *) p, chunk);
      n += 16;
      p += 16;
      continue;
    } else if (nonascii == 1) {
      // puts("Copying single leading ascii char");
      *p++ = utf8[n++];
      continue;
    } else if (nonascii > 0) {
      // puts("Copying partial chunk");
      int len = nonascii;
      memcpy(p, utf8 + n, len);
      n += len;
      p += len;
      if (n >= ulen)
        break;
    }

#elif defined(HAVE_SSE2) && defined(HAVE_FFS)

    if ((ulen - n) >= 16) {
      __m128i chunk = _mm_loadu_si128((__m128i *) (utf8 + n));
      int nonascii = ffs(_mm_movemask_epi8(chunk));
      if (nonascii == 0) {
        // Copy a chunk of 16 ascii characters all at once.
        _mm_storeu_si128((__m128i *) p, chunk);
        n += 16;
        p += 16;
        continue;
      } else if (nonascii > 1) {
        // Copy up to the first 8 bit character and process it in the
        // rest of the loop.
        nonascii -= 1;
        memcpy(p, utf8 + n, nonascii);
        n += nonascii;
        p += nonascii;
        if (n >= ulen)
          break;
      }
    }
#endif

    if (utf8[n] < 128) {
      *p++ = utf8[n];
      n += 1;
    } else if ((utf8[n] & 0xE0) == 0xC0) {
      if ((utf8[n] & 0x1F) <= 0x03) {
        unsigned char output = utf8[n] << 6;
        n += 1;
        output |= utf8[n] & 0x3F;
        *p++ = output;
        n += 1;
      } else {
        *p++ = '?';
        n += 2;
      }
    } else if ((utf8[n] & 0xF8) == 0xF0) {
      *p++ = '?';
      n += 4;
    } else if ((utf8[n] & 0xF0) == 0xE0) {
      *p++ = '?';
      n += 3;
    }
  }

  *p = '\0';
  return s;
}

/**
 * Check to see if a string is valid utf-8 or not.
 * \param utf8 string to validate
 * \return true or false
 */
bool
valid_utf8(const char *utf8)
{
  const unsigned char *u = (const unsigned char *) utf8;

  int nconts = 0;

  while (*u) {
    if (*u < 128) {
      if (nconts)
        return 0;
    } else if ((*u & 0xF8) == 0xF0) {
      if (nconts)
        return 0;
      nconts = 3;
    } else if ((*u & 0xF0) == 0xE0) {
      if (nconts)
        return 0;
      nconts = 2;
    } else if ((*u & 0xE0) == 0xC0) {
      if (nconts)
        return 0;
      nconts = 1;
    } else if ((*u & 0xC0) == 0x80) {
      if (nconts > 0)
        nconts -= 1;
      else
        return 0;
    } else {
      return 0;
    }
    u += 1;
  }

  return nconts == 0;
}
