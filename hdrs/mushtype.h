/**
 * \file mushtype.h
 *
 * \brief Several commonly-used structs, \#defines, and other stuff
 */

#ifndef MUSH_TYPES_H
#define MUSH_TYPES_H

#include "copyrite.h"
#include <openssl/ssl.h>
#include <signal.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "cJSON.h"

#define NUMQ 36

/** Math function floating-point number type */
typedef double NVAL;

/* Math function integral type */
typedef int64_t IVAL;

/** Math function unsigned integral type */
typedef uint64_t UIVAL;

/** Dbref type */
typedef int dbref;

/** The type that stores the warning bitmask */
typedef uint32_t warn_type;

/** Attribute/lock flag types */
typedef uint32_t privbits;

/* special dbref's */
#define NOTHING (-1)   /**< null dbref */
#define AMBIGUOUS (-2) /**< multiple possibilities, for matchers */
#define HOME (-3)      /**< virtual room, represents mover's home */
#define ANY_OWNER (-2) /**< For lstats and \@stat */

#define INTERACT_SEE 0x1
#define INTERACT_HEAR 0x2
#define INTERACT_MATCH 0x4
#define INTERACT_PRESENCE 0x8

typedef uint8_t *object_flag_type;

/* Boolexps and locks */
typedef const char *lock_type;
typedef struct lock_list lock_list;

/* Set this somewhere near the recursion limit */
#define MAX_ITERS 100

/* max length of command argument to process_command */
#define MAX_COMMAND_LEN 4096
#define BUFFER_LEN ((MAX_COMMAND_LEN) *2)
#define MAX_ARG 63

typedef struct new_pe_info NEW_PE_INFO;
typedef struct debug_info Debug_Info;
/** process_expression() info */

#define PE_KEY_LEN 64 /**< The maximum key length. */

/* Types for _pe_regs_ _and_ _pe_reg_vals_ */
#define PE_REGS_Q 0x01                 /**< Q-registers. */
#define PE_REGS_REGEXP 0x02            /**< Regexps. */
#define PE_REGS_CAPTURE PE_REGS_REGEXP /**< Alias for REGEXP */
#define PE_REGS_SWITCH 0x04            /**< switch(), %$0. */
#define PE_REGS_ITER 0x08              /**< iter() and \@dolist, %i0/etc */
#define PE_REGS_ARG 0x10               /**< %0-%9 */
#define PE_REGS_SYS 0x20               /**< %c, %z, %= */

#define PE_REGS_TYPE 0xFF  /**< The type mask, everything over is flags. */
#define PE_REGS_QUEUE 0xFF /**< Every type for a queue. */

/* Flags for _pe_regs_: */
#define PE_REGS_LET                                                            \
  0x100                     /**< Used for let(): Only set qregs that already   \
                             **< exist otherwise pass them up. */
#define PE_REGS_QSTOP 0x200 /**< Q-reg get()s don't travel past this. */
#define PE_REGS_NEWATTR                                                        \
  0x400 /**< This _blocks_ iter, arg, switch, and (unless PE_REGS_ARGPASS is   \
           included) %0-%9 */
#define PE_REGS_IBREAK 0x800 /**< This pe_reg has been ibreak()'d out */
#define PE_REGS_ARGPASS                                                        \
  0x1000 /**< When used with NEWATTR, don't block args (%0-%9) */
#define PE_REGS_LOCALIZED                                                      \
  0x2000 /**< This pe_regs created due to localize() or similar */
#define PE_REGS_LOCALQ (PE_REGS_Q | PE_REGS_LOCALIZED)

/* Isolate: Don't propagate anything down, essentially wiping the slate. */
#define PE_REGS_ISOLATE (PE_REGS_QUEUE | PE_REGS_QSTOP | PE_REGS_NEWATTR)

/* Typeflags for REG_VALs */
#define PE_REGS_STR 0x100    /**< It's a string */
#define PE_REGS_INT 0x200    /**< It's an integer */
#define PE_REGS_NOCOPY 0x400 /**< Don't insert the value into a string */

/** A single value in a pe_regs structure */
typedef struct _pe_reg_val {
  int type;         /**< The type of the value */
  const char *name; /**< The register name */
  union {
    const char *sval; /**< Pointer to value for str-type registers */
    int ival;         /**< The value for int-type registers */
  } val;
  struct _pe_reg_val *next; /**< Pointer to next value */
} PE_REG_VAL;

/** pe_regs structs store environment (%0-%9), q-registers, itext(),
 * stext() and regexp ($0-$9) context, as well as a few %-sub values. */
typedef struct _pe_regs_ {
  struct _pe_regs_ *prev; /**< Previous PE_REGS, for chaining up the stack */
  int flags;              /**< REG_* flags */
  int count;              /**< Total register count. This includes
                           * inherited registers. */
  int qcount;             /**< Q-register count, including inherited
                           * registers. */
  PE_REG_VAL *vals;       /**< The register values */
  const char *name;       /**< For debugging */
} PE_REGS;

/** NEW_PE_INFO holds data about string evaluation via process_expression().  */
struct new_pe_info {
  int fun_invocations; /**< The number of functions invoked (%?) */
  int fun_recursions;  /**< Function recursion depth (%?) */
  int call_depth; /**< Number of times the parser (process_expression()) has
                     recursed */
  int nest_depth; /**< Depth of function nesting, for DEBUG */
  int debugging;  /**< Show debug? 1 = yes, 0 = if DEBUG flag set, -1 = no */
  int refcount; /**< Number of times this pe_info is being used. > 1 when shared
                   by sub-queues. free() when at 0 */
  Debug_Info *debug_strings; /**< DEBUG information */
  PE_REGS *regvals;          /**< Saved register values. */

  char *cmd_raw;    /**< Unevaluated cmd executed (%c) */
  char *cmd_evaled; /**< Evaluated cmd executed (%u) */

  char *attrname; /**< The attr currently being evaluated */
};

/** \struct mque
 ** \brief Contains data on queued action lists. Used in all queues (wait,
 **        semaphore, player, object), and for inplace queue entries.
 */
typedef struct mque MQUE;
struct mque {
  dbref executor; /**< Dbref of the executor, who is running this code (%!) */
  dbref enactor;  /**< Dbref of the enactor, who caused this code to run
                     initially (%#) */
  dbref caller;   /**< Dbref of the caller, who called/triggered this attribute
                     (%\@) */
  dbref semaphore_obj; /**< Object this queue was \@wait'd on as a semaphore */

  char
    *semaphore_attr; /**< Attribute this queue was \@wait'd on as a semaphore */

  NEW_PE_INFO *pe_info; /**< New pe_info struct used for this queue entry */

  PE_REGS *regvals; /**< Queue-specific PE_REGS for inplace queues. */

  MQUE *inplace; /**< Queue entry to run, either via \@include or \@break,
                    \@foo/inplace, etc */
  MQUE *next;    /**< The next queue entry in the linked list */

  char
    *action_list; /**< The action list of commands to run in this queue entry */
  time_t
    wait_until; /**< Time (epoch in seconds) this \@wait'd queue entry runs */
  uint32_t pid; /**< This queue's process id */

  int queue_type; /**< The type of queue entry, bitwise QUEUE_* values */
  int port; /**< The port/descriptor the command came from, or 0 for queue entry
               not from a player's client */
  char *save_attrname; /**< A saved copy of pe_info->attrname, to be reset and
                          freed at the end of the include que */
};

/* new attribute foo */
typedef struct attr ATTR;
typedef ATTR ALIST;

/** A text block
 */
struct text_block {
  int nchars;             /**< Number of characters in the block */
  struct text_block *nxt; /**< Pointer to next block in queue */
  char *start;            /**< Start of text */
  char *buf;              /**< Current position in text */
};
/** A queue of text blocks.
 */
struct text_queue {
  struct text_block *head; /**< Pointer to the head of the queue */
  struct text_block *tail; /**< Pointer to pointer to tail of the queue */
};

/* Descriptor foo */
/** Using Pueblo, Smial, Mushclient, or some other
 *  pueblo-style HTML aware client */
#define CONN_HTML 0x1
/** Using a client that understands telnet options */
#define CONN_TELNET 0x2
/** Send a telnet option to test client */
#define CONN_TELNET_QUERY 0x4
/** Connection that should be closed on load from reboot.db */
#define CONN_CLOSE_READY 0x8
/** Validated connection from an SSL concentrator */
#define CONN_SSL_CONCENTRATOR 0x10
/** Player would like to receive newlines after prompts, because
 *  their client mucks up output after a GOAHEAD */
#define CONN_PROMPT_NEWLINES 0x20
/* Client hasn't sent any data yet */
#define CONN_AWAITING_FIRST_DATA 0x40
/* Strip accents. In lieu of proper charset negotiation, this is set
 * on connections which have negotiated ASCII instead of the charset
 * the MUSH is running in
 */
#define CONN_STRIPACCENTS 0x80
/** Default connection, nothing special */
#define CONN_DEFAULT (CONN_PROMPT_NEWLINES | CONN_AWAITING_FIRST_DATA)
/** Bits reserved for the color style */
#define CONN_COLORSTYLE 0xF00
#define CONN_PLAIN 0x100
#define CONN_ANSI 0x200
#define CONN_ANSICOLOR 0x300
#define CONN_XTERM256 0x400
#define CONN_RESERVED 0x800

/** This connection is marked for closing. Still safe to write to it.
 * Close when ready. */
#define CONN_SHUTDOWN 0x1000

/** Negotiated GMCP via Telnet */
#define CONN_GMCP 0x2000

/** Sending and receiving UTF-8 */
#define CONN_UTF8 0x4000

/* Socket error, do not write to this connection anymore. */
#define CONN_NOWRITE 0x8000

/* HTTP connection, pass input straight to process_http_input */
#define CONN_HTTP_REQUEST 0x10000
/* An active HTTP command: Pemits and the like should be buffered in
 * active_http_request */
#define CONN_HTTP_BUFFER 0x20000
/* An HTTP Request that should be closed. */
#define CONN_HTTP_READY 0x40000
#define CONN_HTTP_CLOSE 0x80000

/* Flag for WebSocket client. */
#define CONN_WEBSOCKETS_REQUEST 0x10000000
#define CONN_WEBSOCKETS 0x20000000

/** Maximum \@doing length */
#define DOING_LEN 40

/** Pueblo checksum length.
 * Pueblo uses md5 now, but if they switch to sha1, this will still
 * be safe.
 */
#define PUEBLO_CHECKSUM_LEN 40

typedef enum conn_source {
  CS_IP_SOCKET,
  CS_OPENSSL_SOCKET,
  CS_LOCAL_SOCKET,
  CS_LOCAL_SSL_SOCKET,
  CS_UNKNOWN
} conn_source;

typedef enum conn_status {
  CONN_SCREEN, /* not connected to a player */
  CONN_PLAYER, /* connected */
  CONN_DENIED  /* connection denied due to login limits/sitelock */
} conn_status;

typedef bool (*sq_func)(void *);
/** System queue event */
struct squeue {
  sq_func fun;         /** Function to run */
  void *data;          /** Data to pass to function, or NULL */
  uint64_t when;       /** When to run the function, in milliseconds. */
  char *event;         /** Softcode Event name to trigger, or NULL if none */
  struct squeue *next; /** Pointer to next squeue event in linked list */
};

/**< Have we used too much CPU? */
extern volatile sig_atomic_t cpu_time_limit_hit;

#define HTTP_METHOD_LEN 16
#define HTTP_CODE_LEN 64

struct http_request {
  char method[HTTP_METHOD_LEN]; /**< GET/POST/PUT/DELETE/HEAD/etc */
  char path[MAX_COMMAND_LEN];   /**< Varies by browser, but 2048 is IE max */
  char inheaders[BUFFER_LEN];   /**< Incoming Headers */
  char *inhp;                   /**< bp for hbuff */
  char inbody[BUFFER_LEN];      /**< Incoming Body */
  char *inbp;                   /**< bp for buff */
  uint32_t state;               /**< Current state of request. */
  int32_t content_length;       /**< Content-Length value. */
  int32_t content_read;         /**< Content-Length value. */

  char code[HTTP_CODE_LEN];    /**< 200 OK, etc */
  char ctype[MAX_COMMAND_LEN]; /**< Content-Type: text/plain */
  char headers[BUFFER_LEN];    /**< Response headers */
  char *hp;                    /**< ptr for headers */
  char response[BUFFER_LEN];   /**< Response body. @pemits, etc. */
  char *rp;                    /**< bp for response */
};

typedef struct descriptor_data DESC;
/** A player descriptor's data.
 * This structure associates a connection's socket (file descriptor)
 * with a lot of other relevant information.
 */

struct descriptor_data {
  int descriptor;            /**< Connection socket (fd) */
  conn_status connected;     /**< Connection status. */
  struct squeue *conn_timer; /**< Timer event used during initial connection */
  char addr[101];            /**< Hostname of connection source */
  char ip[101];              /**< IP address of connection source */
  dbref player; /**< Dbref of player associated with connection, or NOTHING if
                   not connected */
  int output_size;          /**< Size of output left to send */
  char *output_prefix;      /**< Text to show before output */
  char *output_suffix;      /**< Text to show after output */
  struct text_queue output; /**< Output text queue */
  struct text_queue input;  /**< Input text queue */
  char *raw_input;          /**< Pointer to start of next raw input */
  char *raw_input_at;       /**< Pointer to position in raw input */
  time_t connected_at;      /**< Time of connection */
  time_t last_time;         /**< Time of last activity */
  uint32_t quota; /**< Quota of commands allowed, *1000 (for milliseconds) */
  int cmds;       /**< Number of commands sent */
  int hide;       /**< Hide status */
  uint32_t conn_flags; /**< Flags of connection (telnet status, etc.) */
  struct descriptor_data *next; /**< Next descriptor in linked list */
  unsigned long input_chars;    /**< Characters received */
  unsigned long output_chars;   /**< Characters sent */
  int width;                    /**< Screen width */
  int height;                   /**< Screen height */
  char *ttype;                  /**< Terminal type */
  SSL *ssl;                     /**< SSL object */
  int ssl_state;                /**< Keep track of state of SSL object */
  conn_source source;           /**< Where the connection came from. */
  char checksum[PUEBLO_CHECKSUM_LEN + 1]; /**< Pueblo checksum */
  uint64_t ws_frame_len;
  int64_t connlog_id;       /**< ID for this connection's connlog entry */
  const char *close_reason; /**< Why is this socket being closed? */
  dbref closer;             /**< Who closed this socket? */
  struct http_request *http_request;
};

enum json_type {
  JSON_NONE = 0,
  JSON_NUMBER,
  JSON_STR,
  JSON_BOOL,
  JSON_NULL,
  JSON_ARRAY,
  JSON_OBJECT
};

typedef int (*gmcp_handler_func)(char *package, cJSON *data, char *msg,
                                 DESC *d);

#define GMCP_HANDLER(x)                                                        \
  int x(char *package __attribute__((__unused__)),                             \
        cJSON *json __attribute__((__unused__)),                               \
        char *msg __attribute__((__unused__)),                                 \
        DESC *d __attribute__((__unused__)));                                  \
  int x(char *package __attribute__((__unused__)),                             \
        cJSON *json __attribute__((__unused__)),                               \
        char *msg __attribute__((__unused__)),                                 \
        DESC *d __attribute__((__unused__)))

struct gmcp_handler {
  char *package; /* The name of the GMCP package this handler can handle, or the
                    empty string to use as a default handler for all packages */
  gmcp_handler_func func; /* The function for this handler */
  struct gmcp_handler
    *next; /* A pointer to the next handler in the linked list */
};

#endif
