/**
 * \file funtime.c
 *
 * \brief Time functions for mushcode.
 *
 *
 */

#include "copyrite.h"

#include <string.h>
#include <ctype.h>
#if  defined(__GNUC__)  && !defined(__USE_XOPEN_EXTENDED)
/* Required to get the getdate() prototype on glibc. */
#define __USE_XOPEN_EXTENDED
#endif
#include <time.h>
#include <errno.h>
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "log.h"
#include "match.h"
#include "notify.h"
#include "parse.h"
#include "strutil.h"
#include "tz.h"

int do_convtime(const char *mystr, struct tm *ttm);
void do_timestring(char *buff, char **bp, const char *format,
                   unsigned long secs);
char *etime_fmt(char *buf, int secs, int len);

extern char valid_timefmt_codes[256];

FUNCTION(fun_timefmt)
{
  char s[BUFFER_LEN];
  struct tm *ttm;
  time_t tt;
  size_t len, n;
  struct tz_result res;
  bool need_tz_reset = 0, utc = 0;

  if (!args[0] || !*args[0])
    return;                     /* No field? Bad user. */

  if (nargs >= 2 && args[1] && *args[1]) {
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

  len = arglens[0];
  for (n = 0; n < len; n++) {
    if (args[0][n] == '%')
      args[0][n] = 0x5;
    else if (args[0][n] == '$') {
      args[0][n] = '%';
      n++;
      if (args[0][n] == '$')
        args[0][n] = '%';
      else if (!valid_timefmt_codes[args[0][n]]) {
        safe_format(buff, bp, T("#-1 INVALID ESCAPE CODE '$%c'"),
                    args[0][n] ? args[0][n] : ' ');
        return;
      }
    }
  }

  if (nargs == 3 && *args[2]) {
    if (!parse_timezone_arg(args[2], tt, &res)) {
      safe_str(T("#-1 INVALID TIME ZONE"), buff, bp);
      return;
    }

    if (res.tz_utc)
      utc = 1;
    else if (res.tz_attr_missing)
      utc = 0;
    else if (res.tz_has_file) {
      save_and_set_tz(res.tz_name);
      need_tz_reset = 1;
    } else {
      utc = 1;
      tt += res.tz_offset;
    }
  }

  if (utc)
    ttm = gmtime(&tt);
  else
    ttm = localtime(&tt);

  len = strftime(s, BUFFER_LEN, args[0], ttm);
  if (len == 0) {
    /* Problem. Either the output from strftime would be over
     * BUFFER_LEN characters, or there wasn't any output at all.
     * In the former case, what's in s is indeterminate. Instead of
     * trying to figure out which of the two cases happened, just
     * return an empty string.
     */
    safe_str(T("#-1 COULDN'T FORMAT TIME"), buff, bp);
  } else {
    for (n = 0; n < len; n++)
      if (s[n] == '%')
        s[n] = '$';
      else if (s[n] == 0x5)
        s[n] = '%';
    safe_strl(s, len, buff, bp);
  }

  if (need_tz_reset)
    restore_tz();
}

/* ARGSUSED */
FUNCTION(fun_time)
{
  time_t mytime;
  int utc = 0;

  mytime = mudtime;

  if (nargs == 1) {
    struct tz_result res;
    if (!parse_timezone_arg(args[0], mudtime, &res)) {
      safe_str(T("#-1 INVALID TIME ZONE"), buff, bp);
      return;
    }
    if (res.tz_attr_missing)
      utc = 0;
    else {
      utc = 1;
      mytime += res.tz_offset;
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
  bool utc = 0;

  if (!is_integer(args[0])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  tt = parse_integer(args[0]);
  if (errno == ERANGE) {
    safe_str(T(e_range), buff, bp);
    return;
  }
#ifndef HAVE_GETDATE
  if (tt < 0) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
#endif

  if (strcmp(called_as, "CONVUTCSECS") == 0) {
    utc = 1;
  } else if (nargs == 2) {
    struct tz_result res;

    if (!parse_timezone_arg(args[1], tt, &res)) {
      safe_str(T("#-1 INVALID TIME ZONE"), buff, bp);
      return;
    }
    if (res.tz_attr_missing)
      utc = 0;
    else {
      utc = 1;
      tt += res.tz_offset;
    }
  }

  if (utc)
    ttm = gmtime(&tt);
  else
    ttm = localtime(&tt);

  safe_str(show_tm(ttm), buff, bp);
}

/** Descriptions of various time periods. */
struct timeperiods {
  char lc; /**< The lower-case letter representing the time ('h' for hours, etc) */
  char uc; /**< The upper-case letter representing the time ('H' for hours, etc) */
  int seconds; /**< Number of seconds in the time period (3600 for hours, etc) */
};

struct timeperiods TIMEPERIODS[] = {
  {'s', 'S', 1},
  {'m', 'M', 60},
  {'h', 'H', 3600},
  {'d', 'D', 86400},
  {'w', 'W', 604800},
  {'y', 'Y', 31536000},
  {'\0', '\0', 0}
};

enum {
  SECS_SECOND = 0,
  SECS_MINUTE,
  SECS_HOUR,
  SECS_DAY,
  SECS_WEEK,
  SECS_YEAR
};

static char *
squish_time(char *buf, int len)
{
  char *c;
  int slen;

  /* Eat any leading whitespace */
  while (*buf == ' ')
    buf += 1;

  /* Eat up all leading 0 entries.
   *  0y 5d -> 5d
   */
  while (*buf == '0') {
    c = strchr(buf, ' ');
    if (c) {
      while (*c == ' ')
        c += 1;
      buf = c;
    } else
      break;
  }

  /* Eat any intermediate 0 entries unless it's the only one.
   * 1d 0h 40m -> 1d 40m
   * 1d 0h -> 1d
   * 0s -> 0s
   */
  c = buf;
  do {
    char *saved;

    saved = c = strchr(c, ' ');
    if (!c)
      break;

    while (*c == ' ')
      c += 1;

    if (*c == '0') {
      char *n = strchr(c, ' ');
      if (n) {
        int nlen = strlen(n) + 1;
        memmove(saved, n, nlen);
        c = saved;
      } else {
        *saved = '\0';
        break;
      }
    }
  } while (1);

  /* If the string is too long, drop trailing entries and resulting
     whitespace until it fits. */
  for (slen = strlen(buf); slen > len; slen = strlen(buf)) {
    c = strrchr(buf, ' ');
    if (c) {
      while (c > buf && *c == ' ') {
        *c = '\0';
        c -= 1;
      }
    } else
      break;
  }

  return buf;
}

/** Format the time the player has been on for for WHO/DOING/ETC,
 * fitting as much as possible into a given length, dropping least
 * significant numbers as needed.
 *
 * \param buf buffer to use to fill.
 * \param secs the number of seconds to format
 * \param len the length of the field to fill.
 * \return pointer to start of formatted time, somewhere in buf.
 */
char *
etime_fmt(char *buf, int secs, int len)
{
  int years = 0, weeks = 0, days = 0, hours = 0, mins = 0;
  div_t r;

  if (secs >= TIMEPERIODS[SECS_YEAR].seconds) {
    r = div(secs, TIMEPERIODS[SECS_YEAR].seconds);
    years = r.quot;
    secs = r.rem;
  }

  if (secs >= TIMEPERIODS[SECS_WEEK].seconds) {
    r = div(secs, TIMEPERIODS[SECS_WEEK].seconds);
    weeks = r.quot;
    secs = r.rem;
  }

  if (secs >= TIMEPERIODS[SECS_DAY].seconds) {
    r = div(secs, TIMEPERIODS[SECS_DAY].seconds);
    days = r.quot;
    secs = r.rem;
  }

  if (secs >= TIMEPERIODS[SECS_HOUR].seconds) {
    r = div(secs, TIMEPERIODS[SECS_HOUR].seconds);
    hours = r.quot;
    secs = r.rem;
  }

  if (secs >= TIMEPERIODS[SECS_MINUTE].seconds) {
    r = div(secs, TIMEPERIODS[SECS_MINUTE].seconds);
    mins = r.quot;
    secs = r.rem;
  }

  sprintf(buf, "%2dy %2dw %2dd %2dh %2dm %2ds",
          years, weeks, days, hours, mins, secs);

  return squish_time(buf, len);
}

FUNCTION(fun_etime)
{
  int secs;
  int len;
  char tbuf[BUFFER_LEN];

  if (!is_integer(args[0])) {
    safe_str(T(e_int), buff, bp);
    return;
  }

  secs = parse_integer(args[0]);
  if (errno == ERANGE || secs < 0) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  if (nargs == 2) {
    if (!is_integer(args[1])) {
      safe_str(T(e_int), buff, bp);
      return;
    }

    len = parse_integer(args[1]);
    if (len > BUFFER_LEN - 1 || len < 0) {
      safe_str(T(e_range), buff, bp);
      return;
    }
  } else
    len = BUFFER_LEN - 1;

  safe_str(etime_fmt(tbuf, secs, len), buff, bp);
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
 * \param input a time string.
 * \param secs pointer to an int to fill with number of seconds.
 * \retval 1 success.
 * \retval 0 failure.
 */
int
etime_to_secs(char *input, int *secs)
{
  /* parse the result from timestring() back into a number of seconds */
  char *p, *errptr;
  int any = 0;
  long num;

  *secs = 0;
  if (!input || !*input)
    return 0;

  for (p = input; *p; p++) {
    while (*p && isspace(*p))
      p++;
    if (!*p)
      return any;

    num = strtol(p, &errptr, 10);
    if (errptr == p) {
      /* error */
      return 0;
    } else if (!*errptr) {
      /* Just a number of seconds */
      *secs += (int) num;
      return 1;
    } else if (isspace(*errptr)) {
      /* Number of seconds, followed by a space */
      p = errptr;
      *secs += (int) num;
      continue;
    } else {
      int i;
      p = errptr;
      for (i = 0; TIMEPERIODS[i].seconds; i++) {
        if (*p == TIMEPERIODS[i].lc || *p == TIMEPERIODS[i].uc) {
          *secs += ((int) num * TIMEPERIODS[i].seconds);
          break;
        }
      }
      if (!TIMEPERIODS[i].seconds) {
        /* Unknown time period */
        return 0;
      }
    }
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

int do_convtime_gd(const char *str, struct tm *ttm);

#ifdef HAVE_GETDATE
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
#else
int
do_convtime_gd(const char *str __attribute__((__unused__)), struct tm *ttm __attribute__((__unused__)))
{
  return 0;
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
  while (isspace(*p))           /* skip leading space */
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
  const char *tz = "";
  bool save_tz = 0;

  if (strcmp(called_as, "CONVUTCTIME") == 0) {
    save_tz = 1;
  } else if (nargs == 2) {
    struct tz_result res;
    if (strcasecmp(args[1], "utc") == 0) {
      save_tz = 1;
    } else if (parse_timezone_arg(args[1], mudtime, &res)) {
      tz = res.tz_name;
      save_tz = 1;
    } else {
      safe_str("#-1 INVALID TIME ZONE", buff, bp);
      return;
    }
  }

  if (do_convtime(args[0], &ttm)
      || do_convtime_gd(args[0], &ttm)
    ) {
    if (save_tz)
      save_and_set_tz(tz);
    safe_integer(mktime(&ttm), buff, bp);
    if (save_tz)
      restore_tz();
  } else {
    safe_str("#-1", buff, bp);
  }
}

#ifdef WIN32
#pragma warning( disable : 4761)        /* NJG: disable warning re conversion */
#endif
/* ARGSUSED */
FUNCTION(fun_isdaylight)
{
  struct tm *ltime;
  time_t when = mudtime;

  if (nargs >= 1 && args[0] && *args[0]) {
    if (!is_integer(args[0])) {
      safe_str(T(e_int), buff, bp);
      return;
    }
    when = parse_integer(args[0]);
    if (errno == ERANGE) {
      safe_str(T(e_range), buff, bp);
      return;
    }
    if (when < 0) {
      safe_str(T(e_uint), buff, bp);
      return;
    }
  }

  if (nargs == 2) {
    struct tz_result res;
    if (!parse_timezone_arg(args[1], when, &res)) {
      safe_str(T("#-1 INVALID TIME ZONE"), buff, bp);
      return;
    }
    save_and_set_tz(res.tz_name);
  }

  ltime = localtime(&when);
  safe_boolean(ltime->tm_isdst > 0, buff, bp);

  if (nargs == 2)
    restore_tz();
}

/** Convert seconds to a formatted time string.
 * \verbatim
 * Format codes:
 *       $s, $S - Seconds
 *       $m, $M - Minutes
 *       $h, $H - Hours.
 *       $d, $D - Days.
 *       $$ - Literal $.
 *   All of the above can be given as $Nx to pad to N characters wide.
 *   $Nx are padded with spaces. $NX are padded with 0's.
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
