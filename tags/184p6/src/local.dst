/*----------------------------------------------- -*- c -*-
 * Local stuff
 *
 * This file contains custom stuff, and some of the items here are
 * called from within PennMUSH at specific times.
 */

/* Here are some includes you're likely to need or want.
 */
#include "copyrite.h"
#include "config.h"
#include <string.h>
#include "conf.h"
#include "dbio.h"
#include "externs.h"
#include "parse.h"
#include "htab.h"
#include "command.h"
#include "lock.h"
#include "game.h"
#include "confmagic.h"


extern HASHTAB htab_reserved_aliases;

/* Called after all MUSH init is done.
 */
void
local_startup(void)
{

/* Register local_timer to be called once a second. You can also
* register other callbacks to run at other intervals. See local_timer()
* below for an example of what the callback function needs to do if it
* should be run more than once. 
*
* Arguments are: Number of seconds from now to run, the callback function,
* a data argument to pass to it, and a softcoded event name to run at the same
* time. The latter two can be null pointers. The callback function returns true
* if the softcode event should be triggered, false if it shouldn't.
*/
#if 0                           /* Change to 1 if you need local_timer functionality. */
  sq_register_loop(1, local_timer, NULL, NULL);
#endif
}

/* Add you own runtime configuration options here, and you can set
 * them in mush.cnf.
 */
void
local_configs(void)
{
#ifdef EXAMPLE
  /* For each config parameter you add, you should initialize it as a
   * static variable here (or a global variable elsewhere in your
   * code)
   */
  static int config_example = 1;
  static char config_string[BUFFER_LEN];
#endif

  /* Initial size of this hashtable should be close to the number of
   * add_config()'s you plan to do.
   */
  hashinit(&local_options, 4);

#ifdef EXAMPLE
  /* Call add_config for each config parameter you want to add.
   * Note the use of &config_example for simple types (bool, int),
   * but just config_string for strings.
   */
  add_config("use_example", cf_bool, &config_example, sizeof config_example,
             "cosmetic");
  add_config("some_string", cf_str, config_string, sizeof config_string,
             "cosmetic");
#endif
}

/* Add any custom @locks in here. The valid flags can be found in lock.h */
void
local_locks(void)
{
#ifdef EXAMPLE
  define_lock("AdminOnlyLock", LF_PRIVATE | LF_WIZARD);
  define_lock("NormalLock", LF_PRIVATE);
#endif
}


/* Called when the database will be saved
 * This is called JUST before we dump the
 * database to disk
 * Use to save any in-memory structures
 * back to disk
 */
void
local_dump_database(void)
{
}

/* Called when the MUSH is shutting down.
 * The DB has been saved and descriptors closed
 * The log files are still open though.
 */
void
local_shutdown(void)
{
}

/* Called when the MUSH is performing a dbck database check,
 * at the end of the check. A good place to add any regular
 * consistency checking you require.
 */
void
local_dbck(void)
{
}

/* This is called exactly once a second
 * After the MUSH has done all it's stuff
 */
bool
local_timer(void *data __attribute__ ((__unused__)))
{

  /* The callback has to be set back up or it'll only run once. */
  return false;
}

/* Called when a player connects. If this is a new creation,
 * isnew will be true. num gives the number of connections by
 * that player (so if num > 1, this is a multiple connect).
 */
void
local_connect(dbref player __attribute__ ((__unused__)),
              int isnew __attribute__ ((__unused__)),
              int num __attribute__ ((__unused__)))
{
}

/* Called when a player disconnects. If num > 1, this is
 * a partial disconnect.
 */
void
local_disconnect(dbref player __attribute__ ((__unused__)),
                 int num __attribute__ ((__unused__)))
{
}


/* For serious hackers only */

/* Those who are depraved enough to do so (Like me), can always 
 * abuse this as a new and better way of Always Doing Stuff
 * to objects.
 * Like, say you want to put out a message on the wizard
 * channel every time an object is destroyed, do so in the
 * local_data_destroy() routine.
 */

/* Called when a object is created with @create (or @dig, @link) 
 * This is done AFTER object-specific setup, so the types
 * etc will already be set, and object-specific initialization
 * will be done.
 * Note that the game will ALWAYS set the LocData to NULL before
 * this routine is called.
 */

/* For a well-commented example of how to use this code,
 * see: ftp://bimbo.hive.no/pub/PennMUSH/coins.tar.gz
 */

void
local_data_create(dbref object __attribute__ ((__unused__)))
{
}

/* Called when an object is cloned. Since clone is a rather
 * specific form of creation, it has it's own function.
 * Note that local_data_create() is NOT called for this object
 * first, but the system will always set LocData to NULL first.
 * Clone is the 'new' object, while source is the one it's
 * being copied from.
 */

void
local_data_clone(dbref clone __attribute__ ((__unused__)),
                 dbref source __attribute__ ((__unused__)))
{
}

/* Called when a object is REALLY destroyed, not just set
 * Going.
 */

void
local_data_free(dbref object __attribute__ ((__unused__)))
{
}

/* Initiation of objects after a reload or dumping to disk should
 * be handled in local_dump_database() and local_startup().
 */


/* This function is called *before* most standard interaction checks,
 * and can override them. You probably want to do as little as possible
 * here and do most of the work in local_can_interact_last instead.
 * If this returns NOTHING, it means 'go on to more checks'
 */
int
local_can_interact_first(dbref from __attribute__ ((__unused__)),
                         dbref to __attribute__ ((__unused__)), int type
                         __attribute__ ((__unused__)))
{

  return NOTHING;
}

/* This one is called *after* most standard interaction checks. */
int
local_can_interact_last(dbref from __attribute__ ((__unused__)),
                        dbref to __attribute__ ((__unused__)), int type
                        __attribute__ ((__unused__)))
{
  /* from is where the message is coming from, in theory. It makes sense
   * for sound, but think of it as light rays for visiblity or matching. 
   * The rays come *from* someone standing in a room, and go *to* the person
   * looking around.
   */

#ifdef NEVER
  /* Various examples follow */

  switch (type) {
  case INTERACT_SEE:
    /* Someone standing in a room, or doing
     * @verb type stuff that's @bar, @obar, and @abar
     */

    /* Probably a good idea */
    if (See_All(to))
      return 1;

    break;

  case INTERACT_PRESENCE:
    /* Someone arriving or leaving, connecting or disconnecting, 
     * and (for objects) growing or losing ears.
     */

    /* To prevent spying, always notice presence */
    return 1;

    break;

  case INTERACT_HEAR:
    /* People talking */

    /* Telepathy example. Players who can hear telepathy get @set
     * HEAR_TELEPATHY,  players currently using telepathy should be
     * @set USE_TELEPATHY. */

    if (has_flag_by_name(from, "USE_TELEPATHY", NOTYPE))
      return has_flag_by_name(to, "HEAR_TELEPATHY", NOTYPE);

    break;

  case INTERACT_MATCH:
    /* Matching object names so you can pick them up, go through exits,
       etc. */

    break;
  }

  /* Splits the universe in half, half FOO and half not. */
  return (has_flag_by_name(to, "FOO", NOTYPE) ==
          has_flag_by_name(from, "FOO", NOTYPE));


#endif                          /* NEVER */

  /* You want to return NOTHING if you haven't made up your mind */
  return NOTHING;

}
