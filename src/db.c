/**
 * \file db.c
 *
 * \brief Loading and saving the PennMUSH object database.
 *
 *
 */

#define _GNU_SOURCE

#include "copyrite.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "ansi.h"
#include "attrib.h"
#include "conf.h"
#include "dbdefs.h"
#include "dbio.h"
#include "extchat.h"
#include "externs.h"
#include "extmail.h"
#include "flags.h"
#include "game.h"
#include "htab.h"
#include "lock.h"
#include "log.h"
#include "memcheck.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "parse.h"
#include "privtab.h"
#include "strtree.h"
#include "strutil.h"
#include "mushsql.h"
#include "charclass.h"

#ifdef WIN32
#pragma warning(disable : 4761) /* disable warning re conversion */
#endif

#ifdef WIN32SERVICES
void shutdown_checkpoint(void);
#endif

/** Get a ref out of the database if a given db flag is set */
#define MAYBE_GET(f, x) (globals.indb_flags & (x)) ? getref(f) : 0

int loading_db = 0; /**< Are we loading the database? */

char db_timestamp[100]; /**< Time the read database was saved. */

struct object *db = NULL; /**< The object db array */
dbref db_top = 0;         /**< The number of objects in the db array */

dbref errobj; /**< Dbref of object on which an error has occurred */

int dbline = 0; /**< Line of the database file being read */

/** String that markes the end of dumps */
const char EOD[] = "***END OF DUMP***\n";

#ifndef DB_INITIAL_SIZE
#define DB_INITIAL_SIZE 5000 /**< Initial size for db array */
#endif                       /* DB_INITIAL_SIZE */

dbref db_size = DB_INITIAL_SIZE; /**< Current size of db array */

static void db_grow(dbref newtop);

static void db_write_obj_basic(PENNFILE *f, dbref i, struct object *o);
int db_paranoid_write_object(PENNFILE *f, dbref i, int flag);
int db_write_object(PENNFILE *f, dbref i);
void putlocks(PENNFILE *f, lock_list *l);
void getlocks(dbref i, PENNFILE *f);
void get_new_locks(dbref i, PENNFILE *f, int c);
void db_read_attrs(PENNFILE *f, dbref i, int c);
int get_list(PENNFILE *f, dbref i);
void db_free(void);
static void init_objdata();
static void db_write_flags(PENNFILE *f);
static void db_write_attrs(PENNFILE *f);
static dbref db_read_oldstyle(PENNFILE *f);
static void add_object_table(dbref);

StrTree object_names; /**< String tree of object names */
extern StrTree atr_names;

void init_names(void);

void create_minimal_db(void);

extern struct db_stat_info current_state;

/** Initialize the name strtree.
 */
void
init_names(void)
{
  st_init(&object_names, "ObjectNameTree");
}

/** Set an object's name through the name strtree.
 * We maintain object names in a strtree because many objects have
 * the same name (cardinal exits, weapons and armor, etc.)
 * This function is used to set an object's name; if the name's already
 * in the strtree, we just get a pointer to it, saving memory.
 * (If not, we add it to the strtree and use that pointer).
 * \param obj dbref of object whose name is to be set.
 * \param newname name to set on the object, or NULL to clear the name.
 * \return object's new name, or NULL if none is given.
 */
const char *
set_name(dbref obj, const char *newname)
{
  /* if pointer not null unalloc it */
  if (Name(obj))
    st_delete(Name(obj), &object_names);
  if (!newname || !*newname)
    return NULL;
  Name(obj) = st_insert(newname, &object_names);
  return Name(obj);
}

int db_init = 0; /**< Has the db array been initialized yet? */

static void
db_grow(dbref newtop)
{
  struct object *newdb;
  dbref initialized;
  struct object *o;

  if (newtop > db_top) {
    initialized = db_top;
    current_state.total = newtop;
    current_state.garbage += newtop - db_top;
    db_top = newtop;
    if (!db) {
      /* make the initial one */
      db_size = (db_init) ? db_init : DB_INITIAL_SIZE;
      while (db_top > db_size)
        db_size *= 2;
      if ((db = (struct object *) malloc(db_size * sizeof(struct object))) ==
          NULL) {
        do_rawlog(LT_ERR, "ERROR: out of memory while creating database!");
        abort();
      }
    }
    /* maybe grow it */
    if (db_top > db_size) {
      /* make sure it's big enough */
      while (db_top > db_size)
        db_size *= 2;
      if ((newdb = (struct object *) realloc(
             db, db_size * sizeof(struct object))) == NULL) {
        do_rawlog(LT_ERR, "ERROR: out of memory while extending database!");
        abort();
      }
      db = newdb;
    }
    while (initialized < db_top) {
      o = db + initialized;
      o->name = 0;
      o->location = NOTHING;
      o->contents = NOTHING;
      o->exits = NOTHING;
      o->next = NOTHING;
      o->parent = NOTHING;
      o->locks = NULL;
      o->owner = GOD;
      o->zone = NOTHING;
      o->penn = 0;
      o->type = TYPE_GARBAGE;
      o->flags = NULL;
      o->powers = NULL;
      o->warnings = 0;
      o->modification_time = o->creation_time = mudtime;
      o->attrcount = 0;
      o->attrcap = 0;
      o->list = NULL;
      initialized++;
    }
  }
}

/** Allocate a new object structure.
 * This function allocates and returns a new object structure.
 * The caller must see that it gets appropriately typed and otherwise
 * initialized.
 * \return dbref of newly allocated object.
 */
dbref
new_object(void)
{
  dbref newobj;
  struct object *o;
  /* if stuff in free list use it */
  if ((newobj = free_get()) == NOTHING) {
    /* allocate more space */
    newobj = db_top;
    db_grow(db_top + 1);
  }
  /* clear it out */
  o = db + newobj;
  o->name = 0;
  o->list = 0;
  o->location = NOTHING;
  o->contents = NOTHING;
  o->exits = NOTHING;
  o->next = NOTHING;
  o->parent = NOTHING;
  o->locks = NULL;
  o->owner = GOD;
  o->zone = NOTHING;
  o->penn = 0;
  o->type = TYPE_GARBAGE;
  o->warnings = 0;
  o->modification_time = o->creation_time = mudtime;
  o->attrcount = 0;
  /* Flags are set by the functions that call this */
  o->powers = new_flag_bitmask("POWER");
  if (current_state.garbage) {
    current_state.garbage--;
  }

  add_object_table(newobj);

  return newobj;
}

/** Output a long int to a file.
 * \param f file pointer to write to.
 * \param ref value to write.
 */
void
putref(PENNFILE *f, long int ref)
{
  penn_fprintf(f, "%ld\n", ref);
}

/** Output a uint32_t to a file.
 * \param f file pointer to write to.
 * \param ref value to write.
 */
void
putref_u32(PENNFILE *f, uint32_t ref)
{
  penn_fprintf(f, "%" PRIu32 "\n", ref);
}

/** Output a uint64_t to a file.
 * \param f file pointer to write to.
 * \param ref value to write.
 */
void
putref_u64(PENNFILE *f, uint64_t ref)
{
#ifdef WIN32
  penn_fprintf(f, "%I64u\n", ref);
#else
  penn_fprintf(f, "%" PRIu64 "\n", ref);
#endif
}

/** Output a string to a file.
 * This function writes a string to a file, double-quoted,
 * appropriately escaping quotes and backslashes (the escape character).
 * \param f file pointer to write to.
 * \param s value to write.
 */
void
putstring(PENNFILE *f, const char *s)
{
  penn_fputc('"', f);
  while (*s) {
    switch (*s) {
    case '\\':
    case '"':
      penn_fputc('\\', f);
    /* FALL THROUGH */
    default:
      penn_fputc(*s, f);
    }
    s++;
  }
  penn_fputc('"', f);
  penn_fputc('\n', f);
}

/** Read a labeled entry from a database.
 * Labeled entries look like 'label entry', and are used
 * extensively in the current database format, and to a lesser
 * extent in older versions.
 * \param f the file to read from
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param value pointer to update to the address of a static
 * buffer containing the value that was read.
 */
void
db_read_labeled_string(PENNFILE *f, char **label, char **value)
{
  static char lbuf[BUFFER_LEN], vbuf[BUFFER_LEN];
  int c;
  char *p;

  *label = lbuf;
  *value = vbuf;

  /* invariant: we start at the beginning of a line. */

  dbline++;

  do {
    c = penn_fgetc(f);
    while (isspace(c)) {
      if (c == '\n')
        dbline++;
      c = penn_fgetc(f);
    }
    if (c == '#') {
      while ((c = penn_fgetc(f)) != '\n' && c != EOF) {
        /* nothing */
      }
      if (c == '\n')
        dbline++;
    }
  } while (c != EOF && isspace(c));

  if (c == EOF) {
    do_rawlog(LT_ERR, "DB: Unexpected EOF at line %d", dbline);
    longjmp(db_err, 1);
  }

  /* invariant: we should have the first character of a label in 'c'. */

  p = lbuf;
  do {
    if (c != '_' && c != '-' && c != '!' && c != '.' && c != '>' && c != '<' &&
        c != '#' && /* these really should only be first time */
        !isalnum(c)) {
      do_rawlog(LT_ERR, "DB: Illegal character '%c'(%d) in label, line %d", c,
                c, dbline);
      longjmp(db_err, 1);
    }
    safe_chr(c, lbuf, &p);
    c = penn_fgetc(f);
  } while (c != EOF && !isspace(c));
  *p++ = '\0';
  if (p >= lbuf + BUFFER_LEN)
    do_rawlog(LT_ERR, "DB: warning: very long label, line %d", dbline);

  /* suck up separating whitespace */
  while (c != '\n' && c != EOF && isspace(c))
    c = penn_fgetc(f);

  /* check for presence of a value, which we must have. */
  if (c == EOF || c == '\n') {
    if (c == EOF)
      do_rawlog(LT_ERR, "DB: Unexpected EOF at line %d", dbline);
    else
      do_rawlog(LT_ERR, "DB: Missing value for '%s' at line %d", lbuf, dbline);
    longjmp(db_err, 1);
  }

  /* invariant: we should have the first character of a value in 'c'. */

  p = vbuf;
  if (c == '"') {
    /* quoted string */
    int sline;
    sline = dbline;
    for (;;) {
      c = penn_fgetc(f);
      if (c == '"')
        break;
      if (c == '\\')
        c = penn_fgetc(f);
      if (c == EOF) {
        do_rawlog(LT_ERR, "DB: Unclosed quoted string starting on line %d",
                  sline);
        longjmp(db_err, 1);
      }
      if (c == '\0')
        do_rawlog(LT_ERR,
                  "DB: warning: null in quoted string, remainder lost, line %d",
                  dbline);
      if (c == '\n')
        dbline++;
      safe_chr(c, vbuf, &p);
    }
    do {
      c = penn_fgetc(f);
      if (c != EOF && !isspace(c)) {
        do_rawlog(LT_ERR, "DB: Garbage after quoted string, line %d", dbline);
        longjmp(db_err, 1);
      }
    } while (c != '\n' && c != EOF);
  } else {
    /* non-quoted value */
    do {
      if (c != '_' && c != '-' && c != '!' && c != '.' && c != '#' &&
          !isalnum(c) && !isspace(c)) {
        do_rawlog(LT_ERR, "DB: Illegal character '%c'(%d) in value, line %d", c,
                  c, dbline);
        longjmp(db_err, 1);
      }
      safe_chr(c, vbuf, &p);
      c = penn_fgetc(f);
    } while (c != EOF && c != '\n');
    if (c == '\n' && (p - vbuf >= 2) && (*(p - 2) == '\r')) {
      /* Oops, we read in \r\n at the end of this value. Drop the \r */
      p--;
      *(p - 1) = '\n';
    }
  }
  *p++ = '\0';
  if (p >= vbuf + BUFFER_LEN)
    do_rawlog(LT_ERR, "DB: warning: very long value, line %d", dbline);

  /* note no line increment for final newline because of initial increment */
}

/** Read a string with a given label.
 * If the label read is different than the one being checked, the
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param value pointer to update to the address of a static
 * buffer containing the value that was read.
 */
void
db_read_this_labeled_string(PENNFILE *f, const char *label, char **value)
{
  char *readlabel;

  db_read_labeled_string(f, &readlabel, value);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
              "DB: error: Got label '%s', expected label '%s' at line %d",
              readlabel, label, dbline);
    longjmp(db_err, 1);
  }
}

/** Read an int with a given label.
 * If the label read is different than the one being checked, the
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param value pointer to update to the number that was read.
 */
void
db_read_this_labeled_int(PENNFILE *f, const char *label, int *value)
{
  char *readlabel;
  char *readvalue;

  db_read_labeled_string(f, &readlabel, &readvalue);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
              "DB: error: Got label '%s', expected label '%s' at line %d",
              readlabel, label, dbline);
    longjmp(db_err, 1);
  }

  *value = parse_integer(readvalue);
}

/** Read an int and label.
 * \param f the file to read from.
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param value pointer to update to the number that was read.
 */
void
db_read_labeled_int(PENNFILE *f, char **label, int *value)
{
  char *readvalue;
  db_read_labeled_string(f, label, &readvalue);
  *value = parse_integer(readvalue);
}

/** Read a uint32_t with a given label.
 * If the label read is different than the one being checked, the
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param value pointer to update to the number that was read.
 */
void
db_read_this_labeled_uint32(PENNFILE *f, const char *label, uint32_t *value)
{
  char *readlabel;
  char *readvalue;

  db_read_labeled_string(f, &readlabel, &readvalue);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
              "DB: error: Got label '%s', expected label '%s' at line %d",
              readlabel, label, dbline);
    longjmp(db_err, 1);
  }

  *value = parse_uint32(readvalue, NULL, 10);
}

/** Read a uint32_t and label.
 * \param f the file to read from.
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param value pointer to update to the number that was read.
 */
void
db_read_labeled_uint32(PENNFILE *f, char **label, uint32_t *value)
{
  char *readvalue;
  db_read_labeled_string(f, label, &readvalue);
  *value = parse_uint32(readvalue, NULL, 10);
}

/** Read a dbref with a given label.
 * If the label read is different than the one being checked, the
 * database load will abort with an error.
 * \param f the file to read from.
 * \param label the label that should be read.
 * \param val pointer to update to the dbref that was read.
 */
void
db_read_this_labeled_dbref(PENNFILE *f, const char *label, dbref *val)
{
  char *readlabel;
  char *readvalue;

  db_read_labeled_string(f, &readlabel, &readvalue);

  if (strcmp(readlabel, label)) {
    do_rawlog(LT_ERR,
              "DB: error: Got label '%s', expected label '%s' at line %d",
              readlabel, label, dbline);
    longjmp(db_err, 1);
  }
  *val = qparse_dbref(readvalue);
}

/** Read a dbref and label.
 * \param f the file to read from.
 * \param label pointer to update to the address of a static
 * buffer containing the label that was read.
 * \param val pointer to update to the dbref that was read.
 */
void
db_read_labeled_dbref(PENNFILE *f, char **label, dbref *val)
{
  char *readvalue;
  db_read_labeled_string(f, label, &readvalue);
  *val = qparse_dbref(readvalue);
}

static void
db_write_label(PENNFILE *f, char const *l)
{
  penn_fputs(l, f);
  penn_fputc(' ', f);
}

void
db_write_labeled_string(PENNFILE *f, char const *label, char const *value)
{
  db_write_label(f, label);
  putstring(f, value);
}

void
db_write_labeled_int(PENNFILE *f, char const *label, int value)
{
  penn_fprintf(f, "%s %d\n", label, value);
}

void
db_write_labeled_dbref(PENNFILE *f, char const *label, dbref value)
{
  penn_fprintf(f, "%s #%d\n", label, value);
}

/** Write a boolexp to a file in unparsed (text) form.
 * \param f file pointer to write to.
 * \param b pointer to boolexp to write.
 */
void
putboolexp(PENNFILE *f, boolexp b)
{
  db_write_labeled_string(f, "  key", unparse_boolexp(GOD, b, UB_DBREF));
}

/** Write a list of locks to a file.
 * \param f file pointer to write to.
 * \param l pointer to lock_list to write.
 */
void
putlocks(PENNFILE *f, lock_list *l)
{
  lock_list *ll;
  int count = 0;
  for (ll = l; ll; ll = ll->next)
    count++;
  db_write_labeled_int(f, "lockcount", count);
  for (ll = l; ll; ll = ll->next) {
    db_write_labeled_string(f, " type", ll->type);
    db_write_labeled_dbref(f, "  creator", L_CREATOR(ll));
    db_write_labeled_string(f, "  flags", lock_flags_long(ll));
    db_write_labeled_int(f, "  derefs", chunk_derefs(L_KEY(ll)));
    putboolexp(f, ll->key);
    /* putboolexp adds a '\n', so we won't. */
  }
}

/** Write out the basics of an object.
 * This function writes out the basic information associated with an
 * object - just about everything but the attributes.
 * \param f file pointer to write to.
 * \param i dbref of object to write.
 * \param o pointer to object to write.
 */
static void
db_write_obj_basic(PENNFILE *f, dbref i, struct object *o)
{
  db_write_labeled_string(f, "name", o->name);
  db_write_labeled_dbref(f, "location", o->location);
  db_write_labeled_dbref(f, "contents", o->contents);
  db_write_labeled_dbref(f, "exits", o->exits);
  db_write_labeled_dbref(f, "next", o->next);
  db_write_labeled_dbref(f, "parent", o->parent);
  putlocks(f, Locks(i));
  db_write_labeled_dbref(f, "owner", o->owner);
  db_write_labeled_dbref(f, "zone", o->zone);
  db_write_labeled_int(f, "pennies", Pennies(i));
  db_write_labeled_int(f, "type", Typeof(i));
  db_write_labeled_string(f, "flags",
                          bits_to_string("FLAG", o->flags, GOD, NOTHING));
  db_write_labeled_string(f, "powers",
                          bits_to_string("POWER", o->powers, GOD, NOTHING));
  db_write_labeled_string(f, "warnings", unparse_warnings(o->warnings));
  db_write_labeled_int(f, "created", (int) o->creation_time);
  db_write_labeled_int(f, "modified", (int) o->modification_time);
}

/** Write out an object.
 * This function writes a single object out to a file.
 * \param f file pointer to write to.
 * \param i dbref of object to write.
 */
int
db_write_object(PENNFILE *f, dbref i)
{
  struct object *o;
  ALIST *list;
  int count = 0;

  o = db + i;
  db_write_obj_basic(f, i, o);

  /* write the attribute list */

  /* Don't trust AttrCount(thing) for number of attributes to write. */
  ATTR_FOR_EACH (i, list) {
    if (AF_Nodump(list))
      continue;
    count++;
  }
  db_write_labeled_int(f, "attrcount", count);

  ATTR_FOR_EACH (i, list) {
    if (AF_Nodump(list))
      continue;
    db_write_labeled_string(f, " name", AL_NAME(list));
    db_write_labeled_dbref(f, "  owner", Owner(AL_CREATOR(list)));
    db_write_labeled_string(f, "  flags", atrflag_to_string(AL_FLAGS(list)));
    db_write_labeled_int(f, "  derefs", AL_DEREFS(list));
    db_write_labeled_string(f, "  value", atr_value(list));
  }
  return 0;
}

/** Write out the object database to disk.
 * \verbatim
 * This function writes the databsae out to disk. The database
 * structure currently looks something like this:
 * +V<header line>
 * savedtime <timestamp>
 * +FLAGS LIST
 * <flag data>
 * +POWERS LIST
 * <flag data>
 * ~<number of objects>
 * <object data>
 * \endverbatim
 * \param f file pointer to write to.
 * \param flag 0 for normal dump, DBF_PANIC for panic dumps.
 * \return the number of objects in the database (db_top)
 */
dbref
db_write(PENNFILE *f, int flag)
{
  dbref i;
  int dbflag;

  /* print a header line to make a later conversion to 2.0 easier to do.
   * the odd choice of numbers is based on 256*x + 2 offset
   * The original PennMUSH had x=5 (chat) or x=6 (nochat), and Tiny expects
   * to deal with that. We need to use some extra flags as well, so
   * we may be adding to 5/6 as needed, using successive binary numbers.
   */
  dbflag = 5 + flag;
  dbflag += DBF_NO_CHAT_SYSTEM;
  dbflag += DBF_WARNINGS;
  dbflag += DBF_CREATION_TIMES;
  dbflag += DBF_SPIFFY_LOCKS;
  dbflag += DBF_NEW_STRINGS;
  dbflag += DBF_TYPE_GARBAGE;
  dbflag += DBF_SPLIT_IMMORTAL;
  dbflag += DBF_NO_TEMPLE;
  dbflag += DBF_LESS_GARBAGE;
  dbflag += DBF_AF_VISUAL;
  dbflag += DBF_VALUE_IS_COST;
  dbflag += DBF_LINK_ANYWHERE;
  dbflag += DBF_NO_STARTUP_FLAG;
  dbflag += DBF_AF_NODUMP;
  dbflag += DBF_NEW_FLAGS;
  dbflag += DBF_NEW_POWERS;
  dbflag += DBF_POWERS_LOGGED;
  dbflag += DBF_LABELS;
  dbflag += DBF_SPIFFY_AF_ANSI;
  dbflag += DBF_HEAR_CONNECT;
  dbflag += DBF_NEW_VERSIONS;

  penn_fprintf(f, "+V%d\n", dbflag * 256 + 2);

  db_write_labeled_int(f, "dbversion", NDBF_VERSION);

  db_write_labeled_string(f, "savedtime", show_time(mudtime, 1));

  db_write_flags(f);

  db_write_attrs(f);

  penn_fprintf(f, "~%d\n", db_top);

  for (i = 0; i < db_top; i++) {
#ifdef WIN32SERVICES
    /* Keep the service manager happy */
    if (shutdown_flag && (i & 0xFF) == 0)
      shutdown_checkpoint();
#endif
    if (IsGarbage(i))
      continue;
    penn_fprintf(f, "!%d\n", i);
    db_write_object(f, i);
  }
  penn_fputs(EOD, f);
  return db_top;
}

static void
db_write_flags(PENNFILE *f)
{
  penn_fputs("+FLAGS LIST\n", f);
  flag_write_all(f, "FLAG");
  penn_fputs("+POWER LIST\n", f);
  flag_write_all(f, "POWER");
}

static void
db_write_attrs(PENNFILE *f)
{
  penn_fputs("+ATTRIBUTES LIST\n", f);
  attr_write_all(f);
}

/** Write out an object, in paranoid fashion.
 * This function writes a single object out to a file in paranoid
 * mode, which warns about several potential types of corruption,
 * and can fix some of them.
 * \param f file pointer to write to.
 * \param i dbref of object to write.
 * \param flag 1 = debug, 0 = normal
 */
int
db_paranoid_write_object(PENNFILE *f, dbref i, int flag)
{
  struct object *o;
  ALIST *list;
  char name[ATTRIBUTE_NAME_LIMIT + 1];
  char tbuf1[BUFFER_LEN];
  char *p;
  int attrcount = 0;

  o = db + i;
  db_write_obj_basic(f, i, o);

  /* write the attribute list, scanning */
  ATTR_FOR_EACH (i, list) {
    if (AF_Nodump(list))
      continue;
    attrcount++;
  }

  db_write_labeled_int(f, "attrcount", attrcount);

  list = o->list;
  for (int seen = 0; seen < AttrCount(i); seen++, list++) {
    bool fixmemdb = 0, err = 0;
    bool fixname = 0, fixtext = 0;
    dbref owner;

    if (AF_Nodump(list))
      continue;

    /* smash unprintable characters in the name, replace with ! */
    mush_strncpy(name, AL_NAME(list), sizeof name);
    for (p = name; *p; p++) {
      if (!ascii_isprint(*p) || isspace(*p)) {
        *p = '!';
        err = 1;
      }
    }
    if (err) {
      fixname = fixmemdb = 1;
      /* If name already exists on this object, try adding a
       * number to the end. Give up if we can't find one < 10000
       */
      if (atr_get_noparent(i, name)) {
        int count = 0;
        char newname[ATTRIBUTE_NAME_LIMIT + 1];
        do {
          snprintf(newname, sizeof newname, "%.1018s%d", name, count);
          count++;
        } while (count < 10000 && atr_get_noparent(i, newname));
        strcpy(name, newname);
      }
      do_rawlog(LT_CHECK,
                " * Bad attribute name on #%d. Changing name to %s.\n", i,
                name);
      err = 0;
    }
    /* check the owner */
    owner = AL_CREATOR(list);
    if (!GoodObject(owner)) {
      do_rawlog(LT_CHECK, " * Bad owner on attribute %s on #%d.\n", name, i);
      owner = GOD;
      fixmemdb = 1;
    } else {
      owner = Owner(owner);
    }

    /* write that info out */
    db_write_labeled_string(f, " name", name);
    db_write_labeled_dbref(f, "  owner", owner);
    db_write_labeled_string(f, "  flags", atrflag_to_string(AL_FLAGS(list)));
    db_write_labeled_int(f, "  derefs", AL_DEREFS(list));

    /* now check the attribute */
    mush_strncpy(tbuf1, atr_value(list), sizeof tbuf1);
    /* get rid of unprintables and hard newlines */
    for (p = tbuf1; *p; p++) {
      if (!char_isprint(*p) && !isspace(*p) && *p != TAG_START &&
          *p != TAG_END && *p != ESC_CHAR && *p != BEEP_CHAR) {
        *p = '!';
        err = 1;
      }
    }
    if (err) {
      fixtext = fixmemdb = 1;
      do_rawlog(LT_CHECK, " * Bad text in attribute %s on #%d. Changed to:\n",
                name, i);
      do_rawlog(LT_CHECK, "%s\n", tbuf1);
    }
    db_write_labeled_string(f, "  value", tbuf1);

    if (flag && fixmemdb) {
      /* Fix the db in memory. */
      AL_CREATOR(list) = owner;

      if (fixtext) {
        char *t = compress(tbuf1);
        if (!t)
          return 0;

        chunk_delete(list->data);
        list->data = chunk_create(t, strlen(t), 0);
        free(t);
      }
      if (fixname) {
        /* Changing the name of the attribute means this can result in
         * writing the attribute to disk several times, and likely has
         * for many, many years. Shows how often this is used. Figure
         * out something eventually.
         */
        /*
          privbits flags = AL_FLAGS(list);
          atr_clr(i, AL_NAME(list), owner);
          (void) atr_add(i, name, tbuf1, owner, flags);
          list = atr_get_noparent(i, name);
          AL_FLAGS(list) = flags;
        */
      }
    }
  }
  return 0;
}

/** Write out the object database to disk, in paranoid mode.
 * \verbatim
 * This function writes the databsae out to disk, in paranoid mode.
 * The database structure currently looks something like this:
 * +V<header line>
 * +FLAGS LIST
 * <flag data>
 * ~<number of objects>
 * <object data>
 * \endverbatim
 * \param f file pointer to write to.
 * \param flag 0 for normal paranoid dump, 1 for debug paranoid dump.
 * \return the number of objects in the database (db_top)
 */
dbref
db_paranoid_write(PENNFILE *f, int flag)
{
  dbref i;
  int dbflag;

  /* print a header line to make a later conversion to 2.0 easier to do.
   * the odd choice of numbers is based on 256*x + 2 offset
   */
  dbflag = 5;
  dbflag += DBF_NO_CHAT_SYSTEM;
  dbflag += DBF_WARNINGS;
  dbflag += DBF_CREATION_TIMES;
  dbflag += DBF_SPIFFY_LOCKS;
  dbflag += DBF_NEW_STRINGS;
  dbflag += DBF_TYPE_GARBAGE;
  dbflag += DBF_SPLIT_IMMORTAL;
  dbflag += DBF_NO_TEMPLE;
  dbflag += DBF_LESS_GARBAGE;
  dbflag += DBF_AF_VISUAL;
  dbflag += DBF_VALUE_IS_COST;
  dbflag += DBF_LINK_ANYWHERE;
  dbflag += DBF_NO_STARTUP_FLAG;
  dbflag += DBF_AF_NODUMP;
  dbflag += DBF_NEW_FLAGS;
  dbflag += DBF_NEW_POWERS;
  dbflag += DBF_POWERS_LOGGED;
  dbflag += DBF_LABELS;
  dbflag += DBF_SPIFFY_AF_ANSI;
  dbflag += DBF_HEAR_CONNECT;
  dbflag += DBF_NEW_VERSIONS;

  do_rawlog(LT_CHECK, "PARANOID WRITE BEGINNING...\n");

  penn_fprintf(f, "+V%d\n", dbflag * 256 + 2);
  db_write_labeled_int(f, "dbversion", NDBF_VERSION);
  db_write_labeled_string(f, "savedtime", show_time(mudtime, 1));
  db_write_flags(f);
  penn_fprintf(f, "~%d\n", db_top);

  /* write out each object */
  for (i = 0; i < db_top; i++) {
#ifdef WIN32SERVICES
    /* Keep the service manager happy */
    if (shutdown_flag && (i & 0xFF) == 0)
      shutdown_checkpoint();
#endif
    if (IsGarbage(i))
      continue;
    penn_fprintf(f, "!%d\n", i);
    db_paranoid_write_object(f, i, flag);
    /* print out a message every so many objects */
    if (i % globals.paranoid_checkpt == 0)
      do_rawlog(LT_CHECK, "\t...wrote up to object #%d\n", i);
  }
  penn_fputs(EOD, f);
  do_rawlog(LT_CHECK, "\t...finished at object #%d\n", i - 1);
  do_rawlog(LT_CHECK, "END OF PARANOID WRITE.\n");
  return db_top;
}

/** Read in a long int.
 * \param f file pointer to read from.
 * \return long int read.
 */
long int
getref(PENNFILE *f)
{
  static char buf[BUFFER_LEN];
  if (!penn_fgets(buf, sizeof(buf), f)) {
    do_rawlog(LT_ERR, "Unexpected EOF at line %d", dbline);
    longjmp(db_err, 1);
  }
  dbline++;
  return parse_integer(buf);
}

/** Read in a uint32_t
 * \param f file pointer to read from.
 * \return uint32_t read.
 */
uint32_t
getref_u32(PENNFILE *f)
{
  static char buf[BUFFER_LEN];
  if (!penn_fgets(buf, sizeof(buf), f)) {
    do_rawlog(LT_ERR, "Unexpected EOF at line %d", dbline);
    longjmp(db_err, 1);
  }
  dbline++;
  return parse_uint32(buf, NULL, 10);
}

/** Read in a uint64_t
 * \param f file pointer to read from.
 * \return uint64_t read.
 */
uint64_t
getref_u64(PENNFILE *f)
{
  static char buf[BUFFER_LEN];
  if (!penn_fgets(buf, sizeof(buf), f)) {
    do_rawlog(LT_ERR, "Unexpected EOF at line %d", dbline);
    longjmp(db_err, 1);
  }
  dbline++;
  return parse_uint64(buf, NULL, 10);
}

/** Read in a string, into a static buffer.
 * This function reads a double-quoted escaped string of the form
 * written by putstring. The string is read into a static buffer
 * that is not allocated, so the return value must usually be copied
 * elsewhere.
 * \param f file pointer to read from.
 * \return pointer to static buffer containing string read.
 */
char *
getstring_noalloc(PENNFILE *f)
{
  static char buf[BUFFER_LEN];
  char *p;
  int c;

  p = buf;
  c = penn_fgetc(f);
  if (c == EOF) {
    do_rawlog(LT_ERR, "Unexpected EOF at line %d", dbline);
    longjmp(db_err, 1);
  } else if (c != '"') {
    for (;;) {
      if ((c == '\0') || (c == EOF) ||
          ((c == '\n') && ((p == buf) || (p[-1] != '\r')))) {
        *p = '\0';
        if (c == '\n')
          dbline++;
        return buf;
      }
      safe_chr(c, buf, &p);
      c = penn_fgetc(f);
    }
  } else {
    for (;;) {
      c = penn_fgetc(f);
      if (c == '"') {
        /* It's a closing quote if it's followed by \r or \n */
        c = penn_fgetc(f);
        if (c == '\r') {
          /* Get a possible \n, too */
          if ((c = penn_fgetc(f)) != '\n')
            penn_ungetc(c, f);
          else
            dbline++;
        } else if (c != '\n')
          penn_ungetc(c, f);
        *p = '\0';
        return buf;
      } else if (c == '\\') {
        c = penn_fgetc(f);
      }
      if ((c == '\0') || (c == EOF)) {
        *p = '\0';
        return buf;
      }
      safe_chr(c, buf, &p);
    }
  }
}

/** Read a boolexp from a file.
 * This function reads a boolexp from a file. It expects the format that
 * put_boolexp writes out.
 * \param f file pointer to read from.
 * \param type pointer to lock type being read.
 * \return pointer to boolexp read.
 */
boolexp
getboolexp(PENNFILE *f, const char *type)
{
  char *val;
  db_read_this_labeled_string(f, "key", &val);
  return parse_boolexp(GOD, val, type);
}

extern PRIV lock_privs[];

/** Read locks for an object.
 * This function is used for DBF_SPIFFY_LOCKS to read a whole list
 * of locks from an object and set them.
 * \param i dbref of the object.
 * \param f file pointer to read from.
 * \param c number of locks, or -1 if not yet known.
 */
void
get_new_locks(dbref i, PENNFILE *f, int c)
{
  char *val, *key;
  dbref creator;
  privbits flags;
  char type[BUFFER_LEN];
  boolexp b;
  int count = c, derefs = 0, found = 0;

  if (c < 0) {
    db_read_this_labeled_string(f, "lockcount", &val);
    count = parse_integer(val);
  }

  for (;;) {
    int ch;

    ch = penn_fgetc(f);
    penn_ungetc(ch, f);

    if (ch != ' ')
      break;

    found++;

    /* Name of the lock */
    db_read_this_labeled_string(f, "type", &val);
    strcpy(type, val);
    if (globals.indb_flags & DBF_LABELS) {
      db_read_this_labeled_dbref(f, "creator", &creator);
      db_read_this_labeled_string(f, "flags", &val);
      flags = string_to_privs(lock_privs, val, 0);
      db_read_this_labeled_int(f, "derefs", &derefs);
    } else {
      db_read_this_labeled_int(f, "creator", &creator);
      db_read_this_labeled_uint32(f, "flags", &flags);
    }
    /* boolexp */
    db_read_this_labeled_string(f, "key", &key);
    b = parse_boolexp_d(GOD, key, type, derefs);
    if (b == TRUE_BOOLEXP)
      /* Malformed lock key in the db! Oops. */
      do_rawlog(LT_ERR, "WARNING: Invalid lock key '%s' for lock #%d/%s!", key,
                i, type);
    else
      add_lock_raw(creator, i, type, b, flags);
  }

  if (found != count)
    do_rawlog(
      LT_ERR,
      "WARNING: Actual lock count (%d) different from expected count (%d).",
      found, count);
}

/** Free the entire database.
 * This function frees the name, attributes, and locks on every object
 * in the database, and then free the entire database structure and
 * resets db_top.
 */
void
db_free(void)
{
  dbref i;

  if (db) {

    for (i = 0; i < db_top; i++) {
      set_name(i, NULL);
      atr_free_all(i);
      free_locks(Locks(i));
    }

    free((char *) db);
    db = NULL;
    db_init = db_top = 0;
  }
}

/** Read an attribute list for an object from a file
 * \param f file pointer to read from.
 * \param i dbref for the attribute list.
 */
int
get_list(PENNFILE *f, dbref i)
{
  int c;
  char *p, *q;
  char tbuf1[BUFFER_LEN + 150];
  privbits flags;
  int count = 0;
  uint8_t derefs;
  ansi_string *as;
  char tbuf2[BUFFER_LEN];
  char *tb2;

  tbuf1[0] = '\0';
  while (1)
    switch (c = penn_fgetc(f)) {
    case ']': /* new style attribs, read name then value */
      /* Using getstring_noalloc here will cause problems with attribute
         names starting with ". This is probably a better fix than just
         disallowing " in attribute names. */
      penn_fgets(tbuf1, BUFFER_LEN + 150, f);
      if (!(p = strchr(tbuf1, '^'))) {
        do_rawlog(LT_ERR, "ERROR: Bad format on new attributes. object #%d", i);
        return -1;
      }
      *p++ = '\0';
      if (!(q = strchr(p, '^'))) {
        do_rawlog(LT_ERR, "ERROR: Bad format on new attribute %s. object #%d",
                  tbuf1, i);
        return -1;
      }
      *q++ = '\0';
      flags = parse_uinteger(q);
      /* Remove obsolete AF_NUKED flag and AF_STATIC, just in case */
      flags &= ~AF_NUKED;
      flags &= ~AF_STATIC;
      if (!(globals.indb_flags & DBF_AF_VISUAL)) {
        /* Remove AF_ODARK flag. If it wasn't there, set AF_VISUAL */
        if (!(flags & AF_ODARK))
          flags |= AF_VISUAL;
        flags &= ~AF_ODARK;
      }
      /* Read in the deref count for the attribute, or set it to 0 if not
         present. */
      q = strchr(q, '^');
      if (q++)
        derefs = parse_uinteger(q);
      else
        derefs = 0;
      /* We add the attribute assuming that atoi(p) is an ok dbref
       * since we haven't loaded the whole db and can't really tell
       * if it is or not. We'll fix this up at the end of the load
       */
      tb2 = getstring_noalloc(f);
      if (has_markup(tb2)) {
        as = parse_ansi_string(tb2);
        tb2 = tbuf2;
        safe_ansi_string(as, 0, as->len, tbuf2, &tb2);
        *(tb2) = '\0';
        tb2 = tbuf2;
        free_ansi_string(as);
      }

      atr_new_add(i, tbuf1, tb2, atoi(p), flags, derefs, 1);
      count++;
      /* Check removed for atoi(q) == 0  (which results in NOTHING for that
       * parameter, and thus no flags), since this eliminates 'visual'
       * attributes (which, if not built-in attrs, have a flag val of 0.)
       */
      break;
    case '>': /* old style attribs, die noisily */
      do_rawlog(LT_ERR, "ERROR: old-style attribute format in object %d", i);
      return -1;
      break;
    case '<': /* end of list */
      if ('\n' != penn_fgetc(f)) {
        do_rawlog(LT_ERR, "ERROR: no line feed after < on object %d", i);
        return -1;
      }
      return count;
    default:
      if (c == EOF) {
        do_rawlog(LT_ERR, "ERROR: Unexpected EOF on file.");
        return -1;
      }
      do_rawlog(LT_ERR,
                "ERROR: Bad character %c (%d) in attribute list on object %d",
                c, c, i);
      do_rawlog(LT_ERR,
                "  (expecting ], >, or < as first character of the line.)");
      if (*tbuf1)
        do_rawlog(LT_ERR, "  Last attribute read was: %s", tbuf1);
      else
        do_rawlog(LT_ERR, "  No attributes had been read yet.");
      return -1;
    }
}

extern PRIV attr_privs_view[];

/** Read an attribute list for an object from a file
 * \param f file pointer to read from.
 * \param i dbref for the attribute list.
 * \param count the number of attributes to read.
 */
void
db_read_attrs(PENNFILE *f, dbref i, int count)
{
  char name[ATTRIBUTE_NAME_LIMIT + 1];
  char value[BUFFER_LEN + 1];
  dbref owner;
  int derefs;
  privbits flags;
  char *tmp;
  int found = 0;
  ansi_string *as;

  attr_reserve(i, count);

  for (;;) {
    int c;

    c = penn_fgetc(f);
    penn_ungetc(c, f);

    if (c != ' ')
      break;

    found++;

    db_read_this_labeled_string(f, "name", &tmp);
    strcpy(name, tmp);
    db_read_this_labeled_dbref(f, "owner", &owner);
    db_read_this_labeled_string(f, "flags", &tmp);
    flags = string_to_privs(attr_privs_view, tmp, 0);
    db_read_this_labeled_int(f, "derefs", &derefs);
    db_read_this_labeled_string(f, "value", &tmp);
    strcpy(value, tmp);
    if (!(globals.indb_flags & DBF_SPIFFY_AF_ANSI)) {
      if (has_markup(value)) {
        char *vp = value;
        as = parse_ansi_string(value);
        safe_ansi_string(as, 0, as->len, value, &vp);
        *vp = '\0';
        free_ansi_string(as);
      }
    }
    atr_new_add(i, name, value, owner, flags, derefs, 1);
  }

  if (found != count)
    do_rawlog(LT_ERR,
              "WARNING: Actual attribute count (%d) different than "
              "expected count (%d).",
              found, count);
}

/** Read a non-labeled database from a file.
 * \param f the file to read from
 * \return number of objects in the database
 */
static dbref
db_read_oldstyle(PENNFILE *f)
{
  int c;
  dbref i;
  struct object *o;
  int temp = 0;
  time_t temp_time = 0;

  init_objdata();

  for (i = 0;; i++) {
    /* Loop invariant: we always begin at the beginning of a line. */
    errobj = i;
    c = penn_fgetc(f);
    switch (c) {
    /* make sure database is at least this big *1.5 */
    case '~':
      db_init = (getref(f) * 3) / 2;
      break;
    /* Use the MUSH 2.0 header stuff to see what's in this db */
    case '+':
      c = penn_fgetc(f); /* Skip the V */
      if (c == 'F') {
        (void) getstring_noalloc(f);
        flag_read_all(f, "FLAG");
      } else if (c == 'P') {
        (void) getstring_noalloc(f);
        flag_read_all(f, "POWER");
      } else {
        do_rawlog(LT_ERR, "Unrecognized database format!");
        return -1;
      }
      break;
    /* old fashioned database */
    case '#':
    case '&': /* zone oriented database */
      do_rawlog(LT_ERR, "ERROR: old style database.");
      return -1;
      break;
    /* new database */
    case '!': /* non-zone oriented database */
      /* make space */
      i = getref(f);
      db_grow(i + 1);
      /* read it in */
      o = db + i;
      set_name(i, getstring_noalloc(f));
      o->location = getref(f);
      o->contents = getref(f);
      o->exits = getref(f);
      o->next = getref(f);
      o->parent = getref(f);
      o->locks = NULL;
      get_new_locks(i, f, -1);
      o->owner = getref(f);
      o->zone = getref(f);
      s_Pennies(i, getref(f));
      if (globals.indb_flags & DBF_NEW_FLAGS) {
        o->type = getref(f);
        o->flags = string_to_bits("FLAG", getstring_noalloc(f));
      } else {
        int old_flags, old_toggles;
        old_flags = getref(f);
        old_toggles = getref(f);
        if ((o->type = type_from_old_flags(old_flags)) < 0) {
          do_rawlog(LT_ERR, "Unable to determine type of #%d\n", i);
          return -1;
        }
        o->flags =
          flags_from_old_flags("FLAG", old_flags, old_toggles, o->type);
      }

      add_object_table(i);

      /* We need to have flags in order to do this right, which is why
       * we waited until now
       */
      switch (Typeof(i)) {
      case TYPE_PLAYER:
        current_state.players++;
        current_state.garbage--;
        break;
      case TYPE_THING:
        current_state.things++;
        current_state.garbage--;
        break;
      case TYPE_EXIT:
        current_state.exits++;
        current_state.garbage--;
        break;
      case TYPE_ROOM:
        current_state.rooms++;
        current_state.garbage--;
        break;
      }

      if (IsPlayer(i) && (strlen(o->name) > (size_t) PLAYER_NAME_LIMIT)) {
        char buff[BUFFER_LEN]; /* The name plus a NUL */
        mush_strncpy(buff, o->name, PLAYER_NAME_LIMIT);
        set_name(i, buff);
        do_rawlog(LT_CHECK,
                  " * Name of #%d is longer than the maximum, truncating.\n",
                  i);
      } else if (!IsPlayer(i) && (strlen(o->name) > OBJECT_NAME_LIMIT)) {
        char buff[OBJECT_NAME_LIMIT + 1]; /* The name plus a NUL */
        mush_strncpy(buff, o->name, OBJECT_NAME_LIMIT);
        set_name(i, buff);
        do_rawlog(LT_CHECK,
                  " * Name of #%d is longer than the maximum, truncating.\n",
                  i);
      }

      if (!(globals.indb_flags & DBF_VALUE_IS_COST) && IsThing(i))
        s_Pennies(i, (Pennies(i) + 1) * 5);

      if (globals.indb_flags & DBF_NEW_POWERS) {
        o->powers = string_to_bits("POWER", getstring_noalloc(f));
      } else {
        int old_powers;
        old_powers = getref(f);
        o->powers = flags_from_old_flags("POWER", old_powers, 0, o->type);
      }

      /* If we've got a variable exit predating the link_anywhere power,
       * give it the link_anywhere power now.
       */
      if (!(globals.indb_flags & DBF_LINK_ANYWHERE)) {
        if (IsExit(i) && (Destination(i) == AMBIGUOUS))
          set_power_internal(i, "LINK_ANYWHERE");
      }

      /* Remove the STARTUP and ACCESSED flags */
      if (!(globals.indb_flags & DBF_NO_STARTUP_FLAG)) {
        clear_flag_internal(i, "STARTUP");
        clear_flag_internal(i, "ACCESSED");
      }

      /* Clear the GOING flags. If it was scheduled for destruction
       * when the db was saved, it gets a reprieve.
       */
      clear_flag_internal(i, "GOING");
      clear_flag_internal(i, "GOING_TWICE");
      /* If there are channels in the db, read 'em in */
      /* We don't support this anymore, so we just discard them */
      if (!(globals.indb_flags & DBF_NO_CHAT_SYSTEM))
        getref(f);

      /* If there are warnings in the db, read 'em in */
      temp = MAYBE_GET(f, DBF_WARNINGS);
      o->warnings = temp;
      /* If there are creation times in the db, read 'em in */
      temp_time = MAYBE_GET(f, DBF_CREATION_TIMES);
      if (temp_time)
        o->creation_time = (time_t) temp_time;
      else
        o->creation_time = mudtime;
      temp_time = MAYBE_GET(f, DBF_CREATION_TIMES);
      if (temp_time || IsPlayer(i))
        o->modification_time = (time_t) temp_time;
      else
        o->modification_time = o->creation_time;

      /* read attribute list for item */
      if ((o->attrcount = get_list(f, i)) < 0) {
        do_rawlog(LT_ERR, "ERROR: bad attribute list object %d", i);
        return -1;
      }
      if (!(globals.indb_flags & DBF_AF_NODUMP)) {
        /* Clear QUEUE and SEMAPHORE attributes */
        atr_clr(i, "QUEUE", GOD);
        atr_clr(i, "SEMAPHORE", GOD);
      }
      /* check to see if it's a player */
      if (IsPlayer(i)) {
        add_player(i);
        clear_flag_internal(i, "CONNECTED");
        /* If it has the MONITOR flag and the db predates HEAR_CONNECT, swap
         * them over */
        if (!(globals.indb_flags & DBF_HEAR_CONNECT) &&
            has_flag_by_name(i, "MONITOR", NOTYPE)) {
          clear_flag_internal(i, "MONITOR");
          set_flag_internal(i, "HEAR_CONNECT");
        }
      }

      if (IsRoom(i) && has_flag_by_name(i, "HAVEN", TYPE_ROOM)) {
        /* HAVEN flag is no longer settable on rooms. */
        clear_flag_internal(i, "HAVEN");
      }

      break;

    case '*': {
      char buff[80];
      penn_ungetc('*', f);
      penn_fgets(buff, sizeof buff, f);
      if (strcmp(buff, EOD) != 0) {
        do_rawlog(LT_ERR, "ERROR: No end of dump after object #%d", i - 1);
        return -1;
      } else {
        /** In newdb_version 4+, HAVEN defaults to PLAYER only, not PLAYER |
         * ROOM. */
        set_flag_type_by_name("FLAG", "HAVEN", TYPE_PLAYER);
        do_rawlog(LT_ERR, "READING: done");
        loading_db = 0;
        fix_free_list();
        dbck();
        log_mem_check();
        return db_top;
      }
    }
    default:
      do_rawlog(LT_ERR, "ERROR: failed object %d", i);
      return -1;
    }
  }
}

/** Read the object database from a file.
 * This function reads the entire database from a file. See db_write()
 * for some notes about the expected format.
 * \param f file pointer to read from.
 * \return number of objects in the database.
 */
dbref
db_read(PENNFILE *f)
{
  sqlite3 *sqldb;
  sqlite3_stmt *adder;
  int status;
  int c;
  dbref i = 0;
  char *tmp;
  struct object *o;
  int minimum_flags = DBF_NEW_STRINGS | DBF_TYPE_GARBAGE | DBF_SPLIT_IMMORTAL |
                      DBF_NO_TEMPLE | DBF_SPIFFY_LOCKS;

  log_mem_check();

  loading_db = 1;

  sqldb = get_shared_db();
  init_objdata();
  clear_players();
  db_free();
  globals.indb_flags = 1;

  c = penn_fgetc(f);
  if (c != '+') {
    do_rawlog(LT_ERR, "Database does not start with a version string");
    return -1;
  }
  c = penn_fgetc(f);
  if (c != 'V') {
    do_rawlog(LT_ERR, "Database does not start with a version string");
    return -1;
  }
  globals.indb_flags = ((getref(f) - 2) / 256) - 5;
  /* if you want to read in an old-style database, use an earlier
   * patchlevel to upgrade.
   */
  if (((globals.indb_flags & minimum_flags) != minimum_flags) ||
      (globals.indb_flags & DBF_NO_POWERS)) {
    do_rawlog(LT_ERR, "ERROR: Old database without required dbflags.");
    return -1;
  }

  if (!(globals.indb_flags & DBF_LABELS))
    return db_read_oldstyle(f);

  if ((globals.indb_flags & DBF_NEW_VERSIONS)) {
    db_read_this_labeled_int(f, "dbversion", &i);
    globals.new_indb_version = i;
  }

  db_read_this_labeled_string(f, "savedtime", &tmp);
  strcpy(db_timestamp, tmp);

  do_rawlog(LT_ERR, "Loading database saved on %s UTC", db_timestamp);

  sqlite3_exec(sqldb, "BEGIN TRANSACTION", NULL, NULL, NULL);
  adder = prepare_statement(sqldb, "INSERT INTO objects(dbref) VALUES (?)",
                            "objects.add");

  while ((c = penn_fgetc(f)) != EOF) {
    switch (c) {
    case '+':
      c = penn_fgetc(f);
      if (c == 'F') {
        (void) getstring_noalloc(f);
        flag_read_all(f, "FLAG");
      } else if (c == 'P') {
        (void) getstring_noalloc(f);
        flag_read_all(f, "POWER");
      } else if (c == 'A') {
        (void) getstring_noalloc(f);
        attr_read_all(f);
        if (globals.new_indb_version < 2) {
          add_new_attr("MONIKER",
                       AF_WIZARD | AF_NOPROG | AF_VISUAL | AF_LOCKED);
        }
        if (globals.new_indb_version < 5) {
          /* The MAILQUOTA attr was missing from some dbs for some reason */
          add_new_attr("MAILQUOTA",
                       AF_NOPROG | AF_NOCOPY | AF_LOCKED | AF_WIZARD);
        }
      } else {
        do_rawlog(LT_ERR, "Unrecognized database format!");
        sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return -1;
      }
      break;
    case '~':
      db_init = (getref(f) * 3) / 2;
      break;
    case '!':
      /* Read an object */
      {
        char *label, *value;
        /* Thre should be an entry in the enum and following table and
           switch for each top-level label associated with an
           object. Not finding a label is not an error; the default
           set in new_object() is used. Finding a label not listed
           below is an error. */
        enum known_labels {
          LBL_NAME,
          LBL_LOCATION,
          LBL_CONTENTS,
          LBL_EXITS,
          LBL_NEXT,
          LBL_PARENT,
          LBL_LOCKS,
          LBL_OWNER,
          LBL_ZONE,
          LBL_PENNIES,
          LBL_TYPE,
          LBL_FLAGS,
          LBL_POWERS,
          LBL_WARNINGS,
          LBL_CREATED,
          LBL_MODIFIED,
          LBL_ATTRS,
          LBL_ERROR
        };
        struct label_table {
          const char *label;
          enum known_labels tag;
        };
        struct label_table fields[] = {{"name", LBL_NAME},
                                       {"location", LBL_LOCATION},
                                       {"contents", LBL_CONTENTS},
                                       {"exits", LBL_EXITS},
                                       {"next", LBL_NEXT},
                                       {"parent", LBL_PARENT},
                                       {"lockcount", LBL_LOCKS},
                                       {"owner", LBL_OWNER},
                                       {"zone", LBL_ZONE},
                                       {"pennies", LBL_PENNIES},
                                       {"type", LBL_TYPE},
                                       {"flags", LBL_FLAGS},
                                       {"powers", LBL_POWERS},
                                       {"warnings", LBL_WARNINGS},
                                       {"created", LBL_CREATED},
                                       {"modified", LBL_MODIFIED},
                                       {"attrcount", LBL_ATTRS},
                                       /* Add new label types here. */
                                       {NULL, LBL_ERROR}},
                           *entry;
        enum known_labels the_label;

        i = getref(f);
        db_grow(i + 1);
        o = db + i;
        while (1) {
          c = penn_fgetc(f);
          penn_ungetc(c, f);
          /* At the start of another object or the EOD marker */
          if (c == '!' || c == '*')
            break;
          db_read_labeled_string(f, &label, &value);
          the_label = LBL_ERROR;
          /* Look up the right enum value in the label table */
          for (entry = fields; entry->label; entry++) {
            if (strcmp(entry->label, label) == 0) {
              the_label = entry->tag;
              break;
            }
          }
          switch (the_label) {
          case LBL_NAME:
            set_name(i, value);
            break;
          case LBL_LOCATION:
            o->location = qparse_dbref(value);
            break;
          case LBL_CONTENTS:
            o->contents = qparse_dbref(value);
            break;
          case LBL_EXITS:
            o->exits = qparse_dbref(value);
            break;
          case LBL_NEXT:
            o->next = qparse_dbref(value);
            break;
          case LBL_PARENT:
            o->parent = qparse_dbref(value);
            break;
          case LBL_LOCKS:
            get_new_locks(i, f, parse_integer(value));
            break;
          case LBL_OWNER:
            o->owner = qparse_dbref(value);
            break;
          case LBL_ZONE:
            o->zone = qparse_dbref(value);
            break;
          case LBL_PENNIES:
            s_Pennies(i, parse_integer(value));
            break;
          case LBL_TYPE:
            o->type = parse_integer(value);
            switch (Typeof(i)) {
            case TYPE_PLAYER:
              current_state.players++;
              current_state.garbage--;
              break;
            case TYPE_THING:
              current_state.things++;
              current_state.garbage--;
              break;
            case TYPE_EXIT:
              current_state.exits++;
              current_state.garbage--;
              break;
            case TYPE_ROOM:
              current_state.rooms++;
              current_state.garbage--;
              break;
            }
            break;
          case LBL_FLAGS:
            o->flags = string_to_bits("FLAG", value);
            /* Clear the GOING flags. If it was scheduled for destruction
             * when the db was saved, it gets a reprieve.
             */
            clear_flag_internal(i, "GOING");
            clear_flag_internal(i, "GOING_TWICE");
            break;
          case LBL_POWERS:
            o->powers = string_to_bits("POWER", value);
            break;
          case LBL_WARNINGS:
            o->warnings = parse_warnings(NOTHING, value);
            break;
          case LBL_CREATED:
            o->creation_time = (time_t) parse_integer(value);
            break;
          case LBL_MODIFIED:
            o->modification_time = (time_t) parse_integer(value);
            break;
          case LBL_ATTRS: {
            int attrcount = parse_integer(value);
            db_read_attrs(f, i, attrcount);
          } break;
          case LBL_ERROR:
          default:
            do_rawlog(LT_ERR, "Unrecognized field '%s' in object #%d", label,
                      i);
            sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            return -1;
          }
        }
        sqlite3_bind_int(adder, 1, i);
        do {
          status = sqlite3_step(adder);
        } while (is_busy_status(status));
        if (status != SQLITE_DONE) {
          do_rawlog(LT_ERR, "Unable to add #%d to objects table: %s", i,
                    sqlite3_errstr(status));
        }
        sqlite3_reset(adder);

        if (IsPlayer(i) && (strlen(o->name) > (size_t) PLAYER_NAME_LIMIT)) {
          char buff[BUFFER_LEN]; /* The name plus a NUL */
          mush_strncpy(buff, o->name, PLAYER_NAME_LIMIT);
          set_name(i, buff);
          do_rawlog(LT_CHECK,
                    " * Name of #%d is longer than the maximum, truncating.\n",
                    i);
        } else if (!IsPlayer(i) && (strlen(o->name) > OBJECT_NAME_LIMIT)) {
          char buff[OBJECT_NAME_LIMIT + 1]; /* The name plus a NUL */
          mush_strncpy(buff, o->name, OBJECT_NAME_LIMIT);
          set_name(i, buff);
          do_rawlog(LT_CHECK,
                    " * Name of #%d is longer than the maximum, truncating.\n",
                    i);
        }
        if (IsPlayer(i)) {
          add_player(i);
          clear_flag_internal(i, "CONNECTED");
          /* If it has the MONITOR flag and the db predates HEAR_CONNECT, swap
           * them over */
          if (!(globals.indb_flags & DBF_HEAR_CONNECT) &&
              has_flag_by_name(i, "MONITOR", NOTYPE)) {
            clear_flag_internal(i, "MONITOR");
            set_flag_internal(i, "HEAR_CONNECT");
          }
        }

        if (globals.new_indb_version < 4 && IsRoom(i) &&
            has_flag_by_name(i, "HAVEN", TYPE_ROOM)) {
          /* HAVEN flag is no longer settable on rooms. */
          clear_flag_internal(i, "HAVEN");
        }
      }
      break;
    case '*': {
      char buff[80];
      penn_ungetc('*', f);
      penn_fgets(buff, sizeof buff, f);
      if (strcmp(buff, EOD) != 0) {
        do_rawlog(LT_ERR, "ERROR: No end of dump after object #%d", i - 1);
        sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return -1;
      } else {
        if (globals.new_indb_version < 4) {
          /** In newdb_version 4+, HAVEN defaults to PLAYER only, not PLAYER |
           * ROOM. */
          set_flag_type_by_name("FLAG", "HAVEN", TYPE_PLAYER);
        }
        do_rawlog(LT_ERR, "READING: done");
        sqlite3_exec(sqldb, "COMMIT TRANSACTION", NULL, NULL, NULL);
        loading_db = 0;
        fix_free_list();
        dbck();
        log_mem_check();
        return db_top;
      }
    }
    default:
      do_rawlog(LT_ERR, "ERROR: failed object %d", i);
      sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
      return -1;
    }
  }
  sqlite3_exec(sqldb, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
  return -1;
}

static sqlite3 *penn_sqldb = NULL;
static sqlite3 *statement_cache = NULL;
static sqlite3_stmt *find_stmt = NULL;
static sqlite3_stmt *insert_stmt = NULL;
static sqlite3_stmt *delete_stmt = NULL;
static sqlite3_stmt *delete_all_stmts = NULL;
static sqlite3_stmt *find_all_stmts = NULL;

static int
comp_helper(const char *a, int lena, const char *b, int lenb)
{
  int i = 0;
  while (lena && lenb) {
    int d;
    lena -= 1;
    lenb -= 1;
    d = (int) a[i] - (int) b[i];
    if (d) {
      return d;
    }
    i += 1;
  }
  return lena - lenb;
}

/* sqlite3 collator function that sorts strings like foo100, foo1 in the order
 * foo1, foo100 */
static int
comp_trailing_numbers(void *data __attribute__((__unused__)), int lena,
                      const void *va, int lenb, const void *vb)
{
  const char *a = va;
  const char *b = vb;

  if (lena == 0 && lenb == 0) {
    return 0;
  } else if (lena == 0) {
    return -1;
  } else if (lenb == 0) {
    return 1;
  } else if (isdigit(a[lena - 1]) && isdigit(b[lenb - 1])) {
    int traila = lena - 1;
    int trailb = lenb - 1;
    int d;
    int na = 0, nb = 0;
    int place;

    place = 1;
    while (traila > 0 && isdigit(a[traila])) {
      na += place * (a[traila] - '0');
      place *= 10;
      traila -= 1;
    }
    traila += 1;

    place = 1;
    while (trailb > 0 && isdigit(b[trailb])) {
      nb += place * (b[trailb] - '0');
      place *= 10;
      trailb -= 1;
    }
    trailb += 1;

    d = comp_helper(a, traila, b, trailb);
    if (d) {
      return d;
    } else {
      return na - nb;
    }

  } else {
    return comp_helper(a, lena, b, lenb);
  }
}

/** Callback function for sqlite3_bind_text to free a
    mush_malloc-allocated "string" */
void
free_string(void *s)
{
  mush_free(s, "string");
}

static bool
optimize_shared_db(void *data __attribute__((__unused__)))
{
  if (penn_sqldb) {
    return optimize_db(penn_sqldb);
  } else {
    return false;
  }
}

/** Return a pointer to a global in-memory sql database. */
sqlite3 *
get_shared_db(void)
{
#if 1
  /* Normally use a ephemeral in-memory database */
  const char *sqldb_file = "file::memory:?cache=shared";
#else
  /* Use a file based one for testing. Not suitable for use with
   * @shutdown/reboot. */
  const char *sqldb_file = "mushsql.db";
#endif

  /* When merging with threaded branch, database connections should be
     handled by thread local storage - one connection per thread, all
     to the same db. */

  if (!penn_sqldb) {
    sqlite3_enable_shared_cache(1);
    penn_sqldb = open_sql_db(sqldb_file, 0);
    if (!penn_sqldb) {
      mush_panic("Unable to create sql database");
    }
    sq_register_loop(24 * 60 * 60 + 300, optimize_shared_db, NULL, NULL);
  }
  return penn_sqldb;
}

/** Close the shared database connection. Use only when a mush process
    or thread using a shared database connection exits. */
void
close_shared_db(void)
{
  /* When merging with threaded branch, this should instead be handled
     by the tls finalizer callback. */
  if (penn_sqldb) {
    close_sql_db(penn_sqldb);
    penn_sqldb = NULL;
  }
}

int sqlite3_spellfix_init(sqlite3 *db, char **pzErrMsg,
                          const sqlite3_api_routines *pApi);
int sqlite3_remember_init(sqlite3 *db, char **pzErrMsg,
                          const sqlite3_api_routines *pApi);

struct sql_regexp_data {
  pcre2_code *re;
  pcre2_match_data *md;
  bool free_study;
};

static void
sql_regexp_free(void *ptr)
{
  struct sql_regexp_data *d = ptr;
  pcre2_code_free(d->re);
  pcre2_match_data_free(d->md);
  free(d);
}

void
sql_regexp_fun(sqlite3_context *ctx, int nargs __attribute__((__unused__)),
               sqlite3_value **args)
{
  const unsigned char *subj;
  int subj_len;
  int nmatches;
  struct sql_regexp_data *d = sqlite3_get_auxdata(ctx, 0);

  if (sqlite3_value_type(args[0]) == SQLITE_NULL ||
      sqlite3_value_type(args[1]) == SQLITE_NULL) {
    return;
  }

  if (!d) {
    int errcode;
    PCRE2_SIZE erroff;
    d = malloc(sizeof *d);
    d->re =
      pcre2_compile(sqlite3_value_text(args[0]), sqlite3_value_bytes(args[0]),
                    PCRE2_ANCHORED | PCRE2_ENDANCHORED | PCRE2_UTF | PCRE2_UCP,
                    &errcode, &erroff, NULL);
    if (!d->re) {
      PCRE2_UCHAR errstr[120];
      pcre2_get_error_message(errcode, errstr, sizeof errstr);
      sqlite3_result_error(ctx, (const char *) errstr, -1);
      free(d);
      return;
    }
    d->md = pcre2_match_data_create_from_pattern(d->re, NULL);
    sqlite3_set_auxdata(ctx, 0, d, sql_regexp_free);
  }

  subj = sqlite3_value_text(args[1]);
  subj_len = sqlite3_value_bytes(args[1]);
  nmatches = pcre2_match(d->re, subj, subj_len, 0, 0, d->md, re_match_ctx);
  sqlite3_result_int(ctx, nmatches >= 0);
}

/* Turn a string holding a base-16 number into an int */
void
sql_from_hexstr_fun(sqlite3_context *ctx, int nargs __attribute__((unused)),
                    sqlite3_value **args)
{
  int t = sqlite3_value_type(args[0]);
  if (t != SQLITE_TEXT && t != SQLITE_BLOB) {
    return;
  }
  const char *hexstr = (const char *) sqlite3_value_text(args[0]);
  char *end;
  long num = strtol(hexstr, &end, 16);
  if (*end != '\0') {
    return;
  }
  sqlite3_result_int64(ctx, num);
}

/** Open a new connection to a sqlite3 database.
 * If given a NULL file, returns a NEW in-memory database.
 * A zero-length file, returns a new temporary-file based database.
 * Otherwise opens the given file.
 *
 * The connection should only be used in a single thread.
 *
 * \param name the database filename to open URI names are supported.
 * \param nocreate true if the database should not be created if not already
 * present. \return a handle to the database connection or NULL.
 */
sqlite3 *
open_sql_db(const char *name, bool nocreate)
{
  sqlite3 *db = NULL;
  int status;
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI | SQLITE_OPEN_NOMUTEX;

  if (!name) {
    name = ":memory:";
  }

  if (!nocreate) {
    flags |= SQLITE_OPEN_CREATE;
  }

  if ((status = sqlite3_open_v2(name, &db, flags, NULL)) != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to open sqlite3 database %s: %s",
              *name ? name : ":unnamed:",
              db ? sqlite3_errmsg(db) : sqlite3_errstr(status));
    if (db) {
      sqlite3_close(db);
    }
    return NULL;
  }
  if ((status = sqlite3_create_collation(db, "TRAILNUMBERS", SQLITE_UTF8, NULL,
                                         comp_trailing_numbers)) != SQLITE_OK) {
    do_rawlog(LT_ERR,
              "Unable to attach TRAILNUMBERS collator to database %s: %s",
              *name ? name : ":unnamed:", sqlite3_errmsg(db));
  }

#ifdef HAVE_ICU
  // Delete the ICU version
  sqlite3_create_function(db, "regexp", 2, SQLITE_ANY | SQLITE_DETERMINISTIC,
                          NULL, NULL, NULL, NULL);
#endif
  if ((status = sqlite3_create_function(
         db, "regexp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
         sql_regexp_fun, NULL, NULL)) != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to register sqlite3 regexp() function: %s",
              sqlite3_errmsg(db));
  }
  if ((status = sqlite3_create_function(
         db, "from_hexstr", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
         sql_from_hexstr_fun, NULL, NULL)) != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to register sqlite3 from_hexstr() function: %s",
              sqlite3_errmsg(db));
  }
  sqlite3_spellfix_init(db, NULL, NULL);
  sqlite3_remember_init(db, NULL, NULL);
  sqlite3_busy_timeout(db, 250);

  sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);

  return db;
}

/** Returns the application_id and user_version fields from a sqlite3 database.
 *
 * \param db the database connection
 * \param app_id filled with the application_id value.
 * \param version filled with the user_version value.
 * \return 0 on success, -1 on error.
 */
int
get_sql_db_id(sqlite3 *db, int *app_id, int *version)
{
  sqlite3_stmt *vers;
  int status;
  vers = prepare_statement_cache(db, "PRAGMA application_id", "app.id", 0);
  if (!vers) {
    return -1;
  }
  do {
    status = sqlite3_step(vers);
    if (status == SQLITE_ROW) {
      *app_id = sqlite3_column_int(vers, 0);
    }
  } while (is_busy_status(status));
  sqlite3_finalize(vers);
  if (status != SQLITE_ROW) {
    return -1;
  }

  vers = prepare_statement_cache(db, "PRAGMA user_version", "user.version", 0);
  if (!vers) {
    return -1;
  }
  do {
    status = sqlite3_step(vers);
    if (status == SQLITE_ROW) {
      *version = sqlite3_column_int(vers, 0);
    }
  } while (is_busy_status(status));
  sqlite3_finalize(vers);
  if (status != SQLITE_ROW) {
    return -1;
  } else {
    return 0;
  }
}

/** Run PRAGMA optmize on a sqlite3 database */
bool
optimize_db(sqlite3 *db)
{
  char *err;
  if (sqlite3_exec(db, "PRAGMA optimize", NULL, NULL, &err) != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to optimize database: %s", err);
    sqlite3_free(err);
    return false;
  } else {
    return true;
  }
}

/** Returns true if the sqlite status code indicates an operation is
    waiting on another process or thread to unlock a database. */
bool
is_busy_status(int s)
{
  return s == SQLITE_BUSY || s == SQLITE_LOCKED;
}

/** Close a currently-opened sqlite3 handle, cleaning up
   any cached prepared statements first. */
void
close_sql_db(sqlite3 *db)
{
  int status;

  /* Finalize any cached prepared statements associated with this
     connection. */
  if (statement_cache) {
    if (!delete_all_stmts) {
      const char query[] = "DELETE FROM prepared_cache WHERE handle = ?";
      if ((status = sqlite3_prepare_v3(statement_cache, query, sizeof query,
                                       SQLITE_PREPARE_PERSISTENT,
                                       &delete_all_stmts, NULL)) != SQLITE_OK) {
        do_rawlog(LT_ERR,
                  "Unable to prepare query statement_cache.delete_all: %s",
                  sqlite3_errmsg(db));
        delete_all_stmts = NULL;
      }
    }
    if (!find_all_stmts) {
      const char query[] =
        "SELECT statement FROM prepared_cache WHERE handle = ?";
      if ((status = sqlite3_prepare_v3(statement_cache, query, sizeof query,
                                       SQLITE_PREPARE_PERSISTENT,
                                       &find_all_stmts, NULL)) != SQLITE_OK) {
        do_rawlog(LT_ERR,
                  "Unable to prepare query statement_cache.find_all: %s",
                  sqlite3_errmsg(db));
        find_all_stmts = NULL;
      }
    }

    if (find_all_stmts) {
      sqlite3_bind_int64(find_all_stmts, 1, (intptr_t) db);
      do {
        status = sqlite3_step(find_all_stmts);
        if (status == SQLITE_ROW) {
          sqlite3_stmt *stmt = (sqlite3_stmt *) ((
            intptr_t) sqlite3_column_int64(find_all_stmts, 0));
          sqlite3_finalize(stmt);
        }
      } while (status == SQLITE_ROW || is_busy_status(status));
      sqlite3_reset(find_all_stmts);
    }

    if (delete_all_stmts) {
      sqlite3_bind_int64(delete_all_stmts, 1, (intptr_t) db);
      do {
        status = sqlite3_step(delete_all_stmts);
      } while (status != SQLITE_DONE && is_busy_status(status));
      sqlite3_reset(delete_all_stmts);
    }
  }
  sqlite3_exec(db, "PRAGMA optimize", NULL, NULL, NULL);
  sqlite3_close_v2(db);
}

/** Return a cached prepared statement, or create and cache it if not
 * already present.
 *
 * \param db the sqlite3 database connection to use.
 * \param query the SQL query to prepare, in UTF-8.
 * \param name the name of the query. (db,name) is the cache key, not the actual
 * text of the query, in UTF-8.
 * \param cache if true, cache the query, if false not and it needs to be
 * cleaned up with sqlite3_finalize(). \return the prepared statement, NULL on
 * errors.
 */
sqlite3_stmt *
prepare_statement_cache(sqlite3 *db, const char *query, const char *name,
                        bool cache)
{
  sqlite3_stmt *stmt = NULL;
  int status;
  int flags = cache ? SQLITE_PREPARE_PERSISTENT : 0;

  /* When merging with the threaded branch this probably needs a
   * mutex. Also think about if the cache can be tables in the general
   * shared db. */

  /* Set up prepared statement cache database */
  if (!statement_cache) {
    char *errmsg;
    if ((status = sqlite3_open_v2(":memory:", &statement_cache,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                    SQLITE_OPEN_FULLMUTEX,
                                  NULL)) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to create prepared statement cache: %s",
                db ? sqlite3_errmsg(statement_cache) : sqlite3_errstr(status));
      if (statement_cache) {
        sqlite3_close(statement_cache);
        statement_cache = NULL;
      }
      return NULL;
    }

    if (sqlite3_exec(
          statement_cache,
          "CREATE TABLE prepared_cache(handle INTEGER NOT NULL, name TEXT NOT "
          "NULL, statement INTEGER NOT NULL, PRIMARY KEY(handle, name)) "
          "WITHOUT ROWID",
          NULL, NULL, &errmsg) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to build prepared statement cache table: %s",
                errmsg);
      sqlite3_free(errmsg);
      sqlite3_close(statement_cache);
      statement_cache = NULL;
      return NULL;
    }
  }

  if (!find_stmt) {
    const char fquery[] =
      "SELECT statement FROM prepared_cache WHERE handle = ? AND name = ?";
    if ((status = sqlite3_prepare_v3(statement_cache, fquery, sizeof fquery,
                                     SQLITE_PREPARE_PERSISTENT, &find_stmt,
                                     NULL)) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to prepare query statement_cache.find: %s",
                sqlite3_errmsg(statement_cache));
      find_stmt = NULL;
      return NULL;
    }
  }

  if (!insert_stmt) {
    const char iquery[] =
      "INSERT INTO prepared_cache(handle, name, statement) VALUES (?,?,?)";
    if ((status = sqlite3_prepare_v3(statement_cache, iquery, sizeof iquery,
                                     SQLITE_PREPARE_PERSISTENT, &insert_stmt,
                                     NULL)) != SQLITE_OK) {
      do_rawlog(LT_ERR, "Unable to prepare query statement_cache.insert: %s",
                sqlite3_errmsg(statement_cache));
      insert_stmt = NULL;
      return NULL;
    }
  }

  if (cache) {
    /* See if the statement is cached and return it if so */
    sqlite3_bind_int64(find_stmt, 1, (intptr_t) db);
    sqlite3_bind_text(find_stmt, 2, name, strlen(name), SQLITE_TRANSIENT);
    do {
      status = sqlite3_step(find_stmt);
      if (status == SQLITE_ROW) {
        stmt = (sqlite3_stmt *) ((intptr_t) sqlite3_column_int64(find_stmt, 0));
        sqlite3_reset(find_stmt);
        return stmt;
      }
    } while (is_busy_status(status));
    sqlite3_reset(find_stmt);
  }

  /* Prepare a new statement and cache it. */
  if ((status = sqlite3_prepare_v3(db, query, -1, flags, &stmt, NULL)) !=
      SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to prepare query %s: %s", name,
              sqlite3_errmsg(db));
    return NULL;
  }

  if (cache) {
    sqlite3_bind_int64(insert_stmt, 1, (intptr_t) db);
    sqlite3_bind_text(insert_stmt, 2, name, strlen(name), SQLITE_TRANSIENT);
    sqlite3_bind_int64(insert_stmt, 3, (intptr_t) stmt);
    do {
      status = sqlite3_step(insert_stmt);
    } while (is_busy_status(status));
    sqlite3_reset(insert_stmt);
  }

  return stmt;
}

/** Finalize a cached prepared statement and clear it from the cache.
 *
 * \param db the statement to delete.
 */
void
close_statement(sqlite3_stmt *stmt)
{

  /* When merging with the threaded branch this probably needs a mutex. */

  if (statement_cache) {
    int status;
    if (!delete_stmt) {
      const char query[] = "DELETE FROM prepared_cache WHERE statement = ?";

      if ((status = sqlite3_prepare_v3(statement_cache, query, sizeof query,
                                       SQLITE_PREPARE_PERSISTENT, &delete_stmt,
                                       NULL)) != SQLITE_OK) {
        do_rawlog(LT_ERR,
                  "Unable to prepare query statement_cache.delete_one: %s",
                  sqlite3_errmsg(statement_cache));
        delete_stmt = NULL;
        return;
      }
    }

    sqlite3_bind_int64(delete_stmt, 1, (intptr_t) stmt);
    do {
      status = sqlite3_step(delete_stmt);
    } while (is_busy_status(status));
    sqlite3_reset(delete_stmt);
  }

  sqlite3_finalize(stmt);
}

static void
init_objdata()
{
  const char *create_query =
    "CREATE TABLE objects(dbref INTEGER NOT NULL PRIMARY KEY, queue INTEGER "
    "NOT NULL DEFAULT 0);"
    "CREATE TABLE objdata(dbref INTEGER NOT NULL, key TEXT NOT NULL, ptr "
    "INTEGER, PRIMARY KEY (dbref, key), FOREIGN KEY(dbref) REFERENCES "
    "objects(dbref) ON DELETE CASCADE) WITHOUT ROWID;";
  char *errmsg = NULL;
  sqlite3 *sqldb = get_shared_db();

  if (sqlite3_exec(sqldb, create_query, NULL, NULL, &errmsg) != SQLITE_OK) {
    do_rawlog(LT_ERR, "Unable to create objdata table: %s", errmsg);
    sqlite3_free(errmsg);
    return;
  }
}

/** Add data to the object data hashtable.
 * This hash table is typically used to store transient object data
 * that is built at database load and isn't saved to disk, but it
 * can be used for other purposes as well - it's a good general
 * tool for hackers who want to add their own data to objects.
 * This function adds data to the hashtable. NULL data cleared
 * that particular keybase/object entry. It does not free the
 * data pointer.
 * \param thing dbref of object to associate the data with.
 * \param keybase base string for type of data, in UTF-8.
 * \param data pointer to the data to store.
 * \return data passed in.
 */
void *
set_objdata(dbref thing, const char *keybase, void *data)
{
  sqlite3_stmt *setter;
  int status;
  sqlite3 *sqldb;

  if (data == NULL) {
    delete_objdata(thing, keybase);
    return NULL;
  }

  sqldb = get_shared_db();
  setter =
    prepare_statement(sqldb,
                      "INSERT INTO objdata(dbref, key, ptr) VALUES(?, ?, ?) ON "
                      "CONFLICT (dbref, key) DO UPDATE SET ptr=excluded.ptr",
                      "objdata.set");
  if (!setter) {
    return NULL;
  }

  sqlite3_bind_int(setter, 1, thing);
  sqlite3_bind_text(setter, 2, keybase, strlen(keybase), SQLITE_STATIC);
  sqlite3_bind_int64(setter, 3, (intptr_t) data);

  do {
    status = sqlite3_step(setter);
  } while (is_busy_status(status));

  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Unable to execute objdata set query for #%d/%s: %s",
              thing, keybase, sqlite3_errmsg(sqldb));
  }
  sqlite3_reset(setter);

  return data;
}

/** Retrieve data from the object data hashtable.
 * \param thing dbref of object data is associated with.
 * \param keybase base string for type of data, in UTF-8.
 * \return data stored for that object and keybase, or NULL.
 */
void *
get_objdata(dbref thing, const char *keybase)
{
  sqlite3_stmt *getter;
  int status;
  void *data = NULL;
  sqlite3 *sqldb = get_shared_db();

  getter = prepare_statement(
    sqldb, "SELECT ptr FROM objdata WHERE dbref = ? AND key = ?",
    "objdata.get");
  if (!getter) {
    return NULL;
  }

  sqlite3_bind_int(getter, 1, thing);
  sqlite3_bind_text(getter, 2, keybase, strlen(keybase), SQLITE_STATIC);
  do {
    status = sqlite3_step(getter);
  } while (is_busy_status(status));
  if (status == SQLITE_ROW) {
    data = (void *) ((intptr_t) sqlite3_column_int64(getter, 0));
  } else if (status != SQLITE_DONE) {
    do_rawlog(LT_TRACE, "Unable to execute objdata get query for #%d/%s: %s",
              thing, keybase, sqlite3_errmsg(sqldb));
  }
  sqlite3_reset(getter);
  return data;
}

/** Clear an object's data for a specific key.
 * \param thing dbref of object data is associated with.
 * \param keybase the key to remove, in UTF-8.
 */
void
delete_objdata(dbref thing, const char *keybase)
{
  sqlite3_stmt *deleter;
  int status;
  sqlite3 *sqldb = get_shared_db();

  deleter = prepare_statement(
    sqldb, "DELETE FROM objdata WHERE dbref = ? AND key = ?", "objdata.delete");
  if (!deleter) {
    return;
  }

  sqlite3_bind_int(deleter, 1, thing);
  sqlite3_bind_text(deleter, 2, keybase, strlen(keybase), SQLITE_STATIC);
  do {
    status = sqlite3_step(deleter);
  } while (is_busy_status(status));

  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Unable to execute objdata delete query for #%d: %s",
              thing, sqlite3_errmsg(sqldb));
  }
  sqlite3_reset(deleter);
}

static void
add_object_table(dbref obj)
{
  sqlite3 *sqldb;
  sqlite3_stmt *adder;
  int status;

  sqldb = get_shared_db();
  adder = prepare_statement(sqldb, "INSERT INTO objects(dbref) VALUES (?)",
                            "objects.add");
  sqlite3_bind_int(adder, 1, obj);
  do {
    status = sqlite3_step(adder);
  } while (is_busy_status(status));
  if (status != SQLITE_DONE) {
    do_rawlog(LT_ERR, "Unable to add #%d to objects table: %s", obj,
              sqlite3_errmsg(sqldb));
  }
  sqlite3_reset(adder);
}

/** Create a basic 3-object (Start Room, God, Master Room) database. */
void
create_minimal_db(void)
{
  dbref start_room, god, master_room;
  uint32_t desc_flags = AF_VISUAL | AF_NOPROG | AF_PREFIXMATCH | AF_PUBLIC;

  init_objdata();

  start_room = new_object();  /* #0 */
  god = new_object();         /* #1 */
  master_room = new_object(); /* #2 */

  if (!READ_REMOTE_DESC)
    desc_flags |= AF_NEARBY;

  set_name(start_room, "Room Zero");
  Type(start_room) = TYPE_ROOM;
  Flags(start_room) = string_to_bits("FLAG", "LINK_OK");
  atr_new_add(start_room, "DESCRIBE", "You are in Room Zero.", GOD, desc_flags,
              1, 1);
  CreTime(start_room) = ModTime(start_room) = mudtime;
  current_state.rooms++;

  set_name(god, "One");
  Type(god) = TYPE_PLAYER;
  Flags(god) = string_to_bits("FLAG", "WIZARD");
  Location(god) = start_room;
  Home(god) = start_room;
  Owner(god) = god;
  CreTime(god) = mudtime;
  ModTime(god) = (time_t) 0;
  add_lock(god, god, Basic_Lock, parse_boolexp(god, "=me", Basic_Lock),
           LF_DEFAULT);
  add_lock(god, god, Enter_Lock, parse_boolexp(god, "=me", Enter_Lock),
           LF_DEFAULT);
  add_lock(god, god, Use_Lock, parse_boolexp(god, "=me", Use_Lock), LF_DEFAULT);
  atr_new_add(god, "DESCRIBE", "You see Number One.", god, desc_flags, 1, 1);
  atr_new_add(god, "MAILCURF", "0", god, AF_LOCKED | AF_NOPROG | AF_WIZARD, 1,
              1);
  add_folder_name(god, 0, "inbox");
  PUSH(god, Contents(start_room));
  add_player(god);
  s_Pennies(god, START_BONUS);
  local_data_create(god);
  current_state.players++;

  set_name(master_room, "Master Room");
  Type(master_room) = TYPE_ROOM;
  Flags(master_room) = string_to_bits("FLAG", "FLOATING");
  Owner(master_room) = god;
  CreTime(master_room) = ModTime(master_room) = mudtime;
  atr_new_add(master_room, "DESCRIBE",
              "This is the master room. Any exit in here is considered global. "
              "The same is true for objects with $-commands placed here.",
              god, desc_flags, 1, 1);
  current_state.rooms++;

  init_chatdb();
  mail_init();
}

/** Run a function, and jump if error */
/* Uncomment the below line to help with debugging if needed. */
/* static OUTPUT(int) __attribute__((noinline)); */
static inline void
OUTPUT(int r)
{
  if (r < 0)
    longjmp(db_err, 1);
}

/* Wrapper for fopen for use in reboot code. */
PENNFILE *
penn_fopen(const char *filename, const char *mode)
{
  PENNFILE *pf;

  pf = mush_malloc(sizeof *pf, "pennfile");
  pf->type = PFT_FILE;
  pf->handle.f = fopen(filename, mode);
  if (!pf->handle.f) {
    do_rawlog(LT_ERR, "Unable to open %s in mode '%s': %s", filename, mode,
              strerror(errno));
    mush_free(pf, "pennfile");
    return NULL;
  }
  return pf;
}

/* Close a db file, which may really be a pipe */
void
penn_fclose(PENNFILE *pf)
{
  switch (pf->type) {
  case PFT_PIPE:
#ifndef WIN32
    pclose(pf->handle.f);
#endif
    break;
  case PFT_FILE:
    fclose(pf->handle.f);
    break;
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    gzclose(pf->handle.g);
#endif
    break;
  }
  mush_free(pf, "pennfile");
}

int
penn_fgetc(PENNFILE *f)
{
  switch (f->type) {
  case PFT_FILE:
  case PFT_PIPE:
#ifdef HAVE_GETC_UNLOCKED
    return getc_unlocked(f->handle.f);
#else
    return getc(f->handle.f);
#endif
    break;
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    return gzgetc(f->handle.g);
#endif
    break;
  }
  return 0;
}

char *
penn_fgets(char *buf, int len, PENNFILE *pf)
{
  switch (pf->type) {
  case PFT_FILE:
  case PFT_PIPE:
#ifdef HAVE_FGETS_UNLOCKED
    return fgets_unlocked(buf, len, pf->handle.f);
#else
    return fgets(buf, len, pf->handle.f);
#endif
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    return gzgets(pf->handle.g, buf, len);
#endif
    break;
  }
  return NULL;
}

/* c should not be a negative value or it'll screw up gzputc return value
 * testing */
int
penn_fputc(int c, PENNFILE *f)
{
  switch (f->type) {
  case PFT_FILE:
  case PFT_PIPE:
#ifdef HAVE_PUTC_UNLOCKED
    OUTPUT(putc_unlocked(c, f->handle.f));
#else
    OUTPUT(putc(c, f->handle.f));
#endif
    break;
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    OUTPUT(gzputc(f->handle.g, c));
#endif
    break;
  }
  return 0;
}

int
penn_fputs(const char *s, PENNFILE *f)
{
  switch (f->type) {
  case PFT_FILE:
  case PFT_PIPE:
#ifdef HAVE_FPUTS_UNLOCKED
    OUTPUT(fputs_unlocked(s, f->handle.f));
#else
    OUTPUT(fputs(s, f->handle.f));
#endif
    break;
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    OUTPUT(gzputs(f->handle.g, s));
#endif
    break;
  }
  return 0;
}

int
penn_fprintf(PENNFILE *f, const char *fmt, ...)
{
  va_list ap;
  int r = -1;

  switch (f->type) {
  case PFT_FILE:
  case PFT_PIPE:
    va_start(ap, fmt);
    r = vfprintf(f->handle.f, fmt, ap);
    va_end(ap);
    if (r < 0)
      longjmp(db_err, 1);
    break;
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
#ifdef HAVE_GZVPRINTF
    va_start(ap, fmt);
    /* Total length of outputted string can't be more than 64K */
    r = gzvprintf(f->handle.g, fmt, ap);
    va_end(ap);
    if (r <= 0)
      longjmp(db_err, 1);
#elif defined(HAVE_VASPRINTF)
  { /* Safe GNU/BSD way */
    char *line = NULL;
    va_start(ap, fmt);
    r = vasprintf(&line, fmt, ap);
    va_end(ap);
    if (r > -1) {
      OUTPUT(gzputs(f->handle.g, line));
      free(line);
    } else
      longjmp(db_err, 1);
  }
#else
  {
    char line[BUFFER_LEN * 2];

    va_start(ap, fmt);
    r = mush_vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (r > -1)
      OUTPUT(gzputs(f->handle.g, line));
    else
      longjmp(db_err, 1);
  }
#endif
#endif
    break;
  }
  return r;
}

int
penn_ungetc(int c, PENNFILE *f)
{
  switch (f->type) {
  case PFT_FILE:
  case PFT_PIPE:
    OUTPUT(ungetc(c, f->handle.f));
    break;
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    OUTPUT(gzungetc(c, f->handle.g));
#endif
    break;
  }
  return c;
}

int
penn_feof(PENNFILE *pf)
{
  switch (pf->type) {
  case PFT_FILE:
  case PFT_PIPE:
    return feof(pf->handle.f);
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    return gzeof(pf->handle.g);
#endif
    break;
  }
  return 0;
}
