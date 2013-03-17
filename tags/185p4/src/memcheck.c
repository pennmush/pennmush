/**
 * \file memcheck.c
 *
 * \brief A simple memory allocation tracker for PennMUSH.
 *
 * When mem_check is turned on in mush.cnf, every time mush_malloc or
 * add_check is called, the name given to that allocation gets its
 * reference count incremented, and every time mush_free or del_check
 * is called, the count is decrememented. Non-0 counts can be see
 * in-game via \@list allocations, and all counts are periodically
 * dumped to log/trace.log. Negative counts or steadily growing counts
 * without an obvious reason indicate problems. Choose allocation
 * names wisely; 'string', for example, is used so much that it can be
 * hard to track down a leak in it.
 *
 * Reference counts used to be stored in a simple sorted linked list,
 * but profiling showed that add_check() and del_check() were in the
 * top 4 function calls. It's been rewritten to use a skip list, an
 * interesting data structure that combines binary tree-like
 * performance when looking up random entries (but with simpler code),
 * and the ease of traversing a linked list. It'll be interesting to
 * see what difference this makes the next time M*U*S*H gets profiled.
 *
 * See http://en.wikipedia.org/wiki/Skip_list for more on them, or the
 * original paper at ftp://ftp.cs.umd.edu/pub/skipLists/skiplists.pdf.
 *
 * A few notes about this implementation: 
 *
 * The probability used for calculating the number of skips in any
 * given node is \f$\frac{1}{p}\f$. For example, with \f$p = 2\f$: All
 * nodes have at least one link. Half of those will have at least
 * two. Half of those will have at least three, and so on up to our
 * maximimum. 
 *
 * The algorithm that's typically shown for picking the number of
 * skips in a newly initialized node involves repeated generation of
 * random numbers and comparing them to p (See Figure 5 in the linked
 * paper.) We use a trick involving a logarithm to get the same
 * distribution in one step, with a single random number.
 * 
 * <img src="http://raevnos.pennmush.org/images/skip_list.png" />
 *
 * The maximum number of levels links we deal with, \f$L(N)\f$, is
 * based on the equation \f$\log_{\frac{1}{p}}N\f$. \f$N\f$ is the
 * upper bound of the number of elements in the list. Based on
 * M*U*S*H, it's around 180. The MAX_LINKS constant isn't going to
 * have to be changed until it gets over 256.
 *
 * We use \f$p = 4\f$ in this version, which works out to an average
 * of 1.33 links per node and around 17.5 nodes checked in a typical
 * search.
 */
#include "config.h"
#include "conf.h"
#include "copyrite.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "externs.h"
#include "dbdefs.h"
#include "mymalloc.h"
#include "log.h"
#include "SFMT.h"
#include "confmagic.h"

typedef struct mem_check_node MEM;

enum {
  P = 4, /**< 1/P is the probability that a node with N links will
	    have N+1 links, up to... */
  MAX_LINKS = 4, /**< Maximum number of links in a single skip list
		   node. log(1/p)(N), where N is the expected maximum
		   length (~180). p == 1/2 is 8, p == 1/4 is 4 */
  REF_NAME_LEN = 64 /**< Length of longest check name */
};

/** A skip list for storing memory allocation counts */
struct mem_check_node {
  int ref_count;                /**< Number of allocations of this type. */
  int link_count;               /**< Number of forward links */
  char ref_name[REF_NAME_LEN];  /**< Name of this allocation type. */
  MEM *links[MAX_LINKS];        /**< Forward links to further nodes in skiplist. */
};

static MEM memcheck_head_storage = { 0, MAX_LINKS, {'\0'}, {NULL} };

static MEM *memcheck_head = &memcheck_head_storage;

/* MEM structs are kind of large for a slab, but storing them in one
   will boost cache locality when iterating/searching the list. 
   
   TODO: Consider storing nodes with different skip link counts in
   different slabs. Probably overkill when dealing with < 200
   elements, though. We can live with the wasted space.  */
slab *memcheck_slab = NULL;

/*** WARNING! DO NOT USE strcasecoll IN THESE FUNCTIONS OR YOU'LL CREATE
 *** AN INFINITE LOOP. DANGER, WILL ROBINSON!
 ***/

/* Look up an entry in the skip list */
static MEM *
lookup_check(const char *ref)
{
  MEM **links, *prev;
  int n;

  /* Empty list */
  if (!memcheck_head->links[0])
    return NULL;

  links = memcheck_head->links;
  prev = NULL;
  n = memcheck_head->link_count - 1;

  while (n >= 0) {
    MEM *chk;
    int cmp;

    chk = links[n];

    /* Skip nulls and avoid redoing a failed comparision when link n
       and n+1 both point to the same node */
    if (!chk || (prev && chk == prev)) {
      n -= 1;
      continue;
    }

    cmp = strcmp(ref, chk->ref_name);
    if (cmp == 0) {             /* Found it */
      return chk;
    } else if (cmp > 0) {       /* key is further down the chain, start over
                                   with chk as the new head */
      links = chk->links;
      n = chk->link_count - 1;
      prev = NULL;
    } else if (cmp < 0) {       /* Went too far, try the next lowest link */
      n -= 1;
      prev = chk;
    }
  }
  /* Not found */
  return NULL;
}

/* Return the number of links to use for a new node. Result's range is
   1..maxcount */
static int
pick_link_count(int maxcount)
{
  int lev = (int) floor(log(genrand_real3()) / log(P));
  lev = -lev;
  if (lev > maxcount)
    return maxcount;
  else
    return lev;
}

/* Allocate a new reference count struct. Consider moving away from
   slabs; the struct size is pretty big with the skip list fields
   added. But see comment for memcheck_slab's declaration. */
static MEM *
alloc_memcheck_node(const char *ref)
{
  MEM *newcheck;

  if (!memcheck_slab)
    memcheck_slab = slab_create("mem check references", sizeof(MEM));

  newcheck = slab_malloc(memcheck_slab, NULL);
  memset(newcheck, 0, sizeof *newcheck);
  mush_strncpy(newcheck->ref_name, ref, REF_NAME_LEN);
  newcheck->link_count = pick_link_count(MAX_LINKS);
  newcheck->ref_count = 1;
  return newcheck;
}

#ifdef min
#undef min
#endif

static inline int
min(int a, int b)
{
  if (a <= b)
    return a;
  else                          /* a > b */
    return b;
}

/* Update forward skip links for a new element of the list */
static void
update_links(MEM *src, MEM *newnode)
{
  int n = min(src->link_count, newnode->link_count) - 1;

  for (; n > 0; n -= 1) {
    if (!src->links[n])
      src->links[n] = newnode;
    else {
      int cmp = strcmp(src->links[n]->ref_name, newnode->ref_name);
      if (cmp < 0) {
        /* Advance along the skip list and adjust as needed. */
        update_links(src->links[n], newnode);
        return;
      } else if (cmp > 0) {
        /* Insert into skip chain */
        newnode->links[n] = src->links[n];
        src->links[n] = newnode;
      }
    }
  }
}

/* Insert a new entry in the skip list. Bad things happen if you try to insert a duplicate. */
static void
insert_check(const char *ref)
{
  MEM *chk, *node, *prev;
  int n;

  chk = alloc_memcheck_node(ref);

  /* Empty list */
  if (!memcheck_head->links[0]) {
    for (n = 0; n < chk->link_count; n += 1)
      memcheck_head->links[n] = chk;
    return;
  }

  /* Find where to insert the new node, using a simple linked list
     walk. Ideally, this should be using a standard skip list
     insertion algorithm to avoid O(N) performance, but insertions
     occur infrequently once the mush is running, so I don't care that
     much. */
  for (node = memcheck_head->links[0], prev = NULL; node;
       prev = node, node = node->links[0]) {
    if (strcmp(ref, node->ref_name) < 0) {
      if (prev) {
        chk->links[0] = node;
        prev->links[0] = chk;
      } else {
        /* First element */
        chk->links[0] = memcheck_head->links[0];
        memcheck_head->links[0] = chk;
      }
      break;
    }
  }
  if (!node)                    /* Insert at end of list */
    prev->links[0] = chk;

  /* Now adjust forward pointers */
  update_links(memcheck_head, chk);
}

/** Add an allocation check.
 * \param ref type of allocation.
 */
void
add_check(const char *ref)
{
  MEM *chk;

  if (!options.mem_check)
    return;

  chk = lookup_check(ref);
  if (chk)
    chk->ref_count += 1;
  else
    insert_check(ref);
}

/** Remove an allocation check.
 * \param ref type of allocation to remove.
 * \param filename file del_check was called from
 * \param line linenumber in filename where del_check was called
 */
void
del_check(const char *ref, const char *filename, int line)
{
  MEM *chk;

  if (!options.mem_check)
    return;

  chk = lookup_check(ref);

  if (chk) {
    chk->ref_count -= 1;
    if (chk->ref_count < 0)
      do_rawlog(LT_TRACE,
                "ERROR: Deleting a check with a negative count: %s (At %s:%d)",
                ref, filename, line);
  } else {
    do_rawlog(LT_TRACE,
              "ERROR: Deleting a non-existant check: %s (At %s:%d)",
              ref, filename, line);
  }
}

/** List allocations in use.
 * \param player the enactor.
 */
void
list_mem_check(dbref player)
{
  MEM *chk;

  if (!options.mem_check)
    return;
  for (chk = memcheck_head->links[0]; chk; chk = chk->links[0]) {
    if (chk->ref_count != 0)
      notify_format(player, "%s : %d", chk->ref_name, chk->ref_count);
  }
}

/** Log all allocations.
 */
void
log_mem_check(void)
{
  MEM *chk;

  if (!options.mem_check)
    return;
  do_rawlog(LT_TRACE, "MEMCHECK dump starts");
  for (chk = memcheck_head->links[0]; chk; chk = chk->links[0]) {
    do_rawlog(LT_TRACE, "%s : %d", chk->ref_name, chk->ref_count);
  }
  do_rawlog(LT_TRACE, "MEMCHECK dump ends");
}


/** Dump a representation of the memcheck skip list into a file, using the dot language.
 * Use from a debugger:
 * \verbatim
 * (gdb) print memcheck_dump_struct("memcheck.dot")
 * \endverbatim
 * and then turn into an image:
 * \verbatim
 * # dot -Tsvg -o memcheck.svg memcheck.dot
 * \endverbatim
 * (dot is part of the graphviz package)
 * \param filename The output file name.
 */
void
memcheck_dump_struct(const char *filename)
{
  FILE *fp;
  MEM *chk;
  int n;

  fp = fopen(filename, "w");
  if (!fp) {
    penn_perror("fopen");
    return;
  }

  fputs("digraph memcheck_skiplist {\n", fp);
  fputs("rankdir=LR;\n", fp);
  fputs("node [shape=record];\n", fp);
  fputs("head [label=\"<l0>HEAD", fp);
  for (n = 1; n < MAX_LINKS; n += 1) {
    fprintf(fp, "|<l%d>%d", n, n);
    if (!memcheck_head->links[n])
      fputs("\\n(NULL)", fp);
  }
  fputs("\"];\n", fp);

  for (n = 0; n < MAX_LINKS; n += 1) {
    if (memcheck_head->links[n])
      fprintf(fp, "head:l%d -> mc%p:l%d;\n", n,
              (void *) memcheck_head->links[n], n);
  }

  for (chk = memcheck_head->links[0]; chk; chk = chk->links[0]) {
    fprintf(fp, "mc%p [label=\"{<l0>%s|<s0>%d}", (void *) chk, chk->ref_name,
            chk->ref_count);
    for (n = 1; n < chk->link_count; n += 1) {
      fprintf(fp, "|<l%d>", n);
      if (!chk->links[n])
        fputs(" (NULL)", fp);
    }
    fputs("\"];\n", fp);
    if (chk->links[0])
      fprintf(fp, "mc%p:s0 -> mc%p:l0\n", (void *) chk, (void *) chk->links[0]);
    for (n = 1; n < chk->link_count; n += 1) {
      if (chk->links[n])
        fprintf(fp, "mc%p:l%d -> mc%p:l%d;\n", (void *) chk, n,
                (void *) chk->links[n], n);
    }
  }

  fputs("}\n", fp);
  fclose(fp);
}
