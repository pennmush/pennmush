/**
 * \file function.c
 *
 * \brief The function parser.
 *
 *
 */

#include "copyrite.h"
#include "function.h"

#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "ansi.h"
#include "attrib.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "flags.h"
#include "funs.h"
#include "game.h"
#include "htab.h"
#include "lock.h"
#include "match.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "parse.h"
#include "sort.h"
#include "strutil.h"

#ifndef WITHOUT_WEBSOCKETS
#include "websock.h"
#endif /* undef WITHOUT_WEBSOCKETS */

static void func_hash_insert(const char *name, FUN *func);
extern void local_functions(void);
static int apply_restrictions(uint32_t result, const char *restriction);
static char *build_function_report(dbref player, FUN *fp);
static FUN *user_func_hash_lookup(const char *name);
static FUN *any_func_hash_lookup(const char *name);

HASHTAB htab_function;      /**< Function hash table */
HASHTAB htab_user_function; /**< User-defined function hash table */
slab *function_slab;        /**< slab for 'struct fun' allocations */

/* -------------------------------------------------------------------------*
 * Utilities.
 */

/** Check for a delimiter in an argument of a function call.
 * This function checks a given argument of a function call and sees
 * if it could be used as a delimiter. A delimiter must be a single
 * character. If the argument isn't present or is null, we return
 * the default delimiter, a space.
 * \param buff buffer to write error message to.
 * \param bp pointer into buff at which to write error.
 * \param nfargs number of arguments to the function.
 * \param fargs array of function arguments.
 * \param sep_arg index of the argument to check for a delimiter.
 * \param sep pointer to separator character, used to return separator.
 * \retval 0 illegal separator argument.
 * \retval 1 successfully returned a separator (maybe the default one).
 */
int
delim_check(char *buff, char **bp, int nfargs, char *fargs[], int sep_arg,
            char *sep)
{
  /* Find a delimiter. */

  if (nfargs >= sep_arg) {
    if (!*fargs[sep_arg - 1])
      *sep = ' ';
    else if (strlen(fargs[sep_arg - 1]) != 1) {
      safe_str(T("#-1 SEPARATOR MUST BE ONE CHARACTER"), buff, bp);
      return 0;
    } else
      *sep = *fargs[sep_arg - 1];
  } else
    *sep = ' ';

  return 1;
}

/** Check if a function argument is an integer.
 * If the arg is not given, assign a default value.
 * \param buff buffer to write error message to.
 * \param bp pointer into buff at which to write error.
 * \param nfargs number of arguments to the function.
 * \param fargs array of function arguments.
 * \param check_arg index of the argument to check for a delimiter.
 * \param result pointer to separator character, used to return separator.
 * \param def default to use if arg is not given
 * \retval 0 illegal separator argument.
 * \retval 1 successfully returned a separator (maybe the default one).
 */
bool
int_check(char *buff, char **bp, int nfargs, char *fargs[], int check_arg,
          int *result, int def)
{

  if (nfargs >= check_arg) {
    if (!*fargs[check_arg - 1])
      if (NULL_EQ_ZERO)
        *result = 0;
      else
        *result = def;
    else if (!is_strict_integer(fargs[check_arg - 1])) {
      safe_str(T(e_int), buff, bp);
      return 0;
    } else
      *result = parse_integer(fargs[check_arg - 1]);
  } else
    *result = def;

  return 1;
}

/* --------------------------------------------------------------------------
 * The actual function handlers
 */

/** An entry in the function table.
 * This structure represents a function's entry in the function table.
 */
typedef struct fun_tab {
  const char *name;  /**< Name of the function, uppercase. */
  function_func fun; /**< Pointer to code to call for this function. */
  int minargs;       /**< Minimum args required. */
  int maxargs; /**< Maximum args, or INT_MAX. If <0, last arg may have commas */
  int flags;   /**< Flags to control how the function is parsed. */
} FUNTAB;

/** A hardcoded function alias.
 * These are functions which used to be duplicated, but are now properly
 * aliased.
 * They're added here instead of alias.cnf to avoid breakage for people who
 * don't update their alias.cnf immediately. */
typedef struct fun_alias {
  const char *name;  /**< Name of function to alias */
  const char *alias; /**< Name of alias to create */
} FUNALIAS;

/* Table of hardcoded function aliases. Aliases can also be added with
 * @function/alias or a function_alias directive in alias.cnf, both of
 * which call the alias_function() function
 */
FUNALIAS faliases[] = {{"UFUN", "U"},
                       {"IDLE", "IDLESECS"},
                       {"HOST", "HOSTNAME"},
                       {"FLIP", "REVERSE"},
                       {"E", "EXP"},
                       {"STRDELETE", "DELETE"},
                       {"LREPLACE", "REPLACE"},
                       {"LINSERT", "INSERT"},
                       {"MONIKER", "CNAME"}, /* Rhost alias */
                       {"MEAN", "AVG"},      /* Rhost alias */
                       {"MATCH", "ELEMENT"},
                       {"SPEAK", "SPEAKPENN"}, /* Backwards compatability */
                       {NULL, NULL}};

/** The function table. Functions can also be added at runtime with
 * add_function().
 */
FUNTAB flist[] = {
  {"@@", fun_null, 1, INT_MAX, FN_NOPARSE},
  {"ABS", fun_abs, 1, 1, FN_REG | FN_STRIPANSI},
  {"ACCENT", fun_accent, 2, 2, FN_REG},
  {"ACCNAME", fun_accname, 1, 1, FN_REG},
  {"ADD", fun_add, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"AFTER", fun_after, 2, 2, FN_REG},
  {"ALIAS", fun_alias, 1, 2, FN_REG},
  {"ALIGN", fun_align, 2, INT_MAX, FN_REG},
  {"LALIGN", fun_align, 2, 6, FN_REG},
  {"ALLOF", fun_allof, 2, INT_MAX, FN_NOPARSE},
  {"ALPHAMAX", fun_alphamax, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"ALPHAMIN", fun_alphamin, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"AND", fun_and, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"ANDFLAGS", fun_andflags, 2, 2, FN_REG | FN_STRIPANSI},
  {"ANDLFLAGS", fun_andlflags, 2, 2, FN_REG | FN_STRIPANSI},
  {"ANDLPOWERS", fun_andlflags, 2, 2, FN_REG | FN_STRIPANSI},
  {"ANSI", fun_ansi, 2, -2, FN_REG},
#if defined(ANSI_DEBUG) || defined(DEBUG_PENNMUSH)
  {"ANSIGEN", fun_ansigen, 1, 1, FN_REG},
#endif
  {"APOSS", fun_aposs, 1, 1, FN_REG | FN_STRIPANSI},
  {"ART", fun_art, 1, 1, FN_REG | FN_STRIPANSI},
  {"ATRLOCK", fun_atrlock, 1, 2, FN_REG | FN_STRIPANSI},
  {"ATTRIB_SET", fun_attrib_set, 1, -2, FN_REG},
  {"BAND", fun_band, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"BASECONV", fun_baseconv, 3, 3, FN_REG | FN_STRIPANSI},
  {"BEEP", fun_beep, 0, 1, FN_REG | FN_ADMIN | FN_STRIPANSI},
  {"BEFORE", fun_before, 2, 2, FN_REG},
  {"BENCHMARK", fun_benchmark, 2, 3, FN_NOPARSE},
  {"BNAND", fun_bnand, 2, 2, FN_REG | FN_STRIPANSI},
  {"BNOT", fun_bnot, 1, 1, FN_REG | FN_STRIPANSI},
  {"BOR", fun_bor, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"BOUND", fun_bound, 2, 3, FN_REG | FN_STRIPANSI},
  {"BRACKETS", fun_brackets, 1, 1, FN_REG | FN_STRIPANSI},
  {"BXOR", fun_bxor, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"CAND", fun_cand, 2, INT_MAX, FN_NOPARSE | FN_STRIPANSI},
  {"NCAND", fun_cand, 1, INT_MAX, FN_NOPARSE | FN_STRIPANSI},
  {"CAPSTR", fun_capstr, 1, -1, FN_REG},
  {"CASE", fun_switch, 3, INT_MAX, FN_NOPARSE},
  {"CASEALL", fun_switch, 3, INT_MAX, FN_NOPARSE},
  {"CAT", fun_cat, 1, INT_MAX, FN_REG},
  {"CBUFFER", fun_cinfo, 1, 1, FN_REG},
  {"CBUFFERADD", fun_cbufferadd, 2, 3, FN_REG},
  {"CDESC", fun_cinfo, 1, 1, FN_REG},
  {"CEMIT", fun_cemit, 2, 3, FN_REG},
  {"CFLAGS", fun_cflags, 1, 2, FN_REG | FN_STRIPANSI},
  {"CHANNELS", fun_channels, 0, 2, FN_REG | FN_STRIPANSI},
  {"CLFLAGS", fun_cflags, 1, 2, FN_REG | FN_STRIPANSI},
  {"CLOCK", fun_clock, 1, 2, FN_REG | FN_STRIPANSI},
  {"CMOGRIFIER", fun_cmogrifier, 1, 1, FN_REG | FN_STRIPANSI},
  {"CMSGS", fun_cinfo, 1, 1, FN_REG | FN_STRIPANSI},
  {"COLORS", fun_colors, 0, 2, FN_REG | FN_STRIPANSI},
  {"COWNER", fun_cowner, 1, 1, FN_REG | FN_STRIPANSI},
  {"CRECALL", fun_crecall, 1, 5, FN_REG | FN_STRIPANSI},
  {"CSTATUS", fun_cstatus, 2, 2, FN_REG | FN_STRIPANSI},
  {"CTITLE", fun_ctitle, 2, 2, FN_REG | FN_STRIPANSI},
  {"CUSERS", fun_cinfo, 1, 1, FN_REG | FN_STRIPANSI},
  {"CWHO", fun_cwho, 1, 3, FN_REG | FN_STRIPANSI},
  {"CENTER", fun_center, 2, 4, FN_REG},
  {"CHILDREN", fun_lsearch, 1, 1, FN_REG | FN_STRIPANSI},
  {"CHR", fun_chr, 1, 1, FN_REG | FN_STRIPANSI},
  {"CHECKPASS", fun_checkpass, 2, 2, FN_REG | FN_WIZARD | FN_STRIPANSI},
  {"CLONE", fun_clone, 1, 3, FN_REG},
  {"CMDS", fun_cmds, 1, 1, FN_REG | FN_STRIPANSI},
  {"COMP", fun_comp, 2, 3, FN_REG | FN_STRIPANSI},
  {"CON", fun_con, 1, 1, FN_REG | FN_STRIPANSI},
  {"COND", fun_if, 2, INT_MAX, FN_NOPARSE},
  {"CONDALL", fun_if, 2, INT_MAX, FN_NOPARSE},
  {"CONFIG", fun_config, 1, 1, FN_REG | FN_STRIPANSI},
  {"CONN", fun_conn, 1, 1, FN_REG | FN_STRIPANSI},
  {"CONTROLS", fun_controls, 2, 2, FN_REG | FN_STRIPANSI},
  {"CONVSECS", fun_convsecs, 1, 2, FN_REG | FN_STRIPANSI},
  {"CONVUTCSECS", fun_convsecs, 1, 1, FN_REG | FN_STRIPANSI},
  {"CONVTIME", fun_convtime, 1, 2, FN_REG | FN_STRIPANSI},
  {"CONVUTCTIME", fun_convtime, 1, 1, FN_REG | FN_STRIPANSI},
  {"COR", fun_cor, 2, INT_MAX, FN_NOPARSE | FN_STRIPANSI},
  {"NCOR", fun_cor, 1, INT_MAX, FN_NOPARSE | FN_STRIPANSI},
  {"CREATE", fun_create, 1, 3, FN_REG},
  {"CSECS", fun_csecs, 1, 1, FN_REG | FN_STRIPANSI},
  {"CTIME", fun_ctime, 1, 2, FN_REG | FN_STRIPANSI},
  {"DEC", fun_dec, 1, 1, FN_REG | FN_STRIPANSI},
  {"DECODE64", fun_decode64, 1, -1, FN_REG},
  {"DECOMPOSE", fun_decompose, 1, -1, FN_REG},
  {"DECRYPT", fun_decrypt, 2, 3, FN_REG},
  {"DEFAULT", fun_default, 2, INT_MAX, FN_NOPARSE},
  {"STRDELETE", fun_delete, 3, 3, FN_REG},
  {"DIE", fun_die, 2, 3, FN_REG | FN_STRIPANSI},
  {"DIG", fun_dig, 1, 6, FN_REG},
  {"DIGEST", fun_digest, 1, -2, FN_REG},
  {"DIST2D", fun_dist2d, 4, 4, FN_REG | FN_STRIPANSI},
  {"DIST3D", fun_dist3d, 6, 6, FN_REG | FN_STRIPANSI},
  {"DIV", fun_div, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"DOING", fun_doing, 1, 1, FN_REG | FN_STRIPANSI},
  {"EDEFAULT", fun_edefault, 2, 2, FN_NOPARSE},
  {"EDIT", fun_edit, 3, INT_MAX, FN_REG},
  {"ELEMENTS", fun_elements, 2, 4, FN_REG},
  {"ELIST", fun_itemize, 1, 5, FN_REG},
  {"ELOCK", fun_elock, 2, 2, FN_REG | FN_STRIPANSI},
  {"EMIT", fun_emit, 1, -1, FN_REG},
  {"ENCODE64", fun_encode64, 1, -1, FN_REG},
  {"ENCRYPT", fun_encrypt, 2, 3, FN_REG},
  {"ENTRANCES", fun_entrances, 0, 4, FN_REG | FN_STRIPANSI},
  {"ETIME", fun_etime, 1, 2, FN_REG},
  {"ETIMEFMT", fun_etimefmt, 2, 2, FN_REG},
  {"EQ", fun_eq, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"EVAL", fun_eval, 2, 2, FN_REG},
  {"ESCAPE", fun_escape, 1, -1, FN_REG},
  {"EXIT", fun_exit, 1, 1, FN_REG | FN_STRIPANSI},
  {"EXTRACT", fun_extract, 1, 4, FN_REG},
  {"FILTER", fun_filter, 2, MAX_STACK_ARGS + 3, FN_REG},
  {"FILTERBOOL", fun_filter, 2, MAX_STACK_ARGS + 3, FN_REG},
  {"FINDABLE", fun_findable, 2, 2, FN_REG | FN_STRIPANSI},
  {"FIRST", fun_first, 1, 2, FN_REG},
  {"FIRSTOF", fun_firstof, 0, INT_MAX, FN_NOPARSE},
  {"FLAGS", fun_flags, 0, 1, FN_REG | FN_STRIPANSI},
  {"FLIP", fun_flip, 1, -1, FN_REG},
  {"FLOORDIV", fun_floordiv, 2, INT_MAX, FN_REG},
  {"FN", fun_fn, 1, INT_MAX, FN_NOPARSE},
  {"FOLD", fun_fold, 2, 4, FN_REG},
  {"FOLDERSTATS", fun_folderstats, 0, 2, FN_REG | FN_STRIPANSI},
  {"FOLLOWERS", fun_followers, 1, 1, FN_REG | FN_STRIPANSI},
  {"FOLLOWING", fun_following, 1, 1, FN_REG | FN_STRIPANSI},
  {"FOREACH", fun_foreach, 2, 4, FN_REG},
  {"FRACTION", fun_fraction, 1, 2, FN_REG | FN_STRIPANSI},
  {"FUNCTIONS", fun_functions, 0, 1, FN_REG | FN_STRIPANSI},
  {"FULLALIAS", fun_fullalias, 1, 1, FN_REG | FN_STRIPANSI},
  {"FULLNAME", fun_fullname, 1, 1, FN_REG | FN_STRIPANSI},
  {"GET", fun_get, 1, 1, FN_REG | FN_STRIPANSI},
  {"GETPIDS", fun_lpids, 1, 1, FN_REG | FN_STRIPANSI},
  {"GET_EVAL", fun_get_eval, 1, 1, FN_REG},
  {"GRAB", fun_grab, 2, 3, FN_REG},
  {"GRABALL", fun_graball, 2, 4, FN_REG},
  {"GREP", fun_grep, 3, 3, FN_REG},
  {"PGREP", fun_grep, 3, 3, FN_REG},
  {"GREPI", fun_grep, 3, 3, FN_REG},
  {"GT", fun_gt, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"GTE", fun_gte, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"HASATTR", fun_hasattr, 1, 2, FN_REG | FN_STRIPANSI},
  {"HASATTRP", fun_hasattr, 1, 2, FN_REG | FN_STRIPANSI},
  {"HASATTRPVAL", fun_hasattr, 1, 2, FN_REG | FN_STRIPANSI},
  {"HASATTRVAL", fun_hasattr, 1, 2, FN_REG | FN_STRIPANSI},
  {"HASFLAG", fun_hasflag, 2, 2, FN_REG | FN_STRIPANSI},
  {"HASPOWER", fun_haspower, 2, 2, FN_REG | FN_STRIPANSI},
  {"HASTYPE", fun_hastype, 2, 2, FN_REG | FN_STRIPANSI},
  {"HEIGHT", fun_height, 1, 2, FN_REG | FN_STRIPANSI},
  {"HIDDEN", fun_hidden, 1, 1, FN_REG | FN_STRIPANSI},
  {"HOME", fun_home, 1, 1, FN_REG | FN_STRIPANSI},
  {"HOST", fun_hostname, 1, 1, FN_REG | FN_STRIPANSI},
  {"IBREAK", fun_ibreak, 0, 1, FN_REG | FN_STRIPANSI},
  {"IDLE", fun_idlesecs, 1, 1, FN_REG | FN_STRIPANSI},
  {"IF", fun_if, 2, 3, FN_NOPARSE},
  {"IFELSE", fun_if, 3, 3, FN_NOPARSE},
  {"ILEV", fun_ilev, 0, 0, FN_REG | FN_STRIPANSI},
  {"INAME", fun_iname, 1, 1, FN_REG | FN_STRIPANSI},
  {"INC", fun_inc, 1, 1, FN_REG | FN_STRIPANSI},
  {"INDEX", fun_index, 4, 4, FN_REG},
  {"LINSERT", fun_insert, 3, 4, FN_REG},
  {"INUM", fun_inum, 1, 1, FN_REG | FN_STRIPANSI},
  {"IPADDR", fun_ipaddr, 1, 1, FN_REG | FN_STRIPANSI},
  {"ISDAYLIGHT", fun_isdaylight, 0, 2, FN_REG},
  {"ISDBREF", fun_isdbref, 1, 1, FN_REG | FN_STRIPANSI},
  {"ISINT", fun_isint, 1, 1, FN_REG | FN_STRIPANSI},
  {"ISNUM", fun_isnum, 1, 1, FN_REG | FN_STRIPANSI},
  {"ISOBJID", fun_isobjid, 1, 1, FN_REG | FN_STRIPANSI},
  {"ISREGEXP", fun_isregexp, 1, 1, FN_REG | FN_STRIPANSI},
  {"ISWORD", fun_isword, 1, 1, FN_REG | FN_STRIPANSI},
  {"ITER", fun_iter, 2, 4, FN_NOPARSE},
  {"ITEMS", fun_items, 2, 2, FN_REG | FN_STRIPANSI},
  {"ITEMIZE", fun_itemize, 1, 4, FN_REG},
  {"ITEXT", fun_itext, 1, 1, FN_REG | FN_STRIPANSI},
  {"JSON", fun_json, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"JSON_MAP", fun_json_map, 2, MAX_STACK_ARGS + 1, FN_REG | FN_STRIPANSI},
  {"JSON_QUERY", fun_json_query, 1, 3, FN_REG | FN_STRIPANSI},
  {"LAST", fun_last, 1, 2, FN_REG},
  {"LATTR", fun_lattr, 1, 2, FN_REG | FN_STRIPANSI},
  {"LATTRP", fun_lattr, 1, 2, FN_REG | FN_STRIPANSI},
  {"LCON", fun_dbwalker, 1, 2, FN_REG | FN_STRIPANSI},
  {"LCSTR", fun_lcstr, 1, -1, FN_REG},
  {"LDELETE", fun_ldelete, 2, 4, FN_REG},
  {"LEFT", fun_left, 2, 2, FN_REG},
  {"LEMIT", fun_lemit, 1, -1, FN_REG},
  {"LETQ", fun_letq, 1, INT_MAX, FN_NOPARSE},
  {"LEXITS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"LFLAGS", fun_lflags, 0, 1, FN_REG | FN_STRIPANSI},
  {"LINK", fun_link, 2, 3, FN_REG | FN_STRIPANSI},
  {"LIST", fun_list, 1, 2, FN_REG | FN_STRIPANSI},
  {"LISTQ", fun_listq, 0, 1, FN_REG | FN_STRIPANSI},
  {"LIT", fun_lit, 1, -1, FN_LITERAL},
  {"LJUST", fun_ljust, 2, 4, FN_REG},
  {"LLOCKFLAGS", fun_lockflags, 0, 1, FN_REG | FN_STRIPANSI},
  {"LLOCKS", fun_locks, 0, 1, FN_REG | FN_STRIPANSI},
  {"LMATH", fun_lmath, 2, 3, FN_REG | FN_STRIPANSI},
  {"LNUM", fun_lnum, 1, 4, FN_REG | FN_STRIPANSI},
  {"LOC", fun_loc, 1, 1, FN_REG | FN_STRIPANSI},
  {"LOCALIZE", fun_localize, 1, 1, FN_NOPARSE},
  {"LOCATE", fun_locate, 3, 3, FN_REG | FN_STRIPANSI},
  {"LOCK", fun_lock, 1, 2, FN_REG | FN_STRIPANSI},
  {"LOCKFILTER", fun_lockfilter, 2, 3, FN_REG | FN_STRIPANSI},
  {"LOCKFLAGS", fun_lockflags, 0, 1, FN_REG | FN_STRIPANSI},
  {"LOCKOWNER", fun_lockowner, 1, 1, FN_REG | FN_STRIPANSI},
  {"LOCKS", fun_locks, 1, 1, FN_REG | FN_STRIPANSI},
  {"LPARENT", fun_lparent, 1, 1, FN_REG | FN_STRIPANSI},
  {"LPIDS", fun_lpids, 0, 2, FN_REG | FN_STRIPANSI},
  {"LPLAYERS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"LPORTS", fun_lports, 0, 2, FN_REG | FN_STRIPANSI},
  {"LPOS", fun_lpos, 2, 2, FN_REG | FN_STRIPANSI},
  {"LSEARCH", fun_lsearch, 1, INT_MAX, FN_REG},
  {"LSEARCHR", fun_lsearch, 1, INT_MAX, FN_REG},
  {"LSET", fun_lset, 2, 2, FN_REG | FN_STRIPANSI},
  {"LSTATS", fun_lstats, 0, 1, FN_REG | FN_STRIPANSI},
  {"LT", fun_lt, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"LTE", fun_lte, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"LTHINGS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"LVCON", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"LVEXITS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"LVPLAYERS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"LVTHINGS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"LWHO", fun_lwho, 0, 2, FN_REG | FN_STRIPANSI},
  {"LWHOID", fun_lwho, 0, 1, FN_REG | FN_STRIPANSI},
  {"MAIL", fun_mail, 0, 2, FN_REG | FN_STRIPANSI},
  {"MAILLIST", fun_maillist, 0, 2, FN_REG | FN_STRIPANSI},
  {"MAILFROM", fun_mailfrom, 1, 2, FN_REG | FN_STRIPANSI},
  {"MAILSEND", fun_mailsend, 2, 2, FN_REG},
  {"MAILSTATS", fun_mailstats, 1, 1, FN_REG | FN_STRIPANSI},
  {"MAILDSTATS", fun_mailstats, 1, 1, FN_REG | FN_STRIPANSI},
  {"MAILFSTATS", fun_mailstats, 1, 1, FN_REG | FN_STRIPANSI},
  {"MAILSTATUS", fun_mailstatus, 1, 2, FN_REG | FN_STRIPANSI},
  {"MAILSUBJECT", fun_mailsubject, 1, 2, FN_REG | FN_STRIPANSI},
  {"MAILTIME", fun_mailtime, 1, 2, FN_REG | FN_STRIPANSI},
  {"MALIAS", fun_malias, 0, 2, FN_REG | FN_STRIPANSI},
  {"MAP", fun_map, 2, 4, FN_REG},
  {"MAPSQL", fun_mapsql, 2, 4, FN_REG},
  {"MATCH", fun_match, 2, 3, FN_REG | FN_STRIPANSI},
  {"MATCHALL", fun_matchall, 2, 4, FN_REG | FN_STRIPANSI},
  {"MAX", fun_max, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"MEAN", fun_mean, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"MEDIAN", fun_median, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"MEMBER", fun_member, 2, 3, FN_REG | FN_STRIPANSI | FN_STRIPANSI},
  {"MERGE", fun_merge, 3, 3, FN_REG},
  {"MESSAGE", fun_message, 3, 14, FN_REG},
  {"MID", fun_mid, 3, 3, FN_REG},
  {"MIN", fun_min, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"MIX", fun_mix, 3, (MAX_STACK_ARGS + 3), FN_REG},
  {"MODULO", fun_modulo, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"MONEY", fun_money, 1, 1, FN_REG | FN_STRIPANSI},
  {"MSECS", fun_msecs, 1, 1, FN_REG | FN_STRIPANSI},
  {"MTIME", fun_mtime, 1, 2, FN_REG | FN_STRIPANSI},
  {"MUDNAME", fun_mudname, 0, 0, FN_REG},
  {"MUDURL", fun_mudurl, 0, 0, FN_REG},
  {"MUL", fun_mul, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"MUNGE", fun_munge, 3, 5, FN_REG},
  {"MWHO", fun_lwho, 0, 0, FN_REG | FN_STRIPANSI},
  {"MWHOID", fun_lwho, 0, 0, FN_REG | FN_STRIPANSI},
  {"NAME", fun_name, 1, 2, FN_REG | FN_STRIPANSI},
  {"MONIKER", fun_moniker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NAMELIST", fun_namelist, 1, 2, FN_REG},
  {"NAMEGRAB", fun_namegrab, 2, 3, FN_REG | FN_STRIPANSI},
  {"NAMEGRABALL", fun_namegraball, 2, 3, FN_REG | FN_STRIPANSI},
  {"NAND", fun_nand, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"NATTR", fun_nattr, 1, 1, FN_REG | FN_STRIPANSI},
  {"NATTRP", fun_nattr, 1, 1, FN_REG | FN_STRIPANSI},
  {"NCHILDREN", fun_lsearch, 1, 1, FN_REG | FN_STRIPANSI},
  {"NCON", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NCOND", fun_if, 2, INT_MAX, FN_NOPARSE},
  {"NCONDALL", fun_if, 2, INT_MAX, FN_NOPARSE},
  {"NEXITS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NPLAYERS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NEARBY", fun_nearby, 2, 2, FN_REG | FN_STRIPANSI},
  {"NEQ", fun_neq, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"NEXT", fun_next, 1, 1, FN_REG | FN_STRIPANSI},
  {"NEXTDBREF", fun_nextdbref, 0, 0, FN_REG},
  {"NLSEARCH", fun_lsearch, 1, INT_MAX, FN_REG},
  {"NMWHO", fun_nwho, 0, 0, FN_REG},
  {"NOR", fun_nor, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"NOT", fun_not, 1, 1, FN_REG | FN_STRIPANSI},
  {"NSCEMIT", fun_cemit, 2, 3, FN_REG},
  {"NSEARCH", fun_lsearch, 1, INT_MAX, FN_REG},
  {"NSEMIT", fun_emit, 1, -1, FN_REG},
  {"NSLEMIT", fun_lemit, 1, -1, FN_REG},
  {"NSOEMIT", fun_oemit, 2, -2, FN_REG},
  {"NSPEMIT", fun_pemit, 2, -2, FN_REG},
  {"NSPROMPT", fun_prompt, 2, -2, FN_REG},
  {"NSREMIT", fun_remit, 2, -2, FN_REG},
  {"NSZEMIT", fun_zemit, 2, -2, FN_REG},
  {"NTHINGS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NUM", fun_num, 1, 1, FN_REG | FN_STRIPANSI},
  {"NUMVERSION", fun_numversion, 0, 0, FN_REG},
  {"NULL", fun_null, 1, INT_MAX, FN_REG},
  {"NVCON", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NVEXITS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NVPLAYERS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NVTHINGS", fun_dbwalker, 1, 1, FN_REG | FN_STRIPANSI},
  {"NWHO", fun_nwho, 0, 1, FN_REG | FN_STRIPANSI},
  {"OBJ", fun_obj, 1, 1, FN_REG | FN_STRIPANSI},
  {"OBJEVAL", fun_objeval, 2, -2, FN_NOPARSE},
  {"OBJID", fun_objid, 1, 1, FN_REG | FN_STRIPANSI},
  {"OBJMEM", fun_objmem, 1, 1, FN_REG | FN_STRIPANSI},
  {"OEMIT", fun_oemit, 2, -2, FN_REG},
  {"OOB", fun_oob, 2, 3, FN_REG | FN_STRIPANSI},
  {"OPEN", fun_open, 1, 4, FN_REG},
  {"OR", fun_or, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"ORD", fun_ord, 1, 1, FN_REG | FN_STRIPANSI},
  {"ORDINAL", fun_spellnum, 1, 1, FN_REG | FN_STRIPANSI},
  {"ORFLAGS", fun_orflags, 2, 2, FN_REG | FN_STRIPANSI},
  {"ORLFLAGS", fun_orlflags, 2, 2, FN_REG | FN_STRIPANSI},
  {"ORLPOWERS", fun_orlflags, 2, 2, FN_REG | FN_STRIPANSI},
  {"OWNER", fun_owner, 1, 1, FN_REG | FN_STRIPANSI},
  {"PARENT", fun_parent, 1, 2, FN_REG | FN_STRIPANSI},
  {"PCREATE", fun_pcreate, 2, 3, FN_REG},
  {"PEMIT", fun_pemit, 2, -2, FN_REG},
  {"PIDINFO", fun_pidinfo, 1, 3, FN_REG | FN_STRIPANSI},
  {"PLAYERMEM", fun_playermem, 1, 1, FN_REG | FN_STRIPANSI},
  {"PLAYER", fun_player, 1, 1, FN_REG | FN_STRIPANSI},
  {"PMATCH", fun_pmatch, 1, 1, FN_REG | FN_STRIPANSI},
  {"POLL", fun_poll, 0, 0, FN_REG},
  {"PORTS", fun_ports, 1, 1, FN_REG | FN_STRIPANSI},
  {"POS", fun_pos, 2, 2, FN_REG | FN_STRIPANSI},
  {"POSS", fun_poss, 1, 1, FN_REG | FN_STRIPANSI},
  {"POWERS", fun_powers, 0, 2, FN_REG | FN_STRIPANSI},
  {"PROMPT", fun_prompt, 2, -2, FN_REG},
  {"PUEBLO", fun_pueblo, 1, 1, FN_REG | FN_STRIPANSI},
  {"QUOTA", fun_quota, 1, 1, FN_REG | FN_STRIPANSI},
  {"R", fun_r, 1, 2, FN_REG | FN_STRIPANSI},
  {"RAND", fun_rand, 0, 2, FN_REG | FN_STRIPANSI},
  {"RANDEXTRACT", fun_randword, 1, 5, FN_REG},
  {"RANDWORD", fun_randword, 1, 2, FN_REG},
  {"RECV", fun_recv, 1, 1, FN_REG | FN_STRIPANSI},
  {"REGEDIT", fun_regreplace, 3, INT_MAX, FN_NOPARSE},
  {"REGEDITALL", fun_regreplace, 3, INT_MAX, FN_NOPARSE},
  {"REGEDITALLI", fun_regreplace, 3, INT_MAX, FN_NOPARSE},
  {"REGEDITI", fun_regreplace, 3, INT_MAX, FN_NOPARSE},
  {"REGMATCH", fun_regmatch, 2, 3, FN_REG},
  {"REGMATCHI", fun_regmatch, 2, 3, FN_REG},
  {"REGRAB", fun_regrab, 2, 4, FN_REG},
  {"REGRABALL", fun_regrab, 2, 4, FN_REG},
  {"REGRABALLI", fun_regrab, 2, 4, FN_REG},
  {"REGRABI", fun_regrab, 2, 3, FN_REG},
  {"REGLMATCH", fun_regrab, 2, 3, FN_REG},
  {"REGLMATCHI", fun_regrab, 2, 3, FN_REG},
  {"REGLMATCHALL", fun_regrab, 2, 4, FN_REG},
  {"REGLMATCHALLI", fun_regrab, 2, 4, FN_REG},
  {"REGREP", fun_grep, 3, 3, FN_REG},
  {"REGREPI", fun_grep, 3, 3, FN_REG},
  {"REGLATTR", fun_lattr, 1, 2, FN_REG},
  {"REGLATTRP", fun_lattr, 1, 2, FN_REG},
  {"REGNATTR", fun_nattr, 1, 1, FN_REG},
  {"REGNATTRP", fun_nattr, 1, 1, FN_REG},
  {"REGXATTR", fun_lattr, 3, 4, FN_REG},
  {"REGXATTRP", fun_lattr, 3, 4, FN_REG},
  {"RESWITCH", fun_reswitch, 3, INT_MAX, FN_NOPARSE},
  {"RESWITCHALL", fun_reswitch, 3, INT_MAX, FN_NOPARSE},
  {"RESWITCHALLI", fun_reswitch, 3, INT_MAX, FN_NOPARSE},
  {"RESWITCHI", fun_reswitch, 3, INT_MAX, FN_NOPARSE},
  {"REGISTERS", fun_listq, 0, 3, FN_REG | FN_STRIPANSI},
  {"REMAINDER", fun_remainder, 2, INT_MAX, FN_REG},
  {"REMIT", fun_remit, 2, -2, FN_REG},
  {"REMOVE", fun_remove, 2, 3, FN_REG},
  {"RENDER", fun_render, 2, 2, FN_REG},
  {"REPEAT", fun_repeat, 2, 2, FN_REG},
  {"LREPLACE", fun_ldelete, 3, 5, FN_REG},
  {"REST", fun_rest, 1, 2, FN_REG},
  {"RESTARTS", fun_restarts, 0, 0, FN_REG},
  {"RESTARTTIME", fun_restarttime, 0, 0, FN_REG},
  {"REVWORDS", fun_revwords, 1, 3, FN_REG},
  {"RIGHT", fun_right, 2, 2, FN_REG},
  {"RJUST", fun_rjust, 2, 4, FN_REG},
  {"RLOC", fun_rloc, 2, 2, FN_REG | FN_STRIPANSI},
  {"RNUM", fun_rnum, 2, 2, FN_REG | FN_STRIPANSI | FN_DEPRECATED},
  {"ROOM", fun_room, 1, 1, FN_REG | FN_STRIPANSI},
  {"ROOT", fun_root, 2, 2, FN_REG | FN_STRIPANSI},
  {"S", fun_s, 1, -1, FN_REG},
  {"SCAN", fun_scan, 1, 3, FN_REG | FN_STRIPANSI},
  {"SCRAMBLE", fun_scramble, 1, -1, FN_REG},
  {"SECS", fun_secs, 0, 0, FN_REG},
  {"SECURE", fun_secure, 1, -1, FN_REG},
  {"SENT", fun_sent, 1, 1, FN_REG | FN_STRIPANSI},
  {"SET", fun_set, 2, 2, FN_REG},
  {"SETQ", fun_setq, 2, INT_MAX, FN_REG},
  {"SETR", fun_setq, 2, INT_MAX, FN_REG},
  {"SETDIFF", fun_setmanip, 2, 5, FN_REG},
  {"SETINTER", fun_setmanip, 2, 5, FN_REG},
  {"SETSYMDIFF", fun_setmanip, 2, 5, FN_REG},
  {"SETUNION", fun_setmanip, 2, 5, FN_REG},
  {"SHA0", fun_sha0, 1, 1, FN_REG | FN_DEPRECATED},
  {"SHL", fun_shl, 2, 2, FN_REG | FN_STRIPANSI},
  {"SHR", fun_shr, 2, 2, FN_REG | FN_STRIPANSI},
  {"SHUFFLE", fun_shuffle, 1, 3, FN_REG},
  {"SIGN", fun_sign, 1, 1, FN_REG | FN_STRIPANSI},
  {"SORT", fun_sort, 1, 4, FN_REG},
  {"SORTBY", fun_sortby, 2, 4, FN_REG},
  {"SORTKEY", fun_sortkey, 2, 5, FN_REG},
  {"SOUNDEX", fun_soundex, 1, 1, FN_REG | FN_STRIPANSI},
  {"SOUNDSLIKE", fun_soundlike, 2, 2, FN_REG | FN_STRIPANSI},
  {"SPACE", fun_space, 1, 1, FN_REG | FN_STRIPANSI},
  {"SPEAK", fun_speak, 2, 7, FN_REG},
  {"SPELLNUM", fun_spellnum, 1, 1, FN_REG | FN_STRIPANSI},
  {"SPLICE", fun_splice, 3, 4, FN_REG},
  {"SQL", fun_sql, 1, 4, FN_REG},
  {"SQLESCAPE", fun_sql_escape, 1, -1, FN_REG},
  {"SQUISH", fun_squish, 1, 2, FN_REG},
  {"SSL", fun_ssl, 1, 1, FN_REG | FN_STRIPANSI},
  {"STARTTIME", fun_starttime, 0, 0, FN_REG},
  {"STEP", fun_step, 3, 5, FN_REG},
  {"STRFIRSTOF", fun_firstof, 2, INT_MAX, FN_NOPARSE},
  {"STRALLOF", fun_allof, 2, INT_MAX, FN_NOPARSE},
  {"STRCAT", fun_strcat, 1, INT_MAX, FN_REG},
  {"STRINGSECS", fun_stringsecs, 1, 1, FN_REG | FN_STRIPANSI},
  {"STRINSERT", fun_str_rep_or_ins, 3, -3, FN_REG},
  {"STRIPACCENTS", fun_stripaccents, 1, 1, FN_REG},
  {"STRIPANSI", fun_stripansi, 1, -1, FN_REG | FN_STRIPANSI},
  {"STRLEN", fun_strlen, 1, -1, FN_REG},
  {"STRMATCH", fun_strmatch, 2, 3, FN_REG},
  {"STRREPLACE", fun_str_rep_or_ins, 4, 4, FN_REG},
  {"SUB", fun_sub, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"SUBJ", fun_subj, 1, 1, FN_REG | FN_STRIPANSI},
  {"SWITCH", fun_switch, 3, INT_MAX, FN_NOPARSE},
  {"SWITCHALL", fun_switch, 3, INT_MAX, FN_NOPARSE},
  {"SLEV", fun_slev, 0, 0, FN_REG},
  {"STEXT", fun_stext, 1, 1, FN_REG | FN_STRIPANSI},
  {"T", fun_t, 1, 1, FN_REG | FN_STRIPANSI},
  {"TABLE", fun_table, 1, 5, FN_REG},
  {"TEL", fun_tel, 2, 4, FN_REG | FN_STRIPANSI},
  {"TERMINFO", fun_terminfo, 1, 1, FN_REG | FN_STRIPANSI},
  {"TESTLOCK", fun_testlock, 2, 2, FN_REG | FN_STRIPANSI},
  {"TEXTENTRIES", fun_textentries, 2, 3, FN_REG | FN_STRIPANSI},
  {"TEXTFILE", fun_textfile, 2, 2, FN_REG | FN_STRIPANSI},
  {"TEXTSEARCH", fun_textsearch, 2, 3, FN_REG | FN_STRIPANSI},
  {"TIME", fun_time, 0, 1, FN_REG | FN_STRIPANSI},
  {"TIMEFMT", fun_timefmt, 1, 3, FN_REG},
  {"TIMESTRING", fun_timestring, 1, 2, FN_REG | FN_STRIPANSI},
  {"TR", fun_tr, 3, 3, FN_REG},
  {"TRIM", fun_trim, 1, 3, FN_REG},
  {"TRIMPENN", fun_trim, 1, 3, FN_REG},
  {"TRIMTINY", fun_trim, 1, 3, FN_REG},
  {"TRUNC", fun_trunc, 1, 1, FN_REG | FN_STRIPANSI},
  {"TYPE", fun_type, 1, 1, FN_REG | FN_STRIPANSI},
  {"UCSTR", fun_ucstr, 1, -1, FN_REG},
  {"UDEFAULT", fun_udefault, 2, 12, FN_NOPARSE},
  {"UFUN", fun_ufun, 1, (MAX_STACK_ARGS + 1), FN_REG},
  {"PFUN", fun_pfun, 1, (MAX_STACK_ARGS + 1), FN_REG},
  {"ULAMBDA", fun_ufun, 1, (MAX_STACK_ARGS + 1), FN_REG},
  {"ULDEFAULT", fun_udefault, 1, (MAX_STACK_ARGS + 2),
   FN_NOPARSE | FN_LOCALIZE},
  {"ULOCAL", fun_ufun, 1, (MAX_STACK_ARGS + 1), FN_REG | FN_LOCALIZE},
  {"UNIQUE", fun_unique, 1, 4, FN_REG},
  {"UNSETQ", fun_unsetq, 0, 1, FN_REG},
  {"UPTIME", fun_uptime, 0, 1, FN_STRIPANSI},
  {"UTCTIME", fun_time, 0, 0, FN_REG},
  {"V", fun_v, 1, 1, FN_REG | FN_STRIPANSI},
  {"VALID", fun_valid, 2, 3, FN_REG},
  {"VERSION", fun_version, 0, 0, FN_REG},
  {"VISIBLE", fun_visible, 2, 2, FN_REG | FN_STRIPANSI},
  {"WHERE", fun_where, 1, 1, FN_REG | FN_STRIPANSI},
  {"WIDTH", fun_width, 1, 2, FN_REG | FN_STRIPANSI},
  {"WILDGREP", fun_grep, 3, 3, FN_REG},
  {"WILDGREPI", fun_grep, 3, 3, FN_REG},
  {"WIPE", fun_wipe, 1, 1, FN_REG},
  {"WORDPOS", fun_wordpos, 2, 3, FN_REG | FN_STRIPANSI},
  {"WORDS", fun_words, 1, 2, FN_REG | FN_STRIPANSI},
  {"WRAP", fun_wrap, 2, 4, FN_REG},
  {"XATTR", fun_lattr, 3, 4, FN_REG | FN_STRIPANSI},
  {"XATTRP", fun_lattr, 3, 4, FN_REG | FN_STRIPANSI},
  {"XCON", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XEXITS", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XMWHO", fun_xwho, 2, 2, FN_REG | FN_STRIPANSI},
  {"XMWHOID", fun_xwho, 2, 2, FN_REG | FN_STRIPANSI},
  {"XPLAYERS", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XGET", fun_xget, 2, 2, FN_REG | FN_STRIPANSI},
  {"XOR", fun_xor, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"XTHINGS", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XVCON", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XVEXITS", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XVPLAYERS", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XVTHINGS", fun_dbwalker, 3, 3, FN_REG | FN_STRIPANSI},
  {"XWHO", fun_xwho, 2, 3, FN_REG | FN_STRIPANSI},
  {"XWHOID", fun_xwho, 2, 3, FN_REG | FN_STRIPANSI},
  {"ZEMIT", fun_zemit, 2, -2, FN_REG},
  {"ZFUN", fun_zfun, 1, (MAX_STACK_ARGS + 1), FN_REG},
  {"ZONE", fun_zone, 1, 2, FN_REG | FN_STRIPANSI},
  {"ZMWHO", fun_zwho, 1, 1, FN_REG | FN_STRIPANSI},
  {"ZWHO", fun_zwho, 1, 2, FN_REG | FN_STRIPANSI},
  {"VADD", fun_vadd, 2, 3, FN_REG | FN_STRIPANSI},
  {"VCROSS", fun_vcross, 2, 3, FN_REG | FN_STRIPANSI},
  {"VSUB", fun_vsub, 2, 3, FN_REG | FN_STRIPANSI},
  {"VMAX", fun_vmax, 2, 3, FN_REG | FN_STRIPANSI},
  {"VMIN", fun_vmin, 2, 3, FN_REG | FN_STRIPANSI},
  {"VMUL", fun_vmul, 2, 3, FN_REG | FN_STRIPANSI},
  {"VDOT", fun_vdot, 2, 3, FN_REG | FN_STRIPANSI},
  {"VMAG", fun_vmag, 1, 2, FN_REG | FN_STRIPANSI},
  {"VDIM", fun_words, 1, 2, FN_REG | FN_STRIPANSI},
  {"VUNIT", fun_vunit, 1, 2, FN_REG | FN_STRIPANSI},
  {"ACOS", fun_acos, 1, 2, FN_REG | FN_STRIPANSI},
  {"ASIN", fun_asin, 1, 2, FN_REG | FN_STRIPANSI},
  {"ATAN", fun_atan, 1, 2, FN_REG | FN_STRIPANSI},
  {"ATAN2", fun_atan2, 2, 3, FN_REG | FN_STRIPANSI},
  {"CEIL", fun_ceil, 1, 1, FN_REG | FN_STRIPANSI},
  {"COS", fun_cos, 1, 2, FN_REG | FN_STRIPANSI},
  {"CTU", fun_ctu, 3, 3, FN_REG | FN_STRIPANSI},
  {"E", fun_e, 0, 1, FN_REG | FN_STRIPANSI},
  {"FDIV", fun_fdiv, 2, INT_MAX, FN_REG | FN_STRIPANSI},
  {"FMOD", fun_fmod, 2, 2, FN_REG | FN_STRIPANSI},
  {"FLOOR", fun_floor, 1, 1, FN_REG | FN_STRIPANSI},
  {"LOG", fun_log, 1, 2, FN_REG | FN_STRIPANSI},
  {"LN", fun_ln, 1, 1, FN_REG | FN_STRIPANSI},
  {"PI", fun_pi, 0, 0, FN_REG},
  {"POWER", fun_power, 2, 2, FN_REG | FN_STRIPANSI},
  {"ROUND", fun_round, 2, 3, FN_REG | FN_STRIPANSI},
  {"SIN", fun_sin, 1, 2, FN_REG | FN_STRIPANSI},
  {"SQRT", fun_sqrt, 1, 1, FN_REG | FN_STRIPANSI},
  {"STDDEV", fun_stddev, 1, INT_MAX, FN_REG | FN_STRIPANSI},
  {"TAN", fun_tan, 1, 2, FN_REG | FN_STRIPANSI},
  {"HTML", fun_html, 1, 1, FN_REG | FN_WIZARD},
  {"TAG", fun_tag, 1, INT_MAX, FN_REG},
  {"ENDTAG", fun_endtag, 1, 1, FN_REG},
  {"TAGWRAP", fun_tagwrap, 2, 3, FN_REG},
#ifdef DEBUG_PENNMUSH
  {"PE_REGS_DUMP", fun_pe_regs_dump, 0, 1, FN_REG},
#endif                          /* DEBUG_PENNMUSH */
#ifndef WITHOUT_WEBSOCKETS
  {"WSJSON", fun_websocket_json, 1, 2, FN_REG},
  {"WSHTML", fun_websocket_html, 1, 2, FN_REG},
#endif /* undef WITHOUT_WEBSOCKETS */
  {NULL, NULL, 0, 0, 0}
};

/** Map of function restriction bits to textual names */
struct function_restrictions {
  const char *name; /**< Name of restriction */
  uint32_t bit;     /**< FN_* flag for restriction */
};

struct function_restrictions func_restrictions[] = {
  {"Nobody", FN_DISABLED}, /* Should always be the first element */
  {"NoGagged", FN_NOGAGGED},     {"NoFixed", FN_NOFIXED},
  {"NoGuest", FN_NOGUEST},       {"Admin", FN_ADMIN},
  {"Wizard", FN_WIZARD},         {"God", FN_GOD},
  {"NoSideFX", FN_NOSIDEFX},     {"LogArgs", FN_LOGARGS},
  {"LogName", FN_LOGNAME},       {"NoParse", FN_NOPARSE},
  {"Localize", FN_LOCALIZE},     {"Userfn", FN_USERFN},
  {"StripAnsi", FN_STRIPANSI},   {"Literal", FN_LITERAL},
  {"Deprecated", FN_DEPRECATED}, {NULL, 0}};

static uint32_t
fn_restrict_to_bit(const char *r)
{
  int i;

  if (!r || !*r)
    return 0;

  for (i = 0; func_restrictions[i].name; i += 1) {
    if (strcasecmp(func_restrictions[i].name, r) == 0)
      return func_restrictions[i].bit;
  }
  return 0;
}

static const char *fn_restrict_to_str(uint32_t b) __attribute__((unused));

static const char *
fn_restrict_to_str(uint32_t b)
{
  int i;
  for (i = 0; func_restrictions[i].name; i += 1) {
    if (func_restrictions[i].bit == b)
      return func_restrictions[i].name;
  }
  return NULL;
}

/** List all functions.
 * \verbatim
 * This is the mail interface to @list functions.
 * \endverbatim
 * \param player the enactor.
 * \param lc if 1, return functions in lowercase.
 * \param type "local", "builtin", "all" or NULL, to limit which functions are
 * shown
 */
void
do_list_functions(dbref player, int lc, const char *type)
{
  /* lists all built-in functions. */
  char *b = list_functions(type);
  notify_format(player, T("Functions: %s"), lc ? strlower(b) : b);
}

/** Return a list of function names.
 * This function returns the list of function names as a string.
 * \param type if "local", returns \@functions only.  If "builtin",
 *   hardcoded functions. If omitted, both.
 * \return list of function names as a static string.
 */
char *
list_functions(const char *type)
{
  FUN *fp;
  const char **ptrs;
  static char buff[BUFFER_LEN];
  char *bp;
  int nptrs = 0, i;
  int which = 0;
  /* 0x1 for builtin, 0x2 for @function */

  if (!type)
    which = 0x3;
  else if (strcmp(type, "all") == 0)
    which = 0x3;
  else if (strcmp(type, "builtin") == 0)
    which = 0x1;
  else if (strcmp(type, "local") == 0)
    which = 0x2;
  else {
    mush_strncpy(buff, T("#-1 INVALID ARGUMENT"), BUFFER_LEN);
    return buff;
  }

  ptrs = mush_calloc(sizeof(char *),
                     htab_function.entries + htab_user_function.entries,
                     "function.list");

  if (which & 0x1) {
    for (fp = hash_firstentry(&htab_function); fp;
         fp = hash_nextentry(&htab_function)) {
      if (fp->flags & FN_OVERRIDE)
        continue;
      ptrs[nptrs++] = fp->name;
    }
  }

  if (which & 0x2) {
    for (fp = hash_firstentry(&htab_user_function); fp;
         fp = hash_nextentry(&htab_user_function))
      ptrs[nptrs++] = fp->name;
  }

  /* do_gensort needs a dbref now, but only for sort types that aren't
   * used here anyway */
  do_gensort(0, (char **) ptrs, NULL, nptrs, ALPHANUM_LIST);
  bp = buff;
  if (nptrs > 0) {
    safe_str(ptrs[0], buff, &bp);
    for (i = 1; i < nptrs; i++) {
      if (strcmp(ptrs[i], ptrs[i - 1])) {
        safe_chr(' ', buff, &bp);
        safe_str(ptrs[i], buff, &bp);
      }
    }
  }
  *bp = '\0';
  mush_free(ptrs, "function.list");
  return buff;
}

/*---------------------------------------------------------------------------
 * Hashed function table stuff
 */

/** Look up a function by name.
 * \param name name of function to look up.
 * \return pointer to function data, or NULL.
 */
FUN *
func_hash_lookup(const char *name)
{
  FUN *f;
  f = builtin_func_hash_lookup(name);
  if (!f)
    f = user_func_hash_lookup(name);
  else if (f->flags & FN_OVERRIDE)
    f = user_func_hash_lookup(name);
  return f;
}

static FUN *
any_func_hash_lookup(const char *name)
{

  FUN *f;
  f = builtin_func_hash_lookup(name);
  if (!f)
    f = user_func_hash_lookup(name);
  return f;
}

static FUN *
user_func_hash_lookup(const char *name)
{
  return (FUN *) hashfind(strupper(name), &htab_user_function);
}

/** Look up a function by name, builtins only.
 * \param name name of function to look up.
 * \return pointer to function data, or NULL.
 */
FUN *
builtin_func_hash_lookup(const char *name)
{
  FUN *f;
  f = (FUN *) hashfind(strupper(name), &htab_function);
  return f;
}

static void
func_hash_insert(const char *name, FUN *func)
{
  hashadd(name, (void *) func, &htab_function);
}

static void delete_function(void *);

/** Initialize the function hash table.
 */
void
init_func_hashtab(void)
{
  FUNTAB *ftp;
  FUNALIAS *fa;

  hashinit(&htab_function, 512);
  hash_init(&htab_user_function, 32, delete_function);
  function_slab = slab_create("functions", sizeof(FUN));
  for (ftp = flist; ftp->name; ftp++) {
    function_add(ftp->name, ftp->fun, ftp->minargs, ftp->maxargs, ftp->flags);
  }
  for (fa = faliases; fa->name; fa++) {
    alias_function(NOTHING, fa->name, fa->alias);
  }
  local_functions();
}

/** Function initization to perform after reading the config file.
 * This function performs post-config initialization.
 */
void
function_init_postconfig(void)
{
}

/** Check permissions to run a function.
 * \param player the executor.
 * \param fp pointer to function data.
 * \retval 1 executor may use the function.
 * \retval 0 permission denied.
 */
int
check_func(dbref player, FUN *fp)
{
  if (!fp)
    return 0;
  if ((fp->flags & (~FN_ARG_MASK)) == 0)
    return 1;
  if (fp->flags & FN_DISABLED)
    return 0;
  if ((fp->flags & FN_GOD) && !God(player))
    return 0;
  if ((fp->flags & FN_WIZARD) && !Wizard(player))
    return 0;
  if ((fp->flags & FN_ADMIN) && !Hasprivs(player))
    return 0;
  if ((fp->flags & FN_NOGAGGED) && Gagged(player))
    return 0;
  if ((fp->flags & FN_NOFIXED) && Fixed(player))
    return 0;
  if ((fp->flags & FN_NOGUEST) && Guest(player))
    return 0;
  return 1;
}

/** \@function/clone, for creating a copy of a function.
 * \param player the enactor
 * \param function name of function to clone
 * \param clone name of the cloned function
 */
void
do_function_clone(dbref player, const char *function, const char *clone)
{
  FUN *fp, *fpc;
  char realclone[BUFFER_LEN];
  strcpy(realclone, strupper(clone));

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (any_func_hash_lookup(realclone)) {
    notify(player, T("There's already a function with that name."));
    return;
  }

  if (!ok_function_name(realclone)) {
    notify(player, T("Invalid function name."));
    return;
  }

  fp = builtin_func_hash_lookup(function);
  if (!fp) {
    notify(player, T("That's not a builtin function."));
    return;
  }

  fpc = function_add(mush_strdup(realclone, "function.name"), fp->where.fun,
                     fp->minargs, fp->maxargs, (fp->flags | FN_CLONE));
  fpc->clone_template = (fp->clone_template ? fp->clone_template : fp);

  notify(player, T("Function cloned."));
}

/** Add an alias to a function.
 * This function adds an alias to a function in the hash table.
 * \param player dbref of player to notify with errors, or NOTHING to skip
 * \param function name of function to alias.
 * \param alias name of the alias to add.
 * \retval 0 failure (alias exists, or function doesn't, or is a user fun).
 * \retval 1 success.
 */
int
alias_function(dbref player, const char *function, const char *alias)
{
  FUN *fp;
  char realalias[BUFFER_LEN];
  strcpy(realalias, strupper(alias));

  /* Make sure the alias doesn't exist already */
  if (any_func_hash_lookup(realalias)) {
    if (player != NOTHING)
      notify(player, T("There's already a function with that name."));
    return 0;
  }

  if (!ok_function_name(realalias)) {
    if (player != NOTHING)
      notify(player, T("Invalid function name."));
    return 0;
  }

  /* Look up the original */
  fp = func_hash_lookup(function);
  if (!fp) {
    if (player != NOTHING)
      notify(player, T("No such function."));
    return 0;
  }

  /* We can't alias @functions. Just use another @function for these */
  if (!(fp->flags & FN_BUILTIN)) {
    if (player != NOTHING)
      notify(player, T("You cannot alias @functions."));
    return 0;
  }
  if (fp->flags & FN_CLONE) {
    if (player != NOTHING)
      notify(player, T("You cannot alias cloned functions."));
    return 0;
  }

  func_hash_insert(realalias, fp);

  if (player != NOTHING)
    notify(player, T("Alias added."));

  return 1;
}

/** Add a function.
 * \param name name of the function to add.
 * \param fun pointer to compiled function code.
 * \param minargs minimum arguments to function.
 * \param maxargs maximum arguments to function.
 * \param ftype function evaluation flags.
 */
FUN *
function_add(const char *name, function_func fun, int minargs, int maxargs,
             int ftype)
{
  FUN *fp;

  if (!name || name[0] == '\0')
    return NULL;
  fp = slab_malloc(function_slab, NULL);
  memset(fp, 0, sizeof(FUN));
  fp->name = name;
  fp->clone_template = NULL;
  fp->where.fun = fun;
  fp->minargs = minargs;
  fp->maxargs = maxargs;
  fp->flags = FN_BUILTIN | ftype;
  func_hash_insert(name, fp);
  return fp;
}

/*-------------------------------------------------------------------------
 * Function handlers and the other good stuff. Almost all this code is
 * a modification of TinyMUSH 2.0 code.
 */

/** Strip a level of braces.
 * this is a hack which just strips a level of braces. It malloc()s memory
 * which must be free()d later.
 * \param str string to strip braces from.
 * \return newly allocated string with the first level of braces stripped.
 */
char *
strip_braces(const char *str)
{
  char *buff;
  char *bufc;

  buff = mush_malloc(BUFFER_LEN, "strip_braces.buff");
  bufc = buff;

  while (isspace(*str)) /* eat spaces at the beginning */
    str++;

  switch (*str) {
  case '{':
    str++;
    process_expression(buff, &bufc, &str, 0, 0, 0, PE_NOTHING, PT_BRACE, NULL);
    *bufc = '\0';
    return buff;
    break; /* NOT REACHED */
  default:
    strcpy(buff, str);
    return buff;
  }
}

/*------------------------------------------------------------------------
 * User-defined global function handlers
 */

static int
apply_restrictions(uint32_t result, const char *xres)
{
  char *restriction, *rsave, *res;

  if (!xres || !*xres)
    return result;

  rsave = restriction = res = mush_strdup(xres, "ar.string");

  while ((restriction = split_token(&res, ' '))) {
    uint32_t flag = 0;
    bool clear = 0;

    if (*restriction == '!') {
      restriction++;
      clear = 1;
    }

    flag = fn_restrict_to_bit(restriction);
    if (clear)
      result &= ~flag;
    else
      result |= flag;
  }
  mush_free(rsave, "ar.string");
  return result;
}

/** Given a function name and a restriction, apply the restriction to the
 * function in addition to whatever its usual restrictions are.
 * This is used by the configuration file startup in conf.c
 * \verbatim
 * Valid restrictions are:
 *   nobody     disable the command
 *   nogagged   can't be used by gagged players
 *   nofixed    can't be used by fixed players
 *   noguest    can't be used by guests
 *   admin      can only be used by royalty or wizards
 *   wizard     can only be used by wizards
 *   god        can only be used by god
 *   noplayer   can't be used by players, just objects/rooms/exits
 *   nosidefx   can't be used to do side-effect thingies
 *   localize   localize q-registers
 *   userfn     can only be used inside @functions.
 * \endverbatim
 * \param name name of function to restrict.
 * \param restriction name of restriction to apply to function.
 * \retval 1 success.
 * \retval 0 failure (invalid function or restriction name).
 */
int
restrict_function(const char *name, const char *restriction)
{
  FUN *fp;

  if (!name || !*name)
    return 0;
  fp = func_hash_lookup(name);
  if (!fp)
    return 0;
  fp->flags = apply_restrictions(fp->flags, restriction);
  return 1;
}

/** Softcode interface to restrict a function.
 * \verbatim
 * This is the implementation of @function/restrict.
 * \endverbatim
 * \param player the enactor.
 * \param name name of function to restrict.
 * \param restriction name of restriction to add.
 * \param builtin operate on the builtin version, whether overridden or not
 */
void
do_function_restrict(dbref player, const char *name, const char *restriction,
                     int builtin)
{
  FUN *fp;
  uint32_t flags;
  char tbuf1[BUFFER_LEN];
  char *bp = tbuf1;

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (!name || !*name) {
    notify(player, T("Restrict what function?"));
    return;
  }
  if (!restriction) {
    notify(player, T("Do what with the function?"));
    return;
  }
  fp = builtin ? builtin_func_hash_lookup(name) : func_hash_lookup(name);
  if (!fp) {
    notify(player, T("No such function."));
    return;
  }
  flags = fp->flags;
  fp->flags = apply_restrictions(flags, restriction);
  if (fp->flags & FN_BUILTIN)
    safe_format(tbuf1, &bp, "%s %s - ", T("Builtin function"), fp->name);
  else
    safe_format(tbuf1, &bp, "%s #%d/%s - ", "@function", fp->where.ufun->thing,
                fp->where.ufun->name);
  if (fp->flags == flags)
    safe_str(T("Restrictions unchanged."), tbuf1, &bp);
  else
    safe_str(T("Restrictions modified."), tbuf1, &bp);
  *bp = '\0';
  notify(player, tbuf1);
}

/* Sort FUN*s by dbref and then function name */
static int
func_comp(const void *s1, const void *s2)
{
  const FUN *a, *b;
  dbref d1, d2;

  a = *(const FUN **) s1;
  b = *(const FUN **) s2;

  d1 = a->where.ufun->thing;
  d2 = b->where.ufun->thing;

  if (d1 == d2)
    return strcmp(a->name, b->name);
  else if (d1 < d2)
    return -1;
  else
    return 1;
}

/* Add a user-defined function from cnf file */
int
cnf_add_function(char *name, char *opts)
{
  FUN *fp;
  dbref thing;
  int minargs[2] = {0, 0};
  int maxargs[2] = {0, 0};
  char *attrname, *one, *list;

  name = trim_space_sep(name, ' ');
  upcasestr(name);

  if (!ok_function_name(name))
    return 0;

  /* Validate arguments */
  list = trim_space_sep(opts, ' ');
  if (!list)
    return 0;
  one = split_token(&list, ' ');
  if ((attrname = strchr(one, '/')) == NULL)
    return 0;
  *attrname++ = '\0';
  upcasestr(attrname);
  /* Account for #dbref/foo */
  if (*one == '#')
    one++;
  /* Don't care if the attr exists, only if it /could/ exist */
  if (!is_strict_integer(one) || !good_atr_name(attrname))
    return 0;
  thing = (dbref) parse_integer(one);
  if (!GoodObject(thing) || IsGarbage(thing))
    return 0;
  if (list) {
    /* min/max args */
    one = split_token(&list, ' ');
    if (!is_strict_integer(one))
      return 0;
    minargs[0] = parse_integer(one);
    minargs[1] = 1;
    if (minargs[0] < 0 || minargs[0] > MAX_STACK_ARGS)
      minargs[0] = 0;
    if (list) {
      /* max args */
      one = split_token(&list, ' ');
      if (!is_strict_integer(one))
        return 0;
      maxargs[0] = parse_integer(one);
      maxargs[1] = 1;
      if (maxargs[0] < -MAX_STACK_ARGS)
        maxargs[0] = -MAX_STACK_ARGS;
      else if (maxargs[0] > MAX_STACK_ARGS)
        maxargs[0] = MAX_STACK_ARGS;
    }
  }

  fp = func_hash_lookup(name);
  if (fp) {
    if (fp->flags & FN_BUILTIN) {
      /* Override built-in function */
      fp->flags |= FN_OVERRIDE;
      fp = NULL;
    } else {
      if (fp->where.ufun->name) {
        mush_free(fp->where.ufun->name, "userfn.name");
      }
    }
  }

  if (!fp) {
    /* Create new userfunction */
    fp = slab_malloc(function_slab, NULL);
    fp->name = mush_strdup(name, "func_hash.name");
    fp->where.ufun = mush_malloc(sizeof(USERFN_ENTRY), "userfn");
    fp->minargs = 0;
    fp->maxargs = MAX_STACK_ARGS;
    hashadd(name, fp, &htab_user_function);
  }

  fp->where.ufun->thing = thing;
  fp->where.ufun->name = mush_strdup(upcasestr(attrname), "userfn.name");
  if (minargs[1])
    fp->minargs = minargs[0];
  if (maxargs[1])
    fp->maxargs = maxargs[0];

  return 1;
}

/** Add a user-defined function.
 * \verbatim
 * This is the implementation of the @function command. If no arguments
 * are given, it lists all @functions defined. Otherwise, this adds
 * an @function.
 * \endverbatim
 * \param player the enactor.
 * \param name name of function to add.
 * \param argv array of arguments.
 * \param preserve Treat the function like it was called with ulocal() instead
 *  of u().
 */
void
do_function(dbref player, char *name, char *argv[], int preserve)
{
  char tbuf1[BUFFER_LEN];
  char *bp = tbuf1;
  dbref thing;
  FUN *fp;
  size_t userfn_count = htab_user_function.entries;

  /* if no arguments, just give the list of user functions, by walking
   * the function hash table, and looking up all functions marked
   * as user-defined.
   */

  if (!name || !*name) {
    if (userfn_count == 0) {
      notify(player, T("No global user-defined functions exist."));
      return;
    }
    if (Global_Funcs(player)) {
      /* if the player is privileged, display user-def'ed functions
       * with corresponding dbref number of thing and attribute name.
       */
      FUN **funclist;
      int n = 0;

      funclist = mush_calloc(userfn_count, sizeof(FUN *), "function.fp.list");
      notify(player, T("Function Name                   Dbref #    Attrib"));
      for (fp = (FUN *) hash_firstentry(&htab_user_function); fp;
           fp = (FUN *) hash_nextentry(&htab_user_function)) {
        funclist[n] = fp;
        n++;
      }
      qsort(funclist, userfn_count, sizeof(FUN *), func_comp);
      for (n = 0; n < (int) userfn_count; n++) {
        fp = funclist[n];
        notify_format(player, "%-32s %6d    %s", fp->name,
                      fp->where.ufun->thing, fp->where.ufun->name);
      }
      mush_free(funclist, "function.fp.list");
    } else {
      const char **funcnames;
      int n = 0;
      /* just print out the list of available functions */
      safe_str(T("User functions:"), tbuf1, &bp);
      funcnames = mush_calloc(userfn_count, sizeof(char *), "function.list");
      for (fp = (FUN *) hash_firstentry(&htab_user_function); fp;
           fp = (FUN *) hash_nextentry(&htab_user_function)) {
        funcnames[n] = fp->name;
        n++;
      }
      qsort(funcnames, userfn_count, sizeof(char *), str_comp);
      for (n = 0; n < (int) userfn_count; n++) {
        safe_chr(' ', tbuf1, &bp);
        safe_str(funcnames[n], tbuf1, &bp);
      }
      mush_free(funcnames, "function.list");
      *bp = '\0';
      notify(player, tbuf1);
    }
    return;
  }
  /* otherwise, we are adding a user function.
   * Only those with the Global_Funcs power may add stuff.
   * If you add a function that is already a user-defined function,
   * the old function gets over-written.
   */

  if (!Global_Funcs(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (!argv[1] || !*argv[1] || !argv[2] || !*argv[2]) {
    notify(player, T("You must specify an object and an attribute."));
    return;
  }
  /* make sure the function name length is okay */
  upcasestr(name);
  if (!ok_function_name(name)) {
    notify(player, T("Invalid function name."));
    return;
  }
  /* find the object. For some measure of security, the player must
   * be able to examine it.
   */
  if ((thing = noisy_match_result(player, argv[1], NOTYPE, MAT_EVERYTHING)) ==
      NOTHING)
    return;
  if (SAFER_UFUN) {
    if (!controls(player, thing)) {
      notify(player, T("No permission to control object."));
      return;
    }
  } else if (!Can_Examine(player, thing)) {
    notify(player, T("No permission to examine object."));
    return;
  }
  /* we don't need to check if the attribute exists. If it doesn't,
   * it's not our problem - it's the user's responsibility to make
   * sure that the attribute exists (if it doesn't, invoking the
   * function will return a #-1 NO SUCH ATTRIBUTE error).
   * We do, however, need to make sure that the user isn't trying
   * to replace a built-in function.
   */

  fp = func_hash_lookup(name);
  if (!fp) {
    if (argv[6] && *argv[6]) {
      notify(player, T("Expected between 1 and 5 arguments."));
      return;
    }
    /* a completely new entry. First, insert it into general hash table */
    fp = slab_malloc(function_slab, NULL);
    fp->name = mush_strdup(name, "func_hash.name");
    if (argv[3] && *argv[3]) {
      fp->minargs = parse_integer(argv[3]);
      if (fp->minargs < 0)
        fp->minargs = 0;
      else if (fp->minargs > MAX_STACK_ARGS)
        fp->minargs = MAX_STACK_ARGS;
    } else
      fp->minargs = 0;

    if (argv[4] && *argv[4]) {
      fp->maxargs = parse_integer(argv[4]);
      if (fp->maxargs < 0)
        fp->maxargs *= -1;
      if (fp->maxargs > MAX_STACK_ARGS)
        fp->maxargs = MAX_STACK_ARGS;
    } else
      fp->maxargs = DEF_FUNCTION_ARGS;
    if (argv[5] && *argv[5])
      fp->flags = apply_restrictions(0, argv[5]);
    else
      fp->flags = 0;
    if (preserve)
      fp->flags |= FN_LOCALIZE;
    hashadd(name, fp, &htab_user_function);

    /* now add it to the user function table */
    fp->where.ufun = mush_malloc(sizeof(USERFN_ENTRY), "userfn");
    fp->where.ufun->thing = thing;
    fp->where.ufun->name = mush_strdup(upcasestr(argv[2]), "userfn.name");

    notify(player, T("Function added."));
    return;
  } else {
    /* we are modifying an old entry */
    if ((fp->flags & FN_BUILTIN)) {
      notify(player, T("You cannot change that built-in function."));
      return;
    }
    fp->where.ufun->thing = thing;
    if (fp->where.ufun->name)
      mush_free(fp->where.ufun->name, "userfn.name");
    fp->where.ufun->name = mush_strdup(upcasestr(argv[2]), "userfn.name");
    if (argv[3] && *argv[3]) {
      fp->minargs = parse_integer(argv[3]);
      if (fp->minargs < 0)
        fp->minargs = 0;
      else if (fp->minargs > MAX_STACK_ARGS)
        fp->minargs = MAX_STACK_ARGS;
    } else
      fp->minargs = 0;

    if (argv[4] && *argv[4]) {
      fp->maxargs = parse_integer(argv[4]);
      if (fp->maxargs < 0)
        fp->maxargs *= -1;
      if (fp->maxargs > MAX_STACK_ARGS)
        fp->maxargs = MAX_STACK_ARGS;
    } else
      fp->maxargs = DEF_FUNCTION_ARGS;

    /* Set new flags */
    if (argv[5] && *argv[5])
      fp->flags = apply_restrictions(0, argv[5]);
    else
      fp->flags = 0;
    if (preserve)
      fp->flags |= FN_LOCALIZE;

    notify(player, T("Function updated."));
  }
}

/** Free a @function pointer when it's removed from the hash table */
static void
delete_function(void *data)
{
  FUN *fp = data;

  mush_free((void *) fp->name, "func_hash.name");
  mush_free(fp->where.ufun->name, "userfn.name");
  mush_free(fp->where.ufun, "userfn");
  slab_free(function_slab, fp);
}

/** Restore an overridden built-in function.
 * \verbatim
 * If a built-in function is deleted with @function/delete, it can be
 * restored with @function/restore. This implements @function/restore.
 * If a user-defined function has been added, it will be removed by
 * this function.
 * \endverbatim
 * \param player the enactor.
 * \param name name of function to restore.
 */
void
do_function_restore(dbref player, const char *name)
{
  FUN *fp;

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (!name || !*name) {
    notify(player, T("Restore what?"));
    return;
  }

  fp = builtin_func_hash_lookup(name);

  if (!fp) {
    notify(player, T("That's not a builtin function."));
    return;
  }

  if (!(fp->flags & FN_OVERRIDE)) {
    notify(player, T("That function isn't deleted!"));
    return;
  }

  fp->flags &= ~FN_OVERRIDE;
  notify(player, T("Restored."));

  /* Delete any @function with the same name */
  hashdelete(strupper(name), &htab_user_function);
}

/** Delete a function.
 * \verbatim
 * This code implements @function/delete, which deletes a function -
 * either a built-in or a user-defined one.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the function to delete.
 */
void
do_function_delete(dbref player, char *name)
{
  /* Deletes a user-defined function.
   * For security, you must control the object the function uses
   * to delete the function.
   */
  FUN *fp;

  if (!Global_Funcs(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  fp = func_hash_lookup(name);
  if (!fp) {
    notify(player, T("No such function."));
    return;
  }
  if (fp->flags & FN_BUILTIN) {
    if (strcasecmp(name, fp->name)) {
      /* Function alias */
      hashdelete(strupper(name), &htab_function);
      notify(player, T("Function alias deleted."));
      return;
    } else if (fp->flags & FN_CLONE) {
      char safename[BUFFER_LEN];
      strcpy(safename, fp->name);
      mush_free((char *) fp->name, "function.name");
      slab_free(function_slab, fp);
      hashdelete(safename, &htab_function);
      notify(player, T("Function clone deleted."));
      return;
    }
    if (!Wizard(player)) {
      notify(player, T("You can't delete that @function."));
      return;
    }
    fp->flags |= FN_OVERRIDE;
    notify(player, T("Function deleted."));
    return;
  }

  if (!controls(player, fp->where.ufun->thing)) {
    notify(player, T("You can't delete that @function."));
    return;
  }
  /* Remove it from the hash table */
  hashdelete(fp->name, &htab_user_function);
  notify(player, T("Function deleted."));
}

/** Enable or disable a function.
 * \verbatim
 * This implements @function/disable and @function/enable.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the function to enable or disable.
 * \param toggle if 1, enable; if 0, disable.
 */
void
do_function_toggle(dbref player, char *name, int toggle)
{
  FUN *fp;

  if (!Wizard(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  fp = func_hash_lookup(name);
  if (!fp) {
    notify(player, T("No such function."));
    return;
  }

  if (strcasecmp(fp->name, strupper(name))) {
    notify(player, T("You can't disable aliases."));
    return;
  }

  if (toggle) {
    fp->flags &= ~FN_DISABLED;
    notify(player, T("Enabled."));
  } else {
    fp->flags |= FN_DISABLED;
    notify(player, T("Disabled."));
  }
}

/** Get information about a function.
 * \verbatim
 * This implements the @function <function> command, which reports function
 * details to the enactor.
 * \endverbatim
 * \param player the enactor.
 * \param name name of the function.
 */
void
do_function_report(dbref player, char *name)
{
  FUN *fp, *bfp;

  fp = func_hash_lookup(name);
  if (!fp) {
    notify(player, T("No such function."));
    return;
  }
  notify(player, build_function_report(player, fp));

  bfp = builtin_func_hash_lookup(name);
  if (bfp && (fp != bfp))
    notify(player, build_function_report(player, bfp));
}

static char *
build_function_report(dbref player, FUN *fp)
{
  char tbuf[BUFFER_LEN];
  char *tp = tbuf;
  static char buff[BUFFER_LEN];
  char *bp = buff;
  const char *state, *state2;
  bool first;
  int maxargs;
  int i;

  if (fp->flags & FN_BUILTIN)
    state2 = " builtin";
  else
    state2 = " @function";

  if (fp->flags & FN_DISABLED)
    state = "Disabled";
  else if (fp->flags & FN_OVERRIDE)
    state = "Overridden";
  else
    state = "Enabled";

  safe_format(buff, &bp, T("Name      : %s() (%s%s)"), fp->name, state, state2);
  safe_chr('\n', buff, &bp);

  for (first = 1, i = 1; func_restrictions[i].name; i += 1) {
    if (fp->flags & func_restrictions[i].bit) {
      if (!first)
        safe_strl(", ", 2, tbuf, &tp);
      else
        first = 0;
      safe_str(func_restrictions[i].name, tbuf, &tp);
    }
  }

  *tp = '\0';
  safe_format(buff, &bp, T("Flags     : %s"), tbuf);
  safe_chr('\n', buff, &bp);

  if (!(fp->flags & FN_BUILTIN) && Global_Funcs(player))
    safe_format(buff, &bp, T("Location  : #%d/%s\n"), fp->where.ufun->thing,
                fp->where.ufun->name);

  maxargs = abs(fp->maxargs);

  tp = tbuf;

  if (fp->maxargs < 0) {
    safe_str(T("(Commas okay in last argument)"), tbuf, &tp);
    *tp = '\0';
  } else
    tbuf[0] = '\0';

  if (fp->minargs == maxargs)
    safe_format(buff, &bp, T("Arguments : %d %s"), fp->minargs, tbuf);
  else if (fp->maxargs == INT_MAX)
    safe_format(buff, &bp, T("Arguments : At least %d %s"), fp->minargs, tbuf);
  else
    safe_format(buff, &bp, T("Arguments : %d to %d %s"), fp->minargs, maxargs,
                tbuf);
  *bp = '\0';
  return buff;
}
