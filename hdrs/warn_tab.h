/**
 * \file warn_tab.h
 *
 * \brief Tables for warning bit conversions
 */

#pragma once

#define W_UNLOCKED 0x1 /**< Check for unlocked-object warnings */
#define W_LOCKED 0x2   /**< Check for locked-object warnings */

#define W_EXIT_ONEWAY 0x1    /**< Find one-way exits */
#define W_EXIT_MULTIPLE 0x2  /**< Find multiple exits to same place */
#define W_EXIT_MSGS 0x4      /**< Find exits without messages */
#define W_EXIT_DESC 0x8      /**< Find exits without descs */
#define W_EXIT_UNLINKED 0x10 /**< Find unlinked exits */
/* Space for more exit stuff */
#define W_THING_MSGS 0x100 /**< Find things without messages */
#define W_THING_DESC 0x200 /**< Find things without descs */
/* Space for more thing stuff */
#define W_ROOM_DESC 0x1000 /**< Find rooms without descs */
/* Space for more room stuff */
#define W_PLAYER_DESC 0x10000 /**< Find players without descs */

#define W_LOCK_PROBS 0x100000 /**< Find bad locks */

/* Groups of warnings */
#define W_NONE 0 /**< No warnings */
/** Serious warnings only */
#define W_SERIOUS                                                              \
  (W_EXIT_UNLINKED | W_THING_DESC | W_ROOM_DESC | W_PLAYER_DESC | W_LOCK_PROBS)
/** Standard warnings: serious warnings plus others */
#define W_NORMAL (W_SERIOUS | W_EXIT_ONEWAY | W_EXIT_MULTIPLE | W_EXIT_MSGS)
/** Extra warnings: standard warnings plus others */
#define W_EXTRA (W_NORMAL | W_THING_MSGS)
/** All warnings */
#define W_ALL (W_EXTRA | W_EXIT_DESC)

/** A structure representing a topology warning check. */
typedef struct a_tcheck {
  const char *name; /**< Name of warning. */
  warn_type flag;   /**< Bitmask of warning. */
} tcheck;

static tcheck checklist[] = {{"none", W_NONE}, /* MUST BE FIRST! */
                             {"exit-unlinked", W_EXIT_UNLINKED},
                             {"thing-desc", W_THING_DESC},
                             {"room-desc", W_ROOM_DESC},
                             {"my-desc", W_PLAYER_DESC},
                             {"exit-oneway", W_EXIT_ONEWAY},
                             {"exit-multiple", W_EXIT_MULTIPLE},
                             {"exit-msgs", W_EXIT_MSGS},
                             {"thing-msgs", W_THING_MSGS},
                             {"exit-desc", W_EXIT_DESC},
                             {"lock-checks", W_LOCK_PROBS},

                             /* These should stay at the end */
                             {"serious", W_SERIOUS},
                             {"normal", W_NORMAL},
                             {"extra", W_EXTRA},
                             {"all", W_ALL},
                             {NULL, 0}};
