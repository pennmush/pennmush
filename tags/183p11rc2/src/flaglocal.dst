/*-----------------------------------------------------------------
 * Local flags
 *
 * This file is reserved for local flags that you may wish
 * to hack into PennMUSH.
 * This file will not be overwritten when you update
 * to a new distribution, so it's preferable to add new flags
 * here and leave flag.c alone.
 *
 * YOU ARE RESPONSIBLE FOR SEEING THAT YOU DO THIS RIGHT - 
 * It's probably smarter to have God @flag/add rather than
 * do it here, as @flag/add does extensive checks for safety,
 * and add_flag() doesn't! Remember that flags are saved in the
 * database, so flags added here won't overwrite flags that
 * are already defined. When in doubt, use @flag.
 *
 * It is explicitly safe to try to add_flag() an existing flag.
 * It won't do anything, but it won't be harmful.
 *
 */

/* Here are some includes you're likely to need or want.
 */
#include "copyrite.h"
#include "config.h"
#include <string.h>
#include "conf.h"
#include "externs.h"
#include "flags.h"
#include "confmagic.h"

void
local_flags(FLAGSPACE *flags __attribute__ ((__unused__)))
{
#ifdef EXAMPLE
  if (strcmp(flags->name, "FLAG") == 0) {
    add_flag("BIG", 'B', TYPE_PLAYER | TYPE_THING, F_ANY, F_ANY);
  } else if (strcmp(flags->name, "POWER") == 0) {
    add_power("MORPH", '\0', NOTYPE, F_WIZARD | F_LOG, F_ANY);
  }
#endif
}
