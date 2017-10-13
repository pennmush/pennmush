/**
 * \file htab.h
 *
 * \brief Structures and declarations needed for table hashing
 */

#ifndef __HTAB_H
#define __HTAB_H

typedef struct hashtable HASHTAB;

struct hash_bucket;

/** A hash table.
 */
struct hashtable {
  int hashsize;                /**< Size of buckets array */
  int entries;                 /**< Number of entries stored */
  int hashseed_offset;         /**< Which hash seed to use */
  struct hash_bucket *buckets; /**< Buckets */
  int last_index;              /**< State for hashfirst & hashnext. */
  void (*free_data)(void *);   /**< Function to call on data when deleting
                                  a entry. May be NULL if unused. */
};

/** Used to return information from hash_stats(). */
struct hashstats {
  int entries;       /* Number of entries in the hash table. This value is
                        independently calculated when hash_stats() walks the
                        table. */
  int lookups[3];    /* Lookup distance. */
  double key_length; /* Average length of the keys. */
  int bytes;         /* Estimate of bytes used. Overhead from the allocator is
                        not included. */
};

/* TODO: Get rid of these macros. The arguments are in random-ish order, and
 * it's confusing to have hashfind and hash_find return two different things. */
#define hashinit(tab, size) hash_init((tab), (size), NULL)
#define hashfind(key, tab) hash_value(tab, key)
#define hashadd(key, data, tab) hash_add(tab, key, data)
#define hashdelete(key, tab) hash_delete(tab, key)
#define hashflush(tab, size) hash_flush(tab, size)
#define hashfree(tab) hash_flush(tab, 0)
void hash_init(HASHTAB *htab, int size, void (*)(void *));
struct hash_bucket *hash_find(const HASHTAB *htab, const char *key);
void *hash_value(const HASHTAB *htab, const char *key);
bool hash_add(HASHTAB *htab, const char *key, void *hashdata);
void hash_delete(HASHTAB *htab, const char *key);
void hash_flush(HASHTAB *htab, int size);
void *hash_firstentry(HASHTAB *htab);
void *hash_nextentry(HASHTAB *htab);
const char *hash_firstentry_key(HASHTAB *htab);
const char *hash_nextentry_key(HASHTAB *htab);
void hash_stats(const HASHTAB *htab, struct hashstats *stats);
unsigned int next_prime_after(unsigned int);

#endif /* __HTAB_H_ */
