/**
 * \file tz.h
 *
 * \brief Header file for time zone database reading and general tz manipulation
 */
#ifndef TZ_H
#define TZ_H

#include <time.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif                          /* HAVE_STDINT_H */

struct ttinfo {
  int32_t tt_gmtoff;
  int tt_isdst;
  unsigned int tt_abbrind;
  uint8_t tt_std;
  uint8_t tt_utc;
};

struct ttleapsecs {
  time_t tt_when;
  int tt_secs;
};

struct tzinfo {
  int timecnt; /**< The size of the transitions array */
  time_t *transitions; /**< When time zone rules change */
  uint8_t *offset_indexes; /**< Indexes into offsets array */
  int typecnt; /**< The size of the offsets array */
  struct ttinfo *offsets; /**< Array of tz offsets */
  int leapcnt; /**< The size of the leapsecs array */
  struct ttleapsecs *leapsecs; /**< Leapsecond database */
  int charcnt; /** The size of the tz abbreviation array */
  char *abbrevs; /**< Array of timezone name abbreviations */
};

#define TZMAGIC "TZif"

bool is_valid_tzname(const char *name);
bool tzfile_exists(const char *name);

struct tzinfo *read_tzfile(const char *tz);
void free_tzinfo(struct tzinfo *);
int32_t offset_for_tzinfo(struct tzinfo *tz, time_t when);

/** Structure used to store information about a timezone's offset. */
struct tz_result {
  time_t tz_when; /**< The UTC time being used as a base. */
  int32_t tz_offset; /**< Offset from UTC for the base time. */
  const char *tz_name; /**< Name of the timezone in a format suitable for use with tzset() IF tz_has_file is true. */
  bool tz_has_file; /**< True if an underlying file in the zoneinfo database was found for this timezone. */
  bool tz_attr_missing; /**< True if the time zone was requested from an object without a \@TZ attribute. */
  bool tz_utc; /**< True if UTC was requested. */
};

bool parse_timezone_arg(const char *tz, time_t when, struct tz_result *);

void save_and_set_tz(const char *newzone);
void restore_tz(void);

#endif                          /* TZ_H */
