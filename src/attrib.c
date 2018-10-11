/**
 * \file attrib.c
 *
 * \brief Manipulate attributes on objects.
 *
 *
 */

#include "copyrite.h"
#include "attrib.h"

#include <string.h>
#include <ctype.h>

#include "chunk.h"
#include "conf.h"
#include "dbdefs.h"
#include "externs.h"
#include "flags.h"
#include "htab.h"
#include "lock.h"
#include "log.h"
#include "match.h"
#include "memcheck.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "notify.h"
#include "parse.h"
#include "privtab.h"
#include "sort.h"
#include "strtree.h"
#include "strutil.h"

#ifdef WIN32
#pragma warning(disable : 4761) /* disable warning re conversion */
#endif

/** A string tree of attribute names in use, to save us memory since
 * many are duplicated.
 */
StrTree atr_names;
/** Table of attribute flags. */
extern PRIV attr_privs_set[];
extern PRIV attr_privs_view[];

/** A string to hold the name of a missing prefix branch, set by
 * can_write_attr_internal.  Again, gross and ugly.  Please fix.
 */
static char missing_name[ATTRIBUTE_NAME_LIMIT + 1];

/*======================================================================*/

static int real_atr_clr(dbref thinking, char const *atr, dbref player,
                        int we_are_wiping);
static void atr_free_one(dbref, ATTR *);
static int find_atr_pos_in_list(dbref thing, char const *name);
static atr_err can_create_attr(dbref player, dbref obj, char const *atr_name,
                               uint32_t flags);
static ATTR *find_atr_in_list(dbref, char const *name);
static ATTR *atr_get_with_parent(dbref obj, char const *atrname, dbref *parent,
                                 int cmd);
static bool can_debug(dbref player, dbref victim);
static int atr_count_helper(dbref player, dbref thing, dbref parent,
                            char const *pattern, ATTR *atr, void *args);
static void set_cmd_flags(ATTR *a);

/*======================================================================*/

/** Initialize the attribute string tree.
 */
void
init_atr_name_tree(void)
{
  st_init(&atr_names, "AtrNameTree");
}

/** Lookup table for good_atr_name */
extern char atr_name_table[UCHAR_MAX + 1];

/** Decide if a name is valid for an attribute.
 * A good attribute name is at least one character long, no more than
 * ATTRIBUTE_NAME_LIMIT characters long, and every character is a
 * valid character. An attribute name may not start or end with a backtick.
 * An attribute name may not contain multiple consecutive backticks.
 * \param s a string to test for validity as an attribute name.
 */
int
good_atr_name(char const *s)
{
  const char *a;
  int len = 0;
  if (!s || !*s)
    return 0;
  if (*s == '`')
    return 0;
  for (a = s; *a; a++, len++) {
    if (!atr_name_table[*a]) {
      return 0;
    }
    if (*a == '`' && *(a + 1) == '`') {
      return 0;
    }
  }
  if (*(a - 1) == '`')
    return 0;
  return len <= ATTRIBUTE_NAME_LIMIT;
}

/** Find an attribute table entry, given a name.
 * A trivial wrapper around aname_hash_lookup.
 * \param string an attribute name.
 */
ATTR *
atr_match(const char *string)
{
  return aname_hash_lookup(string);
}

/** Find the first attribute branching off the specified attribute.
 * \param branch the attribute to look under
 */
ATTR *
atr_sub_branch(ATTR *branch)
{
  char const *name;
  size_t len;

  name = AL_NAME(branch);
  len = strlen(name);

  for (branch++; AL_NAME(branch); branch++) {
    const char *n2 = AL_NAME(branch);
    if (strlen(n2) <= len)
      return NULL;
    if (n2[len] == '`') {
      if (memcmp(n2, name, len) == 0) {
        return branch;
      } else {
        return NULL;
      }
    }
  }
  return NULL;
}

/** Find the attr immediately before the first child of 'branch'. This is
 *  not necessarily 'branch' itself.
 * \param branch the attr to look for children of
 * \return the attr immediately before branch's first child, or NULL
 *        if it has no children
 */
ATTR *
atr_sub_branch_prev(ATTR *branch)
{
  char const *name;
  size_t len;
  ATTR *prev;

  name = AL_NAME(branch);
  len = strlen(name);
  prev = branch;

  for (branch++; AL_NAME(branch); branch++) {
    const char *n2 = AL_NAME(branch);
    if (strlen(n2) <= len) {
      return NULL;
    }
    if (n2[len] == '`') {
      if (memcmp(n2, name, len) == 0) {
        return prev;
      } else {
        return NULL;
      }
    }
    prev = branch;
  }
  return NULL;
}

/** Test to see if an attribute name is the root of another
 *
 * \param root the string to test to see if it's a root
 * \param path the string to be tested.
 * \return true or false
 */
static bool
is_atree_root(const char *root, const char *path)
{
  size_t rootlen, pathlen;

  rootlen = strlen(root);
  pathlen = strlen(path);

  if (rootlen >= pathlen) {
    return 0;
  }

  if (path[rootlen] != '`') {
    return 0;
  }

  return memcmp(root, path, rootlen) == 0;
}

/** Convert a string of attribute flags to a bitmask.
 * Given a space-separated string of attribute flags, look them up
 * and return a bitmask of them if player is permitted to use
 * all of those flags.
 * \param player the dbref to use for privilege checks.
 * \param p a space-separated string of attribute flags.
 * \param bits pointer to a privbits to store the attribute flags mask.
 * \return 0 on success, -1 on error
 */
int
string_to_atrflag(dbref player, char const *p, privbits *bits)
{
  privbits f;
  f = string_to_privs(attr_privs_view, p, 0);
  if (!f)
    return -1;
  if (!Hasprivs(player) && (f & AF_MDARK))
    return -1;
  if (!See_All(player) && (f & AF_WIZARD))
    return -1;
  f &= ~AF_INTERNAL;
  *bits = f;
  return 0;
}

/** Convert a string of attribute flags to a pair of bitmasks.
 * Given a space-separated string of attribute flags, look them up
 * and return bitmasks of those to set or clear
 * if player is permitted to use all of those flags.
 * \param player the dbref to use for privilege checks.
 * \param p a space-separated string of attribute flags.
 * \param setbits pointer to address of bitmask to set.
 * \param clrbits pointer to address of bitmask to clear.
 * \return 0 on success or -1 on error.
 */
int
string_to_atrflagsets(dbref player, char const *p, privbits *setbits,
                      privbits *clrbits)
{
  int f;
  *setbits = *clrbits = 0;
  f = string_to_privsets(attr_privs_set, p, setbits, clrbits);
  if (f <= 0)
    return -1;
  if (!Hasprivs(player) && ((*setbits & AF_MDARK) || (*clrbits & AF_MDARK)))
    return -1;
  if (!See_All(player) && ((*setbits & AF_WIZARD) || (*clrbits & AF_WIZARD)))
    return -1;
  return 0;
}

/** Convert an attribute flag bitmask into a list of the full
 * names of the flags.
 * \param mask the bitmask of attribute flags to display.
 * \return a pointer to a static buffer with the full names of the flags.
 */
const char *
atrflag_to_string(privbits mask)
{
  return privs_to_string(attr_privs_view, mask);
}

/*======================================================================*/

/** Traversal routine for Can_Read_Attr.
 * This function determines if an attribute can be read by examining
 * the tree path to the attribute.  This is not the full Can_Read_Attr
 * check; only the stuff after See_All (just to avoid function calls
 * when the answer is trivialized by special powers).  If the specified
 * player is NOTHING, then we're doing a generic mortal visibility check.
 * \param player the player trying to do the read.
 * \param obj the object targetted for the read (may be a child of a parent!).
 * \param atr the attribute being interrogated.
 * \retval 0 if the player cannot read the attribute.
 * \retval 1 if the player can read the attribute.
 */
int
can_read_attr_internal(dbref player, dbref obj, ATTR *atr)
{
  static char name[ATTRIBUTE_NAME_LIMIT + 1];
  char *p;
  int cansee;
  int canlook;
  dbref target;
  dbref ancestor;
  int visible;
  int parent_depth;
  visible = (player == NOTHING);
  if (visible) {
    cansee = (Visual(obj) && eval_lock(PLAYER_START, obj, Examine_Lock) &&
              eval_lock(MASTER_ROOM, obj, Examine_Lock));
    canlook = 0;
  } else {
    cansee = controls(player, obj) ||
             (Visual(obj) && eval_lock(player, obj, Examine_Lock));
    canlook = can_look_at(player, obj);
  }

  /* Take an easy out if there is one... */
  /* If we can't see the attribute itself, then that's easy. */
  if (AF_Internal(atr) || AF_Mdark(atr) ||
      !(cansee || (AF_Visual(atr) && (!AF_Nearby(atr) || canlook)) ||
        (!visible && !Mistrust(player) &&
         (Owner(AL_CREATOR(atr)) == Owner(player)))))
    return 0;
  /* If the attribute isn't on a branch, then that's also easy. */
  if (!strchr(AL_NAME(atr), '`'))
    return 1;
  /* Nope, we actually have to go looking for the attribute in a tree. */
  strcpy(name, AL_NAME(atr));
  ancestor = Ancestor_Parent(obj);
  target = obj;
  parent_depth = 0;
  while (parent_depth < MAX_PARENTS && GoodObject(target)) {
    /* If the ancestor of the object is in its explict parent chain,
     * we use it there, and don't check the ancestor later.
     */
    if (target == ancestor)
      ancestor = NOTHING;
    /* Check along the branch for permissions... */
    for (p = strchr(name, '`'); p; p = strchr(p + 1, '`')) {
      *p = '\0';
      atr = find_atr_in_list(target, name);
      if (!atr || (target != obj && AF_Private(atr))) {
        *p = '`';
        goto continue_target;
      }
      if (AF_Internal(atr) || AF_Mdark(atr) ||
          !(cansee || (AF_Visual(atr) && (!AF_Nearby(atr) || canlook)) ||
            (!visible && !Mistrust(player) &&
             (Owner(AL_CREATOR(atr)) == Owner(player)))))
        return 0;
      *p = '`';
    }

    /* Now actually find the attribute. */
    atr = find_atr_in_list(target, name);
    if (atr)
      return 1;

  continue_target:

    /* Attribute wasn't on this object.  Check a parent or ancestor. */
    parent_depth++;
    target = Parent(target);
    if (!GoodObject(target)) {
      parent_depth = 0;
      target = ancestor;
    }
  }

  return 0;
}

/** Utility define for can_write_attr_internal and can_create_attr.
 * \param p the player trying to write
 * \param a the attribute to be written
 * \param s obey the safe flag?
 */
#define Cannot_Write_This_Attr(p, a, s)                                        \
  (!God((p)) &&                                                                \
   (AF_Internal((a)) || ((s) && AF_Safe((a))) ||                               \
    !(Wizard((p)) || (!AF_Wizard((a)) &&                                       \
                      (!AF_Locked((a)) || (AL_CREATOR((a)) == Owner((p))))))))

/** Traversal routine for Can_Write_Attr.
 * This function determines if an attribute can be written by examining
 * the tree path to the attribute.  As a side effect, missing_name is
 * set to the name of a missing prefix branch, if any.  Yes, side effects
 * are evil.  Please fix if you can.
 * \param player the player trying to do the write.
 * \param obj the object targetted for the write.
 * \param atr the attribute being interrogated.
 * \param safe whether to check the safe attribute flag.
 * \retval 0 if the player cannot write the attribute.
 * \retval 1 if the player can write the attribute.
 */
int
can_write_attr_internal(dbref player, dbref obj, ATTR *atr, int safe)
{
  char *p;
  missing_name[0] = '\0';
  if (Cannot_Write_This_Attr(player, atr, safe))
    return 0;
  strcpy(missing_name, AL_NAME(atr));
  for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
    *p = '\0';
    atr = find_atr_in_list(obj, missing_name);
    if (!atr)
      return 0;
    if (Cannot_Write_This_Attr(player, atr, safe)) {
      missing_name[0] = '\0';
      return 0;
    }
    *p = '`';
  }

  return 1;
}

/** If the attribute exists on the object, see if the player can modify it.
 * Otherwise, see if they can create it.
 * \param player the player trying to set the attr
 * \param thing the object to check
 * \param attrname the name of the attribute to check
 * \retval 0 attribute cannot be changed by player
 * \retval 1 attribute can be changed by player
 */
bool
can_edit_attr(dbref player, dbref thing, const char *attrname)
{
  ATTR *ptr = find_atr_in_list(thing, attrname);
  if (ptr)
    return Can_Write_Attr(player, thing, ptr);
  else
    return can_create_attr(player, thing, attrname, 0) == AE_OKAY;
}

/** Utility define for atr_add and can_create_attr */
#define set_default_flags(atr, flags)                                          \
  do {                                                                         \
    ATTR *std = atr_match(AL_NAME((atr)));                                     \
    if (std && !strcmp(AL_NAME(std), AL_NAME((atr)))) {                        \
      AL_FLAGS(atr) = AL_FLAGS(std) | flags;                                   \
    } else {                                                                   \
      AL_FLAGS(atr) = flags;                                                   \
    }                                                                          \
  } while (0)

/** Can an attribute of specified name be created?
 * This function determines if an attribute can be created by examining
 * the tree path to the attribute, and the standard attribute flags for
 * those parts of the path that don't exist yet.
 * \param player the player trying to do the write.
 * \param obj the object targetted for the write.
 * \param atr the attribute being interrogated.
 * \param flags the default flags to add to the attribute.
 * \retval 0 if the player cannot write the attribute.
 * \retval 1 if the player can write the attribute.
 */
static atr_err
can_create_attr(dbref player, dbref obj, char const *atr_name, uint32_t flags)
{
  char *p;
  ATTR tmpatr, *atr;
  int num_new = 1;
  missing_name[0] = '\0';

  atr = &tmpatr;
  AL_CREATOR(atr) = player;
  AL_NAME(atr) = atr_name;
  set_default_flags(atr, flags);
  if (Cannot_Write_This_Attr(player, atr, 1))
    return AE_ERROR;

  strcpy(missing_name, atr_name);
  atr = List(obj);
  for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
    *p = '\0';
    if (atr != &tmpatr)
      atr = find_atr_in_list(obj, missing_name);
    if (!atr) {
      atr = &tmpatr;
      AL_CREATOR(atr) = Owner(player);
    }
    if (atr == &tmpatr) {
      AL_NAME(atr) = missing_name;
      set_default_flags(atr, flags);
      num_new++;
    }
    /* Only GOD can create an AF_NODUMP attribute (used for semaphores)
     * or add a leaf to a tree with such an attribute
     */
    if ((AL_FLAGS(atr) & AF_NODUMP) && (player != GOD)) {
      missing_name[0] = '\0';
      return AE_ERROR;
    }
    if (Cannot_Write_This_Attr(player, atr, 1)) {
      missing_name[0] = '\0';
      return AE_ERROR;
    }
    *p = '`';
  }

  if ((AttrCount(obj) + num_new) >
      (Many_Attribs(obj) ? HARD_MAX_ATTRCOUNT : MAX_ATTRCOUNT)) {
    do_log(LT_ERR, player, obj,
           "Attempt by %s(%d) to create too many attributes on %s(%d)",
           Name(player), player, Name(obj), obj);
    return AE_TOOMANY;
  }

  return AE_OKAY;
}

/*======================================================================*/

#define GROWTH_FACTOR 1.5 /**< Amount to increase capacity when growing. */
#define SHRINK_FACTOR                                                          \
  2.0 /**< Shrink when ratio of count to capacity                              \
         is greater than this. */
#define LINEAR_CUT_OFF                                                         \
  32 /**< Switch to binary search when at least                                \
        this many attributes are on an                                         \
        object. Benchmarking shows binary is                                   \
        slower before this point. */

/** Comparison function to use with bsearch() on an attribute
    array. */
static int
atr_comp(const void *pa, const void *pb)
{
  const ATTR *a = pa;
  const ATTR *b = pb;
  return strcmp(AL_NAME(a), AL_NAME(b));
}

/** Search an attribute list for an attribute with the specified name.
 *
 * Attributes are stored as a sorted array. Use a linear search,
 * switching to binary when the attribute count gets above a certain
 * threshold. Always special case instances of 0 or 1 attribute on an
 * object (Those two cases account for almost 6000 things on M*U*S*H)
 *
 * \param thing the object to search on.
 * \param name the attribute name to look for
 * \return the matching attribute, or NULL
 */
static ATTR *
find_atr_in_list(dbref thing, char const *name)
{
  int count = AttrCount(thing);
  if (count == 0) {
    return NULL;
  } else if (count == 1) {
    if (strcmp(name, AL_NAME(List(thing))) == 0) {
      return List(thing);
    } else {
      return NULL;
    }
  } else if (count < LINEAR_CUT_OFF) {
    ATTR *a;
    ATTR_FOR_EACH (thing, a) {
      int c = strcmp(name, AL_NAME(a));
      if (c == 0) {
        return a;
      } else if (c < 0) {
        return NULL;
      }
    }
    return NULL;
  } else {
    ATTR dummy;
    AL_NAME(&dummy) = name;

    return bsearch(&dummy, List(thing), AttrCount(thing), sizeof(ATTR),
                   atr_comp);
  }
}

/** Find the place to insert/delete an attribute with the specified name.
 * \param thing the object to insert the attribute on
 * \param name the attribute name to look for
 * \return the index of the array where the attribute should go.
 */
static int
find_atr_pos_in_list(dbref thing, char const *name)
{
  int pos = 0;
  ATTR *a;
  /* TODO: Binary search? */
  ATTR_FOR_EACH (thing, a) {
    int comp = strcmp(name, AL_NAME(a));
    if (comp <= 0)
      return pos;
    pos += 1;
  }

  return pos;
}

/** Make sure an attribute array can hold at least a given number of attributes,
 * growing if needed.
 *
 * \param thing the object the attributes are on
 * \param capacity the desired capacity.
 * \return true if it can hold at least cap attributes.
 */
bool
attr_reserve(dbref thing, int cap)
{
  int oldcap;
  ATTR *newattrs;

  oldcap = AttrCap(thing);

  if (oldcap >= cap)
    return true;

  newattrs =
    mush_realloc(List(thing), (cap + 1) * sizeof(ATTR), "obj.attributes");

  if (!newattrs) {
    return false;
  }

  memset(newattrs + oldcap, 0, sizeof(ATTR) * (cap - oldcap + 1));
  List(thing) = newattrs;
  AttrCap(thing) = cap;
  return true;
}

/** Make sure an attribute array has enough capacity to hold another attribute,
 * and expand it if needed.
 *
 * \param thing the object the attribute is being added to.
 * \return true if another dbref can be added.
 */
static bool
atr_check_capacity(dbref thing)
{
  int oldcap = AttrCap(thing);
  if (oldcap == 0) {
    return attr_reserve(thing, 5);
  } else if (AttrCount(thing) < oldcap) {
    return true;
  } else {
    int newcap = oldcap * GROWTH_FACTOR;
    if (newcap < 5) {
      newcap = 5;
    }
    return attr_reserve(thing, newcap);
  }
}

/** Shrink capacity if there's too much unused space.
 *
 * \param thing the object to shrink.
 */
void
attr_shrink(dbref thing)
{
  ATTR *newattrs;
  int newcap;

  if (AttrCount(thing) == 0) {
    /* No attributes, but space; Free it */
    if (AttrCap(thing)) {
      mush_free(List(thing), "obj.attributes");
      List(thing) = NULL;
      AttrCap(thing) = 0;
    }
    return;
  } else if (AttrCap(thing) <= 5 ||
             ((double) AttrCap(thing) / (double) AttrCount(thing)) <
               SHRINK_FACTOR) {
    return;
  } else if (AttrCount(thing) == 1) {
    newcap = 5;
  } else {
    newcap = round(AttrCount(thing) * GROWTH_FACTOR);
  }

  newattrs =
    mush_realloc(List(thing), sizeof(ATTR) * (newcap + 1), "obj.attributes");
  if (newattrs) {
    List(thing) = newattrs;
    AttrCap(thing) = newcap;
  }
}

/** Make room for a new attribute at a given index. The attribute
 * array must have capacity greater than its current count.
 *
 * \param thing the object the attributes are on
 * \param pos the index a new attribute will go at.
 */
static void
atr_move_down(dbref thing, ptrdiff_t pos)
{
  memmove(db[thing].list + pos + 1, db[thing].list + pos,
          sizeof(ATTR) * (db[thing].attrcount - pos));
}

/** Shift an attribute array up to fill in a deleted attribute at a
 *  given index.
 *
 * \param thing the the object the attributes are on.
 * \param pos the position to shift everything to the right over by one.
 */
static void
atr_move_up(dbref thing, ptrdiff_t pos)
{
  if (pos < AttrCount(thing) - 1) {
    memmove(List(thing) + pos, List(thing) + pos + 1,
            sizeof(ATTR) * (AttrCount(thing) - pos - 1));
  }
  memset(List(thing) + AttrCount(thing) - 1, 0, sizeof(ATTR));
}

/** Do the work of creating the attribute entry on an object.
 * This doesn't do any permissions checking.  You should do that yourself.
 * \param thing the object to hold the attribute
 * \param atr_name the name for the attribute
 */
static ATTR *
create_atr(dbref thing, char const *atr_name)
{
  ATTR *ptr;
  char const *name;
  int pos;

  /* grow the attribute array if needed */
  if (!atr_check_capacity(thing)) {
    return NULL;
  }

  /* put the name in the string table */
  name = st_insert(atr_name, &atr_names);
  if (!name) {
    return NULL;
  }

  pos = find_atr_pos_in_list(thing, name);
  atr_move_down(thing, pos);
  ptr = List(thing) + pos;

  /* initialize atr */
  AL_NAME(ptr) = name;
  ptr->data = NULL_CHUNK_REFERENCE;
  AL_FLAGS(ptr) = 0;
  AttrCount(thing)++;

  return ptr;
}

/** Add an attribute to an object, dangerously.
 * This is a stripped down version of atr_add, without duplicate checking,
 * permissions checking, attribute count checking, or auto-ODARKing.
 * If anyone uses this outside of database load or atr_cpy (below),
 * I will personally string them up by their toes.  - Alex
 * \param thing object to set the attribute on.
 * \param atr name of the attribute to set.
 * \param s value of the attribute to set.
 * \param player the attribute creator.
 * \param flags bitmask of attribute flags for this attribute.
 * \param derefs the initial deref count to use for the attribute value.
 * \param makeroots if creating a branch (FOO`BAR) attr, and the root (FOO)
 *                  doesn't exist, should we create it instead of aborting?
 */
void
atr_new_add(dbref thing, const char *RESTRICT atr, const char *RESTRICT s,
            dbref player, uint32_t flags, uint8_t derefs, bool makeroots)
{
  ATTR *ptr;
  char *p, root_name[ATTRIBUTE_NAME_LIMIT + 1];

  if (!EMPTY_ATTRS && !*s && !(flags & AF_ROOT))
    return;

  /* Don't fail on a bad name, but do log it */
  if (!good_atr_name(atr))
    do_rawlog(LT_ERR, "Bad attribute name %s on object %s", atr,
              unparse_dbref(thing));

  ptr = find_atr_in_list(thing, atr);
  if (ptr) {
    /* Duplicate, probably because of an added root attribute.  This
       happens when reading a database written with a different sort
       order than this server is using. */
    AL_FLAGS(ptr) |= flags;
    AL_FLAGS(ptr) &= ~AF_COMMAND & ~AF_LISTEN;
    AL_CREATOR(ptr) = player;

    if (ptr->data) {
      chunk_delete(ptr->data);
      ptr->data = NULL_CHUNK_REFERENCE;
    }

    /* replace string with new string */
    if (!s || !*s) {
      /* nothing */
    } else {
      char *t = compress(s);
      if (!t)
        return;

      ptr->data = chunk_create(t, strlen(t), derefs);
      free(t);
      set_cmd_flags(ptr);
    }
    return;
  }

  strcpy(root_name, atr);
  if ((p = strrchr(root_name, '`'))) {
    ATTR *root;
    *p = '\0';
    root = find_atr_in_list(thing, root_name);
    if (!root) {
      if (!makeroots)
        return;
      do_rawlog(LT_ERR, "Missing root attribute '%s' on object #%d!\n",
                root_name, thing);
      atr_new_add(thing, root_name, EMPTY_ATTRS ? "" : " ", player, AF_ROOT, 0,
                  true);
    } else {
      if (!AF_Root(root)) /* Upgrading old database */
        AL_FLAGS(root) |= AF_ROOT;
    }
  }

  ptr = create_atr(thing, atr);
  if (!ptr)
    return;

  AL_FLAGS(ptr) = flags;
  AL_FLAGS(ptr) &= ~AF_COMMAND & ~AF_LISTEN;
  AL_CREATOR(ptr) = player;

  /* replace string with new string */
  if (!s || !*s) {
    /* nothing */
  } else {
    char *t = compress(s);
    if (!t)
      return;

    ptr->data = chunk_create(t, strlen(t), derefs);
    free(t);
    set_cmd_flags(ptr);
  }
}

static void
set_cmd_flags(ATTR *a)
{
  char *p = atr_value(a);
  int flag = AF_COMMAND;

  switch (*p) {
  case '^':
    flag = AF_LISTEN;
  /* FALL THROUGH */
  case '$':
    for (; *p; p++) {
      if (*p == '\\' && *(p + 1)) {
        p++;
      } else if (*p == ':') {
        AL_FLAGS(a) |= flag;
        break;
      }
    }
    break;
  }
}

void
unanchored_regexp_attr_check(dbref thing, ATTR *atr, dbref player)
{
  char *p;
  bool esc = 0, last_anchor_escaped = 0;

  /* We could check for AF_Listen, but an unanchored regexp
   * in a listen pattern is more likely to be intentional */
  if (!atr || !AF_Command(atr) || AF_Noprog(atr) || !GoodObject(player))
    return;

  p = atr_value(atr);
  if (!p || !*p || *p != '$')
    return;

  p++;
  if (*p != '^')
    goto warn;

  for (p++; *p; p++) {
    if (esc) {
      esc = 0;
      if (*p == '$')
        last_anchor_escaped = 1;
      continue;
    }
    if (*p == '\\') {
      esc = 1;
      continue;
    } else {
      last_anchor_escaped = 0;
    }
    if (*p == ':') {
      if (last_anchor_escaped || *(p - 1) != '$')
        goto warn;
      return;
    }
  }
  return;

warn:
  notify_format(player, T("Warning: Unanchored regexp command in #%d/%s."),
                thing, AL_NAME(atr));
  return;
}

/** Add an attribute to an object, safely.
 * \verbatim
 * This is the function that should be called in hardcode to add
 * an attribute to an object (but not to process things like @set that
 * may add or clear an attribute - see do_set_atr() for that).
 * \endverbatim
 * \param thing object to set the attribute on.
 * \param atr name of the attribute to set.
 * \param s value of the attribute to set.
 * \param player the attribute creator.
 * \param flags bitmask of attribute flags for this attribute. 0 will use
 *         default.
 * \return AE_OKAY or an AE_* error code.
 */
atr_err
atr_add(dbref thing, const char *RESTRICT atr, const char *RESTRICT s,
        dbref player, uint32_t flags)
{
  ATTR *ptr, *root = NULL;
  char *p;

  if (!s || (!EMPTY_ATTRS && !*s))
    return atr_clr(thing, atr, player);

  if (!good_atr_name(atr))
    return AE_BADNAME;

  /* walk the list, looking for a preexisting value */
  ptr = find_atr_in_list(thing, atr);

  /* check for permission to modify existing atr */
  if (ptr && AF_Safe(ptr))
    return AE_SAFE;
  if (ptr && !Can_Write_Attr(player, thing, ptr))
    return AE_ERROR;

  /* make a new atr, if needed */
  if (!ptr) {
    atr_err res = can_create_attr(player, thing, atr, flags);
    if (res != AE_OKAY)
      return res;

    strcpy(missing_name, atr);
    for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
      *p = '\0';

      root = find_atr_in_list(thing, missing_name);

      if (!root) {
        root = create_atr(thing, missing_name);
        if (!root)
          return AE_TREE;

        /* update modification time here, because from now on,
         * we modify even if we fail */
        if (!IsPlayer(thing) && !AF_Nodump(root))
          ModTime(thing) = mudtime;

        set_default_flags(root, flags);
        AL_FLAGS(root) &= ~AF_COMMAND & ~AF_LISTEN;
        AL_FLAGS(root) |= AF_ROOT;
        AL_CREATOR(root) = Owner(player);
        if (!EMPTY_ATTRS) {
          char *t = compress(" ");
          if (!t)
            mush_panic("Unable to allocate memory in atr_add()!");
          root->data = chunk_create(t, strlen(t), 0);
          free(t);
        }
      } else
        AL_FLAGS(root) |= AF_ROOT;

      *p = '`';
    }

    ptr = create_atr(thing, atr);
    if (!ptr)
      return AE_ERROR;

    set_default_flags(ptr, flags);
  }
  /* update modification time here, because from now on,
   * we modify even if we fail */
  if (!IsPlayer(thing) && !AF_Nodump(ptr))
    ModTime(thing) = mudtime;

  /* change owner */
  AL_CREATOR(ptr) = Owner(player);

  AL_FLAGS(ptr) &= ~AF_COMMAND & ~AF_LISTEN;

  /* replace string with new string */
  if (ptr->data)
    chunk_delete(ptr->data);
  if (!s || !*s) {
    ptr->data = NULL_CHUNK_REFERENCE;
  } else {
    char *t = compress(s);
    if (!t) {
      ptr->data = NULL_CHUNK_REFERENCE;
      return AE_ERROR;
    }
    ptr->data = chunk_create(t, strlen(t), 0);
    free(t);
    set_cmd_flags(ptr);
    if (AF_Command(ptr) && AF_Regexp(ptr)) {
      unanchored_regexp_attr_check(thing, ptr, player);
    }
  }

  return AE_OKAY;
}

/** Remove all child attributes from root attribute that can be.
 * \param player object doing a @wipe.
 * \param thing object being @wiped.
 * \param root root of attribute tree.
 * \return 1 if all children were deleted, 0 if some were left.
 */
static int
atr_clear_children(dbref player, dbref thing, ATTR *root)
{
  int skipped = 0;
  size_t len;
  const char *name;
  ATTR *sub;

  if (!root)
    return 1;

  name = AL_NAME(root);
  len = strlen(name);

  sub = atr_sub_branch_prev(root);

  if (!sub) {
    return 1;
  }
  sub += 1;

  while (AL_NAME(sub)) {
    const char *n2 = AL_NAME(sub);
    size_t len2 = strlen(n2);
    if (len2 < (len + 1) || n2[len] != '`' || strncmp(n2, name, len) != 0)
      break;
    if (AF_Root(sub)) {
      if (!atr_clear_children(player, thing, sub)) {
        skipped++;
        while (AL_NAME(++sub)) {
          const char *n3 = AL_NAME(sub);
          if (!n3 || strlen(n3) < (len2 + 1) || n3[len2] != '`' ||
              strncmp(n2, n3, len2) != 0) {
            break;
          }
        }
        continue;
      }
    }

    if (!Can_Write_Attr(player, thing, sub)) {
      skipped++;
      sub++;
      continue;
    }

    /* Can safely delete attribute.  */
    atr_free_one(thing, sub);
  }

  return !skipped;
}

/** Remove an attribute from an object.
 * This function clears an attribute from an object.
 * Permission is denied if the attribute is a branch, not a leaf.
 * \param thing object to clear attribute from.
 * \param atr name of attribute to remove.
 * \param player enactor attempting to remove attribute.
 * \param we_are_wiping true if called by \@wipe.
 * \return AE_OKAY or AE_* error code.
 */
static atr_err
real_atr_clr(dbref thing, char const *atr, dbref player, int we_are_wiping)
{
  int can_clear = 1;
  ATTR *ptr;

  ptr = find_atr_in_list(thing, atr);

  if (!ptr) {
    return AE_NOTFOUND;
  }
  if (AF_Safe(ptr)) {
    return AE_SAFE;
  }
  if (!Can_Write_Attr(player, thing, ptr)) {
    return AE_ERROR;
  }

  if (AF_Root(ptr) && !we_are_wiping)
    return AE_TREE;

  /* We only hit this if wiping. */
  if (AF_Root(ptr))
    can_clear = atr_clear_children(player, thing, ptr);

  if (can_clear) {
    char *p;
    char root_name[ATTRIBUTE_NAME_LIMIT + 1];

    strcpy(root_name, AL_NAME(ptr));

    if (!IsPlayer(thing) && !AF_Nodump(ptr))
      ModTime(thing) = mudtime;

    atr_free_one(thing, ptr);

    /* If this was the only leaf of a tree, clear the AF_ROOT flag from
     * the parent. */
    if ((p = strrchr(root_name, '`'))) {
      ATTR *root;

      *p = '\0';
      root = find_atr_in_list(thing, root_name);
      *p = '`';

      if (!root) {
        do_rawlog(LT_ERR, "Attribute %s on object #%d lacks a parent!",
                  root_name, thing);
      } else {
        if (!atr_sub_branch(root))
          AL_FLAGS(root) &= ~AF_ROOT;
      }
    }

    return AE_OKAY;
  } else
    return AE_TREE;
}

/** Remove an attribute from an object.
 * This function clears an attribute from an object.
 * Permission is denied if the attribute is a branch, not a leaf.
 * \param thing object to clear attribute from.
 * \param atr name of attribute to remove.
 * \param player enactor attempting to remove attribute.
 * \return AE_OKAY or an AE_* error code
 */
atr_err
atr_clr(dbref thing, char const *atr, dbref player)
{
  return real_atr_clr(thing, atr, player, 0);
}

/** \@wipe an attribute (And any leaves) from an object.
 * This function clears an attribute from an object.
 * \param thing object to clear attribute from.
 * \param atr name of attribute to remove.
 * \param player enactor attempting to remove attribute.
 * \return AE_OKAY or an AE_* error code.
 */
atr_err
wipe_atr(dbref thing, char const *atr, dbref player)
{
  return real_atr_clr(thing, atr, player, 1);
}

/** Wrapper for atr_get_with_parent()
 * \verbatim
 * Get an attribute from an object, checking its parents/ancestor if
 * the object does not have the attribute itself. Return a pointer to
 * the attribute structure (not its value), or NULL if the attr is
 * not found.
 * \endverbatim
 */
ATTR *
atr_get(dbref obj, char const *atrname)
{
  return atr_get_with_parent(obj, atrname, NULL, 0);
}

/** Retrieve an attribute from an object or its ancestors.
 * This function retrieves an attribute from an object, or from its
 * parent chain, returning a pointer to the first attribute that
 * matches or NULL. This is a pointer to an attribute structure, not
 * to the value of the attribute, so the value is usually accessed
 * through atr_value() or safe_atr_value().
 * \param obj the object containing the attribute.
 * \param atrname the name of the attribute.
 * \param parent if non-NULL, a dbref pointer to be set to the dbref of
 *               the object the attr was found on
 * \param cmd return NULL for no_command attributes?
 * \return pointer to the attribute structure retrieved, or NULL.
 */
static ATTR *
atr_get_with_parent(dbref obj, char const *atrname, dbref *parent, int cmd)
{
  static char name[ATTRIBUTE_NAME_LIMIT + 1];
  char *p;
  ATTR *atr;
  int parent_depth;
  dbref target;
  dbref ancestor;

  if (obj == NOTHING || !good_atr_name(atrname))
    return NULL;

  /* First try given name, then try alias match. */
  strcpy(name, atrname);
  for (;;) {
    /* Hunt through the parents/ancestor chain... */
    ancestor = Ancestor_Parent(obj);
    target = obj;
    parent_depth = 0;
    while (parent_depth < MAX_PARENTS && GoodObject(target)) {
      /* If the ancestor of the object is in its explict parent chain,
       * we use it there, and don't check the ancestor later.
       */
      if (target == ancestor)
        ancestor = NOTHING;

      /* If we're looking at a parent/ancestor, then we
       * need to check the branch path for privacy. We also
       * need to check the branch path if we're looking for no_command */
      if (target != obj || cmd) {
        for (p = strchr(name, '`'); p; p = strchr(p + 1, '`')) {
          *p = '\0';
          atr = find_atr_in_list(target, name);
          *p = '`';
          if (!atr)
            goto continue_target;
          else if (target != obj && AF_Private(atr)) {
            /* Can't inherit the attr or branches */
            return NULL;
          } else if (cmd && AF_Noprog(atr)) {
            /* Can't run commands in attr or branches */
            return NULL;
          }
        }
      }

      /* Now actually find the attribute. */
      atr = find_atr_in_list(target, name);
      if (atr) {
        if (target != obj && AF_Private(atr))
          return NULL;
        if (cmd && AF_Noprog(atr))
          return NULL;
        if (parent)
          *parent = target;
        return atr;
      }

    continue_target:
      /* Attribute wasn't on this object.  Check a parent or ancestor. */
      parent_depth++;
      target = Parent(target);
      if (!GoodObject(target)) {
        parent_depth = 0;
        target = ancestor;
      }
    }

    /* Try the alias, too... */
    atr = atr_match(atrname);
    if (!atr || strcmp(name, AL_NAME(atr)) == 0)
      break;
    strcpy(name, AL_NAME(atr));
  }

  return NULL;
}

/** Retrieve an attribute from an object.
 * This function retrieves an attribute from an object, and does not
 * check the parent chain. It returns a pointer to the attribute
 * or NULL.  This is a pointer to an attribute structure, not
 * to the value of the attribute, so the (compressed) value is usually
 * to the value of the attribute, so the value is usually accessed
 * through atr_value() or safe_atr_value().
 * \param thing the object containing the attribute.
 * \param atr the name of the attribute.
 * \return pointer to the attribute structure retrieved, or NULL.
 */
ATTR *
atr_get_noparent(dbref thing, char const *atr)
{
  ATTR *ptr;

  if (thing == NOTHING || !good_atr_name(atr))
    return NULL;

  /* try real name */
  ptr = find_atr_in_list(thing, atr);
  if (ptr)
    return ptr;

  ptr = atr_match(atr);
  if (!ptr || strcmp(atr, AL_NAME(ptr)) == 0)
    return NULL;
  atr = AL_NAME(ptr);

  /* try alias */
  ptr = find_atr_in_list(thing, atr);
  if (ptr)
    return ptr;

  return NULL;
}

/** Apply a function to a set of attributes.
 * This function applies another function to a set of attributes on an
 * object specified by a (wildcarded) pattern to match against the
 * attribute name.
 * \param player the enactor.
 * \param thing the object containing the attribute.
 * \param name the pattern to match against the attribute name.
 * \param flags flags to control matching and ordering.
 * \param func the function to call for each matching attribute.
 * \param args additional arguments to pass to the function.
 * \return the sum of the return values of the functions called.
 */
int
atr_iter_get(dbref player, dbref thing, const char *name, unsigned flags,
             aig_func func, void *args)
{
  ATTR *ptr;
  int result = 0;
  size_t len;
  extern bool in_wipe;

  if (!name || !*name) {
    if (flags & AIG_REGEX) {
      flags &= ~AIG_REGEX;
      name = "**";
    } else {
      name = "*";
    }
  }
  len = strlen(name);

  /* Must check name[len-1] first as wildcard_count() can destructively modify
   * name */
  if (!(flags & AIG_REGEX) && name[len - 1] != '`' &&
      wildcard_count((char *) name, 1) != -1) {
    char abuff[BUFFER_LEN];
    ptr = atr_get_noparent(thing, strupper_r(name, abuff, sizeof abuff));
    if (ptr && ((flags & AIG_MORTAL) ? Is_Visible_Attr(thing, ptr)
                                     : Can_Read_Attr(player, thing, ptr)))
      result = func(player, thing, NOTHING, name, ptr, args);
  } else if (AttrCount(thing)) {
    pcre2_code *re = NULL;
    pcre2_match_data *md = NULL;
    int errcode;
    PCRE2_SIZE erroffset;

    if (flags & AIG_REGEX) {
      re = pcre2_compile((const PCRE2_UCHAR *) name, len,
                         re_compile_flags | PCRE2_CASELESS, &errcode,
                         &erroffset, re_compile_ctx);
      if (!re) {
        return 0;
      }
    } else {
      /* Compile wildcard to regexp */
      PCRE2_UCHAR *as_re = NULL;
      PCRE2_SIZE rlen;
      char *glob;

      if (name[len - 1] == '`') {
        glob = sqlite3_mprintf("%s*", name);
        len += 1;
      } else {
        glob = sqlite3_mprintf("%s", name);
      }

      if (pcre2_pattern_convert((const PCRE2_UCHAR *) glob, len,
                                PCRE2_CONVERT_GLOB, &as_re, &rlen,
                                glob_convert_ctx) == 0) {
        re = pcre2_compile(as_re, rlen, re_compile_flags | PCRE2_CASELESS,
                           &errcode, &erroffset, re_compile_ctx);
        pcre2_converted_pattern_free(as_re);
      }
      sqlite3_free(glob);
    }
    if (re) {
      flags |= AIG_REGEX;
      pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);
      md = pcre2_match_data_create_from_pattern(re, NULL);
    }

    ATTR_FOR_EACH (thing, ptr) {
      if (cpu_time_limit_hit)
        break;
      if (strchr(AL_NAME(ptr), '`')) {
        continue;
      }
      if (((flags & AIG_MORTAL) ? Is_Visible_Attr(thing, ptr)
                                : Can_Read_Attr(player, thing, ptr)) &&
          ((flags & AIG_REGEX)
             ? qcomp_regexp_match(re, md, AL_NAME(ptr), PCRE2_ZERO_TERMINATED)
             : atr_wild(name, AL_NAME(ptr)))) {
        int r = func(player, thing, NOTHING, name, ptr, args);
        result += r;
        if (r && in_wipe) {
          ptr -= 1;
          continue;
        }
      }
      if (AL_FLAGS(ptr) & AF_ROOT) {
        ATTR *prev = ptr;
        for (ptr = atr_sub_branch(ptr);
             AL_NAME(ptr) && is_atree_root(AL_NAME(prev), AL_NAME(ptr));
             ptr++) {
          if (((flags & AIG_MORTAL) ? Is_Visible_Attr(thing, ptr)
                                    : Can_Read_Attr(player, thing, ptr)) &&
              ((flags & AIG_REGEX) ? qcomp_regexp_match(re, md, AL_NAME(ptr),
                                                        PCRE2_ZERO_TERMINATED)
                                   : atr_wild(name, AL_NAME(ptr)))) {
            int r = func(player, thing, NOTHING, name, ptr, args);
            result += r;
            if (r && in_wipe) {
              ptr -= 1;
            }
          }
        }
        ptr = prev;
      }
    }
    if (re) {
      pcre2_code_free(re);
    }
    if (md) {
      pcre2_match_data_free(md);
    }
  }

  return result;
}

/** Helper function for atr_pattern_count, passed to atr_iter_get() */
static int
atr_count_helper(dbref player __attribute__((__unused__)),
                 dbref thing __attribute__((__unused__)),
                 dbref parent __attribute__((__unused__)),
                 char const *pattern __attribute__((__unused__)),
                 ATTR *atr __attribute__((__unused__)),
                 void *args __attribute__((__unused__)))
{
  return 1;
}

/** Count the number of attributes an object has that match a pattern,
 * \verbatim
 * If <doparent> is true, then count parent attributes as well,
 * but excluding duplicates.
 * \endverbatim
 * \param player the enactor.
 * \param thing the object containing the attribute.
 * \param name the pattern to match against the attribute name.
 * \param doparent count parent attrbutes as well?
 * \param flags atr_iter_get flags.
 * \param mortal only fetch mortal-visible attributes?
 * \param regexp is name a regexp, rather than a glob pattern?
 * \return the count of matching attributes
 */
int
atr_pattern_count(dbref player, dbref thing, const char *name, int doparent,
                  unsigned flags)
{
  if (doparent) {
    return atr_iter_get_parent(player, thing, name, flags, atr_count_helper,
                               NULL);
  } else {
    return atr_iter_get(player, thing, name, flags, atr_count_helper, NULL);
  }
}

/** Apply a function to a set of attributes, including inherited ones.
 * This function applies another function to a set of attributes on an
 * object specified by a (wildcarded) pattern to match against the
 * attribute name on an object or its parents.
 * \param player the enactor.
 * \param thing the object containing the attribute.
 * \param name the pattern to match against the attribute name.
 * \param flags flags to control matching.
 * \param func the function to call for each matching attribute, with
 *  a pointer to the dbref of the object the attribute is really on passed
 *  as the function's args argument.
 * \param args arguments passed to the func
 * \return the sum of the return values of the functions called.
 */
int
atr_iter_get_parent(dbref player, dbref thing, const char *name, unsigned flags,
                    aig_func func, void *args)
{
  ATTR *ptr;
  int result;
  size_t len;
  dbref parent = NOTHING;

  result = 0;
  if (!name || !*name) {
    if (flags & AIG_REGEX) {
      flags &= ~AIG_REGEX;
      name = "**";
    } else {
      name = "*";
    }
  }
  len = strlen(name);

  /* Must check name[len-1] first as wildcard_count() can destructively modify
   * name */
  if (!(flags & AIG_REGEX) && name[len - 1] != '`' &&
      wildcard_count((char *) name, 1) != -1) {
    char abuff[BUFFER_LEN];
    ptr = atr_get_with_parent(thing, strupper_r(name, abuff, sizeof abuff),
                              &parent, 0);
    if (ptr && ((flags & AIG_MORTAL) ? Is_Visible_Attr(parent, ptr)
                                     : Can_Read_Attr(player, parent, ptr)))
      result = func(player, thing, parent, name, ptr, args);
  } else {
    StrTree seen;
    int parent_depth;
    pcre2_code *re = NULL;
    pcre2_match_data *md = NULL;
    int errcode;
    PCRE2_SIZE erroffset;

    if (flags & AIG_REGEX) {
      re = pcre2_compile((const PCRE2_UCHAR *) name, len,
                         re_compile_flags | PCRE2_CASELESS, &errcode,
                         &erroffset, re_compile_ctx);
      if (!re) {
        return 0;
      }
    } else {
      /* Compile wildcard to regexp */
      PCRE2_UCHAR *as_re = NULL;
      PCRE2_SIZE rlen;
      char *glob;

      if (name[len - 1] == '`') {
        glob = sqlite3_mprintf("%s*", name);
        len += 1;
      } else {
        glob = sqlite3_mprintf("%s", name);
      }

      if (pcre2_pattern_convert((const PCRE2_UCHAR *) glob, len,
                                PCRE2_CONVERT_GLOB, &as_re, &rlen,
                                glob_convert_ctx) == 0) {
        re = pcre2_compile(as_re, rlen, re_compile_flags | PCRE2_CASELESS,
                           &errcode, &erroffset, re_compile_ctx);
        pcre2_converted_pattern_free(as_re);
      }
      sqlite3_free(glob);
    }
    if (re) {
      flags |= AIG_REGEX;
      pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);
      md = pcre2_match_data_create_from_pattern(re, NULL);
    }

    st_init(&seen, "AttrsSeenTree");
    for (parent_depth = MAX_PARENTS + 1, parent = thing;
         parent_depth-- && parent != NOTHING && !cpu_time_limit_hit;
         parent = Parent(parent)) {
      ATTR_FOR_EACH (parent, ptr) {
        if (cpu_time_limit_hit)
          break;
        if (!st_find(AL_NAME(ptr), &seen)) {
          st_insert(AL_NAME(ptr), &seen);
          if (parent != thing) {
            if (AF_Private(ptr))
              continue;
          }

          if (((flags & AIG_MORTAL) ? Is_Visible_Attr(parent, ptr)
                                    : Can_Read_Attr(player, parent, ptr)) &&
              ((flags & AIG_REGEX) ? qcomp_regexp_match(re, md, AL_NAME(ptr),
                                                        PCRE2_ZERO_TERMINATED)
                                   : atr_wild(name, AL_NAME(ptr)))) {
            result += func(player, thing, parent, name, ptr, args);
          }
          if (AL_FLAGS(ptr) & AF_ROOT) {
            ATTR *prev = ptr;
            for (ptr = atr_sub_branch(ptr);
                 AL_NAME(ptr) && is_atree_root(AL_NAME(prev), AL_NAME(ptr));
                 ptr++) {
              if (AF_Private(ptr) && thing != parent) {
                continue;
              }

              if (strchr(AL_NAME(ptr), '`')) {
                /* We need to check all the branches of the tree for no_inherit
                 */
                char bname[BUFFER_LEN];
                char *p;
                ATTR *branch;
                bool skip = 0;

                strcpy(bname, AL_NAME(ptr));
                for (p = strchr(bname, '`'); p; p = strchr(p + 1, '`')) {
                  *p = '\0';
                  branch = find_atr_in_list(parent, bname);
                  *p = '`';
                  if (branch && AF_Private(branch)) {
                    skip = 1;
                    break;
                  }
                }
                if (skip)
                  continue;
              }

              if (!st_find(AL_NAME(ptr), &seen) &&
                  ((flags & AIG_MORTAL) ? Is_Visible_Attr(thing, ptr)
                                        : Can_Read_Attr(player, thing, ptr)) &&
                  ((flags & AIG_REGEX)
                     ? qcomp_regexp_match(re, md, AL_NAME(ptr),
                                          PCRE2_ZERO_TERMINATED)
                     : atr_wild(name, AL_NAME(ptr)))) {
                st_insert(AL_NAME(ptr), &seen);
                result += func(player, thing, parent, name, ptr, args);
              }
            }
            ptr = prev;
          }
        }
      }
    }
    if (re) {
      pcre2_code_free(re);
    }
    if (md) {
      pcre2_match_data_free(md);
    }
    st_flush(&seen);
  }

  return result;
}

/** Free the memory associated with all attributes of an object.
 * This function frees all of an object's attribute memory.
 * This includes the memory allocated to hold the attribute's value,
 * and the attribute's entry in the object's string tree.
 * Freed attribute structures are added to the free list.
 * \param thing dbref of object
 */
void
atr_free_all(dbref thing)
{
  ATTR *ptr;

  if (AttrCap(thing) == 0) {
    return;
  }

  if (!IsPlayer(thing) && AttrCount(thing)) {
    ModTime(thing) = mudtime;
  }

  ATTR_FOR_EACH (thing, ptr) {
    if (ptr->data)
      chunk_delete(ptr->data);
    st_delete(AL_NAME(ptr), &atr_names);
  }

  mush_free(List(thing), "obj.attributes");
  AttrCount(thing) = AttrCap(thing) = 0;
  List(thing) = NULL;
}

/** Copy all of the attributes from one object to another.
 * \verbatim
 * This function is used by @clone to copy all of the attributes
 * from one object to another.
 * \endverbatim
 * \param dest destination object to receive attributes.
 * \param source source object containing attributes.
 */
void
atr_cpy(dbref dest, dbref source)
{
  ATTR *ptr;
  int max_attrs;

  max_attrs = (Many_Attribs(dest) ? HARD_MAX_ATTRCOUNT : MAX_ATTRCOUNT);
  attr_reserve(dest, AttrCount(source));

  ATTR_FOR_EACH (source, ptr) {
    if (AttrCount(dest) > max_attrs) {
      break;
    }
    if (!AF_Nocopy(ptr)) {
      atr_new_add(dest, AL_NAME(ptr), atr_value(ptr), AL_CREATOR(ptr),
                  AL_FLAGS(ptr), AL_DEREFS(ptr), 0);
    }
  }
}

static bool
can_debug(dbref player, dbref victim)
{
  ATTR *a;
  char *aval, *dfl, *curr;
  dbref member;
  int success = 0;

  if (controls(player, victim)) {
    return 1;
  }

  a = atr_get(victim, "DEBUGFORWARDLIST");
  if (!a) {
    return 0;
  }
  aval = safe_atr_value(a, "atrval.can_debug");
  dfl = trim_space_sep(aval, ' ');
  while ((curr = split_token(&dfl, ' ')) != NULL) {
    if (!is_objid(curr))
      continue;
    member = parse_objid(curr);
    if (member == player) {
      success = 1;
      break;
    }
  }
  mush_free(aval, "atrval.can_debug");
  return success;
}

/** Match input against a $command or ^listen attribute.
 * This function attempts to match a string against either an $-command
 * or ^listens on an object. Matches may be glob or regex matches,
 * depending on the attribute's flags. With the reasonably safe assumption
 * that most of the matches are going to fail, the faster non-capturing
 * glob match is done first, and the capturing version only called when
 * we already know it'll match. Due to the way PCRE works, there's no
 * advantage to doing something similar for regular expression matches.
 *
 * This is a helper function used by one_comm_match, atr_comm_match, and
 * others.
 */
int
atr_single_match_r(ATTR *ptr, int flag_mask, int end, const char *input,
                   char *args[], char *match_space, int match_space_len,
                   char cmd_buff[], PE_REGS *pe_regs)
{
  char buff[BUFFER_LEN];
  char *atrval;
  int i, j;
  int match_found = 0;

  if (!ptr) {
    return 0;
  }

  if (!(AL_FLAGS(ptr) & flag_mask)) {
    return 0;
  }

  /* atr_value returns a static buffer, but we won't be calling uncompress
   * again.
   */
  atrval = atr_value(ptr);

  if (!atrval || !atrval[0] || !atrval[1]) {
    return 0;
  }

  if (atrval[0] != '^' && atrval[0] != '$') {
    return 0;
  }
  /* Find and copy the pattern (regexp or wild) to buff.
   * Convert \:s into :, but leave all other \s alone. And
   * make sure we don't trip over foo\\:, which isn't escaping :.
   */
  for (i = 1, j = 0; atrval[i] && atrval[i] != end; i++) {
    if (atrval[i] == '\\' && atrval[i + 1]) {
      if (atrval[i + 1] == end) {
        i++;
      } else {
        buff[j++] = atrval[i++];
      }
    }
    buff[j++] = atrval[i];
  }
  buff[j] = '\0';

  /* at this point, atrval[i] should be the separating ':'.
   * If it's not, this ain't an $ or ^-pattern.
   */
  if (!atrval[i]) {
    return 0;
  }
  i++;

  if (cmd_buff) {
    strncpy(cmd_buff, atrval + i, BUFFER_LEN);
  }

  if (AF_Regexp(ptr)) {
    if (regexp_match_case_r(buff, input, AF_Case(ptr), args, MAX_STACK_ARGS,
                            match_space, match_space_len, pe_regs,
                            PE_REGS_ARG)) {
      match_found = 1;
    }
  } else {
    if (wild_match_case_r(buff, input, AF_Case(ptr), args, MAX_STACK_ARGS,
                          match_space, match_space_len, pe_regs, PE_REGS_ARG)) {
      match_found = 1;
    }
  }
  return match_found;
}

/** Match input against a $command or ^listen attribute.
 * This function attempts to match a string against either the $commands
 * or ^listens on an object. Matches may be glob or regex matches,
 * depending on the attribute's flags. With the reasonably safe assumption
 * that most of the matches are going to fail, the faster non-capturing
 * glob match is done first, and the capturing version only called when
 * we already know it'll match. Due to the way PCRE works, there's no
 * advantage to doing something similar for regular expression matches.
 * \param thing object containing attributes to check.
 * \param player the enactor, for privilege checks.
 * \param type either '$' or '^', indicating the type of attribute to check.
 * \param end character that denotes the end of a command (usually ':').
 * \param str string to match against attributes.
 * \param just_match if true, return match without executing code.
 * \param check_locks check to make sure player passes thing's \@locks?
 * \param atrname used to return the list of matching object/attributes.
 * \param abp pointer to end of atrname.
 * \param show_child always show the child in atrname, even if the command
          was found on the parent?
 * \param errobj if an attribute matches, but the lock fails, this pointer
 *        is used to return the failing dbref. If NULL, we don't bother.
 * \param from_queue the parent queue to run matching attrs inplace for,
          if just_match is false and queue_type says to run inplace
 * \param queue_type QUEUE_* flags of how to queue any matching attrs
 * \param pe_regs_parent if non-null, a PE_REGS containing named arguments,
          copied into the pe_regs when a matched attribute is executed
 * \return number of attributes that matched, or 0
 */
int
atr_comm_match(dbref thing, dbref player, int type, int end, char const *str,
               int just_match, int check_locks, char *atrname, char **abp,
               int show_child, dbref *errobj, MQUE *from_queue, int queue_type,
               PE_REGS *pe_regs_parent)
{
  uint32_t flag_mask;
  ATTR *ptr;
  int parent_depth;
  char *args[MAX_STACK_ARGS];
  PE_REGS *pe_regs = NULL;
  char cmd_buff[BUFFER_LEN];
  int match, match_found;
  int lock_checked = !check_locks;
  char match_space[BUFFER_LEN * 2];
  ssize_t match_space_len = BUFFER_LEN * 2;
  NEW_PE_INFO *pe_info;
  dbref current = thing, next = NOTHING;
  int parent_count = 0;
  StrTree seen, nocmd_roots, private_attrs;

  /* check for lots of easy ways out */
  if (type != '$' && type != '^')
    return 0;
  if (check_locks && (!GoodObject(thing) || Halted(thing) ||
                      (type == '$' && NoCommand(thing))))
    return 0;

  if (type == '$') {
    flag_mask = AF_COMMAND;
    parent_depth = GoodObject(Parent(thing));
  } else {
    flag_mask = AF_LISTEN;
    if (has_flag_by_name(thing, "LISTEN_PARENT",
                         TYPE_PLAYER | TYPE_THING | TYPE_ROOM)) {
      parent_depth = GoodObject(Parent(thing));
    } else {
      parent_depth = 0;
    }
  }
  match = 0;

  pe_info = make_pe_info("pe_info-atr_comm_match");
  if (from_queue && from_queue->pe_info && *from_queue->pe_info->cmd_raw) {
    pe_info->cmd_raw = mush_strdup(from_queue->pe_info->cmd_raw, "string");
  } else {
    pe_info->cmd_raw = mush_strdup(str, "string");
  }

  if (from_queue && from_queue->pe_info && *from_queue->pe_info->cmd_evaled) {
    pe_info->cmd_evaled =
      mush_strdup(from_queue->pe_info->cmd_evaled, "string");
  } else {
    pe_info->cmd_evaled = mush_strdup(str, "string");
  }

  if (!just_match) {
    pe_regs = pe_regs_create(PE_REGS_ARG, "atr_comm_match");
    pe_regs_copystack(pe_regs, pe_regs_parent, PE_REGS_ARG, 1);
  }

  st_init(&seen, "AttrsSeenTree");
  st_init(&nocmd_roots, "AttrsSeenTree");
  st_init(&private_attrs, "AttrsSeenTree");

  do {
    next =
      parent_depth ? next_parent(thing, current, &parent_count, NULL) : NOTHING;

    st_flush(&private_attrs);

    ATTR_FOR_EACH (current, ptr) {
      if (cpu_time_limit_hit)
        break;
      if (current == thing) {
        if (st_find(AL_NAME(ptr), &nocmd_roots)) {
          continue;
        }
        st_insert(AL_NAME(ptr), &seen);
        if (AF_Noprog(ptr)) {
          /* No-command. This, and later trees with this path its root
             are skipped. */
          st_insert(AL_NAME(ptr), &nocmd_roots);
          if (AF_Root(ptr)) {
            ATTR *p2 = atr_sub_branch(ptr);
            if (p2) {
              for (; AL_NAME(p2) && is_atree_root(AL_NAME(ptr), AL_NAME(p2));
                   p2++) {
                st_insert(AL_NAME(p2), &nocmd_roots);
              }
            }
          }
          continue;
        }
      } else {
        if (st_find(AL_NAME(ptr), &private_attrs)) {
          /* Already decided to skip this attribute */
          continue;
        }
        if (st_find(AL_NAME(ptr), &nocmd_roots)) {
          /* Skip attributes that are masked by an earlier nocommand */
          if (AF_Root(ptr)) {
            ATTR *p2 = atr_sub_branch(ptr);
            if (p2) {
              for (; AL_NAME(p2) && is_atree_root(AL_NAME(ptr), AL_NAME(p2));
                   p2++) {
                st_insert(AL_NAME(p2), &nocmd_roots);
                st_insert(AL_NAME(p2), &private_attrs);
              }
            }
          }
          continue;
        }
        if (AF_Private(ptr)) {
          /* No-inherit. This attribute is not visible, but later ones
             with the same name can be */
          st_insert(AL_NAME(ptr), &private_attrs);
          if (AF_Root(ptr)) {
            ATTR *p2 = atr_sub_branch(ptr);
            if (p2) {
              for (; AL_NAME(p2) && is_atree_root(AL_NAME(ptr), AL_NAME(p2));
                   p2++) {
                st_insert(AL_NAME(p2), &private_attrs);
              }
            }
          }
          continue;
        }
        if (AF_Noprog(ptr)) {
          /* No-command. This, and later trees with this path its root
             are skipped. */
          st_insert(AL_NAME(ptr), &nocmd_roots);
          if (AF_Root(ptr)) {
            ATTR *p2 = atr_sub_branch(ptr);
            if (p2) {
              for (; AL_NAME(p2) && is_atree_root(AL_NAME(ptr), AL_NAME(p2));
                   p2++) {
                st_insert(AL_NAME(p2), &nocmd_roots);
              }
            }
          }
          continue;
        }
        if (st_find(AL_NAME(ptr), &seen)) {
          continue;
        } else {
          st_insert(AL_NAME(ptr), &seen);
        }
      }

      if (!(AL_FLAGS(ptr) & flag_mask)) {
        continue;
      }

      if (type == '^' && !AF_Ahear(ptr)) {
        if ((thing == player && !AF_Mhear(ptr)) ||
            (thing != player && AF_Mhear(ptr)))
          continue;
      }

      match_found =
        atr_single_match_r(ptr, flag_mask, end, str, args, match_space,
                           match_space_len, cmd_buff, pe_regs);
      if (match_found)
        match++;

      if (match_found) {
        /* We only want to do the lock check once, so that any side
         * effects in the lock are only performed once per utterance.
         * Thus, '$foo *r:' and '$foo b*:' on the same object will only
         * run the lock once for 'foo bar'. Locks are always checked on
         * the child, even when the attr is inherited.
         */
        if (!lock_checked) {
          lock_checked = 1;
          if ((type == '$' &&
               !eval_lock_with(player, thing, Command_Lock, pe_info)) ||
              (type == '^' &&
               !eval_lock_with(player, thing, Listen_Lock, pe_info)) ||
              !eval_lock_with(player, thing, Use_Lock, pe_info)) {
            match--;
            if (errobj)
              *errobj = thing;
            /* If we failed the lock, there's no point in continuing at all. */
            next = NOTHING;
            break;
          }
        }
        if (atrname && abp) {
          safe_chr(' ', atrname, abp);
          if (current == thing || show_child || !Can_Examine(player, current))
            safe_dbref(thing, atrname, abp);
          else
            safe_dbref(current, atrname, abp);
          safe_chr('/', atrname, abp);
          safe_str(AL_NAME(ptr), atrname, abp);
        }
        if (!just_match) {
          char tmp[BUFFER_LEN];

          if (from_queue &&
              (queue_type & ~QUEUE_DEBUG_PRIVS) != QUEUE_DEFAULT) {
            int pe_flags = PE_INFO_DEFAULT;
            if (!(queue_type & QUEUE_CLEAR_QREG)) {
              /* Copy parent q-registers into new queue */
              pe_flags |= PE_INFO_COPY_QREG;
            } else {
              /* Since we use a new pe_info for this inplace entry, instead of
                 sharing the parent's, we don't need to explicitly clear the
                 q-registers, they're empty by default */
              queue_type &= ~QUEUE_CLEAR_QREG;
            }
            if (!(queue_type & QUEUE_PRESERVE_QREG)) {
              /* Cause q-registers from the end of the new queue entry to be
                 copied into the parent queue entry */
              queue_type |= QUEUE_PROPAGATE_QREG;
            } else {
              /* Since we use a new pe_info for this inplace entry, instead of
                 sharing the parent's, we don't need to implicitly save/reset
                 the
                 q-registers - we'll be altering different copies anyway */
              queue_type &= ~QUEUE_PRESERVE_QREG;
            }
            if (AF_NoDebug(ptr)) {
              queue_type |= QUEUE_NODEBUG;
            } else if (AF_Debug(ptr)) {
              queue_type |= QUEUE_DEBUG;
            }

            /* inplace queue */
            snprintf(tmp, sizeof tmp, "#%d/%s", thing, AL_NAME(ptr));
            new_queue_actionlist_int(thing, player, player, cmd_buff,
                                     from_queue, pe_flags, queue_type, pe_regs,
                                     tmp);
          } else {
            /* Normal queue */
            parse_que_attr(
              thing, player, cmd_buff, pe_regs, ptr,
              (queue_type & QUEUE_DEBUG_PRIVS ? can_debug(player, thing) : 0));
          }
          pe_regs_free(pe_regs);
          pe_regs = pe_regs_create(PE_REGS_ARG, "atr_comm_match");
          pe_regs_copystack(pe_regs, pe_regs_parent, PE_REGS_ARG, 1);
        }
      }
    }
  } while ((current = next) != NOTHING && !cpu_time_limit_hit);

  st_flush(&seen);
  st_flush(&nocmd_roots);
  st_flush(&private_attrs);

  if (pe_regs)
    pe_regs_free(pe_regs);
  free_pe_info(pe_info);
  return match;
}

/** Match input against a specified object's specified $command
 * attribute. Matches may be glob or regex matches. Used in command hooks.
 * depending on the attribute's flags.
 * \param thing object containing attributes to check.
 * \param player the enactor, for privilege checks.
 * \param atr the name of the attribute
 * \param str the string to match
 * \param from_queue parent queue to run the cmds inplace for, if queue_type
 says to do so
 * \param queue_type QUEUE_* flags telling how to run the matched commands
 * \param pe_regs_parent if non-null, a PE_REGS containing named arguments,
          copied into the pe_regs when the matched attribute is executed
 * \retval 1 attribute matched.
 * \retval 0 attribute failed to match.
 */
int
one_comm_match(dbref thing, dbref player, const char *atr, const char *str,
               MQUE *from_queue, int queue_type, PE_REGS *pe_regs_parent)
{
  ATTR *ptr;
  char cmd_buff[BUFFER_LEN];
  PE_REGS *pe_regs;
  char match_space[BUFFER_LEN * 2];
  char *args[MAX_STACK_ARGS];
  ssize_t match_space_len = BUFFER_LEN * 2;
  int success = 0;

  /* check for lots of easy ways out */
  if (!GoodObject(thing) || Halted(thing) || NoCommand(thing))
    return 0;

  if (!(ptr = atr_get_with_parent(thing, atr, NULL, 1)))
    return 0;

  if (!AF_Command(ptr))
    return 0;

  pe_regs = pe_regs_create(PE_REGS_ARG, "one_comm_match");
  pe_regs_copystack(pe_regs, pe_regs_parent, PE_REGS_ARG, 1);

  if (atr_single_match_r(ptr, AF_COMMAND, ':', str, args, match_space,
                         match_space_len, cmd_buff, pe_regs)) {
    char *save_cmd_raw = NULL, *save_cmd_evaled = NULL;
    NEW_PE_INFO *pe_info;

    if (from_queue && (queue_type & ~QUEUE_DEBUG_PRIVS) != QUEUE_DEFAULT) {
      pe_info = from_queue->pe_info;
      /* Save and reset %c/%u */
      save_cmd_raw = pe_info->cmd_raw;
      pe_info->cmd_raw = NULL;
      save_cmd_evaled = pe_info->cmd_evaled;
      pe_info->cmd_evaled = NULL;
    } else {
      pe_info = make_pe_info("pe_info-one_comm_match");
    }
    pe_info->cmd_raw = mush_strdup(str, "string");
    pe_info->cmd_evaled = mush_strdup(str, "string");
    if (eval_lock_clear(player, thing, Command_Lock, pe_info) &&
        eval_lock_clear(player, thing, Use_Lock, pe_info))
      success = 1;
    if (from_queue && (queue_type & ~QUEUE_DEBUG_PRIVS) != QUEUE_DEFAULT) {
      /* Restore */
      mush_free(pe_info->cmd_raw, "string");
      mush_free(pe_info->cmd_evaled, "string");
      pe_info->cmd_raw = save_cmd_raw;
      pe_info->cmd_evaled = save_cmd_evaled;
    } else {
      free_pe_info(pe_info);
    }
    if (success) {
      char tmp[BUFFER_LEN];
      if (from_queue && (queue_type & ~QUEUE_DEBUG_PRIVS) != QUEUE_DEFAULT) {
        /* inplace queue */
        int pe_flags = PE_INFO_DEFAULT;
        if (!(queue_type & QUEUE_CLEAR_QREG)) {
          /* Copy parent q-registers into new queue */
          pe_flags |= PE_INFO_COPY_QREG;
        } else {
          /* Since we use a new pe_info for this inplace entry, instead of
             sharing the parent's, we don't need to explicitly clear the
             q-registers, they're empty by default */
          queue_type &= ~QUEUE_CLEAR_QREG;
        }
        if (!(queue_type & QUEUE_PRESERVE_QREG)) {
          /* Cause q-registers from the end of the new queue entry to be
             copied into the parent queue entry */
          queue_type |= QUEUE_PROPAGATE_QREG;
        } else {
          /* Since we use a new pe_info for this inplace entry, instead of
             sharing the parent's, we don't need to implicitly save/reset the
             q-registers - we'll be altering different copies anyway */
          queue_type &= ~QUEUE_PRESERVE_QREG;
        }
        if (AF_NoDebug(ptr)) {
          queue_type |= QUEUE_NODEBUG;
        } else if (AF_Debug(ptr)) {
          queue_type |= QUEUE_DEBUG;
        }

        /* inplace queue */
        snprintf(tmp, sizeof tmp, "#%d/%s", thing, AL_NAME(ptr));
        new_queue_actionlist_int(thing, player, player, cmd_buff, from_queue,
                                 pe_flags, queue_type, pe_regs, tmp);
      } else {
        /* Normal queue */
        parse_que_attr(
          thing, player, cmd_buff, pe_regs, ptr,
          (queue_type & QUEUE_DEBUG_PRIVS ? can_debug(player, thing) : 0));
      }
    }
  }
  pe_regs_free(pe_regs);
  return success;
}

/*======================================================================*/

/** Set or clear an attribute on an object.
 * \verbatim
 * This is the primary function for implementing @set.
 * A new interface (as of 1.6.9p0) for setting attributes,
 * which takes care of case-fixing, object-level flag munging,
 * alias recognition, add/clr distinction, etc.  Enjoy.
 * \endverbatim
 * \param thing object to set the attribute on or remove it from.
 * \param atr name of the attribute to set or clear.
 * \param s value to set the attribute to (or NULL to clear).
 * \param player enactor, for permission checks.
 * \param flags attribute flags.
 * \retval -1 failure - invalid value for attribute.
 * \retval 0 failure for other reason
 * \retval 1 success.
 */
int
do_set_atr(dbref thing, const char *RESTRICT atr, const char *RESTRICT s,
           dbref player, uint32_t flags)
{
  ATTR *old;
  char name[BUFFER_LEN];
  char tbuf1[BUFFER_LEN];
  atr_err res;
  int was_hearer;
  int was_listener;
  dbref announceloc;
  const char *new;
  if (!EMPTY_ATTRS && s && !*s)
    s = NULL;
  if (IsGarbage(thing)) {
    notify(player, T("Garbage is garbage."));
    return 0;
  }
  if (!controls(player, thing))
    return 0;
  strupper_r(atr, name, sizeof name);
  if (strcmp(name, "ALIAS") == 0) {
    if (IsPlayer(thing)) {
      old = atr_get_noparent(thing, "ALIAS");
      tbuf1[0] = '\0';
      if (old) {
        /* Old alias - we're allowed to change to a different case */
        strcpy(tbuf1, atr_value(old));
        if (s && !*s) {
          notify_format(player, T("'%s' is not a valid alias."), s);
          return -1;
        }
        if (s && strcasecmp(s, tbuf1)) {
          enum opa_error opae_res = ok_player_alias(s, player, thing);
          switch (opae_res) {
          case OPAE_INVALID:
            notify_format(player, T("'%s' is not a valid alias."), s);
            return -1;
          case OPAE_TOOMANY:
            notify_format(player, T("'%s' contains too many aliases."), s);
            return -1;
          case OPAE_NULL:
            notify_format(player, T("Null aliases are not valid."));
            return -1;
          case OPAE_SUCCESS:
            break;
          }
        }
      } else {
        /* No old alias */
        if (s && *s) {
          enum opa_error opae_res = ok_player_alias(s, player, thing);
          switch (opae_res) {
          case OPAE_INVALID:
            notify_format(player, T("'%s' is not a valid alias."), s);
            return -1;
          case OPAE_TOOMANY:
            notify_format(player, T("'%s' contains too many aliases."), s);
            return -1;
          case OPAE_NULL:
            notify_format(player, T("Null aliases are not valid."));
            return -1;
          case OPAE_SUCCESS:
            break;
          }
        }
      }
    } else if (IsExit(thing) && s && *s) {
      char *alias, *aliases;

      strcpy(tbuf1, s);
      aliases = tbuf1;
      while ((alias = split_token(&aliases, ';')) != NULL) {
        if (!ok_name(alias, 1)) {
          notify_format(player, T("'%s' is not a valid exit name."), alias);
          return -1;
        }
      }
    }
  } else if (s && *s &&
             (!strcmp(name, "FORWARDLIST") ||
              !strcmp(name, "MAILFORWARDLIST") ||
              !strcmp(name, "DEBUGFORWARDLIST"))) {
    /* You can only set this to dbrefs of things you're allowed to
     * forward to. If you get one wrong, we puke.
     */
    char *fwdstr, *curr;
    dbref fwd;
    strcpy(tbuf1, s);
    fwdstr = trim_space_sep(tbuf1, ' ');
    while ((curr = split_token(&fwdstr, ' ')) != NULL) {
      if (!is_objid(curr)) {
        notify_format(player, T("%s should contain only dbrefs."), name);
        return -1;
      }
      fwd = parse_objid(curr);
      if (!GoodObject(fwd) || IsGarbage(fwd)) {
        notify_format(player, T("Invalid dbref #%d in %s."), fwd, name);
        return -1;
      }
      if ((!strcmp(name, "FORWARDLIST") || !strcmp(name, "DEBUGFORWARDLIST")) &&
          !Can_Forward(thing, fwd)) {
        notify_format(player, T("I don't think #%d wants to hear from %s."),
                      fwd, AName(thing, AN_SYS, NULL));
        return -1;
      }
      if (!strcmp(name, "MAILFORWARDLIST") && !Can_MailForward(thing, fwd)) {
        notify_format(player, T("I don't think #%d wants %s's mail."), fwd,
                      AName(thing, AN_SYS, NULL));
        return -1;
      }
    }
    /* If you made it here, all your dbrefs were ok */
  }

  /* For ENUM and RLIMIT */
  new = check_attr_value(player, name, s);
  if (s && !new) {
    /* Invalid set - Return, don't clear. */
    return -1;
  } else if (new) {
    s = new;
  }

  was_hearer = Hearer(thing);
  was_listener = Listener(thing);
  res = s ? atr_add(thing, name, s, player, (flags & 0x02) ? AF_NOPROG : 0)
          : atr_clr(thing, name, player);
  switch (res) {
  case AE_SAFE:
    notify_format(player, T("Attribute %s is SAFE. Set it !SAFE to modify it."),
                  name);
    return 0;
  case AE_TREE:
    if (!s) {
      notify_format(
        player,
        T("Unable to remove '%s' because of a protected tree attribute."),
        name);
      return 0;
    } else {
      notify_format(player,
                    T("Unable to set '%s' because of a failure to "
                      "create a needed parent attribute."),
                    name);
      return 0;
    }
  case AE_BADNAME:
    notify(player, T("That's not a very good name for an attribute."));
    return 0;
  case AE_ERROR:
    if (*missing_name) {
      if (s && (EMPTY_ATTRS || *s))
        notify_format(player, T("You must set %s first."), missing_name);
      else
        notify_format(player,
                      T("%s is a branch attribute; remove its children first."),
                      missing_name);
    } else
      notify(player, T("That attribute cannot be changed by you."));
    return 0;
  case AE_TOOMANY:
    notify(player, T("Too many attributes on that object to add another."));
    return 0;
  case AE_NOTFOUND:
    notify(player, T("No such attribute to reset."));
    return 0;
  case AE_OKAY:
    /* Success */
    break;
  }
  if (!strcmp(name, "ALIAS") && IsPlayer(thing)) {
    reset_player_list(thing, Name(thing), s);
    if (s && *s)
      notify(player, T("Alias set."));
    else
      notify(player, T("Alias removed."));
    return 1;
  } else if (!strcmp(name, "LISTEN")) {
    if (IsRoom(thing))
      announceloc = thing;
    else {
      announceloc = Location(thing);
    }
    if (GoodObject(announceloc)) {
      char *bp = tbuf1;
      if (!s && !was_listener && !Hearer(thing)) {
        safe_format(tbuf1, &bp, T("%s loses its ears and becomes deaf."),
                    AName(thing, AN_SAY, NULL));
        *bp = '\0';
        notify_except(thing, announceloc, thing, tbuf1, NA_INTER_PRESENCE);
      } else if (s && !was_hearer && !was_listener) {
        safe_format(tbuf1, &bp, T("%s grows ears and can now hear."),
                    AName(thing, AN_SAY, NULL));
        *bp = '\0';
        notify_except(thing, announceloc, thing, tbuf1, NA_INTER_PRESENCE);
      }
    }
  }
  if ((flags & 0x01) && !AreQuiet(player, thing)) {
    old = atr_get(thing, name);
    if (!old || !AF_Quiet(old)) {
      notify_format(player, "%s/%s - %s.", AName(thing, AN_SYS, NULL), name,
                    s ? T("Set") : T("Cleared"));
    }
  }
  return 1;
}

enum atrlock_status { ATRLOCK_CHECK = 0, ATRLOCK_LOCK, ATRLOCK_UNLOCK };

/** Lock or unlock an attribute.
 * Attribute locks are largely obsolete and should be deprecated,
 * but this is the code that does them.
 * \param player the enactor.
 * \param src the object/attribute, as a string.
 * \param action the desired lock status ('on' or 'off').
 */
void
do_atrlock(dbref player, const char *src, const char *action)
{
  dbref thing;
  char *target, *attrib;
  ATTR *ptr;
  enum atrlock_status status = ATRLOCK_CHECK;
  char abuff[BUFFER_LEN];

  if (action && *action) {
    if (!strcasecmp(action, "on") || !strcasecmp(action, "yes") ||
        !strcasecmp(action, "1"))
      status = ATRLOCK_LOCK;
    else if (!strcasecmp(action, "off") || !strcasecmp(action, "no") ||
             !strcasecmp(action, "0"))
      status = ATRLOCK_UNLOCK;
    else {
      notify(player, T("Invalid argument."));
      return;
    }
  }

  if (!src || !*src) {
    notify(player, T("You need to give an object/attribute pair."));
    return;
  }

  target = mush_strdup(src, "atrlock.string");

  if (!(attrib = strchr(target, '/')) || !(*(attrib + 1))) {
    notify(player, T("You need to give an object/attribute pair."));
    mush_free(target, "atrlock.string");
    return;
  }

  *attrib++ = '\0';
  if ((thing = noisy_match_result(player, target, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING) {
    mush_free(target, "atrlock.string");
    return;
  }
  if (!controls(player, thing)) {
    notify(player, T("Permission denied."));
    mush_free(target, "atrlock.string");
    return;
  }

  ptr = atr_get_noparent(thing, strupper_r(attrib, abuff, sizeof abuff));
  mush_free(target, "atrlock.string");
  if (!ptr || !Can_Read_Attr(player, thing, ptr)) {
    notify(player, T("No such attribute."));
    return;
  }

  if (status == ATRLOCK_CHECK) {
    if (AF_Locked(ptr))
      notify(player, T("That attribute is locked."));
    else
      notify(player, T("That attribute is unlocked."));
    return;
  } else if (!Can_Write_Attr(player, thing, ptr)) {
    notify(player,
           T("You need to be able to set the attribute to change its lock."));
    return;
  } else {
    if (status == ATRLOCK_LOCK) {
      AL_FLAGS(ptr) |= AF_LOCKED;
      AL_CREATOR(ptr) = Owner(player);
      notify(player, T("Attribute locked."));
    } else if (status == ATRLOCK_UNLOCK) {
      AL_FLAGS(ptr) &= ~AF_LOCKED;
      notify(player, T("Attribute unlocked."));
    } else {
      notify(player, T("Invalid status."));
      return;
    }
  }
}

/** Change ownership of an attribute.
 * \verbatim
 * This function is used to implement @atrchown.
 * \endverbatim
 * \param player the enactor, for permission checking.
 * \param xarg1 the object/attribute to change, as a string.
 * \param arg2 the name of the new owner (or "me").
 * \retval 0 failed to change owner.
 * \retval 1 successfully changed owner.
 */
int
do_atrchown(dbref player, const char *xarg1, const char *arg2)
{
  dbref thing, new_owner;
  char *p, *arg1;
  ATTR *ptr;
  char abuff[BUFFER_LEN];
  int retval = 0;

  if (!xarg1 || !*xarg1) {
    notify(player, T("You need to give an object/attribute pair."));
    return 0;
  }

  arg1 = mush_strdup(xarg1, "atrchown.string");

  if (!(p = strchr(arg1, '/')) || !(*(p + 1))) {
    notify(player, T("You need to give an object/attribute pair."));
    retval = 0;
    goto cleanup;
  }
  *p++ = '\0';
  if ((thing = noisy_match_result(player, arg1, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING) {
    retval = 0;
    goto cleanup;
  }
  if (!controls(player, thing)) {
    notify(player, T("Permission denied."));
    retval = 0;
    goto cleanup;
  }

  if (!(arg2 && *arg2) || !strcasecmp(arg2, "me"))
    new_owner = player;
  else
    new_owner = lookup_player(arg2);
  if (new_owner == NOTHING) {
    notify(player, T("I can't find that player"));
    retval = 0;
    goto cleanup;
  }

  ptr = atr_get_noparent(thing, strupper_r(p, abuff, sizeof abuff));
  if (ptr && Can_Read_Attr(player, thing, ptr)) {
    if (Can_Write_Attr(player, thing, ptr)) {
      if (new_owner != Owner(player) && !Wizard(player)) {
        notify(player, T("You can only chown an attribute to yourself."));
        retval = 0;
        goto cleanup;
      }
      AL_CREATOR(ptr) = Owner(new_owner);
      notify(player, T("Attribute owner changed."));
      retval = 1;
      goto cleanup;
    } else {
      notify(player, T("You don't have the permission to chown that."));
      retval = 0;
      goto cleanup;
    }
  } else {
    notify(player, T("No such attribute."));
    retval = 0;
  }
cleanup:
  mush_free(arg1, "atrchown.string");
  return retval;
}

/** Delete one attribute, deallocating its name and data.
 * <strong>Does not update the owning object's attribute list or
 * attribute count. That is the caller's responsibility.</strong>
 *
 * \param thing the object the attribute is on.
 * \param a the attribute to free
 */
static void
atr_free_one(dbref thing, ATTR *a)
{
  ptrdiff_t pos;

  if (!a)
    return;
  st_delete(AL_NAME(a), &atr_names);
  if (a->data)
    chunk_delete(a->data);

  pos = a - List(thing);
  atr_move_up(thing, pos);
  AttrCount(thing) -= 1;
}

/** Return the compressed data for an attribute.
 * This is a chokepoint function for accessing the chunk data.
 * \param atr the attribute struct from which to get the data reference.
 * \return a pointer to the compressed data, in a static buffer.
 */
char const *
atr_get_compressed_data(ATTR *atr)
{
  static char buffer[BUFFER_LEN * 2];
  static char const empty_string[] = {0};
  size_t len;
  if (!atr->data)
    return empty_string;
  len = chunk_fetch(atr->data, buffer, sizeof(buffer));
  if (len > sizeof(buffer))
    return empty_string;
  buffer[len] = '\0';
  return buffer;
}

/** Return the uncompressed data for an attribute in a static buffer.
 * This is a wrapper function, to centralize the use of compression/
 * decompression on attributes.
 * \param atr the attribute struct from which to get the data reference.
 * \return a pointer to the uncompressed data, in a static buffer.
 */
char *
atr_value(ATTR *atr)
{
  return uncompress(atr_get_compressed_data(atr));
}

/** Return the uncompressed data for an attribute in a dynamic buffer.
 * This is a wrapper function, to centralize the use of compression/
 * decompression on attributes. The result must be mush_free()'d.
 * \param atr the attribute struct from which to get the data reference.
 * \param check string to label allocation with.
 * \return a pointer to the uncompressed data, in a dynamic buffer.
 */
char *
safe_atr_value(ATTR *atr, char *check)
{
  add_check(check);
  return safe_uncompress(atr_get_compressed_data(atr));
}
