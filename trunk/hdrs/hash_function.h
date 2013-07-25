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

/* TODO: Use better hash functions such as CityHash or MurmurHash. */
uint32_t jenkins_hash(const char *k, int len);
uint32_t hsieh_hash(const char *data, int len);
uint32_t fnv_hash(const char *str, int len);
uint32_t penn_hash(const char *key, int len);

#endif  /* __HASH_FUNCTION_H_ */
