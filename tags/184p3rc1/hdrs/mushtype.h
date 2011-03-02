#ifndef MUSH_TYPES_H
#define MUSH_TYPES_H
#include "copyrite.h"
#ifdef HAS_OPENSSL
#include <openssl/ssl.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/** Math function floating-point number type */
typedef double NVAL;

/* Math function integral type */
typedef int32_t IVAL;

/** Math function unsigned integral type */
typedef uint32_t UIVAL;

#define SIZEOF_IVAL 4

/** Dbref type */
typedef int dbref;

/** The type that stores the warning bitmask */
typedef uint32_t warn_type;

/** Attribute/lock flag types */
typedef uint32_t privbits;

/* special dbref's */
#define NOTHING (-1)            /* null dbref */
#define AMBIGUOUS (-2)          /* multiple possibilities, for matchers */
#define HOME (-3)               /* virtual room, represents mover's home */
#define ANY_OWNER (-2)          /* For lstats and @stat */


#define INTERACT_SEE 0x1
#define INTERACT_HEAR 0x2
#define INTERACT_MATCH 0x4
#define INTERACT_PRESENCE 0x8

typedef unsigned char *object_flag_type;

/* Boolexps and locks */
typedef const char *lock_type;
typedef struct lock_list lock_list;


/* Set this somewhere near the recursion limit */
#define MAX_ITERS 100

typedef struct pe_info PE_Info;
typedef struct debug_info Debug_Info;
/** process_expression() info
 * This type is used by process_expression().  In all but parse.c,
 * this should be left as an incompletely-specified type, making it
 * impossible to declare anything but pointers to it.
 *
 * Unfortunately, we need to know what it is in funlist.c, too,
 * to prevent denial-of-service attacks.  ARGH!  Don't look at
 * this struct unless you _really_ want to get your hands dirty.
 */
struct pe_info {
  int fun_invocations;          /**< Invocation count */
  int fun_depth;                /**< Recursion count */
  int nest_depth;               /**< Depth of function nesting, for DEBUG */
  int call_depth;               /**< Function call counter */
  Debug_Info *debug_strings;    /**< DEBUG infromation */
  int arg_count;                /**< Number of arguments passed to function */
  int iter_nesting;             /**< Current iter() nesting depth */
  int local_iter_nesting;       /**< Expression-level iter() nesting depth */
  char *iter_itext[MAX_ITERS];  /**< itext() replacements in iter() */
  int iter_inum[MAX_ITERS];     /**< inum() values in iter() */
  int iter_break;               /**< number of ibreak()s to break out of iter()s */
  int dolists;                  /**< Number of @dolist values in iter_itext */
  int switch_nesting;           /**< switch()/@switch nesting depth */
  int local_switch_nesting;     /**< Expression-level switch nesting depth */
  char *switch_text[MAX_ITERS]; /**< #$-values for switch()/@switches */
  int debugging;                /**< Show debug? 1 = yes, 0 = if DEBUG flag set, -1 = no */
};


/* new attribute foo */
typedef struct attr ATTR;
typedef ATTR ALIST;

/** A text block
 */
struct text_block {
  int nchars;                   /**< Number of characters in the block */
  struct text_block *nxt;       /**< Pointer to next block in queue */
  unsigned char *start;         /**< Start of text */
  unsigned char *buf;           /**< Current position in text */
};
/** A queue of text blocks.
 */
struct text_queue {
  struct text_block *head;      /**< Pointer to the head of the queue */
  struct text_block **tail;     /**< Pointer to pointer to tail of the queue */
};

/* Descriptor foo */
/** Using Pueblo, Smial, Mushclient, Simplemu, or some other
 *  *  pueblo-style HTML aware client */
#define CONN_HTML 0x1
/** Using a client that understands telnet options */
#define CONN_TELNET 0x2
/** Send a telnet option to test client */
#define CONN_TELNET_QUERY 0x4
/** Connection that should be close on load from reboot.db */
#define CONN_CLOSE_READY 0x8
/** Validated connection from an SSL concentrator */
#define CONN_SSL_CONCENTRATOR 0x10
/** Player would like to receive newlines after prompts, because
 *  * their client mucks up output after a GOAHEAD */
#define CONN_PROMPT_NEWLINES 0x20
/** Default connection, nothing special */
#define CONN_DEFAULT (CONN_PROMPT_NEWLINES)

#define DOING_LEN 40
/** Pueblo checksum length.
 * Pueblo uses md5 now, but if they switch to sha1, this will still
 * be safe.
 */
#define PUEBLO_CHECKSUM_LEN 40
typedef struct descriptor_data DESC;
/** A player descriptor's data.
 * This structure associates a connection's socket (file descriptor)
 * with a lot of other relevant information.
 */
struct descriptor_data {
  int descriptor;       /**< Connection socket (fd) */
  int connected;        /**< Connection status. 0 = not connected to a player, 1 = connected, 2 = connection denied due to login limits/sitelock */
  char addr[101];       /**< Hostname of connection source */
  char ip[101];         /**< IP address of connection source */
  dbref player;         /**< Dbref of player associated with connection, or NOTHING if not connected */
  unsigned char *output_prefix; /**< Text to show before output */
  unsigned char *output_suffix; /**< Text to show after output */
  int output_size;              /**< Size of output left to send */
  struct text_queue output;     /**< Output text queue */
  struct text_queue input;      /**< Input text queue */
  unsigned char *raw_input;     /**< Pointer to start of next raw input */
  unsigned char *raw_input_at;  /**< Pointer to position in raw input */
  time_t connected_at;    /**< Time of connection */
  time_t last_time;       /**< Time of last activity */
  int quota;            /**< Quota of commands allowed */
  int cmds;             /**< Number of commands sent */
  int hide;             /**< Hide status */
  char doing[DOING_LEN];        /**< Player's doing string */
  struct descriptor_data *next; /**< Next descriptor in linked list */
  struct descriptor_data *prev; /**< Previous descriptor in linked list */
  int conn_flags;       /**< Flags of connection (telnet status, etc.) */
  unsigned long input_chars;    /**< Characters received */
  unsigned long output_chars;   /**< Characters sent */
  int width;                    /**< Screen width */
  int height;                   /**< Screen height */
  char *ttype;                  /**< Terminal type */
#ifdef HAS_OPENSSL
  SSL *ssl;                     /**< SSL object */
  int ssl_state;                /**< Keep track of state of SSL object */
#endif
  char checksum[PUEBLO_CHECKSUM_LEN + 1];       /**< Pueblo checksum */
};

/* max length of command argument to process_command */
#define MAX_COMMAND_LEN 4096
#define BUFFER_LEN ((MAX_COMMAND_LEN)*2)
#define MAX_ARG 63

/* Channel stuff */
typedef struct chanuser CHANUSER;
typedef struct chanlist CHANLIST;
typedef struct channel CHAN;

#endif
