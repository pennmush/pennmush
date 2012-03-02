/**
 * \file tz.h
 *
 * \brief Header file for time zone database reading and general tz manipulation
 */
#ifndef TZ_H
#define TZ_H

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

bool parse_timezone_arg(const char *tz, time_t when, int32_t *offset);

void save_and_set_tz(const char *newzone);
void restore_tz(void);

#endif /* TZ_H */
