/** The memory checker. It implements a rudimentary ref-counting scheme to try
 *  to make it easier to find memory leaks. */

#ifndef __MEMCHECK_H
#define __MEMCHECK_H

#include "mushtype.h"

void add_check(const char *ref);
#define ADD_CHECK(x) add_check(x)
#define DEL_CHECK(x) del_check(x, __FILE__, __LINE__)
void del_check(const char *ref, const char *filename, int line);
void




list_mem_check(void (*callback) (void *data, const char *const name, int count),
               void *data);
void log_mem_check(void);

#endif                          // __MEMCHECK_H
