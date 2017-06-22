#ifndef CHARCONV_H
#define CHARCONV_H

/* Basic functions for converting strings between character sets. */

bool valid_utf8(const char *);

char *latin1_to_utf8(const char *, int, int *, bool) __attribute_malloc__;
char *utf8_to_latin1(const char *) __attribute_malloc__;

#endif
