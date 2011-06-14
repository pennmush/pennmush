/**
 * \file help.h
 *
 * \brief Header file for the PennMUSH help system
 */

#ifndef __HELP_H
#define __HELP_H

#define  LINE_SIZE              90
#define  TOPIC_NAME_LEN         30

/** A help index entry.
 *
 */
typedef struct {
  long pos;                     /**< Position of topic in help file, in bytes */
  char topic[TOPIC_NAME_LEN + 1];       /**< name of topic of help entry */
} help_indx;

/** A help command.
 * Multiple help commands can be defined, each associated with a help
 * file and an in-memory index.
 */
typedef struct {
  char *command;        /**< The name of the help command */
  char *file;           /**< The file of help text */
  int admin;            /**< Is this an admin-only help command? */
  help_indx *indx;      /**< An array of help index entries */
  size_t entries;       /**< Number of entries in the help file */
} help_file;


void init_help_files(void);
void add_help_file(const char *command_name, const char *filename, int admin);
void help_reindex(dbref player);
bool help_reindex_by_name(const char *filename);
#endif                          /* __HELP_H */
