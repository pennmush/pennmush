/**
 * \file funjson.c
 *
 * json softcode functions and related.
 *
 * Mostly uses sqlite3 JSON1 functions for manipulating JSON, with
 * some use of cJSON when that's not feasible. Having multiple JSON
 * APIs is non-optimal, but some things are just easier to do with one
 * than they are the other.
 */

#include "copyrite.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "conf.h"
#include "mushtype.h"
#include "externs.h"
#include "mymalloc.h"
#include "strutil.h"
#include "parse.h"
#include "notify.h"
#include "mushsql.h"
#include "charconv.h"
#include "charclass.h"
#include "cJSON.h"
#include "ansi.h"

char *json_vals[3] = {"false", "true", "null"};
int json_val_lens[3] = {5, 4, 4};

static bool json_map_call(ufun_attrib *ufun, sqlite3_str *rbuff,
                          PE_REGS *pe_regs, NEW_PE_INFO *pe_info,
                          sqlite3_stmt *json, dbref executor, dbref enactor);

/** Escape a string for use as a JSON string. Returns a STATIC buffer. */
char *
json_escape_string(char *input)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  char *p;

  for (p = input; *p; p++) {
    if (*p == '\n') {
      safe_str("\\n", buff, &bp);
    } else if (*p == '\r') {
      // Nothing
    } else if (*p == '\t') {
      safe_str("\\t", buff, &bp);
    } else if (*p > 127 || *p <= 0x1F) {
      safe_format(buff, &bp, "\\u%04X", (unsigned) *p);
    } else {
      if (*p == '"' || *p == '\\') {
        safe_chr('\\', buff, &bp);
      }
      safe_chr(*p, buff, &bp);
    }
  }

  *bp = '\0';

  return buff;
}

#include "jsontypes.c"

enum json_query {
  JSON_QUERY_TYPE,
  JSON_QUERY_SIZE,
  JSON_QUERY_EXISTS,
  JSON_QUERY_GET,
  JSON_QUERY_EXTRACT,
  JSON_QUERY_UNESCAPE
};

FUNCTION(fun_json_query)
{
  cJSON *json = NULL, *curr = NULL;
  enum json_query query_type = JSON_QUERY_TYPE;
  int i, path;

  if (nargs > 1 && args[1] && *args[1]) {
    if (strcasecmp("size", args[1]) == 0) {
      query_type = JSON_QUERY_SIZE;
    } else if (strcasecmp("exists", args[1]) == 0) {
      query_type = JSON_QUERY_EXISTS;
    } else if (strcasecmp("get", args[1]) == 0) {
      query_type = JSON_QUERY_GET;
    } else if (strcasecmp("unescape", args[1]) == 0) {
      query_type = JSON_QUERY_UNESCAPE;
    } else if (strcasecmp("type", args[1]) == 0) {
      query_type = JSON_QUERY_TYPE;
    } else if (strcasecmp("extract", args[1]) == 0) {
      query_type = JSON_QUERY_EXTRACT;
    } else {
      safe_str(T("#-1 INVALID OPERATION"), buff, bp);
      return;
    }
  }

  if ((query_type == JSON_QUERY_GET || query_type == JSON_QUERY_EXISTS ||
       query_type == JSON_QUERY_EXTRACT) &&
      (nargs < 3 || !args[2] || !*args[2])) {
    safe_str(T("#-1 MISSING VALUE"), buff, bp);
    return;
  }

  if (query_type != JSON_QUERY_EXTRACT && query_type != JSON_QUERY_TYPE) {
    int ulen;
    char *utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "json.string");
    json = cJSON_Parse(utf8);
    mush_free(utf8, "json.string");
    if (!json) {
      safe_str(T("#-1 INVALID JSON"), buff, bp);
      return;
    }
  }

  switch (query_type) {
  case JSON_QUERY_TYPE: {
    sqlite3 *sqldb = get_shared_db();
    sqlite3_stmt *op;
    char *utf8;
    int ulen, status;
    op = prepare_statement(sqldb, "VALUES (json_type(?))", "json_query.type");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    status = sqlite3_step(op);
    if (status == SQLITE_ROW) {
      const char *t = (const char *) sqlite3_column_text(op, 0);
      size_t tlen = sqlite3_column_bytes(op, 0);
      /* Use a perfect hash table to map sqlite json1 type names to penn names
       */
      const struct json_type_map *type = json_type_lookup(t, tlen);
      if (type) {
        safe_str(type->pname, buff, bp);
      } else {
        safe_str("#-1 UNKNOWN TYPE", buff, bp);
      }
    } else {
      safe_str("#-1 JSON ERROR", buff, bp);
    }
    sqlite3_reset(op);
    break;
  }
  case JSON_QUERY_SIZE:
    if (cJSON_IsBool(json) || cJSON_IsNumber(json) || cJSON_IsString(json)) {
      safe_chr('1', buff, bp);
    } else if (cJSON_IsNull(json) || cJSON_IsInvalid(json)) {
      safe_chr('0', buff, bp);
    } else if (cJSON_IsArray(json) || cJSON_IsObject(json)) {
      safe_integer(cJSON_GetArraySize(json), buff, bp);
    }
    break;
  case JSON_QUERY_UNESCAPE: {
    char *latin1, *c;
    int len;
    if (!cJSON_IsString(json)) {
      safe_str("#-1", buff, bp);
      break;
    }
    latin1 =
      utf8_to_latin1(cJSON_GetStringValue(json), -1, &len, 0, "json.string");
    for (c = latin1; *c; c += 1) {
      if (!isprint(*c) && !isspace(*c)) {
        *c = '?';
      }
    }
    safe_strl(latin1, len, buff, bp);
    mush_free(latin1, "json.string");
  } break;
  case JSON_QUERY_EXISTS:
  case JSON_QUERY_GET:
    curr = json;
    for (path = 2; path < nargs; path += 1) {
      if (!curr || cJSON_IsInvalid(curr) || cJSON_IsBool(curr) ||
          cJSON_IsNull(curr) || cJSON_IsNumber(curr) || cJSON_IsString(curr)) {
        safe_str("#-1", buff, bp);
        curr = NULL;
        goto err;
      }
      if (cJSON_IsArray(curr) && !is_strict_integer(args[path])) {
        safe_str(T(e_int), buff, bp);
        curr = NULL;
        goto err;
      }

      if (cJSON_IsArray(curr)) {
        i = parse_integer(args[path]);
        curr = cJSON_GetArrayItem(curr, i);
      } else if (cJSON_IsObject(curr)) {
        int ulen;
        char *utf8 =
          latin1_to_utf8(args[path], arglens[path], &ulen, "json.string");
        curr = cJSON_GetObjectItemCaseSensitive(curr, utf8);
        mush_free(utf8, "json.string");
      }
    }
    if (query_type == JSON_QUERY_EXISTS) {
      safe_boolean(curr != NULL, buff, bp);
    } else {
      if (curr) {
        int len;
        char *c;
        char *jstr = cJSON_PrintUnformatted(curr);
        char *latin1 = utf8_to_latin1(jstr, -1, &len, 0, "json.string");
        for (c = latin1; *c; c += 1) {
          if (!isprint(*c) && !isspace(*c)) {
            *c = '?';
          }
        }

        safe_strl(latin1, len, buff, bp);
        mush_free(latin1, "json.string");
        free(jstr);
      }
    }
  err:
    break;
  case JSON_QUERY_EXTRACT: {
    sqlite3 *sqldb = get_shared_db();
    sqlite3_stmt *op;
    char *utf8;
    int ulen, status;

    op = prepare_statement(sqldb, "VALUES (json_extract(?, ?))",
                           "json_query.extract");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[2], arglens[2], &ulen, "string");
    sqlite3_bind_text(op, 2, utf8, ulen, free_string);
    status = sqlite3_step(op);

    if (status == SQLITE_ROW) {
      if (sqlite3_column_type(op, 0) != SQLITE_NULL) {
        char *latin1;
        int len;
        char *c;
        const char *p = (const char *) sqlite3_column_text(op, 0);
        int plen = sqlite3_column_bytes(op, 0);
        latin1 = utf8_to_latin1_us(p, plen, &len, 0, "json.string");
        for (c = latin1; *c; c += 1) {
          if (!isprint(*c) && !isspace(*c)) {
            *c = '?';
          }
        }

        safe_strl(latin1, len, buff, bp);
        mush_free(latin1, "json.string");
      }
    } else {
      safe_str("#-1 JSON ERROR", buff, bp);
    }

    sqlite3_reset(op);
    break;
  }
  }
  if (json) {
    cJSON_Delete(json);
  }
}

FUNCTION(fun_json_mod)
{
  sqlite3 *sqldb = get_shared_db();
  sqlite3_stmt *op = NULL;
  char *utf8;
  int status;
  int ulen;

  if (strcasecmp(args[1], "patch") == 0) {
    op =
      prepare_statement(sqldb, "VALUES (json_patch(?, ?))", "json_mod.patch");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[2], arglens[2], &ulen, "string");
    sqlite3_bind_text(op, 2, utf8, ulen, free_string);
  } else if (strcasecmp(args[1], "insert") == 0) {
    if (nargs != 4) {
      safe_str("#-1 FUNCTION EXPECTS 4 ARGUMENTS", buff, bp);
      return;
    }

    op = prepare_statement(sqldb, "VALUES (json_insert(?, ?, json(?)))",
                           "json_mod.insert");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[2], arglens[2], &ulen, "string");
    sqlite3_bind_text(op, 2, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[3], arglens[3], &ulen, "string");
    sqlite3_bind_text(op, 3, utf8, ulen, free_string);
  } else if (strcasecmp(args[1], "replace") == 0) {
    if (nargs != 4) {
      safe_str("#-1 FUNCTION EXPECTS 4 ARGUMENTS", buff, bp);
      return;
    }

    op = prepare_statement(sqldb, "VALUES (json_replace(?, ?, json(?)))",
                           "json_mod.replace");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[2], arglens[2], &ulen, "string");
    sqlite3_bind_text(op, 2, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[3], arglens[3], &ulen, "string");
    sqlite3_bind_text(op, 3, utf8, ulen, free_string);
  } else if (strcasecmp(args[1], "set") == 0) {
    if (nargs != 4) {
      safe_str("#-1 FUNCTION EXPECTS 4 ARGUMENTS", buff, bp);
      return;
    }

    op = prepare_statement(sqldb, "VALUES (json_set(?, ?, json(?)))",
                           "json_mod.set");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[2], arglens[2], &ulen, "string");
    sqlite3_bind_text(op, 2, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[3], arglens[3], &ulen, "string");
    sqlite3_bind_text(op, 3, utf8, ulen, free_string);
  } else if (strcasecmp(args[1], "remove") == 0) {
    op =
      prepare_statement(sqldb, "VALUES (json_remove(?, ?))", "json_mod.remove");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[2], arglens[2], &ulen, "string");
    sqlite3_bind_text(op, 2, utf8, ulen, free_string);
  } else if (strcasecmp(args[1], "sort") == 0) {
    op = prepare_statement(
      sqldb,
      "WITH ordered(value) AS (SELECT CASE WHEN type='text' THEN "
      "json_quote(value) ELSE value END FROM json_each(?1) ORDER BY "
      "json_extract(CASE WHEN type = 'text' THEN json_quote(value) ELSE value "
      "END, ?2)) SELECT json_group_array(json(value)) FROM ordered",
      "json_mod.sort");
    if (!op) {
      safe_str("#-1 SQLITE ERROR", buff, bp);
      return;
    }

    utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
    sqlite3_bind_text(op, 1, utf8, ulen, free_string);
    utf8 = latin1_to_utf8(args[2], arglens[2], &ulen, "string");
    sqlite3_bind_text(op, 2, utf8, ulen, free_string);
  } else {
    safe_str("#-1 INVALID OPERATION", buff, bp);
    return;
  }

  status = sqlite3_step(op);
  if (status == SQLITE_ROW) {
    if (sqlite3_column_type(op, 0) != SQLITE_NULL) {
      char *latin1, *c;
      int len;
      const char *p = (const char *) sqlite3_column_text(op, 0);
      int plen = sqlite3_column_bytes(op, 0);
      latin1 = utf8_to_latin1_us(p, plen, &len, 0, "string");
      for (c = latin1; *c; c += 1) {
        if (!isprint(*c) && !isspace(*c)) {
          *c = '?';
        }
      }

      safe_strl(latin1, len, buff, bp);
      mush_free(latin1, "string");
    }
  } else {
    safe_str("#-1 JSON ERROR", buff, bp);
  }

  sqlite3_reset(op);
}

FUNCTION(fun_json_map)
{
  ufun_attrib ufun;
  PE_REGS *pe_regs;
  char *osep, osepd[2] = {' ', '\0'};
  sqlite3 *sqldb = get_shared_db();
  sqlite3_stmt *mapper;
  sqlite3_str *rbuff;
  char *utf8;
  int ulen;
  int status, i;
  int rows = 0;

  osep = (nargs >= 3) ? args[2] : osepd;

  if (!fetch_ufun_attrib(args[0], executor, &ufun, UFUN_DEFAULT)) {
    return;
  }

  mapper = prepare_statement_cache(
    sqldb, "SELECT type, value, key FROM json_each(?)", "json_map", 0);

  pe_regs = pe_regs_create(PE_REGS_ARG, "fun_json_map");
  for (i = 3; i <= nargs; i++) {
    pe_regs_setenv_nocopy(pe_regs, i, args[i]);
  }

  utf8 = latin1_to_utf8(args[1], arglens[1], &ulen, "string");
  sqlite3_bind_text(mapper, 1, utf8, ulen, free_string);
  rbuff = sqlite3_str_new(sqldb);

  do {
    status = sqlite3_step(mapper);
    if (status == SQLITE_ROW) {
      if (rows++ > 0) {
        sqlite3_str_appendall(rbuff, osep);
      }
      if (json_map_call(&ufun, rbuff, pe_regs, pe_info, mapper, executor,
                        enactor)) {
        status = SQLITE_DONE;
        break;
      }
    }
  } while (status == SQLITE_ROW);
  if (status == SQLITE_DONE) {
    char *res = sqlite3_str_finish(rbuff);
    safe_str(res, buff, bp);
    sqlite3_free(res);
  } else {
    safe_str("#-1 INVALID JSON", buff, bp);
    sqlite3_str_reset(rbuff);
    sqlite3_str_finish(rbuff);
  }
  sqlite3_finalize(mapper);
  pe_regs_free(pe_regs);
}

/** Used by fun_json_map to call the attr for each JSON element. %2-%9 may
 * already be set in the pe_regs
 * \param ufun the ufun to call
 * \param rbuff buffer to store results of ufun call in
 * \param pe_regs the pe_regs holding info for the ufun call
 * \param pe_info the pe_info to eval the attr with
 * \param json the JSON element to pass to the ufun
 * \param executor
 * \param enactor
 * \retval 0 success
 * \retval 1 function invocation limit exceeded
 */
static bool
json_map_call(ufun_attrib *ufun, sqlite3_str *rbuff, PE_REGS *pe_regs,
              NEW_PE_INFO *pe_info, sqlite3_stmt *json, dbref executor,
              dbref enactor)
{
  const char *jtype;
  const struct json_type_map *ptype;
  char buff[BUFFER_LEN] = {'\0'};
  bool status;

  jtype = (const char *) sqlite3_column_text(json, 0);
  ptype = json_type_lookup(jtype, sqlite3_column_bytes(json, 0));

  pe_regs_setenv_nocopy(pe_regs, 0, ptype->pname);

  if (strcmp(jtype, "true") == 0) {
    pe_regs_setenv_nocopy(pe_regs, 1, "true");
  } else if (strcmp(jtype, "false") == 0) {
    pe_regs_setenv_nocopy(pe_regs, 1, "false");
  } else if (strcmp(jtype, "null") == 0) {
    pe_regs_setenv_nocopy(pe_regs, 1, "null");
  } else if (strcmp(jtype, "integer") == 0) {
    pe_regs_setenv(pe_regs, 1, (const char *) sqlite3_column_text(json, 1));
  } else if (strcmp(jtype, "real") == 0) {
    pe_regs_setenv(pe_regs, 1, (const char *) sqlite3_column_text(json, 1));
  } else {
    const char *utf8;
    char *latin1, *c;
    int ulen, len;

    utf8 = (const char *) sqlite3_column_text(json, 1);
    ulen = sqlite3_column_bytes(json, 1);
    latin1 = utf8_to_latin1_us(utf8, ulen, &len, 0, "string");
    for (c = latin1; *c; c += 1) {
      if (!isprint(*c) && !isspace(*c)) {
        *c = '?';
      }
    }

    pe_regs_setenv(pe_regs, 1, latin1);
    mush_free(latin1, "string");
  }
  if (sqlite3_column_type(json, 2) == SQLITE_INTEGER) {
    pe_regs_setenv(pe_regs, 2, pe_regs_intname(sqlite3_column_int(json, 2)));
  } else if (sqlite3_column_type(json, 2) == SQLITE_TEXT) {
    const char *utf8 = (const char *) sqlite3_column_text(json, 2);
    int ulen = sqlite3_column_bytes(json, 2);
    int len;
    char *c;
    char *latin1 = utf8_to_latin1(utf8, ulen, &len, 0, "string");

    for (c = latin1; *c; c += 1) {
      if (!isprint(*c) && !isspace(*c)) {
        *c = '?';
      }
    }

    pe_regs_setenv(pe_regs, 2, latin1);
    mush_free(latin1, "string");
  }

  status = call_ufun(ufun, buff, executor, enactor, pe_info, pe_regs);
  sqlite3_str_appendall(rbuff, buff);
  return status;
}

FUNCTION(fun_json)
{
  enum json_type type;
  int i;
  char tmp[BUFFER_LEN];

  /* strip ansi markup from the type argument */
  strcpy(args[0], remove_markup(args[0], NULL));
  
  if (!*args[0]) {
    type = JSON_STR;
  } else if (strcasecmp("string", args[0]) == 0) {
    type = JSON_STR;
  } else if (strcasecmp("boolean", args[0]) == 0) {
    type = JSON_BOOL;
  } else if (strcasecmp("array", args[0]) == 0) {
    type = JSON_ARRAY;
  } else if (strcasecmp("object", args[0]) == 0) {
    type = JSON_OBJECT;
  } else if (strcasecmp("null", args[0]) == 0) {
    type = JSON_NULL;
  } else if (strcasecmp("number", args[0]) == 0) {
    type = JSON_NUMBER;
  } else {
    safe_str(T("#-1 INVALID TYPE"), buff, bp);
    return;
  }

  if ((type == JSON_NULL && nargs > 2) ||
      ((type == JSON_STR || type == JSON_NUMBER || type == JSON_BOOL) &&
       nargs != 2) ||
      (type == JSON_OBJECT && (nargs % 2) != 1)) {
    safe_str(T("#-1 WRONG NUMBER OF ARGUMENTS"), buff, bp);
    return;
  }

  /* strip ansi markup from non-string types */
  if (type != JSON_STR) {
    for (i = 1; i < nargs; i++) {
      strcpy(args[i], remove_markup(args[i], NULL));
    }
  }

  switch (type) {
  case JSON_NULL:
    if (nargs == 2 && strcmp(args[1], json_vals[2])) {
      safe_str("#-1", buff, bp);
    } else {
      safe_str(json_vals[2], buff, bp);
    }
    return;
  case JSON_BOOL:
    if (strcmp(json_vals[0], args[1]) == 0 || strcmp(args[1], "0") == 0) {
      safe_str(json_vals[0], buff, bp);
    } else if (strcmp(json_vals[1], args[1]) == 0 ||
               strcmp(args[1], "1") == 0) {
      safe_str(json_vals[1], buff, bp);
    } else {
      safe_str("#-1 INVALID VALUE", buff, bp);
    }
    return;
  case JSON_NUMBER:
    if (!is_number(args[1])) {
      safe_str(e_num, buff, bp);
      return;
    }
    safe_str(args[1], buff, bp);
    return;
  case JSON_STR:
    strcpy(tmp, render_string(args[1], MSG_XTERM256));
    safe_format(buff, bp, "\"%s\"", json_escape_string(tmp));
    return;
  case JSON_ARRAY: {
    char *jstr, *latin1, *c;
    int len;
    cJSON *arr = cJSON_CreateArray();
    for (i = 1; i < nargs; i++) {
      char *utf8;
      int ulen;
      utf8 = latin1_to_utf8(args[i], arglens[i], &ulen, "json.string");
      cJSON *elem = cJSON_Parse(utf8);
      mush_free(utf8, "json.string");
      if (!elem) {
        safe_str("#-1 INVALID VALUE", buff, bp);
        cJSON_Delete(arr);
        return;
      }
      cJSON_AddItemToArray(arr, elem);
    }
    jstr = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    latin1 = utf8_to_latin1(jstr, -1, &len, 0, "json.string");
    for (c = latin1; *c; c += 1) {
      if (!isprint(*c) && !isspace(*c)) {
        *c = '?';
      }
    }

    safe_strl(latin1, len, buff, bp);
    mush_free(latin1, "json.string");
    free(jstr);
  } break;
  case JSON_OBJECT: {
    char *jstr, *latin1, *c;
    int len;
    cJSON *obj = cJSON_CreateObject();
    for (i = 1; i < nargs; i += 2) {
      int ulen;
      char *utf8 =
        latin1_to_utf8(args[i + 1], arglens[i + 1], &ulen, "json.string");
      cJSON *elem = cJSON_Parse(utf8);
      mush_free(utf8, "json.string");
      if (!elem || !args[i][0]) {
        safe_str("#-1 INVALID VALUE", buff, bp);
        cJSON_Delete(obj);
        return;
      }
      utf8 = latin1_to_utf8(args[i], arglens[i], &ulen, "json.string");
      cJSON_AddItemToObject(obj, utf8, elem);
      mush_free(utf8, "json.string");
    }
    jstr = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    latin1 = utf8_to_latin1(jstr, -1, &len, 0, "json.string");
    for (c = latin1; *c; c += 1) {
      if (!isprint(*c) && !isspace(*c)) {
        *c = '?';
      }
    }

    safe_strl(latin1, len, buff, bp);
    mush_free(latin1, "json.string");
    free(jstr);
  } break;
  case JSON_NONE:
    break;
  }
}

FUNCTION(fun_isjson)
{
  sqlite3 *sqldb;
  sqlite3_stmt *verify;
  char *utf8;
  int ulen, status;

  sqldb = get_shared_db();
  verify = prepare_statement(sqldb, "VALUES (json_valid(?))", "isjson");
  if (!verify) {
    safe_str("#-1 SQLITE ERROR", buff, bp);
    return;
  }

  utf8 = latin1_to_utf8(args[0], arglens[0], &ulen, "string");
  sqlite3_bind_text(verify, 1, utf8, ulen, free_string);

  status = sqlite3_step(verify);
  if (status == SQLITE_ROW) {
    safe_boolean(sqlite3_column_int(verify, 0), buff, bp);
  } else {
    safe_boolean(0, buff, bp);
  }
  sqlite3_reset(verify);
}
