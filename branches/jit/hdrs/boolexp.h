#ifndef BOOLEXP_H
#define BOOLEXP_H

#ifdef HAVE_JIT_JIT_H
#include <jit/jit.h>
#endif

#include "copyrite.h"
#include "chunk.h"
#include "dbio.h"

typedef chunk_reference_t boolexp;

/* tokens for locks */
#define NOT_TOKEN '!'
#define AND_TOKEN '&'
#define OR_TOKEN '|'
#define AT_TOKEN '@'
#define IN_TOKEN '+'
#define IS_TOKEN '='
#define OWNER_TOKEN '$'

enum { TRUE_BOOLEXP = NULL_CHUNK_REFERENCE };

/* From boolexp.c */
boolexp dup_bool(boolexp b);
int sizeof_boolexp(boolexp b);
int eval_boolexp(dbref player, boolexp b, dbref target);
boolexp parse_boolexp(dbref player, const char *buf, lock_type ltype);
boolexp parse_boolexp_d(dbref player, const char *buf, lock_type ltype,
                               int derefs);
void free_boolexp(boolexp b);
boolexp getboolexp(PENNFILE *f, const char *ltype);
void putboolexp(PENNFILE *f, boolexp b);
enum u_b_f {
  UB_ALL, /**< Use names of objects */
  UB_DBREF, /**< Use dbrefs */
  UB_MEREF /**< Use dbrefs or "me" if the object is the player arg
              from unparse_boolexp.() For @decompile. */
};
char *unparse_boolexp(dbref player, boolexp b, enum u_b_f flag);

#ifdef USE_JIT
jit_function_t compile_boolexp(dbref thing, boolexp b);

struct string_pool;
struct lock_jit_metadata {
  jit_context_t context;
  struct string_pool *pool;
  int nfuns;
};

void free_string_pool(struct string_pool *);

#endif
#endif                          /* BOOLEXP_H */
