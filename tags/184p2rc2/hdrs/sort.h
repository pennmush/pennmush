#ifndef __PENNSORT_H
#define __PENNSORT_H
#include "copyrite.h"

/* Sort and comparision functions */

#define MAX_SORTSIZE (BUFFER_LEN / 2)  /**< Maximum number of elements to sort */

typedef char *SortType;

SortType autodetect_list(char **ptrs, int nptrs);
SortType autodetect_2lists(char *ptrs[], int nptrs, char *ptrs2[], int nptrs2);
SortType get_list_type(char *args[], int nargs, int type_pos, char *ptrs[],
                       int nptrs);
SortType get_list_type_noauto(char *args[], int nargs, int type_pos);
int gencomp(dbref player, char *a, char *b, SortType sort_type);
void do_gensort(dbref player, char *keys[], char *strs[], int n,
                SortType sort_type);
/** Type definition for a qsort comparison function */
typedef int (*comp_func) (const void *, const void *);
void sane_qsort(void **array, int left, int right, comp_func compare);


/* Comparison functions for qsort() and other routines.  */
int int_comp(const void *s1, const void *s2);
int nval_comp(const void *s1, const void *s2);
int uint_comp(const void *s1, const void *s2);
int str_comp(const void *s1, const void *s2);
int stri_comp(const void *s1, const void *s2);
int dbref_comp(const void *s1, const void *s2);
int attr_comp(const void *s1, const void *s2);
int u_comp(const void *s1, const void *s2);     /* For sortby() */

int compare_attr_names(const char *attr1, const char *attr2);

#endif                          /* __PENNSORT_H */
