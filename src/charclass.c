/** \file charclass.c
 *
 * \brief Character classification functions
 */

#include <pcre.h>

#include "conf.h"
#include "charclass.h"
#include "externs.h"
#include "log.h"

static pcre *
build_re(const char *sre)
{
  const char *err;
  int erroffset;
  pcre *re = pcre_compile(sre, PCRE_NO_UTF8_CHECK | PCRE_UTF8 | PCRE_UCP, &err,
                          &erroffset, NULL);
  if (!re) {
    do_rawlog(LT_ERR, "Unable to compile RE '%s': %s", sre, err);
    mush_panic("Internal error");
  }
  return re;
}

static bool
check_re(const pcre *re, UChar32 c)
{
  char cbuf[5] = {'\0'};
  int clen = 0;
  int ovector[10];
  U8_APPEND_UNSAFE(cbuf, clen, c);
  return pcre_exec(re, NULL, cbuf, clen, 0, PCRE_NO_UTF8_CHECK, ovector, 10) >
         0;
}

bool
re_isprint(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("[[:print:]]");
  }
  return check_re(re, c);
}

bool
re_isspace(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("\\p{Xps}");
  }
  return check_re(re, c);
}

bool
re_islower(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("\\p{Ll}");
  }
  return check_re(re, c);
}

bool
re_isupper(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("\\p{Lu}");
  }
  return check_re(re, c);
}

bool
re_isdigit(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("\\p{Nd}");
  }
  return check_re(re, c);
}

bool
re_isalnum(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("\\p{Xan}");
  }
  return check_re(re, c);
}

bool
re_isalpha(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("\\p{L}");
  }
  return check_re(re, c);
}

bool
re_ispunct(UChar32 c)
{
  static pcre *re = NULL;
  if (!re) {
    re = build_re("[[:punct:]]");
  }
  return check_re(re, c);
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

  int first_cp = 0;
  U8_NEXT(utf8, first_cp, len, c);
  if (c < 0) {
    return first_cp;
  }

  i = first_cp;
  int prev_i = 0;
  gcb_cat cat = get_gcb(c);
  if (cat == U_GCB_PREPEND) {
    i += prepend_len8(utf8, i, len);
    prev_i = i;
    U8_NEXT(utf8, i, len, c);
    if (c <= 0) {
      return first_cp;
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
      return first_cp;
    }
    break;
  case U_GCB_CONTROL:
    return first_cp;
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

/** Return the number of bytes the first codepoint in a utf-8 string takes. */
int
cpbytes(const char *s)
{
  int len = 0;
  U8_FWD_1(s, len, -1);
  return len;
}
