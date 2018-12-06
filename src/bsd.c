/**
 * \file bsd.c
 *
 * \brief Network communication through BSD sockets for PennMUSH.
 *
 * While mysocket.c provides low-level functions for working with
 * sockets, bsd.c focuses on player descriptors, a higher-level
 * structure that tracks all information associated with a connection,
 * and through which connection i/o is done.
 *
 *
 */

#include "copyrite.h"

#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <errno.h>
#include <process.h>
#else /* !WIN32 */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#include <sys/ioctl.h>
#include <errno.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#endif /* !WIN32 */
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <limits.h>
#include <locale.h>
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#include <setjmp.h>
#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif
#include <openssl/rand.h>

#include "access.h"
#include "ansi.h"
#include "attrib.h"
#include "chunk.h"
#include "command.h"
#include "conf.h"
#include "dbdefs.h"
#include "dbio.h"
#include "externs.h"
#include "extmail.h"
#include "flags.h"
#include "game.h"
#include "help.h"
#include "htab.h"
#include "intmap.h"
#include "lock.h"
#include "log.h"
#include "match.h"
#include "mushdb.h"
#include "mymalloc.h"
#include "mypcre.h"
#include "mysocket.h"
#include "myssl.h"
#include "notify.h"
#include "parse.h"
#include "pueblo.h"
#include "sig.h"
#include "strtree.h"
#include "strutil.h"
#include "version.h"
#include "charconv.h"
#include "mushsql.h"
#include "connlog.h"
#include "charclass.h"
#include "cJSON.h"
#include "memcheck.h"
#include "map_file.h"
#include "tests.h"
#include "websock.h"
#include "log.h"

#ifndef WIN32
#include "wait.h"
#ifdef INFO_SLAVE
#include "lookup.h"
#endif
#ifdef SSL_SLAVE
#include "ssl_slave.h"
#endif
#endif /* !WIN32 */

#if defined(SSL_SLAVE) && !defined(WIN32)
#define LOCAL_SOCKET 1
#endif

#ifdef HAVE_GETRLIMIT
void init_rlimit(void);
#endif

/* BSD 4.2 and maybe some others need these defined */
#ifndef FD_ZERO
/** An fd_set is 4 bytes */
#define fd_set int
/** Clear an fd_set */
#define FD_ZERO(p) (*p = 0)
/** Set a bit in an fd_set */
#define FD_SET(n, p) (*p |= (1 << (n)))
/** Clear a bit in an fd_set */
#define FD_CLR(n, p) (*p &= ~(1 << (n)))
/** Check a bit in an fd_set */
#define FD_ISSET(n, p) (*p & (1 << (n)))
#endif /* defines for BSD 4.2 */

#ifdef HAVE_GETRUSAGE
void rusage_stats(void);
#endif
uint64_t queue_msecs_till_next(void); /* from cque.c */
void queue_update(void);              /* from cque.c */
void update_queue_load();

dbref email_register_player(DESC *d, const char *name, const char *email,
                            const char *host,
                            const char *ip); /* from player.c */

int shutdown_flag = 0; /**< Is it time to shut down? */
void chat_player_announce(DESC *desc_player, char *msg, int ungag);
void report_mssp(DESC *d, char *buff, char **bp);

static int login_number = 0;
static int under_limit = 1;

static bool disable_socket_quota = false;

char cf_motd_msg[BUFFER_LEN] = {'\0'};     /**< The message of the day */
char cf_wizmotd_msg[BUFFER_LEN] = {'\0'};  /**< The wizard motd */
char cf_downmotd_msg[BUFFER_LEN] = {'\0'}; /**< The down message */
char cf_fullmotd_msg[BUFFER_LEN] = {'\0'}; /**< The 'mush full' message */
static char poll_msg[DOING_LEN] = {'\0'};  /**< The \@poll/"Doing" message */
char confname[BUFFER_LEN] = {'\0'};        /**< Name of the config file */

char *get_poll(void);
int set_poll(const char *message);

char *etime_fmt(char *, time_t, int);
const char *source_to_s(conn_source source);
bool fcache_read_one(const char *filename);

/** Is this descriptor connected to a telnet-compatible terminal? */
#define TELNET_ABLE(d) ((d)->conn_flags & (CONN_TELNET | CONN_TELNET_QUERY))
/** Is it possible this descriptor may be telnet-compatible? */
#define MAYBE_TELNET_ABLE(d)                                                   \
  ((d)->conn_flags &                                                           \
   (CONN_TELNET | CONN_TELNET_QUERY | CONN_AWAITING_FIRST_DATA))

const char *default_ttype = "unknown";
#define REBOOT_DB_NOVALUE "__NONE__"

/* Just to avoid '1000' appearing everywhere without a reason */
#define MS_PER_SEC 1000

#define QUOTA_MAX (COMMAND_BURST_SIZE * MS_PER_SEC)

/* When the mush gets a new connection, it tries sending a telnet
 * option negotiation code for setting client-side line-editing mode
 * to it. If it gets a reply, a flag in the descriptor struct is
 * turned on indicated telnet-awareness.
 *
 * If the reply indicates that the client supports linemode, further
 * instructions as to what linemode options are to be used is sent.
 * Those options: Client-side line editing, and expanding literal
 * client-side-entered tabs into spaces.
 *
 * Option negotation requests sent by the client are processed,
 * with the only one we confirm rather than refuse outright being
 * suppress-go-ahead, since a number of telnet clients try it.
 *
 * The character 255 is the telnet option escape character, so when it
 * is sent to a telnet-aware client by itself (Since it's also often y-umlaut)
 * it must be doubled to escape it for the client. This is done automatically,
 * and is the original purpose of adding telnet option support.
 */

static void test_telnet(DESC *d);
static void setup_telnet(DESC *d);
bool test_telnet_wrapper(void *data);
bool welcome_user_wrapper(void *data);
static int handle_telnet(DESC *d, char **q, char *qend);
static void set_ttype(DESC *d, char *value);
bool http_finished_wrapper(void *data);

typedef void (*telnet_handler)(DESC *d, char *cmd, int len);
#define TELNET_HANDLER(x)                                                      \
  void x(DESC *d __attribute__((__unused__)),                                  \
         char *cmd __attribute__((__unused__)),                                \
         int len __attribute__((__unused__)));                                 \
  void x(DESC *d __attribute__((__unused__)),                                  \
         char *cmd __attribute__((__unused__)),                                \
         int len __attribute__((__unused__)))

struct telnet_opt {
  int optcode; /**< Code for this telnet option */
  int offer;   /**< One of DO or WILL, to offer this during initial negotiation,
                  or 0 to not */
  telnet_handler handler; /**< Function to run on DO/WILL for this opt */
  telnet_handler
    sb; /**< Function to run for subnegotiation requests for this opt */
};

/** Array of all possible telnet options */
struct telnet_opt *telnet_options[256];
char *starting_telnet_neg = NULL;
int starting_telnet_neg_len = 0;

struct gmcp_handler *gmcp_handlers = NULL;

/** Iterate through a list of descriptors, and do something with those
 * that are connected.
 */
#define DESC_ITER_CONN(d)                                                      \
  for (d = descriptor_list; (d); d = (d)->next)                                \
    if ((d)->connected)

#define DESC_ITER(d) for (d = descriptor_list; (d); d = (d)->next)

/** Is a descriptor hidden? */
#define Hidden(d) ((d->hide == 1))

static const char create_fail_preexisting[] =
  "There is already a player with that name.";
static const char create_fail_bad[] = "That name is not allowed.";
static const char password_fail[] = "The password is invalid (or missing).";
static const char register_fail[] =
  "Unable to register that player with that email address.";
static const char register_success[] =
  "Registration successful! You will receive your password by email.";
static const char shutdown_message[] = "Going down - Bye";
static const char asterisk_line[] =
  "**********************************************************************";
/** Where we save the descriptor info across reboots. */
#define REBOOTFILE "reboot.db"

static void sockset_wrapper(DESC *d, char *cmd);

extern int notify_type(DESC *d); /* from notify.c */
#if 0
/* For translation */
static void dummy_msgs(void);
static void
dummy_msgs()
{
  char *temp;
  temp = T("There is already a player with that name.");
  temp = T("That name is not allowed.");
  temp = T("The password is invalid (or missing).");
  temp = T("Unable to register that player with that email address.");
  temp = T("Registration successful! You will receive your password by email.");
  temp = T("Going down - Bye");
  temp = T("GAME: SSL connections must be dropped, sorry.");
}

#endif

DESC *descriptor_list = NULL; /**< The linked list of descriptors */
intmap *descs_by_fd = NULL;   /**< Map of ports to DESC* objects */

struct http_request *active_http_request = NULL; /**< Active HTTP Request */
/* To roughly average HTTP_SECOND_LIMIT per second, we actually define
 * an http request as MS_PER_SEC http_quota, and every millisecond "adds"
 * HTTP_SECOND_LIMIT.
 *
 * So after 83 milliseconds, if HTTP_SECOND_LIMIT is 50, then we add 4150 to
 * the quota - essentially 4 additional requests. But it caps at
 * HTTP_SECOND_LIMIT * 1000
 */
static int http_quota = 0;

static int sock;
static int sslsock = 0;
SSL *ssl_master_socket = NULL; /**< Master SSL socket for ssl port */
static const char ssl_shutdown_message[] __attribute__((__unused__)) =
  "GAME: SSL connections must be dropped, sorry.";
#ifdef LOCAL_SOCKET
static int localsock = -1;
#endif
static int ndescriptors = 0;
#ifdef WIN32
static WSADATA wsadata;
#endif
int restarting = 0; /**< Are we restarting the server after a reboot? */
int maxd = 0;

extern const unsigned char *tables;
extern void pi_regs_normalize_key(char *lckey);

volatile sig_atomic_t signal_shutdown_flag =
  0; /**< Have we caught a shutdown signal? */
volatile sig_atomic_t usr1_triggered = 0; /**< Have we caught a USR1 signal? */
volatile sig_atomic_t usr2_triggered = 0; /**< Have we caught a USR2 signal? */
volatile sig_atomic_t hup_triggered = 0;  /**< Have we caught a HUP signal? */

#ifndef BOOLEXP_DEBUGGING
#ifdef WIN32SERVICES
void shutdown_checkpoint(void);
int mainthread(int argc, char **argv);
#else
int main(int argc, char **argv);
#endif
#endif
void set_signals(void);
static long int msec_diff(struct timeval now, struct timeval then);
static void update_quotas(struct timeval current);

int how_many_fds(void);
static void open_ports(Port_t port, Port_t sslport);
static void gameloop();
static void ext_startup();
static void ext_shutdown();

#ifndef WIN32
typedef int SOCKET;
#endif

static int test_connection(SOCKET newsock);
static DESC *new_connection(int oldsock, int *result, conn_source source);

static void clearstrings(DESC *d);

/** A block of cached text. */
typedef struct fblock {
  char *buff;  /**< Pointer to the block as a string */
  size_t len;  /**< Length of buff */
  dbref thing; /**< If NOTHING, display buff as raw text. Otherwise, buff is an
                  attrname on thing to eval and display */
} FBLOCK;

/** The complete collection of cached text files. */
struct fcache_entries {
  FBLOCK connect_fcache[2];  /**< connect.txt and connect.html */
  FBLOCK motd_fcache[2];     /**< motd.txt and motd.html */
  FBLOCK wizmotd_fcache[2];  /**< wizmotd.txt and wizmotd.html */
  FBLOCK newuser_fcache[2];  /**< newuser.txt and newuser.html */
  FBLOCK register_fcache[2]; /**< register.txt and register.html */
  FBLOCK quit_fcache[2];     /**< quit.txt and quit.html */
  FBLOCK down_fcache[2];     /**< down.txt and down.html */
  FBLOCK full_fcache[2];     /**< full.txt and full.html */
  FBLOCK guest_fcache[2];    /**< guest.txt and guest.html */
  FBLOCK who_fcache[2];      /**< textfiles to override connect screen WHO */
};

static struct fcache_entries fcache;
static bool fcache_dump(DESC *d, FBLOCK fp[2], const char *prefix, char *arg);
static int fcache_dump_attr(DESC *d, dbref thing, const char *attr, int html,
                            const char *prefix, char *arg);
static int fcache_read(FBLOCK *cp, const char *filename);
static void logout_sock(DESC *d);
static void shutdownsock(DESC *d, const char *reason, dbref executor,
                         int flags);
static void disconnect_desc(DESC *d);
static void cleanup_desc(DESC *d);
DESC *initializesock(int s, char *addr, char *ip, conn_source source);
int process_output(DESC *d);
/* Notify.c */
void free_text_block(struct text_block *t);
void init_text_queue(struct text_queue *);
void add_to_queue(struct text_queue *q, const char *b, int n);
int queue_write(DESC *d, const char *b, int n);
int queue_eol(DESC *d);
int queue_string(DESC *d, const char *s);
int WIN32_CDECL queue_string_eol(DESC *d, const char *s, ...)
  __attribute__((__format__(__printf__, 2, 3)));
void freeqs(DESC *d);
static void welcome_user(DESC *d, int telnet);
static int count_players(void);
static void dump_info(DESC *call_by);
static void save_command(DESC *d, char *command);
static int process_input(DESC *d, int output_ready);
static void process_input_helper(DESC *d, char *tbuf1, int got);
static bool is_http_request(const char *command);
static bool is_http_bodyless(const char *method);
static int process_http_start(DESC *d, char *command);
static void process_http_input(DESC *d, char *buf, int len);
static void http_command_ready(DESC *d);
static void do_http_command(DESC *d);
static void set_userstring(char **userstring, const char *command);
static void process_commands(void);
enum comm_res {
  CRES_OK = 0,
  CRES_LOGOUT,
  CRES_QUIT,
  CRES_SITELOCK,
  CRES_BOOTED
};
static enum comm_res do_command(DESC *d, char *command);
static void parse_puebloclient(DESC *d, char *command);
static int dump_messages(DESC *d, dbref player, int new);
static int check_connect(DESC *d, const char *msg);
static void parse_connect(const char *msg, char *command, char *user,
                          char *pass);
static void close_sockets(void);
dbref find_player_by_desc(int port);
DESC *lookup_desc(dbref executor, const char *name);
void WIN32_CDECL bailout(int sig);
#ifndef WIN32
void signal_shutdown(int sig);
void signal_dump(int sig);
void hup_handler(int);
void usr1_handler(int);
void reaper(int sig);
#endif
#ifndef WIN32
volatile sig_atomic_t dump_error = 0;
WAIT_TYPE dump_status = 0;
#ifdef INFO_SLAVE
volatile sig_atomic_t slave_error = 0;
#endif
#ifdef SSL_SLAVE
volatile sig_atomic_t ssl_slave_error = 0;
extern bool ssl_slave_halted;
#endif
WAIT_TYPE error_code = 0;
#endif
extern pid_t forked_dump_pid; /**< Process id of forking dump process */
static void dump_users(DESC *call_by, char *match);
static char *onfor_time_fmt(time_t at, int len);
static char *idle_time_fmt(time_t last, int len);
static void announce_connect(DESC *d, int isnew, int num);
static void announce_disconnect(DESC *saved, const char *reason,
                                dbref executor);
enum disconn_reason { DISCONNECT_LOGOUT = 0, DISCONNECT_QUIT = 1 };

static void disconnect_player(DESC *d, enum disconn_reason reason);
bool inactivity_check(void);
void load_reboot_db(void);

static bool in_suid_root_mode __attribute__((__unused__)) = 0;
static char *pidfile = NULL;
static char **saved_argv = NULL;

int file_watch_init(void);
void file_watch_event(int);

static char *get_doing(dbref player, dbref caller, dbref enactor,
                       NEW_PE_INFO *pe_info, bool full);

static bool who_check_name(DESC *d, char *name, bool wild);

#ifndef BOOLEXP_DEBUGGING
#ifdef WIN32SERVICES
/* Under WIN32, MUSH is a "service", so we just start a thread here.
 * The real "main" is in win32/services.c
 */
int
mainthread(int argc, char **argv)
#else
/** The main function.
 * \param argc number of arguments.
 * \param argv vector of arguments.
 * \return exit code.
 */
int
main(int argc, char **argv)
#endif /* WIN32SERVICES */
{
  FILE *newerr;
  bool detach_session __attribute__((__unused__)) = 1;
  bool enable_tests = 0, only_test = 0;

/* disallow running as root on unix.
 * This is done as early as possible, before translation is initialized.
 * Hence, no T()s around messages.
 */
#ifndef WIN32
#ifdef HAVE_GETUID
  if (getuid() == 0) {
    fputs("Please run the server as another user.\n", stderr);
    fputs("PennMUSH will not run as root as a security measure. Exiting.\n",
          stderr);
    return EXIT_FAILURE;
  }
/* Add suid-root checks here. */
#endif
#ifdef HAVE_GETEUID
  if (geteuid() == 0) {
    fprintf(stderr, "The  %s binary is set suid and owned by root.\n", argv[0]);
#ifdef HAVE_SETEUID
    fprintf(stderr, "Changing effective user to %d.\n", (int) getuid());
    if (seteuid(getuid()) < 0) {
      fprintf(stderr, "ERROR: seteuid() failed: %s\n", strerror(errno));
      fputs("PennMUSH will not run as root as a security measure. Exiting.\n",
            stderr);
      return EXIT_FAILURE;
    } else
      in_suid_root_mode = 1;
#endif
  }
#endif /* HAVE_GETEUID */
#endif /* !WIN32 */

#ifdef HAVE_PLEDGE
  if (pledge(
        "stdio rpath wpath cpath inet flock unix dns proc exec id prot_exec",
        NULL) < 0) {
    perror("pledge"); /* Happens before logfiles are opened; no penn_perror() */
  }
#endif

  /* read the configuration file */
  if (argc < 2) {
    fprintf(
      stderr,
      "WARNING: Called without a config file argument. Assuming mush.cnf\n");
    strcpy(confname, "mush.cnf");
  } else {
    int n;
    for (n = 1; n < argc; n++) {
      if (argv[n][0] == '-') {
        if (strcmp(argv[n], "--no-session") == 0)
          detach_session = 0;
        else if (strcmp(argv[n], "--disable-socket-quota") == 0) {
          disable_socket_quota = true;
        } else if (strncmp(argv[n], "--pid-file", 10) == 0) {
          char *eq;
          if ((eq = strchr(argv[n], '=')))
            pidfile = eq + 1;
          else {
            if (n + 1 >= argc) {
              fprintf(stderr, "%s: --pid-file needs a filename.\n", argv[0]);
              return EXIT_FAILURE;
            }
            pidfile = argv[n + 1];
            n++;
          }
        } else if (strcmp(argv[n], "--no-pcre-jit") == 0) {
          re_match_flags = PCRE2_NO_JIT;
        } else if (strcmp(argv[n], "--tests") == 0) {
          enable_tests = 1;
        } else if (strcmp(argv[n], "--only-tests") == 0) {
          enable_tests = 1;
          only_test = 1;
          detach_session = 0;
        } else {
          fprintf(stderr, "%s: unknown option \"%s\"\n", argv[0], argv[n]);
        }
      } else {
        mush_strncpy(confname, argv[n], BUFFER_LEN);
        break;
      }
    }
  }

#ifdef HAVE_FORK
  /* Fork off and detach from controlling terminal. */
  if (detach_session) {
    pid_t child;

    child = fork();
    if (child < 0) {
      /* Print a warning and continue */
      penn_perror("fork");
    } else if (child > 0) {
      /* Parent process of a successful fork() */
      return EXIT_SUCCESS;
    } else {
      /* Child process */
      if (new_process_session() < 0)
        penn_perror("Couldn't create a new process session");
    }
  }
#endif

#ifdef HAVE_GETPID
  if (pidfile) {
    FILE *f;
    if (!(f = fopen(pidfile, "w"))) {
      fprintf(stderr, "%s: Unable to write to pidfile '%s'\n", argv[0],
              pidfile);
      return EXIT_FAILURE;
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
  }
#endif

  saved_argv = argv;

#ifdef WIN32
  {
    unsigned short wVersionRequested = MAKEWORD(1, 1);
    int err;

    /* Need to include library: wsock32.lib for Windows Sockets */
    err = WSAStartup(wVersionRequested, &wsadata);
    if (err) {
      printf("Error %i on WSAStartup\n", err);
      exit(1);
    }
  }
#endif /* WIN32 */

#ifdef HAVE_GETRLIMIT
  init_rlimit(); /* unlimit file descriptors */
#endif

  time(&mudtime);

#ifdef HAVE_LIBCURL
  curl_global_init(CURL_GLOBAL_ALL);
#endif

  /* initialize random number generator */
  initialize_rng();

  options.mem_check = 1;

  init_game_config(confname);

#ifdef HAVE_RAND_KEEP_RANDOM_DEVICES_OPEN
  /* OpenSSL leaks a couple of file descriptors on every reboot without this. */
  RAND_keep_random_devices_open(0);
#endif

  /* If we have setlocale, call it to set locale info
   * from environment variables
   */
  {
    char *loc;
    if ((loc = setlocale(LC_CTYPE, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set ctype locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting ctype locale to %s", loc);
    if ((loc = setlocale(LC_TIME, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set time locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting time locale to %s", loc);
#ifdef LC_MESSAGES
    if ((loc = setlocale(LC_MESSAGES, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set messages locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting messages locale to %s", loc);
#else
    do_rawlog(LT_ERR, "No support for message locale.");
#endif
    if ((loc = setlocale(LC_COLLATE, "")) == NULL)
      do_rawlog(LT_ERR, "Failed to set collate locale from environment.");
    else
      do_rawlog(LT_ERR, "Setting collate locale to %s", loc);
  }
#ifndef DONT_TRANSLATE
#ifdef HAVE_TEXTDOMAIN
  textdomain("pennmush");
#endif
#ifdef HAVE_BINDTEXTDOMAIN
  bindtextdomain("pennmush", "../po");
#endif
#endif

  /* Build the contexts used by PCRE2 */
  re_compile_ctx = pcre2_compile_context_create(NULL);
  re_match_ctx = pcre2_match_context_create(NULL);
  glob_convert_ctx = pcre2_convert_context_create(NULL);
  pcre2_set_character_tables(re_compile_ctx, pcre2_maketables(NULL));
  pcre2_set_match_limit(re_match_ctx, PENN_MATCH_LIMIT);
  pcre2_set_heap_limit(re_match_ctx, 10 * 1024); // 10MB max heap memory
  pcre2_set_glob_escape(glob_convert_ctx, '\\');
  pcre2_set_glob_separator(glob_convert_ctx, '`');

  /* save a file descriptor */
  reserve_fd();

  /* decide if we're in @shutdown/reboot */
  restarting = 0;
  newerr = fopen(REBOOTFILE, "r");
  if (newerr) {
    restarting = 1;
    fclose(newerr);
  }
#ifdef LOCAL_SOCKET
  if (!restarting) {
    localsock = make_unix_socket(options.socket_file, SOCK_STREAM);
    if (localsock >= maxd)
      maxd = localsock + 1;
  }
#endif

  if (!init_conndb(restarting)) {
    do_rawlog(LT_ERR, "ERROR: Couldn't initialize connlog! Exiting.");
    exit(2);
  }

  if (init_game_dbs() < 0) {
    do_rawlog(LT_ERR, "ERROR: Couldn't load databases! Exiting.");
    exit(2);
  }

  init_game_postdb(confname);

  globals.database_loaded = 1;

  set_signals();

  if (enable_tests) {
    bool r = run_tests();
    if (r) {
      do_rawlog(LT_ERR, "Hardcode tests all passed!");
    } else {
      do_rawlog(LT_ERR, "Hardcode tests had failures!");
    }
    if (only_test || !r) {
      exit(r ? 0 : 1);
    }
  }

#ifdef INFO_SLAVE
  init_info_slave();
#endif

  descs_by_fd = im_new();

  if (restarting) {
    /* go do it */
    load_reboot_db();
  }

  /* Call Local Startup */
  local_startup();
  /* everything else ok. Restart all objects. */
  do_restart();

  init_sys_events();

  open_ports(TINYPORT, SSLPORT);

  /* start up anything 'external' */
  ext_startup();

  /* Enter the main game loop */
  gameloop();

  /* Shut anything 'external' down */
  ext_shutdown();

/* someone has told us to shut down */
#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

  shutdown_queues();

#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

  close_sockets();

  sql_shutdown();

#ifdef INFO_SLAVE
  kill_info_slave();
#endif

#ifdef SSL_SLAVE
  kill_ssl_slave();
#endif
#ifdef LOCAL_SOCKET
  if (localsock >= 0) {
    closesocket(localsock);
    unlink(options.socket_file);
  }
#endif

#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

  dump_database();

  local_shutdown();

#ifdef HAVE_LIBCURL
  curl_global_cleanup();
#endif

  if (pidfile)
    remove(pidfile);

#ifdef WIN32SERVICES
  /* Keep service manager happy */
  shutdown_checkpoint();
#endif

#ifdef HAVE_GETRUSAGE
  rusage_stats();
#endif /* HAVE_GETRUSAGE */

  close_help_files();

  log_mem_check();

  do_rawlog(LT_ERR, "MUSH shutdown completed.");

  end_all_logs();

  close_shared_db();

  closesocket(sock);
#ifdef WIN32
#ifdef WIN32SERVICES
  shutdown_checkpoint();
#endif
  WSACleanup(); /* clean up */
#endif
  exit(0);
}
#endif /* BOOLEXP_DEBUGGING */

/** Install our default signal handlers. */
void
set_signals(void)
{

#ifndef WIN32
  /* we don't care about SIGPIPE, we notice it in select() and write() */
  ignore_signal(SIGPIPE);
  install_sig_handler(SIGHUP, hup_handler);
  install_sig_handler(SIGUSR1, usr1_handler);
  install_sig_handler(SIGUSR2, signal_dump);
  install_sig_handler(SIGINT, signal_shutdown);
  install_sig_handler(SIGTERM, bailout);
  install_sig_handler(SIGCHLD, reaper);
  sigrecv_setup();
#else
/* Win32 stuff:
 *   No support for SIGUSR2 or SIGINT.
 *   SIGTERM is never generated on NT-based Windows (according to MSDN)
 *   MSVC++ will let you get away with installing a handler anyway,
 *   but VS.NET will not. So if it's MSVC++, we give it a try.
 */
#if _MSC_VER < 1200
  install_sig_handler(SIGTERM, bailout);
#endif
#endif
}

/** Return the difference between two timeval structs in milliseconds.
 * \param now the timeval to subtract from.
 * \param then the timeval to subtract.
 * \return milliseconds of difference between them.
 */
static long int
msec_diff(struct timeval now, struct timeval then)
{
  long int msecs = 0;

  msecs = 1000 * (now.tv_sec - then.tv_sec);
  msecs += (now.tv_usec / 1000);
  msecs -= (then.tv_usec / 1000);
  if (msecs < 0)
    return 0;
  return msecs;
}

/** Update each descriptor's allowed rate of issuing commands.
 * Players are rate-limited; they may only perform up to a certain
 * number of commands per time slice. This function is run periodically
 * to refresh each descriptor's available command quota based on how
 * many slices have passed since it was last updated.
 * \param last pointer to timeval struct of last time quota was updated.
 * \param current pointer to timeval struct of current time.
 */
static void
update_quotas(struct timeval current)
{
  static struct timeval last = {0, 0};
  DESC *d;
  uint64_t msecs;

  if (!last.tv_sec) {
    /* First run */
    last = current;
    return;
  }

  msecs = msec_diff(current, last);
  last = current;

  DESC_ITER (d) {
    d->quota += COMMANDS_PER_SECOND * msecs;
    if (d->quota > QUOTA_MAX)
      d->quota = QUOTA_MAX;
  }

  /* And the HTTP quota */
  http_quota += (msecs * HTTP_SECOND_LIMIT);
  if (http_quota > (HTTP_SECOND_LIMIT * MS_PER_SEC)) {
    http_quota = HTTP_SECOND_LIMIT * MS_PER_SEC;
  }
}

int
http_msecs_till_next()
{
  if (http_quota < MS_PER_SEC && HTTP_SECOND_LIMIT > 0) {
    /* Quota is exhausted. Calculate how long until we can serve an http
     * command again. */
    return ((MS_PER_SEC - http_quota) / HTTP_SECOND_LIMIT) + HTTP_SECOND_LIMIT;
  }
  /* Arbitarily high */
  return SECS_TO_MSECS(500);
}

extern slab *text_block_slab;

/* Is source one an IP connection? */
static inline bool
is_remote_source(conn_source source)
{
  return source == CS_IP_SOCKET || source == CS_OPENSSL_SOCKET;
}

static inline bool
is_remote_desc(DESC *d)
{
  if (!d)
    return 0;
  return is_remote_source(d->source);
}

/** Is a descriptor using SSL? */
static inline bool
is_ssl_desc(DESC *d)
{
  if (!d)
    return 0;
  return d->source == CS_OPENSSL_SOCKET || d->source == CS_LOCAL_SSL_SOCKET;
}

/** Is a descriptor using a websocket? */
static inline bool
is_ws_desc(DESC *d)
{
  if (!d) {
    return 0;
  }
  return IsWebSocket(d);
}

static void
setup_desc(int sockfd, conn_source source)
{
  DESC *newd;
  int result;

  if (!(newd = new_connection(sockfd, &result, source))) {
    if (test_connection(result) < 0)
      return;
  } else {
    ndescriptors++;
    if (newd->descriptor >= maxd)
      maxd = newd->descriptor + 1;
  }
}

#ifdef INFO_SLAVE

static void
got_new_connection(int sockfd, conn_source source)
{
  union sockaddr_u addr;
  socklen_t addr_len;
  int newsock;

  if (!info_slave_halted) {
    addr_len = sizeof(addr);
    newsock = accept(sockfd, (struct sockaddr *) &addr, &addr_len);
    if (newsock < 0) {
      if (test_connection(newsock) < 0)
        return;
    }
    ndescriptors++;
    query_info_slave(newsock);
    if (newsock >= maxd)
      maxd = newsock + 1;
  } else {
    setup_desc(sockfd, source);
  }
}

#endif

#if defined(INFO_SLAVE) || defined(SSL_SLAVE)
static char *
exit_report(const char *prog, pid_t pid, WAIT_TYPE code)
{
  static char buffer[BUFFER_LEN], *bp;
  bp = buffer;
  safe_format(buffer, &bp, "%s (PID %d) exited ", prog, pid);
  if (WIFEXITED(code))
    safe_format(buffer, &bp, "with code %d.", WEXITSTATUS(code));
  else if (WIFSIGNALED(code))
    safe_format(buffer, &bp, "with signal %d.", WTERMSIG(code));
  else
    safe_str("in an unknown fashion.", buffer, &bp);
  *bp = '\0';
  return buffer;
}
#endif

#ifdef HAVE_LIBCURL
int ncurl_queries = 0;
CURLM *curl_handle = NULL;

static void
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

static void
handle_curl_msg(CURLMsg *msg)
{
  if (!msg) {
    return;
  }

  ncurl_queries -= 1;

  if (msg->msg == CURLMSG_DONE) {
    long respcode;
    char *contenttype;
    struct urlreq *resp;
    CURL *handle = msg->easy_handle;
    bool is_utf8 = 0;

    curl_easy_getinfo(handle, CURLINFO_PRIVATE, &resp);

    if (msg->data.result == CURLE_OK || resp->too_big) {
      if (curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &respcode) ==
          CURLE_OK) {
        if (respcode) {
          pe_regs_set_int(resp->pe_regs, PE_REGS_Q, "status", respcode);
        }
      }
      if (curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &contenttype) ==
          CURLE_OK) {
        if (contenttype) {
          pe_regs_set(resp->pe_regs, PE_REGS_Q, "content-type", contenttype);
          if (strstr(contenttype, "charset=utf-8") ||
              strstr(contenttype, "charset=UTF-8")) {
            is_utf8 = 1;
          }
        }
      }
      if (resp->body && sqlite3_str_length(resp->body) > 0) {
        char *body = NULL;
        char *latin1 = NULL;
        int len;
        int body_size = sqlite3_str_length(resp->body);
        body = sqlite3_str_finish(resp->body);
        resp->body = NULL;
        if (is_utf8) {
          latin1 = utf8_to_latin1(body, body_size, &len, 1, "string");
          if (len >= BUFFER_LEN) {
            resp->too_big = 1;
            latin1[BUFFER_LEN - 1] = '\0';
          }
        } else {
          latin1 = body;
          if (body_size >= BUFFER_LEN) {
            resp->too_big = 1;
            body[BUFFER_LEN - 1] = '\0';
          }
        }
        pe_regs_setenv(resp->pe_regs, 0, latin1);
        if (is_utf8) {
          mush_free(latin1, "string");
        }
        if (body) {
          sqlite3_free(body);
        }
      }
      if (resp->too_big) {
        notify(resp->thing, "Too much HTTP data received; excess truncated.");
      }
      queue_attribute_base_priv(resp->thing, resp->attrname, resp->enactor, 0,
                                resp->pe_regs, resp->queue_type, resp->thing,
                                NULL, NULL);
    } else {
      notify_format(resp->thing, "Request failed: %s",
                    curl_easy_strerror(msg->data.result));
    }
    curl_multi_remove_handle(curl_handle, handle);
    curl_easy_cleanup(handle);
    free_urlreq(resp);
  }
}

#endif

/* Check for any errors and status changes, and let gameloop() know if it
 * needs to shut down.
 * \return 1 if everything's okay, 0 to shut down.
 */
static int
check_status()
{
/* Check signal handler flags */
#ifndef WIN32
  if (dump_error) {
    if (WIFSIGNALED(dump_status)) {
      do_rawlog(LT_ERR, "ERROR! forking dump exited with signal %d",
                WTERMSIG(dump_status));
      queue_event(SYSEVENT, "DUMP`ERROR", "%s,%d,SIGNAL %d",
                  T("GAME: ERROR! Forking database save failed!"), 1,
                  dump_status);
      flag_broadcast("ROYALTY WIZARD", 0,
                     T("GAME: ERROR! Forking database save failed!"));
    } else if (WIFEXITED(dump_status)) {
      if (WEXITSTATUS(dump_status) == 0) {
        time(&globals.last_dump_time);
        queue_event(SYSEVENT, "DUMP`COMPLETE", "%s,%d", DUMP_NOFORK_COMPLETE,
                    1);
        if (DUMP_NOFORK_COMPLETE && *DUMP_NOFORK_COMPLETE)
          flag_broadcast(0, 0, "%s", DUMP_NOFORK_COMPLETE);
      } else {
        do_rawlog(LT_ERR, "ERROR! forking dump exited with exit code %d",
                  WEXITSTATUS(dump_status));
        queue_event(SYSEVENT, "DUMP`ERROR", "%s,%d,EXIT %d",
                    T("GAME: ERROR! Forking database save failed!"), 1,
                    dump_status);
        flag_broadcast("ROYALTY WIZARD", 0,
                       T("GAME: ERROR! Forking database save failed!"));
      }
    }
    dump_error = 0;
    dump_status = 0;
  }
#ifdef INFO_SLAVE
  if (slave_error) {
    do_rawlog(LT_ERR, "%s", exit_report("info_slave", slave_error, error_code));
    slave_error = error_code = 0;
  }
#endif /* INFO_SLAVE */
#ifdef SSL_SLAVE
  if (ssl_slave_error) {
    do_rawlog(LT_ERR, "%s",
              exit_report("ssl_slave", ssl_slave_error, error_code));
    ssl_slave_error = error_code = 0;
    if (!ssl_slave_halted)
      make_ssl_slave();
  }
#endif /* SSL_SLAVE */
#endif /* !WIN32 */

  if (signal_shutdown_flag) {
    flag_broadcast(0, 0, T("GAME: Shutdown by external signal"));
    do_rawlog(LT_ERR, "SHUTDOWN by external signal");
    return 0;
  }

  if (hup_triggered) {
    do_rawlog(LT_ERR, "SIGHUP received: reloading .txt and .cnf files");
    config_file_startup(NULL, 0);
    config_file_startup(NULL, 1);
    file_watch_init();
    fcache_load(NOTHING);
    help_rebuild(NOTHING);
    read_access_file();
    reopen_logs();
    hup_triggered = 0;
  }

  if (usr1_triggered) {
    if (!queue_event(SYSEVENT, "SIGNAL`USR1", "%s", "")) {
      do_rawlog(LT_ERR, "SIGUSR1 received. Rebooting.");
      do_reboot(NOTHING, 0);
      /* We shouldn't return from this except in case of a failed db save. */
    }
  }

  if (usr2_triggered) {
    if (!queue_event(SYSEVENT, "SIGNAL`USR2", "%s", "")) {
      globals.paranoid_dump = 0;
      do_rawlog(LT_CHECK, "DUMP by external signal");
      fork_and_dump(1);
    }
    usr2_triggered = 0;
  }

  return 1;
}

void
shutdownsock(DESC *d, const char *reason, dbref executor, int flags)
{
  d->conn_flags |= CONN_SHUTDOWN | flags;
  d->close_reason = reason;
  d->closer = executor;
}

#define CONN_CLOSABLES                                                         \
  (CONN_SHUTDOWN | CONN_NOWRITE | CONN_CLOSE_READY | CONN_HTTP_CLOSE)

void
clean_descriptors(DESC **head)
{
  DESC *d = *head;
  DESC **listp = head;

  while (d) {
    if (d->conn_flags & (CONN_CLOSABLES)) {
      disconnect_desc(d);
      *listp = d->next;
      cleanup_desc(d);
    } else {
      listp = &(d->next);
    }
    d = *listp;
  }
}

void
open_ports(Port_t port, Port_t sslport)
{
  if (!restarting) {

    sock = make_socket(port, SOCK_STREAM, NULL, NULL, MUSH_IP_ADDR);
    if (sock >= maxd)
      maxd = sock + 1;

    if (sslport) {
#ifdef SSL_SLAVE
      if (make_ssl_slave() < 0)
        do_rawlog(LT_ERR, "Unable to start ssl_slave");
#else
      sslsock = make_socket(sslport, SOCK_STREAM, NULL, NULL, SSL_IP_ADDR);
      ssl_master_socket = ssl_setup_socket(sslsock);
      if (sslsock >= maxd)
        maxd = sslsock + 1;
#endif
    }
  }
}

static int avail_descriptors;
static int notify_fd = -1;

#ifdef HAVE_LIBCURL
static struct curl_waitfd *fds = NULL;
static unsigned int fd_size = 0, fds_used = 0;
static CURLMcode curl_status;
#define PENN_POLLIN CURL_WAIT_POLLIN
#define PENN_POLLOUT CURL_WAIT_POLLOUT
#else
#ifdef WIN32
static WSAPOLLFD *fds = NULL;
static ULONG fd_size = 0, fds_used = 0;
#else
static struct pollfd *fds = NULL;
static nfds_t fd_size = 0, fds_used = 0;
#endif
#define PENN_POLLIN POLLIN
#define PENN_POLLOUT POLLOUT
#endif

void
ext_startup()
{

#ifdef HAVE_LIBCURL
  curl_handle = curl_multi_init();
  curl_multi_setopt(curl_handle, CURLMOPT_MAXCONNECTS, 500);
#if CURL_VERSION_NUM >= 0x072B00 /* 7.43.0 */
  curl_multi_setopt(curl_handle, CURLMOPT_PIPELINING,
                    CURLPIPE_HTTP1 | CURLPIPE_MULTIPLEX);
#endif
#endif

  avail_descriptors = how_many_fds() - 5;
#ifdef INFO_SLAVE
  avail_descriptors -= 2; /* reserve some more for setting up the slave */
#endif

  /* done. print message to the log */
  do_rawlog(LT_ERR, "%d file descriptors available.", avail_descriptors);
  do_rawlog(LT_ERR, "RESTART FINISHED.");

  notify_fd = file_watch_init();
}

void
ext_shutdown()
{
  if (fds)
    mush_free(fds, "pollfds");

#ifdef HAVE_LIBCURL
  curl_multi_cleanup(curl_handle);
#endif
}

/* What previously used to be the largest chunk of gameloop(). This routine
 * handles all the network input, output, and checking, but never runs a
 * command, or interacts with softcode.
 *
 * It will wait for up to msec_timeout milliseconds. The only times it will
 * return in less than msec_timeout, will be because of network input, errors,
 * or a spamming user who's at their quota.
 * \param msec_timeout milliseconds to wait
 * \return 1 things are okay
 * \return 0 gotta shutdown
 */
int
check_sockets(uint32_t msec_timeout)
{
#ifdef INFO_SLAVE
  time_t now;
#endif
  int found;
  DESC *d;

  if (((int) fd_size) < ((int) im_count(descs_by_fd) + 6)) {
    fd_size = im_count(descs_by_fd) + 16;
    fds = mush_realloc(fds, sizeof *fds * fd_size, "pollfds");
  }
  fds_used = 0;

  /* Don't check for new connections if we're full up on players
   * we can't accept, anyway! */
  if (ndescriptors < avail_descriptors) {
    fds[fds_used].fd = sock;
    fds[fds_used++].events = PENN_POLLIN;

    if (sslsock) {
      fds[fds_used].fd = sslsock;
      fds[fds_used++].events = PENN_POLLIN;
    }
#ifdef LOCAL_SOCKET
    if (localsock >= 0) {
      fds[fds_used].fd = localsock;
      fds[fds_used++].events = PENN_POLLIN;
    }
#endif
  }

#ifdef INFO_SLAVE
  /** Only check info_slave socket if we're waiting for something
   * from it. */
  if (info_slave_state == INFO_SLAVE_PENDING) {
    fds[fds_used].fd = info_slave;
    fds[fds_used++].events = PENN_POLLIN;
  }
#endif

  /* notify_fd isn't always available, but if it is, it lets us know when
   * any of the game/txt/ files have changed. */
  if (notify_fd >= 0) {
    fds[fds_used].fd = notify_fd;
    fds[fds_used++].events = PENN_POLLIN;
  }

#ifndef WIN32
  if (sigrecv_fd >= 0) {
    fds[fds_used].fd = sigrecv_fd;
    fds[fds_used++].events = PENN_POLLIN;
  }
#endif

  /** Now add all the active descriptors */
  DESC_ITER (d) {
    /* If d->input.head is non-null, the descriptor is being throttled.
     * If d->output.head is non-null, the descriptor is choked on send,
     * we want to watch for POLLOUT event to write some more.
     * */
    int events = 0;

    if (d->input.head) {
      /* They're throttled, be nice and reduce timeout to when we think
       * they'll be unthrottled. */
      uint64_t curr = MS_PER_SEC - d->quota;
      if (msec_timeout > curr)
        msec_timeout = curr;
    } else {
      events |= PENN_POLLIN;
    }

    if (d->output.head) {
      events |= PENN_POLLOUT;
    }

    if (events) {
      fds[fds_used].events = events;
      fds[fds_used++].fd = d->descriptor;
    }
  }

#ifdef HAVE_LIBCURL
  curl_status =
    curl_multi_wait(curl_handle, fds, fds_used, msec_timeout, &found);

  if (curl_status != CURLM_OK) {
    do_rawlog(LT_ERR, "curl_multi_wait: %s", curl_multi_strerror(curl_status));
    return 0;
  }

  if (ncurl_queries > 0) {
    int running = 0;
    curl_status = curl_multi_perform(curl_handle, &running);
    if (curl_status == CURLM_OK) {
      CURLMsg *msg;
      while ((msg = curl_multi_info_read(curl_handle, &running)) != NULL) {
        handle_curl_msg(msg);
        found -= 1;
      }
    }
  }

#else

#ifdef WIN32
  found = WSAPoll(fds, fds_used, msec_timeout);
#else
  found = poll(fds, fds_used, msec_timeout);
#endif
  if (found < 0) {
#ifdef WIN32
    if (found == SOCKET_ERROR && WSAGetLastError() != WSAEINTR)
#else
    if (errno != EINTR)
#endif
    {
      penn_perror("poll");
      return 0;
    }
  }
#endif

#ifdef INFO_SLAVE
  if (info_slave_state == INFO_SLAVE_PENDING) {
    update_pending_info_slaves();
  }
#endif

  if (found > 0) {
    /* We have network activity! */
    fds_used = 0;

#ifdef INFO_SLAVE
    now = mudtime;

    /* Do we have new connections from port or SSL? */
    if (ndescriptors < avail_descriptors) {
      if (found > 0 && fds[fds_used++].revents & PENN_POLLIN) {
        found -= 1;
        got_new_connection(sock, CS_IP_SOCKET);
      }
      if (found > 0 && sslsock && fds[fds_used++].revents & PENN_POLLIN) {
        found -= 1;
        got_new_connection(sslsock, CS_OPENSSL_SOCKET);
      }
#ifdef LOCAL_SOCKET
      if (found > 0 && localsock >= 0 &&
          fds[fds_used++].revents & PENN_POLLIN) {
        found -= 1;
        setup_desc(localsock, CS_LOCAL_SOCKET);
      }
#endif /* LOCAL_SOCKET */
    }

    /* any update from info_slave? */
    if (found > 0 && info_slave_state == INFO_SLAVE_PENDING &&
        fds[fds_used++].revents & PENN_POLLIN) {
      found -= 1;
      reap_info_slave();
    } else if (info_slave_state == INFO_SLAVE_PENDING &&
               now > info_queue_time + 30) {
      /* rerun any pending queries that got lost */
      update_pending_info_slaves();
    }

#else /* INFO_SLAVE */
    /* Do we have new connections from port or SSL? */
    if (ndescriptors < avail_descriptors) {
      if (found > 0 && fds[fds_used++].revents & PENN_POLLIN) {
        found -= 1;
        setup_desc(sock, CS_IP_SOCKET);
      }
      if (found > 0 && sslsock && fds[fds_used++].revents & PENN_POLLIN) {
        found -= 1;
        setup_desc(sslsock, CS_OPENSSL_SOCKET);
      }
#ifdef LOCAL_SOCKET
      if (found > 0 && localsock >= 0 &&
          fds[fds_used++].revents & PENN_POLLIN) {
        found -= 1;
        setup_desc(localsock, CS_LOCAL_SOCKET);
      }
#endif /* LOCAL_SOCKET */
    }
#endif /* INFO_SLAVE */

    /* Any updates to the game/txt/??? files? */
    if (found > 0 && notify_fd >= 0 && fds[fds_used++].revents & PENN_POLLIN) {
      found -= 1;
      file_watch_event(notify_fd);
    }

#ifndef WIN32
    if (found > 0 && sigrecv_fd >= 0 && fds[fds_used++].revents & PENN_POLLIN) {
      found -= 1;
      sigrecv_ack();
    }
#endif

    /* Check all the users for input */
    DESC_ITER (d) {
      unsigned int input_ready, output_ready, errors, full_events;
      if (found <= 0)
        break;

      if ((SOCKET) d->descriptor != fds[fds_used].fd)
        continue;

      input_ready = fds[fds_used].revents & PENN_POLLIN;
      full_events = fds[fds_used].revents;
#ifdef HAVE_LIBCURL
      errors = 0;
#else
      errors = fds[fds_used].revents & (POLLERR | POLLNVAL);
#endif
      output_ready = fds[fds_used++].revents & PENN_POLLOUT;
      if (input_ready || errors || output_ready)
        found -= 1;
      if (errors) {
        /* Socket error; kill this connection. */
        shutdownsock(d, "socket error", d->player >= 0 ? d->player : GOD,
                     CONN_NOWRITE);
      } else {
        if (input_ready) {
          if (!process_input(d, output_ready)) {
            shutdownsock(d, "disconnect", d->player, CONN_NOWRITE);
            continue;
          }
        }
        if (output_ready) {
          if (!process_output(d)) {
            shutdownsock(d, "disconnect", d->player, CONN_NOWRITE);
          }
        }
      }
      if (full_events & POLLHUP) {
        http_command_ready(d);
      }
    }
  }
  return 1;
}

static void
gameloop()
{
  uint64_t msec_timeout, timeout_check;
  struct timeval current_time;

  while (!shutdown_flag) {
    /** let's find out how long we should wait */
#define min_timeout(store, func)                                               \
  timeout_check = func;                                                        \
  if (timeout_check < store)                                                   \
  store = timeout_check

    /* any queued commands or events waiting? */
    msec_timeout = SECS_TO_MSECS(500);
    min_timeout(msec_timeout, queue_msecs_till_next());
    min_timeout(msec_timeout, sq_msecs_till_next());
    min_timeout(msec_timeout, http_msecs_till_next());
    /* Sweet, let's check the sockets for input. We'll wait up to msec_timeout
     * milliseconds, but this may be less.
     *
     * If we have any errors in sockets, gotta shut down.
     */
    if (!check_sockets(msec_timeout)) {
      shutdown_flag = 1;
      break;
    }
    /* It might've been a few seconds, let's check_status */
    if (!check_status()) {
      shutdown_flag = 1;
      break;
    }

    /* Let's get ready to run some commands. */
    time(&mudtime);

    /* Update queue load tracker (@ps's data) */
    update_queue_load();

    /* Process all available incoming commands on the socket. */
    process_commands();

    /* Check wait and semaphore to bump any commands to the queue that need it.
     */
    queue_update();

    /* Let's run 'em. */
    do_top(options.queue_chunk);

    /* Run hardcode events (not in queue) */
    sq_run_all();

    /* Clean up and shutdown any sockets that need it: Booted,
     * QUIT, etc etc etc. */
    clean_descriptors(&descriptor_list);

    /* Update socket command quotas for descriptors and http_quota */
    penn_gettimeofday(&current_time);
    update_quotas(current_time);
  }
}

static int
test_connection(SOCKET newsock)
{
#ifdef WIN32
  if (newsock == INVALID_SOCKET && WSAGetLastError() != WSAEINTR)
#else
  if (errno && errno != EINTR)
#endif
  {
    penn_perror("test_connection");
    return -1;
  }
  return newsock;
}

const char *
source_to_s(conn_source source)
{
  switch (source) {
  case CS_IP_SOCKET:
    return "normal port";
  case CS_OPENSSL_SOCKET:
    return "OpenSSL port";
  case CS_LOCAL_SSL_SOCKET:
    return "OpenSSL proxy";
  case CS_LOCAL_SOCKET:
    return "unix port";
  case CS_UNKNOWN:
    return "unknown source";
  }
  return "(error)";
}

static DESC *
new_connection(int oldsock, int *result, conn_source source)
{
  int newsock;
  union sockaddr_u addr;
  struct hostname_info *hi = NULL;
  socklen_t addr_len;
  char ipbuf[BUFFER_LEN];
  char hostbuf[BUFFER_LEN];
  char *bp, *extra = NULL;
  DESC *d;

  *result = 0;
  addr_len = MAXSOCKADDR;
  newsock = accept(oldsock, (struct sockaddr *) (addr.data), &addr_len);
  if (newsock < 0) {
    *result = newsock;
    return 0;
  }
  if (is_remote_source(source)) {
    bp = ipbuf;
    hi = ip_convert(&addr.addr, addr_len);
    safe_str(hi ? hi->hostname : "", ipbuf, &bp);
    *bp = '\0';
    bp = hostbuf;
    hi = hostname_convert(&addr.addr, addr_len);
    safe_str(hi ? hi->hostname : "", hostbuf, &bp);
    *bp = '\0';
  } else { /* source == CS_LOCAL_SOCKET */
    int len;
    char *split;
    int remote_pid = -1;
    int remote_uid = -1;
    bool good_to_read = 1;

    /* As soon as the SSL slave opens a new connection to the mush, it
       writes a string of the format 'IP^HOSTNAME\r\n'. This will thus
       not block unless somebody's being naughty. People obviously can
       be. So we'll wait a short time for readable data, and use a
       non-blocking socket read anyways. If the client doesn't send
       the hostname string fast enough, oh well.
     */

#ifdef HAVE_POLL
    {
      struct pollfd pfd;
      pfd.fd = newsock;
      pfd.events = POLLIN;
      pfd.revents = 0;
      poll(&pfd, 1, 100);
      if (pfd.revents & POLLIN) {
        good_to_read = 1;
      } else {
        good_to_read = 0;
      }
    }
#endif

    if (good_to_read) {
      len = recv_with_creds(newsock, ipbuf, sizeof ipbuf - 1, &remote_pid,
                            &remote_uid);
    } else {
      len = -1;
      errno = EWOULDBLOCK;
    }

    if (len < 5) {
      if (len < 0 && is_blocking_err(errno)) {
        strcpy(hostbuf, "(Unknown)");
        strcpy(ipbuf, "(Unknown)");
      } else {
        /* Yup, somebody's being naughty. Be mean right back. */
        closesocket(newsock);
        return 0;
      }
    } else {
      ipbuf[len] = '\0';
      split = strchr(ipbuf, '^');
      if (split) {
        *split++ = '\0';
        strcpy(hostbuf, split);
        split = strchr(hostbuf, '\r');
        if (split) {
          *(split++) = '\0';
          if (*split == '\n') {
            split++;
          }
          if (*split) {
            extra = split;
          }
        }
      } else {
        /* Again, shouldn't happen! */
        strcpy(ipbuf, "(Unknown)");
        strcpy(hostbuf, "(Unknown)");
      }
    }

/* Use credential passing to tell if a local socket connection was
   made by ssl_slave or something else (Like a web-based client's
   server side). At the moment, this is only implemented on linux
   and OS X. For other OSes, all local connections look like SSL
   ones since that's the usual case. */
#ifdef SSL_SLAVE
    if (remote_pid >= 0) {
      if (remote_pid == ssl_slave_pid) {
        source = CS_LOCAL_SSL_SOCKET;
      } else {
        do_rawlog(LT_CONN,
                  "[%d] Connection on local socket from pid %d run as uid %d.",
                  newsock, remote_pid, remote_uid);
      }
    } else if (remote_uid >= 0) {
      /* A system like OS X that doesn't pass the remote pid as a
         credential but does pass UID? If the remote and local UIDs
         are the same, assume it's from the slave. */
      if (remote_uid == (int) getuid()) {
        source = CS_LOCAL_SSL_SOCKET;
      } else {
        do_rawlog(LT_CONN,
                  "[%d] Connection on local socket from process run as uid %d.",
                  newsock, remote_uid);
      }
    } else {
      /* Default, for OSes without implemented credential
         passing. Just assume it's a connection from ssl_slave. */
      source = CS_LOCAL_SSL_SOCKET;
    }
#else
    source = CS_LOCAL_SSL_SOCKET;
#endif
  }

  if (Forbidden_Site(ipbuf) || Forbidden_Site(hostbuf)) {
    if (!Deny_Silent_Site(ipbuf, AMBIGUOUS) ||
        !Deny_Silent_Site(hostbuf, AMBIGUOUS)) {
      do_rawlog(LT_CONN, "[%d/%s/%s] Refused connection (Remote port %s)",
                newsock, hostbuf, ipbuf, hi ? hi->port : "(unknown)");
    }
    if (is_remote_source(source)) {
      shutdown(newsock, 2);
    }
    closesocket(newsock);
#ifndef WIN32
    errno = 0;
#endif
    return 0;
  }
  do_rawlog(LT_CONN, "[%d/%s/%s] Connection opened from %s.", newsock, hostbuf,
            ipbuf, source_to_s(source));
  if (is_remote_source(source)) {
    set_keepalive(newsock, options.keepalive_timeout);
  }
  d = initializesock(newsock, hostbuf, ipbuf, source);
  if (d && extra) {
    process_input_helper(d, extra, strlen(extra));
  }
  return d;
}

/** Free the OUTPUTPREFIX and OUTPUTSUFFIX for a descriptor. */
static void
clearstrings(DESC *d)
{
  if (d->output_prefix) {
    mush_free(d->output_prefix, "userstring");
    d->output_prefix = 0;
  }
  if (d->output_suffix) {
    mush_free(d->output_suffix, "userstring");
    d->output_suffix = 0;
  }
}

/** Evaluate an attribute which is used in place of a cached text file,
 * and dump it to a descriptor.
 * \param d descriptor to show text to
 * \param thing object to get attr from
 * \param attr attribute to show
 * \param html Is it an HTML fcache?
 * \param prefix text to print before attr contents, or NULL
 * \return 1 if something was written, 0 if not
 */
static int
fcache_dump_attr(DESC *d, dbref thing, const char *attrib, int html,
                 const char *prefix, char *arg)
{
  char descarg[SBUF_LEN], dbrefarg[SBUF_LEN], buff[BUFFER_LEN], *bp;
  PE_REGS *pe_regs;
  ufun_attrib ufun;

  if (!GoodObject(thing) || IsGarbage(thing))
    return 0;

  if (!fetch_ufun_attrib(attrib, thing, &ufun,
                         UFUN_LOCALIZE | UFUN_IGNORE_PERMS | UFUN_REQUIRE_ATTR))
    return -1;

  bp = descarg;
  safe_integer_sbuf(d->descriptor, descarg, &bp);
  *bp = '\0';

  bp = dbrefarg;
  safe_dbref(d->player, dbrefarg, &bp);
  *bp = '\0';

  pe_regs = pe_regs_create(PE_REGS_ARG, "fcache_dump_attr");
  pe_regs_setenv_nocopy(pe_regs, 0, descarg);
  pe_regs_setenv_nocopy(pe_regs, 1, dbrefarg);
  if (arg && *arg)
    pe_regs_setenv_nocopy(pe_regs, 2, arg);
  call_ufun(&ufun, buff, d->player, d->player, NULL, pe_regs);
  bp = strchr(buff, '\0');
  safe_chr('\n', buff, &bp);
  *bp = '\0';
  pe_regs_free(pe_regs);
  if (prefix) {
    queue_newwrite(d, prefix, strlen(prefix));
    queue_eol(d);
  }
  if (html)
    queue_newwrite(d, buff, strlen(buff));
  else
    queue_write(d, buff, strlen(buff));

  return 1;
}

/** Display a cached text file. If a prefix line was given,
 * display that line before the text file, but only if we've
 * got a text file to display
 * \return 1 if something was written, 0 if not
 */
static bool
fcache_dump(DESC *d, FBLOCK fb[2], const char *prefix, char *arg)
{
  int i;

  /* If we've got nothing nice to say, don't say anything */
  if (!fb[0].buff && !((d->conn_flags & CONN_HTML) && fb[1].buff))
    return 0;

  for (i = ((d->conn_flags & CONN_HTML) && fb[1].buff); i >= 0; i--) {
    if (fb[i].thing != NOTHING) {
      if (fcache_dump_attr(d, fb[i].thing, (char *) fb[i].buff, i, prefix,
                           arg) == 1) {
        /* Attr successfully evaluated and displayed */
        return 1;
      }
    } else {
      /* Output static text from the cached file */
      if (prefix) {
        queue_newwrite(d, prefix, strlen(prefix));
        queue_eol(d);
      }
      if (i)
        queue_newwrite(d, fb[1].buff, fb[1].len);
      else
        queue_write(d, fb[0].buff, fb[0].len);
      return 1;
    }
  }

  return 0;
}

/** Read in a single cached text file
 * \param fb block to store text in
 * \param filename file to read
 * \return -1 on error, or number of bytes read from filename
 */
static int
fcache_read(FBLOCK *fb, const char *filename)
{
  if (!fb || !filename)
    return -1;

  /* Free prior cache */
  if (fb->buff) {
    mush_free(fb->buff, "fcache_data");
  }

  if (!*filename) {
    return -1;
  }

  fb->buff = NULL;
  fb->len = 0;
  fb->thing = NOTHING;
  /* Check for #dbref/attr */
  if (*filename == NUMBER_TOKEN) {
    char objname[BUFFER_LEN];
    char *attrib;
    dbref thing;

    strcpy(objname, filename);
    if ((attrib = strchr(objname, '/')) != NULL) {
      *attrib++ = '\0';
      if ((thing = qparse_dbref(objname)) != NOTHING) {
        /* we have #dbref/attr */
        fb->buff = strupper_a(attrib, "fcache_data");
        fb->len = strlen(fb->buff);
        fb->thing = thing;
        return fb->len;
      }
    } else {
      return -1;
    }
  } else {
    MAPPED_FILE *mf = map_file(filename, 0);
    if (mf) {
      /* Copy instead of using the mapped file directly because what
         happens when a mapped file is edited, even if it's a private
         map? There don't seem to be any promises. */
      fb->buff = mush_malloc(mf->len + 1, "fcache_data");
      if (!fb->buff) {
        unmap_file(mf);
        return -1;
      }
      memcpy(fb->buff, mf->data, mf->len);
      fb->buff[mf->len] = '\0';
      fb->len = mf->len;
      unmap_file(mf);
    } else {
      return -1;
    }
  }
  return fb->len;
}

/** Reload a single cached text file.
 * \param filename the name of the file to reload.
 * \return true if the filename was a cached text file, false if not
 */
bool
fcache_read_one(const char *filename)
{
  FBLOCK *fb;
  static HASHTAB lookup;
  static bool lookup_init = 0;

  if (!lookup_init) {
    int i;

    lookup_init = 1;
    hashinit(&lookup, 20);

    for (i = 0; i < (SUPPORT_PUEBLO ? 2 : 1); i += 1) {
      hash_add(&lookup, options.connect_file[i], &fcache.connect_fcache[i]);
      hash_add(&lookup, options.motd_file[i], &fcache.motd_fcache[i]);
      hash_add(&lookup, options.wizmotd_file[i], &fcache.wizmotd_fcache[i]);
      hash_add(&lookup, options.newuser_file[i], &fcache.newuser_fcache[i]);
      hash_add(&lookup, options.register_file[i], &fcache.register_fcache[i]);
      hash_add(&lookup, options.quit_file[i], &fcache.quit_fcache[i]);
      hash_add(&lookup, options.down_file[i], &fcache.down_fcache[i]);
      hash_add(&lookup, options.full_file[i], &fcache.full_fcache[i]);
      hash_add(&lookup, options.guest_file[i], &fcache.guest_fcache[i]);
      hash_add(&lookup, options.who_file[i], &fcache.who_fcache[i]);
    }
  }

  fb = hashfind(filename, &lookup);
  if (!fb)
    return 0;

  fcache_read(fb, filename);
  return 1;
}

/** Load all of the cached text files.
 * \param player the enactor, if done via \@readcache, or NOTHING.
 */
void
fcache_load(dbref player)
{
  int conn, motd, wiz, new, reg, quit, down, full;
  int guest, who;
  int i;

  for (i = 0; i < (SUPPORT_PUEBLO ? 2 : 1); i++) {
    conn = fcache_read(&fcache.connect_fcache[i], options.connect_file[i]);
    motd = fcache_read(&fcache.motd_fcache[i], options.motd_file[i]);
    wiz = fcache_read(&fcache.wizmotd_fcache[i], options.wizmotd_file[i]);
    new = fcache_read(&fcache.newuser_fcache[i], options.newuser_file[i]);
    reg = fcache_read(&fcache.register_fcache[i], options.register_file[i]);
    quit = fcache_read(&fcache.quit_fcache[i], options.quit_file[i]);
    down = fcache_read(&fcache.down_fcache[i], options.down_file[i]);
    full = fcache_read(&fcache.full_fcache[i], options.full_file[i]);
    guest = fcache_read(&fcache.guest_fcache[i], options.guest_file[i]);
    who = fcache_read(&fcache.who_fcache[i], options.who_file[i]);

    if (player != NOTHING) {
      notify_format(player,
                    T("%s sizes:  NewUser...%d  Connect...%d  "
                      "Guest...%d  Motd...%d  Wizmotd...%d  Quit...%d  "
                      "Register...%d  Down...%d  Full...%d  Who...%d"),
                    i ? "HTMLFile" : "File", new, conn, guest, motd, wiz, quit,
                    reg, down, full, who);
    }
  }
}

/** Initialize all of the cached text files (at startup).
 */
void
fcache_init(void)
{
  fcache_load(NOTHING);
}

static void
disconnect_player(DESC *d, enum disconn_reason reason)
{
  if (d->connected == CONN_PLAYER && IsPlayer(d->player)) {
    fcache_dump(d, fcache.quit_fcache, NULL, NULL);
    if (reason == DISCONNECT_LOGOUT) {
      do_rawlog(LT_CONN,
                "[%d/%s/%s] Logout by %s(#%d) <Connection not dropped>",
                d->descriptor, d->addr, d->ip, Name(d->player), d->player);
    } else {
      do_rawlog(LT_CONN, "[%d/%s/%s] Disconnect by %s(#%d) (%s)", d->descriptor,
                d->addr, d->ip, Name(d->player), d->player, d->close_reason);
    }
    announce_disconnect(d, d->close_reason, d->closer);
    if (can_mail(d->player)) {
      do_mail_purge(d->player);
    }
    login_number--;
    if (MAX_LOGINS) {
      if (!under_limit && (login_number < MAX_LOGINS)) {
        under_limit = 1;
        do_rawlog(LT_CONN, "Below maximum player limit of %d. Logins enabled.",
                  MAX_LOGINS);
      }
    }
  }
}

/** Logout a descriptor from the player it's connected to,
 * without dropping the connection. Run when a player uses LOGOUT
 * \param d descriptor
 */
static void
logout_sock(DESC *d)
{
  if (d->connected == CONN_PLAYER) {
    disconnect_player(d, DISCONNECT_LOGOUT);
  } else {
    do_rawlog(LT_CONN,
              "[%d/%s/%s] Logout, never connected. <Connection not dropped>",
              d->descriptor, d->addr, d->ip);
  }
  process_output(d); /* flush our old output */
  /* pretend we have a new connection */
  d->connected = CONN_SCREEN;
  d->output_prefix = 0;
  d->output_suffix = 0;
  d->output_size = 0;
  d->player = NOTHING;
  init_text_queue(&d->input);
  init_text_queue(&d->output);
  d->raw_input = 0;
  d->raw_input_at = 0;
  d->quota = QUOTA_MAX;
  d->last_time = mudtime;
  d->cmds = 0;
  d->hide = 0;
  welcome_user(d, 0);
}

/** Disconnect a descriptor.
 * This sends appropriate disconnection text, announcements, queues events,
 * logs, etc.
 *
 * \param d pointer to descriptor to disconnect.
 */
static void
disconnect_desc(DESC *d)
{
  const char *reason = d->close_reason;
  if (d->connected == CONN_PLAYER) {
    disconnect_player(d, DISCONNECT_QUIT);
  } else {
    do_rawlog(LT_CONN, "[%d/%s/%s] Connection closed, never connected (%s).",
              d->descriptor, d->addr, d->ip, reason);
  }
  /* (descriptor, ip, cause, recv/sent/cmds) */
  queue_event(SYSEVENT, "SOCKET`DISCONNECT", "%d,%s,%s,%lu/%lu/%d",
              d->descriptor, d->ip, reason, d->input_chars, d->output_chars,
              d->cmds);
  if (d->conn_flags & CONN_GMCP) {
    send_oob(d, "Core.Goodbye", NULL);
  }
  process_output(d);
  clearstrings(d);
  if (d->conn_timer) {
    sq_cancel(d->conn_timer);
    d->conn_timer = NULL;
  }
  d->conn_flags |= CONN_NOWRITE;

  connlog_disconnection(d->connlog_id, reason);
}

/** Clean up a descriptor.
 * This flushes output and then closes the associated socket. When this
 * is called, d should no longer be in descriptor_list
 * \param d pointer to descriptor to disconnect.
 */
static void
cleanup_desc(DESC *d)
{
  shutdown(d->descriptor, 2);
  closesocket(d->descriptor);

  im_delete(descs_by_fd, d->descriptor);

  if (sslsock && d->ssl) {
    ssl_close_connection(d->ssl);
    d->ssl = NULL;
  }

  if (d->http_request) {
    mush_free(d->http_request, "http_request");
  }

  {
    freeqs(d);
    if (d->ttype && d->ttype != default_ttype)
      mush_free(d->ttype, "terminal description");
    memset(d, 0xFF, sizeof *d);
    mush_free(d, "descriptor");
  }

  ndescriptors--;
}

/* ARGSUSED */
DESC *
initializesock(int s, char *addr, char *ip, conn_source source)
{
  DESC *d;
  d = (DESC *) mush_malloc(sizeof(DESC), "descriptor");
  if (!d)
    mush_panic("Out of memory.");
  d->descriptor = s;
  d->closer = NOTHING;
  d->close_reason = "unknown";
  d->http_request = NULL;
  d->connected = CONN_SCREEN;
  d->conn_timer = NULL;
  d->connected_at = mudtime;
  make_nonblocking(s);
  d->output_prefix = 0;
  d->output_suffix = 0;
  d->output_size = 0;
  init_text_queue(&d->input);
  init_text_queue(&d->output);
  d->player = NOTHING;
  d->raw_input = 0;
  d->raw_input_at = 0;
  d->quota = QUOTA_MAX;
  d->last_time = mudtime;
  d->cmds = 0;
  d->hide = 0;
  mush_strncpy(d->addr, addr, 100);
  d->addr[99] = '\0';
  mush_strncpy(d->ip, ip, 100);
  d->ip[99] = '\0';
  d->conn_flags = CONN_DEFAULT;
  d->input_chars = 0;
  d->output_chars = 0;
  d->width = 78;
  d->height = 24;
  d->ttype = NULL;
  d->checksum[0] = '\0';
  d->ssl = NULL;
  d->ssl_state = 0;
  d->source = source;
  d->next = descriptor_list;
  descriptor_list = d;
  if (source == CS_OPENSSL_SOCKET) {
    d->ssl = ssl_listen(d->descriptor, &d->ssl_state);
    if (d->ssl_state < 0) {
      /* Error we can't handle */
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
    }
  }
  im_insert(descs_by_fd, d->descriptor, d);
  d->connlog_id = connlog_connection(ip, addr, is_ssl_desc(d));
  d->conn_timer = sq_register_in(1, test_telnet_wrapper, (void *) d, NULL);
  queue_event(SYSEVENT, "SOCKET`CONNECT", "%d,%s", d->descriptor, d->ip);
  return d;
}

static int
network_send_ssl(DESC *d)
{
  int input_ready, written = 0;
  bool need_write = 0;
  struct text_block *cur;

  if (!d->ssl)
    return 0;

  /* Ensure that we're not in a state where we need an SSL_handshake() */
  if (ssl_need_handshake(d->ssl_state)) {
    d->ssl_state = ssl_handshake(d->ssl);
    if (d->ssl_state < 0) {
      /* Fatal error */
      do_rawlog(LT_CONN, "[%d/%s/%s] SSL handshake failure.\n", d->descriptor,
                d->addr, d->ip);
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
      return 0;
    } else if (ssl_need_handshake(d->ssl_state)) {
      /* We're still not ready to send to this connection. Alas. */
      return 1;
    }
  }
  /* Ensure that we're not in a state where we need an SSL_accept() */
  if (ssl_need_accept(d->ssl_state)) {
    d->ssl_state = ssl_accept(d->ssl);
    if (d->ssl_state < 0) {
      /* Fatal error */
      do_rawlog(LT_CONN, "[%d/%s/%s] SSL accept failure.\n", d->descriptor,
                d->addr, d->ip);
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
      return 0;
    } else if (ssl_need_accept(d->ssl_state)) {
      /* We're still not ready to send to this connection. Alas. */
      return 1;
    }
  }

  /* process_output, alas, gets called from all kinds of places.
   * We need to know if the descriptor is waiting on input, though.
   * So let's find out
   */
  {
#ifdef WIN32
    WSAPOLLFD p;
#else
    struct pollfd p;
#endif

    p.fd = d->descriptor;
    p.events = POLLIN;
    p.revents = 0;
#ifdef WIN32
    input_ready = WSAPoll(&p, 1, 0);
#else
    input_ready = poll(&p, 1, 0);
#endif
  }

  if (input_ready < 0) {
    /* Well, shoot, we have no idea. Guess and proceed. */
    penn_perror("select in process_output");
    input_ready = 0;
  }

  while ((cur = d->output.head) != NULL) {
    int cnt = 0;
    need_write = 0;
    d->ssl_state = ssl_write(d->ssl, d->ssl_state, input_ready, 1, cur->start,
                             cur->nchars, &cnt);
    if (ssl_want_write(d->ssl_state)) {
      need_write = 1;
      break; /* Need to retry */
    }
    written += cnt;
    if (cnt == cur->nchars) {
      /* Wrote a complete block */
      d->output.head = cur->nxt;
      free_text_block(cur);
    } else {
      cur->start += cnt;
      cur->nchars -= cnt;
      break;
    }
  }

  if (!d->output.head)
    d->output.tail = NULL;
  d->output_size -= written;
  d->output_chars += written;

  return written + need_write;
}

#ifdef HAVE_WRITEV
static int
network_send_writev(DESC *d)
{
  int written = 0;

  while (d->output.head) {
    int cnt, n;
    struct iovec lines[10];
    struct text_block *cur = d->output.head;

    for (n = 0; cur && n < 10; cur = cur->nxt) {
      lines[n].iov_base = cur->start;
      lines[n].iov_len = cur->nchars;
      n += 1;
    }

    cnt = writev(d->descriptor, lines, n);
    if (cnt < 0) {
      if (is_blocking_err(errno)) {
        return 1;
      } else {
        shutdownsock(d, "socket error", NOTHING, CONN_NOWRITE);
        return 0;
      }
    }
    written += cnt;
    while (cnt > 0) {
      cur = d->output.head;
      if (cur->nchars <= cnt) {
        /* Wrote a full block */
        cnt -= cur->nchars;
        d->output.head = cur->nxt;
        free_text_block(cur);
      } else {
        /* Wrote a partial block */
        cur->start += cnt;
        cur->nchars -= cnt;
        goto output_done;
      }
    }
  }

output_done:
  if (!d->output.head)
    d->output.tail = NULL;
  d->output_size -= written;
  d->output_chars += written;

  return written;
}
#endif

static int
network_send(DESC *d)
{
  int written = 0;
  struct text_block *cur;

  if (!d || !d->output.head)
    return 1;

#ifdef HAVE_WRITEV
  /* If there's multiple pending blocks of text to send, use writev() if
     possible. */
  if (d->output.head->nxt)
    return network_send_writev(d);
#endif

  while ((cur = d->output.head) != NULL) {
    int cnt = send(d->descriptor, cur->start, cur->nchars, 0);

    if (cnt < 0) {
      if (is_blocking_err(errno))
        return 1;
      else {
        shutdownsock(d, "socket error", NOTHING, CONN_NOWRITE);
        return 0;
      }
    }
    written += cnt;

    if (cnt == cur->nchars) {
      /* Wrote a complete block */
      d->output.head = cur->nxt;
      free_text_block(cur);
    } else {
      /* Partial */
      cur->nchars -= cnt;
      cur->start += cnt;
      break;
    }
  }

  if (!d->output.head)
    d->output.tail = NULL;
  d->output_size -= written;
  d->output_chars += written;
  return written;
}

/** Flush pending output for a descriptor.
 * This function actually sends the queued output over the descriptor's
 * socket.
 * \param d pointer to descriptor to send output to.
 * \retval 1 successfully flushed at least some output.
 * \retval 0 something failed, and the descriptor should probably be closed.
 */
int
process_output(DESC *d)
{
  if (d->ssl)
    return network_send_ssl(d);
  else
    return network_send(d);
}

/** A wrapper around test_telnet(), which is called via the
 * squeue system in timers.c
 * \param data a descriptor, cast as a void pointer
 * \return false
 */
bool
test_telnet_wrapper(void *data)
{
  DESC *d = (DESC *) data;

  test_telnet(d);
  d->conn_timer = sq_register_in(1, welcome_user_wrapper, (void *) d, NULL);
  return false;
}

/** A wrapper around welcome_user(), which is called via the
 * squeue system in timers.c
 * \param data a descriptor, cast as a void pointer
 * \return false
 */
bool
welcome_user_wrapper(void *data)
{
  DESC *d = (DESC *) data;

  welcome_user(d, -1);
  d->conn_timer = NULL;
  return false;
}

/** Show the login screen for a descriptor.
 * \param d descriptor
 * \param telnet should we test for telnet support?
 */
static void
welcome_user(DESC *d, int telnet)
{
  if (telnet == 1)
    test_telnet(d);
  else if (telnet == 0 && SUPPORT_PUEBLO && !(d->conn_flags & CONN_HTML))
    queue_newwrite(d, PUEBLO_HELLO, strlen(PUEBLO_HELLO));
  fcache_dump(d, fcache.connect_fcache, NULL, NULL);
}

static void
save_command(DESC *d, char *command)
{
  if (d->conn_flags & CONN_UTF8) {
    char *latin1;
    int llen;
#ifdef HAVE_ICU
    latin1 = translate_utf8_to_latin1(command, -1, &llen, "string");
#else
    latin1 = utf8_to_latin1(command, -1, &llen, 1, "string");
#endif
    if (latin1) {
      char *c;
      for (c = latin1; *c; c += 1) {
        if (!char_isprint(*c)) {
          *c = '?';
        }
      }
      add_to_queue(&d->input, latin1, llen + 1);
      mush_free(latin1, "string");
    } else {
      const char errmsg[] =
        "ERROR: Unicode sanitization+normalization failed.\r\n";
      queue_newwrite(d, errmsg, sizeof(errmsg) - 1);
      do_rawlog(LT_ERR, "Unable to sanitize+normalize input '%s'", command);
    }
  } else {
    char *c;
    for (c = command; *c; c += 1) {
      if (!char_isprint(*c)) {
        *c = '?';
      }
    }
    add_to_queue(&d->input, command, strlen(command) + 1);
  }
}

/** Send a telnet command to a descriptor to test for telnet support.
 * Also sends the Pueblo test string.
 */
static void
test_telnet(DESC *d)
{
  /* Use rfc 1184 to test telnet support, as it tries to set linemode
     with client-side editing. Good for Broken Telnet Programs. */
  if (!TELNET_ABLE(d)) {
    static const char query[3] = {IAC, DO, TN_LINEMODE};
    if ((d->conn_flags & (CONN_WEBSOCKETS_REQUEST | CONN_WEBSOCKETS))) {
      /* Don't bother testing for TELNET support. */
      return;
    }
    if ((d->conn_flags & (CONN_HTTP_REQUEST))) {
      /* Don't bother testing for TELNET support. */
      return;
    }
    queue_newwrite(d, query, 3);
    d->conn_flags |= CONN_TELNET_QUERY;
    if (SUPPORT_PUEBLO && !(d->conn_flags & CONN_HTML))
      queue_newwrite(d, PUEBLO_HELLO, strlen(PUEBLO_HELLO));
    process_output(d);
  }
}

/** Turn on telnet support when a connection has shown it has support
 */
static void
setup_telnet(DESC *d)
{
  /* Win2k telnet doesn't do local echo by default,
     apparently. Unfortunately, there doesn't seem to be a telnet
     option for local echo, just remote echo. */
  d->conn_flags |= CONN_TELNET;
  if ((d->conn_flags & (CONN_TELNET_QUERY | CONN_AWAITING_FIRST_DATA)) &&
      starting_telnet_neg) {
    d->conn_flags &= ~CONN_TELNET_QUERY;
    do_rawlog(LT_CONN, "[%d/%s/%s] Switching to Telnet mode.", d->descriptor,
              d->addr, d->ip);
    queue_newwrite(d, starting_telnet_neg, starting_telnet_neg_len);
    process_output(d);
  }
}

/* A standard response */
TELNET_HANDLER(telnet_will)
{
  char response[3] = {IAC, WILL, 0};
  response[2] = *(cmd + 1);
  queue_newwrite(d, response, 3);
  process_output(d);
}

/* A standard response */
TELNET_HANDLER(telnet_willdo)
{
  char response[6] = {IAC, WILL, 0, IAC, DO, 0};
  response[2] = response[5] = *(cmd + 1);
  queue_newwrite(d, response, 6);
  process_output(d);
}

/* Handle DO SUPPRESS-GOAHEAD */
TELNET_HANDLER(telnet_sga)
{
  if (*cmd == DO) {
    char response[6] = {IAC, WILL, TN_SGA, IAC, DO, TN_SGA};
    queue_newwrite(d, response, 6);
    process_output(d);
    /* Yeah, we still will send GA, which they should treat as a NOP,
     * but we'd better send newlines, too.
     */
    d->conn_flags |= CONN_PROMPT_NEWLINES;
  }
}

/* NAWS subnegotiation */
TELNET_HANDLER(telnet_naws_sb)
{
  union {
    short s;
    char bytes[2];
  } raw;

  if (len != 4)
    return; /* Invalid */
  raw.bytes[0] = *(cmd++);
  raw.bytes[1] = *(cmd++);
  d->width = ntohs(raw.s);

  raw.bytes[0] = *(cmd++);
  raw.bytes[1] = *cmd;
  d->height = ntohs(raw.s);
}

/** Set the TTYP (terminal type / client name) for a descriptor.
 * \param d descriptor to set ttyp for
 * \param value ttyp to set, or NULL/default_ttype for default */
static void
set_ttype(DESC *d, char *value)
{
  if (!d)
    return;

  if (d->ttype && d->ttype != default_ttype)
    mush_free(d->ttype, "terminal description");
  if (value && *value && value != default_ttype && strcmp(value, default_ttype))
    d->ttype = mush_strdup(value, "terminal description");
  else
    d->ttype = (char *) default_ttype;
}

/* Send TTYP subnegotiation request */
TELNET_HANDLER(telnet_ttype)
{
  char reply[6] = {IAC, SB, TN_TTYPE, 1, IAC, SE};
  queue_newwrite(d, reply, 6);
  process_output(d);
}

/** Handle TTYP */
TELNET_HANDLER(telnet_ttype_sb)
{
  /* cmd should begin with IS, which is 0 */
  if (!len || *cmd != 0)
    return;
  cmd++;

  set_ttype(d, cmd);
}

/* Handle DO CHARSET, send list of known charsets */
TELNET_HANDLER(telnet_charset)
{
  /* Send a list of supported charsets for the client to pick.
   * Currently, we only offer the single charset the MUSH is
   * currently running in (if known, and not "C"),
   * and UTF-8, and plain ol' ascii. */
  /* IAC SB CHARSET REQUEST ";" <charset-list> IAC SE */
  static const char reply_prefix[4] = {IAC, SB, TN_CHARSET,
                                       TN_SB_CHARSET_REQUEST};
  static const char reply_suffix[2] = {IAC, SE};
#ifndef _MSC_VER
/* Offer a selection of possible delimiters, to avoid it appearing
 * in a charset name */
#ifdef HAVE_NL_LANGINFO
  static const char *delim_list = "; +=/!", *delim_curr;
#endif /* HAVE_NL_LANGINFO */
  char delim[2] = {';', '\0'};
  char *curr_locale = NULL;

  if (*cmd != DO)
    return;

  queue_newwrite(d, reply_prefix, 4);
#ifdef HAVE_NL_LANGINFO
  curr_locale = nl_langinfo(CODESET);
  if (curr_locale && *curr_locale && strcmp(curr_locale, "C") &&
      strncasecmp(curr_locale, "UTF-", 4)) {
    for (delim_curr = delim_list; *delim_curr; delim_curr++) {
      if (strchr(curr_locale, *delim_curr) == NULL)
        break;
    }
    if (*delim_curr) {
      delim[0] = *delim_curr;
    } else {
      delim[0] = ';'; /* fall back on ; */
    }
  }
#endif /* HAVE_NL_LANGINFO */
  queue_newwrite(d, delim, 1);
  queue_newwrite(d, "UTF-8", 5);
  queue_newwrite(d, delim, 1);
  if (curr_locale && strlen(curr_locale)) {
    queue_newwrite(d, curr_locale, strlen(curr_locale));
    queue_newwrite(d, delim, 1);
  }
  queue_newwrite(d, "US-ASCII", 8);
  queue_newwrite(d, delim, 1);
  queue_newwrite(d, "ASCII", 5);
  queue_newwrite(d, delim, 1);
  queue_newwrite(d, "x-penn-def", 10);

  queue_newwrite(d, reply_suffix, 2);
#else  /* _MSC_VER */
  /* MSVC doesn't have langinfo.h, and doesn't support nl_langinfo().
   * As a temporary work-around, offer ISO-8859-1 as a hardcoded option
   * in this case. (We could use setlocale(LC_ALL, NULL) as a replacement
   * but it's unlikely to contain a valid charset name, so probably
   * wouldn't help anyway.) */

  if (*cmd != DO)
    return;

  queue_newwrite(d, reply_prefix, 4);
  queue_newwrite(d, ";UTF-8", 6);
  queue_newwrite(d, ";ISO-8859-1", 11);
  queue_newwrite(d, ";US-ASCII;ASCII;x-win-def", 25);
  queue_newwrite(d, reply_suffix, 2);
#endif /* _MSC_VER */
}

/* Handle CHARSET subnegotiation */
TELNET_HANDLER(telnet_charset_sb)
{
  /* Possible subnegotiations are
   * CHARSET ACCEPTED <charset> IAC SE
   * CHARSET REJECTED IAC SE
   */
  char type;

  type = *(cmd++);
  if (type != TN_SB_CHARSET_ACCEPTED) {
    /* This is the only thing we support */
    return;
  }
  if (!strcasecmp(cmd, "US-ASCII") || !strcasecmp(cmd, "ASCII")) {
    /* ascii requested; strip accents for the connection */
    do_rawlog(LT_CONN, "Descriptor %d using charset ASCII.", d->descriptor);
    d->conn_flags |= CONN_STRIPACCENTS;
  }
  if (strcasecmp(cmd, "UTF-8") == 0) {
    /* Send and receive UTF-8, translate to latin-1 */
    do_rawlog(LT_CONN, "Descriptor %d using charset UTF-8.", d->descriptor);
    d->conn_flags |= CONN_UTF8;
  }
}

/* Set our preferred line mods */
TELNET_HANDLER(telnet_linemode)
{
  /* Set up our preferred linemode options. */
  /* IAC SB LINEMODE MODE (EDIT|SOFT_TAB) IAC SE */
  static const char reply[7] = {IAC, SB, TN_LINEMODE, '\x01', '\x09', IAC, SE};
  queue_newwrite(d, reply, 7);
}

/* Send MSSP data */
TELNET_HANDLER(telnet_mssp)
{
  /* IAC SB MSSP MSSP_VAR "variable" MSSP_VAL "value" ... IAC SE */
  char reply[BUFFER_LEN];
  char *bp;
  bp = reply;

  safe_chr((char) IAC, reply, &bp);
  safe_chr((char) SB, reply, &bp);
  safe_chr((char) TN_MSSP, reply, &bp);
  report_mssp((DESC *) NULL, reply, &bp);
  safe_chr((char) IAC, reply, &bp);
  safe_chr((char) SE, reply, &bp);
  *bp = '\0';
  queue_newwrite(d, reply, strlen(reply));
  process_output(d);
}

TELNET_HANDLER(telnet_gmcp) { d->conn_flags |= CONN_GMCP; }

TELNET_HANDLER(telnet_gmcp_sb)
{
  struct gmcp_handler *g;
  char fullpackage[BUFFER_LEN], package[BUFFER_LEN], fullmsg[BUFFER_LEN];
  char *p, *msg;
  cJSON *json = NULL;
  int match = 0, i = 50;

  if (!gmcp_handlers)
    return; /* Nothing to do */

  mush_strncpy(fullpackage, cmd, BUFFER_LEN);
  msg = strchr(fullpackage, ' ');
  if (msg) {
    *msg++ = '\0';
  }
  mush_strncpy(package, fullpackage, BUFFER_LEN);

  p = package;
  if (!*p)
    return; /* We should always get a package name */

  if (msg && *msg) {
    /* string_to_json destructively modifies msg, so make a copy */
    mush_strncpy(fullmsg, msg, BUFFER_LEN);
    json = cJSON_Parse(msg);
    if (!json)
      return; /* Invalid json */
  } else
    fullmsg[0] = '\0';

  while (i > 0) {
    i--;
    g = gmcp_handlers;
    while (g && !match) {
      if (!strcasecmp(g->package, p)) {
        match = g->func(fullpackage, json, fullmsg, d);
      }
      g = g->next;
    }
    if (match || !*p) {
      break; /* Either we got a match, or failed all possible matches */
    } else {
      if ((p = strrchr(p, '.'))) {
        /* Trim off a subpackage and try again for a less specific match */
        *p = '\0';
        p = package;
      } else {
        /* Final check for 'last resort' handlers */
        p = package;
        *p = '\0';
      }
    }
  }
  cJSON_Delete(json);
}

/** Escape a string so it can be sent as a telnet SB (IAC -> IAC IAC). Returns
 * a STATIC buffer. */
char *
telnet_escape(char *str)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  char *p, *save;

  *bp = '\0';
  if (!str || !*str)
    return buff;

  for (p = str; *p; p++) {
    if (*p == IAC) {
      save = bp;
      if (safe_chr(IAC, buff, &bp) || safe_chr(IAC, buff, &bp)) {
        *save = '\0';
        return buff;
      }
    } else if (safe_chr(*p, buff, &bp)) {
      *bp = '\0';
      return buff;
    }
  }
  *bp = '\0';
  return buff;
}

/** Register a handler for GMCP data.
 * \param package the package name (Core.Hello, Foo.Bar.Baz). Use an empty
 *        string as a default handler for all unhandled messages
 * \param func function to run when matching messages are received
 */
void
register_gmcp_handler(char *package, gmcp_handler_func func)
{
  char *p;
  struct gmcp_handler *g;

  if (!func)
    return;

  if (!package)
    p = "";
  else
    p = package;

  g = mush_malloc(sizeof(struct gmcp_handler), "gmcp");
  g->package = mush_strdup(p, "gmcp.package");
  g->func = func;
  g->next = gmcp_handlers;

  gmcp_handlers = g;
}

/* Handler for Core.Hello messages */
GMCP_HANDLER(gmcp_core_hello)
{
  cJSON *j;

  if (strcasecmp(package, "Core.Hello")) {
    return 0; /* Package was Core.Hello.something, and we don't handle that */
  }

  if (!cJSON_IsObject(json)) {
    return 0; /* We're expecting an object */
  }

  j = cJSON_GetObjectItemCaseSensitive(json, "client");
  if (j && cJSON_IsString(j)) {
    set_ttype(d, cJSON_GetStringValue(j));
  }

  return 1;
}

/* Handler for Core.Ping and Core.KeepAlive messages */
GMCP_HANDLER(gmcp_core_ping)
{

  if (!strcasecmp(package, "Core.KeepAlive"))
    return 1;
  else if (!strcasecmp(package, "Core.Ping")) {
    send_oob(d, "Core.Ping", NULL);
    return 1;
  }
  return 0;
}

#ifdef GMCP_EXAMPLE
/* An example softcode handler for GMCP data - in this case, triggering the
 * attr #0/GMCP whenever otherwise-unhandled data is received, with
 * %0 = port/descriptor number, %1 = package name, %2 = json message */
GMCP_HANDLER(gmcp_softcode_example)
{
  dbref obj = 0;
  char *attrname = "GMCP";
  ATTR *a;
  PE_REGS *pe_regs;

  a = atr_get_noparent(obj, attrname);
  if (!a)
    return 0; /* No such attr; gmcp message unhandled */

  pe_regs = pe_regs_create(PE_REGS_ARG, "gmcp_softcode");
  pe_regs_set_int(pe_regs, PE_REGS_ARG, "0", d->descriptor);
  pe_regs_setenv(pe_regs, 1, package);
  pe_regs_setenv(pe_regs, 2, msg);
  queue_attribute_base_priv(obj, attrname, d->player, 1, pe_regs, QUEUE_DEFAULT,
                            NOTHING, NULL, NULL);
  pe_regs_free(pe_regs);
  
  return 1;
}

/* You'll also need to do:
 *   register_gmcp_handler("", gmcp_softcode_example);
 * somewhere like local_startup() to initialize the handler */
#endif

/** Send an out-of-band message to a descriptor using the GMCP telnet
 * subnegotiation
 * \param d descriptor to send to
 * \param package The name of the package[.subpackage(s)] the message belongs to
 * \param data a JSON object, or NULL for no message
 */
void
send_oob(DESC *d, char *package, cJSON *data)
{
  char buff[BUFFER_LEN];
  char *bp = buff;
  char *escmsg = NULL;
  int error;

  if (!d || !(d->conn_flags & CONN_GMCP) || !package || !*package)
    return;

  if (data && !cJSON_IsInvalid(data)) {
    char *str = cJSON_PrintUnformatted(data);
    safe_str(str, buff, &bp);
    *bp = '\0';
    free(str);
    escmsg = telnet_escape(buff);
    bp = buff;
  }

  if (escmsg && *escmsg)
    error = safe_format(buff, &bp, "%c%c%c%s %s%c%c", IAC, SB, TN_GMCP, package,
                        escmsg, IAC, SE);
  else
    error = safe_format(buff, &bp, "%c%c%c%s%c%c", IAC, SB, TN_GMCP, package,
                        IAC, SE);

  *bp = '\0';

  if (!error) {
    queue_newwrite(d, buff, strlen(buff));
    process_output(d);
  }
}

FUNCTION(fun_oob)
{
  dbref who;
  DESC *d;
  cJSON *json;
  int i = 0;
  const char *l = NULL;
  char *p;
  int failed = 0;

  json = cJSON_Parse(args[2]);
  if (!json) {
    safe_str(T("#-1 INVALID JSON"), buff, bp);
    return;
  }

  l = trim_space_sep(args[0], ' ');
  p = next_in_list(&l);

  do {
    who = lookup_player(p);
    if (who == NOTHING) {
      failed++;
      continue;
    }

    if (Owner(who) != Owner(executor) && !Can_Send_OOB(executor)) {
      failed++;
      continue;
    }

    DESC_ITER_CONN (d) {
      if (d->player != who)
        continue;
      if (d->conn_flags & CONN_WEBSOCKETS) {
        send_websocket_object(d, args[1], json);
        i++;
      }
      if (d->conn_flags & CONN_GMCP) {
        send_oob(d, args[1], json);
        i++;
      }
    }
  } while (l && *l && (p = next_in_list(&l)));
  
  if (failed && i < 1) {
    safe_str("#-1 NO VALID PLAYERS", buff, bp);
  } else {
    safe_integer(i, buff, bp);
  }
  
  cJSON_Delete(json);
}

void
init_telnet_opts(void)
{
  int i, len;
  struct telnet_opt *telopt;
  for (i = 0; i < 256; i++) {
    telnet_options[i] = NULL;
  }
  telopt = mush_malloc(sizeof(struct telnet_opt), "telopt");
  telopt->optcode = i = TN_SGA;
  telopt->offer = 0;
  telopt->handler = telnet_sga;
  telopt->sb = NULL;
  telnet_options[i] = telopt;

  telopt = mush_malloc(sizeof(struct telnet_opt), "telopt");
  telopt->optcode = i = TN_TTYPE;
  telopt->offer = DO;
  telopt->handler = telnet_ttype;
  telopt->sb = telnet_ttype_sb;
  telnet_options[i] = telopt;

  telopt = mush_malloc(sizeof(struct telnet_opt), "telopt");
  telopt->optcode = i = TN_NAWS;
  telopt->offer = DO;
  telopt->handler = NULL;
  telopt->sb = telnet_naws_sb;
  telnet_options[i] = telopt;

  telopt = mush_malloc(sizeof(struct telnet_opt), "telopt");
  telopt->optcode = i = TN_LINEMODE;
  telopt->offer = 0;
  telopt->handler = telnet_linemode;
  telopt->sb = NULL;
  telnet_options[i] = telopt;

  telopt = mush_malloc(sizeof(struct telnet_opt), "telopt");
  telopt->optcode = i = TN_CHARSET;
  telopt->offer = WILL;
  telopt->handler = telnet_charset;
  telopt->sb = telnet_charset_sb;
  telnet_options[i] = telopt;

  telopt = mush_malloc(sizeof(struct telnet_opt), "telopt");
  telopt->optcode = i = TN_MSSP;
  telopt->offer = WILL;
  telopt->handler = telnet_mssp;
  telopt->sb = NULL;
  telnet_options[i] = telopt;

  telopt = mush_malloc(sizeof(struct telnet_opt), "telopt");
  telopt->optcode = i = TN_GMCP;
  telopt->offer = WILL;
  telopt->handler = telnet_gmcp;
  telopt->sb = telnet_gmcp_sb;
  telnet_options[i] = telopt;

  /* Store the telnet options we negotiate for new connections,
   * to avoid looking them up every time someone connects */
  len = 0;

  for (i = 0; i < 256; i++) {
    if (telnet_options[i] && telnet_options[i]->offer)
      len += 3;
  }
  if (len) {
    char *p = starting_telnet_neg = malloc(len);
    for (i = 0; i < 256; i++) {
      if (telnet_options[i] && telnet_options[i]->offer) {
        *p++ = IAC;
        *p++ = telnet_options[i]->offer;
        *p++ = telnet_options[i]->optcode;
      }
    }
    starting_telnet_neg_len = len;
  }

  register_gmcp_handler("Core.Hello", gmcp_core_hello);
  register_gmcp_handler("Core.Ping", gmcp_core_ping);
  register_gmcp_handler("Core.KeepAlive", gmcp_core_ping);
}

/** Parse a telnet code received from a connection.
 * \param d descriptor
 * \param q first char after the IAC
 * \param qend end of the string
 * \retval -1 Incomplete telnet code received
 * \retval 0 Invalid telnet code (or IAC IAC) received
 * \retval 1 Telnet code successfully handled
 */
static int
handle_telnet(DESC *d, char **q, char *qend)
{
  char *p;
  unsigned char opt = '\0';
  bool got_iac = 0;
  static char telnet_buff[BUFFER_LEN];
  char *tbp = telnet_buff;
  static const char ayt_reply[] = "\r\n*** AYT received, I'm here ***\r\n";

  /* *(*q - q) == IAC at this point. */
  switch (**q) {
  case IAC:
    setup_telnet(d);
    /* We don't skip over the IAC, we leave it to be written out in
     * process_input_helper */
    return 0;
  case NOP:
    setup_telnet(d);
    return 1;
  case AYT:
    setup_telnet(d);
    queue_newwrite(d, ayt_reply, strlen(ayt_reply));
    process_output(d);
    return 1;
  case DONT:
  case WONT:
    setup_telnet(d);
    (*q)++; /* Skip DONT/WONT */
    return 1;
  case DO:
  case WILL:
    setup_telnet(d);
    p = *q;
    (*q)++;
    opt = **q;
    if (*q > qend)
      return -1;
    if (!telnet_options[opt]) {
      telnet_buff[0] = IAC;
      telnet_buff[1] = (*p == DO) ? WONT : DONT;
      telnet_buff[2] = opt;
      queue_newwrite(d, telnet_buff, 3);
      process_output(d);
    } else if (telnet_options[opt]->handler) {
      telnet_options[opt]->handler(d, p, 2);
    }
    return 1;
  case SB:
    /* Make sure we have a complete subcommand.  */
    /* IAC SB <opt> ... IAC SE */
    (*q)++; /* Skip over SB */
    opt = **q;
    (*q)++;
    if (*q >= qend) {
      return -1;
    }
    for (; *q < qend; (*q)++) {
      if (got_iac) {
        got_iac = 0;
        if (**q == IAC) {
          safe_chr(IAC, telnet_buff, &tbp);
        } else if (**q == SE) {
          /* A complete command */
          *tbp = '\0';
          if (telnet_options[opt] && telnet_options[opt]->sb) {
            telnet_options[opt]->sb(d, telnet_buff, (tbp - telnet_buff));
          }
          return 1;
        } else {
          /* We shouldn't get anything else here after an IAC! */
          /* If we return 0 here, we're probably leaving gibberish
           * in the buffer. However, since we know the telnet code
           * isn't valid, we can't safely discard anything, either.
           * Oh well.
           */
          return 0;
        }
      } else if (**q == IAC) {
        got_iac = 1;
      } else {
        safe_chr(**q, telnet_buff, &tbp);
      }
    }
    return -1; /* If we get here, we never found the closing IAC SE */
  default:
    return 0;
  }
}

static void
process_input_helper(DESC *d, char *tbuf1, int got)
{
  char *p, *pend, *q, *qend;
  int is_first;

  is_first = d->conn_flags & CONN_AWAITING_FIRST_DATA;

  /* Is it an HTTP connection? */
  if (d->conn_flags & CONN_HTTP_REQUEST) {
    process_http_input(d, tbuf1, got);
    return;
  }

  if ((d->conn_flags & CONN_WEBSOCKETS)) {
    /* Process using WebSockets framing. */
    got = process_websocket_frame(d, tbuf1, got);
  }

  if (!d->raw_input) {
    d->raw_input = mush_malloc(MAX_COMMAND_LEN, "descriptor_raw_input");
    if (!d->raw_input)
      mush_panic("Out of memory");
    d->raw_input_at = d->raw_input;
  }
  p = d->raw_input_at;
  d->input_chars += got;
  pend = d->raw_input + MAX_COMMAND_LEN - 1;
  for (q = tbuf1, qend = tbuf1 + got; q < qend; q++) {
    if (*q == '\r' || *q == '\n') {
      /* A broken client (read: WinXP telnet) might send only CR, and not CRLF
       * so it's nice of us to try to handle this.
       */
      *p = '\0';
      if (is_first && is_http_request(d->raw_input)) {
        if (options.use_ws && is_websocket(d->raw_input)) {
          /* Continue processing as a WebSockets upgrade request. */
          d->conn_flags |= CONN_WEBSOCKETS_REQUEST;
        } else {
          if (process_http_start(d, d->raw_input)) {
            if ((qend - q) > 0) {
              if (*q == '\r')
                q++;
              if (*q == '\n')
                q++;
              process_http_input(d, q, qend - q);
            }
          }
          return;
        }
      } else {
        save_command(d, d->raw_input);
      }
      p = d->raw_input;
      if (*q == '\r' && ((q + 1) < qend) && (*(q + 1) == '\n'))
        q++; /* For clients that work */
    } else if (*q == '\b') {
      if (p > d->raw_input)
        p--;
    } else if (*q == IAC) { /* Telnet option foo */
      if (q >= qend)
        break;
      q++; /* Skip over IAC */

      if (!MAYBE_TELNET_ABLE(d) || handle_telnet(d, &q, qend) == 0) {
        if (p < pend)
          *p++ = *q;
      }
    } else if (p < pend) {
      *p++ = *q;
    }
  }
  if (p > d->raw_input) {
    d->raw_input_at = p;
  } else {
    mush_free(d->raw_input, "descriptor_raw_input");
    d->raw_input = 0;
    d->raw_input_at = 0;
  }

  d->conn_flags &= ~CONN_AWAITING_FIRST_DATA;
}

/* ARGSUSED */
static int
process_input(DESC *d, int output_ready __attribute__((__unused__)))
{
  int got = 0;
  char tbuf1[BUFFER_LEN];

  errno = 0;

  if (d->ssl) {
    /* Ensure that we're not in a state where we need an SSL_handshake() */
    if (ssl_need_handshake(d->ssl_state)) {
      d->ssl_state = ssl_handshake(d->ssl);
      if (d->ssl_state < 0) {
        /* Fatal error */
        ssl_close_connection(d->ssl);
        d->ssl = NULL;
        d->ssl_state = 0;
        return 0;
      } else if (ssl_need_handshake(d->ssl_state)) {
        /* We're still not ready to send to this connection. Alas. */
        return 1;
      }
    }
    /* Ensure that we're not in a state where we need an SSL_accept() */
    if (ssl_need_accept(d->ssl_state)) {
      d->ssl_state = ssl_accept(d->ssl);
      if (d->ssl_state < 0) {
        /* Fatal error */
        ssl_close_connection(d->ssl);
        d->ssl = NULL;
        d->ssl_state = 0;
        return 0;
      } else if (ssl_need_accept(d->ssl_state)) {
        /* We're still not ready to send to this connection. Alas. */
        return 1;
      }
    }
    /* It's an SSL connection, proceed accordingly */
    d->ssl_state = ssl_read(d->ssl, d->ssl_state, 1, output_ready, tbuf1,
                            sizeof tbuf1, &got);
    if (d->ssl_state < 0) {
      /* Fatal error */
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
      return 0;
    }
  } else {
    got = recv(d->descriptor, tbuf1, sizeof tbuf1, 0);
    if (got <= 0) {
      /* At this point, select() says there's data waiting to be read from
       * the socket, but we shouldn't assume that read() will actually get it
       * and blindly act like a got of -1 is a disconnect-worthy error.
       */
      if (is_blocking_err(errno))
        return 1;
      else {
        shutdownsock(d, "socket error", NOTHING, CONN_NOWRITE);
        return 0;
      }
    }
  }

  process_input_helper(d, tbuf1, got);

  return 1;
}

static void
set_userstring(char **userstring, const char *command)
{
  if (*userstring) {
    mush_free(*userstring, "userstring");
    *userstring = NULL;
  }
  /* command may be NULL */
  if (command && command[0]) {
    while (*command && isspace(*command))
      command++;
    if (*command)
      *userstring = mush_strdup(command, "userstring");
  }
}

/* For all connected descriptors.
 *
 * 1) If there is any command ready to be run, and the descriptor's quota is
 *    high enough, run it.
 * 2) If it is an http connection, and http_quota is high enough, execute the
 *    http command.
 *
 * And repeat until (1) and (2) fail.
 *
 * What does this mean? If Bob, Joe and Jane all have 2 commands, and Jane has
 * 4, then 1 command is run for each of them (in reverse order of first
 * connection time), until all of them have no commands OR have exceeded their
 * quota.
 *
 * All HTTP connections that are ready are processed on the first run.
 */
static void
process_commands(void)
{
  int nprocessed;

  do {
    DESC *cdesc;

    nprocessed = 0;
    DESC_ITER (cdesc) {
      struct text_block *t;
      /* Should they be disconnected? If so, ignore. */
      if (cdesc->conn_flags & CONN_SHUTDOWN)
        continue;

      if ((t = cdesc->input.head) != NULL) {
        enum comm_res retval;

        if (cdesc->quota < MS_PER_SEC && !disable_socket_quota)
          continue;

        cdesc->quota -= MS_PER_SEC;
        nprocessed += 1;
        start_cpu_timer();
        retval = do_command(cdesc, (char *) t->start);
        reset_cpu_timer();

        switch (retval) {
        case CRES_QUIT:
          shutdownsock(cdesc, "quit", cdesc->player, 0);
          break;
        case CRES_SITELOCK:
          shutdownsock(cdesc, "sitelocked", NOTHING, CONN_NOWRITE);
          break;
        case CRES_LOGOUT:
          logout_sock(cdesc);
        /* Falls through - to free input buffer */
        case CRES_OK:
          cdesc->input.head = t->nxt;
          if (!cdesc->input.head)
            cdesc->input.tail = NULL;
#ifdef DEBUG
          do_rawlog(LT_TRACE, "free_text_block(%p) at 5.", (void *) t);
#endif /* DEBUG */
          free_text_block(t);
          break;
        case CRES_BOOTED:
          break;
        }
      } else if ((cdesc->conn_flags & CONN_HTTP_READY) &&
                 !(cdesc->conn_flags & CONN_HTTP_CLOSE)) {
        if (http_quota >= MS_PER_SEC) {
          http_quota -= MS_PER_SEC;
          do_http_command(cdesc);
          nprocessed++;
        } else if (HTTP_SECOND_LIMIT < 1) {
          /* This should only happen if we're being hammered, and somebody does
           * @config/set http_per_second=0 to stop it. New requests get bounced
           * via mud_url handler, but active open ones end up here. We'll just
           * close it, rather than juggle it with mud_url.
           */
          cdesc->conn_flags |= CONN_HTTP_CLOSE;
        }
      }
    }
  } while (nprocessed > 0);
}

static void
http_bounce_mud_url(DESC *d)
{
  char buf[BUFFER_LEN];
  char *bp = buf;
  bool has_url = strncmp(MUDURL, "http", 4) == 0;
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
  if (has_url) {
    safe_format(buf, &bp, "<meta http-equiv=\"refresh\" content=\"5; url=%s\">",
                MUDURL);
  }
  safe_str("</HEAD><BODY><h1>Oops!</h1>", buf, &bp);
  if (has_url) {
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

#define HTTP_HEADER 1
#define HTTP_BODY 2
#define HTTP_DONE 3

#define HTTP_CONTENT_LENGTH "CONTENT-LENGTH: "

static int
process_http_start(DESC *d, char *line)
{
  struct http_request *req;
  char *c, *method, *path, *version;
  const char *reason = "Malformed Request";
  char buff[BUFFER_LEN];

  if (d->conn_timer) {
    sq_cancel(d->conn_timer);
    d->conn_timer = NULL;
  }

  if (!USABLE(HTTP_HANDLER) || !IsPlayer(HTTP_HANDLER) ||
      (HTTP_SECOND_LIMIT < 1)) {
    reason = "No HTTPHandler";
    goto bad_connection;
  }

  /* At this point, we're expecting an HTTP request. This first line
   * contains:
   *
   * METHOD /path/request HTTP/1.1
   */

  /* Get METHOD, PATH, and VERSION */
  method = line;
  for (c = line; *c && !isspace(*c); c++)
    ;

  if (!*c)
    goto bad_connection;

  *(c++) = '\0';

  if (strlen(method) >= HTTP_METHOD_LEN)
    goto bad_connection;

  /* Skip ahead to the path. */
  for (path = c; *path && isspace(*path); path++)
    ;
  if (!*path)
    goto bad_connection;

  for (c = path; *c && !isspace(*c); c++)
    ;
  if (!*c)
    goto bad_connection;

  *(c++) = '\0';
  if (strlen(path) >= MAX_COMMAND_LEN) {
    reason = "Path too long";
    goto bad_connection;
  }

  version = c;
  /* HTTP/1.0  is sent by apache bench, HTTP/1.1 by most other users. */
  if (strncmp(version, "HTTP/1", 6)) {
    reason = "Invalid HTTP Version";
    goto bad_connection;
  }

  req = mush_malloc(sizeof(struct http_request), "http_request");
  memset(req, 0, sizeof *req);

  d->http_request = req;
  req->state = HTTP_HEADER;
  req->content_length = -1;
  req->content_read = 0;

  /* req->inheaders and req->inbody for input */
  req->inhp = req->inheaders;
  req->inbp = req->inbody;

  /* req->headers and response for output */
  req->hp = req->headers;
  req->rp = req->response;

  strncpy(req->method, method, HTTP_METHOD_LEN - 1);
  strncpy(req->path, path, MAX_COMMAND_LEN - 1);

  d->conn_flags |= CONN_HTTP_REQUEST;
  d->conn_flags &= ~CONN_AWAITING_FIRST_DATA;
  /* Default for HTTP response */
  strncpy(req->code, "HTTP/1.1 200 OK", HTTP_CODE_LEN);
  strncpy(req->ctype, "Content-Type: text/plain", MAX_COMMAND_LEN);

  /* Check @sitelock for HTTP_Handler to see if this host is allowed to
   * use HTTP. It is unlikely we'll have a Hostname by this point,
   * so I'm declaring we'll just work with IPs.
   */
  if (!Site_Can_Connect(d->ip, HTTP_HANDLER)) {
    if (!Deny_Silent_Site(d->ip, HTTP_HANDLER)) {
      queue_event(SYSEVENT, "HTTP`BLOCKED", "%d,%s,%s,%s,%s", d->descriptor,
                  d->ip, req->method, req->path,
                  "http: IP sitelocked !connect");
    }
    reason = NULL;
    goto bad_connection;
  }

  /* Now that we have the path, let's check it for sitelock.
   * Yes, I'm pretending path is a hostname! It works!
   */
  snprintf(buff, BUFFER_LEN, "%s`%s`%s", d->ip, req->method, req->path);
  if (!Site_Can_Connect(buff, HTTP_HANDLER)) {
    if (!Deny_Silent_Site(buff, HTTP_HANDLER)) {
      queue_event(SYSEVENT, "HTTP`BLOCKED", "%d,%s,%s,%s,%s", d->descriptor,
                  d->ip, req->method, req->path,
                  "http: path sitelocked !connect");
    }
    reason = NULL;
    goto bad_connection;
  }

  d->conn_timer = sq_register_in(2, http_finished_wrapper, (void *) d, NULL);

  return 1;

bad_connection:
  http_bounce_mud_url(d);
  if (reason) {
    queue_event(SYSEVENT, "HTTP`FAIL", "%d,%s,%s", d->descriptor, d->ip,
                reason);
  }
  d->conn_flags |= CONN_HTTP_CLOSE;
  return 0;
}

bool
http_finished_wrapper(void *data)
{
  DESC *d = (DESC *) data;
  d->conn_timer = NULL;
  http_command_ready(d);
  return false;
}

static void
process_http_input(DESC *d, char *buf, int len)
{
  char *p, *eol, *val;
  struct http_request *req;
  if (d->conn_timer) {
    sq_cancel(d->conn_timer);
    d->conn_timer = NULL;
  }

  req = d->http_request;

  switch (req->state) {
  case HTTP_HEADER:
    /* Copy to header buffer, then check headers to see if they're finished. */
    safe_strl(buf, len, req->inheaders, &(req->inhp));
    *(req->inhp) = '\0';
    p = d->http_request->inheaders;
    while (p && *p) {
      if (*p == '\r' || *p == '\n') {
        /* End of headers.
         * Shove the rest to inbody, if any.
         * p should be "\r\n" by spec, but might not be, thanks to lazy
         * script writers, ancient browsers, etc.
         */
        if (*p == '\r' && *(p + 1) == '\n')
          *(p++) = '\0';
        *(p++) = '\0';
        if (req->content_length == 0 ||
            (req->content_length < 0 && is_http_bodyless(req->method))) {
          /* We're done, queue! */
          http_command_ready(d);
          return;
        }
        req->state = HTTP_BODY;
        if (*p && req->content_length) {
          /* Switch to body input. content_length of -1 (No content length
           * received) means we depend on timer. Shove rest of body into it.
           */
          buf = p;
          len = (req->inhp - p);
          goto readbody;
        }
      }
      for (eol = p; *eol && *eol != '\r' && *eol != '\n'; eol++)
        ;
      if (!*eol) {
        /* Incomplete headers, wait until we read more. */
        goto waitmore;
      }
      if (*eol == '\r' && *(eol + 1) == '\n')
        eol++;
      if (*eol)
        eol++;
      /* We have a header, check for Content-Length: */
      if (!strncasecmp(p, HTTP_CONTENT_LENGTH, strlen(HTTP_CONTENT_LENGTH))) {
        val = p + strlen(HTTP_CONTENT_LENGTH);
        errno = 0;
        req->content_length = strtol(val, NULL, 10);
        if (req->content_length < 0 || errno) {
          /* Malformed content_length, fall back to timer for handling */
          req->content_length = -1;
          continue;
        }
      }
      p = eol;
    }
    break;
  case HTTP_BODY:
  readbody:
    safe_strl(buf, len, req->inbody, &(req->inbp));
    if (req->content_length > 0 &&
        (req->inbp - req->inbody) >= req->content_length) {
      http_command_ready(d);
      return;
    }
    break;
  }

/* Reset the timer, but only if there is one. */
waitmore:
  d->conn_timer = sq_register_in(2, http_finished_wrapper, (void *) d, NULL);
}

static void
http_command_ready(DESC *d)
{
  /* All we really do is clear the timer, and mark the socket
   * ready for running at the next queue cycle.
   */
  if (d->conn_timer) {
    sq_cancel(d->conn_timer);
    d->conn_timer = NULL;
  }
  /* Don't run twice - from close_write _and_ timer (which shouldn't happen
   * anyway?) */
  if (d->conn_flags & CONN_HTTP_READY)
    return;
  d->conn_flags |= CONN_HTTP_READY;
}

static void
do_http_command(DESC *d)
{
  NEW_PE_INFO *pe_info = NULL;
  struct http_request *req;
  char headernames[BUFFER_LEN];
  char *hp;
  char tmp[BUFFER_LEN];
  char vals[BUFFER_LEN];
  uint32_t content_len;
  char *p, *line;
  char *headername, *headerval;
  const char *rval;
  const char *reason = "Malformed Request";

  if (d->conn_timer)
    sq_cancel(d->conn_timer);
  d->conn_timer = NULL;

  if (!(d->conn_flags & CONN_HTTP_REQUEST)) {
    reason = "not a request";
    goto bad_connection;
  }
  if (d->conn_flags & CONN_HTTP_CLOSE) {
    reason = "closed";
    goto bad_connection;
  }
  if (!(d->http_request)) {
    reason = "no request struct";
    goto bad_connection;
  }

  req = d->http_request;

  pe_info = make_pe_info("pe_info-http");

  *(req->inhp) = '\0';
  *(req->inbp) = '\0';

  p = req->inheaders;
  hp = headernames;
  while (*p) {
    line = p;
    while (*p && *p != '\r' && *p != '\n')
      p++;
    /* Chomp, \r, \n, or \r\n (should be \r\n). */
    if (*p == '\r')
      *(p++) = '\0';
    if (*p == '\n')
      *(p++) = '\0';
    headername = line;
    headerval = strstr(line, ": ");
    if (!headerval) {
      reason = "Malformed header";
      goto bad_connection;
    }
    *(headerval) = '\0';
    headerval += 2; /* skip past ": " */
    /* Normalize the header name into Q-reg acceptable name */
    pi_regs_normalize_key(headername);
    snprintf(tmp, BUFFER_LEN, "HDR.%s", headername);
    rval = PE_Getq(pe_info, tmp);
    if (rval && *rval) {
      snprintf(vals, BUFFER_LEN, "%s\n%s", rval, headerval);
      if (!PE_Setq(pe_info, tmp, vals)) {
        /* Too many headers? Ignore */
        continue;
      }
    } else {
      if (!PE_Setq(pe_info, tmp, headerval)) {
        /* Too many headers? Ignore */
        continue;
      }
      if (hp > headernames) {
        safe_chr(' ', headernames, &hp);
      }
      safe_str(headername, headernames, &hp);
    }
  }

  *hp = '\0';
  PE_Setq(pe_info, "HEADERS", headernames);

  pe_regs_setenv(pe_info->regvals, 0, req->path);
  pe_regs_setenv(pe_info->regvals, 1, req->inbody);

  /* 'invisibly' connect. */
  d->player = HTTP_HANDLER;
  d->connected = CONN_PLAYER;
  d->connected_at = mudtime;

  /* Buffer all output that HTTP_HANDLER receives */
  d->conn_flags |= CONN_HTTP_BUFFER;

  active_http_request = req;
  run_http_command(HTTP_HANDLER, d->descriptor, d->http_request->method,
                   pe_info);

  d->player = NOTHING;
  d->connected = CONN_SCREEN;

  /* pe_info is freed by the parser */
  pe_info = NULL;
  active_http_request = NULL;

  /* Clear the buffer flag so we stop hijacking output */
  d->conn_flags &= ~CONN_HTTP_BUFFER;

  content_len = req->rp - req->response;

  queue_event(SYSEVENT, "HTTP`COMMAND", "%s,%s,%s,%s,%s,%ld,%d", d->ip,
              req->method, req->path, req->code, req->ctype,
              strlen(req->inbody), content_len);

  /* Now write out our response header, populated by @respond, then body. */
  queue_newwrite(d, req->code, strlen(req->code));
  queue_newwrite(d, "\r\n", 2);
  queue_newwrite(d, req->ctype, strlen(req->ctype));
  queue_newwrite(d, "\r\n", 2);
  queue_newwrite(d, req->headers, strlen(req->headers));
  snprintf(tmp, BUFFER_LEN, "Content-Length: %d\r\n\r\n", content_len);
  queue_newwrite(d, tmp, strlen(tmp));

  queue_newwrite(d, req->response, content_len);

  d->conn_flags |= CONN_HTTP_CLOSE;
  return;
bad_connection:
  http_bounce_mud_url(d);
  if (reason) {
    queue_event(SYSEVENT, "HTTP`FAIL", "%d,%s,%s", d->descriptor, d->ip,
                reason);
  }
  if (pe_info)
    free_pe_info(pe_info);
  d->conn_flags |= CONN_HTTP_CLOSE;
}

static bool
is_http_request(const char *command)
{
  const char *methods[] = {"GET ",    "POST ", "PUT ", "DELETE ",
                           "UPDATE ", "HEAD ", NULL};

  int i;

  for (i = 0; methods[i]; i++) {
    if (!strncmp(command, methods[i], strlen(methods[i]))) {
      return 1;
    }
  }
  return 0;
}

static bool
is_http_bodyless(const char *method)
{
  const char *methods[] = {"GET", "DELETE", "HEAD", NULL};

  int i;

  for (i = 0; methods[i]; i++) {
    if (!strcmp(method, methods[i])) {
      return 1;
    }
  }
  return 0;
}

/** Send a descriptor's output prefix */
#define send_prefix(d)                                                         \
  if (d->output_prefix) {                                                      \
    queue_newwrite(d, d->output_prefix, strlen(d->output_prefix));             \
    queue_eol(d);                                                              \
  }

/** Send a descriptor's output suffix */
#define send_suffix(d)                                                         \
  if (d->output_suffix) {                                                      \
    queue_newwrite(d, d->output_suffix, strlen(d->output_suffix));             \
    queue_eol(d);                                                              \
  }

/** Parse a command entered at the socket.
 * \param d descriptor
 * \param command command to parse
 * \return CRES_* enum
 */
static enum comm_res
do_command(DESC *d, char *command)
{
  int j;

  if (d->conn_flags & CONN_WEBSOCKETS_REQUEST) {
    /* Parse WebSockets upgrade request. */
    if (!process_websocket_request(d, command)) {
      return CRES_QUIT;
    }

    return CRES_OK;
  }

  if (!*command) {
    /* Blank lines are ignored by Penn, only used by
     * websockets and HTTP requests.
     */
    return CRES_OK;
  }

  if (!strncmp(command, IDLE_COMMAND, strlen(IDLE_COMMAND))) {
    j = strlen(IDLE_COMMAND);
    if ((int) strlen(command) > j) {
      if (*(command + j) == ' ')
        j++;
      queue_write(d, command + j, strlen(command) - j);
      queue_eol(d);
    }
    return CRES_OK;
  }
  d->last_time = mudtime;
  (d->cmds)++;
  if (SUPPORT_PUEBLO &&
      !strncmp(command, PUEBLO_COMMAND, strlen(PUEBLO_COMMAND))) {
    parse_puebloclient(d, command);
    if (!(d->conn_flags & CONN_HTML)) {
      queue_newwrite(d, PUEBLO_SEND, strlen(PUEBLO_SEND));
      process_output(d);
      do_rawlog(LT_CONN, "[%d/%s/%s] Switching to Pueblo mode.", d->descriptor,
                d->addr, d->ip);
      d->conn_flags |= CONN_HTML;
      if (!d->connected && !d->conn_timer)
        welcome_user(d, 1);
    } else {
      /* Resend the Pueblo start string, but not the 'clear screen'
       * part. Only useful for someone whose client hasn't noticed
       * they're in Pueblo mode and is showing raw HTML */
      queue_newwrite(d, PUEBLO_SEND_SHORT, strlen(PUEBLO_SEND_SHORT));
    }
    return CRES_OK;
  }
  if (d->conn_timer) {
    sq_cancel(d->conn_timer);
    d->conn_timer = NULL;
    welcome_user(d, 1);
  }
  if (!strcmp(command, QUIT_COMMAND)) {
    return CRES_QUIT;
  } else if (!strcmp(command, LOGOUT_COMMAND)) {
    return CRES_LOGOUT;
  } else if (!strcmp(command, INFO_COMMAND)) {
    send_prefix(d);
    dump_info(d);
    send_suffix(d);
  } else if (!strcmp(command, MSSPREQUEST_COMMAND)) {
    send_prefix(d);
    report_mssp(d, NULL, NULL);
    send_suffix(d);
  } else if (!strncmp(command, PREFIX_COMMAND, strlen(PREFIX_COMMAND))) {
    set_userstring(&d->output_prefix, command + strlen(PREFIX_COMMAND));
  } else if (!strncmp(command, SUFFIX_COMMAND, strlen(SUFFIX_COMMAND))) {
    set_userstring(&d->output_suffix, command + strlen(SUFFIX_COMMAND));
  } else if (!strncmp(command, "SCREENWIDTH", 11)) {
    d->width = parse_integer(command + 11);
  } else if (!strncmp(command, "SCREENHEIGHT", 12)) {
    d->height = parse_integer(command + 12);
  } else if (!strncmp(command, "PROMPT_NEWLINES", 15)) {
    if (parse_integer(command + 15))
      d->conn_flags |= CONN_PROMPT_NEWLINES;
    else
      d->conn_flags &= ~CONN_PROMPT_NEWLINES;
  } else if (!strncmp(command, "SOCKSET", 7)) {
    sockset_wrapper(d, command + 7);
  } else {
    if (d->connected) {
      int fd = d->descriptor;
      DESC *tmp;
      send_prefix(d);
      run_user_input(d->player, d->descriptor, command);
      /* Check to make sure the descriptor hasn't been closed while
       * running the command, via @force/inline someobj=@boot %# */
      tmp = im_find(descs_by_fd, fd);
      if (tmp) {
        send_suffix(d);
      } else {
        return CRES_BOOTED;
      }
    } else {
      j = 0;
      if (!strncmp(command, WHO_COMMAND, strlen(WHO_COMMAND))) {
        j = strlen(WHO_COMMAND);
      } else if (!strncmp(command, DOING_COMMAND, strlen(DOING_COMMAND))) {
        j = strlen(DOING_COMMAND);
      } else if (!strncmp(command, SESSION_COMMAND, strlen(SESSION_COMMAND))) {
        j = strlen(SESSION_COMMAND);
      }
      if (j) {
        send_prefix(d);
        if (!fcache_dump(d, fcache.who_fcache, NULL, command + j))
          dump_users(d, command + j);
        send_suffix(d);
      } else if (!check_connect(d, command)) {
        return CRES_SITELOCK;
      }
    }
  }
  return CRES_OK;
}

/** Parse a PUEBLOCLIENT [md5="checksum"] string
 * \param d descriptor
 * \param command string to parse
 */
static void
parse_puebloclient(DESC *d, char *command)
{
  const char *p, *end;
  if ((p = string_match(command, "md5="))) {
    /* Skip md5=" */
    p += 5;
    if ((end = strchr(p, '"'))) {
      if ((end > p) && ((end - p) <= PUEBLO_CHECKSUM_LEN)) {
        /* Got it! */
        mush_strncpy(d->checksum, p, end - p);
      }
    }
  }
}

/** Show all the appropriate messages when a player attempts to log in.
 * \param d descriptor
 * \param player dbref of player
 * \param isnew has the player just been created?
 * \retval 0 player failed to log in
 * \retval 1 player logged in successfully
 */
static int
dump_messages(DESC *d, dbref player, int isnew)
{
  int num = 0;
  DESC *tmpd;

  d->connected = CONN_PLAYER;
  d->connected_at = mudtime;
  d->player = player;

  connlog_login(d->connlog_id, player);

  login_number++;
  if (MAX_LOGINS) {
    /* check for exceeding max player limit */
    if (under_limit && (login_number > MAX_LOGINS)) {
      under_limit = 0;
      do_rawlog(LT_CONN, "Limit of %d players reached. Logins disabled.\n",
                MAX_LOGINS);
    }
  }
  /* give players a message on connection */
  if (!options.login_allow || !under_limit ||
      (Guest(player) && !options.guest_allow)) {
    if (!options.login_allow) {
      fcache_dump(d, fcache.down_fcache, NULL, NULL);
      if (*cf_downmotd_msg) {
        queue_write(d, cf_downmotd_msg, strlen(cf_downmotd_msg));
        queue_eol(d);
      }
    } else if (MAX_LOGINS && !under_limit) {
      fcache_dump(d, fcache.full_fcache, NULL, NULL);
      if (*cf_fullmotd_msg) {
        queue_write(d, cf_fullmotd_msg, strlen(cf_fullmotd_msg));
        queue_eol(d);
      }
    }
    if (!Can_Login(player)) {
      /* when the connection has been refused, we want to update the
       * LASTFAILED info on the player
       */
      check_lastfailed(player, d->addr);
      return 0;
    }
  }

  /* check to see if this is a reconnect */
  DESC_ITER_CONN (tmpd) {
    if (tmpd->player == player) {
      num++;
    }
  }
  /* give permanent text messages */
  if (isnew)
    fcache_dump(d, fcache.newuser_fcache, NULL, NULL);
  if (num == 1) {
    fcache_dump(d, fcache.motd_fcache, NULL, NULL);
    if (Hasprivs(player))
      fcache_dump(d, fcache.wizmotd_fcache, NULL, NULL);
  }
  if (Guest(player))
    fcache_dump(d, fcache.guest_fcache, NULL, NULL);

  if (ModTime(player))
    notify_format(player, T("%ld failed connections since last login."),
                  (long) ModTime(player));
  ModTime(player) = (time_t) 0;
  announce_connect(d, isnew, num);    /* broadcast connect message */
  check_last(player, d->addr, d->ip); /* set Last, Lastsite, give paycheck */
  /* Check all mail folders. If empty, report lack of mail. */
  queue_eol(d);
  if (can_mail(player)) {
    check_all_mail(player);
  }
  set_player_folder(player, 0);
  do_look_around(player);
  if (Haven(player))
    notify(player, T("Your HAVEN flag is set. You cannot receive pages."));
  if (Vacation(player)) {
    notify(player, T("Welcome back from vacation! Don't forget to unset your "
                     "ON-VACATION flag"));
  }
  local_connect(player, isnew, num);
  return 1;
}

/** Check if a string entered at the login screen is an attempt
 * to connect to or create/register a player.
 * \param d descriptor
 * \param msg string to parse
 * \retval 1 Connection successful, or failed due to too many incorrect pws
 * \retval 0 Connection failed (sitelock, max connections reached, etc)
 */
static int
check_connect(DESC *d, const char *msg)
{
  char command[MAX_COMMAND_LEN];
  char user[MAX_COMMAND_LEN];
  char password[MAX_COMMAND_LEN];
  char errbuf[BUFFER_LEN];
  dbref player;

  parse_connect(msg, command, user, password);

  /* fail quietly if command is an empty string */
  if (strlen(command) < 1)
    return 1;

  if (!check_fails(d->ip)) {
    queue_string_eol(d, "%s", T(connect_fail_limit_exceeded));
    return 1;
  }
  if (string_prefixe("connect", command)) {
    if ((player = connect_player(d, user, password, d->addr, d->ip, errbuf)) ==
        NOTHING) {
      queue_string_eol(d, "%s", errbuf);
      do_rawlog(LT_CONN, "[%d/%s/%s] Failed connect to '%s'.", d->descriptor,
                d->addr, d->ip, user);
    } else {
      do_rawlog(LT_CONN, "[%d/%s/%s] Connected to %s(#%d) in %s(#%d)",
                d->descriptor, d->addr, d->ip, Name(player), player,
                Name(Location(player)), Location(player));
      if ((dump_messages(d, player, 0)) == 0) {
        d->connected = CONN_DENIED;
        return 0;
      }
    }

  } else if (strcasecmp(command, "cd") == 0) {
    if ((player = connect_player(d, user, password, d->addr, d->ip, errbuf)) ==
        NOTHING) {
      queue_string_eol(d, "%s", errbuf);
      do_rawlog(LT_CONN, "[%d/%s/%s] Failed connect to '%s'.", d->descriptor,
                d->addr, d->ip, user);
    } else {
      do_rawlog(LT_CONN, "[%d/%s/%s] Connected dark to %s(#%d) in %s(#%d)",
                d->descriptor, d->addr, d->ip, Name(player), player,
                Name(Location(player)), Location(player));
      /* Set player dark */
      d->connected = CONN_PLAYER;
      if (Can_Hide(player))
        d->hide = 1;
      d->player = player;
      set_flag(player, player, "DARK", 0, 0, 0);
      if ((dump_messages(d, player, 0)) == 0) {
        d->connected = CONN_DENIED;
        d->hide = 0;
        return 0;
      }
    }

  } else if (strcasecmp(command, "cv") == 0) {
    if ((player = connect_player(d, user, password, d->addr, d->ip, errbuf)) ==
        NOTHING) {
      queue_string_eol(d, "%s", errbuf);
      do_rawlog(LT_CONN, "[%d/%s/%s] Failed connect to '%s'.", d->descriptor,
                d->addr, d->ip, user);
    } else {
      do_rawlog(LT_CONN, "[%d/%s/%s] Connected to %s(#%d) in %s(#%d)",
                d->descriptor, d->addr, d->ip, Name(player), player,
                Name(Location(player)), Location(player));
      /* Set player !dark */
      d->connected = CONN_PLAYER;
      d->player = player;
      set_flag(player, player, "DARK", 1, 0, 0);
      if ((dump_messages(d, player, 0)) == 0) {
        d->connected = CONN_DENIED;
        return 0;
      }
    }

  } else if (strcasecmp(command, "ch") == 0) {
    if ((player = connect_player(d, user, password, d->addr, d->ip, errbuf)) ==
        NOTHING) {
      queue_string_eol(d, "%s", errbuf);
      do_rawlog(LT_CONN, "[%d/%s/%s] Failed connect to '%s'.", d->descriptor,
                d->addr, d->ip, user);
    } else {
      do_rawlog(LT_CONN, "[%d/%s/%s] Connected hidden to %s(#%d) in %s(#%d)",
                d->descriptor, d->addr, d->ip, Name(player), player,
                Name(Location(player)), Location(player));
      /* Set player hidden */
      d->connected = CONN_PLAYER;
      d->player = player;
      if (Can_Hide(player))
        d->hide = 1;
      if ((dump_messages(d, player, 0)) == 0) {
        d->connected = CONN_DENIED;
        d->hide = 0;
        return 0;
      }
    }

  } else if (string_prefixe("create", command)) {
    if (!Site_Can_Create(d->addr) || !Site_Can_Create(d->ip)) {
      fcache_dump(d, fcache.register_fcache, NULL, NULL);
      if (!Deny_Silent_Site(d->addr, AMBIGUOUS) &&
          !Deny_Silent_Site(d->ip, AMBIGUOUS)) {
        do_rawlog(LT_CONN, "[%d/%s/%s] Refused create for '%s'.", d->descriptor,
                  d->addr, d->ip, user);
        queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                    d->descriptor, d->ip, count_failed(d->ip),
                    "create: sitelocked !create", user);
      }
      return 0;
    }
    if (!options.login_allow || !options.create_allow) {
      if (!options.login_allow) {
        fcache_dump(d, fcache.down_fcache, NULL, NULL);
        if (*cf_downmotd_msg) {
          queue_write(d, cf_downmotd_msg, strlen(cf_downmotd_msg));
          queue_eol(d);
        }
      } else
        fcache_dump(d, fcache.register_fcache, NULL, NULL);
      do_rawlog(LT_CONN, "REFUSED CREATION for %s from %s on descriptor %d.\n",
                user, d->addr, d->descriptor);
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, d->ip, count_failed(d->ip),
                  "create: creation not allowed", user);
      return 0;
    } else if (MAX_LOGINS && !under_limit) {
      fcache_dump(d, fcache.full_fcache, NULL, NULL);
      if (*cf_fullmotd_msg) {
        queue_write(d, cf_fullmotd_msg, strlen(cf_fullmotd_msg));
        queue_eol(d);
      }
      do_rawlog(LT_CONN, "REFUSED CREATION for %s from %s on descriptor %d.\n",
                user, d->addr, d->descriptor);
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, d->ip, count_failed(d->ip),
                  "create: max login count reached", user);
      return 0;
    }
    player = create_player(d, NOTHING, user, password, d->addr, d->ip);
    switch (player) {
    case NOTHING:
    case AMBIGUOUS:
      queue_string_eol(
        d, "%s",
        T((player == NOTHING ? create_fail_bad : create_fail_preexisting)));
      do_rawlog(LT_CONN, "[%d/%s/%s] Failed create for '%s' (bad name).",
                d->descriptor, d->addr, d->ip, user);
      break;
    case HOME:
      queue_string_eol(d, "%s", T(password_fail));
      do_rawlog(LT_CONN, "[%d/%s/%s] Failed create for '%s' (bad password).",
                d->descriptor, d->addr, d->ip, user);
      break;
    default:
      queue_event(SYSEVENT, "PLAYER`CREATE", "%s,%s,%s,%d",
                  unparse_objid(player), Name(player), "create", d->descriptor);
      do_rawlog(LT_CONN, "[%d/%s/%s] Created %s(#%d)", d->descriptor, d->addr,
                d->ip, Name(player), player);
      if ((dump_messages(d, player, 1)) == 0) {
        d->connected = CONN_DENIED;
        return 0;
      }
      break;
    } /* successful player creation */

  } else if (string_prefixe("register", command)) {
    if (!Site_Can_Register(d->addr) || !Site_Can_Register(d->ip)) {
      fcache_dump(d, fcache.register_fcache, NULL, NULL);
      if (!Deny_Silent_Site(d->addr, AMBIGUOUS) &&
          !Deny_Silent_Site(d->ip, AMBIGUOUS)) {
        do_rawlog(LT_CONN,
                  "[%d/%s/%s] Refused registration (bad site) for '%s'.",
                  d->descriptor, d->addr, d->ip, user);
        queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                    d->descriptor, d->ip, mark_failed(d->ip),
                    "register: sitelocked host or ip", user);
      }
      return 0;
    }
    if (!options.create_allow) {
      fcache_dump(d, fcache.register_fcache, NULL, NULL);
      do_rawlog(LT_CONN,
                "Refused registration (creation disabled) for %s from "
                "%s on descriptor %d.\n",
                user, d->addr, d->descriptor);
      queue_event(SYSEVENT, "SOCKET`CREATEFAIL", "%d,%s,%d,%s,%s",
                  d->descriptor, d->ip, mark_failed(d->ip),
                  "register: registration disabled", user);
      return 0;
    }
    if ((player = email_register_player(d, user, password, d->addr, d->ip)) ==
        NOTHING) {
      queue_string_eol(d, "%s", T(register_fail));
      do_rawlog(LT_CONN, "[%d/%s/%s] Failed registration for '%s'.",
                d->descriptor, d->addr, d->ip, user);
    } else {
      queue_string_eol(d, "%s", T(register_success));
      do_rawlog(LT_CONN, "[%d/%s/%s] Registered %s(#%d) to %s", d->descriptor,
                d->addr, d->ip, Name(player), player, password);
    }
    /* Whether it succeeds or fails, leave them connected */

  } else {
    /* invalid command, just repeat login screen */
    welcome_user(d, 0);
  }
  return 1;
}

/** Attempt to parse a string entered at the connect screen
 * as 'connect name password'.
 * \param msg1 string to parse
 * \param command pointer to store the first word in
 * \param user pointer to store the username - possibly given in quotes - in
 * \param pass pointer to store the password in
 */
static void
parse_connect(const char *msg1, char *command, char *user, char *pass)
{
  char *p;
  const char *msg = msg1;

  while (*msg && isspace(*msg))
    msg++;
  p = command;
  while (*msg && isprint(*msg) && !isspace(*msg))
    *p++ = *msg++;
  *p = '\0';
  while (*msg && isspace(*msg))
    msg++;
  p = user;

  if (*msg == '\"') {
    for (; *msg && ((*msg == '\"') || isspace(*msg)); msg++)
      ;
    while (*msg && (*msg != '\"')) {
      while (*msg && !isspace(*msg) && (*msg != '\"'))
        *p++ = *msg++;
      if (*msg == '\"') {
        msg++;
        while (*msg && isspace(*msg))
          msg++;
        break;
      }
      while (*msg && isspace(*msg))
        msg++;
      if (*msg && (*msg != '\"'))
        *p++ = ' ';
    }
  } else
    while (*msg && isprint(*msg) && !isspace(*msg))
      *p++ = *msg++;

  *p = '\0';
  while (*msg && isspace(*msg))
    msg++;
  p = pass;
  while (*msg && isprint(*msg) && !isspace(*msg))
    *p++ = *msg++;
  *p = '\0';
}

/** Close all connections to the MUSH */
static void
close_sockets(void)
{
  DESC *d, *dnext;
  const char *shutmsg;
  int shutlen;
  int ignoreme __attribute__((__unused__));

  shutmsg = T(shutdown_message);
  shutlen = strlen(shutmsg);

  for (d = descriptor_list; d; d = dnext) {
    dnext = d->next;
    if (!d->ssl) {
#ifdef HAVE_WRITEV
      struct iovec byebye[2];
      byebye[0].iov_base = (char *) shutmsg;
      byebye[0].iov_len = shutlen;
      byebye[1].iov_base = (char *) "\r\n";
      byebye[1].iov_len = 2;
      ignoreme = writev(d->descriptor, byebye, 2);
#else
      send(d->descriptor, shutmsg, shutlen, 0);
      send(d->descriptor, (char *) "\r\n", 2, 0);
#endif
    } else {
      int offset;
      offset = 0;
      ssl_write(d->ssl, d->ssl_state, 0, 1, shutmsg, shutlen, &offset);
      offset = 0;
      ssl_write(d->ssl, d->ssl_state, 0, 1, "\r\n", 2, &offset);
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->ssl_state = 0;
    }
    if (is_remote_desc(d)) {
      if (shutdown(d->descriptor, 2) < 0)
        penn_perror("shutdown");
    }
    closesocket(d->descriptor);
  }
  shutdown_conndb(0);
}

/** Give everyone the boot.
 */
void
emergency_shutdown(void)
{
  close_sockets();
#ifdef INFO_SLAVE
  kill_info_slave();
#endif
}

/** Boot a player.
 * Boot all connections associated with victim, or all idle connections if
 * idleonly is true
 * \param player the player being booted
 * \param idleonly only boot idle connections?
 * \param silent suppress notice to player that he's being booted?
 * \param booter the dbref of the player doing the booting
 * \return number of descriptors booted
 */
int
boot_player(dbref player, int idleonly, int silent, dbref booter)
{
  DESC *d, *ignore = NULL, *boot = NULL;
  int count = 0;
  time_t now = mudtime;

  if (idleonly)
    ignore = least_idle_desc(player, 1);

  DESC_ITER_CONN (d) {
    if (boot) {
      boot_desc(boot, "boot", booter);
      boot = NULL;
    }
    if (d->player == player &&
        (!ignore || (d != ignore && difftime(now, d->last_time) > 60.0))) {
      if (!idleonly && !silent && !count)
        notify(player, T("You are politely shown to the door."));
      count++;
      boot = d;
    }
  }

  if (boot)
    boot_desc(boot, "boot", booter);

  if (count && idleonly) {
    if (count == 1)
      notify(player, T("You boot an idle self."));
    else
      notify_format(player, T("You boot %d idle selves."), count);
  }

  return count;
}

/** Disconnect a descriptor.
 * \param d pointer to descriptor to disconnect.
 * \param cause the reason for the descriptor being disconnected, used for
 * events
 * \param executor object causing the boot
 */
void
boot_desc(DESC *d, const char *cause, dbref executor)
{
  shutdownsock(d, cause, executor, 0);
}

/** For sockset: Parse an english bool ('yes', 'no', etc). Assume no,
 *  whitelist yes.
 * \param str The string to check
 * \retval 1 yes
 * \retval 0 no
 */
static int
isyes(char *str)
{
  if (!str)
    return 0;
  if (!strcasecmp(str, "yes"))
    return 1;
  if (!strcasecmp(str, "y"))
    return 1;
  if (!strcasecmp(str, "true"))
    return 1;
  if (!strcasecmp(str, "1"))
    return 1;
  if (!strcasecmp(str, "on"))
    return 1;
  return 0;
}

static void
sockset_wrapper(DESC *d, char *cmd)
{
  const char *res;
  char *p;

  while (cmd && *cmd && isspace(*cmd))
    cmd++;

  if (!*cmd) {
    /* query all */
    res = sockset_show(d, ((d->conn_flags & CONN_HTML) ? "<br>\n" : "\r\n"));
    queue_newwrite(d, res, strlen(res));
    queue_eol(d);
    return;
  }

  if ((p = strchr(cmd, '='))) {
    /* set an option */
    *(p++) = '\0';
    res = sockset(d, cmd, p);
    queue_newwrite(d, res, strlen(res));
    queue_eol(d);
    return;
  } else {
    res = T("You must give an option and a value.");
    queue_newwrite(d, res, strlen(res));
    queue_eol(d);
  }
}

const char *
sockset_show(DESC *d, char *nl)
{
  static char buff[BUFFER_LEN];
  char *bp = buff;
  char colorstyle[SBUF_LEN];
  int ntype;
  int nllen = strlen(nl);

  safe_strl(nl, nllen, buff, &bp);

  if (d->output_prefix && *(d->output_prefix)) {
    safe_format(buff, &bp, "%-15s:  %s", PREFIX_COMMAND, d->output_prefix);
    safe_strl(nl, nllen, buff, &bp);
  }

  if (d->output_suffix && *(d->output_suffix)) {
    safe_format(buff, &bp, "%-15s:  %s", SUFFIX_COMMAND, d->output_suffix);
    safe_strl(nl, nllen, buff, &bp);
  }

  safe_format(buff, &bp, "%-15s:  %s", "Pueblo",
              (d->conn_flags & CONN_HTML ? "Yes" : "No"));
  safe_strl(nl, nllen, buff, &bp);
  safe_format(buff, &bp, "%-15s:  %s", "Telnet",
              (TELNET_ABLE(d) ? "Yes" : "No"));
  safe_strl(nl, nllen, buff, &bp);
  safe_format(buff, &bp, "%-15s:  %d", "Width", d->width);
  safe_strl(nl, nllen, buff, &bp);
  safe_format(buff, &bp, "%-15s:  %d", "Height", d->height);
  safe_strl(nl, nllen, buff, &bp);
  safe_format(buff, &bp, "%-15s:  %s", "Terminal Type",
              (d->ttype ? d->ttype : default_ttype));
  safe_strl(nl, nllen, buff, &bp);

  ntype = notify_type(d);

  safe_format(buff, &bp, "%-15s:  %s", "Stripaccents",
              (ntype & MSG_STRIPACCENTS ? "Yes" : "No"));
  safe_strl(nl, nllen, buff, &bp);

  if (ntype & MSG_XTERM256)
    strcpy(colorstyle, "xterm256");
  else if (ntype & MSG_ANSI16)
    strcpy(colorstyle, "16color");
  else if (ntype & MSG_ANSI2)
    strcpy(colorstyle, "hilite");
  else
    strcpy(colorstyle, "plain");

  if (d->conn_flags & CONN_COLORSTYLE)
    safe_format(buff, &bp, "%-15s:  %s", "Color Style", colorstyle);
  else
    safe_format(buff, &bp, "%-15s:  auto (%s)", "Color Style", colorstyle);
  safe_strl(nl, nllen, buff, &bp);
  safe_format(buff, &bp, "%-15s:  %s", "Prompt Newlines",
              (d->conn_flags & CONN_PROMPT_NEWLINES ? "Yes" : "No"));

  *bp = '\0';
  return buff;
}

/** Set a sock option.
 * \param d the descriptor to set the option on
 * \param name the option name
 * \param val the option value
 * \return string Set message (error or success)
 */
const char *
sockset(DESC *d, char *name, char *val)
{
  static char retval[BUFFER_LEN];
  int ival;

  if (!name || !name[0]) {
    return T("Set what option?");
  }

  if (!strcasecmp(name, PREFIX_COMMAND)) {
    set_userstring(&d->output_prefix, val);
    if (val && *val) {
      return T("OUTPUTPREFIX set.");
    } else {
      return T("OUTPUTPREFIX cleared.");
    }
    return retval;
  }

  if (!strcasecmp(name, SUFFIX_COMMAND)) {
    set_userstring(&d->output_suffix, val);
    if (val && *val) {
      return T("OUTPUTSUFFIX set.");
    } else {
      return T("OUTPUTSUFFIX cleared.");
    }
    return retval;
  }

  if (!strcasecmp(name, "PUEBLO")) {
    if (val && *val) {
      parse_puebloclient(d, val);
      if (!(d->conn_flags & CONN_HTML)) {
        queue_newwrite(d, PUEBLO_SEND, strlen(PUEBLO_SEND));
        process_output(d);
        do_rawlog(LT_CONN,
                  "[%d/%s/%s] Switching to Pueblo mode (via @sockset).",
                  d->descriptor, d->addr, d->ip);
        d->conn_flags |= CONN_HTML;
      }
      return T("Pueblo flag set.");
    } else {
      d->conn_flags &= ~CONN_HTML;
      return T("Pueblo flag cleared.");
    }
  }

  if (!strcasecmp(name, "TELNET")) {
    ival = isyes(val);
    if (ival) {
      d->conn_flags |= CONN_TELNET;
      return T("Telnet flag set.");
    } else {
      d->conn_flags &= ~CONN_TELNET;
      return T("Telnet flag cleared.");
    }
  }

  if (!strcasecmp(name, "WIDTH")) {
    if (!is_strict_integer(val)) {
      return T("Width expects a positive integer.");
    }
    ival = parse_integer(val);
    if (ival < 1) {
      return T("Width expects a positive integer.");
    }
    d->width = ival;
    return T("Width set.");
  }

  if (!strcasecmp(name, "HEIGHT")) {
    if (!is_strict_integer(val)) {
      return T("Height expects a positive integer.");
    }
    ival = parse_integer(val);
    if (ival < 1) {
      return T("Height expects a positive integer.");
    }
    d->height = ival;
    return T("Height set.");
  }

  if (!strcasecmp(name, "TERMINALTYPE")) {
    set_ttype(d, val);
    return T("Terminal Type set.");
  }

  if (strcasecmp(name, "COLORSTYLE") == 0 ||
      strcasecmp(name, "COLOURSTYLE") == 0) {
    if (strcasecmp(val, "auto") == 0) {
      d->conn_flags &= ~CONN_COLORSTYLE;
      snprintf(retval, sizeof retval, T("Colorstyle set to '%s'"), "auto");
      return retval;
    } else if (strcasecmp("plain", val) == 0 || strcasecmp("none", val) == 0) {
      d->conn_flags &= ~CONN_COLORSTYLE;
      d->conn_flags |= CONN_PLAIN;
      snprintf(retval, sizeof retval, T("Colorstyle set to '%s'"), "plain");
      return retval;
    } else if (strcasecmp("hilite", val) == 0 ||
               strcasecmp("highlight", val) == 0) {
      d->conn_flags &= ~CONN_COLORSTYLE;
      d->conn_flags |= CONN_ANSI;
      snprintf(retval, sizeof retval, T("Colorstyle set to '%s'"), "hilite");
      return retval;
    } else if (strcasecmp("16color", val) == 0) {
      d->conn_flags &= ~CONN_COLORSTYLE;
      d->conn_flags |= CONN_ANSICOLOR;
      snprintf(retval, sizeof retval, T("Colorstyle set to '%s'"), "16color");
      return retval;
    } else if (strcasecmp("xterm256", val) == 0 || strcmp(val, "256") == 0) {
      d->conn_flags &= ~CONN_COLORSTYLE;
      d->conn_flags |= CONN_XTERM256;
      snprintf(retval, sizeof retval, T("Colorstyle set to '%s'"), "xterm256");
      return retval;
    }
    snprintf(retval, sizeof retval,
             T("Unknown color style. Valid color styles: %s"),
             "'auto', 'plain', 'hilite', '16color', 'xterm256'.");
    return retval;
  }

  if (!strcasecmp(name, "PROMPT_NEWLINES")) {
    ival = isyes(val);
    if (ival) {
      d->conn_flags |= CONN_PROMPT_NEWLINES;
      return T("A newline will be sent after a prompt.");
    } else {
      d->conn_flags &= ~CONN_PROMPT_NEWLINES;
      return T("No newline will be sent after a prompt.");
    }
  }

  if (!strcasecmp(name, "STRIPACCENTS") || !strcasecmp(name, "NOACCENTS")) {
    ival = isyes(val);
    if (ival) {
      d->conn_flags |= CONN_STRIPACCENTS;
      return T("Accents will be stripped.");
    } else {
      d->conn_flags &= ~CONN_STRIPACCENTS;
      return T("Accents will not be stripped.");
    }
  }

  snprintf(retval, BUFFER_LEN, T("@sockset option '%s' is not a valid option."),
           name);
  return retval;
}

/** Given a player dbref, return the player's first connected descriptor.
 * \param player dbref of player.
 * \return pointer to player's first connected descriptor, or NULL.
 */
DESC *
player_desc(dbref player)
{
  DESC *d;

  for (d = descriptor_list; d; d = d->next) {
    if (d->connected && (d->player == player)) {
      return d;
    }
  }
  return (DESC *) NULL;
}

/** Pemit to a specified socket.
 * \param player the enactor.
 * \param pc string containing port number to send message to.
 * \param message message to send.
 * \param flags PEMIT_* flags
 */
void
do_pemit_port(dbref player, const char *pc, const char *message, int flags)
{
  DESC *d = NULL, *last = NULL;
  int port;
  int total = 0;
  char *next;

  if (!Hasprivs(player)) {
    notify(player, T("Permission denied."));
    return;
  }

  if (!message || !*message || !pc || !*pc)
    return;

  next = (char *) pc;
  do {
    if (flags & PEMIT_LIST)
      next = next_in_list(&pc);
    port = atoi(next);

    if (port <= 0) {
      notify_format(player, T("'%s' is not a port number."), next);
    } else {
      d = port_desc(port);
      if (!d) {
        notify(player, T("That port is not active."));
      } else {
        queue_string_eol(d, "%s", message);
        total++;
        last = d;
      }
    }

  } while ((flags & PEMIT_LIST) && pc && *pc);

  if (!total)
    return;

  if (!(flags & PEMIT_SILENT)) {
    if (total == 1) {
      notify_format(player, T("You pemit \"%s\" to %s."), message,
                    (last && last->connected ? AName(last->player, AN_SYS, NULL)
                                             : T("a connecting player")));
    } else {
      notify_format(player, T("You pemit \"%s\" to %d connections."), message,
                    total);
    }
  }
}

/** Page a specified socket.
 * \param executor the executor.
 * \param pc string containing port number to send message to.
 * \param message message to send.
 */
void
do_page_port(dbref executor, const char *pc, const char *message)
{
  int p, key;
  DESC *d;
  const char *gap;
  char tbuf[BUFFER_LEN], *tbp = tbuf;
  dbref target = NOTHING;

  if (!Hasprivs(executor)) {
    notify(executor, T("Permission denied."));
    return;
  }

  p = atoi(pc);

  if (p <= 0) {
    notify(executor, T("That's not a port number."));
    return;
  }

  if (!message || !*message) {
    notify(executor, T("What do you want to page with?"));
    return;
  }

  gap = " ";
  switch (*message) {
  case SEMI_POSE_TOKEN:
    gap = "";
  /* Fall through */
  case POSE_TOKEN:
    key = 1;
    break;
  default:
    key = 3;
    break;
  }

  d = port_desc(p);
  if (!d) {
    notify(executor, T("That port's not active."));
    return;
  }
  if (d->connected)
    target = d->player;
  switch (key) {
  case 1:
    safe_format(tbuf, &tbp, T("From afar, %s%s%s"), Name(executor), gap,
                message + 1);
    notify_format(executor, T("Long distance to %s: %s%s%s"),
                  target != NOTHING ? AName(target, AN_SAY, NULL)
                                    : T("a connecting player"),
                  AName(executor, AN_SAY, NULL), gap, message + 1);
    break;
  case 3:
    safe_format(tbuf, &tbp, T("%s pages: %s"), Name(executor), message);
    notify_format(executor, T("You paged %s with '%s'"),
                  target != NOTHING ? AName(target, AN_SAY, NULL)
                                    : T("a connecting player"),
                  message);
    break;
  }
  *tbp = '\0';
  if (target != NOTHING)
    page_return(executor, target, "Idle", "IDLE", NULL, NULL);
  if (Typeof(executor) != TYPE_PLAYER && Nospoof(target))
    queue_string_eol(d, "[#%d] %s", executor, tbuf);
  else
    queue_string_eol(d, "%s", tbuf);
}

/** Return an inactive descriptor, as long as there's more than
 * one descriptor connected. Used for boot/me.
 * \param player player to find an inactive descriptor for.
 * \return pointer to player's inactive descriptor, or NULL.
 */
DESC *
inactive_desc(dbref player)
{
  DESC *d, *in = NULL;
  time_t now;
  int numd = 0;
  now = mudtime;
  DESC_ITER_CONN (d) {
    if (d->player == player) {
      numd++;
      if (difftime(now, d->last_time) > 60.0)
        in = d;
    }
  }
  if (numd > 1)
    return in;
  else
    return (DESC *) NULL;
}

/** Given a port (a socket number), return the descriptor.
 * \param port port (socket file descriptor number).
 * \return pointer to descriptor associated with the port.
 */
DESC *
port_desc(int port)
{
  DESC *d;
  for (d = descriptor_list; (d); d = d->next) {
    if (d->descriptor == port) {
      return d;
    }
  }
  return (DESC *) NULL;
}

/** Given a port, find the matching player dbref.
 * \param port (socket file descriptor number).
 * \return dbref of connected player using that port, or NOTHING.
 */
dbref
find_player_by_desc(int port)
{
  DESC *d;
  for (d = descriptor_list; (d); d = d->next) {
    if (d->connected && (d->descriptor == port)) {
      return d->player;
    }
  }

  /* didn't find anything */
  return NOTHING;
}

#ifndef WIN32
/** Handler for SIGINT. Note that we've received it, and reinstall.
 * \param sig signal caught.
 */
void
signal_shutdown(int sig __attribute__((__unused__)))
{
  signal_shutdown_flag = 1;
  sigrecv_notify();
  reload_sig_handler(SIGINT, signal_shutdown);
}

/** Handler for HUP signal.
 * Do the minimal work here - set a global variable and reload the handler.
 * \param x unused.
 */
void
hup_handler(int x __attribute__((__unused__)))
{
  hup_triggered = 1;
  sigrecv_notify();
  reload_sig_handler(SIGHUP, hup_handler);
}

/** Handler for USR1 signal.
 * Do the minimal work here - set a global variable and reload the handler.
 * \param x unused.
 */
void
usr1_handler(int x __attribute__((__unused__)))
{
  usr1_triggered = 1;
  sigrecv_notify();
  reload_sig_handler(SIGUSR1, usr1_handler);
}

/** Handler for SIGUSR2. Note that we've received it, and reinstall
 * \param sig signal caught.
 */
void
signal_dump(int sig __attribute__((__unused__)))
{
  usr2_triggered = 1;
  sigrecv_notify();
  reload_sig_handler(SIGUSR2, signal_dump);
}
#endif

/** A general handler to puke and die.
 * \param sig signal caught.
 */
void
bailout(int sig)
{
  mush_panicf("BAILOUT: caught signal %d", sig);
}

#ifndef WIN32
/** Reap child processes, notably info_slaves and forking dumps,
 * when we receive a SIGCHLD signal. Don't fear this function. :)
 * \param sig signal caught.
 */
void
reaper(int sig __attribute__((__unused__)))
{
  pid_t pid;

  while ((pid = mush_wait(-1, &error_code, WNOHANG)) > 0) {
#ifdef INFO_SLAVE
    if (info_slave_pid > -1 && pid == info_slave_pid) {
      slave_error = info_slave_pid;
      info_slave_state = INFO_SLAVE_DOWN;
      info_slave_pid = -1;
      sigrecv_notify();
    } else
#endif
#ifdef SSL_SLAVE
      if (ssl_slave_pid > -1 && pid == ssl_slave_pid) {
      ssl_slave_error = ssl_slave_pid;
      ssl_slave_state = SSL_SLAVE_DOWN;
      ssl_slave_pid = -1;
      sigrecv_notify();
    } else
#endif
      if (forked_dump_pid > -1 && pid == forked_dump_pid) {
      dump_error = forked_dump_pid;
      dump_status = error_code;
      forked_dump_pid = -1;
      sigrecv_notify();
    }
  }
  reload_sig_handler(SIGCHLD, reaper);
}
#endif /* !(Mac or WIN32) */

/** Return the number of connected players,
 * possibly including Hidden connections */
static int
count_players(void)
{
  int count = 0;
  DESC *d;

  /* Count connected players */
  for (d = descriptor_list; d; d = d->next) {
    if (d->connected) {
      if (!GoodObject(d->player))
        continue;
      if (COUNT_ALL || !Hidden(d))
        count++;
    }
  }

  return count;
}

/** The INFO socket command */
static void
dump_info(DESC *call_by)
{

  queue_string_eol(call_by, "### Begin INFO %s", INFO_VERSION);

  queue_string_eol(call_by, "Name: %s", options.mud_name);
  queue_string_eol(call_by, "Address: %s", options.mud_url);
  queue_string_eol(call_by, "Uptime: %s",
                   show_time(globals.first_start_time, 0));
  queue_string_eol(call_by, "Connected: %d", count_players());
  queue_string_eol(call_by, "Size: %d", db_top);
  queue_string_eol(call_by, "Version: PennMUSH %sp%s", VERSION, PATCHLEVEL);
  queue_string_eol(call_by, "### End INFO");
}

/** The MSSP socket command / telnet option */
void
report_mssp(DESC *d, char *buff, char **bp)
{
  MSSP *opt;

  if (d) {
    queue_string_eol(d, "\r\nMSSP-REPLY-START");
    /* Required by current spec, as of 2010-08-15 */
    queue_string_eol(d, "%s\t%s", "NAME", options.mud_name);
    queue_string_eol(d, "%s\t%d", "PLAYERS", count_players());
    queue_string_eol(d, "%s\t%ld", "UPTIME", (long) globals.first_start_time);
    /* Not required, but we know anyway */
    queue_string_eol(d, "%s\t%d", "PORT", options.port);
    if (options.ssl_port) {
      queue_string_eol(d, "%s\t%d", "SSL", options.ssl_port);
    }
    queue_string_eol(d, "%s\t%d", "PUEBLO", options.support_pueblo);
    queue_string_eol(d, "%s\t%s %sp%s", "CODEBASE", "PennMUSH", VERSION,
                     PATCHLEVEL);
    queue_string_eol(d, "%s\t%s", "FAMILY", "TinyMUD");
    if (strlen(options.mud_url)) {
      queue_string_eol(d, "%s\t%s", "WEBSITE", options.mud_url);
    }
  } else {
    safe_format(buff, bp, "%c%s%c%s", MSSP_VAR, "NAME", MSSP_VAL,
                options.mud_name);
    safe_format(buff, bp, "%c%s%c%d", MSSP_VAR, "PLAYERS", MSSP_VAL,
                count_players());
    safe_format(buff, bp, "%c%s%c%ld", MSSP_VAR, "UPTIME", MSSP_VAL,
                (long) globals.first_start_time);

    safe_format(buff, bp, "%c%s%c%d", MSSP_VAR, "PORT", MSSP_VAL, options.port);
    if (options.ssl_port)
      safe_format(buff, bp, "%c%s%c%d", MSSP_VAR, "SSL", MSSP_VAL,
                  options.ssl_port);
    safe_format(buff, bp, "%c%s%c%d", MSSP_VAR, "PUEBLO", MSSP_VAL,
                options.support_pueblo);
    safe_format(buff, bp, "%c%s%cPennMUSH %sp%s", MSSP_VAR, "CODEBASE",
                MSSP_VAL, VERSION, PATCHLEVEL);
    safe_format(buff, bp, "%c%s%c%s", MSSP_VAR, "FAMILY", MSSP_VAL, "TinyMUD");
    if (strlen(options.mud_url))
      safe_format(buff, bp, "%c%s%c%s", MSSP_VAR, "WEBSITE", MSSP_VAL,
                  options.mud_url);
  }

  if (mssp) {
    opt = mssp;
    if (d) {
      while (opt) {
        queue_string_eol(d, "%s\t%s", opt->name, opt->value);
        opt = opt->next;
      }
      queue_string_eol(d, "MSSP-REPLY-END");
    } else {
      while (opt) {
        safe_format(buff, bp, "%c%s%c%s", MSSP_VAR, opt->name, MSSP_VAL,
                    opt->value);
        opt = opt->next;
      }
    }
  }
}

/** Determine if a new guest can connect at this point. If so, return
 * the dbref of the player they should connect to.
 * The algorithm looks like this:
 * \verbatim
 * 1. Count connected guests. If we have a fixed maximum number and we've
 *    reached it already, fail now.
 * 2. Otherwise, we either have no limit or we're limited to available
 *    unconnected guest players. So if the requested player isn't
 *    connected, succeed now.
 * 3. Search the db for unconnected guest players. If we find any,
 *    succeed immediately.
 * 4. If none were found, succeed only if we have no limit.
 * \endverbatim
 * \param player dbref of guest that connection was attempted to.
 */
dbref
guest_to_connect(dbref player)
{
  DESC *d;
  int desc_count = 0;
  dbref i;

  DESC_ITER_CONN (d) {
    if (!GoodObject(d->player))
      continue;
    if (Guest(d->player))
      desc_count++;
  }
  if ((MAX_GUESTS > 0) && (desc_count >= MAX_GUESTS))
    return NOTHING; /* Limit already reached */

  if (!Connected(player))
    return player; /* Connecting to a free guest */

  /* The requested guest isn't free. Find an available guest in the db */
  for (i = 0; i < db_top; i++) {
    if (IsPlayer(i) && !Hasprivs(i) && Guest(i) && !Connected(i))
      return i;
  }

  /* Oops, all guests are in use. Either fail now or succeed now with
   * a log message
   */
  if (MAX_GUESTS < 0)
    return NOTHING;

  do_rawlog(LT_CONN, "Multiple connection to Guest #%d", player);
  return player;
}

/** The connect-screen WHO command */
static void
dump_users(DESC *call_by, char *match)
{
  DESC *d;
  int count = 0;
  char tbuf[BUFFER_LEN];
  char nbuff[BUFFER_LEN];
  char *np;
  int nlen;

  while (*match && *match == ' ')
    match++;

  if (SUPPORT_PUEBLO && (call_by->conn_flags & CONN_HTML)) {
    queue_newwrite(call_by, "<PRE>", 5);
  }

  snprintf(tbuf, BUFFER_LEN, "%-16s %10s %6s  %s", T("Player Name"),
           T("On For"), T("Idle"), get_poll());
  queue_string_eol(call_by, "%s", tbuf);

  for (d = descriptor_list; d; d = d->next) {
    if (!d->connected || !GoodObject(d->player))
      continue;
    if (COUNT_ALL || !Hidden(d))
      count++;
    if (Hidden(d) || (match && !(string_prefix(Name(d->player), match))))
      continue;

    np = nbuff;
    safe_str(AName(d->player, AN_WHO, NULL), nbuff, &np);
    nlen = strlen(Name(d->player));
    if (nlen < 16)
      safe_fill(' ', 16 - nlen, nbuff, &np);
    *np = '\0';
    snprintf(tbuf, sizeof tbuf, "%16.16s %10.10s %6.6s%c %s", nbuff,
             onfor_time_fmt(d->connected_at, 10),
             idle_time_fmt(d->last_time, 4), (Dark(d->player) ? 'D' : ' '),
             get_doing(d->player, NOTHING, NOTHING, NULL, 0));
    queue_string_eol(call_by, "%s", tbuf);
  }
  switch (count) {
  case 0:
    mush_strncpy(tbuf, T("There are no players connected."), BUFFER_LEN);
    break;
  case 1:
    mush_strncpy(tbuf, T("There is 1 player connected."), BUFFER_LEN);
    break;
  default:
    snprintf(tbuf, BUFFER_LEN, T("There are %d players connected."), count);
    break;
  }
  queue_string_eol(call_by, "%s", tbuf);
  if (SUPPORT_PUEBLO && (call_by->conn_flags & CONN_HTML))
    queue_newwrite(call_by, "</PRE>", 6);
}

/** Filters descriptors based on the name for 'WHO name'.
 * \verbatim
 * Check to see if we should display a line in WHO, DOING or SESSION
 * for the given descriptor. Checks are, in order:
 *  * If 'name' is empty, we show it.
 *  * If d is not a connected player, we don't.
 *  * If wild is false, show only if "name" is a prefix of the player's name
 *  * If wild is true, show if the player's name, or one of his aliases,
 *    matches the wildcard pattern "name".
 * \endverbatim
 * \param d descriptor to check
 * \param name name to match against
 * \param wild is name a wildcard pattern?
 * \retval 1 name matches, or no name filtering
 * \retval 0 name does not match
 */
static bool
who_check_name(DESC *d, char *name, bool wild)
{
  ATTR *a;
  char *aval, *all_aliases, *one_alias;

  if (!name || !*name)
    return 1;

  if (!d->connected || !GoodObject(d->player))
    return 0;

  if (!wild)
    return string_prefix(Name(d->player), name);

  if (quick_wild(name, Name(d->player)))
    return 1;

  a = atr_get(d->player, "ALIAS");
  if (!a)
    return 0;

  aval = safe_atr_value(a, "atrval.who-alias");
  all_aliases = trim_space_sep(aval, ';');
  while ((one_alias = split_token(&all_aliases, ';')) != NULL) {
    if (quick_wild(name, one_alias)) {
      free(aval);
      return 1;
    }
  }
  mush_free(aval, "atrval.who-alias");
  return 0;
}

/** The DOING command */
void
do_who_mortal(dbref player, char *name)
{
  DESC *d;
  int count = 0;
  int privs = Priv_Who(player);
  bool wild = 0;
  char nbuff[BUFFER_LEN];
  char *np;
  int nlen;
  PUEBLOBUFF;

  if (SUPPORT_PUEBLO) {
    PUSE;
    tag("PRE");
    PEND;
    notify_noenter(player, pbuff);
  }

  if (name && *name && wildcard_count(name, 0) == -1)
    wild = 1;

  notify_format(player, "%-16s %10s %6s  %s", T("Player Name"), T("On For"),
                T("Idle"), get_poll());
  for (d = descriptor_list; d; d = d->next) {
    if (!d->connected)
      continue;
    if (COUNT_ALL || (!Hidden(d) || privs))
      count++;
    if (!who_check_name(d, name, wild))
      continue;
    if (Hidden(d) && !privs)
      continue;
    np = nbuff;
    safe_str(AName(d->player, AN_WHO, NULL), nbuff, &np);
    nlen = strlen(Name(d->player));
    if (nlen < 16)
      safe_fill(' ', 16 - nlen, nbuff, &np);
    *np = '\0';
    notify_format(player, "%s %10s   %4s%c %s", nbuff,
                  onfor_time_fmt(d->connected_at, 10),
                  idle_time_fmt(d->last_time, 4),
                  (Dark(d->player) ? 'D' : (Hidden(d) ? 'H' : ' ')),
                  get_doing(d->player, player, player, NULL, 0));
  }
  switch (count) {
  case 0:
    notify(player, T("There are no players connected."));
    break;
  case 1:
    notify(player, T("There is one player connected."));
    break;
  default:
    notify_format(player, T("There are %d players connected."), count);
    break;
  }

  if (SUPPORT_PUEBLO) {
    PUSE;
    tag_cancel("PRE");
    PEND;
    notify_noenter(player, pbuff);
  }
}

/** The admin WHO command */
void
do_who_admin(dbref player, char *name)
{
  DESC *d;
  int count = 0;
  char tbuf[BUFFER_LEN];
  char addr[28];
  bool wild = 0;
  char *tp;
  int nlen;
  PUEBLOBUFF;

  if (SUPPORT_PUEBLO) {
    PUSE;
    tag("PRE");
    PEND;
    notify_noenter(player, pbuff);
  }

  if (name && *name && wildcard_count(name, 0) == -1)
    wild = 1;

  notify_format(player, "%-16s %6s %9s %5s %5s %-4s %-s", T("Player Name"),
                T("Loc #"), T("On For"), T("Idle"), T("Cmds"), T("Des"),
                T("Host"));
  for (d = descriptor_list; d; d = d->next) {
    if (d->connected)
      count++;
    if (!who_check_name(d, name, wild))
      continue;
    if (d->connected) {
      char conntype[3] = {'\0'};
      int cti = 0;
      tp = tbuf;
      safe_str(AName(d->player, AN_WHO, NULL), tbuf, &tp);
      nlen = strlen(Name(d->player));
      if (nlen < 16)
        safe_fill(' ', 16 - nlen, tbuf, &tp);

      if (is_ssl_desc(d))
        conntype[cti++] = 'S';
      else if (!is_remote_desc(d))
        conntype[cti++] = 'L';
      if (is_ws_desc(d))
        conntype[cti] = 'W';
      safe_format(
        tbuf, &tp, " %6s %9s %5s  %4d %3d%s ",
        unparse_dbref(Location(d->player)), onfor_time_fmt(d->connected_at, 9),
        idle_time_fmt(d->last_time, 5), d->cmds, d->descriptor, conntype);
      mush_strncpy(addr, d->addr, sizeof addr);
      if (Dark(d->player)) {
        addr[20] = '\0';
        strcat(addr, " (Dark)");
      } else if (Hidden(d)) {
        addr[20] = '\0';
        strcat(addr, " (Hide)");
      } else {
        addr[27] = '\0';
      }
      safe_str(addr, tbuf, &tp);
      *tp = '\0';
    } else if (d->conn_flags & CONN_HTTP_REQUEST) {
      snprintf(tbuf, sizeof tbuf, "%-16s %6s %9s %5s %4d %3d%c %s",
               T("HTTP Request"), "#-1", onfor_time_fmt(d->connected_at, 9),
               idle_time_fmt(d->last_time, 5), d->cmds, d->descriptor,
               is_ssl_desc(d) ? 'S' : ' ', d->addr);
      tbuf[78] = '\0';
    } else {
      snprintf(tbuf, sizeof tbuf, "%-16s %6s %9s %5s %4d %3d%c %s",
               T("Connecting..."), "#-1", onfor_time_fmt(d->connected_at, 9),
               idle_time_fmt(d->last_time, 5), d->cmds, d->descriptor,
               is_ssl_desc(d) ? 'S' : ' ', d->addr);
      tbuf[78] = '\0';
    }
    notify(player, tbuf);
  }

  switch (count) {
  case 0:
    notify(player, T("There are no players connected."));
    break;
  case 1:
    notify(player, T("There is one player connected."));
    break;
  default:
    notify_format(player, T("There are %d players connected."), count);
    break;
  }

  if (SUPPORT_PUEBLO) {
    PUSE;
    tag_cancel("PRE");
    PEND;
    notify_noenter(player, pbuff);
  }
}

/** The SESSION command */
void
do_who_session(dbref player, char *name)
{
  DESC *d;
  int count = 0;
  bool wild = 0;
  char nbuff[BUFFER_LEN];
  char *np;
  int nlen;
  PUEBLOBUFF;

  if (SUPPORT_PUEBLO) {
    PUSE;
    tag("PRE");
    PEND;
    notify_noenter(player, pbuff);
  }

  if (name && *name && wildcard_count(name, 0) == -1)
    wild = 1;

  notify_format(player, "%-16s %6s %9s %5s %5s %4s %7s %7s %7s",
                T("Player Name"), T("Loc #"), T("On For"), T("Idle"), T("Cmds"),
                T("Des"), T("Sent"), T("Recv"), T("Pend"));

  for (d = descriptor_list; d; d = d->next) {
    if (d->connected)
      count++;
    if (!who_check_name(d, name, wild))
      continue;
    if (d->connected) {
      np = nbuff;
      safe_str(AName(d->player, AN_WHO, NULL), nbuff, &np);
      nlen = strlen(Name(d->player));
      if (nlen < 16)
        safe_fill(' ', 16 - nlen, nbuff, &np);
      *np = '\0';

      notify_format(player, "%s %6s %9s %5s %5d %3d%c %7lu %7lu %7d", nbuff,
                    unparse_dbref(Location(d->player)),
                    onfor_time_fmt(d->connected_at, 9),
                    idle_time_fmt(d->last_time, 5), d->cmds, d->descriptor,
                    is_ssl_desc(d) ? 'S' : ' ', d->input_chars, d->output_chars,
                    d->output_size);
    } else {
      notify_format(player, "%-16s %6s %9s %5s %5d %3d%c %7lu %7lu %7d",
                    T("Connecting..."), "#-1",
                    onfor_time_fmt(d->connected_at, 9),
                    idle_time_fmt(d->last_time, 5), d->cmds, d->descriptor,
                    is_ssl_desc(d) ? 'S' : ' ', d->input_chars, d->output_chars,
                    d->output_size);
    }
  }

  switch (count) {
  case 0:
    notify(player, T("There are no players connected."));
    break;
  case 1:
    notify(player, T("There is one player connected."));
    break;
  default:
    notify_format(player, T("There are %d players connected."), count);
    break;
  }

  if (SUPPORT_PUEBLO) {
    PUSE;
    tag_cancel("PRE");
    PEND;
    notify_noenter(player, pbuff);
  }
}

/** Format the time the player has been on for for WHO/DOING/ETC.
 *
 * \param at the time connected at.
 * \param len the length of the field to fill.
 * \return a static buffer to a string with the formatted elapsed time.
 */
static char *
onfor_time_fmt(time_t at, int len)
{
  static char buf[64];
  int secs = difftime(mudtime, at);
  return etime_fmt(buf, secs, len);
}

/** Format idle time for WHO/DOING
 *
 * \param last the time the player was last active.
 * \parm len the length of the field to fill.
 * \return a static buffer to a string with the formatted elapsed time.
 */
static char *
idle_time_fmt(time_t last, int len)
{
  static char buf[64];
  int secs = difftime(mudtime, last);
  return etime_fmt(buf, secs, len);
}

/* connection messages
 * isnew: newly created or not?
 * num: how many times connected?
 */
static void
announce_connect(DESC *d, int isnew, int num)
{
  dbref loc;
  char tbuf1[BUFFER_LEN];
  char *message;
  PE_REGS *pe_regs;
  dbref zone;
  dbref obj;

  dbref player = d->player;

  set_flag_internal(player, "CONNECTED");

  if (isnew) {
    /* A brand new player created. */
    snprintf(tbuf1, BUFFER_LEN, T("%s created."),
             AName(player, AN_ANNOUNCE, NULL));
    flag_broadcast(0, "HEAR_CONNECT", "%s %s", T("GAME:"), tbuf1);
    if (Suspect(player))
      flag_broadcast("WIZARD", 0, T("GAME: Suspect %s created."),
                     AName(player, AN_ANNOUNCE, NULL));
  }

  /* Redundant, but better for translators */
  if (Hidden(d)) {
    message =
      (num > 1) ? T("has HIDDEN-reconnected.") : T("has HIDDEN-connected.");
  } else {
    message = (num > 1) ? T("has reconnected.") : T("has connected.");
  }
  snprintf(tbuf1, BUFFER_LEN, "%s %s", AName(player, AN_ANNOUNCE, NULL),
           message);

  /* send out messages */
  if (Suspect(player))
    flag_broadcast("WIZARD", 0, T("GAME: Suspect %s"), tbuf1);

  if (Dark(player)) {
    flag_broadcast("ROYALTY WIZARD", "HEAR_CONNECT", "%s %s", T("GAME:"),
                   tbuf1);
  } else
    flag_broadcast(0, "HEAR_CONNECT", "%s %s", T("GAME:"), tbuf1);

  if (ANNOUNCE_CONNECTS)
    chat_player_announce(d, message, 0);

  loc = Location(player);
  if (!GoodObject(loc)) {
    notify(player, T("You are nowhere!"));
    return;
  }

  if (*cf_motd_msg) {
    raw_notify(player, cf_motd_msg);
  }
  raw_notify(player, " ");
  if (Hasprivs(player) && *cf_wizmotd_msg) {
    if (*cf_motd_msg)
      raw_notify(player, asterisk_line);
    raw_notify(player, cf_wizmotd_msg);
  }

  if (ANNOUNCE_CONNECTS)
    notify_except(player, player, player, tbuf1, 0);

  /* added to allow player's inventory to hear a player connect */
  if (ANNOUNCE_CONNECTS)
    if (!Dark(player))
      notify_except(player, loc, player, tbuf1, NA_INTER_PRESENCE);

  queue_event(player, "PLAYER`CONNECT", "%s,%d,%d", unparse_objid(player), num,
              d->descriptor);
  /* And then load it up, as follows:
   * %0 (unused, reserved for "reason for disconnect")
   * %1 (number of connections after connect)
   */
  pe_regs = pe_regs_create(PE_REGS_ARG, "announce_connect");
  pe_regs_setenv(pe_regs, 1, unparse_integer(num));

  /* do the person's personal connect action */
  (void) queue_attribute_base(player, "ACONNECT", player, 0, pe_regs, 0);
  if (ROOM_CONNECTS) {
    /* Do the room the player connected into */
    if (IsRoom(loc) || IsThing(loc)) {
      (void) queue_attribute_base(loc, "ACONNECT", player, 0, pe_regs, 0);
    }
  }
  /* do the zone of the player's location's possible aconnect */
  if ((zone = Zone(loc)) != NOTHING) {
    switch (Typeof(zone)) {
    case TYPE_THING:
      (void) queue_attribute_base(zone, "ACONNECT", player, 0, pe_regs, 0);
      break;
    case TYPE_ROOM:
      /* check every object in the room for a connect action */
      DOLIST (obj, Contents(zone)) {
        (void) queue_attribute_base(obj, "ACONNECT", player, 0, pe_regs, 0);
      }
      break;
    default:
      do_rawlog(LT_ERR, "Invalid zone #%d for %s(#%d) has bad type %d", zone,
                Name(player), player, Typeof(zone));
    }
  }
  /* now try the master room */
  DOLIST (obj, Contents(MASTER_ROOM)) {
    (void) queue_attribute_base(obj, "ACONNECT", player, 0, pe_regs, 0);
  }
  pe_regs_free(pe_regs);
}

static void
announce_disconnect(DESC *saved, const char *reason, dbref executor)
{
  dbref loc;
  int numleft = 0;
  DESC *d;
  char tbuf1[BUFFER_LEN];
  char *message;
  dbref zone, obj;
  dbref player;
  ATTR *a;
  PE_REGS *pe_regs;

  player = saved->player;
  loc = Location(player);
  if (!GoodObject(loc))
    return;

  DESC_ITER_CONN (d) {
    /* Don't count this current DESC, we want number of _other_ DESCs for this
     * player.
     * In a QUIT, saved won't be in descriptor_list, but in a LOGOUT, it will
     * be. */
    if (d == saved)
      continue;
    if (d->player == player)
      numleft += 1;
  }

  /* And then load it up, as follows:
   * %0 (unused, reserved for "reason for disconnect")
   * %1 (number of connections remaining after disconnect)
   * %2 (bytes received)
   * %3 (bytes sent)
   * %4 (commands queued)
   * %5 (hidden)
   */
  pe_regs = pe_regs_create(PE_REGS_ARG, "announce_disconnect");
  pe_regs_setenv(pe_regs, 1, unparse_integer(numleft));
  pe_regs_setenv(pe_regs, 2, unparse_integer(saved->input_chars));
  pe_regs_setenv(pe_regs, 3, unparse_integer(saved->output_chars));
  pe_regs_setenv(pe_regs, 4, unparse_integer(saved->cmds));
  pe_regs_setenv(pe_regs, 5, unparse_integer(Hidden(saved)));

  /* Eww. Unwieldy.
   * (objid, count, hidden, cause, ip, descriptor, conn,
   * idle, recv/sent/commands)  */
  queue_event(executor, "PLAYER`DISCONNECT",
              "%s,%d,%d,%s,%s,%d,%d,%d,%lu/%lu/%d", unparse_objid(player),
              numleft, Hidden(saved), reason, saved->ip, saved->descriptor,
              (int) difftime(mudtime, saved->connected_at),
              (int) difftime(mudtime, saved->last_time), saved->input_chars,
              saved->output_chars, saved->cmds);

  (void) queue_attribute_base(player, "ADISCONNECT", player, 0, pe_regs, 0);
  if (ROOM_CONNECTS)
    if (IsRoom(loc) || IsThing(loc)) {
      a = queue_attribute_getatr(loc, "ADISCONNECT", 0);
      if (a) {
        if (!Priv_Who(loc) && !Can_Examine(loc, player))
          pe_regs_setenv_nocopy(pe_regs, 1, "");
        (void) queue_attribute_useatr(loc, a, player, pe_regs, 0, NULL, NULL);
        if (!Priv_Who(loc) && !Can_Examine(loc, player))
          pe_regs_setenv(pe_regs, 1, unparse_integer(numleft));
      }
    }
  /* do the zone of the player's location's possible adisconnect */
  if ((zone = Zone(loc)) != NOTHING) {
    switch (Typeof(zone)) {
    case TYPE_THING:
      a = queue_attribute_getatr(zone, "ADISCONNECT", 0);
      if (a) {
        if (!Priv_Who(zone) && !Can_Examine(zone, player))
          pe_regs_setenv_nocopy(pe_regs, 1, "");
        (void) queue_attribute_useatr(zone, a, player, pe_regs, 0, NULL, NULL);
        if (!Priv_Who(zone) && !Can_Examine(zone, player))
          pe_regs_setenv(pe_regs, 1, unparse_integer(numleft));
      }
      break;
    case TYPE_ROOM:
      /* check every object in the room for a connect action */
      DOLIST (obj, Contents(zone)) {
        a = queue_attribute_getatr(obj, "ADISCONNECT", 0);
        if (a) {
          if (!Priv_Who(obj) && !Can_Examine(obj, player))
            pe_regs_setenv_nocopy(pe_regs, 1, "");
          (void) queue_attribute_useatr(obj, a, player, pe_regs, 0, NULL, NULL);
          if (!Priv_Who(obj) && !Can_Examine(obj, player))
            pe_regs_setenv(pe_regs, 1, unparse_integer(numleft));
        }
      }
      break;
    default:
      do_rawlog(LT_ERR, "Invalid zone #%d for %s(#%d) has bad type %d", zone,
                Name(player), player, Typeof(zone));
    }
  }
  /* now try the master room */
  DOLIST (obj, Contents(MASTER_ROOM)) {
    a = queue_attribute_getatr(obj, "ADISCONNECT", 0);
    if (a) {
      if (!Priv_Who(obj) && !Can_Examine(obj, player))
        pe_regs_setenv_nocopy(pe_regs, 1, "");
      (void) queue_attribute_useatr(obj, a, player, pe_regs, 0, NULL, NULL);
      if (!Priv_Who(obj) && !Can_Examine(obj, player))
        pe_regs_setenv(pe_regs, 1, unparse_integer(numleft));
    }
  }

  pe_regs_free(pe_regs);

  /* Redundant, but better for translators */
  if (Hidden(saved)) {
    message = (numleft) ? T("has partially HIDDEN-disconnected.")
                        : T("has HIDDEN-disconnected.");
  } else {
    message =
      (numleft) ? T("has partially disconnected.") : T("has disconnected.");
  }
  snprintf(tbuf1, BUFFER_LEN, "%s %s", AName(player, AN_ANNOUNCE, NULL),
           message);

  if (ANNOUNCE_CONNECTS) {
    if (!Dark(player))
      notify_except(player, loc, player, tbuf1, NA_INTER_PRESENCE);
    /* notify contents */
    notify_except(player, player, player, tbuf1, 0);
    /* notify channels */
    chat_player_announce(saved, message, !numleft);
  }

  /* Monitor broadcasts */
  if (Suspect(player))
    flag_broadcast("WIZARD", 0, T("GAME: Suspect %s"), tbuf1);
  if (Dark(player)) {
    flag_broadcast("ROYALTY WIZARD", "HEAR_CONNECT", "%s %s", T("GAME:"),
                   tbuf1);
  } else
    flag_broadcast(0, "HEAR_CONNECT", "%s %s", T("GAME:"), tbuf1);

  if (!numleft) {
    clear_flag_internal(player, "CONNECTED");
    (void) atr_add(player, "LASTLOGOUT", show_time(mudtime, 0), GOD, 0);
  }

  /* local_disconnect expects num to include logged out sock, for backwards
   * compat. */
  local_disconnect(player, numleft + 1);
}

/** Set an motd message.
 * \verbatim
 * This implements @motd.
 * \endverbatim
 * \param player the enactor.
 * \param key type of MOTD to set.
 * \param message text to set the motd to.
 */
void
do_motd(dbref player, int key, const char *message)
{
  const char *what;

  if ((key & MOTD_ACTION) == MOTD_LIST ||
      ((key & MOTD_ACTION) == MOTD_SET && (!message || !*message))) {
    notify_format(player, T("MOTD: %s"), cf_motd_msg);
    if (Hasprivs(player) && (key & MOTD_ACTION) == MOTD_LIST) {
      notify_format(player, T("Wiz MOTD: %s"), cf_wizmotd_msg);
      notify_format(player, T("Down MOTD: %s"), cf_downmotd_msg);
      notify_format(player, T("Full MOTD: %s"), cf_fullmotd_msg);
    }
    return;
  }

  if (!(((key & MOTD_TYPE) == MOTD_MOTD) ? Can_Announce(player)
                                         : Hasprivs(player))) {
    notify(player, T("You may get 15 minutes of fame and glory in life, but "
                     "not right now."));
    return;
  }

  if (key & MOTD_CLEAR) {
    what = T("cleared");
    message = "";
  } else
    what = T("set");

  switch (key & MOTD_TYPE) {
  case MOTD_MOTD:
    mush_strncpy(cf_motd_msg, message, BUFFER_LEN);
    notify_format(player, T("Motd %s."), what);
    break;
  case MOTD_WIZ:
    mush_strncpy(cf_wizmotd_msg, message, BUFFER_LEN);
    notify_format(player, T("Wizard motd %s."), what);
    break;
  case MOTD_DOWN:
    mush_strncpy(cf_downmotd_msg, message, BUFFER_LEN);
    notify_format(player, T("Down motd %s."), what);
    break;
  case MOTD_FULL:
    mush_strncpy(cf_fullmotd_msg, message, BUFFER_LEN);
    notify_format(player, T("Full motd %s."), what);
    break;
  default:
    notify(player, T("Set what?"));
  }
}

/** Return a player's \@doing.
 * \param player the dbref of the player whose \@doing we want
 * \param caller
 * \param enactor the enactor
 * \param full Return the full doing, or limit to DOING_LEN chars for WHO?
 * \return a pointer to a STATIC buffer with the doing in.
 */
static char *
get_doing(dbref player, dbref caller, dbref enactor, NEW_PE_INFO *pe_info,
          bool full)
{
  static char doing[BUFFER_LEN];
  char *dp = doing;
  ufun_attrib ufun;

  doing[0] = '\0';

  if (!GoodObject(player) || !IsPlayer(player)) {
    /* No such player; probably used on an unconnected descriptor */
    return "";
  }

  if (!fetch_ufun_attrib("DOING", player, &ufun,
                         UFUN_LOCALIZE | UFUN_REQUIRE_ATTR | UFUN_IGNORE_PERMS))
    return ""; /* No DOING attribute */

  call_ufun(&ufun, doing, caller, enactor, pe_info, NULL);
  if (!doing[0])
    return "";

  if (!full) {
    /* Truncate to display on WHO */
    if (has_markup(doing)) {
      /* Contains ANSI */
      ansi_string *as;
      dp = doing;
      as = parse_ansi_string(doing);
      safe_ansi_string(as, 0, DOING_LEN - 1, doing, &dp);
      *dp = '\0';
      free_ansi_string(as);
    } else {
      /* Nice and easy */
      doing[DOING_LEN - 1] = '\0';
    }
  }

  /* Smash any undesirable characters */
  dp = doing;
  WALK_ANSI_STRING (dp) {
    if (!char_isprint((int) *dp) || (*dp == '\n') || (*dp == '\r') ||
        (*dp == '\t') || (*dp == BEEP_CHAR)) {
      *dp = ' ';
    }
    dp++;
  }

  return doing;
}

/** Get the current poll message.
 * If there isn't one currently set, sets it to "Doing" first.
 */
char *
get_poll(void)
{
  if (!*poll_msg)
    set_poll(NULL);
  return poll_msg;
}

/** Set the poll message.
 * \param message The new poll, or NULL to use the default, "Doing")
 * \return number of characters trimmed from new poll
 */
int
set_poll(const char *message)
{
  int i = 0;
  size_t len = 0;

  if (message && *message) {
    mush_strncpy(poll_msg, remove_markup(message, &len), sizeof poll_msg);
    len--; /* Length includes trailing null */
  } else {
    mush_strncpy(poll_msg, T("Doing"), sizeof poll_msg);
  }
  for (i = 0; i < DOING_LEN; i++) {
    if (poll_msg[i] == '\0') {
      break;
    } else if ((poll_msg[i] == '\r') || (poll_msg[i] == '\n') ||
               (poll_msg[i] == '\t') || (poll_msg[i] == BEEP_CHAR)) {
      poll_msg[i] = ' ';
    }
  }

  if ((int) len >= DOING_LEN)
    return ((int) len - DOING_LEN);
  else
    return 0;
}

/** Set a poll message (which replaces "Doing" in the DOING output).
 * \verbatim
 * This implements @poll.
 * \endverbatim
 * \param player the enactor.
 * \param message the message to set.
 * \param clear true if the poll should be reset to the default 'Doing'
 */
void
do_poll(dbref player, const char *message, int clear)
{
  int i;

  if ((!message || !*message) && !clear) {
    /* Just display the poll. */
    notify_format(player, T("The current poll is: %s"), get_poll());
    return;
  }

  if (!Change_Poll(player)) {
    notify(player, T("Who do you think you are, Gallup?"));
    return;
  }

  if (clear) {
    set_poll(NULL);
    notify(player, T("Poll reset."));
    return;
  }

  i = set_poll(message);

  if (i) {
    notify_format(player, T("Poll set to '%s'. %d characters lost."), poll_msg,
                  i);
  } else
    notify_format(player, T("Poll set to: %s"), poll_msg);
  do_log(LT_WIZ, player, NOTHING, "Poll Set to '%s'.", poll_msg);
}

/** Match the partial name of a connected player.
 * \param match string to match.
 * \return dbref of a unique connected player whose name partial-matches,
 * AMBIGUOUS, or NOTHING.
 */
dbref
short_page(const char *match)
{
  DESC *d;
  dbref who1 = NOTHING;
  int count = 0;

  if (!(match && *match))
    return NOTHING;

  for (d = descriptor_list; d; d = d->next) {
    if (d->connected) {
      if (!string_prefix(Name(d->player), match))
        continue;
      if (!strcasecmp(Name(d->player), match)) {
        count = 1;
        who1 = d->player;
        break;
      }
      if (who1 == NOTHING || d->player != who1) {
        who1 = d->player;
        count++;
      }
    }
  }

  if (count > 1)
    return AMBIGUOUS;
  else if (count == 0)
    return NOTHING;

  return who1;
}

/** Match the partial name of a connected player the enactor can see.
 * \param player the enactor
 * \param match string to match.
 * \return dbref of a unique connected player whose name partial-matches,
 * AMBIGUOUS, or NOTHING.
 */
dbref
visible_short_page(dbref player, const char *match)
{
  dbref target;
  target = short_page(match);
  if (Priv_Who(player) || !GoodObject(target))
    return target;
  if (Dark(target) || (hidden(target) && !nearby(player, target)))
    return NOTHING;
  return target;
}

/* LWHO() function - really belongs elsewhere but needs stuff declared here */

FUNCTION(fun_xwho)
{
  DESC *d;
  int nwho;
  int first;
  int start, count;
  int powered = (*(called_as + 1) != 'M') && Priv_Who(executor);
  int objid = (strchr(called_as, 'D') != NULL);
  int firstnum = 0;
  dbref victim;

  if (nargs > 2) {
    firstnum = 1;
    if ((victim = noisy_match_result(executor, args[0], NOTYPE,
                                     MAT_EVERYTHING)) == NOTHING) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
    if (!powered && victim != executor) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (!Priv_Who(victim))
      powered = 0;
  }

  if (!is_strict_integer(args[firstnum]) ||
      !is_strict_integer(args[firstnum + 1])) {
    safe_str(T(e_int), buff, bp);
    return;
  }
  start = parse_integer(args[firstnum]);
  count = parse_integer(args[firstnum + 1]);

  if (start < 1 || count < 1) {
    safe_str(T(e_argrange), buff, bp);
    return;
  }

  nwho = 0;
  first = 1;

  DESC_ITER_CONN (d) {
    if (!Hidden(d) || (powered)) {
      nwho += 1;
      if (nwho >= start && nwho < (start + count)) {
        if (first)
          first = 0;
        else
          safe_chr(' ', buff, bp);
        safe_dbref(d->player, buff, bp);
        if (objid) {
          safe_chr(':', buff, bp);
          safe_integer(CreTime(d->player), buff, bp);
        }
      }
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_nwho)
{
  DESC *d;
  dbref victim;
  int count = 0;
  int powered = ((*(called_as + 1) != 'M') && Priv_Who(executor));

  if (nargs && args[0] && *args[0]) {
    /* An argument was given. Find the victim and choose the lowest
     * perms possible */
    if ((victim = noisy_match_result(executor, args[0], NOTYPE,
                                     MAT_EVERYTHING)) == NOTHING) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
    if (!powered && victim != executor) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (!Priv_Who(victim))
      powered = 0;
  }

  DESC_ITER_CONN (d) {
    if (!Hidden(d) || powered) {
      count++;
    }
  }
  safe_integer(count, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_lwho)
{
  DESC *d;
  int first = 1;
  dbref victim;
  int powered = ((*called_as == 'L') && Priv_Who(executor));
  int objid = (strchr(called_as, 'D') != NULL);
  int online = 1;
  int offline = 0;

  if (nargs && args[0] && *args[0]) {
    /* An argument was given. Find the victim and choose the lowest
     * perms possible */
    if ((victim = noisy_match_result(executor, args[0], NOTYPE,
                                     MAT_EVERYTHING)) == NOTHING) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
    if (!powered && victim != executor) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
    if (!Priv_Who(victim))
      powered = 0;
  }

  if (nargs > 1 && args[1] && *args[1]) {
    if (strcasecmp("all", args[1]) == 0) {
      offline = online = 1;
    } else if (strcasecmp("online", args[1]) == 0) {
      online = 1;
      offline = 0;
    } else if (strcasecmp("offline", args[1]) == 0) {
      online = 0;
      offline = 1;
    } else {
      safe_str(T("#-1 INVALID SECOND ARGUMENT"), buff, bp);
      return;
    }
    if (offline && !powered) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
  }

  DESC_ITER (d) {
    if ((d->connected && !online) || (!d->connected && !offline))
      continue;
    if (d->conn_flags & CONN_HTTP_REQUEST)
      continue;
    if (!powered && (d->connected && Hidden(d)))
      continue;
    if (first)
      first = 0;
    else
      safe_chr(' ', buff, bp);
    if (d->connected) {
      safe_dbref(d->player, buff, bp);
      if (objid) {
        safe_chr(':', buff, bp);
        safe_integer(CreTime(d->player), buff, bp);
      }
    } else
      safe_dbref(-1, buff, bp);
  }
}

#ifdef WIN32
#pragma warning(disable : 4761) /* Disable bogus conversion warning */
#endif
/* ARGSUSED */
FUNCTION(fun_hidden)
{
  if (!See_All(executor)) {
    notify(executor, T("Permission denied."));
    safe_str("#-1", buff, bp);
    return;
  }
  if (is_strict_integer(args[0])) {
    DESC *d = lookup_desc(executor, args[0]);
    if (!d) {
      notify(executor, T("Couldn't find that descriptor."));
      safe_str("#-1", buff, bp);
      return;
    }
    safe_boolean(Hidden(d), buff, bp);
  } else {
    dbref it = match_thing(executor, args[0]);
    if ((it == NOTHING) || (!IsPlayer(it))) {
      notify(executor, T("Couldn't find that player."));
      safe_str("#-1", buff, bp);
      return;
    }
    safe_boolean(hidden(it), buff, bp);
  }
}

#ifdef WIN32
#pragma warning(default : 4761) /* Re-enable conversion warning */
#endif

/** Look up a DESC by character name or file descriptor.
 * \param executor the dbref of the object calling the function calling this.
 * \param name the name or descriptor to look up.
 * \retval a pointer to the proper DESC, or NULL
 */
DESC *
lookup_desc(dbref executor, const char *name)
{
  DESC *d;

  /* Given a file descriptor. See-all only. */
  if (is_strict_integer(name)) {
    int fd = parse_integer(name);

    d = im_find(descs_by_fd, fd);
    if (d && (Priv_Who(executor) || (d->connected && d->player == executor)))
      return d;
    else
      return NULL;
  } else { /* Look up player name */
    DESC *match = NULL;
    dbref target = lookup_player(name);
    if (target == NOTHING) {
      target = match_result(executor, name, TYPE_PLAYER,
                            MAT_ABSOLUTE | MAT_PLAYER | MAT_ME | MAT_TYPE);
    }
    if (!GoodObject(target) || !Connected(target))
      return NULL;
    else {
      /* walk the descriptor list looking for a match of a dbref */
      DESC_ITER_CONN (d) {
        if ((d->player == target) && (!Hidden(d) || Priv_Who(executor)) &&
            (!match || (d->last_time > match->last_time)))
          match = d;
      }
      return match;
    }
  }
}

bool
can_see_connected(dbref player, dbref target)
{
  DESC *d;

  DESC_ITER_CONN (d) {
    if ((d->player == target) && (!Hidden(d) || Priv_Who(player)))
      return 1;
  }
  return 0;
}

/** Return the least idle descriptor of a player.
 * Ignores hidden connections unless priv is true.
 * \param player dbref of the player to find the descriptor for
 * \param priv include hidden descriptors?
 * \return pointer to the player's least idle descriptor, or NULL
 */
DESC *
least_idle_desc(dbref player, int priv)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN (d) {
    if ((d->player == player) && (priv || !Hidden(d)) &&
        (!match || (d->last_time > match->last_time)))
      match = d;
  }

  return match;
}

/** Return the conn time of the longest-connected connection of a player.
 * This function treats hidden connectios as nonexistent.
 * \param player dbref of player to get ip for.
 * \return connection time of player as an INT, or -1 if not found or hidden.
 */
int
most_conn_time(dbref player)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN (d) {
    if ((d->player == player) && !Hidden(d) &&
        (!match || (d->connected_at > match->connected_at)))
      match = d;
  }
  if (match) {
    double result = difftime(mudtime, match->connected_at);
    return (int) result;
  } else
    return -1;
}

/** Return the conn time of the longest-connected connection of a player.
 * This function does includes hidden people.
 * \param player dbref of player to get ip for.
 * \return connection time of player as an INT, or -1 if not found.
 */
int
most_conn_time_priv(dbref player)
{
  DESC *d, *match = NULL;
  DESC_ITER_CONN (d) {
    if ((d->player == player) &&
        (!match || (d->connected_at > match->connected_at)))
      match = d;
  }
  if (match) {
    double result = difftime(mudtime, match->connected_at);
    return (int) result;
  } else
    return -1;
}

/** Return the idle time of the least-idle connection of a player.
 * This function treats hidden connections as nonexistant.
 * \param player dbref of player to get time for.
 * \return idle time of player as an INT, or -1 if not found or hidden.
 */
int
least_idle_time(dbref player)
{
  DESC *d;
  d = least_idle_desc(player, 0);
  if (d) {
    double result = difftime(mudtime, d->last_time);
    return (int) result;
  } else
    return -1;
}

/** Return the idle time of the least-idle connection of a player.
 * This function performs no permission checking.
 * \param player dbref of player to get time for.
 * \return idle time of player as an INT, or -1 if not found.
 */
int
least_idle_time_priv(dbref player)
{
  DESC *d;
  d = least_idle_desc(player, 1);
  if (d) {
    double result = difftime(mudtime, d->last_time);
    return (int) result;
  } else
    return -1;
}

/** Return the ip address of the least-idle connection of a player.
 * This function performs no permission checking, and returns the
 * pointer from the descriptor itself (so don't destroy the result!)
 * \param player dbref of player to get ip for.
 * \return IP address of player as a string or NULL if not found.
 */
char *
least_idle_ip(dbref player)
{
  DESC *d;
  d = least_idle_desc(player, 1);
  return d ? (d->ip) : NULL;
}

/** Return the hostname of the least-idle connection of a player.
 * This function performs no permission checking, and returns a static
 * string.
 * \param player dbref of player to get ip for.
 * \return hostname of player as a string or NULL if not found.
 */
char *
least_idle_hostname(dbref player)
{
  DESC *d;
  static char hostname[101];
  char *p;

  d = least_idle_desc(player, 0);
  if (!d)
    return NULL;
  strcpy(hostname, d->addr);
  if ((p = strchr(hostname, '@'))) {
    p++;
    return p;
  } else
    return hostname;
}

/* ZWHO() function - really belongs in eval.c but needs stuff declared here */
/* ARGSUSED */
FUNCTION(fun_zwho)
{
  DESC *d;
  dbref zone, victim;
  int first;
  int powered = (strcmp(called_as, "ZMWHO") && Priv_Who(executor));
  first = 1;

  zone = match_thing(executor, args[0]);

  if (nargs == 1) {
    victim = executor;
  } else if ((nargs == 2) && powered) {
    if ((victim = match_thing(executor, args[1])) == 0) {
      safe_str(T(e_match), buff, bp);
      return;
    }
  } else {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (!GoodObject(zone) ||
      (!Priv_Who(executor) &&
       !eval_lock_with(victim, zone, Zone_Lock, pe_info))) {
    if (GoodObject(zone))
      fail_lock(victim, zone, Zone_Lock, NULL, NOTHING);
    safe_str(T(e_perm), buff, bp);
    return;
  }
  if ((getlock(zone, Zone_Lock) == TRUE_BOOLEXP) ||
      (IsPlayer(zone) && !(has_flag_by_name(zone, "SHARED", TYPE_PLAYER)))) {
    safe_str(T("#-1 INVALID ZONE"), buff, bp);
    return;
  }

  /* Use lowest privilege for victim */
  if (!Priv_Who(victim))
    powered = 0;

  DESC_ITER_CONN (d) {
    if (!Hidden(d) || powered) {
      if (Zone(Location(d->player)) == zone) {
        if (first) {
          first = 0;
        } else {
          safe_chr(' ', buff, bp);
        }
        safe_dbref(d->player, buff, bp);
      }
    }
  }
}

/* ARGSUSED */
FUNCTION(fun_player)
{
  /* Gets the player associated with a particular descriptor */
  DESC *d = lookup_desc(executor, args[0]);
  if (d)
    safe_dbref(d->player, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_doing)
{
  /* Gets a player's @doing */
  DESC *d = lookup_desc(executor, args[0]);
  if (d)
    safe_str(get_doing(d->player, executor, enactor, pe_info, 0), buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_hostname)
{
  /* Gets a player's hostname */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || See_All(executor)))
    safe_str(d->addr, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_ipaddr)
{
  /* Gets a player's IP address */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || See_All(executor)))
    safe_str(d->ip, buff, bp);
  else
    safe_str("#-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_cmds)
{
  /* Gets a player's IP address */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || See_All(executor)))
    safe_integer(d->cmds, buff, bp);
  else
    safe_integer(-1, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_sent)
{
  /* Gets a player's bytes sent */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || See_All(executor)))
    safe_integer(d->input_chars, buff, bp);
  else
    safe_integer(-1, buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_recv)
{
  /* Gets a player's bytes received */
  DESC *d = lookup_desc(executor, args[0]);
  if (d && (d->player == executor || See_All(executor)))
    safe_integer(d->output_chars, buff, bp);
  else
    safe_integer(-1, buff, bp);
}

FUNCTION(fun_poll) { safe_str(get_poll(), buff, bp); }

FUNCTION(fun_pueblo)
{
  /* Return the status of the pueblo flag on the least idle descriptor we
   * find that matches the player's dbref.
   */
  DESC *match = lookup_desc(executor, args[0]);
  if (match)
    safe_boolean(match->conn_flags & CONN_HTML, buff, bp);
  else
    safe_str(T("#-1 NOT CONNECTED"), buff, bp);
}

FUNCTION(fun_ssl)
{
  /* Return the status of the ssl flag on the least idle descriptor we
   * find that matches the player's dbref.
   */
  DESC *match;
  match = lookup_desc(executor, args[0]);
  if (match) {
    if (match->player == executor || See_All(executor))
      safe_boolean(is_ssl_desc(match), buff, bp);
    else
      safe_str(T(e_perm), buff, bp);
  } else
    safe_str(T("#-1 NOT CONNECTED"), buff, bp);
}

FUNCTION(fun_width)
{
  DESC *match;
  if (!*args[0])
    safe_str(T("#-1 FUNCTION REQUIRES ONE ARGUMENT"), buff, bp);
  else if ((match = lookup_desc(executor, args[0])) && match->width > 0)
    safe_integer(match->width, buff, bp);
  else if (args[1])
    safe_str(args[1], buff, bp);
  else
    safe_str("78", buff, bp);
}

FUNCTION(fun_height)
{
  DESC *match;
  if (!*args[0])
    safe_str(T("#-1 FUNCTION REQUIRES ONE ARGUMENT"), buff, bp);
  else if ((match = lookup_desc(executor, args[0])) && match->height > 0)
    safe_integer(match->height, buff, bp);
  else if (args[1])
    safe_str(args[1], buff, bp);
  else
    safe_str("24", buff, bp);
}

FUNCTION(fun_terminfo)
{
  DESC *match;
  if (!*args[0])
    safe_str(T("#-1 FUNCTION REQUIRES ONE ARGUMENT"), buff, bp);
  else if (!(match = lookup_desc(executor, args[0])))
    safe_str(T("#-1 NOT CONNECTED"), buff, bp);
  else {
    bool has_privs = (match->player == executor) || See_All(executor);
    int type;
    if (has_privs && match->ttype)
      safe_str(match->ttype, buff, bp);
    else
      safe_str(default_ttype, buff, bp);
    if (match->conn_flags & CONN_HTML)
      safe_str(" pueblo", buff, bp);
    if (has_privs) {
      if (match->conn_flags & CONN_TELNET)
        safe_str(" telnet", buff, bp);
      if (match->conn_flags & CONN_GMCP)
        safe_str(" gmcp", buff, bp);
      if (match->conn_flags & CONN_PROMPT_NEWLINES)
        safe_str(" prompt_newlines", buff, bp);
      if (is_ssl_desc(match))
        safe_str(" ssl", buff, bp);
      if (is_ws_desc(match))
        safe_str(" websocket", buff, bp);
    }
    type = notify_type(match);
    if (type & MSG_STRIPACCENTS)
      safe_str(" stripaccents", buff, bp);

    if (type & MSG_XTERM256)
      safe_str(" xterm256", buff, bp);
    else if (type & MSG_ANSI16)
      safe_str(" 16color", buff, bp);
    else if (type & MSG_ANSI2)
      safe_str(" hilite", buff, bp);
    else
      safe_str(" plain", buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_idlesecs)
{
  /* returns the number of seconds a player has been idle, using
   * their least idle connection
   */

  DESC *match = lookup_desc(executor, args[0]);
  if (match)
    safe_number(difftime(mudtime, match->last_time), buff, bp);
  else
    safe_str("-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_conn)
{
  /* returns the number of seconds a player has been connected, using
   * their longest-connected descriptor
   */

  DESC *match = lookup_desc(executor, args[0]);
  if (match)
    safe_number(difftime(mudtime, match->connected_at), buff, bp);
  else
    safe_str("-1", buff, bp);
}

/* ARGSUSED */
FUNCTION(fun_lports)
{
  DESC *d;
  int first = 1;
  dbref victim;
  int powered = 1;
  int online = 1;
  int offline = 0;

  if (!Priv_Who(executor)) {
    safe_str(T(e_perm), buff, bp);
    return;
  }

  if (nargs && args[0] && *args[0]) {
    /* An argument was given. Find the victim and adjust perms */
    if ((victim = noisy_match_result(executor, args[0], NOTYPE,
                                     MAT_EVERYTHING)) == NOTHING) {
      safe_str(T(e_notvis), buff, bp);
      return;
    }
    if (!Priv_Who(victim))
      powered = 0;
  }

  if (nargs > 1 && args[1] && *args[1]) {
    if (strcasecmp("all", args[1]) == 0) {
      offline = online = 1;
    } else if (strcasecmp("online", args[1]) == 0) {
      online = 1;
      offline = 0;
    } else if (strcasecmp("offline", args[1]) == 0) {
      online = 0;
      offline = 1;
    } else {
      safe_str(T("#-1 INVALID SECOND ARGUMENT"), buff, bp);
      return;
    }
    if (offline && !powered) {
      safe_str(T(e_perm), buff, bp);
      return;
    }
  }

  DESC_ITER (d) {
    if ((d->connected && !online) || (!d->connected && !offline))
      continue;
    if (!powered && (d->connected && Hidden(d)))
      continue;
    if (first)
      first = 0;
    else
      safe_chr(' ', buff, bp);
    safe_integer(d->descriptor, buff, bp);
  }
}

/* ARGSUSED */
FUNCTION(fun_ports)
{
  /* returns a list of the network descriptors that a player is
   * connected to
   */

  dbref target;
  DESC *d;
  int first;

  target = lookup_player(args[0]);
  if (target == NOTHING) {
    target = match_result(executor, args[0], TYPE_PLAYER,
                          MAT_ABSOLUTE | MAT_PLAYER | MAT_ME | MAT_TYPE);
  }
  if (target != executor && !Priv_Who(executor)) {
    /* This should probably be a safe_str */
    notify(executor, T("Permission denied."));
    return;
  }
  if (!GoodObject(target) || !Connected(target)) {
    return;
  }
  /* Walk descriptor chain. */
  first = 1;
  DESC_ITER_CONN (d) {
    if (d->player == target) {
      if (first)
        first = 0;
      else
        safe_chr(' ', buff, bp);
      safe_integer(d->descriptor, buff, bp);
    }
  }
}

/** Hide or unhide the specified descriptor/player.
 * Although hiding is a per-descriptor state, this function sets all of
 * a player's connected descriptors to be hidden.
 * \param player dbref of player using command.
 * \param hide if 1, hide; if 0, unhide. If 2, unhide if all connections are
 * hidden, hide if any are unhidden
 * \param victim descriptor, or name of player, to hide (or NULL to hide
 * enacting player)
 */
void
hide_player(dbref player, int hide, char *victim)
{
  DESC *d;
  dbref thing;

  if (!Can_Hide(player)) {
    notify(player, T("Permission denied."));
    return;
  }
  if (!victim || !*victim) {
    thing = Owner(player);
  } else {
    if (is_strict_integer(victim)) {
      d = lookup_desc(player, victim);
      if (!d) {
        if (See_All(player))
          notify(player, T("Couldn't find that descriptor."));
        else
          notify(player, T("Permission denied."));
        return;
      }
      thing = d->player;
      if (!Wizard(player) && thing != player) {
        notify(player, T("Permission denied."));
        return;
      }
      if (!d->connected) {
        notify(player, T("Noone is connected to that descriptor."));
        return;
      }
      if (hide == 2)
        hide = !(d->hide);
      d->hide = hide;
      if (hide) {
        notify(player, T("Connection hidden."));
      } else {
        notify(player, T("Connection unhidden."));
      }
      return;
    } else {
      thing = noisy_match_result(player, victim, TYPE_PLAYER,
                                 MAT_ABSOLUTE | MAT_PMATCH | MAT_ME | MAT_TYPE);
      if (!GoodObject(thing)) {
        return;
      }
    }
  }

  if (!Connected(thing)) {
    notify(player, T("That player is not online."));
    return;
  }

  if (hide == 2) {
    hide = 0;
    DESC_ITER_CONN (d) {
      if (d->player == thing && !d->hide) {
        hide = 1;
        break;
      }
    }
  }

  DESC_ITER_CONN (d) {
    if (d->player == thing)
      d->hide = hide;
  }
  if (hide) {
    if (player == thing)
      notify(player, T("You no longer appear on the WHO list."));
    else
      notify_format(player, T("%s no longer appears on the WHO list."),
                    AName(thing, AN_SYS, NULL));
  } else {
    if (player == thing)
      notify(player, T("You now appear on the WHO list."));
    else
      notify_format(player, T("%s now appears on the WHO list."),
                    AName(thing, AN_SYS, NULL));
  }
}

/** Perform the periodic check of inactive descriptors, and
 * disconnect them or autohide them as appropriate.
 * \return true if any players were booted/autohidden.
 */
bool
inactivity_check(void)
{
  DESC *d, *nextd;
  time_t now;
  int idle, idle_for, unconnected_idle;
  bool booted = false;

  now = mudtime;
  idle = INACTIVITY_LIMIT ? INACTIVITY_LIMIT : INT_MAX;
  unconnected_idle = UNCONNECTED_LIMIT ? UNCONNECTED_LIMIT : INT_MAX;
  for (d = descriptor_list; d; d = nextd) {
    nextd = d->next;
    idle_for = (int) difftime(now, d->last_time);

    /* If they've been connected for 60 seconds without getting a telnet-option
       back, the client probably doesn't understand them */
    if (d->conn_flags & CONN_TELNET_QUERY &&
        difftime(now, d->connected_at) >= 60.0)
      d->conn_flags &= ~CONN_TELNET_QUERY;

    /* If they've been idle for 60 seconds and are set KEEPALIVE and using
       a telnet-aware client, send a NOP */
    if (d->connected && (d->conn_flags & CONN_TELNET) && idle_for >= 60 &&
        IS(d->player, TYPE_PLAYER, "KEEPALIVE")) {
      static const char nopmsg[2] = {IAC, NOP};
      queue_newwrite(d, nopmsg, 2);
      process_output(d);
    }

    if ((d->connected) ? (idle_for > idle) : (idle_for > unconnected_idle)) {

      if (!d->connected) {
        shutdownsock(d, "idle", NOTHING, 0);
        booted = true;
      } else if (!Can_Idle(d->player)) {

        queue_string(d, T("\n*** Inactivity timeout ***\n"));
        do_rawlog(LT_CONN, "[%d/%s/%s] Logout by %s(#%d) <Inactivity Timeout>",
                  d->descriptor, d->addr, d->ip, Name(d->player), d->player);
        boot_desc(d, "idle", NOTHING);
        booted = true;
      } else if (Unfind(d->player)) {

        if ((Can_Hide(d->player)) && (!Hidden(d))) {
          queue_string(
            d, T("\n*** Inactivity limit reached. You are now HIDDEN. ***\n"));
          d->hide = 1;
          booted = true;
        }
      }
    }
  }
  return booted;
}

/** Given a player dbref, return the player's hidden status.
 * \param player dbref of player to check.
 * \retval 1 player is hidden.
 * \retval 0 player is not hidden.
 */
int
hidden(dbref player)
{
  DESC *d;
  int i = 0;
  DESC_ITER_CONN (d) {
    if (d->player == player) {
      if (!Hidden(d))
        return 0;
      else
        i++;
    }
  }
  return (i > 0);
}

#ifndef SSL_SLAVE
/** Take down all SSL client connections and close the SSL server socket.
 * Typically, this is in preparation for a shutdown/reboot.
 */
void
close_ssl_connections(void)
{
  DESC *d;

  if (!sslsock)
    return;

  /* Close clients */
  DESC_ITER (d) {
    if (d->ssl) {
      queue_string_eol(d, "%s", T(ssl_shutdown_message));
      process_output(d);
      ssl_close_connection(d->ssl);
      d->ssl = NULL;
      d->conn_flags |= CONN_CLOSE_READY;
      d->close_reason = "ssl shutdown";
    }
  }
  /* Close server socket */
  ssl_close_connection(ssl_master_socket);
  shutdown(sslsock, 2);
  closesocket(sslsock);
  sslsock = 0;
  options.ssl_port = 0;
}
#endif

/** Dump the descriptor list to our REBOOTFILE so we can restore it on reboot.
 */
void
dump_reboot_db(void)
{
  PENNFILE *f;
  DESC *d;
  uint32_t flags = RDBF_SCREENSIZE | RDBF_TTYPE | RDBF_PUEBLO_CHECKSUM |
                   RDBF_SOCKET_SRC | RDBF_NO_DOING | RDBF_CONNLOG_ID;

#ifdef LOCAL_SOCKET
  flags |= RDBF_LOCAL_SOCKET;
#endif

#ifdef SSL_SLAVE
  flags |= RDBF_SSL_SLAVE | RDBF_SLAVE_FD;
#endif

  flags |= RDBF_WEBSOCKET_FRAME;

  if (setjmp(db_err)) {
    flag_broadcast(0, 0, T("GAME: Error writing reboot database!"));
    exit(0);
  } else {
    release_fd();
    f = penn_fopen(REBOOTFILE, "w");
    /* This shouldn't happen */
    if (!f) {
      flag_broadcast(0, 0, T("GAME: Error writing reboot database!"));
      exit(0);
    }
    /* Write out the reboot db flags here */
    penn_fprintf(f, "V%u\n", (unsigned int) flags);
    putref(f, sock);
#ifdef LOCAL_SOCKET
    putref(f, localsock);
#endif
    putref(f, maxd);
    DESC_ITER (d) {
      putref(f, d->descriptor);
      putref(f, d->connected_at);
      putref(f, d->hide);
      putref(f, d->cmds);
      if (GoodObject(d->player))
        putref(f, d->player);
      else
        putref(f, -1);
      putref(f, d->last_time);
      if (d->output_prefix)
        putstring(f, (char *) d->output_prefix);
      else
        putstring(f, REBOOT_DB_NOVALUE);
      if (d->output_suffix)
        putstring(f, (char *) d->output_suffix);
      else
        putstring(f, REBOOT_DB_NOVALUE);
      putstring(f, d->addr);
      putstring(f, d->ip);
      putref_u32(f, d->conn_flags);
      putref(f, d->width);
      putref(f, d->height);
      if (d->ttype)
        putstring(f, d->ttype);
      else
        putstring(f, REBOOT_DB_NOVALUE);
      putref(f, d->source);
      putstring(f, d->checksum);
      putref_u64(f, d->ws_frame_len);
      putref_u64(f, d->connlog_id);
    } /* for loop */

    putref(f, 0);
    putstring(f, poll_msg);
    putref(f, globals.first_start_time);
    putref(f, globals.reboot_count);
#ifdef SSL_SLAVE
    putref(f, ssl_slave_pid);
    putref(f, ssl_slave_ctl_fd);
#endif
    penn_fclose(f);
  }
}

/** Load the descriptor list back from the REBOOTFILE on reboot.
 */
void
load_reboot_db(void)
{
  PENNFILE *volatile f;
  DESC *volatile tail = NULL;

  f = penn_fopen(REBOOTFILE, "r");
  if (!f) {
    restarting = 0;
    return;
  }
  restarting = 1;

  if (setjmp(db_err)) {
    do_rawlog(LT_ERR, "GAME: Unable to read reboot database!");
    penn_fclose(f);
    return;
  } else {
    DESC *closed = NULL;
    DESC *d = NULL;
    int val = 0;
    const char *temp;
    char c;
    uint32_t flags = 0;

    /* Get the first line and see if it's a set of reboot db flags.
     * Those start with V<number>
     * If not, assume we're using the original format, in which the
     * sock appears first
     * */
    c = penn_fgetc(f); /* Skip the V */
    if (c == 'V') {
      flags = getref(f);
    } else {
      penn_ungetc(c, f);
    }

    sock = getref(f);

    if (flags & RDBF_LOCAL_SOCKET)
      val = getref(f);

#ifdef LOCAL_SOCKET
    localsock = val;
#endif

    val = getref(f);
    if (val > maxd)
      maxd = val;

    while ((val = getref(f)) != 0) {
      ndescriptors++;
      d = mush_malloc(sizeof(DESC), "descriptor");
      d->descriptor = val;
      d->http_request = NULL;
      d->closer = NOTHING;
      d->close_reason = "unknown";
      d->connected_at = getref(f);
      d->conn_timer = NULL;
      d->hide = getref(f);
      d->cmds = getref(f);
      d->player = getref(f);
      d->last_time = getref(f);
      d->connected = (GoodObject(d->player) && IsPlayer(d->player))
                       ? CONN_PLAYER
                       : CONN_SCREEN;
      temp = getstring_noalloc(f);
      d->output_prefix = NULL;
      if (strcmp(temp, REBOOT_DB_NOVALUE))
        set_userstring(&d->output_prefix, temp);
      temp = getstring_noalloc(f);
      d->output_suffix = NULL;
      if (strcmp(temp, REBOOT_DB_NOVALUE))
        set_userstring(&d->output_suffix, temp);
      mush_strncpy(d->addr, getstring_noalloc(f), 100);
      mush_strncpy(d->ip, getstring_noalloc(f), 100);
      if (!(flags & RDBF_NO_DOING))
        (void) getstring_noalloc(f);
      d->conn_flags = getref_u32(f);
      if (flags & RDBF_SCREENSIZE) {
        d->width = getref(f);
        d->height = getref(f);
      } else {
        d->width = 78;
        d->height = 24;
      }
      d->ttype = NULL;
      if (flags & RDBF_TTYPE) {
        temp = getstring_noalloc(f);
        if (!strcmp(temp, REBOOT_DB_NOVALUE) || !strcmp(temp, default_ttype))
          set_ttype(d, NULL);
        else
          set_ttype(d, (char *) temp);
      }
      if (flags & RDBF_SOCKET_SRC)
        d->source = getref(f);
      if (flags & RDBF_PUEBLO_CHECKSUM)
        strcpy(d->checksum, getstring_noalloc(f));
      else
        d->checksum[0] = '\0';
      if (flags & RDBF_WEBSOCKET_FRAME) {
        d->ws_frame_len = getref_u64(f);
      } else {
        d->ws_frame_len = 0;
      }

      if (flags & RDBF_CONNLOG_ID) {
        d->connlog_id = getref_u64(f);
      } else {
        d->connlog_id = -1;
      }

      d->input_chars = 0;
      d->output_chars = 0;
      d->output_size = 0;
      init_text_queue(&d->input);
      init_text_queue(&d->output);
      d->raw_input = NULL;
      d->raw_input_at = NULL;
      d->quota = QUOTA_MAX;
      d->ssl = NULL;
      d->ssl_state = 0;
      d->next = NULL;

      if (d->conn_flags & CONN_CLOSE_READY) {
        d->close_reason = "ssl shutdown";
        d->next = closed;
        closed = d;
      } else {
        if (descriptor_list) {
          tail->next = d;
          tail = d;
        } else {
          descriptor_list = tail = d;
        }
      }
      im_insert(descs_by_fd, d->descriptor, d);
      if (d->connected && GoodObject(d->player) && IsPlayer(d->player))
        set_flag_internal(d->player, "CONNECTED");
      else if ((!d->player || !GoodObject(d->player)) && d->connected) {
        d->connected = CONN_SCREEN;
        d->player = NOTHING;
      }
    } /* while loop */

    strcpy(poll_msg, getstring_noalloc(f));
    globals.first_start_time = getref(f);
    globals.reboot_count = getref(f) + 1;

#ifndef SSL_SLAVE
    if (SSLPORT) {
      sslsock = make_socket(SSLPORT, SOCK_STREAM, NULL, NULL, SSL_IP_ADDR);
      ssl_master_socket = ssl_setup_socket(sslsock);
      if (sslsock >= maxd)
        maxd = sslsock + 1;
    }
#endif

    if (flags & RDBF_SSL_SLAVE)
      val = getref(f);
    else
      val = -1;

#ifdef SSL_SLAVE
    ssl_slave_pid = val;

    if (flags & RDBF_SLAVE_FD)
      ssl_slave_ctl_fd = getref(f);
    else
      ssl_slave_ctl_fd = -1;

    if (SSLPORT && (ssl_slave_pid == -1 || kill(ssl_slave_pid, 0) != 0)) {
      /* Attempt to restart a missing ssl_slave on reboot */
      do_rawlog(LT_ERR, "ssl_slave does not appear to be running on reboot. "
                        "Restarting the slave.");
      if (make_ssl_slave() < 0)
        do_rawlog(LT_ERR, "Unable to start ssl_slave");
    } else
      ssl_slave_state = SSL_SLAVE_RUNNING;

#endif

    clean_descriptors(&closed);

    penn_fclose(f);
    remove(REBOOTFILE);
  }

  flag_broadcast(0, 0, T("GAME: Reboot finished."));
}

/** Reboot the game without disconnecting players.
 * \verbatim
 * This implements @shutdown/reboot, which performs a dump, saves
 * information about which player is associated with which socket,
 * and then re-execs the mush process without closing the sockets.
 * \endverbatim
 * \param player the enactor.
 * \param flag if 0, normal dump; if 1, paranoid dump.
 */
void
do_reboot(dbref player, int flag)
{
#ifndef WIN32
  /* Quick and dirty check to make sure the executable is still
     there. Not a security check to speak of, hence the race condition
     implied by using access() doesn't matter. The exec can still fail
     for various reasons, but if it does, it gets logged and you get an
     inadvertent full @shutdown. */
  if (access(saved_argv[0], R_OK | X_OK) < 0) {
    notify_format(player, T("Unable to reboot using executable '%s': %s"),
                  saved_argv[0], strerror(errno));
    return;
  }
#endif

  if (player == NOTHING) {
    flag_broadcast(
      0, 0, T("GAME: Reboot w/o disconnect from game account, please wait."));
    do_rawlog(LT_WIZ, "Reboot w/o disconnect triggered by signal.");
  } else {
    flag_broadcast(0, 0, T("GAME: Reboot w/o disconnect by %s, please wait."),
                   AName(Owner(player), AN_ANNOUNCE, NULL));
    do_rawlog(LT_WIZ, "Reboot w/o disconnect triggered by %s(#%d).",
              Name(player), player);
  }
  if (flag) {
    globals.paranoid_dump = 1;
    globals.paranoid_checkpt = db_top / 5;
    if (globals.paranoid_checkpt < 1)
      globals.paranoid_checkpt = 1;
  }
#ifndef SSL_SLAVE
  close_ssl_connections();
#endif
  if (!fork_and_dump(0)) {
    /* Database save failed. Cancel the reboot */
    flag_broadcast(0, 0, T("GAME: Reboot failed."));
    return;
  }
  sql_shutdown();
  shutdown_queues();
#ifndef PROFILING
#ifndef WIN32
/* Some broken libcs appear to retain the itimer across exec!
 * So we make sure that if we get a SIGPROF in our next incarnation,
 * we ignore it until our proper handler is set up.
 */
#ifdef __CYGWIN__
  ignore_signal(SIGALRM);
#else
  ignore_signal(SIGPROF);
#endif /* __CYGWIN__ */
#endif /* WIN32 */
#endif /* PROFILING */
  dump_reboot_db();
#ifdef INFO_SLAVE
  kill_info_slave();
#endif
  local_shutdown();
  shutdown_conndb(1);
  close_help_files();
  end_all_logs();
#ifndef WIN32
  {
    const char *args[6];
    int n = 0;

    args[n++] = saved_argv[0];
    args[n++] = "--no-session";
    if (pidfile) {
      args[n++] = "--pid-file";
      args[n++] = pidfile;
    }
    args[n++] = confname;
    args[n] = NULL;

    execv(saved_argv[0], (char **) args);
  }
#else
  execl("pennmush.exe", "pennmush.exe", "/run", NULL);
#endif /* WIN32 */
  /* Shouldn't ever get here, but just in case... */
  do_rawlog(LT_ERR, "Unable to restart game: exec: %s\nAborting.",
            strerror(errno));
  exit(1);
}

/* File modification watching code. Linux-specific for now.
 * Future directions include: kqueue() for BSD, fam for linux, irix, others?
 *
 * The idea to watch help.txt and motd.txt and friends to avoid having
 * to do a manual @readcache. Rather than figuring out which exact
 * file was changed, just re-read them all on modification of
 * any. That will probably change in the future.
 */

extern HASHTAB help_files;

#ifdef HAVE_INOTIFY_INIT1
/* Linux 2.6.27 and greater inotify file monitoring interface */

intmap *watchtable = NULL;
int watch_fd = -1;

void
WATCH(const char *name)
{
  int wd;

  if (watch_fd < 0)
    return;

  if (*name != NUMBER_TOKEN) {
    if ((wd = inotify_add_watch(watch_fd, name,
                                IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF)) < 0)
      do_rawlog(LT_TRACE, "file_watch_init:inotify_add_watch(\"%s\"): %s", name,
                strerror(errno));
    else
      im_insert(watchtable, wd, (void *) name);
  }
}

static void
watch_files_in(void)
{
  int n;
  help_file *h;

  if (!watchtable)
    watchtable = im_new();

  if (watch_fd < 0)
    return;

  do_rawlog(LT_TRACE, "'No such file or directory' errors immediately "
                      "following are probably harmless.");
  for (n = 0; n < 2; n++) {
    WATCH(options.connect_file[n]);
    WATCH(options.motd_file[n]);
    WATCH(options.wizmotd_file[n]);
    WATCH(options.newuser_file[n]);
    WATCH(options.register_file[n]);
    WATCH(options.quit_file[n]);
    WATCH(options.down_file[n]);
    WATCH(options.full_file[n]);
    WATCH(options.guest_file[n]);
    WATCH(options.who_file[n]);
  }

  for (h = hash_firstentry(&help_files); h; h = hash_nextentry(&help_files))
    WATCH(h->file);
}

static int
file_watch_init_in(void)
{
  if (watch_fd != -1) {
    close(watch_fd);
    im_destroy(watchtable);
    watchtable = NULL;
  }

  watch_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

  if (watch_fd < 0) {
    penn_perror("file_watch_init: inotify_init1");
    return -1;
  }

  if (watch_fd >= maxd)
    maxd = watch_fd + 1;

  watch_files_in();

  return watch_fd;
}

static void
file_watch_event_in(int fd)
{
  uint8_t raw[BUFFER_LEN], *ptr;
  int len, lastwd = -1;

  while ((len = read(fd, raw, sizeof raw)) > 0) {
    ptr = raw;
    while (len > 0) {
      int thislen;
      struct inotify_event *ev = (struct inotify_event *) ptr;
      const char *file = im_find(watchtable, ev->wd);

      thislen = sizeof(struct inotify_event) + ev->len;
      len -= thislen;
      ptr += thislen;

      if (file) {
        if (!(ev->mask & IN_IGNORED)) {
          if (ev->mask & IN_DELETE_SELF) {
            inotify_rm_watch(fd, ev->wd);
            im_delete(watchtable, ev->wd);
          }
          if (lastwd == ev->wd)
            continue;
          if (fcache_read_one(file)) {
            do_rawlog(LT_TRACE, "Updated cached copy of %s.", file);
            WATCH(file);
          } else if (help_rebuild_by_name(file)) {
            do_rawlog(LT_TRACE, "Reindexing help file %s.", file);
            WATCH(file);
          } else {
            do_rawlog(LT_ERR,
                      "Got status change for file '%s' but I don't "
                      "know what to do with it! Mask 0x%x",
                      file, ev->mask);
          }
          lastwd = ev->wd;
        }
      }
    }
  }
}
#endif

/** Start monitoring various useful files for changes.
 * \return descriptor of the notification service, or -1 on error
 */
int
file_watch_init(void)
{
#ifdef HAVE_INOTIFY_INIT1
  return file_watch_init_in();
#else
  return -1;
#endif
}

/** Test for modified files and re-read them if indicated.
 * \param fd the notification monitorh descriptor
 */
void
file_watch_event(int fd __attribute__((__unused__)))
{
#ifdef HAVE_INOTIFY_INIT1
  file_watch_event_in(fd);
#endif
  return;
}
