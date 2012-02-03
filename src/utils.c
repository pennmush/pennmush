/**
 * \file utils.c
 *
 * \brief Utility functions for PennMUSH.
 *
 *
 */

#include "copyrite.h"
#include "config.h"

#include <stdio.h>
#include <limits.h>
#ifdef sgi
#include <math.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#include <fcntl.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef WIN32
#include <wtypes.h>
#include <winbase.h>            /* For GetCurrentProcessId() */
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "conf.h"
#include "match.h"
#include "externs.h"
#include "ansi.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "log.h"
#include "flags.h"
#include "dbdefs.h"
#include "attrib.h"
#include "parse.h"
#include "lock.h"
#include "SFMT.h"
#include "confmagic.h"

dbref find_entrance(dbref door);
void initialize_mt(void);


/** Parse object/attribute strings into components.
 * This function takes a string which is of the format obj/attr or attr,
 * and returns the dbref of the object, and a pointer to the attribute.
 * If no object is specified, then the dbref returned is the player's.
 * str is destructively modified. This function is probably underused.
 * \param player the default object.
 * \param str the string to parse.
 * \param thing pointer to dbref of object parsed out of string.
 * \param attrib pointer to pointer to attribute structure retrieved.
 */
void
parse_attrib(dbref player, char *str, dbref *thing, ATTR **attrib)
{
  char *name;

  /* find the object */

  if ((name = strchr(str, '/')) != NULL) {
    *name++ = '\0';
    *thing = noisy_match_result(player, str, NOTYPE, MAT_EVERYTHING);
  } else {
    name = str;
    *thing = player;
  }

  /* find the attribute */
  *attrib = (ATTR *) atr_get(*thing, upcasestr(name));
}


/** Populate a ufun_attrib struct from an obj/attr pair.
 * \verbatim Given an attribute [<object>/]<name> pair (which may include #lambda),
 * fetch its value, owner (thing), and pe_flags, and store in the struct
 * pointed to by ufun
 * \endverbatim
 * \param attrstring The obj/name of attribute.
 * \param executor Dbref of the executing object.
 * \param ufun Pointer to an allocated ufun_attrib struct to fill in.
 * \param flags A bitwise or of desired UFUN_* flags.
 * \return 0 on failure, true on success.
 */
bool
fetch_ufun_attrib(const char *attrstring, dbref executor, ufun_attrib * ufun,
                  int flags)
{
  char *thingname, *attrname;
  char astring[BUFFER_LEN];
  ATTR *attrib;

  if (!ufun)
    return 0;

  ufun->contents[0] = '\0';
  ufun->errmess = (char *) "";
  ufun->thing = executor;
  ufun->pe_flags = PE_UDEFAULT;
  ufun->ufun_flags = flags;

  ufun->thing = executor;
  thingname = NULL;

  if (!attrstring)
    return 0;
  strncpy(astring, attrstring, BUFFER_LEN);

  /* Split obj/attr */
  if ((flags & UFUN_OBJECT) && ((attrname = strchr(astring, '/')) != NULL)) {
    thingname = astring;
    *(attrname++) = '\0';
  } else {
    attrname = astring;
  }

  if (thingname && (flags & UFUN_LAMBDA)
      && (strcasecmp(thingname, "#lambda") == 0
          || strncasecmp(thingname, "#apply", 6) == 0)) {
    /* It's a lambda. */

    ufun->ufun_flags &= ~UFUN_NAME;
    ufun->thing = executor;
    if (strcasecmp(thingname, "#lambda") == 0)
      mush_strncpy(ufun->contents, attrname, BUFFER_LEN);
    else {                      /* #apply */
      char *ucb = ufun->contents;
      unsigned nargs = 1, n;

      thingname += 6;

      if (*thingname)
        nargs = parse_uinteger(thingname);

      /* Limit between 1 and 10 arguments (%0-%9) */
      if (nargs == 0)
        nargs = 1;
      if (nargs > 10)
        nargs = 10;

      safe_str(attrname, ufun->contents, &ucb);
      safe_chr('(', ufun->contents, &ucb);
      for (n = 0; n < nargs; n++) {
        if (n > 0)
          safe_chr(',', ufun->contents, &ucb);
        safe_format(ufun->contents, &ucb, "%%%u", n);
      }
      safe_chr(')', ufun->contents, &ucb);
      *ucb = '\0';
    }

    ufun->attrname[0] = '\0';
    return 1;
  }

  if (thingname) {
    /* Attribute is on something else. */
    ufun->thing =
      noisy_match_result(executor, thingname, NOTYPE, MAT_EVERYTHING);
    if (!GoodObject(ufun->thing)) {
      ufun->errmess = (char *) "#-1 INVALID OBJECT";
      return 0;
    }
  }

  attrib = (ATTR *) atr_get(ufun->thing, upcasestr(attrname));
  if (attrib && AF_Internal(attrib)) {
    /* Regardless of whether we're doing permission checks, we should
     * never be showing internal attributes here */
    attrib = NULL;
  }

  /* An empty attrib is the same as no attrib. */
  if (attrib == NULL) {
    if (flags & UFUN_REQUIRE_ATTR) {
      if (!(flags & UFUN_IGNORE_PERMS) && !Can_Examine(executor, ufun->thing))
        ufun->errmess = e_atrperm;
      return 0;
    } else {
      mush_strncpy(ufun->attrname, attrname, ATTRIBUTE_NAME_LIMIT + 1);
      return 1;
    }
  }
  if (!(flags & UFUN_IGNORE_PERMS)
      && !Can_Read_Attr(executor, ufun->thing, attrib)) {
    ufun->errmess = e_atrperm;
    return 0;
  }
  if (!(flags & UFUN_IGNORE_PERMS)
      && !CanEvalAttr(executor, ufun->thing, attrib)) {
    ufun->errmess = e_perm;
    return 0;
  }

  /* DEBUG attributes */
  if (AF_NoDebug(attrib))
    ufun->pe_flags |= PE_NODEBUG;       /* No_Debug overrides Debug */
  else if (AF_Debug(attrib))
    ufun->pe_flags |= PE_DEBUG;

  if (flags & UFUN_NAME) {
    if (attrib->flags & AF_NONAME)
      ufun->ufun_flags &= ~UFUN_NAME;
    else if (attrib->flags & AF_NOSPACE)
      ufun->ufun_flags |= UFUN_NAME_NOSPACE;
  }

  /* Populate the ufun object */
  mush_strncpy(ufun->contents, atr_value(attrib), BUFFER_LEN);
  mush_strncpy(ufun->attrname, AL_NAME(attrib), ATTRIBUTE_NAME_LIMIT + 1);

  /* We're good */
  return 1;
}

/** Given a ufun, executor, enactor, PE_Info, and arguments for %0-%9,
 *  call the ufun with appropriate permissions on values given for
 *  wenv_args. The value returned is stored in the buffer pointed to
 *  by ret, if given.
 * \param ufun The ufun_attrib that was initialized by fetch_ufun_attrib
 * \param ret If desired, a pointer to a buffer in which the results
 *            of the process_expression are stored in.
 * \param caller The caller (%@).
 * \param enactor The enactor. (%#)
 * \param pe_info The pe_info passed to the FUNCTION
 * \param user_regs Other arguments that may want to be added. This nests BELOW
 *                the pe_regs created by call_ufun. (It is checked first)
 * \retval 0 success
 * \retval 1 process_expression failed. (CPU time limit)
 */
bool
call_ufun(ufun_attrib * ufun, char *ret, dbref caller, dbref enactor,
          NEW_PE_INFO *pe_info, PE_REGS *user_regs)
{
  char rbuff[BUFFER_LEN];
  char *rp, *np = NULL;
  int pe_ret;
  char const *ap;
  char old_attr[BUFFER_LEN];
  int made_pe_info = 0;
  PE_REGS *pe_regs;
  PE_REGS *pe_regs_old;

  /* Make sure we have a ufun first */
  if (!ufun)
    return 1;
  if (!pe_info) {
    pe_info = make_pe_info("pe_info.call_ufun");
    made_pe_info = 1;
  } else {
    strcpy(old_attr, pe_info->attrname);
  }

  pe_regs_old = pe_info->regvals;

  if (ufun->ufun_flags & UFUN_LOCALIZE) {
    pe_regs = pe_regs_localize(pe_info, PE_REGS_Q | PE_REGS_NEWATTR,
                               "call_ufun");
  } else {
    pe_regs = pe_regs_localize(pe_info, PE_REGS_NEWATTR, "call_ufun");
  }

  rp = pe_info->attrname;
  if (*ufun->attrname == '\0') {
    safe_str("#LAMBDA", pe_info->attrname, &rp);
    safe_chr('/', pe_info->attrname, &rp);
    safe_str(ufun->contents, pe_info->attrname, &rp);
  } else {
    safe_dbref(ufun->thing, pe_info->attrname, &rp);
    safe_chr('/', pe_info->attrname, &rp);
    safe_str(ufun->attrname, pe_info->attrname, &rp);
  }
  *rp = '\0';

  /* If the user doesn't care about the return of the expression,
   * then use our own rbuff.  */
  if (!ret)
    ret = rbuff;
  rp = ret;

  /* Anything the caller wants available goes on the bottom of the stack */
  if (user_regs) {
    user_regs->prev = pe_info->regvals;
    pe_info->regvals = user_regs;
  }

  if (ufun->ufun_flags & UFUN_NAME) {
    safe_str(Name(enactor), ret, &rp);
    if (!(ufun->ufun_flags & UFUN_NAME_NOSPACE))
      safe_chr(' ', ret, &rp);
    np = rp;
  }


  /* And now, make the call! =) */
  ap = ufun->contents;
  pe_ret = process_expression(ret, &rp, &ap, ufun->thing, caller,
                              enactor, ufun->pe_flags, PT_DEFAULT, pe_info);
  *rp = '\0';

  if ((ufun->ufun_flags & UFUN_NAME) && np == rp) {
    /* Attr was empty, so we take off the name again */
    *ret = '\0';
  }

  /* Restore call_ufun's pe_regs */
  if (user_regs) {
    pe_info->regvals = user_regs->prev;
  }

  /* Restore the pe_regs stack. */
  pe_regs_restore(pe_info, pe_regs);
  pe_regs_free(pe_regs);

  pe_info->regvals = pe_regs_old;

  if (!made_pe_info) {
    /* Restore the old attrname. */
    strcpy(pe_info->attrname, old_attr);
  } else {
    free_pe_info(pe_info);
  }

  return pe_ret;
}

/** Given a thing, attribute, enactor and arguments for %0-%9,
 * call the ufun with appropriate permissions on values given for
 * wenv_args. The value returned is stored in the buffer pointed to
 * by ret, if given.
 * \param thing The thing that has the attribute to be called
 * \param attrname The name of the attribute to call.
 * \param ret If desired, a pointer to a buffer in which the results
 * of the process_expression are stored in.
 * \param enactor The enactor.
 * \param pe_info The pe_info passed to the FUNCTION
 * \param pe_regs Other arguments that may want to be added. This nests BELOW
 *                the pe_regs created by call_ufun. (It is checked first)
 * \retval 1 success
 * \retval 0 No such attribute, or failed.
 */
bool
call_attrib(dbref thing, const char *attrname, char *ret, dbref enactor,
            NEW_PE_INFO *pe_info, PE_REGS *pe_regs)
{
  ufun_attrib ufun;
  if (!fetch_ufun_attrib(attrname, thing, &ufun,
                         UFUN_LOCALIZE | UFUN_REQUIRE_ATTR | UFUN_IGNORE_PERMS))
  {
    return 0;
  }
  return !call_ufun(&ufun, ret, thing, enactor, pe_info, pe_regs);
}

/** Given an exit, find the room that is its source through brute force.
 * This is used in pathological cases where the exit's own source
 * element is invalid.
 * \param door dbref of exit to find source of.
 * \return dbref of exit's source room, or NOTHING.
 */
dbref
find_entrance(dbref door)
{
  dbref room;
  dbref thing;
  for (room = 0; room < db_top; room++)
    if (IsRoom(room)) {
      thing = Exits(room);
      while (thing != NOTHING) {
        if (thing == door)
          return room;
        thing = Next(thing);
      }
    }
  return NOTHING;
}

/** Remove the first occurence of what in chain headed by first.
 * This works for contents and exit chains.
 * \param first dbref of first object in chain.
 * \param what dbref of object to remove from chain.
 * \return new head of chain.
 */
dbref
remove_first(dbref first, dbref what)
{
  dbref prev;
  /* special case if it's the first one */
  if (first == what) {
    return Next(first);
  } else {
    /* have to find it */
    DOLIST(prev, first) {
      if (Next(prev) == what) {
        Next(prev) = Next(what);
        return first;
      }
    }
    return first;
  }
}

/** Is an object on a chain?
 * \param thing object to look for.
 * \param list head of chain to search.
 * \retval 1 found thing on list.
 * \retval 0 did not find thing on list.
 */
bool
member(dbref thing, dbref list)
{
  DOLIST(list, list) {
    if (list == thing)
      return 1;
  }

  return 0;
}


/** Is an object inside another, at any level of depth?
 * That is, we check if disallow is inside of from, i.e., if
 * loc(disallow) = from, or loc(loc(disallow)) = from, etc., with a
 * depth limit of 50.
 * Despite the name of this function, it's not recursive any more.
 * \param disallow interior object to check.
 * \param from check if disallow is inside of this object.
 * \param count depths of nesting checked so far.
 * \retval 1 disallow is inside of from.
 * \retval 0 disallow is not inside of from.
 */
bool
recursive_member(dbref disallow, dbref from, int count)
{
  do {
    /* The end of the location chain. This is a room. */
    if (!GoodObject(disallow) || IsRoom(disallow))
      return 0;

    if (from == disallow)
      return 1;

    disallow = Location(disallow);
    count++;
  } while (count <= 50);

  return 1;
}

/** Is an object or its location unfindable?
 * \param thing object to check.
 * \retval 1 object or location is unfindable.
 * \retval 0 neither object nor location is unfindable.
 */
bool
unfindable(dbref thing)
{
  int count = 0;
  do {
    if (!GoodObject(thing))
      return 0;
    if (Unfind(thing))
      return 1;
    if (IsRoom(thing))
      return 0;
    thing = Location(thing);
    count++;
  } while (count <= 50);
  return 0;
}


/** Reverse the order of a dbref chain.
 * \param list dbref at the head of the chain.
 * \return dbref at the head of the reversed chain.
 */
dbref
reverse(dbref list)
{
  dbref newlist;
  dbref rest;
  newlist = NOTHING;
  while (list != NOTHING) {
    rest = Next(list);
    PUSH(list, newlist);
    list = rest;
  }
  return newlist;
}

/** Wrapper to choose a seed and initialize the Mersenne Twister PRNG.
 * The actual MT code lives in SFMT.c and hdrs/SFMT*.h */
void
initialize_mt(void)
{
#ifdef HAS_DEV_URANDOM
  int fd;
  uint32_t buf[4];              /* The linux manpage for /dev/urandom
                                   advises against reading large amounts of
                                   data from it; we used to read 624*4 (Or *8 on 64-bit systems)
                                   bytes. The new figure is much more reasonable. */

  fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0) {
    int r = read(fd, buf, sizeof buf);
    close(fd);
    if (r <= 0) {
      fprintf(stderr,
              "Couldn't read from /dev/urandom! Resorting to normal seeding method.\n");
    } else {
      fprintf(stderr, "Seeded RNG from /dev/urandom\n");
      init_by_array(buf, r / sizeof(uint32_t));
      return;
    }
  } else
    fprintf(stderr,
            "Couldn't open /dev/urandom to seed random number generator. Resorting to normal seeding method.\n");

#endif
  /* Default seeder. Pick a seed that's fairly random */
#ifdef WIN32
  init_gen_rand(GetCurrentProcessId() | (time(NULL) << 16));
#else
  init_gen_rand(getpid() | (time(NULL) << 16));
#endif
}

/** Get a uniform random long between low and high values, inclusive.
 * Based on MUX's RandomINT32()
 * \param low lower bound for random number.
 * \param high upper bound for random number.
 * \return random number between low and high, or 0 or -1 for error.
 */
uint32_t
get_random32(uint32_t low, uint32_t high)
{
  uint32_t x, n, n_limit;

  /* Validate parameters */
  if (high < low) {
    return 0;
  } else if (high == low) {
    return low;
  }

  x = high - low + 1;

  /* We can now look for an random number on the interval [0,x-1].
     //

     // In order to be perfectly conservative about not introducing any
     // further sources of statistical bias, we're going to call getrand()
     // until we get a number less than the greatest representable
     // multiple of x. We'll then return n mod x.
     //
     // N.B. This loop happens in randomized constant time, and pretty
     // damn fast randomized constant time too, since
     //
     //      P(UINT32_MAX_VALUE - n < UINT32_MAX_VALUE % x) < 0.5, for any x.
     //
     // So even for the least desireable x, the average number of times
     // we will call getrand() is less than 2.
   */

  n_limit = UINT32_MAX - (UINT32_MAX % x);

  do {
    n = gen_rand32();
  } while (n >= n_limit);

  return low + (n % x);
}

/** Return an object's alias. We expect a valid object.
 * \param it dbref of object.
 * \return object's complete alias.
 */
char *
fullalias(dbref it)
{
  static char n[BUFFER_LEN];    /* STATIC */
  ATTR *a = atr_get_noparent(it, "ALIAS");

  if (!IsExit(it)) {
    if (!a) {
      n[0] = '\0';
      return n;
    }

    mush_strncpy(n, atr_value(a), BUFFER_LEN);
  } else {
    char *np = n;
    char *sep;

    if ((sep = strchr(Name(it), ';'))) {
      sep++;
      safe_str(sep, n, &np);
    }
    if (a) {
      if (sep)
        safe_chr(';', n, &np);
      safe_str(atr_value(a), n, &np);
    }
    *np = '\0';
  }

  return n;
}

/** Return only the first component of an object's alias. We expect
 * a valid object.
 * \param it dbref of object.
 * \return object's short alias.
 */
char *
shortalias(dbref it)
{
  static char n[BUFFER_LEN];    /* STATIC */
  char *s;

  s = fullalias(it);
  if (!(s && *s)) {
    n[0] = '\0';
    return n;
  }

  copy_up_to(n, s, ';');

  return n;
}

/** Return an object's name, but for exits, return just the first
 * component. We expect a valid object.
 * \param it dbref of object.
 * \return object's short name.
 */
char *
shortname(dbref it)
{
  static char n[BUFFER_LEN];    /* STATIC */
  char *s;

  mush_strncpy(n, Name(it), BUFFER_LEN);

  if (IsExit(it)) {
    if ((s = strchr(n, ';')))
      *s = '\0';
  }
  return n;
}

/** Return the absolute room (outermost container) of an object.
 * Return  NOTHING if it's in an invalid object or in an invalid
 * location or AMBIGUOUS if there are too many containers.
 * \param it dbref of object.
 * \return absolute room of object, NOTHING, or AMBIGUOUS.
 */
dbref
absolute_room(dbref it)
{
  int rec = 0;
  dbref room;
  if (!GoodObject(it))
    return NOTHING;
  if (IsRoom(it))
    return it;
  room = IsExit(it) ? Home(it) : Location(it);
  while (rec <= 20) {
    if (!GoodObject(room) || IsGarbage(room)) {
      return NOTHING;
    }
    if (IsRoom(room)) {
      return room;
    }
    rec++;
    room = Location(room);
  }
  return AMBIGUOUS;
}


/** Can one object interact with/perceive another in a given way?
 * This funtion checks to see if 'to' can perceive something from
 * 'from'. The types of interactions currently supported include:
 * INTERACT_SEE (will light rays from 'from' reach 'to'?), INTERACT_HEAR
 * (will sound from 'from' reach 'to'?), INTERACT_MATCH (can 'to'
 * match the name of 'from'?), and INTERACT_PRESENCE (will the arrival/
 * departure/connection/disconnection/growing ears/losing ears of
 * 'from' be noticed by 'to'?).
 * \param from object of interaction.
 * \param to subject of interaction, attempting to interact with from.
 * \param type type of interaction.
 * \retval 1 to can interact with from in this way.
 * \retval 0 to can not interact with from in this way.
 */
int
can_interact(dbref from, dbref to, int type, NEW_PE_INFO *pe_info)
{
  int lci;

  /* This shouldn't even be checked for rooms and garbage, but we're
   * paranoid. Trying to stop interaction with yourself will not work 99%
   * of the time, so we don't allow it anyway. */
  if (IsGarbage(from) || IsGarbage(to))
    return 0;

  if ((from == to) || IsRoom(from) || IsRoom(to))
    return 1;

  /* This function can override standard checks! */
  lci = local_can_interact_first(from, to, type);
  if (lci != NOTHING)
    return lci;

  /* Standard checks */

  /* If it's an audible message, it must pass your Interact_Lock
   * (or be from a privileged speaker)
   */
  if ((type == INTERACT_HEAR) && !Pass_Interact_Lock(from, to, pe_info))
    return 0;

  /* You can interact with the object you are in or any objects
   * you're holding.
   * You can interact with objects you control, but not
   * specifically the other way around
   */
  if ((from == Location(to)) || (to == Location(from)) || controls(to, from))
    return 1;


  lci = local_can_interact_last(from, to, type);
  if (lci != NOTHING)
    return lci;

  return 1;
}

/** Return the next parent in an object's parent chain
 * \verbatim
 * For a given thing, return the next object in its parent chain. current is
 * the current parent being looked at (initially the same as thing).
 * parent_count is a pointer to an int which stores the number of parents
 * we've looked at, for MAX_PARENTS checks. use_ancestor is a pointer to an
 * int, initially set to 1, if we should include thing's ancestor object,
 * which we set to 2 when we've seen the ancestor in the chain. Use a NULL
 * pointer for no ancestor check.
 * \endverbatim
 * \param thing the child object
 * \param current the current object
 * \param parent_count pointer to int of how many parents we've used
 * \param use_ancestor pointer to int of whether we should/have used the
 *                     ancestor object, or NULL if we don't want to use it
 * \return dbref of next parent object in chain
 */
dbref
next_parent(dbref thing, dbref current, int *parent_count, int *use_ancestor)
{
  dbref next;

  if ((*parent_count) > MAX_PARENTS || (use_ancestor && *use_ancestor == 2))
    next = NOTHING;
  else
    next = Parent(current);

  (*parent_count)++;


  if (!GoodObject(next) && use_ancestor && (*use_ancestor) == 1
      && !Orphan(thing)) {
    /* Check for ancestor */
    next = Ancestor_Parent(thing);
    (*use_ancestor) = 2;
  } else if (next == Ancestor_Parent(thing) && use_ancestor)
    (*use_ancestor) = 0;

  return next;
}
