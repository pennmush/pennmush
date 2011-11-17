/**
 * \file command.h
 *
 * \brief Command-related prototypes and constants.
 */

#ifndef __COMMAND_H
#define __COMMAND_H

#include "boolexp.h"

typedef uint8_t *switch_mask;
extern int switch_bytes;
#define SW_ALLOC()      mush_calloc(switch_bytes, 1, "cmd.switch.vector")
#define SW_FREE(s)      mush_free((s), "cmd.switch.vector")
#define SW_SET(m,n)     (m[(n) >> 3] |= (1 << ((n) & 0x7)))
#define SW_CLR(m,n)     (m[(n) >> 3] &= ~(1 << ((n) & 0x7)))
#define SW_ISSET(m,n)   (m[(n) >> 3] & (1 << ((n) & 0x7)))
bool SW_BY_NAME(switch_mask, const char *);
#define SW_ZERO(m)      memset(m, 0, switch_bytes)
#define SW_COPY(new,old) memcpy((new), (old), switch_bytes)

/* These are type restrictors */
#define CMD_T_ROOM       0x80000000  /**< Command can be used by rooms */
#define CMD_T_THING      0x40000000  /**< Command can be used by things */
#define CMD_T_EXIT       0x20000000  /**< Command can be used by exits */
#define CMD_T_PLAYER     0x10000000  /**< Command can be used by players */
#define CMD_T_ANY        0xF0000000  /**< Command can be used by any type of object */
#define CMD_T_GOD        0x08000000  /**< Command can only be used by God */

#define CMD_T_SWITCHES   0x02000000  /**< Any unknown or undefined switches will be passed in switches, instead of causing error */

#define CMD_T_DISABLED   0x01000000  /**< Command is disabled, set with \@command */

#define CMD_T_NOGAGGED   0x00800000  /**< Command will fail if object is gagged */

#define CMD_T_NOGUEST    0x00400000  /**< Command will fail if object is a guest */

#define CMD_T_NOFIXED    0x00200000  /**< Command will fail if object is fixed */

#define CMD_T_LISTED     0x00080000  /**< INTERNAL : Command is listed in \@list commands. NO LONGER USED */

#define CMD_T_INTERNAL   0x00040000  /**< INTERNAL : Command is an internal command, and shouldn't be matched or aliased */

#define CMD_T_LOGNAME    0x00020000  /**< Log when the command is used */
#define CMD_T_LOGARGS    0x00010000  /**< Log when the command is used, and the args given */

#define CMD_T_EQSPLIT    0x00000001  /**< Split arguments at =, but don't abort if there's no = */

#define CMD_T_ARGS       0x00000010  /**< Split into argv[] at ,s */

#define CMD_T_ARG_SPACE  0x00000020  /**< Split at spaces instead of commas. CMD_T_ARGS MUST also be defined */

#define CMD_T_NOPARSE    0x00000040  /**< Do NOT parse arguments */

#define CMD_T_RS_BRACE   0x00000080  /**< Strip outer {} from the RHS arg, even if we normally wouldn't */

#define CMD_T_LS_ARGS    CMD_T_ARGS
#define CMD_T_LS_SPACE   CMD_T_ARG_SPACE
#define CMD_T_LS_NOPARSE CMD_T_NOPARSE
#define CMD_T_RS_ARGS    0x00000100     /*CMD_T_ARGS<<4 */
#define CMD_T_RS_SPACE   0x00000200     /*CMD_T_ARG_SPACE<<4 */
#define CMD_T_RS_NOPARSE 0x00000400     /*CMD_T_NOPARSE<<4 */
#define CMD_T_DEPRECATED 0x00000800
#define CMD_T_NOP        0x00001000  /**< A no-op command that exists for @hooks */

/** COMMAND prototype.
 * \verbatim
   Passed arguments:
   executor : Object issuing command.
   sw : switch_mask, check with the SW_ macros.
   raw : *FULL* unparsed, untouched command.
   switches : Any unhandled switches, or NULL if none.
   args_raw : Full argument, untouched. null-string if none.
   arg_left : Left-side arguments, unparsed if CMD_T_NOPARSE.
   args_left : Parsed arguments, if CMD_T_ARGS is defined.
   args_right : Parsed right-side arguments, if CMD_T_RSARGS is defined.

   Note that if you don't specify EQSPLIT, left is still the data you want. If you define EQSPLIT,
   there are also right_XX values.

   Special case:
   If the NOEVAL switch is given, AND EQSPLIT is defined, the right-side will not be parsed.
   If NOEVAL is givean the EQSPLIT isn't defined, the left-side won't be parsed.
 * \endverbatim
 */

#define COMMAND(command_name) \
void command_name (COMMAND_INFO *cmd, dbref executor, dbref enactor, dbref caller, \
 switch_mask sw, const char *raw, const char *switches, char *args_raw, \
                  char *arg_left, char *args_left[MAX_ARG], \
                  char *arg_right, char *args_right[MAX_ARG], MQUE *queue_entry __attribute__ ((__unused__))); \
void command_name(COMMAND_INFO *cmd __attribute__ ((__unused__)), \
                  dbref executor __attribute__ ((__unused__)), \
                  dbref enactor __attribute__ ((__unused__)), \
                  dbref caller __attribute__ ((__unused__)), \
                  switch_mask sw __attribute__ ((__unused__)), \
                  const char *raw __attribute__ ((__unused__)), \
                  const char *switches __attribute__ ((__unused__)), \
                  char *args_raw __attribute__ ((__unused__)), \
                  char *arg_left __attribute__ ((__unused__)), \
                  char *args_left[MAX_ARG] __attribute__ ((__unused__)), \
                  char *arg_right __attribute__ ((__unused__)), \
                  char *args_right[MAX_ARG] __attribute__ ((__unused__)), \
                  MQUE *queue_entry __attribute__ ((__unused__)))

/** Common command prototype macro */
#define COMMAND_PROTO(command_name) \
void command_name (COMMAND_INFO *cmd, dbref player, dbref cause, dbref caller, switch_mask sw, const char *raw, const char *switches, char *args_raw, \
                  char *arg_left, char *args_left[MAX_ARG], \
                  char *arg_right, char *args_right[MAX_ARG], MQUE *queue_entry)

typedef struct command_info COMMAND_INFO;
typedef void (*command_func) (COMMAND_INFO *, dbref, dbref, dbref, switch_mask,
                              const char *, const char *, char *, char *,
                              char *[MAX_ARG], char *, char *[MAX_ARG], MQUE *);

/** A hook specification.
 */
struct hook_data {
  dbref obj;            /**< Object where the hook attribute is stored. */
  char *attrname;       /**< Attribute name of the hook attribute */
  int inplace;          /**< Valid only for override: Run hook in place. */
};

/** A command.
 * This structure represents a command in the table of available commands.
 */
struct command_info {
  const char *name;     /**< Canonical name of the command */
  const char *restrict_message; /**< Message sent when command is restricted */
  command_func func;    /**< Function to call when command is run */
  unsigned int type;    /**< Types of objects that can use the command */
  boolexp cmdlock;      /**< Boolexp restricting who can use command */
  /** Switches for this command. */
  union {
    switch_mask mask;       /**< Bitflags of switches this command can take */
    const char *names; /**< Space-seperated list of switches */
  } sw;
  /** Hooks on this command.
   */
  struct {
    struct hook_data before;    /**< Hook to evaluate before command */
    struct hook_data after;     /**< Hook to evaluate after command */
    struct hook_data ignore;    /**< Hook to evaluate to decide if we should ignore hardcoded command */
    struct hook_data override;  /**< Hook to override command with $command */
  } hooks;
};

typedef struct command_list COMLIST;
/** A command list entry.
 * This structure stores the static array of commands that are
 * initially loaded into the command table. Commands can also be
 * added dynamically, outside of this array.
 */
struct command_list {
  const char *name;     /**< Command name */
  const char *switches; /**< Space-separated list of switch names */
  command_func func;    /**< Function to call when command is run */
  unsigned int type;    /**< Types of objects that can use the command */
  const char *flagstr;  /**< Space-separated list of flags that can use */
  const char *powers;   /**< Powers to which the command is restricted */
};

typedef struct switch_value SWITCH_VALUE;
/** The value associated with a switch.
 * Command switches are given integral values at compile time when
 * the switchinc.c and switches.h files are rebuilt. This structure
 * associates switch names with switch numbers
 */
struct switch_value {
  const char *name;     /**< Name of the switch */
  int value;            /**< Number of the switch */
  bool used;             /**< True if a command uses this switch */
};

typedef struct com_sort_struc COMSORTSTRUC;

/** Sorted linked list of commands.
 * This structure is used to build a sorted linked list of pointers
 * to command data.
 */
struct com_sort_struc {
  struct com_sort_struc *next;  /**< Pointer to next in list */
  COMMAND_INFO *cmd;            /**< Command data */
};

/** Permissions for commands.
 * This structure is used to associate names for command permissions
 * (e.g. "player") with the appropriate bitmask
 */
struct command_perms_t {
  const char *name;     /**< Permission name */
  unsigned int type;    /**< Bitmask for this permission */
};

#define SWITCH_NONE 0
#include "switches.h"

switch_mask switchmask(const char *switches);
COMMAND_INFO *command_find(const char *name);
COMMAND_INFO *command_find_exact(const char *name);
COMMAND_INFO *command_add
  (const char *name, int type, const char *flagstr, const char *powers,
   const char *switchstr, command_func func);
COMMAND_INFO *make_command
  (const char *name, int type, const char *flagstr,
   const char *powerstr, const char *sw, command_func func);
void reserve_alias(const char *a);
int alias_command(const char *command, const char *alias);
void command_init_preconfig(void);
void command_init_postconfig(void);
void command_splitup
  (dbref player, dbref cause, char *from, char *to, char **args,
   COMMAND_INFO *cmd, int side);
void command_argparse
  (dbref executor, dbref enactor, dbref caller, NEW_PE_INFO *pe_info,
   char **from, char *to, char **argv, COMMAND_INFO *cmd, int side,
   int forcenoparse, int pe_flags);
char *command_parse(dbref player, char *string, MQUE *queue_entry);
void do_list_commands(dbref player, int lc, int type);
char *list_commands(int type);
int command_check_with(dbref player, COMMAND_INFO *cmd, int noisy,
                       NEW_PE_INFO *pe_info);
#define command_check(player, cmd, noisy) command_check_with(player,cmd,noisy,NULL)
int command_check_byname(dbref player, const char *name, NEW_PE_INFO *pe_info);
int command_check_byname_quiet(dbref player, const char *name,
                               NEW_PE_INFO *pe_info);
int restrict_command(dbref player, COMMAND_INFO *command,
                     const char *restriction);
void reserve_aliases(void);
void local_commands(void);
void do_command_add(dbref player, char *name, int flags);
void do_command_delete(dbref player, char *name);
int run_command(COMMAND_INFO *cmd, dbref executor, dbref enactor,
                const char *cmd_evaled, switch_mask sw,
                char switch_err[BUFFER_LEN], const char *cmd_raw, char *swp,
                char *ap, char *ls, char *lsa[MAX_ARG], char *rs,
                char *rsa[MAX_ARG], MQUE *queue_entry);
int cnf_add_command(char *name, char *opts);
int cnf_hook_command(char *name, char *opts);

#define SILENT_OR_NOISY(switches, default_silent) (SW_ISSET(switches, SWITCH_SILENT) ? PEMIT_SILENT : (SW_ISSET(switches, SWITCH_NOISY) ? 0 : (default_silent ? PEMIT_SILENT : 0)))


#endif                          /* __COMMAND_H */
