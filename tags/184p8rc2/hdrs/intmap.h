/**
 * \file intmap.h
 *
 * \brief Interface for maps with unsigned 32-bit integers as keys.
 */

#ifndef INTMAP_H
#define INTMAP_H

struct intmap;
typedef struct intmap intmap;

typedef uint32_t im_key; /**< Integer map keys are 32-bit unsigned integers */
/* typedef uint64_t im_key; */

intmap *im_new(void);
void im_destroy(intmap *);

int64_t im_count(intmap *);

bool im_insert(intmap *, im_key, void *);
void *im_find(intmap *, im_key);
bool im_exists(intmap *, im_key);
bool im_delete(intmap *, im_key);
void im_dump_graph(intmap *, const char *);
void im_stats_header(dbref);
void im_stats(dbref, intmap *, const char *);
#endif                          /* INTMAP_H */
