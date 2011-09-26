/**
 * \file privtab.h
 *
 * \brief Defines a privilege table entry for general use
 */


#ifndef __PRIVTAB_H
#define __PRIVTAB_H

#include "copyrite.h"
#include "config.h"
#include "confmagic.h"

typedef struct priv_info PRIV;

/** Privileges.
 * This structure represents a privilege and its associated data.
 * Privileges tables are used to provide a unified way to parse
 * a string of restrictions into a bitmask.
 */
struct priv_info {
  const char *name;     /**< Name of the privilege */
  char letter;          /**< One-letter abbreviation */
  privbits bits_to_set; /**< Bitflags required to set this privilege */
  privbits bits_to_show;        /**< Bitflags required to see this privilege */
};

#define PrivName(x)     ((x)->name) /**< Full name of priv */
#define PrivChar(x)     ((x)->letter) /**< One-char abbreviation of priv */
#define PrivSetBits(x)  ((x)->bits_to_set) /**< Bitflags required to set priv */
#define PrivShowBits(x) ((x)->bits_to_show) /**< Bitflags required to see priv */

privbits string_to_privs(PRIV *table, const char *str, privbits origprivs);
privbits list_to_privs(PRIV *table, const char *str, privbits origprivs);
int string_to_privsets(PRIV *table, const char *str, privbits *setprivs,
                       privbits *clrprivs);
privbits letter_to_privs(PRIV *table, const char *str, privbits origprivs);
extern const char *privs_to_string(PRIV *table, privbits privs);
extern const char *privs_to_letters(PRIV *table, privbits privs);

#endif                          /* __PRIVTAB_H */
