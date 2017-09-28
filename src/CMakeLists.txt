cmake_minimum_required(VERSION 3.7)
project(PennMUSH LANGUAGES C VERSION 1.8.6.2)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(CheckLibraryExists)
include(CheckIncludeFile)
include(FindPackageHandleStandardArgs)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

include(FindUnixCommands)
include(FindPerl)
include(FindDoxygen)
find_program(GPERF gperf)
find_program(ED ed)
find_program(UPTIME uptime)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../hdrs)

# Header files
check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(crypt.h HAVE_CRYPT_H)
check_include_file(fenv.h HAVE_FENV_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(langinfo.h HAVE_LANGINFO_H)
check_include_file(libintl.h HAVE_LIBINTL_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(netinet/in.h HAVE_NETINET_IN_H)
check_include_file(netinet/tcp.h HAVE_NETINET_TCP_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(stdbool.h HAVE_STDBOOL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(sys/event.h HAVE_SYS_EVENT_H)
check_include_file(sys/inotify.h HAVE_SYS_INOTIFY_H)
check_include_file(sys/param.h HAVE_SYS_PARAM_H)
check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/ucred.h HAVE_SYS_UCRED_H)
check_include_file(sys/uio.h HAVE_SYS_UIO_H)
check_include_file(sys/un.h HAVE_SYS_UN_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(zlib.h HAVE_ZLIB_H)

# Required libraries
set(SAVED_CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET 1)
check_symbol_exists(sin "math.h" HAVE_SIN)
if(NOT HAVE_SIN)
  unset(HAVE_SIN)
  list(APPEND CMAKE_REQUIRED_LIBRARIES m)
  list(APPEND LIBS m)
  check_symbol_exists(sin "math.h" HAVE_SIN2)
  if(NOT HAVE_SIN2)
    message(FATAL_ERROR "Unable to link with math functions.")
  else()
    message(STATUS "libm required for math functions.")
  endif()
endif()
set(CMAKE_REQUIRED_QUIET ${SAVED_CMAKE_REQUIRED_QUIET})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
list(APPEND CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})
list(APPEND LIBS ${OPENSSL_LIBRARIES})

find_package(PCRE REQUIRED)
include_directories(${PCRE_INCLUDE_DIR})
list(APPEND LIBS ${PCRE_LIBRARY})

# Optional libraries
include(FindIntl)

find_library(LIBCRYPT_LIBRARY crypt)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibCrypt DEFAULT_MSG LIBCRYPT_LIBRARY)
if(LIBCRYPT_LIBRARY)
  list(APPEND CMAKE_REQUIRED_LIBRARIES crypt)
  list(APPEND LIBS crypt)
endif()

include(FindLibEvent)
if(Libevent_FOUND)
  find_library(LIBEVENT_EXTRA_LIBRARY event_extra)
  find_library(LIBEVENT_OPENSSL_LIBRARY event_openssl)
  set(LIBEVENT_LIBRARIES "${LIBEVENT_LIBRARIES} ${LIBEVENT_EXTRA_LIBRARY} ${LIBEVENT_OPENSSL_LIBRARY}")
endif()

find_path(LIBZ_INCLUDE_DIR zlib.h)
find_library(LIBZ_LIBRARY z)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibZ DEFAULT_MSG LIBZ_LIBRARY LIBZ_INCLUDE_DIR)
if(LIBZ_LIBRARY)
  list(APPEND LIBS z)
  set(HAVE_LIBZ 1)
endif()

# TODO: MySQL, Sqlite3
find_package(PostgreSQL)
if(${PostgreSQL_FOUND})
  include_directories(${PostgreSQL_INCLUDE_DIRS})
  list(APPEND LIBS ${PostgreSQL_LIBRARIES})
  set(HAVE_POSTGRESQL 1)
endif()

# Functions
check_symbol_exists(bindtextdomain "libintl.h" HAVE_BINDTEXTDOMAIN)
check_symbol_exists(cbrt "math.h" HAVE_CBRT)
check_symbol_exists(crypt "unistd.h;crypt.h" HAVE_CRYPT)
check_symbol_exists(DH_set0_pqg "openssl/dh.h" HAVE_DH_SET0_PQG)
check_symbol_exists(EVP_MD_do_all "openssl/evp.h" HAVE_EVP_MD_DO_ALL)
check_symbol_exists(fcntl "unistd.h;fcntl.h" HAVE_FCNTL)
check_symbol_exists(feclearexcept "fenv.h" HAVE_FECLEAREXCEPT)
check_symbol_exists(fetestexcept "fenv.h" HAVE_FETESTEXCEPT)
check_symbol_exists(fork "unistd.h" HAVE_FORK)
check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO)

set(SAVED_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_DEFINITIONS -D_XOPEN_SOURCE=500 -D_GNU_SOURCE)
check_symbol_exists(getdate "time.h" HAVE_GETDATE)
set(CMAKE_REQUIRED_DEFINITIONS ${SAVED_CMAKE_REQUIRED_DEFINITIONS})

check_symbol_exists(geteuid "unistd.h" HAVE_GETEUID)
check_symbol_exists(getpid "unistd.h" HAVE_GETPID)
check_symbol_exists(getppid "unistd.h" HAVE_GETPPID)
check_symbol_exists(getpriority "sys/time.h;sys/resource.h" HAVE_GETPRIORITY)
check_symbol_exists(getrlimit "sys/time.h;sys/resource.h" HAVE_GETRLIMIT)
check_symbol_exists(getrusage "sys/time.h;sys/resource.h" HAVE_GETRUSAGE)
check_symbol_exists(getuid "unistd.h" HAVE_GETUID)
check_symbol_exists(imaxdiv "inttypes.h" HAVE_IMAXDIV)
check_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY_INIT)
check_symbol_exists(inotify_init1 "sys/inotify.h" HAVE_INOTIFY_INIT1)
check_symbol_exists(isnormal "math.h" HAVE_ISNORMAL)
check_symbol_exists(kqueue "sys/event.h" HAVE_KQUEUE)
check_symbol_exists(log2 "math.h" HAVE_LOG2)
check_symbol_exists(lrint "math.h" HAVE_LRINT)
check_symbol_exists(nl_langinfo "langinfo.h" HAVE_NL_LANGINFO)
check_symbol_exists(poll "poll.h" HAVE_POLL)
check_symbol_exists(posix_fadvise "fcntl.h" HAVE_POSIX_FADVISE)
check_symbol_exists(posix_fallocate "fcntl.h" HAVE_POSIX_FALLOCATE)
check_symbol_exists(posix_memalign "stdlib.h" HAVE_POSIX_MEMALIGN)
check_symbol_exists(pread "unistd.h" HAVE_PREAD)
check_symbol_exists(pwrite "unistd.h" HAVE_PWRITE)
check_symbol_exists(select "unistd.h" HAVE_SELECT)
check_symbol_exists(seteuid "unistd.h" HAVE_SETEUID)
check_symbol_exists(setitimer "sys/time.h" HAVE_SETITIMER)
check_symbol_exists(setpgid "unistd.h" HAVE_SETPGID)
check_symbol_exists(setpriority "sys/time.h;sys/resource.h" HAVE_SETPRIORITY)
check_symbol_exists(setsid "unistd.h" HAVE_SETSID)
check_symbol_exists(SHA "openssl/sha.h" HAVE_SHA)
check_symbol_exists(sigaction "signal.h" HAVE_SIGACTION)
check_symbol_exists(sigprocmask "signal.h" HAVE_SIGPROCMASK)
check_symbol_exists(socketpair "sys/types.h;sys/socket.h" HAVE_SOCKETPAIR)

set(SAVED_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(strchrnul "string.h" HAVE_STRCHRNUL)
set(CMAKE_REQUIRED_DEFINITIONS ${SAVED_CMAKE_REQUIRED_DEFINITIONS})

check_symbol_exists(strdup "string.h" HAVE_STRDUP)
check_symbol_exists(strxfrm "string.h" HAVE_STRXFRM)
check_symbol_exists(sysconf "unistd.h" HAVE_SYSCONF)
check_symbol_exists(textdomain "libintl.h" HAVE_TEXTDOMAIN)

set(SAVED_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(vasprintf "stdio.h" HAVE_VASPRINTF)
set(CMAKE_REQUIRED_DEFINITIONS ${SAVED_CMAKE_REQUIRED_DEFINITIONS})

check_symbol_exists(waitpid "sys/types.h;sys/wait.h" HAVE_WAITPID)
check_symbol_exists(writev "sys/uio.h" HAVE_WRITEV)
check_symbol_exists(_stricmp "string.h" HAVE__STRICMP)
check_symbol_exists(_stricoll "string.h" HAVE__STRICOLL)
check_symbol_exists(_strncoll "string.h" HAVE__STRNCOLL)
check_symbol_exists(_strnicmp "string.h" HAVE__STRNICMP)
check_symbol_exists(_strnicoll "string.h" HAVE__STRNICOLL)
check_symbol_exists(_vsnprintf_s "string.h" HAVE__VSNPRINTF_S)

# Files
if(EXISTS /dev/urandom)
  set(HAVE_DEV_URANDOM 1)
endif()

# Programs
if(ED)
  set(HAVE_ED 1)
endif()
if(UPTIME)
  set(HAVE_UPTIME 1)
endif()

# TO-DO:
# ZONEINFO

# create local source files
if(NOT EXISTS cmdlocal.c)
  execute_process(COMMAND ${CP} cmdlocal.dst cmdlocal.c WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
if(NOT EXISTS flaglocal.c)
  execute_process(COMMAND ${CP} flaglocal.dst flaglocal.c WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
if(NOT EXISTS funlocal.c)
  execute_process(COMMAND ${CP} funlocal.dst funlocal.c WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
if(NOT EXISTS local.c)
  execute_process(COMMAND ${CP} local.dst local.c WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()

if(EXISTS ${GPERF})

  if(bflags.gperf IS_NEWER_THAN bflags.c)
    execute_process(COMMAND ${GPERF} -C --output-file bflags.c bflags.gperf WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  endif()

  if(htmltab.gperf IS_NEWER_THAN htmltab.c)
    execute_process(COMMAND ${GPERF} -C --output-file htmltab.c htmltab.gperf WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  endif()

  if(lmathtab.gperf IS_NEWER_THAN lmathtab.c)
    execute_process(COMMAND ${GPERF} -C --output-file lmathtab.c lmathtab.gperf WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  endif()

  if(rgbtab.gperf IS_NEWER_THAN rgbtab.c)
    execute_process(COMMAND ${GPERF} -C --output-file rgbtab.c rgbtab.gperf WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  endif()

else()

  if(bflags.gperf IS_NEWER_THAN bflags.c OR htmltab.gperf IS_NEWER_THAN htmltab.c OR lmathtab.gperf IS_NEWER_THAN lmathtab.c OR rgbtab.gperf IS_NEWER_THAN rgbtab.c)
    message(WARNING "gperf not installed and hash tables need to be updated.")
  endif()

endif()

execute_process(COMMAND ${PERL} ${PROJECT_SOURCE_DIR}/../utils/mkcmds.pl all WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

add_executable(netmush access.c atr_tab.c attrib.c boolexp.c bsd.c bufferq.c
	charconv.c chunk.c cmdlocal.c cmds.c command.c compress.c	
	conf.c cque.c create.c db.c destroy.c extchat.c extmail.c	
	filecopy.c flaglocal.c flags.c funcrypt.c function.c fundb.c	
	funlist.c funlocal.c funmath.c funmisc.c funstr.c funtime.c	
	funufun.c game.c hash_function.c help.c htab.c intmap.c		
	local.c lock.c log.c look.c malias.c markup.c match.c		
	memcheck.c move.c mycrypt.c mymalloc.c mysocket.c myrlimit.c	
	myssl.c notify.c parse.c player.c plyrlist.c predicat.c		
	privtab.c info_master.c ptab.c rob.c services.c set.c SFMT.c	
	sig.c sort.c speech.c sql.c ssl_master.c strdup.c strtree.c	
	strutil.c tables.c timer.c tz.c unparse.c utils.c version.c	
	wait.c warnings.c wild.c wiz.c)

# Compiler features
set_property(TARGET netmush PROPERTY C_STANDARD 99)
target_compile_features(netmush PUBLIC c_restrict)

check_c_source_compiles(
    "int main(void) { return __builtin_expect(0, 1); }" HAVE_BUILTIN_EXPECT)

# TO-DO:
# Warnings
# march=native
# stack protector


target_include_directories(netmush PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../hdrs ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(netmush ${LIBS})

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

include(WriteCompilerDetectionHeader)

write_compiler_detection_header(FILE confcompiler.h PREFIX HAVE COMPILERS GNU Clang FEATURES c_restrict)
