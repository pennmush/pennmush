/**
 * \file attrib.c
 *
 * \brief Manipulate attributes on objects.
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <string.h>
#include <ctype.h>
#include "conf.h"
#include "externs.h"
#include "chunk.h"
#include "attrib.h"
#include "dbdefs.h"
#include "match.h"
#include "parse.h"
#include "htab.h"
#include "privtab.h"
#include "mymalloc.h"
#include "strtree.h"
#include "flags.h"
#include "mushdb.h"
#include "lock.h"
#include "log.h"
#include "sort.h"
#include "confmagic.h"

#ifdef WIN32
#pragma warning( disable : 4761)        /* disable warning re conversion */
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

/** How many attributes go in a "page" of attribute memory? */
#define ATTRS_PER_PAGE (200)

slab *attrib_slab = NULL;

static int real_atr_clr(dbref thinking, char const *atr, dbref player,
                        int we_are_wiping);

static ATTR *alloc_atr(const void *hint);
static void free_atr(ATTR *);
static void atr_free_one(ATTR *);
static ATTR *find_atr_pos_in_list(ATTR ***pos, char const *name);
static atr_err can_create_attr(dbref player, dbref obj, char const *atr_name,
                               uint32_t flags);
static ATTR *find_atr_in_list(ATTR *atr, char const *name);
static ATTR *atr_get_with_parent(dbref obj, char const *atrname, dbref *parent,
                                 int cmd);
static bool can_debug(dbref player, dbref victim);

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
  const unsigned char *a;
  int len = 0;
  if (!s || !*s)
    return 0;
  if (*s == '`')
    return 0;
  if (strstr(s, "``"))
    return 0;
  for (a = (const unsigned char *) s; *a; a++, len++)
    if (!atr_name_table[*a])
      return 0;
  if (*(s + len - 1) == '`')
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
  char const *name, *n2;
  size_t len;

  name = AL_NAME(branch);
  len = strlen(name);
  for (branch = AL_NEXT(branch); branch; branch = AL_NEXT(branch)) {
    n2 = AL_NAME(branch);
    if (strlen(n2) <= len)
      return NULL;
    if (n2[len] == '`') {
      if (!strncmp(n2, name, len))
        return branch;
      else
        return NULL;
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
  char const *name, *n2;
  size_t len;
  ATTR *prev;

  name = AL_NAME(branch);
  len = strlen(name);
  prev = branch;
  for (branch = AL_NEXT(branch); branch; branch = AL_NEXT(branch)) {
    n2 = AL_NAME(branch);
    if (strlen(n2) <= len) {
      return NULL;
    }
    if (n2[len] == '`') {
      if (!strncmp(n2, name, len))
        return prev;
      else
        return NULL;
    }
    prev = branch;
  }
  return NULL;
}


/** Scan an attribute list for an attribute with the specified name.
 * This continues from whatever start point is passed in.
 *
 * Attributes are stored as sorted linked lists. This means that
 * search is O(N) in the worst case. An unsuccessful search is usually
 * better than that, because we don't have to look at every attribute
 * unless you're looking for something greater than all the attributes
 * on the object. There are a couple of possibilities I've been
 * mulling over for... um... years... about ways to improve this.
 *
 * Option 1 is to change the data structure. I'd use a hybrid between
 * a standard linked list and a skip list. Most objects have under 5
 * attributes on them. With this few, a linear linked list is
 * fine. When more attribute are added, though, it would turn into a
 * skip list, with all current attributes having a depth of 1, and
 * further attributes having a randomly chosen depth with a cap of,
 * say, 5 (I'll have to work out the math to find the optimum
 * size). This will provide O(lg N) searches on objects with lots of
 * attributes and yet not take up lots of extra memory on objects with
 * only a few attributes -- a problem with using a tree structure. All
 * the code for this is in my head; I just need to sit down and write
 * it.
 *
 * Option 2 is to speed up the current lookup. There are a lot of
 * string comparisions that we don't strictly need: All attributes
 * with the same name use the same underlying storage from a string
 * pool. You can look up an attribute name and then just compare
 * pointers, saving a lot of calls to strcoll(). The string pool is
 * implemented using a red-black tree, so it needs O(lg P) string
 * comparisions + O(N) pointer equality comparisions (P is the number
 * of unique attribute names in the pool, N the number of attributes
 * on the object). Hmm. I'm not so sure that's much of an improvement
 * after all...  Let's try it out anyways and see what happens. Don't
 * expect this to be permanent, though.
 *
 * \param atr the start of the list to search from \param name the
 * attribute name to look for \return the matching attribute, or NULL
 */
static ATTR *
find_atr_in_list(ATTR *atr, char const *name)
{
#define ATR_PTR_CMP
#ifdef ATR_PTR_CMP
  /* New way; pointer comparisions */
  const char *memoized;

  memoized = st_find(name, &atr_names);
  if (!memoized)                /* This attribute name doesn't exist on any object */
    return NULL;

  while (atr) {
    if (AL_NAME(atr) == memoized)
      return atr;
#if 0
    /* Unfortunately, this will break under many locales, since
       attribute list sorting uses strcoll() and not strcmp().  */
    else if (*memoized < *AL_NAME(atr))
      return NULL;              /* Can't be any of the remaining attributes */
#endif
    else
      atr = AL_NEXT(atr);
  }
#else
  /* Old way; lots of string comparisions */
  int comp;

  while (atr) {
    comp = strcoll(name, AL_NAME(atr));
    if (comp < 0)
      return NULL;
    if (comp == 0)
      return atr;
    atr = AL_NEXT(atr);
  }
#endif

  return NULL;
}

/** Find the place to insert/delete an attribute with the specified name.
 * \param pos a pointer to the ATTR ** holding the list position
 * \param name the attribute name to look for
 * \return the matching attribute, or NULL if no matching attribute
 */
static ATTR *
find_atr_pos_in_list(ATTR ***pos, char const *name)
{
  int comp;

  while (**pos) {
    comp = compare_attr_names(name, AL_NAME(**pos));
    if (comp < 0)
      return NULL;
    if (comp == 0)
      return **pos;
    *pos = &AL_NEXT(**pos);
  }

  return NULL;
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
    cansee = (Visual(obj) &&
              eval_lock(PLAYER_START, obj, Examine_Lock) &&
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
      !(cansee ||
        (AF_Visual(atr) && (!AF_Nearby(atr) || canlook)) ||
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
    atr = List(target);
    /* Check along the branch for permissions... */
    for (p = strchr(name, '`'); p; p = strchr(p + 1, '`')) {
      *p = '\0';
      atr = find_atr_in_list(atr, name);
      if (!atr || (target != obj && AF_Private(atr))) {
        *p = '`';
        goto continue_target;
      }
      if (AF_Internal(atr) || AF_Mdark(atr) ||
          !(cansee ||
            (AF_Visual(atr) && (!AF_Nearby(atr) || canlook)) ||
            (!visible && !Mistrust(player) &&
             (Owner(AL_CREATOR(atr)) == Owner(player)))))
        return 0;
      *p = '`';
    }

    /* Now actually find the attribute. */
    atr = find_atr_in_list(atr, name);
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
#define Cannot_Write_This_Attr(p,a,s) \
  (!God((p)) && \
   (AF_Internal((a)) || \
    ((s) && AF_Safe((a))) || \
    !(Wizard((p)) || \
      (!AF_Wizard((a)) && \
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
  atr = List(obj);
  for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
    *p = '\0';
    atr = find_atr_in_list(atr, missing_name);
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
  ATTR *ptr = find_atr_in_list(List(thing), attrname);
  if (ptr)
    return Can_Write_Attr(player, thing, ptr);
  else
    return can_create_attr(player, thing, attrname, 0) == AE_OKAY;
}

/** Utility define for atr_add and can_create_attr */
#define set_default_flags(atr,flags) \
  do { \
    ATTR *std = atr_match(AL_NAME((atr))); \
    if (std && !strcmp(AL_NAME(std), AL_NAME((atr)))) { \
      AL_FLAGS(atr) = AL_FLAGS(std) | flags; \
    } else { \
        AL_FLAGS(atr) = flags; \
    } \
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
      atr = find_atr_in_list(atr, missing_name);
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

/** Do the work of creating the attribute entry on an object.
 * This doesn't do any permissions checking.  You should do that yourself.
 * \param thing the object to hold the attribute
 * \param atr_name the name for the attribute
 */
static ATTR *
create_atr(dbref thing, char const *atr_name, const ATTR *hint)
{
  ATTR *ptr, **ins;
  char const *name;

  /* put the name in the string table */
  name = st_insert(atr_name, &atr_names);
  if (!name)
    return NULL;

  /* allocate a new page, if needed */
  ptr = alloc_atr(hint);
  if (ptr == NULL) {
    st_delete(name, &atr_names);
    return NULL;
  }

  /* initialize atr */
  AL_NAME(ptr) = name;
  ptr->data = NULL_CHUNK_REFERENCE;
  AL_FLAGS(ptr) = 0;

  /* link it in */
  ins = &List(thing);
  (void) find_atr_pos_in_list(&ins, AL_NAME(ptr));
  AL_NEXT(ptr) = *ins;
  *ins = ptr;
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

  strcpy(root_name, atr);
  if ((p = strrchr(root_name, '`'))) {
    ATTR *root = NULL;
    *p = '\0';
    root = find_atr_in_list(List(thing), root_name);
    if (!root) {
      if (!makeroots)
        return;
      do_rawlog(LT_ERR, "Missing root attribute '%s' on object #%d!\n",
                root_name, thing);
      root = create_atr(thing, root_name, List(thing));
      set_default_flags(root, 0);
      AL_FLAGS(root) |= AF_ROOT;
      AL_CREATOR(root) = player;
      if (!EMPTY_ATTRS) {
        unsigned char *t = compress(" ");
        if (!t) {
          mush_panic("Unable to allocate memory in atr_new_add()!");
        }
        root->data = chunk_create(t, u_strlen(t), 0);
        free(t);
      }
    } else {
      if (!(AL_FLAGS(root) & AF_ROOT))  /* Upgrading old database */
        AL_FLAGS(root) |= AF_ROOT;
    }
  }

  ptr = create_atr(thing, atr, List(thing));
  if (!ptr)
    return;

  AL_FLAGS(ptr) = flags;
  AL_FLAGS(ptr) &= ~AF_COMMAND & ~AF_LISTEN;
  AL_CREATOR(ptr) = player;

  /* replace string with new string */
  if (!s || !*s) {
    /* nothing */
  } else {
    unsigned char *t = compress(s);
    if (!t)
      return;

    ptr->data = chunk_create(t, u_strlen(t), derefs);
    free(t);

    if (*s == '$')
      AL_FLAGS(ptr) |= AF_COMMAND;
    if (*s == '^')
      AL_FLAGS(ptr) |= AF_LISTEN;
  }
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
  ptr = find_atr_in_list(List(thing), atr);

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
    ptr = List(thing);
    for (p = strchr(missing_name, '`'); p; p = strchr(p + 1, '`')) {
      *p = '\0';

      root = find_atr_in_list(ptr, missing_name);

      if (!root) {
        root = create_atr(thing, missing_name, ptr);
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
          unsigned char *t = compress(" ");
          if (!t)
            mush_panic("Unable to allocate memory in atr_add()!");
          root->data = chunk_create(t, u_strlen(t), 0);
          free(t);
        }
      } else
        AL_FLAGS(root) |= AF_ROOT;

      *p = '`';
    }

    ptr = create_atr(thing, atr, root ? root : List(thing));
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
    unsigned char *t = compress(s);
    if (!t) {
      ptr->data = NULL_CHUNK_REFERENCE;
      return AE_ERROR;
    }
    ptr->data = chunk_create(t, u_strlen(t), 0);
    free(t);

    if (*s == '$')
      AL_FLAGS(ptr) |= AF_COMMAND;
    if (*s == '^')
      AL_FLAGS(ptr) |= AF_LISTEN;
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
  ATTR *sub, *next = NULL, *prev;
  int skipped = 0;
  size_t len;
  const char *name, *n2;

  prev = atr_sub_branch_prev(root);
  if (!prev)
    return 1;

  name = AL_NAME(root);
  len = strlen(name);
  for (sub = AL_NEXT(prev); sub; sub = next) {
    n2 = AL_NAME(sub);
    if (strlen(n2) < (len + 1) || n2[len] != '`' || strncmp(n2, name, len))
      break;
    if (AL_FLAGS(sub) & AF_ROOT) {
      if (!atr_clear_children(player, thing, sub)) {
        skipped++;
        next = AL_NEXT(sub);
        prev = sub;
        continue;
      }
    }

    next = AL_NEXT(sub);

    if (!Can_Write_Attr(player, thing, sub)) {
      skipped++;
      prev = sub;
      continue;
    }

    /* Can safely delete attribute.  */
    AL_NEXT(prev) = next;
    atr_free_one(sub);
    AttrCount(thing)--;

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
  ATTR *ptr, **prev;
  int can_clear = 1;

  prev = &List(thing);
  ptr = find_atr_pos_in_list(&prev, atr);

  if (!ptr)
    return AE_NOTFOUND;

  if (ptr && AF_Safe(ptr))
    return AE_SAFE;
  if (!Can_Write_Attr(player, thing, ptr))
    return AE_ERROR;

  if ((AL_FLAGS(ptr) & AF_ROOT) && !we_are_wiping)
    return AE_TREE;

  /* We only hit this if wiping. */
  if (AL_FLAGS(ptr) & AF_ROOT)
    can_clear = atr_clear_children(player, thing, ptr);

  if (can_clear) {
    char *p;
    char root_name[ATTRIBUTE_NAME_LIMIT + 1];

    strcpy(root_name, AL_NAME(ptr));

    if (!IsPlayer(thing) && !AF_Nodump(ptr))
      ModTime(thing) = mudtime;
    *prev = AL_NEXT(ptr);
    atr_free_one(ptr);
    AttrCount(thing)--;

    /* If this was the only leaf of a tree, clear the AF_ROOT flag from
     * the parent. */
    if ((p = strrchr(root_name, '`'))) {
      ATTR *root;
      *p = '\0';

      root = find_atr_in_list(List(thing), root_name);
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
      atr = List(target);

      /* If we're looking at a parent/ancestor, then we
       * need to check the branch path for privacy. We also
       * need to check the branch path if we're looking for no_command */
      if (target != obj || cmd) {
        for (p = strchr(name, '`'); p; p = strchr(p + 1, '`')) {
          *p = '\0';
          atr = find_atr_in_list(atr, name);
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
      atr = find_atr_in_list(atr, name);
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
    if (!atr || !strcmp(name, AL_NAME(atr)))
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
  ptr = find_atr_in_list(List(thing), atr);
  if (ptr)
    return ptr;

  ptr = atr_match(atr);
  if (!ptr || !strcmp(atr, AL_NAME(ptr)))
    return NULL;
  atr = AL_NAME(ptr);

  /* try alias */
  ptr = find_atr_in_list(List(thing), atr);
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
 * \param mortal only fetch mortal-visible attributes?
 * \param regexp is name a regexp, rather than a glob pattern?
 * \param func the function to call for each matching attribute.
 * \param args additional arguments to pass to the function.
 * \return the sum of the return values of the functions called.
 */
int
atr_iter_get(dbref player, dbref thing, const char *name, int mortal,
             int regexp, aig_func func, void *args)
{
  ATTR *ptr, **indirect;
  int result;
  size_t len;

  result = 0;
  if (!name || !*name) {
    if (regexp) {
      regexp = 0;
      name = "**";
    } else {
      name = "*";
    }
  }
  len = strlen(name);

  /* Must check name[len-1] first as wildcard_count() can destructively modify name */
  if (!regexp && name[len - 1] != '`' && wildcard_count((char *) name, 1) != -1) {
    ptr = atr_get_noparent(thing, strupper(name));
    if (ptr && (mortal ? Is_Visible_Attr(thing, ptr)
                : Can_Read_Attr(player, thing, ptr)))
      result = func(player, thing, NOTHING, name, ptr, args);
  } else {
    indirect = &List(thing);
    while (*indirect) {
      ptr = *indirect;
      if ((mortal ? Is_Visible_Attr(thing, ptr)
           : Can_Read_Attr(player, thing, ptr))
          && (regexp ? quick_regexp_match(name, AL_NAME(ptr), 0) :
              atr_wild(name, AL_NAME(ptr))))
        result += func(player, thing, NOTHING, name, ptr, args);
      if (ptr == *indirect)
        indirect = &AL_NEXT(ptr);
    }
  }

  return result;
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
 * \param mortal only fetch mortal-visible attributes?
 * \param regexp is name a regexp, rather than a glob pattern?
 * \return the count of matching attributes
 */
int
atr_pattern_count(dbref player, dbref thing, const char *name,
                  int doparent, int mortal, int regexp)
{
  ATTR *ptr, **indirect;
  int result;
  size_t len;
  dbref parent = NOTHING;

  result = 0;
  if (!name || !*name) {
    if (regexp) {
      regexp = 0;
      name = "**";
    } else {
      name = "*";
    }
  }
  len = strlen(name);

  /* Must check name[len-1] first as wildcard_count() can destructively modify name */
  if (!regexp && name[len - 1] != '`' && wildcard_count((char *) name, 1) != -1) {
    parent = thing;
    if (doparent)
      ptr = atr_get_with_parent(thing, strupper(name), &parent, 0);
    else
      ptr = atr_get_noparent(thing, strupper(name));
    if (ptr && (mortal ? Is_Visible_Attr(parent, ptr)
                : Can_Read_Attr(player, parent, ptr)))
      result += 1;
  } else {
    StrTree seen;
    int parent_depth;
    st_init(&seen, "AttrsSeenTree");
    for (parent_depth = MAX_PARENTS + 1, parent = thing;
         (parent_depth-- && parent != NOTHING) &&
         (doparent || (parent == thing)); parent = Parent(parent)) {
      indirect = &List(parent);
      while (*indirect) {
        ptr = *indirect;
        if (!st_find(AL_NAME(ptr), &seen)) {
          st_insert(AL_NAME(ptr), &seen);
          if ((parent == thing) || !AF_Private(ptr)) {
            if ((mortal ? Is_Visible_Attr(parent, ptr)
                 : Can_Read_Attr(player, parent, ptr))
                && (regexp ? quick_regexp_match(name, AL_NAME(ptr), 0) :
                    atr_wild(name, AL_NAME(ptr))))
              result += 1;
          }
        }
        if (ptr == *indirect)
          indirect = &AL_NEXT(ptr);
      }
    }
    st_flush(&seen);
  }

  return result;
}

/** Apply a function to a set of attributes, including inherited ones.
 * This function applies another function to a set of attributes on an
 * object specified by a (wildcarded) pattern to match against the
 * attribute name on an object or its parents.
 * \param player the enactor.
 * \param thing the object containing the attribute.
 * \param name the pattern to match against the attribute name.
 * \param mortal only fetch mortal-visible attributes?
 * \param regexp is name a regexp pattern, rather than a glob pattern?
 * \param func the function to call for each matching attribute, with
 *  a pointer to the dbref of the object the attribute is really on passed
 *  as the function's args argument.
 * \param args arguments passed to the func
 * \return the sum of the return values of the functions called.
 */
int
atr_iter_get_parent(dbref player, dbref thing, const char *name, int mortal,
                    int regexp, aig_func func, void *args)
{
  ATTR *ptr, **indirect;
  int result;
  size_t len;
  dbref parent = NOTHING;

  result = 0;
  if (!name || !*name) {
    if (regexp) {
      regexp = 0;
      name = "**";
    } else {
      name = "*";
    }
  }
  len = strlen(name);

  /* Must check name[len-1] first as wildcard_count() can destructively modify name */
  if (!regexp && name[len - 1] != '`' && wildcard_count((char *) name, 1) != -1) {
    ptr = atr_get_with_parent(thing, strupper(name), &parent, 0);
    if (ptr && (mortal ? Is_Visible_Attr(parent, ptr)
                : Can_Read_Attr(player, parent, ptr)))
      result = func(player, thing, parent, name, ptr, args);
  } else {
    StrTree seen;
    int parent_depth;
    st_init(&seen, "AttrsSeenTree");
    for (parent_depth = MAX_PARENTS + 1, parent = thing;
         parent_depth-- && parent != NOTHING; parent = Parent(parent)) {
      indirect = &List(parent);
      while (*indirect) {
        ptr = *indirect;
        if (!st_find(AL_NAME(ptr), &seen)) {
          st_insert(AL_NAME(ptr), &seen);
          if ((parent == thing) || !AF_Private(ptr)) {
            if ((mortal ? Is_Visible_Attr(parent, ptr)
                 : Can_Read_Attr(player, parent, ptr))
                && (regexp ? quick_regexp_match(name, AL_NAME(ptr), 0) :
                    atr_wild(name, AL_NAME(ptr))))
              result += func(player, thing, parent, name, ptr, args);
          }
        }
        if (ptr == *indirect)
          indirect = &AL_NEXT(ptr);
      }
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

  if (!List(thing))
    return;

  if (!IsPlayer(thing))
    ModTime(thing) = mudtime;

  while ((ptr = List(thing))) {
    List(thing) = AL_NEXT(ptr);

    if (ptr->data)
      chunk_delete(ptr->data);
    st_delete(AL_NAME(ptr), &atr_names);

    free_atr(ptr);
  }
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
  List(dest) = NULL;
  for (ptr = List(source); ptr; ptr = AL_NEXT(ptr))
    if (!AF_Nocopy(ptr)
        && (AttrCount(dest) < max_attrs)) {
      atr_new_add(dest, AL_NAME(ptr), atr_value(ptr), AL_CREATOR(ptr),
                  AL_FLAGS(ptr), AL_DEREFS(ptr), 0);
    }
}

/** Structure for keeping track of which attributes have appeared
 * on children when doing command matching. */
typedef struct used_attr {
  struct used_attr *next;       /**< Next attribute in list */
  char const *name;             /**< The name of the attribute */
  int no_prog;                  /**< Was it AF_NOPROG */
} UsedAttr;

/** Find an attribute in the list of seen attributes.
 * Since attributes are checked in collation order, the pointer to the
 * list is updated to reflect the current search position.
 * For efficiency of insertions, the pointer used is a trailing pointer,
 * pointing at the pointer to the next used struct.
 * To allow a useful return code, the pointer used is actually a pointer
 * to the pointer mentioned above.  Yes, I know three-star coding is bad,
 * but I have good reason, here.
 * \param prev the pointer to the pointer to the pointer to the next
 *             used attribute.
 * \param name the name of the attribute to look for.
 * \retval 0 the attribute was not in the list,
 *           **prev now points to the next atfer.
 * \retval 1 the attribute was in the list,
 *           **prev now points to the entry for it.
 */
static int
find_attr(UsedAttr ***prev, char const *name)
{
  int comp;

  comp = 1;
  while (**prev) {
    comp = compare_attr_names(name, prev[0][0]->name);
    if (comp <= 0)
      break;
    *prev = &prev[0][0]->next;
  }
  return comp == 0;
}

/** Insert an attribute in the list of seen attributes.
 * Since attributes are inserted in collation order, an updated insertion
 * point is returned (so subsequent calls don't have to go hunting as far).
 * \param prev the pointer to the pointer to the attribute list.
 * \param name the name of the attribute to insert.
 * \param no_prog the AF_NOPROG value from the attribute.
 * \return the pointer to the pointer to the next attribute after
 *         the one inserted.
 */
static UsedAttr **
use_attr(UsedAttr **prev, char const *name, uint32_t no_prog)
{
  int found;
  UsedAttr *used;

  found = find_attr(&prev, name);
  if (!found) {
    used = mush_malloc(sizeof *used, "used_attr");
    used->next = *prev;
    used->name = name;
    used->no_prog = 0;
    *prev = used;
  }
  prev[0]->no_prog |= no_prog;
  /* do_rawlog(LT_TRACE, "Recorded %s: %d -> %d", name,
     no_prog, prev[0]->no_prog); */
  return &prev[0]->next;
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
  aval = safe_atr_value(a);
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
  free(aval);
  return success;
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
 * \return number of attributes that matched, or 0
 */
int
atr_comm_match(dbref thing, dbref player, int type, int end, char const *str,
               int just_match, int check_locks,
               char *atrname, char **abp, int show_child, dbref *errobj,
               MQUE *from_queue, int queue_type)
{
  uint32_t flag_mask;
  ATTR *ptr;
  int parent_depth;
  char *args[10];
  PE_REGS *pe_regs;
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  char *s;
  int match, match_found;
  UsedAttr *used_list, **prev;
  ATTR *skip[ATTRIBUTE_NAME_LIMIT / 2];
  int skipcount;
  int i;
  int lock_checked = !check_locks;
  char match_space[BUFFER_LEN * 2];
  ssize_t match_space_len = BUFFER_LEN * 2;
  NEW_PE_INFO *pe_info;
  dbref current = thing, next = NOTHING;
  int parent_count = 0;

  /* check for lots of easy ways out */
  if (type != '$' && type != '^')
    return 0;
  if (check_locks && (!GoodObject(thing) || Halted(thing)
                      || (type == '$' && NoCommand(thing))))
    return 0;

  if (type == '$') {
    flag_mask = AF_COMMAND;
    parent_depth = GoodObject(Parent(thing));
  } else {
    flag_mask = AF_LISTEN;
    if (has_flag_by_name
        (thing, "LISTEN_PARENT", TYPE_PLAYER | TYPE_THING | TYPE_ROOM)) {
      parent_depth = GoodObject(Parent(thing));
    } else {
      parent_depth = 0;
    }
  }
  match = 0;
  used_list = NULL;
  prev = &used_list;

  pe_info = make_pe_info("pe_info-atr_comm_match");
  strcpy(pe_info->cmd_raw, str);
  strcpy(pe_info->cmd_evaled, str);

  skipcount = 0;
  do {
    next = parent_depth ?
      next_parent(thing, current, &parent_count, NULL) : NOTHING;
    prev = &used_list;

    /* do_rawlog(LT_TRACE, "Searching %s:", Name(current)); */
    for (ptr = List(current); ptr; ptr = AL_NEXT(ptr)) {
      if (skipcount && ptr == skip[skipcount - 1]) {
        size_t len = strrchr(AL_NAME(ptr), '`') - AL_NAME(ptr);
        while (AL_NEXT(ptr) && strlen(AL_NAME(AL_NEXT(ptr))) > len &&
               AL_NAME(AL_NEXT(ptr))[len] == '`') {
          ptr = AL_NEXT(ptr);
          /* do_rawlog(LT_TRACE, "  Skipping %s", AL_NAME(ptr)); */
        }
        skipcount--;
        continue;
      }
      if (current != thing) {
        /* Parent */
        if (AF_Private(ptr)) {
          /* do_rawlog(LT_TRACE, "Private %s:", AL_NAME(ptr)); */
          skip[skipcount] = atr_sub_branch(ptr);
          if (skip[skipcount])
            skipcount++;
          continue;
        }
        if (find_attr(&prev, AL_NAME(ptr))) {
          /* do_rawlog(LT_TRACE, "Found %s:", AL_NAME(ptr)); */
          if (prev[0]->no_prog || AF_Noprog(ptr)) {
            skip[skipcount] = atr_sub_branch(ptr);
            if (skip[skipcount])
              skipcount++;
            prev[0]->no_prog = AF_NOPROG;
          }
          continue;
        }
      }
      if (GoodObject(next)) {
        prev = use_attr(prev, AL_NAME(ptr), AF_Noprog(ptr));
      }
      if (AF_Noprog(ptr)) {
        skip[skipcount] = atr_sub_branch(ptr);
        if (skip[skipcount])
          skipcount++;
        continue;
      }
      if (!(AL_FLAGS(ptr) & flag_mask)) {
        continue;
      }
      strcpy(tbuf1, atr_value(ptr));
      s = tbuf1;
      do {
        s = strchr(s + 1, end);
      } while (s && s[-1] == '\\');
      if (!s)
        continue;
      *s++ = '\0';
      if (type == '^' && !AF_Ahear(ptr)) {
        if ((thing == player && !AF_Mhear(ptr))
            || (thing != player && AF_Mhear(ptr)))
          continue;
      }

      if (AF_Regexp(ptr)) {
        /* Turn \: into : */
        char *from, *to;
        for (from = tbuf1, to = tbuf2; *from; from++, to++) {
          if (*from == '\\' && *(from + 1) == ':')
            from++;
          *to = *from;
        }
        *to = '\0';
      } else
        strcpy(tbuf2, tbuf1);

      match_found = 0;
      if (AF_Regexp(ptr)) {
        if (regexp_match_case_r(tbuf2 + 1, str, AF_Case(ptr), args, 10,
                                match_space, match_space_len, NULL)) {
          match_found = 1;
          match++;
        }
      } else {
        if (quick_wild_new(tbuf2 + 1, str, AF_Case(ptr))) {
          match_found = 1;
          match++;
          if (!just_match)
            wild_match_case_r(tbuf2 + 1, str, AF_Case(ptr), args, 10,
                              match_space, match_space_len, NULL);
        }
      }
      if (match_found) {
        /* We only want to do the lock check once, so that any side
         * effects in the lock are only performed once per utterance.
         * Thus, '$foo *r:' and '$foo b*:' on the same object will only
         * run the lock once for 'foo bar'. Locks are always checked on
         * the child, even when the attr is inherited.
         */
        if (!lock_checked) {
          lock_checked = 1;
          if ((type == '$'
               && !eval_lock_with(player, thing, Command_Lock, pe_info))
              || (type == '^'
                  && !eval_lock_with(player, thing, Listen_Lock, pe_info))
              || !eval_lock_with(player, thing, Use_Lock, pe_info)) {
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
          pe_regs = pe_regs_create(PE_REGS_ARG, "atr_comm_match");
          for (i = 0; i < 10; i++) {
            if (args[i]) {
              pe_regs_setenv_nocopy(pe_regs, i, args[i]);
            }
          }
          if (from_queue && (queue_type & ~QUEUE_DEBUG_PRIVS) != QUEUE_DEFAULT) {
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
            if (AF_NoDebug(ptr))
              queue_type |= QUEUE_NODEBUG;
            else if (AF_Debug(ptr))
              queue_type |= QUEUE_DEBUG;

            /* inplace queue */
            new_queue_actionlist_int(thing, player, player, s, from_queue,
                                     pe_flags, queue_type, pe_regs,
                                     tprintf("#%d/%s", thing, AL_NAME(ptr)));
          } else {
            /* Normal queue */
            parse_que_attr(thing, player, s, pe_regs, ptr,
                           (queue_type & QUEUE_DEBUG_PRIVS ?
                            can_debug(player, thing) : 0));
          }
          pe_regs_free(pe_regs);
        }
      }
    }
  } while ((current = next) != NOTHING);

  while (used_list) {
    UsedAttr *temp = used_list->next;
    mush_free(used_list, "used_attr");
    used_list = temp;
  }
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
 * \param from_queue parent queue to run the cmds inplace for, if queue_type says to do so
 * \param queue_type QUEUE_* flags telling how to run the matched commands
 * \retval 1 attribute matched.
 * \retval 0 attribute failed to match.
 */
int
one_comm_match(dbref thing, dbref player, const char *atr, const char *str,
               MQUE *from_queue, int queue_type)
{
  ATTR *ptr;
  char tbuf1[BUFFER_LEN];
  char tbuf2[BUFFER_LEN];
  char *s;
  PE_REGS *pe_regs;
  int i;
  char match_space[BUFFER_LEN * 2];
  char *args[10];
  ssize_t match_space_len = BUFFER_LEN * 2;

  /* check for lots of easy ways out */
  if (!GoodObject(thing) || Halted(thing) || NoCommand(thing))
    return 0;

  if (!(ptr = atr_get_with_parent(thing, atr, NULL, 1)))
    return 0;

  if (!AF_Command(ptr))
    return 0;

  strcpy(tbuf1, atr_value(ptr));
  s = tbuf1;
  do {
    s = strchr(s + 1, ':');
  } while (s && s[-1] == '\\');
  if (!s)
    return 0;
  *s++ = '\0';

  if (AF_Regexp(ptr)) {
    /* Turn \: into : */
    char *from, *to;
    for (from = tbuf1, to = tbuf2; *from; from++, to++) {
      if (*from == '\\' && *(from + 1) == ':')
        from++;
      *to = *from;
    }
    *to = '\0';
  } else
    strcpy(tbuf2, tbuf1);

  if (AF_Regexp(ptr) ?
      regexp_match_case_r(tbuf2 + 1, str, AF_Case(ptr), args, 10,
                          match_space, match_space_len, NULL) :
      wild_match_case_r(tbuf2 + 1, str, AF_Case(ptr), args,
                        10, match_space, match_space_len, NULL)) {
    char save_cmd_raw[BUFFER_LEN], save_cmd_evaled[BUFFER_LEN];
    int success = 1;
    NEW_PE_INFO *pe_info;

    if (from_queue && (queue_type & ~QUEUE_DEBUG_PRIVS) != QUEUE_DEFAULT) {
      pe_info = from_queue->pe_info;
      /* Save and reset %c/%u */
      strcpy(save_cmd_raw, from_queue->pe_info->cmd_raw);
      strcpy(save_cmd_evaled, from_queue->pe_info->cmd_evaled);
    } else {
      pe_info = make_pe_info("pe_info-one_comm_match");
    }
    strcpy(pe_info->cmd_raw, str);
    strcpy(pe_info->cmd_evaled, str);
    if (!eval_lock_clear(player, thing, Command_Lock, pe_info)
        || !eval_lock_clear(player, thing, Use_Lock, pe_info))
      success = 0;
    if (from_queue && (queue_type & ~QUEUE_DEBUG_PRIVS) != QUEUE_DEFAULT) {
      /* Restore */
      strcpy(from_queue->pe_info->cmd_raw, save_cmd_raw);
      strcpy(from_queue->pe_info->cmd_evaled, save_cmd_evaled);
    } else {
      free_pe_info(pe_info);
    }
    if (success) {
      pe_regs = pe_regs_create(PE_REGS_ARG, "one_comm_match");
      for (i = 0; i < 10; i++) {
        if (args[i]) {
          pe_regs_setenv_nocopy(pe_regs, i, args[i]);
        }
      }
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
        if (AF_NoDebug(ptr))
          queue_type |= QUEUE_NODEBUG;
        else if (AF_Debug(ptr))
          queue_type |= QUEUE_DEBUG;

        /* inplace queue */
        new_queue_actionlist_int(thing, player, player, s, from_queue,
                                 pe_flags, queue_type, pe_regs,
                                 tprintf("#%d/%s", thing, AL_NAME(ptr)));
      } else {
        /* Normal queue */
        parse_que_attr(thing, player, s, pe_regs, ptr,
                       (queue_type & QUEUE_DEBUG_PRIVS ?
                        can_debug(player, thing) : 0));
      }
      pe_regs_free(pe_regs);
    }
    return success;
  }
  return 0;
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
  strcpy(name, atr);
  upcasestr(name);
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
          int opae_res = ok_player_alias(s, player, thing);
          switch (opae_res) {
          case OPAE_INVALID:
            notify_format(player, T("'%s' is not a valid alias."), s);
            break;
          case OPAE_TOOMANY:
            notify_format(player, T("'%s' contains too many aliases."), s);
            break;
          case OPAE_NULL:
            notify_format(player, T("Null aliases are not valid."));
            break;
          }
          if (opae_res != OPAE_SUCCESS)
            return -1;
        }
      } else {
        /* No old alias */
        if (s && *s) {
          int opae_res = ok_player_alias(s, player, thing);
          switch (opae_res) {
          case OPAE_INVALID:
            notify_format(player, T("'%s' is not a valid alias."), s);
            break;
          case OPAE_TOOMANY:
            notify_format(player, T("'%s' contains too many aliases."), s);
            break;
          case OPAE_NULL:
            notify_format(player, T("Null aliases are not valid."));
            break;
          }
          if (opae_res != OPAE_SUCCESS)
            return -1;
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
  } else if (s && *s && (!strcmp(name, "FORWARDLIST")
                         || !strcmp(name, "MAILFORWARDLIST")
                         || !strcmp(name, "DEBUGFORWARDLIST"))) {
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
      if ((!strcmp(name, "FORWARDLIST") || !strcmp(name, "DEBUGFORWARDLIST"))
          && !Can_Forward(thing, fwd)) {
        notify_format(player, T("I don't think #%d wants to hear from %s."),
                      fwd, Name(thing));
        return -1;
      }
      if (!strcmp(name, "MAILFORWARDLIST") && !Can_MailForward(thing, fwd)) {
        notify_format(player, T("I don't think #%d wants %s's mail."), fwd,
                      Name(thing));
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
      notify_format(player,
                    T
                    ("Unable to remove '%s' because of a protected tree attribute."),
                    name);
      return 0;
    } else {
      notify_format(player,
                    T
                    ("Unable to set '%s' because of a failure to create a needed parent attribute."),
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
    reset_player_list(thing, NULL, tbuf1, NULL, s);
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
      orator = thing;
      if (!s && !was_listener && !Hearer(thing)) {
        safe_format(tbuf1, &bp, T("%s loses its ears and becomes deaf."),
                    Name(thing));
        *bp = '\0';
        notify_except(announceloc, thing, tbuf1, NA_INTER_PRESENCE);
      } else if (s && !was_hearer && !was_listener) {
        safe_format(tbuf1, &bp, T("%s grows ears and can now hear."),
                    Name(thing));
        *bp = '\0';
        notify_except(announceloc, thing, tbuf1, NA_INTER_PRESENCE);
      }
    }
  }
  if ((flags & 0x01) && !AreQuiet(player, thing))
    notify_format(player,
                  "%s/%s - %s.", Name(thing), name,
                  s ? T("Set") : T("Cleared"));
  return 1;
}

/** Lock or unlock an attribute.
 * Attribute locks are largely obsolete and should be deprecated,
 * but this is the code that does them.
 * \param player the enactor.
 * \param xarg1 the object/attribute, as a string.
 * \param arg2 the desired lock status ('on' or 'off').
 */
void
do_atrlock(dbref player, const char *xarg1, const char *arg2)
{
  dbref thing;
  char *p, *arg1;
  ATTR *ptr;
  int status;
  if (!arg2 || !*arg2)
    status = 0;
  else {
    if (!strcasecmp(arg2, "on")) {
      status = 1;
    } else if (!strcasecmp(arg2, "off")) {
      status = 2;
    } else
      status = 0;
  }

  if (!xarg1 || !*xarg1) {
    notify(player, T("You need to give an object/attribute pair."));
    return;
  }

  arg1 = mush_strdup(xarg1, "atrlock.string");

  if (!(p = strchr(arg1, '/')) || !(*(p + 1))) {
    notify(player, T("You need to give an object/attribute pair."));
    mush_free(arg1, "atrlock.string");
    return;
  }
  *p++ = '\0';
  if ((thing = noisy_match_result(player, arg1, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING) {
    mush_free(arg1, "atrlock.string");
    return;
  }
  if (!controls(player, thing)) {
    notify(player, T("Permission denied."));
    mush_free(arg1, "atrlock.string");
    return;
  }

  ptr = atr_get_noparent(thing, strupper(p));
  if (ptr && Can_Read_Attr(player, thing, ptr)) {
    if (!status) {
      notify_format(player, T("That attribute is %slocked."),
                    AF_Locked(ptr) ? "" : "un");
      mush_free(arg1, "atrlock.string");
      return;
    } else if (!Can_Write_Attr(player, thing, ptr)) {
      notify(player,
             T("You need to be able to set the attribute to change its lock."));
      mush_free(arg1, "atrlock.string");
      return;
    } else {
      if (status == 1) {
        AL_FLAGS(ptr) |= AF_LOCKED;
        AL_CREATOR(ptr) = Owner(player);
        notify(player, T("Attribute locked."));
        mush_free(arg1, "atrlock.string");
        return;
      } else if (status == 2) {
        AL_FLAGS(ptr) &= ~AF_LOCKED;
        notify(player, T("Attribute unlocked."));
        mush_free(arg1, "atrlock.string");
        return;
      } else {
        notify(player, T("Invalid status on atrlock.. Notify god."));
        mush_free(arg1, "atrlock.string");
        return;
      }
    }
  } else
    notify(player, T("No such attribute."));
  mush_free(arg1, "atrlock.string");
}

/** Change ownership of an attribute.
 * \verbatim
 * This function is used to implement @atrchown.
 * \endverbatim
 * \param player the enactor, for permission checking.
 * \param xarg1 the object/attribute to change, as a string.
 * \param arg2 the name of the new owner (or "me").
 */
void
do_atrchown(dbref player, const char *xarg1, const char *arg2)
{
  dbref thing, new_owner;
  char *p, *arg1;
  ATTR *ptr;
  if (!xarg1 || !*xarg1) {
    notify(player, T("You need to give an object/attribute pair."));
    return;
  }

  arg1 = mush_strdup(xarg1, "atrchown.string");

  if (!(p = strchr(arg1, '/')) || !(*(p + 1))) {
    notify(player, T("You need to give an object/attribute pair."));
    mush_free(arg1, "atrchown.string");
    return;
  }
  *p++ = '\0';
  if ((thing = noisy_match_result(player, arg1, NOTYPE, MAT_EVERYTHING)) ==
      NOTHING) {
    mush_free(arg1, "atrchown.string");
    return;
  }
  if (!controls(player, thing)) {
    notify(player, T("Permission denied."));
    mush_free(arg1, "atrchown.string");
    return;
  }

  if (!(arg2 && *arg2) || !strcasecmp(arg2, "me"))
    new_owner = player;
  else
    new_owner = lookup_player(arg2);
  if (new_owner == NOTHING) {
    notify(player, T("I can't find that player"));
    mush_free(arg1, "atrchown.string");
    return;
  }

  ptr = atr_get_noparent(thing, strupper(p));
  if (ptr && Can_Read_Attr(player, thing, ptr)) {
    if (Can_Write_Attr(player, thing, ptr)) {
      if (new_owner != Owner(player) && !Wizard(player)) {
        notify(player, T("You can only chown an attribute to yourself."));
        mush_free(arg1, "atrchown.string");
        return;
      }
      AL_CREATOR(ptr) = Owner(new_owner);
      notify(player, T("Attribute owner changed."));
      mush_free(arg1, "atrchown.string");
      return;
    } else {
      notify(player, T("You don't have the permission to chown that."));
      mush_free(arg1, "atrchown.string");
      return;
    }
  } else
    notify(player, T("No such attribute."));
  mush_free(arg1, "atrchown.string");
}


/** Allocate a new ATTR from a slab allocator.
 * \return the pointer to the head of the attribute free list.
 */
static ATTR *
alloc_atr(const void *hint)
{
  if (!attrib_slab) {
    attrib_slab = slab_create("attributes", sizeof(ATTR));
    slab_set_opt(attrib_slab, SLAB_ALLOC_BEST_FIT, 1);
    slab_set_opt(attrib_slab, SLAB_HINTLESS_THRESHOLD, 10);
  }

  return slab_malloc(attrib_slab, hint);
}


/** Free an unused attribute struct.
 * \param An attribute that's been deleted from an object and
 * had its chunk reference deleted.
 */
static void
free_atr(ATTR *a)
{
  slab_free(attrib_slab, a);
}

/** Delete one attribute, deallocating its name and data.
 * <strong>Does not update the owning object's attribute list or
 * attribute count. That is the caller's responsibility.</strong>
 *
 * \param a the attribute to free
 */
static void
atr_free_one(ATTR *a)
{
  if (!a)
    return;
  st_delete(AL_NAME(a), &atr_names);
  if (a->data)
    chunk_delete(a->data);
  free_atr(a);
}

/** Return the compressed data for an attribute.
 * This is a chokepoint function for accessing the chunk data.
 * \param atr the attribute struct from which to get the data reference.
 * \return a pointer to the compressed data, in a static buffer.
 */
unsigned char const *
atr_get_compressed_data(ATTR *atr)
{
  static unsigned char buffer[BUFFER_LEN * 2];
  static unsigned char const empty_string[] = { 0 };
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
 * decompression on attributes.
 * \param atr the attribute struct from which to get the data reference.
 * \return a pointer to the uncompressed data, in a dynamic buffer.
 */
char *
safe_atr_value(ATTR *atr)
{
  return safe_uncompress(atr_get_compressed_data(atr));
}
