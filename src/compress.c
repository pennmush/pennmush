/**
 * \file compress.c
 *
 * \brief Compression routine wrapper file for PennMUSH.
 *
 * This file does nothing but conditionally include the appropriate
 * attribute compression source code.
 *
 */

#include "copyrite.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "log.h"
#include "mushtype.h"
#include "dbio.h"
#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "mymalloc.h"


typedef bool (*init_fn)(PENNFILE *);
typedef char* (*comp_fn)(char const *);
		      
struct compression_ops {
  init_fn init;
  comp_fn comp;
  comp_fn decomp;
};

#include "comp_h.c"
#include "comp_w8.c"

static bool
dummy_init(PENNFILE *f __attribute__((__unused__))) {
  return 1;
}

static char dummy_buff[BUFFER_LEN];

static char *
dummy_compress(char const *s) {
  return strdup(s);
}

static char *
dummy_decompress(char const *s) {
  return strcpy(dummy_buff, s);
}

struct compression_ops nocompression_ops = {
  dummy_init,
  dummy_compress,
  dummy_decompress
};

struct compression_ops *comp_ops = NULL;

bool
init_compress(PENNFILE *f) {
  if (comp_ops == NULL) {
    if (strcmp(options.attr_compression, "none") == 0)
      comp_ops = &nocompression_ops;
    else if (strcmp(options.attr_compression, "huffman") == 0)
      comp_ops = &huffman_ops;
    else if (strcmp(options.attr_compression, "word") == 0)
      comp_ops = &word_ops;
    else {
      /* Unknown option! */
      do_rawlog(LT_ERR, "Unknown compression option '%s'. Defaulting to none.", options.attr_compression);
      comp_ops = &nocompression_ops;
    }
  }

  return comp_ops->init(f);
}

__attribute_malloc__ char *
text_compress(char const *s)  {
  return comp_ops->comp(s);
}

char *
text_uncompress(char const *s) {
  return comp_ops->decomp(s);
}

__attribute_malloc__ char *
safe_uncompress(char const *s)  {
  return strdup(comp_ops->decomp(s));
}
