/**
 * \file flag_tab.h
 *
 * \brief Table of default flags and powers
 */

#pragma once

/** This is the old default flag table. We still use it when we have to
 * convert old dbs, but once you have a converted db, it's the flag
 * table in the db that counts, not this one.
 * DO NOT ADD NEW FLAGS HERE. Any new flags added should be done via
 * flag_add_additional() further down in this file.
 */
/* Name     Letter   Type(s)   Flag   Perms   Negate_Perm */
static const FLAG flag_table[] = {
  {"CHOWN_OK", 'C', NOTYPE, CHOWN_OK, F_ANY, F_ANY},
  {"DARK", 'D', NOTYPE, DARK, F_ANY, F_ANY},
  {"GOING", 'G', NOTYPE, GOING, F_INTERNAL, F_INTERNAL},
  {"HAVEN", 'H', TYPE_PLAYER, HAVEN, F_ANY, F_ANY},
  {"TRUST", 'I', NOTYPE, INHERIT, F_INHERIT, F_INHERIT},
  {"LINK_OK", 'L', NOTYPE, LINK_OK, F_ANY, F_ANY},
  {"OPAQUE", 'O', NOTYPE, LOOK_OPAQUE, F_ANY, F_ANY},
  {"QUIET", 'Q', NOTYPE, QUIET, F_ANY, F_ANY},
  {"STICKY", 'S', NOTYPE, STICKY, F_ANY, F_ANY},
  {"UNFINDABLE", 'U', NOTYPE, UNFIND, F_ANY, F_ANY},
  {"VISUAL", 'V', NOTYPE, VISUAL, F_ANY, F_ANY},
  {"WIZARD", 'W', NOTYPE, WIZARD, F_INHERIT | F_WIZARD | F_LOG,
   F_INHERIT | F_WIZARD},
  {"SAFE", 'X', NOTYPE, SAFE, F_ANY, F_ANY},
  {"AUDIBLE", 'a', NOTYPE, AUDIBLE, F_ANY, F_ANY},
  {"DEBUG", 'b', NOTYPE, DEBUGGING, F_ANY, F_ANY},
  {"NO_WARN", 'w', NOTYPE, NOWARN, F_ANY, F_ANY},
  {"ENTER_OK", 'e', NOTYPE, ENTER_OK, F_ANY, F_ANY},
  {"HALT", 'h', NOTYPE, HALT, F_ANY, F_ANY},
  {"NO_COMMAND", 'n', NOTYPE, NO_COMMAND, F_ANY, F_ANY},
  {"LIGHT", 'l', NOTYPE, LIGHT, F_ANY, F_ANY},
  {"ROYALTY", 'r', NOTYPE, ROYALTY, F_INHERIT | F_ROYAL | F_LOG,
   F_INHERIT | F_ROYAL},
  {"TRANSPARENT", 't', NOTYPE, TRANSPARENTED, F_ANY, F_ANY},
  {"VERBOSE", 'v', NOTYPE, VERBOSE, F_ANY, F_ANY},
  {"ANSI", 'A', TYPE_PLAYER, PLAYER_ANSI, F_ANY, F_ANY},
  {"COLOR", 'C', TYPE_PLAYER, PLAYER_COLOR, F_ANY, F_ANY},
  {"MONITOR", 'M', TYPE_PLAYER | TYPE_ROOM | TYPE_THING, 0, F_ANY, F_ANY},
  {"NOSPOOF", '"', TYPE_PLAYER, PLAYER_NOSPOOF, F_ANY | F_ODARK,
   F_ANY | F_ODARK},
  {"SHARED", 'Z', TYPE_PLAYER, PLAYER_ZONE, F_ANY, F_ANY},
  {"TRACK_MONEY", '\0', TYPE_PLAYER, 0, F_ANY, F_ANY},
  {"CONNECTED", 'c', TYPE_PLAYER, PLAYER_CONNECT, F_INTERNAL, F_INTERNAL},
  {"GAGGED", 'g', TYPE_PLAYER, PLAYER_GAGGED, F_WIZARD, F_WIZARD},
  {"MYOPIC", 'm', TYPE_PLAYER, PLAYER_MYOPIC, F_ANY, F_ANY},
  {"TERSE", 'x', TYPE_PLAYER | TYPE_THING, PLAYER_TERSE, F_ANY, F_ANY},
  {"JURY_OK", 'j', TYPE_PLAYER, PLAYER_JURY, F_ROYAL, F_ROYAL},
  {"JUDGE", 'J', TYPE_PLAYER, PLAYER_JUDGE, F_ROYAL, F_ROYAL},
  {"FIXED", 'F', TYPE_PLAYER, PLAYER_FIXED, F_WIZARD, F_WIZARD},
  {"UNREGISTERED", '?', TYPE_PLAYER, PLAYER_UNREG, F_ROYAL, F_ROYAL},
  {"ON-VACATION", 'o', TYPE_PLAYER, PLAYER_VACATION, F_ANY, F_ANY},
  {"SUSPECT", 's', TYPE_PLAYER, PLAYER_SUSPECT, F_WIZARD | F_MDARK | F_LOG,
   F_WIZARD | F_MDARK},
  {"PARANOID", '\0', TYPE_PLAYER, PLAYER_PARANOID, F_ANY | F_ODARK,
   F_ANY | F_ODARK},
  {"NOACCENTS", '~', TYPE_PLAYER, PLAYER_NOACCENTS, F_ANY, F_ANY},
  {"DESTROY_OK", 'd', TYPE_THING, THING_DEST_OK, F_ANY, F_ANY},
  {"PUPPET", 'p', TYPE_THING, THING_PUPPET, F_ANY, F_ANY},
  {"NO_LEAVE", 'N', TYPE_THING, THING_NOLEAVE, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_THING | TYPE_ROOM, 0, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_THING | TYPE_ROOM, 0, F_ANY, F_ANY},
  {"ABODE", 'A', TYPE_ROOM, ROOM_ABODE, F_ANY, F_ANY},
  {"FLOATING", 'F', TYPE_ROOM, ROOM_FLOATING, F_ANY, F_ANY},
  {"JUMP_OK", 'J', TYPE_ROOM, ROOM_JUMP_OK, F_ANY, F_ANY},
  {"NO_TEL", 'N', TYPE_ROOM, ROOM_NO_TEL, F_ANY, F_ANY},
  {"UNINSPECTED", 'u', TYPE_ROOM, ROOM_UNINSPECT, F_ROYAL, F_ROYAL},
  {"CLOUDY", 'x', TYPE_EXIT, EXIT_CLOUDY, F_ANY, F_ANY},
  {"GOING_TWICE", '\0', NOTYPE, GOING_TWICE, F_INTERNAL | F_DARK,
   F_INTERNAL | F_DARK},
  {"KEEPALIVE", 'k', TYPE_PLAYER, 0, F_ANY, F_ANY},
  {"NO_LOG", '\0', NOTYPE, 0, F_WIZARD | F_MDARK | F_LOG, F_WIZARD | F_MDARK},
  {"OPEN_OK", '\0', TYPE_ROOM, 0, F_ANY, F_ANY},
  {NULL, '\0', 0, 0, 0, 0}};

/** The old table to kludge multi-type toggles. Now used only
 * for conversion.
 */
static const FLAG hack_table[] = {
  {"MONITOR", 'M', TYPE_PLAYER, PLAYER_MONITOR, F_ROYAL, F_ROYAL},
  {"MONITOR", 'M', TYPE_THING, THING_LISTEN, F_ANY, F_ANY},
  {"MONITOR", 'M', TYPE_ROOM, ROOM_LISTEN, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_THING, THING_INHEARIT, F_ANY, F_ANY},
  {"LISTEN_PARENT", '^', TYPE_ROOM, ROOM_INHEARIT, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_THING, THING_Z_TEL, F_ANY, F_ANY},
  {"Z_TEL", 'Z', TYPE_ROOM, ROOM_Z_TEL, F_ANY, F_ANY},
  {NULL, '\0', 0, 0, 0, 0}};

/** A table of types, as if they were flags. Some functions that
 * expect flags also accept, for historical reasons, types.
 */
static FLAG type_table[] = {
  {"PLAYER", 'P', TYPE_PLAYER, TYPE_PLAYER, F_INTERNAL, F_INTERNAL},
  {"ROOM", 'R', TYPE_ROOM, TYPE_ROOM, F_INTERNAL, F_INTERNAL},
  {"EXIT", 'E', TYPE_EXIT, TYPE_EXIT, F_INTERNAL, F_INTERNAL},
  {"THING", 'T', TYPE_THING, TYPE_THING, F_INTERNAL, F_INTERNAL},
  {NULL, '\0', 0, 0, 0, 0}};

/** A table of types, as privileges. */
static const PRIV type_privs[] = {{"PLAYER", 'P', TYPE_PLAYER, TYPE_PLAYER},
                                  {"ROOM", 'R', TYPE_ROOM, TYPE_ROOM},
                                  {"EXIT", 'E', TYPE_EXIT, TYPE_EXIT},
                                  {"THING", 'T', TYPE_THING, TYPE_THING},
                                  {NULL, '\0', 0, 0}};

/** The old default aliases for flags. This table is only used in conversion
 * of old databases. Once a database is converted, the alias list in the
 * database is what counts.
 */
static const FLAG_ALIAS flag_alias_tab[] = {{"INHERIT", "TRUST"},
                                            {"TRACE", "DEBUG"},
                                            {"NOWARN", "NO_WARN"},
                                            {"NOCOMMAND", "NO_COMMAND"},
                                            {"LISTENER", "MONITOR"},
                                            {"WATCHER", "MONITOR"},
                                            {"ZONE", "SHARED"},
                                            {"COLOUR", "COLOR"},
                                            {"JURYOK", "JURY_OK"},
                                            {"VACATION", "ON-VACATION"},
                                            {"DEST_OK", "DESTROY_OK"},
                                            {"NOLEAVE", "NO_LEAVE"},
                                            {"TEL_OK", "JUMP_OK"},
                                            {"TELOK", "JUMP_OK"},
                                            {"TEL-OK", "JUMP_OK"},
                                            {"^", "LISTEN_PARENT"},

                                            {NULL, NULL}};

/** This is the old defaultpowr table. We still use it when we
 * have to convert old dbs, but once you have a converted db,
 * it's the power table in the db that counts, not this one.
 */
/*   Name      Flag   */
static const FLAG power_table[] = {
  {"Announce", '\0', NOTYPE, CAN_WALL, F_WIZARD | F_LOG, F_WIZARD},
  {"Boot", '\0', NOTYPE, CAN_BOOT, F_WIZARD | F_LOG, F_WIZARD},
  {"Builder", '\0', NOTYPE, CAN_BUILD, F_WIZARD | F_LOG, F_WIZARD},
  // {"Cemit", '\0', NOTYPE, CEMIT, F_WIZARD | F_LOG, F_WIZARD},
  {"Chat_Privs", '\0', NOTYPE, CHAT_PRIVS, F_WIZARD | F_LOG, F_WIZARD},
  {"Functions", '\0', NOTYPE, GLOBAL_FUNCS, F_WIZARD | F_LOG, F_WIZARD},
  {"Guest", '\0', NOTYPE, IS_GUEST, F_WIZARD | F_LOG, F_WIZARD},
  {"Halt", '\0', NOTYPE, HALT_ANYTHING, F_WIZARD | F_LOG, F_WIZARD},
  {"Hide", '\0', NOTYPE, CAN_HIDE, F_WIZARD | F_LOG, F_WIZARD},
  {"Idle", '\0', NOTYPE, UNLIMITED_IDLE, F_WIZARD | F_LOG, F_WIZARD},
  {"Immortal", '\0', NOTYPE, NO_PAY | NO_QUOTA, F_WIZARD, F_WIZARD},
  {"Link_Anywhere", '\0', NOTYPE, LINK_ANYWHERE, F_WIZARD | F_LOG, F_WIZARD},
  {"Login", '\0', NOTYPE, LOGIN_ANYTIME, F_WIZARD | F_LOG, F_WIZARD},
  {"Long_Fingers", '\0', NOTYPE, LONG_FINGERS, F_WIZARD | F_LOG, F_WIZARD},
  {"No_Pay", '\0', NOTYPE, NO_PAY, F_WIZARD | F_LOG, F_WIZARD},
  {"No_Quota", '\0', NOTYPE, NO_QUOTA, F_WIZARD | F_LOG, F_WIZARD},
  {"Open_Anywhere", '\0', NOTYPE, OPEN_ANYWHERE, F_WIZARD | F_LOG, F_WIZARD},
  {"Pemit_All", '\0', NOTYPE, PEMIT_ALL, F_WIZARD | F_LOG, F_WIZARD},
  {"Player_Create", '\0', NOTYPE, CREATE_PLAYER, F_WIZARD | F_LOG, F_WIZARD},
  {"Poll", '\0', NOTYPE, SET_POLL, F_WIZARD | F_LOG, F_WIZARD},
  {"Queue", '\0', NOTYPE, HUGE_QUEUE, F_WIZARD | F_LOG, F_WIZARD},
  {"Quotas", '\0', NOTYPE, CHANGE_QUOTAS, F_WIZARD | F_LOG, F_WIZARD},
  {"Search", '\0', NOTYPE, SEARCH_EVERYTHING, F_WIZARD | F_LOG, F_WIZARD},
  {"See_All", '\0', NOTYPE, SEE_ALL, F_WIZARD | F_LOG, F_WIZARD},
  {"See_Queue", '\0', NOTYPE, PS_ALL, F_WIZARD | F_LOG, F_WIZARD},
  {"Tport_Anything", '\0', NOTYPE, TEL_OTHER, F_WIZARD | F_LOG, F_WIZARD},
  {"Tport_Anywhere", '\0', NOTYPE, TEL_ANYWHERE, F_WIZARD | F_LOG, F_WIZARD},
  {"Can_spoof", '\0', NOTYPE, CAN_NSPEMIT, F_WIZARD | F_LOG, F_WIZARD},
  {NULL, '\0', 0, 0, 0, 0}};

/** A table of aliases for powers. */
static const FLAG_ALIAS power_alias_tab[] = {
  //{"@cemit", "Cemit"},
  {"@wall", "Announce"},
  {"wall", "Announce"},
  {"Can_nspemit", "Can_spoof"},
  {NULL, NULL}};

/** The table of flag privilege bits. */
static const PRIV flag_privs[] = {{"trusted", '\0', F_INHERIT, F_INHERIT},
                                  {"owned", '\0', F_OWNED, F_OWNED},
                                  {"royalty", '\0', F_ROYAL, F_ROYAL},
                                  {"wizard", '\0', F_WIZARD, F_WIZARD},
                                  {"god", '\0', F_GOD, F_GOD},
                                  {"internal", '\0', F_INTERNAL, F_INTERNAL},
                                  {"dark", '\0', F_DARK, F_DARK},
                                  {"mdark", '\0', F_MDARK, F_MDARK},
                                  {"odark", '\0', F_ODARK, F_ODARK},
                                  {"disabled", '\0', F_DISABLED, F_DISABLED},
                                  {"log", '\0', F_LOG, F_LOG},
                                  {"event", '\0', F_EVENT, F_EVENT},
                                  {NULL, '\0', 0, 0}};
