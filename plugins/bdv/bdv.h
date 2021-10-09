/* bdv.h */

#ifndef _BDV_H_
#define _BDV_H_

/* From combat.c */
extern int check_speed(dbref executor, const char *name);
extern int check_stunmax(dbref executor, const char *name);
extern dbref lookup_race(const char *name);

/* ------------------------------------------------------------------------ */
#define Duke(x)			(has_flag_by_name(x, "DUKE", TYPE_PLAYER))
#define Compin(x)		(has_flag_by_name(x, "COMPIN", TYPE_THING))
#define Weapon(x)		(has_flag_by_name(x, "WEAPON", TYPE_THING))
#define Isic(x)			(has_flag_by_name(x, "IC", NOTYPE))
#define NPC(x)			(has_flag_by_name(x, "NPC", NOTYPE))

#define Airlock(x)		(has_flag_by_name(x, "AIRLOCK", TYPE_EXIT))
#define Pad(x)			(has_flag_by_name(x, "PAD", TYPE_EXIT))
#define Port(x)			(has_flag_by_name(x, "PORT", TYPE_EXIT))
#define Hatch(x)		(has_flag_by_name(x, "HATCH", TYPE_EXIT))
/* ------------------------------------------------------------------------ */ 

#ifdef WIN32
#define snprintf _snprintf
#endif

/* ------------------------------------------------------------------------ */

#endif    /* _BDV_H_ */
