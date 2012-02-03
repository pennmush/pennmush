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
#include <openssl/bn.h>
#include "conf.h"
#include "externs.h"
#include "log.h"
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
unsigned int
next_prime_after(unsigned int val)
{
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
}

/** Initialize a hashtable.
 * \param htab pointer to hash table to initialize.
 * \param size size of hashtable.
 */
void
hash_init(HASHTAB *htab, int size, void (*free_data) (void *))
{
  size = next_prime_after(size);
  htab->last_index = -1;
  htab->free_data = free_data;
  htab->hashsize = size;
  htab->hashfunc_offset = 0;
  htab->entries = 0;
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

    if (htab->buckets[hval].key && len == htab->buckets[hval].keylen &&
        memcmp(htab->buckets[hval].key, key, len) == 0)
      return htab->buckets + hval;
  }

  return NULL;
}

enum { BUMP_LIMIT = 10 };

/** Do cuckoo hash cycling */
static bool
hash_insert(HASHTAB *htab, const char *key, int keylen, void *data)
{
  int loop = 0, n;
  struct hash_bucket bump;

  bump.key = key;
  bump.keylen = keylen;
  bump.data = data;

  while (loop < BUMP_LIMIT) {
    int hval;
    struct hash_bucket temp;

    /* See if bump has any empty choices and use it */
    for (n = 0; n < NHASH_TRIES; n++) {
      hash_func hash;
      int hashindex = (n + htab->hashfunc_offset) % NHASH_MOD;

      hash = hash_functions[hashindex];
      hval = hash(bump.key, bump.keylen) % htab->hashsize;
      if (htab->buckets[hval].key == NULL) {
        htab->buckets[hval] = bump;
        return true;
      }
    }

    /* None. Use a random func and bump the existing element */
    n = htab->hashfunc_offset + get_random32(0, NHASH_TRIES - 1);
    n %= NHASH_MOD;
    hval = (hash_functions[n]) (bump.key, bump.keylen) % htab->hashsize;
    temp = htab->buckets[hval];
    htab->buckets[hval] = bump;
    bump = temp;
    loop += 1;
  }

  /* At this point, we've bumped BUMP_LIMIT times. Probably in a
     loop. Find the first empty bucket, add the last bumped to, and
     return failure. The table will have to be resized now to restore
     the hash. */
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
      if (!hash_insert(htab, oldarr[i].key, oldarr[i].keylen, oldarr[i].data)) {
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
 * \retval false failure.
 * \retval true success.
 */
bool
hash_add(HASHTAB *htab, const char *key, void *hashdata)
{
  char *keycopy;
  int keylen;

  if (hash_find(htab, key) != NULL)
    return false;

  keycopy = mush_strdup(key, "hash.key");
  keylen = strlen(keycopy);

  if (htab->entries == htab->hashsize)
    real_hash_resize(htab, next_prime_after(floor(htab->hashsize * 1.15)),
                     htab->hashfunc_offset);

  htab->entries += 1;
  if (!hash_insert(htab, keycopy, keylen, hashdata)) {
    first_offset = -1;
    resize_calls = 0;
    real_hash_resize(htab, htab->hashsize,
                     (htab->hashfunc_offset + 1) % NHASH_MOD);
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
  resized =
    mush_realloc(htab->buckets, sizeof(struct hash_bucket) * size,
                 "hash.buckets");
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
  int n, entries = 0;
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
      entries += 1;
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
  if (entries != htab->entries)
    notify_format(player, "Mismatch in size: %d expected, %d found!",
                  htab->entries, entries);
}
