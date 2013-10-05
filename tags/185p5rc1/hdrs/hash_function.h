/**
 * \file hash_function.h
 *
 * \brief Hash functions for hash tables.
 */

#ifndef __HASH_FUNCTION_H
#define __HASH_FUNCTION_H

#include "copyrite.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif                          /* HAVE_STDINT_H_ */

uint32_t city_hash(const char *buf, int len, uint64_t seed);
uint32_t murmur3_hash(const char *key, int len, uint64_t seed);
uint32_t spooky_hash(const char *message, int len, uint64_t seed);
uint32_t jenkins_hash(const char *k, int len, uint64_t seed);

#endif                          /* __HASH_FUNCTION_H_ */
