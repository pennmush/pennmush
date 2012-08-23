/**
 * \file sort.c
 * \brief Sorting and comparision functions
 */
#include "copyrite.h"

#include "config.h"
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "conf.h"
#include "externs.h"
#include "parse.h"
#include "ansi.h"
#include "command.h"
#include "sort.h"
#include "confmagic.h"


#define EPSILON 0.000000001  /**< limit of precision for float equality */

/** If sort_order is positive, sort forward. If negative, it sorts backward. */
#define ASCENDING 1
#define DESCENDING -1
int sort_order = ASCENDING;

/** qsort() comparision routine for int */
int
int_comp(const void *s1, const void *s2)
{
  int a, b;

  a = *(const int *) s1;
  b = *(const int *) s2;

  if (a == b)
    return 0;
  else if (a < b)
    return -1 * sort_order;
  else
    return 1 * sort_order;
}


/** qsort() comparision routine for unsigned int */
int
uint_comp(const void *s1, const void *s2)
{
  unsigned int a, b;

  a = *(const unsigned int *) s1;
  b = *(const unsigned int *) s2;

  if (a == b)
    return 0;
  else if (a < b)
    return -1 * sort_order;
  else
    return 1 * sort_order;
}


/** qsort() comparision routine for NVAL/double */
int
nval_comp(const void *a, const void *b)
{
  const NVAL *x = a, *y = b;
  const double epsilon = pow(10.0, -FLOAT_PRECISION);
  int eq = (fabs(*x - *y) <= (epsilon * fabs(*x)));
  return eq ? 0 : ((*x > *y ? 1 : -1) * sort_order);
}


/** qsort() comparision routine for strings.
 * Uses strcmp()
 */
int
str_comp(const void *s1, const void *s2)
{
  const char *a, *b;

  a = *(const char **) s1;
  b = *(const char **) s2;

  return strcmp(a, b) * sort_order;
}

/** qsort() comparision routine for strings.
 * Uses strcasecmp().
 * Case-insensitive.
 */
int
stri_comp(const void *s1, const void *s2)
{
  const char *a, *b;

  a = *(const char **) s1;
  b = *(const char **) s2;

  return strcasecmp(a, b) * sort_order;
}

/** qsort() comparision routine for dbrefs. */
int
dbref_comp(const void *s1, const void *s2)
{
  dbref a, b;

  a = *(const dbref *) s1;
  b = *(const dbref *) s2;

  if (a == b)
    return 0;
  else if (a < b)
    return -1 * sort_order;
  else
    return 1 * sort_order;
}

/** qsort() comparision routine used by sortby() */
int
u_comp(const void *s1, const void *s2, dbref executor, dbref enactor,
       ufun_attrib * ufun, NEW_PE_INFO *pe_info)
{
  char result[BUFFER_LEN];
  int n;
  PE_REGS *pe_regs;
  int ret;

  /* Our two arguments are passed as %0 and %1 to the sortby u-function. */

  /* Note that this function is for use in conjunction with our own
   * sane_qsort routine, NOT with the standard library qsort!
   */
  pe_regs = pe_regs_create(PE_REGS_ARG, "u_comp");
  pe_regs_setenv_nocopy(pe_regs, 0, (char *) s1);
  pe_regs_setenv_nocopy(pe_regs, 1, (char *) s2);

  /* Run the u-function, which should return a number. */
  ret = call_ufun(ufun, result, executor, enactor, pe_info, pe_regs);
  pe_regs_free(pe_regs);

  if (ret) {
    return 0;
  }

  n = parse_integer(result);

  return n;
}



/** Used with fun_sortby()
 *
 * Based on Andrew Molitor's qsort, which doesn't require transitivity
 * between comparisons (essential for preventing crashes due to
 * boneheads who write comparison functions where a > b doesn't mean b
 * < a).
 *
 * Actually, this sort doesn't require commutivity.
 * Sorting doesn't make sense without transitivity...
 */

void
sane_qsort(void *array[], int left, int right, comp_func compare,
           dbref executor, dbref enactor, ufun_attrib * ufun,
           NEW_PE_INFO *pe_info)
{

  int i, last;
  void *tmp;

loop:
  if (left >= right)
    return;

  /* Pick something at random at swap it into the leftmost slot   */
  /* This is the pivot, we'll put it back in the right spot later */

  i = get_random32(left, right);
  tmp = array[i];
  array[i] = array[left];
  array[left] = tmp;

  last = left;
  for (i = left + 1; i <= right; i++) {

    /* Walk the array, looking for stuff that's less than our */
    /* pivot. If it is, swap it with the next thing along     */

    if (compare(array[i], array[left], executor, enactor, ufun, pe_info) < 0) {
      last++;
      if (last == i)
        continue;

      tmp = array[last];
      array[last] = array[i];
      array[i] = tmp;
    }
  }

  /* Now we put the pivot back, it's now in the right spot, we never */
  /* need to look at it again, trust me.                             */

  tmp = array[last];
  array[last] = array[left];
  array[left] = tmp;

  /* At this point everything underneath the 'last' index is < the */
  /* entry at 'last' and everything above it is not < it.          */

  if ((last - left) < (right - last)) {
    sane_qsort(array, left, last - 1, compare, executor, enactor, ufun,
               pe_info);
    left = last + 1;
    goto loop;
  } else {
    sane_qsort(array, last + 1, right, compare, executor, enactor, ufun,
               pe_info);
    right = last - 1;
    goto loop;
  }
}




/****************************** gensort ************/

#define GENRECORD(x) void x(s_rec *rec,dbref player,char *sortflags); \
  void x(s_rec *rec, \
    dbref player __attribute__ ((__unused__)), \
    char *sortflags __attribute__ ((__unused__)))

GENRECORD(gen_alphanum)
{
  size_t len;
  if (strchr(rec->val, ESC_CHAR) || strchr(rec->val, TAG_START)) {
    rec->memo.str.s = mush_strdup(remove_markup(rec->val, &len), "genrecord");
    rec->memo.str.freestr = 1;
  } else {
    rec->memo.str.s = rec->val;
    rec->memo.str.freestr = 0;
  }
}

GENRECORD(gen_magic)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  char *s = rec->val;
  int intval;
  int numdigits;
  bool usedup = false;
  dbref victim;

  while (s && *s) {
    switch (*s) {
    case ESC_CHAR:
      usedup = true;
      while (*s && *s != 'm')
        s++;
      s++;                      /* Advance past m */
      break;
    case TAG_START:
      usedup = true;
      while (*s && *s != TAG_END)
        s++;
      s++;                      /* Advance past tag_end */
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      usedup = true;
      intval = 0;
      while (*s && isdigit((int) *s)) {
        intval *= 10;
        intval += *s - '0';
        s++;
      }
      safe_format(buff, &bp, "%.20d", intval);
      if (*s == '.') {
        s++;
        if (isdigit((int) *s)) {
          intval = 0;
          numdigits = 0;
          while (*s && isdigit((int) *s)) {
            intval *= 10;
            intval += *s - '0';
            numdigits++;
            s++;
          }
          safe_format(buff, &bp, "%d", intval);
          numdigits = 20 - numdigits;
          while (numdigits > 0) {
            safe_chr('0', buff, &bp);
            numdigits--;
          }
        }
      }
      break;
    case NUMBER_TOKEN:
      if (isdigit((int) *(s + 1))) {
        usedup = true;
        s++;
        victim = 0;
        while (*s && isdigit((int) *s)) {
          victim *= 10;
          victim += *s - '0';
          s++;
        }
        if (GoodObject(victim)) {
          safe_str(Name(victim), buff, &bp);
        } else {
          safe_str(T(e_notvis), buff, &bp);
        }
        break;
      }
    default:
      safe_chr(*s, buff, &bp);
      if (*s)
        s++;
    }
  }
  *bp = '\0';

  /* Don't strdup if we haven't done anything useful. */
  if (usedup) {
    rec->memo.str.s = mush_strdup(buff, "genrecord");
    rec->memo.str.freestr = 1;
  } else {
    rec->memo.str.s = rec->val;
  }
}

GENRECORD(gen_dbref)
{
  char *val = remove_markup(rec->val, NULL);
  rec->memo.num =
    (globals.database_loaded ? parse_objid(val) : qparse_dbref(val));
}

GENRECORD(gen_num)
{
  rec->memo.num = parse_integer(remove_markup(rec->val, NULL));
}

GENRECORD(gen_float)
{
  rec->memo.numval = parse_number(remove_markup(rec->val, NULL));
}

GENRECORD(gen_db_name)
{
  rec->memo.str.s = (char *) "";
  if (RealGoodObject(rec->db))
    rec->memo.str.s = (char *) Name(rec->db);
  rec->memo.str.freestr = 0;
}

GENRECORD(gen_db_idle)
{
  rec->memo.num = -1;
  if (RealGoodObject(rec->db)) {
    if (Priv_Who(player))
      rec->memo.num = least_idle_time_priv(rec->db);
    else
      rec->memo.num = least_idle_time(rec->db);
  }
}

GENRECORD(gen_db_conn)
{
  rec->memo.num = -1;
  if (RealGoodObject(rec->db)) {
    if (Priv_Who(player))
      rec->memo.num = most_conn_time_priv(rec->db);
    else
      rec->memo.num = most_conn_time(rec->db);
  }
}

GENRECORD(gen_db_ctime)
{
  if (RealGoodObject(rec->db))
    rec->memo.tm = CreTime(rec->db);
}

GENRECORD(gen_db_mtime)
{
  if (RealGoodObject(rec->db))
    rec->memo.tm = ModTime(rec->db);
}

GENRECORD(gen_db_owner)
{
  if (RealGoodObject(rec->db))
    rec->memo.num = Owner(rec->db);
}

GENRECORD(gen_db_loc)
{
  rec->memo.num = -1;
  if (RealGoodObject(rec->db) && Can_Locate(player, rec->db)) {
    rec->memo.num = Location(rec->db);
  }
}

GENRECORD(gen_db_attr)
{
  /* Eek, I hate dealing with memory issues. */

  static char *defstr = (char *) "";
  const char *ptr;

  rec->memo.str.s = defstr;
  rec->memo.str.freestr = 0;
  if (RealGoodObject(rec->db) && sortflags && *sortflags &&
      (ptr = do_get_attrib(player, rec->db, sortflags)) != NULL) {
    rec->memo.str.s = mush_strdup(ptr, "genrecord");
    rec->memo.str.freestr = 1;
  }
}

/* Compare(r,x,y) {
 *   if (x->db < 0 && y->db < 0)
 *     return 0;  // Garbage is identical.
 *   if (x->db < 0)
 *     return 2;  // Garbage goes last.
 *   if (y->db < 0)
 *     return -2; // Garbage goes last.
 *   if (r < 0)
 *     return -2; // different
 *   if (r > 0)
 *     return 2;  // different
 *   if (x->db < y->db)
 *     return -1; // similar
 *   if (y->db < x-db)
 *     return 1;  // similar
 *   return 0;    // identical
 * }
 */

/* If I could, I'd let sort() toss out non-existant dbrefs
 * Instead, sort stuffs them into a jumble at the end. */

/* Compare a single int, > == or < 0 */
#define Compare(i,x,y) \
  ((x->db < 0 || y->db < 0) ?                                    \
   ((x->db < 0 && y->db < 0) ? 0 : (x->db < 0 ? 2 : -2))         \
   : ((i != 0) ? (i < 0 ? -2 : 2)                                \
      : (x->db == y->db ? 0 : (x->db < y->db ? -1 : 1))          \
      )                                                          \
   )

/* Compare a float, > == or < 0.0 */
#define CompareF(f,x,y) \
  ((x->db < 0 || y->db < 0) ?                                    \
   ((x->db < 0 && y->db < 0) ? 0 : (x->db < 0 ? 2 : -2))         \
   : ((fabs(f) > EPSILON) ? (f < 0.0 ? -2 : 2)                          \
      : (x->db == y->db ? 0 : (x->db < y->db ? -1 : 1))          \
      )                                                          \
   )

/* Compare two ints */
#define Compare2(i1,i2,x,y)         \
  ((x->db < 0 || y->db < 0) ?                                       \
   ((x->db < 0 && y->db < 0) ? 0 : (x->db < 0 ? 2 : -2))            \
   : ((i1 != i2) ? (i1 < i2 ? -2 : 2)                               \
      : (x->db == y->db ? 0 : (x->db < y->db ? -1 : 1))             \
      )                                                             \
   )

/* Compare two doubles */
#define Compare2F(f1,f2,x,y)         \
  ((x->db < 0 || y->db < 0) ?                                       \
   ((x->db < 0 && y->db < 0) ? 0 : (x->db < 0 ? 2 : -2))            \
   : ((fabs(f1 - f2) > EPSILON) ? (f1 < f2 ? -2 : 2)                         \
      : (x->db == y->db ? 0 : (x->db < y->db ? -1 : 1))             \
      )                                                             \
   )

static int
s_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  int res = 0;
  res = strcoll(sr1->memo.str.s, sr2->memo.str.s);
  return Compare(res, sr1, sr2) * sort_order;
}

static int
m_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  int res = 0, ret;
  res = strcoll(sr1->memo.str.s, sr2->memo.str.s);
  ret = Compare(res, sr1, sr2);
  if (ret == 0) {
    char v1[BUFFER_LEN];
    char v2[BUFFER_LEN];
    if (has_markup(sr1->val))
      strcpy(v1, remove_markup(sr1->val, NULL));
    else
      strcpy(v1, sr1->val);
    if (has_markup(sr2->val))
      strcpy(v2, remove_markup(sr2->val, NULL));
    else
      strcpy(v2, sr2->val);
    upcasestr(v1);
    upcasestr(v2);
    res = strcoll(v1, v2);
    ret = Compare(res, sr1, sr2);
  }
  return ret * sort_order;
}

int
attr_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;

  return compare_attr_names(sr1->memo.str.s, sr2->memo.str.s) * sort_order;

}

int
compare_attr_names(const char *attr1, const char *attr2)
{
  char word1[BUFFER_LEN + 1], word2[BUFFER_LEN + 1];
  char *a1, *a2, *next1, *next2;
  int branches1 = 1, branches2 = 1;
  int cmp;

  strcpy(word1, attr1);
  strcpy(word2, attr2);

  a1 = word1;
  a2 = word2;

  while (a1 && a2) {
    next1 = strchr(a1, '`');
    if (next1) {
      branches1++;
      *(next1)++ = '\0';
    }
    next2 = strchr(a2, '`');
    if (next2) {
      branches2++;
      *(next2)++ = '\0';
    }
    cmp = strcoll(a1, a2);
    if (cmp != 0) {
      /* Current branch differs */
      return (cmp < 0 ? -1 : 1);
    }
    if (branches1 != branches2) {
      /* Current branch is the same, but one attr has more branches */
      return (branches1 < branches2 ? -1 : 1);
    }
    a1 = next1;
    a2 = next2;
  }
  /* All branches were the same */
  return 0;

}

static int
i_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  return Compare2(sr1->memo.num, sr2->memo.num, sr1, sr2) * sort_order;
}

static int
tm_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  double r = difftime(sr1->memo.tm, sr2->memo.tm);
  return CompareF(r, sr1, sr2) * sort_order;
}

static int
f_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  return Compare2F(sr1->memo.numval, sr2->memo.numval, sr1, sr2) * sort_order;
}

#define IS_DB 0x1U
#define IS_STRING 0x2U
#define IS_CASE_INSENS 0x4U

char ALPHANUM_LIST[] = "A";
char INSENS_ALPHANUM_LIST[] = "I";
char DBREF_LIST[] = "D";
char NUMERIC_LIST[] = "N";
char FLOAT_LIST[] = "F";
char MAGIC_LIST[] = "M";
char DBREF_NAME_LIST[] = "NAME";
char DBREF_NAMEI_LIST[] = "NAMEI";
char DBREF_IDLE_LIST[] = "IDLE";
char DBREF_CONN_LIST[] = "CONN";
char DBREF_CTIME_LIST[] = "CTIME";
char DBREF_MTIME_LIST[] = "MTIME";
char DBREF_OWNER_LIST[] = "OWNER";
char DBREF_LOCATION_LIST[] = "LOC";
char DBREF_ATTR_LIST[] = "ATTR";
char DBREF_ATTRI_LIST[] = "ATTRI";
char ATTRNAME_LIST[] = "LATTR";
char *UNKNOWN_LIST = NULL;

ListTypeInfo ltypelist[] = {
  /* List type name,            recordmaker,    comparer, dbrefs? */
  {ALPHANUM_LIST, NULL, 0, gen_alphanum, s_comp, IS_STRING},
  {INSENS_ALPHANUM_LIST, NULL, 0, gen_alphanum, s_comp,
   IS_STRING | IS_CASE_INSENS},
  {DBREF_LIST, NULL, 0, gen_dbref, i_comp, 0},
  {NUMERIC_LIST, NULL, 0, gen_num, i_comp, 0},
  {FLOAT_LIST, NULL, 0, gen_float, f_comp, 0},
  {MAGIC_LIST, NULL, 0, gen_magic, m_comp, IS_STRING | IS_CASE_INSENS},
  {DBREF_NAME_LIST, NULL, 0, gen_db_name, s_comp, IS_DB | IS_STRING},
  {DBREF_NAMEI_LIST, NULL, 0, gen_db_name, s_comp,
   IS_DB | IS_STRING | IS_CASE_INSENS},
  {DBREF_IDLE_LIST, NULL, 0, gen_db_idle, i_comp, IS_DB},
  {DBREF_CONN_LIST, NULL, 0, gen_db_conn, i_comp, IS_DB},
  {DBREF_CTIME_LIST, NULL, 0, gen_db_ctime, tm_comp, IS_DB},
  {DBREF_MTIME_LIST, NULL, 0, gen_db_ctime, tm_comp, IS_DB},
  {DBREF_OWNER_LIST, NULL, 0, gen_db_owner, i_comp, IS_DB},
  {DBREF_LOCATION_LIST, NULL, 0, gen_db_loc, i_comp, IS_DB},
  {DBREF_ATTR_LIST, NULL, 0, gen_db_attr, s_comp, IS_DB | IS_STRING},
  {DBREF_ATTRI_LIST, NULL, 0, gen_db_attr, s_comp,
   IS_DB | IS_STRING | IS_CASE_INSENS},
  {ATTRNAME_LIST, NULL, 0, gen_alphanum, attr_comp, IS_STRING},
  /* This stops the loop, so is default */
  {NULL, NULL, 0, gen_alphanum, s_comp, IS_STRING}
};

/**
 * Given a string description of a sort type, generate and return a
 * ListTypeInfo that can be passed to slist_* functions.
 * \param sort_type A string describing a sort type.
 * \retval ListTypeInfo containing all generating and comparison information
 *         needed.
 */
ListTypeInfo *
get_list_type_info(SortType sort_type)
{
  int i, len;
  char *ptr = NULL;
  ListTypeInfo *lti;

  /* i is either the right one or the default, so we return it anyway. */
  lti = mush_calloc(1, sizeof(ListTypeInfo), "list_type_info");

  lti->sort_order = ASCENDING;

  if (*sort_type == '-') {
    lti->sort_order = DESCENDING;
    sort_type++;
  }
  if (!sort_type) {
    /* Advance i to the default */
    for (i = 0; ltypelist[i].name; i++) ;
  } else if ((ptr = strchr(sort_type, ':'))) {
    len = ptr - sort_type;
    ptr += 1;
    if (!*ptr)
      ptr = NULL;
    for (i = 0;
         ltypelist[i].name && strncasecmp(ltypelist[i].name, sort_type, len);
         i++) ;
  } else {
    for (i = 0; ltypelist[i].name && strcasecmp(ltypelist[i].name, sort_type);
         i++) ;
  }

  lti->name = ltypelist[i].name;
  if (ptr) {
    lti->attrname = mush_strdup(ptr, "list_type_info_attrname");
  } else {
    lti->attrname = NULL;
  }
  lti->make_record = ltypelist[i].make_record;
  lti->sorter = ltypelist[i].sorter;
  lti->flags = ltypelist[i].flags;
  return lti;
}

/**
 * Free a ListTypeInfo struct.
 * \param lti The ListTypeInfo to free. Must be created by get_list_type_info.
 */
void
free_list_type_info(ListTypeInfo *lti)
{
  if (lti->attrname) {
    mush_free(lti->attrname, "list_type_info_attrname");
  }
  mush_free(lti, "list_type_info");
}

/**
 * Get the type of a list, as provided by a user. If it is not specified,
 * try and guess the list type.
 * \param args The arguments to a function.
 * \param nargs number of items in args
 * \param type_pos where to look in args for the sort_type definition.
 * \param ptrs The list to autodetect on
 * \param nptrs number of items in ptrs
 * \retval A string that describes the comparison information.
 */
SortType
get_list_type(char *args[], int nargs, int type_pos, char *ptrs[], int nptrs)
{
  int i, len;
  char *str, *ptr;
  sort_order = ASCENDING;
  if (nargs >= type_pos) {
    str = args[type_pos - 1];
    if (*str) {
      if (*str == '-') {
        str++;
        sort_order = DESCENDING;
      }
      ptr = strchr(str, ':');
      if (ptr) {
        len = ptr - str;
      } else {
        len = strlen(str);
      }
      for (i = 0; ltypelist[i].name &&
           strncasecmp(ltypelist[i].name, str, len); i++) ;
      /* return ltypelist[i].name; */
      return args[type_pos - 1];
    }
  }
  return autodetect_list(ptrs, nptrs);
}

/**
 * Get the type of a list, but return UNKNOWN if it is not specified.
 * \param args The arguments to a function.
 * \param nargs number of items in args
 * \param type_pos where to look in args for the sort_type definition.
 * \retval A string that describes the comparison information.
 */
SortType
get_list_type_noauto(char *args[], int nargs, int type_pos)
{
  int i, len;
  char *str, *ptr;
  sort_order = ASCENDING;
  if (nargs >= type_pos) {
    str = args[type_pos - 1];
    if (*str) {
      if (*str == '-') {
        str++;
        sort_order = DESCENDING;
      }
      ptr = strchr(str, ':');
      if (ptr) {
        len = ptr - str;
      } else {
        len = strlen(str);
      }
      for (i = 0; ltypelist[i].name &&
           strncasecmp(ltypelist[i].name, str, len); i++) ;
      /* return ltypelist[i].name; */
      return args[type_pos - 1];
    }
  }
  return UNKNOWN_LIST;
}

static void
genrecord(s_rec *sp, dbref player, ListTypeInfo *lti)
{
  lti->make_record(sp, player, lti->attrname);
  if (lti->flags & IS_CASE_INSENS && sp->memo.str.s) {
    if (sp->memo.str.freestr == 0) {
      sp->memo.str.s = mush_strdup(sp->memo.str.s, "genrecord");
      sp->memo.str.freestr = 1;
    }
    upcasestr(sp->memo.str.s);
  }
}

/** A generic comparer routine to compare two values of any sort type.
 * \param player Player doing the comparison
 * \param a Key 1 to compare
 * \param b Key 2 to compare
 * \param sort_type SortType describing what kind of comparison to make.
 */
int
gencomp(dbref player, char *a, char *b, SortType sort_type)
{
  char *ptr;
  int i, len;
  int result;
  s_rec s1, s2;
  ListTypeInfo *lti;
  ptr = NULL;
  if (!sort_type) {
    /* Advance i to the default */
    for (i = 0; ltypelist[i].name; i++) ;
  } else if ((ptr = strchr(sort_type, ':'))) {
    len = ptr - sort_type;
    ptr += 1;
    if (!*ptr)
      ptr = NULL;
    for (i = 0;
         ltypelist[i].name && strncasecmp(ltypelist[i].name, sort_type, len);
         i++) ;
  } else {
    for (i = 0; ltypelist[i].name && strcasecmp(ltypelist[i].name, sort_type);
         i++) ;
  }
  lti = get_list_type_info(sort_type);

  if (ltypelist[i].flags & IS_DB) {
    s1.db = parse_objid(a);
    s2.db = parse_objid(b);
    if (!RealGoodObject(s1.db))
      s1.db = NOTHING;
    if (!RealGoodObject(s2.db))
      s2.db = NOTHING;
  } else {
    s1.db = s2.db = 0;
  }

  s1.val = a;
  s2.val = b;
  genrecord(&s1, player, lti);
  genrecord(&s2, player, lti);
  result = lti->sorter((const void *) &s1, (const void *) &s2);
  if (lti->flags & IS_STRING) {
    if (s1.memo.str.freestr)
      mush_free(s1.memo.str.s, "genrecord");
    if (s2.memo.str.freestr)
      mush_free(s2.memo.str.s, "genrecord");
  }
  free_list_type_info(lti);
  return result;
}

/**
 * Given a player dbref (For use with viewing permissions for attrs, etc),
 * list of keys, list of strings it maps to (sortkey()-style),
 * # of keys+strings and a list type info, build an array of
 * s_rec structures representing each item.
 * \param player the player executing the sort.
 * \param keys the array of items to sort.
 * \param strs If non-NULL, these are what to sort keys using.
 * \param n Number of items in keys and strs
 * \param lti List Type Info describing how it's sorted and built.
 * \retval A pointer to the first s_rec of an n s_rec array.
 */
s_rec *
slist_build(dbref player, char *keys[], char *strs[], int n, ListTypeInfo *lti)
{
  int i;
  s_rec *sp;

  sort_order = lti->sort_order;

  sp = mush_calloc(n, sizeof(s_rec), "do_gensort");

  for (i = 0; i < n; i++) {
    /* Elements are 0 by default thanks to calloc. Only need to touch
       those that need other values. */
    sp[i].val = keys[i];
    if (strs)
      sp[i].ptr = strs[i];
    if (lti->flags & IS_DB) {
      sp[i].db = parse_objid(keys[i]);
      if (!RealGoodObject(sp[i].db))
        sp[i].db = NOTHING;
    }
    genrecord(&sp[i], player, lti);
  }
  return sp;
}


/**
 * Given an array of s_rec items, sort them in-place using a specified
 * ListTypeInformation.
 * \param sp the array of sort_records, returned by slist_build
 * \param n Number of items in sp
 * \param lti List Type Info describing how it's sorted and built.
 */
void
slist_qsort(s_rec *sp, int n, ListTypeInfo *lti)
{
  qsort((void *) sp, n, sizeof(s_rec), lti->sorter);
}

/**
 * Given an array of _sorted_ s_rec items, unique them in place by
 * freeing them and marking the final elements' freestr = 0.
 * \param sp the array of sort_records, returned by slist_build
 * \param n Number of items in sp
 * \param lti List Type Info describing how it's sorted and built.
 * \retval The count of unique items.
 */
int
slist_uniq(s_rec *sp, int n, ListTypeInfo *lti)
{
  int count, i;

  /* Quick sanity check. */
  if (n < 2)
    return n;

  /* First item's always 'unique' :D. */
  count = 1;

  for (i = 1; i < n; i++) {
    /* If sp[i] is a duplicate of sp[count - 1], free it. If it's not,
     * move it to sp[count] and increment count. */
    if (lti->sorter((const void *) &sp[count - 1], (const void *) &sp[i])) {
      /* Not a duplicate. */
      sp[count++] = sp[i];
    } else {
      /* Free it if needed. */
      if ((lti->flags & IS_STRING) && sp[i].memo.str.freestr) {
        mush_free(sp[i].memo.str.s, "genrecord");
      }
    }
  }
  for (i = count; i < n; i++) {
    if ((lti->flags & IS_STRING) && sp[i].memo.str.freestr) {
      sp[i].memo.str.freestr = 0;
      sp[i].memo.str.s = NULL;
    }
  }
  return count;
}

/**
 * Given an array of s_rec items, free them if they are not NULL.
 * \param sp the array of sort_records, returned by slist_build
 * \param n Number of items in <keys> and <strs>
 * \param lti List Type Info describing how it's sorted and built.
 */
void
slist_free(s_rec *sp, int n, ListTypeInfo *lti)
{
  int i;
  for (i = 0; i < n; i++) {
    if ((lti->flags & IS_STRING) && sp[i].memo.str.freestr)
      mush_free(sp[i].memo.str.s, "genrecord");
  }
  mush_free(sp, "do_gensort");
}

int
slist_comp(s_rec *s1, s_rec *s2, ListTypeInfo *lti)
{
  return lti->sorter((const void *) s1, (const void *) s2);
}

/** A generic sort routine to sort several different
 * types of arrays, in place.
 * \param player the player executing the sort.
 * \param keys the array of items to sort.
 * \param strs If non-NULL, these are what to sort keys using.
 * \param n number of items in keys and strs
 * \param sort_type the string that describes the sort type.
 */
void
do_gensort(dbref player, char *keys[], char *strs[], int n, SortType sort_type)
{
  s_rec *sp;
  ListTypeInfo *lti;
  int i;

  lti = get_list_type_info(sort_type);
  sp = slist_build(player, keys, strs, n, lti);
  slist_qsort(sp, n, lti);

  /* Change keys and strs around. */
  for (i = 0; i < n; i++) {
    keys[i] = sp[i].val;
    if (strs) {
      strs[i] = sp[i].ptr;
    }
  }

  /* Free the s_rec list */
  slist_free(sp, n, lti);

  free_list_type_info(lti);
}

SortType
autodetect_2lists(char *ptrs[], int nptrs, char *ptrs2[], int nptrs2)
{
  SortType a = autodetect_list(ptrs, nptrs);
  SortType b = autodetect_list(ptrs2, nptrs2);

  /* If they're equal, no problem. */
  if (a == b) {
    return a;
  }

  /* Float and numeric means float. */
  if ((a == NUMERIC_LIST || a == FLOAT_LIST) &&
      (b == NUMERIC_LIST || b == FLOAT_LIST)) {
    return FLOAT_LIST;
  }

  /* Magic list by default. */
  return MAGIC_LIST;
}

typedef enum {
  L_NUMERIC,
  L_FLOAT,
  L_ALPHANUM,
  L_DBREF
} ltype;

SortType
autodetect_list(char *ptrs[], int nptrs)
{
  int i;
  ltype lt;
  SortType sort_type = NUMERIC_LIST;

  lt = L_NUMERIC;
  for (i = 0; i < nptrs; i++) {
    /* Just one big chain of fall-throughs =). */
    switch (lt) {
    case L_NUMERIC:
      if (is_strict_integer(ptrs[i])) {
        break;
      }
    case L_FLOAT:
      if (is_strict_number(ptrs[i])) {
        lt = L_FLOAT;
        sort_type = FLOAT_LIST;
        break;
      }
    case L_DBREF:
      if (is_objid(ptrs[i]) && (i == 0 || lt == L_DBREF)) {
        lt = L_DBREF;
        sort_type = DBREF_LIST;
        break;
      }
    case L_ALPHANUM:
      return MAGIC_LIST;
    }
  }
  return sort_type;
}
