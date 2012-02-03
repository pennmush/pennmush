/**
 * \file bufferq.c
 *
 * \brief Code for managing queues of buffers, a handy data structure.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif

#include "conf.h"
#include "externs.h"
#include "flags.h"
#include "dbdefs.h"
#include "bufferq.h"
#include "mymalloc.h"
#include "log.h"
#include "confmagic.h"

#define BUFFERQLINEOVERHEAD     (2*sizeof(int)+sizeof(time_t)+sizeof(dbref))

static void shift_bufferq(BUFFERQ *bq, int space_needed);

/** Add data to a buffer queue.
 * \param bq pointer to buffer queue.
 * \param type caller-specific integer
 * \param player caller-specific dbref
 * \param msg caller-specific string to add.
 */
void
add_to_bufferq(BUFFERQ *bq, int type, dbref player, const char *msg)
{
  int len = strlen(msg);
  int room = len + 1 + BUFFERQLINEOVERHEAD;
  if (!bq)
    return;
  if (room > bq->buffer_size)
    return;
  if ((bq->buffer_end > bq->buffer) &&
      ((bq->buffer_size - (bq->buffer_end - bq->buffer)) < room))
    shift_bufferq(bq, room);
  memcpy(bq->buffer_end, &len, sizeof(len));
  bq->buffer_end += sizeof(len);
  memcpy(bq->buffer_end, &player, sizeof(player));
  bq->buffer_end += sizeof(player);
  memcpy(bq->buffer_end, &type, sizeof(type));
  bq->buffer_end += sizeof(type);
  memcpy(bq->buffer_end, &mudtime, sizeof(time_t));
  bq->buffer_end += sizeof(time_t);
  memcpy(bq->buffer_end, msg, len + 1);
  bq->buffer_end += len + 1;
  strcpy(bq->last_string, msg);
  bq->last_type = type;
  bq->num_buffered++;
}


static void
shift_bufferq(BUFFERQ *bq, int space_needed)
{
  char *p = bq->buffer;
  int size, jump;
  int skipped = 0;

  while ((space_needed > 0) && (p < bq->buffer_end)) {
    /* First 4 bytes is the size of the first string, not including \0 */
    memcpy(&size, p, sizeof(size));
    /* Jump to the start of the next string */
    jump = size + BUFFERQLINEOVERHEAD + 1;
    p += jump;
    space_needed -= jump;
    skipped++;
  }

  if ((p != bq->buffer_end) && (space_needed > 0)) {
    /* Not good. We couldn't get the space we needed even after we
     * emptied the buffer. This should never happen, but if it does,
     * we'll just log a fault and do nothing.
     */
    do_rawlog(LT_ERR, "Unable to get enough buffer queue space");
    return;
  }

  /* Shift everything here and after up to the front
   * At this point, p may be pointing at the very end of the buffer,
   * in which case, we just move it to the front with no shifting.
   */
  if (p < bq->buffer_end)
    memmove(bq->buffer, p, bq->buffer_end - p);
  bq->buffer_end -= (p - bq->buffer);
  bq->num_buffered -= skipped;
}

/** Allocate memory for a buffer queue to hold a given number of lines.
 * \param lines lines to allocate for buffer queue.
 * \retval address of allocated buffer queue.
 */
BUFFERQ *
allocate_bufferq(int lines)
{
  BUFFERQ *bq;
  int bytes = lines * (BUFFER_LEN + BUFFERQLINEOVERHEAD);
  bq = mush_malloc(sizeof(BUFFERQ), "bufferq");
  bq->buffer = mush_malloc(bytes, "bufferq.buffer");
  *bq->buffer = '\0';
  bq->buffer_end = bq->buffer;
  bq->num_buffered = 0;
  bq->buffer_size = bytes;
  strcpy(bq->last_string, "");
  bq->last_type = 0;
  return bq;
}

/** Free memory of a buffer queue.
 * \param bq pointer to buffer queue.
 */
void
free_bufferq(BUFFERQ *bq)
{
  if (!bq)
    return;
  if (bq->buffer)
    mush_free(bq->buffer, "bufferq.buffer");
  mush_free(bq, "bufferq");
}

/** Reallocate a buffer queue (to change its size)
 * \param bq pointer to buffer queue.
 * \param lines new number of lines to store in buffer queue.
 * \retval address of reallocated buffer queue.
 */
BUFFERQ *
reallocate_bufferq(BUFFERQ *bq, int lines)
{
  char *newbuff;
  ptrdiff_t bufflen;
  int bytes = lines * (BUFFER_LEN + 2 * BUFFERQLINEOVERHEAD);
  /* If we were accidentally called without a buffer, deal */
  if (!bq) {
    return allocate_bufferq(lines);
  }
  /* Are we not changing size? */
  if (bq->buffer_size == bytes)
    return bq;
  if (bq->buffer_size > bytes) {
    /* Shrinking the buffer */
    if ((bq->buffer_end - bq->buffer) >= bytes)
      shift_bufferq(bq, bq->buffer_end - bq->buffer - bytes);
  }
  bufflen = bq->buffer_end - bq->buffer;
  newbuff = realloc(bq->buffer, bytes);
  if (newbuff) {
    bq->buffer = newbuff;
    bq->buffer_end = bq->buffer + bufflen;
    bq->buffer_size = bytes;
  }
  return bq;
}



/** Iterate through messages in a bufferq.
 * This function returns the next message in a bufferq, given
 * a pointer to the start of entry (which is modified).
 * It returns NULL when there are no more messages to get.
 * Call this in a loop to get all messages; do not intersperse
 * with calls to insert messages!
 * \param bq pointer to buffer queue structure.
 * \param p address of pointer to track start of next entry. If that's
 *   the address of a null pointer, reset to beginning.
 * \param player address of pointer to return player data in
 * \param type address of pointer to return type value in.
 * \param timestamp address of pointer to return timestamp in.
 * \return next message text or NULL if no more.
 */
char *
iter_bufferq(BUFFERQ *bq, char **p, dbref *player, int *type, time_t *timestamp)
{
  static char tbuf1[BUFFER_LEN];
  int size;

  if (!p || !bq || !bq->buffer || (bq->buffer == bq->buffer_end))
    return NULL;

  if (*p == bq->buffer_end)
    return NULL;

  if (!*p)
    *p = bq->buffer;            /* Reset to beginning */

  memcpy(&size, *p, sizeof(size));
  *p += sizeof(size);
  memcpy(player, *p, sizeof(dbref));
  *p += sizeof(dbref);
  memcpy(type, *p, sizeof(int));
  *p += sizeof(int);
  memcpy(timestamp, *p, sizeof(time_t));
  *p += sizeof(time_t);
  memcpy(tbuf1, *p, size + 1);
  *p += size + 1;
  return tbuf1;
}

/** Size of bufferq buffer in blocks.
 * \param bq pointer to buffer queue.
 * \return size of buffer queue in 8k blocks
 */
int
bufferq_blocks(BUFFERQ *bq)
{
  if (bq && bq->buffer)
    return bq->buffer_size / (BUFFER_LEN + BUFFERQLINEOVERHEAD);
  else
    return 0;
}

/** Number of lines stored in queue.
 * \param bq pointer to buffer queue.
 * \return line count
 */
int
bufferq_lines(BUFFERQ *bq)
{
  int lines = 0;
  char *p = NULL;
  dbref player;
  int type;
  time_t t;


  if (isempty_bufferq(bq))
    return 0;

  while (iter_bufferq(bq, &p, &player, &type, &t))
    lines++;

  return lines;
}

/** Is a buffer queue empty?
 * \param bq pointer to buffer queue.
 * \retval 1 the buffer queue is empty (has no messages).
 * \retval 0 the buffer queue is not empty (has messages).
 */
bool
isempty_bufferq(BUFFERQ *bq)
{
  if (!bq || !bq->buffer)
    return 1;
  if (bq->buffer == bq->buffer_end)
    return 1;
  return 0;
}
