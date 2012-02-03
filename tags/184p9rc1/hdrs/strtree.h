/**
 * \file strtree.h
 *
 * \brief String trees.
 */



#ifndef _STRTREE_H_
#define _STRTREE_H_

/* Here we have the tree node structure.  Pretty basic
 * parentless binary tree.  info holds the red/black
 * property and the usage count.  This structure is
 * rarely fully allocated; instead, only enough is
 * allocated to hold the pointers, info, and the null
 * terminated string.
 */
typedef struct strnode StrNode;

/** A strtree node.
 * This is a node in a red/black binary strtree.
 */
struct strnode {
  StrNode *left;                /**< Pointer to left child */
  StrNode *right;               /**< Pointer to right child */
  uint32_t info;           /**< Red/black and other internal state */
  char string[BUFFER_LEN];      /**< Node label (value) */
};

typedef struct strtree StrTree;
/** A strtree.
 * A red/black binary tree of strings.
 */
struct strtree {
  StrNode *root;        /**< Pointer to root node */
  const char *name;     /**< For tracking memory use */
  size_t count;         /**< Number of nodes in the tree */
  size_t mem;           /**< Memory used by the tree */
};

void st_init(StrTree *root, const char *name);
char const *st_insert(char const *s, StrTree *root);
char const *st_find(char const *s, StrTree *root);
void st_delete(char const *s, StrTree *root);
void st_print(StrTree *root);
typedef void (*STFunc) (const char *, int, void *);
void st_walk(StrTree *, STFunc, void *);
void st_flush(StrTree *root);

extern long st_count;

#endif
