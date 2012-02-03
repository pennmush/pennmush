/**
 * \file boolexp.h
 *
 */

#ifndef BOOLEXP_H
#define BOOLEXP_H
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
#define ATR_TOKEN ':'
#define EVAL_TOKEN '/'
#define FLAG_TOKEN '^'

enum { TRUE_BOOLEXP = NULL_CHUNK_REFERENCE };

/* From boolexp.c */
boolexp dup_bool(boolexp b);
int sizeof_boolexp(boolexp b);
int eval_boolexp(dbref player, boolexp b, dbref target, NEW_PE_INFO *pe_info);
boolexp parse_boolexp(dbref player, const char *buf, lock_type ltype);
boolexp parse_boolexp_d(dbref player, const char *buf, lock_type ltype,
                        int derefs);
void free_boolexp(boolexp b);
boolexp getboolexp(PENNFILE *f, const char *ltype);
void putboolexp(PENNFILE *f, boolexp b);
/** Flags which set how an object in a boolexp is
 * displayed to a player in various commands
 */
enum u_b_f {
  UB_ALL, /**< Use names of objects */
  UB_DBREF, /**< Use dbrefs */
  UB_MEREF /**< Use dbrefs or "me" if the object is the player arg
              from unparse_boolexp(). For \@decompile. */
};
char *unparse_boolexp(dbref player, boolexp b, enum u_b_f flag);
boolexp cleanup_boolexp(boolexp);

bool is_eval_lock(boolexp b);

#endif                          /* BOOLEXP_H */
