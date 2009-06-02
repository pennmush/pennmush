/**
 * \file memcheck.c
 *
 * \brief A simple memory allocation tracker for PennMUSH.
 *
 * This code isn't usually compiled in, but it's handy to debug
 * memory leaks sometimes.
 *
 *
 */
#include "config.h"
#include "conf.h"
#include "copyrite.h"

#include <stdlib.h>
#include <string.h>


#include "externs.h"
#include "dbdefs.h"
#include "mymalloc.h"
#include "log.h"
#include "confmagic.h"

typedef struct mem_check MEM;

/* Length of longest check name */
#define REF_NAME_LEN 64

/** A linked list for storing memory allocation counts */
struct mem_check {
  int ref_count;                /**< Number of allocations of this type. */
  MEM *next;                    /**< Pointer to next in linked list. */
  char ref_name[REF_NAME_LEN];    /**< Name of this allocation type. */
};

static MEM *my_check = NULL;

slab *memcheck_slab = NULL;

/*** WARNING! DO NOT USE strcasecoll IN THESE FUNCTIONS OR YOU'LL CREATE
 *** AN INFINITE LOOP. DANGER, WILL ROBINSON!
 ***/

/** Add an allocation check.
 * \param ref type of allocation.
 */
void
add_check(const char *ref)
{
  MEM *loop, *newcheck, *prev = NULL;
  int cmp;

  if (!options.mem_check)
    return;

  for (loop = my_check; loop; loop = loop->next) {
    cmp = strcmp(ref, loop->ref_name);
    if (cmp == 0) {
      loop->ref_count++;
      return;
    } else if (cmp < 0)
      break;
    prev = loop;
  }
  if (!memcheck_slab)
    memcheck_slab = slab_create("mem check references", sizeof(MEM));

  newcheck = slab_malloc(memcheck_slab, prev);
  mush_strncpy(newcheck->ref_name, ref, REF_NAME_LEN);
  newcheck->ref_count = 1;
  newcheck->next = loop;
  if (prev)
    prev->next = newcheck;
  else
    my_check = newcheck;
  return;
}

/** Remove an allocation check.
 * \param ref type of allocation to remove.
 */
void
del_check(const char *ref, const char *filename, int line)
{
  MEM *loop, *prev = NULL;
  int cmp;

  if (!options.mem_check)
    return;

  for (loop = my_check; loop; loop = loop->next) {
    cmp = strcmp(ref, loop->ref_name);
    if (cmp == 0) {
      loop->ref_count--;
      if (loop->ref_count < 0)
        do_rawlog(LT_TRACE,
                  T
                  ("ERROR: Deleting a check with a negative count: %s (At %s:%d)"),
                  ref, filename, line);
      return;
    } else if (cmp < 0) {
      do_rawlog(LT_TRACE,
                T("ERROR: Deleting a non-existant check: %s (At %s:%d)"),
                ref, filename, line);
      break;
    }
    prev = loop;
  }
}

/** List allocations.
 * \param player the enactor.
 */
void
list_mem_check(dbref player)
{
  MEM *loop;

  if (!options.mem_check)
    return;
  for (loop = my_check; loop; loop = loop->next) {
    if (loop->ref_count != 0)
      notify_format(player, "%s : %d", loop->ref_name, loop->ref_count);
  }
}

/** Log all allocations.
 */
void
log_mem_check(void)
{
  MEM *loop;

  if (!options.mem_check)
    return;
  do_rawlog(LT_TRACE, "MEMCHECK dump starts");
  for (loop = my_check; loop; loop = loop->next) {
    do_rawlog(LT_TRACE, "%s : %d", loop->ref_name, loop->ref_count);
  }
  do_rawlog(LT_TRACE, "MEMCHECK dump ends");
}
