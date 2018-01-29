cmake_minimum_required(VERSION 3.5)
project(PennMUSH LANGUAGES C VERSION 1.8.6.2)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})

include(CheckLibraryExists)
include(CheckIncludeFile)
include(FindPackageHandleStandardArgs)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckCCompilerFlag)

message(STATUS "Checking for installed programs.")
find_package(UnixCommands)
find_package(Perl REQUIRED)
find_package(Doxygen)
find_program(GPERF gperf)
find_program(ED ed)
find_program(UPTIME uptime)
find_program(CTAGS ctags)
find_program(ETAGS etags)
find_program(INDENT NAMES clang-format clang-format-6.0 clang-format-5.0 clang-format-4.0)
find_program(TEST test)
find_program(CAT cat)
find_program(ECHO echo)
find_program(CHMOD chmod)

#include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../hdrs")

# Header files
message(STATUS "Checking for system header files.")
check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(byteswap.h HAVE_BYTESWAP_H)
check_include_file(crypt.h HAVE_CRYPT_H)
check_include_File(endian.h HAVE_ENDIAN_H)
check_include_file(fenv.h HAVE_FENV_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(langinfo.h HAVE_LANGINFO_H)
check_include_file(libintl.h HAVE_LIBINTL_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(netinet/in.h HAVE_NETINET_IN_H)
check_include_file(netinet/tcp.h HAVE_NETINET_TCP_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(sqlite3.h HAVE_SQLITE3_H)
check_include_file(stdbool.h HAVE_STDBOOL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(sys/endian.h HAVE_SYS_ENDIAN_H)
check_include_file(sys/event.h HAVE_SYS_EVENT_H)
check_include_file(sys/eventfd.h HAVE_SYS_EVENTFD_H)
check_include_file(sys/inotify.h HAVE_SYS_INOTIFY_H)
check_include_file(sys/param.h HAVE_SYS_PARAM_H)
check_include_file(sys/prctl.h HAVE_SYS_PRCTL_H)
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
check_c_source_compiles(
  "#include <sys/time.h>
#include <time.h>
int main(void) { return 0; }" TIME_WITH_SYS_TIME)

# Required libraries
message(STATUS "Checking for required library dependencies.")
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
message(STATUS "Checking for optional library dependencies.")
include(FindIntl)
if(Intl_FOUND)
  list(APPEND LIBS ${Intl_LIBRARIES})
endif()

find_library(LIBCRYPT_LIBRARY crypt)
find_package_handle_standard_args(LibCrypt REQUIRED_VARS LIBCRYPT_LIBRARY)
if(LibCrypt_FOUND)
  list(APPEND CMAKE_REQUIRED_LIBRARIES crypt)
  list(APPEND MUSHLIBS crypt)
endif()

#set(Libevent_FIND_QUIETLY 1)
find_package(LibEvent)
if(Libevent_FOUND)
  find_library(LIBEVENT_EXTRA_LIBRARY event_extra)
  find_library(LIBEVENT_OPENSSL_LIBRARY event_openssl)
  list(APPEND LIBEVENT_LIBRARIES ${LIBEVENT_EXTRA_LIBRARY} ${LIBEVENT_OPENSSL_LIBRARY})
  list(APPEND SLAVELIBS ${LIBEVENT_LIBRARIES})
  set(HAVE_LIBEVENT_CORE 1)
endif()

find_library(LIBZ_LIBRARY z)
find_package_handle_standard_args(LibZ REQUIRED_VARS LIBZ_LIBRARY HAVE_ZLIB_H)
if(LibZ_FOUND)
  list(APPEND LIBS ${LIBZ_LIBRARY})
  set(HAVE_LIBZ 1)
  set(SAVED_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
  list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBZ_LIBRARY})
  check_symbol_exists(gzbuffer "zlib.h" HAVE_GZBUFFER)
  check_symbol_exists(gzvprintf "zlib.h" HAVE_GZVPRINTF)
  set(CMAKE_REQUIRED_LIBRARIES ${SAVED_CMAKE_REQUIRED_LIBRARIES})
endif()

find_package(MySQL)
if(MYSQL_FOUND)
  list(APPEND MUSHLIBS ${MYSQL_LIBRARIES})
  include_directories(${MYSQL_INCLUDE_DIRS})
endif()
  
find_library(SQLITE3_LIBRARY sqlite3)
find_package_handle_standard_args(LibSqlite3 REQUIRED_VARS SQLITE3_LIBRARY HAVE_SQLITE3_H)
if(LibSqlite3_FOUND)
  list(APPEND MUSHLIBS ${SQLITE3_LIBRARY})
  set(HAVE_SQLITE3 1)
endif()

find_package(PostgreSQL)
if(PostgreSQL_FOUND)
  include_directories(${PostgreSQL_INCLUDE_DIRS})
  list(APPEND MUSHLIBS ${PostgreSQL_LIBRARIES})
  set(HAVE_POSTGRESQL 1)
endif()

# Functions
message(STATUS "Checking for available system functions.")
check_symbol_exists(bindtextdomain "libintl.h" HAVE_BINDTEXTDOMAIN)
check_symbol_exists(cbrt "math.h" HAVE_CBRT)
check_symbol_exists(crypt "unistd.h;crypt.h" HAVE_CRYPT)
check_symbol_exists(DH_set0_pqg "openssl/dh.h" HAVE_DH_SET0_PQG)
check_symbol_exists(eventfd "sys/eventfd.h" HAVE_EVENTFD)
check_symbol_exists(EVP_MD_do_all "openssl/evp.h" HAVE_EVP_MD_DO_ALL)
check_symbol_exists(fcntl "unistd.h;fcntl.h" HAVE_FCNTL)
check_symbol_exists(feclearexcept "fenv.h" HAVE_FECLEAREXCEPT)
check_symbol_exists(fetestexcept "fenv.h" HAVE_FETESTEXCEPT)
check_symbol_exists(ffs "string.h" HAVE_FFS)
check_symbol_exists(fork "unistd.h" HAVE_FORK)
check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO)
check_symbol_exists(getc_unlocked "stdio.h" HAVE_GETC_UNLOCKED)
check_symbol_exists(geteuid "unistd.h" HAVE_GETEUID)
check_symbol_exists(getpagesize "unistd.h" HAVE_GETPAGESIZE)
check_symbol_exists(getpid "unistd.h" HAVE_GETPID)
check_symbol_exists(getppid "unistd.h" HAVE_GETPPID)
check_symbol_exists(getpriority "sys/time.h;sys/resource.h" HAVE_GETPRIORITY)
check_symbol_exists(getrlimit "sys/time.h;sys/resource.h" HAVE_GETRLIMIT)
check_symbol_exists(getrusage "sys/time.h;sys/resource.h" HAVE_GETRUSAGE)
check_symbol_exists(gettext "libintl.h" HAVE_GETTEXT)
check_symbol_exists(getuid "unistd.h" HAVE_GETUID)
check_symbol_exists(hypot "math.h" HAVE_HYPOT)
check_symbol_exists(imaxdiv "inttypes.h" HAVE_IMAXDIV)
check_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY_INIT)
check_symbol_exists(inotify_init1 "sys/inotify.h" HAVE_INOTIFY_INIT1)
check_symbol_exists(isnormal "math.h" HAVE_ISNORMAL)
check_symbol_exists(kqueue "sys/event.h" HAVE_KQUEUE)
check_symbol_exists(log2 "math.h" HAVE_LOG2)
check_symbol_exists(lrint "math.h" HAVE_LRINT)
check_symbol_exists(nl_langinfo "langinfo.h" HAVE_NL_LANGINFO)
check_symbol_exists(pledge "unistd.h" HAVE_PLEDGE)
check_symbol_exists(poll "poll.h" HAVE_POLL)
check_symbol_exists(posix_fadvise "fcntl.h" HAVE_POSIX_FADVISE)
check_symbol_exists(posix_fallocate "fcntl.h" HAVE_POSIX_FALLOCATE)
check_symbol_exists(posix_memalign "stdlib.h" HAVE_POSIX_MEMALIGN)
check_symbol_exists(prctl "sys/prctl.h" HAVE_PRCTL)
check_symbol_exists(pread "unistd.h" HAVE_PREAD)
check_symbol_exists(putc_unlocked "stdio.h" HAVE_PUTC_UNLOCKED)
check_symbol_exists(pwrite "unistd.h" HAVE_PWRITE)
check_symbol_exists(seteuid "unistd.h" HAVE_SETEUID)
check_symbol_exists(setitimer "sys/time.h" HAVE_SETITIMER)
check_symbol_exists(setpgid "unistd.h" HAVE_SETPGID)
check_symbol_exists(setpriority "sys/time.h;sys/resource.h" HAVE_SETPRIORITY)
check_symbol_exists(setsid "unistd.h" HAVE_SETSID)
check_symbol_exists(SHA "openssl/sha.h" HAVE_SHA)
check_symbol_exists(sigaction "signal.h" HAVE_SIGACTION)
check_symbol_exists(sigprocmask "signal.h" HAVE_SIGPROCMASK)
check_symbol_exists(socketpair "sys/types.h;sys/socket.h" HAVE_SOCKETPAIR)
check_symbol_exists(strcasecmp "string.h" HAVE_STRCASECMP)
check_symbol_exists(strdup "string.h" HAVE_STRDUP)
check_symbol_exists(strncasecmp "string.h" HAVE_STRNCASECMP)
check_symbol_exists(strxfrm "string.h" HAVE_STRXFRM)
check_symbol_exists(sysconf "unistd.h" HAVE_SYSCONF)
check_symbol_exists(textdomain "libintl.h" HAVE_TEXTDOMAIN)
check_symbol_exists(vsnprintf "stdarg.h;stdio.h" HAVE_VSNPRINTF)
check_symbol_exists(wait "sys/types.h;sys/wait.h" HAVE_WAIT)
check_symbol_exists(waitpid "sys/types.h;sys/wait.h" HAVE_WAITPID)
check_symbol_exists(wait3 "sys/types.h;sys/time.h;sys/resource.h;sys/wait.h" HAVE_WAIT3)
check_symbol_exists(writev "sys/uio.h" HAVE_WRITEV)
check_symbol_exists(_stricmp "string.h" HAVE__STRICMP)
check_symbol_exists(_stricoll "string.h" HAVE__STRICOLL)
check_symbol_exists(_strncoll "string.h" HAVE__STRNCOLL)
check_symbol_exists(_strnicmp "string.h" HAVE__STRNICMP)
check_symbol_exists(_strnicoll "string.h" HAVE__STRNICOLL)
check_symbol_exists(_vsnprintf_s "string.h" HAVE__VSNPRINTF_S)

# Functions that need _XOPEN_SOURCE=500
set(SAVED_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_DEFINITIONS -D_XOPEN_SOURCE=500)
check_symbol_exists(getdate "time.h" HAVE_GETDATE)
set(CMAKE_REQUIRED_DEFINITIONS ${SAVED_CMAKE_REQUIRED_DEFINITIONS})

# Functions that need _GNU_SOURCE to be defined.
set(SAVED_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(strchrnul "string.h" HAVE_STRCHRNUL)
check_symbol_exists(fgets_unlocked "stdio.h" HAVE_FGETS_UNLOCKED)
check_symbol_exists(fputs_unlocked "stdio.h" HAVE_FPUTS_UNLOCKED)
check_symbol_exists(vasprintf "stdio.h" HAVE_VASPRINTF)
set(CMAKE_REQUIRED_DEFINITIONS ${SAVED_CMAKE_REQUIRED_DEFINITIONS})

# Types
if(HAVE_SYS_UCRED_H)
  set(SAVED_CMAKE_EXTRA_INCLUDE_FILES ${CMAKE_EXTRA_INCLUDE_FILES})
  list(APPEND CMAKE_EXTRA_INCLUDE_FILES "sys/ucred.h")
  check_type_size("struct xucred" HAVE_STRUCT_XUCRED)
  set(CMAKE_EXTRA_INCLUDE_FILES ${SAVED_CMAKE_EXTRA_INCLUDE_FILES})
endif()

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

# ZONEINFO
foreach(DIR IN ITEMS /usr/share/zoneinfo /usr/lib/zoneinfo /usr/local/etc/zoneinfo /usr/share/lib/zoneinfo)
  if(EXISTS ${DIR}/UTC)
    set(TZDIR ${DIR})
    set(HAVE_ZONEINFO 1)
    message(STATUS "Time zone files found at ${DIR}")
    break()
  endif()
endforeach()

# Compiler features and options

include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)
check_c_source_compiles(
  "int main(void) { return __builtin_expect(0, 1); }" HAVE_BUILTIN_EXPECT)
check_c_source_compiles(
  "int main(int argc, char **argv __attribute__((unused))) { return 0; }"
  HAVE___ATTRIBUTE__)
check_c_source_compiles(
  "char *mystrdup(void) __attribute__((__malloc__)); int main(void) { return 0; }"
  HAVE___ATTRIBUTE___MALLOC)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(SAVED_CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET})
  set(CMAKE_REQUIRED_QUIET 1)
  check_c_compiler_flag(-ggdb HAVE_G_GDB)
  check_c_compiler_flag(-g HAVE_G)
  check_c_compiler_flag(-Og HAVE_OG)
  check_c_compiler_flag(-O HAVE_O)
  check_c_compiler_flag(-Wall HAVE_W_ALL)
  check_c_compiler_flag(-Wextra HAVE_W_EXTRA)
  check_c_compiler_flag(-Wno-comment HAVE_W_NO_COMMENT)
  check_c_compiler_flag(-Wno-char-subscripts HAVE_W_NO_CHAR_SUBSCRIPTS)
  if(WIN32)
    check_c_compiler_flag(-Wno-unknown-pragmas HAVE_W_NO_UNKNOWN_PRAGMAS)
  endif()
  check_c_compiler_flag(-funsigned-char HAVE_UNSIGNED_CHAR)
  check_c_compiler_flag(-fstack-protector HAVE_STACK_PROTECTOR)
  check_c_compiler_flag(-Wa,--noexecstack HAVE_NOEXECSTACK)
  check_c_compiler_flag(-march=native HAVE_MARCH_NATIVE)
  check_c_compiler_flag(-Wl,-z,noexecstack HAVE_LD_NOEXECSTACK)
  set(CMAKE_REQUIRED_QUIET ${SAVED_CMAKE_REQUIRED_QUIET})
  
  if(HAVE_G_GDB)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -ggdb)
  elseif(HAVE_G)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -g)
  endif()
  if(HAVE_OG)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -Og)
  elseif(HAVE_O)
    list(APPEND CMAKE_REQUIRED_DEFINIITONS -O)
  endif()
  if(HAVE_W_ALL)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -Wall)
  endif()
  if(HAVE_W_EXTRA)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -Wextra)
  endif()
  if(HAVE_W_NO_COMMENT)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -Wno-comment)
  endif()
  if(HAVE_W_NO_CHAR_SUBSCRIPTS)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -Wno-char-subscripts)
  endif()
  if(HAVE_W_NO_UNKNOWN_PRAGMAS)
    list(APPEND CMAKE_REQUIRE_DEFINITIONS -Wno-unknown-pragmas)
  endif()
  if(HAVE_UNSIGNED_CHAR)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -funsigned-char)
  endif()
  if(HAVE_STACK_PROTECTOR)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -fstack-protector)
  endif()
  if(HAVE_NOEXECSTACK)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -Wa,--noexecstack)
  endif()
  if(HAVE_MARCH_NATIVE)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -march=native)
  endif()
  if(HAVE_LD_NOEXECSTACK)
    list(APPEND LIBS -Wl,-z,noexecstack)
  endif()
endif()

# Misc stuff

# TO-DO: disable-nls, disable-ipv6, 

# create local source files
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/cmdlocal.c")
  message(STATUS "Copying cmdlocal.dst to cmdlocal.c")
  execute_process(COMMAND ${CP} cmdlocal.dst cmdlocal.c WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
elseif("${PROJECT_SOURCE_DIR}/cmdlocal.dst" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/cmdlocal.c")
  message(WARNING "cmdlocal.dst is newer than cmdlocal.c. You might need to update cmdlocal.c")
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/flaglocal.c")
  message(STATUS "Copying flaglocal.dst to flaglocal.c")
  execute_process(COMMAND ${CP} flaglocal.dst flaglocal.c WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
elseif("${PROJECT_SOURCE_DIR}/flaglocal.dst" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/flaglocal.c")
  message(WARNING "flaglocal.dst is newer than flaglocal.c. You might need to update flaglocal.c")
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/funlocal.c")
  message(STATUS "Copying funlocal.dst to funlocal.c")
  execute_process(COMMAND ${CP} funlocal.dst funlocal.c WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
elseif("${PROJECT_SOURCE_DIR}/funlocal.dst" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/funlocal.c")
  message(WARNING "funlocal.dst is newer than funlocal.c. You might need to update funlocal.c")
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/local.c")
  message(STATUS "Copying local.dst to local.c")
  execute_process(COMMAND ${CP} local.dst local.c WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
elseif("${PROJECT_SOURCE_DIR}/local.dst" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/local.c")
    message(WARNING "local.dst is newer than local.c. You might need to update local.c")
endif()
if(NOT EXISTS "${PROJECT_BINARY_DIR}/game/names.cnf")
  message(STATUS "Copying namescnf.dst to names.cnf")
  execute_process(COMMAND ${CP} namescnf.dst names.cnf WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/game")
endif()

set_source_files_properties(boolexp.c PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/bflags.c")
set_source_files_properties(markup.c PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/htmltab.c;${PROJECT_SOURCE_DIR}/rgbtab.c")
set_source_files_properties(funmath.c PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/lmathtab.c")

if(EXISTS ${GPERF})

  add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/bflags.c"
    COMMAND ${GPERF} -C --output-file bflags.c bflags.gperf
    MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/bflags.gperf"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" COMMENT "Rebuilding bflags.c" VERBATIM)
  add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/htmltab.c"
    COMMAND ${GPERF} -C --output-file htmltab.c htmltab.gperf
    MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/htmltab.gperf"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" COMMENT "Rebuilding htmltab.c" VERBATIM)
  add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/lmathtab.c"
    COMMAND ${GPERF} -C --output-file lmathtab.c lmathtab.gperf
    MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/lmathtab.gperf"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" COMMENT "Rebuilding lmathtab.c" VERBATIM)
  add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/rgbtab.c"
    COMMAND ${GPERF} -C --output-file rgbtab.c rgbtab.gperf
    MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/rgbtab.gperf"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" COMMENT "Rebuilding rgbtab.c" VERBATIM)

else()

  if("${PROJECT_SOURCE_DIR}/bflags.gperf" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/bflags.c" OR
      "${PROJECT_SOURCE_DIR}/htmltab.gperf" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/htmltab.c" OR
      "${PROJECT_SOURCE_DIR}/lmathtab.gperf" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/lmathtab.c" OR
      "${PROJECT_SOURCE_DIR}/rgbtab.gperf" IS_NEWER_THAN "${PROJECT_SOURCE_DIR}/rgbtab.c")
    message(WARNING "gperf not installed and hash tables need to be updated.")
  endif()
endif()

# TO-DO: Make this happen at compile time
execute_process(COMMAND ${PERL} utils/mkcmds.pl gitinfo WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")

# By my understanding this next line shouldn't be needed, but...
set_source_files_properties(command.c PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/switchinc.c;${PROJECT_SOURCE_DIR}/../hdrs/cmds.h")
add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/switchinc.c" "${PROJECT_SOURCE_DIR}/../hdrs/switches.h"
  COMMAND ${PERL} utils/mkcmds.pl switches WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/SWITCHES" COMMENT "Rebuilding switchinc.c and switches.h"
  VERBATIM)

# command.c handled above
set_source_files_properties(cmdlocal.c move.c PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/../hdrs/cmds.h")
add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/../hdrs/cmds.h"
  COMMAND ${PERL} utils/mkcmds.pl commands VERBATIM WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  DEPENDS "${PROJECT_SOURCE_DIR}/cmds.c" "${PROJECT_SOURCE_DIR}/command.c" "${PROJECT_SOURCE_DIR}/cque.c"
  "${PROJECT_SOURCE_DIR}/set.c" "${PROJECT_SOURCE_DIR}/sql.c" COMMENT "Rebuilding cmds.h")

set_source_files_properties(function.c PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/../hdrs/funs.h")
add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/../hdrs/funs.h"
  COMMAND ${PERL} utils/mkcmds.pl functions VERBATIM WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  DEPENDS "${PROJECT_SOURCE_DIR}/funcrypt.c" "${PROJECT_SOURCE_DIR}/function.c"
  "${PROJECT_SOURCE_DIR}/fundb.c" "${PROJECT_SOURCE_DIR}/funlist.c" "${PROJECT_SOURCE_DIR}/funlocal.c"
  "${PROJECT_SOURCE_DIR}/funmath.c" "${PROJECT_SOURCE_DIR}/funmisc.c" "${PROJECT_SOURCE_DIR}/funstr.c"
  "${PROJECT_SOURCE_DIR}/funtime.c" "${PROJECT_SOURCE_DIR}/funufun.c" "${PROJECT_SOURCE_DIR}/bsd.c"
  "${PROJECT_SOURCE_DIR}/conf.c" "${PROJECT_SOURCE_DIR}/extmail.c" "${PROJECT_SOURCE_DIR}/help.c"
  "${PROJECT_SOURCE_DIR}/markup.c" "${PROJECT_SOURCE_DIR}/wiz.c" "${PROJECT_SOURCE_DIR}/sql.c"
  "${PROJECT_SOURCE_DIR}/cque.c")

set(SRC_FILES access.c atr_tab.c attrib.c boolexp.c bsd.c bufferq.c
  charconv.c chunk.c cmdlocal.c cmds.c command.c compress.c conf.c
  cque.c create.c db.c destroy.c extchat.c extmail.c filecopy.c
  flaglocal.c flags.c funcrypt.c function.c fundb.c funlist.c
  funlocal.c funmath.c funmisc.c funstr.c funtime.c funufun.c game.c
  hash_function.c help.c htab.c intmap.c local.c lock.c log.c look.c
  malias.c markup.c match.c memcheck.c move.c mycrypt.c mymalloc.c
  mysocket.c myrlimit.c myssl.c notify.c parse.c pcg_basic.c player.c
  plyrlist.c predicat.c privtab.c info_master.c ptab.c rob.c
  services.c set.c sig.c sort.c speech.c sql.c ssl_master.c strdup.c
  strtree.c strutil.c tables.c timer.c tz.c unparse.c utils.c
  version.c wait.c warnings.c wild.c wiz.c)

add_executable(netmush ${SRC_FILES})
target_include_directories(netmush PUBLIC "${PROJECT_SOURCE_DIR}/../hdrs" "${PROJECT_BINARY_DIR}")
target_link_libraries(netmush ${LIBS} ${MUSHLIBS})
set_property(TARGET netmush PROPERTY C_STANDARD 99)
target_compile_features(netmush PUBLIC c_restrict)
target_compile_options(netmush PUBLIC ${CMAKE_REQUIRED_DEFINITIONS})

if(NOT (NO_INFO_SLAVE OR WIN32))
  set(INFO_SLAVE 1)
  add_executable(info_slave info_slave.c strdup.c sig.c wait.c mysocket.c)
  target_compile_definitions(info_slave PUBLIC SLAVE)
  target_include_directories(info_slave PUBLIC "${PROJECT_SOURCE_DIR}/../hdrs" "${PROJECT_BINARY_DIR}")
  set_property(TARGET info_slave PROPERTY LINKER_LANGUAGE C)
  target_link_libraries(info_slave ${LIBS} ${SLAVELIBS})
  set_property(TARGET info_slave PROPERTY C_STANDARD 99)
  target_compile_features(info_slave PUBLIC c_restrict)
  target_compile_options(info_slave PUBLIC ${CMAKE_REQUIRED_DEFINITIONS})
endif()
  
if(Libevent_FOUND AND NOT (NO_SSL_SLAVE OR WIN32))
  set(SSL_SLAVE 1)
  add_executable(ssl_slave ssl_slave.c mysocket.c myssl.c pcg_basic.c sig.c strdup.c wait.c)
  target_compile_definitions(ssl_slave PUBLIC SLAVE)
  target_include_directories(ssl_slave PUBLIC "${PROJECT_SOURCE_DIR}/../hdrs" "${PROJECT_BINARY_DIR}")
  set_property(TARGET ssl_slave PROPERTY LINKER_LANGUAGE C)
  target_link_libraries(ssl_slave ${LIBS} ${SLAVELIBS})
  set_property(TARGET ssl_slave PROPERTY C_STANDARD 99)
  target_compile_features(ssl_slave PUBLIC c_restrict)
  target_compile_options(ssl_slave PUBLIC ${CMAKE_REQUIRED_DEFINITIONS})
endif()
  
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h" @ONLY)
if(CHMOD)
  configure_file("${PROJECT_BINARY_DIR}/test/alltests.sh.in" "${PROJECT_BINARY_DIR}/test/alltests.sh" @ONLY)
configure_file("${PROJECT_BINARY_DIR}/game/txt/compose.sh.in" "${PROJECT_BINARY_DIR}/game/txt/compose.sh" @ONLY)
  execute_process(COMMAND ${CHMOD} +x test/alltests.sh game/txt/compose.sh WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()

include(WriteCompilerDetectionHeader)
write_compiler_detection_header(FILE confcompiler.h PREFIX HAVE COMPILERS GNU Clang MSVC AppleClang SunPro FEATURES c_restrict)

# Assorted extra make targets
if(CTAGS)
  add_custom_target(ctags COMMAND ${CTAGS} *.c ../hdrs/*.h ../*.h
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" COMMENT "Running ctags on source")
endif()
if(ETAGS)
  add_custom_target(etags COMMAND ${ETAGS} *.c ../hdrs/*.h ../*.h
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" COMMENT "Running etags on source")
endif()
if(INDENT)
  add_custom_target(indent
    COMMAND ${INDENT} -i --style=file ${SRC_FILES} info_slave.c ssl_slave.c ../hdrs/*.h
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" COMMENT "Reformatting source code")
endif()

add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/game/mush.cnf"
  COMMAND ${PERL} "${PROJECT_BINARY_DIR}/utils/update-cnf.pl" game/mush.cnf game/mushcnf.dst
  DEPENDS "${PROJECT_BINARY_DIR}/game/mushcnf.dst"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" COMMENT "Updating mush.cnf" VERBATIM)

add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/game/alias.cnf"
  COMMAND ${PERL} "${PROJECT_BINARY_DIR}/utils/update-cnf.pl" game/alias.cnf game/aliascnf.dst
  DEPENDS "${PROJECT_BINARY_DIR}/game/aliascnf.dst"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" COMMENT "Updating alias.cnf" VERBATIM)

add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/game/restrict.cnf"
  COMMAND ${PERL} "${PROJECT_BINARY_DIR}/utils/update-cnf.pl" game/restrict.cnf game/restrictcnf.dst
  DEPENDS "${PROJECT_BINARY_DIR}/game/restrictcnf.dst"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" COMMENT "Updating restrict.cnf" VERBATIM)

add_custom_target(update
  DEPENDS "${PROJECT_BINARY_DIR}/game/mush.cnf" "${PROJECT_BINARY_DIR}/game/alias.cnf"
          "${PROJECT_BINARY_DIR}/game/restrict.cnf" COMMENT "Updating config files.")
        
install(TARGETS netmush info_slave ssl_slave DESTINATION "game" OPTIONAL)
