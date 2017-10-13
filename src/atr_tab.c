/**
 * \file atr_tab.c
 *
 * \brief The table of standard attributes and code to manipulate it.
 *
 *
 */

#include "atr_tab.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ansi.h"
#include "attrib.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "log.h"
#include "mymalloc.h"
#include "notify.h"
#include "parse.h"
#include "privtab.h"
#include "ptab.h"
#include "strutil.h"

extern const unsigned char *tables;

/** Prefix table for standard attribute names */
PTAB ptab_attrib;

/** Attribute flags for setting */
PRIV attr_privs_set[] = {{"no_command", '$', AF_NOPROG, AF_NOPROG},
                         {"no_inherit", 'i', AF_PRIVATE, AF_PRIVATE},
                         {"private", 'i', AF_PRIVATE, AF_PRIVATE},
                         {"no_clone", 'c', AF_NOCOPY, AF_NOCOPY},
                         {"wizard", 'w', AF_WIZARD, AF_WIZARD},
                         {"visual", 'v', AF_VISUAL, AF_VISUAL},
                         {"mortal_dark", 'm', AF_MDARK, AF_MDARK},
                         {"hidden", 'm', AF_MDARK, AF_MDARK},
                         {"regexp", 'R', AF_REGEXP, AF_REGEXP},
                         {"case", 'C', AF_CASE, AF_CASE},
                         {"locked", '+', AF_LOCKED, AF_LOCKED},
                         {"safe", 'S', AF_SAFE, AF_SAFE},
                         {"prefixmatch", '\0', AF_PREFIXMATCH, AF_PREFIXMATCH},
                         {"veiled", 'V', AF_VEILED, AF_VEILED},
                         {"debug", 'b', AF_DEBUG, AF_DEBUG},
                         {"no_debug", 'B', AF_NODEBUG, AF_NODEBUG},
                         {"public", 'p', AF_PUBLIC, AF_PUBLIC},
                         {"nearby", 'n', AF_NEARBY, AF_NEARBY},
                         {"noname", 'N', AF_NONAME, AF_NONAME},
                         {"no_name", 'N', AF_NONAME, AF_NONAME},
                         {"nospace", 's', AF_NOSPACE, AF_NOSPACE},
                         {"no_space", 's', AF_NOSPACE, AF_NOSPACE},
                         {"amhear", 'M', AF_MHEAR, AF_MHEAR},
                         {"aahear", 'A', AF_AHEAR, AF_AHEAR},
                         {"quiet", 'Q', AF_QUIET, AF_QUIET},
                         {"branch", '`', 0, 0},
                         {NULL, '\0', 0, 0}};

/** Attribute flags which may be present in the db */
PRIV attr_privs_db[] = {{"no_command", '$', AF_NOPROG, AF_NOPROG},
                        {"no_inherit", 'i', AF_PRIVATE, AF_PRIVATE},
                        {"no_clone", 'c', AF_NOCOPY, AF_NOCOPY},
                        {"wizard", 'w', AF_WIZARD, AF_WIZARD},
                        {"visual", 'v', AF_VISUAL, AF_VISUAL},
                        {"mortal_dark", 'm', AF_MDARK, AF_MDARK},
                        {"regexp", 'R', AF_REGEXP, AF_REGEXP},
                        {"case", 'C', AF_CASE, AF_CASE},
                        {"locked", '+', AF_LOCKED, AF_LOCKED},
                        {"safe", 'S', AF_SAFE, AF_SAFE},
                        {"prefixmatch", '\0', AF_PREFIXMATCH, AF_PREFIXMATCH},
                        {"veiled", 'V', AF_VEILED, AF_VEILED},
                        {"debug", 'b', AF_DEBUG, AF_DEBUG},
                        {"no_debug", 'B', AF_NODEBUG, AF_NODEBUG},
                        {"public", 'p', AF_PUBLIC, AF_PUBLIC},
                        {"nearby", 'n', AF_NEARBY, AF_NEARBY},
                        {"noname", 'N', AF_NONAME, AF_NONAME},
                        {"nospace", 's', AF_NOSPACE, AF_NOSPACE},
                        {"amhear", 'M', AF_MHEAR, AF_MHEAR},
                        {"aahear", 'A', AF_AHEAR, AF_AHEAR},
                        {"enum", '\0', AF_ENUM, AF_ENUM},
                        {"limit", '\0', AF_RLIMIT, AF_RLIMIT},
                        {"internal", '\0', AF_INTERNAL, AF_INTERNAL},
                        {"quiet", 'Q', AF_QUIET, AF_QUIET},
                        {NULL, '\0', 0, 0}};

/** Attribute flags for viewing */
PRIV attr_privs_view[] = {{"no_command", '$', AF_NOPROG, AF_NOPROG},
                          {"no_inherit", 'i', AF_PRIVATE, AF_PRIVATE},
                          {"private", 'i', AF_PRIVATE, AF_PRIVATE},
                          {"no_clone", 'c', AF_NOCOPY, AF_NOCOPY},
                          {"wizard", 'w', AF_WIZARD, AF_WIZARD},
                          {"visual", 'v', AF_VISUAL, AF_VISUAL},
                          {"mortal_dark", 'm', AF_MDARK, AF_MDARK},
                          {"hidden", 'm', AF_MDARK, AF_MDARK},
                          {"regexp", 'R', AF_REGEXP, AF_REGEXP},
                          {"case", 'C', AF_CASE, AF_CASE},
                          {"locked", '+', AF_LOCKED, AF_LOCKED},
                          {"safe", 'S', AF_SAFE, AF_SAFE},
                          {"internal", '\0', AF_INTERNAL, AF_INTERNAL},
                          {"prefixmatch", '\0', AF_PREFIXMATCH, AF_PREFIXMATCH},
                          {"veiled", 'V', AF_VEILED, AF_VEILED},
                          {"debug", 'b', AF_DEBUG, AF_DEBUG},
                          {"no_debug", 'B', AF_NODEBUG, AF_NODEBUG},
                          {"public", 'p', AF_PUBLIC, AF_PUBLIC},
                          {"nearby", 'n', AF_NEARBY, AF_NEARBY},
                          {"noname", 'N', AF_NONAME, AF_NONAME},
                          {"no_name", 'N', AF_NONAME, AF_NONAME},
                          {"nospace", 's', AF_NOSPACE, AF_NOSPACE},
                          {"no_space", 's', AF_NOSPACE, AF_NOSPACE},
                          {"amhear", 'M', AF_MHEAR, AF_MHEAR},
                          {"aahear", 'A', AF_AHEAR, AF_AHEAR},
                          {"quiet", 'Q', AF_QUIET, AF_QUIET},
                          {"branch", '`', AF_ROOT, AF_ROOT},
                          {NULL, '\0', 0, 0}};

/*----------------------------------------------------------------------
 * Prefix-table functions of various sorts
 */

static ATTR *aname_find_exact(const char *name);
static ATTR *attr_read(PENNFILE *f);
static ATTR *attr_alias_read(PENNFILE *f, char *alias);

static int free_standard_attr(ATTR *a, bool inserted);
static int free_standard_attr_aliases(ATTR *a);
static void display_attr_info(dbref player, ATTR *ap);

const char *display_attr_limit(ATTR *ap);

void init_aname_table(void);

/** Attribute table lookup by name or alias.
 * given an attribute name, look it up in the complete attribute table
 * (real names plus aliases), and return the appropriate real attribute.
 */
ATTR *
aname_hash_lookup(const char *name)
{
  ATTR *ap;
  /* Exact matches always work */
  if ((ap = (ATTR *) ptab_find_exact(&ptab_attrib, name)))
    return ap;
  /* Prefix matches work if the attribute is AF_PREFIXMATCH */
  if ((ap = (ATTR *) ptab_find(&ptab_attrib, name)) && AF_Prefixmatch(ap))
    return ap;
  return NULL;
}

/** Build the basic attribute table.
 */
void
init_aname_table(void)
{
  ATTR *ap;
  ATRALIAS *aap;

  ptab_init(&ptab_attrib);
  ptab_start_inserts(&ptab_attrib);
  for (ap = attr; ap->name; ap++)
    ptab_insert(&ptab_attrib, ap->name, ap);
  ptab_end_inserts(&ptab_attrib);

  for (aap = attralias; aap->alias; aap++) {
    alias_attribute(aap->realname, aap->alias);
  }
}

/** Free all memory used by a standard attribute, and remove it from the hash
 * table if necessary.
 * \param a attr to remove
 * \param inserted has the attr been inserted into the hash table already?
 * \retval number of entries (including aliases) removed from the hash table
 */
static int
free_standard_attr(ATTR *a, bool inserted)
{
  int count = 0;
  if (!a) {
    return count;
  }

  /* If the attr has no name, there's no way it can be in the hash table */
  if (AL_NAME(a)) {
    if (inserted) {
      count = free_standard_attr_aliases(a) + 1;
      ptab_delete(&ptab_attrib, AL_NAME(a));
    }
    free((char *) AL_NAME(a));
  }

  if (a->data != NULL_CHUNK_REFERENCE) {
    chunk_delete(a->data);
  }

  mush_free(a, "ATTR");

  return count;
}

/* Remove all aliases for a standard attr. */
static int
free_standard_attr_aliases(ATTR *a)
{
  bool found_alias;
  ATTR *curr;
  const char *aliasname;
  int count = 0;

  /* Annoyingly convoluted because the ptab_delete will screw with the
   * counter used by ptab_nextextry_new */
  do {
    found_alias = 0;
    curr = ptab_firstentry_new(&ptab_attrib, &aliasname);
    for (; curr; curr = ptab_nextentry_new(&ptab_attrib, &aliasname)) {
      if (!strcmp(AL_NAME(curr), AL_NAME(a)) &&
          strcmp(AL_NAME(curr), aliasname)) {
        found_alias = 1;
        ptab_delete(&ptab_attrib, aliasname);
        count++;
        break;
      }
    }
  } while (found_alias);

  return count;
}

static ATTR *
attr_read(PENNFILE *f)
{
  ATTR *a;
  char *tmp;
  dbref d = GOD;
  privbits flags = 0;

  a = (ATTR *) mush_malloc(sizeof(ATTR), "ATTR");
  if (!a) {
    mush_panic("Not enough memory to add attribute in attr_read()!");
    return NULL;
  }
  AL_NAME(a) = NULL;
  a->data = NULL_CHUNK_REFERENCE;
  AL_FLAGS(a) = 0;
  AL_CREATOR(a) = GOD;
  a->next = NULL;

  db_read_this_labeled_string(f, "name", &tmp);

  if (!good_atr_name(tmp)) {
    do_rawlog(LT_ERR, "Invalid attribute name '%s' in db.", tmp);
    (void) getstring_noalloc(f); /* flags */
    (void) getstring_noalloc(f); /* creator */
    (void) getstring_noalloc(f); /* data */
    free_standard_attr(a, 0);
    return NULL;
  }

  AL_NAME(a) = strdup(tmp);
  db_read_this_labeled_string(f, "flags", &tmp);
  if (tmp && *tmp && strcasecmp(tmp, "none")) {
    flags = list_to_privs(attr_privs_db, tmp, 0);
    if (!flags) {
      do_rawlog(LT_ERR, "Invalid attribute flags for '%s' in db.", AL_NAME(a));
      free((char *) AL_NAME(a));
      (void) getstring_noalloc(f); /* creator */
      (void) getstring_noalloc(f); /* data */
      free_standard_attr(a, 0);
      return NULL;
    }
  }
  AL_FLAGS(a) = flags;

  db_read_this_labeled_dbref(f, "creator", &d);
  AL_CREATOR(a) = d;

  db_read_this_labeled_string(f, "data", &tmp);
  if (!tmp || !*tmp || !(AL_FLAGS(a) & (AF_ENUM | AF_RLIMIT))) {
    a->data = NULL_CHUNK_REFERENCE;
  } else if (AL_FLAGS(a) & AF_ENUM) {
    /* Store string as it is */
    char *t = compress(tmp);
    a->data = chunk_create(t, strlen(t), 0);
    free(t);
  } else if (AL_FLAGS(a) & AF_RLIMIT) {
    /* Need to validate regexp */
    char *t;
    pcre *re;
    const char *errptr;
    int erroffset;

    re = pcre_compile(tmp, PCRE_CASELESS, &errptr, &erroffset, tables);
    if (!re) {
      do_rawlog(LT_ERR, "Invalid regexp in limit for attribute '%s' in db.",
                AL_NAME(a));
      free_standard_attr(a, 0);
      return NULL;
    }
    pcre_free(re); /* don't need it, just needed to check it */

    t = compress(tmp);
    a->data = chunk_create(t, strlen(t), 0);
    free(t);
  }

  return a;
}

static ATTR *
attr_alias_read(PENNFILE *f, char *alias)
{
  char *tmp;
  ATTR *a;

  db_read_this_labeled_string(f, "name", &tmp);
  a = aname_find_exact(tmp);
  if (!a) {
    /* Oops */
    do_rawlog(LT_ERR, "Alias of non-existant attribute '%s' in db.", tmp);
    (void) getstring_noalloc(f);
    return NULL;
  }
  db_read_this_labeled_string(f, "alias", &tmp);
  strcpy(alias, tmp);

  return a;
}

void
attr_read_all(PENNFILE *f)
{
  ATTR *a;
  int c, found, count = 0;
  char alias[BUFFER_LEN];

  /* Clear existing attributes */
  ptab_free(&ptab_attrib);

  ptab_start_inserts(&ptab_attrib);

  db_read_this_labeled_int(f, "attrcount", &count);
  for (found = 0;;) {

    c = penn_fgetc(f);
    penn_ungetc(c, f);

    if (c != ' ')
      break;

    found++;

    if ((a = attr_read(f)))
      ptab_insert(&ptab_attrib, a->name, a);
  }

  ptab_end_inserts(&ptab_attrib);

  if (found != count)
    do_rawlog(LT_ERR, "WARNING: Actual number of attrs (%d) different than "
                      "expected count (%d).",
              found, count);

  /* Assumes we'll always have at least one alias */
  db_read_this_labeled_int(f, "attraliascount", &count);
  for (found = 0;;) {
    c = penn_fgetc(f);
    penn_ungetc(c, f);

    if (c != ' ')
      break;

    found++;

    if ((a = attr_alias_read(f, alias))) {
      upcasestr(alias);
      if (!good_atr_name(alias)) {
        do_rawlog(LT_ERR, "Bad attribute name on alias '%s' in db.", alias);
      } else if (aname_find_exact(strupper(alias))) {
        do_rawlog(
          LT_ERR,
          "Unable to alias attribute '%s' to '%s' in db: alias already in use.",
          AL_NAME(a), alias);
      } else if (!alias_attribute(AL_NAME(a), alias)) {
        do_rawlog(LT_ERR, "Unable to alias attribute '%s' to '%s' in db.",
                  AL_NAME(a), alias);
      }
    }
  }
  if (found != count)
    do_rawlog(LT_ERR, "WARNING: Actual number of attr aliases (%d) different "
                      "than expected count (%d).",
              found, count);

  return;
}

void
attr_write_all(PENNFILE *f)
{
  int attrcount = 0, aliascount = 0;
  ATTR *a;
  const char *attrname;
  char *data;

  for (a = ptab_firstentry_new(&ptab_attrib, &attrname); a;
       a = ptab_nextentry_new(&ptab_attrib, &attrname)) {
    if (!strcmp(attrname, AL_NAME(a)))
      attrcount++;
    else
      aliascount++;
  }

  db_write_labeled_int(f, "attrcount", attrcount);
  for (a = ptab_firstentry_new(&ptab_attrib, &attrname); a;
       a = ptab_nextentry_new(&ptab_attrib, &attrname)) {
    if (strcmp(attrname, AL_NAME(a)))
      continue; /* skip aliases */
    db_write_labeled_string(f, " name", AL_NAME(a));
    db_write_labeled_string(f, "  flags",
                            privs_to_string(attr_privs_db, AL_FLAGS(a)));
    db_write_labeled_dbref(f, "  creator", AL_CREATOR(a));
    data = atr_value(a);
    db_write_labeled_string(f, "  data", data);
  }

  db_write_labeled_int(f, "attraliascount", aliascount);
  for (a = ptab_firstentry_new(&ptab_attrib, &attrname); a;
       a = ptab_nextentry_new(&ptab_attrib, &attrname)) {
    if (!strcmp(attrname, AL_NAME(a)))
      continue; /* skip non-aliases */
    db_write_labeled_string(f, " name", AL_NAME(a));
    db_write_labeled_string(f, "  alias", attrname);
  }
}

/** Associate a new alias with an existing attribute.
 */
int
alias_attribute(const char *atr, const char *alias)
{
  ATTR *ap;

  /* Make sure the alias doesn't exist already */
  if (aname_find_exact(alias))
    return 0;

  /* Look up the original */
  ap = aname_find_exact(atr);
  if (!ap)
    return 0;

  ptab_insert_one(&ptab_attrib, strupper(alias), ap);
  return 1;
}

static ATTR *
aname_find_exact(const char *name)
{
  char atrname[BUFFER_LEN];
  strcpy(atrname, name);
  upcasestr(atrname);
  return (ATTR *) ptab_find_exact(&ptab_attrib, atrname);
}

/* Add a new, or restrict an existing, standard attribute from cnf file */
int
cnf_attribute_access(char *attrname, char *opts)
{
  ATTR *a;
  privbits flags = 0;

  upcasestr(attrname);
  if (!good_atr_name(attrname))
    return 0;

  if (strcasecmp(opts, "none")) {
    flags = list_to_privs(attr_privs_set, opts, 0);
    if (!flags)
      return 0;
  }

  a = (ATTR *) ptab_find_exact(&ptab_attrib, attrname);
  if (a) {
    if (AF_Internal(a))
      return 0;
  } else {
    a = (ATTR *) mush_malloc(sizeof(ATTR), "ATTR");
    if (!a)
      return 0;
    AL_NAME(a) = strdup(attrname);
    a->data = NULL_CHUNK_REFERENCE;
    ptab_insert_one(&ptab_attrib, attrname, a);
  }
  AL_FLAGS(a) = flags;
  AL_CREATOR(a) = GOD;
  return 1;
}

/** Since enum adds a delim before and after the string, edit them out. */
const char *
display_attr_limit(ATTR *ap)
{
  char *ptr;
  char *s;

  if (ap->data && (ap->flags & AF_ENUM)) {
    ptr = atr_value(ap);
    *(ptr++) = '\0';
    s = ptr + strlen(ptr);
    s--;
    *(s) = '\0';
    return ptr;
  } else if (ap->data && (ap->flags & AF_RLIMIT)) {
    return atr_value(ap);
  }
  return "unset";
}

/** Check an attribute's value against /limit or /enum restrictions.
 * \param player Player to send error message to, or NOTHING to skip
 * \param name the attribute name.
 * \param value The desired attribute value.
 * \retval The new value to set if valid, NULL if not.
 */
const char *
check_attr_value(dbref player, const char *name, const char *value)
{
  /* Check for attribute limits and enums. */
  ATTR *ap;
  char *attrval;
  pcre *re;
  int subpatterns;
  const char *errptr;
  int erroffset;
  char *ptr, *ptr2;
  char delim;
  int len;
  static char buff[BUFFER_LEN];
  char vbuff[BUFFER_LEN];

  if (!name || !*name)
    return value;
  if (!value)
    return value;

  upcasestr((char *) name);
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, name);
  if (!ap)
    return value;

  attrval = atr_value(ap);
  if (!attrval) {
    return value;
  }

  if (ap->flags & AF_RLIMIT) {
    re = pcre_compile(remove_markup(attrval, NULL), PCRE_CASELESS, &errptr,
                      &erroffset, tables);
    if (!re)
      return value;

    subpatterns =
      pcre_exec(re, default_match_limit(), value, strlen(value), 0, 0, NULL, 0);
    pcre_free(re);

    if (subpatterns >= 0) {
      return value;
    } else {
      if (player != NOTHING)
        notify(player, T("Attribute value does not match the /limit regexp."));
      return NULL;
    }
  } else if (ap->flags & AF_ENUM) {
    /* Delimiter is always the first character of the enum string.
     * and the value cannot have the delimiter in it. */
    delim = *attrval;
    if (!*value || strchr(value, delim)) {
      if (player != NOTHING)
        notify_format(player, T("Value for %s needs to be one of: %s"),
                      ap->name, display_attr_limit(ap));
      return NULL;
    }

    /* We match the enum case-insensitively, BUT we use the case
     * that is defined in the enum, so we copy the attr value
     * to buff and use that. */
    snprintf(buff, BUFFER_LEN, "%s", attrval);
    upcasestr(buff);

    len = strlen(value);
    snprintf(vbuff, BUFFER_LEN, "%c%s%c", delim, value, delim);
    upcasestr(vbuff);

    ptr = strstr(buff, vbuff);
    if (!ptr) {
      *(vbuff + len + 1) = '\0'; /* Remove the second delim */
      ptr = strstr(buff, vbuff);
    }

    /* Do we have a match? */
    if (ptr) {
      /* ptr is pointing at the delim before the value. */
      ptr++;
      ptr2 = strchr(ptr, delim);
      if (!ptr2)
        return NULL; /* Shouldn't happen, but sanity check. */

      /* Now we need to copy over the _original case_ version of the
       * enumerated string. Nasty pointer arithmetic. */
      strncpy(buff, attrval + (ptr - buff), (int) (ptr2 - ptr));
      buff[ptr2 - ptr] = '\0';
      return buff;
    } else {
      if (player != NOTHING)
        notify_format(player, T("Value for %s needs to be one of: %s"),
                      ap->name, display_attr_limit(ap));
      return NULL;
    }
  }
  return value;
}

/** Limit an attribute's possible values, using either an enum or a
 *  regexp /limit.
 * \verbatim
 * Given a name, restriction type and string for an attribute,
 * set its data value to said data and set a flag for limit or
 * enum.
 *
 * For an enum, the attr's data will be set to
 * <delim><pattern><delim>, so a simple strstr() can be used when
 * matching the pattern.
 *
 * An optional delimiter can be provided on the left hand side by using
 * @attr/enum <delim> <attrname>=<enum list>
 * \endverbatim
 * \param player the enactor.
 * \param name the attribute name.
 * \param type AF_RLIMIT for regexp, AF_ENUM for enum.
 * \param pattern The allowed pattern for the attribute.
 */
void
do_attribute_limit(dbref player, char *name, int type, char *pattern)
{
  ATTR *ap;
  char buff[BUFFER_LEN];
  char *ptr, *bp;
  char delim = ' ';
  pcre *re;
  const char *errptr;
  int erroffset;
  int unset = 0;

  if (pattern && *pattern) {
    if (type == AF_RLIMIT) {
      /* Compile to regexp. */
      re = pcre_compile(remove_markup(pattern, NULL), PCRE_CASELESS, &errptr,
                        &erroffset, tables);
      if (!re) {
        notify(player, T("Invalid Regular Expression."));
        return;
      }
      /* We only care if it's valid, we're not using it. */
      pcre_free(re);

      /* Copy it to buff to be placed into ap->data. */
      snprintf(buff, BUFFER_LEN, "%s", pattern);
    } else if (type == AF_ENUM) {
      ptr = name;
      /* Check for a delimiter: @attr/enum | attrname=foo */
      if ((name = strchr(ptr, ' ')) != NULL) {
        *(name++) = '\0';
        if (strlen(ptr) > 1) {
          notify(player, T("Delimiter must be one character."));
          return;
        }
        delim = *ptr;
      } else {
        name = ptr;
        delim = ' ';
      }

      /* For speed purposes, we require the pattern to begin and end with
       * a delimiter. */
      snprintf(buff, BUFFER_LEN, "%c%s%c", delim, pattern, delim);
      buff[BUFFER_LEN - 1] = '\0';

      /* For sanity's sake, we'll enforce a properly delimited enum
       * with a quick and dirty squish().
       * We already know we start with a delim, hence the +1 =). */
      for (ptr = buff + 1, bp = buff + 1; *ptr; ptr++) {
        if (!(*ptr == delim && *(ptr - 1) == delim)) {
          *(bp++) = *ptr;
        }
      }
      *bp = '\0';
    } else {
      /* Err, we got called with the wrong limit type? */
      notify(player, T("Unknown limit type?"));
      return;
    }
  } else {
    unset = 1;
  }

  /* Parse name and perms */
  if (!name || !*name) {
    notify(player, T("Which attribute do you mean?"));
    return;
  }
  upcasestr(name);
  if (*name == '@')
    name++;

  /* Is this attribute already in the table? */
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, name);

  if (!ap) {
    notify(player, T("I don't know that attribute. Please use "
                     "@attribute/access to create it, first."));
    return;
  }

  if (AF_Internal(ap)) {
    /* Don't muck with internal attributes */
    notify(player, T("That attribute's permissions cannot be changed."));
    return;
  }

  /* All's good, set the data and the AF_RLIMIT or AF_ENUM flag. */
  if (ap->data != NULL_CHUNK_REFERENCE) {
    chunk_delete(ap->data);
  }
  /* Clear any extant rlimit or enum flags */
  ap->flags &= ~(AF_RLIMIT | AF_ENUM);
  if (unset) {
    if (ap->data != NULL_CHUNK_REFERENCE) {
      ap->data = NULL_CHUNK_REFERENCE;
      notify_format(player, T("%s -- Attribute limit or enum unset."), name);
    } else {
      notify_format(player, T("%s -- Attribute limit or enum already unset."),
                    name);
    }
  } else {
    char *t = compress(buff);
    ap->data = chunk_create(t, strlen(t), 0);
    free(t);
    ap->flags |= type;
    notify_format(player, T("%s -- Attribute %s set to: %s"), name,
                  type == AF_RLIMIT ? "limit" : "enum", display_attr_limit(ap));
  }
}

/** Add new standard attributes, or change permissions on them.
 * \verbatim
 * Given the name and permission string for an attribute, add it to
 * the attribute table (or modify the permissions if it's already
 * there). Permissions may be changed retroactively, which modifies
 * permissions on any copies of that attribute set on objects in the
 * database. This is the top-level code for @attribute/access.
 * \endverbatim
 * \param player the enactor.
 * \param name the attribute name.
 * \param perms a string of attribute permissions, space-separated.
 * \param retroactive if true, apply the permissions retroactively.
 */
void
do_attribute_access(dbref player, char *name, char *perms, int retroactive)
{
  ATTR *ap, *ap2;
  privbits flags = 0;
  int i;
  int insert = 0;

  /* Parse name and perms */
  if (!name || !*name) {
    notify(player, T("Which attribute do you mean?"));
    return;
  }
  if (strcasecmp(perms, "none")) {
    flags = list_to_privs(attr_privs_set, perms, 0);
    if (!flags) {
      notify(player, T("I don't understand those permissions."));
      return;
    }
  }
  upcasestr(name);
  /* Is this attribute already in the table? */
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, name);
  if (ap) {
    if (AF_Internal(ap)) {
      /* Don't muck with internal attributes */
      notify(player, T("That attribute's permissions can not be changed."));
      return;
    }
    /* Preserve any existing @attribute/limit */
    flags |= (AL_FLAGS(ap) & (AF_RLIMIT | AF_ENUM));
  } else {
    /* Create fresh if the name is ok */
    if (!good_atr_name(name)) {
      notify(player, T("Invalid attribute name."));
      return;
    }
    insert = 1;
    ap = (ATTR *) mush_malloc(sizeof(ATTR), "ATTR");
    if (!ap) {
      notify(player, T("Critical memory failure - Alert God!"));
      do_log(LT_ERR, 0, 0, "do_attribute_access: unable to malloc ATTR");
      return;
    }
    AL_NAME(ap) = strdup(name);
    ap->data = NULL_CHUNK_REFERENCE;
  }
  AL_FLAGS(ap) = flags;
  AL_CREATOR(ap) = player;

  /* Only insert when it's not already in the table */
  if (insert) {
    ptab_insert_one(&ptab_attrib, name, ap);
  }

  /* Ok, now we need to see if there are any attributes of this name
   * set on objects in the db. If so, and if we're retroactive, set
   * perms/creator
   */
  if (retroactive) {
    for (i = 0; i < db_top; i++) {
      if ((ap2 = atr_get_noparent(i, name))) {
        if (AL_FLAGS(ap2) & AF_ROOT)
          AL_FLAGS(ap2) = flags | AF_ROOT;
        else
          AL_FLAGS(ap2) = flags;
        AL_CREATOR(ap2) = player;
      }
    }
  }

  notify_format(player, T("%s -- Attribute permissions now: %s"), name,
                privs_to_string(attr_privs_view, flags));
}

/** Add a new attribute. Called from db.c to add new attributes
 * to older databases which have their own attr table.
 * \param name name of attr to add
 * \param flags attribute flags (AF_*)
 */
void
add_new_attr(char *name, uint32_t flags)
{
  ATTR *ap;
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, name);
  if (ap || !good_atr_name(name))
    return;

  ap = (ATTR *) mush_malloc(sizeof(ATTR), "ATTR");
  if (!ap) {
    do_log(LT_ERR, 0, 0, "add_new_attr: unable to malloc ATTR");
    return;
  }
  AL_NAME(ap) = strdup(name);
  ap->data = NULL_CHUNK_REFERENCE;
  AL_FLAGS(ap) = flags;
  AL_CREATOR(ap) = 0;
  ptab_insert_one(&ptab_attrib, name, ap);
}

/** Delete an attribute from the attribute table.
 * \verbatim
 * Top-level function for @attrib/delete.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the attribute to delete.
 */
void
do_attribute_delete(dbref player, char *name)
{
  ATTR *ap;
  int count;

  if (!name || !*name) {
    notify(player, T("Which attribute do you mean?"));
    return;
  }

  /* Is this attribute in the table? */
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, name);
  if (!ap) {
    notify(player, T("That attribute isn't in the attribute table"));
    return;
  }

  /* Display current attr info, for backup/safety reasons */
  display_attr_info(player, ap);

  /* Free all data, remove any aliases, and remove from the hash table */
  count = free_standard_attr(ap, 1);

  switch (count) {
  case 0:
    notify_format(player, T("Failed to remove %s from attribute table."), name);
    break;
  case 1:
    notify_format(player, T("Removed %s from attribute table."), name);
    break;
  default:
    notify_format(player,
                  T("Removed %s and %d alias(es) from attribute table."), name,
                  count - 1);
    break;
  }

  return;
}

/** Rename an attribute in the attribute table.
 * \verbatim
 * Top-level function for @attrib/rename.
 * \endverbatim
 * \param player the enactor.
 * \param old the name of the attribute to rename.
 * \param newname the new name (surprise!)
 */
void
do_attribute_rename(dbref player, char *old, char *newname)
{
  ATTR *ap;
  if (!old || !*old || !newname || !*newname) {
    notify(player, T("Which attributes do you mean?"));
    return;
  }
  upcasestr(old);
  upcasestr(newname);
  /* Is the new name valid? */
  if (!good_atr_name(newname)) {
    notify(player, T("Invalid attribute name."));
    return;
  }
  /* Is the new name already in use? */
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, newname);
  if (ap) {
    notify_format(player,
                  T("The name %s is already used in the attribute table."),
                  newname);
    return;
  }
  /* Is the old name a real attribute? */
  ap = (ATTR *) ptab_find_exact(&ptab_attrib, old);
  if (!ap) {
    notify(player, T("That attribute isn't in the attribute table"));
    return;
  }
  /* Ok, take it out and put it back under the new name */
  ptab_delete(&ptab_attrib, old);
  /*  This causes a slight memory leak if you rename an attribute
     added via /access. But that doesn't happen often. Will fix
     someday.  */
  AL_NAME(ap) = strdup(newname);
  ptab_insert_one(&ptab_attrib, newname, ap);
  notify_format(player, T("Renamed %s to %s in attribute table."), old,
                newname);
  return;
}

/** Display information on an attribute from the table.
 * \verbatim
 * Top-level function for @attribute.
 * \endverbatim
 * \param player the enactor.
 * \param name the name of the attribute.
 */
void
do_attribute_info(dbref player, char *name)
{
  ATTR *ap;
  if (!name || !*name) {
    notify(player, T("Which attribute do you mean?"));
    return;
  }

  /* Is this attribute in the table? */

  if (*name == '@')
    name++;

  ap = aname_hash_lookup(name);
  if (!ap) {
    notify(player, T("That attribute isn't in the attribute table"));
    return;
  }

  display_attr_info(player, ap);
  return;
}

static void
display_attr_info(dbref player, ATTR *ap)
{

  notify_format(player, "%9s: %s", T("Attribute"), AL_NAME(ap));
  if (ap->flags & AF_RLIMIT) {
    notify_format(player, "%9s: %s", T("Limit"), display_attr_limit(ap));
  } else if (ap->flags & AF_ENUM) {
    notify_format(player, "%9s: %s", T("Enum"), display_attr_limit(ap));
  }
  notify_format(player, "%9s: %s", T("Flags"),
                privs_to_string(attr_privs_view, AL_FLAGS(ap)));
  notify_format(player, "%9s: %s", T("Creator"), unparse_dbref(AL_CREATOR(ap)));
  return;
}

/** Decompile the standard attribute table, as per \@attribute/decompile
 * \param player The enactor
 * \param pattern Wildcard pattern of attrnames to decompile
 * \param retroactive Include the /retroactive switch?
*/
void
do_decompile_attribs(dbref player, char *pattern, int retroactive)
{
  ATTR *ap;
  const char *name;

  notify(player, T("@@ Standard Attributes:"));
  for (ap = ptab_firstentry_new(&ptab_attrib, &name); ap;
       ap = ptab_nextentry_new(&ptab_attrib, &name)) {
    if (strcmp(name, AL_NAME(ap)))
      continue;
    if (pattern && *pattern && !quick_wild(pattern, AL_NAME(ap)))
      continue;
    notify_format(player, "@attribute/access%s %s=%s",
                  (retroactive ? "/retroactive" : ""), AL_NAME(ap),
                  privs_to_string(attr_privs_view, AL_FLAGS(ap)));
    if (ap->flags & AF_RLIMIT) {
      notify_format(player, "@attribute/limit %s=%s", AL_NAME(ap),
                    display_attr_limit(ap));
    } else if (ap->flags & AF_ENUM) {
      notify_format(player, "@attribute/enum %s=%s", AL_NAME(ap),
                    display_attr_limit(ap));
    }
  }
}

/** Display a list of standard attributes.
 * \verbatim
 * Top-level function for @list/attribs.
 * \endverbatim
 * \param player the enactor.
 * \param lc if true, display the list in lowercase; otherwise uppercase.
 */
void
do_list_attribs(dbref player, int lc)
{
  char *b = list_attribs();
  notify_format(player, T("Attribs: %s"), lc ? strlower(b) : b);
}

/** Return a list of standard attributes.
 * This functions returns the list of standard attributes, separated by
 * spaces, in a statically allocated buffer.
 */
char *
list_attribs(void)
{
  ATTR *ap;
  const char *ptrs[BUFFER_LEN / 2];
  static char buff[BUFFER_LEN];
  char *bp;
  const char *name;
  int nptrs = -1, i;

  for (ap = ptab_firstentry_new(&ptab_attrib, &name); ap;
       ap = ptab_nextentry_new(&ptab_attrib, &name)) {
    if (strcmp(name, AL_NAME(ap)))
      continue;
    ptrs[++nptrs] = AL_NAME(ap);
  }
  bp = buff;
  if (nptrs >= 0)
    safe_str(ptrs[0], buff, &bp);
  for (i = 1; i < nptrs; i++) {
    safe_chr(' ', buff, &bp);
    safe_str(ptrs[i], buff, &bp);
  }
  *bp = '\0';
  return buff;
}

/** Attr things to be done after the config file is loaded but before
 * objects are restarted.
 */
void
attr_init_postconfig(void)
{
  ATTR *a;
  /* read_remote_desc affects AF_NEARBY flag on DESCRIBE attribute */
  a = aname_hash_lookup("DESCRIBE");
  if (a) {
    if (READ_REMOTE_DESC)
      a->flags &= ~AF_NEARBY;
    else
      a->flags |= AF_NEARBY;
  }
  if (USE_MUXCOMM) {
    a = aname_hash_lookup("CHANALIAS");
    if (!a) {
      add_new_attr("CHANALIAS", AF_NOPROG);
    }
  }
}
