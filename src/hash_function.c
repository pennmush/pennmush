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

/* Hash 128 input bits down to 64 bits of output.
 * This is intended to be a reasonably good hash function. */
static inline uint64_t city_hash_128_to_64(const uint64_t x, const uint64_t y) {
  /* Murmur-inspired hashing. */
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t a, b;
  a = (x ^ y) * kMul;
  a ^= (a >> 47);
  b = (y ^ a) * kMul;
  b ^= (b >> 47);
  b *= kMul;
  return b;
}

static uint64_t CITY_UNALIGNED_LOAD64(const char *p) {
  uint64_t result;
  memcpy(&result, p, sizeof(result));
  return result;
}

static uint32_t CITY_UNALIGNED_LOAD32(const char *p) {
  uint32_t result;
  memcpy(&result, p, sizeof(result));
  return result;
}

#ifndef __BIG_ENDIAN__

#define city_uint32_in_expected_order(x) (x)
#define city_uint64_in_expected_order(x) (x)

#else

#ifdef _MSC_VER
#include <stdlib.h>
#define city_bswap_32(x) _byteswap_ulong(x)
#define city_bswap_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__)
// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define city_bswap_32(x) OSSwapInt32(x)
#define city_bswap_64(x) OSSwapInt64(x)

#else
#include <byteswap.h>
#endif

#define city_uint32_in_expected_order(x) (city_bswap_32(x))
#define city_uint64_in_expected_order(x) (city_bswap_64(x))

#endif  // __BIG_ENDIAN__

static uint64_t city_fetch_64(const char *p) {
  return city_uint64_in_expected_order(CITY_UNALIGNED_LOAD64(p));
}

static uint32_t city_fetch_32(const char *p) {
  return city_uint32_in_expected_order(CITY_UNALIGNED_LOAD32(p));
}

/* Some primes between 2^63 and 2^64 for various uses. */
static const uint64_t city_k0 = 0xc3a5c85c97cb3127ULL;
static const uint64_t city_k1 = 0xb492b66fbe98f273ULL;
static const uint64_t city_k2 = 0x9ae16a3b2f90404fULL;
static const uint64_t city_k3 = 0xc949d7c7509e6557ULL;

/* Bitwise right rotate.  Normally this will compile to a single
 * instruction, especially if the shift is a manifest constant. */
static uint64_t city_rotate(uint64_t val, int shift) {
  /* Avoid shifting by 64: doing so yields an undefined result. */
  return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

/* Equivalent to Rotate(), but requires the second arg to be non-zero.
 * On x86-64, and probably others, it's possible for this to compile
 * to a single instruction if both args are already in registers. */
static uint64_t city_rotate_by_at_least_1(uint64_t val, int shift) {
  return (val >> shift) | (val << (64 - shift));
}

static uint64_t city_shift_mix(uint64_t val) {
  return val ^ (val >> 47);
}

static uint64_t city_hash_len_16(uint64_t u, uint64_t v) {
  return city_hash_128_to_64(u, v);
}

static uint64_t city_hash_len_0_to_16(const char *s, int len) {
  if (len > 8) {
    uint64_t a, b;
    a = city_fetch_64(s);
    b = city_fetch_64(s + len - 8);
    return city_hash_len_16(a, city_rotate_by_at_least_1(b + len, len)) ^ b;
  }
  if (len >= 4) {
    uint64_t a = city_fetch_32(s);
    return city_hash_len_16(len + (a << 3), city_fetch_32(s + len - 4));
  }
  if (len > 0) {
    uint8_t a, b, c;
    uint32_t y, z;
    a = s[0];
    b = s[len >> 1];
    c = s[len - 1];
    y = (uint32_t)(a) + ((uint32_t)(b) << 8);
    z = len + ((uint32_t)(c) << 2);
    return city_shift_mix(y * city_k2 ^ z * city_k3) * city_k2;
  }
  return city_k2;
}

/* This probably works well for 16-byte strings as well, but it may be overkill
 * in that case. */
static uint64_t city_hash_len_17_to_32(const char* s, int len) {
  uint64_t a, b, c, d;
  a = city_fetch_64(s) * city_k1;
  b = city_fetch_64(s + 8);
  c = city_fetch_64(s + len - 8) * city_k2;
  d = city_fetch_64(s + len - 16) * city_k0;
  return city_hash_len_16(city_rotate(a - b, 43) + city_rotate(c, 30) + d,
                          a + city_rotate(b ^ city_k3, 20) - c + len);
}

/* Return a 16-byte hash for 48 bytes.  Quick and dirty.
 * Callers do best to use "random-looking" values for a and b. */
static void city_weak_hash_len_32_with_seeds_helper(
    uint64_t w, uint64_t x, uint64_t y, uint64_t z, uint64_t a, uint64_t b,
    uint64_t *h1, uint64_t *h2) {
  uint64_t c;
  a += w;
  b = city_rotate(b + a + z, 21);
  c = a;
  a += x;
  a += y;
  b += city_rotate(a, 44);
  *h1 = a + z;
  *h2 = b + c;
}

/* Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty. */
static void city_weak_hash_len_32_with_seeds(
    const char *s, uint64_t a, uint64_t b, uint64_t *h1, uint64_t *h2) {
  return city_weak_hash_len_32_with_seeds_helper(
      city_fetch_64(s),
      city_fetch_64(s + 8),
      city_fetch_64(s + 16),
      city_fetch_64(s + 24),
      a,
      b,
      h1,
      h2);
}

/* Return an 8-byte hash for 33 to 64 bytes. */
static uint64_t city_hash_len_33_to_64(const char *s, int len) {
  uint64_t z, a, b, c, vf, vs, wf, ws, r;
  z = city_fetch_64(s + 24);
  a = city_fetch_64(s) + (len + city_fetch_64(s + len - 16)) * city_k0;
  b = city_rotate(a + z, 52);
  c = city_rotate(a, 37);
  a += city_fetch_64(s + 8);
  c += city_rotate(a, 7);
  a += city_fetch_64(s + 16);
  vf = a + z;
  vs = b + city_rotate(a, 31) + c;
  a = city_fetch_64(s + 16) + city_fetch_64(s + len - 32);
  z = city_fetch_64(s + len - 8);
  b = city_rotate(a + z, 52);
  c = city_rotate(a, 37);
  a += city_fetch_64(s + len - 24);
  c += city_rotate(a, 7);
  a += city_fetch_64(s + len - 16);
  wf = a + z;
  ws = b + city_rotate(a, 31) + c;
  r = city_shift_mix((vf + ws) * city_k2 + (wf + vs) * city_k0);
  return city_shift_mix(r * city_k0 + vs) * city_k2;
}

static uint64_t city_hash_64(const char *s, int len) {
  uint64_t x, y, z, v1, v2, w1, w2;
  if (len <= 32) {
    if (len <= 16) {
      return city_hash_len_0_to_16(s, len);
    } else {
      return city_hash_len_17_to_32(s, len);
    }
  } else if (len <= 64) {
    return city_hash_len_33_to_64(s, len);
  }

  /* For strings over 64 bytes we hash the end first, and then as we
   * loop we keep 56 bytes of state: v, w, x, y, and z. */
  x = city_fetch_64(s + len - 40);
  y = city_fetch_64(s + len - 16) + city_fetch_64(s + len - 56);
  z = city_hash_len_16(city_fetch_64(s + len - 48) + len, city_fetch_64(s + len - 24));
  city_weak_hash_len_32_with_seeds(s + len - 64, len, z, &v1, &v2);
  city_weak_hash_len_32_with_seeds(s + len - 32, y + city_k1, x, &w1, &w2);
  x = x * city_k1 + city_fetch_64(s);

  /* Decrease len to the nearest multiple of 64, and operate on 64-byte chunks. */
  len = (len - 1) & ~63;
  do {
    uint64_t temp;
    x = city_rotate(x + y + v1 + city_fetch_64(s + 8), 37) * city_k1;
    y = city_rotate(y + v2 + city_fetch_64(s + 48), 42) * city_k1;
    x ^= w2;
    y += v1 + city_fetch_64(s + 40);
    z = city_rotate(z + w1, 33) * city_k1;
    city_weak_hash_len_32_with_seeds(s, v2 * city_k1, x + w1, &v1, &v2);
    city_weak_hash_len_32_with_seeds(s + 32, z + w2, y + city_fetch_64(s + 16), &w1, &w2);
    temp = x;
    x = z;
    z = temp;
    s += 64;
    len -= 64;
  } while (len != 0);
  return city_hash_len_16(city_hash_len_16(v1, w1) + city_shift_mix(y) * city_k1 + z,
                          city_hash_len_16(v2, w2) + x);
}

uint32_t
city_hash(const char *buf, int len)
{
  return (uint32_t)city_hash_64(buf, len);
}

/* MurmurHash3: https://code.google.com/p/smhasher/
 *
 *-----------------------------------------------------------------------------
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.

 * Note - The x86 and x64 versions do _not_ produce the same results, as the
 * algorithms are optimized for their respective platforms. You can still
 * compile and run any of them on any platform, but your performance with the
 * non-native version will be less than optimal.
 */

/*-----------------------------------------------------------------------------
 * Platform-specific functions and macros */

/* Microsoft Visual Studio */

#if defined(_MSC_VER)

#define MURMUR_FORCE_INLINE    __forceinline

#include <stdlib.h>

#define MURMUR_ROTL32(x,y)     _rotl(x,y)
#define MURMUR_ROTL64(x,y)     _rotl64(x,y)

#define MURMUR_BIG_CONSTANT(x) (x)

/* Other compilers */

#else   /* defined(_MSC_VER) */

#define MURMUR_FORCE_INLINE inline __attribute__((always_inline))

static inline uint32_t murmur_rotl32 ( uint32_t x, int8_t r )
{
  return (x << r) | (x >> (32 - r));
}

static inline uint64_t murmur_rotl64 ( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}

#define MURMUR_ROTL32(x,y)     murmur_rotl32(x,y)
#define MURMUR_ROTL64(x,y)     murmur_rotl64(x,y)

#define MURMUR_BIG_CONSTANT(x) (x##LLU)

#endif // !defined(_MSC_VER)

/*-----------------------------------------------------------------------------
 * Block read - if your platform needs to do endian-swapping or can only
 * handle aligned reads, do the conversion here */

MURMUR_FORCE_INLINE uint32_t murmur_getblock32 ( const uint32_t * p, int i )
{
  return p[i];
}

/*-----------------------------------------------------------------------------
 * Finalization mix - force all bits of a hash block to avalanche */

MURMUR_FORCE_INLINE uint32_t murmur_fmix32 ( uint32_t h )
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

/* Random bits courtesy of random.org. */
static const uint32_t murmur_seed = 0x17315286;

uint32_t
murmur3_x86_32(const char *key, int len)
{
  const uint8_t * data = (const uint8_t *)key;
  const int nblocks = len / 4;
  const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);
  const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

  uint32_t h1, k1;
  int i;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  h1 = murmur_seed;

  /*----------
   * body */

  for(i = -nblocks; i; i++)
  {
    k1 = murmur_getblock32(blocks,i);

    k1 *= c1;
    k1 = MURMUR_ROTL32(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = MURMUR_ROTL32(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  /*----------
   * tail */

  k1 = 0;

  switch(len & 3)
  {
  case 3: k1 ^= tail[2] << 16;
  case 2: k1 ^= tail[1] << 8;
  case 1: k1 ^= tail[0];
          k1 *= c1; k1 = MURMUR_ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;

  h1 = murmur_fmix32(h1);

  return h1;
}

/* SpookyHash: http://burtleburtle.net/bob/hash/spooky.html
 *
 * SpookyHash: a 128-bit noncryptographic hash function
 * By Bob Jenkins, public domain
 *   Oct 31 2010: alpha, framework + SpookyHash::Mix appears right
 *   Oct 31 2011: alpha again, Mix only good to 2^^69 but rest appears right
 *   Dec 31 2011: beta, improved Mix, tested it for 2-bit deltas
 *   Feb  2 2012: production, same bits as beta
 *   Feb  5 2012: adjusted definitions of uint* to be more portable
 * 
 * Up to 4 bytes/cycle for long messages.  Reasonably fast for short messages.
 * All 1 or 2 bit deltas achieve avalanche within 1% bias per output bit.
 *
 * This was developed for and tested on 64-bit x86-compatible processors.
 * It assumes the processor is little-endian.  There is a macro
 * controlling whether unaligned reads are allowed (by default they are).
 * This should be an equally good hash on big-endian machines, but it will
 * compute different results on them than on little-endian machines.
 *
 * Google's CityHash has similar specs to SpookyHash, and CityHash is faster
 * on some platforms.  MD4 and MD5 also have similar specs, but they are orders
 * of magnitude slower.  CRCs are two or more times slower, but unlike 
 * SpookyHash, they have nice math for combining the CRCs of pieces to form 
 * the CRCs of wholes.  There are also cryptographic hashes, but those are even 
 * slower than MD5. */

#ifdef _MSC_VER
# define SPOOKY_INLINE __forceinline
#else
# define SPOOKY_INLINE inline
#endif

/* number of uint64's in internal state */
static const int spooky_numVars = 12;

/* size of the internal state */
static const int spooky_blockSize = 96 /*spooky_numVars*8*/;

/* size of buffer of unhashed data, in bytes */
static const int spooky_bufSize = 192 /*2*spooky_blockSize*/;

/*
 * sc_const: a constant which:
 *  * is not zero
 *  * is odd
 *  * is a not-very-regular mix of 1's and 0's
 *  * does not need any other special mathematical properties
 */

static const uint64_t spooky_const = 0xdeadbeefdeadbeefULL;

/*
 * left rotate a 64-bit value by k bytes
 */
static SPOOKY_INLINE uint64_t spooky_rot64(uint64_t x, int k)
{
  return (x << k) | (x >> (64 - k));
}

/*
 * This is used if the input is 96 bytes long or longer.
 *
 * The internal state is fully overwritten every 96 bytes.
 * Every input bit appears to cause at least 128 bits of entropy
 * before 96 other bytes are combined, when run forward or backward
 *   For every input bit,
 *   Two inputs differing in just that input bit
 *   Where "differ" means xor or subtraction
 *   And the base value is random
 *   When run forward or backwards one Mix
 * I tried 3 pairs of each; they all differed by at least 212 bits.
 */
static SPOOKY_INLINE void spooky_mix(
    const uint64_t *data, 
    uint64_t *s0, uint64_t *s1, uint64_t *s2, uint64_t *s3,
    uint64_t *s4, uint64_t *s5, uint64_t *s6, uint64_t *s7,
    uint64_t *s8, uint64_t *s9, uint64_t *s10,uint64_t *s11)
{
  *s0 += data[0];    *s2 ^= *s10;   *s11 ^= *s0;   *s0 = spooky_rot64(*s0,11);    *s11 += *s1;
  *s1 += data[1];    *s3 ^= *s11;   *s0 ^= *s1;    *s1 = spooky_rot64(*s1,32);    *s0 += *s2;
  *s2 += data[2];    *s4 ^= *s0;    *s1 ^= *s2;    *s2 = spooky_rot64(*s2,43);    *s1 += *s3;
  *s3 += data[3];    *s5 ^= *s1;    *s2 ^= *s3;    *s3 = spooky_rot64(*s3,31);    *s2 += *s4;
  *s4 += data[4];    *s6 ^= *s2;    *s3 ^= *s4;    *s4 = spooky_rot64(*s4,17);    *s3 += *s5;
  *s5 += data[5];    *s7 ^= *s3;    *s4 ^= *s5;    *s5 = spooky_rot64(*s5,28);    *s4 += *s6;
  *s6 += data[6];    *s8 ^= *s4;    *s5 ^= *s6;    *s6 = spooky_rot64(*s6,39);    *s5 += *s7;
  *s7 += data[7];    *s9 ^= *s5;    *s6 ^= *s7;    *s7 = spooky_rot64(*s7,57);    *s6 += *s8;
  *s8 += data[8];    *s10 ^= *s6;   *s7 ^= *s8;    *s8 = spooky_rot64(*s8,55);    *s7 += *s9;
  *s9 += data[9];    *s11 ^= *s7;   *s8 ^= *s9;    *s9 = spooky_rot64(*s9,54);    *s8 += *s10;
  *s10 += data[10];  *s0 ^= *s8;    *s9 ^= *s10;   *s10 = spooky_rot64(*s10,22);  *s9 += *s11;
  *s11 += data[11];  *s1 ^= *s9;    *s10 ^= *s11;  *s11 = spooky_rot64(*s11,46);  *s10 += *s0;
}

/*
 * Mix all 12 inputs together so that h0, h1 are a hash of them all.
 *
 * For two inputs differing in just the input bits
 * Where "differ" means xor or subtraction
 * And the base value is random, or a counting value starting at that bit
 * The final result will have each bit of h0, h1 flip
 * For every input bit,
 * with probability 50 +- .3%
 * For every pair of input bits,
 * with probability 50 +- 3%
 *
 * This does not rely on the last Mix() call having already mixed some.
 * Two iterations was almost good enough for a 64-bit result, but a
 * 128-bit result is reported, so End() does three iterations.
 */
static SPOOKY_INLINE void spooky_endPartial(
    uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3,
    uint64_t *h4, uint64_t *h5, uint64_t *h6, uint64_t *h7, 
    uint64_t *h8, uint64_t *h9, uint64_t *h10,uint64_t *h11)
{
  *h11+= *h1;    *h2 ^= *h11;   *h1 = spooky_rot64(*h1,44);
  *h0 += *h2;    *h3 ^= *h0;    *h2 = spooky_rot64(*h2,15);
  *h1 += *h3;    *h4 ^= *h1;    *h3 = spooky_rot64(*h3,34);
  *h2 += *h4;    *h5 ^= *h2;    *h4 = spooky_rot64(*h4,21);
  *h3 += *h5;    *h6 ^= *h3;    *h5 = spooky_rot64(*h5,38);
  *h4 += *h6;    *h7 ^= *h4;    *h6 = spooky_rot64(*h6,33);
  *h5 += *h7;    *h8 ^= *h5;    *h7 = spooky_rot64(*h7,10);
  *h6 += *h8;    *h9 ^= *h6;    *h8 = spooky_rot64(*h8,13);
  *h7 += *h9;    *h10^= *h7;    *h9 = spooky_rot64(*h9,38);
  *h8 += *h10;   *h11^= *h8;    *h10= spooky_rot64(*h10,53);
  *h9 += *h11;   *h0 ^= *h9;    *h11= spooky_rot64(*h11,42);
  *h10+= *h0;    *h1 ^= *h10;   *h0 = spooky_rot64(*h0,54);
}

static SPOOKY_INLINE void spooky_end(
    uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3,
    uint64_t *h4, uint64_t *h5, uint64_t *h6, uint64_t *h7, 
    uint64_t *h8, uint64_t *h9, uint64_t *h10,uint64_t *h11)
{
    spooky_endPartial(h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
    spooky_endPartial(h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
    spooky_endPartial(h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
}

/*
 * The goal is for each bit of the input to expand into 128 bits of 
 *   apparent entropy before it is fully overwritten.
 * n trials both set and cleared at least m bits of h0 h1 h2 h3
 *   n: 2   m: 29
 *   n: 3   m: 46
 *   n: 4   m: 57
 *   n: 5   m: 107
 *   n: 6   m: 146
 *   n: 7   m: 152
 * when run forwards or backwards
 * for all 1-bit and 2-bit diffs
 * with diffs defined by either xor or subtraction
 * with a base of all zeros plus a counter, or plus another bit, or random
*/
static SPOOKY_INLINE void spooky_shortMix(
    uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3)
{
  *h2 = spooky_rot64(*h2,50);  *h2 += *h3;  *h0 ^= *h2;
  *h3 = spooky_rot64(*h3,52);  *h3 += *h0;  *h1 ^= *h3;
  *h0 = spooky_rot64(*h0,30);  *h0 += *h1;  *h2 ^= *h0;
  *h1 = spooky_rot64(*h1,41);  *h1 += *h2;  *h3 ^= *h1;
  *h2 = spooky_rot64(*h2,54);  *h2 += *h3;  *h0 ^= *h2;
  *h3 = spooky_rot64(*h3,48);  *h3 += *h0;  *h1 ^= *h3;
  *h0 = spooky_rot64(*h0,38);  *h0 += *h1;  *h2 ^= *h0;
  *h1 = spooky_rot64(*h1,37);  *h1 += *h2;  *h3 ^= *h1;
  *h2 = spooky_rot64(*h2,62);  *h2 += *h3;  *h0 ^= *h2;
  *h3 = spooky_rot64(*h3,34);  *h3 += *h0;  *h1 ^= *h3;
  *h0 = spooky_rot64(*h0,5);   *h0 += *h1;  *h2 ^= *h0;
  *h1 = spooky_rot64(*h1,36);  *h1 += *h2;  *h3 ^= *h1;
}

/*
 * Mix all 4 inputs together so that h0, h1 are a hash of them all.
 *
 * For two inputs differing in just the input bits
 * Where "differ" means xor or subtraction
 * And the base value is random, or a counting value starting at that bit
 * The final result will have each bit of h0, h1 flip
 * For every input bit,
 * with probability 50 +- .3% (it is probably better than that)
 * For every pair of input bits,
 * with probability 50 +- .75% (the worst case is approximately that)
 */
static SPOOKY_INLINE void spooky_shortEnd(
    uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3)
{
  *h3 ^= *h2;  *h2 = spooky_rot64(*h2,15);  *h3 += *h2;
  *h0 ^= *h3;  *h3 = spooky_rot64(*h3,52);  *h0 += *h3;
  *h1 ^= *h0;  *h0 = spooky_rot64(*h0,26);  *h1 += *h0;
  *h2 ^= *h1;  *h1 = spooky_rot64(*h1,51);  *h2 += *h1;
  *h3 ^= *h2;  *h2 = spooky_rot64(*h2,28);  *h3 += *h2;
  *h0 ^= *h3;  *h3 = spooky_rot64(*h3,9);   *h0 += *h3;
  *h1 ^= *h0;  *h0 = spooky_rot64(*h0,47);  *h1 += *h0;
  *h2 ^= *h1;  *h1 = spooky_rot64(*h1,54);  *h2 += *h1;
  *h3 ^= *h2;  *h2 = spooky_rot64(*h2,32);  *h3 += *h2;
  *h0 ^= *h3;  *h3 = spooky_rot64(*h3,25);  *h0 += *h3;
  *h1 ^= *h0;  *h0 = spooky_rot64(*h0,63);  *h1 += *h0;
}

#define SPOOKY_ALLOW_UNALIGNED_READS 1

/*
 * short hash ... it could be used on any message, 
 * but it's used by Spooky just for short messages.
 */
void spooky_short(
    const char* message,
    int length,
    uint64_t *hash1,
    uint64_t *hash2)
{
  uint64_t buf[spooky_numVars];
  union 
  { 
      const uint8_t *p8; 
      uint32_t *p32;
      uint64_t *p64; 
      size_t i; 
  } u;
  size_t remainder;
  uint64_t a, b, c, d;

  u.p8 = (const uint8_t *)message;
  
  if (!SPOOKY_ALLOW_UNALIGNED_READS && (u.i & 0x7))
  {
      memcpy(buf, message, length);
      u.p64 = buf;
  }

  remainder = length%32;
  a=*hash1;
  b=*hash2;
  c=spooky_const;
  d=spooky_const;

  if (length > 15)
  {
      const uint64_t *end = u.p64 + (length/32)*4;
      
      // handle all complete sets of 32 bytes
      for (; u.p64 < end; u.p64 += 4)
      {
          c += u.p64[0];
          d += u.p64[1];
          spooky_shortMix(&a,&b,&c,&d);
          a += u.p64[2];
          b += u.p64[3];
      }
      
      //Handle the case of 16+ remaining bytes.
      if (remainder >= 16)
      {
          c += u.p64[0];
          d += u.p64[1];
          spooky_shortMix(&a,&b,&c,&d);
          u.p64 += 2;
          remainder -= 16;
      }
  }
  
  // Handle the last 0..15 bytes, and its length
  d = ((uint64_t)length) << 56;
  switch (remainder)
  {
  case 15:
    d += ((uint64_t)u.p8[14]) << 48;
  case 14:
    d += ((uint64_t)u.p8[13]) << 40;
  case 13:
    d += ((uint64_t)u.p8[12]) << 32;
  case 12:
    d += u.p32[2];
    c += u.p64[0];
    break;
  case 11:
  d += ((uint64_t)u.p8[10]) << 16;
  case 10:
    d += ((uint64_t)u.p8[9]) << 8;
  case 9:
    d += (uint64_t)u.p8[8];
  case 8:
    c += u.p64[0];
    break;
  case 7:
    c += ((uint64_t)u.p8[6]) << 48;
  case 6:
    c += ((uint64_t)u.p8[5]) << 40;
  case 5:
    c += ((uint64_t)u.p8[4]) << 32;
  case 4:
    c += u.p32[0];
    break;
  case 3:
    c += ((uint64_t)u.p8[2]) << 16;
  case 2:
    c += ((uint64_t)u.p8[1]) << 8;
  case 1:
    c += (uint64_t)u.p8[0];
    break;
  case 0:
    c += spooky_const;
    d += spooky_const;
  }
  spooky_shortEnd(&a,&b,&c,&d);
  *hash1 = a;
  *hash2 = b;
}

static void spooky_hash128(
    const char *message,
    int length,
    uint64_t *hash1,
    uint64_t *hash2)
{
  uint64_t h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11;
  uint64_t buf[spooky_numVars];
  uint64_t *end;
  union
  {
    const uint8_t *p8;
    uint64_t *p64;
    /* Editor's note: The author indicates this was developed on an x64 machine. */
    /* TODO: It's possible this should be explicitly sized. */
    size_t i;
  } u;
  size_t remainder;

  if (length < spooky_bufSize)
  {
    spooky_short(message, length, hash1, hash2);
    return;
  }

  h0=h3=h6=h9  = *hash1;
  h1=h4=h7=h10 = *hash2;
  h2=h5=h8=h11 = spooky_const;

  u.p8 = (const uint8_t *)message;
  end = u.p64 + (length/spooky_blockSize)*spooky_numVars;

  /* handle all whole sc_blockSize blocks of bytes */
  if (SPOOKY_ALLOW_UNALIGNED_READS || ((u.i & 0x7) == 0))
  {
    while (u.p64 < end)
    {
      spooky_mix(u.p64, &h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
      u.p64 += spooky_numVars;
    }
  }
  else
  {
    while (u.p64 < end)
    {
      memcpy(buf, u.p64, spooky_blockSize);
      spooky_mix(buf, &h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
      u.p64 += spooky_numVars;
    }
  }

  /* handle the last partial block of sc_blockSize bytes */
  remainder = (length - ((const uint8_t *)end-(const uint8_t *)message));
  memcpy(buf, end, remainder);
  memset(((uint8_t *)buf)+remainder, 0, spooky_blockSize-remainder);
  ((uint8_t *)buf)[spooky_blockSize-1] = remainder;
  spooky_mix(buf, &h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
  
  /* do some final mixing */
  spooky_end(&h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
  *hash1 = h0;
  *hash2 = h1;
}

/* Random bits courtesy of random.org. */
static const uint64_t spooky_seed = 0x9425efc78760e259LLU;

uint32_t
spooky_hash32(const char *message, int length)
{
  uint64_t hash1 = spooky_seed, hash2 = spooky_seed;
  spooky_hash128(message, length, &hash1, &hash2);
  return (uint32_t)hash1;
}

/* The Jenkins hash:  http://burtleburtle.net/bob/hash/evahash.html */

/* The mixing step */
#define mix(a,b,c) \
{ \
  a=a-b;  a=a-c;  a=a^(c>>13); \
  b=b-c;  b=b-a;  b=b^(a<<8);  \
  c=c-a;  c=c-b;  c=c^(b>>13); \
  a=a-b;  a=a-c;  a=a^(c>>12); \
  b=b-c;  b=b-a;  b=b^(a<<16); \
  c=c-a;  c=c-b;  c=c^(b>>5);  \
  a=a-b;  a=a-c;  a=a^(c>>3);  \
  b=b-c;  b=b-a;  b=b^(a<<10); \
  c=c-a;  c=c-b;  c=c^(b>>15); \
}

/* The whole new hash function */
uint32_t
jenkins_hash(const char *k, int len)
{
  uint32_t a, b, c;             /* the internal state */
  uint32_t length;              /* how many key bytes still need mixing */
  static const uint32_t initval = 5432;  /* the previous hash, or an arbitrary value */

  /* Set up the internal state */
  length = len;
  a = b = 0x9e3779b9;           /* the golden ratio; an arbitrary value */
  c = initval;                  /* variable initialization of internal state */

   /*---------------------------------------- handle most of the key */
  while (len >= 12) {
    a =
      a + (k[0] + ((uint32_t) k[1] << 8) + ((uint32_t) k[2] << 16) +
           ((uint32_t) k[3] << 24));
    b =
      b + (k[4] + ((uint32_t) k[5] << 8) + ((uint32_t) k[6] << 16) +
           ((uint32_t) k[7] << 24));
    c =
      c + (k[8] + ((uint32_t) k[9] << 8) + ((uint32_t) k[10] << 16) +
           ((uint32_t) k[11] << 24));
    mix(a, b, c);
    k = k + 12;
    len = len - 12;
  }

   /*------------------------------------- handle the last 11 bytes */
  c = c + length;
  switch (len) {                /* all the case statements fall through */
  case 11:
    c = c + ((uint32_t) k[10] << 24);
  case 10:
    c = c + ((uint32_t) k[9] << 16);
  case 9:
    c = c + ((uint32_t) k[8] << 8);
    /* the first byte of c is reserved for the length */
  case 8:
    b = b + ((uint32_t) k[7] << 24);
  case 7:
    b = b + ((uint32_t) k[6] << 16);
  case 6:
    b = b + ((uint32_t) k[5] << 8);
  case 5:
    b = b + k[4];
  case 4:
    a = a + ((uint32_t) k[3] << 24);
  case 3:
    a = a + ((uint32_t) k[2] << 16);
  case 2:
    a = a + ((uint32_t) k[1] << 8);
  case 1:
    a = a + k[0];
    /* case 0: nothing left to add */
  }
  mix(a, b, c);
   /*-------------------------------------------- report the result */
  return c;
}
