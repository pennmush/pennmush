/**
 * \file funmisc.c
 *
 * \brief Miscellaneous functions for mushcode.
 *
 *
 */

#include "copyrite.h"

#include <time.h>
#include <string.h>
#include <ctype.h>

#include "ansi.h"
#include "attrib.h"
#include "case.h"
#include "command.h"
#include "conf.h"
#include "dbdefs.h"
#include "extchat.h"
#include "externs.h"
#include "flags.h"
#include "function.h"
#include "game.h"
#include "htab.h"
#include "lock.h"
#include "match.h"
#include "memcheck.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "parse.h"
#include "strtree.h"
#include "strutil.h"
#include "gitinfo.h"
#include "tz.h"
#include "version.h"
#include "mushsql.h"
#include "charconv.h"

#ifdef WIN32
#include <windows.h>
#pragma warning(disable : 4761) /* NJG: disable warning re conversion */
#endif

extern FUN flist[];
extern char cf_motd_msg[BUFFER_LEN], cf_wizmotd_msg[BUFFER_LEN],
  cf_downmotd_msg[BUFFER_LEN], cf_fullmotd_msg[BUFFER_LEN];
extern HASHTAB htab_function;

extern const unsigned char *tables;

/* ARGSUSED */
FUNCTION(fun_valid)
{
  /* Checks to see if a given <something> is valid as a parameter of a
   * given type (such as an object name.)
   */
  char tmp[BUFFER_LEN];

  if (!args[0] || !*args[0]) {
    safe_str("#-1", buff, bp);
  } else if (!strcasecmp(args[0], "name")) {
    safe_boolean(ok_name(args[1], 0), buff, bp);
  } else if (!strcasecmp(args[0], "attrname")) {
    safe_boolean(good_atr_name(strupper_r(args[1], tmp, sizeof tmp)), buff, bp);
  } else if (!strcasecmp(args[0], "playername")) {
    dbref target = executor;
    if (nargs >= 3) {
      target = noisy_match_result(executor, args[2], TYPE_PLAYER,
                                  MAT_PMATCH | MAT_TYPE | MAT_ME);
      if (target == NOTHING) {
        safe_str(e_match, buff, bp);
        return;
      }
    }
    safe_boolean(ok_player_name(args[1], target, target), buff, bp);
  } else if (!strcasecmp(args[0], "password")) {
    safe_boolean(ok_password(args[1]), buff, bp);
  } else if (!strcasecmp(args[0], "command")) {
    safe_boolean(ok_command_name(strupper_r(args[1], tmp, sizeof tmp)), buff,
                 bp);
  } else if (!strcasecmp(args[0], "function")) {
    safe_boolean(ok_function_name(strupper_r(args[1], tmp, sizeof tmp)), buff,
                 bp);
  } else if (!strcasecmp(args[0], "flag")) {
    safe_boolean(good_flag_name(strupper_r(args[1], tmp, sizeof tmp)), buff,
                 bp);
  } else if (!strcasecmp(args[0], "qreg")) {
    safe_boolean(ValidQregName(args[1]), buff, bp);
  } else if (!strcasecmp(args[0], "colorname")) {
    safe_boolean(valid_color_name(args[1]), buff, bp);
  } else if (!strcasecmp(args[0], "ansicodes")) {
    ansi_data colors;
    safe_boolean(!define_ansi_data(&colors, args[1]), buff, bp);
  } else if (!strcasecmp(args[0], "channel")) {
    CHAN *target = NULL;
    if (nargs >= 3) {
      find_channel(args[2], &target, executor);
    }
    safe_boolean((ok_channel_name(args[1], target) == NAME_OK), buff, bp);
  } else if (!strcasecmp(args[0], "attrvalue")) {
    safe_boolean(check_attr_value(NOTHING, args[2], args[1]) != NULL, buff, bp);
  } else if (!strcasecmp(args[0], "timezone")) {
    struct tz_result res;
    safe_boolean(parse_timezone_arg(args[1], mudtime, &res), buff, bp);
  } else if (!strcasecmp(args[0], "boolexp") ||
             !strcasecmp(args[0], "lockkey")) {
    boolexp key = parse_boolexp(executor, args[1], "Basic");
    if (key == TRUE_BOOLEXP)
      safe_boolean(0, buff, bp);
    else {
      safe_boolean(1, buff, bp);
      free_boolexp(key);
    }
  } else if (!strcasecmp(args[0], "locktype")) {
    dbref target = executor;
    lock_type lt;
    if (nargs >= 3) {
      target = noisy_match_result(executor, args[2], NOTYPE, MAT_EVERYTHING);
      if (target == NOTHING) {
        safe_str(e_match, buff, bp);
        return;
      } else if (!Can_Examine(executor, target)) {
        safe_str(e_perm, buff, bp);
        return;
      }
    }
    lt = check_lock_type(executor, target, args[1], 1);
    safe_boolean((lt != NULL), buff, bp);
  } else {
    safe_str("#-1", buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_pemit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = PEMIT_LIST | PEMIT_SILENT;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nspemit" : "@pemit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (ns)
    flags |= PEMIT_SPOOF;
  if (is_integer_list(args[0]))
    do_pemit_port(executor, args[0], args[1], flags);
  else
    do_pemit(executor, executor, args[0], args[1], flags, NULL, pe_info);
}

FUNCTION(fun_message)
{
  int i;
  char *argv[10];
  int flags = PEMIT_LIST | PEMIT_SILENT;
  enum emit_type type = EMIT_PEMIT;
  dbref speaker = executor;

  /* Instead of having the '10', '14', etc, hardcoded, this
   * should be using MAX_STACK_ARGS. However, it's potentially
   * slightly incompatible to change it, now. */

  for (i = 0; (i + 3) < nargs && i < 10; i++) {
    argv[i] = args[i + 3];
  }

  if (nargs == 14) {
    char *word, *list;

    list = trim_space_sep(args[13], ' ');

    do {
      word = split_token(&list, ' ');
      if (!word || !*word)
        continue;
      if (strcasecmp("nospoof", word) == 0) {
        if (Can_Nspemit(executor))
          flags |= PEMIT_SPOOF;
      } else if (strcasecmp("spoof", word) == 0) {
        speaker = SPOOF_NOSWITCH(executor, enactor);
      } else if (strcasecmp("remit", word) == 0)
        type = EMIT_REMIT;
      else if (strcasecmp("oemit", word) == 0)
        type = EMIT_OEMIT;
    } while (list);
  }

  do_message(executor, speaker, args[0], args[2], args[1], type, flags, i, argv,
             pe_info);
}

/* ARGSUSED */
FUNCTION(fun_oemit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = ns ? PEMIT_SPOOF : 0;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nsoemit" : "@oemit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_oemit_list(executor, executor, args[0], args[1], flags, NULL, pe_info);
}

/* ARGSUSED */
FUNCTION(fun_emit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = ns ? PEMIT_SPOOF : 0;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nsemit" : "@emit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_emit(executor, executor, args[0], flags, pe_info);
}

/* ARGSUSED */
FUNCTION(fun_remit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = PEMIT_LIST | PEMIT_SILENT;

  if (ns)
    flags |= PEMIT_SPOOF;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nsremit" : "@remit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_remit(executor, executor, args[0], args[1], flags, NULL, pe_info);
}

/* ARGSUSED */
FUNCTION(fun_lemit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = ns ? PEMIT_SPOOF : 0;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nslemit" : "@lemit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_lemit(executor, executor, args[0], flags, pe_info);
}

/* ARGSUSED */
FUNCTION(fun_zemit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = ns ? PEMIT_SPOOF : 0;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nszemit" : "@zemit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  do_zemit(executor, args[0], args[1], flags);
}

/* ARGSUSED */
FUNCTION(fun_prompt)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = PEMIT_LIST | PEMIT_PROMPT;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nspemit" : "@pemit", pe_info) ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if (ns)
    flags |= PEMIT_SPOOF;
  do_pemit(executor, executor, args[0], args[1], flags, NULL, pe_info);
}

/* ARGSUSED */
FUNCTION(fun_setq)
{
  /* sets a variable into a local register */
  int n;
  int invalid = 0;

  if ((nargs % 2) != 0) {
    safe_format(buff, bp,
                T("#-1 FUNCTION (%s) EXPECTS AN EVEN NUMBER OF ARGUMENTS"),
                called_as);
    return;
  }

  for (n = 0; n < nargs; n += 2) {
    if (ValidQregName(args[n])) {
      if (!PE_Setq(pe_info, args[n], args[n + 1])) {
        if (!invalid)
          safe_str(T(e_toomanyregs), buff, bp);
        invalid = 1;
      }
    } else {
      if (!invalid)
        safe_str(T(e_badregname), buff, bp);
      invalid = 1;
    }
  }
  if (!invalid) {
    if (!strcmp(called_as, "SETR") && nargs >= 2) {
      safe_strl(args[1], arglens[1], buff, bp);
    }
  }
}

FUNCTION(fun_letq)
{
  int npairs;
  int n;
  char nbuf[BUFFER_LEN], *nbp;
  char tbuf[BUFFER_LEN], *tbp;
  const char *p;

  PE_REGS *pe_regs;

  if ((nargs % 2) != 1) {
    safe_str(T("#-1 FUNCTION (LETQ) EXPECTS AN ODD NUMBER OF ARGUMENTS"), buff,
             bp);
    return;
  }

  npairs = (nargs - 1) / 2;

  pe_regs = pe_regs_create(PE_REGS_Q | PE_REGS_LET, "fun_letq");

  if (npairs) {
    for (n = 0; n < npairs; n++) {
      int i = n * 2;

      /* The register */
      nbp = nbuf;
      p = args[i];
      if (process_expression(nbuf, &nbp, &p, executor, caller, enactor, eflags,
                             PT_DEFAULT, pe_info))
        goto cleanup;
      *nbp = '\0';

      if (!ValidQregName(nbuf)) {
        safe_str(T(e_badregname), buff, bp);
        goto cleanup;
      }

      tbp = tbuf;
      p = args[i + 1];
      if (process_expression(tbuf, &tbp, &p, executor, caller, enactor, eflags,
                             PT_DEFAULT, pe_info))
        goto cleanup;
      *tbp = '\0';
      pe_regs_set(pe_regs, PE_REGS_Q, nbuf, tbuf);
    }
  }

  /* Localize to our current pe_regs */
  pe_regs->prev = pe_info->regvals;
  pe_info->regvals = pe_regs;

  p = args[nargs - 1];
  process_expression(buff, bp, &p, executor, caller, enactor, eflags,
                     PT_DEFAULT, pe_info);

  pe_info->regvals = pe_regs->prev;
cleanup:
  if (pe_regs) {
    pe_regs_free(pe_regs);
  }
}

/** Helper for listq() */
struct st_qreg_data {
  char *buff; /** Buffer to write matching register names to */
  char **bp;  /** Pointer into buff to write at */
  char *wild; /** Wildcard pattern of qregister names to list */
  char *osep; /** Output separator between register names */
  int count;  /** Number of matched registers so far */
};

static void
listq_walk(const char *cur, int count __attribute__((__unused__)),
           void *userdata)
{
  struct st_qreg_data *st_data = (struct st_qreg_data *) userdata;
  char *name;

  name = (char *) cur + 1;

  if (!st_data->wild || quick_wild(st_data->wild, name)) {
    if (st_data->count++) {
      safe_str(st_data->osep, st_data->buff, st_data->bp);
    }
    safe_str(name, st_data->buff, st_data->bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_listq)
{
  /* Build the Q-reg tree */
  PE_REG_VAL *val;
  PE_REGS *pe_regs;
  StrTree qregs;
  StrTree blanks;
  int types = 0;
  char regname[BUFFER_LEN];
  char *rp;
  struct st_qreg_data st_data;

  st_data.buff = buff;
  st_data.bp = bp;
  st_data.wild = NULL;
  st_data.count = 0;
  st_data.osep = " ";

  /* Quick check: No registers */
  if (pe_info->regvals == NULL) {
    return;
  }

  if (nargs >= 1 && args[0] && *args[0]) {
    st_data.wild = args[0];
  }
  if (nargs >= 2) {
    char *list, *item;
    list = trim_space_sep(args[1], ' ');
    while ((item = split_token(&list, ' '))) {
      if (!*item)
        continue;
      if (strcasecmp("qregisters", item) == 0)
        types |= PE_REGS_Q;
      else if (strcasecmp("regexp", item) == 0)
        types |= PE_REGS_REGEXP;
      else if (strcasecmp("switch", item) == 0)
        types |= PE_REGS_SWITCH;
      else if (strcasecmp("iter", item) == 0)
        types |= PE_REGS_ITER;
      else if (strcasecmp("args", item) == 0 || strcasecmp("stack", item) == 0)
        types |= PE_REGS_ARG;
      else {
        safe_str("#-1", buff, bp);
        return;
      }
    }
  }
  if (!types) {
    if (!strcmp(called_as, "LISTQ")) {
      types = PE_REGS_Q;
    } else {
      types = PE_REGS_TYPE & ~PE_REGS_SYS;
    }
  }

  if (nargs >= 3)
    st_data.osep = args[2];

  st_init(&qregs, "ListQTree");
  st_init(&blanks, "BlankQTree");

  pe_regs = pe_info->regvals;
  while (pe_regs) {
    val = pe_regs->vals;
    while (val) {
      if (!(val->type & types)) {
        val = val->next;
        continue; /* not the right type of register */
      }
      rp = regname;
      switch (val->type & PE_REGS_TYPE) {
      case PE_REGS_Q:
        safe_chr('Q', regname, &rp);
        break;
      case PE_REGS_REGEXP:
        safe_chr('R', regname, &rp);
        break;
      case PE_REGS_SWITCH:
      case PE_REGS_ITER:
        /* Do nothing; they're stored with a leading 'T' */
        break;
      case PE_REGS_ARG:
        safe_chr('A', regname, &rp);
        break;
      default:
        val = val->next;
        continue;
      }

      safe_str(val->name, regname, &rp);
      *rp = '\0';

      if (val->type & PE_REGS_SWITCH) {
        regname[0] = 'S'; /* to differentiate stext and itext */
      }

      /* Insert it into the tree if it's non-blank. */
      if ((val->type & PE_REGS_INT) ||
          ((val->type & PE_REGS_STR) && *(val->val.sval) &&
           !st_find(regname, &blanks))) {
        st_insert(regname, &qregs);
      } else {
        st_insert(regname, &blanks);
      }
      val = val->next;
    }
    if (pe_regs->flags & PE_REGS_QSTOP) {
      /* Remove q-registers */
      types &= ~PE_REGS_Q;
    }
    if (pe_regs->flags & PE_REGS_NEWATTR) {
      /* Remove iter, switch, regexp and %0-%9 context */
      types &= ~(PE_REGS_ITER | PE_REGS_SWITCH | PE_REGS_REGEXP);
      if (!(pe_regs->flags & PE_REGS_ARGPASS))
        types &= ~PE_REGS_ARG;
    } else if (pe_regs->flags & PE_REGS_ARG) {
      /* Only the first pe_regs holding ARGs is checked */
      types &= ~PE_REGS_ARG;
    }
    if (!types)
      break; /* nothing left */
    pe_regs = pe_regs->prev;
  }
  st_walk(&qregs, listq_walk, &st_data);
  st_flush(&qregs);
  st_flush(&blanks);
}

void
clear_allq(NEW_PE_INFO *pe_info)
{
  PE_REGS *pe_regs;
  PE_REG_VAL *pe_val;
  pe_regs = pe_info->regvals;
  while (pe_regs) {
    if (pe_regs->flags & PE_REGS_Q) {
      /* Do this for everything up to the lowest level q-reg that _isn't_ a
       * letq() */
      for (pe_val = pe_regs->vals; pe_val; pe_val = pe_val->next) {
        if (pe_val->type & PE_REGS_Q) {
          if (pe_val->type & PE_REGS_STR) {
            /* Quick and dirty: Set it to "". */
            pe_regs_set(pe_regs, pe_val->type, pe_val->name, "");
          } else {
            pe_val->val.ival = 0;
          }
        }
      }
    }
    if ((pe_regs->flags & (PE_REGS_LET | PE_REGS_LOCALIZED))) {
      /* This was created by localize(), letq(), or something similar,
       * so we can't change anything above it.
       * Instead, just set PE_REGS_QSTOP so we don't look at anything above it.
       * This effectively clears everything above while this pe_regs exists,
       * magically bringing it back when this localized pe_regs goes away */
      pe_regs->flags |= PE_REGS_QSTOP;
      return;
    }
    pe_regs = pe_regs->prev;
  }
}

/** Helper function for unsetq() */
struct st_unsetq_data {
  char *buff;           /**< Unused */
  char **bp;            /**< Unused */
  char *wild;           /**< Wildcard pattern of register names to unset */
  NEW_PE_INFO *pe_info; /**< pe_info to clear registers from */
};

static void
unsetq_walk(const char *cur, int count __attribute__((__unused__)),
            void *userdata)
{
  struct st_unsetq_data *st_data = (struct st_unsetq_data *) userdata;

  /* If it matches the pattern, then set it to "" (blank / unset) */
  if (!st_data->wild || quick_wild(st_data->wild, cur)) {
    PE_Setq(st_data->pe_info, cur, "");
  }
}

/* ARGSUSED */
FUNCTION(fun_unsetq)
{
  PE_REG_VAL *val;
  PE_REGS *pe_regs;
  StrTree qregs;
  StrTree blanks;
  struct st_unsetq_data st_data;
  char *list, *cur;

  st_data.buff = buff;
  st_data.bp = bp;
  st_data.wild = NULL;
  st_data.pe_info = pe_info;

  /* Quick check: No q-regs */
  if (pe_info->regvals == NULL) {
    return;
  }

  /* An unsetq() with no arguments has special behavior: It clears all
   * Q-registers up the stack until it finds the full scope (not marked
   * PE_REGS_LET), then marks that scope as PE_REG_QSTOP, so that
   * attempts to fetch q-registers will not go past it.
   */
  if (nargs == 0 || args[0][0] == '\0') {
    clear_allq(pe_info);
    return;
  }

  /* Special case: One arg, a "*" (will match all q-regs) */
  if (nargs == 1 && args[0][0] == '*' && args[0][1] == '\0') {
    clear_allq(pe_info);
    return;
  }

  /* unsetq with arguments: We build a list of what q-registers we have
   * and compare them with the tree */

  st_init(&qregs, "ListQTree");
  st_init(&blanks, "BlankQTree");

  /* Build the Q-reg tree */
  pe_regs = pe_info->regvals;
  while (pe_regs) {
    val = pe_regs->vals;
    while (val) {
      /* Insert it into the tree if it's non-blank. */
      if ((val->type & PE_REGS_STR) && *(val->val.sval) &&
          !st_find(val->name, &blanks)) {
        st_insert(val->name, &qregs);
      } else {
        st_insert(val->name, &blanks);
      }
      val = val->next;
    }
    /* If it's a QSTOP, we stop. */
    if (pe_regs->flags & PE_REGS_QSTOP) {
      break;
    }
    pe_regs = pe_regs->prev;
  }

  list = args[0];
  while (list) {
    cur = split_token(&list, ' ');
    if (cur && *cur) {
      if (*cur == '*' && *(cur + 1) == '\0') {
        clear_allq(pe_info);
        break;
      } else {
        st_data.wild = cur;
        st_walk(&qregs, unsetq_walk, &st_data);
      }
    }
  }
  st_flush(&qregs);
  st_flush(&blanks);
}

/* ARGSUSED */
FUNCTION(fun_r)
{
  int type = PE_REGS_Q;
  const char *s;

  if (nargs >= 2 && args[1] && *args[1]) {
    if (string_prefix("qregisters", args[1]))
      type = PE_REGS_Q;
    else if (string_prefix("regexp", args[1]))
      type = PE_REGS_REGEXP;
    else if (string_prefix("switch", args[1]))
      type = PE_REGS_SWITCH;
    else if (string_prefix("iter", args[1]))
      type = PE_REGS_ITER;
    else if (string_prefix("args", args[1]) || string_prefix("stack", args[1]))
      type = PE_REGS_ARG;
    else {
      safe_str("#-1", buff, bp);
      return;
    }
  }

  switch (type) {
  case PE_REGS_Q:
    if (ValidQregName(args[0]))
      safe_str(PE_Getq(pe_info, args[0]), buff, bp);
    else
      safe_str(T(e_badregname), buff, bp);
    break;
  case PE_REGS_ARG:
    s = pi_regs_get_env(pe_info, args[0]);
    if (s)
      safe_str(s, buff, bp);
    break;
  case PE_REGS_ITER:
  case PE_REGS_SWITCH: {
    int level = 0, total = 0;

    if (type == PE_REGS_ITER)
      total = PE_Get_Ilev(pe_info);
    else
      total = PE_Get_Slev(pe_info);

    if ((*args[0] == 'l' || *args[0] == 'L') && !args[0][1])
      level = total;
    else if (!is_strict_number(args[0])) {
      safe_str(T(e_badregname), buff, bp);
      return;
    } else {
      level = parse_integer(args[0]);
    }
    if (level < 0 || level > total) {
      safe_str(T(e_argrange), buff, bp);
    } else {
      if (type == PE_REGS_ITER)
        safe_str(PE_Get_Itext(pe_info, level), buff, bp);
      else
        safe_str(PE_Get_Stext(pe_info, level), buff, bp);
    }
    break;
  }
  }
}

/* --------------------------------------------------------------------------
 * Utility functions: RAND, DIE, SECURE, SPACE, BEEP, SWITCH, EDIT,
 *      ESCAPE, SQUISH, ENCRYPT, DECRYPT, LIT
 */

/* ARGSUSED */
FUNCTION(fun_rand)
{
  uint32_t low, high, rand;
  int lowint, highint, offset = 0;

  if (nargs == 0) {
    /* Floating pont number in the range [0,1) */
    safe_number(get_random_d(), buff, bp);
    return;
  }

  /* Otherwise, an integer in a user-supplied range */

  if (!is_strict_integer(args[0])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  if (nargs == 1) {
    low = lowint = 0;
    highint = parse_integer(args[0]);
    if (highint == 0) {
      safe_str(T(e_range), buff, bp);
      return;
    } else if (highint < 0) {
      high = offset = (highint * -1);
      offset -= 1;
    } else {
      high = highint;
    }
    high -= 1;
  } else {
    if (!is_strict_integer(args[1])) {
      safe_str(T(e_ints), buff, bp);
      return;
    }
    lowint = parse_integer(args[0]);
    highint = parse_integer(args[1]);
    if (lowint > highint) {
      /* reverse numbers */
      int temp = lowint;
      lowint = highint;
      highint = temp;
    }
    if (lowint < 0) {
      offset = abs(lowint);
      low = 0;
      high = abs(highint + offset);
    } else {
      low = lowint;
      high = highint;
    }
  }
  rand = get_random_u32(low, high);
  safe_integer((int) rand - offset, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_die)
{
  unsigned int n;
  unsigned int die;
  unsigned int count;
  unsigned int total = 0;
  int show_all = 0, first = 1;

  if (!is_uinteger(args[0]) || !is_uinteger(args[1])) {
    safe_str(T(e_uints), buff, bp);
    return;
  }
  n = parse_uinteger(args[0]);
  die = parse_uinteger(args[1]);
  if (nargs == 3)
    show_all = parse_boolean(args[2]);

  if (n == 0 || n > 700) {
    safe_str(T("#-1 NUMBER OUT OF RANGE"), buff, bp);
    return;
  }
  if (show_all) {
    for (count = 0; count < n; count++) {
      if (first)
        first = 0;
      else
        safe_chr(' ', buff, bp);
      safe_uinteger(get_random_u32(1, die), buff, bp);
    }
  } else {
    for (count = 0; count < n; count++)
      total += get_random_u32(1, die);

    safe_uinteger(total, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_switch)
{
  /* this works a bit like the @switch command: it returns the string
   * appropriate to the match. It picks the first match, like @select
   * does, though.
   * Args to this function are passed unparsed. Args are not evaluated
   * until they are needed.
   */
  int ret;
  int j, per;
  char mstr[BUFFER_LEN], pstr[BUFFER_LEN], *dp;
  char const *sp;
  char *tbuf1 = NULL;
  int first = 1, found = 0, exact = 0;
  PE_REGS *pe_regs = NULL;

  if (strstr(called_as, "ALL"))
    first = 0;

  if (string_prefix(called_as, "CASE"))
    exact = 1;

  dp = mstr;
  sp = args[0];
  if (process_expression(mstr, &dp, &sp, executor, caller, enactor, eflags,
                         PT_DEFAULT, pe_info))
    return;
  *dp = '\0';

  if (exact)
    pe_regs = pe_regs_localize(pe_info, PE_REGS_SWITCH, "fun_switch");
  else
    pe_regs =
      pe_regs_localize(pe_info, PE_REGS_SWITCH | PE_REGS_CAPTURE, "fun_switch");
  pe_regs_set(pe_regs, PE_REGS_NOCOPY | PE_REGS_SWITCH, "t0", mstr);

  /* try matching, return match immediately when found */
  for (j = 1; j < (nargs - 1); j += 2) {
    dp = pstr;
    sp = args[j];
    if (process_expression(pstr, &dp, &sp, executor, caller, enactor, eflags,
                           PT_DEFAULT, pe_info))
      goto exit_sequence;
    *dp = '\0';

    if (exact) {
      ret = (strcmp(pstr, mstr) == 0);
    } else {
      pe_regs_clear_type(pe_regs, PE_REGS_CAPTURE);
      ret = local_wild_match(pstr, mstr, pe_regs);
    }

    if (ret) {
      /* If there's a #$ in a switch's action-part, replace it with
       * the value of the conditional (mstr) before evaluating it.
       */
      if (!exact)
        tbuf1 = replace_string("#$", mstr, args[j + 1]);
      else
        tbuf1 = args[j + 1];

      sp = tbuf1;

      per = process_expression(buff, bp, &sp, executor, caller, enactor, eflags,
                               PT_DEFAULT, pe_info);
      if (!exact)
        mush_free(tbuf1, "replace_string.buff");
      found = 1;
      if (per || first) {
        goto exit_sequence;
      }
    }
  }

  if (!(nargs & 1) && !found) {
    /* Default case */
    if (!exact) {
      tbuf1 = replace_string("#$", mstr, args[nargs - 1]);
      sp = tbuf1;
    } else
      sp = args[nargs - 1];
    process_expression(buff, bp, &sp, executor, caller, enactor, eflags,
                       PT_DEFAULT, pe_info);
    if (!exact)
      mush_free(tbuf1, "replace_string.buff");
  }
exit_sequence:
  if (pe_regs) {
    pe_regs_restore(pe_info, pe_regs);
    pe_regs_free(pe_regs);
  }
}

/* ARGSUSED */
FUNCTION(fun_slev) { safe_integer(PE_Get_Slev(pe_info), buff, bp); }

/* ARGSUSED */
FUNCTION(fun_stext)
{
  int i;
  int maxlev = PE_Get_Slev(pe_info);

  if (!strcasecmp(args[0], "l")) {
    i = maxlev;
  } else if (is_strict_integer(args[0])) {
    i = parse_integer(args[0]);
  } else {
    safe_str(T(e_int), buff, bp);
    return;
  }

  if (i < 0 || i > maxlev) {
    safe_str(T(e_argrange), buff, bp);
    return;
  }
  safe_str(PE_Get_Stext(pe_info, i), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_reswitch)
{
  /* this works a bit like the @switch/regexp command */

  int j, per = 0;
  char mstr[BUFFER_LEN], pstr[BUFFER_LEN], *dp;
  char const *sp;
  char *tbuf1;
  int first = 1, found = 0, flags = re_compile_flags;
  int search, subpatterns;
  pcre2_code *re;
  pcre2_match_data *md;
  PE_REGS *pe_regs;
  ansi_string *mas = NULL;
  const PCRE2_UCHAR *haystack;
  int haystacklen;
  int errcode;
  PCRE2_SIZE erroffset;

  if (strstr(called_as, "ALL")) {
    first = 0;
  }

  if (strcmp(called_as, "RESWITCHI") == 0 ||
      strcmp(called_as, "RESWITCHALLI") == 0) {
    flags |= PCRE2_CASELESS;
  }

  dp = mstr;
  sp = args[0];
  if (process_expression(mstr, &dp, &sp, executor, caller, enactor, eflags,
                         PT_DEFAULT, pe_info)) {
    return;
  }
  *dp = '\0';
  if (has_markup(mstr)) {
    mas = parse_ansi_string(mstr);
    haystack = (const PCRE2_UCHAR *) mas->text;
    haystacklen = mas->len;
  } else {
    haystack = (const PCRE2_UCHAR *) mstr;
    haystacklen = dp - mstr;
  }

  pe_regs =
    pe_regs_localize(pe_info, PE_REGS_REGEXP | PE_REGS_SWITCH, "fun_reswitch");
  pe_regs_set(pe_regs, PE_REGS_SWITCH | PE_REGS_NOCOPY, "t0", mstr);

  /* try matching, return match immediately when found */

  for (j = 1; j < (nargs - 1) && !cpu_time_limit_hit; j += 2) {
    dp = pstr;
    sp = args[j];
    if (process_expression(pstr, &dp, &sp, executor, caller, enactor, eflags,
                           PT_DEFAULT, pe_info)) {
      goto exit_sequence;
    }
    *dp = '\0';

    if ((re = pcre2_compile((const PCRE2_UCHAR *) remove_markup(pstr, NULL),
                            PCRE2_ZERO_TERMINATED, flags, &errcode, &erroffset,
                            re_compile_ctx)) == NULL) {
      /* Matching error. Ignore this one, move on. */
      continue;
    }
    ADD_CHECK("pcre");
    md = pcre2_match_data_create_from_pattern(re, NULL);
    search = 0;
    subpatterns = pcre2_match(re, haystack, haystacklen, search, re_match_flags,
                              md, re_match_ctx);
    if (subpatterns >= 0) {
      /* If there's a #$ in a switch's action-part, replace it with
       * the value of the conditional (mstr) before evaluating it.
       */
      tbuf1 = replace_string("#$", mstr, args[j + 1]);
      sp = tbuf1;

      /* set regexp context here */
      pe_regs_clear(pe_regs);
      if (mas) {
        pe_regs_set_rx_context_ansi(pe_regs, 0, re, md, subpatterns, mas);
      } else {
        pe_regs_set_rx_context(pe_regs, 0, re, md, subpatterns);
      }
      per = process_expression(buff, bp, &sp, executor, caller, enactor,
                               eflags | PE_DOLLAR, PT_DEFAULT, pe_info);
      mush_free(tbuf1, "replace_string.buff");
      found = 1;
    }
    pcre2_code_free(re);
    pcre2_match_data_free(md);
    DEL_CHECK("pcre");
    if ((first && found) || per) {
      goto exit_sequence;
    }
  }
  if (!(nargs & 1) && !found) {
    /* Default case */
    tbuf1 = replace_string("#$", mstr, args[nargs - 1]);
    sp = tbuf1;
    process_expression(buff, bp, &sp, executor, caller, enactor, eflags,
                       PT_DEFAULT, pe_info);
    mush_free(tbuf1, "replace_string.buff");
  }
exit_sequence:
  if (mas) {
    free_ansi_string(mas);
  }
  pe_regs_restore(pe_info, pe_regs);
  pe_regs_free(pe_regs);
}

/* ARGSUSED */
FUNCTION(fun_if)
{
  char tbuf[BUFFER_LEN], *tp;
  char const *sp;
  /* Since this may be used as COND(), NCOND(), CONDALL() or NCONDALL,
   * we check for that. */
  bool findtrue = 1;
  bool findall = 0;
  bool found = 0;
  int i;

  if (called_as[0] == 'N')
    findtrue = 0;

  if (strstr(called_as, "ALL"))
    findall = 1;

  for (i = 0; i < nargs - 1; i += 2) {
    tp = tbuf;
    sp = args[i];
    if (process_expression(tbuf, &tp, &sp, executor, caller, enactor, eflags,
                           PT_DEFAULT, pe_info))
      return;
    *tp = '\0';
    if (parse_boolean(tbuf) == findtrue) {
      sp = args[i + 1];
      if (process_expression(buff, bp, &sp, executor, caller, enactor, eflags,
                             PT_DEFAULT, pe_info))
        return;
      if (!findall)
        return;
      found = 1;
    }
  }
  /* If we've found no true case, run the default if it exists. */
  if (!found && (nargs & 1)) {
    sp = args[nargs - 1];
    process_expression(buff, bp, &sp, executor, caller, enactor, eflags,
                       PT_DEFAULT, pe_info);
  }
}

/* ARGSUSED */
FUNCTION(fun_mudname) { safe_str(MUDNAME, buff, bp); }

/* ARGSUSED */
FUNCTION(fun_mudurl) { safe_str(MUDURL, buff, bp); }

/* ARGSUSED */
FUNCTION(fun_version)
{
  safe_format(buff, bp, "PennMUSH version %s patchlevel %s %s", VERSION,
              PATCHLEVEL, PATCHDATE);
#ifdef GIT_REVISION
  safe_format(buff, bp, " (rev %s)", GIT_REVISION);
#endif
}

/* ARGSUSED */
FUNCTION(fun_numversion) { safe_integer(NUMVERSION, buff, bp); }

/* ARGSUSED */
FUNCTION(fun_starttime)
{
  safe_str(show_time(globals.first_start_time, 0), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_restarttime)
{
  safe_str(show_time(globals.start_time, 0), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_restarts) { safe_integer(globals.reboot_count, buff, bp); }

extern char soundex_val[UCHAR_MAX + 1];

enum sound_hash_type { HASH_SOUNDEX, HASH_PHONE };

char *
sound_hash(const char *str, int len, enum sound_hash_type type)
{
  sqlite3 *sqldb = get_shared_db();
  sqlite3_stmt *hasher;
  char *utf8, *result = NULL;
  int ulen;
  int status;

  switch (type) {
  case HASH_SOUNDEX:
    /* Classic Penn soundex turns a leading ph into f. This makes
       sense but isn't typical. */
    hasher = prepare_statement(sqldb,
                               "VALUES (soundex(CASE WHEN ?1 LIKE 'ph%' THEN "
                               "printf('f%s', substr(?1, 3)) ELSE ?1 END))",
                               "hash.soundex");
    break;
  case HASH_PHONE:
    hasher =
      prepare_statement(sqldb, "VALUES (spellfix1_phonehash(?))", "hash.phone");
    break;
  default:
    return NULL;
  }

  utf8 = latin1_to_utf8(str, len, &ulen, "string");
  sqlite3_bind_text(hasher, 1, utf8, ulen, free_string);
  status = sqlite3_step(hasher);
  if (status == SQLITE_ROW) {
    result =
      mush_strdup((const char *) sqlite3_column_text(hasher, 0), "string");
  }
  sqlite3_reset(hasher);
  return result;
}

/* ARGSUSED */
FUNCTION(fun_soundex)
{
  enum sound_hash_type type = HASH_SOUNDEX;
  char *hashed;

  if (nargs == 2) {
    if (strcasecmp(args[1], "soundex") == 0) {
      type = HASH_SOUNDEX;
    } else if (strcasecmp(args[1], "phone") == 0) {
      type = HASH_PHONE;
    } else {
      safe_str("#-1 UNKNOWN HASH TYPE", buff, bp);
      return;
    }
  }
  hashed = sound_hash(args[0], arglens[0], type);
  if (hashed) {
    safe_str(hashed, buff, bp);
    mush_free(hashed, "string");
  } else {
    safe_str("#-1 HASH ERROR", buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_soundlike)
{
  /* Return 1 if the two arguments have the same soundex.
   * This can be optimized to go character-by-character, but
   * I deem the modularity to be more important. So there.
   */
  enum sound_hash_type type = HASH_SOUNDEX;
  char *hash1, *hash2;

  if (nargs == 3) {
    if (strcasecmp(args[2], "soundex") == 0) {
      type = HASH_SOUNDEX;
    } else if (strcasecmp(args[2], "phone") == 0) {
      type = HASH_PHONE;
    } else {
      safe_str("#-1 UNKNOWN HASH TYPE", buff, bp);
      return;
    }
  }

  hash1 = sound_hash(args[0], arglens[0], type);
  hash2 = sound_hash(args[1], arglens[1], type);
  if (!hash1 || !hash2) {
    safe_str("#-1 HASH ERROR", buff, bp);
  } else {
    safe_boolean(strcmp(hash1, hash2) == 0, buff, bp);
  }
  mush_free(hash1, "string");
  mush_free(hash2, "string");
}

/* ARGSUSED */
FUNCTION(fun_functions)
{
  safe_str(list_functions(nargs == 1 ? args[0] : NULL), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_null) { return; }

/* ARGSUSED */
FUNCTION(fun_list)
{
  int which = 3;
  static const char *const fwhich[3] = {"builtin", "local", "all"};
  if (nargs == 2) {
    if (!strcasecmp(args[1], "local"))
      which = 2;
    else if (!strcasecmp(args[1], "builtin"))
      which = 1;
    else if (strcasecmp(args[1], "all")) {
      safe_str("#-1", buff, bp);
      return;
    }
  }
  if (!args[0] || !*args[0])
    safe_str("#-1", buff, bp);
  else if (strcasecmp("motd", args[0]) == 0)
    safe_str(cf_motd_msg, buff, bp);
  else if (strcasecmp("wizmotd", args[0]) == 0 && Hasprivs(executor))
    safe_str(cf_wizmotd_msg, buff, bp);
  else if (strcasecmp("downmotd", args[0]) == 0 && Hasprivs(executor))
    safe_str(cf_downmotd_msg, buff, bp);
  else if (strcasecmp("fullmotd", args[0]) == 0 && Hasprivs(executor))
    safe_str(cf_fullmotd_msg, buff, bp);
  else if (string_prefix("functions", args[0]))
    safe_str(list_functions(fwhich[which - 1]), buff, bp);
  else if (string_prefix("@functions", args[0]))
    safe_str(list_functions("local"), buff, bp);
  else if (string_prefix("commands", args[0]))
    safe_str(list_commands(which), buff, bp);
  else if (string_prefix("attribs", args[0]))
    safe_str(list_attribs(), buff, bp);
  else if (string_prefix("locks", args[0]))
    list_locks(buff, bp, NULL);
  else if (string_prefix("flags", args[0]))
    safe_str(list_all_flags("FLAG", "", executor, FLAG_LIST_NAMECHAR), buff,
             bp);
  else if (string_prefix("powers", args[0]))
    safe_str(list_all_flags("POWER", "", executor, FLAG_LIST_NAMECHAR), buff,
             bp);
  else
    safe_str("#-1", buff, bp);
  return;
}

/* ARGSUSED */
FUNCTION(fun_scan)
{
  dbref thing = executor;
  char *cmdptr = args[0];
  char *prefstr, *thispref;
  int scan_type = 0;

  if (nargs > 1) {
    cmdptr = args[1];
    if (arglens[0]) {
      thing = match_thing(executor, args[0]);
      if (!GoodObject(thing)) {
        safe_str(T(e_notvis), buff, bp);
        return;
      }
    }
  }

  if (!See_All(executor) && !controls(executor, thing)) {
    notify(executor, T("Permission denied."));
    safe_str("#-1", buff, bp);
    return;
  }

  if (nargs == 3 && arglens[2]) {
    prefstr = trim_space_sep(args[2], ' ');
    while ((thispref = split_token(&prefstr, ' '))) {
      if (strcasecmp("room", thispref) == 0)
        scan_type |= CHECK_HERE | CHECK_NEIGHBORS;
      else if (strcasecmp("me", thispref) == 0)
        scan_type |= CHECK_SELF;
      else if (strcasecmp("inventory", thispref) == 0)
        scan_type |= CHECK_INVENTORY;
      else if (strcasecmp("self", thispref) == 0)
        scan_type |= CHECK_SELF | CHECK_INVENTORY;
      else if (strcasecmp("zone", thispref) == 0)
        scan_type |= CHECK_ZONE;
      else if (strcasecmp("globals", thispref) == 0)
        scan_type |= CHECK_GLOBAL;
      else if (strcasecmp("break", thispref) == 0)
        scan_type |= CHECK_BREAK;
      else if (strcasecmp("all", thispref) == 0) {
        scan_type |= CHECK_ALL;
      } else {
        notify(executor, T("Invalid type."));
        safe_str("#-1", buff, bp);
        return;
      }
    }
  }
  if ((scan_type & ~CHECK_BREAK) == 0)
    scan_type |= CHECK_ALL;
  safe_str(scan_list(executor, thing, cmdptr, scan_type), buff, bp);
}

enum whichof_t { DO_FIRSTOF, DO_ALLOF };
static void
do_whichof(char *args[], int nargs, enum whichof_t flag, char *buff, char **bp,
           dbref executor, dbref caller, dbref enactor, NEW_PE_INFO *pe_info,
           int eflags, int isbool)
{
  int j;
  char tbuf[BUFFER_LEN], *tp;
  char sep[BUFFER_LEN];
  char const *ap;
  int first = 1;
  tbuf[0] = '\0';

  if (eflags <= 0)
    eflags = PE_DEFAULT;

  if (flag == DO_ALLOF) {
    /* The last arg is a delimiter. Parse it in place. */
    char *sp = sep;
    const char *arglast = args[nargs - 1];
    if (process_expression(sep, &sp, &arglast, executor, caller, enactor,
                           eflags, PT_DEFAULT, pe_info))
      return;
    *sp = '\0';
    nargs--;
  } else
    sep[0] = '\0';

  for (j = 0; j < nargs; j++) {
    tp = tbuf;
    ap = args[j];
    if (process_expression(tbuf, &tp, &ap, executor, caller, enactor, eflags,
                           PT_DEFAULT, pe_info))
      return;
    *tp = '\0';
    if ((isbool && parse_boolean(tbuf)) || (!isbool && strlen(tbuf))) {
      if (!first && *sep) {
        safe_str(sep, buff, bp);
      } else
        first = 0;
      safe_str(tbuf, buff, bp);
      if (flag == DO_FIRSTOF)
        return;
    }
  }
  if (flag == DO_FIRSTOF)
    safe_str(tbuf, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_firstof)
{
  do_whichof(args, nargs, DO_FIRSTOF, buff, bp, executor, caller, enactor,
             pe_info, eflags, !!strcasecmp(called_as, "STRFIRSTOF"));
}

/* ARGSUSED */
FUNCTION(fun_allof)
{
  do_whichof(args, nargs, DO_ALLOF, buff, bp, executor, caller, enactor,
             pe_info, eflags, !!strcasecmp(called_as, "STRALLOF"));
}

/* Returns a platform-specific timestamp with platform-dependent resolution. */
static uint64_t
get_tsc()
{
#ifdef WIN32
  LARGE_INTEGER li;
  QueryPerformanceCounter(&li);
  return li.QuadPart;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000ULL * tv.tv_sec + tv.tv_usec;
#endif
}

static uint64_t
tsc_diff_to_microseconds(uint64_t start, uint64_t end)
{
#ifdef WIN32
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  return (end - start) * 1000000.0 / frequency.QuadPart;
#else
  return end - start;
#endif
}

extern int global_fun_invocations; /* From parse.c */

/* ARGSUSED */
FUNCTION(fun_benchmark)
{
  char tbuf[BUFFER_LEN], *tp;
  char const *sp;
  int n;
  unsigned int min = UINT_MAX;
  unsigned int max = 0;
  unsigned int total = 0;
  int i = 0;
  dbref thing = NOTHING;

  if (!is_number(args[1])) {
    safe_str(T(e_uint), buff, bp);
    return;
  }
  n = parse_number(args[1]);
  if (n < 1) {
    safe_str(T(e_uint), buff, bp);
    return;
  }

  if (nargs > 2) {
    /* Evaluate <sendto> argument */
    tp = tbuf;
    sp = args[2];
    if (process_expression(tbuf, &tp, &sp, executor, caller, enactor, eflags,
                           PT_DEFAULT, pe_info))
      return;
    *tp = '\0';
    thing = noisy_match_result(executor, tbuf, NOTYPE, MAT_EVERYTHING);
    if (!GoodObject(thing)) {
      safe_dbref(thing, buff, bp);
      return;
    }
    if (!okay_pemit(executor, thing, 1, 1, pe_info)) {
      safe_str("#-1", buff, bp);
      return;
    }
  }

  while (i < n) {
    uint64_t start;
    unsigned int elapsed;
    tp = tbuf;
    sp = args[0];
    start = get_tsc();
    ++i;
    if (process_expression(tbuf, &tp, &sp, executor, caller, enactor, eflags,
                           PT_DEFAULT, pe_info)) {
      *tp = '\0';
      break;
    }
    *tp = '\0';
    elapsed = tsc_diff_to_microseconds(start, get_tsc());
    if (elapsed < min) {
      min = elapsed;
    }
    if (elapsed > max) {
      max = elapsed;
    }
    total += elapsed;
  }

  if (thing != NOTHING) {
    safe_str(tbuf, buff, bp);
    if (pe_info->fun_invocations >= FUNCTION_LIMIT ||
        (global_fun_invocations >= FUNCTION_LIMIT * 5))
      notify(thing, T("Function invocation limit reached. Benchmark timings "
                      "may not be reliable."));
    notify_format(thing, T("Average: %.2f   Min: %u   Max: %u"),
                  ((double) total) / i, min, max);
  } else {
    safe_format(buff, bp, T("Average: %.2f   Min: %u   Max: %u"),
                ((double) total) / i, min, max);
    if (pe_info->fun_invocations >= FUNCTION_LIMIT ||
        (global_fun_invocations >= FUNCTION_LIMIT * 5))
      safe_str(T(" Note: Function invocation limit reached. Benchmark timings "
                 "may not be reliable."),
               buff, bp);
  }

  return;
}
