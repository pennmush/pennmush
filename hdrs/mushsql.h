#pragma once
/**
 * \file mushsql.c
 *
 * \brief API for working with internal SQLite3 databases.
 */

#include "sqlite3.h"
#include "compile.h"

sqlite3 *open_sql_db(const char *, bool);
void close_sql_db(sqlite3 *);
sqlite3 *get_shared_db(void);
void close_shared_db(void);

int get_sql_db_id(sqlite3 *, int *app_id, int *version);

sqlite3_stmt *prepare_statement_cache(sqlite3 *, const char *, const char *,
                                      bool);
static inline sqlite3_stmt *
prepare_statement(sqlite3 *db, const char *query, const char *name)
{
  return prepare_statement_cache(db, query, name, 1);
}

void close_statement(sqlite3_stmt *);

char *glob_to_like(const char *orig, char esc, int *len) __attribute_malloc__;
char *escape_like(const char *orig, char esc, int *len) __attribute_malloc__;

bool is_busy_status(int);
void free_string(void *);

bool optimize_db(sqlite3 *);
