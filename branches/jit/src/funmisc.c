/**
 * \file funmisc.c
 *
 * \brief Miscellaneous functions for mushcode.
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "conf.h"
#include "case.h"
#include "externs.h"
#include "version.h"
#include "htab.h"
#include "flags.h"
#include "lock.h"
#include "match.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "parse.h"
#include "function.h"
#include "command.h"
#include "game.h"
#include "attrib.h"
#include "confmagic.h"
#include "ansi.h"

#ifdef WIN32
#include <windows.h>
#pragma warning( disable : 4761)        /* NJG: disable warning re conversion */
#endif

extern FUN flist[];
static char *soundex(char *str);
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

  if (!args[0] || !*args[0])
    safe_str("#-1", buff, bp);
  else if (!strcasecmp(args[0], "name"))
    safe_boolean(ok_name(args[1]), buff, bp);
  else if (!strcasecmp(args[0], "attrname"))
    safe_boolean(good_atr_name(upcasestr(args[1])), buff, bp);
  else if (!strcasecmp(args[0], "playername"))
    safe_boolean(ok_player_name(args[1], executor, executor), buff, bp);
  else if (!strcasecmp(args[0], "password"))
    safe_boolean(ok_password(args[1]), buff, bp);
  else if (!strcasecmp(args[0], "command"))
    safe_boolean(ok_command_name(upcasestr(args[1])), buff, bp);
  else if (!strcasecmp(args[0], "function"))
    safe_boolean(ok_function_name(upcasestr(args[1])), buff, bp);
  else if (!strcasecmp(args[0], "flag"))
    safe_boolean(good_flag_name(upcasestr(args[1])), buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_pemit)
{
  int ns = (string_prefix(called_as, "NS") && Can_Nspemit(executor));
  int flags = PEMIT_LIST | PEMIT_SILENT;
  dbref saved_orator = orator;

  if (!FUNCTION_SIDE_EFFECTS) {
    safe_str(T(e_disabled), buff, bp);
    return;
  }

  if (!command_check_byname(executor, ns ? "@nspemit" : "@pemit") ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  orator = executor;
  if (ns)
    flags |= PEMIT_SPOOF;
  if (is_strict_integer(args[0]))
    do_pemit_port(executor, args[0], args[1], flags);
  else
    do_pemit_list(executor, args[0], args[1], flags);
  orator = saved_orator;
}

FUNCTION(fun_message)
{
  int i;
  char *argv[10];

  for (i = 0; (i + 3) < nargs; i++) {
    argv[i] = args[i + 3];
  }

  do_message_list(executor, executor, args[0], args[2], args[1], 0, i, argv);
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

  if (!command_check_byname(executor, ns ? "@nsoemit" : "@oemit") ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  orator = executor;
  do_oemit_list(executor, args[0], args[1], flags);
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

  if (!command_check_byname(executor, ns ? "@nsemit" : "@emit") ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  orator = executor;
  do_emit(executor, args[0], flags);
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

  if (!command_check_byname(executor, ns ? "@nsremit" : "@remit") ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  orator = executor;
  do_remit(executor, args[0], args[1], flags);
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

  if (!command_check_byname(executor, ns ? "@nslemit" : "@lemit") ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  orator = executor;
  do_lemit(executor, args[0], flags);
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

  if (!command_check_byname(executor, ns ? "@nszemit" : "@zemit") ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  orator = executor;
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

  if (!command_check_byname(executor, ns ? "@nspemit" : "@pemit") ||
      fun->flags & FN_NOSIDEFX) {
    safe_str(T(e_perm), buff, bp);
    return;
  }
  orator = executor;
  if (ns)
    flags |= PEMIT_SPOOF;
  do_pemit_list(executor, args[0], args[1], flags);
}


extern signed char qreg_indexes[UCHAR_MAX + 1];
/* ARGSUSED */
FUNCTION(fun_setq)
{
  /* sets a variable into a local register */
  int qindex;
  int n;

  if ((nargs % 2) != 0) {
    safe_format(buff, bp,
                T("#-1 FUNCTION (%s) EXPECTS AN EVEN NUMBER OF ARGUMENTS"),
                called_as);
    return;
  }

  for (n = 0; n < nargs; n += 2) {
    if (*args[n] && (*(args[n] + 1) == '\0') &&
        ((qindex = qreg_indexes[(unsigned char) args[n][0]]) != -1)
        && global_eval_context.renv[qindex]) {
      strcpy(global_eval_context.renv[qindex], args[n + 1]);
      if (n == 0 && !strcmp(called_as, "SETR"))
        safe_strl(args[n + 1], arglens[n + 1], buff, bp);
    } else
      safe_str(T("#-1 REGISTER OUT OF RANGE"), buff, bp);
  }
}

FUNCTION(fun_letq)
{
  char **values = NULL;
  int *regs = NULL;
  int npairs;
  int n;
  char tbuf[BUFFER_LEN], *tbp;
  char *preserve[NUMQ];
  const char *p;

  if ((nargs % 2) != 1) {
    safe_str(T("#-1 FUNCTION (LETQ) EXPECTS AN ODD NUMBER OF ARGUMENTS"),
             buff, bp);
    return;
  }

  npairs = (nargs - 1) / 2;

  for (n = 0; n < NUMQ; n++)
    preserve[n] = NULL;

  if (npairs) {
    values = mush_calloc(npairs, sizeof(char *), "letq.values");
    if (!values) {
      safe_str(T("#-1 UNABLE TO ALLOCATE MEMORY"), buff, bp);
      return;
    }

    regs = mush_calloc(npairs, sizeof(int), "letq.registers");
    if (!regs) {
      safe_str(T("#-1 UNABLE TO ALLOCATE MEMORY"), buff, bp);
      mush_free(values, "letq.values");
      return;
    }

    for (n = 0; n < npairs; n++) {
      int i = n * 2;

      tbp = tbuf;
      p = args[i];
      process_expression(tbuf, &tbp, &p, executor, caller, enactor, PE_DEFAULT,
                         PT_DEFAULT, pe_info);
      *tbp = '\0';
      regs[n] = qreg_indexes[(unsigned char) tbuf[0]];
      if (regs[n] < 0) {
        safe_str(T("#-1 REGISTER OUT OF RANGE"), buff, bp);
        goto cleanup;
      }

      tbp = tbuf;
      p = args[i + 1];
      process_expression(tbuf, &tbp, &p, executor, caller, enactor, PE_DEFAULT,
                         PT_DEFAULT, pe_info);
      *tbp = '\0';
      values[n] = mush_strdup(tbuf, "letq.value");
      if (!values[n]) {
        safe_str(T("#-1 UNABLE TO ALLOCATE MEMORY"), buff, bp);
        goto cleanup;
      }
    }

    for (n = 0; n < npairs; n++) {
      save_partial_global_reg("letq", preserve, regs[n]);
      mush_strncpy(global_eval_context.renv[regs[n]], values[n], BUFFER_LEN);
    }
  }
  p = args[nargs - 1];
  process_expression(buff, bp, &p, executor, caller, enactor, PE_DEFAULT,
                     PT_DEFAULT, pe_info);

cleanup:
  if (regs)
    mush_free(regs, "letq.registers");
  if (values) {
    restore_partial_global_regs("letq", preserve);
    for (n = 0; n < npairs; n++)
      mush_free(values[n], "letq.value");
    mush_free(values, "letq.values");
  }
}

/* ARGSUSED */
FUNCTION(fun_unsetq)
{
  /* sets a variable into a local register */
  char *ptr;
  int qindex;
  int i;

  if (nargs == 0 || args[0][0] == '\0') {
    for (i = 0; i < NUMQ; i++) {
      *(global_eval_context.renv[i]) = '\0';
    }
    return;
  }

  for (ptr = args[0]; *ptr; ptr++) {
    if ((qindex = qreg_indexes[(unsigned char) *ptr]) != -1) {
      *(global_eval_context.renv[qindex]) = '\0';
    } else if (!isspace((int) *ptr)) {
      safe_str(T("#-1 REGISTER OUT OF RANGE"), buff, bp);
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_r)
{
  /* returns a local register */
  int qindex;

  if (*args[0] && (*(args[0] + 1) == '\0') &&
      ((qindex = qreg_indexes[(unsigned char) args[0][0]]) != -1)
      && global_eval_context.renv[qindex])
    safe_str(global_eval_context.renv[qindex], buff, bp);
  else
    safe_str(T("#-1 REGISTER OUT OF RANGE"), buff, bp);
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
      offset = lowint;
      lowint = highint;
      highint = offset;
      offset = 0;
    }
    if (lowint < 0) {
      offset = 0 - lowint;
      low = 0;
      high = highint + offset;
    } else {
      low = lowint;
      high = highint;
    }
  }
  rand = get_random32(low, high);
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
      safe_uinteger(get_random32(1, die), buff, bp);
    }
  } else {
    for (count = 0; count < n; count++)
      total += get_random32(1, die);

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

  int j, per;
  char mstr[BUFFER_LEN], pstr[BUFFER_LEN], *dp;
  char const *sp;
  char *tbuf1 = NULL;
  int first = 1, found = 0, exact = 0;

  if (strstr(called_as, "ALL"))
    first = 0;

  if (string_prefix(called_as, "CASE"))
    exact = 1;

  dp = mstr;
  sp = args[0];
  process_expression(mstr, &dp, &sp, executor, caller, enactor,
                     PE_DEFAULT, PT_DEFAULT, pe_info);
  *dp = '\0';

  /* try matching, return match immediately when found */

  for (j = 1; j < (nargs - 1); j += 2) {
    dp = pstr;
    sp = args[j];
    process_expression(pstr, &dp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    *dp = '\0';

    if ((!exact)
        ? local_wild_match(pstr, mstr)
        : (strcmp(pstr, mstr) == 0)) {
      /* If there's a #$ in a switch's action-part, replace it with
       * the value of the conditional (mstr) before evaluating it.
       */
      if (!exact)
        tbuf1 = replace_string("#$", mstr, args[j + 1]);
      else
        tbuf1 = args[j + 1];

      sp = tbuf1;

      per = process_expression(buff, bp, &sp,
                               executor, caller, enactor,
                               PE_DEFAULT, PT_DEFAULT, pe_info);
      if (!exact)
        mush_free(tbuf1, "replace_string.buff");
      found = 1;
      if (per || first)
        return;
    }
  }

  if (!(nargs & 1) && !found) {
    /* Default case */
    if (!exact) {
      tbuf1 = replace_string("#$", mstr, args[nargs - 1]);
      sp = tbuf1;
    } else
      sp = args[nargs - 1];
    process_expression(buff, bp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    if (!exact)
      mush_free(tbuf1, "replace_string.buff");
  }
}

FUNCTION(fun_reswitch)
{
  /* this works a bit like the @switch/regexp command */

  int j, per = 0;
  char mstr[BUFFER_LEN], pstr[BUFFER_LEN], *dp;
  char const *sp;
  char *tbuf1;
  int first = 1, found = 0, flags = 0;
  int search, subpatterns, offsets[99];
  pcre *re;
  struct re_save rsave;
  ansi_string *mas;
  const char *errptr;
  int erroffset;
  pcre_extra *extra;

  if (strstr(called_as, "ALL"))
    first = 0;

  if (strcmp(called_as, "RESWITCHI") == 0
      || strcmp(called_as, "RESWITCHALLI") == 0)
    flags = PCRE_CASELESS;

  dp = mstr;
  sp = args[0];
  process_expression(mstr, &dp, &sp, executor, caller, enactor,
                     PE_DEFAULT, PT_DEFAULT, pe_info);
  *dp = '\0';
  mas = parse_ansi_string(mstr);

  save_regexp_context(&rsave);

  /* try matching, return match immediately when found */

  for (j = 1; j < (nargs - 1); j += 2) {
    dp = pstr;
    sp = args[j];
    process_expression(pstr, &dp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    *dp = '\0';

    if ((re =
         pcre_compile(remove_markup(pstr, NULL), flags, &errptr, &erroffset,
                      tables)) == NULL) {
      /* Matching error. Ignore this one, move on. */
      continue;
    }
    add_check("pcre");
    extra = default_match_limit();
    search = 0;
    subpatterns =
      pcre_exec(re, extra, mas->text, mas->len, search, 0, offsets, 99);
    if (subpatterns >= 0) {
      /* If there's a #$ in a switch's action-part, replace it with
       * the value of the conditional (mstr) before evaluating it.
       */
      tbuf1 = replace_string("#$", mstr, args[j + 1]);
      sp = tbuf1;
      /* set regexp context here */
      global_eval_context.re_code = re;
      global_eval_context.re_from = mas;
      global_eval_context.re_offsets = offsets;
      global_eval_context.re_subpatterns = subpatterns;
      per = process_expression(buff, bp, &sp,
                               executor, caller, enactor,
                               PE_DEFAULT | PE_DOLLAR, PT_DEFAULT, pe_info);
      mush_free(tbuf1, "replace_string.buff");
      found = 1;
    }
    mush_free(re, "pcre");
    if (first && found) {
      free_ansi_string(mas);
      restore_regexp_context(&rsave);
      return;
    }
    /* clear regexp context again here */
    global_eval_context.re_code = NULL;
    global_eval_context.re_subpatterns = -1;
    global_eval_context.re_offsets = NULL;
    global_eval_context.re_from = NULL;
    if (per) {
      free_ansi_string(mas);
      restore_regexp_context(&rsave);
      return;
    }
  }
  if (!(nargs & 1) && !found) {
    /* Default case */
    tbuf1 = replace_string("#$", mstr, args[nargs - 1]);
    sp = tbuf1;
    process_expression(buff, bp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    mush_free(tbuf1, "replace_string.buff");
  }
  free_ansi_string(mas);
  restore_regexp_context(&rsave);
}

/* ARGSUSED */
FUNCTION(fun_if)
{
  char tbuf[BUFFER_LEN], *tp;
  char const *sp;
  /* Since this may be used as COND(), NCOND(), CONDALL() or NCONDALL,
   * we check for that. */
  int findtrue = 1;
  int findall = 0;
  int found = 0;
  int i;

  if (called_as[0] == 'N')
    findtrue = 0;

  if (strstr(called_as, "ALL"))
    findall = 1;

  for (i = 0; i < nargs - 1; i += 2) {
    tp = tbuf;
    sp = args[i];
    process_expression(tbuf, &tp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    *tp = '\0';
    if (parse_boolean(tbuf) == findtrue) {
      sp = args[i + 1];
      process_expression(buff, bp, &sp, executor, caller, enactor,
                         PE_DEFAULT, PT_DEFAULT, pe_info);
      if (!findall)
        return;
      found = 1;
    }
  }
  /* If we've found no true case, run the default if it exists. */
  if (!found && (nargs & 1)) {
    sp = args[nargs - 1];
    process_expression(buff, bp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
  }
}

/* ARGSUSED */
FUNCTION(fun_mudname)
{
  safe_str(MUDNAME, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_mudurl)
{
  safe_str(MUDURL, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_version)
{
  safe_format(buff, bp, "PennMUSH version %s patchlevel %s %s",
              VERSION, PATCHLEVEL, PATCHDATE);
}

/* ARGSUSED */
FUNCTION(fun_numversion)
{
  safe_integer(NUMVERSION, buff, bp);
}

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
FUNCTION(fun_restarts)
{
  safe_integer(globals.reboot_count, buff, bp);
}

extern char soundex_val[UCHAR_MAX + 1];

/* The actual soundex routine */
static char *
soundex(char *str)
{
  static char tbuf1[BUFFER_LEN];
  char *p;

  memset(tbuf1, '\0', 4);

  p = tbuf1;

  /* First character is just copied */
  *p = UPCASE(*str);
  str++;
  /* Special case for PH->F */
  if ((UPCASE(*p) == 'P') && *str && (UPCASE(*str) == 'H')) {
    *p = 'F';
    str++;
  }
  p++;
  /* Convert letters to soundex values, squash duplicates, skip accents and other non-ascii characters */
  while (*str) {
    if (!isalpha((unsigned char) *str) || (unsigned char) *str > 127) {
      str++;
      continue;
    }
    *p = soundex_val[(unsigned char) *str++];
    if (*p != *(p - 1))
      p++;
  }
  *p = '\0';
  /* Remove zeros */
  p = str = tbuf1;
  while (*str) {
    if (*str != '0')
      *p++ = *str;
    str++;
  }
  *p = '\0';
  /* Pad/truncate to 4 chars */
  if (tbuf1[1] == '\0')
    tbuf1[1] = '0';
  if (tbuf1[2] == '\0')
    tbuf1[2] = '0';
  if (tbuf1[3] == '\0')
    tbuf1[3] = '0';
  tbuf1[4] = '\0';
  return tbuf1;
}

/* ARGSUSED */
FUNCTION(fun_soundex)
{
  /* Returns the soundex code for a word. This 4-letter code is:
   * 1. The first letter of the word (exception: ph -> f)
   * 2. Replace each letter with a numeric code from the soundex table
   * 3. Remove consecutive numbers that are the same
   * 4. Remove 0's
   * 5. Truncate to 4 characters or pad with 0's.
   * It's actually a bit messier than that to make it faster.
   */
  if (!args[0] || !*args[0] || !isalpha((unsigned char) *args[0])
      || strchr(args[0], ' ')) {
    safe_str(T("#-1 FUNCTION (SOUNDEX) REQUIRES A SINGLE WORD ARGUMENT"), buff,
             bp);
    return;
  }
  safe_str(soundex(args[0]), buff, bp);
  return;
}

/* ARGSUSED */
FUNCTION(fun_soundlike)
{
  /* Return 1 if the two arguments have the same soundex.
   * This can be optimized to go character-by-character, but
   * I deem the modularity to be more important. So there.
   */
  char tbuf1[5];
  if (!*args[0] || !*args[1] || !isalpha((unsigned char) *args[0])
      || !isalpha((unsigned char) *args[1]) || strchr(args[0], ' ')
      || strchr(args[1], ' ')) {
    safe_str(T("#-1 FUNCTION (SOUNDLIKE) REQUIRES TWO ONE-WORD ARGUMENTS"),
             buff, bp);
    return;
  }
  /* soundex uses a static buffer, so we need to save it */
  strcpy(tbuf1, soundex(args[0]));
  safe_boolean(!strcmp(tbuf1, soundex(args[1])), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_functions)
{
  safe_str(list_functions(nargs == 1 ? args[0] : NULL), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_null)
{
  return;
}

/* ARGSUSED */
FUNCTION(fun_list)
{
  if (!args[0] || !*args[0])
    safe_str("#-1", buff, bp);
  else if (string_prefix("motd", args[0]))
    safe_str(cf_motd_msg, buff, bp);
  else if (string_prefix("wizmotd", args[0]) && Hasprivs(executor))
    safe_str(cf_wizmotd_msg, buff, bp);
  else if (string_prefix("downmotd", args[0]) && Hasprivs(executor))
    safe_str(cf_downmotd_msg, buff, bp);
  else if (string_prefix("fullmotd", args[0]) && Hasprivs(executor))
    safe_str(cf_fullmotd_msg, buff, bp);
  else if (string_prefix("functions", args[0]))
    safe_str(list_functions(NULL), buff, bp);
  else if (string_prefix("@functions", args[0]))
    safe_str(list_functions("local"), buff, bp);
  else if (string_prefix("commands", args[0]))
    safe_str(list_commands(), buff, bp);
  else if (string_prefix("attribs", args[0]))
    safe_str(list_attribs(), buff, bp);
  else if (string_prefix("locks", args[0]))
    list_locks(buff, bp, NULL);
  else if (string_prefix("flags", args[0]))
    safe_str(list_all_flags("FLAG", "", executor, 0x3), buff, bp);
  else if (string_prefix("powers", args[0]))
    safe_str(list_all_flags("POWER", "", executor, 0x3), buff, bp);
  else
    safe_str("#-1", buff, bp);
  return;
}

/* ARGSUSED */
FUNCTION(fun_scan)
{
  dbref thing;
  char save_ccom[BUFFER_LEN];
  char *cmdptr;
  if (nargs == 1) {
    thing = executor;
    cmdptr = args[0];
  } else {
    thing = match_thing(executor, args[0]);
    if (!GoodObject(thing)) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
    if (!See_All(executor) && !controls(executor, thing)) {
      notify(executor, T("Permission denied."));
      safe_str("#-1", buff, bp);
      return;
    }
    cmdptr = args[1];
  }
  strcpy(save_ccom, global_eval_context.ccom);
  strncpy(global_eval_context.ccom, cmdptr, BUFFER_LEN);
  global_eval_context.ccom[BUFFER_LEN - 1] = '\0';
  safe_str(scan_list(thing, cmdptr), buff, bp);
  strcpy(global_eval_context.ccom, save_ccom);
}


enum whichof_t { DO_FIRSTOF, DO_ALLOF };
static void
do_whichof(char *args[], int nargs, enum whichof_t flag,
           char *buff, char **bp, dbref executor,
           dbref caller, dbref enactor, PE_Info *pe_info, int isbool)
{
  int j;
  char tbuf[BUFFER_LEN], *tp;
  char const *sp;
  char sep = ' ';
  int first = 1;
  tbuf[0] = '\0';
  if (flag == DO_ALLOF) {
    /* The last arg is a delimiter. Parse it in place. */
    char insep[BUFFER_LEN];
    char *isep = insep;
    const char *arglast = args[nargs - 1];
    process_expression(insep, &isep, &arglast, executor,
                       caller, enactor, PE_DEFAULT, PT_DEFAULT, pe_info);
    *isep = '\0';
    strcpy(args[nargs - 1], insep);
    if (!delim_check(buff, bp, nargs, args, nargs, &sep))
      return;
    nargs--;
  }

  for (j = 0; j < nargs; j++) {
    tp = tbuf;
    sp = args[j];
    process_expression(tbuf, &tp, &sp, executor, caller,
                       enactor, PE_DEFAULT, PT_DEFAULT, pe_info);
    *tp = '\0';
    if ((isbool && parse_boolean(tbuf)) || (!isbool && strlen(tbuf))) {
      if (!first) {
        safe_chr(sep, buff, bp);
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
  do_whichof(args, nargs, DO_FIRSTOF, buff, bp, executor,
             caller, enactor, pe_info, !!strcasecmp(called_as, "STRFIRSTOF"));
}


/* ARGSUSED */
FUNCTION(fun_allof)
{
  do_whichof(args, nargs, DO_ALLOF, buff, bp, executor,
             caller, enactor, pe_info, !!strcasecmp(called_as, "STRALLOF"));
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

/* ARGSUSED */
FUNCTION(fun_benchmark)
{
  char tbuf[BUFFER_LEN], *tp;
  char const *sp;
  int n;
  unsigned int min = UINT_MAX;
  unsigned int max = 0;
  unsigned int total = 0;
  int i;
  dbref thing = NOTHING;

  if (!is_number(args[1])) {
    safe_str(T(e_nums), buff, bp);
    return;
  }
  n = parse_number(args[1]);
  if (n < 1) {
    safe_str(T(e_range), buff, bp);
    return;
  }

  if (nargs > 2) {
    /* Evaluate <sendto> argument */
    tp = tbuf;
    sp = args[2];
    process_expression(tbuf, &tp, &sp, executor, caller, enactor,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    *tp = '\0';
    thing = noisy_match_result(executor, tbuf, NOTYPE, MAT_EVERYTHING);
    if (!GoodObject(thing)) {
      safe_dbref(thing, buff, bp);
      return;
    }
    if (!okay_pemit(executor, thing)) {
      notify_format(executor, T("I don't think #%d wants to hear from you."),
                    thing);
      safe_str("#-1", buff, bp);
      return;
    }
  }

  for (i = 1; i <= n; i++) {
    uint64_t start;
    unsigned int elapsed;
    tp = tbuf;
    sp = args[0];
    start = get_tsc();
    if (process_expression(tbuf, &tp, &sp, executor, caller, enactor,
                           PE_DEFAULT, PT_DEFAULT, pe_info)) {
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
    notify_format(thing, T("Average: %.2f   Min: %u   Max: %u"),
                  ((double) total) / i, min, max);
  } else {
    safe_format(buff, bp, T("Average: %.2f   Min: %u   Max: %u"),
                ((double) total) / i, min, max);
  }

  return;
}
