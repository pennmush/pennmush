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
#endif /* HAVE_STDINT_H_ */

uint32_t city_hash(const char *buf, int len, uint64_t seed);

#endif /* __HASH_FUNCTION_H_ */
