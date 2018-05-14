/**
 * \file charconv.c
 *
 * \brief Character set conversion functions.
 */

#include "copyrite.h"

#include <ctype.h>

#ifdef HAVE_ICU
#include <unicode/ustring.h>
#include <unicode/unorm2.h>
#include <unicode/utf8.h>
#include <unicode/utf16.h>
#include <unicode/uchar.h>
#else
#include "myutf8.h"
#include "punicode/utf16.h"
#endif

#include "mysocket.h"
#include "mymalloc.h"
#include "charconv.h"
#include "log.h"
#include "strutil.h"

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
latin1_to_utf8(const char * restrict latin1, int len, int *outlen,
               const char *name)
{
  char *utf8;
  int i, o;

  if (len < 0) {
    len = strlen(latin1);
  }
  /* Worst case, every character takes two bytes */
  utf8 = mush_calloc((len * 2) + 1, 1, name);
  for (i = 0, o = 0; i < len; i += 1) {
    U8_APPEND_UNSAFE(utf8, o, (UChar32)latin1[i]);
  }
  if (outlen) {
    *outlen = o;
  }
  return utf8;
}

/**
 * Convert a latin-1 encoded string to utf-8 optionally handling
 * telnet escape sequences.
 *
 * \param s the latin-1 string.
 * \param latin the length of the string.
 * \param outlen the number of bytes of the returned string, NOT counting the trailing nul.
 * \param telnet true if we should handle telnet escape sequences.
 * \param name memcheck tag.
 * \return a newly allocated utf-8 string.
 */
char *
latin1_to_utf8_tn(const char * restrict latin1, int len, int *outlen,
                  bool telnet, const char *name)
{
  char *utf8;
  int i, o;

  if (len < 0) {
    len = strlen(latin1);
  }
  /* Worst case, every character takes two bytes */
  utf8 = mush_calloc((len * 2) + 1, 1, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c = latin1[i++];
    if (telnet && c == IAC) {
      /* Single IAC is the start of a telnet sequence. Double IAC IAC is
       * an escape for a single character. */
      switch (latin1[i]) {
      case IAC:
        U8_APPEND_UNSAFE(utf8, o, IAC);
        i += 1;
        break;
      case SB:
        utf8[o++] = IAC;
        utf8[o++] = SB;
        i += 1;
        while (latin1[i] != SE) {
          utf8[o++] = latin1[i++];
        }
        utf8[o++] = SE;
        break;
      case DO:
      case DONT:
      case WILL:
      case WONT:
        utf8[o++] = IAC;
        utf8[o++] = latin1[i++];
        utf8[o++] = latin1[i++];
        break;
      case NOP:
        utf8[o++] = IAC;
        utf8[o++] = NOP;
        i += 1;
        break;
      default:
        /* This should never be reached. */
        do_rawlog(LT_ERR, "Invalid telnet sequence character %X", latin1[i]);
        i += 1;
      }
    } else {
      U8_APPEND_UNSAFE(utf8, o, c);
    }
  }
  if (outlen) {
    *outlen = o;
  }
  return utf8;
}


enum translit_action { TRANS_KEEP, TRANS_SKIP, TRANS_REPLACE };

static enum translit_action
translit_to_latin1(UChar32 c, char *rep)
{
  if (c <= 0xFF) {
    *rep = c;
    return TRANS_KEEP;
  }
#ifdef HAVE_ICU
  if (u_charType(c) == U_NON_SPACING_MARK) {
    return TRANS_SKIP;
  }
#endif
  switch (c) {
    /* Single quotes */
  case 0x2018:
  case 0x2019:
  case 0x201A:
  case 0x201B:
  case 0x2039:
  case 0x203A:
    *rep = '\'';
    return TRANS_REPLACE;
    /* Double quotes */
  case 0x201C:
  case 0x201D:
  case 0x201E:
  case 0x201F:
  case 0x2E42:
    *rep = '"';
    return TRANS_REPLACE;
  default:
    *rep = '?';
    return TRANS_REPLACE;
  }
}

/**
 * Convert a UTF-8 encoded string to Latin-1
 *
 * Characters outside the Latin-1 range and invalid byte sequences are
 * turned into question marks.
 *
 * \param utf8 a utf-8 string. It should be normalized in NFC/NFKC for best results.
 * \param len the length of the string in bytes
 * \param outlen set to the length of the returned string NOT including trailing nul
 * \bool translit true to try to transliterate characters to latin-1 equivalents.
 * \param name memcheck tag
 * \return a newly allocated latin-1 string
 */
char *
utf8_to_latin1(const char * restrict utf8, int len, int *outlen, bool translit,
               const char *name)
{
  char *latin1;
  int i, o;

  if (len < 0) {
    len = strlen(utf8);
  }

  latin1 = mush_calloc(len + 1, 1, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c;
    U8_NEXT_OR_FFFD(utf8, i, len, c);
    if (translit) {
      char rep;
      switch (translit_to_latin1(c, &rep)) {
      case TRANS_KEEP:
      case TRANS_REPLACE:
        latin1[o++] = rep;
        break;
      case TRANS_SKIP:
      default:
        break;
      }
    } else if (c <= 0xFF) {
      latin1[o++] = c;
    } else {
      latin1[o++] = '?';
    }
  }
  if (outlen) {
    *outlen = o;
  }
  return latin1;
}

/**
 * Convert a well-formed UTF-8 encoded string to Latin-1
 *
 * Characters outside the Latin-1 range are turned into question marks.
 *
 * \param utf8 a utf-8 string. It should be normalized in NFC/NFKC for best results.
 * \param len the length of the string in bytes
 * \param outlen set to the length of the returned string NOT including trailing nul
 * \bool translit true to try to transliterate characters to latin-1 equivalents.
 * \param name memcheck tag
 * \return a newly allocated latin-1 string
 */
char *
utf8_to_latin1_us(const char * restrict utf8, int len, int *outlen,
                  bool translit, const char *name)
{
  char *latin1;
  int i, o;

  if (len < 0) {
    len = strlen(utf8);
  }

  latin1 = mush_calloc(len + 1, 1, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c;
    U8_NEXT_UNSAFE(utf8, i, c);
    if (translit) {
      char rep;
      switch (translit_to_latin1(c, &rep)) {
      case TRANS_KEEP:
      case TRANS_REPLACE:
        latin1[o++] = c;
        break;
      case TRANS_SKIP:
      default:
        break;
      }
    } else if (c <= 0xFF) {
      latin1[o++] = c;
    } else {
      latin1[o++] = '?';
    }
  }
  if (outlen) {
    *outlen = o;
  }
  return latin1;
}

/**
 * Check to see if a string is valid utf-8 or not.
 * \param utf8 string to validate
 * \return true or false
 */
bool
valid_utf8(const char *utf8)
{
  int i = 0;
  while (1) {
    UChar32 c;
    U8_NEXT(utf8, i, -1, c);
    if (c < 0) {
      return 0;
    }
    if (utf8[i] == '\0') {
      return 1;
    }
  }
  return 0;
}

/** Convert a well-formed UTF-16 encoded string to UTF-8.
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
  uint8_t *utf8 = NULL;
  int i, o;

  if (len < 0) {
    const UChar *c = utf16;
    len = 0;
    while (*c++) {
      len += 1;
    }
  }

  /* Every 1 UChar CP becomes 1-3 bytes, every 2 UChar CP becomes 4. */
  utf8 = mush_calloc(len * 3 + 1, 1, name);
  for (i = 0, o = 0; i < len;) {
    UChar32 c;
    U16_NEXT_UNSAFE(utf16, i, c);
    U8_APPEND_UNSAFE(utf8, o, c);
  }
  if (outlen) {
    *outlen = o;
  }
  return (char *)utf8;
}

/** Convert a UTF-8 encoded string to UTF-16.
 *
 * Invalid byte sequences are turned into U+FFFD.
 *
 * \parm utf8 the UTF-8 string
 * \param len the length of the string in bytes. -1 to use 0-terminated length
 * \param outlen pointer to store the length of the UTF-16 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-16 string
 */
UChar *
utf8_to_utf16(const char * restrict utf8, int len, int *outlen,
              const char *name)
{
  UChar *utf16;
  int i, o;

  if (len < 0) {
    len = strlen(utf8);
  }
  /* A UTF-16 array will always have the same or fewer elements than a
     UTF-8 array. 1-3 byte sequences become 1 UChar, 4 byte sequences
     become 2. */
  utf16 = mush_calloc(len + 1, sizeof(UChar), name);
  for (i = 0, o = 0; i < len;) {
    UChar32 c;
    U8_NEXT_OR_FFFD(utf8, i, len, c);
    U16_APPEND_UNSAFE(utf16, o, c);
  }
  if (outlen) {
    *outlen = o;
  }
  return utf16;
}

/** Convert a well-formed UTF-8 encoded string to UTF-16.
 *
 * \parm utf8 the UTF-8 string
 * \param len the length of the string in bytes. -1 to use 0-terminated length
 * \param outlen pointer to store the length of the UTF-16 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-16 string
 */
UChar *
utf8_to_utf16_us(const char * restrict utf8, int len, int *outlen,
                 const char *name)
{
  UChar *utf16;
  int i, o;

  if (len < 0) {
    len = strlen(utf8);
  }
  /* A UTF-16 array will always have the same or fewer elements than a
     UTF-8 array. 1-3 byte sequences become 1 UChar, 4 byte sequences
     become 2. */
  utf16 = mush_calloc(len + 1, sizeof(UChar), name);
  for (i = 0, o = 0; i < len;) {
    UChar32 c;
    U8_NEXT_UNSAFE(utf8, i, c);
    U16_APPEND_UNSAFE(utf16, o, c);
  }
  if (outlen) {
    *outlen = o;
  }
  return utf16;
}

/** Convert a UTF-8 encoded string to UTF-32
 *
 * Invalid byte sequences are turned into U+FFFD.
 *
 * \parm utf8 the UTF-8 string
 * \param len the length of the string in bytes.
 * \param outlen pointer to store the length of the UTF-32 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-32 string
 */
UChar32 *
utf8_to_utf32(const char * restrict utf8, int len, int *outlen,
              const char *name)
{
  UChar32 *utf32;
  int i, o;

  if (len < 0) {
    len = strlen(utf8);
  }

  utf32 = mush_calloc(len + 1, sizeof(UChar32), name);
  for (i = 0, o = 0; i < len; o += 1) {
    UChar32 c;
    U8_NEXT_OR_FFFD(utf8, i, len, c);
    utf32[o] = c;
  }
  if (outlen) {
    *outlen = o;
  }
  return utf32;
}

/** Convert a well-formed UTF-8 encoded string to UTF-32
 *
 * \parm utf8 the UTF-8 string
 * \param len the length of the string in bytes.
 * \param outlen pointer to store the length of the UTF-32 string
 * \param name memcheck tag for new string
 * \return newly allocated UTF-32 string
 */
UChar32 *
utf8_to_utf32_us(const char * restrict utf8, int len, int *outlen,
                 const char *name)
{
  UChar32 *utf32;
  int i, o;

  if (len < 0) {
    len = strlen(utf8);
  }

  utf32 = mush_calloc(len + 1, sizeof(UChar32), name);
  for (i = 0, o = 0; i < len; o += 1) {
    UChar32 c;
    U8_NEXT_UNSAFE(utf8, i, c);
    utf32[o] = c;
  }
  if (outlen) {
    *outlen = o;
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
  char *utf8;
  int i, o;

  if (len < 0) {
    const UChar32 *c = utf32;
    len = 0;
    while (*c++) {
      len += 1;
    }
  }

  /* Worst case, every character take 4 bytes in UTF-8 */
  utf8 = mush_calloc((len * 4) + 1, 1, name);
  for (i = 0, o = 0; i < len; i += 1) {
    U8_APPEND_UNSAFE(utf8, o, utf32[i]);
  }
  if (outlen) {
    *outlen = o;
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
latin1_to_utf32(const char * restrict latin1, int len, int *outlen,
                const char *name)
{
  UChar32 *utf32;
  int i;

  if (len < 0) {
    len = strlen(latin1);
  }

  utf32 = mush_calloc(len + 1, sizeof(UChar32), name);
  for (i = 0; i < len; i += 1) {
    utf32[i] = (UChar32)latin1[i];
  }
  if (outlen) {
    *outlen = i;
  }
  return utf32;
}

/** Convert a UTF-32 encoded string to latin-1
 *
 * \parm utf32 the UTF-32 string. Should be normalized in NFC or NFKC for best results.
 * \param len the length of the string.
 * \param outlen pointer to store the length of the latin-1 string
 * \bool translit true to try to transliterate characters to latin-1 equivalents.
 * \param name memcheck tag for new string
 * \return newly allocated latin-1 string
 */
char *
utf32_to_latin1(const UChar32 *utf32, int len, int *outlen, bool translit,
                const char * name)
{
  char *latin1;
  int i, o;

  if (len < 0) {
    const UChar32 *c = utf32;
    len = 0;
    while (*c++) {
      len += 1;
    }
  }
  latin1 = mush_calloc(len + 1, 1, name);
  for (i = 0, o = 0; i < len; i += 1) {
    UChar32 c = utf32[i];
    if (translit) {
      char rep;
      switch (translit_to_latin1(c, &rep)) {
      case TRANS_KEEP:
      case TRANS_REPLACE:
        latin1[o++] = rep;
        break;
      case TRANS_SKIP:
      default:
        break;
      }
    } else if (c <= 0xFF) {
      latin1[o++] = c;
    } else {
      latin1[o++] = '?';
    }
  }
  if (outlen) {
    *outlen = o;
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
latin1_to_utf16(const char * restrict latin1, int len, int *outlen,
                const char *name)
{
  UChar *utf16;
  int i;

  if (len < 0) {
    len = strlen(latin1);
  }

  utf16 = mush_calloc(len + 1, sizeof(UChar), name);
  for (i = 0; i < len; i += 1) {
    utf16[i] = (UChar)latin1[i];
  }
  if (outlen) {
    *outlen = i;
  }
  return utf16;
}

/**
 * Convert a well-formed utf-16 encoded string to latin-1.
 *
 * \param s the utf-16 string. Should be normalized in NFC or NFKC for best results.
 * \param len the length of the string.
 * \param outlen set to the length of the returned string, NOT counting the trailing nul.
 * \bool translit true to try to transliterate characters to latin-1 equivalents.
 * \param name memcheck tag
 * \return a newly allocated latin-1 string.
 */
char *
utf16_to_latin1(const UChar * utf16, int len, int *outlen, bool translit,
                const char * name)
{
  char *latin1;
  int i, o;

  if (len < 0) {
    const UChar *c = utf16;
    len = 0;
    while (*c++) {
      len += 1;
    }
  }
  latin1 = mush_calloc(len + 1, 1, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c;
    U16_NEXT_UNSAFE(utf16, i, c);
    if (translit) {
      char rep;
      switch (translit_to_latin1(c, &rep)) {
      case TRANS_KEEP:
      case TRANS_REPLACE:
        latin1[o++] = rep;
        break;
      case TRANS_SKIP:
      default:
        break;
      }
    } else if (c <= 0xFF) {
      latin1[o++] = c;
    } else {
      latin1[o++] = '?';
    }
  }
  if (outlen) {
    *outlen = o;
  }
  return latin1;
}

#ifdef HAVE_ICU

/** Return a smart lower-cased latin1 string.
 * 
 * \param s the string to lower case
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
latin1_to_lower(const char * restrict s, int len, int *outlen,
                const char *name)
{
  UChar *utf16 = NULL, *lower16 = NULL;
  char *lower = NULL;
  int ulen, llen;
  UErrorCode uerr = U_ZERO_ERROR;
  
  utf16 = latin1_to_utf16(s, len, &ulen, "temp.utf16");
  lower16 = mush_calloc(ulen + 1, sizeof(UChar), "temp.utf16");
  llen = ulen + 1;

  llen = u_strToLower(lower16, llen, utf16, ulen, NULL, &uerr);
  if (U_SUCCESS(uerr)) {
  } else if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    goto cleanup;
  } else {
    llen += 1;
    lower16 = mush_realloc(lower16, llen * sizeof(UChar), "temp.utf16");
    uerr = U_ZERO_ERROR;
    llen = u_strToLower(lower16, llen, utf16, ulen, NULL, &uerr);
    if (U_FAILURE(uerr)) {
      goto cleanup;
    }
  }

  lower = utf16_to_latin1(lower16, llen, outlen, 0, name);

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
latin1_to_upper(const char * restrict s, int len, int *outlen,
                const char *name)
{
  UChar *utf16 = NULL, *upper16 = NULL;
  char *upper = NULL;
  int ulen, llen;
  UErrorCode uerr = U_ZERO_ERROR;
  
  utf16 = latin1_to_utf16(s, len, &ulen, "temp.utf16");
  upper16 = mush_calloc(ulen + 1, sizeof(UChar), "temp.utf16");
  llen = ulen + 1;
  
  llen = u_strToUpper(upper16, llen, utf16, ulen, NULL, &uerr);
  if (U_SUCCESS(uerr)) {
  } else if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    goto cleanup;
  } else {
    llen += 1;
    upper16 = mush_realloc(upper16, llen * sizeof(UChar), "temp.utf16");

    uerr = U_ZERO_ERROR;
    llen = u_strToUpper(upper16, llen, utf16, ulen, NULL, &uerr);
    if (U_FAILURE(uerr)) {
      goto cleanup;
    }
  }

  upper = utf16_to_latin1(upper16, llen, outlen, 0, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(upper16, "temp.utf16");
  return upper;
}

/** Return a smart lower-cased utf-8 string.
 * 
 * Invalid byte sequences are replaced with U+FFFD
 *
 * \param s the string to lower case
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
utf8_to_lower(const char * restrict s, int len, int *outlen,
              const char *name)
{
  UChar *utf16 = NULL, *lower16 = NULL;
  char *lower8 = NULL;
  int ulen, llen;
  UErrorCode uerr = U_ZERO_ERROR;
  
  utf16 = utf8_to_utf16(s, len, &ulen, "temp.utf16");

  lower16 = mush_calloc(ulen + 1, sizeof(UChar), "temp.utf16");
  llen = ulen + 1;
  
  llen = u_strToLower(lower16, llen, utf16, ulen, NULL, &uerr);
  if (U_SUCCESS(uerr)) {
  } else if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    goto cleanup;
  } else {
    llen += 1;
    lower16 = mush_realloc(lower16, llen * sizeof(UChar), "temp.utf16");
    uerr = U_ZERO_ERROR;
    llen = u_strToLower(lower16, llen, utf16, ulen, NULL, &uerr);
    if (U_FAILURE(uerr)) {
      goto cleanup;
    }
  }

  lower8 = utf16_to_utf8(lower16, llen, outlen, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(lower16, "temp.utf16");
  return lower8;
}

/** Return a smart upper-cased utf-8 string.
 * 
 * Invalid byte sequences are replaced with U+FFFD
 *
 * \param s the string to upper case
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
utf8_to_upper(const char * restrict s, int len, int *outlen,
              const char *name)
{
  UChar *utf16 = NULL, *upper16 = NULL;
  char *upper8 = NULL;
  int ulen, llen;
  UErrorCode uerr = U_ZERO_ERROR;
  
  utf16 = utf8_to_utf16(s, len, &ulen, "temp.utf16");
  upper16 = mush_calloc(ulen + 1, sizeof(UChar), "temp.utf16");
  llen = ulen + 1;
  
  llen = u_strToUpper(upper16, llen, utf16, ulen, NULL, &uerr);
  if (U_SUCCESS(uerr)) {
  } if (U_FAILURE(uerr) && uerr != U_BUFFER_OVERFLOW_ERROR) {
    goto cleanup;
  } else {
    llen += 1;
    upper16 = mush_realloc(upper16, llen * sizeof(UChar), "temp.utf16");
    uerr = U_ZERO_ERROR;
    llen = u_strToUpper(upper16, llen, utf16, ulen, NULL, &uerr);
    if (U_FAILURE(uerr)) {
      goto cleanup;
    }
  }

  upper8 = utf16_to_utf8(upper16, llen, outlen, name);

 cleanup:
  mush_free(utf16, "temp.utf16");
  mush_free(upper16, "temp.utf16");
  return upper8;
}

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
  default:
    return NULL;
  }
}

/** Normalize a well-formed UTF-16 string.
 *
 * \param type The normalization form.
 * \param utf16 the string to normalize
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
UChar *
normalize_utf16(enum normalization_type type, const UChar *utf16,
                int len, int *outlen, const char *name)
{
  UChar *norm16 = NULL;
  int nlen;
  UErrorCode uerr = U_ZERO_ERROR;
  const UNormalizer2 *mode;
  
  mode = get_normalizer(type, &uerr);
  if (U_FAILURE(uerr)) {
    return NULL;
  }

  if (len < 0) {
    const UChar *c = utf16;
    len = 0;
    while (*c++) {
      len += 1;
    }
  }

  /* Check to see if string is already in normalized form */
  uerr = U_ZERO_ERROR;
  if (unorm2_quickCheck(mode, utf16, len, &uerr) == UNORM_YES
      && U_SUCCESS(uerr)) {
    norm16 = mush_calloc(len + 1, sizeof(UChar), name);
    memcpy(norm16, utf16, len * sizeof(UChar));
    if (outlen) {
      *outlen = len;
    }
    return norm16;
  }

  norm16 = mush_calloc(len + 1, sizeof(UChar), name);
  nlen = len + 1;

  uerr = U_ZERO_ERROR;
  nlen = unorm2_normalize(mode, utf16, len, norm16, nlen, &uerr);
  if (U_FAILURE(uerr)) {
    if (uerr != U_BUFFER_OVERFLOW_ERROR) {
      mush_free(norm16, name);
      return NULL;
    }
    nlen += 1;
    norm16 = mush_realloc(norm16, nlen * sizeof(UChar), name);
    uerr = U_ZERO_ERROR;
    nlen = unorm2_normalize(mode, utf16, len, norm16, nlen, &uerr);
    if (U_FAILURE(uerr)) {
      mush_free(norm16, name);
      return NULL;
    }
  }
  if (outlen) {
    *outlen = nlen;
  }
  return norm16;
}

/** Normalize a UTF-8 string and convert to latin-1.
 *
 * Invalid byte sequences get turned into question marks.
 * Tries to downgrade Unicode characters to Latin-1 characters.
 *
 * \param utf8 the string to normalize
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string in latin-1.
 */
char *
translate_utf8_to_latin1(const char * restrict utf8, int len, int *outlen,
                         const char *name)
{
  UChar *utf16, *norm16;
  char *norm8;
  int ulen, nlen;

  utf16 = utf8_to_utf16(utf8, len, &ulen, "temp.utf16");
  if (!utf16) {
    return NULL;
  }

  norm16 = normalize_utf16(NORM_NFKC, utf16, ulen, &nlen, "temp.utf16");

  if (!norm16) {
    mush_free(utf16, "temp.utf16");
    return NULL;
  }

  mush_free(utf16, "temp.utf16");

  norm8 = utf16_to_latin1(norm16, nlen, outlen, 1, name);

  mush_free(norm16, "temp.utf16");
  
  return norm8;
}

/** Normalize a UTF-8 string.
 * 
 * Invalid byte sequences get turned into U+FFFD.
 *
 * \param type The normalization form.
 * \param utf8 the string to normalize
 * \param len the length of the string or -1 for 0-terminated length.
 * \param outlen the length of the returned string.
 * \param name memcheck tag
 * \return newly allocated string.
 */
char *
normalize_utf8(enum normalization_type type, const char * restrict utf8,
               int len, int *outlen, const char *name)
{
  UChar *utf16, *norm16;
  int ulen, nlen;
  char *norm8;

  utf16 = utf8_to_utf16(utf8, len, &ulen, "temp.utf16");
  if (!utf16) {
    return NULL;
  }
  
  norm16 = normalize_utf16(type, utf16, ulen, &nlen, "temp.utf16");
  if (!norm16) {
    mush_free(utf16, "temp.utf16");
    return NULL;
  }

  norm8 = utf16_to_utf8(norm16, nlen, outlen, name);

  mush_free(utf16, "temp.utf16");
  mush_free(norm16, "temp.utf16");
  
  return norm8;
}
#endif

/** Sanitize a UTF-8 string.
 *
 * Returns a newly allocated string with invalid byte sequences in the
 * orignal replaced by U+FFFD.
 *
 * \param orig the orignal utf-8 string.
 * \parma len the length of the string or -1 for 0-terminated length.
 * \param outlen set to the length of the returned string not counting the trailing 0.
 * \param name memcheck tag
 * \return new string.
 */
char *
sanitize_utf8(const char * restrict orig, int len, int *outlen,
              const char *name)
{
  char *san8;
  int32_t i, o;

  if (len == -1) {
    len = strlen(orig);
  }
  /* Allocate enough space for the worst case: every byte in orig is
     invalid. */
  san8 = mush_calloc(len + 1, 3, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c;
    U8_NEXT_OR_FFFD(orig, i, len, c);
    U8_APPEND_UNSAFE(san8, o, c);
  }
  if (outlen) {
    *outlen = o;
  }
  return san8;
}
