#ifndef MUSH_TYPES_H
#define MUSH_TYPES_H
#include "copyrite.h"
#ifdef HAS_OPENSSL
#include <openssl/ssl.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#define NUMQ    36


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
#define NOTHING (-1)            /**< null dbref */
#define AMBIGUOUS (-2)          /**< multiple possibilities, for matchers */
#define HOME (-3)               /**< virtual room, represents mover's home */
#define ANY_OWNER (-2)          /**< For lstats and \@stat */


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

/* max length of command argument to process_command */
#define MAX_COMMAND_LEN 4096
#define BUFFER_LEN ((MAX_COMMAND_LEN)*2)
#define MAX_ARG 63

typedef struct new_pe_info NEW_PE_INFO;
typedef struct debug_info Debug_Info;
/** process_expression() info */

#define PE_KEY_LEN     64  /* The maximum key length. */

/* Types for _pe_regs_ _and_ _pe_reg_vals_ */
#define PE_REGS_Q      0x01 /* Q-registers. */
#define PE_REGS_REGEXP 0x02 /* Regexps. */
#define PE_REGS_SWITCH 0x04 /* switch(), %$0. */
#define PE_REGS_ITER   0x08 /* iter() and @dol, %i0/etc */
#define PE_REGS_ARG    0x10 /* %0-%9 */

#define PE_REGS_TYPE   0xFF /* type & PE_REGS_TYPE always gets one type */

#define PE_REGS_QUEUE  0xFF /* Every item. */

/* Flags for _pe_regs_: */
#define PE_REGS_LET     0x100 /* Used for let(): Only set qregs that already
                               * exist otherwise pass them up. */
#define PE_REGS_QSTOP   0x200 /* Q-reg get()s don't travel past this.
                              * exist otherwise pass them up. */
#define PE_REGS_NEWATTR 0x400 /* This _blocks_ iter, arg, switch */

/* Typeflags for REG_VALs */
#define PE_REGS_STR    0x100 /* It's a string */
#define PE_REGS_INT    0x200 /* It's an integer */

typedef struct _pe_reg_val {
  int  type;
  const char *name;
  union {
    const char *sval;
    int   ival;
  } val;
  struct _pe_reg_val *next;
} PE_REG_VAL;

typedef struct _pe_regs_ {
  struct   _pe_regs_ *prev;    /* Previous PE_REGS, for chaining up the stack */
  uint32_t flags;              /* REG_* flags */
  int      count;              /* Total register count. This includes
                                * inherited registers. */
  int      qcount;             /* Q-register count, including inherited
                                * registers. */
  PE_REG_VAL *vals;            /* The register values */
} PE_REGS;

/* Initialize the pe_regs strtrees */
void init_pe_regs_trees();

/* Functions used to create new pe_reg stacks */
PE_REGS *pe_regs_create(uint32_t pr_flags);
void pe_regs_clear(PE_REGS *pe_regs);
void pe_regs_free(PE_REGS *pe_regs);
PE_REGS *pe_regs_localize(NEW_PE_INFO *pe_info, uint32_t pr_flags);
void pe_regs_restore(NEW_PE_INFO *pe_info, PE_REGS *pe_regs);

/* Copy all values from src to dst PE_REGS. */
void pe_regs_copyto(PE_REGS *dst, PE_REGS *src);

/* Copy a stack of PE_REGS into a new one: For creating new queue entries.
 * This squashes all values in pe_regs to a single PE_REGS. The returned
 * pe_regs type has PE_REGS_QUEUE. */
PE_REGS *pe_regs_copystack(PE_REGS *pe_regs);

/* Manipulating PE_REGS directly */
void pe_regs_set(PE_REGS *pe_regs, int type,
                 const char *key, const char *val);
void pe_regs_set_int(PE_REGS *pe_regs, int type, const char *key, int val);
const char *pe_regs_get(PE_REGS *pe_regs, int type, const char *key);
int pe_regs_get_int(PE_REGS *pe_regs, int type, const char *key);

/* Helper functions: Mostly used in process_expression, r(), itext(), etc */
int pi_regs_has_type(NEW_PE_INFO *pe_info, int type);
#define PE_HAS_REGTYPE(p,t) pi_regs_has_type(p,t)

/* PE_REGS_Q */
int pi_regs_valid_key(const char *key);
#define ValidQregName(x) pi_regs_valid_key(x)
int pi_regs_setq(NEW_PE_INFO *pe_info, const char *key, const char *val);
#define PE_Setq(pi,k,v) pi_regs_setq(pi,k,v)
const char *pi_regs_getq(NEW_PE_INFO *pe_info, const char *key);
#define PE_Getq(pi,k) pi_regs_getq(pi,k)

/* PE_REGS_REGEXP */
struct real_pcre;
struct _ansi_string;
void pe_regs_set_rx_context(PE_REGS *regs,
                            struct real_pcre *re_code,
                            int *re_offsets,
                            int re_subpatterns,
                            const char *re_from);
void pe_regs_set_rx_context_ansi(PE_REGS *regs,
                                 struct real_pcre *re_code,
                                 int *re_offsets,
                                 int re_subpatterns,
                                 struct _ansi_string *re_from);
const char *pi_regs_get_rx(NEW_PE_INFO *pe_info, const char *key);
#define PE_Get_re(pi,k) pi_regs_get_rx(pi,k)

/* NEW_PE_INFO: May need more documentation.  */
struct new_pe_info {
  int fun_invocations;          /**< The number of functions invoked (%?) */
  int fun_recursions;           /**< Function recursion depth (%?) */
  int call_depth;               /**< Number of times the parser (process_expression()) has recursed */

  Debug_Info *debug_strings;    /**< DEBUG information */
  int nest_depth;               /**< Depth of function nesting, for DEBUG */
  int debugging;                /**< Show debug? 1 = yes, 0 = if DEBUG flag set, -1 = no */

  PE_REGS *regvals;      /**< Saved register values. */
  char *env[10];                /**< Current environment variables (%0-%9) */
  int arg_count;                /**< Number of arguments available in env (%+) */

  int iter_nestings;            /**< Total number of iter()/\@dolist nestings */
  int iter_nestings_local;      /**< Number of iter() nestings accessible at present */
  int iter_breaks;              /**< Number of iter()s to break out of */
  int iter_dolists;             /**< Number of iter_nestings which are from \@dolist, and can't be broken out of */
  char *iter_itext[MAX_ITERS];  /**< itext() values */
  int iter_inum[MAX_ITERS];     /**< inum() values */

  int switch_nestings;          /**< Total number of switch()/\@switch nestings */
  int switch_nestings_local;    /**< Number of switch nestings available at present */
  char *switch_text[MAX_ITERS]; /**< stext() values */

  char cmd_raw[BUFFER_LEN];     /**< Unevaluated cmd executed (%c) */
  char cmd_evaled[BUFFER_LEN];  /**< Evaluated cmd executed (%u) */

  char attrname[BUFFER_LEN];    /**< The attr currently being evaluated */

  char name[BUFFER_LEN];        /**< TEMP: Used for memory-leak checking. Remove me later!!!! */

  int refcount;                 /**< Number of times this pe_info is being used. > 1 when shared by sub-queues. free() when at 0 */
};


/** \struct mque
 ** \brief Contains data on queued action lists. Used in all queues (wait,
 **        semaphore, player, object), and for inplace queue entries.
 */
typedef struct mque MQUE;
struct mque {
  dbref executor;               /**< Dbref of the executor, who is running this code (%!) */
  dbref enactor;                /**< Dbref of the enactor, who caused this code to run initially (%#) */
  dbref caller;                 /**< Dbref of the caller, who called/triggered this attribute (%\@) */

  NEW_PE_INFO *pe_info;         /**< New pe_info struct used for this queue entry */

  MQUE *inplace;                /**< Queue entry to run, either via \@include or \@break, \@foo/inplace, etc */
  MQUE *next;                   /**< The next queue entry in the linked list */

  dbref semaphore_obj;          /**< Object this queue was \@wait'd on as a semaphore */
  char *semaphore_attr;         /**< Attribute this queue was \@wait'd on as a semaphore */
  time_t wait_until;            /**< Time (epoch in seconds) this \@wait'd queue entry runs */
  uint32_t pid;                 /**< This queue's process id */
  char *action_list;            /**< The action list of commands to run in this queue entry */
  int queue_type;               /**< The type of queue entry, bitwise QUEUE_* values */
  int port;                     /**< The port/descriptor the command came from, or 0 for queue entry not from a player's client */
  char *save_env[10];           /**< If queue_type contains (QUEUE_INPLACE | QUEUE_RESTORE_ENV), at the end of this inplace queue
                                     entry, free the stack (pe_info->env) and restore it to these values */
  char *save_attrname;          /**< A saved copy of pe_info->attrname, to be reset and freed at the end of the include que */

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


/* Channel stuff */
typedef struct chanuser CHANUSER;
typedef struct chanlist CHANLIST;
typedef struct channel CHAN;

#endif
