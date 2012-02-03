/**
 * \file db.c
 *
 * \brief Loading and saving the PennMUSH object database.
 *
 *
 */

#include "copyrite.h"
#include "config.h"
#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef I_SYS_TIME
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
#include "conf.h"
#include "dbio.h"
#include "externs.h"
#include "mushdb.h"
#include "attrib.h"
#include "mymalloc.h"
#include "game.h"
#include "flags.h"
#include "lock.h"
#include "dbdefs.h"
#include "log.h"
#include "strtree.h"
#include "parse.h"
#include "privtab.h"
#include "htab.h"
#include "extchat.h"
#include "extmail.h"
#include "confmagic.h"
#include "ansi.h"

#ifdef WIN32
#pragma warning( disable : 4761)        /* disable warning re conversion */
#endif

#ifdef WIN32SERVICES
void shutdown_checkpoint(void);
#endif

/** Get a ref out of the database if a given db flag is set */
#define MAYBE_GET(f,x) \
        (globals.indb_flags & (x)) ? getref(f) : 0


int loading_db = 0;   /**< Are we loading the database? */

char db_timestamp[100]; /**< Time the read database was saved. */

struct object *db = NULL; /**< The object db array */
dbref db_top = 0;         /**< The number of objects in the db array */

dbref errobj;             /**< Dbref of object on which an error has occurred */

int dbline = 0;           /**< Line of the database file being read */

/** String that markes the end of dumps */
const char *EOD = "***END OF DUMP***\n";

#ifndef DB_INITIAL_SIZE
#define DB_INITIAL_SIZE 5000   /**< Initial size for db array */
#endif                          /* DB_INITIAL_SIZE */

dbref db_size = DB_INITIAL_SIZE;  /**< Current size of db array */

HASHTAB htab_objdata;         /**< Object data hash table */
HASHTAB htab_objdata_keys;    /**< Object data keys hash table */

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
static void init_objdata_htab(int size, void (*free_data) (void *));
static void db_write_flags(PENNFILE *f);
static dbref db_read_oldstyle(PENNFILE *f);

StrTree object_names;       /**< String tree of object names */
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

int db_init = 0;  /**< Has the db array been initialized yet? */

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
      if ((db = (struct object *)
           malloc(db_size * sizeof(struct object))) == NULL) {
        do_rawlog(LT_ERR, "ERROR: out of memory while creating database!");
        abort();
      }
    }
    /* maybe grow it */
    if (db_top > db_size) {
      /* make sure it's big enough */
      while (db_top > db_size)
        db_size *= 2;
      if ((newdb = (struct object *)
           realloc(db, db_size * sizeof(struct object))) == NULL) {
        do_rawlog(LT_ERR, "ERROR: out of memory while extending database!");
        abort();
      }
      db = newdb;
    }
    while (initialized < db_top) {
      o = db + initialized;
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
      o->flags = NULL;
      o->powers = NULL;
      o->warnings = 0;
      o->modification_time = o->creation_time = mudtime;
      o->attrcount = 0;
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
  if (current_state.garbage)
    current_state.garbage--;
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
      penn_fputc((unsigned char) *s, f);
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
    if (c != '_' && c != '-' && c != '!' && c != '.' && c != '>' && c != '<' && c != '#' &&     /* these really should only be first time */
        !isalnum(c)) {
      do_rawlog(LT_ERR, "DB: Illegal character '%c'(%d) in label, line %d",
                c, c, dbline);
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
      if (c != '_' && c != '-' && c != '!' && c != '.' &&
          c != '#' && !isalnum(c) && !isspace(c)) {
        do_rawlog(LT_ERR, "DB: Illegal character '%c'(%d) in value, line %d",
                  c, c, dbline);
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
  for (list = o->list; list; list = AL_NEXT(list)) {
    if (AF_Nodump(list))
      continue;
    count++;
  }
  db_write_labeled_int(f, "attrcount", count);

  for (list = o->list; list; list = AL_NEXT(list)) {
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

  penn_fprintf(f, "+V%d\n", dbflag * 256 + 2);

  db_write_labeled_string(f, "savedtime", show_time(mudtime, 1));

  db_write_flags(f);

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
  penn_fprintf(f, "+FLAGS LIST\n");
  flag_write_all(f, "FLAG");
  penn_fprintf(f, "+POWER LIST\n");
  flag_write_all(f, "POWER");
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
  ALIST *list, *next;
  char name[BUFFER_LEN];
  char tbuf1[BUFFER_LEN];
  int err = 0;
  char *p;
  dbref owner;
  int fixmemdb = 0;
  int count = 0;
  int attrcount = 0;

  o = db + i;
  db_write_obj_basic(f, i, o);
  /* fflush(f); */

  /* write the attribute list, scanning */
  for (list = o->list; list; list = AL_NEXT(list)) {
    if (AF_Nodump(list))
      continue;
    attrcount++;
  }

  db_write_labeled_int(f, "attrcount", attrcount);

  for (list = o->list; list; list = next) {
    next = AL_NEXT(list);
    if (AF_Nodump(list))
      continue;
    fixmemdb = err = 0;
    /* smash unprintable characters in the name, replace with ! */
    strcpy(name, AL_NAME(list));
    for (p = name; *p; p++) {
      if (!isprint((unsigned char) *p) || isspace((unsigned char) *p)) {
        *p = '!';
        fixmemdb = err = 1;
      }
    }
    if (err) {
      /* If name already exists on this object, try adding a
       * number to the end. Give up if we can't find one < 10000
       */
      if (atr_get_noparent(i, name)) {
        count = 0;
        do {
          name[BUFFER_LEN - 6] = '\0';
          snprintf(tbuf1, BUFFER_LEN, "%s%d", name, count);
          count++;
        } while (count < 10000 && atr_get_noparent(i, tbuf1));
        mush_strncpy(name, tbuf1, BUFFER_LEN);
      }
      do_rawlog(LT_CHECK,
                " * Bad attribute name on #%d. Changing name to %s.\n",
                i, name);
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
    strcpy(tbuf1, atr_value(list));
    /* get rid of unprintables and hard newlines */
    for (p = tbuf1; *p; p++) {
      if (!isprint((unsigned char) *p) && !isspace((unsigned char) *p) &&
          *p != TAG_START && *p != TAG_END && *p != ESC_CHAR
          && *p != BEEP_CHAR) {
        *p = '!';
        err = 1;
      }
    }
    if (err) {
      fixmemdb = 1;
      do_rawlog(LT_CHECK,
                " * Bad text in attribute %s on #%d. Changed to:\n", name, i);
      do_rawlog(LT_CHECK, "%s\n", tbuf1);
    }
    db_write_labeled_string(f, "  value", tbuf1);
    if (flag && fixmemdb) {
      /* Fix the db in memory */
      privbits flags = AL_FLAGS(list);
      atr_clr(i, AL_NAME(list), owner);
      (void) atr_add(i, name, tbuf1, owner, flags);
      list = atr_get_noparent(i, name);
      AL_FLAGS(list) = flags;
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

  do_rawlog(LT_CHECK, "PARANOID WRITE BEGINNING...\n");

  penn_fprintf(f, "+V%d\n", dbflag * 256 + 2);

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
      do_rawlog(LT_ERR, "WARNING: Invalid lock key '%s' for lock #%d/%s!",
                key, i, type);
    else
      add_lock_raw(creator, i, type, b, flags);
  }

  if (found != count)
    do_rawlog(LT_ERR,
              "WARNING: Actual lock count (%d) different from expected count (%d).",
              found, count);

}


/** Read locks for an object.
 * This function is used for DBF_NEW_LOCKS to read a whole list
 * of locks from an object and set them. DBF_NEW_LOCKS aren't really
 * new any more, and get_new_locks() is probably being used instead of
 * this function.
 * \param i dbref of the object.
 * \param f file pointer to read from.
 */
void
getlocks(dbref i, PENNFILE *f)
{
  /* Assumes it begins at the beginning of a line. */
  int c;
  boolexp b;
  char buf[BUFFER_LEN], *p;
  while ((c = penn_fgetc(f)), c != EOF && c == '_') {
    p = buf;
    while ((c = penn_fgetc(f)), c != EOF && c != '|') {
      *p++ = c;
    }
    *p = '\0';
    if (c == EOF || (p - buf == 0)) {
      do_rawlog(LT_ERR, "ERROR: Invalid lock format on object #%d", i);
      return;
    }
    b = getboolexp(f, buf);     /* Which will clobber a '\n' */
    if (b == TRUE_BOOLEXP) {
      /* getboolexp() would already have complained. */
      return;
    } else {
      add_lock_raw(Owner(i), i, buf, b, LF_DEFAULT);
    }
  }
  penn_ungetc(c, f);
  return;
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

  List(i) = NULL;
  tbuf1[0] = '\0';
  while (1)
    switch (c = penn_fgetc(f)) {
    case ']':                  /* new style attribs, read name then value */
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
        do_rawlog(LT_ERR,
                  "ERROR: Bad format on new attribute %s. object #%d",
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
      if (strchr(tb2, TAG_START) || strchr(tb2, ESC_CHAR)) {
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
    case '>':                  /* old style attribs, die noisily */
      do_rawlog(LT_ERR, "ERROR: old-style attribute format in object %d", i);
      return -1;
      break;
    case '<':                  /* end of list */
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

  List(i) = NULL;

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
      if (strchr(value, ESC_CHAR) || strchr(value, TAG_START)) {
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
              "WARNING: Actual attribute count (%d) different than expected count (%d).",
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

  for (i = 0;; i++) {
    /* Loop invariant: we always begin at the beginning of a line. */
    errobj = i;
    c = penn_fgetc(f);
    switch (c) {
      /* make sure database is at least this big *1.5 */
    case '~':
      db_init = (getref(f) * 3) / 2;
      init_objdata_htab(db_init / 8, NULL);
      break;
      /* Use the MUSH 2.0 header stuff to see what's in this db */
    case '+':
      c = penn_fgetc(f);        /* Skip the V */
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
    case '&':                  /* zone oriented database */
      do_rawlog(LT_ERR, "ERROR: old style database.");
      return -1;
      break;
      /* new database */
    case '!':                  /* non-zone oriented database */
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
        char buff[BUFFER_LEN + 1];      /* The name plus a NUL */
        mush_strncpy(buff, o->name, PLAYER_NAME_LIMIT);
        set_name(i, buff);
        do_rawlog(LT_CHECK,
                  " * Name of #%d is longer than the maximum, truncating.\n",
                  i);
      } else if (!IsPlayer(i) && (strlen(o->name) > OBJECT_NAME_LIMIT)) {
        char buff[OBJECT_NAME_LIMIT + 1];       /* The name plus a NUL */
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
        /* If it has the MONITOR flag and the db predates HEAR_CONNECT, swap them over */
        if (!(globals.indb_flags & DBF_HEAR_CONNECT) &&
            has_flag_by_name(i, "MONITOR", NOTYPE)) {
          clear_flag_internal(i, "MONITOR");
          set_flag_internal(i, "HEAR_CONNECT");
        }
      }
      break;

    case '*':
      {
        char buff[80];
        penn_ungetc('*', f);
        penn_fgets(buff, sizeof buff, f);
        if (strcmp(buff, EOD) != 0) {
          do_rawlog(LT_ERR, "ERROR: No end of dump after object #%d", i - 1);
          return -1;
        } else {
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
  int c;
  dbref i = 0;
  char *tmp;
  struct object *o;
  int minimum_flags =
    DBF_NEW_STRINGS | DBF_TYPE_GARBAGE | DBF_SPLIT_IMMORTAL | DBF_NO_TEMPLE |
    DBF_SPIFFY_LOCKS;

  log_mem_check();

  loading_db = 1;

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

  db_read_this_labeled_string(f, "savedtime", &tmp);
  strcpy(db_timestamp, tmp);

  do_rawlog(LT_ERR, "Loading database saved on %s UTC", db_timestamp);

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
      } else {
        do_rawlog(LT_ERR, "Unrecognized database format!");
        return -1;
      }
      break;
    case '~':
      db_init = (getref(f) * 3) / 2;
      init_objdata_htab(db_init / 8, NULL);
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
          LBL_NAME, LBL_LOCATION, LBL_CONTENTS, LBL_EXITS,
          LBL_NEXT, LBL_PARENT, LBL_LOCKS, LBL_OWNER, LBL_ZONE,
          LBL_PENNIES, LBL_TYPE, LBL_FLAGS, LBL_POWERS, LBL_WARNINGS,
          LBL_CREATED, LBL_MODIFIED, LBL_ATTRS, LBL_ERROR
        };
        struct label_table {
          const char *label;
          enum known_labels tag;
        };
        struct label_table fields[] = {
          {"name", LBL_NAME},
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
          {NULL, LBL_ERROR}
        }, *entry;
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
          case LBL_ATTRS:
            {
              int attrcount = parse_integer(value);
              db_read_attrs(f, i, attrcount);
            }
            break;
          case LBL_ERROR:
          default:
            do_rawlog(LT_ERR, "Unrecognized field '%s' in object #%d",
                      label, i);
            return -1;
          }
        }
        if (IsPlayer(i) && (strlen(o->name) > (size_t) PLAYER_NAME_LIMIT)) {
          char buff[BUFFER_LEN + 1];    /* The name plus a NUL */
          strncpy(buff, o->name, PLAYER_NAME_LIMIT);
          buff[PLAYER_NAME_LIMIT] = '\0';
          set_name(i, buff);
          do_rawlog(LT_CHECK,
                    " * Name of #%d is longer than the maximum, truncating.\n",
                    i);
        } else if (!IsPlayer(i) && (strlen(o->name) > OBJECT_NAME_LIMIT)) {
          char buff[OBJECT_NAME_LIMIT + 1];     /* The name plus a NUL */
          strncpy(buff, o->name, OBJECT_NAME_LIMIT);
          buff[OBJECT_NAME_LIMIT] = '\0';
          set_name(i, buff);
          do_rawlog(LT_CHECK,
                    " * Name of #%d is longer than the maximum, truncating.\n",
                    i);
        }
        if (IsPlayer(i)) {
          add_player(i);
          clear_flag_internal(i, "CONNECTED");
          /* If it has the MONITOR flag and the db predates HEAR_CONNECT, swap them over */
          if (!(globals.indb_flags & DBF_HEAR_CONNECT) &&
              has_flag_by_name(i, "MONITOR", NOTYPE)) {
            clear_flag_internal(i, "MONITOR");
            set_flag_internal(i, "HEAR_CONNECT");
          }
        }
      }
      break;
    case '*':
      {
        char buff[80];
        penn_ungetc('*', f);
        penn_fgets(buff, sizeof buff, f);
        if (strcmp(buff, EOD) != 0) {
          do_rawlog(LT_ERR, "ERROR: No end of dump after object #%d", i - 1);
          return -1;
        } else {
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
  return -1;
}

static void
init_objdata_htab(int size, void (*free_data) (void *))
{
  if (size < 10)
    size = 10;
  hash_init(&htab_objdata, size, free_data);
  hashinit(&htab_objdata_keys, 8);
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
 * \param keybase base string for type of data.
 * \param data pointer to the data to store.
 * \return data passed in.
 */
void *
set_objdata(dbref thing, const char *keybase, void *data)
{
  char keyname[BUFFER_LEN];

  mush_strncpy(keyname, tprintf("%s_#%d", keybase, thing), BUFFER_LEN);
  hashdelete(keyname, &htab_objdata);
  if (data) {
    if (!hashadd(keyname, data, &htab_objdata))
      return NULL;
    if (hash_find(&htab_objdata_keys, keybase) == NULL) {
      char *newkey = mush_strdup(keyname, "objdata.key");
      hashadd(keybase, newkey, &htab_objdata_keys);
    }
  }
  return data;
}

/** Retrieve data from the object data hashtable.
 * \param thing dbref of object data is associated with.
 * \param keybase base string for type of data.
 * \return data stored for that object and keybase, or NULL.
 */
void *
get_objdata(dbref thing, const char *keybase)
{
  return hashfind(tprintf("%s_#%d", keybase, thing), &htab_objdata);
}

/** Clear all of an object's data from the object data hashtable.
 * This function clears any data associated with a given object
 * that's in the object data hashtable (under any keybase).
 * It's used before we free the object.
 * \param thing dbref of object data is associated with.
 */
void
clear_objdata(dbref thing)
{
  char *p;
  for (p = (char *) hash_firstentry(&htab_objdata_keys);
       p; p = (char *) hash_nextentry(&htab_objdata_keys)) {
    set_objdata(thing, p, NULL);
  }
}

/** Create a basic 3-object (Start Room, God, Master Room) database. */
void
create_minimal_db(void)
{
  dbref start_room, god, master_room;
  uint32_t desc_flags = AF_VISUAL | AF_NOPROG | AF_PREFIXMATCH | AF_PUBLIC;

  start_room = new_object();    /* #0 */
  god = new_object();           /* #1 */
  master_room = new_object();   /* #2 */

  init_objdata_htab(128, NULL);

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
              "This is the master room. Any exit in here is considered global. The same is true for objects with $-commands placed here.",
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
    do_rawlog(LT_ERR, "Unable to open %s in mode '%s': %s",
              filename, mode, strerror(errno));
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
    return fgetc(f->handle.f);
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
    return fgets(buf, len, pf->handle.f);
  case PFT_GZFILE:
#ifdef HAVE_LIBZ
    return gzgets(pf->handle.g, buf, len);
#endif
    break;
  }
  return NULL;
}

/* c should not be a negative value or it'll screw up gzputc return value testing */
int
penn_fputc(int c, PENNFILE *f)
{
  switch (f->type) {
  case PFT_FILE:
  case PFT_PIPE:
    OUTPUT(fputc(c, f->handle.f));
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
    OUTPUT(fputs(s, f->handle.f));
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
    /* No equivalent to vfprintf in zlib... */
#ifdef HAVE_VASPRINTF
    {                           /* Safe GNU/BSD way */
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
      va_end(ap, fmt);
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
