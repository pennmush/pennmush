#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <pcre.h>

#if defined(HAVE_ENDIAN_H)
#include <endian.h>
#elif defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>
#elif defined(WIN32)
/* For ntohl() */
#include <Winsock2.h>
#elif defined(HAVE_ARPA_INET_H)
/* For ntohl() */
#include <arpa/inet.h>
#else
#error "No endian conversion functions available!"
#endif

#include "tz.h"
#include "attrib.h"
#include "conf.h"
#include "externs.h"
#include "log.h"
#include "mymalloc.h"
#include "parse.h"
#include "strutil.h"

#ifndef be32toh
#define be32toh(i) ntohl(i)
#endif

#ifndef be64toh
static inline int64_t
be64toh(int64_t i)
{
#ifdef WORDS_BIGENDIAN
  return i;
#else
  union {
    int64_t i64;
    int32_t i32a[2];
  } a, r;
  a.i64 = i;
  r.i32a[0] = be32toh(a.i32a[1]);
  r.i32a[1] = be32toh(a.i32a[0]);
  return r.i64;
#endif
}
#endif

static inline int32_t
decode32(int32_t i)
{
  return be32toh(i);
}

static inline int64_t
decode64(int64_t i)
{
  return be64toh(i);
}

extern const unsigned char *tables;

#ifndef TZDIR
#define TZDIR ""
#endif

/** Validates a timezone name to see if it fits the right format.
 * \param name The name of the time zone.
 * \return true or false
 */
bool
is_valid_tzname(const char *name)
{
  static pcre *re = NULL;
  static pcre_extra *extra = NULL;
  int len;
  int ovec[15];

  if (!re) {
    int erroffset;
    const char *errptr;

    re = pcre_compile("^[A-Z][\\w+-]+(?:/[A-Z][\\w+-]+)?$", 0, &errptr,
                      &erroffset, tables);

    if (!re) {
      do_rawlog(LT_ERR, "tz: Unable to compile timezone name validation RE: %s",
                errptr);
      return 0;
    }
    extra = pcre_study(re, pcre_study_flags, &errptr);
  }

  len = strlen(name);

  return pcre_exec(re, extra, name, len, 0, 0, ovec, 15) > 0;
}

/** Tests to see if a timezone actually exists in the database.
 * There are race conditions here
 * \param name The name of the time zone to test.
 * \return true or false
 */
bool
tzfile_exists(const char *name)
{
#ifdef HAVE_ZONEINFO
  struct stat info;
  char path[BUFFER_LEN];

  if (!is_valid_tzname(name))
    return 0;

  snprintf(path, sizeof path, "%s/%s", TZDIR, name);

  if (stat(path, &info) < 0)
    return 0;

  if (!S_ISREG(info.st_mode))
    return 0;

  return access(path, R_OK) == 0;
#else
  return 0;
#endif
}

#define READ_CHUNK(buf, size)                                                  \
  do {                                                                         \
    if (read(fd, (buf), (size)) != (size)) {                                   \
      do_rawlog(LT_ERR, "tz: Unable to read chunk from %s: %s\n", tzfile,      \
                strerror(errno));                                              \
      goto error;                                                              \
    }                                                                          \
  } while (0)

#define READ_CHUNKF(buf, size)                                                 \
  do {                                                                         \
    if (read(fd, (buf), (size)) != (size)) {                                   \
      do_rawlog(LT_ERR, "tz: Unable to read chunk from %s: %s\n", tzfile,      \
                strerror(errno));                                              \
      free((buf));                                                             \
      goto error;                                                              \
    }                                                                          \
  } while (0)

static struct tzinfo *
do_read_tzfile(int fd, const char *tzfile, int time_size)
{
  struct tzinfo *tz = NULL;
  int size, n;
  bool has_64bit_times = 0;
  int isstdcnt, isgmtcnt;

  {
    char magic[5] = {'\0'};

    if (read(fd, magic, 4) != 4) {
      do_rawlog(LT_ERR, "tz: Unable to read header from %s: %s.\n", tzfile,
                strerror(errno));
      goto error;
    }

    if (memcmp(magic, TZMAGIC, 4) != 0) {
      do_rawlog(LT_ERR, "tz: %s is not a valid tzfile. Wrong magic number.\n",
                tzfile);
      goto error;
    }
  }

  {
    char version[16];
    if (read(fd, version, 16) != 16) {
      do_rawlog(LT_ERR, "tz: Unable to read chunk from %s: %s\n", tzfile,
                strerror(errno));
      goto error;
    }

    /* There's a second copy of the data using 64 bit times, following
       the chunk with 32 bit times. */
    if (version[0] == '2')
      has_64bit_times = 1;
  }

  tz = mush_malloc_zero(sizeof *tz, "timezone");

  {
    int32_t counts[6];

    READ_CHUNK(counts, sizeof counts);

    isgmtcnt = decode32(counts[0]);
    isstdcnt = decode32(counts[1]);
    tz->leapcnt = decode32(counts[2]);
    tz->timecnt = decode32(counts[3]);
    tz->typecnt = decode32(counts[4]);
    tz->charcnt = decode32(counts[5]);
  }

  /* Use 64-bit time_t version on such systems. */
  if (has_64bit_times && sizeof(time_t) == 8 && time_size == 4) {
    off_t skip = 44; /* Header and sizes */

    skip += tz->timecnt * 5;
    skip += tz->typecnt * 6;
    skip += tz->charcnt;
    skip += tz->leapcnt * (4 + time_size);
    skip += isgmtcnt + isstdcnt;

    if (lseek(fd, skip, SEEK_SET) < 0) {
      do_rawlog(LT_ERR, "tz: Unable to seek to second section of %s: %s\n",
                tzfile, strerror(errno));
      goto error;
    }

    mush_free(tz, "timezone");
    return do_read_tzfile(fd, tzfile, 8);
  }
#define READ_TRANSITIONS(type, decode)                                         \
  do {                                                                         \
    type *buf;                                                                 \
                                                                               \
    size = tz->timecnt * time_size;                                            \
    buf = malloc(size);                                                        \
    READ_CHUNKF(buf, size);                                                    \
                                                                               \
    tz->transitions = calloc(tz->timecnt, sizeof(time_t));                     \
    for (n = 0; n < tz->timecnt; n += 1)                                       \
      tz->transitions[n] = (time_t) decode(buf[n]);                            \
                                                                               \
    free(buf);                                                                 \
  } while (0)

  if (time_size == 4) {
    READ_TRANSITIONS(int32_t, decode32);
  } else {
    READ_TRANSITIONS(int64_t, decode64);
  }

  tz->offset_indexes = malloc(tz->timecnt);
  READ_CHUNK(tz->offset_indexes, tz->timecnt);

  {
    uint8_t *buf;
    int m, offsize = tz->typecnt * 6;

    buf = malloc(offsize);
    READ_CHUNKF(buf, offsize);

    tz->offsets = calloc(tz->typecnt, sizeof(struct ttinfo));

    for (n = 0, m = 0; n < tz->typecnt; n += 1, m += 6) {
      int32_t gmtoff;

      memcpy(&gmtoff, &buf[m], 4);
      tz->offsets[n].tt_gmtoff = decode32(gmtoff);
      tz->offsets[n].tt_isdst = buf[m + 4];
      tz->offsets[n].tt_abbrind = buf[m + 5];
      tz->offsets[n].tt_std = tz->offsets[n].tt_utc = 0;
    }

    free(buf);
  }

  tz->abbrevs = malloc(tz->charcnt);
  READ_CHUNK(tz->abbrevs, tz->charcnt);

#define READ_LEAPSECS(type, decode)                                            \
  do {                                                                         \
    type *buf;                                                                 \
    int m, lpsize = tz->leapcnt * (4 + time_size);                             \
                                                                               \
    buf = malloc(lpsize);                                                      \
    READ_CHUNKF(buf, lpsize);                                                  \
                                                                               \
    tz->leapsecs = calloc(tz->leapcnt, sizeof(struct ttleapsecs));             \
                                                                               \
    for (n = 0, m = 0; n < tz->leapcnt; n += 1, m += (4 + time_size)) {        \
      type when;                                                               \
      int32_t secs;                                                            \
                                                                               \
      memcpy(&when, buf, time_size);                                           \
      memcpy(&secs, buf + time_size, 4);                                       \
      tz->leapsecs[n].tt_when = (time_t) decode(when);                         \
      tz->leapsecs[n].tt_secs = decode32(secs);                                \
    }                                                                          \
    free(buf);                                                                 \
  } while (0)

  if (tz->leapcnt) {
    if (time_size == 4)
      READ_LEAPSECS(int32_t, decode32);
    else
      READ_LEAPSECS(int64_t, decode64);
  }

  {
    uint8_t *buf;
    int i;

    buf = malloc(isstdcnt);
    READ_CHUNKF(buf, isstdcnt);

    for (i = 0; i < isstdcnt; i += 1)
      tz->offsets[i].tt_std = buf[i];

    free(buf);

    buf = malloc(isgmtcnt);
    READ_CHUNKF(buf, isgmtcnt);

    for (i = 0; i < isgmtcnt; i += 1)
      tz->offsets[i].tt_utc = buf[i];

    free(buf);
  }

  return tz;

error:
  if (tz)
    free_tzinfo(tz);
  return NULL;
}

/** Parse a time zone description file. */
struct tzinfo *
read_tzfile(const char *tzname)
{
  char tzfile[BUFFER_LEN];
  int fd;
  struct tzinfo *tz;

  if (!is_valid_tzname(tzname))
    return NULL;

  snprintf(tzfile, BUFFER_LEN, "%s/%s", TZDIR, tzname);

  if ((fd = open(tzfile, O_RDONLY)) < 0) {
    if (errno != ENOENT)
      do_rawlog(LT_ERR, "tz: Unable to open %s: %s\n", tzfile, strerror(errno));
    return NULL;
  }

  tz = do_read_tzfile(fd, tzfile, 4);

  close(fd);

  return tz;
}

/** Free a time zone description struct. */
void
free_tzinfo(struct tzinfo *tz)
{
  free(tz->transitions);
  free(tz->offset_indexes);
  free(tz->offsets);
  free(tz->leapsecs);
  free(tz->abbrevs);
  mush_free(tz, "timezone");
}

/** Given a time zone struct and a time, return the offset in seconds from GMT
 * at that time.
 * \param tz the time zone description struct
 * \param when the time to calculate the offset for.
 * \return the offset in seconds.
 */
int32_t
offset_for_tzinfo(struct tzinfo *tz, time_t when)
{
  int n;

  if (tz->timecnt == 0 || when < tz->transitions[0]) {
    for (n = 0; n < tz->typecnt; n += 1)
      if (!tz->offsets[n].tt_isdst)
        return tz->offsets[n].tt_gmtoff;

    return tz->offsets[0].tt_gmtoff;
  }

  for (n = 0; n < tz->timecnt - 1; n += 1)
    if (when >= tz->transitions[n] && when < tz->transitions[n + 1])
      return tz->offsets[tz->offset_indexes[n]].tt_gmtoff;

  return tz->offsets[tz->offset_indexes[n]].tt_gmtoff;
}

/** Parse a softcode timezone request.
 *
 * \verbatim
 *
 * If arg is a objid, look up that object's @TZ attribute and parse
 * that. Otherwise, parse arg.
 *
 * If an object doesn't have a @TZ set, offset is set to 0 and tznotset to 1, to
 * be able to tell
 * that case apart from a UTC timezone.
 *
 * If a timezone database is present, try to read the given zone from
 * it. Integers are treated as 'Etc/GMT[+-]N' first.
 *
 * If no tzinfo database, or reading the given zone from one fails,
 * and the arg is an integer, treat it as the number of hours
 * difference from GMT.  Otherwise fail.
 *
 * \endverbatim
 *
 * \param arg The string to parse for a dbref, number or symbolic tz name
 * \param when When to calculate the offset for.
 * \param res Structure to store the parsed results in.
 * \return 1 for success, 0 for failure in parsing the time zone.
 */
bool
parse_timezone_arg(const char *arg, time_t when, struct tz_result *res)
{
  if (!res)
    return 0;

  memset(res, 0, sizeof *res);
  res->tz_when = when;

  if (strcasecmp(arg, "UTC") == 0) {
    res->tz_utc = 1;
    return 1;
  } else if (is_objid(arg)) {
    ATTR *a;
    dbref thing = parse_objid(arg);

    if (!RealGoodObject(thing))
      return 0;

    a = atr_get(thing, "TZ");
    if (!a) {
      /* No timezone attribute isn't an error. Just use the server's
         zone. */
      res->tz_attr_missing = 1;
      return 1;
    }

    arg = atr_value(a);
  }
#ifdef HAVE_ZONEINFO
  {
    struct tzinfo *tz = NULL;
    static char tz_path[BUFFER_LEN];

    if (is_valid_tzname(arg)) {
      tz = read_tzfile(arg);
      snprintf(tz_path, sizeof tz_path, ":%s", arg);
    } else if (is_strict_integer(arg)) {
      int offset;
      char tzname[100];

      offset = parse_integer(arg);

      /* GMT-8 is 8 hours ahead, GMT+8 is 8 hours behind, which makes
         no sense to me. */
      offset = -offset;

      snprintf(tzname, sizeof tzname, "Etc/GMT%+d", offset);
      tz = read_tzfile(tzname);
      snprintf(tz_path, sizeof tz_path, ":%s", tzname);
    }

    if (tz) {
      res->tz_offset = offset_for_tzinfo(tz, when);
      free_tzinfo(tz);
      res->tz_name = tz_path;
      res->tz_has_file = 1;
      return 1;
    }
    /* Fall through to gross numeric offset on failure */
  }
#endif

  if (is_strict_number(arg)) {
    double n = parse_number(arg);

    if (fabs(n) >= 24.0)
      return 0;

    res->tz_offset = floor(n * 3600.0);
    return 1;
  }

  return 0;
}

static char *saved_tz = NULL;

/** Save the current timezone (The TZ environment variable) and set a new one.
 * \param newzone The new timezone.
 */
void
save_and_set_tz(const char *newzone)
{
  const char *tz;

  if (!newzone)
    newzone = "";

  tz = getenv("TZ");
  if (tz)
    saved_tz = mush_strdup(tz, "timezone");
  else
    saved_tz = NULL;

#ifdef WIN32
  _putenv_s("TZ", newzone);
#else
  setenv("TZ", newzone, 1);
#endif

  tzset();
}

/** Restore the previously saved timezone. */
void
restore_tz(void)
{
  if (saved_tz) {
#ifdef WIN32
    _putenv_s("TZ", saved_tz);
#else
    setenv("TZ", saved_tz, 1);
#endif
    mush_free(saved_tz, "timezone");
    saved_tz = NULL;
  } else {
#ifdef WIN32
    _putenv_s("TZ", "");
#else
    unsetenv("TZ");
#endif
  }

  tzset();
}
