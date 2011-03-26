/**
 * \file funufun.c
 *
 * \brief Evaluation and user-function functions for mushcode.
 *
 *
 */

#include "copyrite.h"

#include "config.h"
#include "conf.h"
#include "externs.h"
#include "match.h"
#include "parse.h"
#include "mymalloc.h"
#include "attrib.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "flags.h"
#include "lock.h"
#include "function.h"
#include "confmagic.h"

/* ARGSUSED */
FUNCTION(fun_s)
{
  char const *p;
  p = args[0];
  process_expression(buff, bp, &p, executor, caller, enactor, PE_DEFAULT,
                     PT_DEFAULT, pe_info);
}

/* ARGSUSED */
FUNCTION(fun_fn)
{
  /* First argument is name of a function, remaining are arguments
   * for that function.
   */
  char tbuf[BUFFER_LEN];
  char *tp = tbuf;
  char const *p;
  int i;
  if (!args[0] || !*args[0])
    return;                     /* No function name */
  /* Evaluate first argument */
  p = args[0];
  process_expression(tbuf, &tp, &p, executor, caller,
                     enactor, PE_DEFAULT, PT_DEFAULT, pe_info);
  *tp = '\0';
  /* Make sure a builtin function with the name actually exists */
  if (!builtin_func_hash_lookup(tbuf)) {
    safe_str(T("#-1 FUNCTION ("), buff, bp);
    safe_str(tbuf, buff, bp);
    safe_str(T(") NOT FOUND"), buff, bp);
    return;
  }

  safe_chr('(', tbuf, &tp);
  for (i = 1; i < nargs; i++) {
    if (i > 1)
      safe_chr(',', tbuf, &tp);
    safe_strl(args[i], arglens[i], tbuf, &tp);
  }
  safe_chr(')', tbuf, &tp);
  *tp = '\0';
  p = tbuf;
  process_expression(buff, bp, &p, executor, caller, enactor,
                     PE_DEFAULT | PE_BUILTINONLY, PT_DEFAULT, pe_info);
}

/* ARGSUSED */
FUNCTION(fun_localize)
{
  char const *p;
  PE_REGS *pe_regs;

  pe_regs = pe_regs_localize(pe_info, PE_REGS_Q);

  p = args[0];
  process_expression(buff, bp, &p, executor, caller, enactor, PE_DEFAULT,
                     PT_DEFAULT, pe_info);

  pe_regs_restore(pe_info, pe_regs);
  pe_regs_free(pe_regs);
}

/* ARGSUSED */
FUNCTION(fun_objeval)
{
  char name[BUFFER_LEN];
  char *s;
  char const *p;
  dbref obj;

  /* First, we evaluate our first argument so people can use
   * functions on it.
   */
  s = name;
  p = args[0];
  process_expression(name, &s, &p, executor, caller, enactor, PE_DEFAULT,
                     PT_DEFAULT, pe_info);
  *s = '\0';

  if (FUNCTION_SIDE_EFFECTS) {
    /* The security hole created by function side effects is too great
     * to allow a see_all player to evaluate functions from someone else's
     * standpoint. We require control.
     */
    if (((obj = match_thing(executor, name)) == NOTHING)
        || !controls(executor, obj))
      obj = executor;
  } else {
    /* In order to evaluate from something else's viewpoint, you
     * must control it, or be able to see_all.
     */
    if (((obj = match_thing(executor, name)) == NOTHING)
        || (!controls(executor, obj) && !See_All(executor)))
      obj = executor;
  }

  p = args[1];
  process_expression(buff, bp, &p, obj, executor, enactor, PE_DEFAULT,
                     PT_DEFAULT, pe_info);
}

/** Helper function for ufun and family.
 * \param buff string to store result of evaluation.
 * \param bp pointer into end of buff.
 * \param obj object on which the ufun is stored.
 * \param attrib pointer to attribute on which the ufun is stored.
 * \param nargs number of arguments passed to the ufun.
 * \param args array of arguments passed to the ufun.
 * \param executor executor.
 * \param caller caller (unused).
 * \param enactor enactor.
 * \param pe_info pointer to structure for process_expression data.
 * \param extra_flags extra PE_ flags to pass in (PE_USERFN or 0).
 */
void
do_userfn(char *buff, char **bp, dbref obj, ATTR *attrib, int nargs,
          char **args, dbref executor, dbref caller
          __attribute__ ((__unused__)), dbref enactor, NEW_PE_INFO * pe_info,
          int extra_flags)
{
  int j;
  int made_pe_info = 0;
  char *tptr[10];
  char *tbuf;
  char const *tp;
  int pe_flags = PE_DEFAULT | extra_flags;
  int old_args = 0;

  if (nargs > 10)
    nargs = 10;                 /* maximum ten args */

  /* save our stack */
  if (!pe_info) {
    made_pe_info = 1;
    pe_info = make_pe_info("pe_info-do_userfn");
  } else {
    old_args = pe_info->arg_count;
    save_env("do_userfn_env", tptr, pe_info->env);
    pe_info->arg_count = nargs;
  }

  /* copy the appropriate args into the stack */
  for (j = 0; j < nargs; j++)
    pe_info->env[j] = args[j];
  for (; j < 10; j++)
    pe_info->env[j] = NULL;

  tp = tbuf = safe_atr_value(attrib);
  if (AF_NoDebug(attrib))
    pe_flags |= PE_NODEBUG;     /* no_debug overrides debug */
  else if (AF_Debug(attrib))
    pe_flags |= PE_DEBUG;

  process_expression(buff, bp, &tp, obj, executor, enactor, pe_flags,
                     PT_DEFAULT, pe_info);
  free(tbuf);

  if (made_pe_info) {
    for (j = 0; j < 10; j++) {
      pe_info->env[j] = NULL;
    }
    free_pe_info(pe_info);
  } else {
    /* restore the stack */
    for (j = 0; j < 10; j++) {
      pe_info->env[j] = tptr[j];
    }
    pe_info->arg_count = old_args;
  }
}

/* ARGSUSED */
FUNCTION(fun_ufun)
{
  char rbuff[BUFFER_LEN];
  ufun_attrib ufun;
  int flags = UFUN_OBJECT;

  if (!strcmp(called_as, "ULAMBDA")) {
    flags |= UFUN_LAMBDA;
  }

  if (!fetch_ufun_attrib(args[0], executor, &ufun, flags)) {
    safe_str(T(ufun.errmess), buff, bp);
    return;
  }

  call_ufun(&ufun, args + 1, nargs - 1, rbuff, executor, enactor, pe_info);

  safe_str(rbuff, buff, bp);

  return;
}

/* ARGSUSED */
FUNCTION(fun_pfun)
{

  char rbuff[BUFFER_LEN];
  ATTR *a;
  int pe_flags = PE_UDEFAULT;
  dbref parent;
  ufun_attrib ufun;

  parent = Parent(executor);

  if (!GoodObject(parent))
    return;

  /* This is a stripped down version of fetch_ufun_attrib that gets
     the atr value directly from the parent */

  a = atr_get(parent, upcasestr(args[0]));
  if (!a)
    return;                     /* no attr */

  if (AF_Internal(a) || AF_Private(a))
    return;                     /* attr isn't inheritable */

  /* DEBUG attributes */
  if (AF_NoDebug(a))
    pe_flags |= PE_NODEBUG;     /* no_debug overrides debug */
  else if (AF_Debug(a))
    pe_flags |= PE_DEBUG;

  ufun.thing = executor;
  mush_strncpy(ufun.contents, atr_value(a), BUFFER_LEN);
  mush_strncpy(ufun.attrname, AL_NAME(a), ATTRIBUTE_NAME_LIMIT+1);
  ufun.pe_flags = pe_flags;
  ufun.errmess = (char *) "";
  ufun.ufun_flags = UFUN_NONE;

  call_ufun(&ufun, args + 1, nargs - 1, rbuff, executor, enactor, pe_info);

  safe_str(rbuff, buff, bp);

  return;
}


/* Like fun_ufun, but takes as second argument a default message
 * to use if the attribute isn't there.
 */
/* ARGSUSED */
FUNCTION(fun_udefault)
{
  ufun_attrib ufun;
  char *dp;
  char const *sp;
  char mstr[BUFFER_LEN];
  char rbuff[BUFFER_LEN];
  char **xargs;
  int i;

  /* find our object and attribute */
  dp = mstr;
  sp = args[0];
  process_expression(mstr, &dp, &sp, executor, caller, enactor,
                     PE_DEFAULT, PT_DEFAULT, pe_info);
  *dp = '\0';
  if (!fetch_ufun_attrib
      (mstr, executor, &ufun, UFUN_OBJECT | UFUN_REQUIRE_ATTR)) {
    /* We couldn't get it. Evaluate args[1] and return it */
    sp = args[1];

    process_expression(buff, bp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    return;
  }

  /* Ok, we've got it */
  /* We must now evaluate all the arguments from args[2] on and
   * pass them to the function */
  xargs = NULL;
  if (nargs > 2) {
    xargs = mush_calloc(nargs - 2, sizeof(char *), "udefault.xargs");
    for (i = 0; i < nargs - 2; i++) {
      xargs[i] = mush_malloc(BUFFER_LEN, "udefault");
      dp = xargs[i];
      sp = args[i + 2];
      process_expression(xargs[i], &dp, &sp, executor, caller, enactor,
                         PE_DEFAULT, PT_DEFAULT, pe_info);
      *dp = '\0';
    }
  }
  call_ufun(&ufun, xargs, nargs - 2, rbuff, executor, enactor, pe_info);
  safe_str(rbuff, buff, bp);

  /* Free the xargs */
  if (nargs > 2) {
    for (i = 0; i < nargs - 2; i++)
      mush_free(xargs[i], "udefault");
    mush_free(xargs, "udefault.xargs");
  }
  return;

}


/* ARGSUSED */
FUNCTION(fun_zfun)
{
  ufun_attrib ufun;
  dbref zone;
  char rbuff[BUFFER_LEN];

  zone = Zone(executor);

  if (zone == NOTHING) {
    safe_str(T("#-1 INVALID ZONE"), buff, bp);
    return;
  }

  /* find the user function attribute */
  if (!fetch_ufun_attrib
      (tprintf("#%d/%s", zone, args[0]), executor, &ufun, UFUN_OBJECT)) {
    safe_str(T(ufun.errmess), buff, bp);
    return;
  }

  call_ufun(&ufun, args + 1, nargs - 1, rbuff, executor, enactor, pe_info);

  safe_str(rbuff, buff, bp);
}
