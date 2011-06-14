/**
 * \file intmap.c
 * \brief Implementation of integer-keyed maps.
 *
 * \verbatim
 * Uses patricia trees to efficiently store sparse integer maps. Keys
 * are unsigned 32 bit integers, and thus well suited for a radix tree
 * implementation. Simpler than balanced binary trees (Compare the
 * amount of code in strtree.c, which implements a red-black tree,
 * with this), and comparable in number of nodes visited while walking
 * the tree.
 *
 * To summarize, a patricia tree is a type of binary tree where
 * branching is determined by looking at a single bit of the key,
 * instead of the entire thing. There are no null links; each link
 * either points to a node with a higher bit to compare, or back at
 * itself or an ancestor node. When searching the tree, if you come
 * upon a backwards or self link, the current node has every bit
 * that's been checked so far in common with the search key. You then
 * compare the search key to the node's key to see if it matches. If
 * it doesn't, the search key isn't present. Only a few bits are
 * typically looked at when walking down the tree.
 *
 * There are various properties of patricia trees that aren't used at
 * all in this (Like finding all keys that have a particular
 * prefix). However, consider using them in a rewrite of the prefix
 * table code...
 *
 * Normally patricia trees use the leftmost bit as position 0. I've
 * found that when using the small integers that are typical for what
 * these trees are used for on Penn, going right to left produces a
 * slightly shallower tree, because with the leftmost bit as 0,
 * everything hangs off of the root node's 0 branch. The drawback to
 * rightmost is that you can't walk the nodes in sorted order. Which
 * is okay because we don't need to do that.
 *
 * The implementation is based loosely on ones found at
 * http://www.mcdowella.demon.co.uk/Patricia.html and in the book
 * <ul>Algorithms in C, 3rd Edition</ul> by Robert Sedgewick.
 * \endverbatim
 */

#include "config.h"


/* Uncomment this to turn off various consistency checks. Not
   recommended yet. */
/* #define NDEBUG */

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "conf.h"
#include "externs.h"
#include "mymalloc.h"
#include "intmap.h"
#include "confmagic.h"

/** Structure that represents a node in a patricia tree. */
typedef struct patricia {
  im_key key; /**< Key value */
  int bit; /**< Which bit to test in this node */
  void *data; /**< Pointer to data */
  struct patricia *links[2]; /**< Links to nodes to branch to based on set bit */
} patricia;

/** Integer map struct */
struct intmap {
  int64_t count; /**< Number of elements in tree */
  patricia *root; /**< pointer to root of tree */
};

#define MAX_BIT 31
/* #define MAX_BIT 63 */

slab *intmap_slab = NULL; /**< Allocator for patricia nodes */

/** Return the number of elements in an integer map, or -1 on error
    (Passing a NULL pointer instead of a map). */
int64_t
im_count(intmap *im)
{
  if (im)
    return im->count;
  else
    return -1;
}

/** Allocate and initialize a new integer map. */
intmap *
im_new(void)
{
  intmap *im;
  im = mush_malloc(sizeof *im, "int_map");
  if (!intmap_slab)
    intmap_slab = slab_create("patricia tree nodes", sizeof(struct patricia));
  im->count = 0;
  im->root = NULL;
  return im;
}

static void
pat_destroy(patricia *node)
{
  int n;

  if (!node)
    return;

  for (n = 0; n < 2; n++)
    if (node->links[n]->bit > node->bit)
      pat_destroy(node->links[n]);

  slab_free(intmap_slab, node);
}

/** Deallocate an integer map. All data pointers that need to be freed
 *  qmust be deallocated seperately before this, or you'll get a memory
 *  leak.
 *  \param im the map to delete.
 */
void
im_destroy(intmap *im)
{
  if (im) {
    pat_destroy(im->root);
    mush_free(im, "int_map");
  }
}

/* Returns 1 if a given bit is set, 0 if not */
#define digit(n, pos) (!!((n) & (1U << (pos))))

/* More traditional right to left scanning version */
/* #define digit(n, pos) (!!((n) & (1U << (31 - (pos))))) */

/* Returns the node matching the key or its prefix */
static patricia *
pat_search(patricia *node, im_key key, int bit)
{
  assert(node);

  if (node->bit <= bit)
    return node;
  else
    return pat_search(node->links[digit(key, node->bit)], key, node->bit);
}

/** Look up an element in the map.
 * \param im the map.
 * \param key the key to look up.
 * \returns the value of the data stored under that key, or NULL
 */
void *
im_find(intmap *im, im_key key)
{
  patricia *node;

  if (!im->root)
    return NULL;

  node = pat_search(im->root, key, -1);

  if (node->key == key)
    return node->data;
  else
    return NULL;
}

/** Test if a particular key exists in a map.
 * \param im the map.
 * \param key the key to look up.
 * \return true if the key is present, false otherwise.
 */
bool
im_exists(intmap *im, im_key key)
{
  patricia *node;

  if (!im->root)
    return 0;
  node = pat_search(im->root, key, -1);
  return node->key == key;
}

/** Insert a new element into the map.
 * \param im the map to insert into.
 * \param key the key
 * \param data Pointer to data to store under this key.
 * \return true on success, false on failure (Usually a duplicate key)
 */
bool
im_insert(intmap *im, im_key key, void *data)
{
  patricia *here, *newnode, *prev = NULL;
  int bit, prevbit = 0;

  newnode = slab_malloc(intmap_slab, im->root);
  newnode->key = key;
  newnode->data = data;
  newnode->links[0] = newnode->links[1] = newnode;

  /* First key added to tree */
  if (!im->root) {
    newnode->bit = 0;
    im->root = newnode;
    im->count += 1;
    assert(im->count == 1);
    return true;
  }

  here = pat_search(im->root, key, -1);
  if (here && here->key == key) {
    /* Duplicate key fails */
    slab_free(intmap_slab, newnode);
    return false;
  }

  /* Not a duplicate, so key and here->key /will/ differ in at least one
     bit. No need to make sure that bit doesn't go > 31 */
  for (bit = 0; digit(key, bit) == digit(here->key, bit); bit++) ;

  newnode->bit = bit;

  if (bit < im->root->bit) {
    newnode->links[digit(im->root->key, bit)] = im->root;
    im->root = newnode;
    im->count += 1;
    assert(im->count > 1);
    return true;
  }

  for (here = im->root;;) {
    int i;
    if (here->bit == bit) {
      newnode->bit = bit + 1;
      here->links[digit(key, bit)] = newnode;
      im->count += 1;
      assert(im->count > 1);
      return true;
    }
    if (here->bit > bit || (prev && here->bit <= prev->bit)) {
      prev->links[prevbit] = newnode;
      newnode->links[digit(here->key, bit)] = here;
      im->count += 1;
      assert(im->count > 1);
      return true;
    }

    prev = here;
    i = digit(key, here->bit);
    here = prev->links[i];
    prevbit = i;
  }

  /* Not reached */
  assert(0);
  return false;
}


/** Delete a key from the map.
 * \param im the map.
 * \param key the key to delete.
 * \return true on success, false on failure (Key not present?)
 */
bool
im_delete(intmap *im, im_key key)
{
  patricia *parent = NULL, *firstparent = NULL, *grandparent = NULL;
  patricia *here = im->root;

  if (!here)
    return false;

  while (1) {
    int x = digit(key, here->bit);

    grandparent = parent;
    parent = here;
    here = here->links[x];

    assert(here);

    if (here->key == key && !firstparent)
      firstparent = parent;

    if (here->bit <= parent->bit)
      break;
  }

  /* Key not found? */
  if (here->key != key)
    return false;

  /* Case 1: key is the only node in tree */
  if (im->root == here && here->links[0] == here && here->links[1] == here) {
    slab_free(intmap_slab, im->root);
    im->root = NULL;
    im->count -= 1;
    assert(im->count == 0);
    return true;
  }

  /* Case 2: node points to itself. Edit it out. */
  if (parent->key == key) {
    int i;
    patricia *replacement;

    i = (here->links[0] == here);
    replacement = here->links[i];

    if (replacement != here) {
      if (!grandparent || grandparent == here)
        im->root = replacement;
      else
        grandparent->links[grandparent->links[1] == parent] = replacement;
    } else
      grandparent->links[grandparent->links[1] == parent] = grandparent;

    slab_free(intmap_slab, here);
    im->count -= 1;
    assert(im->count >= 1);
    return true;
  }

  /* Case 3: Node with children pointing up to it. */
  {
    patricia *otherlink;

    if (firstparent == parent)
      im->root = parent;
    else
      firstparent->links[firstparent->links[1] == here] = parent;

    otherlink = parent->links[parent->links[0] == here];
    if (here == grandparent) {
      int i;
      patricia *other;

      i = (here->links[0] == parent);
      other = parent->links[parent->links[0] == here];
      parent->links[i] = here->links[i];
      parent->links[1 - i] = other;
      if (parent->links[0] == here)
        parent->links[0] = parent;
      if (parent->links[1] == here)
        parent->links[1] = parent;
      parent->bit = here->bit;
      slab_free(intmap_slab, here);
      im->count -= 1;
      assert(im->count >= 1);
      return true;
    }

    grandparent->links[grandparent->links[1] == parent] = otherlink;
    parent->links[0] = here->links[0];
    if (parent->links[0] == here)
      parent->links[0] = parent;
    parent->links[1] = here->links[1];
    if (parent->links[1] == here)
      parent->links[1] = parent;
    parent->bit = here->bit;
    slab_free(intmap_slab, here);
    im->count -= 1;
    assert(im->count >= 1);
    return true;
  }
}

int format_long(intmax_t n, char *buff, char **bp, int maxlen, int base);

static void
pat_list_nodes(patricia *node, FILE * fp)
{
  int n;
  char tmpbuf[100];
  char *bp = tmpbuf;

  if (!node)
    return;

  format_long(node->key, tmpbuf, &bp, 99, 2);
  *bp = '\0';

  fprintf(fp,
          "node%u [label=\"{ <key> key = 0b%s (%u) | bit = %d | { <b0> 0 | <b1> 1 } }\", ",
          (unsigned int) node->key, tmpbuf, (unsigned int) node->key,
          node->bit);
  if (node->links[0]->bit > node->bit && node->links[1]->bit > node->bit)
    fputs("fillcolor=1", fp);
  else if (node->links[0]->bit <= node->bit && node->links[1]->bit <= node->bit)
    fputs("fillcolor=3", fp);
  else
    fputs("fillcolor=2", fp);

  fputs("];\n", fp);

  for (n = 0; n < 2; n++) {
    if (node->links[n]->bit > node->bit)
      pat_list_nodes(node->links[n], fp);
  }
}

static void
pat_list_links(patricia *node, FILE * fp)
{
  int i;
  const char *edge_styles[] = { "style=dashed,arrowhead=open",
    "style=solid,arrowhead=normal"
  };

  if (!node)
    return;

  for (i = 0; i < 2; i++) {
    if (node->links[i]) {
      fprintf(fp, "node%u:b%d -> node%u:key [%s];\n", (unsigned int) node->key,
              i, (unsigned int) node->links[i]->key,
              edge_styles[node->links[i]->bit > node->bit]);
      if (node->links[i]->bit > node->bit)
        pat_list_links(node->links[i], fp);
    }
  }
}

/** Dump a representation of an intmap into a file, using the dot language.
 * Use from a debugger:
 * \verbatim
 * (gdb)print im_dump_graph(queue_map, "queue.dot")
 * \endverbatim
 * and then turn into an image:
 * \verbatim
 * # dot -Tpng -o queue.png queue.dot
 * \endverbatim
 * (dot is part of the graphviz package)
 * \param im the map to print.
 * \param filename The output file name.
 */
void
im_dump_graph(intmap *im, const char *filename)
{
  FILE *fp;

  if (!im || !filename)
    return;

  fp = fopen(filename, "w");

  if (!fp) {
    penn_perror("fopen");
    return;
  }

  fputs("digraph patricia { \n", fp);
  fputs("node [shape=Mrecord, colorscheme=blues3, style=filled];\n", fp);
  pat_list_nodes(im->root, fp);
  pat_list_links(im->root, fp);
  fputs("}\n", fp);
  fclose(fp);
}

/** Header line for \@stats/tables for intmaps */
void
im_stats_header(dbref player)
{
  notify(player, "Map         Entries ~Memory");
}

/** \@stats/tables line */
void
im_stats(dbref player, intmap *im, const char *name)
{
  notify_format(player, "%-11s %7lld %7u", name, (long long) im->count,
                (unsigned int) (sizeof(*im) + (sizeof(patricia) * im->count)));
}
