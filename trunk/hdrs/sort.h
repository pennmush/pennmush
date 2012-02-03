/**
 * \file sort.h
 *
 * \brief Routines for sorting lists in Penn
 */

#ifndef __PENNSORT_H
#define __PENNSORT_H
#include "copyrite.h"

/* Sort and comparision functions */
typedef char *SortType;
typedef struct sort_record s_rec;
typedef int (*qsort_func) (const void *, const void *);
typedef void (*makerecord) (s_rec *, dbref player, char *sortflags);
typedef struct _list_type_info_ ListTypeInfo;

/** Sorting strings by different values. We store both the string and
 * its 'key' to sort by. Sort of a hardcode munge.
 */
struct sort_record {
  char *ptr;     /**< NULL except for sortkey */
  char *val;     /**< The string this is */
  dbref db;      /**< dbref (default 0, bad is -1) */
  union {
    struct {
      char *s;     /**< string comparisons */
      bool freestr;   /**< free str on completion */
    } str;
    int num;       /**< integer comparisons */
    NVAL numval;   /**< float comparisons */
    time_t tm;     /**< time comparisions */
  } memo;
};

struct _list_type_info_ {
  SortType name;
  char *attrname;
  int sort_order;
  makerecord make_record;
  qsort_func sorter;
  uint32_t flags;
};

ListTypeInfo *get_list_type_info(SortType type);

#define MAX_SORTSIZE (BUFFER_LEN / 2)  /**< Maximum number of elements to sort */

SortType autodetect_list(char **ptrs, int nptrs);
SortType autodetect_2lists(char *ptrs[], int nptrs, char *ptrs2[], int nptrs2);
SortType get_list_type(char *args[], int nargs, int type_pos, char *ptrs[],
                       int nptrs);
SortType get_list_type_noauto(char *args[], int nargs, int type_pos);

/** Routines to actually deal with (sort) lists. */
ListTypeInfo *get_list_type_info(SortType sort_type);
void free_list_type_info(ListTypeInfo *lti);
s_rec *slist_build(dbref player, char *keys[], char *strs[], int n,
                   ListTypeInfo *lti);
void slist_qsort(s_rec *sp, int n, ListTypeInfo *lti);
int slist_uniq(s_rec *sp, int n, ListTypeInfo *lti);
void slist_free(s_rec *sp, int n, ListTypeInfo *lti);
int slist_comp(s_rec *s1, s_rec *s2, ListTypeInfo *lti);

/** General-use sorting routines, good for most purposes. */
int gencomp(dbref player, char *a, char *b, SortType sort_type);
void do_gensort(dbref player, char *keys[], char *strs[], int n,
                SortType sort_type);

/** Type definition for a qsort comparison function */
typedef int (*comp_func) (const void *, const void *, dbref, dbref,
                          ufun_attrib *, NEW_PE_INFO *);
void sane_qsort(void **array, int left, int right, comp_func compare,
                dbref executor, dbref enactor, ufun_attrib * ufun,
                NEW_PE_INFO *pe_info);


/* Comparison functions for qsort() and other routines.  */
int int_comp(const void *s1, const void *s2);
int nval_comp(const void *s1, const void *s2);
int uint_comp(const void *s1, const void *s2);
int str_comp(const void *s1, const void *s2);
int stri_comp(const void *s1, const void *s2);
int dbref_comp(const void *s1, const void *s2);
int attr_comp(const void *s1, const void *s2);
int u_comp(const void *s1, const void *s2, dbref executor, dbref enactor, ufun_attrib * ufun, NEW_PE_INFO *pe_info);    /* For sortby() */

int compare_attr_names(const char *attr1, const char *attr2);

#endif                          /* __PENNSORT_H */
