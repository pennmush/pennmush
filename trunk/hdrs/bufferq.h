/**
 * \file bufferq.h
 *
 * \brief Headers for managing queues of buffers, a handy data structure.
 *
 *
 */


#ifndef BUFFERQ_H
#define BUFFERQ_H

typedef struct bufferq BUFFERQ;

/** A bufferq. */
struct bufferq {
  char *buffer;         /**< Pointer to start of buffer */
  char *buffer_end;     /**< Pointer to insertion point in buffer */
  int buffer_size;      /**< Size allocated to buffer, in bytes */
  int num_buffered;     /**< Number of strings in the buffer */
  char last_string[BUFFER_LEN]; /**< Cache of last string inserted */
  char last_type;       /**< Cache of type of last string inserted */
};

#define BufferQSize(b) ((b)->buffer_size)    /**< Size of a bufferq, in bytes */
#define BufferQNum(b) ((b)->num_buffered)    /**< Number of (variable-length) strings buffered */
#define BufferQLast(b) ((b)->last_string)    /**< Last string inserted */
#define BufferQLastType(b) ((b)->last_type)  /**< Type of last string inserted */

BUFFERQ *allocate_bufferq(int lines);
BUFFERQ *reallocate_bufferq(BUFFERQ *bq, int lines);
void free_bufferq(BUFFERQ *bq);
void add_to_bufferq(BUFFERQ *bq, int type, dbref player, const char *msg);
char *iter_bufferq(BUFFERQ *bq, char **p, dbref *player, int *type,
                   time_t *timestamp);
int bufferq_lines(BUFFERQ *bq);
int bufferq_blocks(BUFFERQ *bq);
bool isempty_bufferq(BUFFERQ *bq);

#endif
