/**
 * \file map_file.h
 *
 * \brief Functions for memory-mapped files.
 */

#pragma once

/** Struct for holding a memory mapped file */
struct mapped_file {
#ifdef WIN32
  HANDLE fh; /**< Win32 only: Handle for the open file */
  HANDLE mh; /**< Win32 only: Handle for the map view of the file */
#endif
  void *data; /**< Pointer to file contents */
  size_t len; /**< Length of file contents */
};

typedef struct mapped_file MAPPED_FILE;

MAPPED_FILE *map_file(const char *filename, bool writable);
void unmap_file(MAPPED_FILE *);
