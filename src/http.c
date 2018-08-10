
#include <string.h>
#include <ctype.h>

#include "mushtype.h"
#include "http.h"
#include "strutil.h"
#include "game.h"
#include "externs.h"
#include "attrib.h"
#include "mymalloc.h"
#include "case.h"

static http_method parse_http_method(char *command);
static int parse_http_query(http_request *req, char *line);
static void parse_http_header(http_request *req, char *line);
static int parse_http_content(http_request *req, char *line);

static bool run_http_request(DESC *d);

bool http_timeout_wrapper(void *data);

static void send_http_code(DESC *d, char *code, char *content);
static void send_mudurl(DESC *d);

/* from bsd.c */
extern int queue_write(DESC *d, const char *b, int n);
extern int queue_eol(DESC *d);

const char *http_method_str[] = {
  "UNKNOWN ",
  "GET ",
  "POST ",
  "PUT ",
  "PATCH ",
  "DELETE ",
  NULL
};

/** Parse the HTTP method from a command string
 * \param command string to parse
 */
static http_method
parse_http_method(char *command)
{
  http_method i;

  for (i = HTTP_METHOD_GET; i < HTTP_NUM_METHODS; i++) {
    if (!strncmp(command, http_method_str[i], strlen(http_method_str[i]))) {
      return i;
    }
  }
  return HTTP_METHOD_UNKNOWN;
}

/** Test for a HTTP request command
 * \param command string to test
 */
bool
is_http_request(char *command)
{
  http_method i = parse_http_method(command);
  
  if (i != HTTP_METHOD_UNKNOWN) {
    return true;
  }
  
  return false;
}

/** Parse the HTTP request query string
 * \param req http request object
 * \param line query string to process
 */
static int
parse_http_query(http_request *req, char *line)
{
  char *c, *path, *query, *version;
  http_method method;

  /* Parse a line of the format:
   * METHOD /route/path?query_string HTTP/1.1
   */
  
  if (!req) {
    return 0;
  }
  
  /* extract the method from the start of line */
  method = parse_http_method(line);
  if (method == HTTP_METHOD_UNKNOWN) {
    return 0;
  }
  req->method = method;
  
  /* skip ahead to the path */
  c = strchr(line, ' ');
  if (!c) {
    return 0;
  }
  *(c++) = '\0';

  /* skip extra spaces and get the path+query string */
  for (path = c; *path && isspace(*path); path++);
  c = strchr(path, ' ');
  if (!c) {
    return 0;
  }
  *(c++) = '\0';
  
  /* make sure the path isn't too long */
  if (strlen(path) >= HTTP_PATH_LEN) {
    return 0;
  }
  
  /* check the version string, why not? */
  version = c;
  if (strncmp(version, "HTTP/1.1", 8)) {
    return 0;
  }
  
  /* find the optional query string */
  c = strchr(path, '?');
  if (c) {
    *(c++) = '\0';
    query = c;
    strncpy(req->query, query, HTTP_PATH_LEN - 1);
  }
  
  /* copy the path, with query string removed */
  strncpy(req->path, path, HTTP_PATH_LEN - 1);
  
  /* initialize the request metadata */
  req->state = HTTP_REQUEST_HEADERS;
  req->length = 0;
  req->recv = 0;
  req->hp = req->headers;
  req->cp = req->content;
  
  /* Default HTTP response metadata */
  strncpy(req->res_code, "HTTP/1.1 200 OK", HTTP_CODE_LEN);
  strncpy(req->res_type, "Content-Type: text/plain", HTTP_PATH_LEN);
  
  /* setup the route attribute, skip leading slashes */
  for (c = path; *c == '/'; c++);
  if (!*c) {
    return 0;
  }
  path = c;
  
  /* and trailing slashes */
  for (c = path; *c; c++);
  for (c--; c >= path && *c == '/'; c--);
  c++;
  if (*c == '/') {
    *c = '\0';
  }
  
  /* swap slashes / for ticks ` */
  for (c = path; *c; c++) {
    if (*c == '/') {
      *c = '`';
    }
  }
  
  /* convert the string to upper case */
  c = path;
  while (*c)
  {
    *c = UPCASE(*c);
    c++;
  }
  
  /* copy the route attribute name */
  snprintf(req->route, HTTP_PATH_LEN, "HTTP`%s", path);

  return 1;
}

/** Parse an HTTP request header string
 * \param req http request object
 * \param line header string to process
 */
static void
parse_http_header(http_request *req, char *line)
{
  char *value;
  size_t clen;
  size_t len = strlen(line);
  safe_strl(line, len, req->headers, &(req->hp));
  safe_chr('\n', req->headers, &(req->hp));
  *(req->hp) = '\0';
  
  value = strchr(line, ':');
  if (!value) {
    return;
  }
  *(value++) = '\0';
  
  clen = strlen(line);
  
  if (!strncmp(line, HTTP_CONTENT_LENGTH, clen)) {
    req->length = strtol(value, NULL, 10);
  } else if (!strncmp(line, HTTP_CONTENT_TYPE, clen)) {
    strncpy(req->type, value, strlen(value));
  }
}

/** Parse an HTTP request content string
 * \param req http request object
 * \param line content string to process
 * \retval 1 received content-length number of bytes, done processing
 * \retval 0 continue processing
 */
static int
parse_http_content(http_request *req, char *line)
{
  size_t len = strlen(line);
  
  safe_strl(line, len, req->content, &(req->cp));
  safe_chr('\n', req->content, &(req->cp));
  *(req->cp) = '\0';
  
  req->recv += len;
  
  if (req->recv >= req->length) {
    return 1;
  }
  
  return 0;
}


/** Process HTTP request headers and data
 * \param d descriptor of http request
 * \param command command string to buffer
 */
int
process_http_request(DESC *d, char *command)
{
  char buff[BUFFER_LEN];
  char *bp;
  int done;
  http_request *req = d->http;
  
  if (!req || !command) {
    send_mudurl(d);
    return 0;
  }

  if (d->conn_timer) {
    sq_cancel(d->conn_timer);
    d->conn_timer = NULL;
  }
  
  if (req->state == HTTP_REQUEST_HEADERS) {
    /* a blank line ends the headers */
    if (*command == '\0') {
      if (req->method == HTTP_METHOD_GET) {
        req->state = HTTP_REQUEST_DONE;
        /* we don't need to parse any content, just call the route event */
        if (!run_http_request(d)) {
          bp = buff;
          safe_format(buff, &bp, "File not found. \"%s\"", req->route);
          *bp = '\0';
          send_http_code(d, "404 Not Found", buff);
          return 0;
        }
      } else {
        req->state = HTTP_REQUEST_CONTENT;
      }
    } else {
      parse_http_header(req, command);
    }
  } else if (req->state == HTTP_REQUEST_CONTENT) {
    done = parse_http_content(req, command);
    if (done) {
      req->state = HTTP_REQUEST_DONE;
      /* we finished parsing content, call the route event */
      if (!run_http_request(d)) {
        bp = buff;
        safe_format(buff, &bp, "File not found. \"%s\"", req->route);
        *bp = '\0';
        send_http_code(d, "404 Not Found", buff);
        return 0;
      }
    }
  }

  /* setup a timer to end the connection if no
   * data is sent with a few seconds
   */
  d->conn_timer = sq_register_in(2, http_timeout_wrapper, (void *) d, NULL);

  return 1;
}

/** Parse an HTTP request 
 * \param d descriptor of http request
 * \param command command string to parse
 */
int
do_http_command(DESC *d, char *command)
{
  http_request *req;
  ATTR *a;
  
  /* cancel any test_telnet or welcome_user timers */
  if (d->conn_timer) {
    sq_cancel(d->conn_timer);
    d->conn_timer = NULL;
  }

  /* if the route handler doesn't exist we can
   * close the connection early without parsing
   */
  a = atr_get_noparent(EVENT_HANDLER, "HTTP");
  if (!a) {
    send_http_code(d, "301 Moved Permanently", "Moved to MUDURL.");
    return 0;
  }

  /* allocate the http_request to hold headers and path info */
  req = d->http = mush_malloc(sizeof(http_request), "http_request");
  if (!req) {
    send_http_code(d, "500 Internal Server Error", "Unable to allocate http request.");
    return 0;
  }
  memset(req, 0, sizeof(http_request));
  
  /* parse the query string, return 400 if request is bad */
  if (!parse_http_query(d->http, command)) {
    send_http_code(d, "400 Bad Request", "Invalid request method.");
    return 0;
  }
  
  /* setup a timer to end the connection if no
   * data is sent with a few seconds
   */
  d->conn_timer = sq_register_in(2, http_timeout_wrapper, (void *) d, NULL);
  
  return 1;
}

static bool
run_http_request(DESC *d)
{
  http_request *req = d->http;
  
  if (!req) {
    return false;
  }

  return queue_event(EVENT_HANDLER, req->route,
                     "%d,%s,%s,%s,%s,%s,%s",
                     d->descriptor,
                     d->ip,
                     http_method_str[req->method],
                     req->path,
                     req->query,
                     req->headers,
                     req->content);
}


bool
http_timeout_wrapper(void *data)
{
  DESC *d = (DESC *) data;
  http_request *req = d->http;

  /* we didn't finish parsing content, but call the route event anyway */
  if (req && req->state != HTTP_REQUEST_DONE) {
    req->state = HTTP_REQUEST_DONE;
    if (!run_http_request(d)) {
      send_http_code(d, "404 Not Found", "File not found.");
      return false;
    }
    
    d->conn_timer = sq_register_in(2, http_timeout_wrapper, (void *) d, NULL);
    return false;
  }
  
  send_http_code(d, "408 Request Timeout", "Unable to complete request.");
  
  d->conn_flags |= CONN_HTTP_CLOSE;
  return false;
}

/** Send a HTTP response code.
 * \param d descriptor of the http request
 */  
static void
send_http_code(DESC *d, char *code, char *content)
{
  char buff[BUFFER_LEN];
  char buff2[BUFFER_LEN];
  char *bp = buff;
  char *bp2 = buff2;
  ATTR *a;
  
  http_request *req = d->http;
  
  a = atr_get_noparent(EVENT_HANDLER, req->route);
  if (a) {
    bp2 = safe_atr_value(a, "http route");
  } else {
    safe_str("NO ROUTE", buff2, &bp2);
    *bp2 = '\0';
    bp2 = buff2;
  }
    
  safe_format(buff, &bp,
              "HTTP/1.1 %s\r\n"
              "Content-Type: text/html; charset:iso-8859-1\r\n"
              "Pragma: no-cache\r\n"
              "Connection: Close\r\n"
              "\r\n"
              "<!DOCTYPE html>\r\n"
              "<HTML><HEAD>"
              "<TITLE>%s</TITLE>"
              "</HEAD><BODY><p>%s</p>\r\n"
              "<PRE>"
              "%s: %s\r\n"
              "%s %s?%s\r\n"
              "%s\r\n\r\n"
              "%s\r\n"
              "</PRE></BODY></HTML>\r\n",
              code, code, content, req->route, bp2,
              http_method_str[req->method], req->path, req->query,
              req->headers, req->content);
  *bp = '\0';
  
  if (a) {
    mush_free(bp2, "http route");
  }
  
  queue_write(d, buff, bp - buff);
  queue_eol(d);
}

/** Send the MUDURL default webpage
 * \param d descriptor of the http request
 */  
static void
send_mudurl(DESC *d)
{
  char buf[BUFFER_LEN];
  char *bp = buf;
  bool has_mudurl = strncmp(MUDURL, "http", 4) == 0;

  safe_format(buf, &bp,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html; charset:iso-8859-1\r\n"
              "Pragma: no-cache\r\n"
              "Connection: Close\r\n"
              "\r\n"
              "<!DOCTYPE html>\r\n"
              "<HTML><HEAD>"
              "<TITLE>Welcome to %s!</TITLE>",
              MUDNAME);
  if (has_mudurl) {
    safe_format(buf, &bp,
                "<meta http-equiv=\"refresh\" content=\"5; url=%s\">",
                MUDURL);
  }
  safe_str("</HEAD><BODY><h1>Oops!</h1>", buf, &bp);
  if (has_mudurl) {
    safe_format(buf, &bp,
                "<p>You've come here by accident! Please click <a "
                "href=\"%s\">%s</a> to go to the website for %s if your "
                "browser doesn't redirect you in a few seconds.</p>",
                MUDURL, MUDURL, MUDNAME);
  } else {
    safe_format(buf, &bp,
                "<p>You've come here by accident! Try using a MUSH client, "
                "not a browser, to connect to %s.</p>",
                MUDNAME);
  }
  safe_str("</BODY></HTML>\r\n", buf, &bp);
  *bp = '\0';
  queue_write(d, buf, bp - buf);
  queue_eol(d);
}

