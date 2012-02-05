/**
 * \file strtree.c
 *
 * \brief String tables for PennMUSH.
 *
 * This is a string table implemented as a red-black tree.
 *
 * There are a couple of peculiarities about this implementation:
 *
 * (1) Parent pointers are not stored.  Instead, insertion and
 *     deletion remember the search path used to get to the
 *     current point in the tree, and use that path to determine
 *     parents.
 *
 * (2) A reference count is kept on items in the tree.
 *
 * (3) The red/black coloring is stored as the low order bit
 *     in the same byte as the reference count (which takes up
 *     the other 31 bits of that word).
 *
 * (4) The data string is stored directly in the tree node,
 *     instead of hung in a pointer off the node.  This means
 *     that the nodes are of variable size.  What fun.
 *
 * (5) The strings are stored in the table _unaligned_.  If
 *     you try to use this for anything other than strings,
 *     expect alignment problems.
 *
 * This string table is _NOT_ reentrant.  If you try to use this
 * in a multithreaded environment, you will probably get burned.
 */

#include "copyrite.h"

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "conf.h"
#include "externs.h"
#include "strtree.h"
#include "confmagic.h"

/* Various constants.  Their import is either bleedingly obvious
 * or explained below. */
#define ST_MAX_DEPTH 64         /**< Max depth of the tree */
#define ST_RED 1                /**< This node is red */
#define ST_BLACK 0              /**< This node is black */
#define ST_COLOR 1              /**< Bit mask for colors */
#define ST_USE_STEP 2
#define ST_USE_LIMIT (UINT32_MAX - ST_USE_STEP + 1)

/* Here we have a global for the path info, just so we don't
 * eat tons of stack space.  (This code isn't reentrant no
 * matter where we put this, so might as well save stack.)
 * The fixed size of this array puts a limit on the maximum
 * size of the string table... but with ST_MAX_DEPTH == 64,
 * the tree can hold between 4 billion and 8 quintillion
 * strings.  I don't think capacity is a problem.
 */
static StrNode *path[ST_MAX_DEPTH];

unsigned long st_mem = 0;       /**< Memory used by string trees */

static void st_left_rotate(int tree_depth, StrNode **root);
static void st_right_rotate(int tree_depth, StrNode **root);
static void st_print_tree(StrNode *node, int tree_depth, int lead);
static void st_traverse_stats
  (StrNode *node, int *maxdepth, int *mindepth, int *avgdepth, int *leaves);

void st_stats_header(dbref player);
void st_stats(dbref player, StrTree *root, const char *name);
static void delete_node(StrNode *node, const char *name);

/** Initialize a string tree.
 * \param root pointer to root of string tree.
 */
void
st_init(StrTree *root, const char *name)
{
  assert(root);
  root->root = NULL;
  root->count = 0;
  root->mem = 0;
  root->name = name;
}

static void
delete_node(StrNode *node, const char *name)
{
  if (node->left)
    delete_node(node->left, name);
  if (node->right)
    delete_node(node->right, name);
  mush_free(node, name);
}


/** Clear a string tree.
 * \param root pointer to root of string tree.
 */
void
st_flush(StrTree *root)
{
  if (!root->root)
    return;
  delete_node(root->root, root->name);
  root->root = NULL;
  root->count = 0;
  root->mem = 0;
}

/** Header for string tree stats.
 * \param player player to notify with header.
 */
void
st_stats_header(dbref player)
{
  notify(player, "Tree       Entries  Leaves MinDep  Max  Avg   ~Memory");
}

/** Statistics about the tree.
 * \param player player to notify with header.
 * \param root pointer to root of string tree.
 * \param name name of string tree, for row header.
 */
void
st_stats(dbref player, StrTree *root, const char *name)
{
  unsigned long bytes;
  int maxdepth = 0, mindepth = 0, avgdepth = 0, leaves = 0;

  bytes = (sizeof(StrNode) - BUFFER_LEN) * root->count + root->mem;
  st_traverse_stats(root->root, &maxdepth, &mindepth, &avgdepth, &leaves);
  notify_format(player, "%-10s %7d %7d %6d %4d %4d %7lu",
                name, (int) root->count, leaves, mindepth, maxdepth,
                avgdepth, bytes);
}

/* Tree rotations.  These preserve left-to-right ordering,
 * while modifying depth.
 */
static void
st_left_rotate(int tree_depth, StrNode **root)
{
  StrNode *x;
  StrNode *y;

  x = path[tree_depth];
  assert(x);
  y = x->right;
  assert(y);
  x->right = y->left;
  y->left = x;
  if (*root == x)
    *root = y;
  else if (path[tree_depth - 1]->left == x)
    path[tree_depth - 1]->left = y;
  else
    path[tree_depth - 1]->right = y;
}

static void
st_right_rotate(int tree_depth, StrNode **root)
{
  StrNode *y;
  StrNode *x;

  y = path[tree_depth];
  assert(y);
  x = y->left;
  assert(x);
  y->left = x->right;
  x->right = y;
  if (*root == y)
    *root = x;
  else if (path[tree_depth - 1]->right == y)
    path[tree_depth - 1]->right = x;
  else
    path[tree_depth - 1]->left = x;
}

/** String tree insert.  If the string is already in the tree, bump its usage
 * count and return the tree's version.  Otherwise, allocate a new tree
 * node, copy the string into the node, insert it into the tree, and
 * return the new node's string.
 * \param s string to insert in tree.
 * \param root pointer to root of string tree.
 * \return string inserted or NULL.
 */
char const *
st_insert(char const *s, StrTree *root)
{
  int tree_depth;
  StrNode *n;
  int cmp = 0;
  size_t keylen;

  assert(s);

  /* Hunt for the string in the tree. */
  tree_depth = 0;
  n = root->root;
  while (n && (cmp = strcmp(s, n->string))) {
    path[tree_depth] = n;
    tree_depth++;
    assert(tree_depth < ST_MAX_DEPTH);
    if (cmp < 0)
      n = n->left;
    else
      n = n->right;
  }

  if (n) {
    /* Found the string, so bump the usage and return. */
    if (n->info < ST_USE_LIMIT)
      n->info += ST_USE_STEP;
    return n->string;
  }

  /* Need a new node.  Allocate and initialize it. */
  keylen = strlen(s) + 1;
  n = mush_malloc(sizeof(StrNode) - BUFFER_LEN + keylen, root->name);
  if (!n)
    return NULL;
  memcpy(n->string, s, keylen);
  n->left = NULL;
  n->right = NULL;
  if (tree_depth == 0) {
    /* This is the first insertion!  Just stick it at the root
     * and get out of here. */
    root->root = n;
    n->info = ST_BLACK + ST_USE_STEP;
    return n->string;
  }
  n->info = ST_RED + ST_USE_STEP;

  /* Foo.  Have to do a complex insert.  Well, start by putting
   * the new node at the tip of an appropriate branch. */
  path[tree_depth] = n;
  tree_depth--;
  if (cmp < 0)
    path[tree_depth]->left = n;
  else
    path[tree_depth]->right = n;

  /* I rely on ST_RED != 0 and ST_BLACK == 0 in my bitwise ops. */
  assert(ST_RED);

  /* Sigh.  Fix the tree to maintain the red-black properties. */
  while (tree_depth > 0 && (path[tree_depth]->info & ST_COLOR) == ST_RED) {
    /* We have a double-red.  Blitch.  Now we have some mirrored
     * cases to look for, so stuff is duplicated left/right. */
    if (path[tree_depth] == path[tree_depth - 1]->left) {
      StrNode *y;
      y = path[tree_depth - 1]->right;
      if (y && (y->info & ST_COLOR) == ST_RED) {
        /* Hmph.  Uncle is red.  Push the mess up the tree. */
        path[tree_depth]->info &= ~ST_RED;
        y->info &= ~ST_RED;
        tree_depth--;
        path[tree_depth]->info |= ST_RED;
        tree_depth--;
      } else {
        /* Okay, uncle is black.  We can fix everything, now. */
        if (path[tree_depth + 1] == path[tree_depth]->right) {
          st_left_rotate(tree_depth, &root->root);
          path[tree_depth + 1]->info &= ~ST_RED;
        } else {
          path[tree_depth]->info &= ~ST_RED;
        }
        path[tree_depth - 1]->info |= ST_RED;
        st_right_rotate(tree_depth - 1, &root->root);
        break;
      }
    } else {
      StrNode *y;
      y = path[tree_depth - 1]->left;
      if (y && (y->info & ST_COLOR) == ST_RED) {
        /* Hmph.  Uncle is red.  Push the mess up the tree. */
        path[tree_depth]->info &= ~ST_RED;
        y->info &= ~ST_RED;
        tree_depth--;
        path[tree_depth]->info |= ST_RED;
        tree_depth--;
      } else {
        /* Okay, uncle is black.  We can fix everything, now. */
        if (path[tree_depth + 1] == path[tree_depth]->left) {
          st_right_rotate(tree_depth, &root->root);
          path[tree_depth + 1]->info &= ~ST_RED;
        } else {
          path[tree_depth]->info &= ~ST_RED;
        }
        path[tree_depth - 1]->info |= ST_RED;
        st_left_rotate(tree_depth - 1, &root->root);
        break;
      }
    }
  }

  /* The tree is now red-black true again.  Make the root black
   * just for convenience. */
  root->root->info &= ~ST_RED;
  root->count++;
  root->mem += strlen(s) + 1;
  return n->string;
}

/** Tree find.  Basically the first part of insert.
 * \param s string to find.
 * \param root pointer to root of string tree.
 * \return string if found, or NULL.
 */
char const *
st_find(char const *s, StrTree *root)
{
  int tree_depth;
  StrNode *n;
  int cmp;

  assert(s);

  /* Hunt for the string in the tree. */
  tree_depth = 0;
  n = root->root;
  while (n && (cmp = strcmp(s, n->string))) {
    path[tree_depth] = n;
    tree_depth++;
    assert(tree_depth < ST_MAX_DEPTH);
    if (cmp < 0)
      n = n->left;
    else
      n = n->right;
  }

  if (n)
    return n->string;
  return NULL;
}

/** Tree delete.  Decrement the usage count of the string, unless the
 * count is pegged.  If count reaches zero, delete.
 * \param s string to find and delete.
 * \param root pointer to root of string tree.
 */
void
st_delete(char const *s, StrTree *root)
{
  int tree_depth;
  StrNode *y;
  StrNode *x;
  int cmp;

  assert(s);

  /* Hunt for the string in the tree. */
  tree_depth = 0;
  y = root->root;
  while (y && (cmp = strcmp(s, y->string))) {
    path[tree_depth] = y;
    tree_depth++;
    assert(tree_depth < ST_MAX_DEPTH);
    if (cmp < 0)
      y = y->left;
    else
      y = y->right;
  }

  /* If it wasn't in the tree, we're done. */
  if (!y)
    return;

  /* If this node is permanent, then we're done. */
  if (y->info >= ST_USE_LIMIT)
    return;

  /* If this node has been used more than once, then decrement and exit. */
  if (y->info >= ST_USE_STEP * 2) {
    y->info -= ST_USE_STEP;
    return;
  }
  if (y->left && y->right) {
    /* It has two children.  We need to swap with successor. */
    int z_depth;
    char color;
    /* Record where we are. */
    z_depth = tree_depth;
    /* Find the successor. */
    path[tree_depth] = y;
    tree_depth++;
    y = y->right;
    while (y->left) {
      path[tree_depth] = y;
      tree_depth++;
      y = y->left;
    }
    /* Fix the parent's pointer... */
    if (z_depth == 0)
      root->root = y;
    else if (path[z_depth - 1]->left == path[z_depth])
      path[z_depth - 1]->left = y;
    else
      path[z_depth - 1]->right = y;
    /* Swap out the path pieces. */
    path[tree_depth] = path[z_depth];
    path[z_depth] = y;
    y = path[tree_depth];
    /* Swap out the child pointers */
    path[z_depth]->left = y->left;
    y->left = NULL;
    y->right = path[z_depth]->right;
    path[z_depth]->right = path[z_depth + 1];
    /* Fix the child pointer of the parent of the replacement */
    if (tree_depth > z_depth + 1)
      path[tree_depth - 1]->left = y;
    else
      path[tree_depth - 1]->right = y;
    /* Swap out the color */
    color = y->info & ST_COLOR;
    y->info = (y->info & ~ST_COLOR) | (path[z_depth]->info & ST_COLOR);
    path[z_depth]->info = (path[z_depth]->info & ~ST_COLOR) | color;
  }

  /* We are now looking at a node with less than two children */
  assert(!y->left || !y->right);
  /* Move the child (if any) up */
  if (y->left)
    x = y->left;
  else
    x = y->right;
  if (root->root == y)
    root->root = x;
  else if (path[tree_depth - 1]->left == y)
    path[tree_depth - 1]->left = x;
  else
    path[tree_depth - 1]->right = x;
  if ((y->info & ST_COLOR) == ST_BLACK) {
    while (x != root->root && (!x || (x->info & ST_COLOR) == ST_BLACK)) {
      if (x == path[tree_depth - 1]->left) {
        StrNode *w = path[tree_depth - 1]->right;
        assert(w);
        if (w && (w->info & ST_COLOR) == ST_RED) {
          w->info &= ~ST_RED;
          path[tree_depth - 1]->info |= ST_RED;
          st_left_rotate(tree_depth - 1, &root->root);
          path[tree_depth] = path[tree_depth - 1];
          path[tree_depth - 1] = w;
          tree_depth++;
          w = path[tree_depth - 1]->right;
          assert(w);
        }
        assert((w->info & ST_COLOR) == ST_BLACK);
        if ((!w->left || (w->left->info & ST_COLOR) == ST_BLACK) &&
            (!w->right || (w->right->info & ST_COLOR) == ST_BLACK)) {
          w->info |= ST_RED;
          x = path[tree_depth - 1];
          tree_depth--;
        } else {
          if (!w->right || (w->right->info & ST_COLOR) == ST_BLACK) {
            assert(w->left);
            w->left->info &= ~ST_RED;
            path[tree_depth] = w;
            st_right_rotate(tree_depth, &root->root);
            w = path[tree_depth - 1]->right;
            assert(w);
          }
          w->info =
            (w->info & ~ST_COLOR) | (path[tree_depth - 1]->info & ST_COLOR);
          path[tree_depth - 1]->info &= ~ST_RED;
          assert(w->right);
          w->right->info &= ~ST_RED;
          st_left_rotate(tree_depth - 1, &root->root);
          x = root->root;
        }
      } else {
        StrNode *w = path[tree_depth - 1]->left;
        assert(w);
        if (w && (w->info & ST_COLOR) == ST_RED) {
          w->info &= ~ST_RED;
          path[tree_depth - 1]->info |= ST_RED;
          st_right_rotate(tree_depth - 1, &root->root);
          path[tree_depth] = path[tree_depth - 1];
          path[tree_depth - 1] = w;
          tree_depth++;
          w = path[tree_depth - 1]->left;
          assert(w);
        }
        assert((w->info & ST_COLOR) == ST_BLACK);
        if ((!w->right || (w->right->info & ST_COLOR) == ST_BLACK) &&
            (!w->left || (w->left->info & ST_COLOR) == ST_BLACK)) {
          w->info |= ST_RED;
          x = path[tree_depth - 1];
          tree_depth--;
        } else {
          if (!w->left || (w->left->info & ST_COLOR) == ST_BLACK) {
            assert(w->right);
            w->right->info &= ~ST_RED;
            path[tree_depth] = w;
            st_left_rotate(tree_depth, &root->root);
            w = path[tree_depth - 1]->left;
            assert(w);
          }
          w->info =
            (w->info & ~ST_COLOR) | (path[tree_depth - 1]->info & ST_COLOR);
          path[tree_depth - 1]->info &= ~ST_RED;
          assert(w->left);
          w->left->info &= ~ST_RED;
          st_right_rotate(tree_depth - 1, &root->root);
          x = root->root;
        }
      }
    }
    if (x)
      x->info &= ~ST_RED;
  }
  root->mem -= strlen(s) + 1;
  mush_free(y, root->name);
  root->count--;
}

static void
st_node_walk(StrNode *node, STFunc callback, void *data)
{
  if (node->left)
    st_node_walk(node->left, callback, data);
  callback(node->string, node->info >> ST_COLOR, data);
  if (node->right)
    st_node_walk(node->right, callback, data);
}

/** Call a function for each node in the tree, in-order */
void
st_walk(StrTree *tree, STFunc callback, void *data)
{
  if (!tree || !tree->root)
    return;
  st_node_walk(tree->root, callback, data);
}

/* Print the tree, for debugging purposes. */
static void
st_print_tree(StrNode *node, int tree_depth, int lead)
{
  static char leader[ST_MAX_DEPTH * 2 + 1];
  char tmp;
  static StrNode *print_path[ST_MAX_DEPTH];
  int looped;

  if (tree_depth == 0)
    memset(leader, ' ', sizeof leader);

  print_path[tree_depth] = node;
  looped = tree_depth;
  while (looped--)
    if (print_path[looped] == node)
      break;
  looped++;

  if (node->left && !looped)
    st_print_tree(node->left, tree_depth + 1, '.');
  tmp = leader[tree_depth * 2];
  leader[tree_depth * 2] = '\0';
  printf("%s%c-+ %c %d %s%s\n", leader, lead,
         (node->info & ST_COLOR) ? 'r' : 'b', (int) node->info / ST_USE_STEP,
         node->string, looped ? " -LOOPING" : "");
  leader[tree_depth * 2] = ' ' + '|' - tmp;
  leader[0] = ' ';
  if (node->right && !looped)
    st_print_tree(node->right, tree_depth + 1, '`');
}

/** Print a string tree (for debugging).
 * \param root pointer to root of string tree.
 */
void
st_print(StrTree *root)
{
  printf("---- print\n");
  if (root->root)
    st_print_tree(root->root, 0, '-');
  printf("----\n");
}

static void st_depth_helper
  (StrNode *node, int *maxdepth, int *mindepth, int *avgdepth, int *leaves,
   int count);
static void
st_depth_helper(StrNode *node, int *maxdepth, int *mindepth,
                int *avgdepth, int *leaves, int count)
{
  if (!node)
    return;

  if (count > *maxdepth)
    *maxdepth = count;

  if (node->left) {
    /* Inner node */
    st_depth_helper(node->left, maxdepth, mindepth, avgdepth, leaves,
                    count + 1);
  }
  if (node->right) {
    /* Inner node */
    st_depth_helper(node->right, maxdepth, mindepth, avgdepth, leaves,
                    count + 1);
  }
  if (!node->left && !node->right) {    /* This is a leaf node */
    (*leaves)++;
    (*avgdepth) += count;
    if (*mindepth > count)
      *mindepth = count;
  }
}


/* Find the depth and number of permanment nodes */
static void
st_traverse_stats(StrNode *node, int *maxdepth, int *mindepth, int *avgdepth,
                  int *leaves)
{
  *maxdepth = 0;
  *mindepth = node ? (ST_MAX_DEPTH + 1) : 0;
  *avgdepth = 0;
  *leaves = 0;
  st_depth_helper(node, maxdepth, mindepth, avgdepth, leaves, 1);
  if (*avgdepth)
    *avgdepth = *avgdepth / *leaves;
}
