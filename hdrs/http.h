/**
 * \file http.h
 *
 * \brief Miscellaneous functions for http functionality.
 */

#ifndef _HTTP_H_
#define _HTTP_H_
#ifdef HAVE_LIBCURL

#include "copyrite.h"
#include "mushsql.h"
#include "mushtype.h"
#include "mypcre.h"
#include <curl/curl.h>

/** Supported http types */
enum http_verb {
  HTTP_GET = 0,
  HTTP_POST = 1,
  HTTP_DELETE = 2,
  HTTP_PUT = 3
};

/* Data for successfull @fetch commands */
struct urlreq {
  dbref thing;
  dbref enactor;
  int queue_type;
  int too_big;
  PE_REGS *pe_regs;
  char *attrname;
  sqlite3_str *body;
  void *header_slist;
};

void free_urlreq(struct urlreq *req);
int req_set_cloexec(void *clientp __attribute__((__unused__)), curl_socket_t fd,
                curlsocktype purpose __attribute__((__unused__)));
size_t req_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
enum http_verb string_to_verb(char * verb);

#endif /** HAVE_LIBCURL **/
#endif