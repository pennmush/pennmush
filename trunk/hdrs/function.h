/**
 * \file function.h
 *
 * \brief Stuff relating to softcode functions and \@function
 */

#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include "copyrite.h"

#define FN_REG 0x0
/* Function arguments aren't parsed */
#define FN_NOPARSE      0x1
#define FN_LITERAL      0x2
#define FN_ARG_MASK     0x3
/* Function is disabled */
#define FN_DISABLED     0x4
/* Function will fail if object is gagged */
#define FN_NOGAGGED  0x8
/* Function will fail if object is a guest */
#define FN_NOGUEST   0x10
/* Function will fail if object is fixed */
#define FN_NOFIXED   0x20
/* Function is wizard-only */
#define FN_WIZARD 0x40
/* Function is royalty or wizard */
#define FN_ADMIN  0x80
/* Function is god-only */
#define FN_GOD    0x100
/* Function is builtin */
#define FN_BUILTIN 0x200
/* Function can be overridden with a @function */
#define FN_OVERRIDE 0x400
/* Side-effect version of function no workie */
#define FN_NOSIDEFX 0x800
/* Log function name */
#define FN_LOGNAME 0x1000
/* Log function name and args */
#define FN_LOGARGS 0x2000
/* Localize function registers */
#define FN_LOCALIZE     0x4000
/* Allowed in @function only */
#define FN_USERFN     0x8000
/* Strip ANSI/markup from function's arguments */
#define FN_STRIPANSI  0x10000
/* Function is obsolete and code that uses it should be re-written */
#define FN_DEPRECATED 0x20000

#ifndef HAVE_FUN_DEFINED
typedef struct fun FUN;
#define HAVE_FUN_DEFINED
#endif

typedef void (*function_func) (FUN *, char *, char **, int, char *[], int[],
                               dbref, dbref, dbref, const char *,
                               NEW_PE_INFO *, int);

typedef struct userfn_entry USERFN_ENTRY;

/** A calling pointer to a function.
 * This union holds either a pointer to a function's code or
 * the offset of the function in the user-defined function table.
 */
union fun_call {
  function_func fun;    /**< Pointer to compiled function code */
  USERFN_ENTRY *ufun;   /**< Pointer to \@function location */
};


/** A function.
 * This structure represents a mushcode function.
 */
struct fun {
  const char *name;     /**< Function name */
  union fun_call where; /**< Where to find the function to call it */
  int minargs;          /**< Minimum arguments required, or 0 */
  /** Maximum arguments allowed.
   * Maximum arguments allowed. If there is no limit, this is INT_MAX.
   * If this is negatve, the final argument to the function can contain
   * commas that won't be parsed, and the maximum number of arguments
   * is the absolute value of this variable.
   */
  int maxargs;
  uint32_t flags;   /**< Bitflags of function */
};


/** A user-defined function
 * This structure represents an entry in the user-defined function table.
 */
struct userfn_entry {
  dbref thing;          /**< Dbref of object where the function is defined */
  char *name;           /**< Name of attribute where the function is defined */
};

void do_userfn(char *buff, char **bp,
               dbref obj, ATTR *attrib,
               int nargs, char **args,
               dbref executor, dbref caller, dbref enactor,
               NEW_PE_INFO *pe_info, int extra_flags);

FUN *func_hash_lookup(const char *name);
FUN *builtin_func_hash_lookup(const char *name);
int check_func(dbref player, FUN *fp);
int restrict_function(const char *name, const char *restriction);
int alias_function(dbref player, const char *function, const char *alias);
void do_function_restrict(dbref player, const char *name,
                          const char *restriction, int builtin);
void do_function_restore(dbref player, const char *name);
void do_list_functions(dbref player, int lc, char *type);
char *list_functions(const char *);
void do_function(dbref player, char *name, char **argv, int preserve);
void do_function_toggle(dbref player, char *name, int toggle);
void do_function_report(dbref player, char *name);
void do_function_delete(dbref player, char *name);
void do_function_clone(dbref player, const char *function, const char *clone);

void function_init_postconfig(void);


#define FUNCTION_PROTO(fun_name) \
  extern void fun_name (FUN *fun, char *buff, char **bp, int nargs, char *args[], \
                   int arglen[], dbref executor, dbref caller, dbref enactor, \
                   char const *called_as, NEW_PE_INFO *pe_info, int eflags)
extern void function_add(const char *name, function_func fun, int minargs,
                         int maxargs, int ftype);

int cnf_add_function(char *name, char *opts);


#endif
