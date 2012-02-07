/**
 * \file mushtype.h
 *
 * \brief Several commonly-used structs, \#defines, and other stuff
 */

#ifndef MUSH_TYPES_H
#define MUSH_TYPES_H
#include "copyrite.h"
#include <openssl/ssl.h>
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

typedef uint8_t *object_flag_type;

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

#define PE_KEY_LEN     64       /**< The maximum key length. */

/* Types for _pe_regs_ _and_ _pe_reg_vals_ */
#define PE_REGS_Q      0x01     /**< Q-registers. */
#define PE_REGS_REGEXP 0x02     /**< Regexps. */
#define PE_REGS_CAPTURE PE_REGS_REGEXP  /**< Alias for REGEXP */
#define PE_REGS_SWITCH 0x04     /**< switch(), %$0. */
#define PE_REGS_ITER   0x08     /**< iter() and @dol, %i0/etc */
#define PE_REGS_ARG    0x10     /**< %0-%9 */
#define PE_REGS_SYS    0x20     /**< %c, %z, %= */

#define PE_REGS_TYPE   0xFF     /**< The type mask, everything over is flags. */
#define PE_REGS_QUEUE  0xFF     /**< Every type for a queue. */

/* Flags for _pe_regs_: */
#define PE_REGS_LET     0x100   /**< Used for let(): Only set qregs that already
                                 **< exist otherwise pass them up. */
#define PE_REGS_QSTOP   0x200   /**< Q-reg get()s don't travel past this. */
#define PE_REGS_NEWATTR 0x400   /**< This _blocks_ iter, arg, switch */
#define PE_REGS_IBREAK  0x800   /**< This pe_reg has been ibreak()'d out */
#define PE_REGS_ARGPASS 0x1000  /**< This pe_reg has been ibreak()'d out */

/* Isolate: Don't propagate anything down, essentially wiping the slate. */
#define PE_REGS_ISOLATE (PE_REGS_QUEUE | PE_REGS_QSTOP | PE_REGS_NEWATTR)

/* Typeflags for REG_VALs */
#define PE_REGS_STR    0x100    /**< It's a string */
#define PE_REGS_INT    0x200    /**< It's an integer */
#define PE_REGS_NOCOPY 0x400    /**< Don't insert the value into a string */

/** A single value in a pe_regs structure */
typedef struct _pe_reg_val {
  int type;                     /**< The type of the value */
  const char *name;             /**< The register name */
  union {
    const char *sval;           /**< Pointer to value for str-type registers */
    int ival;                   /**< The value for int-type registers */
  } val;
  struct _pe_reg_val *next;     /**< Pointer to next value */
} PE_REG_VAL;

/** pe_regs structs store environment (%0-%9), q-registers, itext(),
 * stext() and regexp ($0-$9) context, as well as a few %-sub values. */
typedef struct _pe_regs_ {
  struct _pe_regs_ *prev;       /**< Previous PE_REGS, for chaining up the stack */
  int flags;                    /**< REG_* flags */
  int count;                    /**< Total register count. This includes
                                 * inherited registers. */
  int qcount;                   /**< Q-register count, including inherited
                                 * registers. */
  PE_REG_VAL *vals;             /**< The register values */
  const char *name;             /**< For debugging */
} PE_REGS;

/* Initialize the pe_regs strtrees */
void init_pe_regs_trees();
void free_pe_regs_trees();

/* Functions used to create new pe_reg stacks */
void pe_regs_dump(PE_REGS *pe_regs, dbref who);
PE_REGS *pe_regs_create_real(int pr_flags, const char *name);
#define pe_regs_create(x,y) pe_regs_create_real(x, "pe_regs-" y)
void pe_reg_val_free(PE_REG_VAL *val);
void pe_regs_clear(PE_REGS *pe_regs);
void pe_regs_clear_type(PE_REGS *pe_regs, int type);
void pe_regs_free(PE_REGS *pe_regs);
PE_REGS *pe_regs_localize_real(NEW_PE_INFO *pe_info, uint32_t pr_flags,
                               const char *name);
#define pe_regs_localize(p,x,y) pe_regs_localize_real(p, x, "pe_regs-" y)
void pe_regs_restore(NEW_PE_INFO *pe_info, PE_REGS *pe_regs);

/* Copy a stack of PE_REGS into a new one: For creating new queue entries.
 * This squashes all values in pe_regs to a single PE_REGS. The returned
 * pe_regs type has PE_REGS_QUEUE. */
void pe_regs_copystack(PE_REGS *new_regs, PE_REGS *pe_regs,
                       int copytypes, int override);

/* Manipulating PE_REGS directly */
void pe_regs_set_if(PE_REGS *pe_regs, int type,
                    const char *key, const char *val, int override);
#define pe_regs_set(p,t,k,v) pe_regs_set_if(p,t,k,v,1)
void pe_regs_set_int_if(PE_REGS *pe_regs, int type,
                        const char *key, int val, int override);
#define pe_regs_set_int(p,t,k,v) pe_regs_set_int_if(p,t,k,v,1)
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
/* Copy all Q registers from src to dst PE_REGS. */
void pe_regs_qcopy(PE_REGS *dst, PE_REGS *src);

/* PE_REGS_REGEXP */
struct real_pcre;
struct _ansi_string;
void pe_regs_set_rx_context(PE_REGS *regs,
                            struct real_pcre *re_code,
                            int *re_offsets,
                            int re_subpatterns, const char *re_from);
void pe_regs_set_rx_context_ansi(PE_REGS *regs,
                                 struct real_pcre *re_code,
                                 int *re_offsets,
                                 int re_subpatterns,
                                 struct _ansi_string *re_from);
const char *pi_regs_get_rx(NEW_PE_INFO *pe_info, const char *key);
#define PE_Get_re(pi,k) pi_regs_get_rx(pi,k)

/* PE_REGS_SWITCH and PE_REGS_ITER
 *
 * Here is how SWITCH and ITER fetching works.
 *
 * + Only the topmost PE_REGS (the one associated with the pe_info directly)
 *   will ever have more than one switch or iter value.
 * + If a non-top PE_REGS_ITER is encountered, it is considered to have
 *   1 itext/stext
 * + ilev is caculated by counting the number of PE_REGS_ITER up to the top.
 *   Topmost queue entries will have an int "ilev" set with the appropriate
 *   PE_REGS_foo type.
 * + inum is saved as "n%d", itext as "t%d"
 * + Each non-top level saves as "i0" or "n0" for itext and inum,
 *    respectively.
 * + Copystack will rebuild the itext(0)-itext(MAX_ITERS) count, increasing
 *   them as needed.
 * + Switches are just iters without inums, so they're functionally the same.
 *   The only difference from above is the type of the value.
 */
const char *pi_regs_get_itext(NEW_PE_INFO *pe_info, int type, int lev);
int pi_regs_get_ilev(NEW_PE_INFO *pe_info, int type);
int pi_regs_get_inum(NEW_PE_INFO *pe_info, int type, int lev);

/* Get iter info */
#define PE_Get_Itext(pi,k) pi_regs_get_itext(pi, PE_REGS_ITER, k)
#define PE_Get_Ilev(pi) pi_regs_get_ilev(pi, PE_REGS_ITER)
#define PE_Get_Inum(pi,k) pi_regs_get_inum(pi, PE_REGS_ITER, k)
/* Get switch info */
#define PE_Get_Stext(pi,k) pi_regs_get_itext(pi, PE_REGS_SWITCH, k)
#define PE_Get_Slev(pi) pi_regs_get_ilev(pi, PE_REGS_SWITCH)

/* Get env (%0-%9) info */

const char *pe_regs_intname(int num);
void pe_regs_setenv(PE_REGS *pe_regs, int num, const char *val);
void pe_regs_setenv_nocopy(PE_REGS *pe_regs, int num, const char *val);
const char *pi_regs_get_env(NEW_PE_INFO *pe_info, int num);
int pi_regs_get_envc(NEW_PE_INFO *pe_info);
#define PE_Get_Env(pi,n) pi_regs_get_env(pi, n)
#define PE_Get_Envc(pi) pi_regs_get_envc(pi)

/** NEW_PE_INFO holds data about string evaluation via process_expression().  */
struct new_pe_info {
  int fun_invocations;          /**< The number of functions invoked (%?) */
  int fun_recursions;           /**< Function recursion depth (%?) */
  int call_depth;               /**< Number of times the parser (process_expression()) has recursed */

  Debug_Info *debug_strings;    /**< DEBUG information */
  int nest_depth;               /**< Depth of function nesting, for DEBUG */
  int debugging;                /**< Show debug? 1 = yes, 0 = if DEBUG flag set, -1 = no */

  PE_REGS *regvals;             /**< Saved register values. */

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

  PE_REGS *regvals;             /**< Queue-specific PE_REGS for inplace queues. */

  MQUE *inplace;                /**< Queue entry to run, either via \@include or \@break, \@foo/inplace, etc */
  MQUE *next;                   /**< The next queue entry in the linked list */

  dbref semaphore_obj;          /**< Object this queue was \@wait'd on as a semaphore */
  char *semaphore_attr;         /**< Attribute this queue was \@wait'd on as a semaphore */
  time_t wait_until;            /**< Time (epoch in seconds) this \@wait'd queue entry runs */
  uint32_t pid;                 /**< This queue's process id */
  char *action_list;            /**< The action list of commands to run in this queue entry */
  int queue_type;               /**< The type of queue entry, bitwise QUEUE_* values */
  int port;                     /**< The port/descriptor the command came from, or 0 for queue entry not from a player's client */
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
  struct text_block *tail;      /**< Pointer to pointer to tail of the queue */
};

/* Descriptor foo */
/** Using Pueblo, Smial, Mushclient, Simplemu, or some other
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
/** Default connection, nothing special */
#define CONN_DEFAULT (CONN_PROMPT_NEWLINES | CONN_AWAITING_FIRST_DATA)

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
  CS_UNKNOWN
} conn_source;

typedef enum conn_status {
  CONN_SCREEN,                  /* not connected to a player */
  CONN_PLAYER,                  /* connected */
  CONN_DENIED                   /* connection denied due to login limits/sitelock */
} conn_status;

typedef bool (*sq_func) (void *);
struct squeue {
  sq_func fun;
  void *data;
  time_t when;
  char *event;
  struct squeue *next;
};


typedef struct descriptor_data DESC;
/** A player descriptor's data.
 * This structure associates a connection's socket (file descriptor)
 * with a lot of other relevant information.
 */
struct descriptor_data {
  int descriptor;       /**< Connection socket (fd) */
  conn_status connected; /**< Connection status. */
  struct squeue *conn_timer; /**< Timer event used during initial connection */
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
  struct descriptor_data *next; /**< Next descriptor in linked list */
  struct descriptor_data *prev; /**< Previous descriptor in linked list */
  int conn_flags;       /**< Flags of connection (telnet status, etc.) */
  unsigned long input_chars;    /**< Characters received */
  unsigned long output_chars;   /**< Characters sent */
  int width;                    /**< Screen width */
  int height;                   /**< Screen height */
  char *ttype;                  /**< Terminal type */
  SSL *ssl;                     /**< SSL object */
  int ssl_state;                /**< Keep track of state of SSL object */
  conn_source source;           /**< Where the connection came from. */
  char checksum[PUEBLO_CHECKSUM_LEN + 1];       /**< Pueblo checksum */
};


/* Channel stuff */
typedef struct chanuser CHANUSER;
typedef struct chanlist CHANLIST;
typedef struct channel CHAN;

#endif
