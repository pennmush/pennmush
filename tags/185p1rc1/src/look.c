/**
 * \file look.c
 *
 * \brief Commands that look at things.
 *
 *
 */

#include "config.h"
#include "copyrite.h"
#define _GNU_SOURCE
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "flags.h"
#include "lock.h"
#include "attrib.h"
#include "match.h"
#include "ansi.h"
#include "pueblo.h"
#include "extchat.h"
#include "game.h"
#include "command.h"
#include "parse.h"
#include "privtab.h"
#include "confmagic.h"
#include "log.h"

static void look_exits(dbref player, dbref loc, const char *exit_name,
                       NEW_PE_INFO *pe_info);
static void look_contents(dbref player, dbref loc, const char *contents_name,
                          NEW_PE_INFO *pe_info);
static void examine_atrs(dbref player, dbref thing, const char *mstr, int all,
                         int mortal, int parent);
static void mortal_examine_atrs(dbref player, dbref thing, const char *mstr,
                                int all, int parent);
static void look_simple(dbref player, dbref thing, NEW_PE_INFO *pe_info);
static void look_description(dbref player, dbref thing, const char *def,
                             const char *descname, const char *descformatname,
                             NEW_PE_INFO *pe_info);

enum decompile_attrflags { DECOMP_ALL, DECOMP_NODEFAULTS, DECOMP_NONE };

static int decompile_helper(dbref player, dbref thing, dbref parent,
                            char const *pattern, ATTR *atr, void *args);
static int examine_helper(dbref player, dbref thing, dbref parent,
                          char const *pattern, ATTR *atr, void *args);
static int examine_helper_veiled(dbref player, dbref thing, dbref parent,
                                 char const *pattern, ATTR *atr, void *args);
void decompile_atrs(dbref player, dbref thing, const char *name,
                    const char *pattern, const char *prefix,
                    enum decompile_attrflags skipflags);
void decompile_locks(dbref player, dbref thing, const char *name, int skipdef,
                     const char *prefix);
static char *parent_chain(dbref player, dbref thing);

extern PRIV attr_privs_view[];
extern int real_decompose_str(char *str, char *buff, char **bp);

/* Show the 'Obvious Exits' list for a room. Used in 'look' and 'examine'.
 * \param player The player looking
 * \param loc room whose exits we're showing
 * \param exit_name "Obvious Exits" string
 * \param pe_info the pe_info to use for evaluating EXITFORMAT and interact locks
 */
static void
look_exits(dbref player, dbref loc, const char *exit_name, NEW_PE_INFO *pe_info)
{
  dbref thing;
  char *tbuf1, *tbuf2, *nbuf;
  char *s1, *s2;
  char *p;
  int exit_count, this_exit, total_count;
  int texits;
  ufun_attrib ufun;
  PUEBLOBUFF;

  /* make sure location is a room */
  if (!IsRoom(loc))
    return;

  tbuf1 = (char *) mush_malloc(BUFFER_LEN, "string");
  tbuf2 = (char *) mush_malloc(BUFFER_LEN, "string");
  nbuf = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!tbuf1 || !tbuf2 || !nbuf)
    mush_panic("Unable to allocate memory in look_exits");
  s1 = tbuf1;
  s2 = tbuf2;
  texits = exit_count = total_count = 0;
  this_exit = 1;

  if (fetch_ufun_attrib
      ("EXITFORMAT", loc, &ufun, UFUN_IGNORE_PERMS | UFUN_REQUIRE_ATTR)) {
    char *arg, *buff, *bp;
    PE_REGS *pe_regs = pe_regs_create(PE_REGS_ARG, "look_exits");

    arg = (char *) mush_malloc(BUFFER_LEN, "string");
    buff = (char *) mush_malloc(BUFFER_LEN, "string");
    if (!arg || !buff)
      mush_panic("Unable to allocate memory in look_exits");

    bp = arg;
    DOLIST(thing, Exits(loc)) {
      if (((Light(loc) || Light(thing)) || !(Dark(loc) || Dark(thing)))
          && can_interact(thing, player, INTERACT_SEE, pe_info)) {
        if (bp != arg)
          safe_chr(' ', arg, &bp);
        safe_dbref(thing, arg, &bp);
      }
    }
    *bp = '\0';
    pe_regs_setenv_nocopy(pe_regs, 0, arg);

    call_ufun(&ufun, buff, player, player, pe_info, pe_regs);

    pe_regs_free(pe_regs);
    notify_by(loc, player, buff);
    mush_free(tbuf1, "string");
    mush_free(tbuf2, "string");
    mush_free(nbuf, "string");
    mush_free(arg, "string");
    mush_free(buff, "string");
    return;
  }
  /* Scan the room and see if there are any visible exits */
  if (Light(loc)) {
    for (thing = Exits(loc); thing != NOTHING; thing = Next(thing)) {
      total_count++;
      if (!Transparented(loc) || Opaque(thing))
        exit_count++;
    }
  } else if (Dark(loc)) {
    for (thing = Exits(loc); thing != NOTHING; thing = Next(thing)) {
      if (Light(thing) && can_interact(thing, player, INTERACT_SEE, pe_info)) {
        total_count++;
        if (!Transparented(loc) || Opaque(thing))
          exit_count++;
      }
    }
  } else {
    for (thing = Exits(loc); thing != NOTHING; thing = Next(thing)) {
      if ((Light(thing) || !DarkLegal(thing)) &&
          can_interact(thing, player, INTERACT_SEE, pe_info)) {
        total_count++;
        if (!Transparented(loc) || Opaque(thing))
          exit_count++;
      }
    }
  }
  if (total_count == 0) {
    /* No visible exits. We are outta here */
    mush_free(tbuf1, "string");
    mush_free(tbuf2, "string");
    mush_free(nbuf, "string");
    return;
  }

  PUSE;
  tag_wrap("FONT", "SIZE=+1", exit_name);
  PEND;
  notify_by(loc, player, pbuff);

  for (thing = Exits(loc); thing != NOTHING; thing = Next(thing)) {
    if ((Light(loc) || Light(thing) || (!DarkLegal(thing) && !Dark(loc)))
        && can_interact(thing, player, INTERACT_SEE, pe_info)) {
      strcpy(pbuff, accented_name(thing));
      if ((p = strchr(pbuff, ';')))
        *p = '\0';
      p = nbuf;
      safe_tag_wrap("A", tprintf("XCH_CMD=\"goto #%d\"", thing), pbuff, nbuf,
                    &p, NOTHING);
      *p = '\0';
      if (Transparented(loc) && !(Opaque(thing))) {
        if (SUPPORT_PUEBLO && !texits) {
          texits = 1;
          notify_noenter_by(loc, player, open_tag("UL"));
        }
        s1 = tbuf1;
        safe_tag("LI", tbuf1, &s1);
        safe_chr(' ', tbuf1, &s1);
        if (Destination(thing) == NOTHING)
          safe_format(tbuf1, &s1, T("%s leads nowhere."), nbuf);
        else if (HomeExit(thing))
          safe_format(tbuf1, &s1, T("%s leads home."), nbuf);
        else if (VariableExit(thing))
          safe_format(tbuf1, &s1, T("%s leads to a variable location."), nbuf);
        else if (!GoodObject(thing))
          safe_format(tbuf1, &s1, T("%s is corrupt!"), nbuf);
        else {
          safe_format(tbuf1, &s1, T("%s leads to %s."), nbuf,
                      Name(Destination(thing)));
        }
        safe_tag_cancel("LI", tbuf1, &s1);
        *s1 = '\0';
        notify_nopenter_by(loc, player, tbuf1);
      } else {
        if (COMMA_EXIT_LIST) {
          safe_itemizer(this_exit, (this_exit == exit_count),
                        ",", T("and"), " ", tbuf2, &s2);
          safe_str(nbuf, tbuf2, &s2);
          this_exit++;
        } else {
          safe_str(nbuf, tbuf2, &s2);
          safe_str("  ", tbuf2, &s2);
        }
      }
    }
  }
  if (SUPPORT_PUEBLO && texits) {
    PUSE;
    tag_cancel("UL");
    PEND;
    notify_noenter_by(loc, player, pbuff);
  }
  *s2 = '\0';
  notify_by(loc, player, tbuf2);
  mush_free(tbuf1, "string");
  mush_free(tbuf2, "string");
  mush_free(nbuf, "string");
}

/** Show the contents list of an object when it's looked at, obeying @conformat
 * \param player object looking
 * \param object looked at
 * \param contents_name String to show before contents list. "Contents" for rooms, "Carrying" for players/things
 * \param pe_info the pe_info to use for evaluating CONFORMAT and interact locks
 */
static void
look_contents(dbref player, dbref loc, const char *contents_name,
              NEW_PE_INFO *pe_info)
{
  dbref thing;
  dbref can_see_loc;
  ufun_attrib ufun;
  PUEBLOBUFF;
  /* check to see if he can see the location */
  /*
   * patched so that player can't see in dark rooms even if owned by that
   * player.  (he must use examine command)
   */
  can_see_loc = !Dark(loc);

  if (fetch_ufun_attrib
      ("CONFORMAT", loc, &ufun, UFUN_IGNORE_PERMS | UFUN_REQUIRE_ATTR)) {
    char *arg, *buff, *bp;
    char *arg2, *bp2;
    PE_REGS *pe_regs = pe_regs_create(PE_REGS_ARG, "look_contents");

    arg = (char *) mush_malloc(BUFFER_LEN, "string");
    arg2 = (char *) mush_malloc(BUFFER_LEN, "string");
    buff = (char *) mush_malloc(BUFFER_LEN, "string");
    if (!arg || !buff || !arg2)
      mush_panic("Unable to allocate memory in look_contents");
    bp = arg;
    bp2 = arg2;
    DOLIST(thing, Contents(loc)) {
      if (can_see(player, thing, can_see_loc)) {
        if (bp != arg)
          safe_chr(' ', arg, &bp);
        safe_dbref(thing, arg, &bp);
        if (bp2 != arg2)
          safe_chr('|', arg2, &bp2);
        safe_str(unparse_object_myopic(player, thing), arg2, &bp2);
      }
    }
    *bp = '\0';
    *bp2 = '\0';
    pe_regs_setenv_nocopy(pe_regs, 0, arg);
    pe_regs_setenv_nocopy(pe_regs, 1, arg2);

    call_ufun(&ufun, buff, player, player, pe_info, pe_regs);

    pe_regs_free(pe_regs);

    notify_by(loc, player, buff);
    mush_free(arg, "string");
    mush_free(arg2, "string");
    mush_free(buff, "string");
    return;
  }
  /* check to see if there is anything there */
  DOLIST(thing, Contents(loc)) {
    if (can_see(player, thing, can_see_loc)) {
      /* something exists!  show him everything */
      PUSE;
      tag_wrap("FONT", "SIZE=+1", contents_name);
      tag("UL");
      PEND;
      notify_nopenter_by(loc, player, pbuff);
      DOLIST(thing, Contents(loc)) {
        if (can_see(player, thing, can_see_loc)) {
          PUSE;
          tag("LI");
          tag_wrap("A", tprintf("XCH_CMD=\"look #%d\"", thing),
                   unparse_object_myopic(player, thing));
          tag_cancel("LI");
          PEND;
          notify_nopenter_by(loc, player, pbuff);
        }
      }
      PUSE;
      tag_cancel("UL");
      PEND;
      notify_noenter_by(loc, player, pbuff);
      break;                    /* we're done */
    }
  }
}

/* Helper function for atr_iter_get, obeying VEILED attrflag */
static int
examine_helper_veiled(dbref player, dbref thing __attribute__ ((__unused__)),
                      dbref parent __attribute__ ((__unused__)),
                      char const *pattern, ATTR *atr, void *args
                      __attribute__ ((__unused__)))
{
  char fbuf[BUFFER_LEN];
  char *r;

  if (EX_PUBLIC_ATTRIBS &&
      !strcmp(AL_NAME(atr), "DESCRIBE") && !strcmp(pattern, "*"))
    return 0;
  if (parent == thing || !GoodObject(parent))
    parent = NOTHING;
  strcpy(fbuf, privs_to_letters(attr_privs_view, AL_FLAGS(atr)));
  if (AF_Veiled(atr)) {
    if (ShowAnsi(player)) {
      if (GoodObject(parent))
        notify_format(player,
                      T("%s#%d/%s [#%d%s]%s is veiled"), ANSI_HILITE, parent,
                      AL_NAME(atr), Owner(AL_CREATOR(atr)), fbuf, ANSI_END);
      else
        notify_format(player,
                      T("%s%s [#%d%s]%s is veiled"), ANSI_HILITE, AL_NAME(atr),
                      Owner(AL_CREATOR(atr)), fbuf, ANSI_END);
    } else {
      if (GoodObject(parent))
        notify_format(player,
                      T("#%d/%s [#%d%s] is veiled"), parent, AL_NAME(atr),
                      Owner(AL_CREATOR(atr)), fbuf);
      else
        notify_format(player,
                      T("%s [#%d%s] is veiled"), AL_NAME(atr),
                      Owner(AL_CREATOR(atr)), fbuf);
    }
  } else {
    r = safe_atr_value(atr);
    if (ShowAnsi(player)) {
      if (GoodObject(parent))
        notify_format(player,
                      "%s#%d/%s [#%d%s]:%s %s", ANSI_HILITE, parent,
                      AL_NAME(atr), Owner(AL_CREATOR(atr)), fbuf, ANSI_END, r);
      else
        notify_format(player,
                      "%s%s [#%d%s]:%s %s", ANSI_HILITE, AL_NAME(atr),
                      Owner(AL_CREATOR(atr)), fbuf, ANSI_END, r);
    } else {
      if (GoodObject(parent))
        notify_format(player, "#%d/%s [#%d%s]: %s", parent, AL_NAME(atr),
                      Owner(AL_CREATOR(atr)), fbuf, r);
      else
        notify_format(player, "%s [#%d%s]: %s", AL_NAME(atr),
                      Owner(AL_CREATOR(atr)), fbuf, r);
    }
    free(r);
  }
  return 1;
}

/* Helper function for atr_iter_get(), ignoring VEILED attrflag */
static int
examine_helper(dbref player, dbref thing __attribute__ ((__unused__)),
               dbref parent __attribute__ ((__unused__)),
               char const *pattern, ATTR *atr, void *args
               __attribute__ ((__unused__)))
{
  char fbuf[BUFFER_LEN];
  char *r;

  if (EX_PUBLIC_ATTRIBS &&
      !strcmp(AL_NAME(atr), "DESCRIBE") && !strcmp(pattern, "*"))
    return 0;
  if (parent == thing || !GoodObject(parent))
    parent = NOTHING;
  strcpy(fbuf, privs_to_letters(attr_privs_view, AL_FLAGS(atr)));
  r = safe_atr_value(atr);
  if (ShowAnsi(player)) {
    if (GoodObject(parent))
      notify_format(player,
                    "%s#%d/%s [#%d%s]:%s %s", ANSI_HILITE, parent,
                    AL_NAME(atr), Owner(AL_CREATOR(atr)), fbuf, ANSI_END, r);
    else
      notify_format(player,
                    "%s%s [#%d%s]:%s %s", ANSI_HILITE, AL_NAME(atr),
                    Owner(AL_CREATOR(atr)), fbuf, ANSI_END, r);
  } else {
    if (GoodObject(parent))
      notify_format(player, "#%d/%s [#%d%s]: %s", parent, AL_NAME(atr),
                    Owner(AL_CREATOR(atr)), fbuf, r);
    else
      notify_format(player, "%s [#%d%s]: %s", AL_NAME(atr),
                    Owner(AL_CREATOR(atr)), fbuf, r);
  }
  free(r);
  return 1;
}

/** Show attributes on an object, for the 'examine' command
 * \param player object using 'examine'
 * \param thing object being examined
 * \param mstr wildcard pattern of attrs to show, or NULL for all
 * \param all show contents of veiled attributes?
 * \param mortal only show attrs vis to mortals, ignoring player's privs?
 * \param parent include inherited attributes from @parent?
 */
static void
examine_atrs(dbref player, dbref thing, const char *mstr, int all, int mortal,
             int parent)
{
  if (all || (mstr && *mstr && !wildcard((char *) mstr))) {
    if (parent) {
      if (!atr_iter_get_parent
          (player, thing, mstr, mortal, 0, examine_helper, NULL)
          && mstr)
        notify(player, T("No matching attributes."));
    } else {
      if (!atr_iter_get(player, thing, mstr, mortal, 0, examine_helper, NULL)
          && mstr)
        notify(player, T("No matching attributes."));
    }
  } else {
    if (parent) {
      if (!atr_iter_get_parent
          (player, thing, mstr, mortal, 0, examine_helper_veiled, NULL) && mstr)
        notify(player, T("No matching attributes."));
    } else {
      if (!atr_iter_get
          (player, thing, mstr, mortal, 0, examine_helper_veiled, NULL)
          && mstr)
        notify(player, T("No matching attributes."));
    }
  }
}

/* Wrapper for examine_atrs which only shows attrs visible to mortals */
static void
mortal_examine_atrs(dbref player, dbref thing, const char *mstr, int all,
                    int parent)
{
  examine_atrs(player, thing, mstr, all, 1, parent);
}

static void
look_simple(dbref player, dbref thing, NEW_PE_INFO *pe_info)
{
  enum look_type flag = LOOK_NORMAL;
  PUEBLOBUFF;

  PUSE;
  tag_wrap("FONT", "SIZE=+2", unparse_object_myopic(player, thing));
  PEND;
  notify_by(thing, player, pbuff);
  look_description(player, thing, T("You see nothing special."), "DESCRIBE",
                   "DESCFORMAT", pe_info);
  did_it(player, thing, NULL, NULL, "ODESCRIBE", NULL, "ADESCRIBE", NOTHING);
  if (IsExit(thing) && Transparented(thing)) {
    if (Cloudy(thing))
      flag = LOOK_CLOUDYTRANS;
    else
      flag = LOOK_TRANS;
  } else if (Cloudy(thing))
    flag = LOOK_CLOUDY;
  if (flag) {
    if (Location(thing) == HOME)
      look_room(player, Home(player), flag, pe_info);
    else if (GoodObject(thing) && GoodObject(Destination(thing)))
      look_room(player, Destination(thing), flag, pe_info);
  }
}

/** Look at a room.
 * The style parameter tells you what kind of look it is:
 * LOOK_NORMAL (caused by "look"), LOOK_TRANS (look through a transparent
 * exit), LOOK_AUTO (automatic look, by moving),
 * LOOK_CLOUDY (look through a cloudy exit - contents only), LOOK_CLOUDYTRANS
 * (look through a cloudy transparent exit - desc only).
 * \param player the looker.
 * \param loc room being looked at.
 * \param style how the room is being looked at.
 */
void
look_room(dbref player, dbref loc, enum look_type style, NEW_PE_INFO *pe_info)
{

  PUEBLOBUFF;
  ATTR *a;
  bool made_pe_info = 0;

  if (loc == NOTHING)
    return;

  if (!pe_info) {
    made_pe_info = 1;
    pe_info = make_pe_info("look_room");
    strcpy(pe_info->cmd_raw, "LOOK");
    strcpy(pe_info->cmd_evaled, "LOOK");
  }
  /* don't give the unparse if looking through Transparent exit */
  if (style == LOOK_NORMAL || style == LOOK_AUTO) {
    PUSE;
    tag("XCH_PAGE CLEAR=\"LINKS PLUGINS\"");
    if (SUPPORT_PUEBLO && style == LOOK_AUTO) {
      a = atr_get(loc, "VRML_URL");
      if (a) {
        tag(tprintf("IMG XCH_GRAPH=LOAD HREF=\"%s\"", atr_value(a)));
      } else {
        tag("IMG XCH_GRAPH=HIDE");
      }
    }
    tag("HR");
    tag_wrap("FONT", "SIZE=+2", unparse_room(player, loc, pe_info));
    PEND;
    notify_by(loc, player, pbuff);
  }
  if (!IsRoom(loc)) {
    if (style != LOOK_AUTO || !Terse(player)) {
      if (atr_get(loc, "IDESCRIBE")) {
        look_description(player, loc, NULL, "IDESCRIBE", "IDESCFORMAT",
                         pe_info);
        did_it(player, loc, NULL, NULL, "OIDESCRIBE", NULL, "AIDESCRIBE",
               NOTHING);
      } else if (atr_get(loc, "IDESCFORMAT")) {
        look_description(player, loc, NULL, "DESCRIBE", "IDESCFORMAT", pe_info);
      } else
        look_description(player, loc, NULL, "DESCRIBE", "DESCFORMAT", pe_info);
    }
  } else if (style == LOOK_NORMAL || style == LOOK_AUTO) {
    if (style == LOOK_NORMAL || !Terse(player)) {
      look_description(player, loc, NULL, "DESCRIBE", "DESCFORMAT", pe_info);
      did_it(player, loc, NULL, NULL, "ODESCRIBE", NULL, "ADESCRIBE", NOTHING);
    }
  } else if (style != LOOK_CLOUDY) {
    did_it(player, loc, NULL, NULL, "ODESCRIBE", NULL, "ADESCRIBE", NOTHING);
  }

  /* tell him the appropriate messages if he has the key */
  if (IsRoom(loc) && (style == LOOK_NORMAL || style == LOOK_AUTO)) {
    if (style == LOOK_AUTO && Terse(player)) {
      if (could_doit(player, loc, pe_info))
        did_it(player, loc, NULL, NULL, "OSUCCESS", NULL, "ASUCCESS", NOTHING);
      else
        did_it(player, loc, NULL, NULL, "OFAILURE", NULL, "AFAILURE", NOTHING);
    } else if (could_doit(player, loc, pe_info))
      did_it(player, loc, "SUCCESS", NULL, "OSUCCESS", NULL, "ASUCCESS",
             NOTHING);
    else
      fail_lock(player, loc, Basic_Lock, NULL, NOTHING);
  }

  /* tell him the contents */
  if (style != LOOK_CLOUDYTRANS)
    look_contents(player, loc, T("Contents:"), pe_info);
  if (style == LOOK_NORMAL || style == LOOK_AUTO) {
    look_exits(player, loc, T("Obvious exits:"), pe_info);
  }
  if (made_pe_info)
    free_pe_info(pe_info);
}

static void
look_description(dbref player, dbref thing, const char *def,
                 const char *descname, const char *descformatname,
                 NEW_PE_INFO *pe_info)
{
  /* Show thing's description to player, obeying DESCFORMAT if set */
  char buff[BUFFER_LEN], fbuff[BUFFER_LEN];
  ufun_attrib ufun;
  char *bp = buff;

  if (!GoodObject(player) || !GoodObject(thing))
    return;
  if (fetch_ufun_attrib
      (descname, thing, &ufun, UFUN_REQUIRE_ATTR | UFUN_IGNORE_PERMS))
    call_ufun(&ufun, buff, player, player, pe_info, NULL);
  else
    bp = NULL;

  if (fetch_ufun_attrib
      (descformatname, thing, &ufun, UFUN_REQUIRE_ATTR | UFUN_IGNORE_PERMS)) {
    PE_REGS *pe_regs = NULL;
    if (bp) {
      pe_regs = pe_regs_create(PE_REGS_ARG, "look_desc");
      pe_regs_setenv_nocopy(pe_regs, 0, buff);
    }
    /* We have a DESCFORMAT, evaluate it into fbuff and use it */
    /* If we have a DESCRIBE, pass the evaluated version as %0 */
    call_ufun(&ufun, fbuff, player, player, pe_info, pe_regs);
    if (pe_regs)
      pe_regs_free(pe_regs);
    notify_by(thing, player, fbuff);
  } else if (bp) {
    /* DESCRIBE only */
    notify_by(thing, player, buff);
  } else if (def) {
    /* Nothing, go with the default message */
    notify_by(thing, player, def);
  }
}

/** An automatic look (due to motion).
 * \param player the looker.
 */
void
do_look_around(dbref player)
{
  dbref loc;

  if ((loc = Location(player)) == NOTHING)
    return;
  look_room(player, loc, LOOK_AUTO, NULL);      /* auto-look. Obey TERSE. */
}

/** Look at something.
 * \param player the looker.
 * \param name name of object to look at.
 * \param key 0 for normal look, 1 for look/outside.
 * \param pe_info the pe_info for the *FORMAT attrs, lock checks, etc
 */
void
do_look_at(dbref player, const char *name, int key, NEW_PE_INFO *pe_info)
{
  dbref thing;
  dbref loc;
  int nearthis = 0;

  if (!GoodObject(Location(player)))
    return;

  if (key) {                    /* look outside */
    /* can't see through opaque objects */
    if (IsRoom(Location(player)) || Opaque(Location(player))) {
      notify(player, T("You can't see through that."));
      return;
    }
    loc = Location(Location(player));

    if (!GoodObject(loc))
      return;

    /* look at location of location */
    if (*name == '\0') {
      look_room(player, loc, LOOK_NORMAL, pe_info);
      return;
    }
    thing =
      match_result(loc, name, NOTYPE,
                   MAT_POSSESSION | MAT_CARRIED_EXIT | MAT_ENGLISH);
    if (thing == NOTHING) {
      notify(player, T("I don't see that here."));
      return;
    } else if (thing == AMBIGUOUS) {
      notify(player, T("I don't know which one you mean."));
      return;
    }
    nearthis = (loc == Location(thing));
  } else {                      /* regular look */
    if (*name == '\0') {
      look_room(player, Location(player), LOOK_NORMAL, pe_info);
      return;
    }
    /* look at a thing in location */
    if ((thing = match_result(player, name, NOTYPE, MAT_EVERYTHING)) == NOTHING) {
      dbref box;
      const char *boxname;
      char objnamebuf[BUFFER_LEN], *objname;
      boxname = name;
      strcpy(objnamebuf, name);
      objname = objnamebuf;
      box = parse_match_possessor(player, &objname, 1);
      if (box == NOTHING) {
        notify(player, T("I don't see that here."));
        return;
      } else if (box == AMBIGUOUS) {
        notify_format(player, T("I can't tell which %s."), boxname);
        return;
      }
      if (IsExit(box)) {
        /* Looking through an exit at an object on the other side */
        if (!(Transparented(box) && !Cloudy(box))
            && !(Cloudy(box) && !Transparented(box))) {
          notify_format(player, T("You can't see through that."));
          return;
        }
        box = Location(box);
        if (box == HOME)
          box = Home(player);   /* Resolve exits linked to HOME */
        if (!GoodObject(box)) {
          /* Do nothing for exits with no destination, or a variable destination */
          notify(player, T("You can't see through that."));
          return;
        }
        /* Including MAT_CARRIED_EXIT allows looking at remote exits, but gives slightly strange
           results when the remote exit is set transparent, and possibly lets you look at the back
           of the door you're looking through, which is odd */
        thing =
          match_result_relative(player, box, objname, NOTYPE, MAT_OBJ_CONTENTS);
        if (!GoodObject(thing)) {
          notify(player, T("I don't see that here."));
          return;
        }
        look_simple(player, thing, pe_info);
        return;
      }
      thing =
        match_result_relative(player, box, objname, NOTYPE, MAT_OBJ_CONTENTS);
      if (thing == NOTHING) {
        notify(player, T("I don't see that here."));
        return;
      } else if (thing == AMBIGUOUS) {
        notify_format(player, T("I can't tell which %s."), name);
        return;
      }
      if (Opaque(Location(thing)) &&
          (!See_All(player) &&
           !controls(player, thing) && !controls(player, Location(thing)))) {
        notify(player, T("You can't look at that from here."));
        return;
      }
      nearthis = nearby(player, box) && nearby(box, thing);
    } else if (thing == AMBIGUOUS) {
      notify(player, T("I can't tell which one you mean."));
      return;
    }
    nearthis = nearthis || nearby(player, thing);
  }

  /* once we've determined the object to look at, it doesn't matter whether
   * this is look or look/outside.
   */

  /* we need to check for the special case of a player doing 'look here'
   * while inside an object.
   */
  if (Location(player) == thing) {
    look_room(player, thing, LOOK_NORMAL, pe_info);
    return;
  } else if (!nearthis && !Long_Fingers(player) && !See_All(player)) {
    ATTR *desc;

    desc = atr_get(thing, "DESCRIBE");
    if ((desc && AF_Nearby(desc)) || (!desc && !READ_REMOTE_DESC)) {
      notify_format(player, T("You can't see that from here."));
      return;
    }
  }

  switch (Typeof(thing)) {
  case TYPE_ROOM:
    look_room(player, thing, LOOK_NORMAL, pe_info);
    break;
  case TYPE_THING:
  case TYPE_PLAYER:
    look_simple(player, thing, pe_info);
    if (!(Opaque(thing)))
      look_contents(player, thing, T("Carrying:"), pe_info);
    break;
  default:
    look_simple(player, thing, pe_info);
    break;
  }
}


/** Examine an object.
 * \param player the enactor doing the examining.
 * \param xname name of object to examine.
 * \param flag if 1, a brief examination. if 2, a mortal examination.
 * \param all if 1, include veiled attributes.
 * \param parent if 1, include parent attributes
 * \param opaque if 1, don't show contents list
 */
void
do_examine(dbref player, const char *xname, enum exam_type flag, int all,
           int parent, int opaque)
{
  dbref thing;
  ATTR *a;
  char *r;
  dbref content;
  dbref exit_dbref;
  char *name = NULL;
  char *attrib_name = NULL;
  char *tp;
  char *tbuf;
  int ok = 0;
  int listed = 0;
  PUEBLOBUFF;

  if (*xname == '\0') {
    if ((thing = Location(player)) == NOTHING)
      return;
  } else {
    name = mush_strdup(xname, "de.string");
    if ((attrib_name = strchr(name, '/')) != NULL) {
      *attrib_name = '\0';
      attrib_name++;
    }
    /* look it up */
    if ((thing =
         noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING)) == NOTHING) {
      mush_free(name, "de.string");
      return;
    }
  }
  /*  can't examine destructed objects  */
  if (IsGarbage(thing)) {
    notify(player, T("Garbage is garbage."));
    if (name)
      mush_free(name, "de.string");
    return;
  }
  /*  only look at some of the attributes */
  if (attrib_name && *attrib_name) {
    examine_atrs(player, thing, attrib_name, all, 0, parent);
    if (name)
      mush_free(name, "de.string");
    return;
  }
  if (flag == EXAM_MORTAL) {
    ok = 0;
  } else {
    ok = Can_Examine(player, thing);
  }

  tbuf = (char *) mush_malloc(BUFFER_LEN, "string");
  if (!ok && (!EX_PUBLIC_ATTRIBS || !nearby(player, thing))) {
    /* if it's not examinable and we're not near it, we can only get the
     * name and the owner.
     */
    tp = tbuf;
    safe_str(object_header(player, thing), tbuf, &tp);
    safe_str(T(" is owned by "), tbuf, &tp);
    safe_str(object_header(player, Owner(thing)), tbuf, &tp);
    *tp = '\0';
    notify(player, tbuf);
    mush_free(tbuf, "string");
    if (name)
      mush_free(name, "de.string");
    return;
  }
  if (ok) {
    PUSE;
    tag_wrap("FONT", "SIZE=+2", object_header(player, thing));
    PEND;
    notify(player, pbuff);
    if (FLAGS_ON_EXAMINE)
      notify(player, flag_description(player, thing));
  }
  if (EX_PUBLIC_ATTRIBS && (flag != EXAM_BRIEF)) {
    a = atr_get_noparent(thing, "DESCRIBE");
    if (a) {
      r = safe_atr_value(a);
      notify(player, r);
      free(r);
    }
  }
  if (ok) {
    char tbuf1[BUFFER_LEN];
    strcpy(tbuf1, object_header(player, Zone(thing)));
    notify_format(player,
                  T("Owner: %s  Zone: %s  %s: %d"),
                  object_header(player, Owner(thing)),
                  tbuf1, MONIES, Pennies(thing));
    notify_format(player, T("Parent: %s"), parent_chain(player, thing));
    {
      struct lock_list *ll;
      for (ll = Locks(thing); ll; ll = ll->next) {
        notify_format(player, T("%s Lock [#%d%s]: %s"),
                      L_TYPE(ll), L_CREATOR(ll), lock_flags(ll),
                      unparse_boolexp(player, L_KEY(ll), UB_ALL));
      }
    }
    notify_format(player, T("Powers: %s"), power_description(player, thing));

    notify(player, channel_description(thing));

    notify_format(player, T("Warnings checked: %s"),
                  unparse_warnings(Warnings(thing)));

    notify_format(player, T("Created: %s"), show_time(CreTime(thing), 0));
    if (!IsPlayer(thing))
      notify_format(player, T("Last Modification: %s"),
                    show_time(ModTime(thing), 0));
  }

  /* show attributes */
  switch (flag) {
  case EXAM_NORMAL:            /* Standard */
    if (EX_PUBLIC_ATTRIBS || ok)
      examine_atrs(player, thing, NULL, all, 0, parent);
    break;
  case EXAM_BRIEF:             /* Brief */
    break;
  case EXAM_MORTAL:            /* Mortal */
    if (EX_PUBLIC_ATTRIBS)
      mortal_examine_atrs(player, thing, NULL, all, parent);
    break;
  }

  /* show contents */
  if (!opaque && (Contents(thing) != NOTHING) &&
      (ok || (!IsRoom(thing) && !Opaque(thing)))) {
    DOLIST_VISIBLE(content, Contents(thing), (ok) ? GOD : player) {
      if (!listed) {
        listed = 1;
        if (IsPlayer(thing))
          notify(player, T("Carrying:"));
        else
          notify(player, T("Contents:"));
      }
      notify(player, object_header(player, content));
    }
  }
  if (!ok) {
    /* if not examinable, just show obvious exits and name and owner */
    if (IsRoom(thing))
      look_exits(player, thing, T("Obvious exits:"), NULL);
    tp = tbuf;
    safe_str(object_header(player, thing), tbuf, &tp);
    safe_str(T(" is owned by "), tbuf, &tp);
    safe_str(object_header(player, Owner(thing)), tbuf, &tp);
    *tp = '\0';
    notify(player, tbuf);
    mush_free(tbuf, "string");
    if (name)
      mush_free(name, "de.string");
    return;
  }
  switch (Typeof(thing)) {
  case TYPE_ROOM:
    /* tell him about exits */
    if (Exits(thing) != NOTHING) {
      notify(player, T("Exits:"));
      DOLIST(exit_dbref, Exits(thing))
        notify(player, object_header(player, exit_dbref));
    } else
      notify(player, T("No exits."));
    /* print dropto if present */
    if (Location(thing) != NOTHING) {
      notify_format(player,
                    T("Dropped objects go to: %s"),
                    object_header(player, Location(thing)));
    }
    break;
  case TYPE_THING:
  case TYPE_PLAYER:
    /* print home */
    notify_format(player, T("Home: %s"), object_header(player, Home(thing)));   /* home */
    /* print location if player can link to it */
    if (Location(thing) != NOTHING)
      notify_format(player,
                    T("Location: %s"), object_header(player, Location(thing)));
    break;
  case TYPE_EXIT:
    /* print source */
    switch (Source(thing)) {
    case NOTHING:
      do_rawlog(LT_ERR,
                "*** BLEAH *** Weird exit %s(#%d) in #%d with source NOTHING.",
                Name(thing), thing, Destination(thing));
      break;
    case AMBIGUOUS:
      do_rawlog(LT_ERR,
                "*** BLEAH *** Weird exit %s(#%d) in #%d with source AMBIG.",
                Name(thing), thing, Destination(thing));
      break;
    case HOME:
      do_rawlog(LT_ERR,
                "*** BLEAH *** Weird exit %s(#%d) in #%d with source HOME.",
                Name(thing), thing, Destination(thing));
      break;
    default:
      notify_format(player,
                    T("Source: %s"), object_header(player, Source(thing)));
      break;
    }
    /* print destination */
    switch (Destination(thing)) {
    case NOTHING:
      notify(player, T("Destination: *UNLINKED*"));
      break;
    case HOME:
      notify(player, T("Destination: *HOME*"));
      break;
    default:
      notify_format(player,
                    T("Destination: %s"),
                    object_header(player, Destination(thing)));
      break;
    }
    break;
  default:
    /* do nothing */
    break;
  }
  mush_free(tbuf, "string");
  if (name)
    mush_free(name, "de.string");
}

/** The score command: check a player's money.
 * \param player the enactor.
 */
void
do_score(dbref player)
{
  if (NoPay(player))
    notify_format(player, T("You have unlimited %s."), MONIES);
  else {
    notify_format(player,
                  T("You have %d %s."),
                  Pennies(player), Pennies(player) == 1 ? MONEY : MONIES);
    if (Moneybags(player))
      notify_format(player, T("You may give unlimited %s"), MONIES);
  }
}

/** The inventory command.
 * \param player the enactor.
 */
void
do_inventory(dbref player)
{
  dbref thing;
  ATTR *a;

  a = atr_get(player, "INVFORMAT");
  if (a) {
    char *arg, *buff, *bp, *save;
    char *arg2, *bp2;
    char const *sp;
    NEW_PE_INFO *pe_info;

    arg = (char *) mush_malloc(BUFFER_LEN, "string");
    arg2 = (char *) mush_malloc(BUFFER_LEN, "string");
    buff = (char *) mush_malloc(BUFFER_LEN, "string");
    if (!arg || !buff || !arg2)
      mush_panic("Unable to allocate memory in do_inventory");
    bp = arg;
    bp2 = arg2;
    DOLIST(thing, Contents(player)) {
      if (bp != arg)
        safe_chr(' ', arg, &bp);
      safe_dbref(thing, arg, &bp);
      if (bp2 != arg2)
        safe_chr('|', arg2, &bp2);
      safe_str(unparse_object_myopic(player, thing), arg2, &bp2);
    }
    *bp = '\0';
    *bp2 = '\0';
    pe_info = make_pe_info("pe_info-do_inv");
    pe_regs_setenv_nocopy(pe_info->regvals, 0, arg);
    pe_regs_setenv_nocopy(pe_info->regvals, 1, arg2);
    sp = save = safe_atr_value(a);
    bp = buff;
    process_expression(buff, &bp, &sp, player, player, player,
                       PE_DEFAULT, PT_DEFAULT, pe_info);
    *bp = '\0';
    free_pe_info(pe_info);
    free(save);
    notify(player, buff);
    mush_free(arg, "string");
    mush_free(arg2, "string");
    mush_free(buff, "string");
    return;
  }

  /* Default if no INVFORMAT */
  if ((thing = Contents(player)) == NOTHING) {
    notify(player, T("You aren't carrying anything."));
  } else {
    notify(player, T("You are carrying:"));
    DOLIST(thing, thing) {
      notify(player, unparse_object_myopic(player, thing));
    }
  }
  do_score(player);
}

/** The find command.
 * \param player the enactor.
 * \param name name pattern to search for.
 * \param argv array of additional arguments (for dbref ranges)
 */
void
do_find(dbref player, const char *name, char *argv[])
{
  dbref i;
  int count = 0;
  int bot = 0;
  int top = db_top;

  /* determinte range */
  if (argv[1] && *argv[1]) {
    size_t offset = 0;
    if (argv[1][0] == '#')
      offset = 1;
    bot = parse_integer(argv[1] + offset);
    if (!GoodObject(bot)) {
      notify(player, T("Invalid range argument"));
      return;
    }
  }
  if (argv[2] && *argv[2]) {
    size_t offset = 0;
    if (argv[2][0] == '#')
      offset = 1;
    top = parse_integer(argv[2] + offset);
    if (!GoodObject(top)) {
      notify(player, T("Invalid range argument"));
      return;
    }
  }

  for (i = bot; i < top; i++) {
    if (!IsGarbage(i) && !IsExit(i) && controls(player, i) &&
        (!*name || string_match(Name(i), name))) {
      notify(player, object_header(player, i));
      count++;
    }
  }
  notify_format(player, T("*** %d objects found ***"), count);
}

/** Sweep the current location for bugs.
 * \verbatim
 * This implements @sweep.
 * \endverbatim
 * \param player the enactor.
 * \param arg1 optional area to sweep.
 */
void
do_sweep(dbref player, const char *arg1)
{
  char tbuf1[BUFFER_LEN];
  dbref here = Location(player);
  int connect_flag = 0;
  int here_flag = 0;
  int inven_flag = 0;
  int exit_flag = 0;

  if (here == NOTHING)
    return;

  if (arg1 && *arg1) {
    if (string_prefix(arg1, "connected"))
      connect_flag = 1;
    else if (string_prefix(arg1, "here"))
      here_flag = 1;
    else if (string_prefix(arg1, "inventory"))
      inven_flag = 1;
    else if (string_prefix(arg1, "exits"))
      exit_flag = 1;
    else {
      notify(player, T("Invalid parameter."));
      return;
    }
  }
  if (!inven_flag && !exit_flag) {
    notify(player, T("Listening in ROOM:"));

    if (connect_flag) {
      /* only worry about puppet and players who's owner's are connected */
      if (Connected(here) || (Puppet(here) && Connected(Owner(here)))) {
        if (IsPlayer(here)) {
          notify_format(player, T("%s is listening."), Name(here));
        } else {
          notify_format(player, T("%s [owner: %s] is listening."),
                        Name(here), Name(Owner(here)));
        }
      }
    } else {
      if (Hearer(here) || Listener(here)) {
        if (Connected(here))
          notify_format(player, T("%s (this room) [speech]. (connected)"),
                        Name(here));
        else
          notify_format(player, T("%s (this room) [speech]."), Name(here));
      }
      if (Commer(here))
        notify_format(player, T("%s (this room) [commands]."), Name(here));
      if (Audible(here))
        notify_format(player, T("%s (this room) [broadcasting]."), Name(here));
    }

    for (here = Contents(here); here != NOTHING; here = Next(here)) {
      if (connect_flag) {
        /* only worry about puppet and players who's owner's are connected */
        if (Connected(here) || (Puppet(here) && Connected(Owner(here)))) {
          if (IsPlayer(here)) {
            notify_format(player, T("%s is listening."), Name(here));
          } else {
            notify_format(player, T("%s [owner: %s] is listening."),
                          Name(here), Name(Owner(here)));
          }
        }
      } else {
        if (Hearer(here) || Listener(here)) {
          if (Connected(here))
            notify_format(player, T("%s [speech]. (connected)"), Name(here));
          else
            notify_format(player, T("%s [speech]."), Name(here));
        }
        if (Commer(here))
          notify_format(player, T("%s [commands]."), Name(here));
      }
    }
  }
  if (!connect_flag && !inven_flag && IsRoom(Location(player))) {
    notify(player, T("Listening EXITS:"));
    if (Audible(Location(player))) {
      /* listening exits only work if the room is AUDIBLE */
      for (here = Exits(Location(player)); here != NOTHING; here = Next(here)) {
        if (Audible(here)) {
          copy_up_to(tbuf1, Name(here), ';');
          notify_format(player, T("%s [broadcasting]."), tbuf1);
        }
      }
    }
  }
  if (!here_flag && !exit_flag) {
    notify(player, T("Listening in your INVENTORY:"));

    for (here = Contents(player); here != NOTHING; here = Next(here)) {
      if (connect_flag) {
        /* only worry about puppet and players who's owner's are connected */
        if (Connected(here) || (Puppet(here) && Connected(Owner(here)))) {
          if (IsPlayer(here)) {
            notify_format(player, T("%s is listening."), Name(here));
          } else {
            notify_format(player, T("%s [owner: %s] is listening."),
                          Name(here), Name(Owner(here)));
          }
        }
      } else {
        if (Hearer(here) || Listener(here)) {
          if (Connected(here))
            notify_format(player, T("%s [speech]. (connected)"), Name(here));
          else
            notify_format(player, T("%s [speech]."), Name(here));
        }
        if (Commer(here))
          notify_format(player, T("%s [commands]."), Name(here));
      }
    }
  }
}

/** Locate a player.
 * \verbatim
 * This implements @whereis.
 * \endverbatim
 * \param player the enactor.
 * \param name name of player to locate.
 */
void
do_whereis(dbref player, const char *name)
{
  dbref thing;
  if (*name == '\0') {
    notify(player, T("You must specify a valid player name."));
    return;
  }
  if ((thing = lookup_player(name)) == NOTHING) {
    notify(player, T("That player does not seem to exist."));
    return;
  }
  if (!Can_Locate(player, thing)) {
    notify(player, T("That player wishes to have some privacy."));
    notify_format(thing, T("%s tried to locate you and failed."), Name(player));
    return;
  }
  notify_format(player,
                T("%s is at: %s."), Name(thing),
                unparse_object(player, Location(thing)));
  if (!See_All(player))
    notify_format(thing, T("%s has just located your position."), Name(player));
  return;

}

/** Store arguments for decompile_helper() */
struct dh_args {
  char const *prefix;   /**< Decompile/tf prefix */
  char const *name;     /**< Decompile object name */
  enum decompile_attrflags skipdef;          /**< Show all attrflags, none, or just skip defaults? */
};

/* **DESTRUCTIVELY MODIFIES** the original string! Beware! */
char *
decompose_str(char *what)
{
  static char value[BUFFER_LEN];
  char *vp = value;

  real_decompose_str(what, value, &vp);
  *vp = '\0';

  return value;
}

static int
decompile_helper(dbref player, dbref thing __attribute__ ((__unused__)),
                 dbref parent __attribute__ ((__unused__)),
                 const char *pattern
                 __attribute__ ((__unused__)), ATTR *atr, void *args)
{
  struct dh_args *dh = args;
  ATTR *ptr;
  char *avalue;
  int avlen;
  char msg[BUFFER_LEN];
  char *bp;

  if (AF_Nodump(atr))
    return 0;

  ptr = atr_match(AL_NAME(atr));
  bp = msg;
  safe_str(dh->prefix, msg, &bp);

  avalue = atr_value(atr);
  avlen = strlen(avalue);
  /* If avalue includes a %r, a %t, begins or ends with a %b, or has markup,
   * then use @set on the decompose_str'd value instead of &atrname */
  if (strchr(avalue, '\n') || strchr(avalue, '\t') ||
      strchr(avalue, TAG_START) || *avalue == ' ' || avalue[avlen - 1] == ' ') {
    safe_str("@set ", msg, &bp);
    safe_str(dh->name, msg, &bp);
    safe_chr('=', msg, &bp);
    safe_str(AL_NAME(atr), msg, &bp);
    safe_chr(':', msg, &bp);
    safe_str(decompose_str(avalue), msg, &bp);
  } else {
    /* Always use &attr, even for standard attributes, to avoid
     * clashing with @-commands, which take priority in the
     * command parser */
    safe_chr('&', msg, &bp);
    if (!ptr || strcmp(AL_NAME(atr), AL_NAME(ptr))) {
      ptr = NULL;               /* To speed later checks */
    }
    safe_str(AL_NAME(atr), msg, &bp);
    safe_chr(' ', msg, &bp);
    safe_str(dh->name, msg, &bp);
    safe_chr('=', msg, &bp);
    safe_str(avalue, msg, &bp);
  }
  *bp = '\0';
  notify(player, msg);
  /* Now deal with attribute flags, if not FugueEditing */
  if (dh->skipdef != DECOMP_NONE) {
    /* If skipdef is on, only show sets that aren't the defaults */
    const char *privs = NULL;
    if (dh->skipdef == DECOMP_NODEFAULTS && ptr) {
      /* Standard attribute. Get the default perms, if any. */
      /* Are we different? If so, do as usual */
      uint32_t npmflags = AL_FLAGS(ptr) & (~AF_PREFIXMATCH) & (~AF_ROOT);
      if (AL_FLAGS(atr) != AL_FLAGS(ptr) && AL_FLAGS(atr) != npmflags)
        privs = privs_to_string(attr_privs_view, AL_FLAGS(atr));
    } else {
      privs = privs_to_string(attr_privs_view, (AL_FLAGS(atr) & (~AF_ROOT)));
    }
    if (privs && *privs)
      notify_format(player, "%s@set %s/%s=%s", dh->prefix, dh->name,
                    AL_NAME(atr), privs);
  }
  return 1;
}

/** Decompile attributes on an object.
 * \param player the enactor.
 * \param thing object with attributes to decompile.
 * \param name name to refer to object by in decompile.
 * \param pattern pattern to match attributes to decompile.
 * \param prefix prefix to use for decompile/tf.
 * \param skipdef Whether or not to print all attribute flags, none, or skip defaults
 */
void
decompile_atrs(dbref player, dbref thing, const char *name, const char *pattern,
               const char *prefix, enum decompile_attrflags skipdef)
{
  struct dh_args dh;
  dh.prefix = prefix;
  dh.name = name;
  dh.skipdef = skipdef;
  /* Comment complaints if none are found */
  if (!atr_iter_get(player, thing, pattern, 0, 0, decompile_helper, &dh))
    notify_format(player, T("@@ No attributes match '%s'. @@"), pattern);
}

/** Decompile locks on an object.
 * \param player the enactor.
 * \param thing object with attributes to decompile.
 * \param name name to refer to object by in decompile.
 * \param skipdef if true, skip showing default lock flags.
 * \param prefix  The prefix to show before the locks.
 */
void
decompile_locks(dbref player, dbref thing, const char *name,
                int skipdef, const char *prefix)
{
  lock_list *ll;
  for (ll = Locks(thing); ll; ll = ll->next) {
    const lock_list *p = get_lockproto(L_TYPE(ll));
    if (p) {
      notify_format(player, "%s@lock/%s %s=%s", prefix,
                    L_TYPE(ll), name,
                    decompose_str(unparse_boolexp(player, L_KEY(ll),
                                                  UB_MEREF)));
      if (skipdef) {
        if (p && L_FLAGS(ll) == L_FLAGS(p))
          continue;
      }
      if (L_FLAGS(ll))
        notify_format(player, "%s@lset %s/%s=%s", prefix, name,
                      L_TYPE(ll), lock_flags_long(ll));
      if ((L_FLAGS(p) & LF_PRIVATE) && !(L_FLAGS(ll) & LF_PRIVATE))
        notify_format(player, "%s@lset %s/%s=!no_inherit", prefix,
                      name, L_TYPE(ll));
    } else {
      notify_format(player, "%s@lock/user:%s %s=%s", prefix,
                    ll->type, name,
                    decompose_str(unparse_boolexp(player, ll->key, UB_MEREF)));
      if (L_FLAGS(ll))
        notify_format(player, "%s@lset %s/%s=%s", prefix, name,
                      L_TYPE(ll), lock_flags_long(ll));
    }
  }
}

/** Decompile.
 * \verbatim
 * This implements @decompile.
 * \endverbatim
 * \param player the enactor.
 * \param xname name of object to decompile.
 * \param prefix the prefix to show before each line of output
 * \param dec_type flags for what to show in decompile, and how to show it
 */
void
do_decompile(dbref player, const char *xname, const char *prefix, int dec_type)
{
  dbref thing;
  char object[BUFFER_LEN];
  char *objp, *attrib, *attrname, *name;

  enum decompile_attrflags skipdef = DECOMP_ALL;

  if (dec_type & DEC_TF)
    skipdef = DECOMP_NONE;
  else if (dec_type & DEC_SKIPDEF)
    skipdef = DECOMP_NODEFAULTS;

  /* @decompile must always have an argument */
  if (!xname || !*xname) {
    notify(player, T("What do you want to @decompile?"));
    return;
  }
  name = mush_strdup(xname, "decompile.name");
  attrib = strchr(name, '/');
  if (attrib)
    *attrib++ = '\0';

  /* find object */
  if ((thing = noisy_match_result(player, name, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING) {
    mush_free(name, "decompile.name");
    return;
  }

  if (!GoodObject(thing) || IsGarbage(thing)) {
    notify(player, T("Garbage is garbage."));
    mush_free(name, "decompile.name");
    return;
  }

  objp = object;
  /* determine what we call the object */
  if (dec_type & DEC_DB)
    safe_dbref(thing, object, &objp);
  else {
    switch (Typeof(thing)) {
    case TYPE_PLAYER:
      if (!strcasecmp(name, "me"))
        safe_str("me", object, &objp);
      else {
        safe_chr('*', object, &objp);
        safe_str(Name(thing), object, &objp);
      }
      break;
    case TYPE_THING:
      safe_str(Name(thing), object, &objp);
      break;
    case TYPE_EXIT:
      safe_str(shortname(thing), object, &objp);
      break;
    case TYPE_ROOM:
      safe_str("here", object, &objp);
      break;
    }
  }
  *objp = '\0';

  /* if we have an attribute arg specified, wild match on it */
  if (attrib && *attrib) {
    attrname = attrib;
    while ((attrib = split_token(&attrname, ' ')) != NULL) {
      decompile_atrs(player, thing, object, attrib, prefix, skipdef);
    }
    mush_free(name, "decompile.name");
    return;
  } else if (!(dec_type & DEC_FLAG)) {
    /* Show all attrs, nothing else */
    decompile_atrs(player, thing, object, "**", prefix, skipdef);
    mush_free(name, "decompile.name");
    return;
  }

  /* else we have a full decompile */
  if (!Can_Examine(player, thing)) {
    notify(player, T("Permission denied."));
    mush_free(name, "decompile.name");
    return;
  }

  notify_format(player, "%s@@ %s (#%d)", prefix, shortname(thing), thing);
  switch (Typeof(thing)) {
  case TYPE_THING:
    notify_format(player, "%s@create %s", prefix, Name(thing));
    break;
  case TYPE_ROOM:
    notify_format(player, "%s@dig/teleport %s", prefix, Name(thing));
    break;
  case TYPE_EXIT:
    notify_format(player, "%s@open %s", prefix, Name(thing));
    break;
  }
  if (Mobile(thing)) {
    if (GoodObject(Home(thing)))
      notify_format(player, "%s@link %s = #%d", prefix, object, Home(thing));
    else if (Home(thing) == HOME)
      notify_format(player, "%s@link %s = HOME", prefix, object);
  } else {
    if (GoodObject(Destination(thing)))
      notify_format(player, "%s@link %s = #%d", prefix, object,
                    Destination(thing));
    else if (Destination(thing) == AMBIGUOUS)
      notify_format(player, "%s@link %s = VARIABLE", prefix, object);
    else if (Destination(thing) == HOME)
      notify_format(player, "%s@link %s = HOME", prefix, object);
  }
  if (GoodObject(Zone(thing)))
    notify_format(player, "%s@chzone %s = #%d", prefix, object, Zone(thing));
  if (GoodObject(Parent(thing)))
    notify_format(player, "%s@parent %s=#%d", prefix, object, Parent(thing));
  decompile_locks(player, thing, object, (skipdef != DECOMP_ALL), prefix);
  decompile_flags(player, thing, object, prefix);
  decompile_powers(player, thing, object, prefix);

  /* Show attrs as well */
  if (dec_type & DEC_ATTR) {
    decompile_atrs(player, thing, object, "**", prefix, skipdef);
  }
  mush_free(name, "decompile.name");
}

static char *
parent_chain(dbref player, dbref thing)
{
  static char tbuf1[BUFFER_LEN];
  char *bp;
  dbref parent;
  int depth = 0;

  bp = tbuf1;
  parent = Parent(thing);
  safe_str(object_header(player, parent), tbuf1, &bp);
  while (depth < MAX_PARENTS && GoodObject(parent) &&
         GoodObject(Parent(parent)) && Can_Examine(player, Parent(parent))) {
    parent = Parent(parent);
    safe_str(" -> ", tbuf1, &bp);
    safe_str(object_header(player, parent), tbuf1, &bp);
    depth++;
  }
  *bp = '\0';
  return tbuf1;
}
