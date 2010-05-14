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
    return -1;
  else
    return 1;
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
    return -1;
  else
    return 1;
}


/** qsort() comparision routine for NVAL/double */
int
nval_comp(const void *a, const void *b)
{
  const NVAL *x = a, *y = b;
  const double epsilon = pow(10.0, -FLOAT_PRECISION);
  int eq = (fabs(*x - *y) <= (epsilon * fabs(*x)));
  return eq ? 0 : (*x > *y ? 1 : -1);
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

  return strcmp(a, b);
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

  return strcasecmp(a, b);
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
    return -1;
  else
    return 1;
}


dbref ucomp_executor, ucomp_caller, ucomp_enactor;
char ucomp_buff[BUFFER_LEN];
PE_Info *ucomp_pe_info;

/** qsort() comparision routine used by sortby() */
int
u_comp(const void *s1, const void *s2)
{
  char result[BUFFER_LEN], *rp;
  char const *tbuf;
  int n;

  /* Our two arguments are passed as %0 and %1 to the sortby u-function. */

  /* Note that this function is for use in conjunction with our own
   * sane_qsort routine, NOT with the standard library qsort!
   */
  global_eval_context.wenv[0] = (char *) s1;
  global_eval_context.wenv[1] = (char *) s2;

  /* Run the u-function, which should return a number. */

  tbuf = ucomp_buff;
  rp = result;
  if (process_expression(result, &rp, &tbuf,
                         ucomp_executor, ucomp_caller, ucomp_enactor,
                         PE_DEFAULT, PT_DEFAULT, ucomp_pe_info))
    return 0;
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
sane_qsort(void *array[], int left, int right, comp_func compare)
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

    if (compare(array[i], array[left]) < 0) {
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
    sane_qsort(array, left, last - 1, compare);
    left = last + 1;
    goto loop;
  } else {
    sane_qsort(array, last + 1, right, compare);
    right = last - 1;
    goto loop;
  }
}




/****************************** gensort ************/

typedef struct sort_record s_rec;

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

#define RealGoodObject(x) (GoodObject(x) && !IsGarbage(x))

GENRECORD(gen_magic)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  char *s = rec->val;
  int intval;
  int numdigits;
  dbref victim;

  while (s && *s) {
    switch (*s) {
    case ESC_CHAR:
      while (*s && *s != 'm')
        s++;
      break;
    case TAG_START:
      while (*s && *s != TAG_END)
        s++;
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
          safe_str(T("#-1 NO SUCH OBJECT VISIBLE"), buff, &bp);
        }
        break;
      }
    default:
      safe_chr(*s, buff, &bp);
    }
    if (*s)
      s++;
  }
  *bp = '\0';
  rec->memo.str.s = mush_strdup(buff, "genrecord");
  rec->memo.str.freestr = 1;
}

GENRECORD(gen_dbref)
{
  rec->memo.num = qparse_dbref(rec->val);
}

GENRECORD(gen_num)
{
  rec->memo.num = parse_integer(rec->val);
}

GENRECORD(gen_float)
{
  rec->memo.numval = parse_number(rec->val);
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


typedef int (*qsort_func) (const void *, const void *);
typedef void (*makerecord) (s_rec *, dbref player, char *sortflags);


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
  return Compare(res, sr1, sr2);
}

static int
si_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  int res = 0;
  res = strcasecoll(sr1->memo.str.s, sr2->memo.str.s);
  return Compare(res, sr1, sr2);
}

static int
i_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  return Compare2(sr1->memo.num, sr2->memo.num, sr1, sr2);
}

static int
tm_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  double r = difftime(sr1->memo.tm, sr2->memo.tm);
  return CompareF(r, sr1, sr2);
}

static int
f_comp(const void *s1, const void *s2)
{
  const s_rec *sr1 = (const s_rec *) s1;
  const s_rec *sr2 = (const s_rec *) s2;
  return Compare2F(sr1->memo.numval, sr2->memo.numval, sr1, sr2);
}

typedef struct _list_type_list_ {
  char *name;
  makerecord make_record;
  qsort_func sorter;
  uint32_t flags;
} list_type_list;

#define IS_DB 0x1U
#define IS_STRING 0x2U

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
char DBREF_OWNER_LIST[] = "OWNER";
char DBREF_LOCATION_LIST[] = "LOC";
char DBREF_ATTR_LIST[] = "ATTR";
char DBREF_ATTRI_LIST[] = "ATTRI";
char *UNKNOWN_LIST = NULL;

list_type_list ltypelist[] = {
  /* List type name,            recordmaker,    comparer, dbrefs? */
  {ALPHANUM_LIST, gen_alphanum, s_comp, IS_STRING},
  {INSENS_ALPHANUM_LIST, gen_alphanum, si_comp, IS_STRING},
  {DBREF_LIST, gen_dbref, i_comp, 0},
  {NUMERIC_LIST, gen_num, i_comp, 0},
  {FLOAT_LIST, gen_float, f_comp, 0},
  {MAGIC_LIST, gen_magic, si_comp, 0},
  {DBREF_NAME_LIST, gen_db_name, si_comp, IS_DB | IS_STRING},
  {DBREF_NAMEI_LIST, gen_db_name, si_comp, IS_DB | IS_STRING},
  {DBREF_IDLE_LIST, gen_db_idle, i_comp, IS_DB},
  {DBREF_CONN_LIST, gen_db_conn, i_comp, IS_DB},
  {DBREF_CTIME_LIST, gen_db_ctime, tm_comp, IS_DB},
  {DBREF_OWNER_LIST, gen_db_owner, i_comp, IS_DB},
  {DBREF_LOCATION_LIST, gen_db_loc, i_comp, IS_DB},
  {DBREF_ATTR_LIST, gen_db_attr, s_comp, IS_DB | IS_STRING},
  {DBREF_ATTRI_LIST, gen_db_attr, si_comp, IS_DB | IS_STRING},
  /* This stops the loop, so is default */
  {NULL, gen_alphanum, s_comp, IS_STRING}
};

char *
get_list_type(char *args[], int nargs, int type_pos, char *ptrs[], int nptrs)
{
  static char stype[BUFFER_LEN];
  int i;
  char *str;
  if (nargs >= type_pos) {
    str = args[type_pos - 1];
    if (*str) {
      strcpy(stype, str);
      str = strchr(stype, ':');
      if (str)
        *(str++) = '\0';
      for (i = 0; ltypelist[i].name && strcasecmp(ltypelist[i].name, stype);
           i++) ;
      /* return ltypelist[i].name; */
      if (ltypelist[i].name) {
        return args[type_pos - 1];
      }
    }
  }
  return autodetect_list(ptrs, nptrs);
}

char *
get_list_type_noauto(char *args[], int nargs, int type_pos)
{
  static char stype[BUFFER_LEN];
  int i;
  char *str;
  if (nargs >= type_pos) {
    str = args[type_pos - 1];
    if (*str) {
      strcpy(stype, str);
      str = strchr(stype, ':');
      if (str)
        *(str++) = '\0';
      for (i = 0; ltypelist[i].name && strcasecmp(ltypelist[i].name, stype);
           i++) ;
      /* return ltypelist[i].name; */
      return args[type_pos - 1];
    }
  }
  return UNKNOWN_LIST;
}


/** A generic comparer routine to compare two values of any sort type.
 * \param
 */
int
gencomp(dbref player, char *a, char *b, char *sort_type)
{
  char *ptr;
  int i, len;
  int result;
  s_rec s1, s2;
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
  ltypelist[i].make_record(&s1, player, ptr);
  ltypelist[i].make_record(&s2, player, ptr);
  result = ltypelist[i].sorter((const void *) &s1, (const void *) &s2);
  if (ltypelist[i].flags & IS_STRING) {
    if (s1.memo.str.freestr)
      mush_free(s1.memo.str.s, "genrecord");
    if (s2.memo.str.freestr)
      mush_free(s2.memo.str.s, "genrecord");
  }
  return result;
}

/** A generic sort routine to sort several different
 * types of arrays, in place.
 * \param player the player executing the sort.
 * \param s the array to sort.
 * \param n number of elements in array s
 * \param sort_type the string that describes the sort type.
 */

void
do_gensort(dbref player, char *keys[], char *strs[], int n, char *sort_type)
{
  char *ptr;
  static char stype[BUFFER_LEN];
  int i, sorti;
  s_rec *sp;
  ptr = NULL;
  if (!sort_type || !*sort_type) {
    /* Advance sorti to the default */
    for (sorti = 0; ltypelist[sorti].name; sorti++) ;
  } else if (strchr(sort_type, ':') != NULL) {
    strcpy(stype, sort_type);
    ptr = strchr(stype, ':');
    *(ptr++) = '\0';
    if (!*ptr)
      ptr = NULL;
    for (sorti = 0;
         ltypelist[sorti].name && strcasecmp(ltypelist[sorti].name, stype);
         sorti++) ;
  } else {
    for (sorti = 0;
         ltypelist[sorti].name && strcasecmp(ltypelist[sorti].name, sort_type);
         sorti++) ;
  }
  sp = mush_calloc(n, sizeof(s_rec), "do_gensort");
  for (i = 0; i < n; i++) {
    /* Elements are 0 by default thanks to calloc. Only need to touch
       those that need other values. */
    sp[i].val = keys[i];
    if (strs)
      sp[i].ptr = strs[i];
    if (ltypelist[sorti].flags & IS_DB) {
      sp[i].db = parse_objid(keys[i]);
      if (!RealGoodObject(sp[i].db))
        sp[i].db = NOTHING;
    }
    ltypelist[sorti].make_record(&(sp[i]), player, ptr);
  }
  qsort((void *) sp, n, sizeof(s_rec), ltypelist[sorti].sorter);

  for (i = 0; i < n; i++) {
    keys[i] = sp[i].val;
    if (strs) {
      strs[i] = sp[i].ptr;
    }
    if ((ltypelist[sorti].flags & IS_STRING) && sp[i].memo.str.freestr)
      mush_free(sp[i].memo.str.s, "genrecord");
  }
  mush_free(sp, "do_gensort");
}

typedef enum {
  L_NUMERIC,
  L_FLOAT,
  L_ALPHANUM,
  L_DBREF
} ltype;

char *
autodetect_list(char *ptrs[], int nptrs)
{
  char *sort_type;
  ltype lt;
  int i;

  lt = L_NUMERIC;
  sort_type = NUMERIC_LIST;

  for (i = 0; i < nptrs; i++) {
    switch (lt) {
    case L_NUMERIC:
      if (!is_strict_integer(ptrs[i])) {
        /* If it's not an integer, see if it's a floating-point number */
        if (is_strict_number(ptrs[i])) {
          lt = L_FLOAT;
          sort_type = FLOAT_LIST;
        } else if (i == 0) {

          /* If we get something non-numeric, switch to an
           * alphanumeric guess, unless this is the first
           * element and we have a dbref.
           */
          if (is_objid(ptrs[i])) {
            lt = L_DBREF;
            sort_type = DBREF_LIST;
          } else
            return ALPHANUM_LIST;
        }
      }
      break;
    case L_FLOAT:
      if (!is_strict_number(ptrs[i]))
        return ALPHANUM_LIST;
      break;
    case L_DBREF:
      if (!is_objid(ptrs[i]))
        return ALPHANUM_LIST;
      break;
    default:
      return ALPHANUM_LIST;
    }
  }
  return sort_type;
}
