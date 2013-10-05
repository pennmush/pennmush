#ifndef _CHUNK_H_
#define _CHUNK_H_

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "mushtype.h"

#undef LOG_CHUNK_STATS

#if ATTR_STORAGE == 0

typedef uint8_t *chunk_reference_t;
#define NULL_CHUNK_REFERENCE 0

#elif ATTR_STORAGE == 1

typedef uint32_t chunk_reference_t;
#define NULL_CHUNK_REFERENCE 0

#else

#error "Invalid ATTR_STORAGE setting."

#endif

chunk_reference_t chunk_create(char const *data, uint16_t len, uint8_t derefs);
void chunk_delete(chunk_reference_t reference);
uint16_t chunk_fetch(chunk_reference_t reference,
                     char *buffer, uint16_t buffer_len);
uint16_t chunk_len(chunk_reference_t reference);
uint8_t chunk_derefs(chunk_reference_t reference);
void chunk_migration(int count, chunk_reference_t **references);
int chunk_num_swapped(void);
void chunk_init(void);
enum chunk_stats_type { CSTATS_SUMMARY, CSTATS_REGIONG, CSTATS_PAGINGG,
  CSTATS_FREESPACEG, CSTATS_REGION, CSTATS_PAGING
};
void chunk_stats(dbref player, enum chunk_stats_type which);
void chunk_new_period(void);

#ifndef WIN32
int chunk_fork_file(void);
void chunk_fork_parent(void);
void chunk_fork_child(void);
void chunk_fork_done(void);
#endif

#endif                          /* _CHUNK_H_ */
