
#include <string.h>
#include <ctype.h>

#include "mushtype.h"
#include "strutil.h"
#include "game.h"
#include "externs.h"
#include "attrib.h"
#include "mymalloc.h"
#include "case.h"
#include "command.h"
#include "parse.h"

#include "http.h"

static http_method parse_http_method(char *command);
static int parse_http_query(http_request *req, char *line);
static void parse_http_header(http_request *req, char *line);
static int parse_http_content(http_request *req, char *line);

static bool run_http_request(DESC *d);

bool http_timeout_wrapper(void *data);

static void send_http_status(DESC *d, char *status, char *content);
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

struct HTTP_STATUS_CODE http_status_codes[] = {
  {200, "200 Ok"},
  {400, "400 Bad Request"},
  {404, "404 Not Found"},
  {408, "408 Request Timeout"},
  {500, "500 Internal Server Error"},
  {0, NULL}
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

/** Return the status code string.
 * \param code the status code to transform
 */
static const char *
get_http_status(http_status code)
{
  struct HTTP_STATUS_CODE *c;
  
  for (c = http_status_codes; c && c->code; c++)
  {
    if (c->code == code) {
      return c->str;
    }
  }
  
  return NULL;
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
  if (strlen(path) >= HTTP_STR_LEN) {
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
    strncpy(req->query, query, HTTP_STR_LEN - 1);
  }
  
  /* copy the path, with query string removed */
  strncpy(req->path, path, HTTP_STR_LEN - 1);
  
  /* initialize the request metadata */
  req->state = HTTP_REQUEST_HEADERS;
  req->length = 0;
  req->recv = 0;
  req->hp = req->headers;
  req->cp = req->content;
  req->rp = req->response;
  
  /* Default HTTP response metadata */
  req->status = HTTP_STATUS_200;
  strncpy(req->res_type, "Content-Type: text/plain\r\n", HTTP_STR_LEN);
  
  /* setup the route attribute, skip leading slashes */
  for (c = path; *c == '/'; c++);
  if (*c) {
    /* path has more to it that just /, let's parse the rest */
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
  } else {
    /* the path was just /, default to INDEX */
    path = "INDEX";
  }
  
  /* copy the route attribute name */
  snprintf(req->route, HTTP_STR_LEN, "HTTP`%s", path);

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
  size_t len = strlen(line);
  safe_strl(line, len, req->headers, &(req->hp));
  safe_chr('\n', req->headers, &(req->hp));
  *(req->hp) = '\0';
  
  value = strchr(line, ':');
  if (!value) {
    return;
  }
  *(value++) = '\0';
  
  if (!strncmp(line, HTTP_CONTENT_LENGTH, strlen(HTTP_CONTENT_LENGTH))) {
    req->length = parse_integer(value);
  } else if (!strncmp(line, HTTP_CONTENT_TYPE, strlen(HTTP_CONTENT_TYPE))) {
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
  
  notify_format((dbref) 5, "PROCESS: %s", command);
  
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
          send_http_status(d, "404 Not Found", buff);
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
        send_http_status(d, "404 Not Found", buff);
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
    send_mudurl(d);
    return 0;
  }

  /* allocate the http_request to hold headers and path info */
  req = d->http = mush_malloc(sizeof(http_request), "http_request");
  if (!req) {
    send_http_status(d, "500 Internal Server Error", "Unable to allocate http request.");
    return 0;
  }
  memset(req, 0, sizeof(http_request));
  
  /* parse the query string, return 400 if request is bad */
  if (!parse_http_query(d->http, command)) {
    send_http_status(d, "400 Bad Request", "Invalid request method.");
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
      send_http_status(d, "404 Not Found", "File not found.");
      return false;
    }
    
    d->conn_timer = sq_register_in(2, http_timeout_wrapper, (void *) d, NULL);
    return false;
  }
  
  send_http_status(d, "408 Request Timeout", "Unable to complete request.");
  
  boot_desc(d, "http close", GOD);
  return false;
}

/** Send a HTTP response code.
 * \param d descriptor of the http request
 */  
static void
send_http_status(DESC *d, char *code, char *content)
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
  char buff[BUFFER_LEN];
  char *bp = buff;
  bool has_mudurl = strncmp(MUDURL, "http", 4) == 0;

  safe_format(buff, &bp,
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
    safe_format(buff, &bp,
                "<meta http-equiv=\"refresh\" content=\"5; url=%s\">",
                MUDURL);
  }
  safe_str("</HEAD><BODY><h1>Oops!</h1>", buff, &bp);
  if (has_mudurl) {
    safe_format(buff, &bp,
                "<p>You've come here by accident! Please click <a "
                "href=\"%s\">%s</a> to go to the website for %s if your "
                "browser doesn't redirect you in a few seconds.</p>",
                MUDURL, MUDURL, MUDNAME);
  } else {
    safe_format(buff, &bp,
                "<p>You've come here by accident! Try using a MUSH client, "
                "not a browser, to connect to %s.</p>",
                MUDNAME);
  }
  safe_str("</BODY></HTML>\r\n", buff, &bp);
  *bp = '\0';
  queue_write(d, buff, bp - buff);
  queue_eol(d);
}

/* @respond command used to send HTTP responses */
COMMAND(cmd_respond)
{
  char buff[BUFFER_LEN];
  char *bp;
  const char *p;
  http_request *req;
  int code;
  DESC *d;
  int set_type = 0;
  
  if (!arg_left || !*arg_left) {
    notify(executor, T("Invalid arguments."));
    return;
  }
  
  d = port_desc(parse_integer(arg_left));
  if (!d) {
    notify(executor, T("Descriptor not found."));
    return;
  }
  
  if (!d->http || !(d->conn_flags & CONN_HTTP_REQUEST)) {
    notify(executor, T("Descriptor has not made an HTTP request."));
    return;
  }
  
  req = d->http;
  
  /* if /html, /text, or /json are set change the Content-Type */
  if (SW_ISSET(sw, SWITCH_HTML)) {
    strncpy(req->res_type, "Content-Type: text/html\r\n", HTTP_STR_LEN);
    set_type = 1;
  } else if (SW_ISSET(sw, SWITCH_JSON)) {
    strncpy(req->res_type, "Content-Type: application/json\r\n", HTTP_STR_LEN);
    set_type = 1;
  } else if (SW_ISSET(sw, SWITCH_TEXT)) {
    strncpy(req->res_type, "Content-Type: text/plain\r\n", HTTP_STR_LEN);
    set_type = 1;
  }

  if (SW_ISSET(sw, SWITCH_TYPE)) {
    /* @respond/type set the content-type header, defaults to text/plain */
    
    if (!arg_right || !*arg_right) {
      notify(executor, T("Invalid arguments."));
      return;
    }
    
    snprintf(req->res_type, HTTP_STR_LEN, "Content-Type: %s\r\n", arg_right);
    return;
  
  } else if (SW_ISSET(sw, SWITCH_HEADER)) {
    /* @respond/header set any other headers */
    
    if (!arg_right || !*arg_right) {
      notify(executor, T("Invalid arguments."));
      return;
    }
    
    /* check the header format */
    p = strchr(arg_right, ':');
    if (!p || (p - arg_right) < 1) {
      notify(executor, T("Invalid format, expected \"Header-Name: Value\"."));
      return;
    }
    
    /* prevent hijacking Content-Type or Content-Length */
    if (!strncasecmp(arg_right, HTTP_CONTENT_LENGTH, strlen(HTTP_CONTENT_LENGTH))) {
      notify(executor, T("You may not manually set the Content-Length header."));
      return;
    } else if (!strncasecmp(arg_right, HTTP_CONTENT_TYPE, strlen(HTTP_CONTENT_TYPE))) {
      notify(executor, T("You may not manually set the Content-Type header."));
      return;
    }
    
    /* save the response header */
    safe_str(arg_right, req->response, &(req->rp));
    safe_str("\r\n", req->response, &(req->rp));
    *(req->hp) = '\0';
    return;
  } else if (SW_ISSET(sw, SWITCH_STATUS)) {
    /* @respond/status set the response status code, default 200 Ok */
    
    if (!arg_right || !*arg_right) {
      notify(executor, T("Invalid arguments."));
      return;
    }
    
    code = parse_integer(arg_right);
    p = get_http_status(code);
    if (!p) {
      notify(executor, T("Invalid HTTP status code."));
      return;
    }
    
    req->status = code;
    return;
  } else {
    /* @respond send the given response with headers and close the request */
    
    if (!arg_right || !*arg_right) {
      if (!set_type) {
        /* only send an error if we didn't set /text, /html, or /json */
        notify(executor, T("Invalid arguments."));
      }
      return;
    }
    
    p = get_http_status(req->status);
    
    /* build the response buffer */
    bp = buff;
    safe_format(buff, &bp, "HTTP/1.1 %s\r\n", p);
    safe_str(req->response, buff, &bp);
    safe_str(req->res_type, buff, &bp);
    safe_format(buff, &bp, "Content-Length: %lu\r\n", strlen(arg_right));
    safe_str("\r\n", buff, &bp);
    safe_str(arg_right, buff, &bp);
    *bp = '\0';
    
    /* send the response and close the connection */
    queue_write(d, buff, bp - buff);
    queue_eol(d);
    boot_desc(d, "http response", GOD);
  }
  
  return;
}



