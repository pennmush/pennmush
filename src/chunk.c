/**
 * \file chunk.c
 *
 * Used to have a fancy paging system. Now just uses malloc to manage memory.
 *
 * Data format is a 16 bit length field , followed by 16 bits reserved
 * for future use (And to ensure 4-byte alignment), followed by the
 * data. Derefs are not used.
 */

#include "copyrite.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "chunk.h"
#include "mymalloc.h"

#ifdef WIN32
#pragma warning(disable : 4761) /* disable warning re conversion */
#endif

/** Allocate a chunk of storage.
 * \param data the data to be stored.
 * \param len the length of the data to be stored.
 * \param derefs the deref count to set on the chunk.
 * \return the chunk reference for retrieving (or deleting) the data.
 */
chunk_reference_t
chunk_create(char const *data, uint16_t len,
             uint8_t derefs __attribute__((__unused__)))
{
  uint8_t *chunk;

  chunk = mush_malloc(len + 4, "chunk");

  memset(chunk, 0, 4);
  memcpy(chunk, &len, 2);
  memcpy(chunk + 4, data, len);

  return (uintptr_t) chunk;
}

/** Deallocate a chunk of storage.
 * \param reference the reference to the chunk to be freed.
 */
void
chunk_delete(chunk_reference_t reference)
{
  if (reference) {
    mush_free((void *) reference, "chunk");
  }
}

/** Fetch a chunk of data.
 * If the chunk is too large to fit in the supplied buffer, then
 * the buffer will be left untouched.  The length of the data is
 * returned regardless; this can be used to resize the buffer
 * (or just as information for further processing of the data).
 * \param reference the reference to the chunk to be fetched.
 * \param buffer the buffer to put the data into.
 * \param buffer_len the length of the buffer.
 * \return the length of the data.
 */
uint16_t
chunk_fetch(chunk_reference_t reference, char *buffer, uint16_t buffer_len)
{
  uint16_t len;

  if (!reference) {
    return 0;
  }

  memcpy(&len, (void *) reference, 2);

  if (buffer_len >= len) {
    memcpy(buffer, ((uint8_t *) reference) + 4, len);
  }

  return len;
}

/** Get the length of a chunk.
 * This is equivalent to calling chunk_fetch(reference, NULL, 0).
 * It can be used to glean the proper size for a buffer to actually
 * retrieve the data, if you're being stingy.
 * \param reference the reference to the chunk to be queried.
 * \return the length of the data.
 */
uint16_t
chunk_len(chunk_reference_t reference)
{
  uint16_t len;
  if (!reference) {
    return 0;
  }
  memcpy(&len, (void *) reference, 2);
  return len;
}

/** Get the deref count of a chunk.
 * This can be used to preserve the deref count across database saves
 * or similar save and restore operations.
 * \param reference the reference to the chunk to be queried.
 * \return the deref count for data.
 */
uint8_t
chunk_derefs(chunk_reference_t reference __attribute__((__unused__)))
{
  return 0;
}
