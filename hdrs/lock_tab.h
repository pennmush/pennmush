/**
 * \file lock_tab.h
 *
 * \brief Default lock permissions
 */

#pragma once

/** Table of lock names and permissions */
lock_list lock_types[] = {
  {"Basic", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Enter", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Use", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Zone", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Page", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Teleport", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Speech", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Listen", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Command", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Parent", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Link", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Leave", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Drop", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Give", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"From", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Pay", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Receive", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Mail", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Follow", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Examine", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Chzone", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Forward", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Control", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Dropto", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Destroy", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {"Interact", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"MailForward", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Take", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Open", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Filter", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"InFilter", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"DropIn", TRUE_BOOLEXP, GOD, LF_PRIVATE, NULL},
  {"Chown", TRUE_BOOLEXP, GOD, LF_PRIVATE | LF_OWNER, NULL},
  {NULL, TRUE_BOOLEXP, GOD, 0, NULL}};

/** Table of lock permissions */
PRIV lock_privs[] = {{"visual", 'v', LF_VISUAL, LF_VISUAL},
                     {"no_inherit", 'i', LF_PRIVATE, LF_PRIVATE},
                     {"no_clone", 'c', LF_NOCLONE, LF_NOCLONE},
                     {"wizard", 'w', LF_WIZARD, LF_WIZARD},
                     /*  {"owner", 'o', LF_OWNER, LF_OWNER}, */
                     {"locked", '+', LF_LOCKED, LF_LOCKED},
                     {NULL, '\0', 0, 0}};
