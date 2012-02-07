/**
 * \file chunk.c
 *
 * \brief Chunk memory management system
 *
 * <h3>Synopsis:</h3>
 * The chunk memory management system has three goals: to reduce overall
 * memory consumption, to improve locality of reference, and to allow
 * less-used sections of memory to be paged out to disk.  These three
 * goals are accomplished by implementing an allocation management layer
 * on top of malloc(), with significantly reduced overhead and the ability
 * to rearrange allocations to actively control fragmentation and increase
 * locality.
 *
 *
 * <h3>Basic operation:</h3>
 * The managed memory pool is divided into regions of approximately 64KB.
 * These regions contain variable-size chunks representing allocated and
 * available (free) memory.  No individual allocation may be larger than
 * will fit in a single region, and no allocation may be smaller than one
 * byte.  Each chunk has between two and four bytes of overhead (indicating
 * the used/free status, the size of the chunk, and the number of
 * dereferences for the chunk), and each region has additional overhead
 * of about 42 bytes.
 *
 * Allocations are made with the chunk_create() call, which is given
 * the size of the data, the data value to be stored, and an initial
 * dereference count to be assigned to the chunk.  Once created, the
 * value of a chunk cannot be changed; the storage is immutable.
 * chunk_create() returns an integral reference value that can be
 * used to retrieve or free the allocation.
 *
 * Allocations are accessed with the chunk_fetch(), chunk_len(), and
 * chunk_derefs() calls.  Each of these if given a reference (as
 * returned by chunk_create()), and chunk_fetch() is additionally
 * given a buffer and length to fill with the allocated value.  Both
 * chunk_fetch() and chunk_len() increment a chunk's dereference
 * count (up to the maximum of 255), which is used in migration to
 * improve locality.
 *
 * Allocations are freed with the chunk_delete() call, which also
 * requires a reference as input.
 *
 * Finally, allocations are allowed to rearrange themselves with the
 * chunk_migration() call.  chunk_migration() takes an array of
 * pointers to chunk references as input, and examines each of the
 * indicated chunks to see which need to be moved to improve the
 * distribution of allocations.  If any allocations are moved, then
 * the references to the moved allocations are updated in place
 * (hence the array of pointers to references, instead of just an
 * array of references).  Migration may be done incrementally by
 * submitting only a portion of the allocations with each call to
 * chunk_migration(); however, _all_ allocations made with chunk_create()
 * must eventually be submitted for migration in order to maintain the
 * memory pool in a non-fragmented state.
 *
 *
 * <h3>Migration:</h3>
 * Under normal conditions, extended use of this chunk allocation system
 * would lead to a significantly fragmented datastore, unless there was
 * some means to defragment the storage arena.  In the long run, this could
 * be very bad, leading to quite a mess.  Calling chunk_migration() gives
 * the allocator permission to move allocations around both to defragment
 * the arena and to improve locality of reference (by making sure that
 * all the infrequently used chunks are segregated from the chunks in
 * active use).  Of course, moving all the allocated chunks at once would
 * be a slow and painful process.  Instead, migration may be done
 * incrementally, giving permission to move a small number of chunks
 * at any one time, and spreading out the cost of defragmenting the
 * data store.
 *
 * Just because you give permission to move a chunk doesn't mean that it
 * will be moved.  The chunk may be perfectly happy where it is, with
 * no need to move it elsewhere.  Chunks are only moved when their
 * personal happiness would be improved by a move.  In general, maximizing
 * the happiness of individual chunks will improve the happiness of the
 * whole.
 *
 * There are two things that factor into a chunk's happiness.
 * The things that make a chunk unhappy are:
 * <ul>
 * <li> Having a dereference count different from the region average.
 *      The greater the difference, the more unhappy the chunk is.
 * <li> Being in a sparsely populated region.  The fewer chunks in a
 *      region, the more unhappy the chunks in it.
 * </ul>
 * Neither of these factors are absolute; both of them have different
 * weights that add into a general unhappiness for the chunk.  The lower
 * the unhappiness, the better.
 *
 * Over time and usage, the dereference counts for chunks will increase
 * and eventually reach a maximum value of 255.  (The count is limited
 * by the fact that it's stored in a single byte for each chunk.)  If
 * this is left unchecked, eventually all chunks would have a dereference
 * count of 255, and the counts would be useless for improving locality.
 * To counteract this, when the average dereference count for a certain
 * number of regions exceeds 128, the 'migration period' is incremented
 * and all chunk dereference counts are halved.  The critical number of
 * regions is determined based on the cache size and the total number of
 * regions.  If you're not using forking dumps, then period change should
 * be controlled primarily by the frequency of database dumps (which end
 * up incrementing the dereference count on all chunks, and thus all
 * regions).  Given a dump frequency of once per hour (the default), there
 * should be a period change about every 2.6 days.
 *
 *
 * <h3>Statistics:</h3>
 * The chunk memory management system keeps several statistics about
 * the allocation pool, both to maintain good operation through active
 * encouragement of locality, and to satisfy the curiosity of people
 * using the system (and its designer ;-)).  These statistics are
 * reported (in PennMUSH) through the use of the \@stats command,
 * with /chunks switch.
 *
 * \@stats/chunks generates output similar to this:
 * \verbatim
 * Chunks:         99407 allocated (   8372875 bytes,     223808 ( 2%) overhead)
 *                   74413 short     (   1530973 bytes,     148826 ( 9%) overhead)
 *                   24994 medium    (   6841902 bytes,      74982 ( 1%) overhead)
 *                       0 long      (         0 bytes,          0 ( 0%) overhead)
 *                   128 free      (   1319349 bytes,      23058 ( 1%) fragmented)
 * Regions:          147 total,       16 cached
 * Paging:        158686 out,     158554 in
 * Storage:      9628500 total (86% saturation)
 *
 * Period:             1 (   5791834 accesses so far,       1085 chunks at max)
 * Migration:     245543 moves this period
 *                  145536 slide
 *                      45 away
 *                   30719 fill exact
 *                   69243 fill inexact
 * \endverbatim
 *
 * First, the number of allocated chunks is given, along with their
 * total size and overhead.  Then, the allocated chunks are broken up
 * by size-range; short chunks (2 to 63 bytes) with two bytes of
 * overhead each, medium chunks (64 to 8191 bytes) with three bytes of
 * overhead each, and long chunks (8192 to ~64K bytes) with four bytes
 * of overhead each.  Rounding out the individual chunk statistics is
 * the number of free chunks, their total size, and the amount of
 * fragmented free space (free space not in the largest free chunk for
 * its region is considered fragmented).
 *
 * After that, the total amount of storage (in memory or on disk) used
 * is given, along with the saturation rate (where saturation is
 * indicated by what fraction of the used space is actually allocated
 * in chunks).
 *
 * Next comes statistics on regions: the number of regions in use and
 * the number held in the memory cache.  All regions not in the cache
 * are paged out to disk.  Paging statistics follow, listing the
 * number of times a region has been moved out of or into memory
 * cache.
 *
 * Finally comes statistics on migration and the migration period.
 * The period number is listed, along with the total number of
 * dereferences in the period and how many chunks have the maximum
 * dereference count of 255.  Then the amount of migration movement is
 * listed, both in total and broken up by category.  Slides occur when
 * an allocation is shifted to the other side of a neighboring free
 * space.  Away moves are made when an allocation is extremely unhappy
 * where it is, and is pushed out to somewhere else.  Fills are when
 * an allocation is moved in order to fill in a free space; the space
 * can be either exactly filled by the move, or inexactly filled
 * (leaving some remaining free space).
 *
 *
 * <h3>Histograms:</h3>
 * The chunk memory management system can also display a few
 * histograms about itself.  These histograms are reported (in PennMUSH)
 * through the use of the \@stats command, with the /regions, /freespace,
 * or /paging switches.
 *
 * All of \@stats/regions, \@stats/freespace, and \@stats/paging produce
 * histograms vs. region average dereference count.  The histograms
 * use buckets four counts wide, so all regions from 0-3 will be in
 * the first bucket, 4-7 in the second, etc., up to 252-255 in the
 * last bucket.  If the heights of the buckets are significantly
 * different, then the highest spikes will be allowed to extend off
 * the top of the histogram (with their real values labeled in
 * parenthesis next to them).
 *
 * \@stats/regions is a histogram of how many regions at each count
 * currently exist.  In a healthy game, there should be a large spike
 * at some dereference count between 64 and 128 (representing the
 * largely unused portion of the database), a lesser spike at 255
 * (representing the portion of the database that's used very frequently),
 * and a smattering of regions at other counts, with either new areas
 * of the database (below the large spike) or semi-frequently used
 * areas (above the large spike).  New migration periods occur when
 * the large spike would pass 128, at which point everything is halved
 * and the spike is pushed back down to 64.
 *
 * \@stats/freespace is a histogram of how much free space exists in
 * regions at each dereference count.  This histogram is included
 * to aid in diagnosis of the cause for dropping saturation rates.
 *
 * \@stats/paging is a histogram of the number of regions being paged
 * in or out at each dereference count.  As of this writing, a very
 * unhealthy behaviour is observed, wherein the histogram shows a
 * trapeziod between 64 and 128, drowning out most of the rest of the
 * chart.  This indicates that as time goes on, the attributes
 * associated with a single low-use object are getting scattered
 * randomly throughout all the low-use regions, and thus when dumps
 * occur (with their linear enumeration of all attributes on objects)
 * the low-use regions thrash in and out of cache.  This can be very
 * detrimental to dump performance.  Something will have to be done
 * to fix this tendency of migration.  Healthy behaviour will make
 * some other pattern in the paging histogram which has not yet been
 * determined.
 */

#include "copyrite.h"
#include "config.h"
#include "conf.h"

#define _XOPEN_SOURCE 600
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#ifdef WIN32
#include <wtypes.h>
#include <io.h>
#else
#define __USE_UNIX98
#include <unistd.h>
#endif
#include <errno.h>
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif


#include "externs.h"
#include "chunk.h"
#include "command.h"
#include "dbdefs.h"
#include "intrface.h"
#include "log.h"
#include "mymalloc.h"
#include "confmagic.h"

#ifdef WIN32
#pragma warning( disable : 4761)        /* disable warning re conversion */
#endif

#ifdef WIN32
#define PRIdS "I"
#else
#define PRIdS "t"
#endif

/* A whole bunch of debugging #defines. */
/** Basic debugging stuff - are assertions checked? */
#define CHUNK_DEBUG
/** Paranoid people check for region validity after every operation
 * that modifies a region. */
#define CHUNK_PARANOID
/** Log all moves and slides during migration. */
#undef DEBUG_CHUNK_MIGRATE
/** Log creation of regions. */
#undef DEBUG_CHUNK_REGION_CREATE
/** Log paging of regions. */
#undef DEBUG_CHUNK_PAGING
/** Log all mallocs. */
#undef DEBUG_CHUNK_MALLOC

/** For debugging, we keep a rolling log of debug messages.
 * These get dumped to disk if we're about to panic.
 */
#define ROLLING_LOG_SIZE 200
#define ROLLING_LOG_ENTRY_LEN 1024

/* debug... */
#ifdef CHUNK_DEBUG
#define ASSERT(x) assert(x)
#else                           /* CHUNK_DEBUG */
static int ignore;      /**< Used to shut up compiler warnings when not asserting */
#define ASSERT(x) ignore++
#endif                          /* CHUNK_DEBUG */

/*
 * Sizes, limits, etc.
 */
/** Region size, including header.
 * This is a little less than 64K to allow for malloc overhead without
 * spilling into next page */
#define REGION_SIZE 65500

/** Region capacity.
 * This is the size minus the fixed region overhead.
 */
#define REGION_CAPACITY (REGION_SIZE - FIRST_CHUNK_OFFSET_IN_REGION)

/** Maximum chunk length.
 * This is fairly arbitrary, but must be less than
 * REGION_CAPACITY (it must fit in a region).
 */
#define MAX_CHUNK_LEN (16384-1)

/** Number of oddballs tracked in regions.
 * This is used to figure out when we should pull regions in because
 * we have an opportunity to migrate chunks that don't match.
 * Relatively arbitrary; too low means you don't move things out
 * enough, but boosting it too high wastes memory.
 */
#define NUM_ODDBALLS 10

/** Minimum disagreement to be an oddball.
 * This is used to figure out when we should pull regions in because
 * we have an opportunity to migrate chunks that don't match.
 * Relatively arbitrary; too low means you don't move things out
 * enough, but boosting it too high wastes migration time.
 */
#define ODDBALL_THRESHOLD 8

/*
 * FIXME: pulling config variables out of my left ear. Fix later.
 */
/** How much space is initially allocated for the in-memory region array? */
#define FIXME_INIT_REGION_LEN 20
/** How much does the region array grow by each time it has to grow? */
#define FIXME_REGION_ARRAY_INCREMENT 10

/** Limit for when being a nearly-empty region counts against being
 * a good region.  This is exponential: an empty region gets a penalty
 * of 1 << LONLINESS_LIMIT.  A near-empty region gets a penalty of
 * 1 << (LONLINESS_LIMIT - used_count).
 *
 * Rationale: we don't want to reuse empty regions (or make new regions)
 * for trivialities.
 */
#define LONLINESS_LIMIT 5

/** Free space limit for when we consider making new regions.
 * The total free space must be less than this percent of capacity.
 *
 * Rationale: we don't want to waste memory with lots of extra regions.
 */
#define FREE_PERCENT_LIMIT 2

/** Bias for allocating chunks in a region that's already in memory.
 * Actually, this is a bias against allocating in swapped-out regions,
 * but that's a nit...
 *
 * Rationale: reduce the amount of paging during migration.
 */
#define IN_MEMORY_BIAS 4

/*
 *  Structures and Accessor Macros
 */
/*
 * What a chunk_reference_t looks like from the inside
 */
/** Get the region from a chunk_reference_t. */
#define ChunkReferenceToRegion(ref) ((ref) >> 16)
/** Get the offset from a chunk_reference_t. */
#define ChunkReferenceToOffset(ref) ((ref) & 0xFFFF)
/** Make a chunk_reference_t from a region and offset. */
#define ChunkReference(region, offset) \
  ((chunk_reference_t)(((region) << 16) | (offset)))

/** Sentinel value used to mark unused cache regions. */
#define INVALID_REGION_ID 0xffff

/**
 * \verbatim
 * The chunk headers look like this:
 *
 * Short:
 * byte 0     byte 1
 *  76543210   76543210
 * +--------+ +--------+
 * |ft len  | | deref  |
 * +--------+ +--------+
 *  ||\__6_/   \___8__/
 *  ||   |         `----- deref count (decays)
 *  ||   `--------------- length of data
 *  |`------------------- tag bit (0 for short)
 *  `-------------------- free flag (0 for allocated, 1 for free)
 *
 * Medium:
 * byte 0     byte 1     byte 2
 *  76543210   76543210   76543210
 * +--------+ +--------+ +--------+
 * |ftg lenM| | deref  | |  lenL  |
 * +--------+ +--------+ +--------+
 *  |||\_5_/   \___8__/   \___8__/
 *  |||  |         |          `----- length, least significant
 *  |||  |         `---------------- deref count (decays)
 *  |||  `-------------------------- length, most significant
 *  ||`----------------------------- tag bit (0 for medium)
 *  |`------------------------------ tag bit (1 for medium/long)
 *  `------------------------------- free flag
 *
 * Long:
 * byte 0     byte 1     byte 2     byte 3
 *  76543210   76543210   76543210   76543210
 * +--------+ +--------+ +--------+ +--------+
 * |ftg     | | deref  | |  lenM  | |  lenL  |
 * +--------+ +--------+ +--------+ +--------+
 *  |||        \___8__/   \_______16________/
 *  |||            |               `----------- length of data
 *  |||            `--------------------------- deref count (decays)
 *  ||`---------------------------------------- tag bit (1 for long)
 *  |`----------------------------------------- tag bit (1 for medium/long)
 *  `------------------------------------------ free flag
 *
 *
 * Note in particular that the dereference count is always in the second
 * byte of a chunk, to simplify the access logic.
 * \endverbatim
 */

/*
 * Fields in chunk headers
 */
#define CHUNK_FREE_MASK   0x80
#define CHUNK_FREE        0x80
#define CHUNK_USED        0x00

#define CHUNK_TAG1_MASK   0x40
#define CHUNK_TAG1_SHORT  0x00
#define CHUNK_TAG1_MEDIUM 0x40
#define CHUNK_TAG1_LONG   0x40
#define CHUNK_TAG1_OFFSET 0

#define CHUNK_TAG2_MASK   0x20
#define CHUNK_TAG2_MEDIUM 0x00
#define CHUNK_TAG2_LONG   0x20
#define CHUNK_TAG2_OFFSET 0

#define CHUNK_SHORT_LEN_MASK 0x3F
#define CHUNK_SHORT_LEN_OFFSET 0

#define CHUNK_MEDIUM_LEN_MSB_MASK 0x1F
#define CHUNK_MEDIUM_LEN_LSB_MASK 0xFF
#define CHUNK_MEDIUM_LEN_MSB_OFFSET 0
#define CHUNK_MEDIUM_LEN_LSB_OFFSET 2

#define CHUNK_LONG_LEN_MSB_MASK 0xFF
#define CHUNK_LONG_LEN_LSB_MASK 0xFF
#define CHUNK_LONG_LEN_MSB_OFFSET 2
#define CHUNK_LONG_LEN_LSB_OFFSET 3

#define CHUNK_DEREF_OFFSET 1

#define CHUNK_DEREF_MAX 0xFF

#define CHUNK_AGED_MASK   0x10
#define CHUNK_AGED_OFFSET 0

#define CHUNK_SHORT_DATA_OFFSET 2
#define CHUNK_MEDIUM_DATA_OFFSET 3
#define CHUNK_LONG_DATA_OFFSET 4

#define MIN_CHUNK_LEN 1
#define MIN_REMNANT_LEN (CHUNK_SHORT_DATA_OFFSET + MIN_CHUNK_LEN)

#define MAX_SHORT_CHUNK_LEN CHUNK_SHORT_LEN_MASK
#define MAX_MEDIUM_CHUNK_LEN \
        ((CHUNK_MEDIUM_LEN_MSB_MASK << 8) | CHUNK_MEDIUM_LEN_LSB_MASK)
#define MAX_LONG_CHUNK_LEN \
        (REGION_CAPACITY - CHUNK_LONG_DATA_OFFSET)

static int
LenToFullLen(int len)
{
  return (len + ((len > MAX_SHORT_CHUNK_LEN)
                 ? (len > MAX_MEDIUM_CHUNK_LEN)
                 ? CHUNK_LONG_DATA_OFFSET
                 : CHUNK_MEDIUM_DATA_OFFSET : CHUNK_SHORT_DATA_OFFSET));
}

static inline unsigned char *ChunkPointer(uint16_t, uint16_t);

/*
 * Functions for probing and manipulating chunk headers
 */
static inline uint16_t
CPLenShort(const unsigned char *cptr)
{
  return cptr[CHUNK_SHORT_LEN_OFFSET] & CHUNK_SHORT_LEN_MASK;
}

static inline uint16_t
CPLenMedium(const unsigned char *cptr)
{
  return ((cptr[CHUNK_MEDIUM_LEN_MSB_OFFSET] & CHUNK_MEDIUM_LEN_MSB_MASK) << 8)
    + (cptr[CHUNK_MEDIUM_LEN_LSB_OFFSET] & CHUNK_MEDIUM_LEN_LSB_MASK);
}

static inline uint16_t
CPLenLong(const unsigned char *cptr)
{
  return ((cptr[CHUNK_LONG_LEN_MSB_OFFSET] & CHUNK_LONG_LEN_MSB_MASK) << 8) +
    (cptr[CHUNK_LONG_LEN_LSB_OFFSET] & CHUNK_LONG_LEN_LSB_MASK);
}

static uint16_t
CPLen(const unsigned char *cptr)
{
  if (*cptr & CHUNK_TAG1_MASK) {
    if (*cptr & CHUNK_TAG2_MASK)
      return CPLenLong(cptr);
    else
      return CPLenMedium(cptr);
  } else
    return CPLenShort(cptr);
}

static inline uint16_t
ChunkLen(uint16_t region, uint16_t offset)
{
  return CPLen(ChunkPointer(region, offset));
}

static inline uint16_t
CPFullLen(const unsigned char *cptr)
{
  if (*cptr & CHUNK_TAG1_MASK) {
    if (*cptr & CHUNK_TAG2_MASK)
      return CPLenLong(cptr) + CHUNK_LONG_DATA_OFFSET;
    else
      return CPLenMedium(cptr) + CHUNK_MEDIUM_DATA_OFFSET;
  } else
    return CPLenShort(cptr) + CHUNK_SHORT_DATA_OFFSET;
}

static inline uint16_t
ChunkFullLen(uint16_t region, uint16_t offset)
{
  return CPFullLen(ChunkPointer(region, offset));
}

static inline bool
ChunkIsFree(uint16_t region, uint16_t offset)
{
  return (*ChunkPointer(region, offset) & CHUNK_FREE_MASK) == CHUNK_FREE;
}

static inline bool
ChunkIsShort(uint16_t region, uint16_t offset)
{
  return (*ChunkPointer(region, offset) & CHUNK_TAG1_MASK) == CHUNK_TAG1_SHORT;
}

static inline bool
ChunkIsMedium(uint16_t region, uint16_t offset)
{
  return (*ChunkPointer(region, offset) & (CHUNK_TAG1_MASK | CHUNK_TAG2_MASK))
    == (CHUNK_TAG1_MEDIUM | CHUNK_TAG2_MEDIUM);
}

static inline bool
ChunkIsLong(uint16_t region, uint16_t offset)
{
  return (*ChunkPointer(region, offset) & (CHUNK_TAG1_MASK | CHUNK_TAG2_MASK))
    == (CHUNK_TAG1_LONG | CHUNK_TAG2_LONG);
}


static inline uint8_t
ChunkDerefs(uint16_t region, uint16_t offset)
{
  return ChunkPointer(region, offset)[CHUNK_DEREF_OFFSET];
}

static void
SetChunkDerefs(uint16_t region, uint16_t offset, uint8_t derefs)
{
  ChunkPointer(region, offset)[CHUNK_DEREF_OFFSET] = derefs;
}

static unsigned char *
CPDataPtr(unsigned char *cptr)
{
  if (*cptr & CHUNK_TAG1_MASK) {
    if (*cptr & CHUNK_TAG2_MASK)
      return cptr + CHUNK_LONG_DATA_OFFSET;
    else
      return cptr + CHUNK_MEDIUM_DATA_OFFSET;
  } else
    return cptr + CHUNK_SHORT_DATA_OFFSET;
}

static inline unsigned char *
ChunkDataPtr(uint16_t region, uint16_t offset)
{
  return CPDataPtr(ChunkPointer(region, offset));
}

static inline uint16_t
ChunkNextFree(uint16_t region, uint16_t offset)
{
  return (ChunkDataPtr(region, offset)[0] << 8) + ChunkDerefs(region, offset);
}

/* 0 for no, 1 for yes with room, 2 for exact */
static int
FitsInSpace(int size, int capacity)
{
  if (size == capacity)
    return 2;
  else
    return size <= capacity - MIN_REMNANT_LEN;
}

/** Region info that gets paged out with its region.
 * This is at the start of the region;
 * the rest of the 64K bytes of the region contain chunks.
 */
typedef struct region_header {
  uint16_t region_id;           /**< will be INVALID_REGION_ID if not in use */
  uint16_t first_free;          /**< offset of 1st free chunk */
  struct region_header *prev;   /**< linked list prev for LRU cache */
  struct region_header *next;   /**< linked list next for LRU cache */
} RegionHeader;

#define FIRST_CHUNK_OFFSET_IN_REGION sizeof(RegionHeader)

/** In-memory (never paged) region info.  */
typedef struct region {
  uint16_t used_count;          /**< number of used chunks */
  uint16_t free_count;          /**< number of free chunks */
  uint16_t free_bytes;          /**< number of free bytes (with headers) */
  uint16_t largest_free_chunk;  /**< largest single free chunk */
  uint32_t total_derefs;        /**< total of all used chunk derefs */
  uint32_t period_last_touched; /**< "this" period, for deref counts;
                                           we don't page in regions to update
                                           counts on period change! */
  RegionHeader *in_memory;      /**< cache entry; NULL if paged out */
  uint16_t oddballs[NUM_ODDBALLS];      /**< chunk offsets with odd derefs */
} Region;


/*
 *  Globals
 */

/** Swap File */
#ifdef WIN32
typedef HANDLE fd_type;
static HANDLE swap_fd;
static HANDLE swap_fd_child = INVALID_HANDLE_VALUE;
#else
typedef int fd_type;
static int swap_fd;
static int swap_fd_child = -1;
#endif
static char child_filename[300];

/** Deref scale control.
 * When the deref counts get too big, the current period is incremented
 * and all derefs are divided by 2. */
static uint32_t curr_period;

/*
 * Info about all regions
 */
static uint32_t region_count;   /**< regions in use */
static uint32_t region_array_len;       /**< length of regions array */
static Region *regions;         /**< regions array, realloced as (rarely) needed */

/*
 * regions presently in memory
 */
static uint32_t cached_region_count;    /**< number of regions in cache */
static RegionHeader *cache_head;        /**< most recently used region */
static RegionHeader *cache_tail;        /**< least recently used region */

/*
 * statistics
 */
static int stat_used_short_count;       /**< How many short chunks? */
static int stat_used_short_bytes;       /**< How much space in short chunks? */
static int stat_used_medium_count;      /**< How many medium chunks? */
static int stat_used_medium_bytes;      /**< How much space in medium chunks? */
static int stat_used_long_count;        /**< How many long chunks? */
static int stat_used_long_bytes;        /**< How much space in long chunks? */
static int stat_deref_count;            /**< Dereferences this period */
static int stat_deref_maxxed;           /**< Number of chunks with max derefs */
/** histogram for average derefs of regions being paged in/out */
static int stat_paging_histogram[CHUNK_DEREF_MAX + 1];
static int stat_page_out;               /**< Number of page-outs */
static int stat_page_in;                /**< Number of page-ins */
static int stat_migrate_slide;          /**< Number of slide migrations */
static int stat_migrate_move;           /**< Number of move migrations */
static int stat_migrate_away;           /**< Number of chunk evictions */
static int stat_create;                 /**< Number of chunk creations */
static int stat_delete;                 /**< Number of chunk deletions */



/*
 * migration globals that are used for holding relevant data...
 */
static int m_count;             /**< The used length for the arrays. */
static chunk_reference_t **m_references; /**< The passed-in references array. */

#ifdef CHUNK_PARANOID
/** Log of recent actions for debug purposes */
static char rolling_log[ROLLING_LOG_SIZE][ROLLING_LOG_ENTRY_LEN];
static int rolling_pos;
static int noisy_log = 0;
#endif


/*
 * Forward decls
 */
static void find_oddballs(uint16_t region);

/*
 * Lookup functions
 */

static inline unsigned char *
ChunkPointer(uint16_t region, uint16_t offset)
{
  return ((unsigned char *) (regions[region].in_memory)) + offset;
}

static uint8_t
RegionDerefs(uint16_t region)
{
  if (regions[region].used_count)
    return (regions[region].total_derefs >>
            (curr_period - regions[region].period_last_touched)) /
      regions[region].used_count;
  else
    return 0;
}

static uint8_t
RegionDerefsWithChunk(uint16_t region, uint16_t derefs)
{
  return ((regions[region].total_derefs >>
           (curr_period - regions[region].period_last_touched)) + derefs) /
    (regions[region].used_count + 1);
}

/*
 * Debug routines
 */
/** Add a message to the rolling log. */
static void
debug_log(char const *format, ...)
{
#ifdef CHUNK_PARANOID
  va_list args;

  va_start(args, format);
  vsprintf(rolling_log[rolling_pos], format, args);
  va_end(args);

  rolling_log[rolling_pos][ROLLING_LOG_ENTRY_LEN - 1] = '\0';
  if (noisy_log)
    do_rawlog(LT_TRACE, "%s\n", rolling_log[rolling_pos]);
  rolling_pos = (rolling_pos + 1) % ROLLING_LOG_SIZE;
#else
  if (format)
    return;                     /* shut up the compiler warning */
#endif
}

#ifdef CHUNK_PARANOID
/** Dump the rolling log. */
static void
dump_debug_log(FILE * fp)
{
  int j;
  fputs("Recent chunk activity:\n", fp);
  j = rolling_pos;
  do {
    if (rolling_log[j][0]) {
      fputs(rolling_log[j], fp);
      fputc('\n', fp);
      rolling_log[j][0] = '\0';
    }
    j = (j + 1) % ROLLING_LOG_SIZE;
  } while (j != rolling_pos);
  fputs("End of recent chunk activity.\n", fp);
  fflush(fp);
}

/** Test if a chunk is migratable. */
static int
migratable(uint16_t region, uint16_t offset)
{
  chunk_reference_t ref = ChunkReference(region, offset);
  int j;

  for (j = 0; j < m_count; j++)
    if (m_references[j][0] == ref)
      return 1;
  return 0;
}

/** Give a detailed map of a region.
 * Lists pertinent region information, and all the chunks in the region.
 * Does not print the contents of the chunks (which would probably be
 * unreadable, anyway).
 * \param region the region to display.
 * \param fp the FILE* to output to.
 */
static void
debug_dump_region(uint16_t region, FILE * fp)
{
  Region *rp = regions + region;
  RegionHeader *rhp;
  uint16_t offset, count;

  ASSERT(region < region_count);
  rhp = rp->in_memory;

  fprintf(fp, "region: id:%04x period:%-8x deref:%-8x (%-2x per chunk)\n",
          region, (unsigned int) rp->period_last_touched, (unsigned int) rp->total_derefs,
          RegionDerefs(region));
  fprintf(fp, "        #used:%-4x #free:%-4x fbytes:%-4x hole:%-4x ",
          rp->used_count, rp->free_count, rp->free_bytes,
          rp->largest_free_chunk);
  if (rhp)
    fprintf(fp, "first:%-4x h_id:%-4x\n", rhp->first_free, rhp->region_id);
  else
    fprintf(fp, "PAGED\n");
  fflush(fp);

  if (rhp) {
    for (offset = FIRST_CHUNK_OFFSET_IN_REGION;
         offset < REGION_SIZE; offset += ChunkFullLen(region, offset)) {
      fprintf(fp, "chunk:%c%4s %-6s off:%04x full:%04x ",
              migratable(region, offset) ? '*' : ' ',
              ChunkIsFree(region, offset) ? "FREE" : "",
              ChunkIsShort(region, offset) ? "SHORT" :
              (ChunkIsMedium(region, offset) ? "MEDIUM" : "LONG"),
              offset, ChunkFullLen(region, offset));
      if (ChunkIsFree(region, offset)) {
        fprintf(fp, "next:%04x\n", ChunkNextFree(region, offset));
      } else {
        fprintf(fp, "doff:%04" PRIdS "x len:%04x ",
                ChunkDataPtr(region, offset) - (unsigned char *) rhp,
                ChunkLen(region, offset));
        count = ChunkDerefs(region, offset);
        if (count == 0xFF) {
          fprintf(fp, "deref:many\n");
        } else {
          fprintf(fp, "deref:%04x\n", count);
        }
      }
    }
  }
}

/** Make sure a chunk is real.
 * Detect bogus chunk references handed to the system.
 * \param region the region to verify.
 * \param offset the offset to verify.
 */
static void
verify_used_chunk(uint16_t region, uint16_t offset)
{
  uint16_t pos;

  ASSERT(region < region_count);

  for (pos = FIRST_CHUNK_OFFSET_IN_REGION;
       pos < REGION_SIZE; pos += ChunkFullLen(region, pos)) {
    if (pos == offset) {
      if (ChunkIsFree(region, pos))
        mush_panic("Invalid reference to free chunk as used");
      return;
    }
  }
  mush_panic("Invalid reference to non-chunk as used");
}

/** Verify that a region is sane.
 * Do a thorough consistency check on a region, verifying all the region
 * totals, making sure the counts are consistent, and that all the space
 * in the region is accounted for.
 * \param region the region to verify.
 * \return true if the region is valid.
 */
static int
region_is_valid(uint16_t region)
{
  int result;
  Region *rp;
  RegionHeader *rhp;
  int used_count;
  int free_count;
  int free_bytes;
  int largest_free;
  unsigned int total_derefs;
  int len;
  int was_free;
  int dump;
  uint16_t next_free;
  uint16_t offset;

  if (region >= region_count) {
    do_rawlog(LT_ERR, "region 0x%04x is not valid: region_count is 0x%04x",
              region, (unsigned int) region_count);
    return 0;
  }
  result = 1;

  rp = regions + region;
  if (rp->used_count > REGION_SIZE / MIN_REMNANT_LEN) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: chunk count is ludicrous: 0x%04x",
              region, rp->used_count);
    result = 0;
  }
  if (rp->free_count > REGION_SIZE / MIN_REMNANT_LEN) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: free count is ludicrous: 0x%04x",
              region, rp->free_count);
    result = 0;
  }
  if (rp->largest_free_chunk > rp->free_bytes) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: largest free chunk > free bytes:"
              " 0x%04x > 0x%04x",
              region, rp->largest_free_chunk, rp->free_bytes);
    result = 0;
  }
  if (!rp->in_memory)
    return result;

  rhp = rp->in_memory;

  if (rhp->region_id != region) {
    do_rawlog(LT_ERR, "region 0x%04x is not valid: region in cache is 0x%04x",
              region, rhp->region_id);
    result = 0;
  }
  dump = 0;
  used_count = 0;
  total_derefs = 0;
  free_count = 0;
  free_bytes = 0;
  largest_free = 0;
  was_free = 0;
  next_free = rhp->first_free;
  for (offset = FIRST_CHUNK_OFFSET_IN_REGION;
       offset < REGION_SIZE; offset += len) {
    if (was_free && ChunkIsFree(region, offset)) {
      do_rawlog(LT_ERR,
                "region 0x%04x is not valid: uncoalesced free chunk:"
                " 0x%04x (see map)", region, offset);
      result = 0;
      dump = 1;
    }
    len = ChunkFullLen(region, offset);
    was_free = ChunkIsFree(region, offset);
    if (was_free) {
      free_count++;
      free_bytes += len;
      if (largest_free < len)
        largest_free = len;
      if (next_free != offset) {
        do_rawlog(LT_ERR,
                  "region 0x%04x is not valid: free chain broken:"
                  " 0x%04x, expecting 0x%04x (see map)",
                  region, offset, next_free);
        result = 0;
        dump = 1;
      }
      next_free = ChunkNextFree(region, offset);
    } else {
      used_count++;
      total_derefs += ChunkDerefs(region, offset);
      if (ChunkIsMedium(region, offset) &&
          ChunkLen(region, offset) <= MAX_SHORT_CHUNK_LEN) {
        do_rawlog(LT_ERR,
                  "region 0x%04x is not valid: medium chunk too small:"
                  " 0x%04x (see map)", region, offset);
        result = 0;
        dump = 1;
      }
      if (ChunkIsLong(region, offset) &&
          ChunkLen(region, offset) <= MAX_MEDIUM_CHUNK_LEN) {
        do_rawlog(LT_ERR,
                  "region 0x%04x is not valid: long chunk too small:"
                  " 0x%04x (see map)", region, offset);
        result = 0;
        dump = 1;
      }
    }
  }
  if (offset != REGION_SIZE) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: last chunk past bounds (see map)",
              region);
    result = 0;
  }
  if (next_free != 0) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: free chain unterminated:"
              " expecting 0x%04x (see map)", region, next_free);
    result = 0;
    dump = 1;
  }
  if (rp->used_count != used_count) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: used count is wrong:"
              " 0x%04x should be 0x%04x", region, rp->used_count, used_count);
    result = 0;
  }
  if (rp->total_derefs != total_derefs) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: total derefs is wrong:"
              " 0x%04x should be 0x%04x",
              region, (unsigned int) rp->total_derefs, total_derefs);
    result = 0;
  }
  if (rp->free_count != free_count) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: free count is wrong:"
              " 0x%04x should be 0x%04x", region, rp->free_count, free_count);
    result = 0;
  }
  if (rp->free_bytes != free_bytes) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: free bytes is wrong:"
              " 0x%04x should be 0x%04x", region, rp->free_bytes, free_bytes);
    result = 0;
  }
  if (rp->largest_free_chunk != largest_free) {
    do_rawlog(LT_ERR,
              "region 0x%04x is not valid: largest free is wrong:"
              " 0x%04x should be 0x%04x",
              region, rp->largest_free_chunk, largest_free);
    result = 0;
  }
  if (dump) {
    debug_dump_region(region, lookup_log(LT_TRACE)->fp);
  }
  return result;
}
#endif


/*
 * Utility Routines - Chunks
 */
/** Write a used chunk.
 * \param region the region to put the chunk in.
 * \param offset the offset to put the chunk at.
 * \param full_len the length for the chunk, including headers.
 * \param data the externally supplied data for the chunk.
 * \param data_len the length of the externally supplied data.
 * \param derefs the deref count to set on the chunk.
 */
static void
write_used_chunk(uint16_t region, uint16_t offset, uint16_t full_len,
                 unsigned char const *data, uint16_t data_len, uint8_t derefs)
{
  unsigned char *cptr = ChunkPointer(region, offset);
  if (full_len <= MAX_SHORT_CHUNK_LEN + CHUNK_SHORT_DATA_OFFSET) {
    /* chunk is short */
    cptr[0] = full_len - CHUNK_SHORT_DATA_OFFSET +
      CHUNK_USED + CHUNK_TAG1_SHORT;
    cptr[CHUNK_DEREF_OFFSET] = derefs;
    memcpy(cptr + CHUNK_SHORT_DATA_OFFSET, data, data_len);
  } else if (full_len <= MAX_MEDIUM_CHUNK_LEN + CHUNK_MEDIUM_DATA_OFFSET) {
    /* chunk is medium */
    uint16_t len = full_len - CHUNK_MEDIUM_DATA_OFFSET;
    cptr[0] = (len >> 8) + CHUNK_USED + CHUNK_TAG1_MEDIUM + CHUNK_TAG2_MEDIUM;
    cptr[CHUNK_DEREF_OFFSET] = derefs;
    cptr[CHUNK_MEDIUM_LEN_LSB_OFFSET] = len & 0xff;
    memcpy(cptr + CHUNK_MEDIUM_DATA_OFFSET, data, data_len);
  } else {
    /* chunk is long */
    uint16_t len = full_len - CHUNK_LONG_DATA_OFFSET;
    cptr[0] = CHUNK_USED + CHUNK_TAG1_LONG + CHUNK_TAG2_LONG;
    cptr[CHUNK_DEREF_OFFSET] = derefs;
    cptr[CHUNK_LONG_LEN_MSB_OFFSET] = len >> 8;
    cptr[CHUNK_LONG_LEN_LSB_OFFSET] = len & 0xff;
    memcpy(cptr + CHUNK_LONG_DATA_OFFSET, data, data_len);
  }
}

/** Write a free chunk.
 * \param region the region to put the chunk in.
 * \param offset the offset to put the chunk at.
 * \param full_len the length for the chunk, including headers.
 * \param next the offset for the next free chunk.
 */
static void
write_free_chunk(uint16_t region, uint16_t offset, uint16_t full_len,
                 uint16_t next)
{
  unsigned char *cptr = ChunkPointer(region, offset);
  if (full_len <= MAX_SHORT_CHUNK_LEN + CHUNK_SHORT_DATA_OFFSET) {
    /* chunk is short */
    cptr[0] = full_len - CHUNK_SHORT_DATA_OFFSET +
      CHUNK_FREE + CHUNK_TAG1_SHORT;
    cptr[CHUNK_SHORT_DATA_OFFSET] = next >> 8;
    cptr[CHUNK_DEREF_OFFSET] = next & 0xff;
  } else if (full_len <= MAX_MEDIUM_CHUNK_LEN + CHUNK_MEDIUM_DATA_OFFSET) {
    /* chunk is medium */
    uint16_t len = full_len - CHUNK_MEDIUM_DATA_OFFSET;
    cptr[0] = (len >> 8) + CHUNK_FREE + CHUNK_TAG1_MEDIUM + CHUNK_TAG2_MEDIUM;
    cptr[CHUNK_MEDIUM_LEN_LSB_OFFSET] = len & 0xff;
    cptr[CHUNK_MEDIUM_DATA_OFFSET] = next >> 8;
    cptr[CHUNK_DEREF_OFFSET] = next & 0xff;
  } else {
    /* chunk is long */
    uint16_t len = full_len - CHUNK_LONG_DATA_OFFSET;
    cptr[0] = CHUNK_FREE + CHUNK_TAG1_LONG + CHUNK_TAG2_LONG;
    cptr[CHUNK_LONG_LEN_MSB_OFFSET] = len >> 8;
    cptr[CHUNK_LONG_LEN_LSB_OFFSET] = len & 0xff;
    cptr[CHUNK_LONG_DATA_OFFSET] = next >> 8;
    cptr[CHUNK_DEREF_OFFSET] = next & 0xff;
  }
}

/** Write the next pointer for a free chunk.
 * \param region the region of the chunk to write in.
 * \param offset the offset of the chunk to write in.
 * \param next the offset for the next free chunk.
 */
static void
write_next_free(uint16_t region, uint16_t offset, uint16_t next)
{
  unsigned char *cptr = ChunkPointer(region, offset);
  if (ChunkIsShort(region, offset)) {
    /* chunk is short */
    cptr[CHUNK_SHORT_DATA_OFFSET] = next >> 8;
    cptr[CHUNK_DEREF_OFFSET] = next & 0xff;
  } else if (ChunkIsMedium(region, offset)) {
    /* chunk is medium */
    cptr[CHUNK_MEDIUM_DATA_OFFSET] = next >> 8;
    cptr[CHUNK_DEREF_OFFSET] = next & 0xff;
  } else {
    /* chunk is long */
    cptr[CHUNK_LONG_DATA_OFFSET] = next >> 8;
    cptr[CHUNK_DEREF_OFFSET] = next & 0xff;
  }
}

/** Combine neighboring free chunks, if possible.
 * The left-hand candidate chunk is passed in.
 * \param region the region of the chunks to coalesce.
 * \param offset the offset of the left-hand chunk to coalesce.
 */
static void
coalesce_frees(uint16_t region, uint16_t offset)
{
  Region *rp = regions + region;
  uint16_t full_len, next;
  full_len = ChunkFullLen(region, offset);
  next = ChunkNextFree(region, offset);
  if (offset + full_len == next) {
    full_len += ChunkFullLen(region, next);
    next = ChunkNextFree(region, next);
    write_free_chunk(region, offset, full_len, next);
    rp->free_count--;
    if (rp->largest_free_chunk < full_len)
      rp->largest_free_chunk = full_len;
  }
}

/** Free a used chunk.
 * \param region the region of the chunk to free.
 * \param offset the offset of the chunk to free.
 */
static void
free_chunk(uint16_t region, uint16_t offset)
{
  Region *rp = regions + region;
  uint16_t full_len, left;

  full_len = ChunkFullLen(region, offset);
  rp->total_derefs -= ChunkDerefs(region, offset);
  rp->used_count--;
  rp->free_count++;
  rp->free_bytes += full_len;
  if (rp->largest_free_chunk < full_len)
    rp->largest_free_chunk = full_len;

  if (ChunkIsShort(region, offset)) {
    /* chunk is short */
    stat_used_short_count--;
    stat_used_short_bytes -= full_len;
  } else if (ChunkIsMedium(region, offset)) {
    /* chunk is medium */
    stat_used_medium_count--;
    stat_used_medium_bytes -= full_len;
  } else {
    /* chunk is long */
    stat_used_long_count--;
    stat_used_long_bytes -= full_len;
  }

  left = rp->in_memory->first_free;
  if (!left) {
    write_free_chunk(region, offset, full_len, 0);
    rp->largest_free_chunk = full_len;
    rp->in_memory->first_free = offset;
  } else if (left > offset) {
    write_free_chunk(region, offset, full_len, left);
    rp->in_memory->first_free = offset;
    left = 0;
  } else {
    uint16_t next;
    next = ChunkNextFree(region, left);
    while (next && next < offset) {
      left = next;
      next = ChunkNextFree(region, left);
    }
    write_free_chunk(region, offset, full_len, next);
    write_next_free(region, left, offset);
  }
  coalesce_frees(region, offset);
  if (left)
    coalesce_frees(region, left);
}

/** Find the largest free chunk in a region.
 * \param region the region to search for a large hole in.
 * \return the size of the largest free chunk.
 */
static uint16_t
largest_hole(uint16_t region)
{
  uint16_t size;
  uint16_t offset;
  size = 0;
  for (offset = regions[region].in_memory->first_free;
       offset; offset = ChunkNextFree(region, offset))
    if (size < ChunkFullLen(region, offset))
      size = ChunkFullLen(region, offset);
  return size;
}

/** Allocate a used chunk out of a free hole.
 * This possibly splits the hole into two chunks, and it maintains the
 * free list.  It does NOT write the used chunk; that must be done by
 * caller.
 * \param region the region to allocate in.
 * \param offset the offset of the hole to use.
 * \param full_len the length (including headers) of the space to allocate.
 * \param align the alignment to use: 0 = easiest, 1 = left, 2 = right.
 * \return the offset of the allocated space.
 */
static uint16_t
split_hole(uint16_t region, uint16_t offset, uint16_t full_len, int align)
{
  Region *rp = regions + region;
  uint16_t hole_len = ChunkFullLen(region, offset);

  rp->used_count++;
  if (full_len <= MAX_SHORT_CHUNK_LEN + CHUNK_SHORT_DATA_OFFSET) {
    /* chunk is short */
    stat_used_short_count++;
    stat_used_short_bytes += full_len;
  } else if (full_len <= MAX_MEDIUM_CHUNK_LEN + CHUNK_MEDIUM_DATA_OFFSET) {
    /* chunk is medium */
    stat_used_medium_count++;
    stat_used_medium_bytes += full_len;
  } else {
    /* chunk is long */
    stat_used_long_count++;
    stat_used_long_bytes += full_len;
  }

  if (hole_len == full_len) {
    rp->free_count--;
    rp->free_bytes -= full_len;
    if (rp->in_memory->first_free == offset)
      rp->in_memory->first_free = ChunkNextFree(region, offset);
    else {
      uint16_t hole;
      for (hole = rp->in_memory->first_free;
           hole; hole = ChunkNextFree(region, hole))
        if (ChunkNextFree(region, hole) == offset)
          break;
      ASSERT(hole);
      write_next_free(region, hole, ChunkNextFree(region, offset));
    }
    if (rp->largest_free_chunk == hole_len)
      rp->largest_free_chunk = largest_hole(region);
    return offset;
  }

  ASSERT(hole_len >= full_len + MIN_REMNANT_LEN);
  if (!align) {
    if (rp->in_memory->first_free == offset)
      align = 1;
    else
      align = 2;
  }
  if (align == 1) {
    rp->free_bytes -= full_len;
    write_free_chunk(region, offset + full_len,
                     hole_len - full_len, ChunkNextFree(region, offset));
    if (rp->in_memory->first_free == offset)
      rp->in_memory->first_free += full_len;
    else {
      uint16_t hole;
      for (hole = rp->in_memory->first_free;
           hole; hole = ChunkNextFree(region, hole))
        if (ChunkNextFree(region, hole) == offset)
          break;
      ASSERT(hole);
      write_next_free(region, hole, offset + full_len);
    }
    if (rp->largest_free_chunk == hole_len)
      rp->largest_free_chunk = largest_hole(region);
    return offset;
  } else {
    rp->free_bytes -= full_len;
    write_free_chunk(region, offset,
                     hole_len - full_len, ChunkNextFree(region, offset));
    if (rp->largest_free_chunk == hole_len)
      rp->largest_free_chunk = largest_hole(region);
    return offset + hole_len - full_len;
  }
}

/*
 *  Utility Routines - Cache
 */

/** Read a region from a file.
 * \param fd file to read from
 * \param rhp region buffer to use
 * \param region region to read
 */
static void
read_cache_region(fd_type fd, RegionHeader *rhp, uint16_t region)
{
  off_t file_offset = region * REGION_SIZE;
  int j;
  char *pos;
  size_t remaining;
  ssize_t done;

  debug_log("read_cache_region %04x", region);

#ifndef HAVE_PREAD
  /* Try to seek up to 3 times... */
  for (j = 0; j < 3; j++)
#ifdef WIN32
    if (SetFilePointer(fd, file_offset, NULL, FILE_BEGIN) == file_offset)
      break;
#else
    if (lseek(fd, file_offset, SEEK_SET) == file_offset)
      break;
#endif
  if (j >= 3)
#ifdef WIN32
    mush_panicf("chunk swap file seek, GetLastError %d", GetLastError());
#else
    mush_panicf("chunk swap file seek, errno %d: %s", errno, strerror(errno));
#endif
#endif                          /* !HAVE_PREAD */
  pos = (char *) rhp;
  remaining = REGION_SIZE;
  for (j = 0; j < 10; j++) {
#if defined(HAVE_PREAD)
    done = pread(fd, pos, remaining, file_offset);
#elif defined(WIN32) && !defined(__MINGW32__)
    if (!ReadFile(fd, pos, remaining, &done, NULL)) {
      /* nothing */
    }
#else
    done = read(fd, pos, remaining);
#endif
    if (done >= 0) {
      remaining -= done;
      pos += done;
      file_offset += done;
      if (!remaining)
        return;
    }
  }
#ifdef WIN32
  mush_panicf("chunk swap file read, %lu remaining, GetLastError %d",
              (unsigned long) remaining, GetLastError());
#else
  mush_panicf("chunk swap file read, %lu remaining, errno %d: %s",
              (unsigned long) remaining, errno, strerror(errno));
#endif
}

/** Write a region to a file.
 * \param fd file to write to
 * \param rhp region buffer to use
 * \param region region to write
 */
static void
write_cache_region(fd_type fd, RegionHeader *rhp, uint16_t region)
{
  off_t file_offset = region * REGION_SIZE;
  int j;
  char *pos;
  size_t remaining;
  ssize_t done;

  debug_log("write_cache_region %04x", region);

#ifndef HAVE_PWRITE
  /* Try to seek up to 3 times... */
  for (j = 0; j < 3; j++)
#ifdef WIN32
    if (SetFilePointer(fd, file_offset, NULL, FILE_BEGIN) == file_offset)
      break;
#else
    if (lseek(fd, file_offset, SEEK_SET) == file_offset)
      break;
#endif
  if (j >= 3)
#ifdef WIN32
    mush_panicf("chunk swap file seek, GetLastError %d", GetLastError());
#else
    mush_panicf("chunk swap file seek, errno %d: %s", errno, strerror(errno));
#endif
#endif                          /* !HAVE_PWRITE */
  pos = (char *) rhp;
  remaining = REGION_SIZE;

  /* SW: I'm not sure why Talek has this loop so many times -- the
     only recoverable way the writes should fail is if they're
     interrupted by a signal and can't be restarted for some
     reason. */
  for (j = 0; j < 10; j++) {

#if defined(HAVE_PWRITE)
    done = pwrite(fd, pos, remaining, file_offset);
#elif defined(WIN32) && !defined(__MINGW32__)
    if (!WriteFile(fd, pos, remaining, &done, NULL)) {
      /* nothing */
    }
#else
    done = write(fd, pos, remaining);
#endif
    if (done >= 0) {
      remaining -= done;
      pos += done;
      file_offset += done;
      if (!remaining)
        return;
    }
  }
#ifdef WIN32
  mush_panicf("chunk swap file write, %lu remaining, GetLastError %d",
              (unsigned long) remaining, GetLastError());
#else
  mush_panicf("chunk swap file write, %lu remaining, errno %d: %s",
              (unsigned long) remaining, errno, strerror(errno));
#endif
}

/** Update cache position to stave off recycling.
 * \param rhp the cached region to keep around.
 */
static void
touch_cache_region(RegionHeader *rhp)
{
  debug_log("touch_cache_region %04x", rhp->region_id);

  if (cache_head == rhp)
    return;
  if (cache_tail == rhp)
    cache_tail = rhp->prev;
  if (rhp->prev)
    rhp->prev->next = rhp->next;
  if (rhp->next)
    rhp->next->prev = rhp->prev;

  if (cache_head)
    cache_head->prev = rhp;
  rhp->next = cache_head;
  rhp->prev = NULL;
  cache_head = rhp;
  if (!cache_tail)
    cache_tail = rhp;
}

/** Find space in the cache.
 * This is likely to require paging out something.
 * \return a pointer to an available cache region.
 */
static RegionHeader *
find_available_cache_region(void)
{
  RegionHeader *rhp;

  debug_log("find_available_cache_region");

  if (!cache_tail ||
      cached_region_count * REGION_SIZE < (unsigned) CHUNK_CACHE_MEMORY) {
    /* first use ... normal case if empty ... so allocate space */
#ifdef DEBUG_CHUNK_MALLOC
    do_rawlog(LT_TRACE, "CHUNK: malloc()ing a cache region");
#endif
    rhp = mush_malloc(REGION_SIZE, "chunk region cache buffer");
    if (!rhp) {
      mush_panic("chunk region cache buffer allocation failure");
    }
    cached_region_count++;
    rhp->region_id = INVALID_REGION_ID;
    rhp->prev = NULL;
    rhp->next = NULL;
    return rhp;
  }
  if (cache_tail->region_id == INVALID_REGION_ID)
    return cache_tail;

  rhp = cache_tail;
  /* page the current occupant out */
  find_oddballs(rhp->region_id);
#ifdef DEBUG_CHUNK_PAGING
  do_rawlog(LT_TRACE, "CHUNK: Paging out region %04x (offset %08x)",
            rhp->region_id, (unsigned) file_offset);
#endif
  write_cache_region(swap_fd, rhp, rhp->region_id);
  /* keep statistics */
  stat_paging_histogram[RegionDerefs(rhp->region_id)]++;
  stat_page_out++;

  /* mark the paged out region as not in memory */
  regions[rhp->region_id].in_memory = NULL;
  /* mark it not in use for sanity check reasons */
  rhp->region_id = INVALID_REGION_ID;

  return rhp;
}

/** Bring a paged out region back into memory.
 * If neccessary, make room by paging another region out.
 * \param region the region to bring in.
 */
static void
bring_in_region(uint16_t region)
{
  Region *rp = regions + region;
  RegionHeader *rhp, *prev, *next;
  uint32_t offset;
  unsigned int shift;

  debug_log("bring_in_region %04x", region);

  ASSERT(region < region_count);
  if (rp->in_memory)
    return;
  rhp = find_available_cache_region();
  ASSERT(rhp->region_id == INVALID_REGION_ID);

  /* This is cheesy, but I _really_ don't want to do dual data structures */
  prev = rhp->prev;
  next = rhp->next;

  /* page it in */
#ifdef DEBUG_CHUNK_PAGING
  do_rawlog(LT_TRACE, "CHUNK: Paging in region %04x (offset %08x)",
            region, (unsigned) file_offset);
#endif
  read_cache_region(swap_fd, rhp, region);
  /* link the region to its cache entry */
  rp->in_memory = rhp;

  /* touch the cache entry */
  rhp->prev = prev;
  rhp->next = next;
  touch_cache_region(rhp);

  /* make derefs current */
  if (rp->period_last_touched != curr_period) {
    shift = curr_period - rp->period_last_touched;
    if (shift > 8) {
      rp->total_derefs = 0;
      for (offset = FIRST_CHUNK_OFFSET_IN_REGION;
           offset < REGION_SIZE; offset += ChunkFullLen(region, offset)) {
        SetChunkDerefs(region, offset, 0);
      }
    } else {
      rp->total_derefs = 0;
      for (offset = FIRST_CHUNK_OFFSET_IN_REGION;
           offset < REGION_SIZE; offset += ChunkFullLen(region, offset)) {
        if (ChunkIsFree(region, offset))
          continue;
        SetChunkDerefs(region, offset, ChunkDerefs(region, offset) >> shift);
        rp->total_derefs += ChunkDerefs(region, offset);
      }
    }
    rp->period_last_touched = curr_period;
  }

  /* keep statistics */
  stat_page_in++;
  stat_paging_histogram[RegionDerefs(region)]++;
}

/*
 * Utility Routines - Regions
 */
/** Create a new region.
 * Recycle an empty region if possible.
 * \return the region id for the new region.
 */
static uint16_t
create_region(void)
{
  uint16_t region;

  for (region = 0; region < region_count; region++)
    if (regions[region].used_count == 0)
      break;
  if (region >= region_count) {
    if (region_count >= region_array_len) {
      /* need to grow the regions array */
      region_array_len += FIXME_REGION_ARRAY_INCREMENT;
#ifdef DEBUG_CHUNK_MALLOC
      do_rawlog(LT_TRACE, "CHUNK: realloc()ing region array");
#endif
      regions = (Region *) realloc(regions, region_array_len * sizeof(Region));
      if (!regions)
        mush_panic("chunk: region array realloc failure");
    }
    region = region_count;
    region_count++;
    regions[region].in_memory = NULL;
  }

  regions[region].used_count = 0;
  regions[region].free_count = 1;
  regions[region].free_bytes = REGION_CAPACITY;
  regions[region].largest_free_chunk = regions[region].free_bytes;
  regions[region].total_derefs = 0;
  regions[region].period_last_touched = curr_period;
  if (!regions[region].in_memory)
    regions[region].in_memory = find_available_cache_region();
  regions[region].in_memory->region_id = region;
  regions[region].in_memory->first_free = FIRST_CHUNK_OFFSET_IN_REGION;
  write_free_chunk(region, FIRST_CHUNK_OFFSET_IN_REGION,
                   regions[region].free_bytes, 0);

  touch_cache_region(regions[region].in_memory);
  return region;
}

/** Find the oddball chunks in a region.
 * \param region the region to search in.
 */
static void
find_oddballs(uint16_t region)
{
  Region *rp = regions + region;
  int j, d1, d2;
  uint16_t offset, len;
  int mean;

  for (j = 0; j < NUM_ODDBALLS; j++)
    rp->oddballs[j] = 0;

  mean = RegionDerefs(region);

  for (offset = FIRST_CHUNK_OFFSET_IN_REGION;
       offset < REGION_SIZE; offset += len) {
    len = ChunkFullLen(region, offset);
    if (ChunkIsFree(region, offset))
      continue;
    d1 = abs(mean - ChunkDerefs(region, offset));
    if (d1 < ODDBALL_THRESHOLD)
      continue;
    j = NUM_ODDBALLS;
    while (j--) {
      if (!rp->oddballs[j])
        continue;
      d2 = abs(mean - ChunkDerefs(region, rp->oddballs[j]));
      if (d1 < d2)
        break;
      if (j < NUM_ODDBALLS - 1)
        rp->oddballs[j + 1] = rp->oddballs[j];
    }
    j++;
    if (j >= NUM_ODDBALLS)
      continue;
    rp->oddballs[j] = offset;
  }
}

/** Find the best region to hold a chunk.
 * This is done by going through all the known regions and getting
 * prospective unhappiness ratings if the chunk was placed there.
 * The region with the least unhappiness wins.  Note that this may
 * well be a new region, if all existing regions are either full or
 * sufficiently unhappy.
 * \param full_len the size of the chunk, including headers.
 * \param derefs the number of dereferences on the chunk.
 * \param old_region the region the chunk was in before (if any).
 * \return the region id for the least unhappy region.
 */
static uint16_t
find_best_region(uint16_t full_len, int derefs, uint16_t old_region)
{
  uint16_t best_region, region;
  int best_score, score;
  int free_bytes;
  Region *rp;

  best_region = INVALID_REGION_ID;
  best_score = INT_MAX;
  free_bytes = 0;
  for (region = 0; region < region_count; region++) {
    rp = regions + region;
    free_bytes += rp->free_bytes;
    if (!FitsInSpace(full_len, rp->largest_free_chunk) &&
        !(rp->free_count == 2 &&
          rp->free_bytes - rp->largest_free_chunk == full_len))
      continue;

    if (region == old_region)
      score = derefs - RegionDerefs(region);
    else
      score = derefs - RegionDerefsWithChunk(region, derefs);
    if (score < 0)
      score = -score;
    if (!rp->in_memory)
      score += IN_MEMORY_BIAS;
    if (rp->used_count <= LONLINESS_LIMIT)
      score += 1 << (LONLINESS_LIMIT - rp->used_count);

    if (best_score > score) {
      best_score = score;
      best_region = region;
    }
  }

  if (best_region == INVALID_REGION_ID) {
#ifdef DEBUG_CHUNK_REGION_CREATE
    do_rawlog(LT_TRACE, "find_best_region had to create region %04x", region);
#endif
    best_region = create_region();
  } else if (best_score > (1 << LONLINESS_LIMIT) + IN_MEMORY_BIAS &&
             (free_bytes * 100 / (REGION_CAPACITY * region_count)) <
             FREE_PERCENT_LIMIT) {
#ifdef DEBUG_CHUNK_REGION_CREATE
    do_rawlog(LT_TRACE, "find_best_region chose to create region %04x", region);
#endif
    best_region = create_region();
  }
  return best_region;
}

/** Find the best offset in a region to hold a chunk.
 * \param full_len the length of the chunk, including headers.
 * \param region the region to allocate in.
 * \param old_region the region the chunk was in before (if any).
 * \param old_offset the offset the chunk was at before (if any).
 */
static uint16_t
find_best_offset(uint16_t full_len, uint16_t region,
                 uint16_t old_region, uint16_t old_offset)
{
  uint16_t fits, offset;

  bring_in_region(region);

  fits = 0;
  for (offset = regions[region].in_memory->first_free; offset;
       offset = ChunkNextFree(region, offset)) {
    if (region == old_region) {
      if (offset > old_offset)
        break;
      if (offset + ChunkFullLen(region, offset) == old_offset)
        return fits ? fits : offset;
    }
    if (ChunkFullLen(region, offset) == full_len)
      return offset;
    if (!fits && ChunkFullLen(region, offset) >= full_len + MIN_REMNANT_LEN)
      fits = offset;
  }

  return fits;
}

/*
 * Utility Routines - Statistics and debugging
 */
/** Compile a histogram for the region dereferences.
 * \return histogram data for the regions.
 */
static int *
chunk_regionhist(void)
{
  static int histogram[CHUNK_DEREF_MAX + 1];
  unsigned int j;

  for (j = 0; j <= CHUNK_DEREF_MAX; j++)
    histogram[j] = 0;
  for (j = 0; j < region_count; j++) {
    histogram[RegionDerefs(j)]++;
  }
  return histogram;
}

/** Compile a histogram for the region free space.
 * \return histogram data for the free space.
 */
static int const *
chunk_freehist(void)
{
  static int histogram[CHUNK_DEREF_MAX + 1];
  unsigned int j;

  for (j = 0; j <= CHUNK_DEREF_MAX; j++)
    histogram[j] = 0;
  for (j = 0; j < region_count; j++) {
    histogram[RegionDerefs(j)] += regions[j].free_bytes;
  }
  return histogram;
}

/** Display statistics to a player, or dump them to a log
 */
#define STAT_OUT(x) \
  do { \
    s = (x); \
    if (GoodObject(player)) \
      notify(player, s); \
    else \
      do_rawlog(LT_TRACE, "%s", s); \
  } while (0)

/** Display the stats summary page.
 * \param player the player to display it to, or NOTHING to log it.
 */
static void
chunk_statistics(dbref player)
{
  const char *s;
  int overhead;
  int free_count = 0;
  int free_bytes = 0;
  int free_large = 0;
  int used_count = 0;
  int used_bytes = 0;
  uint16_t rid;

  for (rid = 0; rid < region_count; rid++) {
    free_count += regions[rid].free_count;
    free_bytes += regions[rid].free_bytes;
    free_large += regions[rid].largest_free_chunk;
    used_count += regions[rid].used_count;
  }
  used_bytes = (REGION_CAPACITY * region_count) - free_bytes;

  if (!GoodObject(player)) {
    do_rawlog(LT_TRACE, "---- Chunk statistics");
  }
  overhead = stat_used_short_count * CHUNK_SHORT_DATA_OFFSET +
    stat_used_medium_count * CHUNK_MEDIUM_DATA_OFFSET +
    stat_used_long_count * CHUNK_LONG_DATA_OFFSET;
  STAT_OUT(tprintf
           ("Chunks:    %10d allocated (%10d bytes, %10d (%2d%%) overhead)",
            used_count, used_bytes, overhead,
            used_bytes ? overhead * 100 / used_bytes : 0));
  overhead = stat_used_short_count * CHUNK_SHORT_DATA_OFFSET;
  STAT_OUT(tprintf
           ("             %10d short     (%10d bytes, %10d (%2d%%) overhead)",
            stat_used_short_count, stat_used_short_bytes, overhead,
            stat_used_short_bytes ? overhead * 100 /
            stat_used_short_bytes : 0));
  overhead = stat_used_medium_count * CHUNK_MEDIUM_DATA_OFFSET;
  STAT_OUT(tprintf
           ("             %10d medium    (%10d bytes, %10d (%2d%%) overhead)",
            stat_used_medium_count, stat_used_medium_bytes, overhead,
            stat_used_medium_bytes ? overhead * 100 /
            stat_used_medium_bytes : 0));
  overhead = stat_used_long_count * CHUNK_LONG_DATA_OFFSET;
  STAT_OUT(tprintf
           ("             %10d long      (%10d bytes, %10d (%2d%%) overhead)",
            stat_used_long_count, stat_used_long_bytes, overhead,
            stat_used_long_bytes ? overhead * 100 / stat_used_long_bytes : 0));
  STAT_OUT(tprintf
           ("           %10d free      (%10d bytes, %10d (%2d%%) fragmented)",
            free_count, free_bytes, free_bytes - free_large,
            free_bytes ? (free_bytes - free_large) * 100 / free_bytes : 0));
  overhead = region_count * REGION_SIZE + region_array_len * sizeof(Region);
  STAT_OUT(tprintf("Storage:   %10d total (%2d%% saturation)",
                   overhead, used_bytes * 100 / overhead));
  STAT_OUT(tprintf("Regions:   %10d total, %8d cached",
                   (int) region_count, (int) cached_region_count));
  STAT_OUT(tprintf("Paging:    %10d out, %10d in",
                   stat_page_out, stat_page_in));
  STAT_OUT(" ");
  STAT_OUT(tprintf("Period:    %10d (%10d accesses so far, %10d chunks at max)",
                   (int) curr_period, stat_deref_count, stat_deref_maxxed));
  STAT_OUT(tprintf("Activity:  %10d creates, %10d deletes this period",
                   stat_create, stat_delete));
  STAT_OUT(tprintf("Migration: %10d moves this period",
                   stat_migrate_slide + stat_migrate_move));
  STAT_OUT(tprintf("             %10d slide    %10d move",
                   stat_migrate_slide, stat_migrate_move));
  STAT_OUT(tprintf("             %10d in region%10d out of region",
                   stat_migrate_slide + stat_migrate_move - stat_migrate_away,
                   stat_migrate_away));
}

/** Show just the page counts.
 * \param player the player to display it to, or NOTHING to log it.
 */
static void
chunk_page_stats(dbref player)
{
  const char *s;
  STAT_OUT(tprintf("Paging:    %10d out, %10d in",
                   stat_page_out, stat_page_in));
}

/** Display the per-region stats.
 * \param player the player to display it to, or NOTHING to log it.
 */
static void
chunk_region_statistics(dbref player)
{
  uint16_t rid;
  const char *s;

  if (!GoodObject(player)) {
    do_rawlog(LT_TRACE, "---- Region statistics");
  }
  for (rid = 0; rid < region_count; rid++) {
    STAT_OUT(tprintf
             ("region:%4d  #used:%5d  #free:%5d  "
              "fbytes:%04x  largest:%04x  deref:%3d",
              rid, regions[rid].used_count, regions[rid].free_count,
              regions[rid].free_bytes, regions[rid].largest_free_chunk,
              (int) RegionDerefs(rid)));
  }
}

/** Display a histogram.
 * \param player the player to display it to, or NOTHING to log it.
 * \param histogram the histogram data to display.
 * \param legend the legend for the histogram.
 */
static void
chunk_histogram(dbref player, int const *histogram, char const *legend)
{
  const char *s;
  int j, k, max, pen, ante;
  char buffer[20][65];
  char num[16];

  max = pen = ante = 0;
  for (j = 0; j < 64; j++) {
    k = histogram[j * 4 + 0] + histogram[j * 4 + 1] +
      histogram[j * 4 + 2] + histogram[j * 4 + 3];
    if (max < k) {
      ante = pen;
      pen = max;
      max = k;
    } else if (pen < k) {
      ante = pen;
      pen = k;
    } else if (ante < k) {
      ante = k;
    }
  }
  if (ante < max / 2) {
    if (pen < max / 2 && ante >= pen / 2)
      max = pen;
    else
      max = ante;
  }
  if (max == 0)
    max = 1;
  for (j = 0; j < 20; j++) {
    for (k = 0; k < 64; k++)
      buffer[j][k] = ' ';
    buffer[j][64] = '\0';
  }
  for (j = 0; j < 64; j++) {
    k = histogram[j * 4 + 0] + histogram[j * 4 + 1] +
      histogram[j * 4 + 2] + histogram[j * 4 + 3];
    k = k * 20 / max;
    if (k >= 20)
      k = 20;
    while (k-- > 0)
      buffer[k][j] = '*';
  }
  pen = 0;
  for (j = 0; j < 64; j++) {
    k = histogram[j * 4 + 0] + histogram[j * 4 + 1] +
      histogram[j * 4 + 2] + histogram[j * 4 + 3];
    if (k > max) {
      sprintf(num, "(%d)", k);
      if (j < 32) {
        if (j < pen)
          ante = 18;
        else
          ante = 19;
        memcpy(buffer[ante] + j + 1, num, strlen(num));
        pen = j + strlen(num) + 1;
      } else {
        if (j - (int) strlen(num) < pen)
          ante = 18;
        else
          ante = 19;
        memcpy(buffer[ante] + j - strlen(num), num, strlen(num));
        pen = j;
      }
    }
  }
  STAT_OUT("");
  STAT_OUT(legend);
  STAT_OUT(tprintf("%6d |%s", max, buffer[19]));
  j = 19;
  while (j-- > 1)
    STAT_OUT(tprintf("       |%s", buffer[j]));
  STAT_OUT(tprintf("     0 |%s", buffer[0]));
  for (j = 0, k = 2; j < 64; j++, k += 4)
    buffer[0][j] = '-';
  STAT_OUT(tprintf("       +%s", buffer[0]));
  STAT_OUT(tprintf("        0%31s%32d", "|", 255));
}

#undef STAT_OUT


/*
 * Utility Routines - Migration
 */

static void
migrate_sort(void)
{
  int j, k;
  chunk_reference_t *t;

  for (j = 1; j < m_count; j++) {
    t = m_references[j];
    for (k = j; k--;) {
      if (m_references[k][0] < t[0])
        break;
      m_references[k + 1] = m_references[k];
    }
    m_references[k + 1] = t;
  }
}

/** Slide an allocated chunk over into a neighboring free space.
 * \param region the region of the free space.
 * \param offset the offset of the free space.
 * \param which the index (in the migration arrays) of the chunk to move.
 */
static void
migrate_slide(uint16_t region, uint16_t offset, int which)
{
  Region *rp = regions + region;
  uint16_t o_len, len, next, other, prev, o_off, o_oth;

  debug_log("migrate_slide %d (%08x) to %04x%04x",
            which, m_references[which][0], region, offset);

  bring_in_region(region);

  len = ChunkFullLen(region, offset);
  next = ChunkNextFree(region, offset);
  other = ChunkReferenceToOffset(m_references[which][0]);
  o_len = ChunkFullLen(region, other);

  o_off = offset;
  o_oth = other;
  if (other > offset) {
    memmove(ChunkPointer(region, offset), ChunkPointer(region, other), o_len);
#ifdef DEBUG_CHUNK_MIGRATE
    do_rawlog(LT_TRACE, "CHUNK: Sliding chunk %08x to %04x%04x",
              m_references[which][0], region, offset);
#endif
    m_references[which][0] = ChunkReference(region, offset);
    other = offset + o_len;
  } else {
    prev = offset + len - o_len;
    memmove(ChunkPointer(region, prev), ChunkPointer(region, other), o_len);
#ifdef DEBUG_CHUNK_MIGRATE
    do_rawlog(LT_TRACE, "CHUNK: Sliding chunk %08x to %04x%04x",
              m_references[which][0], region, prev);
#endif
    m_references[which][0] = ChunkReference(region, prev);
  }
  write_free_chunk(region, other, len, next);
  coalesce_frees(region, other);
  if (rp->in_memory->first_free == offset) {
    rp->in_memory->first_free = other;
  } else {
    prev = rp->in_memory->first_free;
    while (prev && ChunkNextFree(region, prev) != offset)
      prev = ChunkNextFree(region, prev);
    write_next_free(region, prev, other);
    coalesce_frees(region, prev);
  }

  stat_migrate_slide++;

#ifdef CHUNK_PARANOID
  if (!region_is_valid(region)) {
    struct log_stream *trace;
    do_rawlog(LT_TRACE, "Invalid region after migrate_slide!");
    do_rawlog(LT_TRACE, "Was moving %04x%04x to %04x%04x (became %08x)...",
              region, o_oth, region, o_off, (unsigned int) m_references[which][0]);
    do_rawlog(LT_TRACE, "Chunk length %04x into hole length %04x", o_len, len);
    trace = lookup_log(LT_TRACE);
    debug_dump_region(region, trace->fp);
    dump_debug_log(trace->fp);
    mush_panic("Invalid region after migrate_slide!");
  }
#endif
}

/** Move an allocated chunk into a free hole.
 * \param region the region of the free space.
 * \param offset the offset of the free space.
 * \param align the alignment to use: 0 = easiest, 1 = left, 2 = right.
 * \param which the index (in the migration arrays) of the chunk to move.
 */
static void
migrate_move(uint16_t region, uint16_t offset, int which, int align)
{
  Region *rp = regions + region;
  uint16_t s_reg, s_off, s_len, o_off, length;
  Region *srp;

  debug_log("migrate_move %d (%08x) to %04x%04x, alignment %d",
            which, m_references[which][0], region, offset, align);

  s_reg = ChunkReferenceToRegion(m_references[which][0]);
  s_off = ChunkReferenceToOffset(m_references[which][0]);
  srp = regions + s_reg;

  bring_in_region(region);
  if (!srp->in_memory) {
    touch_cache_region(rp->in_memory);
    bring_in_region(s_reg);
    touch_cache_region(rp->in_memory);
  }

  s_len = ChunkFullLen(s_reg, s_off);
  length = ChunkFullLen(region, offset);

  if (s_reg == region && (s_off + s_len == offset || offset + length == s_off)) {
    migrate_slide(region, offset, which);
    return;
  }
#ifdef CHUNK_PARANOID
  if (!FitsInSpace(s_len, ChunkFullLen(region, offset))) {
    dump_debug_log(lookup_log(LT_TRACE)->fp);
    mush_panicf("Trying to migrate into too small a hole: %04x into %04x!",
                s_len, length);
  }
#endif

  o_off = offset;
  offset = split_hole(region, offset, s_len, align);
  memcpy(ChunkPointer(region, offset), ChunkPointer(s_reg, s_off), s_len);
#ifdef DEBUG_CHUNK_MIGRATE
  do_rawlog(LT_TRACE, "CHUNK: moving chunk %08x to %04x%04x",
            m_references[which][0], region, offset);
#endif
  m_references[which][0] = ChunkReference(region, offset);
  rp->total_derefs += ChunkDerefs(region, offset);
  free_chunk(s_reg, s_off);

  stat_migrate_move++;

#ifdef CHUNK_PARANOID
  if (!region_is_valid(region)) {
    do_rawlog(LT_TRACE, "Invalid region after migrate_move!");
    do_rawlog(LT_TRACE, "Was moving %04x%04x to %04x%04x (became %04x%04x)...",
              s_reg, s_off, region, o_off, region, offset);
    do_rawlog(LT_TRACE, "Chunk length %04x into hole length %04x, alignment %d",
              s_len, length, align);
    debug_dump_region(region, lookup_log(LT_TRACE)->fp);
    mush_panic("Invalid region after migrate_move!");
  }
#endif
}

static void
migrate_region(uint16_t region)
{
  chunk_reference_t high, low;
  int j, derefs;
  uint16_t offset, length, best_region, best_offset;

  bring_in_region(region);

  high = ChunkReference(region, REGION_SIZE);
  low = ChunkReference(region, 0);

  for (j = 0; j < m_count; j++) {
    if (low < m_references[j][0] && m_references[j][0] < high) {
      offset = ChunkReferenceToOffset(m_references[j][0]);
      derefs = ChunkDerefs(region, offset);
      length = ChunkFullLen(region, offset);
      best_region = find_best_region(length, derefs, region);
      best_offset = find_best_offset(length, best_region, region, offset);
      if (best_offset)
        migrate_move(best_region, best_offset, j, 1);
      if (best_region != region)
        stat_migrate_away++;
    }
  }
  migrate_sort();
}



/*
 * Interface routines
 */
/** Allocate a chunk of storage.
 * \param data the data to be stored.
 * \param len the length of the data to be stored.
 * \param derefs the deref count to set on the chunk.
 * \return the chunk reference for retrieving (or deleting) the data.
 */
chunk_reference_t
chunk_create(unsigned char const *data, uint16_t len, uint8_t derefs)
{
  uint16_t full_len, region, offset;

  if (len < MIN_CHUNK_LEN || len > MAX_CHUNK_LEN)
    mush_panicf("Illegal chunk length requested: %d bytes", len);

  full_len = LenToFullLen(len);
  region = find_best_region(full_len, derefs, INVALID_REGION_ID);
  offset = find_best_offset(full_len, region, INVALID_REGION_ID, 0);
  if (!offset) {
    region = create_region();
#ifdef DEBUG_CHUNK_REGION_CREATE
    do_rawlog(LT_TRACE, "chunk_create created region %04x", region);
#endif
    offset = FIRST_CHUNK_OFFSET_IN_REGION;
  }
  offset = split_hole(region, offset, full_len, 0);
  write_used_chunk(region, offset, full_len, data, len, derefs);
  regions[region].total_derefs += derefs;
  touch_cache_region(regions[region].in_memory);
#ifdef CHUNK_PARANOID
  if (!region_is_valid(region))
    mush_panic("Invalid region after chunk_create!");
#endif
  stat_create++;
  return ChunkReference(region, offset);
}

/** Deallocate a chunk of storage.
 * \param reference the reference to the chunk to be freed.
 */
void
chunk_delete(chunk_reference_t reference)
{
  uint16_t region, offset;
  region = ChunkReferenceToRegion(reference);
  offset = ChunkReferenceToOffset(reference);
  ASSERT(region < region_count);
  bring_in_region(region);
#ifdef CHUNK_PARANOID
  verify_used_chunk(region, offset);
#endif
  free_chunk(region, offset);
  touch_cache_region(regions[region].in_memory);
#ifdef CHUNK_PARANOID
  if (!region_is_valid(region))
    mush_panic("Invalid region after chunk_delete!");
#endif
  stat_delete++;
}

/** Fetch a chunk of data.
 * If the chunk is too large to fit in the supplied buffer, then
 * the buffer will be left untouched.  The length of the data is
 * returned regardless; this can be used to resize the buffer
 * (or just as information for further processing of the data).
 * \param reference the reference to the chunk to be fetched.
 * \param buffer the buffer to put the data into.
 * \param buffer_len the length of the buffer.
 * \return the length of the data.
 */
uint16_t
chunk_fetch(chunk_reference_t reference,
            unsigned char *buffer, uint16_t buffer_len)
{
  uint16_t region, offset, len;
  region = ChunkReferenceToRegion(reference);
  offset = ChunkReferenceToOffset(reference);
  ASSERT(region < region_count);
  bring_in_region(region);
#ifdef CHUNK_PARANOID
  verify_used_chunk(region, offset);
#endif
  len = ChunkLen(region, offset);
  if (len <= buffer_len)
    memcpy(buffer, ChunkDataPtr(region, offset), len);
  touch_cache_region(regions[region].in_memory);
  stat_deref_count++;
  if (ChunkDerefs(region, offset) < CHUNK_DEREF_MAX) {
    SetChunkDerefs(region, offset, ChunkDerefs(region, offset) + 1);
    regions[region].total_derefs++;
    if (ChunkDerefs(region, offset) == CHUNK_DEREF_MAX)
      stat_deref_maxxed++;
  }
  return len;
}

/** Get the length of a chunk.
 * This is equivalent to calling chunk_fetch(reference, NULL, 0).
 * It can be used to glean the proper size for a buffer to actually
 * retrieve the data, if you're being stingy.
 * \param reference the reference to the chunk to be queried.
 * \return the length of the data.
 */
uint16_t
chunk_len(chunk_reference_t reference)
{
  return chunk_fetch(reference, NULL, 0);
}

/** Get the deref count of a chunk.
 * This can be used to preserve the deref count across database saves
 * or similar save and restore operations.
 * \param reference the reference to the chunk to be queried.
 * \return the deref count for data.
 */
unsigned char
chunk_derefs(chunk_reference_t reference)
{
  uint16_t region, offset;
  region = ChunkReferenceToRegion(reference);
  offset = ChunkReferenceToOffset(reference);
  ASSERT(region < region_count);
  bring_in_region(region);
#ifdef CHUNK_PARANOID
  verify_used_chunk(region, offset);
#endif
  return ChunkDerefs(region, offset);
}

/** Migrate allocated chunks around.
 *
 * \param count the number of chunks to move.
 * \param references an array of pointers to chunk references,
 * which will be updated in place if necessary.
 */
void
chunk_migration(int count, chunk_reference_t **references)
{
  int k, l;
  unsigned total;
  uint16_t region, offset;

  debug_log("*** chunk_migration starts, count = %d", count);

  /* Before everything, see if we need a new period. */
  total = 0;
  for (region = 0; region < region_count; region++) {
    if (RegionDerefs(region) > (CHUNK_DEREF_MAX / 2))
      total++;
  }
  if (total > cached_region_count || total > region_count / 2)
    chunk_new_period();

  m_count = count;
  m_references = references;
  migrate_sort();

  /* Go through each of the regions. */
  for (region = 0; region < region_count; region++) {
    /* Make sure we have something to migrate, in the region. */
    for (k = 0; k < m_count; k++)
      if (ChunkReferenceToRegion(m_references[k][0]) == region)
        break;
    if (k >= m_count)
      continue;

    if (!regions[region].in_memory) {
      /* If not in memory, see if we've got an oddball. */
      while (k < m_count) {
        if (ChunkReferenceToRegion(m_references[k][0]) != region)
          break;
        offset = ChunkReferenceToOffset(m_references[k][0]);
        for (l = 0; l < NUM_ODDBALLS; l++) {
          if (regions[region].oddballs[l] == offset) {
            /* Yup, have an oddball... that's worth bringing it in. */
            bring_in_region(region);
            goto do_migrate;
          }
        }
        k++;
      }
    } else {
    do_migrate:
      /* It's in memory, so migrate it. */
      migrate_region(region);
    }
  }

  m_references = NULL;
  m_count = 0;

  debug_log("*** chunk_migration ends", count);
}

/** Get the number of paged regions.
 * Since the memory allocator cannot be reliably accessed from
 * multiple processes if any of the chunks have been swapped out
 * to disk, it's useful to check on the number of paged out regions
 * before doing any forking maneuvers.
 * \return the number of regions pages out.
 */
int
chunk_num_swapped(void)
{
  int count;
  uint16_t region;
  count = 0;
  for (region = 0; region < region_count; region++)
    if (!regions[region].in_memory)
      count++;
  return count;
}

/** Initialize chunk subsystem.
 * Nothing to see here... just call it before using the subsystem.
 */
void
chunk_init(void)
{
  /* In any case, this assert should be in main code, not here */
  ASSERT(BUFFER_LEN <= MAX_LONG_CHUNK_LEN);

#ifdef WIN32
  swap_fd = CreateFile(CHUNK_SWAP_FILE, GENERIC_READ | GENERIC_WRITE,
                       0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
  if (swap_fd == INVALID_HANDLE_VALUE)
    mush_panicf("Cannot open swap file: %d", GetLastError());
#else
  swap_fd = open(CHUNK_SWAP_FILE, O_RDWR | O_TRUNC | O_CREAT, 0600);
  if (swap_fd < 0)
    mush_panicf("Cannot open swap file: %s", strerror(errno));
#endif
  curr_period = 0;

  cached_region_count = 0;
  cache_head = NULL;
  cache_tail = NULL;

#ifdef HAVE_POSIX_FALLOCATE
  /* Reserve some space for the swap file to start with. */
  if (options.chunk_swap_initial > 0)
    posix_fallocate(swap_fd, 0, (options.chunk_swap_initial * 1024));
#endif

  region_count = 0;
  region_array_len = FIXME_INIT_REGION_LEN;
#ifdef DEBUG_CHUNK_MALLOC
  do_rawlog(LT_TRACE, "CHUNK: malloc()ing initial region array");
#endif
  regions = mush_calloc(region_array_len, sizeof(Region), "chunk region list");
  if (!regions)
    mush_panic("cannot malloc space for chunk region list");

/*
  command_add("@DEBUGCHUNK", CMD_T_ANY | CMD_T_GOD, 0, 0, 0,
              switchmask("ALL BRIEF FULL"), cmd_debugchunk);
*/
  do_rawlog(LT_TRACE, "CHUNK: chunk subsystem initialized");
}

/** Report statistics.
 * Display either the statistics summary or one of the histograms.
 * \param player the player to display it to, or NOTHING to log it.
 * \param which what type of information to display.
 */
void
chunk_stats(dbref player, enum chunk_stats_type which)
{
  switch (which) {
  case CSTATS_SUMMARY:
    chunk_statistics(player);
    break;
  case CSTATS_REGIONG:
    chunk_histogram(player, chunk_regionhist(),
                    "Chart number of regions (y) vs. references (x)");
    break;
  case CSTATS_PAGINGG:
    chunk_histogram(player, stat_paging_histogram,
                    "Chart pages in/out (y) vs. references (x)");
    break;
  case CSTATS_FREESPACEG:
    chunk_histogram(player, chunk_freehist(),
                    "Chart region free space (y) vs. references (x)");
    break;
  case CSTATS_REGION:
    chunk_region_statistics(player);
    break;
  case CSTATS_PAGING:
    chunk_page_stats(player);
    break;
  }
}

/** Start a new migration period.
 * This chops all the dereference counts in half.  Since this is called
 * from migration as needed, you probably shouldn't bother calling it
 * yourself.
 */
void
chunk_new_period(void)
{
  RegionHeader *rhp;
  Region *rp;
  uint16_t region, offset;
  int shift;

#ifdef LOG_CHUNK_STATS
  /* Log stats */
  chunk_statistics(NOTHING);
#endif

  /* Reset period info */
  curr_period++;
  stat_deref_count = 0;
  stat_deref_maxxed = 0;
  stat_migrate_slide = 0;
  stat_migrate_move = 0;
  stat_migrate_away = 0;
  stat_create = 0;
  stat_delete = 0;

  /* make derefs current */
  for (rhp = cache_head; rhp; rhp = rhp->next) {
    region = rhp->region_id;
    if (region == INVALID_REGION_ID)
      continue;
    rp = regions + region;

    shift = curr_period - rp->period_last_touched;
    if (shift > 8) {
      rp->total_derefs = 0;
      for (offset = FIRST_CHUNK_OFFSET_IN_REGION;
           offset < REGION_SIZE; offset += ChunkFullLen(region, offset)) {
        SetChunkDerefs(region, offset, 0);
      }
    } else {
      rp->total_derefs = 0;
      for (offset = FIRST_CHUNK_OFFSET_IN_REGION;
           offset < REGION_SIZE; offset += ChunkFullLen(region, offset)) {
        if (ChunkIsFree(region, offset))
          continue;
        SetChunkDerefs(region, offset, ChunkDerefs(region, offset) >> shift);
        rp->total_derefs += ChunkDerefs(region, offset);
      }
    }
    rp->period_last_touched = curr_period;
  }
}

#ifndef WIN32
/** Clone the chunkswap file for forking dumps.
 * \retval 0 if unable to clone the swap file
 * \retval 1 if swap file clone succeeded
 */
int
chunk_fork_file(void)
{
  unsigned int j;
  RegionHeader *rhp, *prev, *next;

  /* abort if already cloned */
  if (swap_fd_child >= 0)
    return 0;

  j = 0;
  for (;;) {
    sprintf(child_filename, "%s.%d", CHUNK_SWAP_FILE, j);
    swap_fd_child = open(child_filename, O_RDWR | O_EXCL | O_CREAT, 0600);
    if (swap_fd_child >= 0)
      break;
    if (j >= 10)
      return 0;
    j++;
  }

#ifdef HAVE_POSIX_FALLOCATE
  /* Try to reserve all the space needed for the child's copy of the chunk file all at once. */
  {
    struct stat fsize;
    if (fstat(swap_fd, &fsize) == 0)
      posix_fallocate(swap_fd_child, 0, fsize.st_size);
  }
#endif

#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(swap_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  rhp = find_available_cache_region();
  prev = rhp->prev;
  next = rhp->next;
  for (j = 0; j < region_count; j++) {
    if (regions[j].in_memory)
      continue;

    read_cache_region(swap_fd, rhp, j);
    write_cache_region(swap_fd_child, rhp, j);
  }
  rhp->region_id = INVALID_REGION_ID;
  rhp->prev = prev;
  rhp->next = next;

#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(swap_fd, 0, 0, POSIX_FADV_RANDOM);
#endif

  return 1;
}

/** Assert that we're the parent after fork.
 */
void
chunk_fork_parent(void)
{
  if (swap_fd_child < 0)
    return;

  close(swap_fd_child);
  swap_fd_child = -1;
}

/** Assert that we're the child after fork.
 */
void
chunk_fork_child(void)
{
  if (swap_fd_child < 0)
    return;

  close(swap_fd);

#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(swap_fd_child, 0, 0, POSIX_FADV_RANDOM);
#endif

  swap_fd = swap_fd_child;
  swap_fd_child = -1;
}

/** Assert that we're done with the cloned chunkswap file.
 */
void
chunk_fork_done(void)
{
  if (swap_fd_child < 0)
    close(swap_fd);
  else
    close(swap_fd_child);

  unlink(child_filename);
  swap_fd_child = -1;
}
#endif                          /* !WIN32 */
