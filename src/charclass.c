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
