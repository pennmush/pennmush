/* bdv_funcs.c */
#include "config.h"

#include <ctype.h>
#include <string.h>
#include <math.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "copyrite.h"
#include "ansi.h"
#include "conf.h"
#include "externs.h"
#include "intrface.h"
#include "parse.h"
#include "game.h"
#include "match.h"
#include "confmagic.h"
#include "dbdefs.h"
#include "function.h"
#include "log.h"
#include "flags.h"
#include "attrib.h"
#include "mushdb.h"
#include "lock.h"
#include "bdv.h"
#include "extmail.h"

struct pennmush_flag_info_bdv
{
   const char* name;
   char letter;
   int type;
   int perms;
   int negate_perms;
   const char* description;
};

struct pennmush_flag_info_bdv bdv_flag_table[] = 
{
   {"DUKE", '\0', TYPE_PLAYER, F_WIZARD, F_WIZARD, "Duke Flag"},
   {"IC", '\0', NOTYPE, F_ROYAL, F_ROYAL, "IC Flag"},
   {"NPC", '\0', NOTYPE, F_ROYAL, F_ROYAL, "NPC Flag"},
   {"AIRLOCK", '$', NOTYPE, F_INHERIT | F_ROYAL, F_INHERIT | F_ROYAL, "Airlock Flag"},
   {"SECURITY", '\0', TYPE_ROOM, F_INHERIT | F_ROYAL, F_INHERIT | F_ROYAL, "Security Flag"},
   {"BRIG", '\0', TYPE_ROOM, F_INHERIT | F_ROYAL, F_INHERIT | F_ROYAL, "Brig Flag"},
   {"WEAPON", '\0', TYPE_THING, F_WIZARD, F_WIZARD, "Weapon Flag"},
   {"COMPIN", '#', TYPE_THING, F_INHERIT | F_WIZARD, F_WIZARD, "Communicator Flag"},
   {"COMBAT", '*', NOTYPE, F_ROYAL, F_ROYAL, "Combat Flag"},
   {"MAGAZINE", 'f', TYPE_PLAYER, F_WIZARD, F_WIZARD, "Magazine Flag"},
   {NULL,'0',0,0,0}
};

/* ------------------------------------------------------------------------------- */

void setupBDVFlags()
{
   struct pennmush_flag_info_bdv* pFlagInfo;
   
   for (pFlagInfo = bdv_flag_table; pFlagInfo->name; pFlagInfo++)
     {
	
	add_flag(pFlagInfo->name, pFlagInfo->letter, pFlagInfo->type, 
		 pFlagInfo->perms, pFlagInfo->negate_perms);
     }
}

int plugin_init() {
  setupBDVFlags();
  return 1;
}
