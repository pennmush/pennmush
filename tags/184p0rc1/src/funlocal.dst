/*-----------------------------------------------------------------
 * Local functions
 *
 * This file is reserved for local functions that you may wish
 * to hack into PennMUSH. Read parse.h for information on adding
 * functions. This file will not be overwritten when you update
 * to a new distribution, so it's preferable to add new functions
 * here and leave the other fun*.c files alone.
 *
 */

/* Here are some includes you're likely to need or want.
 * If your functions are doing math, include <math.h>, too.
 */
#include "copyrite.h"
#include "config.h"
#include <string.h>
#include "conf.h"
#include "externs.h"
#include "parse.h"
#include "confmagic.h"
#include "function.h"

void local_functions(void);

/* Here you can use the new add_function instead of hacking into function.c
 * Example included :)
 */

#ifdef EXAMPLE
FUNCTION(local_fun_silly)
{
  safe_format(buff, bp, "Silly%sSilly", args[0]);
}

#endif

void
local_functions(void)
{
#ifdef EXAMPLE
  function_add("SILLY", local_fun_silly, 1, 1, FN_REG);
#endif
}
