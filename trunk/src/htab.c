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
#include "hash_function.h"
#include "log.h"
#include "htab.h"
#include "mymalloc.h"
#include "confmagic.h"

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
 * \param free_data void pointer to a function to call whenever a hash entry is deleted, or NULL
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
