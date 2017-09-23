cmake_minimum_required(VERSION 3.7)
project(PennMUSH LANGUAGES C VERSION 1.8.6.2)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(FindUnixCommands)
include(FindPerl)
include(FindDoxygen)
find_program(GPERF gperf)

include(CheckLibraryExists)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../hdrs)

# Required libraries
include(FindOpenSSL)
if(NOT ${OPENSSL_FOUND})
       message(FATAL_ERROR "OpenSSL is required to use PennMUSH")
endif(NOT ${OPENSSL_FOUND})
include(FindPCRE)
if(NOT ${OPENSSL_FOUND})
       message(FATA_ERROR "PCRE is required to use PennMUSH")
endif(NOT ${OPENSSL_FOUND})

# Optional libraries
CHECK_LIBRARY_EXISTS(intl gettext "" HAVE_LIBINTL)
CHECK_LIBRARY_EXISTS(crypt crypt "" HAVE_LIBCRYPT)
CHECK_LIBRARY_EXISTS(event_core event_base_loop "" HAVE_LIBEVENT_CORE)
CHECK_LIBRARY_EXISTS(event_extra evdns_base_new "" HAVE_LIBEVENT_EXTRA)
CHECK_LIBRARY_EXISTS(event_openssl bufferevent_openssl_socket_new "" HAVE_LIBEVENT_OPENSSL)
CHECK_LIBRARY_EXISTS(z gzungetz "" HAVE_LIBZ)
include(FindPostgreSQL)


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
	
target_link_libraries(netmush ${OPENSSL_LIBRARIES} ${PostgreSQL_LIBRARY_DIRS} ${PostgreSQL_LIBRARIES})
