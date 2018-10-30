/** \file charclass.c
 *
 * \brief Character classification functions
 */

#include "conf.h"
#include "charclass.h"
#include "externs.h"
#include "log.h"
#include "mypcre.h"

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
  m = pcre2_match(re, cbuf, clen, 0,
                  re_match_flags | PCRE2_UTF | PCRE2_NO_UTF_CHECK, md,
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
  static pcre *re = NULL;
  int len;
  int ovec[10];
  if (!re) {
    re = build_re("^\\X");
  }

  len = strlen(s);

  if (pcre_exec(re, NULL, s, len, 0, 0, ovec, 10) > 0) {
    return ovec[1];
  } else {
    return 1;
  }
}

#endif

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
