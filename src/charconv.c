/**
 * \file charconv.c
 *
 * \brief Character set conversion functions.
 */

#include "copyrite.h"

#ifdef HAVE_ICU
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#include <unicode/unorm2.h>
#endif

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

#ifdef HAVE_ICU
static UConverter *loc_latin1_cnv = NULL;
static UConverter *loc_utf32_cnv = NULL;

static UConverter *
make_converter(const char *charset)
{
  UConverter *cnv;
  UErrorCode uerr = 0;
  cnv = ucnv_open(charset, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Unable to open ICU %s converter: %s\n",
	      charset, u_errorName(uerr));
    return NULL;
  }

  uerr = 0;
  ucnv_setSubstChars(cnv, "?", 1, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Unable to set ICU %s substitution character: %s\n",
	      charset, u_errorName(uerr));
    ucnv_close(cnv);
    return NULL;
  }
  return cnv;
}

static UConverter *
get_latin1_cnv(void)
{
  /* thread local when merging into threaded */
  if (!loc_latin1_cnv) {
    loc_latin1_cnv = make_converter("LATIN-1");
  }
  return loc_latin1_cnv;
}

static UConverter *
get_utf32_cnv(void)
{
  /* thread local when merging into threaded */
  if (!loc_utf32_cnv) {
    loc_utf32_cnv = make_converter("UTF-32");
  }
  return loc_utf32_cnv;
}

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
 * \param s the latin-1 string.
 * \param len the length of the string.
 * \param outlen set to the number of bytes of the returned string, NOT counting the trailing nul.
 * \param name memcheck tag
 * \return a newly allocated utf-8 string.
 */
char*
latin1_to_utf8(const char * restrict latin1, int len, int *outlen, const char * restrict name)
{
#ifdef HAVE_ICU
  UErrorCode uerr = 0;
  int32_t destlen;
  char *utf8;
  UConverter *latin1_cnv = get_latin1_cnv();
  
  if (!latin1_cnv) {
    return NULL;
  }

  destlen = ucnv_toAlgorithmic(UCNV_UTF8, latin1_cnv, NULL, 0, latin1, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf8 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += 1;
  utf8 = mush_calloc(destlen, 1, name);
  uerr = 0;
  destlen = ucnv_toAlgorithmic(UCNV_UTF8, latin1_cnv, utf8, destlen, latin1, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf8 failed: %s\n",
	      u_errorName(uerr));
    mush_free(utf8, name);
    return NULL;
  }
  if (outlen) {
    *outlen = destlen;
  }
  return utf8;
#else
  return latin1_to_utf8_tn(latin1, len, outlen, 0, name);
#endif
}

/**
 * Convert a latin-1 encoded string to utf-8.
 *
 *
 * \param s the latin-1 string.
 * \param latin the length of the string.
 * \param outlen the number of bytes of the returned string, NOT counting the trailing nul.
 * \param telnet true if we should handle telnet escape sequences.
 * \param name memcheck tag.
 * \return a newly allocated utf-8 string.
 */
char *
latin1_to_utf8_tn(const char * RESTRICT latin, int len, int *outlen, bool telnet,
		  const char * RESTRICT name)
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

  unsigned char *utf8 = mush_malloc(bytes, name);

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
 * \param len the length of the string in bytes
 * \param outlen set to the length of the returned string NOT including trailing nul
 * \param name memcheck tag
 * \return a newly allocated latin-1 string
 */
char *
utf8_to_latin1(const char * RESTRICT utf8, int len, int *outlen, const char *name)
{
#ifdef HAVE_ICU
  int32_t destlen;
  char *latin1;
  UErrorCode uerr = 0;
  UConverter *latin1_cnv = get_latin1_cnv();
  
  if (!latin1_cnv) {
    return NULL;
  }
  
  destlen = ucnv_fromAlgorithmic(latin1_cnv, UCNV_UTF8, NULL, 0, utf8, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from utf8 to latin1 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += 1;
  latin1 = mush_calloc(destlen, 1, name);
  uerr = 0;
  ucnv_fromAlgorithmic(latin1_cnv, UCNV_UTF8, latin1, destlen, utf8, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from utf8 to latin1 failed: %s\n",
	      u_errorName(uerr));
    mush_free(latin1, name);
    return NULL;
  }
  if (outlen) {
    *outlen = destlen;
  }
  return latin1;
  
#else
  
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
    *outlen = bytes - 1;

  char *s = mush_malloc(bytes, name);
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
#endif
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
  int32_t cp = 0;
  int nconts = 0, bytes = 0;

  while (*u) {
    if (*u < 128) {
      if (nconts) {
        return 0;
      }
      bytes = 1;
      cp = *u;
    } else if ((*u & 0xF8) == 0xF0) {
      if (nconts) {
        return 0;
      }
      bytes = 4;
      cp = *u & 0x7;
      nconts = 3;
    } else if ((*u & 0xF0) == 0xE0) {
      if (nconts) {
        return 0;
      }
      bytes = 3;
      cp = *u & 0xF;
      nconts = 2;
    } else if ((*u & 0xE0) == 0xC0) {
      if (nconts) {
        return 0;
      }
      cp = *u & 0x1F;
      bytes = 2;
      nconts = 1;
    } else if ((*u & 0xC0) == 0x80) {
      if (nconts > 0) {
        nconts -= 1;
	cp = (cp << 6) | (*u & 0x3F);
      } else {
        return 0;
      }
    } else {
      return 0;
    }
    if (nconts == 0) {
      if (cp >= 0xD800 && cp <= 0xDFFF) {
	/* UTF-16 surrogate pair characters are invalid in UTF-8 */
	return 0;
	/* Now catch overlong byte sequences. */
      } else if (bytes == 2 && !(cp >= 0x80 && cp <= 0x07FF)) {
	return 0;
      } else if (bytes == 3 && !(cp >= 0x0800 && cp <= 0xFFFF)) {
	return 0;
      } else if (bytes == 4 && !(cp >= 0x10000 && cp <= 0x10FFFF)) {
	return 0;
      } else if (cp > 0x10FFFF) {
	/* Too large for unicode */
	return 0;
      }
    }
    u += 1;
  }

  return nconts == 0;
}

#ifdef HAVE_ICU

/** Convert a UTF-16 encoded string to UTF-8.
 *
 * \parm utf16 the UTF-16 string
 * \param len the length of the string. -1 to use 0-terminated length
 * \param outlen pointer to store the length of the UTF-8 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-8 string
 */
char *
utf16_to_utf8(const UChar *utf16, int len, int *outlen, const char *name)
{
  char *utf8;
  int32_t destlen, destwritten;
  UErrorCode uerr = 0;

  u_strToUTF8(NULL, 0, &destwritten, utf16, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    return NULL;
  }

  destlen = destwritten + 1;
  utf8 = mush_calloc(destlen, 1, name);

  uerr = 0;
  u_strToUTF8(utf8, destlen, outlen, utf16, len, &uerr);
  if (U_FAILURE(uerr)) {
    mush_free(utf8, name);
    return NULL;
  }
  
  return utf8;
}

/** Convert a UTF-8 encoded string to UTF-16.
 *
 * \parm utf8 the UTF-8 string
 * \param len the length of the string in bytes. -1 to use 0-terminated length
 * \param outlen pointer to store the length of the UTF-16 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-16 string
 */
UChar *
utf8_to_utf16(const char * restrict utf8, int len, int *outlen, const char * restrict name)
{
  UChar *utf16;
  int32_t destlen, destwritten;
  UErrorCode uerr = 0;

  u_strFromUTF8(NULL, 0, &destwritten, utf8, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    return NULL;
  }

  destlen = destwritten + 1;
  utf16 = mush_calloc(destlen, sizeof(UChar), name);

  uerr = 0;
  u_strFromUTF8(utf16, destlen, outlen, utf8, len, &uerr);
  if (U_FAILURE(uerr)) {
    mush_free(utf16, name);
    return NULL;
  }
  
  return utf16;
}

/** Convert a UTF-8 encoded string to UTF-32
 *
 * \parm utf8 the UTF-8 string
 * \param len the length of the string in bytes.
 * \param outlen pointer to store the length of the UTF-32 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-32 string
 */
UChar32 *
utf8_to_utf32(const char * restrict utf8, int len, int *outlen, const char * restrict name)
{
  int32_t destlen;
  UChar32 *utf32;
  UErrorCode uerr = 0;
  UConverter *utf32_cnv = get_utf32_cnv();
  
  if (!utf32_cnv) {
    return NULL;
  }

  destlen = ucnv_fromAlgorithmic(utf32_cnv, UCNV_UTF8, NULL, 0, utf8, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from utf8 to utf32 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += sizeof(UChar32);
  utf32 = mush_calloc(destlen, 1, name);
  uerr = 0;
  ucnv_fromAlgorithmic(utf32_cnv, UCNV_UTF8, (char *)utf32, destlen, utf8, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from utf8 to latin1 failed: %s\n",
	      u_errorName(uerr));
    mush_free(utf32, name);
    return NULL;
  }
  if (outlen) {
    *outlen = (destlen / sizeof(UChar32)) - 1;
  }
  return utf32;
}

/** Convert a UTF-32 encoded string to UTF-8
 *
 * \parm utf32 the UTF-32 string
 * \param len the length of the string.
 * \param outlen pointer to store the length of the UTF-8 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-8 string
 */
char *
utf32_to_utf8(const UChar32 *utf32, int len, int *outlen, const char * name)
{
  int32_t destlen;
  char *utf8;
  UErrorCode uerr = 0;
  UConverter *utf32_cnv = get_utf32_cnv();
  
  if (!utf32_cnv) {
    return NULL;
  }

  len *= sizeof(UChar32);
  destlen = ucnv_toAlgorithmic(UCNV_UTF8, utf32_cnv, NULL, 0, (char *)utf32, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from utf32 to utf8 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += 1;
  utf8 = mush_calloc(destlen, 1, name);
  uerr = 0;
  ucnv_toAlgorithmic(UCNV_UTF8, utf32_cnv, utf8, destlen, (char *)utf32, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from utf32 to utf8 failed: %s\n",
	      u_errorName(uerr));
    mush_free(utf8, name);
    return NULL;
  }
  if (outlen) {
    *outlen = destlen - 1; 
  }
  return utf8;
}

/** Convert a latin-1 encoded string to UTF-32
 *
 * \parm latin1 the latin-1 string
 * \param len the length of the string in bytes.
 * \param outlen pointer to store the length of the UTF-32 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-32 string
 */
UChar32 *
latin1_to_utf32(const char * restrict latin1, int len, int *outlen, const char * restrict name)
{
  int32_t destlen;
  UChar32 *utf32;
  UErrorCode uerr = 0;
  UConverter *latin1_cnv = get_latin1_cnv();
  
  if (!latin1_cnv) {
    return NULL;
  }

  destlen = ucnv_toAlgorithmic(UCNV_UTF32, latin1_cnv, NULL, 0, latin1, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf32 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += sizeof(UChar32);
  utf32 = mush_calloc(destlen, 1, name);
  uerr = 0;
  ucnv_toAlgorithmic(UCNV_UTF32, latin1_cnv, (char *)utf32, destlen, latin1, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf32 failed: %s\n",
	      u_errorName(uerr));
    mush_free(utf32, name);
    return NULL;
  }
  if (outlen) {
    *outlen = (destlen / sizeof(UChar32)) - 1;
  }
  return utf32;
}

/** Convert a UTF-32 encoded string to latin-1
 *
 * \parm utf32 the UTF-32 string
 * \param len the length of the string.
 * \param outlen pointer to store the length of the latin-1 string
 * \param name memcheck tag for new string
 * \return newly allocated latin-1 string
 */
char *
utf32_to_latin1(const UChar32 *utf32, int len, int *outlen, const char * name)
{
  int32_t destlen;
  char *latin1;
  UErrorCode uerr = 0;
  UConverter *latin1_cnv = get_latin1_cnv();
  
  if (!latin1_cnv) {
    return NULL;
  }

  len *= sizeof(UChar32);
  destlen = ucnv_fromAlgorithmic(latin1_cnv, UCNV_UTF32, NULL, 0, (char *)utf32, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from utf32 to latin1 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += 1;
  latin1 = mush_calloc(destlen, 1, name);
  uerr = 0;
  ucnv_fromAlgorithmic(latin1_cnv, UCNV_UTF32, latin1, destlen, (char *)utf32, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from utf32 to latin1 failed: %s\n",
	      u_errorName(uerr));
    mush_free(latin1, name);
    return NULL;
  }
  if (outlen) {
    *outlen = destlen - 1; 
  }
  return latin1;
}

/**
 * Convert a latin-1 encoded string to utf-16.
 *
 * \param s the latin-1 string.
 * \param len the length of the string.
 * \param outlen set to the length of the returned string, NOT counting the trailing nul.
 * \param name memcheck tag
 * \return a newly allocated utf-16 string.
 */
UChar *
latin1_to_utf16(const char * restrict latin1, int len, int *outlen, const char * restrict name)
{
  UErrorCode uerr = 0;
  int32_t destlen;
  UChar *utf16;
  UConverter *latin1_cnv = get_latin1_cnv();
  
  if (!latin1_cnv) {
    return NULL;
  }

  destlen = ucnv_toUChars(latin1_cnv, NULL, 0, latin1, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf16 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += 1;
  utf16 = mush_calloc(destlen, sizeof(UChar), name);
  uerr = 0;
  destlen = ucnv_toUChars(latin1_cnv, utf16, destlen, latin1, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf16 failed: %s\n",
	      u_errorName(uerr));
    mush_free(utf16, name);
    return NULL;
  }
  if (outlen) {
    *outlen = destlen;
  }
  
  return utf16;
}

/**
 * Convert a utf-16 encoded string to latin-1.
 *
 * \param s the utf-16 string.
 * \param len the length of the string.
 * \param outlen set to the length of the returned string, NOT counting the trailing nul.
 * \param name memcheck tag
 * \return a newly allocated latin-1 string.
 */
char *
utf16_to_latin1(const UChar * restrict utf16, int len, int *outlen, const char * restrict name)
{
  UErrorCode uerr = 0;
  int32_t destlen;
  char *latin1;
  UConverter *latin1_cnv = get_latin1_cnv();
  
  if (!latin1_cnv) {
    return NULL;
  }

  destlen = ucnv_fromUChars(latin1_cnv, NULL, 0, utf16, len, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf16 failed (preflight): %s\n",
	      u_errorName(uerr));
    return NULL;
  }

  destlen += 1;
  latin1 = mush_calloc(destlen, 1, name);
  uerr = 0;
  destlen = ucnv_fromUChars(latin1_cnv, latin1, destlen, utf16, len, &uerr);
  if (U_FAILURE(uerr)) {
    do_rawlog(LT_ERR, "Conversion from latin1 to utf16 failed: %s\n",
	      u_errorName(uerr));
    mush_free(latin1, name);
    return NULL;
  }
  if (outlen) {
    *outlen = destlen;
  }

  return latin1;
}

/** Return a smart lower-cased latin1 string.
 * 
 * \param s the string to lower case
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
latin1_to_lower(const char * restrict s, int len, int *outlen, const char * restrict name)
{
  UChar *utf16 = NULL, *lower16 = NULL;
  char *lower = NULL;
  int ulen, llen;
  UErrorCode uerr = 0;
  
  utf16 = latin1_to_utf16(s, len, &ulen, "temp.utf16");
  
  llen = u_strToLower(NULL, 0, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    mush_free(utf16, "temp.utf16");
    return NULL;
  }

  llen += 1;
  lower16 = mush_calloc(llen, sizeof(UChar), "temp.utf16");

  uerr = 0;
  llen = u_strToLower(lower16, llen, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr)) {
    goto cleanup;
  }

  lower = utf16_to_latin1(lower16, llen, outlen, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(lower16, "temp.utf16");
  return lower;
}

/** Return a smart upper-cased latin1 string.
 * 
 * \param s the string to upper case
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
latin1_to_upper(const char * restrict s, int len, int *outlen, const char * restrict name)
{
  UChar *utf16 = NULL, *upper16 = NULL;
  char *upper = NULL;
  int ulen, llen;
  UErrorCode uerr = 0;
  
  utf16 = latin1_to_utf16(s, len, &ulen, "temp.utf16");
  
  llen = u_strToUpper(NULL, 0, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    mush_free(utf16, "temp.utf16");
    return NULL;
  }

  llen += 1;
  upper16 = mush_calloc(llen, sizeof(UChar), "temp.utf16");

  uerr = 0;
  llen = u_strToUpper(upper16, llen, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr)) {
    goto cleanup;
  }

  upper = utf16_to_latin1(upper16, llen, outlen, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(upper16, "temp.utf16");
  return upper;
}

/** Return a smart lower-cased utf-8 string.
 * 
 * \param s the string to lower case
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
utf8_to_lower(const char * restrict s, int len, int *outlen, const char * restrict name)
{
  UChar *utf16 = NULL, *lower16 = NULL;
  char *lower8 = NULL;
  int ulen, llen;
  UErrorCode uerr = 0;
  
  utf16 = utf8_to_utf16(s, len, &ulen, "temp.utf16");
  
  llen = u_strToLower(NULL, 0, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    mush_free(utf16, "temp.utf16");
    return NULL;
  }

  llen += 1;
  lower16 = mush_calloc(llen, sizeof(UChar), "temp.utf16");

  uerr = 0;
  llen = u_strToLower(lower16, llen, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr)) {
    goto cleanup;
  }

  lower8 = utf16_to_utf8(lower16, llen, outlen, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(lower16, "temp.utf16");
  return lower8;
}

/** Return a smart upper-cased utf-8 string.
 * 
 * \param s the string to upper case
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
utf8_to_upper(const char * restrict s, int len, int *outlen, const char * restrict name)
{
  UChar *utf16 = NULL, *upper16 = NULL;
  char *upper8 = NULL;
  int ulen, llen;
  UErrorCode uerr = 0;
  
  utf16 = utf8_to_utf16(s, len, &ulen, "temp.utf16");
  
  llen = u_strToUpper(NULL, 0, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    mush_free(utf16, "temp.utf16");
    return NULL;
  }

  llen += 1;
  upper16 = mush_calloc(llen, sizeof(UChar), "temp.utf16");

  uerr = 0;
  llen = u_strToUpper(upper16, llen, utf16, ulen, NULL, &uerr);
  if (U_FAILURE(uerr)) {
    goto cleanup;
  }

  upper8 = utf16_to_utf8(upper16, llen, outlen, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(upper16, "temp.utf16");
  return upper8;
}

#endif

#ifdef HAVE_ICU

static const UNormalizer2 *
get_normalizer(enum normalization_type n, UErrorCode *uerr)
{
  switch (n) {
  case NORM_NFC:
    return unorm2_getNFCInstance(uerr);
  case NORM_NFD:
    return unorm2_getNFDInstance(uerr);
  case NORM_NFKC:
    return unorm2_getNFKCInstance(uerr);
  case NORM_NFKD:
    return unorm2_getNFKDInstance(uerr);
  }
}

#endif

/** Normalize a UTF-8 string.
 * If ICU is not present, just returns a fresh copy of its string. 
 * 
 * \param utf8 the string to normalize
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen the length of the returned string.
 * \param name memcheck tag
 * \param type The normalization form.
 * \return newly allocated string.
 */
char *
normalize_utf8(const char * restrict utf8, int len, int *outlen,
               const char * restrict name, enum normalization_type type)
{
#ifdef HAVE_ICU

  UChar *utf16 = NULL, *norm16 = NULL;
  char *norm8 = NULL;
  int ulen, nlen;
  const UNormalizer2 *mode;
  UErrorCode uerr = 0;

  mode = get_normalizer(type, &uerr);
  if (U_FAILURE(uerr)) {
    return NULL;
  }
     
  utf16 = utf8_to_utf16(utf8, len, &ulen, "temp.utf16");
  if (!utf16) {
    return NULL;
  }

  /* Check to see if string is already in normalized form */
  uerr = 0;
  if (unorm2_quickCheck(mode, utf16, ulen, &uerr) == UNORM_YES
      && U_SUCCESS(uerr)) {
    norm8 = utf16_to_utf8(utf16, ulen, outlen, name);
    mush_free(utf16, "temp.utf16");
    return norm8;
  }
    
  uerr = 0;
  nlen = unorm2_normalize(mode, utf16, ulen, NULL, 0, &uerr);
  if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    mush_free(utf16, "temp.utf16");
    return NULL;
  }

  nlen += 1;
  norm16 = mush_calloc(nlen, sizeof(UChar), "temp.utf16");
  uerr = 0;
  nlen = unorm2_normalize(mode, utf16, ulen, norm16, nlen, &uerr);

  if (U_FAILURE(uerr)) {
    goto cleanup;
  }
  
  norm8 = utf16_to_utf8(norm16, nlen, outlen, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(norm16, "temp.utf16");
  return norm8;

#else

  if (outlen) {
    *outlen = len == -1 ? strlen(utf8) : len;
  }
  return mush_strdup(utf8, name);

#endif
}
