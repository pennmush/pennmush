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

#include "copyrite.h"
#include "htab.h"

#include <string.h>
#include <math.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <openssl/bn.h>

#include "hash_function.h"
#include "mymalloc.h"

/* Temporary prototypes to make the compiler happy. */
char *
mush_strdup(const char *s, const char *check)
  __attribute_malloc__;
    uint32_t get_random32(uint32_t low, uint32_t high);

    struct hash_bucket {
      const char *key;
      void *data;
      int keylen;
    };

    static const uint64_t hash_seeds[] = {
      0x28187BCC53900639ULL,
      0x37FF1FD24D473811ULL,
      0xDFBE49319032F8A0ULL,
      0x511425D4D0A6E518ULL,
      0xAC30C8C94941DE18ULL,
      0xC61F7F133E0DCF02ULL,
      0xA32C48FC8A34D36AULL,
      0x6561992839F450CBULL,
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
    if (BN_is_prime_ex(p, BN_prime_checks, ctx, NULL) > 0)
      break;
    val += 2;
  }

  return val;
}

/** Initialize a hashtable.
 * \param htab pointer to hash table to initialize.
 * \param size size of hashtable.
 * \param free_data void pointer to a function to call whenever a hash entry is deleted, or NULL
 */
void
hash_init(HASHTAB *htab, int size, void (*free_data) (void *))
{
  size = next_prime_after(size);
  htab->last_index = -1;
  htab->free_data = free_data;
  htab->hashsize = size;
  htab->hashseed_offset = 0;
  htab->entries = 0;
  htab->buckets = mush_calloc(size, sizeof(struct hash_bucket), "hash.buckets");
}

/** Return a hashtable entry given a key.
 * \param htab pointer to hash table to search.
 * \param key key to look up in the table.
 * \return pointer to hash table entry for given key.
 */
struct hash_bucket *
hash_find(const HASHTAB *htab, const char *key)
{
  int len, n;

  if (!htab->entries)
    return NULL;

  len = strlen(key);

  for (n = 0; n < NHASH_TRIES; n++) {
    int hval, seedindex = (n + htab->hashseed_offset) % NHASH_MOD;
    hval = city_hash(key, len, hash_seeds[seedindex]) % htab->hashsize;

    if (htab->buckets[hval].key && len == htab->buckets[hval].keylen &&
        memcmp(htab->buckets[hval].key, key, len) == 0)
      return htab->buckets + hval;
  }

  return NULL;
}

void *
hash_value(const HASHTAB *htab, const char *key)
{
  struct hash_bucket *entry = hash_find(htab, key);
  return entry ? entry->data : NULL;
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
    int hval, seed_index;
    struct hash_bucket temp;

    /* See if bump has any empty choices and use it */
    for (n = 0; n < NHASH_TRIES; n++) {
      seed_index = (n + htab->hashseed_offset) % NHASH_MOD;

      hval = city_hash(bump.key, bump.keylen, hash_seeds[seed_index]) %
        htab->hashsize;
      if (htab->buckets[hval].key == NULL) {
        htab->buckets[hval] = bump;
        return true;
      }
    }

    /* None. Use a random seed and bump the existing element */
    seed_index = htab->hashseed_offset + get_random32(0, NHASH_TRIES - 1);
    seed_index %= NHASH_MOD;
    hval = city_hash(bump.key, bump.keylen, hash_seeds[seed_index]) %
      htab->hashsize;
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
 * \param seed_index Index of first hash seed to use
 */
static bool
hash_resize(HASHTAB *htab, int newsize, int hashseed_offset)
{
  struct hash_bucket *oldarr;
  int oldsize, oldoffset, i;

  /* Massive overkill here */
  if (resize_calls > 150) {
    fputs("Ooops. Too many attempts to resize a hash table.\n", stderr);
    return false;
  }

  /* If all possible hash function combos have been exhausted,
     grow the array */
  if (hashseed_offset == first_offset) {
    int newersize = next_prime_after(floor(newsize * 1.15));
    first_offset = -1;
    return hash_resize(htab, newersize, hashseed_offset);
  }

  resize_calls += 1;

  /* Save the old data we need */
  oldsize = htab->hashsize;
  oldoffset = htab->hashseed_offset;
  oldarr = htab->buckets;

  htab->buckets =
    mush_calloc(newsize, sizeof(struct hash_bucket), "hash.buckets");
  htab->hashsize = newsize;
  htab->hashseed_offset = hashseed_offset;
  for (i = 0; i < oldsize; i++) {
    if (oldarr[i].key) {
      if (!hash_insert(htab, oldarr[i].key, oldarr[i].keylen, oldarr[i].data)) {
        /* Couldn't fit an element in. Try with different hash functions. */
        mush_free(htab->buckets, "hash.buckets");
        htab->buckets = oldarr;
        htab->hashsize = oldsize;
        htab->hashseed_offset = oldoffset;
        if (first_offset == -1)
          first_offset = hashseed_offset;
        return hash_resize(htab, newsize, (hashseed_offset + 1) % NHASH_MOD);
      }
    }
  }

  mush_free(oldarr, "hash.buckets");
  return true;
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
    hash_resize(htab, next_prime_after(floor(htab->hashsize * 1.15)),
                htab->hashseed_offset);

  htab->entries += 1;
  if (!hash_insert(htab, keycopy, keylen, hashdata)) {
    first_offset = -1;
    resize_calls = 0;
    hash_resize(htab, htab->hashsize, (htab->hashseed_offset + 1) % NHASH_MOD);
  }
  return true;
}

static void
hash_delete_bucket(HASHTAB *htab, struct hash_bucket *entry)
{
  if (htab->free_data)
    htab->free_data(entry->data);
  mush_free((void *) entry->key, "hash.key");
  memset(entry, 0, sizeof *entry);
  htab->entries -= 1;
}

/** Delete an entry in a hash table.
 * \param htab pointer to hash table.
 * \param key key to delete from the table.
 */
void
hash_delete(HASHTAB *htab, const char *key)
{
  struct hash_bucket *entry = hash_find(htab, key);
  if (!entry)
    return;

  hash_delete_bucket(htab, entry);
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
        hash_delete_bucket(htab, &htab->buckets[i]);
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

/** Display stats on a hashtable.
 * \param htab pointer to the hash table.
 * \param stats pointer to hashstats to output to.
 */
void
hash_stats(const HASHTAB *htab, struct hashstats *stats)
{
  int n;

  if (!htab || !stats)
    return;

  memset(stats, 0, sizeof *stats);
  stats->bytes = sizeof(*htab);
  stats->bytes += sizeof(struct hash_bucket) * htab->hashsize;

  for (n = 0; n < htab->hashsize; n++) {
    if (htab->buckets[n].key) {
      int i;
      stats->bytes += htab->buckets[n].keylen + 1;
      stats->key_length += htab->buckets[n].keylen;
      stats->entries += 1;
      for (i = 0; i < 3; i++) {
        int seed_index = (i + htab->hashseed_offset) % NHASH_MOD;
        if ((city_hash(htab->buckets[n].key,
                       htab->buckets[n].keylen,
                       hash_seeds[seed_index]) % htab->hashsize) ==
            (uint32_t) n) {
          stats->lookups[i] += 1;
          break;
        }
      }
    }
  }

  if (stats->entries > 0) {
    stats->key_length /= stats->entries;
  }
}
