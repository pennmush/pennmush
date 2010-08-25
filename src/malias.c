/**
 * \file malias.c
 *
 * \brief  Global mail aliases/lists
 *
 * \verbatim
 *
 * This code implements an extension to extended @mail which allows
 * admin (and others who are so em@powered) to create mail aliases
 * for the MUSH. Optionally, any player can be allowed to.
 *
 * Aliases are used by @mail'ing to !<alias name>
 * Aliases have a name, a description, a list of members (dbrefs), an owner
 * a size (how many members), and two kinds of flags. 
 * nflags control who can use/see an alias name, and mflags 
 * control who can see the alias members. The choices
 * are everyone, alias members, owner, admin
 * 
 * Interface:
 * @malias[/list]
 * @malias/members !name
 * @malias[/create] !name=list-of-members
 * @malias/destroy !name
 * @malias/add !name=list-of-members
 * @malias/remove !name=list-of-members
 * @malias/desc !name=description
 * @malias/nameprivs !name=flags
 * @malias/listprivs !name=flags
 * @malias/stat
 * @malias/chown !name=owner    (Admin only)
 * @malias/nuke                 (Admin only)
 *
 * \endverbatim
 */

#define MA_INC 3        /**< How many maliases we malloc at a time */
#include "config.h"
#include "copyrite.h"

#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <ctype.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#include <string.h>

#include "conf.h"
#include "externs.h"
#include "mushdb.h"
#include "dbdefs.h"
#include "match.h"
#include "parse.h"
#include "malias.h"
#include "privtab.h"
#include "mymalloc.h"
#include "flags.h"
#include "pueblo.h"
#include "log.h"
#include "dbio.h"
#include "confmagic.h"



int ma_size = 0;   /**< Number of maliases */
int ma_top = 0;    /**< Top of alias array */
struct mail_alias *malias; /**< Pointer to linked list of aliases */

/** Privilege table for maliases. */
static PRIV malias_priv_table[] = {
  {"Admin", 'A', ALIAS_ADMIN, ALIAS_ADMIN},
  {"Members", 'M', ALIAS_MEMBERS, ALIAS_MEMBERS},
  {"Owner", 'O', ALIAS_OWNER, ALIAS_OWNER},
  {NULL, '\0', 0, 0}
};

static const char *get_shortprivs(struct mail_alias *m);


/***********************************************************
***** User-commands *****
***********************************************************/


/** List or create a malias.
 * \verbatim
 * This implements the @malias command (with no switches).
 * \endverbatim
 * \param player the enactor.
 * \param arg1 name of malias to create or list, or NULL to list all.
 * \param arg2 parameters for creation, or NULL to list.
 */
void
do_malias(dbref player, char *arg1, char *arg2)
{
  if (!arg1 || !*arg1) {
    if (arg2 && *arg2) {
      notify(player, T("MAIL: Invalid malias command."));
      return;
    }
    /* just the "@malias" command */
    do_malias_list(player);
    return;
  }
  if (arg2 && *arg2) {
    /* Creating malias */
    do_malias_create(player, arg1, arg2);
  } else {
    /* List specific alias - no arg2 */
    do_malias_members(player, arg1);
  }
}


/** Create a malias.
 * \verbatim
 * This implements the @malias/create command.
 * \endverbatim
 * \param player the enactor.
 * \param alias name of malias to create.
 * \param tolist parameters for creation.
 */
void
do_malias_create(dbref player, char *alias, char *tolist)
{
  char *head, *tail, spot;
  struct mail_alias *m;
  char *na;
  const char *buff, *good, *scan;
  int i = 0;
  dbref target;
  dbref alist[100];

  if (!IsPlayer(player)) {
    notify(player, T("MAIL: Only players may create mail aliases."));
    return;
  }
  if (!alias || !*alias || !tolist || !*tolist) {
    notify(player, T("MAIL: What alias do you want to create?"));
    return;
  }
  if (*alias != MALIAS_TOKEN) {
    notify_format(player,
                  T("MAIL: All Mail aliases must begin with '%c'."),
                  MALIAS_TOKEN);
    return;
  }
  good = "`$_-.'";
  /* Make sure that the name contains legal characters only */
  for (scan = alias + 1; scan && *scan; scan++) {
    if (isalnum((unsigned char) *scan))
      continue;
    if (!strchr(good, *scan)) {
      notify(player, T("MAIL: Invalid character in mail alias."));
      return;
    }
  }
  m = get_malias(GOD, alias);   /* GOD can see all aliases */
  if (m) {                      /* Ensures no duplicates!  */
    notify_format(player, T("MAIL: Mail Alias '%s' already exists."), alias);
    return;
  }
  if (!ma_size) {
    ma_size = MA_INC;
    malias = mush_calloc(ma_size, sizeof(struct mail_alias), "malias_list");
  } else if (ma_top >= ma_size) {
    ma_size += MA_INC;
    m = mush_calloc(ma_size, sizeof(struct mail_alias), "malias_list");
    memcpy(m, malias, sizeof(struct mail_alias) * ma_top);
    mush_free(malias, "malias_list");
    malias = m;
  }
  i = 0;

  /*
   * Parse the player list
   */
  head = (char *) tolist;
  while (head && *head) {
    while (*head == ' ')
      head++;
    tail = head;
    while (*tail && (*tail != ' ')) {
      if (*tail == '"') {
        head++;
        tail++;
        while (*tail && (*tail != '"'))
          tail++;
      }
      if (*tail)
        tail++;
    }
    tail--;
    if (*tail != '"')
      tail++;
    spot = *tail;
    *tail = '\0';
    /*
     * Now locate a target
     */
    if (!strcasecmp(head, "me"))
      target = player;
    else if (*head == '#') {
      target = atoi(head + 1);
    } else
      target = lookup_player(head);
    if (!(GoodObject(target)) || (!IsPlayer(target))) {
      notify_format(player, T("MAIL: No such player '%s'."), head);
    } else {
      buff = unparse_object(player, target);
      notify_format(player, T("MAIL: %s added to alias %s"), buff, alias);
      alist[i] = target;
      i++;
    }
    /*
     * Get the next recip
     */
    *tail = spot;
    head = tail;
    if (*head == '"')
      head++;
    if (i == 100)
      break;
  }

  if (head && *head) {
    notify(player, T("MAIL: Alias list is restricted to maximal 100 entries!"));
  }
  if (!i) {
    notify(player, T("MAIL: No valid recipients for alias-list!"));
    return;
  }
  m = &malias[ma_top];
  m->members = mush_calloc(i, sizeof(dbref), "malias_members");
  memcpy(m->members, alist, sizeof(dbref) * i);

  na = alias + 1;
  m->size = i;
  m->owner = player;
  m->name = mush_strdup(na, "malias_name");
  m->desc = compress(na);
  add_check("malias_desc");
  m->nflags = ALIAS_OWNER | ALIAS_MEMBERS;
  m->mflags = ALIAS_OWNER;
  ma_top++;


  notify_format(player, T("MAIL: Alias set '%s' defined."), alias);
}


/** List maliases.
 * \verbatim
 * This function implements @malias/list.
 * \endverbatim
 * \param player the enactor.
 */
void
do_malias_list(dbref player)
{
  struct mail_alias *m;
  int i = 0;
  int notified = 0;

  for (i = 0; i < ma_top; i++) {
    m = &malias[i];
    if ((m->owner == player) || (m->nflags == 0) ||
        ((m->nflags & ALIAS_ADMIN) && Hasprivs(player)) ||
        ((m->nflags & ALIAS_MEMBERS) && ismember(m, player))) {
      if (!notified) {
        notify_format(player, "%-13s %-35s %s %-15s",
                      T("Name"), T("Alias Description"), T("Use See"),
                      T("Owner"));
        notified++;
      }
      notify_format(player,
                    "%c%-12.12s %-35.35s %s %-15.15s", MALIAS_TOKEN, m->name,
                    uncompress((unsigned char *) (m->desc)), get_shortprivs(m),
                    Name(m->owner));
    }
  }

  notify(player, T("*****  End of Mail Aliases *****"));
}

/** List malias members.
 * \verbatim
 * This function implements @malias/members.
 * \endverbatim
 * \param player the enactor.
 * \param alias name of the alias to list members of.
 */
void
do_malias_members(dbref player, char *alias)
{
  struct mail_alias *m;
  int i = 0;
  char buff[BUFFER_LEN];
  char *bp;

  m = get_malias(player, alias);

  if (!m) {
    notify_format(player, T("MAIL: Alias '%s' not found."), alias);
    return;
  }
  if ((m->owner == player) || (m->mflags == 0) ||
      (Hasprivs(player)) ||
      ((m->mflags & ALIAS_MEMBERS) && ismember(m, player))) {
    /* Dummy to avoid having to invert the "if" above ;-) */
  } else {
    notify(player, T("MAIL: Permission denied."));
    return;
  }
  bp = buff;
  safe_format(buff, &bp, T("MAIL: Alias %c%s: "), MALIAS_TOKEN, m->name);
  for (i = 0; i < m->size; i++) {
    safe_str(Name(m->members[i]), buff, &bp);
    safe_chr(' ', buff, &bp);
    /* Attention if player names may contain spaces!! */
  }
  *bp = '\0';
  notify(player, buff);
}

FUNCTION(fun_malias)
{
  /* With no arguments, list all alias names
   * With one argument, it's either a delimiter or list all member dbrefs
   * With two arguments, it's a malias name and a delimiter, and we
   * list dbrefs, delimited
   */
  int i;
  int count = 0;
  char sep = ' ';
  struct mail_alias *m;

  if (nargs >= 1) {
    m = get_malias(executor, args[0]);
    if (m) {
      if (!delim_check(buff, bp, nargs, args, 2, &sep))
        return;
      if ((m->owner == executor) || (m->mflags == 0) ||
          (Hasprivs(executor)) ||
          ((m->mflags & ALIAS_MEMBERS) && ismember(m, executor))) {
        for (i = 0; i < m->size; i++) {
          if (count++)
            safe_chr(sep, buff, bp);
          safe_dbref(m->members[i], buff, bp);
        }
      } else {
        safe_str(T(e_perm), buff, bp);
      }
      return;
    } else {
      /* Perhaps it's a delimiter? */
      if (arglens[0] > 1) {
        /* Oops, not if it's longer than one character */
        safe_str(T(e_match), buff, bp);
        return;
      }
      if (!delim_check(buff, bp, nargs, args, 1, &sep))
        return;

    }

  }
  /* List maliases, possibly with a delimiter */
  for (i = 0; i < ma_top; i++) {
    m = &malias[i];
    if ((m->owner == executor) || (m->nflags == 0) ||
        ((m->nflags & ALIAS_ADMIN) && Hasprivs(executor)) ||
        ((m->nflags & ALIAS_MEMBERS) && ismember(m, executor))) {
      if (count++)
        safe_chr(sep, buff, bp);
      safe_chr(MALIAS_TOKEN, buff, bp);
      safe_str(m->name, buff, bp);
    }
  }
}


/** Describe a malias.
 * \verbatim
 * This implements the @malias/desc command.
 * \endverbatim
 * \param player the enactor.
 * \param alias name of the malias to describe.
 * \param desc description to set.
 */
void
do_malias_desc(dbref player, char *alias, char *desc)
{
  struct mail_alias *m;

  if (!(m = get_malias(player, alias))) {
    notify_format(player, T("MAIL: Alias %s not found."), alias);
    return;
  } else if (Wizard(player) || (player == m->owner)) {
    if (m->desc)
      free(m->desc);            /* No need to update MEM_CHECK records here */
    m->desc = compress(desc);
    notify(player, T("MAIL: Description changed."));
  } else
    notify(player, T("MAIL: Permission denied."));
  return;
}


/** Change ownership of a malias.
 * \verbatim
 * This implements the @malias/chown command.
 * \endverbatim
 * \param player the enactor.
 * \param alias name of the malias to chown.
 * \param owner name of the new owner.
 */
void
do_malias_chown(dbref player, char *alias, char *owner)
{
  struct mail_alias *m;
  dbref no = NOTHING;

  if (!(m = get_malias(player, alias))) {
    notify_format(player, T("MAIL: Alias %s not found."), alias);
    return;
  } else {
    if (!Wizard(player)) {
      notify(player, T("MAIL: You cannot do that!"));
      return;
    } else {
      if ((no = lookup_player(owner)) == NOTHING) {
        notify(player, T("MAIL: I cannot find that player."));
        return;
      }
      m->owner = no;
      notify(player, T("MAIL: Owner changed for alias."));
    }
  }
}



/** Change name of a malias.
 * \verbatim
 * This implements the @malias/rename command.
 * \endverbatim
 * \param player the enactor.
 * \param alias name of the malias to rename.
 * \param newname new name for the malias.
 */
void
do_malias_rename(dbref player, char *alias, char *newname)
{
  struct mail_alias *m;

  if ((m = get_malias(player, alias)) == NULL) {
    notify(player, T("MAIL: I cannot find that alias!"));
    return;
  }
  if (*newname != MALIAS_TOKEN) {
    notify_format(player,
                  T("MAIL: Bad alias. Aliases must start with '%c'."),
                  MALIAS_TOKEN);
    return;
  }
  if (get_malias(GOD, newname) != NULL) {
    notify(player, T("MAIL: That name already exists!"));
    return;
  }
  if (!Wizard(player) && !(m->owner == player)) {
    notify(player, T("MAIL: Permission denied."));
    return;
  }

  free(m->name);                /* No need to update MEM_CHECK records here. */
  m->name = strdup(newname + 1);

  notify(player, T("MAIL: Mail Alias renamed."));
}



/** Delete a malias.
 * \verbatim
 * This implements the @malias/destroy command.
 * \endverbatim
 * \param player the enactor.
 * \param alias name of the malias to destroy.
 */
void
do_malias_destroy(dbref player, char *alias)
{
  struct mail_alias *m;
  m = get_malias(player, alias);
  if (!m) {
    notify_format(player,
                  T
                  ("MAIL: Not a valid alias. Remember to prefix the alias name with %c."),
                  MALIAS_TOKEN);
    return;
  }
  if (Wizard(player) || (m->owner == player)) {
    notify(player, T("MAIL: Alias Destroyed."));
    if (m->members)
      mush_free(m->members, "malias_members");
    if (m->name)
      mush_free(m->name, "malias_name");
    if (m->desc)
      mush_free(m->desc, "malias_desc");
    *m = malias[--ma_top];
  } else {
    notify(player, T("MAIL: Permission denied!"));
  }
}


/** Set the membership list for a malias.
 * \verbatim
 * This implements the @malias/set command.
 * \endverbatim
 * \param player the enactor.
 * \param alias name of the malias to set members for.
 * \param tolist space-separated list of players to set as members.
 */
void
do_malias_set(dbref player, char *alias, char *tolist)
{
  struct mail_alias *m;
  int i = 0;
  char *head, *tail, spot;
  const char *buff;
  dbref alist[100];
  dbref target;

  m = get_malias(player, alias);
  if (!m) {
    notify_format(player,
                  T
                  ("MAIL: Not a valid alias. Remember to prefix the alias name with %c."),
                  MALIAS_TOKEN);
    return;
  }
  if (!tolist || !*tolist) {
    notify(player, T("MAIL: You must set the alias to a non-empty list."));
    return;
  }
  if (!(Wizard(player) || (m->owner == player))) {
    notify(player, T("MAIL: Permission denied!"));
    return;
  }

  /*
   * Parse the player list
   */
  head = (char *) tolist;
  while (head && *head) {
    while (*head == ' ')
      head++;
    tail = head;
    while (*tail && (*tail != ' ')) {
      if (*tail == '"') {
        head++;
        tail++;
        while (*tail && (*tail != '"'))
          tail++;
      }
      if (*tail)
        tail++;
    }
    tail--;
    if (*tail != '"')
      tail++;
    spot = *tail;
    *tail = '\0';
    /*
     * Now locate a target
     */
    if (!strcasecmp(head, "me"))
      target = player;
    else if (*head == '#') {
      target = atoi(head + 1);
    } else
      target = lookup_player(head);
    if (!(GoodObject(target)) || (!IsPlayer(target))) {
      notify_format(player, T("MAIL: No such player '%s'."), head);
    } else {
      buff = unparse_object(player, target);
      notify_format(player, T("MAIL: %s added to alias %s"), buff, alias);
      alist[i] = target;
      i++;
    }
    /*
     * Get the next recip
     */
    *tail = spot;
    head = tail;
    if (*head == '"')
      head++;
    if (i == 100)
      break;
  }

  if (head && *head) {
    notify(player, T("MAIL: Alias list is restricted to maximal 100 entries!"));
  }
  if (!i) {
    notify(player, T("MAIL: No valid recipients for alias-list!"));
    return;
  }
  if (m->members)
    mush_free(m->members, "malias_members");
  m->members = mush_calloc(i, sizeof(dbref), "malias_members");
  memcpy(m->members, alist, sizeof(dbref) * i);
  m->size = i;
  notify(player, T("MAIL: Alias list set."));
}



/** List all maliases.
 * \verbatim
 * This implements the @malias/list command.
 * \endverbatim
 * \param player the enactor.
 */
void
do_malias_all(dbref player)
{
  struct mail_alias *m;
  int i;

  if (!Hasprivs(player)) {
    do_malias_list(player);
    return;
  }
  notify(player,
         "Num   Name       Description                              Owner       Count");

  for (i = 0; i < ma_top; i++) {
    m = &malias[i];
    notify_format(player, "#%-4d %c%-10.10s %-40.40s %-11.11s (%3d)",
                  i, MALIAS_TOKEN, m->name,
                  uncompress((unsigned char *) m->desc),
                  Name(m->owner), m->size);
  }

  notify(player, T("***** End of Mail Aliases *****"));
}




/** Statistics on maliases.
 * \verbatim
 * This implements the @malias/stat command.
 * \endverbatim
 * \param player the enactor.
 */
void
do_malias_stats(dbref player)
{
  if (!Hasprivs(player))
    notify(player, T("MAIL: Permission denied."));
  else {
    notify_format(player,
                  T("MAIL: Number of mail aliases defined: %d"), ma_top);
    notify_format(player, T("MAIL: Allocated slots %d"), ma_size);
  }
}

/** Remove all maliases.
 * \verbatim
 * This implements the @malias/nuke command.
 * \endverbatim
 * \param player the enactor.
 */
void
do_malias_nuke(dbref player)
{
  struct mail_alias *m;
  int i;

  if (!God(player)) {
    notify(player, T("MAIL: Only god can do that!"));
    return;
  }
  if (ma_size) {                /* aliases defined ? */
    for (i = 0; i < ma_top; i++) {
      m = &malias[i];
      if (m->name)
        mush_free(m->name, "malias_name");
      if (m->desc)
        mush_free(m->desc, "malias_desc");
      if (m->members)
        mush_free(m->members, "malias_members");
    }
    mush_free(malias, "malias_list");
  }
  ma_size = ma_top = 0;
  notify(player, T("MAIL: All mail aliases destroyed!"));
}


/** Set permisions on maliases.
 * \verbatim
 * This implements @malias/use and @malias/see
 * \endverbatim
 * \param player the enactor.
 * \param alias name of the malias.
 * \param privs string of privs to set.
 * \param type if 1, setting nprivs, if 0, mprivs.
 */
void
do_malias_privs(dbref player, char *alias, char *privs, int type)
{
  struct mail_alias *m;
  int *p;

  if (!(m = get_malias(player, alias))) {
    notify(player, T("MAIL: I cannot find that alias!"));
    return;
  }
  if (!Wizard(player) && (m->owner != player)) {
    notify(player, T("MAIL: Permission denied."));
    return;
  }
  p = type ? &m->mflags : &m->nflags;
  *p = string_to_privs(malias_priv_table, privs, 0);
  notify_format(player,
                T("MAIL: Permission to see/use alias '%s' changed to %s"),
                alias, privs_to_string(malias_priv_table, *p));
}


/** Add players to a malias.
 * \param player dbref of enactor.
 * \param alias name of malias.
 * \param tolist string with list of players to add.
 */
void
do_malias_add(dbref player, char *alias, char *tolist)
{
  char *head, *tail, spot;
  struct mail_alias *m;
  const char *buff;
  int i = 0;
  dbref target;
  dbref alist[100];
  dbref *members;

  m = get_malias(player, alias);
  if (!m) {
    notify_format(player, T("MAIL: Mail Alias '%s' not found."), alias);
    return;
  }
  if (!Wizard(player) && (m->owner != player)) {
    notify(player, T("Permission denied."));
    return;
  }
  i = 0;

  /*
   * Parse the player list
   */
  head = (char *) tolist;
  while (head && *head) {
    while (*head == ' ')
      head++;
    tail = head;
    while (*tail && (*tail != ' ')) {
      if (*tail == '"') {
        head++;
        tail++;
        while (*tail && (*tail != '"'))
          tail++;
      }
      if (*tail)
        tail++;
    }
    tail--;
    if (*tail != '"')
      tail++;
    spot = *tail;
    *tail = '\0';
    /*
     * Now locate a target
     */
    if (!strcasecmp(head, "me"))
      target = player;
    else if (*head == '#') {
      target = atoi(head + 1);
    } else
      target = lookup_player(head);
    if (!(GoodObject(target)) || (!IsPlayer(target))) {
      notify_format(player, T("MAIL: No such player '%s'."), head);
    } else {
      if (ismember(m, target)) {
        notify_format(player,
                      T("MAIL: player '%s' exists already in alias %s."),
                      head, alias);
      } else {
        buff = unparse_object(player, target);
        notify_format(player, T("MAIL: %s added to alias %s"), buff, alias);
        alist[i] = target;
        i++;
      }
    }
    /*
     * Get the next recip
     */
    *tail = spot;
    head = tail;
    if (*head == '"')
      head++;
    if (i == 100)
      break;
  }

  if (head && *head) {
    notify(player, T("MAIL: Alias list is restricted to maximal 100 entries!"));
  }
  if (!i) {
    notify(player, T("MAIL: No valid recipients for alias-list!"));
    return;
  }
  members = mush_calloc(i + m->size, sizeof(dbref), "malias_members");

  memcpy(members, m->members, sizeof(dbref) * m->size);
  memcpy(&members[m->size], alist, sizeof(dbref) * i);
  mush_free(m->members, "malias_members");
  m->members = members;

  m->size += i;

  notify_format(player, T("MAIL: Alias set '%s' redefined."), alias);
}





/** Remove players from a malias.
 * \param player dbref of enactor.
 * \param alias name of malias.
 * \param tolist string with list of players to remove.
 */
void
do_malias_remove(dbref player, char *alias, char *tolist)
{
  char *head, *tail, spot;
  struct mail_alias *m;
  const char *buff;
  int i = 0;
  dbref target;

  m = get_malias(player, alias);
  if (!m) {
    notify_format(player, T("MAIL: Mail Alias '%s' not found."), alias);
    return;
  }
  if (!Wizard(player) && (m->owner != player)) {
    notify(player, T("Permission denied."));
    return;
  }

  /*
   * Parse the player list
   */
  head = (char *) tolist;
  while (head && *head) {
    while (*head == ' ')
      head++;
    tail = head;
    while (*tail && (*tail != ' ')) {
      if (*tail == '"') {
        head++;
        tail++;
        while (*tail && (*tail != '"'))
          tail++;
      }
      if (*tail)
        tail++;
    }
    tail--;
    if (*tail != '"')
      tail++;
    spot = *tail;
    *tail = '\0';
    /*
     * Now locate a target
     */
    if (!strcasecmp(head, "me"))
      target = player;
    else if (*head == '#') {
      target = atoi(head + 1);
    } else
      target = lookup_player(head);
    if (!(GoodObject(target)) || (!IsPlayer(target))) {
      notify_format(player, T("MAIL: No such player '%s'."), head);
    } else {
      if (!(i = ismember(m, target))) {
        notify_format(player, T("MAIL: player '%s' is not in alias %s."),
                      head, alias);
      } else {
        buff = unparse_object(player, target);
        m->members[i - 1] = m->members[--m->size];
        notify_format(player, T("MAIL: %s removed from alias %s"), buff, alias);
      }
    }
    /*
     * Get the next recip
     */
    *tail = spot;
    head = tail;
    if (*head == '"')
      head++;
  }

  notify_format(player, T("MAIL: Alias set '%s' redefined."), alias);
}






/***********************************************************
***** "Utility" functions *****
***********************************************************/

static const char *
get_shortprivs(struct mail_alias *m)
{
  static char privs[10];
  strcpy(privs, "--  -- ");

  if (!m->nflags)
    privs[0] = 'E';
  else {
    if (m->nflags & ALIAS_MEMBERS)
      privs[0] = 'M';
    if (m->nflags & ALIAS_ADMIN)
      privs[1] = 'A';
    if (!strncmp(privs, "--", 2))
      privs[1] = 'O';
  }

  if (!m->mflags)
    privs[4] = 'E';
  else {
    if (m->mflags & ALIAS_MEMBERS)
      privs[4] = 'M';
    if (m->mflags & ALIAS_ADMIN)
      privs[5] = 'A';
    if (!strncmp(privs + 4, "--", 2))
      privs[5] = 'O';
  }

  return privs;
}


/** Is a player a member of a malias?
 * \param m pointer to malias.
 * \param player dbref of player.
 * \retval 1 player is a member of the malias.
 * \retval 0 player is not a member of the malias.
 */
int
ismember(struct mail_alias *m, dbref player)
{
  int i;
  for (i = 0; i < m->size; i++) {
    if (player == m->members[i])
      return (i + 1);           /* To avoid entry "0" */
  }
  return 0;
}

/** Remove a destroyed player from all maliases.
 * \param player player to remove from maliases.
 */
void
malias_cleanup(dbref player)
{
  struct mail_alias *m;
  int n, i = 0;

  for (n = 0; n < ma_top; n++) {
    m = &malias[n];
    if ((i = ismember(m, player)) != 0) {
      do_rawlog(LT_ERR, "Removing #%d from malias %s", player, m->name);
      m->members[i - 1] = m->members[--m->size];
    }
  }
}

/** Get a malias pointer with permission checking.
 * \param player player dbref, for permission check.
 * \param alias name of malias to retrieve.
 * \return pointer to malias structure, or NULL if player can't see it.
 */
struct mail_alias *
get_malias(dbref player, char *alias)
{
  const char *mal;
  struct mail_alias *m;
  int i = 0;

  if (*alias != MALIAS_TOKEN)
    return NULL;

  mal = alias + 1;

  for (i = 0; i < ma_top; i++) {
    m = &malias[i];
    if ((m->owner == player) || (m->nflags == 0) ||
        /* ((m->nflags & ALIAS_ADMIN) && Hasprivs(player)) || */
        Hasprivs(player) ||
        ((m->nflags & ALIAS_MEMBERS) && ismember(m, player))) {

      if (!strcasecmp(mal, m->name))
        return m;
    }
  }
  return NULL;
}





/***********************************************************
***** Loading and saving of mail-aliases *****
***********************************************************/

/** Load maliases from the mail db.
 * \param fp file pointer to read from.
 */
void
load_malias(PENNFILE *fp)
{
  int i, j;
  char buffer[BUFFER_LEN];
  struct mail_alias *m;
  char *s;

  ma_top = getref(fp);

  ma_size = ma_top;

  if (ma_top > 0)
    malias = mush_calloc(ma_size, sizeof(struct mail_alias), "malias_list");
  else
    malias = NULL;

  for (i = 0; i < ma_top; i++) {
    m = &malias[i];

    m->owner = getref(fp);
    m->name = mush_strdup(getstring_noalloc(fp), "malias_name");
    m->desc = compress(getstring_noalloc(fp));
    add_check("malias_desc");

    m->nflags = getref(fp);
    m->mflags = getref(fp);
    m->size = getref(fp);

    if (m->size > 0) {
      m->members = mush_calloc(m->size, sizeof(dbref), "malias_members");
      for (j = 0; j < m->size; j++) {
        m->members[j] = getref(fp);
      }
    } else {
      m->members = NULL;
    }
  }
  s = penn_fgets(buffer, sizeof(buffer), fp);

  if (!s || strcmp(buffer, "\"*** End of MALIAS ***\"\n") != 0) {
    do_rawlog(LT_ERR, "MAIL: Error reading MALIAS list");
  }
}

/** Write maliases to the maildb
 * \param fp file pointer to write to.
 */
void
save_malias(PENNFILE *fp)
{
  int i, j;
  struct mail_alias *m;

  putref(fp, ma_top);

  for (i = 0; i < ma_top; i++) {
    m = &malias[i];
    putref(fp, m->owner);
    putstring(fp, (char *) (m->name));
    putstring(fp, uncompress(m->desc));

    putref(fp, m->nflags);
    putref(fp, m->mflags);
    putref(fp, m->size);

    for (j = 0; j < m->size; j++)
      putref(fp, m->members[j]);
  }
  putstring(fp, "*** End of MALIAS ***");
}
