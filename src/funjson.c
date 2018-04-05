/**
 * \file funjson.c
 *
 * json softcode functions and related.
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

char *json_vals[3] = {"false", "true", "null"};
int json_val_lens[3] = {5, 4, 4};

static bool json_map_call(ufun_attrib *ufun, char *rbuff, PE_REGS *pe_regs,
                          NEW_PE_INFO *pe_info, JSON *json, dbref executor,
                          dbref enactor);


/** Free all memory used by a JSON struct */
void
json_free(JSON *json)
{
  if (!json)
    return;

  if (json->next) {
    json_free(json->next);
    json->next = NULL;
  }

  if (json->data) {
    switch (json->type) {
    case JSON_NONE:
      break; /* Included for completeness; never has data */
    case JSON_NULL:
    case JSON_BOOL:
      break; /* pointers to static args */
    case JSON_OBJECT:
    case JSON_ARRAY:
      json_free(json->data); /* Nested JSON structs */
      break;
    case JSON_STR:
    case JSON_NUMBER:
      mush_free(json->data, "json.data"); /* Plain, malloc'd value */
      break;
    }
    json->data = NULL;
  }

  mush_free(json, "json");
}

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
    } else {
      if (*p == '"' || *p == '\\')
        safe_chr('\\', buff, &bp);
      safe_chr(*p, buff, &bp);
    }
  }

  *bp = '\0';

  return buff;
}

/** Unescape a JSON string. Returns a STATIC buffer. */
char *
json_unescape_string(char *input)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  char *p;
  int escape = 0;

  for (p = input; *p; p++) {
    if (escape) {
      switch (*p) {
      case 'n':
        safe_chr('\n', buff, &bp);
        break;
      case 'r':
        /* Nothing */
        break;
      case 't':
        safe_chr('\t', buff, &bp);
        break;
      case '"':
      case '\\':
        safe_chr(*p, buff, &bp);
        break;
      }
      escape = 0;
    } else if (*p == '\\') {
      escape = 1;
    } else {
      safe_chr(*p, buff, &bp);
    }
  }

  *bp = '\0';

  return buff;
}

/** Convert a JSON struct into a string representation of the JSON
 * \param json The JSON struct to convert
 * \param verbose Add spaces, carriage returns, etc, to make the JSON
 * human-readable?
 * \param recurse Number of recursions; always call with this set to 0
 * \retval NULL error occurred
 * \retval result string representation of the JSON struct, malloc'd as
 * "json_str"
 */
char *
json_to_string_real(JSON *json, int verbose, int recurse)
{
  char buff[BUFFER_LEN];
  char *bp = buff;
  JSON *next;
  int i = 0;
  char *sub;
  int error = 0;
  double *np;

  if (!json)
    return NULL;

  switch (json->type) {
  case JSON_NONE:
    break;
  case JSON_NUMBER:
    np = (NVAL *) json->data;
    error = safe_number(*np, buff, &bp);
    break;
  case JSON_STR:
    error =
      safe_format(buff, &bp, "\"%s\"", json_escape_string((char *) json->data));
    break;
  case JSON_BOOL:
    error = safe_str((char *) json->data, buff, &bp);
    break;
  case JSON_NULL:
    error = safe_str((char *) json->data, buff, &bp);
    break;
  case JSON_ARRAY:
    error = safe_chr('[', buff, &bp);
    next = (JSON *) json->data;
    i = 0;
    for (next = (JSON *) json->data, i = 0; next; next = next->next, i++) {
      sub = json_to_string_real(next, verbose, recurse + 1);
      if (i)
        error = safe_chr(',', buff, &bp);
      if (sub != NULL) {
        if (verbose) {
          error = safe_chr('\n', buff, &bp);
          error = safe_fill(' ', (recurse + 1) * 4, buff, &bp);
        }
        error = safe_str(sub, buff, &bp);
        mush_free(sub, "json_str");
      }
    }
    if (verbose) {
      error = safe_chr('\n', buff, &bp);
      error = safe_fill(' ', recurse * 4, buff, &bp);
    }
    error = safe_chr(']', buff, &bp);
    break;
  case JSON_OBJECT:
    error = safe_chr('{', buff, &bp);
    next = (JSON *) json->data;
    i = 0;
    while (next && !error) {
      if (!(i % 2) && next->type != JSON_STR) {
        error = 1;
        break;
      }
      if (i > 0) {
        error = safe_chr((i % 2) ? ':' : ',', buff, &bp);
        if (verbose)
          error = safe_chr(' ', buff, &bp);
      }
      if (verbose && !(i % 2)) {
        error = safe_chr('\n', buff, &bp);
        error = safe_fill(' ', (recurse + 1) * 4, buff, &bp);
      }
      sub = json_to_string_real(next, verbose, recurse + 1);
      if (sub != NULL) {
        error = safe_str(sub, buff, &bp);
        mush_free(sub, "json_str");
      } else {
        error = 1;
        break;
      }
      next = next->next;
      i++;
    }
    if (verbose) {
      error = safe_chr('\n', buff, &bp);
      error = safe_fill(' ', recurse * 4, buff, &bp);
    }
    error = safe_chr('}', buff, &bp);
    break;
  }

  if (error) {
    return NULL;
  } else {
    *bp = '\0';
    return mush_strdup(buff, "json_str");
  }
}

/** Convert a string representation to a JSON struct.
 *  Destructively modifies input.
 * \param input The string to parse
 * \param ip A pointer to the position we're at in "input", for recursive calls.
 *           Set to NULL for initial call.
 * \param recurse Recursion level. Set to 0 for initial call.
 * \retval NULL string did not contain valid JSON
 * \retval json a JSON struct representing the json from input
 */
JSON *
string_to_json_real(char *input, char **ip, int recurse)
{
  JSON *result = NULL, *last = NULL, *next = NULL;
  char *p;
  double d;

  if (ip == NULL) {
    ip = &input;
  }

  result = mush_malloc(sizeof(JSON), "json");
  result->type = JSON_NONE;
  result->data = NULL;
  result->next = NULL;

  if (!input || !*input) {
    return result;
  }

  /* Skip over leading spaces */
  while (**ip && isspace(**ip))
    (*ip)++;

  if (!**ip) {
    return result;
  }

  if (!strncmp(*ip, json_vals[0], json_val_lens[0])) {
    result->type = JSON_BOOL;
    result->data = json_vals[0];
    *ip += json_val_lens[0];
  } else if (!strncmp(*ip, json_vals[1], json_val_lens[1])) {
    result->type = JSON_BOOL;
    result->data = json_vals[1];
    *ip += json_val_lens[1];
  } else if (!strncmp(*ip, json_vals[2], json_val_lens[2])) {
    result->type = JSON_NULL;
    result->data = json_vals[2];
    *ip += json_val_lens[2];
  } else if (**ip == '"') {
    /* Validate string */
    for (p = ++(*ip); **ip; (*ip)++) {
      if (**ip == '\\') {
        (*ip)++;
      } else if (**ip == '"') {
        break;
      }
    }
    if (**ip == '"') {
      result->type = JSON_STR;
      *(*ip)++ = '\0';
      result->data = mush_strdup(json_unescape_string(p), "json.data");
    }
  } else if (**ip == '[') {
    int i = 0;
    (*ip)++; /* Skip over the opening [ */
    while (**ip) {
      while (**ip && isspace(**ip))
        (*ip)++; /* Skip over leading spaces */
      if (**ip == ']')
        break;
      next = string_to_json_real(input, ip, recurse + 1);
      if (next == NULL)
        break; /* Error in the array contents */
      if (i == 0) {
        result->data = next;
      } else {
        last->next = next;
      }
      last = next;
      while (**ip && isspace(**ip))
        (*ip)++;
      if (**ip == ',') {
        (*ip)++;
      } else {
        break;
      }
      i++;
    }
    if (**ip == ']') {
      (*ip)++;
      result->type = JSON_ARRAY;
    }
  } else if (**ip == '{') {
    int i = 0;
    (*ip)++;
    while (**ip) {
      while (**ip && isspace(**ip))
        (*ip)++;
      if (**ip == '}')
        break;
      next = string_to_json_real(input, ip, recurse + 1);
      if (next == NULL)
        break; /* Error */
      if (i == 0)
        result->data = next;
      else
        last->next = next;
      last = next;
      if (!(i % 2) && next->type != JSON_STR) {
        /* It should have been a label, but it's not */
        break;
      }
      while (**ip && isspace(**ip))
        (*ip)++;
      if (**ip == ',' && (i % 2))
        (*ip)++;
      else if (**ip == ':' && !(i % 2))
        (*ip)++;
      else {
        break; /* error */
      }
      i++;
    }
    if ((i == 0 || (i % 2)) && **ip == '}') {
      (*ip)++;
      result->type = JSON_OBJECT;
    }
  } else {
    d = strtod(*ip, &p);
    if (p != *ip) {
      /* We have a number */
      NVAL *data = mush_malloc(sizeof(NVAL), "json.data");
      result->type = JSON_NUMBER;
      *data = d;
      result->data = data;
      *ip = p;
    } else {
      result->type = JSON_NONE;
    }
  }

  if (result->type == JSON_NONE) {
    /* If it's set to JSON_NONE at this point, we had an error */
    json_free(result);
    return NULL;
  }
  while (**ip && isspace(**ip))
    (*ip)++;
  if (!recurse && **ip != '\0') {
    /* Text left after we finished parsing; invalid JSON */
    json_free(result);
    return NULL;
  } else {
    return result;
  }
}

enum json_query {
  JSON_QUERY_TYPE,
  JSON_QUERY_SIZE,
  JSON_QUERY_EXISTS,
  JSON_QUERY_GET,
  JSON_QUERY_UNESCAPE
};

FUNCTION(fun_json_query)
{
  JSON *json, *next;
  enum json_query query_type = JSON_QUERY_TYPE;
  int i;

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
    } else {
      safe_str(T("#-1 INVALID OPERATION"), buff, bp);
      return;
    }
  }

  if ((query_type == JSON_QUERY_GET || query_type == JSON_QUERY_EXISTS) &&
      (nargs < 3 || !args[2] || !*args[2])) {
    safe_str(T("#-1 MISSING VALUE"), buff, bp);
    return;
  }

  json = string_to_json(args[0]);
  if (!json) {
    safe_str(T("#-1 INVALID JSON"), buff, bp);
    return;
  }

  switch (query_type) {
  case JSON_QUERY_TYPE:
    switch (json->type) {
    case JSON_NONE:
      break; /* Should never happen */
    case JSON_STR:
      safe_str("string", buff, bp);
      break;
    case JSON_BOOL:
      safe_str("boolean", buff, bp);
      break;
    case JSON_NULL:
      safe_str("null", buff, bp);
      break;
    case JSON_NUMBER:
      safe_str("number", buff, bp);
      break;
    case JSON_ARRAY:
      safe_str("array", buff, bp);
      break;
    case JSON_OBJECT:
      safe_str("object", buff, bp);
      break;
    }
    break;
  case JSON_QUERY_SIZE:
    switch (json->type) {
    case JSON_NONE:
      break;
    case JSON_STR:
    case JSON_BOOL:
    case JSON_NUMBER:
      safe_chr('1', buff, bp);
      break;
    case JSON_NULL:
      safe_chr('0', buff, bp);
      break;
    case JSON_ARRAY:
    case JSON_OBJECT:
      next = (JSON *) json->data;
      if (!next) {
        safe_chr('0', buff, bp);
        break;
      }
      for (i = 1; next->next; i++, next = next->next)
        ;
      if (json->type == JSON_OBJECT) {
        i = i / 2; /* Key/value pairs, so we have half as many */
      }
      safe_integer(i, buff, bp);
      break;
    }
    break;
  case JSON_QUERY_UNESCAPE:
    if (json->type != JSON_STR) {
      safe_str("#-1", buff, bp);
      break;
    }
    safe_str(json_unescape_string((char *) json->data), buff, bp);
    break;
  case JSON_QUERY_EXISTS:
  case JSON_QUERY_GET:
    switch (json->type) {
    case JSON_NONE:
      break;
    case JSON_STR:
    case JSON_BOOL:
    case JSON_NUMBER:
    case JSON_NULL:
      safe_str("#-1", buff, bp);
      break;
    case JSON_ARRAY:
      if (!is_strict_integer(args[2])) {
        safe_str(T(e_int), buff, bp);
        break;
      }
      i = parse_integer(args[2]);
      for (next = json->data; i > 0 && next; next = next->next, i--)
        ;

      if (query_type == JSON_QUERY_EXISTS) {
        safe_chr((next) ? '1' : '0', buff, bp);
      } else if (next) {
        char *s = json_to_string(next, 0);
        if (s) {
          safe_str(s, buff, bp);
          mush_free(s, "json_str");
        }
      }
      break;
    case JSON_OBJECT:
      next = (JSON *) json->data;
      while (next) {
        if (next->type != JSON_STR) {
          /* We should have a string label */
          next = NULL;
          break;
        }
        if (!strcasecmp((char *) next->data, args[2])) {
          /* Success! */
          next = next->next;
          break;
        } else {
          /* Skip */
          next = next->next; /* Move to this entry's value */
          if (next) {
            next = next->next; /* Move to next entry's name */
          }
        }
      }
      if (query_type == JSON_QUERY_EXISTS) {
        safe_chr((next) ? '1' : '0', buff, bp);
      } else if (next) {
        char *s = json_to_string(next, 0);
        if (s) {
          safe_str(s, buff, bp);
          mush_free(s, "json_str");
        }
      }
      break;
    }
    break;
  }
  json_free(json);
}

FUNCTION(fun_json_map)
{
  ufun_attrib ufun;
  PE_REGS *pe_regs;
  int funccount;
  char *osep, osepd[2] = {' ', '\0'};
  JSON *json, *next;
  int i;
  char rbuff[BUFFER_LEN];

  osep = (nargs >= 3) ? args[2] : osepd;

  if (!fetch_ufun_attrib(args[0], executor, &ufun, UFUN_DEFAULT))
    return;

  json = string_to_json(args[1]);
  if (!json) {
    safe_str(T("#-1 INVALID JSON"), buff, bp);
    return;
  }

  pe_regs = pe_regs_create(PE_REGS_ARG, "fun_json_map");
  for (i = 3; i <= nargs; i++) {
    pe_regs_setenv_nocopy(pe_regs, i, args[i]);
  }

  switch (json->type) {
  case JSON_NONE:
    break;
  case JSON_STR:
  case JSON_BOOL:
  case JSON_NULL:
  case JSON_NUMBER:
    /* Basic data types */
    json_map_call(&ufun, rbuff, pe_regs, pe_info, json, executor, enactor);
    safe_str(rbuff, buff, bp);
    break;
  case JSON_ARRAY:
  case JSON_OBJECT:
    /* Complex types */
    for (next = json->data, i = 0; next; next = next->next, i++) {
      funccount = pe_info->fun_invocations;
      if (json->type == JSON_ARRAY) {
        pe_regs_setenv(pe_regs, 2, pe_regs_intname(i));
      } else {
        pe_regs_setenv_nocopy(pe_regs, 2, (char *) next->data);
        next = next->next;
        if (!next)
          break;
      }
      if (json_map_call(&ufun, rbuff, pe_regs, pe_info, next, executor,
                        enactor))
        break;
      if (i > 0)
        safe_str(osep, buff, bp);
      safe_str(rbuff, buff, bp);
      if (*bp >= (buff + BUFFER_LEN - 1) &&
          pe_info->fun_invocations == funccount)
        break;
    }
    break;
  }

  json_free(json);
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
json_map_call(ufun_attrib *ufun, char *rbuff, PE_REGS *pe_regs,
              NEW_PE_INFO *pe_info, JSON *json, dbref executor, dbref enactor)
{
  char *jstr = NULL;

  switch (json->type) {
  case JSON_NONE:
    return 0;
  case JSON_STR:
    pe_regs_setenv_nocopy(pe_regs, 0, "string");
    pe_regs_setenv_nocopy(pe_regs, 1, (char *) json->data);
    break;
  case JSON_BOOL:
    pe_regs_setenv_nocopy(pe_regs, 0, "boolean");
    pe_regs_setenv_nocopy(pe_regs, 1, (char *) json->data);
    break;
  case JSON_NULL:
    pe_regs_setenv_nocopy(pe_regs, 0, "null");
    pe_regs_setenv_nocopy(pe_regs, 1, (char *) json->data);
    break;
  case JSON_NUMBER:
    pe_regs_setenv_nocopy(pe_regs, 0, "number");
    {
      char buff[BUFFER_LEN];
      char *bp = buff;
      safe_number(*(NVAL *) json->data, buff, &bp);
      *bp = '\0';
      pe_regs_setenv(pe_regs, 1, buff);
    }
    break;
  case JSON_ARRAY:
    pe_regs_setenv_nocopy(pe_regs, 0, "array");
    jstr = json_to_string(json, 0);
    pe_regs_setenv(pe_regs, 1, jstr);
    if (jstr)
      mush_free(jstr, "json_str");
    break;
  case JSON_OBJECT:
    pe_regs_setenv_nocopy(pe_regs, 0, "object");
    jstr = json_to_string(json, 0);
    pe_regs_setenv(pe_regs, 1, jstr);
    if (jstr)
      mush_free(jstr, "json_str");
    break;
  }

  return call_ufun(ufun, rbuff, executor, enactor, pe_info, pe_regs);
}

FUNCTION(fun_json)
{
  enum json_type type;
  int i;

  if (!*args[0])
    type = JSON_STR;
  else if (strcasecmp("string", args[0]) == 0)
    type = JSON_STR;
  else if (strcasecmp("boolean", args[0]) == 0)
    type = JSON_BOOL;
  else if (strcasecmp("array", args[0]) == 0)
    type = JSON_ARRAY;
  else if (strcasecmp("object", args[0]) == 0)
    type = JSON_OBJECT;
  else if (strcasecmp("null", args[0]) == 0)
    type = JSON_NULL;
  else if (strcasecmp("number", args[0]) == 0)
    type = JSON_NUMBER;
  else {
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

  switch (type) {
  case JSON_NULL:
    if (nargs == 2 && strcmp(args[1], json_vals[2]))
      safe_str("#-1", buff, bp);
    else
      safe_str(json_vals[2], buff, bp);
    return;
  case JSON_BOOL:
    if (strcmp(json_vals[0], args[1]) == 0 || strcmp(args[1], "0") == 0)
      safe_str(json_vals[0], buff, bp);
    else if (strcmp(json_vals[1], args[1]) == 0 || strcmp(args[1], "1") == 0)
      safe_str(json_vals[1], buff, bp);
    else
      safe_str("#-1 INVALID VALUE", buff, bp);
    return;
  case JSON_NUMBER:
    if (!is_number(args[1])) {
      safe_str(e_num, buff, bp);
      return;
    }
    safe_str(args[1], buff, bp);
    return;
  case JSON_STR:
    safe_format(buff, bp, "\"%s\"", json_escape_string(args[1]));
    return;
  case JSON_ARRAY:
    safe_chr('[', buff, bp);
    for (i = 1; i < nargs; i++) {
      if (i > 1) {
        safe_strl(", ", 2, buff, bp);
      }
      safe_str(args[i], buff, bp);
    }
    safe_chr(']', buff, bp);
    return;
  case JSON_OBJECT:
    safe_chr('{', buff, bp);
    for (i = 1; i < nargs; i += 2) {
      if (i > 1)
        safe_strl(", ", 2, buff, bp);
      safe_format(buff, bp, "\"%s\": %s", json_escape_string(args[i]),
                  args[i + 1]);
    }
    safe_chr('}', buff, bp);
    return;
  case JSON_NONE:
    break;
  }
}
