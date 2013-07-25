/**
 * \file hash_function.h
 *
 * \brief Hash functions for hash tables.
 */

#ifndef __HASH_FUNCTION_H
#define __HASH_FUNCTION_H

#include "config.h"
#include "copyrite.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif  /* HAVE_STDINT_H_ */

/* Note that CityHash doesn't have an implementation that returns a 32-bit hash.
 * This is just CityHash64 using only the lowest 32 bits. */
uint32_t city_hash(const char *buf, int len);
/* TODO: Conditionally compile x86 or x64 version depending on platform. */
uint32_t murmur3_x86_32(const char *key, int len);
uint32_t spooky_hash32(const char *message, int len);
uint32_t jenkins_hash(const char *k, int len);

#endif  /* __HASH_FUNCTION_H_ */
