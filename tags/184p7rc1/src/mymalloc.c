/**
 * \file mymalloc.c
 *
 * \brief Malloc wrapper file.
 *
 * Three things in this file:
 *
 * -# It includes the body of csrimalloc.c if using a MALLOC_PACKAGE
 *     of 2 or 3.
 *
 * -# It has the mush_FOO() wrapper functions for
 *     malloc()/calloc()/realloc()/free(). These are used to keep
 *     track of reference counts when the mem_check config option is
 *     turned on in mush.cnf. Use them instead of plain
 *     malloc()/free()/etc.
 *
 *  -# It has the slab allocator.  The slab allocator should be used
 *     for small, frequently allocated fixed-size objects. (Structs,
 *     but not strings, basically), to cut down on system malloc
 *     overhead. Each time you allocate an object, a bit more space
 *     than what you requested is used -- there are some bytes before
 *     or after (Or both) what you can use that the malloc system uses
 *     to keep track of important things. With lots of small objects,
 *     this adds to a lot of bytes. A slab allocator knows that it
 *     will only be dealing with objects of a fixed size, so it can be
 *     more intelligent but less general-purpose and use a lot less
 *     overhead.
 *
 */
#include "config.h"
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#include "options.h"
#include "conf.h"
#include "dbdefs.h"
#include "log.h"
#include "externs.h"
#include "getpgsiz.h"
#include "mymalloc.h"
#include "confmagic.h"

/** A malloc wrapper that tracks type of allocation.
 * This should be used in preference to malloc() when possible,
 * to enable memory leak tracing with MEM_CHECK.
 * \param bytes bytes to allocate.
 * \param check string to label allocation with.
 * \return allocated block of memory or NULL.
 */
void *
mush_malloc(size_t bytes, const char *check)
{
  void *ptr;
  ptr = malloc(bytes);
  if (!ptr)
    do_rawlog(LT_TRACE, "mush_malloc failed to malloc %lu bytes for %s",
              (unsigned long) bytes, check);
  add_check(check);
  return ptr;
}

/** A calloc wrapper that tracks type of allocation.
 * Use in preference to calloc() when possible to enable
 * memory leak checking.
 * \param count number of elements to allocate
 * \param size size of each element
 * \param check string to label allocation with
 * \return allocated zeroed out block or NULL
 */
void *
mush_calloc(size_t count, size_t size, const char *check)
{
  void *ptr;

  ptr = calloc(count, size);
  if (!ptr)
    do_rawlog(LT_TRACE, "mush_calloc failed to allocate %lu bytes for %s",
              (unsigned long) (size * count), check);
  add_check(check);
  return ptr;
}

/** A realloc wrapper that tracks type of allocation.
 * Use in preference to realloc() when possible to enable
 * memory leak checking.
 * \param ptr pointer to resize
 * \param newsize what to resize it to
 * \param check string to label with
 * \param filename file name it was called from
 * \param line line it called from
 */
void *
mush_realloc_where(void *restrict ptr, size_t newsize,
                   const char *restrict check, const char *restrict filename,
                   int line)
{
  void *newptr;

  newptr = realloc(ptr, newsize);

  if (!ptr)
    add_check(check);
  else if (newsize == 0)
    del_check(check, filename, line);

  return newptr;
}

/** A free wrapper that tracks type of allocation.
 * If mush_malloc() gets the memory, mush_free() should free it
 * to enable memory leak tracing with MEM_CHECK.
 * \param ptr pointer to block of member to free.
 * \param check string to label allocation with.
 * \param filename file name it was called from
 * \param line line it called from
 */
void
mush_free_where(void *restrict ptr, const char *restrict check,
                const char *restrict filename, int line)
{
  del_check(check, filename, line);
  free(ptr);
  return;
}

/** Turn on lots of noisy debug info in log/trace.log */
/* #define SLAB_DEBUG */

/* Slab allocator functions */
/** A struct that represents an unallocated object */
struct slab_page_list {
  struct slab_page_list *next;
};

/** A struct that represents one page's worth of objects */
struct slab_page {
  int nalloced; /**< Number of objects allocated from this page */
  int nfree; /**< Number of objects on this page's free list */
  void *last_obj; /**< Pointer to last object in page. */
  struct slab_page *next; /**< Pointer to next allocated page */
  struct slab_page_list *freelist; /**< Pointer to list of unallocated objects */
};

/** Struct for a slab allocator */
struct slab {
  char name[64]; /**< Name of the slab */
  int item_size; /**< Size of the objects this slab returns */
  int items_per_page; /**< Number of objects that fit into a page */
  ptrdiff_t data_offset; /**< offset from start of the page where objects
                      are allocated from. */
  bool fill_strategy; /**< How to find empty nodes? true for
                         FIRST_FIT, false for BEST_FIT. */
  bool keep_last_empty; /**< False if empty pages are always deleted,
                           true to keep an empty page if it is the
                           only allocated page. */
  int hintless_threshold; /**< See documentation for
                             SLAB_HINTLESS_THRESHOLD option */
  struct slab_page *slabs; /**< Pointer to the head of the list of
                              allocated pages. */
};


/** Create a new slab allocator.
 * \param name The name of the allocator
 * \param item_size The size of objects to be allocated. It must be smaller
 * than the size of a VM page (Usually 4 or 8 k)
 * \return a pointer to the new slab allocator.
 */
slab *
slab_create(const char *name, size_t item_size)
{
  struct slab *sl;
  size_t pgsize, offset;

  sl = malloc(sizeof(struct slab));
  pgsize = getpagesize();
  offset = sizeof(struct slab_page);
  /* Start the objects 16-byte aligned */
  offset += sizeof(struct slab_page) % 16;
  sl->data_offset = offset;
  mush_strncpy(sl->name, name, 64);
  sl->fill_strategy = 1;
  sl->keep_last_empty = 0;
  sl->hintless_threshold = 0;
  sl->slabs = NULL;
  if (item_size < SIZEOF_VOID_P)
    item_size = SIZEOF_VOID_P;
  /* Align objects after the first with the size of a pointer */
  item_size += item_size % SIZEOF_VOID_P;
  sl->item_size = item_size;
  sl->items_per_page = (pgsize - offset) / item_size;

  if (item_size >= (pgsize - offset)) {
    do_rawlog(LT_TRACE,
              "slab(%s): item_size of %lu bytes is too large for a pagesize of %lu bytes. Using malloc() for allocations from this slab.",
              name, (unsigned long) item_size, (unsigned long) pgsize);
    sl->items_per_page = 0;
    return sl;
  }

  return sl;
}

/** Set a slab allocator option
 * \param sl the allocator
 * \param opt The option to set
 * \param val Value to set option too (Not meaningful for all options)
 */
void
slab_set_opt(slab *sl, enum slab_options opt, int val
             __attribute__ ((__unused__)))
{
  if (!sl)
    return;
  switch (opt) {
  case SLAB_ALLOC_FIRST_FIT:
    sl->fill_strategy = 1;
    break;
  case SLAB_ALLOC_BEST_FIT:
    sl->fill_strategy = 0;
    break;
  case SLAB_ALWAYS_KEEP_A_PAGE:
    sl->keep_last_empty = val;
    break;
  case SLAB_HINTLESS_THRESHOLD:
    sl->hintless_threshold = val;
    break;
  default:
    /* Unknown option */
    break;
  }
}

/** Allocate a new page.
 * \param sl Allocator to create the page for.
 * \return new page, NOT linked into the allocator's list of pages
 */
static struct slab_page *
slab_alloc_page(struct slab *sl)
{
  struct slab_page *sp;
  uint8_t *page = NULL;
  int n;
  int pgsize;

  pgsize = getpagesize();

#ifdef HAVE_POSIX_MEMALIGN
  /* Used to use valloc() here, but on some systems, memory returned by
     valloc() can't be passed to free(). Those same systems probably won't have
     posix_memalign. Deal.
   */
  if (posix_memalign((void **) &page, pgsize, pgsize) < 0) {
    do_rawlog(LT_ERR, "Unable to allocate %d bytes via posix_memalign: %s",
              pgsize, strerror(errno));
    page = malloc(pgsize);
  }
#else
  page = malloc(pgsize);
#endif
  memset(page, 0, pgsize);

  sp = (struct slab_page *) page;
  sp->nfree = sl->items_per_page;
  sp->nalloced = 0;
  sp->freelist = NULL;
  page = page + sl->data_offset;
  for (n = 0; n < sl->items_per_page; n++) {
    struct slab_page_list *item = (struct slab_page_list *) page;
    page += sl->item_size;
    item->next = sp->freelist;
    sp->freelist = item;
  }
  sp->last_obj = sp->freelist;
  sp->next = NULL;
#ifdef SLAB_DEBUG
  do_rawlog(LT_TRACE,
            "Allocating page starting at %p for slab(%s).\n\tFirst object allocated at %p, last object at %p",
            (void *) sp, sl->name, (void *) (sp + sl->data_offset),
            sp->last_obj);
#endif

  return sp;
}

/** Allocate a new object from a page
 * \param where the page to allocate from.
 * \return pointer to object, or NULL if no room left on page
 */
static void *
slab_alloc_obj(struct slab_page *where)
{
  struct slab_page_list *obj;

  obj = where->freelist;
  if (obj == NULL)
    return NULL;

  where->freelist = obj->next;
  where->nalloced += 1;
  where->nfree -= 1;

  return obj;
}

/** Return a new object allocated from a slab.
 * \param sl the slab  allocator
 * \param hint If non-NULL, try to allocate new object on the same page.
 * \return pointer to object, or NULL
 */
void *
slab_malloc(slab *sl, const void *hint)
{
  if (!sl)
    return NULL;

  /* If objects are too big to fit in a single page, use plain malloc */
  if (sl->items_per_page == 0)
    return malloc(sl->item_size);

  /* If no pages have been allocated, make one and use it. */
  if (!sl->slabs) {
    sl->slabs = slab_alloc_page(sl);
    return slab_alloc_obj(sl->slabs);
  }

  if (!hint) {
    /* Use first empty spot. */
    struct slab_page *page, *last = NULL, *best = NULL;
    int best_free = INT_MAX;
    for (page = sl->slabs; page; page = page->next) {
      if (page->nfree > sl->hintless_threshold && sl->fill_strategy)
        /* First fit */
        return slab_alloc_obj(page);
      else if (page->nfree > sl->hintless_threshold) {
        /* Best fit */
        if (page->nfree < best_free) {
          best_free = page->nfree;
          best = page;
          if (best->nfree == sl->hintless_threshold + 1)
            break;
        }
      }
      last = page;
    }

    if (best)
      return slab_alloc_obj(best);

    /* All pages are full; allocate a new one */
    last->next = slab_alloc_page(sl);
    return slab_alloc_obj(last->next);
  } else {
    struct slab_page *page, *last = NULL;
    /* Okay. We have a hint for where to allocate the object. Find the
       page the hint is on. */
    for (page = sl->slabs; page; page = page->next) {
      if (hint > (void *) page && hint <= page->last_obj) {
        /* If there's space, use this page, otherwise, if using
           first-fit, use the first page with room if using best-fit,
           see if the next or previous page has room, otherwise,
           normal best-fit match */
        if (page->nfree > 0)
          return slab_alloc_obj(page);
        if (sl->fill_strategy)
          return slab_malloc(sl, NULL);
        else if (page->next && page->next->nfree > 0)
          return slab_alloc_obj(page->next);
        else if (last && last->nfree > 0)
          return slab_alloc_obj(last);
        else
          return slab_malloc(sl, NULL);
      }
      last = page;
    }
    /* This should never be reached, but handle it anyways. */
#ifdef SLAB_DEBUG
    do_rawlog(LT_TRACE, "page hint %p not found in slab(%s)", (void *) hint,
              sl->name);
#endif
    last->next = slab_alloc_page(sl);
    return slab_alloc_obj(last->next);
  }
  return NULL;
}

/** Free an allocated slab object
 * \param sl the slab allocator
 * \param obj the object to free
 */
void
slab_free(slab *sl, void *obj)
{
  struct slab_page *page, *last;

  /* If objects are too big to fit in a single page, use plain free */
  if (sl->items_per_page == 0) {
    free(obj);
    return;
  }

  /* Find the page the object is on and push it into that page's free list */
  last = NULL;
  for (page = sl->slabs; page; page = page->next) {
    if (obj > (void *) page && obj <= page->last_obj) {
      struct slab_page_list *item = obj;
#ifdef SLAB_DEBUG
      struct slab_page_list *scan;
      for (scan = page->freelist; scan; scan = scan->next)
        if (item == scan)
          do_rawlog(LT_TRACE,
                    "Attempt to free already free object %p from page %p of slab(%s)",
                    (void *) item, (void *) page, sl->name);
#endif
      item->next = page->freelist;
      page->freelist = item;
      page->nalloced -= 1;
      page->nfree += 1;
#ifdef SLAB_DEBUG
      assert(page->nalloced >= 0 && page->nalloced <= sl->items_per_page);
      assert(page->nfree >= 0 && page->nfree <= sl->items_per_page);
#endif
      if (page->nalloced == 0) {
        /* Empty page. Free it. */

        /* Unless it's the only allocated page and we want to keep it */
        if (sl->keep_last_empty && page == sl->slabs && !page->next)
          return;

        if (last)
          last->next = page->next;
        else
          sl->slabs = page->next;

#ifdef SLAB_DEBUG
        do_rawlog(LT_TRACE, "Freeing empty page %p of slab(%s)", (void *) page,
                  sl->name);
#endif
        free(page);
      }
      return;
    }
    last = page;
  }
  /* Ooops. An object not allocated by this allocator! */
  do_rawlog(LT_TRACE,
            "Attempt to free object %p not allocated by slab(%s)",
            obj, sl->name);
}

/** Destroy a slab and all objects allocated from it.
 *  Any objects allocated from the slab with pointers to
 * objects allocated from outside the slab will NOT be freed.
 * \param sl the slab to destroy.
 */
void
slab_destroy(slab *sl)
{
  struct slab_page *page, *next;
  for (page = sl->slabs; page; page = next) {
    next = page->next;
    free(page);
  }
  free(sl);
}

/** Describe a slab for \@list allocations
 * \param player who to display to
 * \param sl the slab
 */
void
slab_describe(dbref player, slab *sl)
{
  struct slab_page *page;
  int n = 0;
  size_t allocated = 0, freed = 0;
  int min_fill = INT_MAX, max_fill = 0;
  int full = 0, under100 = 0, under75 = 0, under50 = 0, under25 = 0;

  if (!sl)
    return;

  for (page = sl->slabs; page; page = page->next) {
    double p;
    n++;
    allocated += page->nalloced;
    freed += page->nfree;
    if (page->nalloced > max_fill)
      max_fill = page->nalloced;
    if (page->nalloced < min_fill)
      min_fill = page->nalloced;
    p = (double) page->nalloced / (double) sl->items_per_page;
    if (p == 1.0)
      full += 1;
    else if (p > 0.75)
      under100 += 1;
    else if (p > 0.50)
      under75 += 1;
    else if (p > 0.25)
      under50 += 1;
    else
      under25 += 1;
  }

  notify_format(player, "Allocator for %s:", sl->name);
  notify_format(player,
                "   object size (bytes): %-6d       objects per page: %-6d",
                sl->item_size, sl->items_per_page);
  notify_format(player,
                "       allocated pages: %-6d      objects added via: %s", n,
                sl->fill_strategy ? "first fit" : "best fit");
  notify_format(player,
                "     allocated objects: %-6ld           free objects: %-6ld",
                (unsigned long) allocated, (unsigned long) freed);
  if (allocated > 0) {
    notify_format(player,
                  " fewest allocs in page: %-6d    most allocs in page: %-6d",
                  min_fill, max_fill);
    notify_format(player,
                  "    allocation average:%6.2f%%        pages 100%% full: %-6d",
                  (((double) allocated / ((double) allocated + (double) freed))
                   * 100.0), full);
    notify_format(player,
                  "       pages >75%% full: %-6d        pages >50%% full: %-6d",
                  under100, under75);
    notify_format(player,
                  "       pages >25%% full: %-6d        pages <25%% full: %d",
                  under50, under25);
  }
}


extern slab *attrib_slab, *lock_slab, *boolexp_slab, *bvm_asmnode_slab,
  *bvm_strnode_slab, *flag_slab, *player_dbref_slab,
  *chanuser_slab, *chanlist_slab, *mail_slab,
  *text_block_slab, *function_slab, *memcheck_slab, *intmap_slab,
  *pe_reg_slab, *pe_reg_val_slab, *flagbucket_slab;

#if COMPRESSION_TYPE == 1 || COMPRESSION_TYPE == 2
extern slab *huffman_slab;
#endif

/** List information about slab allocators and memcheck data. Admin only. */
void
do_list_allocations(dbref player)
{
  if (!Hasprivs(player)) {
    notify(player, T("Sorry."));
    return;
  }
  slab_describe(player, attrib_slab);
#ifdef DEBUG
  /* These should always be 0. No need to display them most of the
     time. */
  slab_describe(player, boolexp_slab);
  slab_describe(player, bvm_asmnode_slab);
  slab_describe(player, bvm_strnode_slab);
#endif
  slab_describe(player, chanlist_slab);
  slab_describe(player, chanuser_slab);
  slab_describe(player, flag_slab);
  slab_describe(player, function_slab);
#if COMPRESSION_TYPE == 1 || COMPRESSION_TYPE == 2
  slab_describe(player, huffman_slab);
#endif
  slab_describe(player, lock_slab);
  slab_describe(player, mail_slab);
  slab_describe(player, memcheck_slab);
  slab_describe(player, text_block_slab);
  slab_describe(player, player_dbref_slab);
  slab_describe(player, intmap_slab);
  slab_describe(player, pe_reg_slab);
  slab_describe(player, pe_reg_val_slab);
  slab_describe(player, flagbucket_slab);

  if (options.mem_check) {
    notify(player, "malloc allocations:");
    list_mem_check(player);
  }
}

#ifdef WIN32
/** Windows version of getpagesize() */
unsigned int
getpagesize_win32(void)
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
}
#endif
