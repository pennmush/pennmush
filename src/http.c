/**
 * \file http.c
 *
 * \brief Miscellaneous functions for http functionality.
 *
 *
 */

#ifdef HAVE_LIBCURL
#include "http.h"
#include "mymalloc.h"
#include "mysocket.h"
#include "parse.h"

void
free_urlreq(struct urlreq *req)
{
  pe_regs_free(req->pe_regs);
  if (req->body) {
    sqlite3_str_reset(req->body);
    sqlite3_str_finish(req->body);
  }
  mush_free(req->attrname, "urlreq.attrname");
  curl_slist_free_all(req->header_slist);
  mush_free(req, "urlreq");
}

size_t
req_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  struct urlreq *req = userp;
  size_t realsize = size * nmemb;
  sqlite3_str_append(req->body, contents, realsize);
  if (sqlite3_str_length(req->body) >= BUFFER_LEN) {
    /* Raise an error but keep going. We will handle this later. */
    req->too_big = 1;
    return realsize;
  } else {
    return realsize;
  }
}

int
req_set_cloexec(void *clientp __attribute__((__unused__)), curl_socket_t fd,
                curlsocktype purpose __attribute__((__unused__)))
{
  set_close_exec(fd);
  return CURL_SOCKOPT_OK;
}
#endif /** HAVE_LIBCURL **/