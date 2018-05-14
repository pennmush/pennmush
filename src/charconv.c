/**
 * \file charconv.c
 *
 * \brief Character set conversion functions.
 */

#include "copyrite.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

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

/* Based on the Unicode->ASCII table in spellfix.c */
typedef struct Transliteration Transliteration;
struct Transliteration {
 UChar32 cFrom;
 char cTo0, cTo1, cTo2, cTo3;
};

/*
** Table of translations from unicode characters into ASCII.
*/
static const Transliteration translit[] = {
  { 0x0100,  0x41, 0x00, 0x00, 0x00 },  /* Ā to A */
  { 0x0101,  0x61, 0x00, 0x00, 0x00 },  /* ā to a */
  { 0x0102,  0x41, 0x00, 0x00, 0x00 },  /* Ă to A */
  { 0x0103,  0x61, 0x00, 0x00, 0x00 },  /* ă to a */
  { 0x0104,  0x41, 0x00, 0x00, 0x00 },  /* Ą to A */
  { 0x0105,  0x61, 0x00, 0x00, 0x00 },  /* ą to a */
  { 0x0106,  0x43, 0x00, 0x00, 0x00 },  /* Ć to C */
  { 0x0107,  0x63, 0x00, 0x00, 0x00 },  /* ć to c */
  { 0x0108,  0x43, 0x68, 0x00, 0x00 },  /* Ĉ to Ch */
  { 0x0109,  0x63, 0x68, 0x00, 0x00 },  /* ĉ to ch */
  { 0x010A,  0x43, 0x00, 0x00, 0x00 },  /* Ċ to C */
  { 0x010B,  0x63, 0x00, 0x00, 0x00 },  /* ċ to c */
  { 0x010C,  0x43, 0x00, 0x00, 0x00 },  /* Č to C */
  { 0x010D,  0x63, 0x00, 0x00, 0x00 },  /* č to c */
  { 0x010E,  0x44, 0x00, 0x00, 0x00 },  /* Ď to D */
  { 0x010F,  0x64, 0x00, 0x00, 0x00 },  /* ď to d */
  { 0x0110,  0x44, 0x00, 0x00, 0x00 },  /* Đ to D */
  { 0x0111,  0x64, 0x00, 0x00, 0x00 },  /* đ to d */
  { 0x0112,  0x45, 0x00, 0x00, 0x00 },  /* Ē to E */
  { 0x0113,  0x65, 0x00, 0x00, 0x00 },  /* ē to e */
  { 0x0114,  0x45, 0x00, 0x00, 0x00 },  /* Ĕ to E */
  { 0x0115,  0x65, 0x00, 0x00, 0x00 },  /* ĕ to e */
  { 0x0116,  0x45, 0x00, 0x00, 0x00 },  /* Ė to E */
  { 0x0117,  0x65, 0x00, 0x00, 0x00 },  /* ė to e */
  { 0x0118,  0x45, 0x00, 0x00, 0x00 },  /* Ę to E */
  { 0x0119,  0x65, 0x00, 0x00, 0x00 },  /* ę to e */
  { 0x011A,  0x45, 0x00, 0x00, 0x00 },  /* Ě to E */
  { 0x011B,  0x65, 0x00, 0x00, 0x00 },  /* ě to e */
  { 0x011C,  0x47, 0x68, 0x00, 0x00 },  /* Ĝ to Gh */
  { 0x011D,  0x67, 0x68, 0x00, 0x00 },  /* ĝ to gh */
  { 0x011E,  0x47, 0x00, 0x00, 0x00 },  /* Ğ to G */
  { 0x011F,  0x67, 0x00, 0x00, 0x00 },  /* ğ to g */
  { 0x0120,  0x47, 0x00, 0x00, 0x00 },  /* Ġ to G */
  { 0x0121,  0x67, 0x00, 0x00, 0x00 },  /* ġ to g */
  { 0x0122,  0x47, 0x00, 0x00, 0x00 },  /* Ģ to G */
  { 0x0123,  0x67, 0x00, 0x00, 0x00 },  /* ģ to g */
  { 0x0124,  0x48, 0x68, 0x00, 0x00 },  /* Ĥ to Hh */
  { 0x0125,  0x68, 0x68, 0x00, 0x00 },  /* ĥ to hh */
  { 0x0126,  0x48, 0x00, 0x00, 0x00 },  /* Ħ to H */
  { 0x0127,  0x68, 0x00, 0x00, 0x00 },  /* ħ to h */
  { 0x0128,  0x49, 0x00, 0x00, 0x00 },  /* Ĩ to I */
  { 0x0129,  0x69, 0x00, 0x00, 0x00 },  /* ĩ to i */
  { 0x012A,  0x49, 0x00, 0x00, 0x00 },  /* Ī to I */
  { 0x012B,  0x69, 0x00, 0x00, 0x00 },  /* ī to i */
  { 0x012C,  0x49, 0x00, 0x00, 0x00 },  /* Ĭ to I */
  { 0x012D,  0x69, 0x00, 0x00, 0x00 },  /* ĭ to i */
  { 0x012E,  0x49, 0x00, 0x00, 0x00 },  /* Į to I */
  { 0x012F,  0x69, 0x00, 0x00, 0x00 },  /* į to i */
  { 0x0130,  0x49, 0x00, 0x00, 0x00 },  /* İ to I */
  { 0x0131,  0x69, 0x00, 0x00, 0x00 },  /* ı to i */
  { 0x0132,  0x49, 0x4A, 0x00, 0x00 },  /* Ĳ to IJ */
  { 0x0133,  0x69, 0x6A, 0x00, 0x00 },  /* ĳ to ij */
  { 0x0134,  0x4A, 0x68, 0x00, 0x00 },  /* Ĵ to Jh */
  { 0x0135,  0x6A, 0x68, 0x00, 0x00 },  /* ĵ to jh */
  { 0x0136,  0x4B, 0x00, 0x00, 0x00 },  /* Ķ to K */
  { 0x0137,  0x6B, 0x00, 0x00, 0x00 },  /* ķ to k */
  { 0x0138,  0x6B, 0x00, 0x00, 0x00 },  /* ĸ to k */
  { 0x0139,  0x4C, 0x00, 0x00, 0x00 },  /* Ĺ to L */
  { 0x013A,  0x6C, 0x00, 0x00, 0x00 },  /* ĺ to l */
  { 0x013B,  0x4C, 0x00, 0x00, 0x00 },  /* Ļ to L */
  { 0x013C,  0x6C, 0x00, 0x00, 0x00 },  /* ļ to l */
  { 0x013D,  0x4C, 0x00, 0x00, 0x00 },  /* Ľ to L */
  { 0x013E,  0x6C, 0x00, 0x00, 0x00 },  /* ľ to l */
  { 0x013F,  0x4C, 0x2E, 0x00, 0x00 },  /* Ŀ to L. */
  { 0x0140,  0x6C, 0x2E, 0x00, 0x00 },  /* ŀ to l. */
  { 0x0141,  0x4C, 0x00, 0x00, 0x00 },  /* Ł to L */
  { 0x0142,  0x6C, 0x00, 0x00, 0x00 },  /* ł to l */
  { 0x0143,  0x4E, 0x00, 0x00, 0x00 },  /* Ń to N */
  { 0x0144,  0x6E, 0x00, 0x00, 0x00 },  /* ń to n */
  { 0x0145,  0x4E, 0x00, 0x00, 0x00 },  /* Ņ to N */
  { 0x0146,  0x6E, 0x00, 0x00, 0x00 },  /* ņ to n */
  { 0x0147,  0x4E, 0x00, 0x00, 0x00 },  /* Ň to N */
  { 0x0148,  0x6E, 0x00, 0x00, 0x00 },  /* ň to n */
  { 0x0149,  0x27, 0x6E, 0x00, 0x00 },  /* ŉ to 'n */
  { 0x014A,  0x4E, 0x47, 0x00, 0x00 },  /* Ŋ to NG */
  { 0x014B,  0x6E, 0x67, 0x00, 0x00 },  /* ŋ to ng */
  { 0x014C,  0x4F, 0x00, 0x00, 0x00 },  /* Ō to O */
  { 0x014D,  0x6F, 0x00, 0x00, 0x00 },  /* ō to o */
  { 0x014E,  0x4F, 0x00, 0x00, 0x00 },  /* Ŏ to O */
  { 0x014F,  0x6F, 0x00, 0x00, 0x00 },  /* ŏ to o */
  { 0x0150,  0x4F, 0x00, 0x00, 0x00 },  /* Ő to O */
  { 0x0151,  0x6F, 0x00, 0x00, 0x00 },  /* ő to o */
  { 0x0152,  0x4F, 0x45, 0x00, 0x00 },  /* Œ to OE */
  { 0x0153,  0x6F, 0x65, 0x00, 0x00 },  /* œ to oe */
  { 0x0154,  0x52, 0x00, 0x00, 0x00 },  /* Ŕ to R */
  { 0x0155,  0x72, 0x00, 0x00, 0x00 },  /* ŕ to r */
  { 0x0156,  0x52, 0x00, 0x00, 0x00 },  /* Ŗ to R */
  { 0x0157,  0x72, 0x00, 0x00, 0x00 },  /* ŗ to r */
  { 0x0158,  0x52, 0x00, 0x00, 0x00 },  /* Ř to R */
  { 0x0159,  0x72, 0x00, 0x00, 0x00 },  /* ř to r */
  { 0x015A,  0x53, 0x00, 0x00, 0x00 },  /* Ś to S */
  { 0x015B,  0x73, 0x00, 0x00, 0x00 },  /* ś to s */
  { 0x015C,  0x53, 0x68, 0x00, 0x00 },  /* Ŝ to Sh */
  { 0x015D,  0x73, 0x68, 0x00, 0x00 },  /* ŝ to sh */
  { 0x015E,  0x53, 0x00, 0x00, 0x00 },  /* Ş to S */
  { 0x015F,  0x73, 0x00, 0x00, 0x00 },  /* ş to s */
  { 0x0160,  0x53, 0x00, 0x00, 0x00 },  /* Š to S */
  { 0x0161,  0x73, 0x00, 0x00, 0x00 },  /* š to s */
  { 0x0162,  0x54, 0x00, 0x00, 0x00 },  /* Ţ to T */
  { 0x0163,  0x74, 0x00, 0x00, 0x00 },  /* ţ to t */
  { 0x0164,  0x54, 0x00, 0x00, 0x00 },  /* Ť to T */
  { 0x0165,  0x74, 0x00, 0x00, 0x00 },  /* ť to t */
  { 0x0166,  0x54, 0x00, 0x00, 0x00 },  /* Ŧ to T */
  { 0x0167,  0x74, 0x00, 0x00, 0x00 },  /* ŧ to t */
  { 0x0168,  0x55, 0x00, 0x00, 0x00 },  /* Ũ to U */
  { 0x0169,  0x75, 0x00, 0x00, 0x00 },  /* ũ to u */
  { 0x016A,  0x55, 0x00, 0x00, 0x00 },  /* Ū to U */
  { 0x016B,  0x75, 0x00, 0x00, 0x00 },  /* ū to u */
  { 0x016C,  0x55, 0x00, 0x00, 0x00 },  /* Ŭ to U */
  { 0x016D,  0x75, 0x00, 0x00, 0x00 },  /* ŭ to u */
  { 0x016E,  0x55, 0x00, 0x00, 0x00 },  /* Ů to U */
  { 0x016F,  0x75, 0x00, 0x00, 0x00 },  /* ů to u */
  { 0x0170,  0x55, 0x00, 0x00, 0x00 },  /* Ű to U */
  { 0x0171,  0x75, 0x00, 0x00, 0x00 },  /* ű to u */
  { 0x0172,  0x55, 0x00, 0x00, 0x00 },  /* Ų to U */
  { 0x0173,  0x75, 0x00, 0x00, 0x00 },  /* ų to u */
  { 0x0174,  0x57, 0x00, 0x00, 0x00 },  /* Ŵ to W */
  { 0x0175,  0x77, 0x00, 0x00, 0x00 },  /* ŵ to w */
  { 0x0176,  0x59, 0x00, 0x00, 0x00 },  /* Ŷ to Y */
  { 0x0177,  0x79, 0x00, 0x00, 0x00 },  /* ŷ to y */
  { 0x0178,  0x59, 0x00, 0x00, 0x00 },  /* Ÿ to Y */
  { 0x0179,  0x5A, 0x00, 0x00, 0x00 },  /* Ź to Z */
  { 0x017A,  0x7A, 0x00, 0x00, 0x00 },  /* ź to z */
  { 0x017B,  0x5A, 0x00, 0x00, 0x00 },  /* Ż to Z */
  { 0x017C,  0x7A, 0x00, 0x00, 0x00 },  /* ż to z */
  { 0x017D,  0x5A, 0x00, 0x00, 0x00 },  /* Ž to Z */
  { 0x017E,  0x7A, 0x00, 0x00, 0x00 },  /* ž to z */
  { 0x017F,  0x73, 0x00, 0x00, 0x00 },  /* ſ to s */
  { 0x0192,  0x66, 0x00, 0x00, 0x00 },  /* ƒ to f */
  { 0x0218,  0x53, 0x00, 0x00, 0x00 },  /* Ș to S */
  { 0x0219,  0x73, 0x00, 0x00, 0x00 },  /* ș to s */
  { 0x021A,  0x54, 0x00, 0x00, 0x00 },  /* Ț to T */
  { 0x021B,  0x74, 0x00, 0x00, 0x00 },  /* ț to t */
  { 0x0386,  0x41, 0x00, 0x00, 0x00 },  /* Ά to A */
  { 0x0388,  0x45, 0x00, 0x00, 0x00 },  /* Έ to E */
  { 0x0389,  0x49, 0x00, 0x00, 0x00 },  /* Ή to I */
  { 0x038A,  0x49, 0x00, 0x00, 0x00 },  /* Ί to I */
  { 0x038C,  0x4f, 0x00, 0x00, 0x00 },  /* Ό to O */
  { 0x038E,  0x59, 0x00, 0x00, 0x00 },  /* Ύ to Y */
  { 0x038F,  0x4f, 0x00, 0x00, 0x00 },  /* Ώ to O */
  { 0x0390,  0x69, 0x00, 0x00, 0x00 },  /* ΐ to i */
  { 0x0391,  0x41, 0x00, 0x00, 0x00 },  /* Α to A */
  { 0x0392,  0x42, 0x00, 0x00, 0x00 },  /* Β to B */
  { 0x0393,  0x47, 0x00, 0x00, 0x00 },  /* Γ to G */
  { 0x0394,  0x44, 0x00, 0x00, 0x00 },  /* Δ to D */
  { 0x0395,  0x45, 0x00, 0x00, 0x00 },  /* Ε to E */
  { 0x0396,  0x5a, 0x00, 0x00, 0x00 },  /* Ζ to Z */
  { 0x0397,  0x49, 0x00, 0x00, 0x00 },  /* Η to I */
  { 0x0398,  0x54, 0x68, 0x00, 0x00 },  /* Θ to Th */
  { 0x0399,  0x49, 0x00, 0x00, 0x00 },  /* Ι to I */
  { 0x039A,  0x4b, 0x00, 0x00, 0x00 },  /* Κ to K */
  { 0x039B,  0x4c, 0x00, 0x00, 0x00 },  /* Λ to L */
  { 0x039C,  0x4d, 0x00, 0x00, 0x00 },  /* Μ to M */
  { 0x039D,  0x4e, 0x00, 0x00, 0x00 },  /* Ν to N */
  { 0x039E,  0x58, 0x00, 0x00, 0x00 },  /* Ξ to X */
  { 0x039F,  0x4f, 0x00, 0x00, 0x00 },  /* Ο to O */
  { 0x03A0,  0x50, 0x00, 0x00, 0x00 },  /* Π to P */
  { 0x03A1,  0x52, 0x00, 0x00, 0x00 },  /* Ρ to R */
  { 0x03A3,  0x53, 0x00, 0x00, 0x00 },  /* Σ to S */
  { 0x03A4,  0x54, 0x00, 0x00, 0x00 },  /* Τ to T */
  { 0x03A5,  0x59, 0x00, 0x00, 0x00 },  /* Υ to Y */
  { 0x03A6,  0x46, 0x00, 0x00, 0x00 },  /* Φ to F */
  { 0x03A7,  0x43, 0x68, 0x00, 0x00 },  /* Χ to Ch */
  { 0x03A8,  0x50, 0x73, 0x00, 0x00 },  /* Ψ to Ps */
  { 0x03A9,  0x4f, 0x00, 0x00, 0x00 },  /* Ω to O */
  { 0x03AA,  0x49, 0x00, 0x00, 0x00 },  /* Ϊ to I */
  { 0x03AB,  0x59, 0x00, 0x00, 0x00 },  /* Ϋ to Y */
  { 0x03AC,  0x61, 0x00, 0x00, 0x00 },  /* ά to a */
  { 0x03AD,  0x65, 0x00, 0x00, 0x00 },  /* έ to e */
  { 0x03AE,  0x69, 0x00, 0x00, 0x00 },  /* ή to i */
  { 0x03AF,  0x69, 0x00, 0x00, 0x00 },  /* ί to i */
  { 0x03B1,  0x61, 0x00, 0x00, 0x00 },  /* α to a */
  { 0x03B2,  0x62, 0x00, 0x00, 0x00 },  /* β to b */
  { 0x03B3,  0x67, 0x00, 0x00, 0x00 },  /* γ to g */
  { 0x03B4,  0x64, 0x00, 0x00, 0x00 },  /* δ to d */
  { 0x03B5,  0x65, 0x00, 0x00, 0x00 },  /* ε to e */
  { 0x03B6,  0x7a, 0x00, 0x00, 0x00 },  /* ζ to z */
  { 0x03B7,  0x69, 0x00, 0x00, 0x00 },  /* η to i */
  { 0x03B8,  0x74, 0x68, 0x00, 0x00 },  /* θ to th */
  { 0x03B9,  0x69, 0x00, 0x00, 0x00 },  /* ι to i */
  { 0x03BA,  0x6b, 0x00, 0x00, 0x00 },  /* κ to k */
  { 0x03BB,  0x6c, 0x00, 0x00, 0x00 },  /* λ to l */
  { 0x03BC,  0x6d, 0x00, 0x00, 0x00 },  /* μ to m */
  { 0x03BD,  0x6e, 0x00, 0x00, 0x00 },  /* ν to n */
  { 0x03BE,  0x78, 0x00, 0x00, 0x00 },  /* ξ to x */
  { 0x03BF,  0x6f, 0x00, 0x00, 0x00 },  /* ο to o */
  { 0x03C0,  0x70, 0x00, 0x00, 0x00 },  /* π to p */
  { 0x03C1,  0x72, 0x00, 0x00, 0x00 },  /* ρ to r */
  { 0x03C3,  0x73, 0x00, 0x00, 0x00 },  /* σ to s */
  { 0x03C4,  0x74, 0x00, 0x00, 0x00 },  /* τ to t */
  { 0x03C5,  0x79, 0x00, 0x00, 0x00 },  /* υ to y */
  { 0x03C6,  0x66, 0x00, 0x00, 0x00 },  /* φ to f */
  { 0x03C7,  0x63, 0x68, 0x00, 0x00 },  /* χ to ch */
  { 0x03C8,  0x70, 0x73, 0x00, 0x00 },  /* ψ to ps */
  { 0x03C9,  0x6f, 0x00, 0x00, 0x00 },  /* ω to o */
  { 0x03CA,  0x69, 0x00, 0x00, 0x00 },  /* ϊ to i */
  { 0x03CB,  0x79, 0x00, 0x00, 0x00 },  /* ϋ to y */
  { 0x03CC,  0x6f, 0x00, 0x00, 0x00 },  /* ό to o */
  { 0x03CD,  0x79, 0x00, 0x00, 0x00 },  /* ύ to y */
  { 0x03CE,  0x69, 0x00, 0x00, 0x00 },  /* ώ to i */
  { 0x0400,  0x45, 0x00, 0x00, 0x00 },  /* Ѐ to E */
  { 0x0401,  0x45, 0x00, 0x00, 0x00 },  /* Ё to E */
  { 0x0402,  0x44, 0x00, 0x00, 0x00 },  /* Ђ to D */
  { 0x0403,  0x47, 0x00, 0x00, 0x00 },  /* Ѓ to G */
  { 0x0404,  0x45, 0x00, 0x00, 0x00 },  /* Є to E */
  { 0x0405,  0x5a, 0x00, 0x00, 0x00 },  /* Ѕ to Z */
  { 0x0406,  0x49, 0x00, 0x00, 0x00 },  /* І to I */
  { 0x0407,  0x49, 0x00, 0x00, 0x00 },  /* Ї to I */
  { 0x0408,  0x4a, 0x00, 0x00, 0x00 },  /* Ј to J */
  { 0x0409,  0x49, 0x00, 0x00, 0x00 },  /* Љ to I */
  { 0x040A,  0x4e, 0x00, 0x00, 0x00 },  /* Њ to N */
  { 0x040B,  0x44, 0x00, 0x00, 0x00 },  /* Ћ to D */
  { 0x040C,  0x4b, 0x00, 0x00, 0x00 },  /* Ќ to K */
  { 0x040D,  0x49, 0x00, 0x00, 0x00 },  /* Ѝ to I */
  { 0x040E,  0x55, 0x00, 0x00, 0x00 },  /* Ў to U */
  { 0x040F,  0x44, 0x00, 0x00, 0x00 },  /* Џ to D */
  { 0x0410,  0x41, 0x00, 0x00, 0x00 },  /* А to A */
  { 0x0411,  0x42, 0x00, 0x00, 0x00 },  /* Б to B */
  { 0x0412,  0x56, 0x00, 0x00, 0x00 },  /* В to V */
  { 0x0413,  0x47, 0x00, 0x00, 0x00 },  /* Г to G */
  { 0x0414,  0x44, 0x00, 0x00, 0x00 },  /* Д to D */
  { 0x0415,  0x45, 0x00, 0x00, 0x00 },  /* Е to E */
  { 0x0416,  0x5a, 0x68, 0x00, 0x00 },  /* Ж to Zh */
  { 0x0417,  0x5a, 0x00, 0x00, 0x00 },  /* З to Z */
  { 0x0418,  0x49, 0x00, 0x00, 0x00 },  /* И to I */
  { 0x0419,  0x49, 0x00, 0x00, 0x00 },  /* Й to I */
  { 0x041A,  0x4b, 0x00, 0x00, 0x00 },  /* К to K */
  { 0x041B,  0x4c, 0x00, 0x00, 0x00 },  /* Л to L */
  { 0x041C,  0x4d, 0x00, 0x00, 0x00 },  /* М to M */
  { 0x041D,  0x4e, 0x00, 0x00, 0x00 },  /* Н to N */
  { 0x041E,  0x4f, 0x00, 0x00, 0x00 },  /* О to O */
  { 0x041F,  0x50, 0x00, 0x00, 0x00 },  /* П to P */
  { 0x0420,  0x52, 0x00, 0x00, 0x00 },  /* Р to R */
  { 0x0421,  0x53, 0x00, 0x00, 0x00 },  /* С to S */
  { 0x0422,  0x54, 0x00, 0x00, 0x00 },  /* Т to T */
  { 0x0423,  0x55, 0x00, 0x00, 0x00 },  /* У to U */
  { 0x0424,  0x46, 0x00, 0x00, 0x00 },  /* Ф to F */
  { 0x0425,  0x4b, 0x68, 0x00, 0x00 },  /* Х to Kh */
  { 0x0426,  0x54, 0x63, 0x00, 0x00 },  /* Ц to Tc */
  { 0x0427,  0x43, 0x68, 0x00, 0x00 },  /* Ч to Ch */
  { 0x0428,  0x53, 0x68, 0x00, 0x00 },  /* Ш to Sh */
  { 0x0429,  0x53, 0x68, 0x63, 0x68 },  /* Щ to Shch */
  { 0x042A,  0x61, 0x00, 0x00, 0x00 },  /*  to A */
  { 0x042B,  0x59, 0x00, 0x00, 0x00 },  /* Ы to Y */
  { 0x042C,  0x59, 0x00, 0x00, 0x00 },  /*  to Y */
  { 0x042D,  0x45, 0x00, 0x00, 0x00 },  /* Э to E */
  { 0x042E,  0x49, 0x75, 0x00, 0x00 },  /* Ю to Iu */
  { 0x042F,  0x49, 0x61, 0x00, 0x00 },  /* Я to Ia */
  { 0x0430,  0x61, 0x00, 0x00, 0x00 },  /* а to a */
  { 0x0431,  0x62, 0x00, 0x00, 0x00 },  /* б to b */
  { 0x0432,  0x76, 0x00, 0x00, 0x00 },  /* в to v */
  { 0x0433,  0x67, 0x00, 0x00, 0x00 },  /* г to g */
  { 0x0434,  0x64, 0x00, 0x00, 0x00 },  /* д to d */
  { 0x0435,  0x65, 0x00, 0x00, 0x00 },  /* е to e */
  { 0x0436,  0x7a, 0x68, 0x00, 0x00 },  /* ж to zh */
  { 0x0437,  0x7a, 0x00, 0x00, 0x00 },  /* з to z */
  { 0x0438,  0x69, 0x00, 0x00, 0x00 },  /* и to i */
  { 0x0439,  0x69, 0x00, 0x00, 0x00 },  /* й to i */
  { 0x043A,  0x6b, 0x00, 0x00, 0x00 },  /* к to k */
  { 0x043B,  0x6c, 0x00, 0x00, 0x00 },  /* л to l */
  { 0x043C,  0x6d, 0x00, 0x00, 0x00 },  /* м to m */
  { 0x043D,  0x6e, 0x00, 0x00, 0x00 },  /* н to n */
  { 0x043E,  0x6f, 0x00, 0x00, 0x00 },  /* о to o */
  { 0x043F,  0x70, 0x00, 0x00, 0x00 },  /* п to p */
  { 0x0440,  0x72, 0x00, 0x00, 0x00 },  /* р to r */
  { 0x0441,  0x73, 0x00, 0x00, 0x00 },  /* с to s */
  { 0x0442,  0x74, 0x00, 0x00, 0x00 },  /* т to t */
  { 0x0443,  0x75, 0x00, 0x00, 0x00 },  /* у to u */
  { 0x0444,  0x66, 0x00, 0x00, 0x00 },  /* ф to f */
  { 0x0445,  0x6b, 0x68, 0x00, 0x00 },  /* х to kh */
  { 0x0446,  0x74, 0x63, 0x00, 0x00 },  /* ц to tc */
  { 0x0447,  0x63, 0x68, 0x00, 0x00 },  /* ч to ch */
  { 0x0448,  0x73, 0x68, 0x00, 0x00 },  /* ш to sh */
  { 0x0449,  0x73, 0x68, 0x63, 0x68 },  /* щ to shch */
  { 0x044A,  0x61, 0x00, 0x00, 0x00 },  /*  to a */
  { 0x044B,  0x79, 0x00, 0x00, 0x00 },  /* ы to y */
  { 0x044C,  0x79, 0x00, 0x00, 0x00 },  /*  to y */
  { 0x044D,  0x65, 0x00, 0x00, 0x00 },  /* э to e */
  { 0x044E,  0x69, 0x75, 0x00, 0x00 },  /* ю to iu */
  { 0x044F,  0x69, 0x61, 0x00, 0x00 },  /* я to ia */
  { 0x0450,  0x65, 0x00, 0x00, 0x00 },  /* ѐ to e */
  { 0x0451,  0x65, 0x00, 0x00, 0x00 },  /* ё to e */
  { 0x0452,  0x64, 0x00, 0x00, 0x00 },  /* ђ to d */
  { 0x0453,  0x67, 0x00, 0x00, 0x00 },  /* ѓ to g */
  { 0x0454,  0x65, 0x00, 0x00, 0x00 },  /* є to e */
  { 0x0455,  0x7a, 0x00, 0x00, 0x00 },  /* ѕ to z */
  { 0x0456,  0x69, 0x00, 0x00, 0x00 },  /* і to i */
  { 0x0457,  0x69, 0x00, 0x00, 0x00 },  /* ї to i */
  { 0x0458,  0x6a, 0x00, 0x00, 0x00 },  /* ј to j */
  { 0x0459,  0x69, 0x00, 0x00, 0x00 },  /* љ to i */
  { 0x045A,  0x6e, 0x00, 0x00, 0x00 },  /* њ to n */
  { 0x045B,  0x64, 0x00, 0x00, 0x00 },  /* ћ to d */
  { 0x045C,  0x6b, 0x00, 0x00, 0x00 },  /* ќ to k */
  { 0x045D,  0x69, 0x00, 0x00, 0x00 },  /* ѝ to i */
  { 0x045E,  0x75, 0x00, 0x00, 0x00 },  /* ў to u */
  { 0x045F,  0x64, 0x00, 0x00, 0x00 },  /* џ to d */
  { 0x1E02,  0x42, 0x00, 0x00, 0x00 },  /* Ḃ to B */
  { 0x1E03,  0x62, 0x00, 0x00, 0x00 },  /* ḃ to b */
  { 0x1E0A,  0x44, 0x00, 0x00, 0x00 },  /* Ḋ to D */
  { 0x1E0B,  0x64, 0x00, 0x00, 0x00 },  /* ḋ to d */
  { 0x1E1E,  0x46, 0x00, 0x00, 0x00 },  /* Ḟ to F */
  { 0x1E1F,  0x66, 0x00, 0x00, 0x00 },  /* ḟ to f */
  { 0x1E40,  0x4D, 0x00, 0x00, 0x00 },  /* Ṁ to M */
  { 0x1E41,  0x6D, 0x00, 0x00, 0x00 },  /* ṁ to m */
  { 0x1E56,  0x50, 0x00, 0x00, 0x00 },  /* Ṗ to P */
  { 0x1E57,  0x70, 0x00, 0x00, 0x00 },  /* ṗ to p */
  { 0x1E60,  0x53, 0x00, 0x00, 0x00 },  /* Ṡ to S */
  { 0x1E61,  0x73, 0x00, 0x00, 0x00 },  /* ṡ to s */
  { 0x1E6A,  0x54, 0x00, 0x00, 0x00 },  /* Ṫ to T */
  { 0x1E6B,  0x74, 0x00, 0x00, 0x00 },  /* ṫ to t */
  { 0x1E80,  0x57, 0x00, 0x00, 0x00 },  /* Ẁ to W */
  { 0x1E81,  0x77, 0x00, 0x00, 0x00 },  /* ẁ to w */
  { 0x1E82,  0x57, 0x00, 0x00, 0x00 },  /* Ẃ to W */
  { 0x1E83,  0x77, 0x00, 0x00, 0x00 },  /* ẃ to w */
  { 0x1E84,  0x57, 0x00, 0x00, 0x00 },  /* Ẅ to W */
  { 0x1E85,  0x77, 0x00, 0x00, 0x00 },  /* ẅ to w */
  { 0x1EF2,  0x59, 0x00, 0x00, 0x00 },  /* Ỳ to Y */
  { 0x1EF3,  0x79, 0x00, 0x00, 0x00 },  /* ỳ to y */
  { 0x2018,  0x27, 0x00, 0x00, 0x00 },  /* ‘ to ' */
  { 0x2019,  0x27, 0x00, 0x00, 0x00 },  /* ’ to ' */
  { 0x201A,  0x27, 0x00, 0x00, 0x00 },  /* ‚ to ' */
  { 0x201B,  0x27, 0x00, 0x00, 0x00 },  /* ‛ to ' */
  { 0x201C,  0x22, 0x00, 0x00, 0x00 },  /* “ to " */
  { 0x201D,  0x22, 0x00, 0x00, 0x00 },  /* ” to " */
  { 0x201E,  0x22, 0x00, 0x00, 0x00 },  /* „ to " */
  { 0x201F,  0x22, 0x00, 0x00, 0x00 },  /* ‟ to " */
  { 0x2039,  0x27, 0x00, 0x00, 0x00 },  /* ‹ to ' */
  { 0x203A,  0x27, 0x00, 0x00, 0x00 },  /* › to ' */
  { 0xFB00,  0x66, 0x66, 0x00, 0x00 },  /* ﬀ to ff */
  { 0xFB01,  0x66, 0x69, 0x00, 0x00 },  /* ﬁ to fi */
  { 0xFB02,  0x66, 0x6C, 0x00, 0x00 },  /* ﬂ to fl */
  { 0xFB05,  0x73, 0x74, 0x00, 0x00 },  /* ﬅ to st */
  { 0xFB06,  0x73, 0x74, 0x00, 0x00 },  /* ﬆ to st */
};

static int translit_cmp(const void *aa, const void *bb)
{
  const Transliteration *a = aa, *b = bb;
  if (a->cFrom < b->cFrom) {
    return -1;
  } else if (a->cFrom > b->cFrom) {
    return 1;
  } else {
    return 0;
  }
}

static enum translit_action
translit_to_latin1(UChar32 c, char rep[4])
{
  Transliteration *t, s;
  
  if (c <= 0xFF) {
    return TRANS_KEEP;
  }

#ifdef HAVE_ICU
  if (u_charType(c) == U_NON_SPACING_MARK) {
    return TRANS_SKIP;
  }
#endif

  s.cFrom = c;
  t = bsearch(&s, translit, sizeof(translit) / sizeof(Transliteration),
              sizeof(Transliteration), translit_cmp);
  if (t) {
    rep[0] = t->cTo0;
    rep[1] = t->cTo1;
    rep[2] = t->cTo2;
    rep[3] = t->cTo3;
    return TRANS_REPLACE;
  } else {
    rep[0] = '?';
    rep[1] = '\0';
    return TRANS_REPLACE;
  }
}

/**
 * Convert a UTF-8 encoded string to Latin-1
 *
 * Invalid byte sequences are turned into question marks. Characters
 * outside the Latin-1 range are either turned into question marks or
 * transliterated into ASCII equivalents.
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

  latin1 = mush_calloc((translit ? len * 4 : len) + 1, 1, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c;
    U8_NEXT_OR_FFFD(utf8, i, len, c);
    if (translit) {
      char rep[4];
      int n;
      switch (translit_to_latin1(c, rep)) {
      case TRANS_KEEP:
        latin1[o++] = c;
        break;
      case TRANS_REPLACE:
        for (n = 0; n < 4; n += 1) {
          if (rep[n]) {
            latin1[o++] = rep[n];
          } else {
            break;
          }
        }
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
 * Characters outside the Latin-1 range are either turned into
 * question marks or transliterated into ASCII equivalents.
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

  latin1 = mush_calloc((translit ? len * 4 : len) + 1, 1, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c;
    U8_NEXT_UNSAFE(utf8, i, c);
    if (translit) {
      char rep[4];
      int n;
      switch (translit_to_latin1(c, rep)) {
      case TRANS_KEEP:
        latin1[o++] = c;
        break;
      case TRANS_REPLACE:
        for (n = 0; n < 4; n += 1) {
          if (rep[n]) {
            latin1[o++] = rep[n];
          } else {
            break;
          }
        }
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
 * Characters outside the Latin-1 range are either turned into
 * question marks or transliterated into ASCII equivalents.
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
  latin1 = mush_calloc((translit ? len * 4 : len) + 1, 1, name);
  for (i = 0, o = 0; i < len; i += 1) {
    UChar32 c = utf32[i];
    if (translit) {
      char rep[4];
      int n;
      switch (translit_to_latin1(c, rep)) {
      case TRANS_KEEP:
        latin1[o++] = c;
        break;
      case TRANS_REPLACE:
        for (n = 0; n < 4; n += 1) {
          if (rep[n]) {
            latin1[o++] = rep[n];
          } else {
            break;
          }
        }
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
 * Characters outside the Latin-1 range are either turned into
 * question marks or transliterated into ASCII equivalents.
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
  latin1 = mush_calloc((translit ? len * 4 : len) + 1, 1, name);
  for (i = 0, o = 0; i < len; ) {
    UChar32 c;
    U16_NEXT_UNSAFE(utf16, i, c);
    if (translit) {
      char rep[4];
      int n;
      switch (translit_to_latin1(c, rep)) {
      case TRANS_KEEP:
        latin1[o++] = c;
        break;
      case TRANS_REPLACE:
        for (n = 0; n < 4; n += 1) {
          if (rep[n]) {
            latin1[o++] = rep[n];
          } else {
            break;
          }
        }
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
 * Invalid byte sequences get turned into question marks. Tries to
 * gracefully downgrade Unicode characters to Latin-1 characters.
 * Characters that can't are replaced by question marks.
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

  norm16 = normalize_utf16(NORM_NFC, utf16, ulen, &nlen, "temp.utf16");
  mush_free(utf16, "temp.utf16");
  if (!norm16) {
    return NULL;
  }

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
  mush_free(utf16, "temp.utf16");
  if (!norm16) {
    return NULL;
  }

  norm8 = utf16_to_utf8(norm16, nlen, outlen, name);
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
  san8 = mush_calloc((len * 3) + 1, 1, name);
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
