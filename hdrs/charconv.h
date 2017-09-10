#ifndef CHARCONV_H
#define CHARCONV_H

/* Basic functions for converting strings between character sets. */

bool valid_utf8(const char *);
char* normalized_utf8(const char *, size_t, size_t *) __attribute_malloc__;
size_t latin1_as_utf8_bytes(const char *, size_t);

char *latin1_to_utf8(const char *, size_t, size_t *, bool) __attribute_malloc__;
char *utf8_to_latin1(const char *, size_t *, bool) __attribute_malloc__;

#endif
