/**
 * \file funtime.c
 *
 * \brief Time functions for mushcode.
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <string.h>
#include <ctype.h>
#if  (defined(__GNUC__) || defined(__LCC__)) && !defined(__USE_XOPEN_EXTENDED)
/* Required to get the getdate() prototype on glibc. */
#define __USE_XOPEN_EXTENDED
#endif
#include <time.h>
#include <errno.h>
#include "conf.h"
#include "externs.h"
#include "parse.h"
#include "dbdefs.h"
#include "log.h"
#include "match.h"
#include "attrib.h"
#include "confmagic.h"

int do_convtime(const char *mystr, struct tm *ttm);
void do_timestring(char *buff, char **bp, const char *format,
                   unsigned long secs);

extern char valid_timefmt_codes[256];

FUNCTION(fun_timefmt)
{
  char s[BUFFER_LEN];
  struct tm *ttm;
  time_t tt;
  size_t len, n;

  if (!args[0] || !*args[0])
    return;                     /* No field? Bad user. */

  if (nargs == 2) {
    /* This is silly, but time_t is signed on several platforms,
     * so we can't assign an unsigned int to it safely
     */
    if (!is_integer(args[1])) {
      safe_str(T(e_int), buff, bp);
      return;
    }
    tt = parse_integer(args[1]);
    if (errno == ERANGE) {
      safe_str(T(e_range), buff, bp);
      return;
    }
    if (tt < 0) {
      safe_str(T(e_uint), buff, bp);
      return;
    }
  } else
    tt = mudtime;

  ttm = localtime(&tt);
  len = arglens[0];
  for (n = 0; n < len; n++) {
    if (args[0][n] == '%')
      args[0][n] = 0x5;
    else if (args[0][n] == '$') {
      args[0][n] = '%';
      n++;
      if (args[0][n] == '$')
        args[0][n] = '%';
      else if (!valid_timefmt_codes[(unsigned char) args[0][n]]) {
        safe_format(buff, bp, T("#-1 INVALID ESCAPE CODE '$%c'"),
                    args[0][n] ? args[0][n] : ' ');
        return;
      }
    }
  }
  len = strftime(s, BUFFER_LEN, args[0], ttm);
  if (len == 0) {
    /* Problem. Either the output from strftime would be over
     * BUFFER_LEN characters, or there wasn't any output at all.
     * In the former case, what's in s is indeterminate. Instead of
     * trying to figure out which of the two cases happened, just
     * return an empty string.
     */
    safe_str(T("#-1 COULDN'T FORMAT TIME"), buff, bp);
    return;
  }
  for (n = 0; n < len; n++)
    if (s[n] == '%')
      s[n] = '$';
    else if (s[n] == 0x5)
      s[n] = '%';
  safe_str(s, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_time)
{
  int utc = 0;
  int mytime;
  double tz = 0;

  mytime = mudtime;

  if (nargs == 1) {
    if (!strcasecmp("UTC", args[0])) {
      utc = 1;
    } else if (args[0] && *args[0] && is_strict_number(args[0])) {
      utc = 1;
      tz = strtod(args[0], NULL);
      if (tz < -24.0 || tz > 24.0) {
        safe_str(T("#-1 INVALID TIME ZONE"), buff, bp);
        return;
      }
      mytime += (int) (tz * 3600);
    } else if (args[0] && *args[0]) {
      dbref thing;
      ATTR *a;
      char *ptr;
      utc = 1;
      thing = match_thing(executor, args[0]);
      if (!GoodObject(thing)) {
        safe_str(T(e_notvis), buff, bp);
        return;
      }
      /* Always make time(player) return a time,
       * even if player's TZ is unset or wonky */
      a = atr_get(thing, "TZ");
      if (a && is_strict_number(ptr = atr_value(a))) {
        tz = strtod(ptr, NULL);
        if (tz >= -24.0 && tz <= 24.0) {
          mytime += (int) (tz * 3600);
        }
      }
    }
  } else if (!strcmp("UTCTIME", called_as)) {
    utc = 1;
  }

  safe_str(show_time(mytime, utc), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_secs)
{
  safe_time_t(mudtime, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_convsecs)
{
  /* converts seconds to a time string */

  time_t tt;
  struct tm *ttm;
  int utc = 0;

  if (strcmp(called_as, "CONVUTCSECS") == 0 ||
      (nargs == 2 && strcasecmp("UTC", args[1]) == 0))
    utc = 1;

  if (!is_integer(args[0])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  tt = parse_integer(args[0]);
  if (errno == ERANGE) {
    safe_str(T(e_range), buff, bp);
    return;
  }
  if (tt < 0) {
    safe_str(T(e_uint), buff, bp);
    return;
  }

  if (utc)
    ttm = gmtime(&tt);
  else
    ttm = localtime(&tt);

  safe_str(show_tm(ttm), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_etimefmt)
{
  unsigned long secs;

  if (!is_uinteger(args[1])) {
    safe_str(e_uint, buff, bp);
    return;
  }

  secs = strtoul(args[1], NULL, 10);

  do_timestring(buff, bp, args[0], secs);

}

/* ARGSUSED */
FUNCTION(fun_stringsecs)
{
  int secs;
  if (etime_to_secs(args[0], &secs))
    safe_integer(secs, buff, bp);
  else
    safe_str(T("#-1 INVALID TIMESTRING"), buff, bp);
}

/** Convert an elapsed time string (3d 2h 1m 10s) to seconds.
 * \param str1 a time string.
 * \param secs pointer to an int to fill with number of seconds.
 * \retval 1 success.
 * \retval 0 failure.
 */
int
etime_to_secs(char *str1, int *secs)
{
  /* parse the result from timestring() back into a number of seconds */
  char str2[BUFFER_LEN];
  int i;

  *secs = 0;
  while (str1 && *str1) {
    while (*str1 == ' ')
      str1++;
    i = 0;
    while (isdigit((unsigned char) *str1)) {
      str2[i] = *str1;
      str1++;
      i++;
    }
    if (i == 0) {
      return 0;                 /* No numbers given */
    }
    str2[i] = '\0';
    if (!*str1) {
      *secs += parse_integer(str2);     /* no more chars, just add seconds and stop */
      break;
    }
    switch (*str1) {
    case 'd':
    case 'D':
      *secs += (parse_integer(str2) * 86400);   /* days */
      break;
    case 'h':
    case 'H':
      *secs += (parse_integer(str2) * 3600);    /* hours */
      break;
    case 'm':
    case 'M':
      *secs += (parse_integer(str2) * 60);      /* minutes */
      break;
    case 's':
    case 'S':
    case ' ':
      *secs += parse_integer(str2);     /* seconds */
      break;
    default:
      return 0;
    }
    str1++;                     /* move past the time char */
  }
  return 1;
}

/* ARGSUSED */
FUNCTION(fun_timestring)
{
  /* Convert seconds to #d #h #m #s
   * If pad > 0, pad with 0's (i.e. 0d 0h 5m 1s)
   * If pad > 1, force all #'s to be 2 digits
   */
  unsigned int secs, pad;
  unsigned int days, hours, mins;

  if (!is_uinteger(args[0])) {
    safe_str(T(e_uints), buff, bp);
    return;
  }
  if (nargs == 1)
    pad = 0;
  else {
    if (!is_uinteger(args[1])) {
      safe_str(T(e_uints), buff, bp);
      return;
    }
    pad = parse_uinteger(args[1]);
  }

  secs = parse_uinteger(args[0]);

  days = secs / 86400;
  secs %= 86400;
  hours = secs / 3600;
  secs %= 3600;
  mins = secs / 60;
  secs %= 60;
  if (pad || (days > 0)) {
    if (pad == 2)
      safe_format(buff, bp, "%02ud %02uh %02um %02us", days, hours, mins, secs);
    else
      safe_format(buff, bp, "%ud %2uh %2um %2us", days, hours, mins, secs);
  } else if (hours > 0)
    safe_format(buff, bp, "%2uh %2um %2us", hours, mins, secs);
  else if (mins > 0)
    safe_format(buff, bp, "%2um %2us", mins, secs);
  else
    safe_format(buff, bp, "%2us", secs);
}

#ifdef HAS_GETDATE
int do_convtime_gd(const char *str, struct tm *ttm);
/** Convert a time string to a struct tm using getdate().
 * Formats for the time string are taken from the file referenced in
 * the DATEMSK environment variable.
 * \param str a time string.
 * \param ttm pointer to a struct tm to fill.
 * \retval 1 success.
 * \retval 0 failure.
 */
int
do_convtime_gd(const char *str, struct tm *ttm)
{
  /* converts time string to a struct tm. Returns 1 on success, 0 on fail.
   * Formats of the time string are taken from the file listed in the
   * DATEMSK env variable
   */
  struct tm *tc;

  tc = getdate(str);

  if (tc == NULL) {
#ifdef NEVER
    if (getdate_err <= 7)
      do_rawlog(LT_ERR, "getdate returned error code %d for %s", getdate_err,
                str);
#endif
    return 0;
  }

  memcpy(ttm, tc, sizeof(struct tm));
  ttm->tm_isdst = -1;

  return 1;
}

#endif
/* do_convtime for systems without getdate(). Will probably break if in
         a non en_US locale */
static const char *month_table[] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "May",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Oct",
  "Nov",
  "Dec",
};

/** Convert a time string to a struct tm, without getdate().
 * \param mystr a time string.
 * \param ttm pointer to a struct tm to fill.
 * \retval 1 success.
 * \retval 0 failure.
 */
int
do_convtime(const char *mystr, struct tm *ttm)
{
  /* converts time string to a struct tm. Returns 1 on success, 0 on fail.
   * Time string format is always 24 characters long, in format
   * Ddd Mmm DD HH:MM:SS YYYY
   */

  char *p, *q;
  char str[25];
  int i;

  if (strlen(mystr) != 24)
    return 0;
  strcpy(str, mystr);

  /* move over the day of week and truncate. Will always be 3 chars.
   * we don't need this, so we can ignore it.
   */
  if (!(p = strchr(str, ' ')))
    return 0;
  *p++ = '\0';
  if (strlen(str) != 3)
    return 0;

  /* get the month (3 chars), and convert it to a number */
  if (!(q = strchr(p, ' ')))
    return 0;
  *q++ = '\0';
  if (strlen(p) != 3)
    return 0;
  for (i = 0; (i < 12) && strcmp(month_table[i], p); i++) ;
  if (i == 12)                  /* not found */
    return 0;
  else
    ttm->tm_mon = i;

  /* get the day of month */
  p = q;
  while (isspace((unsigned char) *p))   /* skip leading space */
    p++;
  if (!(q = strchr(p, ' ')))
    return 0;
  *q++ = '\0';
  ttm->tm_mday = atoi(p);

  /* get hours */
  if (!(p = strchr(q, ':')))
    return 0;
  *p++ = '\0';
  ttm->tm_hour = atoi(q);

  /* get minutes */
  if (!(q = strchr(p, ':')))
    return 0;
  *q++ = '\0';
  ttm->tm_min = atoi(p);

  /* get seconds */
  if (!(p = strchr(q, ' ')))
    return 0;
  *p++ = '\0';
  ttm->tm_sec = atoi(q);

  /* get year */
  ttm->tm_year = atoi(p) - 1900;

  ttm->tm_isdst = -1;

  return 1;
}

/* ARGSUSED */
FUNCTION(fun_convtime)
{
  /* converts time string to seconds */
  struct tm ttm;
  char *tz = NULL;
  int doutc = (!strcmp(called_as, "CONVUTCTIME") ||
               (nargs > 1 && !strcmp(args[1], "utc")));

  if (do_convtime(args[0], &ttm)
#ifdef HAS_GETDATE
      || do_convtime_gd(args[0], &ttm)
#endif
    ) {
    if (doutc) {
      tz = getenv("TZ");
      /* A blank, overridden TZ forces UTC. */
#ifndef WIN32
      setenv("TZ", "", 1);
#else
      _putenv_s("TZ", "", 1);
#endif
      tzset();
    }
#ifdef SUN_OS
    safe_integer(timelocal(&ttm), buff, bp);
#else
    safe_integer(mktime(&ttm), buff, bp);
#endif                          /* SUN_OS */
    if (doutc) {
      if (tz) {
#ifndef WIN32
        setenv("TZ", tz, 1);
#else
        _putenv_s("TZ", tz, 1);
#endif
      } else {
#ifndef WIN32
        unsetenv("TZ");
#else
        _putenv_s("TZ", 0, 1);
#endif
      }
      tzset();
    }
  } else {
    safe_str("-1", buff, bp);
  }
}

#ifdef WIN32
#pragma warning( disable : 4761)        /* NJG: disable warning re conversion */
#endif
/* ARGSUSED */
FUNCTION(fun_isdaylight)
{
  struct tm *ltime;

  ltime = localtime(&mudtime);

  safe_boolean(ltime->tm_isdst > 0, buff, bp);
}

/** Convert seconds to a formatted time string.
 * \verbatim
 * Format codes:
 *       $s - Seconds. $S - Seconds, force 2 digits.
 *       $m - Minutes. $M - Minutes, force 2 digits.
 *       $h - Hours.   $H - Hours, force 2 digits.
 *       $d - Days.    $D - Days, force 2 digits.
 * $$ - Literal $.
 * \endverbatim
 * \param buff string to store the result in.
 * \param bp pointer into end of buff.
 * \param format format code string.
 * \param secs seconds to convert.
 */
void
do_timestring(char *buff, char **bp, const char *format, unsigned long secs)
{
  unsigned long days, hours, mins;
  int pad = 0;
  int width;
  const char *c;
  char *w;
  bool include_suffix, in_format_flags, even_if_0;

  days = secs / 86400;
  secs %= 86400;
  hours = secs / 3600;
  secs %= 3600;
  mins = secs / 60;
  secs %= 60;

  for (c = format; c && *c; c++) {
    if (*c == '$') {
      c++;
      width = parse_int(c, &w, 10);
      if (c == w)
        pad = 0;
      else
        pad = 1;
      if (width < 0)
        width = 0;
      else if (width >= BUFFER_LEN)
        width = BUFFER_LEN - 1;
      even_if_0 = in_format_flags = 1;
      include_suffix = 0;
      while (in_format_flags) {
        switch (*w) {
        case 'x':
        case 'X':
          include_suffix = 1;
          w++;
          break;
        case 'z':
        case 'Z':
          even_if_0 = 0;
          w++;
          break;
        case '$':
          in_format_flags = 0;
          if (pad)
            safe_format(buff, bp, "%*c", width, '$');
          else
            safe_chr('$', buff, bp);
          break;
        case 's':
          in_format_flags = 0;
          if (secs || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%*lu", width, secs);
            else
              safe_uinteger(secs, buff, bp);
            if (include_suffix)
              safe_chr('s', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        case 'S':
          in_format_flags = 0;
          if (secs || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%0*lu", width, secs);
            else
              safe_format(buff, bp, "%0lu", secs);
            if (include_suffix)
              safe_chr('s', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        case 'm':
          in_format_flags = 0;
          if (mins || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%*lu", width, mins);
            else
              safe_uinteger(mins, buff, bp);
            if (include_suffix)
              safe_chr('m', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        case 'M':
          in_format_flags = 0;
          if (mins || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%0*lu", width, mins);
            else
              safe_format(buff, bp, "%0lu", mins);
            if (include_suffix)
              safe_chr('m', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        case 'h':
          in_format_flags = 0;
          if (hours || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%*lu", width, hours);
            else
              safe_uinteger(hours, buff, bp);
            if (include_suffix)
              safe_chr('h', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        case 'H':
          in_format_flags = 0;
          if (hours || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%0*lu", width, hours);
            else
              safe_format(buff, bp, "%0lu", hours);
            if (include_suffix)
              safe_chr('h', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        case 'd':
          in_format_flags = 0;
          if (days || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%*lu", width, days);
            else
              safe_uinteger(days, buff, bp);
            if (include_suffix)
              safe_chr('d', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        case 'D':
          in_format_flags = 0;
          if (days || even_if_0) {
            if (pad)
              safe_format(buff, bp, "%0*lu", width, days);
            else
              safe_format(buff, bp, "%0lu", days);
            if (include_suffix)
              safe_chr('d', buff, bp);
          } else if (pad)
            safe_fill(' ', width + (include_suffix ? 1 : 0), buff, bp);
          break;
        default:
          in_format_flags = 0;
          safe_chr('$', buff, bp);
          for (; c != w; c++)
            safe_chr(*c, buff, bp);
          safe_chr(*c, buff, bp);
        }
      }
      c = w;
    } else
      safe_chr(*c, buff, bp);
  }
}

#ifdef WIN32
#pragma warning( default : 4761)        /* NJG: enable warning re conversion */
#endif
