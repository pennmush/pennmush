/**
 * \file help.h
 *
 * \brief Header file for the PennMUSH help system
 */

#ifndef __HELP_H
#define __HELP_H

#include <stddef.h>
#include "mushtype.h"

/** A help command.
 * Multiple help commands can be defined, each associated with a help
 * file and an in-memory index.
 */
typedef struct {
  char *command; /**< The name of the help command */
  char *file;    /**< The file of help text */
  int admin;     /**< Is this an admin-only help command? */
} help_file;

void init_help_files(void);
void close_help_files(void);
void add_help_file(const char *command_name, const char *filename, int admin);
void help_rebuild(dbref player);
bool help_rebuild_by_name(const char *filename);
#endif /* __HELP_H */
