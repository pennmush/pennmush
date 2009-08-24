/**
 * \file boolexp.c
 *
 * \brief Boolean expression parser.
 *
 * This code implements a parser for boolean expressions of the form
 * used in locks. Summary of parsing rules, lowest to highest precedence:
 * \verbatim
 * E -> T; E -> T | E                   (or)
 * T -> F; T -> F & T                   (and)
 * F -> !F;F -> A                       (not)
 * A -> @L; A -> I                      (indirect)
 * I -> =Identifier ; I -> C            (equality)
 * C -> +Identifier ; C -> O            (carry)
 * O -> $Identifier ; O -> L            (owner)
 * L -> (E); L -> eval/attr/flag lock   (parens, special atoms)
 * L -> E, L is an object name or dbref or #t* or #f*   (simple atoms)
 * \endverbatim
 *
 * Previously, the boolexp code just used a parse tree of the
 * boolexp. Now, it turns the parse tree into bytecode that can be
 * stored in the chunk manager. It probably also evaluates faster, but
 * no profiling has been done to support this claim. It certainly
 * involves less non-tail recursion and better cache behavior.
 *
 * It's a three-stage process. First, the lock string is turned into a
 * parse tree. Second, the tree is walked and "assembler" instructions
 * are generated, including labels for jumps. Third, the "assembly" is
 * stepped through and bytecode emitted, with labeled jumps replaced
 * by distances that are offsets from the start of the
 * bytecode. Pretty standard stuff.
 *
 * Each bytecode instruction is 5 bytes long (1 byte opcode + 4 byte
 * int argument), and the minimum number of instructions in a compiled
 * boolexp is 2, for a minimum size of 10 bytes. Compare this to the
 * size of one parse-tree node, 16 bytes. Savings appear to be
 * substantial, especially with complex locks with lots of ors or
 * ands.
 *
 * Many lock keys have string arguments. The strings are standard
 * 0-terminated C strings stored in a section of the same string as
 * the bytecode instructions, starting right after the last
 * instruction. They're accessed by offset from the start of the
 * bytecode string. If the same string appears multiple times in the
 * lock, only one copy is actually present in the string section.
 *
 * The VM for the bytecode is a simple register-based one.  The
 * registers are R, the result register, set by test instructions and
 * a few others, and S, the string register, which holds the extra
 * string in the few tests that need two (A:B, A/B). There are
 * instructions for each lock key type. There's a few extra ones to
 * make decompiling back into a string dead easy. Nothing very
 * complex.
 *
 * Future directions?
 * \verbatim
 * [Development] Raevnos is tempted in passing to re-write the boolexp parser in
 * lex and yacc.
 * [Development] Brazil laughs.
 * [Development] Brazil says, "That might not be a bad idea."
 * [Development] Raevnos has redone everything else in boolexp.c in Penn, so why
 * not? :)
 * [Development] Raevnos says, "Using the justification that it's a lot easier to
 * expand the langage by adding new key types that way."
 * \endverbatim
 *
 * So now you know who to blame if that particular item appears in a
 * changelog for Penn or MUX.
 *
 * On a more serious note, a) #1234 is equivalent to b)
 * =#1234|+#1234. Detecting b and turning it into a, or vis versa,
 * would be easy to do. a is a common key, but turning it into b gets
 * rid of a test instruction in the VM, at the cost of more
 * instructions in generated lock bytecode. CISC or RISC? :) It's also
 * easy to turn !!foo into foo, but nobody makes locks like that. Same
 * with !#true and !#false. Of possibly more interest is rearranging
 * the logic when ands, ors and nots are being used together. For
 * example, !a|!b can become !(a&b).
 *
 * The only optimization done right now is thread jumping: If a jump
 * would move the program counter to another jump operation, it instead
 * goes to that jump's destination.
 * 
 * There's more useful room for improvement in the lock
 * @warnings. Checking things like flag and power keys for valid flags
 * comes to mind.
 *
 */

#include "copyrite.h"
#include "config.h"

#include <ctype.h>
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "conf.h"
#include "dbdefs.h"
#include "mushdb.h"
#include "match.h"
#include "externs.h"
#include "lock.h"
#include "parse.h"
#include "attrib.h"
#include "flags.h"
#include "log.h"
#include "extchat.h"
#include "strtree.h"
#include "mymalloc.h"
#include "confmagic.h"

#ifdef WIN32
#pragma warning( disable : 4761)        /* disable warning re conversion */
#endif

/* #define DEBUG_BYTECODE */

/** Parse tree node types */
typedef enum boolexp_type {
  BOOLEXP_AND, /**< A&B */
  BOOLEXP_OR, /**< A|B */
  BOOLEXP_NOT, /**< !A */
  BOOLEXP_CONST, /**< A */
  BOOLEXP_ATR, /**< A:B */
  BOOLEXP_IND, /**< @A/B */
  BOOLEXP_CARRY, /**< +A */
  BOOLEXP_IS, /**< =A */
  BOOLEXP_OWNER, /**< $A */
  BOOLEXP_EVAL, /**< A/B */
  BOOLEXP_FLAG, /**< A^B */
  BOOLEXP_BOOL /**< #true, #false */
} boolexp_type;

/** An attribute lock specification for the parse tree.
 * This structure is a piece of a boolexp that's used to store 
 * attribute locks (CANDO:1), eval locks (CANDO/1), and flag locks
 * FLAG^WIZARD.
 */
struct boolatr {
  const char *name;             /**< Name of attribute, flag, etc. to test */
  char text[BUFFER_LEN];        /**< Value to test against */
};

/** A boolean expression parse tree node.
 * Boolean expressions are most widely used in locks. This structure
 * is a general representation of the possible boolean expressions
 * that can be specified in MUSHcode. It's used internally by the lock
 * compiler.
 */
struct boolexp_node {
  /** Type of expression.
   * The type of expressio is one of the boolexp_type's, such as
   * and, or, not, constant, attribute, indirect, carry, is,
   * owner, eval, flag, etc.
   */
  boolexp_type type;
  dbref thing;                  /**< An object, or a boolean val */
  /** The expression itself.
   * This union comprises the various possible types of data we
   * might need to represent any of the expression types.
   */
  union {
    /** And and or locks: combinations of boolexps.
     * This union member is used with and and or locks.
     */
    struct {
      struct boolexp_node *a;   /**< One boolean expression */
      struct boolexp_node *b;   /**< Another boolean expression */
    } sub;
    struct boolexp_node *n;             /**< Not locks: boolean expression to negate */
    struct boolatr *atr_lock;   /**< Atr, eval and flag locks */
    const char *ind_lock;       /**< Indirect locks */
  } data;
};


/** The opcodes supported by the boolexp virtual machine. */
typedef enum bvm_opcode {
  OP_JMPT, /**< Jump to ARG if R is true */
  OP_JMPF, /**< Jump to ARG if R is false */
  OP_TCONST, /**< Tests plain #ARG */
  OP_TATR, /**< Tests S:ARG */
  OP_TIND, /**< Tests @#ARG/S */
  OP_TCARRY, /**< Tests +#ARG */
  OP_TIS, /**< Tests =#ARG */
  OP_TOWNER, /**< Tests $#ARG */
  OP_TEVAL, /**< Tests S/ARG */
  OP_TFLAG, /**< Tests FLAG^ARG */
  OP_TTYPE, /**< Tests TYPE^ARG */
  OP_TPOWER, /**< Tests POWER^ARG */
  OP_TCHANNEL, /**< Tests CHANNEL^ARG */
  OP_TIP, /**< Tests IP^ARG */
  OP_THOSTNAME, /**< Tests HOSTNAME^ARG */
  OP_TOBJID, /**< Tests OBJID^ARG */
  OP_TDBREFLIST,        /**< Tests DBERFLIST^ARG */
  OP_LOADS, /**< Load ARG into S */
  OP_LOADR, /**< Load ARG into R */
  OP_NEGR,  /**< Negate R */
  OP_PAREN, /**< ARG = 0 for a (, ARG = 1 for a ) in decompiling */
  OP_LABEL, /**< A label. Not actually in compiled bytecode */
  OP_RET    /**< Stop evaluating bytecode */
} bvm_opcode;

/** The size of a single bytecode instruction. Probably 5 bytes
 * everywhere. */
#define INSN_LEN (1 + sizeof(int))

/** Information describing one VM instruction or label in the
 * intermediate "assembly" generated from a parse tree. The nodes are
 * part of a linked list.  */
struct bvm_asmnode {
  bvm_opcode op; /**< The opcode */
  int arg; /**< The arg value, or a label or string number */
  struct bvm_asmnode *next; /**< Pointer to the next node */
};

/** Information describing a string to emit in the string section of
 * the bytecode. The nodes are part of a linked list.  */
struct bvm_strnode {
  char *s; /**< The string */
  size_t len; /**< Its length */
  struct bvm_strnode *next; /**< Pointer to the next node */
};

/** A struct describing the complete assembly information needed to
 * generate bytecode */
struct bvm_asm {
  struct bvm_asmnode *head; /**< The start of the list of assembly instructions */
  struct bvm_asmnode *tail; /**< The end of the list */
  struct bvm_strnode *shead; /**< The start of the list of strings */
  struct bvm_strnode *stail; /**< The end of the list */
  int label; /**< The current label id to use */
  size_t strcount;      /**< The number of nodes in the string list */
};

/** The flag lock key (A^B) only allows a few values for A. This
 * struct and the the following table define the allowable ones. When
 * adding a new type here, a matching new bytecode instruction should
 * be added. */
struct flag_lock_types {
  const char *name; /**< The value of A */
  bvm_opcode op;  /**< The associated opcode */
};

/** What's allowed on the left-hand-side of LHS^RHS lock keys */
static struct flag_lock_types flag_locks[] = {
  {"FLAG", OP_TFLAG},
  {"POWER", OP_TPOWER},
  {"TYPE", OP_TTYPE},
  {"CHANNEL", OP_TCHANNEL},
  {"OBJID", OP_TOBJID},
  {"IP", OP_TIP},
  {"HOSTNAME", OP_THOSTNAME},
  {"DBREFLIST", OP_TDBREFLIST},
  {NULL, OP_RET}
};

static uint8_t *
safe_get_bytecode(boolexp b)
  __attribute_malloc__;
    static uint8_t *get_bytecode(boolexp b, uint16_t * storelen);
    static struct boolexp_node *alloc_bool(void) __attribute_malloc__;
    static struct boolatr *alloc_atr(const char *name,
                                     const char *s) __attribute_malloc__;
    static void skip_whitespace(void);
    static void free_bool(struct boolexp_node *b);
    static struct boolexp_node *test_atr(char *s, char c);
    static struct boolexp_node *parse_boolexp_R(void);
    static struct boolexp_node *parse_boolexp_L(void);
    static struct boolexp_node *parse_boolexp_O(void);
    static struct boolexp_node *parse_boolexp_C(void);
    static struct boolexp_node *parse_boolexp_I(void);
    static struct boolexp_node *parse_boolexp_A(void);
    static struct boolexp_node *parse_boolexp_F(void);
    static struct boolexp_node *parse_boolexp_T(void);
    static struct boolexp_node *parse_boolexp_E(void);
    static int check_attrib_lock(dbref player, dbref target,
                                 const char *atrname, const char *str);
    static void free_boolexp_node(struct boolexp_node *b);
    static int gen_label_id(struct bvm_asm *a);
    static void append_insn(struct bvm_asm *a, bvm_opcode op, int arg,
                            const char *s);
    static struct bvm_asm *generate_bvm_asm(struct boolexp_node *b)
 __attribute_malloc__;
    static void generate_bvm_asm1(struct bvm_asm *a, struct boolexp_node *b,
                                  boolexp_type outer);
    static size_t pos_of_label(struct bvm_asm *a, int label);
    static size_t offset_to_string(struct bvm_asm *a, int c);
    static struct bvm_asmnode *insn_after_label(struct bvm_asm *a, int label);
    static void opt_thread_jumps(struct bvm_asm *a);
    static void optimize_bvm_asm(struct bvm_asm *a);
    static boolexp emit_bytecode(struct bvm_asm *a, int derefs);
    static void free_bvm_asm(struct bvm_asm *a);
#ifdef DEBUG_BYTECODE
    static int sizeof_boolexp_node(struct boolexp_node *b);
    static void print_bytecode(boolexp b);
#endif
    extern void complain
      (dbref player, dbref i, const char *name, const char *desc, ...)
  __attribute__ ((__format__(__printf__, 4, 5)));
    void check_lock(dbref player, dbref i, const char *name, boolexp be);
    int warning_lock_type(const boolexp l);


/** String tree of attribute names. Used in the parse tree. Might go
 * away as the trees aren't persistant any more. */
    extern StrTree atr_names;
/** String tree of lock names. Used in the parse tree. Might go away
 * as the trees aren't persistant any more. */
    extern StrTree lock_names;
/** Are we currently loading the db? If so, we avoid certain checks that
 * would create a circularity.
 */
    extern int loading_db;

/** Given a chunk id, return the bytecode for a boolexp.  
 * \param b the boolexp to retrieve.
 * \return a malloced copy of the bytecode.
 */
    static uint8_t *safe_get_bytecode(boolexp b)
{
  uint8_t *bytecode;
  uint16_t len;

  len = chunk_len(b);
  bytecode = mush_malloc(len, "boolexp.bytecode");
  chunk_fetch(b, bytecode, len);
  return bytecode;
}

/** Given a chunk id, return the bytecode for a boolexp.  
 * \param b The boolexp to retrieve.
 * \param storelen the length of the bytecode.
 * \return a static copy of the bytecode.
 */
static uint8_t *
get_bytecode(boolexp b, uint16_t * storelen)
{
  static uint8_t bytecode[BUFFER_LEN * 2];
  uint16_t len;

  len = chunk_fetch(b, bytecode, sizeof bytecode);
  if (storelen)
    *storelen = len;
  return bytecode;
}

/* Public functions */

/** Copy a boolexp.
 * This function makes a copy of a boolexp, allocating new memory for
 * the copy.
 * \param b a boolexp to copy.
 * \return an allocated copy of the boolexp.
 */
boolexp
dup_bool(boolexp b)
{
  boolexp r;
  uint8_t *bytecode;
  uint16_t len = 0;

  if (b == TRUE_BOOLEXP)
    return TRUE_BOOLEXP;

  bytecode = get_bytecode(b, &len);

  r = chunk_create(bytecode, len, 1);

  return r;
}

/** Free a boolexp
 * This function deallocates a boolexp
 * \param b a boolexp to delete
 */
void
free_boolexp(boolexp b)
{
  if (b != TRUE_BOOLEXP)
    chunk_delete(b);
}

/** Determine the memory usage of a boolexp.
 * This function computes the total memory usage of a boolexp.
 * \param b boolexp to analyze.
 * \return size of boolexp in bytes.
 */
int
sizeof_boolexp(boolexp b)
{

  if (b == TRUE_BOOLEXP)
    return 0;
  else
    return (int) chunk_len(b);
}

/** Evaluate a boolexp.
 * This is the main function to be called by other hardcode. It
 * determines whether a player can pass a boolexp lock on a given
 * object.
 * \param player the player trying to pass the lock.
 * \param b the boolexp to evaluate.
 * \param target the object with the lock.
 * \retval 0 player fails to pass lock.
 * \retval 1 player successfully passes lock.
 */
int
eval_boolexp(dbref player /* The player trying to pass */ ,
             boolexp b /* The boolexp */ ,
             dbref target /* The object with the lock */ )
{
  static int boolexp_recursion = 0;

  if (!GoodObject(player))
    return 0;

  if (boolexp_recursion > MAX_DEPTH) {
    notify(player, T("Too much recursion in lock!"));
    return 0;
  }
  if (b == TRUE_BOOLEXP) {
    return 1;
  } else {
    bvm_opcode op;
    int arg;
    ATTR *a;
    int r = 0;
    char *s = NULL;
    uint8_t *bytecode, *pc;

    bytecode = pc = safe_get_bytecode(b);

    while (1) {
      op = (bvm_opcode) *pc;
      memcpy(&arg, pc + 1, sizeof arg);
      pc += INSN_LEN;
      switch (op) {
      case OP_RET:
        goto done;
      case OP_JMPT:
        if (r)
          pc = bytecode + arg;
        break;
      case OP_JMPF:
        if (!r)
          pc = bytecode + arg;
        break;
      case OP_LABEL:
      case OP_PAREN:
        break;
      case OP_LOADS:
        s = (char *) (bytecode + arg);
        break;
      case OP_LOADR:
        r = arg;
        break;
      case OP_NEGR:
        r = !r;
        break;
      case OP_TCONST:
        r = (GoodObject(arg)
             && !IsGarbage(arg)
             && (arg == player || member(arg, Contents(player))));
        break;
      case OP_TIS:
        r = (GoodObject(arg)
             && !IsGarbage(arg)
             && arg == player);
        break;
      case OP_TCARRY:
        r = (GoodObject(arg)
             && !IsGarbage(arg)
             && member(arg, Contents(player)));
        break;
      case OP_TOWNER:
        r = (GoodObject(arg)
             && !IsGarbage(arg)
             && Owner(arg) == Owner(player));
        break;
      case OP_TIND:
        /* We only allow evaluation of indirect locks if target can run
         * the lock on the referenced object.
         */
        boolexp_recursion++;
        if (!GoodObject(arg) || IsGarbage(arg))
          r = 0;
        else if (!Can_Read_Lock(target, arg, s))
          r = 0;
        else
          r = eval_boolexp(player, getlock(arg, s), arg);
        boolexp_recursion--;
        break;
      case OP_TATR:
        boolexp_recursion++;
        a = atr_get(player, s);
        if (!a || !Can_Read_Attr(target, player, a))
          r = 0;
        else {
          char tbuf[BUFFER_LEN];
          strcpy(tbuf, atr_value(a));
          r = local_wild_match((char *) bytecode + arg, tbuf);
        }
        boolexp_recursion--;
        break;
      case OP_TEVAL:
        boolexp_recursion++;
        r = check_attrib_lock(player, target, s, (char *) bytecode + arg);
        boolexp_recursion--;
        break;
      case OP_TFLAG:
        /* Note that both fields of a boolattr struct are upper-cased */
        if (sees_flag("FLAG", target, player, (char *) bytecode + arg))
          r = 1;
        else
          r = 0;
        break;
      case OP_TPOWER:
        if (sees_flag("POWER", target, player, (char *) bytecode + arg))
          r = 1;
        else
          r = 0;
        break;
      case OP_TOBJID:
        {
          dbref d;
          d = parse_objid((char *) bytecode + arg);
          r = (player == d);
          break;
        }
      case OP_TCHANNEL:
        {
          CHAN *chan;
          boolexp_recursion++;
          find_channel((char *) bytecode + arg, &chan, target);
          r = chan && onchannel(player, chan);
          boolexp_recursion--;
        }
        break;
      case OP_TIP:
        boolexp_recursion++;
        if (!Connected(Owner(player)))
          r = 0;
        else {
          /* We use the attribute for permission checks, but we
           * do the actual boolexp itself with the least idle
           * descriptor's ip address.
           */
          a = atr_get(Owner(player), "LASTIP");
          if (!a || !Can_Read_Attr(target, player, a))
            r = 0;
          else {
            char *p = least_idle_ip(Owner(player));
            r = p ? quick_wild((char *) bytecode + arg, p) : 0;
          }
        }
        boolexp_recursion--;
        break;
      case OP_THOSTNAME:
        boolexp_recursion++;
        if (!Connected(Owner(player)))
          r = 0;
        else {
          /* See comment for OP_TIP */
          a = atr_get(Owner(player), "LASTSITE");
          if (!a || !Can_Read_Attr(target, player, a))
            r = 0;
          else {
            char *p = least_idle_hostname(Owner(player));
            r = p ? quick_wild((char *) bytecode + arg, p) : 0;
          }
        }
        boolexp_recursion--;
        break;
      case OP_TTYPE:
        switch (bytecode[arg]) {
        case 'R':
          r = Typeof(player) == TYPE_ROOM;
          break;
        case 'E':
          r = Typeof(player) == TYPE_EXIT;
          break;
        case 'T':
          r = Typeof(player) == TYPE_THING;
          break;
        case 'P':
          r = Typeof(player) == TYPE_PLAYER;
          break;
        }
        break;
      case OP_TDBREFLIST:
        {
          char *idstr, *curr, *orig;
          dbref mydb;

          r = 0;
          a = atr_get(target, (char *) bytecode + arg);
          if (!a)
            break;

          orig = safe_atr_value(a);
          idstr = trim_space_sep(orig, ' ');

          while ((curr = split_token(&idstr, ' ')) != NULL) {
            mydb = parse_objid(curr);
            if (mydb == player) {
              r = 1;
              break;
            }
          }
          free((Malloc_t) orig);
        }
        break;
      default:
        do_log(LT_ERR, 0, 0, "Bad boolexp opcode %d %d in object #%d",
               op, arg, target);
        report();
        r = 0;
      }
    }
  done:
    mush_free(bytecode, "boolexp.bytecode");
    return r;
  }
}

/** Pretty-print object references for unparse_boolexp().
 * \param player the object seeing the decompiled lock.
 * \param thing the object referenced in the lock.
 * \param flag How to print thing.
 * \param buff The start of the output buffer.
 * \param bp Pointer to the current position in buff.
 * \return 0 on success, true on buffer overflow.
 */
static int
safe_boref(dbref player, dbref thing, enum u_b_f flag, char *buff, char **bp)
{
  switch (flag) {
  case UB_MEREF:
    if (player == thing)
      return safe_strl("me", 2, buff, bp);
    else
      return safe_dbref(thing, buff, bp);
  case UB_DBREF:
    return safe_dbref(thing, buff, bp);
  case UB_ALL:
  default:
    return safe_str(unparse_object(player, thing), buff, bp);
  }
}

/** True if unparse_boolexp() is being evaluated. */
int unparsing_boolexp = 0;

/** Display a boolexp.
 * This function returns the textual representation of the boolexp.
 * \param player The object wanting the decompiled boolexp.
 * \param b The boolexp to decompile.
 * \param flag How to format objects in the result.
 * \return a static string with the decompiled boolexp.
 */
char *
unparse_boolexp(dbref player, boolexp b, enum u_b_f flag)
{
  static char boolexp_buf[BUFFER_LEN];
  char *buftop = boolexp_buf;
  unsigned char *bytecode = NULL;

  unparsing_boolexp = 1;

  if (b == TRUE_BOOLEXP)
    safe_str("*UNLOCKED*", boolexp_buf, &buftop);
  else {
    bvm_opcode op;
    int arg;
    unsigned char *pc;
    unsigned char *s = NULL;

    bytecode = pc = get_bytecode(b, NULL);

    while (1) {
      op = (bvm_opcode) *pc;
      memcpy(&arg, pc + 1, sizeof arg);
      pc += INSN_LEN;
      /* Handle most negation cases */
      if (op != OP_RET && (bvm_opcode) *pc == OP_NEGR && op != OP_PAREN)
        safe_chr('!', boolexp_buf, &buftop);
      switch (op) {
      case OP_JMPT:
        safe_chr('|', boolexp_buf, &buftop);
        break;
      case OP_JMPF:
        safe_chr('&', boolexp_buf, &buftop);
        break;
      case OP_RET:
        goto done;
      case OP_LABEL:           /* Will never happen, but shuts up the compiler */
      case OP_NEGR:
        break;
      case OP_LOADS:
        s = bytecode + arg;
        break;
      case OP_LOADR:
        if (arg)
          safe_str("#TRUE", boolexp_buf, &buftop);
        else
          safe_str("#FALSE", boolexp_buf, &buftop);
        break;
      case OP_PAREN:
        if (arg == 0) {
          int pstack = 1, parg;
          unsigned char *tpc = pc;
          while (1) {
            if ((bvm_opcode) *tpc == OP_PAREN) {
              memcpy(&parg, tpc + 1, sizeof parg);
              if (parg)
                pstack--;
              else
                pstack++;
              if (pstack == 0) {
                tpc += INSN_LEN;
                break;
              }
            }
            tpc += INSN_LEN;
          }
          if ((bvm_opcode) *tpc == OP_NEGR)
            safe_strl("!(", 2, boolexp_buf, &buftop);
          else
            safe_chr('(', boolexp_buf, &buftop);
        } else if (arg == 1)
          safe_chr(')', boolexp_buf, &buftop);
        break;
      case OP_TCONST:
        safe_boref(player, arg, flag, boolexp_buf, &buftop);
        break;
      case OP_TATR:
        safe_format(boolexp_buf, &buftop, "%s:%s", s, bytecode + arg);
        break;
      case OP_TIND:
        safe_chr(AT_TOKEN, boolexp_buf, &buftop);
        safe_boref(player, arg, flag, boolexp_buf, &buftop);
        safe_format(boolexp_buf, &buftop, "/%s", s);
        break;
      case OP_TCARRY:
        safe_chr(IN_TOKEN, boolexp_buf, &buftop);
        safe_boref(player, arg, flag, boolexp_buf, &buftop);
        break;
      case OP_TIS:
        safe_chr(IS_TOKEN, boolexp_buf, &buftop);
        safe_boref(player, arg, flag, boolexp_buf, &buftop);
        break;
      case OP_TOWNER:
        safe_chr(OWNER_TOKEN, boolexp_buf, &buftop);
        safe_boref(player, arg, flag, boolexp_buf, &buftop);
        break;
      case OP_TEVAL:
        safe_format(boolexp_buf, &buftop, "%s/%s", s, bytecode + arg);
        break;
      case OP_TFLAG:
        safe_format(boolexp_buf, &buftop, "FLAG^%s", bytecode + arg);
        break;
      case OP_TTYPE:
        safe_format(boolexp_buf, &buftop, "TYPE^%s", bytecode + arg);
        break;
      case OP_TPOWER:
        safe_format(boolexp_buf, &buftop, "POWER^%s", bytecode + arg);
        break;
      case OP_TOBJID:
        safe_format(boolexp_buf, &buftop, "OBJID^%s", bytecode + arg);
        break;
      case OP_TCHANNEL:
        safe_format(boolexp_buf, &buftop, "CHANNEL^%s", bytecode + arg);
        break;
      case OP_TIP:
        safe_format(boolexp_buf, &buftop, "IP^%s", bytecode + arg);
        break;
      case OP_THOSTNAME:
        safe_format(boolexp_buf, &buftop, "HOSTNAME^%s", bytecode + arg);
        break;
      case OP_TDBREFLIST:
        safe_format(boolexp_buf, &buftop, "DBREFLIST^%s", bytecode + arg);
        break;
      }
    }
  }
done:
  *buftop++ = '\0';
  unparsing_boolexp = 0;

  return boolexp_buf;
}

/* Parser and parse-tree related functions. If the parser returns NULL, you lose */
/** The source string for the lock we're parsing */
static const char *parsebuf;
/** The player from whose perspective we're parsing */
static dbref parse_player;
/** The name of the lock we're parsing */
static lock_type parse_ltype;

/** Allocate a boolatr for a parse tree node.
 * \param name the name of the attribute.
 * \param s a pattern to match against.
 * \return a newly allocated boolatr.
 */
static struct boolatr *
alloc_atr(const char *name, const char *s)
{
  struct boolatr *a;
  size_t len;

  if (s)
    len = strlen(s) + 1;
  else
    len = 1;

  a = (struct boolatr *)
    mush_malloc(sizeof(struct boolatr) - BUFFER_LEN + len, "boolatr");
  if (!a)
    return NULL;
  a->name = st_insert(strupper(name), &atr_names);
  if (!a->name) {
    mush_free(a, "boolatr");
    return NULL;
  }
  if (s)
    memcpy(a->text, s, len);
  else
    a->text[0] = '\0';
  return a;
}

/** Returns a new boolexp_node for the parse tree.
 * \return a new newly allocated boolexp_node.
 */
static struct boolexp_node *
alloc_bool(void)
{
  struct boolexp_node *b;

  b = mush_malloc(sizeof *b, "boolexp.node");

  b->data.sub.a = NULL;
  b->data.sub.b = NULL;
  b->thing = NOTHING;

  return b;
}

/** Frees a boolexp node.
 * \param b the boolexp_node to deallocate.
 */
static void
free_bool(struct boolexp_node *b)
{
  mush_free(b, "boolexp.node");
}

/** Free a boolexp ast node.
 * This function frees a boolexp, including all subexpressions,
 * recursively.
 * \param b boolexp to free.
 */
static void
free_boolexp_node(struct boolexp_node *b)
{
  if (b) {
    switch (b->type) {
    case BOOLEXP_AND:
    case BOOLEXP_OR:
      free_boolexp_node(b->data.sub.a);
      free_boolexp_node(b->data.sub.b);
      free_bool(b);
      break;
    case BOOLEXP_NOT:
      free_boolexp_node(b->data.n);
      free_bool(b);
      break;
    case BOOLEXP_CONST:
    case BOOLEXP_CARRY:
    case BOOLEXP_IS:
    case BOOLEXP_OWNER:
    case BOOLEXP_BOOL:
      free_bool(b);
      break;
    case BOOLEXP_IND:
      if (b->data.ind_lock)
        st_delete(b->data.ind_lock, &lock_names);
      free_bool(b);
      break;
    case BOOLEXP_ATR:
    case BOOLEXP_EVAL:
    case BOOLEXP_FLAG:
      if (b->data.atr_lock) {
        if (b->data.atr_lock->name)
          st_delete(b->data.atr_lock->name, &atr_names);
        mush_free((Malloc_t) b->data.atr_lock, "boolatr");
      }
      free_bool(b);
      break;
    }
  }
}

/** Skip over leading whitespace characters in parsebuf */
static void
skip_whitespace(void)
{
  while (*parsebuf && isspace((unsigned char) *parsebuf))
    parsebuf++;
}

static struct boolexp_node *
test_atr(char *s, char c)
{
  struct boolexp_node *b;
  char tbuf1[BUFFER_LEN];
  strcpy(tbuf1, strupper(s));
  for (s = tbuf1; *s && (*s != c); s++) ;
  if (!*s)
    return 0;
  *s++ = 0;
  if (strlen(tbuf1) == 0 || !good_atr_name(tbuf1))
    return 0;
  if (c == '^') {
    int n;
    for (n = 0; flag_locks[n].name; n++) {
      if (strcmp(flag_locks[n].name, tbuf1) == 0)
        break;
    }
    if (!flag_locks[n].name)
      return 0;
  }
  b = alloc_bool();
  if (c == ':')
    b->type = BOOLEXP_ATR;
  else if (c == '/')
    b->type = BOOLEXP_EVAL;
  else if (c == '^')
    b->type = BOOLEXP_FLAG;
  b->data.atr_lock = alloc_atr(tbuf1, s);
  return b;
}

/* L -> E, L is an object name or dbref or #t* or #f* */
static struct boolexp_node *
parse_boolexp_R(void)
{
  struct boolexp_node *b;
  char tbuf1[BUFFER_LEN];
  char *p;
  b = alloc_bool();
  b->type = BOOLEXP_CONST;
  p = tbuf1;
  while (*parsebuf
         && *parsebuf != AND_TOKEN && *parsebuf != '/'
         && *parsebuf != OR_TOKEN && *parsebuf != ')') {
    *p++ = *parsebuf++;
  }
  /* strip trailing whitespace */
  *p-- = '\0';
  while (isspace((unsigned char) *p))
    *p-- = '\0';
  /* do the match */
  if (loading_db) {
    if (*tbuf1 == '#' && *(tbuf1 + 1)) {
      if (*(tbuf1 + 1) == 't' || *(tbuf1 + 1) == 'T') {
        b->type = BOOLEXP_BOOL;
        b->thing = 1;
      } else if (*(tbuf1 + 1) == 'f' || *(tbuf1 + 1) == 'F') {
        b->type = BOOLEXP_BOOL;
        b->thing = 0;
      } else {
        b->thing = parse_integer(tbuf1 + 1);
      }
    } else {
      /* Ooog. Dealing with a malformed lock in the database. */
      free_bool(b);
      return NULL;
    }
    return b;
  } else {
    /* Are these special atoms? */
    if (*tbuf1 && *tbuf1 == '#' && *(tbuf1 + 1)) {
      if (*(tbuf1 + 1) == 't' || *(tbuf1 + 1) == 'T') {
        b->type = BOOLEXP_BOOL;
        b->thing = 1;
        return b;
      } else if (*(tbuf1 + 1) == 'f' || *(tbuf1 + 1) == 'F') {
        b->type = BOOLEXP_BOOL;
        b->thing = 0;
        return b;
      }
    }
    b->thing = match_result(parse_player, tbuf1, TYPE_THING, MAT_EVERYTHING);
    if (b->thing == NOTHING) {
      notify_format(parse_player, T("I don't see %s here."), tbuf1);
      free_bool(b);
      return NULL;
    } else if (b->thing == AMBIGUOUS) {
      notify_format(parse_player, T("I don't know which %s you mean!"), tbuf1);
      free_bool(b);
      return NULL;
    } else {
      return b;
    }
  }
}

/* L -> (E); L -> eval/attr/flag lock, (lock) */
static struct boolexp_node *
parse_boolexp_L(void)
{
  struct boolexp_node *b;
  char *p;
  const char *savebuf;
  char tbuf1[BUFFER_LEN];
  skip_whitespace();
  switch (*parsebuf) {
  case '(':
    parsebuf++;
    b = parse_boolexp_E();
    skip_whitespace();
    if (b == NULL || *parsebuf++ != ')') {
      free_boolexp_node(b);
      return NULL;
    } else {
      return b;
    }
    /* break; */
  default:
    /* must have hit an object ref */
    /* load the name into our buffer */
    p = tbuf1;
    savebuf = parsebuf;
    while (*parsebuf
           && *parsebuf != AND_TOKEN
           && *parsebuf != OR_TOKEN && *parsebuf != ')') {
      *p++ = *parsebuf++;
    }
    /* strip trailing whitespace */
    *p-- = '\0';
    while (isspace((unsigned char) *p))
      *p-- = '\0';
    /* check for an attribute */
    b = test_atr(tbuf1, ':');
    if (b)
      return b;
    /* check for an eval */
    b = test_atr(tbuf1, '/');
    if (b)
      return b;
    /* Check for a flag */
    b = test_atr(tbuf1, '^');
    if (b)
      return b;
    /* Nope. Check for an object reference */
    parsebuf = savebuf;
    return parse_boolexp_R();
  }
}

/* O -> $Identifier ; O -> L */
static struct boolexp_node *
parse_boolexp_O(void)
{
  struct boolexp_node *b2, *t;
  skip_whitespace();
  if (*parsebuf == OWNER_TOKEN) {
    parsebuf++;
    b2 = alloc_bool();
    b2->type = BOOLEXP_OWNER;
    t = parse_boolexp_R();
    if (t == NULL) {
      free_boolexp_node(b2);
      return NULL;
    } else {
      b2->thing = t->thing;
      free_boolexp_node(t);
      return b2;
    }
  }
  return parse_boolexp_L();
}

/* C -> +Identifier ; C -> O */
static struct boolexp_node *
parse_boolexp_C(void)
{
  struct boolexp_node *b2, *t;
  skip_whitespace();
  if (*parsebuf == IN_TOKEN) {
    parsebuf++;
    b2 = alloc_bool();
    b2->type = BOOLEXP_CARRY;
    t = parse_boolexp_R();
    if (t == NULL) {
      free_boolexp_node(b2);
      return NULL;
    } else {
      b2->thing = t->thing;
      free_boolexp_node(t);
      return b2;
    }
  }
  return parse_boolexp_O();
}

/* I -> =Identifier ; I -> C */
static struct boolexp_node *
parse_boolexp_I(void)
{
  struct boolexp_node *b2, *t;
  skip_whitespace();
  if (*parsebuf == IS_TOKEN) {
    parsebuf++;
    b2 = alloc_bool();
    b2->type = BOOLEXP_IS;
    t = parse_boolexp_R();
    if (t == NULL) {
      free_boolexp_node(b2);
      return NULL;
    } else {
      b2->thing = t->thing;
      free_boolexp_node(t);
      return b2;
    }
  }
  return parse_boolexp_C();
}

/* A -> @L; A -> I */
static struct boolexp_node *
parse_boolexp_A(void)
{
  struct boolexp_node *b2, *t;
  skip_whitespace();
  if (*parsebuf == AT_TOKEN) {
    parsebuf++;
    b2 = alloc_bool();
    b2->type = BOOLEXP_IND;
    t = parse_boolexp_R();
    if (t == NULL) {
      free_boolexp_node(b2);
      return NULL;
    }
    b2->thing = t->thing;
    free_boolexp_node(t);
    if (*parsebuf == '/') {
      char tbuf1[BUFFER_LEN], *p;
      const char *m;
      parsebuf++;
      p = tbuf1;
      while (*parsebuf
             && *parsebuf != AND_TOKEN
             && *parsebuf != OR_TOKEN && *parsebuf != ')') {
        *p++ = *parsebuf++;
      }
      /* strip trailing whitespace */
      *p-- = '\0';
      while (isspace((unsigned char) *p))
        *p-- = '\0';
      upcasestr(tbuf1);
      if (!good_atr_name(tbuf1)) {
        free_boolexp_node(b2);
        return NULL;
      }
      m = match_lock(tbuf1);
      b2->data.ind_lock = st_insert(m ? m : tbuf1, &lock_names);
    } else {
      b2->data.ind_lock = st_insert(parse_ltype, &lock_names);
    }
    return b2;
  }
  return parse_boolexp_I();
}

/* F -> !F;F -> A */
static struct boolexp_node *
parse_boolexp_F(void)
{
  struct boolexp_node *b2;
  skip_whitespace();
  if (*parsebuf == NOT_TOKEN) {
    parsebuf++;
    b2 = alloc_bool();
    b2->type = BOOLEXP_NOT;
    if ((b2->data.n = parse_boolexp_F()) == NULL) {
      free_boolexp_node(b2);
      return NULL;
    } else
      return b2;
  }
  return parse_boolexp_A();
}


/* T -> F; T -> F & T */
static struct boolexp_node *
parse_boolexp_T(void)
{
  struct boolexp_node *b, *b2;

  if ((b = parse_boolexp_F()) == NULL) {
    return b;
  } else {
    skip_whitespace();
    if (*parsebuf == AND_TOKEN) {
      parsebuf++;
      b2 = alloc_bool();
      b2->type = BOOLEXP_AND;
      b2->data.sub.a = b;
      if ((b2->data.sub.b = parse_boolexp_T()) == NULL) {
        free_boolexp_node(b2);
        return NULL;
      } else {
        return b2;
      }
    } else {
      return b;
    }
  }
}

/* E -> T; E -> T | E */
static struct boolexp_node *
parse_boolexp_E(void)
{
  struct boolexp_node *b, *b2;

  if ((b = parse_boolexp_T()) == NULL) {
    return b;
  } else {
    skip_whitespace();
    if (*parsebuf == OR_TOKEN) {
      parsebuf++;
      b2 = alloc_bool();
      b2->type = BOOLEXP_OR;
      b2->data.sub.a = b;
      if ((b2->data.sub.b = parse_boolexp_E()) == NULL) {
        free_boolexp_node(b2);
        return NULL;
      } else {
        return b2;
      }
    } else {
      return b;
    }
  }
}

/* Functions for turning the parse tree into assembly */

/** Create a label identifier.
 * \param a the assembler list the label is for.
 * \return a new label id
 */
static int
gen_label_id(struct bvm_asm *a)
{
  int l = a->label;
  a->label++;
  return l;
}

slab *bvm_asmnode_slab = NULL;

/** Add an instruction to the assembler list.
 * \param a the assembler list.
 * \param op the opcode of the instruction.
 * \param arg the argument for the instruction if numeric.
 * \param s the string to use as the argument for the instruction. If non-NULL, arg's value is ignored.
 */
static void
append_insn(struct bvm_asm *a, bvm_opcode op, int arg, const char *s)
{
  struct bvm_asmnode *newop;

  if (s) {
    struct bvm_strnode *newstr;
    uint32_t count = 0;
    bool found = 0;

    /* Look for an existing string */
    for (newstr = a->shead; newstr; newstr = newstr->next, count++) {
      if (strcmp(newstr->s, s) == 0) {
        arg = count;
        found = 1;
        break;
      }
    }
    /* Allocate a new string if needed. */
    if (!found) {
      newstr = mush_malloc(sizeof *newstr, "bvm.strnode");
      if (!s)
        mush_panic(T("Unable to allocate memory for boolexp string node!"));
      newstr->s = mush_strdup(s, "bvm.string");
      if (!newstr->s)
        mush_panic(T("Unable to allocate memory for boolexp string!"));
      newstr->len = strlen(s) + 1;
      newstr->next = NULL;
      if (a->shead == NULL)
        a->shead = a->stail = newstr;
      else {
        a->stail->next = newstr;
        a->stail = newstr;
      }
      arg = a->strcount;
      a->strcount++;
    }
  }

  if (bvm_asmnode_slab == NULL)
    bvm_asmnode_slab = slab_create("bvm.asmnode", sizeof *newop);
  newop = slab_malloc(bvm_asmnode_slab, NULL);
  if (!newop)
    mush_panic(T("Unable to allocate memory for boolexp asm node!"));
  newop->op = op;
  newop->arg = arg;
  newop->next = NULL;
  if (a->head == NULL)
    a->head = a->tail = newop;
  else {
    a->tail->next = newop;
    a->tail = newop;
  }
}

/** Does the actual work of walking the parse tree and creating an
 * assembler list from it.
 * \param a the assembler list.
 * \param b the root of the parse tree.
 * \param outer the type of root's parent node.
 */
static void
generate_bvm_asm1(struct bvm_asm *a, struct boolexp_node *b, boolexp_type outer)
{
  int lbl;

  switch (b->type) {
  case BOOLEXP_AND:
    lbl = gen_label_id(a);
    if (outer == BOOLEXP_NOT)
      append_insn(a, OP_PAREN, 0, NULL);
    generate_bvm_asm1(a, b->data.sub.a, b->type);
    append_insn(a, OP_JMPF, lbl, NULL);
    generate_bvm_asm1(a, b->data.sub.b, b->type);
    if (outer == BOOLEXP_NOT)
      append_insn(a, OP_PAREN, 1, NULL);
    append_insn(a, OP_LABEL, lbl, NULL);
    break;
  case BOOLEXP_OR:
    lbl = gen_label_id(a);
    if (outer == BOOLEXP_NOT || outer == BOOLEXP_AND)
      append_insn(a, OP_PAREN, 0, NULL);
    generate_bvm_asm1(a, b->data.sub.a, b->type);
    append_insn(a, OP_JMPT, lbl, NULL);
    generate_bvm_asm1(a, b->data.sub.b, b->type);
    if (outer == BOOLEXP_NOT || outer == BOOLEXP_AND)
      append_insn(a, OP_PAREN, 1, NULL);
    append_insn(a, OP_LABEL, lbl, NULL);
    break;
  case BOOLEXP_IND:
    append_insn(a, OP_LOADS, 0, b->data.ind_lock);
    append_insn(a, OP_TIND, b->thing, NULL);
    break;
  case BOOLEXP_IS:
    append_insn(a, OP_TIS, b->thing, NULL);
    break;
  case BOOLEXP_CARRY:
    append_insn(a, OP_TCARRY, b->thing, NULL);
    break;
  case BOOLEXP_OWNER:
    append_insn(a, OP_TOWNER, b->thing, NULL);
    break;
  case BOOLEXP_NOT:
    generate_bvm_asm1(a, b->data.n, b->type);
    append_insn(a, OP_NEGR, 0, NULL);
    break;
  case BOOLEXP_CONST:
    append_insn(a, OP_TCONST, b->thing, NULL);
    break;
  case BOOLEXP_BOOL:
    append_insn(a, OP_LOADR, b->thing, NULL);
    break;
  case BOOLEXP_ATR:
    append_insn(a, OP_LOADS, 0, b->data.atr_lock->name);
    append_insn(a, OP_TATR, 0, b->data.atr_lock->text);
    break;
  case BOOLEXP_EVAL:
    append_insn(a, OP_LOADS, 0, b->data.atr_lock->name);
    append_insn(a, OP_TEVAL, 0, b->data.atr_lock->text);
    break;
  case BOOLEXP_FLAG:
    {
      enum bvm_opcode op = OP_RET;
      int n;
      for (n = 0; flag_locks[n].name; n++) {
        if (strcmp(b->data.atr_lock->name, flag_locks[n].name) == 0) {
          op = flag_locks[n].op;
          break;
        }
      }
      append_insn(a, op, 0, b->data.atr_lock->text);
      break;
    }
  }
}

/** Turn a parse tree into an assembler list.
 * \param the parse tree
 * \return newly allocated assembler list.
 */
static struct bvm_asm *
generate_bvm_asm(struct boolexp_node *b)
{
  struct bvm_asm *a;

  if (!b)
    return NULL;

  a = mush_malloc(sizeof *a, "bvm.asm");
  if (!a)
    return NULL;

  a->strcount = a->label = 0;
  a->head = a->tail = NULL;
  a->shead = a->stail = NULL;

  generate_bvm_asm1(a, b, BOOLEXP_CONST);
  append_insn(a, OP_RET, 0, NULL);

  return a;
}

/** Frees an assembler list.
 * \param a the assembler list to deallocate.
 */
static void
free_bvm_asm(struct bvm_asm *a)
{
  struct bvm_strnode *s, *tmp2;

  if (!a)
    return;

  slab_destroy(bvm_asmnode_slab);
  bvm_asmnode_slab = NULL;

  for (s = a->shead; s; s = tmp2) {
    tmp2 = s->next;
    mush_free(s->s, "bvm.string");
    mush_free(s, "bvm.strnode");
  }
  mush_free(a, "bvm.asm");
}

/** Find the position of a labeled instruction.
 * \param as the assembler list.
 * \param the id of the label to find.
 * \return the number of instructions before the label.
 */
static size_t
pos_of_label(struct bvm_asm *as, int label)
{
  size_t offset = 0;
  struct bvm_asmnode *a;
  for (a = as->head; a; a = a->next) {
    if (a->op == OP_LABEL && a->arg == label)
      return offset;
    if (a->op != OP_LABEL)
      offset++;
  }
  return offset;                /* Never reached! */
}

/** Find the position of a string.
 * \param a the assembler list
 * \param c The c-th string is the one that's wanted.
 * \return the distance from the start of the string section to the start of the c-th string.
 */
static size_t
offset_to_string(struct bvm_asm *a, int c)
{
  size_t offset = 0;
  int n = 0;
  struct bvm_strnode *s;

  for (s = a->shead; s; s = s->next, n++) {
    if (n == c)
      return offset;
    else
      offset += s->len;
  }
  return offset;                /* Never reached! */
}

/** Find the next instruction after a label.
 * \param a the assembler list.
 * \param label the label id to look for.
 * \return a pointer to the first real instruction after a label; where a jump to that label will go to.
 */
static struct bvm_asmnode *
insn_after_label(struct bvm_asm *a, int label)
{
  struct bvm_asmnode *n;

  for (n = a->head; n; n = n->next) {
    if (n->op == OP_LABEL && n->arg == label) {
      do {
        n = n->next;
      } while (n->op == OP_LABEL);
      return n;
    }
  }
  return NULL;
}

/** Avoid jumps that lead straight to another jump. If the second jump
 * is on the same condition as the first one, jump instead to its
 * destination. If it's the opposite condition, jump instead to the
 * first instruction after the second jump to avoid the useless
 * conditional check. 
 * \param a the assembler list to thread. */
static void
opt_thread_jumps(struct bvm_asm *a)
{
  struct bvm_asmnode *n, *target;

  for (n = a->head; n;) {
    if (n->op == OP_JMPT || n->op == OP_JMPF) {
      target = insn_after_label(a, n->arg);
      if (target && (target->op == OP_JMPT || target->op == OP_JMPF)) {
        if (target->op == n->op) {
          /* Avoid daisy-chained conditional jumps on the same
             condition.
           */
          n->arg = target->arg;
        } else {
          /* Avoid useless conditional jumps on different conditions by
             jumping to the next instruction after. Ex: a&b|c */
          struct bvm_asmnode *newlbl;
          newlbl = slab_malloc(bvm_asmnode_slab, NULL);
          if (!newlbl)
            mush_panic(T("Unable to allocate memory for boolexp asm node!"));
          newlbl->op = OP_LABEL;
          n->arg = newlbl->arg = gen_label_id(a);
          if (target->next)
            newlbl->next = target->next;
          else
            newlbl->next = NULL;
          target->next = newlbl;
          if (a->tail == target)
            a->tail = newlbl;
        }
      } else
        n = n->next;
    } else
      n = n->next;
  }
}

/** Do some trivial optimizations.  
 * \param a the assembler list to transform.
 */
static void
optimize_bvm_asm(struct bvm_asm *a)
{
  if (!a)
    return;
  opt_thread_jumps(a);
}

/** Turn assembly into bytecode. 
 * \param a the assembly list to emit.
 * \param the compiled bytecode.
 */
static boolexp
emit_bytecode(struct bvm_asm *a, int derefs)
{
  boolexp b;
  struct bvm_asmnode *i;
  struct bvm_strnode *s;
  unsigned char *pc, *bytecode;
  uint16_t len, blen;

  if (!a)
    return TRUE_BOOLEXP;

  /* Calculate the total size of the bytecode */
  len = 0;

  for (i = a->head; i; i = i->next) {
    if (i->op == OP_LABEL)
      continue;
    len++;
  }

  len *= INSN_LEN;
  blen = len;

  for (s = a->shead; s; s = s->next)
    len += s->len;

  pc = bytecode = mush_malloc(len, "boolexp.bytecode");
  if (!pc)
    return TRUE_BOOLEXP;

  /* Emit the instructions */
  for (i = a->head; i; i = i->next) {
    switch (i->op) {
    case OP_LABEL:
      continue;
    case OP_JMPT:
    case OP_JMPF:
      i->arg = pos_of_label(a, i->arg) * INSN_LEN;
      break;
    case OP_LOADS:
    case OP_TEVAL:
    case OP_TATR:
    case OP_TFLAG:
    case OP_TPOWER:
    case OP_TOBJID:
    case OP_TTYPE:
    case OP_TCHANNEL:
    case OP_TIP:
    case OP_THOSTNAME:
    case OP_TDBREFLIST:
      i->arg = blen + offset_to_string(a, i->arg);
      break;
    default:
      break;
    }

    *pc = (char) i->op;
    memcpy(pc + 1, &i->arg, sizeof i->arg);
    pc += INSN_LEN;
  }

  /* Emit the strings section */
  for (s = a->shead; s; s = s->next) {
    memcpy(pc, s->s, s->len);
    pc += s->len;
  }

  b = chunk_create(bytecode, len, derefs);
  mush_free(bytecode, "boolexp.bytecode");
  return b;
}

/** Compile a string into boolexp bytecode.
 * Given a textual representation of a boolexp in a string, parse it into
 * a syntax tree, compile to bytecode, and return a pointer to a boolexp
 * structure.
 * \param player the enactor.
 * \param buf string representation of a boolexp.
 * \param ltype the type of lock for which the boolexp is being parsed.
 * \param derefs the starting deref count for chunk storage.
 * \return pointer to a newly allocated boolexp.
 */
boolexp
parse_boolexp_d(dbref player, const char *buf, lock_type ltype, int derefs)
{
  struct boolexp_node *ast;
  struct bvm_asm *bvasm;
  boolexp bytecode;
  /* Parse */
  parsebuf = buf;
  parse_player = player;
  parse_ltype = ltype;
  ast = parse_boolexp_E();
  if (!ast)
    return TRUE_BOOLEXP;
  bvasm = generate_bvm_asm(ast);
  if (!bvasm) {
    free_boolexp_node(ast);
    return TRUE_BOOLEXP;
  }
  optimize_bvm_asm(bvasm);
  bytecode = emit_bytecode(bvasm, derefs);
#ifdef DEBUG_BYTECODE
  printf("\nSource string: \"%s\"\n", buf);
  printf("Parse tree size: %d bytes\n", sizeof_boolexp_node(ast));
  print_bytecode(bytecode);
#endif
  free_boolexp_node(ast);
  free_bvm_asm(bvasm);
  return bytecode;
}

/** Compile a string into boolexp bytecode.
 * Given a textual representation of a boolexp in a string, parse it into
 * a syntax tree, compile to bytecode, and return a pointer to a boolexp
 * structure.
 * \param player the enactor.
 * \param buf string representation of a boolexp.
 * \param ltype the type of lock for which the boolexp is being parsed.
 * \return pointer to a newly allocated boolexp.
 */
boolexp
parse_boolexp(dbref player, const char *buf, lock_type ltype)
{
  return parse_boolexp_d(player, buf, ltype, 0);
}

/** Test to see if an eval lock passes, with a exact match.
 * \param player the object attempting to pass the lock.
 * \param target the object the lock is on.
 * \param atrname the name of the attribute to evaluate on target.
 * \param str What the attribute should evaluate to to succeed.
 * \retval 1 the lock succeeds.
 * \retval 0 the lock fails.
 */
static int
check_attrib_lock(dbref player, dbref target,
                  const char *atrname, const char *str)
{

  ATTR *a;
  char *asave;
  const char *ap;
  char buff[BUFFER_LEN], *bp;
  char *preserve[NUMQ];
  if (!atrname || !*atrname || !str || !*str)
    return 0;
  /* fail if there's no matching attribute */
  a = atr_get(target, strupper(atrname));
  if (!a)
    return 0;
  if (!Can_Read_Attr(target, target, a))
    return 0;
  asave = safe_atr_value(a);
  /* perform pronoun substitution */
  save_global_regs("check_attrib_lock_save", preserve);
  bp = buff;
  ap = asave;
  process_expression(buff, &bp, &ap, target, player,
                     player, PE_DEFAULT, PT_DEFAULT, NULL);
  *bp = '\0';
  restore_global_regs("check_attrib_lock_save", preserve);
  free(asave);

  return !strcasecmp(buff, str);
}

#ifdef DEBUG_BYTECODE

/** Find the size of a parse tree node, recursively to count all child nodes.
 * \param b the root of the parse tree.
 * \return the size of the parse tree in bytes.
 */
static int
sizeof_boolexp_node(struct boolexp_node *b)
{
  if (!b)
    return 0;
  switch (b->type) {
  case BOOLEXP_CONST:
  case BOOLEXP_IS:
  case BOOLEXP_CARRY:
  case BOOLEXP_OWNER:
  case BOOLEXP_IND:
  case BOOLEXP_BOOL:
    return sizeof *b;
  case BOOLEXP_NOT:
    return sizeof *b + sizeof_boolexp_node(b->data.n);
  case BOOLEXP_AND:
  case BOOLEXP_OR:
    return sizeof *b +
      sizeof_boolexp_node(b->data.sub.a) + sizeof_boolexp_node(b->data.sub.b);
  case BOOLEXP_ATR:
  case BOOLEXP_EVAL:
  case BOOLEXP_FLAG:
    return sizeof *b + sizeof *b->data.atr_lock - BUFFER_LEN +
      strlen(b->data.atr_lock->text) + 1;
  default:
    /* Broken lock */
    return sizeof *b;
  }
}

/** Print out a decompiled-to-assembly bytecode to stdout.
 * \param b the boolexp to decompile.
 */
static void
print_bytecode(boolexp b)
{
  bvm_opcode op;
  int arg, len = 0, pos = 0;
  char *pc, *bytecode;

  if (b == TRUE_BOOLEXP) {
    puts("NULL bytecode!");
    return;
  }

  pc = bytecode = get_bytecode(b, &len);

  printf("Total length of bytecode+strings: %d bytes\n", len);

  while (1) {
    op = (bvm_opcode) *pc;
    memcpy(&arg, pc + 1, sizeof arg);
    pc += INSN_LEN;
    printf("%-5d ", pos);
    pos++;
    switch (op) {
    case OP_RET:
      puts("RET");
      return;
    case OP_PAREN:
      printf("PAREN %c\n", (arg == 0) ? '(' : ((arg == 1) ? ')' : '!'));
      break;
    case OP_JMPT:
      printf("JMPT %d\n", arg / INSN_LEN);
      break;
    case OP_JMPF:
      printf("JMPF %d\n", arg / INSN_LEN);
      break;
    case OP_TCONST:
      printf("TCONST #%d\n", arg);
      break;
    case OP_TCARRY:
      printf("TCARRY #%d\n", arg);
      break;
    case OP_TIS:
      printf("TIS #%d\n", arg);
      break;
    case OP_TOWNER:
      printf("TOWNER #%d\n", arg);
      break;
    case OP_TIND:
      printf("TIND #%d\n", arg);
      break;
    case OP_TATR:
      printf("TATR \"%s\"\n", bytecode + arg);
      break;
    case OP_TEVAL:
      printf("TEVAL \"%s\"\n", bytecode + arg);
      break;
    case OP_TFLAG:
      printf("TFLAG \"%s\"\n", bytecode + arg);
      break;
    case OP_TPOWER:
      printf("TPOWER \"%s\"\n", bytecode + arg);
      break;
    case OP_TOBJID:
      printf("TOBJID \"%s\"\n", bytecode + arg);
      break;
    case OP_TTYPE:
      printf("TTYPE \"%s\"\n", bytecode + arg);
      break;
    case OP_TCHANNEL:
      printf("TCHANNEL \"%s\"\n", bytecode + arg);
      break;
    case OP_TIP:
      printf("TIP \"%s\"\n", bytecode + arg);
      break;
    case OP_THOSTNAME:
      printf("THOSTNAME \"%s\"\n", bytecode + arg);
      break;
    case OP_TDBREFLIST:
      printf("TDBREFLIST \"%s\"\n", bytecode + arg);
      break;
    case OP_LOADS:
      printf("LOADS \"%s\"\n", bytecode + arg);
      break;
    case OP_LOADR:
      printf("LOADR %d\n", arg);
      break;
    case OP_NEGR:
      puts("NEGR");
      break;
    default:
      printf("Hmm: %d %d\n", op, arg);
    }
  }
}
#endif

/* Warnings-related stuff here because I don't want to export details
   of the bytecode outside this file. */
#define W_UNLOCKED      0x1  /**< Returned if a boolexp is unlocked */
#define W_LOCKED        0x2  /**< Returned if a boolexp is locked */

/** Check to see if a lock is considered possibly unlocked or not.
 *  This is really simple-minded for efficiency. Basically, if it's *
 *  unlocked, it's unlocked. If it's locked to something starting with
 *  a specific db#, it's locked. Anything else, and we don't know.
 *  \param l the boolexp to check.
 *  \retval W_UNLOCKED the boolexp is unlocked.
 *  \retval W_LOCKED the boolexp is considered locked.
 *  \retval W_LOCKED|W_UNLOCKED the boolexp is in an unknown state.
 */
int
warning_lock_type(const boolexp l)
     /* 0== unlocked. 1== locked, 2== sometimes */
{
  if (l == TRUE_BOOLEXP)
    return W_UNLOCKED;
  /* Two instructions means one of the simple lock cases */
  else if (sizeof_boolexp(l) == (INSN_LEN + INSN_LEN))
    return W_LOCKED;
  else
    return W_LOCKED | W_UNLOCKED;
}

/** Check for lock-check @warnings.
 * Things like non-existant attributes in eval locks, references to
 * garbage objects, or indirect locks that aren't present or visible.
 * \param player the object to report warnings to.
 * \param i the object the lock is on.
 * \param name the lock type.
 * \param be the lock key.
 */
void
check_lock(dbref player, dbref i, const char *name, boolexp be)
{
  unsigned char *pc, *bytecode;
  bvm_opcode op;
  int arg;
  char *s = NULL;

  bytecode = pc = get_bytecode(be, NULL);

  while (1) {
    op = (bvm_opcode) *pc;
    memcpy(&arg, pc + 1, sizeof arg);
    pc += INSN_LEN;
    switch (op) {
    case OP_RET:
      return;
    case OP_LOADS:
      s = (char *) bytecode + arg;
      break;
    case OP_TCONST:
    case OP_TCARRY:
    case OP_TIS:
    case OP_TOWNER:
      if (!GoodObject(arg) || IsGarbage(arg))
        complain(player, i, "lock-checks",
                 T("%s lock refers to garbage object"), name);
      break;
    case OP_TEVAL:
      {
        ATTR *a;
        a = atr_get(i, s);
        if (!a || !Can_Read_Attr(i, i, a))
          complain(player, i, "lock-checks",
                   T
                   ("%s lock has eval-lock that uses a nonexistant attribute '%s'."),
                   name, s);
      }
      break;
    case OP_TIND:
      if (!GoodObject(arg) || IsGarbage(arg))
        complain(player, i, "lock-checks",
                 T("%s lock refers to garbage object"), name);
      else if (!(Can_Read_Lock(i, arg, s) && getlock(arg, s) != TRUE_BOOLEXP))
        complain(player, i, "lock-checks",
                 T("%s lock has indirect lock to %s/%s that it can't read"),
                 name, unparse_object(player, arg), s);
      break;
    default:
      break;
    }
  }
}
