/**
 * \file comp_h.c
 *
 * \brief Huffman compression routines.
 *
 * One of several options for attribute compression. Usually the best.
 * Talek's rewrite of compress.c, using a Huffman compression routine.
 * This routine adds some time to the MUSH startup, since it reads a file 
 * in order to auto-tune the compression at each restart. The SAMPLE_SIZE
 * define can trade efficiency for speed.
 * This rewrite was inspired by Javelin's rewrite on a similar vein.
 * Most of the comments are his. The nasty ones are Talek's.
 *
 */

/* Compression routines */
#include "copyrite.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "dbio.h"
#include "confmagic.h"
#ifdef WIN32
#pragma warning( disable : 4244)        /* NJG: disable warning re conversion */
#endif

#define TABLE_SIZE      256     /**< allow all characters */
#define EOS             0       /**< use null code for end of string */
#define CHAR_BITS       8       /**< number of bits in char */
#define CHAR_MASK       255     /**< mask for just one char */
#define CODE_BITS       25      /**< max number of bits in code */
#ifndef SAMPLE_SIZE
#define SAMPLE_SIZE     0       /**< sample entire database */
#endif


/** Type for a huffman code. It must be at least CODE_BITS+CHAR_BITS-1
 * bits long.
 */
typedef unsigned long CType;

/** A node in the huffman compression tree. */
typedef struct cnode {
  struct cnode *left;           /**< Left child node. */
  struct cnode *right;          /**< Right child node. */
  unsigned char c;              /**< character at this node. */
} CNode;

static CNode *ctop;
static CType ctable[TABLE_SIZE];
static char ltable[TABLE_SIZE];

slab *huffman_slab = NULL;

static int fix_tree_depth(CNode *node, int height, int zeros);
static void add_ones(CNode *node);
static void build_ctable(CNode *root, CType code, int numbits);
int init_compress(PENNFILE *f);


/** Huffman-compress a string.
 * Compress a string: this is pretty easy. For each char in the string,
 * look up its code in ctable and add it to the compressed string we
 * build, keeping careful track of the number of bits we add.
 * Then stick the EOS character at the end.
 *
 * Important notes:
 *   This function mallocs memory that should be freed by the caller!
 *   The caller is also currently responsible for adding mem checks
 *   Don't use it to compress strings longer than BUFFER_LEN or the
 *     later uncompression will not go well.
 *
 * \param s string to be compressed.
 * \return newly allocated compressed string.
 */
unsigned char *
text_compress(const char *s)
{
  CType stage;
  int bits = 0;
  const unsigned char *p;
  unsigned char *b, *buf;
  int needed_length;

  /* Part 1 - how long will the compressed string be? */
  for (p = (const unsigned char *) s; p && *p; p++)
    bits += ltable[*p];
  bits += CHAR_BITS * 2 - 1;    /* add space for the ending \0 */
  needed_length = bits / CHAR_BITS;

  /* Part 2 - Actually get around to compressing the data... */
  p = (const unsigned char *) s;
  b = buf = (unsigned char *) malloc(needed_length);
  stage = 0;
  bits = 0;

  while (p && *p) {
    /* Put code on stage */
    stage |= ctable[*p] << bits;
    bits += ltable[*p];
    /* Put any full bytes of stage into the compressed string */
    while (bits >= CHAR_BITS) {
      *b++ = stage & CHAR_MASK;
      stage = stage >> CHAR_BITS;
      bits -= CHAR_BITS;
    }
    p++;
  }
  /* Put in EOS, and put the rest of the stage into the compressed string */
  /* This relies on EOS == 00000000 */
  bits += ltable[EOS] + CHAR_BITS - 1;
  while (bits >= CHAR_BITS) {
    *b++ = stage & CHAR_MASK;
    stage = stage >> CHAR_BITS;
    bits -= CHAR_BITS;
  }

  return buf;
}

/** Walk the huffman tree.
 * This macro is used for walking the compression tree.
 * It is a macro for efficiency.
 */
#define WALK_TREE(bitpos) \
do { \
  if (*p & (bitpos)) \
    node = node->right; \
  else \
    node = node->left; \
  if (!node->left && !node->right) { \
    /* Got a char */ \
    *b++ = node->c; \
    if (!*p || ((long)(b - buf) >= (long)(sizeof(buf) - 1))) { \
      *b++ = EOS; \
      return buf; \
    } \
    if (node->c == EOS) \
      return buf; \
    node = ctop; \
  } \
} while (0)

/** Huffman uncompress a string.
 * Uncompression is a snap, too. Go bit by bit, using the
 * bits to traverse the binary tree (0=left, 1=right) until reaching
 * a leaf node, which is the uncompressed character.
 * Stop when the leaf node turns out to be EOS.
 *
 * To avoid generating memory problems, this function should be
 * used with something of the format
 * \verbatim
 * char tbuf1[BUFFER_LEN];
 * strcpy(tbuf1, text_uncompress(a->value));
 * \endverbatim
 * if you are using something of type char *buff, use the
 * safe_uncompress function instead.
 *
 * \param s a compressed string.
 * \return a pointer to a static buffer containing the uncompressed string.
 */
char *
text_uncompress(const unsigned char *s)
{

  static char buf[BUFFER_LEN];
  const unsigned char *p;
  char *b;
  CNode *node;

  buf[0] = '\0';
  if (!s || !*s)
    return buf;
  p = s;
  b = buf;
  /* Finally start decompressing the string... */
  node = ctop;
  for (;;) {
    WALK_TREE(1);
    WALK_TREE(2);
    WALK_TREE(4);
    WALK_TREE(8);
    WALK_TREE(16);
    WALK_TREE(32);
    WALK_TREE(64);
    WALK_TREE(128);
    p++;
  }
}

/** Huffman uncompress a string, allocating memory.
 * this function should be used when you're doing something like
 * \verbatim
 * char *attrib = safe_uncompress(a->value);
 *
 * NEVER use it with something like
 *
 * char tbuf1[BUFFER_LEN]; 
 * strcpy(tbuf1, safe_uncompress(a->value));
 * \endverbatim
 * or you will create a horrendous memory leak.
 *
 * \param s compressed string to uncompress.
 * \return pointer to newly allocated string containing uncompressed text.
 */
char *
safe_uncompress(unsigned char const *s)
{
  return (char *) strdup((char *) uncompress(s));
}


static int
fix_tree_depth(CNode *node, int height, int zeros)
{
  int a, b;
  CNode *temp;

  if (!node)
    return height + (zeros > 2);
  a = fix_tree_depth(node->left, height + 1 + (zeros == 7), (zeros + 1) % 8);
  b = fix_tree_depth(node->right, height + 1, 0);
  if ((a > CODE_BITS) && (b < (a - 1))) {
#ifdef STANDALONE
    printf("Rotate right at depth %d.\n", height);
#endif
    temp = node->right;
    node->right = node->left;
    node->left = node->right->left;
    node->right->left = node->right->right;
    node->right->right = temp;
    a = fix_tree_depth(node->left, height + 1 + (zeros == 7), (zeros + 1) % 8);
    b = fix_tree_depth(node->right, height + 1, 0);
  } else if ((b > CODE_BITS) && (a < (b - 1))) {
#ifdef STANDALONE
    printf("Rotate left at depth %d.\n", height);
#endif
    temp = node->left;
    node->left = node->right;
    node->right = node->left->right;
    node->left->right = node->left->left;
    node->left->left = temp;
    a = fix_tree_depth(node->left, height + 1 + (zeros == 7), (zeros + 1) % 8);
    b = fix_tree_depth(node->right, height + 1, 0);
  }
  return ((a > b) ? a : b);
}

/* Add 1s to the tree, recursively */
static void
add_ones(CNode *node)
{
  int count;

  count = 0;
  do {
    if (node->right)
      add_ones(node->right);
    if ((count >= 7) || ((count >= 3) && !node->left && !node->right)) {
      ctop = slab_malloc(huffman_slab, node);
      if (!ctop) {
        do_rawlog(LT_ERR,
                  "Cannot allocate memory for compression tree. Aborting.");
        exit(1);
      }
      ctop->left = node->left;
      ctop->right = node->right;
      ctop->c = node->c;
      node->left = (CNode *) NULL;
      node->right = ctop;
      node = ctop;
      count = 0;
    }
    node = node->left;
    count++;
  } while (node);
}

/* Build ctable and ltable from the tree, recursively */
static void
build_ctable(CNode *root, CType code, int numbits)
{
#ifdef STANDALONE
  int i;
#endif

  if (!root->left && !root->right) {
    ctable[root->c] = code;
    ltable[root->c] = numbits;
#ifdef STANDALONE
    printf(isprint(root->c) ? "Code for '%c':\t" : "Code for %d:\t", root->c);
    for (i = 0; i < numbits; i++)
      printf("%d", (code >> i) & 1);
    printf("\n");
#endif
    if (numbits > CODE_BITS) {
      do_rawlog(LT_ERR, "Illegal compression code length (%d). Aborting.",
                numbits);
      exit(1);
    }
  } else {
    if (root->left)
      build_ctable(root->left, code | (0 << numbits), numbits + 1);
    if (root->right)
      build_ctable(root->right, code | (1 << numbits), numbits + 1);
  }
}

/** Initialize huffman compression.
 * Initialize the compression tree and table in 5 steps:
 * 1. Initialize arrays and things
 * 2. Read indb (up to SAMPLE_SIZE chars, if defined) and count
 *    the frequency of every character
 * 3. Cheat the relative frequency of some known special chars
 *    and upper-case letters
 * 4. Construct an (un)compression tree based on frequencies
 * 5. Construct a compression table by searching the tree
 * \param f filehandle to read from to build the tree.
 */
int
init_compress(PENNFILE *f)
{
  int total;
  unsigned char c;
  struct {
    long freq;
    CNode *node;
  } table[TABLE_SIZE];
  int indx, count;
  long temp;
  CNode *node;

#ifdef STANDALONE
  printf("init_compress: Part 1\n");
#endif

  huffman_slab = slab_create("huffman attribute compression", sizeof(CNode));
  slab_set_opt(huffman_slab, SLAB_ALLOC_BEST_FIT, 1);

  /* Part 1: initialize */
  for (total = 0; total < TABLE_SIZE; total++) {
    table[total].freq = 0;
    table[total].node = slab_malloc(huffman_slab, NULL);
    if (!table[total].node) {
      do_rawlog(LT_ERR,
                "Cannot allocate memory for compression tree. Aborting.");
      exit(1);
    }
    table[total].node->c = total;
    table[total].node->left = (CNode *) NULL;
    table[total].node->right = (CNode *) NULL;
  }

#ifdef STANDALONE
  printf("init_compress: Part 2\n");
#endif

  /* Part 2: count frequencies */
  if (f) {
    total = 0;
    while (!penn_feof(f) && (!SAMPLE_SIZE || (total++ < SAMPLE_SIZE))) {
      c = penn_fgetc(f);
      table[c].freq++;
    }
  }
#ifdef STANDALONE
  for (indx = 0; indx < TABLE_SIZE; indx++) {
    printf(isprint(indx) ? "Frequency for '%c': %d\n"
           : "Frequency for %d: %d\n", (unsigned char) indx, table[indx].freq);
  }
#endif

#ifdef STANDALONE
  printf("init_compress: Part 3\n");
#endif

  /* Part 3: Cheat the frequencies. Because there's a lot of wierd
   * stuff in indb (like ]'s and upper-case letters), we downplay it
   * by cutting frequencies. Actually, we shouldn't need to much.
   */

  /* The ']' character is artificially raised by being the
   * start-of-attribute marker in indb.  Set it back to '[',
   * which it should be balancing...
   */
  table[']'].freq = table['['].freq;

  /* The DEL character is returned once for no apparent reason (I think
   * it is returned at EOF), so remove that one count...
   */
  if (table[255].freq)
    table[255].freq--;

  /* Newlines really aren't all that common in the attributes, so
   * chop the value substantially.
   */
  table['\n'].freq /= 16;

#ifdef STANDALONE
  printf("init_compress: Part 4(a)\n");
#endif

  /* Part 4(a): Sort the table.  I'm using a stupid insert sort here
   * because this only gets called once, and I don't want to conform
   * to the qsort interface.  NOTE: I don't sort in EOS.
   */
  for (indx = 2; indx < TABLE_SIZE; indx++) {
    for (count = indx;
         (count > 1) && (table[count - 1].freq < table[count].freq); count--) {
      temp = table[count].freq;
      table[count].freq = table[count - 1].freq;
      table[count - 1].freq = temp;
      node = table[count].node;
      table[count].node = table[count - 1].node;
      table[count - 1].node = node;
    }
  }

#ifdef STANDALONE
  printf("init_compress: Part 4(b)\n");
#endif

  /* Part 4(b): Now we've got a list sorted from most freq (table[0]) to
   * least freq. We build a binary tree by traversing the list, making
   * a subtree out of the two least frequent nodes (creating a parent
   * node whose frequency is the sum of its children's frequency),
   * and reinserting the subtree's parent into the list. We repeat
   * until there's only one node in the list, the root of the tree.
   * NOTE: I'm still not dealing with EOS.
   */
  for (indx = TABLE_SIZE - 1; indx > 0; indx--) {
#ifdef NEVER
    printf("Freq. table:\n");
    for (count = indx; count >= 0; count--)
      printf("%3d: %d\t", table[count].node->c, table[count].freq);
    printf("\n");
#endif
    node = slab_malloc(huffman_slab, table[indx].node);
    if (!node) {
      do_rawlog(LT_ERR,
                "Cannot allocate memory for compression tree. Aborting.");
      exit(1);
    }
    node->left = table[indx].node;
    node->right = table[indx - 1].node;
    table[indx - 1].freq += table[indx].freq;
    table[indx - 1].node = node;
    for (count = indx - 1;
         (count > 1) && (table[count - 1].freq <= table[count].freq); count--) {
      temp = table[count].freq;
      table[count].freq = table[count - 1].freq;
      table[count - 1].freq = temp;
      node = table[count].node;
      table[count].node = table[count - 1].node;
      table[count - 1].node = node;
    }
  }

#ifdef NEVER
  build_ctable(table[1].node, 0, 0);
#endif

#ifdef STANDALONE
  printf("init_compress: Part 4(c)\n");
#endif

  /* Part 4(c): If necessary, squash the tree so that it obeys
   * the code length limitations (CODE_BITS et all).  This is
   * done recursively, with rotations.
   */
  (void) fix_tree_depth(table[1].node, 0, 2);

#ifdef STANDALONE
  printf("init_compress: Part 4(d)\n");
#endif

  /* Part 4(d): It is now time to insure that sequences of eight 0s
   * never occur in the output data, because having nulls in the
   * output would royally confuse strcpy et all.
   */

  /* Force a 1 at fifth position on the left edge of tree. (Or terminating
   * 1 for the all 0 code.)
   */
  node = table[1].node;         /* top of tree */
  for (count = 0; node->left && (count < 4); count++)
    node = node->left;
  ctop = slab_malloc(huffman_slab, node);
  if (!ctop) {
    do_rawlog(LT_ERR, "Cannot allocate memory for compression tree. Aborting.");
    exit(1);
  }
  ctop->left = node->left;
  ctop->right = node->right;
  ctop->c = node->c;
  node->left = (CNode *) NULL;
  node->right = ctop;

  /* Recursively descend tree adding 1s where needed. */

  add_ones(table[1].node);

#ifdef STANDALONE
  printf("init_compress: Part 4(e)\n");
#endif

  /* Part 4(e): Finally add in EOS as 00000000.
   */
  node = table[1].node;         /* top of tree */
  for (count = 0; count < 8; count++) {
    if (!node->left) {
      ctop = slab_malloc(huffman_slab, node);
      if (!ctop) {
        do_rawlog(LT_ERR,
                  "Cannot allocate memory for compression tree. Aborting.");
        exit(1);
      }
      ctop->left = (CNode *) NULL;
      ctop->right = (CNode *) NULL;
      ctop->c = EOS;
      node->left = ctop;
    }
    node = node->left;
  }

#ifdef STANDALONE
  printf("init_compress: Part 5\n");
#endif

  /* Part 5: Now traverse the tree, depth-first, and construct
   * the compression table.
   */

  ctop = table[1].node;
  build_ctable(ctop, 0, 0);

#ifdef STANDALONE
  printf("init_compress: Done\n");
#endif

  /* Whew */
  return 0;
}

#ifdef STANDALONE
void
main(argc, argv)
    int argc;
    char *argv[];
{
  FILE *input;
  unsigned char buffer[BUFFER_LEN];
  unsigned char otherbuf[BUFFER_LEN];
  unsigned char newbuffer[BUFFER_LEN];
  unsigned char *p1, *p2;
  int count;

  if ((input = fopen(argv[1], "rb")) == NULL) {
    printf("Can't open %s.\n", argv[1]);
    exit(1);
  }
  init_compress(input);
  fclose(input);
  do {
    printf("Enter text: ");
    fgets(buffer, 4095, stdin);
    if ((buffer[0] == '\n') || (buffer[0] == '\r') || (buffer[0] == '\0'))
      exit(0);
    printf("Text: %s!\n", buffer);
    printf("Compressing\n");
    strcpy(otherbuf, compress(buffer));
    printf("Compressed: ");
    p1 = otherbuf;
    while (p1 && *p1) {
      for (count = 0; count < 8; count++)
        printf("%d", (*p1 >> count) & 1);
      p1++;
    }
    printf("\n");
    printf("Length: %d, Complength: %d\n", strlen(buffer), strlen(otherbuf));
    printf("Uncompressing\n");
    strcpy(newbuffer, uncompress(otherbuf));
    printf("Text: %s!\n", newbuffer);
    printf("Strcoll(orig,uncomp) = %d\n", strcoll(newbuffer, buffer));
    printf("strlen(orig) = %d, strlen(uncomp) = %d\n", strlen(buffer),
           strlen(newbuffer));
    p1 = buffer;
    p2 = newbuffer;
/*
 * while (p1 && p2 && *p1 && *p2) {
 * if (*p1 != *p2) printf("Unequal: %d and %d\n",*p1,*p2);
 * else printf("Equal: %c and %c\n",*p1,*p2);
 * p1++; p2++;
 * }
 */
    strcpy(newbuffer, otherbuf);
/*
 * printf("Trying safe.\n");
 * buf = safe_uncompress(newbuffer);
 * printf("Safe uncompress: %s!\n",buf);
 */
  } while (1);
}
#endif
