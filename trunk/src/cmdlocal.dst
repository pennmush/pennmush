/* -*- c -*-
/*-----------------------------------------------------------------
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
#include "externs.h"
#include "parse.h"
#include "htab.h"
#include "flags.h"
#include "command.h"
#include "cmds.h"
#include "confmagic.h"

extern HASHTAB htab_reserved_aliases;

/* Called during the command init sequence before any commands are
 * added (including local_commands, below). This is where you
 * want to reserve any strings that you *don't* want any command
 * to alias to (because you want to preserve it for matching exits
 * or globals)
 */
void
reserve_aliases(void)
{
#ifdef EXAMPLE
  /* Example: Don't alias any commands to cardinal directions.
   * Remove the #ifdef EXAMPLE and #endif to use this code
   */
  reserve_alias("W");
  reserve_alias("E");
  reserve_alias("S");
  reserve_alias("N");
  reserve_alias("NW");
  reserve_alias("NE");
  reserve_alias("SW");
  reserve_alias("SE");
  reserve_alias("U");
  reserve_alias("D");
#endif
}

#ifdef EXAMPLE
COMMAND(cmd_local_silly)
{
  if (SW_ISSET(sw, SWITCH_NOISY))
    notify_format(executor, "Noisy silly with %s", arg_left);
  if (SW_BY_NAME(sw, "VERY"))
    notify(executor, "The following line will be very silly indeed.");
  notify_format(executor, "SillyCommand %s", arg_left);
}
#endif


/* Called during the command init sequence.
 * This is where you'd put calls to command_add to insert a local
 * command into the command hash table. Any command you add here
 * will be auto-aliased for you.
 * The way to call command_add is illustrated below. The arguments are:
 *   Name of the command, a string ("@SILLY")
 *   Command parsing modifiers, a bitmask (see hdrs/command.h)
 *   Flags to restrict command to, a string ("WIZARD ROYALTY") or NULL
 *     (Someone with *any* one of these flags can use the command)
 *   Powers to restrict command to, a string ("SEE_ALL") or NULL
 *     (Someone with this power can use the command)
 *   Switches the command can take, a string or NULL ("NOISY NOEVAL")
 *   Hardcoded function the command should call (cmd_local_silly)
 */
void
local_commands(void)
{
#ifdef EXAMPLE
  command_add("@SILLY", CMD_T_ANY, "WIZARD ROYALTY", "SEE_ALL",
              "NOISY NOEVAL VERY", cmd_local_silly);
#endif
}
