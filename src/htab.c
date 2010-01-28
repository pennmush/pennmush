/**
 * \file htab.c
 *
 * \brief Hashtable routines.
 *
 * The hash tables here implement open addressing using cuckoo hashing
 * to resolve collisions. This gives an O(1) worse-case performance
 * (As well as best, of course), compared to the worst-case O(N) of
 * chained or linear probing tables.
 * 
 * A lookup will require at most X hash functions and string
 * comparisions.  The old tables had, with data used by Penn, 1 hash
 * function and up to 6 or 7 comparisions in the worst case. Best case
 * for both is 1 hash and 1 comparision. Cuckoo hashing comes out
 * ahead when most lookups are successful; true for normal usage in
 * Penn.
 *
 * Insertions are more expensive, but that's okay because we do a lot
 * fewer of those.
 *
 * For details on the technique, see
 * http://citeseer.ist.psu.edu/pagh01cuckoo.html
 *
 * Essentially: Use X hash functions (3 for us), and when inserting,
 * try them in order looking for an empty bucket. If none are found,
 * use one of the full buckets for the new entry, and bump the old one
 * to another one of its possible buckets. Repeat the bumping some
 * bounded number of times (Otherwise the possiblity of an infinite
 * loop arises), and if no empty buckets are found, try rehashing the
 * entire table with a new set of hash functions. If those are
 * exhausted, only then grow the table.
 *
 * Possible to-do: Switch the string tree implementation from using
 * red-black trees to these tables. Talek choose binary trees over
 * hash tables when writing strtree.c because of the better worst-case
 * behavior, which was a good decision at the time. However, O(1) is
 * better than O(log N).
 *
 * At the moment, though, insertions can be fairly costly. The growth
 * factor should be able to be specified -- large for cases where fast
 * inserts are important, small for cases where we want to save save.
 */

#include "config.h"
#include "copyrite.h"
#include <string.h>
#include <math.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAS_OPENSSL
#include <openssl/bn.h>
#endif
#include "conf.h"
#include "externs.h"

#include "htab.h"
#include "mymalloc.h"
#include "confmagic.h"

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
static uint32_t
jenkins_hash(const char *k, int len)
{
  uint32_t a, b, c;             /* the internal state */
  uint32_t length;              /* how many key bytes still need mixing */
  static uint32_t initval = 5432;       /* the previous hash, or an arbitrary value */

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

static uint32_t
hsieh_hash(const char *data, int len)
{
  uint32_t hash, tmp;
  int rem;

  hash = len;

  if (len <= 0 || data == NULL)
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
static inline uint32_t
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
static uint32_t
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

typedef uint32_t (*hash_func) (const char *, int);

hash_func hash_functions[] = {
  hsieh_hash,
  fnv_hash,
  jenkins_hash,
  penn_hash,
  hsieh_hash,
  fnv_hash,
  penn_hash,
  jenkins_hash
};

enum { NHASH_TRIES = 3, NHASH_MOD = 8 };

/* Return the next prime number after its arg */
static unsigned int
next_prime_after(unsigned int val)
{
#ifdef HAS_OPENSSL
  /* Calculate primes on the fly using OpenSSL. Takes up less space
     than using a table, deals better with pathologically large tables. */
  static BIGNUM *p = NULL;
  static BN_CTX *ctx = NULL;

  if (!ctx)
    ctx = BN_CTX_new();
  if (!p)
    p = BN_new();

  /* Make sure we only try odd numbers; evens can't be primes. */
  if (val & 0x1)
    val += 2;
  else
    val += 1;

  while (1) {
    BN_set_word(p, val);
    if (BN_is_prime(p, BN_prime_checks, NULL, ctx, NULL) > 0)
      break;
    val += 2;
  }

  return val;

#else
  /* For non-SSL systems; use a static table of primes that should be more than big enough. */
  /* Most of the first thousand primes */
  static unsigned int primes[] = {
    7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79,
    83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157,
    163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239,
    241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331,
    337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421,
    431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509,
    521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613,
    617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709,
    719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821,
    823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919,
    929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013, 1019,
    1021, 1031, 1033, 1039, 1049, 1051, 1061, 1063, 1069, 1087, 1091, 1093,
    1097, 1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163, 1171, 1181, 1187,
    1193, 1201, 1213, 1217, 1223, 1229, 1231, 1237, 1249, 1259, 1277, 1279,
    1283, 1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321, 1327, 1361, 1367,
    1373, 1381, 1399, 1409, 1423, 1427, 1429, 1433, 1439, 1447, 1451, 1453,
    1459, 1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511, 1523, 1531, 1543,
    1549, 1553, 1559, 1567, 1571, 1579, 1583, 1597, 1601, 1607, 1609, 1613,
    1619, 1621, 1627, 1637, 1657, 1663, 1667, 1669, 1693, 1697, 1699, 1709,
    1721, 1723, 1733, 1741, 1747, 1753, 1759, 1777, 1783, 1787, 1789, 1801,
    1811, 1823, 1831, 1847, 1861, 1867, 1871, 1873, 1877, 1879, 1889, 1901,
    1907, 1913, 1931, 1933, 1949, 1951, 1973, 1979, 1987, 1993, 1997, 1999,
    2003, 2011, 2017, 2027, 2029, 2039, 2053, 2063, 2069, 2081, 2083, 2087,
    2089, 2099, 2111, 2113, 2129, 2131, 2137, 2141, 2143, 2153, 2161, 2179,
    2203, 2207, 2213, 2221, 2237, 2239, 2243, 2251, 2267, 2269, 2273, 2281,
    2287, 2293, 2297, 2309, 2311, 2333, 2339, 2341, 2347, 2351, 2357, 2371,
    2377, 2381, 2383, 2389, 2393, 2399, 2411, 2417, 2423, 2437, 2441, 2447,
    2459, 2467, 2473, 2477, 2503, 2521, 2531, 2539, 2543, 2549, 2551, 2557,
    2579, 2591, 2593, 2609, 2617, 2621, 2633, 2647, 2657, 2659, 2663, 2671,
    2677, 2683, 2687, 2689, 2693, 2699, 2707, 2711, 2713, 2719, 2729, 2731,
    2741, 2749, 2753, 2767, 2777, 2789, 2791, 2797, 2801, 2803, 2819, 2833,
    2837, 2843, 2851, 2857, 2861, 2879, 2887, 2897, 2903, 2909, 2917, 2927,
    2939, 2953, 2957, 2963, 2969, 2971, 2999, 3001, 3011, 3019, 3023, 3037,
    3041, 3049, 3061, 3067, 3079, 3083, 3089, 3109, 3119, 3121, 3137, 3163,
    3167, 3169, 3181, 3187, 3191, 3203, 3209, 3217, 3221, 3229, 3251, 3253,
    3257, 3259, 3271, 3299, 3301, 3307, 3313, 3319, 3323, 3329, 3331, 3343,
    3347, 3359, 3361, 3371, 3373, 3389, 3391, 3407, 3413, 3433, 3449, 3457,
    3461, 3463, 3467, 3469, 3491, 3499, 3511, 3517, 3527, 3529, 3533, 3539,
    3541, 3547, 3557, 3559, 3571, 3581, 3583, 3593, 3607, 3613, 3617, 3623,
    3631, 3637, 3643, 3659, 3671, 3673, 3677, 3691, 3697, 3701, 3709, 3719,
    3727, 3733, 3739, 3761, 3767, 3769, 3779, 3793, 3797, 3803, 3821, 3823,
    3833, 3847, 3851, 3853, 3863, 3877, 3881, 3889, 3907, 3911, 3917, 3919,
    3923, 3929, 3931, 3943, 3947, 3967, 3989, 4001, 4003, 4007, 4013, 4019,
    4021, 4027, 4049, 4051, 4057, 4073, 4079, 4091, 4093, 4099, 4111, 4127,
    4129, 4133, 4139, 4153, 4157, 4159, 4177, 4201, 4211, 4217, 4219, 4229,
    4231, 4241, 4243, 4253, 4259, 4261, 4271, 4273, 4283, 4289, 4297, 4327,
    4337, 4339, 4349, 4357, 4363, 4373, 4391, 4397, 4409, 4421, 4423, 4441,
    4447, 4451, 4457, 4463, 4481, 4483, 4493, 4507, 4513, 4517, 4519, 4523,
    4547, 4549, 4561, 4567, 4583, 4591, 4597, 4603, 4621, 4637, 4639, 4643,
    4649, 4651, 4657, 4663, 4673, 4679, 4691, 4703, 4721, 4723, 4729, 4733,
    4751, 4759, 4783, 4787, 4789, 4793, 4799, 4801, 4813, 4817, 4831, 4861,
    4871, 4877, 4889, 4903, 4909, 4919, 4931, 4933, 4937, 4943, 4951, 4957,
    4967, 4969, 4973, 4987, 4993, 4999, 5003, 5009, 5011, 5021, 5023, 5039,
    5051, 5059, 5077, 5081, 5087, 5099, 5101, 5107, 5113, 5119, 5147, 5153,
    5167, 5171, 5179, 5189, 5197, 5209, 5227, 5231, 5233, 5237, 5261, 5273,
    5279, 5281, 5297, 5303, 5309, 5323, 5333, 5347, 5351, 5381, 5387, 5393,
    5399, 5407, 5413, 5417, 5419, 5431, 5437, 5441, 5443, 5449, 5471, 5477,
    5479, 5483, 5501, 5503, 5507, 5519, 5521, 5527, 5531, 5557, 5563, 5569,
    5573, 5581, 5591, 5623, 5639, 5641, 5647, 5651, 5653, 5657, 5659, 5669,
    5683, 5689, 5693, 5701, 5711, 5717, 5737, 5741, 5743, 5749, 5779, 5783,
    5791, 5801, 5807, 5813, 5821, 5827, 5839, 5843, 5849, 5851, 5857, 5861,
    5867, 5869, 5879, 5881, 5897, 5903, 5923, 5927, 5939, 5953, 5981, 5987,
    6007, 6011, 6029, 6037, 6043, 6047, 6053, 6067, 6073, 6079, 6089, 6091,
    6101, 6113, 6121, 6131, 6133, 6143, 6151, 6163, 6173, 6197, 6199, 6203,
    6211, 6217, 6221, 6229, 6247, 6257, 6263, 6269, 6271, 6277, 6287, 6299,
    6301, 6311, 6317, 6323, 6329, 6337, 6343, 6353, 6359, 6361, 6367, 6373,
    6379, 6389, 6397, 6421, 6427, 6449, 6451, 6469, 6473, 6481, 6491, 6521,
    6529, 6547, 6551, 6553, 6563, 6569, 6571, 6577, 6581, 6599, 6607, 6619,
    6637, 6653, 6659, 6661, 6673, 6679, 6689, 6691, 6701, 6703, 6709, 6719,
    6733, 6737, 6761, 6763, 6779, 6781, 6791, 6793, 6803, 6823, 6827, 6829,
    6833, 6841, 6857, 6863, 6869, 6871, 6883, 6899, 6907, 6911, 6917, 6947,
    6949, 6959, 6961, 6967, 6971, 6977, 6983, 6991, 6997, 7001, 7013, 7019,
    7027, 7039, 7043, 7057, 7069, 7079, 7103, 7109, 7121, 7127, 7129, 7151,
    7159, 7177, 7187, 7193, 7207, 7211, 7213, 7219, 7229, 7237, 7243, 7247,
    7253, 7283, 7297, 7307, 7309, 7321, 7331, 7333, 7349, 7351, 7369, 7393,
    7411, 7417, 7433, 7451, 7457, 7459, 7477, 7481, 7487, 7489, 7499, 7507,
    7517, 7523, 7529, 7537, 7541, 7547, 7549, 7559, 7561, 7573, 7577, 7583,
    7589, 7591, 7603, 7607, 7621, 7639, 7643, 7649, 7669, 7673, 7681, 7687,
    7691, 7699, 7703, 7717, 7723, 7727, 7741, 7753, 7757, 7759, 7789, 7793,
    7817, 7823, 7829, 7841, 7853, 7867, 7873, 7877, 7879, 7883, 7901, 7907,
    7919, -1
  };
  int n;
  int nprimes = sizeof primes / sizeof(int);

  /* Find the first prime greater than val */
  primes[nprimes - 1] = val + 1;
  n = 0;
  while (primes[n] < val)
    n += 1;

  n += 1;
  if (n == nprimes)
    /* Semi-gracefully deal with numbers larger than the table has.
       Should never happen, though. */
    return val + 1;
  else
    return primes[n];
#endif
}

/** Initialize a hashtable.
 * \param htab pointer to hash table to initialize.
 * \param size size of hashtable.
 * \param data_size size of an individual datum to store in the table.
 */
void
hash_init(HASHTAB *htab, int size, void (*free_data) (void *))
{
  size = next_prime_after(size);
  htab->last_index = -1;
  htab->free_data = free_data;
  htab->hashsize = size;
  htab->hashfunc_offset = 0;
  htab->buckets = mush_calloc(size, sizeof(struct hash_bucket), "hash.buckets");
}

/** Return a hashtable entry given a key.
 * \param htab pointer to hash table to search.
 * \param key key to look up in the table.
 * \return pointer to hash table entry for given key.
 */
HASHENT *
hash_find(HASHTAB *htab, const char *key)
{
  int len, n;

  if (!htab->entries)
    return NULL;

  len = strlen(key);

  for (n = 0; n < NHASH_TRIES; n++) {
    hash_func hash;
    int hval, hashindex = (n + htab->hashfunc_offset) % NHASH_MOD;
    hash = hash_functions[hashindex];
    hval = hash(key, len) % htab->hashsize;

    if (htab->buckets[hval].key && strcmp(htab->buckets[hval].key, key) == 0)
      return htab->buckets + hval;
  }

  return NULL;
}

enum { BUMP_LIMIT = 10 };

/** Do cuckoo hash cycling */
static bool
hash_insert(HASHTAB *htab, const char *key, void *data)
{
  int loop = 0, n;
  struct hash_bucket bump;

  bump.key = key;
  bump.data = data;

  while (loop < BUMP_LIMIT) {
    int hval, keylen;
    struct hash_bucket temp;

    keylen = strlen(bump.key);

    /* See if bump has any empty choices and use it */
    for (n = 0; n < NHASH_TRIES; n++) {
      hash_func hash;
      int hashindex = (n + htab->hashfunc_offset) % NHASH_MOD;

      hash = hash_functions[hashindex];
      hval = hash(bump.key, keylen) % htab->hashsize;
      if (htab->buckets[hval].key == NULL) {
        htab->buckets[hval] = bump;
        return true;
      }
    }

    /* None. Use a random func and bump the existing element */
    n = htab->hashfunc_offset + get_random32(0, NHASH_TRIES - 1);
    n %= NHASH_MOD;
    hval = (hash_functions[n]) (bump.key, keylen) % htab->hashsize;
    temp = htab->buckets[hval];
    htab->buckets[hval] = bump;
    bump = temp;
    loop += 1;
  }

  /* At this point, we've bumped BUMP_LIMIT times. Probably in a
     loop. Find the first empty bucket, add the last bumped to, and
     return failure. */
  for (n = 0; n < htab->hashsize; n++)
    if (htab->buckets[n].key == NULL) {
      htab->buckets[n] = bump;
      return false;
    }

  /* Never reached. */
  return false;
}


static int resize_calls = 0, first_offset = -1;

/** Resize a hash table.
 * \param htab pointer to hashtable.
 * \param primesize new size.
 * \param hashindex Index of first hash function to use
 */
static bool
real_hash_resize(HASHTAB *htab, int newsize, int hashfunc_offset)
{
  HASHENT *oldarr;
  int oldsize, oldoffset, i;

  /* Massive overkill here */
  if (resize_calls > 150) {
    fputs("Ooops. Too many attempts to resize a hash table.\n", stderr);
    return false;
  }

  /* If all possible hash function combos have been exhausted,
     grow the array */
  if (hashfunc_offset == first_offset) {
    int newersize = next_prime_after(floor(newsize * 1.15));
    first_offset = -1;
    return real_hash_resize(htab, newersize, hashfunc_offset);
  }

  resize_calls += 1;

  /* Save the old data we need */
  oldsize = htab->hashsize;
  oldoffset = htab->hashfunc_offset;
  oldarr = htab->buckets;

  htab->buckets =
    mush_calloc(newsize, sizeof(struct hash_bucket), "hash.buckets");
  htab->hashsize = newsize;
  htab->hashfunc_offset = hashfunc_offset;
  for (i = 0; i < oldsize; i++) {

    if (oldarr[i].key) {

      if (!hash_insert(htab, oldarr[i].key, oldarr[i].data)) {
        /* Couldn't fit an element in. Try with different hash functions. */
        mush_free(htab->buckets, "hash.buckets");
        htab->buckets = oldarr;
        htab->hashsize = oldsize;
        htab->hashfunc_offset = oldoffset;
        if (first_offset == -1)
          first_offset = hashfunc_offset;
        return
          real_hash_resize(htab, newsize, (hashfunc_offset + 1) % NHASH_MOD);
      }
    }
  }

  mush_free(oldarr, "hash.buckets");
  return true;
}

/** Resize a hash table.
 * \param htab pointer to hashtable.
 * \param size new size.
 */
bool
hash_resize(HASHTAB *htab, int size)
{
  if (htab) {
    htab->last_index = -1;
    first_offset = -1;
    resize_calls = 0;
    return real_hash_resize(htab, next_prime_after(size),
                            htab->hashfunc_offset);
  } else
    return false;
}

/** Add an entry to a hash table.
 * \param htab pointer to hash table.
 * \param key key string to store data under.
 * \param hashdata void pointer to data to be stored.
 * \param extra_size unused.
 * \retval false failure.
 * \retval true success.
 */
bool
hash_add(HASHTAB *htab, const char *key, void *hashdata)
{
  const char *keycopy;

  if (hash_find(htab, key) != NULL)
    return false;

  htab->entries += 1;

  keycopy = mush_strdup(key, "hash.key");

  if (!hash_insert(htab, keycopy, hashdata)) {
    first_offset = -1;
    resize_calls = 0;
    if (!real_hash_resize(htab, htab->hashsize,
                          (htab->hashfunc_offset + 1) % NHASH_MOD)) {
      htab->entries -= 1;
      return false;
    }
  }
  return true;
}

/** Delete an entry in a hash table.
 * \param htab pointer to hash table.
 * \param entry pointer to hash entry to delete (and free).
 */
void
hash_delete(HASHTAB *htab, HASHENT *entry)
{
  if (!entry)
    return;

  if (htab->free_data)
    htab->free_data(entry->data);
  mush_free((void *) entry->key, "hash.key");
  memset(entry, 0, sizeof *entry);
  htab->entries -= 1;
}

/** Flush a hash table, freeing all entries.
 * \param htab pointer to a hash table.
 * \param size size of hash table.
 */
void
hash_flush(HASHTAB *htab, int size)
{
  int i;
  struct hash_bucket *resized;

  if (htab->entries) {
    for (i = 0; i < htab->hashsize; i++) {
      if (htab->buckets[i].key) {
        mush_free((void *) htab->buckets[i].key, "hash.key");
        if (htab->free_data)
          htab->free_data(htab->buckets[i].data);
      }
    }
  }
  htab->entries = 0;
  size = next_prime_after(size);
  resized = mush_realloc(htab->buckets, size, "hash.buckets");
  if (resized) {
    htab->buckets = resized;
    htab->hashsize = size;
  }
  memset(htab->buckets, 0, sizeof(struct hash_bucket) * htab->hashsize);
}

/** Return the first entry of a hash table.
 * This function is used with hash_nextentry() to iterate through a 
 * hash table.
 * \param htab pointer to hash table.
 * \return first hash table entry.
 */
void *
hash_firstentry(HASHTAB *htab)
{
  int n;

  for (n = 0; n < htab->hashsize; n++)
    if (htab->buckets[n].key) {
      htab->last_index = n;
      return htab->buckets[n].data;
    }
  return NULL;
}

/** Return the first key of a hash table.
 * This function is used with hash_nextentry_key() to iterate through a 
 * hash table.
 * \param htab pointer to hash table.
 * \return first hash table key.
 */
const char *
hash_firstentry_key(HASHTAB *htab)
{
  int n;

  for (n = 0; n < htab->hashsize; n++)
    if (htab->buckets[n].key) {
      htab->last_index = n;
      return htab->buckets[n].key;
    }
  return NULL;
}

/** Return the next entry of a hash table.
 * This function is used with hash_firstentry() to iterate through a 
 * hash table. hash_firstentry() must be called before calling
 * this function.
 * \param htab pointer to hash table.
 * \return next hash table entry.
 */
void *
hash_nextentry(HASHTAB *htab)
{
  int n = htab->last_index + 1;
  while (n < htab->hashsize) {
    if (htab->buckets[n].key) {
      htab->last_index = n;
      return htab->buckets[n].data;
    }
    n += 1;
  }
  return NULL;
}

/** Return the next key of a hash table.
 * This function is used with hash_firstentry{,_key}() to iterate through a 
 * hash table. hash_firstentry{,_key}() must be called before calling
 * this function.
 * \param htab pointer to hash table.
 * \return next hash table key.
 */
const char *
hash_nextentry_key(HASHTAB *htab)
{
  int n = htab->last_index + 1;
  while (n < htab->hashsize) {
    if (htab->buckets[n].key) {
      htab->last_index = n;
      return htab->buckets[n].key;
    }
    n += 1;
  }
  return NULL;
}

/** Display a header for a stats listing.
 * \param player player to notify with header.
 */
void
hash_stats_header(dbref player)
{
  notify_format(player,
                "Table       Buckets Entries 1Lookup 2Lookup 3Lookup ~Memory");
}

/** Display stats on a hashtable.
 * \param player player to notify with stats.
 * \param htab pointer to the hash table.
 * \param hname name of the hash table.
 */
void
hash_stats(dbref player, HASHTAB *htab, const char *hname)
{
  int n;
  size_t bytes;
  unsigned int compares[3] = { 0, 0, 0 };

  if (!htab || !hname)
    return;

  bytes = sizeof *htab;
  bytes += sizeof(struct hash_bucket) * htab->hashsize;

  for (n = 0; n < htab->hashsize; n++)
    if (htab->buckets[n].key) {
      int i;
      int len = strlen(htab->buckets[n].key);
      bytes += len + 1;
      for (i = 0; i < 3; i++) {
        hash_func hash =
          hash_functions[(i + htab->hashfunc_offset) % NHASH_MOD];
        if ((hash(htab->buckets[n].key, len) % htab->hashsize) == (uint32_t) n) {
          compares[i] += 1;
          break;
        }
      }
    }

  notify_format(player,
                "%-11s %7d %7d %7u %7u %7u %7u",
                hname, htab->hashsize, htab->entries, compares[0], compares[1],
                compares[2], (unsigned int) bytes);
}
