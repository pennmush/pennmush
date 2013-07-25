/**
 * \file hash_function.c
 *
 * \brief Hash functions for hash tables.
 */

#include "hash_function.h"

#include "copyrite.h"

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


/* The Hsieh hash function. See
   http://www.azillionmonkeys.com/qed/hash.html */

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t
hsieh_hash(const char *data, int len)
{
  uint32_t hash, tmp;
  int rem;

  hash = len;

  if (len <= 0)
    return 0;

  rem = len & 3;
  len >>= 2;

  /* Main loop */
  for (; len > 0; len--) {
    hash += get16bits(data);
    tmp = (get16bits(data + 2) << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    data += 2 * sizeof(uint16_t);
    hash += hash >> 11;
  }

  /* Handle end cases */
  switch (rem) {
  case 3:
    hash += get16bits(data);
    hash ^= hash << 16;
    hash ^= data[sizeof(uint16_t)] << 18;
    hash += hash >> 11;
    break;
  case 2:
    hash += get16bits(data);
    hash ^= hash << 11;
    hash += hash >> 17;
    break;
  case 1:
    hash += *data;
    hash ^= hash << 10;
    hash += hash >> 1;
  }

  /* Force "avalanching" of final 127 bits */
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;

  return hash;
}

/* FNV hash: http://isthe.com/chongo/tech/comp/fnv/ */
/*
 * fnv_32_str - perform a 32 bit Fowler/Noll/Vo hash on a string
 *
 * input:
 *	str	- string to hash
 *	hval	- previous hash value or 0 if first call
 *
 * returns:
 *	32 bit hash as a static hash type
 *
 * NOTE: To use the 32 bit FNV-0 historic hash, use FNV0_32_INIT as the hval
 *	 argument on the first call to either fnv_32_buf() or fnv_32_str().
 *
 * NOTE: To use the recommended 32 bit FNV-1 hash, use FNV1_32_INIT as the hval
 *	 argument on the first call to either fnv_32_buf() or fnv_32_str().
 *
 * Modified by Raevnos: Takes length arg, no hval arg, and does array-style
 * iteration of the string.
 */
#define FNV_32_PRIME ((Fnv32_t)0x01000193)
uint32_t
fnv_hash(const char *str, int len)
{
  const unsigned char *s = (const unsigned char *) str;
  int n;
  uint32_t hval = 0;

  /*
   * FNV-1 hash each octet in the buffer
   */
  for (n = 0; n < len; n++) {

    /* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(NO_FNV_GCC_OPTIMIZATION)
    hval *= FNV_32_PRIME;
#else
    hval +=
      (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
#endif

    /* xor the bottom with the current octet */
    hval ^= (uint32_t) s[n];
  }

  /* return our new hash value */
  return hval;
}

/* Silly old Penn hash function. */
uint32_t
penn_hash(const char *key, int len)
{
  uint32_t hash = 0;
  int n;

  if (!key || !*key || len == 0)
    return 0;
  for (n = 0; n < len; n++)
    hash = (hash << 5) + hash + key[n];
  return hash;
}

