/* A wrapper header that sets up the proper defines based on
 * options.h's MALLOC_PACKAGE
 */

#ifndef _MYMALLOC_H
#define _MYMALLOC_H

#include "compile.h"
#include "mushtype.h"

/* GC functions */
#include <gc/gc.h>

#define alloc_buf() GC_MALLOC_ATOMIC(BUFFER_LEN)

typedef struct slab slab;
slab *slab_create(const char *name, size_t item_size);
void slab_destroy(slab *);
void *slab_malloc(slab *sl, const void *hint);
void slab_free(slab *sl, void *obj);
    enum slab_options {
      SLAB_ALLOC_FIRST_FIT,
                        /**< When allocating without a hint (Or when a
                           hint page is full) , use the first page
                           found with room for the object. Default. Mutually exclusive with SLAB_ALLOC_BEST_FIT. */
      SLAB_ALLOC_BEST_FIT,
                       /**< When allocating without a hint (Or when a
                          hint page is full), use the page with the
                          fewest free objects. Mutually exclusive with SLAB_ALLOC_FIRST_FIT. */

      SLAB_ALWAYS_KEEP_A_PAGE,
                           /**< If set to 1, do not delete an empty
                             page if it is the only page allocated for
                             that slab. Defaults to 0. */

      SLAB_HINTLESS_THRESHOLD,
                          /**< The number of free objects that must
                              exist in a page for a hintless object to
                              be allocated from it. Defaults to
                              1. Raise for cases where you'll have a
                              lot of allocations using hints and
                              deletions, to improve caching -- e.g.,
                              attributes. */
      
  SLAB_NO_EXT_POINTERS /**< Objects allocated through this slab have
			  no pointers to other objects. */
};

    void slab_set_opt(slab *sl, enum slab_options opt, int val);

    struct slab_stats {
      const char *name;
      int item_size;
      int items_per_page;
      int fill_strategy;
      int page_count;
      int allocated;
      int freed;
      int min_fill;
      int max_fill;
      int full;
      int under100;
      int under75;
      int under50;
      int under25;
    };
    void slab_describe(const slab *sl, struct slab_stats *stats);

#endif                          /* _MYMALLOC_H */
