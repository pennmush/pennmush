cmake_minimum_required(VERSION 3.7)
project(PennMUSH LANGUAGES C VERSION 1.8.6.2)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(FindUnixCommands)
include(FindPerl)
include(FindDoxygen)
find_program(GPERF gperf)

include(CheckLibraryExists)
include(CheckIncludeFile)
include(FindPackageHandleStandardArgs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../hdrs)

# Header files
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(stdbool.h HAVE_STDBOOL_H)
CHECK_INCLUDE_FILE(sys/wait.h HAVE_SYS_WAIT_H)
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(sys/socket.h HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILE(arpa/inet.h HAVE_ARPA_INET_H)
CHECK_INCLUDE_FILE(netdb.h HAVE_NETDB_H)
CHECK_INCLUDE_FILE(netinet/tcp.h HAVE_NETINET_TCP_H)
CHECK_INCLUDE_FILE(netinet/in.h HAVE_NETINET_IN_H)
CHECK_INCLUDE_FILE(sys/un.h HAVE_SYS_UN_H)
CHECK_INCLUDE_FILE(sys/resource.h HAVE_SYS_RESOURCE_H)
CHECK_INCLUDE_FILE(sys/event.h HAVE_SYS_EVENT_H)
CHECK_INCLUDE_FILE(sys/uio.h HAVE_SYS_UIO.H)
CHECK_INCLUDE_FILE(byteswap.h HAVE_BYTESWAP.H)
CHECK_INCLUDE_FILE(poll.h HAVE_POLL_H)
CHECK_INCLUDE_FILE(sys/select.h HAVE_SYS_SELECT_H)
CHECK_INCLUDE_FILE(sys/inotify.h HAVE_SYS_INOTIFY_H)
CHECK_INCLUDE_FILE(langinfo.h HAVE_LANGINFO_H)
CHECK_INCLUDE_FILE(fenv.h HAVE_FENV_H)
CHECK_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)


# Required libraries
include(FindOpenSSL)
if(NOT ${OPENSSL_FOUND})
       message(FATAL_ERROR "OpenSSL is required to use PennMUSH")
endif()
include(FindPCRE)
if(NOT ${PCRE_FOUND})
  message(FATAL_ERROR "PCRE is required to use PennMUSH")
endif()

# Optional libraries
include(FindIntl)
find_path(LIBCRYPT_INCLUDE_DIR crypt.h)
find_library(LIBCRYPT_LIBRARY crypt)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibCrypt DEFAULT_MSG LIBCRYPT_LIBRARY LIBCRYPT_INCLUDE_DIR)
include(FindLibEvent)
if(${Libevent_FOUND})
  find_library(LIBEVENT_EXTRA_LIBRARY event_extra)
  find_library(LIBEVENT_OPENSSL_LIBRARY event_openssl)
  set(LIBEVENT_LIBRARIES "${LIBEVENT_LIBRARIES} ${LIBEVENT_EXTRA_LIBRARY} ${LIBEVENT_OPENSSL_LIBRARY}")
endif()

find_path(LIBZ_INCLUDE_DIR zlib.h)
find_library(LIBZ_LIBRARY z)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibZ DEFAULT_MSG LIBZ_LIBRARY LIBZ_INCLUDE_DIR)

# TODO: MySQL, Sqlite3
include(FindPostgreSQL)

# Assorted headers


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
target_compile_features(netmush PUBLIC c_restrict)
target_include_directories(netmush PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../hdrs ${CMAKE_CURRENT_BINARY_DIR})
#target_compile_options(netmush PUBLIC "-include config.h" "-include options.h" "-include confmagic.h")
target_link_libraries(netmush ${OPENSSL_LIBRARIES} ${PostgreSQL_LIBRARY_DIRS} ${PostgreSQL_LIBRARIES} ${LIBEVENT_LIBRARIES})

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include(WriteCompilerDetectionHeader)
write_compiler_detection_header(FILE confcompiler.h PREFIX HAVE COMPILERS GNU Clang FEATURES c_restrict)
