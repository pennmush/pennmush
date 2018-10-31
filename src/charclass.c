/** \file charclass.c
 *
 * \brief Character classification functions
 */

#include "conf.h"
#include "charclass.h"
#include "externs.h"
#include "log.h"
#include "mypcre.h"
#include "tests.h"
#include "case.h"

static pcre2_code *
build_re(const char *sre)
{
  int errcode;
  PCRE2_SIZE erroffset;
  pcre2_code *re =
    pcre2_compile((const PCRE2_UCHAR *) sre, PCRE2_ZERO_TERMINATED,
                  re_compile_flags | PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP,
                  &errcode, &erroffset, re_compile_ctx);
  if (!re) {
    PCRE2_UCHAR errmsg[512];
    pcre2_get_error_message(errcode, errmsg, sizeof errmsg);
    do_rawlog(LT_ERR, "Unable to compile RE '%s': %s", sre,
              (const char *) errmsg);
    mush_panic("Internal error");
  }
  return re;
}

static bool
check_re(const pcre2_code *re, pcre2_match_data *md, UChar32 c)
{
  PCRE2_UCHAR cbuf[5] = {'\0'};
  int clen = 0;
  int m;

  U8_APPEND_UNSAFE(cbuf, clen, c);
  m = pcre2_match(re, cbuf, clen, 0, re_match_flags | PCRE2_NO_UTF_CHECK, md,
                  re_match_ctx);
  return m >= 0;
}

bool
re_isprint(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("[[:print:]]");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(isprint)
{
  TEST("re_isprint.1", re_isprint('a'));
  TEST("re_isprint.2",
       re_isprint(0x00C4)); // LATIN CAPITAL LETTER A WITH DIARESIS
  TEST("re_isprint.3", re_isprint(0x0014) == 0);
  TEST("re_isprint.4", re_isprint(' '));
#ifdef HAVE_ICU
  TEST("uni_isprint.1", uni_isprint('a'));
  TEST("uni_isprint.2",
       uni_isprint(0x00C4)); // LATIN CAPITAL LETTER A WITH DIARESIS
  TEST("uni_isprint.3", uni_isprint(0x0014) == 0);
  TEST("uni_isprint.4", uni_isprint(' '));
#endif
  TEST("ascii_isprint.1", ascii_isprint('a'));
  TEST("ascii_isprint.2",
       ascii_isprint(0x00C4) == 0); // LATIN CAPITAL LETTER A WITH DIARESIS
}

bool
re_isspace(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("\\p{Xps}");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(isspace)
{
  TEST("re_isspace.1", re_isspace(' '));
  TEST("re_isspace.2", re_isspace(0x00A0)); // NO-BREAK SPACE
  TEST("re_isspace.3", re_isspace('A') == 0);
#ifdef HAVE_ICU
  TEST("uni_isspace.1", uni_isspace(' '));
  TEST("uni_isspace.2", uni_isspace(0x00A0)); // NO-BREAK SPACE
  TEST("uni_isspace.3", uni_isspace('A') == 0);
#endif
  TEST("ascii_isspace.1", ascii_isspace(' '));
  TEST("ascii_isspace.2", ascii_isspace(0x00A0) == 0); // NO-BREAK SPACE
}

bool
re_islower(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("\\p{Ll}");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(islower)
{
  TEST("re_islower.1", re_islower('a'));
  TEST("re_islower.2", re_islower(0x00E1)); // LATIN SMALL LETTER A WITH ACUTE
  TEST("re_islower.3", re_islower('A') == 0);
  TEST("re_islower.4",
       re_islower(0x00C1) == 0); // LATIN CAPITAL LETTER A WITH ACUTE
  TEST("re_islower.5", re_islower('0') == 0);
#ifdef HAVE_ICU
  TEST("uni_islower.1", uni_islower('a'));
  TEST("uni_islower.2", uni_islower(0x00E1)); // LATIN SMALL LETTER A WITH ACUTE
  TEST("uni_islower.3", uni_islower('A') == 0);
  TEST("uni_islower.4",
       uni_islower(0x00C1) == 0); // LATIN CAPITAL LETTER A WITH ACUTE
  TEST("uni_islower.5", uni_islower('0') == 0);
#endif
  TEST("ascii_islower.1", ascii_islower('a'));
  TEST("ascii_islower.2",
       ascii_islower(0x00E1) == 0); // LATIN SMALL LETTER A WITH ACUTE
  TEST("ascii_islower.3", ascii_islower('A') == 0);
  TEST("ascii_islower.4",
       ascii_islower(0x00C1) == 0); // LATIN CAPITAL LETTER A WITH ACUTE
  TEST("ascii_islower.5", ascii_islower('0') == 0);
}

bool
re_isupper(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("\\p{Lu}");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(isupper)
{
  TEST("re_isupper.1", re_isupper('a') == 0);
  TEST("re_isupper.2",
       re_isupper(0x00E1) == 0); // LATIN SMALL LETTER A WITH ACUTE
  TEST("re_isupper.3", re_isupper('A'));
  TEST("re_isupper.4", re_isupper(0x00C1)); // LATIN CAPITAL LETTER A WITH ACUTE
  TEST("re_isupper.5", re_isupper('0') == 0);
#ifdef HAVE_ICU
  TEST("uni_isupper.1", uni_isupper('a') == 0);
  TEST("uni_isupper.2",
       uni_isupper(0x00E1) == 0); // LATIN SMALL LETTER A WITH ACUTE
  TEST("uni_isupper.3", uni_isupper('A'));
  TEST("uni_isupper.4",
       uni_isupper(0x00C1)); // LATIN CAPITAL LETTER A WITH ACUTE
  TEST("uni_isupper.5", uni_isupper('0') == 0);
#endif
  TEST("ascii_isupper.1", ascii_isupper('a') == 0);
  TEST("ascii_isupper.2",
       ascii_isupper(0x00E1) == 0); // LATIN SMALL LETTER A WITH ACUTE
  TEST("ascii_isupper.3", ascii_isupper('A'));
  TEST("ascii_isupper.4",
       ascii_isupper(0x00C1) == 0); // LATIN CAPITAL LETTER A WITH ACUTE
  TEST("ascii_isupper.5", ascii_isupper('0') == 0);
}

bool
re_isdigit(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("\\p{Nd}");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(isdigit)
{
  TEST("re_isdigit.1", re_isdigit('0'));
  TEST("re_isdigit.2", re_isdigit('1'));
  TEST("re_isdigit.3", re_isdigit('2'));
  TEST("re_isdigit.4", re_isdigit('3'));
  TEST("re_isdigit.5", re_isdigit('4'));
  TEST("re_isdigit.6", re_isdigit('5'));
  TEST("re_isdigit.7", re_isdigit('6'));
  TEST("re_isdigit.8", re_isdigit('7'));
  TEST("re_isdigit.9", re_isdigit('8'));
  TEST("re_isdigit.10", re_isdigit('9'));
  TEST("re_isdigit.11", re_isdigit(0x09E7)); // BENGALI DIGIT ONE
  TEST("re_isdigit.12", re_isdigit(0x0666)); // ARABIC-INDIC DIGIT SIX
  TEST("re_isdigit.13", re_isdigit('a') == 0);
  TEST("re_isdigit.14", re_isdigit(' ') == 0);
#ifdef HAVE_ICU
  TEST("uni_isdigit.1", uni_isdigit('0'));
  TEST("uni_isdigit.2", uni_isdigit('1'));
  TEST("uni_isdigit.3", uni_isdigit('2'));
  TEST("uni_isdigit.4", uni_isdigit('3'));
  TEST("uni_isdigit.5", uni_isdigit('4'));
  TEST("uni_isdigit.6", uni_isdigit('5'));
  TEST("uni_isdigit.7", uni_isdigit('6'));
  TEST("uni_isdigit.8", uni_isdigit('7'));
  TEST("uni_isdigit.9", uni_isdigit('8'));
  TEST("uni_isdigit.10", uni_isdigit('9'));
  TEST("uni_isdigit.11", uni_isdigit(0x09E7)); // BENGALI DIGIT ONE
  TEST("uni_isdigit.12", uni_isdigit(0x0666)); // ARABIC-INDIC DIGIT SIX
  TEST("uni_isdigit.13", uni_isdigit('a') == 0);
  TEST("uni_isdigit.14", uni_isdigit(' ') == 0);
#endif
  TEST("ascii_isdigit.1", ascii_isdigit('0'));
  TEST("ascii_isdigit.2", ascii_isdigit(0x09E7) == 0); // BENGALI DIGIT ONE
  TEST("ascii_isdigit.3", ascii_isdigit('a') == 0);
  TEST("ascii_isdigit.4", ascii_isdigit(' ') == 0);
}

bool
re_isalnum(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("\\p{Xan}");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(isalnum)
{
  TEST("re_isalnum.1", re_isalnum('0'));
  TEST("re_isalnum.2", re_isalnum('1'));
  TEST("re_isalnum.3", re_isalnum('2'));
  TEST("re_isalnum.4", re_isalnum('3'));
  TEST("re_isalnum.5", re_isalnum('4'));
  TEST("re_isalnum.6", re_isalnum('5'));
  TEST("re_isalnum.7", re_isalnum('6'));
  TEST("re_isalnum.8", re_isalnum('7'));
  TEST("re_isalnum.9", re_isalnum('8'));
  TEST("re_isalnum.10", re_isalnum('9'));
  TEST("re_isalnum.11", re_isalnum(0x09E7)); // BENGALI DIGIT ONE
  TEST("re_isalnum.12", re_isalnum(0x0666)); // ARABIC-INDIC DIGIT SIX
  TEST("re_isalnum.13", re_isalnum('a'));
  TEST("re_isalnum.14", re_isalnum(' ') == 0);
  TEST("re_isalnum.15", re_isalnum(0x00A3) == 0); // POUND SIGN
  TEST("re_isalnum.16",
       re_isalnum(0x00E1)); // LATIN SMALL LETTER A WITH ACUTE
  TEST("re_isalnum.17", re_isalnum('A'));
  TEST("re_isalnum.18",
       re_isalnum(0x00C1)); // LATIN CAPITAL LETTER A WITH ACUTE
#ifdef HAVE__ICU
  TEST("uni_isalnum.1", uni_isalnum('0'));
  TEST("uni_isalnum.2", uni_isalnum('1'));
  TEST("uni_isalnum.3", uni_isalnum('2'));
  TEST("uni_isalnum.4", uni_isalnum('3'));
  TEST("uni_isalnum.5", uni_isalnum('4'));
  TEST("uni_isalnum.6", uni_isalnum('5'));
  TEST("uni_isalnum.7", uni_isalnum('6'));
  TEST("uni_isalnum.8", uni_isalnum('7'));
  TEST("uni_isalnum.9", uni_isalnum('8'));
  TEST("uni_isalnum.10", uni_isalnum('9'));
  TEST("uni_isalnum.11", uni_isalnum(0x09E7)); // BENGALI DIGIT ONE
  TEST("uni_isalnum.12", uni_isalnum(0x0666)); // ARABIC-INDIC DIGIT SIX
  TEST("uni_isalnum.13", uni_isalnum('a'));
  TEST("uni_isalnum.14", uni_isalnum(' ') == 0);
  TEST("uni_isalnum.15", uni_isalnum(0x00A3) == 0); // POUND SIGN
  TEST("uni_isalnum.16",
       uni_isalnum(0x00E1)); // LATIN SMALL LETTER A WITH ACUTE
  TEST("uni_isalnum.17", re_isalnum('A'));
  TEST("uni_isalnum.18",
       uni_isalnum(0x00C1)); // LATIN CAPITAL LETTER A WITH ACUTE
#endif
  TEST("ascii_isalnum.1", ascii_isalnum('0'));
  TEST("ascii_isalnum.2", ascii_isalnum(0x09E7) == 0); // BENGALI DIGIT ONE
  TEST("ascii_isalnum.3", ascii_isalnum('a'));
  TEST("ascii_isalnum.4", ascii_isalnum(' ') == 0);
  TEST("ascii_isalnum.5", ascii_isalnum(0x00A3) == 0); // POUND SIGN
  TEST("ascii_isalnum.6",
       ascii_isalnum(0x00E1) == 0); // LATIN SMALL LETTER A WITH ACUTE
  TEST("ascii_isalnum.7", ascii_isalnum('A'));
  TEST("ascii_isalnum.8",
       ascii_isalnum(0x00C1) == 0); // LATIN CAPITAL LETTER A WITH ACUTE
}

bool
re_isalpha(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("\\p{L}");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(isalpha)
{
  TEST("re_isalpha.1", re_isalpha('a'));
  TEST("re_isalpha.2", re_isalpha('0') == 0);
  TEST("re_isalpha.3", re_isalpha(0x00E1));
  TEST("re_isalpha.4", re_isalpha(0x00A3) == 0); // POUND SIGN
#ifdef HAVE_ICU
  TEST("uni_isalpha.1", uni_isalpha('a'));
  TEST("uni_isalpha.2", uni_isalpha('0') == 0);
  TEST("uni_isalpha.3", uni_isalpha(0x00E1));
  TEST("uni_isalpha.4", uni_isalpha(0x00A3) == 0); // POUND SIGN
#endif
  TEST("ascii_isalpha.1", ascii_isalpha('a'));
  TEST("ascii_isalpha.2", ascii_isalpha('0') == 0);
  TEST("ascii_isalpha.3", ascii_isalpha(0x00E1) == 0);
  TEST("ascii_isalpha.4", ascii_isalpha(0x00A3) == 0); // POUND SIGN
}

bool
re_ispunct(UChar32 c)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;
  if (!re) {
    re = build_re("[[:punct:]]");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }
  return check_re(re, md, c);
}

TEST_GROUP(ispunct)
{
  TEST("re_ispunct.1", re_ispunct('.'));
  TEST("re_ispunct.2", re_ispunct(' ') == 0);
  TEST("re_ispunct.3", re_ispunct('a') == 0);
  TEST("re_ispunct.4", re_ispunct(0x00A1)); // INVERTED EXCLAIMATION MARK
#ifdef HAVE_ICU
  TEST("uni_ispunct.1", uni_ispunct('.'));
  TEST("uni_ispunct.2", uni_ispunct(' ') == 0);
  TEST("uni_ispunct.3", uni_ispunct('a') == 0);
  TEST("uni_ispunct.4", uni_ispunct(0x00A1)); // INVERTED EXCLAIMATION MARK
#endif
  TEST("ascii_ispunct.1", ascii_ispunct('.'));
  TEST("ascii_ispunct.2", ascii_ispunct(0x00A1) == 0);
}

static int
re_gcbytes(const char *s)
{
  static pcre2_code *re = NULL;
  static pcre2_match_data *md;

  if (!re) {
    re = build_re("^\\X");
    md = pcre2_match_data_create_from_pattern(re, NULL);
  }

  if (pcre2_match(re, (const PCRE2_UCHAR *) s, PCRE2_ZERO_TERMINATED, 0,
                  re_match_flags, md, re_match_ctx) >= 0) {
    PCRE2_SIZE len;
    pcre2_substring_length_bynumber(md, 0, &len);
    return len;
  } else {
    return 1;
  }
}

#ifdef HAVE_ICU

typedef enum UGraphemeClusterBreak gcb_cat;
static gcb_cat
get_gcb(UChar32 c)
{
  return u_getIntPropertyValue(c, UCHAR_GRAPHEME_CLUSTER_BREAK);
}

/* UTF-8 Extended Grapheme Cluster parser */

// returns length of prepend*
static int
prepend_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;

  for (int prev_i = i; 1; prev_i = i) {
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i - start_i;
    }
    if (get_gcb(c) != U_GCB_PREPEND) {
      return prev_i - start_i;
    }
  }
}

// returns length of Regional_Indicator*
static int
ri_sequence_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;

  for (int prev_i = i; 1; prev_i = i) {
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i - start_i;
    }
    if (get_gcb(c) != U_GCB_REGIONAL_INDICATOR) {
      return prev_i = start_i;
    }
  }
}

static int
l_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;
  for (int prev_i = i; 1; prev_i = i) {
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i - start_i;
    }
    if (get_gcb(c) != U_GCB_L) {
      return prev_i - start_i;
    }
  }
}

static int
v_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;
  for (int prev_i = i; 1; prev_i = i) {
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i - start_i;
    }
    if (get_gcb(c) != U_GCB_V) {
      return prev_i - start_i;
    }
  }
}

static int
t_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;
  for (int prev_i = i; 1; prev_i = i) {
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i - start_i;
    }
    if (get_gcb(c) != U_GCB_T) {
      return prev_i - start_i;
    }
  }
}

// returns length of Hangul-Syllable
static int
hangul_syllable_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;

  // | L+

  i += l_len8(utf8, i, len);

  int prev_i = i;
  U8_NEXT(utf8, i, len, c);
  if (c <= 0) {
    return prev_i - start_i; // | L+
  }
  switch (get_gcb(c)) {
  case U_GCB_V:
  case U_GCB_LV:
    //  L* V+ T*
    // | L* LV V* T*
    i += v_len8(utf8, i, len);
    i += t_len8(utf8, i, len);
    return i - start_i;
  case U_GCB_LVT:
    // | L* LVT T*
    i += t_len8(utf8, i, len);
    return i - start_i;
  case U_GCB_T:
    if (prev_i == start_i) { // | T+
      i += t_len8(utf8, i, len);
      return i - start_i;
    } else {
      return prev_i - start_i; // | L+
    }
  default:
    return prev_i - start_i; // | L+
  }
}

// Returns length of SpacingMark*
static int
sm_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;
  for (int prev_i = i; 1; prev_i = i) {
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i - start_i;
    }
    if (get_gcb(c) != U_GCB_SPACING_MARK) {
      return prev_i - start_i;
    }
  }
}

// Returns length of GraphemeExtend*
static int
ge_len8(const char *utf8, int i, int len)
{
  int start_i = i;
  UChar32 c;
  for (int prev_i = i; 1; prev_i = i) {
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i - start_i;
    }
    if (get_gcb(c) != U_GCB_EXTEND) {
      return prev_i - start_i;
    }
  }
}

static int
egc_len8(const char *utf8, int len)
{
  UChar32 c;
  int i = 0;

  if (!*utf8 || len == 0) {
    return 0;
  }

  // CRLF matches
  if (len != 1 && utf8[0] == '\r' && utf8[1] == '\n') {
    return 2;
  }

  int firstcp = 0;
  U8_NEXT(utf8, firstcp, len, c);
  if (c < 0) {
    return firstcp;
  }

  i = firstcp;
  int prev_i = 0;
  gcb_cat cat = get_gcb(c);
  if (cat == U_GCB_PREPEND) {
    i += prepend_len8(utf8, i, len);
    prev_i = i;
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return firstcp;
    }
    cat = get_gcb(c);
  }

  // (RI-Sequence | Hangul-Syllable | !Control)
  switch (cat) {
  case U_GCB_REGIONAL_INDICATOR:
    i += ri_sequence_len8(utf8, i, len);
    break;
  case U_GCB_L:
  case U_GCB_T:
  case U_GCB_V:
  case U_GCB_LV:
  case U_GCB_LVT:
    i = prev_i;
    i += hangul_syllable_len8(utf8, i, len);
    if (prev_i == i) {
      return firstcp;
    }
    break;
  case U_GCB_CONTROL:
    return firstcp;
  default:
    (void) 0;
  }

  // ( Grapheme_Extend | SpacingMark )*
  do {
    prev_i = i;
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return prev_i;
    }
    cat = get_gcb(c);

    if (cat == U_GCB_EXTEND) {
      i += ge_len8(utf8, i, len);
    } else if (cat == U_GCB_SPACING_MARK) {
      i += sm_len8(utf8, i, len);
    } else {
      return prev_i;
    }
  } while (1);
}

/** Returns the number of bytes of the first extended grapheme cluster in a
 * UTF-8 string */
int
gcbytes(const char *s)
{
  return egc_len8(s, -1);
}

#else

int
gcbytes(const char *s)
{
  return re_gcbytes(s);
}

#endif

TEST_GROUP(gcbytes)
{
  TEST("re_gcbytes.1", re_gcbytes("a") == 1);
  TEST("re_gcbytes.2", re_gcbytes("\xC3\xA1") == 2);
  TEST("re_gcbytes.3", re_gcbytes("\x61\xCC\xB1") == 3);
  TEST("re_gcbytes.4", re_gcbytes("aa") == 1);
  TEST("re_gcbytes.5", re_gcbytes("\xC3\xA1q") == 2);
  TEST("re_gcbytes.6", re_gcbytes("\x61\xCC\xB1q") == 3);
  TEST("re_gcbytes.7", re_gcbytes("a\xC3\xA1") == 1);
#ifdef HAVE_ICU
  TEST("gcbytes.1", gcbytes("a") == 1);
  TEST("gcbytes.2", gcbytes("\xC3\xA1") == 2);
  TEST("gcbytes.3", gcbytes("\x61\xCC\xB1") == 3);
  TEST("gcbytes.4", gcbytes("aa") == 1);
  TEST("gcbytes.5", gcbytes("\xC3\xA1q") == 2);
  TEST("gcbytes.6", gcbytes("\x61\xCC\xB1q") == 3);
  TEST("gcbytes.7", gcbytes("a\xC3\xA1") == 1);
#endif
}

/** Calculate the number of bytes used by the first N extended
    grapheme clusters in a UTF-8 string. */
int
strnlen_gc(const char *s, int n)
{
  int bytes = 0;
  while (*s && n-- > 0) {
    int len = gcbytes(s);
    s += len;
    bytes += len;
  }
  return bytes;
}

TEST_GROUP(strnlen_gc)
{
  // TEST strnlen_gc REQUIRES gcbytes
  TEST("strnlen_gc.1", strnlen_gc("aa", 5) == 2);
  TEST("strnlen_gc.2", strnlen_gc("\xC3\xA1q", 5) == 3);
  TEST("strnlen_gc.3", strnlen_gc("a\x61\xCC\xB1q", 2) == 4);
  TEST("strnlen_gc.4", strnlen_gc("aa", 1) == 1);
  TEST("strnlen_gc.5", strnlen_gc("\xC3\xA1q", 1) == 2);
  TEST("strnlen_gc.6", strnlen_gc("\x61\xCC\xB1q", 1) == 3);
}

TEST_GROUP(toupper)
{
  TEST("ascii_toupper.1", ascii_toupper('a') == 'A');
  TEST("ascii_toupper.2", ascii_toupper(0x00E1) == 0x00E1);
  TEST("ascii_toupper.3", ascii_toupper(0x00A3) == 0x00A3);
  TEST("ascii_toupper.4", ascii_toupper('1') == '1');
  TEST("ascii_toupper.5", ascii_toupper('B') == 'B');
#ifdef HAVE_ICU
  TEST("uni_toupper.1", uni_toupper('a') == 'A');
  TEST("uni_toupper.2", uni_toupper(0x00E1) == 0x00C1);
  TEST("uni_toupper.3", uni_toupper(0x00A3) == 0x00A3);
  TEST("uni_toupper.4", uni_toupper('1') == '1');
  TEST("uni_toupper.5", uni_toupper('B') == 'B');
#endif
}

TEST_GROUP(tolower)
{
  TEST("ascii_tolower.1", ascii_tolower('A') == 'a');
  TEST("ascii_tolower.2", ascii_tolower(0x00C1) == 0x00C1);
  TEST("ascii_tolower.3", ascii_tolower(0x00A3) == 0x00A3);
  TEST("ascii_tolower.4", ascii_tolower('1') == '1');
  TEST("ascii_tolower.5", ascii_tolower('b') == 'b');
#ifdef HAVE_ICU
  TEST("uni_tolower.1", uni_tolower('A') == 'a');
  TEST("uni_tolower.2", uni_tolower(0x00C1) == 0x00E1);
  TEST("uni_tolower.3", uni_tolower(0x00A3) == 0x00A3);
  TEST("uni_tolower.4", uni_tolower('1') == '1');
  TEST("uni_tolower.5", uni_tolower('b') == 'b');
#endif
}
