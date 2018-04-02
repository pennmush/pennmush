#pragma once
/**
 * \file mushsql.c
 *
 * \brief API for working with internal SQLite3 databases.
 */

#include "sqlite3.h"

sqlite3 *open_sql_db(const char *);
void close_sql_db(sqlite3 *);
sqlite3 *get_shared_db(void);
void close_shared_db(void);

sqlite3_stmt* prepare_statement(sqlite3 *, const char *, const char *);
void close_statement(sqlite3_stmt *);

char *glob_to_like(const char *orig, char esc, int *len) __attribute_malloc__;

bool is_busy_status(int);
void free_string(void *);

