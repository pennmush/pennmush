/**
 * \file dbio.h
 *
 * \brief header files for functions for reading/writing database files
 */

#ifndef __DBIO_H
#define __DBIO_H

#include <setjmp.h>
#include <stdio.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

extern jmp_buf db_err;

typedef struct pennfile {
  enum { PFT_FILE, PFT_PIPE, PFT_GZFILE } type;
  union {
    FILE *f;
#ifdef HAVE_LIBZ
    gzFile g;
#endif
  } handle;
} PENNFILE;


PENNFILE *penn_fopen(const char *, const char *);
void penn_fclose(PENNFILE *);

int penn_fgetc(PENNFILE *);
char *penn_fgets(char *, int, PENNFILE *);
int penn_fputc(int, PENNFILE *);
int penn_fputs(const char *, PENNFILE *);
int penn_fprintf(PENNFILE *, const char *fmt, ...)
  __attribute__ ((__format__(__printf__, 2, 3)));
int penn_ungetc(int, PENNFILE *);

int penn_feof(PENNFILE *);

/* Output */
void putref(PENNFILE *f, long int ref);
void putstring(PENNFILE *f, const char *s);
void db_write_labeled_string(PENNFILE *f, char const *label, char const *value);
void db_write_labeled_int(PENNFILE *f, char const *label, int value);
void db_write_labeld_uint32(PENNFILE *, char const *, uint32_t);
void db_write_labeled_dbref(PENNFILE *f, char const *label, dbref value);

dbref db_write(PENNFILE *f, int flag);
int db_paranoid_write(PENNFILE *f, int flag);

/* Input functions */
char *getstring_noalloc(PENNFILE *f);
long getref(PENNFILE *f);
void db_read_this_labeled_string(PENNFILE *f, const char *label, char **val);
void db_read_labeled_string(PENNFILE *f, char **label, char **val);
void db_read_this_labeled_int(PENNFILE *f, const char *label, int *val);
void db_read_this_labeled_uint32(PENNFILE *f, const char *lable, uint32_t *val);
void db_read_labeled_int(PENNFILE *f, char **label, int *val);
void db_read_labeled_uint32(PENNFILE *f, char **label, uint32_t *val);
void db_read_this_labeled_dbref(PENNFILE *f, const char *label, dbref *val);
void db_read_labeled_dbref(PENNFILE *f, char **label, dbref *val);


dbref db_read(PENNFILE *f);


#endif
