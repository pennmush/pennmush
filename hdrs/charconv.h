#ifndef CHARCONV_H
#define CHARCONV_H

/* Basic functions for converting strings between character sets. */

bool valid_utf8(const char *);

char *latin1_to_utf8(const char * RESTRICT, int, int *, const char * RESTRICT) __attribute_malloc__;
char *latin1_to_utf8_tn(const char * RESTRICT, int, int *, bool, const char * RESTRICT) __attribute_malloc__;
char *utf8_to_latin1(const char * RESTRICT, int *, const char * RESTRICT) __attribute_malloc__;

#endif
