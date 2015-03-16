/**
 * \file hash_function.c
 *
 * \brief Hash functions for hash tables.
 *
 * The canonical implementation for CityHash, Murmur3, and SpookyHash are
 * written in C++. The following C versions are derived from the canonical
 * implementations. In general, the derivations follow the coding style of the
 * canonical implementation, even when it differs from Penn's, in order to make
 * it easier to port future changes.
 */

#include "copyrite.h"
#include "hash_function.h"

#include <string.h>

#if defined(_M_X64) || defined(__x86_64__)
/* We assume that x86-64 provides relatively fast unaligned reads. */
#define ARCH_X86_64 1
#else
#define ARCH_X86_64 0
#endif

/* CityHash: https://code.google.com/p/cityhash/.
 *
 * Copyright (c) 2011 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * CityHash, by Geoff Pike and Jyrki Alakuijala
 *
 * This file provides a few functions for hashing strings. On x86-64
 * hardware in 2011, CityHash64() is faster than other high-quality
 * hash functions, such as Murmur.  This is largely due to higher
 * instruction-level parallelism.  CityHash64() and CityHash128() also perform
 * well on hash-quality tests.
 *
 * CityHash128() is optimized for relatively long strings and returns
 * a 128-bit hash.  For strings more than about 2000 bytes it can be
 * faster than CityHash64().
 *
 * Functions in the CityHash family are not suitable for cryptography.
 *
 * WARNING: This code has not been tested on big-endian platforms!
 * It is known to work well on little-endian platforms that have a small penalty
 * for unaligned reads, such as current Intel and AMD moderate-to-high-end CPUs.
 *
 * By the way, for some hash functions, given strings a and b, the hash
 * of a+b is easily derived from the hashes of a and b.  This property
 * doesn't hold for any hash functions in this file.
 */

struct city_uint128 {
  uint64_t first;
  uint64_t second;
};

static inline uint64_t
city_Uint128Low64(const struct city_uint128 *x)
{
  return x->first;
}

static inline uint64_t
city_Uint128High64(const struct city_uint128 *x)
{
  return x->second;
}

/* Hash 128 input bits down to 64 bits of output.
 * This is intended to be a reasonably good hash function. */
static inline uint64_t
city_Hash128to64(const struct city_uint128 *x)
{
  /* Murmur-inspired hashing. */
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t a, b;
  a = (city_Uint128Low64(x) ^ city_Uint128High64(x)) * kMul;
  a ^= (a >> 47);
  b = (city_Uint128High64(x) ^ a) * kMul;
  b ^= (b >> 47);
  b *= kMul;
  return b;
}

static uint64_t
CITY_UNALIGNED_LOAD64(const char *p)
{
  uint64_t result;
  memcpy(&result, p, sizeof(result));
  return result;
}

static uint32_t
CITY_UNALIGNED_LOAD32(const char *p)
{
  uint32_t result;
  memcpy(&result, p, sizeof(result));
  return result;
}

#ifdef _MSC_VER

#include <stdlib.h>
#define city_bswap_32(x) _byteswap_ulong(x)
#define city_bswap_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__)

/* Mac OS X / Darwin features */
#include <libkern/OSByteOrder.h>
#define city_bswap_32(x) OSSwapInt32(x)
#define city_bswap_64(x) OSSwapInt64(x)

#elif defined (__FreeBSD__)

#include <sys/endian.h>
#define city_bswap_32(x) bswap32(x)
#define city_bswap_64(x) bswap64(x)

#elif defined (__OpenBSD__)

#include <sys/types.h>
#define city_bswap_32(x) swap32(x)
#define city_bswap_64(x) swap64(x)

#elif defined(__NetBSD__)

#include <sys/types.h>
#include <machine/bswap.h>

#define city_bswap_32(x) bswap32(x)
#define city_bswap_64(x) bswap64(x)

#elif defined(HAVE_BYTESWAP_H)

/* Linux header */
#include <byteswap.h>
#define city_bswap_32(x) bswap_32(x)
#define city_bswap_64(x) bswap_64(x)

#else

static inline uint16_t __attribute__ ((__const__))
  city_bswap_16(uint16_t hword)
{
  uint16_t low, hi;

  low = hword & 0xFF;
  hi = hword >> 8;

  return hi + (low << 8);
}

static inline uint32_t __attribute__ ((__const__))
  city_bswap_32(uint32_t word)
{
  uint32_t low, hi;

  low = word & 0xFFFF;
  hi = word >> 16;

  return city_bswap_16(hi) + ((uint32_t) city_bswap_16(low) << 16);
}

static inline uint64_t __attribute__ ((__const__))
  city_bswap_64(uint64_t dword)
{
  uint64_t low, hi;

  low = dword & 0xFFFFFFFF;
  hi = dword >> 32;

  return city_bswap_32(hi) + ((uint64_t) city_bswap_32(low) << 32);
}

#endif

#ifdef WORDS_BIGENDIAN
#define city_uint32_in_expected_order(x) (city_bswap_32(x))
#define city_uint64_in_expected_order(x) (city_bswap_64(x))
#else
#define city_uint32_in_expected_order(x) (x)
#define city_uint64_in_expected_order(x) (x)
#endif

#if !defined(LIKELY)
#if HAVE_BUILTIN_EXPECT
#define LIKELY(x) (__builtin_expect(!!(x), 1))
#else
#define LIKELY(x) (x)
#endif
#endif

static uint64_t
city_Fetch64(const char *p)
{
  return city_uint64_in_expected_order(CITY_UNALIGNED_LOAD64(p));
}

static uint32_t
city_Fetch32(const char *p)
{
  return city_uint32_in_expected_order(CITY_UNALIGNED_LOAD32(p));
}

/* Some primes between 2^63 and 2^64 for various uses. */
static const uint64_t city_k0 = 0xc3a5c85c97cb3127ULL;
static const uint64_t city_k1 = 0xb492b66fbe98f273ULL;
static const uint64_t city_k2 = 0x9ae16a3b2f90404fULL;

/* Bitwise right rotate.  Normally this will compile to a single
 * instruction, especially if the shift is a manifest constant. */
static uint64_t
city_Rotate(uint64_t val, int shift)
{
  /* Avoid shifting by 64: doing so yields an undefined result. */
  return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

static uint64_t
city_ShiftMix(uint64_t val)
{
  return val ^ (val >> 47);
}

static uint64_t
city_HashLen16(uint64_t u, uint64_t v)
{
  struct city_uint128 x = { u, v };
  return city_Hash128to64(&x);
}

static uint64_t
city_HashLen16WithMul(uint64_t u, uint64_t v, uint64_t mul)
{
  /* Murmur-inspired hashing. */
  uint64_t a, b;
  a = (u ^ v) * mul;
  a ^= (a >> 47);
  b = (v ^ a) * mul;
  b ^= (b >> 47);
  b *= mul;
  return b;
}

static uint64_t
city_HashLen0to16(const char *s, size_t len)
{
  if (len >= 8) {
    uint64_t mul, a, b, c, d;
    mul = city_k2 + len * 2;
    a = city_Fetch64(s) + city_k2;
    b = city_Fetch64(s + len - 8);
    c = city_Rotate(b, 37) * mul + a;
    d = (city_Rotate(a, 25) + b) * mul;
    return city_HashLen16WithMul(c, d, mul);
  }
  if (len >= 4) {
    uint64_t mul, a;
    mul = city_k2 + len * 2;
    a = city_Fetch32(s);
    return city_HashLen16WithMul(len + (a << 3), city_Fetch32(s + len - 4),
                                 mul);
  }
  if (len > 0) {
    uint8_t a, b, c;
    uint32_t y, z;
    a = s[0];
    b = s[len >> 1];
    c = s[len - 1];
    y = (uint32_t) (a) + ((uint32_t) (b) << 8);
    z = len + ((uint32_t) (c) << 2);
    return city_ShiftMix(y * city_k2 ^ z * city_k0) * city_k2;
  }
  return city_k2;
}

/* This probably works well for 16-byte strings as well, but it may be overkill
 * in that case. */
static uint64_t
city_HashLen17to32(const char *s, size_t len)
{
  uint64_t mul, a, b, c, d;
  mul = city_k2 + len * 2;
  a = city_Fetch64(s) * city_k1;
  b = city_Fetch64(s + 8);
  c = city_Fetch64(s + len - 8) * mul;
  d = city_Fetch64(s + len - 16) * city_k2;
  return city_HashLen16WithMul(city_Rotate(a + b, 43) + city_Rotate(c, 30) + d,
                               a + city_Rotate(b + city_k2, 18) + c, mul);
}

/* Return a 16-byte hash for 48 bytes.  Quick and dirty.
 * Callers do best to use "random-looking" values for a and b. */
static void
city_WeakHashLen32WithSeedsHelper(uint64_t w, uint64_t x, uint64_t y,
                                  uint64_t z, uint64_t a, uint64_t b,
                                  uint64_t *h1, uint64_t *h2)
{
  uint64_t c;
  a += w;
  b = city_Rotate(b + a + z, 21);
  c = a;
  a += x;
  a += y;
  b += city_Rotate(a, 44);
  *h1 = a + z;
  *h2 = b + c;
}

/* Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty. */
static void
city_WeakHashLen32WithSeeds(const char *s, uint64_t a, uint64_t b, uint64_t *h1,
                            uint64_t *h2)
{
  city_WeakHashLen32WithSeedsHelper(city_Fetch64(s),
                                    city_Fetch64(s + 8),
                                    city_Fetch64(s + 16),
                                    city_Fetch64(s + 24), a, b, h1, h2);
}

/* Return an 8-byte hash for 33 to 64 bytes. */
static uint64_t
city_HashLen33to64(const char *s, size_t len)
{
  uint64_t mul, a, b, c, d, e, f, g, h, u, v, w, x, y, z;
  mul = city_k2 + len * 2;
  a = city_Fetch64(s) * city_k2;
  b = city_Fetch64(s + 8);
  c = city_Fetch64(s + len - 24);
  d = city_Fetch64(s + len - 32);
  e = city_Fetch64(s + 16) * city_k2;
  f = city_Fetch64(s + 24) * 9;
  g = city_Fetch64(s + len - 8);
  h = city_Fetch64(s + len - 16) * mul;
  u = city_Rotate(a + g, 43) + (city_Rotate(b, 30) + c) * 9;
  v = ((a + g) ^ d) + f + 1;
  w = city_bswap_64((u + v) * mul) + h;
  x = city_Rotate(e + f, 42) + c;
  y = (city_bswap_64((v + w) * mul) + g) * mul;
  z = e + f + c;
  a = city_bswap_64((x + z) * mul + y) + b;
  b = city_ShiftMix((z + a) * mul + d + h) * mul;
  return b + x;
}

uint64_t
city_CityHash64(const char *s, size_t len)
{
  uint64_t x, y, z, v1, v2, w1, w2;
  if (len <= 32) {
    if (len <= 16) {
      return city_HashLen0to16(s, len);
    } else {
      return city_HashLen17to32(s, len);
    }
  } else if (len <= 64) {
    return city_HashLen33to64(s, len);
  }

  /* For strings over 64 bytes we hash the end first, and then as we
   * loop we keep 56 bytes of state: v, w, x, y, and z. */
  x = city_Fetch64(s + len - 40);
  y = city_Fetch64(s + len - 16) + city_Fetch64(s + len - 56);
  z =
    city_HashLen16(city_Fetch64(s + len - 48) + len,
                   city_Fetch64(s + len - 24));
  city_WeakHashLen32WithSeeds(s + len - 64, len, z, &v1, &v2);
  city_WeakHashLen32WithSeeds(s + len - 32, y + city_k1, x, &w1, &w2);
  x = x * city_k1 + city_Fetch64(s);

  /* Decrease len to the nearest multiple of 64, and operate on 64-byte chunks. */
  len = (len - 1) & ~(size_t) (63);
  do {
    uint64_t temp;
    x = city_Rotate(x + y + v1 + city_Fetch64(s + 8), 37) * city_k1;
    y = city_Rotate(y + v2 + city_Fetch64(s + 48), 42) * city_k1;
    x ^= w2;
    y += v1 + city_Fetch64(s + 40);
    z = city_Rotate(z + w1, 33) * city_k1;
    city_WeakHashLen32WithSeeds(s, v2 * city_k1, x + w1, &v1, &v2);
    city_WeakHashLen32WithSeeds(s + 32, z + w2, y + city_Fetch64(s + 16), &w1,
                                &w2);
    temp = z;
    z = x;
    x = temp;
    s += 64;
    len -= 64;
  } while (len != 0);
  return city_HashLen16(city_HashLen16(v1, w1) + city_ShiftMix(y) * city_k1 + z,
                        city_HashLen16(v2, w2) + x);
}

uint64_t
city_CityHash64WithSeeds(const char *s, size_t len,
                         uint64_t seed0, uint64_t seed1)
{
  return city_HashLen16(city_CityHash64(s, len) - seed0, seed1);
}

uint32_t
city_hash(const char *s, int len, uint64_t seed)
{
  return city_CityHash64WithSeeds(s, len, city_k2, seed);
}
